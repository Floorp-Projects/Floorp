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

#include "mozilla/dom/PContentParent.h"
#include "RegistryMessageUtils.h"
#include "nsResProtocolHandler.h"

#include "nsChromeRegistryChrome.h"

#if defined(XP_WIN)
#include <windows.h>
#elif defined(XP_MACOSX)
#include <CoreServices/CoreServices.h>
#elif defined(MOZ_WIDGET_GTK2)
#include <gtk/gtk.h>
#endif

#include "nsArrayEnumerator.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManager.h"
#include "nsEnumeratorUtils.h"
#include "nsNetUtil.h"
#include "nsStringEnumerator.h"
#include "nsTextFormatter.h"
#include "nsUnicharUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "nsZipArchive.h"

#include "mozilla/LookAndFeel.h"

#include "nsICommandLine.h"
#include "nsILocaleService.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsIPrefBranch2.h"
#include "nsIPrefService.h"
#include "nsIResProtocolHandler.h"
#include "nsIScriptError.h"
#include "nsIVersionComparator.h"
#include "nsIXPConnect.h"
#include "nsIXULAppInfo.h"
#include "nsIXULRuntime.h"

#define UILOCALE_CMD_LINE_ARG "UILocale"

#define MATCH_OS_LOCALE_PREF "intl.locale.matchOS"
#define SELECTED_LOCALE_PREF "general.useragent.locale"
#define SELECTED_SKIN_PREF   "general.skins.selectedSkin"

using namespace mozilla;

static PLDHashOperator
RemoveAll(PLDHashTable *table, PLDHashEntryHdr *entry, PRUint32 number, void *arg)
{
  return (PLDHashOperator) (PL_DHASH_NEXT | PL_DHASH_REMOVE);
}

// We use a "best-fit" algorithm for matching locales and themes. 
// 1) the exact selected locale/theme
// 2) (locales only) same language, different country
//    e.g. en-GB is the selected locale, only en-US is available
// 3) any available locale/theme

/**
 * Match the language-part of two lang-COUNTRY codes, hopefully but
 * not guaranteed to be in the form ab-CD or abz-CD. "ab" should also
 * work, any other garbage-in will produce undefined results as long
 * as it does not crash.
 */
static bool
LanguagesMatch(const nsACString& a, const nsACString& b)
{
  if (a.Length() < 2 || b.Length() < 2)
    return PR_FALSE;

  nsACString::const_iterator as, ae, bs, be;
  a.BeginReading(as);
  a.EndReading(ae);
  b.BeginReading(bs);
  b.EndReading(be);

  while (*as == *bs) {
    if (*as == '-')
      return PR_TRUE;
 
    ++as; ++bs;

    // reached the end
    if (as == ae && bs == be)
      return PR_TRUE;

    // "a" is short
    if (as == ae)
      return (*bs == '-');

    // "b" is short
    if (bs == be)
      return (*as == '-');
  }

  return PR_FALSE;
}

nsChromeRegistryChrome::nsChromeRegistryChrome()
  : mProfileLoaded(PR_FALSE)
{
  mPackagesHash.ops = nsnull;
}

nsChromeRegistryChrome::~nsChromeRegistryChrome()
{
  if (mPackagesHash.ops)
    PL_DHashTableFinish(&mPackagesHash);
}

nsresult
nsChromeRegistryChrome::Init()
{
  nsresult rv = nsChromeRegistry::Init();
  if (NS_FAILED(rv))
    return rv;

  if (!mOverlayHash.Init() ||
      !mStyleHash.Init())
    return NS_ERROR_FAILURE;
  
  mSelectedLocale = NS_LITERAL_CSTRING("en-US");
  mSelectedSkin = NS_LITERAL_CSTRING("classic/1.0");

  if (!PL_DHashTableInit(&mPackagesHash, &kTableOps,
                         nsnull, sizeof(PackageEntry), 16))
    return NS_ERROR_FAILURE;

  bool safeMode = false;
  nsCOMPtr<nsIXULRuntime> xulrun (do_GetService(XULAPPINFO_SERVICE_CONTRACTID));
  if (xulrun)
    xulrun->GetInSafeMode(&safeMode);
  
  nsCOMPtr<nsIPrefService> prefserv (do_GetService(NS_PREFSERVICE_CONTRACTID));
  nsCOMPtr<nsIPrefBranch> prefs;

  if (safeMode)
    prefserv->GetDefaultBranch(nsnull, getter_AddRefs(prefs));
  else
    prefs = do_QueryInterface(prefserv);

  if (!prefs) {
    NS_WARNING("Could not get pref service!");
  }
  else {
    nsXPIDLCString provider;
    rv = prefs->GetCharPref(SELECTED_SKIN_PREF, getter_Copies(provider));
    if (NS_SUCCEEDED(rv))
      mSelectedSkin = provider;

    SelectLocaleFromPref(prefs);

    nsCOMPtr<nsIPrefBranch2> prefs2 (do_QueryInterface(prefs));
    if (prefs2) {
      rv = prefs2->AddObserver(MATCH_OS_LOCALE_PREF, this, PR_TRUE);
      rv = prefs2->AddObserver(SELECTED_LOCALE_PREF, this, PR_TRUE);
      rv = prefs2->AddObserver(SELECTED_SKIN_PREF, this, PR_TRUE);
    }
  }

  nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
  if (obsService) {
    obsService->AddObserver(this, "command-line-startup", PR_TRUE);
    obsService->AddObserver(this, "profile-initial-state", PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistryChrome::CheckForOSAccessibility()
{
  PRInt32 useAccessibilityTheme =
    LookAndFeel::GetInt(LookAndFeel::eIntID_UseAccessibilityTheme, 0);

  if (useAccessibilityTheme) {
    /* Set the skin to classic and remove pref observers */
    if (!mSelectedSkin.EqualsLiteral("classic/1.0")) {
      mSelectedSkin.AssignLiteral("classic/1.0");
      RefreshSkins();
    }

    nsCOMPtr<nsIPrefBranch2> prefs (do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefs) {
      prefs->RemoveObserver(SELECTED_SKIN_PREF, this);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistryChrome::GetLocalesForPackage(const nsACString& aPackage,
                                       nsIUTF8StringEnumerator* *aResult)
{
  nsTArray<nsCString> *a = new nsTArray<nsCString>;
  if (!a)
    return NS_ERROR_OUT_OF_MEMORY;

  PackageEntry* entry =
      static_cast<PackageEntry*>(PL_DHashTableOperate(&mPackagesHash,
                                                      & aPackage,
                                                      PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
    entry->locales.EnumerateToArray(a);
  }

  nsresult rv = NS_NewAdoptingUTF8StringEnumerator(aResult, a);
  if (NS_FAILED(rv))
    delete a;

  return rv;
}

static nsresult
getUILangCountry(nsACString& aUILang)
{
  nsresult rv;

  nsCOMPtr<nsILocaleService> localeService = do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString uiLang;
  rv = localeService->GetLocaleComponentForUserAgent(uiLang);
  NS_ENSURE_SUCCESS(rv, rv);

  CopyUTF16toUTF8(uiLang, aUILang);
  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistryChrome::IsLocaleRTL(const nsACString& package, bool *aResult)
{
  *aResult = PR_FALSE;

  nsCAutoString locale;
  GetSelectedLocale(package, locale);
  if (locale.Length() < 2)
    return NS_OK;

  // first check the intl.uidirection.<locale> preference, and if that is not
  // set, check the same preference but with just the first two characters of
  // the locale. If that isn't set, default to left-to-right.
  nsCAutoString prefString = NS_LITERAL_CSTRING("intl.uidirection.") + locale;
  nsCOMPtr<nsIPrefBranch> prefBranch (do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (!prefBranch)
    return NS_OK;
  
  nsXPIDLCString dir;
  prefBranch->GetCharPref(prefString.get(), getter_Copies(dir));
  if (dir.IsEmpty()) {
    PRInt32 hyphen = prefString.FindChar('-');
    if (hyphen >= 1) {
      nsCAutoString shortPref(Substring(prefString, 0, hyphen));
      prefBranch->GetCharPref(shortPref.get(), getter_Copies(dir));
    }
  }
  *aResult = dir.EqualsLiteral("rtl");
  return NS_OK;
}

nsresult
nsChromeRegistryChrome::GetSelectedLocale(const nsACString& aPackage,
                                          nsACString& aLocale)
{
  PackageEntry* entry =
      static_cast<PackageEntry*>(PL_DHashTableOperate(&mPackagesHash,
                                                      & aPackage,
                                                      PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_FREE(entry))
    return NS_ERROR_FAILURE;

  aLocale = entry->locales.GetSelected(mSelectedLocale, nsProviderArray::LOCALE);
  if (aLocale.IsEmpty())
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
nsChromeRegistryChrome::SelectLocaleFromPref(nsIPrefBranch* prefs)
{
  nsresult rv;
  bool matchOSLocale = false;
  rv = prefs->GetBoolPref(MATCH_OS_LOCALE_PREF, &matchOSLocale);

  if (NS_SUCCEEDED(rv) && matchOSLocale) {
    // compute lang and region code only when needed!
    nsCAutoString uiLocale;
    rv = getUILangCountry(uiLocale);
    if (NS_SUCCEEDED(rv))
      mSelectedLocale = uiLocale;
  }
  else {
    nsXPIDLCString provider;
    rv = prefs->GetCharPref(SELECTED_LOCALE_PREF, getter_Copies(provider));
    if (NS_SUCCEEDED(rv)) {
      mSelectedLocale = provider;
    }
  }

  if (NS_FAILED(rv))
    NS_ERROR("Couldn't select locale from pref!");

  return rv;
}

NS_IMETHODIMP
nsChromeRegistryChrome::Observe(nsISupports *aSubject, const char *aTopic,
                                const PRUnichar *someData)
{
  nsresult rv = NS_OK;

  if (!strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic)) {
    nsCOMPtr<nsIPrefBranch> prefs (do_QueryInterface(aSubject));
    NS_ASSERTION(prefs, "Bad observer call!");

    NS_ConvertUTF16toUTF8 pref(someData);

    if (pref.EqualsLiteral(MATCH_OS_LOCALE_PREF) ||
        pref.EqualsLiteral(SELECTED_LOCALE_PREF)) {
        rv = UpdateSelectedLocale();
        if (NS_SUCCEEDED(rv) && mProfileLoaded)
          FlushAllCaches();
    }
    else if (pref.EqualsLiteral(SELECTED_SKIN_PREF)) {
      nsXPIDLCString provider;
      rv = prefs->GetCharPref(pref.get(), getter_Copies(provider));
      if (NS_FAILED(rv)) {
        NS_ERROR("Couldn't get new skin pref!");
        return rv;
      }

      mSelectedSkin = provider;
      RefreshSkins();
    } else {
      NS_ERROR("Unexpected pref!");
    }
  }
  else if (!strcmp("command-line-startup", aTopic)) {
    nsCOMPtr<nsICommandLine> cmdLine (do_QueryInterface(aSubject));
    if (cmdLine) {
      nsAutoString uiLocale;
      rv = cmdLine->HandleFlagWithParam(NS_LITERAL_STRING(UILOCALE_CMD_LINE_ARG),
                                        PR_FALSE, uiLocale);
      if (NS_SUCCEEDED(rv) && !uiLocale.IsEmpty()) {
        CopyUTF16toUTF8(uiLocale, mSelectedLocale);
        nsCOMPtr<nsIPrefBranch2> prefs (do_GetService(NS_PREFSERVICE_CONTRACTID));
        if (prefs) {
          prefs->RemoveObserver(SELECTED_LOCALE_PREF, this);
        }
      }
    }
  }
  else if (!strcmp("profile-initial-state", aTopic)) {
    mProfileLoaded = PR_TRUE;
  }
  else {
    NS_ERROR("Unexpected observer topic!");
  }

  return rv;
}

NS_IMETHODIMP
nsChromeRegistryChrome::CheckForNewChrome()
{
  PL_DHashTableEnumerate(&mPackagesHash, RemoveAll, nsnull);
  mOverlayHash.Clear();
  mStyleHash.Clear();
  mOverrideTable.Clear();

  nsComponentManagerImpl::gComponentManager->RereadChromeManifests();
  return NS_OK;
}

nsresult nsChromeRegistryChrome::UpdateSelectedLocale()
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefs) {
    rv = SelectLocaleFromPref(prefs);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
      NS_ASSERTION(obsSvc, "Couldn't get observer service.");
      obsSvc->NotifyObservers((nsIChromeRegistry*) this,
                              "selected-locale-has-changed", nsnull);
    }
  }

  return rv;
}

static void
SerializeURI(nsIURI* aURI,
             SerializedURI& aSerializedURI)
{
  if (!aURI)
    return;

  aURI->GetSpec(aSerializedURI.spec);
  aURI->GetOriginCharset(aSerializedURI.charset);
}

static PLDHashOperator
EnumerateOverride(nsIURI* aURIKey,
                  nsIURI* aURI,
                  void* aArg)
{
  nsTArray<OverrideMapping>* overrides =
      static_cast<nsTArray<OverrideMapping>*>(aArg);

  SerializedURI chromeURI, overrideURI;

  SerializeURI(aURIKey, chromeURI);
  SerializeURI(aURI, overrideURI);
        
  OverrideMapping override = {
    chromeURI, overrideURI
  };
  overrides->AppendElement(override);
  return (PLDHashOperator)PL_DHASH_NEXT;
}

struct EnumerationArgs
{
  InfallibleTArray<ChromePackage>& packages;
  const nsCString& selectedLocale;
  const nsCString& selectedSkin;
};

void
nsChromeRegistryChrome::SendRegisteredChrome(
    mozilla::dom::PContentParent* aParent)
{
  InfallibleTArray<ChromePackage> packages;
  InfallibleTArray<ResourceMapping> resources;
  InfallibleTArray<OverrideMapping> overrides;

  EnumerationArgs args = {
    packages, mSelectedLocale, mSelectedSkin
  };
  PL_DHashTableEnumerate(&mPackagesHash, CollectPackages, &args);

  nsCOMPtr<nsIIOService> io (do_GetIOService());
  NS_ENSURE_TRUE(io, );

  nsCOMPtr<nsIProtocolHandler> ph;
  nsresult rv = io->GetProtocolHandler("resource", getter_AddRefs(ph));
  NS_ENSURE_SUCCESS(rv, );

  //FIXME: Some substitutions are set up lazily and might not exist yet
  nsCOMPtr<nsIResProtocolHandler> irph (do_QueryInterface(ph));
  nsResProtocolHandler* rph = static_cast<nsResProtocolHandler*>(irph.get());
  rph->CollectSubstitutions(resources);

  mOverrideTable.EnumerateRead(&EnumerateOverride, &overrides);

  bool success = aParent->SendRegisterChrome(packages, resources, overrides,
                                             mSelectedLocale);
  NS_ENSURE_TRUE(success, );
}

PLDHashOperator
nsChromeRegistryChrome::CollectPackages(PLDHashTable *table,
                                  PLDHashEntryHdr *entry,
                                  PRUint32 number,
                                  void *arg)
{
  EnumerationArgs* args = static_cast<EnumerationArgs*>(arg);
  PackageEntry* package = static_cast<PackageEntry*>(entry);

  SerializedURI contentURI, localeURI, skinURI;

  SerializeURI(package->baseURI, contentURI);
  SerializeURI(package->locales.GetBase(args->selectedLocale,
                                        nsProviderArray::LOCALE), localeURI);
  SerializeURI(package->skins.GetBase(args->selectedSkin, nsProviderArray::ANY),
               skinURI);
  
  ChromePackage chromePackage = {
    package->package,
    contentURI,
    localeURI,
    skinURI,
    package->flags
  };
  args->packages.AppendElement(chromePackage);
  return (PLDHashOperator)PL_DHASH_NEXT;
}

static bool
CanLoadResource(nsIURI* aResourceURI)
{
  bool isLocalResource = false;
  (void)NS_URIChainHasFlags(aResourceURI,
                            nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                            &isLocalResource);
  return isLocalResource;
}

nsIURI*
nsChromeRegistryChrome::GetBaseURIFromPackage(const nsCString& aPackage,
                                              const nsCString& aProvider,
                                              const nsCString& aPath)
{
  PackageEntry* entry =
      static_cast<PackageEntry*>(PL_DHashTableOperate(&mPackagesHash,
                                                      &aPackage,
                                                      PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_FREE(entry)) {
    if (!mInitialized)
      return nsnull;

    LogMessage("No chrome package registered for chrome://%s/%s/%s",
               aPackage.get(), aProvider.get(), aPath.get());

    return nsnull;
  }

  if (aProvider.EqualsLiteral("locale")) {
    return entry->locales.GetBase(mSelectedLocale, nsProviderArray::LOCALE);
  }
  else if (aProvider.EqualsLiteral("skin")) {
    return entry->skins.GetBase(mSelectedSkin, nsProviderArray::ANY);
  }
  else if (aProvider.EqualsLiteral("content")) {
    return entry->baseURI;
  }
  return nsnull;
}

nsresult
nsChromeRegistryChrome::GetFlagsFromPackage(const nsCString& aPackage,
                                            PRUint32* aFlags)
{
  PackageEntry* entry =
      static_cast<PackageEntry*>(PL_DHashTableOperate(&mPackagesHash,
                                                      & (nsACString&) aPackage,
                                                      PL_DHASH_LOOKUP));
  if (PL_DHASH_ENTRY_IS_FREE(entry))
    return NS_ERROR_NOT_AVAILABLE;

  *aFlags = entry->flags;
  return NS_OK;
}

PLHashNumber
nsChromeRegistryChrome::HashKey(PLDHashTable *table, const void *key)
{
  const nsACString& str = *reinterpret_cast<const nsACString*>(key);
  return HashString(str);
}

bool
nsChromeRegistryChrome::MatchKey(PLDHashTable *table, const PLDHashEntryHdr *entry,
                           const void *key)
{
  const nsACString& str = *reinterpret_cast<const nsACString*>(key);
  const PackageEntry* pentry = static_cast<const PackageEntry*>(entry);
  return str.Equals(pentry->package);
}

void
nsChromeRegistryChrome::ClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  PackageEntry* pentry = static_cast<PackageEntry*>(entry);
  pentry->~PackageEntry();
}

bool
nsChromeRegistryChrome::InitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                            const void *key)
{
  const nsACString& str = *reinterpret_cast<const nsACString*>(key);

  new (entry) PackageEntry(str);
  return PR_TRUE;
}

const PLDHashTableOps
nsChromeRegistryChrome::kTableOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  HashKey,
  MatchKey,
  PL_DHashMoveEntryStub,
  ClearEntry,
  PL_DHashFinalizeStub,
  InitEntry
};

nsChromeRegistryChrome::ProviderEntry*
nsChromeRegistryChrome::nsProviderArray::GetProvider(const nsACString& aPreferred, MatchType aType)
{
  PRInt32 i = mArray.Count();
  if (!i)
    return nsnull;

  ProviderEntry* found = nsnull;  // Only set if we find a partial-match locale
  ProviderEntry* entry;

  while (i--) {
    entry = reinterpret_cast<ProviderEntry*>(mArray[i]);
    if (aPreferred.Equals(entry->provider))
      return entry;

    if (aType != LOCALE)
      continue;

    if (LanguagesMatch(aPreferred, entry->provider)) {
      found = entry;
      continue;
    }

    if (!found && entry->provider.EqualsLiteral("en-US"))
      found = entry;
  }

  if (!found && aType != EXACT)
    return entry;

  return found;
}

nsIURI*
nsChromeRegistryChrome::nsProviderArray::GetBase(const nsACString& aPreferred, MatchType aType)
{
  ProviderEntry* provider = GetProvider(aPreferred, aType);

  if (!provider)
    return nsnull;

  return provider->baseURI;
}

const nsACString&
nsChromeRegistryChrome::nsProviderArray::GetSelected(const nsACString& aPreferred, MatchType aType)
{
  ProviderEntry* entry = GetProvider(aPreferred, aType);

  if (entry)
    return entry->provider;

  return EmptyCString();
}

void
nsChromeRegistryChrome::nsProviderArray::SetBase(const nsACString& aProvider, nsIURI* aBaseURL)
{
  ProviderEntry* provider = GetProvider(aProvider, EXACT);

  if (provider) {
    provider->baseURI = aBaseURL;
    return;
  }

  // no existing entries, add a new one
  provider = new ProviderEntry(aProvider, aBaseURL);
  if (!provider)
    return; // It's safe to silently fail on OOM

  mArray.AppendElement(provider);
}

void
nsChromeRegistryChrome::nsProviderArray::EnumerateToArray(nsTArray<nsCString> *a)
{
  PRInt32 i = mArray.Count();
  while (i--) {
    ProviderEntry *entry = reinterpret_cast<ProviderEntry*>(mArray[i]);
    a->AppendElement(entry->provider);
  }
}

void
nsChromeRegistryChrome::nsProviderArray::Clear()
{
  PRInt32 i = mArray.Count();
  while (i--) {
    ProviderEntry* entry = reinterpret_cast<ProviderEntry*>(mArray[i]);
    delete entry;
  }

  mArray.Clear();
}

void
nsChromeRegistryChrome::OverlayListEntry::AddURI(nsIURI* aURI)
{
  PRInt32 i = mArray.Count();
  while (i--) {
    bool equals;
    if (NS_SUCCEEDED(aURI->Equals(mArray[i], &equals)) && equals)
      return;
  }

  mArray.AppendObject(aURI);
}

void
nsChromeRegistryChrome::OverlayListHash::Add(nsIURI* aBase, nsIURI* aOverlay)
{
  OverlayListEntry* entry = mTable.PutEntry(aBase);
  if (entry)
    entry->AddURI(aOverlay);
}

const nsCOMArray<nsIURI>*
nsChromeRegistryChrome::OverlayListHash::GetArray(nsIURI* aBase)
{
  OverlayListEntry* entry = mTable.GetEntry(aBase);
  if (!entry)
    return nsnull;

  return &entry->mArray;
}

#ifdef MOZ_XUL
NS_IMETHODIMP
nsChromeRegistryChrome::GetStyleOverlays(nsIURI *aChromeURL,
                                         nsISimpleEnumerator **aResult)
{
  const nsCOMArray<nsIURI>* parray = mStyleHash.GetArray(aChromeURL);
  if (!parray)
    return NS_NewEmptyEnumerator(aResult);

  return NS_NewArrayEnumerator(aResult, *parray);
}

NS_IMETHODIMP
nsChromeRegistryChrome::GetXULOverlays(nsIURI *aChromeURL,
                                       nsISimpleEnumerator **aResult)
{
  const nsCOMArray<nsIURI>* parray = mOverlayHash.GetArray(aChromeURL);
  if (!parray)
    return NS_NewEmptyEnumerator(aResult);

  return NS_NewArrayEnumerator(aResult, *parray);
}
#endif // MOZ_XUL

nsIURI*
nsChromeRegistry::ManifestProcessingContext::GetManifestURI()
{
  if (!mManifestURI) {
    nsCOMPtr<nsIIOService> io = mozilla::services::GetIOService();
    if (!io) {
      NS_WARNING("No IO service trying to process chrome manifests");
      return NULL;
    }

    if (mPath) {
      nsCOMPtr<nsIURI> fileURI;
      io->NewFileURI(mFile, getter_AddRefs(fileURI));

      nsCAutoString spec;
      fileURI->GetSpec(spec);
      spec.Insert(NS_LITERAL_CSTRING("jar:"), 0);
      spec.AppendLiteral("!/");
      spec.Append(mPath);

      NS_NewURI(getter_AddRefs(mManifestURI), spec, NULL, NULL, io);
    }
    else {
      io->NewFileURI(mFile, getter_AddRefs(mManifestURI));
    }
  }
  return mManifestURI;
}

nsIXPConnect*
nsChromeRegistry::ManifestProcessingContext::GetXPConnect()
{
  if (!mXPConnect)
    mXPConnect = do_GetService("@mozilla.org/js/xpc/XPConnect;1");

  return mXPConnect;
}

already_AddRefed<nsIURI>
nsChromeRegistry::ManifestProcessingContext::ResolveURI(const char* uri)
{
  nsIURI* baseuri = GetManifestURI();
  if (!baseuri)
    return NULL;

  nsCOMPtr<nsIURI> resolved;
  nsresult rv = NS_NewURI(getter_AddRefs(resolved), uri, baseuri);
  if (NS_FAILED(rv))
    return NULL;

  return resolved.forget();
}

static void
EnsureLowerCase(char *aBuf)
{
  for (; *aBuf; ++aBuf) {
    char ch = *aBuf;
    if (ch >= 'A' && ch <= 'Z')
      *aBuf = ch + 'a' - 'A';
  }
}

void
nsChromeRegistryChrome::ManifestContent(ManifestProcessingContext& cx, int lineno,
                                        char *const * argv, bool platform,
                                        bool contentaccessible)
{
  char* package = argv[0];
  char* uri = argv[1];

  EnsureLowerCase(package);

  nsCOMPtr<nsIURI> resolved = cx.ResolveURI(uri);
  if (!resolved) {
    LogMessageWithContext(cx.GetManifestURI(), lineno, nsIScriptError::warningFlag,
                          "During chrome registration, unable to create URI '%s'.", uri);
    return;
  }

  if (!CanLoadResource(resolved)) {
    LogMessageWithContext(resolved, lineno, nsIScriptError::warningFlag,
                          "During chrome registration, cannot register non-local URI '%s' as content.",
                          uri);
    return;
  }

  PackageEntry* entry =
    static_cast<PackageEntry*>(PL_DHashTableOperate(&mPackagesHash,
                                                    & (const nsACString&) nsDependentCString(package),
                                                    PL_DHASH_ADD));
  if (!entry)
    return;

  entry->baseURI = resolved;

  if (platform)
    entry->flags |= PLATFORM_PACKAGE;
  if (contentaccessible)
    entry->flags |= CONTENT_ACCESSIBLE;
}

void
nsChromeRegistryChrome::ManifestLocale(ManifestProcessingContext& cx, int lineno,
                                       char *const * argv, bool platform,
                                       bool contentaccessible)
{
  char* package = argv[0];
  char* provider = argv[1];
  char* uri = argv[2];

  EnsureLowerCase(package);

  nsCOMPtr<nsIURI> resolved = cx.ResolveURI(uri);
  if (!resolved) {
    LogMessageWithContext(cx.GetManifestURI(), lineno, nsIScriptError::warningFlag,
                          "During chrome registration, unable to create URI '%s'.", uri);
    return;
  }

  if (!CanLoadResource(resolved)) {
    LogMessageWithContext(resolved, lineno, nsIScriptError::warningFlag,
                          "During chrome registration, cannot register non-local URI '%s' as content.",
                          uri);
    return;
  }

  PackageEntry* entry =
    static_cast<PackageEntry*>(PL_DHashTableOperate(&mPackagesHash,
                                                    & (const nsACString&) nsDependentCString(package),
                                                    PL_DHASH_ADD));
  if (!entry)
    return;

  entry->locales.SetBase(nsDependentCString(provider), resolved);
}

void
nsChromeRegistryChrome::ManifestSkin(ManifestProcessingContext& cx, int lineno,
                                     char *const * argv, bool platform,
                                     bool contentaccessible)
{
  char* package = argv[0];
  char* provider = argv[1];
  char* uri = argv[2];

  EnsureLowerCase(package);

  nsCOMPtr<nsIURI> resolved = cx.ResolveURI(uri);
  if (!resolved) {
    LogMessageWithContext(cx.GetManifestURI(), lineno, nsIScriptError::warningFlag,
                          "During chrome registration, unable to create URI '%s'.", uri);
    return;
  }

  if (!CanLoadResource(resolved)) {
    LogMessageWithContext(resolved, lineno, nsIScriptError::warningFlag,
                          "During chrome registration, cannot register non-local URI '%s' as content.",
                          uri);
    return;
  }

  PackageEntry* entry =
    static_cast<PackageEntry*>(PL_DHashTableOperate(&mPackagesHash,
                                                    & (const nsACString&) nsDependentCString(package),
                                                    PL_DHASH_ADD));
  if (!entry)
    return;

  entry->skins.SetBase(nsDependentCString(provider), resolved);
}

void
nsChromeRegistryChrome::ManifestOverlay(ManifestProcessingContext& cx, int lineno,
                                        char *const * argv, bool platform,
                                        bool contentaccessible)
{
  char* base = argv[0];
  char* overlay = argv[1];

  nsCOMPtr<nsIURI> baseuri = cx.ResolveURI(base);
  nsCOMPtr<nsIURI> overlayuri = cx.ResolveURI(overlay);
  if (!baseuri || !overlayuri) {
    LogMessageWithContext(cx.GetManifestURI(), lineno, nsIScriptError::warningFlag,
                          "During chrome registration, unable to create URI.");
    return;
  }

  if (!CanLoadResource(overlayuri)) {
    LogMessageWithContext(cx.GetManifestURI(), lineno, nsIScriptError::warningFlag,
                          "Cannot register non-local URI '%s' as an overlay.", overlay);
    return;
  }

  mOverlayHash.Add(baseuri, overlayuri);
}

void
nsChromeRegistryChrome::ManifestStyle(ManifestProcessingContext& cx, int lineno,
                                      char *const * argv, bool platform,
                                      bool contentaccessible)
{
  char* base = argv[0];
  char* overlay = argv[1];

  nsCOMPtr<nsIURI> baseuri = cx.ResolveURI(base);
  nsCOMPtr<nsIURI> overlayuri = cx.ResolveURI(overlay);
  if (!baseuri || !overlayuri) {
    LogMessageWithContext(cx.GetManifestURI(), lineno, nsIScriptError::warningFlag,
                          "During chrome registration, unable to create URI.");
    return;
  }

  if (!CanLoadResource(overlayuri)) {
    LogMessageWithContext(cx.GetManifestURI(), lineno, nsIScriptError::warningFlag,
                          "Cannot register non-local URI '%s' as a style overlay.", overlay);
    return;
  }

  mStyleHash.Add(baseuri, overlayuri);
}

void
nsChromeRegistryChrome::ManifestOverride(ManifestProcessingContext& cx, int lineno,
                                         char *const * argv, bool platform,
                                         bool contentaccessible)
{
  char* chrome = argv[0];
  char* resolved = argv[1];

  nsCOMPtr<nsIURI> chromeuri = cx.ResolveURI(chrome);
  nsCOMPtr<nsIURI> resolveduri = cx.ResolveURI(resolved);
  if (!chromeuri || !resolveduri) {
    LogMessageWithContext(cx.GetManifestURI(), lineno, nsIScriptError::warningFlag,
                          "During chrome registration, unable to create URI.");
    return;
  }

  if (!CanLoadResource(resolveduri)) {
    LogMessageWithContext(cx.GetManifestURI(), lineno, nsIScriptError::warningFlag,
                          "Cannot register non-local URI '%s' for an override.", resolved);
    return;
  }
  mOverrideTable.Put(chromeuri, resolveduri);
}

void
nsChromeRegistryChrome::ManifestResource(ManifestProcessingContext& cx, int lineno,
                                         char *const * argv, bool platform,
                                         bool contentaccessible)
{
  char* package = argv[0];
  char* uri = argv[1];

  EnsureLowerCase(package);
  nsDependentCString host(package);

  nsCOMPtr<nsIIOService> io = mozilla::services::GetIOService();
  if (!io) {
    NS_WARNING("No IO service trying to process chrome manifests");
    return;
  }

  nsCOMPtr<nsIProtocolHandler> ph;
  nsresult rv = io->GetProtocolHandler("resource", getter_AddRefs(ph));
  if (NS_FAILED(rv))
    return;
  
  nsCOMPtr<nsIResProtocolHandler> rph = do_QueryInterface(ph);

  bool exists = false;
  rv = rph->HasSubstitution(host, &exists);
  if (exists) {
    LogMessageWithContext(cx.GetManifestURI(), lineno, nsIScriptError::warningFlag,
                          "Duplicate resource declaration for '%s' ignored.", package);
    return;
  }

  nsCOMPtr<nsIURI> resolved = cx.ResolveURI(uri);
  if (!resolved) {
    LogMessageWithContext(cx.GetManifestURI(), lineno, nsIScriptError::warningFlag,
                          "During chrome registration, unable to create URI '%s'.", uri);
    return;
  }

  if (!CanLoadResource(resolved)) {
    LogMessageWithContext(cx.GetManifestURI(), lineno, nsIScriptError::warningFlag,
                          "Warning: cannot register non-local URI '%s' as a resource.",
                          uri);
    return;
  }

  rph->SetSubstitution(host, resolved);
}
