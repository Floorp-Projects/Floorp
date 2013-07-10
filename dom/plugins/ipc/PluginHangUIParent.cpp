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
#include "nsIWinTaskbar.h"
#include "nsServiceManagerUtils.h"

#include "WidgetUtils.h"

#define NS_TASKBAR_CONTRACTID "@mozilla.org/windows-taskbar;1"

using base::ProcessHandle;

using mozilla::widget::WidgetUtils;

using std::string;
using std::vector;

namespace {
class nsPluginHangUITelemetry : public nsRunnable
{
public:
  nsPluginHangUITelemetry(int aResponseCode, int aDontAskCode,
                          uint32_t aResponseTimeMs, uint32_t aTimeoutMs)
    : mResponseCode(aResponseCode),
      mDontAskCode(aDontAskCode),
      mResponseTimeMs(aResponseTimeMs),
      mTimeoutMs(aTimeoutMs)
  {
  }

  NS_IMETHOD
  Run()
  {
    mozilla::Telemetry::Accumulate(
              mozilla::Telemetry::PLUGIN_HANG_UI_USER_RESPONSE, mResponseCode);
    mozilla::Telemetry::Accumulate(
              mozilla::Telemetry::PLUGIN_HANG_UI_DONT_ASK, mDontAskCode);
    mozilla::Telemetry::Accumulate(
              mozilla::Telemetry::PLUGIN_HANG_UI_RESPONSE_TIME, mResponseTimeMs);
    mozilla::Telemetry::Accumulate(
              mozilla::Telemetry::PLUGIN_HANG_TIME, mTimeoutMs + mResponseTimeMs);
    return NS_OK;
  }

private:
  int mResponseCode;
  int mDontAskCode;
  uint32_t mResponseTimeMs;
  uint32_t mTimeoutMs;
};
} // anonymous namespace

namespace mozilla {
namespace plugins {

PluginHangUIParent::PluginHangUIParent(PluginModuleParent* aModule,
                                       const int32_t aHangUITimeoutPref,
                                       const int32_t aChildTimeoutPref)
  : mMutex("mozilla::plugins::PluginHangUIParent::mMutex"),
    mModule(aModule),
    mTimeoutPrefMs(static_cast<uint32_t>(aHangUITimeoutPref) * 1000U),
    mIPCTimeoutMs(static_cast<uint32_t>(aChildTimeoutPref) * 1000U),
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
  { // Scope for lock
    MutexAutoLock lock(mMutex);
    UnwatchHangUIChildProcess(true);
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
  rv = mMiniShm.Init(this, ::IsDebuggerPresent() ? INFINITE : mIPCTimeoutMs);
  NS_ENSURE_SUCCESS(rv, false);
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
  procHandleStr.AppendPrintf("%p", procHandle.Get());
  commandLine.AppendLooseValue(procHandleStr.get());

  // On Win7+, pass the application user model to the child, so it can
  // register with it. This insures windows created by the Hang UI
  // properly group with the parent app on the Win7 taskbar.
  nsCOMPtr<nsIWinTaskbar> taskbarInfo = do_GetService(NS_TASKBAR_CONTRACTID);
  if (taskbarInfo) {
    bool isSupported = false;
    taskbarInfo->GetAvailable(&isSupported);
    nsAutoString appId;
    if (isSupported && NS_SUCCEEDED(taskbarInfo->GetDefaultGroupId(appId))) {
      commandLine.AppendLooseValue(appId.get());
    } else {
      commandLine.AppendLooseValue(L"-");
    }
  } else {
    commandLine.AppendLooseValue(L"-");
  }

  nsAutoString ipcTimeoutStr;
  ipcTimeoutStr.AppendInt(mIPCTimeoutMs);
  commandLine.AppendLooseValue(ipcTimeoutStr.get());

  std::wstring ipcCookie;
  rv = mMiniShm.GetCookie(ipcCookie);
  if (NS_FAILED(rv)) {
    return false;
  }
  commandLine.AppendLooseValue(ipcCookie);

  ScopedHandle showEvent(::CreateEvent(NULL, FALSE, FALSE, NULL));
  if (!showEvent.IsValid()) {
    return false;
  }
  mShowEvent = showEvent.Get();

  MutexAutoLock lock(mMutex);
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
    ::WaitForSingleObject(mShowEvent, ::IsDebuggerPresent() ? INFINITE
                                                            : mIPCTimeoutMs);
    // Setting this to true even if we time out on mShowEvent. This timeout 
    // typically occurs when the machine is thrashing so badly that 
    // plugin-hang-ui.exe is taking a while to start. If we didn't set 
    // this to true, Firefox would keep spawning additional plugin-hang-ui 
    // processes, which is not what we want.
    mIsShowing = true;
  }
  mShowEvent = NULL;
  return !(!isProcessCreated);
}

// static
VOID CALLBACK PluginHangUIParent::SOnHangUIProcessExit(PVOID aContext,
                                                       BOOLEAN aIsTimer)
{
  PluginHangUIParent* object = static_cast<PluginHangUIParent*>(aContext);
  MutexAutoLock lock(object->mMutex);
  // If the Hang UI child process died unexpectedly, act as if the UI cancelled
  if (object->IsShowing()) {
    object->RecvUserResponse(HANGUI_USER_RESPONSE_CANCEL);
    // Firefox window was disabled automatically when the Hang UI was shown.
    // If plugin-hang-ui.exe was unexpectedly terminated, we need to re-enable.
    ::EnableWindow(object->mMainWindowHandle, TRUE);
  }
}

// A precondition for this function is that the caller has locked mMutex
bool
PluginHangUIParent::UnwatchHangUIChildProcess(bool aWait)
{
  mMutex.AssertCurrentThreadOwns();
  if (mRegWait) {
    // If aWait is false then we want to pass a NULL (i.e. default constructor)
    // completionEvent
    ScopedHandle completionEvent;
    if (aWait) {
      completionEvent.Set(::CreateEvent(NULL, FALSE, FALSE, NULL));
      if (!completionEvent.IsValid()) {
        return false;
      }
    }

    // if aWait == false and UnregisterWaitEx fails with ERROR_IO_PENDING,
    // it is okay to clear mRegWait; Windows is telling us that the wait's
    // callback is running but will be cleaned up once the callback returns.
    if (::UnregisterWaitEx(mRegWait, completionEvent) ||
        !aWait && ::GetLastError() == ERROR_IO_PENDING) {
      mRegWait = NULL;
      if (aWait) {
        // We must temporarily unlock mMutex while waiting for the registered
        // wait callback to complete, or else we could deadlock.
        MutexAutoUnlock unlock(mMutex);
        ::WaitForSingleObject(completionEvent, INFINITE);
      }
      return true;
    }
  }
  return false;
}

bool
PluginHangUIParent::Cancel()
{
  MutexAutoLock lock(mMutex);
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

// A precondition for this function is that the caller has locked mMutex
bool
PluginHangUIParent::RecvUserResponse(const unsigned int& aResponse)
{
  mMutex.AssertCurrentThreadOwns();
  if (!mIsShowing && !(aResponse & HANGUI_USER_RESPONSE_CANCEL)) {
    // Don't process a user response if a cancellation is already pending
    return true;
  }
  mLastUserResponse = aResponse;
  mResponseTicks = ::GetTickCount();
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
                                                               dontAskCode,
                                                               LastShowDurationMs(),
                                                               mTimeoutPrefMs);
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
    // The child process has returned a response so we shouldn't worry about 
    // its state anymore.
    MutexAutoLock lock(mMutex);
    UnwatchHangUIChildProcess(false);
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
  if (NS_SUCCEEDED(aMiniShmObj->Send())) {
    mShowTicks = ::GetTickCount();
  }
  ::SetEvent(mShowEvent);
}

} // namespace plugins
} // namespace mozilla
