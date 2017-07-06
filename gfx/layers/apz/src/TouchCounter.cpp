/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TouchCounter.h"

#include "InputData.h"

namespace mozilla {
namespace layers {

TouchCounter::TouchCounter()
  : mActiveTouchCount(0)
{
}

void
TouchCounter::Update(const MultiTouchInput& aInput)
{
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
        NS_WARNING("Got an unexpected touchend/touchcancel");
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

uint32_t
TouchCounter::GetActiveTouchCount() const
{
  return mActiveTouchCount;
}

} // namespace layers
} // namespace mozilla
