/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChromeRegistryChrome_h
#define nsChromeRegistryChrome_h

#include "nsChromeRegistry.h"

namespace mozilla {
namespace dom {
class PContentParent;
}
}

class nsIPrefBranch;

class nsChromeRegistryChrome : public nsChromeRegistry
{
 public:
  nsChromeRegistryChrome();
  ~nsChromeRegistryChrome();

  nsresult Init() MOZ_OVERRIDE;

  NS_IMETHOD CheckForNewChrome() MOZ_OVERRIDE;
  NS_IMETHOD CheckForOSAccessibility() MOZ_OVERRIDE;
  NS_IMETHOD GetLocalesForPackage(const nsACString& aPackage,
                                  nsIUTF8StringEnumerator* *aResult) MOZ_OVERRIDE;
  NS_IMETHOD IsLocaleRTL(const nsACString& package,
                         bool *aResult) MOZ_OVERRIDE;
  NS_IMETHOD GetSelectedLocale(const nsACString& aPackage,
                               nsACString& aLocale) MOZ_OVERRIDE;
  NS_IMETHOD Observe(nsISupports *aSubject, const char *aTopic,
                     const PRUnichar *someData) MOZ_OVERRIDE;

#ifdef MOZ_XUL
  NS_IMETHOD GetXULOverlays(nsIURI *aURI,
                            nsISimpleEnumerator **_retval) MOZ_OVERRIDE;
  NS_IMETHOD GetStyleOverlays(nsIURI *aURI,
                              nsISimpleEnumerator **_retval) MOZ_OVERRIDE;
#endif
  
  void SendRegisteredChrome(mozilla::dom::PContentParent* aChild);

 private:
  static PLDHashOperator CollectPackages(PLDHashTable *table,
                                         PLDHashEntryHdr *entry,
                                         PRUint32 number, void *arg);

  nsresult SelectLocaleFromPref(nsIPrefBranch* prefs);
  nsresult UpdateSelectedLocale() MOZ_OVERRIDE;
  nsIURI* GetBaseURIFromPackage(const nsCString& aPackage,
                                 const nsCString& aProvider,
                                 const nsCString& aPath) MOZ_OVERRIDE;
  nsresult GetFlagsFromPackage(const nsCString& aPackage,
                               PRUint32* aFlags) MOZ_OVERRIDE;

  static const PLDHashTableOps kTableOps;
  static PLDHashNumber HashKey(PLDHashTable *table, const void *key);
  static bool          MatchKey(PLDHashTable *table, const PLDHashEntryHdr *entry,
                                const void *key);
  static void          ClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry);
  static bool          InitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                                 const void *key);

  struct ProviderEntry
  {
    ProviderEntry(const nsACString& aProvider, nsIURI* aBase) :
    provider(aProvider),
    baseURI(aBase) { }

    nsCString        provider;
    nsCOMPtr<nsIURI> baseURI;
  };

  class nsProviderArray
  {
   public:
    nsProviderArray() :
    mArray(1) { }
    ~nsProviderArray()
    { Clear(); }

    // When looking up locales and skins, the "selected" locale is not always
    // available. This enum identifies what kind of match is desired/found.
    enum MatchType {
      EXACT = 0,
      LOCALE = 1, // "en-GB" is selected, we found "en-US"
      ANY = 2
    };

    nsIURI* GetBase(const nsACString& aPreferred, MatchType aType);
    const nsACString& GetSelected(const nsACString& aPreferred, MatchType aType);
    void    SetBase(const nsACString& aProvider, nsIURI* base);
    void    EnumerateToArray(nsTArray<nsCString> *a);
    void    Clear();

   private:
    ProviderEntry* GetProvider(const nsACString& aPreferred, MatchType aType);

    nsVoidArray mArray;
  };

  struct PackageEntry : public PLDHashEntryHdr
  {
    PackageEntry(const nsACString& package)
    : package(package), flags(0) { }
    ~PackageEntry() { }

    nsCString        package;
    nsCOMPtr<nsIURI> baseURI;
    PRUint32         flags;
    nsProviderArray  locales;
    nsProviderArray  skins;
  };

  class OverlayListEntry : public nsURIHashKey
  {
   public:
    typedef nsURIHashKey::KeyType        KeyType;
    typedef nsURIHashKey::KeyTypePointer KeyTypePointer;

    OverlayListEntry(KeyTypePointer aKey) : nsURIHashKey(aKey) { }
    OverlayListEntry(OverlayListEntry& toCopy) : nsURIHashKey(toCopy),
                                                 mArray(toCopy.mArray) { }
    ~OverlayListEntry() { }

    void AddURI(nsIURI* aURI);

    nsCOMArray<nsIURI> mArray;
  };

  class OverlayListHash
  {
   public:
    OverlayListHash() { }
    ~OverlayListHash() { }

    void Init() { mTable.Init(); }
    void Add(nsIURI* aBase, nsIURI* aOverlay);
    void Clear() { mTable.Clear(); }
    const nsCOMArray<nsIURI>* GetArray(nsIURI* aBase);

   private:
    nsTHashtable<OverlayListEntry> mTable;
  };

  // Hashes on the file to be overlaid (chrome://browser/content/browser.xul)
  // to a list of overlays/stylesheets
  OverlayListHash mOverlayHash;
  OverlayListHash mStyleHash;

  bool mProfileLoaded;
  
  nsCString mSelectedLocale;
  nsCString mSelectedSkin;

  // Hash of package names ("global") to PackageEntry objects
  PLDHashTable mPackagesHash;

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

#endif // nsChromeRegistryChrome_h
