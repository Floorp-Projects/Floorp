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
#include <algorithm>
#include <sstream>
#include <vector>

namespace mozilla {
namespace plugins {

struct WinInfo
{
  WinInfo(HWND aHwnd, POINT& aPos, SIZE& aSize)
    :hwnd(aHwnd)
  {
    pos.x = aPos.x;
    pos.y = aPos.y;
    size.cx = aSize.cx;
    size.cy = aSize.cy;
  }
  HWND  hwnd;
  POINT pos;
  SIZE  size;
};
typedef std::vector<WinInfo> WinInfoVec;

PluginHangUIChild* PluginHangUIChild::sSelf = nullptr;
const int PluginHangUIChild::kExpectedMinimumArgc = 10;

PluginHangUIChild::PluginHangUIChild()
  : mResponseBits(0),
    mParentWindow(nullptr),
    mDlgHandle(nullptr),
    mMainThread(nullptr),
    mParentProcess(nullptr),
    mRegWaitProcess(nullptr),
    mIPCTimeoutMs(0)
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
  // Only set the App User Model ID if it's present in the args
  if (wcscmp(aArgv[++i], L"-")) {
    HMODULE shell32 = LoadLibrary(L"shell32.dll");
    if (shell32) {
      SETAPPUSERMODELID fSetAppUserModelID = (SETAPPUSERMODELID)
        GetProcAddress(shell32, "SetCurrentProcessExplicitAppUserModelID");
      if (fSetAppUserModelID) {
        fSetAppUserModelID(aArgv[i]);
      }
      FreeLibrary(shell32);
    }
  }
  std::wistringstream issTimeout(aArgv[++i]);
  issTimeout >> mIPCTimeoutMs;
  if (!issTimeout) {
    return false;
  }

  nsresult rv = mMiniShm.Init(this,
                              std::wstring(aArgv[++i]),
                              IsDebuggerPresent() ? INFINITE : mIPCTimeoutMs);
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

void
PluginHangUIChild::ResizeButtons()
{
  // Control IDs are specified right-to-left as they appear in the dialog
  UINT ids[] = { IDC_STOP, IDC_CONTINUE };
  UINT numIds = sizeof(ids)/sizeof(ids[0]);

  // Pass 1: Compute the ideal size
  bool needResizing = false;
  SIZE idealSize = {0};
  WinInfoVec winInfo;
  for (UINT i = 0; i < numIds; ++i) {
    HWND wnd = GetDlgItem(mDlgHandle, ids[i]);
    if (!wnd) {
      return;
    }

    // Get the button's dimensions in screen coordinates
    RECT curRect;
    if (!GetWindowRect(wnd, &curRect)) {
      return;
    }

    // Get (x,y) position of the button in client coordinates
    POINT pt;
    pt.x = curRect.left;
    pt.y = curRect.top;
    if (!ScreenToClient(mDlgHandle, &pt)) {
      return;
    }

    // Request the button's text margins
    RECT margins;
    if (!Button_GetTextMargin(wnd, &margins)) {
      return;
    }

    // Compute the button's width and height
    SIZE curSize;
    curSize.cx = curRect.right - curRect.left;
    curSize.cy = curRect.bottom - curRect.top;

    // Request the button's ideal width and height and add in the margins
    SIZE size = {0};
    if (!Button_GetIdealSize(wnd, &size)) {
      return;
    }
    size.cx += margins.left + margins.right;
    size.cy += margins.top + margins.bottom;

    // Size all buttons to be the same width as the longest button encountered
    idealSize.cx = std::max(idealSize.cx, size.cx);
    idealSize.cy = std::max(idealSize.cy, size.cy);

    // We won't bother resizing unless we need extra space
    if (idealSize.cx > curSize.cx) {
      needResizing = true;
    }

    // Save the relevant info for the resize, if any. We do this even if 
    // needResizing is false because another button may trigger a resize later.
    winInfo.push_back(WinInfo(wnd, pt, curSize));
  }

  if (!needResizing) {
    return;
  }

  // Pass 2: Resize the windows
  int deltaX = 0;
  HDWP hwp = BeginDeferWindowPos((int) winInfo.size());
  if (!hwp) {
    return;
  }
  for (WinInfoVec::const_iterator itr = winInfo.begin();
       itr != winInfo.end(); ++itr) {
    // deltaX accumulates the size changes so that each button's x coordinate 
    // can compensate for the width increases
    deltaX += idealSize.cx - itr->size.cx;
    hwp = DeferWindowPos(hwp, itr->hwnd, nullptr,
                         itr->pos.x - deltaX, itr->pos.y,
                         idealSize.cx, itr->size.cy,
                         SWP_NOZORDER | SWP_NOACTIVATE);
    if (!hwp) {
      return;
    }
  }
  EndDeferWindowPos(hwp);
}

INT_PTR
PluginHangUIChild::HangUIDlgProc(HWND aDlgHandle, UINT aMsgCode, WPARAM aWParam,
                                 LPARAM aLParam)
{
  mDlgHandle = aDlgHandle;
  switch (aMsgCode) {
    case WM_INITDIALOG: {
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
      ResizeButtons();
      HANDLE icon = LoadImage(nullptr, IDI_QUESTION, IMAGE_ICON, 0, 0,
                              LR_DEFAULTSIZE | LR_SHARED);
      if (icon) {
        SendDlgItemMessage(aDlgHandle, IDC_DLGICON, STM_SETICON, (WPARAM)icon, 0);
      }
      EnableWindow(mParentWindow, FALSE);
      return TRUE;
    }
    case WM_CLOSE: {
      mResponseBits |= HANGUI_USER_RESPONSE_CANCEL;
      EndDialog(aDlgHandle, 0);
      SetWindowLongPtr(aDlgHandle, DWLP_MSGRESULT, 0);
      return TRUE;
    }
    case WM_COMMAND: {
      switch (LOWORD(aWParam)) {
        case IDC_CONTINUE:
          if (HIWORD(aWParam) == BN_CLICKED) {
            mResponseBits |= HANGUI_USER_RESPONSE_CONTINUE;
            EndDialog(aDlgHandle, 1);
            SetWindowLongPtr(aDlgHandle, DWLP_MSGRESULT, 0);
            return TRUE;
          }
          break;
        case IDC_STOP:
          if (HIWORD(aWParam) == BN_CLICKED) {
            mResponseBits |= HANGUI_USER_RESPONSE_STOP;
            EndDialog(aDlgHandle, 1);
            SetWindowLongPtr(aDlgHandle, DWLP_MSGRESULT, 0);
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
            SetWindowLongPtr(aDlgHandle, DWLP_MSGRESULT, 0);
            return TRUE;
          }
          break;
        default:
          break;
      }
      break;
    }
    case WM_DESTROY: {
      EnableWindow(mParentWindow, TRUE);
      SetForegroundWindow(mParentWindow);
      break;
    }
    default:
      break;
  }
  return FALSE;
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
  INT_PTR dlgResult = DialogBox(GetModuleHandle(nullptr),
                                MAKEINTRESOURCE(IDD_HANGUIDLG),
                                nullptr,
                                &SHangUIDlgProc);
  mDlgHandle = nullptr;
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
                                           mIPCTimeoutMs,
                                           TRUE);
  return waitResult == WAIT_OBJECT_0 ||
         waitResult == WAIT_IO_COMPLETION;
}

bool
PluginHangUIChild::SetMainThread()
{
  if (mMainThread) {
    CloseHandle(mMainThread);
    mMainThread = nullptr;
  }
  mMainThread = OpenThread(THREAD_SET_CONTEXT,
                           FALSE,
                           GetCurrentThreadId());
  return !(!mMainThread);
}

} // namespace plugins
} // namespace mozilla

#ifdef __MINGW32__
extern "C"
#endif
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

