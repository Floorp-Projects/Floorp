/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaEngineCameraVideoSource_h
#define MediaEngineCameraVideoSource_h

#include "MediaEngine.h"
#include "MediaTrackConstraints.h"

#include "nsDirectoryServiceDefs.h"

// conflicts with #include of scoped_ptr.h
#undef FF
#include "webrtc/video_engine/include/vie_capture.h"

namespace mozilla {

class MediaEngineCameraVideoSource : public MediaEngineVideoSource
{
public:
  MediaEngineCameraVideoSource(int aIndex,
                               const char* aMonitorName = "Camera.Monitor")
    : MediaEngineVideoSource(kReleased)
    , mMonitor(aMonitorName)
    , mWidth(0)
    , mHeight(0)
    , mInitDone(false)
    , mHasDirectListeners(false)
    , mCaptureIndex(aIndex)
    , mTrackID(0)
    , mFps(-1)
  {}


  virtual void GetName(nsAString& aName) MOZ_OVERRIDE;
  virtual void GetUUID(nsAString& aUUID) MOZ_OVERRIDE;
  virtual void SetDirectListeners(bool aHasListeners) MOZ_OVERRIDE;
  virtual nsresult Config(bool aEchoOn, uint32_t aEcho,
                          bool aAgcOn, uint32_t aAGC,
                          bool aNoiseOn, uint32_t aNoise,
                          int32_t aPlayoutDelay) MOZ_OVERRIDE
  {
    return NS_OK;
  };

  virtual bool IsFake() MOZ_OVERRIDE
  {
    return false;
  }

  virtual const MediaSourceType GetMediaSource() {
      return MediaSourceType::Camera;
  }

  virtual nsresult TakePhoto(PhotoCallback* aCallback) MOZ_OVERRIDE
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

protected:
  ~MediaEngineCameraVideoSource() {}

  // guts for appending data to the MSG track
  virtual bool AppendToTrack(SourceMediaStream* aSource,
                             layers::Image* aImage,
                             TrackID aID,
                             TrackTicks delta);

  static bool IsWithin(int32_t n, const dom::ConstrainLongRange& aRange);
  static bool IsWithin(double n, const dom::ConstrainDoubleRange& aRange);
  static int32_t Clamp(int32_t n, const dom::ConstrainLongRange& aRange);
  static bool AreIntersecting(const dom::ConstrainLongRange& aA,
                              const dom::ConstrainLongRange& aB);
  static bool Intersect(dom::ConstrainLongRange& aA, const dom::ConstrainLongRange& aB);
  void GuessCapability(const VideoTrackConstraintsN& aConstraints,
                       const MediaEnginePrefs& aPrefs);

  // Engine variables.

  // mMonitor protects mImage access/changes, and transitions of mState
  // from kStarted to kStopped (which are combined with EndTrack() and
  // image changes).  Note that mSources is not accessed from other threads
  // for video and is not protected.
  // All the mMonitor accesses are from the child classes.
  Monitor mMonitor; // Monitor for processing Camera frames.
  nsRefPtr<layers::Image> mImage;
  nsRefPtr<layers::ImageContainer> mImageContainer;
  int mWidth, mHeight; // protected with mMonitor on Gonk due to different threading
  // end of data protected by mMonitor

  nsTArray<SourceMediaStream*> mSources; // When this goes empty, we shut down HW

  bool mInitDone;
  bool mHasDirectListeners;
  int mCaptureIndex;
  TrackID mTrackID;
  int mFps; // Track rate (30 fps by default)

  webrtc::CaptureCapability mCapability; // Doesn't work on OS X.

  nsString mDeviceName;
  nsString mUniqueId;
};


} // namespace mozilla
#endif // MediaEngineCameraVideoSource_h
