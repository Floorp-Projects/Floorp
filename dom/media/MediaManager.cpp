/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaManager.h"

#include "MediaStreamGraph.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "GetUserMediaRequest.h"
#include "MediaStreamListener.h"
#include "nsArray.h"
#include "nsContentUtils.h"
#include "nsHashPropertyBag.h"
#ifdef MOZ_WIDGET_GONK
#include "nsIAudioManager.h"
#endif
#include "nsIEventTarget.h"
#include "nsIUUIDGenerator.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPermissionManager.h"
#include "nsIPopupWindowManager.h"
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
#include "mozilla/Telemetry.h"
#include "mozilla/Types.h"
#include "mozilla/PeerIdentity.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ContentChild.h"
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
#include "Latency.h"
#include "nsProxyRelease.h"
#include "NullPrincipal.h"
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

#ifdef MOZ_B2G
#include "MediaPermissionGonk.h"
#endif

#if defined (XP_WIN)
#include "mozilla/WindowsVersion.h"
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
struct nsIMediaDevice::COMTypeInfo<mozilla::VideoDevice, void> {
  static const nsIID kIID;
};
const nsIID nsIMediaDevice::COMTypeInfo<mozilla::VideoDevice, void>::kIID = NS_IMEDIADEVICE_IID;
template<>
struct nsIMediaDevice::COMTypeInfo<mozilla::AudioDevice, void> {
  static const nsIID kIID;
};
const nsIID nsIMediaDevice::COMTypeInfo<mozilla::AudioDevice, void>::kIID = NS_IMEDIADEVICE_IID;

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

static Atomic<bool> sInShutdown;

typedef media::Pledge<bool, dom::MediaStreamError*> PledgeVoid;

static bool
HostIsHttps(nsIURI &docURI)
{
  bool isHttps;
  nsresult rv = docURI.SchemeIs("https", &isHttps);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  return isHttps;
}

class SourceListener : public MediaStreamListener {
public:
  SourceListener();

  /**
   * Registers this source listener as belonging to the given window listener.
   */
  void Register(GetUserMediaWindowListener* aListener);

  /**
   * Marks this listener as active and adds itself as a listener to aStream.
   */
  void Activate(SourceMediaStream* aStream,
                AudioDevice* aAudioDevice,
                VideoDevice* aVideoDevice);

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
   * Stops all screen/app/window/audioCapture sharing, but not camera or
   * microphone.
   */
  void StopSharing();

  MediaStream* Stream() const
  {
    return mStream;
  }

  SourceMediaStream* GetSourceStream();

  AudioDevice* GetAudioDevice() const
  {
    return mAudioDevice;
  }

  VideoDevice* GetVideoDevice() const
  {
    return mVideoDevice;
  }

  void GetSettings(dom::MediaTrackSettings& aOutSettings, TrackID aTrackID);

  void NotifyPull(MediaStreamGraph* aGraph,
                  StreamTime aDesiredTime) override;

  void NotifyEvent(MediaStreamGraph* aGraph,
                   MediaStreamGraphEvent aEvent) override;

  void NotifyFinished();

  /**
   * this can be in response to our own RemoveListener() (via ::Remove()), or
   * because the DOM GC'd the DOMLocalMediaStream/etc we're attached to.
   */
  void NotifyRemoved();

  void NotifyDirectListeners(MediaStreamGraph* aGraph, bool aHasListeners);

  bool Activated() const
  {
    return mActivated;
  }

  bool Stopped() const
  {
    return mStopped;
  }

  bool CapturingVideo() const;

  bool CapturingAudio() const;

  bool CapturingScreen() const;

  bool CapturingWindow() const;

  bool CapturingApplication() const;

  bool CapturingBrowser() const;

  already_AddRefed<PledgeVoid>
  ApplyConstraintsToTrack(nsPIDOMWindowInner* aWindow,
                          TrackID aTrackID,
                          const dom::MediaTrackConstraints& aConstraints,
                          dom::CallerType aCallerType);

  PrincipalHandle GetPrincipalHandle() const;

private:
  // true after this listener has been Activate()d in a WindowListener.
  // MainThread only.
  bool mActivated;

  // true after this listener has had all devices stopped. MainThread only.
  bool mStopped;

  // true after the stream this listener is listening to has finished in the
  // MediaStreamGraph. MainThread only.
  bool mFinished;

  // true after this listener has been removed from its MediaStream.
  // MainThread only.
  bool mRemoved;

  // true if we have stopped mAudioDevice. MainThread only.
  bool mAudioStopped;

  // true if we have stopped mVideoDevice. MainThread only.
  bool mVideoStopped;

  // never ever indirect off this; just for assertions
  PRThread* mMainThreadCheck;

  // Set in Register() on main thread, then read from any thread.
  PrincipalHandle mPrincipalHandle;

  // Weak pointer to the window listener that owns us. MainThread only.
  GetUserMediaWindowListener* mWindowListener;

  // Set at Activate on MainThread

  // Accessed from MediaStreamGraph thread, MediaManager thread, and MainThread
  // No locking needed as they're only addrefed except on the MediaManager thread
  RefPtr<AudioDevice> mAudioDevice; // threadsafe refcnt
  RefPtr<VideoDevice> mVideoDevice; // threadsafe refcnt
  RefPtr<SourceMediaStream> mStream; // threadsafe refcnt
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
    if (!aListener || aListener->Activated()) {
      MOZ_ASSERT(false, "Invalid listener");
      return;
    }
    if (mInactiveListeners.Contains(aListener)) {
      MOZ_ASSERT(false, "Already registered");
      return;
    }
    if (mActiveListeners.Contains(aListener)) {
      MOZ_ASSERT(false, "Already activated");
      return;
    }

    aListener->Register(this);
    mInactiveListeners.AppendElement(aListener);
  }

  /**
   * Activates an already registered and inactive gUM source listener for this
   * WindowListener.
   */
  void Activate(SourceListener* aListener,
                SourceMediaStream* aStream,
                AudioDevice* aAudioDevice,
                VideoDevice* aVideoDevice)
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!aListener || aListener->Activated()) {
      MOZ_ASSERT(false, "Cannot activate already activated source listener");
      return;
    }

    if (!mInactiveListeners.RemoveElement(aListener)) {
      MOZ_ASSERT(false, "Cannot activate non-registered source listener");
      return;
    }

    RefPtr<SourceListener> listener = aListener;
    listener->Activate(aStream, aAudioDevice, aVideoDevice);
    mActiveListeners.AppendElement(listener.forget());
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

    RefPtr<MediaManager> mgr = MediaManager::GetIfExists();
    if (!mgr) {
      MOZ_ASSERT(false, "MediaManager should stay until everything is removed");
      return;
    }
    GetUserMediaWindowListener* windowListener =
      mgr->GetWindowListener(mWindowID);

    if (!windowListener) {
      nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
      auto* globalWindow = nsGlobalWindow::GetInnerWindowWithId(mWindowID);
      if (globalWindow) {
        RefPtr<GetUserMediaRequest> req =
          new GetUserMediaRequest(globalWindow->AsInner(),
                                  NullString(), NullString());
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

    if (VideoDevice* removedDevice = aListener->GetVideoDevice()) {
      bool revokeVideoPermission = true;
      nsString removedRawId;
      nsString removedSourceType;
      removedDevice->GetRawId(removedRawId);
      removedDevice->GetMediaSource(removedSourceType);
      for (const auto& l : mActiveListeners) {
        if (VideoDevice* device = l->GetVideoDevice()) {
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
        auto* globalWindow = nsGlobalWindow::GetInnerWindowWithId(mWindowID);
        nsPIDOMWindowInner* window = globalWindow ? globalWindow->AsInner()
                                                  : nullptr;
        RefPtr<GetUserMediaRequest> req =
          new GetUserMediaRequest(window, removedRawId, removedSourceType);
        obs->NotifyObservers(req, "recording-device-stopped", nullptr);
      }
    }

    if (AudioDevice* removedDevice = aListener->GetAudioDevice()) {
      bool revokeAudioPermission = true;
      nsString removedRawId;
      nsString removedSourceType;
      removedDevice->GetRawId(removedRawId);
      removedDevice->GetMediaSource(removedSourceType);
      for (const auto& l : mActiveListeners) {
        if (AudioDevice* device = l->GetAudioDevice()) {
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
        auto* globalWindow = nsGlobalWindow::GetInnerWindowWithId(mWindowID);
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

  /**
   * Called by one of our SourceListeners when one of its tracks has stopped.
   * Schedules an event for the next stable state to update chrome.
   */
  void NotifySourceTrackStopped();

  /**
   * Called in stable state to send a notification to update chrome.
   */
  void NotifyChromeOfTrackStops();

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
  bool CapturingScreen() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    for (auto& l : mActiveListeners) {
      if (l->CapturingScreen()) {
        return true;
      }
    }
    return false;
  }
  bool CapturingWindow() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    for (auto& l : mActiveListeners) {
      if (l->CapturingWindow()) {
        return true;
      }
    }
    return false;
  }
  bool CapturingApplication() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    for (auto& l : mActiveListeners) {
      if (l->CapturingApplication()) {
        return true;
      }
    }
    return false;
  }
  bool CapturingBrowser() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    for (auto& l : mActiveListeners) {
      if (l->CapturingBrowser()) {
        return true;
      }
    }
    return false;
  }

  uint64_t WindowID() const
  {
    return mWindowID;
  }

  PrincipalHandle GetPrincipalHandle() const { return mPrincipalHandle; }

private:
  ~GetUserMediaWindowListener()
  {
    for (auto& l : mInactiveListeners) {
      l->NotifyRemoved();
    }
    mInactiveListeners.Clear();
    for (auto& l : mActiveListeners) {
      l->NotifyRemoved();
    }
    mActiveListeners.Clear();
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
 * Send an error back to content.
 * Do this only on the main thread. The onSuccess callback is also passed here
 * so it can be released correctly.
 */
template<class SuccessCallbackType>
class ErrorCallbackRunnable : public Runnable
{
public:
  ErrorCallbackRunnable(
    nsCOMPtr<SuccessCallbackType>&& aOnSuccess,
    nsCOMPtr<nsIDOMGetUserMediaErrorCallback>&& aOnFailure,
    MediaMgrError& aError,
    uint64_t aWindowID)
    : mError(&aError)
    , mWindowID(aWindowID)
    , mManager(MediaManager::GetInstance())
  {
    mOnSuccess.swap(aOnSuccess);
    mOnFailure.swap(aOnFailure);
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<SuccessCallbackType> onSuccess = mOnSuccess.forget();
    nsCOMPtr<nsIDOMGetUserMediaErrorCallback> onFailure = mOnFailure.forget();

    // Only run if the window is still active.
    if (!(mManager->IsWindowStillActive(mWindowID))) {
      return NS_OK;
    }
    // This is safe since we're on main-thread, and the windowlist can only
    // be invalidated from the main-thread (see OnNavigation)
    if (auto* window = nsGlobalWindow::GetInnerWindowWithId(mWindowID)) {
      RefPtr<MediaStreamError> error =
        new MediaStreamError(window->AsInner(), *mError);
      onFailure->OnError(error);
    }
    return NS_OK;
  }
private:
  ~ErrorCallbackRunnable()
  {
    MOZ_ASSERT(!mOnSuccess && !mOnFailure);
  }

  nsCOMPtr<SuccessCallbackType> mOnSuccess;
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> mOnFailure;
  RefPtr<MediaMgrError> mError;
  uint64_t mWindowID;
  RefPtr<MediaManager> mManager; // get ref to this when creating the runnable
};

/**
 * nsIMediaDevice implementation.
 */
NS_IMPL_ISUPPORTS(MediaDevice, nsIMediaDevice)

MediaDevice::MediaDevice(MediaEngineSource* aSource, bool aIsVideo)
  : mScary(aSource->GetScary())
  , mMediaSource(aSource->GetMediaSource())
  , mSource(aSource)
  , mIsVideo(aIsVideo)
{
  mSource->GetName(mName);
  nsCString id;
  mSource->GetUUID(id);
  CopyUTF8toUTF16(id, mID);
}

VideoDevice::VideoDevice(MediaEngineVideoSource* aSource)
  : MediaDevice(aSource, true)
{}

/**
 * Helper functions that implement the constraints algorithm from
 * http://dev.w3.org/2011/webrtc/editor/getusermedia.html#methods-5
 */

bool
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
  nsString id;
  if (aIsChrome) {
    GetRawId(id);
  } else {
    GetId(id);
  }
  return mSource->GetBestFitnessDistance(aConstraintSets, id);
}

AudioDevice::AudioDevice(MediaEngineAudioSource* aSource)
  : MediaDevice(aSource, false)
{
  mMediaSource = aSource->GetMediaSource();
}

NS_IMETHODIMP
MediaDevice::GetName(nsAString& aName)
{
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
MediaDevice::GetType(nsAString& aType)
{
  return NS_OK;
}

NS_IMETHODIMP
VideoDevice::GetType(nsAString& aType)
{
  aType.AssignLiteral(u"video");
  return NS_OK;
}

NS_IMETHODIMP
AudioDevice::GetType(nsAString& aType)
{
  aType.AssignLiteral(u"audio");
  return NS_OK;
}

NS_IMETHODIMP
MediaDevice::GetId(nsAString& aID)
{
  aID.Assign(mID);
  return NS_OK;
}

NS_IMETHODIMP
MediaDevice::GetRawId(nsAString& aID)
{
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
MediaDevice::SetId(const nsAString& aID)
{
  mID.Assign(aID);
}

void
MediaDevice::SetRawId(const nsAString& aID)
{
  mRawID.Assign(aID);
}

NS_IMETHODIMP
MediaDevice::GetMediaSource(nsAString& aMediaSource)
{
  if (mMediaSource == MediaSourceEnum::Microphone) {
    aMediaSource.Assign(NS_LITERAL_STRING("microphone"));
  } else if (mMediaSource == MediaSourceEnum::AudioCapture) {
    aMediaSource.Assign(NS_LITERAL_STRING("audioCapture"));
  } else if (mMediaSource == MediaSourceEnum::Window) { // this will go away
    aMediaSource.Assign(NS_LITERAL_STRING("window"));
  } else { // all the rest are shared
    aMediaSource.Assign(NS_ConvertUTF8toUTF16(
      dom::MediaSourceEnumValues::strings[uint32_t(mMediaSource)].value));
  }
  return NS_OK;
}

VideoDevice::Source*
VideoDevice::GetSource()
{
  return static_cast<Source*>(&*mSource);
}

AudioDevice::Source*
AudioDevice::GetSource()
{
  return static_cast<Source*>(&*mSource);
}

nsresult MediaDevice::Allocate(const dom::MediaTrackConstraints &aConstraints,
                               const MediaEnginePrefs &aPrefs,
                               const ipc::PrincipalInfo& aPrincipalInfo,
                               const char** aOutBadConstraint) {
  return GetSource()->Allocate(aConstraints, aPrefs, mID, aPrincipalInfo,
                               getter_AddRefs(mAllocationHandle),
                               aOutBadConstraint);
}

nsresult MediaDevice::Restart(const dom::MediaTrackConstraints &aConstraints,
                              const MediaEnginePrefs &aPrefs,
                              const char** aOutBadConstraint) {
  return GetSource()->Restart(mAllocationHandle, aConstraints, aPrefs, mID,
                              aOutBadConstraint);
}

nsresult MediaDevice::Deallocate() {
  return GetSource()->Deallocate(mAllocationHandle);
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
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FakeTrackSourceGetter)
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
    nsCOMPtr<nsIDOMGetUserMediaSuccessCallback>& aOnSuccess,
    nsCOMPtr<nsIDOMGetUserMediaErrorCallback>& aOnFailure,
    uint64_t aWindowID,
    GetUserMediaWindowListener* aWindowListener,
    SourceListener* aSourceListener,
    const ipc::PrincipalInfo& aPrincipalInfo,
    const MediaStreamConstraints& aConstraints,
    AudioDevice* aAudioDevice,
    VideoDevice* aVideoDevice,
    PeerIdentity* aPeerIdentity)
    : mConstraints(aConstraints)
    , mAudioDevice(aAudioDevice)
    , mVideoDevice(aVideoDevice)
    , mWindowID(aWindowID)
    , mWindowListener(aWindowListener)
    , mSourceListener(aSourceListener)
    , mPrincipalInfo(aPrincipalInfo)
    , mPeerIdentity(aPeerIdentity)
    , mManager(MediaManager::GetInstance())
  {
    mOnSuccess.swap(aOnSuccess);
    mOnFailure.swap(aOnFailure);
  }

  ~GetUserMediaStreamRunnable() {}

  class TracksAvailableCallback : public OnTracksAvailableCallback
  {
  public:
    TracksAvailableCallback(MediaManager* aManager,
                            already_AddRefed<nsIDOMGetUserMediaSuccessCallback> aSuccess,
                            uint64_t aWindowID,
                            DOMMediaStream* aStream)
      : mWindowID(aWindowID), mOnSuccess(aSuccess), mManager(aManager),
        mStream(aStream) {}
    void NotifyTracksAvailable(DOMMediaStream* aStream) override
    {
      // We're in the main thread, so no worries here.
      if (!(mManager->IsWindowStillActive(mWindowID))) {
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
    uint64_t mWindowID;
    nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> mOnSuccess;
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
    nsGlobalWindow* globalWindow = nsGlobalWindow::GetInnerWindowWithId(mWindowID);
    nsPIDOMWindowInner* window = globalWindow ? globalWindow->AsInner() : nullptr;

    // We're on main-thread, and the windowlist can only
    // be invalidated from the main-thread (see OnNavigation)
    GetUserMediaWindowListener* listener =
      mManager->GetWindowListener(mWindowID);
    if (!listener || !window || !window->GetExtantDoc()) {
      // This window is no longer live.  mListener has already been removed
      return NS_OK;
    }

    MediaStreamGraph::GraphDriverType graphDriverType =
      mAudioDevice ? MediaStreamGraph::AUDIO_THREAD_DRIVER
                   : MediaStreamGraph::SYSTEM_THREAD_DRIVER;
    MediaStreamGraph* msg =
      MediaStreamGraph::GetInstance(graphDriverType,
                                    dom::AudioChannel::Normal);

    RefPtr<DOMMediaStream> domStream;
    RefPtr<SourceMediaStream> stream;
    // AudioCapture is a special case, here, in the sense that we're not really
    // using the audio source and the SourceMediaStream, which acts as
    // placeholders. We re-route a number of stream internaly in the MSG and mix
    // them down instead.
    if (mAudioDevice &&
        mAudioDevice->GetMediaSource() == MediaSourceEnum::AudioCapture) {
      // It should be possible to pipe the capture stream to anything. CORS is
      // not a problem here, we got explicit user content.
      nsCOMPtr<nsIPrincipal> principal = window->GetExtantDoc()->NodePrincipal();
      domStream =
        DOMMediaStream::CreateAudioCaptureStreamAsInput(window, principal, msg);

      stream = msg->CreateSourceStream(
        globalWindow->AbstractMainThreadFor(TaskCategory::Other)); // Placeholder
      msg->RegisterCaptureStreamForWindow(
            mWindowID, domStream->GetInputStream()->AsProcessedStream());
      window->SetAudioCapture(true);
    } else {
      class LocalTrackSource : public MediaStreamTrackSource
      {
      public:
        LocalTrackSource(nsIPrincipal* aPrincipal,
                         const nsString& aLabel,
                         SourceListener* aListener,
                         const MediaSourceEnum aSource,
                         const TrackID aTrackID,
                         const PeerIdentity* aPeerIdentity)
          : MediaStreamTrackSource(aPrincipal, aLabel), mListener(aListener),
            mSource(aSource), mTrackID(aTrackID), mPeerIdentity(aPeerIdentity) {}

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
          if (sInShutdown || !mListener) {
            // Track has been stopped, or we are in shutdown. In either case
            // there's no observable outcome, so pretend we succeeded.
            RefPtr<PledgeVoid> p = new PledgeVoid();
            p->Resolve(false);
            return p.forget();
          }
          return mListener->ApplyConstraintsToTrack(aWindow, mTrackID,
                                                    aConstraints, aCallerType);
        }

        void
        GetSettings(dom::MediaTrackSettings& aOutSettings) override
        {
          if (mListener) {
            mListener->GetSettings(aOutSettings, mTrackID);
          }
        }

        void Stop() override
        {
          if (mListener) {
            mListener->StopTrack(mTrackID);
            mListener = nullptr;
          }
        }

      protected:
        ~LocalTrackSource() {}

        RefPtr<SourceListener> mListener;
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
      domStream =
        DOMLocalMediaStream::CreateSourceStreamAsInput(window, msg,
                                                       new FakeTrackSourceGetter(principal));
      stream = domStream->GetInputStream()->AsSourceStream();

      if (mAudioDevice) {
        nsString audioDeviceName;
        mAudioDevice->GetName(audioDeviceName);
        const MediaSourceEnum source =
          mAudioDevice->GetSource()->GetMediaSource();
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
        const MediaSourceEnum source =
          mVideoDevice->GetSource()->GetMediaSource();
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

    if (!domStream || !stream || sInShutdown) {
      nsCOMPtr<nsIDOMGetUserMediaErrorCallback> onFailure = mOnFailure.forget();
      LOG(("Returning error for getUserMedia() - no stream"));

      if (auto* window = nsGlobalWindow::GetInnerWindowWithId(mWindowID)) {
        RefPtr<MediaStreamError> error = new MediaStreamError(window->AsInner(),
            NS_LITERAL_STRING("InternalError"),
            sInShutdown ? NS_LITERAL_STRING("In shutdown") :
                          NS_LITERAL_STRING("No stream."));
        onFailure->OnError(error);
      }
      return NS_OK;
    }

    // Activate our source listener. We'll call Start() on the source when we
    // get a callback that the MediaStream has started consuming. The listener
    // is freed when the page is invalidated (on navigation or close).
    mWindowListener->Activate(mSourceListener, stream, mAudioDevice, mVideoDevice);

    // Note: includes JS callbacks; must be released on MainThread
    auto callback = MakeRefPtr<Refcountable<UniquePtr<OnTracksAvailableCallback>>>(
        new TracksAvailableCallback(mManager, mOnSuccess.forget(), mWindowID, domStream));

    // Dispatch to the media thread to ask it to start the sources,
    // because that can take a while.
    // Pass ownership of domStream through the lambda to
    // GetUserMediaNotificationEvent to ensure it's kept alive until the
    // GetUserMediaNotificationEvent runs or is discarded.
    RefPtr<GetUserMediaStreamRunnable> self = this;
    MediaManager::PostTask(NewTaskFrom([self, domStream, callback]() mutable {
      MOZ_ASSERT(MediaManager::IsInMediaThread());
      SourceMediaStream* source = self->mSourceListener->GetSourceStream();

      RefPtr<MediaMgrError> error = nullptr;
      if (self->mAudioDevice) {
        nsresult rv =
          self->mAudioDevice->GetSource()->Start(source, kAudioTrack,
                                                 self->mSourceListener->GetPrincipalHandle());
        if (NS_FAILED(rv)) {
          nsString log;
          log.AssignASCII("Starting audio failed");
          error = new MediaMgrError(NS_LITERAL_STRING("InternalError"), log);
        }
      }

      if (!error && self->mVideoDevice) {
        nsresult rv =
          self->mVideoDevice->GetSource()->Start(source, kVideoTrack,
                                                 self->mSourceListener->GetPrincipalHandle());
        if (NS_FAILED(rv)) {
          nsString log;
          log.AssignASCII("Starting video failed");
          error = new MediaMgrError(NS_LITERAL_STRING("InternalError"), log);
        }
      }

      if (error) {
        // The DOM stream and track callback must be released on main thread.
        NS_DispatchToMainThread(do_AddRef(new ReleaseMediaOperationResource(
          domStream.forget(), callback.forget())));

        // Dispatch the error callback on main thread.
        nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> onSuccess;
        NS_DispatchToMainThread(do_AddRef(
          new ErrorCallbackRunnable<nsIDOMGetUserMediaSuccessCallback>(
            Move(onSuccess), Move(self->mOnFailure), *error, self->mWindowID)));

        // This should be empty now
        MOZ_ASSERT(!self->mOnFailure);
        return NS_OK;
      }

      // Start() queued the tracks to be added synchronously to avoid races
      source->FinishAddTracks();

      source->SetPullEnabled(true);
      source->AdvanceKnownTracksTime(STREAM_TIME_MAX);

      LOG(("started all sources"));

      // Forward onTracksAvailableCallback to GetUserMediaNotificationEvent,
      // because onTracksAvailableCallback needs to be added to domStream
      // on the main thread.
      // The event runnable must always be released on mainthread due to the JS
      // callbacks in the TracksAvailableCallback.
      NS_DispatchToMainThread(do_AddRef(
        new GetUserMediaNotificationEvent(
          GetUserMediaNotificationEvent::STARTING,
          domStream.forget(),
          callback.forget(),
          self->mWindowID,
          self->mOnFailure.forget())));
      return NS_OK;
    }));

    if (!IsPincipalInfoPrivate(mPrincipalInfo)) {
      // Call GetPrincipalKey again, this time w/persist = true, to promote
      // deviceIds to persistent, in case they're not already. Fire'n'forget.
      RefPtr<Pledge<nsCString>> p =
        media::GetPrincipalKey(mPrincipalInfo, true);
    }
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> mOnSuccess;
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> mOnFailure;
  MediaStreamConstraints mConstraints;
  RefPtr<AudioDevice> mAudioDevice;
  RefPtr<VideoDevice> mVideoDevice;
  uint64_t mWindowID;
  RefPtr<GetUserMediaWindowListener> mWindowListener;
  RefPtr<SourceListener> mSourceListener;
  ipc::PrincipalInfo mPrincipalInfo;
  RefPtr<PeerIdentity> mPeerIdentity;
  RefPtr<MediaManager> mManager; // get ref to this when creating the runnable
};

// Source getter returning full list

template<class DeviceType>
static void
GetSources(MediaEngine *engine, MediaSourceEnum aSrcType,
           void (MediaEngine::* aEnumerate)(MediaSourceEnum,
               nsTArray<RefPtr<typename DeviceType::Source> >*),
           nsTArray<RefPtr<DeviceType>>& aResult,
           const char* media_device_name = nullptr)
{
  nsTArray<RefPtr<typename DeviceType::Source>> sources;

  (engine->*aEnumerate)(aSrcType, &sources);
  /**
    * We're allowing multiple tabs to access the same camera for parity
    * with Chrome.  See bug 811757 for some of the issues surrounding
    * this decision.  To disallow, we'd filter by IsAvailable() as we used
    * to.
    */
  if (media_device_name && *media_device_name)  {
    for (auto& source : sources) {
      nsString deviceName;
      source->GetName(deviceName);
      if (deviceName.EqualsASCII(media_device_name)) {
        aResult.AppendElement(new DeviceType(source));
        break;
      }
    }
  } else {
    for (auto& source : sources) {
      aResult.AppendElement(new DeviceType(source));
    }
  }
}

// TODO: Remove once upgraded to GCC 4.8+ on linux. Bogus error on static func:
// error: 'this' was not captured for this lambda function

static auto& MediaManager_GetInstance = MediaManager::GetInstance;
static auto& MediaManager_ToJSArray = MediaManager::ToJSArray;
static auto& MediaManager_AnonymizeDevices = MediaManager::AnonymizeDevices;

already_AddRefed<MediaManager::PledgeChar>
MediaManager::SelectSettings(
    MediaStreamConstraints& aConstraints,
    bool aIsChrome,
    RefPtr<Refcountable<UniquePtr<SourceSet>>>& aSources)
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

    nsTArray<RefPtr<VideoDevice>> videos;
    nsTArray<RefPtr<AudioDevice>> audios;

    for (auto& source : sources) {
      if (source->mIsVideo) {
        RefPtr<VideoDevice> video = static_cast<VideoDevice*>(source.get());
        videos.AppendElement(video);
      } else {
        RefPtr<AudioDevice> audio = static_cast<AudioDevice*>(source.get());
        audios.AppendElement(audio);
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
      RefPtr<MediaManager> mgr = MediaManager_GetInstance();
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
    already_AddRefed<nsIDOMGetUserMediaSuccessCallback> aOnSuccess,
    already_AddRefed<nsIDOMGetUserMediaErrorCallback> aOnFailure,
    uint64_t aWindowID, GetUserMediaWindowListener *aWindowListener,
    SourceListener *aSourceListener, MediaEnginePrefs &aPrefs,
    const ipc::PrincipalInfo& aPrincipalInfo,
    bool aIsChrome,
    MediaManager::SourceSet* aSourceSet)
    : mConstraints(aConstraints)
    , mOnSuccess(aOnSuccess)
    , mOnFailure(aOnFailure)
    , mWindowID(aWindowID)
    , mWindowListener(aWindowListener)
    , mSourceListener(aSourceListener)
    , mPrefs(aPrefs)
    , mPrincipalInfo(aPrincipalInfo)
    , mIsChrome(aIsChrome)
    , mDeviceChosen(false)
    , mSourceSet(aSourceSet)
    , mManager(MediaManager::GetInstance())
  {}

  ~GetUserMediaTask() {
  }

  void
  Fail(const nsAString& aName,
       const nsAString& aMessage = EmptyString(),
       const nsAString& aConstraint = EmptyString()) {
    RefPtr<MediaMgrError> error = new MediaMgrError(aName, aMessage, aConstraint);
    auto errorRunnable = MakeRefPtr<ErrorCallbackRunnable<nsIDOMGetUserMediaSuccessCallback>>(
        Move(mOnSuccess), Move(mOnFailure), *error, mWindowID);
    // These should be empty now
    MOZ_ASSERT(!mOnSuccess);
    MOZ_ASSERT(!mOnFailure);

    NS_DispatchToMainThread(errorRunnable.forget());
    // Do after ErrorCallbackRunnable Run()s, as it checks active window list
    NS_DispatchToMainThread(NewRunnableMethod<RefPtr<SourceListener>>(
      mWindowListener, &GetUserMediaWindowListener::Remove, mSourceListener));
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mOnSuccess);
    MOZ_ASSERT(mOnFailure);
    MOZ_ASSERT(mDeviceChosen);

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
          nsTArray<RefPtr<AudioDevice>> audios;
          audios.AppendElement(mAudioDevice);
          badConstraint = MediaConstraintsHelper::SelectSettings(
              NormalizedConstraints(constraints), audios, mIsChrome);
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
          nsTArray<RefPtr<VideoDevice>> videos;
          videos.AppendElement(mVideoDevice);
          badConstraint = MediaConstraintsHelper::SelectSettings(
              NormalizedConstraints(constraints), videos, mIsChrome);
        }
        if (mAudioDevice) {
          mAudioDevice->Deallocate();
        }
      }
    }
    if (errorMsg) {
      LOG(("%s %" PRIu32, errorMsg, static_cast<uint32_t>(rv)));
      if (badConstraint) {
        Fail(NS_LITERAL_STRING("OverconstrainedError"),
             NS_LITERAL_STRING(""),
             NS_ConvertUTF8toUTF16(badConstraint));
      } else {
        Fail(NS_LITERAL_STRING("NotReadableError"),
             NS_ConvertUTF8toUTF16(errorMsg));
      }
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
                                       peerIdentity)));
    MOZ_ASSERT(!mOnSuccess);
    MOZ_ASSERT(!mOnFailure);
    return NS_OK;
  }

  nsresult
  Denied(const nsAString& aName,
         const nsAString& aMessage = EmptyString())
  {
    MOZ_ASSERT(mOnSuccess);
    MOZ_ASSERT(mOnFailure);

    // We add a disabled listener to the StreamListeners array until accepted
    // If this was the only active MediaStream, remove the window from the list.
    if (NS_IsMainThread()) {
      // This is safe since we're on main-thread, and the window can only
      // be invalidated from the main-thread (see OnNavigation)
      nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> onSuccess = mOnSuccess.forget();
      nsCOMPtr<nsIDOMGetUserMediaErrorCallback> onFailure = mOnFailure.forget();

      if (auto* window = nsGlobalWindow::GetInnerWindowWithId(mWindowID)) {
        RefPtr<MediaStreamError> error = new MediaStreamError(window->AsInner(),
                                                              aName, aMessage);
        onFailure->OnError(error);
      }
      // Should happen *after* error runs for consistency, but may not matter
      mWindowListener->Remove(mSourceListener);
    } else {
      // This will re-check the window being alive on main-thread
      // and remove the listener on MainThread as well
      Fail(aName, aMessage);
    }

    MOZ_ASSERT(!mOnSuccess);
    MOZ_ASSERT(!mOnFailure);

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
  SetAudioDevice(AudioDevice* aAudioDevice)
  {
    mAudioDevice = aAudioDevice;
    mDeviceChosen = true;
    return NS_OK;
  }

  nsresult
  SetVideoDevice(VideoDevice* aVideoDevice)
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

  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> mOnSuccess;
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> mOnFailure;
  uint64_t mWindowID;
  RefPtr<GetUserMediaWindowListener> mWindowListener;
  RefPtr<SourceListener> mSourceListener;
  RefPtr<AudioDevice> mAudioDevice;
  RefPtr<VideoDevice> mVideoDevice;
  MediaEnginePrefs mPrefs;
  ipc::PrincipalInfo mPrincipalInfo;
  bool mIsChrome;

  bool mDeviceChosen;
public:
  nsAutoPtr<MediaManager::SourceSet> mSourceSet;
private:
  RefPtr<MediaManager> mManager; // get ref to this when creating the runnable
};

#if defined(ANDROID) && !defined(MOZ_WIDGET_GONK)
class GetUserMediaRunnableWrapper : public Runnable
{
public:
  // This object must take ownership of task
  GetUserMediaRunnableWrapper(GetUserMediaTask* task) :
    mTask(task) {
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

already_AddRefed<MediaManager::PledgeSourceSet>
MediaManager::EnumerateRawDevices(uint64_t aWindowId,
                                  MediaSourceEnum aVideoType,
                                  MediaSourceEnum aAudioType,
                                  bool aFake)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aVideoType != MediaSourceEnum::Other ||
             aAudioType != MediaSourceEnum::Other);
  RefPtr<PledgeSourceSet> p = new PledgeSourceSet();
  uint32_t id = mOutstandingPledges.Append(*p);

  nsAdoptingCString audioLoopDev, videoLoopDev;
  if (!aFake) {
    // Fake stream not requested. The entire device stack is available.
    // Loop in loopback devices if they are set, and their respective type is
    // requested. This is currently used for automated media tests only.
    if (aVideoType == MediaSourceEnum::Camera) {
      videoLoopDev = Preferences::GetCString("media.video_loopback_dev");
    }
    if (aAudioType == MediaSourceEnum::Microphone) {
      audioLoopDev = Preferences::GetCString("media.audio_loopback_dev");
    }
  }

  MediaManager::PostTask(NewTaskFrom([id, aWindowId, audioLoopDev,
                                      videoLoopDev, aVideoType,
                                      aAudioType, aFake]() mutable {
    // Only enumerate what's asked for, and only fake cams and mics.
    bool hasVideo = aVideoType != MediaSourceEnum::Other;
    bool hasAudio = aAudioType != MediaSourceEnum::Other;
    bool fakeCams = aFake && aVideoType == MediaSourceEnum::Camera;
    bool fakeMics = aFake && aAudioType == MediaSourceEnum::Microphone;

    RefPtr<MediaEngine> fakeBackend, realBackend;
    if (fakeCams || fakeMics) {
      fakeBackend = new MediaEngineDefault();
    }
    if ((!fakeCams && hasVideo) || (!fakeMics && hasAudio)) {
      RefPtr<MediaManager> manager = MediaManager_GetInstance();
      realBackend = manager->GetBackend(aWindowId);
    }

    auto result = MakeUnique<SourceSet>();

    if (hasVideo) {
      nsTArray<RefPtr<VideoDevice>> videos;
      GetSources(fakeCams? fakeBackend : realBackend, aVideoType,
                 &MediaEngine::EnumerateVideoDevices, videos, videoLoopDev);
      for (auto& source : videos) {
        result->AppendElement(source);
      }
    }
    if (hasAudio) {
      nsTArray<RefPtr<AudioDevice>> audios;
      GetSources(fakeMics? fakeBackend : realBackend, aAudioType,
                 &MediaEngine::EnumerateAudioDevices, audios, audioLoopDev);
      for (auto& source : audios) {
        result->AppendElement(source);
      }
    }
    SourceSet* handoff = result.release();
    NS_DispatchToMainThread(NewRunnableFrom([id, handoff]() mutable {
      UniquePtr<SourceSet> result(handoff); // grab result
      RefPtr<MediaManager> mgr = MediaManager_GetInstance();
      if (!mgr) {
        return NS_OK;
      }
      RefPtr<PledgeSourceSet> p = mgr->mOutstandingPledges.Remove(id);
      if (p) {
        p->Resolve(result.release());
      }
      return NS_OK;
    }));
  }));
  return p.forget();
}

MediaManager::MediaManager()
  : mMediaThread(nullptr)
  , mBackend(nullptr) {
  mPrefs.mFreq         = 1000; // 1KHz test tone
  mPrefs.mWidth        = 0; // adaptive default
  mPrefs.mHeight       = 0; // adaptive default
  mPrefs.mFPS          = MediaEngine::DEFAULT_VIDEO_FPS;
  mPrefs.mMinFPS       = MediaEngine::DEFAULT_VIDEO_MIN_FPS;
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
  mPrefs.mPlayoutDelay = 0;
  mPrefs.mFullDuplex = false;
  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);
    if (branch) {
      GetPrefs(branch, nullptr);
    }
  }
  LOG(("%s: default prefs: %dx%d @%dfps (min %d), %dHz test tones, aec: %s,"
       "agc: %s, noise: %s, aec level: %d, agc level: %d, noise level: %d,"
       "playout delay: %d, %sfull_duplex, extended aec %s, delay_agnostic %s",
       __FUNCTION__, mPrefs.mWidth, mPrefs.mHeight,
       mPrefs.mFPS, mPrefs.mMinFPS, mPrefs.mFreq, mPrefs.mAecOn ? "on" : "off",
       mPrefs.mAgcOn ? "on": "off", mPrefs.mNoiseOn ? "on": "off", mPrefs.mAec,
       mPrefs.mAgc, mPrefs.mNoise, mPrefs.mPlayoutDelay, mPrefs.mFullDuplex ? "" : "not ",
       mPrefs.mExtendedFilter ? "on" : "off", mPrefs.mDelayAgnostic ? "on" : "off"));
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

    sSingleton->mMediaThread = new base::Thread("MediaManager");
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
      obs->AddObserver(sSingleton, "getUserMedia:privileged:allow", false);
      obs->AddObserver(sSingleton, "getUserMedia:response:allow", false);
      obs->AddObserver(sSingleton, "getUserMedia:response:deny", false);
      obs->AddObserver(sSingleton, "getUserMedia:revoke", false);
      obs->AddObserver(sSingleton, "phone-state-changed", false);
    }
    // else MediaManager won't work properly and will leak (see bug 837874)
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs) {
      prefs->AddObserver("media.navigator.video.default_width", sSingleton, false);
      prefs->AddObserver("media.navigator.video.default_height", sSingleton, false);
      prefs->AddObserver("media.navigator.video.default_fps", sSingleton, false);
      prefs->AddObserver("media.navigator.video.default_minfps", sSingleton, false);
      prefs->AddObserver("media.navigator.audio.fake_frequency", sSingleton, false);
      prefs->AddObserver("media.navigator.audio.full_duplex", sSingleton, false);
#ifdef MOZ_WEBRTC
      prefs->AddObserver("media.getusermedia.aec_enabled", sSingleton, false);
      prefs->AddObserver("media.getusermedia.aec", sSingleton, false);
      prefs->AddObserver("media.getusermedia.agc_enabled", sSingleton, false);
      prefs->AddObserver("media.getusermedia.agc", sSingleton, false);
      prefs->AddObserver("media.getusermedia.noise_enabled", sSingleton, false);
      prefs->AddObserver("media.getusermedia.noise", sSingleton, false);
      prefs->AddObserver("media.getusermedia.playout_delay", sSingleton, false);
      prefs->AddObserver("media.ondevicechange.fakeDeviceChangeEvent.enabled", sSingleton, false);
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
#ifdef MOZ_B2G
    // Init MediaPermissionManager before sending out any permission requests.
    (void) MediaPermissionManager::GetInstance();
#endif //MOZ_B2G
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
  if (sInShutdown) {
    // Can't safely delete task here since it may have items with specific
    // thread-release requirements.
    // XXXkhuey well then who is supposed to delete it?! We don't signal
    // that we failed ...
    MOZ_CRASH();
    return;
  }
  NS_ASSERTION(Get(), "MediaManager singleton?");
  NS_ASSERTION(Get()->mMediaThread, "No thread yet");
  Get()->mMediaThread->message_loop()->PostTask(Move(task));
}

/* static */ nsresult
MediaManager::NotifyRecordingStatusChange(nsPIDOMWindowInner* aWindow,
                                          const nsString& aMsg)
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
                       aMsg.get());

  return NS_OK;
}

int MediaManager::AddDeviceChangeCallback(DeviceChangeCallback* aCallback)
{
  bool fakeDeviceChangeEventOn = mPrefs.mFakeDeviceChangeEventOn;
  MediaManager::PostTask(NewTaskFrom([fakeDeviceChangeEventOn]() {
    RefPtr<MediaManager> manager = MediaManager_GetInstance();
    manager->GetBackend(0)->AddDeviceChangeCallback(manager);
    if (fakeDeviceChangeEventOn)
      manager->GetBackend(0)->SetFakeDeviceChangeEvents();
  }));

  return DeviceChangeCallback::AddDeviceChangeCallback(aCallback);
}

void MediaManager::OnDeviceChange() {
  RefPtr<MediaManager> self(this);
  NS_DispatchToMainThread(media::NewRunnableFrom([self,this]() mutable {
    MOZ_ASSERT(NS_IsMainThread());
    DeviceChangeCallback::OnDeviceChange();
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
  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> onSuccess(aOnSuccess);
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> onFailure(aOnFailure);
  uint64_t windowID = aWindow->WindowID();

  MediaStreamConstraints c(aConstraintsPassedIn); // use a modifiable copy

  // Do all the validation we can while we're sync (to return an
  // already-rejected promise on failure).

  if (!IsOn(c.mVideo) && !IsOn(c.mAudio)) {
    RefPtr<MediaStreamError> error =
        new MediaStreamError(aWindow,
                             NS_LITERAL_STRING("TypeError"),
                             NS_LITERAL_STRING("audio and/or video is required"));
    onFailure->OnError(error);
    return NS_OK;
  }
  if (sInShutdown) {
    RefPtr<MediaStreamError> error =
        new MediaStreamError(aWindow,
                             NS_LITERAL_STRING("AbortError"),
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
    nsGlobalWindow::Cast(aWindow)->GetPrincipal();
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
            (!privileged && !HostIsHttps(*docURI))) {
          RefPtr<MediaStreamError> error =
              new MediaStreamError(aWindow,
                                   NS_LITERAL_STRING("NotAllowedError"));
          onFailure->OnError(error);
          return NS_OK;
        }
        break;

      case MediaSourceEnum::Microphone:
      case MediaSourceEnum::Other:
      default: {
        RefPtr<MediaStreamError> error =
            new MediaStreamError(aWindow,
                                 NS_LITERAL_STRING("OverconstrainedError"),
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
  }

  if (c.mAudio.IsMediaTrackConstraints()) {
    auto& ac = c.mAudio.GetAsMediaTrackConstraints();
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
                                 NS_LITERAL_STRING("NotAllowedError"));
          onFailure->OnError(error);
          return NS_OK;
        }
        break;

      case MediaSourceEnum::Other:
      default: {
        RefPtr<MediaStreamError> error =
            new MediaStreamError(aWindow,
                                 NS_LITERAL_STRING("OverconstrainedError"),
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
      rv = permManager->TestExactPermissionFromPrincipal(
        principal, "microphone", &audioPerm);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    uint32_t videoPerm = nsIPermissionManager::UNKNOWN_ACTION;
    if (IsOn(c.mVideo)) {
      rv = permManager->TestExactPermissionFromPrincipal(
        principal, videoType == MediaSourceEnum::Camera ? "camera" : "screen",
        &videoPerm);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if ((!IsOn(c.mAudio) && !IsOn(c.mVideo)) ||
        (IsOn(c.mAudio) && audioPerm == nsIPermissionManager::DENY_ACTION) ||
        (IsOn(c.mVideo) && videoPerm == nsIPermissionManager::DENY_ACTION)) {
      RefPtr<MediaStreamError> error =
          new MediaStreamError(aWindow, NS_LITERAL_STRING("NotAllowedError"));
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

  bool fake = c.mFake.WasPassed()? c.mFake.Value() :
      Preferences::GetBool("media.navigator.streams.fake");

  bool askPermission =
      (!privileged || Preferences::GetBool("media.navigator.permission.force")) &&
      (!fake || Preferences::GetBool("media.navigator.permission.fake"));

  RefPtr<PledgeSourceSet> p = EnumerateDevicesImpl(windowID, videoType,
                                                   audioType, fake);
  RefPtr<MediaManager> self = this;
  p->Then([self, onSuccess, onFailure, windowID, c, windowListener,
           sourceListener, askPermission, prefs, isHTTPS, callID, principalInfo,
           isChrome](SourceSet*& aDevices) mutable {
    // grab result
    auto devices = MakeRefPtr<Refcountable<UniquePtr<SourceSet>>>(aDevices);

    // Ensure that our windowID is still good.
    if (!nsGlobalWindow::GetInnerWindowWithId(windowID)) {
      return;
    }

    // Apply any constraints. This modifies the passed-in list.
    RefPtr<PledgeChar> p2 = self->SelectSettings(c, isChrome, devices);

    p2->Then([self, onSuccess, onFailure, windowID, c,
              windowListener, sourceListener, askPermission, prefs, isHTTPS,
              callID, principalInfo, isChrome, devices
             ](const char*& badConstraint) mutable {

      // Ensure that the captured 'this' pointer and our windowID are still good.
      auto* globalWindow = nsGlobalWindow::GetInnerWindowWithId(windowID);
      RefPtr<nsPIDOMWindowInner> window = globalWindow ? globalWindow->AsInner()
                                                       : nullptr;
      if (!MediaManager::Exists() || !window) {
        return;
      }

      if (badConstraint) {
        nsString constraint;
        constraint.AssignASCII(badConstraint);
        RefPtr<MediaStreamError> error =
            new MediaStreamError(window,
                                 NS_LITERAL_STRING("OverconstrainedError"),
                                 NS_LITERAL_STRING(""),
                                 constraint);
        onFailure->OnError(error);
        return;
      }
      if (!(*devices)->Length()) {
        RefPtr<MediaStreamError> error =
            new MediaStreamError(window, NS_LITERAL_STRING("NotFoundError"));
        onFailure->OnError(error);
        return;
      }

      nsCOMPtr<nsIMutableArray> devicesCopy = nsArray::Create(); // before we give up devices below
      if (!askPermission) {
        for (auto& device : **devices) {
          nsresult rv = devicesCopy->AppendElement(device, /*weak =*/ false);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return;
          }
        }
      }

      // Pass callbacks and listeners along to GetUserMediaTask.
      RefPtr<GetUserMediaTask> task (new GetUserMediaTask(c,
                                                          onSuccess.forget(),
                                                          onFailure.forget(),
                                                          windowID,
                                                          windowListener,
                                                          sourceListener,
                                                          prefs,
                                                          principalInfo,
                                                          isChrome,
                                                          devices->release()));
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
            new GetUserMediaRequest(window, callID, c, isHTTPS);
        obs->NotifyObservers(req, "getUserMedia:request", nullptr);
      }

#ifdef MOZ_WEBRTC
      EnableWebRtcLog();
#endif
    }, [onFailure](MediaStreamError*& reason) mutable {
      onFailure->OnError(reason);
    });
  }, [onFailure](MediaStreamError*& reason) mutable {
    onFailure->OnError(reason);
  });
  return NS_OK;
}

/* static */ void
MediaManager::AnonymizeDevices(SourceSet& aDevices, const nsACString& aOriginKey)
{
  if (!aOriginKey.IsEmpty()) {
    for (auto& device : aDevices) {
      nsString id;
      device->GetId(id);
      device->SetRawId(id);
      AnonymizeId(id, aOriginKey);
      device->SetId(id);
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
MediaManager::ToJSArray(SourceSet& aDevices)
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

already_AddRefed<MediaManager::PledgeSourceSet>
MediaManager::EnumerateDevicesImpl(uint64_t aWindowId,
                                   MediaSourceEnum aVideoType,
                                   MediaSourceEnum aAudioType,
                                   bool aFake)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsPIDOMWindowInner* window =
    nsGlobalWindow::GetInnerWindowWithId(aWindowId)->AsInner();

  // This function returns a pledge, a promise-like object with the future result
  RefPtr<PledgeSourceSet> pledge = new PledgeSourceSet();
  uint32_t id = mOutstandingPledges.Append(*pledge);

  // To get a device list anonymized for a particular origin, we must:
  // 1. Get an origin-key (for either regular or private browsing)
  // 2. Get the raw devices list
  // 3. Anonymize the raw list with the origin-key.

  nsCOMPtr<nsIPrincipal> principal =
    nsGlobalWindow::Cast(window)->GetPrincipal();
  MOZ_ASSERT(principal);

  ipc::PrincipalInfo principalInfo;
  nsresult rv = PrincipalToPrincipalInfo(principal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RefPtr<PledgeSourceSet> p = new PledgeSourceSet();
    RefPtr<MediaStreamError> error =
      new MediaStreamError(window, NS_LITERAL_STRING("NotAllowedError"));
    p->Reject(error);
    return p.forget();
  }

  bool persist = IsActivelyCapturingOrHasAPermission(aWindowId);

  // GetPrincipalKey is an async API that returns a pledge (a promise-like
  // pattern). We use .Then() to pass in a lambda to run back on this same
  // thread later once GetPrincipalKey resolves. Needed variables are "captured"
  // (passed by value) safely into the lambda.

  RefPtr<Pledge<nsCString>> p = media::GetPrincipalKey(principalInfo, persist);
  p->Then([id, aWindowId, aVideoType, aAudioType,
           aFake](const nsCString& aOriginKey) mutable {
    MOZ_ASSERT(NS_IsMainThread());
    RefPtr<MediaManager> mgr = MediaManager_GetInstance();

    RefPtr<PledgeSourceSet> p = mgr->EnumerateRawDevices(aWindowId, aVideoType,
                                                         aAudioType, aFake);
    p->Then([id, aWindowId, aOriginKey](SourceSet*& aDevices) mutable {
      UniquePtr<SourceSet> devices(aDevices); // secondary result

      // Only run if window is still on our active list.
      RefPtr<MediaManager> mgr = MediaManager_GetInstance();
      if (!mgr) {
        return NS_OK;
      }
      RefPtr<PledgeSourceSet> p = mgr->mOutstandingPledges.Remove(id);
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
                               nsIDOMGetUserMediaErrorCallback* aOnFailure)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(!sInShutdown, NS_ERROR_FAILURE);
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

  bool fake = Preferences::GetBool("media.navigator.streams.fake");

  RefPtr<PledgeSourceSet> p = EnumerateDevicesImpl(windowId,
                                                   MediaSourceEnum::Camera,
                                                   MediaSourceEnum::Microphone,
                                                   fake);
  p->Then([onSuccess, windowListener, sourceListener](SourceSet*& aDevices) mutable {
    UniquePtr<SourceSet> devices(aDevices); // grab result
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
        nsCOMPtr<nsIWritableVariant> array = MediaManager_ToJSArray(*task->mSourceSet);
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
    MOZ_RELEASE_ASSERT(!sInShutdown);  // we should never create a new backend in shutdown
#if defined(MOZ_WEBRTC)
    mBackend = new MediaEngineWebRTC(mPrefs);
#else
    mBackend = new MediaEngineDefault();
#endif
  }
  return mBackend;
}

static void
StopSharingCallback(MediaManager *aThis,
                    uint64_t aWindowID,
                    GetUserMediaWindowListener *aListener,
                    void *aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Grab a strong ref since RemoveAll() might destroy the listener mid-way
  // when clearing the mActiveWindows reference.
  RefPtr<GetUserMediaWindowListener> listener(aListener);
  if (!listener) {
    return;
  }

  listener->Stop();
  listener->RemoveAll();
  MOZ_ASSERT(!aThis->GetWindowListener(aWindowID));
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
  auto* window = nsGlobalWindow::GetInnerWindowWithId(aWindowID);
  if (window) {
    IterateWindowListeners(window->AsInner(), StopSharingCallback, nullptr);
  } else {
    RemoveWindowID(aWindowID);
  }
  MOZ_ASSERT(!GetWindowListener(aWindowID));

  RemoveMediaDevicesCallback(aWindowID);
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
  auto* window = nsGlobalWindow::GetInnerWindowWithId(aWindowId);
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
  GetPref(aBranch, "media.navigator.video.default_minfps", aData, &mPrefs.mMinFPS);
  GetPref(aBranch, "media.navigator.audio.fake_frequency", aData, &mPrefs.mFreq);
#ifdef MOZ_WEBRTC
  GetPrefBool(aBranch, "media.getusermedia.aec_enabled", aData, &mPrefs.mAecOn);
  GetPrefBool(aBranch, "media.getusermedia.agc_enabled", aData, &mPrefs.mAgcOn);
  GetPrefBool(aBranch, "media.getusermedia.noise_enabled", aData, &mPrefs.mNoiseOn);
  GetPref(aBranch, "media.getusermedia.aec", aData, &mPrefs.mAec);
  GetPref(aBranch, "media.getusermedia.agc", aData, &mPrefs.mAgc);
  GetPref(aBranch, "media.getusermedia.noise", aData, &mPrefs.mNoise);
  GetPref(aBranch, "media.getusermedia.playout_delay", aData, &mPrefs.mPlayoutDelay);
  GetPrefBool(aBranch, "media.getusermedia.aec_extended_filter", aData, &mPrefs.mExtendedFilter);
  GetPrefBool(aBranch, "media.getusermedia.aec_aec_delay_agnostic", aData, &mPrefs.mDelayAgnostic);
  GetPrefBool(aBranch, "media.ondevicechange.fakeDeviceChangeEvent.enabled", aData, &mPrefs.mFakeDeviceChangeEventOn);
#endif
  GetPrefBool(aBranch, "media.navigator.audio.full_duplex", aData, &mPrefs.mFullDuplex);
}

void
MediaManager::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (sInShutdown) {
    return;
  }
  sInShutdown = true;

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
    prefs->RemoveObserver("media.navigator.video.default_minfps", this);
    prefs->RemoveObserver("media.navigator.audio.fake_frequency", this);
#ifdef MOZ_WEBRTC
    prefs->RemoveObserver("media.getusermedia.aec_enabled", this);
    prefs->RemoveObserver("media.getusermedia.aec", this);
    prefs->RemoveObserver("media.getusermedia.agc_enabled", this);
    prefs->RemoveObserver("media.getusermedia.agc", this);
    prefs->RemoveObserver("media.getusermedia.noise_enabled", this);
    prefs->RemoveObserver("media.getusermedia.noise", this);
    prefs->RemoveObserver("media.getusermedia.playout_delay", this);
    prefs->RemoveObserver("media.ondevicechange.fakeDeviceChangeEvent.enabled", this);
#endif
    prefs->RemoveObserver("media.navigator.audio.full_duplex", this);
  }

  // Close off any remaining active windows.
  GetActiveWindows()->Clear();
  mActiveCallbacks.Clear();
  mCallIds.Clear();
#ifdef MOZ_WEBRTC
  StopWebRtcLog();
#endif

  // Because mMediaThread is not an nsThread, we must dispatch to it so it can
  // clean up BackgroundChild. Continue stopping thread once this is done.

  class ShutdownTask : public Runnable
  {
  public:
    ShutdownTask(MediaManager* aManager,
                 already_AddRefed<Runnable> aReply)
      : mManager(aManager)
      , mReply(aReply) {}
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
  // Don't use MediaManager::PostTask() because we're sInShutdown=true here!
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

nsresult
MediaManager::Observe(nsISupports* aSubject, const char* aTopic,
  const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> branch( do_QueryInterface(aSubject) );
    if (branch) {
      GetPrefs(branch,NS_ConvertUTF16toUTF8(aData).get());
      LOG(("%s: %dx%d @%dfps (min %d)", __FUNCTION__,
           mPrefs.mWidth, mPrefs.mHeight, mPrefs.mFPS, mPrefs.mMinFPS));
    }
  } else if (!strcmp(aTopic, "last-pb-context-exited")) {
    // Clear memory of private-browsing-specific deviceIds. Fire and forget.
    media::SanitizeOriginKeys(0, true);
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
        if (device) {
          nsString type;
          device->GetType(type);
          if (type.EqualsLiteral("video")) {
            if (!videoFound) {
              task->SetVideoDevice(static_cast<VideoDevice*>(device.get()));
              videoFound = true;
            }
          } else if (type.EqualsLiteral("audio")) {
            if (!audioFound) {
              task->SetAudioDevice(static_cast<AudioDevice*>(device.get()));
              audioFound = true;
            }
          } else {
            NS_WARNING("Unknown device type in getUserMedia");
          }
        }
      }
      bool needVideo = IsOn(task->GetConstraints().mVideo);
      bool needAudio = IsOn(task->GetConstraints().mAudio);
      MOZ_ASSERT(needVideo || needAudio);

      if ((needVideo && !videoFound) || (needAudio && !audioFound)) {
        task->Denied(NS_LITERAL_STRING("NotAllowedError"));
        return NS_OK;
      }
    }

    if (sInShutdown) {
      return task->Denied(NS_LITERAL_STRING("In shutdown"));
    }
    // Reuse the same thread to save memory.
    MediaManager::PostTask(task.forget());
    return NS_OK;

  } else if (!strcmp(aTopic, "getUserMedia:response:deny")) {
    nsString errorMessage(NS_LITERAL_STRING("NotAllowedError"));

    if (aSubject) {
      nsCOMPtr<nsISupportsString> msg(do_QueryInterface(aSubject));
      MOZ_ASSERT(msg);
      msg->GetData(errorMessage);
      if (errorMessage.IsEmpty())
        errorMessage.AssignLiteral(u"InternalError");
    }

    nsString key(aData);
    RefPtr<GetUserMediaTask> task;
    mActiveCallbacks.Remove(key, getter_AddRefs(task));
    if (task) {
      task->Denied(errorMessage);
      nsTArray<nsString>* array;
      if (!mCallIds.Get(task->GetWindowID(), &array)) {
        return NS_OK;
      }
      array->RemoveElement(key);
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
#ifdef MOZ_WIDGET_GONK
  else if (!strcmp(aTopic, "phone-state-changed")) {
    nsString state(aData);
    nsresult rv;
    uint32_t phoneState = state.ToInteger(&rv);

    if (NS_SUCCEEDED(rv) && phoneState == nsIAudioManager::PHONE_STATE_IN_CALL) {
      StopMediaStreams();
    }
    return NS_OK;
  }
#endif

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
      nsGlobalWindow::GetInnerWindowWithId(id)->AsInner();
    MOZ_ASSERT(window);
    // XXXkhuey ...
    if (!window) {
      continue;
    }

    if (winListener->CapturingVideo() || winListener->CapturingAudio() ||
        winListener->CapturingScreen() || winListener->CapturingWindow() ||
        winListener->CapturingApplication()) {
      array->AppendElement(window, /*weak =*/ false);
    }
  }

  array.forget(aArray);
  return NS_OK;
}

// XXX flags might be better...
struct CaptureWindowStateData {
  bool *mVideo;
  bool *mAudio;
  bool *mScreenShare;
  bool *mWindowShare;
  bool *mAppShare;
  bool *mBrowserShare;
};

static void
CaptureWindowStateCallback(MediaManager *aThis,
                           uint64_t aWindowID,
                           GetUserMediaWindowListener  *aListener,
                           void *aData)
{
  struct CaptureWindowStateData *data = (struct CaptureWindowStateData *) aData;

  if (!aListener) {
    return;
  }

  if (aListener->CapturingVideo()) {
    *data->mVideo = true;
  }
  if (aListener->CapturingAudio()) {
    *data->mAudio = true;
  }
  if (aListener->CapturingScreen()) {
    *data->mScreenShare = true;
  }
  if (aListener->CapturingWindow()) {
    *data->mWindowShare = true;
  }
  if (aListener->CapturingApplication()) {
    *data->mAppShare = true;
  }
  if (aListener->CapturingBrowser()) {
    *data->mBrowserShare = true;
  }
}

NS_IMETHODIMP
MediaManager::MediaCaptureWindowState(nsIDOMWindow* aWindow, bool* aVideo,
                                      bool* aAudio, bool *aScreenShare,
                                      bool* aWindowShare, bool *aAppShare,
                                      bool *aBrowserShare)
{
  MOZ_ASSERT(NS_IsMainThread());
  struct CaptureWindowStateData data;
  data.mVideo = aVideo;
  data.mAudio = aAudio;
  data.mScreenShare = aScreenShare;
  data.mWindowShare = aWindowShare;
  data.mAppShare = aAppShare;
  data.mBrowserShare = aBrowserShare;

  *aVideo = false;
  *aAudio = false;
  *aScreenShare = false;
  *aWindowShare = false;
  *aAppShare = false;
  *aBrowserShare = false;

  nsCOMPtr<nsPIDOMWindowInner> piWin = do_QueryInterface(aWindow);
  if (piWin) {
    IterateWindowListeners(piWin, CaptureWindowStateCallback, &data);
  }
#ifdef DEBUG
  LOG(("%s: window %" PRIu64 " capturing %s %s %s %s %s %s", __FUNCTION__, piWin ? piWin->WindowID() : -1,
       *aVideo ? "video" : "", *aAudio ? "audio" : "",
       *aScreenShare ? "screenshare" : "",  *aWindowShare ? "windowshare" : "",
       *aAppShare ? "appshare" : "", *aBrowserShare ? "browsershare" : ""));
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

static void
StopScreensharingCallback(MediaManager *aThis,
                          uint64_t aWindowID,
                          GetUserMediaWindowListener *aListener,
                          void *aData)
{
  if (!aListener) {
    return;
  }

  aListener->StopSharing();
}

void
MediaManager::StopScreensharing(uint64_t aWindowID)
{
  // We need to stop window/screensharing for all streams in all innerwindows that
  // correspond to that outerwindow.

  auto* window = nsGlobalWindow::GetInnerWindowWithId(aWindowID);
  if (!window) {
    return;
  }
  IterateWindowListeners(window->AsInner(), &StopScreensharingCallback, nullptr);
}

// lets us do all sorts of things to the listeners
void
MediaManager::IterateWindowListeners(nsPIDOMWindowInner* aWindow,
                                     WindowListenerCallback aCallback,
                                     void *aData)
{
  // Iterate the docshell tree to find all the child windows, and for each
  // invoke the callback
  if (aWindow) {
    {
      uint64_t windowID = aWindow->WindowID();
      GetUserMediaWindowListener* listener = GetWindowListener(windowID);
      (*aCallback)(this, windowID, listener, aData);
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
          IterateWindowListeners(winOuter->GetCurrentInnerWindow(),
                                 aCallback, aData);
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

  auto* window = nsGlobalWindow::GetInnerWindowWithId(aWindowId);
  if (NS_WARN_IF(!window)) {
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
    auto* principal = window->GetExtantDoc()->NodePrincipal();
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
  : mActivated(false)
  , mStopped(false)
  , mFinished(false)
  , mRemoved(false)
  , mAudioStopped(false)
  , mVideoStopped(false)
  , mMainThreadCheck(nullptr)
  , mPrincipalHandle(PRINCIPAL_HANDLE_NONE)
  , mWindowListener(nullptr)
{}

void
SourceListener::Register(GetUserMediaWindowListener* aListener)
{
  LOG(("SourceListener %p registering with window listener %p", this, aListener));

  if (mWindowListener) {
    MOZ_ASSERT(false, "Already registered");
    return;
  }
  if (mActivated) {
    MOZ_ASSERT(false, "Already activated");
    return;
  }
  if (!aListener) {
    MOZ_ASSERT(false, "No listener");
    return;
  }
  mPrincipalHandle = aListener->GetPrincipalHandle();
  mWindowListener = aListener;
}

void
SourceListener::Activate(SourceMediaStream* aStream,
                         AudioDevice* aAudioDevice,
                         VideoDevice* aVideoDevice)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

  LOG(("SourceListener %p activating audio=%p video=%p", this, aAudioDevice, aVideoDevice));

  if (mActivated) {
    MOZ_ASSERT(false, "Already activated");
    return;
  }

  mActivated = true;
  mMainThreadCheck = PR_GetCurrentThread();
  mStream = aStream;
  mAudioDevice = aAudioDevice;
  mVideoDevice = aVideoDevice;
  mStream->AddListener(this);
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

  if (mAudioDevice && !mAudioStopped) {
    StopTrack(kAudioTrack);
  }
  if (mVideoDevice && !mVideoStopped) {
    StopTrack(kVideoTrack);
  }
  RefPtr<SourceMediaStream> source = GetSourceStream();
  MediaManager::PostTask(NewTaskFrom([source]() {
    MOZ_ASSERT(MediaManager::IsInMediaThread());
    source->EndAllTrackAndFinish();
  }));
}

void
SourceListener::Remove()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mStream || mRemoved) {
    return;
  }

  LOG(("SourceListener %p removed on purpose, mFinished = %d", this, (int) mFinished));
  mRemoved = true; // RemoveListener is async, avoid races
  mWindowListener = nullptr;

  // If it's destroyed, don't call - listener will be removed and we'll be notified!
  if (!mStream->IsDestroyed()) {
    mStream->RemoveListener(this);
  }
}

void
SourceListener::StopTrack(TrackID aTrackID)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

  RefPtr<MediaDevice> device;
  RefPtr<SourceMediaStream> source;

  switch (aTrackID) {
    case kAudioTrack: {
      LOG(("SourceListener %p stopping audio track %d", this, aTrackID));
      if (!mAudioDevice) {
        NS_ASSERTION(false, "Can't stop audio. No device.");
        return;
      }
      if (mAudioStopped) {
        // Audio already stopped
        return;
      }
      device = mAudioDevice;
      source = GetSourceStream();
      mAudioStopped = true;
      break;
    }
    case kVideoTrack: {
      LOG(("SourceListener %p stopping video track %d", this, aTrackID));
      if (!mVideoDevice) {
        NS_ASSERTION(false, "Can't stop video. No device.");
        return;
      }
      if (mVideoStopped) {
        // Video already stopped
        return;
      }
      device = mVideoDevice;
      source = GetSourceStream();
      mVideoStopped = true;
      break;
    }
    default: {
      MOZ_ASSERT(false, "Unknown track id");
      return;
    }
  }

  MediaManager::PostTask(NewTaskFrom([device, source, aTrackID]() {
    device->GetSource()->Stop(source, aTrackID);
    device->Deallocate();
  }));

  if ((!mAudioDevice || mAudioStopped) &&
      (!mVideoDevice || mVideoStopped)) {
    LOG(("SourceListener %p this was the last track stopped", this));
    Stop();
  }

  if (!mWindowListener) {
    MOZ_ASSERT(false, "Should still have window listener");
    return;
  }
  mWindowListener->NotifySourceTrackStopped();
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

  if (mVideoDevice &&
      (mVideoDevice->GetMediaSource() == MediaSourceEnum::Screen ||
       mVideoDevice->GetMediaSource() == MediaSourceEnum::Application ||
       mVideoDevice->GetMediaSource() == MediaSourceEnum::Window)) {
    // We want to stop the whole stream if there's no audio;
    // just the video track if we have both.
    // StopTrack figures this out for us.
    StopTrack(kVideoTrack);
  }
  if (mAudioDevice &&
      mAudioDevice->GetMediaSource() == MediaSourceEnum::AudioCapture) {
    uint64_t windowID = mWindowListener->WindowID();
    nsCOMPtr<nsPIDOMWindowInner> window = nsGlobalWindow::GetInnerWindowWithId(windowID)->AsInner();
    MOZ_RELEASE_ASSERT(window);
    window->SetAudioCapture(false);
    MediaStreamGraph* graph =
      MediaStreamGraph::GetInstance(MediaStreamGraph::AUDIO_THREAD_DRIVER,
                                    dom::AudioChannel::Normal);
    graph->UnregisterCaptureStreamForWindow(windowID);
    mStream->Destroy();
  }
}

SourceMediaStream*
SourceListener::GetSourceStream()
{
  NS_ASSERTION(mStream,"Getting stream from never-activated SourceListener");
  if (!mStream) {
    return nullptr;
  }
  return mStream->AsSourceStream();
}

void
SourceListener::GetSettings(dom::MediaTrackSettings& aOutSettings, TrackID aTrackID)
{
  switch (aTrackID) {
    case kVideoTrack: {
      if (mVideoDevice) {
        mVideoDevice->GetSource()->GetSettings(aOutSettings);
      }
      break;
    }
    case kAudioTrack: {
      if (mAudioDevice) {
        mAudioDevice->GetSource()->GetSettings(aOutSettings);
      }
      break;
    }
    default: {
      MOZ_ASSERT(false, "Unknown track id");
    }
  }
}

// Proxy NotifyPull() to sources
void
SourceListener::NotifyPull(MediaStreamGraph* aGraph,
                           StreamTime aDesiredTime)
{
  // Currently audio sources ignore NotifyPull, but they could
  // watch it especially for fake audio.
  if (mAudioDevice) {
    mAudioDevice->GetSource()->NotifyPull(aGraph, mStream, kAudioTrack,
                                          aDesiredTime, mPrincipalHandle);
  }
  if (mVideoDevice) {
    mVideoDevice->GetSource()->NotifyPull(aGraph, mStream, kVideoTrack,
                                          aDesiredTime, mPrincipalHandle);
  }
}

void
SourceListener::NotifyEvent(MediaStreamGraph* aGraph,
                            MediaStreamGraphEvent aEvent)
{
  nsresult rv;
  nsCOMPtr<nsIThread> thread;

  switch (aEvent) {
    case MediaStreamGraphEvent::EVENT_FINISHED:
      rv = NS_GetMainThread(getter_AddRefs(thread));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        NS_ASSERTION(false, "Mainthread not available; running on current thread");
        // Ensure this really *was* MainThread (NS_GetCurrentThread won't work)
        MOZ_RELEASE_ASSERT(mMainThreadCheck == PR_GetCurrentThread());
        NotifyFinished();
        return;
      }
      thread->Dispatch(NewRunnableMethod(this, &SourceListener::NotifyFinished),
                       NS_DISPATCH_NORMAL);
      break;
    case MediaStreamGraphEvent::EVENT_REMOVED:
      rv = NS_GetMainThread(getter_AddRefs(thread));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        NS_ASSERTION(false, "Mainthread not available; running on current thread");
        // Ensure this really *was* MainThread (NS_GetCurrentThread won't work)
        MOZ_RELEASE_ASSERT(mMainThreadCheck == PR_GetCurrentThread());
        NotifyRemoved();
        return;
      }
      thread->Dispatch(NewRunnableMethod(this, &SourceListener::NotifyRemoved),
                       NS_DISPATCH_NORMAL);
      break;
    case MediaStreamGraphEvent::EVENT_HAS_DIRECT_LISTENERS:
      NotifyDirectListeners(aGraph, true);
      break;
    case MediaStreamGraphEvent::EVENT_HAS_NO_DIRECT_LISTENERS:
      NotifyDirectListeners(aGraph, false);
      break;
    default:
      break;
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

  if (!mFinished) {
    NotifyFinished();
  }

  mWindowListener = nullptr;
}

void
SourceListener::NotifyDirectListeners(MediaStreamGraph* aGraph,
                                      bool aHasListeners)
{
  if (!mVideoDevice) {
    return;
  }

  auto& videoDevice = mVideoDevice;
  MediaManager::PostTask(NewTaskFrom([videoDevice, aHasListeners]() {
    videoDevice->GetSource()->SetDirectListeners(aHasListeners);
    return NS_OK;
  }));
}

bool
SourceListener::CapturingVideo() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mActivated && mVideoDevice && !mVideoStopped &&
         !mVideoDevice->GetSource()->IsAvailable() &&
         mVideoDevice->GetMediaSource() == dom::MediaSourceEnum::Camera &&
         (!mVideoDevice->GetSource()->IsFake() ||
          Preferences::GetBool("media.navigator.permission.fake"));
}

bool
SourceListener::CapturingAudio() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mActivated && mAudioDevice && !mAudioStopped &&
         !mAudioDevice->GetSource()->IsAvailable() &&
         (!mAudioDevice->GetSource()->IsFake() ||
          Preferences::GetBool("media.navigator.permission.fake"));
}

bool
SourceListener::CapturingScreen() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mActivated && mVideoDevice && !mVideoStopped &&
         !mVideoDevice->GetSource()->IsAvailable() &&
         mVideoDevice->GetMediaSource() == dom::MediaSourceEnum::Screen;
}

bool
SourceListener::CapturingWindow() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mActivated && mVideoDevice && !mVideoStopped &&
         !mVideoDevice->GetSource()->IsAvailable() &&
         mVideoDevice->GetMediaSource() == dom::MediaSourceEnum::Window;
}

bool
SourceListener::CapturingApplication() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mActivated && mVideoDevice && !mVideoStopped &&
         !mVideoDevice->GetSource()->IsAvailable() &&
         mVideoDevice->GetMediaSource() == dom::MediaSourceEnum::Application;
}

bool
SourceListener::CapturingBrowser() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mActivated && mVideoDevice && !mVideoStopped &&
         !mVideoDevice->GetSource()->IsAvailable() &&
         mVideoDevice->GetMediaSource() == dom::MediaSourceEnum::Browser;
}

already_AddRefed<PledgeVoid>
SourceListener::ApplyConstraintsToTrack(nsPIDOMWindowInner* aWindow,
                                        TrackID aTrackID,
                                        const dom::MediaTrackConstraints& aConstraints,
                                        dom::CallerType aCallerType)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<PledgeVoid> p = new PledgeVoid();

  // XXX to support multiple tracks of a type in a stream, this should key off
  // the TrackID and not just the type
  RefPtr<AudioDevice> audioDevice =
    aTrackID == kAudioTrack ? mAudioDevice.get() : nullptr;
  RefPtr<VideoDevice> videoDevice =
    aTrackID == kVideoTrack ? mVideoDevice.get() : nullptr;

  if (mStopped || (!audioDevice && !videoDevice))
  {
    LOG(("gUM track %d applyConstraints, but we don't have type %s",
         aTrackID, aTrackID == kAudioTrack ? "audio" : "video"));
    p->Resolve(false);
    return p.forget();
  }

  RefPtr<MediaManager> mgr = MediaManager::GetInstance();
  uint32_t id = mgr->mOutstandingVoidPledges.Append(*p);
  uint64_t windowId = aWindow->WindowID();
  bool isChrome = (aCallerType == dom::CallerType::System);

  MediaManager::PostTask(NewTaskFrom([id, windowId,
                                      audioDevice, videoDevice,
                                      aConstraints, isChrome]() mutable {
    MOZ_ASSERT(MediaManager::IsInMediaThread());
    RefPtr<MediaManager> mgr = MediaManager::GetInstance();
    const char* badConstraint = nullptr;
    nsresult rv = NS_OK;

    if (audioDevice) {
      rv = audioDevice->Restart(aConstraints, mgr->mPrefs, &badConstraint);
      if (rv == NS_ERROR_NOT_AVAILABLE && !badConstraint) {
        nsTArray<RefPtr<AudioDevice>> audios;
        audios.AppendElement(audioDevice);
        badConstraint = MediaConstraintsHelper::SelectSettings(
            NormalizedConstraints(aConstraints), audios, isChrome);
      }
    } else {
      rv = videoDevice->Restart(aConstraints, mgr->mPrefs, &badConstraint);
      if (rv == NS_ERROR_NOT_AVAILABLE && !badConstraint) {
        nsTArray<RefPtr<VideoDevice>> videos;
        videos.AppendElement(videoDevice);
        badConstraint = MediaConstraintsHelper::SelectSettings(
            NormalizedConstraints(aConstraints), videos, isChrome);
      }
    }
    NS_DispatchToMainThread(NewRunnableFrom([id, windowId, rv,
                                             badConstraint]() mutable {
      MOZ_ASSERT(NS_IsMainThread());
      RefPtr<MediaManager> mgr = MediaManager_GetInstance();
      if (!mgr) {
        return NS_OK;
      }
      RefPtr<PledgeVoid> p = mgr->mOutstandingVoidPledges.Remove(id);
      if (p) {
        if (NS_SUCCEEDED(rv)) {
          p->Resolve(false);
        } else {
          auto* window = nsGlobalWindow::GetInnerWindowWithId(windowId);
          if (window) {
            if (badConstraint) {
              nsString constraint;
              constraint.AssignASCII(badConstraint);
              RefPtr<MediaStreamError> error =
                  new MediaStreamError(window->AsInner(),
                                       NS_LITERAL_STRING("OverconstrainedError"),
                                       NS_LITERAL_STRING(""),
                                       constraint);
              p->Reject(error);
            } else {
              RefPtr<MediaStreamError> error =
                  new MediaStreamError(window->AsInner(),
                                       NS_LITERAL_STRING("InternalError"));
              p->Reject(error);
            }
          }
        }
      }
      return NS_OK;
    }));
  }));
  return p.forget();
}

PrincipalHandle
SourceListener::GetPrincipalHandle() const
{
  return mPrincipalHandle;
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
GetUserMediaWindowListener::NotifySourceTrackStopped()
{
  MOZ_ASSERT(NS_IsMainThread());

  // We wait until stable state before notifying chrome so chrome only does one
  // update if more tracks are stopped in this event loop.

  if (mChromeNotificationTaskPosted) {
    return;
  }

  nsCOMPtr<nsIRunnable> runnable =
    NewRunnableMethod(this, &GetUserMediaWindowListener::NotifyChromeOfTrackStops);
  nsContentUtils::RunInStableState(runnable.forget());
  mChromeNotificationTaskPosted = true;
}

void
GetUserMediaWindowListener::NotifyChromeOfTrackStops()
{
  MOZ_ASSERT(mChromeNotificationTaskPosted);
  mChromeNotificationTaskPosted = false;

  NS_DispatchToMainThread(do_AddRef(new GetUserMediaNotificationEvent(
    GetUserMediaNotificationEvent::STOPPING, mWindowID)));
}

GetUserMediaNotificationEvent::GetUserMediaNotificationEvent(
    GetUserMediaStatus aStatus,
    uint64_t aWindowID)
: mStatus(aStatus), mWindowID(aWindowID) {}

GetUserMediaNotificationEvent::GetUserMediaNotificationEvent(
    GetUserMediaStatus aStatus,
    already_AddRefed<DOMMediaStream> aStream,
    already_AddRefed<Refcountable<UniquePtr<OnTracksAvailableCallback>>> aOnTracksAvailableCallback,
    uint64_t aWindowID,
    already_AddRefed<nsIDOMGetUserMediaErrorCallback> aError)
: mStream(aStream),
  mOnTracksAvailableCallback(aOnTracksAvailableCallback),
  mStatus(aStatus),
  mWindowID(aWindowID),
  mOnFailure(aError) {}
GetUserMediaNotificationEvent::~GetUserMediaNotificationEvent()
{
}

NS_IMETHODIMP
GetUserMediaNotificationEvent::Run()
{
  MOZ_ASSERT(NS_IsMainThread());
  // Make sure mStream is cleared and our reference to the DOMMediaStream
  // is dropped on the main thread, no matter what happens in this method.
  // Otherwise this object might be destroyed off the main thread,
  // releasing DOMMediaStream off the main thread, which is not allowed.
  RefPtr<DOMMediaStream> stream = mStream.forget();

  nsString msg;
  switch (mStatus) {
  case STARTING:
    msg = NS_LITERAL_STRING("starting");
    stream->OnTracksAvailable(mOnTracksAvailableCallback->release());
    break;
  case STOPPING:
    msg = NS_LITERAL_STRING("shutdown");
    break;
  }

  RefPtr<nsGlobalWindow> window = nsGlobalWindow::GetInnerWindowWithId(mWindowID);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  return MediaManager::NotifyRecordingStatusChange(window->AsInner(), msg);
}

} // namespace mozilla
