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

    virtual void GetName(nsAString_internal&);
    virtual void GetUUID(nsAString_internal&);
    virtual nsresult Allocate(const VideoTrackConstraintsN &,
                              const mozilla::MediaEnginePrefs&);
    virtual nsresult Deallocate();
    virtual nsresult Start(mozilla::SourceMediaStream*, mozilla::TrackID);
    virtual void SetDirectListeners(bool aHasDirectListeners) {};
    virtual nsresult Snapshot(uint32_t, nsIDOMFile**);
    virtual void NotifyPull(mozilla::MediaStreamGraph*, mozilla::SourceMediaStream*, mozilla::TrackID, mozilla::StreamTime, mozilla::TrackTicks&);
    virtual nsresult Stop(mozilla::SourceMediaStream*, mozilla::TrackID);
    virtual nsresult Config(bool, uint32_t, bool, uint32_t, bool, uint32_t, int32_t);
    virtual bool IsFake();
    virtual const MediaSourceType GetMediaSource() {
      return MediaSourceType::Browser;
    }

    void Draw();

    class StartRunnable : public nsRunnable {
    public:
      StartRunnable(MediaEngineTabVideoSource *videoSource) : mVideoSource(videoSource) {}
      NS_IMETHOD Run();
      nsRefPtr<MediaEngineTabVideoSource> mVideoSource;
    };

    class StopRunnable : public nsRunnable {
    public:
    StopRunnable(MediaEngineTabVideoSource *videoSource) : mVideoSource(videoSource) {}
      NS_IMETHOD Run();
      nsRefPtr<MediaEngineTabVideoSource> mVideoSource;
    };

    class InitRunnable : public nsRunnable {
    public:
    InitRunnable(MediaEngineTabVideoSource *videoSource) : mVideoSource(videoSource) {}
      NS_IMETHOD Run();
      nsRefPtr<MediaEngineTabVideoSource> mVideoSource;
    };

protected:
    ~MediaEngineTabVideoSource() {}

private:
    int mBufW;
    int mBufH;
    int mTimePerFrame;
    ScopedFreePtr<unsigned char> mData;
    nsCOMPtr<nsIDOMWindow> mWindow;
    nsRefPtr<layers::CairoImage> mImage;
    nsCOMPtr<nsITimer> mTimer;
    Monitor mMonitor;
    nsCOMPtr<nsITabSource> mTabSource;
  };
}
