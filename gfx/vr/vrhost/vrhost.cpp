/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// vrhost.cpp
// Definition of functions that are exported from this dll

#include "vrhostex.h"
#include "VRShMem.h"

#include <stdio.h>
#include "windows.h"

static const char s_pszSharedEvent[] = "vrhost_test_event_signal";
static const DWORD s_dwWFSO_WAIT = 20000;

void SampleExport() { printf("vrhost.cpp hello world"); }

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
//  13 mgr: pull system
//  14 mgr: verify data
//  15 return
// These tests can be run with two instances of vrtesthost.exe, one first
// running with -testmgr and the second running with -testsvc.
// TODO: Bug 1563235 - Convert vrtesthost.exe tests into unit tests

// For testing VRShMem as the Manager (i.e., the one who creates the
// shmem). The sequence of how it tests with the service is listed above.
void TestTheManager() {
  HANDLE hEvent = ::CreateEventA(nullptr,          // lpEventAttributes
                                 FALSE,            // bManualReset
                                 FALSE,            // bInitialState
                                 s_pszSharedEvent  // lpName
  );

  printf("\n01 mgr: create mgr\n");
  mozilla::gfx::VRShMem shmem(nullptr, false, false);
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

  printf("13 mgr: pull system\n");
  mozilla::gfx::VRSystemState state;
  shmem.PullSystemState(state.displayState, state.sensorState,
                        state.controllerState, state.enumerationCompleted,
                        nullptr);

  printf(
      "14 mgr: verify data\n"
      "\tstate.enumerationCompleted = %d\n"
      "\tstate.displayState.displayName = \"%s\"\n"
      "\tstate.controllerState[1].hand = %hhu\n"
      "\tstate.sensorState.inputFrameID = %llu\n",
      state.enumerationCompleted, state.displayState.displayName,
      state.controllerState[1].hand, state.sensorState.inputFrameID);

  shmem.CloseShMem();
}

// For testing VRShMem as the Service (i.e., the one who consumes the
// shmem). The sequence of how it tests with the service is listed above.
void TestTheService() {
  // Handle created by BeTheManager above.
  HANDLE hEvent = ::OpenEventA(EVENT_ALL_ACCESS,  // dwDesiredAccess
                               FALSE,             // bInheritHandle
                               s_pszSharedEvent   // lpName
  );

  printf("\n03 svc: create svc\n");
  mozilla::gfx::VRShMem shmem(nullptr, false, false);
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

  shmem.LeaveShMem();
}