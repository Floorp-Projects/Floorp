/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_DateTimeFormatUtils_h_
#define intl_components_DateTimeFormatUtils_h_
#include "unicode/udat.h"

#include "mozilla/intl/DateTimePart.h"

namespace mozilla::intl {
DateTimePartType ConvertUFormatFieldToPartType(UDateFormatField fieldName);
}  // namespace mozilla::intl

#endif
