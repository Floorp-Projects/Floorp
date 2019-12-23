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
#include <queue>

#include "windows.h"

class VRShmemInstance {
 public:
  VRShmemInstance() = delete;
  VRShmemInstance(const VRShmemInstance& aRHS) = delete;

  static mozilla::gfx::VRShMem& GetInstance() {
    static mozilla::gfx::VRShMem shmem(nullptr, true /*aRequiresMutex*/);
    return shmem;
  }
};

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

  uint32_t GetId(HWND hwnd) {
    if (hwnd == hWindow) {
      return nWindow;
    } else {
      return 0;
    }
  }

  HANDLE GetProc(uint32_t nId) {
    if (nId == nWindow) {
      return hProc;
    } else {
      return nullptr;
    }
  }

  HANDLE GetEvent() { return hEvent; }

  uint32_t SetHWND(HWND hwnd, HANDLE hproc, HANDLE hevent) {
    if (hWindow == nullptr) {
      MOZ_ASSERT(hwnd != nullptr && hproc != nullptr);
      hWindow = hwnd;
      hProc = hproc;
      hEvent = hevent;
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
  HANDLE hEvent = nullptr;
  std::random_device randomGenerator;
};
VRWindowManager* VRWindowManager::Instance = nullptr;

class VRTelemetryManager {
 public:
  void SendTelemetry(uint32_t aTelemetryId, uint32_t aValue) {
    if (!aTelemetryId) {
      return;
    }

    mozilla::gfx::VRTelemetryState telemetryState = {0};
    VRShmemInstance::GetInstance().PullTelemetryState(telemetryState);

    if (telemetryState.uid == 0) {
      telemetryState.uid = sUid;
    }

    switch (mozilla::gfx::VRTelemetryId(aTelemetryId)) {
      case mozilla::gfx::VRTelemetryId::INSTALLED_FROM:
        MOZ_ASSERT(aValue <= 0x07,
                   "VRTelemetryId::INSTALLED_FROM only allows 3 bits.");
        telemetryState.installedFrom = true;
        telemetryState.installedFromValue = aValue;
        break;
      case mozilla::gfx::VRTelemetryId::ENTRY_METHOD:
        MOZ_ASSERT(aValue <= 0x07,
                   "VRTelemetryId::ENTRY_METHOD only allows 3 bits.");
        telemetryState.entryMethod = true;
        telemetryState.entryMethodValue = aValue;
        break;
      case mozilla::gfx::VRTelemetryId::FIRST_RUN:
        MOZ_ASSERT(aValue <= 0x01,
                   "VRTelemetryId::FIRST_RUN only allows 1 bit.");
        telemetryState.firstRun = true;
        telemetryState.firstRunValue = aValue;
        break;
      default:
        MOZ_CRASH("Undefined VR telemetry type.");
        break;
    }
    VRShmemInstance::GetInstance().PushTelemetryState(telemetryState);
    ++sUid;
  }

  static VRTelemetryManager* GetManager() {
    if (Instance == nullptr) {
      Instance = new VRTelemetryManager();
    }
    return Instance;
  }

 private:
  static VRTelemetryManager* Instance;
  static uint32_t sUid;  // It starts from 1, Zero means the data is read yet
                         // from VRManager.
};
uint32_t VRTelemetryManager::sUid = 1;
VRTelemetryManager* VRTelemetryManager::Instance = nullptr;

// Struct to send params to StartFirefoxThreadProc
struct StartFirefoxParams {
  char* firefoxFolder;
  char* firefoxProfileFolder;
  HANDLE hProcessFx;
};

// Helper threadproc function for CreateVRWindow
DWORD StartFirefoxThreadProc(_In_ LPVOID lpParameter) {
  wchar_t cmd[] = L"%Sfirefox.exe -wait-for-browser -profile %S --fxr";

  StartFirefoxParams* params = static_cast<StartFirefoxParams*>(lpParameter);
  wchar_t cmdWithPath[MAX_PATH + MAX_PATH] = {0};
  int err = swprintf_s(cmdWithPath, ARRAYSIZE(cmdWithPath), cmd,
                       params->firefoxFolder, params->firefoxProfileFolder);

  if (err != -1) {
    PROCESS_INFORMATION procFx = {0};
    STARTUPINFO startupInfoFx = {0};

#if defined(DEBUG) && defined(NIGHTLY_BUILD)
    printf("Starting Firefox via: %S\n", cmdWithPath);
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
// See nsFxrCommandLineHandler::Handle for more details about the bootstrapping
// process with Firefox.
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
                                   FALSE,    // bManualReset
                                   FALSE,    // bInitialState
                                   windowState.signalName);

    if (hEvent != nullptr) {
      // Create Shmem and push state
      VRShmemInstance::GetInstance().CreateShMem(
          true /*aCreateOnSharedMemory*/);
      VRShmemInstance::GetInstance().PushWindowState(windowState);

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
        VRShmemInstance::GetInstance().PullWindowState(windowState);

        (*hTex) = windowState.textureFx;
        (*windowId) = VRWindowManager::GetManager()->SetHWND(
            (HWND)windowState.hwndFx, fxParams.hProcessFx, hEvent);
        (*width) = windowState.widthFx;
        (*height) = windowState.heightFx;
      } else {
        // How do I failfast?
      }
    }
  }
}

// Keep track of when WaitForVREvent is running to manage shutdown of
// this vrhost. See CloseVRWindow for more details.
volatile bool s_WaitingForVREvent = false;

// Blocks until a new event is set on the shmem.
// Note: this function can be called from any thread.
void WaitForVREvent(uint32_t& nVRWindowID, uint32_t& eventType,
                    uint32_t& eventData1, uint32_t& eventData2) {
  MOZ_ASSERT(!s_WaitingForVREvent);
  s_WaitingForVREvent = true;

  // Initialize all of the out params
  nVRWindowID = 0;
  eventType = 0;
  eventData1 = 0;
  eventData2 = 0;

  if (VRShmemInstance::GetInstance().HasExternalShmem()) {
    HANDLE evt = VRWindowManager::GetManager()->GetEvent();
    const DWORD waitResult = ::WaitForSingleObject(evt, INFINITE);
    if (waitResult != WAIT_OBJECT_0) {
      MOZ_ASSERT(false && "Error WaitForVREvent().\n");
      return;
    }
    mozilla::gfx::VRWindowState windowState = {0};
    VRShmemInstance::GetInstance().PullWindowState(windowState);

    nVRWindowID =
        VRWindowManager::GetManager()->GetId((HWND)windowState.hwndFx);
    if (nVRWindowID != 0) {
      eventType = (uint32_t)windowState.eventType;
      mozilla::gfx::VRFxEventType fxEvent =
          mozilla::gfx::VRFxEventType(eventType);

      switch (fxEvent) {
        case mozilla::gfx::VRFxEventType::IME:
          eventData1 = (uint32_t)windowState.eventState;
          break;
        case mozilla::gfx::VRFxEventType::FULLSCREEN:
          eventData1 = (uint32_t)windowState.eventState;
          break;
        case mozilla::gfx::VRFxEventType::SHUTDOWN:
          VRShmemInstance::GetInstance().CloseShMem();
          break;
        default:
          MOZ_ASSERT(false && "Undefined VR Fx event.");
          break;
      }
    }
  }
  s_WaitingForVREvent = false;
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

  // If a thread is currently blocked on WaitForVREvent, then defer closing the
  // shmem to that thread by sending an async event.
  // If WaitForVREvent is not running, it is safe to close the shmem now.
  // Subsequent calls to WaitForVREvent will return early because it does not
  // have an external shmem.
  if (s_WaitingForVREvent) {
    VRShmemInstance::GetInstance().SendShutdowmState(nVRWindowID);
  } else {
    VRShmemInstance::GetInstance().CloseShMem();
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
      case WM_MOUSEWHEEL:
        // For MOUSEWHEEL, the coordinates are supposed to be at Screen origin
        // rather than window client origin.
        // Make the conversion to screen coordinates before posting the message
        // to the Fx window.
        POINT pt;
        POINTSTOPOINT(pt, MAKEPOINTS(lparam));
        if (!::ClientToScreen(hwnd, &pt)) {
          break;
        }
        // otherwise, fallthrough
        lparam = POINTTOPOINTS(pt);
      case WM_MOUSEMOVE:
      case WM_LBUTTONDOWN:
      case WM_LBUTTONUP:
      case WM_CHAR:
      case WM_KEYDOWN:
      case WM_KEYUP:
        ::PostMessage(hwnd, msg, wparam, lparam);
        break;

      default:
        break;
    }
  }
}

void SendVRTelemetry(uint32_t nVRWindowID, uint32_t telemetryId,
                     uint32_t value) {
  HWND hwnd = VRWindowManager::GetManager()->GetHWND(nVRWindowID);
  if (hwnd == nullptr) {
    return;
  }
  VRTelemetryManager::GetManager()->SendTelemetry(telemetryId, value);
}