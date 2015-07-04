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

class MediaEngineCameraVideoSource : public MediaEngineVideoSource,
                                     private MediaConstraintsHelper
{
public:
  explicit MediaEngineCameraVideoSource(int aIndex,
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


  virtual void GetName(nsAString& aName) override;
  virtual void GetUUID(nsACString& aUUID) override;
  virtual void SetDirectListeners(bool aHasListeners) override;
  virtual nsresult Config(bool aEchoOn, uint32_t aEcho,
                          bool aAgcOn, uint32_t aAGC,
                          bool aNoiseOn, uint32_t aNoise,
                          int32_t aPlayoutDelay) override
  {
    return NS_OK;
  };

  virtual bool IsFake() override
  {
    return false;
  }

  virtual const dom::MediaSourceEnum GetMediaSource() override {
      return dom::MediaSourceEnum::Camera;
  }

  virtual nsresult TakePhoto(PhotoCallback* aCallback) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  uint32_t GetBestFitnessDistance(
      const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) override;

  virtual void Shutdown() override {};

protected:
  struct CapabilityCandidate {
    explicit CapabilityCandidate(uint8_t index, uint32_t distance = 0)
    : mIndex(index), mDistance(distance) {}

    size_t mIndex;
    uint32_t mDistance;
  };
  typedef nsTArray<CapabilityCandidate> CapabilitySet;

  ~MediaEngineCameraVideoSource() {}

  // guts for appending data to the MSG track
  virtual bool AppendToTrack(SourceMediaStream* aSource,
                             layers::Image* aImage,
                             TrackID aID,
                             StreamTime delta);
  uint32_t GetFitnessDistance(const webrtc::CaptureCapability& aCandidate,
                              const dom::MediaTrackConstraintSet &aConstraints,
                              bool aAdvanced,
                              const nsString& aDeviceId);
  static void TrimLessFitCandidates(CapabilitySet& set);
  static void LogConstraints(const dom::MediaTrackConstraintSet& aConstraints,
                             bool aAdvanced);
  virtual size_t NumCapabilities();
  virtual void GetCapability(size_t aIndex, webrtc::CaptureCapability& aOut);
  bool ChooseCapability(const dom::MediaTrackConstraints &aConstraints,
                        const MediaEnginePrefs &aPrefs,
                        const nsString& aDeviceId);
  void SetName(nsString aName);
  void SetUUID(const char* aUUID);
  const nsCString& GetUUID(); // protected access

  // Engine variables.

  // mMonitor protects mImage access/changes, and transitions of mState
  // from kStarted to kStopped (which are combined with EndTrack() and
  // image changes).
  // mMonitor also protects mSources[] access/changes.
  // mSources[] is accessed from webrtc threads.

  // All the mMonitor accesses are from the child classes.
  Monitor mMonitor; // Monitor for processing Camera frames.
  nsTArray<nsRefPtr<SourceMediaStream>> mSources; // When this goes empty, we shut down HW
  nsRefPtr<layers::Image> mImage;
  nsRefPtr<layers::ImageContainer> mImageContainer;
  int mWidth, mHeight; // protected with mMonitor on Gonk due to different threading
  // end of data protected by mMonitor


  bool mInitDone;
  bool mHasDirectListeners;
  int mCaptureIndex;
  TrackID mTrackID;
  int mFps; // Track rate (30 fps by default)

  webrtc::CaptureCapability mCapability; // Doesn't work on OS X.

  nsTArray<webrtc::CaptureCapability> mHardcodedCapabilities; // For OSX & B2G
private:
  nsString mDeviceName;
  nsCString mUniqueId;
  nsString mFacingMode;
};


} // namespace mozilla
#endif // MediaEngineCameraVideoSource_h
