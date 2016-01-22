/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMEventListener.h"
#include "MediaEngine.h"
#include "ImageContainer.h"
#include "nsITimer.h"
#include "mozilla/Monitor.h"
#include "nsITabSource.h"

namespace mozilla {

class MediaEngineTabVideoSource : public MediaEngineVideoSource, nsIDOMEventListener, nsITimerCallback {
  public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIDOMEVENTLISTENER
    NS_DECL_NSITIMERCALLBACK
    MediaEngineTabVideoSource();

    void Shutdown() override {};
    void GetName(nsAString_internal&) override;
    void GetUUID(nsACString_internal&) override;
    nsresult Allocate(const dom::MediaTrackConstraints &,
                      const mozilla::MediaEnginePrefs&,
                      const nsString& aDeviceId) override;
    nsresult Deallocate() override;
    nsresult Start(mozilla::SourceMediaStream*, mozilla::TrackID) override;
    void SetDirectListeners(bool aHasDirectListeners) override {};
    void NotifyPull(mozilla::MediaStreamGraph*, mozilla::SourceMediaStream*, mozilla::TrackID, mozilla::StreamTime) override;
    nsresult Stop(mozilla::SourceMediaStream*, mozilla::TrackID) override;
    nsresult Restart(const dom::MediaTrackConstraints& aConstraints,
                     const mozilla::MediaEnginePrefs& aPrefs,
                     const nsString& aDeviceId) override;
    nsresult Config(bool, uint32_t, bool, uint32_t, bool, uint32_t, int32_t) override;
    bool IsFake() override;
    dom::MediaSourceEnum GetMediaSource() const override {
      return dom::MediaSourceEnum::Browser;
    }
    uint32_t GetBestFitnessDistance(
      const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) override
    {
      return 0;
    }

    nsresult TakePhoto(PhotoCallback* aCallback) override
    {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    void Draw();

    class StartRunnable : public nsRunnable {
    public:
      explicit StartRunnable(MediaEngineTabVideoSource *videoSource) : mVideoSource(videoSource) {}
      NS_IMETHOD Run();
      RefPtr<MediaEngineTabVideoSource> mVideoSource;
    };

    class StopRunnable : public nsRunnable {
    public:
      explicit StopRunnable(MediaEngineTabVideoSource *videoSource) : mVideoSource(videoSource) {}
      NS_IMETHOD Run();
      RefPtr<MediaEngineTabVideoSource> mVideoSource;
    };

    class InitRunnable : public nsRunnable {
    public:
      explicit InitRunnable(MediaEngineTabVideoSource *videoSource) : mVideoSource(videoSource) {}
      NS_IMETHOD Run();
      RefPtr<MediaEngineTabVideoSource> mVideoSource;
    };

protected:
    ~MediaEngineTabVideoSource() {}

private:
    int32_t mBufWidthMax;
    int32_t mBufHeightMax;
    int64_t mWindowId;
    bool mScrollWithPage;
    int32_t mViewportOffsetX;
    int32_t mViewportOffsetY;
    int32_t mViewportWidth;
    int32_t mViewportHeight;
    int32_t mTimePerFrame;
    ScopedFreePtr<unsigned char> mData;
    size_t mDataSize;
    nsCOMPtr<nsIDOMWindow> mWindow;
    RefPtr<layers::SourceSurfaceImage> mImage;
    nsCOMPtr<nsITimer> mTimer;
    Monitor mMonitor;
    nsCOMPtr<nsITabSource> mTabSource;
  };
}
