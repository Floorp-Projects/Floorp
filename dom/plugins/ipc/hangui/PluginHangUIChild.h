/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_PluginHangUIChild_h
#define mozilla_plugins_PluginHangUIChild_h

#include "MiniShmChild.h"

#include <string>

#include <windows.h>

namespace mozilla {
namespace plugins {

/**
 * This class implements the plugin-hang-ui.
 *
 * NOTE: PluginHangUIChild is *not* an IPDL actor! In this case, "Child" 
 *       is describing the fact that plugin-hang-ui is a child process to the 
 *       firefox process, which is the PluginHangUIParent.
 *       PluginHangUIParent and PluginHangUIChild are a matched pair.
 * @see PluginHangUIParent
 */
class PluginHangUIChild : public MiniShmObserver
{
public:
  PluginHangUIChild();
  virtual ~PluginHangUIChild();

  bool
  Init(int aArgc, wchar_t* aArgv[]);

  /**
   * Displays the Plugin Hang UI and does not return until the UI has 
   * been dismissed.
   *
   * @return true if the UI was displayed and the user response was 
   *         successfully sent back to the parent. Otherwise false.
   */
  bool
  Show();

  /**
   * Causes the calling thread to wait either for the Hang UI to be 
   * dismissed or for the parent process to terminate. This should 
   * be called by the main thread.
   *
   * @return true unless there was an error initiating the wait
   */
  bool
  WaitForDismissal();

  virtual void
  OnMiniShmEvent(MiniShmBase* aMiniShmObj) MOZ_OVERRIDE;

private:
  bool
  RecvShow();

  bool
  RecvCancel();

  bool
  SetMainThread();

  void
  ResizeButtons();

  INT_PTR
  HangUIDlgProc(HWND aDlgHandle, UINT aMsgCode, WPARAM aWParam, LPARAM aLParam);

  static VOID CALLBACK
  ShowAPC(ULONG_PTR aContext);

  static INT_PTR CALLBACK
  SHangUIDlgProc(HWND aDlgHandle, UINT aMsgCode, WPARAM aWParam,
                 LPARAM aLParam);

  static VOID CALLBACK
  SOnParentProcessExit(PVOID aObject, BOOLEAN aIsTimer);

  static PluginHangUIChild *sSelf;

  const wchar_t* mMessageText;
  const wchar_t* mWindowTitle;
  const wchar_t* mWaitBtnText;
  const wchar_t* mKillBtnText;
  const wchar_t* mNoFutureText;
  unsigned int mResponseBits;
  HWND mParentWindow;
  HWND mDlgHandle;
  HANDLE mMainThread;
  HANDLE mParentProcess;
  HANDLE mRegWaitProcess;
  MiniShmChild mMiniShm;

  static const int kExpectedMinimumArgc;
  static const DWORD kProcessTimeout;
  static const DWORD kShmTimeout;

  typedef HRESULT (WINAPI *SETAPPUSERMODELID)(PCWSTR);

  DISALLOW_COPY_AND_ASSIGN(PluginHangUIChild);
};

} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_PluginHangUIChild_h

