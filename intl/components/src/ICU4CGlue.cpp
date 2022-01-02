/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/ICU4CGlue.h"
#include "unicode/uformattedvalue.h"

namespace mozilla::intl {

ICUError ToICUError(UErrorCode status) {
  MOZ_ASSERT(!U_SUCCESS(status));
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

// static
Result<Span<const char16_t>, ICUError> FormattedResult::ToSpanImpl(
    const UFormattedValue* value) {
  UErrorCode status = U_ZERO_ERROR;
  int32_t strLength;
  const char16_t* str = ufmtval_getString(value, &strLength, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  return Span{str, AssertedCast<size_t>(strLength)};
}

}  // namespace mozilla::intl
