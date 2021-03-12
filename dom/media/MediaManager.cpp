/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaManager.h"

#include "AudioCaptureTrack.h"
#include "AudioDeviceInfo.h"
#include "AudioStreamTrack.h"
#include "CubebDeviceEnumerator.h"
#include "MediaTimer.h"
#include "MediaTrackConstraints.h"
#include "MediaTrackGraphImpl.h"
#include "MediaTrackListener.h"
#include "VideoStreamTrack.h"
#include "VideoUtils.h"
#include "mozilla/Base64.h"
#include "mozilla/MozPromise.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/PeerIdentity.h"
#include "mozilla/PermissionDelegateHandler.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Types.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/GetUserMediaRequestBinding.h"
#include "mozilla/dom/MediaDeviceInfo.h"
#include "mozilla/dom/MediaDevices.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/media/MediaChild.h"
#include "mozilla/media/MediaTaskUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsArray.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsHashPropertyBag.h"
#include "nsICryptoHMAC.h"
#include "nsIEventTarget.h"
#include "nsIKeyModule.h"
#include "nsIPermissionManager.h"
#include "nsIUUIDGenerator.h"
#include "nsJSUtils.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsProxyRelease.h"
#include "nsVariant.h"
#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"

/* Using WebRTC backend on Desktops (Mac, Windows, Linux), otherwise default */
#include "MediaEngineDefault.h"
#if defined(MOZ_WEBRTC)
#  include "MediaEngineWebRTC.h"
#  include "MediaEngineWebRTCAudio.h"
#  include "browser_logging/WebRtcLog.h"
#  include "webrtc/modules/audio_processing/include/audio_processing.h"
#endif

#if defined(XP_WIN)
#  include <iphlpapi.h>
#  include <objbase.h>
#  include <tchar.h>
#  include <winsock2.h>

#  include "mozilla/WindowsVersion.h"
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
using dom::Element;
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

  // true if mDevice is currently enabled.
  // A device must be both enabled and unmuted to be turned on and capturing.
  // MainThread only.
  bool mDeviceEnabled = false;

  // true if mDevice is currently muted.
  // A device that is either muted or disabled is turned off and not capturing.
  // MainThread only.
  bool mDeviceMuted;

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
  // are disabled. Only affects disabling; always turns off on user-agent mute.
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
 * MTG threads. But it has a non-threadsafe SupportsWeakPtr for WeakPtr usage
 * only from main thread, to ensure that garbage- and cycle-collected objects
 * don't hold a reference to it during late shutdown.
 */
class SourceListener : public SupportsWeakPtr {
 public:
  typedef MozPromise<bool /* aIgnored */, RefPtr<MediaMgrError>, true>
      SourceListenerPromise;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DELETE_ON_MAIN_THREAD(
      SourceListener)

  SourceListener();

  /**
   * Registers this source listener as belonging to the given window listener.
   * Stop() must be called on registered SourceListeners before destruction.
   */
  void Register(GetUserMediaWindowListener* aListener);

  /**
   * Marks this listener as active and creates internal device states.
   */
  void Activate(RefPtr<MediaDevice> aAudioDevice,
                RefPtr<LocalTrackSource> aAudioTrackSource,
                RefPtr<MediaDevice> aVideoDevice,
                RefPtr<LocalTrackSource> aVideoTrackSource,
                bool aStartVideoMuted, bool aStartAudioMuted);

  /**
   * Posts a task to initialize and start all associated devices.
   */
  RefPtr<SourceListenerPromise> InitializeAsync();

  /**
   * Stops all live tracks, ends the associated MediaTrack and cleans up the
   * weak reference to the associated window listener.
   * This will also tell the window listener to remove its hard reference to
   * this SourceListener, so any caller will need to keep its own hard ref.
   */
  void Stop();

  /**
   * Posts a task to stop the device associated with aTrack and notifies the
   * associated window listener that a track was stopped.
   * Should this track be the last live one to be stopped, we'll also call Stop.
   * This might tell the window listener to remove its hard reference to this
   * SourceListener, so any caller will need to keep its own hard ref.
   */
  void StopTrack(MediaTrack* aTrack);

  /**
   * Like StopTrack with the audio device's track.
   */
  void StopAudioTrack();

  /**
   * Like StopTrack with the video device's track.
   */
  void StopVideoTrack();

  /**
   * Gets the main thread MediaTrackSettings from the MediaEngineSource
   * associated with aTrack.
   */
  void GetSettingsFor(MediaTrack* aTrack,
                      MediaTrackSettings& aOutSettings) const;

  /**
   * Posts a task to set the enabled state of the device associated with
   * aTrack to aEnabled and notifies the associated window listener that a
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
  void SetEnabledFor(MediaTrack* aTrack, bool aEnabled);

  /**
   * Posts a task to set the muted state of the device associated with
   * aTrackSource to aMuted and notifies the associated window listener that a
   * track's state has changed.
   *
   * Turning the hardware off while the device is muted is supported for:
   * - Camera (enabled by default, controlled by pref
   *   "media.getusermedia.camera.off_while_disabled.enabled")
   * - Microphone (disabled by default, controlled by pref
   *   "media.getusermedia.microphone.off_while_disabled.enabled")
   * Screen-, app-, or windowsharing is not supported at this time.
   */
  void SetMutedFor(LocalTrackSource* aTrackSource, bool aMuted);

  /**
   * Stops all screen/app/window/audioCapture sharing, but not camera or
   * microphone.
   */
  void StopSharing();

  /**
   * Mutes or unmutes the associated video device if it is a camera.
   */
  void MuteOrUnmuteCamera(bool aMute);
  void MuteOrUnmuteMicrophone(bool aMute);

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
      MediaTrack* aTrack, const MediaTrackConstraints& aConstraints,
      CallerType aCallerType);

  PrincipalHandle GetPrincipalHandle() const;

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    size_t amount = aMallocSizeOf(this);
    // Assume mPrincipalHandle refers to a principal owned elsewhere.
    // DeviceState does not have support for memory accounting.
    return amount;
  }

 private:
  virtual ~SourceListener() { MOZ_ASSERT(!mWindowListener); }

  using DeviceOperationPromise =
      MozPromise<nsresult, bool, /* IsExclusive = */ true>;

  /**
   * Posts a task to start or stop the device associated with aTrack, based on
   * a passed-in boolean. Private method used by SetEnabledFor and SetMutedFor.
   */
  RefPtr<DeviceOperationPromise> UpdateDevice(MediaTrack* aTrack, bool aOn);

  /**
   * Returns a pointer to the device state for aTrack.
   *
   * This is intended for internal use where we need to figure out which state
   * corresponds to aTrack, not for availability checks. As such, we assert
   * that the device does indeed exist.
   *
   * Since this is a raw pointer and the state lifetime depends on the
   * SourceListener's lifetime, it's internal use only.
   */
  DeviceState& GetDeviceStateFor(MediaTrack* aTrack) const;

  // true after this listener has had all devices stopped. MainThread only.
  bool mStopped;

  // never ever indirect off this; just for assertions
  PRThread* mMainThreadCheck;

  // Set in Register() on main thread, then read from any thread.
  PrincipalHandle mPrincipalHandle;

  // Weak pointer to the window listener that owns us. MainThread only.
  GetUserMediaWindowListener* mWindowListener;

  // Accessed from MediaTrackGraph thread, MediaManager thread, and MainThread
  // No locking needed as they're set on Activate() and never assigned to again.
  UniquePtr<DeviceState> mAudioDeviceState;
  UniquePtr<DeviceState> mVideoDeviceState;
};

/**
 * This class represents a WindowID and handles all MediaTrackListeners
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
  GetUserMediaWindowListener(uint64_t aWindowID,
                             const PrincipalHandle& aPrincipalHandle)
      : mWindowID(aWindowID),
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
                        std::move(aVideoDevice), std::move(aVideoTrackSource),
                        mCamerasAreMuted, mMicrophonesAreMuted);
    mActiveListeners.AppendElement(std::move(aListener));
  }

  /**
   * Removes all SourceListeners from this window listener.
   * Removes this window listener from the list of active windows, so callers
   * need to make sure to hold a strong reference.
   */
  void RemoveAll() {
    MOZ_ASSERT(NS_IsMainThread());

    for (auto& l : mInactiveListeners.Clone()) {
      Remove(l);
    }
    for (auto& l : mActiveListeners.Clone()) {
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
        obs->NotifyWhenScriptSafe(req, "recording-device-stopped", nullptr);
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
   *
   * For use only from GetUserMediaWindowListener and SourceListener.
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
        obs->NotifyWhenScriptSafe(req, "recording-device-stopped", nullptr);
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
        obs->NotifyWhenScriptSafe(req, "recording-device-stopped", nullptr);
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

  void MuteOrUnmuteCameras(bool aMute);
  void MuteOrUnmuteMicrophones(bool aMute);

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

  void GetDevices(
      const RefPtr<MediaManager::MediaDeviceSetRefCnt>& aOutDevices) {
    for (auto& l : mActiveListeners) {
      if (RefPtr<MediaDevice> device = l->GetAudioDevice()) {
        aOutDevices->AppendElement(device);
      } else if (RefPtr<MediaDevice> device = l->GetVideoDevice()) {
        aOutDevices->AppendElement(device);
      }
    }
  }

  uint64_t WindowID() const { return mWindowID; }

  PrincipalHandle GetPrincipalHandle() const { return mPrincipalHandle; }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    size_t amount = aMallocSizeOf(this);
    // Assume mPrincipalHandle refers to a principal owned elsewhere.
    amount += mInactiveListeners.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (const RefPtr<SourceListener>& listener : mInactiveListeners) {
      amount += listener->SizeOfIncludingThis(aMallocSizeOf);
    }
    amount += mActiveListeners.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (const RefPtr<SourceListener>& listener : mActiveListeners) {
      amount += listener->SizeOfIncludingThis(aMallocSizeOf);
    }
    return amount;
  }

 private:
  ~GetUserMediaWindowListener() {
    MOZ_ASSERT(mInactiveListeners.Length() == 0,
               "Inactive listeners should already be removed");
    MOZ_ASSERT(mActiveListeners.Length() == 0,
               "Active listeners should already be removed");
  }

  uint64_t mWindowID;
  const PrincipalHandle mPrincipalHandle;

  // true if we have scheduled a task to notify chrome in the next stable state.
  // The task will reset this to false. MainThread only.
  bool mChromeNotificationTaskPosted;

  nsTArray<RefPtr<SourceListener>> mInactiveListeners;
  nsTArray<RefPtr<SourceListener>> mActiveListeners;

  // Whether camera and microphone access in this window are currently
  // User Agent (UA) muted. When true, new and cloned tracks must start
  // out muted, to avoid JS circumventing UA mute. Per-camera and
  // per-microphone UA muting is not supported.
  bool mCamerasAreMuted = false;
  bool mMicrophonesAreMuted = false;
};

class LocalTrackSource : public MediaStreamTrackSource {
 public:
  LocalTrackSource(nsIPrincipal* aPrincipal, const nsString& aLabel,
                   const RefPtr<SourceListener>& aListener,
                   MediaSourceEnum aSource, MediaTrack* aTrack,
                   RefPtr<PeerIdentity> aPeerIdentity)
      : MediaStreamTrackSource(aPrincipal, aLabel),
        mSource(aSource),
        mTrack(aTrack),
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
    return mListener->ApplyConstraintsToTrack(mTrack, aConstraints,
                                              aCallerType);
  }

  void GetSettings(MediaTrackSettings& aOutSettings) override {
    if (mListener) {
      mListener->GetSettingsFor(mTrack, aOutSettings);
    }
  }

  void Stop() override {
    if (mListener) {
      mListener->StopTrack(mTrack);
      mListener = nullptr;
    }
    if (!mTrack->IsDestroyed()) {
      mTrack->Destroy();
    }
  }

  void Disable() override {
    if (mListener) {
      mListener->SetEnabledFor(mTrack, false);
    }
  }

  void Enable() override {
    if (mListener) {
      mListener->SetEnabledFor(mTrack, true);
    }
  }

  void Mute() {
    MutedChanged(true);
    mTrack->SetDisabledTrackMode(DisabledTrackMode::SILENCE_BLACK);
  }

  void Unmute() {
    MutedChanged(false);
    mTrack->SetDisabledTrackMode(DisabledTrackMode::ENABLED);
  }

  const MediaSourceEnum mSource;
  const RefPtr<MediaTrack> mTrack;
  const RefPtr<const PeerIdentity> mPeerIdentity;

 protected:
  ~LocalTrackSource() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mTrack->IsDestroyed());
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
                          AudioCaptureTrack* aAudioCaptureTrack,
                          RefPtr<PeerIdentity> aPeerIdentity)
      : LocalTrackSource(aPrincipal, aLabel, nullptr,
                         MediaSourceEnum::AudioCapture, aAudioCaptureTrack,
                         std::move(aPeerIdentity)),
        mWindow(aWindow),
        mAudioCaptureTrack(aAudioCaptureTrack) {
    mAudioCaptureTrack->Start();
    mAudioCaptureTrack->Graph()->RegisterCaptureTrackForWindow(
        mWindow->WindowID(), mAudioCaptureTrack);
    mWindow->SetAudioCapture(true);
  }

  void Stop() override {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mAudioCaptureTrack->IsDestroyed()) {
      MOZ_ASSERT(mWindow);
      mWindow->SetAudioCapture(false);
      mAudioCaptureTrack->Graph()->UnregisterCaptureTrackForWindow(
          mWindow->WindowID());
      mWindow = nullptr;
    }
    // LocalTrackSource destroys the track.
    LocalTrackSource::Stop();
    MOZ_ASSERT(mAudioCaptureTrack->IsDestroyed());
  }

  ProcessedMediaTrack* InputTrack() const { return mAudioCaptureTrack.get(); }

 protected:
  ~AudioCaptureTrackSource() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mAudioCaptureTrack->IsDestroyed());
  }

  RefPtr<nsPIDOMWindowInner> mWindow;
  const RefPtr<AudioCaptureTrack> mAudioCaptureTrack;
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
      mType(
          NS_ConvertASCIItoUTF16(dom::MediaDeviceKindValues::GetString(mKind))),
      mName(aName),
      mID(aID),
      mGroupID(aGroupID),
      mRawID(aRawID),
      mRawName(aName) {
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
      mType(
          NS_ConvertASCIItoUTF16(dom::MediaDeviceKindValues::GetString(mKind))),
      mName(mSinkInfo->Name()),
      mID(aID),
      mGroupID(aGroupID),
      mRawID(aRawID),
      mRawName(mSinkInfo->Name()) {
  // For now this ctor is used only for Audiooutput.
  // It could be used for Audioinput and Videoinput
  // when we do not instantiate a MediaEngineSource
  // during EnumerateDevices.
  MOZ_ASSERT(mKind == MediaDeviceKind::Audiooutput);
  MOZ_ASSERT(mSinkInfo);
}

MediaDevice::MediaDevice(const RefPtr<MediaDevice>& aOther, const nsString& aID,
                         const nsString& aGroupID, const nsString& aRawID,
                         const nsString& aRawGroupID)
    : MediaDevice(aOther, aID, aGroupID, aRawID, aRawGroupID, aOther->mName) {}

MediaDevice::MediaDevice(const RefPtr<MediaDevice>& aOther, const nsString& aID,
                         const nsString& aGroupID, const nsString& aRawID,
                         const nsString& aRawGroupID, const nsString& aName)
    : mSource(aOther->mSource),
      mSinkInfo(aOther->mSinkInfo),
      mKind(aOther->mKind),
      mScary(aOther->mScary),
      mIsFake(aOther->mIsFake),
      mType(aOther->mType),
      mName(aName),
      mID(aID),
      mGroupID(aGroupID),
      mRawID(aRawID),
      mRawGroupID(aRawGroupID),
      mRawName(aOther->mRawName) {
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
MediaDevice::GetRawName(nsAString& aName) {
  MOZ_ASSERT(NS_IsMainThread());
  aName.Assign(mRawName);
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
MediaDevice::GetRawGroupId(nsAString& aRawGroupID) {
  MOZ_ASSERT(NS_IsMainThread());
  aRawGroupID.Assign(mRawGroupID);
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
  aMediaSource.AssignASCII(
      dom::MediaSourceEnumValues::GetString(GetMediaSource()));
  return NS_OK;
}

nsresult MediaDevice::Allocate(const MediaTrackConstraints& aConstraints,
                               const MediaEnginePrefs& aPrefs,
                               uint64_t aWindowID,
                               const char** aOutBadConstraint) {
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);

  // Mock failure for automated tests.
  if (mIsFake && aConstraints.mDeviceId.WasPassed() &&
      aConstraints.mDeviceId.Value().IsString() &&
      aConstraints.mDeviceId.Value().GetAsString().EqualsASCII("bad device")) {
    return NS_ERROR_FAILURE;
  }

  return mSource->Allocate(aConstraints, aPrefs, aWindowID, aOutBadConstraint);
}

void MediaDevice::SetTrack(const RefPtr<MediaTrack>& aTrack,
                           const PrincipalHandle& aPrincipalHandle) {
  MOZ_ASSERT(MediaManager::IsInMediaThread());
  MOZ_ASSERT(mSource);
  mSource->SetTrack(aTrack, aPrincipalHandle);
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

static const MediaTrackConstraints& GetInvariant(
    const OwningBooleanOrMediaTrackConstraints& aUnion) {
  static const MediaTrackConstraints empty;
  return aUnion.IsMediaTrackConstraints() ? aUnion.GetAsMediaTrackConstraints()
                                          : empty;
}

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
    aResult = std::move(devices);
    if (MOZ_LOG_TEST(gMediaManagerLog, mozilla::LogLevel::Debug)) {
      for (auto& device : aResult) {
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

  return MediaManager::Dispatch<BadConstraintsPromise>(
      __func__, [aConstraints, aSources,
                 aIsChrome](MozPromiseHolder<BadConstraintsPromise>& holder) {
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
 * Describes a requested task that handles response from the UI and sends
 * results back to the DOM.
 */
class GetUserMediaTask final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GetUserMediaTask)

  GetUserMediaTask(const MediaStreamConstraints& aConstraints,
                   MozPromiseHolder<MediaManager::StreamPromise>&& aHolder,
                   uint64_t aWindowID,
                   RefPtr<GetUserMediaWindowListener> aWindowListener,
                   RefPtr<SourceListener> aSourceListener,
                   const MediaEnginePrefs& aPrefs,
                   const ipc::PrincipalInfo& aPrincipalInfo, bool aIsChrome,
                   RefPtr<MediaManager::MediaDeviceSetRefCnt>&& aMediaDeviceSet,
                   bool aShouldFocusSource)
      : mConstraints(aConstraints),
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

  void Allowed() {
    // Reuse the same thread to save memory.
    MediaManager::Dispatch(
        NewRunnableMethod("GetUserMediaTask::AllocateDevices", this,
                          &GetUserMediaTask::AllocateDevices));
  }

 private:
  ~GetUserMediaTask() {
    if (!mHolder.IsEmpty()) {
      Fail(MediaMgrError::Name::NotAllowedError);
    }
  }

  void Fail(MediaMgrError::Name aName, const nsCString& aMessage = ""_ns,
            const nsString& aConstraint = u""_ns) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "GetUserMediaTask::Fail",
        [aName, aMessage, aConstraint, holder = std::move(mHolder)]() mutable {
          holder.Reject(MakeRefPtr<MediaMgrError>(aName, aMessage, aConstraint),
                        __func__);
        }));
    // Do after the above runs, as it checks active window list
    NS_DispatchToMainThread(NewRunnableMethod(
        "SourceListener::Stop", mSourceListener, &SourceListener::Stop));
  }

  /**
   * Runs on a separate thread and is responsible for allocating devices.
   *
   * Do not run this on the main thread.
   */
  void AllocateDevices() {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mDeviceChosen);
    LOG("GetUserMediaTask::Run()");

    // Allocate a video or audio device and return a MediaStream via
    // PrepareDOMStream().

    nsresult rv;
    const char* errorMsg = nullptr;
    const char* badConstraint = nullptr;

    if (mAudioDevice) {
      auto& constraints = GetInvariant(mConstraints.mAudio);
      rv = mAudioDevice->Allocate(constraints, mPrefs, mWindowID,
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
      rv = mVideoDevice->Allocate(constraints, mPrefs, mWindowID,
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
        Fail(MediaMgrError::Name::OverconstrainedError, ""_ns,
             NS_ConvertUTF8toUTF16(badConstraint));
      } else {
        Fail(MediaMgrError::Name::NotReadableError, nsCString(errorMsg));
      }
      NS_DispatchToMainThread(
          NS_NewRunnableFunction("MediaManager::SendPendingGUMRequest", []() {
            MediaManager* manager = MediaManager::GetIfExists();
            if (!manager) {
              return;
            }
            manager->SendPendingGUMRequest();
          }));
      return;
    }
    NS_DispatchToMainThread(
        NewRunnableMethod("GetUserMediaTask::PrepareDOMStream", this,
                          &GetUserMediaTask::PrepareDOMStream));
  }

 public:
  nsresult Denied(MediaMgrError::Name aName,
                  const nsCString& aMessage = ""_ns) {
    // We add a disabled listener to the StreamListeners array until accepted
    // If this was the only active MediaStream, remove the window from the list.
    if (NS_IsMainThread()) {
      mHolder.Reject(MakeRefPtr<MediaMgrError>(aName, aMessage), __func__);
      // Should happen *after* error runs for consistency, but may not matter
      mSourceListener->Stop();
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

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    size_t amount = aMallocSizeOf(this);
    // Assume mWindowListener is owned by MediaManager.
    // Assume mSourceListener is owned by mWindowListener.
    // Assume PrincipalInfo string buffers are shared.
    // Member types without support for accounting of pointees:
    //   MozPromiseHolder, RefPtr<MediaDevice>.
    // We don't have a good way to account for lambda captures for MozPromise
    // callbacks.
    return amount;
  }

 private:
  void PrepareDOMStream();

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

/**
 * Creates a MediaTrack, attaches a listener and resolves a MozPromise to
 * provide the stream to the DOM.
 *
 * All of this must be done on the main thread!
 */
void GetUserMediaTask::PrepareDOMStream() {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("GetUserMediaTask::PrepareDOMStream()");
  nsGlobalWindowInner* window =
      nsGlobalWindowInner::GetInnerWindowWithId(mWindowID);

  // We're on main-thread, and the windowlist can only
  // be invalidated from the main-thread (see OnNavigation)
  if (!mManager->IsWindowListenerStillActive(mWindowListener)) {
    // This window is no longer live. mListener has already been removed.
    return;
  }

  MediaTrackGraph::GraphDriverType graphDriverType =
      mAudioDevice ? MediaTrackGraph::AUDIO_THREAD_DRIVER
                   : MediaTrackGraph::SYSTEM_THREAD_DRIVER;
  MediaTrackGraph* mtg = MediaTrackGraph::GetInstance(
      graphDriverType, window, MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      MediaTrackGraph::DEFAULT_OUTPUT_DEVICE);

  auto domStream = MakeRefPtr<DOMMediaStream>(window);
  RefPtr<LocalTrackSource> audioTrackSource;
  RefPtr<LocalTrackSource> videoTrackSource;
  nsCOMPtr<nsIPrincipal> principal;
  RefPtr<PeerIdentity> peerIdentity = nullptr;
  if (!mConstraints.mPeerIdentity.IsEmpty()) {
    peerIdentity = new PeerIdentity(mConstraints.mPeerIdentity);
    principal = NullPrincipal::CreateWithInheritedAttributes(
        window->GetExtantDoc()->NodePrincipal());
  } else {
    principal = window->GetExtantDoc()->NodePrincipal();
  }
  RefPtr<GenericNonExclusivePromise> firstFramePromise;
  if (mAudioDevice) {
    if (mAudioDevice->GetMediaSource() == MediaSourceEnum::AudioCapture) {
      // AudioCapture is a special case, here, in the sense that we're not
      // really using the audio source and the SourceMediaTrack, which acts
      // as placeholders. We re-route a number of tracks internally in the
      // MTG and mix them down instead.
      NS_WARNING(
          "MediaCaptureWindowState doesn't handle "
          "MediaSourceEnum::AudioCapture. This must be fixed with UX "
          "before shipping.");
      auto audioCaptureSource = MakeRefPtr<AudioCaptureTrackSource>(
          principal, window, u"Window audio capture"_ns,
          mtg->CreateAudioCaptureTrack(), peerIdentity);
      audioTrackSource = audioCaptureSource;
      RefPtr<MediaStreamTrack> track = new dom::AudioStreamTrack(
          window, audioCaptureSource->InputTrack(), audioCaptureSource);
      domStream->AddTrackInternal(track);
    } else {
      nsString audioDeviceName;
      mAudioDevice->GetName(audioDeviceName);
      RefPtr<MediaTrack> track;
#ifdef MOZ_WEBRTC
      if (mAudioDevice->mIsFake) {
        track = mtg->CreateSourceTrack(MediaSegment::AUDIO);
      } else {
        track = AudioInputTrack::Create(mtg);
        track->Suspend();  // Microphone source resumes in SetTrack
      }
#else
      track = mtg->CreateSourceTrack(MediaSegment::AUDIO);
#endif
      audioTrackSource = new LocalTrackSource(
          principal, audioDeviceName, mSourceListener,
          mAudioDevice->GetMediaSource(), track, peerIdentity);
      MOZ_ASSERT(MediaManager::IsOn(mConstraints.mAudio));
      RefPtr<MediaStreamTrack> domTrack = new dom::AudioStreamTrack(
          window, track, audioTrackSource, dom::MediaStreamTrackState::Live,
          false, GetInvariant(mConstraints.mAudio));
      domStream->AddTrackInternal(domTrack);
    }
  }
  if (mVideoDevice) {
    nsString videoDeviceName;
    mVideoDevice->GetName(videoDeviceName);
    RefPtr<MediaTrack> track = mtg->CreateSourceTrack(MediaSegment::VIDEO);
    videoTrackSource = new LocalTrackSource(
        principal, videoDeviceName, mSourceListener,
        mVideoDevice->GetMediaSource(), track, peerIdentity);
    MOZ_ASSERT(MediaManager::IsOn(mConstraints.mVideo));
    RefPtr<MediaStreamTrack> domTrack = new dom::VideoStreamTrack(
        window, track, videoTrackSource, dom::MediaStreamTrackState::Live,
        false, GetInvariant(mConstraints.mVideo));
    domStream->AddTrackInternal(domTrack);
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

  if (!domStream || (!audioTrackSource && !videoTrackSource) || sHasShutdown) {
    LOG("Returning error for getUserMedia() - no stream");

    mHolder.Reject(MakeRefPtr<MediaMgrError>(
                       MediaMgrError::Name::AbortError,
                       sHasShutdown ? "In shutdown"_ns : "No stream."_ns),
                   __func__);
    return;
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
            LOG("GetUserMediaTask::PrepareDOMStream: starting success callback "
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
                          CreateAndReject(MakeRefPtr<MediaMgrError>(
                                              MediaMgrError::Name::AbortError,
                                              "In shutdown"),
                                          __func__);
                    });
            return resolvePromise;
          },
          [](RefPtr<MediaMgrError>&& aError) {
            LOG("GetUserMediaTask::PrepareDOMStream: starting failure callback "
                "following InitializeAsync()");
            return SourceListener::SourceListenerPromise::CreateAndReject(
                aError, __func__);
          })
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [holder = std::move(mHolder), domStream](
              const SourceListener::SourceListenerPromise::ResolveOrRejectValue&
                  aValue) mutable {
            if (aValue.IsResolve()) {
              holder.Resolve(domStream, __func__);
            } else {
              holder.Reject(aValue.RejectValue(), __func__);
            }
          });

  if (!IsPrincipalInfoPrivate(mPrincipalInfo)) {
    // Call GetPrincipalKey again, this time w/persist = true, to promote
    // deviceIds to persistent, in case they're not already. Fire'n'forget.
    media::GetPrincipalKey(mPrincipalInfo, true)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [](const media::PrincipalKeyPromise::ResolveOrRejectValue& aValue) {
              if (aValue.IsReject()) {
                LOG("Failed get Principal key. Persisting of deviceIds "
                    "will be broken");
              }
            });
  }
}

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
        newVideoGroupID = u""_ns;
        break;
      }
    }
    if (updateGroupId) {
      aVideo = new MediaDevice(aVideo, aVideo->mID, newVideoGroupID,
                               aVideo->mRawID, aVideo->mRawGroupID);
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
        (aVideoInputType != MediaSourceEnum::Camera)       ? u"audio"
        : (aAudioInputType != MediaSourceEnum::Microphone) ? u"video"
                                                           : u"all";
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    obs->NotifyObservers(static_cast<nsIRunnable*>(task),
                         "getUserMedia:ask-device-permission", type);
  } else {
    // Don't need to ask permission to retrieve list of all devices;
    // post the retrieval task immediately.
    MediaManager::Dispatch(task.forget());
  }

  return promise;
}

MediaManager::MediaManager(already_AddRefed<TaskQueue> aMediaThread)
    : mMediaThread(aMediaThread), mBackend(nullptr) {
  mPrefs.mFreq = 1000;  // 1KHz test tone
  mPrefs.mWidth = 0;    // adaptive default
  mPrefs.mHeight = 0;   // adaptive default
  mPrefs.mFPS = MediaEnginePrefs::DEFAULT_VIDEO_FPS;
  mPrefs.mAecOn = false;
  mPrefs.mUseAecMobile = false;
  mPrefs.mAgcOn = false;
  mPrefs.mHPFOn = false;
  mPrefs.mNoiseOn = false;
  mPrefs.mExtendedFilter = true;
  mPrefs.mDelayAgnostic = true;
  mPrefs.mFakeDeviceChangeEventOn = false;
#ifdef MOZ_WEBRTC
  mPrefs.mAec =
      webrtc::EchoCancellation::SuppressionLevel::kModerateSuppression;
  mPrefs.mAgc = webrtc::GainControl::Mode::kAdaptiveDigital;
  mPrefs.mNoise = webrtc::NoiseSuppression::Level::kModerate;
  mPrefs.mRoutingMode = webrtc::EchoControlMobile::RoutingMode::kSpeakerphone;
#else
  mPrefs.mAec = 0;
  mPrefs.mAgc = 0;
  mPrefs.mNoise = 0;
  mPrefs.mRoutingMode = 0;
#endif
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
      "agc: %s, hpf: %s, noise: %s, aec level: %d, agc level: %d, noise level: "
      "%d, aec mobile routing mode: %d,"
      "extended aec %s, delay_agnostic %s "
      "channels %d",
      __FUNCTION__, mPrefs.mWidth, mPrefs.mHeight, mPrefs.mFPS, mPrefs.mFreq,
      mPrefs.mAecOn ? "on" : "off", mPrefs.mAgcOn ? "on" : "off",
      mPrefs.mHPFOn ? "on" : "off", mPrefs.mNoiseOn ? "on" : "off", mPrefs.mAec,
      mPrefs.mAgc, mPrefs.mNoise, mPrefs.mRoutingMode,
      mPrefs.mExtendedFilter ? "on" : "off",
      mPrefs.mDelayAgnostic ? "on" : "off", mPrefs.mChannels);
}

NS_IMPL_ISUPPORTS(MediaManager, nsIMediaManagerService, nsIMemoryReporter,
                  nsIObserver)

/* static */
StaticRefPtr<MediaManager> MediaManager::sSingleton;
/* static */
StaticMutex MediaManager::sSingletonMutex;

#ifdef DEBUG
/* static */
bool MediaManager::IsInMediaThread() {
  StaticMutexAutoLock lock(sSingletonMutex);
  return sSingleton && sSingleton->mMediaThread->IsOnCurrentThread();
}
#endif

// NOTE: never Dispatch(....,NS_DISPATCH_SYNC) to the MediaManager
// thread from the MainThread, as we NS_DISPATCH_SYNC to MainThread
// from MediaManager thread.

// Guaranteed never to return nullptr.
/* static */
MediaManager* MediaManager::Get() {
  StaticMutexAutoLock lock(sSingletonMutex);
  if (!sSingleton) {
    MOZ_ASSERT(NS_IsMainThread());

    static int timesCreated = 0;
    timesCreated++;
    MOZ_RELEASE_ASSERT(timesCreated == 1);

    RefPtr<TaskQueue> mediaThread = new TaskQueue(
        GetMediaThreadPool(MediaThreadType::SUPERVISOR), "MediaManager");
    LOG("New Media thread for gum");

    sSingleton = new MediaManager(mediaThread.forget());

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
      obs->AddObserver(sSingleton, "getUserMedia:muteVideo", false);
      obs->AddObserver(sSingleton, "getUserMedia:unmuteVideo", false);
      obs->AddObserver(sSingleton, "getUserMedia:muteAudio", false);
      obs->AddObserver(sSingleton, "getUserMedia:unmuteAudio", false);
      obs->AddObserver(sSingleton, "application-background", false);
      obs->AddObserver(sSingleton, "application-foreground", false);
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
#ifdef MOZ_WEBRTC
      prefs->AddObserver("media.getusermedia.aec_enabled", sSingleton, false);
      prefs->AddObserver("media.getusermedia.aec", sSingleton, false);
      prefs->AddObserver("media.getusermedia.agc_enabled", sSingleton, false);
      prefs->AddObserver("media.getusermedia.agc", sSingleton, false);
      prefs->AddObserver("media.getusermedia.hpf_enabled", sSingleton, false);
      prefs->AddObserver("media.getusermedia.noise_enabled", sSingleton, false);
      prefs->AddObserver("media.getusermedia.noise", sSingleton, false);
      prefs->AddObserver("media.ondevicechange.fakeDeviceChangeEvent.enabled",
                         sSingleton, false);
      prefs->AddObserver("media.getusermedia.channels", sSingleton, false);
#endif
    }
    RegisterStrongMemoryReporter(sSingleton);

    // Prepare async shutdown

    class Blocker : public media::ShutdownBlocker {
     public:
      Blocker()
          : media::ShutdownBlocker(
                u"Media shutdown: blocking on media thread"_ns) {}

      NS_IMETHOD BlockShutdown(nsIAsyncShutdownClient*) override {
        MOZ_RELEASE_ASSERT(MediaManager::GetIfExists());
        MediaManager::GetIfExists()->Shutdown();
        return NS_OK;
      }
    };

    sSingleton->mShutdownBlocker = new Blocker();
    nsresult rv = media::GetShutdownBarrier()->AddBlocker(
        sSingleton->mShutdownBlocker, NS_LITERAL_STRING_FROM_CSTRING(__FILE__),
        __LINE__, u""_ns);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  }
  return sSingleton;
}

/* static */
MediaManager* MediaManager::GetIfExists() {
  StaticMutexAutoLock lock(sSingletonMutex);
  return sSingleton;
}

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
void MediaManager::Dispatch(already_AddRefed<Runnable> task) {
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
  MOZ_ALWAYS_SUCCEEDS(Get()->mMediaThread->Dispatch(std::move(task)));
}

template <typename MozPromiseType, typename FunctionType>
/* static */
RefPtr<MozPromiseType> MediaManager::Dispatch(const char* aName,
                                              FunctionType&& aFunction) {
  MozPromiseHolder<MozPromiseType> holder;
  RefPtr<MozPromiseType> promise = holder.Ensure(aName);
  MediaManager::Dispatch(NS_NewRunnableFunction(
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

  props->SetPropertyAsAString(u"requestURL"_ns, requestURL);
  props->SetPropertyAsInterface(u"window"_ns, aWindow);

  obs->NotifyObservers(static_cast<nsIPropertyBag2*>(props),
                       "recording-device-events", nullptr);
  LOG("Sent recording-device-events for url '%s'", pageURL.get());

  return NS_OK;
}

void MediaManager::DeviceListChanged() {
  MOZ_ASSERT(NS_IsMainThread());
  if (sHasShutdown) {
    return;
  }
  mDeviceListChangeEvent.Notify();

  // Wait 200 ms, because
  // A) on some Windows machines, if we call EnumerateRawDevices immediately
  //    after receiving devicechange event, we'd get an outdated devices list.
  // B) Waiting helps coalesce multiple calls on us into one, which can happen
  //    if a device with both audio input and output is attached or removed.
  //    We want to react & fire a devicechange event only once in that case.

  if (mDeviceChangeTimer) {
    mDeviceChangeTimer->Cancel();
  } else {
    mDeviceChangeTimer = MakeRefPtr<MediaTimer>();
  }
  RefPtr<MediaManager> self = this;
  auto devices = MakeRefPtr<MediaDeviceSetRefCnt>();
  mDeviceChangeTimer->WaitFor(TimeDuration::FromMilliseconds(200), __func__)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self, this, devices] {
            if (!MediaManager::GetIfExists()) {
              return MgrPromise::CreateAndReject(
                  MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError,
                                            "In shutdown"),
                  __func__);
            }
            return EnumerateRawDevices(
                0, MediaSourceEnum::Camera, MediaSourceEnum::Microphone,
                MediaSinkEnum::Speaker, DeviceEnumerationType::Normal,
                DeviceEnumerationType::Normal, false, devices);
          },
          []() {
            // Timer was canceled by us, or we're in shutdown.
            return MgrPromise::CreateAndReject(
                MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError),
                __func__);
          })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self, this, devices](bool) {
            if (!MediaManager::GetIfExists()) {
              return;
            }

            MediaManager::DeviceIdSet deviceIDs;
            for (auto& device : *devices) {
              nsString id;
              device->GetId(id);
              MOZ_ALWAYS_TRUE(deviceIDs.put(std::move(id)));
            }
            // For any real removed cameras, microphones or speakers, notify
            // their listeners cleanly that the source has stopped, so JS knows
            // and usage indicators update.
            for (auto iter = mDeviceIDs.iter(); !iter.done(); iter.next()) {
              const auto& id = iter.get();
              if (deviceIDs.has(id)) {
                // Device has not been removed
                continue;
              }
              // Stop the corresponding SourceListener. In order to do that
              // first collect the listeners in an array and stop them after
              // the loop. The StopRawID method modify indirectly the
              // mActiveWindows and will assert-crash since the iterator is
              // active and the table is being enumerated.
              nsTArray<RefPtr<GetUserMediaWindowListener>> stopListeners;
              for (auto iter = mActiveWindows.Iter(); !iter.Done();
                   iter.Next()) {
                stopListeners.AppendElement(iter.UserData());
              }
              for (auto& l : stopListeners) {
                l->StopRawID(id);
              }
            }
            mDeviceIDs = std::move(deviceIDs);
          },
          [](RefPtr<MediaMgrError>&& reason) {});
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
  if (!MediaManager::IsOn(aConstraint)) {
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

  if (sHasShutdown) {
    return StreamPromise::CreateAndReject(
        MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError,
                                  "In shutdown"),
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
      vc.mMediaSource.Construct().AssignASCII(
          dom::MediaSourceEnumValues::GetString(MediaSourceEnum::Camera));
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
        [[fallthrough]];
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
                                      "", u"mediaSource"_ns),
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
            dom::MediaSourceEnumValues::GetString(videoType));
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
      ac.mMediaSource.Construct(NS_ConvertASCIItoUTF16(
          dom::MediaSourceEnumValues::GetString(MediaSourceEnum::Microphone)));
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
                                      "", u"mediaSource"_ns),
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
      GetOrMakeWindowListener(aWindow);
  MOZ_ASSERT(windowListener);
  auto sourceListener = MakeRefPtr<SourceListener>();
  windowListener->Register(sourceListener);

  if (!privileged) {
    // Check if this site has had persistent permissions denied.
    RefPtr<PermissionDelegateHandler> permDelegate =
        doc->GetPermissionDelegateHandler();
    MOZ_RELEASE_ASSERT(permDelegate);

    uint32_t audioPerm = nsIPermissionManager::UNKNOWN_ACTION;
    if (IsOn(c.mAudio)) {
      if (audioType == MediaSourceEnum::Microphone) {
        if (Preferences::GetBool("media.getusermedia.microphone.deny", false)) {
          audioPerm = nsIPermissionManager::DENY_ACTION;
        } else {
          rv = permDelegate->GetPermission("microphone"_ns, &audioPerm, true);
          MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
        }
      } else {
        rv = permDelegate->GetPermission("screen"_ns, &audioPerm, true);
        MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
      }
    }

    uint32_t videoPerm = nsIPermissionManager::UNKNOWN_ACTION;
    if (IsOn(c.mVideo)) {
      if (videoType == MediaSourceEnum::Camera) {
        if (Preferences::GetBool("media.getusermedia.camera.deny", false)) {
          videoPerm = nsIPermissionManager::DENY_ACTION;
        } else {
          rv = permDelegate->GetPermission("camera"_ns, &videoPerm, true);
          MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
        }
      } else {
        rv = permDelegate->GetPermission("screen"_ns, &videoPerm, true);
        MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
      }
    }

    if ((!IsOn(c.mAudio) && !IsOn(c.mVideo)) ||
        (IsOn(c.mAudio) && audioPerm == nsIPermissionManager::DENY_ACTION) ||
        (IsOn(c.mVideo) && videoPerm == nsIPermissionManager::DENY_ACTION)) {
      sourceListener->Stop();
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
  return EnumerateDevicesImpl(aWindow, videoType, audioType,
                              MediaSinkEnum::Other, videoEnumerationType,
                              audioEnumerationType, true, devices)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
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
          GetCurrentSerialEventTarget(), __func__,
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
              sourceListener->Stop();
              return StreamPromise::CreateAndReject(
                  MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError),
                  __func__);
            }

            if (badConstraint) {
              LOG("GetUserMedia: bad constraint found in post enumeration "
                  "promise2 success callback! Calling error handler!");
              nsString constraint;
              constraint.AssignASCII(badConstraint);
              sourceListener->Stop();
              return StreamPromise::CreateAndReject(
                  MakeRefPtr<MediaMgrError>(
                      MediaMgrError::Name::OverconstrainedError, "",
                      constraint),
                  __func__);
            }
            if (!devices->Length()) {
              LOG("GetUserMedia: no devices found in post enumeration promise2 "
                  "success callback! Calling error handler!");
              sourceListener->Stop();
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
                  sourceListener->Stop();
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
            self->mActiveCallbacks.InsertOrUpdate(callID, std::move(task));

            // Add a WindowID cross-reference so OnNavigation can tear
            // things down
            nsTArray<nsString>* const array =
                self->mCallIds.GetOrInsertNew(windowID);
            array->AppendElement(callID);

            nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
            if (!askPermission) {
              obs->NotifyObservers(devicesCopy, "getUserMedia:privileged:allow",
                                   callID.get());
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
          [sourceListener](RefPtr<MediaMgrError>&& aError) {
            LOG("GetUserMedia: post enumeration SelectSettings failure "
                "callback called!");
            sourceListener->Stop();
            return StreamPromise::CreateAndReject(std::move(aError), __func__);
          });
};

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
      nsString rawGroupId = groupId;
      // Use window id to salt group id in order to make it session based as
      // required by the spec. This does not provide unique group ids through
      // out a browser restart. However, this is not agaist the spec.
      // Furtermore, since device ids are the same after a browser restart the
      // fingerprint is not bigger.
      groupId.AppendInt(aWindowId);
      AnonymizeId(groupId, aOriginKey);

      nsString name;
      device->GetName(name);
      if (name.Find(u"AirPods"_ns) != -1) {
        name = u"AirPods"_ns;
      }
      device = new MediaDevice(device, id, groupId, rawId, rawGroupId, name);
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

  CopyUTF8toUTF16(mac, aId);
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
    nsPIDOMWindowInner* aWindow, MediaSourceEnum aVideoInputType,
    MediaSourceEnum aAudioInputType, MediaSinkEnum aAudioOutputType,
    DeviceEnumerationType aVideoInputEnumType,
    DeviceEnumerationType aAudioInputEnumType, bool aForceNoPermRequest,
    const RefPtr<MediaDeviceSetRefCnt>& aOutDevices) {
  MOZ_ASSERT(NS_IsMainThread());

  uint64_t windowId = aWindow->WindowID();
  LOG("%s: windowId=%" PRIu64 ", aVideoInputType=%" PRIu8
      ", aAudioInputType=%" PRIu8 ", aVideoInputEnumType=%" PRIu8
      ", aAudioInputEnumType=%" PRIu8,
      __func__, windowId, static_cast<uint8_t>(aVideoInputType),
      static_cast<uint8_t>(aAudioInputType),
      static_cast<uint8_t>(aVideoInputEnumType),
      static_cast<uint8_t>(aAudioInputEnumType));

  // To get a device list anonymized for a particular origin, we must:
  // 1. Get an origin-key (for either regular or private browsing)
  // 2. Get the raw devices list
  // 3. Anonymize the raw list with the origin-key.

  nsCOMPtr<nsIPrincipal> principal =
      nsGlobalWindowInner::Cast(aWindow)->GetPrincipal();
  MOZ_ASSERT(principal);

  ipc::PrincipalInfo principalInfo;
  nsresult rv = PrincipalToPrincipalInfo(principal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return MgrPromise::CreateAndReject(
        MakeRefPtr<MediaMgrError>(MediaMgrError::Name::NotAllowedError),
        __func__);
  }

  // Add the window id here to check for that and abort silently if no longer
  // exists.
  RefPtr<GetUserMediaWindowListener> windowListener =
      GetOrMakeWindowListener(aWindow);
  MOZ_ASSERT(windowListener);
  // Create an inactive SourceListener to act as a placeholder, so the
  // window listener doesn't clean itself up until we're done.
  auto sourceListener = MakeRefPtr<SourceListener>();
  windowListener->Register(sourceListener);

  bool persist = IsActivelyCapturingOrHasAPermission(windowId);

  // GetPrincipalKey is an async API that returns a promise. We use .Then() to
  // pass in a lambda to run back on this same thread later once
  // GetPrincipalKey resolves. Needed variables are "captured"
  // (passed by value) safely into the lambda.
  auto originKey = MakeRefPtr<Refcountable<nsCString>>();
  return media::GetPrincipalKey(principalInfo, persist)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [windowId, aVideoInputType, aAudioInputType, aAudioOutputType,
           aVideoInputEnumType, aAudioInputEnumType, aForceNoPermRequest,
           aOutDevices, originKey](const nsCString& aOriginKey) {
            MOZ_ASSERT(NS_IsMainThread());
            originKey->Assign(aOriginKey);
            MediaManager* mgr = MediaManager::GetIfExists();
            MOZ_ASSERT(mgr);
            if (!mgr->IsWindowStillActive(windowId)) {
              return MgrPromise::CreateAndReject(
                  MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError),
                  __func__);
            }
            return mgr->EnumerateRawDevices(
                windowId, aVideoInputType, aAudioInputType, aAudioOutputType,
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
          [windowId, sourceListener, originKey, aVideoInputEnumType,
           aAudioInputEnumType, aOutDevices](bool) {
            // Only run if window is still on our active list.
            MediaManager* mgr = MediaManager::GetIfExists();
            if (!mgr || !mgr->IsWindowStillActive(windowId)) {
              // The listener has already been removed if the window is no
              // longer active.
              MOZ_ASSERT(sourceListener->Stopped());
              return MgrPromise::CreateAndReject(
                  MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError),
                  __func__);
            }
            MOZ_ASSERT(!sourceListener->Stopped());
            sourceListener->Stop();

            for (auto& device : *aOutDevices) {
              if (device->mKind == MediaDeviceKind::Audiooutput ||
                  (device->mKind == MediaDeviceKind::Audioinput &&
                   aAudioInputEnumType != DeviceEnumerationType::Fake &&
                   device->GetMediaSource() == MediaSourceEnum::Microphone) ||
                  (device->mKind == MediaDeviceKind::Videoinput &&
                   aVideoInputEnumType != DeviceEnumerationType::Fake &&
                   device->GetMediaSource() == MediaSourceEnum::Camera)) {
                nsString id;
                device->GetId(id);
                MOZ_ALWAYS_TRUE(mgr->mDeviceIDs.put(std::move(id)));
              }
            }
            MediaManager::AnonymizeDevices(*aOutDevices, *originKey, windowId);
            return MgrPromise::CreateAndResolve(false, __func__);
          },
          [sourceListener](RefPtr<MediaMgrError>&& aError) {
            // EnumerateDevicesImpl may fail if a new doc has been set, in which
            // case the OnNavigation() method should have removed all previous
            // active listeners.
            MOZ_ASSERT(sourceListener->Stopped());
            return MgrPromise::CreateAndReject(std::move(aError), __func__);
          });
}

RefPtr<MediaManager::DevicesPromise> MediaManager::EnumerateDevices(
    nsPIDOMWindowInner* aWindow, CallerType aCallerType) {
  MOZ_ASSERT(NS_IsMainThread());
  if (sHasShutdown) {
    return DevicesPromise::CreateAndReject(
        MakeRefPtr<MediaMgrError>(MediaMgrError::Name::AbortError,
                                  "In shutdown"),
        __func__);
  }
  Document* doc = aWindow->GetExtantDoc();
  MOZ_ASSERT(doc);

  DeviceEnumerationType videoEnumerationType = DeviceEnumerationType::Normal;
  DeviceEnumerationType audioEnumerationType = DeviceEnumerationType::Normal;

  // Only expose devices which are allowed to use:
  // https://w3c.github.io/mediacapture-main/#dom-mediadevices-enumeratedevices
  MediaSourceEnum videoType =
      dom::FeaturePolicyUtils::IsFeatureAllowed(doc, u"camera"_ns)
          ? MediaSourceEnum::Camera
          : MediaSourceEnum::Other;
  MediaSourceEnum audioType =
      dom::FeaturePolicyUtils::IsFeatureAllowed(doc, u"microphone"_ns)
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

  return EnumerateDevicesImpl(aWindow, videoType, audioType, audioOutputType,
                              videoEnumerationType, audioEnumerationType, false,
                              devices)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [devices](bool) {
            return DevicesPromise::CreateAndResolve(devices, __func__);
          },
          [](RefPtr<MediaMgrError>&& aError) {
            return DevicesPromise::CreateAndReject(std::move(aError), __func__);
          });
}

RefPtr<AudioDeviceInfo> CopyWithNullDeviceId(AudioDeviceInfo* aDeviceInfo) {
  MOZ_ASSERT(aDeviceInfo->Preferred());

  nsString vendor;
  aDeviceInfo->GetVendor(vendor);
  uint16_t type;
  aDeviceInfo->GetType(&type);
  uint16_t state;
  aDeviceInfo->GetState(&state);
  uint16_t pref;
  aDeviceInfo->GetPreferred(&pref);
  uint16_t supportedFormat;
  aDeviceInfo->GetSupportedFormat(&supportedFormat);
  uint16_t defaultFormat;
  aDeviceInfo->GetDefaultFormat(&defaultFormat);
  uint32_t maxChannels;
  aDeviceInfo->GetMaxChannels(&maxChannels);
  uint32_t defaultRate;
  aDeviceInfo->GetDefaultRate(&defaultRate);
  uint32_t maxRate;
  aDeviceInfo->GetMaxRate(&maxRate);
  uint32_t minRate;
  aDeviceInfo->GetMinRate(&minRate);
  uint32_t maxLatency;
  aDeviceInfo->GetMaxLatency(&maxLatency);
  uint32_t minLatency;
  aDeviceInfo->GetMinLatency(&minLatency);

  return MakeRefPtr<AudioDeviceInfo>(
      nullptr, aDeviceInfo->Name(), aDeviceInfo->GroupID(), vendor, type, state,
      pref, supportedFormat, defaultFormat, maxChannels, defaultRate, maxRate,
      minRate, maxLatency, minLatency);
}

RefPtr<SinkInfoPromise> MediaManager::GetSinkDevice(nsPIDOMWindowInner* aWindow,
                                                    const nsString& aDeviceId) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  bool isSecure = aWindow->IsSecureContext();
  auto devices = MakeRefPtr<MediaDeviceSetRefCnt>();
  return EnumerateDevicesImpl(aWindow, MediaSourceEnum::Other,
                              MediaSourceEnum::Other, MediaSinkEnum::Speaker,
                              DeviceEnumerationType::Normal,
                              DeviceEnumerationType::Normal, true, devices)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [aDeviceId, isSecure, devices](bool) {
            for (RefPtr<MediaDevice>& device : *devices) {
              if (aDeviceId.IsEmpty() && device->mSinkInfo->Preferred()) {
                return SinkInfoPromise::CreateAndResolve(
                    CopyWithNullDeviceId(device->mSinkInfo), __func__);
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
    mDeviceListChangeListener = mBackend->DeviceListChangeEvent().Connect(
        AbstractThread::MainThread(), this, &MediaManager::DeviceListChanged);
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
      for (auto& request : mPendingGUMRequest.Clone()) {
        nsString id;
        request->GetCallID(id);
        if (id == callID) {
          mPendingGUMRequest.RemoveElement(request);
        }
      }
    }
    mCallIds.Remove(aWindowID);
  }

  if (RefPtr<GetUserMediaWindowListener> listener =
          GetWindowListener(aWindowID)) {
    listener->RemoveAll();
  }
  MOZ_ASSERT(!GetWindowListener(aWindowID));
}

void MediaManager::OnCameraMute(bool aMute) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("OnCameraMute for all windows");
  mCamerasMuted = aMute;
  // This is safe since we're on main-thread, and the windowlist can only
  // be added to from the main-thread
  for (auto iter = mActiveWindows.Iter(); !iter.Done(); iter.Next()) {
    iter.UserData()->MuteOrUnmuteCameras(aMute);
  }
}

void MediaManager::OnMicrophoneMute(bool aMute) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("OnMicrophoneMute for all windows");
  mMicrophonesMuted = aMute;
  // This is safe since we're on main-thread, and the windowlist can only
  // be added to from the main-thread
  for (auto iter = mActiveWindows.Iter(); !iter.Done(); iter.Next()) {
    iter.UserData()->MuteOrUnmuteMicrophones(aMute);
  }
}

RefPtr<GetUserMediaWindowListener> MediaManager::GetOrMakeWindowListener(
    nsPIDOMWindowInner* aWindow) {
  Document* doc = aWindow->GetExtantDoc();
  if (!doc) {
    // The window has been destroyed. Destroyed windows don't have listeners.
    return nullptr;
  }
  nsIPrincipal* principal = doc->NodePrincipal();
  uint64_t windowId = aWindow->WindowID();
  RefPtr<GetUserMediaWindowListener> windowListener =
      GetWindowListener(windowId);
  if (windowListener) {
    MOZ_ASSERT(PrincipalHandleMatches(windowListener->GetPrincipalHandle(),
                                      principal));
  } else {
    windowListener = new GetUserMediaWindowListener(
        windowId, MakePrincipalHandle(principal));
    AddWindowID(windowId, windowListener);
  }
  return windowListener;
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

  aListener->MuteOrUnmuteCameras(mCamerasMuted);
  aListener->MuteOrUnmuteMicrophones(mMicrophonesMuted);
  GetActiveWindows()->InsertOrUpdate(aWindowId, std::move(aListener));
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
  obs->NotifyWhenScriptSafe(nullptr, "recording-window-ended", data.get());
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
  GetPrefBool(aBranch, "media.getusermedia.hpf_enabled", aData, &mPrefs.mHPFOn);
  GetPrefBool(aBranch, "media.getusermedia.noise_enabled", aData,
              &mPrefs.mNoiseOn);
  GetPref(aBranch, "media.getusermedia.aec", aData, &mPrefs.mAec);
  GetPref(aBranch, "media.getusermedia.agc", aData, &mPrefs.mAgc);
  GetPref(aBranch, "media.getusermedia.noise", aData, &mPrefs.mNoise);
  GetPref(aBranch, "media.getusermedia.aecm_output_routing", aData,
          &mPrefs.mRoutingMode);
  GetPrefBool(aBranch, "media.getusermedia.aec_extended_filter", aData,
              &mPrefs.mExtendedFilter);
  GetPrefBool(aBranch, "media.getusermedia.aec_aec_delay_agnostic", aData,
              &mPrefs.mDelayAgnostic);
  GetPref(aBranch, "media.getusermedia.channels", aData, &mPrefs.mChannels);
  bool oldFakeDeviceChangeEventOn = mPrefs.mFakeDeviceChangeEventOn;
  GetPrefBool(aBranch, "media.ondevicechange.fakeDeviceChangeEvent.enabled",
              aData, &mPrefs.mFakeDeviceChangeEventOn);
  if (mPrefs.mFakeDeviceChangeEventOn != oldFakeDeviceChangeEventOn) {
    // Dispatch directly to the media thread since we're guaranteed to not be in
    // shutdown here. This is called either on construction, or when a pref has
    // changed. The pref observers are disconnected during shutdown.
    MOZ_DIAGNOSTIC_ASSERT(!sHasShutdown);
    MOZ_ALWAYS_SUCCEEDS(mMediaThread->Dispatch(NS_NewRunnableFunction(
        "MediaManager::SetFakeDeviceChangeEventsEnabled",
        [enable = mPrefs.mFakeDeviceChangeEventOn] {
          if (MediaManager* mm = MediaManager::GetIfExists()) {
            mm->GetBackend()->SetFakeDeviceChangeEventsEnabled(enable);
          }
        })));
  }
#endif
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
  obs->RemoveObserver(this, "getUserMedia:muteVideo");
  obs->RemoveObserver(this, "getUserMedia:unmuteVideo");
  obs->RemoveObserver(this, "getUserMedia:muteAudio");
  obs->RemoveObserver(this, "getUserMedia:unmuteAudio");
  obs->RemoveObserver(this, "application-background");
  obs->RemoveObserver(this, "application-foreground");

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
    prefs->RemoveObserver("media.getusermedia.hpf_enabled", this);
    prefs->RemoveObserver("media.getusermedia.agc", this);
    prefs->RemoveObserver("media.getusermedia.noise_enabled", this);
    prefs->RemoveObserver("media.getusermedia.noise", this);
    prefs->RemoveObserver("media.ondevicechange.fakeDeviceChangeEvent.enabled",
                          this);
    prefs->RemoveObserver("media.getusermedia.channels", this);
#endif
  }

  if (mDeviceChangeTimer) {
    mDeviceChangeTimer->Cancel();
    // Drop ref to MediaTimer early to avoid blocking SharedThreadPool shutdown
    mDeviceChangeTimer = nullptr;
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
  mDeviceIDs.clear();
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
          mManager->mBackend->SetFakeDeviceChangeEventsEnabled(false);
          mManager->mBackend->Shutdown();  // idempotent
          mManager->mDeviceListChangeListener.DisconnectIfExists();
        }
      }
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
#ifdef DEBUG
  {
    StaticMutexAutoLock lock(sSingletonMutex);
    MOZ_ASSERT(this == sSingleton);
  }
#endif

  // Release the backend (and call Shutdown()) from within the MediaManager
  // thread Don't use MediaManager::Dispatch() because we're sHasShutdown=true
  // here!
  auto shutdown = MakeRefPtr<ShutdownTask>(
      this, media::NewRunnableFrom([]() {
        LOG("MediaManager shutdown lambda running, releasing MediaManager "
            "singleton and thread");
        StaticMutexAutoLock lock(sSingletonMutex);
        // Remove async shutdown blocker
        media::GetShutdownBarrier()->RemoveBlocker(
            sSingleton->mShutdownBlocker);

        sSingleton = nullptr;
        return NS_OK;
      }));
  MOZ_ALWAYS_SUCCEEDS(mMediaThread->Dispatch(shutdown.forget()));
  mMediaThread->BeginShutdown();
  mMediaThread->AwaitShutdownAndIdle();
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

static MediaSourceEnum ParseScreenColonWindowID(const char16_t* aData,
                                                uint64_t* aWindowIDOut) {
  MOZ_ASSERT(aWindowIDOut);
  // may be windowid or screen:windowid
  const nsDependentString data(aData);
  if (Substring(data, 0, strlen("screen:")).EqualsLiteral("screen:")) {
    nsresult rv;
    *aWindowIDOut = Substring(data, strlen("screen:")).ToInteger64(&rv);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
    return MediaSourceEnum::Screen;
  }
  nsresult rv;
  *aWindowIDOut = data.ToInteger64(&rv);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  return MediaSourceEnum::Camera;
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
    MediaManager::Dispatch(NewTaskFrom([task] { task->Run(); }));
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
      return task->Denied(MediaMgrError::Name::AbortError, "In shutdown"_ns);
    }
    task->Allowed();
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
    uint64_t windowID;
    if (ParseScreenColonWindowID(aData, &windowID) == MediaSourceEnum::Screen) {
      LOG("Revoking ScreenCapture access for window %" PRIu64, windowID);
      StopScreensharing(windowID);
    } else {
      LOG("Revoking MediaCapture access for window %" PRIu64, windowID);
      OnNavigation(windowID);
    }
    return NS_OK;
  } else if (!strcmp(aTopic, "getUserMedia:muteVideo") ||
             !strcmp(aTopic, "getUserMedia:unmuteVideo")) {
    OnCameraMute(!strcmp(aTopic, "getUserMedia:muteVideo"));
    return NS_OK;
  } else if (!strcmp(aTopic, "getUserMedia:muteAudio") ||
             !strcmp(aTopic, "getUserMedia:unmuteAudio")) {
    OnMicrophoneMute(!strcmp(aTopic, "getUserMedia:muteAudio"));
    return NS_OK;
  } else if ((!strcmp(aTopic, "application-background") ||
              !strcmp(aTopic, "application-foreground")) &&
             StaticPrefs::media_getusermedia_camera_background_mute_enabled()) {
    // On mobile we turn off any cameras (but not mics) while in the background.
    // Keeping things simple for now by duplicating test-covered code above.
    //
    // NOTE: If a mobile device ever wants to implement "getUserMedia:muteVideo"
    // as well, it'd need to update this code to handle & test the combinations.
    OnCameraMute(!strcmp(aTopic, "application-background"));
  }

  return NS_OK;
}

NS_IMETHODIMP
MediaManager::CollectReports(nsIHandleReportCallback* aHandleReport,
                             nsISupports* aData, bool aAnonymize) {
  size_t amount = 0;
  amount += mActiveWindows.ShallowSizeOfExcludingThis(MallocSizeOf);
  for (auto iter = mActiveWindows.ConstIter(); !iter.Done(); iter.Next()) {
    const GetUserMediaWindowListener* listener = iter.UserData();
    amount += listener->SizeOfIncludingThis(MallocSizeOf);
  }
  amount += mActiveCallbacks.ShallowSizeOfExcludingThis(MallocSizeOf);
  for (auto iter = mActiveCallbacks.ConstIter(); !iter.Done(); iter.Next()) {
    // Assume nsString buffers for keys are accounted in mCallIds.
    const GetUserMediaTask* task = iter.UserData();
    amount += task->SizeOfIncludingThis(MallocSizeOf);
  }
  amount += mCallIds.ShallowSizeOfExcludingThis(MallocSizeOf);
  for (auto iter = mCallIds.ConstIter(); !iter.Done(); iter.Next()) {
    const nsTArray<nsString>* array = iter.UserData();
    amount += array->ShallowSizeOfExcludingThis(MallocSizeOf);
    for (const nsString& callID : *array) {
      amount += callID.SizeOfExcludingThisEvenIfShared(MallocSizeOf);
    }
  }
  amount += mPendingGUMRequest.ShallowSizeOfExcludingThis(MallocSizeOf);
  // GetUserMediaRequest pointees of mPendingGUMRequest do not have support
  // for memory accounting.  mPendingGUMRequest logic should probably be moved
  // to the front end (bug 1691625).
  amount += mDeviceIDs.shallowSizeOfExcludingThis(MallocSizeOf);
  for (auto iter = mDeviceIDs.iter(); !iter.done(); iter.next()) {
    const nsString deviceID = iter.get();
    amount += deviceID.SizeOfExcludingThisEvenIfShared(MallocSizeOf);
  }
  MOZ_COLLECT_REPORT("explicit/media/media-manager-aggregates", KIND_HEAP,
                     UNITS_BYTES, amount,
                     "Memory used by MediaManager variable length members.");
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
MediaManager::MediaCaptureWindowState(
    nsIDOMWindow* aCapturedWindow, uint16_t* aCamera, uint16_t* aMicrophone,
    uint16_t* aScreen, uint16_t* aWindow, uint16_t* aBrowser,
    nsTArray<RefPtr<nsIMediaDevice>>& aDevices) {
  MOZ_ASSERT(NS_IsMainThread());

  CaptureState camera = CaptureState::Off;
  CaptureState microphone = CaptureState::Off;
  CaptureState screen = CaptureState::Off;
  CaptureState window = CaptureState::Off;
  CaptureState browser = CaptureState::Off;
  auto devices = MakeRefPtr<MediaDeviceSetRefCnt>();

  nsCOMPtr<nsPIDOMWindowInner> piWin = do_QueryInterface(aCapturedWindow);
  if (piWin) {
    if (RefPtr<GetUserMediaWindowListener> listener =
            GetWindowListener(piWin->WindowID())) {
      camera = listener->CapturingSource(MediaSourceEnum::Camera);
      microphone = listener->CapturingSource(MediaSourceEnum::Microphone);
      screen = listener->CapturingSource(MediaSourceEnum::Screen);
      window = listener->CapturingSource(MediaSourceEnum::Window);
      browser = listener->CapturingSource(MediaSourceEnum::Browser);
      listener->GetDevices(devices);
    }
  }

  *aCamera = FromCaptureState(camera);
  *aMicrophone = FromCaptureState(microphone);
  *aScreen = FromCaptureState(screen);
  *aWindow = FromCaptureState(window);
  *aBrowser = FromCaptureState(browser);
  for (auto& device : *devices) {
    aDevices.AppendElement(device);
  }

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
  // We need to stop window/screensharing for all streams in this innerwindow.

  if (RefPtr<GetUserMediaWindowListener> listener =
          GetWindowListener(aWindowID)) {
    listener->StopSharing();
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
  RefPtr<PermissionDelegateHandler> permDelegate =
      doc->GetPermissionDelegateHandler();
  if (NS_WARN_IF(!permDelegate)) {
    return false;
  }

  uint32_t audio = nsIPermissionManager::UNKNOWN_ACTION;
  uint32_t video = nsIPermissionManager::UNKNOWN_ACTION;
  {
    rv = permDelegate->GetPermission("microphone"_ns, &audio, true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
    rv = permDelegate->GetPermission("camera"_ns, &video, true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
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
                              RefPtr<LocalTrackSource> aVideoTrackSource,
                              bool aStartVideoMuted, bool aStartAudioMuted) {
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
    mAudioDeviceState->mDeviceMuted = aStartAudioMuted;
    if (aStartAudioMuted) {
      mAudioDeviceState->mTrackSource->Mute();
    }
  }

  if (aVideoDevice) {
    bool offWhileDisabled =
        aVideoDevice->GetMediaSource() == MediaSourceEnum::Camera &&
        Preferences::GetBool(
            "media.getusermedia.camera.off_while_disabled.enabled", true);
    mVideoDeviceState =
        MakeUnique<DeviceState>(std::move(aVideoDevice),
                                std::move(aVideoTrackSource), offWhileDisabled);
    mVideoDeviceState->mDeviceMuted = aStartVideoMuted;
    if (aStartVideoMuted) {
      mVideoDeviceState->mTrackSource->Mute();
    }
  }
}

RefPtr<SourceListener::SourceListenerPromise>
SourceListener::InitializeAsync() {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");
  MOZ_DIAGNOSTIC_ASSERT(!mStopped);

  return MediaManager::Dispatch<SourceListenerPromise>(
             __func__,
             [principal = GetPrincipalHandle(),
              audioDevice =
                  mAudioDeviceState ? mAudioDeviceState->mDevice : nullptr,
              audioTrack = mAudioDeviceState
                               ? mAudioDeviceState->mTrackSource->mTrack
                               : nullptr,
              audioDeviceMuted =
                  mAudioDeviceState ? mAudioDeviceState->mDeviceMuted : false,
              videoDevice =
                  mVideoDeviceState ? mVideoDeviceState->mDevice : nullptr,
              videoTrack = mVideoDeviceState
                               ? mVideoDeviceState->mTrackSource->mTrack
                               : nullptr,
              videoDeviceMuted =
                  mVideoDeviceState ? mVideoDeviceState->mDeviceMuted : false](
                 MozPromiseHolder<SourceListenerPromise>& aHolder) {
               if (audioDevice) {
                 audioDevice->SetTrack(audioTrack, principal);
               }

               if (videoDevice) {
                 videoDevice->SetTrack(videoTrack, principal);
               }

               if (audioDevice) {
                 nsresult rv = audioDeviceMuted ? NS_OK : audioDevice->Start();
                 if (rv == NS_ERROR_NOT_AVAILABLE) {
                   PR_Sleep(200);
                   rv = audioDevice->Start();
                 }
                 if (NS_FAILED(rv)) {
                   nsCString log;
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
                 nsresult rv = videoDeviceMuted ? NS_OK : videoDevice->Start();
                 if (NS_FAILED(rv)) {
                   if (audioDevice) {
                     if (NS_WARN_IF(NS_FAILED(audioDevice->Stop()))) {
                       MOZ_ASSERT_UNREACHABLE("Stopping audio failed");
                     }
                   }
                   aHolder.Reject(MakeRefPtr<MediaMgrError>(
                                      MediaMgrError::Name::AbortError,
                                      "Starting video failed"),
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
      StopAudioTrack();
    }
  }
  if (mVideoDeviceState) {
    mVideoDeviceState->mDisableTimer->Cancel();
    if (!mVideoDeviceState->mStopped) {
      StopVideoTrack();
    }
  }

  mWindowListener->Remove(this);
  mWindowListener = nullptr;
}

void SourceListener::StopTrack(MediaTrack* aTrack) {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");
  MOZ_ASSERT(Activated(), "No device to stop");
  DeviceState& state = GetDeviceStateFor(aTrack);

  LOG("SourceListener %p stopping %s track for track %p", this,
      &state == mAudioDeviceState.get() ? "audio" : "video", aTrack);

  if (state.mStopped) {
    // device already stopped.
    return;
  }
  state.mStopped = true;

  state.mDisableTimer->Cancel();

  MediaManager::Dispatch(NewTaskFrom([device = state.mDevice]() {
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

void SourceListener::StopAudioTrack() {
  StopTrack(mAudioDeviceState->mTrackSource->mTrack);
}

void SourceListener::StopVideoTrack() {
  StopTrack(mVideoDeviceState->mTrackSource->mTrack);
}

void SourceListener::GetSettingsFor(MediaTrack* aTrack,
                                    MediaTrackSettings& aOutSettings) const {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");
  DeviceState& state = GetDeviceStateFor(aTrack);
  state.mDevice->GetSettings(aOutSettings);

  MediaSourceEnum mediaSource = state.mDevice->GetMediaSource();
  if (mediaSource == MediaSourceEnum::Camera ||
      mediaSource == MediaSourceEnum::Microphone) {
    aOutSettings.mDeviceId.Construct(state.mDevice->mID);
    aOutSettings.mGroupId.Construct(state.mDevice->mGroupID);
  }
}

auto SourceListener::UpdateDevice(MediaTrack* aTrack, bool aOn)
    -> RefPtr<DeviceOperationPromise> {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<SourceListener> self = this;
  DeviceState& state = GetDeviceStateFor(aTrack);

  return MediaManager::Dispatch<DeviceOperationPromise>(
             __func__,
             [self, device = state.mDevice,
              aOn](MozPromiseHolder<DeviceOperationPromise>& h) {
               LOG("Turning %s device (%s)", aOn ? "on" : "off",
                   NS_ConvertUTF16toUTF8(device->mName).get());
               h.Resolve(aOn ? device->Start() : device->Stop(), __func__);
             })
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self, this, &state, track = RefPtr<MediaTrack>(aTrack),
           aOn](nsresult aResult) {
            if (state.mStopped) {
              // Device was stopped on main thread during the operation. Done.
              return DeviceOperationPromise::CreateAndResolve(aResult,
                                                              __func__);
            }
            LOG("SourceListener %p turning %s %s input device for track %p %s",
                this, aOn ? "on" : "off",
                &state == mAudioDeviceState.get() ? "audio" : "video",
                track.get(), NS_SUCCEEDED(aResult) ? "succeeded" : "failed");

            if (NS_FAILED(aResult) && aResult != NS_ERROR_ABORT) {
              // This path handles errors from starting or stopping the device.
              // NS_ERROR_ABORT are for cases where *we* aborted. They need
              // graceful handling.
              if (aOn) {
                // Starting the device failed. Stopping the track here will make
                // the MediaStreamTrack end after a pass through the
                // MediaTrackGraph.
                StopTrack(track);
              } else {
                // Stopping the device failed. This is odd, but not fatal.
                MOZ_ASSERT_UNREACHABLE("The device should be stoppable");
              }
            }
            return DeviceOperationPromise::CreateAndResolve(aResult, __func__);
          },
          []() {
            MOZ_ASSERT_UNREACHABLE("Unexpected and unhandled reject");
            return DeviceOperationPromise::CreateAndReject(false, __func__);
          });
}

void SourceListener::SetEnabledFor(MediaTrack* aTrack, bool aEnable) {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");
  MOZ_ASSERT(Activated(), "No device to set enabled state for");

  DeviceState& state = GetDeviceStateFor(aTrack);

  LOG("SourceListener %p %s %s track for track %p", this,
      aEnable ? "enabling" : "disabling",
      &state == mAudioDeviceState.get() ? "audio" : "video", aTrack);

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
            &state == mAudioDeviceState.get()
                ? "media.getusermedia.microphone.off_while_disabled.delay_ms"
                : "media.getusermedia.camera.off_while_disabled.delay_ms",
            3000));
    const TimeDuration durationEnabled =
        TimeStamp::Now() - state.mTrackEnabledTime;
    const TimeDuration delay = TimeDuration::Max(
        TimeDuration::FromMilliseconds(0), maxDelay - durationEnabled);
    timerPromise = state.mDisableTimer->WaitFor(delay, __func__);
  }

  RefPtr<SourceListener> self = this;
  timerPromise
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self, this, &state, track = RefPtr<MediaTrack>(aTrack),
           aEnable]() mutable {
            MOZ_ASSERT(state.mDeviceEnabled != aEnable,
                       "Device operation hasn't started");
            MOZ_ASSERT(state.mOperationInProgress,
                       "It's our responsibility to reset the inProgress state");

            LOG("SourceListener %p %s %s track for track %p - starting device "
                "operation",
                this, aEnable ? "enabling" : "disabling",
                &state == mAudioDeviceState.get() ? "audio" : "video",
                track.get());

            if (state.mStopped) {
              // Source was stopped between timer resolving and this runnable.
              return DeviceOperationPromise::CreateAndResolve(NS_ERROR_ABORT,
                                                              __func__);
            }

            state.mDeviceEnabled = aEnable;

            if (mWindowListener) {
              mWindowListener->ChromeAffectingStateChanged();
            }
            if (!state.mOffWhileDisabled || state.mDeviceMuted) {
              // If the feature to turn a device off while disabled is itself
              // disabled, or the device is currently user agent muted, then
              // we shortcut the device operation and tell the
              // ux-updating code that everything went fine.
              return DeviceOperationPromise::CreateAndResolve(NS_OK, __func__);
            }
            return UpdateDevice(track, aEnable);
          },
          []() {
            // Timer was canceled by us. We signal this with NS_ERROR_ABORT.
            return DeviceOperationPromise::CreateAndResolve(NS_ERROR_ABORT,
                                                            __func__);
          })
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self, this, &state, track = RefPtr<MediaTrack>(aTrack),
           aEnable](nsresult aResult) mutable {
            MOZ_ASSERT_IF(aResult != NS_ERROR_ABORT,
                          state.mDeviceEnabled == aEnable);
            MOZ_ASSERT(state.mOperationInProgress);
            state.mOperationInProgress = false;

            if (state.mStopped) {
              // Device was stopped on main thread during the operation. Nothing
              // to do.
              return;
            }

            if (NS_FAILED(aResult) && aResult != NS_ERROR_ABORT && !aEnable) {
              // To keep our internal state sane in this case, we disallow
              // future stops due to disable.
              state.mOffWhileDisabled = false;
              return;
            }

            // This path is for a device operation aResult that was success or
            // NS_ERROR_ABORT (*we* canceled the operation).
            // At this point we have to follow up on the intended state, i.e.,
            // update the device state if the track state changed in the
            // meantime.

            if (state.mTrackEnabled != state.mDeviceEnabled) {
              // Track state changed during this operation. We'll start over.
              SetEnabledFor(track, state.mTrackEnabled);
            }
          },
          []() { MOZ_ASSERT_UNREACHABLE("Unexpected and unhandled reject"); });
}

void SourceListener::SetMutedFor(LocalTrackSource* aTrackSource, bool aMute) {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");
  MOZ_ASSERT(Activated(), "No device to set muted state for");

  MediaTrack* track = aTrackSource->mTrack;
  DeviceState& state = GetDeviceStateFor(track);

  LOG("SourceListener %p %s %s track for track %p", this,
      aMute ? "muting" : "unmuting",
      &state == mAudioDeviceState.get() ? "audio" : "video", track);

  if (state.mStopped) {
    // Device terminally stopped. Updating device state is pointless.
    return;
  }

  if (state.mDeviceMuted == aMute) {
    // Device is already in the desired state.
    return;
  }

  LOG("SourceListener %p %s %s track for track %p - starting device operation",
      this, aMute ? "muting" : "unmuting",
      &state == mAudioDeviceState.get() ? "audio" : "video", track);

  state.mDeviceMuted = aMute;

  if (mWindowListener) {
    mWindowListener->ChromeAffectingStateChanged();
  }
  // Update trackSource to fire mute/unmute events on all its tracks
  if (aMute) {
    aTrackSource->Mute();
  } else {
    aTrackSource->Unmute();
  }
  if (!state.mOffWhileDisabled || !state.mDeviceEnabled) {
    // If the pref to turn the underlying device is itself off, or the device is
    // already off, it's unecessary to do anything else.
    return;
  }
  UpdateDevice(track, !aMute);
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
    // We want to stop the whole track if there's no audio;
    // just the video track if we have both.
    // StopTrack figures this out for us.
    StopTrack(mVideoDeviceState->mTrackSource->mTrack);
  }
  if (mAudioDeviceState && mAudioDeviceState->mDevice->GetMediaSource() ==
                               MediaSourceEnum::AudioCapture) {
    static_cast<AudioCaptureTrackSource*>(mAudioDeviceState->mTrackSource.get())
        ->Stop();
  }
}

void SourceListener::MuteOrUnmuteCamera(bool aMute) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mStopped) {
    return;
  }

  MOZ_RELEASE_ASSERT(mWindowListener);
  LOG("SourceListener %p MuteOrUnmuteCamera", this);

  if (mVideoDeviceState && (mVideoDeviceState->mDevice->GetMediaSource() ==
                            MediaSourceEnum::Camera)) {
    SetMutedFor(mVideoDeviceState->mTrackSource, aMute);
  }
}

void SourceListener::MuteOrUnmuteMicrophone(bool aMute) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mStopped) {
    return;
  }

  MOZ_RELEASE_ASSERT(mWindowListener);
  LOG("SourceListener %p MuteOrUnmuteMicrophone", this);

  if (mAudioDeviceState && (mAudioDeviceState->mDevice->GetMediaSource() ==
                            MediaSourceEnum::Microphone)) {
    SetMutedFor(mAudioDeviceState->mTrackSource, aMute);
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

  // Source is a match and is active and unmuted

  if (state.mDeviceEnabled && !state.mDeviceMuted) {
    return CaptureState::Enabled;
  }

  return CaptureState::Disabled;
}

RefPtr<SourceListener::SourceListenerPromise>
SourceListener::ApplyConstraintsToTrack(
    MediaTrack* aTrack, const MediaTrackConstraints& aConstraints,
    CallerType aCallerType) {
  MOZ_ASSERT(NS_IsMainThread());
  DeviceState& state = GetDeviceStateFor(aTrack);

  if (mStopped || state.mStopped) {
    LOG("gUM %s track for track %p applyConstraints, but source is stopped",
        &state == mAudioDeviceState.get() ? "audio" : "video", aTrack);
    return SourceListenerPromise::CreateAndResolve(false, __func__);
  }

  MediaManager* mgr = MediaManager::GetIfExists();
  if (!mgr) {
    return SourceListenerPromise::CreateAndResolve(false, __func__);
  }

  return MediaManager::Dispatch<SourceListenerPromise>(
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

          aHolder.Reject(MakeRefPtr<MediaMgrError>(
                             MediaMgrError::Name::OverconstrainedError, "",
                             NS_ConvertASCIItoUTF16(badConstraint)),
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

DeviceState& SourceListener::GetDeviceStateFor(MediaTrack* aTrack) const {
  if (mAudioDeviceState && mAudioDeviceState->mTrackSource->mTrack == aTrack) {
    return *mAudioDeviceState;
  }
  if (mVideoDeviceState && mVideoDeviceState->mTrackSource->mTrack == aTrack) {
    return *mVideoDeviceState;
  }
  MOZ_CRASH("Unknown track");
}

// Doesn't kill audio
void GetUserMediaWindowListener::StopSharing() {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

  for (auto& l : mActiveListeners.Clone()) {
    l->StopSharing();
  }
}

void GetUserMediaWindowListener::StopRawID(const nsString& removedDeviceID) {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

  for (auto& source : mActiveListeners.Clone()) {
    if (source->GetAudioDevice()) {
      nsString id;
      source->GetAudioDevice()->GetRawId(id);
      if (removedDeviceID.Equals(id)) {
        source->StopAudioTrack();
      }
    }
    if (source->GetVideoDevice()) {
      nsString id;
      source->GetVideoDevice()->GetRawId(id);
      if (removedDeviceID.Equals(id)) {
        source->StopVideoTrack();
      }
    }
  }
}

void GetUserMediaWindowListener::MuteOrUnmuteCameras(bool aMute) {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

  if (mCamerasAreMuted == aMute) {
    return;
  }
  mCamerasAreMuted = aMute;

  for (auto& source : mActiveListeners) {
    if (source->GetVideoDevice()) {
      source->MuteOrUnmuteCamera(aMute);
    }
  }
}

void GetUserMediaWindowListener::MuteOrUnmuteMicrophones(bool aMute) {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

  if (mMicrophonesAreMuted == aMute) {
    return;
  }
  mMicrophonesAreMuted = aMute;

  for (auto& source : mActiveListeners) {
    if (source->GetAudioDevice()) {
      source->MuteOrUnmuteMicrophone(aMute);
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
