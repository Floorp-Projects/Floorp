/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaController.h"

#include "MediaControlService.h"
#include "MediaControlUtils.h"
#include "MediaControlKeySource.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/MediaSession.h"
#include "mozilla/dom/PositionStateEvent.h"

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                                                    \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug,                             \
          ("MediaController=%p, Id=%" PRId64 ", " msg, this, this->Id(), \
           ##__VA_ARGS__))

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaController, DOMEventTargetHelper)
NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(MediaController,
                                             DOMEventTargetHelper,
                                             nsITimerCallback, nsINamed)
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(MediaController,
                                               DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

nsISupports* MediaController::GetParentObject() const {
  RefPtr<BrowsingContext> bc = BrowsingContext::Get(Id());
  return bc;
}

JSObject* MediaController::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return MediaController_Binding::Wrap(aCx, this, aGivenProto);
}

void MediaController::GetSupportedKeys(
    nsTArray<MediaControlKey>& aRetVal) const {
  aRetVal.Clear();
  for (const auto& key : mSupportedKeys) {
    aRetVal.AppendElement(key);
  }
}

void MediaController::GetMetadata(MediaMetadataInit& aMetadata,
                                  ErrorResult& aRv) {
  if (!IsActive() || mShutdown) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  const MediaMetadataBase metadata = GetCurrentMediaMetadata();
  aMetadata.mTitle = metadata.mTitle;
  aMetadata.mArtist = metadata.mArtist;
  aMetadata.mAlbum = metadata.mAlbum;
  for (const auto& artwork : metadata.mArtwork) {
    if (MediaImage* image = aMetadata.mArtwork.AppendElement(fallible)) {
      image->mSrc = artwork.mSrc;
      image->mSizes = artwork.mSizes;
      image->mType = artwork.mType;
    } else {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
  }
}

static const MediaControlKey sDefaultSupportedKeys[] = {
    MediaControlKey::Focus,     MediaControlKey::Play, MediaControlKey::Pause,
    MediaControlKey::Playpause, MediaControlKey::Stop,
};

static void GetDefaultSupportedKeys(nsTArray<MediaControlKey>& aKeys) {
  for (const auto& key : sDefaultSupportedKeys) {
    aKeys.AppendElement(key);
  }
}

MediaController::MediaController(uint64_t aBrowsingContextId)
    : MediaStatusManager(aBrowsingContextId) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(),
                        "MediaController only runs on Chrome process!");
  LOG("Create controller %" PRId64, Id());
  GetDefaultSupportedKeys(mSupportedKeys);
  mSupportedActionsChangedListener = SupportedActionsChangedEvent().Connect(
      AbstractThread::MainThread(), this,
      &MediaController::HandleSupportedMediaSessionActionsChanged);
  mPlaybackChangedListener = PlaybackChangedEvent().Connect(
      AbstractThread::MainThread(), this,
      &MediaController::HandleActualPlaybackStateChanged);
  mPositionStateChangedListener = PositionChangedEvent().Connect(
      AbstractThread::MainThread(), this,
      &MediaController::HandlePositionStateChanged);
  mMetadataChangedListener =
      MetadataChangedEvent().Connect(AbstractThread::MainThread(), this,
                                     &MediaController::HandleMetadataChanged);
}

MediaController::~MediaController() {
  LOG("Destroy controller %" PRId64, Id());
  if (!mShutdown) {
    Shutdown();
  }
};

void MediaController::Focus() {
  LOG("Focus");
  UpdateMediaControlActionToContentMediaIfNeeded(
      MediaControlAction(MediaControlKey::Focus));
}

void MediaController::Play() {
  LOG("Play");
  UpdateMediaControlActionToContentMediaIfNeeded(
      MediaControlAction(MediaControlKey::Play));
}

void MediaController::Pause() {
  LOG("Pause");
  UpdateMediaControlActionToContentMediaIfNeeded(
      MediaControlAction(MediaControlKey::Pause));
}

void MediaController::PrevTrack() {
  LOG("Prev Track");
  UpdateMediaControlActionToContentMediaIfNeeded(
      MediaControlAction(MediaControlKey::Previoustrack));
}

void MediaController::NextTrack() {
  LOG("Next Track");
  UpdateMediaControlActionToContentMediaIfNeeded(
      MediaControlAction(MediaControlKey::Nexttrack));
}

void MediaController::SeekBackward() {
  LOG("Seek Backward");
  UpdateMediaControlActionToContentMediaIfNeeded(
      MediaControlAction(MediaControlKey::Seekbackward));
}

void MediaController::SeekForward() {
  LOG("Seek Forward");
  UpdateMediaControlActionToContentMediaIfNeeded(
      MediaControlAction(MediaControlKey::Seekforward));
}

void MediaController::SkipAd() {
  LOG("Skip Ad");
  UpdateMediaControlActionToContentMediaIfNeeded(
      MediaControlAction(MediaControlKey::Skipad));
}

void MediaController::SeekTo(double aSeekTime, bool aFastSeek) {
  LOG("Seek To");
  UpdateMediaControlActionToContentMediaIfNeeded(MediaControlAction(
      MediaControlKey::Seekto, SeekDetails(aSeekTime, aFastSeek)));
}

void MediaController::Stop() {
  LOG("Stop");
  UpdateMediaControlActionToContentMediaIfNeeded(
      MediaControlAction(MediaControlKey::Stop));
  MediaStatusManager::ClearActiveMediaSessionContextIdIfNeeded();
}

uint64_t MediaController::Id() const { return mTopLevelBrowsingContextId; }

bool MediaController::IsAudible() const { return IsMediaAudible(); }

bool MediaController::IsPlaying() const { return IsMediaPlaying(); }

bool MediaController::IsActive() const { return mIsActive; };

bool MediaController::ShouldPropagateActionToAllContexts(
    const MediaControlAction& aAction) const {
  // These three actions have default action handler for each frame, so we
  // need to propagate to all contexts. We would handle default handlers in
  // `ContentMediaController::HandleMediaKey`.
  return aAction.mKey == MediaControlKey::Play ||
         aAction.mKey == MediaControlKey::Pause ||
         aAction.mKey == MediaControlKey::Stop;
}

void MediaController::UpdateMediaControlActionToContentMediaIfNeeded(
    const MediaControlAction& aAction) {
  // If the controller isn't active or it has been shutdown, we don't need to
  // update media action to the content process.
  if (!mIsActive || mShutdown) {
    return;
  }

  // For some actions which have default action handler, we want to propagate
  // them on all contexts in order to trigger the default handler on each
  // context separately. Otherwise, other action should only be propagated to
  // the context where active media session exists.
  const bool propateToAll = ShouldPropagateActionToAllContexts(aAction);
  const uint64_t targetContextId = propateToAll || !mActiveMediaSessionContextId
                                       ? Id()
                                       : *mActiveMediaSessionContextId;
  RefPtr<BrowsingContext> context = BrowsingContext::Get(targetContextId);
  if (!context || context->IsDiscarded()) {
    return;
  }

  if (propateToAll) {
    context->PreOrderWalk([&](BrowsingContext* bc) {
      bc->Canonical()->UpdateMediaControlAction(aAction);
    });
  } else {
    context->Canonical()->UpdateMediaControlAction(aAction);
  }
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  MOZ_ASSERT(service);
  service->NotifyMediaControlHasEverBeenUsed();
}

void MediaController::Shutdown() {
  MOZ_ASSERT(!mShutdown, "Do not call shutdown twice!");
  // The media controller would be removed from the service when we receive a
  // notification from the content process about all controlled media has been
  // stoppped. However, if controlled media is stopped after detaching
  // browsing context, then sending the notification from the content process
  // would fail so that we are not able to notify the chrome process to remove
  // the corresponding controller. Therefore, we should manually remove the
  // controller from the service.
  Deactivate();
  mShutdown = true;
  mSupportedActionsChangedListener.DisconnectIfExists();
  mPlaybackChangedListener.DisconnectIfExists();
  mPositionStateChangedListener.DisconnectIfExists();
  mMetadataChangedListener.DisconnectIfExists();
}

void MediaController::NotifyMediaPlaybackChanged(uint64_t aBrowsingContextId,
                                                 MediaPlaybackState aState) {
  if (mShutdown) {
    return;
  }
  MediaStatusManager::NotifyMediaPlaybackChanged(aBrowsingContextId, aState);
  UpdateDeactivationTimerIfNeeded();
  UpdateActivatedStateIfNeeded();
}

void MediaController::UpdateDeactivationTimerIfNeeded() {
  if (!StaticPrefs::media_mediacontrol_stopcontrol_timer()) {
    return;
  }

  bool shouldBeAlwaysActive = IsPlaying() || IsBeingUsedInPIPModeOrFullscreen();
  if (shouldBeAlwaysActive && mDeactivationTimer) {
    LOG("Cancel deactivation timer");
    mDeactivationTimer->Cancel();
    mDeactivationTimer = nullptr;
  } else if (!shouldBeAlwaysActive && !mDeactivationTimer) {
    nsresult rv = NS_NewTimerWithCallback(
        getter_AddRefs(mDeactivationTimer), this,
        StaticPrefs::media_mediacontrol_stopcontrol_timer_ms(),
        nsITimer::TYPE_ONE_SHOT, AbstractThread::MainThread());
    if (NS_SUCCEEDED(rv)) {
      LOG("Create a deactivation timer");
    } else {
      LOG("Failed to create a deactivation timer");
    }
  }
}

bool MediaController::IsBeingUsedInPIPModeOrFullscreen() const {
  return mIsInPictureInPictureMode || mIsInFullScreenMode;
}

NS_IMETHODIMP MediaController::Notify(nsITimer* aTimer) {
  mDeactivationTimer = nullptr;
  if (!StaticPrefs::media_mediacontrol_stopcontrol_timer()) {
    return NS_OK;
  }

  if (mShutdown) {
    LOG("Cancel deactivation timer because controller has been shutdown");
    return NS_OK;
  }

  // As the media being used in the PIP mode or fullscreen would always display
  // on the screen, users would have high chance to interact with it again, so
  // we don't want to stop media control.
  if (IsBeingUsedInPIPModeOrFullscreen()) {
    LOG("Cancel deactivation timer because controller is in PIP mode");
    return NS_OK;
  }

  if (IsPlaying()) {
    LOG("Cancel deactivation timer because controller is still playing");
    return NS_OK;
  }

  if (!mIsActive) {
    LOG("Cancel deactivation timer because controller has been deactivated");
    return NS_OK;
  }
  Deactivate();
  return NS_OK;
}

NS_IMETHODIMP MediaController::GetName(nsACString& aName) {
  aName.AssignLiteral("MediaController");
  return NS_OK;
}

void MediaController::NotifyMediaAudibleChanged(uint64_t aBrowsingContextId,
                                                MediaAudibleState aState) {
  if (mShutdown) {
    return;
  }

  bool oldAudible = IsAudible();
  MediaStatusManager::NotifyMediaAudibleChanged(aBrowsingContextId, aState);
  if (IsAudible() == oldAudible) {
    return;
  }
  UpdateActivatedStateIfNeeded();

  // Request the audio focus amongs different controllers that could cause
  // pausing other audible controllers if we enable the audio focus management.
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  MOZ_ASSERT(service);
  if (IsAudible()) {
    service->GetAudioFocusManager().RequestAudioFocus(this);
  } else {
    service->GetAudioFocusManager().RevokeAudioFocus(this);
  }
}

bool MediaController::ShouldActivateController() const {
  MOZ_ASSERT(!mShutdown);
  // After media is successfully loaded and match our critiera, such as its
  // duration is longer enough, which is used to exclude the notification-ish
  // sound, then it would be able to be controlled once the controll gets
  // activated.
  //
  // Activating a controller means that we would start to intercept the media
  // keys on the platform and show the virtual control interface (if needed).
  // The controller would be activated when (1) controllable media starts in the
  // browsing context that controller belongs to (2) controllable media enters
  // fullscreen or PIP mode.
  return IsAnyMediaBeingControlled() &&
         (IsPlaying() || IsBeingUsedInPIPModeOrFullscreen()) && !mIsActive;
}

bool MediaController::ShouldDeactivateController() const {
  MOZ_ASSERT(!mShutdown);
  // If we don't have an active media session and no controlled media exists,
  // then we don't need to keep controller active, because there is nothing to
  // control. However, if we still have an active media session, then we should
  // keep controller active in order to receive media keys even if we don't have
  // any controlled media existing, because a website might start other media
  // when media session receives media keys.
  return !IsAnyMediaBeingControlled() && mIsActive &&
         !mActiveMediaSessionContextId;
}

void MediaController::Activate() {
  MOZ_ASSERT(!mShutdown);
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  if (service && !mIsActive) {
    LOG("Activate");
    mIsActive = service->RegisterActiveMediaController(this);
    MOZ_ASSERT(mIsActive, "Fail to register controller!");
    DispatchAsyncEvent(u"activated"_ns);
  }
}

void MediaController::Deactivate() {
  MOZ_ASSERT(!mShutdown);
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  if (service) {
    service->GetAudioFocusManager().RevokeAudioFocus(this);
    if (mIsActive) {
      LOG("Deactivate");
      mIsActive = !service->UnregisterActiveMediaController(this);
      MOZ_ASSERT(!mIsActive, "Fail to unregister controller!");
      DispatchAsyncEvent(u"deactivated"_ns);
    }
  }
}

void MediaController::SetIsInPictureInPictureMode(
    uint64_t aBrowsingContextId, bool aIsInPictureInPictureMode) {
  if (mIsInPictureInPictureMode == aIsInPictureInPictureMode) {
    return;
  }
  LOG("Set IsInPictureInPictureMode to %s",
      aIsInPictureInPictureMode ? "true" : "false");
  mIsInPictureInPictureMode = aIsInPictureInPictureMode;
  ForceToBecomeMainControllerIfNeeded();
  UpdateDeactivationTimerIfNeeded();
  mPictureInPictureModeChangedEvent.Notify(mIsInPictureInPictureMode);
}

void MediaController::NotifyMediaFullScreenState(uint64_t aBrowsingContextId,
                                                 bool aIsInFullScreen) {
  if (mIsInFullScreenMode == aIsInFullScreen) {
    return;
  }
  LOG("%s fullscreen", aIsInFullScreen ? "Entered" : "Left");
  mIsInFullScreenMode = aIsInFullScreen;
  ForceToBecomeMainControllerIfNeeded();
  mFullScreenChangedEvent.Notify(mIsInFullScreenMode);
}

bool MediaController::IsMainController() const {
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  return service ? service->GetMainController() == this : false;
}

bool MediaController::ShouldRequestForMainController() const {
  // This controller is already the main controller.
  if (IsMainController()) {
    return false;
  }
  // We would only force controller to become main controller if it's in the
  // PIP mode or fullscreen, otherwise it should follow the general rule.
  // In addition, do nothing if the controller has been shutdowned.
  return IsBeingUsedInPIPModeOrFullscreen() && !mShutdown;
}

void MediaController::ForceToBecomeMainControllerIfNeeded() {
  if (!ShouldRequestForMainController()) {
    return;
  }
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  MOZ_ASSERT(service, "service was shutdown before shutting down controller?");
  // If the controller hasn't been activated and it's ready to be activated,
  // then activating it should also make it become a main controller. If it's
  // already activated but isn't a main controller yet, then explicitly request
  // it.
  if (!IsActive() && ShouldActivateController()) {
    Activate();
  } else if (IsActive()) {
    service->RequestUpdateMainController(this);
  }
}

void MediaController::HandleActualPlaybackStateChanged() {
  // Media control service would like to know all controllers' playback state
  // in order to decide which controller should be the main controller that is
  // usually the last tab which plays media.
  if (RefPtr<MediaControlService> service = MediaControlService::GetService()) {
    service->NotifyControllerPlaybackStateChanged(this);
  }
  DispatchAsyncEvent(u"playbackstatechange"_ns);
}

void MediaController::UpdateActivatedStateIfNeeded() {
  if (ShouldActivateController()) {
    Activate();
  } else if (ShouldDeactivateController()) {
    Deactivate();
  }
}

void MediaController::HandleSupportedMediaSessionActionsChanged(
    const nsTArray<MediaSessionAction>& aSupportedAction) {
  // Convert actions to keys, some of them have been included in the supported
  // keys, such as "play", "pause" and "stop".
  nsTArray<MediaControlKey> newSupportedKeys;
  GetDefaultSupportedKeys(newSupportedKeys);
  for (const auto& action : aSupportedAction) {
    MediaControlKey key = ConvertMediaSessionActionToControlKey(action);
    if (!newSupportedKeys.Contains(key)) {
      newSupportedKeys.AppendElement(key);
    }
  }
  // As the supported key event should only be notified when supported keys
  // change, so abort following steps if they don't change.
  if (newSupportedKeys == mSupportedKeys) {
    return;
  }
  LOG("Supported keys changes");
  mSupportedKeys = newSupportedKeys;
  mSupportedKeysChangedEvent.Notify(mSupportedKeys);
  RefPtr<AsyncEventDispatcher> asyncDispatcher = new AsyncEventDispatcher(
      this, u"supportedkeyschange"_ns, CanBubble::eYes);
  asyncDispatcher->PostDOMEvent();
  MediaController_Binding::ClearCachedSupportedKeysValue(this);
}

void MediaController::HandlePositionStateChanged(const PositionState& aState) {
  PositionStateEventInit init;
  init.mDuration = aState.mDuration;
  init.mPlaybackRate = aState.mPlaybackRate;
  init.mPosition = aState.mLastReportedPlaybackPosition;
  RefPtr<PositionStateEvent> event =
      PositionStateEvent::Constructor(this, u"positionstatechange"_ns, init);
  DispatchAsyncEvent(event.forget());
}

void MediaController::HandleMetadataChanged(
    const MediaMetadataBase& aMetadata) {
  // The reason we don't append metadata with `metadatachange` event is that
  // allocating artwork might fail if the memory is not enough, but for the
  // event we are not able to throw an error. Therefore, we want to the listener
  // to use `getMetadata()` to get metadata, because it would throw an error if
  // we fail to allocate artwork.
  DispatchAsyncEvent(u"metadatachange"_ns);
  // If metadata change is because of resetting active media session, then we
  // should check if controller needs to be deactivated.
  if (ShouldDeactivateController()) {
    Deactivate();
  }
}

void MediaController::DispatchAsyncEvent(const nsAString& aName) {
  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
  event->InitEvent(aName, false, false);
  event->SetTrusted(true);
  DispatchAsyncEvent(event.forget());
}

void MediaController::DispatchAsyncEvent(already_AddRefed<Event> aEvent) {
  RefPtr<Event> event = aEvent;
  MOZ_ASSERT(event);
  nsAutoString eventType;
  event->GetType(eventType);
  if (!mIsActive && !eventType.EqualsLiteral("deactivated")) {
    LOG("Only 'deactivated' can be dispatched on a deactivated controller, not "
        "'%s'",
        NS_ConvertUTF16toUTF8(eventType).get());
    return;
  }
  LOG("Dispatch event %s", NS_ConvertUTF16toUTF8(eventType).get());
  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, event.forget());
  asyncDispatcher->PostDOMEvent();
}

CopyableTArray<MediaControlKey> MediaController::GetSupportedMediaKeys() const {
  return mSupportedKeys;
}

void MediaController::Select() const {
  if (RefPtr<BrowsingContext> bc = BrowsingContext::Get(Id())) {
    bc->Canonical()->AddPageAwakeRequest();
  }
}

void MediaController::Unselect() const {
  if (RefPtr<BrowsingContext> bc = BrowsingContext::Get(Id())) {
    bc->Canonical()->RemovePageAwakeRequest();
  }
}

}  // namespace mozilla::dom
