# Usage: ./update.sh <android_ics_os_src_directory>
#
# Copies the needed files from the directory containing the original
# Android ICS OS source and applies the B2G specific changes for the
# camcorder functionality in B2G.
cp $1/frameworks/base/media/libmediaplayerservice/StagefrightRecorder.cpp ./GonkRecorder.cpp
cp $1/frameworks/base/media/libmediaplayerservice/StagefrightRecorder.h ./GonkRecorder.h
cp $1/frameworks/base/media/libstagefright/CameraSource.cpp ./GonkCameraSource.cpp
cp $1/frameworks/base/include/media/stagefright/CameraSource.h ./GonkCameraSource.h
cp $1/frameworks/base/media/libmedia/AudioParameter.cpp ./AudioParameter.cpp
cp $1/frameworks/base/include/camera/Camera.h ./GonkCameraListener.h
patch -p1 <update.patch
# If you import CAF sources, you also need to apply update2.patch
patch -p1 <update2.patch
