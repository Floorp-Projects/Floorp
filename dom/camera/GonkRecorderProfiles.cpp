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
#include "nsMimeTypes.h"
#include "CameraControlImpl.h"
#include "CameraCommon.h"

#ifdef MOZ_WIDGET_GONK
#include "GonkRecorder.h"
#endif

using namespace mozilla;
using namespace android;

namespace mozilla {

struct ProfileConfig {
  const char* name;
  int quality;
  uint32_t priority;
};

#define DEF_GONK_RECORDER_PROFILE(e, n, p) { n, e, p },
static const ProfileConfig ProfileList[] = {
  #include "GonkRecorderProfiles.def"
};

static const size_t ProfileListSize = MOZ_ARRAY_LENGTH(ProfileList);

struct ProfileConfigDetect {
  const char* name;
  uint32_t width;
  uint32_t height;
  uint32_t priority;
};

#define DEF_GONK_RECORDER_PROFILE_DETECT(n, w, h, p) { n, w, h, p },
static const ProfileConfigDetect ProfileListDetect[] = {
  #include "GonkRecorderProfiles.def"
};

static const size_t ProfileListDetectSize = MOZ_ARRAY_LENGTH(ProfileListDetect);

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
IsProfileSupported(uint32_t aCameraId, int aQuality)
{
  MediaProfiles* profiles = GetMediaProfiles();
  return profiles->hasCamcorderProfile(static_cast<int>(aCameraId),
                                       static_cast<camcorder_quality>(aQuality));
}

static int
GetProfileParameter(uint32_t aCameraId, int aQuality, const char* aParameter)
{
  MediaProfiles* profiles = GetMediaProfiles();
  return profiles->getCamcorderProfileParamByName(aParameter, static_cast<int>(aCameraId),
                                                  static_cast<camcorder_quality>(aQuality));
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
  return ::GetProfileParameter(mCameraId, mQuality, aParameter);
}

GonkRecorderVideo::GonkRecorderVideo(uint32_t aCameraId, int aQuality)
  : mCameraId(aCameraId)
  , mQuality(aQuality)
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
  return ::GetProfileParameter(mCameraId, mQuality, aParameter);
}

GonkRecorderAudio::GonkRecorderAudio(uint32_t aCameraId, int aQuality)
  : mCameraId(aCameraId)
  , mQuality(aQuality)
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
  return ::GetProfileParameter(mCameraId, mQuality, aParameter);
}

GonkRecorderProfile::GonkRecorderProfile(uint32_t aCameraId,
                                         int aQuality)
  : GonkRecorderProfileBase<GonkRecorderAudio, GonkRecorderVideo>(aCameraId,
                                                                  aQuality)
  , mCameraId(aCameraId)
  , mQuality(aQuality)
  , mIsValid(false)
{
  mOutputFormat = static_cast<output_format>(GetProfileParameter("file.format"));
  bool isValid = Translate(mOutputFormat, mContainer);
  isValid = GetMimeType(mOutputFormat, mMimeType) ? isValid : false;

  mIsValid = isValid && mAudio.IsValid() && mVideo.IsValid();
}

/* static */
already_AddRefed<GonkRecorderProfile>
GonkRecorderProfile::CreateProfile(uint32_t aCameraId, int aQuality)
{
  if (!IsProfileSupported(aCameraId, aQuality)) {
    DOM_CAMERA_LOGI("Profile %d not supported by platform\n", aQuality);
    return nullptr;
  }

  RefPtr<GonkRecorderProfile> profile = new GonkRecorderProfile(aCameraId, aQuality);
  if (!profile->IsValid()) {
    DOM_CAMERA_LOGE("Profile %d is not valid\n", aQuality);
    return nullptr;
  }

  return profile.forget();
}

/* static */
ProfileHashtable*
GonkRecorderProfile::GetProfileHashtable(uint32_t aCameraId)
{
  ProfileHashtable* profiles = sProfiles.Get(aCameraId);
  if (!profiles) {
    profiles = new ProfileHashtable();
    sProfiles.Put(aCameraId, profiles);

    /* First handle the profiles with a known enum. We can process those
       efficently because MediaProfiles indexes their profiles that way. */
    int highestKnownQuality = CAMCORDER_QUALITY_LIST_START - 1;
    for (size_t i = 0; i < ProfileListSize; ++i) {
      const ProfileConfig& p = ProfileList[i];
      if (p.quality > highestKnownQuality) {
        highestKnownQuality = p.quality;
      }

      RefPtr<GonkRecorderProfile> profile = CreateProfile(aCameraId, p.quality);
      if (!profile) {
        continue;
      }

      DOM_CAMERA_LOGI("Profile %d '%s' supported by platform\n", p.quality, p.name);
      profile->mName.AssignASCII(p.name);
      profile->mPriority = p.priority;
      profiles->Put(profile->GetName(), profile);
    }

    /* However not all of the potentially supported profiles have a known
       enum on all of our supported platforms because some entries may
       be missing from MediaProfiles.h. As such, we can't rely upon
       having the CAMCORDER_QUALITY_* enums for those profiles. We need
       to map the profiles to a name by matching the width and height of
       the video resolution to our configured values.

       In theory there may be collisions given that there can be multiple
       resolutions sharing the same name (e.g. 800x480 and 768x480 are both
       wvga). In practice this should not happen because there should be
       only one WVGA profile given there is only one enum for it. In the
       situation there is a collision, it will merely select the last
       detected profile. */
    for (int q = highestKnownQuality + 1; q <= CAMCORDER_QUALITY_LIST_END; ++q) {
      RefPtr<GonkRecorderProfile> profile = CreateProfile(aCameraId, q);
      if (!profile) {
        continue;
      }

      const ICameraControl::Size& s = profile->GetVideo().GetSize();
      size_t match;
      for (match = 0; match < ProfileListDetectSize; ++match) {
        const ProfileConfigDetect& p = ProfileListDetect[match];
        if (s.width == p.width && s.height == p.height) {
          DOM_CAMERA_LOGI("Profile %d '%s' supported by platform\n", q, p.name);
          profile->mName.AssignASCII(p.name);
          profile->mPriority = p.priority;
          profiles->Put(profile->GetName(), profile);
          break;
        }
      }

      if (match == ProfileListDetectSize) {
        DOM_CAMERA_LOGW("Profile %d size %u x %u is not recognized\n",
                        q, s.width, s.height);
      }
    }
  }
  return profiles;
}

/* static */ nsresult
GonkRecorderProfile::GetAll(uint32_t aCameraId,
                            nsTArray<RefPtr<ICameraControl::RecorderProfile>>& aProfiles)
{
  ProfileHashtable* profiles = GetProfileHashtable(aCameraId);
  if (!profiles) {
    return NS_ERROR_FAILURE;
  }

  aProfiles.Clear();
  for (auto iter = profiles->Iter(); !iter.Done(); iter.Next()) {
    aProfiles.AppendElement(iter.UserData());
  }

  return NS_OK;
}

#ifdef MOZ_WIDGET_GONK
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
#endif
