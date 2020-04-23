/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaSessionController.h"

#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/StaticPrefs_media.h"
#include "nsIChromeRegistry.h"
#include "nsIObserverService.h"
#include "nsIXULAppInfo.h"

#ifdef MOZ_PLACES
#  include "nsIFaviconService.h"
#endif  // MOZ_PLACES

mozilla::LazyLogModule gMediaSession("MediaSession");

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                     \
  MOZ_LOG(gMediaSession, LogLevel::Debug, \
          ("MediaSessionController=%p, " msg, this, ##__VA_ARGS__))

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

MediaSessionController::MediaSessionController(uint64_t aContextId)
    : mTopLevelBCId(aContextId) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(),
                        "MediaSessionController only runs on Chrome process!");
}

void MediaSessionController::NotifySessionCreated(uint64_t aSessionContextId) {
  if (mMediaSessionInfoMap.Contains(aSessionContextId)) {
    return;
  }

  LOG("Session %" PRId64 " has been created", aSessionContextId);
  mMediaSessionInfoMap.Put(aSessionContextId, MediaSessionInfo::EmptyInfo());
  UpdateActiveMediaSessionContextId();
}

void MediaSessionController::NotifySessionDestroyed(
    uint64_t aSessionContextId) {
  if (!mMediaSessionInfoMap.Contains(aSessionContextId)) {
    return;
  }
  LOG("Session %" PRId64 " has been destroyed", aSessionContextId);
  mMediaSessionInfoMap.Remove(aSessionContextId);
  UpdateActiveMediaSessionContextId();
}

void MediaSessionController::UpdateMetadata(
    uint64_t aSessionContextId, const Maybe<MediaMetadataBase>& aMetadata) {
  if (!mMediaSessionInfoMap.Contains(aSessionContextId)) {
    return;
  }

  MediaSessionInfo* info = mMediaSessionInfoMap.GetValue(aSessionContextId);
  if (IsMetadataEmpty(aMetadata)) {
    LOG("Reset metadata for session %" PRId64, aSessionContextId);
    info->mMetadata.reset();
  } else {
    LOG("Update metadata for session %" PRId64 " title=%s artist=%s album=%s",
        aSessionContextId, NS_ConvertUTF16toUTF8((*aMetadata).mTitle).get(),
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

void MediaSessionController::UpdateActiveMediaSessionContextId() {
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

MediaMetadataBase MediaSessionController::CreateDefaultMetadata() const {
  MediaMetadataBase metadata;
  metadata.mTitle = GetDefaultTitle();
  metadata.mArtwork.AppendElement()->mSrc = GetDefaultFaviconURL();

  LOG("Default media metadata, title=%s, album src=%s",
      NS_ConvertUTF16toUTF8(metadata.mTitle).get(),
      NS_ConvertUTF16toUTF8(metadata.mArtwork[0].mSrc).get());
  return metadata;
}

nsString MediaSessionController::GetDefaultTitle() const {
  RefPtr<CanonicalBrowsingContext> bc =
      CanonicalBrowsingContext::Get(mTopLevelBCId);
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
    defaultTitle = globalParent->GetDocumentTitle();
  }
  return defaultTitle;
}

nsString MediaSessionController::GetDefaultFaviconURL() const {
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

void MediaSessionController::SetDeclaredPlaybackState(
    uint64_t aSessionContextId, MediaSessionPlaybackState aState) {
  if (!mMediaSessionInfoMap.Contains(aSessionContextId)) {
    return;
  }
  MediaSessionInfo* info = mMediaSessionInfoMap.GetValue(aSessionContextId);
  LOG("SetDeclaredPlaybackState from %s to %s",
      ToMediaSessionPlaybackStateStr(info->mDeclaredPlaybackState),
      ToMediaSessionPlaybackStateStr(aState));
  info->mDeclaredPlaybackState = aState;
}

MediaSessionPlaybackState
MediaSessionController::GetCurrentDeclaredPlaybackState() const {
  if (!mActiveMediaSessionContextId) {
    return MediaSessionPlaybackState::None;
  }
  return mMediaSessionInfoMap.Get(*mActiveMediaSessionContextId)
      .mDeclaredPlaybackState;
}

MediaMetadataBase MediaSessionController::GetCurrentMediaMetadata() const {
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

void MediaSessionController::FillMissingTitleAndArtworkIfNeeded(
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

bool MediaSessionController::IsInPrivateBrowsing() const {
  RefPtr<CanonicalBrowsingContext> bc =
      CanonicalBrowsingContext::Get(mTopLevelBCId);
  if (!bc) {
    return false;
  }
  RefPtr<Element> element = bc->GetEmbedderElement();
  if (!element) {
    return false;
  }
  return nsContentUtils::IsInPrivateBrowsing(element->OwnerDoc());
}

}  // namespace dom
}  // namespace mozilla
