/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OSPreferences.h"
#include "dlfcn.h"
#include "glib.h"
#include "gio/gio.h"

using namespace mozilla::intl;

bool
OSPreferences::ReadSystemLocales(nsTArray<nsCString>& aLocaleList)
{
  MOZ_ASSERT(aLocaleList.IsEmpty());

  nsAutoCString defaultLang(uloc_getDefault());

  if (CanonicalizeLanguageTag(defaultLang)) {
    aLocaleList.AppendElement(defaultLang);
    return true;
  }
  return false;
}

/*
 * This looks up into gtk settings for hourCycle format.
 *
 * This works for all GUIs that use gtk settings like Gnome, Elementary etc.
 * Ubuntu does not use those settings so we'll want to support them separately.
 *
 * We're taking the current 12/24h settings irrelevant of the locale, because
 * in the UI user selects this setting for all locales.
 */
typedef GVariant* (*get_value_fn_t)(GSettings*, const gchar*);

static get_value_fn_t
FindGetValueFunction()
{
  get_value_fn_t fn = reinterpret_cast<get_value_fn_t>(
    dlsym(RTLD_DEFAULT, "g_settings_get_user_value")
  );
  return fn ? fn : &g_settings_get_value;
}

static int
HourCycle()
{
  int rval = 0;

  const char* schema;
  const char* key;
  const char* env = getenv("XDG_CURRENT_DESKTOP");
  if (env && strcmp(env, "Unity") == 0) {
    schema = "com.canonical.indicator.datetime";
    key = "time-format";
  } else {
    schema = "org.gnome.desktop.interface";
    key = "clock-format";
  }

  // This is a workaround for old GTK versions.
  // Once we bump the minimum version to 2.40 we should replace
  // this with g_settings_schme_source_lookup.
  // See bug 1356718 for details.
  const char* const* schemas = g_settings_list_schemas();
  GSettings* settings = nullptr;

  for (uint32_t i = 0; schemas[i] != nullptr; i++) {
    if (strcmp(schemas[i], schema) == 0) {
      settings = g_settings_new(schema);
      break;
    }
  }

  if (settings) {
    // We really want to use g_settings_get_user_value which will
    // only want to take it if user manually changed the value.
    // But this requires glib 2.40, and we still support older glib versions,
    // so we have to check whether it's available and fall back to the older
    // g_settings_get_value if not.
    static get_value_fn_t sGetValueFunction = FindGetValueFunction();
    GVariant* value = sGetValueFunction(settings, key);
    if (value) {
      if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
        const char* strVal = g_variant_get_string(value, nullptr);
        if (strncmp("12", strVal, 2) == 0) {
          rval = 12;
        } else if (strncmp("24", strVal, 2) == 0) {
          rval = 24;
        }
      }
      g_variant_unref(value);
    }
    g_object_unref(settings);
  }
  return rval;
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
bool
OSPreferences::ReadDateTimePattern(DateTimeFormatStyle aDateStyle,
                                   DateTimeFormatStyle aTimeStyle,
                                   const nsACString& aLocale, nsAString& aRetVal)
{
  nsAutoString skeleton;
  if (!GetDateTimeSkeletonForStyle(aDateStyle, aTimeStyle, aLocale, skeleton)) {
    return false;
  }

  // Customize the skeleton if necessary to reflect user's 12/24hr pref
  switch (HourCycle()) {
    case 12: {
      // If skeleton contains 'H' or 'k', replace with 'h' or 'K' respectively,
      // and add 'a' unless already present.
      if (skeleton.FindChar('H') == -1 && skeleton.FindChar('k') == -1) {
        break; // nothing to do
      }
      bool foundA = false;
      for (size_t i = 0; i < skeleton.Length(); ++i) {
        switch (skeleton[i]) {
          case 'a':
            foundA = true;
            break;
          case 'H':
            skeleton.SetCharAt('h', i);
            break;
          case 'k':
            skeleton.SetCharAt('K', i);
            break;
        }
      }
      if (!foundA) {
        skeleton.Append(char16_t('a'));
      }
      break;
    }
    case 24:
      // If skeleton contains 'h' or 'K', replace with 'H' or 'k' respectively,
      // and delete 'a' if present.
      if (skeleton.FindChar('h') == -1 && skeleton.FindChar('K') == -1) {
        break; // nothing to do
      }
      for (int32_t i = 0; i < int32_t(skeleton.Length()); ++i) {
        switch (skeleton[i]) {
          case 'a':
            skeleton.Cut(i, 1);
            --i;
            break;
          case 'h':
            skeleton.SetCharAt('H', i);
            break;
          case 'K':
            skeleton.SetCharAt('k', i);
            break;
        }
      }
      break;
  }

  if (!GetPatternForSkeleton(skeleton, aLocale, aRetVal)) {
    return false;
  }

  return true;
}

