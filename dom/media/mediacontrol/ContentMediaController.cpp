/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentMediaController.h"

#include "MediaControlUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "nsGlobalWindowOuter.h"

namespace mozilla::dom {

#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("ContentMediaController=%p, " msg, this, ##__VA_ARGS__))

static Maybe<bool> sXPCOMShutdown;

static void InitXPCOMShutdownMonitor() {
  if (sXPCOMShutdown) {
    return;
  }
  sXPCOMShutdown.emplace(false);
  RunOnShutdown([&] { sXPCOMShutdown = Some(true); });
}

static ContentMediaController* GetContentMediaControllerFromBrowsingContext(
    BrowsingContext* aBrowsingContext) {
  MOZ_ASSERT(NS_IsMainThread());
  InitXPCOMShutdownMonitor();
  if (!aBrowsingContext || aBrowsingContext->IsDiscarded()) {
    return nullptr;
  }

  nsPIDOMWindowOuter* outer = aBrowsingContext->GetDOMWindow();
  if (!outer) {
    return nullptr;
  }

  nsGlobalWindowInner* inner =
      nsGlobalWindowInner::Cast(outer->GetCurrentInnerWindow());
  return inner ? inner->GetContentMediaController() : nullptr;
}

static already_AddRefed<BrowsingContext> GetBrowsingContextForAgent(
    uint64_t aBrowsingContextId) {
  // If XPCOM has been shutdown, then we're not able to access browsing context.
  if (sXPCOMShutdown && *sXPCOMShutdown) {
    return nullptr;
  }
  return BrowsingContext::Get(aBrowsingContextId);
}

/* static */
ContentMediaControlKeyReceiver* ContentMediaControlKeyReceiver::Get(
    BrowsingContext* aBC) {
  MOZ_ASSERT(NS_IsMainThread());
  return GetContentMediaControllerFromBrowsingContext(aBC);
}

/* static */
ContentMediaAgent* ContentMediaAgent::Get(BrowsingContext* aBC) {
  MOZ_ASSERT(NS_IsMainThread());
  return GetContentMediaControllerFromBrowsingContext(aBC);
}

void ContentMediaAgent::NotifyMediaPlaybackChanged(uint64_t aBrowsingContextId,
                                                   MediaPlaybackState aState) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<BrowsingContext> bc = GetBrowsingContextForAgent(aBrowsingContextId);
  if (!bc || bc->IsDiscarded()) {
    return;
  }

  LOG("Notify media %s in BC %" PRId64, ToMediaPlaybackStateStr(aState),
      bc->Id());
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyMediaPlaybackChanged(bc, aState);
  } else {
    // Currently this only happen when we disable e10s, otherwise all controlled
    // media would be run in the content process.
    if (RefPtr<IMediaInfoUpdater> updater =
            bc->Canonical()->GetMediaController()) {
      updater->NotifyMediaPlaybackChanged(bc->Id(), aState);
    }
  }
}

void ContentMediaAgent::NotifyMediaAudibleChanged(uint64_t aBrowsingContextId,
                                                  MediaAudibleState aState) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<BrowsingContext> bc = GetBrowsingContextForAgent(aBrowsingContextId);
  if (!bc || bc->IsDiscarded()) {
    return;
  }

  LOG("Notify media became %s in BC %" PRId64,
      aState == MediaAudibleState::eAudible ? "audible" : "inaudible",
      bc->Id());
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyMediaAudibleChanged(bc, aState);
  } else {
    // Currently this only happen when we disable e10s, otherwise all controlled
    // media would be run in the content process.
    if (RefPtr<IMediaInfoUpdater> updater =
            bc->Canonical()->GetMediaController()) {
      updater->NotifyMediaAudibleChanged(bc->Id(), aState);
    }
  }
}

void ContentMediaAgent::SetIsInPictureInPictureMode(
    uint64_t aBrowsingContextId, bool aIsInPictureInPictureMode) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<BrowsingContext> bc = GetBrowsingContextForAgent(aBrowsingContextId);
  if (!bc || bc->IsDiscarded()) {
    return;
  }

  LOG("Notify media Picture-in-Picture mode '%s' in BC %" PRId64,
      aIsInPictureInPictureMode ? "enabled" : "disabled", bc->Id());
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyPictureInPictureModeChanged(
        bc, aIsInPictureInPictureMode);
  } else {
    // Currently this only happen when we disable e10s, otherwise all controlled
    // media would be run in the content process.
    if (RefPtr<IMediaInfoUpdater> updater =
            bc->Canonical()->GetMediaController()) {
      updater->SetIsInPictureInPictureMode(bc->Id(), aIsInPictureInPictureMode);
    }
  }
}

void ContentMediaAgent::SetDeclaredPlaybackState(
    uint64_t aBrowsingContextId, MediaSessionPlaybackState aState) {
  RefPtr<BrowsingContext> bc = GetBrowsingContextForAgent(aBrowsingContextId);
  if (!bc || bc->IsDiscarded()) {
    return;
  }

  LOG("Notify declared playback state  '%s' in BC %" PRId64,
      ToMediaSessionPlaybackStateStr(aState), bc->Id());
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyMediaSessionPlaybackStateChanged(bc,
                                                                       aState);
    return;
  }
  // This would only happen when we disable e10s.
  if (RefPtr<IMediaInfoUpdater> updater =
          bc->Canonical()->GetMediaController()) {
    updater->SetDeclaredPlaybackState(bc->Id(), aState);
  }
}

void ContentMediaAgent::NotifySessionCreated(uint64_t aBrowsingContextId) {
  RefPtr<BrowsingContext> bc = GetBrowsingContextForAgent(aBrowsingContextId);
  if (!bc || bc->IsDiscarded()) {
    return;
  }

  LOG("Notify media session being created in BC %" PRId64, bc->Id());
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyMediaSessionUpdated(bc, true);
    return;
  }
  // This would only happen when we disable e10s.
  if (RefPtr<IMediaInfoUpdater> updater =
          bc->Canonical()->GetMediaController()) {
    updater->NotifySessionCreated(bc->Id());
  }
}

void ContentMediaAgent::NotifySessionDestroyed(uint64_t aBrowsingContextId) {
  RefPtr<BrowsingContext> bc = GetBrowsingContextForAgent(aBrowsingContextId);
  if (!bc || bc->IsDiscarded()) {
    return;
  }

  LOG("Notify media session being destroyed in BC %" PRId64, bc->Id());
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyMediaSessionUpdated(bc, false);
    return;
  }
  // This would only happen when we disable e10s.
  if (RefPtr<IMediaInfoUpdater> updater =
          bc->Canonical()->GetMediaController()) {
    updater->NotifySessionDestroyed(bc->Id());
  }
}

void ContentMediaAgent::UpdateMetadata(
    uint64_t aBrowsingContextId, const Maybe<MediaMetadataBase>& aMetadata) {
  RefPtr<BrowsingContext> bc = GetBrowsingContextForAgent(aBrowsingContextId);
  if (!bc || bc->IsDiscarded()) {
    return;
  }

  LOG("Notify media session metadata change in BC %" PRId64, bc->Id());
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyUpdateMediaMetadata(bc, aMetadata);
    return;
  }
  // This would only happen when we disable e10s.
  if (RefPtr<IMediaInfoUpdater> updater =
          bc->Canonical()->GetMediaController()) {
    updater->UpdateMetadata(bc->Id(), aMetadata);
  }
}

void ContentMediaAgent::EnableAction(uint64_t aBrowsingContextId,
                                     MediaSessionAction aAction) {
  RefPtr<BrowsingContext> bc = GetBrowsingContextForAgent(aBrowsingContextId);
  if (!bc || bc->IsDiscarded()) {
    return;
  }

  LOG("Notify to enable action '%s' in BC %" PRId64,
      ToMediaSessionActionStr(aAction), bc->Id());
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyMediaSessionSupportedActionChanged(
        bc, aAction, true);
    return;
  }
  // This would only happen when we disable e10s.
  if (RefPtr<IMediaInfoUpdater> updater =
          bc->Canonical()->GetMediaController()) {
    updater->EnableAction(bc->Id(), aAction);
  }
}

void ContentMediaAgent::DisableAction(uint64_t aBrowsingContextId,
                                      MediaSessionAction aAction) {
  RefPtr<BrowsingContext> bc = GetBrowsingContextForAgent(aBrowsingContextId);
  if (!bc || bc->IsDiscarded()) {
    return;
  }

  LOG("Notify to disable action '%s' in BC %" PRId64,
      ToMediaSessionActionStr(aAction), bc->Id());
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyMediaSessionSupportedActionChanged(
        bc, aAction, false);
    return;
  }
  // This would only happen when we disable e10s.
  if (RefPtr<IMediaInfoUpdater> updater =
          bc->Canonical()->GetMediaController()) {
    updater->DisableAction(bc->Id(), aAction);
  }
}

void ContentMediaAgent::NotifyMediaFullScreenState(uint64_t aBrowsingContextId,
                                                   bool aIsInFullScreen) {
  RefPtr<BrowsingContext> bc = GetBrowsingContextForAgent(aBrowsingContextId);
  if (!bc || bc->IsDiscarded()) {
    return;
  }

  LOG("Notify %s fullscreen in BC %" PRId64,
      aIsInFullScreen ? "entered" : "left", bc->Id());
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyMediaFullScreenState(bc, aIsInFullScreen);
    return;
  }
  // This would only happen when we disable e10s.
  if (RefPtr<IMediaInfoUpdater> updater =
          bc->Canonical()->GetMediaController()) {
    updater->NotifyMediaFullScreenState(bc->Id(), aIsInFullScreen);
  }
}

void ContentMediaAgent::UpdatePositionState(uint64_t aBrowsingContextId,
                                            const PositionState& aState) {
  RefPtr<BrowsingContext> bc = GetBrowsingContextForAgent(aBrowsingContextId);
  if (!bc || bc->IsDiscarded()) {
    return;
  }
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyPositionStateChanged(bc, aState);
    return;
  }
  // This would only happen when we disable e10s.
  if (RefPtr<IMediaInfoUpdater> updater =
          bc->Canonical()->GetMediaController()) {
    updater->UpdatePositionState(bc->Id(), aState);
  }
}

ContentMediaController::ContentMediaController(uint64_t aId) {
  LOG("Create content media controller for BC %" PRId64, aId);
}

void ContentMediaController::AddReceiver(
    ContentMediaControlKeyReceiver* aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  mReceivers.AppendElement(aListener);
}

void ContentMediaController::RemoveReceiver(
    ContentMediaControlKeyReceiver* aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  mReceivers.RemoveElement(aListener);
}

void ContentMediaController::HandleMediaKey(MediaControlKey aKey) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mReceivers.IsEmpty()) {
    return;
  }
  LOG("Handle '%s' event, receiver num=%zu", ToMediaControlKeyStr(aKey),
      mReceivers.Length());
  // We have default handlers for play, pause and stop.
  // https://w3c.github.io/mediasession/#ref-for-dom-mediasessionaction-play%E2%91%A3
  switch (aKey) {
    case MediaControlKey::Pause:
      PauseOrStopMedia();
      return;
    case MediaControlKey::Play:
      [[fallthrough]];
    case MediaControlKey::Stop:
      // When receiving `Stop`, the amount of receiver would vary during the
      // iteration, so we use the backward iteration to avoid accessing the
      // index which is over the array length.
      for (auto& receiver : Reversed(mReceivers)) {
        receiver->HandleMediaKey(aKey);
      }
      return;
    default:
      MOZ_ASSERT_UNREACHABLE("Not supported media key for default handler");
  }
}

void ContentMediaController::PauseOrStopMedia() {
  // When receiving `pause`, if a page contains playing media and paused media
  // at that moment, that means a user intends to pause those playing
  // media, not the already paused ones. Then, we're going to stop those already
  // paused media and keep those latest paused media in `mReceivers`.
  // The reason for doing that is, when resuming paused media, we only want to
  // resume latest paused media, not all media, in order to get a better user
  // experience, which matches Chrome's behavior.
  bool isAnyMediaPlaying = false;
  for (const auto& receiver : mReceivers) {
    if (receiver->IsPlaying()) {
      isAnyMediaPlaying = true;
      break;
    }
  }

  for (auto& receiver : Reversed(mReceivers)) {
    if (isAnyMediaPlaying && !receiver->IsPlaying()) {
      receiver->HandleMediaKey(MediaControlKey::Stop);
    } else {
      receiver->HandleMediaKey(MediaControlKey::Pause);
    }
  }
}

}  // namespace mozilla::dom
