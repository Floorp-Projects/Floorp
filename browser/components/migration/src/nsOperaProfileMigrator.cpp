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
 * The Original Code is The Browser Profile Migrator.
 *
 * The Initial Developer of the Original Code is Ben Goodger.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ben Goodger <ben@bengoodger.com>
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

#include "nsAppDirectoryServiceDefs.h"
#include "nsBrowserProfileMigratorUtils.h"
#include "nsCRT.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIBookmarksService.h"
#include "nsIBrowserProfileMigrator.h"
#include "nsIBrowserHistory.h"
#include "nsICookieManager2.h"
#include "nsIGlobalHistory.h"
#include "nsIInputStream.h"
#include "nsILineInputStream.h"
#include "nsILocalFile.h"
#include "nsINIParser.h"
#include "nsIObserverService.h"
#include "nsIPermissionManager.h"
#include "nsIPrefLocalizedString.h"
#include "nsIPrefService.h"
#include "nsIProfileMigrator.h"
#include "nsIProperties.h"
#include "nsIRDFContainer.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsISupportsPrimitives.h"
#include "nsNetUtil.h"
#include "nsOperaProfileMigrator.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsToolkitCompsCID.h"
#ifdef XP_WIN
#include <windows.h>
#endif

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kGlobalHistoryCID, NS_GLOBALHISTORY_CID);
#define MIGRATION_BUNDLE "chrome://browser/locale/migration/migration.properties"

#ifdef XP_WIN
#define OPERA_PREFERENCES_FOLDER_NAME NS_LITERAL_STRING("Opera")
#define OPERA_PREFERENCES_FILE_NAME NS_LITERAL_STRING("opera6.ini")
#define OPERA_HISTORY_FILE_NAME NS_LITERAL_STRING("global.dat")
#define OPERA_BOOKMARKS_FILE_NAME NS_LITERAL_STRING("opera6.adr")
#elif defined(XP_MACOSX)
#define OPERA_PREFERENCES_FOLDER_NAME NS_LITERAL_STRING("Opera 6 Preferences")
#define OPERA_PREFERENCES_FILE_NAME NS_LITERAL_STRING("Opera 6 Preferences")
#define OPERA_HISTORY_FILE_NAME NS_LITERAL_STRING("Opera Global History")
#define OPERA_BOOKMARKS_FILE_NAME NS_LITERAL_STRING("Bookmarks")
#elif defined (XP_UNIX)
#define OPERA_PREFERENCES_FOLDER_NAME NS_LITERAL_STRING(".opera")
#define OPERA_PREFERENCES_FILE_NAME NS_LITERAL_STRING("opera6.ini")
#define OPERA_HISTORY_FILE_NAME NS_LITERAL_STRING("global.dat")
#define OPERA_BOOKMARKS_FILE_NAME NS_LITERAL_STRING("opera6.adr")
#elif defined (XP_BEOS)
#define OPERA_PREFERENCES_FOLDER_NAME NS_LITERAL_STRING("Opera")
#define OPERA_PREFERENCES_FILE_NAME NS_LITERAL_STRING("opera.ini")
#define OPERA_HISTORY_FILE_NAME NS_LITERAL_STRING("global.dat")
#define OPERA_BOOKMARKS_FILE_NAME NS_LITERAL_STRING("opera.adr")
#else
#error Need to define location of Opera Profile data.
#endif

#define OPERA_COOKIES_FILE_NAME NS_LITERAL_STRING("cookies4.dat")
#ifdef XP_BEOS
#define OPERA_COOKIES_FILE_NAME NS_LITERAL_STRING("cookies.dat")
#endif

///////////////////////////////////////////////////////////////////////////////
// nsBrowserProfileMigrator

NS_IMPL_ISUPPORTS1(nsOperaProfileMigrator, nsIBrowserProfileMigrator)

nsOperaProfileMigrator::nsOperaProfileMigrator()
{
  mObserverService = do_GetService("@mozilla.org/observer-service;1");
}

nsOperaProfileMigrator::~nsOperaProfileMigrator()
{
}

NS_IMETHODIMP
nsOperaProfileMigrator::Migrate(PRUint16 aItems, nsIProfileStartup* aStartup, const PRUnichar* aProfile)
{
  nsresult rv = NS_OK;
  PRBool aReplace = aStartup ? PR_TRUE : PR_FALSE;

  if (aStartup) {
    rv = aStartup->DoStartup();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!mOperaProfile)
    GetOperaProfile(aProfile, getter_AddRefs(mOperaProfile));

  NOTIFY_OBSERVERS(MIGRATION_STARTED, nsnull);

  COPY_DATA(CopyPreferences,  aReplace, nsIBrowserProfileMigrator::SETTINGS);
  COPY_DATA(CopyCookies,      aReplace, nsIBrowserProfileMigrator::COOKIES);
  COPY_DATA(CopyHistory,      aReplace, nsIBrowserProfileMigrator::HISTORY);
  COPY_DATA(CopyBookmarks,    aReplace, nsIBrowserProfileMigrator::BOOKMARKS);

  NOTIFY_OBSERVERS(MIGRATION_ENDED, nsnull);

  return rv;
}

NS_IMETHODIMP
nsOperaProfileMigrator::GetMigrateData(const PRUnichar* aProfile, 
                                       PRBool aReplace,
                                       PRUint16* aResult)
{
  *aResult = 0;
  if (!mOperaProfile) {
    GetOperaProfile(aProfile, getter_AddRefs(mOperaProfile));
    if (!mOperaProfile)
      return NS_ERROR_FILE_NOT_FOUND;
  }

  MigrationData data[] = { { ToNewUnicode(OPERA_PREFERENCES_FILE_NAME),
                             nsIBrowserProfileMigrator::SETTINGS,
                             PR_FALSE },
                           { ToNewUnicode(OPERA_COOKIES_FILE_NAME),
                             nsIBrowserProfileMigrator::COOKIES,
                             PR_FALSE },
                           { ToNewUnicode(OPERA_HISTORY_FILE_NAME),
                             nsIBrowserProfileMigrator::HISTORY,
                             PR_FALSE },
                           { ToNewUnicode(OPERA_BOOKMARKS_FILE_NAME),
                             nsIBrowserProfileMigrator::BOOKMARKS,
                             PR_FALSE } };
                                                                  
  // Frees file name strings allocated above.
  GetMigrateDataFromArray(data, sizeof(data)/sizeof(MigrationData), 
                          aReplace, mOperaProfile, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsOperaProfileMigrator::GetSourceExists(PRBool* aResult)
{
  nsCOMPtr<nsISupportsArray> profiles;
  GetSourceProfiles(getter_AddRefs(profiles));

  if (profiles) { 
    PRUint32 count;
    profiles->Count(&count);
    *aResult = count > 0;
  }
  else
    *aResult = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsOperaProfileMigrator::GetSourceHasMultipleProfiles(PRBool* aResult)
{
  nsCOMPtr<nsISupportsArray> profiles;
  GetSourceProfiles(getter_AddRefs(profiles));

#ifdef XP_WIN
  if (profiles) {
    PRUint32 count;
    profiles->Count(&count);
    *aResult = count > 1;
  }
  else
#endif
    *aResult = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsOperaProfileMigrator::GetSourceProfiles(nsISupportsArray** aResult)
{
  if (!mProfiles) {
    nsresult rv = NS_NewISupportsArray(getter_AddRefs(mProfiles));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIProperties> fileLocator(do_GetService("@mozilla.org/file/directory_service;1"));
    nsCOMPtr<nsILocalFile> file;
#ifdef XP_WIN
    fileLocator->Get(NS_WIN_APPDATA_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(file));

    // Opera profile lives under %APP_DATA%\Opera\<operaver>\profile 
    file->Append(OPERA_PREFERENCES_FOLDER_NAME);

    nsCOMPtr<nsISimpleEnumerator> e;
    rv = file->GetDirectoryEntries(getter_AddRefs(e));
    if (NS_FAILED(rv))
      return rv;

    PRBool hasMore;
    e->HasMoreElements(&hasMore);
    while (hasMore) {
      nsCOMPtr<nsILocalFile> curr;
      e->GetNext(getter_AddRefs(curr));

      PRBool isDirectory = PR_FALSE;
      curr->IsDirectory(&isDirectory);
      if (isDirectory) {
        nsCOMPtr<nsISupportsString> string(do_CreateInstance("@mozilla.org/supports-string;1"));
        nsAutoString leafName;
        curr->GetLeafName(leafName);
        string->SetData(leafName);
        mProfiles->AppendElement(string);
      }

      e->HasMoreElements(&hasMore);
    }
#elif defined (XP_MACOSX)
    fileLocator->Get(NS_MAC_USER_LIB_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(file));
    
    file->Append(NS_LITERAL_STRING("Preferences"));
    file->Append(OPERA_PREFERENCES_FOLDER_NAME);
    
    PRBool exists;
    file->Exists(&exists);
    
    if (exists) {
      nsCOMPtr<nsISupportsString> string(do_CreateInstance("@mozilla.org/supports-string;1"));
      string->SetData(OPERA_PREFERENCES_FOLDER_NAME);
      mProfiles->AppendElement(string);
    }
#elif defined (XP_UNIX)
    fileLocator->Get(NS_UNIX_HOME_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(file));
    
    file->Append(OPERA_PREFERENCES_FOLDER_NAME);
    
    PRBool exists;
    file->Exists(&exists);
    
    if (exists) {
      nsCOMPtr<nsISupportsString> string(do_CreateInstance("@mozilla.org/supports-string;1"));
      string->SetData(OPERA_PREFERENCES_FOLDER_NAME);
      mProfiles->AppendElement(string);
    }
#endif
  }

  *aResult = mProfiles;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

#define _OPM(type) nsOperaProfileMigrator::type

static
nsOperaProfileMigrator::PrefTransform gTransforms[] = {
  { "User Prefs", "Download Directory", _OPM(STRING), "browser.download.defaultFolder", _OPM(SetFile), PR_FALSE, -1 },
  { nsnull, "Enable Cookies", _OPM(INT), "network.cookie.cookieBehavior", _OPM(SetCookieBehavior), PR_FALSE, -1 },
  { nsnull, "Accept Cookies Session Only", _OPM(BOOL), "network.cookie.enableForCurrentSessionOnly", _OPM(SetBool), PR_FALSE, -1 },
  { nsnull, "Allow script to resize window", _OPM(BOOL), "dom.disable_window_move_resize", _OPM(SetBool), PR_FALSE, -1 },
  { nsnull, "Allow script to move window", _OPM(BOOL), "dom.disable_window_move_resize", _OPM(SetBool), PR_FALSE, -1 },
  { nsnull, "Allow script to raise window", _OPM(BOOL), "dom.disable_window_flip", _OPM(SetBool), PR_FALSE, -1 },
  { nsnull, "Allow script to change status", _OPM(BOOL), "dom.disable_window_status_change", _OPM(SetBool), PR_FALSE, -1 },
  { nsnull, "Home URL", _OPM(STRING), "browser.startup.homepage", _OPM(SetWString), PR_FALSE, -1 },
  { nsnull, "Ignore Unrequested Popups", _OPM(BOOL), "dom.disable_open_during_load", _OPM(SetBool), PR_FALSE, -1 },
  { nsnull, "Load Figures", _OPM(BOOL), "network.image.imageBehavior", _OPM(SetImageBehavior), PR_FALSE, -1 },

  { "Visited link", nsnull, _OPM(COLOR), "browser.visited_color", _OPM(SetString), PR_FALSE, -1 },
  { "Link", nsnull, _OPM(COLOR), "browser.anchor_color", _OPM(SetString), PR_FALSE, -1 },
  { nsnull, "Underline", _OPM(BOOL), "browser.underline_anchors", _OPM(SetBool), PR_FALSE, -1 },
  { nsnull, "Expiry", _OPM(INT), "browser.history_expire_days", _OPM(SetInt), PR_FALSE, -1 },

  { "Security Prefs", "Enable SSL v2", _OPM(BOOL), "security.enable_ssl2", _OPM(SetBool), PR_FALSE, -1 },
  { nsnull, "Enable SSL v3", _OPM(BOOL), "security.enable_ssl3", _OPM(SetBool), PR_FALSE, -1 },
  { nsnull, "Enable TLS v1.0", _OPM(BOOL), "security.enable_tls", _OPM(SetBool), PR_FALSE, -1 },

  { "Extensions", "Scripting", _OPM(BOOL), "javascript.enabled", _OPM(SetBool), PR_FALSE, -1 }
};

nsresult 
nsOperaProfileMigrator::SetFile(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  nsCOMPtr<nsILocalFile> lf(do_CreateInstance("@mozilla.org/file/local;1"));
  lf->InitWithNativePath(nsDependentCString(xform->stringValue));
  return aBranch->SetComplexValue(xform->targetPrefName, NS_GET_IID(nsILocalFile), lf);
}

nsresult 
nsOperaProfileMigrator::SetCookieBehavior(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  PRInt32 val = (xform->intValue == 3) ? 0 : (xform->intValue == 0) ? 2 : 1;
  return aBranch->SetIntPref(xform->targetPrefName, val);
}

nsresult 
nsOperaProfileMigrator::SetImageBehavior(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  return aBranch->SetIntPref(xform->targetPrefName, xform->boolValue ? 0 : 2);
}

nsresult 
nsOperaProfileMigrator::SetBool(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  return aBranch->SetBoolPref(xform->targetPrefName, xform->boolValue);
}

nsresult 
nsOperaProfileMigrator::SetWString(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  nsCOMPtr<nsIPrefLocalizedString> pls(do_CreateInstance("@mozilla.org/pref-localizedstring;1"));
  nsAutoString data; data.AssignWithConversion(xform->stringValue);
  pls->SetData(data.get());
  return aBranch->SetComplexValue(xform->targetPrefName, NS_GET_IID(nsIPrefLocalizedString), pls);
}

nsresult 
nsOperaProfileMigrator::SetInt(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  return aBranch->SetIntPref(xform->targetPrefName, xform->intValue);
}

nsresult 
nsOperaProfileMigrator::SetString(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  return aBranch->SetCharPref(xform->targetPrefName, xform->stringValue);
}

nsresult
nsOperaProfileMigrator::CopyPreferences(PRBool aReplace)
{
  nsCOMPtr<nsIFile> operaPrefs;
  mOperaProfile->Clone(getter_AddRefs(operaPrefs));
  operaPrefs->Append(OPERA_PREFERENCES_FILE_NAME);

  nsCAutoString path;
  operaPrefs->GetNativePath(path);
  nsINIParser* parser = new nsINIParser(path.get());
  if (!parser)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIPrefBranch> branch(do_GetService(NS_PREFSERVICE_CONTRACTID));

  // Traverse the standard transforms
  PrefTransform* transform;
  PrefTransform* end = gTransforms + sizeof(gTransforms)/sizeof(PrefTransform);

  PRInt32 length;
  char* lastSectionName = nsnull;
  for (transform = gTransforms; transform < end; ++transform) {
    if (transform->sectionName)
      lastSectionName = transform->sectionName;

    if (transform->type == _OPM(COLOR)) {
      char* colorString = nsnull;
      nsresult rv = ParseColor(parser, lastSectionName, &colorString);
      if (NS_SUCCEEDED(rv)) {
        transform->stringValue = colorString;
   
        transform->prefHasValue = PR_TRUE;
        transform->prefSetterFunc(transform, branch);
      }
      if (colorString)
        nsCRT::free(colorString);
    }
    else {
      nsXPIDLCString val;
      PRInt32 err = parser->GetStringAlloc(lastSectionName, transform->keyName, getter_Copies(val), &length);
      if (err == nsINIParser::OK) {
        PRInt32 strerr;
        switch (transform->type) {
        case _OPM(STRING):
          transform->stringValue = ToNewCString(val);
          break;
        case _OPM(INT): {
            nsCAutoString valStr; valStr = val;
            transform->intValue = valStr.ToInteger(&strerr);
          }
          break;
        case _OPM(BOOL): {
            nsCAutoString valStr; valStr = val;
            transform->boolValue = valStr.ToInteger(&strerr) != 0;
          }
          break;
        default:
          break;
        }
        transform->prefHasValue = PR_TRUE;
        transform->prefSetterFunc(transform, branch);
        if (transform->type == _OPM(STRING) && transform->stringValue) {
          nsCRT::free(transform->stringValue);
          transform->stringValue = nsnull;
        }
      }
    }
  }
  
  // Copy Proxy Settings
  CopyProxySettings(parser, branch);

  // Copy User Content Sheet
  if (aReplace)
    CopyUserContentSheet(parser);

  delete parser;
  parser = nsnull;
  
  return NS_OK;
}

nsresult
nsOperaProfileMigrator::CopyProxySettings(nsINIParser* aParser, 
                                          nsIPrefBranch* aBranch)
{
  PRInt32 networkProxyType = 0;

  const char* protocols[4] = { "HTTP", "HTTPS", "FTP", "GOPHER" };
  const char* protocols_l[4] = { "http", "https", "ftp", "gopher" };
  char toggleBuf[15], serverBuf[20], serverPrefBuf[20], 
       serverPortPrefBuf[25];
  PRInt32 length, err, enabled;
  for (PRUint32 i = 0; i < 4; ++i) {
    sprintf(toggleBuf, "Use %s", protocols[i]);
    GetInteger(aParser, "Proxy", toggleBuf, &enabled);
    if (enabled) {
      // Enable the "manual configuration" setting if we have at least
      // one protocol using a Proxy. 
      networkProxyType = 1;
    }

    sprintf(serverBuf, "%s Server", protocols[i]);
    nsXPIDLCString proxyServer;
    err = aParser->GetStringAlloc("Proxy", serverBuf, getter_Copies(proxyServer), &length);
    if (err != nsINIParser::OK)
      continue;

    sprintf(serverPrefBuf, "network.proxy.%s", protocols_l[i]);
    sprintf(serverPortPrefBuf, "network.proxy.%s_port", protocols_l[i]);
    SetProxyPref(proxyServer, serverPrefBuf, serverPortPrefBuf, aBranch);
  }

  GetInteger(aParser, "Proxy", "Use Automatic Proxy Configuration", &enabled);
  if (enabled)
    networkProxyType = 2;
  nsXPIDLCString configURL;
  err = aParser->GetStringAlloc("Proxy", "Automatic Proxy Configuration URL", getter_Copies(configURL), &length);
  if (err == nsINIParser::OK)
    aBranch->SetCharPref("network.proxy.autoconfig_url", configURL);

  GetInteger(aParser, "Proxy", "No Proxy Servers Check", &enabled);
  if (enabled) {
    nsXPIDLCString servers;
    err = aParser->GetStringAlloc("Proxy", "No Proxy Servers", getter_Copies(servers), &length);
    if (err == nsINIParser::OK)
      ParseOverrideServers(servers.get(), aBranch);
  }

  aBranch->SetIntPref("network.proxy.type", networkProxyType);

  return NS_OK;
}

nsresult
nsOperaProfileMigrator::GetInteger(nsINIParser* aParser, 
                                   char* aSectionName, 
                                   char* aKeyName, 
                                   PRInt32* aResult)
{
  char val[20];
  PRInt32 length = 20;

  PRInt32 err = aParser->GetString(aSectionName, aKeyName, val, &length);
  if (err != nsINIParser::OK)
    return NS_ERROR_FAILURE;

  nsCAutoString valueStr((char*)val);
  PRInt32 stringErr;
  *aResult = valueStr.ToInteger(&stringErr);

  return NS_OK;
}


nsresult
nsOperaProfileMigrator::ParseColor(nsINIParser* aParser, char* aSectionName, char** aResult)
{
  nsresult rv;
  PRInt32 r, g, b;

  rv = GetInteger(aParser, aSectionName, "Red", &r);
  rv |= GetInteger(aParser, aSectionName, "Green", &g);
  rv |= GetInteger(aParser, aSectionName, "Blue", &b);
  if (NS_FAILED(rv)) 
    return NS_OK; // This Preference has no value. Bail now before we get in trouble.

  *aResult = (char*)malloc(sizeof(char) * 8);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  sprintf(*aResult, "#%02X%02X%02X", r, g, b);

  return NS_OK;
}

nsresult 
nsOperaProfileMigrator::CopyUserContentSheet(nsINIParser* aParser)
{
  nsresult rv = NS_OK;

  nsXPIDLCString userContentCSS;
  PRInt32 size;
  PRInt32 err = aParser->GetStringAlloc("User Prefs", "Local CSS File", getter_Copies(userContentCSS), &size);
  if (err == nsINIParser::OK && userContentCSS.Length() > 0) {
    // Copy the file
    nsCOMPtr<nsILocalFile> userContentCSSFile(do_CreateInstance("@mozilla.org/file/local;1"));
    if (!userContentCSSFile)
      return NS_ERROR_OUT_OF_MEMORY;

    userContentCSSFile->InitWithNativePath(userContentCSS);
    PRBool exists;
    userContentCSSFile->Exists(&exists);
    if (!exists)
      return NS_OK;

    nsCOMPtr<nsIFile> profileChromeDir;
    NS_GetSpecialDirectory(NS_APP_USER_CHROME_DIR,
                           getter_AddRefs(profileChromeDir));
    if (!profileChromeDir)
      return NS_ERROR_OUT_OF_MEMORY;

    rv = userContentCSSFile->CopyToNative(profileChromeDir, nsDependentCString("userContent.css"));
  }
  return rv;
}

nsresult
nsOperaProfileMigrator::CopyCookies(PRBool aReplace)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIFile> temp;
  mOperaProfile->Clone(getter_AddRefs(temp));
  nsCOMPtr<nsILocalFile> historyFile(do_QueryInterface(temp));

  historyFile->Append(OPERA_COOKIES_FILE_NAME);

  nsCOMPtr<nsIInputStream> fileStream;
  NS_NewLocalFileInputStream(getter_AddRefs(fileStream), historyFile);
  if (!fileStream) 
    return NS_ERROR_OUT_OF_MEMORY;

  nsOperaCookieMigrator* ocm = new nsOperaCookieMigrator(fileStream);
  if (!ocm)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = ocm->Migrate();

  if (ocm) {
    delete ocm;
    ocm = nsnull;
  }

  return rv;
}

nsOperaCookieMigrator::nsOperaCookieMigrator(nsIInputStream* aSourceStream) :
  mAppVersion(0), mFileVersion(0), mTagTypeLength(0), mPayloadTypeLength(0), 
  mCookieOpen(PR_FALSE), mCurrHandlingInfo(0)
{
  mStream = do_CreateInstance("@mozilla.org/binaryinputstream;1");
  if (mStream)
    mStream->SetInputStream(aSourceStream);

  mCurrCookie.isSecure = PR_FALSE;
  mCurrCookie.expiryTime = 0;
}

nsOperaCookieMigrator::~nsOperaCookieMigrator() 
{ 
  if (mStream)
    mStream->SetInputStream(nsnull);
}


nsresult
nsOperaCookieMigrator::Migrate()
{
  if (!mStream)
    return NS_ERROR_FAILURE;

  nsresult rv;

  rv = ReadHeader();
  if (NS_FAILED(rv)) 
    return NS_OK;

  nsCOMPtr<nsICookieManager2> manager(do_GetService(NS_COOKIEMANAGER_CONTRACTID));
  nsCOMPtr<nsIPermissionManager> permissionManager(do_GetService("@mozilla.org/permissionmanager;1"));

  PRUint8 tag;
  PRUint16 length, segmentLength;

  char* buf = nsnull;
  do {
    if (NS_FAILED(mStream->Read8(&tag))) 
      return NS_OK; // EOF.

    switch (tag) {
    case BEGIN_DOMAIN_SEGMENT:
      mStream->Read16(&length);
      break;
    case DOMAIN_COMPONENT: 
      {
        mStream->Read16(&length);
        
        mStream->ReadBytes(length, &buf);
        buf = (char*)nsMemory::Realloc(buf, length+1);
        buf[length] = '\0';
        mDomainStack.AppendElement((void*)buf);
      }
      break;
    case END_DOMAIN_SEGMENT:
      {
        if (mCurrHandlingInfo)
          AddCookieOverride(permissionManager);

        // Pop the domain stack
        PRUint32 count = mDomainStack.Count();
        if (count > 0) {
          char* segment = (char*)mDomainStack.ElementAt(count - 1);
          if (segment) 
            nsMemory::Free(segment);
          mDomainStack.RemoveElementAt(count - 1);
        }
      }
      break;

    case BEGIN_PATH_SEGMENT:
      mStream->Read16(&length);
      break;
    case PATH_COMPONENT:
      {
        mStream->Read16(&length);
        
        mStream->ReadBytes(length, &buf);
        buf = (char*)nsMemory::Realloc(buf, length+1);
        buf[length] = '\0';
        mPathStack.AppendElement((void*)buf);
      }
      break;
    case END_PATH_SEGMENT:
      {
        // Add the last remaining cookie for this path.
        if (mCookieOpen) 
          AddCookie(manager);

        // We receive one "End Path Segment" even if the path stack is empty
        // i.e. telling us that we are done processing cookies for "/"

        // Pop the path stack
        PRUint32 count = mPathStack.Count();
        if (count > 0) {
          char* segment = (char*)mPathStack.ElementAt(count - 1);
          if (segment)
            nsMemory::Free(segment);
          mPathStack.RemoveElementAt(count - 1);
        }
      }
      break;

    case FILTERING_INFO:
      mStream->Read16(&length);
      mStream->Read8(&mCurrHandlingInfo);
      break;
    case PATH_HANDLING_INFO:
    case THIRD_PARTY_HANDLING_INFO: 
      {
        mStream->Read16(&length);
        PRUint8 temp;
        mStream->Read8(&temp);
      }
      break;

    case BEGIN_COOKIE_SEGMENT:
      {
        // Be sure to save the last cookie before overwriting the buffers
        // with data from subsequent cookies. 
        if (mCookieOpen)
          AddCookie(manager);

        mStream->Read16(&segmentLength);
        mCookieOpen = PR_TRUE;
      }
      break;
    case COOKIE_ID:
      {
        mStream->Read16(&length);
        mStream->ReadBytes(length, &buf);
        buf = (char*)nsMemory::Realloc(buf, length+1);
        buf[length] = '\0';
        mCurrCookie.id.Assign(buf);
        if (buf) {
          nsMemory::Free(buf);
          buf = nsnull;
        }
      }
      break;
    case COOKIE_DATA:
      {
        mStream->Read16(&length);
        mStream->ReadBytes(length, &buf);
        buf = (char*)nsMemory::Realloc(buf, length+1);
        buf[length] = '\0';
        mCurrCookie.data.Assign(buf);
        if (buf) {
          nsMemory::Free(buf);
          buf = nsnull;
        }
      }
      break;
    case COOKIE_EXPIRY:
      mStream->Read16(&length);
      mStream->Read32(NS_REINTERPRET_CAST(PRUint32*, &(mCurrCookie.expiryTime)));
      break;
    case COOKIE_SECURE:
      mCurrCookie.isSecure = PR_TRUE;
      break;

    // We don't support any of these fields but we must read them in
    // to advance the stream cursor. 
    case COOKIE_LASTUSED: 
      {
        mStream->Read16(&length);
        PRTime temp;
        mStream->Read32(NS_REINTERPRET_CAST(PRUint32*, &temp));
      }
      break;
    case COOKIE_COMMENT:
    case COOKIE_COMMENT_URL:
    case COOKIE_V1_DOMAIN:
    case COOKIE_V1_PATH:
    case COOKIE_V1_PORT_LIMITATIONS:
      {
        mStream->Read16(&length);
        mStream->ReadBytes(length, &buf);
        if (buf) {
          nsMemory::Free(buf);
          buf = nsnull;
        }
      }
      break;
    case COOKIE_VERSION: 
      {
        mStream->Read16(&length);
        PRUint8 temp;
        mStream->Read8(&temp);
      }
      break;
    case COOKIE_OTHERFLAG_1:
    case COOKIE_OTHERFLAG_2:
    case COOKIE_OTHERFLAG_3:
    case COOKIE_OTHERFLAG_4:
    case COOKIE_OTHERFLAG_5:
    case COOKIE_OTHERFLAG_6: 
      break;
    }
  }
  while (1);
  
  // Make sure the path and domain stacks are clear. 
  char* segment = nsnull;
  PRUint32 i;
  PRUint32 count = mPathStack.Count();
  for (i = 0; i < count; ++i) {
    segment = (char*)mPathStack.ElementAt(i);
    if (segment) {
      nsMemory::Free(segment);
      segment = nsnull;
    }
  }
  count = mDomainStack.Count();
  for (i = 0; i < count; ++i) {
    segment = (char*)mDomainStack.ElementAt(i);
    if (segment) {
      nsMemory::Free(segment);
      segment = nsnull;
    }
  }
}

nsresult
nsOperaCookieMigrator::AddCookieOverride(nsIPermissionManager* aManager)
{
  nsresult rv;

  nsXPIDLCString domain;
  SynthesizeDomain(getter_Copies(domain));
  nsCOMPtr<nsIURI> uri(do_CreateInstance("@mozilla.org/network/standard-url;1"));
  if (!uri)
    return NS_ERROR_OUT_OF_MEMORY;
  uri->SetHost(domain);

  rv = aManager->Add(uri, "cookie", 
                     (mCurrHandlingInfo == 1 || mCurrHandlingInfo == 3) ? nsIPermissionManager::ALLOW_ACTION :
                                                                          nsIPermissionManager::DENY_ACTION);

  mCurrHandlingInfo = 0;

  return rv;
}


nsresult
nsOperaCookieMigrator::AddCookie(nsICookieManager2* aManager)
{
  // This is where we use the information gathered in all the other 
  // states to add a cookie to the Firebird Cookie Manager.
  nsXPIDLCString domain;
  SynthesizeDomain(getter_Copies(domain));

  nsXPIDLCString path;
  SynthesizePath(getter_Copies(path));

  mCookieOpen = PR_FALSE;
  
  nsresult rv = aManager->Add(domain, 
                              path, 
                              mCurrCookie.id, 
                              mCurrCookie.data, 
                              mCurrCookie.isSecure, 
                              PR_FALSE, 
                              PRInt64(mCurrCookie.expiryTime));

  mCurrCookie.isSecure = 0;
  mCurrCookie.expiryTime = 0;

  return rv;
}

void
nsOperaCookieMigrator::SynthesizePath(char** aResult)
{
  PRUint32 count = mPathStack.Count();
  nsCAutoString synthesizedPath("/");
  for (PRUint32 i = 0; i < count; ++i) {
    synthesizedPath.Append((char*)mPathStack.ElementAt(i));
    if (i != count-1)
      synthesizedPath.Append("/");
  }
  if (synthesizedPath.IsEmpty())
    synthesizedPath.Assign("/");

  *aResult = ToNewCString(synthesizedPath);
}

void
nsOperaCookieMigrator::SynthesizeDomain(char** aResult)
{
  PRUint32 count = mDomainStack.Count();
  if (count == 0)
    return;

  nsCAutoString synthesizedDomain;
  for (PRInt32 i = (PRInt32)count - 1; i >= 0; --i) {
    synthesizedDomain.Append((char*)mDomainStack.ElementAt((PRUint32)i));
    if (i != 0)
      synthesizedDomain.Append(".");
  }

  *aResult = ToNewCString(synthesizedDomain);
}

nsresult
nsOperaCookieMigrator::ReadHeader()
{
  mStream->Read32(&mAppVersion);
  mStream->Read32(&mFileVersion);

  if (mAppVersion & 0x1000 && mFileVersion & 0x2000) {
    mStream->Read16(&mTagTypeLength);
    mStream->Read16(&mPayloadTypeLength);

    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsOperaProfileMigrator::CopyHistory(PRBool aReplace)
{
  nsCOMPtr<nsIBrowserHistory> hist(do_GetService(kGlobalHistoryCID));

  nsCOMPtr<nsIFile> temp;
  mOperaProfile->Clone(getter_AddRefs(temp));
  nsCOMPtr<nsILocalFile> historyFile(do_QueryInterface(temp));
  historyFile->Append(OPERA_HISTORY_FILE_NAME);

  nsCOMPtr<nsIInputStream> fileStream;
  NS_NewLocalFileInputStream(getter_AddRefs(fileStream), historyFile);
  if (!fileStream) return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsILineInputStream> lineStream = do_QueryInterface(fileStream);

  nsCAutoString buffer, url;
  nsAutoString title;
  PRTime lastVisitDate;
  PRBool moreData = PR_FALSE;

  enum { TITLE, URL, LASTVISIT } state = TITLE;

  // Format is "title\nurl\nlastvisitdate"
  do {
    nsresult rv = lineStream->ReadLine(buffer, &moreData);
    if (NS_FAILED(rv))
      return rv;

    switch (state) {
    case TITLE:
      CopyUTF8toUTF16(buffer, title);
      state = URL;
      break;
    case URL:
      url = buffer;
      state = LASTVISIT;
      break;
    case LASTVISIT:
      // Opera time format is a second offset, PRTime is a microsecond offset
      PRInt32 err;
      lastVisitDate = buffer.ToInteger(&err);
      
      PRInt64 temp, million;
      LL_I2L(temp, lastVisitDate);
      LL_I2L(million, PR_USEC_PER_SEC);
      LL_MUL(lastVisitDate, temp, million);

      nsCOMPtr<nsIURI> uri;
      NS_NewURI(getter_AddRefs(uri), url);
      if (uri)
        hist->AddPageWithDetails(uri, title.get(), lastVisitDate);
      
      state = TITLE;
      break;
    }
  }
  while (moreData);

  return NS_OK;
}

nsresult
nsOperaProfileMigrator::CopyBookmarks(PRBool aReplace)
{
  // Find Opera Bookmarks
  nsCOMPtr<nsIFile> operaBookmarks;
  mOperaProfile->Clone(getter_AddRefs(operaBookmarks));
  operaBookmarks->Append(OPERA_BOOKMARKS_FILE_NAME);

  nsCOMPtr<nsIInputStream> fileInputStream;
  NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), operaBookmarks);
  if (!fileInputStream) return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsILineInputStream> lineInputStream(do_QueryInterface(fileInputStream));

  nsCOMPtr<nsIBookmarksService> bms(do_GetService("@mozilla.org/browser/bookmarks-service;1"));
  NS_ENSURE_TRUE(bms, NS_ERROR_FAILURE);
  PRBool dummy;
  bms->ReadBookmarks(&dummy);

  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(kStringBundleServiceCID));
  nsCOMPtr<nsIStringBundle> bundle;
  bundleService->CreateBundle(MIGRATION_BUNDLE, getter_AddRefs(bundle));

  nsCOMPtr<nsIRDFService> rdf(do_GetService("@mozilla.org/rdf/rdf-service;1"));
  nsCOMPtr<nsIRDFResource> root;
  rdf->GetResource(NS_LITERAL_CSTRING("NC:BookmarksRoot"), 
                   getter_AddRefs(root));
  nsCOMPtr<nsIRDFResource> parentFolder;
  if (!aReplace) {
    nsXPIDLString sourceNameOpera;
    bundle->GetStringFromName(NS_LITERAL_STRING("sourceNameOpera").get(), 
                              getter_Copies(sourceNameOpera));

    const PRUnichar* sourceNameStrings[] = { sourceNameOpera.get() };
    nsXPIDLString importedOperaHotlistTitle;
    bundle->FormatStringFromName(NS_LITERAL_STRING("importedBookmarksFolder").get(),
                                 sourceNameStrings, 1, 
                                 getter_Copies(importedOperaHotlistTitle));

    bms->CreateFolderInContainer(importedOperaHotlistTitle.get(), 
                                 root, -1, getter_AddRefs(parentFolder));
  }
  else
    parentFolder = root;

#if defined(XP_WIN) || (defined(XP_UNIX) && !defined(XP_MACOSX))
  printf("*** about to copy smart keywords\n");
  CopySmartKeywords(bms, bundle, parentFolder);
  printf("*** done copying smart keywords\n");
#endif

  nsCOMPtr<nsIRDFResource> toolbar;
  bms->GetBookmarksToolbarFolder(getter_AddRefs(toolbar));
  
  if (aReplace)
    ClearToolbarFolder(bms, toolbar);

  return ParseBookmarksFolder(lineInputStream, parentFolder, toolbar, bms);
}

#if defined(XP_WIN) || (defined(XP_UNIX) && !defined(XP_MACOSX))
nsresult
nsOperaProfileMigrator::CopySmartKeywords(nsIBookmarksService* aBMS, 
                                          nsIStringBundle* aBundle, 
                                          nsIRDFResource* aParentFolder)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIFile> smartKeywords;
  mOperaProfile->Clone(getter_AddRefs(smartKeywords));
  smartKeywords->Append(NS_LITERAL_STRING("search.ini"));

  nsCAutoString path;
  smartKeywords->GetNativePath(path);
  nsINIParser* parser = new nsINIParser(path.get());
  if (!parser)
    return NS_ERROR_OUT_OF_MEMORY;

  nsXPIDLString sourceNameOpera;
  aBundle->GetStringFromName(NS_LITERAL_STRING("sourceNameOpera").get(), 
                             getter_Copies(sourceNameOpera));

  const PRUnichar* sourceNameStrings[] = { sourceNameOpera.get() };
  nsXPIDLString importedSearchUrlsTitle;
  aBundle->FormatStringFromName(NS_LITERAL_STRING("importedSearchURLsFolder").get(),
                                sourceNameStrings, 1, 
                                getter_Copies(importedSearchUrlsTitle));

  nsCOMPtr<nsIRDFResource> keywordsFolder;
  aBMS->CreateFolderInContainer(importedSearchUrlsTitle.get(), 
                                aParentFolder, -1, getter_AddRefs(keywordsFolder));

  PRInt32 sectionIndex = 1;
  char section[35];
  nsXPIDLCString name, url, keyword;
  PRInt32 keyValueLength = 0;
  do {
    sprintf(section, "Search Engine %d", sectionIndex++);
    PRInt32 err = parser->GetStringAlloc(section, "Name", getter_Copies(name), &keyValueLength);
    if (err != nsINIParser::OK)
      break;

    err = parser->GetStringAlloc(section, "URL", getter_Copies(url), &keyValueLength);
    if (err != nsINIParser::OK)
      continue;
    err = parser->GetStringAlloc(section, "Key", getter_Copies(keyword), &keyValueLength);
    if (err != nsINIParser::OK)
      continue;

    PRInt32 post;
    err = GetInteger(parser, section, "Is post", &post);
    if (post)
      continue;

    if (url.IsEmpty() || keyword.IsEmpty() || name.IsEmpty())
      continue;

    nsAutoString nameStr; nameStr.Assign(NS_ConvertUTF8toUCS2(name));
    PRUint32 length = nameStr.Length();
    PRInt32 index = 0; 
    do {
      index = nameStr.FindChar('&', index);
      if (index >= length - 2)
        break;

      // Assume "&&" is an escaped ampersand in the search query title. 
      if (nameStr.CharAt(index + 1) == '&') {
        nameStr.Cut(index, 1);
        index += 2;
        continue;
      }

      nameStr.Cut(index, 1);
    }
    while (index < length);

    nsCOMPtr<nsIRDFResource> itemRes;

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), url.get());
    if (!uri)
      return NS_ERROR_OUT_OF_MEMORY;
    nsCAutoString hostCStr;
    uri->GetHost(hostCStr);
    nsAutoString host; host.AssignWithConversion(hostCStr.get());

    const PRUnichar* descStrings[] = { NS_ConvertUTF8toUCS2(keyword).get(), host.get() };
    nsXPIDLString keywordDesc;
    aBundle->FormatStringFromName(NS_LITERAL_STRING("importedSearchUrlDesc").get(),
                                  descStrings, 2, getter_Copies(keywordDesc));

    // XXX We don't know for sure how Opera deals with IDN hostnames in URL.
    // Assuming it's in UTF-8 is rather safe because it covers two cases 
    // (UTF-8 and ASCII) out of three cases (the last is a non-UTF-8
    // multibyte encoding).
    rv = aBMS->CreateBookmarkInContainer(nameStr.get(), 
                                         NS_ConvertUTF8toUTF16(url).get(), 
                                         NS_ConvertUTF8toUTF16(keyword).get(), 
                                         keywordDesc.get(), 
                                         nsnull, 
                                         nsnull,
                                         keywordsFolder,
                                         -1, 
                                         getter_AddRefs(itemRes));
  }
  while (1);
  
  if (parser) {
    delete parser; 
    parser = nsnull;
  }

  return rv;
}
#endif

void
nsOperaProfileMigrator::ClearToolbarFolder(nsIBookmarksService* aBookmarksService, nsIRDFResource* aToolbarFolder)
{
  // If we're here, it means the user's doing a _replace_ import which means
  // clear out the content of this folder, and replace it with the new content
  nsCOMPtr<nsIRDFContainer> ctr(do_CreateInstance("@mozilla.org/rdf/container;1"));
  nsCOMPtr<nsIRDFDataSource> bmds(do_QueryInterface(aBookmarksService));
  ctr->Init(bmds, aToolbarFolder);

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

typedef enum { LineType_FOLDER, 
               LineType_BOOKMARK, 
               LineType_SEPARATOR, 
               LineType_NAME, 
               LineType_URL, 
               LineType_KEYWORD,
               LineType_DESCRIPTION,
               LineType_ONTOOLBAR,
               LineType_NL,
               LineType_OTHER } LineType;

static LineType GetLineType(nsACString& aBuffer, nsACString& aResult)
{
  if (Substring(aBuffer, 0, 7).EqualsLiteral("#FOLDER"))
    return LineType_FOLDER;
  if (Substring(aBuffer, 0, 4).EqualsLiteral("#URL"))
    return LineType_BOOKMARK;
  if (Substring(aBuffer, 0, 1).EqualsLiteral("-"))
    return LineType_SEPARATOR;
  if (Substring(aBuffer, 1, 5).EqualsLiteral("NAME=")) {
    aResult.Assign(Substring(aBuffer, 6, aBuffer.Length() - 6));
    return LineType_NAME;
  }
  if (Substring(aBuffer, 1, 4).EqualsLiteral("URL=")) {
    aResult.Assign(Substring(aBuffer, 5, aBuffer.Length() - 5));
    return LineType_URL;
  }
  if (Substring(aBuffer, 1, 12).EqualsLiteral("DESCRIPTION=")) {
    aResult.Assign(Substring(aBuffer, 13, aBuffer.Length() - 13));
    return LineType_DESCRIPTION;
  }
  if (Substring(aBuffer, 1, 11).EqualsLiteral("SHORT NAME=")) {
    aResult.Assign(Substring(aBuffer, 12, aBuffer.Length() - 12));
    return LineType_KEYWORD;
  }
  if (Substring(aBuffer, 1, 15).EqualsLiteral("ON PERSONALBAR=")) {
    aResult.Assign(Substring(aBuffer, 16, aBuffer.Length() - 16));
    return LineType_ONTOOLBAR;
  }
  if (aBuffer.IsEmpty())
    return LineType_NL; // Newlines separate bookmarks
  return LineType_OTHER;
}

typedef enum { EntryType_BOOKMARK, EntryType_FOLDER } EntryType;

nsresult
nsOperaProfileMigrator::ParseBookmarksFolder(nsILineInputStream* aStream, 
                                             nsIRDFResource* aParent, 
                                             nsIRDFResource* aToolbar,
                                             nsIBookmarksService* aBMS)
{
  nsresult rv;
  PRBool moreData = PR_FALSE;
  nsCAutoString buffer, result;
  EntryType entryType = EntryType_BOOKMARK;
  nsAutoString name, keyword, description, url;
  PRBool onToolbar = PR_FALSE;
  do {
    rv = aStream->ReadLine(buffer, &moreData);
    if (NS_FAILED(rv)) return rv;
    
    if (!moreData) break;

    LineType type = GetLineType(buffer, result);
    switch(type) {
    case LineType_FOLDER:
      entryType = EntryType_FOLDER;
      break;
    case LineType_BOOKMARK:
      entryType = EntryType_BOOKMARK;
      break;
    case LineType_SEPARATOR:
      // If we're here, we need to break out of the loop for the current folder, 
      // essentially terminating this instance of ParseBookmarksFolder and return
      // to the calling function, which is either ParseBookmarksFolder for a parent
      // folder, or CopyBookmarks (which means we're done parsing all bookmarks).
      goto done;
    case LineType_NAME:
      CopyUTF8toUTF16(result, name);
      break;
    case LineType_URL:
      CopyUTF8toUTF16(result, url);
      break;
    case LineType_KEYWORD:
      CopyUTF8toUTF16(result, keyword);
      break;
    case LineType_DESCRIPTION:
      CopyUTF8toUTF16(result, description);
      break;
    case LineType_ONTOOLBAR:
      if (result.EqualsLiteral("YES"))
        onToolbar = PR_TRUE;
      break;
    case LineType_NL: {
      // XXX We don't know for sure how Opera deals with IDN hostnames in URL.
      // Assuming it's in UTF-8 is rather safe because it covers two cases 
      // (UTF-8 and ASCII) out of three cases (the last is a non-UTF-8
      // multibyte encoding).
      nsCOMPtr<nsIRDFResource> itemRes;
      if (entryType == EntryType_BOOKMARK) {
        if (!name.IsEmpty() && !url.IsEmpty()) {
          rv = aBMS->CreateBookmarkInContainer(name.get(), 
                                               url.get(), 
                                               keyword.get(), 
                                               description.get(), 
                                               nsnull, 
                                               nsnull,
                                               onToolbar ? aToolbar : aParent, 
                                               -1, 
                                               getter_AddRefs(itemRes));
          name.Truncate();
          url.Truncate();
          keyword.Truncate();
          description.Truncate();
          if (NS_FAILED(rv))
            continue;
        }
      }
      else if (entryType == EntryType_FOLDER) {
        if (!name.IsEmpty()) {
          rv = aBMS->CreateFolderInContainer(name.get(), 
                                             onToolbar ? aToolbar : aParent, 
                                             -1, 
                                             getter_AddRefs(itemRes));
          name.Truncate();
          if (NS_FAILED(rv)) 
            continue;
          rv = ParseBookmarksFolder(aStream, itemRes, aToolbar, aBMS);
        }
      }
      break;
    }
    case LineType_OTHER:
      break;
    }
  }
  while (1);

done:
  return rv;
}

void
nsOperaProfileMigrator::GetOperaProfile(const PRUnichar* aProfile, nsILocalFile** aFile)
{
  nsCOMPtr<nsIProperties> fileLocator(do_GetService("@mozilla.org/file/directory_service;1"));
  nsCOMPtr<nsILocalFile> file;
#ifdef XP_WIN
  fileLocator->Get(NS_WIN_APPDATA_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(file));

  // Opera profile lives under %APP_DATA%\Opera\<operaver>\profile 
  file->Append(OPERA_PREFERENCES_FOLDER_NAME);
  file->Append(nsDependentString(aProfile));
  file->Append(NS_LITERAL_STRING("profile"));
#elif defined (XP_MACOSX)
  fileLocator->Get(NS_MAC_USER_LIB_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(file));
    
  file->Append(NS_LITERAL_STRING("Preferences"));
  file->Append(OPERA_PREFERENCES_FOLDER_NAME);
#elif defined (XP_UNIX)
  fileLocator->Get(NS_UNIX_HOME_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(file));
    
  file->Append(OPERA_PREFERENCES_FOLDER_NAME);
#endif    

  *aFile = file;
  NS_ADDREF(*aFile);
}

