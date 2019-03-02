/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TouchCounter.h"

#include "InputData.h"
#include "mozilla/TouchEvents.h"

namespace mozilla {
namespace layers {

TouchCounter::TouchCounter() : mActiveTouchCount(0) {}

void TouchCounter::Update(const MultiTouchInput& aInput) {
  switch (aInput.mType) {
    case MultiTouchInput::MULTITOUCH_START:
      // touch-start event contains all active touches of the current session
      mActiveTouchCount = aInput.mTouches.Length();
      break;
    case MultiTouchInput::MULTITOUCH_END:
      if (mActiveTouchCount >= aInput.mTouches.Length()) {
        // touch-end event contains only released touches
        mActiveTouchCount -= aInput.mTouches.Length();
      } else {
        NS_WARNING("Got an unexpected touchend");
        mActiveTouchCount = 0;
      }
      break;
    case MultiTouchInput::MULTITOUCH_CANCEL:
      mActiveTouchCount = 0;
      break;
    case MultiTouchInput::MULTITOUCH_MOVE:
      break;
  }
}

void TouchCounter::Update(const WidgetTouchEvent& aEvent) {
  switch (aEvent.mMessage) {
    case eTouchStart:
      // touch-start event contains all active touches of the current session
      mActiveTouchCount = aEvent.mTouches.Length();
      break;
    case eTouchEnd: {
      // touch-end contains all touches, but ones being lifted are marked as
      // changed
      uint32_t liftedTouches = 0;
      for (const auto& touch : aEvent.mTouches) {
        if (touch->mChanged) {
          liftedTouches++;
        }
      }
      if (mActiveTouchCount >= liftedTouches) {
        mActiveTouchCount -= liftedTouches;
      } else {
        NS_WARNING("Got an unexpected touchend");
        mActiveTouchCount = 0;
      }
      break;
    }
    case eTouchCancel:
      mActiveTouchCount = 0;
      break;
    default:
      break;
  }
}

uint32_t TouchCounter::GetActiveTouchCount() const { return mActiveTouchCount; }

}  // namespace layers
}  // namespace mozilla
