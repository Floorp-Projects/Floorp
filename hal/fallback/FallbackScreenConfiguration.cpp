/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"

namespace mozilla::hal_impl {

RefPtr<GenericNonExclusivePromise> LockScreenOrientation(
    const hal::ScreenOrientation& aOrientation) {
  return GenericNonExclusivePromise::CreateAndReject(
      NS_ERROR_DOM_NOT_SUPPORTED_ERR, __func__);
}

void UnlockScreenOrientation() {}

}  // namespace mozilla::hal_impl
