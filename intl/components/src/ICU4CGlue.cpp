/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/ICU4CGlue.h"

namespace mozilla::intl {

ICUError ToICUError(UErrorCode status) {
  if (status == U_MEMORY_ALLOCATION_ERROR) {
    return ICUError::OutOfMemory;
  }
  return ICUError::InternalError;
}

ICUResult ToICUResult(UErrorCode status) {
  if (U_SUCCESS(status)) {
    return Ok();
  }
  return Err(ToICUError(status));
}

}  // namespace mozilla::intl
