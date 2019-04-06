/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/Preferences.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/intl/MozLocale.h"
#include "nsIToolkitChromeRegistry.h"

using namespace mozilla::intl;

TEST(Intl_Locale_LocaleService, GetAppLocalesAsBCP47)
{
  nsTArray<nsCString> appLocales;
  LocaleService::GetInstance()->GetAppLocalesAsBCP47(appLocales);

  ASSERT_FALSE(appLocales.IsEmpty());
}

TEST(Intl_Locale_LocaleService, GetAppLocalesAsLangTags)
{
  nsTArray<nsCString> appLocales;
  LocaleService::GetInstance()->GetAppLocalesAsLangTags(appLocales);

  ASSERT_FALSE(appLocales.IsEmpty());
}

TEST(Intl_Locale_LocaleService, GetAppLocalesAsLangTags_lastIsEnUS)
{
  nsAutoCString lastFallbackLocale;
  LocaleService::GetInstance()->GetLastFallbackLocale(lastFallbackLocale);

  nsTArray<nsCString> appLocales;
  LocaleService::GetInstance()->GetAppLocalesAsLangTags(appLocales);

  int32_t len = appLocales.Length();
  ASSERT_TRUE(appLocales[len - 1].Equals(lastFallbackLocale));
}

TEST(Intl_Locale_LocaleService, GetAppLocaleAsLangTag)
{
  nsTArray<nsCString> appLocales;
  LocaleService::GetInstance()->GetAppLocalesAsLangTags(appLocales);

  nsAutoCString locale;
  LocaleService::GetInstance()->GetAppLocaleAsLangTag(locale);

  ASSERT_TRUE(appLocales[0] == locale);
}

TEST(Intl_Locale_LocaleService, GetRegionalPrefsLocales)
{
  nsTArray<nsCString> rpLocales;
  LocaleService::GetInstance()->GetRegionalPrefsLocales(rpLocales);

  int32_t len = rpLocales.Length();
  ASSERT_TRUE(len > 0);
}

TEST(Intl_Locale_LocaleService, GetWebExposedLocales)
{
  const nsTArray<nsCString> spoofLocale{NS_LITERAL_CSTRING("de")};
  LocaleService::GetInstance()->SetAvailableLocales(spoofLocale);
  LocaleService::GetInstance()->SetRequestedLocales(spoofLocale);

  nsTArray<nsCString> pvLocales;

  mozilla::Preferences::SetInt("privacy.spoof_english", 0);
  LocaleService::GetInstance()->GetWebExposedLocales(pvLocales);
  ASSERT_TRUE(pvLocales.Length() > 0);
  ASSERT_TRUE(pvLocales[0].Equals(NS_LITERAL_CSTRING("de")));

  mozilla::Preferences::SetCString("intl.locale.privacy.web_exposed", "zh-TW");
  LocaleService::GetInstance()->GetWebExposedLocales(pvLocales);
  ASSERT_TRUE(pvLocales.Length() > 0);
  ASSERT_TRUE(pvLocales[0].Equals(NS_LITERAL_CSTRING("zh-TW")));

  mozilla::Preferences::SetInt("privacy.spoof_english", 2);
  LocaleService::GetInstance()->GetWebExposedLocales(pvLocales);
  ASSERT_EQ(1u, pvLocales.Length());
  ASSERT_TRUE(pvLocales[0].Equals(NS_LITERAL_CSTRING("en-US")));
}

TEST(Intl_Locale_LocaleService, GetRequestedLocales)
{
  nsTArray<nsCString> reqLocales;
  LocaleService::GetInstance()->GetRequestedLocales(reqLocales);

  int32_t len = reqLocales.Length();
  ASSERT_TRUE(len > 0);
}

TEST(Intl_Locale_LocaleService, GetAvailableLocales)
{
  nsTArray<nsCString> availableLocales;
  LocaleService::GetInstance()->GetAvailableLocales(availableLocales);

  int32_t len = availableLocales.Length();
  ASSERT_TRUE(len > 0);
}

TEST(Intl_Locale_LocaleService, GetPackagedLocales)
{
  nsTArray<nsCString> packagedLocales;
  LocaleService::GetInstance()->GetPackagedLocales(packagedLocales);

  int32_t len = packagedLocales.Length();
  ASSERT_TRUE(len > 0);
}

TEST(Intl_Locale_LocaleService, GetDefaultLocale)
{
  nsAutoCString locStr;
  LocaleService::GetInstance()->GetDefaultLocale(locStr);

  ASSERT_FALSE(locStr.IsEmpty());
  ASSERT_TRUE(Locale(locStr).IsWellFormed());
}

TEST(Intl_Locale_LocaleService, IsAppLocaleRTL)
{
  // For now we can only test if the method doesn't crash.
  LocaleService::GetInstance()->IsAppLocaleRTL();
  ASSERT_TRUE(true);
}
