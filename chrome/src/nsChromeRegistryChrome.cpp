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

#ifdef MOZ_IPC
#include "mozilla/dom/PContentProcessParent.h"
#include "RegistryMessageUtils.h"
#include "nsResProtocolHandler.h"
#endif

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
#include "nsEnumeratorUtils.h"
#include "nsNetUtil.h"
#include "nsStringEnumerator.h"
#include "nsTextFormatter.h"
#include "nsUnicharUtils.h"
#include "nsWidgetsCID.h"
#include "nsXPCOMCIDInternal.h"

#include "nsICommandLine.h"
#include "nsILocaleService.h"
#include "nsILocalFile.h"
#include "nsILookAndFeel.h"
#include "nsIObserverService.h"
#include "nsIPrefBranch2.h"
#include "nsIPrefService.h"
#include "nsIScriptError.h"
#include "nsIVersionComparator.h"
#include "nsIXPConnect.h"
#include "nsIXULAppInfo.h"
#include "nsIXULRuntime.h"

#define UILOCALE_CMD_LINE_ARG "UILocale"

#define MATCH_OS_LOCALE_PREF "intl.locale.matchOS"
#define SELECTED_LOCALE_PREF "general.useragent.locale"
#define SELECTED_SKIN_PREF   "general.skins.selectedSkin"

static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

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

  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistryChrome::CheckForOSAccessibility()
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
nsChromeRegistryChrome::IsLocaleRTL(const nsACString& package, PRBool *aResult)
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
  PRBool matchOSLocale = PR_FALSE, userLocaleOverride = PR_FALSE;
  prefs->PrefHasUserValue(SELECTED_LOCALE_PREF, &userLocaleOverride);
  rv = prefs->GetBoolPref(MATCH_OS_LOCALE_PREF, &matchOSLocale);

  if (NS_SUCCEEDED(rv) && matchOSLocale && !userLocaleOverride) {
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

NS_IMETHODIMP
nsChromeRegistryChrome::CheckForNewChrome()
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

#ifdef MOZ_IPC
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
  nsTArray<ChromePackage>& packages;
  const nsCString& selectedLocale;
  const nsCString& selectedSkin;
};

void
nsChromeRegistryChrome::SendRegisteredChrome(
    mozilla::dom::PContentProcessParent* aParent)
{
  nsTArray<ChromePackage> packages;
  nsTArray<ResourceMapping> resources;
  nsTArray<OverrideMapping> overrides;

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

  bool success = aParent->SendRegisterChrome(packages, resources, overrides);
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
#endif

static PRBool
CanLoadResource(nsIURI* aResourceURI)
{
  PRBool isLocalResource = PR_FALSE;
  (void)NS_URIChainHasFlags(aResourceURI,
                            nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                            &isLocalResource);
  return isLocalResource;
}

nsresult
nsChromeRegistryChrome::ProcessManifest(nsILocalFile* aManifest,
                                        PRBool aSkinOnly)
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
nsChromeRegistryChrome::ProcessManifestBuffer(char *buf, PRInt32 length,
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
        entry->flags |= PLATFORM_PACKAGE;
      if (xpcNativeWrappers)
        entry->flags |= XPCNATIVEWRAPPERS;
      if (contentAccessible)
        entry->flags |= CONTENT_ACCESSIBLE;
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

nsresult
nsChromeRegistryChrome::GetBaseURIFromPackage(const nsCString& aPackage,
                                              const nsCString& aProvider,
                                              const nsCString& aPath,
                                              nsIURI* *aResult)
{
  PackageEntry* entry =
      static_cast<PackageEntry*>(PL_DHashTableOperate(&mPackagesHash,
                                                      &aPackage,
                                                      PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_FREE(entry)) {
    if (!mInitialized)
      return NS_ERROR_NOT_INITIALIZED;

    LogMessage("No chrome package registered for chrome://%s/%s/%s",
               aPackage.get(), aProvider.get(), aPath.get());

    return NS_ERROR_FAILURE;
  }

  *aResult = nsnull;
  if (aProvider.EqualsLiteral("locale")) {
    *aResult = entry->locales.GetBase(mSelectedLocale, nsProviderArray::LOCALE);
  }
  else if (aProvider.EqualsLiteral("skin")) {
    *aResult = entry->skins.GetBase(mSelectedSkin, nsProviderArray::ANY);
  }
  else if (aProvider.EqualsLiteral("content")) {
    *aResult = entry->baseURI;
  }
  return NS_OK;
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

PRBool
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

PRBool
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
    PRBool equals;
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
