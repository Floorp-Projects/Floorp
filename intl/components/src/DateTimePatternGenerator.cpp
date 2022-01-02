/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/intl/DateTimePatternGenerator.h"

namespace mozilla::intl {

DateTimePatternGenerator::~DateTimePatternGenerator() {
  // The mGenerator will not exist when the DateTimePatternGenerator is being
  // moved.
  if (mGenerator) {
    udatpg_close(mGenerator.GetMut());
  }
}

/* static */
Result<UniquePtr<DateTimePatternGenerator>, ICUError>
DateTimePatternGenerator::TryCreate(const char* aLocale) {
  UErrorCode status = U_ZERO_ERROR;
  UDateTimePatternGenerator* generator =
      udatpg_open(IcuLocale(aLocale), &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }
  return MakeUnique<DateTimePatternGenerator>(generator);
};

DateTimePatternGenerator::DateTimePatternGenerator(
    DateTimePatternGenerator&& other) noexcept
    : mGenerator(other.mGenerator.GetMut()) {
  other.mGenerator = nullptr;
}

DateTimePatternGenerator& DateTimePatternGenerator::operator=(
    DateTimePatternGenerator&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  if (mGenerator) {
    udatpg_close(mGenerator.GetMut());
  }
  mGenerator = other.mGenerator.GetMut();
  other.mGenerator = nullptr;

  return *this;
}

}  // namespace mozilla::intl
