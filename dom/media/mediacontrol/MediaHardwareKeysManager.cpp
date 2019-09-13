/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaHardwareKeysManager.h"

#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"

extern mozilla::LazyLogModule gMediaControlLog;

#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MediaHardwareKeysManager=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

MediaHardwareKeysManager::MediaHardwareKeysManager() {
  StartMonitoringHardwareKeys();
}

MediaHardwareKeysManager::~MediaHardwareKeysManager() {
  StopMonitoringHardwareKeys();
}

void MediaHardwareKeysManager::StartMonitoringHardwareKeys() {
  LOG("StartMonitoringHardwareKeys");
  CreateEventSource();
  if (mEventSource) {
    mEventSource->AddListener(new MediaHardwareKeysEventListener());
  }
}

void MediaHardwareKeysManager::CreateEventSource() {
  // TODO : create source per platform.
}

void MediaHardwareKeysManager::StopMonitoringHardwareKeys() {
  LOG("StopMonitoringHardwareKeys");
  if (mEventSource) {
    mEventSource->Close();
    mEventSource = nullptr;
  }
}

}  // namespace dom
}  // namespace mozilla
