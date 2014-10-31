/*
 * Copyright (C) 2012-2014 Mozilla Foundation
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

#include "GonkRecorderProfiles.h"
#include <media/MediaProfiles.h>
#include "nsMimeTypes.h"
#include "GonkRecorder.h"
#include "CameraControlImpl.h"
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

/* static */ nsClassHashtable<nsUint32HashKey, ProfileHashtable> GonkRecorderProfile::sProfiles;
/* static */ android::MediaProfiles* sMediaProfiles = nullptr;

static MediaProfiles*
GetMediaProfiles()
{
  if (!sMediaProfiles) {
    sMediaProfiles = MediaProfiles::getInstance();
  }
  MOZ_ASSERT(sMediaProfiles);
  return sMediaProfiles;
}

static bool
IsProfileSupported(uint32_t aCameraId, uint32_t aProfileIndex)
{
  MediaProfiles* profiles = GetMediaProfiles();
  camcorder_quality q = static_cast<camcorder_quality>(ProfileList[aProfileIndex].quality);
  return profiles->hasCamcorderProfile(static_cast<int>(aCameraId), q);
}

static int
GetProfileParameter(uint32_t aCameraId, uint32_t aProfileIndex, const char* aParameter)
{
  MediaProfiles* profiles = GetMediaProfiles();
  camcorder_quality q = static_cast<camcorder_quality>(ProfileList[aProfileIndex].quality);
  return profiles->getCamcorderProfileParamByName(aParameter, static_cast<int>(aCameraId), q);
}

/* static */ bool
GonkRecorderVideo::Translate(video_encoder aCodec, nsAString& aCodecName)
{
  switch (aCodec) {
    case VIDEO_ENCODER_H263:
      aCodecName.AssignASCII("h263");
      break;

    case VIDEO_ENCODER_H264:
      aCodecName.AssignASCII("h264");
      break;

    case VIDEO_ENCODER_MPEG_4_SP:
      aCodecName.AssignASCII("mpeg4sp");
      break;

    default:
      return false;
  }

  return true;
}

int
GonkRecorderVideo::GetProfileParameter(const char* aParameter)
{
  return ::GetProfileParameter(mCameraId, mProfileIndex, aParameter);
}

GonkRecorderVideo::GonkRecorderVideo(uint32_t aCameraId, uint32_t aProfileIndex)
  : mCameraId(aCameraId)
  , mProfileIndex(aProfileIndex)
  , mIsValid(false)
{
  mPlatformEncoder = static_cast<video_encoder>(GetProfileParameter("vid.codec"));
  bool isValid = Translate(mPlatformEncoder, mCodec);

  int v = GetProfileParameter("vid.width");
  if (v >= 0) {
    mSize.width = v;
  } else {
    isValid = false;
  }
  v = GetProfileParameter("vid.height");
  if (v >= 0) {
    mSize.height = v;
  } else {
    isValid = false;
  }
  v = GetProfileParameter("vid.bps");
  if (v >= 0) {
    mBitsPerSecond = v;
  } else {
    isValid = false;
  }
  v = GetProfileParameter("vid.fps");
  if (v >= 0) {
    mFramesPerSecond = v;
  } else {
    isValid = false;
  }

  mIsValid = isValid;
}

/* static */ bool
GonkRecorderAudio::Translate(audio_encoder aCodec, nsAString& aCodecName)
{
  switch (aCodec) {
    case AUDIO_ENCODER_AMR_NB:
      aCodecName.AssignASCII("amrnb");
      break;

    case AUDIO_ENCODER_AMR_WB:
      aCodecName.AssignASCII("amrwb");
      break;

    case AUDIO_ENCODER_AAC:
      aCodecName.AssignASCII("aac");
      break;

    default:
      return false;
  }

  return true;
}

int
GonkRecorderAudio::GetProfileParameter(const char* aParameter)
{
  return ::GetProfileParameter(mCameraId, mProfileIndex, aParameter);
}

GonkRecorderAudio::GonkRecorderAudio(uint32_t aCameraId, uint32_t aProfileIndex)
  : mCameraId(aCameraId)
  , mProfileIndex(aProfileIndex)
  , mIsValid(false)
{
  mPlatformEncoder = static_cast<audio_encoder>(GetProfileParameter("aud.codec"));
  bool isValid = Translate(mPlatformEncoder, mCodec);

  int v = GetProfileParameter("aud.ch");
  if (v >= 0) {
    mChannels = v;
  } else {
    isValid = false;
  }
  v = GetProfileParameter("aud.bps");
  if (v >= 0) {
    mBitsPerSecond = v;
  } else {
    isValid = false;
  }
  v = GetProfileParameter("aud.hz");
  if (v >= 0) {
    mSamplesPerSecond = v;
  } else {
    isValid = false;
  }

  mIsValid = isValid;
}

/* static */ bool
GonkRecorderProfile::Translate(output_format aContainer, nsAString& aContainerName)
{
  switch (aContainer) {
    case OUTPUT_FORMAT_THREE_GPP:
      aContainerName.AssignASCII("3gp");
      break;

    case OUTPUT_FORMAT_MPEG_4:
      aContainerName.AssignASCII("mp4");
      break;

    default:
      return false;
  }

  return true;
}

/* static */ bool
GonkRecorderProfile::GetMimeType(output_format aContainer, nsAString& aMimeType)
{
  switch (aContainer) {
    case OUTPUT_FORMAT_THREE_GPP:
      aMimeType.AssignASCII(VIDEO_3GPP);
      break;

    case OUTPUT_FORMAT_MPEG_4:
      aMimeType.AssignASCII(VIDEO_MP4);
      break;

    default:
      return false;
  }

  return true;
}

int
GonkRecorderProfile::GetProfileParameter(const char* aParameter)
{
  return ::GetProfileParameter(mCameraId, mProfileIndex, aParameter);
}

GonkRecorderProfile::GonkRecorderProfile(uint32_t aCameraId,
                                         uint32_t aProfileIndex,
                                         const nsAString& aName)
  : GonkRecorderProfileBase<GonkRecorderAudio, GonkRecorderVideo>(aCameraId,
                                                                  aProfileIndex, aName)
  , mCameraId(aCameraId)
  , mProfileIndex(aProfileIndex)
  , mIsValid(false)
{
  mOutputFormat = static_cast<output_format>(GetProfileParameter("file.format"));
  bool isValid = Translate(mOutputFormat, mContainer);
  isValid = GetMimeType(mOutputFormat, mMimeType) ? isValid : false;

  mIsValid = isValid && mAudio.IsValid() && mVideo.IsValid();
}

/* static */ PLDHashOperator
GonkRecorderProfile::Enumerate(const nsAString& aProfileName,
                               GonkRecorderProfile* aProfile,
                               void* aUserArg)
{
  nsTArray<nsRefPtr<ICameraControl::RecorderProfile>>* profiles =
    static_cast<nsTArray<nsRefPtr<ICameraControl::RecorderProfile>>*>(aUserArg);
  MOZ_ASSERT(profiles);
  profiles->AppendElement(aProfile);
  return PL_DHASH_NEXT;
}

/* static */
ProfileHashtable*
GonkRecorderProfile::GetProfileHashtable(uint32_t aCameraId)
{
  ProfileHashtable* profiles = sProfiles.Get(aCameraId);
  if (!profiles) {
    profiles = new ProfileHashtable;
    sProfiles.Put(aCameraId, profiles);

    for (uint32_t i = 0; ProfileList[i].name; ++i) {
      if (IsProfileSupported(aCameraId, i)) {
        DOM_CAMERA_LOGI("Profile %d '%s' supported by platform\n", i, ProfileList[i].name);
        nsAutoString name;
        name.AssignASCII(ProfileList[i].name);
        nsRefPtr<GonkRecorderProfile> profile = new GonkRecorderProfile(aCameraId, i, name);
        if (!profile->IsValid()) {
          DOM_CAMERA_LOGE("Profile %d '%s' is not valid\n", i, ProfileList[i].name);
          continue;
        }
        profiles->Put(name, profile);
      } else {
        DOM_CAMERA_LOGI("Profile %d '%s' not supported by platform\n", i, ProfileList[i].name);
      }
    }
  }
  return profiles;
}

/* static */ nsresult
GonkRecorderProfile::GetAll(uint32_t aCameraId,
                            nsTArray<nsRefPtr<ICameraControl::RecorderProfile>>& aProfiles)
{
  ProfileHashtable* profiles = GetProfileHashtable(aCameraId);
  if (!profiles) {
    return NS_ERROR_FAILURE;
  }

  aProfiles.Clear();
  profiles->EnumerateRead(Enumerate, static_cast<void*>(&aProfiles));
  
  return NS_OK;
}

nsresult
GonkRecorderProfile::ConfigureRecorder(GonkRecorder& aRecorder)
{
  static const size_t SIZE = 256;
  char buffer[SIZE];

  // set all the params
  CHECK_SETARG(aRecorder.setAudioSource(AUDIO_SOURCE_CAMCORDER));
  CHECK_SETARG(aRecorder.setVideoSource(VIDEO_SOURCE_CAMERA));
  CHECK_SETARG(aRecorder.setOutputFormat(mOutputFormat));
  CHECK_SETARG(aRecorder.setVideoFrameRate(mVideo.GetFramesPerSecond()));
  CHECK_SETARG(aRecorder.setVideoSize(mVideo.GetSize().width, mVideo.GetSize().height));
  CHECK_SETARG(aRecorder.setVideoEncoder(mVideo.GetPlatformEncoder()));
  CHECK_SETARG(aRecorder.setAudioEncoder(mAudio.GetPlatformEncoder()));

  snprintf(buffer, SIZE, "video-param-encoding-bitrate=%d", mVideo.GetBitsPerSecond());
  CHECK_SETARG(aRecorder.setParameters(String8(buffer)));

  snprintf(buffer, SIZE, "audio-param-encoding-bitrate=%d", mAudio.GetBitsPerSecond());
  CHECK_SETARG(aRecorder.setParameters(String8(buffer)));

  snprintf(buffer, SIZE, "audio-param-number-of-channels=%d", mAudio.GetChannels());
  CHECK_SETARG(aRecorder.setParameters(String8(buffer)));

  snprintf(buffer, SIZE, "audio-param-sampling-rate=%d", mAudio.GetSamplesPerSecond());
  CHECK_SETARG(aRecorder.setParameters(String8(buffer)));

  return NS_OK;
}

/* static */ nsresult
GonkRecorderProfile::ConfigureRecorder(android::GonkRecorder& aRecorder,
                                       uint32_t aCameraId,
                                       const nsAString& aProfileName)
{
  ProfileHashtable* profiles = GetProfileHashtable(aCameraId);
  if (!profiles) {
    return NS_ERROR_FAILURE;
  }

  GonkRecorderProfile* profile;
  if (!profiles->Get(aProfileName, &profile)) {
    return NS_ERROR_INVALID_ARG;
  }

  return profile->ConfigureRecorder(aRecorder);
}
