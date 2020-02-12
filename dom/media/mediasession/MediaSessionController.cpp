/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaSessionController.h"

#include "mozilla/dom/BrowsingContext.h"

mozilla::LazyLogModule gMediaSession("MediaSession");

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                     \
  MOZ_LOG(gMediaSession, LogLevel::Debug, \
          ("MediaSessionController=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

void MediaSessionController::NotifySessionCreated(uint64_t aSessionContextId) {
  if (mMetadataMap.Contains(aSessionContextId)) {
    return;
  }
  Maybe<MediaMetadataBase> empty;
  LOG("Session %" PRId64 " has been created", aSessionContextId);
  mMetadataMap.Put(aSessionContextId, empty);
  UpdateActiveMediaSessionContextId();
}

void MediaSessionController::NotifySessionDestroyed(
    uint64_t aSessionContextId) {
  if (!mMetadataMap.Contains(aSessionContextId)) {
    return;
  }
  LOG("Session %" PRId64 " has been destroyed", aSessionContextId);
  mMetadataMap.Remove(aSessionContextId);
  UpdateActiveMediaSessionContextId();
}

void MediaSessionController::UpdateMetadata(uint64_t aSessionContextId,
                                            MediaMetadataBase& aMetadata) {
  if (!mMetadataMap.Contains(aSessionContextId)) {
    return;
  }
  LOG("Update metadata for session %" PRId64, aSessionContextId);
  mMetadataMap.GetValue(aSessionContextId)->emplace(aMetadata);
}

void MediaSessionController::UpdateActiveMediaSessionContextId() {
  // If there is a media session created from top level browsing context, it
  // would always be chosen as an active media session. Otherwise, we would
  // check if we have an active media session already. If we do have, it would
  // still remain as an active session. But if we don't, we would simply choose
  // arbitrary one as an active session.
  uint64_t candidateId = 0;
  if (mActiveMediaSessionContextId &&
      mMetadataMap.Contains(*mActiveMediaSessionContextId)) {
    candidateId = *mActiveMediaSessionContextId;
  }

  for (auto iter = mMetadataMap.ConstIter(); !iter.Done(); iter.Next()) {
    if (RefPtr<BrowsingContext> bc = BrowsingContext::Get(iter.Key());
        bc->IsTopContent()) {
      candidateId = iter.Key();
      break;
    }
    if (!candidateId) {
      candidateId = iter.Key();
    }
  }

  if (mActiveMediaSessionContextId &&
      *mActiveMediaSessionContextId == candidateId) {
    LOG("Active session %" PRId64 " keeps unchanged",
        *mActiveMediaSessionContextId);
    return;
  }

  mActiveMediaSessionContextId = Some(candidateId);
  LOG("Session %" PRId64 " becomes active session",
      *mActiveMediaSessionContextId);
}

MediaMetadataBase MediaSessionController::CreateDefaultMetadata() const {
  // TODO : using website's title and favicon as title and arwork to fill out
  // returned result in bug1611328.
  return MediaMetadataBase();
}

MediaMetadataBase MediaSessionController::GetCurrentMediaMetadata() const {
  // If we don't have active media session, or active media session doesn't have
  // media metadata, then we should create a default metadata which is using
  // website's title and favicon as artist name and album cover.
  if (mActiveMediaSessionContextId) {
    Maybe<MediaMetadataBase> metadata =
        mMetadataMap.Get(*mActiveMediaSessionContextId);
    return metadata ? *metadata : CreateDefaultMetadata();
  }
  return CreateDefaultMetadata();
}

}  // namespace dom
}  // namespace mozilla
