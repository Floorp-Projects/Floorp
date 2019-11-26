/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaControlKeysManager.h"

#include "MediaControlUtils.h"
#include "MediaControlService.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_media.h"

#ifdef MOZ_APPLEMEDIA
#  include "MediaHardwareKeysEventSourceMac.h"
#endif

#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MediaControlKeysManager=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED0(MediaControlKeysManager,
                             MediaControlKeysEventSource)

void MediaControlKeysManager::Init() {
  mControllerAmountChangedListener =
      MediaControlService::GetService()
          ->MediaControllerAmountChangedEvent()
          .Connect(AbstractThread::MainThread(), this,
                   &MediaControlKeysManager::ControllerAmountChanged);
}

MediaControlKeysManager::~MediaControlKeysManager() {
  StopMonitoringControlKeys();
  mControllerAmountChangedListener.DisconnectIfExists();
}

void MediaControlKeysManager::StartMonitoringControlKeys() {
  LOG("StartMonitoringControlKeys");
  if (!StaticPrefs::media_hardwaremediakeys_enabled()) {
    return;
  }
  CreateEventSource();
}

void MediaControlKeysManager::CreateEventSource() {
#ifdef MOZ_APPLEMEDIA
  mEventSource = new MediaHardwareKeysEventSourceMac();
#endif
  if (mEventSource) {
    mEventSource->AddListener(this);
  }
}

void MediaControlKeysManager::StopMonitoringControlKeys() {
  LOG("StopMonitoringControlKeys");
  if (mEventSource) {
    mEventSource->Close();
    mEventSource = nullptr;
  }
}

void MediaControlKeysManager::ControllerAmountChanged(
    uint64_t aControllerAmount) {
  LOG("Controller amount changed=%" PRId64, aControllerAmount);
  if (aControllerAmount > 0 && !mEventSource) {
    StartMonitoringControlKeys();
  } else if (aControllerAmount == 0 && mEventSource) {
    StopMonitoringControlKeys();
  }
}

void MediaControlKeysManager::OnKeyPressed(MediaControlKeysEvent aKeyEvent) {
  for (auto listener : mListeners) {
    listener->OnKeyPressed(aKeyEvent);
  }
}

}  // namespace dom
}  // namespace mozilla
