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

void ContentMediaController::NotifyPlaybackStateChanged(
    const ContentControlKeyEventReceiver* aMedia, MediaPlaybackState aState) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mReceivers.Contains(aMedia)) {
    return;
  }

  RefPtr<BrowsingContext> bc = aMedia->GetBrowsingContext();
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
    if (RefPtr<MediaController> controller =
            bc->Canonical()->GetMediaController()) {
      controller->NotifyMediaPlaybackChanged(aState);
    }
  }
}

void ContentMediaController::NotifyAudibleStateChanged(
    const ContentControlKeyEventReceiver* aMedia, MediaAudibleState aState) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mReceivers.Contains(aMedia)) {
    return;
  }

  RefPtr<BrowsingContext> bc = aMedia->GetBrowsingContext();
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
    if (RefPtr<MediaController> controller =
            bc->Canonical()->GetMediaController()) {
      controller->NotifyMediaAudibleChanged(aState);
    }
  }
}

void ContentMediaController::NotifyPictureInPictureModeChanged(
    const ContentControlKeyEventReceiver* aMedia, bool aEnabled) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mReceivers.Contains(aMedia)) {
    return;
  }

  RefPtr<BrowsingContext> bc = aMedia->GetBrowsingContext();
  if (!bc || bc->IsDiscarded()) {
    return;
  }

  LOG("Notify media Picture-in-Picture mode '%s' in BC %" PRId64,
      aEnabled ? "enabled" : "disabled", bc->Id());
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyPictureInPictureModeChanged(bc, aEnabled);
  } else {
    // Currently this only happen when we disable e10s, otherwise all controlled
    // media would be run in the content process.
    if (RefPtr<MediaController> controller =
            bc->Canonical()->GetMediaController()) {
      controller->SetIsInPictureInPictureMode(aEnabled);
    }
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
