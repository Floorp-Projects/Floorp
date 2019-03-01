/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DateTimeFormat.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "mozilla/intl/LocaleService.h"
#include "OSPreferences.h"
#include "mozIOSPreferences.h"
#include "unicode/udatpg.h"

namespace mozilla {
using namespace mozilla::intl;

nsCString* DateTimeFormat::mLocale = nullptr;
nsDataHashtable<nsCStringHashKey, UDateFormat*>* DateTimeFormat::mFormatCache;

/*static*/
nsresult DateTimeFormat::Initialize() {
  if (mLocale) {
    return NS_OK;
  }

  mLocale = new nsCString();
  AutoTArray<nsCString, 10> regionalPrefsLocales;
  intl::LocaleService::GetInstance()->GetRegionalPrefsLocales(
      regionalPrefsLocales);
  mLocale->Assign(regionalPrefsLocales[0]);

  return NS_OK;
}

// performs a locale sensitive date formatting operation on the PRTime parameter
/*static*/
nsresult DateTimeFormat::FormatPRTime(
    const nsDateFormatSelector aDateFormatSelector,
    const nsTimeFormatSelector aTimeFormatSelector, const PRTime aPrTime,
    nsAString& aStringOut) {
  return FormatUDateTime(aDateFormatSelector, aTimeFormatSelector,
                         (aPrTime / PR_USEC_PER_MSEC), nullptr, aStringOut);
}

// performs a locale sensitive date formatting operation on the PRExplodedTime
// parameter
/*static*/
nsresult DateTimeFormat::FormatPRExplodedTime(
    const nsDateFormatSelector aDateFormatSelector,
    const nsTimeFormatSelector aTimeFormatSelector,
    const PRExplodedTime* aExplodedTime, nsAString& aStringOut) {
  return FormatUDateTime(aDateFormatSelector, aTimeFormatSelector,
                         (PR_ImplodeTime(aExplodedTime) / PR_USEC_PER_MSEC),
                         &(aExplodedTime->tm_params), aStringOut);
}

// performs a locale sensitive date formatting operation on the UDate parameter
/*static*/
nsresult DateTimeFormat::FormatUDateTime(
    const nsDateFormatSelector aDateFormatSelector,
    const nsTimeFormatSelector aTimeFormatSelector, const UDate aUDateTime,
    const PRTimeParameters* aTimeParameters, nsAString& aStringOut) {
  const int32_t DATETIME_FORMAT_INITIAL_LEN = 127;
  int32_t dateTimeLen = 0;
  nsresult rv = NS_OK;

  // return, nothing to format
  if (aDateFormatSelector == kDateFormatNone &&
      aTimeFormatSelector == kTimeFormatNone) {
    aStringOut.Truncate();
    return NS_OK;
  }

  // set up locale data
  rv = Initialize();

  if (NS_FAILED(rv)) {
    return rv;
  }

  UErrorCode status = U_ZERO_ERROR;

  nsAutoCString key;
  key.AppendInt((int)aDateFormatSelector);
  key.Append(':');
  key.AppendInt((int)aTimeFormatSelector);
  if (aTimeParameters) {
    key.Append(':');
    key.AppendInt(aTimeParameters->tp_gmt_offset);
    key.Append(':');
    key.AppendInt(aTimeParameters->tp_dst_offset);
  }

  if (mFormatCache && mFormatCache->Count() == kMaxCachedFormats) {
    // Don't allow a pathological page to extend the cache unreasonably.
    NS_WARNING("flushing UDateFormat cache");
    DeleteCache();
  }
  if (!mFormatCache) {
    mFormatCache =
        new nsDataHashtable<nsCStringHashKey, UDateFormat*>(kMaxCachedFormats);
  }

  UDateFormat*& dateTimeFormat = mFormatCache->GetOrInsert(key);

  if (!dateTimeFormat) {
    // We didn't have a cached formatter for this key, so create one.

    // Get the date style for the formatter.
    nsAutoString skeletonDate;
    nsAutoString patternDate;
    bool haveSkeleton = true;
    switch (aDateFormatSelector) {
      case kDateFormatLong:
        rv = OSPreferences::GetInstance()->GetDateTimePattern(
            mozIOSPreferences::dateTimeFormatStyleLong,
            mozIOSPreferences::dateTimeFormatStyleNone,
            nsDependentCString(mLocale->get()), patternDate);
        NS_ENSURE_SUCCESS(rv, rv);
        haveSkeleton = false;
        break;
      case kDateFormatShort:
        rv = OSPreferences::GetInstance()->GetDateTimePattern(
            mozIOSPreferences::dateTimeFormatStyleShort,
            mozIOSPreferences::dateTimeFormatStyleNone,
            nsDependentCString(mLocale->get()), patternDate);
        NS_ENSURE_SUCCESS(rv, rv);
        haveSkeleton = false;
        break;
      case kDateFormatYearMonth:
        skeletonDate.AssignLiteral("yyyyMM");
        break;
      case kDateFormatYearMonthLong:
        skeletonDate.AssignLiteral("yyyyMMMM");
        break;
      case kDateFormatMonthLong:
        skeletonDate.AssignLiteral("MMMM");
        break;
      case kDateFormatWeekday:
        skeletonDate.AssignLiteral("EEE");
        break;
      case kDateFormatNone:
        haveSkeleton = false;
        break;
      default:
        NS_ERROR("Unknown nsDateFormatSelector");
        return NS_ERROR_ILLEGAL_VALUE;
    }

    if (haveSkeleton) {
      // Get pattern for skeleton.
      UDateTimePatternGenerator* patternGenerator =
          udatpg_open(mLocale->get(), &status);
      if (U_SUCCESS(status)) {
        int32_t patternLength;
        patternDate.SetLength(DATETIME_FORMAT_INITIAL_LEN);
        patternLength = udatpg_getBestPattern(
            patternGenerator,
            reinterpret_cast<const UChar*>(skeletonDate.BeginReading()),
            skeletonDate.Length(),
            reinterpret_cast<UChar*>(patternDate.BeginWriting()),
            DATETIME_FORMAT_INITIAL_LEN, &status);
        patternDate.SetLength(patternLength);

        if (status == U_BUFFER_OVERFLOW_ERROR) {
          status = U_ZERO_ERROR;
          udatpg_getBestPattern(
              patternGenerator,
              reinterpret_cast<const UChar*>(skeletonDate.BeginReading()),
              skeletonDate.Length(),
              reinterpret_cast<UChar*>(patternDate.BeginWriting()),
              patternLength, &status);
        }
      }
      udatpg_close(patternGenerator);
    }

    // Get the time style for the formatter.
    nsAutoString patternTime;
    switch (aTimeFormatSelector) {
      case kTimeFormatSeconds:
        rv = OSPreferences::GetInstance()->GetDateTimePattern(
            mozIOSPreferences::dateTimeFormatStyleNone,
            mozIOSPreferences::dateTimeFormatStyleLong,
            nsDependentCString(mLocale->get()), patternTime);
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      case kTimeFormatNoSeconds:
        rv = OSPreferences::GetInstance()->GetDateTimePattern(
            mozIOSPreferences::dateTimeFormatStyleNone,
            mozIOSPreferences::dateTimeFormatStyleShort,
            nsDependentCString(mLocale->get()), patternTime);
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      case kTimeFormatNone:
        break;
      default:
        NS_ERROR("Unknown nsTimeFormatSelector");
        return NS_ERROR_ILLEGAL_VALUE;
    }

    nsAutoString pattern;
    if (patternTime.Length() == 0) {
      pattern.Assign(patternDate);
    } else if (patternDate.Length() == 0) {
      pattern.Assign(patternTime);
    } else {
      OSPreferences::GetDateTimeConnectorPattern(
          nsDependentCString(mLocale->get()), pattern);
      int32_t index = pattern.Find("{1}");
      if (index != kNotFound) pattern.Replace(index, 3, patternDate);
      index = pattern.Find("{0}");
      if (index != kNotFound) pattern.Replace(index, 3, patternTime);
    }

    // Generate date/time string.
    nsAutoString timeZoneID(u"GMT");
    if (aTimeParameters) {
      int32_t totalOffsetMinutes =
          (aTimeParameters->tp_gmt_offset + aTimeParameters->tp_dst_offset) /
          60;
      if (totalOffsetMinutes != 0) {
        char sign = totalOffsetMinutes < 0 ? '-' : '+';
        int32_t hours = abs(totalOffsetMinutes) / 60;
        int32_t minutes = abs(totalOffsetMinutes) % 60;
        timeZoneID.AppendPrintf("%c%02d:%02d", sign, hours, minutes);
      }
    }

    if (aTimeParameters) {
      dateTimeFormat =
          udat_open(UDAT_PATTERN, UDAT_PATTERN, mLocale->get(),
                    reinterpret_cast<const UChar*>(timeZoneID.BeginReading()),
                    timeZoneID.Length(),
                    reinterpret_cast<const UChar*>(pattern.BeginReading()),
                    pattern.Length(), &status);
    } else {
      dateTimeFormat =
          udat_open(UDAT_PATTERN, UDAT_PATTERN, mLocale->get(), nullptr, -1,
                    reinterpret_cast<const UChar*>(pattern.BeginReading()),
                    pattern.Length(), &status);
    }
  }

  if (U_SUCCESS(status) && dateTimeFormat) {
    aStringOut.SetLength(DATETIME_FORMAT_INITIAL_LEN);
    dateTimeLen =
        udat_format(dateTimeFormat, aUDateTime,
                    reinterpret_cast<UChar*>(aStringOut.BeginWriting()),
                    DATETIME_FORMAT_INITIAL_LEN, nullptr, &status);
    aStringOut.SetLength(dateTimeLen);

    if (status == U_BUFFER_OVERFLOW_ERROR) {
      status = U_ZERO_ERROR;
      udat_format(dateTimeFormat, aUDateTime,
                  reinterpret_cast<UChar*>(aStringOut.BeginWriting()),
                  dateTimeLen, nullptr, &status);
    }
  }

  if (U_FAILURE(status)) {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

/*static*/
void DateTimeFormat::DeleteCache() {
  if (mFormatCache) {
    for (auto i = mFormatCache->Iter(); !i.Done(); i.Next()) {
      udat_close(i.Data());
    }
    delete mFormatCache;
    mFormatCache = nullptr;
  }
}

/*static*/
void DateTimeFormat::Shutdown() {
  DeleteCache();
  if (mLocale) {
    delete mLocale;
  }
}

}  // namespace mozilla
