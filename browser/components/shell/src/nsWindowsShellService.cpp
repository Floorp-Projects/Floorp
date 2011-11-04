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
 *  Ben Goodger    <ben@mozilla.org>       (Clients, Mail, New Default Browser)
 *  Joe Hewitt     <hewitt@netscape.com>   (Set Background)
 *  Blake Ross     <blake@cs.stanford.edu> (Desktop Color, DDE support)
 *  Jungshik Shin  <jshin@mailaps.org>     (I18N)
 *  Robert Strong  <robert.bugzilla@gmail.com>
 *  Asaf Romano    <mano@mozilla.com>
 *  Ryan Jones     <sciguyryan@gmail.com>
 *  Paul O'Shannessy <paul@oshannessy.com>
 *  Jim Mathies    <jmathies@mozilla.com>
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

#include "imgIContainer.h"
#include "imgIRequest.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIImageLoadingContent.h"
#include "nsIPrefService.h"
#include "nsIPrefLocalizedString.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsNetUtil.h"
#include "nsShellService.h"
#include "nsWindowsShellService.h"
#include "nsIProcess.h"
#include "nsICategoryManager.h"
#include "nsBrowserCompsCID.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIWindowsRegKey.h"
#include "nsUnicharUtils.h"
#include "nsIWinTaskbar.h"
#include "nsISupportsPrimitives.h"

#include "windows.h"
#include "shellapi.h"

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600
#define INITGUID
#include <shlobj.h>

#include <mbstring.h>

#ifndef MAX_BUF
#define MAX_BUF 4096
#endif

#define REG_SUCCEEDED(val) \
  (val == ERROR_SUCCESS)

#define REG_FAILED(val) \
  (val != ERROR_SUCCESS)

#define NS_TASKBAR_CONTRACTID "@mozilla.org/windows-taskbar;1"

NS_IMPL_ISUPPORTS2(nsWindowsShellService, nsIWindowsShellService, nsIShellService)

static nsresult
OpenKeyForReading(HKEY aKeyRoot, const nsAString& aKeyName, HKEY* aKey)
{
  const nsString &flatName = PromiseFlatString(aKeyName);

  DWORD res = ::RegOpenKeyExW(aKeyRoot, flatName.get(), 0, KEY_READ, aKey);
  switch (res) {
  case ERROR_SUCCESS:
    break;
  case ERROR_ACCESS_DENIED:
    return NS_ERROR_FILE_ACCESS_DENIED;
  case ERROR_FILE_NOT_FOUND:
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Default Browser Registry Settings
//
// The setting of these values are made by an external binary since writing
// these values may require elevation.
//
// - File Extension Mappings
//   -----------------------
//   The following file extensions:
//    .htm .html .shtml .xht .xhtml 
//   are mapped like so:
//
//   HKCU\SOFTWARE\Classes\.<ext>\      (default)         REG_SZ     FirefoxHTML
//
//   as aliases to the class:
//
//   HKCU\SOFTWARE\Classes\FirefoxHTML\
//     DefaultIcon                      (default)         REG_SZ     <apppath>,1
//     shell\open\command               (default)         REG_SZ     <apppath> -requestPending -osint -url "%1"
//     shell\open\ddeexec               (default)         REG_SZ     "%1",,0,0,,,,
//     shell\open\ddeexec               NoActivateHandler REG_SZ
//                       \Application   (default)         REG_SZ     Firefox
//                       \Topic         (default)         REG_SZ     WWW_OpenURL
//
// - Windows Vista Protocol Handler
//
//   HKCU\SOFTWARE\Classes\FirefoxURL\  (default)         REG_SZ     <appname> URL
//                                      EditFlags         REG_DWORD  2
//                                      FriendlyTypeName  REG_SZ     <appname> URL
//     DefaultIcon                      (default)         REG_SZ     <apppath>,1
//     shell\open\command               (default)         REG_SZ     <apppath> -requestPending -osint -url "%1"
//     shell\open\ddeexec               (default)         REG_SZ     "%1",,0,0,,,,
//     shell\open\ddeexec               NoActivateHandler REG_SZ
//                       \Application   (default)         REG_SZ     Firefox
//                       \Topic         (default)         REG_SZ     WWW_OpenURL
//
// - Protocol Mappings
//   -----------------
//   The following protocols:
//    HTTP, HTTPS, FTP
//   are mapped like so:
//
//   HKCU\SOFTWARE\Classes\<protocol>\
//     DefaultIcon                      (default)         REG_SZ     <apppath>,1
//     shell\open\command               (default)         REG_SZ     <apppath> -requestPending -osint -url "%1"
//     shell\open\ddeexec               (default)         REG_SZ     "%1",,0,0,,,,
//     shell\open\ddeexec               NoActivateHandler REG_SZ
//                       \Application   (default)         REG_SZ     Firefox
//                       \Topic         (default)         REG_SZ     WWW_OpenURL
//
// - Windows Start Menu (Win2K SP2, XP SP1, and newer)
//   -------------------------------------------------
//   The following keys are set to make Firefox appear in the Start Menu as the
//   browser:
//   
//   HKCU\SOFTWARE\Clients\StartMenuInternet\FIREFOX.EXE\
//                                      (default)         REG_SZ     <appname>
//     DefaultIcon                      (default)         REG_SZ     <apppath>,0
//     InstallInfo                      HideIconsCommand  REG_SZ     <uninstpath> /HideShortcuts
//     InstallInfo                      IconsVisible      REG_DWORD  1
//     InstallInfo                      ReinstallCommand  REG_SZ     <uninstpath> /SetAsDefaultAppGlobal
//     InstallInfo                      ShowIconsCommand  REG_SZ     <uninstpath> /ShowShortcuts
//     shell\open\command               (default)         REG_SZ     <apppath>
//     shell\properties                 (default)         REG_SZ     <appname> &Options
//     shell\properties\command         (default)         REG_SZ     <apppath> -preferences
//     shell\safemode                   (default)         REG_SZ     <appname> &Safe Mode
//     shell\safemode\command           (default)         REG_SZ     <apppath> -safe-mode
//

typedef struct {
  char* keyName;
  char* valueName;
  char* valueData;
} SETTING;

#define APP_REG_NAME L"Firefox"
#define CLS_HTML "FirefoxHTML"
#define CLS_URL "FirefoxURL"
#define CPL_DESKTOP L"Control Panel\\Desktop"
#define VAL_OPEN "\"%APPPATH%\" -requestPending -osint -url \"%1\""
#define VAL_FILE_ICON "%APPPATH%,1"
#define DI "\\DefaultIcon"
#define SOP "\\shell\\open\\command"

#define MAKE_KEY_NAME1(PREFIX, MID) \
  PREFIX MID

// The DefaultIcon registry key value should never be used when checking if
// Firefox is the default browser for file handlers since other applications
// (e.g. MS Office) may modify the DefaultIcon registry key value to add Icon
// Handlers. see http://msdn2.microsoft.com/en-us/library/aa969357.aspx for
// more info.
static SETTING gSettings[] = {
  // File Handler Class
  { MAKE_KEY_NAME1(CLS_HTML, SOP), "", VAL_OPEN },

  // Protocol Handler Class - for Vista and above
  { MAKE_KEY_NAME1(CLS_URL, SOP), "", VAL_OPEN },

  // Protocol Handlers
  { MAKE_KEY_NAME1("HTTP", DI),    "", VAL_FILE_ICON },
  { MAKE_KEY_NAME1("HTTP", SOP),   "", VAL_OPEN },
  { MAKE_KEY_NAME1("HTTPS", DI),   "", VAL_FILE_ICON },
  { MAKE_KEY_NAME1("HTTPS", SOP),  "", VAL_OPEN }
};

nsresult
GetHelperPath(nsAutoString& aPath)
{
  nsresult rv;
  nsCOMPtr<nsIProperties> directoryService = 
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILocalFile> appHelper;
  rv = directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR,
                             NS_GET_IID(nsILocalFile),
                             getter_AddRefs(appHelper));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = appHelper->AppendNative(NS_LITERAL_CSTRING("uninstall"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = appHelper->AppendNative(NS_LITERAL_CSTRING("helper.exe"));
  NS_ENSURE_SUCCESS(rv, rv);

  return appHelper->GetPath(aPath);
}

nsresult
LaunchHelper(nsAutoString& aPath)
{
  STARTUPINFOW si = {sizeof(si), 0};
  PROCESS_INFORMATION pi = {0};

  BOOL ok = CreateProcessW(NULL, (LPWSTR)aPath.get(), NULL, NULL,
                           FALSE, 0, NULL, NULL, &si, &pi);

  if (!ok)
    return NS_ERROR_FAILURE;

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::ShortcutMaintenance()
{
  nsresult rv;

  // XXX App ids were updated to a constant install path hash,
  // XXX this code can be removed after a few upgrade cycles.

  // Launch helper.exe so it can update the application user model ids on
  // shortcuts in the user's taskbar and start menu. This keeps older pinned
  // shortcuts grouped correctly after major updates. Note, we also do this
  // through the upgrade installer script, however, this is the only place we
  // have a chance to trap links created by users who do control the install/
  // update process of the browser.

  nsCOMPtr<nsIWinTaskbar> taskbarInfo =
    do_GetService(NS_TASKBAR_CONTRACTID);
  if (!taskbarInfo) // If we haven't built with win7 sdk features, this fails.
    return NS_OK;

  // Avoid if this isn't Win7+
  bool isSupported = false;
  taskbarInfo->GetAvailable(&isSupported);
  if (!isSupported)
    return NS_OK;

  nsAutoString appId;
  if (NS_FAILED(taskbarInfo->GetDefaultGroupId(appId)))
    return NS_ERROR_UNEXPECTED;

  NS_NAMED_LITERAL_CSTRING(prefName, "browser.taskbar.lastgroupid");
  nsCOMPtr<nsIPrefService> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!prefs)
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIPrefBranch> prefBranch;
  prefs->GetBranch(nsnull, getter_AddRefs(prefBranch));
  if (!prefBranch)
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsISupportsString> prefString;
  rv = prefBranch->GetComplexValue(prefName.get(),
                                   NS_GET_IID(nsISupportsString),
                                   getter_AddRefs(prefString));
  if (NS_SUCCEEDED(rv)) {
    nsAutoString version;
    prefString->GetData(version);
    if (!version.IsEmpty() && version.Equals(appId)) {
      // We're all good, get out of here.
      return NS_OK;
    }
  }
  // Update the version in prefs
  prefString =
    do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  prefString->SetData(appId);
  rv = prefBranch->SetComplexValue(prefName.get(),
                                   NS_GET_IID(nsISupportsString),
                                   prefString);
  if (NS_FAILED(rv)) {
    NS_WARNING("Couldn't set last user model id!");
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoString appHelperPath;
  if (NS_FAILED(GetHelperPath(appHelperPath)))
    return NS_ERROR_UNEXPECTED;

  appHelperPath.AppendLiteral(" /UpdateShortcutAppUserModelIds");

  return LaunchHelper(appHelperPath);
}

bool
nsWindowsShellService::IsDefaultBrowserVista(bool* aIsDefaultBrowser)
{
#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
  IApplicationAssociationRegistration* pAAR;
  
  HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration,
                                NULL,
                                CLSCTX_INPROC,
                                IID_IApplicationAssociationRegistration,
                                (void**)&pAAR);

  if (SUCCEEDED(hr)) {
    BOOL res;
    hr = pAAR->QueryAppIsDefaultAll(AL_EFFECTIVE,
                                    APP_REG_NAME,
                                    &res);
    *aIsDefaultBrowser = res;

    pAAR->Release();
    return true;
  }
#endif  
  return false;
}

NS_IMETHODIMP
nsWindowsShellService::IsDefaultBrowser(bool aStartupCheck,
                                        bool* aIsDefaultBrowser)
{
  // If this is the first browser window, maintain internal state that we've
  // checked this session (so that subsequent window opens don't show the 
  // default browser dialog).
  if (aStartupCheck)
    mCheckedThisSession = true;

  SETTING* settings;
  SETTING* end = gSettings + sizeof(gSettings)/sizeof(SETTING);

  *aIsDefaultBrowser = true;

  PRUnichar exePath[MAX_BUF];
  if (!::GetModuleFileNameW(0, exePath, MAX_BUF))
    return NS_ERROR_FAILURE;

  // Convert the path to a long path since GetModuleFileNameW returns the path
  // that was used to launch Firefox which is not necessarily a long path.
  if (!::GetLongPathNameW(exePath, exePath, MAX_BUF))
    return NS_ERROR_FAILURE;

  nsAutoString appLongPath(exePath);

  nsresult rv;
  PRUnichar currValue[MAX_BUF];
  for (settings = gSettings; settings < end; ++settings) {
    NS_ConvertUTF8toUTF16 dataLongPath(settings->valueData);
    NS_ConvertUTF8toUTF16 key(settings->keyName);
    NS_ConvertUTF8toUTF16 value(settings->valueName);
    PRInt32 offset = dataLongPath.Find("%APPPATH%");
    dataLongPath.Replace(offset, 9, appLongPath);

    ::ZeroMemory(currValue, sizeof(currValue));
    HKEY theKey;
    rv = OpenKeyForReading(HKEY_CLASSES_ROOT, key, &theKey);
    if (NS_FAILED(rv)) {
      *aIsDefaultBrowser = false;
      return NS_OK;
    }

    DWORD len = sizeof currValue;
    DWORD res = ::RegQueryValueExW(theKey, PromiseFlatString(value).get(),
                                   NULL, NULL, (LPBYTE)currValue, &len);
    // Close the key we opened.
    ::RegCloseKey(theKey);
    if (REG_FAILED(res) ||
        !dataLongPath.Equals(currValue, CaseInsensitiveCompare)) {
      // Key wasn't set, or was set to something other than our registry entry
      *aIsDefaultBrowser = false;
      return NS_OK;
    }
  }

  // Only check if Firefox is the default browser on Vista if the previous
  // checks show that Firefox is the default browser.
  if (*aIsDefaultBrowser)
    IsDefaultBrowserVista(aIsDefaultBrowser);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::SetDefaultBrowser(bool aClaimAllTypes, bool aForAllUsers)
{
  nsAutoString appHelperPath;
  if (NS_FAILED(GetHelperPath(appHelperPath)))
    return NS_ERROR_FAILURE;

  if (aForAllUsers) {
    appHelperPath.AppendLiteral(" /SetAsDefaultAppGlobal");
  } else {
    appHelperPath.AppendLiteral(" /SetAsDefaultAppUser");
  }

  return LaunchHelper(appHelperPath);
}

NS_IMETHODIMP
nsWindowsShellService::GetShouldCheckDefaultBrowser(bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  // If we've already checked, the browser has been started and this is a 
  // new window open, and we don't want to check again.
  if (mCheckedThisSession) {
    *aResult = false;
    return NS_OK;
  }

  nsCOMPtr<nsIPrefBranch> prefs;
  nsresult rv;
  nsCOMPtr<nsIPrefService> pserve(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = pserve->GetBranch("", getter_AddRefs(prefs));
  NS_ENSURE_SUCCESS(rv, rv);

  return prefs->GetBoolPref(PREF_CHECKDEFAULTBROWSER, aResult);
}

NS_IMETHODIMP
nsWindowsShellService::SetShouldCheckDefaultBrowser(bool aShouldCheck)
{
  nsCOMPtr<nsIPrefBranch> prefs;
  nsresult rv;

  nsCOMPtr<nsIPrefService> pserve(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = pserve->GetBranch("", getter_AddRefs(prefs));
  NS_ENSURE_SUCCESS(rv, rv);

  return prefs->SetBoolPref(PREF_CHECKDEFAULTBROWSER, aShouldCheck);
}

static nsresult
WriteBitmap(nsIFile* aFile, imgIContainer* aImage)
{
  nsRefPtr<gfxImageSurface> image;
  nsresult rv = aImage->CopyFrame(imgIContainer::FRAME_FIRST,
                                  imgIContainer::FLAG_SYNC_DECODE,
                                  getter_AddRefs(image));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 width = image->Width();
  PRInt32 height = image->Height();

  PRUint8* bits = image->Data();
  PRUint32 length = image->GetDataSize();
  PRUint32 bpr = PRUint32(image->Stride());
  PRInt32 bitCount = bpr/width;

  // initialize these bitmap structs which we will later
  // serialize directly to the head of the bitmap file
  BITMAPINFOHEADER bmi;
  bmi.biSize = sizeof(BITMAPINFOHEADER);
  bmi.biWidth = width;
  bmi.biHeight = height;
  bmi.biPlanes = 1;
  bmi.biBitCount = (WORD)bitCount*8;
  bmi.biCompression = BI_RGB;
  bmi.biSizeImage = length;
  bmi.biXPelsPerMeter = 0;
  bmi.biYPelsPerMeter = 0;
  bmi.biClrUsed = 0;
  bmi.biClrImportant = 0;

  BITMAPFILEHEADER bf;
  bf.bfType = 0x4D42; // 'BM'
  bf.bfReserved1 = 0;
  bf.bfReserved2 = 0;
  bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  bf.bfSize = bf.bfOffBits + bmi.biSizeImage;

  // get a file output stream
  nsCOMPtr<nsIOutputStream> stream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(stream), aFile);
  NS_ENSURE_SUCCESS(rv, rv);

  // write the bitmap headers and rgb pixel data to the file
  rv = NS_ERROR_FAILURE;
  if (stream) {
    PRUint32 written;
    stream->Write((const char*)&bf, sizeof(BITMAPFILEHEADER), &written);
    if (written == sizeof(BITMAPFILEHEADER)) {
      stream->Write((const char*)&bmi, sizeof(BITMAPINFOHEADER), &written);
      if (written == sizeof(BITMAPINFOHEADER)) {
        // write out the image data backwards because the desktop won't
        // show bitmaps with negative heights for top-to-bottom
        PRUint32 i = length;
        do {
          i -= bpr;
          stream->Write(((const char*)bits) + i, bpr, &written);
          if (written == bpr) {
            rv = NS_OK;
          } else {
            rv = NS_ERROR_FAILURE;
            break;
          }
        } while (i != 0);
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

  nsCOMPtr<imgIContainer> container;
  nsCOMPtr<nsIDOMHTMLImageElement> imgElement(do_QueryInterface(aElement));
  if (!imgElement) {
    // XXX write background loading stuff!
    return NS_ERROR_NOT_AVAILABLE;
  } 
  else {
    nsCOMPtr<nsIImageLoadingContent> imageContent =
      do_QueryInterface(aElement, &rv);
    if (!imageContent)
      return rv;

    // get the image container
    nsCOMPtr<imgIRequest> request;
    rv = imageContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                                  getter_AddRefs(request));
    if (!request)
      return rv;
    rv = request->GetImage(getter_AddRefs(container));
    if (!container)
      return NS_ERROR_FAILURE;
  }

  // get the file name from localized strings
  nsCOMPtr<nsIStringBundleService>
    bundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundle> shellBundle;
  rv = bundleService->CreateBundle(SHELLSERVICE_PROPERTIES,
                                   getter_AddRefs(shellBundle));
  NS_ENSURE_SUCCESS(rv, rv);
 
  // e.g. "Desktop Background.bmp"
  nsString fileLeafName;
  rv = shellBundle->GetStringFromName
                      (NS_LITERAL_STRING("desktopBackgroundLeafNameWin").get(),
                       getter_Copies(fileLeafName));
  NS_ENSURE_SUCCESS(rv, rv);

  // get the profile root directory
  nsCOMPtr<nsIFile> file;
  rv = NS_GetSpecialDirectory(NS_APP_APPLICATION_REGISTRY_DIR,
                              getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  // eventually, the path is "%APPDATA%\Mozilla\Firefox\Desktop Background.bmp"
  rv = file->Append(fileLeafName);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString path;
  rv = file->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  // write the bitmap to a file in the profile directory
  rv = WriteBitmap(file, container);

  // if the file was written successfully, set it as the system wallpaper
  if (NS_SUCCEEDED(rv)) {
     bool result = false;
     DWORD  dwDisp = 0;
     HKEY   key;
     // Try to create/open a subkey under HKCU.
     DWORD res = ::RegCreateKeyExW(HKEY_CURRENT_USER, CPL_DESKTOP,
                                   0, NULL, REG_OPTION_NON_VOLATILE,
                                   KEY_WRITE, NULL, &key, &dwDisp);
    if (REG_SUCCEEDED(res)) {
      PRUnichar tile[2], style[2];
      switch (aPosition) {
        case BACKGROUND_TILE:
          tile[0] = '1';
          style[0] = '1';
          break;
        case BACKGROUND_CENTER:
          tile[0] = '0';
          style[0] = '0';
          break;
        case BACKGROUND_STRETCH:
          tile[0] = '0';
          style[0] = '2';
          break;
      }
      tile[1] = '\0';
      style[1] = '\0';

      // The size is always 3 unicode characters.
      PRInt32 size = 3 * sizeof(PRUnichar);
      ::RegSetValueExW(key, L"TileWallpaper",
                       0, REG_SZ, (const BYTE *)tile, size);
      ::RegSetValueExW(key, L"WallpaperStyle",
                       0, REG_SZ, (const BYTE *)style, size);
      ::SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)path.get(),
                              SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

      // Close the key we opened.
      ::RegCloseKey(key);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsWindowsShellService::OpenApplication(PRInt32 aApplication)
{
  nsAutoString application;
  switch (aApplication) {
  case nsIShellService::APPLICATION_MAIL:
    application.AssignLiteral("Mail");
    break;
  case nsIShellService::APPLICATION_NEWS:
    application.AssignLiteral("News");
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

  // Find the default application for this class.
  HKEY theKey;
  nsresult rv = OpenKeyForReading(HKEY_CLASSES_ROOT, application, &theKey);
  if (NS_FAILED(rv))
    return rv;

  PRUnichar buf[MAX_BUF];
  DWORD type, len = sizeof buf;
  DWORD res = ::RegQueryValueExW(theKey, EmptyString().get(), 0,
                                 &type, (LPBYTE)&buf, &len);

  if (REG_FAILED(res) || !*buf)
    return NS_OK;

  // Close the key we opened.
  ::RegCloseKey(theKey);

  // Find the "open" command
  application.AppendLiteral("\\");
  application.Append(buf);
  application.AppendLiteral("\\shell\\open\\command");

  rv = OpenKeyForReading(HKEY_CLASSES_ROOT, application, &theKey);
  if (NS_FAILED(rv))
    return rv;

  ::ZeroMemory(buf, sizeof(buf));
  len = sizeof buf;
  res = ::RegQueryValueExW(theKey, EmptyString().get(), 0,
                           &type, (LPBYTE)&buf, &len);
  if (REG_FAILED(res) || !*buf)
    return NS_ERROR_FAILURE;

  // Close the key we opened.
  ::RegCloseKey(theKey);

  // Look for any embedded environment variables and substitute their 
  // values, as |::CreateProcessW| is unable to do this.
  nsAutoString path(buf);
  PRInt32 end = path.Length();
  PRInt32 cursor = 0, temp = 0;
  ::ZeroMemory(buf, sizeof(buf));
  do {
    cursor = path.FindChar('%', cursor);
    if (cursor < 0) 
      break;

    temp = path.FindChar('%', cursor + 1);
    ++cursor;

    ::ZeroMemory(&buf, sizeof(buf));

    ::GetEnvironmentVariableW(nsAutoString(Substring(path, cursor, temp - cursor)).get(),
                              buf, sizeof(buf));
    
    // "+ 2" is to subtract the extra characters used to delimit the environment
    // variable ('%').
    path.Replace((cursor - 1), temp - cursor + 2, nsDependentString(buf));

    ++cursor;
  }
  while (cursor < end);

  STARTUPINFOW si;
  PROCESS_INFORMATION pi;

  ::ZeroMemory(&si, sizeof(STARTUPINFOW));
  ::ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

  BOOL success = ::CreateProcessW(NULL, (LPWSTR)path.get(), NULL,
                                  NULL, FALSE, 0, NULL,  NULL,
                                  &si, &pi);
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

  bool result = false;
  DWORD  dwDisp = 0;
  HKEY   key;
  // Try to create/open a subkey under HKCU.
  DWORD rv = ::RegCreateKeyExW(HKEY_CURRENT_USER,
                               L"Control Panel\\Colors", 0, NULL,
                               REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                               &key, &dwDisp);

  if (REG_SUCCEEDED(rv)) {
    char rgb[12];
    sprintf((char*)rgb, "%u %u %u\0", r, g, b);
    NS_ConvertUTF8toUTF16 backColor(rgb);

    ::RegSetValueExW(key, L"Background",
                     0, REG_SZ, (const BYTE *)backColor.get(),
                     (backColor.Length() + 1) * sizeof(PRUnichar));
  }
  
  // Close the key we opened.
  ::RegCloseKey(key);
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
    DWORD res = ::RegQueryValueExW(accountKey, L"MessageCount", 0,
                                   &type, (LPBYTE)&unreadCount, &len);
    if (REG_SUCCEEDED(res))
      *aCount = unreadCount;

    // Close the key we opened.
    ::RegCloseKey(accountKey);
  }

  return NS_OK;
}

bool
nsWindowsShellService::GetMailAccountKey(HKEY* aResult)
{
  NS_NAMED_LITERAL_STRING(unread,
    "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\UnreadMail\\");

  HKEY mailKey;
  DWORD res = ::RegOpenKeyExW(HKEY_CURRENT_USER, unread.get(), 0,
                              KEY_ENUMERATE_SUB_KEYS, &mailKey);

  PRInt32 i = 0;
  do {
    PRUnichar subkeyName[MAX_BUF];
    DWORD len = sizeof subkeyName;
    res = ::RegEnumKeyExW(mailKey, i++, subkeyName, &len, NULL, NULL,
                          NULL, NULL);
    if (REG_SUCCEEDED(res)) {
      HKEY accountKey;
      res = ::RegOpenKeyExW(mailKey, PromiseFlatString(subkeyName).get(),
                            0, KEY_READ, &accountKey);
      if (REG_SUCCEEDED(res)) {
        *aResult = accountKey;
    
        // Close the key we opened.
        ::RegCloseKey(mailKey);
	 
        return true;
      }
    }
    else
      break;
  }
  while (1);

  // Close the key we opened.
  ::RegCloseKey(mailKey);
  return false;
}

NS_IMETHODIMP
nsWindowsShellService::OpenApplicationWithURI(nsILocalFile* aApplication,
                                              const nsACString& aURI)
{
  nsresult rv;
  nsCOMPtr<nsIProcess> process = 
    do_CreateInstance("@mozilla.org/process/util;1", &rv);
  if (NS_FAILED(rv))
    return rv;
  
  rv = process->Init(aApplication);
  if (NS_FAILED(rv))
    return rv;
  
  const nsCString spec(aURI);
  const char* specStr = spec.get();
  return process->Run(false, &specStr, 1);
}

NS_IMETHODIMP
nsWindowsShellService::GetDefaultFeedReader(nsILocalFile** _retval)
{
  *_retval = nsnull;

  nsresult rv;
  nsCOMPtr<nsIWindowsRegKey> regKey =
    do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                    NS_LITERAL_STRING("feed\\shell\\open\\command"),
                    nsIWindowsRegKey::ACCESS_READ);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString path;
  rv = regKey->ReadStringValue(EmptyString(), path);
  NS_ENSURE_SUCCESS(rv, rv);
  if (path.IsEmpty())
    return NS_ERROR_FAILURE;

  if (path.First() == '"') {
    // Everything inside the quotes
    path = Substring(path, 1, path.FindChar('"', 1) - 1);
  }
  else {
    // Everything up to the first space
    path = Substring(path, 0, path.FindChar(' '));
  }

  nsCOMPtr<nsILocalFile> defaultReader =
    do_CreateInstance("@mozilla.org/file/local;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = defaultReader->InitWithPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = defaultReader->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*_retval = defaultReader);
  return NS_OK;
}
