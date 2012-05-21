/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include <shlwapi.h>

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
//     shell\open\command               (default)         REG_SZ     <apppath> -osint -url "%1"
//     shell\open\ddeexec               (default)         REG_SZ     <empty string>
//
// - Windows Vista and above Protocol Handler
//
//   HKCU\SOFTWARE\Classes\FirefoxURL\  (default)         REG_SZ     <appname> URL
//                                      EditFlags         REG_DWORD  2
//                                      FriendlyTypeName  REG_SZ     <appname> URL
//     DefaultIcon                      (default)         REG_SZ     <apppath>,1
//     shell\open\command               (default)         REG_SZ     <apppath> -osint -url "%1"
//     shell\open\ddeexec               (default)         REG_SZ     <empty string>
//
// - Protocol Mappings
//   -----------------
//   The following protocols:
//    HTTP, HTTPS, FTP
//   are mapped like so:
//
//   HKCU\SOFTWARE\Classes\<protocol>\
//     DefaultIcon                      (default)         REG_SZ     <apppath>,1
//     shell\open\command               (default)         REG_SZ     <apppath> -osint -url "%1"
//     shell\open\ddeexec               (default)         REG_SZ     <empty string>
//
// - Windows Start Menu (XP SP1 and newer)
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

// The values checked are all default values so the value name is not needed.
typedef struct {
  char* keyName;
  char* valueData;
  char* oldValueData;
} SETTING;

#define APP_REG_NAME L"Firefox"
#define VAL_FILE_ICON "%APPPATH%,1"
#define VAL_OPEN "\"%APPPATH%\" -osint -url \"%1\""
#define OLD_VAL_OPEN "\"%APPPATH%\" -requestPending -osint -url \"%1\""
#define DI "\\DefaultIcon"
#define SOC "\\shell\\open\\command"
#define SOD "\\shell\\open\\ddeexec"
// Used for updating the FTP protocol handler's shell open command under HKCU.
#define FTP_SOC L"Software\\Classes\\ftp\\shell\\open\\command"

#define MAKE_KEY_NAME1(PREFIX, MID) \
  PREFIX MID

// The DefaultIcon registry key value should never be used when checking if
// Firefox is the default browser for file handlers since other applications
// (e.g. MS Office) may modify the DefaultIcon registry key value to add Icon
// Handlers. see http://msdn2.microsoft.com/en-us/library/aa969357.aspx for
// more info. The FTP protocol is not checked so advanced users can set the FTP
// handler to another application and still have Firefox check if it is the
// default HTTP and HTTPS handler.
static SETTING gSettings[] = {
  // File Handler Class
  { MAKE_KEY_NAME1("FirefoxHTML", SOC), VAL_OPEN, OLD_VAL_OPEN },

  // Protocol Handler Class - for Vista and above
  { MAKE_KEY_NAME1("FirefoxURL", SOC), VAL_OPEN, OLD_VAL_OPEN },

  // Protocol Handlers
  { MAKE_KEY_NAME1("HTTP", DI), VAL_FILE_ICON },
  { MAKE_KEY_NAME1("HTTP", SOC), VAL_OPEN, OLD_VAL_OPEN },
  { MAKE_KEY_NAME1("HTTPS", DI), VAL_FILE_ICON },
  { MAKE_KEY_NAME1("HTTPS", SOC), VAL_OPEN, OLD_VAL_OPEN }
};

// The settings to disable DDE are separate from the default browser settings
// since they are only checked when Firefox is the default browser and if they
// are incorrect they are fixed without notifying the user.
static SETTING gDDESettings[] = {
  // File Handler Class
  { MAKE_KEY_NAME1("Software\\Classes\\FirefoxHTML", SOD) },

  // Protocol Handler Class - for Vista and above
  { MAKE_KEY_NAME1("Software\\Classes\\FirefoxURL", SOD) },

  // Protocol Handlers
  { MAKE_KEY_NAME1("Software\\Classes\\FTP", SOD) },
  { MAKE_KEY_NAME1("Software\\Classes\\HTTP", SOD) },
  { MAKE_KEY_NAME1("Software\\Classes\\HTTPS", SOD) }
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

  if (!CreateProcessW(NULL, (LPWSTR)aPath.get(), NULL, NULL, FALSE, 0, NULL,
                      NULL, &si, &pi)) {
    return NS_ERROR_FAILURE;
  }

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

  *aIsDefaultBrowser = true;

  PRUnichar exePath[MAX_BUF];
  if (!::GetModuleFileNameW(0, exePath, MAX_BUF))
    return NS_ERROR_FAILURE;

  // Convert the path to a long path since GetModuleFileNameW returns the path
  // that was used to launch Firefox which is not necessarily a long path.
  if (!::GetLongPathNameW(exePath, exePath, MAX_BUF))
    return NS_ERROR_FAILURE;

  nsAutoString appLongPath(exePath);

  HKEY theKey;
  DWORD res;
  nsresult rv;
  PRUnichar currValue[MAX_BUF];

  SETTING* settings;
  SETTING* end = gSettings + sizeof(gSettings) / sizeof(SETTING);

  for (settings = gSettings; settings < end; ++settings) {
    NS_ConvertUTF8toUTF16 keyName(settings->keyName);
    NS_ConvertUTF8toUTF16 valueData(settings->valueData);
    PRInt32 offset = valueData.Find("%APPPATH%");
    valueData.Replace(offset, 9, appLongPath);

    rv = OpenKeyForReading(HKEY_CLASSES_ROOT, keyName, &theKey);
    if (NS_FAILED(rv)) {
      *aIsDefaultBrowser = false;
      return NS_OK;
    }

    ::ZeroMemory(currValue, sizeof(currValue));
    DWORD len = sizeof currValue;
    res = ::RegQueryValueExW(theKey, L"", NULL, NULL, (LPBYTE)currValue, &len);
    // Close the key that was opened.
    ::RegCloseKey(theKey);
    if (REG_FAILED(res) ||
        !valueData.Equals(currValue, CaseInsensitiveCompare)) {
      // Key wasn't set or was set to something other than our registry entry.
      NS_ConvertUTF8toUTF16 oldValueData(settings->oldValueData);
      offset = oldValueData.Find("%APPPATH%");
      oldValueData.Replace(offset, 9, appLongPath);
      // The current registry value doesn't match the current or the old format.
      if (!oldValueData.Equals(currValue, CaseInsensitiveCompare)) {
        *aIsDefaultBrowser = false;
        return NS_OK;
      }

      res = ::RegOpenKeyExW(HKEY_CLASSES_ROOT, PromiseFlatString(keyName).get(),
                            0, KEY_SET_VALUE, &theKey);
      if (REG_FAILED(res)) {
        // If updating the open command fails try to update it using the helper
        // application when setting Firefox as the default browser.
        *aIsDefaultBrowser = false;
        return NS_OK;
      }

      const nsString &flatValue = PromiseFlatString(valueData);
      res = ::RegSetValueExW(theKey, L"", 0, REG_SZ,
                             (const BYTE *) flatValue.get(),
                             (flatValue.Length() + 1) * sizeof(PRUnichar));
      // Close the key that was created.
      ::RegCloseKey(theKey);
      if (REG_FAILED(res)) {
        // If updating the open command fails try to update it using the helper
        // application when setting Firefox as the default browser.
        *aIsDefaultBrowser = false;
        return NS_OK;
      }
    }
  }

  // Only check if Firefox is the default browser on Vista and above if the
  // previous checks show that Firefox is the default browser.
  if (*aIsDefaultBrowser) {
    IsDefaultBrowserVista(aIsDefaultBrowser);
  }

  // To handle the case where DDE isn't disabled due for a user because there
  // account didn't perform a Firefox update this will check if Firefox is the
  // default browser and if dde is disabled for each handler
  // and if it isn't disable it. When Firefox is not the default browser the
  // helper application will disable dde for each handler.
  if (*aIsDefaultBrowser) {
    // Check ftp settings

    end = gDDESettings + sizeof(gDDESettings) / sizeof(SETTING);

    for (settings = gDDESettings; settings < end; ++settings) {
      NS_ConvertUTF8toUTF16 keyName(settings->keyName);

      rv = OpenKeyForReading(HKEY_CURRENT_USER, keyName, &theKey);
      if (NS_FAILED(rv)) {
        ::RegCloseKey(theKey);
        // If disabling DDE fails try to disable it using the helper
        // application when setting Firefox as the default browser.
        *aIsDefaultBrowser = false;
        return NS_OK;
      }

      ::ZeroMemory(currValue, sizeof(currValue));
      DWORD len = sizeof currValue;
      res = ::RegQueryValueExW(theKey, L"", NULL, NULL, (LPBYTE)currValue,
                               &len);
      // Close the key that was opened.
      ::RegCloseKey(theKey);
      if (REG_FAILED(res) || PRUnichar('\0') != *currValue) {
        // Key wasn't set or was set to something other than our registry entry.
        // Delete the key along with all of its childrean and then recreate it.
        const nsString &flatName = PromiseFlatString(keyName);
        ::SHDeleteKeyW(HKEY_CURRENT_USER, flatName.get());
        res = ::RegCreateKeyExW(HKEY_CURRENT_USER, flatName.get(), 0, NULL,
                                REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL,
                                &theKey, NULL);
        if (REG_FAILED(res)) {
          // If disabling DDE fails try to disable it using the helper
          // application when setting Firefox as the default browser.
          *aIsDefaultBrowser = false;
          return NS_OK;
        }

        res = ::RegSetValueExW(theKey, L"", 0, REG_SZ, (const BYTE *) L"",
                               sizeof(PRUnichar));
        // Close the key that was created.
        ::RegCloseKey(theKey);
        if (REG_FAILED(res)) {
          // If disabling DDE fails try to disable it using the helper
          // application when setting Firefox as the default browser.
          *aIsDefaultBrowser = false;
          return NS_OK;
        }
      }
    }

    // Update the FTP protocol handler's shell open command if it is the old
    // format.
    res = ::RegOpenKeyExW(HKEY_CURRENT_USER, FTP_SOC, 0, KEY_ALL_ACCESS,
                          &theKey);
    // Don't update the FTP protocol handler's shell open command when opening
    // its registry key fails under HKCU since it most likely doesn't exist.
    if (NS_FAILED(rv)) {
      return NS_OK;
    }

    NS_ConvertUTF8toUTF16 oldValueOpen(OLD_VAL_OPEN);
    PRInt32 offset = oldValueOpen.Find("%APPPATH%");
    oldValueOpen.Replace(offset, 9, appLongPath);

    ::ZeroMemory(currValue, sizeof(currValue));
    DWORD len = sizeof currValue;
    res = ::RegQueryValueExW(theKey, L"", NULL, NULL, (LPBYTE)currValue,
                             &len);

    // Don't update the FTP protocol handler's shell open command when the
    // current registry value doesn't exist or matches the old format.
    if (REG_FAILED(res) ||
        !oldValueOpen.Equals(currValue, CaseInsensitiveCompare)) {
      ::RegCloseKey(theKey);
      return NS_OK;
    }

    NS_ConvertUTF8toUTF16 valueData(VAL_OPEN);
    valueData.Replace(offset, 9, appLongPath);
    const nsString &flatValue = PromiseFlatString(valueData);
    res = ::RegSetValueExW(theKey, L"", 0, REG_SZ,
                           (const BYTE *) flatValue.get(),
                           (flatValue.Length() + 1) * sizeof(PRUnichar));
    // Close the key that was created.
    ::RegCloseKey(theKey);
    // If updating the FTP protocol handlers shell open command fails try to
    // update it using the helper application when setting Firefox as the
    // default browser.
    if (REG_FAILED(res)) {
      *aIsDefaultBrowser = false;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::GetCanSetDesktopBackground(bool* aResult)
{
  *aResult = true;
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
    nsCOMPtr<nsIWindowsRegKey> regKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = regKey->Create(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                        NS_LITERAL_STRING("Control Panel\\Desktop"),
                        nsIWindowsRegKey::ACCESS_SET_VALUE);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString tile;
    nsAutoString style;
    switch (aPosition) {
      case BACKGROUND_TILE:
        style.AssignLiteral("0");
        tile.AssignLiteral("1");
        break;
      case BACKGROUND_CENTER:
        style.AssignLiteral("0");
        tile.AssignLiteral("0");
        break;
      case BACKGROUND_STRETCH:
        style.AssignLiteral("2");
        tile.AssignLiteral("0");
        break;
      case BACKGROUND_FILL:
        style.AssignLiteral("10");
        tile.AssignLiteral("0");
        break;
      case BACKGROUND_FIT:
        style.AssignLiteral("6");
        tile.AssignLiteral("0");
        break;
    }

    rv = regKey->WriteStringValue(NS_LITERAL_STRING("TileWallpaper"), tile);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = regKey->WriteStringValue(NS_LITERAL_STRING("WallpaperStyle"), style);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = regKey->Close();
    NS_ENSURE_SUCCESS(rv, rv);

    ::SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)path.get(),
                            SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
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

  nsresult rv;
  nsCOMPtr<nsIWindowsRegKey> regKey =
    do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = regKey->Create(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                      NS_LITERAL_STRING("Control Panel\\Colors"),
                      nsIWindowsRegKey::ACCESS_SET_VALUE);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUnichar rgb[12];
  _snwprintf(rgb, 12, L"%u %u %u", r, g, b);

  rv = regKey->WriteStringValue(NS_LITERAL_STRING("Background"),
                                nsDependentString(rgb));
  NS_ENSURE_SUCCESS(rv, rv);

  return regKey->Close();
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
