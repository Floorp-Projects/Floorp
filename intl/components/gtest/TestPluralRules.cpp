/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/Vector.h"
#include "mozilla/intl/PluralRules.h"

namespace mozilla {
namespace intl {

TEST(IntlPluralRules, CategoriesEnCardinal)
{
  PluralRulesOptions defaultOptions;
  UniquePtr<PluralRules> pr =
      PluralRules::TryCreate("en", defaultOptions).unwrap();

  auto set = pr->Categories().unwrap();
  ASSERT_EQ(set.size(), 2);
  ASSERT_TRUE(set.contains(PluralRules::Keyword::One));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Other));
}

TEST(IntlPluralRules, CategoriesEnOrdinal)
{
  PluralRulesOptions options;
  options.mPluralType = PluralRules::Type::Ordinal;
  UniquePtr<PluralRules> pr = PluralRules::TryCreate("en", options).unwrap();

  auto set = pr->Categories().unwrap();
  ASSERT_EQ(set.size(), 4);
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Few));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::One));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Other));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Two));
}

TEST(IntlPluralRules, CategoriesCyCardinal)
{
  PluralRulesOptions defaultOptions;
  UniquePtr<PluralRules> pr =
      PluralRules::TryCreate("cy", defaultOptions).unwrap();

  auto set = pr->Categories().unwrap();
  ASSERT_EQ(set.size(), 6);
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Few));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::One));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Other));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Many));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Two));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Zero));
}

TEST(IntlPluralRules, CategoriesCyOrdinal)
{
  PluralRulesOptions options;
  options.mPluralType = PluralRules::Type::Ordinal;
  UniquePtr<PluralRules> pr = PluralRules::TryCreate("cy", options).unwrap();

  auto set = pr->Categories().unwrap();
  ASSERT_EQ(set.size(), 6);
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Few));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::One));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Other));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Many));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Two));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Zero));
}

TEST(IntlPluralRules, CategoriesBrCardinal)
{
  PluralRulesOptions defaultOptions;
  UniquePtr<PluralRules> pr =
      PluralRules::TryCreate("br", defaultOptions).unwrap();

  auto set = pr->Categories().unwrap();
  ASSERT_EQ(set.size(), 5);
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Few));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::One));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Other));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Many));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Two));
}

TEST(IntlPluralRules, CategoriesBrOrdinal)
{
  PluralRulesOptions options;
  options.mPluralType = PluralRules::Type::Ordinal;
  UniquePtr<PluralRules> pr = PluralRules::TryCreate("br", options).unwrap();

  auto set = pr->Categories().unwrap();
  ASSERT_EQ(set.size(), 1);
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Other));
}

TEST(IntlPluralRules, CategoriesHsbCardinal)
{
  PluralRulesOptions defaultOptions;
  UniquePtr<PluralRules> pr =
      PluralRules::TryCreate("hsb", defaultOptions).unwrap();

  auto set = pr->Categories().unwrap();
  ASSERT_EQ(set.size(), 4);
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Few));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::One));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Other));
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Two));
}

TEST(IntlPluralRules, CategoriesHsbOrdinal)
{
  PluralRulesOptions options;
  options.mPluralType = PluralRules::Type::Ordinal;
  UniquePtr<PluralRules> pr = PluralRules::TryCreate("hsb", options).unwrap();

  auto set = pr->Categories().unwrap();
  ASSERT_EQ(set.size(), 1);
  ASSERT_TRUE(set.contains(PluralRules::Keyword::Other));
}

// PluralRules should define the sort order of the keywords.
// ICU returns these keywords in alphabetical order, so our implementation
// should do the same.
//
// https://github.com/tc39/ecma402/issues/578
TEST(IntlPluralRules, CategoriesSortOrder)
{
  PluralRulesOptions defaultOptions;
  UniquePtr<PluralRules> pr =
      PluralRules::TryCreate("cy", defaultOptions).unwrap();

  PluralRules::Keyword expected[] = {
      PluralRules::Keyword::Few, PluralRules::Keyword::Many,
      PluralRules::Keyword::One, PluralRules::Keyword::Other,
      PluralRules::Keyword::Two, PluralRules::Keyword::Zero,
  };

  // Categories() returns an EnumSet so we want to ensure it still iterates
  // over elements in the expected sorted order.
  size_t index = 0;
  for (const PluralRules::Keyword keyword : pr->Categories().unwrap()) {
    ASSERT_EQ(keyword, expected[index++]);
  }
}

// en Cardinal Plural Rules
//   one: i = 1 and v = 0 @integer 1
// other: @integer 0, 2~16, 100, 1000, 10000, 100000, 1000000, …
//        @decimal 0.0~1.5, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0, …
TEST(IntlPluralRules, SelectEnCardinal)
{
  PluralRulesOptions defaultOptions;
  UniquePtr<PluralRules> pr =
      PluralRules::TryCreate("en", defaultOptions).unwrap();

  ASSERT_EQ(pr->Select(0.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(1.00).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(1.01).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(0.99).unwrap(), PluralRules::Keyword::Other);
}

// en Ordinal Plural Rules
//   one: n % 10 = 1 and n % 100 != 11
//        @integer 1, 21, 31, 41, 51, 61, 71, 81, 101, 1001, …,
//   two: n % 10 = 2 and n % 100 != 12
//        @integer 2, 22, 32, 42, 52, 62, 72, 82, 102, 1002, …,
//   few: n % 10 = 3 and n % 100 != 13
//        @integer 3, 23, 33, 43, 53, 63, 73, 83, 103, 1003, …,
// other: @integer 0, 4~18, 100, 1000, 10000, 100000, 1000000, …
TEST(IntlPluralRules, SelectEnOrdinal)
{
  PluralRulesOptions options;
  options.mPluralType = PluralRules::Type::Ordinal;
  UniquePtr<PluralRules> pr = PluralRules::TryCreate("en", options).unwrap();

  ASSERT_EQ(pr->Select(01.00).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(21.00).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(31.00).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(41.00).unwrap(), PluralRules::Keyword::One);

  ASSERT_EQ(pr->Select(02.00).unwrap(), PluralRules::Keyword::Two);
  ASSERT_EQ(pr->Select(22.00).unwrap(), PluralRules::Keyword::Two);
  ASSERT_EQ(pr->Select(32.00).unwrap(), PluralRules::Keyword::Two);
  ASSERT_EQ(pr->Select(42.00).unwrap(), PluralRules::Keyword::Two);

  ASSERT_EQ(pr->Select(03.00).unwrap(), PluralRules::Keyword::Few);
  ASSERT_EQ(pr->Select(23.00).unwrap(), PluralRules::Keyword::Few);
  ASSERT_EQ(pr->Select(33.00).unwrap(), PluralRules::Keyword::Few);
  ASSERT_EQ(pr->Select(43.00).unwrap(), PluralRules::Keyword::Few);

  ASSERT_EQ(pr->Select(00.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(11.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(12.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(13.00).unwrap(), PluralRules::Keyword::Other);
}

// cy Cardinal Plural Rules
//  zero: n = 0 @integer 0 @decimal 0.0, 0.00, 0.000, 0.0000,
//   one: n = 1 @integer 1 @decimal 1.0, 1.00, 1.000, 1.0000,
//   two: n = 2 @integer 2 @decimal 2.0, 2.00, 2.000, 2.0000,
//   few: n = 3 @integer 3 @decimal 3.0, 3.00, 3.000, 3.0000,
//  many: n = 6 @integer 6 @decimal 6.0, 6.00, 6.000, 6.0000,
// other: @integer 4, 5, 7~20, 100, 1000, 10000, 100000, 1000000, …
//        @decimal 0.1~0.9, 1.1~1.7, 10.0, 100.0, 1000.0, 10000.0, 100000.0, …
TEST(IntlPluralRules, SelectCyCardinal)
{
  PluralRulesOptions defaultOptions;
  UniquePtr<PluralRules> pr =
      PluralRules::TryCreate("cy", defaultOptions).unwrap();

  ASSERT_EQ(pr->Select(0.00).unwrap(), PluralRules::Keyword::Zero);
  ASSERT_EQ(pr->Select(1.00).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(2.00).unwrap(), PluralRules::Keyword::Two);
  ASSERT_EQ(pr->Select(3.00).unwrap(), PluralRules::Keyword::Few);
  ASSERT_EQ(pr->Select(4.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(5.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(6.00).unwrap(), PluralRules::Keyword::Many);
  ASSERT_EQ(pr->Select(7.00).unwrap(), PluralRules::Keyword::Other);
}

// cy Ordinal Plural Rules
//  zero: n = 0,7,8,9 @integer 0, 7~9,
//   one: n = 1 @integer 1,
//   two: n = 2 @integer 2,
//   few: n = 3,4 @integer 3, 4,
//  many: n = 5,6 @integer 5, 6,
// other: @integer 10~25, 100, 1000, 10000, 100000, 1000000, …
TEST(IntlPluralRules, SelectCyOrdinal)
{
  PluralRulesOptions options;
  options.mPluralType = PluralRules::Type::Ordinal;
  UniquePtr<PluralRules> pr = PluralRules::TryCreate("cy", options).unwrap();

  ASSERT_EQ(pr->Select(0.00).unwrap(), PluralRules::Keyword::Zero);
  ASSERT_EQ(pr->Select(7.00).unwrap(), PluralRules::Keyword::Zero);
  ASSERT_EQ(pr->Select(8.00).unwrap(), PluralRules::Keyword::Zero);
  ASSERT_EQ(pr->Select(9.00).unwrap(), PluralRules::Keyword::Zero);

  ASSERT_EQ(pr->Select(1.00).unwrap(), PluralRules::Keyword::One);

  ASSERT_EQ(pr->Select(2.00).unwrap(), PluralRules::Keyword::Two);

  ASSERT_EQ(pr->Select(3.00).unwrap(), PluralRules::Keyword::Few);
  ASSERT_EQ(pr->Select(4.00).unwrap(), PluralRules::Keyword::Few);

  ASSERT_EQ(pr->Select(5.00).unwrap(), PluralRules::Keyword::Many);
  ASSERT_EQ(pr->Select(6.00).unwrap(), PluralRules::Keyword::Many);

  ASSERT_EQ(pr->Select(10.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(11.00).unwrap(), PluralRules::Keyword::Other);
}

// br Cardinal Plural Rules
//   one: n % 10 = 1 and n % 100 != 11,71,91
//        @integer 1, 21, 31, 41, 51, 61, 81, 101, 1001, …
//        @decimal 1.0, 21.0, 31.0, 41.0, 51.0, 61.0, 81.0, 101.0, 1001.0, …
//   two: n % 10 = 2 and n % 100 != 12,72,92
//        @integer 2, 22, 32, 42, 52, 62, 82, 102, 1002, …
//        @decimal 2.0, 22.0, 32.0, 42.0, 52.0, 62.0, 82.0, 102.0, 1002.0, …
//   few: n % 10 = 3..4,9 and n % 100 != 10..19,70..79,90..99
//        @integer 3, 4, 9, 23, 24, 29, 33, 34, 39, 43, 44, 49, 103, 1003, …
//        @decimal 3.0, 4.0, 9.0, 23.0, 24.0, 29.0, 33.0, 34.0, 103.0, 1003.0, …
//  many: n != 0 and n % 1000000 = 0
//        @integer 1000000, …
//        @decimal 1000000.0, 1000000.00, 1000000.000, 1000000.0000, …
// other: @integer 0, 5~8, 10~20, 100, 1000, 10000, 100000, …
//        @decimal 0.0~0.9, 1.1~1.6, 10.0, 100.0, 1000.0, 10000.0, 100000.0, …
TEST(IntlPluralRules, SelectBrCardinal)
{
  PluralRulesOptions defaultOptions;
  UniquePtr<PluralRules> pr =
      PluralRules::TryCreate("br", defaultOptions).unwrap();

  ASSERT_EQ(pr->Select(00.00).unwrap(), PluralRules::Keyword::Other);

  ASSERT_EQ(pr->Select(01.00).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(11.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(21.00).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(31.00).unwrap(), PluralRules::Keyword::One);

  ASSERT_EQ(pr->Select(02.00).unwrap(), PluralRules::Keyword::Two);
  ASSERT_EQ(pr->Select(12.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(22.00).unwrap(), PluralRules::Keyword::Two);
  ASSERT_EQ(pr->Select(32.00).unwrap(), PluralRules::Keyword::Two);

  ASSERT_EQ(pr->Select(03.00).unwrap(), PluralRules::Keyword::Few);
  ASSERT_EQ(pr->Select(04.00).unwrap(), PluralRules::Keyword::Few);
  ASSERT_EQ(pr->Select(09.00).unwrap(), PluralRules::Keyword::Few);
  ASSERT_EQ(pr->Select(23.00).unwrap(), PluralRules::Keyword::Few);

  ASSERT_EQ(pr->Select(1000000).unwrap(), PluralRules::Keyword::Many);

  ASSERT_EQ(pr->Select(999999).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(1000005).unwrap(), PluralRules::Keyword::Other);
}

// br Ordinal Plural Rules
// br has no rules for Ordinal, so everything is PluralRules::Keyword::Other.
TEST(IntlPluralRules, SelectBrOrdinal)
{
  PluralRulesOptions options;
  options.mPluralType = PluralRules::Type::Ordinal;
  UniquePtr<PluralRules> pr = PluralRules::TryCreate("br", options).unwrap();

  ASSERT_EQ(pr->Select(00.00).unwrap(), PluralRules::Keyword::Other);

  ASSERT_EQ(pr->Select(01.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(11.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(21.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(31.00).unwrap(), PluralRules::Keyword::Other);

  ASSERT_EQ(pr->Select(02.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(12.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(22.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(32.00).unwrap(), PluralRules::Keyword::Other);

  ASSERT_EQ(pr->Select(03.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(04.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(09.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(23.00).unwrap(), PluralRules::Keyword::Other);

  ASSERT_EQ(pr->Select(1000000).unwrap(), PluralRules::Keyword::Other);

  ASSERT_EQ(pr->Select(999999).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(1000005).unwrap(), PluralRules::Keyword::Other);
}

// hsb Cardinal Plural Rules
//   one: v = 0 and i % 100 = 1 or f % 100 = 1
//        @integer 1, 101, 201, 301, 401, 501, 601, 701, 1001, …
//        @decimal 0.1, 1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1, 10.1, 100.1, 1000.1,
//        …,
//   two: v = 0 and i % 100 = 2 or f % 100 = 2
//        @integer 2, 102, 202, 302, 402, 502, 602, 702, 1002, …
//        @decimal 0.2, 1.2, 2.2, 3.2, 4.2, 5.2, 6.2, 7.2, 10.2, 100.2, 1000.2,
//        …,
//   few: v = 0 and i % 100 = 3..4 or f % 100 = 3..4
//        @integer 3, 4, 103, 104, 203, 204, 303, 304, 403, 404, 503, 504, 603,
//        604, 703, 704, 1003, …
//        @decimal 0.3,
//        0.4, 1.3, 1.4, 2.3, 2.4, 3.3, 3.4, 4.3, 4.4, 5.3, 5.4, 6.3, 6.4, 7.3, 7.4,
//        10.3, 100.3, 1000.3, …,
// other: @integer 0, 5~19, 100, 1000, 10000, 100000, 1000000, …
//        @decimal 0.0, 0.5~1.0, 1.5~2.0, 2.5~2.7, 10.0, 100.0, 1000.0, 10000.0,
//        100000.0, 1000000.0, …
TEST(IntlPluralRules, SelectHsbCardinal)
{
  PluralRulesOptions defaultOptions;
  UniquePtr<PluralRules> pr =
      PluralRules::TryCreate("hsb", defaultOptions).unwrap();

  ASSERT_EQ(pr->Select(1.00).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(101.00).unwrap(), PluralRules::Keyword::One);

  ASSERT_EQ(pr->Select(2.00).unwrap(), PluralRules::Keyword::Two);
  ASSERT_EQ(pr->Select(102.00).unwrap(), PluralRules::Keyword::Two);

  ASSERT_EQ(pr->Select(3.00).unwrap(), PluralRules::Keyword::Few);
  ASSERT_EQ(pr->Select(4.00).unwrap(), PluralRules::Keyword::Few);
  ASSERT_EQ(pr->Select(103.00).unwrap(), PluralRules::Keyword::Few);

  ASSERT_EQ(pr->Select(0.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(5.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(19.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(100.00).unwrap(), PluralRules::Keyword::Other);
}

// hsb Ordinal Plural Rules
// other: @integer 0~15, 100, 1000, 10000, 100000, 1000000, …
TEST(IntlPluralRules, SelectHsbOrdinal)
{
  PluralRulesOptions options;
  options.mPluralType = PluralRules::Type::Ordinal;
  UniquePtr<PluralRules> pr = PluralRules::TryCreate("hsb", options).unwrap();

  ASSERT_EQ(pr->Select(00.00).unwrap(), PluralRules::Keyword::Other);

  ASSERT_EQ(pr->Select(01.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(11.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(21.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(31.00).unwrap(), PluralRules::Keyword::Other);

  ASSERT_EQ(pr->Select(02.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(12.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(22.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(32.00).unwrap(), PluralRules::Keyword::Other);

  ASSERT_EQ(pr->Select(03.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(04.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(09.00).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(23.00).unwrap(), PluralRules::Keyword::Other);

  ASSERT_EQ(pr->Select(1000000).unwrap(), PluralRules::Keyword::Other);

  ASSERT_EQ(pr->Select(999999).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(1000005).unwrap(), PluralRules::Keyword::Other);
}

TEST(IntlPluralRules, DefaultFractionDigits)
{
  PluralRulesOptions defaultOptions;
  UniquePtr<PluralRules> pr =
      PluralRules::TryCreate("en", defaultOptions).unwrap();

  ASSERT_EQ(pr->Select(1.000).unwrap(), PluralRules::Keyword::One);

  ASSERT_EQ(pr->Select(1.100).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(1.010).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(1.001).unwrap(), PluralRules::Keyword::Other);

  ASSERT_EQ(pr->Select(0.900).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(0.990).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(0.999).unwrap(), PluralRules::Keyword::Other);
}

TEST(IntlPluralRules, MaxFractionDigitsZero)
{
  PluralRulesOptions options;
  options.mFractionDigits = Some(std::pair<uint32_t, uint32_t>(0, 0));
  UniquePtr<PluralRules> pr = PluralRules::TryCreate("en", options).unwrap();

  ASSERT_EQ(pr->Select(1.000).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(1.100).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(1.010).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(1.001).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(0.900).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(0.990).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(0.999).unwrap(), PluralRules::Keyword::One);
}

TEST(IntlPluralRules, MaxFractionDigitsOne)
{
  PluralRulesOptions options;
  options.mFractionDigits = Some(std::pair<uint32_t, uint32_t>(0, 1));
  UniquePtr<PluralRules> pr = PluralRules::TryCreate("en", options).unwrap();

  ASSERT_EQ(pr->Select(1.000).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(1.010).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(1.001).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(0.990).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(0.999).unwrap(), PluralRules::Keyword::One);

  ASSERT_EQ(pr->Select(1.100).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(0.900).unwrap(), PluralRules::Keyword::Other);
}

TEST(IntlPluralRules, MaxSignificantDigitsOne)
{
  PluralRulesOptions options;
  options.mSignificantDigits = Some(std::pair<uint32_t, uint32_t>(1, 1));
  UniquePtr<PluralRules> pr = PluralRules::TryCreate("en", options).unwrap();

  ASSERT_EQ(pr->Select(1.000).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(1.100).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(1.010).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(1.001).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(0.990).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(0.999).unwrap(), PluralRules::Keyword::One);

  ASSERT_EQ(pr->Select(0.900).unwrap(), PluralRules::Keyword::Other);
}

TEST(IntlPluralRules, MaxFractionDigitsTwo)
{
  PluralRulesOptions options;
  options.mFractionDigits = Some(std::pair<uint32_t, uint32_t>(0, 2));
  UniquePtr<PluralRules> pr = PluralRules::TryCreate("en", options).unwrap();

  ASSERT_EQ(pr->Select(1.000).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(1.001).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(0.999).unwrap(), PluralRules::Keyword::One);

  ASSERT_EQ(pr->Select(1.100).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(1.010).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(0.900).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(0.990).unwrap(), PluralRules::Keyword::Other);
}

TEST(IntlPluralRules, MaxSignificantDigitsTwo)
{
  PluralRulesOptions options;
  options.mSignificantDigits = Some(std::pair<uint32_t, uint32_t>(1, 2));
  UniquePtr<PluralRules> pr = PluralRules::TryCreate("en", options).unwrap();

  ASSERT_EQ(pr->Select(1.000).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(1.010).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(1.001).unwrap(), PluralRules::Keyword::One);
  ASSERT_EQ(pr->Select(0.999).unwrap(), PluralRules::Keyword::One);

  ASSERT_EQ(pr->Select(1.100).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(0.900).unwrap(), PluralRules::Keyword::Other);
  ASSERT_EQ(pr->Select(0.990).unwrap(), PluralRules::Keyword::Other);
}

}  // namespace intl
}  // namespace mozilla
