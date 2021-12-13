/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "TestBuffer.h"
#include "mozilla/intl/DisplayNames.h"

namespace mozilla::intl {

TEST(IntlDisplayNames, Script)
{
  TestBuffer<char16_t> buffer;

  DisplayNames::Options options{};
  options.style = DisplayNames::Style::Long;

  {
    auto result = DisplayNames::TryCreate("en-US", options);
    ASSERT_TRUE(result.isOk());
    auto displayNames = result.unwrap();

    ASSERT_TRUE(displayNames->GetScript(buffer, MakeStringSpan("Hans")).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"Simplified Han"));

    buffer.clear();
    {
      // The code is too long here.
      auto err =
          displayNames->GetScript(buffer, MakeStringSpan("ThisIsTooLong"));
      ASSERT_TRUE(err.isErr());
      ASSERT_EQ(err.unwrapErr(), DisplayNamesError::InvalidOption);
      ASSERT_TRUE(buffer.verboseMatches(u""));
    }

    // Test fallbacking for unknown scripts.

    buffer.clear();
    ASSERT_TRUE(displayNames
                    ->GetScript(buffer, MakeStringSpan("Fake"),
                                DisplayNames::Fallback::None)
                    .isOk());
    ASSERT_TRUE(buffer.verboseMatches(u""));

    buffer.clear();
    ASSERT_TRUE(displayNames
                    ->GetScript(buffer, MakeStringSpan("Fake"),
                                DisplayNames::Fallback::Code)
                    .isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"Fake"));
  }

  {
    auto result = DisplayNames::TryCreate("es-ES", options);
    ASSERT_TRUE(result.isOk());
    auto displayNames = result.unwrap();

    buffer.clear();
    ASSERT_TRUE(displayNames->GetScript(buffer, MakeStringSpan("Hans")).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"han simplificado"));
  }

  options.style = DisplayNames::Style::Short;
  {
    auto result = DisplayNames::TryCreate("en-US", options);
    ASSERT_TRUE(result.isOk());
    auto displayNames = result.unwrap();

    buffer.clear();
    ASSERT_TRUE(displayNames->GetScript(buffer, MakeStringSpan("Hans")).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"Simplified"));

    // Test fallbacking for unknown scripts.
    buffer.clear();
    ASSERT_TRUE(displayNames
                    ->GetScript(buffer, MakeStringSpan("Fake"),
                                DisplayNames::Fallback::None)
                    .isOk());
    ASSERT_TRUE(buffer.verboseMatches(u""));

    buffer.clear();
    ASSERT_TRUE(displayNames
                    ->GetScript(buffer, MakeStringSpan("Fake"),
                                DisplayNames::Fallback::Code)
                    .isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"Fake"));
  }

  {
    auto result = DisplayNames::TryCreate("es-ES", options);
    ASSERT_TRUE(result.isOk());
    auto displayNames = result.unwrap();

    buffer.clear();
    ASSERT_TRUE(displayNames->GetScript(buffer, MakeStringSpan("Hans")).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"simplificado"));
  }
}

TEST(IntlDisplayNames, Language)
{
  TestBuffer<char16_t> buffer;
  DisplayNames::Options options{};

  {
    auto result = DisplayNames::TryCreate("en-US", options);
    ASSERT_TRUE(result.isOk());
    auto displayNames = result.unwrap();

    ASSERT_TRUE(
        displayNames->GetLanguage(buffer, MakeStringSpan("es-ES")).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"Spanish (Spain)"));

    buffer.clear();
    ASSERT_TRUE(
        displayNames->GetLanguage(buffer, MakeStringSpan("zh-Hant")).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"Chinese (Traditional)"));

    // The undefined locale returns an empty string.
    buffer.clear();
    ASSERT_TRUE(
        displayNames->GetLanguage(buffer, MakeStringSpan("und")).isOk());
    ASSERT_TRUE(buffer.get_string_view().empty());

    // Invalid locales are an error.
    buffer.clear();
    ASSERT_EQ(
        displayNames->GetLanguage(buffer, MakeStringSpan("asdf")).unwrapErr(),
        DisplayNamesError::InvalidOption);
    ASSERT_TRUE(buffer.get_string_view().empty());

    // Unknown locales return an empty string.
    buffer.clear();
    ASSERT_TRUE(displayNames
                    ->GetLanguage(buffer, MakeStringSpan("zz"),
                                  DisplayNames::Fallback::None)
                    .isOk());
    ASSERT_TRUE(buffer.verboseMatches(u""));

    // Unknown locales can fallback to the language code.
    buffer.clear();
    ASSERT_TRUE(displayNames
                    ->GetLanguage(buffer, MakeStringSpan("zz-US"),
                                  DisplayNames::Fallback::Code)
                    .isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"zz-US"));

    // Unknown locales with a unicode extension error. Is this correct?
    buffer.clear();
    ASSERT_TRUE(displayNames
                    ->GetLanguage(buffer, MakeStringSpan("zz-US-u-ca-chinese"),
                                  DisplayNames::Fallback::Code)
                    .isErr());
    ASSERT_TRUE(buffer.verboseMatches(u""));
  }
  {
    auto result = DisplayNames::TryCreate("es-ES", options);
    ASSERT_TRUE(result.isOk());
    auto displayNames = result.unwrap();

    buffer.clear();
    ASSERT_TRUE(
        displayNames->GetLanguage(buffer, MakeStringSpan("es-ES")).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"español (España)"));

    buffer.clear();
    ASSERT_TRUE(
        displayNames->GetLanguage(buffer, MakeStringSpan("zh-Hant")).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"chino (tradicional)"));
  }
}

TEST(IntlDisplayNames, Region)
{
  TestBuffer<char16_t> buffer;

  DisplayNames::Options options{};
  options.style = DisplayNames::Style::Long;

  {
    auto result = DisplayNames::TryCreate("en-US", options);
    ASSERT_TRUE(result.isOk());
    auto displayNames = result.unwrap();

    ASSERT_TRUE(displayNames->GetRegion(buffer, MakeStringSpan("US")).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"United States"));

    ASSERT_TRUE(displayNames->GetRegion(buffer, MakeStringSpan("ES")).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"Spain"));

    ASSERT_TRUE(displayNames
                    ->GetRegion(buffer, MakeStringSpan("ZX"),
                                DisplayNames::Fallback::None)
                    .isOk());
    ASSERT_TRUE(buffer.verboseMatches(u""));

    ASSERT_TRUE(displayNames
                    ->GetRegion(buffer, MakeStringSpan("ZX"),
                                DisplayNames::Fallback::Code)
                    .isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"ZX"));
  }
  {
    auto result = DisplayNames::TryCreate("es-ES", options);
    ASSERT_TRUE(result.isOk());
    auto displayNames = result.unwrap();

    ASSERT_TRUE(displayNames->GetRegion(buffer, MakeStringSpan("US")).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"Estados Unidos"));

    ASSERT_TRUE(displayNames->GetRegion(buffer, MakeStringSpan("ES")).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"España"));
  }
}

TEST(IntlDisplayNames, Currency)
{
  TestBuffer<char16_t> buffer;

  DisplayNames::Options options{};
  options.style = DisplayNames::Style::Long;

  auto result = DisplayNames::TryCreate("en-US", options);
  ASSERT_TRUE(result.isOk());
  auto displayNames = result.unwrap();
  ASSERT_TRUE(displayNames->GetCurrency(buffer, MakeStringSpan("EUR")).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Euro"));

  ASSERT_TRUE(displayNames->GetCurrency(buffer, MakeStringSpan("USD")).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"US Dollar"));

  ASSERT_TRUE(displayNames
                  ->GetCurrency(buffer, MakeStringSpan("moz"),
                                DisplayNames::Fallback::None)
                  .isOk());
  ASSERT_TRUE(buffer.verboseMatches(u""));

  ASSERT_TRUE(displayNames
                  ->GetCurrency(buffer, MakeStringSpan("moz"),
                                DisplayNames::Fallback::Code)
                  .isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"MOZ"));

  // Invalid options.
  {
    // Code with fewer than 3 characters.
    auto err = displayNames->GetCurrency(buffer, MakeStringSpan("US"));
    ASSERT_TRUE(err.isErr());
    ASSERT_EQ(err.unwrapErr(), DisplayNamesError::InvalidOption);
  }
  {
    // Code with more than 3 characters.
    auto err = displayNames->GetCurrency(buffer, MakeStringSpan("USDDDDDDD"));
    ASSERT_TRUE(err.isErr());
    ASSERT_EQ(err.unwrapErr(), DisplayNamesError::InvalidOption);
  }
  {
    // Code with non-ascii alpha letters/
    auto err = displayNames->GetCurrency(buffer, MakeStringSpan("US1"));
    ASSERT_TRUE(err.isErr());
    ASSERT_EQ(err.unwrapErr(), DisplayNamesError::InvalidOption);
  }
}

TEST(IntlDisplayNames, Calendar)
{
  TestBuffer<char16_t> buffer;

  DisplayNames::Options options{};
  auto result = DisplayNames::TryCreate("en-US", options);
  ASSERT_TRUE(result.isOk());
  auto displayNames = result.unwrap();

  ASSERT_TRUE(
      displayNames->GetCalendar(buffer, MakeStringSpan("buddhist")).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Buddhist Calendar"));

  ASSERT_TRUE(
      displayNames->GetCalendar(buffer, MakeStringSpan("gregory")).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Gregorian Calendar"));

  ASSERT_TRUE(
      displayNames->GetCalendar(buffer, MakeStringSpan("GREGORY")).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Gregorian Calendar"));

  {
    // Code with non-ascii alpha letters.
    auto err =
        displayNames->GetCalendar(buffer, MakeStringSpan("🥸 not ascii"));
    ASSERT_TRUE(err.isErr());
    ASSERT_EQ(err.unwrapErr(), DisplayNamesError::InvalidOption);
  }
  {
    // Empty string.
    auto err = displayNames->GetCalendar(buffer, MakeStringSpan(""));
    ASSERT_TRUE(err.isErr());
    ASSERT_EQ(err.unwrapErr(), DisplayNamesError::InvalidOption);
  }
  {
    // Non-valid ascii.
    auto err = displayNames->GetCalendar(
        buffer, MakeStringSpan("ascii-but_not(valid)1234"));
    ASSERT_TRUE(err.isErr());
    ASSERT_EQ(err.unwrapErr(), DisplayNamesError::InvalidOption);
  }

  // Test fallbacking.

  ASSERT_TRUE(displayNames
                  ->GetCalendar(buffer, MakeStringSpan("moz"),
                                DisplayNames::Fallback::None)
                  .isOk());
  ASSERT_TRUE(buffer.verboseMatches(u""));

  ASSERT_TRUE(displayNames
                  ->GetCalendar(buffer, MakeStringSpan("moz"),
                                DisplayNames::Fallback::Code)
                  .isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"moz"));

  ASSERT_TRUE(displayNames
                  ->GetCalendar(buffer, MakeStringSpan("MOZ"),
                                DisplayNames::Fallback::Code)
                  .isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"moz"));
}

TEST(IntlDisplayNames, Weekday)
{
  TestBuffer<char16_t> buffer;

  DisplayNames::Options options{};
  Span<const char> calendar{};
  auto result = DisplayNames::TryCreate("en-US", options);
  ASSERT_TRUE(result.isOk());
  auto displayNames = result.unwrap();

  ASSERT_TRUE(
      displayNames->GetWeekday(buffer, Weekday::Monday, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Monday"));
  ASSERT_TRUE(
      displayNames->GetWeekday(buffer, Weekday::Tuesday, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Tuesday"));
  ASSERT_TRUE(
      displayNames->GetWeekday(buffer, Weekday::Wednesday, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Wednesday"));
  ASSERT_TRUE(
      displayNames->GetWeekday(buffer, Weekday::Thursday, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Thursday"));
  ASSERT_TRUE(
      displayNames->GetWeekday(buffer, Weekday::Friday, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Friday"));
  ASSERT_TRUE(
      displayNames->GetWeekday(buffer, Weekday::Saturday, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Saturday"));
  ASSERT_TRUE(
      displayNames->GetWeekday(buffer, Weekday::Sunday, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Sunday"));
}

TEST(IntlDisplayNames, WeekdaySpanish)
{
  TestBuffer<char16_t> buffer;

  DisplayNames::Options options{};
  Span<const char> calendar{};
  auto result = DisplayNames::TryCreate("es-ES", options);
  ASSERT_TRUE(result.isOk());
  auto displayNames = result.unwrap();

  ASSERT_TRUE(
      displayNames->GetWeekday(buffer, Weekday::Monday, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"lunes"));
  ASSERT_TRUE(
      displayNames->GetWeekday(buffer, Weekday::Tuesday, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"martes"));
  ASSERT_TRUE(
      displayNames->GetWeekday(buffer, Weekday::Wednesday, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"miércoles"));
  ASSERT_TRUE(
      displayNames->GetWeekday(buffer, Weekday::Thursday, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"jueves"));
  ASSERT_TRUE(
      displayNames->GetWeekday(buffer, Weekday::Friday, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"viernes"));
  ASSERT_TRUE(
      displayNames->GetWeekday(buffer, Weekday::Saturday, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"sábado"));
  ASSERT_TRUE(
      displayNames->GetWeekday(buffer, Weekday::Sunday, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"domingo"));
}

TEST(IntlDisplayNames, Month)
{
  TestBuffer<char16_t> buffer;

  DisplayNames::Options options{};
  Span<const char> calendar{};
  auto result = DisplayNames::TryCreate("en-US", options);
  ASSERT_TRUE(result.isOk());
  auto displayNames = result.unwrap();

  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::January, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"January"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::February, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"February"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::March, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"March"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::April, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"April"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::May, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"May"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::June, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"June"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::July, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"July"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::August, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"August"));
  ASSERT_TRUE(
      displayNames->GetMonth(buffer, Month::September, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"September"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::October, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"October"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::November, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"November"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::December, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"December"));
  ASSERT_TRUE(
      displayNames->GetMonth(buffer, Month::Undecimber, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u""));
}

TEST(IntlDisplayNames, MonthHebrew)
{
  TestBuffer<char16_t> buffer;

  DisplayNames::Options options{};
  Span<const char> calendar{};
  auto result = DisplayNames::TryCreate("en-u-ca-hebrew", options);
  ASSERT_TRUE(result.isOk());
  auto displayNames = result.unwrap();

  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::January, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Tishri"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::February, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Heshvan"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::March, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Kislev"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::April, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Tevet"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::May, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Shevat"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::June, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Adar I"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::July, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Adar"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::August, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Nisan"));
  ASSERT_TRUE(
      displayNames->GetMonth(buffer, Month::September, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Iyar"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::October, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Sivan"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::November, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Tamuz"));
  ASSERT_TRUE(displayNames->GetMonth(buffer, Month::December, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Av"));
  ASSERT_TRUE(
      displayNames->GetMonth(buffer, Month::Undecimber, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"Elul"));
}

TEST(IntlDisplayNames, MonthCalendarOption)
{
  TestBuffer<char16_t> buffer;

  {
    // No calendar.
    DisplayNames::Options options{};
    Span<const char> calendar{};
    auto result = DisplayNames::TryCreate("en", options);
    ASSERT_TRUE(result.isOk());
    auto displayNames = result.unwrap();

    ASSERT_TRUE(
        displayNames->GetMonth(buffer, Month::January, calendar).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"January"));
  }
  {
    // Switch to hebrew.
    DisplayNames::Options options{};
    Span<const char> calendar = MakeStringSpan("hebrew");
    auto result = DisplayNames::TryCreate("en", options);
    ASSERT_TRUE(result.isOk());
    auto displayNames = result.unwrap();

    ASSERT_TRUE(
        displayNames->GetMonth(buffer, Month::January, calendar).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"Tishri"));
  }
  {
    // Conflicting tags.
    DisplayNames::Options options{};
    Span<const char> calendar = MakeStringSpan("hebrew");
    auto result = DisplayNames::TryCreate("en-u-ca-gregory", options);
    ASSERT_TRUE(result.isOk());
    auto displayNames = result.unwrap();

    ASSERT_TRUE(
        displayNames->GetMonth(buffer, Month::January, calendar).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"Tishri"));
  }
  {
    // Conflicting tags.
    DisplayNames::Options options{};
    Span<const char> calendar = MakeStringSpan("gregory");
    auto result = DisplayNames::TryCreate("en-u-ca-hebrew", options);
    ASSERT_TRUE(result.isOk());
    auto displayNames = result.unwrap();

    ASSERT_TRUE(
        displayNames->GetMonth(buffer, Month::January, calendar).isOk());
    ASSERT_TRUE(buffer.verboseMatches(u"January"));
  }
}

TEST(IntlDisplayNames, Quarter)
{
  TestBuffer<char16_t> buffer;

  DisplayNames::Options options{};
  Span<const char> calendar{};
  auto result = DisplayNames::TryCreate("en-US", options);
  ASSERT_TRUE(result.isOk());
  auto displayNames = result.unwrap();

  ASSERT_TRUE(displayNames->GetQuarter(buffer, Quarter::Q1, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"1st quarter"));
  ASSERT_TRUE(displayNames->GetQuarter(buffer, Quarter::Q2, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"2nd quarter"));
  ASSERT_TRUE(displayNames->GetQuarter(buffer, Quarter::Q3, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"3rd quarter"));
  ASSERT_TRUE(displayNames->GetQuarter(buffer, Quarter::Q4, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"4th quarter"));
}

TEST(IntlDisplayNames, DayPeriod_en_US)
{
  TestBuffer<char16_t> buffer;

  DisplayNames::Options options{};
  Span<const char> calendar{};
  auto result = DisplayNames::TryCreate("en-US", options);
  ASSERT_TRUE(result.isOk());
  auto displayNames = result.unwrap();

  ASSERT_TRUE(
      displayNames->GetDayPeriod(buffer, DayPeriod::AM, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"AM"));
  ASSERT_TRUE(
      displayNames->GetDayPeriod(buffer, DayPeriod::PM, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"PM"));
}

TEST(IntlDisplayNames, DayPeriod_ar)
{
  TestBuffer<char16_t> buffer;
  DisplayNames::Options options{};
  Span<const char> calendar{};
  auto result = DisplayNames::TryCreate("ar", options);
  ASSERT_TRUE(result.isOk());
  auto displayNames = result.unwrap();

  ASSERT_TRUE(
      displayNames->GetDayPeriod(buffer, DayPeriod::AM, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"ص"));
  ASSERT_TRUE(
      displayNames->GetDayPeriod(buffer, DayPeriod::PM, calendar).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"م"));
}

TEST(IntlDisplayNames, DateTimeField)
{
  TestBuffer<char16_t> buffer;

  DisplayNames::Options options{};
  Span<const char> locale = MakeStringSpan("en-US");
  auto result = DisplayNames::TryCreate(locale.data(), options);
  ASSERT_TRUE(result.isOk());
  auto displayNames = result.unwrap();
  auto gen = DateTimePatternGenerator::TryCreate(locale.data()).unwrap();

  ASSERT_TRUE(
      displayNames->GetDateTimeField(buffer, DateTimeField::Year, *gen).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"year"));
  ASSERT_TRUE(
      displayNames->GetDateTimeField(buffer, DateTimeField::Quarter, *gen)
          .isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"quarter"));
  ASSERT_TRUE(displayNames->GetDateTimeField(buffer, DateTimeField::Month, *gen)
                  .isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"month"));
  ASSERT_TRUE(
      displayNames->GetDateTimeField(buffer, DateTimeField::WeekOfYear, *gen)
          .isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"week"));
  ASSERT_TRUE(
      displayNames->GetDateTimeField(buffer, DateTimeField::Weekday, *gen)
          .isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"day of the week"));
  ASSERT_TRUE(
      displayNames->GetDateTimeField(buffer, DateTimeField::Day, *gen).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"day"));
  ASSERT_TRUE(
      displayNames->GetDateTimeField(buffer, DateTimeField::DayPeriod, *gen)
          .isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"AM/PM"));
  ASSERT_TRUE(
      displayNames->GetDateTimeField(buffer, DateTimeField::Hour, *gen).isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"hour"));
  ASSERT_TRUE(
      displayNames->GetDateTimeField(buffer, DateTimeField::Minute, *gen)
          .isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"minute"));
  ASSERT_TRUE(
      displayNames->GetDateTimeField(buffer, DateTimeField::Second, *gen)
          .isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"second"));
  ASSERT_TRUE(
      displayNames->GetDateTimeField(buffer, DateTimeField::TimeZoneName, *gen)
          .isOk());
  ASSERT_TRUE(buffer.verboseMatches(u"time zone"));
}

}  // namespace mozilla::intl
