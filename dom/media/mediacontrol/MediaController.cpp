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
#include "mozilla/dom/MediaSessionUtils.h"

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                                                    \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug,                             \
          ("MediaController=%p, Id=%" PRId64 ", " msg, this, this->Id(), \
           ##__VA_ARGS__))

namespace mozilla {
namespace dom {

MediaController::MediaController(uint64_t aContextId)
    : MediaSessionController(aContextId) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(),
                        "MediaController only runs on Chrome process!");
  LOG("Create controller %" PRId64, Id());
}

MediaController::~MediaController() {
  LOG("Destroy controller %" PRId64, Id());
  if (!mShutdown) {
    Shutdown();
  }
};

void MediaController::Play() {
  LOG("Play");
  SetGuessedPlayState(MediaSessionPlaybackState::Playing);
  UpdateMediaControlKeysEventToContentMediaIfNeeded(
      MediaControlKeysEvent::ePlay);
}

void MediaController::Pause() {
  LOG("Pause");
  SetGuessedPlayState(MediaSessionPlaybackState::Paused);
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
  SetGuessedPlayState(MediaSessionPlaybackState::None);
  UpdateMediaControlKeysEventToContentMediaIfNeeded(
      MediaControlKeysEvent::eStop);
}

void MediaController::UpdateMediaControlKeysEventToContentMediaIfNeeded(
    MediaControlKeysEvent aEvent) {
  // There is no controlled media existing or controller has been shutdown, we
  // have no need to update media action to the content process.
  if (!ControlledMediaNum() || mShutdown) {
    return;
  }
  // If we have an active media session, then we should directly notify the
  // browsing context where active media session exists in order to let the
  // session handle media control key events. Otherwises, we would notify the
  // top-level browsing context to let it handle events.
  RefPtr<BrowsingContext> context =
      mActiveMediaSessionContextId
          ? BrowsingContext::Get(*mActiveMediaSessionContextId)
          : BrowsingContext::Get(mTopLevelBCId);
  if (context && !context->IsDiscarded()) {
    context->Canonical()->UpdateMediaControlKeysEvent(aEvent);
  }
}

void MediaController::Shutdown() {
  MOZ_ASSERT(!mShutdown, "Do not call shutdown twice!");
  SetGuessedPlayState(MediaSessionPlaybackState::None);
  // The media controller would be removed from the service when we receive a
  // notification from the content process about all controlled media has been
  // stoppped. However, if controlled media is stopped after detaching
  // browsing context, then sending the notification from the content process
  // would fail so that we are not able to notify the chrome process to remove
  // the corresponding controller. Therefore, we should manually remove the
  // controller from the service.
  Deactivate();
  mControlledMediaNum = 0;
  mPlayingControlledMediaNum = 0;
  mShutdown = true;
}

void MediaController::NotifyMediaStateChanged(ControlledMediaState aState) {
  if (mShutdown) {
    return;
  }
  if (aState == ControlledMediaState::eStarted) {
    IncreaseControlledMediaNum();
  } else if (aState == ControlledMediaState::eStopped) {
    DecreaseControlledMediaNum();
  } else if (aState == ControlledMediaState::ePlayed) {
    IncreasePlayingControlledMediaNum();
  } else if (aState == ControlledMediaState::ePaused) {
    DecreasePlayingControlledMediaNum();
  }
}

void MediaController::NotifyMediaAudibleChanged(bool aAudible) {
  if (mShutdown) {
    return;
  }
  mAudible = aAudible;
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  MOZ_ASSERT(service);
  if (mAudible) {
    service->GetAudioFocusManager().RequestAudioFocus(this);
  } else {
    service->GetAudioFocusManager().RevokeAudioFocus(this);
  }
}

void MediaController::IncreaseControlledMediaNum() {
  MOZ_ASSERT(!mShutdown);
  MOZ_DIAGNOSTIC_ASSERT(mControlledMediaNum >= 0);
  mControlledMediaNum++;
  LOG("Increase controlled media num to %" PRId64, mControlledMediaNum);
  if (mControlledMediaNum == 1) {
    Activate();
  }
}

void MediaController::DecreaseControlledMediaNum() {
  MOZ_ASSERT(!mShutdown);
  MOZ_DIAGNOSTIC_ASSERT(mControlledMediaNum >= 1);
  mControlledMediaNum--;
  LOG("Decrease controlled media num to %" PRId64, mControlledMediaNum);
  if (mControlledMediaNum == 0) {
    Deactivate();
  }
}

void MediaController::IncreasePlayingControlledMediaNum() {
  MOZ_ASSERT(!mShutdown);
  MOZ_ASSERT(mPlayingControlledMediaNum >= 0);
  mPlayingControlledMediaNum++;
  LOG("Increase playing controlled media num to %" PRId64,
      mPlayingControlledMediaNum);
  MOZ_ASSERT(mPlayingControlledMediaNum <= mControlledMediaNum,
             "The number of playing media should not exceed the number of "
             "controlled media!");
  if (mPlayingControlledMediaNum == 1) {
    SetGuessedPlayState(MediaSessionPlaybackState::Playing);
  }
}

void MediaController::DecreasePlayingControlledMediaNum() {
  MOZ_ASSERT(!mShutdown);
  mPlayingControlledMediaNum--;
  LOG("Decrease playing controlled media num to %" PRId64,
      mPlayingControlledMediaNum);
  MOZ_ASSERT(mPlayingControlledMediaNum >= 0);
  if (mPlayingControlledMediaNum == 0) {
    SetGuessedPlayState(MediaSessionPlaybackState::Paused);
  }
}

// TODO : Use watchable to moniter mControlledMediaNum
void MediaController::Activate() {
  MOZ_ASSERT(!mShutdown);
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  if (service && !mIsRegisteredToService) {
    mIsRegisteredToService = service->RegisterActiveMediaController(this);
    MOZ_ASSERT(mIsRegisteredToService, "Fail to register controller!");
  }
}

void MediaController::Deactivate() {
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

void MediaController::SetDeclaredPlaybackState(
    uint64_t aSessionContextId, MediaSessionPlaybackState aState) {
  MediaSessionController::SetDeclaredPlaybackState(aSessionContextId, aState);
  UpdateActualPlaybackState();
}

void MediaController::SetGuessedPlayState(MediaSessionPlaybackState aState) {
  if (mShutdown || mGuessedPlaybackState == aState) {
    return;
  }
  LOG("SetGuessedPlayState : '%s'", ToMediaSessionPlaybackStateStr(aState));
  mGuessedPlaybackState = aState;
  UpdateActualPlaybackState();
}

void MediaController::UpdateActualPlaybackState() {
  // The way to compute the actual playback state is based on the spec.
  // https://w3c.github.io/mediasession/#actual-playback-state
  MediaSessionPlaybackState newState =
      GetCurrentDeclaredPlaybackState() == MediaSessionPlaybackState::Playing
          ? MediaSessionPlaybackState::Playing
          : mGuessedPlaybackState;
  if (mActualPlaybackState == newState) {
    return;
  }
  mActualPlaybackState = newState;
  LOG("UpdateActualPlaybackState : '%s'",
      ToMediaSessionPlaybackStateStr(mActualPlaybackState));
  if (RefPtr<MediaControlService> service = MediaControlService::GetService()) {
    service->NotifyControllerPlaybackStateChanged(this);
  }
}

void MediaController::SetIsInPictureInPictureMode(
    bool aIsInPictureInPictureMode) {
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

bool MediaController::IsInPictureInPictureMode() const {
  return mIsInPictureInPictureMode;
}

MediaSessionPlaybackState MediaController::GetState() const {
  return mActualPlaybackState;
}

bool MediaController::IsAudible() const {
  return mGuessedPlaybackState == MediaSessionPlaybackState::Playing &&
         mAudible;
}

uint64_t MediaController::ControlledMediaNum() const {
  return mControlledMediaNum;
}

}  // namespace dom
}  // namespace mozilla
