/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// vrhostapi.cpp
// Definition of functions that are exported from this dll

#include "vrhostex.h"
#include "VRShMem.h"

#include <stdio.h>
#include <string.h>
#include <random>

#include "windows.h"

// VRWindowManager adds a level of indirection so that system HWND isn't exposed
// outside of these APIs
class VRWindowManager {
 public:
  HWND GetHWND(uint32_t nId) {
    if (nId == nWindow) {
      return hWindow;
    } else {
      return nullptr;
    }
  }

  HANDLE GetProc(uint32_t nId) {
    if (nId == nWindow) {
      return hProc;
    } else {
      return nullptr;
    }
  }

  uint32_t SetHWND(HWND hwnd, HANDLE hproc) {
    if (hWindow == nullptr) {
      MOZ_ASSERT(hwnd != nullptr && hproc != nullptr);
      hWindow = hwnd;
      hProc = hproc;
      nWindow = GetRandomUInt();
#if defined(DEBUG) && defined(NIGHTLY_BUILD)
      printf("VRWindowManager: Storing HWND: 0x%p as ID: 0x%X\n", hWindow,
             nWindow);
#endif
      return nWindow;
    } else {
      return -1;
    }
  }

  uint32_t GetRandomUInt() { return randomGenerator(); }

  static VRWindowManager* GetManager() {
    if (Instance == nullptr) {
      Instance = new VRWindowManager();
    }
    return Instance;
  }

 private:
  static VRWindowManager* Instance;

  // For now, simply store the ID and HWND, and expand
  // to a map when multiple windows/instances are supported.
  uint32_t nWindow = 0;
  HWND hWindow = nullptr;
  HANDLE hProc = nullptr;
  std::random_device randomGenerator;
};
VRWindowManager* VRWindowManager::Instance = nullptr;

// Struct to send params to StartFirefoxThreadProc
struct StartFirefoxParams {
  char* firefoxFolder;
  char* firefoxProfileFolder;
  HANDLE hProcessFx;
};

// Helper threadproc function for CreateVRWindow
DWORD StartFirefoxThreadProc(_In_ LPVOID lpParameter) {
  char cmd[] = "%sfirefox.exe -wait-for-browser -profile %s --fxr";

  StartFirefoxParams* params = static_cast<StartFirefoxParams*>(lpParameter);
  char cmdWithPath[MAX_PATH + MAX_PATH] = {0};
  int err = sprintf_s(cmdWithPath, ARRAYSIZE(cmdWithPath), cmd,
                      params->firefoxFolder, params->firefoxProfileFolder);

  if (err != -1) {
    PROCESS_INFORMATION procFx = {0};
    STARTUPINFO startupInfoFx = {0};

#if defined(DEBUG) && defined(NIGHTLY_BUILD)
    printf("Starting Firefox via: %s\n", cmdWithPath);
#endif

    // Start Firefox
    bool fCreateContentProc = ::CreateProcess(nullptr,  // lpApplicationName,
                                              cmdWithPath,
                                              nullptr,  // lpProcessAttributes,
                                              nullptr,  // lpThreadAttributes,
                                              TRUE,     // bInheritHandles,
                                              0,        // dwCreationFlags,
                                              nullptr,  // lpEnvironment,
                                              nullptr,  // lpCurrentDirectory,
                                              &startupInfoFx, &procFx);

    if (!fCreateContentProc) {
      printf("Failed to create Firefox process");
    }

    params->hProcessFx = procFx.hProcess;
  }

  return 0;
}

// This export is responsible for starting up a new VR window in Firefox and
// returning data related to its creation back to the caller.
void CreateVRWindow(char* firefoxFolderPath, char* firefoxProfilePath,
                    uint32_t dxgiAdapterID, uint32_t widthHost,
                    uint32_t heightHost, uint32_t* windowId, void** hTex,
                    uint32_t* width, uint32_t* height) {
  mozilla::gfx::VRWindowState windowState = {0};

  int err = sprintf_s(windowState.signalName, ARRAYSIZE(windowState.signalName),
                      "fxr::CreateVRWindow::%X",
                      VRWindowManager::GetManager()->GetRandomUInt());

  if (err > 0) {
    HANDLE hEvent = ::CreateEventA(nullptr,  // attributes
                                   TRUE,     // bManualReset
                                   FALSE,    // bInitialState
                                   windowState.signalName);

    if (hEvent != nullptr) {
      // Create Shmem and push state
      mozilla::gfx::VRShMem shmem(nullptr, true /*aRequiresMutex*/);
      shmem.CreateShMem(true /*aCreateOnSharedMemory*/);
      shmem.PushWindowState(windowState);

      // Start Firefox in another thread so that this thread can wait for the
      // window state to be updated during Firefox startup
      StartFirefoxParams fxParams = {0};
      fxParams.firefoxFolder = firefoxFolderPath;
      fxParams.firefoxProfileFolder = firefoxProfilePath;
      DWORD dwTid = 0;
      HANDLE hThreadFx = CreateThread(nullptr, 0, StartFirefoxThreadProc,
                                      &fxParams, 0, &dwTid);
      if (hThreadFx != nullptr) {
        // Wait for Firefox to populate rest of window state
        ::WaitForSingleObject(hEvent, INFINITE);

        // Update local WindowState with data from Firefox
        shmem.PullWindowState(windowState);

        (*hTex) = windowState.textureFx;
        (*windowId) = VRWindowManager::GetManager()->SetHWND(
            (HWND)windowState.hwndFx, fxParams.hProcessFx);
        (*width) = windowState.widthFx;
        (*height) = windowState.heightFx;

        // Neither the Shmem nor its window state are needed anymore
        windowState = {0};
        shmem.PushWindowState(windowState);
      } else {
        // How do I failfast?
      }

      shmem.CloseShMem();
    }
  }
}

// Sends a message to the VR window to close.
void CloseVRWindow(uint32_t nVRWindowID, bool waitForTerminate) {
  HWND hwnd = VRWindowManager::GetManager()->GetHWND(nVRWindowID);
  if (hwnd != nullptr) {
    ::SendMessage(hwnd, WM_CLOSE, 0, 0);

    if (waitForTerminate) {
      // Wait for Firefox main process to exit
      ::WaitForSingleObject(VRWindowManager::GetManager()->GetProc(nVRWindowID),
                            INFINITE);
    }
  }
}

// Forwards Win32 UI window messages to the Firefox widget/window associated
// with nVRWindowID. Note that not all message type is supported (only those
// allowed in the switch block below).
void SendUIMessageToVRWindow(uint32_t nVRWindowID, uint32_t msg,
                             uint64_t wparam, uint64_t lparam) {
  HWND hwnd = VRWindowManager::GetManager()->GetHWND(nVRWindowID);
  if (hwnd != nullptr) {
    switch (msg) {
      case WM_MOUSEMOVE:
      case WM_LBUTTONDOWN:
      case WM_LBUTTONUP:
      case WM_MOUSEWHEEL:
      case WM_CHAR:
        ::PostMessage(hwnd, msg, wparam, lparam);
        break;

      default:
        break;
    }
  }
}