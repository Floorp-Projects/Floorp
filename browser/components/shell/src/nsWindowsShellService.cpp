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
 * The Original Code is Shell Service.
 *
 * The Initial Developer of the Original Code is mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ben Goodger    <ben@mozilla.org>     (Clients, Mail, New Default Browser)
 *  Joe Hewitt     <hewitt@netscape.com> (Set Background)
 *  Blake Ross     <blake@blakeross.com> (Desktop Color)
 *  Jungshik Shin  <jshin@mailaps.org>   (I18N)
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

#include "gfxIImageFrame.h"
#include "imgIContainer.h"
#include "imgIRequest.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIFrame.h"
#include "nsIImageLoadingContent.h"
#include "nsIOutputStream.h"
#include "nsIPrefService.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsNetUtil.h"
#include "nsShellService.h"
#include "nsWindowsShellService.h"
#include "nsNativeCharsetUtils.h"

#include <mbstring.h>


#define MOZ_HWND_BROADCAST_MSG_TIMEOUT 5000
#define MOZ_BACKUP_REGISTRY "SOFTWARE\\Mozilla\\Desktop"

#ifndef MAX_BUF
#define MAX_BUF 4096
#endif

#define REG_SUCCEEDED(val) \
  (val == ERROR_SUCCESS)

#define REG_FAILED(val) \
  (val != ERROR_SUCCESS)

NS_IMPL_ISUPPORTS2(nsWindowsShellService, nsIWindowsShellService, nsIShellService)

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
    if (aStartKey == HKEY_LOCAL_MACHINE) {
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
// Default Browser Registry Settings
//
// - File Extension Mappings
//   -----------------------
//   The following file extensions:
//    .htm .html .shtml .xht .xhtml 
//   are mapped like so:
//
//   HKCU\SOFTWARE\Classes\.<ext>\      (default)   REG_SZ  FirefoxHTML
//
//   as aliases to the class:
//
//   HKCU\SOFTWARE\Classes\FirefoxHTML\
//     DefaultIcon                      (default)   REG_SZ  <appname>,1
//     shell\open\command               (default)   REG_SZ  <appname> -url "%1"
//
// - Protocol Mappings
//   -----------------
//   The following protocols:
//    HTTP, HTTPS, FTP, GOPHER, CHROME
//   are mapped like so:
//
//   HKCU\SOFTWARE\Classes\<protocol>\
//     DefaultIcon                      (default)   REG_SZ  <appname>,1
//     shell\open\command               (default)   REG_SZ  <appname> -url "%1"
//     shell\open\ddeexec               (default)   REG_SZ  "%1",,-1,0,,,,
//                       \application   (default)   REG_SZ  Firefox
//                       \topic         (default)   REG_SZ  WWW_OpenURL
//                    
//
// - Windows XP Start Menu Browser
//   -----------------------------
//   The following keys are set to make Firefox appear in the Windows XP
//   Start Menu as the browser:
//   
//   HKCU\SOFTWARE\Clients\StartMenuInternet
//     firefox.exe\DefaultIcon             (default)   REG_SZ  <appname>,0
//     firefox.exe\shell\open\command      (default)   REG_SZ  <appname>
//     firefox.exe\shell\properties        (default)   REG_SZ  Firefox &Options
//     firefox.exe\shell\properties\command(default)   REG_SZ  <appname> -chrome "chrome://browser/content/pref.xul"
//
// - Uninstall Information
//   ---------------------
//   Every key that is set has the previous value stored in:
//    
//   HKCU\SOFTWARE\Mozilla\Desktop\        <keyname>   REG_SZ oldval
//
//   If there is no previous value, an empty value is set to indicate that the
//   key should be removed completely. 
//

typedef enum { NO_SUBSTITUTION    = 0x00,
               PATH_SUBSTITUTION  = 0x01,
               EXE_SUBSTITUTION   = 0x02,
               NON_ESSENTIAL      = 0x04} SettingFlags;
typedef struct {
  char* keyName;
  char* valueName;
  char* valueData;

  PRInt32 flags;
} SETTING;

#define SMI "SOFTWARE\\Clients\\StartMenuInternet\\"
#define CLS "SOFTWARE\\Classes\\"
#define DI "\\DefaultIcon"
#define SOP "\\shell\\open\\command"
#define EXE "firefox.exe"

#define CLS_HTML "FirefoxHTML"
#define VAL_ICON "%APPPATH%,1"
#define VAL_OPEN "%APPPATH% -url \"%1\""

#define MAKE_KEY_NAME1(PREFIX, MID) \
  PREFIX MID

#define MAKE_KEY_NAME2(PREFIX, MID, SUFFIX) \
  PREFIX MID SUFFIX

static SETTING gSettings[] = {
  // Extension Manager Keys
  { MAKE_KEY_NAME1(CLS, "MIME\\Database\\Content Type\\application/x-xpinstall;app=firefox"),
    "Extension",
    ".xpi",
    NO_SUBSTITUTION | NON_ESSENTIAL },

  // File Extension Aliases
  { MAKE_KEY_NAME1(CLS, ".htm"),    "", CLS_HTML, NO_SUBSTITUTION | NON_ESSENTIAL },
  { MAKE_KEY_NAME1(CLS, ".html"),   "", CLS_HTML, NO_SUBSTITUTION | NON_ESSENTIAL },
  { MAKE_KEY_NAME1(CLS, ".shtml"),  "", CLS_HTML, NO_SUBSTITUTION | NON_ESSENTIAL },
  { MAKE_KEY_NAME1(CLS, ".xht"),    "", CLS_HTML, NO_SUBSTITUTION | NON_ESSENTIAL },
  { MAKE_KEY_NAME1(CLS, ".xhtml"),  "", CLS_HTML, NO_SUBSTITUTION | NON_ESSENTIAL },

  // File Extension Class
  { MAKE_KEY_NAME2(CLS, CLS_HTML, DI),  "", VAL_ICON, PATH_SUBSTITUTION | NON_ESSENTIAL },
  { MAKE_KEY_NAME2(CLS, CLS_HTML, SOP), "", VAL_OPEN, PATH_SUBSTITUTION | NON_ESSENTIAL },

  // Protocol Handlers
  { MAKE_KEY_NAME2(CLS, "HTTP", DI),    "", VAL_ICON, PATH_SUBSTITUTION },
  { MAKE_KEY_NAME2(CLS, "HTTP", SOP),   "", VAL_OPEN, PATH_SUBSTITUTION },
  { MAKE_KEY_NAME2(CLS, "HTTPS", DI),   "", VAL_ICON, PATH_SUBSTITUTION },
  { MAKE_KEY_NAME2(CLS, "HTTPS", SOP),  "", VAL_OPEN, PATH_SUBSTITUTION },
  { MAKE_KEY_NAME2(CLS, "FTP", DI),     "", VAL_ICON, PATH_SUBSTITUTION | NON_ESSENTIAL },
  { MAKE_KEY_NAME2(CLS, "FTP", SOP),    "", VAL_OPEN, PATH_SUBSTITUTION | NON_ESSENTIAL },
  { MAKE_KEY_NAME2(CLS, "GOPHER", DI),  "", VAL_ICON, PATH_SUBSTITUTION | NON_ESSENTIAL },
  { MAKE_KEY_NAME2(CLS, "GOPHER", SOP), "", VAL_OPEN, PATH_SUBSTITUTION | NON_ESSENTIAL },
  { MAKE_KEY_NAME2(CLS, "CHROME", DI),  "", VAL_ICON, PATH_SUBSTITUTION | NON_ESSENTIAL },
  { MAKE_KEY_NAME2(CLS, "CHROME", SOP), "", VAL_OPEN, PATH_SUBSTITUTION | NON_ESSENTIAL },

  // Windows XP Start Menu
  { MAKE_KEY_NAME2(SMI, "%APPEXE%", DI),  
    "", 
    "%APPPATH%,0", 
    PATH_SUBSTITUTION | EXE_SUBSTITUTION | NON_ESSENTIAL },
  { MAKE_KEY_NAME2(SMI, "%APPEXE%", SOP), 
    "", 
    "%APPPATH%",   
    PATH_SUBSTITUTION | EXE_SUBSTITUTION | NON_ESSENTIAL },
  { MAKE_KEY_NAME1(SMI, "%APPEXE%\\shell\\properties\\command"),
    "", 
    "%APPPATH% -chrome \"chrome://browser/content/pref/pref.xul\"",   
    PATH_SUBSTITUTION | EXE_SUBSTITUTION | NON_ESSENTIAL }

  // The value of the menu must be set by hand, since it contains a localized
  // string.
  //     firefox.exe\shell\properties        (default)   REG_SZ  Firefox &Options
};

NS_IMETHODIMP
nsWindowsShellService::IsDefaultBrowser(PRBool aStartupCheck, PRBool* aIsDefaultBrowser)
{
  SETTING* settings;
  SETTING* end = gSettings + sizeof(gSettings)/sizeof(SETTING);

  *aIsDefaultBrowser = PR_TRUE;

  nsCAutoString appPath;
  char buf[MAX_BUF];
  ::GetModuleFileName(NULL, buf, sizeof(buf));
  ::GetShortPathName(buf, buf, sizeof(buf));
  ToUpperCase(appPath = buf);

  // 0x5C can be the second byte of a multibyte character.
  char *pathSep = (char *) _mbsrchr((const unsigned char *) buf, '\\');
  nsCAutoString exeName;
  if (pathSep) {
    PRInt32 n = pathSep - buf; 
    exeName = Substring(appPath, n + 1, appPath.Length() - (n - 1));
  }
  else
    exeName = appPath;

  char currValue[MAX_BUF];
  for (settings = gSettings; settings < end; ++settings) {
    if (settings->flags & NON_ESSENTIAL)
      continue; // This is not a registry key that determines whether
                // or not we consider Firefox the "Default Browser."

    nsCAutoString data(settings->valueData);
    nsCAutoString key(settings->keyName);
    if (settings->flags & PATH_SUBSTITUTION) {
      PRInt32 offset = data.Find("%APPPATH%");
      data.Replace(offset, 9, appPath);
    }
    if (settings->flags & EXE_SUBSTITUTION) {
      PRInt32 offset = key.Find("%APPEXE%");
      key.Replace(offset, 8, exeName);
    }

    ::ZeroMemory(currValue, sizeof(currValue));
    HKEY theKey;
    nsresult rv = OpenUserKeyForReading(HKEY_CURRENT_USER, key.get(), &theKey);
    if (NS_SUCCEEDED(rv)) {
      DWORD len = sizeof currValue;
      DWORD result = ::RegQueryValueEx(theKey, settings->valueName, NULL, NULL, (LPBYTE)currValue, &len);
      if (REG_FAILED(result) || strcmp(data.get(), currValue) != 0) {
        // Key wasn't set, or was set to something else (something else became the default browser)
        *aIsDefaultBrowser = PR_FALSE;
        break;
      }
    }
  }

  // If this is the first browser window, maintain internal state that we've
  // checked this session (so that subsequent window opens don't show the 
  // default browser dialog).
  if (aStartupCheck)
    mCheckedThisSession = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::SetDefaultBrowser(PRBool aClaimAllTypes, PRBool aForAllUsers)
{
  // Locate the Backup key
  HKEY backupKey;
  nsresult rv = OpenKeyForWriting(MOZ_BACKUP_REGISTRY, &backupKey, aForAllUsers, PR_TRUE);
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND) return rv;

  SETTING* settings;
  SETTING* end = gSettings + sizeof(gSettings)/sizeof(SETTING);

  nsCAutoString appPath;
  char buf[MAX_BUF];
  ::GetModuleFileName(NULL, buf, sizeof(buf));
  ::GetShortPathName(buf, buf, sizeof(buf));
  ToUpperCase(appPath = buf);

  // 0x5C can be the second byte of a multibyte character.
  char *pathSep = (char *) _mbsrchr((const unsigned char *) buf, '\\');
  nsCAutoString exeName;
  if (pathSep) {
    PRInt32 n = pathSep - buf; 
    exeName = Substring(appPath, n + 1, appPath.Length() - (n - 1));
  }
  else
    exeName = appPath;

  for (settings = gSettings; settings < end; ++settings) {
    nsCAutoString data(settings->valueData);
    nsCAutoString key(settings->keyName);
    if (settings->flags & PATH_SUBSTITUTION) {
      PRInt32 offset = data.Find("%APPPATH%");
      data.Replace(offset, 9, appPath);
    }
    if (settings->flags & EXE_SUBSTITUTION) {
      PRInt32 offset = key.Find("%APPEXE%");
      key.Replace(offset, 8, exeName);
    }

    PRBool replaceExisting = aClaimAllTypes ? PR_TRUE : !(settings->flags & NON_ESSENTIAL);
    SetRegKey(key.get(), settings->valueName, data.get(),
              PR_TRUE, backupKey, replaceExisting, aForAllUsers);
  }

  // Select the Default Browser for the Windows XP Start Menu
  SetRegKey(NS_LITERAL_CSTRING(SMI).get(), "", exeName.get(), PR_TRUE, 
            backupKey, aClaimAllTypes, aForAllUsers);

  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService("@mozilla.org/intl/stringbundle;1"));
  nsCOMPtr<nsIStringBundle> bundle, brandBundle;
  bundleService->CreateBundle(SHELLSERVICE_PROPERTIES, getter_AddRefs(bundle));
  bundleService->CreateBundle(BRAND_PROPERTIES, getter_AddRefs(brandBundle));

  // Set the Start Menu item subtitle
  nsXPIDLString brandFullName;
  brandBundle->GetStringFromName(NS_LITERAL_STRING("brandFullName").get(),
                                 getter_Copies(brandFullName));
  nsCAutoString key1(NS_LITERAL_CSTRING(SMI));
  key1.Append(exeName);
  key1.Append("\\");
  nsCAutoString nativeFullName;
  // For now, we use 'A' APIs (see bug 240272,  239279)
  NS_CopyUnicodeToNative(brandFullName, nativeFullName);
  SetRegKey(key1.get(), "", nativeFullName.get(), PR_TRUE, backupKey, aClaimAllTypes, aForAllUsers);
  
  // Set the Options menu item title
  nsXPIDLString brandShortName;
  brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                 getter_Copies(brandShortName));
  const PRUnichar* brandNameStrings[] = { brandShortName.get() };
  nsXPIDLString optionsTitle;
  bundle->FormatStringFromName(NS_LITERAL_STRING("optionsLabel").get(),
                               brandNameStrings, 1, getter_Copies(optionsTitle));

  
  nsCAutoString key2(NS_LITERAL_CSTRING(SMI "%APPEXE%\\shell\\properties"));
  PRInt32 offset = key2.Find("%APPEXE%");
  key2.Replace(offset, 8, exeName);
  nsCAutoString nativeTitle;
  // For now, we use 'A' APIs (see bug 240272,  239279)
  NS_CopyUnicodeToNative(optionsTitle, nativeTitle);
  
  SetRegKey(key2.get(), "", nativeTitle.get(), PR_TRUE, backupKey, aClaimAllTypes, aForAllUsers);

  // Refresh the Shell
  ::SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, NULL,
                       (LPARAM)"SOFTWARE\\Clients\\StartMenuInternet",
                       SMTO_NORMAL|SMTO_ABORTIFHUNG,
                       MOZ_HWND_BROADCAST_MSG_TIMEOUT, NULL);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::RestoreFileSettings(PRBool aForAllUsers)
{
  // Locate the Backup key
  HKEY backupKey;
  nsresult rv = OpenKeyForWriting(MOZ_BACKUP_REGISTRY, &backupKey, aForAllUsers, PR_FALSE);
  if (NS_FAILED(rv)) return rv;

  DWORD i = 0;
  do {
    char origKeyName[MAX_BUF];
    DWORD len = sizeof origKeyName;
    DWORD result = ::RegEnumValue(backupKey, i++, origKeyName, &len, 0, 0, 0, 0);
    if (REG_SUCCEEDED(result)) {
      char origValue[MAX_BUF];
      DWORD len = sizeof origValue;
      result = ::RegQueryValueEx(backupKey, origKeyName, NULL, NULL, (LPBYTE)origValue, &len);
      if (REG_SUCCEEDED(result)) {
        HKEY origKey;
        result = ::RegOpenKeyEx(NULL, origKeyName, 0, KEY_READ, &origKey);
        if (REG_SUCCEEDED(result))
          result = ::RegSetValueEx(origKey, "", 0, REG_SZ, (LPBYTE)origValue, len);
      }
    }
    else
      break;
  }
  while (1);

  // Refresh the Shell
  ::SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, NULL,
                       (LPARAM)"SOFTWARE\\Clients\\StartMenuInternet",
                       SMTO_NORMAL|SMTO_ABORTIFHUNG,
                       MOZ_HWND_BROADCAST_MSG_TIMEOUT, NULL);
  return NS_OK;
}

void
nsWindowsShellService::SetRegKey(const char* aKeyName, const char* aValueName, 
                                 const char* aValue, PRBool aBackup,
                                 HKEY aBackupKey, PRBool aReplaceExisting,
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

  // Back up the old value
  if (aBackup && REG_SUCCEEDED(result))
    ::RegSetValueEx(aBackupKey, aKeyName, 0, REG_SZ, (LPBYTE)buf, len);

  // Set the new value
  if (REG_FAILED(result) || strcmp(buf, aValue) != 0)
    ::RegSetValueEx(theKey, aValueName, 0, REG_SZ, 
                    (LPBYTE)aValue, nsDependentCString(aValue).Length());
}

NS_IMETHODIMP
nsWindowsShellService::GetShouldCheckDefaultBrowser(PRBool* aResult)
{
  // If we've already checked, the browser has been started and this is a 
  // new window open, and we don't want to check again.
  if (mCheckedThisSession) {
    *aResult = PR_FALSE;
    return NS_OK;
  }

  nsCOMPtr<nsIPrefBranch> prefs;
  nsCOMPtr<nsIPrefService> pserve(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (pserve)
    pserve->GetBranch("", getter_AddRefs(prefs));

  prefs->GetBoolPref(PREF_CHECKDEFAULTBROWSER, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::SetShouldCheckDefaultBrowser(PRBool aShouldCheck)
{
  nsCOMPtr<nsIPrefBranch> prefs;
  nsCOMPtr<nsIPrefService> pserve(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (pserve)
    pserve->GetBranch("", getter_AddRefs(prefs));

  prefs->SetBoolPref(PREF_CHECKDEFAULTBROWSER, aShouldCheck);

  return NS_OK;
}

nsresult
WriteBitmap(nsString& aPath, gfxIImageFrame* aImage)
{
  PRInt32 width, height;
  aImage->GetWidth(&width);
  aImage->GetHeight(&height);

  PRUint8* bits;
  PRUint32 length;
  aImage->GetImageData(&bits, &length);
  if (!bits) return NS_ERROR_FAILURE;

  PRUint32 bpr;
  aImage->GetImageBytesPerRow(&bpr);
  PRInt32 bitCount = bpr/width;

  // initialize these bitmap structs which we will later
  // serialize directly to the head of the bitmap file
  LPBITMAPINFOHEADER bmi = (LPBITMAPINFOHEADER)new BITMAPINFO;
  bmi->biSize = sizeof(BITMAPINFOHEADER);
  bmi->biWidth = width;
  bmi->biHeight = height;
  bmi->biPlanes = 1;
  bmi->biBitCount = (WORD)bitCount*8;
  bmi->biCompression = BI_RGB;
  bmi->biSizeImage = 0; // don't need to set this if bmp is uncompressed
  bmi->biXPelsPerMeter = 0;
  bmi->biYPelsPerMeter = 0;
  bmi->biClrUsed = 0;
  bmi->biClrImportant = 0;

  BITMAPFILEHEADER bf;
  bf.bfType = 0x4D42; // 'BM'
  bf.bfReserved1 = 0;
  bf.bfReserved2 = 0;
  bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  bf.bfSize = bf.bfOffBits + bmi->biSizeImage;

  // get a file output stream
  nsresult rv;
  nsCOMPtr<nsILocalFile> path;
  rv = NS_NewLocalFile(aPath, PR_TRUE, getter_AddRefs(path));

  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIOutputStream> stream;
  NS_NewLocalFileOutputStream(getter_AddRefs(stream), path);

  // write the bitmap headers and rgb pixel data to the file
  rv = NS_ERROR_FAILURE;
  if (stream) {
    PRUint32 written;
    stream->Write((const char*)&bf, sizeof(BITMAPFILEHEADER), &written);
    if (written == sizeof(BITMAPFILEHEADER)) {
      stream->Write((const char*)bmi, sizeof(BITMAPINFOHEADER), &written);
      if (written == sizeof(BITMAPINFOHEADER)) {
        stream->Write((const char*)bits, length, &written);
        if (written == length)
          rv = NS_OK;
      }
    }

    stream->Close();
  }

  return rv;
}

NS_IMETHODIMP
nsWindowsShellService::SetDesktopBackground(nsIDOMElement* aElement, 
                                            PRInt32 aPosition)
{
  nsresult rv;

  nsCOMPtr<gfxIImageFrame> gfxFrame;

  nsCOMPtr<nsIDOMHTMLImageElement> imgElement(do_QueryInterface(aElement));
  if (!imgElement) {
    // XXX write background loading stuff!
  } 
  else {
    nsCOMPtr<nsIImageLoadingContent> imageContent = do_QueryInterface(aElement, &rv);
    if (!imageContent) return rv;

    // get the image container
    nsCOMPtr<imgIRequest> request;
    rv = imageContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                                  getter_AddRefs(request));
    if (!request) return rv;
    nsCOMPtr<imgIContainer> container;
    rv = request->GetImage(getter_AddRefs(container));
    if (!request) return rv;

    // get the current frame, which holds the image data
    container->GetCurrentFrame(getter_AddRefs(gfxFrame));
  }

  if (!gfxFrame)
    return NS_ERROR_FAILURE;

  // get the windows directory ('c:\windows' usually)
  char winDir[256];
  ::GetWindowsDirectory(winDir, sizeof(winDir));
  nsAutoString winPath;
  NS_CopyNativeToUnicode(nsDependentCString(winDir), winPath);

  // get the product brand name from localized strings
  nsXPIDLString brandName;
  nsCID bundleCID = NS_STRINGBUNDLESERVICE_CID;
  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(bundleCID));
  if (bundleService) {
    nsCOMPtr<nsIStringBundle> brandBundle;
    rv = bundleService->CreateBundle("chrome://global/locale/brand.properties",
                                     getter_AddRefs(brandBundle));
    if (NS_SUCCEEDED(rv) && brandBundle) {
      if (NS_FAILED(rv = brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                            getter_Copies(brandName))))
        return rv;
    }
  }

  // build the file name
  winPath.Append(NS_LITERAL_STRING("\\").get());
  winPath.Append(brandName);
  winPath.Append(NS_LITERAL_STRING(" Wallpaper.bmp").get());

  // write the bitmap to a file in the windows dir
  rv = WriteBitmap(winPath, gfxFrame);

  // if the file was written successfully, set it as the system wallpaper
  if (NS_SUCCEEDED(rv)) {
     char   subKey[] = "Control Panel\\Desktop";
     PRBool result = PR_FALSE;
     DWORD  dwDisp = 0;
     HKEY   key;
     // Try to create/open a subkey under HKLM.
     DWORD rc = ::RegCreateKeyEx( HKEY_CURRENT_USER,
                                  subKey,
                                  0,
                                  NULL,
                                  REG_OPTION_NON_VOLATILE,
                                  KEY_WRITE,
                                  NULL,
                                  &key,
                                  &dwDisp );
    if (REG_SUCCEEDED(rc)) {
      unsigned char tile[2];
      unsigned char style[2];
      if (aPosition == BACKGROUND_TILE) {
        tile[0] = '1';
        style[0] = '1';
      }
      else if (aPosition == BACKGROUND_CENTER) {
        tile[0] = '0';
        style[0] = '0';
      }
      else if (aPosition == BACKGROUND_STRETCH) {
        tile[0] = '0';
        style[0] = '2';
      }
      tile[1] = '\0';
      style[1] = '\0';
      ::RegSetValueEx(key, "TileWallpaper", 0, REG_SZ, tile, sizeof(tile));
      ::RegSetValueEx(key, "WallpaperStyle", 0, REG_SZ, style, sizeof(style));
      nsCAutoString nativePath;
      NS_CopyUnicodeToNative(winPath, nativePath);
      char *pathCStr = ToNewCString(nativePath);
      ::SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, pathCStr, SPIF_UPDATEINIFILE | SPIF_SENDWININICHANGE);
      nsMemory::Free(pathCStr);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsWindowsShellService::OpenPreferredApplication(PRInt32 aApplication)
{
  nsCAutoString application;
  switch (aApplication) {
  case nsIShellService::APPLICATION_MAIL:
    application = NS_LITERAL_CSTRING("Mail");
    break;
  case nsIShellService::APPLICATION_NEWS:
    application = NS_LITERAL_CSTRING("News");
    break;
  }

  // The Default Client section of the Windows Registry looks like this:
  // 
  // Clients\aClient\
  //  e.g. aClient = "Mail"...
  //        \Mail\(default) = Client Subkey Name
  //             \Client Subkey Name
  //             \Client Subkey Name\shell\open\command\ 
  //             \Client Subkey Name\shell\open\command\(default) = path to exe
  //
  nsCAutoString clientKey(NS_LITERAL_CSTRING("SOFTWARE\\Clients\\"));
  clientKey += application;

  // Find the default application for this class.
  HKEY theKey;
  nsresult rv = OpenUserKeyForReading(HKEY_CURRENT_USER, clientKey.get(), &theKey);
  if (NS_FAILED(rv)) return rv;

  char buf[MAX_BUF];
  DWORD type, len = sizeof buf;
  DWORD result = ::RegQueryValueEx(theKey, "", 0, &type, (LPBYTE)&buf, &len);
  if (REG_FAILED(result) || nsDependentCString(buf).IsEmpty()) 
    return NS_OK;

  // Find the "open" command
  clientKey.Append("\\");
  clientKey.Append(buf);
  clientKey.Append("\\shell\\open\\command");

  rv = OpenUserKeyForReading(HKEY_CURRENT_USER, clientKey.get(), &theKey);
  if (NS_FAILED(rv)) return rv;

  ::ZeroMemory(buf, sizeof(buf));
  len = sizeof buf;
  result = ::RegQueryValueEx(theKey, "", 0, &type, (LPBYTE)&buf, &len);
  if (REG_FAILED(result) || nsDependentCString(buf).IsEmpty()) 
    return NS_ERROR_FAILURE;

  nsCAutoString path(buf);

  // Look for any embedded environment variables and substitute their 
  // values, as |::CreateProcess| is unable to do this. 
  PRInt32 end = path.Length();
  PRInt32 cursor = 0, temp = 0;
  ::ZeroMemory(buf, sizeof(buf));
  do {
    // XXX : This would not work with multibyte strings. 
    cursor = path.FindChar('%', cursor);
    if (cursor < 0) 
      break;

    temp = path.FindChar('%', cursor + 1);

    ++cursor;

    ::ZeroMemory(&buf, sizeof(buf));
    ::GetEnvironmentVariable(nsCAutoString(Substring(path, cursor, temp - cursor)).get(), 
                             buf, sizeof(buf));
    
    // "+ 2" is to subtract the extra characters used to delimit the environment
    // variable ('%').
    path.Replace((cursor - 1), temp - cursor + 2, nsDependentCString(buf));

    ++cursor;
  }
  while (cursor < end);

  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  ::ZeroMemory(&si, sizeof(STARTUPINFO));
  ::ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

  char *pathCStr = ToNewCString(path);
  BOOL success = ::CreateProcess(NULL, pathCStr, NULL, NULL, FALSE, 0, NULL, 
                                 NULL, &si, &pi);
  nsMemory::Free(pathCStr);
  if (!success)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::GetDesktopBackgroundColor(PRUint32* aColor)
{
  PRUint32 color = ::GetSysColor(COLOR_DESKTOP);
  *aColor = (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::SetDesktopBackgroundColor(PRUint32 aColor)
{
  int aParameters[2] = { COLOR_BACKGROUND, COLOR_DESKTOP };
  BYTE r = (aColor >> 16);
  BYTE g = (aColor << 16) >> 24;
  BYTE b = (aColor << 24) >> 24;
  COLORREF colors[2] = { RGB(r,g,b), RGB(r,g,b) };

  ::SetSysColors(sizeof(aParameters) / sizeof(int), aParameters, colors);

  char   subKey[] = "Control Panel\\Colors";
  PRBool result = PR_FALSE;
  DWORD  dwDisp = 0;
  HKEY   key;
  // Try to create/open a subkey under HKLM.
  DWORD rc = ::RegCreateKeyEx(HKEY_CURRENT_USER, subKey, 0, NULL, 
                              REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &key,
                              &dwDisp);
  if (REG_SUCCEEDED(rc)) {
    unsigned char rgb[12];
    sprintf((char*)rgb, "%u %u %u\0", r, g, b);
    ::RegSetValueEx(key, "Background", 0, REG_SZ, (const unsigned char*)rgb, strlen((char*)rgb));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::GetUnreadMailCount(PRUint32* aCount)
{
  *aCount = 0;

  HKEY accountKey;
  if (GetMailAccountKey(&accountKey)) {
    DWORD type, unreadCount;
    DWORD len = sizeof unreadCount;
    DWORD result = ::RegQueryValueEx(accountKey, "MessageCount", 0, &type, 
                                     (LPBYTE)&unreadCount, &len);
    if (REG_SUCCEEDED(result)) {
      *aCount = unreadCount;
    }
  }

  return NS_OK;
}

PRBool
nsWindowsShellService::GetMailAccountKey(HKEY* aResult)
{
  HKEY mailKey;
  DWORD result = ::RegOpenKeyEx(HKEY_CURRENT_USER, 
                                "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\UnreadMail\\",
                                0, KEY_ENUMERATE_SUB_KEYS, &mailKey);

  PRInt32 i = 0;
  do {
    char subkeyName[MAX_BUF];
    DWORD len = sizeof subkeyName;
    result = ::RegEnumKeyEx(mailKey, i++, subkeyName, &len, 0, 0, 0, 0);
    if (REG_SUCCEEDED(result)) {
      HKEY accountKey;
      result = ::RegOpenKeyEx(mailKey, subkeyName, 0, KEY_READ, &accountKey);
      if (REG_SUCCEEDED(result)) {
        *aResult = accountKey;
        return PR_TRUE;
      }
    }
    else
      break;
  }
  while (1);

  return PR_FALSE;
}

NS_IMETHODIMP
nsWindowsShellService::GetRegistryEntry(PRInt32 aHKEYConstant,
                                        const char *aSubKeyName,
                                        const char *aValueName,
                                        char **aResult)
{
  HKEY hKey, fullKey;

  *aResult = 0;
  // Calculate HKEY_* base key
  switch (aHKEYConstant) {
  case HKCR:
    hKey = HKEY_CLASSES_ROOT;
    break;
  case HKCC:
    hKey = HKEY_CURRENT_CONFIG;
    break;
  case HKCU:
    hKey = HKEY_CURRENT_USER;
    break;
  case HKLM:
    hKey = HKEY_LOCAL_MACHINE;
    break;
  case HKU:
    hKey = HKEY_USERS;
    break;
  default:
    return NS_ERROR_INVALID_ARG;
  }
  
  // Open Key
  LONG rv = ::RegOpenKeyEx(hKey, aSubKeyName, 0, KEY_READ, &fullKey);

  if (rv == ERROR_SUCCESS) {
    char buffer[4096] = { 0 };
    DWORD len = sizeof buffer;
    rv = ::RegQueryValueEx(fullKey, aValueName, NULL, NULL,
                           (LPBYTE)buffer, &len);

    if (rv == ERROR_SUCCESS)
      *aResult = PL_strdup(buffer);
  }

  ::RegCloseKey(fullKey);

  return *aResult ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
