/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaController.h"

#include "MediaControlService.h"
#include "MediaControlUtils.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                                                    \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug,                             \
          ("MediaController=%p, Id=%" PRId64 ", " msg, this, this->Id(), \
           ##__VA_ARGS__))

namespace mozilla {
namespace dom {

already_AddRefed<BrowsingContext> MediaController::GetContext() const {
  return BrowsingContext::Get(mBrowsingContextId);
}

MediaController::MediaController(uint64_t aContextId)
    : mBrowsingContextId(aContextId) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(),
                        "MediaController only runs on Chrome process!");
  LOG("Create controller %" PRId64, Id());
}

MediaController::~MediaController() {
  LOG("Destroy controller %" PRId64, Id());
  MOZ_DIAGNOSTIC_ASSERT(!mControlledMediaNum);
};

void MediaController::Play() {
  LOG("Play");
  mIsPlaying = true;
  RefPtr<BrowsingContext> context = GetContext();
  if (context) {
    context->Canonical()->UpdateMediaAction(MediaControlActions::ePlay);
  }
}

void MediaController::Pause() {
  LOG("Pause");
  mIsPlaying = false;
  RefPtr<BrowsingContext> context = GetContext();
  if (context) {
    context->Canonical()->UpdateMediaAction(MediaControlActions::ePause);
  }
}

void MediaController::Stop() {
  LOG("Stop");
  mIsPlaying = false;
  RefPtr<BrowsingContext> context = GetContext();
  if (context) {
    context->Canonical()->UpdateMediaAction(MediaControlActions::eStop);
  }
}

void MediaController::Shutdown() {
  mIsPlaying = false;
  mControlledMediaNum = 0;
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  MOZ_ASSERT(service);
  service->GetAudioFocusManager().RevokeAudioFocus(Id());
}

void MediaController::NotifyMediaActiveChanged(bool aActive) {
  if (aActive) {
    IncreaseControlledMediaNum();
  } else {
    DecreaseControlledMediaNum();
  }
}

void MediaController::NotifyMediaAudibleChanged(bool aAudible) {
  mAudible = aAudible;
  if (mAudible) {
    RefPtr<MediaControlService> service = MediaControlService::GetService();
    MOZ_ASSERT(service);
    service->GetAudioFocusManager().RequestAudioFocus(Id());
  }
}

void MediaController::IncreaseControlledMediaNum() {
  MOZ_DIAGNOSTIC_ASSERT(mControlledMediaNum >= 0);
  mControlledMediaNum++;
  LOG("Increase controlled media num to %" PRId64, mControlledMediaNum);
  if (mControlledMediaNum == 1) {
    Activate();
  }
}

void MediaController::DecreaseControlledMediaNum() {
  MOZ_DIAGNOSTIC_ASSERT(mControlledMediaNum >= 1);
  mControlledMediaNum--;
  LOG("Decrease controlled media num to %" PRId64, mControlledMediaNum);
  if (mControlledMediaNum == 0) {
    Deactivate();
  }
}

// TODO : Use watchable to moniter mControlledMediaNum
void MediaController::Activate() {
  mIsPlaying = true;
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  MOZ_ASSERT(service);
  service->AddMediaController(this);
}

void MediaController::Deactivate() {
  mIsPlaying = false;
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  MOZ_ASSERT(service);
  service->RemoveMediaController(this);
  service->GetAudioFocusManager().RevokeAudioFocus(Id());
}

uint64_t MediaController::Id() const { return mBrowsingContextId; }

bool MediaController::IsPlaying() const { return mIsPlaying; }

bool MediaController::IsAudible() const { return mIsPlaying && mAudible; }

uint64_t MediaController::ControlledMediaNum() const {
  return mControlledMediaNum;
}

}  // namespace dom
}  // namespace mozilla
