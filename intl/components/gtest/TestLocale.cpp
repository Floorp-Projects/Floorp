/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/Locale.h"
#include "mozilla/Span.h"

#include "TestBuffer.h"

namespace mozilla::intl {

TEST(IntlLocale, LocaleSettersAndGetters)
{
  Locale locale;
  locale.setLanguage("fr");
  locale.setRegion("CA");
  locale.setScript("Latn");
  ASSERT_TRUE(locale.setUnicodeExtension("u-ca-gregory"));
  ASSERT_TRUE(locale.language().equalTo("fr"));
  ASSERT_TRUE(locale.region().equalTo("CA"));
  ASSERT_TRUE(locale.script().equalTo("Latn"));
  ASSERT_EQ(MakeStringSpan(locale.unicodeExtension()),
            MakeStringSpan("u-ca-gregory"));

  TestBuffer<char> buffer;
  ASSERT_TRUE(locale.toString(buffer).isOk());
  ASSERT_TRUE(buffer.verboseMatches("fr-Latn-CA-u-ca-gregory"));

  // No setters for variants or other extensions...
  Locale locale2;
  ASSERT_TRUE(LocaleParser::tryParse(
                  MakeStringSpan("fr-CA-fonipa-t-es-AR-h0-hybrid"), locale2)
                  .isOk());
  ASSERT_EQ(MakeStringSpan(locale2.variants()[0].get()),
            MakeStringSpan("fonipa"));
  ASSERT_EQ(MakeStringSpan(locale2.extensions()[0].get()),
            MakeStringSpan("t-es-AR-h0-hybrid"));
  locale2.clearVariants();
  ASSERT_EQ(locale2.variants().length(), 0UL);
}

TEST(IntlLocale, LocaleParser)
{
  const char* tags[] = {
      "en-US",       "en-GB",       "es-AR",      "it",         "zh-Hans-CN",
      "de-AT",       "pl",          "fr-FR",      "de-AT",      "sr-Cyrl-SR",
      "nb-NO",       "fr-FR",       "mk",         "uk",         "und-PL",
      "und-Latn-AM", "ug-Cyrl",     "sr-ME",      "mn-Mong",    "lif-Limb",
      "gan",         "zh-Hant",     "yue-Hans",   "unr",        "unr-Deva",
      "und-Thai-CN", "ug-Cyrl",     "en-Latn-DE", "pl-FR",      "de-CH",
      "tuq",         "sr-ME",       "ng",         "klx",        "kk-Arab",
      "en-Cyrl",     "und-Cyrl-UK", "und-Arab",   "und-Arab-FO"};

  Locale locale;
  for (const auto* tag : tags) {
    ASSERT_TRUE(LocaleParser::tryParse(MakeStringSpan(tag), locale).isOk());
  }
}

TEST(IntlLocale, LikelySubtags)
{
  Locale locale;
  ASSERT_TRUE(LocaleParser::tryParse(MakeStringSpan("zh"), locale).isOk());
  ASSERT_TRUE(locale.addLikelySubtags());
  TestBuffer<char> buffer;
  ASSERT_TRUE(locale.toString(buffer).isOk());
  ASSERT_TRUE(buffer.verboseMatches("zh-Hans-CN"));
  ASSERT_TRUE(locale.removeLikelySubtags());
  buffer.clear();
  ASSERT_TRUE(locale.toString(buffer).isOk());
  ASSERT_TRUE(buffer.verboseMatches("zh"));
}

TEST(IntlLocale, Canonicalize)
{
  Locale locale;
  ASSERT_TRUE(
      LocaleParser::tryParse(MakeStringSpan("nob-bokmal"), locale).isOk());
  ASSERT_TRUE(locale.canonicalize().isOk());
  TestBuffer<char> buffer;
  ASSERT_TRUE(locale.toString(buffer).isOk());
  ASSERT_TRUE(buffer.verboseMatches("nb"));
}

// These tests are dependent on the machine that this test is being run on.
TEST(IntlLocale, SystemDependentTests)
{
  // e.g. "en_US"
  const char* locale = Locale::GetDefaultLocale();
  ASSERT_TRUE(locale != nullptr);
}

TEST(IntlLocale, GetAvailableLocales)
{
  using namespace std::literals;

  int32_t english = 0;
  int32_t german = 0;
  int32_t chinese = 0;

  // Since this list is dependent on ICU, and may change between upgrades, only
  // test a subset of the available locales.
  for (const char* locale : Locale::GetAvailableLocales()) {
    if (locale == "en"sv) {
      english++;
    } else if (locale == "de"sv) {
      german++;
    } else if (locale == "zh"sv) {
      chinese++;
    }
  }

  // Each locale should be found exactly once.
  ASSERT_EQ(english, 1);
  ASSERT_EQ(german, 1);
  ASSERT_EQ(chinese, 1);
}

}  // namespace mozilla::intl
