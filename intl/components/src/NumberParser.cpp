/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/intl/NumberParser.h"

namespace mozilla::intl {

/*static*/ Result<UniquePtr<NumberParser>, ICUError> NumberParser::TryCreate(
    const char* aLocale, bool aUseGrouping) {
  UniquePtr<NumberParser> nf = MakeUnique<NumberParser>();

  UErrorCode status = U_ZERO_ERROR;
  nf->mNumberFormat =
      unum_open(UNUM_DECIMAL, nullptr, 0, aLocale, nullptr, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  if (!aUseGrouping) {
    unum_setAttribute(nf->mNumberFormat.GetMut(), UNUM_GROUPING_USED, UBool(0));
  }

  return nf;
}

NumberParser::~NumberParser() {
  if (mNumberFormat) {
    unum_close(mNumberFormat.GetMut());
  }
}

Result<std::pair<double, int32_t>, ICUError> NumberParser::ParseDouble(
    Span<const char16_t> aDouble) const {
  UErrorCode status = U_ZERO_ERROR;
  int32_t parsePos = 0;
  double value = unum_parseDouble(mNumberFormat.GetConst(), aDouble.data(),
                                  static_cast<int32_t>(aDouble.size()),
                                  &parsePos, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }
  return std::make_pair(value, parsePos);
}

}  // namespace mozilla::intl
