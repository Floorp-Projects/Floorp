/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OSPreferences.h"
#include "mozilla/intl/LocaleService.h"
#include "nsReadableUtils.h"

#include <windows.h>

using namespace mozilla::intl;

bool
OSPreferences::ReadSystemLocales(nsTArray<nsCString>& aLocaleList)
{
  MOZ_ASSERT(aLocaleList.IsEmpty());

  WCHAR locale[LOCALE_NAME_MAX_LENGTH];
  if (NS_WARN_IF(!LCIDToLocaleName(LOCALE_SYSTEM_DEFAULT, locale,
                                   LOCALE_NAME_MAX_LENGTH, 0))) {
    return false;
  }

  NS_LossyConvertUTF16toASCII loc(locale);

  if (CanonicalizeLanguageTag(loc)) {
    aLocaleList.AppendElement(loc);
    return true;
  }
  return false;
}

/**
 * Windows distinguishes between System Locale (the locale OS is in), and
 * User Locale (the locale used for regional settings etc.).
 *
 * For DateTimePattern, we want to retrieve the User Locale.
 */
static void
ReadUserLocale(nsCString& aRetVal)
{
  WCHAR locale[LOCALE_NAME_MAX_LENGTH];
  if (NS_WARN_IF(!LCIDToLocaleName(LOCALE_USER_DEFAULT, locale,
                                   LOCALE_NAME_MAX_LENGTH, 0))) {
    aRetVal.Assign("en-US");
    return;
  }

  LossyCopyUTF16toASCII(locale, aRetVal);
}

static LCTYPE
ToDateLCType(OSPreferences::DateTimeFormatStyle aFormatStyle)
{
  switch (aFormatStyle) {
    case OSPreferences::DateTimeFormatStyle::None:
      return LOCALE_SLONGDATE;
    case OSPreferences::DateTimeFormatStyle::Short:
      return LOCALE_SSHORTDATE;
    case OSPreferences::DateTimeFormatStyle::Medium:
      return LOCALE_SSHORTDATE;
    case OSPreferences::DateTimeFormatStyle::Long:
      return LOCALE_SLONGDATE;
    case OSPreferences::DateTimeFormatStyle::Full:
      return LOCALE_SLONGDATE;
    case OSPreferences::DateTimeFormatStyle::Invalid:
    default:
      MOZ_ASSERT_UNREACHABLE("invalid date format");
      return LOCALE_SLONGDATE;
  }
}

static LCTYPE
ToTimeLCType(OSPreferences::DateTimeFormatStyle aFormatStyle)
{
  switch (aFormatStyle) {
    case OSPreferences::DateTimeFormatStyle::None:
      return LOCALE_STIMEFORMAT;
    case OSPreferences::DateTimeFormatStyle::Short:
      return LOCALE_SSHORTTIME;
    case OSPreferences::DateTimeFormatStyle::Medium:
      return LOCALE_SSHORTTIME;
    case OSPreferences::DateTimeFormatStyle::Long:
      return LOCALE_STIMEFORMAT;
    case OSPreferences::DateTimeFormatStyle::Full:
      return LOCALE_STIMEFORMAT;
    case OSPreferences::DateTimeFormatStyle::Invalid:
    default:
      MOZ_ASSERT_UNREACHABLE("invalid time format");
      return LOCALE_STIMEFORMAT;
  }
}

LPWSTR
GetWindowsLocaleFor(const nsACString& aLocale, LPWSTR aBuffer)
{
  nsAutoCString reqLocale;
  nsAutoCString userLocale;
  ReadUserLocale(userLocale);

  if (aLocale.IsEmpty()) {
    LocaleService::GetInstance()->GetAppLocaleAsBCP47(reqLocale);
  } else {
    reqLocale.Assign(aLocale);
  }

  bool match = LocaleService::LanguagesMatch(reqLocale, userLocale);
  if (match || reqLocale.Length() >= LOCALE_NAME_MAX_LENGTH) {
    UTF8ToUnicodeBuffer(userLocale, (char16_t*)aBuffer);
  } else {
    UTF8ToUnicodeBuffer(reqLocale, (char16_t*)aBuffer);
  }

  return aBuffer;
}

/**
 * Windows API includes regional preferences from the user only
 * if we pass empty locale string or if the locale string matches
 * the current locale.
 *
 * Since Windows API only allows us to retrieve two options - short/long
 * we map it to our four options as:
 *
 *   short  -> short
 *   medium -> short
 *   long   -> long
 *   full   -> long
 *
 * In order to produce a single date/time format, we use CLDR pattern
 * for combined date/time string, since Windows API does not provide an
 * option for this.
 */
bool
OSPreferences::ReadDateTimePattern(DateTimeFormatStyle aDateStyle,
                                   DateTimeFormatStyle aTimeStyle,
                                   const nsACString& aLocale, nsAString& aRetVal)
{
  WCHAR buffer[LOCALE_NAME_MAX_LENGTH];

  LPWSTR localeName = GetWindowsLocaleFor(aLocale, buffer);

  bool isDate = aDateStyle != DateTimeFormatStyle::None &&
                aDateStyle != DateTimeFormatStyle::Invalid;
  bool isTime = aTimeStyle != DateTimeFormatStyle::None &&
                aTimeStyle != DateTimeFormatStyle::Invalid;

  // If both date and time are wanted, we'll initially read them into a
  // local string, and then insert them into the overall date+time pattern;
  // but if only one is needed we'll work directly with the return value.
  // Set 'str' to point to the string we will use to retrieve patterns
  // from Windows.
  nsAutoString tmpStr;
  nsAString* str;
  if (isDate && isTime) {
    if (!GetDateTimeConnectorPattern(NS_ConvertUTF16toUTF8(localeName), aRetVal)) {
      NS_WARNING("failed to get date/time connector");
      aRetVal.AssignLiteral(u"{1} {0}");
    }
    str = &tmpStr;
  } else if (isDate || isTime) {
    str = &aRetVal;
  } else {
    aRetVal.Truncate(0);
    return true;
  }

  if (isDate) {
    LCTYPE lcType = ToDateLCType(aDateStyle);
    size_t len = GetLocaleInfoEx(localeName, lcType, nullptr, 0);
    if (len == 0) {
      return false;
    }

    // We're doing it to ensure the terminator will fit when Windows writes the data
    // to its output buffer. See bug 1358159 for details.
    str->SetLength(len);
    GetLocaleInfoEx(localeName, lcType, (WCHAR*)str->BeginWriting(), len);
    str->SetLength(len - 1); // -1 because len counts the null terminator

    // Windows uses "ddd" and "dddd" for abbreviated and full day names respectively,
    //   https://msdn.microsoft.com/en-us/library/windows/desktop/dd317787(v=vs.85).aspx
    // but in a CLDR/ICU-style pattern these should be "EEE" and "EEEE".
    //   http://userguide.icu-project.org/formatparse/datetime
    // So we fix that up here.
    nsAString::const_iterator start, pos, end;
    start = str->BeginReading(pos);
    str->EndReading(end);
    if (FindInReadable(NS_LITERAL_STRING("dddd"), pos, end)) {
      str->Replace(pos - start, 4, NS_LITERAL_STRING("EEEE"));
    } else if (FindInReadable(NS_LITERAL_STRING("ddd"), pos, end)) {
      str->Replace(pos - start, 3, NS_LITERAL_STRING("EEE"));
    }

    // Also, Windows uses lowercase "g" or "gg" for era, but ICU wants uppercase "G"
    // (it would interpret "g" as "modified Julian day"!). So fix that.
    int32_t index = str->FindChar('g');
    if (index >= 0) {
      str->Replace(index, 1, 'G');
      // If it was a double "gg", just drop the second one.
      index++;
      if (str->CharAt(index) == 'g') {
        str->Cut(index, 1);
      }
    }

    // If time was also requested, we need to substitute the date pattern from Windows
    // into the date+time format that we have in aRetVal.
    if (isTime) {
      nsAString::const_iterator start, pos, end;
      start = aRetVal.BeginReading(pos);
      aRetVal.EndReading(end);
      if (FindInReadable(NS_LITERAL_STRING("{1}"), pos, end)) {
        aRetVal.Replace(pos - start, 3, tmpStr);
      }
    }
  }

  if (isTime) {
    LCTYPE lcType = ToTimeLCType(aTimeStyle);
    size_t len = GetLocaleInfoEx(localeName, lcType, nullptr, 0);
    if (len == 0) {
      return false;
    }

    // We're doing it to ensure the terminator will fit when Windows writes the data
    // to its output buffer. See bug 1358159 for details.
    str->SetLength(len);
    GetLocaleInfoEx(localeName, lcType, (WCHAR*)str->BeginWriting(), len);
    str->SetLength(len - 1);

    // Windows uses "t" or "tt" for a "time marker" (am/pm indicator),
    //   https://msdn.microsoft.com/en-us/library/windows/desktop/dd318148(v=vs.85).aspx
    // but in a CLDR/ICU-style pattern that should be "a".
    //   http://userguide.icu-project.org/formatparse/datetime
    // So we fix that up here.
    int32_t index = str->FindChar('t');
    if (index >= 0) {
      str->Replace(index, 1, 'a');
      index++;
      if (str->CharAt(index) == 't') {
        str->Cut(index, 1);
      }
    }

    if (isDate) {
      nsAString::const_iterator start, pos, end;
      start = aRetVal.BeginReading(pos);
      aRetVal.EndReading(end);
      if (FindInReadable(NS_LITERAL_STRING("{0}"), pos, end)) {
        aRetVal.Replace(pos - start, 3, tmpStr);
      }
    }
  }

  return true;
}
