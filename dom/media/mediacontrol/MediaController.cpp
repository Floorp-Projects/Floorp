/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaController.h"

#include "MediaControlService.h"
#include "MediaControlUtils.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                                                    \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug,                             \
          ("MediaController=%p, Id=%" PRId64 ", " msg, this, this->Id(), \
           ##__VA_ARGS__))

namespace mozilla {
namespace dom {

static const MediaControlKeysEvent sDefaultSupportedKeys[] = {
    MediaControlKeysEvent::eFocus, MediaControlKeysEvent::ePlay,
    MediaControlKeysEvent::ePause, MediaControlKeysEvent::ePlayPause,
    MediaControlKeysEvent::eStop,
};

static void GetDefaultSupportedKeys(nsTArray<MediaControlKeysEvent>& aKeys) {
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
}

MediaController::~MediaController() {
  LOG("Destroy controller %" PRId64, Id());
  if (!mShutdown) {
    Shutdown();
  }
};

void MediaController::Focus() {
  LOG("Focus");
  UpdateMediaControlKeysEventToContentMediaIfNeeded(
      MediaControlKeysEvent::eFocus);
}

void MediaController::Play() {
  LOG("Play");
  UpdateMediaControlKeysEventToContentMediaIfNeeded(
      MediaControlKeysEvent::ePlay);
}

void MediaController::Pause() {
  LOG("Pause");
  UpdateMediaControlKeysEventToContentMediaIfNeeded(
      MediaControlKeysEvent::ePause);
}

void MediaController::PrevTrack() {
  LOG("Prev Track");
  UpdateMediaControlKeysEventToContentMediaIfNeeded(
      MediaControlKeysEvent::ePrevTrack);
}

void MediaController::NextTrack() {
  LOG("Next Track");
  UpdateMediaControlKeysEventToContentMediaIfNeeded(
      MediaControlKeysEvent::eNextTrack);
}

void MediaController::SeekBackward() {
  LOG("Seek Backward");
  UpdateMediaControlKeysEventToContentMediaIfNeeded(
      MediaControlKeysEvent::eSeekBackward);
}

void MediaController::SeekForward() {
  LOG("Seek Forward");
  UpdateMediaControlKeysEventToContentMediaIfNeeded(
      MediaControlKeysEvent::eSeekForward);
}

void MediaController::Stop() {
  LOG("Stop");
  UpdateMediaControlKeysEventToContentMediaIfNeeded(
      MediaControlKeysEvent::eStop);
}

uint64_t MediaController::Id() const { return mTopLevelBrowsingContextId; }

bool MediaController::IsAudible() const { return IsMediaAudible(); }

bool MediaController::IsPlaying() const { return IsMediaPlaying(); }

void MediaController::UpdateMediaControlKeysEventToContentMediaIfNeeded(
    MediaControlKeysEvent aEvent) {
  // There is no controlled media existing or controller has been shutdown, we
  // have no need to update media action to the content process.
  if (!IsAnyMediaBeingControlled() || mShutdown) {
    return;
  }
  // If we have an active media session, then we should directly notify the
  // browsing context where active media session exists in order to let the
  // session handle media control key events. Otherwises, we would notify the
  // top-level browsing context to let it handle events.
  RefPtr<BrowsingContext> context =
      mActiveMediaSessionContextId
          ? BrowsingContext::Get(*mActiveMediaSessionContextId)
          : BrowsingContext::Get(Id());
  if (context && !context->IsDiscarded()) {
    context->Canonical()->UpdateMediaControlKeysEvent(aEvent);
  }
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
}

void MediaController::NotifyMediaPlaybackChanged(uint64_t aBrowsingContextId,
                                                 MediaPlaybackState aState) {
  if (mShutdown) {
    return;
  }
  MediaStatusManager::NotifyMediaPlaybackChanged(aBrowsingContextId, aState);
  UpdateActivatedStateIfNeeded();
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
  return IsAnyMediaBeingControlled() && IsAudible() && !mIsRegisteredToService;
}

bool MediaController::ShouldDeactivateController() const {
  MOZ_ASSERT(!mShutdown);
  return !IsAnyMediaBeingControlled() && mIsRegisteredToService;
}

void MediaController::Activate() {
  LOG("Activate");
  MOZ_ASSERT(!mShutdown);
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  if (service && !mIsRegisteredToService) {
    mIsRegisteredToService = service->RegisterActiveMediaController(this);
    MOZ_ASSERT(mIsRegisteredToService, "Fail to register controller!");
  }
}

void MediaController::Deactivate() {
  LOG("Deactivate");
  MOZ_ASSERT(!mShutdown);
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  if (service) {
    service->GetAudioFocusManager().RevokeAudioFocus(this);
    if (mIsRegisteredToService) {
      mIsRegisteredToService = !service->UnregisterActiveMediaController(this);
      MOZ_ASSERT(!mIsRegisteredToService, "Fail to unregister controller!");
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
  if (RefPtr<MediaControlService> service = MediaControlService::GetService();
      service && mIsInPictureInPictureMode) {
    service->NotifyControllerBeingUsedInPictureInPictureMode(this);
  }
}

void MediaController::HandleActualPlaybackStateChanged() {
  // Media control service would like to know all controllers' playback state
  // in order to decide which controller should be the main controller that is
  // usually the last tab which plays media.
  if (RefPtr<MediaControlService> service = MediaControlService::GetService()) {
    service->NotifyControllerPlaybackStateChanged(this);
  }
}

bool MediaController::IsInPictureInPictureMode() const {
  return mIsInPictureInPictureMode;
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
  nsTArray<MediaControlKeysEvent> newSupportedKeys;
  GetDefaultSupportedKeys(newSupportedKeys);
  for (const auto& action : aSupportedAction) {
    MediaControlKeysEvent key = ConvertMediaSessionActionToControlKey(action);
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
}

CopyableTArray<MediaControlKeysEvent> MediaController::GetSupportedMediaKeys()
    const {
  return mSupportedKeys;
}

}  // namespace dom
}  // namespace mozilla
