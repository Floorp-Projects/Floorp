/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Matthews <josh@joshmatthews.net> (Initial Developer)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "RegistryMessageUtils.h"
#include "nsChromeRegistry.h"
#include "nsChromeRegistryContent.h"
#include "nsString.h"
#include "nsNetUtil.h"
#include "nsResProtocolHandler.h"

nsChromeRegistryContent::nsChromeRegistryContent()
{
  mPackagesHash.Init();
}

void
nsChromeRegistryContent::RegisterRemoteChrome(
    const nsTArray<ChromePackage>& aPackages,
    const nsTArray<ResourceMapping>& aResources,
    const nsTArray<OverrideMapping>& aOverrides,
    const nsACString& aLocale)
{
  NS_ABORT_IF_FALSE(mLocale == nsDependentCString(""),
                    "RegisterChrome twice?");

  for (PRUint32 i = aPackages.Length(); i > 0; ) {
    --i;
    RegisterPackage(aPackages[i]);
  }

  for (PRUint32 i = aResources.Length(); i > 0; ) {
    --i;
    RegisterResource(aResources[i]);
  }

  for (PRUint32 i = aOverrides.Length(); i > 0; ) {
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
                            aPackage.contentBaseURI.charset.get(),
                            nsnull, io);
    if (NS_FAILED(rv))
      return;
  }
  if (aPackage.localeBaseURI.spec.Length()) {
    nsresult rv = NS_NewURI(getter_AddRefs(locale),
                            aPackage.localeBaseURI.spec,
                            aPackage.localeBaseURI.charset.get(),
                            nsnull, io);
    if (NS_FAILED(rv))
      return;
  }
  if (aPackage.skinBaseURI.spec.Length()) {
    nsCOMPtr<nsIURI> skinBaseURI;
    nsresult rv = NS_NewURI(getter_AddRefs(skin),
                            aPackage.skinBaseURI.spec,
                            aPackage.skinBaseURI.charset.get(),
                            nsnull, io);
    if (NS_FAILED(rv))
      return;
  }

  PackageEntry* entry = new PackageEntry;
  entry->flags = aPackage.flags;
  entry->contentBaseURI = content;
  entry->localeBaseURI = locale;
  entry->skinBaseURI = skin;

  nsresult rv = mPackagesHash.Put(aPackage.package, entry);
  if (NS_FAILED(rv))
    return;
}

void
nsChromeRegistryContent::RegisterResource(const ResourceMapping& aResource)
{
  nsCOMPtr<nsIIOService> io (do_GetIOService());
  if (!io)
    return;

  nsCOMPtr<nsIProtocolHandler> ph;
  nsresult rv = io->GetProtocolHandler("resource", getter_AddRefs(ph));
  if (NS_FAILED(rv))
    return;
  
  nsCOMPtr<nsIResProtocolHandler> rph (do_QueryInterface(ph));
  if (!rph)
    return;

  nsCOMPtr<nsIURI> resolvedURI;
  if (aResource.resolvedURI.spec.Length()) {
    nsresult rv = NS_NewURI(getter_AddRefs(resolvedURI),
                            aResource.resolvedURI.spec,
                            aResource.resolvedURI.charset.get(),
                            nsnull, io);                 
    if (NS_FAILED(rv))
      return;
  }

  rv = rph->SetSubstitution(aResource.resource, resolvedURI);
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
                          aOverride.originalURI.charset.get(),
                          nsnull, io);
  if (NS_FAILED(rv))
    return;

  rv = NS_NewURI(getter_AddRefs(overrideURI), aOverride.overrideURI.spec,
                 aOverride.overrideURI.charset.get(), nsnull, io);
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
    return nsnull;
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
  return nsnull;
}

nsresult
nsChromeRegistryContent::GetFlagsFromPackage(const nsCString& aPackage,
                                             PRUint32* aFlags)
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
  NS_NOTREACHED("Content should not be calling this")

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
nsChromeRegistryContent::IsLocaleRTL(const nsACString& package,
                                     bool *aResult)
{
  CONTENT_NOT_IMPLEMENTED();
}

NS_IMETHODIMP
nsChromeRegistryContent::GetSelectedLocale(const nsACString& aPackage,
                                           nsACString& aLocale)
{
  if (aPackage != nsDependentCString("global")) {
    NS_ERROR("Uh-oh, caller wanted something other than 'some local'");
    return NS_ERROR_NOT_AVAILABLE;
  }
  aLocale = mLocale;
  return NS_OK;
}
  
NS_IMETHODIMP
nsChromeRegistryContent::Observe(nsISupports* aSubject, const char* aTopic,
                                 const PRUnichar* aData)
{
  CONTENT_NOT_IMPLEMENTED();
}

NS_IMETHODIMP
nsChromeRegistryContent::GetStyleOverlays(nsIURI *aChromeURL,
                                          nsISimpleEnumerator **aResult)
{
  CONTENT_NOT_IMPLEMENTED();
}

NS_IMETHODIMP
nsChromeRegistryContent::GetXULOverlays(nsIURI *aChromeURL,
                                        nsISimpleEnumerator **aResult)
{
  CONTENT_NOT_IMPLEMENTED();
}

nsresult nsChromeRegistryContent::UpdateSelectedLocale()
{
  CONTENT_NOT_IMPLEMENTED();
}

void
nsChromeRegistryContent::ManifestContent(ManifestProcessingContext& cx,
                                         int lineno, char *const * argv,
                                         bool platform, bool contentaccessible)
{
  CONTENT_NOTREACHED();
}

void
nsChromeRegistryContent::ManifestLocale(ManifestProcessingContext& cx,
                                        int lineno,
                                        char *const * argv, bool platform,
                                        bool contentaccessible)
{
  CONTENT_NOTREACHED();
}

void
nsChromeRegistryContent::ManifestSkin(ManifestProcessingContext& cx,
                                      int lineno,
                                      char *const * argv, bool platform,
                                      bool contentaccessible)
{
  CONTENT_NOTREACHED();
}

void
nsChromeRegistryContent::ManifestOverlay(ManifestProcessingContext& cx, int lineno,
                                         char *const * argv, bool platform,
                                         bool contentaccessible)
{
  CONTENT_NOTREACHED();
}

void
nsChromeRegistryContent::ManifestStyle(ManifestProcessingContext& cx,
                                       int lineno,
                                       char *const * argv, bool platform,
                                       bool contentaccessible)
{
  CONTENT_NOTREACHED();
}

void
nsChromeRegistryContent::ManifestOverride(ManifestProcessingContext& cx,
                                          int lineno,
                                          char *const * argv, bool platform,
                                          bool contentaccessible)
{
  CONTENT_NOTREACHED();
}

void
nsChromeRegistryContent::ManifestResource(ManifestProcessingContext& cx,
                                          int lineno,
                                          char *const * argv, bool platform,
                                          bool contentaccessible)
{
  CONTENT_NOTREACHED();
}
