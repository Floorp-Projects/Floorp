/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngine.h"
#include "mozilla/Services.h"

#include "nsHashKeys.h"
#include "nsGlobalWindow.h"
#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsObserverService.h"

#include "nsPIDOMWindow.h"
#include "nsIDOMNavigatorUserMedia.h"
#include "mozilla/Attributes.h"
#include "prlog.h"

namespace mozilla {

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaManagerLog;
#define MM_LOG(msg) PR_LOG(gMediaManagerLog, PR_LOG_DEBUG, msg)
#else
#define MM_LOG(msg)
#endif

// We only support 1 audio and 1 video track for now.
enum {
  kVideoTrack = 1,
  kAudioTrack = 2
};

class GetUserMediaNotificationEvent: public nsRunnable
{
  public:
    enum GetUserMediaStatus {
      STARTING,
      STOPPING
    };
    GetUserMediaNotificationEvent(GetUserMediaStatus aStatus)
    : mStatus(aStatus) {}

    NS_IMETHOD
    Run()
    {
      nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
      if (!obs) {
        NS_WARNING("Could not get the Observer service for GetUserMedia recording notification.");
        return NS_ERROR_FAILURE;
      }
      if (mStatus) {
        obs->NotifyObservers(nullptr,
            "recording-device-events",
            NS_LITERAL_STRING("starting").get());
      } else {
        obs->NotifyObservers(nullptr,
            "recording-device-events",
            NS_LITERAL_STRING("shutdown").get());
      }
      return NS_OK;
    }

  protected:
    GetUserMediaStatus mStatus;
};

/**
 * This class is an implementation of MediaStreamListener. This is used
 * to Start() and Stop() the underlying MediaEngineSource when MediaStreams
 * are assigned and deassigned in content.
 */
class GetUserMediaCallbackMediaStreamListener : public MediaStreamListener
{
public:
  GetUserMediaCallbackMediaStreamListener(nsDOMMediaStream* aStream,
    MediaEngineSource* aAudioSource,
    MediaEngineSource* aVideoSource)
    : mAudioSource(aAudioSource)
    , mVideoSource(aVideoSource)
    , mStream(aStream)
    , mValid(true) {}

  void
  Invalidate()
  {
    if (!mValid) {
      return;
    }

    mValid = false;
    if (mAudioSource) {
      mAudioSource->Stop();
      mAudioSource->Deallocate();
    }
    if (mVideoSource) {
      mVideoSource->Stop();
      mVideoSource->Deallocate();
    }
    nsCOMPtr<GetUserMediaNotificationEvent> event =
      new GetUserMediaNotificationEvent(GetUserMediaNotificationEvent::STOPPING);

    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }

  void
  NotifyConsumptionChanged(MediaStreamGraph* aGraph, Consumption aConsuming)
  {
    if (aConsuming == CONSUMED) {
      nsresult rv;

      SourceMediaStream* stream = mStream->GetStream()->AsSourceStream();
      if (mAudioSource) {
        rv = mAudioSource->Start(stream, kAudioTrack);
        if (NS_FAILED(rv)) {
          MM_LOG(("Starting audio failed, rv=%d",rv));
        }
      }
      if (mVideoSource) {
        rv = mVideoSource->Start(stream, kVideoTrack);
        if (NS_FAILED(rv)) {
          MM_LOG(("Starting video failed, rv=%d",rv));
        }
      }
      MM_LOG(("started all sources"));
      nsCOMPtr<GetUserMediaNotificationEvent> event =
        new GetUserMediaNotificationEvent(GetUserMediaNotificationEvent::STARTING);

      NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
      return;
    }

    // NOT_CONSUMED
    Invalidate();
    return;
  }

private:
  nsRefPtr<MediaEngineSource> mAudioSource;
  nsRefPtr<MediaEngineSource> mVideoSource;
  nsCOMPtr<nsDOMMediaStream> mStream;
  bool mValid;
};

typedef nsTArray<nsRefPtr<GetUserMediaCallbackMediaStreamListener> > StreamListeners;
typedef nsClassHashtable<nsUint64HashKey, StreamListeners> WindowTable;

class MediaDevice : public nsIMediaDevice
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEDIADEVICE

  MediaDevice(MediaEngineVideoSource* aSource) {
    mSource = aSource;
    mType.Assign(NS_LITERAL_STRING("video"));
    mSource->GetName(mName);
  };
  MediaDevice(MediaEngineAudioSource* aSource) {
    mSource = aSource;
    mType.Assign(NS_LITERAL_STRING("audio"));
    mSource->GetName(mName);
  };
  virtual ~MediaDevice() {};

  MediaEngineSource* GetSource();
private:
  nsString mName;
  nsString mType;
  nsRefPtr<MediaEngineSource> mSource;
};

class MediaManager MOZ_FINAL : public nsIObserver
{
public:
  static MediaManager* Get() {
    if (!sSingleton) {
      sSingleton = new MediaManager();

      nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
      obs->AddObserver(sSingleton, "xpcom-shutdown", false);
      obs->AddObserver(sSingleton, "getUserMedia:response:allow", false);
      obs->AddObserver(sSingleton, "getUserMedia:response:deny", false);
    }
    return sSingleton;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  MediaEngine* GetBackend();
  WindowTable* GetActiveWindows();

  nsresult GetUserMedia(bool aPrivileged, nsPIDOMWindow* aWindow,
    nsIMediaStreamOptions* aParams,
    nsIDOMGetUserMediaSuccessCallback* onSuccess,
    nsIDOMGetUserMediaErrorCallback* onError);
  nsresult GetUserMediaDevices(nsPIDOMWindow* aWindow,
    nsIGetUserMediaDevicesSuccessCallback* onSuccess,
    nsIDOMGetUserMediaErrorCallback* onError);
  void OnNavigation(uint64_t aWindowID);

private:
  // Make private because we want only one instance of this class
  MediaManager()
  : mBackend(nullptr)
  , mMediaThread(nullptr) {
    mActiveWindows.Init();
    mActiveCallbacks.Init();
  };
  MediaManager(MediaManager const&) {};

  ~MediaManager() {
    delete mBackend;
  };

  MediaEngine* mBackend;
  nsCOMPtr<nsIThread> mMediaThread;
  WindowTable mActiveWindows;
  nsRefPtrHashtable<nsStringHashKey, nsRunnable> mActiveCallbacks;

  static nsRefPtr<MediaManager> sSingleton;
};

} // namespace mozilla
