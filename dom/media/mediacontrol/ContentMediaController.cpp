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
#include "nsDataHashtable.h"
#include "nsGlobalWindowOuter.h"

namespace mozilla {
namespace dom {

using ControllerMap =
    nsDataHashtable<nsUint64HashKey, RefPtr<ContentMediaController>>;
static StaticAutoPtr<ControllerMap> sControllers;

#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("ContentMediaController=%p, " msg, this, ##__VA_ARGS__))

static already_AddRefed<ContentMediaController>
GetContentMediaControllerFromBrowsingContext(
    BrowsingContext* aBrowsingContext) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!sControllers) {
    sControllers = new ControllerMap();
    ClearOnShutdown(&sControllers);
  }

  RefPtr<BrowsingContext> topLevelBC =
      GetAliveTopBrowsingContext(aBrowsingContext);
  if (!topLevelBC) {
    return nullptr;
  }

  const uint64_t topLevelBCId = topLevelBC->Id();
  RefPtr<ContentMediaController> controller;
  if (!sControllers->Contains(topLevelBCId)) {
    controller = new ContentMediaController(topLevelBCId);
    sControllers->Put(topLevelBCId, controller);
  } else {
    controller = sControllers->Get(topLevelBCId);
  }
  return controller.forget();
}

/* static */
ContentControlKeyEventReceiver* ContentControlKeyEventReceiver::Get(
    BrowsingContext* aBC) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ContentMediaController> controller =
      GetContentMediaControllerFromBrowsingContext(aBC);
  return controller
             ? static_cast<ContentControlKeyEventReceiver*>(controller.get())
             : nullptr;
}

/* static */
ContentMediaAgent* ContentMediaAgent::Get(BrowsingContext* aBC) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ContentMediaController> controller =
      GetContentMediaControllerFromBrowsingContext(aBC);
  return controller ? static_cast<ContentMediaAgent*>(controller.get())
                    : nullptr;
}

void ContentMediaAgent::NotifyMediaPlaybackChanged(uint64_t aBrowsingContextId,
                                                   MediaPlaybackState aState) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<BrowsingContext> bc = BrowsingContext::Get(aBrowsingContextId);
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
  RefPtr<BrowsingContext> bc = BrowsingContext::Get(aBrowsingContextId);
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
  RefPtr<BrowsingContext> bc = BrowsingContext::Get(aBrowsingContextId);
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
  RefPtr<BrowsingContext> bc = BrowsingContext::Get(aBrowsingContextId);
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
  RefPtr<BrowsingContext> bc = BrowsingContext::Get(aBrowsingContextId);
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
  RefPtr<BrowsingContext> bc = BrowsingContext::Get(aBrowsingContextId);
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
  RefPtr<BrowsingContext> bc = BrowsingContext::Get(aBrowsingContextId);
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
  RefPtr<BrowsingContext> bc = BrowsingContext::Get(aBrowsingContextId);
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
  RefPtr<BrowsingContext> bc = BrowsingContext::Get(aBrowsingContextId);
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

ContentMediaController::ContentMediaController(uint64_t aId)
    : mTopLevelBrowsingContextId(aId) {}

void ContentMediaController::AddReceiver(
    ContentControlKeyEventReceiver* aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  mReceivers.AppendElement(aListener);
}

void ContentMediaController::RemoveReceiver(
    ContentControlKeyEventReceiver* aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  mReceivers.RemoveElement(aListener);
  // No more media needs to be controlled, so we can release this and recreate
  // it when someone needs it. We have to check `sControllers` because this can
  // be called via CC after we clear `sControllers`.
  if (mReceivers.IsEmpty() && sControllers) {
    sControllers->Remove(mTopLevelBrowsingContextId);
  }
}

void ContentMediaController::HandleEvent(MediaControlKeysEvent aEvent) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("Handle '%s' event, receiver num=%zu", ToMediaControlKeysEventStr(aEvent),
      mReceivers.Length());
  for (auto& receiver : mReceivers) {
    receiver->HandleEvent(aEvent);
  }
}

}  // namespace dom
}  // namespace mozilla
