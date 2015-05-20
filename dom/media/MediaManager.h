/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIAMANAGER_H
#define MOZILLA_MEDIAMANAGER_H

#include "MediaEngine.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "nsIMediaManager.h"

#include "nsHashKeys.h"
#include "nsGlobalWindow.h"
#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsIObserver.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "nsPIDOMWindow.h"
#include "nsIDOMNavigatorUserMedia.h"
#include "nsXULAppAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/MediaStreamError.h"
#include "mozilla/Logging.h"
#include "DOMMediaStream.h"

#ifdef MOZ_WEBRTC
#include "mtransport/runnable_utils.h"
#endif

// Note, these suck in Windows headers, unfortunately.
#include "base/thread.h"
#include "base/task.h"

#ifdef MOZ_WIDGET_GONK
#include "DOMCameraManager.h"
#endif

namespace mozilla {
namespace dom {
struct MediaStreamConstraints;
struct MediaTrackConstraintSet;
}

extern PRLogModuleInfo* GetMediaManagerLog();
#define MM_LOG(msg) PR_LOG(GetMediaManagerLog(), PR_LOG_DEBUG, msg)

/**
 * This class is an implementation of MediaStreamListener. This is used
 * to Start() and Stop() the underlying MediaEngineSource when MediaStreams
 * are assigned and deassigned in content.
 */
class GetUserMediaCallbackMediaStreamListener : public MediaStreamListener
{
public:
  // Create in an inactive state
  GetUserMediaCallbackMediaStreamListener(base::Thread *aThread,
    uint64_t aWindowID)
    : mMediaThread(aThread)
    , mWindowID(aWindowID)
    , mStopped(false)
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
    mStream = aStream;
    mAudioSource = aAudioSource;
    mVideoSource = aVideoSource;

    mStream->AddListener(this);
  }

  MediaStream *Stream() // Can be used to test if Activate was called
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

  void StopScreenWindowSharing();

  void StopTrack(TrackID aID, bool aIsAudio);

  // mVideo/AudioSource are set by Activate(), so we assume they're capturing
  // if set and represent a real capture device.
  bool CapturingVideo()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    return mVideoSource && !mStopped &&
           mVideoSource->GetMediaSource() == dom::MediaSourceEnum::Camera &&
           (!mVideoSource->IsFake() ||
            Preferences::GetBool("media.navigator.permission.fake"));
  }
  bool CapturingAudio()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    return mAudioSource && !mStopped &&
           (!mAudioSource->IsFake() ||
            Preferences::GetBool("media.navigator.permission.fake"));
  }
  bool CapturingScreen()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    return mVideoSource && !mStopped && !mVideoSource->IsAvailable() &&
           mVideoSource->GetMediaSource() == dom::MediaSourceEnum::Screen;
  }
  bool CapturingWindow()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    return mVideoSource && !mStopped && !mVideoSource->IsAvailable() &&
           mVideoSource->GetMediaSource() == dom::MediaSourceEnum::Window;
  }
  bool CapturingApplication()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    return mVideoSource && !mStopped && !mVideoSource->IsAvailable() &&
           mVideoSource->GetMediaSource() == dom::MediaSourceEnum::Application;
  }
  bool CapturingBrowser()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    return mVideoSource && !mStopped && mVideoSource->IsAvailable() &&
           mVideoSource->GetMediaSource() == dom::MediaSourceEnum::Browser;
  }

  void SetStopped()
  {
    mStopped = true;
  }

  // implement in .cpp to avoid circular dependency with MediaOperationTask
  // Can be invoked from EITHER MainThread or MSG thread
  void Invalidate();

  void
  AudioConfig(bool aEchoOn, uint32_t aEcho,
              bool aAgcOn, uint32_t aAGC,
              bool aNoiseOn, uint32_t aNoise,
              int32_t aPlayoutDelay);

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
  NotifyPull(MediaStreamGraph* aGraph, StreamTime aDesiredTime) override
  {
    // Currently audio sources ignore NotifyPull, but they could
    // watch it especially for fake audio.
    if (mAudioSource) {
      mAudioSource->NotifyPull(aGraph, mStream, kAudioTrack, aDesiredTime);
    }
    if (mVideoSource) {
      mVideoSource->NotifyPull(aGraph, mStream, kVideoTrack, aDesiredTime);
    }
  }

  virtual void
  NotifyEvent(MediaStreamGraph* aGraph,
              MediaStreamListener::MediaStreamGraphEvent aEvent) override
  {
    switch (aEvent) {
      case EVENT_FINISHED:
        NotifyFinished(aGraph);
        break;
      case EVENT_REMOVED:
        NotifyRemoved(aGraph);
        break;
      case EVENT_HAS_DIRECT_LISTENERS:
        NotifyDirectListeners(aGraph, true);
        break;
      case EVENT_HAS_NO_DIRECT_LISTENERS:
        NotifyDirectListeners(aGraph, false);
        break;
      default:
        break;
    }
  }

  virtual void
  NotifyFinished(MediaStreamGraph* aGraph);

  virtual void
  NotifyRemoved(MediaStreamGraph* aGraph);

  virtual void
  NotifyDirectListeners(MediaStreamGraph* aGraph, bool aHasListeners);

private:
  // Set at construction
  base::Thread* mMediaThread;
  uint64_t mWindowID;

  bool mStopped; // MainThread only

  // Set at Activate on MainThread

  // Accessed from MediaStreamGraph thread, MediaManager thread, and MainThread
  // No locking needed as they're only addrefed except on the MediaManager thread
  nsRefPtr<MediaEngineSource> mAudioSource; // threadsafe refcnt
  nsRefPtr<MediaEngineSource> mVideoSource; // threadsafe refcnt
  nsRefPtr<SourceMediaStream> mStream; // threadsafe refcnt
  bool mFinished;

  // Accessed from MainThread and MSG thread
  Mutex mLock; // protects mRemoved access from MainThread
  bool mRemoved;
};

class GetUserMediaNotificationEvent: public nsRunnable
{
  public:
    enum GetUserMediaStatus {
      STARTING,
      STOPPING,
      STOPPED_TRACK
    };
    GetUserMediaNotificationEvent(GetUserMediaCallbackMediaStreamListener* aListener,
                                  GetUserMediaStatus aStatus,
                                  bool aIsAudio, bool aIsVideo, uint64_t aWindowID)
    : mListener(aListener) , mStatus(aStatus) , mIsAudio(aIsAudio)
    , mIsVideo(aIsVideo), mWindowID(aWindowID) {}

    GetUserMediaNotificationEvent(GetUserMediaStatus aStatus,
                                  already_AddRefed<DOMMediaStream> aStream,
                                  DOMMediaStream::OnTracksAvailableCallback* aOnTracksAvailableCallback,
                                  bool aIsAudio, bool aIsVideo, uint64_t aWindowID,
                                  already_AddRefed<nsIDOMGetUserMediaErrorCallback> aError)
    : mStream(aStream), mOnTracksAvailableCallback(aOnTracksAvailableCallback),
      mStatus(aStatus), mIsAudio(aIsAudio), mIsVideo(aIsVideo), mWindowID(aWindowID),
      mOnFailure(aError) {}
    virtual ~GetUserMediaNotificationEvent()
    {

    }

    NS_IMETHOD Run() override;

  protected:
    nsRefPtr<GetUserMediaCallbackMediaStreamListener> mListener; // threadsafe
    nsRefPtr<DOMMediaStream> mStream;
    nsAutoPtr<DOMMediaStream::OnTracksAvailableCallback> mOnTracksAvailableCallback;
    GetUserMediaStatus mStatus;
    bool mIsAudio;
    bool mIsVideo;
    uint64_t mWindowID;
    nsRefPtr<nsIDOMGetUserMediaErrorCallback> mOnFailure;
};

typedef enum {
  MEDIA_START,
  MEDIA_STOP,
  MEDIA_STOP_TRACK,
  MEDIA_DIRECT_LISTENERS
} MediaOperation;

class MediaManager;
class GetUserMediaTask;

class ReleaseMediaOperationResource : public nsRunnable
{
public:
  ReleaseMediaOperationResource(already_AddRefed<DOMMediaStream> aStream,
    DOMMediaStream::OnTracksAvailableCallback* aOnTracksAvailableCallback):
    mStream(aStream),
    mOnTracksAvailableCallback(aOnTracksAvailableCallback) {}
  NS_IMETHOD Run() override {return NS_OK;}
private:
  nsRefPtr<DOMMediaStream> mStream;
  nsAutoPtr<DOMMediaStream::OnTracksAvailableCallback> mOnTracksAvailableCallback;
};

// Generic class for running long media operations like Start off the main
// thread, and then (because nsDOMMediaStreams aren't threadsafe),
// ProxyReleases mStream since it's cycle collected.
class MediaOperationTask : public Task
{
public:
  // so we can send Stop without AddRef()ing from the MSG thread
  MediaOperationTask(MediaOperation aType,
    GetUserMediaCallbackMediaStreamListener* aListener,
    DOMMediaStream* aStream,
    DOMMediaStream::OnTracksAvailableCallback* aOnTracksAvailableCallback,
    MediaEngineSource* aAudioSource,
    MediaEngineSource* aVideoSource,
    bool aBool,
    uint64_t aWindowID,
    already_AddRefed<nsIDOMGetUserMediaErrorCallback> aError)
    : mType(aType)
    , mStream(aStream)
    , mOnTracksAvailableCallback(aOnTracksAvailableCallback)
    , mAudioSource(aAudioSource)
    , mVideoSource(aVideoSource)
    , mListener(aListener)
    , mBool(aBool)
    , mWindowID(aWindowID)
    , mOnFailure(aError)
  {}

  ~MediaOperationTask()
  {
    // MediaStreams can be released on any thread.
  }

  void
  ReturnCallbackError(nsresult rv, const char* errorLog);

  void
  Run()
  {
    SourceMediaStream *source = mListener->GetSourceStream();
    // No locking between these is required as all the callbacks for the
    // same MediaStream will occur on the same thread.
    if (!source) // means the stream was never Activated()
      return;

    switch (mType) {
      case MEDIA_START:
        {
          NS_ASSERTION(!NS_IsMainThread(), "Never call on main thread");
          nsresult rv;

          if (mAudioSource) {
            rv = mAudioSource->Start(source, kAudioTrack);
            if (NS_FAILED(rv)) {
              ReturnCallbackError(rv, "Starting audio failed");
              return;
            }
          }
          if (mVideoSource) {
            rv = mVideoSource->Start(source, kVideoTrack);
            if (NS_FAILED(rv)) {
              ReturnCallbackError(rv, "Starting video failed");
              return;
            }
          }
          // Start() queued the tracks to be added synchronously to avoid races
          source->FinishAddTracks();

          source->SetPullEnabled(true);
          source->AdvanceKnownTracksTime(STREAM_TIME_MAX);

          MM_LOG(("started all sources"));
          // Forward mOnTracksAvailableCallback to GetUserMediaNotificationEvent,
          // because mOnTracksAvailableCallback needs to be added to mStream
          // on the main thread.
          nsIRunnable *event =
            new GetUserMediaNotificationEvent(GetUserMediaNotificationEvent::STARTING,
                                              mStream.forget(),
                                              mOnTracksAvailableCallback.forget(),
                                              mAudioSource != nullptr,
                                              mVideoSource != nullptr,
                                              mWindowID, mOnFailure.forget());
          // event must always be released on mainthread due to the JS callbacks
          // in the TracksAvailableCallback
          NS_DispatchToMainThread(event);
        }
        break;

      case MEDIA_STOP:
      case MEDIA_STOP_TRACK:
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
          if (mBool) {
            source->Finish();
          }

          nsIRunnable *event =
            new GetUserMediaNotificationEvent(mListener,
                                              mType == MEDIA_STOP ?
                                              GetUserMediaNotificationEvent::STOPPING :
                                              GetUserMediaNotificationEvent::STOPPED_TRACK,
                                              mAudioSource != nullptr,
                                              mVideoSource != nullptr,
                                              mWindowID);
          // event must always be released on mainthread due to the JS callbacks
          // in the TracksAvailableCallback
          NS_DispatchToMainThread(event);
        }
        break;

      case MEDIA_DIRECT_LISTENERS:
        {
          NS_ASSERTION(!NS_IsMainThread(), "Never call on main thread");
          if (mVideoSource) {
            mVideoSource->SetDirectListeners(mBool);
          }
        }
        break;

      default:
        MOZ_ASSERT(false,"invalid MediaManager operation");
        break;
    }
  }

private:
  MediaOperation mType;
  nsRefPtr<DOMMediaStream> mStream;
  nsAutoPtr<DOMMediaStream::OnTracksAvailableCallback> mOnTracksAvailableCallback;
  nsRefPtr<MediaEngineSource> mAudioSource; // threadsafe
  nsRefPtr<MediaEngineSource> mVideoSource; // threadsafe
  nsRefPtr<GetUserMediaCallbackMediaStreamListener> mListener; // threadsafe
  bool mBool;
  uint64_t mWindowID;
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> mOnFailure;
};

typedef nsTArray<nsRefPtr<GetUserMediaCallbackMediaStreamListener> > StreamListeners;
typedef nsClassHashtable<nsUint64HashKey, StreamListeners> WindowTable;

class MediaDevice : public nsIMediaDevice
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEDIADEVICE

  void SetId(const nsAString& aID);
protected:
  virtual ~MediaDevice() {}
  explicit MediaDevice(MediaEngineSource* aSource);
  nsString mName;
  nsString mID;
  bool mHasFacingMode;
  dom::VideoFacingModeEnum mFacingMode;
  dom::MediaSourceEnum mMediaSource;
  nsRefPtr<MediaEngineSource> mSource;
};

class VideoDevice : public MediaDevice
{
public:
  typedef MediaEngineVideoSource Source;

  explicit VideoDevice(Source* aSource);
  NS_IMETHOD GetType(nsAString& aType);
  Source* GetSource();
  uint32_t GetBestFitnessDistance(
    const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets);
};

class AudioDevice : public MediaDevice
{
public:
  typedef MediaEngineAudioSource Source;

  explicit AudioDevice(Source* aSource);
  NS_IMETHOD GetType(nsAString& aType);
  Source* GetSource();
  uint32_t GetBestFitnessDistance(
    const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets);
};

// we could add MediaManager if needed
typedef void (*WindowListenerCallback)(MediaManager *aThis,
                                       uint64_t aWindowID,
                                       StreamListeners *aListeners,
                                       void *aData);

class MediaManager final : public nsIMediaManagerService,
                           public nsIObserver
{
public:
  static already_AddRefed<MediaManager> GetInstance();

  // NOTE: never Dispatch(....,NS_DISPATCH_SYNC) to the MediaManager
  // thread from the MainThread, as we NS_DISPATCH_SYNC to MainThread
  // from MediaManager thread.
  static MediaManager* Get();
  static MediaManager* GetIfExists();
  static MessageLoop* GetMessageLoop();
#ifdef DEBUG
  static bool IsInMediaThread();
#endif

  static bool Exists()
  {
    return !!sSingleton;
  }

  static nsresult NotifyRecordingStatusChange(nsPIDOMWindow* aWindow,
                                              const nsString& aMsg,
                                              const bool& aIsAudio,
                                              const bool& aIsVideo);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEDIAMANAGERSERVICE

  MediaEngine* GetBackend(uint64_t aWindowId = 0);
  StreamListeners *GetWindowListeners(uint64_t aWindowId) {
    NS_ASSERTION(NS_IsMainThread(), "Only access windowlist on main thread");

    return mActiveWindows.Get(aWindowId);
  }
  void RemoveWindowID(uint64_t aWindowId);
  bool IsWindowStillActive(uint64_t aWindowId) {
    return !!GetWindowListeners(aWindowId);
  }
  // Note: also calls aListener->Remove(), even if inactive
  void RemoveFromWindowList(uint64_t aWindowID,
    GetUserMediaCallbackMediaStreamListener *aListener);

  nsresult GetUserMedia(
    nsPIDOMWindow* aWindow,
    const dom::MediaStreamConstraints& aConstraints,
    nsIDOMGetUserMediaSuccessCallback* onSuccess,
    nsIDOMGetUserMediaErrorCallback* onError);

  nsresult GetUserMediaDevices(nsPIDOMWindow* aWindow,
    const dom::MediaStreamConstraints& aConstraints,
    nsIGetUserMediaDevicesSuccessCallback* onSuccess,
    nsIDOMGetUserMediaErrorCallback* onError,
    uint64_t aInnerWindowID = 0,
    bool aPrivileged = true);

  nsresult EnumerateDevices(nsPIDOMWindow* aWindow,
                            nsIGetUserMediaDevicesSuccessCallback* aOnSuccess,
                            nsIDOMGetUserMediaErrorCallback* aOnFailure);

  nsresult EnumerateDevices(nsPIDOMWindow* aWindow, dom::Promise& aPromise);
  void OnNavigation(uint64_t aWindowID);
  bool IsWindowActivelyCapturing(uint64_t aWindowId);

  MediaEnginePrefs mPrefs;

private:
  StreamListeners* AddWindowID(uint64_t aWindowId);
  WindowTable *GetActiveWindows() {
    NS_ASSERTION(NS_IsMainThread(), "Only access windowlist on main thread");
    return &mActiveWindows;
  }

  void GetPref(nsIPrefBranch *aBranch, const char *aPref,
               const char *aData, int32_t *aVal);
  void GetPrefBool(nsIPrefBranch *aBranch, const char *aPref,
                   const char *aData, bool *aVal);
  void GetPrefs(nsIPrefBranch *aBranch, const char *aData);

  // Make private because we want only one instance of this class
  MediaManager();

  ~MediaManager() {}

  void StopScreensharing(uint64_t aWindowID);
  void IterateWindowListeners(nsPIDOMWindow *aWindow,
                              WindowListenerCallback aCallback,
                              void *aData);

  void StopMediaStreams();

  // ONLY access from MainThread so we don't need to lock
  WindowTable mActiveWindows;
  nsClassHashtable<nsStringHashKey, GetUserMediaTask> mActiveCallbacks;
  nsClassHashtable<nsUint64HashKey, nsTArray<nsString>> mCallIds;

  // Always exists
  nsAutoPtr<base::Thread> mMediaThread;

  Mutex mMutex;
  // protected with mMutex:
  RefPtr<MediaEngine> mBackend;

  static StaticRefPtr<MediaManager> sSingleton;

#if defined(MOZ_B2G_CAMERA) && defined(MOZ_WIDGET_GONK)
  nsRefPtr<nsDOMCameraManager> mCameraManager;
#endif
};

} // namespace mozilla

#endif // MOZILLA_MEDIAMANAGER_H
