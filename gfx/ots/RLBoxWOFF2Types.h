/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MODULES_WOFF2_RLBOXWOFF2TYPES_H_
#define MODULES_WOFF2_RLBOXWOFF2TYPES_H_

#include <stddef.h>
#include "mozilla/rlbox/rlbox_types.hpp"
#include "woff2/decode.h"

#ifdef MOZ_WASM_SANDBOXING_WOFF2
RLBOX_DEFINE_BASE_TYPES_FOR(woff2, wasm2c)
#else
RLBOX_DEFINE_BASE_TYPES_FOR(woff2, noop)
#endif

#include "mozilla/RLBoxSandboxPool.h"
#include "mozilla/StaticPtr.h"

class RLBoxWOFF2SandboxPool : public mozilla::RLBoxSandboxPool {
 public:
  explicit RLBoxWOFF2SandboxPool(size_t aDelaySeconds)
      : RLBoxSandboxPool(aDelaySeconds) {}

  static mozilla::StaticRefPtr<RLBoxWOFF2SandboxPool> sSingleton;
  static void Initalize(size_t aDelaySeconds = 10);

 protected:
  mozilla::UniquePtr<mozilla::RLBoxSandboxDataBase> CreateSandboxData(
      uint64_t aSize) override;
  ~RLBoxWOFF2SandboxPool() = default;
};

#endif
