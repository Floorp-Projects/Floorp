/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OSPreferences.h"
#include "mozilla/Preferences.h"

#include "FennecJNIWrappers.h"
#include "GeneratedJNIWrappers.h"

using namespace mozilla::intl;

OSPreferences::OSPreferences()
{
}

OSPreferences::~OSPreferences()
{
}

bool
OSPreferences::ReadSystemLocales(nsTArray<nsCString>& aLocaleList)
{
  if (!mozilla::jni::IsAvailable()) {
    return false;
  }

  //XXX: Notice, this value may be empty on an early read. In that case
  //     we won't add anything to the return list so that it doesn't get
  //     cached in mSystemLocales.
  auto locales = mozilla::jni::IsFennec() ?
                   java::BrowserLocaleManager::GetLocales() :
                   java::GeckoAppShell::GetDefaultLocales();
  if (locales) {
    for (size_t i = 0; i < locales->Length(); i++) {
      jni::String::LocalRef locale = locales->GetElement(i);
      aLocaleList.AppendElement(locale->ToCString());
    }
    return true;
  }
  return false;
}

bool
OSPreferences::ReadRegionalPrefsLocales(nsTArray<nsCString>& aLocaleList)
{
  // For now we're just taking System Locales since we don't know of any better
  // API for regional prefs.
  return ReadSystemLocales(aLocaleList);
}

bool
OSPreferences::ReadDateTimePattern(DateTimeFormatStyle aDateStyle,
                                   DateTimeFormatStyle aTimeStyle,
                                   const nsACString& aLocale, nsAString& aRetVal)
{
  return false;
}
