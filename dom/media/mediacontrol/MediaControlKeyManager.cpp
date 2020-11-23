/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaControlKeyManager.h"

#include "MediaControlUtils.h"
#include "MediaControlService.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/widget/MediaKeysEventSourceFactory.h"
#include "nsContentUtils.h"
#include "nsIObserverService.h"

#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MediaControlKeyManager=%p, " msg, this, ##__VA_ARGS__))

#undef LOG_INFO
#define LOG_INFO(msg, ...)                  \
  MOZ_LOG(gMediaControlLog, LogLevel::Info, \
          ("MediaControlKeyManager=%p, " msg, this, ##__VA_ARGS__))

#define MEDIA_CONTROL_PREF "media.hardwaremediakeys.enabled"

namespace mozilla::dom {

bool MediaControlKeyManager::IsOpened() const {
  return mEventSource && mEventSource->IsOpened();
}

bool MediaControlKeyManager::Open() {
  if (IsOpened()) {
    return true;
  }
  const bool isEnabledMediaControl = StartMonitoringControlKeys();
  if (isEnabledMediaControl) {
    RefPtr<MediaControlService> service = MediaControlService::GetService();
    MOZ_ASSERT(service);
    service->NotifyMediaControlHasEverBeenEnabled();
  }
  return isEnabledMediaControl;
}

void MediaControlKeyManager::Close() {
  // We don't call parent's `Close()` because we want to keep the listener
  // (MediaControlKeyHandler) all the time. It would be manually removed by
  // `MediaControlService` when shutdown.
  StopMonitoringControlKeys();
}

MediaControlKeyManager::MediaControlKeyManager()
    : mObserver(new Observer(this)) {
  nsContentUtils::RegisterShutdownObserver(mObserver);
  Preferences::AddStrongObserver(mObserver, MEDIA_CONTROL_PREF);
}

MediaControlKeyManager::~MediaControlKeyManager() { Shutdown(); }

void MediaControlKeyManager::Shutdown() {
  StopMonitoringControlKeys();
  mEventSource = nullptr;
  if (mObserver) {
    nsContentUtils::UnregisterShutdownObserver(mObserver);
    Preferences::RemoveObserver(mObserver, MEDIA_CONTROL_PREF);
    mObserver = nullptr;
  }
}

bool MediaControlKeyManager::StartMonitoringControlKeys() {
  if (!StaticPrefs::media_hardwaremediakeys_enabled()) {
    return false;
  }

  if (!mEventSource) {
    mEventSource = widget::CreateMediaControlKeySource();
  }
  if (mEventSource && mEventSource->Open()) {
    LOG_INFO("StartMonitoringControlKeys");
    mEventSource->SetPlaybackState(mPlaybackState);
    mEventSource->SetMediaMetadata(mMetadata);
    mEventSource->SetSupportedMediaKeys(mSupportedKeys);
    mEventSource->AddListener(this);
    return true;
  }
  // Fail to open or create event source (eg. when cross-compiling with MinGW,
  // we cannot use the related WinAPI)
  return false;
}

void MediaControlKeyManager::StopMonitoringControlKeys() {
  if (!mEventSource || !mEventSource->IsOpened()) {
    return;
  }

  LOG_INFO("StopMonitoringControlKeys");
  mEventSource->Close();
  if (StaticPrefs::media_mediacontrol_testingevents_enabled()) {
    // Close the source would reset the displayed playback state and metadata.
    if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
      obs->NotifyObservers(nullptr, "media-displayed-playback-changed",
                           nullptr);
      obs->NotifyObservers(nullptr, "media-displayed-metadata-changed",
                           nullptr);
    }
  }
}

void MediaControlKeyManager::OnActionPerformed(
    const MediaControlAction& aAction) {
  for (auto listener : mListeners) {
    listener->OnActionPerformed(aAction);
  }
}

void MediaControlKeyManager::SetPlaybackState(
    MediaSessionPlaybackState aState) {
  if (mEventSource && mEventSource->IsOpened()) {
    mEventSource->SetPlaybackState(aState);
  }
  mPlaybackState = aState;
  LOG_INFO("playbackState=%s", ToMediaSessionPlaybackStateStr(mPlaybackState));
  if (StaticPrefs::media_mediacontrol_testingevents_enabled()) {
    if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
      obs->NotifyObservers(nullptr, "media-displayed-playback-changed",
                           nullptr);
    }
  }
}

MediaSessionPlaybackState MediaControlKeyManager::GetPlaybackState() const {
  return (mEventSource && mEventSource->IsOpened())
             ? mEventSource->GetPlaybackState()
             : mPlaybackState;
}

void MediaControlKeyManager::SetMediaMetadata(
    const MediaMetadataBase& aMetadata) {
  if (mEventSource && mEventSource->IsOpened()) {
    mEventSource->SetMediaMetadata(aMetadata);
  }
  mMetadata = aMetadata;
  LOG_INFO("title=%s, artist=%s album=%s",
           NS_ConvertUTF16toUTF8(mMetadata.mTitle).get(),
           NS_ConvertUTF16toUTF8(mMetadata.mArtist).get(),
           NS_ConvertUTF16toUTF8(mMetadata.mAlbum).get());
  if (StaticPrefs::media_mediacontrol_testingevents_enabled()) {
    if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
      obs->NotifyObservers(nullptr, "media-displayed-metadata-changed",
                           nullptr);
    }
  }
}

void MediaControlKeyManager::SetSupportedMediaKeys(
    const MediaKeysArray& aSupportedKeys) {
  mSupportedKeys.Clear();
  for (const auto& key : aSupportedKeys) {
    LOG_INFO("Supported keys=%s", ToMediaControlKeyStr(key));
    mSupportedKeys.AppendElement(key);
  }
  if (mEventSource && mEventSource->IsOpened()) {
    mEventSource->SetSupportedMediaKeys(mSupportedKeys);
  }
}

void MediaControlKeyManager::SetEnableFullScreen(bool aIsEnabled) {
  LOG_INFO("Set fullscreen %s", aIsEnabled ? "enabled" : "disabled");
  if (mEventSource && mEventSource->IsOpened()) {
    mEventSource->SetEnableFullScreen(aIsEnabled);
  }
}

void MediaControlKeyManager::SetEnablePictureInPictureMode(bool aIsEnabled) {
  LOG_INFO("Set Picture-In-Picture mode %s",
           aIsEnabled ? "enabled" : "disabled");
  if (mEventSource && mEventSource->IsOpened()) {
    mEventSource->SetEnablePictureInPictureMode(aIsEnabled);
  }
}

void MediaControlKeyManager::SetPositionState(const PositionState& aState) {
  LOG_INFO("Set PositionState, duration=%f, playbackRate=%f, position=%f",
           aState.mDuration, aState.mPlaybackRate,
           aState.mLastReportedPlaybackPosition);
  if (mEventSource && mEventSource->IsOpened()) {
    mEventSource->SetPositionState(aState);
  }
}

void MediaControlKeyManager::OnPreferenceChange() {
  const bool isPrefEnabled = StaticPrefs::media_hardwaremediakeys_enabled();
  // Only start monitoring control keys when the pref is on and having a main
  // controller that means already having media which need to be controlled.
  const bool shouldMonitorKeys =
      isPrefEnabled && MediaControlService::GetService()->GetMainController();
  LOG_INFO("Preference change : %s media control",
           isPrefEnabled ? "enable" : "disable");
  if (shouldMonitorKeys) {
    Unused << StartMonitoringControlKeys();
  } else {
    StopMonitoringControlKeys();
  }
}

NS_IMPL_ISUPPORTS(MediaControlKeyManager::Observer, nsIObserver);

MediaControlKeyManager::Observer::Observer(MediaControlKeyManager* aManager)
    : mManager(aManager) {}

NS_IMETHODIMP
MediaControlKeyManager::Observer::Observe(nsISupports* aSubject,
                                          const char* aTopic,
                                          const char16_t* aData) {
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    mManager->Shutdown();
  } else if (!strcmp(aTopic, "nsPref:changed")) {
    mManager->OnPreferenceChange();
  }
  return NS_OK;
}

}  // namespace mozilla::dom
