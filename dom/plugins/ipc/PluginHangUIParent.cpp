/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginHangUI.h"

#include "PluginHangUIParent.h"

#include "mozilla/Telemetry.h"
#include "mozilla/plugins/PluginModuleParent.h"

#include "nsContentUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIProperties.h"
#include "nsIWindowMediator.h"
#include "nsServiceManagerUtils.h"

#include "WidgetUtils.h"

using base::ProcessHandle;

using mozilla::widget::WidgetUtils;

using std::string;
using std::vector;

namespace {
class nsPluginHangUITelemetry : public nsRunnable
{
public:
  nsPluginHangUITelemetry(int aResponseCode, int aDontAskCode)
    : mResponseCode(aResponseCode),
      mDontAskCode(aDontAskCode)
  {
  }

  NS_IMETHOD
  Run()
  {
    mozilla::Telemetry::Accumulate(
              mozilla::Telemetry::PLUGIN_HANG_UI_USER_RESPONSE, mResponseCode);
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::PLUGIN_HANG_UI_DONT_ASK,
                                   mDontAskCode);
    return NS_OK;
  }

private:
  int mResponseCode;
  int mDontAskCode;
};
} // anonymous namespace

namespace mozilla {
namespace plugins {

const DWORD PluginHangUIParent::kTimeout = 5000U;

PluginHangUIParent::PluginHangUIParent(PluginModuleParent* aModule)
  : mModule(aModule),
    mMainThreadMessageLoop(MessageLoop::current()),
    mIsShowing(false),
    mLastUserResponse(0),
    mHangUIProcessHandle(NULL),
    mMainWindowHandle(NULL),
    mRegWait(NULL),
    mShowEvent(NULL),
    mShowTicks(0),
    mResponseTicks(0)
{
}

PluginHangUIParent::~PluginHangUIParent()
{
  if (mRegWait) {
    ::UnregisterWaitEx(mRegWait, INVALID_HANDLE_VALUE);
  }
  if (mShowEvent) {
    ::CloseHandle(mShowEvent);
  }
  if (mHangUIProcessHandle) {
    ::CloseHandle(mHangUIProcessHandle);
  }
}

bool
PluginHangUIParent::DontShowAgain() const
{
  return (mLastUserResponse & HANGUI_USER_RESPONSE_DONT_SHOW_AGAIN);
}

bool
PluginHangUIParent::WasLastHangStopped() const
{
  return (mLastUserResponse & HANGUI_USER_RESPONSE_STOP);
}

unsigned int
PluginHangUIParent::LastShowDurationMs() const
{
  // We only return something if there was a user response
  if (!mLastUserResponse) {
    return 0;
  }
  return static_cast<unsigned int>(mResponseTicks - mShowTicks);
}

bool
PluginHangUIParent::Init(const nsString& aPluginName)
{
  if (mHangUIProcessHandle) {
    return false;
  }

  nsresult rv;
  rv = mMiniShm.Init(this, ::IsDebuggerPresent() ? INFINITE : kTimeout);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIProperties>
    directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
  if (!directoryService) {
    return false;
  }
  nsCOMPtr<nsIFile> greDir;
  rv = directoryService->Get(NS_GRE_DIR,
                             NS_GET_IID(nsIFile),
                             getter_AddRefs(greDir));
  if (NS_FAILED(rv)) {
    return false;
  }
  nsAutoString path;
  greDir->GetPath(path);

  FilePath exePath(path.get());
  exePath = exePath.AppendASCII(MOZ_HANGUI_PROCESS_NAME);
  CommandLine commandLine(exePath.value());

  nsXPIDLString localizedStr;
  const PRUnichar* formatParams[] = { aPluginName.get() };
  rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                             "PluginHangUIMessage",
                                             formatParams,
                                             localizedStr);
  if (NS_FAILED(rv)) {
    return false;
  }
  commandLine.AppendLooseValue(localizedStr.get());

  const char* keys[] = { "PluginHangUITitle",
                         "PluginHangUIWaitButton",
                         "PluginHangUIStopButton",
                         "DontAskAgain" };
  for (unsigned int i = 0; i < ArrayLength(keys); ++i) {
    rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                            keys[i],
                                            localizedStr);
    if (NS_FAILED(rv)) {
      return false;
    }
    commandLine.AppendLooseValue(localizedStr.get());
  }

  rv = GetHangUIOwnerWindowHandle(mMainWindowHandle);
  if (NS_FAILED(rv)) {
    return false;
  }
  nsAutoString hwndStr;
  hwndStr.AppendPrintf("%p", mMainWindowHandle);
  commandLine.AppendLooseValue(hwndStr.get());

  ScopedHandle procHandle(::OpenProcess(SYNCHRONIZE,
                                        TRUE,
                                        GetCurrentProcessId()));
  if (!procHandle.IsValid()) {
    return false;
  }
  nsAutoString procHandleStr;
  procHandleStr.AppendPrintf("%p", procHandle);
  commandLine.AppendLooseValue(procHandleStr.get());

  std::wstring ipcCookie;
  rv = mMiniShm.GetCookie(ipcCookie);
  if (NS_FAILED(rv)) {
    return false;
  }
  commandLine.AppendLooseValue(ipcCookie);

  mShowEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
  ScopedHandle showEvent(::CreateEvent(NULL, FALSE, FALSE, NULL));
  if (!showEvent.IsValid()) {
    return false;
  }
  mShowEvent = showEvent.Get();

  STARTUPINFO startupInfo = { sizeof(STARTUPINFO) };
  PROCESS_INFORMATION processInfo = { NULL };
  BOOL isProcessCreated = ::CreateProcess(exePath.value().c_str(),
                                          const_cast<wchar_t*>(commandLine.command_line_string().c_str()),
                                          nullptr,
                                          nullptr,
                                          TRUE,
                                          DETACHED_PROCESS,
                                          nullptr,
                                          nullptr,
                                          &startupInfo,
                                          &processInfo);
  if (isProcessCreated) {
    ::CloseHandle(processInfo.hThread);
    mHangUIProcessHandle = processInfo.hProcess;
    ::RegisterWaitForSingleObject(&mRegWait,
                                  processInfo.hProcess,
                                  &SOnHangUIProcessExit,
                                  this,
                                  INFINITE,
                                  WT_EXECUTEDEFAULT | WT_EXECUTEONLYONCE);
    ::WaitForSingleObject(mShowEvent, kTimeout);
  }
  mShowEvent = NULL;
  return !(!isProcessCreated);
}

// static
VOID CALLBACK PluginHangUIParent::SOnHangUIProcessExit(PVOID aContext,
                                                       BOOLEAN aIsTimer)
{
  PluginHangUIParent* object = static_cast<PluginHangUIParent*>(aContext);
  // If the Hang UI child process died unexpectedly, act as if the UI cancelled
  if (object->IsShowing()) {
    object->RecvUserResponse(HANGUI_USER_RESPONSE_CANCEL);
    // Firefox window was disabled automatically when the Hang UI was shown.
    // If plugin-hang-ui.exe was unexpectedly terminated, we need to re-enable.
    ::EnableWindow(object->mMainWindowHandle, TRUE);
  }
  object->mMiniShm.CleanUp();
}

bool
PluginHangUIParent::Cancel()
{
  bool result = mIsShowing && SendCancel();
  if (result) {
    mIsShowing = false;
  }
  return result;
}

bool
PluginHangUIParent::SendCancel()
{
  PluginHangUICommand* cmd = nullptr;
  nsresult rv = mMiniShm.GetWritePtr(cmd);
  if (NS_FAILED(rv)) {
    return false;
  }
  cmd->mCode = PluginHangUICommand::HANGUI_CMD_CANCEL;
  return NS_SUCCEEDED(mMiniShm.Send());
}

bool
PluginHangUIParent::RecvUserResponse(const unsigned int& aResponse)
{
  mLastUserResponse = aResponse;
  mResponseTicks = GetTickCount();
  mIsShowing = false;
  // responseCode: 1 = Stop, 2 = Continue, 3 = Cancel
  int responseCode;
  if (aResponse & HANGUI_USER_RESPONSE_STOP) {
    // User clicked Stop
    mModule->TerminateChildProcess(mMainThreadMessageLoop);
    responseCode = 1;
  } else if(aResponse & HANGUI_USER_RESPONSE_CONTINUE) {
    // User clicked Continue
    responseCode = 2;
  } else {
    // Dialog was cancelled
    responseCode = 3;
  }
  int dontAskCode = (aResponse & HANGUI_USER_RESPONSE_DONT_SHOW_AGAIN) ? 1 : 0;
  nsCOMPtr<nsIRunnable> workItem = new nsPluginHangUITelemetry(responseCode,
                                                               dontAskCode);
  NS_DispatchToMainThread(workItem, NS_DISPATCH_NORMAL);
  return true;
}

nsresult
PluginHangUIParent::GetHangUIOwnerWindowHandle(NativeWindowHandle& windowHandle)
{
  windowHandle = NULL;

  nsresult rv;
  nsCOMPtr<nsIWindowMediator> winMediator(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID,
                                                        &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMWindow> navWin;
  rv = winMediator->GetMostRecentWindow(NS_LITERAL_STRING("navigator:browser").get(),
                                        getter_AddRefs(navWin));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!navWin) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWidget> widget = WidgetUtils::DOMWindowToWidget(navWin);
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  windowHandle = reinterpret_cast<NativeWindowHandle>(widget->GetNativeData(NS_NATIVE_WINDOW));
  if (!windowHandle) {
    return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}

void
PluginHangUIParent::OnMiniShmEvent(MiniShmBase *aMiniShmObj)
{
  const PluginHangUIResponse* response = nullptr;
  nsresult rv = aMiniShmObj->GetReadPtr(response);
  NS_ASSERTION(NS_SUCCEEDED(rv),
               "Couldn't obtain read pointer OnMiniShmEvent");
  if (NS_SUCCEEDED(rv)) {
    RecvUserResponse(response->mResponseBits);
  }
}

void
PluginHangUIParent::OnMiniShmConnect(MiniShmBase* aMiniShmObj)
{
  PluginHangUICommand* cmd = nullptr;
  nsresult rv = aMiniShmObj->GetWritePtr(cmd);
  NS_ASSERTION(NS_SUCCEEDED(rv),
               "Couldn't obtain write pointer OnMiniShmConnect");
  if (NS_FAILED(rv)) {
    return;
  }
  cmd->mCode = PluginHangUICommand::HANGUI_CMD_SHOW;
  mIsShowing = NS_SUCCEEDED(aMiniShmObj->Send());
  if (mIsShowing) {
    mShowTicks = GetTickCount();
  }
  ::SetEvent(mShowEvent);
}

} // namespace plugins
} // namespace mozilla
