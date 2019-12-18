/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlaybackController.h"

#include "nsIAudioChannelAgent.h"
#include "MediaControlUtils.h"

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("PlaybackController=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

PlaybackController::PlaybackController(BrowsingContext* aContext) {
  MOZ_ASSERT(aContext);
  // TODO : Because of bug 1593826, now we can't ganrantee that we always have
  // the outer window. In the ideal situation, we should always provide a
  // correct browsing context which has an outer window, this would be fixed in
  // bug 1593826.
  mWindow = aContext->GetDOMWindow();
}

void PlaybackController::Play() {
  /**
   * TODO : if media session has set its action handler, then we should use that
   * instead of applying our default behavior.
   * ex.
   * ```
   * MediaSession* session = MediaSessionManager::GetActiveSession();
   * if (!session || !session->HasPlayHandler()) {
   *    // Do default behavior
   * }
   * session->ExecutePlayHandler();
   * ```
   */
  // Our default behavior is to play all media elements within this window and
  // its children.
  if (mWindow) {
    LOG("Handle 'play' in default behavior");
    mWindow->SetMediaSuspend(nsISuspendedTypes::NONE_SUSPENDED);
  }
};

void PlaybackController::Pause() {
  // TODO : same as Play(), we would provide default action handling if current
  // media session doesn't have an action handler.
  if (mWindow) {
    LOG("Handle 'pause' in default behavior");
    mWindow->SetMediaSuspend(nsISuspendedTypes::SUSPENDED_PAUSE_DISPOSABLE);
  }
}

void PlaybackController::SeekBackward() {
  // TODO : use media session's action handler if it exists.
  return;
}

void PlaybackController::SeekForward() {
  // TODO : use media session's action handler if it exists.
  return;
}

void PlaybackController::PreviousTrack() {
  // TODO : use media session's action handler if it exists.
  return;
}

void PlaybackController::NextTrack() {
  // TODO : use media session's action handler if it exists.
  return;
}

void PlaybackController::SkipAd() {
  // TODO : use media session's action handler if it exists.
  return;
}

void PlaybackController::Stop() {
  // TODO : same as Play(), we would provide default action handling if current
  // media session doesn't have an action handler.
  if (mWindow) {
    LOG("Handle 'stop' in default behavior");
    mWindow->SetMediaSuspend(nsISuspendedTypes::SUSPENDED_STOP_DISPOSABLE);
  }
}

void PlaybackController::SeekTo() {
  // TODO : use media session's action handler if it exists.
  return;
}

void MediaActionHandler::HandleMediaControlKeysEvent(
    BrowsingContext* aContext, MediaControlKeysEvent aEvent) {
  PlaybackController controller(aContext);
  switch (aEvent) {
    case MediaControlKeysEvent::ePlay:
      controller.Play();
      break;
    case MediaControlKeysEvent::ePause:
      controller.Pause();
      break;
    case MediaControlKeysEvent::eStop:
      controller.Stop();
      break;
    case MediaControlKeysEvent::ePlayPause:
    case MediaControlKeysEvent::ePrevTrack:
    case MediaControlKeysEvent::eNextTrack:
    case MediaControlKeysEvent::eSeekBackward:
    case MediaControlKeysEvent::eSeekForward:
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid event.");
  };
}

}  // namespace dom
}  // namespace mozilla
