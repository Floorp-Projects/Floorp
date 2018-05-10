/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "LaunchUnelevated.h"

#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/mscom/COMApartmentRegion.h"
#include "mozilla/RefPtr.h"
#include "nsWindowsHelpers.h"

// For _bstr_t and _variant_t
#include <comdef.h>
#include <comutil.h>

#include <windows.h>
#include <exdisp.h>
#include <objbase.h>
#include <servprov.h>
#include <shlobj.h>
#include <shobjidl.h>

namespace mozilla {

// If we're running at an elevated integrity level, re-run ourselves at the
// user's normal integrity level. We do this by locating the active explorer
// shell, and then asking it to do a ShellExecute on our behalf. We do it this
// way to ensure that the child process runs as the original user in the active
// session; an elevated process could be running with different credentials than
// those of the session.
// See https://blogs.msdn.microsoft.com/oldnewthing/20131118-00/?p=2643

bool
LaunchUnelevated(int aArgc, wchar_t* aArgv[])
{
  // We require a single-threaded apartment to talk to Explorer.
  mscom::STARegion sta;
  if (!sta.IsValid()) {
    return false;
  }

  // NB: Explorer is a local server, not an inproc server
  RefPtr<IShellWindows> shellWindows;
  HRESULT hr = ::CoCreateInstance(CLSID_ShellWindows, nullptr,
                                  CLSCTX_LOCAL_SERVER, IID_IShellWindows,
                                  getter_AddRefs(shellWindows));
  if (FAILED(hr)) {
    return false;
  }

  // 1. Find the shell view for the desktop.
  _variant_t loc(CSIDL_DESKTOP);
  _variant_t empty;
  long hwnd;
  RefPtr<IDispatch> dispDesktop;
  hr = shellWindows->FindWindowSW(&loc, &empty, SWC_DESKTOP, &hwnd,
                                  SWFO_NEEDDISPATCH, getter_AddRefs(dispDesktop));
  if (FAILED(hr)) {
    return false;
  }

  RefPtr<IServiceProvider> servProv;
  hr = dispDesktop->QueryInterface(IID_IServiceProvider, getter_AddRefs(servProv));
  if (FAILED(hr)) {
    return false;
  }

  RefPtr<IShellBrowser> browser;
  hr = servProv->QueryService(SID_STopLevelBrowser, IID_IShellBrowser,
                              getter_AddRefs(browser));
  if (FAILED(hr)) {
    return false;
  }

  RefPtr<IShellView> activeShellView;
  hr = browser->QueryActiveShellView(getter_AddRefs(activeShellView));
  if (FAILED(hr)) {
    return false;
  }

  // 2. Get the automation object for the desktop.
  RefPtr<IDispatch> dispView;
  hr = activeShellView->GetItemObject(SVGIO_BACKGROUND, IID_IDispatch,
                                      getter_AddRefs(dispView));
  if (FAILED(hr)) {
    return false;
  }

  RefPtr<IShellFolderViewDual> folderView;
  hr = dispView->QueryInterface(IID_IShellFolderViewDual,
                                getter_AddRefs(folderView));
  if (FAILED(hr)) {
    return false;
  }

  // 3. Get the interface to IShellDispatch2
  RefPtr<IDispatch> dispShell;
  hr = folderView->get_Application(getter_AddRefs(dispShell));
  if (FAILED(hr)) {
    return false;
  }

  RefPtr<IShellDispatch2> shellDisp;
  hr = dispShell->QueryInterface(IID_IShellDispatch2, getter_AddRefs(shellDisp));
  if (FAILED(hr)) {
    return false;
  }

  // 4. Now call IShellDispatch2::ShellExecute to ask Explorer to re-launch us.

  // Omit argv[0] because ShellExecute doesn't need it in params
  UniquePtr<wchar_t[]> cmdLine(MakeCommandLine(aArgc - 1, aArgv + 1));
  if (!cmdLine) {
    return false;
  }

  _bstr_t exe(aArgv[0]);
  _variant_t args(cmdLine.get());
  _variant_t operation(L"open");
  _variant_t directory;
  _variant_t showCmd(SW_SHOWNORMAL);
  hr = shellDisp->ShellExecute(exe, args, operation, directory, showCmd);
  return SUCCEEDED(hr);
}

mozilla::Maybe<bool>
IsElevated()
{
  HANDLE rawToken;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &rawToken)) {
    return mozilla::Nothing();
  }

  nsAutoHandle token(rawToken);

  DWORD reqdLen;
  if (!::GetTokenInformation(token.get(), TokenIntegrityLevel, nullptr, 0,
                             &reqdLen) &&
      ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return mozilla::Nothing();
  }

  auto buf = mozilla::MakeUnique<char[]>(reqdLen);

  if (!::GetTokenInformation(token.get(), TokenIntegrityLevel, buf.get(),
                             reqdLen, &reqdLen)) {
    return mozilla::Nothing();
  }

  auto tokenLabel = reinterpret_cast<PTOKEN_MANDATORY_LABEL>(buf.get());

  DWORD subAuthCount = *::GetSidSubAuthorityCount(tokenLabel->Label.Sid);
  DWORD integrityLevel = *::GetSidSubAuthority(tokenLabel->Label.Sid,
                                               subAuthCount - 1);
  return mozilla::Some(integrityLevel > SECURITY_MANDATORY_MEDIUM_RID);
}

} // namespace mozilla

