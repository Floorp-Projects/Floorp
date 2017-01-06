/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DateTimeFormat.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsILocaleService.h"
#include "unicode/udat.h"

namespace mozilla {

nsCString* DateTimeFormat::mLocale = nullptr;

/*static*/ nsresult
DateTimeFormat::Initialize()
{
  nsAutoString localeStr;
  nsresult rv = NS_OK;

  if (!mLocale) {
    mLocale = new nsCString();
  } else if (!mLocale->IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsILocaleService> localeService =
           do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsILocale> appLocale;
    rv = localeService->GetApplicationLocale(getter_AddRefs(appLocale));
    if (NS_SUCCEEDED(rv)) {
      rv = appLocale->GetCategory(NS_LITERAL_STRING("NSILOCALE_TIME"), localeStr);
      if (NS_SUCCEEDED(rv) && !localeStr.IsEmpty()) {
        *mLocale = NS_LossyConvertUTF16toASCII(localeStr); // cache locale name
      }
    }
  }

  return rv;
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
#define DATETIME_FORMAT_INITIAL_LEN 127
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

  UDate timeUDate = aPrTime / PR_USEC_PER_MSEC;

  // Get the date style for the formatter:
  UDateFormatStyle dateStyle;
  switch (aDateFormatSelector) {
    case kDateFormatLong:
      dateStyle = UDAT_LONG;
      break;
    case kDateFormatShort:
      dateStyle = UDAT_SHORT;
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

  UErrorCode status = U_ZERO_ERROR;

  UDateFormat* dateTimeFormat = udat_open(timeStyle, dateStyle, mLocale->get(), nullptr, -1, nullptr, -1, &status);

  if (U_SUCCESS(status) && dateTimeFormat) {
    aStringOut.SetLength(DATETIME_FORMAT_INITIAL_LEN);
    dateTimeLen = udat_format(dateTimeFormat, timeUDate, reinterpret_cast<UChar*>(aStringOut.BeginWriting()), DATETIME_FORMAT_INITIAL_LEN, nullptr, &status);
    aStringOut.SetLength(dateTimeLen);

    if (status == U_BUFFER_OVERFLOW_ERROR) {
      status = U_ZERO_ERROR;
      udat_format(dateTimeFormat, timeUDate, reinterpret_cast<UChar*>(aStringOut.BeginWriting()), dateTimeLen, nullptr, &status);
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

// performs a locale sensitive date formatting operation on the PRExplodedTime parameter
/*static*/ nsresult
DateTimeFormat::FormatPRExplodedTime(const nsDateFormatSelector aDateFormatSelector,
                                     const nsTimeFormatSelector aTimeFormatSelector,
                                     const PRExplodedTime* aExplodedTime,
                                     nsAString& aStringOut)
{
  return FormatPRTime(aDateFormatSelector, aTimeFormatSelector, PR_ImplodeTime(aExplodedTime), aStringOut);
}

/*static*/ void
DateTimeFormat::Shutdown()
{
  if (mLocale) {
    delete mLocale;
  }
}

}
