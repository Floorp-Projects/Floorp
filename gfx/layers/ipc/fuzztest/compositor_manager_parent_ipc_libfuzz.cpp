/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "FuzzingInterface.h"
#include "ProtocolFuzzer.h"

#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorManagerParent.h"
#include "mozilla/layers/LayerTreeOwnerTracker.h"

int
FuzzingInitCompositorManagerParentIPC(int* argc, char*** argv)
{
  mozilla::ipc::ProtocolFuzzerHelper::CompositorBridgeParentSetup();
  mozilla::layers::LayerTreeOwnerTracker::Initialize();
  return 0;
}

static int
RunCompositorManagerParentIPCFuzzing(const uint8_t* data, size_t size)
{
  static mozilla::layers::CompositorManagerParent* p =
    mozilla::layers::CompositorManagerParent::CreateSameProcess().take();

  static nsTArray<nsCString> ignored = mozilla::ipc::LoadIPCMessageBlacklist(
    getenv("MOZ_IPC_MESSAGE_FUZZ_BLACKLIST"));

  mozilla::ipc::FuzzProtocol(p, data, size, ignored);

  return 0;
}

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitCompositorManagerParentIPC,
                          RunCompositorManagerParentIPCFuzzing,
                          CompositorManagerParentIPC);
