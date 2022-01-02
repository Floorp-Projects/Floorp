/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChromeRegistryContent_h
#define nsChromeRegistryContent_h

#include "nsChromeRegistry.h"
#include "nsClassHashtable.h"

struct ChromePackage;
struct SubstitutionMapping;
struct OverrideMapping;

class nsChromeRegistryContent : public nsChromeRegistry {
 public:
  nsChromeRegistryContent();

  void RegisterRemoteChrome(const nsTArray<ChromePackage>& aPackages,
                            const nsTArray<SubstitutionMapping>& aResources,
                            const nsTArray<OverrideMapping>& aOverrides,
                            const nsACString& aLocale, bool aReset);

  NS_IMETHOD GetLocalesForPackage(const nsACString& aPackage,
                                  nsIUTF8StringEnumerator** aResult) override;
  NS_IMETHOD CheckForNewChrome() override;
  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override;
  NS_IMETHOD IsLocaleRTL(const nsACString& package, bool* aResult) override;

  void RegisterPackage(const ChromePackage& aPackage);
  void RegisterOverride(const OverrideMapping& aOverride);
  void RegisterSubstitution(const SubstitutionMapping& aResource);

 private:
  struct PackageEntry {
    PackageEntry() : flags(0) {}
    ~PackageEntry() {}

    nsCOMPtr<nsIURI> contentBaseURI;
    nsCOMPtr<nsIURI> localeBaseURI;
    nsCOMPtr<nsIURI> skinBaseURI;
    uint32_t flags;
  };

  nsIURI* GetBaseURIFromPackage(const nsCString& aPackage,
                                const nsCString& aProvider,
                                const nsCString& aPath) override;
  nsresult GetFlagsFromPackage(const nsCString& aPackage,
                               uint32_t* aFlags) override;

  nsClassHashtable<nsCStringHashKey, PackageEntry> mPackagesHash;
  nsCString mLocale;

  virtual void ManifestContent(ManifestProcessingContext& cx, int lineno,
                               char* const* argv, int flags) override;
  virtual void ManifestLocale(ManifestProcessingContext& cx, int lineno,
                              char* const* argv, int flags) override;
  virtual void ManifestSkin(ManifestProcessingContext& cx, int lineno,
                            char* const* argv, int flags) override;
  virtual void ManifestOverride(ManifestProcessingContext& cx, int lineno,
                                char* const* argv, int flags) override;
  virtual void ManifestResource(ManifestProcessingContext& cx, int lineno,
                                char* const* argv, int flags) override;
};

#endif  // nsChromeRegistryContent_h
