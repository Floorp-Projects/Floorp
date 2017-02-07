/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OSPreferences.h"
#include "nsWin32Locale.h"

using namespace mozilla::intl;

bool
OSPreferences::ReadSystemLocales(nsTArray<nsCString>& aLocaleList)
{
  MOZ_ASSERT(aLocaleList.IsEmpty());

  nsAutoString locale;

  LCID win_lcid = GetSystemDefaultLCID();
  nsWin32Locale::GetXPLocale(win_lcid, locale);

  NS_LossyConvertUTF16toASCII loc(locale);

  if (CanonicalizeLanguageTag(loc)) {
    aLocaleList.AppendElement(loc);
    return true;
  }
  return false;
}
