/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/intl/LocaleService.h"
#include "nsIToolkitChromeRegistry.h"

using namespace mozilla::intl;

TEST(Intl_Locale_LocaleService, Negotiate)
{
  nsTArray<nsCString> requestedLocales;
  nsTArray<nsCString> availableLocales;
  nsTArray<nsCString> supportedLocales;
  nsAutoCString defaultLocale("en-US");
  int32_t strategy = LocaleService::kLangNegStrategyFiltering;

  requestedLocales.AppendElement(NS_LITERAL_CSTRING("sr"));

  availableLocales.AppendElement(NS_LITERAL_CSTRING("sr-Cyrl"));
  availableLocales.AppendElement(NS_LITERAL_CSTRING("sr-Latn"));

  LocaleService::GetInstance()->NegotiateLanguages(
      requestedLocales, availableLocales, defaultLocale, strategy,
      supportedLocales);

  ASSERT_TRUE(supportedLocales.Length() == 2);
  ASSERT_TRUE(supportedLocales[0].EqualsLiteral("sr-Cyrl"));
  ASSERT_TRUE(supportedLocales[1].EqualsLiteral("en-US"));
}

TEST(Intl_Locale_LocaleService, UseLSDefaultLocale)
{
  nsTArray<nsCString> requestedLocales;
  nsTArray<nsCString> availableLocales;
  nsTArray<nsCString> supportedLocales;
  nsAutoCString defaultLocale("");
  int32_t strategy = LocaleService::kLangNegStrategyLookup;

  requestedLocales.AppendElement(NS_LITERAL_CSTRING("sr"));

  availableLocales.AppendElement(NS_LITERAL_CSTRING("de"));

  LocaleService::GetInstance()->NegotiateLanguages(
      requestedLocales, availableLocales, defaultLocale, strategy,
      supportedLocales);

  nsAutoCString lsDefaultLocale;
  LocaleService::GetInstance()->GetDefaultLocale(lsDefaultLocale);
  ASSERT_TRUE(supportedLocales.Length() == 1);
  ASSERT_TRUE(supportedLocales[0].Equals(lsDefaultLocale));
}
