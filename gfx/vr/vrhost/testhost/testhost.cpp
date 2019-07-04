/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include "vrhost/vrhostex.h"

int main(int argc, char* argv[], char* envp[]) {
  HINSTANCE hVR = ::LoadLibrary("vrhost.dll");
  if (hVR != nullptr) {
    PFN_SAMPLE lpfnSample = (PFN_SAMPLE)GetProcAddress(hVR, "SampleExport");
    if (lpfnSample != nullptr) {
      lpfnSample();
    }

    if (strcmp(argv[1], "-testsvc") == 0) {
      PFN_SAMPLE lpfnService =
          (PFN_SAMPLE)GetProcAddress(hVR, "TestTheService");
      lpfnService();
    } else if (strcmp(argv[1], "-testmgr") == 0) {
      PFN_SAMPLE lpfnManager =
          (PFN_SAMPLE)GetProcAddress(hVR, "TestTheManager");
      lpfnManager();
    }

    ::FreeLibrary(hVR);
    hVR = nullptr;
  }

  return 0;
}