/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include <string.h>
#include "mozilla/intl/Collator.h"
#include "mozilla/Span.h"
#include "TestBuffer.h"

namespace mozilla::intl {

TEST(IntlCollator, SetAttributesInternal)
{
  // Run through each settings to make sure MOZ_ASSERT is not triggered for
  // misconfigured attributes.
  auto result = Collator::TryCreate("en-US");
  ASSERT_TRUE(result.isOk());
  auto collator = result.unwrap();

  collator->SetStrength(Collator::Strength::Primary);
  collator->SetStrength(Collator::Strength::Secondary);
  collator->SetStrength(Collator::Strength::Tertiary);
  collator->SetStrength(Collator::Strength::Quaternary);
  collator->SetStrength(Collator::Strength::Identical);
  collator->SetStrength(Collator::Strength::Default);

  collator->SetAlternateHandling(Collator::AlternateHandling::NonIgnorable)
      .unwrap();
  collator->SetAlternateHandling(Collator::AlternateHandling::Shifted).unwrap();
  collator->SetAlternateHandling(Collator::AlternateHandling::Default).unwrap();

  collator->SetCaseFirst(Collator::CaseFirst::False).unwrap();
  collator->SetCaseFirst(Collator::CaseFirst::Upper).unwrap();
  collator->SetCaseFirst(Collator::CaseFirst::Lower).unwrap();

  collator->SetCaseLevel(Collator::Feature::On).unwrap();
  collator->SetCaseLevel(Collator::Feature::Off).unwrap();
  collator->SetCaseLevel(Collator::Feature::Default).unwrap();

  collator->SetNumericCollation(Collator::Feature::On).unwrap();
  collator->SetNumericCollation(Collator::Feature::Off).unwrap();
  collator->SetNumericCollation(Collator::Feature::Default).unwrap();

  collator->SetNormalizationMode(Collator::Feature::On).unwrap();
  collator->SetNormalizationMode(Collator::Feature::Off).unwrap();
  collator->SetNormalizationMode(Collator::Feature::Default).unwrap();
}

TEST(IntlCollator, GetSortKey)
{
  // Do some light sort key comparisons to ensure everything is wired up
  // correctly. This is not doing extensive correctness testing.
  auto result = Collator::TryCreate("en-US");
  ASSERT_TRUE(result.isOk());
  auto collator = result.unwrap();
  TestBuffer<uint8_t> bufferA;
  TestBuffer<uint8_t> bufferB;

  auto compareSortKeys = [&](const char16_t* a, const char16_t* b) {
    collator->GetSortKey(MakeStringSpan(a), bufferA).unwrap();
    collator->GetSortKey(MakeStringSpan(b), bufferB).unwrap();
    return strcmp(reinterpret_cast<const char*>(bufferA.data()),
                  reinterpret_cast<const char*>(bufferB.data()));
  };

  ASSERT_TRUE(compareSortKeys(u"aaa", u"bbb") < 0);
  ASSERT_TRUE(compareSortKeys(u"bbb", u"aaa") > 0);
  ASSERT_TRUE(compareSortKeys(u"aaa", u"aaa") == 0);
  ASSERT_TRUE(compareSortKeys(u"👍", u"👎") < 0);
}

TEST(IntlCollator, CompareStrings)
{
  // Do some light string comparisons to ensure everything is wired up
  // correctly. This is not doing extensive correctness testing.
  auto result = Collator::TryCreate("en-US");
  ASSERT_TRUE(result.isOk());
  auto collator = result.unwrap();
  TestBuffer<uint8_t> bufferA;
  TestBuffer<uint8_t> bufferB;

  ASSERT_EQ(collator->CompareStrings(u"aaa", u"bbb"), -1);
  ASSERT_EQ(collator->CompareStrings(u"bbb", u"aaa"), 1);
  ASSERT_EQ(collator->CompareStrings(u"aaa", u"aaa"), 0);
  ASSERT_EQ(collator->CompareStrings(u"👍", u"👎"), -1);
}

TEST(IntlCollator, SetOptionsSensitivity)
{
  // Test the ECMA 402 sensitivity behavior per:
  // https://tc39.es/ecma402/#sec-collator-comparestrings
  auto result = Collator::TryCreate("en-US");
  ASSERT_TRUE(result.isOk());
  auto collator = result.unwrap();

  TestBuffer<uint8_t> bufferA;
  TestBuffer<uint8_t> bufferB;
  ICUResult optResult = Ok();
  Collator::Options options{};

  options.sensitivity = Collator::Sensitivity::Base;
  optResult = collator->SetOptions(options);
  ASSERT_TRUE(optResult.isOk());
  ASSERT_EQ(collator->CompareStrings(u"a", u"b"), -1);
  ASSERT_EQ(collator->CompareStrings(u"a", u"á"), 0);
  ASSERT_EQ(collator->CompareStrings(u"a", u"A"), 0);

  options.sensitivity = Collator::Sensitivity::Accent;
  optResult = collator->SetOptions(options);
  ASSERT_TRUE(optResult.isOk());
  ASSERT_EQ(collator->CompareStrings(u"a", u"b"), -1);
  ASSERT_EQ(collator->CompareStrings(u"a", u"á"), -1);
  ASSERT_EQ(collator->CompareStrings(u"a", u"A"), 0);

  options.sensitivity = Collator::Sensitivity::Case;
  optResult = collator->SetOptions(options);
  ASSERT_TRUE(optResult.isOk());
  ASSERT_EQ(collator->CompareStrings(u"a", u"b"), -1);
  ASSERT_EQ(collator->CompareStrings(u"a", u"á"), 0);
  ASSERT_EQ(collator->CompareStrings(u"a", u"A"), -1);

  options.sensitivity = Collator::Sensitivity::Variant;
  optResult = collator->SetOptions(options);
  ASSERT_TRUE(optResult.isOk());
  ASSERT_EQ(collator->CompareStrings(u"a", u"b"), -1);
  ASSERT_EQ(collator->CompareStrings(u"a", u"á"), -1);
  ASSERT_EQ(collator->CompareStrings(u"a", u"A"), -1);
}

TEST(IntlCollator, LocaleSensitiveCollations)
{
  UniquePtr<Collator> collator = nullptr;
  TestBuffer<uint8_t> bufferA;
  TestBuffer<uint8_t> bufferB;

  auto changeLocale = [&](const char* locale) {
    auto result = Collator::TryCreate(locale);
    ASSERT_TRUE(result.isOk());
    collator = result.unwrap();

    Collator::Options options{};
    options.sensitivity = Collator::Sensitivity::Base;
    auto optResult = collator->SetOptions(options);
    ASSERT_TRUE(optResult.isOk());
  };

  // Swedish treats "Ö" as a separate character, which sorts after "Z".
  changeLocale("en-US");
  ASSERT_EQ(collator->CompareStrings(u"Österreich", u"Västervik"), -1);
  changeLocale("sv-SE");
  ASSERT_EQ(collator->CompareStrings(u"Österreich", u"Västervik"), 1);

  // Country names in their respective scripts.
  auto china = MakeStringSpan(u"中国");
  auto japan = MakeStringSpan(u"日本");
  auto korea = MakeStringSpan(u"한국");

  changeLocale("en-US");
  ASSERT_EQ(collator->CompareStrings(china, japan), -1);
  ASSERT_EQ(collator->CompareStrings(china, korea), 1);
  changeLocale("zh");
  ASSERT_EQ(collator->CompareStrings(china, japan), 1);
  ASSERT_EQ(collator->CompareStrings(china, korea), -1);
  changeLocale("ja");
  ASSERT_EQ(collator->CompareStrings(china, japan), -1);
  ASSERT_EQ(collator->CompareStrings(china, korea), -1);
  changeLocale("ko");
  ASSERT_EQ(collator->CompareStrings(china, japan), 1);
  ASSERT_EQ(collator->CompareStrings(china, korea), -1);
}

TEST(IntlCollator, IgnorePunctuation)
{
  TestBuffer<uint8_t> bufferA;
  TestBuffer<uint8_t> bufferB;

  auto result = Collator::TryCreate("en-US");
  ASSERT_TRUE(result.isOk());
  auto collator = result.unwrap();
  Collator::Options options{};
  options.ignorePunctuation = true;

  auto optResult = collator->SetOptions(options);
  ASSERT_TRUE(optResult.isOk());

  ASSERT_EQ(collator->CompareStrings(u"aa", u".bb"), -1);

  options.ignorePunctuation = false;
  optResult = collator->SetOptions(options);
  ASSERT_TRUE(optResult.isOk());

  ASSERT_EQ(collator->CompareStrings(u"aa", u".bb"), 1);
}

TEST(IntlCollator, GetBcp47KeywordValuesForLocale)
{
  auto result = Collator::TryCreate("en-US");
  ASSERT_TRUE(result.isOk());
  auto collator = result.unwrap();
  auto extsResult = collator->GetBcp47KeywordValuesForLocale("de");
  ASSERT_TRUE(extsResult.isOk());
  auto extensions = extsResult.unwrap();

  // Since this list is dependent on ICU, and may change between upgrades, only
  // test a subset of the keywords.
  auto standard = MakeStringSpan("standard");
  auto search = MakeStringSpan("search");
  auto phonebk = MakeStringSpan("phonebk");      // Valid BCP 47.
  auto phonebook = MakeStringSpan("phonebook");  // Not valid BCP 47.
  bool hasStandard = false;
  bool hasSearch = false;
  bool hasPhonebk = false;
  bool hasPhonebook = false;

  for (auto extensionResult : extensions) {
    ASSERT_TRUE(extensionResult.isOk());
    auto extension = extensionResult.unwrap();
    hasStandard |= extension == standard;
    hasSearch |= extension == search;
    hasPhonebk |= extension == phonebk;
    hasPhonebook |= extension == phonebook;
  }

  ASSERT_TRUE(hasStandard);
  ASSERT_TRUE(hasSearch);
  ASSERT_TRUE(hasPhonebk);

  ASSERT_FALSE(hasPhonebook);  // Not valid BCP 47.
}

}  // namespace mozilla::intl
