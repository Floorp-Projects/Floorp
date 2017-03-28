/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/Services.h"
#include "nsIToolkitChromeRegistry.h"

using namespace mozilla::intl;


TEST(Intl_Locale_LocaleService, GetAppLocalesAsLangTags) {
  nsTArray<nsCString> appLocales;
  LocaleService::GetInstance()->GetAppLocalesAsLangTags(appLocales);

  ASSERT_FALSE(appLocales.IsEmpty());
}

TEST(Intl_Locale_LocaleService, GetAppLocalesAsLangTags_firstMatchesChromeReg) {
  nsTArray<nsCString> appLocales;
  LocaleService::GetInstance()->GetAppLocalesAsLangTags(appLocales);

  nsAutoCString uaLangTag;
  nsCOMPtr<nsIToolkitChromeRegistry> cr =
    mozilla::services::GetToolkitChromeRegistryService();
  if (cr) {
    cr->GetSelectedLocale(NS_LITERAL_CSTRING("global"), true, uaLangTag);
  }

  ASSERT_TRUE(appLocales[0].Equals(uaLangTag));
}

TEST(Intl_Locale_LocaleService, GetAppLocalesAsLangTags_lastIsEnUS) {
  nsTArray<nsCString> appLocales;
  LocaleService::GetInstance()->GetAppLocalesAsLangTags(appLocales);

  int32_t len = appLocales.Length();
  ASSERT_TRUE(appLocales[len - 1].EqualsLiteral("en-US"));
}

TEST(Intl_Locale_LocaleService, GetRequestedLocales) {
  nsTArray<nsCString> reqLocales;
  LocaleService::GetInstance()->GetRequestedLocales(reqLocales);

  int32_t len = reqLocales.Length();
  ASSERT_TRUE(len > 0);
}

TEST(Intl_Locale_LocaleService, GetAppLocaleAsLangTag) {
  nsTArray<nsCString> appLocales;
  LocaleService::GetInstance()->GetAppLocalesAsLangTags(appLocales);

  nsAutoCString locale;
  LocaleService::GetInstance()->GetAppLocaleAsLangTag(locale);

  ASSERT_TRUE(appLocales[0] == locale);
}

TEST(Intl_Locale_LocaleService, IsAppLocaleRTL) {
  // For now we can only test if the method doesn't crash.
  LocaleService::GetInstance()->IsAppLocaleRTL();
  ASSERT_TRUE(true);

}
