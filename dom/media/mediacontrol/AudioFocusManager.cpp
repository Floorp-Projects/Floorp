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
#include "mozilla/Telemetry.h"
#include "nsThreadUtils.h"

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
  const bool hasManagedAudioFocus = ClearFocusControllersIfNeeded();
  LOG("Controller %" PRId64 " grants audio focus", aController->Id());
  mOwningFocusControllers.AppendElement(aController);
  if (hasManagedAudioFocus) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_TABS_AUDIO_COMPETITION::ManagedFocusByGecko);
  } else if (GetAudioFocusNums() == 1) {
    // Only one audible tab is playing within gecko.
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_TABS_AUDIO_COMPETITION::None);
  } else {
    // Multiple audible tabs are playing at the same time within gecko.
    CreateTimerForUpdatingTelemetry();
  }
}

void AudioFocusManager::RevokeAudioFocus(IMediaController* aController) {
  MOZ_ASSERT(aController);
  if (!mOwningFocusControllers.Contains(aController)) {
    return;
  }
  LOG("Controller %" PRId64 " loses audio focus", aController->Id());
  mOwningFocusControllers.RemoveElement(aController);
}

bool AudioFocusManager::ClearFocusControllersIfNeeded() {
  // Enable audio focus management will start the audio competition which is
  // only allowing one controller playing at a time.
  if (!StaticPrefs::media_audioFocus_management()) {
    return false;
  }

  bool hasStoppedAnyController = false;
  for (auto& controller : mOwningFocusControllers) {
    LOG("Controller %" PRId64 " loses audio focus in audio competitition",
        controller->Id());
    hasStoppedAnyController = true;
    controller->Stop();
  }
  mOwningFocusControllers.Clear();
  return hasStoppedAnyController;
}

uint32_t AudioFocusManager::GetAudioFocusNums() const {
  return mOwningFocusControllers.Length();
}

void AudioFocusManager::CreateTimerForUpdatingTelemetry() {
  MOZ_ASSERT(NS_IsMainThread());
  // Already create the timer.
  if (mTelemetryTimer) {
    return;
  }

  const uint32_t focusNum = GetAudioFocusNums();
  MOZ_ASSERT(focusNum > 1);

  RefPtr<MediaControlService> service = MediaControlService::GetService();
  MOZ_ASSERT(service);
  const uint32_t activeControllerNum = service->GetActiveControllersNum();

  // It takes time if users want to manually manage the audio competition by
  // pausing one of playing tabs. So we will check the status after a short
  // while to see if users handle the audio competition, or simply ignore it.
  nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction(
      "AudioFocusManager::RequestAudioFocus",
      [focusNum, activeControllerNum]() {
        if (RefPtr<MediaControlService> service =
                MediaControlService::GetService()) {
          service->GetAudioFocusManager().UpdateTelemetryDataFromTimer(
              focusNum, activeControllerNum);
        }
      });
  mTelemetryTimer =
      SimpleTimer::Create(task, 4000, GetMainThreadSerialEventTarget());
}

void AudioFocusManager::UpdateTelemetryDataFromTimer(
    uint32_t aPrevFocusNum, uint64_t aPrevActiveControllerNum) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mTelemetryTimer);
  // Users pause or mute tabs which decreases the amount of audible playing
  // tabs, which should not affect the total controller amount.
  if (GetAudioFocusNums() < aPrevFocusNum) {
    // If active controller amount is not equal, that means controllers got
    // deactivated by other reasons, such as reaching to the end, which are not
    // the situation we would like to accumulate for telemetry.
    if (MediaControlService::GetService()->GetActiveControllersNum() ==
        aPrevActiveControllerNum) {
      AccumulateCategorical(mozilla::Telemetry::LABELS_TABS_AUDIO_COMPETITION::
                                ManagedFocusByUser);
    }
  } else {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_TABS_AUDIO_COMPETITION::Ignored);
  }
  mTelemetryTimer = nullptr;
}

AudioFocusManager::~AudioFocusManager() {
  if (mTelemetryTimer) {
    mTelemetryTimer->Cancel();
    mTelemetryTimer = nullptr;
  }
}

}  // namespace mozilla::dom
