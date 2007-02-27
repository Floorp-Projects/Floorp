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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Matejka
 *     (Original Author)
 *   Ben Goodger <ben@bengoodger.com> 
 *     (History, Favorites, Passwords, Form Data, some settings)
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

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "nsAppDirectoryServiceDefs.h"
#include "nsBrowserProfileMigratorUtils.h"
#include "nsCOMPtr.h"
#include "nsCRTGlue.h"
#include "nsNetCID.h"
#include "nsDocShellCID.h"
#include "nsDebug.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsStringAPI.h"
#include "plstr.h"
#include "prio.h"
#include "prmem.h"
#include "prlong.h"
#include "nsICookieManager2.h"
#include "nsIEProfileMigrator.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsISimpleEnumerator.h"
#include "nsISupportsArray.h"
#include "nsIProfileMigrator.h"
#include "nsIBrowserProfileMigrator.h"
#include "nsIObserverService.h"

#include <objbase.h>
#include <shlguid.h>
#include <urlhist.h>
#include <comdef.h>
#include <shlobj.h>
#include <intshcut.h>

#include "nsIBrowserHistory.h"
#include "nsIGlobalHistory.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIURI.h"
#include "nsIPasswordManager.h"
#include "nsIPasswordManagerInternal.h"
#include "nsIFormHistory.h"
#include "nsIRDFService.h"
#include "nsIRDFContainer.h"
#include "nsIURL.h"
#ifdef MOZ_PLACES_BOOKMARKS
#include "nsINavBookmarksService.h"
#include "nsBrowserCompsCID.h"
#else
#include "nsIBookmarksService.h"
#endif
#include "nsIStringBundle.h"
#include "nsNetUtil.h"
#include "nsToolkitCompsCID.h"
#include "nsUnicharUtils.h"
#include "nsIWindowsRegKey.h"

#define TRIDENTPROFILE_BUNDLE       "chrome://browser/locale/migration/migration.properties"

const int sInitialCookieBufferSize = 1024; // but it can grow
const int sUsernameLengthLimit     = 80;
const int sHostnameLengthLimit     = 255;

//***********************************************************************
//*** Replacements for comsupp.lib calls used by pstorec.dll
//***********************************************************************
void  __stdcall _com_issue_error(HRESULT hr)
{
  // XXX - Do nothing for now
}

//***********************************************************************
//*** windows registry to mozilla prefs data type translation functions
//***********************************************************************

typedef void (*regEntryHandler)(nsIWindowsRegKey *, const nsString&,
                                nsIPrefBranch *, char *);

// yes/no string to T/F boolean
void
TranslateYNtoTF(nsIWindowsRegKey *aRegKey, const nsString& aRegValueName,
                nsIPrefBranch *aPrefs, char *aPrefKeyName) {

  // input type is a string, lowercase "yes" or "no"
  nsAutoString regValue; 
  if (NS_SUCCEEDED(aRegKey->ReadStringValue(aRegValueName, regValue)))
    aPrefs->SetBoolPref(aPrefKeyName, regValue.EqualsLiteral("yes"));
}

// yes/no string to F/T boolean
void
TranslateYNtoFT(nsIWindowsRegKey *aRegKey, const nsString& aRegValueName,
                nsIPrefBranch *aPrefs, char *aPrefKeyName) {

  // input type is a string, lowercase "yes" or "no"
  nsAutoString regValue; 
  if (NS_SUCCEEDED(aRegKey->ReadStringValue(aRegValueName, regValue)))
    aPrefs->SetBoolPref(aPrefKeyName, !regValue.EqualsLiteral("yes"));
}

void
TranslateYNtoImageBehavior(nsIWindowsRegKey *aRegKey,
                           const nsString& aRegValueName,
                           nsIPrefBranch *aPrefs, char *aPrefKeyName) {
  // input type is a string, lowercase "yes" or "no"
  nsAutoString regValue; 
  if (NS_SUCCEEDED(aRegKey->ReadStringValue(aRegValueName, regValue)) &&
      !regValue.IsEmpty()) {
    if (regValue.EqualsLiteral("yes"))
      aPrefs->SetIntPref(aPrefKeyName, 1);
    else
      aPrefs->SetIntPref(aPrefKeyName, 2);
  }
}

void
TranslateDWORDtoHTTPVersion(nsIWindowsRegKey *aRegKey,
                            const nsString& aRegValueName,
                            nsIPrefBranch *aPrefs, char *aPrefKeyName) {
  PRUint32 val;
  if (NS_SUCCEEDED(aRegKey->ReadIntValue(aRegValueName, &val))) {
    if (val & 0x1) 
      aPrefs->SetCharPref(aPrefKeyName, "1.1");
    else
      aPrefs->SetCharPref(aPrefKeyName, "1.0");
  }
}

// decimal RGB (1,2,3) to hex RGB (#010203)
void
TranslateDRGBtoHRGB(nsIWindowsRegKey *aRegKey, const nsString& aRegValueName,
                    nsIPrefBranch *aPrefs, char *aPrefKeyName) {

  // clear previous settings with defaults
  char prefStringValue[10];

  nsAutoString regValue; 
  if (NS_SUCCEEDED(aRegKey->ReadStringValue(aRegValueName, regValue)) &&
      !regValue.IsEmpty()) {
    int red, green, blue;
    ::swscanf(regValue.get(), L"%d,%d,%d", &red, &green, &blue);
    ::sprintf(prefStringValue, "#%02X%02X%02X", red, green, blue);
    aPrefs->SetCharPref(aPrefKeyName, prefStringValue);
  }
}

// translate a windows registry DWORD int to a mozilla prefs PRInt32
void
TranslateDWORDtoPRInt32(nsIWindowsRegKey *aRegKey,
                        const nsString& aRegValueName,
                        nsIPrefBranch *aPrefs, char *aPrefKeyName) {

  // clear previous settings with defaults
  PRInt32 prefIntValue = 0;

  if (NS_SUCCEEDED(aRegKey->ReadIntValue(aRegValueName, 
                   NS_REINTERPRET_CAST(PRUint32 *, &prefIntValue))))
    aPrefs->SetIntPref(aPrefKeyName, prefIntValue);
}

// string copy
void
TranslateString(nsIWindowsRegKey *aRegKey, const nsString& aRegValueName,
                nsIPrefBranch *aPrefs, char *aPrefKeyName) {
  nsAutoString regValue; 
  if (NS_SUCCEEDED(aRegKey->ReadStringValue(aRegValueName, regValue)) &&
      !regValue.IsEmpty()) {
    aPrefs->SetCharPref(aPrefKeyName, NS_ConvertUTF16toUTF8(regValue).get());
  }
}

// translate accepted language character set formats
// (modified string copy)
void
TranslateLanglist(nsIWindowsRegKey *aRegKey, const nsString& aRegValueName,
                  nsIPrefBranch *aPrefs, char *aPrefKeyName) {

  nsAutoString lang;
  if (NS_FAILED(aRegKey->ReadStringValue(aRegValueName, lang)))
      return;

  // copy source format like "en-us,ar-kw;q=0.7,ar-om;q=0.3" into
  // destination format like "en-us, ar-kw, ar-om"

  char prefStringValue[MAX_PATH]; // a convenient size, one hopes
  NS_LossyConvertUTF16toASCII langCstr(lang);
  const char   *source = langCstr.get(),
               *sourceEnd = source + langCstr.Length();
  char   *dest = prefStringValue,
         *destEnd = dest + (MAX_PATH-2); // room for " \0"
  PRBool  skip = PR_FALSE,
          comma = PR_FALSE;

  while (source < sourceEnd && *source && dest < destEnd) {
    if (*source == ',')
      skip = PR_FALSE;
    else if (*source == ';')
      skip = PR_TRUE;
    if (!skip) {
      if (comma && *source != ' ')
        *dest++ = ' ';
      *dest++ = *source;
    }
    comma = *source == ',';
    ++source;
  }
  *dest = 0;

  aPrefs->SetCharPref(aPrefKeyName, prefStringValue);
}

static int CALLBACK
fontEnumProc(const LOGFONTW *aLogFont, const TEXTMETRICW *aMetric,
             DWORD aFontType, LPARAM aClosure) {
  *((int *) aClosure) = aLogFont->lfPitchAndFamily & FF_ROMAN;
  return 0;
}
void
TranslatePropFont(nsIWindowsRegKey *aRegKey, const nsString& aRegValueName,
                  nsIPrefBranch *aPrefs, char *aPrefKeyName) {

  HDC      dc = ::GetDC(0);
  LOGFONTW lf;
  int      isSerif = 1;

  // serif or sans-serif font?
  lf.lfCharSet = DEFAULT_CHARSET;
  lf.lfPitchAndFamily = 0;
  nsAutoString font;
  if (NS_FAILED(aRegKey->ReadStringValue(aRegValueName, font)))
      return;

  ::wcsncpy(lf.lfFaceName, font.get(), LF_FACESIZE);
  lf.lfFaceName[LF_FACESIZE - 1] = L'\0';
  ::EnumFontFamiliesExW(dc, &lf, fontEnumProc, (LPARAM) &isSerif, 0);
  ::ReleaseDC(0, dc);

  // XXX : For now, only x-western font is translated.
  // All or Locale-dependent subset of fonts need to be translated.
  nsDependentCString generic(isSerif ? "serif" : "sans-serif");
  nsCAutoString prefName("font.name.");
  prefName.Append(generic);
  prefName.Append(".x-western");
  aPrefs->SetCharPref(prefName.get(), NS_ConvertUTF16toUTF8(font).get());
  aPrefs->SetCharPref("font.default.x-western", generic.get());
}

//***********************************************************************
//*** master table of registry-to-gecko-pref translations
//***********************************************************************

struct regEntry {
  char            *regKeyName,     // registry key (HKCU\Software ...)
                  *regValueName;   // registry key leaf
  char            *prefKeyName;    // pref name ("javascript.enabled" ...)
  regEntryHandler  entryHandler;   // processing func
};

const regEntry gRegEntries[] = {
  { "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoComplete",
     "AutoSuggest",
     "browser.urlbar.autocomplete.enabled",
     TranslateYNtoTF },
  { "Software\\Microsoft\\Internet Explorer\\International",
    "AcceptLanguage",
    "intl.accept_languages",
    TranslateLanglist },
  // XXX : For now, only x-western font is translated.
  // All or Locale-dependent subset of fonts need to be translated.
  { "Software\\Microsoft\\Internet Explorer\\International\\Scripts\\3",
    "IEFixedFontName",
    "font.name.monospace.x-western",
    TranslateString },
  { 0, // an optimization: 0 means use the previous key
    "IEPropFontName",
    "", // special-cased in the translation function
    TranslatePropFont },
  { "Software\\Microsoft\\Internet Explorer\\Main",
    "Use_DlgBox_Colors",
    "browser.display.use_system_colors",
    TranslateYNtoTF },
  { 0,
    "Use FormSuggest",
    "browser.formfill.enable",
    TranslateYNtoTF },
  { 0,
    "FormSuggest Passwords",
    "signon.rememberSignons",
    TranslateYNtoTF },
#if 0
  // Firefox supplies its own home page.
  { 0,
    "Start Page",
     REG_SZ,
    "browser.startup.homepage",
    TranslateString },
#endif
  { 0, 
    "Anchor Underline",
    "browser.underline_anchors",
    TranslateYNtoTF },
  { 0,
    "Display Inline Images",
    "permissions.default.image",
    TranslateYNtoImageBehavior },
  { 0,
    "Enable AutoImageResize",
    "browser.enable_automatic_image_resizing",
    TranslateYNtoTF },
  { 0,
    "Move System Caret",
    "accessibility.browsewithcaret",
    TranslateYNtoTF },
  { 0,
    "NotifyDownloadComplete",
    "browser.download.manager.showAlertOnComplete",
    TranslateYNtoTF },
  { 0,
    "SmoothScroll",   // XXX DWORD 
    "general.smoothScroll",
    TranslateYNtoTF },
  { "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
    "EnableHttp1_1",
    "network.http.version",
    TranslateDWORDtoHTTPVersion },
  { 0,
    "ProxyHttp1.1",
    "network.http.proxy.version",
    TranslateDWORDtoHTTPVersion },
// SecureProtocols
  { "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings\\Url History",
    "DaysToKeep",
    "browser.history_expire_days",
    TranslateDWORDtoPRInt32 },
  { "Software\\Microsoft\\Internet Explorer\\Settings",
    "Always Use My Colors",            // XXX DWORD
    "browser.display.use_document_colors",
    TranslateYNtoFT },
  { 0,
    "Text Color",
    "browser.display.foreground_color",
    TranslateDRGBtoHRGB },
  { 0,
    "Background Color",
    "browser.display.background_color",
    TranslateDRGBtoHRGB },
  { 0,
    "Anchor Color",
    "browser.anchor_color",
    TranslateDRGBtoHRGB },
  { 0,
    "Anchor Color Visited",
    "browser.visited_color",
    TranslateDRGBtoHRGB },
  { 0,
    "Always Use My Font Face",    // XXX DWORD
    "browser.display.use_document_fonts",
    TranslateYNtoFT },
  { "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings\\Url History",
    "DaysToKeep",
    "browser.history_expire_days",
    TranslateDWORDtoPRInt32 }
};

#if 0
user_pref("font.size.fixed.x-western", 14);
user_pref("font.size.variable.x-western", 15);
#endif

///////////////////////////////////////////////////////////////////////////////
// nsIBrowserProfileMigrator
NS_IMETHODIMP
nsIEProfileMigrator::Migrate(PRUint16 aItems, nsIProfileStartup* aStartup, const PRUnichar* aProfile)
{
  nsresult rv = NS_OK;

  PRBool aReplace = PR_FALSE;

  if (aStartup) {
    aReplace = PR_TRUE;
    rv = aStartup->DoStartup();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NOTIFY_OBSERVERS(MIGRATION_STARTED, nsnull);

  COPY_DATA(CopyPreferences,  aReplace, nsIBrowserProfileMigrator::SETTINGS);
  COPY_DATA(CopyCookies,      aReplace, nsIBrowserProfileMigrator::COOKIES);
  COPY_DATA(CopyHistory,      aReplace, nsIBrowserProfileMigrator::HISTORY);
  COPY_DATA(CopyFormData,     aReplace, nsIBrowserProfileMigrator::FORMDATA);
  COPY_DATA(CopyPasswords,    aReplace, nsIBrowserProfileMigrator::PASSWORDS);
  COPY_DATA(CopyFavorites,    aReplace, nsIBrowserProfileMigrator::BOOKMARKS);

  NOTIFY_OBSERVERS(MIGRATION_ENDED, nsnull);

  return rv;
}

NS_IMETHODIMP
nsIEProfileMigrator::GetMigrateData(const PRUnichar* aProfile, 
                                    PRBool aReplace,
                                    PRUint16* aResult)
{
  // There's no harm in assuming everything is available.
  *aResult = nsIBrowserProfileMigrator::SETTINGS | nsIBrowserProfileMigrator::COOKIES | 
             nsIBrowserProfileMigrator::HISTORY | nsIBrowserProfileMigrator::FORMDATA |
             nsIBrowserProfileMigrator::PASSWORDS | nsIBrowserProfileMigrator::BOOKMARKS;
  return NS_OK;
}

NS_IMETHODIMP
nsIEProfileMigrator::GetSourceExists(PRBool* aResult)
{
  // IE always exists. 
  *aResult = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
nsIEProfileMigrator::GetSourceHasMultipleProfiles(PRBool* aResult)
{
  *aResult = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsIEProfileMigrator::GetSourceProfiles(nsISupportsArray** aResult)
{
  *aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsIEProfileMigrator::GetSourceHomePageURL(nsACString& aResult)
{
  nsCOMPtr<nsIWindowsRegKey> regKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  NS_NAMED_LITERAL_STRING(homeURLKey,
                          "Software\\Microsoft\\Internet Explorer\\Main");
  if (!regKey ||
      NS_FAILED(regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                             homeURLKey, nsIWindowsRegKey::ACCESS_READ)))
    return NS_OK;
  // read registry data
  NS_NAMED_LITERAL_STRING(homeURLValName, "Start Page");
  nsAutoString  homeURLVal;
  if (NS_SUCCEEDED(regKey->ReadStringValue(homeURLValName, homeURLVal))) {
    // Do we need this round-about way to get |homePageURL|? 
    // Perhaps, we do to have the form of URL under our control 
    // (cf. network.standard-url.escape-utf8)
    // Note that Windows stores URLs in IRI in the registry
    nsCAutoString  homePageURL;
    nsCOMPtr<nsIURI> homePageURI;

    if (NS_SUCCEEDED(NS_NewURI(getter_AddRefs(homePageURI), homeURLVal)))
        if (NS_SUCCEEDED(homePageURI->GetSpec(homePageURL)) 
            && !homePageURL.IsEmpty())
            aResult.Assign(homePageURL);
  }
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// nsIEProfileMigrator
NS_IMPL_ISUPPORTS1(nsIEProfileMigrator, nsIBrowserProfileMigrator);

nsIEProfileMigrator::nsIEProfileMigrator() 
{
  mObserverService = do_GetService("@mozilla.org/observer-service;1");
}

nsIEProfileMigrator::~nsIEProfileMigrator() 
{
}

nsresult
nsIEProfileMigrator::CopyHistory(PRBool aReplace) 
{
  nsCOMPtr<nsIBrowserHistory> hist(do_GetService(NS_GLOBALHISTORY2_CONTRACTID));
  nsCOMPtr<nsIIOService> ios(do_GetService(NS_IOSERVICE_CONTRACTID));

  // First, Migrate standard IE History entries...
  ::CoInitialize(NULL);

  IUrlHistoryStg2* ieHistory;
  HRESULT hr = ::CoCreateInstance(CLSID_CUrlHistory, 
                                  NULL,
                                  CLSCTX_INPROC_SERVER, 
                                  IID_IUrlHistoryStg2, 
                                  reinterpret_cast<void**>(&ieHistory));
  if (SUCCEEDED(hr)) {
    IEnumSTATURL* enumURLs;
    hr = ieHistory->EnumUrls(&enumURLs);
    if (SUCCEEDED(hr)) {
      STATURL statURL;
      ULONG fetched;
      _bstr_t url;
      nsCAutoString scheme;
      SYSTEMTIME st;
      PRBool validScheme = PR_FALSE;
      PRUnichar* tempTitle = nsnull;

      for (int count = 0; (hr = enumURLs->Next(1, &statURL, &fetched)) == S_OK; ++count) {
        if (statURL.pwcsUrl) {
          // 1 - Page Title
          tempTitle = statURL.pwcsTitle ? (PRUnichar*)((wchar_t*)(statURL.pwcsTitle)) : nsnull;

          // 2 - Last Visit Date
          ::FileTimeToSystemTime(&(statURL.ftLastVisited), &st);
          PRExplodedTime prt;
          prt.tm_year = st.wYear;
          prt.tm_month = st.wMonth - 1; // SYSTEMTIME's day-of-month parameter is 1-based, PRExplodedTime's is 0-based.
          prt.tm_mday = st.wDay;
          prt.tm_hour = st.wHour;
          prt.tm_min = st.wMinute;
          prt.tm_sec = st.wSecond;
          prt.tm_usec = st.wMilliseconds * 1000;
          prt.tm_wday = 0;
          prt.tm_yday = 0;
          prt.tm_params.tp_gmt_offset = 0;
          prt.tm_params.tp_dst_offset = 0;
          PRTime lastVisited = PR_ImplodeTime(&prt);

          // 3 - URL
          url = statURL.pwcsUrl;

          NS_ConvertUTF16toUTF8 urlStr(url);

          if (NS_FAILED(ios->ExtractScheme(urlStr, scheme))) {
            ::CoTaskMemFree(statURL.pwcsUrl);    
            if (statURL.pwcsTitle) 
              ::CoTaskMemFree(statURL.pwcsTitle);
            continue;
          }
          ToLowerCase(scheme);

          // XXXben - 
          // MSIE stores some types of URLs in its history that we can't handle, like HTMLHelp
          // and others. At present Necko isn't clever enough to delegate handling of these types
          // to the system, so we should just avoid importing them. 
          const char* schemes[] = { "http", "https", "ftp", "file" };
          for (int i = 0; i < 4; ++i) {
            if (validScheme = scheme.Equals(schemes[i]))
              break;
          }
          
          // 4 - Now add the page
          if (validScheme) {
            nsCOMPtr<nsIURI> uri;
            ios->NewURI(urlStr, nsnull, nsnull, getter_AddRefs(uri));
            if (uri) {
              if (tempTitle) 
                hist->AddPageWithDetails(uri, tempTitle, lastVisited);
              else
                hist->AddPageWithDetails(uri, url, lastVisited);
            }
          }
          ::CoTaskMemFree(statURL.pwcsUrl);    
        }
        if (statURL.pwcsTitle) 
          ::CoTaskMemFree(statURL.pwcsTitle);    
      }
      nsCOMPtr<nsIRDFRemoteDataSource> ds(do_QueryInterface(hist));
      if (ds)
        ds->Flush();
  
      enumURLs->Release();
    }

    ieHistory->Release();
  }
  ::CoUninitialize();

  // Now, find out what URLs were typed in by the user
  nsCOMPtr<nsIWindowsRegKey> regKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  NS_NAMED_LITERAL_STRING(typedURLKey,
                          "Software\\Microsoft\\Internet Explorer\\TypedURLs");
  if (regKey && 
      NS_SUCCEEDED(regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                                typedURLKey, nsIWindowsRegKey::ACCESS_READ))) {
    int offset = 0;

    while (1) {
      nsAutoString valueName;
      if (NS_FAILED(regKey->GetValueName(offset, valueName)))
        break;

      nsAutoString url; 
      if (Substring(valueName, 0, 3).EqualsLiteral("url") &&
          NS_SUCCEEDED(regKey->ReadStringValue(valueName, url))) {
        nsCOMPtr<nsIURI> uri;
        ios->NewURI(NS_ConvertUTF16toUTF8(url), nsnull, nsnull,
                    getter_AddRefs(uri));
        if (uri)
          hist->MarkPageAsTyped(uri);
      }
      ++offset;
    }
  }

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// IE PASSWORDS AND FORM DATA - A PROTECTED STORAGE SYSTEM PRIMER
//
// Internet Explorer 4.0 and up store sensitive form data (saved through form
// autocomplete) and saved passwords in a special area of the Registry called
// the Protected Storage System. The data IE stores in the Protected Storage 
// System is located under:
//
// HKEY_CURRENT_USER\Software\Microsoft\Protected Storage System Provider\
//   <USER_ID>\Data\<IE_PSS_GUID>\<IE_PSS_GUID>\
//
// <USER_ID> is a long string that uniquely identifies the current user
// <IE_PSS_GUID> is a GUID that identifies a subsection of the Protected Storage
// System specific to MSIE. This GUID is defined below ("IEPStoreAutocompGUID").
//
// Data is stored in the Protected Strage System ("PStore") in the following
// format:
// 
// <IE_PStore_Key> \
//                  fieldName1:StringData \ ItemData = <REG_BINARY>
//                  fieldName2:StringData \ ItemData = <REG_BINARY>
//                  http://foo.com/login.php:StringData \ ItemData = <REG_BINARY>
//                  ... etc ...
// 
// Each key represents either the identifier of a web page text field that 
// data was saved from (e.g. <input name="fieldName1">), or a URL that a login
// (username + password) was saved at (e.g. "http://foo.com/login.php")
//
// Data is stored for each of these cases in the following format:
//
// for both types of data, the Value ItemData is REG_BINARY data format encrypted with 
// a 3DES cipher. 
//
// for FormData: the decrypted data is in the form:
//                  value1\0value2\0value3\0value4 ...
// for Signons:  the decrypted data is in the form:
//                  username\0password
//
// We cannot read the PStore directly by using Registry functions because the 
// keys have limited read access such that only System process can read from them.
// In order to read from the PStore we need to use Microsoft's undocumented PStore
// API(*). 
//
// (* Sparse documentation became available as of the January 2004 MSDN Library
//    release)
//
// The PStore API lets us read decrypted data from the PStore. Encryption does not
// appear to be strong.
// 
// Details on how each type of data is read from the PStore and migrated appropriately
// is discussed in more detail below.
//
// For more information about the PStore, read:
//
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/devnotes/winprog/pstore.asp
//


typedef HRESULT (WINAPI *PStoreCreateInstancePtr)(IPStore**, DWORD, DWORD, DWORD);

struct SignonData {
  PRUnichar* user;
  PRUnichar* pass;
  char*      realm;
};

// IE PStore Type GUIDs
// {e161255a-37c3-11d2-bcaa-00c04fd929db} Autocomplete Password & Form Data
//                                        Subtype has same GUID
// {5e7e8100-9138-11d1-945a-00c04fc308ff} HTTP/FTP Auth Login Data
//                                        Subtype has a zero GUID
static GUID IEPStoreAutocompGUID = { 0xe161255a, 0x37c3, 0x11d2, { 0xbc, 0xaa, 0x00, 0xc0, 0x4f, 0xd9, 0x29, 0xdb } };
static GUID IEPStoreSiteAuthGUID = { 0x5e7e8100, 0x9138, 0x11d1, { 0x94, 0x5a, 0x00, 0xc0, 0x4f, 0xc3, 0x08, 0xff } };

///////////////////////////////////////////////////////////////////////////////
// IMPORTING AUTOCOMPLETE PASSWORDS
//
// This is tricky, and requires 2 passes through the subkeys in IE's PStore 
// section.
//
// First, we walk IE's PStore section, looking for subkeys that are prefixed
// with a URI. As mentioned above, we assume all such subkeys are stored 
// passwords.
//
//   http://foo.com/login.php:StringData   username\0password
//
// We can't add this item to the Password Manager just yet though. 
//
// The password manager requires the uniquifier of the username field (that is,
// the value of the "name" or "id" attribute) from the webpage so that it can
// prefill the value automatically. IE doesn't store this information with
// the password entry, but it DOES store this information independently as a 
// separate Form Data entry. 
//
// In other words, if you go to foo.com above and log in with "username" and
// "password" as your details in a form where the username field is uniquified
// as "un" (<input type="text" name="un">), when you login and elect to have IE 
// save the password for the site, the following TWO entries are created in IE's
// PStore section:
//
//   http://foo.com/login.php:StringData   username\0password
//   un:StringData                         username
//
// Thus to discover the field name for each login we need to first gather up 
// all the signons (collecting usernames in the process), then walk the list
// again, looking ONLY at non-URI prefixed subkeys, and searching for each
// username as a value in each such subkey's value list. If we have a match, 
// we assume that the subkey (with its uniquifier prefix) is a login field. 
//
// With this information, we call Password Manager's "AddUserFull" method 
// providing this detail. We don't need to provide the password field name, 
// we have no means of retrieving this info from IE, and the Password Manager
// knows to hunt for a password field near the login field if none is specified.
//
// IMPLICATIONS:
//  1) redundant signon entries for non-login forms might be created, but these
//     should be benign.
//  2) if the IE user ever clears his Form AutoComplete cache but doesn't clear
//     his passwords, we will be hosed, as we have no means of locating the
//     username field. Maybe someday the Password Manager will become 
//     artificially intelligent and be able to guess where the login fields are,
//     but I'm not holding my breath. 
//

nsresult
nsIEProfileMigrator::CopyPasswords(PRBool aReplace)
{
  HRESULT hr;
  nsresult rv;
  nsVoidArray signonsFound;

  HMODULE pstoreDLL = ::LoadLibrary("pstorec.dll");
  if (!pstoreDLL) {
    // XXXben TODO
    // Need to figure out what to do here on Windows 98 etc... it may be that the key is universal read
    // and we can just blunder into the registry and use CryptUnprotect to get the data out. 
    return NS_ERROR_FAILURE;
  }

  PStoreCreateInstancePtr PStoreCreateInstance = (PStoreCreateInstancePtr)::GetProcAddress(pstoreDLL, "PStoreCreateInstance");
  IPStorePtr PStore;
  hr = PStoreCreateInstance(&PStore, 0, 0, 0);

  rv = GetSignonsListFromPStore(PStore, &signonsFound);
  if (NS_SUCCEEDED(rv))
    ResolveAndMigrateSignons(PStore, &signonsFound);

  MigrateSiteAuthSignons(PStore);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// IMPORTING SITE AUTHENTICATION PASSWORDS
//
// This is simple and straightforward. We iterate through the part of the 
// PStore that matches the type GUID defined in IEPStoreSiteAuthGUID and
// a zero subtype GUID. For each item, we check the data for a ':' that
// separates the username and password parts. If there is no ':', we give up.
// After that, we check to see if the name of the item starts with "DPAPI:".
// We bail out if that's the case, because we can't handle those yet.
// However, if everything is all and well, we convert the itemName to a realm
// string that the password manager can work with and save this login
// via AddUser.

nsresult
nsIEProfileMigrator::MigrateSiteAuthSignons(IPStore* aPStore)
{
  HRESULT hr;

  NS_ENSURE_ARG_POINTER(aPStore);

  nsCOMPtr<nsIPasswordManager> pwmgr(do_GetService("@mozilla.org/passwordmanager;1"));
  if (!pwmgr)
    return NS_OK;

  GUID mtGuid = {0};
  IEnumPStoreItemsPtr enumItems = NULL;
  hr = aPStore->EnumItems(0, &IEPStoreSiteAuthGUID, &mtGuid, 0, &enumItems);
  if (SUCCEEDED(hr) && enumItems != NULL) {
    LPWSTR itemName = NULL;
    while ((enumItems->Next(1, &itemName, 0) == S_OK) && itemName) {
      unsigned long count = 0;
      unsigned char* data = NULL;

      hr = aPStore->ReadItem(0, &IEPStoreSiteAuthGUID, &mtGuid, itemName,
                             &count, &data, NULL, 0);
      if (SUCCEEDED(hr) && data) {
        unsigned long i;
        unsigned char* password = NULL;
        for (i = 0; i < count; i++)
          if (data[i] == ':') {
            data[i] = '\0';
            if (i + 1 < count)
              password = &data[i + 1];
            break;
          }

        nsAutoString realm(itemName);
        if (Substring(realm, 0, 6).EqualsLiteral("DPAPI:")) // often FTP logins
          password = NULL; // We can't handle these yet

        if (password) {
          int idx;
          idx = realm.FindChar('/');
          if (idx) {
            realm.Replace(idx, 1, NS_LITERAL_STRING(" ("));
            realm.Append(')');
          }
          // XXX: username and password are always ASCII in IPStore?
          // If not, are they in UTF-8 or the default codepage? (ref. bug 41489)
          pwmgr->AddUser(NS_ConvertUTF16toUTF8(realm),
                         NS_ConvertASCIItoUTF16((char *)data),
                         NS_ConvertASCIItoUTF16((char *)password));
        }
        ::CoTaskMemFree(data);
      }
    }
  }
  return NS_OK;
}

nsresult
nsIEProfileMigrator::GetSignonsListFromPStore(IPStore* aPStore, nsVoidArray* aSignonsFound)
{
  HRESULT hr;

  NS_ENSURE_ARG_POINTER(aPStore);

  IEnumPStoreItemsPtr enumItems = NULL;
  hr = aPStore->EnumItems(0, &IEPStoreAutocompGUID, &IEPStoreAutocompGUID, 0, &enumItems);
  if (SUCCEEDED(hr) && enumItems != NULL) {
    LPWSTR itemName = NULL;
    while ((enumItems->Next(1, &itemName, 0) == S_OK) && itemName) {
      unsigned long count = 0;
      unsigned char* data = NULL;

      // We are responsible for freeing |data| using |CoTaskMemFree|!!
      // But we don't do it here... 
      hr = aPStore->ReadItem(0, &IEPStoreAutocompGUID, &IEPStoreAutocompGUID, itemName, &count, &data, NULL, 0);
      if (SUCCEEDED(hr) && data) {
        nsAutoString itemNameString(itemName);
        if (StringTail(itemNameString, 11).
            LowerCaseEqualsLiteral(":stringdata")) {
          // :StringData contains the saved data
          const nsAString& key = Substring(itemNameString, 0, itemNameString.Length() - 11);
          char* realm = nsnull;
          if (KeyIsURI(key, &realm)) {
            // This looks like a URL and could be a password. If it has username and password data, then we'll treat
            // it as one and add it to the password manager
            unsigned char* username = NULL;
            unsigned char* pass = NULL;
            GetUserNameAndPass(data, count, &username, &pass);

            if (username && pass) {
              // username and pass are pointers into the data buffer allocated by IPStore's ReadItem
              // method, and we own that buffer. We don't free it here, since we're going to be using 
              // it after the password harvesting stage to locate the username field. Only after the second
              // phase is complete do we free the buffer. 
              SignonData* d = new SignonData;
              if (!d)
                return NS_ERROR_OUT_OF_MEMORY;
              d->user = (PRUnichar*)username;
              d->pass = (PRUnichar*)pass;
              d->realm = realm; // freed in ResolveAndMigrateSignons
              aSignonsFound->AppendElement(d);
            }
          }
        }
      }
    }
  }
  return NS_OK;
}

PRBool
nsIEProfileMigrator::KeyIsURI(const nsAString& aKey, char** aRealm)
{
  *aRealm = nsnull;

  nsCOMPtr<nsIURI> uri;

  if (NS_FAILED(NS_NewURI(getter_AddRefs(uri), aKey))) 
    return PR_FALSE;

  PRBool validScheme = PR_FALSE;
  const char* schemes[] = { "http", "https" };
  for (int i = 0; i < 2; ++i) {
    uri->SchemeIs(schemes[i], &validScheme);
    if (validScheme) {
      nsCAutoString realm;
      uri->GetScheme(realm);
      realm.AppendLiteral("://");

      nsCAutoString host;
      uri->GetHost(host);
      realm.Append(host);

      *aRealm = ToNewCString(realm);
      return validScheme;
    }
  }
  return PR_FALSE;
}

nsresult
nsIEProfileMigrator::ResolveAndMigrateSignons(IPStore* aPStore, nsVoidArray* aSignonsFound)
{
  HRESULT hr;

  IEnumPStoreItemsPtr enumItems = NULL;
  hr = aPStore->EnumItems(0, &IEPStoreAutocompGUID, &IEPStoreAutocompGUID, 0, &enumItems);
  if (SUCCEEDED(hr) && enumItems != NULL) {
    LPWSTR itemName = NULL;
    while ((enumItems->Next(1, &itemName, 0) == S_OK) && itemName) {
      unsigned long count = 0;
      unsigned char* data = NULL;

      hr = aPStore->ReadItem(0, &IEPStoreAutocompGUID, &IEPStoreAutocompGUID, itemName, &count, &data, NULL, 0);
      if (SUCCEEDED(hr) && data) {
        nsAutoString itemNameString(itemName);
        if (StringTail(itemNameString, 11).
            LowerCaseEqualsLiteral(":stringdata")) {
          // :StringData contains the saved data
          const nsAString& key = Substring(itemNameString, 0, itemNameString.Length() - 11);
          
          // Assume all keys that are valid URIs are signons, not saved form data, and that 
          // all keys that aren't valid URIs are form field names (containing form data).
          nsCString realm;
          if (!KeyIsURI(key, getter_Copies(realm))) {
            // Search the data for a username that matches one of the found signons. 
            EnumerateUsernames(key, (PRUnichar*)data, (count/sizeof(PRUnichar)), aSignonsFound);
          }
        }

        ::CoTaskMemFree(data);
      }
    }
    // Now that we've done resolving signons, we need to walk the signons list, freeing the data buffers 
    // for each SignonData entry, since these buffers were allocated by the system back in |GetSignonListFromPStore|
    // but never freed. 
    PRInt32 signonCount = aSignonsFound->Count();
    for (PRInt32 i = 0; i < signonCount; ++i) {
      SignonData* sd = (SignonData*)aSignonsFound->ElementAt(i);
      ::CoTaskMemFree(sd->user);  // |sd->user| is a pointer to the start of a buffer that also contains sd->pass
      NS_Free(sd->realm);
      delete sd;
    }
  }
  return NS_OK;
}

void
nsIEProfileMigrator::EnumerateUsernames(const nsAString& aKey, PRUnichar* aData, unsigned long aCount, nsVoidArray* aSignonsFound)
{
  nsCOMPtr<nsIPasswordManagerInternal> pwmgr(do_GetService("@mozilla.org/passwordmanager;1"));
  if (!pwmgr)
    return;

  PRUnichar* cursor = aData;
  PRInt32 offset = 0;
  PRInt32 signonCount = aSignonsFound->Count();

  while (offset < aCount) {
    nsAutoString curr; curr = cursor;

    // Compare the value at the current cursor position with the collected list of signons
    for (PRInt32 i = 0; i < signonCount; ++i) {
      SignonData* sd = (SignonData*)aSignonsFound->ElementAt(i);
      if (curr.Equals(sd->user)) {
        // Bingo! Found a username in the saved data for this item. Now, add a Signon.
        nsDependentString usernameStr(sd->user), passStr(sd->pass);
        nsDependentCString realm(sd->realm);
        pwmgr->AddUserFull(realm, usernameStr, passStr, aKey, EmptyString());
      }
    }

    // Advance the cursor
    PRInt32 advance = curr.Length() + 1;
    cursor += advance; // Advance to next string (length of curr string + 1 PRUnichar for null separator)
    offset += advance;
  } 
}

void 
nsIEProfileMigrator::GetUserNameAndPass(unsigned char* data, unsigned long len, unsigned char** username, unsigned char** pass)
{
  *username = data;
  *pass = NULL;

  unsigned char* temp = data; 

  for (unsigned int i = 0; i < len; i += 2, temp += 2*sizeof(unsigned char)) {
    if (*temp == '\0') {
      *pass = temp + 2*sizeof(unsigned char);
      break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// IMPORTING FORM DATA
//
// This is a much simpler task as all we need is the field name and that's part
// of the key used to identify each data set in the PStore. The algorithm here
// is as follows:
//
//   fieldName1:StringData    value1\0value2\0value3\0
//   fieldName2:StringData    value1\0value2\0value3\0
//   fieldName3:StringData    value1\0value2\0value3\0
//
// Walk each non-URI prefixed key in IE's PStore section, split the value provided
// into chunks (\0 delimited) and use nsIFormHistory2's |addEntry| method to add
// an entry for the fieldName prefix and each value. 
//
// "Quite Easily Done". ;-)
// 
nsresult
nsIEProfileMigrator::CopyFormData(PRBool aReplace)
{
  HRESULT hr;

  HMODULE pstoreDLL = ::LoadLibrary("pstorec.dll");
  if (!pstoreDLL) {
    // XXXben TODO
    // Need to figure out what to do here on Windows 98 etc... it may be that the key is universal read
    // and we can just blunder into the registry and use CryptUnprotect to get the data out. 
    return NS_ERROR_FAILURE;
  }

  PStoreCreateInstancePtr PStoreCreateInstance = (PStoreCreateInstancePtr)::GetProcAddress(pstoreDLL, "PStoreCreateInstance");
  IPStorePtr PStore = NULL;
  hr = PStoreCreateInstance(&PStore, 0, 0, 0);
  if (FAILED(hr) || PStore == NULL)
    return NS_OK;

  IEnumPStoreItemsPtr enumItems = NULL;
  hr = PStore->EnumItems(0, &IEPStoreAutocompGUID, &IEPStoreAutocompGUID, 0, &enumItems);
  if (SUCCEEDED(hr) && enumItems != NULL) {
    LPWSTR itemName = NULL;
    while ((enumItems->Next(1, &itemName, 0) == S_OK) && itemName) {
      unsigned long count = 0;
      unsigned char* data = NULL;

      // We are responsible for freeing |data| using |CoTaskMemFree|!!
      hr = PStore->ReadItem(0, &IEPStoreAutocompGUID, &IEPStoreAutocompGUID, itemName, &count, &data, NULL, 0);
      if (SUCCEEDED(hr) && data) {
        nsAutoString itemNameString(itemName);
        if (StringTail(itemNameString, 11).
            LowerCaseEqualsLiteral(":stringdata")) {
          // :StringData contains the saved data
          const nsAString& key = Substring(itemNameString, 0, itemNameString.Length() - 11);
          nsCString realm;
          if (!KeyIsURI(key, getter_Copies(realm))) {
            nsresult rv = AddDataToFormHistory(key, (PRUnichar*)data, (count/sizeof(PRUnichar)));
            if (NS_FAILED(rv)) return rv;
          }
        }
      }
    }
  }
  return NS_OK;
}

nsresult
nsIEProfileMigrator::AddDataToFormHistory(const nsAString& aKey, PRUnichar* aData, unsigned long aCount)
{
  nsCOMPtr<nsIFormHistory2> formHistory(do_GetService("@mozilla.org/satchel/form-history;1"));
  if (!formHistory)
    return NS_ERROR_OUT_OF_MEMORY;

  PRUnichar* cursor = aData;
  PRInt32 offset = 0;

  while (offset < aCount) {
    nsAutoString curr; curr = cursor;

    formHistory->AddEntry(aKey, curr);

    // Advance the cursor
    PRInt32 advance = curr.Length() + 1;
    cursor += advance; // Advance to next string (length of curr string + 1 PRUnichar for null separator)
    offset += advance;
  } 

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
//
// favorites
// search keywords
//

nsresult
nsIEProfileMigrator::CopyFavorites(PRBool aReplace) {
  // If "aReplace" is true, merge into the root level of bookmarks. Otherwise, create
  // a folder called "Imported IE Favorites" and place all the Bookmarks there. 
  nsresult rv;

#ifdef MOZ_PLACES_BOOKMARKS
  nsCOMPtr<nsINavBookmarksService> bms(do_GetService(NS_NAVBOOKMARKSSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt64 root;
  rv = bms->GetBookmarksRoot(&root);
  NS_ENSURE_SUCCESS(rv, rv);
#else
  nsCOMPtr<nsIRDFService> rdf(do_GetService("@mozilla.org/rdf/rdf-service;1"));
  nsCOMPtr<nsIRDFResource> root;
  rdf->GetResource(NS_LITERAL_CSTRING("NC:BookmarksRoot"), getter_AddRefs(root));

  nsCOMPtr<nsIBookmarksService> bms(do_GetService("@mozilla.org/browser/bookmarks-service;1"));
  NS_ENSURE_TRUE(bms, NS_ERROR_FAILURE);
  PRBool dummy;
  bms->ReadBookmarks(&dummy);
#endif

  nsAutoString personalToolbarFolderName;

#ifdef MOZ_PLACES_BOOKMARKS
  PRInt64 folder;
#else
  nsCOMPtr<nsIRDFResource> folder;
#endif
  if (!aReplace) {
    nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIStringBundle> bundle;
    bundleService->CreateBundle(TRIDENTPROFILE_BUNDLE, getter_AddRefs(bundle));

    nsString sourceNameIE;
    bundle->GetStringFromName(NS_LITERAL_STRING("sourceNameIE").get(), 
                              getter_Copies(sourceNameIE));

    const PRUnichar* sourceNameStrings[] = { sourceNameIE.get() };
    nsString importedIEFavsTitle;
    bundle->FormatStringFromName(NS_LITERAL_STRING("importedBookmarksFolder").get(),
                                 sourceNameStrings, 1, getter_Copies(importedIEFavsTitle));

#ifdef MOZ_PLACES_BOOKMARKS
    bms->CreateFolder(root, importedIEFavsTitle, -1, &folder);
#else
    bms->CreateFolderInContainer(importedIEFavsTitle.get(), root, -1, getter_AddRefs(folder));
#endif
  }
  else {
    // Locate the Links toolbar folder, we want to replace the Personal Toolbar content with 
    // Favorites in this folder. 
    nsCOMPtr<nsIWindowsRegKey> regKey = 
      do_CreateInstance("@mozilla.org/windows-registry-key;1");
    NS_NAMED_LITERAL_STRING(toolbarKey,
                            "Software\\Microsoft\\Internet Explorer\\Toolbar");
    if (regKey && 
        NS_SUCCEEDED(regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                                  toolbarKey, nsIWindowsRegKey::ACCESS_READ))) {
      nsAutoString linksFolderName;
      if (NS_SUCCEEDED(regKey->ReadStringValue(
                       NS_LITERAL_STRING("LinksFolderName"),
                       linksFolderName)))
        personalToolbarFolderName = linksFolderName; 
    }
    folder = root;
  }

  nsCOMPtr<nsIProperties> fileLocator(do_GetService("@mozilla.org/file/directory_service;1", &rv));
  if (NS_FAILED(rv)) 
      return rv;

  nsCOMPtr<nsIFile> favoritesDirectory;
  fileLocator->Get("Favs", NS_GET_IID(nsIFile), getter_AddRefs(favoritesDirectory));

  // If |favoritesDirectory| is null, it means that we're on a Windows 
  // platform that does not have a Favorites folder, e.g. Windows 95 
  // (early SRs, before IE integrated with the shell). Only try to 
  // read Favorites folder if it exists on the machine. 
  if (favoritesDirectory) {
    rv = ParseFavoritesFolder(favoritesDirectory, folder, bms, personalToolbarFolderName, PR_TRUE);
    if (NS_FAILED(rv)) return rv;
  }

  return CopySmartKeywords(root);
}

nsresult
#ifdef MOZ_PLACES_BOOKMARKS
nsIEProfileMigrator::CopySmartKeywords(PRInt64 aParentFolder)
#else
nsIEProfileMigrator::CopySmartKeywords(nsIRDFResource* aParentFolder)
#endif
{ 
  nsCOMPtr<nsIWindowsRegKey> regKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  NS_NAMED_LITERAL_STRING(searchUrlKey,
                          "Software\\Microsoft\\Internet Explorer\\SearchUrl");
  if (regKey && 
      NS_SUCCEEDED(regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                                searchUrlKey, nsIWindowsRegKey::ACCESS_READ))) {

#ifdef MOZ_PLACES_BOOKMARKS
    nsresult rv;
    nsCOMPtr<nsINavBookmarksService> bms(do_GetService(NS_NAVBOOKMARKSSERVICE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    PRInt64 keywordsFolder = 0;
#else
    nsCOMPtr<nsIBookmarksService> bms(do_GetService("@mozilla.org/browser/bookmarks-service;1"));
    nsCOMPtr<nsIRDFResource> keywordsFolder, bookmark;
#endif

    nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    
    nsCOMPtr<nsIStringBundle> bundle;
    bundleService->CreateBundle(TRIDENTPROFILE_BUNDLE, getter_AddRefs(bundle));


    int offset = 0;
    while (1) {
      nsAutoString keyName;
      if (NS_FAILED(regKey->GetChildName(offset, keyName)))
        break;

      if (!keywordsFolder) {
        nsString sourceNameIE;
        bundle->GetStringFromName(NS_LITERAL_STRING("sourceNameIE").get(), 
                                  getter_Copies(sourceNameIE));

        const PRUnichar* sourceNameStrings[] = { sourceNameIE.get() };
        nsString importedIESearchUrlsTitle;
        bundle->FormatStringFromName(NS_LITERAL_STRING("importedSearchURLsFolder").get(),
                                    sourceNameStrings, 1, getter_Copies(importedIESearchUrlsTitle));
#ifdef MOZ_PLACES_BOOKMARKS
        bms->CreateFolder(aParentFolder, importedIESearchUrlsTitle, -1, &keywordsFolder);
#else
        bms->CreateFolderInContainer(importedIESearchUrlsTitle.get(), aParentFolder, -1, 
                                     getter_AddRefs(keywordsFolder));
#endif
      }

      nsCOMPtr<nsIWindowsRegKey> childKey; 
      if (NS_SUCCEEDED(regKey->OpenChild(keyName,
                       nsIWindowsRegKey::ACCESS_READ,
                       getter_AddRefs(childKey)))) {
        nsAutoString url; 
        if (NS_SUCCEEDED(childKey->ReadStringValue(EmptyString(), url))) {
          nsCOMPtr<nsIURI> uri;
          if (NS_FAILED(NS_NewURI(getter_AddRefs(uri), url))) {
            NS_WARNING("Invalid url while importing smart keywords of MS IE");
            ++offset;
            childKey->Close();
            continue;
          }
#ifdef MOZ_PLACES_BOOKMARKS
          PRInt64 id;
          bms->InsertItem(keywordsFolder, uri,
                          nsINavBookmarksService::DEFAULT_INDEX, &id);
          bms->SetItemTitle(id, keyName);
#else
          nsCAutoString hostCStr;
          uri->GetHost(hostCStr);
          NS_ConvertUTF8toUTF16 host(hostCStr); 

          const PRUnichar* nameStrings[] = { host.get() };
          nsString keywordName;
          nsresult rv = bundle->FormatStringFromName(
                        NS_LITERAL_STRING("importedSearchURLsTitle").get(),
                        nameStrings, 1, getter_Copies(keywordName));

          const PRUnichar* descStrings[] = { keyName.get(), host.get() };
          nsString keywordDesc;
          rv = bundle->FormatStringFromName(
                       NS_LITERAL_STRING("importedSearchUrlDesc").get(),
                       descStrings, 2, getter_Copies(keywordDesc));
          bms->CreateBookmarkInContainer(keywordName.get(), 
                                         url.get(),
                                         keyName.get(), 
                                         keywordDesc.get(), 
                                         nsnull,
                                         nsnull, 
                                         keywordsFolder, 
                                         -1, 
                                         getter_AddRefs(bookmark));
#endif
        }
        childKey->Close();
      }

      ++offset;
    }
  }

  return NS_OK;
}

void 
nsIEProfileMigrator::ResolveShortcut(const nsString &aFileName, char** aOutURL) 
{
  HRESULT result;

  IUniformResourceLocator* urlLink = nsnull;
  result = ::CoCreateInstance(CLSID_InternetShortcut, NULL, CLSCTX_INPROC_SERVER,
                              IID_IUniformResourceLocator, (void**)&urlLink);
  if (SUCCEEDED(result) && urlLink) {
    IPersistFile* urlFile = nsnull;
    result = urlLink->QueryInterface(IID_IPersistFile, (void**)&urlFile);
    if (SUCCEEDED(result) && urlFile) {
      result = urlFile->Load(aFileName.get(), STGM_READ);
      if (SUCCEEDED(result) ) {
        LPSTR lpTemp = nsnull;
        result = urlLink->GetURL(&lpTemp);
        if (SUCCEEDED(result) && lpTemp) {
          *aOutURL = PL_strdup(lpTemp);

          // free the string that GetURL alloc'd
          ::CoTaskMemFree(lpTemp);
        }
      }
      urlFile->Release();
    }
    urlLink->Release();
  }
}

nsresult
nsIEProfileMigrator::ParseFavoritesFolder(nsIFile* aDirectory, 
#ifdef MOZ_PLACES_BOOKMARKS
                                          PRInt64 aParentFolder,
                                          nsINavBookmarksService* aBookmarksService,
#else
                                          nsIRDFResource* aParentResource,
                                          nsIBookmarksService* aBookmarksService,
#endif 
                                          const nsAString& aPersonalToolbarFolderName,
                                          PRBool aIsAtRootLevel)
{
  nsresult rv;

  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_FAILED(rv)) return rv;

  do {
    PRBool hasMore = PR_FALSE;
    rv = entries->HasMoreElements(&hasMore);
    if (NS_FAILED(rv) || !hasMore) break;

    nsCOMPtr<nsISupports> supp;
    rv = entries->GetNext(getter_AddRefs(supp));
    if (NS_FAILED(rv)) break;

    nsCOMPtr<nsIFile> currFile(do_QueryInterface(supp));

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewFileURI(getter_AddRefs(uri), currFile);
    if (NS_FAILED(rv)) break;

    nsAutoString bookmarkName;
    currFile->GetLeafName(bookmarkName);

    PRBool isSymlink = PR_FALSE;
    PRBool isDir = PR_FALSE;

    currFile->IsSymlink(&isSymlink);
    currFile->IsDirectory(&isDir);

    if (isSymlink) {
      // It's a .lnk file.  Get the path and check to see if it's
      // a dir.  If so, create a bookmark for the dir.  If not, then
      // simply do nothing and continue.

      // Get the path that the .lnk file is pointing to.
      nsAutoString path;
      rv = currFile->GetTarget(path);
      if (NS_FAILED(rv)) continue;

      nsCOMPtr<nsILocalFile> localFile;
      rv = NS_NewLocalFile(path, PR_TRUE, getter_AddRefs(localFile));
      if (NS_FAILED(rv)) continue;

      // Check for dir here.  If path is not a dir, just continue with
      // next import.
      rv = localFile->IsDirectory(&isDir);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!isDir) continue;

      // Look for and strip out the .lnk extension.
      NS_NAMED_LITERAL_STRING(lnkExt, ".lnk");
      PRInt32 lnkExtStart = bookmarkName.Length() - lnkExt.Length();
      if (StringEndsWith(bookmarkName, lnkExt,
                         CaseInsensitiveCompare))
        bookmarkName.SetLength(lnkExtStart);

#ifdef MOZ_PLACES_BOOKMARKS
      nsCOMPtr<nsIURI> bookmarkURI;
      rv = NS_NewFileURI(getter_AddRefs(bookmarkURI), localFile);
      NS_ENSURE_SUCCESS(rv, rv);
      PRInt64 id;
      rv = aBookmarksService->InsertItem(aParentFolder, bookmarkURI,
                                         nsINavBookmarksService::DEFAULT_INDEX, &id);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aBookmarksService->SetItemTitle(id, bookmarkName);
      NS_ENSURE_SUCCESS(rv, rv);
#else
      nsCAutoString spec;
      nsCOMPtr<nsIFile> filePath(localFile);
      // Get the file url format (file:///...) of the native file path.
      rv = NS_GetURLSpecFromFile(filePath, spec);
      if (NS_FAILED(rv)) continue;

      nsCOMPtr<nsIRDFResource> bookmark;
      // Here it's assumed that NS_GetURLSpecFromFile returns spec in UTF-8.
      // It is very likely to be ASCII (with everything escaped beyond file://),
      // but we don't lose much assuming that it's UTF-8. This is not perf.
      // critical.
      aBookmarksService->CreateBookmarkInContainer(bookmarkName.get(), 
                                                   NS_ConvertUTF8toUTF16(spec).get(), 
                                                   nsnull,
                                                   nsnull, 
                                                   nsnull, 
                                                   nsnull, 
                                                   aParentResource, 
                                                   -1, 
                                                   getter_AddRefs(bookmark));
#endif
      if (NS_FAILED(rv)) continue;
    }
    else if (isDir) {
#ifdef MOZ_PLACES_BOOKMARKS
      PRInt64 folder;
#else
      nsCOMPtr<nsIRDFResource> folder;
#endif
      if (bookmarkName.Equals(aPersonalToolbarFolderName)) {
#ifdef MOZ_PLACES_BOOKMARKS
        aBookmarksService->GetToolbarRoot(&folder);
        // If we're here, it means the user's doing a _replace_ import which means
        // clear out the content of this folder, and replace it with the new content
        aBookmarksService->RemoveFolderChildren(folder);
#else
        aBookmarksService->GetBookmarksToolbarFolder(getter_AddRefs(folder));
        
        // If we're here, it means the user's doing a _replace_ import which means
        // clear out the content of this folder, and replace it with the new content
        nsCOMPtr<nsIRDFContainer> ctr(do_CreateInstance("@mozilla.org/rdf/container;1"));
        nsCOMPtr<nsIRDFDataSource> bmds(do_QueryInterface(aBookmarksService));
        ctr->Init(bmds, folder);

        nsCOMPtr<nsISimpleEnumerator> e;
        ctr->GetElements(getter_AddRefs(e));

        PRBool hasMore;
        e->HasMoreElements(&hasMore);
        while (hasMore) {
          nsCOMPtr<nsIRDFResource> b;
          e->GetNext(getter_AddRefs(b));

          ctr->RemoveElement(b, PR_FALSE);

          e->HasMoreElements(&hasMore);
        }
#endif
      }
      else {
#ifdef MOZ_PLACES_BOOKMARKS
        rv = aBookmarksService->CreateFolder(aParentFolder,
                                             bookmarkName,
                                             nsINavBookmarksService::DEFAULT_INDEX,
                                             &folder);
#else
        rv = aBookmarksService->CreateFolderInContainer(bookmarkName.get(), 
                                                        aParentResource, 
                                                        -1, 
                                                        getter_AddRefs(folder));
#endif
        if (NS_FAILED(rv)) continue;
      }

      rv = ParseFavoritesFolder(currFile, folder, aBookmarksService, aPersonalToolbarFolderName, PR_FALSE);
      if (NS_FAILED(rv)) continue;
    }
    else {
      nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
      nsCAutoString extension;

      url->GetFileExtension(extension);
      if (!extension.Equals("url", CaseInsensitiveCompare))
        continue;

      nsAutoString name(Substring(bookmarkName, 0, 
                                  bookmarkName.Length() - extension.Length() - 1));

      nsAutoString path;
      currFile->GetPath(path);

      nsCString resolvedURL;
      ResolveShortcut(path, getter_Copies(resolvedURL));

#ifdef MOZ_PLACES_BOOKMARKS
      nsCOMPtr<nsIURI> resolvedURI;
      rv = NS_NewURI(getter_AddRefs(resolvedURI), resolvedURL);
      NS_ENSURE_SUCCESS(rv, rv);
      PRInt64 id;
      rv = aBookmarksService->InsertItem(aParentFolder, resolvedURI,
                                         nsINavBookmarksService::DEFAULT_INDEX, &id);
      if (NS_FAILED(rv)) continue;
      rv = aBookmarksService->SetItemTitle(id, name);
#else
      nsCOMPtr<nsIRDFResource> bookmark;
      // As far as I can tell reading the MSDN API document,
      // IUniformResourceLocator::GetURL (used by ResolveShortcut) returns a 
      // URL in ASCII (with non-ASCII characters escaped) and it doesn't yet
      // support IDN (i18n) hostname. However, it may in the future so that
      // using UTF8toUTF16 wouldn't be a bad idea.
      rv = aBookmarksService->CreateBookmarkInContainer(name.get(), 
                                                        NS_ConvertUTF8toUTF16(resolvedURL).get(), 
                                                        nsnull, 
                                                        nsnull, 
                                                        nsnull, 
                                                        nsnull, 
                                                        aParentResource, 
                                                        -1, 
                                                        getter_AddRefs(bookmark));
#endif
      if (NS_FAILED(rv)) continue;
    }
  }
  while (1);

  return rv;
}

nsresult
nsIEProfileMigrator::CopyPreferences(PRBool aReplace) 
{
  PRBool          regKeyOpen = PR_FALSE;
  const regEntry  *entry,
                  *endEntry = gRegEntries + NS_ARRAY_LENGTH(gRegEntries);
                              

  nsCOMPtr<nsIPrefBranch> prefs;

  { // scope pserve why not
    nsCOMPtr<nsIPrefService> pserve(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (pserve)
      pserve->GetBranch("", getter_AddRefs(prefs));
  }
  if (!prefs)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIWindowsRegKey> regKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey)
    return NS_ERROR_UNEXPECTED;
  
  // step through gRegEntries table
  for (entry = gRegEntries; entry < endEntry; ++entry) {

    // a new keyname? close any previous one and open the new one
    if (entry->regKeyName) {
      if (regKeyOpen) {
        regKey->Close();
        regKeyOpen = PR_FALSE;
      }
      regKeyOpen = NS_SUCCEEDED(regKey->
                                Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                                NS_ConvertASCIItoUTF16(
                                  nsDependentCString(entry->regKeyName)),
                                nsIWindowsRegKey::ACCESS_READ));
    }

    if (regKeyOpen) 
      // read registry data
      entry->entryHandler(regKey,
                          NS_ConvertASCIItoUTF16(
                            nsDependentCString(entry->regValueName)),
                          prefs, entry->prefKeyName);
  }

  nsresult rv = CopySecurityPrefs(prefs);
  if (NS_FAILED(rv)) return rv;

  rv = CopyProxyPreferences(prefs);
  if (NS_FAILED(rv)) return rv;

  return CopyStyleSheet(aReplace);
}

/* Fetch and translate the current user's cookies.
   Return true if successful. */
nsresult
nsIEProfileMigrator::CopyCookies(PRBool aReplace) 
{
  // IE cookies are stored in files named <username>@domain[n].txt
  // (in <username>'s Cookies folder. isn't the naming redundant?)
  PRBool rv = NS_OK;

  nsCOMPtr<nsIFile> cookiesDir;
  nsCOMPtr<nsISimpleEnumerator> cookieFiles;

  nsCOMPtr<nsICookieManager2> cookieManager(do_GetService(NS_COOKIEMANAGER_CONTRACTID));
  if (!cookieManager)
    return NS_ERROR_FAILURE;

  // find the cookies directory
  NS_GetSpecialDirectory(NS_WIN_COOKIES_DIR, getter_AddRefs(cookiesDir));
  if (cookiesDir)
    cookiesDir->GetDirectoryEntries(getter_AddRefs(cookieFiles));
  if (!cookieFiles)
    return NS_ERROR_FAILURE;

  // fetch the current user's name from the environment
  PRUnichar username[sUsernameLengthLimit+2];
  ::GetEnvironmentVariableW(L"USERNAME", username,
                            sizeof(username)/sizeof(PRUnichar));
  username[sUsernameLengthLimit] = L'\0';
  wcscat(username, L"@");
  int usernameLength = wcslen(username);

  // allocate a buffer into which to read each cookie file
  char *fileContents = (char *) PR_Malloc(sInitialCookieBufferSize);
  if (!fileContents)
    return NS_ERROR_OUT_OF_MEMORY;
  PRUint32 fileContentsSize = sInitialCookieBufferSize;

  do { // for each file in the cookies directory
    // get the next file
    PRBool moreFiles = PR_FALSE;
    if (NS_FAILED(cookieFiles->HasMoreElements(&moreFiles)) || !moreFiles)
      break;

    nsCOMPtr<nsISupports> supFile;
    cookieFiles->GetNext(getter_AddRefs(supFile));
    nsCOMPtr<nsIFile> cookieFile(do_QueryInterface(supFile));
    if (!cookieFile) {
      rv = NS_ERROR_FAILURE;
      break; // unexpected! punt!
    }

    // is it a cookie file for the current user?
    nsAutoString fileName;
    cookieFile->GetLeafName(fileName);
    const nsAString &fileOwner = Substring(fileName, 0, usernameLength);
    if (!fileOwner.Equals(username, CaseInsensitiveCompare))
      continue;

    // ensure the contents buffer is large enough to hold the entire file
    // plus one byte (see DelimitField())
    PRInt64 llFileSize;
    if (NS_FAILED(cookieFile->GetFileSize(&llFileSize)))
      continue;

    PRUint32 fileSize, readSize;
    LL_L2UI(fileSize, llFileSize);
    if (fileSize >= fileContentsSize) {
      PR_Free(fileContents);
      fileContents = (char *) PR_Malloc(fileSize+1);
      if (!fileContents) {
        rv = NS_ERROR_FAILURE;
        break; // fatal error
      }
      fileContentsSize = fileSize;
    }

    // read the entire cookie file
    PRFileDesc *fd;
    nsCOMPtr<nsILocalFile> localCookieFile(do_QueryInterface(cookieFile));
    if (localCookieFile &&
        NS_SUCCEEDED(localCookieFile->OpenNSPRFileDesc(PR_RDONLY, 0444, &fd))) {

      readSize = PR_Read(fd, fileContents, fileSize);
      PR_Close(fd);

      if (fileSize == readSize) { // translate this file's cookies
        nsresult onerv;
        onerv = CopyCookiesFromBuffer(fileContents, readSize, cookieManager);
        if (NS_FAILED(onerv))
          rv = onerv;
      }
    }
  } while(1);

  if (fileContents)
    PR_Free(fileContents);
  return rv;
}

/* Fetch cookies from a single IE cookie file.
   Return true if successful. */
nsresult
nsIEProfileMigrator::CopyCookiesFromBuffer(char *aBuffer,
                                           PRUint32 aBufferLength,
                                           nsICookieManager2 *aCookieManager) 
{
  nsresult  rv = NS_OK;

  const char *bufferEnd = aBuffer + aBufferLength;
  // cookie fields:
  char    *name,
          *value,
          *host,
          *path,
          *flags,
          *expirationDate1, *expirationDate2,
          *creationDate1, *creationDate2,
          *terminator;
  int      flagsValue;
  time_t   expirationDate,
           creationDate;
  char     hostCopy[sHostnameLengthLimit+1],
          *hostCopyConstructor,
          *hostCopyEnd = hostCopy + sHostnameLengthLimit;

  do { // for each cookie in the buffer
    DelimitField(&aBuffer, bufferEnd, &name);
    DelimitField(&aBuffer, bufferEnd, &value);
    DelimitField(&aBuffer, bufferEnd, &host);
    DelimitField(&aBuffer, bufferEnd, &flags);
    DelimitField(&aBuffer, bufferEnd, &expirationDate1);
    DelimitField(&aBuffer, bufferEnd, &expirationDate2);
    DelimitField(&aBuffer, bufferEnd, &creationDate1);
    DelimitField(&aBuffer, bufferEnd, &creationDate2);
    DelimitField(&aBuffer, bufferEnd, &terminator);

    // it's a cookie if we got one of each
    if (terminator >= bufferEnd)
      break;

    // IE stores deleted cookies with a zero-length value
    if (*value == '\0')
      continue;

    // convert flags to an int, date numbers to useable dates
    ::sscanf(flags, "%d", &flagsValue);
    expirationDate = FileTimeToTimeT(expirationDate1, expirationDate2);
    creationDate = FileTimeToTimeT(creationDate1, creationDate2);

    // munge host, and separate host from path

    hostCopyConstructor = hostCopy;

    // first, with a non-null domain, assume it's what Mozilla considers
    // a domain cookie. see bug 222343.
    if (*host && *host != '.' && *host != '/')
      *hostCopyConstructor++ = '.';

    // copy the host part and leave path pointing to the path part
    for (path = host; *path && *path != '/'; ++path)
      ;
    int hostLength = path - host;
    if (hostLength > hostCopyEnd - hostCopyConstructor)
      hostLength = hostCopyEnd - hostCopyConstructor;
    PL_strncpy(hostCopyConstructor, host, hostLength);
    hostCopyConstructor += hostLength;

    *hostCopyConstructor = '\0';

    nsDependentCString stringName(name),
                       stringPath(path);

    // delete any possible extant matching host cookie
    if (hostCopy[0] == '.')
      aCookieManager->Remove(nsDependentCString(hostCopy+1),
                             stringName, stringPath, PR_FALSE);

    nsresult onerv;
    // Add() makes a new domain cookie
    onerv = aCookieManager->Add(nsDependentCString(hostCopy),
                                stringPath,
                                stringName,
                                nsDependentCString(value),
                                flagsValue & 0x1,
                                PR_FALSE,
                                PRInt64(expirationDate));
    if (NS_FAILED(onerv)) {
      rv = onerv;
      break;
    }

  } while(aBuffer < bufferEnd);

  return rv;
}

/* Delimit the next field in the IE cookie buffer.
   when called:
    aBuffer: at the beginning of the next field or preceding whitespace
    aBufferEnd: one past the last valid character in the buffer
   on return:
    aField: at the beginning of the next field, which is null delimited
    aBuffer: after the null at the end of the field

   the character at which aBufferEnd points must be part of the buffer
   so we can set it to \0.
*/
void
nsIEProfileMigrator::DelimitField(char **aBuffer,
                                  const char *aBufferEnd,
                                  char **aField) 
{
  char *scan = *aBuffer;
  *aField = scan;
  while (scan < aBufferEnd && (*scan != '\r' && *scan != '\n'))
    ++scan;
  if (scan+1 < aBufferEnd && (*(scan+1) == '\r' || *(scan+1) == '\n') &&
                             *scan != *(scan+1)) {
    *scan = '\0';
    scan += 2;
  } else {
    if (scan <= aBufferEnd) // (1 byte past bufferEnd is guaranteed allocated)
      *scan = '\0';
    ++scan;
  }
  *aBuffer = scan;
}

// conversion routine. returns 0 (epoch date) if the input is out of range
time_t
nsIEProfileMigrator::FileTimeToTimeT(const char *aLowDateIntString,
                                     const char *aHighDateIntString) 
{
  FILETIME   fileTime;
  SYSTEMTIME systemTime;
  tm         tTime;
  time_t     rv;

  ::sscanf(aLowDateIntString, "%ld", &fileTime.dwLowDateTime);
  ::sscanf(aHighDateIntString, "%ld", &fileTime.dwHighDateTime);
  ::FileTimeToSystemTime(&fileTime, &systemTime);
  tTime.tm_year = systemTime.wYear - 1900;
  tTime.tm_mon = systemTime.wMonth-1;
  tTime.tm_mday = systemTime.wDay;
  tTime.tm_hour = systemTime.wHour;
  tTime.tm_min = systemTime.wMinute;
  tTime.tm_sec = systemTime.wSecond;
  tTime.tm_isdst = -1;
  rv = ::mktime(&tTime);
  return rv < 0 ? 0 : rv;
}

/* Find the accessibility stylesheet if it exists and replace Mozilla's
   with it. Return true if we found and copied a stylesheet. */
nsresult
nsIEProfileMigrator::CopyStyleSheet(PRBool aReplace) 
{
  nsresult rv = NS_OK; // return failure only if filecopy fails

  // is there a trident user stylesheet?
  nsCOMPtr<nsIWindowsRegKey> regKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey)
    return NS_ERROR_UNEXPECTED;

  NS_NAMED_LITERAL_STRING(styleKey,
                          "Software\\Microsoft\\Internet Explorer\\Styles");
  if (NS_FAILED(regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                             styleKey, nsIWindowsRegKey::ACCESS_READ)))
    return NS_OK;

  NS_NAMED_LITERAL_STRING(myStyleValName, "Use My StyleSheet");
  PRUint32 type, useMyStyle; 
  if (NS_SUCCEEDED(regKey->GetValueType(myStyleValName, &type)) &&
      type == nsIWindowsRegKey::TYPE_INT &&
      NS_SUCCEEDED(regKey->ReadIntValue(myStyleValName, &useMyStyle)) &&
      useMyStyle == 1) {

    nsAutoString tridentFilename;
    if (NS_SUCCEEDED(regKey->ReadStringValue(
                     NS_LITERAL_STRING("User Stylesheet"), tridentFilename))) {

      // tridentFilename is a native path to the specified stylesheet file
      // point an nsIFile at it
      nsCOMPtr<nsILocalFile> tridentFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
      if (tridentFile) {
        PRBool exists;

        tridentFile->InitWithPath(tridentFilename);
        tridentFile->Exists(&exists);
        if (exists) {
          // now establish our file (userContent.css in the profile/chrome dir)
          nsCOMPtr<nsIFile> chromeDir;
          NS_GetSpecialDirectory(NS_APP_USER_CHROME_DIR,
                                 getter_AddRefs(chromeDir));
          if (chromeDir)
            rv = tridentFile->CopyTo(chromeDir,
                              NS_LITERAL_STRING("userContent.css"));
        }
      }
    }
  }
  return rv;
}

void
nsIEProfileMigrator::GetUserStyleSheetFile(nsIFile **aUserFile) 
{
  nsCOMPtr<nsIFile> userChrome;

  *aUserFile = 0;

  // establish the chrome directory
  NS_GetSpecialDirectory(NS_APP_USER_CHROME_DIR, getter_AddRefs(userChrome));

  if (userChrome) {
    PRBool exists;
    userChrome->Exists(&exists);
    if (!exists &&
        NS_FAILED(userChrome->Create(nsIFile::DIRECTORY_TYPE, 0755)))
      return;

    // establish the user content stylesheet file
    userChrome->Append(NS_LITERAL_STRING("userContent.css"));
    *aUserFile = userChrome;
    NS_ADDREF(*aUserFile);
  }
}

nsresult
nsIEProfileMigrator::CopySecurityPrefs(nsIPrefBranch* aPrefs)
{
  nsCOMPtr<nsIWindowsRegKey> regKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  NS_NAMED_LITERAL_STRING(key,
      "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
  if (regKey && 
      NS_SUCCEEDED(regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                                key, nsIWindowsRegKey::ACCESS_READ))) {
    
    PRUint32 value;
    if (NS_SUCCEEDED(regKey->ReadIntValue(NS_LITERAL_STRING("SecureProtocols"),
                                          &value))) { 
      aPrefs->SetBoolPref("security.enable_ssl2", (value >> 3) & PR_TRUE);
      aPrefs->SetBoolPref("security.enable_ssl3", (value >> 5) & PR_TRUE);
      aPrefs->SetBoolPref("security.enable_tls",  (value >> 7) & PR_TRUE);
    }
  }

  return NS_OK;
}

struct ProxyData {
  char*   prefix;
  PRInt32 prefixLength;
  PRBool  proxyConfigured;
  char*   hostPref;
  char*   portPref;
};

nsresult
nsIEProfileMigrator::CopyProxyPreferences(nsIPrefBranch* aPrefs)
{
  nsCOMPtr<nsIWindowsRegKey> regKey =
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  NS_NAMED_LITERAL_STRING(key,
    "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
  if (regKey && 
      NS_SUCCEEDED(regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                                key, nsIWindowsRegKey::ACCESS_READ))) {
    nsAutoString buf; 
    // If there's an autoconfig URL specified in the registry at all, 
    // it is being used. 
    if (NS_SUCCEEDED(regKey->
                     ReadStringValue(NS_LITERAL_STRING("AutoConfigURL"), buf))) {
      // make this future-proof (MS IE will support IDN eventually and
      // 'URL' will contain more than ASCII characters)
      SetUnicharPref("network.proxy.autoconfig_url", buf, aPrefs);
      aPrefs->SetIntPref("network.proxy.type", 2);
    }

    // ProxyEnable
    PRUint32 enabled;
    if (NS_SUCCEEDED(regKey->
                     ReadIntValue(NS_LITERAL_STRING("ProxyEnable"), &enabled)))
      aPrefs->SetIntPref("network.proxy.type", (enabled & 0x1) ? 1 : 0); 
    
    if (NS_SUCCEEDED(regKey->
                     ReadStringValue(NS_LITERAL_STRING("ProxyOverride"), buf)))
      ParseOverrideServers(buf, aPrefs);

    if (NS_SUCCEEDED(regKey->
                     ReadStringValue(NS_LITERAL_STRING("ProxyServer"), buf))) {

      ProxyData data[] = {
        { "ftp=",     4, PR_FALSE, "network.proxy.ftp",
          "network.proxy.ftp_port"    },
        { "gopher=",  7, PR_FALSE, "network.proxy.gopher",
          "network.proxy.gopher_port" },
        { "http=",    5, PR_FALSE, "network.proxy.http",
          "network.proxy.http_port"   },
        { "https=",   6, PR_FALSE, "network.proxy.ssl",
          "network.proxy.ssl_port"    },
        { "socks=",   6, PR_FALSE, "network.proxy.socks",
          "network.proxy.socks_port"  },
      };

      PRInt32 startIndex = 0, count = 0;
      PRBool foundSpecificProxy = PR_FALSE;
      for (PRUint32 i = 0; i < 5; ++i) {
        PRInt32 offset = buf.Find(NS_ConvertASCIItoUTF16(data[i].prefix));
        if (offset >= 0) {
          foundSpecificProxy = PR_TRUE;

          data[i].proxyConfigured = PR_TRUE;

          startIndex = offset + data[i].prefixLength;

          PRInt32 terminal = buf.FindChar(';', offset);
          count = terminal > startIndex ? terminal - startIndex : 
                                          buf.Length() - startIndex;

          // hostPort now contains host:port
          SetProxyPref(Substring(buf, startIndex, count), data[i].hostPref,
                       data[i].portPref, aPrefs);
        }
      }

      if (!foundSpecificProxy) {
        // No proxy config for any specific type was found, assume 
        // the ProxyServer value is of the form host:port and that 
        // it applies to all protocols.
        for (PRUint32 i = 0; i < 5; ++i)
          SetProxyPref(buf, data[i].hostPref, data[i].portPref, aPrefs);
        aPrefs->SetBoolPref("network.proxy.share_proxy_settings", PR_TRUE);
      }
    }

  }

  return NS_OK;
}

