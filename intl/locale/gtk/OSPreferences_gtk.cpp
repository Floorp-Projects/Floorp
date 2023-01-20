/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <locale.h>
#include "mozilla/intl/Locale.h"
#include "OSPreferences.h"

#include "nsServiceManagerUtils.h"
#include "nsIGSettingsService.h"

using namespace mozilla;
using namespace mozilla::intl;

OSPreferences::OSPreferences() = default;

bool OSPreferences::ReadSystemLocales(nsTArray<nsCString>& aLocaleList) {
  MOZ_ASSERT(aLocaleList.IsEmpty());

  nsAutoCString defaultLang(Locale::GetDefaultLocale());

  if (CanonicalizeLanguageTag(defaultLang)) {
    aLocaleList.AppendElement(defaultLang);
    return true;
  }
  return false;
}

bool OSPreferences::ReadRegionalPrefsLocales(nsTArray<nsCString>& aLocaleList) {
  MOZ_ASSERT(aLocaleList.IsEmpty());

  // For now we're just taking the LC_TIME from POSIX environment for all
  // regional preferences.
  nsAutoCString localeStr(setlocale(LC_TIME, nullptr));

  if (CanonicalizeLanguageTag(localeStr)) {
    aLocaleList.AppendElement(localeStr);
    return true;
  }

  return false;
}

/*
 * This looks up into gtk settings for hourCycle format.
 *
 * This works for all GUIs that use gtk settings like Gnome, Elementary etc.
 *
 * We're taking the current 12/24h settings irrelevant of the locale, because
 * in the UI user selects this setting for all locales.
 */
static int HourCycle() {
  nsCOMPtr<nsIGSettingsService> gsettings =
      do_GetService(NS_GSETTINGSSERVICE_CONTRACTID);
  if (!gsettings) {
    return 0;
  }

  nsCOMPtr<nsIGSettingsCollection> desktop_settings;
  gsettings->GetCollectionForSchema("org.gnome.desktop.interface"_ns,
                                    getter_AddRefs(desktop_settings));
  if (!desktop_settings) {
    return 0;
  }

  nsAutoCString result;
  desktop_settings->GetString("clock-format"_ns, result);
  if (result == "12h") {
    return 12;
  }
  if (result == "24h") {
    return 24;
  }
  return 0;
}

/**
 * Since Gtk does not provide a way to customize or format date/time patterns,
 * we're reusing ICU data here, but we do modify it according to the only
 * setting Gtk gives us - hourCycle.
 *
 * This means that for gtk we will return a pattern from ICU altered to
 * represent h12/h24 hour cycle if the user modified the default value.
 *
 * In short, this should work like this:
 *
 *  * gtk defaults, pl: 24h
 *  * gtk defaults, en: 12h
 *
 *  * gtk 12h, pl: 12h
 *  * gtk 12h, en: 12h
 *
 *  * gtk 24h, pl: 24h
 *  * gtk 12h, en: 12h
 */
bool OSPreferences::ReadDateTimePattern(DateTimeFormatStyle aDateStyle,
                                        DateTimeFormatStyle aTimeStyle,
                                        const nsACString& aLocale,
                                        nsACString& aRetVal) {
  nsAutoCString skeleton;
  if (!GetDateTimeSkeletonForStyle(aDateStyle, aTimeStyle, aLocale, skeleton)) {
    return false;
  }

  // Customize the skeleton if necessary to reflect user's 12/24hr pref
  int hourCycle = HourCycle();
  if (hourCycle == 12 || hourCycle == 24) {
    OverrideSkeletonHourCycle(hourCycle == 24, skeleton);
  }

  if (!GetPatternForSkeleton(skeleton, aLocale, aRetVal)) {
    return false;
  }

  return true;
}

void OSPreferences::RemoveObservers() {}
