/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RLBOX_CONFIG
#define RLBOX_CONFIG

#include "mozilla/Assertions.h"

// All uses of rlbox's function and callbacks invocations are on a single
// thread right now, so we disable rlbox thread checks for performance
// See (Bug 1739298) for more details
#define RLBOX_SINGLE_THREADED_INVOCATIONS

#define RLBOX_CUSTOM_ABORT(msg) MOZ_CRASH_UNSAFE_PRINTF("RLBox crash: %s", msg)

// The MingW compiler does not correctly handle static thread_local inline
// members. This toggles a workaround that allows the host application (firefox)
// to provide TLS storage via functions. This can be removed if the MingW bug is
// fixed.
#define RLBOX_EMBEDDER_PROVIDES_TLS_STATIC_VARIABLES

// When instantiating a wasm sandbox, rlbox requires the name of the wasm module
// being instantiated. LLVM and wasm2c use the module name by choosing the name
// used to generate the wasm file. In Firefox this is a static library called
// rlbox
#define RLBOX_WASM2C_MODULE_NAME rlbox

#endif
