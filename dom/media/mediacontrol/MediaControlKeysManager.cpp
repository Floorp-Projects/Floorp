/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaControlKeysManager.h"

#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"

#ifdef MOZ_APPLEMEDIA
#  include "MediaHardwareKeysEventSourceMac.h"
#endif

extern mozilla::LazyLogModule gMediaControlLog;

#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MediaControlKeysManager=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

MediaControlKeysManager::MediaControlKeysManager() {
  StartMonitoringControlKeys();
}

MediaControlKeysManager::~MediaControlKeysManager() {
  StopMonitoringControlKeys();
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
}

void MediaControlKeysManager::StopMonitoringControlKeys() {
  LOG("StopMonitoringControlKeys");
  if (mEventSource) {
    mEventSource->Close();
    mEventSource = nullptr;
  }
}

bool MediaControlKeysManager::AddListener(
    MediaControlKeysEventListener* aListener) {
  if (!mEventSource) {
    LOG("No event source for adding a listener");
    return false;
  }
  mEventSource->AddListener(aListener);
  return true;
}

bool MediaControlKeysManager::RemoveListener(
    MediaControlKeysEventListener* aListener) {
  if (!mEventSource) {
    LOG("No event source for removing a listener");
    return false;
  }
  mEventSource->RemoveListener(aListener);
  return true;
}

}  // namespace dom
}  // namespace mozilla
