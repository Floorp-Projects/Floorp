/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_GeneralCategory_h_
#define intl_components_GeneralCategory_h_

#include <cstdint>

namespace mozilla::intl {

// See https://www.unicode.org/reports/tr44/#General_Category_Values
// for details of these values.

// The values here must match the values used by ICU's UCharCategory.

enum class GeneralCategory : uint8_t {
  Unassigned = 0,
  Uppercase_Letter = 1,
  Lowercase_Letter = 2,
  Titlecase_Letter = 3,
  Modifier_Letter = 4,
  Other_Letter = 5,
  Nonspacing_Mark = 6,
  Enclosing_Mark = 7,
  Spacing_Mark = 8,
  Decimal_Number = 9,
  Letter_Number = 10,
  Other_Number = 11,
  Space_Separator = 12,
  Line_Separator = 13,
  Paragraph_Separator = 14,
  Control = 15,
  Format = 16,
  Private_Use = 17,
  Surrogate = 18,
  Dash_Punctuation = 19,
  Open_Punctuation = 20,
  Close_Punctuation = 21,
  Connector_Punctuation = 22,
  Other_Punctuation = 23,
  Math_Symbol = 24,
  Currency_Symbol = 25,
  Modifier_Symbol = 26,
  Other_Symbol = 27,
  Initial_Punctuation = 28,
  Final_Punctuation = 29,
  GeneralCategoryCount
};

}  // namespace mozilla::intl

#endif
