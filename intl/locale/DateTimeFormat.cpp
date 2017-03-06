/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DateTimeFormat.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "mozilla/intl/LocaleService.h"
#include "unicode/udatpg.h"

namespace mozilla {

nsCString* DateTimeFormat::mLocale = nullptr;

/*static*/ nsresult
DateTimeFormat::Initialize()
{
  if (mLocale) {
    return NS_OK;
  }

  mLocale = new nsCString();
  nsAutoCString locale;
  intl::LocaleService::GetInstance()->GetAppLocale(locale);
  mLocale->Assign(locale);

  return NS_OK;
}

// performs a locale sensitive date formatting operation on the time_t parameter
/*static*/ nsresult
DateTimeFormat::FormatTime(const nsDateFormatSelector aDateFormatSelector,
                           const nsTimeFormatSelector aTimeFormatSelector,
                           const time_t aTimetTime,
                           nsAString& aStringOut)
{
  return FormatPRTime(aDateFormatSelector, aTimeFormatSelector, (aTimetTime * PR_USEC_PER_SEC), aStringOut);
}

// performs a locale sensitive date formatting operation on the PRTime parameter
/*static*/ nsresult
DateTimeFormat::FormatPRTime(const nsDateFormatSelector aDateFormatSelector,
                             const nsTimeFormatSelector aTimeFormatSelector,
                             const PRTime aPrTime,
                             nsAString& aStringOut)
{
  return FormatUDateTime(aDateFormatSelector, aTimeFormatSelector, (aPrTime / PR_USEC_PER_MSEC), nullptr, aStringOut);
}

// performs a locale sensitive date formatting operation on the PRExplodedTime parameter
/*static*/ nsresult
DateTimeFormat::FormatPRExplodedTime(const nsDateFormatSelector aDateFormatSelector,
                                     const nsTimeFormatSelector aTimeFormatSelector,
                                     const PRExplodedTime* aExplodedTime,
                                     nsAString& aStringOut)
{
  return FormatUDateTime(aDateFormatSelector, aTimeFormatSelector, (PR_ImplodeTime(aExplodedTime) / PR_USEC_PER_MSEC), &(aExplodedTime->tm_params), aStringOut);
}

// performs a locale sensitive date formatting operation on the UDate parameter
/*static*/ nsresult
DateTimeFormat::FormatUDateTime(const nsDateFormatSelector aDateFormatSelector,
                                const nsTimeFormatSelector aTimeFormatSelector,
                                const UDate aUDateTime,
                                const PRTimeParameters* aTimeParameters,
                                nsAString& aStringOut)
{
  const int32_t DATETIME_FORMAT_INITIAL_LEN = 127;
  int32_t dateTimeLen = 0;
  nsresult rv = NS_OK;

  // return, nothing to format
  if (aDateFormatSelector == kDateFormatNone && aTimeFormatSelector == kTimeFormatNone) {
    aStringOut.Truncate();
    return NS_OK;
  }

  // set up locale data
  rv = Initialize();

  if (NS_FAILED(rv)) {
    return rv;
  }

  // Get the date style for the formatter:
  UDateFormatStyle dateStyle;
  switch (aDateFormatSelector) {
    case kDateFormatLong:
      dateStyle = UDAT_LONG;
      break;
    case kDateFormatShort:
      dateStyle = UDAT_SHORT;
      break;
    case kDateFormatYearMonth:
    case kDateFormatYearMonthLong:
    case kDateFormatMonthLong:
    case kDateFormatWeekday:
      dateStyle = UDAT_PATTERN;
      break;
    case kDateFormatNone:
      dateStyle = UDAT_NONE;
      break;
    default:
      NS_ERROR("Unknown nsDateFormatSelector");
      return NS_ERROR_ILLEGAL_VALUE;
  }

  // Get the time style for the formatter:
  UDateFormatStyle timeStyle;
  switch (aTimeFormatSelector) {
    case kTimeFormatSeconds:
      timeStyle = UDAT_MEDIUM;
      break;
    case kTimeFormatNoSeconds:
      timeStyle = UDAT_SHORT;
      break;
    case kTimeFormatNone:
      timeStyle = UDAT_NONE;
      break;
    default:
      NS_ERROR("Unknown nsTimeFormatSelector");
      return NS_ERROR_ILLEGAL_VALUE;
  }

  // generate date/time string

  nsAutoString timeZoneID(u"GMT");
  if (aTimeParameters) {
    int32_t totalOffsetMinutes = (aTimeParameters->tp_gmt_offset + aTimeParameters->tp_dst_offset) / 60;
    if (totalOffsetMinutes != 0) {
      char sign = totalOffsetMinutes < 0 ? '-' : '+';
      int32_t hours = abs(totalOffsetMinutes) / 60;
      int32_t minutes = abs(totalOffsetMinutes) % 60;
      timeZoneID.AppendPrintf("%c%02d:%02d", sign, hours, minutes);
    }
  }

  UErrorCode status = U_ZERO_ERROR;

  UDateFormat* dateTimeFormat;
  if (dateStyle == UDAT_PATTERN) {
    nsAutoString pattern;

    dateTimeFormat = udat_open(timeStyle, UDAT_NONE, mLocale->get(), nullptr, -1, nullptr, -1, &status);

    if (U_SUCCESS(status) && dateTimeFormat) {
      int32_t patternLength;
      if (timeStyle != UDAT_NONE) {
        pattern.SetLength(DATETIME_FORMAT_INITIAL_LEN);
        patternLength = udat_toPattern(dateTimeFormat, FALSE, reinterpret_cast<UChar*>(pattern.BeginWriting()), DATETIME_FORMAT_INITIAL_LEN, &status);
        pattern.SetLength(patternLength);

        if (status == U_BUFFER_OVERFLOW_ERROR) {
          status = U_ZERO_ERROR;
          udat_toPattern(dateTimeFormat, FALSE, reinterpret_cast<UChar*>(pattern.BeginWriting()), patternLength, &status);
        }
      }

      nsAutoString skeleton;
      switch (aDateFormatSelector) {
      case kDateFormatYearMonth:
        skeleton.AssignLiteral("yyyyMM ");
        break;
      case kDateFormatYearMonthLong:
        skeleton.AssignLiteral("yyyyMMMM ");
        break;
      case kDateFormatMonthLong:
        skeleton.AssignLiteral("MMMM ");
        break;
      case kDateFormatWeekday:
        skeleton.AssignLiteral("EEE ");
        break;
      default:
        break;
      }
      int32_t dateSkeletonLen = skeleton.Length();

      if (timeStyle != UDAT_NONE) {
        skeleton.SetLength(DATETIME_FORMAT_INITIAL_LEN);
        int32_t skeletonLength = udatpg_getSkeleton(nullptr, reinterpret_cast<const UChar*>(pattern.BeginReading()), patternLength,
          reinterpret_cast<UChar*>(skeleton.BeginWriting() + dateSkeletonLen), DATETIME_FORMAT_INITIAL_LEN - dateSkeletonLen, &status);
        skeleton.SetLength(dateSkeletonLen + skeletonLength);

        if (status == U_BUFFER_OVERFLOW_ERROR) {
          status = U_ZERO_ERROR;
          udatpg_getSkeleton(nullptr, reinterpret_cast<const UChar*>(pattern.BeginReading()), patternLength,
            reinterpret_cast<UChar*>(skeleton.BeginWriting() + dateSkeletonLen), dateSkeletonLen + skeletonLength, &status);
        }
      }

      UDateTimePatternGenerator* patternGenerator = udatpg_open(mLocale->get(), &status);
      if (U_SUCCESS(status)) {
        pattern.SetLength(DATETIME_FORMAT_INITIAL_LEN);
        patternLength = udatpg_getBestPattern(patternGenerator, reinterpret_cast<const UChar*>(skeleton.BeginReading()), skeleton.Length(), 
                                              reinterpret_cast<UChar*>(pattern.BeginWriting()), DATETIME_FORMAT_INITIAL_LEN, &status);
        pattern.SetLength(patternLength);

        if (status == U_BUFFER_OVERFLOW_ERROR) {
          status = U_ZERO_ERROR;
          udatpg_getBestPattern(patternGenerator, reinterpret_cast<const UChar*>(skeleton.BeginReading()), skeleton.Length(),
                                reinterpret_cast<UChar*>(pattern.BeginWriting()), patternLength, &status);
        }
      }

      udatpg_close(patternGenerator);
    }

    udat_close(dateTimeFormat);

    if (aTimeParameters) {
      dateTimeFormat = udat_open(UDAT_PATTERN, UDAT_PATTERN, mLocale->get(), reinterpret_cast<const UChar*>(timeZoneID.BeginReading()), timeZoneID.Length(), 
                                 reinterpret_cast<const UChar*>(pattern.BeginReading()), pattern.Length(), &status);
    } else {
      dateTimeFormat = udat_open(UDAT_PATTERN, UDAT_PATTERN, mLocale->get(), nullptr, -1, reinterpret_cast<const UChar*>(pattern.BeginReading()), pattern.Length(), &status);
    }
  } else {
    if (aTimeParameters) {
      dateTimeFormat = udat_open(timeStyle, dateStyle, mLocale->get(), reinterpret_cast<const UChar*>(timeZoneID.BeginReading()), timeZoneID.Length(), nullptr, -1, &status);
    } else {
      dateTimeFormat = udat_open(timeStyle, dateStyle, mLocale->get(), nullptr, -1, nullptr, -1, &status);
    }
  }

  if (U_SUCCESS(status) && dateTimeFormat) {
    aStringOut.SetLength(DATETIME_FORMAT_INITIAL_LEN);
    dateTimeLen = udat_format(dateTimeFormat, aUDateTime, reinterpret_cast<UChar*>(aStringOut.BeginWriting()), DATETIME_FORMAT_INITIAL_LEN, nullptr, &status);
    aStringOut.SetLength(dateTimeLen);

    if (status == U_BUFFER_OVERFLOW_ERROR) {
      status = U_ZERO_ERROR;
      udat_format(dateTimeFormat, aUDateTime, reinterpret_cast<UChar*>(aStringOut.BeginWriting()), dateTimeLen, nullptr, &status);
    }
  }

  if (U_FAILURE(status)) {
    rv = NS_ERROR_FAILURE;
  }

  if (dateTimeFormat) {
    udat_close(dateTimeFormat);
  }

  return rv;
}

/*static*/ void
DateTimeFormat::Shutdown()
{
  if (mLocale) {
    delete mLocale;
  }
}

}
