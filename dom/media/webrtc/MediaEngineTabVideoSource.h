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

class MediaEngineTabVideoSource : public MediaEngineSource
{
public:
  MediaEngineTabVideoSource();

  nsString GetName() const override;
  nsCString GetUUID() const override;

  bool GetScary() const override
  {
    return true;
  }

  dom::MediaSourceEnum GetMediaSource() const override
  {
    return dom::MediaSourceEnum::Browser;
  }

  nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                    const MediaEnginePrefs &aPrefs,
                    const nsString& aDeviceId,
                    const ipc::PrincipalInfo& aPrincipalInfo,
                    AllocationHandle** aOutHandle,
                    const char** aOutBadConstraint) override;
  nsresult Deallocate(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult SetTrack(const RefPtr<const AllocationHandle>& aHandle,
                    const RefPtr<SourceMediaStream>& aStream,
                    TrackID aTrackID,
                    const PrincipalHandle& aPrincipal) override;
  nsresult Start(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Reconfigure(const RefPtr<AllocationHandle>& aHandle,
                       const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const nsString& aDeviceId,
                       const char** aOutBadConstraint) override;
  nsresult FocusOnSelectedSource(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Stop(const RefPtr<const AllocationHandle>& aHandle) override;

  void Pull(const RefPtr<const AllocationHandle>& aHandle,
            const RefPtr<SourceMediaStream>& aStream,
            TrackID aTrackID,
            StreamTime aDesiredTime,
            const PrincipalHandle& aPrincipalHandle) override;

  uint32_t GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId) const override
  {
    return 0;
  }

  void Draw();

  class StartRunnable : public Runnable {
  public:
    explicit StartRunnable(MediaEngineTabVideoSource *videoSource)
      : Runnable("MediaEngineTabVideoSource::StartRunnable")
      , mVideoSource(videoSource)
    {}
    NS_IMETHOD Run() override;
    RefPtr<MediaEngineTabVideoSource> mVideoSource;
  };

  class StopRunnable : public Runnable {
  public:
    explicit StopRunnable(MediaEngineTabVideoSource *videoSource)
      : Runnable("MediaEngineTabVideoSource::StopRunnable")
      , mVideoSource(videoSource)
    {}
    NS_IMETHOD Run() override;
    RefPtr<MediaEngineTabVideoSource> mVideoSource;
  };

  class InitRunnable : public Runnable {
  public:
    explicit InitRunnable(MediaEngineTabVideoSource *videoSource)
      : Runnable("MediaEngineTabVideoSource::InitRunnable")
      , mVideoSource(videoSource)
    {}
    NS_IMETHOD Run() override;
    RefPtr<MediaEngineTabVideoSource> mVideoSource;
  };

  class DestroyRunnable : public Runnable {
  public:
    explicit DestroyRunnable(MediaEngineTabVideoSource* videoSource)
      : Runnable("MediaEngineTabVideoSource::DestroyRunnable")
      , mVideoSource(videoSource)
    {}
    NS_IMETHOD Run() override;
    RefPtr<MediaEngineTabVideoSource> mVideoSource;
  };

protected:
  ~MediaEngineTabVideoSource() {}

private:
  int32_t mBufWidthMax = 0;
  int32_t mBufHeightMax = 0;
  int64_t mWindowId = 0;
  bool mScrollWithPage = 0;
  int32_t mViewportOffsetX = 0;
  int32_t mViewportOffsetY = 0;
  int32_t mViewportWidth = 0;
  int32_t mViewportHeight = 0;
  int32_t mTimePerFrame = 0;
  UniquePtr<unsigned char[]> mData;
  size_t mDataSize = 0;
  nsCOMPtr<nsPIDOMWindowOuter> mWindow;
  // If this is set, we will run despite mWindow == nullptr.
  bool mBlackedoutWindow = false;
  // Current state of this source.
  // Written on owning thread *and* under mMutex.
  // Can be read on owning thread *or* under mMutex.
  MediaEngineSourceState mState = kReleased;
  // mStream and mTrackID are set in SetTrack() to keep track of what to end
  // in Deallocate().
  // Owning thread only.
  RefPtr<SourceMediaStream> mStream;
  TrackID mTrackID = TRACK_NONE;
  // mImage and mImageSize is Protected by mMutex.
  RefPtr<layers::SourceSurfaceImage> mImage;
  gfx::IntSize mImageSize;
  nsCOMPtr<nsITimer> mTimer;
  Mutex mMutex;
  nsCOMPtr<nsITabSource> mTabSource;
};

} // namespace mozilla
