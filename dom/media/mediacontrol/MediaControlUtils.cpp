/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaControlUtils.h"
#include "MediaControlService.h"

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ContentChild.h"
#include "nsGlobalWindowOuter.h"

mozilla::LazyLogModule gMediaControlLog("MediaControl");

#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MediaControlUtils, " msg, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

static RefPtr<BrowsingContext> GetBrowingContextByWindowID(uint64_t aWindowID) {
  RefPtr<nsGlobalWindowOuter> window =
      nsGlobalWindowOuter::GetOuterWindowWithId(aWindowID);
  if (!window) {
    return nullptr;
  }
  return window->GetBrowsingContext();
}

static void NotifyMediaActiveChanged(const RefPtr<BrowsingContext>& aBc,
                                     bool aActive) {
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyMediaActiveChanged(aBc, aActive);
  } else {
    MediaControlService::GetService()
        ->GetOrCreateControllerById(aBc->Id())
        ->NotifyMediaActiveChanged(aActive);
  }
}

void NotifyMediaStarted(uint64_t aWindowID) {
  RefPtr<BrowsingContext> bc = GetBrowingContextByWindowID(aWindowID);
  if (!bc || bc->IsDiscarded()) {
    return;
  }
  LOG("Notify media started in BC %" PRId64, bc->Id());
  bc = bc->Top();
  NotifyMediaActiveChanged(bc, true);
}

void NotifyMediaStopped(uint64_t aWindowID) {
  RefPtr<BrowsingContext> bc = GetBrowingContextByWindowID(aWindowID);
  if (!bc || bc->IsDiscarded()) {
    return;
  }
  LOG("Notify media stopped in BC %" PRId64, bc->Id());
  bc = bc->Top();
  NotifyMediaActiveChanged(bc, false);
}

void NotifyMediaAudibleChanged(uint64_t aWindowID, bool aAudible) {
  RefPtr<BrowsingContext> bc = GetBrowingContextByWindowID(aWindowID);
  if (!bc || bc->IsDiscarded()) {
    return;
  }
  LOG("Notify media became %s in BC %" PRId64,
      aAudible ? "audible" : "inaudible", bc->Id());
  bc = bc->Top();
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyMediaAudibleChanged(bc, aAudible);
  } else {
    RefPtr<MediaController> controller =
        MediaControlService::GetService()->GetControllerById(bc->Id());
    if (controller) {
      controller->NotifyMediaAudibleChanged(aAudible);
    }
  }
}

}  // namespace dom
}  // namespace mozilla
