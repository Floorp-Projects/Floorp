/*
 * Copyright (C) 2012 Mozilla Foundation
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

#include <media/MediaProfiles.h>
#include "GonkRecorder.h"
#include "CameraControlImpl.h"
#include "GonkRecorderProfiles.h"
#include "CameraCommon.h"

using namespace mozilla;
using namespace android;

#define DEF_GONK_RECORDER_PROFILE(e, n) e##_INDEX,
enum {
  #include "GonkRecorderProfiles.def"
  PROFILE_COUNT
};

#define DEF_GONK_RECORDER_PROFILE(e, n) { n, e },
static struct {
  const char* name;
  int quality;
} ProfileList[] = {
  #include "GonkRecorderProfiles.def"
  { nullptr, 0 }
};

static MediaProfiles* sMediaProfiles = nullptr;

static bool
IsQualitySupported(uint32_t aCameraId, uint32_t aQualityIndex)
{
  if (!sMediaProfiles) {
    sMediaProfiles = MediaProfiles::getInstance();
  }
  camcorder_quality q = static_cast<camcorder_quality>(ProfileList[aQualityIndex].quality);
  return sMediaProfiles->hasCamcorderProfile(static_cast<int>(aCameraId), q);
}

static int
GetProfileParam(uint32_t aCameraId, uint32_t aQualityIndex, const char* aParam)
{
  if (!sMediaProfiles) {
    sMediaProfiles = MediaProfiles::getInstance();
  }
  camcorder_quality q = static_cast<camcorder_quality>(ProfileList[aQualityIndex].quality);
  return sMediaProfiles->getCamcorderProfileParamByName(aParam, static_cast<int>(aCameraId), q);
}

/**
 * Recorder profile.
 */
static RecorderProfile::FileFormat
TranslateFileFormat(output_format aFileFormat)
{
  switch (aFileFormat) {
    case OUTPUT_FORMAT_THREE_GPP: return RecorderProfile::THREE_GPP;
    case OUTPUT_FORMAT_MPEG_4:    return RecorderProfile::MPEG4;
    default:                      return RecorderProfile::UNKNOWN;
  }
}

GonkRecorderProfile::GonkRecorderProfile(uint32_t aCameraId, uint32_t aQualityIndex)
  : RecorderProfileBase<GonkRecorderAudioProfile, GonkRecorderVideoProfile>(aCameraId, aQualityIndex)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p, mCameraId=%d, mQualityIndex=%d\n", __func__, __LINE__, this, mCameraId, mQualityIndex);
  mPlatformOutputFormat = static_cast<output_format>(GetProfileParam(mCameraId, mQualityIndex, "file.format"));
  mFileFormat = TranslateFileFormat(mPlatformOutputFormat);
  if (aQualityIndex < PROFILE_COUNT) {
    mName = ProfileList[aQualityIndex].name;
    DOM_CAMERA_LOGI("Created camera %d profile index %d: '%s'\n", mCameraId, mQualityIndex, mName);
  }
}

GonkRecorderProfile::~GonkRecorderProfile()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

nsresult
GonkRecorderProfile::ConfigureRecorder(GonkRecorder* aRecorder)
{
  static const size_t SIZE = 256;
  char buffer[SIZE];

  // set all the params
  CHECK_SETARG(aRecorder->setAudioSource(AUDIO_SOURCE_CAMCORDER));
  CHECK_SETARG(aRecorder->setVideoSource(VIDEO_SOURCE_CAMERA));
  CHECK_SETARG(aRecorder->setOutputFormat(GetOutputFormat()));
  CHECK_SETARG(aRecorder->setVideoFrameRate(mVideo.GetFramerate()));
  CHECK_SETARG(aRecorder->setVideoSize(mVideo.GetWidth(), mVideo.GetHeight()));
  CHECK_SETARG(aRecorder->setVideoEncoder(mVideo.GetPlatformCodec()));
  CHECK_SETARG(aRecorder->setAudioEncoder(mAudio.GetPlatformCodec()));

  snprintf(buffer, SIZE, "video-param-encoding-bitrate=%d", mVideo.GetBitrate());
  CHECK_SETARG(aRecorder->setParameters(String8(buffer)));

  snprintf(buffer, SIZE, "audio-param-encoding-bitrate=%d", mAudio.GetBitrate());
  CHECK_SETARG(aRecorder->setParameters(String8(buffer)));

  snprintf(buffer, SIZE, "audio-param-number-of-channels=%d", mAudio.GetChannels());
  CHECK_SETARG(aRecorder->setParameters(String8(buffer)));

  snprintf(buffer, SIZE, "audio-param-sampling-rate=%d", mAudio.GetSamplerate());
  CHECK_SETARG(aRecorder->setParameters(String8(buffer)));

  return NS_OK;
}

/**
 * Recorder audio profile.
 */
static RecorderAudioProfile::Codec
TranslateAudioCodec(audio_encoder aCodec)
{
  switch (aCodec) {
    case AUDIO_ENCODER_AMR_NB:  return RecorderAudioProfile::AMRNB;
    case AUDIO_ENCODER_AMR_WB:  return RecorderAudioProfile::AMRWB;
    case AUDIO_ENCODER_AAC:     return RecorderAudioProfile::AAC;
    default:                    return RecorderAudioProfile::UNKNOWN;
  }
}

GonkRecorderAudioProfile::GonkRecorderAudioProfile(uint32_t aCameraId, uint32_t aQualityIndex)
  : RecorderAudioProfile(aCameraId, aQualityIndex)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p, mCameraId=%d, mQualityIndex=%d\n", __func__, __LINE__, this, mCameraId, mQualityIndex);
  mPlatformCodec = static_cast<audio_encoder>(GetProfileParam(mCameraId, mQualityIndex, "aud.codec"));
  mCodec = TranslateAudioCodec(mPlatformCodec);
  mBitrate = GetProfileParam(mCameraId, mQualityIndex, "aud.bps");
  mSamplerate = GetProfileParam(mCameraId, mQualityIndex, "aud.hz");
  mChannels = GetProfileParam(mCameraId, mQualityIndex, "aud.ch");
}

GonkRecorderAudioProfile::~GonkRecorderAudioProfile()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

/**
 * Recorder video profile.
 */
static RecorderVideoProfile::Codec
TranslateVideoCodec(video_encoder aCodec)
{
  switch (aCodec) {
    case VIDEO_ENCODER_H263:      return RecorderVideoProfile::H263;
    case VIDEO_ENCODER_H264:      return RecorderVideoProfile::H264;
    case VIDEO_ENCODER_MPEG_4_SP: return RecorderVideoProfile::MPEG4SP;
    default:                      return RecorderVideoProfile::UNKNOWN;
  }
}

GonkRecorderVideoProfile::GonkRecorderVideoProfile(uint32_t aCameraId, uint32_t aQualityIndex)
  : RecorderVideoProfile(aCameraId, aQualityIndex)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p, mCameraId=%d, mQualityIndex=%d\n", __func__, __LINE__, this, mCameraId, mQualityIndex);
  mPlatformCodec = static_cast<video_encoder>(GetProfileParam(mCameraId, mQualityIndex, "vid.codec"));
  mCodec = TranslateVideoCodec(mPlatformCodec);
  mBitrate = GetProfileParam(mCameraId, mQualityIndex, "vid.bps");
  mFramerate = GetProfileParam(mCameraId, mQualityIndex, "vid.fps");
  mWidth = GetProfileParam(mCameraId, mQualityIndex, "vid.width");
  mHeight = GetProfileParam(mCameraId, mQualityIndex, "vid.height");
}

GonkRecorderVideoProfile::~GonkRecorderVideoProfile()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

GonkRecorderProfileManager::GonkRecorderProfileManager(uint32_t aCameraId)
  : RecorderProfileManager(aCameraId)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  mMaxQualityIndex = sizeof(ProfileList) / sizeof(ProfileList[0]) - 1;
}

GonkRecorderProfileManager::~GonkRecorderProfileManager()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

bool
GonkRecorderProfileManager::IsSupported(uint32_t aQualityIndex) const
{
  if (!IsQualitySupported(mCameraId, aQualityIndex)) {
    // This profile is not supported
    return false;
  }

  int width = GetProfileParam(mCameraId, aQualityIndex, "vid.width");
  int height = GetProfileParam(mCameraId, aQualityIndex, "vid.height");
  if (width == -1 || height == -1) {
    // This would be unexpected, but we handle it just in case
    DOM_CAMERA_LOGE("Camera %d recorder profile %d has width=%d, height=%d\n", mCameraId, aQualityIndex, width, height);
    return false;
  }

  for (uint32_t i = 0; i < mSupportedSizes.Length(); ++i) {
    if (static_cast<uint32_t>(width) == mSupportedSizes[i].width &&
      static_cast<uint32_t>(height) == mSupportedSizes[i].height)
    {
      return true;
    }
  }
  return false;
}

already_AddRefed<RecorderProfile>
GonkRecorderProfileManager::Get(uint32_t aQualityIndex) const
{
  // This overrides virtual RecorderProfileManager::Get(...)
  DOM_CAMERA_LOGT("%s:%d : aQualityIndex=%d\n", __func__, __LINE__, aQualityIndex);
  nsRefPtr<RecorderProfile> profile = new GonkRecorderProfile(mCameraId, aQualityIndex);
  return profile.forget();
}

already_AddRefed<GonkRecorderProfile>
GonkRecorderProfileManager::Get(const char* aProfileName) const
{
  DOM_CAMERA_LOGT("%s:%d : aProfileName='%s'\n", __func__, __LINE__, aProfileName);
  for (uint32_t i = 0; i < mMaxQualityIndex; ++i) {
    if (strcmp(ProfileList[i].name, aProfileName) == 0) {
      nsRefPtr<GonkRecorderProfile> profile = nullptr;
      if (IsSupported(i)) {
        profile = new GonkRecorderProfile(mCameraId, i);
        return profile.forget();
      }
      return nullptr;
    }
  }

  DOM_CAMERA_LOGW("Couldn't file recorder profile named '%s'\n", aProfileName);
  return nullptr;
}
