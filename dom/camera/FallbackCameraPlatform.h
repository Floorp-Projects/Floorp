/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2015 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DOM_CAMERA_FALLBACKCAMERAPLATFORM_H
#define DOM_CAMERA_FALLBACKCAMERAPLATFORM_H

#include <inttypes.h>
#include <string.h>

typedef struct {
  int32_t id;
  int32_t score;
  int32_t rect[4];
  int32_t left_eye[2];
  int32_t right_eye[2];
  int32_t mouth[2];
} camera_face_t;

typedef struct {
  uint32_t number_of_faces;
  camera_face_t* faces;
} camera_frame_metadata_t;

namespace android {
  enum camcorder_quality {
    CAMCORDER_QUALITY_LOW,
    CAMCORDER_QUALITY_HIGH,
    CAMCORDER_QUALITY_QCIF,
    CAMCORDER_QUALITY_CIF,
    CAMCORDER_QUALITY_480P,
    CAMCORDER_QUALITY_720P,
    CAMCORDER_QUALITY_1080P,
    CAMCORDER_QUALITY_QVGA,
    CAMCORDER_QUALITY_VGA,
    CAMCORDER_QUALITY_LIST_START = CAMCORDER_QUALITY_LOW,
    CAMCORDER_QUALITY_LIST_END = CAMCORDER_QUALITY_VGA
  };

  enum output_format {
    OUTPUT_FORMAT_THREE_GPP,
    OUTPUT_FORMAT_MPEG_4
  };

  enum video_encoder {
    VIDEO_ENCODER_H263,
    VIDEO_ENCODER_H264,
    VIDEO_ENCODER_MPEG_4_SP
  };

  enum audio_encoder {
    AUDIO_ENCODER_AMR_WB,
    AUDIO_ENCODER_AMR_NB,
    AUDIO_ENCODER_AAC
  };

  template <class T>
  class sp final
  {
  public:
    sp()
      : mPtr(nullptr)
    { }

    sp(T *aPtr)
      : mPtr(aPtr)
    { }

    virtual ~sp()         { }
    T* get() const        { return mPtr; }
    void clear()          { mPtr = nullptr; }
    T* operator->() const { return get(); }

  private:
    nsRefPtr<T> mPtr;
  };

  typedef uint64_t nsecs_t;

  enum error_t {
    OK = 0,
    UNKNOWN_ERROR,
    INVALID_OPERATION
  };

  enum camera_msg_t {
    CAMERA_MSG_SHUTTER,
    CAMERA_MSG_COMPRESSED_IMAGE
  };

  class String8 final
  {
  public:
    String8()                  { }
    String8(const char* aData) { mData.AssignASCII(aData); }
    virtual ~String8()         { }
    const char* string() const { return mData.Data(); }

  private:
    nsCString mData;
  };

  enum camera_facing_t {
    CAMERA_FACING_BACK,
    CAMERA_FACING_FRONT
  };

  struct CameraInfo {
    camera_facing_t facing;
  };

  class Camera final : public nsISupports
  {
  public:
    NS_DECL_ISUPPORTS;

    void disconnect()                         { }
    String8 getParameters()                   { return String8(); }
    int setParameters(const String8& aParams) { return UNKNOWN_ERROR; }
    int storeMetaDataInBuffers(bool aEnabled) { return UNKNOWN_ERROR; }
    int autoFocus()                           { return UNKNOWN_ERROR; }
    int cancelAutoFocus()                     { return UNKNOWN_ERROR; }
    int takePicture(uint32_t flags)           { return UNKNOWN_ERROR; }
    int startPreview()                        { return UNKNOWN_ERROR; }
    int stopPreview()                         { return UNKNOWN_ERROR; }
    int startRecording()                      { return UNKNOWN_ERROR; }
    int stopRecording()                       { return UNKNOWN_ERROR; }
    int startFaceDetection()                  { return UNKNOWN_ERROR; }
    int stopFaceDetection()                   { return UNKNOWN_ERROR; }
    static int32_t getNumberOfCameras()       { return 2; }

    static int getCameraInfo(int32_t aDevice, CameraInfo* aInfo)
    {
      switch (aDevice) {
        case 0:
          aInfo->facing = CAMERA_FACING_BACK;
          break;
        case 1:
          aInfo->facing = CAMERA_FACING_FRONT;
          break;
        default:
          return UNKNOWN_ERROR;
      }
      return OK;
    }

  protected:
    Camera()          { }
    virtual ~Camera() { }

  private:
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;
  };

  class CameraParameters final
  {
  public:
    static const char KEY_PREVIEW_SIZE[];
    static const char KEY_SUPPORTED_PREVIEW_SIZES[];
    static const char KEY_PREVIEW_FPS_RANGE[];
    static const char KEY_SUPPORTED_PREVIEW_FPS_RANGE[];
    static const char KEY_PREVIEW_FORMAT[];
    static const char KEY_SUPPORTED_PREVIEW_FORMATS[];
    static const char KEY_PREVIEW_FRAME_RATE[];
    static const char KEY_SUPPORTED_PREVIEW_FRAME_RATES[];
    static const char KEY_PICTURE_SIZE[];
    static const char KEY_SUPPORTED_PICTURE_SIZES[];
    static const char KEY_PICTURE_FORMAT[];
    static const char KEY_SUPPORTED_PICTURE_FORMATS[];
    static const char KEY_JPEG_THUMBNAIL_WIDTH[];
    static const char KEY_JPEG_THUMBNAIL_HEIGHT[];
    static const char KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES[];
    static const char KEY_JPEG_THUMBNAIL_QUALITY[];
    static const char KEY_JPEG_QUALITY[];
    static const char KEY_ROTATION[];
    static const char KEY_GPS_LATITUDE[];
    static const char KEY_GPS_LONGITUDE[];
    static const char KEY_GPS_ALTITUDE[];
    static const char KEY_GPS_TIMESTAMP[];
    static const char KEY_GPS_PROCESSING_METHOD[];
    static const char KEY_WHITE_BALANCE[];
    static const char KEY_SUPPORTED_WHITE_BALANCE[];
    static const char KEY_EFFECT[];
    static const char KEY_SUPPORTED_EFFECTS[];
    static const char KEY_ANTIBANDING[];
    static const char KEY_SUPPORTED_ANTIBANDING[];
    static const char KEY_SCENE_MODE[];
    static const char KEY_SUPPORTED_SCENE_MODES[];
    static const char KEY_FLASH_MODE[];
    static const char KEY_SUPPORTED_FLASH_MODES[];
    static const char KEY_FOCUS_MODE[];
    static const char KEY_SUPPORTED_FOCUS_MODES[];
    static const char KEY_MAX_NUM_FOCUS_AREAS[];
    static const char KEY_FOCUS_AREAS[];
    static const char KEY_FOCAL_LENGTH[];
    static const char KEY_HORIZONTAL_VIEW_ANGLE[];
    static const char KEY_VERTICAL_VIEW_ANGLE[];
    static const char KEY_EXPOSURE_COMPENSATION[];
    static const char KEY_MAX_EXPOSURE_COMPENSATION[];
    static const char KEY_MIN_EXPOSURE_COMPENSATION[];
    static const char KEY_EXPOSURE_COMPENSATION_STEP[];
    static const char KEY_AUTO_EXPOSURE_LOCK[];
    static const char KEY_AUTO_EXPOSURE_LOCK_SUPPORTED[];
    static const char KEY_AUTO_WHITEBALANCE_LOCK[];
    static const char KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED[];
    static const char KEY_MAX_NUM_METERING_AREAS[];
    static const char KEY_METERING_AREAS[];
    static const char KEY_ZOOM[];
    static const char KEY_MAX_ZOOM[];
    static const char KEY_ZOOM_RATIOS[];
    static const char KEY_ZOOM_SUPPORTED[];
    static const char KEY_SMOOTH_ZOOM_SUPPORTED[];
    static const char KEY_FOCUS_DISTANCES[];
    static const char KEY_VIDEO_SIZE[];
    static const char KEY_SUPPORTED_VIDEO_SIZES[];
    static const char KEY_MAX_NUM_DETECTED_FACES_HW[];
    static const char KEY_MAX_NUM_DETECTED_FACES_SW[];
    static const char KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO[];
    static const char KEY_VIDEO_FRAME_FORMAT[];
    static const char KEY_RECORDING_HINT[];
    static const char KEY_VIDEO_SNAPSHOT_SUPPORTED[];
    static const char KEY_VIDEO_STABILIZATION[];
    static const char KEY_VIDEO_STABILIZATION_SUPPORTED[];
    static const char KEY_LIGHTFX[];
  };

  class MediaProfiles final
  {
  public:
    static MediaProfiles* getInstance() {
      if (!sMediaProfiles) {
        sMediaProfiles = new MediaProfiles();
      }
      return sMediaProfiles;
    }

    bool hasCamcorderProfile(int aCameraId, camcorder_quality aQuality) const {
      switch (aQuality) {
        case CAMCORDER_QUALITY_LOW:
        case CAMCORDER_QUALITY_HIGH:
        case CAMCORDER_QUALITY_QVGA:
        case CAMCORDER_QUALITY_VGA:
          return true;
        default:
          break;
      }
      return false;
    }

    int getCamcorderProfileParamByName(const char* aParameter, int aCameraId, camcorder_quality aQuality) const {
      switch (aQuality) {
        case CAMCORDER_QUALITY_LOW:
        case CAMCORDER_QUALITY_QVGA:
          if (strcmp(aParameter, "vid.width") == 0) {
            return 320;
          } else if (strcmp(aParameter, "vid.height") == 0) {
            return 240;
          } else if (strcmp(aParameter, "vid.fps") == 0) {
            return 30;
          }
          return 0;
        case CAMCORDER_QUALITY_HIGH:
        case CAMCORDER_QUALITY_VGA:
          if (strcmp(aParameter, "vid.width") == 0) {
            return 640;
          } else if (strcmp(aParameter, "vid.height") == 0) {
            return 480;
          } else if (strcmp(aParameter, "vid.fps") == 0) {
            return 30;
          }
          return 0;
        default:
          break;
      }
      return -1;
    }

  protected:
    MediaProfiles()          { }
    virtual ~MediaProfiles() { }

  private:
    MediaProfiles(const MediaProfiles&) = delete;
    MediaProfiles& operator=(const MediaProfiles&) = delete;

    static MediaProfiles* sMediaProfiles;
  };
}

#endif
