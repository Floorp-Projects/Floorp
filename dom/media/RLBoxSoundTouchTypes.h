/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RLBOXSOUNDTOUCHTYPES_H_
#define RLBOXSOUNDTOUCHTYPES_H_

#include "mozilla/rlbox/rlbox_types.hpp"

#ifdef MOZ_WASM_SANDBOXING_SOUNDTOUCH
namespace rlbox {
class rlbox_wasm2c_sandbox;
}
using rlbox_soundtouch_sandbox_type = rlbox::rlbox_wasm2c_sandbox;
#else
using rlbox_soundtouch_sandbox_type = rlbox::rlbox_noop_sandbox;
#endif

using rlbox_sandbox_soundtouch =
    rlbox::rlbox_sandbox<rlbox_soundtouch_sandbox_type>;
template <typename T>
using sandbox_callback_soundtouch =
    rlbox::sandbox_callback<T, rlbox_soundtouch_sandbox_type>;
template <typename T>
using tainted_soundtouch = rlbox::tainted<T, rlbox_soundtouch_sandbox_type>;
template <typename T>
using tainted_opaque_soundtouch =
    rlbox::tainted_opaque<T, rlbox_soundtouch_sandbox_type>;
template <typename T>
using tainted_volatile_soundtouch =
    rlbox::tainted_volatile<T, rlbox_soundtouch_sandbox_type>;
using rlbox::tainted_boolean_hint;

#endif
