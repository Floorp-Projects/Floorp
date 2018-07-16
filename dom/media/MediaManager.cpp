/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaManager.h"

#include "AllocationHandle.h"
#include "MediaStreamGraph.h"
#include "MediaTimer.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "mozilla/dom/MediaDeviceInfo.h"
#include "MediaStreamListener.h"
#include "nsArray.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsHashPropertyBag.h"
#include "nsIEventTarget.h"
#include "nsIUUIDGenerator.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPermissionManager.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsISupportsPrimitives.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIIDNService.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsICryptoHash.h"
#include "nsICryptoHMAC.h"
#include "nsIKeyModule.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIInputStream.h"
#include "nsILineInputStream.h"
#include "nsIWeakReferenceUtils.h"
#include "nsPIDOMWindow.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/MozPromise.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Types.h"
#include "mozilla/PeerIdentity.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/GetUserMediaRequestBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/MediaDevices.h"
#include "mozilla/Base64.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/media/MediaChild.h"
#include "mozilla/media/MediaTaskUtils.h"
#include "MediaTrackConstraints.h"
#include "VideoUtils.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "Latency.h"
#include "nsProxyRelease.h"
#include "nsVariant.h"

// For snprintf
#include "mozilla/Sprintf.h"

#include "nsJSUtils.h"
#include "nsGlobalWindow.h"
#include "nsIUUIDGenerator.h"
#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"

/* Using WebRTC backend on Desktops (Mac, Windows, Linux), otherwise default */
#include "MediaEngineDefault.h"
#if defined(MOZ_WEBRTC)
#include "MediaEngineWebRTC.h"
#include "browser_logging/WebRtcLog.h"
#endif

#if defined (XP_WIN)
#include "mozilla/WindowsVersion.h"
#include <objbase.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <tchar.h>
#endif

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount() and conflicts with MediaStream::GetCurrentTime.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

// XXX Workaround for bug 986974 to maintain the existing broken semantics
template<>
struct nsIMediaDevice::COMTypeInfo<mozilla::MediaDevice, void> {
  static const nsIID kIID;
};
const nsIID nsIMediaDevice::COMTypeInfo<mozilla::MediaDevice, void>::kIID = NS_IMEDIADEVICE_IID;

namespace {
already_AddRefed<nsIAsyncShutdownClient> GetShutdownPhase() {
  nsCOMPtr<nsIAsyncShutdownService> svc = mozilla::services::GetAsyncShutdown();
  MOZ_RELEASE_ASSERT(svc);

  nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase;
  nsresult rv = svc->GetProfileBeforeChange(getter_AddRefs(shutdownPhase));
  if (!shutdownPhase) {
    // We are probably in a content process. We need to do cleanup at
    // XPCOM shutdown in leakchecking builds.
    rv = svc->GetXpcomWillShutdown(getter_AddRefs(shutdownPhase));
  }
  MOZ_RELEASE_ASSERT(shutdownPhase);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  return shutdownPhase.forget();
}
}

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

LogModule*
GetMediaManagerLog()
{
  static LazyLogModule sLog("MediaManager");
  return sLog;
}
#define LOG(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Debug, msg)

using dom::BasicTrackSource;
using dom::ConstrainDOMStringParameters;
using dom::File;
using dom::GetUserMediaRequest;
using dom::MediaSourceEnum;
using dom::MediaStreamConstraints;
using dom::MediaStreamError;
using dom::MediaStreamTrack;
using dom::MediaStreamTrackSource;
using dom::MediaTrackConstraints;
using dom::MediaTrackConstraintSet;
using dom::OwningBooleanOrMediaTrackConstraints;
using dom::OwningStringOrStringSequence;
using dom::OwningStringOrStringSequenceOrConstrainDOMStringParameters;
using dom::Promise;
using dom::Sequence;
using media::NewRunnableFrom;
using media::NewTaskFrom;
using media::Pledge;
using media::Refcountable;

static Atomic<bool> sHasShutdown;

struct DeviceState {
  DeviceState(const RefPtr<MediaDevice>& aDevice, bool aOffWhileDisabled)
    : mOffWhileDisabled(aOffWhileDisabled)
    , mDevice(aDevice)
  {
    MOZ_ASSERT(mDevice);
  }

  // true if we have stopped mDevice, this is a terminal state.
  // MainThread only.
  bool mStopped = false;

  // true if mDevice is currently enabled, i.e., turned on and capturing.
  // MainThread only.
  bool mDeviceEnabled = false;

  // true if the application has currently enabled mDevice.
  // MainThread only.
  bool mTrackEnabled = false;

  // Time when the application last enabled mDevice.
  // MainThread only.
  TimeStamp mTrackEnabledTime;

  // true if an operation to Start() or Stop() mDevice has been dispatched to
  // the media thread and is not finished yet.
  // MainThread only.
  bool mOperationInProgress = false;

  // true if we are allowed to turn off the underlying source while all tracks
  // are disabled.
  // MainThread only.
  bool mOffWhileDisabled = false;

  // Timer triggered by a MediaStreamTrackSource signaling that all tracks got
  // disabled. When the timer fires we initiate Stop()ing mDevice.
  // If set we allow dynamically stopping and starting mDevice.
  // Any thread.
  const RefPtr<MediaTimer> mDisableTimer = new MediaTimer();

  // The underlying device we keep state for. Always non-null.
  // Threadsafe access, but see method declarations for individual constraints.
  const RefPtr<MediaDevice> mDevice;
};

/**
 * This mimics the capture state from nsIMediaManagerService.
 */
enum class CaptureState : uint16_t {
  Off = nsIMediaManagerService::STATE_NOCAPTURE,
  Enabled = nsIMediaManagerService::STATE_CAPTURE_ENABLED,
  Disabled = nsIMediaManagerService::STATE_CAPTURE_DISABLED,
};

static CaptureState
CombineCaptureState(CaptureState aFirst, CaptureState aSecond)
{
  if (aFirst == CaptureState::Enabled || aSecond == CaptureState::Enabled) {
    return CaptureState::Enabled;
  }
  if (aFirst == CaptureState::Disabled || aSecond == CaptureState::Disabled) {
    return CaptureState::Disabled;
  }
  MOZ_ASSERT(aFirst == CaptureState::Off);
  MOZ_ASSERT(aSecond == CaptureState::Off);
  return CaptureState::Off;
}

static uint16_t
FromCaptureState(CaptureState aState)
{
  MOZ_ASSERT(aState == CaptureState::Off ||
             aState == CaptureState::Enabled ||
             aState == CaptureState::Disabled);
  return static_cast<uint16_t>(aState);
}

/**
 * SourceListener has threadsafe refcounting for use across the main, media and
 * MSG threads. But it has a non-threadsafe SupportsWeakPtr for WeakPtr usage
 * only from main thread, to ensure that garbage- and cycle-collected objects
 * don't hold a reference to it during late shutdown.
 *
 * There's also a hard reference to the SourceListener through its
 * SourceStreamListener and the MediaStreamGraph. MediaStreamGraph
 * clears this on XPCOM_WILL_SHUTDOWN, before MediaManager enters shutdown.
 */
class SourceListener : public SupportsWeakPtr<SourceListener> {
public:
  typedef MozPromise<bool /* aIgnored */, Maybe<nsString>, true> ApplyConstraintsPromise;
  typedef MozPromise<bool /* aIgnored */, RefPtr<MediaMgrError>, true> InitPromise;

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(SourceListener)
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(SourceListener)

  SourceListener();

  /**
   * Registers this source listener as belonging to the given window listener.
   */
  void Register(GetUserMediaWindowListener* aListener);

  /**
   * Marks this listener as active and adds itself as a listener to aStream.
   */
  void Activate(SourceMediaStream* aStream,
                MediaDevice* aAudioDevice,
                MediaDevice* aVideoDevice);

  /**
   * Posts a task to initialize and start all associated devices.
   */
  RefPtr<InitPromise> InitializeAsync();

  /**
   * Stops all live tracks, finishes the associated MediaStream and cleans up.
   */
  void Stop();

  /**
   * Removes this SourceListener from its associated MediaStream and marks it
   * removed. Also removes the weak reference to the associated window listener.
   */
  void Remove();

  /**
   * Posts a task to stop the device associated with aTrackID and notifies the
   * associated window listener that a track was stopped.
   * Should this track be the last live one to be stopped, we'll also clean up.
   */
  void StopTrack(TrackID aTrackID);

  /**
   * Gets the main thread MediaTrackSettings from the MediaEngineSource
   * associated with aTrackID.
   */
  void GetSettingsFor(TrackID aTrackID, dom::MediaTrackSettings& aOutSettings) const;

  /**
   * Posts a task to set the enabled state of the device associated with
   * aTrackID to aEnabled and notifies the associated window listener that a
   * track's state has changed.
   *
   * Turning the hardware off while the device is disabled is supported for:
   * - Camera (enabled by default, controlled by pref
   *   "media.getusermedia.camera.off_while_disabled.enabled")
   * - Microphone (disabled by default, controlled by pref
   *   "media.getusermedia.microphone.off_while_disabled.enabled")
   * Screen-, app-, or windowsharing is not supported at this time.
   *
   * The behavior is also different between disabling and enabling a device.
   * While enabling is immediate, disabling only happens after a delay.
   * This is now defaulting to 3 seconds but can be overriden by prefs:
   * - "media.getusermedia.camera.off_while_disabled.delay_ms" and
   * - "media.getusermedia.microphone.off_while_disabled.delay_ms".
   *
   * The delay is in place to prevent misuse by malicious sites. If a track is
   * re-enabled before the delay has passed, the device will not be touched
   * until another disable followed by the full delay happens.
   */
  void SetEnabledFor(TrackID aTrackID, bool aEnabled);

  /**
   * Stops all screen/app/window/audioCapture sharing, but not camera or
   * microphone.
   */
  void StopSharing();

  MediaStream* Stream() const
  {
    return mStream;
  }

  SourceMediaStream* GetSourceStream();

  MediaDevice* GetAudioDevice() const
  {
    return mAudioDeviceState ? mAudioDeviceState->mDevice.get() : nullptr;
  }

  MediaDevice* GetVideoDevice() const
  {
    return mVideoDeviceState ? mVideoDeviceState->mDevice.get() : nullptr;
  }

  /**
   * Called on MediaStreamGraph thread when MSG asks us for more data from
   * input devices.
   */
  void NotifyPull(MediaStreamGraph* aGraph,
                  StreamTime aDesiredTime);

  /**
   * Called on main thread after MediaStreamGraph notifies us that our
   * MediaStream was marked finish in the graph.
   */
  void NotifyFinished();

  /**
   * Called on main thread after MediaStreamGraph notifies us that we
   * were removed as listener from the MediaStream in the graph.
   */
  void NotifyRemoved();

  bool Activated() const
  {
    return mStream;
  }

  bool Stopped() const
  {
    return mStopped;
  }

  bool CapturingVideo() const;

  bool CapturingAudio() const;

  CaptureState CapturingSource(MediaSourceEnum aSource) const;

  RefPtr<ApplyConstraintsPromise>
  ApplyConstraintsToTrack(nsPIDOMWindowInner* aWindow,
                          TrackID aTrackID,
                          const dom::MediaTrackConstraints& aConstraints,
                          dom::CallerType aCallerType);

  PrincipalHandle GetPrincipalHandle() const;

private:
  /**
   * Wrapper class for the MediaStreamListener part of SourceListener.
   *
   * This is required since MediaStreamListener and SupportsWeakPtr
   * both implement refcounting.
   */
  class SourceStreamListener : public MediaStreamListener {
  public:
    explicit SourceStreamListener(SourceListener* aSourceListener)
      : mSourceListener(aSourceListener)
    {
    }

    void NotifyPull(MediaStreamGraph* aGraph,
                    StreamTime aDesiredTime) override
    {
      mSourceListener->NotifyPull(aGraph, aDesiredTime);
    }

    void NotifyEvent(MediaStreamGraph* aGraph,
                     MediaStreamGraphEvent aEvent) override
    {
      nsCOMPtr<nsIEventTarget> target;

      switch (aEvent) {
        case MediaStreamGraphEvent::EVENT_FINISHED:
          target = GetMainThreadEventTarget();
          if (NS_WARN_IF(!target)) {
            NS_ASSERTION(false, "Mainthread not available; running on current thread");
            // Ensure this really *was* MainThread (NS_GetCurrentThread won't work)
            MOZ_RELEASE_ASSERT(mSourceListener->mMainThreadCheck == GetCurrentVirtualThread());
            mSourceListener->NotifyFinished();
            return;
          }
          target->Dispatch(NewRunnableMethod("SourceListener::NotifyFinished",
                                             mSourceListener,
                                             &SourceListener::NotifyFinished),
                           NS_DISPATCH_NORMAL);
          break;
        case MediaStreamGraphEvent::EVENT_REMOVED:
          target = GetMainThreadEventTarget();
          if (NS_WARN_IF(!target)) {
            NS_ASSERTION(false, "Mainthread not available; running on current thread");
            // Ensure this really *was* MainThread (NS_GetCurrentThread won't work)
            MOZ_RELEASE_ASSERT(mSourceListener->mMainThreadCheck == GetCurrentVirtualThread());
            mSourceListener->NotifyRemoved();
            return;
          }
          target->Dispatch(NewRunnableMethod("SourceListener::NotifyRemoved",
                                             mSourceListener,
                                             &SourceListener::NotifyRemoved),
                           NS_DISPATCH_NORMAL);
          break;
        default:
          break;
      }
    }
  private:
    RefPtr<SourceListener> mSourceListener;
  };

  virtual ~SourceListener() = default;

  /**
   * Returns a pointer to the device state for aTrackID.
   *
   * This is intended for internal use where we need to figure out which state
   * corresponds to aTrackID, not for availability checks. As such, we assert
   * that the device does indeed exist.
   *
   * Since this is a raw pointer and the state lifetime depends on the
   * SourceListener's lifetime, it's internal use only.
   */
  DeviceState& GetDeviceStateFor(TrackID aTrackID) const;

  // true after this listener has had all devices stopped. MainThread only.
  bool mStopped;

  // true after the stream this listener is listening to has finished in the
  // MediaStreamGraph. MainThread only.
  bool mFinished;

  // true after this listener has been removed from its MediaStream.
  // MainThread only.
  bool mRemoved;

  // never ever indirect off this; just for assertions
  PRThread* mMainThreadCheck;

  // Set in Register() on main thread, then read from any thread.
  PrincipalHandle mPrincipalHandle;

  // Weak pointer to the window listener that owns us. MainThread only.
  GetUserMediaWindowListener* mWindowListener;

  // Accessed from MediaStreamGraph thread, MediaManager thread, and MainThread
  // No locking needed as they're set on Activate() and never assigned to again.
  UniquePtr<DeviceState> mAudioDeviceState;
  UniquePtr<DeviceState> mVideoDeviceState;
  RefPtr<SourceMediaStream> mStream; // threadsafe refcnt
  RefPtr<SourceStreamListener> mStreamListener; // threadsafe refcnt
};

/**
 * This class represents a WindowID and handles all MediaStreamListeners
 * (here subclassed as SourceListeners) used to feed GetUserMedia source
 * streams. It proxies feedback from them into messages for browser chrome.
 * The SourceListeners are used to Start() and Stop() the underlying
 * MediaEngineSource when MediaStreams are assigned and deassigned in content.
 */
class GetUserMediaWindowListener
{
  friend MediaManager;
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GetUserMediaWindowListener)

  // Create in an inactive state
  GetUserMediaWindowListener(base::Thread *aThread,
    uint64_t aWindowID,
    const PrincipalHandle& aPrincipalHandle)
    : mMediaThread(aThread)
    , mWindowID(aWindowID)
    , mPrincipalHandle(aPrincipalHandle)
    , mChromeNotificationTaskPosted(false)
  {}

  /**
   * Registers an inactive gUM source listener for this WindowListener.
   */
  void Register(SourceListener* aListener)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aListener);
    MOZ_ASSERT(!aListener->Activated());
    MOZ_ASSERT(!mInactiveListeners.Contains(aListener), "Already registered");
    MOZ_ASSERT(!mActiveListeners.Contains(aListener), "Already activated");

    aListener->Register(this);
    mInactiveListeners.AppendElement(aListener);
  }

  /**
   * Activates an already registered and inactive gUM source listener for this
   * WindowListener.
   */
  void Activate(SourceListener* aListener,
                SourceMediaStream* aStream,
                MediaDevice* aAudioDevice,
                MediaDevice* aVideoDevice)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aListener);
    MOZ_ASSERT(!aListener->Activated());
    MOZ_ASSERT(mInactiveListeners.Contains(aListener), "Must be registered to activate");
    MOZ_ASSERT(!mActiveListeners.Contains(aListener), "Already activated");

    mInactiveListeners.RemoveElement(aListener);
    aListener->Activate(aStream, aAudioDevice, aVideoDevice);
    mActiveListeners.AppendElement(do_AddRef(aListener));
  }

  // Can be invoked from EITHER MainThread or MSG thread
  void Stop()
  {
    MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

    for (auto& source : mActiveListeners) {
      source->Stop();
    }

    // Once all tracks have stopped, that will trigger the chrome notification
  }

  /**
   * Removes all SourceListeners from this window listener.
   * Removes this window listener from the list of active windows, so callers
   * need to make sure to hold a strong reference.
   */
  void RemoveAll()
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Shallow copy since SourceListener::Remove() will modify the arrays.
    nsTArray<RefPtr<SourceListener>> listeners(mInactiveListeners.Length()
                                               + mActiveListeners.Length());
    listeners.AppendElements(mInactiveListeners);
    listeners.AppendElements(mActiveListeners);
    for (auto& l : listeners) {
      Remove(l);
    }

    MOZ_ASSERT(mInactiveListeners.Length() == 0);
    MOZ_ASSERT(mActiveListeners.Length() == 0);

    MediaManager* mgr = MediaManager::GetIfExists();
    if (!mgr) {
      MOZ_ASSERT(false, "MediaManager should stay until everything is removed");
      return;
    }
    GetUserMediaWindowListener* windowListener =
      mgr->GetWindowListener(mWindowID);

    if (!windowListener) {
      nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
      auto* globalWindow = nsGlobalWindowInner::GetInnerWindowWithId(mWindowID);
      if (globalWindow) {
        RefPtr<GetUserMediaRequest> req =
          new GetUserMediaRequest(globalWindow->AsInner(),
                                  VoidString(), VoidString());
        obs->NotifyObservers(req, "recording-device-stopped", nullptr);
      }
      return;
    }

    MOZ_ASSERT(windowListener == this,
               "There should only be one window listener per window ID");

    LOG(("GUMWindowListener %p removing windowID %" PRIu64, this, mWindowID));
    mgr->RemoveWindowID(mWindowID);
  }

  bool Remove(SourceListener* aListener)
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mInactiveListeners.RemoveElement(aListener) &&
        !mActiveListeners.RemoveElement(aListener)) {
      return false;
    }

    MOZ_ASSERT(!mInactiveListeners.Contains(aListener),
               "A SourceListener should only be once in one of "
               "mInactiveListeners and mActiveListeners");
    MOZ_ASSERT(!mActiveListeners.Contains(aListener),
               "A SourceListener should only be once in one of "
               "mInactiveListeners and mActiveListeners");

    LOG(("GUMWindowListener %p removing SourceListener %p.", this, aListener));
    aListener->Remove();

    if (MediaDevice* removedDevice = aListener->GetVideoDevice()) {
      bool revokeVideoPermission = true;
      nsString removedRawId;
      nsString removedSourceType;
      removedDevice->GetRawId(removedRawId);
      removedDevice->GetMediaSource(removedSourceType);
      for (const auto& l : mActiveListeners) {
        if (MediaDevice* device = l->GetVideoDevice()) {
          nsString rawId;
          device->GetRawId(rawId);
          if (removedRawId.Equals(rawId)) {
            revokeVideoPermission = false;
            break;
          }
        }
      }

      if (revokeVideoPermission) {
        nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
        auto* globalWindow = nsGlobalWindowInner::GetInnerWindowWithId(mWindowID);
        nsPIDOMWindowInner* window = globalWindow ? globalWindow->AsInner()
                                                  : nullptr;
        RefPtr<GetUserMediaRequest> req =
          new GetUserMediaRequest(window, removedRawId, removedSourceType);
        obs->NotifyObservers(req, "recording-device-stopped", nullptr);
      }
    }

    if (MediaDevice* removedDevice = aListener->GetAudioDevice()) {
      bool revokeAudioPermission = true;
      nsString removedRawId;
      nsString removedSourceType;
      removedDevice->GetRawId(removedRawId);
      removedDevice->GetMediaSource(removedSourceType);
      for (const auto& l : mActiveListeners) {
        if (MediaDevice* device = l->GetAudioDevice()) {
          nsString rawId;
          device->GetRawId(rawId);
          if (removedRawId.Equals(rawId)) {
            revokeAudioPermission = false;
            break;
          }
        }
      }

      if (revokeAudioPermission) {
        nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
        auto* globalWindow = nsGlobalWindowInner::GetInnerWindowWithId(mWindowID);
        nsPIDOMWindowInner* window = globalWindow ? globalWindow->AsInner()
                                                  : nullptr;
        RefPtr<GetUserMediaRequest> req =
          new GetUserMediaRequest(window, removedRawId, removedSourceType);
        obs->NotifyObservers(req, "recording-device-stopped", nullptr);
      }
    }

    if (mInactiveListeners.Length() == 0 &&
        mActiveListeners.Length() == 0) {
      LOG(("GUMWindowListener %p Removed the last SourceListener. "
           "Cleaning up.", this));
      RemoveAll();
    }

    return true;
  }

  void StopSharing();

  void StopRawID(const nsString& removedDeviceID);

  /**
   * Called by one of our SourceListeners when one of its tracks has changed so
   * that chrome state is affected.
   * Schedules an event for the next stable state to update chrome.
   */
  void ChromeAffectingStateChanged();

  /**
   * Called in stable state to send a notification to update chrome.
   */
  void NotifyChrome();

  bool CapturingVideo() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    for (auto& l : mActiveListeners) {
      if (l->CapturingVideo()) {
        return true;
      }
    }
    return false;
  }

  bool CapturingAudio() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    for (auto& l : mActiveListeners) {
      if (l->CapturingAudio()) {
        return true;
      }
    }
    return false;
  }

  CaptureState CapturingSource(MediaSourceEnum aSource) const
  {
    MOZ_ASSERT(NS_IsMainThread());
    CaptureState result = CaptureState::Off;
    for (auto& l : mActiveListeners) {
      result = CombineCaptureState(result, l->CapturingSource(aSource));
    }
    return result;
  }

  uint64_t WindowID() const
  {
    return mWindowID;
  }

  PrincipalHandle GetPrincipalHandle() const { return mPrincipalHandle; }

private:
  ~GetUserMediaWindowListener()
  {
    MOZ_ASSERT(mInactiveListeners.Length() == 0,
               "Inactive listeners should already be removed");
    MOZ_ASSERT(mActiveListeners.Length() == 0,
               "Active listeners should already be removed");
    Unused << mMediaThread;
    // It's OK to release mStream on any thread; they have thread-safe
    // refcounts.
  }

  // Set at construction
  base::Thread* mMediaThread;

  uint64_t mWindowID;
  const PrincipalHandle mPrincipalHandle;

  // true if we have scheduled a task to notify chrome in the next stable state.
  // The task will reset this to false. MainThread only.
  bool mChromeNotificationTaskPosted;

  nsTArray<RefPtr<SourceListener>> mInactiveListeners;
  nsTArray<RefPtr<SourceListener>> mActiveListeners;
};

/**
 * Send an error back to content. Do this only on the main thread.
 */
class ErrorCallbackRunnable : public Runnable
{
public:
  ErrorCallbackRunnable(const nsMainThreadPtrHandle<nsIDOMGetUserMediaErrorCallback>& aOnFailure,
                        MediaMgrError& aError,
                        uint64_t aWindowID)
    : Runnable("ErrorCallbackRunnable")
    , mOnFailure(aOnFailure)
    , mError(&aError)
    , mWindowID(aWindowID)
    , mManager(MediaManager::GetInstance())
  {
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Only run if the window is still active.
    if (!(mManager->IsWindowStillActive(mWindowID))) {
      return NS_OK;
    }
    // This is safe since we're on main-thread, and the windowlist can only
    // be invalidated from the main-thread (see OnNavigation)
    if (auto* window = nsGlobalWindowInner::GetInnerWindowWithId(mWindowID)) {
      RefPtr<MediaStreamError> error =
        new MediaStreamError(window->AsInner(), *mError);
      mOnFailure->OnError(error);
    }
    return NS_OK;
  }
private:
  ~ErrorCallbackRunnable() override = default;

  nsMainThreadPtrHandle<nsIDOMGetUserMediaErrorCallback> mOnFailure;
  RefPtr<MediaMgrError> mError;
  uint64_t mWindowID;
  RefPtr<MediaManager> mManager; // get ref to this when creating the runnable
};

/**
 * nsIMediaDevice implementation.
 */
NS_IMPL_ISUPPORTS(MediaDevice, nsIMediaDevice)

MediaDevice::MediaDevice(MediaEngineSource* aSource,
                         const nsString& aName,
                         const nsString& aID,
                         const nsString& aRawID)
  : mSource(aSource)
  , mKind((mSource && MediaEngineSource::IsVideo(mSource->GetMediaSource())) ?
          dom::MediaDeviceKind::Videoinput : dom::MediaDeviceKind::Audioinput)
  , mScary(mSource->GetScary())
  , mType(NS_ConvertUTF8toUTF16(dom::MediaDeviceKindValues::strings[uint32_t(mKind)].value))
  , mName(aName)
  , mID(aID)
  , mRawID(aRawID)
{
  MOZ_ASSERT(mSource);
}

MediaDevice::MediaDevice(const nsString& aName,
                         const dom::MediaDeviceKind aKind,
                         const nsString& aID,
                         const nsString& aRawID)
  : mSource(nullptr)
  , mKind(aKind)
  , mScary(false)
  , mType(NS_ConvertUTF8toUTF16(dom::MediaDeviceKindValues::strings[uint32_t(mKind)].value))
  , mName(aName)
  , mID(aID)
  , mRawID(aRawID)
{
  // For now this ctor is used only for Audiooutput.
  // It could be used for Audioinput and Videoinput
  // when we do not instantiate a MediaEngineSource
  // during EnumerateDevices.
  MOZ_ASSERT(mKind == dom::MediaDeviceKind::Audiooutput);
}

MediaDevice::MediaDevice(const MediaDevice* aOther,
                         const nsString& aID,
                         const nsString& aRawID)
  : mSource(aOther->mSource)
  , mKind(aOther->mKind)
  , mScary(aOther->mScary)
  , mType(aOther->mType)
  , mName(aOther->mName)
  , mID(aID)
  , mRawID(aRawID)
{
  MOZ_ASSERT(aOther);
}

/**
 * Helper functions that implement the constraints algorithm from
 * http://dev.w3.org/2011/webrtc/editor/getusermedia.html#methods-5
 */

/* static */ bool
MediaDevice::StringsContain(const OwningStringOrStringSequence& aStrings,
                            nsString aN)
{
  return aStrings.IsString() ? aStrings.GetAsString() == aN
                             : aStrings.GetAsStringSequence().Contains(aN);
}

/* static */ uint32_t
MediaDevice::FitnessDistance(nsString aN,
                             const ConstrainDOMStringParameters& aParams)
{
  if (aParams.mExact.WasPassed() && !StringsContain(aParams.mExact.Value(), aN)) {
    return UINT32_MAX;
  }
  if (aParams.mIdeal.WasPassed() && !StringsContain(aParams.mIdeal.Value(), aN)) {
    return 1;
  }
  return 0;
}

// Binding code doesn't templatize well...

/* static */ uint32_t
MediaDevice::FitnessDistance(nsString aN,
    const OwningStringOrStringSequenceOrConstrainDOMStringParameters& aConstraint)
{
  if (aConstraint.IsString()) {
    ConstrainDOMStringParameters params;
    params.mIdeal.Construct();
    params.mIdeal.Value().SetAsString() = aConstraint.GetAsString();
    return FitnessDistance(aN, params);
  } else if (aConstraint.IsStringSequence()) {
    ConstrainDOMStringParameters params;
    params.mIdeal.Construct();
    params.mIdeal.Value().SetAsStringSequence() = aConstraint.GetAsStringSequence();
    return FitnessDistance(aN, params);
  } else {
    return FitnessDistance(aN, aConstraint.GetAsConstrainDOMStringParameters());
  }
}

uint32_t
MediaDevice::GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
    bool aIsChrome)
{
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);

  nsString mediaSource;
  GetMediaSource(mediaSource);

  // This code is reused for audio, where the mediaSource constraint does
  // not currently have a function, but because it defaults to "camera" in
  // webidl, we ignore it for audio here.
  if (!mediaSource.EqualsASCII("microphone")) {
    for (const auto& constraint : aConstraintSets) {
      if (constraint->mMediaSource.mIdeal.find(mediaSource) ==
          constraint->mMediaSource.mIdeal.end()) {
        return UINT32_MAX;
      }
    }
  }
  // Forward request to underlying object to interrogate per-mode capabilities.
  // Pass in device's origin-specific id for deviceId constraint comparison.
  const nsString& id = aIsChrome ? mRawID : mID;
  return mSource->GetBestFitnessDistance(aConstraintSets, id);
}

NS_IMETHODIMP
MediaDevice::GetName(nsAString& aName)
{
  MOZ_ASSERT(NS_IsMainThread());
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
MediaDevice::GetType(nsAString& aType)
{
  MOZ_ASSERT(NS_IsMainThread());
  aType.Assign(mType);
  return NS_OK;
}

NS_IMETHODIMP
MediaDevice::GetId(nsAString& aID)
{
  MOZ_ASSERT(NS_IsMainThread());
  aID.Assign(mID);
  return NS_OK;
}

NS_IMETHODIMP
MediaDevice::GetRawId(nsAString& aID)
{
  MOZ_ASSERT(NS_IsMainThread());
  aID.Assign(mRawID);
  return NS_OK;
}

NS_IMETHODIMP
MediaDevice::GetScary(bool* aScary)
{
  *aScary = mScary;
  return NS_OK;
}

void
MediaDevice::GetSettings(dom::MediaTrackSettings& aOutSettings) const
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mSource);
  mSource->GetSettings(aOutSettings);
}

  // Threadsafe since mSource is const.
NS_IMETHODIMP
MediaDevice::GetMediaSource(nsAString& aMediaSource)
{
  aMediaSource.Assign(NS_ConvertUTF8toUTF16(
    dom::MediaSourceEnumValues::strings[uint32_t(GetMediaSource())].value));
  return NS_OK;
}

nsresult
MediaDevice::Allocate(const dom::MediaTrackConstraints &aConstraints,
                      const MediaEnginePrefs &aPrefs,
                      const ipc::PrincipalInfo& aPrincipalInfo,
                      const char** aOutBadConstraint)
{
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);
  return mSource->Allocate(aConstraints,
                           aPrefs,
                           mID,
                           aPrincipalInfo,
                           getter_AddRefs(mAllocationHandle),
                           aOutBadConstraint);
}

nsresult
MediaDevice::SetTrack(const RefPtr<SourceMediaStream>& aStream,
                      TrackID aTrackID,
                      const PrincipalHandle& aPrincipalHandle)
{
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);
  return mSource->SetTrack(mAllocationHandle, aStream, aTrackID, aPrincipalHandle);
}

nsresult
MediaDevice::Start()
{
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);
  return mSource->Start(mAllocationHandle);
}

nsresult
MediaDevice::Reconfigure(const dom::MediaTrackConstraints &aConstraints,
                         const MediaEnginePrefs &aPrefs,
                         const char** aOutBadConstraint)
{
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);
  return mSource->Reconfigure(mAllocationHandle,
                              aConstraints,
                              aPrefs,
                              mID,
                              aOutBadConstraint);
}

nsresult
MediaDevice::FocusOnSelectedSource()
{
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);
  return mSource->FocusOnSelectedSource(mAllocationHandle);
}

nsresult
MediaDevice::Stop()
{
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);
  return mSource->Stop(mAllocationHandle);
}

nsresult
MediaDevice::Deallocate()
{
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);
  return mSource->Deallocate(mAllocationHandle);
}

void
MediaDevice::Pull(const RefPtr<SourceMediaStream>& aStream,
                  TrackID aTrackID,
                  StreamTime aDesiredTime,
                  const PrincipalHandle& aPrincipal)
{
  // This is on the graph thread, but mAllocationHandle is safe since we never
  // change it after it's been set, which is guaranteed to happen before
  // registering the listener for pulls.
  MOZ_ASSERT(mSource);
  mSource->Pull(mAllocationHandle, aStream, aTrackID, aDesiredTime, aPrincipal);
}

dom::MediaSourceEnum
MediaDevice::GetMediaSource() const
{
  // Threadsafe because mSource is const. GetMediaSource() might have other
  // requirements.
  MOZ_ASSERT(mSource);
  return mSource->GetMediaSource();
}

static bool
IsOn(const OwningBooleanOrMediaTrackConstraints &aUnion) {
  return !aUnion.IsBoolean() || aUnion.GetAsBoolean();
}

static const MediaTrackConstraints&
GetInvariant(const OwningBooleanOrMediaTrackConstraints &aUnion) {
  static const MediaTrackConstraints empty;
  return aUnion.IsMediaTrackConstraints() ?
      aUnion.GetAsMediaTrackConstraints() : empty;
}

/**
 * This class is only needed since fake tracks are added dynamically.
 * Instead of refactoring to add them explicitly we let the DOMMediaStream
 * query us for the source as they become available.
 * Since they are used only for testing the API surface, we make them very
 * simple.
 */
class FakeTrackSourceGetter : public MediaStreamTrackSourceGetter
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FakeTrackSourceGetter,
                                           MediaStreamTrackSourceGetter)

  explicit FakeTrackSourceGetter(nsIPrincipal* aPrincipal)
    : mPrincipal(aPrincipal) {}

  already_AddRefed<dom::MediaStreamTrackSource>
  GetMediaStreamTrackSource(TrackID aInputTrackID) override
  {
    NS_ASSERTION(kAudioTrack != aInputTrackID,
                 "Only fake tracks should appear dynamically");
    NS_ASSERTION(kVideoTrack != aInputTrackID,
                 "Only fake tracks should appear dynamically");
    return do_AddRef(new BasicTrackSource(mPrincipal));
  }

protected:
  virtual ~FakeTrackSourceGetter() {}

  nsCOMPtr<nsIPrincipal> mPrincipal;
};

NS_IMPL_ADDREF_INHERITED(FakeTrackSourceGetter, MediaStreamTrackSourceGetter)
NS_IMPL_RELEASE_INHERITED(FakeTrackSourceGetter, MediaStreamTrackSourceGetter)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FakeTrackSourceGetter)
NS_INTERFACE_MAP_END_INHERITING(MediaStreamTrackSourceGetter)
NS_IMPL_CYCLE_COLLECTION_INHERITED(FakeTrackSourceGetter,
                                   MediaStreamTrackSourceGetter,
                                   mPrincipal)

/**
 * Creates a MediaStream, attaches a listener and fires off a success callback
 * to the DOM with the stream. We also pass in the error callback so it can
 * be released correctly.
 *
 * All of this must be done on the main thread!
 *
 * Note that the various GetUserMedia Runnable classes currently allow for
 * two streams.  If we ever need to support getting more than two streams
 * at once, we could convert everything to nsTArray<RefPtr<blah> >'s,
 * though that would complicate the constructors some.  Currently the
 * GetUserMedia spec does not allow for more than 2 streams to be obtained in
 * one call, to simplify handling of constraints.
 */
class GetUserMediaStreamRunnable : public Runnable
{
public:
  GetUserMediaStreamRunnable(
    const nsMainThreadPtrHandle<nsIDOMGetUserMediaSuccessCallback>& aOnSuccess,
    const nsMainThreadPtrHandle<nsIDOMGetUserMediaErrorCallback>& aOnFailure,
    uint64_t aWindowID,
    GetUserMediaWindowListener* aWindowListener,
    SourceListener* aSourceListener,
    const ipc::PrincipalInfo& aPrincipalInfo,
    const MediaStreamConstraints& aConstraints,
    MediaDevice* aAudioDevice,
    MediaDevice* aVideoDevice,
    PeerIdentity* aPeerIdentity,
    bool aIsChrome)
    : Runnable("GetUserMediaStreamRunnable")
    , mOnSuccess(aOnSuccess)
    , mOnFailure(aOnFailure)
    , mConstraints(aConstraints)
    , mAudioDevice(aAudioDevice)
    , mVideoDevice(aVideoDevice)
    , mWindowID(aWindowID)
    , mWindowListener(aWindowListener)
    , mSourceListener(aSourceListener)
    , mPrincipalInfo(aPrincipalInfo)
    , mPeerIdentity(aPeerIdentity)
    , mManager(MediaManager::GetInstance())
  {
  }

  ~GetUserMediaStreamRunnable() {}

  class TracksAvailableCallback : public OnTracksAvailableCallback
  {
  public:
    TracksAvailableCallback(MediaManager* aManager,
                            const nsMainThreadPtrHandle<nsIDOMGetUserMediaSuccessCallback>& aSuccess,
                            const RefPtr<GetUserMediaWindowListener>& aWindowListener,
                            DOMMediaStream* aStream)
      : mWindowListener(aWindowListener),
        mOnSuccess(aSuccess),
        mManager(aManager),
        mStream(aStream)
    {}
    void NotifyTracksAvailable(DOMMediaStream* aStream) override
    {
      // We're on the main thread, so no worries here.
      if (!mManager->IsWindowListenerStillActive(mWindowListener)) {
        return;
      }

      // Start currentTime from the point where this stream was successfully
      // returned.
      aStream->SetLogicalStreamStartTime(aStream->GetPlaybackStream()->GetCurrentTime());

      // This is safe since we're on main-thread, and the windowlist can only
      // be invalidated from the main-thread (see OnNavigation)
      LOG(("Returning success for getUserMedia()"));
      mOnSuccess->OnSuccess(aStream);
    }
    RefPtr<GetUserMediaWindowListener> mWindowListener;
    nsMainThreadPtrHandle<nsIDOMGetUserMediaSuccessCallback> mOnSuccess;
    RefPtr<MediaManager> mManager;
    // Keep the DOMMediaStream alive until the NotifyTracksAvailable callback
    // has fired, otherwise we might immediately destroy the DOMMediaStream and
    // shut down the underlying MediaStream prematurely.
    // This creates a cycle which is broken when NotifyTracksAvailable
    // is fired (which will happen unless the browser shuts down,
    // since we only add this callback when we've successfully appended
    // the desired tracks in the MediaStreamGraph) or when
    // DOMMediaStream::NotifyMediaStreamGraphShutdown is called.
    RefPtr<DOMMediaStream> mStream;
  };

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    LOG(("GetUserMediaStreamRunnable::Run()"));
    nsGlobalWindowInner* globalWindow = nsGlobalWindowInner::GetInnerWindowWithId(mWindowID);
    nsPIDOMWindowInner* window = globalWindow ? globalWindow->AsInner() : nullptr;

    // We're on main-thread, and the windowlist can only
    // be invalidated from the main-thread (see OnNavigation)
    if (!mManager->IsWindowListenerStillActive(mWindowListener)) {
      // This window is no longer live. mListener has already been removed.
      return NS_OK;
    }

    MediaStreamGraph::GraphDriverType graphDriverType =
      mAudioDevice ? MediaStreamGraph::AUDIO_THREAD_DRIVER
                   : MediaStreamGraph::SYSTEM_THREAD_DRIVER;
    MediaStreamGraph* msg =
      MediaStreamGraph::GetInstance(graphDriverType, window,
                                    MediaStreamGraph::REQUEST_DEFAULT_SAMPLE_RATE);

    nsMainThreadPtrHandle<DOMMediaStream> domStream;
    RefPtr<SourceMediaStream> stream;
    // AudioCapture is a special case, here, in the sense that we're not really
    // using the audio source and the SourceMediaStream, which acts as
    // placeholders. We re-route a number of stream internaly in the MSG and mix
    // them down instead.
    if (mAudioDevice &&
        mAudioDevice->GetMediaSource() == MediaSourceEnum::AudioCapture) {
      NS_WARNING("MediaCaptureWindowState doesn't handle "
                 "MediaSourceEnum::AudioCapture. This must be fixed with UX "
                 "before shipping.");
      // It should be possible to pipe the capture stream to anything. CORS is
      // not a problem here, we got explicit user content.
      nsCOMPtr<nsIPrincipal> principal = window->GetExtantDoc()->NodePrincipal();
      domStream = new nsMainThreadPtrHolder<DOMMediaStream>(
        "GetUserMediaStreamRunnable::AudioCaptureDOMStreamMainThreadHolder",
        DOMMediaStream::CreateAudioCaptureStreamAsInput(window, principal, msg));

      stream = msg->CreateSourceStream(); // Placeholder
      msg->RegisterCaptureStreamForWindow(
            mWindowID, domStream->GetInputStream()->AsProcessedStream());
      window->SetAudioCapture(true);
    } else {
      class LocalTrackSource : public MediaStreamTrackSource
      {
      public:
        LocalTrackSource(nsIPrincipal* aPrincipal,
                         const nsString& aLabel,
                         const RefPtr<SourceListener>& aListener,
                         const MediaSourceEnum aSource,
                         const TrackID aTrackID,
                         const PeerIdentity* aPeerIdentity)
          : MediaStreamTrackSource(aPrincipal, aLabel),
            mListener(aListener.get()),
            mSource(aSource),
            mTrackID(aTrackID),
            mPeerIdentity(aPeerIdentity)
        {}

        MediaSourceEnum GetMediaSource() const override
        {
          return mSource;
        }

        const PeerIdentity* GetPeerIdentity() const override
        {
          return mPeerIdentity;
        }

        already_AddRefed<PledgeVoid>
        ApplyConstraints(nsPIDOMWindowInner* aWindow,
                         const MediaTrackConstraints& aConstraints,
                         dom::CallerType aCallerType) override
        {
          RefPtr<PledgeVoid> p = new PledgeVoid();
          if (sHasShutdown || !mListener) {
            // Track has been stopped, or we are in shutdown. In either case
            // there's no observable outcome, so pretend we succeeded.
            p->Resolve(false);
            return p.forget();
          }

          mListener->ApplyConstraintsToTrack(aWindow, mTrackID,
                                             aConstraints, aCallerType)
            ->Then(GetMainThreadSerialEventTarget(), __func__,
                [p]()
                {
                  if (!MediaManager::Exists()) {
                    return;
                  }

                  p->Resolve(false);
                },
                [p, weakWindow = nsWeakPtr(do_GetWeakReference(aWindow)),
                 listener = mListener, trackID = mTrackID]
                (Maybe<nsString>&& aBadConstraint)
                {
                  if (!MediaManager::Exists()) {
                    return;
                  }

                  if (!weakWindow->IsAlive()) {
                    return;
                  }

                  if (aBadConstraint.isNothing()) {
                    // Unexpected error during reconfig that left the source
                    // stopped. We resolve the promise and end the track.
                    if (listener) {
                      listener->StopTrack(trackID);
                    }
                    p->Resolve(false);
                    return;
                  }

                  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(weakWindow);
                  auto error = MakeRefPtr<MediaStreamError>(
                      window,
                      MediaMgrError::Name::OverconstrainedError,
                      NS_LITERAL_STRING(""),
                      aBadConstraint.valueOr(nsString()));
                  p->Reject(error);
                });

          return p.forget();
        }

        void
        GetSettings(dom::MediaTrackSettings& aOutSettings) override
        {
          if (mListener) {
            mListener->GetSettingsFor(mTrackID, aOutSettings);
          }
        }

        void Stop() override
        {
          if (mListener) {
            mListener->StopTrack(mTrackID);
            mListener = nullptr;
          }
        }

        void Disable() override
        {
          if (mListener) {
            mListener->SetEnabledFor(mTrackID, false);
          }
        }

        void Enable() override
        {
          if (mListener) {
            mListener->SetEnabledFor(mTrackID, true);
          }
        }

      protected:
        ~LocalTrackSource() {}

        // This is a weak pointer to avoid having the SourceListener (which may
        // have references to threads and threadpools) kept alive by DOM-objects
        // that may have ref-cycles and thus are released very late during
        // shutdown, even after xpcom-shutdown-threads. See bug 1351655 for what
        // can happen.
        WeakPtr<SourceListener> mListener;
        const MediaSourceEnum mSource;
        const TrackID mTrackID;
        const RefPtr<const PeerIdentity> mPeerIdentity;
      };

      nsCOMPtr<nsIPrincipal> principal;
      if (mPeerIdentity) {
        principal = NullPrincipal::CreateWithInheritedAttributes(window->GetExtantDoc()->NodePrincipal());
      } else {
        principal = window->GetExtantDoc()->NodePrincipal();
      }

      // Normal case, connect the source stream to the track union stream to
      // avoid us blocking. Pass a simple TrackSourceGetter for potential
      // fake tracks. Apart from them gUM never adds tracks dynamically.
      domStream = new nsMainThreadPtrHolder<DOMMediaStream>(
        "GetUserMediaStreamRunnable::DOMMediaStreamMainThreadHolder",
        DOMLocalMediaStream::CreateSourceStreamAsInput(window, msg,
                                                       new FakeTrackSourceGetter(principal)));
      stream = domStream->GetInputStream()->AsSourceStream();

      if (mAudioDevice) {
        nsString audioDeviceName;
        mAudioDevice->GetName(audioDeviceName);
        const MediaSourceEnum source = mAudioDevice->GetMediaSource();
        RefPtr<MediaStreamTrackSource> audioSource =
          new LocalTrackSource(principal, audioDeviceName, mSourceListener,
                               source, kAudioTrack, mPeerIdentity);
        MOZ_ASSERT(IsOn(mConstraints.mAudio));
        RefPtr<MediaStreamTrack> track =
          domStream->CreateDOMTrack(kAudioTrack, MediaSegment::AUDIO, audioSource,
                                    GetInvariant(mConstraints.mAudio));
        domStream->AddTrackInternal(track);
      }
      if (mVideoDevice) {
        nsString videoDeviceName;
        mVideoDevice->GetName(videoDeviceName);
        const MediaSourceEnum source = mVideoDevice->GetMediaSource();
        RefPtr<MediaStreamTrackSource> videoSource =
          new LocalTrackSource(principal, videoDeviceName, mSourceListener,
                               source, kVideoTrack, mPeerIdentity);
        MOZ_ASSERT(IsOn(mConstraints.mVideo));
        RefPtr<MediaStreamTrack> track =
          domStream->CreateDOMTrack(kVideoTrack, MediaSegment::VIDEO, videoSource,
                                    GetInvariant(mConstraints.mVideo));
        domStream->AddTrackInternal(track);
      }
    }

    if (!domStream || !stream || sHasShutdown) {
      LOG(("Returning error for getUserMedia() - no stream"));

      if (auto* window = nsGlobalWindowInner::GetInnerWindowWithId(mWindowID)) {
        RefPtr<MediaStreamError> error = new MediaStreamError(window->AsInner(),
            MediaStreamError::Name::AbortError,
            sHasShutdown ? NS_LITERAL_STRING("In shutdown") :
                           NS_LITERAL_STRING("No stream."));
        mOnFailure->OnError(error);
      }
      return NS_OK;
    }

    // Activate our source listener. We'll call Start() on the source when we
    // get a callback that the MediaStream has started consuming. The listener
    // is freed when the page is invalidated (on navigation or close).
    mWindowListener->Activate(mSourceListener, stream, mAudioDevice, mVideoDevice);

    // Note: includes JS callbacks; must be released on MainThread
    typedef Refcountable<UniquePtr<TracksAvailableCallback>> Callback;
    nsMainThreadPtrHandle<Callback> callback(
      new nsMainThreadPtrHolder<Callback>(
        "GetUserMediaStreamRunnable::TracksAvailableCallbackMainThreadHolder",
        MakeAndAddRef<Callback>(
          new TracksAvailableCallback(mManager,
                                      mOnSuccess,
                                      mWindowListener,
                                      domStream))));

    // Dispatch to the media thread to ask it to start the sources,
    // because that can take a while.
    // Pass ownership of domStream through the lambda to the nested chrome
    // notification lambda to ensure it's kept alive until that lambda runs or is discarded.
    mSourceListener->InitializeAsync()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [manager = mManager, domStream, callback,
       windowListener = mWindowListener]()
      {
        LOG(("GetUserMediaStreamRunnable::Run: starting success callback "
             "following InitializeAsync()"));
        // Initiating and starting devices succeeded.
        // onTracksAvailableCallback must be added to domStream on main thread.
        domStream->OnTracksAvailable(callback->release());
        windowListener->ChromeAffectingStateChanged();
        manager->SendPendingGUMRequest();
      },[manager = mManager, windowID = mWindowID,
         onFailure = std::move(mOnFailure)](const RefPtr<MediaMgrError>& error)
      {
        LOG(("GetUserMediaStreamRunnable::Run: starting failure callback "
             "following InitializeAsync()"));
        // Initiating and starting devices failed.

        // Only run if the window is still active for our window listener.
        if (!(manager->IsWindowStillActive(windowID))) {
          return;
        }
        // This is safe since we're on main-thread, and the windowlist can only
        // be invalidated from the main-thread (see OnNavigation)
        if (auto* window = nsGlobalWindowInner::GetInnerWindowWithId(windowID)) {
          auto streamError = MakeRefPtr<MediaStreamError>(window->AsInner(), *error);
          onFailure->OnError(streamError);
        }
      });

    if (!IsPincipalInfoPrivate(mPrincipalInfo)) {
      // Call GetPrincipalKey again, this time w/persist = true, to promote
      // deviceIds to persistent, in case they're not already. Fire'n'forget.
      RefPtr<Pledge<nsCString>> p =
        media::GetPrincipalKey(mPrincipalInfo, true);
    }
    return NS_OK;
  }

private:
  nsMainThreadPtrHandle<nsIDOMGetUserMediaSuccessCallback> mOnSuccess;
  nsMainThreadPtrHandle<nsIDOMGetUserMediaErrorCallback> mOnFailure;
  MediaStreamConstraints mConstraints;
  RefPtr<MediaDevice> mAudioDevice;
  RefPtr<MediaDevice> mVideoDevice;
  uint64_t mWindowID;
  RefPtr<GetUserMediaWindowListener> mWindowListener;
  RefPtr<SourceListener> mSourceListener;
  ipc::PrincipalInfo mPrincipalInfo;
  RefPtr<PeerIdentity> mPeerIdentity;
  RefPtr<MediaManager> mManager; // get ref to this when creating the runnable
};

// Source getter returning full list

static void
GetMediaDevices(MediaEngine *aEngine,
                uint64_t aWindowId,
                MediaSourceEnum aSrcType,
                nsTArray<RefPtr<MediaDevice>>& aResult,
                const char* aMediaDeviceName = nullptr)
{
  MOZ_ASSERT(MediaManager::IsInMediaThread());

  LOG(("%s: aEngine=%p, aWindowId=%" PRIu64 ", aSrcType=%" PRIu8 ", aMediaDeviceName=%s",
       __func__, aEngine, aWindowId, static_cast<uint8_t>(aSrcType),
       aMediaDeviceName ? aMediaDeviceName : "null"));
  nsTArray<RefPtr<MediaDevice>> devices;
  aEngine->EnumerateDevices(aWindowId, aSrcType, MediaSinkEnum::Other, &devices);

  /*
   * We're allowing multiple tabs to access the same camera for parity
   * with Chrome.  See bug 811757 for some of the issues surrounding
   * this decision.  To disallow, we'd filter by IsAvailable() as we used
   * to.
   */
  if (aMediaDeviceName && *aMediaDeviceName)  {
    for (auto& device : devices) {
      if (device->mName.EqualsASCII(aMediaDeviceName)) {
        aResult.AppendElement(device);
        LOG(("%s: found aMediaDeviceName=%s", __func__, aMediaDeviceName));
        break;
      }
    }
  } else {
    aResult = devices;
    if (MOZ_LOG_TEST(GetMediaManagerLog(), mozilla::LogLevel::Debug)) {
      for (auto& device : devices) {
        LOG(("%s: appending device=%s", __func__,
             NS_ConvertUTF16toUTF8(device->mName).get()));
      }
    }
  }
}

// TODO: Remove once upgraded to GCC 4.8+ on linux. Bogus error on static func:
// error: 'this' was not captured for this lambda function

static auto& MediaManager_ToJSArray = MediaManager::ToJSArray;
static auto& MediaManager_AnonymizeDevices = MediaManager::AnonymizeDevices;

already_AddRefed<MediaManager::PledgeChar>
MediaManager::SelectSettings(
    MediaStreamConstraints& aConstraints,
    bool aIsChrome,
    RefPtr<Refcountable<UniquePtr<MediaDeviceSet>>>& aSources)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<PledgeChar> p = new PledgeChar();
  uint32_t id = mOutstandingCharPledges.Append(*p);

  // Algorithm accesses device capabilities code and must run on media thread.
  // Modifies passed-in aSources.

  MediaManager::PostTask(NewTaskFrom([id, aConstraints,
                                      aSources, aIsChrome]() mutable {
    auto& sources = **aSources;

    // Since the advanced part of the constraints algorithm needs to know when
    // a candidate set is overconstrained (zero members), we must split up the
    // list into videos and audios, and put it back together again at the end.

    nsTArray<RefPtr<MediaDevice>> videos;
    nsTArray<RefPtr<MediaDevice>> audios;

    for (auto& source : sources) {
      MOZ_ASSERT(source->mKind == dom::MediaDeviceKind::Videoinput ||
                 source->mKind == dom::MediaDeviceKind::Audioinput);
      if (source->mKind == dom::MediaDeviceKind::Videoinput) {
        videos.AppendElement(source);
      } else if (source->mKind == dom::MediaDeviceKind::Audioinput) {
        audios.AppendElement(source);
      }
    }
    sources.Clear();
    const char* badConstraint = nullptr;
    bool needVideo = IsOn(aConstraints.mVideo);
    bool needAudio = IsOn(aConstraints.mAudio);

    if (needVideo && videos.Length()) {
      badConstraint = MediaConstraintsHelper::SelectSettings(
          NormalizedConstraints(GetInvariant(aConstraints.mVideo)), videos,
          aIsChrome);
    }
    if (!badConstraint && needAudio && audios.Length()) {
      badConstraint = MediaConstraintsHelper::SelectSettings(
          NormalizedConstraints(GetInvariant(aConstraints.mAudio)), audios,
          aIsChrome);
    }
    if (!badConstraint &&
        !needVideo == !videos.Length() &&
        !needAudio == !audios.Length()) {
      for (auto& video : videos) {
        sources.AppendElement(video);
      }
      for (auto& audio : audios) {
        sources.AppendElement(audio);
      }
    }
    NS_DispatchToMainThread(NewRunnableFrom([id, badConstraint]() mutable {
      MediaManager* mgr = MediaManager::GetIfExists();
      if (!mgr) {
        return NS_OK;
      }
      RefPtr<PledgeChar> p = mgr->mOutstandingCharPledges.Remove(id);
      if (p) {
        p->Resolve(badConstraint);
      }
      return NS_OK;
    }));
  }));
  return p.forget();
}

/**
 * Runs on a seperate thread and is responsible for enumerating devices.
 * Depending on whether a picture or stream was asked for, either
 * ProcessGetUserMedia is called, and the results are sent back to the DOM.
 *
 * Do not run this on the main thread. The success and error callbacks *MUST*
 * be dispatched on the main thread!
 */
class GetUserMediaTask : public Runnable
{
public:
  GetUserMediaTask(
    const MediaStreamConstraints& aConstraints,
    const nsMainThreadPtrHandle<nsIDOMGetUserMediaSuccessCallback>& aOnSuccess,
    const nsMainThreadPtrHandle<nsIDOMGetUserMediaErrorCallback>& aOnFailure,
    uint64_t aWindowID,
    GetUserMediaWindowListener* aWindowListener,
    SourceListener* aSourceListener,
    MediaEnginePrefs& aPrefs,
    const ipc::PrincipalInfo& aPrincipalInfo,
    bool aIsChrome,
    MediaManager::MediaDeviceSet* aMediaDeviceSet,
    bool aShouldFocusSource)
    : Runnable("GetUserMediaTask")
    , mConstraints(aConstraints)
    , mOnSuccess(aOnSuccess)
    , mOnFailure(aOnFailure)
    , mWindowID(aWindowID)
    , mWindowListener(aWindowListener)
    , mSourceListener(aSourceListener)
    , mPrefs(aPrefs)
    , mPrincipalInfo(aPrincipalInfo)
    , mIsChrome(aIsChrome)
    , mShouldFocusSource(aShouldFocusSource)
    , mDeviceChosen(false)
    , mMediaDeviceSet(aMediaDeviceSet)
    , mManager(MediaManager::GetInstance())
  {}

  ~GetUserMediaTask() {
  }

  void
  Fail(MediaMgrError::Name aName,
       const nsAString& aMessage = EmptyString(),
       const nsAString& aConstraint = EmptyString()) {
    RefPtr<MediaMgrError> error = new MediaMgrError(aName, aMessage, aConstraint);
    auto errorRunnable = MakeRefPtr<ErrorCallbackRunnable>(
        mOnFailure, *error, mWindowID);

    NS_DispatchToMainThread(errorRunnable.forget());
    // Do after ErrorCallbackRunnable Run()s, as it checks active window list
    NS_DispatchToMainThread(NewRunnableMethod<RefPtr<SourceListener>>(
      "GetUserMediaWindowListener::Remove",
      mWindowListener,
      &GetUserMediaWindowListener::Remove,
      mSourceListener));
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mOnSuccess);
    MOZ_ASSERT(mOnFailure);
    MOZ_ASSERT(mDeviceChosen);
    LOG(("GetUserMediaTask::Run()"));

    // Allocate a video or audio device and return a MediaStream via
    // a GetUserMediaStreamRunnable.

    nsresult rv;
    const char* errorMsg = nullptr;
    const char* badConstraint = nullptr;

    if (mAudioDevice) {
      auto& constraints = GetInvariant(mConstraints.mAudio);
      rv = mAudioDevice->Allocate(constraints, mPrefs, mPrincipalInfo,
                                  &badConstraint);
      if (NS_FAILED(rv)) {
        errorMsg = "Failed to allocate audiosource";
        if (rv == NS_ERROR_NOT_AVAILABLE && !badConstraint) {
          nsTArray<RefPtr<MediaDevice>> devices;
          devices.AppendElement(mAudioDevice);
          badConstraint = MediaConstraintsHelper::SelectSettings(
              NormalizedConstraints(constraints), devices, mIsChrome);
        }
      }
    }
    if (!errorMsg && mVideoDevice) {
      auto& constraints = GetInvariant(mConstraints.mVideo);
      rv = mVideoDevice->Allocate(constraints, mPrefs, mPrincipalInfo,
                                  &badConstraint);
      if (NS_FAILED(rv)) {
        errorMsg = "Failed to allocate videosource";
        if (rv == NS_ERROR_NOT_AVAILABLE && !badConstraint) {
          nsTArray<RefPtr<MediaDevice>> devices;
          devices.AppendElement(mVideoDevice);
          badConstraint = MediaConstraintsHelper::SelectSettings(
              NormalizedConstraints(constraints), devices, mIsChrome);
        }
        if (mAudioDevice) {
          mAudioDevice->Deallocate();
        }
      } else {
        if (!mIsChrome) {
          if (mShouldFocusSource) {
            rv = mVideoDevice->FocusOnSelectedSource();

            if (NS_FAILED(rv)) {
              LOG(("FocusOnSelectedSource failed"));
            }
          }
        }
      }
    }
    if (errorMsg) {
      LOG(("%s %" PRIu32, errorMsg, static_cast<uint32_t>(rv)));
      if (badConstraint) {
        Fail(MediaMgrError::Name::OverconstrainedError,
             NS_LITERAL_STRING(""),
             NS_ConvertUTF8toUTF16(badConstraint));
      } else {
        Fail(MediaMgrError::Name::NotReadableError,
             NS_ConvertUTF8toUTF16(errorMsg));
      }
      NS_DispatchToMainThread(NS_NewRunnableFunction("MediaManager::SendPendingGUMRequest",
                                                     []() -> void {
        MediaManager* manager = MediaManager::GetIfExists();
        if (!manager) {
          return;
        }
        manager->SendPendingGUMRequest();
      }));
      return NS_OK;
    }
    PeerIdentity* peerIdentity = nullptr;
    if (!mConstraints.mPeerIdentity.IsEmpty()) {
      peerIdentity = new PeerIdentity(mConstraints.mPeerIdentity);
    }

    NS_DispatchToMainThread(do_AddRef(
        new GetUserMediaStreamRunnable(mOnSuccess, mOnFailure, mWindowID,
                                       mWindowListener, mSourceListener,
                                       mPrincipalInfo, mConstraints,
                                       mAudioDevice, mVideoDevice,
                                       peerIdentity, mIsChrome)));
    return NS_OK;
  }

  nsresult
  Denied(MediaMgrError::Name aName,
         const nsAString& aMessage = EmptyString())
  {
    MOZ_ASSERT(mOnSuccess);
    MOZ_ASSERT(mOnFailure);

    // We add a disabled listener to the StreamListeners array until accepted
    // If this was the only active MediaStream, remove the window from the list.
    if (NS_IsMainThread()) {
      if (auto* window = nsGlobalWindowInner::GetInnerWindowWithId(mWindowID)) {
        RefPtr<MediaStreamError> error = new MediaStreamError(window->AsInner(),
                                                              aName, aMessage);
        mOnFailure->OnError(error);
      }
      // Should happen *after* error runs for consistency, but may not matter
      mWindowListener->Remove(mSourceListener);
    } else {
      // This will re-check the window being alive on main-thread
      Fail(aName, aMessage);
    }

    return NS_OK;
  }

  nsresult
  SetContraints(const MediaStreamConstraints& aConstraints)
  {
    mConstraints = aConstraints;
    return NS_OK;
  }

  const MediaStreamConstraints&
  GetConstraints()
  {
    return mConstraints;
  }

  nsresult
  SetAudioDevice(MediaDevice* aAudioDevice)
  {
    mAudioDevice = aAudioDevice;
    mDeviceChosen = true;
    return NS_OK;
  }

  nsresult
  SetVideoDevice(MediaDevice* aVideoDevice)
  {
    mVideoDevice = aVideoDevice;
    mDeviceChosen = true;
    return NS_OK;
  }

  uint64_t
  GetWindowID()
  {
    return mWindowID;
  }

private:
  MediaStreamConstraints mConstraints;

  nsMainThreadPtrHandle<nsIDOMGetUserMediaSuccessCallback> mOnSuccess;
  nsMainThreadPtrHandle<nsIDOMGetUserMediaErrorCallback> mOnFailure;
  uint64_t mWindowID;
  RefPtr<GetUserMediaWindowListener> mWindowListener;
  RefPtr<SourceListener> mSourceListener;
  RefPtr<MediaDevice> mAudioDevice;
  RefPtr<MediaDevice> mVideoDevice;
  MediaEnginePrefs mPrefs;
  ipc::PrincipalInfo mPrincipalInfo;
  bool mIsChrome;
  bool mShouldFocusSource;

  bool mDeviceChosen;
public:
  nsAutoPtr<MediaManager::MediaDeviceSet> mMediaDeviceSet;
private:
  RefPtr<MediaManager> mManager; // get ref to this when creating the runnable
};

#if defined(ANDROID)
class GetUserMediaRunnableWrapper : public Runnable
{
public:
  // This object must take ownership of task
  explicit GetUserMediaRunnableWrapper(GetUserMediaTask* task)
    : Runnable("GetUserMediaRunnableWrapper")
    , mTask(task) {
  }

  ~GetUserMediaRunnableWrapper() {
  }

  NS_IMETHOD Run() override {
    mTask->Run();
    return NS_OK;
  }

private:
  nsAutoPtr<GetUserMediaTask> mTask;
};
#endif

/**
 * EnumerateRawDevices - Enumerate a list of audio & video devices that
 * satisfy passed-in constraints. List contains raw id's.
 */

already_AddRefed<MediaManager::PledgeMediaDeviceSet>
MediaManager::EnumerateRawDevices(uint64_t aWindowId,
                                  MediaSourceEnum aVideoInputType,
                                  MediaSourceEnum aAudioInputType,
                                  MediaSinkEnum aAudioOutputType,
                                  DeviceEnumerationType aVideoInputEnumType /* = DeviceEnumerationType::Normal */,
                                  DeviceEnumerationType aAudioInputEnumType /* = DeviceEnumerationType::Normal */)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aVideoInputType != MediaSourceEnum::Other ||
             aAudioInputType != MediaSourceEnum::Other ||
             aAudioOutputType != MediaSinkEnum::Other);
  // Since the enums can take one of several values, the following asserts rely
  // on short circuting behavior. E.g. aVideoInputEnumType != Fake will be true if
  // the requested device is not fake and thus the assert will pass. However,
  // if the device is fake, aVideoInputType == MediaSourceEnum::Camera will be
  // checked as well, ensuring that fake devices are of the camera type.
  MOZ_ASSERT(aVideoInputEnumType != DeviceEnumerationType::Fake ||
             aVideoInputType == MediaSourceEnum::Camera,
             "If fake cams are requested video type should be camera!");
  MOZ_ASSERT(aVideoInputEnumType != DeviceEnumerationType::Loopback ||
             aVideoInputType == MediaSourceEnum::Camera,
             "If loopback video is requested video type should be camera!");
  MOZ_ASSERT(aAudioInputEnumType != DeviceEnumerationType::Fake ||
             aAudioInputType == MediaSourceEnum::Microphone,
             "If fake mics are requested audio type should be microphone!");
  MOZ_ASSERT(aAudioInputEnumType != DeviceEnumerationType::Loopback ||
             aAudioInputType == MediaSourceEnum::Microphone,
             "If loopback audio is requested audio type should be microphone!");

  LOG(("%s: aWindowId=%" PRIu64 ", aVideoInputType=%" PRIu8 ", aAudioInputType=%" PRIu8
       ", aVideoInputEnumType=%" PRIu8 ", aAudioInputEnumType=%" PRIu8,
       __func__, aWindowId,
       static_cast<uint8_t>(aVideoInputType), static_cast<uint8_t>(aAudioInputType),
       static_cast<uint8_t>(aVideoInputEnumType), static_cast<uint8_t>(aAudioInputEnumType)));
  RefPtr<PledgeMediaDeviceSet> p = new PledgeMediaDeviceSet();
  uint32_t id = mOutstandingPledges.Append(*p);

  bool hasVideo = aVideoInputType != MediaSourceEnum::Other;
  bool hasAudio = aAudioInputType != MediaSourceEnum::Other;
  bool hasAudioOutput = aAudioOutputType == MediaSinkEnum::Speaker;

  // True of at least one of video input or audio input is a fake device
  bool fakeDeviceRequested = (aVideoInputEnumType == DeviceEnumerationType::Fake && hasVideo) ||
                             (aAudioInputEnumType == DeviceEnumerationType::Fake && hasAudio);
  // True if at least one of video input or audio input is a real device
  // or there is audio output.
  bool realDeviceRequested = (aVideoInputEnumType != DeviceEnumerationType::Fake && hasVideo) ||
                             (aAudioInputEnumType != DeviceEnumerationType::Fake && hasAudio) ||
                             hasAudioOutput;

  nsAutoCString videoLoopDev, audioLoopDev;
  if (hasVideo && aVideoInputEnumType == DeviceEnumerationType::Loopback) {
    Preferences::GetCString("media.video_loopback_dev", videoLoopDev);
  }
  if (hasAudio && aAudioInputEnumType == DeviceEnumerationType::Loopback) {
    Preferences::GetCString("media.audio_loopback_dev", audioLoopDev);
  }

  RefPtr<Runnable> task = NewTaskFrom([id, aWindowId, aVideoInputType, aAudioInputType,
                                       aVideoInputEnumType, aAudioInputEnumType,
                                       videoLoopDev, audioLoopDev,
                                       hasVideo, hasAudio, hasAudioOutput,
                                       fakeDeviceRequested, realDeviceRequested]() {
    // Only enumerate what's asked for, and only fake cams and mics.
    RefPtr<MediaEngine> fakeBackend, realBackend;
    if (fakeDeviceRequested) {
      fakeBackend = new MediaEngineDefault();
    }
    if (realDeviceRequested) {
      MediaManager* manager = MediaManager::GetIfExists();
      MOZ_RELEASE_ASSERT(manager); // Must exist while media thread is alive
      realBackend = manager->GetBackend(aWindowId);
    }

    auto result = MakeUnique<MediaDeviceSet>();

    if (hasVideo) {
      MediaDeviceSet videos;
      LOG(("EnumerateRawDevices Task: Getting video sources with %s backend",
           aVideoInputEnumType == DeviceEnumerationType::Fake ? "fake" : "real"));
      GetMediaDevices(aVideoInputEnumType == DeviceEnumerationType::Fake ? fakeBackend : realBackend,
                      aWindowId, aVideoInputType, videos, videoLoopDev.get());
      result->AppendElements(videos);
    }
    if (hasAudio) {
      MediaDeviceSet audios;
      LOG(("EnumerateRawDevices Task: Getting audio sources with %s backend",
           aAudioInputEnumType == DeviceEnumerationType::Fake ? "fake" : "real"));
      GetMediaDevices(aAudioInputEnumType == DeviceEnumerationType::Fake ? fakeBackend : realBackend,
                      aWindowId, aAudioInputType, audios, audioLoopDev.get());
      result->AppendElements(audios);
    }
    if (hasAudioOutput) {
      MediaDeviceSet outputs;
      MOZ_ASSERT(realBackend);
      realBackend->EnumerateDevices(aWindowId,
                                    MediaSourceEnum::Other,
                                    MediaSinkEnum::Speaker,
                                    &outputs);
      result->AppendElements(outputs);
    }

    NS_DispatchToMainThread(NewRunnableFrom([id, result = std::move(result)]() mutable {
      MediaManager* mgr = MediaManager::GetIfExists();
      if (!mgr) {
        return NS_OK;
      }
      RefPtr<PledgeMediaDeviceSet> p = mgr->mOutstandingPledges.Remove(id);
      if (p) {
        p->Resolve(result.release());
      }
      return NS_OK;
    }));
  });

  if (realDeviceRequested &&
      Preferences::GetBool("media.navigator.permission.device", false)) {
    // Need to ask permission to retrieve list of all devices;
    // notify frontend observer and wait for callback notification to post task.
    const char16_t* const type =
      (aVideoInputType != MediaSourceEnum::Camera)     ? u"audio" :
      (aAudioInputType != MediaSourceEnum::Microphone) ? u"video" :
                                                         u"all";
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    obs->NotifyObservers(static_cast<nsIRunnable*>(task),
                         "getUserMedia:ask-device-permission",
                         type);
  } else {
    // Don't need to ask permission to retrieve list of all devices;
    // post the retrieval task immediately.
    MediaManager::PostTask(task.forget());
  }

  return p.forget();
}

MediaManager::MediaManager()
  : mMediaThread(nullptr)
  , mBackend(nullptr) {
  mPrefs.mFreq         = 1000; // 1KHz test tone
  mPrefs.mWidth        = 0; // adaptive default
  mPrefs.mHeight       = 0; // adaptive default
  mPrefs.mFPS          = MediaEnginePrefs::DEFAULT_VIDEO_FPS;
  mPrefs.mAecOn        = false;
  mPrefs.mAgcOn        = false;
  mPrefs.mNoiseOn      = false;
  mPrefs.mExtendedFilter = true;
  mPrefs.mDelayAgnostic = true;
  mPrefs.mFakeDeviceChangeEventOn = false;
#ifdef MOZ_WEBRTC
  mPrefs.mAec          = webrtc::kEcUnchanged;
  mPrefs.mAgc          = webrtc::kAgcUnchanged;
  mPrefs.mNoise        = webrtc::kNsUnchanged;
#else
  mPrefs.mAec          = 0;
  mPrefs.mAgc          = 0;
  mPrefs.mNoise        = 0;
#endif
  mPrefs.mFullDuplex = false;
  mPrefs.mChannels     = 0; // max channels default
  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);
    if (branch) {
      GetPrefs(branch, nullptr);
    }
  }
  LOG(("%s: default prefs: %dx%d @%dfps, %dHz test tones, aec: %s,"
       "agc: %s, noise: %s, aec level: %d, agc level: %d, noise level: %d,"
       "%sfull_duplex, extended aec %s, delay_agnostic %s "
       "channels %d",
       __FUNCTION__, mPrefs.mWidth, mPrefs.mHeight,
       mPrefs.mFPS, mPrefs.mFreq, mPrefs.mAecOn ? "on" : "off",
       mPrefs.mAgcOn ? "on": "off", mPrefs.mNoiseOn ? "on": "off", mPrefs.mAec,
       mPrefs.mAgc, mPrefs.mNoise, mPrefs.mFullDuplex ? "" : "not ",
       mPrefs.mExtendedFilter ? "on" : "off", mPrefs.mDelayAgnostic ? "on" : "off",
       mPrefs.mChannels));
}

NS_IMPL_ISUPPORTS(MediaManager, nsIMediaManagerService, nsIObserver)

/* static */ StaticRefPtr<MediaManager> MediaManager::sSingleton;

#ifdef DEBUG
/* static */ bool
MediaManager::IsInMediaThread()
{
  return sSingleton?
      (sSingleton->mMediaThread->thread_id() == PlatformThread::CurrentId()) :
      false;
}
#endif

#ifdef XP_WIN
class MTAThread : public base::Thread {
public:
  explicit MTAThread(const char* aName)
    : base::Thread(aName)
    , mResult(E_FAIL)
  {
  }

protected:
  virtual void Init() override {
    mResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  }

  virtual void CleanUp() override {
    if (SUCCEEDED(mResult)) {
      CoUninitialize();
    }
  }

private:
  HRESULT mResult;
};
#endif

// NOTE: never Dispatch(....,NS_DISPATCH_SYNC) to the MediaManager
// thread from the MainThread, as we NS_DISPATCH_SYNC to MainThread
// from MediaManager thread.

// Guaranteed never to return nullptr.
/* static */  MediaManager*
MediaManager::Get() {
  if (!sSingleton) {
    MOZ_ASSERT(NS_IsMainThread());

    static int timesCreated = 0;
    timesCreated++;
    MOZ_RELEASE_ASSERT(timesCreated == 1);

    sSingleton = new MediaManager();

#ifdef XP_WIN
    sSingleton->mMediaThread = new MTAThread("MediaManager");
#else
    sSingleton->mMediaThread = new base::Thread("MediaManager");
#endif
    base::Thread::Options options;
#if defined(_WIN32)
    options.message_loop_type = MessageLoop::TYPE_MOZILLA_NONMAINUITHREAD;
#else
    options.message_loop_type = MessageLoop::TYPE_MOZILLA_NONMAINTHREAD;
#endif
    if (!sSingleton->mMediaThread->StartWithOptions(options)) {
      MOZ_CRASH();
    }

    LOG(("New Media thread for gum"));

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->AddObserver(sSingleton, "last-pb-context-exited", false);
      obs->AddObserver(sSingleton, "getUserMedia:got-device-permission", false);
      obs->AddObserver(sSingleton, "getUserMedia:privileged:allow", false);
      obs->AddObserver(sSingleton, "getUserMedia:response:allow", false);
      obs->AddObserver(sSingleton, "getUserMedia:response:deny", false);
      obs->AddObserver(sSingleton, "getUserMedia:revoke", false);
    }
    // else MediaManager won't work properly and will leak (see bug 837874)
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs) {
      prefs->AddObserver("media.navigator.video.default_width", sSingleton, false);
      prefs->AddObserver("media.navigator.video.default_height", sSingleton, false);
      prefs->AddObserver("media.navigator.video.default_fps", sSingleton, false);
      prefs->AddObserver("media.navigator.audio.fake_frequency", sSingleton, false);
      prefs->AddObserver("media.navigator.audio.full_duplex", sSingleton, false);
#ifdef MOZ_WEBRTC
      prefs->AddObserver("media.getusermedia.aec_enabled", sSingleton, false);
      prefs->AddObserver("media.getusermedia.aec", sSingleton, false);
      prefs->AddObserver("media.getusermedia.agc_enabled", sSingleton, false);
      prefs->AddObserver("media.getusermedia.agc", sSingleton, false);
      prefs->AddObserver("media.getusermedia.noise_enabled", sSingleton, false);
      prefs->AddObserver("media.getusermedia.noise", sSingleton, false);
      prefs->AddObserver("media.ondevicechange.fakeDeviceChangeEvent.enabled", sSingleton, false);
      prefs->AddObserver("media.getusermedia.channels", sSingleton, false);
#endif
    }

    // Prepare async shutdown

    nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase = GetShutdownPhase();

    class Blocker : public media::ShutdownBlocker
    {
    public:
      Blocker()
      : media::ShutdownBlocker(NS_LITERAL_STRING(
          "Media shutdown: blocking on media thread")) {}

      NS_IMETHOD BlockShutdown(nsIAsyncShutdownClient*) override
      {
        MOZ_RELEASE_ASSERT(MediaManager::GetIfExists());
        MediaManager::GetIfExists()->Shutdown();
        return NS_OK;
      }
    };

    sSingleton->mShutdownBlocker = new Blocker();
    nsresult rv = shutdownPhase->AddBlocker(sSingleton->mShutdownBlocker,
                                            NS_LITERAL_STRING(__FILE__),
                                            __LINE__,
                                            NS_LITERAL_STRING("Media shutdown"));
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  }
  return sSingleton;
}

/* static */  MediaManager*
MediaManager::GetIfExists() {
  return sSingleton;
}

/* static */ already_AddRefed<MediaManager>
MediaManager::GetInstance()
{
  // so we can have non-refcounted getters
  RefPtr<MediaManager> service = MediaManager::Get();
  return service.forget();
}

media::Parent<media::NonE10s>*
MediaManager::GetNonE10sParent()
{
  if (!mNonE10sParent) {
    mNonE10sParent = new media::Parent<media::NonE10s>();
  }
  return mNonE10sParent;
}

/* static */ void
MediaManager::StartupInit()
{
#ifdef WIN32
  if (!IsWin8OrLater()) {
    // Bug 1107702 - Older Windows fail in GetAdaptersInfo (and others) if the
    // first(?) call occurs after the process size is over 2GB (kb/2588507).
    // Attempt to 'prime' the pump by making a call at startup.
    unsigned long out_buf_len = sizeof(IP_ADAPTER_INFO);
    PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO *) moz_xmalloc(out_buf_len);
    if (GetAdaptersInfo(pAdapterInfo, &out_buf_len) == ERROR_BUFFER_OVERFLOW) {
      free(pAdapterInfo);
      pAdapterInfo = (IP_ADAPTER_INFO *) moz_xmalloc(out_buf_len);
      GetAdaptersInfo(pAdapterInfo, &out_buf_len);
    }
    if (pAdapterInfo) {
      free(pAdapterInfo);
    }
  }
#endif
}

/* static */
void
MediaManager::PostTask(already_AddRefed<Runnable> task)
{
  if (sHasShutdown) {
    // Can't safely delete task here since it may have items with specific
    // thread-release requirements.
    // XXXkhuey well then who is supposed to delete it?! We don't signal
    // that we failed ...
    MOZ_CRASH();
    return;
  }
  NS_ASSERTION(Get(), "MediaManager singleton?");
  NS_ASSERTION(Get()->mMediaThread, "No thread yet");
  Get()->mMediaThread->message_loop()->PostTask(std::move(task));
}

template<typename MozPromiseType, typename FunctionType>
/* static */ RefPtr<MozPromiseType>
MediaManager::PostTask(const char* aName, FunctionType&& aFunction)
{
  MozPromiseHolder<MozPromiseType> holder;
  RefPtr<MozPromiseType> promise = holder.Ensure(aName);
  MediaManager::PostTask(NS_NewRunnableFunction(aName,
        [h = std::move(holder), func = std::forward<FunctionType>(aFunction)]() mutable
        {
          func(h);
        }));
  return promise;
}

/* static */ nsresult
MediaManager::NotifyRecordingStatusChange(nsPIDOMWindowInner* aWindow)
{
  NS_ENSURE_ARG(aWindow);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    NS_WARNING("Could not get the Observer service for GetUserMedia recording notification.");
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();

  nsCString pageURL;
  nsCOMPtr<nsIURI> docURI = aWindow->GetDocumentURI();
  NS_ENSURE_TRUE(docURI, NS_ERROR_FAILURE);

  nsresult rv = docURI->GetSpec(pageURL);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ConvertUTF8toUTF16 requestURL(pageURL);

  props->SetPropertyAsAString(NS_LITERAL_STRING("requestURL"), requestURL);

  obs->NotifyObservers(static_cast<nsIPropertyBag2*>(props),
                       "recording-device-events",
                       nullptr);

  return NS_OK;
}

int MediaManager::AddDeviceChangeCallback(DeviceChangeCallback* aCallback)
{
  bool fakeDeviceChangeEventOn = mPrefs.mFakeDeviceChangeEventOn;
  MediaManager::PostTask(NewTaskFrom([fakeDeviceChangeEventOn]() {
    MediaManager* manager = MediaManager::GetIfExists();
    MOZ_RELEASE_ASSERT(manager); // Must exist while media thread is alive
    // this is needed in case persistent permission is given but no gUM()
    // or enumeration() has created the real backend yet
    manager->GetBackend(0);
    if (fakeDeviceChangeEventOn)
      manager->GetBackend(0)->SetFakeDeviceChangeEvents();
  }));

  return DeviceChangeCallback::AddDeviceChangeCallback(aCallback);
}

void MediaManager::OnDeviceChange() {
  RefPtr<MediaManager> self(this);
  NS_DispatchToMainThread(media::NewRunnableFrom([self]() mutable {
    MOZ_ASSERT(NS_IsMainThread());
    if (sHasShutdown) {
      return NS_OK;
    }
    self->DeviceChangeCallback::OnDeviceChange();

    // On some Windows machine, if we call EnumerateRawDevices immediately after receiving
    // devicechange event, sometimes we would get outdated devices list.
    PR_Sleep(PR_MillisecondsToInterval(100));
    RefPtr<PledgeMediaDeviceSet> p = self->EnumerateRawDevices(0,
                                                               MediaSourceEnum::Camera,
                                                               MediaSourceEnum::Microphone,
                                                               MediaSinkEnum::Speaker);
    p->Then([self](MediaDeviceSet*& aDevices) mutable {
      UniquePtr<MediaDeviceSet> devices(aDevices);
      nsTArray<nsString> deviceIDs;

      for (auto& device : *devices) {
        nsString id;
        device->GetId(id);
        id.ReplaceSubstring(NS_LITERAL_STRING("default: "), NS_LITERAL_STRING(""));
        if (!deviceIDs.Contains(id)) {
          deviceIDs.AppendElement(id);
        }
      }

      for (auto& id : self->mDeviceIDs) {
        if (deviceIDs.Contains(id)) {
          continue;
        }

        // Stop the coresponding SourceListener
        nsGlobalWindowInner::InnerWindowByIdTable* windowsById =
          nsGlobalWindowInner::GetWindowsTable();
        if (!windowsById) {
          continue;
        }

        for (auto iter = windowsById->Iter(); !iter.Done(); iter.Next()) {
          nsGlobalWindowInner* window = iter.Data();
          self->IterateWindowListeners(window->AsInner(),
              [&id](GetUserMediaWindowListener* aListener)
              {
                aListener->StopRawID(id);
              });
        }
      }

      self->mDeviceIDs = deviceIDs;
    }, [](MediaStreamError*& reason) {});
    return NS_OK;
  }));
}

nsresult MediaManager::GenerateUUID(nsAString& aResult)
{
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
      do_GetService("@mozilla.org/uuid-generator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Generate a call ID.
  nsID id;
  rv = uuidgen->GenerateUUIDInPlace(&id);
  NS_ENSURE_SUCCESS(rv, rv);

  char buffer[NSID_LENGTH];
  id.ToProvidedString(buffer);
  aResult.Assign(NS_ConvertUTF8toUTF16(buffer));
  return NS_OK;
}

static bool IsFullyActive(nsPIDOMWindowInner* aWindow)
{
  while (true) {
    if (!aWindow) {
      return false;
    }
    nsIDocument* document = aWindow->GetExtantDoc();
    if (!document) {
      return false;
    }
    if (!document->IsCurrentActiveDocument()) {
      return false;
    }
    nsPIDOMWindowOuter* context = aWindow->GetOuterWindow();
    if (!context) {
      return false;
    }
    if (context->IsTopLevelWindow()) {
      return true;
    }
    nsCOMPtr<Element> frameElement =
      nsGlobalWindowOuter::Cast(context)->GetRealFrameElementOuter();
    if (!frameElement) {
      return false;
    }
    aWindow = frameElement->OwnerDoc()->GetInnerWindow();
  }
}

enum class GetUserMediaSecurityState {
  Other = 0,
  HTTPS = 1,
  File = 2,
  App = 3,
  Localhost = 4,
  Loop = 5,
  Privileged = 6
};

/**
 * This function is used in getUserMedia when privacy.resistFingerprinting is true.
 * Only mediaSource of audio/video constraint will be kept.
 */
static void
ReduceConstraint(
    mozilla::dom::OwningBooleanOrMediaTrackConstraints& aConstraint) {
  // Not requesting stream.
  if (!IsOn(aConstraint)) {
    return;
  }

  // It looks like {audio: true}, do nothing.
  if (!aConstraint.IsMediaTrackConstraints()) {
    return;
  }

  // Keep mediaSource, ignore all other constraints.
  auto& c = aConstraint.GetAsMediaTrackConstraints();
  nsString mediaSource = c.mMediaSource;
  aConstraint.SetAsMediaTrackConstraints().mMediaSource = mediaSource;
}

/**
 * The entry point for this file. A call from Navigator::mozGetUserMedia
 * will end up here. MediaManager is a singleton that is responsible
 * for handling all incoming getUserMedia calls from every window.
 */
nsresult
MediaManager::GetUserMedia(nsPIDOMWindowInner* aWindow,
                           const MediaStreamConstraints& aConstraintsPassedIn,
                           nsIDOMGetUserMediaSuccessCallback* aOnSuccess,
                           nsIDOMGetUserMediaErrorCallback* aOnFailure,
                           dom::CallerType aCallerType)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aOnFailure);
  MOZ_ASSERT(aOnSuccess);
  nsMainThreadPtrHandle<nsIDOMGetUserMediaSuccessCallback> onSuccess(
      new nsMainThreadPtrHolder<nsIDOMGetUserMediaSuccessCallback>(
          "GetUserMedia::SuccessCallback", aOnSuccess));
  nsMainThreadPtrHandle<nsIDOMGetUserMediaErrorCallback> onFailure(
      new nsMainThreadPtrHolder<nsIDOMGetUserMediaErrorCallback>(
          "GetUserMedia::FailureCallback", aOnFailure));
  uint64_t windowID = aWindow->WindowID();

  MediaStreamConstraints c(aConstraintsPassedIn); // use a modifiable copy

  // Do all the validation we can while we're sync (to return an
  // already-rejected promise on failure).

  if (!IsOn(c.mVideo) && !IsOn(c.mAudio)) {
    RefPtr<MediaStreamError> error =
        new MediaStreamError(aWindow,
                             MediaStreamError::Name::TypeError,
                             NS_LITERAL_STRING("audio and/or video is required"));
    onFailure->OnError(error);
    return NS_OK;
  }

  if (!IsFullyActive(aWindow)) {
    RefPtr<MediaStreamError> error =
        new MediaStreamError(aWindow, MediaStreamError::Name::InvalidStateError);
    onFailure->OnError(error);
    return NS_OK;
  }

  if (sHasShutdown) {
    RefPtr<MediaStreamError> error =
        new MediaStreamError(aWindow,
                             MediaStreamError::Name::AbortError,
                             NS_LITERAL_STRING("In shutdown"));
    onFailure->OnError(error);
    return NS_OK;
  }

  // Determine permissions early (while we still have a stack).

  nsIURI* docURI = aWindow->GetDocumentURI();
  if (!docURI) {
    return NS_ERROR_UNEXPECTED;
  }
  bool isChrome = (aCallerType == dom::CallerType::System);
  bool privileged = isChrome ||
      Preferences::GetBool("media.navigator.permission.disabled", false);
  bool isHTTPS = false;
  bool isHandlingUserInput = EventStateManager::IsHandlingUserInput();;
  docURI->SchemeIs("https", &isHTTPS);
  nsCString host;
  nsresult rv = docURI->GetHost(host);
  // Test for some other schemes that ServiceWorker recognizes
  bool isFile;
  docURI->SchemeIs("file", &isFile);
  bool isApp;
  docURI->SchemeIs("app", &isApp);
  // Same localhost check as ServiceWorkers uses
  // (see IsOriginPotentiallyTrustworthy())
  bool isLocalhost = NS_SUCCEEDED(rv) &&
                     (host.LowerCaseEqualsLiteral("localhost") ||
                      host.LowerCaseEqualsLiteral("127.0.0.1") ||
                      host.LowerCaseEqualsLiteral("::1"));

  // Record telemetry about whether the source of the call was secure, i.e.,
  // privileged or HTTPS.  We may handle other cases
  if (privileged) {
    Telemetry::Accumulate(Telemetry::WEBRTC_GET_USER_MEDIA_SECURE_ORIGIN,
                          (uint32_t) GetUserMediaSecurityState::Privileged);
  } else if (isHTTPS) {
    Telemetry::Accumulate(Telemetry::WEBRTC_GET_USER_MEDIA_SECURE_ORIGIN,
                          (uint32_t) GetUserMediaSecurityState::HTTPS);
  } else if (isFile) {
    Telemetry::Accumulate(Telemetry::WEBRTC_GET_USER_MEDIA_SECURE_ORIGIN,
                          (uint32_t) GetUserMediaSecurityState::File);
  } else if (isApp) {
    Telemetry::Accumulate(Telemetry::WEBRTC_GET_USER_MEDIA_SECURE_ORIGIN,
                          (uint32_t) GetUserMediaSecurityState::App);
  } else if (isLocalhost) {
    Telemetry::Accumulate(Telemetry::WEBRTC_GET_USER_MEDIA_SECURE_ORIGIN,
                          (uint32_t) GetUserMediaSecurityState::Localhost);
  } else {
    Telemetry::Accumulate(Telemetry::WEBRTC_GET_USER_MEDIA_SECURE_ORIGIN,
                          (uint32_t) GetUserMediaSecurityState::Other);
  }

  nsCOMPtr<nsIPrincipal> principal =
    nsGlobalWindowInner::Cast(aWindow)->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    return NS_ERROR_FAILURE;
  }

  // This principal needs to be sent to different threads and so via IPC.
  // For this reason it's better to convert it to PrincipalInfo right now.
  ipc::PrincipalInfo principalInfo;
  rv = PrincipalToPrincipalInfo(principal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  const bool resistFingerprinting = nsContentUtils::ResistFingerprinting(aCallerType);

  if (resistFingerprinting) {
    ReduceConstraint(c.mVideo);
    ReduceConstraint(c.mAudio);
  }

  if (!Preferences::GetBool("media.navigator.video.enabled", true)) {
    c.mVideo.SetAsBoolean() = false;
  }

  MediaSourceEnum videoType = MediaSourceEnum::Other; // none
  MediaSourceEnum audioType = MediaSourceEnum::Other; // none

  if (c.mVideo.IsMediaTrackConstraints()) {
    auto& vc = c.mVideo.GetAsMediaTrackConstraints();
    videoType = StringToEnum(dom::MediaSourceEnumValues::strings,
                             vc.mMediaSource,
                             MediaSourceEnum::Other);
    Telemetry::Accumulate(Telemetry::WEBRTC_GET_USER_MEDIA_TYPE,
                          (uint32_t) videoType);
    switch (videoType) {
      case MediaSourceEnum::Camera:
        break;

      case MediaSourceEnum::Browser:
        // If no window id is passed in then default to the caller's window.
        // Functional defaults are helpful in tests, but also a natural outcome
        // of the constraints API's limited semantics for requiring input.
        if (!vc.mBrowserWindow.WasPassed()) {
          nsPIDOMWindowOuter* outer = aWindow->GetOuterWindow();
          vc.mBrowserWindow.Construct(outer->WindowID());
        }
        MOZ_FALLTHROUGH;
      case MediaSourceEnum::Screen:
      case MediaSourceEnum::Application:
      case MediaSourceEnum::Window:
        // Deny screensharing request if support is disabled, or
        // the requesting document is not from a host on the whitelist.
        if (!Preferences::GetBool(((videoType == MediaSourceEnum::Browser)?
                                   "media.getusermedia.browser.enabled" :
                                   "media.getusermedia.screensharing.enabled"),
                                  false) ||
            (!privileged && !aWindow->IsSecureContext())) {
          RefPtr<MediaStreamError> error =
              new MediaStreamError(aWindow,
                                   MediaStreamError::Name::NotAllowedError);
          onFailure->OnError(error);
          return NS_OK;
        }
        break;

      case MediaSourceEnum::Microphone:
      case MediaSourceEnum::Other:
      default: {
        RefPtr<MediaStreamError> error =
            new MediaStreamError(aWindow,
                                 MediaStreamError::Name::OverconstrainedError,
                                 NS_LITERAL_STRING(""),
                                 NS_LITERAL_STRING("mediaSource"));
        onFailure->OnError(error);
        return NS_OK;
      }
    }

    if (vc.mAdvanced.WasPassed() && videoType != MediaSourceEnum::Camera) {
      // iterate through advanced, forcing all unset mediaSources to match "root"
      const char *unset = EnumToASCII(dom::MediaSourceEnumValues::strings,
                                      MediaSourceEnum::Camera);
      for (MediaTrackConstraintSet& cs : vc.mAdvanced.Value()) {
        if (cs.mMediaSource.EqualsASCII(unset)) {
          cs.mMediaSource = vc.mMediaSource;
        }
      }
    }
    if (!privileged) {
      // only allow privileged content to set the window id
      if (vc.mBrowserWindow.WasPassed()) {
        vc.mBrowserWindow.Value() = -1;
      }
      if (vc.mAdvanced.WasPassed()) {
        for (MediaTrackConstraintSet& cs : vc.mAdvanced.Value()) {
          if (cs.mBrowserWindow.WasPassed()) {
            cs.mBrowserWindow.Value() = -1;
          }
        }
      }
    }
  } else if (IsOn(c.mVideo)) {
    videoType = MediaSourceEnum::Camera;
    Telemetry::Accumulate(Telemetry::WEBRTC_GET_USER_MEDIA_TYPE,
                          (uint32_t) videoType);
  }

  if (c.mAudio.IsMediaTrackConstraints()) {
    auto& ac = c.mAudio.GetAsMediaTrackConstraints();
    MediaConstraintsHelper::ConvertOldWithWarning(ac.mMozAutoGainControl,
                                                  ac.mAutoGainControl,
                                                  "MozAutoGainControlWarning",
                                                  aWindow);
    MediaConstraintsHelper::ConvertOldWithWarning(ac.mMozNoiseSuppression,
                                                  ac.mNoiseSuppression,
                                                  "MozNoiseSuppressionWarning",
                                                  aWindow);
    audioType = StringToEnum(dom::MediaSourceEnumValues::strings,
                             ac.mMediaSource,
                             MediaSourceEnum::Other);
    // Work around WebIDL default since spec uses same dictionary w/audio & video.
    if (audioType == MediaSourceEnum::Camera) {
      audioType = MediaSourceEnum::Microphone;
      ac.mMediaSource.AssignASCII(EnumToASCII(dom::MediaSourceEnumValues::strings,
                                              audioType));
    }
    Telemetry::Accumulate(Telemetry::WEBRTC_GET_USER_MEDIA_TYPE,
                          (uint32_t) audioType);

    switch (audioType) {
      case MediaSourceEnum::Microphone:
        break;

      case MediaSourceEnum::AudioCapture:
        // Only enable AudioCapture if the pref is enabled. If it's not, we can
        // deny right away.
        if (!Preferences::GetBool("media.getusermedia.audiocapture.enabled")) {
          RefPtr<MediaStreamError> error =
            new MediaStreamError(aWindow,
                                 MediaStreamError::Name::NotAllowedError);
          onFailure->OnError(error);
          return NS_OK;
        }
        break;

      case MediaSourceEnum::Other:
      default: {
        RefPtr<MediaStreamError> error =
            new MediaStreamError(aWindow,
                                 MediaStreamError::Name::OverconstrainedError,
                                 NS_LITERAL_STRING(""),
                                 NS_LITERAL_STRING("mediaSource"));
        onFailure->OnError(error);
        return NS_OK;
      }
    }
    if (ac.mAdvanced.WasPassed()) {
      // iterate through advanced, forcing all unset mediaSources to match "root"
      const char *unset = EnumToASCII(dom::MediaSourceEnumValues::strings,
                                      MediaSourceEnum::Camera);
      for (MediaTrackConstraintSet& cs : ac.mAdvanced.Value()) {
        if (cs.mMediaSource.EqualsASCII(unset)) {
          cs.mMediaSource = ac.mMediaSource;
        }
      }
    }
  } else if (IsOn(c.mAudio)) {
    audioType = MediaSourceEnum::Microphone;
    Telemetry::Accumulate(Telemetry::WEBRTC_GET_USER_MEDIA_TYPE,
                          (uint32_t) audioType);
  }

  // Create a window listener if it doesn't already exist.
  RefPtr<GetUserMediaWindowListener> windowListener =
    GetWindowListener(windowID);
  if (windowListener) {
    PrincipalHandle existingPrincipalHandle = windowListener->GetPrincipalHandle();
    MOZ_ASSERT(PrincipalHandleMatches(existingPrincipalHandle, principal));
  } else {
    windowListener = new GetUserMediaWindowListener(mMediaThread, windowID,
                                                    MakePrincipalHandle(principal));
    AddWindowID(windowID, windowListener);
  }

  RefPtr<SourceListener> sourceListener = new SourceListener();
  windowListener->Register(sourceListener);

  if (!privileged) {
    // Check if this site has had persistent permissions denied.
    nsCOMPtr<nsIPermissionManager> permManager =
      do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t audioPerm = nsIPermissionManager::UNKNOWN_ACTION;
    if (IsOn(c.mAudio)) {
      if (audioType == MediaSourceEnum::Microphone &&
          Preferences::GetBool("media.getusermedia.microphone.deny", false)) {
        audioPerm = nsIPermissionManager::DENY_ACTION;
      } else {
        rv = permManager->TestExactPermissionFromPrincipal(
          principal, "microphone", &audioPerm);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    uint32_t videoPerm = nsIPermissionManager::UNKNOWN_ACTION;
    if (IsOn(c.mVideo)) {
      if (videoType == MediaSourceEnum::Camera &&
          Preferences::GetBool("media.getusermedia.camera.deny", false)) {
        videoPerm = nsIPermissionManager::DENY_ACTION;
      } else {
        rv = permManager->TestExactPermissionFromPrincipal(
          principal, videoType == MediaSourceEnum::Camera ? "camera" : "screen",
          &videoPerm);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    if ((!IsOn(c.mAudio) && !IsOn(c.mVideo)) ||
        (IsOn(c.mAudio) && audioPerm == nsIPermissionManager::DENY_ACTION) ||
        (IsOn(c.mVideo) && videoPerm == nsIPermissionManager::DENY_ACTION)) {
      RefPtr<MediaStreamError> error =
          new MediaStreamError(aWindow, MediaStreamError::Name::NotAllowedError);
      onFailure->OnError(error);
      windowListener->Remove(sourceListener);
      return NS_OK;
    }
  }

  // Get list of all devices, with origin-specific device ids.

  MediaEnginePrefs prefs = mPrefs;

  nsString callID;
  rv = GenerateUUID(callID);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasVideo = videoType != MediaSourceEnum::Other;
  bool hasAudio = audioType != MediaSourceEnum::Other;
  DeviceEnumerationType videoEnumerationType = DeviceEnumerationType::Normal;
  DeviceEnumerationType audioEnumerationType = DeviceEnumerationType::Normal;

  // Handle loopback and fake requests. For gUM we don't consider resist
  // fingerprinting as users should be prompted anyway.
  bool wantFakes = c.mFake.WasPassed() ? c.mFake.Value() :
    Preferences::GetBool("media.navigator.streams.fake");
  nsAutoCString videoLoopDev, audioLoopDev;
  // Video
  if (videoType == MediaSourceEnum::Camera) {
    Preferences::GetCString("media.video_loopback_dev", videoLoopDev);
    // Loopback prefs take precedence over fake prefs
    if (!videoLoopDev.IsEmpty()) {
      videoEnumerationType = DeviceEnumerationType::Loopback;
    } else if (wantFakes) {
      videoEnumerationType = DeviceEnumerationType::Fake;
    }
  }
  // Audio
  if (audioType == MediaSourceEnum::Microphone) {
    Preferences::GetCString("media.audio_loopback_dev", audioLoopDev);
    // Loopback prefs take precedence over fake prefs
    if (!audioLoopDev.IsEmpty()) {
      audioEnumerationType = DeviceEnumerationType::Loopback;
    } else if (wantFakes) {
      audioEnumerationType = DeviceEnumerationType::Fake;
    }
  }

  bool realDevicesRequested = (videoEnumerationType != DeviceEnumerationType::Fake && hasVideo) ||
                              (audioEnumerationType != DeviceEnumerationType::Fake && hasAudio);
  bool askPermission =
    (!privileged || Preferences::GetBool("media.navigator.permission.force")) &&
    (realDevicesRequested || Preferences::GetBool("media.navigator.permission.fake"));

  LOG(("%s: Preparing to enumerate devices. windowId=%" PRIu64
       ", videoType=%" PRIu8 ", audioType=%" PRIu8
       ", videoEnumerationType=%" PRIu8 ", audioEnumerationType=%" PRIu8
       ", askPermission=%s",
       __func__, windowID,
       static_cast<uint8_t>(videoType), static_cast<uint8_t>(audioType),
       static_cast<uint8_t>(videoEnumerationType), static_cast<uint8_t>(audioEnumerationType),
       askPermission ? "true" : "false"));

  RefPtr<PledgeMediaDeviceSet> p = EnumerateDevicesImpl(windowID,
                                                        videoType,
                                                        audioType,
                                                        MediaSinkEnum::Other,
                                                        videoEnumerationType,
                                                        audioEnumerationType);
  RefPtr<MediaManager> self = this;
  p->Then([self, onSuccess, onFailure, windowID, c, windowListener,
           sourceListener, askPermission, prefs, isHTTPS, isHandlingUserInput,
           callID, principalInfo, isChrome, resistFingerprinting](MediaDeviceSet*& aDevices) mutable {
    LOG(("GetUserMedia: post enumeration pledge success callback starting"));
    // grab result
    auto devices = MakeRefPtr<Refcountable<UniquePtr<MediaDeviceSet>>>(aDevices);

    // Ensure that our windowID is still good.
    if (!nsGlobalWindowInner::GetInnerWindowWithId(windowID)) {
      LOG(("GetUserMedia: bad windowID found in post enumeration pledge "
           " success callback! Bailing out!"));
      return;
    }

    // Apply any constraints. This modifies the passed-in list.
    RefPtr<PledgeChar> p2 = self->SelectSettings(c, isChrome, devices);

    p2->Then([self, onSuccess, onFailure, windowID, c,
              windowListener, sourceListener, askPermission, prefs, isHTTPS,
              isHandlingUserInput, callID, principalInfo, isChrome, devices,
              resistFingerprinting
             ](const char*& badConstraint) mutable {
      LOG(("GetUserMedia: starting post enumeration pledge2 success "
           "callback!"));

      // Ensure that the captured 'this' pointer and our windowID are still good.
      auto* globalWindow = nsGlobalWindowInner::GetInnerWindowWithId(windowID);
      RefPtr<nsPIDOMWindowInner> window = globalWindow ? globalWindow->AsInner()
                                                       : nullptr;
      if (!MediaManager::Exists() || !window) {
        return;
      }

      if (badConstraint) {
        LOG(("GetUserMedia: bad constraint found in post enumeration pledge2 "
             "success callback! Calling error handler!"));
        nsString constraint;
        constraint.AssignASCII(badConstraint);
        RefPtr<MediaStreamError> error =
            new MediaStreamError(window,
                                 MediaStreamError::Name::OverconstrainedError,
                                 NS_LITERAL_STRING(""),
                                 constraint);
        onFailure->OnError(error);
        return;
      }
      if (!(*devices)->Length()) {
        LOG(("GetUserMedia: no devices found in post enumeration pledge2 "
             "success callback! Calling error handler!"));
        RefPtr<MediaStreamError> error =
            new MediaStreamError(
                window,
                // When privacy.resistFingerprinting = true, no available
                // device implies content script is requesting a fake
                // device, so report NotAllowedError.
                resistFingerprinting ? MediaStreamError::Name::NotAllowedError
                                     : MediaStreamError::Name::NotFoundError);
        onFailure->OnError(error);
        return;
      }

      nsCOMPtr<nsIMutableArray> devicesCopy = nsArray::Create(); // before we give up devices below
      if (!askPermission) {
        for (auto& device : **devices) {
          nsresult rv = devicesCopy->AppendElement(device);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return;
          }
        }
      }

      bool focusSource;
      focusSource = mozilla::Preferences::GetBool("media.getusermedia.window.focus_source.enabled", true);

      // Pass callbacks and listeners along to GetUserMediaTask.
      RefPtr<GetUserMediaTask> task (new GetUserMediaTask(c,
                                                          onSuccess,
                                                          onFailure,
                                                          windowID,
                                                          windowListener,
                                                          sourceListener,
                                                          prefs,
                                                          principalInfo,
                                                          isChrome,
                                                          devices->release(),
                                                          focusSource));
      // Store the task w/callbacks.
      self->mActiveCallbacks.Put(callID, task.forget());

      // Add a WindowID cross-reference so OnNavigation can tear things down
      nsTArray<nsString>* array;
      if (!self->mCallIds.Get(windowID, &array)) {
        array = new nsTArray<nsString>();
        self->mCallIds.Put(windowID, array);
      }
      array->AppendElement(callID);

      nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
      if (!askPermission) {
        obs->NotifyObservers(devicesCopy, "getUserMedia:privileged:allow",
                             callID.BeginReading());
      } else {
        RefPtr<GetUserMediaRequest> req =
            new GetUserMediaRequest(window, callID, c, isHTTPS, isHandlingUserInput);
        if (!Preferences::GetBool("media.navigator.permission.force") && array->Length() > 1) {
          // there is at least 1 pending gUM request
          // For the scarySources test case, always send the request
          self->mPendingGUMRequest.AppendElement(req.forget());
        } else {
          obs->NotifyObservers(req, "getUserMedia:request", nullptr);
        }
      }

#ifdef MOZ_WEBRTC
      EnableWebRtcLog();
#endif
    }, [onFailure](MediaStreamError*& reason) mutable {
      LOG(("GetUserMedia: post enumeration pledge2 failure callback called!"));
      onFailure->OnError(reason);
    });
  }, [onFailure](MediaStreamError*& reason) mutable {
    LOG(("GetUserMedia: post enumeration pledge failure callback called!"));
    onFailure->OnError(reason);
  });
  return NS_OK;
}

/* static */ void
MediaManager::AnonymizeDevices(MediaDeviceSet& aDevices, const nsACString& aOriginKey)
{
  if (!aOriginKey.IsEmpty()) {
    for (RefPtr<MediaDevice>& device : aDevices) {
      nsString id;
      device->GetId(id);
      nsString rawId(id);
      AnonymizeId(id, aOriginKey);
      device = new MediaDevice(device, id, rawId);
    }
  }
}

/* static */ nsresult
MediaManager::AnonymizeId(nsAString& aId, const nsACString& aOriginKey)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  nsCOMPtr<nsIKeyObjectFactory> factory =
    do_GetService("@mozilla.org/security/keyobjectfactory;1", &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCString rawKey;
  rv = Base64Decode(aOriginKey, rawKey);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIKeyObject> key;
  rv = factory->KeyFromString(nsIKeyObject::HMAC, rawKey, getter_AddRefs(key));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsICryptoHMAC> hasher =
    do_CreateInstance(NS_CRYPTO_HMAC_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = hasher->Init(nsICryptoHMAC::SHA256, key);
  if (NS_FAILED(rv)) {
    return rv;
  }
  NS_ConvertUTF16toUTF8 id(aId);
  rv = hasher->Update(reinterpret_cast<const uint8_t*> (id.get()), id.Length());
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCString mac;
  rv = hasher->Finish(true, mac);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aId = NS_ConvertUTF8toUTF16(mac);
  return NS_OK;
}

/* static */
already_AddRefed<nsIWritableVariant>
MediaManager::ToJSArray(MediaDeviceSet& aDevices)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<nsVariantCC> var = new nsVariantCC();
  size_t len = aDevices.Length();
  if (len) {
    nsTArray<nsIMediaDevice*> tmp(len);
    for (auto& device : aDevices) {
      tmp.AppendElement(device);
    }
    auto* elements = static_cast<const void*>(tmp.Elements());
    nsresult rv = var->SetAsArray(nsIDataType::VTYPE_INTERFACE,
                                  &NS_GET_IID(nsIMediaDevice), len,
                                  const_cast<void*>(elements));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  } else {
    var->SetAsEmptyArray(); // because SetAsArray() fails on zero length arrays.
  }
  return var.forget();
}

already_AddRefed<MediaManager::PledgeMediaDeviceSet>
MediaManager::EnumerateDevicesImpl(uint64_t aWindowId,
                                   MediaSourceEnum       aVideoInputType,
                                   MediaSourceEnum       aAudioInputType,
                                   MediaSinkEnum  aAudioOutputType,
                                   DeviceEnumerationType aVideoInputEnumType,
                                   DeviceEnumerationType aAudioInputEnumType)
{
  MOZ_ASSERT(NS_IsMainThread());

  LOG(("%s: aWindowId=%" PRIu64 ", aVideoInputType=%" PRIu8 ", aAudioInputType=%" PRIu8
       ", aVideoInputEnumType=%" PRIu8 ", aAudioInputEnumType=%" PRIu8,
       __func__, aWindowId,
       static_cast<uint8_t>(aVideoInputType), static_cast<uint8_t>(aAudioInputType),
       static_cast<uint8_t>(aVideoInputEnumType), static_cast<uint8_t>(aAudioInputEnumType)));
  nsPIDOMWindowInner* window =
    nsGlobalWindowInner::GetInnerWindowWithId(aWindowId)->AsInner();

  // This function returns a pledge, a promise-like object with the future result
  RefPtr<PledgeMediaDeviceSet> pledge = new PledgeMediaDeviceSet();
  uint32_t id = mOutstandingPledges.Append(*pledge);

  // To get a device list anonymized for a particular origin, we must:
  // 1. Get an origin-key (for either regular or private browsing)
  // 2. Get the raw devices list
  // 3. Anonymize the raw list with the origin-key.

  nsCOMPtr<nsIPrincipal> principal =
    nsGlobalWindowInner::Cast(window)->GetPrincipal();
  MOZ_ASSERT(principal);

  ipc::PrincipalInfo principalInfo;
  nsresult rv = PrincipalToPrincipalInfo(principal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RefPtr<PledgeMediaDeviceSet> p = new PledgeMediaDeviceSet();
    RefPtr<MediaStreamError> error =
      new MediaStreamError(window, MediaStreamError::Name::NotAllowedError);
    p->Reject(error);
    return p.forget();
  }

  bool persist = IsActivelyCapturingOrHasAPermission(aWindowId);

  // GetPrincipalKey is an async API that returns a pledge (a promise-like
  // pattern). We use .Then() to pass in a lambda to run back on this same
  // thread later once GetPrincipalKey resolves. Needed variables are "captured"
  // (passed by value) safely into the lambda.

  RefPtr<Pledge<nsCString>> p = media::GetPrincipalKey(principalInfo, persist);
  p->Then([id, aWindowId, aVideoInputType, aAudioInputType,
          aVideoInputEnumType, aAudioInputEnumType, aAudioOutputType](const nsCString& aOriginKey) mutable {
    MOZ_ASSERT(NS_IsMainThread());
    MediaManager* mgr = MediaManager::GetIfExists();
    if (!mgr) {
      return;
    }

    RefPtr<PledgeMediaDeviceSet> p = mgr->EnumerateRawDevices(aWindowId,
                                                              aVideoInputType,
                                                              aAudioInputType,
                                                              aAudioOutputType,
                                                              aVideoInputEnumType,
                                                              aAudioInputEnumType);
    p->Then([id,
             aWindowId,
             aOriginKey,
             aVideoInputEnumType,
             aAudioInputEnumType,
             aVideoInputType,
             aAudioInputType](MediaDeviceSet*& aDevices) mutable {
      UniquePtr<MediaDeviceSet> devices(aDevices); // secondary result

      // Only run if window is still on our active list.
      MediaManager* mgr = MediaManager::GetIfExists();
      if (!mgr) {
        return NS_OK;
      }

      // If we fetched any real cameras or mics, remove the "default" part of
      // their IDs.
      if (aVideoInputType == MediaSourceEnum::Camera &&
          aAudioInputType == MediaSourceEnum::Microphone &&
          (aVideoInputEnumType != DeviceEnumerationType::Fake ||
           aAudioInputEnumType != DeviceEnumerationType::Fake)) {
        mgr->mDeviceIDs.Clear();
        for (auto& device : *devices) {
          nsString id;
          device->GetId(id);
          id.ReplaceSubstring(NS_LITERAL_STRING("default: "), NS_LITERAL_STRING(""));
          if (!mgr->mDeviceIDs.Contains(id)) {
            mgr->mDeviceIDs.AppendElement(id);
          }
        }
      }

      RefPtr<PledgeMediaDeviceSet> p = mgr->mOutstandingPledges.Remove(id);
      if (!p || !mgr->IsWindowStillActive(aWindowId)) {
        return NS_OK;
      }
      MediaManager_AnonymizeDevices(*devices, aOriginKey);
      p->Resolve(devices.release());
      return NS_OK;
    });
  });
  return pledge.forget();
}

nsresult
MediaManager::EnumerateDevices(nsPIDOMWindowInner* aWindow,
                               nsIGetUserMediaDevicesSuccessCallback* aOnSuccess,
                               nsIDOMGetUserMediaErrorCallback* aOnFailure,
                               dom::CallerType aCallerType)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(!sHasShutdown, NS_ERROR_FAILURE);
  nsCOMPtr<nsIGetUserMediaDevicesSuccessCallback> onSuccess(aOnSuccess);
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> onFailure(aOnFailure);
  uint64_t windowId = aWindow->WindowID();

  nsIPrincipal* principal = aWindow->GetExtantDoc()->NodePrincipal();

  RefPtr<GetUserMediaWindowListener> windowListener =
    GetWindowListener(windowId);
  if (windowListener) {
    PrincipalHandle existingPrincipalHandle =
      windowListener->GetPrincipalHandle();
    MOZ_ASSERT(PrincipalHandleMatches(existingPrincipalHandle, principal));
  } else {
    windowListener = new GetUserMediaWindowListener(mMediaThread, windowId,
                                                    MakePrincipalHandle(principal));
    AddWindowID(windowId, windowListener);
  }

  // Create an inactive SourceListener to act as a placeholder, so the
  // window listener doesn't clean itself up until we're done.
  RefPtr<SourceListener> sourceListener = new SourceListener();
  windowListener->Register(sourceListener);

  DeviceEnumerationType videoEnumerationType = DeviceEnumerationType::Normal;
  DeviceEnumerationType audioEnumerationType = DeviceEnumerationType::Normal;
  bool resistFingerprinting = nsContentUtils::ResistFingerprinting(aCallerType);

  // In order of precedence: resist fingerprinting > loopback > fake pref
  if (resistFingerprinting) {
    videoEnumerationType = DeviceEnumerationType::Fake;
    audioEnumerationType = DeviceEnumerationType::Fake;
  } else {
    // Handle loopback and fake requests
    nsAutoCString videoLoopDev, audioLoopDev;
    bool wantFakes = Preferences::GetBool("media.navigator.streams.fake");
    // Video
    Preferences::GetCString("media.video_loopback_dev", videoLoopDev);
    // Loopback prefs take precedence over fake prefs
    if (!videoLoopDev.IsEmpty()) {
      videoEnumerationType = DeviceEnumerationType::Loopback;
    } else if (wantFakes) {
      videoEnumerationType = DeviceEnumerationType::Fake;
    }
    // Audio
    Preferences::GetCString("media.audio_loopback_dev", audioLoopDev);
    // Loopback prefs take precedence over fake prefs
    if (!audioLoopDev.IsEmpty()) {
      audioEnumerationType = DeviceEnumerationType::Loopback;
    } else if (wantFakes) {
      audioEnumerationType = DeviceEnumerationType::Fake;
    }
  }

  MediaSinkEnum audioOutputType = MediaSinkEnum::Other;
  if (Preferences::GetBool("media.setsinkid.enabled")) {
    audioOutputType = MediaSinkEnum::Speaker;
  }
  RefPtr<PledgeMediaDeviceSet> p = EnumerateDevicesImpl(windowId,
                                                        MediaSourceEnum::Camera,
                                                        MediaSourceEnum::Microphone,
                                                        audioOutputType,
                                                        videoEnumerationType,
                                                        audioEnumerationType);
  p->Then([onSuccess, windowListener, sourceListener](MediaDeviceSet*& aDevices) mutable {
    UniquePtr<MediaDeviceSet> devices(aDevices); // grab result
    DebugOnly<bool> rv = windowListener->Remove(sourceListener);
    MOZ_ASSERT(rv);
    nsCOMPtr<nsIWritableVariant> array = MediaManager_ToJSArray(*devices);
    onSuccess->OnSuccess(array);
  }, [onFailure, windowListener, sourceListener](MediaStreamError*& reason) mutable {
    DebugOnly<bool> rv = windowListener->Remove(sourceListener);
    MOZ_ASSERT(rv);
    onFailure->OnError(reason);
  });
  return NS_OK;
}

/*
 * GetUserMediaDevices - called by the UI-part of getUserMedia from chrome JS.
 */

nsresult
MediaManager::GetUserMediaDevices(nsPIDOMWindowInner* aWindow,
                                  const MediaStreamConstraints& aConstraints,
                                  nsIGetUserMediaDevicesSuccessCallback* aOnSuccess,
                                  nsIDOMGetUserMediaErrorCallback* aOnFailure,
                                  uint64_t aWindowId,
                                  const nsAString& aCallID)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIGetUserMediaDevicesSuccessCallback> onSuccess(aOnSuccess);
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> onFailure(aOnFailure);
  if (!aWindowId) {
    aWindowId = aWindow->WindowID();
  }

  // Ignore passed-in constraints, instead locate + return already-constrained list.

  nsTArray<nsString>* callIDs;
  if (!mCallIds.Get(aWindowId, &callIDs)) {
    return NS_ERROR_UNEXPECTED;
  }

  for (auto& callID : *callIDs) {
    RefPtr<GetUserMediaTask> task;
    if (!aCallID.Length() || aCallID == callID) {
      if (mActiveCallbacks.Get(callID, getter_AddRefs(task))) {
        nsCOMPtr<nsIWritableVariant> array = MediaManager_ToJSArray(*task->mMediaDeviceSet);
        onSuccess->OnSuccess(array);
        return NS_OK;
      }
    }
  }
  return NS_ERROR_UNEXPECTED;
}

MediaEngine*
MediaManager::GetBackend(uint64_t aWindowId)
{
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  // Plugin backends as appropriate. The default engine also currently
  // includes picture support for Android.
  // This IS called off main-thread.
  if (!mBackend) {
    MOZ_RELEASE_ASSERT(!sHasShutdown);  // we should never create a new backend in shutdown
#if defined(MOZ_WEBRTC)
    mBackend = new MediaEngineWebRTC(mPrefs);
#else
    mBackend = new MediaEngineDefault();
#endif
    mBackend->AddDeviceChangeCallback(this);
  }
  return mBackend;
}

void
MediaManager::OnNavigation(uint64_t aWindowID)
{
  MOZ_ASSERT(NS_IsMainThread());
  LOG(("OnNavigation for %" PRIu64, aWindowID));

  // Stop the streams for this window. The runnables check this value before
  // making a call to content.

  nsTArray<nsString>* callIDs;
  if (mCallIds.Get(aWindowID, &callIDs)) {
    for (auto& callID : *callIDs) {
      mActiveCallbacks.Remove(callID);
    }
    mCallIds.Remove(aWindowID);
  }

  // This is safe since we're on main-thread, and the windowlist can only
  // be added to from the main-thread
  auto* window = nsGlobalWindowInner::GetInnerWindowWithId(aWindowID);
  if (window) {
    IterateWindowListeners(window->AsInner(),
        [self = RefPtr<MediaManager>(this),
         windowID = DebugOnly<decltype(aWindowID)>(aWindowID)]
        (GetUserMediaWindowListener* aListener)
        {
          // Grab a strong ref since RemoveAll() might destroy the listener
          // mid-way when clearing the mActiveWindows reference.
          RefPtr<GetUserMediaWindowListener> listener(aListener);

          listener->Stop();
          listener->RemoveAll();
          MOZ_ASSERT(!self->GetWindowListener(windowID));
        });
  } else {
    RemoveWindowID(aWindowID);
  }
  MOZ_ASSERT(!GetWindowListener(aWindowID));

  RemoveMediaDevicesCallback(aWindowID);

  RefPtr<MediaManager> self = this;
  MediaManager::PostTask(NewTaskFrom([self, aWindowID]() {
    self->GetBackend()->ReleaseResourcesForWindow(aWindowID);
  }));
}

void
MediaManager::RemoveMediaDevicesCallback(uint64_t aWindowID)
{
  MutexAutoLock lock(mCallbackMutex);
  for (DeviceChangeCallback* observer : mDeviceChangeCallbackList)
  {
    dom::MediaDevices* mediadevices = static_cast<dom::MediaDevices *>(observer);
    MOZ_ASSERT(mediadevices);
    if (mediadevices) {
      nsPIDOMWindowInner* window = mediadevices->GetOwner();
      MOZ_ASSERT(window);
      if (window && window->WindowID() == aWindowID) {
        DeviceChangeCallback::RemoveDeviceChangeCallbackLocked(observer);
        return;
      }
    }
  }
}

void
MediaManager::AddWindowID(uint64_t aWindowId,
                          GetUserMediaWindowListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  // Store the WindowID in a hash table and mark as active. The entry is removed
  // when this window is closed or navigated away from.
  // This is safe since we're on main-thread, and the windowlist can only
  // be invalidated from the main-thread (see OnNavigation)
  if (IsWindowStillActive(aWindowId)) {
    MOZ_ASSERT(false, "Window already added");
    return;
  }

  GetActiveWindows()->Put(aWindowId, aListener);
}

void
MediaManager::RemoveWindowID(uint64_t aWindowId)
{
  mActiveWindows.Remove(aWindowId);

  // get outer windowID
  auto* window = nsGlobalWindowInner::GetInnerWindowWithId(aWindowId);
  if (!window) {
    LOG(("No inner window for %" PRIu64, aWindowId));
    return;
  }

  nsPIDOMWindowOuter* outer = window->AsInner()->GetOuterWindow();
  if (!outer) {
    LOG(("No outer window for inner %" PRIu64, aWindowId));
    return;
  }

  uint64_t outerID = outer->WindowID();

  // Notify the UI that this window no longer has gUM active
  char windowBuffer[32];
  SprintfLiteral(windowBuffer, "%" PRIu64, outerID);
  nsString data = NS_ConvertUTF8toUTF16(windowBuffer);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  obs->NotifyObservers(nullptr, "recording-window-ended", data.get());
  LOG(("Sent recording-window-ended for window %" PRIu64 " (outer %" PRIu64 ")",
       aWindowId, outerID));
}

bool
MediaManager::IsWindowListenerStillActive(GetUserMediaWindowListener* aListener)
{
  MOZ_DIAGNOSTIC_ASSERT(aListener);
  return aListener && aListener == GetWindowListener(aListener->WindowID());
}

void
MediaManager::GetPref(nsIPrefBranch *aBranch, const char *aPref,
                      const char *aData, int32_t *aVal)
{
  int32_t temp;
  if (aData == nullptr || strcmp(aPref,aData) == 0) {
    if (NS_SUCCEEDED(aBranch->GetIntPref(aPref, &temp))) {
      *aVal = temp;
    }
  }
}

void
MediaManager::GetPrefBool(nsIPrefBranch *aBranch, const char *aPref,
                          const char *aData, bool *aVal)
{
  bool temp;
  if (aData == nullptr || strcmp(aPref,aData) == 0) {
    if (NS_SUCCEEDED(aBranch->GetBoolPref(aPref, &temp))) {
      *aVal = temp;
    }
  }
}

void
MediaManager::GetPrefs(nsIPrefBranch *aBranch, const char *aData)
{
  GetPref(aBranch, "media.navigator.video.default_width", aData, &mPrefs.mWidth);
  GetPref(aBranch, "media.navigator.video.default_height", aData, &mPrefs.mHeight);
  GetPref(aBranch, "media.navigator.video.default_fps", aData, &mPrefs.mFPS);
  GetPref(aBranch, "media.navigator.audio.fake_frequency", aData, &mPrefs.mFreq);
#ifdef MOZ_WEBRTC
  GetPrefBool(aBranch, "media.getusermedia.aec_enabled", aData, &mPrefs.mAecOn);
  GetPrefBool(aBranch, "media.getusermedia.agc_enabled", aData, &mPrefs.mAgcOn);
  GetPrefBool(aBranch, "media.getusermedia.noise_enabled", aData, &mPrefs.mNoiseOn);
  GetPref(aBranch, "media.getusermedia.aec", aData, &mPrefs.mAec);
  GetPref(aBranch, "media.getusermedia.agc", aData, &mPrefs.mAgc);
  GetPref(aBranch, "media.getusermedia.noise", aData, &mPrefs.mNoise);
  GetPrefBool(aBranch, "media.getusermedia.aec_extended_filter", aData, &mPrefs.mExtendedFilter);
  GetPrefBool(aBranch, "media.getusermedia.aec_aec_delay_agnostic", aData, &mPrefs.mDelayAgnostic);
  GetPref(aBranch, "media.getusermedia.channels", aData, &mPrefs.mChannels);
  GetPrefBool(aBranch, "media.ondevicechange.fakeDeviceChangeEvent.enabled", aData, &mPrefs.mFakeDeviceChangeEventOn);
#endif
  GetPrefBool(aBranch, "media.navigator.audio.full_duplex", aData, &mPrefs.mFullDuplex);
}

void
MediaManager::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (sHasShutdown) {
    return;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();

  obs->RemoveObserver(this, "last-pb-context-exited");
  obs->RemoveObserver(this, "getUserMedia:privileged:allow");
  obs->RemoveObserver(this, "getUserMedia:response:allow");
  obs->RemoveObserver(this, "getUserMedia:response:deny");
  obs->RemoveObserver(this, "getUserMedia:revoke");

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->RemoveObserver("media.navigator.video.default_width", this);
    prefs->RemoveObserver("media.navigator.video.default_height", this);
    prefs->RemoveObserver("media.navigator.video.default_fps", this);
    prefs->RemoveObserver("media.navigator.audio.fake_frequency", this);
#ifdef MOZ_WEBRTC
    prefs->RemoveObserver("media.getusermedia.aec_enabled", this);
    prefs->RemoveObserver("media.getusermedia.aec", this);
    prefs->RemoveObserver("media.getusermedia.agc_enabled", this);
    prefs->RemoveObserver("media.getusermedia.agc", this);
    prefs->RemoveObserver("media.getusermedia.noise_enabled", this);
    prefs->RemoveObserver("media.getusermedia.noise", this);
    prefs->RemoveObserver("media.ondevicechange.fakeDeviceChangeEvent.enabled", this);
    prefs->RemoveObserver("media.getusermedia.channels", this);
#endif
    prefs->RemoveObserver("media.navigator.audio.full_duplex", this);
  }

  {
    // Close off any remaining active windows.

    // Live capture at this point is rare but can happen. Stopping it will make
    // the window listeners attempt to remove themselves from the active windows
    // table. We cannot touch the table at point so we grab a copy of the window
    // listeners first.
    nsTArray<RefPtr<GetUserMediaWindowListener>> listeners(GetActiveWindows()->Count());
    for (auto iter = GetActiveWindows()->Iter(); !iter.Done(); iter.Next()) {
      listeners.AppendElement(iter.UserData());
    }
    for (auto& listener : listeners) {
      listener->Stop();
      listener->RemoveAll();
    }
  }
  MOZ_ASSERT(GetActiveWindows()->Count() == 0);

  GetActiveWindows()->Clear();
  mActiveCallbacks.Clear();
  mCallIds.Clear();
  mPendingGUMRequest.Clear();
  mDeviceIDs.Clear();
#ifdef MOZ_WEBRTC
  StopWebRtcLog();
#endif

  // From main thread's point of view, shutdown is now done.
  // All that remains is shutting down the media thread.
  sHasShutdown = true;

  // Because mMediaThread is not an nsThread, we must dispatch to it so it can
  // clean up BackgroundChild. Continue stopping thread once this is done.

  class ShutdownTask : public Runnable
  {
  public:
    ShutdownTask(MediaManager* aManager, already_AddRefed<Runnable> aReply)
      : mozilla::Runnable("ShutdownTask")
      , mManager(aManager)
      , mReply(aReply)
    {
    }

  private:
    NS_IMETHOD
    Run() override
    {
      LOG(("MediaManager Thread Shutdown"));
      MOZ_ASSERT(MediaManager::IsInMediaThread());
      // Must shutdown backend on MediaManager thread, since that's where we started it from!
      {
        if (mManager->mBackend) {
          mManager->mBackend->Shutdown(); // ok to invoke multiple times
          mManager->mBackend->RemoveDeviceChangeCallback(mManager);
        }
      }
      mozilla::ipc::BackgroundChild::CloseForCurrentThread();
      // must explicitly do this before dispatching the reply, since the reply may kill us with Stop()
      mManager->mBackend = nullptr; // last reference, will invoke Shutdown() again

      if (NS_FAILED(NS_DispatchToMainThread(mReply.forget()))) {
        LOG(("Will leak thread: DispatchToMainthread of reply runnable failed in MediaManager shutdown"));
      }

      return NS_OK;
    }
    RefPtr<MediaManager> mManager;
    RefPtr<Runnable> mReply;
  };

  // Post ShutdownTask to execute on mMediaThread and pass in a lambda
  // callback to be executed back on this thread once it is done.
  //
  // The lambda callback "captures" the 'this' pointer for member access.
  // This is safe since this is guaranteed to be here since sSingleton isn't
  // cleared until the lambda function clears it.

  // note that this == sSingleton
  MOZ_ASSERT(this == sSingleton);
  RefPtr<MediaManager> that = this;

  // Release the backend (and call Shutdown()) from within the MediaManager thread
  // Don't use MediaManager::PostTask() because we're sHasShutdown=true here!
  RefPtr<ShutdownTask> shutdown = new ShutdownTask(this,
      media::NewRunnableFrom([this, that]() mutable {
    LOG(("MediaManager shutdown lambda running, releasing MediaManager singleton and thread"));
    if (mMediaThread) {
      mMediaThread->Stop();
    }

    // Remove async shutdown blocker

    nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase = GetShutdownPhase();
    shutdownPhase->RemoveBlocker(sSingleton->mShutdownBlocker);

    // we hold a ref to 'that' which is the same as sSingleton
    sSingleton = nullptr;

    return NS_OK;
  }));
  mMediaThread->message_loop()->PostTask(shutdown.forget());
}

void
MediaManager::SendPendingGUMRequest()
{
  if (mPendingGUMRequest.Length() > 0) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    obs->NotifyObservers(mPendingGUMRequest[0], "getUserMedia:request", nullptr);
    mPendingGUMRequest.RemoveElementAt(0);
  }
}

nsresult
MediaManager::Observe(nsISupports* aSubject, const char* aTopic,
  const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> branch( do_QueryInterface(aSubject) );
    if (branch) {
      GetPrefs(branch,NS_ConvertUTF16toUTF8(aData).get());
      LOG(("%s: %dx%d @%dfps", __FUNCTION__,
           mPrefs.mWidth, mPrefs.mHeight, mPrefs.mFPS));
    }
  } else if (!strcmp(aTopic, "last-pb-context-exited")) {
    // Clear memory of private-browsing-specific deviceIds. Fire and forget.
    media::SanitizeOriginKeys(0, true);
    return NS_OK;
  } else if (!strcmp(aTopic, "getUserMedia:got-device-permission")) {
    MOZ_ASSERT(aSubject);
    nsCOMPtr<nsIRunnable> task = do_QueryInterface(aSubject);
    MediaManager::PostTask(NewTaskFrom([task] {
      task->Run();
    }));
    return NS_OK;
  } else if (!strcmp(aTopic, "getUserMedia:privileged:allow") ||
             !strcmp(aTopic, "getUserMedia:response:allow")) {
    nsString key(aData);
    RefPtr<GetUserMediaTask> task;
    mActiveCallbacks.Remove(key, getter_AddRefs(task));
    if (!task) {
      return NS_OK;
    }

    nsTArray<nsString>* array;
    if (!mCallIds.Get(task->GetWindowID(), &array)) {
      return NS_OK;
    }
    array->RemoveElement(key);

    if (aSubject) {
      // A particular device or devices were chosen by the user.
      // NOTE: does not allow setting a device to null; assumes nullptr
      nsCOMPtr<nsIArray> array(do_QueryInterface(aSubject));
      MOZ_ASSERT(array);
      uint32_t len = 0;
      array->GetLength(&len);
      bool videoFound = false, audioFound = false;
      for (uint32_t i = 0; i < len; i++) {
        nsCOMPtr<nsIMediaDevice> device;
        array->QueryElementAt(i, NS_GET_IID(nsIMediaDevice),
                              getter_AddRefs(device));
        MOZ_ASSERT(device); // shouldn't be returning anything else...
        if (!device) {
          continue;
        }

        // Casting here is safe because a MediaDevice is created
        // only in Gecko side, JS can only query for an instance.
        MediaDevice* dev = static_cast<MediaDevice*>(device.get());
        if (dev->mKind == dom::MediaDeviceKind::Videoinput) {
          if (!videoFound) {
            task->SetVideoDevice(dev);
            videoFound = true;
          }
        } else if (dev->mKind == dom::MediaDeviceKind::Audioinput) {
          if (!audioFound) {
            task->SetAudioDevice(dev);
            audioFound = true;
          }
        } else {
          NS_WARNING("Unknown device type in getUserMedia");
        }
      }
      bool needVideo = IsOn(task->GetConstraints().mVideo);
      bool needAudio = IsOn(task->GetConstraints().mAudio);
      MOZ_ASSERT(needVideo || needAudio);

      if ((needVideo && !videoFound) || (needAudio && !audioFound)) {
        task->Denied(MediaMgrError::Name::NotAllowedError);
        return NS_OK;
      }
    }

    if (sHasShutdown) {
      return task->Denied(MediaMgrError::Name::AbortError,
                          NS_LITERAL_STRING("In shutdown"));
    }
    // Reuse the same thread to save memory.
    MediaManager::PostTask(task.forget());
    return NS_OK;

  } else if (!strcmp(aTopic, "getUserMedia:response:deny")) {
    nsString key(aData);
    RefPtr<GetUserMediaTask> task;
    mActiveCallbacks.Remove(key, getter_AddRefs(task));
    if (task) {
      task->Denied(MediaMgrError::Name::NotAllowedError);
      nsTArray<nsString>* array;
      if (!mCallIds.Get(task->GetWindowID(), &array)) {
        return NS_OK;
      }
      array->RemoveElement(key);
      SendPendingGUMRequest();
    }
    return NS_OK;

  } else if (!strcmp(aTopic, "getUserMedia:revoke")) {
    nsresult rv;
    // may be windowid or screen:windowid
    nsDependentString data(aData);
    if (Substring(data, 0, strlen("screen:")).EqualsLiteral("screen:")) {
      uint64_t windowID = PromiseFlatString(Substring(data, strlen("screen:"))).ToInteger64(&rv);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (NS_SUCCEEDED(rv)) {
        LOG(("Revoking Screen/windowCapture access for window %" PRIu64, windowID));
        StopScreensharing(windowID);
      }
    } else {
      uint64_t windowID = nsString(aData).ToInteger64(&rv);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (NS_SUCCEEDED(rv)) {
        LOG(("Revoking MediaCapture access for window %" PRIu64, windowID));
        OnNavigation(windowID);
      }
    }
    return NS_OK;
  }

  return NS_OK;
}

nsresult
MediaManager::GetActiveMediaCaptureWindows(nsIArray** aArray)
{
  MOZ_ASSERT(aArray);

  nsCOMPtr<nsIMutableArray> array = nsArray::Create();

  for (auto iter = mActiveWindows.Iter(); !iter.Done(); iter.Next()) {
    const uint64_t& id = iter.Key();
    RefPtr<GetUserMediaWindowListener> winListener = iter.UserData();
    if (!winListener) {
      continue;
    }

    nsPIDOMWindowInner* window =
      nsGlobalWindowInner::GetInnerWindowWithId(id)->AsInner();
    MOZ_ASSERT(window);
    // XXXkhuey ...
    if (!window) {
      continue;
    }

    if (winListener->CapturingVideo() || winListener->CapturingAudio()) {
      array->AppendElement(window);
    }
  }

  array.forget(aArray);
  return NS_OK;
}

struct CaptureWindowStateData {
  uint16_t* mCamera;
  uint16_t* mMicrophone;
  uint16_t* mScreenShare;
  uint16_t* mWindowShare;
  uint16_t* mAppShare;
  uint16_t* mBrowserShare;
};

NS_IMETHODIMP
MediaManager::MediaCaptureWindowState(nsIDOMWindow* aCapturedWindow,
                                      uint16_t* aCamera,
                                      uint16_t* aMicrophone,
                                      uint16_t* aScreen,
                                      uint16_t* aWindow,
                                      uint16_t* aApplication,
                                      uint16_t* aBrowser)
{
  MOZ_ASSERT(NS_IsMainThread());

  CaptureState camera = CaptureState::Off;
  CaptureState microphone = CaptureState::Off;
  CaptureState screen = CaptureState::Off;
  CaptureState window = CaptureState::Off;
  CaptureState application = CaptureState::Off;
  CaptureState browser = CaptureState::Off;

  nsCOMPtr<nsPIDOMWindowInner> piWin = do_QueryInterface(aCapturedWindow);
  if (piWin) {
    IterateWindowListeners(piWin,
      [&camera, &microphone, &screen, &window, &application, &browser]
      (GetUserMediaWindowListener* aListener)
      {
        camera = CombineCaptureState(
            camera, aListener->CapturingSource(MediaSourceEnum::Camera));
        microphone = CombineCaptureState(
            microphone, aListener->CapturingSource(MediaSourceEnum::Microphone));
        screen = CombineCaptureState(
            screen, aListener->CapturingSource(MediaSourceEnum::Screen));
        window = CombineCaptureState(
            window, aListener->CapturingSource(MediaSourceEnum::Window));
        application = CombineCaptureState(
            application, aListener->CapturingSource(MediaSourceEnum::Application));
        browser = CombineCaptureState(
            browser, aListener->CapturingSource(MediaSourceEnum::Browser));
      });
  }

  *aCamera = FromCaptureState(camera);
  *aMicrophone= FromCaptureState(microphone);
  *aScreen = FromCaptureState(screen);
  *aWindow = FromCaptureState(window);
  *aApplication = FromCaptureState(application);
  *aBrowser = FromCaptureState(browser);

#ifdef DEBUG
  LOG(("%s: window %" PRIu64 " capturing %s %s %s %s %s %s", __FUNCTION__, piWin ? piWin->WindowID() : -1,
       *aCamera == nsIMediaManagerService::STATE_CAPTURE_ENABLED
         ? "camera (enabled)"
         : (*aCamera == nsIMediaManagerService::STATE_CAPTURE_DISABLED
            ? "camera (disabled)" : ""),
       *aMicrophone == nsIMediaManagerService::STATE_CAPTURE_ENABLED
         ? "microphone (enabled)"
         : (*aMicrophone == nsIMediaManagerService::STATE_CAPTURE_DISABLED
            ? "microphone (disabled)" : ""),
       *aScreen ? "screenshare" : "",
       *aWindow ? "windowshare" : "",
       *aApplication ? "appshare" : "",
       *aBrowser ? "browsershare" : ""));
#endif
  return NS_OK;
}

NS_IMETHODIMP
MediaManager::SanitizeDeviceIds(int64_t aSinceWhen)
{
  MOZ_ASSERT(NS_IsMainThread());
  LOG(("%s: sinceWhen = %" PRId64, __FUNCTION__, aSinceWhen));

  media::SanitizeOriginKeys(aSinceWhen, false); // we fire and forget
  return NS_OK;
}

void
MediaManager::StopScreensharing(uint64_t aWindowID)
{
  // We need to stop window/screensharing for all streams in all innerwindows that
  // correspond to that outerwindow.

  auto* window = nsGlobalWindowInner::GetInnerWindowWithId(aWindowID);
  if (!window) {
    return;
  }
  IterateWindowListeners(window->AsInner(),
    [](GetUserMediaWindowListener* aListener)
    {
      aListener->StopSharing();
    });
}

template<typename FunctionType>
void
MediaManager::IterateWindowListeners(nsPIDOMWindowInner* aWindow,
                                     const FunctionType& aCallback)
{
  // Iterate the docshell tree to find all the child windows, and for each
  // invoke the callback
  if (aWindow) {
    {
      uint64_t windowID = aWindow->WindowID();
      GetUserMediaWindowListener* listener = GetWindowListener(windowID);
      if (listener) {
        aCallback(listener);
      }
      // NB: `listener` might have been destroyed.
    }

    // iterate any children of *this* window (iframes, etc)
    nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
    if (docShell) {
      int32_t i, count;
      docShell->GetChildCount(&count);
      for (i = 0; i < count; ++i) {
        nsCOMPtr<nsIDocShellTreeItem> item;
        docShell->GetChildAt(i, getter_AddRefs(item));
        nsCOMPtr<nsPIDOMWindowOuter> winOuter = item ? item->GetWindow() : nullptr;

        if (winOuter) {
          IterateWindowListeners(winOuter->GetCurrentInnerWindow(), aCallback);
        }
      }
    }
  }
}


void
MediaManager::StopMediaStreams()
{
  nsCOMPtr<nsIArray> array;
  GetActiveMediaCaptureWindows(getter_AddRefs(array));
  uint32_t len;
  array->GetLength(&len);
  for (uint32_t i = 0; i < len; i++) {
    nsCOMPtr<nsPIDOMWindowInner> win;
    array->QueryElementAt(i, NS_GET_IID(nsPIDOMWindowInner),
                          getter_AddRefs(win));
    if (win) {
      OnNavigation(win->WindowID());
    }
  }
}

bool
MediaManager::IsActivelyCapturingOrHasAPermission(uint64_t aWindowId)
{
  // Does page currently have a gUM stream active?

  nsCOMPtr<nsIArray> array;
  GetActiveMediaCaptureWindows(getter_AddRefs(array));
  uint32_t len;
  array->GetLength(&len);
  for (uint32_t i = 0; i < len; i++) {
    nsCOMPtr<nsPIDOMWindowInner> win;
    array->QueryElementAt(i, NS_GET_IID(nsPIDOMWindowInner),
                          getter_AddRefs(win));
    if (win && win->WindowID() == aWindowId) {
      return true;
    }
  }

  // Or are persistent permissions (audio or video) granted?

  auto* window = nsGlobalWindowInner::GetInnerWindowWithId(aWindowId);
  if (NS_WARN_IF(!window) || NS_WARN_IF(!window->GetPrincipal())) {
    return false;
  }
  // Check if this site has persistent permissions.
  nsresult rv;
  nsCOMPtr<nsIPermissionManager> mgr =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false; // no permission manager no permissions!
  }

  uint32_t audio = nsIPermissionManager::UNKNOWN_ACTION;
  uint32_t video = nsIPermissionManager::UNKNOWN_ACTION;
  {
    auto* principal = window->GetPrincipal();
    rv = mgr->TestExactPermissionFromPrincipal(principal, "microphone", &audio);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
    rv = mgr->TestExactPermissionFromPrincipal(principal, "camera", &video);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
  }
  return audio == nsIPermissionManager::ALLOW_ACTION ||
         video == nsIPermissionManager::ALLOW_ACTION;
}

SourceListener::SourceListener()
  : mStopped(false)
  , mFinished(false)
  , mRemoved(false)
  , mMainThreadCheck(nullptr)
  , mPrincipalHandle(PRINCIPAL_HANDLE_NONE)
  , mWindowListener(nullptr)
{}

void
SourceListener::Register(GetUserMediaWindowListener* aListener)
{
  LOG(("SourceListener %p registering with window listener %p", this, aListener));

  MOZ_ASSERT(aListener, "No listener");
  MOZ_ASSERT(!mWindowListener, "Already registered");
  MOZ_ASSERT(!Activated(), "Already activated");

  mPrincipalHandle = aListener->GetPrincipalHandle();
  mWindowListener = aListener;
}

void
SourceListener::Activate(SourceMediaStream* aStream,
                         MediaDevice* aAudioDevice,
                         MediaDevice* aVideoDevice)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

  LOG(("SourceListener %p activating audio=%p video=%p", this, aAudioDevice, aVideoDevice));

  MOZ_ASSERT(!mStopped, "Cannot activate stopped source listener");
  MOZ_ASSERT(!Activated(), "Already activated");

  mMainThreadCheck = GetCurrentVirtualThread();
  mStream = aStream;
  mStreamListener = new SourceStreamListener(this);
  if (aAudioDevice) {
    mAudioDeviceState =
      MakeUnique<DeviceState>(
          aAudioDevice,
          aAudioDevice->GetMediaSource() == dom::MediaSourceEnum::Microphone &&
          Preferences::GetBool("media.getusermedia.microphone.off_while_disabled.enabled", true));
  }

  if (aVideoDevice) {
    mVideoDeviceState =
      MakeUnique<DeviceState>(
          aVideoDevice,
          aVideoDevice->GetMediaSource() == dom::MediaSourceEnum::Camera &&
          Preferences::GetBool("media.getusermedia.camera.off_while_disabled.enabled", true));
  }

  mStream->AddListener(mStreamListener);
}

RefPtr<SourceListener::InitPromise>
SourceListener::InitializeAsync()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");
  MOZ_DIAGNOSTIC_ASSERT(!mStopped);

  RefPtr<InitPromise> init = MediaManager::PostTask<InitPromise>(__func__,
    [ stream = mStream
    , principal = GetPrincipalHandle()
    , audioDevice = mAudioDeviceState ? mAudioDeviceState->mDevice : nullptr
    , videoDevice = mVideoDeviceState ? mVideoDeviceState->mDevice : nullptr
    ](MozPromiseHolder<InitPromise>& aHolder)
    {
      if (audioDevice) {
        nsresult rv = audioDevice->SetTrack(stream, kAudioTrack, principal);
        if (NS_SUCCEEDED(rv)) {
          rv = audioDevice->Start();
        }
        if (NS_FAILED(rv)) {
          nsString log;
          if (rv == NS_ERROR_NOT_AVAILABLE) {
            log.AssignASCII("Concurrent mic process limit.");
            aHolder.Reject(MakeRefPtr<MediaMgrError>(
                  MediaMgrError::Name::NotReadableError, log), __func__);
            return;
          }
          log.AssignASCII("Starting audio failed");
          aHolder.Reject(MakeRefPtr<MediaMgrError>(
                MediaMgrError::Name::AbortError, log), __func__);
          return;
        }
      }

      if (videoDevice) {
        nsresult rv = videoDevice->SetTrack(stream, kVideoTrack, principal);
        if (NS_SUCCEEDED(rv)) {
          rv = videoDevice->Start();
        }
        if (NS_FAILED(rv)) {
          if (audioDevice) {
            if (NS_WARN_IF(NS_FAILED(audioDevice->Stop()))) {
              MOZ_ASSERT_UNREACHABLE("Stopping audio failed");
            }
          }
          nsString log;
          log.AssignASCII("Starting video failed");
          aHolder.Reject(MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError, log), __func__);
          return;
        }
      }

      // Start() queued the tracks to be added synchronously to avoid races
      stream->FinishAddTracks();
      stream->AdvanceKnownTracksTime(STREAM_TIME_MAX);
      LOG(("started all sources"));

      aHolder.Resolve(true, __func__);
    });

  return init->Then(GetMainThreadSerialEventTarget(), __func__,
    [self = RefPtr<SourceListener>(this), this]()
    {
      if (mStopped) {
        // We were shut down during the async init
        return InitPromise::CreateAndResolve(true, __func__);
      }

      mStream->SetPullEnabled(true);

      for (DeviceState* state : {mAudioDeviceState.get(),
                                 mVideoDeviceState.get()}) {
        if (!state) {
          continue;
        }
        MOZ_DIAGNOSTIC_ASSERT(!state->mTrackEnabled);
        MOZ_DIAGNOSTIC_ASSERT(!state->mDeviceEnabled);
        MOZ_DIAGNOSTIC_ASSERT(!state->mStopped);

        state->mDeviceEnabled = true;
        state->mTrackEnabled = true;
        state->mTrackEnabledTime = TimeStamp::Now();
      }
      return InitPromise::CreateAndResolve(true, __func__);
    }, [self = RefPtr<SourceListener>(this), this](RefPtr<MediaMgrError>&& aResult)
    {
      if (mStopped) {
        return InitPromise::CreateAndReject(std::move(aResult), __func__);
      }

      for (DeviceState* state : {mAudioDeviceState.get(),
                                 mVideoDeviceState.get()}) {
        if (!state) {
          continue;
        }
        MOZ_DIAGNOSTIC_ASSERT(!state->mTrackEnabled);
        MOZ_DIAGNOSTIC_ASSERT(!state->mDeviceEnabled);
        MOZ_DIAGNOSTIC_ASSERT(!state->mStopped);

        state->mStopped = true;
      }
      return InitPromise::CreateAndReject(std::move(aResult), __func__);
    });
}

void
SourceListener::Stop()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

  if (mStopped) {
    return;
  }

  LOG(("SourceListener %p stopping", this));

  // StopSharing() has some special logic, at least for audio capture.
  // It must be called when all tracks have stopped, before setting mStopped.
  StopSharing();

  mStopped = true;

  MOZ_ASSERT(Activated(), "There are no devices or any source stream to stop");
  MOZ_ASSERT(mStream, "Can't end tracks. No source stream.");

  if (mAudioDeviceState && !mAudioDeviceState->mStopped) {
    StopTrack(kAudioTrack);
  }
  if (mVideoDeviceState && !mVideoDeviceState->mStopped) {
    StopTrack(kVideoTrack);
  }

  MediaManager::PostTask(NewTaskFrom([source = mStream]() {
    MOZ_ASSERT(MediaManager::IsInMediaThread());
    source->EndAllTrackAndFinish();
  }));
}

void
SourceListener::Remove()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mAudioDeviceState) {
    mAudioDeviceState->mDisableTimer->Cancel();
  }
  if (mVideoDeviceState) {
    mVideoDeviceState->mDisableTimer->Cancel();
  }

  if (!mStream || mRemoved) {
    return;
  }

  LOG(("SourceListener %p removed on purpose, mFinished = %d", this, (int) mFinished));
  mRemoved = true; // RemoveListener is async, avoid races
  mWindowListener = nullptr;

  // If it's destroyed, don't call - listener will be removed and we'll be notified!
  if (!mStream->IsDestroyed()) {
    // We disable pulling before removing so we don't risk having live tracks
    // without a listener attached - that wouldn't produce data and would be
    // illegal to the graph.
    mStream->SetPullEnabled(false);
    mStream->RemoveListener(mStreamListener);
  }
  mStreamListener = nullptr;
}

void
SourceListener::StopTrack(TrackID aTrackID)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");
  MOZ_ASSERT(Activated(), "No device to stop");
  MOZ_ASSERT(aTrackID == kAudioTrack || aTrackID == kVideoTrack,
             "Unknown track id");
  DeviceState& state = GetDeviceStateFor(aTrackID);

  LOG(("SourceListener %p stopping %s track %d",
       this, aTrackID == kAudioTrack ? "audio" : "video", aTrackID));

  if (state.mStopped) {
    // device already stopped.
    return;
  }
  state.mStopped = true;

  state.mDisableTimer->Cancel();

  MediaManager::PostTask(NewTaskFrom([device = state.mDevice]() {
    device->Stop();
    device->Deallocate();
  }));

  if ((!mAudioDeviceState || mAudioDeviceState->mStopped) &&
      (!mVideoDeviceState || mVideoDeviceState->mStopped)) {
    LOG(("SourceListener %p this was the last track stopped", this));
    Stop();
  }

  MOZ_ASSERT(mWindowListener, "Should still have window listener");
  mWindowListener->ChromeAffectingStateChanged();
}

void
SourceListener::GetSettingsFor(TrackID aTrackID,
                               dom::MediaTrackSettings& aOutSettings) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");
  GetDeviceStateFor(aTrackID).mDevice->GetSettings(aOutSettings);
}

void
SourceListener::SetEnabledFor(TrackID aTrackID, bool aEnable)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");
  MOZ_ASSERT(Activated(), "No device to set enabled state for");
  MOZ_ASSERT(aTrackID == kAudioTrack || aTrackID == kVideoTrack,
             "Unknown track id");

  if (mRemoved) {
    return;
  }

  LOG(("SourceListener %p %s %s track %d",
       this, aEnable ? "enabling" : "disabling",
       aTrackID == kAudioTrack ? "audio" : "video", aTrackID));

  DeviceState& state = GetDeviceStateFor(aTrackID);

  state.mTrackEnabled = aEnable;

  if (state.mStopped) {
    // Device terminally stopped. Updating device state is pointless.
    return;
  }

  if (state.mOperationInProgress) {
    // If a timer is in progress, it needs to be canceled now so the next
    // DisableTrack() gets a fresh start. Canceling will trigger another
    // operation.
    state.mDisableTimer->Cancel();
    return;
  }

  if (state.mDeviceEnabled == aEnable) {
    // Device is already in the desired state.
    return;
  }

  // All paths from here on must end in setting `state.mOperationInProgress`
  // to false.
  state.mOperationInProgress = true;

  RefPtr<MediaTimerPromise> timerPromise;
  if (aEnable) {
    timerPromise = MediaTimerPromise::CreateAndResolve(true, __func__);
    state.mTrackEnabledTime = TimeStamp::Now();
  } else {
    const TimeDuration maxDelay = TimeDuration::FromMilliseconds(
      Preferences::GetUint(
        aTrackID == kAudioTrack
          ? "media.getusermedia.microphone.off_while_disabled.delay_ms"
          : "media.getusermedia.camera.off_while_disabled.delay_ms",
        3000));
    const TimeDuration durationEnabled =
      TimeStamp::Now() - state.mTrackEnabledTime;
    const TimeDuration delay =
      TimeDuration::Max(TimeDuration::FromMilliseconds(0),
                        maxDelay - durationEnabled);
    timerPromise = state.mDisableTimer->WaitFor(delay, __func__);
  }

  typedef MozPromise<nsresult, bool, /* IsExclusive = */ true> DeviceOperationPromise;
  RefPtr<SourceListener> self = this;
  timerPromise->Then(GetMainThreadSerialEventTarget(), __func__,
    [self, this, &state, aTrackID, aEnable]() mutable {
      MOZ_ASSERT(state.mDeviceEnabled != aEnable,
                 "Device operation hasn't started");
      MOZ_ASSERT(state.mOperationInProgress,
                 "It's our responsibility to reset the inProgress state");

      LOG(("SourceListener %p %s %s track %d - starting device operation",
           this, aEnable ? "enabling" : "disabling",
           aTrackID == kAudioTrack ? "audio" : "video",
           aTrackID));

      if (mRemoved) {
        // Listener was removed between timer resolving and this runnable.
        return DeviceOperationPromise::CreateAndResolve(NS_ERROR_ABORT, __func__);
      }

      if (state.mStopped) {
        // Source was stopped between timer resolving and this runnable.
        return DeviceOperationPromise::CreateAndResolve(NS_ERROR_ABORT, __func__);
      }

      state.mDeviceEnabled = aEnable;

      if (mWindowListener) {
        mWindowListener->ChromeAffectingStateChanged();
      }

      if (!state.mOffWhileDisabled) {
        // If the feature to turn a device off while disabled is itself disabled
        // we shortcut the device operation and tell the ux-updating code
        // that everything went fine.
        return DeviceOperationPromise::CreateAndResolve(NS_OK, __func__);
      }

      return MediaManager::PostTask<DeviceOperationPromise>(__func__,
          [self, device = state.mDevice, aEnable]
          (MozPromiseHolder<DeviceOperationPromise>& h) {
            h.Resolve(aEnable ? device->Start() : device->Stop(), __func__);
          });
    }, []() {
      // Timer was canceled by us. We signal this with NS_ERROR_ABORT.
      return DeviceOperationPromise::CreateAndResolve(NS_ERROR_ABORT, __func__);
    })->Then(GetMainThreadSerialEventTarget(), __func__,
    [self, this, &state, aTrackID, aEnable](nsresult aResult) mutable {
      MOZ_ASSERT_IF(aResult != NS_ERROR_ABORT,
                    state.mDeviceEnabled == aEnable);
      MOZ_ASSERT(state.mOperationInProgress);
      state.mOperationInProgress = false;

      if (state.mStopped) {
        // Device was stopped on main thread during the operation. Nothing to do.
        return;
      }

      LOG(("SourceListener %p %s %s track %d %s",
           this,
           aEnable ? "enabling" : "disabling",
           aTrackID == kAudioTrack ? "audio" : "video",
           aTrackID,
           NS_SUCCEEDED(aResult) ? "succeeded" : "failed"));

      if (NS_FAILED(aResult) && aResult != NS_ERROR_ABORT) {
        // This path handles errors from starting or stopping the device.
        // NS_ERROR_ABORT are for cases where *we* aborted. They need graceful
        // handling.
        if (aEnable) {
          // Starting the device failed. Stopping the track here will make the
          // MediaStreamTrack end after a pass through the MediaStreamGraph.
          StopTrack(aTrackID);
        } else {
          // Stopping the device failed. This is odd, but not fatal.
          MOZ_ASSERT_UNREACHABLE("The device should be stoppable");

          // To keep our internal state sane in this case, we disallow future
          // stops due to disable.
          state.mOffWhileDisabled = false;
        }
        return;
      }

      // This path is for a device operation aResult that was success or
      // NS_ERROR_ABORT (*we* canceled the operation).
      // At this point we have to follow up on the intended state, i.e., update
      // the device state if the track state changed in the meantime.

      if (state.mTrackEnabled == state.mDeviceEnabled) {
        // Intended state is same as device's current state.
        // Nothing more to do.
        return;
      }

      // Track state changed during this operation. We'll start over.
      if (state.mTrackEnabled) {
        SetEnabledFor(aTrackID, true);
      } else {
        SetEnabledFor(aTrackID, false);
      }
    }, []() {
      MOZ_ASSERT_UNREACHABLE("Unexpected and unhandled reject");
    });
}

void
SourceListener::StopSharing()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(mWindowListener);

  if (mStopped) {
    return;
  }

  LOG(("SourceListener %p StopSharing", this));

  if (mVideoDeviceState &&
      (mVideoDeviceState->mDevice->GetMediaSource() == MediaSourceEnum::Screen ||
       mVideoDeviceState->mDevice->GetMediaSource() == MediaSourceEnum::Application ||
       mVideoDeviceState->mDevice->GetMediaSource() == MediaSourceEnum::Window)) {
    // We want to stop the whole stream if there's no audio;
    // just the video track if we have both.
    // StopTrack figures this out for us.
    StopTrack(kVideoTrack);
  }
  if (mAudioDeviceState &&
      mAudioDeviceState->mDevice->GetMediaSource() == MediaSourceEnum::AudioCapture) {
    uint64_t windowID = mWindowListener->WindowID();
    nsCOMPtr<nsPIDOMWindowInner> window = nsGlobalWindowInner::GetInnerWindowWithId(windowID)->AsInner();
    MOZ_RELEASE_ASSERT(window);
    window->SetAudioCapture(false);
    MediaStreamGraph* graph = mStream->Graph();
    graph->UnregisterCaptureStreamForWindow(windowID);
    mStream->Destroy();
  }
}

SourceMediaStream*
SourceListener::GetSourceStream()
{
  NS_ASSERTION(mStream,"Getting stream from never-activated SourceListener");
  return mStream;
}


// Proxy NotifyPull() to sources
void
SourceListener::NotifyPull(MediaStreamGraph* aGraph,
                           StreamTime aDesiredTime)
{
  if (mAudioDeviceState) {
    mAudioDeviceState->mDevice->Pull(mStream, kAudioTrack,
                                     aDesiredTime, mPrincipalHandle);
  }
  if (mVideoDeviceState) {
    mVideoDeviceState->mDevice->Pull(mStream, kVideoTrack,
                                     aDesiredTime, mPrincipalHandle);
  }
}

void
SourceListener::NotifyFinished()
{
  MOZ_ASSERT(NS_IsMainThread());
  mFinished = true;
  if (!mWindowListener) {
    // Removed explicitly before finished.
    return;
  }

  LOG(("SourceListener %p NotifyFinished", this));

  Stop(); // we know it's been activated
  mWindowListener->Remove(this);
}

void
SourceListener::NotifyRemoved()
{
  MOZ_ASSERT(NS_IsMainThread());
  LOG(("SourceListener removed, mFinished = %d", (int) mFinished));
  mRemoved = true;

  if (Activated() && !mFinished) {
    NotifyFinished();
  }

  mWindowListener = nullptr;
  mStreamListener = nullptr;
}

bool
SourceListener::CapturingVideo() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return Activated() && mVideoDeviceState && !mVideoDeviceState->mStopped &&
         (!mVideoDeviceState->mDevice->mSource->IsFake() ||
          Preferences::GetBool("media.navigator.permission.fake"));
}

bool
SourceListener::CapturingAudio() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return Activated() && mAudioDeviceState && !mAudioDeviceState->mStopped &&
         (!mAudioDeviceState->mDevice->mSource->IsFake() ||
          Preferences::GetBool("media.navigator.permission.fake"));
}

CaptureState
SourceListener::CapturingSource(MediaSourceEnum aSource) const
{
  MOZ_ASSERT(NS_IsMainThread());
  if ((!GetVideoDevice() || GetVideoDevice()->GetMediaSource() != aSource) &&
      (!GetAudioDevice() || GetAudioDevice()->GetMediaSource() != aSource)) {
    // This SourceListener doesn't capture a matching source
    return CaptureState::Off;
  }

  DeviceState& state =
    (GetAudioDevice() && GetAudioDevice()->GetMediaSource() == aSource)
    ? *mAudioDeviceState : *mVideoDeviceState;
  MOZ_ASSERT(state.mDevice->GetMediaSource() == aSource);

  if (state.mStopped) {
    // The source is a match but has been permanently stopped
    return CaptureState::Off;
  }

  if ((aSource == MediaSourceEnum::Camera ||
       aSource == MediaSourceEnum::Microphone) &&
      state.mDevice->mSource->IsFake() &&
      !Preferences::GetBool("media.navigator.permission.fake")) {
    // Fake Camera and Microphone only count if there is no fake permission
    return CaptureState::Off;
  }

  // Source is a match and is active

  if (state.mDeviceEnabled) {
    return CaptureState::Enabled;
  }

  return CaptureState::Disabled;
}

RefPtr<SourceListener::ApplyConstraintsPromise>
SourceListener::ApplyConstraintsToTrack(
    nsPIDOMWindowInner* aWindow,
    TrackID aTrackID,
    const MediaTrackConstraints& aConstraintsPassedIn,
    dom::CallerType aCallerType)
{
  MOZ_ASSERT(NS_IsMainThread());
  DeviceState& state = GetDeviceStateFor(aTrackID);

  if (mStopped || state.mStopped) {
    LOG(("gUM %s track %d applyConstraints, but source is stopped",
         aTrackID == kAudioTrack ? "audio" : "video", aTrackID));
    return ApplyConstraintsPromise::CreateAndResolve(false, __func__);
  }

  MediaTrackConstraints c(aConstraintsPassedIn); // use a modifiable copy
  MediaConstraintsHelper::ConvertOldWithWarning(c.mMozAutoGainControl,
                                                c.mAutoGainControl,
                                                "MozAutoGainControlWarning",
                                                aWindow);
  MediaConstraintsHelper::ConvertOldWithWarning(c.mMozNoiseSuppression,
                                                c.mNoiseSuppression,
                                                "MozNoiseSuppressionWarning",
                                                aWindow);

  MediaManager* mgr = MediaManager::GetIfExists();
  if (!mgr) {
    return ApplyConstraintsPromise::CreateAndResolve(false, __func__);
  }

  return MediaManager::PostTask<ApplyConstraintsPromise>(__func__,
      [device = state.mDevice, c,
       isChrome = aCallerType == dom::CallerType::System]
      (MozPromiseHolder<ApplyConstraintsPromise>& aHolder) mutable {
    MOZ_ASSERT(MediaManager::IsInMediaThread());
    MediaManager* mgr = MediaManager::GetIfExists();
    MOZ_RELEASE_ASSERT(mgr); // Must exist while media thread is alive
    const char* badConstraint = nullptr;
    nsresult rv = device->Reconfigure(c, mgr->mPrefs, &badConstraint);
    if (rv == NS_ERROR_INVALID_ARG) {
      // Reconfigure failed due to constraints
      if (!badConstraint) {
        nsTArray<RefPtr<MediaDevice>> devices;
        devices.AppendElement(device);
        badConstraint = MediaConstraintsHelper::SelectSettings(
            NormalizedConstraints(c), devices, isChrome);
      }

      aHolder.Reject(Some(NS_ConvertASCIItoUTF16(badConstraint)), __func__);
      return;
    }

    if (NS_FAILED(rv)) {
      // Reconfigure failed unexpectedly
      aHolder.Reject(Nothing(), __func__);
      return;
    }

    // Reconfigure was successful
    aHolder.Resolve(false, __func__);
  });
}

PrincipalHandle
SourceListener::GetPrincipalHandle() const
{
  return mPrincipalHandle;
}

DeviceState&
SourceListener::GetDeviceStateFor(TrackID aTrackID) const
{
  // XXX to support multiple tracks of a type in a stream, this should key off
  // the TrackID and not just the type
  switch (aTrackID) {
    case kAudioTrack:
      MOZ_ASSERT(mAudioDeviceState, "No audio device");
      return *mAudioDeviceState;
    case kVideoTrack:
      MOZ_ASSERT(mVideoDeviceState, "No video device");
      return *mVideoDeviceState;
    default:
      MOZ_CRASH("Unknown track id");
  }
}

// Doesn't kill audio
void
GetUserMediaWindowListener::StopSharing()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

  for (auto& source : mActiveListeners) {
    source->StopSharing();
  }
}

void
GetUserMediaWindowListener::StopRawID(const nsString& removedDeviceID)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

  for (auto& source : mActiveListeners) {
    if (source->GetAudioDevice()) {
      nsString id;
      source->GetAudioDevice()->GetRawId(id);
      if (removedDeviceID.Equals(id)) {
        source->StopTrack(kAudioTrack);
      }
    }
    if (source->GetVideoDevice()) {
      nsString id;
      source->GetVideoDevice()->GetRawId(id);
      if (removedDeviceID.Equals(id)) {
        source->StopTrack(kVideoTrack);
      }
    }
  }
}

void
GetUserMediaWindowListener::ChromeAffectingStateChanged()
{
  MOZ_ASSERT(NS_IsMainThread());

  // We wait until stable state before notifying chrome so chrome only does one
  // update if more updates happen in this event loop.

  if (mChromeNotificationTaskPosted) {
    return;
  }

  nsCOMPtr<nsIRunnable> runnable =
    NewRunnableMethod("GetUserMediaWindowListener::NotifyChrome",
                      this,
                      &GetUserMediaWindowListener::NotifyChrome);
  nsContentUtils::RunInStableState(runnable.forget());
  mChromeNotificationTaskPosted = true;
}

void
GetUserMediaWindowListener::NotifyChrome()
{
  MOZ_ASSERT(mChromeNotificationTaskPosted);
  mChromeNotificationTaskPosted = false;

  NS_DispatchToMainThread(NS_NewRunnableFunction("MediaManager::NotifyChrome",
                                                 [windowID = mWindowID]() {
    nsGlobalWindowInner* window =
      nsGlobalWindowInner::GetInnerWindowWithId(windowID);
    if (!window) {
      MOZ_ASSERT_UNREACHABLE("Should have window");
      return;
    }

    nsresult rv = MediaManager::NotifyRecordingStatusChange(window->AsInner());
    if (NS_FAILED(rv)) {
      MOZ_ASSERT_UNREACHABLE("Should be able to notify chrome");
      return;
    }
  }));
}

} // namespace mozilla
