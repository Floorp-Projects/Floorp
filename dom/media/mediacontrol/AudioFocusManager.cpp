/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioFocusManager.h"

#include "MediaControlService.h"

#include "mozilla/Logging.h"

extern mozilla::LazyLogModule gMediaControlLog;

#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("AudioFocusManager=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

AudioFocusManager::AudioFocusManager(MediaControlService* aService)
    : mService(aService) {}

void AudioFocusManager::RequestAudioFocus(uint64_t aId) {
  if (mOwningFocusControllers.Contains(aId)) {
    return;
  }

  LOG("Controller %" PRId64 " grants audio focus", aId);
  mOwningFocusControllers.AppendElement(aId);
  HandleAudioCompetition(aId);
}

void AudioFocusManager::RevokeAudioFocus(uint64_t aId) {
  if (!mOwningFocusControllers.Contains(aId)) {
    return;
  }

  LOG("Controller %" PRId64 " loses audio focus", aId);
  mOwningFocusControllers.RemoveElement(aId);
}

void AudioFocusManager::HandleAudioCompetition(uint64_t aId) {
  for (size_t idx = 0; idx < mOwningFocusControllers.Length(); idx++) {
    const uint64_t controllerId = mOwningFocusControllers[idx];
    if (controllerId != aId) {
      LOG("Controller %" PRId64 " loses audio focus in audio competitition",
          controllerId);
      RefPtr<MediaController> controller =
          mService->GetControllerById(controllerId);
      MOZ_ASSERT(controller);
      controller->Stop();
    }
  }

  mOwningFocusControllers.Clear();
  mOwningFocusControllers.AppendElement(aId);
}

uint32_t AudioFocusManager::GetAudioFocusNums() const {
  return mOwningFocusControllers.Length();
}

void AudioFocusManager::Shutdown() { mService = nullptr; }

}  // namespace dom
}  // namespace mozilla
