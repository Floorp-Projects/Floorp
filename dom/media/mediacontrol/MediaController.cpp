/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaController.h"

#include "MediaControlService.h"
#include "mozilla/dom/BrowsingContext.h"

extern mozilla::LazyLogModule gMediaControlLog;

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                                                       \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug,                                \
          ("TabMediaController=%p, Id=%" PRId64 ", " msg, this, this->Id(), \
           ##__VA_ARGS__))

namespace mozilla {
namespace dom {

already_AddRefed<BrowsingContext> MediaController::GetContext() const {
  return BrowsingContext::Get(mBrowsingContextId);
}

TabMediaController::TabMediaController(uint64_t aContextId)
    : MediaController(aContextId) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(),
                        "MediaController only runs on Chrome process!");
  LOG("Create controller %" PRId64, Id());
}

TabMediaController::~TabMediaController() {
  LOG("Destroy controller %" PRId64, Id());
  MOZ_DIAGNOSTIC_ASSERT(!mControlledMediaNum);
};

void TabMediaController::Play() {
  LOG("Play");
  mIsPlaying = true;
}

void TabMediaController::Pause() {
  LOG("Pause");
  mIsPlaying = false;
}

void TabMediaController::Stop() {
  LOG("Stop");
  mIsPlaying = false;
}

void TabMediaController::Shutdown() {
  mIsPlaying = false;
  mControlledMediaNum = 0;
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  MOZ_ASSERT(service);
  service->GetAudioFocusManager().RevokeAudioFocus(Id());
}

bool TabMediaController::IsAudible() const { return mIsPlaying && mAudible; }

void TabMediaController::NotifyMediaActiveChanged(bool aActive) {
  if (aActive) {
    IncreaseControlledMediaNum();
  } else {
    DecreaseControlledMediaNum();
  }
}

void TabMediaController::NotifyMediaAudibleChanged(bool aAudible) {
  mAudible = aAudible;
  if (mAudible) {
    RefPtr<MediaControlService> service = MediaControlService::GetService();
    MOZ_ASSERT(service);
    service->GetAudioFocusManager().RequestAudioFocus(Id());
  }
}

void TabMediaController::IncreaseControlledMediaNum() {
  MOZ_DIAGNOSTIC_ASSERT(mControlledMediaNum >= 0);
  mControlledMediaNum++;
  LOG("Increase controlled media num to %" PRId64, mControlledMediaNum);
  if (mControlledMediaNum == 1) {
    Activate();
  }
}

void TabMediaController::DecreaseControlledMediaNum() {
  MOZ_DIAGNOSTIC_ASSERT(mControlledMediaNum >= 1);
  mControlledMediaNum--;
  LOG("Decrease controlled media num to %" PRId64, mControlledMediaNum);
  if (mControlledMediaNum == 0) {
    Deactivate();
  }
}

// TODO : Use watchable to moniter mControlledMediaNum
void TabMediaController::Activate() {
  mIsPlaying = true;
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  MOZ_ASSERT(service);
  service->AddMediaController(this);
}

void TabMediaController::Deactivate() {
  mIsPlaying = false;
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  MOZ_ASSERT(service);
  service->RemoveMediaController(this);
  service->GetAudioFocusManager().RevokeAudioFocus(Id());
}

uint64_t TabMediaController::ControlledMediaNum() const {
  return mControlledMediaNum;
}

}  // namespace dom
}  // namespace mozilla
