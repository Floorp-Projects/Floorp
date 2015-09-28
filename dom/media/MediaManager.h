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
#include "mozilla/media/MediaChild.h"
#include "mozilla/media/MediaParent.h"
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
struct MediaTrackConstraints;
struct MediaTrackConstraintSet;
} // namespace dom

extern PRLogModuleInfo* GetMediaManagerLog();
#define MM_LOG(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Debug, msg)

class MediaDevice : public nsIMediaDevice
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEDIADEVICE

  void SetId(const nsAString& aID);
  virtual uint32_t GetBestFitnessDistance(
      const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets);
protected:
  virtual ~MediaDevice() {}
  explicit MediaDevice(MediaEngineSource* aSource, bool aIsVideo);
  static uint32_t FitnessDistance(nsString aN,
    const dom::OwningStringOrStringSequenceOrConstrainDOMStringParameters& aConstraint);
private:
  static bool StringsContain(const dom::OwningStringOrStringSequence& aStrings,
                             nsString aN);
  static uint32_t FitnessDistance(nsString aN,
      const dom::ConstrainDOMStringParameters& aParams);
protected:
  nsString mName;
  nsString mID;
  dom::MediaSourceEnum mMediaSource;
  nsRefPtr<MediaEngineSource> mSource;
public:
  dom::MediaSourceEnum GetMediaSource() {
    return mMediaSource;
  }
  bool mIsVideo;
};

class VideoDevice : public MediaDevice
{
public:
  typedef MediaEngineVideoSource Source;

  explicit VideoDevice(Source* aSource);
  NS_IMETHOD GetType(nsAString& aType);
  Source* GetSource();
  nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                    const MediaEnginePrefs &aPrefs);
  nsresult Restart(const dom::MediaTrackConstraints &aConstraints,
                   const MediaEnginePrefs &aPrefs);
};

class AudioDevice : public MediaDevice
{
public:
  typedef MediaEngineAudioSource Source;

  explicit AudioDevice(Source* aSource);
  NS_IMETHOD GetType(nsAString& aType);
  Source* GetSource();
  nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                    const MediaEnginePrefs &aPrefs);
  nsresult Restart(const dom::MediaTrackConstraints &aConstraints,
                   const MediaEnginePrefs &aPrefs);
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
    unused << mMediaThread;
    // It's OK to release mStream on any thread; they have thread-safe
    // refcounts.
  }

  void Activate(already_AddRefed<SourceMediaStream> aStream,
                AudioDevice* aAudioDevice,
                VideoDevice* aVideoDevice)
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    mStream = aStream;
    mAudioDevice = aAudioDevice;
    mVideoDevice = aVideoDevice;

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

  void StopSharing();

  void StopTrack(TrackID aID, bool aIsAudio);

  typedef media::Pledge<bool, dom::MediaStreamError*> PledgeVoid;

  already_AddRefed<PledgeVoid>
  ApplyConstraintsToTrack(nsPIDOMWindow* aWindow,
                          TrackID aID, bool aIsAudio,
                          const dom::MediaTrackConstraints& aConstraints);

  // mVideo/AudioDevice are set by Activate(), so we assume they're capturing
  // if set and represent a real capture device.
  bool CapturingVideo()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    return mVideoDevice && !mStopped &&
           !mVideoDevice->GetSource()->IsAvailable() &&
           mVideoDevice->GetMediaSource() == dom::MediaSourceEnum::Camera &&
           (!mVideoDevice->GetSource()->IsFake() ||
            Preferences::GetBool("media.navigator.permission.fake"));
  }
  bool CapturingAudio()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    return mAudioDevice && !mStopped &&
           !mAudioDevice->GetSource()->IsAvailable() &&
           (!mAudioDevice->GetSource()->IsFake() ||
            Preferences::GetBool("media.navigator.permission.fake"));
  }
  bool CapturingScreen()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    return mVideoDevice && !mStopped &&
           !mVideoDevice->GetSource()->IsAvailable() &&
           mVideoDevice->GetMediaSource() == dom::MediaSourceEnum::Screen;
  }
  bool CapturingWindow()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    return mVideoDevice && !mStopped &&
           !mVideoDevice->GetSource()->IsAvailable() &&
           mVideoDevice->GetMediaSource() == dom::MediaSourceEnum::Window;
  }
  bool CapturingApplication()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    return mVideoDevice && !mStopped &&
           !mVideoDevice->GetSource()->IsAvailable() &&
           mVideoDevice->GetMediaSource() == dom::MediaSourceEnum::Application;
  }
  bool CapturingBrowser()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    return mVideoDevice && !mStopped &&
           mVideoDevice->GetSource()->IsAvailable() &&
           mVideoDevice->GetMediaSource() == dom::MediaSourceEnum::Browser;
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
    if (mAudioDevice) {
      mAudioDevice->GetSource()->NotifyPull(aGraph, mStream, kAudioTrack,
                                            aDesiredTime);
    }
    if (mVideoDevice) {
      mVideoDevice->GetSource()->NotifyPull(aGraph, mStream, kVideoTrack,
                                            aDesiredTime);
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
  nsRefPtr<AudioDevice> mAudioDevice; // threadsafe refcnt
  nsRefPtr<VideoDevice> mVideoDevice; // threadsafe refcnt
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
      STOPPED_TRACK,
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
  MEDIA_DIRECT_LISTENERS,
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

typedef nsTArray<nsRefPtr<GetUserMediaCallbackMediaStreamListener> > StreamListeners;
typedef nsClassHashtable<nsUint64HashKey, StreamListeners> WindowTable;

// we could add MediaManager if needed
typedef void (*WindowListenerCallback)(MediaManager *aThis,
                                       uint64_t aWindowID,
                                       StreamListeners *aListeners,
                                       void *aData);

class MediaManager final : public nsIMediaManagerService,
                           public nsIObserver
{
  friend GetUserMediaCallbackMediaStreamListener;
public:
  static already_AddRefed<MediaManager> GetInstance();

  // NOTE: never Dispatch(....,NS_DISPATCH_SYNC) to the MediaManager
  // thread from the MainThread, as we NS_DISPATCH_SYNC to MainThread
  // from MediaManager thread.
  static MediaManager* Get();
  static MediaManager* GetIfExists();
  static void PostTask(const tracked_objects::Location& from_here, Task* task);
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

  media::Parent<media::NonE10s>* GetNonE10sParent();
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
                               uint64_t aInnerWindowID = 0);

  nsresult EnumerateDevices(nsPIDOMWindow* aWindow,
                            nsIGetUserMediaDevicesSuccessCallback* aOnSuccess,
                            nsIDOMGetUserMediaErrorCallback* aOnFailure);

  nsresult EnumerateDevices(nsPIDOMWindow* aWindow, dom::Promise& aPromise);
  void OnNavigation(uint64_t aWindowID);
  bool IsActivelyCapturingOrHasAPermission(uint64_t aWindowId);

  MediaEnginePrefs mPrefs;

  typedef nsTArray<nsRefPtr<MediaDevice>> SourceSet;
  static bool IsPrivateBrowsing(nsPIDOMWindow *window);
private:
  typedef media::Pledge<SourceSet*, dom::MediaStreamError*> PledgeSourceSet;

  static bool IsPrivileged();
  static bool IsLoop(nsIURI* aDocURI);
  static nsresult GenerateUUID(nsAString& aResult);
  static nsresult AnonymizeId(nsAString& aId, const nsACString& aOriginKey);
public: // TODO: make private once we upgrade to GCC 4.8+ on linux.
  static void AnonymizeDevices(SourceSet& aDevices, const nsACString& aOriginKey);
  static already_AddRefed<nsIWritableVariant> ToJSArray(SourceSet& aDevices);
private:
  already_AddRefed<PledgeSourceSet>
  EnumerateRawDevices(uint64_t aWindowId,
                      dom::MediaSourceEnum aVideoType,
                      dom::MediaSourceEnum aAudioType,
                      bool aFake, bool aFakeTracks);
  already_AddRefed<PledgeSourceSet>
  EnumerateDevicesImpl(uint64_t aWindowId,
                       dom::MediaSourceEnum aVideoSrcType,
                       dom::MediaSourceEnum aAudioSrcType,
                       bool aFake = false, bool aFakeTracks = false);

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

  media::CoatCheck<PledgeSourceSet> mOutstandingPledges;
  media::CoatCheck<GetUserMediaCallbackMediaStreamListener::PledgeVoid> mOutstandingVoidPledges;
#if defined(MOZ_B2G_CAMERA) && defined(MOZ_WIDGET_GONK)
  nsRefPtr<nsDOMCameraManager> mCameraManager;
#endif
public:
  media::CoatCheck<media::Pledge<nsCString>> mGetOriginKeyPledges;
  ScopedDeletePtr<media::Parent<media::NonE10s>> mNonE10sParent;
};

} // namespace mozilla

#endif // MOZILLA_MEDIAMANAGER_H
