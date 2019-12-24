/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaControlService.h"

#include "MediaController.h"
#include "MediaControlUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsIObserverService.h"
#include "nsXULAppAPI.h"

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
    gMediaControlService->Init();
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

MediaControlService::MediaControlService() : mAudioFocusManager(this) {
  LOG("create media control service");
  RefPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown", false);
  }
}

void MediaControlService::Init() {
  mMediaKeysHandler = new MediaControlKeysHandler();
  mMediaControlKeysManager = new MediaControlKeysManager();
  mMediaControlKeysManager->Open();
  MOZ_ASSERT(mMediaControlKeysManager->IsOpened());
  mMediaControlKeysManager->AddListener(mMediaKeysHandler.get());
}

MediaControlService::~MediaControlService() {
  LOG("destroy media control service");
  Shutdown();
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
    Shutdown();
    sIsXPCOMShutdown = true;
    gMediaControlService = nullptr;
  }
  return NS_OK;
}

void MediaControlService::Shutdown() {
  ShutdownAllControllers();
  mControllers.Clear();
  mAudioFocusManager.Shutdown();
  mMediaControlKeysManager->RemoveListener(mMediaKeysHandler.get());
}

MediaController* MediaControlService::GetOrCreateControllerById(
    const uint64_t aId) const {
  MediaController* controller = GetControllerById(aId);
  if (!controller) {
    controller = new MediaController(aId);
  }
  return controller;
}

MediaController* MediaControlService::GetControllerById(
    const uint64_t aId) const {
  return mControllers.Get(aId).get();
}

void MediaControlService::AddMediaController(MediaController* aController) {
  MOZ_DIAGNOSTIC_ASSERT(aController);
  const uint64_t cId = aController->Id();
  MOZ_DIAGNOSTIC_ASSERT(!mControllers.GetValue(cId),
                        "Controller has been added already!");
  mControllers.Put(cId, aController);
  mControllerHistory.AppendElement(cId);
  LOG("Add media controller %" PRId64 ", currentNum=%" PRId64, cId,
      GetControllersNum());
  mMediaControllerAmountChangedEvent.Notify(GetControllersNum());
}

void MediaControlService::RemoveMediaController(MediaController* aController) {
  MOZ_DIAGNOSTIC_ASSERT(aController);
  const uint64_t cId = aController->Id();
  MOZ_DIAGNOSTIC_ASSERT(mControllers.GetValue(cId),
                        "Controller does not exist!");
  mControllers.Remove(cId);
  mControllerHistory.RemoveElement(cId);
  LOG("Remove media controller %" PRId64 ", currentNum=%" PRId64, cId,
      GetControllersNum());
  mMediaControllerAmountChangedEvent.Notify(GetControllersNum());
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

MediaController* MediaControlService::GetLastAddedController() const {
  if (mControllerHistory.IsEmpty()) {
    return nullptr;
  }
  return GetControllerById(mControllerHistory.LastElement());
}

void MediaControlService::GenerateMediaControlKeysTestEvent(
    MediaControlKeysEvent aEvent) {
  if (!StaticPrefs::media_mediacontrol_testingevents_enabled()) {
    return;
  }
  mMediaKeysHandler->OnKeyPressed(aEvent);
}

}  // namespace dom
}  // namespace mozilla
