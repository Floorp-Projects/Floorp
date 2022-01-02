/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaPlaybackStatus.h"

#include "MediaControlUtils.h"

namespace mozilla::dom {

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
  // If the removed context is owning the audio focus, we would find another
  // context to take the audio focus if it's possible.
  if (IsContextOwningAudioFocus(aContextId)) {
    ChooseNewContextToOwnAudioFocus();
  }
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
  if (ShouldRequestAudioFocusForInfo(info)) {
    SetOwningAudioFocusContextId(Some(aContextId));
  } else if (ShouldAbandonAudioFocusForInfo(info)) {
    ChooseNewContextToOwnAudioFocus();
  }
}

bool MediaPlaybackStatus::IsPlaying() const {
  MOZ_ASSERT(NS_IsMainThread());
  return std::any_of(mContextInfoMap.Values().cbegin(),
                     mContextInfoMap.Values().cend(),
                     [](const auto& info) { return info->IsPlaying(); });
}

bool MediaPlaybackStatus::IsAudible() const {
  MOZ_ASSERT(NS_IsMainThread());
  return std::any_of(mContextInfoMap.Values().cbegin(),
                     mContextInfoMap.Values().cend(),
                     [](const auto& info) { return info->IsAudible(); });
}

bool MediaPlaybackStatus::IsAnyMediaBeingControlled() const {
  MOZ_ASSERT(NS_IsMainThread());
  return std::any_of(
      mContextInfoMap.Values().cbegin(), mContextInfoMap.Values().cend(),
      [](const auto& info) { return info->IsAnyMediaBeingControlled(); });
}

MediaPlaybackStatus::ContextMediaInfo&
MediaPlaybackStatus::GetNotNullContextInfo(uint64_t aContextId) {
  MOZ_ASSERT(NS_IsMainThread());
  return *mContextInfoMap.GetOrInsertNew(aContextId, aContextId);
}

Maybe<uint64_t> MediaPlaybackStatus::GetAudioFocusOwnerContextId() const {
  return mOwningAudioFocusContextId;
}

void MediaPlaybackStatus::ChooseNewContextToOwnAudioFocus() {
  for (const auto& info : mContextInfoMap.Values()) {
    if (info->IsAudible()) {
      SetOwningAudioFocusContextId(Some(info->Id()));
      return;
    }
  }
  // No context is audible, so no one should the own audio focus.
  SetOwningAudioFocusContextId(Nothing());
}

void MediaPlaybackStatus::SetOwningAudioFocusContextId(
    Maybe<uint64_t>&& aContextId) {
  if (mOwningAudioFocusContextId == aContextId) {
    return;
  }
  mOwningAudioFocusContextId = aContextId;
}

bool MediaPlaybackStatus::ShouldRequestAudioFocusForInfo(
    const ContextMediaInfo& aInfo) const {
  return aInfo.IsAudible() && !IsContextOwningAudioFocus(aInfo.Id());
}

bool MediaPlaybackStatus::ShouldAbandonAudioFocusForInfo(
    const ContextMediaInfo& aInfo) const {
  // The owner becomes inaudible and there is other context still playing, so we
  // should switch the audio focus to the audible context.
  return !aInfo.IsAudible() && IsContextOwningAudioFocus(aInfo.Id()) &&
         IsAudible();
}

bool MediaPlaybackStatus::IsContextOwningAudioFocus(uint64_t aContextId) const {
  return mOwningAudioFocusContextId ? *mOwningAudioFocusContextId == aContextId
                                    : false;
}

}  // namespace mozilla::dom
