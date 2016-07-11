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

bool operator == (const webrtc::CaptureCapability& a,
                  const webrtc::CaptureCapability& b);
bool operator != (const webrtc::CaptureCapability& a,
                  const webrtc::CaptureCapability& b);

class MediaEngineCameraVideoSource : public MediaEngineVideoSource,
                                     protected MediaConstraintsHelper
{
public:
  // Some subclasses use an index to track multiple instances.
  explicit MediaEngineCameraVideoSource(int aIndex,
                                        const char* aMonitorName = "Camera.Monitor")
    : MediaEngineVideoSource(kReleased)
    , mMonitor(aMonitorName)
    , mWidth(0)
    , mHeight(0)
    , mInitDone(false)
    , mHasDirectListeners(false)
    , mNrAllocations(0)
    , mCaptureIndex(aIndex)
    , mTrackID(0)
  {}

  explicit MediaEngineCameraVideoSource(const char* aMonitorName = "Camera.Monitor")
    : MediaEngineCameraVideoSource(0, aMonitorName) {}

  void GetName(nsAString& aName) const override;
  void GetUUID(nsACString& aUUID) const override;
  void SetDirectListeners(bool aHasListeners) override;

  bool IsFake() override
  {
    return false;
  }

  nsresult TakePhoto(MediaEnginePhotoCallback* aCallback) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) const override;

  void Shutdown() override {};

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
                             StreamTime delta,
                             const PrincipalHandle& aPrincipalHandle);
  uint32_t GetFitnessDistance(const webrtc::CaptureCapability& aCandidate,
                              const NormalizedConstraintSet &aConstraints,
                              const nsString& aDeviceId) const;
  static void TrimLessFitCandidates(CapabilitySet& set);
  static void LogConstraints(const NormalizedConstraintSet& aConstraints);
  static void LogCapability(const char* aHeader,
                            const webrtc::CaptureCapability &aCapability,
                            uint32_t aDistance);
  virtual size_t NumCapabilities() const;
  virtual void GetCapability(size_t aIndex, webrtc::CaptureCapability& aOut) const;
  virtual bool ChooseCapability(const NormalizedConstraints &aConstraints,
                                const MediaEnginePrefs &aPrefs,
                                const nsString& aDeviceId);
  void SetName(nsString aName);
  void SetUUID(const char* aUUID);
  const nsCString& GetUUID() const; // protected access

  // Engine variables.

  // mMonitor protects mImage access/changes, and transitions of mState
  // from kStarted to kStopped (which are combined with EndTrack() and
  // image changes).
  // mMonitor also protects mSources[] and mPrincipalHandles[] access/changes.
  // mSources[] and mPrincipalHandles[] are accessed from webrtc threads.

  // All the mMonitor accesses are from the child classes.
  Monitor mMonitor; // Monitor for processing Camera frames.
  nsTArray<RefPtr<SourceMediaStream>> mSources; // When this goes empty, we shut down HW
  nsTArray<PrincipalHandle> mPrincipalHandles; // Directly mapped to mSources.
  RefPtr<layers::Image> mImage;
  RefPtr<layers::ImageContainer> mImageContainer;
  int mWidth, mHeight; // protected with mMonitor on Gonk due to different threading
  // end of data protected by mMonitor


  bool mInitDone;
  bool mHasDirectListeners;
  int mNrAllocations; // When this becomes 0, we shut down HW
  int mCaptureIndex;
  TrackID mTrackID;

  webrtc::CaptureCapability mCapability;

  mutable nsTArray<webrtc::CaptureCapability> mHardcodedCapabilities;
private:
  nsString mDeviceName;
  nsCString mUniqueId;
  nsString mFacingMode;
};


} // namespace mozilla

#endif // MediaEngineCameraVideoSource_h
