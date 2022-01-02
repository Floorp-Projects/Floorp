/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_LocaleCanonicalizer_h_
#define intl_components_LocaleCanonicalizer_h_

#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/Span.h"
#include "mozilla/Vector.h"

namespace mozilla::intl {

/**
 * 32 is somewhat an arbitrary size, but it should fit most locales on the
 * stack to avoid heap allocations.
 */
constexpr size_t INITIAL_LOCALE_CANONICALIZER_BUFFER_SIZE = 32;

/**
 * Eventually this class will unify the behaviors of Locale Canonicalization.
 * See Bug 1723586.
 */
class LocaleCanonicalizer {
 public:
  using Vector =
      mozilla::Vector<char, INITIAL_LOCALE_CANONICALIZER_BUFFER_SIZE>;

  /**
   * This static method will canonicalize a locale string, per the Level 1
   * canonicalization steps outlined in:
   * http://userguide.icu-project.org/locale#TOC-Canonicalization
   *
   * For instance it will turn the string "en-US" to "en_US". It guarantees that
   * the string span targeted will be in the ASCII range. The canonicalization
   * process on ICU is somewhat permissive in what it accepts as input, but only
   * ASCII locales are technically correct.
   */
  static ICUResult CanonicalizeICULevel1(
      const char* aLocale, LocaleCanonicalizer::Vector& aLocaleOut);
};

}  // namespace mozilla::intl
#endif
