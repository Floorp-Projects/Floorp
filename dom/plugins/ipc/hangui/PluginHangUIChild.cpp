/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginHangUI.h"

#include "PluginHangUIChild.h"
#include "HangUIDlg.h"

#include <assert.h>
#include <commctrl.h>
#include <windowsx.h>
#include <sstream>

namespace mozilla {
namespace plugins {

PluginHangUIChild* PluginHangUIChild::sSelf = nullptr;
const int PluginHangUIChild::kExpectedMinimumArgc = 9;
const DWORD PluginHangUIChild::kProcessTimeout = 1200000U;
const DWORD PluginHangUIChild::kShmTimeout = 5000U;

PluginHangUIChild::PluginHangUIChild()
  : mResponseBits(0),
    mParentWindow(NULL),
    mDlgHandle(NULL),
    mMainThread(NULL),
    mParentProcess(NULL),
    mRegWaitProcess(NULL)
{
}

PluginHangUIChild::~PluginHangUIChild()
{
  if (mMainThread) {
    CloseHandle(mMainThread);
  }
  if (mRegWaitProcess) {
    UnregisterWaitEx(mRegWaitProcess, INVALID_HANDLE_VALUE);
  }
  if (mParentProcess) {
    CloseHandle(mParentProcess);
  }
  sSelf = nullptr;
}

bool
PluginHangUIChild::Init(int aArgc, wchar_t* aArgv[])
{
  if (aArgc < kExpectedMinimumArgc) {
    return false;
  }
  unsigned int i = 1;
  mMessageText = aArgv[i];
  mWindowTitle = aArgv[++i];
  mWaitBtnText = aArgv[++i];
  mKillBtnText = aArgv[++i];
  mNoFutureText = aArgv[++i];
  std::wistringstream issHwnd(aArgv[++i]);
  issHwnd >> reinterpret_cast<HANDLE&>(mParentWindow);
  if (!issHwnd) {
    return false;
  }
  std::wistringstream issProc(aArgv[++i]);
  issProc >> mParentProcess;
  if (!issProc) {
    return false;
  }

  nsresult rv = mMiniShm.Init(this,
                              std::wstring(aArgv[++i]),
                              IsDebuggerPresent() ? INFINITE : kShmTimeout);
  if (NS_FAILED(rv)) {
    return false;
  }
  sSelf = this;
  return true;
}

void
PluginHangUIChild::OnMiniShmEvent(MiniShmBase* aMiniShmObj)
{
  const PluginHangUICommand* cmd = nullptr;
  nsresult rv = aMiniShmObj->GetReadPtr(cmd);
  assert(NS_SUCCEEDED(rv));
  bool returnStatus = false;
  if (NS_SUCCEEDED(rv)) {
    switch (cmd->mCode) {
      case PluginHangUICommand::HANGUI_CMD_SHOW:
        returnStatus = RecvShow();
        break;
      case PluginHangUICommand::HANGUI_CMD_CANCEL:
        returnStatus = RecvCancel();
        break;
      default:
        break;
    }
  }
}

// static
INT_PTR CALLBACK
PluginHangUIChild::SHangUIDlgProc(HWND aDlgHandle, UINT aMsgCode,
                                  WPARAM aWParam, LPARAM aLParam)
{
  PluginHangUIChild *self = PluginHangUIChild::sSelf;
  if (self) {
    return self->HangUIDlgProc(aDlgHandle, aMsgCode, aWParam, aLParam);
  }
  return FALSE;
}

INT_PTR
PluginHangUIChild::HangUIDlgProc(HWND aDlgHandle, UINT aMsgCode, WPARAM aWParam,
                                 LPARAM aLParam)
{
  mDlgHandle = aDlgHandle;
  switch (aMsgCode) {
    case WM_INITDIALOG: {
      // Disentangle our input queue from the hung Firefox process
      AttachThreadInput(GetCurrentThreadId(),
                        GetWindowThreadProcessId(mParentWindow, nullptr),
                        FALSE);
      // Register a wait on the Firefox process so that we will be informed
      // if it dies while the dialog is showing
      RegisterWaitForSingleObject(&mRegWaitProcess,
                                  mParentProcess,
                                  &SOnParentProcessExit,
                                  this,
                                  INFINITE,
                                  WT_EXECUTEDEFAULT | WT_EXECUTEONLYONCE);
      SetWindowText(aDlgHandle, mWindowTitle);
      SetDlgItemText(aDlgHandle, IDC_MSG, mMessageText);
      SetDlgItemText(aDlgHandle, IDC_NOFUTURE, mNoFutureText);
      SetDlgItemText(aDlgHandle, IDC_CONTINUE, mWaitBtnText);
      SetDlgItemText(aDlgHandle, IDC_STOP, mKillBtnText);
      HANDLE icon = LoadImage(NULL, IDI_QUESTION, IMAGE_ICON, 0, 0,
                              LR_DEFAULTSIZE | LR_SHARED);
      if (icon) {
        SendDlgItemMessage(aDlgHandle, IDC_DLGICON, STM_SETICON, (WPARAM)icon, 0);
      }
      return TRUE;
    }
    case WM_CLOSE: {
      mResponseBits |= HANGUI_USER_RESPONSE_CANCEL;
      EndDialog(aDlgHandle, 0);
      return TRUE;
    }
    case WM_COMMAND: {
      switch (LOWORD(aWParam)) {
        case IDC_CONTINUE:
          if (HIWORD(aWParam) == BN_CLICKED) {
            mResponseBits |= HANGUI_USER_RESPONSE_CONTINUE;
            EndDialog(aDlgHandle, 1);
            return TRUE;
          }
          break;
        case IDC_STOP:
          if (HIWORD(aWParam) == BN_CLICKED) {
            mResponseBits |= HANGUI_USER_RESPONSE_STOP;
            EndDialog(aDlgHandle, 1);
            return TRUE;
          }
          break;
        case IDC_NOFUTURE:
          if (HIWORD(aWParam) == BN_CLICKED) {
            if (Button_GetCheck(GetDlgItem(aDlgHandle,
                                           IDC_NOFUTURE)) == BST_CHECKED) {
              mResponseBits |= HANGUI_USER_RESPONSE_DONT_SHOW_AGAIN;
            } else {
              mResponseBits &=
                ~static_cast<DWORD>(HANGUI_USER_RESPONSE_DONT_SHOW_AGAIN);
            }
            return TRUE;
          }
        default:
          break;
      }
      return FALSE;
    }
    default:
      return FALSE;
  }
}

// static
VOID CALLBACK
PluginHangUIChild::SOnParentProcessExit(PVOID aObject, BOOLEAN aIsTimer)
{
  // Simulate a cancel if the parent process died
  PluginHangUIChild* object = static_cast<PluginHangUIChild*>(aObject);
  object->RecvCancel();
}

bool
PluginHangUIChild::RecvShow()
{
  return (QueueUserAPC(&ShowAPC,
                       mMainThread,
                       reinterpret_cast<ULONG_PTR>(this)));
}

bool
PluginHangUIChild::Show()
{
  INT_PTR dlgResult = DialogBox(GetModuleHandle(NULL),
                                MAKEINTRESOURCE(IDD_HANGUIDLG),
                                mParentWindow,
                                &SHangUIDlgProc);
  mDlgHandle = NULL;
  assert(dlgResult != -1);
  bool result = false;
  if (dlgResult != -1) {
    PluginHangUIResponse* response = nullptr;
    nsresult rv = mMiniShm.GetWritePtr(response);
    if (NS_SUCCEEDED(rv)) {
      response->mResponseBits = mResponseBits;
      result = NS_SUCCEEDED(mMiniShm.Send());
    }
  }
  return result;
}

// static
VOID CALLBACK
PluginHangUIChild::ShowAPC(ULONG_PTR aContext)
{
  PluginHangUIChild* object = reinterpret_cast<PluginHangUIChild*>(aContext);
  object->Show();
}

bool
PluginHangUIChild::RecvCancel()
{
  if (mDlgHandle) {
    PostMessage(mDlgHandle, WM_CLOSE, 0, 0);
  }
  return true;
}

bool
PluginHangUIChild::WaitForDismissal()
{
  if (!SetMainThread()) {
    return false;
  }
  DWORD waitResult = WaitForSingleObjectEx(mParentProcess,
                                           kProcessTimeout,
                                           TRUE);
  return waitResult == WAIT_OBJECT_0 ||
         waitResult == WAIT_IO_COMPLETION;
}

bool
PluginHangUIChild::SetMainThread()
{
  if (mMainThread) {
    CloseHandle(mMainThread);
    mMainThread = NULL;
  }
  mMainThread = OpenThread(THREAD_SET_CONTEXT,
                           FALSE,
                           GetCurrentThreadId());
  return !(!mMainThread);
}

} // namespace plugins
} // namespace mozilla

int
wmain(int argc, wchar_t *argv[])
{
  INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX),
                               ICC_STANDARD_CLASSES };
  if (!InitCommonControlsEx(&icc)) {
    return 1;
  }
  mozilla::plugins::PluginHangUIChild hangui;
  if (!hangui.Init(argc, argv)) {
    return 1;
  }
  if (!hangui.WaitForDismissal()) {
    return 1;
  }
  return 0;
}

