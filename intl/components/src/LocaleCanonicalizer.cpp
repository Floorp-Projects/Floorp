/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/LocaleCanonicalizer.h"
#include <cstdio>
#include "unicode/uloc.h"

namespace mozilla::intl {

/* static */
ICUResult LocaleCanonicalizer::CanonicalizeICULevel1(
    const char* aLocaleIn, LocaleCanonicalizer::Vector& aLocaleOut) {
  auto result = FillBufferWithICUCall(
      aLocaleOut,
      [&aLocaleIn](char* target, int32_t length, UErrorCode* status) {
        return uloc_canonicalize(aLocaleIn, target, length, status);
      });

  if (result.isErr()) {
    return Err(result.unwrapErr());
  }

  // This step is not included in the normal ICU4C canonicalization step, but
  // consumers were expecting the results to actually be ASCII. It seemed safer
  // to include it.
  for (auto byte : aLocaleOut) {
    if (static_cast<unsigned char>(byte) > 127) {
      return Err(ICUError::InternalError);
    }
  }

  return Ok();
}

}  // namespace mozilla::intl
