/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef intl_components_Locale_h_
#define intl_components_Locale_h_

#include "unicode/uloc.h"

#include "mozilla/intl/ICU4CGlue.h"

namespace mozilla::intl {

class Locale final {
 public:
  Locale() = delete;

  /**
   * Returns the default locale as an ICU locale identifier. The returned string
   * is NOT a valid BCP 47 language tag!
   *
   * Also see <https://unicode-org.github.io/icu/userguide/locale>.
   */
  static const char* GetDefaultLocale() { return uloc_getDefault(); }

  /**
   * Returns an iterator over all supported locales.
   *
   * The returned strings are ICU locale identifiers and NOT BCP 47 language
   * tags.
   *
   * Also see <https://unicode-org.github.io/icu/userguide/locale>.
   */
  static auto GetAvailableLocales() {
    return AvailableLocalesEnumeration<uloc_countAvailable,
                                       uloc_getAvailable>();
  }
};

}  // namespace mozilla::intl

#endif
