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

#undef LOG_MAINCONTROLLER
#define LOG_MAINCONTROLLER(msg, ...) \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

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
  mControllerManager = MakeUnique<ControllerManager>();
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
  mControllerManager->Shutdown();
  mAudioFocusManager.Shutdown();
  mMediaControlKeysManager->RemoveListener(mMediaKeysHandler.get());
}

MediaController* MediaControlService::GetOrCreateControllerById(
    uint64_t aId) const {
  MediaController* controller = GetControllerById(aId);
  if (!controller) {
    controller = new MediaController(aId);
  }
  return controller;
}

MediaController* MediaControlService::GetControllerById(uint64_t aId) const {
  MOZ_DIAGNOSTIC_ASSERT(mControllerManager);
  return mControllerManager->GetControllerById(aId);
}

void MediaControlService::AddMediaController(MediaController* aController) {
  MOZ_DIAGNOSTIC_ASSERT(mControllerManager,
                        "Add controller before initializing service");
  mControllerManager->AddController(aController);
  LOG("Add media controller %" PRId64 ", currentNum=%" PRId64,
      aController->Id(), GetControllersNum());
  mMediaControllerAmountChangedEvent.Notify(GetControllersNum());
}

void MediaControlService::RemoveMediaController(MediaController* aController) {
  MOZ_DIAGNOSTIC_ASSERT(mControllerManager,
                        "Remove controller before initializing service");
  mControllerManager->RemoveController(aController);
  LOG("Remove media controller %" PRId64 ", currentNum=%" PRId64,
      aController->Id(), GetControllersNum());
  mMediaControllerAmountChangedEvent.Notify(GetControllersNum());
}

uint64_t MediaControlService::GetControllersNum() const {
  MOZ_DIAGNOSTIC_ASSERT(mControllerManager);
  return mControllerManager->GetControllersNum();
}

MediaController* MediaControlService::GetMainController() const {
  MOZ_DIAGNOSTIC_ASSERT(mControllerManager);
  return mControllerManager->GetMainController();
}

void MediaControlService::GenerateMediaControlKeysTestEvent(
    MediaControlKeysEvent aEvent) {
  if (!StaticPrefs::media_mediacontrol_testingevents_enabled()) {
    return;
  }
  mMediaKeysHandler->OnKeyPressed(aEvent);
}

// Following functions belong to ControllerManager
void MediaControlService::ControllerManager::AddController(
    MediaController* aController) {
  MOZ_DIAGNOSTIC_ASSERT(aController);
  MOZ_DIAGNOSTIC_ASSERT(!mControllers.GetValue(aController->Id()),
                        "Controller has been added already!");
  MOZ_DIAGNOSTIC_ASSERT(mControllers.Count() == mControllerHistory.Length());
  mControllers.Put(aController->Id(), aController);
  mControllerHistory.AppendElement(aController->Id());
  UpdateMainController(aController);
}

void MediaControlService::ControllerManager::RemoveController(
    MediaController* aController) {
  MOZ_DIAGNOSTIC_ASSERT(aController);
  MOZ_DIAGNOSTIC_ASSERT(mControllers.GetValue(aController->Id()),
                        "Controller does not exist!");
  MOZ_DIAGNOSTIC_ASSERT(mControllers.Count() == mControllerHistory.Length());
  mControllers.Remove(aController->Id());
  mControllerHistory.RemoveElement(aController->Id());
  if (mControllerHistory.IsEmpty()) {
    UpdateMainController(nullptr);
  } else {
    UpdateMainController(
        mControllers.Get(mControllerHistory.LastElement()).get());
  }
}

void MediaControlService::ControllerManager::Shutdown() {
  for (auto iter = mControllers.ConstIter(); !iter.Done(); iter.Next()) {
    iter.Data()->Shutdown();
  }
  mControllers.Clear();
}

void MediaControlService::ControllerManager::UpdateMainController(
    MediaController* aController) {
  mMainController = aController;
  if (!mMainController) {
    LOG_MAINCONTROLLER("Clear main controller");
    return;
  }
  LOG_MAINCONTROLLER("Set controller %" PRId64 " as main controller",
                     mMainController->Id());
  // TODO : monitor main controller's play state.
}

MediaController* MediaControlService::ControllerManager::GetMainController()
    const {
  return mMainController.get();
}

MediaController* MediaControlService::ControllerManager::GetControllerById(
    uint64_t aId) const {
  return mControllers.Get(aId).get();
}

uint64_t MediaControlService::ControllerManager::GetControllersNum() const {
  return mControllers.Count();
}

}  // namespace dom
}  // namespace mozilla
