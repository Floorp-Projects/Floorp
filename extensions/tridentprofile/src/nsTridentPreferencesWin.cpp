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
#include <wingdi.h>
#include "nsAppDirectoryServiceDefs.h"
#include "nsCOMPtr.h"
#include "nsCCookieManager.h"
#include "nsDebug.h"
#include "nsDependentString.h"
#include "nsDirectoryServiceDefs.h"
#include "nsString.h"
#include "nsTridentPreferencesWin.h"
#include "plstr.h"
#include "prio.h"
#include "prmem.h"
#include "nsICookieManager2.h"
#include "nsICookieService.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIServiceManagerUtils.h"
#include "nsISimpleEnumerator.h"
#include "nsITridentProfileMigrator.h"

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

// translate homepage
// (modified string copy)
void
TranslateHomepage(unsigned char *aRegValue, DWORD aRegValueLength,
                  DWORD aRegValueType,
                  nsIPrefBranch *prefs, char *aPrefKeyName) {

  if (aRegValueType != REG_SZ)
    NS_WARNING("unexpected string data type");

  if (aRegValueType == REG_SZ) {
    // strcmp is safe; it's bounded by its second parameter
    if (strcmp(NS_REINTERPRET_CAST(char *, aRegValue), "about:blank") == 0)
      prefs->SetIntPref("browser.startup.page", 0);
    else {
      prefs->SetIntPref("browser.startup.page", 1);
      prefs->SetCharPref(aPrefKeyName,
                        NS_REINTERPRET_CAST(char *, aRegValue));
    }
  }
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
    "wallet.captureForms",
    TranslateYNtoTF },
  { 0,
    "FormSuggest Passwords",
    "signon.rememberSignons",
    TranslateYNtoTF },
  { 0,
    "Start Page",
    "browser.startup.homepage",
    TranslateHomepage },
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

//*****************************************************************************
//*** factory
//*****************************************************************************

nsTridentPreferences *
MakeTridentPreferences() {
  return new nsTridentPreferencesWin();
}

//*****************************************************************************
//*** nsTridentPreferences
//*****************************************************************************

nsTridentPreferences::nsTridentPreferences() {
}

nsTridentPreferences::~nsTridentPreferences() {
}

//*****************************************************************************
//*** nsTridentPreferencesWin
//*****************************************************************************

nsTridentPreferencesWin::nsTridentPreferencesWin() {
}

nsTridentPreferencesWin::~nsTridentPreferencesWin() {
}

// migration entry point
nsresult
nsTridentPreferencesWin::MigrateTridentPreferences(PRUint32 aItems) {

  nsresult rv = NS_OK;

  if (aItems & nsITridentProfileMigrator::PREFERENCES)
    rv = CopyPreferences();
  if (NS_SUCCEEDED(rv) && (aItems & nsITridentProfileMigrator::COOKIES))
    rv = CopyCookies();

  return rv;
}

nsresult
nsTridentPreferencesWin::CopyPreferences() {

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

  return CopyStyleSheet();
}

/* Fetch and translate the current user's cookies.
   Return true if successful. */
nsresult
nsTridentPreferencesWin::CopyCookies() {

  // IE cookies are stored in files named <username>@domain[n].txt
  // (in <username>'s Cookies folder. isn't the naming redundant?)

  PRBool rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIFile> cookiesDir;
  nsCOMPtr<nsISimpleEnumerator> cookieFiles;

  nsCOMPtr<nsICookieManager2> cookieManager(do_GetService(NS_COOKIEMANAGER_CONTRACTID));
  nsCOMPtr<nsICookieService> cookieService(do_GetService(NS_COOKIESERVICE_CONTRACTID));
  if (!cookieManager || !cookieService)
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
    if (!cookieFile)
      break; // unexpected! punt!

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
      if (!fileContents)
        break; // fatal error
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
        if (NS_SUCCEEDED(onerv))
          rv = NS_OK;
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
nsTridentPreferencesWin::CopyCookiesFromBuffer(
                           char *aBuffer,
                           PRUint32 aBufferLength,
                           nsICookieManager2 *aCookieManager) {

  nsresult  rv = NS_ERROR_FAILURE;

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
  char     hostCopy[sHostnameLengthLimit+1];

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

    // convert flags to an int, date numbers to useable dates
    ::sscanf(flags, "%d", &flagsValue);
    expirationDate = FileTimeToTimeT(expirationDate1, expirationDate2);
    creationDate = FileTimeToTimeT(creationDate1, creationDate2);

    // separate host from path
    for (path = host; *path && *path != '/'; ++path)
      ;
    if (*path == '/') {
      // to separate them, point host to a null-terminated copy
      int hostLength = path - host;
      if (hostLength > sHostnameLengthLimit)
        hostLength = sHostnameLengthLimit;
      *path = '\0';
      PL_strncpy(hostCopy, host, hostLength);
      hostCopy[hostLength] = '\0';
      host = hostCopy;
      *path = '/';
    }

    nsresult onerv;
    onerv = aCookieManager->Add(nsDependentCString(host),
                                nsDependentCString(path),
                                nsDependentCString(name),
                                nsDependentCString(value),
                                flagsValue & 0x1, expirationDate);
    if (NS_SUCCEEDED(onerv))
      rv = NS_OK;

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
nsTridentPreferencesWin::DelimitField(char **aBuffer,
                                      const char *aBufferEnd,
                                      char **aField) {

  char *scan = *aBuffer;
  while (scan < aBufferEnd && (*scan == '\r' || *scan == '\n'))
    ++scan;
  *aField = scan;
  while (scan < aBufferEnd && (*scan != '\r' && *scan != '\n'))
    ++scan;
  if (scan <= aBufferEnd)
    *scan = '\0';
  *aBuffer = scan+1;
}

// conversion routine. returns 0 (epoch date) if the input is out of range
time_t
nsTridentPreferencesWin::FileTimeToTimeT(const char *aLowDateIntString,
                                         const char *aHighDateIntString) {

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
nsTridentPreferencesWin::CopyStyleSheet() {

  nsresult rv = NS_ERROR_FAILURE;
  HKEY     regKey;
  DWORD    regType,
           regLength,
           regDWordValue;
  char     tridentFilename[MAX_PATH];

  // is there a trident user stylesheet?
  if (::RegOpenKeyEx(HKEY_CURRENT_USER,
                 "Software\\Microsoft\\Internet Explorer\\Styles", 0,
                 KEY_READ, &regKey) != ERROR_SUCCESS)
    return NS_ERROR_FAILURE;
  
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
nsTridentPreferencesWin::GetUserStyleSheetFile(nsIFile **aUserFile) {

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

