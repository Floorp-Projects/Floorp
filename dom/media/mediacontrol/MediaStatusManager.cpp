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
  Maybe<uint64_t> oldAudioFocusOwnerId =
      mPlaybackStatusDelegate.GetAudioFocusOwnerContextId();
  mPlaybackStatusDelegate.UpdateMediaAudibleState(aBrowsingContextId, aState);
  Maybe<uint64_t> newAudioFocusOwnerId =
      mPlaybackStatusDelegate.GetAudioFocusOwnerContextId();
  if (oldAudioFocusOwnerId != newAudioFocusOwnerId) {
    HandleAudioFocusOwnerChanged(newAudioFocusOwnerId);
  }
}

void MediaStatusManager::NotifySessionCreated(uint64_t aBrowsingContextId) {
  if (mMediaSessionInfoMap.Contains(aBrowsingContextId)) {
    return;
  }

  LOG("Session %" PRIu64 " has been created", aBrowsingContextId);
  mMediaSessionInfoMap.Put(aBrowsingContextId, MediaSessionInfo::EmptyInfo());
  if (IsSessionOwningAudioFocus(aBrowsingContextId)) {
    SetActiveMediaSessionContextId(aBrowsingContextId);
  }
}

void MediaStatusManager::NotifySessionDestroyed(uint64_t aBrowsingContextId) {
  if (!mMediaSessionInfoMap.Contains(aBrowsingContextId)) {
    return;
  }
  LOG("Session %" PRIu64 " has been destroyed", aBrowsingContextId);
  mMediaSessionInfoMap.Remove(aBrowsingContextId);
  if (mActiveMediaSessionContextId &&
      *mActiveMediaSessionContextId == aBrowsingContextId) {
    ClearActiveMediaSessionContextIdIfNeeded();
  }
}

void MediaStatusManager::UpdateMetadata(
    uint64_t aBrowsingContextId, const Maybe<MediaMetadataBase>& aMetadata) {
  if (!mMediaSessionInfoMap.Contains(aBrowsingContextId)) {
    return;
  }

  MediaSessionInfo* info = mMediaSessionInfoMap.GetValue(aBrowsingContextId);
  if (IsMetadataEmpty(aMetadata)) {
    LOG("Reset metadata for session %" PRIu64, aBrowsingContextId);
    info->mMetadata.reset();
  } else {
    LOG("Update metadata for session %" PRIu64 " title=%s artist=%s album=%s",
        aBrowsingContextId, NS_ConvertUTF16toUTF8((*aMetadata).mTitle).get(),
        NS_ConvertUTF16toUTF8(aMetadata->mArtist).get(),
        NS_ConvertUTF16toUTF8(aMetadata->mAlbum).get());
    info->mMetadata = aMetadata;
  }
  // Only notify the event if the changed metadata belongs to the active media
  // session.
  if (!mActiveMediaSessionContextId ||
      *mActiveMediaSessionContextId == aBrowsingContextId) {
    mMetadataChangedEvent.Notify(GetCurrentMediaMetadata());
  }
  if (StaticPrefs::media_mediacontrol_testingevents_enabled()) {
    if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
      obs->NotifyObservers(nullptr, "media-session-controller-metadata-changed",
                           nullptr);
    }
  }
}

void MediaStatusManager::HandleAudioFocusOwnerChanged(
    Maybe<uint64_t>& aBrowsingContextId) {
  // No one is holding the audio focus.
  if (!aBrowsingContextId) {
    LOG("No one is owning audio focus");
    return ClearActiveMediaSessionContextIdIfNeeded();
  }

  // This owner of audio focus doesn't have media session, so we should deactive
  // the active session because the active session must own the audio focus.
  if (!mMediaSessionInfoMap.Contains(*aBrowsingContextId)) {
    LOG("The owner of audio focus doesn't have media session");
    return ClearActiveMediaSessionContextIdIfNeeded();
  }

  // This owner has media session so it should become an active session context.
  SetActiveMediaSessionContextId(*aBrowsingContextId);
}

void MediaStatusManager::SetActiveMediaSessionContextId(
    uint64_t aBrowsingContextId) {
  if (mActiveMediaSessionContextId &&
      *mActiveMediaSessionContextId == aBrowsingContextId) {
    LOG("Active session context %" PRIu64 " keeps unchanged",
        *mActiveMediaSessionContextId);
    return;
  }
  mActiveMediaSessionContextId = Some(aBrowsingContextId);
  LOG("context %" PRIu64 " becomes active session context",
      *mActiveMediaSessionContextId);
  mMetadataChangedEvent.Notify(GetCurrentMediaMetadata());
  mSupportedActionsChangedEvent.Notify(GetSupportedActions());
}

void MediaStatusManager::ClearActiveMediaSessionContextIdIfNeeded() {
  if (!mActiveMediaSessionContextId) {
    return;
  }
  LOG("Clear active session context");
  mActiveMediaSessionContextId.reset();
  mMetadataChangedEvent.Notify(GetCurrentMediaMetadata());
  mSupportedActionsChangedEvent.Notify(GetSupportedActions());
}

bool MediaStatusManager::IsSessionOwningAudioFocus(
    uint64_t aBrowsingContextId) const {
  Maybe<uint64_t> audioFocusContextId =
      mPlaybackStatusDelegate.GetAudioFocusOwnerContextId();
  return audioFocusContextId ? *audioFocusContextId == aBrowsingContextId
                             : false;
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
  // TODO : maybe need l10n? (bug1657701)
  nsString defaultTitle;
  defaultTitle.AssignLiteral("Firefox is playing media");

  RefPtr<CanonicalBrowsingContext> bc =
      CanonicalBrowsingContext::Get(mTopLevelBrowsingContextId);
  if (!bc) {
    return defaultTitle;
  }

  RefPtr<WindowGlobalParent> globalParent = bc->GetCurrentWindowGlobal();
  if (!globalParent) {
    return defaultTitle;
  }

  // The media metadata would be shown on the virtual controller interface. For
  // example, on Android, the interface would be shown on both notification bar
  // and lockscreen. Therefore, what information we provide via metadata is
  // quite important, because if we're in private browsing, we don't want to
  // expose details about what website the user is browsing on the lockscreen.
  // Therefore, using the default title when in the private browsing or the
  // document title is empty. Otherwise, use the document title.
  nsString documentTitle;
  if (!IsInPrivateBrowsing()) {
    globalParent->GetDocumentTitle(documentTitle);
  }
  return documentTitle.IsEmpty() ? defaultTitle : documentTitle;
}

nsString MediaStatusManager::GetDefaultFaviconURL() const {
#ifdef MOZ_PLACES
  nsCOMPtr<nsIURI> faviconURI;
  nsresult rv = NS_NewURI(getter_AddRefs(faviconURI),
                          nsLiteralCString(FAVICON_DEFAULT_URL));
  NS_ENSURE_SUCCESS(rv, u""_ns);

  // Convert URI from `chrome://XXX` to `file://XXX` because we would like to
  // let OS related frameworks, such as SMTC and MPRIS, handle this URL in order
  // to show the icon on virtual controller interface.
  nsCOMPtr<nsIChromeRegistry> regService = services::GetChromeRegistry();
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
  LOG("UpdateMediaPlaybackState %s for context %" PRIu64,
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

void MediaStatusManager::EnableAction(uint64_t aBrowsingContextId,
                                      MediaSessionAction aAction) {
  if (!mMediaSessionInfoMap.Contains(aBrowsingContextId)) {
    return;
  }
  MediaSessionInfo* info = mMediaSessionInfoMap.GetValue(aBrowsingContextId);
  if (info->IsActionSupported(aAction)) {
    LOG("Action '%s' has already been enabled for context %" PRIu64,
        ToMediaSessionActionStr(aAction), aBrowsingContextId);
    return;
  }
  LOG("Enable action %s for context %" PRIu64, ToMediaSessionActionStr(aAction),
      aBrowsingContextId);
  info->EnableAction(aAction);
  NotifySupportedKeysChangedIfNeeded(aBrowsingContextId);
}

void MediaStatusManager::DisableAction(uint64_t aBrowsingContextId,
                                       MediaSessionAction aAction) {
  if (!mMediaSessionInfoMap.Contains(aBrowsingContextId)) {
    return;
  }
  MediaSessionInfo* info = mMediaSessionInfoMap.GetValue(aBrowsingContextId);
  if (!info->IsActionSupported(aAction)) {
    LOG("Action '%s' hasn't been enabled yet for context %" PRIu64,
        ToMediaSessionActionStr(aAction), aBrowsingContextId);
    return;
  }
  LOG("Disable action %s for context %" PRIu64,
      ToMediaSessionActionStr(aAction), aBrowsingContextId);
  info->DisableAction(aAction);
  NotifySupportedKeysChangedIfNeeded(aBrowsingContextId);
}

void MediaStatusManager::UpdatePositionState(uint64_t aBrowsingContextId,
                                             const PositionState& aState) {
  // The position state comes from non-active media session which we don't care.
  if (!mActiveMediaSessionContextId ||
      *mActiveMediaSessionContextId != aBrowsingContextId) {
    return;
  }
  mPositionStateChangedEvent.Notify(aState);
}

void MediaStatusManager::NotifySupportedKeysChangedIfNeeded(
    uint64_t aBrowsingContextId) {
  // Only the active media session's supported actions would be shown in virtual
  // control interface, so we only notify the event when supported actions
  // change happens on the active media session.
  if (!mActiveMediaSessionContextId ||
      *mActiveMediaSessionContextId != aBrowsingContextId) {
    return;
  }
  mSupportedActionsChangedEvent.Notify(GetSupportedActions());
}

CopyableTArray<MediaSessionAction> MediaStatusManager::GetSupportedActions()
    const {
  CopyableTArray<MediaSessionAction> supportedActions;
  if (!mActiveMediaSessionContextId) {
    return supportedActions;
  }

  MediaSessionInfo info =
      mMediaSessionInfoMap.Get(*mActiveMediaSessionContextId);
  const uint8_t actionNums = uint8_t(MediaSessionAction::EndGuard_);
  for (uint8_t actionValue = 0; actionValue < actionNums; actionValue++) {
    MediaSessionAction action = ConvertToMediaSessionAction(actionValue);
    if (info.IsActionSupported(action)) {
      supportedActions.AppendElement(action);
    }
  }
  return supportedActions;
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

MediaSessionPlaybackState MediaStatusManager::PlaybackState() const {
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
