/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "mozilla/intl/AppDateTimeFormat.h"
#include "mozilla/intl/DateTimePatternGenerator.h"
#include "mozilla/intl/FormatBuffer.h"
#include "mozilla/intl/LocaleService.h"
#include "OSPreferences.h"
#include "mozIOSPreferences.h"
#ifdef DEBUG
#  include "nsThreadManager.h"
#endif

namespace mozilla::intl {

nsCString* AppDateTimeFormat::sLocale = nullptr;
nsTHashMap<nsCStringHashKey, UniquePtr<DateTimeFormat>>*
    AppDateTimeFormat::sFormatCache;

static const int32_t DATETIME_FORMAT_INITIAL_LEN = 127;

/*static*/
nsresult AppDateTimeFormat::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());
  if (sLocale) {
    return NS_OK;
  }

  sLocale = new nsCString();
  AutoTArray<nsCString, 10> regionalPrefsLocales;
  LocaleService::GetInstance()->GetRegionalPrefsLocales(regionalPrefsLocales);
  sLocale->Assign(regionalPrefsLocales[0]);

  return NS_OK;
}

// performs a locale sensitive date formatting operation on the PRTime parameter
/*static*/
nsresult AppDateTimeFormat::Format(const DateTimeFormat::StyleBag& aStyle,
                                   const PRTime aPrTime,
                                   nsAString& aStringOut) {
  return AppDateTimeFormat::Format(
      aStyle, (static_cast<double>(aPrTime) / PR_USEC_PER_MSEC), nullptr,
      aStringOut);
}

// performs a locale sensitive date formatting operation on the PRExplodedTime
// parameter
/*static*/
nsresult AppDateTimeFormat::Format(const DateTimeFormat::StyleBag& aStyle,
                                   const PRExplodedTime* aExplodedTime,
                                   nsAString& aStringOut) {
  return AppDateTimeFormat::Format(
      aStyle, (PR_ImplodeTime(aExplodedTime) / PR_USEC_PER_MSEC),
      &(aExplodedTime->tm_params), aStringOut);
}

// performs a locale sensitive date formatting operation on the PRExplodedTime
// parameter, using the specified options.
/*static*/
nsresult AppDateTimeFormat::Format(const DateTimeFormat::ComponentsBag& aBag,
                                   const PRExplodedTime* aExplodedTime,
                                   nsAString& aStringOut) {
  // set up locale data
  nsresult rv = Initialize();
  if (NS_FAILED(rv)) {
    return rv;
  }

  aStringOut.Truncate();

  nsAutoCString str;
  nsAutoString timeZoneID;
  BuildTimeZoneString(aExplodedTime->tm_params, timeZoneID);

  auto genResult = DateTimePatternGenerator::TryCreate(sLocale->get());
  NS_ENSURE_TRUE(genResult.isOk(), NS_ERROR_FAILURE);
  auto dateTimePatternGenerator = genResult.unwrap();

  auto result = DateTimeFormat::TryCreateFromComponents(
      *sLocale, aBag, dateTimePatternGenerator.get(), Some(timeZoneID));
  NS_ENSURE_TRUE(result.isOk(), NS_ERROR_FAILURE);
  auto dateTimeFormat = result.unwrap();

  double unixEpoch =
      static_cast<float>((PR_ImplodeTime(aExplodedTime) / PR_USEC_PER_MSEC));

  aStringOut.SetLength(DATETIME_FORMAT_INITIAL_LEN);
  nsTStringToBufferAdapter buffer(aStringOut);
  NS_ENSURE_TRUE(dateTimeFormat->TryFormat(unixEpoch, buffer).isOk(),
                 NS_ERROR_FAILURE);

  return rv;
}

/**
 * An internal utility function to serialize a Maybe<DateTimeFormat::Style> to
 * an int, to be used as a caching key.
 */
static int StyleToInt(const Maybe<DateTimeFormat::Style>& aStyle) {
  if (aStyle.isSome()) {
    switch (*aStyle) {
      case DateTimeFormat::Style::Full:
        return 1;
      case DateTimeFormat::Style::Long:
        return 2;
      case DateTimeFormat::Style::Medium:
        return 3;
      case DateTimeFormat::Style::Short:
        return 4;
    }
  }
  return 0;
}

/*static*/
nsresult AppDateTimeFormat::Format(const DateTimeFormat::StyleBag& aStyle,
                                   const double aUnixEpoch,
                                   const PRTimeParameters* aTimeParameters,
                                   nsAString& aStringOut) {
  nsresult rv = NS_OK;

  // return, nothing to format
  if (aStyle.date.isNothing() && aStyle.time.isNothing()) {
    aStringOut.Truncate();
    return NS_OK;
  }

  // set up locale data
  rv = Initialize();

  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString key;
  key.AppendInt(StyleToInt(aStyle.date));
  key.Append(':');
  key.AppendInt(StyleToInt(aStyle.time));
  if (aTimeParameters) {
    key.Append(':');
    key.AppendInt(aTimeParameters->tp_gmt_offset);
    key.Append(':');
    key.AppendInt(aTimeParameters->tp_dst_offset);
  }

  if (sFormatCache && sFormatCache->Count() == kMaxCachedFormats) {
    // Don't allow a pathological page to extend the cache unreasonably.
    NS_WARNING("flushing DateTimeFormat cache");
    DeleteCache();
  }
  if (!sFormatCache) {
    sFormatCache = new nsTHashMap<nsCStringHashKey, UniquePtr<DateTimeFormat>>(
        kMaxCachedFormats);
  }

  UniquePtr<DateTimeFormat>& dateTimeFormat = sFormatCache->LookupOrInsert(key);

  if (!dateTimeFormat) {
    // We didn't have a cached formatter for this key, so create one.
    int32_t dateFormatStyle = mozIOSPreferences::dateTimeFormatStyleNone;
    if (aStyle.date.isSome()) {
      switch (*aStyle.date) {
        case DateTimeFormat::Style::Full:
        case DateTimeFormat::Style::Long:
          dateFormatStyle = mozIOSPreferences::dateTimeFormatStyleLong;
          break;
        case DateTimeFormat::Style::Medium:
        case DateTimeFormat::Style::Short:
          dateFormatStyle = mozIOSPreferences::dateTimeFormatStyleShort;
          break;
      }
    }

    int32_t timeFormatStyle = mozIOSPreferences::dateTimeFormatStyleNone;
    if (aStyle.time.isSome()) {
      switch (*aStyle.time) {
        case DateTimeFormat::Style::Full:
        case DateTimeFormat::Style::Long:
          timeFormatStyle = mozIOSPreferences::dateTimeFormatStyleLong;
          break;
        case DateTimeFormat::Style::Medium:
        case DateTimeFormat::Style::Short:
          timeFormatStyle = mozIOSPreferences::dateTimeFormatStyleShort;
          break;
      }
    }

    nsAutoCString str;
    rv = OSPreferences::GetInstance()->GetDateTimePattern(
        dateFormatStyle, timeFormatStyle, nsDependentCString(sLocale->get()),
        str);
    NS_ENSURE_SUCCESS(rv, rv);
    nsAutoString pattern = NS_ConvertUTF8toUTF16(str);

    Maybe<Span<const char16_t>> timeZoneOverride = Nothing();
    nsAutoString timeZoneID;
    if (aTimeParameters) {
      BuildTimeZoneString(*aTimeParameters, timeZoneID);
      timeZoneOverride =
          Some(Span<const char16_t>(timeZoneID.Data(), timeZoneID.Length()));
    }

    auto result = DateTimeFormat::TryCreateFromPattern(*sLocale, pattern,
                                                       timeZoneOverride);
    NS_ENSURE_TRUE(result.isOk(), NS_ERROR_FAILURE);
    dateTimeFormat = result.unwrap();
  }

  MOZ_ASSERT(dateTimeFormat);

  aStringOut.SetLength(DATETIME_FORMAT_INITIAL_LEN);
  nsTStringToBufferAdapter buffer(aStringOut);
  NS_ENSURE_TRUE(dateTimeFormat->TryFormat(aUnixEpoch, buffer).isOk(),
                 NS_ERROR_FAILURE);

  return rv;
}

/*static*/
void AppDateTimeFormat::BuildTimeZoneString(
    const PRTimeParameters& aTimeParameters, nsAString& aStringOut) {
  aStringOut.Truncate();
  aStringOut.Append(u"GMT");
  int32_t totalOffsetMinutes =
      (aTimeParameters.tp_gmt_offset + aTimeParameters.tp_dst_offset) / 60;
  if (totalOffsetMinutes != 0) {
    char sign = totalOffsetMinutes < 0 ? '-' : '+';
    int32_t hours = abs(totalOffsetMinutes) / 60;
    int32_t minutes = abs(totalOffsetMinutes) % 60;
    aStringOut.AppendPrintf("%c%02d:%02d", sign, hours, minutes);
  }
}

/*static*/
void AppDateTimeFormat::DeleteCache() {
  MOZ_ASSERT(NS_IsMainThread());
  if (sFormatCache) {
    delete sFormatCache;
    sFormatCache = nullptr;
  }
}

/*static*/
void AppDateTimeFormat::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  DeleteCache();
  delete sLocale;
}

/*static*/
void AppDateTimeFormat::ClearLocaleCache() {
  MOZ_ASSERT(NS_IsMainThread());
  DeleteCache();
  delete sLocale;
  sLocale = nullptr;
}

}  // namespace mozilla::intl
