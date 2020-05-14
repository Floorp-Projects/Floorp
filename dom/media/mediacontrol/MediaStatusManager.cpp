/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStatusManager.h"

#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/MediaControlUtils.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/StaticPrefs_media.h"
#include "nsIChromeRegistry.h"
#include "nsIObserverService.h"
#include "nsIXULAppInfo.h"

#ifdef MOZ_PLACES
#  include "nsIFaviconService.h"
#endif  // MOZ_PLACES

extern mozilla::LazyLogModule gMediaControlLog;

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MediaStatusManager=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

static bool IsMetadataEmpty(const Maybe<MediaMetadataBase>& aMetadata) {
  // Media session's metadata is null.
  if (!aMetadata) {
    return true;
  }

  // All attirbutes in metadata are empty.
  // https://w3c.github.io/mediasession/#empty-metadata
  const MediaMetadataBase& metadata = *aMetadata;
  return metadata.mTitle.IsEmpty() && metadata.mArtist.IsEmpty() &&
         metadata.mAlbum.IsEmpty() && metadata.mArtwork.IsEmpty();
}

MediaStatusManager::MediaStatusManager(uint64_t aBrowsingContextId)
    : mTopLevelBrowsingContextId(aBrowsingContextId) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(),
                        "MediaStatusManager only runs on Chrome process!");
}

void MediaStatusManager::NotifyMediaAudibleChanged(uint64_t aBrowsingContextId,
                                                   MediaAudibleState aState) {
  mPlaybackStatusDelegate.UpdateMediaAudibleState(aBrowsingContextId, aState);
}

void MediaStatusManager::NotifySessionCreated(uint64_t aBrowsingContextId) {
  if (mMediaSessionInfoMap.Contains(aBrowsingContextId)) {
    return;
  }

  LOG("Session %" PRId64 " has been created", aBrowsingContextId);
  mMediaSessionInfoMap.Put(aBrowsingContextId, MediaSessionInfo::EmptyInfo());
  UpdateActiveMediaSessionContextId();
}

void MediaStatusManager::NotifySessionDestroyed(uint64_t aBrowsingContextId) {
  if (!mMediaSessionInfoMap.Contains(aBrowsingContextId)) {
    return;
  }
  LOG("Session %" PRId64 " has been destroyed", aBrowsingContextId);
  mMediaSessionInfoMap.Remove(aBrowsingContextId);
  UpdateActiveMediaSessionContextId();
}

void MediaStatusManager::UpdateMetadata(
    uint64_t aBrowsingContextId, const Maybe<MediaMetadataBase>& aMetadata) {
  if (!mMediaSessionInfoMap.Contains(aBrowsingContextId)) {
    return;
  }

  MediaSessionInfo* info = mMediaSessionInfoMap.GetValue(aBrowsingContextId);
  if (IsMetadataEmpty(aMetadata)) {
    LOG("Reset metadata for session %" PRId64, aBrowsingContextId);
    info->mMetadata.reset();
  } else {
    LOG("Update metadata for session %" PRId64 " title=%s artist=%s album=%s",
        aBrowsingContextId, NS_ConvertUTF16toUTF8((*aMetadata).mTitle).get(),
        NS_ConvertUTF16toUTF8(aMetadata->mArtist).get(),
        NS_ConvertUTF16toUTF8(aMetadata->mAlbum).get());
    info->mMetadata = aMetadata;
  }
  mMetadataChangedEvent.Notify(GetCurrentMediaMetadata());
  if (StaticPrefs::media_mediacontrol_testingevents_enabled()) {
    if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
      obs->NotifyObservers(nullptr, "media-session-controller-metadata-changed",
                           nullptr);
    }
  }
}

void MediaStatusManager::UpdateActiveMediaSessionContextId() {
  // If there is a media session created from top level browsing context, it
  // would always be chosen as an active media session. Otherwise, we would
  // check if we have an active media session already. If we do have, it would
  // still remain as an active session. But if we don't, we would simply choose
  // arbitrary one as an active session.
  uint64_t candidateId = 0;
  if (mActiveMediaSessionContextId &&
      mMediaSessionInfoMap.Contains(*mActiveMediaSessionContextId)) {
    candidateId = *mActiveMediaSessionContextId;
  }

  for (auto iter = mMediaSessionInfoMap.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<BrowsingContext> bc = BrowsingContext::Get(iter.Key());
    // The browsing context which media session belongs to has been detroyed,
    // and it wasn't be removed correctly via the IPC message. That could happen
    // if the browsing context was destroyed before ContentPatent receives the
    // remove message. Therefore, we should remove it and continue to iterate
    // other elements.
    if (!bc) {
      iter.Remove();
      continue;
    }
    if (bc->IsTopContent()) {
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

MediaMetadataBase MediaStatusManager::CreateDefaultMetadata() const {
  MediaMetadataBase metadata;
  metadata.mTitle = GetDefaultTitle();
  metadata.mArtwork.AppendElement()->mSrc = GetDefaultFaviconURL();

  LOG("Default media metadata, title=%s, album src=%s",
      NS_ConvertUTF16toUTF8(metadata.mTitle).get(),
      NS_ConvertUTF16toUTF8(metadata.mArtwork[0].mSrc).get());
  return metadata;
}

nsString MediaStatusManager::GetDefaultTitle() const {
  RefPtr<CanonicalBrowsingContext> bc =
      CanonicalBrowsingContext::Get(mTopLevelBrowsingContextId);
  if (!bc) {
    return EmptyString();
  }

  RefPtr<WindowGlobalParent> globalParent = bc->GetCurrentWindowGlobal();
  if (!globalParent) {
    return EmptyString();
  }

  // The media metadata would be shown on the virtual controller interface. For
  // example, on Android, the interface would be shown on both notification bar
  // and lockscreen. Therefore, what information we provide via metadata is
  // quite important, because if we're in private browsing, we don't want to
  // expose details about what website the user is browsing on the lockscreen.
  nsString defaultTitle;
  if (IsInPrivateBrowsing()) {
    // TODO : maybe need l10n?
    if (nsCOMPtr<nsIXULAppInfo> appInfo =
            do_GetService("@mozilla.org/xre/app-info;1")) {
      nsCString appName;
      appInfo->GetName(appName);
      CopyUTF8toUTF16(appName, defaultTitle);
    } else {
      defaultTitle.AssignLiteral("Firefox");
    }
    defaultTitle.AppendLiteral(" is playing media");
  } else {
    globalParent->GetDocumentTitle(defaultTitle);
  }
  return defaultTitle;
}

nsString MediaStatusManager::GetDefaultFaviconURL() const {
#ifdef MOZ_PLACES
  nsCOMPtr<nsIURI> faviconURI;
  nsresult rv = NS_NewURI(getter_AddRefs(faviconURI),
                          NS_LITERAL_CSTRING(FAVICON_DEFAULT_URL));
  NS_ENSURE_SUCCESS(rv, NS_LITERAL_STRING(""));

  // Convert URI from `chrome://XXX` to `file://XXX` because we would like to
  // let OS related frameworks, such as SMTC and MPRIS, handle this URL in order
  // to show the icon on virtual controller interface.
  nsCOMPtr<nsIChromeRegistry> regService = services::GetChromeRegistryService();
  if (!regService) {
    return EmptyString();
  }
  nsCOMPtr<nsIURI> processedURI;
  regService->ConvertChromeURL(faviconURI, getter_AddRefs(processedURI));

  nsAutoCString spec;
  if (NS_FAILED(processedURI->GetSpec(spec))) {
    return EmptyString();
  }
  return NS_ConvertUTF8toUTF16(spec);
#endif
  return EmptyString();
}

void MediaStatusManager::SetDeclaredPlaybackState(
    uint64_t aBrowsingContextId, MediaSessionPlaybackState aState) {
  if (!mMediaSessionInfoMap.Contains(aBrowsingContextId)) {
    return;
  }
  MediaSessionInfo* info = mMediaSessionInfoMap.GetValue(aBrowsingContextId);
  LOG("SetDeclaredPlaybackState from %s to %s",
      ToMediaSessionPlaybackStateStr(info->mDeclaredPlaybackState),
      ToMediaSessionPlaybackStateStr(aState));
  info->mDeclaredPlaybackState = aState;
  UpdateActualPlaybackState();
}

MediaSessionPlaybackState MediaStatusManager::GetCurrentDeclaredPlaybackState()
    const {
  if (!mActiveMediaSessionContextId) {
    return MediaSessionPlaybackState::None;
  }
  return mMediaSessionInfoMap.Get(*mActiveMediaSessionContextId)
      .mDeclaredPlaybackState;
}

void MediaStatusManager::NotifyMediaPlaybackChanged(uint64_t aBrowsingContextId,
                                                    MediaPlaybackState aState) {
  LOG("UpdateMediaPlaybackState %s for context %" PRId64,
      ToMediaPlaybackStateStr(aState), aBrowsingContextId);
  const bool oldPlaying = mPlaybackStatusDelegate.IsPlaying();
  mPlaybackStatusDelegate.UpdateMediaPlaybackState(aBrowsingContextId, aState);

  // Playback state doesn't change, we don't need to update the guessed playback
  // state. This is used to prevent the state from changing from `none` to
  // `paused` when receiving `MediaPlaybackState::eStarted`.
  if (mPlaybackStatusDelegate.IsPlaying() == oldPlaying) {
    return;
  }
  if (mPlaybackStatusDelegate.IsPlaying()) {
    SetGuessedPlayState(MediaSessionPlaybackState::Playing);
  } else {
    SetGuessedPlayState(MediaSessionPlaybackState::Paused);
  }
}

void MediaStatusManager::SetGuessedPlayState(MediaSessionPlaybackState aState) {
  if (aState == mGuessedPlaybackState) {
    return;
  }
  LOG("SetGuessedPlayState : '%s'", ToMediaSessionPlaybackStateStr(aState));
  mGuessedPlaybackState = aState;
  UpdateActualPlaybackState();
}

void MediaStatusManager::UpdateActualPlaybackState() {
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
  HandleActualPlaybackStateChanged();
}

MediaMetadataBase MediaStatusManager::GetCurrentMediaMetadata() const {
  // If we don't have active media session, active media session doesn't have
  // media metadata, or we're in private browsing mode, then we should create a
  // default metadata which is using website's title and favicon as title and
  // artwork.
  if (mActiveMediaSessionContextId && !IsInPrivateBrowsing()) {
    MediaSessionInfo info =
        mMediaSessionInfoMap.Get(*mActiveMediaSessionContextId);
    if (!info.mMetadata) {
      return CreateDefaultMetadata();
    }
    MediaMetadataBase& metadata = *(info.mMetadata);
    FillMissingTitleAndArtworkIfNeeded(metadata);
    return metadata;
  }
  return CreateDefaultMetadata();
}

void MediaStatusManager::FillMissingTitleAndArtworkIfNeeded(
    MediaMetadataBase& aMetadata) const {
  // If the metadata doesn't set its title and artwork properly, we would like
  // to use default title and favicon instead in order to prevent showing
  // nothing on the virtual control interface.
  if (aMetadata.mTitle.IsEmpty()) {
    aMetadata.mTitle = GetDefaultTitle();
  }
  if (aMetadata.mArtwork.IsEmpty()) {
    aMetadata.mArtwork.AppendElement()->mSrc = GetDefaultFaviconURL();
  }
}

bool MediaStatusManager::IsInPrivateBrowsing() const {
  RefPtr<CanonicalBrowsingContext> bc =
      CanonicalBrowsingContext::Get(mTopLevelBrowsingContextId);
  if (!bc) {
    return false;
  }
  RefPtr<Element> element = bc->GetEmbedderElement();
  if (!element) {
    return false;
  }
  return nsContentUtils::IsInPrivateBrowsing(element->OwnerDoc());
}

MediaSessionPlaybackState MediaStatusManager::GetState() const {
  return mActualPlaybackState;
}

bool MediaStatusManager::IsMediaAudible() const {
  return mPlaybackStatusDelegate.IsAudible();
}

bool MediaStatusManager::IsMediaPlaying() const {
  return mActualPlaybackState == MediaSessionPlaybackState::Playing;
}

bool MediaStatusManager::IsAnyMediaBeingControlled() const {
  return mPlaybackStatusDelegate.IsAnyMediaBeingControlled();
}

}  // namespace dom
}  // namespace mozilla
