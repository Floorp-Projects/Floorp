/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __HAL_WAKELOCK_H_
#define __HAL_WAKELOCK_H_

namespace mozilla {
namespace hal {

enum WakeLockState {
  WAKE_LOCK_STATE_UNLOCKED,
  WAKE_LOCK_STATE_HIDDEN,
  WAKE_LOCK_STATE_VISIBLE
};

/**
 * Return the wake lock state according to the numbers.
 */
WakeLockState ComputeWakeLockState(int aNumLocks, int aNumHidden);

} // hal
} // mozilla

#endif /* __HAL_WAKELOCK_H_ */
