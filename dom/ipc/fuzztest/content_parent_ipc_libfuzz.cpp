/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "FuzzingInterface.h"
#include "ProtocolFuzzer.h"

#include "mozilla/RefPtr.h"
#include "mozilla/devtools/PHeapSnapshotTempFileHelper.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/gfx/gfxVars.h"

int
FuzzingInitContentParentIPC(int* argc, char*** argv)
{
  return 0;
}

static int
RunContentParentIPCFuzzing(const uint8_t* data, size_t size)
{
  static mozilla::dom::ContentParent* p =
    mozilla::ipc::ProtocolFuzzerHelper::CreateContentParent(
      nullptr, NS_LITERAL_STRING(DEFAULT_REMOTE_TYPE));

  static nsTArray<nsCString> ignored = mozilla::ipc::LoadIPCMessageBlacklist(
    getenv("MOZ_IPC_MESSAGE_FUZZ_BLACKLIST"));

  mozilla::ipc::FuzzProtocol(p, data, size, ignored);

  return 0;
}

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitContentParentIPC,
                          RunContentParentIPCFuzzing,
                          ContentParentIPC);
