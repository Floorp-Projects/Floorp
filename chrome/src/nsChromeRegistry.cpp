/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
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
 *   Gagan Saksena <gagan@netscape.com>
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

#include "nsChromeRegistry.h"

#include <string.h>

#include "prio.h"
#include "prprf.h"
#if defined(XP_WIN)
#include <windows.h>
#elif defined(XP_MACOSX)
#include <Carbon/Carbon.h>
#elif defined(MOZ_WIDGET_GTK2)
#include <gtk/gtk.h>
#endif

#include "nsAppDirectoryServiceDefs.h"
#include "nsArrayEnumerator.h"
#include "nsStringEnumerator.h"
#include "nsEnumeratorUtils.h"
#include "nsCOMPtr.h"
#include "nsDOMError.h"
#include "nsEscape.h"
#include "nsInt64.h"
#include "nsLayoutCID.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsStaticAtom.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsWidgetsCID.h"
#include "nsXPCOMCIDInternal.h"
#include "nsXPIDLString.h"
#include "nsXULAppAPI.h"
#include "nsTextFormatter.h"

#include "nsIAtom.h"
#include "nsICommandLine.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsIConsoleService.h"
#include "nsIDirectoryService.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDocShell.h"
#include "nsIDocumentObserver.h"
#include "nsIDOMElement.h"
#include "nsIDOMLocation.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDOMWindowInternal.h"
#include "nsIFileChannel.h"
#include "nsIFileURL.h"
#include "nsIIOService.h"
#include "nsIJARProtocolHandler.h"
#include "nsIJARURI.h"
#include "nsILocalFile.h"
#include "nsILocaleService.h"
#include "nsILookAndFeel.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"
#include "nsIPresShell.h"
#include "nsIProtocolHandler.h"
#include "nsIResProtocolHandler.h"
#include "nsIScriptError.h"
#include "nsIServiceManager.h"
#include "nsISimpleEnumerator.h"
#include "nsIStyleSheet.h"
#include "nsISupportsArray.h"
#include "nsIVersionComparator.h"
#include "nsIWindowMediator.h"
#include "nsIXPConnect.h"
#include "nsIXULAppInfo.h"
#include "nsIXULRuntime.h"

#define UILOCALE_CMD_LINE_ARG "UILocale"

#define MATCH_OS_LOCALE_PREF "intl.locale.matchOS"
#define SELECTED_LOCALE_PREF "general.useragent.locale"
#define SELECTED_SKIN_PREF   "general.skins.selectedSkin"

static NS_DEFINE_CID(kCSSLoaderCID, NS_CSS_LOADER_CID);
static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

nsChromeRegistry* nsChromeRegistry::gChromeRegistry;

////////////////////////////////////////////////////////////////////////////////

static void
LogMessage(const char* aMsg, ...)
{
  nsCOMPtr<nsIConsoleService> console 
    (do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!console)
    return;

  va_list args;
  va_start(args, aMsg);
  char* formatted = PR_vsmprintf(aMsg, args);
  va_end(args);
  if (!formatted)
    return;

  console->LogStringMessage(NS_ConvertUTF8toUTF16(formatted).get());
  PR_smprintf_free(formatted);
}

static void
LogMessageWithContext(nsIURI* aURL, PRUint32 aLineNumber, PRUint32 flags,
                      const char* aMsg, ...)
{
  nsresult rv;

  nsCOMPtr<nsIConsoleService> console 
    (do_GetService(NS_CONSOLESERVICE_CONTRACTID));

  nsCOMPtr<nsIScriptError> error
    (do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
  if (!console || !error)
    return;

  va_list args;
  va_start(args, aMsg);
  char* formatted = PR_vsmprintf(aMsg, args);
  va_end(args);
  if (!formatted)
    return;

  nsCString spec;
  if (aURL)
    aURL->GetSpec(spec);

  rv = error->Init(NS_ConvertUTF8toUTF16(formatted).get(),
                   NS_ConvertUTF8toUTF16(spec).get(),
                   nsnull,
                   aLineNumber, 0, flags, "chrome registration");
  PR_smprintf_free(formatted);

  if (NS_FAILED(rv))
    return;

  console->LogMessage(error);
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
static PRBool
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

static PRBool
CanLoadResource(nsIURI* aResourceURI)
{
  PRBool isLocalResource = PR_FALSE;
  (void)NS_URIChainHasFlags(aResourceURI,
                            nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                            &isLocalResource);
  return isLocalResource;
}

nsChromeRegistry::ProviderEntry*
nsChromeRegistry::nsProviderArray::GetProvider(const nsACString& aPreferred, MatchType aType)
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
nsChromeRegistry::nsProviderArray::GetBase(const nsACString& aPreferred, MatchType aType)
{
  ProviderEntry* provider = GetProvider(aPreferred, aType);

  if (!provider)
    return nsnull;

  return provider->baseURI;
}

const nsACString&
nsChromeRegistry::nsProviderArray::GetSelected(const nsACString& aPreferred, MatchType aType)
{
  ProviderEntry* entry = GetProvider(aPreferred, aType);

  if (entry)
    return entry->provider;

  return EmptyCString();
}

void
nsChromeRegistry::nsProviderArray::SetBase(const nsACString& aProvider, nsIURI* aBaseURL)
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
nsChromeRegistry::nsProviderArray::EnumerateToArray(nsTArray<nsCString> *a)
{
  PRInt32 i = mArray.Count();
  while (i--) {
    ProviderEntry *entry = reinterpret_cast<ProviderEntry*>(mArray[i]);
    a->AppendElement(entry->provider);
  }
}

void
nsChromeRegistry::nsProviderArray::Clear()
{
  PRInt32 i = mArray.Count();
  while (i--) {
    ProviderEntry* entry = reinterpret_cast<ProviderEntry*>(mArray[i]);
    delete entry;
  }

  mArray.Clear();
}

nsChromeRegistry::PackageEntry::PackageEntry(const nsACString& aPackage) :
  package(aPackage), flags(0)
{
}

PLHashNumber
nsChromeRegistry::HashKey(PLDHashTable *table, const void *key)
{
  const nsACString& str = *reinterpret_cast<const nsACString*>(key);
  return HashString(str);
}

PRBool
nsChromeRegistry::MatchKey(PLDHashTable *table, const PLDHashEntryHdr *entry,
                           const void *key)
{
  const nsACString& str = *reinterpret_cast<const nsACString*>(key);
  const PackageEntry* pentry = static_cast<const PackageEntry*>(entry);
  return str.Equals(pentry->package);
}

void
nsChromeRegistry::ClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  PackageEntry* pentry = static_cast<PackageEntry*>(entry);
  pentry->~PackageEntry();
}

PRBool
nsChromeRegistry::InitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                            const void *key)
{
  const nsACString& str = *reinterpret_cast<const nsACString*>(key);

  new (entry) PackageEntry(str);
  return PR_TRUE;
}

const PLDHashTableOps
nsChromeRegistry::kTableOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  HashKey,
  MatchKey,
  PL_DHashMoveEntryStub,
  ClearEntry,
  PL_DHashFinalizeStub,
  InitEntry
};

void
nsChromeRegistry::OverlayListEntry::AddURI(nsIURI* aURI)
{
  PRInt32 i = mArray.Count();
  while (i--) {
    PRBool equals;
    if (NS_SUCCEEDED(aURI->Equals(mArray[i], &equals)) && equals)
        return;
  }

  mArray.AppendObject(aURI);
}

void
nsChromeRegistry::OverlayListHash::Add(nsIURI* aBase, nsIURI* aOverlay)
{
  OverlayListEntry* entry = mTable.PutEntry(aBase);
  if (entry)
    entry->AddURI(aOverlay);
}

const nsCOMArray<nsIURI>*
nsChromeRegistry::OverlayListHash::GetArray(nsIURI* aBase)
{
  OverlayListEntry* entry = mTable.GetEntry(aBase);
  if (!entry)
    return nsnull;

  return &entry->mArray;
}

nsChromeRegistry::~nsChromeRegistry()
{
  if (mPackagesHash.ops)
    PL_DHashTableFinish(&mPackagesHash);
  gChromeRegistry = nsnull;
}

NS_INTERFACE_MAP_BEGIN(nsChromeRegistry)
  NS_INTERFACE_MAP_ENTRY(nsIChromeRegistry)
  NS_INTERFACE_MAP_ENTRY(nsIXULChromeRegistry)
  NS_INTERFACE_MAP_ENTRY(nsIToolkitChromeRegistry)
#ifdef MOZ_XUL
  NS_INTERFACE_MAP_ENTRY(nsIXULOverlayProvider)
#endif
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIChromeRegistry)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsChromeRegistry)
NS_IMPL_RELEASE(nsChromeRegistry)

////////////////////////////////////////////////////////////////////////////////
// nsIChromeRegistry methods:

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

nsresult
nsChromeRegistry::Init()
{
  nsresult rv;

  // Check to see if necko and the JAR protocol handler are registered yet
  // if not, somebody is doing work during XPCOM registration that they
  // shouldn't be doing. See bug 292549, where JS components are trying
  // to call Components.utils.import("chrome:///") early in registration

  nsCOMPtr<nsIIOService> io (do_GetIOService());
  if (!io) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIProtocolHandler> ph;
  rv = io->GetProtocolHandler("jar", getter_AddRefs(ph));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIJARProtocolHandler> jph = do_QueryInterface(ph);
  if (!jph)
    return NS_ERROR_NOT_INITIALIZED;

  if (!PL_DHashTableInit(&mPackagesHash, &kTableOps,
                         nsnull, sizeof(PackageEntry), 16))
    return NS_ERROR_FAILURE;

  if (!mOverlayHash.Init() ||
      !mStyleHash.Init() ||
      !mOverrideTable.Init())
    return NS_ERROR_FAILURE;

  mSelectedLocale = NS_LITERAL_CSTRING("en-US");
  mSelectedSkin = NS_LITERAL_CSTRING("classic/1.0");

  // This initialization process is fairly complicated and may cause reentrant
  // getservice calls to resolve chrome URIs (especially locale files). We
  // don't want that, so we inform the protocol handler about our existence
  // before we are actually fully initialized.
  gChromeRegistry = this;

  PRBool safeMode = PR_FALSE;
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

  nsCOMPtr<nsIObserverService> obsService (do_GetService("@mozilla.org/observer-service;1"));
  if (obsService) {
    obsService->AddObserver(this, "command-line-startup", PR_TRUE);
    obsService->AddObserver(this, "profile-initial-state", PR_TRUE);
  }

  CheckForNewChrome();

  mInitialized = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::CheckForOSAccessibility()
{
  nsresult rv;

  nsCOMPtr<nsILookAndFeel> lookAndFeel (do_GetService(kLookAndFeelCID));
  if (lookAndFeel) {
    PRInt32 useAccessibilityTheme = 0;

    rv = lookAndFeel->GetMetric(nsILookAndFeel::eMetric_UseAccessibilityTheme,
                                useAccessibilityTheme);

    if (NS_SUCCEEDED(rv) && useAccessibilityTheme) {
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
  }

  return NS_OK;
}

nsresult
nsChromeRegistry::GetProviderAndPath(nsIURL* aChromeURL,
                                     nsACString& aProvider, nsACString& aPath)
{
  nsresult rv;

#ifdef DEBUG
  PRBool isChrome;
  aChromeURL->SchemeIs("chrome", &isChrome);
  NS_ASSERTION(isChrome, "Non-chrome URI?");
#endif

  nsCAutoString path;
  rv = aChromeURL->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  if (path.Length() < 3) {
    LogMessage("Invalid chrome URI: %s", path.get());
    return NS_ERROR_FAILURE;
  }

  path.SetLength(nsUnescapeCount(path.BeginWriting()));
  NS_ASSERTION(path.First() == '/', "Path should always begin with a slash!");

  PRInt32 slash = path.FindChar('/', 1);
  if (slash == 1) {
    LogMessage("Invalid chrome URI: %s", path.get());
    return NS_ERROR_FAILURE;
  }

  if (slash == -1) {
    aPath.Truncate();
  }
  else {
    if (slash == (PRInt32) path.Length() - 1)
      aPath.Truncate();
    else
      aPath.Assign(path.get() + slash + 1, path.Length() - slash - 1);

    --slash;
  }

  aProvider.Assign(path.get() + 1, slash);
  return NS_OK;
}


nsresult
nsChromeRegistry::Canonify(nsIURL* aChromeURL)
{
  NS_NAMED_LITERAL_CSTRING(kSlash, "/");

  nsresult rv;

  nsCAutoString provider, path;
  rv = GetProviderAndPath(aChromeURL, provider, path);
  NS_ENSURE_SUCCESS(rv, rv);

  if (path.IsEmpty()) {
    nsCAutoString package;
    rv = aChromeURL->GetHost(package);
    NS_ENSURE_SUCCESS(rv, rv);

    // we re-use the "path" local string to build a new URL path
    path.Assign(kSlash + provider + kSlash + package);
    if (provider.EqualsLiteral("content")) {
      path.AppendLiteral(".xul");
    }
    else if (provider.EqualsLiteral("locale")) {
      path.AppendLiteral(".dtd");
    }
    else if (provider.EqualsLiteral("skin")) {
      path.AppendLiteral(".css");
    }
    else {
      return NS_ERROR_INVALID_ARG;
    }
    aChromeURL->SetPath(path);
  }
  else {
    // prevent directory traversals ("..")
    // path is already unescaped once, but uris can get unescaped twice
    const char* pos = path.BeginReading();
    const char* end = path.EndReading();
    while (pos < end) {
      switch (*pos) {
        case ':':
          return NS_ERROR_DOM_BAD_URI;
        case '.':
          if (pos[1] == '.')
            return NS_ERROR_DOM_BAD_URI;
          break;
        case '%':
          // chrome: URIs with double-escapes are trying to trick us.
          // watch for %2e, and %25 in case someone triple unescapes
          if (pos[1] == '2' &&
               ( pos[2] == 'e' || pos[2] == 'E' || 
                 pos[2] == '5' ))
            return NS_ERROR_DOM_BAD_URI;
          break;
        case '?':
        case '#':
          pos = end;
          continue;
      }
      ++pos;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::ConvertChromeURL(nsIURI* aChromeURI, nsIURI* *aResult)
{
  nsresult rv;
  NS_ASSERTION(aChromeURI, "null url!");

  if (mOverrideTable.Get(aChromeURI, aResult))
    return NS_OK;

  nsCOMPtr<nsIURL> chromeURL (do_QueryInterface(aChromeURI));
  NS_ENSURE_TRUE(chromeURL, NS_NOINTERFACE);

  nsCAutoString package, provider, path;
  rv = chromeURL->GetHostPort(package);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GetProviderAndPath(chromeURL, provider, path);
  NS_ENSURE_SUCCESS(rv, rv);

  PackageEntry* entry =
    static_cast<PackageEntry*>(PL_DHashTableOperate(&mPackagesHash,
                                                    & (nsACString&) package,
                                                    PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_FREE(entry)) {
    if (!mInitialized)
      return NS_ERROR_NOT_INITIALIZED;

    LogMessage("No chrome package registered for chrome://%s/%s/%s",
               package.get(), provider.get(), path.get());

    return NS_ERROR_FAILURE;
  }

  if (entry->flags & PackageEntry::PLATFORM_PACKAGE) {
#if defined(XP_WIN) || defined(XP_OS2)
    path.Insert("win/", 0);
#elif defined(XP_MACOSX)
    path.Insert("mac/", 0);
#else
    path.Insert("unix/", 0);
#endif
  }

  nsIURI* baseURI = nsnull;
  if (provider.EqualsLiteral("locale")) {
    baseURI = entry->locales.GetBase(mSelectedLocale, nsProviderArray::LOCALE);
  }
  else if (provider.EqualsLiteral("skin")) {
    baseURI = entry->skins.GetBase(mSelectedSkin, nsProviderArray::ANY);
  }
  else if (provider.EqualsLiteral("content")) {
    baseURI = entry->baseURI;
  }

  if (!baseURI) {
    LogMessage("No chrome package registered for chrome://%s/%s/%s",
               package.get(), provider.get(), path.get());
    return NS_ERROR_FAILURE;
  }

  return NS_NewURI(aResult, path, nsnull, baseURI);
}

nsresult
nsChromeRegistry::GetSelectedLocale(const nsACString& aPackage, nsACString& aLocale)
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

NS_IMETHODIMP
nsChromeRegistry::IsLocaleRTL(const nsACString& package, PRBool *aResult)
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

NS_IMETHODIMP
nsChromeRegistry::GetLocalesForPackage(const nsACString& aPackage,
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

#ifdef MOZ_XUL
NS_IMETHODIMP
nsChromeRegistry::GetStyleOverlays(nsIURI *aChromeURL,
                                   nsISimpleEnumerator **aResult)
{
  const nsCOMArray<nsIURI>* parray = mStyleHash.GetArray(aChromeURL);
  if (!parray)
    return NS_NewEmptyEnumerator(aResult);

  return NS_NewArrayEnumerator(aResult, *parray);
}

NS_IMETHODIMP
nsChromeRegistry::GetXULOverlays(nsIURI *aChromeURL, nsISimpleEnumerator **aResult)
{
  const nsCOMArray<nsIURI>* parray = mOverlayHash.GetArray(aChromeURL);
  if (!parray)
    return NS_NewEmptyEnumerator(aResult);

  return NS_NewArrayEnumerator(aResult, *parray);
}
#endif // MOZ_XUL

////////////////////////////////////////////////////////////////////////

// theme stuff


static void FlushSkinBindingsForWindow(nsIDOMWindowInternal* aWindow)
{
  // Get the DOM document.
  nsCOMPtr<nsIDOMDocument> domDocument;
  aWindow->GetDocument(getter_AddRefs(domDocument));
  if (!domDocument)
    return;

  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);
  if (!document)
    return;

  // Annihilate all XBL bindings.
  document->FlushSkinBindings();
}

// XXXbsmedberg: move this to nsIWindowMediator
NS_IMETHODIMP nsChromeRegistry::RefreshSkins()
{
  nsCOMPtr<nsICSSLoader> cssLoader(do_CreateInstance(kCSSLoaderCID));
  if (!cssLoader)
    return NS_OK;

  nsCOMPtr<nsIWindowMediator> windowMediator
    (do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!windowMediator)
    return NS_OK;

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));
  PRBool more;
  windowEnumerator->HasMoreElements(&more);
  while (more) {
    nsCOMPtr<nsISupports> protoWindow;
    windowEnumerator->GetNext(getter_AddRefs(protoWindow));
    if (protoWindow) {
      nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(protoWindow);
      if (domWindow)
        FlushSkinBindingsForWindow(domWindow);
    }
    windowEnumerator->HasMoreElements(&more);
  }

  FlushSkinCaches();
  
  windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));
  windowEnumerator->HasMoreElements(&more);
  while (more) {
    nsCOMPtr<nsISupports> protoWindow;
    windowEnumerator->GetNext(getter_AddRefs(protoWindow));
    if (protoWindow) {
      nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(protoWindow);
      if (domWindow)
        RefreshWindow(domWindow, cssLoader);
    }
    windowEnumerator->HasMoreElements(&more);
  }
   
  return NS_OK;
}

void
nsChromeRegistry::FlushSkinCaches()
{
  nsCOMPtr<nsIObserverService> obsSvc =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ASSERTION(obsSvc, "Couldn't get observer service.");

  obsSvc->NotifyObservers(static_cast<nsIChromeRegistry*>(this),
                          NS_CHROME_FLUSH_SKINS_TOPIC, nsnull);
}

static PRBool IsChromeURI(nsIURI* aURI)
{
    PRBool isChrome=PR_FALSE;
    if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && isChrome)
        return PR_TRUE;
    return PR_FALSE;
}

// XXXbsmedberg: move this to windowmediator
nsresult nsChromeRegistry::RefreshWindow(nsIDOMWindowInternal* aWindow,
                                         nsICSSLoader* aCSSLoader)
{
  // Deal with our subframes first.
  nsCOMPtr<nsIDOMWindowCollection> frames;
  aWindow->GetFrames(getter_AddRefs(frames));
  PRUint32 length;
  frames->GetLength(&length);
  PRUint32 j;
  for (j = 0; j < length; j++) {
    nsCOMPtr<nsIDOMWindow> childWin;
    frames->Item(j, getter_AddRefs(childWin));
    nsCOMPtr<nsIDOMWindowInternal> childInt(do_QueryInterface(childWin));
    RefreshWindow(childInt, aCSSLoader);
  }

  nsresult rv;
  // Get the DOM document.
  nsCOMPtr<nsIDOMDocument> domDocument;
  aWindow->GetDocument(getter_AddRefs(domDocument));
  if (!domDocument)
    return NS_OK;

  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);
  if (!document)
    return NS_OK;

  // Deal with the agent sheets first.  Have to do all the style sets by hand.
  nsCOMPtr<nsIPresShell> shell = document->GetPrimaryShell();
  if (shell) {
    // Reload only the chrome URL agent style sheets.
    nsCOMArray<nsIStyleSheet> agentSheets;
    rv = shell->GetAgentStyleSheets(agentSheets);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMArray<nsIStyleSheet> newAgentSheets;
    for (PRInt32 l = 0; l < agentSheets.Count(); ++l) {
      nsIStyleSheet *sheet = agentSheets[l];

      nsCOMPtr<nsIURI> uri;
      rv = sheet->GetSheetURI(getter_AddRefs(uri));
      if (NS_FAILED(rv)) return rv;

      if (IsChromeURI(uri)) {
        // Reload the sheet.
        nsCOMPtr<nsICSSStyleSheet> newSheet;
        rv = aCSSLoader->LoadSheetSync(uri, PR_TRUE, PR_TRUE,
                                       getter_AddRefs(newSheet));
        if (NS_FAILED(rv)) return rv;
        if (newSheet) {
          rv = newAgentSheets.AppendObject(newSheet) ? NS_OK : NS_ERROR_FAILURE;
          if (NS_FAILED(rv)) return rv;
        }
      }
      else {  // Just use the same sheet.
        rv = newAgentSheets.AppendObject(sheet) ? NS_OK : NS_ERROR_FAILURE;
        if (NS_FAILED(rv)) return rv;
      }
    }

    rv = shell->SetAgentStyleSheets(newAgentSheets);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Build an array of nsIURIs of style sheets we need to load.
  nsCOMArray<nsIStyleSheet> oldSheets;
  nsCOMArray<nsIStyleSheet> newSheets;

  PRInt32 count = document->GetNumberOfStyleSheets();

  // Iterate over the style sheets.
  PRInt32 i;
  for (i = 0; i < count; i++) {
    // Get the style sheet
    nsIStyleSheet *styleSheet = document->GetStyleSheetAt(i);
    
    if (!oldSheets.AppendObject(styleSheet)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Iterate over our old sheets and kick off a sync load of the new 
  // sheet if and only if it's a chrome URL.
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsICSSStyleSheet> sheet = do_QueryInterface(oldSheets[i]);
    nsIURI* uri = sheet ? sheet->GetOriginalURI() : nsnull;

    if (uri && IsChromeURI(uri)) {
      // Reload the sheet.
      nsCOMPtr<nsICSSStyleSheet> newSheet;
      // XXX what about chrome sheets that have a title or are disabled?  This
      // only works by sheer dumb luck.
      // XXXbz this should really use the document's CSSLoader!
      aCSSLoader->LoadSheetSync(uri, getter_AddRefs(newSheet));
      // Even if it's null, we put in in there.
      newSheets.AppendObject(newSheet);
    }
    else {
      // Just use the same sheet.
      newSheets.AppendObject(sheet);
    }
  }

  // Now notify the document that multiple sheets have been added and removed.
  document->UpdateStyleSheets(oldSheets, newSheets);
  return NS_OK;
}

void
nsChromeRegistry::FlushAllCaches()
{
  nsCOMPtr<nsIObserverService> obsSvc =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ASSERTION(obsSvc, "Couldn't get observer service.");

  obsSvc->NotifyObservers((nsIChromeRegistry*) this,
                          NS_CHROME_FLUSH_TOPIC, nsnull);
}  

// xxxbsmedberg Move me to nsIWindowMediator
NS_IMETHODIMP
nsChromeRegistry::ReloadChrome()
{
  FlushAllCaches();
  // Do a reload of all top level windows.
  nsresult rv = NS_OK;

  // Get the window mediator
  nsCOMPtr<nsIWindowMediator> windowMediator
    (do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (windowMediator) {
    nsCOMPtr<nsISimpleEnumerator> windowEnumerator;

    rv = windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));
    if (NS_SUCCEEDED(rv)) {
      // Get each dom window
      PRBool more;
      rv = windowEnumerator->HasMoreElements(&more);
      if (NS_FAILED(rv)) return rv;
      while (more) {
        nsCOMPtr<nsISupports> protoWindow;
        rv = windowEnumerator->GetNext(getter_AddRefs(protoWindow));
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIDOMWindowInternal> domWindow =
            do_QueryInterface(protoWindow);
          if (domWindow) {
            nsCOMPtr<nsIDOMLocation> location;
            domWindow->GetLocation(getter_AddRefs(location));
            if (location) {
              rv = location->Reload(PR_FALSE);
              if (NS_FAILED(rv)) return rv;
            }
          }
        }
        rv = windowEnumerator->HasMoreElements(&more);
        if (NS_FAILED(rv)) return rv;
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsChromeRegistry::AllowScriptsForPackage(nsIURI* aChromeURI, PRBool *aResult)
{
  nsresult rv;
  *aResult = PR_FALSE;

#ifdef DEBUG
  PRBool isChrome;
  aChromeURI->SchemeIs("chrome", &isChrome);
  NS_ASSERTION(isChrome, "Non-chrome URI passed to AllowScriptsForPackage!");
#endif

  nsCOMPtr<nsIURL> url (do_QueryInterface(aChromeURI));
  NS_ENSURE_TRUE(url, NS_NOINTERFACE);

  nsCAutoString provider, file;
  rv = GetProviderAndPath(url, provider, file);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!provider.EqualsLiteral("skin"))
    *aResult = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::AllowContentToAccess(nsIURI *aURI, PRBool *aResult)
{
  nsresult rv;

  *aResult = PR_FALSE;

#ifdef DEBUG
  PRBool isChrome;
  aURI->SchemeIs("chrome", &isChrome);
  NS_ASSERTION(isChrome, "Non-chrome URI passed to AllowContentToAccess!");
#endif

  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
  if (!url) {
    NS_ERROR("Chrome URL doesn't implement nsIURL.");
    return NS_ERROR_UNEXPECTED;
  }

  nsCAutoString package;
  rv = url->GetHostPort(package);
  NS_ENSURE_SUCCESS(rv, rv);

  PackageEntry *entry =
    static_cast<PackageEntry*>(PL_DHashTableOperate(&mPackagesHash,
                                                    & (nsACString&) package,
                                                    PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
    *aResult = !!(entry->flags & PackageEntry::CONTENT_ACCESSIBLE);
  }
  return NS_OK;
}

static PLDHashOperator
RemoveAll(PLDHashTable *table, PLDHashEntryHdr *entry, PRUint32 number, void *arg)
{
  return (PLDHashOperator) (PL_DHASH_NEXT | PL_DHASH_REMOVE);
}

NS_IMETHODIMP
nsChromeRegistry::CheckForNewChrome()
{
  nsresult rv;

  PL_DHashTableEnumerate(&mPackagesHash, RemoveAll, nsnull);
  mOverlayHash.Clear();
  mStyleHash.Clear();
  mOverrideTable.Clear();

  nsCOMPtr<nsIProperties> dirSvc (do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
  NS_ENSURE_TRUE(dirSvc, NS_ERROR_FAILURE);

  // check the extra chrome directories
  nsCOMPtr<nsISimpleEnumerator> chromeML;
  rv = dirSvc->Get(NS_CHROME_MANIFESTS_FILE_LIST, NS_GET_IID(nsISimpleEnumerator),
                   getter_AddRefs(chromeML));
  if (NS_FAILED(rv)) {
    // ok, then simply load all .manifest files in the app chrome dir.
    nsCOMPtr<nsIFile> chromeDir;
    rv = dirSvc->Get(NS_APP_CHROME_DIR, NS_GET_IID(nsIFile),
                     getter_AddRefs(chromeDir));
    if (NS_FAILED(rv))
      return rv;
    rv = NS_NewSingletonEnumerator(getter_AddRefs(chromeML), chromeDir);
    if (NS_FAILED(rv))
      return rv;
  }

  PRBool exists;
  nsCOMPtr<nsISupports> next;
  while (NS_SUCCEEDED(chromeML->HasMoreElements(&exists)) && exists) {
    chromeML->GetNext(getter_AddRefs(next));
    nsCOMPtr<nsILocalFile> lmanifest = do_QueryInterface(next);
    if (!lmanifest) {
      NS_ERROR("Directory enumerator returned a non-nsILocalFile");
      continue;
    }

    PRBool isDir;
    if (NS_SUCCEEDED(lmanifest->IsDirectory(&isDir)) && isDir) {
      nsCOMPtr<nsISimpleEnumerator> entries;
      rv = lmanifest->GetDirectoryEntries(getter_AddRefs(entries));
      if (NS_FAILED(rv))
        continue;

      while (NS_SUCCEEDED(entries->HasMoreElements(&exists)) && exists) {
        entries->GetNext(getter_AddRefs(next));
        lmanifest = do_QueryInterface(next);
        if (lmanifest) {
          nsCAutoString leafName;
          lmanifest->GetNativeLeafName(leafName);
          if (StringEndsWith(leafName, NS_LITERAL_CSTRING(".manifest"))) {
            rv = ProcessManifest(lmanifest, PR_FALSE);
            if (NS_FAILED(rv)) {
              nsCAutoString path;
              lmanifest->GetNativePath(path);
              LogMessage("Failed to process chrome manifest '%s'.",
                         path.get());

            }
          }
        }
      }
    }
    else {
      rv = ProcessManifest(lmanifest, PR_FALSE);
      if (NS_FAILED(rv)) {
        nsCAutoString path;
        lmanifest->GetNativePath(path);
        LogMessage("Failed to process chrome manifest: '%s'.",
                   path.get());
      }
    }
  }

  rv = dirSvc->Get(NS_SKIN_MANIFESTS_FILE_LIST, NS_GET_IID(nsISimpleEnumerator),
                   getter_AddRefs(chromeML));
  if (NS_FAILED(rv))
    return NS_OK;

  while (NS_SUCCEEDED(chromeML->HasMoreElements(&exists)) && exists) {
    chromeML->GetNext(getter_AddRefs(next));
    nsCOMPtr<nsILocalFile> lmanifest = do_QueryInterface(next);
    if (!lmanifest) {
      NS_ERROR("Directory enumerator returned a non-nsILocalFile");
      continue;
    }

    rv = ProcessManifest(lmanifest, PR_TRUE);
    if (NS_FAILED(rv)) {
      nsCAutoString path;
      lmanifest->GetNativePath(path);
      LogMessage("Failed to process chrome manifest: '%s'.",
                 path.get());
    }
  }

  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsChromeRegistry::WrappersEnabled(nsIURI *aURI)
{
  nsCOMPtr<nsIURL> chromeURL (do_QueryInterface(aURI));
  if (!chromeURL)
    return PR_FALSE;

  PRBool isChrome = PR_FALSE;
  nsresult rv = chromeURL->SchemeIs("chrome", &isChrome);
  if (NS_FAILED(rv) || !isChrome)
    return PR_FALSE;

  nsCAutoString package;
  rv = chromeURL->GetHostPort(package);
  if (NS_FAILED(rv))
    return PR_FALSE;

  PackageEntry* entry =
    static_cast<PackageEntry*>(PL_DHashTableOperate(&mPackagesHash,
                                                    & (nsACString&) package,
                                                    PL_DHASH_LOOKUP));

  return PL_DHASH_ENTRY_IS_LIVE(entry) &&
         entry->flags & PackageEntry::XPCNATIVEWRAPPERS;
}

nsresult
nsChromeRegistry::SelectLocaleFromPref(nsIPrefBranch* prefs)
{
  nsresult rv;
  PRBool matchOSLocale = PR_FALSE;
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

  return rv;
}

NS_IMETHODIMP nsChromeRegistry::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  nsresult rv = NS_OK;

  if (!strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic)) {
    nsCOMPtr<nsIPrefBranch> prefs (do_QueryInterface(aSubject));
    NS_ASSERTION(prefs, "Bad observer call!");

    NS_ConvertUTF16toUTF8 pref(someData);

    if (pref.EqualsLiteral(MATCH_OS_LOCALE_PREF) ||
        pref.EqualsLiteral(SELECTED_LOCALE_PREF)) {
      rv = SelectLocaleFromPref(prefs);
      if (NS_SUCCEEDED(rv) && mProfileLoaded)
        FlushAllCaches();
    }
    else if (pref.EqualsLiteral(SELECTED_SKIN_PREF)) {
      nsXPIDLCString provider;
      rv = prefs->GetCharPref(pref.get(), getter_Copies(provider));
      if (NS_FAILED(rv)) {
        NS_ERROR("Couldn't get new locale pref!");
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

nsresult
nsChromeRegistry::ProcessManifest(nsILocalFile* aManifest, PRBool aSkinOnly)
{
  nsresult rv;

  PRFileDesc* fd;
  rv = aManifest->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 n, size;
  char *buf;

  size = PR_Available(fd);
  if (size == -1) {
    rv = NS_ERROR_UNEXPECTED;
    goto mend;
  }

  buf = (char *) malloc(size + 1);
  if (!buf) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto mend;
  }

  n = PR_Read(fd, buf, size);
  if (n > 0) {
    buf[size] = '\0';
    rv = ProcessManifestBuffer(buf, size, aManifest, aSkinOnly);
  }
  free(buf);

mend:
  PR_Close(fd);
  return rv;
}

static const char kWhitespace[] = "\t ";
static const char kNewlines[]   = "\r\n";

/**
 * Check for a modifier flag of the following forms:
 *   "flag"   (same as "true")
 *   "flag=yes|true|1"
 *   "flag="no|false|0"
 * @param aFlag The flag to compare.
 * @param aData The tokenized data to check; this is lowercased
 *              before being passed in.
 * @param aResult If the flag is found, the value is assigned here.
 * @return Whether the flag was handled.
 */
static PRBool
CheckFlag(const nsSubstring& aFlag, const nsSubstring& aData, PRBool& aResult)
{
  if (!StringBeginsWith(aData, aFlag))
    return PR_FALSE;

  if (aFlag.Length() == aData.Length()) {
    // the data is simply "flag", which is the same as "flag=yes"
    aResult = PR_TRUE;
    return PR_TRUE;
  }

  if (aData.CharAt(aFlag.Length()) != '=') {
    // the data is "flag2=", which is not anything we care about
    return PR_FALSE;
  }

  if (aData.Length() == aFlag.Length() + 1) {
    aResult = PR_FALSE;
    return PR_TRUE;
  }

  switch (aData.CharAt(aFlag.Length() + 1)) {
  case '1':
  case 't': //true
  case 'y': //yes
    aResult = PR_TRUE;
    return PR_TRUE;

  case '0':
  case 'f': //false
  case 'n': //no
    aResult = PR_FALSE;
    return PR_TRUE;
  }

  return PR_FALSE;
}

enum TriState {
  eUnspecified,
  eBad,
  eOK
};

/**
 * Check for a modifier flag of the following form:
 *   "flag=string"
 *   "flag!=string"
 * @param aFlag The flag to compare.
 * @param aData The tokenized data to check; this is lowercased
 *              before being passed in.
 * @param aValue The value that is expected.
 * @param aResult If this is "ok" when passed in, this is left alone.
 *                Otherwise if the flag is found it is set to eBad or eOK.
 * @return Whether the flag was handled.
 */
static PRBool
CheckStringFlag(const nsSubstring& aFlag, const nsSubstring& aData,
                const nsSubstring& aValue, TriState& aResult)
{
  if (aData.Length() < aFlag.Length() + 1)
    return PR_FALSE;

  if (!StringBeginsWith(aData, aFlag))
    return PR_FALSE;

  PRBool comparison = PR_TRUE;
  if (aData[aFlag.Length()] != '=') {
    if (aData[aFlag.Length()] == '!' &&
        aData.Length() >= aFlag.Length() + 2 &&
        aData[aFlag.Length() + 1] == '=')
      comparison = PR_FALSE;
    else
      return PR_FALSE;
  }

  if (aResult != eOK) {
    nsDependentSubstring testdata = Substring(aData, aFlag.Length() + (comparison ? 1 : 2));
    if (testdata.Equals(aValue))
      aResult = comparison ? eOK : eBad;
    else
      aResult = comparison ? eBad : eOK;
  }

  return PR_TRUE;
}

/**
 * Check for a modifier flag of the following form:
 *   "flag=version"
 *   "flag<=version"
 *   "flag<version"
 *   "flag>=version"
 *   "flag>version"
 * @param aFlag The flag to compare.
 * @param aData The tokenized data to check; this is lowercased
 *              before being passed in.
 * @param aValue The value that is expected. If this is empty then no
 *               comparison will match.
 * @param aChecker the version checker to use. If null, aResult will always
 *                 be eBad.
 * @param aResult If this is eOK when passed in, this is left alone.
 *                Otherwise if the flag is found it is set to eBad or eOK.
 * @return Whether the flag was handled.
 */

#define COMPARE_EQ    1 << 0
#define COMPARE_LT    1 << 1
#define COMPARE_GT    1 << 2

static PRBool
CheckVersionFlag(const nsSubstring& aFlag, const nsSubstring& aData,
                 const nsSubstring& aValue, nsIVersionComparator* aChecker,
                 TriState& aResult)
{
  if (aData.Length() < aFlag.Length() + 2)
    return PR_FALSE;

  if (!StringBeginsWith(aData, aFlag))
    return PR_FALSE;

  if (aValue.Length() == 0) {
    if (aResult != eOK)
      aResult = eBad;
    return PR_TRUE;
  }

  PRUint32 comparison;
  nsAutoString testdata;

  switch (aData[aFlag.Length()]) {
  case '=':
    comparison = COMPARE_EQ;
    testdata = Substring(aData, aFlag.Length() + 1);
    break;

  case '<':
    if (aData[aFlag.Length() + 1] == '=') {
      comparison = COMPARE_EQ | COMPARE_LT;
      testdata = Substring(aData, aFlag.Length() + 2);
    }
    else {
      comparison = COMPARE_LT;
      testdata = Substring(aData, aFlag.Length() + 1);
    }
    break;

  case '>':
    if (aData[aFlag.Length() + 1] == '=') {
      comparison = COMPARE_EQ | COMPARE_GT;
      testdata = Substring(aData, aFlag.Length() + 2);
    }
    else {
      comparison = COMPARE_GT;
      testdata = Substring(aData, aFlag.Length() + 1);
    }
    break;

  default:
    return PR_FALSE;
  }

  if (testdata.Length() == 0)
    return PR_FALSE;

  if (aResult != eOK) {
    if (!aChecker) {
      aResult = eBad;
    }
    else {
      PRInt32 c;
      nsresult rv = aChecker->Compare(NS_ConvertUTF16toUTF8(aValue),
                                      NS_ConvertUTF16toUTF8(testdata), &c);
      if (NS_FAILED(rv)) {
        aResult = eBad;
      }
      else {
        if ((c == 0 && comparison & COMPARE_EQ) ||
            (c < 0 && comparison & COMPARE_LT) ||
            (c > 0 && comparison & COMPARE_GT))
          aResult = eOK;
        else
          aResult = eBad;
      }
    }
  }

  return PR_TRUE;
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

nsresult
nsChromeRegistry::ProcessManifestBuffer(char *buf, PRInt32 length,
                                        nsILocalFile* aManifest,
                                        PRBool aSkinOnly)
{
  nsresult rv;

  NS_NAMED_LITERAL_STRING(kPlatform, "platform");
  NS_NAMED_LITERAL_STRING(kXPCNativeWrappers, "xpcnativewrappers");
  NS_NAMED_LITERAL_STRING(kContentAccessible, "contentaccessible");
  NS_NAMED_LITERAL_STRING(kApplication, "application");
  NS_NAMED_LITERAL_STRING(kAppVersion, "appversion");
  NS_NAMED_LITERAL_STRING(kOs, "os");
  NS_NAMED_LITERAL_STRING(kOsVersion, "osversion");

  nsCOMPtr<nsIIOService> io (do_GetIOService());
  if (!io) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIProtocolHandler> ph;
  rv = io->GetProtocolHandler("resource", getter_AddRefs(ph));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIResProtocolHandler> rph (do_QueryInterface(ph));
  if (!rph) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIURI> manifestURI;
  rv = io->NewFileURI(aManifest, getter_AddRefs(manifestURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXPConnect> xpc (do_GetService("@mozilla.org/js/xpc/XPConnect;1"));
  nsCOMPtr<nsIVersionComparator> vc (do_GetService("@mozilla.org/xpcom/version-comparator;1"));

  nsAutoString appID;
  nsAutoString appVersion;
  nsAutoString osTarget;
  nsCOMPtr<nsIXULAppInfo> xapp (do_GetService(XULAPPINFO_SERVICE_CONTRACTID));
  if (xapp) {
    nsCAutoString s;
    rv = xapp->GetID(s);
    if (NS_SUCCEEDED(rv))
      CopyUTF8toUTF16(s, appID);

    rv = xapp->GetVersion(s);
    if (NS_SUCCEEDED(rv))
      CopyUTF8toUTF16(s, appVersion);
    
    nsCOMPtr<nsIXULRuntime> xruntime (do_QueryInterface(xapp));
    if (xruntime) {
      rv = xruntime->GetOS(s);
      if (NS_SUCCEEDED(rv)) {
        CopyUTF8toUTF16(s, osTarget);
        ToLowerCase(osTarget);
      }
    }
  }
  
  nsAutoString osVersion;
#if defined(XP_WIN)
  OSVERSIONINFO info = { sizeof(OSVERSIONINFO) };
  if (GetVersionEx(&info)) {
    nsTextFormatter::ssprintf(osVersion, NS_LITERAL_STRING("%ld.%ld").get(),
                                         info.dwMajorVersion,
                                         info.dwMinorVersion);
  }
#elif defined(XP_MACOSX)
  SInt32 majorVersion, minorVersion;
  if ((Gestalt(gestaltSystemVersionMajor, &majorVersion) == noErr) &&
      (Gestalt(gestaltSystemVersionMinor, &minorVersion) == noErr)) {
    nsTextFormatter::ssprintf(osVersion, NS_LITERAL_STRING("%ld.%ld").get(),
                                         majorVersion,
                                         minorVersion);
  }
#elif defined(MOZ_WIDGET_GTK2)
  nsTextFormatter::ssprintf(osVersion, NS_LITERAL_STRING("%ld.%ld").get(),
                                       gtk_major_version,
                                       gtk_minor_version);
#endif

  char *token;
  char *newline = buf;
  PRUint32 line = 0;

  // outer loop tokenizes by newline
  while (nsnull != (token = nsCRT::strtok(newline, kNewlines, &newline))) {
    ++line;

    if (*token == '#') // ignore lines that begin with # as comments
      continue;

    char *whitespace = token;
    token = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
    if (!token) continue;

    if (!strcmp(token, "content")) {
      if (aSkinOnly) {
        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Ignoring content registration in skin-only manifest.");
        continue;
      }
      char *package = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      char *uri     = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      if (!package || !uri) {
        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Malformed content registration.");
        continue;
      }

      EnsureLowerCase(package);

      // NOTE: We check for platform and xpcnativewrappers modifiers on
      // content packages, but they are *applied* to content|skin|locale.

      PRBool platform = PR_FALSE;
      PRBool xpcNativeWrappers = PR_TRUE;
      PRBool contentAccessible = PR_FALSE;
      TriState stAppVersion = eUnspecified;
      TriState stApp = eUnspecified;
      TriState stOsVersion = eUnspecified;
      TriState stOs = eUnspecified;

      PRBool badFlag = PR_FALSE;

      while (nsnull != (token = nsCRT::strtok(whitespace, kWhitespace, &whitespace)) &&
             !badFlag) {
        NS_ConvertASCIItoUTF16 wtoken(token);
        ToLowerCase(wtoken);

        if (CheckFlag(kPlatform, wtoken, platform) ||
            CheckFlag(kXPCNativeWrappers, wtoken, xpcNativeWrappers) ||
            CheckFlag(kContentAccessible, wtoken, contentAccessible) ||
            CheckStringFlag(kApplication, wtoken, appID, stApp) ||
            CheckStringFlag(kOs, wtoken, osTarget, stOs) ||
            CheckVersionFlag(kOsVersion, wtoken, osVersion, vc, stOsVersion) ||
            CheckVersionFlag(kAppVersion, wtoken, appVersion, vc, stAppVersion))
          continue;

        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Unrecognized chrome registration modifier '%s'.",
                              token);
        badFlag = PR_TRUE;
      }

      if (badFlag || stApp == eBad || stAppVersion == eBad || 
          stOs == eBad || stOsVersion == eBad)
        continue;

      nsCOMPtr<nsIURI> resolved;
      rv = io->NewURI(nsDependentCString(uri), nsnull, manifestURI,
                      getter_AddRefs(resolved));
      if (NS_FAILED(rv))
        continue;

      if (!CanLoadResource(resolved)) {
        LogMessageWithContext(resolved, line, nsIScriptError::warningFlag,
                              "Warning: cannot register non-local URI '%s' as content.",
                              uri);
        continue;
      }

      PackageEntry* entry =
        static_cast<PackageEntry*>(PL_DHashTableOperate(&mPackagesHash,
                                                            & (const nsACString&) nsDependentCString(package),
                                                            PL_DHASH_ADD));
      if (!entry)
        return NS_ERROR_OUT_OF_MEMORY;

      entry->baseURI = resolved;

      if (platform)
        entry->flags |= PackageEntry::PLATFORM_PACKAGE;
      if (xpcNativeWrappers)
        entry->flags |= PackageEntry::XPCNATIVEWRAPPERS;
      if (contentAccessible)
        entry->flags |= PackageEntry::CONTENT_ACCESSIBLE;
      if (xpc) {
        nsCAutoString urlp("chrome://");
        urlp.Append(package);
        urlp.Append('/');

        rv = xpc->FlagSystemFilenamePrefix(urlp.get(), xpcNativeWrappers);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    else if (!strcmp(token, "locale")) {
      if (aSkinOnly) {
        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Ignoring locale registration in skin-only manifest.");
        continue;
      }
      char *package  = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      char *provider = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      char *uri      = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      if (!package || !provider || !uri) {
        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Malformed locale registration.");
        continue;
      }

      EnsureLowerCase(package);

      TriState stAppVersion = eUnspecified;
      TriState stApp = eUnspecified;
      TriState stOs = eUnspecified;
      TriState stOsVersion = eUnspecified;

      PRBool badFlag = PR_FALSE;

      while (nsnull != (token = nsCRT::strtok(whitespace, kWhitespace, &whitespace)) &&
             !badFlag) {
        NS_ConvertASCIItoUTF16 wtoken(token);
        ToLowerCase(wtoken);

        if (CheckStringFlag(kApplication, wtoken, appID, stApp) ||
            CheckStringFlag(kOs, wtoken, osTarget, stOs) ||
            CheckVersionFlag(kOsVersion, wtoken, osVersion, vc, stOsVersion) ||
            CheckVersionFlag(kAppVersion, wtoken, appVersion, vc, stAppVersion))
          continue;

        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Unrecognized chrome registration modifier '%s'.",
                              token);
        badFlag = PR_TRUE;
      }

      if (badFlag || stApp == eBad || stAppVersion == eBad ||
          stOs == eBad || stOsVersion == eBad)
        continue;

      nsCOMPtr<nsIURI> resolved;
      rv = io->NewURI(nsDependentCString(uri), nsnull, manifestURI,
                      getter_AddRefs(resolved));
      if (NS_FAILED(rv))
        continue;

      if (!CanLoadResource(resolved)) {
        LogMessageWithContext(resolved, line, nsIScriptError::warningFlag,
                              "Warning: cannot register non-local URI '%s' as a locale.",
                              uri);
        continue;
      }

      PackageEntry* entry =
        static_cast<PackageEntry*>(PL_DHashTableOperate(&mPackagesHash,
                                                            & (const nsACString&) nsDependentCString(package),
                                                            PL_DHASH_ADD));
      if (!entry)
        return NS_ERROR_OUT_OF_MEMORY;

      entry->locales.SetBase(nsDependentCString(provider), resolved);
    }
    else if (!strcmp(token, "skin")) {
      char *package  = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      char *provider = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      char *uri      = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      if (!package || !provider || !uri) {
        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Malformed skin registration.");
        continue;
      }

      EnsureLowerCase(package);

      TriState stAppVersion = eUnspecified;
      TriState stApp = eUnspecified;
      TriState stOs = eUnspecified;
      TriState stOsVersion = eUnspecified;

      PRBool badFlag = PR_FALSE;

      while (nsnull != (token = nsCRT::strtok(whitespace, kWhitespace, &whitespace)) &&
             !badFlag) {
        NS_ConvertASCIItoUTF16 wtoken(token);
        ToLowerCase(wtoken);

        if (CheckStringFlag(kApplication, wtoken, appID, stApp) ||
            CheckStringFlag(kOs, wtoken, osTarget, stOs) ||
            CheckVersionFlag(kOsVersion, wtoken, osVersion, vc, stOsVersion) ||
            CheckVersionFlag(kAppVersion, wtoken, appVersion, vc, stAppVersion))
          continue;

        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Unrecognized chrome registration modifier '%s'.",
                              token);
        badFlag = PR_TRUE;
      }

      if (badFlag || stApp == eBad || stAppVersion == eBad ||
          stOs == eBad || stOsVersion == eBad)
        continue;

      nsCOMPtr<nsIURI> resolved;
      rv = io->NewURI(nsDependentCString(uri), nsnull, manifestURI,
                      getter_AddRefs(resolved));
      if (NS_FAILED(rv))
        continue;

      if (!CanLoadResource(resolved)) {
        LogMessageWithContext(resolved, line, nsIScriptError::warningFlag,
                              "Warning: cannot register non-local URI '%s' as a skin.",
                              uri);
        continue;
      }

      PackageEntry* entry =
        static_cast<PackageEntry*>(PL_DHashTableOperate(&mPackagesHash,
                                                            & (const nsACString&) nsDependentCString(package),
                                                            PL_DHASH_ADD));
      if (!entry)
        return NS_ERROR_OUT_OF_MEMORY;

      entry->skins.SetBase(nsDependentCString(provider), resolved);
    }
    else if (!strcmp(token, "overlay")) {
      if (aSkinOnly) {
        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Ignoring overlay registration in skin-only manifest.");
        continue;
      }
      char *base    = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      char *overlay = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      if (!base || !overlay) {
        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: malformed chrome overlay instruction.");
        continue;
      }

      TriState stAppVersion = eUnspecified;
      TriState stApp = eUnspecified;
      TriState stOs = eUnspecified;
      TriState stOsVersion = eUnspecified;

      PRBool badFlag = PR_FALSE;

      while (nsnull != (token = nsCRT::strtok(whitespace, kWhitespace, &whitespace)) &&
             !badFlag) {
        NS_ConvertASCIItoUTF16 wtoken(token);
        ToLowerCase(wtoken);

        if (CheckStringFlag(kApplication, wtoken, appID, stApp) ||
            CheckStringFlag(kOs, wtoken, osTarget, stOs) ||
            CheckVersionFlag(kOsVersion, wtoken, osVersion, vc, stOsVersion) ||
            CheckVersionFlag(kAppVersion, wtoken, appVersion, vc, stAppVersion))
          continue;

        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Unrecognized chrome registration modifier '%s'.",
                              token);
        badFlag = PR_TRUE;
      }

      if (badFlag || stApp == eBad || stAppVersion == eBad ||
          stOs == eBad || stOsVersion == eBad)
        continue;

      nsCOMPtr<nsIURI> baseuri, overlayuri;
      rv  = io->NewURI(nsDependentCString(base), nsnull, nsnull,
                       getter_AddRefs(baseuri));
      rv |= io->NewURI(nsDependentCString(overlay), nsnull, nsnull,
                       getter_AddRefs(overlayuri));
      if (NS_FAILED(rv)) {
        NS_WARNING("Could not make URIs for overlay directive. Ignoring.");
        continue;
      }

      if (!CanLoadResource(overlayuri)) {
        LogMessageWithContext(overlayuri, line, nsIScriptError::warningFlag,
                              "Warning: cannot register non-local URI '%s' as an overlay.",
                              overlay);
        continue;
      }

      mOverlayHash.Add(baseuri, overlayuri);
    }
    else if (!strcmp(token, "style")) {
      char *base    = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      char *overlay = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      if (!base || !overlay) {
        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: malformed chrome style instruction.");
        continue;
      }

      TriState stAppVersion = eUnspecified;
      TriState stApp = eUnspecified;
      TriState stOs = eUnspecified;
      TriState stOsVersion = eUnspecified;

      PRBool badFlag = PR_FALSE;

      while (nsnull != (token = nsCRT::strtok(whitespace, kWhitespace, &whitespace)) &&
             !badFlag) {
        NS_ConvertASCIItoUTF16 wtoken(token);
        ToLowerCase(wtoken);

        if (CheckStringFlag(kApplication, wtoken, appID, stApp) ||
            CheckStringFlag(kOs, wtoken, osTarget, stOs) ||
            CheckVersionFlag(kOsVersion, wtoken, osVersion, vc, stOsVersion) ||
            CheckVersionFlag(kAppVersion, wtoken, appVersion, vc, stAppVersion))
          continue;

        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Unrecognized chrome registration modifier '%s'.",
                              token);
        badFlag = PR_TRUE;
      }

      if (badFlag || stApp == eBad || stAppVersion == eBad ||
          stOs == eBad || stOsVersion == eBad)
        continue;

      nsCOMPtr<nsIURI> baseuri, overlayuri;
      rv  = io->NewURI(nsDependentCString(base), nsnull, nsnull,
                      getter_AddRefs(baseuri));
      rv |= io->NewURI(nsDependentCString(overlay), nsnull, nsnull,
                       getter_AddRefs(overlayuri));
      if (NS_FAILED(rv))
        continue;

      if (!CanLoadResource(overlayuri)) {
        LogMessageWithContext(overlayuri, line, nsIScriptError::warningFlag,
                              "Warning: cannot register non-local URI '%s' as a style overlay.",
                              overlay);
        continue;
      }

      mStyleHash.Add(baseuri, overlayuri);
    }
    else if (!strcmp(token, "override")) {
      if (aSkinOnly) {
        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Ignoring override registration in skin-only manifest.");
        continue;
      }

      char *chrome    = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      char *resolved  = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      if (!chrome || !resolved) {
        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: malformed chrome override instruction.");
        continue;
      }

      TriState stAppVersion = eUnspecified;
      TriState stApp = eUnspecified;
      TriState stOs = eUnspecified;
      TriState stOsVersion = eUnspecified;

      PRBool badFlag = PR_FALSE;

      while (nsnull != (token = nsCRT::strtok(whitespace, kWhitespace, &whitespace)) &&
             !badFlag) {
        NS_ConvertASCIItoUTF16 wtoken(token);
        ToLowerCase(wtoken);

        if (CheckStringFlag(kApplication, wtoken, appID, stApp) ||
            CheckStringFlag(kOs, wtoken, osTarget, stOs) ||
            CheckVersionFlag(kOsVersion, wtoken, osVersion, vc, stOsVersion) ||
            CheckVersionFlag(kAppVersion, wtoken, appVersion, vc, stAppVersion))
          continue;

        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Unrecognized chrome registration modifier '%s'.",
                              token);
        badFlag = PR_TRUE;
      }

      if (badFlag || stApp == eBad || stAppVersion == eBad ||
          stOs == eBad || stOsVersion == eBad)
        continue;

      nsCOMPtr<nsIURI> chromeuri, resolveduri;
      rv  = io->NewURI(nsDependentCString(chrome), nsnull, nsnull,
                      getter_AddRefs(chromeuri));
      rv |= io->NewURI(nsDependentCString(resolved), nsnull, manifestURI,
                       getter_AddRefs(resolveduri));
      if (NS_FAILED(rv))
        continue;

      if (!CanLoadResource(resolveduri)) {
        LogMessageWithContext(resolveduri, line, nsIScriptError::warningFlag,
                              "Warning: cannot register non-local URI '%s' as an override.",
                              resolved);
        continue;
      }

      mOverrideTable.Put(chromeuri, resolveduri);
    }
    else if (!strcmp(token, "resource")) {
      if (aSkinOnly) {
        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Ignoring resource registration in skin-only manifest.");
        continue;
      }

      char *package = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      char *uri     = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
      if (!package || !uri) {
        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Malformed resource registration.");
        continue;
      }

      EnsureLowerCase(package);

      TriState stAppVersion = eUnspecified;
      TriState stApp = eUnspecified;
      TriState stOsVersion = eUnspecified;
      TriState stOs = eUnspecified;

      PRBool badFlag = PR_FALSE;

      while (nsnull != (token = nsCRT::strtok(whitespace, kWhitespace, &whitespace)) &&
             !badFlag) {
        NS_ConvertASCIItoUTF16 wtoken(token);
        ToLowerCase(wtoken);

        if (CheckStringFlag(kApplication, wtoken, appID, stApp) ||
            CheckStringFlag(kOs, wtoken, osTarget, stOs) ||
            CheckVersionFlag(kOsVersion, wtoken, osVersion, vc, stOsVersion) ||
            CheckVersionFlag(kAppVersion, wtoken, appVersion, vc, stAppVersion))
          continue;

        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Unrecognized chrome registration modifier '%s'.",
                              token);
        badFlag = PR_TRUE;
      }

      if (badFlag || stApp == eBad || stAppVersion == eBad || 
          stOs == eBad || stOsVersion == eBad)
        continue;
      
      nsDependentCString host(package);

      PRBool exists;
      rv = rph->HasSubstitution(host, &exists);
      NS_ENSURE_SUCCESS(rv, rv);
      if (exists) {
        LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                              "Warning: Duplicate resource declaration for '%s' ignored.",
                              package);
        continue;
      }

      nsCOMPtr<nsIURI> resolved;
      rv = io->NewURI(nsDependentCString(uri), nsnull, manifestURI,
                      getter_AddRefs(resolved));
      if (NS_FAILED(rv))
        continue;

      if (!CanLoadResource(resolved)) {
        LogMessageWithContext(resolved, line, nsIScriptError::warningFlag,
                              "Warning: cannot register non-local URI '%s' as a resource.",
                              uri);
        continue;
      }

      rv = rph->SetSubstitution(host, resolved);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      LogMessageWithContext(manifestURI, line, nsIScriptError::warningFlag,
                            "Warning: Ignoring unrecognized chrome manifest instruction.");
    }
  }

  return NS_OK;
}
