/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RegistryMessageUtils.h"
#include "nsChromeRegistryContent.h"
#include "nsString.h"
#include "nsNetUtil.h"
#include "nsIResProtocolHandler.h"

nsChromeRegistryContent::nsChromeRegistryContent()
{
}

void
nsChromeRegistryContent::RegisterRemoteChrome(
    const InfallibleTArray<ChromePackage>& aPackages,
    const InfallibleTArray<SubstitutionMapping>& aSubstitutions,
    const InfallibleTArray<OverrideMapping>& aOverrides,
    const nsACString& aLocale,
    bool aReset)
{
  MOZ_ASSERT(aReset || mLocale.IsEmpty(),
             "RegisterChrome twice?");

  if (aReset) {
    mPackagesHash.Clear();
    mOverrideTable.Clear();
    // XXX Can't clear resources.
  }

  for (uint32_t i = aPackages.Length(); i > 0; ) {
    --i;
    RegisterPackage(aPackages[i]);
  }

  for (uint32_t i = aSubstitutions.Length(); i > 0; ) {
    --i;
    RegisterSubstitution(aSubstitutions[i]);
  }

  for (uint32_t i = aOverrides.Length(); i > 0; ) {
    --i;
    RegisterOverride(aOverrides[i]);
  }

  mLocale = aLocale;
}

void
nsChromeRegistryContent::RegisterPackage(const ChromePackage& aPackage)
{
  nsCOMPtr<nsIIOService> io (do_GetIOService());
  if (!io)
    return;

  nsCOMPtr<nsIURI> content, locale, skin;

  if (aPackage.contentBaseURI.spec.Length()) {
    nsresult rv = NS_NewURI(getter_AddRefs(content),
                            aPackage.contentBaseURI.spec,
                            nullptr, nullptr, io);
    if (NS_FAILED(rv))
      return;
  }
  if (aPackage.localeBaseURI.spec.Length()) {
    nsresult rv = NS_NewURI(getter_AddRefs(locale),
                            aPackage.localeBaseURI.spec,
                            nullptr, nullptr, io);
    if (NS_FAILED(rv))
      return;
  }
  if (aPackage.skinBaseURI.spec.Length()) {
    nsresult rv = NS_NewURI(getter_AddRefs(skin),
                            aPackage.skinBaseURI.spec,
                            nullptr, nullptr, io);
    if (NS_FAILED(rv))
      return;
  }

  PackageEntry* entry = new PackageEntry;
  entry->flags = aPackage.flags;
  entry->contentBaseURI = content;
  entry->localeBaseURI = locale;
  entry->skinBaseURI = skin;

  mPackagesHash.Put(aPackage.package, entry);
}

void
nsChromeRegistryContent::RegisterSubstitution(const SubstitutionMapping& aSubstitution)
{
  nsCOMPtr<nsIIOService> io (do_GetIOService());
  if (!io)
    return;

  nsCOMPtr<nsIProtocolHandler> ph;
  nsresult rv = io->GetProtocolHandler(aSubstitution.scheme.get(), getter_AddRefs(ph));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsISubstitutingProtocolHandler> sph (do_QueryInterface(ph));
  if (!sph)
    return;

  nsCOMPtr<nsIURI> resolvedURI;
  if (aSubstitution.resolvedURI.spec.Length()) {
    rv = NS_NewURI(getter_AddRefs(resolvedURI),
                   aSubstitution.resolvedURI.spec,
                   nullptr, nullptr, io);
    if (NS_FAILED(rv))
      return;
  }

  rv = sph->SetSubstitutionWithFlags(aSubstitution.path, resolvedURI, aSubstitution.flags);
  if (NS_FAILED(rv))
    return;
}

void
nsChromeRegistryContent::RegisterOverride(const OverrideMapping& aOverride)
{
  nsCOMPtr<nsIIOService> io (do_GetIOService());
  if (!io)
    return;

  nsCOMPtr<nsIURI> chromeURI, overrideURI;
  nsresult rv = NS_NewURI(getter_AddRefs(chromeURI),
                          aOverride.originalURI.spec,
                          nullptr, nullptr, io);
  if (NS_FAILED(rv))
    return;

  rv = NS_NewURI(getter_AddRefs(overrideURI), aOverride.overrideURI.spec,
                 nullptr, nullptr, io);
  if (NS_FAILED(rv))
    return;

  mOverrideTable.Put(chromeURI, overrideURI);
}

nsIURI*
nsChromeRegistryContent::GetBaseURIFromPackage(const nsCString& aPackage,
                                               const nsCString& aProvider,
                                               const nsCString& aPath)
{
  PackageEntry* entry;
  if (!mPackagesHash.Get(aPackage, &entry)) {
    return nullptr;
  }

  if (aProvider.EqualsLiteral("locale")) {
    return entry->localeBaseURI;
  }
  else if (aProvider.EqualsLiteral("skin")) {
    return entry->skinBaseURI;
  }
  else if (aProvider.EqualsLiteral("content")) {
    return entry->contentBaseURI;
  }
  return nullptr;
}

nsresult
nsChromeRegistryContent::GetFlagsFromPackage(const nsCString& aPackage,
                                             uint32_t* aFlags)
{
  PackageEntry* entry;
  if (!mPackagesHash.Get(aPackage, &entry)) {
    return NS_ERROR_FAILURE;
  }
  *aFlags = entry->flags;
  return NS_OK;
}

// All functions following only make sense in chrome, and therefore assert

#define CONTENT_NOTREACHED() \
  MOZ_ASSERT_UNREACHABLE("Content should not be calling this")

#define CONTENT_NOT_IMPLEMENTED() \
  CONTENT_NOTREACHED();           \
  return NS_ERROR_NOT_IMPLEMENTED;

NS_IMETHODIMP
nsChromeRegistryContent::GetLocalesForPackage(const nsACString& aPackage,
                                              nsIUTF8StringEnumerator* *aResult)
{
  CONTENT_NOT_IMPLEMENTED();
}

NS_IMETHODIMP
nsChromeRegistryContent::CheckForOSAccessibility()
{
  CONTENT_NOT_IMPLEMENTED();
}

NS_IMETHODIMP
nsChromeRegistryContent::CheckForNewChrome()
{
  CONTENT_NOT_IMPLEMENTED();
}

NS_IMETHODIMP
nsChromeRegistryContent::IsLocaleRTL(const nsACString& aPackage,
                                     bool *aResult)
{
  if (aPackage != nsDependentCString("global")) {
    NS_ERROR("Packages other than global unavailable");
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aResult = GetDirectionForLocale(mLocale);
  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistryContent::GetSelectedLocale(const nsACString& aPackage,
                                           bool aAsBCP47,
                                           nsACString& aLocale)
{
  if (aPackage != nsDependentCString("global")) {
    NS_ERROR("Uh-oh, caller wanted something other than 'some local'");
    return NS_ERROR_NOT_AVAILABLE;
  }
  aLocale = mLocale;
  if (aAsBCP47) {
    SanitizeForBCP47(aLocale);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistryContent::Observe(nsISupports* aSubject, const char* aTopic,
                                 const char16_t* aData)
{
  CONTENT_NOT_IMPLEMENTED();
}

void
nsChromeRegistryContent::ManifestContent(ManifestProcessingContext& cx,
                                         int lineno, char *const * argv,
                                         int flags)
{
  CONTENT_NOTREACHED();
}

void
nsChromeRegistryContent::ManifestLocale(ManifestProcessingContext& cx,
                                        int lineno,
                                        char *const * argv, int flags)
{
  CONTENT_NOTREACHED();
}

void
nsChromeRegistryContent::ManifestSkin(ManifestProcessingContext& cx,
                                      int lineno,
                                      char *const * argv, int flags)
{
  CONTENT_NOTREACHED();
}

void
nsChromeRegistryContent::ManifestOverride(ManifestProcessingContext& cx,
                                          int lineno,
                                          char *const * argv, int flags)
{
  CONTENT_NOTREACHED();
}

void
nsChromeRegistryContent::ManifestResource(ManifestProcessingContext& cx,
                                          int lineno,
                                          char *const * argv, int flags)
{
  CONTENT_NOTREACHED();
}
