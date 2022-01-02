/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaControlKeySource.h"

#include "MediaController.h"
#include "MediaControlUtils.h"
#include "MediaControlService.h"
#include "mozilla/Logging.h"

namespace mozilla::dom {

// avoid redefined macro in unified build
#undef LOG_SOURCE
#define LOG_SOURCE(msg, ...)                 \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MediaControlKeySource=%p, " msg, this, ##__VA_ARGS__))

#undef LOG_KEY
#define LOG_KEY(msg, key, ...)                                                 \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug,                                   \
          ("MediaControlKeyHandler=%p, " msg, this, ToMediaControlKeyStr(key), \
           ##__VA_ARGS__));

void MediaControlKeyHandler::OnActionPerformed(
    const MediaControlAction& aAction) {
  LOG_KEY("OnActionPerformed '%s'", aAction.mKey);

  RefPtr<MediaControlService> service = MediaControlService::GetService();
  MOZ_ASSERT(service);
  RefPtr<IMediaController> controller = service->GetMainController();
  if (!controller) {
    return;
  }

  switch (aAction.mKey) {
    case MediaControlKey::Focus:
      controller->Focus();
      return;
    case MediaControlKey::Play:
      controller->Play();
      return;
    case MediaControlKey::Pause:
      controller->Pause();
      return;
    case MediaControlKey::Playpause: {
      if (controller->IsPlaying()) {
        controller->Pause();
      } else {
        controller->Play();
      }
      return;
    }
    case MediaControlKey::Previoustrack:
      controller->PrevTrack();
      return;
    case MediaControlKey::Nexttrack:
      controller->NextTrack();
      return;
    case MediaControlKey::Seekbackward:
      controller->SeekBackward();
      return;
    case MediaControlKey::Seekforward:
      controller->SeekForward();
      return;
    case MediaControlKey::Skipad:
      controller->SkipAd();
      return;
    case MediaControlKey::Seekto: {
      const SeekDetails& details = *aAction.mDetails;
      controller->SeekTo(details.mSeekTime, details.mFastSeek);
      return;
    }
    case MediaControlKey::Stop:
      controller->Stop();
      return;
    default:
      MOZ_ASSERT_UNREACHABLE("Error : undefined media key!");
      return;
  }
}

MediaControlKeySource::MediaControlKeySource()
    : mPlaybackState(MediaSessionPlaybackState::None) {}

void MediaControlKeySource::AddListener(MediaControlKeyListener* aListener) {
  MOZ_ASSERT(aListener);
  LOG_SOURCE("Add listener %p", aListener);
  mListeners.AppendElement(aListener);
}

void MediaControlKeySource::RemoveListener(MediaControlKeyListener* aListener) {
  MOZ_ASSERT(aListener);
  LOG_SOURCE("Remove listener %p", aListener);
  mListeners.RemoveElement(aListener);
}

size_t MediaControlKeySource::GetListenersNum() const {
  return mListeners.Length();
}

void MediaControlKeySource::Close() {
  LOG_SOURCE("Close source");
  mListeners.Clear();
}

void MediaControlKeySource::SetPlaybackState(MediaSessionPlaybackState aState) {
  if (mPlaybackState == aState) {
    return;
  }
  LOG_SOURCE("SetPlaybackState '%s'", ToMediaSessionPlaybackStateStr(aState));
  mPlaybackState = aState;
}

MediaSessionPlaybackState MediaControlKeySource::GetPlaybackState() const {
  return mPlaybackState;
}

}  // namespace mozilla::dom
