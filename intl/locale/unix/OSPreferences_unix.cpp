/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPosixLocale.h"
#include "OSPreferences.h"

using namespace mozilla::intl;

bool
OSPreferences::ReadSystemLocales(nsTArray<nsCString>& aLocaleList)
{
#ifdef ENABLE_INTL_API
  MOZ_ASSERT(aLocaleList.IsEmpty());

  nsAutoCString defaultLang(uloc_getDefault());

  if (CanonicalizeLanguageTag(defaultLang)) {
    aLocaleList.AppendElement(defaultLang);
    return true;
  }
  return false;
#else
  nsAutoString locale;
  nsPosixLocale::GetXPLocale(getenv("LANG"), locale);
  if (locale.IsEmpty()) {
    locale.AssignLiteral("en-US");
  }
  aLocaleList.AppendElement(NS_LossyConvertUTF16toASCII(locale));
  return true;
#endif
}

bool
OSPreferences::ReadDateTimePattern(DateTimeFormatStyle aDateStyle,
                                   DateTimeFormatStyle aTimeStyle,
                                   const nsACString& aLocale, nsAString& aRetVal)
{
  return false;
}
