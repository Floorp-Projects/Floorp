/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/Currency.h"

#include "unicode/ucurr.h"
#include "unicode/uenum.h"
#include "unicode/utypes.h"

namespace mozilla::intl {

Result<SpanEnumeration<char>, ICUError> Currency::GetISOCurrencies() {
  UErrorCode status = U_ZERO_ERROR;
  UEnumeration* enumeration = ucurr_openISOCurrencies(UCURR_ALL, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }
  return SpanEnumeration<char>(enumeration);
}

}  // namespace mozilla::intl
