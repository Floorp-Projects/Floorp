/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is a shared part of the OSPreferences API implementation.
 * It defines helper methods and public methods that are calling
 * platform-specific private methods.
 */

#include "OSPreferences.h"
#include "mozilla/ClearOnShutdown.h"

using namespace mozilla::intl;

mozilla::StaticAutoPtr<OSPreferences> OSPreferences::sInstance;

OSPreferences*
OSPreferences::GetInstance()
{
  if (!sInstance) {
    sInstance = new OSPreferences();
    ClearOnShutdown(&sInstance);
  }
  return sInstance;
}

bool
OSPreferences::GetSystemLocales(nsTArray<nsCString>& aRetVal)
{
  bool status = true;
  if (mSystemLocales.IsEmpty()) {
    status = ReadSystemLocales(mSystemLocales);
  }
  aRetVal = mSystemLocales;
  return status;
}

/**
 * This method should be called by every method of OSPreferences that
 * retrieves a locale id from external source.
 *
 * It attempts to retrieve as much of the locale ID as possible, cutting
 * out bits that are not understood (non-strict behavior of ICU).
 *
 * It returns true if the canonicalization was successful.
 */
bool
OSPreferences::CanonicalizeLanguageTag(nsCString& aLoc)
{
  char langTag[512];

  UErrorCode status = U_ZERO_ERROR;

  int32_t langTagLen =
    uloc_toLanguageTag(aLoc.get(), langTag, sizeof(langTag) - 1, false, &status);

  if (U_FAILURE(status)) {
    return false;
  }

  aLoc.Assign(langTag, langTagLen);
  return true;
}
