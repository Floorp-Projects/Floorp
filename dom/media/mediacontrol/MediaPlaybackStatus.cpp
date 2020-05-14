/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaPlaybackStatus.h"

#include "MediaControlUtils.h"

namespace mozilla {
namespace dom {

#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MediaPlaybackStatus=%p, " msg, this, ##__VA_ARGS__))

void MediaPlaybackStatus::UpdateMediaPlaybackState(uint64_t aContextId,
                                                   MediaPlaybackState aState) {
  LOG("Update playback state '%s' for context %" PRIu64,
      ToMediaPlaybackStateStr(aState), aContextId);
  MOZ_ASSERT(NS_IsMainThread());

  ContextMediaInfo& info = GetNotNullContextInfo(aContextId);
  if (aState == MediaPlaybackState::eStarted) {
    info.IncreaseControlledMediaNum();
  } else if (aState == MediaPlaybackState::eStopped) {
    info.DecreaseControlledMediaNum();
  } else if (aState == MediaPlaybackState::ePlayed) {
    info.IncreasePlayingMediaNum();
  } else {
    MOZ_ASSERT(aState == MediaPlaybackState::ePaused);
    info.DecreasePlayingMediaNum();
  }

  // The context still has controlled media, we should keep its alive.
  if (info.IsAnyMediaBeingControlled()) {
    return;
  }
  MOZ_ASSERT(!info.IsPlaying());
  MOZ_ASSERT(!info.IsAudible());
  // DO NOT access `info` after this line.
  DestroyContextInfo(aContextId);
}

void MediaPlaybackStatus::DestroyContextInfo(uint64_t aContextId) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("Remove context %" PRIu64, aContextId);
  mContextInfoMap.Remove(aContextId);
}

void MediaPlaybackStatus::UpdateMediaAudibleState(uint64_t aContextId,
                                                  MediaAudibleState aState) {
  LOG("Update audible state '%s' for context %" PRIu64,
      ToMediaAudibleStateStr(aState), aContextId);
  MOZ_ASSERT(NS_IsMainThread());
  ContextMediaInfo& info = GetNotNullContextInfo(aContextId);
  if (aState == MediaAudibleState::eAudible) {
    info.IncreaseAudibleMediaNum();
  } else {
    MOZ_ASSERT(aState == MediaAudibleState::eInaudible);
    info.DecreaseAudibleMediaNum();
  }
}

bool MediaPlaybackStatus::IsPlaying() const {
  MOZ_ASSERT(NS_IsMainThread());
  for (auto iter = mContextInfoMap.ConstIter(); !iter.Done(); iter.Next()) {
    if (iter.Data()->IsPlaying()) {
      return true;
    }
  }
  return false;
}

bool MediaPlaybackStatus::IsAudible() const {
  MOZ_ASSERT(NS_IsMainThread());
  for (auto iter = mContextInfoMap.ConstIter(); !iter.Done(); iter.Next()) {
    if (iter.Data()->IsAudible()) {
      return true;
    }
  }
  return false;
}

bool MediaPlaybackStatus::IsAnyMediaBeingControlled() const {
  MOZ_ASSERT(NS_IsMainThread());
  for (auto iter = mContextInfoMap.ConstIter(); !iter.Done(); iter.Next()) {
    if (iter.Data()->IsAnyMediaBeingControlled()) {
      return true;
    }
  }
  return false;
}

MediaPlaybackStatus::ContextMediaInfo&
MediaPlaybackStatus::GetNotNullContextInfo(uint64_t aContextId) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mContextInfoMap.Contains(aContextId)) {
    mContextInfoMap.Put(aContextId, MakeUnique<ContextMediaInfo>(aContextId));
  }
  return *(mContextInfoMap.GetValue(aContextId)->get());
}

}  // namespace dom
}  // namespace mozilla
