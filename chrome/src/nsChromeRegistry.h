/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Benjamin Smedberg <benjamin@smedbergs.us>
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

#include "nsIChromeRegistry.h"
#include "nsIToolkitChromeRegistry.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

#ifdef MOZ_XUL
#include "nsIXULOverlayProvider.h"
#endif

#include "pldhash.h"

#include "nsCOMArray.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "nsURIHashKey.h"
#include "nsVoidArray.h"
#include "nsInterfaceHashtable.h"

struct PRFileDesc;
class nsIAtom;
class nsICSSLoader;
class nsICSSStyleSheet;
class nsIDOMWindowInternal;
class nsILocalFile;
class nsIRDFDataSource;
class nsIRDFResource;
class nsIRDFService;
class nsISimpleEnumerator;
class nsIURL;

// for component registration
// {47049e42-1d87-482a-984d-56ae185e367a}
#define NS_CHROMEREGISTRY_CID \
{ 0x47049e42, 0x1d87, 0x482a, { 0x98, 0x4d, 0x56, 0xae, 0x18, 0x5e, 0x36, 0x7a } }

class nsChromeRegistry : public nsIToolkitChromeRegistry,
#ifdef MOZ_XUL
                         public nsIXULOverlayProvider,
#endif
                         public nsIObserver,
                         public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS

  // nsIChromeRegistry methods:
  NS_DECL_NSICHROMEREGISTRY
  NS_DECL_NSIXULCHROMEREGISTRY
  NS_DECL_NSITOOLKITCHROMEREGISTRY

#ifdef MOZ_XUL
  NS_DECL_NSIXULOVERLAYPROVIDER
#endif

  NS_DECL_NSIOBSERVER

  // nsChromeRegistry methods:
  nsChromeRegistry() : mInitialized(PR_FALSE) { }
  ~nsChromeRegistry();

  nsresult Init();

  static nsChromeRegistry* gChromeRegistry;

  static nsresult Canonify(nsIURL* aChromeURL);

protected:
  nsresult GetDynamicInfo(nsIURI *aChromeURL, PRBool aIsOverlay, nsISimpleEnumerator **aResult);

  nsresult LoadInstallDataSource();
  nsresult LoadProfileDataSource();

  void FlushSkinCaches();
  void FlushAllCaches();

private:
  static nsresult RefreshWindow(nsIDOMWindowInternal* aWindow,
                                nsICSSLoader* aCSSLoader);
  static nsresult GetProviderAndPath(nsIURL* aChromeURL,
                                     nsACString& aProvider, nsACString& aPath);

#ifdef MOZ_XUL
  NS_HIDDEN_(void) ProcessProvider(PRFileDesc *fd, nsIRDFService* aRDFs,
                                   nsIRDFDataSource* ds, nsIRDFResource* aRoot,
                                   PRBool aIsLocale, const nsACString& aBaseURL);
  NS_HIDDEN_(void) ProcessOverlays(PRFileDesc *fd, nsIRDFDataSource* ds,
                                   nsIRDFResource* aRoot,
                                   const nsCSubstring& aType);
#endif

  NS_HIDDEN_(nsresult) ProcessManifest(nsILocalFile* aManifest, PRBool aSkinOnly);
  NS_HIDDEN_(nsresult) ProcessManifestBuffer(char *aBuffer, PRInt32 aLength, nsILocalFile* aManifest, PRBool aSkinOnly);
  NS_HIDDEN_(nsresult) ProcessNewChromeFile(nsILocalFile *aListFile, nsIURI* aManifest);
  NS_HIDDEN_(nsresult) ProcessNewChromeBuffer(char *aBuffer, PRInt32 aLength, nsIURI* aManifest);

public:
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
    void    EnumerateToArray(nsCStringArray *a);
    void    Clear();

  private:
    ProviderEntry* GetProvider(const nsACString& aPreferred, MatchType aType);

    nsVoidArray mArray;
  };

  struct PackageEntry : public PLDHashEntryHdr
  {
    PackageEntry(const nsACString& package);
    ~PackageEntry() { }

    // Available flags
    enum {
      // This is a "platform" package (e.g. chrome://global-platform/).
      // Appends one of win/ unix/ mac/ to the base URI.
      PLATFORM_PACKAGE = 1 << 0,

      // This package should use the new XPCNativeWrappers to separate
      // content from chrome. This flag is currently unused (because we call
      // into xpconnect at registration time).
      XPCNATIVEWRAPPERS = 1 << 1
    };

    nsCString        package;
    nsCOMPtr<nsIURI> baseURI;
    PRUint32         flags;
    nsProviderArray  locales;
    nsProviderArray  skins;
  };

private:
  static PLDHashNumber HashKey(PLDHashTable *table, const void *key);
  static PRBool        MatchKey(PLDHashTable *table, const PLDHashEntryHdr *entry,
                                const void *key);
  static void          ClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry);
  static PRBool        InitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                                 const void *key);

  static const PLDHashTableOps kTableOps;

public:
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

private:
  PRBool mInitialized;

  // Hash of package names ("global") to PackageEntry objects
  PLDHashTable mPackagesHash;

  // Hashes on the file to be overlaid (chrome://browser/content/browser.xul)
  // to a list of overlays/stylesheets
  OverlayListHash mOverlayHash;
  OverlayListHash mStyleHash;

  // "Override" table (chrome URI string -> real URI)
  nsInterfaceHashtable<nsURIHashKey, nsIURI> mOverrideTable;

  nsCString mSelectedLocale;
  nsCString mSelectedSkin;
};
