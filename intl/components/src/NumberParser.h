/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_NumberParser_h_
#define intl_components_NumberParser_h_

#include "mozilla/intl/ICUError.h"
#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"

#include "unicode/unum.h"

namespace mozilla::intl {

class NumberParser {
 public:
  /**
   * Initialize a new NumberParser for the provided locale and using the
   * provided options.
   */
  static Result<UniquePtr<NumberParser>, ICUError> TryCreate(
      const char* aLocale, bool aUseGrouping);

  NumberParser() : mNumberFormat(nullptr){};
  NumberParser(const NumberParser&) = delete;
  NumberParser& operator=(const NumberParser&) = delete;
  ~NumberParser();

  /**
   * Attempts to parse a string representing a double, returning the parsed
   * double and the parse position if successful, or an error.
   *
   * The parse position is the index into the input string where parsing
   * stopped because an non-numeric character was encountered.
   */
  Result<std::pair<double, int32_t>, ICUError> ParseDouble(
      Span<const char16_t> aDouble) const;

 private:
  ICUPointer<UNumberFormat> mNumberFormat;
};

}  // namespace mozilla::intl

#endif
