/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_GONK_RECORDER_PROFILES_H
#define DOM_CAMERA_GONK_RECORDER_PROFILES_H

#ifdef MOZ_WIDGET_GONK
#include <media/MediaProfiles.h>
#else
#include "FallbackCameraPlatform.h"
#endif

#include "ICameraControl.h"
#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"

#ifndef CHECK_SETARG_RETURN
#define CHECK_SETARG_RETURN(x, rv)      \
  do {                                  \
    if (x) {                            \
      DOM_CAMERA_LOGE(#x " failed\n");  \
      return rv;                        \
    }                                   \
  } while(0)
#endif

#ifndef CHECK_SETARG
#define CHECK_SETARG(x) CHECK_SETARG_RETURN(x, NS_ERROR_NOT_AVAILABLE)
#endif

namespace android {
  class GonkRecorder;
};

namespace mozilla {

/**
 * class GonkRecorderProfileBase
 */
template<class A, class V>
class GonkRecorderProfileBase : public ICameraControl::RecorderProfile
{
public:
  GonkRecorderProfileBase(uint32_t aCameraId, int aQuality)
    : RecorderProfile()
    , mAudio(aCameraId, aQuality)
    , mVideo(aCameraId, aQuality)
  { }

  virtual const Audio& GetAudio() const override { return mAudio; }
  virtual const Video& GetVideo() const override { return mVideo; }

protected:
  virtual ~GonkRecorderProfileBase() { }
  A mAudio;
  V mVideo;
};

/**
 * class GonkRecorderVideo
 */
class GonkRecorderVideo : public ICameraControl::RecorderProfile::Video
{
public:
  GonkRecorderVideo(uint32_t aCameraId, int aQuality);
  virtual ~GonkRecorderVideo() { }

  android::video_encoder GetPlatformEncoder() const { return mPlatformEncoder; }
  bool IsValid() const { return mIsValid; }

protected:
  int GetProfileParameter(const char* aParameter);
  static bool Translate(android::video_encoder aCodec, nsAString& aCodecName);

  uint32_t mCameraId;
  int mQuality;
  bool mIsValid;
  android::video_encoder mPlatformEncoder;
};

/**
 * class GonkRecorderAudio
 */
class GonkRecorderAudio : public ICameraControl::RecorderProfile::Audio
{
public:
  GonkRecorderAudio(uint32_t aCameraId, int aQuality);
  virtual ~GonkRecorderAudio() { }

  android::audio_encoder GetPlatformEncoder() const { return mPlatformEncoder; }
  bool IsValid() const { return mIsValid; }

protected:
  int GetProfileParameter(const char* aParameter);
  static bool Translate(android::audio_encoder aCodec, nsAString& aCodecName);

  uint32_t mCameraId;
  int mQuality;
  bool mIsValid;
  android::audio_encoder mPlatformEncoder;
};

/**
 * class GonkRecorderProfile
 */
class GonkRecorderProfile;
typedef nsRefPtrHashtable<nsStringHashKey, GonkRecorderProfile> ProfileHashtable;

class GonkRecorderProfile
  : public GonkRecorderProfileBase<GonkRecorderAudio, GonkRecorderVideo>
{
public:
  static nsresult GetAll(uint32_t aCameraId,
                         nsTArray<RefPtr<ICameraControl::RecorderProfile>>& aProfiles);

#ifdef MOZ_WIDGET_GONK
  // Configures the specified recorder using the specified profile.
  //
  // Return values:
  //  - NS_OK on success;
  //  - NS_ERROR_INVALID_ARG if the profile isn't supported;
  //  - NS_ERROR_NOT_AVAILABLE if the recorder rejected the profile.
  static nsresult ConfigureRecorder(android::GonkRecorder& aRecorder,
                                    uint32_t aCameraId,
                                    const nsAString& aProfileName);
#endif

protected:
  GonkRecorderProfile(uint32_t aCameraId,
                      int aQuality);

  int GetProfileParameter(const char* aParameter);

  bool Translate(android::output_format aContainer, nsAString& aContainerName);
  bool GetMimeType(android::output_format aContainer, nsAString& aMimeType);
  bool IsValid() const { return mIsValid; };

#ifdef MOZ_WIDGET_GONK
  nsresult ConfigureRecorder(android::GonkRecorder& aRecorder);
#endif
  static already_AddRefed<GonkRecorderProfile> CreateProfile(uint32_t aCameraId,
                                                             int aQuality);
  static ProfileHashtable* GetProfileHashtable(uint32_t aCameraId);

  uint32_t mCameraId;
  int mQuality;
  bool mIsValid;
  android::output_format mOutputFormat;

  static nsClassHashtable<nsUint32HashKey, ProfileHashtable> sProfiles;

private:
  DISALLOW_EVIL_CONSTRUCTORS(GonkRecorderProfile);
};

}; // namespace mozilla

#endif // DOM_CAMERA_GONK_RECORDER_PROFILES_H
