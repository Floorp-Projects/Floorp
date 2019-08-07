/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaControlService.h"

#include "MediaController.h"

#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsIObserverService.h"
#include "nsXULAppAPI.h"

extern mozilla::LazyLogModule gMediaControlLog;

#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MediaControlService=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

StaticRefPtr<MediaControlService> gMediaControlService;
static bool sIsXPCOMShutdown = false;

/* static */
RefPtr<MediaControlService> MediaControlService::GetService() {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(),
                        "MediaControlService only runs on Chrome process!");
  if (sIsXPCOMShutdown) {
    return nullptr;
  }
  if (!gMediaControlService) {
    gMediaControlService = new MediaControlService();
  }
  RefPtr<MediaControlService> service = gMediaControlService.get();
  return service;
}

NS_INTERFACE_MAP_BEGIN(MediaControlService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(MediaControlService)
NS_IMPL_RELEASE(MediaControlService)

MediaControlService::MediaControlService() {
  LOG("create media control service");
  RefPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown", false);
  }
}

MediaControlService::~MediaControlService() {
  LOG("destroy media control service");
  ShutdownAllControllers();
}

NS_IMETHODIMP
MediaControlService::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData) {
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    LOG("XPCOM shutdown");
    MOZ_ASSERT(gMediaControlService);
    RefPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "xpcom-shutdown");
    }
    ShutdownAllControllers();
    mControllers.Clear();
    sIsXPCOMShutdown = true;
    gMediaControlService = nullptr;
  }
  return NS_OK;
}

RefPtr<MediaController> MediaControlService::GetOrCreateControllerById(
    const uint64_t aId) const {
  RefPtr<MediaController> controller = mControllers.Get(aId);
  if (!controller) {
    controller = new TabMediaController(aId);
  }
  return controller;
}

RefPtr<MediaController> MediaControlService::GetControllerById(
    const uint64_t aId) const {
  return mControllers.Get(aId);
}

void MediaControlService::AddMediaController(
    const RefPtr<MediaController>& aController) {
  MOZ_DIAGNOSTIC_ASSERT(aController);
  const uint64_t cId = aController->Id();
  MOZ_DIAGNOSTIC_ASSERT(!mControllers.GetValue(cId),
                        "Controller has been added already!");
  mControllers.Put(cId, aController);
  LOG("Add media controller %" PRId64 ", currentNum=%" PRId64, cId,
      GetControllersNum());
}

void MediaControlService::RemoveMediaController(
    const RefPtr<MediaController>& aController) {
  MOZ_DIAGNOSTIC_ASSERT(aController);
  const uint64_t cId = aController->Id();
  MOZ_DIAGNOSTIC_ASSERT(mControllers.GetValue(cId),
                        "Controller does not exist!");
  mControllers.Remove(cId);
  LOG("Remove media controller %" PRId64 ", currentNum=%" PRId64, cId,
      GetControllersNum());
}

void MediaControlService::PlayAllControllers() const {
  for (auto iter = mControllers.ConstIter(); !iter.Done(); iter.Next()) {
    const RefPtr<MediaController>& controller = iter.Data();
    controller->Play();
  }
}

void MediaControlService::PauseAllControllers() const {
  for (auto iter = mControllers.ConstIter(); !iter.Done(); iter.Next()) {
    const RefPtr<MediaController>& controller = iter.Data();
    controller->Pause();
  }
}

void MediaControlService::StopAllControllers() const {
  for (auto iter = mControllers.ConstIter(); !iter.Done(); iter.Next()) {
    const RefPtr<MediaController>& controller = iter.Data();
    controller->Stop();
  }
}

void MediaControlService::ShutdownAllControllers() const {
  for (auto iter = mControllers.ConstIter(); !iter.Done(); iter.Next()) {
    const RefPtr<MediaController>& controller = iter.Data();
    controller->Shutdown();
  }
}

uint64_t MediaControlService::GetControllersNum() const {
  return mControllers.Count();
}

}  // namespace dom
}  // namespace mozilla
