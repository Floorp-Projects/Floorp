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
  locale.SetLanguage("fr");
  locale.SetRegion("CA");
  locale.SetScript("Latn");
  ASSERT_TRUE(
      locale.SetUnicodeExtension(MakeStringSpan("u-ca-gregory")).isOk());
  ASSERT_TRUE(locale.Language().EqualTo("fr"));
  ASSERT_TRUE(locale.Region().EqualTo("CA"));
  ASSERT_TRUE(locale.Script().EqualTo("Latn"));
  ASSERT_EQ(locale.GetUnicodeExtension().value(),
            MakeStringSpan("u-ca-gregory"));

  TestBuffer<char> buffer;
  ASSERT_TRUE(locale.ToString(buffer).isOk());
  ASSERT_TRUE(buffer.verboseMatches("fr-Latn-CA-u-ca-gregory"));

  // No setters for variants or other extensions...
  Locale locale2;
  ASSERT_TRUE(LocaleParser::TryParse(
                  MakeStringSpan("fr-CA-fonipa-t-es-AR-h0-hybrid"), locale2)
                  .isOk());
  ASSERT_EQ(locale2.Variants()[0], MakeStringSpan("fonipa"));
  ASSERT_EQ(locale2.Extensions()[0], MakeStringSpan("t-es-AR-h0-hybrid"));
  locale2.ClearVariants();
  ASSERT_EQ(locale2.Variants().length(), 0UL);
}

TEST(IntlLocale, LocaleMove)
{
  Locale locale;
  ASSERT_TRUE(
      LocaleParser::TryParse(
          MakeStringSpan(
              "fr-Latn-CA-fonipa-u-ca-gregory-t-es-AR-h0-hybrid-x-private"),
          locale)
          .isOk());

  ASSERT_TRUE(locale.Language().EqualTo("fr"));
  ASSERT_TRUE(locale.Script().EqualTo("Latn"));
  ASSERT_TRUE(locale.Region().EqualTo("CA"));
  ASSERT_EQ(locale.Variants()[0], MakeStringSpan("fonipa"));
  ASSERT_EQ(locale.Extensions()[0], MakeStringSpan("u-ca-gregory"));
  ASSERT_EQ(locale.Extensions()[1], MakeStringSpan("t-es-AR-h0-hybrid"));
  ASSERT_EQ(locale.GetUnicodeExtension().value(),
            MakeStringSpan("u-ca-gregory"));
  ASSERT_EQ(locale.PrivateUse().value(), MakeStringSpan("x-private"));

  Locale locale2 = std::move(locale);

  ASSERT_TRUE(locale2.Language().EqualTo("fr"));
  ASSERT_TRUE(locale2.Script().EqualTo("Latn"));
  ASSERT_TRUE(locale2.Region().EqualTo("CA"));
  ASSERT_EQ(locale2.Variants()[0], MakeStringSpan("fonipa"));
  ASSERT_EQ(locale2.Extensions()[0], MakeStringSpan("u-ca-gregory"));
  ASSERT_EQ(locale2.Extensions()[1], MakeStringSpan("t-es-AR-h0-hybrid"));
  ASSERT_EQ(locale2.GetUnicodeExtension().value(),
            MakeStringSpan("u-ca-gregory"));
  ASSERT_EQ(locale2.PrivateUse().value(), MakeStringSpan("x-private"));
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

  for (const auto* tag : tags) {
    Locale locale;
    ASSERT_TRUE(LocaleParser::TryParse(MakeStringSpan(tag), locale).isOk());
  }
}

TEST(IntlLocale, LikelySubtags)
{
  Locale locale;
  ASSERT_TRUE(LocaleParser::TryParse(MakeStringSpan("zh"), locale).isOk());
  ASSERT_TRUE(locale.AddLikelySubtags().isOk());
  TestBuffer<char> buffer;
  ASSERT_TRUE(locale.ToString(buffer).isOk());
  ASSERT_TRUE(buffer.verboseMatches("zh-Hans-CN"));
  ASSERT_TRUE(locale.RemoveLikelySubtags().isOk());
  buffer.clear();
  ASSERT_TRUE(locale.ToString(buffer).isOk());
  ASSERT_TRUE(buffer.verboseMatches("zh"));
}

TEST(IntlLocale, Canonicalize)
{
  Locale locale;
  ASSERT_TRUE(
      LocaleParser::TryParse(MakeStringSpan("nob-bokmal"), locale).isOk());
  ASSERT_TRUE(locale.Canonicalize().isOk());
  TestBuffer<char> buffer;
  ASSERT_TRUE(locale.ToString(buffer).isOk());
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
