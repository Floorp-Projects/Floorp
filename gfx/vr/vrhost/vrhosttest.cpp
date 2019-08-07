/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// vrhosttest.cpp
// Definition of testing and validation functions that are exported from this
// dll

#include "vrhostex.h"
#include "VRShMem.h"

#include <stdio.h>
#include "windows.h"

static const char s_pszSharedEvent[] = "vrhost_test_event_signal";
static const DWORD s_dwWFSO_WAIT = 20000;

void SampleExport() { printf("vrhost.cpp hello world\n"); }

// For testing ShMem as Manager and Service:
// The two processes should output the steps, synchronously, to validate
// 2-way communication via VRShMem as follows
//  01 mgr: create mgr
//  02 mgr: wait for signal
//  03 svc: create svc
//  04 svc: send signal
//  05 svc: wait for signal
//  06 mgr: push browser
//  07 mgr: send signal
//  08 mgr: wait for signal
//  09 svc: pull browser
//  10 svc: verify data
//  11 svc: push system
//  12 svc: send signal
//  13 svc: wait for signal
//  14 mgr: pull system
//  15 mgr: verify data
//  16 mgr: push window
//  17 mgr: send signal
//  18 mgr: wait for signal
//  19 svc: pull window
//  20 svc: verify data
//  21 svc: push window
//  22 svc: send signal
//  23 mgr: pull window
//  24 mgr: verify data
//  25 return
// These tests can be run with two instances of vrtesthost.exe, one first
// running with -testmgr and the second running with -testsvc.
// TODO: Bug 1563235 - Convert vrtesthost.exe tests into unit tests

// For testing VRShMem as the Manager (i.e., the one who creates the
// shmem). The sequence of how it tests with the service is listed above.
void TestTheManager() {
  printf("TestTheManager Start\n");

  MOZ_ASSERT(GetLastError() == 0,
             "TestTheManager should start with no OS errors");
  HANDLE hEvent = ::CreateEventA(nullptr,          // lpEventAttributes
                                 FALSE,            // bManualReset
                                 FALSE,            // bInitialState
                                 s_pszSharedEvent  // lpName
  );

  printf("\n01 mgr: create mgr\n");
  mozilla::gfx::VRShMem shmem(nullptr, true, true);
  shmem.CreateShMem();

  printf("02 mgr: wait for signal\n");
  ::WaitForSingleObject(hEvent, s_dwWFSO_WAIT);

  // Set some state to verify on the other side
  mozilla::gfx::VRBrowserState browserState = {0};
  browserState.presentationActive = true;
  browserState.layerState[0].type =
      mozilla::gfx::VRLayerType::LayerType_2D_Content;
  browserState.hapticState[0].controllerIndex = 987;

  printf("06 mgr: push browser\n");
  shmem.PushBrowserState(browserState, true);

  printf("07 mgr: send signal\n");
  ::SetEvent(hEvent);

  printf("08 mgr: wait for signal\n");
  ::WaitForSingleObject(hEvent, s_dwWFSO_WAIT);

  printf("14 mgr: pull system\n");
  mozilla::gfx::VRSystemState state;
  shmem.PullSystemState(state.displayState, state.sensorState,
                        state.controllerState, state.enumerationCompleted,
                        nullptr);

  printf(
      "15 mgr: verify data\n"
      "\tstate.enumerationCompleted = %d\n"
      "\tstate.displayState.displayName = \"%s\"\n"
      "\tstate.controllerState[1].hand = %hhu\n"
      "\tstate.sensorState.inputFrameID = %llu\n",
      state.enumerationCompleted, state.displayState.displayName,
      state.controllerState[1].hand, state.sensorState.inputFrameID);

  // Test the WindowState functions as the host
  mozilla::gfx::VRWindowState windowState = {0};
  strcpy(windowState.signalName, "randomsignalstring");
  windowState.dxgiAdapterHost = 99;
  windowState.heightHost = 42;
  windowState.widthHost = 24;

  printf("16 mgr: push window\n");
  shmem.PushWindowState(windowState);

  printf("17 mgr: send signal\n");
  ::SetEvent(hEvent);

  printf("18 mgr: wait for signal\n");
  ::WaitForSingleObject(hEvent, s_dwWFSO_WAIT);

  printf("23 mgr: pull window\n");
  shmem.PullWindowState(windowState);

  printf(
      "24 svc: verify data\n"
      "\tstate.hwndFx = 0x%llX\n"
      "\tstate.heightFx = %d\n"
      "\tstate.widthFx = %d\n"
      "\tstate.textureHandle = %p\n",
      windowState.hwndFx, windowState.heightFx, windowState.widthFx,
      windowState.textureFx);

  shmem.CloseShMem();

  printf("TestTheManager complete");
  fflush(nullptr);
}

// For testing VRShMem as the Service (i.e., the one who consumes the
// shmem). The sequence of how it tests with the service is listed above.
void TestTheService() {
  printf("TestTheService Start\n");

  MOZ_ASSERT(GetLastError() == 0,
             "TestTheService should start with no OS errors");
  // Handle created by TestTheManager above.
  HANDLE hEvent = ::OpenEventA(EVENT_ALL_ACCESS,  // dwDesiredAccess
                               FALSE,             // bInheritHandle
                               s_pszSharedEvent   // lpName
  );

  printf("\n03 svc: create svc\n");
  mozilla::gfx::VRShMem shmem(nullptr, true, false);
  shmem.JoinShMem();

  printf("04 svc: send signal\n");
  ::SetEvent(hEvent);

  printf("05 svc: wait for signal\n");
  ::WaitForSingleObject(hEvent, s_dwWFSO_WAIT);

  printf("09 svc: pull browser\n");
  mozilla::gfx::VRBrowserState state;
  shmem.PullBrowserState(state);

  printf(
      "10 svc: verify data\n"
      "\tstate.presentationActive = %d\n"
      "\tstate.layerState[0].type = %hu\n"
      "\tstate.hapticState[0].controllerIndex = %d\n",
      state.presentationActive, state.layerState[0].type,
      state.hapticState[0].controllerIndex);

  // Set some state to verify on the other side
  mozilla::gfx::VRSystemState systemState;
  systemState.enumerationCompleted = true;
  strncpy(systemState.displayState.displayName, "test from vrservice shmem",
          mozilla::gfx::kVRDisplayNameMaxLen);
  systemState.controllerState[1].hand = mozilla::gfx::ControllerHand::Left;
  systemState.sensorState.inputFrameID = 1234567;

  printf("11 svc: push system\n");
  shmem.PushSystemState(systemState);

  printf("12 svc: send signal\n");
  ::SetEvent(hEvent);

  printf("13 svc: wait for signal\n");
  ::WaitForSingleObject(hEvent, s_dwWFSO_WAIT);

  // Test the WindowState functions as Firefox
  printf("19 svc: pull window\n");
  mozilla::gfx::VRWindowState windowState;
  shmem.PullWindowState(windowState);

  printf(
      "20 svc: verify data\n"
      "\tstate.signalName = \"%s\"\n"
      "\tstate.dxgiAdapterHost = %d\n"
      "\tstate.heightHost = %d\n"
      "\tstate.widthHost = %d\n",
      windowState.signalName, windowState.dxgiAdapterHost,
      windowState.heightHost, windowState.widthHost);

  windowState.hwndFx = 0x1234;
  windowState.heightFx = 1234;
  windowState.widthFx = 4321;
  windowState.textureFx = (HANDLE)0x77777;

  printf("21 svc: push window\n");
  shmem.PushWindowState(windowState);

  printf("22 svc: send signal\n");
  ::SetEvent(hEvent);

  shmem.LeaveShMem();

  printf("TestTheService complete");
  fflush(nullptr);
}

// This function tests the export CreateVRWindow by outputting the return values
// from the call to the console, as well as testing CloseVRWindow after the data
// is retrieved.
void TestCreateVRWindow() {
  printf("TestCreateVRWindow Start\n");

  // Cache function calls to test real-world export and usage
  HMODULE hVRHost = ::GetModuleHandleA("vrhost.dll");
  PFN_CREATEVRWINDOW fnCreate =
      (PFN_CREATEVRWINDOW)::GetProcAddress(hVRHost, "CreateVRWindow");
  PFN_CLOSEVRWINDOW fnClose =
      (PFN_CLOSEVRWINDOW)::GetProcAddress(hVRHost, "CloseVRWindow");

  // Create the VR Window and store data from creation
  char currentDir[MAX_PATH] = {0};
  char currentDirProfile[MAX_PATH] = {0};
  DWORD currentDirLength =
      ::GetCurrentDirectory(ARRAYSIZE(currentDir), currentDir);
  currentDir[currentDirLength] = '\\';

  int err = sprintf_s(currentDirProfile, ARRAYSIZE(currentDirProfile),
                      "%svrhosttest-profile", currentDir);
  if (err > 0) {
    printf("Starting Firefox from %s\n", currentDir);

    UINT windowId;
    HANDLE hTex;
    UINT width;
    UINT height;
    fnCreate(currentDir, currentDirProfile, 0, 100, 200, &windowId, &hTex,
             &width, &height);

    // Close the Firefox VR Window
    fnClose(windowId, true);

    // Print output from CreateVRWindow
    printf(
        "\n\nTestCreateVRWindow End:\n"
        "\twindowId = 0x%X\n"
        "\thTex = 0x%p\n"
        "\twidth = %d\n"
        "\theight = %d\n",
        windowId, hTex, width, height);
    printf("\n***Note: profile folder created at %s***\n", currentDirProfile);
  }
}
