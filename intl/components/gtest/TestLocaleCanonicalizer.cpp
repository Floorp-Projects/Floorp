/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/LocaleCanonicalizer.h"
#include "mozilla/Span.h"

namespace mozilla::intl {

static void CheckLocaleResult(LocaleCanonicalizer::Vector& ascii,
                              const char* before, const char* after) {
  auto result = LocaleCanonicalizer::CanonicalizeICULevel1(before, ascii);
  ASSERT_TRUE(result.isOk());
  ASSERT_EQ(Span(const_cast<const char*>(ascii.begin()), ascii.length()),
            MakeStringSpan(after));
}

/**
 * Asserts the behavior of canonicalization as defined in:
 * http://userguide.icu-project.org/locale#TOC-Canonicalization
 */
TEST(IntlLocaleCanonicalizer, CanonicalizeICULevel1)
{
  LocaleCanonicalizer::Vector ascii{};

  // Canonicalizes en-US
  CheckLocaleResult(ascii, "en-US", "en_US");
  // Canonicalizes POSIX
  CheckLocaleResult(ascii, "en-US-posix", "en_US_POSIX");
  // und gets changed to an empty string
  CheckLocaleResult(ascii, "und", "");
  // retains incorrect locales
  CheckLocaleResult(ascii, "asdf", "asdf");
  // makes text uppercase
  CheckLocaleResult(ascii, "es-es", "es_ES");
  // Converts 3 letter country codes to 2 letter.
  CheckLocaleResult(ascii, "en-USA", "en_US");
  // Does not perform level 2 canonicalization where the result would be
  // fr_FR@currency=EUR
  CheckLocaleResult(ascii, "fr-fr@EURO", "fr_FR_EURO");
  // Removes the .utf8 ends
  CheckLocaleResult(ascii, "ar-MA.utf8", "ar_MA");

  // Allows valid ascii inputs
  CheckLocaleResult(
      ascii,
      "abcdefghijlkmnopqrstuvwxyzABCDEFGHIJLKMNOPQRSTUVWXYZ-_.0123456789",
      "abcdefghijlkmnopqrstuvwxyzabcdefghijlkmnopqrstuvwxyz__");
  CheckLocaleResult(ascii, "exotic ascii:", "exotic ascii:");

  // Does not accept non-ascii inputs.
  ASSERT_EQ(LocaleCanonicalizer::CanonicalizeICULevel1("👍", ascii).unwrapErr(),
            ICUError::InternalError);
  ASSERT_EQ(
      LocaleCanonicalizer::CanonicalizeICULevel1("ᏣᎳᎩ", ascii).unwrapErr(),
      ICUError::InternalError);
}

}  // namespace mozilla::intl
