/*

 Copyright (c) 2014 University of Edinburgh, Imperial College, University of Manchester.
 Developed in the PAMELA project, EPSRC Programme Grant EP/K008730/1

 This code is licensed under the MIT License.

 */

#ifndef INTERFACE_H_
#define INTERFACE_H_

#ifdef __APPLE__
    #include <mach/clock.h>
    #include <mach/mach.h>
#endif

#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

// added because of socket
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <stdbool.h>
//#include <unistd.h>
//#include <time.h>
#include <fstream>
#define MAXHOSTNAME 256


enum ReaderType {
	READER_RAW, READER_SCENE, READER_OPENNI
};




class DepthReader {
public:
	virtual ~DepthReader() {
	}
	virtual bool readNextDepthFrame(float * depthMap)= 0;
	inline bool readNextDepthFrame(unsigned short int * UintdepthMap) {
		return readNextDepthFrame(NULL, UintdepthMap);
	}
	virtual bool readNextDepthFrame(uchar3* raw_rgb,
			unsigned short int * depthMap) = 0;
	virtual float4 getK() = 0;
	virtual uint2 getinputSize() = 0;
	virtual void restart()=0;
	bool isValid() {
		return cameraOpen;
	}
	int getFrameNumber() {
		return (_frame);
	}

	virtual ReaderType getType() =0;
	void get_next_frame() {

		if (_fps == 0) {
			_frame++;
			return;
		}

#ifdef __APPLE__
		clock_serv_t cclock;
		mach_timespec_t clockData;
		host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
		clock_get_time(cclock, &clockData);
		mach_port_deallocate(mach_task_self(), cclock);
#else
		struct timespec clockData;
		clock_gettime(CLOCK_MONOTONIC, &clockData);
#endif
		double current_frame = clockData.tv_sec
				+ clockData.tv_nsec / 1000000000.0;
		static double first_frame = current_frame;

		int frame = std::ceil((current_frame - first_frame) * (double) _fps);
		_frame = frame;
		std::cout << "next frame is " << _frame << std::endl;
		double tpf = 1.0 / _fps;
		double ttw = ((double) _frame * tpf - current_frame + first_frame);
		if (_blocking_read) {
			if (ttw > 0)
				usleep(1000000.0 * ttw);
		}

	}

	bool cameraActive;
	bool cameraOpen;
protected:
	int _frame;
	int _fps;bool _blocking_read;
};

static const float SceneK[3][3] = { { 481.20, 0.00, 319.50 }, { 0.00, -424.00,
		239.50 }, { 0.00, 0.00, 1.00 } };

static const int _scenewidth = 512;
static const int _sceneheight = 424;
static const float _u0 = SceneK[0][2];
static const float _v0 = SceneK[1][2];
static const float _focal_x = SceneK[0][0];
static const float _focal_y = SceneK[1][1];

class SceneDepthReader: public DepthReader {
private:

	std::string _dir;
	uint2 _size;
	int PIXELNUMBER;
	struct sockaddr_in socketInfo;
	char sysHost[MAXHOSTNAME+1];  // Hostname of this computer we are running on
	struct hostent *hPtr;
	int socketHandle;
	int portNumber;
	int socketConnection;

public:
	~SceneDepthReader() {
	}
	;
	SceneDepthReader(std::string dir, int fps, bool blocking_read) :
			DepthReader(), _dir(dir), _size(make_uint2(512, 424)), PIXELNUMBER(217088),portNumber(8899) {
		std::cerr << "No such directory NNNNNNNNNNNNNNN" << dir << std::endl;
		struct stat st;
		lstat(dir.c_str(), &st);
		if (S_ISDIR(st.st_mode)) {
			cameraOpen = true;
			cameraActive = true;
			_frame = -1;
			_fps = fps;
			_blocking_read = blocking_read;
		} else {
			std::cerr << "No such directory " << dir << std::endl;
			cameraOpen = false;
			cameraActive = false;
		}

		// added because of socket

		   bzero(&socketInfo, sizeof(sockaddr_in));  // Clear structure memory

		   // Get system information
		   gethostname(sysHost, MAXHOSTNAME);  // Get the name of this computer we are running on
		   if((hPtr = gethostbyname(sysHost)) == NULL)
		   {
		      std::cerr << "System hostname misconfigured." << std::endl;
		      exit(EXIT_FAILURE);
		   }

		   // create socket

		   if((socketHandle = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		   {
		      close(socketHandle);
		      exit(EXIT_FAILURE);
		   }

		   // Load system information into socket data structures

		   socketInfo.sin_family = AF_INET;
		   socketInfo.sin_addr.s_addr = htonl(INADDR_ANY); // Use any address available to the system
		   socketInfo.sin_port = htons(portNumber);      // Set port number

		   // Bind the socket to a local socket address

		   if( bind(socketHandle, (struct sockaddr *) &socketInfo, sizeof(socketInfo)) < 0)
		   {
		      close(socketHandle);
		      perror("bind");
		      exit(EXIT_FAILURE);
		   }

		   listen(socketHandle, 1);


		   std::cerr << "before accepting socket" << std::endl;
		   if( (socketConnection = accept(socketHandle, NULL, NULL)) < 0)
		   {
		      std::cerr << "socket accept failed " << std::endl;
		      exit(EXIT_FAILURE);
		   }
		   std::cerr << "after accepting socket" << std::endl;
		   close(socketHandle);


	}
	;
	ReaderType getType() {
		return (READER_SCENE);
	}
	inline float4 getK() {
		return make_float4(481.20, 424.00, 319.50, 239.50);

	}
	inline uint2 getinputSize() {
		return _size;
	}
	inline void restart() {
		_frame = 0;
	}

	inline bool readNextDepthFrame(uchar3*, unsigned short int * depthMap) {

		float* FloatdepthMap = (float*) malloc(
				_size.x * _size.y * sizeof(float));
		bool res = readNextDepthFrame(FloatdepthMap);

		for (unsigned int i = 0; i < _size.x * _size.y; i++) {
//			depthMap[i] = FloatdepthMap[i] * 1000.0f;
			depthMap[i] = FloatdepthMap[i];
		}
		free(FloatdepthMap);
		return res;

	}
	inline bool readNextDepthFrame(float * depthMap) {

		std::ostringstream filename;
		get_next_frame();
//		filename << this->_dir << "/scene_00_" << std::setfill('0')
//				<< std::setw(4) << _frame << ".depth";



		   int rc = 0;  // Actual number of bytes read
		   char buf[PIXELNUMBER*2 + 1];

		   // rc is the number of characters returned.
		   // Note this is not typical. Typically one would only specify the number
		   // of bytes to read a fixed header which would include the number of bytes
		   // to read. See "Tips and Best Practices" below.
		   unsigned short aa = 0;

		   rc = recv(socketConnection, buf, PIXELNUMBER*2, MSG_WAITALL);
		   buf[rc]= (char) NULL;
		   int i = 0;
		   for(i = 0; i<PIXELNUMBER; ++i)
		   {
			   depthMap[i] = ((unsigned char)buf[2*i]) | ((unsigned char)buf[2*i + 1] << 8);
			   //fout << aa/1000.0 << " ";
		   }

//		   std::cerr << "got next frame" << std::endl;
/*
		filename << this->_dir << "/scene" << _frame;
		clock_t begin = clock();
		std::ifstream source;
	//	for(int i=0;i<500000; ++i){;}
		int jj = 10000000;
		while (jj>0){jj--;}
		std::cerr << "After for " ;
		std::cerr << (double) (clock()-begin)/CLOCKS_PER_SEC << std::endl;
		std::cerr << "Processing Frame " << _frame << std::endl;
		long smallcounter = 0;
		source.open(filename.str().c_str(), std::ios_base::in);
		while(!source){
			if(smallcounter == 10000000000){
			//	std::cerr << "haha" << std::endl;	
				std::cerr << "waiting for data" << std::endl;
				smallcounter = 0;
				source.open(filename.str().c_str(), std::ios_base::in);
			}
			smallcounter ++;
		}


/*
		if (!source) {
			std::cerr << "waiting for data " << std::endl;
			while(smallcounter < 1000000){
				smallcounter 
			}

//			return 0;
		}

		uint index = 0;
		while (source.good()) {
			float d;
			source >> d;
			if (_scenewidth * _sceneheight <= index)
				continue;
			depthMap[index] = d;
			index++;
		}

*/


		for (int v = 0; v < _sceneheight; v++) {
			for (int u = 0; u < _scenewidth; u++) {
				float u_u0_by_fx = (u - _u0) / _focal_x;
				float v_v0_by_fy = (v - _v0) / _focal_y;

				depthMap[u + v * _scenewidth] = depthMap[u + v * _scenewidth]
						/ std::sqrt(
								u_u0_by_fx * u_u0_by_fx
										+ v_v0_by_fy * v_v0_by_fy + 1);

			}
		}
		return i > 0;
	}

};

class RawDepthReader: public DepthReader {
private:
	FILE* _pFile;
	uint2 _size;

public:
	RawDepthReader(std::string filename, int fps, bool blocking_read) :
			DepthReader(), _pFile(fopen(filename.c_str(), "rb")) {

		size_t res = fread(&(_size), sizeof(_size), 1, _pFile);
		cameraOpen = false;
		cameraActive = false;
		if (res != 1) {
			std::cerr << "Invalid Raw file." << std::endl;

		} else {
			cameraOpen = true;
			cameraActive = true;
			_frame = -1;
			_fps = fps;
			_blocking_read = blocking_read;
			fseek(_pFile, 0, SEEK_SET);
		}
	};
	ReaderType getType() {
		return (READER_RAW);
	}
	inline bool readNextDepthFrame(uchar3* raw_rgb,
			unsigned short int * depthMap) {

		int total = 0;
		int expected_size = 0;
		unsigned int newImageSize[2];

		get_next_frame();
#ifdef LIGHT_RAW // This LightRaw mode is used to get smaller raw files
		unsigned int size_of_frame = (sizeof(unsigned int) * 2 + _size.x * _size.y * sizeof(unsigned short int) );
#else
		unsigned int size_of_frame = (sizeof(unsigned int) * 4
				+ _size.x * _size.y * sizeof(unsigned short int)
				+ _size.x * _size.y * sizeof(uchar3));
#endif
		fseek(_pFile, size_of_frame * _frame, SEEK_SET);

		if (depthMap) {
			total += fread(&(newImageSize), sizeof(newImageSize), 1, _pFile);
			total += fread(depthMap, sizeof(unsigned short int),
					newImageSize[0] * newImageSize[1], _pFile);
			expected_size += 1 + newImageSize[0] * newImageSize[1];
		} else {
			total += fread(&(newImageSize), sizeof(newImageSize), 1, _pFile);
			total += newImageSize[0] * newImageSize[1];
			fseek(_pFile,
					newImageSize[0] * newImageSize[1]
							* sizeof(unsigned short int), SEEK_CUR);
			expected_size += 1 + newImageSize[0] * newImageSize[1];
		}

#ifdef LIGHT_RAW // This LightRaw mode is used to get smaller raw files

		if (raw_rgb) {
			raw_rgb[0].x = 0;
		} else {
		}

#else
		if (raw_rgb) {
			total += fread(&(newImageSize), sizeof(newImageSize), 1, _pFile);
			total += fread(raw_rgb, sizeof(uchar3),
					newImageSize[0] * newImageSize[1], _pFile);
			expected_size += 1 + newImageSize[0] * newImageSize[1];
		} else {
			total += fread(&(newImageSize), sizeof(newImageSize), 1, _pFile);
			total += newImageSize[0] * newImageSize[1];
			fseek(_pFile, newImageSize[0] * newImageSize[1] * sizeof(uchar3),
					SEEK_CUR);
			expected_size += 1 + newImageSize[0] * newImageSize[1];
		}
#endif

		if (total != expected_size) {
			std::cout << "End of file" << (total == 0 ? "" : "(garbage found)")
					<< "." << std::endl;
			return false;
		} else {
			return true;
		}
	}

	inline void restart() {
		_frame = -1;
		rewind(_pFile);

	}

	inline bool readNextDepthFrame(float * depthMap) {

		unsigned short int* UintdepthMap = (unsigned short int*) malloc(
				_size.x * _size.y * sizeof(unsigned short int));
		bool res = readNextDepthFrame(NULL, UintdepthMap);

		for (unsigned int i = 0; i < _size.x * _size.y; i++) {
			depthMap[i] = (float) UintdepthMap[i] / 1000.0f;
		}
		free(UintdepthMap);
		return res;
	}

	inline uint2 getinputSize() {
		return _size;
	}
	inline float4 getK() {
		return make_float4(531.15, 531.15, 512 / 2, 424 / 2);
	}

};

#ifdef DO_OPENNI
#include <OpenNI.h>

class MyDepthFrameAllocator: public openni::VideoStream::FrameAllocator {
private:
	uint16_t *depth_buffers_;

public:
	MyDepthFrameAllocator(uint16_t *depth_buffers) :
	depth_buffers_(depth_buffers) {
	}
	void *allocateFrameBuffer(int ) {
		return depth_buffers_;
	}
	void freeFrameBuffer(void *) {
	}
};

class MyColorFrameAllocator: public openni::VideoStream::FrameAllocator {
private:
	uchar3 *rgb_buffer_;

public:
	MyColorFrameAllocator(uchar3 *rgb_buffer) {
		rgb_buffer_ = rgb_buffer;
	}
	void *allocateFrameBuffer(int ) {
		return rgb_buffer_;
	}
	void freeFrameBuffer(void *) {
	}
};

class OpenNIDepthReader: public DepthReader {
private:
	FILE* _pFile;
	uint2 _size;

	openni::Device device;
	openni::VideoStream depth;
	openni::VideoStream rgb;
	openni::VideoFrameRef depthFrame;
	openni::VideoFrameRef colorFrame;

	uint16_t * depthImage;
	uchar3 * rgbImage;

	openni::Status rc;

public:
	~OpenNIDepthReader() {
		if(device.isValid()) {
			//std::cout << "Stopping depth stream..." << std::endl;

			depth.stop();
			//std::cout << "Stopping rgb stream..." << std::endl;       
			rgb.stop();
			//std::cout << "Destroying depth stream..." << std::endl;       
			depth.destroy();
			// std::cout << "Destroying rgb stream..." << std::endl;
			rgb.destroy();
			//std::cout << "Closing device..." << std::endl;
			device.close();

			//std::cout << "Shutting down OpenNI..." << std::endl;
			openni::OpenNI::shutdown();

			//std::cout << "Done!" << std::endl;
			cameraOpen = false;
			cameraActive = false;
		}

	};
	ReaderType getType() {
		return(READER_OPENNI);
	}
	OpenNIDepthReader(std::string filename, int fps, bool blocking_read) :
	DepthReader(), _pFile(fopen(filename.c_str(), "rb")) {

		cameraActive = false;
		cameraOpen = false;

		rc = openni::OpenNI::initialize();

		if (rc != openni::STATUS_OK) {
			return;
		}

		if (filename == "") {
			rc = device.open(openni::ANY_DEVICE);
		} else {
			rc = device.open(filename.c_str());
			//std::cerr<<"Opened an oni file\n";
		}

		if (rc != openni::STATUS_OK) {
			std::cout << "No kinect device found. " << cameraActive << " " << cameraOpen << "\n";
			return;
		}

		if (device.getSensorInfo(openni::SENSOR_DEPTH) != NULL) {
			rc = depth.create(device, openni::SENSOR_DEPTH);

			if (rc != openni::STATUS_OK) {
				std::cout << "Couldn't create depth stream" << std::endl << openni::OpenNI::getExtendedError() << std::endl;
				//exit(3);
				return;
			} else {
				rc = depth.start();
				if (rc != openni::STATUS_OK) {
					printf("Couldn't start depth stream:\n%s\n", openni::OpenNI::getExtendedError());
					depth.destroy();
					//exit(3);
					return;
				}
				depth.stop();

			}

			rc = rgb.create(device, openni::SENSOR_COLOR);

			if (rc != openni::STATUS_OK) {
				std::cout << "Couldn't create color stream" << std::endl << openni::OpenNI::getExtendedError() << std::endl;
				//exit(3);
				return;
			}

		}

		device.getSensorInfo(openni::SENSOR_DEPTH)->getSupportedVideoModes();

		if(!device.isFile())
		{
			openni::VideoMode newDepthMode;
			newDepthMode.setFps(fps == 0 ? 30 : fps);
			newDepthMode.setPixelFormat(openni::PIXEL_FORMAT_DEPTH_1_MM);
			newDepthMode.setResolution(512,424);

			rc = depth.setVideoMode(newDepthMode);
			if(rc != openni::STATUS_OK)
			{
				std::cout << "Could not set videomode" << std::endl;
				//exit(3);
				return;
			}

			openni::VideoMode newRGBMode;
			newRGBMode.setFps(fps == 0 ? 30 : fps);
			newRGBMode.setPixelFormat(openni::PIXEL_FORMAT_RGB888);
			newRGBMode.setResolution(512,424);

			rc = rgb.setVideoMode(newRGBMode);
			if(rc != openni::STATUS_OK)
			{
				std::cout << "Could not set videomode" << std::endl;

				return;
			}
		}

		openni::VideoMode depthMode = depth.getVideoMode();
		openni::VideoMode colorMode = rgb.getVideoMode();

		_size.x = depthMode.getResolutionX();
		_size.y = depthMode.getResolutionY();

		if (colorMode.getResolutionX() != _size.x || colorMode.getResolutionY() != _size.y) {
			std::cout << "Incorrect rgb resolution: " << colorMode.getResolutionX() << " " << colorMode.getResolutionY() << std::endl;
			//exit(3);
			return;
		}

		depthImage = (uint16_t*) malloc(_size.x * _size.y * sizeof(uint16_t));
		rgbImage = (uchar3*) malloc(_size.x * _size.y * sizeof(uchar3));

		rgb.setMirroringEnabled(false);
		depth.setMirroringEnabled(false);

		device.setImageRegistrationMode(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR);

		if(device.isFile())
		{
			device.getPlaybackControl()->setRepeatEnabled(false);
			depthFrame.release();
			colorFrame.release();
			device.getPlaybackControl()->setSpeed(-1); // Set the playback in a manual mode i.e. read a frame whenever the application requests it
		}

		// The allocators must survive this initialization function.
		MyDepthFrameAllocator *depthAlloc = new MyDepthFrameAllocator(depthImage);
		MyColorFrameAllocator *colorAlloc = new MyColorFrameAllocator(rgbImage);

		// Use allocators to have OpenNI write directly into our buffers
		depth.setFrameBuffersAllocator(depthAlloc);
		depth.start();

		rgb.setFrameBuffersAllocator(colorAlloc);
		rgb.start();

		cameraOpen =true;
		cameraActive =true;
		_frame=-1;
		_fps = fps;
		_blocking_read = blocking_read;

	};
	inline bool readNextDepthFrame(uchar3* raw_rgb, unsigned short int * depthMap) {

		rc = depth.readFrame(&depthFrame);

		if (rc != openni::STATUS_OK) {
			std::cout << "Wait failed" << std::endl;
			exit(1);
		}

		rc = rgb.readFrame(&colorFrame);
		if (rc != openni::STATUS_OK) {
			std::cout << "Wait failed" << std::endl;
			exit(1);
		}

		if (depthFrame.getVideoMode().getPixelFormat() != openni::PIXEL_FORMAT_DEPTH_1_MM
				&& depthFrame.getVideoMode().getPixelFormat() != openni::PIXEL_FORMAT_DEPTH_100_UM) {
			std::cout << "Unexpected frame format" << std::endl;
			exit(1);
		}

		if (colorFrame.getVideoMode().getPixelFormat() != openni::PIXEL_FORMAT_RGB888) {
			std::cout << "Unexpected frame format" << std::endl;
			exit(1);
		}

		if (raw_rgb) {
			memcpy(raw_rgb,rgbImage,_size.x * _size.y*sizeof(uchar3));
		}
		if (depthMap) {
			memcpy(depthMap,depthImage,_size.x * _size.y*sizeof(uint16_t));
		}

		get_next_frame ();

		return true;

	}

	inline void restart() {
		_frame=-1;
		//FIXME : how to rewind OpenNI ?

	}

	inline bool readNextDepthFrame(float * depthMap) {

		unsigned short int* UintdepthMap = (unsigned short int*) malloc(_size.x * _size.y * sizeof(unsigned short int));
		bool res = readNextDepthFrame(NULL,UintdepthMap);

		for (unsigned int i = 0; i < _size.x * _size.y; i++) {
			depthMap[i] = (float) UintdepthMap[i] / 1000.0f;
		}
		free(UintdepthMap);
		return res;
	}

	inline uint2 getinputSize() {
		return _size;
	}
	inline float4 getK() {
		return make_float4(481.2, 424, 512/2, 424/2);

	}

};

#else
class OpenNIDepthReader: public DepthReader {
public:
	OpenNIDepthReader(std::string, int, bool) {
		std::cerr << "OpenNI Library Not found." << std::endl;
		cameraOpen = false;
		cameraActive = false;
	}
	bool readNextDepthFrame(float * depthMap) {
	}
	bool readNextDepthFrame(uchar3* raw_rgb, unsigned short int * depthMap) {
	}
	float4 getK() {
	}
	uint2 getinputSize() {
	}
	void restart() {
	}
	
	ReaderType getType() {
		return (READER_OPENNI);
	}
};
#endif /* DO_OPENNI*/

#endif /* INTERFACE_H_ */
