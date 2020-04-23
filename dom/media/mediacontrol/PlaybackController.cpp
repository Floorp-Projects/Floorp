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

MediaSession* PlaybackController::GetMediaSession() {
  RefPtr<nsPIDOMWindowOuter> window = mBC->GetDOMWindow();
  if (!window) {
    return nullptr;
  }

  RefPtr<Navigator> navigator = window->GetNavigator();
  return navigator->HasCreatedMediaSession() ? navigator->MediaSession()
                                             : nullptr;
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
  RefPtr<MediaSession> session = GetMediaSession();
  if (!session || !session->IsSupportedAction(action)) {
    // Our default behavior is to play all media elements within same browsing
    // context tree.
    LOG("Handle 'play' in default behavior");
    if (RefPtr<ContentControlKeyEventReceiver> receiver =
            ContentControlKeyEventReceiver::Get(mBC)) {
      receiver->OnKeyPressed(MediaControlKeysEvent::ePlay);
    }
  } else {
    session->NotifyHandler(action);
  }
};

void PlaybackController::Pause() {
  const MediaSessionAction action = MediaSessionAction::Pause;
  RefPtr<MediaSession> session = GetMediaSession();
  if (!session || !session->IsSupportedAction(action)) {
    // Our default behavior is to pause all media elements within same browsing
    // context tree.
    LOG("Handle 'pause' in default behavior");
    if (RefPtr<ContentControlKeyEventReceiver> receiver =
            ContentControlKeyEventReceiver::Get(mBC)) {
      receiver->OnKeyPressed(MediaControlKeysEvent::ePause);
    }
  } else {
    session->NotifyHandler(action);
  }
}

void PlaybackController::SeekBackward() {
  const MediaSessionAction action = MediaSessionAction::Seekbackward;
  if (RefPtr<MediaSession> session = GetMediaSession();
      session && session->IsSupportedAction(action)) {
    session->NotifyHandler(action);
  }
}

void PlaybackController::SeekForward() {
  const MediaSessionAction action = MediaSessionAction::Seekforward;
  if (RefPtr<MediaSession> session = GetMediaSession();
      session && session->IsSupportedAction(action)) {
    session->NotifyHandler(action);
  }
}

void PlaybackController::PreviousTrack() {
  if (RefPtr<MediaSession> session = GetMediaSession()) {
    session->NotifyHandler(MediaSessionAction::Previoustrack);
  }
}

void PlaybackController::NextTrack() {
  if (RefPtr<MediaSession> session = GetMediaSession()) {
    session->NotifyHandler(MediaSessionAction::Nexttrack);
  }
}

void PlaybackController::SkipAd() {
  // TODO : use media session's action handler if it exists. MediaSessionAction
  // doesn't support `skipad` yet.
  return;
}

void PlaybackController::Stop() {
  const MediaSessionAction action = MediaSessionAction::Stop;
  RefPtr<MediaSession> session = GetMediaSession();
  if (!session || !session->IsSupportedAction(action)) {
    // Our default behavior is to stop all media elements within same browsing
    // context tree.
    LOG("Handle 'stop' in default behavior");
    RefPtr<ContentControlKeyEventReceiver> receiver =
        ContentControlKeyEventReceiver::Get(mBC);
    if (receiver) {
      receiver->OnKeyPressed(MediaControlKeysEvent::eStop);
    }
  } else {
    session->NotifyHandler(action);
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
