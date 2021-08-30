/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_NumberFormatFieldsUtil_h_
#define intl_components_NumberFormatFieldsUtil_h_

#include "mozilla/Maybe.h"

#include "unicode/unum.h"

namespace mozilla::intl {

Maybe<NumberPartType> GetPartTypeForNumberField(UNumberFormatFields fieldName,
                                                Maybe<double> number,
                                                bool isNegative,
                                                bool formatForUnit);

}  // namespace mozilla::intl

#endif
