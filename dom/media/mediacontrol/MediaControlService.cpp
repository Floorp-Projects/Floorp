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

MediaControlService::MediaControlService() {
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
  mControllerManager = MakeUnique<ControllerManager>(this);
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
  mMediaControlKeysManager->RemoveListener(mMediaKeysHandler.get());
}

bool MediaControlService::RegisterActiveMediaController(
    MediaController* aController) {
  MOZ_DIAGNOSTIC_ASSERT(mControllerManager,
                        "Register controller before initializing service");
  if (!mControllerManager->AddController(aController)) {
    LOG("Fail to register controller %" PRId64, aController->Id());
    return false;
  }
  LOG("Register media controller %" PRId64 ", currentNum=%" PRId64,
      aController->Id(), GetActiveControllersNum());
  mMediaControllerAmountChangedEvent.Notify(GetActiveControllersNum());
  return true;
}

bool MediaControlService::UnregisterActiveMediaController(
    MediaController* aController) {
  MOZ_DIAGNOSTIC_ASSERT(mControllerManager,
                        "Unregister controller before initializing service");
  if (!mControllerManager->RemoveController(aController)) {
    LOG("Fail to unregister controller %" PRId64, aController->Id());
    return false;
  }
  LOG("Unregister media controller %" PRId64 ", currentNum=%" PRId64,
      aController->Id(), GetActiveControllersNum());
  mMediaControllerAmountChangedEvent.Notify(GetActiveControllersNum());
  return true;
}

uint64_t MediaControlService::GetActiveControllersNum() const {
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

MediaMetadataBase MediaControlService::GetMainControllerMediaMetadata() const {
  MediaMetadataBase metadata;
  if (!StaticPrefs::media_mediacontrol_testingevents_enabled()) {
    return metadata;
  }
  return GetMainController() ? GetMainController()->GetCurrentMediaMetadata()
                             : metadata;
}

MediaSessionPlaybackState MediaControlService::GetMainControllerPlaybackState()
    const {
  if (!StaticPrefs::media_mediacontrol_testingevents_enabled()) {
    return MediaSessionPlaybackState::None;
  }
  return GetMainController() ? GetMainController()->GetState()
                             : MediaSessionPlaybackState::None;
}

// Following functions belong to ControllerManager
MediaControlService::ControllerManager::ControllerManager(
    MediaControlService* aService)
    : mSource(aService->GetMediaControlKeysEventSource()) {
  MOZ_ASSERT(mSource);
}

bool MediaControlService::ControllerManager::AddController(
    MediaController* aController) {
  MOZ_DIAGNOSTIC_ASSERT(aController);
  if (mControllers.contains(aController)) {
    return false;
  }
  mControllers.insertBack(aController);
  UpdateMainController(aController);
  return true;
}

bool MediaControlService::ControllerManager::RemoveController(
    MediaController* aController) {
  MOZ_DIAGNOSTIC_ASSERT(aController);
  if (!mControllers.contains(aController)) {
    return false;
  }
  // This is LinkedListElement's method which will remove controller from
  // `mController`.
  aController->remove();
  UpdateMainController(mControllers.isEmpty() ? nullptr
                                              : mControllers.getLast());
  return true;
}

void MediaControlService::ControllerManager::Shutdown() {
  mControllers.clear();
  DisconnectMainControllerEvents();
}

void MediaControlService::ControllerManager::ControllerPlaybackStateChanged(
    MediaSessionPlaybackState aState) {
  MOZ_ASSERT(NS_IsMainThread());
  mSource->SetPlaybackState(aState);
  if (StaticPrefs::media_mediacontrol_testingevents_enabled()) {
    if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
      obs->NotifyObservers(nullptr, "main-media-controller-playback-changed",
                           nullptr);
    }
  }
}

void MediaControlService::ControllerManager::ControllerMetadataChanged(
    const MediaMetadataBase& aMetadata) {
  MOZ_ASSERT(NS_IsMainThread());
  mSource->SetMediaMetadata(aMetadata);
}

void MediaControlService::ControllerManager::UpdateMainController(
    MediaController* aController) {
  MOZ_ASSERT(NS_IsMainThread());
  mMainController = aController;

  // As main controller has been changed, we should disconnect the listener from
  // the previous controller and reconnect it to the new controller.
  DisconnectMainControllerEvents();

  if (!mMainController) {
    LOG_MAINCONTROLLER("Clear main controller");
    mSource->SetPlaybackState(MediaSessionPlaybackState::None);
  } else {
    LOG_MAINCONTROLLER("Set controller %" PRId64 " as main controller",
                       mMainController->Id());
    ConnectToMainControllerEvents();
  }
  if (StaticPrefs::media_mediacontrol_testingevents_enabled()) {
    if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
      obs->NotifyObservers(nullptr, "main-media-controller-changed", nullptr);
    }
  }
}

void MediaControlService::ControllerManager::ConnectToMainControllerEvents() {
  MOZ_ASSERT(mMainController);
  // Listen to main controller's event in order to get its playback state and
  // metadata update.
  mPlayStateChangedListener =
      mMainController->PlaybackStateChangedEvent().Connect(
          AbstractThread::MainThread(), this,
          &ControllerManager::ControllerPlaybackStateChanged);
  mMetadataChangedListener = mMainController->MetadataChangedEvent().Connect(
      AbstractThread::MainThread(), this,
      &ControllerManager::ControllerMetadataChanged);

  // Update controller's current status to the event source.
  mSource->SetPlaybackState(mMainController->GetState());
  mSource->SetMediaMetadata(mMainController->GetCurrentMediaMetadata());
}

void MediaControlService::ControllerManager::DisconnectMainControllerEvents() {
  mPlayStateChangedListener.DisconnectIfExists();
  mMetadataChangedListener.DisconnectIfExists();
}

MediaController* MediaControlService::ControllerManager::GetMainController()
    const {
  return mMainController.get();
}

uint64_t MediaControlService::ControllerManager::GetControllersNum() const {
  return mControllers.length();
}

}  // namespace dom
}  // namespace mozilla
