/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChromeRegistryChrome_h
#define nsChromeRegistryChrome_h

#include <utility>

#include "nsCOMArray.h"
#include "nsChromeRegistry.h"
#include "nsClassHashtable.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
class PContentParent;
}  // namespace dom
}  // namespace mozilla

class nsIPrefBranch;
struct ChromePackage;

class nsChromeRegistryChrome : public nsChromeRegistry {
 public:
  nsChromeRegistryChrome();
  ~nsChromeRegistryChrome();

  nsresult Init() override;

  NS_IMETHOD CheckForNewChrome() override;
  NS_IMETHOD GetLocalesForPackage(const nsACString& aPackage,
                                  nsIUTF8StringEnumerator** aResult) override;
  NS_IMETHOD IsLocaleRTL(const nsACString& package, bool* aResult) override;
  nsresult GetSelectedLocale(const nsACString& aPackage, nsACString& aLocale);
  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* someData) override;

  // If aChild is non-null then it is a new child to notify. If aChild is
  // null, then we have installed new chrome and we are resetting all of our
  // children's registered chrome.
  void SendRegisteredChrome(mozilla::dom::PContentParent* aChild);

 private:
  struct PackageEntry;
  static void ChromePackageFromPackageEntry(const nsACString& aPackageName,
                                            PackageEntry* aPackage,
                                            ChromePackage* aChromePackage,
                                            const nsCString& aSelectedSkin);

  nsresult OverrideLocalePackage(const nsACString& aPackage,
                                 nsACString& aOverride);
  nsIURI* GetBaseURIFromPackage(const nsCString& aPackage,
                                const nsCString& aProvider,
                                const nsCString& aPath) override;
  nsresult GetFlagsFromPackage(const nsCString& aPackage,
                               uint32_t* aFlags) override;

  struct ProviderEntry {
    ProviderEntry(const nsACString& aProvider, nsIURI* aBase)
        : provider(aProvider), baseURI(aBase) {}

    nsCString provider;
    nsCOMPtr<nsIURI> baseURI;
  };

  class nsProviderArray {
   public:
    nsProviderArray() : mArray(1) {}
    ~nsProviderArray() {}

    // When looking up locales and skins, the "selected" locale is not always
    // available. This enum identifies what kind of match is desired/found.
    enum MatchType {
      EXACT = 0,
      LOCALE = 1,  // "en-GB" is selected, we found "en-US"
      ANY = 2
    };

    nsIURI* GetBase(const nsACString& aPreferred, MatchType aType);
    const nsACString& GetSelected(const nsACString& aPreferred,
                                  MatchType aType);
    void SetBase(const nsACString& aProvider, nsIURI* base);
    void EnumerateToArray(nsTArray<nsCString>* a);

   private:
    ProviderEntry* GetProvider(const nsACString& aPreferred, MatchType aType);

    nsTArray<ProviderEntry> mArray;
  };

  struct PackageEntry : public PLDHashEntryHdr {
    PackageEntry() : flags(0) {}
    ~PackageEntry() {}

    nsCOMPtr<nsIURI> baseURI;
    uint32_t flags;
    nsProviderArray locales;
    nsProviderArray skins;
  };

  bool mProfileLoaded;
  bool mDynamicRegistration;

  // Hash of package names ("global") to PackageEntry objects
  nsClassHashtable<nsCStringHashKey, PackageEntry> mPackagesHash;

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

#endif  // nsChromeRegistryChrome_h
