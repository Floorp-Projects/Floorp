/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FallbackEncoding.h"

#include "mozilla/dom/EncodingUtils.h"
#include "nsUConvPropertySearch.h"
#include "nsIChromeRegistry.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"

namespace mozilla {
namespace dom {

static const nsUConvProp localesFallbacks[] = {
#include "localesfallbacks.properties.h"
};

static const nsUConvProp domainsFallbacks[] = {
#include "domainsfallbacks.properties.h"
};

static const nsUConvProp nonParticipatingDomains[] = {
#include "nonparticipatingdomains.properties.h"
};

FallbackEncoding* FallbackEncoding::sInstance = nullptr;
bool FallbackEncoding::sGuessFallbackFromTopLevelDomain = true;

FallbackEncoding::FallbackEncoding()
{
  MOZ_COUNT_CTOR(FallbackEncoding);
  MOZ_ASSERT(!FallbackEncoding::sInstance,
             "Singleton already exists.");
}

FallbackEncoding::~FallbackEncoding()
{
  MOZ_COUNT_DTOR(FallbackEncoding);
}

void
FallbackEncoding::Get(nsACString& aFallback)
{
  if (!mFallback.IsEmpty()) {
    aFallback = mFallback;
    return;
  }

  const nsAdoptingCString& override =
    Preferences::GetCString("intl.charset.fallback.override");
  // Don't let the user break things by setting the override to unreasonable
  // values via about:config
  if (!EncodingUtils::FindEncodingForLabel(override, mFallback) ||
      !EncodingUtils::IsAsciiCompatible(mFallback) ||
      mFallback.EqualsLiteral("UTF-8")) {
    mFallback.Truncate();
  }

  if (!mFallback.IsEmpty()) {
    aFallback = mFallback;
    return;
  }

  nsAutoCString locale;
  nsCOMPtr<nsIXULChromeRegistry> registry =
    mozilla::services::GetXULChromeRegistryService();
  if (registry) {
    registry->GetSelectedLocale(NS_LITERAL_CSTRING("global"), locale);
  }

  // Let's lower case the string just in case unofficial language packs
  // don't stick to conventions.
  ToLowerCase(locale); // ASCII lowercasing with CString input!

  // Special case Traditional Chinese before throwing away stuff after the
  // language itself. Today we only ship zh-TW, but be defensive about
  // possible future values.
  if (locale.EqualsLiteral("zh-tw") ||
      locale.EqualsLiteral("zh-hk") ||
      locale.EqualsLiteral("zh-mo") ||
      locale.EqualsLiteral("zh-hant")) {
    mFallback.AssignLiteral("Big5");
    aFallback = mFallback;
    return;
  }

  // Throw away regions and other variants to accommodate weird stuff seen
  // in telemetry--apparently unofficial language packs.
  int32_t index = locale.FindChar('-');
  if (index >= 0) {
    locale.Truncate(index);
  }

  if (NS_FAILED(nsUConvPropertySearch::SearchPropertyValue(
      localesFallbacks, ArrayLength(localesFallbacks), locale, mFallback))) {
    mFallback.AssignLiteral("windows-1252");
  }

  aFallback = mFallback;
}

void
FallbackEncoding::FromLocale(nsACString& aFallback)
{
  MOZ_ASSERT(FallbackEncoding::sInstance,
             "Using uninitialized fallback cache.");
  FallbackEncoding::sInstance->Get(aFallback);
}

// PrefChangedFunc
void
FallbackEncoding::PrefChanged(const char*, void*)
{
  MOZ_ASSERT(FallbackEncoding::sInstance,
             "Pref callback called with null fallback cache.");
  FallbackEncoding::sInstance->Invalidate();
}

void
FallbackEncoding::Initialize()
{
  MOZ_ASSERT(!FallbackEncoding::sInstance,
             "Initializing pre-existing fallback cache.");
  FallbackEncoding::sInstance = new FallbackEncoding;
  Preferences::RegisterCallback(FallbackEncoding::PrefChanged,
                                "intl.charset.fallback.override",
                                nullptr);
  Preferences::RegisterCallback(FallbackEncoding::PrefChanged,
                                "general.useragent.locale",
                                nullptr);
  Preferences::AddBoolVarCache(&sGuessFallbackFromTopLevelDomain,
                               "intl.charset.fallback.tld");
}

void
FallbackEncoding::Shutdown()
{
  MOZ_ASSERT(FallbackEncoding::sInstance,
             "Releasing non-existent fallback cache.");
  delete FallbackEncoding::sInstance;
  FallbackEncoding::sInstance = nullptr;
}

bool
FallbackEncoding::IsParticipatingTopLevelDomain(const nsACString& aTLD)
{
  nsAutoCString dummy;
  return NS_FAILED(nsUConvPropertySearch::SearchPropertyValue(
      nonParticipatingDomains,
      ArrayLength(nonParticipatingDomains),
      aTLD,
      dummy));
}

void
FallbackEncoding::FromTopLevelDomain(const nsACString& aTLD,
                                     nsACString& aFallback)
{
  if (NS_FAILED(nsUConvPropertySearch::SearchPropertyValue(
      domainsFallbacks, ArrayLength(domainsFallbacks), aTLD, aFallback))) {
    aFallback.AssignLiteral("windows-1252");
  }
}

} // namespace dom
} // namespace mozilla
