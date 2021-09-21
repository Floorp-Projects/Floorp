/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/NumberingSystem.h"
#include "mozilla/intl/ICU4CGlue.h"

#include "unicode/unumsys.h"
#include "unicode/utypes.h"

namespace mozilla::intl {

NumberingSystem::~NumberingSystem() {
  MOZ_ASSERT(mNumberingSystem);
  unumsys_close(mNumberingSystem);
}

Result<UniquePtr<NumberingSystem>, ICUError> NumberingSystem::TryCreate(
    const char* aLocale) {
  UErrorCode status = U_ZERO_ERROR;
  UNumberingSystem* numbers = unumsys_open(IcuLocale(aLocale), &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  return MakeUnique<NumberingSystem>(numbers);
}

Result<Span<const char>, ICUError> NumberingSystem::GetName() {
  const char* name = unumsys_getName(mNumberingSystem);
  if (!name) {
    return Err(ICUError::InternalError);
  }

  return MakeStringSpan(name);
}

}  // namespace mozilla::intl
