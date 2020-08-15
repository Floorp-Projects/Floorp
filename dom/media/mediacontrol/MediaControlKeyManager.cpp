/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaControlKeyManager.h"

#include "MediaControlUtils.h"
#include "MediaControlService.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/widget/MediaKeysEventSourceFactory.h"

#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MediaControlKeyManager=%p, " msg, this, ##__VA_ARGS__))

#undef LOG_INFO
#define LOG_INFO(msg, ...)                  \
  MOZ_LOG(gMediaControlLog, LogLevel::Info, \
          ("MediaControlKeyManager=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

bool MediaControlKeyManager::IsOpened() const {
  // As MediaControlKeyManager represents a platform-indenpendent event source,
  // which we can use to add other listeners to moniter media key events, we
  // would always return true even if we fail to open the real media key event
  // source, because we still have chances to open the source again when there
  // are other new controllers being added.
  return true;
}

bool MediaControlKeyManager::Open() {
  mControllerAmountChangedListener =
      MediaControlService::GetService()
          ->MediaControllerAmountChangedEvent()
          .Connect(AbstractThread::MainThread(), this,
                   &MediaControlKeyManager::ControllerAmountChanged);
  return true;
}

MediaControlKeyManager::~MediaControlKeyManager() {
  StopMonitoringControlKeys();
  mEventSource = nullptr;
  mControllerAmountChangedListener.DisconnectIfExists();
}

void MediaControlKeyManager::StartMonitoringControlKeys() {
  if (!StaticPrefs::media_hardwaremediakeys_enabled()) {
    return;
  }

  if (!mEventSource) {
    mEventSource = widget::CreateMediaControlKeySource();
  }

  // When cross-compiling with MinGW, we cannot use the related WinAPI, thus
  // mEventSource might be null there.
  if (!mEventSource) {
    return;
  }

  LOG_INFO("StartMonitoringControlKeys");
  if (!mEventSource->IsOpened() && mEventSource->Open()) {
    mEventSource->SetPlaybackState(mPlaybackState);
    mEventSource->SetMediaMetadata(mMetadata);
    mEventSource->SetSupportedMediaKeys(mSupportedKeys);
    mEventSource->AddListener(this);
  }
}

void MediaControlKeyManager::StopMonitoringControlKeys() {
  if (mEventSource && mEventSource->IsOpened()) {
    LOG_INFO("StopMonitoringControlKeys");
    mEventSource->Close();
  }
}

void MediaControlKeyManager::ControllerAmountChanged(
    uint64_t aControllerAmount) {
  LOG("Controller amount changed=%" PRId64, aControllerAmount);
  if (aControllerAmount > 0) {
    StartMonitoringControlKeys();
  } else if (aControllerAmount == 0) {
    StopMonitoringControlKeys();
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

void MediaControlKeyManager::SetControlledTabBrowsingContextId(
    Maybe<uint64_t> aTopLevelBrowsingContextId) {
  if (aTopLevelBrowsingContextId) {
    LOG_INFO("Controlled tab Id=%" PRId64, *aTopLevelBrowsingContextId);
  } else {
    LOG_INFO("No controlled tab exists");
  }
  if (mEventSource && mEventSource->IsOpened()) {
    mEventSource->SetControlledTabBrowsingContextId(aTopLevelBrowsingContextId);
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

}  // namespace dom
}  // namespace mozilla
