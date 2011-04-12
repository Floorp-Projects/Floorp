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

  NS_OVERRIDE nsresult Init();

  NS_OVERRIDE NS_IMETHOD CheckForNewChrome();
  NS_OVERRIDE NS_IMETHOD CheckForOSAccessibility();
  NS_OVERRIDE NS_IMETHOD GetLocalesForPackage(const nsACString& aPackage,
                                              nsIUTF8StringEnumerator* *aResult);
  NS_OVERRIDE NS_IMETHOD IsLocaleRTL(const nsACString& package,
                                     PRBool *aResult);
  NS_OVERRIDE NS_IMETHOD GetSelectedLocale(const nsACString& aPackage,
                                           nsACString& aLocale);
  NS_OVERRIDE NS_IMETHOD Observe(nsISupports *aSubject, const char *aTopic,
                                 const PRUnichar *someData);

#ifdef MOZ_XUL
  NS_OVERRIDE NS_IMETHOD GetXULOverlays(nsIURI *aURI,
                                        nsISimpleEnumerator **_retval);
  NS_OVERRIDE NS_IMETHOD GetStyleOverlays(nsIURI *aURI,
                                          nsISimpleEnumerator **_retval);
#endif
  
  void SendRegisteredChrome(mozilla::dom::PContentParent* aChild);

 private:
  static PLDHashOperator CollectPackages(PLDHashTable *table,
                                         PLDHashEntryHdr *entry,
                                         PRUint32 number, void *arg);

  nsresult SelectLocaleFromPref(nsIPrefBranch* prefs);
  NS_OVERRIDE void UpdateSelectedLocale();
  NS_OVERRIDE nsIURI* GetBaseURIFromPackage(const nsCString& aPackage,
                                             const nsCString& aProvider,
                                             const nsCString& aPath);
  NS_OVERRIDE nsresult GetFlagsFromPackage(const nsCString& aPackage,
                                           PRUint32* aFlags);

  static const PLDHashTableOps kTableOps;
  static PLDHashNumber HashKey(PLDHashTable *table, const void *key);
  static PRBool        MatchKey(PLDHashTable *table, const PLDHashEntryHdr *entry,
                                const void *key);
  static void          ClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry);
  static PRBool        InitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
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

    PRBool Init() { return mTable.Init(); }
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

  PRBool mProfileLoaded;
  
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
