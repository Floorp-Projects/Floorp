/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_USING_WASM_SANDBOXING

// For MOZ_CRASH_UNSAFE_PRINTF
#  include "mozilla/Assertions.h"

// Load general firefox configuration of RLBox
#  include "mozilla/rlbox/rlbox_config.h"

#  include "mozilla/rlbox/rlbox_wasm2c_sandbox.hpp"

#  include "mozilla/rlbox/rlbox.hpp"

// The MingW compiler does not correctly handle static thread_local inline
// members. We instead TLS storage via functions. This can be removed if the
// MingW bug is fixed.
RLBOX_WASM2C_SANDBOX_STATIC_VARIABLES();

extern "C" {

// Any error encountered by the wasm2c runtime or wasm sandboxed library code
// is configured to call the below trap handler.
void moz_wasm2c_trap_handler(const char* msg) {
  MOZ_CRASH_UNSAFE_PRINTF("wasm2c crash: %s", msg);
}
}

#endif
