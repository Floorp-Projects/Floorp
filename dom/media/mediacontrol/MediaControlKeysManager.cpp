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
#include "mozilla/widget/MediaKeysEventSourceFactory.h"

#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MediaControlKeysManager=%p, " msg, this, ##__VA_ARGS__))

#undef LOG_INFO
#define LOG_INFO(msg, ...)                  \
  MOZ_LOG(gMediaControlLog, LogLevel::Info, \
          ("MediaControlKeysManager=%p, " msg, this, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

bool MediaControlKeysManager::IsOpened() const {
  // As MediaControlKeysManager represents a platform-indenpendent event source,
  // which we can use to add other listeners to moniter media key events, we
  // would always return true even if we fail to open the real media key event
  // source, because we still have chances to open the source again when there
  // are other new controllers being added.
  return true;
}

bool MediaControlKeysManager::Open() {
  mControllerAmountChangedListener =
      MediaControlService::GetService()
          ->MediaControllerAmountChangedEvent()
          .Connect(AbstractThread::MainThread(), this,
                   &MediaControlKeysManager::ControllerAmountChanged);
  return true;
}

MediaControlKeysManager::~MediaControlKeysManager() {
  StopMonitoringControlKeys();
  mEventSource = nullptr;
  mControllerAmountChangedListener.DisconnectIfExists();
}

void MediaControlKeysManager::StartMonitoringControlKeys() {
  if (!StaticPrefs::media_hardwaremediakeys_enabled()) {
    return;
  }

  if (!mEventSource) {
    mEventSource = widget::CreateMediaControlKeysEventSource();
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
    mEventSource->AddListener(this);
  }
}

void MediaControlKeysManager::StopMonitoringControlKeys() {
  if (mEventSource && mEventSource->IsOpened()) {
    LOG_INFO("StopMonitoringControlKeys");
    mEventSource->Close();
  }
}

void MediaControlKeysManager::ControllerAmountChanged(
    uint64_t aControllerAmount) {
  LOG("Controller amount changed=%" PRId64, aControllerAmount);
  if (aControllerAmount > 0) {
    StartMonitoringControlKeys();
  } else if (aControllerAmount == 0) {
    StopMonitoringControlKeys();
  }
}

void MediaControlKeysManager::OnKeyPressed(MediaControlKeysEvent aKeyEvent) {
  for (auto listener : mListeners) {
    listener->OnKeyPressed(aKeyEvent);
  }
}

void MediaControlKeysManager::SetPlaybackState(
    MediaSessionPlaybackState aState) {
  if (mEventSource && mEventSource->IsOpened()) {
    mEventSource->SetPlaybackState(aState);
  } else {
    // If the event source hasn't been created or been opened yet, we would
    // cache the state, and set it again when creating the event source.
    mPlaybackState = aState;
  }
}

MediaSessionPlaybackState MediaControlKeysManager::GetPlaybackState() const {
  return (mEventSource && mEventSource->IsOpened())
             ? mEventSource->GetPlaybackState()
             : mPlaybackState;
}

void MediaControlKeysManager::SetMediaMetadata(
    const MediaMetadataBase& aMetadata) {
  if (mEventSource && mEventSource->IsOpened()) {
    mEventSource->SetMediaMetadata(aMetadata);
  } else {
    // If the event source hasn't been created or been opened yet, we would
    // cache the state, and set it again when creating the event source.
    mMetadata = aMetadata;
  }
}

}  // namespace dom
}  // namespace mozilla
