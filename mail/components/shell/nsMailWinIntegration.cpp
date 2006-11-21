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

#include <mbstring.h>

#define MOZ_CLIENT_MAIL_KEY "Software\\Clients\\Mail"
#define MOZ_CLIENT_NEWS_KEY "Software\\Clients\\News"

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

  switch (result) {
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

static nsresult
OpenKeyForWriting(const char* aKeyName, HKEY* aKey, PRBool aForAllUsers, PRBool aCreate)
{
  nsresult rv = NS_OK;

  HKEY rootKey = aForAllUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  DWORD result = ::RegOpenKeyEx(rootKey, aKeyName, 0, KEY_READ | KEY_WRITE, aKey);

  switch (result) {
    case ERROR_SUCCESS:
      break;
    case ERROR_ACCESS_DENIED:
      return NS_ERROR_FILE_ACCESS_DENIED;
    case ERROR_FILE_NOT_FOUND:
      if (aCreate)
        result = ::RegCreateKey(HKEY_LOCAL_MACHINE, aKeyName, aKey);
      rv = NS_ERROR_FILE_NOT_FOUND;
      break;
  }
  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// Default Mail Registry Settings
///////////////////////////////////////////////////////////////////////////////

typedef enum { NO_SUBSTITUTION    = 0x00,
               PATH_SUBSTITUTION  = 0x01,
               APPNAME_SUBSTITUTION = 0x02,
               MAPIDLLPATH_SUBSTITUTION = 0x04,
               USE_FOR_DEFAULT_TEST = 0x08} SettingFlags;

#define CLS "SOFTWARE\\Classes\\"
#define MAILCLIENTS "SOFTWARE\\Clients\\Mail\\"
#define NEWSCLIENTS "SOFTWARE\\Clients\\News\\"
#define DI "\\DefaultIcon"
#define CLS_EML "ThunderbirdEML"
#define SOP "\\shell\\open\\command"
#define EXE "thunderbird.exe"

#define VAL_URL_ICON "%APPPATH%,0"
#define VAL_FILE_ICON "%APPPATH%,1"
#define VAL_OPEN "%APPPATH% \"%1\""
#define VAL_OPEN_WITH_URL "%APPPATH% -url \"%1\""

#define MAKE_KEY_NAME1(PREFIX, MID) \
  PREFIX MID

#define MAKE_KEY_NAME2(PREFIX, MID, SUFFIX) \
  PREFIX MID SUFFIX

static SETTING gMailSettings[] = {
  // File Extensions
  { MAKE_KEY_NAME1(CLS, ".eml"),    "", "", NO_SUBSTITUTION },
  { MAKE_KEY_NAME2(CLS, CLS_EML, DI),  "",  VAL_FILE_ICON, PATH_SUBSTITUTION },
  { MAKE_KEY_NAME2(CLS, CLS_EML, SOP), "",  VAL_OPEN, PATH_SUBSTITUTION},

  // Protocol Handlers
  { MAKE_KEY_NAME2(CLS, "mailto", DI),  "", VAL_URL_ICON, PATH_SUBSTITUTION},
  // we use the following key for our default mail app test so don't set the NON_ESSENTIAL flag
  { MAKE_KEY_NAME2(CLS, "mailto", SOP), "", "%APPPATH% -compose \"%1\"", PATH_SUBSTITUTION | USE_FOR_DEFAULT_TEST}, 

  // Windows XP Start Menu
  { MAKE_KEY_NAME1(MAILCLIENTS, "%APPNAME%"),  
    "DLLPath", 
    "%MAPIDLLPATH%", 
    MAPIDLLPATH_SUBSTITUTION | APPNAME_SUBSTITUTION },
  { MAKE_KEY_NAME2(MAILCLIENTS, "%APPNAME%", DI),  
    "", 
    "%APPPATH%,0", 
    PATH_SUBSTITUTION | APPNAME_SUBSTITUTION },
  { MAKE_KEY_NAME2(MAILCLIENTS, "%APPNAME%", SOP), 
    "", 
    "%APPPATH% -mail",   
    PATH_SUBSTITUTION | APPNAME_SUBSTITUTION },
  { MAKE_KEY_NAME1(MAILCLIENTS, "%APPNAME%\\shell\\properties\\command"),
    "", 
    "%APPPATH% -options",   
    PATH_SUBSTITUTION | APPNAME_SUBSTITUTION },
  { MAKE_KEY_NAME2(MAILCLIENTS, "%APPNAME%\\Protocols\\mailto", DI),  "", VAL_URL_ICON, PATH_SUBSTITUTION | APPNAME_SUBSTITUTION},
  { MAKE_KEY_NAME2(MAILCLIENTS, "%APPNAME%\\Protocols\\mailto", SOP), "", "%APPPATH% -compose \"%1\"", PATH_SUBSTITUTION | APPNAME_SUBSTITUTION },
  { MAKE_KEY_NAME2(MAILCLIENTS, "%APPNAME%\\.eml", DI),  "", VAL_URL_ICON, PATH_SUBSTITUTION | APPNAME_SUBSTITUTION},
  { MAKE_KEY_NAME2(MAILCLIENTS, "%APPNAME%\\.eml", SOP), "", VAL_OPEN, PATH_SUBSTITUTION | APPNAME_SUBSTITUTION }

  // These values must be set by hand, since they contain localized strings.
  //     shell\properties        (default)   REG_SZ  Firefox &Options
};

static SETTING gNewsSettings[] = {
  // Protocol Handlers
  { MAKE_KEY_NAME2(CLS, "news", DI),  "", VAL_URL_ICON, PATH_SUBSTITUTION},
  { MAKE_KEY_NAME2(CLS, "news", SOP), "", "%APPPATH% -mail \"%1\"", PATH_SUBSTITUTION | USE_FOR_DEFAULT_TEST},
  { MAKE_KEY_NAME2(CLS, "nntp", DI),  "", VAL_URL_ICON, PATH_SUBSTITUTION},
  { MAKE_KEY_NAME2(CLS, "nntp", SOP), "", "%APPPATH% -mail \"%1\"", PATH_SUBSTITUTION | USE_FOR_DEFAULT_TEST}, 
  { MAKE_KEY_NAME2(CLS, "snews", DI),  "", VAL_URL_ICON, PATH_SUBSTITUTION},
  { MAKE_KEY_NAME2(CLS, "snews", SOP), "", "%APPPATH% -mail \"%1\"", PATH_SUBSTITUTION}, 

  // Client Keys
  { MAKE_KEY_NAME1(NEWSCLIENTS, "%APPNAME%"),  
    "DLLPath", 
    "%MAPIDLLPATH%", 
    MAPIDLLPATH_SUBSTITUTION | APPNAME_SUBSTITUTION },
  { MAKE_KEY_NAME2(NEWSCLIENTS, "%APPNAME%", DI),  
    "", 
    "%APPPATH%,0", 
    PATH_SUBSTITUTION | APPNAME_SUBSTITUTION },
  { MAKE_KEY_NAME2(NEWSCLIENTS, "%APPNAME%", SOP), 
    "", 
    "%APPPATH% -mail",   
    PATH_SUBSTITUTION | APPNAME_SUBSTITUTION },
  { MAKE_KEY_NAME2(NEWSCLIENTS, "%APPNAME%\\Protocols\\news", DI),  "", VAL_URL_ICON, PATH_SUBSTITUTION | APPNAME_SUBSTITUTION},
  { MAKE_KEY_NAME2(NEWSCLIENTS, "%APPNAME%\\Protocols\\news", SOP), "", "%APPPATH% -mail \"%1\"", PATH_SUBSTITUTION | APPNAME_SUBSTITUTION },
  { MAKE_KEY_NAME2(NEWSCLIENTS, "%APPNAME%\\Protocols\\nntp", DI),  "", VAL_URL_ICON, PATH_SUBSTITUTION | APPNAME_SUBSTITUTION},
  { MAKE_KEY_NAME2(NEWSCLIENTS, "%APPNAME%\\Protocols\\nntp", SOP), "", "%APPPATH% -mail \"%1\"", PATH_SUBSTITUTION | APPNAME_SUBSTITUTION },
  { MAKE_KEY_NAME2(NEWSCLIENTS, "%APPNAME%\\Protocols\\snews", DI),  "", VAL_URL_ICON, PATH_SUBSTITUTION | APPNAME_SUBSTITUTION},
  { MAKE_KEY_NAME2(NEWSCLIENTS, "%APPNAME%\\Protocols\\snews", SOP), "", "%APPPATH% -mail \"%1\"", PATH_SUBSTITUTION | APPNAME_SUBSTITUTION }
};

static SETTING gFeedSettings[] = {
  // Protocol Handlers
  { MAKE_KEY_NAME2(CLS, "feed", DI),  "", VAL_URL_ICON, PATH_SUBSTITUTION},
  { MAKE_KEY_NAME2(CLS, "feed", SOP), "", "%APPPATH% -mail \"%1\"", PATH_SUBSTITUTION | USE_FOR_DEFAULT_TEST},
};

nsWindowsShellService::nsWindowsShellService()
:mCheckedThisSession(PR_FALSE)
{
  nsresult rv;
  // fill in mAppPath
  char buf[MAX_BUF];
  ::GetModuleFileName(NULL, buf, sizeof(buf));
  ::GetShortPathName(buf, buf, sizeof(buf));
  ToUpperCase(mAppPath = buf);

  mMapiDLLPath = mAppPath;
  // remove the process name from the string (thunderbird.exe)
  char* pathSep = (char *) _mbsrchr((const unsigned char *) mAppPath.get(), '\\');
  if (pathSep)
    mMapiDLLPath.Truncate(pathSep - mAppPath.get() + 1);
  // now append mozMapi32.dll
   mMapiDLLPath += "mozMapi32.dll";

  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService("@mozilla.org/intl/stringbundle;1", &rv));
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIStringBundle> bundle, brandBundle;
    rv = bundleService->CreateBundle("chrome://branding/locale/brand.properties", getter_AddRefs(brandBundle));
    if (NS_SUCCEEDED(rv))
    {
      brandBundle->GetStringFromName(NS_LITERAL_STRING("brandFullName").get(),
                                     getter_Copies(mBrandFullName));
      brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                     getter_Copies(mBrandShortName));
    }
  }
}

NS_IMETHODIMP
nsWindowsShellService::IsDefaultClient(PRBool aStartupCheck, PRUint16 aApps, PRBool *aIsDefaultClient)
{
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

NS_IMETHODIMP
nsWindowsShellService::SetDefaultClient(PRBool aForAllUsers, PRUint16 aApps)
{
  nsresult rv = NS_OK;
  if (aApps & nsIShellService::MAIL)
    rv |= setDefaultMail(aForAllUsers);
  if (aApps & nsIShellService::NEWS)
    rv |= setDefaultNews(aForAllUsers);
  if (aApps & nsIShellService::RSS)
    setKeysForSettings(gFeedSettings, sizeof(gFeedSettings)/sizeof(SETTING), 
                       NS_ConvertUTF16toUTF8(mBrandFullName).get(), aForAllUsers);

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
nsWindowsShellService::setDefaultMail(PRBool aForAllUsers)
{
  nsresult rv;
  NS_ConvertUTF16toUTF8 appName(mBrandFullName);
  setKeysForSettings(gMailSettings, sizeof(gMailSettings)/sizeof(SETTING), appName.get(), aForAllUsers);

  // at least for now, this key needs to be written to HKLM instead of HKCU 
  // which is where the windows operating system looks (at least on Win XP and earlier)
  // for Tools / Internet Options / Programs) so pass in 
  // true for all users.  
  SetRegKey(NS_LITERAL_CSTRING(MOZ_CLIENT_MAIL_KEY).get(), "", appName.get(), PR_TRUE, /* aForAllUsers */ PR_TRUE);

  nsCAutoString nativeFullName;
  // For now, we use 'A' APIs (see bug 240272, 239279)
  NS_CopyUnicodeToNative(mBrandFullName, nativeFullName);

  nsCAutoString key1(NS_LITERAL_CSTRING(MAILCLIENTS));
  key1.Append(appName);
  key1.Append("\\");
  SetRegKey(key1.get(), "", nativeFullName.get(), PR_TRUE, aForAllUsers);

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
  SetRegKey(optionsKey.get(), "", nativeTitle.get(), PR_TRUE, aForAllUsers);
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
nsWindowsShellService::setDefaultNews(PRBool aForAllUsers)
{
  NS_ConvertUTF16toUTF8 appName(mBrandFullName);
  setKeysForSettings(gNewsSettings, sizeof(gNewsSettings)/sizeof(SETTING), appName.get(), aForAllUsers);

  // at least for now, this key needs to be written to HKLM instead of HKCU 
  // which is where the windows operating system looks (at least on Win XP and earlier)
  // for Tools / Internet Options / Programs) so pass in 
  // true for all users.  
  SetRegKey(NS_LITERAL_CSTRING(MOZ_CLIENT_NEWS_KEY).get(), "", appName.get(), PR_TRUE, /* aForAllUsers */ PR_TRUE);

  nsCAutoString nativeFullName;
  // For now, we use 'A' APIs (see bug 240272, 239279)
  NS_CopyUnicodeToNative(mBrandFullName, nativeFullName);
  nsCAutoString key1(NS_LITERAL_CSTRING(NEWSCLIENTS));
  key1.Append(appName);
  key1.Append("\\");
  SetRegKey(key1.get(), "", nativeFullName.get(), PR_TRUE, aForAllUsers);
  return NS_OK;
}

void
nsWindowsShellService::SetRegKey(const char* aKeyName, const char* aValueName, 
                                 const char* aValue, PRBool aReplaceExisting,
                                 PRBool aForAllUsers)
{
  char buf[MAX_BUF];
  DWORD len = sizeof buf;

  HKEY theKey;
  nsresult rv = OpenKeyForWriting(aKeyName, &theKey, aForAllUsers, PR_TRUE);
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND) return;

  // If we're not allowed to replace an existing key, and one exists (i.e. the
  // result isn't ERROR_FILE_NOT_FOUND, then just return now. 
  if (!aReplaceExisting && rv != NS_ERROR_FILE_NOT_FOUND)
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
      nsCAutoString data(settings->valueData);
      nsCAutoString key(settings->keyName);
      if (settings->flags & PATH_SUBSTITUTION) {
        PRInt32 offset = data.Find("%APPPATH%");
        data.Replace(offset, 9, mAppPath);
      }
      if (settings->flags & APPNAME_SUBSTITUTION) {
        PRInt32 offset = key.Find("%APPNAME%");
        key.Replace(offset, 9, appName);
      }

      ::ZeroMemory(currValue, sizeof(currValue));
      HKEY theKey;
      nsresult rv = OpenUserKeyForReading(HKEY_CURRENT_USER, key.get(), &theKey);
      if (NS_SUCCEEDED(rv)) {
        DWORD len = sizeof currValue;
        DWORD result = ::RegQueryValueEx(theKey, settings->valueName, NULL, NULL, (LPBYTE)currValue, &len);
        // Close the key we opened.
        ::RegCloseKey(theKey);
        if (REG_FAILED(result) || strcmp(data.get(), currValue) != 0) {
          // Key wasn't set, or was set to something else (something else became the default browser)
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
nsWindowsShellService::setKeysForSettings(SETTING aSettings[], PRInt32 aSize, const char * aAppName, PRBool aForAllUsers)
{
  SETTING* settings;
  SETTING* end = aSettings + aSize;

  for (settings = aSettings; settings < end; ++settings) {
    nsCAutoString data(settings->valueData);
    nsCAutoString key(settings->keyName);
    if (settings->flags & PATH_SUBSTITUTION) {
      PRInt32 offset = data.Find("%APPPATH%");
      data.Replace(offset, 9, mAppPath);
    }
    if (settings->flags & MAPIDLLPATH_SUBSTITUTION) {
      PRInt32 offset = data.Find("%MAPIDLLPATH%");
      data.Replace(offset, 13, mMapiDLLPath);
    }
    if (settings->flags & APPNAME_SUBSTITUTION) {
      PRInt32 offset = key.Find("%APPNAME%");
      key.Replace(offset, 9, aAppName);
    }

    SetRegKey(key.get(), settings->valueName, data.get(),
              PR_TRUE, aForAllUsers);
  }
}
