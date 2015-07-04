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

    virtual void Shutdown() override {};
    virtual void GetName(nsAString_internal&) override;
    virtual void GetUUID(nsACString_internal&) override;
    virtual nsresult Allocate(const dom::MediaTrackConstraints &,
                              const mozilla::MediaEnginePrefs&,
                              const nsString& aDeviceId) override;
    virtual nsresult Deallocate() override;
    virtual nsresult Start(mozilla::SourceMediaStream*, mozilla::TrackID) override;
    virtual void SetDirectListeners(bool aHasDirectListeners) override {};
    virtual void NotifyPull(mozilla::MediaStreamGraph*, mozilla::SourceMediaStream*, mozilla::TrackID, mozilla::StreamTime) override;
    virtual nsresult Stop(mozilla::SourceMediaStream*, mozilla::TrackID) override;
    virtual nsresult Config(bool, uint32_t, bool, uint32_t, bool, uint32_t, int32_t) override;
    virtual bool IsFake() override;
    virtual const dom::MediaSourceEnum GetMediaSource() override {
      return dom::MediaSourceEnum::Browser;
    }
    virtual uint32_t GetBestFitnessDistance(
      const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) override
    {
      return 0;
    }

    virtual nsresult TakePhoto(PhotoCallback* aCallback) override
    {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    void Draw();

    class StartRunnable : public nsRunnable {
    public:
      explicit StartRunnable(MediaEngineTabVideoSource *videoSource) : mVideoSource(videoSource) {}
      NS_IMETHOD Run();
      nsRefPtr<MediaEngineTabVideoSource> mVideoSource;
    };

    class StopRunnable : public nsRunnable {
    public:
      explicit StopRunnable(MediaEngineTabVideoSource *videoSource) : mVideoSource(videoSource) {}
      NS_IMETHOD Run();
      nsRefPtr<MediaEngineTabVideoSource> mVideoSource;
    };

    class InitRunnable : public nsRunnable {
    public:
      explicit InitRunnable(MediaEngineTabVideoSource *videoSource) : mVideoSource(videoSource) {}
      NS_IMETHOD Run();
      nsRefPtr<MediaEngineTabVideoSource> mVideoSource;
    };

protected:
    ~MediaEngineTabVideoSource() {}

private:
    int mBufWidthMax;
    int mBufHeightMax;
    int64_t mWindowId;
    bool mScrollWithPage;
    int mTimePerFrame;
    ScopedFreePtr<unsigned char> mData;
    size_t mDataSize;
    nsCOMPtr<nsIDOMWindow> mWindow;
    nsRefPtr<layers::CairoImage> mImage;
    nsCOMPtr<nsITimer> mTimer;
    Monitor mMonitor;
    nsCOMPtr<nsITabSource> mTabSource;
  };
}
