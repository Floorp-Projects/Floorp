/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"

namespace mozilla::hal_impl {

RefPtr<mozilla::MozPromise<bool, bool, false>> LockScreenOrientation(
    const hal::ScreenOrientation& aOrientation) {
  return mozilla::MozPromise<bool, bool, false>::CreateAndReject(false,
                                                                 __func__);
}

void UnlockScreenOrientation() {}

}  // namespace mozilla::hal_impl
