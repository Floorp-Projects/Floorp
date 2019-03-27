/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngine.h"
#include "ImageContainer.h"
#include "nsITimer.h"
#include "mozilla/Mutex.h"
#include "mozilla/UniquePtr.h"
#include "nsITabSource.h"

namespace mozilla {

namespace layers {
class ImageContainer;
}

class MediaEngineTabVideoSource : public MediaEngineSource {
 public:
  MediaEngineTabVideoSource();

  nsString GetName() const override;
  nsCString GetUUID() const override;
  nsString GetGroupId() const override;

  bool GetScary() const override { return true; }

  dom::MediaSourceEnum GetMediaSource() const override {
    return dom::MediaSourceEnum::Browser;
  }

  nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                    const MediaEnginePrefs& aPrefs, const nsString& aDeviceId,
                    const ipc::PrincipalInfo& aPrincipalInfo,
                    const char** aOutBadConstraint) override;
  nsresult Deallocate() override;
  void SetTrack(const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
                const PrincipalHandle& aPrincipal) override;
  nsresult Start() override;
  nsresult Reconfigure(const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const nsString& aDeviceId,
                       const char** aOutBadConstraint) override;
  nsresult FocusOnSelectedSource() override;
  nsresult Stop() override;

  uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) const override {
    return 0;
  }

  void Draw();

  class StartRunnable : public Runnable {
   public:
    StartRunnable(MediaEngineTabVideoSource* videoSource,
                  SourceMediaStream* aStream, TrackID aTrackID,
                  const PrincipalHandle& aPrincipal)
        : Runnable("MediaEngineTabVideoSource::StartRunnable"),
          mVideoSource(videoSource),
          mStream(aStream),
          mTrackID(aTrackID),
          mPrincipal(aPrincipal) {}
    NS_IMETHOD Run() override;
    const RefPtr<MediaEngineTabVideoSource> mVideoSource;
    const RefPtr<SourceMediaStream> mStream;
    const TrackID mTrackID;
    const PrincipalHandle mPrincipal;
  };

  class StopRunnable : public Runnable {
   public:
    explicit StopRunnable(MediaEngineTabVideoSource* videoSource)
        : Runnable("MediaEngineTabVideoSource::StopRunnable"),
          mVideoSource(videoSource) {}
    NS_IMETHOD Run() override;
    const RefPtr<MediaEngineTabVideoSource> mVideoSource;
  };

  class InitRunnable : public Runnable {
   public:
    InitRunnable(MediaEngineTabVideoSource* videoSource,
                 SourceMediaStream* aStream, TrackID aTrackID,
                 const PrincipalHandle& aPrincipal)
        : Runnable("MediaEngineTabVideoSource::InitRunnable"),
          mVideoSource(videoSource),
          mStream(aStream),
          mTrackID(aTrackID),
          mPrincipal(aPrincipal) {}
    NS_IMETHOD Run() override;
    const RefPtr<MediaEngineTabVideoSource> mVideoSource;
    const RefPtr<SourceMediaStream> mStream;
    const TrackID mTrackID;
    const PrincipalHandle mPrincipal;
  };

  class DestroyRunnable : public Runnable {
   public:
    explicit DestroyRunnable(MediaEngineTabVideoSource* videoSource)
        : Runnable("MediaEngineTabVideoSource::DestroyRunnable"),
          mVideoSource(videoSource) {}
    NS_IMETHOD Run() override;
    const RefPtr<MediaEngineTabVideoSource> mVideoSource;
  };

 protected:
  ~MediaEngineTabVideoSource() = default;

 private:
  // These are accessed only on main thread.
  int32_t mBufWidthMax = 0;
  int32_t mBufHeightMax = 0;
  int64_t mWindowId = 0;
  bool mScrollWithPage = false;
  int32_t mViewportOffsetX = 0;
  int32_t mViewportOffsetY = 0;
  int32_t mViewportWidth = 0;
  int32_t mViewportHeight = 0;
  int32_t mTimePerFrame = 0;
  RefPtr<layers::ImageContainer> mImageContainer;

  nsCOMPtr<nsPIDOMWindowOuter> mWindow;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsITabSource> mTabSource;
  RefPtr<SourceMediaStream> mStreamMain;
  TrackID mTrackIDMain = TRACK_NONE;
  PrincipalHandle mPrincipalHandleMain = PRINCIPAL_HANDLE_NONE;
  // If this is set, we will run despite mWindow == nullptr.
  bool mBlackedoutWindow = false;

  // Current state of this source. Accessed on owning thread only.
  MediaEngineSourceState mState = kReleased;
  // mStream and mTrackID are set in SetTrack() to keep track of what to end
  // in Deallocate(). Owning thread only.
  RefPtr<SourceMediaStream> mStream;
  TrackID mTrackID = TRACK_NONE;
  PrincipalHandle mPrincipalHandle = PRINCIPAL_HANDLE_NONE;
};

}  // namespace mozilla
