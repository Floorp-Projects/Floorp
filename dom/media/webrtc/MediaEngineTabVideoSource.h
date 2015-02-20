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

    virtual void GetName(nsAString_internal&) MOZ_OVERRIDE;
    virtual void GetUUID(nsAString_internal&) MOZ_OVERRIDE;
    virtual nsresult Allocate(const dom::MediaTrackConstraints &,
                              const mozilla::MediaEnginePrefs&) MOZ_OVERRIDE;
    virtual nsresult Deallocate() MOZ_OVERRIDE;
    virtual nsresult Start(mozilla::SourceMediaStream*, mozilla::TrackID) MOZ_OVERRIDE;
    virtual void SetDirectListeners(bool aHasDirectListeners) MOZ_OVERRIDE {};
    virtual void NotifyPull(mozilla::MediaStreamGraph*, mozilla::SourceMediaStream*, mozilla::TrackID, mozilla::StreamTime) MOZ_OVERRIDE;
    virtual nsresult Stop(mozilla::SourceMediaStream*, mozilla::TrackID) MOZ_OVERRIDE;
    virtual nsresult Config(bool, uint32_t, bool, uint32_t, bool, uint32_t, int32_t) MOZ_OVERRIDE;
    virtual bool IsFake() MOZ_OVERRIDE;
    virtual const dom::MediaSourceEnum GetMediaSource() MOZ_OVERRIDE {
      return dom::MediaSourceEnum::Browser;
    }
    virtual uint32_t GetBestFitnessDistance(
      const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets) MOZ_OVERRIDE
    {
      return 0;
    }

    virtual nsresult TakePhoto(PhotoCallback* aCallback) MOZ_OVERRIDE
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
