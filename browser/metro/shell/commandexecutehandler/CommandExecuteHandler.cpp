/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "CEHHelper.h"

#include <objbase.h>
#include <combaseapi.h>
#include <atlcore.h>
#include <atlstr.h>
#include <wininet.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <propkey.h>
#include <propvarutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include <io.h>
#include <shellapi.h>
#include <wininet.h>

#ifdef SHOW_CONSOLE
#define DEBUG_DELAY_SHUTDOWN 1
#endif

// Heartbeat timer duration used while waiting for an incoming request.
#define HEARTBEAT_MSEC 1000
// Total number of heartbeats we wait before giving up and shutting down.
#define REQUEST_WAIT_TIMEOUT 30
// Pulled from desktop browser's shell
#define APP_REG_NAME L"Firefox"

// If we have a restart request, attempt to wait up to RESTART_WAIT_TIMEOUT
// until the previous instance closes.  We don't want to wait too long
// because the browser could appear to randomly start for the user. We want
// it to also be long enough so the browser has time to close.
#define RESTART_WAIT_PER_RETRY 50
#define RESTART_WAIT_TIMEOUT 38000

static const WCHAR* kFirefoxExe = L"firefox.exe";
static const WCHAR* kMetroFirefoxExe = L"firefox.exe";
static const WCHAR* kDefaultMetroBrowserIDPathKey = L"FirefoxURL";
static const WCHAR* kMetroRestartCmdLine = L"--metro-restart";

static bool GetDefaultBrowserPath(CStringW& aPathBuffer);

/*
 * Retrieve our module dir path.
 *
 * @aPathBuffer Buffer to fill
 */
static bool GetModulePath(CStringW& aPathBuffer)
{
  WCHAR buffer[MAX_PATH];
  memset(buffer, 0, sizeof(buffer));

  if (!GetModuleFileName(nullptr, buffer, MAX_PATH)) {
    Log(L"GetModuleFileName failed.");
    return false;
  }

  WCHAR* slash = wcsrchr(buffer, '\\');
  if (!slash)
    return false;
  *slash = '\0';

  aPathBuffer = buffer;
  return true;
}


template <class T>void SafeRelease(T **ppT)
{
  if (*ppT) {
    (*ppT)->Release();
    *ppT = nullptr;
  }
}

template <class T> HRESULT SetInterface(T **ppT, IUnknown *punk)
{
  SafeRelease(ppT);
  return punk ? punk->QueryInterface(ppT) : E_NOINTERFACE;
}

class __declspec(uuid("5100FEC1-212B-4BF5-9BF8-3E650FD794A3"))
  CExecuteCommandVerb : public IExecuteCommand,
                        public IObjectWithSelection,
                        public IInitializeCommand,
                        public IObjectWithSite,
                        public IExecuteCommandApplicationHostEnvironment
{
public:

  CExecuteCommandVerb() :
    mRef(1),
    mShellItemArray(nullptr),
    mUnkSite(nullptr),
    mTargetIsFileSystemLink(false),
    mTargetIsDefaultBrowser(false),
    mTargetIsBrowser(false),
    mIsDesktopRequest(true),
    mIsRestartMetroRequest(false),
    mRequestMet(false)
  {
  }

  bool RequestMet() { return mRequestMet; }
  long RefCount() { return mRef; }

  // IUnknown
  IFACEMETHODIMP QueryInterface(REFIID aRefID, void **aInt)
  {
    static const QITAB qit[] = {
      QITABENT(CExecuteCommandVerb, IExecuteCommand),
      QITABENT(CExecuteCommandVerb, IObjectWithSelection),
      QITABENT(CExecuteCommandVerb, IInitializeCommand),
      QITABENT(CExecuteCommandVerb, IObjectWithSite),
      QITABENT(CExecuteCommandVerb, IExecuteCommandApplicationHostEnvironment),
      { 0 },
    };
    return QISearch(this, qit, aRefID, aInt);
  }

  IFACEMETHODIMP_(ULONG) AddRef()
  {
    return InterlockedIncrement(&mRef);
  }

  IFACEMETHODIMP_(ULONG) Release()
  {
    long cRef = InterlockedDecrement(&mRef);
    if (!cRef) {
      delete this;
    }
    return cRef;
  }

  // IExecuteCommand
  IFACEMETHODIMP SetKeyState(DWORD aKeyState)
  {
    mKeyState = aKeyState;
    return S_OK;
  }

  IFACEMETHODIMP SetParameters(PCWSTR aParameters)
  {
    Log(L"SetParameters: '%s'", aParameters);

    if (_wcsicmp(aParameters, kMetroRestartCmdLine) == 0) {
      mIsRestartMetroRequest = true;
    } else {
      mParameters = aParameters;
    }
    return S_OK;
  }

  IFACEMETHODIMP SetPosition(POINT aPoint)
  { return S_OK; }

  IFACEMETHODIMP SetShowWindow(int aShowFlag)
  { return S_OK; }

  IFACEMETHODIMP SetNoShowUI(BOOL aNoUI)
  { return S_OK; }

  IFACEMETHODIMP SetDirectory(PCWSTR aDirPath)
  { return S_OK; }

  IFACEMETHODIMP Execute();

  // IObjectWithSelection
  IFACEMETHODIMP SetSelection(IShellItemArray *aArray)
  {
    if (!aArray) {
      return E_FAIL;
    }

    SetInterface(&mShellItemArray, aArray);

    DWORD count = 0;
    aArray->GetCount(&count);
    if (!count) {
      return E_FAIL;
    }

#ifdef SHOW_CONSOLE
    Log(L"SetSelection param count: %d", count);
    for (DWORD idx = 0; idx < count; idx++) {
      IShellItem* item = nullptr;
      if (SUCCEEDED(aArray->GetItemAt(idx, &item))) {
        LPWSTR str = nullptr;
        if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &str))) {
          if (FAILED(item->GetDisplayName(SIGDN_URL, &str))) {
            Log(L"Failed to get a shell item array item.");
            item->Release();
            continue;
          }
        }
        item->Release();
        Log(L"SetSelection param: '%s'", str);
        CoTaskMemFree(str);
      }
    }
#endif

    IShellItem* item = nullptr;
    if (FAILED(aArray->GetItemAt(0, &item))) {
      return E_FAIL;
    }

    bool isFileSystem = false;
    if (!SetTargetPath(item) || !mTarget.GetLength()) {
      Log(L"SetTargetPath failed.");
      return E_FAIL;
    }
    item->Release();

    Log(L"SetSelection target: %s", mTarget);
    return S_OK;
  }

  IFACEMETHODIMP GetSelection(REFIID aRefID, void **aInt)
  {
    *aInt = nullptr;
    return mShellItemArray ? mShellItemArray->QueryInterface(aRefID, aInt) : E_FAIL;
  }

  // IInitializeCommand
  IFACEMETHODIMP Initialize(PCWSTR aVerb, IPropertyBag* aPropBag)
  {
    if (!aVerb)
      return E_FAIL;
    // 'open', 'edit', etc. Based on our registry settings
    Log(L"Initialize(%s)", aVerb);
    mVerb = aVerb;
    return S_OK;
  }

  // IObjectWithSite
  IFACEMETHODIMP SetSite(IUnknown *aUnkSite)
  {
    SetInterface(&mUnkSite, aUnkSite);
    return S_OK;
  }

  IFACEMETHODIMP GetSite(REFIID aRefID, void **aInt)
  {
    *aInt = nullptr;
    return mUnkSite ? mUnkSite->QueryInterface(aRefID, aInt) : E_FAIL;
  }

  // IExecuteCommandApplicationHostEnvironment
  IFACEMETHODIMP GetValue(AHE_TYPE *aLaunchType)
  {
    Log(L"IExecuteCommandApplicationHostEnvironment::GetValue()");
    *aLaunchType = AHE_DESKTOP;
    mIsDesktopRequest = true;

    if (!mUnkSite) {
      Log(L"No mUnkSite.");
      return S_OK;
    }

    HRESULT hr;
    IServiceProvider* pSvcProvider = nullptr;
    hr = mUnkSite->QueryInterface(IID_IServiceProvider, (void**)&pSvcProvider);
    if (!pSvcProvider) {
      Log(L"Couldn't get IServiceProvider service from explorer. (%X)", hr);
      return S_OK;
    }

    IExecuteCommandHost* pHost = nullptr;
    // If we can't get this it's a conventional desktop launch
    hr = pSvcProvider->QueryService(SID_ExecuteCommandHost,
                                    IID_IExecuteCommandHost, (void**)&pHost);
    if (!pHost) {
      Log(L"Couldn't get IExecuteCommandHost service from explorer. (%X)", hr);
      SafeRelease(&pSvcProvider);
      return S_OK;
    }
    SafeRelease(&pSvcProvider);

    EC_HOST_UI_MODE mode;
    if (FAILED(pHost->GetUIMode(&mode))) {
      Log(L"GetUIMode failed.");
      SafeRelease(&pHost);
      return S_OK;
    }

    // 0 - launched from desktop
    // 1 - ?
    // 2 - launched from tile interface
    Log(L"GetUIMode: %d", mode);

    if (!IsDefaultBrowser()) {
      mode = ECHUIM_DESKTOP;
    }

    if (mode == ECHUIM_DESKTOP) {
      Log(L"returning AHE_DESKTOP");
      SafeRelease(&pHost);
      return S_OK;
    }
    SafeRelease(&pHost);

    if (!IsDX10Available()) {
      Log(L"returning AHE_DESKTOP because DX10 is not available");
      *aLaunchType = AHE_DESKTOP;
      mIsDesktopRequest = true;
    } else {
      Log(L"returning AHE_IMMERSIVE");
      *aLaunchType = AHE_IMMERSIVE;
      mIsDesktopRequest = false;
    }
    return S_OK;
  }

  /*
   * Retrieve the target path if it is the default browser
   * or if not default, retreives the target path if it is a firefox browser
   * or if the target is not firefox, relies on a hack to get the
   * 'module dir path\firefox.exe'
   * The reason why it's not good to rely on the CEH path is because there is
   * no guarantee win8 will use the CEH at our expected path.  It has an in
   * memory cache even if the registry is updated for the CEH path.
   *
   * @aPathBuffer Buffer to fill
   */
  bool GetDesktopBrowserPath(CStringW& aPathBuffer)
  {
    // If the target was the default browser itself then return early.  Otherwise
    // rely on a hack to check CEH path and calculate it relative to it.

    if (mTargetIsDefaultBrowser || mTargetIsBrowser) {
      aPathBuffer = mTarget;
      return true;
    }

    if (!GetModulePath(aPathBuffer))
      return false;

    // ceh.exe sits in dist/bin root with the desktop browser. Since this
    // is a firefox only component, this hardcoded filename is ok.
    aPathBuffer.Append(L"\\");
    aPathBuffer.Append(kFirefoxExe);
    return true;
  }

  bool IsDefaultBrowser()
  {
    IApplicationAssociationRegistration* pAAR;
    HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration,
                                  nullptr,
                                  CLSCTX_INPROC,
                                  IID_IApplicationAssociationRegistration,
                                  (void**)&pAAR);
    if (FAILED(hr))
      return false;

    BOOL res = FALSE;
    hr = pAAR->QueryAppIsDefaultAll(AL_EFFECTIVE,
                                    APP_REG_NAME,
                                    &res);
    Log(L"QueryAppIsDefaultAll: %d", res);
    if (!res) {
      pAAR->Release();
      return false;
    }
    // Make sure the Prog ID matches what we have
    LPWSTR registeredApp;
    hr = pAAR->QueryCurrentDefault(L"http", AT_URLPROTOCOL, AL_EFFECTIVE,
                                    &registeredApp);
    pAAR->Release();
    Log(L"QueryCurrentDefault: %X", hr);
    if (FAILED(hr))
      return false;

    Log(L"registeredApp=%s", registeredApp);
    bool result = !wcsicmp(registeredApp, kDefaultMetroBrowserIDPathKey);
    CoTaskMemFree(registeredApp);
    if (!result)
      return false;

    // If the registry points another browser's path,
    // activating the Metro browser will fail. So fallback to the desktop.
    CStringW selfPath;
    GetDesktopBrowserPath(selfPath);
    CStringW browserPath;
    GetDefaultBrowserPath(browserPath);

    return !selfPath.CompareNoCase(browserPath);
  }
private:
  ~CExecuteCommandVerb()
  {
    SafeRelease(&mShellItemArray);
    SafeRelease(&mUnkSite);
  }

  void LaunchDesktopBrowser();
  bool SetTargetPath(IShellItem* aItem);

  long mRef;
  IShellItemArray *mShellItemArray;
  IUnknown *mUnkSite;
  CStringW mVerb;
  CStringW mTarget;
  CStringW mParameters;
  bool mTargetIsFileSystemLink;
  bool mTargetIsDefaultBrowser;
  bool mTargetIsBrowser;
  DWORD mKeyState;
  bool mIsDesktopRequest;
  bool mIsRestartMetroRequest;
  bool mRequestMet;
};

/*
 * Retrieve the current default browser's path.
 *
 * @aPathBuffer Buffer to fill
 */
static bool GetDefaultBrowserPath(CStringW& aPathBuffer)
{
  WCHAR buffer[MAX_PATH];
  DWORD length = MAX_PATH;

  if (FAILED(AssocQueryStringW(ASSOCF_NOTRUNCATE | ASSOCF_INIT_IGNOREUNKNOWN,
                               ASSOCSTR_EXECUTABLE,
                               kDefaultMetroBrowserIDPathKey, nullptr,
                               buffer, &length))) {
    Log(L"AssocQueryString failed.");
    return false;
  }

  // sanity check
  if (lstrcmpiW(PathFindFileNameW(buffer), kFirefoxExe))
    return false;

  aPathBuffer = buffer;
  return true;
}

/*
 * Retrieve the app model id of the firefox metro browser.
 *
 * @aPathBuffer Buffer to fill
 * @aCharLength Length of buffer to fill in characters
 */
template <size_t N>
static bool GetDefaultBrowserAppModelID(WCHAR (&aIDBuffer)[N])
{
  HKEY key;
  if (RegOpenKeyExW(HKEY_CLASSES_ROOT, kDefaultMetroBrowserIDPathKey,
                    0, KEY_READ, &key) != ERROR_SUCCESS) {
    return false;
  }
  DWORD len = sizeof(aIDBuffer);
  memset(aIDBuffer, 0, len);
  if (RegQueryValueExW(key, L"AppUserModelID", nullptr, nullptr,
                       (LPBYTE)aIDBuffer, &len) != ERROR_SUCCESS || !len) {
    RegCloseKey(key);
    return false;
  }
  RegCloseKey(key);
  return true;
}

namespace {
  const FORMATETC kPlainTextFormat =
    {CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  const FORMATETC kPlainTextWFormat =
    {CF_UNICODETEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
}

bool HasPlainText(IDataObject* aDataObj) {
  return SUCCEEDED(aDataObj->QueryGetData((FORMATETC*)&kPlainTextWFormat)) ||
      SUCCEEDED(aDataObj->QueryGetData((FORMATETC*)&kPlainTextFormat));
}

bool GetPlainText(IDataObject* aDataObj, CStringW& cstrText)
{
  if (!HasPlainText(aDataObj))
    return false;

  STGMEDIUM store;

  // unicode text
  if (SUCCEEDED(aDataObj->GetData((FORMATETC*)&kPlainTextWFormat, &store))) {
    // makes a copy
    cstrText = static_cast<LPCWSTR>(GlobalLock(store.hGlobal));
    GlobalUnlock(store.hGlobal);
    ReleaseStgMedium(&store);
    return true;
  }

  // ascii text
  if (SUCCEEDED(aDataObj->GetData((FORMATETC*)&kPlainTextFormat, &store))) {
    // makes a copy
    cstrText = static_cast<char*>(GlobalLock(store.hGlobal));
    GlobalUnlock(store.hGlobal);
    ReleaseStgMedium(&store);
    return true;
  }

  return false;
}

/*
 * Updates the current target based on the contents of
 * a shell item.
 */
bool CExecuteCommandVerb::SetTargetPath(IShellItem* aItem)
{
  if (!aItem)
    return false;

  CString cstrText;
  CComPtr<IDataObject> object;
  // Check the underlying data object first to insure we get
  // absolute uri. See chromium bug 157184.
  if (SUCCEEDED(aItem->BindToHandler(nullptr, BHID_DataObject,
                                     IID_IDataObject,
                                     reinterpret_cast<void**>(&object))) &&
      GetPlainText(object, cstrText)) {
    wchar_t scheme[16];
    URL_COMPONENTS components = {0};
    components.lpszScheme = scheme;
    components.dwSchemeLength = sizeof(scheme)/sizeof(scheme[0]);
    components.dwStructSize = sizeof(components);
    // note, more advanced use may have issues with paths with spaces.
    if (!InternetCrackUrlW(cstrText, 0, 0, &components)) {
      Log(L"Failed to identify object text '%s'", cstrText);
      return false;
    }

    mTargetIsFileSystemLink = (components.nScheme == INTERNET_SCHEME_FILE);
    mTarget = cstrText;

    return true;
  }

  Log(L"No data object or data object has no text.");

  // Use the shell item display name
  LPWSTR str = nullptr;
  mTargetIsFileSystemLink = true;
  if (FAILED(aItem->GetDisplayName(SIGDN_FILESYSPATH, &str))) {
    mTargetIsFileSystemLink = false;
    if (FAILED(aItem->GetDisplayName(SIGDN_URL, &str))) {
      Log(L"Failed to get parameter string.");
      return false;
    }
  }
  mTarget = str;
  CoTaskMemFree(str);

  CStringW defaultPath;
  GetDefaultBrowserPath(defaultPath);
  mTargetIsDefaultBrowser = !mTarget.CompareNoCase(defaultPath);

  size_t browserEXELen = wcslen(kFirefoxExe);
  mTargetIsBrowser = mTarget.GetLength() >= browserEXELen &&
                     !mTarget.Right(browserEXELen).CompareNoCase(kFirefoxExe);

  return true;
}

/*
 * Desktop launch - Launch the destop browser to display the current
 * target using shellexecute.
 */
void CExecuteCommandVerb::LaunchDesktopBrowser()
{
  CStringW browserPath;
  if (!GetDesktopBrowserPath(browserPath)) {
    return;
  }

  // If a taskbar shortcut, link or local file is clicked, the target will
  // be the browser exe or file.  Don't pass in -url for the target if the
  // target is known to be a browser.  Otherwise, one instance of Firefox
  // will try to open another instance.
  CStringW params;
  if (!mTargetIsDefaultBrowser && !mTargetIsBrowser && !mTarget.IsEmpty()) {
    // Fallback to the module path if it failed to get the default browser.
    GetDefaultBrowserPath(browserPath);
    params += "-url ";
    params += "\"";
    params += mTarget;
    params += "\"";
  }

  // Tack on any extra parameters we received (for example -profilemanager)
  if (!mParameters.IsEmpty()) {
    params += " ";
    params += mParameters;
  }

  Log(L"Desktop Launch: verb:%s exe:%s params:%s", mVerb, browserPath, params); 

  SHELLEXECUTEINFOW seinfo;
  memset(&seinfo, 0, sizeof(seinfo));
  seinfo.cbSize = sizeof(SHELLEXECUTEINFOW);
  seinfo.fMask  = 0;
  seinfo.hwnd   = nullptr;
  seinfo.lpVerb = nullptr;
  seinfo.lpFile = browserPath;
  seinfo.lpParameters =  params;
  seinfo.lpDirectory  = nullptr;
  seinfo.nShow  = SW_SHOWNORMAL;
        
  ShellExecuteExW(&seinfo);
}

class AutoSetRequestMet
{
public:
  explicit AutoSetRequestMet(bool* aFlag) :
    mFlag(aFlag) {}
  ~AutoSetRequestMet() { if (mFlag) *mFlag = true; }
private:
  bool* mFlag;
};

static HRESULT
PrepareActivationManager(CComPtr<IApplicationActivationManager> &activateMgr)
{
  HRESULT hr = activateMgr.CoCreateInstance(CLSID_ApplicationActivationManager, NULL, CLSCTX_LOCAL_SERVER);
  if (FAILED(hr)) {
    Log(L"CoCreateInstance failed, launching on desktop.");
    return E_FAIL;
  }

  // Hand off focus rights to the out-of-process activation server. Without
  // this the metro interface won't launch.
  hr = CoAllowSetForegroundWindow(activateMgr, nullptr);
  if (FAILED(hr)) {
    Log(L"CoAllowSetForegroundWindow result %X", hr);
    return E_FAIL;
  }

  return S_OK;
}

DWORD WINAPI
DelayedExecuteThread(LPVOID param)
{
  Log(L"Starting delayed execute thread...");
  bool &bRequestMet(*(bool*)param);
  AutoSetRequestMet asrm(&bRequestMet);

  CoInitialize(NULL);

  CComPtr<IApplicationActivationManager> activateMgr;
  if (FAILED(PrepareActivationManager(activateMgr))) {
      Log(L"Warning: Could not prepare activation manager");
  }

  size_t currentWaitTime = 0;
  while(currentWaitTime < RESTART_WAIT_TIMEOUT) {
    if (!IsImmersiveProcessRunning(kMetroFirefoxExe))
      break;
    currentWaitTime += RESTART_WAIT_PER_RETRY;
    Sleep(RESTART_WAIT_PER_RETRY);
  }

  Log(L"Done waiting, getting app ID");
  // Activate the application as long as we can obtian the appModelID
  WCHAR appModelID[256];
  if (GetDefaultBrowserAppModelID(appModelID)) {
    Log(L"Activating application");
    DWORD processID;
    HRESULT hr = activateMgr->ActivateApplication(appModelID, L"", AO_NOSPLASHSCREEN, &processID);
    if (SUCCEEDED(hr)) {
      Log(L"Activate application succeeded");
    } else {
      Log(L"Activate application failed! (%x)", hr);
    }
  }

  CoUninitialize();
  return 0;
}

IFACEMETHODIMP CExecuteCommandVerb::Execute()
{
  Log(L"Execute()");

  if (!mTarget.GetLength()) {
    // We shut down when this flips to true
    mRequestMet = true;
    return E_FAIL;
  }

  if (mIsRestartMetroRequest) {
    HANDLE thread = CreateThread(NULL, 0, DelayedExecuteThread, &mRequestMet, 0, NULL);
    CloseHandle(thread);
    return S_OK;
  }

  // We shut down when this flips to true
  AutoSetRequestMet asrm(&mRequestMet);

  // Launch on the desktop
  if (mIsDesktopRequest) {
    LaunchDesktopBrowser();
    return S_OK;
  }

  CComPtr<IApplicationActivationManager> activateMgr;
  if (FAILED(PrepareActivationManager(activateMgr))) {
      LaunchDesktopBrowser();
      return S_OK;
  }

  HRESULT hr;
  WCHAR appModelID[256];
  if (!GetDefaultBrowserAppModelID(appModelID)) {
    Log(L"GetDefaultBrowserAppModelID failed, launching on desktop.");
    LaunchDesktopBrowser();
    return S_OK;
  }

  Log(L"Metro Launch: verb:%s appid:%s params:%s", mVerb, appModelID, mTarget); 


  // shortcuts to the application
  DWORD processID;
  if (mTargetIsDefaultBrowser) {
    hr = activateMgr->ActivateApplication(appModelID, L"", AO_NONE, &processID);
    Log(L"ActivateApplication result %X", hr);
  // files
  } else if (mTargetIsFileSystemLink) {
    hr = activateMgr->ActivateForFile(appModelID, mShellItemArray, mVerb, &processID);
    Log(L"ActivateForFile result %X", hr);
  // protocols
  } else {
    hr = activateMgr->ActivateForProtocol(appModelID, mShellItemArray, &processID);
    Log(L"ActivateForProtocol result %X", hr);
  }
  return S_OK;
}

class ClassFactory : public IClassFactory 
{
public:
  ClassFactory(IUnknown *punkObject);
  ~ClassFactory();
  STDMETHODIMP Register(CLSCTX classContent, REGCLS classUse);
  STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
  STDMETHODIMP_(ULONG) AddRef() { return 2; }
  STDMETHODIMP_(ULONG) Release() { return 1; }
  STDMETHODIMP CreateInstance(IUnknown *punkOuter, REFIID riid, void **ppv);
  STDMETHODIMP LockServer(BOOL);
private:
  IUnknown* mUnkObject;
  DWORD mRegID;
};

ClassFactory::ClassFactory(IUnknown* aUnkObj) :
  mUnkObject(aUnkObj),
  mRegID(0)
{
  if (mUnkObject) {
    mUnkObject->AddRef();
  }
}

ClassFactory::~ClassFactory()
{
  if (mRegID) {
    CoRevokeClassObject(mRegID);
  }
  mUnkObject->Release();
}

STDMETHODIMP
ClassFactory::Register(CLSCTX aClass, REGCLS aUse)
{
  return CoRegisterClassObject(__uuidof(CExecuteCommandVerb),
                               static_cast<IClassFactory *>(this),
                               aClass, aUse, &mRegID);
}

STDMETHODIMP
ClassFactory::QueryInterface(REFIID riid, void **ppv)
{
  IUnknown *punk = nullptr;
  if (riid == IID_IUnknown || riid == IID_IClassFactory) {
    punk = static_cast<IClassFactory*>(this);
  }
  *ppv = punk;
  if (punk) {
    punk->AddRef();
    return S_OK;
  } else {
    return E_NOINTERFACE;
  }
}

STDMETHODIMP
ClassFactory::CreateInstance(IUnknown *punkOuter, REFIID riid, void **ppv)
{
  *ppv = nullptr;
  if (punkOuter)
    return CLASS_E_NOAGGREGATION;
  return mUnkObject->QueryInterface(riid, ppv);
}

LONG gObjRefCnt;

STDMETHODIMP
ClassFactory::LockServer(BOOL fLock)
{
  if (fLock)
    InterlockedIncrement(&gObjRefCnt);
  else
    InterlockedDecrement(&gObjRefCnt);
  Log(L"ClassFactory::LockServer() %d", gObjRefCnt);
  return S_OK;
}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR pszCmdLine, int)
{
#if defined(SHOW_CONSOLE)
  SetupConsole();
#endif
  //Log(pszCmdLine);

  if (!wcslen(pszCmdLine) || StrStrI(pszCmdLine, L"-Embedding"))
  {
      CoInitialize(nullptr);

      CExecuteCommandVerb *pHandler = new CExecuteCommandVerb();
      if (!pHandler)
        return E_OUTOFMEMORY;

      IUnknown* ppi;
      pHandler->QueryInterface(IID_IUnknown, (void**)&ppi);
      if (!ppi)
        return E_FAIL;

      ClassFactory classFactory(ppi);
      ppi->Release();
      ppi = nullptr;

      // REGCLS_SINGLEUSE insures we only get used once and then discarded.
      if (FAILED(classFactory.Register(CLSCTX_LOCAL_SERVER, REGCLS_SINGLEUSE)))
        return -1;

      if (!SetTimer(nullptr, 1, HEARTBEAT_MSEC, nullptr)) {
        Log(L"Failed to set timer, can't process request.");
        return -1;
      }

      MSG msg;
      long beatCount = 0;
      while (GetMessage(&msg, 0, 0, 0) > 0) {
        if (msg.message == WM_TIMER) {
          if (++beatCount > REQUEST_WAIT_TIMEOUT ||
              (pHandler->RequestMet() && pHandler->RefCount() < 2)) {
            break;
          }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

#ifdef DEBUG_DELAY_SHUTDOWN
      Sleep(10000);
#endif
      CoUninitialize();
      return 0;
  }
  return 0;
}
