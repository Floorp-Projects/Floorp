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

void ContentMediaController::AddListener(
    MediaControlKeysEventListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  ContentMediaAgent::AddListener(aListener);
}

void ContentMediaController::RemoveListener(
    MediaControlKeysEventListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  ContentMediaAgent::RemoveListener(aListener);
  // No more media needs to be controlled, so we can release this and recreate
  // it when someone needs it.
  if (mListeners.IsEmpty()) {
    Close();
  }
}

void ContentMediaController::NotifyMediaStateChanged(
    const MediaControlKeysEventListener* aMedia, ControlledMediaState aState) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mListeners.Contains(aMedia)) {
    return;
  }

  RefPtr<BrowsingContext> bc = GetTopLevelBrowsingContext();
  if (!bc || bc->IsDiscarded()) {
    return;
  }

  LOG("Notify media %s in BC %" PRId64, ToControlledMediaStateStr(aState),
      bc->Id());
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyMediaStateChanged(bc, aState);
  } else {
    // Currently this only happen when we disable e10s, otherwise all controlled
    // media would be run in the content process.
    if (RefPtr<MediaController> controller =
            bc->Canonical()->GetMediaController()) {
      controller->NotifyMediaStateChanged(aState);
    }
  }
}

void ContentMediaController::NotifyAudibleStateChanged(
    const MediaControlKeysEventListener* aMedia, bool aAudible) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mListeners.Contains(aMedia)) {
    return;
  }

  RefPtr<BrowsingContext> bc = GetTopLevelBrowsingContext();
  if (!bc || bc->IsDiscarded()) {
    return;
  }

  LOG("Notify media became %s in BC %" PRId64,
      aAudible ? "audible" : "inaudible", bc->Id());
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyMediaAudibleChanged(bc, aAudible);
  } else {
    // Currently this only happen when we disable e10s, otherwise all controlled
    // media would be run in the content process.
    if (RefPtr<MediaController> controller =
            bc->Canonical()->GetMediaController()) {
      controller->NotifyMediaAudibleChanged(aAudible);
    }
  }
}

void ContentMediaController::NotifyPictureInPictureModeChanged(
    const MediaControlKeysEventListener* aMedia, bool aEnabled) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mListeners.Contains(aMedia)) {
    return;
  }

  RefPtr<BrowsingContext> bc = GetTopLevelBrowsingContext();
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

void ContentMediaController::OnKeyPressed(MediaControlKeysEvent aEvent) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("Handle '%s' event, listener num=%zu", ToMediaControlKeysEventStr(aEvent),
      mListeners.Length());
  for (auto& listener : mListeners) {
    listener->OnKeyPressed(aEvent);
  }
}

void ContentMediaController::Close() {
  MOZ_ASSERT(NS_IsMainThread());
  MediaControlKeysEventSource::Close();
  // `sControllers` might be null if ContentMediaController is detroyed after
  // freeing `sControllers`.
  if (sControllers) {
    sControllers->Remove(mTopLevelBrowsingContextId);
  }
}

already_AddRefed<BrowsingContext>
ContentMediaController::GetTopLevelBrowsingContext() const {
  if (!sControllers) {
    // `sControllers` would be destroyed when XPCOM is shutdown, which means
    // we are not able to access browsing context anymore.
    return nullptr;
  }
  return BrowsingContext::Get(mTopLevelBrowsingContextId);
}

}  // namespace dom
}  // namespace mozilla
