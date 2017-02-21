#include "gtest/gtest.h"
#include "DateTimeFormat.h"

namespace mozilla {

TEST(DateTimeFormat, FormatPRExplodedTime) {
  PRTime prTime = 0;
  PRExplodedTime prExplodedTime;
  PR_ExplodeTime(prTime, PR_GMTParameters, &prExplodedTime);

  mozilla::DateTimeFormat::mLocale = new nsCString("en-US");

  nsAutoString formattedTime;
  nsresult rv = mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("January 1, 1970 at 12:00:00 AM", NS_ConvertUTF16toUTF8(formattedTime).get());

  prExplodedTime = { 0, 0, 19, 0, 1, 0, 1970, 4, 0, { (19 * 60), 0 } };
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("January 1, 1970 at 12:19:00 AM", NS_ConvertUTF16toUTF8(formattedTime).get());

  prExplodedTime = { 0, 0, 0, 7, 1, 0, 1970, 4, 0, { (6 * 60 * 60), (1 * 60 * 60) } };
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("January 1, 1970 at 7:00:00 AM", NS_ConvertUTF16toUTF8(formattedTime).get());

  prExplodedTime = { 0, 0, 29, 11, 1, 0, 1970, 4, 0, { (10 * 60 * 60) + (29 * 60), (1 * 60 * 60) } };
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("January 1, 1970 at 11:29:00 AM", NS_ConvertUTF16toUTF8(formattedTime).get());

  prExplodedTime = { 0, 0, 37, 23, 31, 11, 1969, 3, 364, { -(23 * 60), 0 } };
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("December 31, 1969 at 11:37:00 PM", NS_ConvertUTF16toUTF8(formattedTime).get());

  prExplodedTime = { 0, 0, 0, 17, 31, 11, 1969, 3, 364, { -(7 * 60 * 60), 0 } };
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("December 31, 1969 at 5:00:00 PM", NS_ConvertUTF16toUTF8(formattedTime).get());

  prExplodedTime = { 0, 0, 47, 14, 31, 11, 1969, 3, 364, { -((10 * 60 * 60) + (13 * 60)), (1 * 60 * 60) } };
  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatLong, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("December 31, 1969 at 2:47:00 PM", NS_ConvertUTF16toUTF8(formattedTime).get());
}

TEST(DateTimeFormat, DateFormatSelectors) {
  PRTime prTime = 0;
  PRExplodedTime prExplodedTime;
  PR_ExplodeTime(prTime, PR_GMTParameters, &prExplodedTime);

  mozilla::DateTimeFormat::mLocale = new nsCString("en-US");

  nsAutoString formattedTime;
  nsresult rv = mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatYearMonth, kTimeFormatNone, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("01/1970", NS_ConvertUTF16toUTF8(formattedTime).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatYearMonthLong, kTimeFormatNone, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("January 1970", NS_ConvertUTF16toUTF8(formattedTime).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatMonthLong, kTimeFormatNone, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("January", NS_ConvertUTF16toUTF8(formattedTime).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatYearMonth, kTimeFormatNoSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("01/1970, 12:00 AM", NS_ConvertUTF16toUTF8(formattedTime).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatYearMonth, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("01/1970, 12:00:00 AM", NS_ConvertUTF16toUTF8(formattedTime).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatWeekday, kTimeFormatNone, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("Thu", NS_ConvertUTF16toUTF8(formattedTime).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatWeekday, kTimeFormatNoSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("Thu 12:00 AM", NS_ConvertUTF16toUTF8(formattedTime).get());

  rv = mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatWeekday, kTimeFormatSeconds, &prExplodedTime, formattedTime);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_STREQ("Thu 12:00:00 AM", NS_ConvertUTF16toUTF8(formattedTime).get());
}

}
