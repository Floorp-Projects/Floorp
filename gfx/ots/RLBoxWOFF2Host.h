/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MODULES_WOFF2_RLBOXWOFF2_HOST_H_
#define MODULES_WOFF2_RLBOXWOFF2_HOST_H_

#include "RLBoxWOFF2Types.h"

// Load general firefox configuration of RLBox
#include "mozilla/rlbox/rlbox_config.h"

#ifdef MOZ_WASM_SANDBOXING_WOFF2
// Include the generated header file so that we are able to resolve the symbols
// in the wasm binary
#  include "rlbox.wasm.h"
#  define RLBOX_USE_STATIC_CALLS() rlbox_wasm2c_sandbox_lookup_symbol
#  include "mozilla/rlbox/rlbox_wasm2c_sandbox.hpp"
#else
// Extra configuration for no-op sandbox
#  define RLBOX_USE_STATIC_CALLS() rlbox_noop_sandbox_lookup_symbol
#  include "mozilla/rlbox/rlbox_noop_sandbox.hpp"
#endif

#include "mozilla/rlbox/rlbox.hpp"

#include "woff2/RLBoxWOFF2Sandbox.h"
#include "./src/ots.h"

class RLBoxWOFF2SandboxData : public mozilla::RLBoxSandboxDataBase {
  friend class RLBoxWOFF2SandboxPool;

 public:
  RLBoxWOFF2SandboxData(uint64_t aSize,
                        mozilla::UniquePtr<rlbox_sandbox_woff2> aSandbox);
  ~RLBoxWOFF2SandboxData();

  rlbox_sandbox_woff2* Sandbox() const { return mSandbox.get(); }

 private:
  mozilla::UniquePtr<rlbox_sandbox_woff2> mSandbox;
  sandbox_callback_woff2<BrotliDecompressCallback*> mDecompressCallback;
};

using ProcessTTCFunc = bool(ots::FontFile* aHeader, ots::OTSStream* aOutput,
                            const uint8_t* aData, size_t aLength,
                            uint32_t aIndex);

using ProcessTTFFunc = bool(ots::FontFile* aHeader, ots::Font* aFont,
                            ots::OTSStream* aOutput, const uint8_t* aData,
                            size_t aLength, uint32_t aOffset);

bool RLBoxProcessWOFF2(ots::FontFile* aHeader, ots::OTSStream* aOutput,
                       const uint8_t* aData, size_t aLength, uint32_t aIndex,
                       ProcessTTCFunc* aProcessTTC,
                       ProcessTTFFunc* aProcessTTF);
#endif
