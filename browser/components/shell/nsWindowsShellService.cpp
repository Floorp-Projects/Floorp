/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindowsShellService.h"

#include "city.h"
#include "imgIContainer.h"
#include "imgIRequest.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIImageLoadingContent.h"
#include "nsIOutputStream.h"
#include "nsIPrefService.h"
#include "nsIPrefLocalizedString.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsShellService.h"
#include "nsIProcess.h"
#include "nsICategoryManager.h"
#include "nsBrowserCompsCID.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIWindowsRegKey.h"
#include "nsUnicharUtils.h"
#include "nsIURLFormatter.h"
#include "nsXULAppAPI.h"
#include "mozilla/WindowsVersion.h"

#include "windows.h"
#include "shellapi.h"

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600
#define INITGUID
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN8
// Needed for access to IApplicationActivationManager
#include <shlobj.h>

#include <mbstring.h>
#include <shlwapi.h>

#include <lm.h>
#undef ACCESS_READ

#ifndef MAX_BUF
#define MAX_BUF 4096
#endif

#define REG_SUCCEEDED(val) \
  (val == ERROR_SUCCESS)

#define REG_FAILED(val) \
  (val != ERROR_SUCCESS)

#define APP_REG_NAME_BASE L"Firefox-"

using mozilla::IsWin8OrLater;
using namespace mozilla;
using namespace mozilla::gfx;

NS_IMPL_ISUPPORTS(nsWindowsShellService, nsIShellService)

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

nsresult
GetHelperPath(nsAutoString& aPath)
{
  nsresult rv;
  nsCOMPtr<nsIProperties> directoryService =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> appHelper;
  rv = directoryService->Get(XRE_EXECUTABLE_FILE,
                             NS_GET_IID(nsIFile),
                             getter_AddRefs(appHelper));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = appHelper->SetNativeLeafName(NS_LITERAL_CSTRING("uninstall"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = appHelper->AppendNative(NS_LITERAL_CSTRING("helper.exe"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = appHelper->GetPath(aPath);

  aPath.Insert(L'"', 0);
  aPath.Append(L'"');
  return rv;
}

nsresult
LaunchHelper(nsAutoString& aPath)
{
  STARTUPINFOW si = {sizeof(si), 0};
  PROCESS_INFORMATION pi = {0};

  if (!CreateProcessW(nullptr, (LPWSTR)aPath.get(), nullptr, nullptr, FALSE,
                      0, nullptr, nullptr, &si, &pi)) {
    return NS_ERROR_FAILURE;
  }

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return NS_OK;
}

static bool
IsPathDefaultForClass(const RefPtr<IApplicationAssociationRegistration>& pAAR,
                      wchar_t *exePath, LPCWSTR aClassName)
{
  // Make sure the Prog ID matches what we have
  LPWSTR registeredApp;
  bool isProtocol = *aClassName != L'.';
  ASSOCIATIONTYPE queryType = isProtocol ? AT_URLPROTOCOL : AT_FILEEXTENSION;
  HRESULT hr = pAAR->QueryCurrentDefault(aClassName, queryType, AL_EFFECTIVE,
                                         &registeredApp);
  if (FAILED(hr)) {
    return false;
  }

  LPCWSTR progID = isProtocol ? L"FirefoxURL" : L"FirefoxHTML";
  bool isDefault = !wcsnicmp(registeredApp, progID, wcslen(progID));

  nsAutoString regAppName(registeredApp);
  CoTaskMemFree(registeredApp);

  if (isDefault) {
    // Make sure the application path for this progID is this installation.
    regAppName.AppendLiteral("\\shell\\open\\command");
    HKEY theKey;
    nsresult rv = OpenKeyForReading(HKEY_CLASSES_ROOT, regAppName, &theKey);
    if (NS_FAILED(rv)) {
      return false;
    }

    wchar_t cmdFromReg[MAX_BUF] = L"";
    DWORD len = sizeof(cmdFromReg);
    DWORD res = ::RegQueryValueExW(theKey, nullptr, nullptr, nullptr,
                                   (LPBYTE)cmdFromReg, &len);
    ::RegCloseKey(theKey);
    if (REG_FAILED(res)) {
      return false;
    }

    wchar_t fullCmd[MAX_BUF] = L"";
    _snwprintf(fullCmd, MAX_BUF, L"\"%s\" -osint -url \"%%1\"", exePath);

    isDefault = _wcsicmp(fullCmd, cmdFromReg) == 0;
  }

  return isDefault;
}

static nsresult
GetAppRegName(nsAutoString &aAppRegName)
{
  nsresult rv;
  nsCOMPtr<nsIProperties> dirSvc =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> exeFile;
  rv = dirSvc->Get(XRE_EXECUTABLE_FILE,
                   NS_GET_IID(nsIFile),
                   getter_AddRefs(exeFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> appDir;
  rv = exeFile->GetParent(getter_AddRefs(appDir));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString appDirStr;
  rv = appDir->GetPath(appDirStr);
  NS_ENSURE_SUCCESS(rv, rv);

  aAppRegName = APP_REG_NAME_BASE;
  uint64_t hash = CityHash64(static_cast<const char *>(appDirStr.get()),
                             appDirStr.Length() * sizeof(nsAutoString::char_type));
  aAppRegName.AppendInt((int)(hash >> 32), 16);
  aAppRegName.AppendInt((int)hash, 16);

  return rv;
}

NS_IMETHODIMP
nsWindowsShellService::IsDefaultBrowser(bool aStartupCheck,
                                        bool aForAllTypes,
                                        bool* aIsDefaultBrowser)
{
  mozilla::Unused << aStartupCheck;

  *aIsDefaultBrowser = false;

  RefPtr<IApplicationAssociationRegistration> pAAR;
  HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration,
                                nullptr,
                                CLSCTX_INPROC,
                                IID_IApplicationAssociationRegistration,
                                getter_AddRefs(pAAR));
  if (FAILED(hr)) {
    return NS_OK;
  }

  wchar_t exePath[MAX_BUF] = L"";
  if (!::GetModuleFileNameW(0, exePath, MAX_BUF)) {
    return NS_OK;
  }
  // Convert the path to a long path since GetModuleFileNameW returns the path
  // that was used to launch Firefox which is not necessarily a long path.
  if (!::GetLongPathNameW(exePath, exePath, MAX_BUF)) {
    return NS_OK;
  }

  *aIsDefaultBrowser = IsPathDefaultForClass(pAAR, exePath, L"http");
  if (*aIsDefaultBrowser && aForAllTypes) {
    *aIsDefaultBrowser = IsPathDefaultForClass(pAAR, exePath, L".html");
  }
  return NS_OK;
}

nsresult
nsWindowsShellService::LaunchControlPanelDefaultsSelectionUI()
{
  IApplicationAssociationRegistrationUI* pAARUI;
  HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistrationUI,
                                NULL,
                                CLSCTX_INPROC,
                                IID_IApplicationAssociationRegistrationUI,
                                (void**)&pAARUI);
  if (SUCCEEDED(hr)) {
    nsAutoString appRegName;
    GetAppRegName(appRegName);
    hr = pAARUI->LaunchAdvancedAssociationUI(appRegName.get());
    pAARUI->Release();
  }
  return SUCCEEDED(hr) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsWindowsShellService::LaunchControlPanelDefaultPrograms()
{
  // Build the path control.exe path safely
  WCHAR controlEXEPath[MAX_PATH + 1] = { '\0' };
  if (!GetSystemDirectoryW(controlEXEPath, MAX_PATH)) {
    return NS_ERROR_FAILURE;
  }
  LPCWSTR controlEXE = L"control.exe";
  if (wcslen(controlEXEPath) + wcslen(controlEXE) >= MAX_PATH) {
    return NS_ERROR_FAILURE;
  }
  if (!PathAppendW(controlEXEPath, controlEXE)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString params(NS_LITERAL_STRING("control.exe /name Microsoft.DefaultPrograms "
    "/page pageDefaultProgram\\pageAdvancedSettings?pszAppName="));
  nsAutoString appRegName;
  GetAppRegName(appRegName);
  params.Append(appRegName);
  STARTUPINFOW si = {sizeof(si), 0};
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_SHOWDEFAULT;
  PROCESS_INFORMATION pi = {0};
  if (!CreateProcessW(controlEXEPath, static_cast<LPWSTR>(params.get()), nullptr,
                      nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
    return NS_ERROR_FAILURE;
  }
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return NS_OK;
}

static bool
IsWindowsLogonConnected()
{
  WCHAR userName[UNLEN + 1];
  DWORD size = ArrayLength(userName);
  if (!GetUserNameW(userName, &size)) {
    return false;
  }

  LPUSER_INFO_24 info;
  if (NetUserGetInfo(nullptr, userName, 24, (LPBYTE *)&info)
      != NERR_Success) {
    return false;
  }
  bool connected = info->usri24_internet_identity;
  NetApiBufferFree(info);

  return connected;
}

static bool
SettingsAppBelievesConnected()
{
  nsresult rv;
  nsCOMPtr<nsIWindowsRegKey> regKey =
    do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
  if (NS_FAILED(rv)) {
    return false;
  }

  rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                    NS_LITERAL_STRING("SOFTWARE\\Microsoft\\Windows\\Shell\\Associations"),
                    nsIWindowsRegKey::ACCESS_READ);
  if (NS_FAILED(rv)) {
    return false;
  }

  uint32_t value;
  rv = regKey->ReadIntValue(NS_LITERAL_STRING("IsConnectedAtLogon"), &value);
  if (NS_FAILED(rv)) {
    return false;
  }

  return !!value;
}

nsresult
nsWindowsShellService::LaunchModernSettingsDialogDefaultApps()
{
  if (!IsWindowsBuildOrLater(14965) &&
      !IsWindowsLogonConnected() && SettingsAppBelievesConnected()) {
    // Use the classic Control Panel to work around a bug of older
    // builds of Windows 10.
    return LaunchControlPanelDefaultPrograms();
  }

  IApplicationActivationManager* pActivator;
  HRESULT hr = CoCreateInstance(CLSID_ApplicationActivationManager,
                                nullptr,
                                CLSCTX_INPROC,
                                IID_IApplicationActivationManager,
                                (void**)&pActivator);

  if (SUCCEEDED(hr)) {
    DWORD pid;
    hr = pActivator->ActivateApplication(
           L"windows.immersivecontrolpanel_cw5n1h2txyewy"
           L"!microsoft.windows.immersivecontrolpanel",
           L"page=SettingsPageAppsDefaults", AO_NONE, &pid);
    if (SUCCEEDED(hr)) {
      // Do not check error because we could at least open
      // the "Default apps" setting.
      pActivator->ActivateApplication(
             L"windows.immersivecontrolpanel_cw5n1h2txyewy"
             L"!microsoft.windows.immersivecontrolpanel",
             L"page=SettingsPageAppsDefaults"
             L"&target=SystemSettings_DefaultApps_Browser", AO_NONE, &pid);
    }
    pActivator->Release();
    return SUCCEEDED(hr) ? NS_OK : NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
nsWindowsShellService::InvokeHTTPOpenAsVerb()
{
  nsCOMPtr<nsIURLFormatter> formatter(
    do_GetService("@mozilla.org/toolkit/URLFormatterService;1"));
  if (!formatter) {
    return NS_ERROR_UNEXPECTED;
  }

  nsString urlStr;
  nsresult rv = formatter->FormatURLPref(
    NS_LITERAL_STRING("app.support.baseURL"), urlStr);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!StringBeginsWith(urlStr, NS_LITERAL_STRING("https://"))) {
    return NS_ERROR_FAILURE;
  }
  urlStr.AppendLiteral("win10-default-browser");

  SHELLEXECUTEINFOW seinfo = { sizeof(SHELLEXECUTEINFOW) };
  seinfo.lpVerb = L"openas";
  seinfo.lpFile = urlStr.get();
  seinfo.nShow = SW_SHOWNORMAL;
  if (!ShellExecuteExW(&seinfo)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
nsWindowsShellService::LaunchHTTPHandlerPane()
{
  OPENASINFO info;
  info.pcszFile = L"http";
  info.pcszClass = nullptr;
  info.oaifInFlags = OAIF_FORCE_REGISTRATION |
                     OAIF_URL_PROTOCOL |
                     OAIF_REGISTER_EXT;

  HRESULT hr = SHOpenWithDialog(nullptr, &info);
  if (SUCCEEDED(hr) || (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))) {
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
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

  nsresult rv = LaunchHelper(appHelperPath);
  if (NS_SUCCEEDED(rv) && IsWin8OrLater()) {
    if (aClaimAllTypes) {
      if (IsWin10OrLater()) {
        rv = LaunchModernSettingsDialogDefaultApps();
      } else {
        rv = LaunchControlPanelDefaultsSelectionUI();
      }
      // The above call should never really fail, but just in case
      // fall back to showing the HTTP association screen only.
      if (NS_FAILED(rv)) {
        if (IsWin10OrLater()) {
          rv = InvokeHTTPOpenAsVerb();
        } else {
          rv = LaunchHTTPHandlerPane();
        }
      }
    } else {
      // Windows 10 blocks attempts to load the
      // HTTP Handler association dialog.
      if (IsWin10OrLater()) {
        rv = LaunchModernSettingsDialogDefaultApps();
      } else {
        rv = LaunchHTTPHandlerPane();
      }

      // The above call should never really fail, but just in case
      // fall back to showing control panel for all defaults
      if (NS_FAILED(rv)) {
        rv = LaunchControlPanelDefaultsSelectionUI();
      }
    }
  }

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefs) {
    (void) prefs->SetBoolPref(PREF_CHECKDEFAULTBROWSER, true);
    // Reset the number of times the dialog should be shown
    // before it is silenced.
    (void) prefs->SetIntPref(PREF_DEFAULTBROWSERCHECKCOUNT, 0);
  }

  return rv;
}

static nsresult
WriteBitmap(nsIFile* aFile, imgIContainer* aImage)
{
  nsresult rv;

  RefPtr<SourceSurface> surface =
    aImage->GetFrame(imgIContainer::FRAME_FIRST,
                     imgIContainer::FLAG_SYNC_DECODE);
  NS_ENSURE_TRUE(surface, NS_ERROR_FAILURE);

  // For either of the following formats we want to set the biBitCount member
  // of the BITMAPINFOHEADER struct to 32, below. For that value the bitmap
  // format defines that the A8/X8 WORDs in the bitmap byte stream be ignored
  // for the BI_RGB value we use for the biCompression member.
  MOZ_ASSERT(surface->GetFormat() == SurfaceFormat::B8G8R8A8 ||
             surface->GetFormat() == SurfaceFormat::B8G8R8X8);

  RefPtr<DataSourceSurface> dataSurface = surface->GetDataSurface();
  NS_ENSURE_TRUE(dataSurface, NS_ERROR_FAILURE);

  int32_t width = dataSurface->GetSize().width;
  int32_t height = dataSurface->GetSize().height;
  int32_t bytesPerPixel = 4 * sizeof(uint8_t);
  uint32_t bytesPerRow = bytesPerPixel * width;

  // initialize these bitmap structs which we will later
  // serialize directly to the head of the bitmap file
  BITMAPINFOHEADER bmi;
  bmi.biSize = sizeof(BITMAPINFOHEADER);
  bmi.biWidth = width;
  bmi.biHeight = height;
  bmi.biPlanes = 1;
  bmi.biBitCount = (WORD)bytesPerPixel*8;
  bmi.biCompression = BI_RGB;
  bmi.biSizeImage = bytesPerRow * height;
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

  DataSourceSurface::MappedSurface map;
  if (!dataSurface->Map(DataSourceSurface::MapType::READ, &map)) {
    return NS_ERROR_FAILURE;
  }

  // write the bitmap headers and rgb pixel data to the file
  rv = NS_ERROR_FAILURE;
  if (stream) {
    uint32_t written;
    stream->Write((const char*)&bf, sizeof(BITMAPFILEHEADER), &written);
    if (written == sizeof(BITMAPFILEHEADER)) {
      stream->Write((const char*)&bmi, sizeof(BITMAPINFOHEADER), &written);
      if (written == sizeof(BITMAPINFOHEADER)) {
        // write out the image data backwards because the desktop won't
        // show bitmaps with negative heights for top-to-bottom
        uint32_t i = map.mStride * height;
        do {
          i -= map.mStride;
          stream->Write(((const char*)map.mData) + i, bytesPerRow, &written);
          if (written == bytesPerRow) {
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

  dataSurface->Unmap();

  return rv;
}

NS_IMETHODIMP
nsWindowsShellService::SetDesktopBackground(nsIDOMElement* aElement,
                                            int32_t aPosition)
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
                      (u"desktopBackgroundLeafNameWin",
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
        style.Assign('0');
        tile.Assign('1');
        break;
      case BACKGROUND_CENTER:
        style.Assign('0');
        tile.Assign('0');
        break;
      case BACKGROUND_STRETCH:
        style.Assign('2');
        tile.Assign('0');
        break;
      case BACKGROUND_FILL:
        style.AssignLiteral("10");
        tile.Assign('0');
        break;
      case BACKGROUND_FIT:
        style.Assign('6');
        tile.Assign('0');
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
nsWindowsShellService::OpenApplication(int32_t aApplication)
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

  wchar_t buf[MAX_BUF];
  DWORD type, len = sizeof buf;
  DWORD res = ::RegQueryValueExW(theKey, EmptyString().get(), 0,
                                 &type, (LPBYTE)&buf, &len);

  if (REG_FAILED(res) || !*buf)
    return NS_OK;

  // Close the key we opened.
  ::RegCloseKey(theKey);

  // Find the "open" command
  application.Append('\\');
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
  int32_t end = path.Length();
  int32_t cursor = 0, temp = 0;
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

  BOOL success = ::CreateProcessW(nullptr, (LPWSTR)path.get(), nullptr,
                                  nullptr, FALSE, 0, nullptr,  nullptr,
                                  &si, &pi);
  if (!success)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::GetDesktopBackgroundColor(uint32_t* aColor)
{
  uint32_t color = ::GetSysColor(COLOR_DESKTOP);
  *aColor = (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::SetDesktopBackgroundColor(uint32_t aColor)
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

  wchar_t rgb[12];
  _snwprintf(rgb, 12, L"%u %u %u", r, g, b);

  rv = regKey->WriteStringValue(NS_LITERAL_STRING("Background"),
                                nsDependentString(rgb));
  NS_ENSURE_SUCCESS(rv, rv);

  return regKey->Close();
}

nsWindowsShellService::nsWindowsShellService()
{
}

nsWindowsShellService::~nsWindowsShellService()
{
}

NS_IMETHODIMP
nsWindowsShellService::OpenApplicationWithURI(nsIFile* aApplication,
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
nsWindowsShellService::GetDefaultFeedReader(nsIFile** _retval)
{
  *_retval = nullptr;

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

  nsCOMPtr<nsIFile> defaultReader =
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
