/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioFocusManager.h"

#include "MediaController.h"
#include "MediaControlUtils.h"
#include "MediaControlService.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_media.h"

#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("AudioFocusManager=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla::dom {

void AudioFocusManager::RequestAudioFocus(IMediaController* aController) {
  MOZ_ASSERT(aController);
  if (mOwningFocusControllers.Contains(aController)) {
    return;
  }
  ClearFocusControllersIfNeeded();
  LOG("Controller %" PRId64 " grants audio focus", aController->Id());
  mOwningFocusControllers.AppendElement(aController);
}

void AudioFocusManager::RevokeAudioFocus(IMediaController* aController) {
  MOZ_ASSERT(aController);
  if (!mOwningFocusControllers.Contains(aController)) {
    return;
  }
  LOG("Controller %" PRId64 " loses audio focus", aController->Id());
  mOwningFocusControllers.RemoveElement(aController);
}

void AudioFocusManager::ClearFocusControllersIfNeeded() {
  // Enable audio focus management will start the audio competition which is
  // only allowing one controller playing at a time.
  if (!StaticPrefs::media_audioFocus_management()) {
    return;
  }

  for (auto& controller : mOwningFocusControllers) {
    LOG("Controller %" PRId64 " loses audio focus in audio competitition",
        controller->Id());
    controller->Stop();
  }
  mOwningFocusControllers.Clear();
}

uint32_t AudioFocusManager::GetAudioFocusNums() const {
  return mOwningFocusControllers.Length();
}

}  // namespace mozilla::dom
