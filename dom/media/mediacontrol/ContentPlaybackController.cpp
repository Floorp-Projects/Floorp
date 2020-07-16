/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentPlaybackController.h"

#include "MediaControlUtils.h"
#include "mozilla/dom/MediaSession.h"
#include "mozilla/dom/Navigator.h"
#include "nsFocusManager.h"

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("ContentPlaybackController=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

ContentPlaybackController::ContentPlaybackController(
    BrowsingContext* aContext) {
  MOZ_ASSERT(aContext);
  mBC = aContext;
}

MediaSession* ContentPlaybackController::GetMediaSession() const {
  RefPtr<nsPIDOMWindowOuter> window = mBC->GetDOMWindow();
  if (!window) {
    return nullptr;
  }

  RefPtr<Navigator> navigator = window->GetNavigator();
  return navigator->HasCreatedMediaSession() ? navigator->MediaSession()
                                             : nullptr;
}

void ContentPlaybackController::NotifyContentMediaControlKeyReceiver(
    MediaControlKey aKey) {
  if (RefPtr<ContentMediaControlKeyReceiver> receiver =
          ContentMediaControlKeyReceiver::Get(mBC)) {
    LOG("Handle '%s' in default behavior", ToMediaControlKeyStr(aKey));
    receiver->HandleMediaKey(aKey);
  }
}

void ContentPlaybackController::NotifyMediaSession(MediaSessionAction aAction) {
  MediaSessionActionDetails details;
  details.mAction = aAction;
  NotifyMediaSession(details);
}

void ContentPlaybackController::NotifyMediaSession(
    const MediaSessionActionDetails& aDetails) {
  if (RefPtr<MediaSession> session = GetMediaSession()) {
    LOG("Handle '%s' in media session behavior",
        ToMediaSessionActionStr(aDetails.mAction));
    session->NotifyHandler(aDetails);
  }
}

void ContentPlaybackController::NotifyMediaSessionWhenActionIsSupported(
    MediaSessionAction aAction) {
  if (IsMediaSessionActionSupported(aAction)) {
    NotifyMediaSession(aAction);
  }
}

bool ContentPlaybackController::IsMediaSessionActionSupported(
    MediaSessionAction aAction) const {
  RefPtr<MediaSession> session = GetMediaSession();
  return session ? session->IsSupportedAction(aAction) : false;
}

void ContentPlaybackController::Focus() {
  // Focus is not part of the MediaSession standard, so always use the
  // default behavior and focus the window currently playing media.
  if (RefPtr<nsPIDOMWindowOuter> win = mBC->GetDOMWindow()) {
    nsFocusManager::FocusWindow(win, CallerType::System);
  }
}

void ContentPlaybackController::Play() {
  const MediaSessionAction action = MediaSessionAction::Play;
  if (IsMediaSessionActionSupported(action)) {
    NotifyMediaSession(action);
  } else {
    NotifyContentMediaControlKeyReceiver(MediaControlKey::Play);
  }
}

void ContentPlaybackController::Pause() {
  const MediaSessionAction action = MediaSessionAction::Pause;
  if (IsMediaSessionActionSupported(action)) {
    NotifyMediaSession(action);
  } else {
    NotifyContentMediaControlKeyReceiver(MediaControlKey::Pause);
  }
}

void ContentPlaybackController::SeekBackward() {
  NotifyMediaSessionWhenActionIsSupported(MediaSessionAction::Seekbackward);
}

void ContentPlaybackController::SeekForward() {
  NotifyMediaSessionWhenActionIsSupported(MediaSessionAction::Seekforward);
}

void ContentPlaybackController::PreviousTrack() {
  NotifyMediaSessionWhenActionIsSupported(MediaSessionAction::Previoustrack);
}

void ContentPlaybackController::NextTrack() {
  NotifyMediaSessionWhenActionIsSupported(MediaSessionAction::Nexttrack);
}

void ContentPlaybackController::SkipAd() {
  NotifyMediaSessionWhenActionIsSupported(MediaSessionAction::Skipad);
}

void ContentPlaybackController::Stop() {
  const MediaSessionAction action = MediaSessionAction::Stop;
  if (IsMediaSessionActionSupported(action)) {
    NotifyMediaSession(action);
  } else {
    NotifyContentMediaControlKeyReceiver(MediaControlKey::Stop);
  }
}

void ContentPlaybackController::SeekTo(double aSeekTime, bool aFastSeek) {
  MediaSessionActionDetails details;
  details.mAction = MediaSessionAction::Seekto;
  details.mSeekTime.Construct(aSeekTime);
  if (aFastSeek) {
    details.mFastSeek.Construct(aFastSeek);
  }
  if (IsMediaSessionActionSupported(details.mAction)) {
    NotifyMediaSession(details);
  }
}

void ContentMediaControlKeyHandler::HandleMediaControlAction(
    BrowsingContext* aContext, const MediaControlAction& aAction) {
  ContentPlaybackController controller(aContext);
  switch (aAction.mKey) {
    case MediaControlKey::Focus:
      controller.Focus();
      return;
    case MediaControlKey::Play:
      controller.Play();
      return;
    case MediaControlKey::Pause:
      controller.Pause();
      return;
    case MediaControlKey::Stop:
      controller.Stop();
      return;
    case MediaControlKey::Previoustrack:
      controller.PreviousTrack();
      return;
    case MediaControlKey::Nexttrack:
      controller.NextTrack();
      return;
    case MediaControlKey::Seekbackward:
      controller.SeekBackward();
      return;
    case MediaControlKey::Seekforward:
      controller.SeekForward();
      return;
    case MediaControlKey::Skipad:
      controller.SkipAd();
      return;
    case MediaControlKey::Seekto: {
      const SeekDetails& details = *aAction.mDetails;
      controller.SeekTo(details.mSeekTime, details.mFastSeek);
      return;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid event.");
  };
}

}  // namespace dom
}  // namespace mozilla
