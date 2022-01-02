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
#include "mozilla/intl/DateTimePatternGenerator.h"
#include "mozilla/intl/DateTimeFormat.h"
#include "mozilla/Result.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"

using namespace mozilla::intl;

NS_IMPL_ISUPPORTS(OSPreferences, mozIOSPreferences)

mozilla::StaticRefPtr<OSPreferences> OSPreferences::sInstance;

// Return a new strong reference to the instance, creating it if necessary.
already_AddRefed<OSPreferences> OSPreferences::GetInstanceAddRefed() {
  RefPtr<OSPreferences> result = sInstance;
  if (!result) {
    MOZ_ASSERT(NS_IsMainThread(),
               "OSPreferences should be initialized on main thread!");
    if (!NS_IsMainThread()) {
      return nullptr;
    }
    sInstance = new OSPreferences();
    result = sInstance;

    DebugOnly<nsresult> rv = Preferences::RegisterPrefixCallback(
        PreferenceChanged, "intl.date_time.pattern_override");
    MOZ_ASSERT(NS_SUCCEEDED(rv), "Adding observers failed.");

    ClearOnShutdown(&sInstance);
  }
  return result.forget();
}

// Return a raw pointer to the instance: not for off-main-thread use,
// because ClearOnShutdown means it could go away unexpectedly.
OSPreferences* OSPreferences::GetInstance() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    // This will create the static instance; then we just drop the extra
    // reference.
    RefPtr<OSPreferences> result = GetInstanceAddRefed();
  }
  return sInstance;
}

void OSPreferences::Refresh() {
  nsTArray<nsCString> newLocales;
  ReadSystemLocales(newLocales);

  if (mSystemLocales != newLocales) {
    mSystemLocales = std::move(newLocales);
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->NotifyObservers(nullptr, "intl:system-locales-changed", nullptr);
    }
  }
}

OSPreferences::~OSPreferences() {
  Preferences::UnregisterPrefixCallback(PreferenceChanged,
                                        "intl.date_time.pattern_override");
  RemoveObservers();
}

/*static*/
void OSPreferences::PreferenceChanged(const char* aPrefName,
                                      void* /* aClosure */) {
  if (sInstance) {
    sInstance->mPatternCache.Clear();
  }
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
bool OSPreferences::CanonicalizeLanguageTag(nsCString& aLoc) {
  return LocaleService::CanonicalizeLanguageId(aLoc);
}

/**
 * This method retrieves from mozilla::intl the best pattern for a given
 * date/time style.
 */
bool OSPreferences::GetDateTimePatternForStyle(DateTimeFormatStyle aDateStyle,
                                               DateTimeFormatStyle aTimeStyle,
                                               const nsACString& aLocale,
                                               nsACString& aRetVal) {
  DateTimeFormat::StyleBag style;

  switch (aTimeStyle) {
    case DateTimeFormatStyle::Short:
      style.time = Some(DateTimeFormat::Style::Short);
      break;
    case DateTimeFormatStyle::Medium:
      style.time = Some(DateTimeFormat::Style::Medium);
      break;
    case DateTimeFormatStyle::Long:
      style.time = Some(DateTimeFormat::Style::Long);
      break;
    case DateTimeFormatStyle::Full:
      style.time = Some(DateTimeFormat::Style::Full);
      break;
    case DateTimeFormatStyle::None:
    case DateTimeFormatStyle::Invalid:
      // Do nothing.
      break;
  }

  switch (aDateStyle) {
    case DateTimeFormatStyle::Short:
      style.date = Some(DateTimeFormat::Style::Short);
      break;
    case DateTimeFormatStyle::Medium:
      style.date = Some(DateTimeFormat::Style::Medium);
      break;
    case DateTimeFormatStyle::Long:
      style.date = Some(DateTimeFormat::Style::Long);
      break;
    case DateTimeFormatStyle::Full:
      style.date = Some(DateTimeFormat::Style::Full);
      break;
    case DateTimeFormatStyle::None:
    case DateTimeFormatStyle::Invalid:
      // Do nothing.
      break;
  }

  nsAutoCString locale;
  if (aLocale.IsEmpty()) {
    AutoTArray<nsCString, 10> regionalPrefsLocales;
    LocaleService::GetInstance()->GetRegionalPrefsLocales(regionalPrefsLocales);
    locale.Assign(regionalPrefsLocales[0]);
  } else {
    locale.Assign(aLocale);
  }

  auto genResult =
      DateTimePatternGenerator::TryCreate(PromiseFlatCString(aLocale).get());
  if (genResult.isErr()) {
    return false;
  }
  auto generator = genResult.unwrap();

  auto dfResult = DateTimeFormat::TryCreateFromStyle(
      MakeStringSpan(locale.get()), style, generator.get(), Nothing());
  if (dfResult.isErr()) {
    return false;
  }
  auto df = dfResult.unwrap();

  DateTimeFormat::PatternVector pattern;
  auto patternResult = df->GetPattern(pattern);
  if (patternResult.isErr()) {
    return false;
  }

  aRetVal = NS_ConvertUTF16toUTF8(pattern.begin(), pattern.length());
  return true;
}

/**
 * This method retrieves from mozilla::intl the best skeleton for a given
 * date/time style.
 *
 * This is useful for cases where an OS does not provide its own patterns,
 * but provide ability to customize the skeleton, like alter hourCycle setting.
 *
 * The returned value is a skeleton that matches the styles.
 */
bool OSPreferences::GetDateTimeSkeletonForStyle(DateTimeFormatStyle aDateStyle,
                                                DateTimeFormatStyle aTimeStyle,
                                                const nsACString& aLocale,
                                                nsACString& aRetVal) {
  nsAutoCString pattern;
  if (!GetDateTimePatternForStyle(aDateStyle, aTimeStyle, aLocale, pattern)) {
    return false;
  }

  auto genResult =
      DateTimePatternGenerator::TryCreate(PromiseFlatCString(aLocale).get());
  if (genResult.isErr()) {
    return false;
  }

  nsAutoString patternAsUtf16 = NS_ConvertUTF8toUTF16(pattern);
  DateTimeFormat::SkeletonVector skeleton;
  auto generator = genResult.unwrap();
  auto skeletonResult = generator->GetSkeleton(patternAsUtf16, skeleton);
  if (skeletonResult.isErr()) {
    return false;
  }

  aRetVal = NS_ConvertUTF16toUTF8(skeleton.begin(), skeleton.length());
  return true;
}

/**
 * This method checks for preferences that override the defaults
 */
bool OSPreferences::OverrideDateTimePattern(DateTimeFormatStyle aDateStyle,
                                            DateTimeFormatStyle aTimeStyle,
                                            const nsACString& aLocale,
                                            nsACString& aRetVal) {
  const auto PrefToMaybeString = [](const char* pref) -> Maybe<nsAutoCString> {
    nsAutoCString value;
    nsresult nr = Preferences::GetCString(pref, value);
    if (NS_FAILED(nr) || value.IsEmpty()) {
      return Nothing();
    }
    return Some(std::move(value));
  };

  Maybe<nsAutoCString> timeSkeleton;
  switch (aTimeStyle) {
    case DateTimeFormatStyle::Short:
      timeSkeleton =
          PrefToMaybeString("intl.date_time.pattern_override.time_short");
      break;
    case DateTimeFormatStyle::Medium:
      timeSkeleton =
          PrefToMaybeString("intl.date_time.pattern_override.time_medium");
      break;
    case DateTimeFormatStyle::Long:
      timeSkeleton =
          PrefToMaybeString("intl.date_time.pattern_override.time_long");
      break;
    case DateTimeFormatStyle::Full:
      timeSkeleton =
          PrefToMaybeString("intl.date_time.pattern_override.time_full");
      break;
    default:
      break;
  }

  Maybe<nsAutoCString> dateSkeleton;
  switch (aDateStyle) {
    case DateTimeFormatStyle::Short:
      dateSkeleton =
          PrefToMaybeString("intl.date_time.pattern_override.date_short");
      break;
    case DateTimeFormatStyle::Medium:
      dateSkeleton =
          PrefToMaybeString("intl.date_time.pattern_override.date_medium");
      break;
    case DateTimeFormatStyle::Long:
      dateSkeleton =
          PrefToMaybeString("intl.date_time.pattern_override.date_long");
      break;
    case DateTimeFormatStyle::Full:
      dateSkeleton =
          PrefToMaybeString("intl.date_time.pattern_override.date_full");
      break;
    default:
      break;
  }

  nsAutoCString locale;
  if (aLocale.IsEmpty()) {
    AutoTArray<nsCString, 10> regionalPrefsLocales;
    LocaleService::GetInstance()->GetRegionalPrefsLocales(regionalPrefsLocales);
    locale.Assign(regionalPrefsLocales[0]);
  } else {
    locale.Assign(aLocale);
  }

  const auto FillConnectorPattern = [&locale](
                                        const nsAutoCString& datePattern,
                                        const nsAutoCString& timePattern) {
    nsAutoCString pattern;
    GetDateTimeConnectorPattern(nsDependentCString(locale.get()), pattern);
    int32_t index = pattern.Find("{1}");
    if (index != kNotFound) {
      pattern.Replace(index, 3, datePattern);
    }
    index = pattern.Find("{0}");
    if (index != kNotFound) {
      pattern.Replace(index, 3, timePattern);
    }
    return pattern;
  };

  if (timeSkeleton && dateSkeleton) {
    aRetVal.Assign(FillConnectorPattern(*dateSkeleton, *timeSkeleton));
  } else if (timeSkeleton) {
    if (aDateStyle != DateTimeFormatStyle::None) {
      nsAutoCString pattern;
      if (!ReadDateTimePattern(aDateStyle, DateTimeFormatStyle::None, aLocale,
                               pattern) &&
          !GetDateTimePatternForStyle(aDateStyle, DateTimeFormatStyle::None,
                                      aLocale, pattern)) {
        return false;
      }
      aRetVal.Assign(FillConnectorPattern(pattern, *timeSkeleton));
    } else {
      aRetVal.Assign(*timeSkeleton);
    }
  } else if (dateSkeleton) {
    if (aTimeStyle != DateTimeFormatStyle::None) {
      nsAutoCString pattern;
      if (!ReadDateTimePattern(DateTimeFormatStyle::None, aTimeStyle, aLocale,
                               pattern) &&
          !GetDateTimePatternForStyle(DateTimeFormatStyle::None, aTimeStyle,
                                      aLocale, pattern)) {
        return false;
      }
      aRetVal.Assign(FillConnectorPattern(*dateSkeleton, pattern));
    } else {
      aRetVal.Assign(*dateSkeleton);
    }
  } else {
    return false;
  }

  return true;
}

/**
 * This function is a counterpart to GetDateTimeSkeletonForStyle.
 *
 * It takes a skeleton and returns the best available pattern for a given locale
 * that represents the provided skeleton.
 *
 * For example:
 * "Hm" skeleton for "en-US" will return "H:m"
 */
bool OSPreferences::GetPatternForSkeleton(const nsACString& aSkeleton,
                                          const nsACString& aLocale,
                                          nsACString& aRetVal) {
  aRetVal.Truncate();

  auto genResult =
      DateTimePatternGenerator::TryCreate(PromiseFlatCString(aLocale).get());
  if (genResult.isErr()) {
    return false;
  }

  nsAutoString skeletonAsUtf16 = NS_ConvertUTF8toUTF16(aSkeleton);
  DateTimeFormat::PatternVector pattern;
  auto generator = genResult.unwrap();
  auto patternResult = generator->GetBestPattern(skeletonAsUtf16, pattern);
  if (patternResult.isErr()) {
    return false;
  }

  aRetVal = NS_ConvertUTF16toUTF8(pattern.begin(), pattern.length());
  return true;
}

/**
 * This function returns a pattern that should be used to join date and time
 * patterns into a single date/time pattern string.
 *
 * It's useful for OSes that do not provide an API to retrieve such combined
 * pattern.
 *
 * An example output is "{1}, {0}".
 */
bool OSPreferences::GetDateTimeConnectorPattern(const nsACString& aLocale,
                                                nsACString& aRetVal) {
  // Check for a valid override pref and use that if present.
  nsAutoCString value;
  nsresult nr = Preferences::GetCString(
      "intl.date_time.pattern_override.connector_short", value);
  if (NS_SUCCEEDED(nr) && value.Find("{0}") != kNotFound &&
      value.Find("{1}") != kNotFound) {
    aRetVal = std::move(value);
    return true;
  }

  auto genResult =
      DateTimePatternGenerator::TryCreate(PromiseFlatCString(aLocale).get());
  if (genResult.isErr()) {
    return false;
  }

  auto generator = genResult.unwrap();
  Span<const char16_t> result = generator->GetPlaceholderPattern();
  aRetVal = NS_ConvertUTF16toUTF8(result.data(), result.size());
  return true;
}

/**
 * mozIOSPreferences methods
 */
NS_IMETHODIMP
OSPreferences::GetSystemLocales(nsTArray<nsCString>& aRetVal) {
  if (!mSystemLocales.IsEmpty()) {
    aRetVal = mSystemLocales.Clone();
    return NS_OK;
  }

  if (ReadSystemLocales(aRetVal)) {
    mSystemLocales = aRetVal.Clone();
    return NS_OK;
  }

  // If we failed to get the system locale, we still need
  // to return something because there are tests out there that
  // depend on system locale to be set.
  aRetVal.AppendElement("en-US"_ns);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
OSPreferences::GetSystemLocale(nsACString& aRetVal) {
  if (!mSystemLocales.IsEmpty()) {
    aRetVal = mSystemLocales[0];
  } else {
    AutoTArray<nsCString, 10> locales;
    GetSystemLocales(locales);
    if (!locales.IsEmpty()) {
      aRetVal = locales[0];
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
OSPreferences::GetRegionalPrefsLocales(nsTArray<nsCString>& aRetVal) {
  if (!mRegionalPrefsLocales.IsEmpty()) {
    aRetVal = mRegionalPrefsLocales.Clone();
    return NS_OK;
  }

  if (ReadRegionalPrefsLocales(aRetVal)) {
    mRegionalPrefsLocales = aRetVal.Clone();
    return NS_OK;
  }

  // If we failed to read regional prefs locales,
  // use system locales as last fallback.
  return GetSystemLocales(aRetVal);
}

static OSPreferences::DateTimeFormatStyle ToDateTimeFormatStyle(
    int32_t aTimeFormat) {
  switch (aTimeFormat) {
    // See mozIOSPreferences.idl for the integer values here.
    case 0:
      return OSPreferences::DateTimeFormatStyle::None;
    case 1:
      return OSPreferences::DateTimeFormatStyle::Short;
    case 2:
      return OSPreferences::DateTimeFormatStyle::Medium;
    case 3:
      return OSPreferences::DateTimeFormatStyle::Long;
    case 4:
      return OSPreferences::DateTimeFormatStyle::Full;
  }
  return OSPreferences::DateTimeFormatStyle::Invalid;
}

NS_IMETHODIMP
OSPreferences::GetDateTimePattern(int32_t aDateFormatStyle,
                                  int32_t aTimeFormatStyle,
                                  const nsACString& aLocale,
                                  nsACString& aRetVal) {
  DateTimeFormatStyle dateStyle = ToDateTimeFormatStyle(aDateFormatStyle);
  if (dateStyle == DateTimeFormatStyle::Invalid) {
    return NS_ERROR_INVALID_ARG;
  }
  DateTimeFormatStyle timeStyle = ToDateTimeFormatStyle(aTimeFormatStyle);
  if (timeStyle == DateTimeFormatStyle::Invalid) {
    return NS_ERROR_INVALID_ARG;
  }

  // If the user is asking for None on both, date and time style,
  // let's exit early.
  if (timeStyle == DateTimeFormatStyle::None &&
      dateStyle == DateTimeFormatStyle::None) {
    return NS_OK;
  }

  // If the locale is not specified, default to first regional prefs locale
  const nsACString* locale = &aLocale;
  AutoTArray<nsCString, 10> rpLocales;
  if (aLocale.IsEmpty()) {
    LocaleService::GetInstance()->GetRegionalPrefsLocales(rpLocales);
    MOZ_ASSERT(rpLocales.Length() > 0);
    locale = &rpLocales[0];
  }

  // Create a cache key from the locale + style options
  nsAutoCString key(*locale);
  key.Append(':');
  key.AppendInt(aDateFormatStyle);
  key.Append(':');
  key.AppendInt(aTimeFormatStyle);

  nsCString pattern;
  if (mPatternCache.Get(key, &pattern)) {
    aRetVal = pattern;
    return NS_OK;
  }

  if (!OverrideDateTimePattern(dateStyle, timeStyle, *locale, pattern)) {
    if (!ReadDateTimePattern(dateStyle, timeStyle, *locale, pattern)) {
      if (!GetDateTimePatternForStyle(dateStyle, timeStyle, *locale, pattern)) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  if (mPatternCache.Count() == kMaxCachedPatterns) {
    // Don't allow unlimited cache growth; just throw it away in the case of
    // pathological behavior where a page keeps requesting different formats
    // and locales.
    NS_WARNING("flushing DateTimePattern cache");
    mPatternCache.Clear();
  }
  mPatternCache.InsertOrUpdate(key, pattern);

  aRetVal = pattern;
  return NS_OK;
}

void OSPreferences::OverrideSkeletonHourCycle(bool aIs24Hour,
                                              nsAutoCString& aSkeleton) {
  if (aIs24Hour) {
    // If aSkeleton contains 'h' or 'K', replace with 'H' or 'k' respectively,
    // and delete 'a' if present.
    if (aSkeleton.FindChar('h') == -1 && aSkeleton.FindChar('K') == -1) {
      return;
    }
    for (int32_t i = 0; i < int32_t(aSkeleton.Length()); ++i) {
      switch (aSkeleton[i]) {
        case 'a':
          aSkeleton.Cut(i, 1);
          --i;
          break;
        case 'h':
          aSkeleton.SetCharAt('H', i);
          break;
        case 'K':
          aSkeleton.SetCharAt('k', i);
          break;
      }
    }
  } else {
    // If skeleton contains 'H' or 'k', replace with 'h' or 'K' respectively,
    // and add 'a' unless already present.
    if (aSkeleton.FindChar('H') == -1 && aSkeleton.FindChar('k') == -1) {
      return;
    }
    bool foundA = false;
    for (size_t i = 0; i < aSkeleton.Length(); ++i) {
      switch (aSkeleton[i]) {
        case 'a':
          foundA = true;
          break;
        case 'H':
          aSkeleton.SetCharAt('h', i);
          break;
        case 'k':
          aSkeleton.SetCharAt('K', i);
          break;
      }
    }
    if (!foundA) {
      aSkeleton.Append(char16_t('a'));
    }
  }
}
