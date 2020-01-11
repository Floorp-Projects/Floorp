/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaControlKeysEvent.h"

#include "MediaController.h"
#include "MediaControlUtils.h"
#include "MediaControlService.h"
#include "mozilla/Logging.h"

namespace mozilla {
namespace dom {

// avoid redefined macro in unified build
#undef LOG_SOURCE
#define LOG_SOURCE(msg, ...)                 \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MediaControlKeysEventSource=%p, " msg, this, ##__VA_ARGS__))

#undef LOG_KEY
#define LOG_KEY(msg, key, ...)                       \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug,         \
          ("MediaControlKeysHandler=%p, " msg, this, \
           ToMediaControlKeysEventStr(key), ##__VA_ARGS__));

void MediaControlKeysHandler::OnKeyPressed(MediaControlKeysEvent aKeyEvent) {
  LOG_KEY("OnKeyPressed '%s'", aKeyEvent);

  RefPtr<MediaControlService> service = MediaControlService::GetService();
  MOZ_ASSERT(service);
  RefPtr<MediaController> controller = service->GetMainController();
  if (!controller) {
    return;
  }

  const bool isControllerPlaying =
      controller->GetState() == PlaybackState::ePlaying;
  switch (aKeyEvent) {
    case MediaControlKeysEvent::ePlay:
      if (!isControllerPlaying) {
        controller->Play();
      }
      return;
    case MediaControlKeysEvent::ePause:
      if (isControllerPlaying) {
        controller->Pause();
      }
      return;
    case MediaControlKeysEvent::ePlayPause: {
      if (isControllerPlaying) {
        controller->Pause();
      } else {
        controller->Play();
      }
      return;
    }
    case MediaControlKeysEvent::ePrevTrack:
    case MediaControlKeysEvent::eNextTrack:
    case MediaControlKeysEvent::eSeekBackward:
    case MediaControlKeysEvent::eSeekForward:
      // TODO : implement related controller functions.
      return;
    case MediaControlKeysEvent::eStop:
      controller->Stop();
      return;
    default:
      MOZ_ASSERT_UNREACHABLE("Error : undefined event!");
      return;
  }
}

MediaControlKeysEventSource::MediaControlKeysEventSource()
    : mPlaybackState(PlaybackState::eStopped) {}

void MediaControlKeysEventSource::AddListener(
    MediaControlKeysEventListener* aListener) {
  MOZ_ASSERT(aListener);
  LOG_SOURCE("Add listener %p", aListener);
  mListeners.AppendElement(aListener);
}

void MediaControlKeysEventSource::RemoveListener(
    MediaControlKeysEventListener* aListener) {
  MOZ_ASSERT(aListener);
  LOG_SOURCE("Remove listener %p", aListener);
  mListeners.RemoveElement(aListener);
}

size_t MediaControlKeysEventSource::GetListenersNum() const {
  return mListeners.Length();
}

void MediaControlKeysEventSource::Close() {
  LOG_SOURCE("Close source");
  mListeners.Clear();
}

void MediaControlKeysEventSource::SetPlaybackState(PlaybackState aState) {
  if (mPlaybackState == aState) {
    return;
  }
  LOG_SOURCE("SetPlaybackState '%s'", ToPlaybackStateEventStr(aState));
  mPlaybackState = aState;
}

PlaybackState MediaControlKeysEventSource::GetPlaybackState() const {
  return mPlaybackState;
}

}  // namespace dom
}  // namespace mozilla
