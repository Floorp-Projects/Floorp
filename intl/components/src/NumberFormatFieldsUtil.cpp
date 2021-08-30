/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "NumberFormatFieldsUtil.h"

#include "mozilla/FloatingPoint.h"

namespace mozilla::intl {

// See intl/icu/source/i18n/unicode/unum.h for a detailed field list.  This
// list is deliberately exhaustive: cases might have to be added/removed if
// this code is compiled with a different ICU with more UNumberFormatFields
// enum initializers.  Please guard such cases with appropriate ICU
// version-testing #ifdefs, should cross-version divergence occur.
Maybe<NumberPartType> GetPartTypeForNumberField(UNumberFormatFields fieldName,
                                                Maybe<double> number,
                                                bool isNegative,
                                                bool formatForUnit) {
  switch (fieldName) {
    case UNUM_INTEGER_FIELD:
      if (number.isSome()) {
        if (IsNaN(*number)) {
          return Some(NumberPartType::Nan);
        }
        if (!IsFinite(*number)) {
          return Some(NumberPartType::Infinity);
        }
      }
      return Some(NumberPartType::Integer);
    case UNUM_FRACTION_FIELD:
      return Some(NumberPartType::Fraction);
    case UNUM_DECIMAL_SEPARATOR_FIELD:
      return Some(NumberPartType::Decimal);
    case UNUM_EXPONENT_SYMBOL_FIELD:
      return Some(NumberPartType::ExponentSeparator);
    case UNUM_EXPONENT_SIGN_FIELD:
      return Some(NumberPartType::ExponentMinusSign);
    case UNUM_EXPONENT_FIELD:
      return Some(NumberPartType::ExponentInteger);
    case UNUM_GROUPING_SEPARATOR_FIELD:
      return Some(NumberPartType::Group);
    case UNUM_CURRENCY_FIELD:
      return Some(NumberPartType::Currency);
    case UNUM_PERCENT_FIELD:
      if (formatForUnit) {
        return Some(NumberPartType::Unit);
      }
      return Some(NumberPartType::Percent);
    case UNUM_PERMILL_FIELD:
      MOZ_ASSERT_UNREACHABLE(
          "unexpected permill field found, even though "
          "we don't use any user-defined patterns that "
          "would require a permill field");
      break;
    case UNUM_SIGN_FIELD:
      if (isNegative) {
        return Some(NumberPartType::MinusSign);
      }
      return Some(NumberPartType::PlusSign);
    case UNUM_MEASURE_UNIT_FIELD:
      return Some(NumberPartType::Unit);
    case UNUM_COMPACT_FIELD:
      return Some(NumberPartType::Compact);
#ifndef U_HIDE_DEPRECATED_API
    case UNUM_FIELD_COUNT:
      MOZ_ASSERT_UNREACHABLE(
          "format field sentinel value returned by iterator!");
      break;
#endif
  }

  MOZ_ASSERT_UNREACHABLE(
      "unenumerated, undocumented format field returned by iterator");
  return Nothing();
}

}  // namespace mozilla::intl
