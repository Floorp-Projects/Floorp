/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OGG_RLBOX_TYPES
#define OGG_RLBOX_TYPES

#include "mozilla/rlbox/rlbox_types.hpp"

#ifdef MOZ_WASM_SANDBOXING_OGG
namespace rlbox {
class rlbox_wasm2c_sandbox;
}
using rlbox_ogg_sandbox_type = rlbox::rlbox_wasm2c_sandbox;
#else
using rlbox_ogg_sandbox_type = rlbox::rlbox_noop_sandbox;
#endif

using rlbox_sandbox_ogg = rlbox::rlbox_sandbox<rlbox_ogg_sandbox_type>;
template <typename T>
using sandbox_callback_ogg = rlbox::sandbox_callback<T, rlbox_ogg_sandbox_type>;
template <typename T>
using tainted_ogg = rlbox::tainted<T, rlbox_ogg_sandbox_type>;
template <typename T>
using tainted_opaque_ogg = rlbox::tainted_opaque<T, rlbox_ogg_sandbox_type>;
template <typename T>
using tainted_volatile_ogg = rlbox::tainted_volatile<T, rlbox_ogg_sandbox_type>;
using rlbox::tainted_boolean_hint;

#endif
