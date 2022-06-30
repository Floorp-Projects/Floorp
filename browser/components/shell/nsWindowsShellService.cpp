/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define UNICODE

#include "nsWindowsShellService.h"

#include "BinaryPath.h"
#include "imgIContainer.h"
#include "imgIRequest.h"
#include "mozilla/RefPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIContent.h"
#include "nsIImageLoadingContent.h"
#include "nsIOutputStream.h"
#include "nsIPrefService.h"
#include "nsIStringBundle.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsShellService.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIWindowsRegKey.h"
#include "nsUnicharUtils.h"
#include "nsIURLFormatter.h"
#include "nsXULAppAPI.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/intl/Localization.h"
#include "WindowsDefaultBrowser.h"
#include "WindowsUserChoice.h"
#include "nsLocalFile.h"
#include "nsIXULAppInfo.h"
#include "nsINIParser.h"
#include "nsNativeAppSupportWin.h"
#include "nsWindowsHelpers.h"

#include <windows.h>
#include <shellapi.h>
#include <propvarutil.h>
#include <propkey.h>

#ifdef __MINGW32__
// MinGW-w64 headers are missing PropVariantToString.
#  include <propsys.h>
PSSTDAPI PropVariantToString(REFPROPVARIANT propvar, PWSTR psz, UINT cch);
#  define UNLEN 256
#else
#  include <Lmcons.h>  // For UNLEN
#endif

#include <comutil.h>
#include <objbase.h>
#include <shlobj.h>
#include <knownfolders.h>
#include "WinUtils.h"
#include "mozilla/widget/WinTaskbar.h"

#include <mbstring.h>

#define PIN_TO_TASKBAR_SHELL_VERB 5386

#undef ACCESS_READ

#ifndef MAX_BUF
#  define MAX_BUF 4096
#endif

#define REG_SUCCEEDED(val) (val == ERROR_SUCCESS)

#define REG_FAILED(val) (val != ERROR_SUCCESS)

#ifdef DEBUG
#  define NS_ENSURE_HRESULT(hres, ret)                    \
    do {                                                  \
      HRESULT result = hres;                              \
      if (MOZ_UNLIKELY(FAILED(result))) {                 \
        mozilla::SmprintfPointer msg = mozilla::Smprintf( \
            "NS_ENSURE_HRESULT(%s, %s) failed with "      \
            "result 0x%" PRIX32,                          \
            #hres, #ret, static_cast<uint32_t>(result));  \
        NS_WARNING(msg.get());                            \
        return ret;                                       \
      }                                                   \
    } while (false)
#else
#  define NS_ENSURE_HRESULT(hres, ret) \
    if (MOZ_UNLIKELY(FAILED(hres))) return ret
#endif

using mozilla::IsWin8OrLater;
using namespace mozilla;
using mozilla::intl::Localization;

struct SysFreeStringDeleter {
  void operator()(BSTR aPtr) { ::SysFreeString(aPtr); }
};
using BStrPtr = mozilla::UniquePtr<OLECHAR, SysFreeStringDeleter>;

NS_IMPL_ISUPPORTS(nsWindowsShellService, nsIToolkitShellService,
                  nsIShellService, nsIWindowsShellService)

static nsresult OpenKeyForReading(HKEY aKeyRoot, const nsAString& aKeyName,
                                  HKEY* aKey) {
  const nsString& flatName = PromiseFlatString(aKeyName);

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

nsresult GetHelperPath(nsAutoString& aPath) {
  nsresult rv;
  nsCOMPtr<nsIProperties> directoryService =
      do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> appHelper;
  rv = directoryService->Get(XRE_EXECUTABLE_FILE, NS_GET_IID(nsIFile),
                             getter_AddRefs(appHelper));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = appHelper->SetNativeLeafName("uninstall"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = appHelper->AppendNative("helper.exe"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = appHelper->GetPath(aPath);

  aPath.Insert(L'"', 0);
  aPath.Append(L'"');
  return rv;
}

nsresult LaunchHelper(nsAutoString& aPath) {
  STARTUPINFOW si = {sizeof(si), 0};
  PROCESS_INFORMATION pi = {0};

  if (!CreateProcessW(nullptr, (LPWSTR)aPath.get(), nullptr, nullptr, FALSE, 0,
                      nullptr, nullptr, &si, &pi)) {
    return NS_ERROR_FAILURE;
  }

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return NS_OK;
}

static bool IsPathDefaultForClass(
    const RefPtr<IApplicationAssociationRegistration>& pAAR, wchar_t* exePath,
    LPCWSTR aClassName) {
  LPWSTR registeredApp;
  bool isProtocol = *aClassName != L'.';
  ASSOCIATIONTYPE queryType = isProtocol ? AT_URLPROTOCOL : AT_FILEEXTENSION;
  HRESULT hr = pAAR->QueryCurrentDefault(aClassName, queryType, AL_EFFECTIVE,
                                         &registeredApp);
  if (FAILED(hr)) {
    return false;
  }

  nsAutoString regAppName(registeredApp);
  CoTaskMemFree(registeredApp);

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

  nsAutoString pathFromReg(cmdFromReg);
  nsLocalFile::CleanupCmdHandlerPath(pathFromReg);

  return _wcsicmp(exePath, pathFromReg.Data()) == 0;
}

NS_IMETHODIMP
nsWindowsShellService::IsDefaultBrowser(bool aForAllTypes,
                                        bool* aIsDefaultBrowser) {
  *aIsDefaultBrowser = false;

  RefPtr<IApplicationAssociationRegistration> pAAR;
  HRESULT hr = CoCreateInstance(
      CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_INPROC,
      IID_IApplicationAssociationRegistration, getter_AddRefs(pAAR));
  if (FAILED(hr)) {
    return NS_OK;
  }

  wchar_t exePath[MAXPATHLEN] = L"";
  nsresult rv = BinaryPath::GetLong(exePath);

  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  *aIsDefaultBrowser = IsPathDefaultForClass(pAAR, exePath, L"http");
  if (*aIsDefaultBrowser && aForAllTypes) {
    *aIsDefaultBrowser = IsPathDefaultForClass(pAAR, exePath, L".html");
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::IsDefaultHandlerFor(
    const nsAString& aFileExtensionOrProtocol, bool* aIsDefaultHandlerFor) {
  *aIsDefaultHandlerFor = false;

  RefPtr<IApplicationAssociationRegistration> pAAR;
  HRESULT hr = CoCreateInstance(
      CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_INPROC,
      IID_IApplicationAssociationRegistration, getter_AddRefs(pAAR));
  if (FAILED(hr)) {
    return NS_OK;
  }

  wchar_t exePath[MAXPATHLEN] = L"";
  nsresult rv = BinaryPath::GetLong(exePath);

  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  const nsString& flatClass = PromiseFlatString(aFileExtensionOrProtocol);

  *aIsDefaultHandlerFor = IsPathDefaultForClass(pAAR, exePath, flatClass.get());
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::QueryCurrentDefaultHandlerFor(
    const nsAString& aFileExtensionOrProtocol, nsAString& aResult) {
  aResult.Truncate();

  RefPtr<IApplicationAssociationRegistration> pAAR;
  HRESULT hr = CoCreateInstance(
      CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_INPROC,
      IID_IApplicationAssociationRegistration, getter_AddRefs(pAAR));
  if (FAILED(hr)) {
    return NS_OK;
  }

  const nsString& flatClass = PromiseFlatString(aFileExtensionOrProtocol);

  LPWSTR registeredApp;
  bool isProtocol = flatClass.First() != L'.';
  ASSOCIATIONTYPE queryType = isProtocol ? AT_URLPROTOCOL : AT_FILEEXTENSION;
  hr = pAAR->QueryCurrentDefault(flatClass.get(), queryType, AL_EFFECTIVE,
                                 &registeredApp);
  if (hr == HRESULT_FROM_WIN32(ERROR_NO_ASSOCIATION)) {
    return NS_OK;
  }
  NS_ENSURE_HRESULT(hr, NS_ERROR_FAILURE);

  aResult = registeredApp;
  CoTaskMemFree(registeredApp);

  return NS_OK;
}

nsresult nsWindowsShellService::LaunchControlPanelDefaultsSelectionUI() {
  IApplicationAssociationRegistrationUI* pAARUI;
  HRESULT hr = CoCreateInstance(
      CLSID_ApplicationAssociationRegistrationUI, NULL, CLSCTX_INPROC,
      IID_IApplicationAssociationRegistrationUI, (void**)&pAARUI);
  if (SUCCEEDED(hr)) {
    mozilla::UniquePtr<wchar_t[]> appRegName;
    GetAppRegName(appRegName);
    hr = pAARUI->LaunchAdvancedAssociationUI(appRegName.get());
    pAARUI->Release();
  }
  return SUCCEEDED(hr) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsWindowsShellService::LaunchControlPanelDefaultPrograms() {
  return ::LaunchControlPanelDefaultPrograms() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsShellService::CheckAllProgIDsExist(bool* aResult) {
  *aResult = false;
  nsAutoString aumid;
  if (!mozilla::widget::WinTaskbar::GetAppUserModelID(aumid)) {
    return NS_OK;
  }
  *aResult =
      CheckProgIDExists(FormatProgID(L"FirefoxURL", aumid.get()).get()) &&
      CheckProgIDExists(FormatProgID(L"FirefoxHTML", aumid.get()).get()) &&
      CheckProgIDExists(FormatProgID(L"FirefoxPDF", aumid.get()).get());
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::CheckBrowserUserChoiceHashes(bool* aResult) {
  *aResult = ::CheckBrowserUserChoiceHashes();
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::CanSetDefaultBrowserUserChoice(bool* aResult) {
  *aResult = false;
// If the WDBA is not available, this could never succeed.
#ifdef MOZ_DEFAULT_BROWSER_AGENT
  bool progIDsExist = false;
  bool hashOk = false;
  *aResult = NS_SUCCEEDED(CheckAllProgIDsExist(&progIDsExist)) &&
             progIDsExist &&
             NS_SUCCEEDED(CheckBrowserUserChoiceHashes(&hashOk)) && hashOk;
#endif
  return NS_OK;
}

nsresult nsWindowsShellService::LaunchModernSettingsDialogDefaultApps() {
  return ::LaunchModernSettingsDialogDefaultApps() ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsWindowsShellService::InvokeHTTPOpenAsVerb() {
  nsCOMPtr<nsIURLFormatter> formatter(
      do_GetService("@mozilla.org/toolkit/URLFormatterService;1"));
  if (!formatter) {
    return NS_ERROR_UNEXPECTED;
  }

  nsString urlStr;
  nsresult rv = formatter->FormatURLPref(u"app.support.baseURL"_ns, urlStr);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!StringBeginsWith(urlStr, u"https://"_ns)) {
    return NS_ERROR_FAILURE;
  }
  urlStr.AppendLiteral("win10-default-browser");

  SHELLEXECUTEINFOW seinfo = {sizeof(SHELLEXECUTEINFOW)};
  seinfo.lpVerb = L"openas";
  seinfo.lpFile = urlStr.get();
  seinfo.nShow = SW_SHOWNORMAL;
  if (!ShellExecuteExW(&seinfo)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult nsWindowsShellService::LaunchHTTPHandlerPane() {
  OPENASINFO info;
  info.pcszFile = L"http";
  info.pcszClass = nullptr;
  info.oaifInFlags =
      OAIF_FORCE_REGISTRATION | OAIF_URL_PROTOCOL | OAIF_REGISTER_EXT;

  HRESULT hr = SHOpenWithDialog(nullptr, &info);
  if (SUCCEEDED(hr) || (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))) {
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowsShellService::SetDefaultBrowser(bool aClaimAllTypes,
                                         bool aForAllUsers) {
  // If running from within a package, don't attempt to set default with
  // the helper, as it will not work and will only confuse our package's
  // virtualized registry.
  nsresult rv = NS_OK;
  if (!widget::WinUtils::HasPackageIdentity()) {
    nsAutoString appHelperPath;
    if (NS_FAILED(GetHelperPath(appHelperPath))) return NS_ERROR_FAILURE;

    if (aForAllUsers) {
      appHelperPath.AppendLiteral(" /SetAsDefaultAppGlobal");
    } else {
      appHelperPath.AppendLiteral(" /SetAsDefaultAppUser");
    }

    rv = LaunchHelper(appHelperPath);
  }

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
    (void)prefs->SetBoolPref(PREF_CHECKDEFAULTBROWSER, true);
    // Reset the number of times the dialog should be shown
    // before it is silenced.
    (void)prefs->SetIntPref(PREF_DEFAULTBROWSERCHECKCOUNT, 0);
  }

  return rv;
}

static nsresult WriteBitmap(nsIFile* aFile, imgIContainer* aImage) {
  nsresult rv;

  RefPtr<gfx::SourceSurface> surface = aImage->GetFrame(
      imgIContainer::FRAME_FIRST, imgIContainer::FLAG_SYNC_DECODE);
  NS_ENSURE_TRUE(surface, NS_ERROR_FAILURE);

  // For either of the following formats we want to set the biBitCount member
  // of the BITMAPINFOHEADER struct to 32, below. For that value the bitmap
  // format defines that the A8/X8 WORDs in the bitmap byte stream be ignored
  // for the BI_RGB value we use for the biCompression member.
  MOZ_ASSERT(surface->GetFormat() == gfx::SurfaceFormat::B8G8R8A8 ||
             surface->GetFormat() == gfx::SurfaceFormat::B8G8R8X8);

  RefPtr<gfx::DataSourceSurface> dataSurface = surface->GetDataSurface();
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
  bmi.biBitCount = (WORD)bytesPerPixel * 8;
  bmi.biCompression = BI_RGB;
  bmi.biSizeImage = bytesPerRow * height;
  bmi.biXPelsPerMeter = 0;
  bmi.biYPelsPerMeter = 0;
  bmi.biClrUsed = 0;
  bmi.biClrImportant = 0;

  BITMAPFILEHEADER bf;
  bf.bfType = 0x4D42;  // 'BM'
  bf.bfReserved1 = 0;
  bf.bfReserved2 = 0;
  bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  bf.bfSize = bf.bfOffBits + bmi.biSizeImage;

  // get a file output stream
  nsCOMPtr<nsIOutputStream> stream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(stream), aFile);
  NS_ENSURE_SUCCESS(rv, rv);

  gfx::DataSourceSurface::MappedSurface map;
  if (!dataSurface->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
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
nsWindowsShellService::SetDesktopBackground(dom::Element* aElement,
                                            int32_t aPosition,
                                            const nsACString& aImageName) {
  if (!aElement || !aElement->IsHTMLElement(nsGkAtoms::img)) {
    // XXX write background loading stuff!
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv;
  nsCOMPtr<nsIImageLoadingContent> imageContent =
      do_QueryInterface(aElement, &rv);
  if (!imageContent) return rv;

  // get the image container
  nsCOMPtr<imgIRequest> request;
  rv = imageContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                                getter_AddRefs(request));
  if (!request) return rv;

  nsCOMPtr<imgIContainer> container;
  rv = request->GetImage(getter_AddRefs(container));
  if (!container) return NS_ERROR_FAILURE;

  // get the file name from localized strings
  nsCOMPtr<nsIStringBundleService> bundleService(
      do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundle> shellBundle;
  rv = bundleService->CreateBundle(SHELLSERVICE_PROPERTIES,
                                   getter_AddRefs(shellBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  // e.g. "Desktop Background.bmp"
  nsAutoString fileLeafName;
  rv = shellBundle->GetStringFromName("desktopBackgroundLeafNameWin",
                                      fileLeafName);
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

  // write the bitmap to a file in the profile directory.
  // We have to write old bitmap format for Windows 7 wallpapar support.
  rv = WriteBitmap(file, container);

  // if the file was written successfully, set it as the system wallpaper
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWindowsRegKey> regKey =
        do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = regKey->Create(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                        u"Control Panel\\Desktop"_ns,
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
      case BACKGROUND_SPAN:
        style.AssignLiteral("22");
        tile.Assign('0');
        break;
    }

    rv = regKey->WriteStringValue(u"TileWallpaper"_ns, tile);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = regKey->WriteStringValue(u"WallpaperStyle"_ns, style);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = regKey->Close();
    NS_ENSURE_SUCCESS(rv, rv);

    ::SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)path.get(),
                            SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
  }
  return rv;
}

NS_IMETHODIMP
nsWindowsShellService::GetDesktopBackgroundColor(uint32_t* aColor) {
  uint32_t color = ::GetSysColor(COLOR_DESKTOP);
  *aColor =
      (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::SetDesktopBackgroundColor(uint32_t aColor) {
  int aParameters[2] = {COLOR_BACKGROUND, COLOR_DESKTOP};
  BYTE r = (aColor >> 16);
  BYTE g = (aColor << 16) >> 24;
  BYTE b = (aColor << 24) >> 24;
  COLORREF colors[2] = {RGB(r, g, b), RGB(r, g, b)};

  ::SetSysColors(sizeof(aParameters) / sizeof(int), aParameters, colors);

  nsresult rv;
  nsCOMPtr<nsIWindowsRegKey> regKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = regKey->Create(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                      u"Control Panel\\Colors"_ns,
                      nsIWindowsRegKey::ACCESS_SET_VALUE);
  NS_ENSURE_SUCCESS(rv, rv);

  wchar_t rgb[12];
  _snwprintf(rgb, 12, L"%u %u %u", r, g, b);

  rv = regKey->WriteStringValue(u"Background"_ns, nsDependentString(rgb));
  NS_ENSURE_SUCCESS(rv, rv);

  return regKey->Close();
}

/*
 * Writes information about a shortcut to a shortcuts log in
 * %PROGRAMDATA%\Mozilla-1de4eec8-1241-4177-a864-e594e8d1fb38.
 * (This is the same directory used for update staging.)
 * For more on the shortcuts log format and purpose, consult
 * /toolkit/mozapps/installer/windows/nsis/common.nsh.
 *
 * The shortcuts log created or appended here is named after
 * the currently running application and current user SID.
 * For example: Firefox_$SID_shortcuts.ini.
 *
 * If it does not exist, it will be created. If it exists
 * and a matching shortcut named already exists in the file,
 * a new one will not be appended.
 */
static nsresult WriteShortcutToLog(KNOWNFOLDERID aFolderId,
                                   const nsAString& aShortcutName) {
  // the section inside the shortcuts log
  nsAutoCString section;
  // the shortcuts log wants "Programs" shortcuts in its "STARTMENU" section
  if (aFolderId == FOLDERID_CommonPrograms || aFolderId == FOLDERID_Programs) {
    section.Assign("STARTMENU");
  } else if (aFolderId == FOLDERID_PublicDesktop ||
             aFolderId == FOLDERID_Desktop) {
    section.Assign("DESKTOP");
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsCOMPtr<nsIProperties> directoryService =
      do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> updRoot, parent, shortcutsLog;
  rv = directoryService->Get(XRE_UPDATE_ROOT_DIR, NS_GET_IID(nsIFile),
                             getter_AddRefs(updRoot));
  rv = updRoot->GetParent(getter_AddRefs(parent));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = parent->GetParent(getter_AddRefs(shortcutsLog));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString appName;
  nsCOMPtr<nsIXULAppInfo> appInfo =
      do_GetService("@mozilla.org/xre/app-info;1");
  rv = appInfo->GetName(appName);
  NS_ENSURE_SUCCESS(rv, rv);

  auto userSid = GetCurrentUserStringSid();
  if (!userSid) {
    return NS_ERROR_FILE_NOT_FOUND;
  }

  nsAutoString filename;
  filename.AppendPrintf("%s_%ls_shortcuts.ini", appName.get(), userSid.get());
  rv = shortcutsLog->Append(filename);
  NS_ENSURE_SUCCESS(rv, rv);

  nsINIParser parser;
  bool fileExists = false;
  bool shortcutsLogEntryExists = false;
  nsAutoCString keyName, shortcutName;
  shortcutName = NS_ConvertUTF16toUTF8(aShortcutName);

  shortcutsLog->IsFile(&fileExists);
  // if the shortcuts log exists, find either an existing matching
  // entry, or the next available shortcut index
  if (fileExists) {
    rv = parser.Init(shortcutsLog);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCString iniShortcut;
    // surely we'll never need more than 10 shortcuts in one section...
    for (int i = 0; i < 10; i++) {
      keyName.AssignLiteral("Shortcut");
      keyName.AppendInt(i);
      rv = parser.GetString(section.get(), keyName.get(), iniShortcut);
      if (NS_FAILED(rv)) {
        // we found an unused index
        break;
      } else if (iniShortcut.Equals(shortcutName)) {
        shortcutsLogEntryExists = true;
      }
    }
    // otherwise, this is safe to use
  } else {
    keyName.AssignLiteral("Shortcut0");
  }

  if (!shortcutsLogEntryExists) {
    parser.SetString(section.get(), keyName.get(), shortcutName.get());
    rv = parser.WriteToFile(shortcutsLog);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

static nsresult CreateShortcutImpl(
    nsIFile* aBinary, const nsTArray<nsString>& aArguments,
    const nsAString& aDescription, nsIFile* aIconFile, uint16_t aIconIndex,
    const nsAString& aAppUserModelId, KNOWNFOLDERID aShortcutFolder,
    const nsAString& aShortcutName, nsAString& aShortcutPath) {
  NS_ENSURE_ARG(aBinary);
  NS_ENSURE_ARG(aIconFile);

  nsresult rv = WriteShortcutToLog(aShortcutFolder, aShortcutName);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<IShellLinkW> link;
  HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, getter_AddRefs(link));
  NS_ENSURE_HRESULT(hr, NS_ERROR_FAILURE);

  nsString path(aBinary->NativePath());
  link->SetPath(path.get());

  wchar_t workingDir[MAX_PATH + 1];
  wcscpy_s(workingDir, MAX_PATH + 1, aBinary->NativePath().get());
  PathRemoveFileSpecW(workingDir);
  link->SetWorkingDirectory(workingDir);

  if (!aDescription.IsEmpty()) {
    link->SetDescription(PromiseFlatString(aDescription).get());
  }

  // TODO: Properly escape quotes in the string, see bug 1604287.
  nsString arguments;
  for (auto& arg : aArguments) {
    arguments.AppendPrintf("\"%S\" ", static_cast<const wchar_t*>(arg.get()));
  }

  link->SetArguments(arguments.get());

  if (aIconFile) {
    nsString icon(aIconFile->NativePath());
    link->SetIconLocation(icon.get(), aIconIndex);
  }

  if (!aAppUserModelId.IsEmpty()) {
    RefPtr<IPropertyStore> propStore;
    hr = link->QueryInterface(IID_IPropertyStore, getter_AddRefs(propStore));
    NS_ENSURE_HRESULT(hr, NS_ERROR_FAILURE);

    PROPVARIANT pv;
    if (FAILED(InitPropVariantFromString(
            PromiseFlatString(aAppUserModelId).get(), &pv))) {
      return NS_ERROR_FAILURE;
    }

    hr = propStore->SetValue(PKEY_AppUserModel_ID, pv);
    PropVariantClear(&pv);
    NS_ENSURE_HRESULT(hr, NS_ERROR_FAILURE);

    hr = propStore->Commit();
    NS_ENSURE_HRESULT(hr, NS_ERROR_FAILURE);
  }

  RefPtr<IPersistFile> persist;
  hr = link->QueryInterface(IID_IPersistFile, getter_AddRefs(persist));
  NS_ENSURE_HRESULT(hr, NS_ERROR_FAILURE);

  nsCOMPtr<nsIProperties> directoryService =
      do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> shortcutFile;
  if (aShortcutFolder == FOLDERID_Programs) {
    rv = directoryService->Get(NS_WIN_PROGRAMS_DIR, NS_GET_IID(nsIFile),
                               getter_AddRefs(shortcutFile));
  } else if (aShortcutFolder == FOLDERID_Desktop) {
    rv = directoryService->Get(NS_OS_DESKTOP_DIR, NS_GET_IID(nsIFile),
                               getter_AddRefs(shortcutFile));
  } else {
    return NS_ERROR_FILE_NOT_FOUND;
  }
  if (NS_FAILED(rv)) {
    return NS_ERROR_FILE_NOT_FOUND;
  }
  shortcutFile->Append(aShortcutName);

  nsString target(shortcutFile->NativePath());
  hr = persist->Save(target.get(), TRUE);
  NS_ENSURE_HRESULT(hr, NS_ERROR_FAILURE);

  aShortcutPath.Assign(target);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::CreateShortcut(
    nsIFile* aBinary, const nsTArray<nsString>& aArguments,
    const nsAString& aDescription, nsIFile* aIconFile, uint16_t aIconIndex,
    const nsAString& aAppUserModelId, const nsAString& aShortcutFolder,
    const nsAString& aShortcutName, nsAString& aShortcutPath) {
  // In an ideal world we'd probably send along nsIFile pointers
  // here, but it's easier to determine the needed shortcuts log
  // entry with a KNOWNFOLDERID - so we pass this along instead
  // and let CreateShortcutImpl take care of converting it to
  // an nsIFile.
  KNOWNFOLDERID folderId;
  if (aShortcutFolder.Equals(L"Programs")) {
    folderId = FOLDERID_Programs;
  } else if (aShortcutFolder.Equals(L"Desktop")) {
    folderId = FOLDERID_Desktop;
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  return CreateShortcutImpl(aBinary, aArguments, aDescription, aIconFile,
                            aIconIndex, aAppUserModelId, folderId,
                            aShortcutName, aShortcutPath);
}

// Constructs a path to an installer-created shortcut, under a directory
// specified by a CSIDL.
// Earlier versions of this code generated the shortcut name themselves.
// This is no longer possible because the Private Browsing shortcut name
// is localized, the Localization class does not work off main thread,
// and this code is called off main thread in some contexts -- therefore
// it must be passed through by any callers.
static nsresult GetShortcutPath(int aCSIDL, const nsAString& aShortcutName,
                                /* out */ nsAutoString& aPath) {
  wchar_t folderPath[MAX_PATH] = {};
  HRESULT hr = SHGetFolderPathW(nullptr, aCSIDL, nullptr, SHGFP_TYPE_CURRENT,
                                folderPath);
  if (NS_WARN_IF(FAILED(hr))) {
    return NS_ERROR_FAILURE;
  }

  aPath.Assign(folderPath);
  if (NS_WARN_IF(aPath.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }
  if (aPath[aPath.Length() - 1] != '\\') {
    aPath.AppendLiteral("\\");
  }
  aPath.Append(aShortcutName);

  return NS_OK;
}

// Check if the instaler-created shortcut in the given location matches,
// if so output its path
//
// NOTE: DO NOT USE if a false negative (mismatch) is unacceptable.
// aExePath is compared directly to the path retrieved from the shortcut.
// Due to the presence of symlinks or other filesystem issues, it's possible
// for different paths to refer to the same file, which would cause the check
// to fail.
// This should rarely be an issue as we are most likely to be run from a path
// written by the installer (shortcut, association, launch from installer),
// which also wrote the shortcuts. But it is possible.
//
// aCSIDL   the CSIDL of the directory containing the shortcut to check.
// aAUMID   the AUMID to check for
// aExePath the target exe path to check for, should be a long path where
//          possible
// aShortcutName the filename portion (excluding extension) of the shortcut
//               to look for.
// aShortcutPath outparam, set to matching shortcut path if NS_OK is returned.
//
// Returns
//   NS_ERROR_FAILURE on errors before the shortcut was loaded
//   NS_ERROR_FILE_NOT_FOUND if the shortcut doesn't exist
//   NS_ERROR_FILE_ALREADY_EXISTS if the shortcut exists but doesn't match the
//                                current app
//   NS_OK if the shortcut matches
static nsresult GetMatchingShortcut(int aCSIDL, const nsAString& aAUMID,
                                    const wchar_t aExePath[MAXPATHLEN],
                                    const nsAString& aShortcutName,
                                    /* out */ nsAutoString& aShortcutPath) {
  nsresult result = NS_ERROR_FAILURE;

  nsAutoString path;
  nsresult rv = GetShortcutPath(aCSIDL, aShortcutName, path);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return result;
  }

  // Create a shell link object for loading the shortcut
  RefPtr<IShellLinkW> link;
  HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, getter_AddRefs(link));
  if (NS_WARN_IF(FAILED(hr))) {
    return result;
  }

  // Load
  RefPtr<IPersistFile> persist;
  hr = link->QueryInterface(IID_IPersistFile, getter_AddRefs(persist));
  if (NS_WARN_IF(FAILED(hr))) {
    return result;
  }

  hr = persist->Load(path.get(), STGM_READ);
  if (FAILED(hr)) {
    if (NS_WARN_IF(hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))) {
      // empty branch, result unchanged but warning issued
    } else {
      result = NS_ERROR_FILE_NOT_FOUND;
    }
    return result;
  }
  result = NS_ERROR_FILE_ALREADY_EXISTS;

  // Check the AUMID
  RefPtr<IPropertyStore> propStore;
  hr = link->QueryInterface(IID_IPropertyStore, getter_AddRefs(propStore));
  if (NS_WARN_IF(FAILED(hr))) {
    return result;
  }

  PROPVARIANT pv;
  hr = propStore->GetValue(PKEY_AppUserModel_ID, &pv);
  if (NS_WARN_IF(FAILED(hr))) {
    return result;
  }

  wchar_t storedAUMID[MAX_PATH];
  hr = PropVariantToString(pv, storedAUMID, MAX_PATH);
  PropVariantClear(&pv);
  if (NS_WARN_IF(FAILED(hr))) {
    return result;
  }

  if (!aAUMID.Equals(storedAUMID)) {
    return result;
  }

  // Check the exe path
  static_assert(MAXPATHLEN == MAX_PATH);
  wchar_t storedExePath[MAX_PATH] = {};
  // With no flags GetPath gets a long path
  hr = link->GetPath(storedExePath, ArrayLength(storedExePath), nullptr, 0);
  if (FAILED(hr) || hr == S_FALSE) {
    return result;
  }
  // Case insensitive path comparison
  if (wcsnicmp(storedExePath, aExePath, MAXPATHLEN) != 0) {
    return result;
  }

  // Success, report the shortcut path
  aShortcutPath.Assign(path);
  result = NS_OK;

  return result;
}

static nsresult FindMatchingShortcut(const nsAString& aAppUserModelId,
                                     const nsAString& aShortcutName,
                                     nsAutoString& aShortcutPath) {
  wchar_t exePath[MAXPATHLEN] = {};
  if (NS_WARN_IF(NS_FAILED(BinaryPath::GetLong(exePath)))) {
    return NS_ERROR_FAILURE;
  }

  int shortcutCSIDLs[] = {CSIDL_COMMON_PROGRAMS, CSIDL_PROGRAMS,
                          CSIDL_COMMON_DESKTOPDIRECTORY,
                          CSIDL_DESKTOPDIRECTORY};
  for (int shortcutCSIDL : shortcutCSIDLs) {
    // GetMatchingShortcut may fail when the exe path doesn't match, even
    // if it refers to the same file. This should be rare, and the worst
    // outcome would be failure to pin, so the risk is acceptable.
    nsresult rv = GetMatchingShortcut(shortcutCSIDL, aAppUserModelId, exePath,
                                      aShortcutName, aShortcutPath);
    if (NS_SUCCEEDED(rv)) {
      return NS_OK;
    }
  }

  return NS_ERROR_FILE_NOT_FOUND;
}

NS_IMETHODIMP
nsWindowsShellService::HasMatchingShortcut(const nsAString& aAppUserModelId,
                                           const bool aPrivateBrowsing,
                                           bool* aHasMatch) {
  // NOTE: In the installer, non-private shortcuts are named
  // "${BrandShortName}.lnk". This is set from MOZ_APP_DISPLAYNAME in
  // defines.nsi.in. (Except in dev edition where it's explicitly set to
  // "Firefox Developer Edition" in branding.nsi, which matches
  // MOZ_APP_DISPLAYNAME in aurora/configure.sh.)
  //
  // If this changes, we could expand this to check shortcuts_log.ini,
  // which records the name of the shortcuts as created by the installer.
  //
  // Private shortcuts are not created by the installer (they're created
  // upon user request, ultimately by CreateShortcutImpl, and recorded in
  // a separate shortcuts log. As with non-private shortcuts they have a known
  // name - so there's no need to look through logs to find them.
  nsAutoString shortcutName;
  if (aPrivateBrowsing) {
    nsTArray<nsCString> resIds = {
        "branding/brand.ftl"_ns,
        "browser/browser.ftl"_ns,
    };
    RefPtr<Localization> l10n = Localization::Create(resIds, true);
    nsAutoCString pbStr;
    IgnoredErrorResult rv;
    l10n->FormatValueSync("private-browsing-shortcut-text"_ns, {}, pbStr, rv);
    shortcutName.Append(NS_ConvertUTF8toUTF16(pbStr));
    shortcutName.AppendLiteral(".lnk");
  } else {
    shortcutName.AppendLiteral(MOZ_APP_DISPLAYNAME ".lnk");
  }

  nsAutoString shortcutPath;
  nsresult rv =
      FindMatchingShortcut(aAppUserModelId, shortcutName, shortcutPath);
  if (SUCCEEDED(rv)) {
    *aHasMatch = true;
  } else {
    *aHasMatch = false;
  }

  return NS_OK;
}

static nsresult PinCurrentAppToTaskbarWin7(bool aCheckOnly,
                                           nsAutoString aShortcutPath) {
  nsModuleHandle shellInst(LoadLibraryW(L"shell32.dll"));

  RefPtr<IShellWindows> shellWindows;
  HRESULT hr =
      ::CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER,
                         IID_IShellWindows, getter_AddRefs(shellWindows));
  if (FAILED(hr)) return NS_ERROR_FAILURE;

  // 1. Find the shell view for the desktop.
  _variant_t loc(int(CSIDL_DESKTOP));
  _variant_t empty;
  long hwnd;
  RefPtr<IDispatch> dispDesktop;
  hr = shellWindows->FindWindowSW(&loc, &empty, SWC_DESKTOP, &hwnd,
                                  SWFO_NEEDDISPATCH,
                                  getter_AddRefs(dispDesktop));
  if (FAILED(hr) || hr == S_FALSE) return NS_ERROR_FAILURE;

  RefPtr<IServiceProvider> servProv;
  hr = dispDesktop->QueryInterface(IID_IServiceProvider,
                                   getter_AddRefs(servProv));
  if (FAILED(hr)) return NS_ERROR_FAILURE;

  RefPtr<IShellBrowser> browser;
  hr = servProv->QueryService(SID_STopLevelBrowser, IID_IShellBrowser,
                              getter_AddRefs(browser));
  if (FAILED(hr)) return NS_ERROR_FAILURE;

  RefPtr<IShellView> activeShellView;
  hr = browser->QueryActiveShellView(getter_AddRefs(activeShellView));
  if (FAILED(hr)) return NS_ERROR_FAILURE;

  // 2. Get the automation object for the desktop.
  RefPtr<IDispatch> dispView;
  hr = activeShellView->GetItemObject(SVGIO_BACKGROUND, IID_IDispatch,
                                      getter_AddRefs(dispView));
  if (FAILED(hr)) return NS_ERROR_FAILURE;

  RefPtr<IShellFolderViewDual> folderView;
  hr = dispView->QueryInterface(IID_IShellFolderViewDual,
                                getter_AddRefs(folderView));
  if (FAILED(hr)) return NS_ERROR_FAILURE;

  // 3. Get the interface to IShellDispatch
  RefPtr<IDispatch> dispShell;
  hr = folderView->get_Application(getter_AddRefs(dispShell));
  if (FAILED(hr)) return NS_ERROR_FAILURE;

  RefPtr<IShellDispatch2> shellDisp;
  hr =
      dispShell->QueryInterface(IID_IShellDispatch2, getter_AddRefs(shellDisp));
  if (FAILED(hr)) return NS_ERROR_FAILURE;

  wchar_t shortcutDir[MAX_PATH + 1];
  wcscpy_s(shortcutDir, MAX_PATH + 1, aShortcutPath.get());
  if (!PathRemoveFileSpecW(shortcutDir)) return NS_ERROR_FAILURE;

  VARIANT dir;
  dir.vt = VT_BSTR;
  BStrPtr bstrShortcutDir = BStrPtr(SysAllocString(shortcutDir));
  if (bstrShortcutDir.get() == NULL) return NS_ERROR_FAILURE;
  dir.bstrVal = bstrShortcutDir.get();

  RefPtr<Folder> folder;
  hr = shellDisp->NameSpace(dir, getter_AddRefs(folder));
  if (FAILED(hr)) return NS_ERROR_FAILURE;

  wchar_t linkName[MAX_PATH + 1];
  wcscpy_s(linkName, MAX_PATH + 1, aShortcutPath.get());
  PathStripPathW(linkName);
  BStrPtr bstrLinkName = BStrPtr(SysAllocString(linkName));
  if (bstrLinkName.get() == NULL) return NS_ERROR_FAILURE;

  RefPtr<FolderItem> folderItem;
  hr = folder->ParseName(bstrLinkName.get(), getter_AddRefs(folderItem));
  if (FAILED(hr)) return NS_ERROR_FAILURE;

  RefPtr<FolderItemVerbs> verbs;
  hr = folderItem->Verbs(getter_AddRefs(verbs));
  if (FAILED(hr)) return NS_ERROR_FAILURE;

  long count;
  hr = verbs->get_Count(&count);
  if (FAILED(hr)) return NS_ERROR_FAILURE;

  WCHAR verbName[100];
  if (!LoadStringW(shellInst.get(), PIN_TO_TASKBAR_SHELL_VERB, verbName,
                   ARRAYSIZE(verbName))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  VARIANT v;
  v.vt = VT_I4;
  BStrPtr name;
  for (long i = 0; i < count; ++i) {
    RefPtr<FolderItemVerb> fiVerb;
    v.lVal = i;
    hr = verbs->Item(v, getter_AddRefs(fiVerb));
    if (FAILED(hr)) {
      continue;
    }

    BSTR tmpName;
    hr = fiVerb->get_Name(&tmpName);
    if (FAILED(hr)) {
      continue;
    }
    name = BStrPtr(tmpName);
    if (!wcscmp((WCHAR*)name.get(), verbName)) {
      if (aCheckOnly) {
        // we've done as much as we can without actually
        // changing anything
        return NS_OK;
      }

      hr = fiVerb->DoIt();
      if (SUCCEEDED(hr)) {
        return NS_OK;
      }
    }
  }

  // if we didn't return in the block above, something failed
  return NS_ERROR_FAILURE;
}

static nsresult PinCurrentAppToTaskbarWin10(bool aCheckOnly,
                                            nsAutoString aShortcutPath) {
  // This enum is likely only used for Windows telemetry, INT_MAX is chosen to
  // avoid confusion with existing uses.
  enum PINNEDLISTMODIFYCALLER { PLMC_INT_MAX = INT_MAX };

  // The types below, and the idea of using IPinnedList3::Modify,
  // are thanks to Gee Law <https://geelaw.blog/entries/msedge-pins/>
  static constexpr GUID CLSID_TaskbandPin = {
      0x90aa3a4e,
      0x1cba,
      0x4233,
      {0xb8, 0xbb, 0x53, 0x57, 0x73, 0xd4, 0x84, 0x49}};

  static constexpr GUID IID_IPinnedList3 = {
      0x0dd79ae2,
      0xd156,
      0x45d4,
      {0x9e, 0xeb, 0x3b, 0x54, 0x97, 0x69, 0xe9, 0x40}};

  struct IPinnedList3Vtbl;
  struct IPinnedList3 {
    IPinnedList3Vtbl* vtbl;
  };

  typedef ULONG STDMETHODCALLTYPE ReleaseFunc(IPinnedList3 * that);
  typedef HRESULT STDMETHODCALLTYPE ModifyFunc(
      IPinnedList3 * that, PCIDLIST_ABSOLUTE unpin, PCIDLIST_ABSOLUTE pin,
      PINNEDLISTMODIFYCALLER caller);

  struct IPinnedList3Vtbl {
    void* QueryInterface;  // 0
    void* AddRef;          // 1
    ReleaseFunc* Release;  // 2
    void* Other[13];       // 3-15
    ModifyFunc* Modify;    // 16
  };

  struct ILFreeDeleter {
    void operator()(LPITEMIDLIST aPtr) {
      if (aPtr) {
        ILFree(aPtr);
      }
    }
  };

  mozilla::UniquePtr<__unaligned ITEMIDLIST, ILFreeDeleter> path(
      ILCreateFromPathW(aShortcutPath.get()));
  if (NS_WARN_IF(!path)) {
    return NS_ERROR_FAILURE;
  }

  IPinnedList3* pinnedList = nullptr;
  HRESULT hr =
      CoCreateInstance(CLSID_TaskbandPin, nullptr, CLSCTX_INPROC_SERVER,
                       IID_IPinnedList3, (void**)&pinnedList);
  if (FAILED(hr) || !pinnedList) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!aCheckOnly) {
    hr =
        pinnedList->vtbl->Modify(pinnedList, nullptr, path.get(), PLMC_INT_MAX);
  }

  pinnedList->vtbl->Release(pinnedList);

  if (FAILED(hr)) {
    return NS_ERROR_FAILURE;
  } else {
    return NS_OK;
  }
}

static nsresult PinCurrentAppToTaskbarImpl(bool aCheckOnly,
                                           bool aPrivateBrowsing,
                                           const nsAString& aAppUserModelId,
                                           const nsAString& aShortcutName) {
  MOZ_DIAGNOSTIC_ASSERT(
      !NS_IsMainThread(),
      "PinCurrentAppToTaskbarImpl should be called off main thread only");

  nsAutoString shortcutPath;
  nsresult rv =
      FindMatchingShortcut(aAppUserModelId, aShortcutName, shortcutPath);
  if (NS_FAILED(rv)) {
    shortcutPath.Truncate();
  }
  if (shortcutPath.IsEmpty()) {
    if (aCheckOnly) {
      // Later checks rely on a shortcut already existing.
      // We don't want to create a shortcut in check only mode
      // so the best we can do is assume those parts will work.
      return NS_OK;
    }

    nsTArray<nsString> arguments;

    if (aPrivateBrowsing) {
      nsAutoString arg;
      arg.AssignLiteral("-private-window");
      arguments.AppendElement(arg);
    }

    nsAutoString linkName(aShortcutName);

    wchar_t exePath[MAXPATHLEN] = {};
    if (NS_WARN_IF(NS_FAILED(BinaryPath::GetLong(exePath)))) {
      return NS_ERROR_FAILURE;
    }

    nsAutoString exeStr;
    nsCOMPtr<nsIFile> exeFile;
    exeStr.Assign(exePath);
    nsresult rv = NS_NewLocalFile(exeStr, true, getter_AddRefs(exeFile));
    if (!NS_SUCCEEDED(rv)) {
      return NS_ERROR_FILE_NOT_FOUND;
    }

    uint16_t iconIndex = aPrivateBrowsing ? IDI_PBMODE : IDI_APPICON;
    // Icon indexes are defined as Resource IDs, but CreateShortcutImpl
    // needs an index.
    iconIndex--;

    rv = CreateShortcutImpl(exeFile, arguments, aShortcutName, exeFile,
                            iconIndex, aAppUserModelId, FOLDERID_Programs,
                            linkName, shortcutPath);
    if (!NS_SUCCEEDED(rv)) {
      return NS_ERROR_FILE_NOT_FOUND;
    }
  }

  if (IsWin10OrLater()) {
    return PinCurrentAppToTaskbarWin10(aCheckOnly, shortcutPath);
  } else {
    return PinCurrentAppToTaskbarWin7(aCheckOnly, shortcutPath);
  }
}

static nsresult PinCurrentAppToTaskbarAsyncImpl(bool aCheckOnly,
                                                bool aPrivateBrowsing,
                                                JSContext* aCx,
                                                dom::Promise** aPromise) {
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  // First available on 1809
  if (IsWin10OrLater() && !IsWin10Sep2018UpdateOrLater()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  ErrorResult rv;
  RefPtr<dom::Promise> promise =
      dom::Promise::Create(xpc::CurrentNativeGlobal(aCx), rv);

  if (MOZ_UNLIKELY(rv.Failed())) {
    return rv.StealNSResult();
  }

  nsAutoString aumid;
  if (NS_WARN_IF(!mozilla::widget::WinTaskbar::GenerateAppUserModelID(
          aumid, aPrivateBrowsing))) {
    return NS_ERROR_FAILURE;
  }

  // NOTE: In the installer, non-private shortcuts are named
  // "${BrandShortName}.lnk". This is set from MOZ_APP_DISPLAYNAME in
  // defines.nsi.in. (Except in dev edition where it's explicitly set to
  // "Firefox Developer Edition" in branding.nsi, which matches
  // MOZ_APP_DISPLAYNAME in aurora/configure.sh.)
  //
  // If this changes, we could expand this to check shortcuts_log.ini,
  // which records the name of the shortcuts as created by the installer.
  //
  // Private shortcuts are not created by the installer (they're created
  // upon user request, ultimately by CreateShortcutImpl, and recorded in
  // a separate shortcuts log. As with non-private shortcuts they have a known
  // name - so there's no need to look through logs to find them.
  nsAutoString shortcutName;
  if (aPrivateBrowsing) {
    nsTArray<nsCString> resIds = {
        "branding/brand.ftl"_ns,
        "browser/browser.ftl"_ns,
    };
    RefPtr<Localization> l10n = Localization::Create(resIds, true);
    nsAutoCString pbStr;
    IgnoredErrorResult rv;
    l10n->FormatValueSync("private-browsing-shortcut-text"_ns, {}, pbStr, rv);
    shortcutName.Append(NS_ConvertUTF8toUTF16(pbStr));
    shortcutName.AppendLiteral(".lnk");
  } else {
    shortcutName.AppendLiteral(MOZ_APP_DISPLAYNAME ".lnk");
  }

  auto promiseHolder = MakeRefPtr<nsMainThreadPtrHolder<dom::Promise>>(
      "CheckPinCurrentAppToTaskbarAsync promise", promise);

  NS_DispatchBackgroundTask(
      NS_NewRunnableFunction(
          "CheckPinCurrentAppToTaskbarAsync",
          [aCheckOnly, aPrivateBrowsing, shortcutName, aumid = nsString{aumid},
           promiseHolder = std::move(promiseHolder)] {
            nsresult rv = NS_ERROR_FAILURE;
            HRESULT hr = CoInitialize(nullptr);

            if (SUCCEEDED(hr)) {
              rv = PinCurrentAppToTaskbarImpl(aCheckOnly, aPrivateBrowsing,
                                              aumid, shortcutName);
              CoUninitialize();
            }

            NS_DispatchToMainThread(NS_NewRunnableFunction(
                "CheckPinCurrentAppToTaskbarAsync callback",
                [rv, promiseHolder = std::move(promiseHolder)] {
                  dom::Promise* promise = promiseHolder.get()->get();

                  if (NS_SUCCEEDED(rv)) {
                    promise->MaybeResolveWithUndefined();
                  } else {
                    promise->MaybeReject(rv);
                  }
                }));
          }),
      NS_DISPATCH_EVENT_MAY_BLOCK);

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::PinCurrentAppToTaskbarAsync(bool aPrivateBrowsing,
                                                   JSContext* aCx,
                                                   dom::Promise** aPromise) {
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1712628 tracks implementing
  // this for MSIX packages.
  if (widget::WinUtils::HasPackageIdentity()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return PinCurrentAppToTaskbarAsyncImpl(
      /* aCheckOnly */ false, aPrivateBrowsing, aCx, aPromise);
}

NS_IMETHODIMP
nsWindowsShellService::CheckPinCurrentAppToTaskbarAsync(
    bool aPrivateBrowsing, JSContext* aCx, dom::Promise** aPromise) {
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1712628 tracks implementing
  // this for MSIX packages.
  if (widget::WinUtils::HasPackageIdentity()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return PinCurrentAppToTaskbarAsyncImpl(
      /* aCheckOnly = */ true, aPrivateBrowsing, aCx, aPromise);
}

static bool IsCurrentAppPinnedToTaskbarSync(const nsAutoString& aumid) {
  wchar_t exePath[MAXPATHLEN] = {};
  if (NS_WARN_IF(NS_FAILED(BinaryPath::GetLong(exePath)))) {
    return false;
  }

  wchar_t folderChars[MAX_PATH] = {};
  HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr,
                                SHGFP_TYPE_CURRENT, folderChars);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  nsAutoString folder;
  folder.Assign(folderChars);
  if (NS_WARN_IF(folder.IsEmpty())) {
    return false;
  }
  if (folder[folder.Length() - 1] != '\\') {
    folder.AppendLiteral("\\");
  }
  folder.AppendLiteral(
      "Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar");
  nsAutoString pattern;
  pattern.Assign(folder);
  pattern.AppendLiteral("\\*.lnk");

  WIN32_FIND_DATAW findData = {};
  HANDLE hFindFile = FindFirstFileW(pattern.get(), &findData);
  if (hFindFile == INVALID_HANDLE_VALUE) {
    Unused << NS_WARN_IF(GetLastError() != ERROR_FILE_NOT_FOUND);
    return false;
  }
  // Past this point we don't return until the end of the function,
  // when FindClose() is called.

  // Check all shortcuts until a match is found
  bool isPinned = false;
  do {
    nsAutoString fileName;
    fileName.Assign(folder);
    fileName.AppendLiteral("\\");
    fileName.Append(findData.cFileName);

    // Create a shell link object for loading the shortcut
    RefPtr<IShellLinkW> link;
    HRESULT hr =
        CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                         IID_IShellLinkW, getter_AddRefs(link));
    if (NS_WARN_IF(FAILED(hr))) {
      continue;
    }

    // Load
    RefPtr<IPersistFile> persist;
    hr = link->QueryInterface(IID_IPersistFile, getter_AddRefs(persist));
    if (NS_WARN_IF(FAILED(hr))) {
      continue;
    }

    hr = persist->Load(fileName.get(), STGM_READ);
    if (NS_WARN_IF(FAILED(hr))) {
      continue;
    }

    // Note: AUMID is not checked, so a pin that does not group properly
    // will still count as long as the exe matches.

    // Check the exe path
    static_assert(MAXPATHLEN == MAX_PATH);
    wchar_t storedExePath[MAX_PATH] = {};
    // With no flags GetPath gets a long path
    hr = link->GetPath(storedExePath, ArrayLength(storedExePath), nullptr, 0);
    if (FAILED(hr) || hr == S_FALSE) {
      continue;
    }
    // Case insensitive path comparison
    // NOTE: Because this compares the path directly, it is possible to
    // have a false negative mismatch.
    if (wcsnicmp(storedExePath, exePath, MAXPATHLEN) == 0) {
      RefPtr<IPropertyStore> propStore;
      hr = link->QueryInterface(IID_IPropertyStore, getter_AddRefs(propStore));
      if (NS_WARN_IF(FAILED(hr))) {
        continue;
      }

      PROPVARIANT pv;
      hr = propStore->GetValue(PKEY_AppUserModel_ID, &pv);
      if (NS_WARN_IF(FAILED(hr))) {
        continue;
      }

      wchar_t storedAUMID[MAX_PATH];
      hr = PropVariantToString(pv, storedAUMID, MAX_PATH);
      PropVariantClear(&pv);
      if (NS_WARN_IF(FAILED(hr))) {
        continue;
      }

      if (aumid.Equals(storedAUMID)) {
        isPinned = true;
        break;
      }
    }
  } while (FindNextFileW(hFindFile, &findData));

  FindClose(hFindFile);

  return isPinned;
}

NS_IMETHODIMP
nsWindowsShellService::IsCurrentAppPinnedToTaskbarAsync(
    const nsAString& aumid, JSContext* aCx, /* out */ dom::Promise** aPromise) {
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1712628 tracks implementing
  // this for MSIX packages.
  if (widget::WinUtils::HasPackageIdentity()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  ErrorResult rv;
  RefPtr<dom::Promise> promise =
      dom::Promise::Create(xpc::CurrentNativeGlobal(aCx), rv);
  if (MOZ_UNLIKELY(rv.Failed())) {
    return rv.StealNSResult();
  }

  // A holder to pass the promise through the background task and back to
  // the main thread when finished.
  auto promiseHolder = MakeRefPtr<nsMainThreadPtrHolder<dom::Promise>>(
      "IsCurrentAppPinnedToTaskbarAsync promise", promise);

  // nsAString can't be captured by a lambda because it does not have a
  // public copy constructor
  nsAutoString capturedAumid(aumid);
  NS_DispatchBackgroundTask(
      NS_NewRunnableFunction(
          "IsCurrentAppPinnedToTaskbarAsync",
          [capturedAumid, promiseHolder = std::move(promiseHolder)] {
            bool isPinned = false;

            HRESULT hr = CoInitialize(nullptr);
            if (SUCCEEDED(hr)) {
              isPinned = IsCurrentAppPinnedToTaskbarSync(capturedAumid);
              CoUninitialize();
            }

            // Dispatch back to the main thread to resolve the promise.
            NS_DispatchToMainThread(NS_NewRunnableFunction(
                "IsCurrentAppPinnedToTaskbarAsync callback",
                [isPinned, promiseHolder = std::move(promiseHolder)] {
                  promiseHolder.get()->get()->MaybeResolve(isPinned);
                }));
          }),
      NS_DISPATCH_EVENT_MAY_BLOCK);

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowsShellService::ClassifyShortcut(const nsAString& aPath,
                                        nsAString& aResult) {
  aResult.Truncate();

  nsAutoString shortcutPath(PromiseFlatString(aPath));

  // NOTE: On Windows 7, Start Menu pin shortcuts are stored under
  // "<FOLDERID_User Pinned>\StartMenu", but on Windows 10 they are just normal
  // Start Menu shortcuts. These both map to "StartMenu" for consistency,
  // rather than having a separate "StartMenuPins" which would only apply on
  // Win7.
  struct {
    KNOWNFOLDERID folderId;
    const char16_t* postfix;
    const char16_t* classification;
  } folders[] = {{FOLDERID_CommonStartMenu, u"\\", u"StartMenu"},
                 {FOLDERID_StartMenu, u"\\", u"StartMenu"},
                 {FOLDERID_PublicDesktop, u"\\", u"Desktop"},
                 {FOLDERID_Desktop, u"\\", u"Desktop"},
                 {FOLDERID_UserPinned, u"\\TaskBar\\", u"Taskbar"},
                 {FOLDERID_UserPinned, u"\\StartMenu\\", u"StartMenu"}};

  for (size_t i = 0; i < ArrayLength(folders); ++i) {
    nsAutoString knownPath;

    // These flags are chosen to avoid I/O, see bug 1363398.
    DWORD flags =
        KF_FLAG_SIMPLE_IDLIST | KF_FLAG_DONT_VERIFY | KF_FLAG_NO_ALIAS;
    PWSTR rawPath = nullptr;

    if (FAILED(SHGetKnownFolderPath(folders[i].folderId, flags, nullptr,
                                    &rawPath))) {
      continue;
    }

    knownPath = nsDependentString(rawPath);
    CoTaskMemFree(rawPath);

    knownPath.Append(folders[i].postfix);
    // Check if the shortcut path starts with the shell folder path.
    if (wcsnicmp(shortcutPath.get(), knownPath.get(), knownPath.Length()) ==
        0) {
      aResult.Assign(folders[i].classification);
      nsTArray<nsCString> resIds = {
          "branding/brand.ftl"_ns,
          "browser/browser.ftl"_ns,
      };
      RefPtr<Localization> l10n = Localization::Create(resIds, true);
      nsAutoCString pbStr;
      IgnoredErrorResult rv;
      l10n->FormatValueSync("private-browsing-shortcut-text"_ns, {}, pbStr, rv);
      NS_ConvertUTF8toUTF16 widePbStr(pbStr);
      if (wcsstr(shortcutPath.get(), widePbStr.get())) {
        aResult.AppendLiteral("Private");
      }
      return NS_OK;
    }
  }

  // Nothing found, aResult is already "".
  return NS_OK;
}

nsWindowsShellService::nsWindowsShellService() {}

nsWindowsShellService::~nsWindowsShellService() {}
