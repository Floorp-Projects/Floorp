/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngine.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "nsIMediaManager.h"

#include "nsHashKeys.h"
#include "nsGlobalWindow.h"
#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsObserverService.h"

#include "nsPIDOMWindow.h"
#include "nsIDOMNavigatorUserMedia.h"
#include "nsXULAppAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/StaticPtr.h"
#include "prlog.h"

#ifdef MOZ_WEBRTC
#include "mtransport/runnable_utils.h"
#endif

namespace mozilla {

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetMediaManagerLog();
#define MM_LOG(msg) PR_LOG(GetMediaManagerLog(), PR_LOG_DEBUG, msg)
#else
#define MM_LOG(msg)
#endif

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
        // Forward recording events to parent process.
        // The events are gathered in chrome process and used for recording indicator
        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          unused << mozilla::dom::ContentChild::GetSingleton()->SendRecordingDeviceEvents(NS_LITERAL_STRING("starting"));
        }
      } else {
        obs->NotifyObservers(nullptr,
            "recording-device-events",
            NS_LITERAL_STRING("shutdown").get());
        // Forward recording events to parent process.
        // The events are gathered in chrome process and used for recording indicator
        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          unused << mozilla::dom::ContentChild::GetSingleton()->SendRecordingDeviceEvents(NS_LITERAL_STRING("shutdown"));
        }
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
  // Create in an inactive state
  GetUserMediaCallbackMediaStreamListener(nsIThread *aThread,
    uint64_t aWindowID)
    : mMediaThread(aThread)
    , mWindowID(aWindowID)
    , mFinished(false)
    , mLock("mozilla::GUMCMSL")
    , mRemoved(false) {}

  ~GetUserMediaCallbackMediaStreamListener()
  {
    // It's OK to release mStream on any thread; they have thread-safe
    // refcounts.
  }

  void Activate(already_AddRefed<SourceMediaStream> aStream,
    MediaEngineSource* aAudioSource,
    MediaEngineSource* aVideoSource)
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    mStream = aStream; // also serves as IsActive();
    mAudioSource = aAudioSource;
    mVideoSource = aVideoSource;
    mLastEndTimeAudio = 0;
    mLastEndTimeVideo = 0;

    mStream->AddListener(this);
  }

  MediaStream *Stream()
  {
    return mStream;
  }
  SourceMediaStream *GetSourceStream()
  {
    NS_ASSERTION(mStream,"Getting stream from never-activated GUMCMSListener");
    if (!mStream) {
      return nullptr;
    }
    return mStream->AsSourceStream();
  }

  // implement in .cpp to avoid circular dependency with MediaOperationRunnable
  // Can be invoked from EITHER MainThread or MSG thread
  void Invalidate();

  void
  AudioConfig(bool aEchoOn, uint32_t aEcho,
              bool aAgcOn, uint32_t aAGC,
              bool aNoiseOn, uint32_t aNoise)
  {
    if (mAudioSource) {
#ifdef MOZ_WEBRTC
      // Right now these configs are only of use if webrtc is available
      RUN_ON_THREAD(mMediaThread,
                    WrapRunnable(nsRefPtr<MediaEngineSource>(mAudioSource), // threadsafe
                                 &MediaEngineSource::Config,
                                 aEchoOn, aEcho, aAgcOn, aAGC, aNoiseOn, aNoise),
                    NS_DISPATCH_NORMAL);
#endif
    }
  }

  void
  Remove()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    // allow calling even if inactive (!mStream) for easier cleanup
    // Caller holds strong reference to us, so no death grip required
    MutexAutoLock lock(mLock); // protect access to mRemoved
    if (mStream && !mRemoved) {
      MM_LOG(("Listener removed on purpose, mFinished = %d", (int) mFinished));
      mRemoved = true; // RemoveListener is async, avoid races
      // If it's destroyed, don't call - listener will be removed and we'll be notified!
      if (!mStream->IsDestroyed()) {
        mStream->RemoveListener(this);
      }
    }
  }

  // Proxy NotifyPull() to sources
  virtual void
  NotifyPull(MediaStreamGraph* aGraph, StreamTime aDesiredTime) MOZ_OVERRIDE
  {
    // Currently audio sources ignore NotifyPull, but they could
    // watch it especially for fake audio.
    if (mAudioSource) {
      mAudioSource->NotifyPull(aGraph, mStream, kAudioTrack, aDesiredTime, mLastEndTimeAudio);
    }
    if (mVideoSource) {
      mVideoSource->NotifyPull(aGraph, mStream, kVideoTrack, aDesiredTime, mLastEndTimeVideo);
    }
  }

  virtual void
  NotifyFinished(MediaStreamGraph* aGraph) MOZ_OVERRIDE;

  virtual void
  NotifyRemoved(MediaStreamGraph* aGraph) MOZ_OVERRIDE;

private:
  // Set at construction
  nsCOMPtr<nsIThread> mMediaThread;
  uint64_t mWindowID;

  // Set at Activate on MainThread

  // Accessed from MediaStreamGraph thread, MediaManager thread, and MainThread
  // No locking needed as they're only addrefed except on the MediaManager thread
  nsRefPtr<MediaEngineSource> mAudioSource; // threadsafe refcnt
  nsRefPtr<MediaEngineSource> mVideoSource; // threadsafe refcnt
  nsRefPtr<SourceMediaStream> mStream; // threadsafe refcnt
  TrackTicks mLastEndTimeAudio;
  TrackTicks mLastEndTimeVideo;
  bool mFinished;

  // Accessed from MainThread and MSG thread
  Mutex mLock; // protects mRemoved access from MainThread
  bool mRemoved;
};

typedef enum {
  MEDIA_START,
  MEDIA_STOP
} MediaOperation;

// Generic class for running long media operations like Start off the main
// thread, and then (because nsDOMMediaStreams aren't threadsafe),
// ProxyReleases mStream since it's cycle collected.
class MediaOperationRunnable : public nsRunnable
{
public:
  // so we can send Stop without AddRef()ing from the MSG thread
  MediaOperationRunnable(MediaOperation aType,
    GetUserMediaCallbackMediaStreamListener* aListener,
    MediaEngineSource* aAudioSource,
    MediaEngineSource* aVideoSource,
    bool aNeedsFinish)
    : mType(aType)
    , mAudioSource(aAudioSource)
    , mVideoSource(aVideoSource)
    , mListener(aListener)
    , mFinish(aNeedsFinish)
    {}

  ~MediaOperationRunnable()
  {
    // MediaStreams can be released on any thread.
  }

  NS_IMETHOD
  Run()
  {
    SourceMediaStream *source = mListener->GetSourceStream();
    // No locking between these is required as all the callbacks for the
    // same MediaStream will occur on the same thread.
    if (!source) // means the stream was never Activated()
      return NS_OK;

    switch (mType) {
      case MEDIA_START:
        {
          NS_ASSERTION(!NS_IsMainThread(), "Never call on main thread");
          nsresult rv;

          source->SetPullEnabled(true);

          if (mAudioSource) {
            rv = mAudioSource->Start(source, kAudioTrack);
            if (NS_FAILED(rv)) {
              MM_LOG(("Starting audio failed, rv=%d",rv));
            }
          }
          if (mVideoSource) {
            rv = mVideoSource->Start(source, kVideoTrack);
            if (NS_FAILED(rv)) {
              MM_LOG(("Starting video failed, rv=%d",rv));
            }
          }

          MM_LOG(("started all sources"));
          nsRefPtr<GetUserMediaNotificationEvent> event =
            new GetUserMediaNotificationEvent(GetUserMediaNotificationEvent::STARTING);


          NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
        }
        break;

      case MEDIA_STOP:
        {
          NS_ASSERTION(!NS_IsMainThread(), "Never call on main thread");
          if (mAudioSource) {
            mAudioSource->Stop(source, kAudioTrack);
            mAudioSource->Deallocate();
          }
          if (mVideoSource) {
            mVideoSource->Stop(source, kVideoTrack);
            mVideoSource->Deallocate();
          }
          // Do this after stopping all tracks with EndTrack()
          if (mFinish) {
            source->Finish();
          }
          // the TrackUnion destination of the port will autofinish

          nsRefPtr<GetUserMediaNotificationEvent> event =
            new GetUserMediaNotificationEvent(GetUserMediaNotificationEvent::STOPPING);

          NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
        }
        break;

      default:
        MOZ_ASSERT(false,"invalid MediaManager operation");
        break;
    }
    return NS_OK;
  }

private:
  MediaOperation mType;
  nsRefPtr<MediaEngineSource> mAudioSource; // threadsafe
  nsRefPtr<MediaEngineSource> mVideoSource; // threadsafe
  nsRefPtr<GetUserMediaCallbackMediaStreamListener> mListener; // threadsafe
  bool mFinish;
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
    mSource->GetUUID(mID);
  }
  MediaDevice(MediaEngineAudioSource* aSource) {
    mSource = aSource;
    mType.Assign(NS_LITERAL_STRING("audio"));
    mSource->GetName(mName);
    mSource->GetUUID(mID);
  }
  virtual ~MediaDevice() {}

  MediaEngineSource* GetSource();
private:
  nsString mName;
  nsString mType;
  nsString mID;
  nsRefPtr<MediaEngineSource> mSource;
};

class MediaManager MOZ_FINAL : public nsIMediaManagerService,
                               public nsIObserver
{
public:
  static already_AddRefed<MediaManager> GetInstance();

  static MediaManager* Get() {
    if (!sSingleton) {
      sSingleton = new MediaManager();

      NS_NewThread(getter_AddRefs(sSingleton->mMediaThread));
      MM_LOG(("New Media thread for gum"));

      NS_ASSERTION(NS_IsMainThread(), "Only create MediaManager on main thread");
      nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
      if (obs) {
        obs->AddObserver(sSingleton, "xpcom-shutdown", false);
        obs->AddObserver(sSingleton, "getUserMedia:response:allow", false);
        obs->AddObserver(sSingleton, "getUserMedia:response:deny", false);
        obs->AddObserver(sSingleton, "getUserMedia:revoke", false);
      }
      // else MediaManager won't work properly and will leak (see bug 837874)
    }
    return sSingleton;
  }
  static nsIThread* GetThread() {
    return Get()->mMediaThread;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEDIAMANAGERSERVICE

  MediaEngine* GetBackend();
  StreamListeners *GetWindowListeners(uint64_t aWindowId) {
    NS_ASSERTION(NS_IsMainThread(), "Only access windowlist on main thread");

    return mActiveWindows.Get(aWindowId);
  }
  void RemoveWindowID(uint64_t aWindowId) {
    mActiveWindows.Remove(aWindowId);
  }
  bool IsWindowStillActive(uint64_t aWindowId) {
    return !!GetWindowListeners(aWindowId);
  }
  // Note: also calls aListener->Remove(), even if inactive
  void RemoveFromWindowList(uint64_t aWindowID,
    GetUserMediaCallbackMediaStreamListener *aListener);

  nsresult GetUserMedia(bool aPrivileged, nsPIDOMWindow* aWindow,
    nsIMediaStreamOptions* aParams,
    nsIDOMGetUserMediaSuccessCallback* onSuccess,
    nsIDOMGetUserMediaErrorCallback* onError);
  nsresult GetUserMediaDevices(nsPIDOMWindow* aWindow,
    nsIGetUserMediaDevicesSuccessCallback* onSuccess,
    nsIDOMGetUserMediaErrorCallback* onError);
  void OnNavigation(uint64_t aWindowID);

private:
  WindowTable *GetActiveWindows() {
    NS_ASSERTION(NS_IsMainThread(), "Only access windowlist on main thread");
    return &mActiveWindows;
  }

  // Make private because we want only one instance of this class
  MediaManager()
  : mMediaThread(nullptr)
  , mMutex("mozilla::MediaManager")
  , mBackend(nullptr) {
    mActiveWindows.Init();
    mActiveCallbacks.Init();
  }

  ~MediaManager() {
    delete mBackend;
  }

  // ONLY access from MainThread so we don't need to lock
  WindowTable mActiveWindows;
  nsRefPtrHashtable<nsStringHashKey, nsRunnable> mActiveCallbacks;
  // Always exists
  nsCOMPtr<nsIThread> mMediaThread;

  Mutex mMutex;
  // protected with mMutex:
  MediaEngine* mBackend;

  static StaticRefPtr<MediaManager> sSingleton;
};

} // namespace mozilla
