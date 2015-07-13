/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(XP_WIN)
#include <d3d9.h> // needed to prevent re-definition of enums
#include <stdio.h>
#include <string>
#include <vector>
#include <windows.h>

#include "opmapi.h"
#endif

namespace mozilla {
namespace gmptest {

#if defined(XP_WIN)
typedef HRESULT(STDAPICALLTYPE * OPMGetVideoOutputsFromHMONITORProc)
          (HMONITOR, OPM_VIDEO_OUTPUT_SEMANTICS, ULONG*, IOPMVideoOutput***);

static OPMGetVideoOutputsFromHMONITORProc sOPMGetVideoOutputsFromHMONITORProc = nullptr;

static BOOL CALLBACK EnumDisplayMonitorsCallback(HMONITOR hMonitor, HDC hdc,
                                                 LPRECT lprc, LPARAM pData)
{
  std::vector<std::string>* failureMsgs = (std::vector<std::string>*)pData;

  MONITORINFOEXA miex;
  ZeroMemory(&miex, sizeof(miex));
  miex.cbSize = sizeof(miex);
  if (!GetMonitorInfoA(hMonitor, &miex)) {
    failureMsgs->push_back("FAIL GetMonitorInfoA call failed");
  }

  DISPLAY_DEVICEA dd;
  ZeroMemory(&dd, sizeof(dd));
  dd.cb = sizeof(dd);
  if (!EnumDisplayDevicesA(miex.szDevice, 0, &dd, 1)) {
    failureMsgs->push_back("FAIL EnumDisplayDevicesA call failed");
  }

  ULONG numVideoOutputs = 0;
  IOPMVideoOutput** opmVideoOutputArray = nullptr;
  HRESULT hr = sOPMGetVideoOutputsFromHMONITORProc(hMonitor,
                                                   OPM_VOS_OPM_SEMANTICS,
                                                   &numVideoOutputs,
                                                   &opmVideoOutputArray);
  if (S_OK != hr) {
    if (0x8007001f != hr && 0x80070032 != hr) {
      char msg[100];
      sprintf(msg, "FAIL OPMGetVideoOutputsFromHMONITOR call failed: HRESULT=0x%08x", hr);
      failureMsgs->push_back(msg);
    }
    return true;
  }

  for (ULONG i = 0; i < numVideoOutputs; ++i) {
    OPM_RANDOM_NUMBER opmRandomNumber;
    BYTE* certificate = nullptr;
    ULONG certificateLength = 0;
    hr = opmVideoOutputArray[i]->StartInitialization(&opmRandomNumber,
                                                     &certificate,
                                                     &certificateLength);
    if (S_OK != hr) {
      char msg[100];
      sprintf(msg, "FAIL StartInitialization call failed: HRESULT=0x%08x", hr);
      failureMsgs->push_back(msg);
    }

    if (certificate) {
      CoTaskMemFree(certificate);
    }

    opmVideoOutputArray[i]->Release();
  }

  if (opmVideoOutputArray) {
    CoTaskMemFree(opmVideoOutputArray);
  }

  return true;
}
#endif

static void
RunOutputProtectionAPITests()
{
#if defined(XP_WIN)
  // Get hold of OPMGetVideoOutputsFromHMONITOR function.
  HMODULE hDvax2DLL = GetModuleHandleW(L"dxva2.dll");
  if (!hDvax2DLL) {
    FakeDecryptor::Message("FAIL GetModuleHandleW call failed for dxva2.dll");
    return;
  }

  sOPMGetVideoOutputsFromHMONITORProc = (OPMGetVideoOutputsFromHMONITORProc)
    GetProcAddress(hDvax2DLL, "OPMGetVideoOutputsFromHMONITOR");
  if (!sOPMGetVideoOutputsFromHMONITORProc) {
    FakeDecryptor::Message("FAIL GetProcAddress call failed for OPMGetVideoOutputsFromHMONITOR");
    return;
  }

  // Test EnumDisplayMonitors.
  // Other APIs are tested in the callback function.
  std::vector<std::string> failureMsgs;
  if (!EnumDisplayMonitors(NULL, NULL, EnumDisplayMonitorsCallback,
                           (LPARAM) &failureMsgs)) {
    FakeDecryptor::Message("FAIL EnumDisplayMonitors call failed");
  }

  // Report any failures in the callback function.
  for (size_t i = 0; i < failureMsgs.size(); i++) {
    FakeDecryptor::Message(failureMsgs[i]);
  }
#endif
}

static void
TestOuputProtectionAPIs()
{
  RunOutputProtectionAPITests();
  FakeDecryptor::Message("OP tests completed");
  return;
}

} // namespace gmptest
} // namespace mozilla
