#include "gtest/gtest.h"
#include "DateTimeFormat.h"

namespace mozilla {

// Normalise time.
static nsAutoCString nt(nsAutoCString aDatetime) {
  nsAutoCString datetime = aDatetime;

  // Replace "January 01" with "January 1" (found on Windows).
  int32_t ind = datetime.Find("January 01");
  if (ind != kNotFound) datetime.ReplaceLiteral(ind, 10, "January 1");

  // Strip trailing " GMT" (found on Mac/Linux).
  ind = datetime.Find(" GMT");
  if (ind != kNotFound) datetime.Truncate(ind);

  // Strip leading "Thursday, " or "Wednesday, " (found on Windows).
  ind = datetime.Find("Thursday, ");
  if (ind == 0) datetime.ReplaceLiteral(0, 10, "");

  ind = datetime.Find("Wednesday, ");
  if (ind == 0) datetime.ReplaceLiteral(0, 11, "");

  return datetime;
}

TEST(DateTimeFormat, FormatPRExplodedTime)
{
  PRTime prTime = 0;
  PRExplodedTime prExplodedTime;
  PR_ExplodeTime(prTime, PR_GMTParameters, &prExplodedTime);

  mozilla::DateTimeFormat::mLocale = new nsCString("en-US");

  nsAutoString formattedTime;
  nsresult rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("January 1, 1970, 12:00:00 AM",
               nt(NS_ConvertUTF16toUTF8(formattedTime)).get());

  prExplodedTime = {0, 0, 19, 0, 1, 0, 1970, 4, 0, {(19 * 60), 0}};
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("January 1, 1970, 12:19:00 AM",
               nt(NS_ConvertUTF16toUTF8(formattedTime)).get());

  prExplodedTime = {0, 0,    0, 7, 1,
                    0, 1970, 4, 0, {(6 * 60 * 60), (1 * 60 * 60)}};
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("January 1, 1970, 7:00:00 AM",
               nt(NS_ConvertUTF16toUTF8(formattedTime)).get());

  prExplodedTime = {
      0, 0,    29, 11, 1,
      0, 1970, 4,  0,  {(10 * 60 * 60) + (29 * 60), (1 * 60 * 60)}};
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("January 1, 1970, 11:29:00 AM",
               nt(NS_ConvertUTF16toUTF8(formattedTime)).get());

  prExplodedTime = {0, 0, 37, 23, 31, 11, 1969, 3, 364, {-(23 * 60), 0}};
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("December 31, 1969, 11:37:00 PM",
               nt(NS_ConvertUTF16toUTF8(formattedTime)).get());

  prExplodedTime = {0, 0, 0, 17, 31, 11, 1969, 3, 364, {-(7 * 60 * 60), 0}};
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("December 31, 1969, 5:00:00 PM",
               nt(NS_ConvertUTF16toUTF8(formattedTime)).get());

  prExplodedTime = {
      0,  0,    47, 14,  31,
      11, 1969, 3,  364, {-((10 * 60 * 60) + (13 * 60)), (1 * 60 * 60)}};
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("December 31, 1969, 2:47:00 PM",
               nt(NS_ConvertUTF16toUTF8(formattedTime)).get());
}

TEST(DateTimeFormat, DateFormatSelectors)
{
  PRTime prTime = 0;
  PRExplodedTime prExplodedTime;
  PR_ExplodeTime(prTime, PR_GMTParameters, &prExplodedTime);

  mozilla::DateTimeFormat::mLocale = new nsCString("en-US");

  nsAutoString formattedTime;
  nsresult rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatYearMonth, kTimeFormatNone, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("01/1970", NS_ConvertUTF16toUTF8(formattedTime).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatYearMonthLong, kTimeFormatNone, &prExplodedTime,
      formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("January 1970", NS_ConvertUTF16toUTF8(formattedTime).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatMonthLong, kTimeFormatNone, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("January", NS_ConvertUTF16toUTF8(formattedTime).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatYearMonth, kTimeFormatNoSeconds, &prExplodedTime,
      formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("01/1970, 12:00 AM",
               nt(NS_ConvertUTF16toUTF8(formattedTime)).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatYearMonth, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("01/1970, 12:00:00 AM",
               nt(NS_ConvertUTF16toUTF8(formattedTime)).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatWeekday, kTimeFormatNone, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("Thu", NS_ConvertUTF16toUTF8(formattedTime).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatWeekday, kTimeFormatNoSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("Thu, 12:00 AM", nt(NS_ConvertUTF16toUTF8(formattedTime)).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatWeekday, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("Thu, 12:00:00 AM",
               nt(NS_ConvertUTF16toUTF8(formattedTime)).get());
}

// Normalise time.
static nsAutoCString ntd(nsAutoCString aDatetime) {
  nsAutoCString datetime = aDatetime;

  // Strip trailing " GMT" (found on Mac/Linux).
  int32_t ind = datetime.Find(" GMT");
  if (ind != kNotFound) datetime.Truncate(ind);

  // Strip leading "Donnerstag, " or "Mittwoch, " (found on Windows).
  ind = datetime.Find("Donnerstag, ");
  if (ind == 0) datetime.Replace(0, 12, "");
  ind = datetime.Find("Mittwoch, ");
  if (ind == 0) datetime.Replace(0, 10, "");

  return datetime;
}

TEST(DateTimeFormat, FormatPRExplodedTimeForeign)
{
  PRTime prTime = 0;
  PRExplodedTime prExplodedTime;
  PR_ExplodeTime(prTime, PR_GMTParameters, &prExplodedTime);

  mozilla::DateTimeFormat::mLocale = new nsCString("de-DE");

  nsAutoString formattedTime;
  nsresult rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("1. Januar 1970, 00:00:00",
               ntd(NS_ConvertUTF16toUTF8(formattedTime)).get());

  prExplodedTime = {0, 0, 19, 0, 1, 0, 1970, 4, 0, {(19 * 60), 0}};
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("1. Januar 1970, 00:19:00",
               ntd(NS_ConvertUTF16toUTF8(formattedTime)).get());

  prExplodedTime = {0, 0,    0, 7, 1,
                    0, 1970, 4, 0, {(6 * 60 * 60), (1 * 60 * 60)}};
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("1. Januar 1970, 07:00:00",
               ntd(NS_ConvertUTF16toUTF8(formattedTime)).get());

  prExplodedTime = {
      0, 0,    29, 11, 1,
      0, 1970, 4,  0,  {(10 * 60 * 60) + (29 * 60), (1 * 60 * 60)}};
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("1. Januar 1970, 11:29:00",
               ntd(NS_ConvertUTF16toUTF8(formattedTime)).get());

  prExplodedTime = {0, 0, 37, 23, 31, 11, 1969, 3, 364, {-(23 * 60), 0}};
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("31. Dezember 1969, 23:37:00",
               ntd(NS_ConvertUTF16toUTF8(formattedTime)).get());

  prExplodedTime = {0, 0, 0, 17, 31, 11, 1969, 3, 364, {-(7 * 60 * 60), 0}};
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("31. Dezember 1969, 17:00:00",
               ntd(NS_ConvertUTF16toUTF8(formattedTime)).get());

  prExplodedTime = {
      0,  0,    47, 14,  31,
      11, 1969, 3,  364, {-((10 * 60 * 60) + (13 * 60)), (1 * 60 * 60)}};
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("31. Dezember 1969, 14:47:00",
               ntd(NS_ConvertUTF16toUTF8(formattedTime)).get());
}

TEST(DateTimeFormat, DateFormatSelectorsForeign)
{
  PRTime prTime = 0;
  PRExplodedTime prExplodedTime;
  PR_ExplodeTime(prTime, PR_GMTParameters, &prExplodedTime);

  mozilla::DateTimeFormat::mLocale = new nsCString("de-DE");

  nsAutoString formattedTime;
  nsresult rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatYearMonth, kTimeFormatNone, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("01.1970", NS_ConvertUTF16toUTF8(formattedTime).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatYearMonthLong, kTimeFormatNone, &prExplodedTime,
      formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("Januar 1970", NS_ConvertUTF16toUTF8(formattedTime).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatMonthLong, kTimeFormatNone, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("Januar", NS_ConvertUTF16toUTF8(formattedTime).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatYearMonth, kTimeFormatNoSeconds, &prExplodedTime,
      formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("01.1970, 00:00",
               ntd(NS_ConvertUTF16toUTF8(formattedTime)).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatYearMonth, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("01.1970, 00:00:00",
               ntd(NS_ConvertUTF16toUTF8(formattedTime)).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatWeekday, kTimeFormatNone, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("Do", NS_ConvertUTF16toUTF8(formattedTime).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatWeekday, kTimeFormatNoSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("Do, 00:00", ntd(NS_ConvertUTF16toUTF8(formattedTime)).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(
      kDateFormatWeekday, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("Do, 00:00:00", ntd(NS_ConvertUTF16toUTF8(formattedTime)).get());
}

}  // namespace mozilla
