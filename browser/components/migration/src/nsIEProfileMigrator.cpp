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
#include "nsNetCID.h"
#include "nsDebug.h"
#include "nsDependentString.h"
#include "nsDirectoryServiceDefs.h"
#include "nsString.h"
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
#include "nsIServiceManagerUtils.h"
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
#include "nsIPasswordManagerInternal.h"
#include "nsIFormHistory.h"
#include "nsIRDFService.h"
#include "nsIRDFContainer.h"
#include "nsIURL.h"
#include "nsIBookmarksService.h"
#include "nsIStringBundle.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsToolkitCompsCID.h"
#include "nsUnicharUtils.h"

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kGlobalHistoryCID, NS_GLOBALHISTORY_CID);
#define TRIDENTPROFILE_BUNDLE       "chrome://browser/locale/migration/migration.properties"

#define LARGE_BUFFER 1024

const int sInitialCookieBufferSize = 1024; // but it can grow
const int sUsernameLengthLimit     = 80;
const int sHostnameLengthLimit     = 255;

//***********************************************************************
//*** windows registry to mozilla prefs data type translation functions
//***********************************************************************

typedef void (*regEntryHandler)(unsigned char *, DWORD, DWORD,
                                nsIPrefBranch *, char *);

// yes/no string to T/F boolean
void
TranslateYNtoTF(unsigned char *aRegValue, DWORD aRegValueLength,
                DWORD aRegValueType,
                nsIPrefBranch *prefs, char *aPrefKeyName) {

  PRInt32 prefIntValue = 0;

  // input type is a string, lowercase "yes" or "no"
  if (aRegValueType != REG_SZ)
    NS_WARNING("unexpected yes/no data type");

  if (aRegValueType == REG_SZ && aRegValue[0] != 0) {
    // strcmp is safe; it's bounded by its second parameter
    prefIntValue = strcmp(NS_REINTERPRET_CAST(char *, aRegValue), "yes") == 0;
    prefs->SetBoolPref(aPrefKeyName, prefIntValue);
  }
}

// yes/no string to F/T boolean
void
TranslateYNtoFT(unsigned char *aRegValue, DWORD aRegValueLength,
                DWORD aRegValueType,
                nsIPrefBranch *prefs, char *aPrefKeyName) {

  PRInt32 prefIntValue = 0;

  // input type is a string, lowercase "yes" or "no"
  if (aRegValueType != REG_SZ)
    NS_WARNING("unexpected yes/no data type");

  if (aRegValueType == REG_SZ && aRegValue[0] != 0) {
    // strcmp is safe; it's bounded by its second parameter
    prefIntValue = strcmp(NS_REINTERPRET_CAST(char *, aRegValue), "yes") != 0;
    prefs->SetBoolPref(aPrefKeyName, prefIntValue);
  }
}

void
TranslateYNtoImageBehavior(unsigned char *aRegValue, DWORD aRegValueLength,
                           DWORD aRegValueType,
                           nsIPrefBranch *prefs, char *aPrefKeyName) {
  // input type is a string, lowercase "yes" or "no"
  if (aRegValueType != REG_SZ)
    NS_WARNING("unexpected yes/no data type");

  if (aRegValueType == REG_SZ && aRegValue[0] != 0) {
    // strcmp is safe; it's bounded by its second parameter
    if (!strcmp(NS_REINTERPRET_CAST(char *, aRegValue), "yes"))
      prefs->SetIntPref(aPrefKeyName, 0);
    else
      prefs->SetIntPref(aPrefKeyName, 2);
  }
}

void
TranslateDWORDtoHTTPVersion(unsigned char *aRegValue, DWORD aRegValueLength,
                            DWORD aRegValueType,
                            nsIPrefBranch *prefs, char *aPrefKeyName) {
  // input type is a string, lowercase "yes" or "no"
  if (aRegValueType != REG_DWORD)
    NS_WARNING("unexpected signed int data type");

  if (aRegValueType == REG_DWORD) {
    DWORD val = *(NS_REINTERPRET_CAST(DWORD *, aRegValue));
    if (val & 0x1) 
      prefs->SetCharPref(aPrefKeyName, "1.1");
    else
      prefs->SetCharPref(aPrefKeyName, "1.0");
  }
}

// decimal RGB (1,2,3) to hex RGB (#010203)
void
TranslateDRGBtoHRGB(unsigned char *aRegValue, DWORD aRegValueLength,
                    DWORD aRegValueType,
                    nsIPrefBranch *prefs, char *aPrefKeyName) {

  // clear previous settings with defaults
  char prefStringValue[10];

  // input type is a string
  if (aRegValueType != REG_SZ)
    NS_WARNING("unexpected RGB data type");

  if (aRegValueType == REG_SZ && aRegValue[0] != 0) {
    int red, green, blue;
    ::sscanf(NS_REINTERPRET_CAST(char *, aRegValue), "%d,%d,%d",
             &red, &green, &blue);
    ::sprintf(prefStringValue, "#%02X%02X%02X", red, green, blue);
    prefs->SetCharPref(aPrefKeyName, prefStringValue);
  }
}

// translate a windows registry DWORD int to a mozilla prefs PRInt32
void
TranslateDWORDtoPRInt32(unsigned char *aRegValue, DWORD aRegValueLength,
                        DWORD aRegValueType,
                        nsIPrefBranch *prefs, char *aPrefKeyName) {

  // clear previous settings with defaults
  PRInt32 prefIntValue = 0;

  if (aRegValueType != REG_DWORD)
    NS_WARNING("unexpected signed int data type");

  if (aRegValueType == REG_DWORD) {
    prefIntValue = *(NS_REINTERPRET_CAST(DWORD *, aRegValue));
    prefs->SetIntPref(aPrefKeyName, prefIntValue);
  }
}

// string copy
void
TranslateString(unsigned char *aRegValue, DWORD aRegValueLength,
                DWORD aRegValueType,
                nsIPrefBranch *prefs, char *aPrefKeyName) {

  if (aRegValueType != REG_SZ)
    NS_WARNING("unexpected string data type");

  if (aRegValueType == REG_SZ)
    prefs->SetCharPref(aPrefKeyName,
                       NS_REINTERPRET_CAST(char *, aRegValue));
}

// translate accepted language character set formats
// (modified string copy)
void
TranslateLanglist(unsigned char *aRegValue, DWORD aRegValueLength,
                  DWORD aRegValueType,
                  nsIPrefBranch *prefs, char *aPrefKeyName) {

  char prefStringValue[MAX_PATH]; // a convenient size, one hopes

  if (aRegValueType != REG_SZ)
    NS_WARNING("unexpected string data type");

  if (aRegValueType != REG_SZ)
    return;

  // copy source format like "en-us,ar-kw;q=0.7,ar-om;q=0.3" into
  // destination format like "en-us, ar-kw, ar-om"

  char   *source = NS_REINTERPRET_CAST(char *, aRegValue),
         *dest = prefStringValue,
         *sourceEnd = source + aRegValueLength,
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
      *dest = *source;
    }
    comma = *source == ',';
    ++source;
  }
  *dest = 0;

  prefs->SetCharPref(aPrefKeyName, prefStringValue);
}

static int CALLBACK
fontEnumProc(const LOGFONT *aLogFont, const TEXTMETRIC *aMetric,
             DWORD aFontType, LPARAM aClosure) {
  *((int *) aClosure) = aLogFont->lfPitchAndFamily & FF_ROMAN;
  return 0;
}
void
TranslatePropFont(unsigned char *aRegValue, DWORD aRegValueLength,
                  DWORD aRegValueType,
                  nsIPrefBranch *prefs, char *aPrefKeyName) {

  HDC     dc = ::GetDC(0);
  LOGFONT lf;
  int     isSerif = 1;

  // serif or sans-serif font?
  lf.lfCharSet = DEFAULT_CHARSET;
  lf.lfPitchAndFamily = 0;
  PL_strncpyz(lf.lfFaceName, NS_REINTERPRET_CAST(char *, aRegValue),
              LF_FACESIZE);
  ::EnumFontFamiliesEx(dc, &lf, fontEnumProc, (LPARAM) &isSerif, 0);
  ::ReleaseDC(0, dc);

  if (isSerif) {
    prefs->SetCharPref("font.name.serif.x-western",
                       NS_REINTERPRET_CAST(char *, aRegValue));
    prefs->SetCharPref("font.default", "serif");
  } else {
    prefs->SetCharPref("font.name.sans-serif.x-western",
                       NS_REINTERPRET_CAST(char *, aRegValue));
    prefs->SetCharPref("font.default", "sans-serif");
  }
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
struct regEntry gRegEntries[] = {
  { "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoComplete",
     "AutoSuggest",
     "browser.urlbar.autocomplete.enabled",
     TranslateYNtoTF },
  { "Software\\Microsoft\\Internet Explorer\\International",
    "AcceptLanguage",
    "intl.accept_languages",
    TranslateLanglist },
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
    "browser.startup.homepage",
    TranslateString },
#endif
  { 0, 
    "Anchor Underline",
    "browser.underline_anchors",
    TranslateYNtoTF },
  { 0,
    "Display Inline Images",
    "network.image.imageBehavior",
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
    "SmoothScroll",
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
    "Always Use My Colors",
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
    "Always Use My Font Face",
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
  nsCOMPtr<nsIBrowserHistory> hist(do_GetService(kGlobalHistoryCID));

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
      _bstr_t url, title;
      nsCAutoString str;
      SYSTEMTIME st;
      PRBool validScheme = PR_FALSE;
      PRUnichar* tempTitle = nsnull;
      nsCOMPtr<nsIURI> uri(do_CreateInstance("@mozilla.org/network/standard-url;1"));

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

          str = (char*)(url);
          uri->SetSpec(str);
          // XXXben - 
          // MSIE stores some types of URLs in its history that we can't handle, like HTMLHelp
          // and others. At present Necko isn't clever enough to delegate handling of these types
          // to the system, so we should just avoid importing them. 
          const char* schemes[] = { "http", "https", "ftp", "file" };
          for (int i = 0; i < 4; ++i) {
            uri->SchemeIs(schemes[i], &validScheme);
            if (validScheme)
              break;
          }
          
          // 4 - Now add the page
          if (validScheme) {
            if (tempTitle) 
              hist->AddPageWithDetails((char*)url, tempTitle, lastVisited);
            else {
              nsAutoString urlTitle; urlTitle.AssignWithConversion(url);
              hist->AddPageWithDetails((char*)url, urlTitle.get(), lastVisited);
            }
          }
        }
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
  HKEY key;
  if (::RegOpenKeyEx(HKEY_CURRENT_USER, 
                     "Software\\Microsoft\\Internet Explorer\\TypedURLs",
                     0, KEY_READ, &key) == ERROR_SUCCESS) {
    int offset = 0;
#define KEY_NAME_LENGTH 20
    while (1) {
      char valueName[KEY_NAME_LENGTH];
      DWORD valueSize = KEY_NAME_LENGTH;
      unsigned char data[MAX_PATH];
      DWORD dataSize = MAX_PATH;
      DWORD type;
      LONG rv = ::RegEnumValue(key, offset, valueName, &valueSize, 
                               NULL, &type, data, &dataSize);
      if (rv == ERROR_NO_MORE_ITEMS)
        break;

      nsCAutoString valueNameStr(valueName);
      const nsACString& prefix = Substring(valueNameStr, 0, 3);
      if (prefix.Equals("url"))
        hist->MarkPageAsTyped((const char*)data);
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
// System specific to MSIE. This GUID is defined below ("IEPStoreGUID").
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

// The IEPStore GUID is the registry key under which IE Protected Store data lives. 
// {e161255a-37c3-11d2-bcaa-00c04fd929db}
static GUID IEPStoreGUID = { 0xe161255a, 0x37c3, 0x11d2, { 0xbc, 0xaa, 0x00, 0xc0, 0x4f, 0xd9, 0x29, 0xdb } };

///////////////////////////////////////////////////////////////////////////////
// IMPORTING PASSWORDS
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

  return NS_OK;
}

nsresult
nsIEProfileMigrator::GetSignonsListFromPStore(IPStore* aPStore, nsVoidArray* aSignonsFound)
{
  HRESULT hr;

  IEnumPStoreItemsPtr enumItems;
  hr = aPStore->EnumItems(0, &IEPStoreGUID, &IEPStoreGUID, 0, &enumItems);
  if (SUCCEEDED(hr)) {
    LPWSTR itemName;
    while (enumItems->raw_Next(1, &itemName, 0) == S_OK) {
      unsigned long count = 0;
      unsigned char* data = NULL;

      // We are responsible for freeing |data| using |CoTaskMemFree|!!
      // But we don't do it here... 
      hr = aPStore->ReadItem(0, &IEPStoreGUID, &IEPStoreGUID, itemName, &count, &data, NULL, 0);
      if (SUCCEEDED(hr)) {
        nsAutoString itemNameString(itemName);
        nsAutoString suffix;
        itemNameString.Right(suffix, 11);
        if (suffix.EqualsIgnoreCase(":StringData")) {
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

  nsCOMPtr<nsIURI> uri(do_CreateInstance("@mozilla.org/network/standard-url;1"));
  nsCAutoString keyCStr; keyCStr.AssignWithConversion(aKey);
  uri->SetSpec(keyCStr);

  PRBool validScheme = PR_FALSE;
  const char* schemes[] = { "http", "https" };
  for (int i = 0; i < 2; ++i) {
    uri->SchemeIs(schemes[i], &validScheme);
    if (validScheme) {
      nsCAutoString realm;
      uri->GetScheme(realm);
      realm.Append(NS_LITERAL_CSTRING("://"));

      nsCAutoString host;
      uri->GetHost(host);
      realm.Append(host);

      *aRealm = nsCRT::strdup(realm.get());
      return validScheme;
    }
  }
  return PR_FALSE;
}

nsresult
nsIEProfileMigrator::ResolveAndMigrateSignons(IPStore* aPStore, nsVoidArray* aSignonsFound)
{
  HRESULT hr;

  IEnumPStoreItemsPtr enumItems;
  hr = aPStore->EnumItems(0, &IEPStoreGUID, &IEPStoreGUID, 0, &enumItems);
  if (SUCCEEDED(hr)) {
    LPWSTR itemName;
    while (enumItems->raw_Next(1, &itemName, 0) == S_OK) {
      unsigned long count = 0;
      unsigned char* data = NULL;

      hr = aPStore->ReadItem(0, &IEPStoreGUID, &IEPStoreGUID, itemName, &count, &data, NULL, 0);
      if (SUCCEEDED(hr)) {
        nsAutoString itemNameString(itemName);
        nsAutoString suffix;
        itemNameString.Right(suffix, 11);
        if (suffix.EqualsIgnoreCase(":StringData")) {
          // :StringData contains the saved data
          const nsAString& key = Substring(itemNameString, 0, itemNameString.Length() - 11);
          
          // Assume all keys that are valid URIs are signons, not saved form data, and that 
          // all keys that aren't valid URIs are form field names (containing form data).
          nsXPIDLCString realm;
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
      nsCRT::free(sd->realm);
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
// into chunks (\0 delimited) and use nsIFormHistory's |addEntry| method to add
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
  IPStorePtr PStore;
  hr = PStoreCreateInstance(&PStore, 0, 0, 0);

  IEnumPStoreItemsPtr enumItems;
  hr = PStore->EnumItems(0, &IEPStoreGUID, &IEPStoreGUID, 0, &enumItems);
  if (SUCCEEDED(hr)) {
    LPWSTR itemName;
    while (enumItems->raw_Next(1, &itemName, 0) == S_OK) {
      unsigned long count = 0;
      unsigned char* data = NULL;

      // We are responsible for freeing |data| using |CoTaskMemFree|!!
      hr = PStore->ReadItem(0, &IEPStoreGUID, &IEPStoreGUID, itemName, &count, &data, NULL, 0);
      if (SUCCEEDED(hr)) {
        nsAutoString itemNameString(itemName);
        nsAutoString suffix;
        itemNameString.Right(suffix, 11);
        if (suffix.EqualsIgnoreCase(":StringData")) {
          // :StringData contains the saved data
          const nsAString& key = Substring(itemNameString, 0, itemNameString.Length() - 11);
          nsXPIDLCString realm;
          if (!KeyIsURI(key, getter_Copies(realm)))
            AddDataToFormHistory(key, (PRUnichar*)data, (count/sizeof(PRUnichar)));
        }
      }
    }
  }
  return NS_OK;
}

nsresult
nsIEProfileMigrator::AddDataToFormHistory(const nsAString& aKey, PRUnichar* aData, unsigned long aCount)
{
  nsCOMPtr<nsIFormHistory> formHistory(do_GetService("@mozilla.org/satchel/form-history;1"));

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

  nsCOMPtr<nsIRDFService> rdf(do_GetService("@mozilla.org/rdf/rdf-service;1"));
  nsCOMPtr<nsIRDFResource> root;
  rdf->GetResource(NS_LITERAL_CSTRING("NC:BookmarksRoot"), getter_AddRefs(root));

  nsCOMPtr<nsIBookmarksService> bms(do_GetService("@mozilla.org/browser/bookmarks-service;1"));
  NS_ENSURE_TRUE(bms, NS_ERROR_FAILURE);
  PRBool dummy;
  bms->ReadBookmarks(&dummy);

  nsAutoString personalToolbarFolderName;

  nsCOMPtr<nsIRDFResource> folder;
  if (!aReplace) {
    nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(kStringBundleServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIStringBundle> bundle;
    bundleService->CreateBundle(TRIDENTPROFILE_BUNDLE, getter_AddRefs(bundle));

    nsXPIDLString sourceNameIE;
    bundle->GetStringFromName(NS_LITERAL_STRING("sourceNameIE").get(), 
                              getter_Copies(sourceNameIE));

    const PRUnichar* sourceNameStrings[] = { sourceNameIE.get() };
    nsXPIDLString importedIEFavsTitle;
    bundle->FormatStringFromName(NS_LITERAL_STRING("importedBookmarksFolder").get(),
                                 sourceNameStrings, 1, getter_Copies(importedIEFavsTitle));

    bms->CreateFolderInContainer(importedIEFavsTitle.get(), root, -1, getter_AddRefs(folder));
  }
  else {
    // Locate the Links toolbar folder, we want to replace the Personal Toolbar content with 
    // Favorites in this folder. 
    HKEY key;
    if (::RegOpenKeyEx(HKEY_CURRENT_USER, 
                      "Software\\Microsoft\\Internet Explorer\\Toolbar",
                      0, KEY_READ, &key) == ERROR_SUCCESS) {
      DWORD type, length;
      unsigned char linksFolderName[MAX_PATH];
      if (::RegQueryValueEx(key, "LinksFolderName", 0, &type, linksFolderName, &length) == ERROR_SUCCESS)
        personalToolbarFolderName.AssignWithConversion((char*)linksFolderName);
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
nsIEProfileMigrator::CopySmartKeywords(nsIRDFResource* aParentFolder)
{ 
  HKEY key;
  if (::RegOpenKeyEx(HKEY_CURRENT_USER, 
                    "Software\\Microsoft\\Internet Explorer\\SearchUrl",
                    0, KEY_READ, &key) == ERROR_SUCCESS) {
    nsCOMPtr<nsIBookmarksService> bms(do_GetService("@mozilla.org/browser/bookmarks-service;1"));
    nsCOMPtr<nsIRDFResource> keywordsFolder, bookmark;
    nsCOMPtr<nsIURI> uri(do_CreateInstance("@mozilla.org/network/standard-url;1"));

    nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(kStringBundleServiceCID);
    
    nsCOMPtr<nsIStringBundle> bundle;
    bundleService->CreateBundle(TRIDENTPROFILE_BUNDLE, getter_AddRefs(bundle));


    int offset = 0;
    while (1) {
      char keyName[MAX_PATH];
      DWORD keySize = MAX_PATH;
      LONG rv = ::RegEnumKey(key, offset, keyName, keySize);
      if (rv == ERROR_NO_MORE_ITEMS)
        break;

      if (!keywordsFolder) {
        nsXPIDLString sourceNameIE;
        bundle->GetStringFromName(NS_LITERAL_STRING("sourceNameIE").get(), 
                                  getter_Copies(sourceNameIE));

        const PRUnichar* sourceNameStrings[] = { sourceNameIE.get() };
        nsXPIDLString importedIESearchUrlsTitle;
        bundle->FormatStringFromName(NS_LITERAL_STRING("importedSearchURLsFolder").get(),
                                    sourceNameStrings, 1, getter_Copies(importedIESearchUrlsTitle));
        bms->CreateFolderInContainer(importedIESearchUrlsTitle.get(), aParentFolder, -1, 
                                     getter_AddRefs(keywordsFolder));
      }

      nsCAutoString keyNameStr(keyName);
      nsCAutoString smartKeywordKey(NS_LITERAL_CSTRING("Software\\Microsoft\\Internet Explorer\\SearchUrl\\"));
      smartKeywordKey += keyNameStr;
      HKEY skey;
      if (::RegOpenKeyEx(HKEY_CURRENT_USER, 
                        smartKeywordKey.get(),
                        0, KEY_READ, &skey) == ERROR_SUCCESS) {
        LONG length = LARGE_BUFFER;
        char url[LARGE_BUFFER];
        if (::RegQueryValue(skey, NULL, (char*)url, &length) == ERROR_SUCCESS) {
          uri->SetSpec(nsDependentCString((char*)url));
          nsCAutoString hostCStr;
          uri->GetHost(hostCStr);
          nsAutoString host; host.AssignWithConversion(hostCStr.get());

          const PRUnichar* nameStrings[] = { host.get() };
          nsXPIDLString keywordName;
          rv = bundle->FormatStringFromName(NS_LITERAL_STRING("importedSearchURLsTitle").get(),
                                            nameStrings, 1, getter_Copies(keywordName));

          nsAutoString keyword; keyword.AssignWithConversion(keyName);

          const PRUnichar* descStrings[] = { keyword.get(), host.get() };
          nsXPIDLString keywordDesc;
          rv = bundle->FormatStringFromName(NS_LITERAL_STRING("importedSearchUrlDesc").get(),
                                            descStrings, 2, getter_Copies(keywordDesc));
          // XXX: Assume Windows stores URLs in ASCII (with everything escaped).
          // It may change in the future, but we can't assume that it's UTF-8
          // here. Unlike other places, it's likely to be in a legacy encoding.
          // Assuming ASCII at least works for Latin-1 if URLs are stored
          // without being escaped and Win32 supports IDN (it doesn't at the 
          // moment.) Eventually, we have to use 'W' APIs when available.
          bms->CreateBookmarkInContainer(keywordName.get(), 
                                         NS_ConvertASCIItoUTF16(url).get(), 
                                         keyword.get(), 
                                         keywordDesc.get(), 
                                         nsnull,
                                         nsnull, 
                                         keywordsFolder, 
                                         -1, 
                                         getter_AddRefs(bookmark));
        }
      }

      ++offset;
    }
  }

  return NS_OK;
}

void 
nsIEProfileMigrator::ResolveShortcut(const nsAFlatString &aFileName, char** aOutURL) 
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
          IMalloc* pMalloc;
          result = SHGetMalloc(&pMalloc);
          if (SUCCEEDED(result)) {
            pMalloc->Free(lpTemp);
            pMalloc->Release();
          } 
        }
      }
      urlFile->Release();
    }
    urlLink->Release();
  }
}

nsresult
nsIEProfileMigrator::ParseFavoritesFolder(nsIFile* aDirectory, 
                                          nsIRDFResource* aParentResource,
                                          nsIBookmarksService* aBookmarksService, 
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
      // It's a .lnk file.  Get the native path and check to see if it's
      // a dir.  If so, create a bookmark for the dir.  If not, then
      // simply do nothing and continue.

      // Get the native path that the .lnk file is pointing to.
      nsCAutoString path;
      rv = currFile->GetNativeTarget(path);
      if (NS_FAILED(rv)) continue;

      nsCOMPtr<nsILocalFile> localFile;
      rv = NS_NewNativeLocalFile(path, PR_TRUE, getter_AddRefs(localFile));
      if (NS_FAILED(rv)) continue;

      // Check for dir here.  If path is not a dir, just continue with
      // next import.
      rv = localFile->IsDirectory(&isDir);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!isDir) continue;

      nsCAutoString spec;
      nsCOMPtr<nsIFile> filePath(localFile);
      // Get the file url format (file:///...) of the native file path.
      rv = NS_GetURLSpecFromFile(filePath, spec);
      if (NS_FAILED(rv)) continue;

      // Look for and strip out the .lnk extension.
      NS_NAMED_LITERAL_STRING(lnkExt, ".lnk");
      PRInt32 lnkExtStart = bookmarkName.Length() - lnkExt.Length();
      if (StringEndsWith(bookmarkName, lnkExt,
                         nsCaseInsensitiveStringComparator()))
        bookmarkName.Truncate(lnkExtStart);

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
      if (NS_FAILED(rv)) continue;
    }
    else if (isDir) {
      nsCOMPtr<nsIRDFResource> folder;
      if (bookmarkName.Equals(aPersonalToolbarFolderName)) {
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
      }
      else {
        rv = aBookmarksService->CreateFolderInContainer(bookmarkName.get(), 
                                                        aParentResource, 
                                                        -1, 
                                                        getter_AddRefs(folder));
        if (NS_FAILED(rv)) continue;
      }

      rv = ParseFavoritesFolder(currFile, folder, aBookmarksService, aPersonalToolbarFolderName, PR_FALSE);
      if (NS_FAILED(rv)) continue;
    }
    else {
      nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
      nsCAutoString extension;

      url->GetFileExtension(extension);
      if (!extension.Equals(NS_LITERAL_CSTRING("url"),
                            nsCaseInsensitiveCStringComparator()))
        continue;

      nsAutoString name(Substring(bookmarkName, 0, 
                                  bookmarkName.Length() - extension.Length() - 1));

      nsAutoString path;
      currFile->GetPath(path);

      nsXPIDLCString resolvedURL;
      ResolveShortcut(path, getter_Copies(resolvedURL));

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
      if (NS_FAILED(rv)) continue;
    }
  }
  while (1);

  return rv;
}

nsresult
nsIEProfileMigrator::CopyPreferences(PRBool aReplace) 
{
  HKEY            regKey;
  PRBool          regKeyOpen = PR_FALSE;
  struct regEntry *entry,
                  *endEntry = gRegEntries +
                              sizeof(gRegEntries)/sizeof(struct regEntry);
  DWORD           regType;
  DWORD           regLength;
  unsigned char   regValue[MAX_PATH];  // a convenient size

  nsCOMPtr<nsIPrefBranch> prefs;

  { // scope pserve why not
    nsCOMPtr<nsIPrefService> pserve(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (pserve)
      pserve->GetBranch("", getter_AddRefs(prefs));
  }
  if (!prefs)
    return NS_ERROR_FAILURE;

  // step through gRegEntries table
  
  for (entry = gRegEntries; entry < endEntry; ++entry) {

    // a new keyname? close any previous one and open the new one
    if (entry->regKeyName) {
      if (regKeyOpen) {
        ::RegCloseKey(regKey);
        regKeyOpen = PR_FALSE;
      }
      regKeyOpen = ::RegOpenKeyEx(HKEY_CURRENT_USER, entry->regKeyName, 0,
                                  KEY_READ, &regKey) == ERROR_SUCCESS;
    }

    if (regKeyOpen) {
      // read registry data
      regLength = MAX_PATH;
      if (::RegQueryValueEx(regKey, entry->regValueName, 0,
          &regType, regValue, &regLength) == ERROR_SUCCESS)

        entry->entryHandler(regValue, regLength, regType,
                            prefs, entry->prefKeyName);
    }
  }

  if (regKeyOpen)
    ::RegCloseKey(regKey);

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
  char username[sUsernameLengthLimit+2];
  ::GetEnvironmentVariable("USERNAME", username, sizeof(username));
  username[sizeof(username)-2] = '\0';
  PL_strcat(username, "@");
  int usernameLength = PL_strlen(username);

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
    nsCAutoString fileName;
    cookieFile->GetNativeLeafName(fileName);
    const nsACString &fileOwner = Substring(fileName, 0, usernameLength);
    if (!fileOwner.Equals(username, nsCaseInsensitiveCStringComparator()))
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
  HKEY     regKey;
  DWORD    regType,
           regLength,
           regDWordValue;
  char     tridentFilename[MAX_PATH];

  // is there a trident user stylesheet?
  if (::RegOpenKeyEx(HKEY_CURRENT_USER,
                 "Software\\Microsoft\\Internet Explorer\\Styles", 0,
                 KEY_READ, &regKey) != ERROR_SUCCESS)
    return NS_OK;
  
  regLength = sizeof(DWORD);
  if (::RegQueryValueEx(regKey, "Use My Stylesheet", 0, &regType,
                        NS_REINTERPRET_CAST(unsigned char *, &regDWordValue),
                        &regLength) == ERROR_SUCCESS &&
      regType == REG_DWORD && regDWordValue == 1) {

    regLength = MAX_PATH;
    if (::RegQueryValueEx(regKey, "User Stylesheet", 0, &regType,
                          NS_REINTERPRET_CAST(unsigned char *, tridentFilename),
                          &regLength)
        == ERROR_SUCCESS) {

      // tridentFilename is a native path to the specified stylesheet file
      // point an nsIFile at it
      nsCOMPtr<nsILocalFile> tridentFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
      if (tridentFile) {
        PRBool exists;

        tridentFile->InitWithNativePath(nsDependentCString(tridentFilename));
        tridentFile->Exists(&exists);
        if (exists) {
          // now establish our file (userContent.css in the profile/chrome dir)
          nsCOMPtr<nsIFile> chromeDir;
          NS_GetSpecialDirectory(NS_APP_USER_CHROME_DIR,
                                 getter_AddRefs(chromeDir));
          if (chromeDir)
            rv = tridentFile->CopyToNative(chromeDir,
                              nsDependentCString("userContent.css"));
        }
      }
    }
  }
  ::RegCloseKey(regKey);
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
    userChrome->AppendNative(NS_LITERAL_CSTRING("userContent.css"));
    *aUserFile = userChrome;
    NS_ADDREF(*aUserFile);
  }
}

nsresult
nsIEProfileMigrator::CopySecurityPrefs(nsIPrefBranch* aPrefs)
{
  HKEY key;
  if (::RegOpenKeyEx(HKEY_CURRENT_USER, 
                     "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
                     0, KEY_READ, &key) == ERROR_SUCCESS) {
    DWORD value, type; 
    DWORD length = sizeof DWORD;
    if (::RegQueryValueEx(key, "SecureProtocols", 0, &type, (LPBYTE)&value, &length) == ERROR_SUCCESS) {
      aPrefs->SetBoolPref("security.enable_ssl2", value & 0x08);
      aPrefs->SetBoolPref("security.enable_ssl3", value & 0x20);
      aPrefs->SetBoolPref("security.enable_tls",  value & 0x80);
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
  HKEY key;
  if (::RegOpenKeyEx(HKEY_CURRENT_USER, 
                     "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
                     0, KEY_READ, &key) == ERROR_SUCCESS) {
    DWORD type, length = LARGE_BUFFER;
    char buf[LARGE_BUFFER];
    // If there's an autoconfig URL specified in the registry at all, it is being used. 
    if (::RegQueryValueEx(key, "AutoConfigURL", 0, &type, (LPBYTE)&buf, &length) == ERROR_SUCCESS) {      
      aPrefs->SetCharPref("network.proxy.autoconfig_url", buf);
      aPrefs->SetIntPref("network.proxy.type", 2);
    }
    // ProxyEnable
    DWORD enabled;
    length = LARGE_BUFFER;
    if (::RegQueryValueEx(key, "ProxyEnable", 0, &type, (LPBYTE)&enabled, &length) == ERROR_SUCCESS) {
      if (enabled & 0x1) 
        aPrefs->SetIntPref("network.proxy.type", 1);
      else
        aPrefs->SetIntPref("network.proxy.type", 0);
    }
    
    length = LARGE_BUFFER;
    LONG r = ::RegQueryValueEx(key, "ProxyOverride", 0, &type, (LPBYTE)&buf, &length);
    if (r == ERROR_SUCCESS)
      ParseOverrideServers(buf, aPrefs);

    length = LARGE_BUFFER;
    r = ::RegQueryValueEx(key, "ProxyServer", 0, &type, (LPBYTE)&buf, &length);
    if (r == ERROR_SUCCESS) {
      nsCAutoString bufStr; bufStr = (char*)buf;

      ProxyData data[] = {
        { "ftp=",     4, PR_FALSE, "network.proxy.ftp",     "network.proxy.ftp_port"    },
        { "gopher=",  7, PR_FALSE, "network.proxy.gopher",  "network.proxy.gopher_port" },
        { "http=",    5, PR_FALSE, "network.proxy.http",    "network.proxy.http_port"   },
        { "https=",   6, PR_FALSE, "network.proxy.ssl",     "network.proxy.ssl_port"    },
        { "socks=",   6, PR_FALSE, "network.proxy.socks",   "network.proxy.socks_port"  },
      };

      PRInt32 startIndex = 0, count = 0;
      PRBool foundSpecificProxy = PR_FALSE;
      for (PRUint32 i = 0; i < 5; ++i) {
        PRInt32 offset = bufStr.Find(data[i].prefix);
        if (offset >= 0) {
          foundSpecificProxy = PR_TRUE;

          data[i].proxyConfigured = PR_TRUE;

          startIndex = offset + data[i].prefixLength;

          PRInt32 terminal = bufStr.FindChar(';', offset);
          count = terminal > startIndex ? terminal - startIndex : bufStr.Length() - startIndex;

          // hostPort now contains host:port
          SetProxyPref(Substring(bufStr, startIndex, count), data[i].hostPref, data[i].portPref, aPrefs);
        }
      }

      if (!foundSpecificProxy) {
        // No proxy config for any specific type was found, assume the ProxyServer value
        // is of the form host:port and that it applies to all protocols.
        for (PRUint32 i = 0; i < 5; ++i)
          SetProxyPref(bufStr, data[i].hostPref, data[i].portPref, aPrefs);
      }
    }

  }

  return NS_OK;
}

