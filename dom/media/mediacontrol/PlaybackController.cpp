/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlaybackController.h"

#include "MediaControlUtils.h"
#include "mozilla/dom/MediaSession.h"
#include "mozilla/dom/Navigator.h"
#include "nsFocusManager.h"

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("PlaybackController=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

PlaybackController::PlaybackController(BrowsingContext* aContext) {
  MOZ_ASSERT(aContext);
  mBC = aContext;
}

MediaSession* PlaybackController::GetMediaSession() const {
  RefPtr<nsPIDOMWindowOuter> window = mBC->GetDOMWindow();
  if (!window) {
    return nullptr;
  }

  RefPtr<Navigator> navigator = window->GetNavigator();
  return navigator->HasCreatedMediaSession() ? navigator->MediaSession()
                                             : nullptr;
}

void PlaybackController::NotifyContentControlKeyEventReceiver(
    MediaControlKeysEvent aEvent) {
  if (RefPtr<ContentControlKeyEventReceiver> receiver =
          ContentControlKeyEventReceiver::Get(mBC)) {
    LOG("Handle '%s' in default behavior", ToMediaControlKeysEventStr(aEvent));
    receiver->HandleEvent(aEvent);
  }
}

void PlaybackController::NotifyMediaSession(MediaSessionAction aAction) {
  if (RefPtr<MediaSession> session = GetMediaSession()) {
    LOG("Handle '%s' in media session behavior",
        ToMediaSessionActionStr(aAction));
    session->NotifyHandler(aAction);
  }
}

void PlaybackController::NotifyMediaSessionWhenActionIsSupported(
    MediaSessionAction aAction) {
  if (IsMediaSessionActionSupported(aAction)) {
    NotifyMediaSession(aAction);
  }
}

bool PlaybackController::IsMediaSessionActionSupported(
    MediaSessionAction aAction) const {
  RefPtr<MediaSession> session = GetMediaSession();
  return session ? session->IsSupportedAction(aAction) : false;
}

void PlaybackController::Focus() {
  // Focus is not part of the MediaSession standard, so always use the
  // default behavior and focus the window currently playing media.
  if (RefPtr<nsPIDOMWindowOuter> win = mBC->GetDOMWindow()) {
    nsFocusManager::FocusWindow(win, CallerType::System);
  }
}

void PlaybackController::Play() {
  const MediaSessionAction action = MediaSessionAction::Play;
  if (IsMediaSessionActionSupported(action)) {
    NotifyMediaSession(action);
  } else {
    NotifyContentControlKeyEventReceiver(MediaControlKeysEvent::ePlay);
  }
}

void PlaybackController::Pause() {
  const MediaSessionAction action = MediaSessionAction::Pause;
  if (IsMediaSessionActionSupported(action)) {
    NotifyMediaSession(action);
  } else {
    NotifyContentControlKeyEventReceiver(MediaControlKeysEvent::ePause);
  }
}

void PlaybackController::SeekBackward() {
  NotifyMediaSessionWhenActionIsSupported(MediaSessionAction::Seekbackward);
}

void PlaybackController::SeekForward() {
  NotifyMediaSessionWhenActionIsSupported(MediaSessionAction::Seekforward);
}

void PlaybackController::PreviousTrack() {
  NotifyMediaSessionWhenActionIsSupported(MediaSessionAction::Previoustrack);
}

void PlaybackController::NextTrack() {
  NotifyMediaSessionWhenActionIsSupported(MediaSessionAction::Nexttrack);
}

void PlaybackController::SkipAd() {
  // TODO : use media session's action handler if it exists. MediaSessionAction
  // doesn't support `skipad` yet.
  return;
}

void PlaybackController::Stop() {
  const MediaSessionAction action = MediaSessionAction::Stop;
  if (IsMediaSessionActionSupported(action)) {
    NotifyMediaSession(action);
  } else {
    NotifyContentControlKeyEventReceiver(MediaControlKeysEvent::eStop);
  }
}

void PlaybackController::SeekTo() {
  // TODO : use media session's action handler if it exists. MediaSessionAction
  // doesn't support `seekto` yet.
  return;
}

void MediaActionHandler::HandleMediaControlKeysEvent(
    BrowsingContext* aContext, MediaControlKeysEvent aEvent) {
  PlaybackController controller(aContext);
  switch (aEvent) {
    case MediaControlKeysEvent::eFocus:
      controller.Focus();
      return;
    case MediaControlKeysEvent::ePlay:
      controller.Play();
      return;
    case MediaControlKeysEvent::ePause:
      controller.Pause();
      return;
    case MediaControlKeysEvent::eStop:
      controller.Stop();
      return;
    case MediaControlKeysEvent::ePrevTrack:
      controller.PreviousTrack();
      return;
    case MediaControlKeysEvent::eNextTrack:
      controller.NextTrack();
      return;
    case MediaControlKeysEvent::eSeekBackward:
      controller.SeekBackward();
      return;
    case MediaControlKeysEvent::eSeekForward:
      controller.SeekForward();
      return;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid event.");
  };
}

}  // namespace dom
}  // namespace mozilla
