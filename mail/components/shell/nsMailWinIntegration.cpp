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
 * The Original Code is Thunderbird Windows Integration.
 *
 * The Initial Developer of the Original Code is
 *   Scott MacGregor <mscott@mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#include "nsMailWinIntegration.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsCRT.h"
#include "nsIStringBundle.h"
#include "nsNativeCharsetUtils.h"
#include "nsIPrefService.h"
#ifndef __MINGW32__
#include "nsIMapiSupport.h"
#endif
#include "shlobj.h"
#include "windows.h"
#include "shellapi.h"
#include "nsILocalFile.h"

#include <mbstring.h>

#ifndef MAX_BUF
#define MAX_BUF 4096
#endif

#define REG_FAILED(val) \
  (val != ERROR_SUCCESS)

NS_IMPL_ISUPPORTS1(nsWindowsShellService, nsIShellService)

static nsresult
OpenUserKeyForReading(HKEY aStartKey, const char* aKeyName, HKEY* aKey)
{
  DWORD result = ::RegOpenKeyEx(aStartKey, aKeyName, 0, KEY_READ, aKey);

  switch (result) 
  {
    case ERROR_SUCCESS:
      break;
    case ERROR_ACCESS_DENIED:
      return NS_ERROR_FILE_ACCESS_DENIED;
    case ERROR_FILE_NOT_FOUND:
      if (aStartKey == HKEY_LOCAL_MACHINE) 
      {
        // prevent infinite recursion on the second pass through here if 
        // ::RegOpenKeyEx fails in the all-users case. 
        return NS_ERROR_NOT_AVAILABLE;
      }
      return OpenUserKeyForReading(HKEY_LOCAL_MACHINE, aKeyName, aKey);
  }
  return NS_OK;
}

// Sets the default mail registry keys for Windows versions prior to Vista.
// Try to open / create the key in HKLM and if that fails try to do the same
// in HKCU. Though this is not strictly the behavior I would expect it is the
// same behavior that Firefox and IE has when setting the default browser previous to Vista.
static nsresult OpenKeyForWriting(HKEY aStartKey, const char* aKeyName, HKEY* aKey, PRBool aHKLMOnly)
{
  DWORD dwDisp = 0;
  DWORD rv = ::RegCreateKeyEx(aStartKey, aKeyName, 0, NULL, 0,
                              KEY_READ | KEY_WRITE, NULL, aKey, &dwDisp);

  switch (rv) 
  {
    case ERROR_SUCCESS:
      break;
    case ERROR_ACCESS_DENIED:
      if (aHKLMOnly || aStartKey == HKEY_CURRENT_USER)
        return NS_ERROR_FILE_ACCESS_DENIED;
      // fallback to HKCU immediately on access denied since we won't be able
      // to create the key.
      return OpenKeyForWriting(HKEY_CURRENT_USER, aKeyName, aKey, aHKLMOnly);
    case ERROR_FILE_NOT_FOUND:
      rv = ::RegCreateKey(aStartKey, aKeyName, aKey);
      if (rv != ERROR_SUCCESS) 
      {
        if (aHKLMOnly || aStartKey == HKEY_CURRENT_USER) 
        {
          // prevent infinite recursion on the second pass through here if 
          // ::RegCreateKey fails in the current user case.
          return NS_ERROR_FILE_ACCESS_DENIED;
        }
        return OpenKeyForWriting(HKEY_CURRENT_USER, aKeyName, aKey, aHKLMOnly);
      }
  }

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Default Mail Registry Settings
///////////////////////////////////////////////////////////////////////////////

typedef enum { NO_SUBSTITUTION    = 0x00,
               APP_PATH_SUBSTITUTION  = 0x01,
               APPNAME_SUBSTITUTION = 0x02,
               UNINST_PATH_SUBSTITUTION  = 0x04,
               MAPIDLL_PATH_SUBSTITUTION = 0x08,
               HKLM_ONLY = 0x10,
               USE_FOR_DEFAULT_TEST = 0x20} SettingFlags;

#define CLS "SOFTWARE\\Classes\\"
#define MAILCLIENTS "SOFTWARE\\Clients\\Mail\\"
#define NEWSCLIENTS "SOFTWARE\\Clients\\News\\"
#define MOZ_CLIENT_MAIL_KEY "Software\\Clients\\Mail"
#define MOZ_CLIENT_NEWS_KEY "Software\\Clients\\News"
#define DI "\\DefaultIcon"
#define II "\\InstallInfo" 

// APP_REG_NAME_MAIL and APP_REG_NAME_NEWS should be kept in synch with
// AppRegNameMail and AppRegNameNews in the installer file: defines.nsi.in
#define APP_REG_NAME_MAIL L"Thunderbird"
#define APP_REG_NAME_NEWS L"Thunderbird (News)"
#define CLS_EML "ThunderbirdEML"
#define CLS_MAILTOURL "Thunderbird.Url.mailto"
#define CLS_NEWSURL "Thunderbird.Url.news"
#define CLS_FEEDURL "Thunderbird.Url.feed"
#define SOP "\\shell\\open\\command"

// For the InstallInfo HideIconsCommand, ShowIconsCommand, and ReinstallCommand
// registry keys. This must be kept in sync with the uninstaller.
#define UNINSTALL_EXE "\\uninstall\\helper.exe"
#define EXE "thunderbird.exe"

#define VAL_URL_ICON "%APPPATH%,0"
#define VAL_FILE_ICON "%APPPATH%,0"
#define VAL_OPEN "\"%APPPATH%\" \"%1\""

#define MAKE_KEY_NAME1(PREFIX, MID) \
  PREFIX MID

#define MAKE_KEY_NAME2(PREFIX, MID, SUFFIX) \
  PREFIX MID SUFFIX

static SETTING gMailSettings[] = {
  // File Extension Aliases
  { MAKE_KEY_NAME1(CLS, ".eml"),    "", CLS_EML, NO_SUBSTITUTION },

  // File Extension Class
  { MAKE_KEY_NAME2(CLS, CLS_EML, DI),  "",  VAL_FILE_ICON, APP_PATH_SUBSTITUTION },
  { MAKE_KEY_NAME2(CLS, CLS_EML, SOP), "",  VAL_OPEN, APP_PATH_SUBSTITUTION},

  // Protocol Handler Class - for Vista and above
  { MAKE_KEY_NAME2(CLS, CLS_MAILTOURL, DI),  "", VAL_URL_ICON, APP_PATH_SUBSTITUTION },
  { MAKE_KEY_NAME2(CLS, CLS_MAILTOURL, SOP), "", "\"%APPPATH%\" -compose \"%1\"", APP_PATH_SUBSTITUTION },
  
  // Protocol Handlers
  { MAKE_KEY_NAME2(CLS, "mailto", DI),  "", VAL_URL_ICON, APP_PATH_SUBSTITUTION},
  { MAKE_KEY_NAME2(CLS, "mailto", SOP), "", "\"%APPPATH%\" -compose \"%1\"", APP_PATH_SUBSTITUTION | USE_FOR_DEFAULT_TEST}, 

  // Mail Client Keys
  { MAKE_KEY_NAME1(MAILCLIENTS, "%APPNAME%"),  
    "DLLPath", 
    "%MAPIDLLPATH%", 
    MAPIDLL_PATH_SUBSTITUTION | HKLM_ONLY | APPNAME_SUBSTITUTION },
  { MAKE_KEY_NAME2(MAILCLIENTS, "%APPNAME%", II),
    "HideIconsCommand",
    "\"%UNINSTPATH%\" /HideShortcuts",
    UNINST_PATH_SUBSTITUTION | APPNAME_SUBSTITUTION | HKLM_ONLY },
  { MAKE_KEY_NAME2(MAILCLIENTS, "%APPNAME%", II),
    "ReinstallCommand",
    "\"%UNINSTPATH%\" /SetAsDefaultAppGlobal",
    UNINST_PATH_SUBSTITUTION | APPNAME_SUBSTITUTION | HKLM_ONLY },
  { MAKE_KEY_NAME2(MAILCLIENTS, "%APPNAME%", II),
    "ShowIconsCommand",
    "\"%UNINSTPATH%\" /ShowShortcuts",
    UNINST_PATH_SUBSTITUTION | APPNAME_SUBSTITUTION | HKLM_ONLY },
  { MAKE_KEY_NAME2(MAILCLIENTS, "%APPNAME%", DI),  
    "", 
    "%APPPATH%,0", 
    APP_PATH_SUBSTITUTION | APPNAME_SUBSTITUTION | HKLM_ONLY },
  { MAKE_KEY_NAME2(MAILCLIENTS, "%APPNAME%", SOP), 
    "", 
    "\"%APPPATH%\" -mail",   
    APP_PATH_SUBSTITUTION | APPNAME_SUBSTITUTION | HKLM_ONLY },
  { MAKE_KEY_NAME1(MAILCLIENTS, "%APPNAME%\\shell\\properties\\command"),
    "", 
    "\"%APPPATH%\" -options",   
    APP_PATH_SUBSTITUTION | APPNAME_SUBSTITUTION | HKLM_ONLY },
};

static SETTING gNewsSettings[] = {
   // Protocol Handler Class - for Vista and above
  { MAKE_KEY_NAME2(CLS, CLS_NEWSURL, DI),  "", VAL_URL_ICON, APP_PATH_SUBSTITUTION },
  { MAKE_KEY_NAME2(CLS, CLS_NEWSURL, SOP), "", "\"%APPPATH%\" -mail \"%1\"", APP_PATH_SUBSTITUTION },

  // Protocol Handlers
  { MAKE_KEY_NAME2(CLS, "news", DI),  "", VAL_URL_ICON, APP_PATH_SUBSTITUTION},
  { MAKE_KEY_NAME2(CLS, "news", SOP), "", "\"%APPPATH%\" -mail \"%1\"", APP_PATH_SUBSTITUTION | USE_FOR_DEFAULT_TEST},
  { MAKE_KEY_NAME2(CLS, "nntp", DI),  "", VAL_URL_ICON, APP_PATH_SUBSTITUTION},
  { MAKE_KEY_NAME2(CLS, "nntp", SOP), "", "\"%APPPATH%\" -mail \"%1\"", APP_PATH_SUBSTITUTION | USE_FOR_DEFAULT_TEST}, 
  { MAKE_KEY_NAME2(CLS, "snews", DI),  "", VAL_URL_ICON, APP_PATH_SUBSTITUTION},
  { MAKE_KEY_NAME2(CLS, "snews", SOP), "", "\"%APPPATH%\"-mail \"%1\"", APP_PATH_SUBSTITUTION}, 

  // News Client Keys
  { MAKE_KEY_NAME1(NEWSCLIENTS, "%APPNAME%"),  
    "DLLPath", 
    "%MAPIDLLPATH%", 
    MAPIDLL_PATH_SUBSTITUTION | APPNAME_SUBSTITUTION | HKLM_ONLY },
  { MAKE_KEY_NAME2(NEWSCLIENTS, "%APPNAME%", DI),  
    "", 
    "%APPPATH%,0", 
    APP_PATH_SUBSTITUTION | APPNAME_SUBSTITUTION | HKLM_ONLY },
  { MAKE_KEY_NAME2(NEWSCLIENTS, "%APPNAME%", SOP), 
    "", 
    "\"%APPPATH%\" -mail",   
    APP_PATH_SUBSTITUTION | APPNAME_SUBSTITUTION | HKLM_ONLY },
};

static SETTING gFeedSettings[] = {
   // Protocol Handler Class - for Vista and above
  { MAKE_KEY_NAME2(CLS, CLS_FEEDURL, DI),  "", VAL_URL_ICON, APP_PATH_SUBSTITUTION },
  { MAKE_KEY_NAME2(CLS, CLS_FEEDURL, SOP), "", VAL_OPEN, APP_PATH_SUBSTITUTION },
  
  // Protocol Handlers
  { MAKE_KEY_NAME2(CLS, "feed", DI),  "", VAL_URL_ICON, APP_PATH_SUBSTITUTION},
  { MAKE_KEY_NAME2(CLS, "feed", SOP), "", "%APPPATH% -mail \"%1\"", APP_PATH_SUBSTITUTION | USE_FOR_DEFAULT_TEST},
};

nsresult nsWindowsShellService::Init()
{
  nsresult rv;
  
  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService("@mozilla.org/intl/stringbundle;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIStringBundle> bundle, brandBundle;
  rv = bundleService->CreateBundle("chrome://branding/locale/brand.properties", getter_AddRefs(brandBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  brandBundle->GetStringFromName(NS_LITERAL_STRING("brandFullName").get(),
                                 getter_Copies(mBrandFullName));
  brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                 getter_Copies(mBrandShortName));

  char appPath[MAX_BUF];
  if (!::GetModuleFileName(0, appPath, MAX_BUF))
    return NS_ERROR_FAILURE;

  mAppLongPath = appPath;

  nsCOMPtr<nsILocalFile> lf;
  rv = NS_NewNativeLocalFile(mAppLongPath, PR_TRUE,
                                      getter_AddRefs(lf));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> appDir;
  rv = lf->GetParent(getter_AddRefs(appDir));
  NS_ENSURE_SUCCESS(rv, rv);

  appDir->GetNativePath(mUninstallPath);
  mUninstallPath.Append(UNINSTALL_EXE);

  // Support short path to the exe so if it is already set the user is not
  // prompted to set the default mail client again.
  if (!::GetShortPathName(appPath, appPath, MAX_BUF))
    return NS_ERROR_FAILURE;

  ToUpperCase(mAppShortPath = appPath);

  rv = NS_NewNativeLocalFile(mAppLongPath, PR_TRUE, getter_AddRefs(lf));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = lf->SetNativeLeafName(nsDependentCString("mozMapi32.dll"));
  NS_ENSURE_SUCCESS(rv, rv);

  return lf->GetNativePath(mMapiDLLPath);
}

nsWindowsShellService::nsWindowsShellService()
:mCheckedThisSession(PR_FALSE)
{
}

NS_IMETHODIMP
nsWindowsShellService::IsDefaultClient(PRBool aStartupCheck, PRUint16 aApps, PRBool *aIsDefaultClient)
{
  if (IsDefaultClientVista(aStartupCheck, aApps, aIsDefaultClient))
    return NS_OK;

  *aIsDefaultClient = PR_TRUE;

  // for each type, 
  if (aApps & nsIShellService::MAIL)
    *aIsDefaultClient &= TestForDefault(gMailSettings, sizeof(gMailSettings)/sizeof(SETTING));
  if (aApps & nsIShellService::NEWS)
    *aIsDefaultClient &= TestForDefault(gNewsSettings, sizeof(gNewsSettings)/sizeof(SETTING));
  if (aApps & nsIShellService::RSS)
    *aIsDefaultClient &= TestForDefault(gFeedSettings, sizeof(gFeedSettings)/sizeof(SETTING));

  // If this is the first mail window, maintain internal state that we've
  // checked this session (so that subsequent window opens don't show the 
  // default client dialog).
  if (aStartupCheck)
    mCheckedThisSession = PR_TRUE;

  return NS_OK;
}

DWORD
nsWindowsShellService::DeleteRegKeyDefaultValue(HKEY baseKey, const char *keyName)
{
  HKEY key;
  DWORD rc = ::RegOpenKeyEx(baseKey, keyName, 0, KEY_WRITE, &key);
  if (rc == ERROR_SUCCESS) {
    rc = ::RegDeleteValue(key, "");
    ::RegCloseKey(key);
  }
  return rc;
}

NS_IMETHODIMP
nsWindowsShellService::SetDefaultClient(PRBool aForAllUsers, PRUint16 aApps)
{
  // Delete the protocol and file handlers under HKCU if they exist. This way
  // the HKCU registry is cleaned up when HKLM is writeable or if it isn't
  // the values will then be added under HKCU.
  if (aApps & nsIShellService::MAIL)
  {
    (void)DeleteRegKey(HKEY_CURRENT_USER, "Software\\Classes\\ThunderbirdEML");
    (void)DeleteRegKeyDefaultValue(HKEY_CURRENT_USER, "Software\\Classes\\.eml");
    (void)DeleteRegKey(HKEY_CURRENT_USER, "Software\\Classes\\mailto\\shell\\open");
    (void)DeleteRegKey(HKEY_CURRENT_USER, "Software\\Classes\\mailto\\DefaultIcon");
  }

  if (aApps & nsIShellService::NEWS)
  {
    (void)DeleteRegKey(HKEY_CURRENT_USER, "Software\\Classes\\news\\shell\\open");
    (void)DeleteRegKey(HKEY_CURRENT_USER, "Software\\Classes\\news\\DefaultIcon");
    (void)DeleteRegKey(HKEY_CURRENT_USER, "Software\\Classes\\snews\\shell\\open");
    (void)DeleteRegKey(HKEY_CURRENT_USER, "Software\\Classes\\snews\\DefaultIcon");
    (void)DeleteRegKey(HKEY_CURRENT_USER, "Software\\Classes\\nntp\\shell\\open");
    (void)DeleteRegKey(HKEY_CURRENT_USER, "Software\\Classes\\nntp\\DefaultIcon");
  }

  if (aApps & nsIShellService::RSS)
  {
    (void)DeleteRegKey(HKEY_CURRENT_USER, "Software\\Classes\\feed\\shell\\open");
    (void)DeleteRegKey(HKEY_CURRENT_USER, "Software\\Classes\\feed\\DefaultIcon");
  }

  if (SetDefaultClientVista(aApps))
    return NS_OK;

  nsresult rv = NS_OK;
  if (aApps & nsIShellService::MAIL)
    rv |= setDefaultMail();

  if (aApps & nsIShellService::NEWS)
    rv |= setDefaultNews();

  if (aApps & nsIShellService::RSS)
    setKeysForSettings(gFeedSettings, sizeof(gFeedSettings)/sizeof(SETTING), 
                       NS_ConvertUTF16toUTF8(mBrandFullName).get());

  // Refresh the Shell
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
  return rv;
}

NS_IMETHODIMP
nsWindowsShellService::GetShouldCheckDefaultClient(PRBool* aResult)
{
  if (mCheckedThisSession) 
  {
    *aResult = PR_FALSE;
    return NS_OK;
  }

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  return prefs->GetBoolPref("mail.shell.checkDefaultClient", aResult);
}

NS_IMETHODIMP
nsWindowsShellService::SetShouldCheckDefaultClient(PRBool aShouldCheck)
{
  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  return prefs->SetBoolPref("mail.shell.checkDefaultClient", aShouldCheck);
}

nsresult
nsWindowsShellService::setDefaultMail()
{
  nsresult rv;
  NS_ConvertUTF16toUTF8 appName(mBrandFullName);
  setKeysForSettings(gMailSettings, sizeof(gMailSettings)/sizeof(SETTING), appName.get());

  // at least for now, this key needs to be written to HKLM instead of HKCU 
  // which is where the windows operating system looks (at least on Win XP and earlier)
  SetRegKey(NS_LITERAL_CSTRING(MOZ_CLIENT_MAIL_KEY).get(), "", appName.get(), PR_TRUE);

  nsCAutoString nativeFullName;
  // For now, we use 'A' APIs (see bug 240272, 239279)
  NS_CopyUnicodeToNative(mBrandFullName, nativeFullName);

  nsCAutoString key1(NS_LITERAL_CSTRING(MAILCLIENTS));
  key1.Append(appName);
  key1.Append("\\");
  SetRegKey(key1.get(), "", nativeFullName.get(), PR_TRUE);

  // Set the Options and Safe Mode start menu context menu item labels
  nsCOMPtr<nsIStringBundle> bundle;
  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService("@mozilla.org/intl/stringbundle;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = bundleService->CreateBundle("chrome://messenger/locale/shellservice.properties", getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCAutoString optionsKey(NS_LITERAL_CSTRING(MAILCLIENTS "%APPNAME%\\shell\\properties"));
  optionsKey.ReplaceSubstring("%APPNAME%", appName.get());

  const PRUnichar* brandNameStrings[] = { mBrandShortName.get() };

  // Set the Options menu item
  nsXPIDLString optionsTitle;
  bundle->FormatStringFromName(NS_LITERAL_STRING("optionsLabel").get(),
                               brandNameStrings, 1, getter_Copies(optionsTitle));
  // Set the registry keys
  nsCAutoString nativeTitle;
  // For the now, we use 'A' APIs (see bug 240272,  239279)
  NS_CopyUnicodeToNative(optionsTitle, nativeTitle);
  SetRegKey(optionsKey.get(), "", nativeTitle.get(), PR_TRUE);
#ifndef __MINGW32__
  // Tell the MAPI Service to register the mapi proxy dll now that we are the default mail application
  nsCOMPtr<nsIMapiSupport> mapiService (do_GetService(NS_IMAPISUPPORT_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  return mapiService->RegisterServer();
#else
  return NS_OK;
#endif
}

nsresult
nsWindowsShellService::setDefaultNews()
{
  NS_ConvertUTF16toUTF8 appName(mBrandFullName);
  setKeysForSettings(gNewsSettings, sizeof(gNewsSettings)/sizeof(SETTING), appName.get());

  // at least for now, this key needs to be written to HKLM instead of HKCU 
  // which is where the windows operating system looks (at least on Win XP and earlier)
  SetRegKey(NS_LITERAL_CSTRING(MOZ_CLIENT_NEWS_KEY).get(), "", appName.get(), PR_TRUE);

  nsCAutoString nativeFullName;
  // For now, we use 'A' APIs (see bug 240272, 239279)
  NS_CopyUnicodeToNative(mBrandFullName, nativeFullName);
  nsCAutoString key1(NS_LITERAL_CSTRING(NEWSCLIENTS));
  key1.Append(appName);
  key1.Append("\\");
  SetRegKey(key1.get(), "", nativeFullName.get(), PR_TRUE);
  return NS_OK;
}

// Utility function to delete a registry subkey.
DWORD
nsWindowsShellService::DeleteRegKey(HKEY baseKey, const char *keyName)
{
 // Make sure input subkey isn't null. 
 if (!keyName || !::strlen(keyName))
   return ERROR_BADKEY;

 DWORD rc;
 // Open subkey.
 HKEY key;
 rc = ::RegOpenKeyEx(baseKey, keyName, 0, KEY_ENUMERATE_SUB_KEYS | DELETE, &key);
 
 // Continue till we get an error or are done.
 while (rc == ERROR_SUCCESS) 
 {
   char subkeyName[_MAX_PATH];
   DWORD len = sizeof subkeyName;
   // Get first subkey name.  Note that we always get the
   // first one, then delete it.  So we need to get
   // the first one next time, also.
   rc = ::RegEnumKeyEx(key, 0, subkeyName, &len, 0, 0, 0, 0);
   if (rc == ERROR_NO_MORE_ITEMS) 
   {
     // No more subkeys.  Delete the main one.
     rc = ::RegDeleteKey(baseKey, keyName);
     break;
   } 
   if (rc == ERROR_SUCCESS) 
   {
     // Another subkey, delete it, recursively.
     rc = DeleteRegKey(key, subkeyName);
   }
 }
 
 // Close the key we opened.
 ::RegCloseKey(key);
 return rc;
}

void
nsWindowsShellService::SetRegKey(const char* aKeyName, const char* aValueName, 
                                 const char* aValue, PRBool aHKLMOnly)
{
  char buf[MAX_BUF];
  DWORD len = sizeof buf;

  HKEY theKey;
  nsresult rv = OpenKeyForWriting(HKEY_LOCAL_MACHINE, aKeyName, &theKey, aHKLMOnly);
  if (NS_FAILED(rv))
    return;

  // Get the old value
  DWORD result = ::RegQueryValueEx(theKey, aValueName, NULL, NULL, (LPBYTE)buf, &len);

  // Set the new value
  if (REG_FAILED(result) || strcmp(buf, aValue) != 0)
    ::RegSetValueEx(theKey, aValueName, 0, REG_SZ, 
                    (LPBYTE)aValue, nsDependentCString(aValue).Length());
  
  // Close the key we opened.
  ::RegCloseKey(theKey);
}

/* helper routine. Iterate over the passed in settings object,
   testing each key with the USE_FOR_DEFAULT_TEST to see if 
   we are handling it. 
*/
PRBool
nsWindowsShellService::TestForDefault(SETTING aSettings[], PRInt32 aSize)
{
  PRBool isDefault = PR_TRUE;
  NS_ConvertUTF16toUTF8 appName(mBrandFullName);
  char currValue[MAX_BUF];
  SETTING* end = aSettings + aSize;
  for (SETTING * settings = aSettings; settings < end; ++settings) 
  {
    if (settings->flags & USE_FOR_DEFAULT_TEST)
    {
      nsCAutoString dataLongPath(settings->valueData);
      nsCAutoString dataShortPath(settings->valueData);
      if (settings->flags & APP_PATH_SUBSTITUTION) {
        PRInt32 offset = dataLongPath.Find("%APPPATH%");
        dataLongPath.Replace(offset, 9, mAppLongPath);
        // Remove the quotes around %APPPATH% in VAL_OPEN for short paths
        PRInt32 offsetQuoted = dataShortPath.Find("\"%APPPATH%\"");
        if (offsetQuoted != -1)
          dataShortPath.Replace(offsetQuoted, 11, mAppShortPath);
        else
          dataShortPath.Replace(offset, 9, mAppShortPath);
      }
      
      nsCAutoString key(settings->keyName);
      if (settings->flags & APPNAME_SUBSTITUTION) 
      {
        PRInt32 offset = key.Find("%APPNAME%");
        key.Replace(offset, 9, appName);
      }

      ::ZeroMemory(currValue, sizeof(currValue));
      HKEY theKey;
      nsresult rv = OpenUserKeyForReading(HKEY_CURRENT_USER, key.get(), &theKey);
      if (NS_SUCCEEDED(rv)) 
      {
        DWORD len = sizeof currValue;
        DWORD result = ::RegQueryValueEx(theKey, settings->valueName, NULL, NULL, (LPBYTE)currValue, &len);
        // Close the key we opened.
        ::RegCloseKey(theKey);
        if (REG_FAILED(result) || !dataLongPath.EqualsIgnoreCase(currValue) && !dataShortPath.EqualsIgnoreCase(currValue))
        {
          // Key wasn't set, or was set to something else (something else became the default client)
          isDefault = PR_FALSE;
          break;
        }
      }
    }
  }  // for each registry key we want to look at

  return isDefault;
}


/* helper routine. Iterate over the passed in settings array, setting each key
 * in the windows registry.
*/

void 
nsWindowsShellService::setKeysForSettings(SETTING aSettings[], PRInt32 aSize, const char * aAppName)
{
  SETTING* settings;
  SETTING* end = aSettings + aSize;
  PRInt32 offset;

  for (settings = aSettings; settings < end; ++settings) 
  {
    nsCAutoString data(settings->valueData);
    nsCAutoString key(settings->keyName);
    if (settings->flags & APP_PATH_SUBSTITUTION) 
    {
      offset = data.Find("%APPPATH%");
      data.Replace(offset, 9, mAppLongPath);
    }
    if (settings->flags & MAPIDLL_PATH_SUBSTITUTION) 
    {
      offset = data.Find("%MAPIDLLPATH%");
      data.Replace(offset, 13, mMapiDLLPath);
    }
    if (settings->flags & APPNAME_SUBSTITUTION) 
    {
      offset = key.Find("%APPNAME%");
      key.Replace(offset, 9, aAppName);
    }
    if (settings->flags & UNINST_PATH_SUBSTITUTION) 
    {
      offset = data.Find("%UNINSTPATH%"); 
      data.Replace(offset, 12, mUninstallPath);
    }

    SetRegKey(key.get(), settings->valueName, data.get(), settings->flags & HKLM_ONLY);
  }
}

// Support for versions of shlobj.h that don't include the Vista API's 
#if !defined(IApplicationAssociationRegistration)

typedef enum tagASSOCIATIONLEVEL
{
  AL_MACHINE,
  AL_EFFECTIVE,
  AL_USER
} ASSOCIATIONLEVEL;

typedef enum tagASSOCIATIONTYPE
{
  AT_FILEEXTENSION,
  AT_URLPROTOCOL,
  AT_STARTMENUCLIENT,
  AT_MIMETYPE
} ASSOCIATIONTYPE;

MIDL_INTERFACE("4e530b0a-e611-4c77-a3ac-9031d022281b")
IApplicationAssociationRegistration : public IUnknown
{
 public:
  virtual HRESULT STDMETHODCALLTYPE QueryCurrentDefault(LPCWSTR pszQuery,
                                                        ASSOCIATIONTYPE atQueryType,
                                                        ASSOCIATIONLEVEL alQueryLevel,
                                                        LPWSTR *ppszAssociation) = 0;
  virtual HRESULT STDMETHODCALLTYPE QueryAppIsDefault(LPCWSTR pszQuery,
                                                      ASSOCIATIONTYPE atQueryType,
                                                      ASSOCIATIONLEVEL alQueryLevel,
                                                      LPCWSTR pszAppRegistryName,
                                                      BOOL *pfDefault) = 0;
  virtual HRESULT STDMETHODCALLTYPE QueryAppIsDefaultAll(ASSOCIATIONLEVEL alQueryLevel,
                                                         LPCWSTR pszAppRegistryName,
                                                         BOOL *pfDefault) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetAppAsDefault(LPCWSTR pszAppRegistryName,
                                                    LPCWSTR pszSet,
                                                    ASSOCIATIONTYPE atSetType) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetAppAsDefaultAll(LPCWSTR pszAppRegistryName) = 0;
  virtual HRESULT STDMETHODCALLTYPE ClearUserAssociations( void) = 0;
};
#endif

static const CLSID CLSID_ApplicationAssociationReg = {0x591209C7,0x767B,0x42B2,{0x9F,0xBA,0x44,0xEE,0x46,0x15,0xF2,0xC7}};
static const IID   IID_IApplicationAssociationReg  = {0x4e530b0a,0xe611,0x4c77,{0xa3,0xac,0x90,0x31,0xd0,0x22,0x28,0x1b}};

PRBool
nsWindowsShellService::IsDefaultClientVista(PRBool aStartupCheck, PRUint16 aApps, PRBool* aIsDefaultClient)
{
  IApplicationAssociationRegistration* pAAR;

  HRESULT hr = CoCreateInstance (CLSID_ApplicationAssociationReg,
                                 NULL,
                                 CLSCTX_INPROC,
                                 IID_IApplicationAssociationReg,
                                 (void**)&pAAR);
  
  if (SUCCEEDED(hr))
  {
    PRBool isDefaultMail = PR_TRUE;
    PRBool isDefaultNews = PR_TRUE;
    if (aApps & nsIShellService::MAIL)
      pAAR->QueryAppIsDefaultAll(AL_EFFECTIVE, APP_REG_NAME_MAIL, &isDefaultMail);
    if (aApps & nsIShellService::NEWS)
      pAAR->QueryAppIsDefaultAll(AL_EFFECTIVE, APP_REG_NAME_NEWS, &isDefaultNews);

    *aIsDefaultClient = isDefaultNews && isDefaultMail;    
    
    // If this is the first mail window, maintain internal state that we've
    // checked this session (so that subsequent window opens don't show the 
    // default browser dialog).
    if (aStartupCheck)
      mCheckedThisSession = PR_TRUE;
    
    pAAR->Release();
    return PR_TRUE;
  }
  
  return PR_FALSE;
}

PRBool
nsWindowsShellService::SetDefaultClientVista(PRUint16 aApps)
{
  IApplicationAssociationRegistration* pAAR;

  HRESULT hr = CoCreateInstance (CLSID_ApplicationAssociationReg,
                                 NULL,
                                 CLSCTX_INPROC,
                                 IID_IApplicationAssociationReg,
                                 (void**)&pAAR);
  
  if (SUCCEEDED(hr))
  {
    if (aApps & nsIShellService::MAIL)
      hr = pAAR->SetAppAsDefaultAll(APP_REG_NAME_MAIL);
    if (aApps & nsIShellService::NEWS)
      hr = pAAR->SetAppAsDefaultAll(APP_REG_NAME_NEWS);   
    
    pAAR->Release();
    return PR_TRUE;
  }
  
  return PR_FALSE;
}
