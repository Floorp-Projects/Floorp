/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaManager.h"

#include "AudioCaptureStream.h"
#include "AudioDeviceInfo.h"
#include "AudioStreamTrack.h"
#include "MediaStreamGraphImpl.h"
#include "MediaTimer.h"
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
#include "mozilla/dom/Document.h"
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
#include "mozilla/MozPromise.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Types.h"
#include "mozilla/PeerIdentity.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MediaDevices.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/GetUserMediaRequestBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/Base64.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/media/MediaChild.h"
#include "mozilla/media/MediaTaskUtils.h"
#include "MediaTrackConstraints.h"
#include "VideoStreamTrack.h"
#include "VideoUtils.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
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
#  include "MediaEngineWebRTC.h"
#  include "browser_logging/WebRtcLog.h"
#  include "webrtc/modules/audio_processing/include/audio_processing.h"
#endif

#if defined(XP_WIN)
#  include "mozilla/WindowsVersion.h"
#  include <objbase.h>
#  include <winsock2.h>
#  include <iphlpapi.h>
#  include <tchar.h>
#endif

// XXX Workaround for bug 986974 to maintain the existing broken semantics
template <>
struct nsIMediaDevice::COMTypeInfo<mozilla::MediaDevice, void> {
  static const nsIID kIID;
};
const nsIID nsIMediaDevice::COMTypeInfo<mozilla::MediaDevice, void>::kIID =
    NS_IMEDIADEVICE_IID;

// A specialization of nsMainThreadPtrHolder for
// mozilla::dom::CallbackObjectHolder.  See documentation for
// nsMainThreadPtrHolder in nsProxyRelease.h.  This specialization lets us avoid
// wrapping the CallbackObjectHolder into a separate refcounted object.
template <class WebIDLCallbackT, class XPCOMCallbackT>
class nsMainThreadPtrHolder<
    mozilla::dom::CallbackObjectHolder<WebIDLCallbackT, XPCOMCallbackT>>
    final {
  typedef mozilla::dom::CallbackObjectHolder<WebIDLCallbackT, XPCOMCallbackT>
      Holder;

 public:
  nsMainThreadPtrHolder(const char* aName, Holder&& aHolder)
      : mHolder(std::move(aHolder))
#ifndef RELEASE_OR_BETA
        ,
        mName(aName)
#endif
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

 private:
  // We can be released on any thread.
  ~nsMainThreadPtrHolder() {
    if (NS_IsMainThread()) {
      mHolder.Reset();
    } else if (mHolder.GetISupports()) {
      nsCOMPtr<nsIEventTarget> target = do_GetMainThread();
      MOZ_ASSERT(target);
      NS_ProxyRelease(
#ifdef RELEASE_OR_BETA
          nullptr,
#else
          mName,
#endif
          target, mHolder.Forget());
    }
  }

 public:
  Holder* get() {
    // Nobody should be touching the raw pointer off-main-thread.
    if (MOZ_UNLIKELY(!NS_IsMainThread())) {
      NS_ERROR("Can't dereference nsMainThreadPtrHolder off main thread");
      MOZ_CRASH();
    }
    return &mHolder;
  }

  bool operator!() const { return !mHolder; }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsMainThreadPtrHolder<Holder>)

 private:
  // Our holder.
  Holder mHolder;

#ifndef RELEASE_OR_BETA
  const char* mName = nullptr;
#endif

  // Copy constructor and operator= not implemented. Once constructed, the
  // holder is immutable.
  Holder& operator=(const nsMainThreadPtrHolder& aOther) = delete;
  nsMainThreadPtrHolder(const nsMainThreadPtrHolder& aOther) = delete;
};

namespace mozilla {

LazyLogModule gMediaManagerLog("MediaManager");
#define LOG(...) MOZ_LOG(gMediaManagerLog, LogLevel::Debug, (__VA_ARGS__))

class LocalTrackSource;

using dom::CallerType;
using dom::ConstrainDOMStringParameters;
using dom::ConstrainDoubleRange;
using dom::ConstrainLongRange;
using dom::DisplayMediaStreamConstraints;
using dom::Document;
using dom::FeaturePolicyUtils;
using dom::File;
using dom::GetUserMediaRequest;
using dom::MediaDeviceKind;
using dom::MediaDevices;
using dom::MediaSourceEnum;
using dom::MediaStreamConstraints;
using dom::MediaStreamError;
using dom::MediaStreamTrack;
using dom::MediaStreamTrackSource;
using dom::MediaTrackConstraints;
using dom::MediaTrackConstraintSet;
using dom::MediaTrackSettings;
using dom::MozGetUserMediaDevicesSuccessCallback;
using dom::OwningBooleanOrMediaTrackConstraints;
using dom::OwningStringOrStringSequence;
using dom::OwningStringOrStringSequenceOrConstrainDOMStringParameters;
using dom::Promise;
using dom::Sequence;
using dom::UserActivation;
using media::NewRunnableFrom;
using media::NewTaskFrom;
using media::Refcountable;

static Atomic<bool> sHasShutdown;

struct DeviceState {
  DeviceState(RefPtr<MediaDevice> aDevice,
              RefPtr<LocalTrackSource> aTrackSource, bool aOffWhileDisabled)
      : mOffWhileDisabled(aOffWhileDisabled),
        mDevice(std::move(aDevice)),
        mTrackSource(std::move(aTrackSource)) {
    MOZ_ASSERT(mDevice);
    MOZ_ASSERT(mTrackSource);
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

  // The MediaStreamTrackSource for any tracks (original and clones) originating
  // from this device. Always non-null. Threadsafe access, but see method
  // declarations for individual constraints.
  const RefPtr<LocalTrackSource> mTrackSource;
};

/**
 * This mimics the capture state from nsIMediaManagerService.
 */
enum class CaptureState : uint16_t {
  Off = nsIMediaManagerService::STATE_NOCAPTURE,
  Enabled = nsIMediaManagerService::STATE_CAPTURE_ENABLED,
  Disabled = nsIMediaManagerService::STATE_CAPTURE_DISABLED,
};

static CaptureState CombineCaptureState(CaptureState aFirst,
                                        CaptureState aSecond) {
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

static uint16_t FromCaptureState(CaptureState aState) {
  MOZ_ASSERT(aState == CaptureState::Off || aState == CaptureState::Enabled ||
             aState == CaptureState::Disabled);
  return static_cast<uint16_t>(aState);
}

void MediaManager::CallOnError(GetUserMediaErrorCallback& aCallback,
                               MediaStreamError& aError) {
  aCallback.Call(aError);
}

void MediaManager::CallOnSuccess(GetUserMediaSuccessCallback& aCallback,
                                 DOMMediaStream& aStream) {
  aCallback.Call(aStream);
}

/**
 * SourceListener has threadsafe refcounting for use across the main, media and
 * MSG threads. But it has a non-threadsafe SupportsWeakPtr for WeakPtr usage
 * only from main thread, to ensure that garbage- and cycle-collected objects
 * don't hold a reference to it during late shutdown.
 */
class SourceListener : public SupportsWeakPtr<SourceListener> {
 public:
  typedef MozPromise<bool /* aIgnored */, RefPtr<MediaMgrError>, true>
      SourceListenerPromise;

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(SourceListener)
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION_AND_RECORDING(
      SourceListener, recordreplay::Behavior::Preserve)

  SourceListener();

  /**
   * Registers this source listener as belonging to the given window listener.
   */
  void Register(GetUserMediaWindowListener* aListener);

  /**
   * Marks this listener as active and adds itself as a listener to aStream.
   */
  void Activate(RefPtr<MediaDevice> aAudioDevice,
                RefPtr<LocalTrackSource> aAudioTrackSource,
                RefPtr<MediaDevice> aVideoDevice,
                RefPtr<LocalTrackSource> aVideoTrackSource);

  /**
   * Posts a task to initialize and start all associated devices.
   */
  RefPtr<SourceListenerPromise> InitializeAsync();

  /**
   * Stops all live tracks, finishes the associated MediaStream and cleans up
   * the weak reference to the associated window listener.
   * This will also tell the window listener to remove its hard reference to
   * this SourceListener, so any caller will need to keep its own hard ref.
   */
  void Stop();

  /**
   * Posts a task to stop the device associated with aTrackID and notifies the
   * associated window listener that a track was stopped.
   * Should this track be the last live one to be stopped, we'll also call Stop.
   * This might tell the window listener to remove its hard reference to this
   * SourceListener, so any caller will need to keep its own hard ref.
   */
  void StopTrack(TrackID aTrackID);

  /**
   * Gets the main thread MediaTrackSettings from the MediaEngineSource
   * associated with aTrackID.
   */
  void GetSettingsFor(TrackID aTrackID, MediaTrackSettings& aOutSettings) const;

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

  MediaDevice* GetAudioDevice() const {
    return mAudioDeviceState ? mAudioDeviceState->mDevice.get() : nullptr;
  }

  MediaDevice* GetVideoDevice() const {
    return mVideoDeviceState ? mVideoDeviceState->mDevice.get() : nullptr;
  }

  bool Activated() const { return mAudioDeviceState || mVideoDeviceState; }

  bool Stopped() const { return mStopped; }

  bool CapturingVideo() const;

  bool CapturingAudio() const;

  CaptureState CapturingSource(MediaSourceEnum aSource) const;

  RefPtr<SourceListenerPromise> ApplyConstraintsToTrack(
      TrackID aTrackID, const MediaTrackConstraints& aConstraints,
      CallerType aCallerType);

  PrincipalHandle GetPrincipalHandle() const;

 private:
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
};

/**
 * This class represents a WindowID and handles all MediaStreamTrackListeners
 * (here subclassed as SourceListeners) used to feed GetUserMedia source
 * streams. It proxies feedback from them into messages for browser chrome.
 * The SourceListeners are used to Start() and Stop() the underlying
 * MediaEngineSource when MediaStreams are assigned and deassigned in content.
 */
class GetUserMediaWindowListener {
  friend MediaManager;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GetUserMediaWindowListener)

  // Create in an inactive state
  GetUserMediaWindowListener(base::Thread* aThread, uint64_t aWindowID,
                             const PrincipalHandle& aPrincipalHandle)
      : mMediaThread(aThread),
        mWindowID(aWindowID),
        mPrincipalHandle(aPrincipalHandle),
        mChromeNotificationTaskPosted(false) {}

  /**
   * Registers an inactive gUM source listener for this WindowListener.
   */
  void Register(RefPtr<SourceListener> aListener) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aListener);
    MOZ_ASSERT(!aListener->Activated());
    MOZ_ASSERT(!mInactiveListeners.Contains(aListener), "Already registered");
    MOZ_ASSERT(!mActiveListeners.Contains(aListener), "Already activated");

    aListener->Register(this);
    mInactiveListeners.AppendElement(std::move(aListener));
  }

  /**
   * Activates an already registered and inactive gUM source listener for this
   * WindowListener.
   */
  void Activate(RefPtr<SourceListener> aListener,
                RefPtr<MediaDevice> aAudioDevice,
                RefPtr<LocalTrackSource> aAudioTrackSource,
                RefPtr<MediaDevice> aVideoDevice,
                RefPtr<LocalTrackSource> aVideoTrackSource) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aListener);
    MOZ_ASSERT(!aListener->Activated());
    MOZ_ASSERT(mInactiveListeners.Contains(aListener),
               "Must be registered to activate");
    MOZ_ASSERT(!mActiveListeners.Contains(aListener), "Already activated");

    mInactiveListeners.RemoveElement(aListener);
    aListener->Activate(std::move(aAudioDevice), std::move(aAudioTrackSource),
                        std::move(aVideoDevice), std::move(aVideoTrackSource));
    mActiveListeners.AppendElement(std::move(aListener));
  }

  /**
   * Removes all SourceListeners from this window listener.
   * Removes this window listener from the list of active windows, so callers
   * need to make sure to hold a strong reference.
   */
  void RemoveAll() {
    MOZ_ASSERT(NS_IsMainThread());

    for (auto& l : nsTArray<RefPtr<SourceListener>>(mInactiveListeners)) {
      Remove(l);
    }
    for (auto& l : nsTArray<RefPtr<SourceListener>>(mActiveListeners)) {
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
        auto req = MakeRefPtr<GetUserMediaRequest>(
            globalWindow, VoidString(), VoidString(),
            UserActivation::IsHandlingUserInput());
        obs->NotifyObservers(req, "recording-device-stopped", nullptr);
      }
      return;
    }

    MOZ_ASSERT(windowListener == this,
               "There should only be one window listener per window ID");

    LOG("GUMWindowListener %p removing windowID %" PRIu64, this, mWindowID);
    mgr->RemoveWindowID(mWindowID);
  }

  /**
   * Removes a listener from our lists. Safe to call without holding a hard
   * reference. That said, you'll still want to iterate on a copy of said lists,
   * if you end up calling this method (or methods that may call this method) in
   * the loop, to avoid inadvertently skipping members.
   */
  bool Remove(RefPtr<SourceListener> aListener) {
    // We refcount aListener on entry since we're going to proxy-release it
    // below to prevent the refcount going to zero on callers who might be
    // inside the listener, but operating without a hard reference to self.
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

    LOG("GUMWindowListener %p stopping SourceListener %p.", this,
        aListener.get());
    aListener->Stop();

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
        auto* window = nsGlobalWindowInner::GetInnerWindowWithId(mWindowID);
        auto req = MakeRefPtr<GetUserMediaRequest>(
            window, removedRawId, removedSourceType,
            UserActivation::IsHandlingUserInput());
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
        auto* window = nsGlobalWindowInner::GetInnerWindowWithId(mWindowID);
        auto req = MakeRefPtr<GetUserMediaRequest>(
            window, removedRawId, removedSourceType,
            UserActivation::IsHandlingUserInput());
        obs->NotifyObservers(req, "recording-device-stopped", nullptr);
      }
    }
    if (mInactiveListeners.Length() == 0 && mActiveListeners.Length() == 0) {
      LOG("GUMWindowListener %p Removed last SourceListener. Cleaning up.",
          this);
      RemoveAll();
    }
    nsCOMPtr<nsIEventTarget> mainTarget = do_GetMainThread();
    // To allow being invoked by callers not holding a strong reference to self,
    // hold the listener alive until the stack has unwound, by always
    // dispatching a runnable (aAlwaysProxy = true)
    NS_ProxyRelease(__func__, mainTarget, aListener.forget(), true);
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

  bool CapturingVideo() const {
    MOZ_ASSERT(NS_IsMainThread());
    for (auto& l : mActiveListeners) {
      if (l->CapturingVideo()) {
        return true;
      }
    }
    return false;
  }

  bool CapturingAudio() const {
    MOZ_ASSERT(NS_IsMainThread());
    for (auto& l : mActiveListeners) {
      if (l->CapturingAudio()) {
        return true;
      }
    }
    return false;
  }

  CaptureState CapturingSource(MediaSourceEnum aSource) const {
    MOZ_ASSERT(NS_IsMainThread());
    CaptureState result = CaptureState::Off;
    for (auto& l : mActiveListeners) {
      result = CombineCaptureState(result, l->CapturingSource(aSource));
    }
    return result;
  }

  uint64_t WindowID() const { return mWindowID; }

  PrincipalHandle GetPrincipalHandle() const { return mPrincipalHandle; }

 private:
  ~GetUserMediaWindowListener() {
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

class LocalTrackSource : public MediaStreamTrackSource {
 public:
  LocalTrackSource(nsIPrincipal* aPrincipal, const nsString& aLabel,
                   const RefPtr<SourceListener>& aListener,
                   MediaSourceEnum aSource, MediaStream* aStream,
                   TrackID aTrackID, RefPtr<PeerIdentity> aPeerIdentity)
      : MediaStreamTrackSource(aPrincipal, aLabel),
        mSource(aSource),
        mStream(aStream),
        mTrackID(aTrackID),
        mPeerIdentity(std::move(aPeerIdentity)),
        mListener(aListener.get()) {}

  MediaSourceEnum GetMediaSource() const override { return mSource; }

  const PeerIdentity* GetPeerIdentity() const override { return mPeerIdentity; }

  RefPtr<MediaStreamTrackSource::ApplyConstraintsPromise> ApplyConstraints(
      const MediaTrackConstraints& aConstraints,
      CallerType aCallerType) override {
    MOZ_ASSERT(NS_IsMainThread());
    if (sHasShutdown || !mListener) {
      // Track has been stopped, or we are in shutdown. In either case
      // there's no observable outcome, so pretend we succeeded.
      return MediaStreamTrackSource::ApplyConstraintsPromise::CreateAndResolve(
          false, __func__);
    }
    return mListener->ApplyConstraintsToTrack(mTrackID, aConstraints,
                                              aCallerType);
  }

  void GetSettings(MediaTrackSettings& aOutSettings) override {
    if (mListener) {
      mListener->GetSettingsFor(mTrackID, aOutSettings);
    }
  }

  void Stop() override {
    if (mListener) {
      mListener->StopTrack(mTrackID);
      mListener = nullptr;
    }
    if (!mStream->IsDestroyed()) {
      mStream->Destroy();
    }
  }

  void Disable() override {
    if (mListener) {
      mListener->SetEnabledFor(mTrackID, false);
    }
  }

  void Enable() override {
    if (mListener) {
      mListener->SetEnabledFor(mTrackID, true);
    }
  }

  const MediaSourceEnum mSource;
  const RefPtr<MediaStream> mStream;
  const TrackID mTrackID;
  const RefPtr<const PeerIdentity> mPeerIdentity;

 protected:
  ~LocalTrackSource() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mStream->IsDestroyed());
  }

  // This is a weak pointer to avoid having the SourceListener (which may
  // have references to threads and threadpools) kept alive by DOM-objects
  // that may have ref-cycles and thus are released very late during
  // shutdown, even after xpcom-shutdown-threads. See bug 1351655 for what
  // can happen.
  WeakPtr<SourceListener> mListener;
};

class AudioCaptureTrackSource : public LocalTrackSource {
 public:
  AudioCaptureTrackSource(nsIPrincipal* aPrincipal, nsPIDOMWindowInner* aWindow,
                          const nsString& aLabel,
                          AudioCaptureStream* aAudioCaptureStream,
                          RefPtr<PeerIdentity> aPeerIdentity)
      : LocalTrackSource(aPrincipal, aLabel, nullptr,
                         MediaSourceEnum::AudioCapture, aAudioCaptureStream,
                         kAudioTrack, std::move(aPeerIdentity)),
        mWindow(aWindow),
        mAudioCaptureStream(aAudioCaptureStream) {
    mAudioCaptureStream->Start();
    mAudioCaptureStream->Graph()->RegisterCaptureStreamForWindow(
        mWindow->WindowID(), mAudioCaptureStream);
    mWindow->SetAudioCapture(true);
  }

  void Stop() override {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mAudioCaptureStream->IsDestroyed()) {
      MOZ_ASSERT(mWindow);
      mWindow->SetAudioCapture(false);
      mAudioCaptureStream->Graph()->UnregisterCaptureStreamForWindow(
          mWindow->WindowID());
      mWindow = nullptr;
    }
    // LocalTrackSource destroys the stream.
    LocalTrackSource::Stop();
    MOZ_ASSERT(mAudioCaptureStream->IsDestroyed());
  }

  ProcessedMediaStream* InputStream() const {
    return mAudioCaptureStream.get();
  }

 protected:
  ~AudioCaptureTrackSource() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mAudioCaptureStream->IsDestroyed());
  }

  RefPtr<nsPIDOMWindowInner> mWindow;
  const RefPtr<AudioCaptureStream> mAudioCaptureStream;
};

/**
 * nsIMediaDevice implementation.
 */
NS_IMPL_ISUPPORTS(MediaDevice, nsIMediaDevice)

MediaDevice::MediaDevice(const RefPtr<MediaEngineSource>& aSource,
                         const nsString& aName, const nsString& aID,
                         const nsString& aGroupID, const nsString& aRawID)
    : mSource(aSource),
      mSinkInfo(nullptr),
      mKind((mSource && MediaEngineSource::IsVideo(mSource->GetMediaSource()))
                ? MediaDeviceKind::Videoinput
                : MediaDeviceKind::Audioinput),
      mScary(mSource->GetScary()),
      mIsFake(mSource->IsFake()),
      mType(NS_ConvertUTF8toUTF16(
          dom::MediaDeviceKindValues::strings[uint32_t(mKind)].value)),
      mName(aName),
      mID(aID),
      mGroupID(aGroupID),
      mRawID(aRawID) {
  MOZ_ASSERT(mSource);
}

MediaDevice::MediaDevice(const RefPtr<AudioDeviceInfo>& aAudioDeviceInfo,
                         const nsString& aID, const nsString& aGroupID,
                         const nsString& aRawID)
    : mSource(nullptr),
      mSinkInfo(aAudioDeviceInfo),
      mKind(mSinkInfo->Type() == AudioDeviceInfo::TYPE_INPUT
                ? MediaDeviceKind::Audioinput
                : MediaDeviceKind::Audiooutput),
      mScary(false),
      mIsFake(false),
      mType(NS_ConvertUTF8toUTF16(
          dom::MediaDeviceKindValues::strings[uint32_t(mKind)].value)),
      mName(mSinkInfo->Name()),
      mID(aID),
      mGroupID(aGroupID),
      mRawID(aRawID) {
  // For now this ctor is used only for Audiooutput.
  // It could be used for Audioinput and Videoinput
  // when we do not instantiate a MediaEngineSource
  // during EnumerateDevices.
  MOZ_ASSERT(mKind == MediaDeviceKind::Audiooutput);
  MOZ_ASSERT(mSinkInfo);
}

MediaDevice::MediaDevice(const RefPtr<MediaDevice>& aOther, const nsString& aID,
                         const nsString& aGroupID, const nsString& aRawID)
    : mSource(aOther->mSource),
      mSinkInfo(aOther->mSinkInfo),
      mKind(aOther->mKind),
      mScary(aOther->mScary),
      mIsFake(aOther->mIsFake),
      mType(aOther->mType),
      mName(aOther->mName),
      mID(aID),
      mGroupID(aGroupID),
      mRawID(aRawID) {
  MOZ_ASSERT(aOther);
}

/**
 * Helper functions that implement the constraints algorithm from
 * http://dev.w3.org/2011/webrtc/editor/getusermedia.html#methods-5
 */

/* static */
bool MediaDevice::StringsContain(const OwningStringOrStringSequence& aStrings,
                                 nsString aN) {
  return aStrings.IsString() ? aStrings.GetAsString() == aN
                             : aStrings.GetAsStringSequence().Contains(aN);
}

/* static */
uint32_t MediaDevice::FitnessDistance(
    nsString aN, const ConstrainDOMStringParameters& aParams) {
  if (aParams.mExact.WasPassed() &&
      !StringsContain(aParams.mExact.Value(), aN)) {
    return UINT32_MAX;
  }
  if (aParams.mIdeal.WasPassed() &&
      !StringsContain(aParams.mIdeal.Value(), aN)) {
    return 1;
  }
  return 0;
}

// Binding code doesn't templatize well...

/* static */
uint32_t MediaDevice::FitnessDistance(
    nsString aN,
    const OwningStringOrStringSequenceOrConstrainDOMStringParameters&
        aConstraint) {
  if (aConstraint.IsString()) {
    ConstrainDOMStringParameters params;
    params.mIdeal.Construct();
    params.mIdeal.Value().SetAsString() = aConstraint.GetAsString();
    return FitnessDistance(aN, params);
  } else if (aConstraint.IsStringSequence()) {
    ConstrainDOMStringParameters params;
    params.mIdeal.Construct();
    params.mIdeal.Value().SetAsStringSequence() =
        aConstraint.GetAsStringSequence();
    return FitnessDistance(aN, params);
  } else {
    return FitnessDistance(aN, aConstraint.GetAsConstrainDOMStringParameters());
  }
}

uint32_t MediaDevice::GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
    bool aIsChrome) {
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);

  const nsString& id = aIsChrome ? mRawID : mID;
  auto type = GetMediaSource();
  uint64_t distance = 0;
  if (!aConstraintSets.IsEmpty()) {
    if (aIsChrome /* For the screen/window sharing preview */ ||
        type == MediaSourceEnum::Camera ||
        type == MediaSourceEnum::Microphone) {
      distance += uint64_t(MediaConstraintsHelper::FitnessDistance(
                      Some(id), aConstraintSets[0]->mDeviceId)) +
                  uint64_t(MediaConstraintsHelper::FitnessDistance(
                      Some(mGroupID), aConstraintSets[0]->mGroupId));
    }
  }
  if (distance < UINT32_MAX) {
    // Forward request to underlying object to interrogate per-mode
    // capabilities.
    distance += mSource->GetBestFitnessDistance(aConstraintSets);
  }
  return std::min<uint64_t>(distance, UINT32_MAX);
}

NS_IMETHODIMP
MediaDevice::GetName(nsAString& aName) {
  MOZ_ASSERT(NS_IsMainThread());
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
MediaDevice::GetType(nsAString& aType) {
  MOZ_ASSERT(NS_IsMainThread());
  aType.Assign(mType);
  return NS_OK;
}

NS_IMETHODIMP
MediaDevice::GetId(nsAString& aID) {
  MOZ_ASSERT(NS_IsMainThread());
  aID.Assign(mID);
  return NS_OK;
}

NS_IMETHODIMP
MediaDevice::GetRawId(nsAString& aID) {
  MOZ_ASSERT(NS_IsMainThread());
  aID.Assign(mRawID);
  return NS_OK;
}

NS_IMETHODIMP
MediaDevice::GetGroupId(nsAString& aGroupID) {
  MOZ_ASSERT(NS_IsMainThread());
  aGroupID.Assign(mGroupID);
  return NS_OK;
}

NS_IMETHODIMP
MediaDevice::GetScary(bool* aScary) {
  *aScary = mScary;
  return NS_OK;
}

void MediaDevice::GetSettings(MediaTrackSettings& aOutSettings) const {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mSource);
  mSource->GetSettings(aOutSettings);
}

// Threadsafe since mSource is const.
NS_IMETHODIMP
MediaDevice::GetMediaSource(nsAString& aMediaSource) {
  aMediaSource.Assign(NS_ConvertUTF8toUTF16(
      dom::MediaSourceEnumValues::strings[uint32_t(GetMediaSource())].value));
  return NS_OK;
}

nsresult MediaDevice::Allocate(const MediaTrackConstraints& aConstraints,
                               const MediaEnginePrefs& aPrefs,
                               const ipc::PrincipalInfo& aPrincipalInfo,
                               const char** aOutBadConstraint) {
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);

  // Mock failure for automated tests.
  if (mIsFake && aConstraints.mDeviceId.WasPassed() &&
      aConstraints.mDeviceId.Value().IsString() &&
      aConstraints.mDeviceId.Value().GetAsString().EqualsASCII("bad device")) {
    return NS_ERROR_FAILURE;
  }

  return mSource->Allocate(aConstraints, aPrefs, aPrincipalInfo,
                           aOutBadConstraint);
}

void MediaDevice::SetTrack(const RefPtr<SourceMediaStream>& aStream,
                           TrackID aTrackID,
                           const PrincipalHandle& aPrincipalHandle) {
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);
  mSource->SetTrack(aStream, aTrackID, aPrincipalHandle);
}

nsresult MediaDevice::Start() {
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);
  return mSource->Start();
}

nsresult MediaDevice::Reconfigure(const MediaTrackConstraints& aConstraints,
                                  const MediaEnginePrefs& aPrefs,
                                  const char** aOutBadConstraint) {
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);
  auto type = GetMediaSource();
  if (type == MediaSourceEnum::Camera || type == MediaSourceEnum::Microphone) {
    NormalizedConstraints c(aConstraints);
    if (MediaConstraintsHelper::FitnessDistance(Some(mID), c.mDeviceId) ==
        UINT32_MAX) {
      *aOutBadConstraint = "deviceId";
      return NS_ERROR_INVALID_ARG;
    }
    if (MediaConstraintsHelper::FitnessDistance(Some(mGroupID), c.mGroupId) ==
        UINT32_MAX) {
      *aOutBadConstraint = "groupId";
      return NS_ERROR_INVALID_ARG;
    }
  }
  return mSource->Reconfigure(aConstraints, aPrefs, aOutBadConstraint);
}

nsresult MediaDevice::FocusOnSelectedSource() {
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);
  return mSource->FocusOnSelectedSource();
}

nsresult MediaDevice::Stop() {
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);
  return mSource->Stop();
}

nsresult MediaDevice::Deallocate() {
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);
  return mSource->Deallocate();
}

MediaSourceEnum MediaDevice::GetMediaSource() const {
  // Threadsafe because mSource is const. GetMediaSource() might have other
  // requirements.
  MOZ_ASSERT(mSource);
  return mSource->GetMediaSource();
}

static bool IsOn(const OwningBooleanOrMediaTrackConstraints& aUnion) {
  return !aUnion.IsBoolean() || aUnion.GetAsBoolean();
}

static const MediaTrackConstraints& GetInvariant(
    const OwningBooleanOrMediaTrackConstraints& aUnion) {
  static const MediaTrackConstraints empty;
  return aUnion.IsMediaTrackConstraints() ? aUnion.GetAsMediaTrackConstraints()
                                          : empty;
}

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
class GetUserMediaStreamRunnable : public Runnable {
 public:
  GetUserMediaStreamRunnable(
      MozPromiseHolder<MediaManager::StreamPromise>&& aHolder,
      uint64_t aWindowID, RefPtr<GetUserMediaWindowListener> aWindowListener,
      RefPtr<SourceListener> aSourceListener,
      const ipc::PrincipalInfo& aPrincipalInfo,
      const MediaStreamConstraints& aConstraints,
      RefPtr<MediaDevice> aAudioDevice, RefPtr<MediaDevice> aVideoDevice,
      RefPtr<PeerIdentity> aPeerIdentity, bool aIsChrome)
      : Runnable("GetUserMediaStreamRunnable"),
        mHolder(std::move(aHolder)),
        mConstraints(aConstraints),
        mAudioDevice(std::move(aAudioDevice)),
        mVideoDevice(std::move(aVideoDevice)),
        mWindowID(aWindowID),
        mWindowListener(std::move(aWindowListener)),
        mSourceListener(std::move(aSourceListener)),
        mPrincipalInfo(aPrincipalInfo),
        mPeerIdentity(std::move(aPeerIdentity)),
        mManager(MediaManager::GetInstance()) {}

  ~GetUserMediaStreamRunnable() {
    mHolder.RejectIfExists(
        MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError), __func__);
  }

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    LOG("GetUserMediaStreamRunnable::Run()");
    nsGlobalWindowInner* window =
        nsGlobalWindowInner::GetInnerWindowWithId(mWindowID);

    // We're on main-thread, and the windowlist can only
    // be invalidated from the main-thread (see OnNavigation)
    if (!mManager->IsWindowListenerStillActive(mWindowListener)) {
      // This window is no longer live. mListener has already been removed.
      return NS_OK;
    }

    MediaStreamGraph::GraphDriverType graphDriverType =
        mAudioDevice ? MediaStreamGraph::AUDIO_THREAD_DRIVER
                     : MediaStreamGraph::SYSTEM_THREAD_DRIVER;
    MediaStreamGraph* msg = MediaStreamGraph::GetInstance(
        graphDriverType, window, MediaStreamGraph::REQUEST_DEFAULT_SAMPLE_RATE);

    auto domStream = MakeRefPtr<DOMMediaStream>(window);
    RefPtr<LocalTrackSource> audioTrackSource;
    RefPtr<LocalTrackSource> videoTrackSource;
    nsCOMPtr<nsIPrincipal> principal;
    if (mPeerIdentity) {
      principal = NullPrincipal::CreateWithInheritedAttributes(
          window->GetExtantDoc()->NodePrincipal());
    } else {
      principal = window->GetExtantDoc()->NodePrincipal();
    }
    RefPtr<GenericNonExclusivePromise> firstFramePromise;
    if (mAudioDevice) {
      if (mAudioDevice->GetMediaSource() == MediaSourceEnum::AudioCapture) {
        // AudioCapture is a special case, here, in the sense that we're not
        // really using the audio source and the SourceMediaStream, which acts
        // as placeholders. We re-route a number of streams internally in the
        // MSG and mix them down instead.
        NS_WARNING(
            "MediaCaptureWindowState doesn't handle "
            "MediaSourceEnum::AudioCapture. This must be fixed with UX "
            "before shipping.");
        auto audioCaptureSource = MakeRefPtr<AudioCaptureTrackSource>(
            principal, window, NS_LITERAL_STRING("Window audio capture"),
            msg->CreateAudioCaptureStream(kAudioTrack), mPeerIdentity);
        audioTrackSource = audioCaptureSource;
        RefPtr<MediaStreamTrack> track =
            new dom::AudioStreamTrack(window, audioCaptureSource->InputStream(),
                                      kAudioTrack, audioCaptureSource);
        domStream->AddTrackInternal(track);
      } else {
        nsString audioDeviceName;
        mAudioDevice->GetName(audioDeviceName);
        RefPtr<MediaStream> stream = msg->CreateSourceStream();
        audioTrackSource = new LocalTrackSource(
            principal, audioDeviceName, mSourceListener,
            mAudioDevice->GetMediaSource(), stream, kAudioTrack, mPeerIdentity);
        MOZ_ASSERT(IsOn(mConstraints.mAudio));
        RefPtr<MediaStreamTrack> track = new dom::AudioStreamTrack(
            window, stream, kAudioTrack, audioTrackSource,
            dom::MediaStreamTrackState::Live,
            GetInvariant(mConstraints.mAudio));
        domStream->AddTrackInternal(track);
      }
    }
    if (mVideoDevice) {
      nsString videoDeviceName;
      mVideoDevice->GetName(videoDeviceName);
      RefPtr<MediaStream> stream = msg->CreateSourceStream();
      videoTrackSource = new LocalTrackSource(
          principal, videoDeviceName, mSourceListener,
          mVideoDevice->GetMediaSource(), stream, kVideoTrack, mPeerIdentity);
      MOZ_ASSERT(IsOn(mConstraints.mVideo));
      RefPtr<MediaStreamTrack> track = new dom::VideoStreamTrack(
          window, stream, kVideoTrack, videoTrackSource,
          dom::MediaStreamTrackState::Live, GetInvariant(mConstraints.mVideo));
      domStream->AddTrackInternal(track);
      switch (mVideoDevice->GetMediaSource()) {
        case MediaSourceEnum::Browser:
        case MediaSourceEnum::Screen:
        case MediaSourceEnum::Window:
          // Wait for first frame for screen-sharing devices, to ensure
          // with and height settings are available immediately, to pass wpt.
          firstFramePromise = mVideoDevice->mSource->GetFirstFramePromise();
          break;
        default:
          break;
      }
    }

    if (!domStream || (!audioTrackSource && !videoTrackSource) ||
        sHasShutdown) {
      LOG("Returning error for getUserMedia() - no stream");

      mHolder.Reject(MakeRefPtr<MediaMgrError>(
                         MediaMgrError::Name::AbortError,
                         sHasShutdown ? NS_LITERAL_STRING("In shutdown")
                                      : NS_LITERAL_STRING("No stream.")),
                     __func__);
      return NS_OK;
    }

    // Activate our source listener. We'll call Start() on the source when we
    // get a callback that the MediaStream has started consuming. The listener
    // is freed when the page is invalidated (on navigation or close).
    mWindowListener->Activate(mSourceListener, mAudioDevice,
                              std::move(audioTrackSource), mVideoDevice,
                              std::move(videoTrackSource));

    // Dispatch to the media thread to ask it to start the sources, because that
    // can take a while.
    mSourceListener->InitializeAsync()
        ->Then(
            GetMainThreadSerialEventTarget(), __func__,
            [manager = mManager, windowListener = mWindowListener,
             firstFramePromise] {
              LOG("GetUserMediaStreamRunnable::Run: starting success callback "
                  "following InitializeAsync()");
              // Initiating and starting devices succeeded.
              windowListener->ChromeAffectingStateChanged();
              manager->SendPendingGUMRequest();
              if (!firstFramePromise) {
                return SourceListener::SourceListenerPromise::CreateAndResolve(
                    true, __func__);
              }
              RefPtr<SourceListener::SourceListenerPromise> resolvePromise =
                  firstFramePromise->Then(
                      GetMainThreadSerialEventTarget(), __func__,
                      [] {
                        return SourceListener::SourceListenerPromise::
                            CreateAndResolve(true, __func__);
                      },
                      [] {
                        return SourceListener::SourceListenerPromise::
                            CreateAndReject(
                                MakeRefPtr<MediaMgrError>(
                                    MediaMgrError::Name::AbortError,
                                    NS_LITERAL_STRING("In shutdown")),
                                __func__);
                      });
              return resolvePromise;
            },
            [](RefPtr<MediaMgrError>&& aError) {
              LOG("GetUserMediaStreamRunnable::Run: starting failure callback "
                  "following InitializeAsync()");
              return SourceListener::SourceListenerPromise::CreateAndReject(
                  aError, __func__);
            })
        ->Then(GetMainThreadSerialEventTarget(), __func__,
               [holder = std::move(mHolder),
                domStream](const SourceListener::SourceListenerPromise::
                               ResolveOrRejectValue& aValue) mutable {
                 if (aValue.IsResolve()) {
                   holder.Resolve(domStream, __func__);
                 } else {
                   holder.Reject(aValue.RejectValue(), __func__);
                 }
               });

    if (!IsPincipalInfoPrivate(mPrincipalInfo)) {
      // Call GetPrincipalKey again, this time w/persist = true, to promote
      // deviceIds to persistent, in case they're not already. Fire'n'forget.
      media::GetPrincipalKey(mPrincipalInfo, true)
          ->Then(GetCurrentThreadSerialEventTarget(), __func__,
                 [](const PrincipalKeyPromise::ResolveOrRejectValue& aValue) {
                   if (aValue.IsReject()) {
                     LOG("Failed get Principal key. Persisting of deviceIds "
                         "will be broken");
                   }
                 });
    }
    return NS_OK;
  }

 private:
  MozPromiseHolder<MediaManager::StreamPromise> mHolder;
  MediaStreamConstraints mConstraints;
  RefPtr<MediaDevice> mAudioDevice;
  RefPtr<MediaDevice> mVideoDevice;
  uint64_t mWindowID;
  RefPtr<GetUserMediaWindowListener> mWindowListener;
  RefPtr<SourceListener> mSourceListener;
  ipc::PrincipalInfo mPrincipalInfo;
  RefPtr<PeerIdentity> mPeerIdentity;
  RefPtr<MediaManager> mManager;  // get ref to this when creating the runnable
};

// Source getter returning full list

static void GetMediaDevices(MediaEngine* aEngine, uint64_t aWindowId,
                            MediaSourceEnum aSrcType,
                            MediaManager::MediaDeviceSet& aResult,
                            const char* aMediaDeviceName = nullptr) {
  MOZ_ASSERT(MediaManager::IsInMediaThread());

  LOG("%s: aEngine=%p, aWindowId=%" PRIu64 ", aSrcType=%" PRIu8
      ", aMediaDeviceName=%s",
      __func__, aEngine, aWindowId, static_cast<uint8_t>(aSrcType),
      aMediaDeviceName ? aMediaDeviceName : "null");
  nsTArray<RefPtr<MediaDevice>> devices;
  aEngine->EnumerateDevices(aWindowId, aSrcType, MediaSinkEnum::Other,
                            &devices);

  /*
   * We're allowing multiple tabs to access the same camera for parity
   * with Chrome.  See bug 811757 for some of the issues surrounding
   * this decision.  To disallow, we'd filter by IsAvailable() as we used
   * to.
   */
  if (aMediaDeviceName && *aMediaDeviceName) {
    for (auto& device : devices) {
      if (device->mName.EqualsASCII(aMediaDeviceName)) {
        aResult.AppendElement(device);
        LOG("%s: found aMediaDeviceName=%s", __func__, aMediaDeviceName);
        break;
      }
    }
  } else {
    aResult = devices;
    if (MOZ_LOG_TEST(gMediaManagerLog, mozilla::LogLevel::Debug)) {
      for (auto& device : devices) {
        LOG("%s: appending device=%s", __func__,
            NS_ConvertUTF16toUTF8(device->mName).get());
      }
    }
  }
}

RefPtr<MediaManager::BadConstraintsPromise> MediaManager::SelectSettings(
    const MediaStreamConstraints& aConstraints, bool aIsChrome,
    const RefPtr<MediaDeviceSetRefCnt>& aSources) {
  MOZ_ASSERT(NS_IsMainThread());

  // Algorithm accesses device capabilities code and must run on media thread.
  // Modifies passed-in aSources.

  return MediaManager::PostTask<BadConstraintsPromise>(
      __func__, [aConstraints, aSources, aIsChrome](
                    MozPromiseHolder<BadConstraintsPromise>& holder) mutable {
        auto& sources = *aSources;

        // Since the advanced part of the constraints algorithm needs to know
        // when a candidate set is overconstrained (zero members), we must split
        // up the list into videos and audios, and put it back together again at
        // the end.

        nsTArray<RefPtr<MediaDevice>> videos;
        nsTArray<RefPtr<MediaDevice>> audios;

        for (auto& source : sources) {
          MOZ_ASSERT(source->mKind == MediaDeviceKind::Videoinput ||
                     source->mKind == MediaDeviceKind::Audioinput);
          if (source->mKind == MediaDeviceKind::Videoinput) {
            videos.AppendElement(source);
          } else if (source->mKind == MediaDeviceKind::Audioinput) {
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
        if (!badConstraint && !needVideo == !videos.Length() &&
            !needAudio == !audios.Length()) {
          for (auto& video : videos) {
            sources.AppendElement(video);
          }
          for (auto& audio : audios) {
            sources.AppendElement(audio);
          }
        }
        holder.Resolve(badConstraint, __func__);
      });
}

/**
 * Runs on a seperate thread and is responsible for enumerating devices.
 * Depending on whether a picture or stream was asked for, either
 * ProcessGetUserMedia is called, and the results are sent back to the DOM.
 *
 * Do not run this on the main thread. The success and error callbacks *MUST*
 * be dispatched on the main thread!
 */
class GetUserMediaTask : public Runnable {
 public:
  GetUserMediaTask(const MediaStreamConstraints& aConstraints,
                   MozPromiseHolder<MediaManager::StreamPromise>&& aHolder,
                   uint64_t aWindowID,
                   RefPtr<GetUserMediaWindowListener> aWindowListener,
                   RefPtr<SourceListener> aSourceListener,
                   const MediaEnginePrefs& aPrefs,
                   const ipc::PrincipalInfo& aPrincipalInfo, bool aIsChrome,
                   RefPtr<MediaManager::MediaDeviceSetRefCnt>&& aMediaDeviceSet,
                   bool aShouldFocusSource)
      : Runnable("GetUserMediaTask"),
        mConstraints(aConstraints),
        mHolder(std::move(aHolder)),
        mWindowID(aWindowID),
        mWindowListener(std::move(aWindowListener)),
        mSourceListener(std::move(aSourceListener)),
        mPrefs(aPrefs),
        mPrincipalInfo(aPrincipalInfo),
        mIsChrome(aIsChrome),
        mShouldFocusSource(aShouldFocusSource),
        mDeviceChosen(false),
        mMediaDeviceSet(aMediaDeviceSet),
        mManager(MediaManager::GetInstance()) {}

  ~GetUserMediaTask() {
    if (!mHolder.IsEmpty()) {
      Fail(MediaMgrError::Name::NotAllowedError);
    }
  }

  void Fail(MediaMgrError::Name aName, const nsString& aMessage = EmptyString(),
            const nsString& aConstraint = EmptyString()) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "GetUserMediaTask::Fail",
        [aName, aMessage, aConstraint, holder = std::move(mHolder)]() mutable {
          holder.Reject(MakeRefPtr<MediaMgrError>(aName, aMessage, aConstraint),
                        __func__);
        }));
    // Do after the above runs, as it checks active window list
    NS_DispatchToMainThread(NewRunnableMethod<RefPtr<SourceListener>>(
        "GetUserMediaWindowListener::Remove", mWindowListener,
        &GetUserMediaWindowListener::Remove, mSourceListener));
  }

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mDeviceChosen);
    LOG("GetUserMediaTask::Run()");

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
              LOG("FocusOnSelectedSource failed");
            }
          }
        }
      }
    }
    if (errorMsg) {
      LOG("%s %" PRIu32, errorMsg, static_cast<uint32_t>(rv));
      if (badConstraint) {
        Fail(MediaMgrError::Name::OverconstrainedError, NS_LITERAL_STRING(""),
             NS_ConvertUTF8toUTF16(badConstraint));
      } else {
        Fail(MediaMgrError::Name::NotReadableError,
             NS_ConvertUTF8toUTF16(errorMsg));
      }
      NS_DispatchToMainThread(
          NS_NewRunnableFunction("MediaManager::SendPendingGUMRequest", []() {
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

    NS_DispatchToMainThread(do_AddRef(new GetUserMediaStreamRunnable(
        std::move(mHolder), mWindowID, mWindowListener, mSourceListener,
        mPrincipalInfo, mConstraints, mAudioDevice, mVideoDevice, peerIdentity,
        mIsChrome)));
    return NS_OK;
  }

  nsresult Denied(MediaMgrError::Name aName,
                  const nsString& aMessage = EmptyString()) {
    // We add a disabled listener to the StreamListeners array until accepted
    // If this was the only active MediaStream, remove the window from the list.
    if (NS_IsMainThread()) {
      mHolder.Reject(MakeRefPtr<MediaMgrError>(aName, aMessage), __func__);
      // Should happen *after* error runs for consistency, but may not matter
      mWindowListener->Remove(mSourceListener);
    } else {
      // This will re-check the window being alive on main-thread
      Fail(aName, aMessage);
    }
    return NS_OK;
  }

  nsresult SetContraints(const MediaStreamConstraints& aConstraints) {
    mConstraints = aConstraints;
    return NS_OK;
  }

  const MediaStreamConstraints& GetConstraints() { return mConstraints; }

  nsresult SetAudioDevice(RefPtr<MediaDevice> aAudioDevice) {
    mAudioDevice = std::move(aAudioDevice);
    mDeviceChosen = true;
    return NS_OK;
  }

  nsresult SetVideoDevice(RefPtr<MediaDevice> aVideoDevice) {
    mVideoDevice = std::move(aVideoDevice);
    mDeviceChosen = true;
    return NS_OK;
  }

  uint64_t GetWindowID() { return mWindowID; }

 private:
  MediaStreamConstraints mConstraints;

  MozPromiseHolder<MediaManager::StreamPromise> mHolder;
  uint64_t mWindowID;
  RefPtr<GetUserMediaWindowListener> mWindowListener;
  RefPtr<SourceListener> mSourceListener;
  RefPtr<MediaDevice> mAudioDevice;
  RefPtr<MediaDevice> mVideoDevice;
  const MediaEnginePrefs mPrefs;
  ipc::PrincipalInfo mPrincipalInfo;
  bool mIsChrome;
  bool mShouldFocusSource;

  bool mDeviceChosen;

 public:
  RefPtr<MediaManager::MediaDeviceSetRefCnt> mMediaDeviceSet;

 private:
  RefPtr<MediaManager> mManager;  // get ref to this when creating the runnable
};

#if defined(ANDROID)
class GetUserMediaRunnableWrapper : public Runnable {
 public:
  // This object must take ownership of task
  explicit GetUserMediaRunnableWrapper(GetUserMediaTask* task)
      : Runnable("GetUserMediaRunnableWrapper"), mTask(task) {}

  ~GetUserMediaRunnableWrapper() {}

  NS_IMETHOD Run() override {
    mTask->Run();
    return NS_OK;
  }

 private:
  nsAutoPtr<GetUserMediaTask> mTask;
};
#endif

/* static */
void MediaManager::GuessVideoDeviceGroupIDs(MediaDeviceSet& aDevices,
                                            const MediaDeviceSet& aAudios) {
  // Run the logic in a lambda to avoid duplication.
  auto updateGroupIdIfNeeded = [&](RefPtr<MediaDevice>& aVideo,
                                   const MediaDeviceKind aKind) -> bool {
    MOZ_ASSERT(aVideo->mKind == MediaDeviceKind::Videoinput);
    MOZ_ASSERT(aKind == MediaDeviceKind::Audioinput ||
               aKind == MediaDeviceKind::Audiooutput);
    // This will store the new group id if a match is found.
    nsString newVideoGroupID;
    // If the group id needs to be updated this will become true. It is
    // necessary when the new group id is an empty string. Without this extra
    // variable to signal the update, we would resort to test if
    // `newVideoGroupId` is empty. However,
    // that check does not work when the new group id is an empty string.
    bool updateGroupId = false;
    for (const RefPtr<MediaDevice>& dev : aAudios) {
      if (dev->mKind != aKind) {
        continue;
      }
      if (!FindInReadable(aVideo->mName, dev->mName)) {
        continue;
      }
      if (newVideoGroupID.IsEmpty()) {
        // This is only expected on first match. If that's the only match group
        // id will be updated to this one at the end of the loop.
        updateGroupId = true;
        newVideoGroupID = dev->mGroupID;
      } else {
        // More than one device found, it is impossible to know which group id
        // is the correct one.
        updateGroupId = false;
        newVideoGroupID = NS_LITERAL_STRING("");
        break;
      }
    }
    if (updateGroupId) {
      aVideo =
          new MediaDevice(aVideo, aVideo->mID, newVideoGroupID, aVideo->mRawID);
      return true;
    }
    return false;
  };

  for (RefPtr<MediaDevice>& video : aDevices) {
    if (video->mKind != MediaDeviceKind::Videoinput) {
      continue;
    }
    if (updateGroupIdIfNeeded(video, MediaDeviceKind::Audioinput)) {
      // GroupId has been updated, continue to the next video device
      continue;
    }
    // GroupId has not been updated, check among the outputs
    updateGroupIdIfNeeded(video, MediaDeviceKind::Audiooutput);
  }
}

/**
 * EnumerateRawDevices - Enumerate a list of audio & video devices that
 * satisfy passed-in constraints. List contains raw id's.
 */

RefPtr<MediaManager::MgrPromise> MediaManager::EnumerateRawDevices(
    uint64_t aWindowId, MediaSourceEnum aVideoInputType,
    MediaSourceEnum aAudioInputType, MediaSinkEnum aAudioOutputType,
    DeviceEnumerationType aVideoInputEnumType,
    DeviceEnumerationType aAudioInputEnumType, bool aForceNoPermRequest,
    const RefPtr<MediaDeviceSetRefCnt>& aOutDevices) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aVideoInputType != MediaSourceEnum::Other ||
             aAudioInputType != MediaSourceEnum::Other ||
             aAudioOutputType != MediaSinkEnum::Other);
  // Since the enums can take one of several values, the following asserts rely
  // on short circuting behavior. E.g. aVideoInputEnumType != Fake will be true
  // if the requested device is not fake and thus the assert will pass. However,
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

  LOG("%s: aWindowId=%" PRIu64 ", aVideoInputType=%" PRIu8
      ", aAudioInputType=%" PRIu8 ", aVideoInputEnumType=%" PRIu8
      ", aAudioInputEnumType=%" PRIu8,
      __func__, aWindowId, static_cast<uint8_t>(aVideoInputType),
      static_cast<uint8_t>(aAudioInputType),
      static_cast<uint8_t>(aVideoInputEnumType),
      static_cast<uint8_t>(aAudioInputEnumType));

  auto holder = MakeUnique<MozPromiseHolder<MgrPromise>>();
  RefPtr<MgrPromise> promise = holder->Ensure(__func__);

  bool hasVideo = aVideoInputType != MediaSourceEnum::Other;
  bool hasAudio = aAudioInputType != MediaSourceEnum::Other;
  bool hasAudioOutput = aAudioOutputType == MediaSinkEnum::Speaker;

  // True of at least one of video input or audio input is a fake device
  bool fakeDeviceRequested =
      (aVideoInputEnumType == DeviceEnumerationType::Fake && hasVideo) ||
      (aAudioInputEnumType == DeviceEnumerationType::Fake && hasAudio);
  // True if at least one of video input or audio input is a real device
  // or there is audio output.
  bool realDeviceRequested =
      (aVideoInputEnumType != DeviceEnumerationType::Fake && hasVideo) ||
      (aAudioInputEnumType != DeviceEnumerationType::Fake && hasAudio) ||
      hasAudioOutput;

  nsAutoCString videoLoopDev, audioLoopDev;
  if (hasVideo && aVideoInputEnumType == DeviceEnumerationType::Loopback) {
    Preferences::GetCString("media.video_loopback_dev", videoLoopDev);
  }
  if (hasAudio && aAudioInputEnumType == DeviceEnumerationType::Loopback) {
    Preferences::GetCString("media.audio_loopback_dev", audioLoopDev);
  }

  RefPtr<Runnable> task = NewTaskFrom([holder = std::move(holder), aWindowId,
                                       aVideoInputType, aAudioInputType,
                                       aVideoInputEnumType, aAudioInputEnumType,
                                       videoLoopDev, audioLoopDev, hasVideo,
                                       hasAudio, hasAudioOutput,
                                       fakeDeviceRequested, realDeviceRequested,
                                       aOutDevices]() {
    // Only enumerate what's asked for, and only fake cams and mics.
    RefPtr<MediaEngine> fakeBackend, realBackend;
    if (fakeDeviceRequested) {
      fakeBackend = new MediaEngineDefault();
    }
    if (realDeviceRequested) {
      MediaManager* manager = MediaManager::GetIfExists();
      MOZ_RELEASE_ASSERT(manager);  // Must exist while media thread is alive
      realBackend = manager->GetBackend();
    }

    RefPtr<MediaEngine> videoBackend;
    RefPtr<MediaEngine> audioBackend;
    Maybe<MediaDeviceSet> micsOfVideoBackend;
    Maybe<MediaDeviceSet> speakers;

    if (hasVideo) {
      videoBackend = aVideoInputEnumType == DeviceEnumerationType::Fake
                         ? fakeBackend
                         : realBackend;
      MediaDeviceSet videos;
      LOG("EnumerateRawDevices Task: Getting video sources with %s backend",
          videoBackend == fakeBackend ? "fake" : "real");
      GetMediaDevices(videoBackend, aWindowId, aVideoInputType, videos,
                      videoLoopDev.get());
      aOutDevices->AppendElements(videos);
    }
    if (hasAudio) {
      audioBackend = aAudioInputEnumType == DeviceEnumerationType::Fake
                         ? fakeBackend
                         : realBackend;
      MediaDeviceSet audios;
      LOG("EnumerateRawDevices Task: Getting audio sources with %s backend",
          audioBackend == fakeBackend ? "fake" : "real");
      GetMediaDevices(audioBackend, aWindowId, aAudioInputType, audios,
                      audioLoopDev.get());
      if (aAudioInputType == MediaSourceEnum::Microphone &&
          audioBackend == videoBackend) {
        micsOfVideoBackend = Some(MediaDeviceSet());
        micsOfVideoBackend->AppendElements(audios);
      }
      aOutDevices->AppendElements(audios);
    }
    if (hasAudioOutput) {
      MediaDeviceSet outputs;
      MOZ_ASSERT(realBackend);
      realBackend->EnumerateDevices(aWindowId, MediaSourceEnum::Other,
                                    MediaSinkEnum::Speaker, &outputs);
      speakers = Some(MediaDeviceSet());
      speakers->AppendElements(outputs);
      aOutDevices->AppendElements(outputs);
    }
    if (hasVideo && aVideoInputType == MediaSourceEnum::Camera) {
      MediaDeviceSet audios;
      LOG("EnumerateRawDevices Task: Getting audio sources with %s backend "
          "for "
          "groupId correlation",
          videoBackend == fakeBackend ? "fake" : "real");
      // We need to correlate cameras with audio groupIds. We use the backend
      // of the camera to always do correlation on devices in the same scope.
      // If we don't do this, video-only getUserMedia will not apply groupId
      // constraints to the same set of groupIds as gets returned by
      // enumerateDevices.
      if (micsOfVideoBackend.isSome()) {
        // Microphones from the same backend used for the cameras have already
        // been enumerated. Avoid doing it again.
        audios.AppendElements(*micsOfVideoBackend);
      } else {
        GetMediaDevices(videoBackend, aWindowId, MediaSourceEnum::Microphone,
                        audios, audioLoopDev.get());
      }
      if (videoBackend == realBackend) {
        // When using the real backend for video, there could also be speakers
        // to correlate with. There are no fake speakers.
        if (speakers.isSome()) {
          // Speakers have already been enumerated. Avoid doing it again.
          audios.AppendElements(*speakers);
        } else {
          realBackend->EnumerateDevices(aWindowId, MediaSourceEnum::Other,
                                        MediaSinkEnum::Speaker, &audios);
        }
      }
      GuessVideoDeviceGroupIDs(*aOutDevices, audios);
    }

    holder->Resolve(false, __func__);
  });

  if (realDeviceRequested && aForceNoPermRequest &&
      Preferences::GetBool("media.navigator.permission.device", false)) {
    // Need to ask permission to retrieve list of all devices;
    // notify frontend observer and wait for callback notification to post task.
    const char16_t* const type =
        (aVideoInputType != MediaSourceEnum::Camera)
            ? u"audio"
            : (aAudioInputType != MediaSourceEnum::Microphone) ? u"video"
                                                               : u"all";
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    obs->NotifyObservers(static_cast<nsIRunnable*>(task),
                         "getUserMedia:ask-device-permission", type);
  } else {
    // Don't need to ask permission to retrieve list of all devices;
    // post the retrieval task immediately.
    MediaManager::PostTask(task.forget());
  }

  return promise;
}

MediaManager::MediaManager() : mMediaThread(nullptr), mBackend(nullptr) {
  mPrefs.mFreq = 1000;  // 1KHz test tone
  mPrefs.mWidth = 0;    // adaptive default
  mPrefs.mHeight = 0;   // adaptive default
  mPrefs.mFPS = MediaEnginePrefs::DEFAULT_VIDEO_FPS;
  mPrefs.mAecOn = false;
  mPrefs.mUseAecMobile = false;
  mPrefs.mAgcOn = false;
  mPrefs.mNoiseOn = false;
  mPrefs.mExtendedFilter = true;
  mPrefs.mDelayAgnostic = true;
  mPrefs.mFakeDeviceChangeEventOn = false;
#ifdef MOZ_WEBRTC
  mPrefs.mAec =
      webrtc::EchoCancellation::SuppressionLevel::kModerateSuppression;
  mPrefs.mAgc = webrtc::GainControl::Mode::kAdaptiveDigital;
  mPrefs.mNoise = webrtc::NoiseSuppression::Level::kModerate;
#else
  mPrefs.mAec = 0;
  mPrefs.mAgc = 0;
  mPrefs.mNoise = 0;
#endif
  mPrefs.mFullDuplex = false;
  mPrefs.mChannels = 0;  // max channels default
  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs =
      do_GetService("@mozilla.org/preferences-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);
    if (branch) {
      GetPrefs(branch, nullptr);
    }
  }
  LOG("%s: default prefs: %dx%d @%dfps, %dHz test tones, aec: %s,"
      "agc: %s, noise: %s, aec level: %d, agc level: %d, noise level: %d,"
      "%sfull_duplex, extended aec %s, delay_agnostic %s "
      "channels %d",
      __FUNCTION__, mPrefs.mWidth, mPrefs.mHeight, mPrefs.mFPS, mPrefs.mFreq,
      mPrefs.mAecOn ? "on" : "off", mPrefs.mAgcOn ? "on" : "off",
      mPrefs.mNoiseOn ? "on" : "off", mPrefs.mAec, mPrefs.mAgc, mPrefs.mNoise,
      mPrefs.mFullDuplex ? "" : "not ", mPrefs.mExtendedFilter ? "on" : "off",
      mPrefs.mDelayAgnostic ? "on" : "off", mPrefs.mChannels);
}

NS_IMPL_ISUPPORTS(MediaManager, nsIMediaManagerService, nsIObserver)

/* static */
StaticRefPtr<MediaManager> MediaManager::sSingleton;

#ifdef DEBUG
/* static */
bool MediaManager::IsInMediaThread() {
  return sSingleton ? (sSingleton->mMediaThread->thread_id() ==
                       PlatformThread::CurrentId())
                    : false;
}
#endif

#ifdef XP_WIN
class MTAThread : public base::Thread {
 public:
  explicit MTAThread(const char* aName)
      : base::Thread(aName), mResult(E_FAIL) {}

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
/* static */
MediaManager* MediaManager::Get() {
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
    options.message_loop_type = MessageLoop::TYPE_MOZILLA_NONMAINTHREAD;
    if (!sSingleton->mMediaThread->StartWithOptions(options)) {
      MOZ_CRASH();
    }

    LOG("New Media thread for gum");

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->AddObserver(sSingleton, "last-pb-context-exited", false);
      obs->AddObserver(sSingleton, "getUserMedia:got-device-permission", false);
      obs->AddObserver(sSingleton, "getUserMedia:privileged:allow", false);
      obs->AddObserver(sSingleton, "getUserMedia:response:allow", false);
      obs->AddObserver(sSingleton, "getUserMedia:response:deny", false);
      obs->AddObserver(sSingleton, "getUserMedia:response:noOSPermission",
                       false);
      obs->AddObserver(sSingleton, "getUserMedia:revoke", false);
    }
    // else MediaManager won't work properly and will leak (see bug 837874)
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs) {
      prefs->AddObserver("media.navigator.video.default_width", sSingleton,
                         false);
      prefs->AddObserver("media.navigator.video.default_height", sSingleton,
                         false);
      prefs->AddObserver("media.navigator.video.default_fps", sSingleton,
                         false);
      prefs->AddObserver("media.navigator.audio.fake_frequency", sSingleton,
                         false);
      prefs->AddObserver("media.navigator.audio.full_duplex", sSingleton,
                         false);
#ifdef MOZ_WEBRTC
      prefs->AddObserver("media.getusermedia.aec_enabled", sSingleton, false);
      prefs->AddObserver("media.getusermedia.aec", sSingleton, false);
      prefs->AddObserver("media.getusermedia.agc_enabled", sSingleton, false);
      prefs->AddObserver("media.getusermedia.agc", sSingleton, false);
      prefs->AddObserver("media.getusermedia.noise_enabled", sSingleton, false);
      prefs->AddObserver("media.getusermedia.noise", sSingleton, false);
      prefs->AddObserver("media.ondevicechange.fakeDeviceChangeEvent.enabled",
                         sSingleton, false);
      prefs->AddObserver("media.getusermedia.channels", sSingleton, false);
#endif
    }

    // Prepare async shutdown

    class Blocker : public media::ShutdownBlocker {
     public:
      Blocker()
          : media::ShutdownBlocker(
                NS_LITERAL_STRING("Media shutdown: blocking on media thread")) {
      }

      NS_IMETHOD BlockShutdown(nsIAsyncShutdownClient*) override {
        MOZ_RELEASE_ASSERT(MediaManager::GetIfExists());
        MediaManager::GetIfExists()->Shutdown();
        return NS_OK;
      }
    };

    sSingleton->mShutdownBlocker = new Blocker();
    nsresult rv = media::GetShutdownBarrier()->AddBlocker(
        sSingleton->mShutdownBlocker, NS_LITERAL_STRING(__FILE__), __LINE__,
        NS_LITERAL_STRING(""));
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  }
  return sSingleton;
}

/* static */
MediaManager* MediaManager::GetIfExists() { return sSingleton; }

/* static */
already_AddRefed<MediaManager> MediaManager::GetInstance() {
  // so we can have non-refcounted getters
  RefPtr<MediaManager> service = MediaManager::Get();
  return service.forget();
}

media::Parent<media::NonE10s>* MediaManager::GetNonE10sParent() {
  if (!mNonE10sParent) {
    mNonE10sParent = new media::Parent<media::NonE10s>();
  }
  return mNonE10sParent;
}

/* static */
void MediaManager::StartupInit() {
#ifdef WIN32
  if (!IsWin8OrLater()) {
    // Bug 1107702 - Older Windows fail in GetAdaptersInfo (and others) if the
    // first(?) call occurs after the process size is over 2GB (kb/2588507).
    // Attempt to 'prime' the pump by making a call at startup.
    unsigned long out_buf_len = sizeof(IP_ADAPTER_INFO);
    PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO*)moz_xmalloc(out_buf_len);
    if (GetAdaptersInfo(pAdapterInfo, &out_buf_len) == ERROR_BUFFER_OVERFLOW) {
      free(pAdapterInfo);
      pAdapterInfo = (IP_ADAPTER_INFO*)moz_xmalloc(out_buf_len);
      GetAdaptersInfo(pAdapterInfo, &out_buf_len);
    }
    if (pAdapterInfo) {
      free(pAdapterInfo);
    }
  }
#endif
}

/* static */
void MediaManager::PostTask(already_AddRefed<Runnable> task) {
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

template <typename MozPromiseType, typename FunctionType>
/* static */
RefPtr<MozPromiseType> MediaManager::PostTask(const char* aName,
                                              FunctionType&& aFunction) {
  MozPromiseHolder<MozPromiseType> holder;
  RefPtr<MozPromiseType> promise = holder.Ensure(aName);
  MediaManager::PostTask(NS_NewRunnableFunction(
      aName, [h = std::move(holder), func = std::forward<FunctionType>(
                                         aFunction)]() mutable { func(h); }));
  return promise;
}

/* static */
nsresult MediaManager::NotifyRecordingStatusChange(
    nsPIDOMWindowInner* aWindow) {
  NS_ENSURE_ARG(aWindow);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    NS_WARNING(
        "Could not get the Observer service for GetUserMedia recording "
        "notification.");
    return NS_ERROR_FAILURE;
  }

  auto props = MakeRefPtr<nsHashPropertyBag>();

  nsCString pageURL;
  nsCOMPtr<nsIURI> docURI = aWindow->GetDocumentURI();
  NS_ENSURE_TRUE(docURI, NS_ERROR_FAILURE);

  nsresult rv = docURI->GetSpec(pageURL);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ConvertUTF8toUTF16 requestURL(pageURL);

  props->SetPropertyAsAString(NS_LITERAL_STRING("requestURL"), requestURL);

  obs->NotifyObservers(static_cast<nsIPropertyBag2*>(props),
                       "recording-device-events", nullptr);
  LOG("Sent recording-device-events for url '%s'", pageURL.get());

  return NS_OK;
}

int MediaManager::AddDeviceChangeCallback(DeviceChangeCallback* aCallback) {
  bool fakeDeviceChangeEventOn = mPrefs.mFakeDeviceChangeEventOn;
  MediaManager::PostTask(NewTaskFrom([fakeDeviceChangeEventOn]() {
    MediaManager* manager = MediaManager::GetIfExists();
    MOZ_RELEASE_ASSERT(manager);  // Must exist while media thread is alive
    // this is needed in case persistent permission is given but no gUM()
    // or enumeration() has created the real backend yet
    manager->GetBackend();
    if (fakeDeviceChangeEventOn)
      manager->GetBackend()->SetFakeDeviceChangeEvents();
  }));

  return DeviceChangeNotifier::AddDeviceChangeCallback(aCallback);
}

void MediaManager::OnDeviceChange() {
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "MediaManager::OnDeviceChange", [self = RefPtr<MediaManager>(this)]() {
        MOZ_ASSERT(NS_IsMainThread());
        if (sHasShutdown) {
          return;
        }
        self->NotifyDeviceChange();

        // On some Windows machine, if we call EnumerateRawDevices immediately
        // after receiving devicechange event, sometimes we would get outdated
        // devices list.
        PR_Sleep(PR_MillisecondsToInterval(200));
        auto devices = MakeRefPtr<MediaDeviceSetRefCnt>();
        self->EnumerateRawDevices(
                0, MediaSourceEnum::Camera, MediaSourceEnum::Microphone,
                MediaSinkEnum::Speaker, DeviceEnumerationType::Normal,
                DeviceEnumerationType::Normal, false, devices)
            ->Then(
                GetCurrentThreadSerialEventTarget(), __func__,
                [self, devices](bool) {
                  if (!MediaManager::GetIfExists()) {
                    return;
                  }

                  nsTArray<nsString> deviceIDs;

                  for (auto& device : *devices) {
                    nsString id;
                    device->GetId(id);
                    id.ReplaceSubstring(NS_LITERAL_STRING("default: "),
                                        NS_LITERAL_STRING(""));
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

                    for (auto iter = windowsById->Iter(); !iter.Done();
                         iter.Next()) {
                      nsGlobalWindowInner* window = iter.Data();
                      self->IterateWindowListeners(
                          window,
                          [&id](const RefPtr<GetUserMediaWindowListener>&
                                    aListener) { aListener->StopRawID(id); });
                    }
                  }

                  self->mDeviceIDs = deviceIDs;
                },
                [](RefPtr<MediaMgrError>&& reason) {});
      }));
}

nsresult MediaManager::GenerateUUID(nsAString& aResult) {
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

static bool IsFullyActive(nsPIDOMWindowInner* aWindow) {
  while (true) {
    if (!aWindow) {
      return false;
    }
    Document* document = aWindow->GetExtantDoc();
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
 * This function is used in getUserMedia when privacy.resistFingerprinting is
 * true. Only mediaSource of audio/video constraint will be kept.
 */
static void ReduceConstraint(
    OwningBooleanOrMediaTrackConstraints& aConstraint) {
  // Not requesting stream.
  if (!IsOn(aConstraint)) {
    return;
  }

  // It looks like {audio: true}, do nothing.
  if (!aConstraint.IsMediaTrackConstraints()) {
    return;
  }

  // Keep mediaSource, ignore all other constraints.
  Maybe<nsString> mediaSource;
  if (aConstraint.GetAsMediaTrackConstraints().mMediaSource.WasPassed()) {
    mediaSource =
        Some(aConstraint.GetAsMediaTrackConstraints().mMediaSource.Value());
  }
  aConstraint.Uninit();
  if (mediaSource) {
    aConstraint.SetAsMediaTrackConstraints().mMediaSource.Construct(
        *mediaSource);
  } else {
    aConstraint.SetAsMediaTrackConstraints();
  }
}

/**
 * The entry point for this file. A call from Navigator::mozGetUserMedia
 * will end up here. MediaManager is a singleton that is responsible
 * for handling all incoming getUserMedia calls from every window.
 */
RefPtr<MediaManager::StreamPromise> MediaManager::GetUserMedia(
    nsPIDOMWindowInner* aWindow,
    const MediaStreamConstraints& aConstraintsPassedIn,
    CallerType aCallerType) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  uint64_t windowID = aWindow->WindowID();

  MediaStreamConstraints c(aConstraintsPassedIn);  // use a modifiable copy

  // Do all the validation we can while we're sync (to return an
  // already-rejected promise on failure).

  if (!IsOn(c.mVideo) && !IsOn(c.mAudio)) {
    return StreamPromise::CreateAndReject(
        MakeRefPtr<MediaMgrError>(
            MediaMgrError::Name::TypeError,
            NS_LITERAL_STRING("audio and/or video is required")),
        __func__);
  }

  if (!IsFullyActive(aWindow)) {
    return StreamPromise::CreateAndReject(
        MakeRefPtr<MediaMgrError>(MediaMgrError::Name::InvalidStateError),
        __func__);
  }

  if (sHasShutdown) {
    return StreamPromise::CreateAndReject(
        MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError,
                                  NS_LITERAL_STRING("In shutdown")),
        __func__);
  }

  // Determine permissions early (while we still have a stack).

  nsIURI* docURI = aWindow->GetDocumentURI();
  if (!docURI) {
    return StreamPromise::CreateAndReject(
        MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError), __func__);
  }
  bool isChrome = (aCallerType == CallerType::System);
  bool privileged =
      isChrome ||
      Preferences::GetBool("media.navigator.permission.disabled", false);
  bool isSecure = aWindow->IsSecureContext();
  bool isHandlingUserInput = UserActivation::IsHandlingUserInput();
  nsCString host;
  nsresult rv = docURI->GetHost(host);

  nsCOMPtr<nsIPrincipal> principal =
      nsGlobalWindowInner::Cast(aWindow)->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    return StreamPromise::CreateAndReject(
        MakeRefPtr<MediaMgrError>(MediaMgrError::Name::SecurityError),
        __func__);
  }

  Document* doc = aWindow->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return StreamPromise::CreateAndReject(
        MakeRefPtr<MediaMgrError>(MediaMgrError::Name::SecurityError),
        __func__);
  }

  // Disallow access to null principal pages and http pages (unless pref)
  if (principal->GetIsNullPrincipal() ||
      !(isSecure || StaticPrefs::media_getusermedia_insecure_enabled())) {
    return StreamPromise::CreateAndReject(
        MakeRefPtr<MediaMgrError>(MediaMgrError::Name::NotAllowedError),
        __func__);
  }

  // This principal needs to be sent to different threads and so via IPC.
  // For this reason it's better to convert it to PrincipalInfo right now.
  ipc::PrincipalInfo principalInfo;
  rv = PrincipalToPrincipalInfo(principal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return StreamPromise::CreateAndReject(
        MakeRefPtr<MediaMgrError>(MediaMgrError::Name::SecurityError),
        __func__);
  }

  const bool resistFingerprinting =
      nsContentUtils::ResistFingerprinting(aCallerType);
  if (resistFingerprinting) {
    ReduceConstraint(c.mVideo);
    ReduceConstraint(c.mAudio);
  }

  if (!Preferences::GetBool("media.navigator.video.enabled", true)) {
    c.mVideo.SetAsBoolean() = false;
  }

  MediaSourceEnum videoType = MediaSourceEnum::Other;  // none
  MediaSourceEnum audioType = MediaSourceEnum::Other;  // none

  if (c.mVideo.IsMediaTrackConstraints()) {
    auto& vc = c.mVideo.GetAsMediaTrackConstraints();
    if (!vc.mMediaSource.WasPassed()) {
      vc.mMediaSource.Construct().AssignASCII(EnumToASCII(
          dom::MediaSourceEnumValues::strings, MediaSourceEnum::Camera));
    }
    videoType = StringToEnum(dom::MediaSourceEnumValues::strings,
                             vc.mMediaSource.Value(), MediaSourceEnum::Other);
    Telemetry::Accumulate(Telemetry::WEBRTC_GET_USER_MEDIA_TYPE,
                          (uint32_t)videoType);
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
      case MediaSourceEnum::Window:
        // Deny screensharing request if support is disabled, or
        // the requesting document is not from a host on the whitelist.
        if (!Preferences::GetBool(
                ((videoType == MediaSourceEnum::Browser)
                     ? "media.getusermedia.browser.enabled"
                     : "media.getusermedia.screensharing.enabled"),
                false) ||
            (!privileged && !aWindow->IsSecureContext())) {
          return StreamPromise::CreateAndReject(
              MakeRefPtr<MediaMgrError>(MediaMgrError::Name::NotAllowedError),
              __func__);
        }
        break;

      case MediaSourceEnum::Microphone:
      case MediaSourceEnum::Other:
      default: {
        return StreamPromise::CreateAndReject(
            MakeRefPtr<MediaMgrError>(MediaMgrError::Name::OverconstrainedError,
                                      NS_LITERAL_STRING(""),
                                      NS_LITERAL_STRING("mediaSource")),
            __func__);
      }
    }

    if (!privileged) {
      // Only allow privileged content to explicitly pick full-screen,
      // application or tabsharing, since these modes are still available for
      // testing. All others get "Window" (*) sharing.
      //
      // *) We overload "Window" with the new default getDisplayMedia spec-
      // mandated behavior of not influencing user-choice, which we currently
      // implement as a list containing BOTH windows AND screen(s).
      //
      // Notes on why we chose "Window" as the one to overload. Two reasons:
      //
      // 1. It's the closest logically & behaviorally (multi-choice, no default)
      // 2. Screen is still useful in tests (implicit default is entire screen)
      //
      // For UX reasons we don't want "Entire Screen" to be the first/default
      // choice (in our code first=default). It's a "scary" source that comes
      // with complicated warnings on-top that would be confusing as the first
      // thing people see, and also deserves to be listed as last resort for
      // privacy reasons.

      if (videoType == MediaSourceEnum::Screen ||
          videoType == MediaSourceEnum::Browser) {
        videoType = MediaSourceEnum::Window;
        vc.mMediaSource.Value().AssignASCII(
            EnumToASCII(dom::MediaSourceEnumValues::strings, videoType));
      }
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
                          (uint32_t)videoType);
  }

  if (c.mAudio.IsMediaTrackConstraints()) {
    auto& ac = c.mAudio.GetAsMediaTrackConstraints();
    if (!ac.mMediaSource.WasPassed()) {
      ac.mMediaSource.Construct(NS_ConvertASCIItoUTF16(EnumToASCII(
          dom::MediaSourceEnumValues::strings, MediaSourceEnum::Microphone)));
    }
    audioType = StringToEnum(dom::MediaSourceEnumValues::strings,
                             ac.mMediaSource.Value(), MediaSourceEnum::Other);
    Telemetry::Accumulate(Telemetry::WEBRTC_GET_USER_MEDIA_TYPE,
                          (uint32_t)audioType);

    switch (audioType) {
      case MediaSourceEnum::Microphone:
        break;

      case MediaSourceEnum::AudioCapture:
        // Only enable AudioCapture if the pref is enabled. If it's not, we can
        // deny right away.
        if (!Preferences::GetBool("media.getusermedia.audiocapture.enabled")) {
          return StreamPromise::CreateAndReject(
              MakeRefPtr<MediaMgrError>(MediaMgrError::Name::NotAllowedError),
              __func__);
        }
        break;

      case MediaSourceEnum::Other:
      default: {
        return StreamPromise::CreateAndReject(
            MakeRefPtr<MediaMgrError>(MediaMgrError::Name::OverconstrainedError,
                                      NS_LITERAL_STRING(""),
                                      NS_LITERAL_STRING("mediaSource")),
            __func__);
      }
    }
  } else if (IsOn(c.mAudio)) {
    audioType = MediaSourceEnum::Microphone;
    Telemetry::Accumulate(Telemetry::WEBRTC_GET_USER_MEDIA_TYPE,
                          (uint32_t)audioType);
  }

  // Create a window listener if it doesn't already exist.
  RefPtr<GetUserMediaWindowListener> windowListener =
      GetWindowListener(windowID);
  if (windowListener) {
    PrincipalHandle existingPrincipalHandle =
        windowListener->GetPrincipalHandle();
    MOZ_ASSERT(PrincipalHandleMatches(existingPrincipalHandle, principal));
  } else {
    windowListener = new GetUserMediaWindowListener(
        mMediaThread, windowID, MakePrincipalHandle(principal));
    AddWindowID(windowID, windowListener);
  }

  auto sourceListener = MakeRefPtr<SourceListener>();
  windowListener->Register(sourceListener);

  if (!privileged) {
    // Check if this site has had persistent permissions denied.
    nsCOMPtr<nsIPermissionManager> permManager =
        do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

    uint32_t audioPerm = nsIPermissionManager::UNKNOWN_ACTION;
    if (IsOn(c.mAudio)) {
      if (audioType == MediaSourceEnum::Microphone) {
        if (Preferences::GetBool("media.getusermedia.microphone.deny", false) ||
            !FeaturePolicyUtils::IsFeatureAllowed(
                doc, NS_LITERAL_STRING("microphone"))) {
          audioPerm = nsIPermissionManager::DENY_ACTION;
        } else {
          rv = permManager->TestExactPermissionFromPrincipal(
              principal, NS_LITERAL_CSTRING("microphone"), &audioPerm);
          MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
        }
      } else {
        if (!FeaturePolicyUtils::IsFeatureAllowed(
                doc, NS_LITERAL_STRING("display-capture"))) {
          audioPerm = nsIPermissionManager::DENY_ACTION;
        } else {
          rv = permManager->TestExactPermissionFromPrincipal(
              principal, NS_LITERAL_CSTRING("screen"), &audioPerm);
          MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
        }
      }
    }

    uint32_t videoPerm = nsIPermissionManager::UNKNOWN_ACTION;
    if (IsOn(c.mVideo)) {
      if (videoType == MediaSourceEnum::Camera) {
        if (Preferences::GetBool("media.getusermedia.camera.deny", false) ||
            !FeaturePolicyUtils::IsFeatureAllowed(
                doc, NS_LITERAL_STRING("camera"))) {
          videoPerm = nsIPermissionManager::DENY_ACTION;
        } else {
          rv = permManager->TestExactPermissionFromPrincipal(
              principal, NS_LITERAL_CSTRING("camera"), &videoPerm);
          MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
        }
      } else {
        if (!FeaturePolicyUtils::IsFeatureAllowed(
                doc, NS_LITERAL_STRING("display-capture"))) {
          videoPerm = nsIPermissionManager::DENY_ACTION;
        } else {
          rv = permManager->TestExactPermissionFromPrincipal(
              principal, NS_LITERAL_CSTRING("screen"), &videoPerm);
          MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
        }
      }
    }

    if ((!IsOn(c.mAudio) && !IsOn(c.mVideo)) ||
        (IsOn(c.mAudio) && audioPerm == nsIPermissionManager::DENY_ACTION) ||
        (IsOn(c.mVideo) && videoPerm == nsIPermissionManager::DENY_ACTION)) {
      windowListener->Remove(sourceListener);
      return StreamPromise::CreateAndReject(
          MakeRefPtr<MediaMgrError>(MediaMgrError::Name::NotAllowedError),
          __func__);
    }
  }

  // Get list of all devices, with origin-specific device ids.

  MediaEnginePrefs prefs = mPrefs;

  nsString callID;
  rv = GenerateUUID(callID);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

  bool hasVideo = videoType != MediaSourceEnum::Other;
  bool hasAudio = audioType != MediaSourceEnum::Other;
  DeviceEnumerationType videoEnumerationType = DeviceEnumerationType::Normal;
  DeviceEnumerationType audioEnumerationType = DeviceEnumerationType::Normal;

  // Handle loopback and fake requests. For gUM we don't consider resist
  // fingerprinting as users should be prompted anyway.
  bool wantFakes = c.mFake.WasPassed()
                       ? c.mFake.Value()
                       : Preferences::GetBool("media.navigator.streams.fake");
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

  bool realDevicesRequested =
      (videoEnumerationType != DeviceEnumerationType::Fake && hasVideo) ||
      (audioEnumerationType != DeviceEnumerationType::Fake && hasAudio);
  bool askPermission =
      (!privileged ||
       Preferences::GetBool("media.navigator.permission.force")) &&
      (realDevicesRequested ||
       Preferences::GetBool("media.navigator.permission.fake"));

  LOG("%s: Preparing to enumerate devices. windowId=%" PRIu64
      ", videoType=%" PRIu8 ", audioType=%" PRIu8
      ", videoEnumerationType=%" PRIu8 ", audioEnumerationType=%" PRIu8
      ", askPermission=%s",
      __func__, windowID, static_cast<uint8_t>(videoType),
      static_cast<uint8_t>(audioType),
      static_cast<uint8_t>(videoEnumerationType),
      static_cast<uint8_t>(audioEnumerationType),
      askPermission ? "true" : "false");

  RefPtr<MediaManager> self = this;
  auto devices = MakeRefPtr<MediaDeviceSetRefCnt>();
  return EnumerateDevicesImpl(windowID, videoType, audioType,
                              MediaSinkEnum::Other, videoEnumerationType,
                              audioEnumerationType, true, devices)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [self, windowID, c, windowListener, isChrome, devices](bool) {
            LOG("GetUserMedia: post enumeration promise success callback "
                "starting");
            // Ensure that our windowID is still good.
            RefPtr<nsPIDOMWindowInner> window =
                nsGlobalWindowInner::GetInnerWindowWithId(windowID);
            if (!window || !self->IsWindowListenerStillActive(windowListener)) {
              LOG("GetUserMedia: bad window (%" PRIu64
                  ") in post enumeration success callback!",
                  windowID);
              return BadConstraintsPromise::CreateAndReject(
                  MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError),
                  __func__);
            }
            // Apply any constraints. This modifies the passed-in list.
            return self->SelectSettings(c, isChrome, devices);
          },
          [](RefPtr<MediaMgrError>&& aError) {
            LOG("GetUserMedia: post enumeration EnumerateDevicesImpl "
                "failure callback called!");
            return BadConstraintsPromise::CreateAndReject(std::move(aError),
                                                          __func__);
          })
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [self, windowID, c, windowListener, sourceListener, askPermission,
           prefs, isSecure, isHandlingUserInput, callID, principalInfo,
           isChrome, devices,
           resistFingerprinting](const char* badConstraint) mutable {
            LOG("GetUserMedia: starting post enumeration promise2 success "
                "callback!");

            // Ensure that the window is still good.
            RefPtr<nsPIDOMWindowInner> window =
                nsGlobalWindowInner::GetInnerWindowWithId(windowID);
            if (!window || !self->IsWindowListenerStillActive(windowListener)) {
              LOG("GetUserMedia: bad window (%" PRIu64
                  ") in post enumeration success callback 2!",
                  windowID);
              return StreamPromise::CreateAndReject(
                  MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError),
                  __func__);
            }

            if (badConstraint) {
              LOG("GetUserMedia: bad constraint found in post enumeration "
                  "promise2 success callback! Calling error handler!");
              nsString constraint;
              constraint.AssignASCII(badConstraint);
              return StreamPromise::CreateAndReject(
                  MakeRefPtr<MediaMgrError>(
                      MediaMgrError::Name::OverconstrainedError,
                      NS_LITERAL_STRING(""), constraint),
                  __func__);
            }
            if (!devices->Length()) {
              LOG("GetUserMedia: no devices found in post enumeration promise2 "
                  "success callback! Calling error handler!");
              // When privacy.resistFingerprinting = true, no
              // available device implies content script is requesting
              // a fake device, so report NotAllowedError.
              auto error = resistFingerprinting
                               ? MediaMgrError::Name::NotAllowedError
                               : MediaMgrError::Name::NotFoundError;
              return StreamPromise::CreateAndReject(
                  MakeRefPtr<MediaMgrError>(error), __func__);
            }

            // before we give up devices below
            nsCOMPtr<nsIMutableArray> devicesCopy = nsArray::Create();
            if (!askPermission) {
              for (auto& device : *devices) {
                nsresult rv = devicesCopy->AppendElement(device);
                if (NS_WARN_IF(NS_FAILED(rv))) {
                  return StreamPromise::CreateAndReject(
                      MakeRefPtr<MediaMgrError>(
                          MediaMgrError::Name::AbortError),
                      __func__);
                }
              }
            }

            bool focusSource = mozilla::Preferences::GetBool(
                "media.getusermedia.window.focus_source.enabled", true);

            // Incremental hack to compile. To be replaced by deeper
            // refactoring. MediaManager allows
            // "neither-resolve-nor-reject" semantics, so we cannot
            // use MozPromiseHolder here.
            auto holder = MozPromiseHolder<StreamPromise>();
            RefPtr<StreamPromise> p = holder.Ensure(__func__);

            // Pass callbacks and listeners along to GetUserMediaTask.
            auto task = MakeRefPtr<GetUserMediaTask>(
                c, std::move(holder), windowID, windowListener, sourceListener,
                prefs, principalInfo, isChrome, std::move(devices),
                focusSource);

            // Store the task w/callbacks.
            self->mActiveCallbacks.Put(callID, task.forget());

            // Add a WindowID cross-reference so OnNavigation can tear
            // things down
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
              auto req = MakeRefPtr<GetUserMediaRequest>(
                  window, callID, c, isSecure, isHandlingUserInput);
              if (!Preferences::GetBool("media.navigator.permission.force") &&
                  array->Length() > 1) {
                // there is at least 1 pending gUM request
                // For the scarySources test case, always send the
                // request
                self->mPendingGUMRequest.AppendElement(req.forget());
              } else {
                obs->NotifyObservers(req, "getUserMedia:request", nullptr);
              }
            }
#ifdef MOZ_WEBRTC
            EnableWebRtcLog();
#endif
            return p;
          },
          [](RefPtr<MediaMgrError>&& aError) {
            LOG("GetUserMedia: post enumeration SelectSettings failure "
                "callback called!");
            return StreamPromise::CreateAndReject(std::move(aError), __func__);
          });
};

RefPtr<MediaManager::StreamPromise> MediaManager::GetDisplayMedia(
    nsPIDOMWindowInner* aWindow,
    const DisplayMediaStreamConstraints& aConstraintsPassedIn,
    CallerType aCallerType) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  if (!IsOn(aConstraintsPassedIn.mVideo)) {
    return StreamPromise::CreateAndReject(
        MakeRefPtr<MediaMgrError>(MediaMgrError::Name::TypeError,
                                  NS_LITERAL_STRING("video is required")),
        __func__);
  }

  MediaStreamConstraints c;
  auto& vc = c.mVideo.SetAsMediaTrackConstraints();

  if (aConstraintsPassedIn.mVideo.IsMediaTrackConstraints()) {
    vc = aConstraintsPassedIn.mVideo.GetAsMediaTrackConstraints();
    if (vc.mAdvanced.WasPassed()) {
      return StreamPromise::CreateAndReject(
          MakeRefPtr<MediaMgrError>(MediaMgrError::Name::TypeError,
                                    NS_LITERAL_STRING("advanced not allowed")),
          __func__);
    }
    auto getCLR = [](const auto& aCon) -> const ConstrainLongRange& {
      static ConstrainLongRange empty;
      return (aCon.WasPassed() && !aCon.Value().IsLong())
                 ? aCon.Value().GetAsConstrainLongRange()
                 : empty;
    };
    auto getCDR = [](auto&& aCon) -> const ConstrainDoubleRange& {
      static ConstrainDoubleRange empty;
      return (aCon.WasPassed() && !aCon.Value().IsDouble())
                 ? aCon.Value().GetAsConstrainDoubleRange()
                 : empty;
    };
    const auto& w = getCLR(vc.mWidth);
    const auto& h = getCLR(vc.mHeight);
    const auto& f = getCDR(vc.mFrameRate);
    if (w.mMin.WasPassed() || h.mMin.WasPassed() || f.mMin.WasPassed()) {
      return StreamPromise::CreateAndReject(
          MakeRefPtr<MediaMgrError>(MediaMgrError::Name::TypeError,
                                    NS_LITERAL_STRING("min not allowed")),
          __func__);
    }
    if (w.mExact.WasPassed() || h.mExact.WasPassed() || f.mExact.WasPassed()) {
      return StreamPromise::CreateAndReject(
          MakeRefPtr<MediaMgrError>(MediaMgrError::Name::TypeError,
                                    NS_LITERAL_STRING("exact not allowed")),
          __func__);
    }
    // As a UA optimization, we fail early without incurring a prompt, on
    // known-to-fail constraint values that don't reveal anything about the
    // user's system.
    const char* badConstraint = nullptr;
    if (w.mMax.WasPassed() && w.mMax.Value() < 1) {
      badConstraint = "width";
    }
    if (h.mMax.WasPassed() && h.mMax.Value() < 1) {
      badConstraint = "height";
    }
    if (f.mMax.WasPassed() && f.mMax.Value() < 1) {
      badConstraint = "frameRate";
    }
    if (badConstraint) {
      return StreamPromise::CreateAndReject(
          MakeRefPtr<MediaMgrError>(MediaMgrError::Name::OverconstrainedError,
                                    NS_LITERAL_STRING(""),
                                    NS_ConvertASCIItoUTF16(badConstraint)),
          __func__);
    }
  }
  // We ask for "screen" sharing.
  //
  // If this is a privileged call or permission is disabled, this gives us full
  // screen sharing by default, which is useful for internal testing.
  //
  // If this is a non-priviliged call, GetUserMedia() will change it to "window"
  // for us.
  vc.mMediaSource.Reset();
  vc.mMediaSource.Construct().AssignASCII(EnumToASCII(
      dom::MediaSourceEnumValues::strings, MediaSourceEnum::Screen));

  return MediaManager::GetUserMedia(aWindow, c, aCallerType);
}

/* static */
void MediaManager::AnonymizeDevices(MediaDeviceSet& aDevices,
                                    const nsACString& aOriginKey,
                                    const uint64_t aWindowId) {
  if (!aOriginKey.IsEmpty()) {
    for (RefPtr<MediaDevice>& device : aDevices) {
      nsString id;
      device->GetId(id);
      nsString rawId(id);
      AnonymizeId(id, aOriginKey);

      nsString groupId;
      device->GetGroupId(groupId);
      // Use window id to salt group id in order to make it session based as
      // required by the spec. This does not provide unique group ids through
      // out a browser restart. However, this is not agaist the spec.
      // Furtermore, since device ids are the same after a browser restart the
      // fingerprint is not bigger.
      groupId.AppendInt(aWindowId);
      AnonymizeId(groupId, aOriginKey);

      device = new MediaDevice(device, id, groupId, rawId);
    }
  }
}

/* static */
nsresult MediaManager::AnonymizeId(nsAString& aId,
                                   const nsACString& aOriginKey) {
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
  rv = hasher->Update(reinterpret_cast<const uint8_t*>(id.get()), id.Length());
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
already_AddRefed<nsIWritableVariant> MediaManager::ToJSArray(
    MediaDeviceSet& aDevices) {
  MOZ_ASSERT(NS_IsMainThread());
  auto var = MakeRefPtr<nsVariantCC>();
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
    var->SetAsEmptyArray();  // because SetAsArray() fails on zero length
                             // arrays.
  }
  return var.forget();
}

RefPtr<MediaManager::MgrPromise> MediaManager::EnumerateDevicesImpl(
    uint64_t aWindowId, MediaSourceEnum aVideoInputType,
    MediaSourceEnum aAudioInputType, MediaSinkEnum aAudioOutputType,
    DeviceEnumerationType aVideoInputEnumType,
    DeviceEnumerationType aAudioInputEnumType, bool aForceNoPermRequest,
    const RefPtr<MediaDeviceSetRefCnt>& aOutDevices) {
  MOZ_ASSERT(NS_IsMainThread());

  LOG("%s: aWindowId=%" PRIu64 ", aVideoInputType=%" PRIu8
      ", aAudioInputType=%" PRIu8 ", aVideoInputEnumType=%" PRIu8
      ", aAudioInputEnumType=%" PRIu8,
      __func__, aWindowId, static_cast<uint8_t>(aVideoInputType),
      static_cast<uint8_t>(aAudioInputType),
      static_cast<uint8_t>(aVideoInputEnumType),
      static_cast<uint8_t>(aAudioInputEnumType));
  auto* window = nsGlobalWindowInner::GetInnerWindowWithId(aWindowId);

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
    return MgrPromise::CreateAndReject(
        MakeRefPtr<MediaMgrError>(MediaMgrError::Name::NotAllowedError),
        __func__);
  }

  bool persist = IsActivelyCapturingOrHasAPermission(aWindowId);

  // GetPrincipalKey is an async API that returns a promise. We use .Then() to
  // pass in a lambda to run back on this same thread later once
  // GetPrincipalKey resolves. Needed variables are "captured"
  // (passed by value) safely into the lambda.
  auto originKey = MakeRefPtr<Refcountable<nsCString>>();
  return media::GetPrincipalKey(principalInfo, persist)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [aWindowId, aVideoInputType, aAudioInputType, aAudioOutputType,
           aVideoInputEnumType, aAudioInputEnumType, aForceNoPermRequest,
           aOutDevices, originKey](const nsCString& aOriginKey) {
            MOZ_ASSERT(NS_IsMainThread());
            originKey->Assign(aOriginKey);
            MediaManager* mgr = MediaManager::GetIfExists();
            MOZ_ASSERT(mgr);
            if (!mgr->IsWindowStillActive(aWindowId)) {
              return MgrPromise::CreateAndReject(
                  MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError),
                  __func__);
            }
            return mgr->EnumerateRawDevices(
                aWindowId, aVideoInputType, aAudioInputType, aAudioOutputType,
                aVideoInputEnumType, aAudioInputEnumType, aForceNoPermRequest,
                aOutDevices);
          },
          [](nsresult rs) {
            NS_WARNING(
                "EnumerateDevicesImpl failed to get Principal Key. Enumeration "
                "will not continue.");
            return MgrPromise::CreateAndReject(
                MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError),
                __func__);
          })
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [aWindowId, originKey, aVideoInputEnumType, aAudioInputEnumType,
           aVideoInputType, aAudioInputType, aOutDevices](bool) {
            // Only run if window is still on our active list.
            MediaManager* mgr = MediaManager::GetIfExists();
            if (!mgr || !mgr->IsWindowStillActive(aWindowId)) {
              return MgrPromise::CreateAndReject(
                  MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError),
                  __func__);
            }

            // If we fetched any real cameras or mics, remove the
            // "default" part of their IDs.
            if (aVideoInputType == MediaSourceEnum::Camera &&
                aAudioInputType == MediaSourceEnum::Microphone &&
                (aVideoInputEnumType != DeviceEnumerationType::Fake ||
                 aAudioInputEnumType != DeviceEnumerationType::Fake)) {
              mgr->mDeviceIDs.Clear();
              for (auto& device : *aOutDevices) {
                nsString id;
                device->GetId(id);
                id.ReplaceSubstring(NS_LITERAL_STRING("default: "),
                                    NS_LITERAL_STRING(""));
                if (!mgr->mDeviceIDs.Contains(id)) {
                  mgr->mDeviceIDs.AppendElement(id);
                }
              }
            }
            if (!mgr->IsWindowStillActive(aWindowId)) {
              return MgrPromise::CreateAndReject(
                  MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError),
                  __func__);
            }
            MediaManager::AnonymizeDevices(*aOutDevices, *originKey, aWindowId);
            return MgrPromise::CreateAndResolve(false, __func__);
          },
          [](RefPtr<MediaMgrError>&& aError) {
            return MgrPromise::CreateAndReject(std::move(aError), __func__);
          });
}

RefPtr<MediaManager::DevicesPromise> MediaManager::EnumerateDevices(
    nsPIDOMWindowInner* aWindow, CallerType aCallerType) {
  MOZ_ASSERT(NS_IsMainThread());
  if (sHasShutdown) {
    return DevicesPromise::CreateAndReject(
        MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError,
                                  NS_LITERAL_STRING("In shutdown")),
        __func__);
  }
  uint64_t windowId = aWindow->WindowID();
  Document* doc = aWindow->GetExtantDoc();
  MOZ_ASSERT(doc);

  nsIPrincipal* principal = doc->NodePrincipal();

  RefPtr<GetUserMediaWindowListener> windowListener =
      GetWindowListener(windowId);
  if (windowListener) {
    PrincipalHandle existingPrincipalHandle =
        windowListener->GetPrincipalHandle();
    MOZ_ASSERT(PrincipalHandleMatches(existingPrincipalHandle, principal));
  } else {
    windowListener = new GetUserMediaWindowListener(
        mMediaThread, windowId, MakePrincipalHandle(principal));
    AddWindowID(windowId, windowListener);
  }

  // Create an inactive SourceListener to act as a placeholder, so the
  // window listener doesn't clean itself up until we're done.
  auto sourceListener = MakeRefPtr<SourceListener>();
  windowListener->Register(sourceListener);

  DeviceEnumerationType videoEnumerationType = DeviceEnumerationType::Normal;
  DeviceEnumerationType audioEnumerationType = DeviceEnumerationType::Normal;

  // Only expose devices which are allowed to use:
  // https://w3c.github.io/mediacapture-main/#dom-mediadevices-enumeratedevices
  MediaSourceEnum videoType = dom::FeaturePolicyUtils::IsFeatureAllowed(
                                  doc, NS_LITERAL_STRING("camera"))
                                  ? MediaSourceEnum::Camera
                                  : MediaSourceEnum::Other;
  MediaSourceEnum audioType = dom::FeaturePolicyUtils::IsFeatureAllowed(
                                  doc, NS_LITERAL_STRING("microphone"))
                                  ? MediaSourceEnum::Microphone
                                  : MediaSourceEnum::Other;

  auto devices = MakeRefPtr<MediaDeviceSetRefCnt>();
  MediaSinkEnum audioOutputType = MediaSinkEnum::Other;
  // TODO bug Bug 1577199 we don't seem to support the "speaker" feature policy
  // yet.
  if (Preferences::GetBool("media.setsinkid.enabled")) {
    audioOutputType = MediaSinkEnum::Speaker;
  } else if (audioType == MediaSourceEnum::Other &&
             videoType == MediaSourceEnum::Other) {
    return DevicesPromise::CreateAndResolve(devices, __func__);
  }

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
  }

  return EnumerateDevicesImpl(windowId, videoType, audioType, audioOutputType,
                              videoEnumerationType, audioEnumerationType, false,
                              devices)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [windowListener, sourceListener, devices](bool) {
            DebugOnly<bool> rv = windowListener->Remove(sourceListener);
            MOZ_ASSERT(rv);
            return DevicesPromise::CreateAndResolve(devices, __func__);
          },
          [windowListener, sourceListener](RefPtr<MediaMgrError>&& aError) {
            // This may fail, if a new doc has been set the OnNavigation
            // method should have removed all previous active listeners.
            // Attempt to clean it here, just in case, but ignore the return
            // value.
            Unused << windowListener->Remove(sourceListener);
            return DevicesPromise::CreateAndReject(std::move(aError), __func__);
          });
}

RefPtr<SinkInfoPromise> MediaManager::GetSinkDevice(nsPIDOMWindowInner* aWindow,
                                                    const nsString& aDeviceId) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  // We have to add the window id here because enumerate methods
  // check for that and abort silently if it does not exist.
  uint64_t windowId = aWindow->WindowID();
  nsIPrincipal* principal = aWindow->GetExtantDoc()->NodePrincipal();
  RefPtr<GetUserMediaWindowListener> windowListener =
      GetWindowListener(windowId);
  if (windowListener) {
    PrincipalHandle existingPrincipalHandle =
        windowListener->GetPrincipalHandle();
    MOZ_ASSERT(PrincipalHandleMatches(existingPrincipalHandle, principal));
  } else {
    windowListener = new GetUserMediaWindowListener(
        mMediaThread, windowId, MakePrincipalHandle(principal));
    AddWindowID(windowId, windowListener);
  }
  // Create an inactive SourceListener to act as a placeholder, so the
  // window listener doesn't clean itself up until we're done.
  auto sourceListener = MakeRefPtr<SourceListener>();
  windowListener->Register(sourceListener);

  bool isSecure = aWindow->IsSecureContext();
  auto devices = MakeRefPtr<MediaDeviceSetRefCnt>();
  return EnumerateDevicesImpl(aWindow->WindowID(), MediaSourceEnum::Other,
                              MediaSourceEnum::Other, MediaSinkEnum::Speaker,
                              DeviceEnumerationType::Normal,
                              DeviceEnumerationType::Normal, true, devices)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [aDeviceId, isSecure, devices](bool) {
            for (RefPtr<MediaDevice>& device : *devices) {
              if (aDeviceId.IsEmpty() && device->mSinkInfo->Preferred()) {
                return SinkInfoPromise::CreateAndResolve(device->mSinkInfo,
                                                         __func__);
              }
              if (device->mID.Equals(aDeviceId)) {
                // TODO: Check if the application is authorized to play audio
                // through this device (Bug 1493982).
                if (isSecure || device->mSinkInfo->Preferred()) {
                  return SinkInfoPromise::CreateAndResolve(device->mSinkInfo,
                                                           __func__);
                }
                return SinkInfoPromise::CreateAndReject(
                    NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR, __func__);
              }
            }
            return SinkInfoPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                    __func__);
          },
          [](RefPtr<MediaMgrError>&& aError) {
            return SinkInfoPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                    __func__);
          });
}

/*
 * GetUserMediaDevices - called by the UI-part of getUserMedia from chrome JS.
 */

nsresult MediaManager::GetUserMediaDevices(
    nsPIDOMWindowInner* aWindow, const MediaStreamConstraints& aConstraints,
    MozGetUserMediaDevicesSuccessCallback& aOnSuccess, uint64_t aWindowId,
    const nsAString& aCallID) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!aWindowId) {
    aWindowId = aWindow->WindowID();
  }

  // Ignore passed-in constraints, instead locate + return already-constrained
  // list.

  nsTArray<nsString>* callIDs;
  if (!mCallIds.Get(aWindowId, &callIDs)) {
    return NS_ERROR_UNEXPECTED;
  }

  for (auto& callID : *callIDs) {
    RefPtr<GetUserMediaTask> task;
    if (!aCallID.Length() || aCallID == callID) {
      if (mActiveCallbacks.Get(callID, getter_AddRefs(task))) {
        nsCOMPtr<nsIWritableVariant> array =
            MediaManager::ToJSArray(*task->mMediaDeviceSet);
        aOnSuccess.Call(array);
        return NS_OK;
      }
    }
  }
  return NS_ERROR_UNEXPECTED;
}

MediaEngine* MediaManager::GetBackend() {
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  // Plugin backends as appropriate. The default engine also currently
  // includes picture support for Android.
  // This IS called off main-thread.
  if (!mBackend) {
    MOZ_RELEASE_ASSERT(
        !sHasShutdown);  // we should never create a new backend in shutdown
#if defined(MOZ_WEBRTC)
    mBackend = new MediaEngineWebRTC(mPrefs);
#else
    mBackend = new MediaEngineDefault();
#endif
    mBackend->AddDeviceChangeCallback(this);
  }
  return mBackend;
}

void MediaManager::OnNavigation(uint64_t aWindowID) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("OnNavigation for %" PRIu64, aWindowID);

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
    IterateWindowListeners(
        window, [self = RefPtr<MediaManager>(this),
                 windowID = DebugOnly<decltype(aWindowID)>(aWindowID)](
                    const RefPtr<GetUserMediaWindowListener>& aListener) {
          aListener->RemoveAll();
          MOZ_ASSERT(!self->GetWindowListener(windowID));
        });
  } else {
    RemoveWindowID(aWindowID);
  }
  MOZ_ASSERT(!GetWindowListener(aWindowID));

  RemoveMediaDevicesCallback(aWindowID);
}

void MediaManager::RemoveMediaDevicesCallback(uint64_t aWindowID) {
  MutexAutoLock lock(mCallbackMutex);
  for (DeviceChangeCallback* observer : mDeviceChangeCallbackList) {
    MediaDevices* mediadevices = static_cast<MediaDevices*>(observer);
    MOZ_ASSERT(mediadevices);
    if (mediadevices) {
      nsPIDOMWindowInner* window = mediadevices->GetOwner();
      MOZ_ASSERT(window);
      if (window && window->WindowID() == aWindowID) {
        RemoveDeviceChangeCallbackLocked(observer);
        return;
      }
    }
  }
}

void MediaManager::AddWindowID(uint64_t aWindowId,
                               RefPtr<GetUserMediaWindowListener> aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  // Store the WindowID in a hash table and mark as active. The entry is removed
  // when this window is closed or navigated away from.
  // This is safe since we're on main-thread, and the windowlist can only
  // be invalidated from the main-thread (see OnNavigation)
  if (IsWindowStillActive(aWindowId)) {
    MOZ_ASSERT(false, "Window already added");
    return;
  }

  GetActiveWindows()->Put(aWindowId, aListener.forget());
}

void MediaManager::RemoveWindowID(uint64_t aWindowId) {
  mActiveWindows.Remove(aWindowId);

  // get outer windowID
  auto* window = nsGlobalWindowInner::GetInnerWindowWithId(aWindowId);
  if (!window) {
    LOG("No inner window for %" PRIu64, aWindowId);
    return;
  }

  auto* outer = window->GetOuterWindow();
  if (!outer) {
    LOG("No outer window for inner %" PRIu64, aWindowId);
    return;
  }

  uint64_t outerID = outer->WindowID();

  // Notify the UI that this window no longer has gUM active
  char windowBuffer[32];
  SprintfLiteral(windowBuffer, "%" PRIu64, outerID);
  nsString data = NS_ConvertUTF8toUTF16(windowBuffer);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  obs->NotifyObservers(nullptr, "recording-window-ended", data.get());
  LOG("Sent recording-window-ended for window %" PRIu64 " (outer %" PRIu64 ")",
      aWindowId, outerID);
}

bool MediaManager::IsWindowListenerStillActive(
    const RefPtr<GetUserMediaWindowListener>& aListener) {
  MOZ_DIAGNOSTIC_ASSERT(aListener);
  return aListener && aListener == GetWindowListener(aListener->WindowID());
}

void MediaManager::GetPref(nsIPrefBranch* aBranch, const char* aPref,
                           const char* aData, int32_t* aVal) {
  int32_t temp;
  if (aData == nullptr || strcmp(aPref, aData) == 0) {
    if (NS_SUCCEEDED(aBranch->GetIntPref(aPref, &temp))) {
      *aVal = temp;
    }
  }
}

void MediaManager::GetPrefBool(nsIPrefBranch* aBranch, const char* aPref,
                               const char* aData, bool* aVal) {
  bool temp;
  if (aData == nullptr || strcmp(aPref, aData) == 0) {
    if (NS_SUCCEEDED(aBranch->GetBoolPref(aPref, &temp))) {
      *aVal = temp;
    }
  }
}

void MediaManager::GetPrefs(nsIPrefBranch* aBranch, const char* aData) {
  GetPref(aBranch, "media.navigator.video.default_width", aData,
          &mPrefs.mWidth);
  GetPref(aBranch, "media.navigator.video.default_height", aData,
          &mPrefs.mHeight);
  GetPref(aBranch, "media.navigator.video.default_fps", aData, &mPrefs.mFPS);
  GetPref(aBranch, "media.navigator.audio.fake_frequency", aData,
          &mPrefs.mFreq);
#ifdef MOZ_WEBRTC
  GetPrefBool(aBranch, "media.getusermedia.aec_enabled", aData, &mPrefs.mAecOn);
  GetPrefBool(aBranch, "media.getusermedia.agc_enabled", aData, &mPrefs.mAgcOn);
  GetPrefBool(aBranch, "media.getusermedia.noise_enabled", aData,
              &mPrefs.mNoiseOn);
  GetPref(aBranch, "media.getusermedia.aec", aData, &mPrefs.mAec);
  GetPref(aBranch, "media.getusermedia.agc", aData, &mPrefs.mAgc);
  GetPref(aBranch, "media.getusermedia.noise", aData, &mPrefs.mNoise);
  GetPrefBool(aBranch, "media.getusermedia.aec_extended_filter", aData,
              &mPrefs.mExtendedFilter);
  GetPrefBool(aBranch, "media.getusermedia.aec_aec_delay_agnostic", aData,
              &mPrefs.mDelayAgnostic);
  GetPref(aBranch, "media.getusermedia.channels", aData, &mPrefs.mChannels);
  GetPrefBool(aBranch, "media.ondevicechange.fakeDeviceChangeEvent.enabled",
              aData, &mPrefs.mFakeDeviceChangeEventOn);
#endif
  GetPrefBool(aBranch, "media.navigator.audio.full_duplex", aData,
              &mPrefs.mFullDuplex);
}

void MediaManager::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  if (sHasShutdown) {
    return;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();

  obs->RemoveObserver(this, "last-pb-context-exited");
  obs->RemoveObserver(this, "getUserMedia:privileged:allow");
  obs->RemoveObserver(this, "getUserMedia:response:allow");
  obs->RemoveObserver(this, "getUserMedia:response:deny");
  obs->RemoveObserver(this, "getUserMedia:response:noOSPermission");
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
    prefs->RemoveObserver("media.ondevicechange.fakeDeviceChangeEvent.enabled",
                          this);
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
    nsTArray<RefPtr<GetUserMediaWindowListener>> listeners(
        GetActiveWindows()->Count());
    for (auto iter = GetActiveWindows()->Iter(); !iter.Done(); iter.Next()) {
      listeners.AppendElement(iter.UserData());
    }
    for (auto& listener : listeners) {
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

  class ShutdownTask : public Runnable {
   public:
    ShutdownTask(RefPtr<MediaManager> aManager, RefPtr<Runnable> aReply)
        : mozilla::Runnable("ShutdownTask"),
          mManager(std::move(aManager)),
          mReply(std::move(aReply)) {}

   private:
    NS_IMETHOD
    Run() override {
      LOG("MediaManager Thread Shutdown");
      MOZ_ASSERT(MediaManager::IsInMediaThread());
      // Must shutdown backend on MediaManager thread, since that's where we
      // started it from!
      {
        if (mManager->mBackend) {
          mManager->mBackend->Shutdown();  // ok to invoke multiple times
          mManager->mBackend->RemoveDeviceChangeCallback(mManager);
        }
      }
      mozilla::ipc::BackgroundChild::CloseForCurrentThread();
      // must explicitly do this before dispatching the reply, since the reply
      // may kill us with Stop()
      mManager->mBackend =
          nullptr;  // last reference, will invoke Shutdown() again

      if (NS_FAILED(NS_DispatchToMainThread(mReply.forget()))) {
        LOG("Will leak thread: DispatchToMainthread of reply runnable failed "
            "in MediaManager shutdown");
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

  // Release the backend (and call Shutdown()) from within the MediaManager
  // thread Don't use MediaManager::PostTask() because we're sHasShutdown=true
  // here!
  auto shutdown = MakeRefPtr<ShutdownTask>(
      this, media::NewRunnableFrom([this, self = RefPtr<MediaManager>(this)]() {
        LOG("MediaManager shutdown lambda running, releasing MediaManager "
            "singleton and thread");
        if (mMediaThread) {
          mMediaThread->Stop();
        }
        // Remove async shutdown blocker
        media::GetShutdownBarrier()->RemoveBlocker(
            sSingleton->mShutdownBlocker);

        // we hold a ref to 'self' which is the same as sSingleton
        sSingleton = nullptr;
        return NS_OK;
      }));
  mMediaThread->message_loop()->PostTask(shutdown.forget());
}

void MediaManager::SendPendingGUMRequest() {
  if (mPendingGUMRequest.Length() > 0) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    obs->NotifyObservers(mPendingGUMRequest[0], "getUserMedia:request",
                         nullptr);
    mPendingGUMRequest.RemoveElementAt(0);
  }
}

bool IsGUMResponseNoAccess(const char* aTopic,
                           MediaMgrError::Name& aErrorName) {
  if (!strcmp(aTopic, "getUserMedia:response:deny")) {
    aErrorName = MediaMgrError::Name::NotAllowedError;
    return true;
  }

  if (!strcmp(aTopic, "getUserMedia:response:noOSPermission")) {
    aErrorName = MediaMgrError::Name::NotFoundError;
    return true;
  }

  return false;
}

nsresult MediaManager::Observe(nsISupports* aSubject, const char* aTopic,
                               const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  MediaMgrError::Name gumNoAccessError = MediaMgrError::Name::NotAllowedError;

  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> branch(do_QueryInterface(aSubject));
    if (branch) {
      GetPrefs(branch, NS_ConvertUTF16toUTF8(aData).get());
      LOG("%s: %dx%d @%dfps", __FUNCTION__, mPrefs.mWidth, mPrefs.mHeight,
          mPrefs.mFPS);
    }
  } else if (!strcmp(aTopic, "last-pb-context-exited")) {
    // Clear memory of private-browsing-specific deviceIds. Fire and forget.
    media::SanitizeOriginKeys(0, true);
    return NS_OK;
  } else if (!strcmp(aTopic, "getUserMedia:got-device-permission")) {
    MOZ_ASSERT(aSubject);
    nsCOMPtr<nsIRunnable> task = do_QueryInterface(aSubject);
    MediaManager::PostTask(NewTaskFrom([task] { task->Run(); }));
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
        MOZ_ASSERT(device);  // shouldn't be returning anything else...
        if (!device) {
          continue;
        }

        // Casting here is safe because a MediaDevice is created
        // only in Gecko side, JS can only query for an instance.
        MediaDevice* dev = static_cast<MediaDevice*>(device.get());
        if (dev->mKind == MediaDeviceKind::Videoinput) {
          if (!videoFound) {
            task->SetVideoDevice(dev);
            videoFound = true;
          }
        } else if (dev->mKind == MediaDeviceKind::Audioinput) {
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

  } else if (IsGUMResponseNoAccess(aTopic, gumNoAccessError)) {
    nsString key(aData);
    RefPtr<GetUserMediaTask> task;
    mActiveCallbacks.Remove(key, getter_AddRefs(task));
    if (task) {
      task->Denied(gumNoAccessError);
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
      uint64_t windowID = PromiseFlatString(Substring(data, strlen("screen:")))
                              .ToInteger64(&rv);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (NS_SUCCEEDED(rv)) {
        LOG("Revoking Screen/windowCapture access for window %" PRIu64,
            windowID);
        StopScreensharing(windowID);
      }
    } else {
      uint64_t windowID = nsString(aData).ToInteger64(&rv);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (NS_SUCCEEDED(rv)) {
        LOG("Revoking MediaCapture access for window %" PRIu64, windowID);
        OnNavigation(windowID);
      }
    }
    return NS_OK;
  }

  return NS_OK;
}

nsresult MediaManager::GetActiveMediaCaptureWindows(nsIArray** aArray) {
  MOZ_ASSERT(aArray);

  nsCOMPtr<nsIMutableArray> array = nsArray::Create();

  for (auto iter = mActiveWindows.Iter(); !iter.Done(); iter.Next()) {
    const uint64_t& id = iter.Key();
    RefPtr<GetUserMediaWindowListener> winListener = iter.UserData();
    if (!winListener) {
      continue;
    }

    auto* window = nsGlobalWindowInner::GetInnerWindowWithId(id);
    MOZ_ASSERT(window);
    // XXXkhuey ...
    if (!window) {
      continue;
    }

    if (winListener->CapturingVideo() || winListener->CapturingAudio()) {
      array->AppendElement(ToSupports(window));
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
                                      uint16_t* aCamera, uint16_t* aMicrophone,
                                      uint16_t* aScreen, uint16_t* aWindow,
                                      uint16_t* aBrowser) {
  MOZ_ASSERT(NS_IsMainThread());

  CaptureState camera = CaptureState::Off;
  CaptureState microphone = CaptureState::Off;
  CaptureState screen = CaptureState::Off;
  CaptureState window = CaptureState::Off;
  CaptureState browser = CaptureState::Off;

  nsCOMPtr<nsPIDOMWindowInner> piWin = do_QueryInterface(aCapturedWindow);
  if (piWin) {
    IterateWindowListeners(
        piWin, [&camera, &microphone, &screen, &window,
                &browser](const RefPtr<GetUserMediaWindowListener>& aListener) {
          camera = CombineCaptureState(
              camera, aListener->CapturingSource(MediaSourceEnum::Camera));
          microphone = CombineCaptureState(
              microphone,
              aListener->CapturingSource(MediaSourceEnum::Microphone));
          screen = CombineCaptureState(
              screen, aListener->CapturingSource(MediaSourceEnum::Screen));
          window = CombineCaptureState(
              window, aListener->CapturingSource(MediaSourceEnum::Window));
          browser = CombineCaptureState(
              browser, aListener->CapturingSource(MediaSourceEnum::Browser));
        });
  }

  *aCamera = FromCaptureState(camera);
  *aMicrophone = FromCaptureState(microphone);
  *aScreen = FromCaptureState(screen);
  *aWindow = FromCaptureState(window);
  *aBrowser = FromCaptureState(browser);

  LOG("%s: window %" PRIu64 " capturing %s %s %s %s %s", __FUNCTION__,
      piWin ? piWin->WindowID() : -1,
      *aCamera == nsIMediaManagerService::STATE_CAPTURE_ENABLED
          ? "camera (enabled)"
          : (*aCamera == nsIMediaManagerService::STATE_CAPTURE_DISABLED
                 ? "camera (disabled)"
                 : ""),
      *aMicrophone == nsIMediaManagerService::STATE_CAPTURE_ENABLED
          ? "microphone (enabled)"
          : (*aMicrophone == nsIMediaManagerService::STATE_CAPTURE_DISABLED
                 ? "microphone (disabled)"
                 : ""),
      *aScreen ? "screenshare" : "", *aWindow ? "windowshare" : "",
      *aBrowser ? "browsershare" : "");

  return NS_OK;
}

NS_IMETHODIMP
MediaManager::SanitizeDeviceIds(int64_t aSinceWhen) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("%s: sinceWhen = %" PRId64, __FUNCTION__, aSinceWhen);

  media::SanitizeOriginKeys(aSinceWhen, false);  // we fire and forget
  return NS_OK;
}

void MediaManager::StopScreensharing(uint64_t aWindowID) {
  // We need to stop window/screensharing for all streams in all innerwindows
  // that correspond to that outerwindow.

  auto* window = nsGlobalWindowInner::GetInnerWindowWithId(aWindowID);
  if (!window) {
    return;
  }
  IterateWindowListeners(
      window, [](const RefPtr<GetUserMediaWindowListener>& aListener) {
        aListener->StopSharing();
      });
}

template <typename FunctionType>
void MediaManager::IterateWindowListeners(nsPIDOMWindowInner* aWindow,
                                          const FunctionType& aCallback) {
  // Iterate the docshell tree to find all the child windows, and for each
  // invoke the callback
  if (aWindow) {
    {
      uint64_t windowID = aWindow->WindowID();
      RefPtr<GetUserMediaWindowListener> listener = GetWindowListener(windowID);
      if (listener) {
        aCallback(listener);
      }
      // NB: `listener` might have been destroyed.
    }

    // iterate any children of *this* window (iframes, etc)
    nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
    if (docShell) {
      int32_t i, count;
      docShell->GetInProcessChildCount(&count);
      for (i = 0; i < count; ++i) {
        nsCOMPtr<nsIDocShellTreeItem> item;
        docShell->GetInProcessChildAt(i, getter_AddRefs(item));
        nsCOMPtr<nsPIDOMWindowOuter> winOuter =
            item ? item->GetWindow() : nullptr;

        if (winOuter) {
          IterateWindowListeners(winOuter->GetCurrentInnerWindow(), aCallback);
        }
      }
    }
  }
}

void MediaManager::StopMediaStreams() {
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

bool MediaManager::IsActivelyCapturingOrHasAPermission(uint64_t aWindowId) {
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

  Document* doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return false;
  }

  nsIPrincipal* principal = window->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    return false;
  }

  // Check if this site has persistent permissions.
  nsresult rv;
  nsCOMPtr<nsIPermissionManager> mgr =
      do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;  // no permission manager no permissions!
  }

  uint32_t audio = nsIPermissionManager::UNKNOWN_ACTION;
  uint32_t video = nsIPermissionManager::UNKNOWN_ACTION;
  {
    if (!FeaturePolicyUtils::IsFeatureAllowed(
            doc, NS_LITERAL_STRING("microphone"))) {
      audio = nsIPermissionManager::DENY_ACTION;
    } else {
      rv = mgr->TestExactPermissionFromPrincipal(
          principal, NS_LITERAL_CSTRING("microphone"), &audio);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }
    }

    if (!FeaturePolicyUtils::IsFeatureAllowed(doc,
                                              NS_LITERAL_STRING("camera"))) {
      video = nsIPermissionManager::DENY_ACTION;
    } else {
      rv = mgr->TestExactPermissionFromPrincipal(
          principal, NS_LITERAL_CSTRING("camera"), &video);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }
    }
  }
  return audio == nsIPermissionManager::ALLOW_ACTION ||
         video == nsIPermissionManager::ALLOW_ACTION;
}

SourceListener::SourceListener()
    : mStopped(false),
      mMainThreadCheck(nullptr),
      mPrincipalHandle(PRINCIPAL_HANDLE_NONE),
      mWindowListener(nullptr) {}

void SourceListener::Register(GetUserMediaWindowListener* aListener) {
  LOG("SourceListener %p registering with window listener %p", this, aListener);

  MOZ_ASSERT(aListener, "No listener");
  MOZ_ASSERT(!mWindowListener, "Already registered");
  MOZ_ASSERT(!Activated(), "Already activated");

  mPrincipalHandle = aListener->GetPrincipalHandle();
  mWindowListener = aListener;
}

void SourceListener::Activate(RefPtr<MediaDevice> aAudioDevice,
                              RefPtr<LocalTrackSource> aAudioTrackSource,
                              RefPtr<MediaDevice> aVideoDevice,
                              RefPtr<LocalTrackSource> aVideoTrackSource) {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

  LOG("SourceListener %p activating audio=%p video=%p", this,
      aAudioDevice.get(), aVideoDevice.get());

  MOZ_ASSERT(!mStopped, "Cannot activate stopped source listener");
  MOZ_ASSERT(!Activated(), "Already activated");

  mMainThreadCheck = PR_GetCurrentThread();
  if (aAudioDevice) {
    bool offWhileDisabled =
        aAudioDevice->GetMediaSource() == MediaSourceEnum::Microphone &&
        Preferences::GetBool(
            "media.getusermedia.microphone.off_while_disabled.enabled", true);
    mAudioDeviceState =
        MakeUnique<DeviceState>(std::move(aAudioDevice),
                                std::move(aAudioTrackSource), offWhileDisabled);
  }

  if (aVideoDevice) {
    bool offWhileDisabled =
        aVideoDevice->GetMediaSource() == MediaSourceEnum::Camera &&
        Preferences::GetBool(
            "media.getusermedia.camera.off_while_disabled.enabled", true);
    mVideoDeviceState =
        MakeUnique<DeviceState>(std::move(aVideoDevice),
                                std::move(aVideoTrackSource), offWhileDisabled);
  }
}

RefPtr<SourceListener::SourceListenerPromise>
SourceListener::InitializeAsync() {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");
  MOZ_DIAGNOSTIC_ASSERT(!mStopped);

  return MediaManager::PostTask<SourceListenerPromise>(
             __func__,
             [principal = GetPrincipalHandle(),
              audioDevice =
                  mAudioDeviceState ? mAudioDeviceState->mDevice : nullptr,
              audioStream = mAudioDeviceState
                                ? mAudioDeviceState->mTrackSource->mStream
                                : nullptr,
              videoDevice =
                  mVideoDeviceState ? mVideoDeviceState->mDevice : nullptr,
              videoStream = mVideoDeviceState
                                ? mVideoDeviceState->mTrackSource->mStream
                                : nullptr](
                 MozPromiseHolder<SourceListenerPromise>& aHolder) {
               if (audioDevice) {
                 audioDevice->SetTrack(audioStream->AsSourceStream(),
                                       kAudioTrack, principal);
               }

               if (videoDevice) {
                 videoDevice->SetTrack(videoStream->AsSourceStream(),
                                       kVideoTrack, principal);
               }

               if (audioDevice) {
                 nsresult rv = audioDevice->Start();
                 if (rv == NS_ERROR_NOT_AVAILABLE) {
                   PR_Sleep(200);
                   rv = audioDevice->Start();
                 }
                 if (NS_FAILED(rv)) {
                   nsString log;
                   if (rv == NS_ERROR_NOT_AVAILABLE) {
                     log.AssignLiteral("Concurrent mic process limit.");
                     aHolder.Reject(
                         MakeRefPtr<MediaMgrError>(
                             MediaMgrError::Name::NotReadableError, log),
                         __func__);
                     return;
                   }
                   log.AssignLiteral("Starting audio failed");
                   aHolder.Reject(MakeRefPtr<MediaMgrError>(
                                      MediaMgrError::Name::AbortError, log),
                                  __func__);
                   return;
                 }
               }

               if (videoDevice) {
                 nsresult rv = videoDevice->Start();
                 if (NS_FAILED(rv)) {
                   if (audioDevice) {
                     if (NS_WARN_IF(NS_FAILED(audioDevice->Stop()))) {
                       MOZ_ASSERT_UNREACHABLE("Stopping audio failed");
                     }
                   }
                   nsString log;
                   log.AssignLiteral("Starting video failed");
                   aHolder.Reject(MakeRefPtr<MediaMgrError>(
                                      MediaMgrError::Name::AbortError, log),
                                  __func__);
                   return;
                 }
               }

               LOG("started all sources");
               aHolder.Resolve(true, __func__);
             })
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self = RefPtr<SourceListener>(this), this]() {
            if (mStopped) {
              // We were shut down during the async init
              return SourceListenerPromise::CreateAndResolve(true, __func__);
            }

            for (DeviceState* state :
                 {mAudioDeviceState.get(), mVideoDeviceState.get()}) {
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
            return SourceListenerPromise::CreateAndResolve(true, __func__);
          },
          [self = RefPtr<SourceListener>(this),
           this](RefPtr<MediaMgrError>&& aResult) {
            if (mStopped) {
              return SourceListenerPromise::CreateAndReject(std::move(aResult),
                                                            __func__);
            }

            for (DeviceState* state :
                 {mAudioDeviceState.get(), mVideoDeviceState.get()}) {
              if (!state) {
                continue;
              }
              MOZ_DIAGNOSTIC_ASSERT(!state->mTrackEnabled);
              MOZ_DIAGNOSTIC_ASSERT(!state->mDeviceEnabled);
              MOZ_DIAGNOSTIC_ASSERT(!state->mStopped);

              state->mStopped = true;
            }
            return SourceListenerPromise::CreateAndReject(std::move(aResult),
                                                          __func__);
          });
}

void SourceListener::Stop() {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

  // StopSharing() has some special logic, at least for audio capture.
  // It must be called when all tracks have stopped, before setting mStopped.
  StopSharing();

  if (mStopped) {
    return;
  }
  mStopped = true;

  LOG("SourceListener %p stopping", this);

  if (mAudioDeviceState) {
    mAudioDeviceState->mDisableTimer->Cancel();
    if (!mAudioDeviceState->mStopped) {
      StopTrack(kAudioTrack);
    }
  }
  if (mVideoDeviceState) {
    mVideoDeviceState->mDisableTimer->Cancel();
    if (!mVideoDeviceState->mStopped) {
      StopTrack(kVideoTrack);
    }
  }

  mWindowListener->Remove(this);
  mWindowListener = nullptr;
}

void SourceListener::StopTrack(TrackID aTrackID) {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");
  MOZ_ASSERT(Activated(), "No device to stop");
  MOZ_ASSERT(aTrackID == kAudioTrack || aTrackID == kVideoTrack,
             "Unknown track id");
  DeviceState& state = GetDeviceStateFor(aTrackID);

  LOG("SourceListener %p stopping %s track %d", this,
      aTrackID == kAudioTrack ? "audio" : "video", aTrackID);

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

  MOZ_ASSERT(mWindowListener, "Should still have window listener");
  mWindowListener->ChromeAffectingStateChanged();

  if ((!mAudioDeviceState || mAudioDeviceState->mStopped) &&
      (!mVideoDeviceState || mVideoDeviceState->mStopped)) {
    LOG("SourceListener %p this was the last track stopped", this);
    Stop();
  }
}

void SourceListener::GetSettingsFor(TrackID aTrackID,
                                    MediaTrackSettings& aOutSettings) const {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");
  DeviceState& state = GetDeviceStateFor(aTrackID);
  state.mDevice->GetSettings(aOutSettings);

  MediaSourceEnum mediaSource = state.mDevice->GetMediaSource();
  if (mediaSource == MediaSourceEnum::Camera ||
      mediaSource == MediaSourceEnum::Microphone) {
    aOutSettings.mDeviceId.Construct(state.mDevice->mID);
    aOutSettings.mGroupId.Construct(state.mDevice->mGroupID);
  }
}

void SourceListener::SetEnabledFor(TrackID aTrackID, bool aEnable) {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");
  MOZ_ASSERT(Activated(), "No device to set enabled state for");
  MOZ_ASSERT(aTrackID == kAudioTrack || aTrackID == kVideoTrack,
             "Unknown track id");

  LOG("SourceListener %p %s %s track %d", this,
      aEnable ? "enabling" : "disabling",
      aTrackID == kAudioTrack ? "audio" : "video", aTrackID);

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
    const TimeDuration maxDelay =
        TimeDuration::FromMilliseconds(Preferences::GetUint(
            aTrackID == kAudioTrack
                ? "media.getusermedia.microphone.off_while_disabled.delay_ms"
                : "media.getusermedia.camera.off_while_disabled.delay_ms",
            3000));
    const TimeDuration durationEnabled =
        TimeStamp::Now() - state.mTrackEnabledTime;
    const TimeDuration delay = TimeDuration::Max(
        TimeDuration::FromMilliseconds(0), maxDelay - durationEnabled);
    timerPromise = state.mDisableTimer->WaitFor(delay, __func__);
  }

  typedef MozPromise<nsresult, bool, /* IsExclusive = */ true>
      DeviceOperationPromise;
  RefPtr<SourceListener> self = this;
  timerPromise
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self, this, &state, aTrackID, aEnable]() mutable {
            MOZ_ASSERT(state.mDeviceEnabled != aEnable,
                       "Device operation hasn't started");
            MOZ_ASSERT(state.mOperationInProgress,
                       "It's our responsibility to reset the inProgress state");

            LOG("SourceListener %p %s %s track %d - starting device operation",
                this, aEnable ? "enabling" : "disabling",
                aTrackID == kAudioTrack ? "audio" : "video", aTrackID);

            if (state.mStopped) {
              // Source was stopped between timer resolving and this runnable.
              return DeviceOperationPromise::CreateAndResolve(NS_ERROR_ABORT,
                                                              __func__);
            }

            state.mDeviceEnabled = aEnable;

            if (mWindowListener) {
              mWindowListener->ChromeAffectingStateChanged();
            }

            if (!state.mOffWhileDisabled) {
              // If the feature to turn a device off while disabled is itself
              // disabled we shortcut the device operation and tell the
              // ux-updating code that everything went fine.
              return DeviceOperationPromise::CreateAndResolve(NS_OK, __func__);
            }

            return MediaManager::PostTask<DeviceOperationPromise>(
                __func__, [self, device = state.mDevice, aEnable](
                              MozPromiseHolder<DeviceOperationPromise>& h) {
                  h.Resolve(aEnable ? device->Start() : device->Stop(),
                            __func__);
                });
          },
          []() {
            // Timer was canceled by us. We signal this with NS_ERROR_ABORT.
            return DeviceOperationPromise::CreateAndResolve(NS_ERROR_ABORT,
                                                            __func__);
          })
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self, this, &state, aTrackID, aEnable](nsresult aResult) mutable {
            MOZ_ASSERT_IF(aResult != NS_ERROR_ABORT,
                          state.mDeviceEnabled == aEnable);
            MOZ_ASSERT(state.mOperationInProgress);
            state.mOperationInProgress = false;

            if (state.mStopped) {
              // Device was stopped on main thread during the operation. Nothing
              // to do.
              return;
            }

            LOG("SourceListener %p %s %s track %d %s", this,
                aEnable ? "enabling" : "disabling",
                aTrackID == kAudioTrack ? "audio" : "video", aTrackID,
                NS_SUCCEEDED(aResult) ? "succeeded" : "failed");

            if (NS_FAILED(aResult) && aResult != NS_ERROR_ABORT) {
              // This path handles errors from starting or stopping the device.
              // NS_ERROR_ABORT are for cases where *we* aborted. They need
              // graceful handling.
              if (aEnable) {
                // Starting the device failed. Stopping the track here will make
                // the MediaStreamTrack end after a pass through the
                // MediaStreamGraph.
                StopTrack(aTrackID);
              } else {
                // Stopping the device failed. This is odd, but not fatal.
                MOZ_ASSERT_UNREACHABLE("The device should be stoppable");

                // To keep our internal state sane in this case, we disallow
                // future stops due to disable.
                state.mOffWhileDisabled = false;
              }
              return;
            }

            // This path is for a device operation aResult that was success or
            // NS_ERROR_ABORT (*we* canceled the operation).
            // At this point we have to follow up on the intended state, i.e.,
            // update the device state if the track state changed in the
            // meantime.

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
          },
          []() { MOZ_ASSERT_UNREACHABLE("Unexpected and unhandled reject"); });
}

void SourceListener::StopSharing() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mStopped) {
    return;
  }

  MOZ_RELEASE_ASSERT(mWindowListener);
  LOG("SourceListener %p StopSharing", this);

  RefPtr<SourceListener> self(this);
  if (mVideoDeviceState && (mVideoDeviceState->mDevice->GetMediaSource() ==
                                MediaSourceEnum::Screen ||
                            mVideoDeviceState->mDevice->GetMediaSource() ==
                                MediaSourceEnum::Window)) {
    // We want to stop the whole stream if there's no audio;
    // just the video track if we have both.
    // StopTrack figures this out for us.
    StopTrack(kVideoTrack);
  }
  if (mAudioDeviceState && mAudioDeviceState->mDevice->GetMediaSource() ==
                               MediaSourceEnum::AudioCapture) {
    static_cast<AudioCaptureTrackSource*>(mAudioDeviceState->mTrackSource.get())
        ->Stop();
  }
}

bool SourceListener::CapturingVideo() const {
  MOZ_ASSERT(NS_IsMainThread());
  return Activated() && mVideoDeviceState && !mVideoDeviceState->mStopped &&
         (!mVideoDeviceState->mDevice->mSource->IsFake() ||
          Preferences::GetBool("media.navigator.permission.fake"));
}

bool SourceListener::CapturingAudio() const {
  MOZ_ASSERT(NS_IsMainThread());
  return Activated() && mAudioDeviceState && !mAudioDeviceState->mStopped &&
         (!mAudioDeviceState->mDevice->mSource->IsFake() ||
          Preferences::GetBool("media.navigator.permission.fake"));
}

CaptureState SourceListener::CapturingSource(MediaSourceEnum aSource) const {
  MOZ_ASSERT(NS_IsMainThread());
  if ((!GetVideoDevice() || GetVideoDevice()->GetMediaSource() != aSource) &&
      (!GetAudioDevice() || GetAudioDevice()->GetMediaSource() != aSource)) {
    // This SourceListener doesn't capture a matching source
    return CaptureState::Off;
  }

  DeviceState& state =
      (GetAudioDevice() && GetAudioDevice()->GetMediaSource() == aSource)
          ? *mAudioDeviceState
          : *mVideoDeviceState;
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

RefPtr<SourceListener::SourceListenerPromise>
SourceListener::ApplyConstraintsToTrack(
    TrackID aTrackID, const MediaTrackConstraints& aConstraints,
    CallerType aCallerType) {
  MOZ_ASSERT(NS_IsMainThread());
  DeviceState& state = GetDeviceStateFor(aTrackID);

  if (mStopped || state.mStopped) {
    LOG("gUM %s track %d applyConstraints, but source is stopped",
        aTrackID == kAudioTrack ? "audio" : "video", aTrackID);
    return SourceListenerPromise::CreateAndResolve(false, __func__);
  }

  MediaManager* mgr = MediaManager::GetIfExists();
  if (!mgr) {
    return SourceListenerPromise::CreateAndResolve(false, __func__);
  }

  return MediaManager::PostTask<SourceListenerPromise>(
      __func__, [device = state.mDevice, aConstraints,
                 isChrome = aCallerType == CallerType::System](
                    MozPromiseHolder<SourceListenerPromise>& aHolder) mutable {
        MOZ_ASSERT(MediaManager::IsInMediaThread());
        MediaManager* mgr = MediaManager::GetIfExists();
        MOZ_RELEASE_ASSERT(mgr);  // Must exist while media thread is alive
        const char* badConstraint = nullptr;
        nsresult rv =
            device->Reconfigure(aConstraints, mgr->mPrefs, &badConstraint);
        if (NS_FAILED(rv)) {
          if (rv == NS_ERROR_INVALID_ARG) {
            // Reconfigure failed due to constraints
            if (!badConstraint) {
              nsTArray<RefPtr<MediaDevice>> devices;
              devices.AppendElement(device);
              badConstraint = MediaConstraintsHelper::SelectSettings(
                  NormalizedConstraints(aConstraints), devices, isChrome);
            }
          } else {
            // Unexpected. ApplyConstraints* cannot fail with any other error.
            badConstraint = "";
            LOG("ApplyConstraintsToTrack-Task: Unexpected fail %" PRIx32,
                static_cast<uint32_t>(rv));
          }

          aHolder.Reject(
              MakeRefPtr<MediaMgrError>(
                  MediaMgrError::Name::OverconstrainedError,
                  NS_LITERAL_STRING(""), NS_ConvertASCIItoUTF16(badConstraint)),
              __func__);
          return;
        }
        // Reconfigure was successful
        aHolder.Resolve(false, __func__);
      });
}

PrincipalHandle SourceListener::GetPrincipalHandle() const {
  return mPrincipalHandle;
}

DeviceState& SourceListener::GetDeviceStateFor(TrackID aTrackID) const {
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
void GetUserMediaWindowListener::StopSharing() {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

  for (auto& l : nsTArray<RefPtr<SourceListener>>(mActiveListeners)) {
    l->StopSharing();
  }
}

void GetUserMediaWindowListener::StopRawID(const nsString& removedDeviceID) {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

  for (auto& source : nsTArray<RefPtr<SourceListener>>(mActiveListeners)) {
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

void GetUserMediaWindowListener::ChromeAffectingStateChanged() {
  MOZ_ASSERT(NS_IsMainThread());

  // We wait until stable state before notifying chrome so chrome only does one
  // update if more updates happen in this event loop.

  if (mChromeNotificationTaskPosted) {
    return;
  }

  nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod("GetUserMediaWindowListener::NotifyChrome", this,
                        &GetUserMediaWindowListener::NotifyChrome);
  nsContentUtils::RunInStableState(runnable.forget());
  mChromeNotificationTaskPosted = true;
}

void GetUserMediaWindowListener::NotifyChrome() {
  MOZ_ASSERT(mChromeNotificationTaskPosted);
  mChromeNotificationTaskPosted = false;

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "MediaManager::NotifyChrome", [windowID = mWindowID]() {
        auto* window = nsGlobalWindowInner::GetInnerWindowWithId(windowID);
        if (!window) {
          MOZ_ASSERT_UNREACHABLE("Should have window");
          return;
        }

        nsresult rv = MediaManager::NotifyRecordingStatusChange(window);
        if (NS_FAILED(rv)) {
          MOZ_ASSERT_UNREACHABLE("Should be able to notify chrome");
          return;
        }
      }));
}

#undef LOG

}  // namespace mozilla
