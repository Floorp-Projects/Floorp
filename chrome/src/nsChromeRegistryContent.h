/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChromeRegistryContent_h
#define nsChromeRegistryContent_h

#include "nsChromeRegistry.h"
#include "nsTArray.h"
#include "nsClassHashtable.h"

class nsCString;
struct ChromePackage;
struct ResourceMapping;
struct OverrideMapping;

class nsChromeRegistryContent : public nsChromeRegistry
{
 public:
  nsChromeRegistryContent();
  
  void RegisterRemoteChrome(const nsTArray<ChromePackage>& aPackages,
                            const nsTArray<ResourceMapping>& aResources,
                            const nsTArray<OverrideMapping>& aOverrides,
                            const nsACString& aLocale);

  NS_OVERRIDE NS_IMETHOD GetLocalesForPackage(const nsACString& aPackage,
                                              nsIUTF8StringEnumerator* *aResult);
  NS_OVERRIDE NS_IMETHOD CheckForNewChrome();
  NS_OVERRIDE NS_IMETHOD CheckForOSAccessibility();
  NS_OVERRIDE NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                                 const PRUnichar* aData);
  NS_OVERRIDE NS_IMETHOD IsLocaleRTL(const nsACString& package,
                                     bool *aResult);
  NS_OVERRIDE NS_IMETHOD GetSelectedLocale(const nsACString& aPackage,
                                           nsACString& aLocale);
  NS_OVERRIDE NS_IMETHOD GetStyleOverlays(nsIURI *aChromeURL,
                                          nsISimpleEnumerator **aResult);
  NS_OVERRIDE NS_IMETHOD GetXULOverlays(nsIURI *aChromeURL,
                                        nsISimpleEnumerator **aResult);

 private:
  struct PackageEntry
  {
    PackageEntry() : flags(0) { }
    ~PackageEntry() { }

    nsCOMPtr<nsIURI> contentBaseURI;
    nsCOMPtr<nsIURI> localeBaseURI;
    nsCOMPtr<nsIURI> skinBaseURI;
    PRUint32         flags;
  };
  
  void RegisterPackage(const ChromePackage& aPackage);
  void RegisterResource(const ResourceMapping& aResource);
  void RegisterOverride(const OverrideMapping& aOverride);

  NS_OVERRIDE nsresult UpdateSelectedLocale();
  NS_OVERRIDE nsIURI* GetBaseURIFromPackage(const nsCString& aPackage,
                                 const nsCString& aProvider,
                                 const nsCString& aPath);
  NS_OVERRIDE nsresult GetFlagsFromPackage(const nsCString& aPackage, PRUint32* aFlags);

  nsClassHashtable<nsCStringHashKey, PackageEntry> mPackagesHash;
  nsCString mLocale;

  virtual void ManifestContent(ManifestProcessingContext& cx, int lineno,
                               char *const * argv, bool platform,
                               bool contentaccessible);
  virtual void ManifestLocale(ManifestProcessingContext& cx, int lineno,
                              char *const * argv, bool platform,
                              bool contentaccessible);
  virtual void ManifestSkin(ManifestProcessingContext& cx, int lineno,
                            char *const * argv, bool platform,
                            bool contentaccessible);
  virtual void ManifestOverlay(ManifestProcessingContext& cx, int lineno,
                               char *const * argv, bool platform,
                               bool contentaccessible);
  virtual void ManifestStyle(ManifestProcessingContext& cx, int lineno,
                             char *const * argv, bool platform,
                             bool contentaccessible);
  virtual void ManifestOverride(ManifestProcessingContext& cx, int lineno,
                                char *const * argv, bool platform,
                                bool contentaccessible);
  virtual void ManifestResource(ManifestProcessingContext& cx, int lineno,
                                char *const * argv, bool platform,
                                bool contentaccessible);
};

#endif // nsChromeRegistryContent_h
