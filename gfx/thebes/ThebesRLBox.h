/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef THEBES_RLBOX
#define THEBES_RLBOX

#include "ThebesRLBoxTypes.h"

// Load general firefox configuration of RLBox
#include "mozilla/rlbox/rlbox_config.h"

#ifdef MOZ_WASM_SANDBOXING_GRAPHITE
#  include "mozilla/rlbox/rlbox_wasm2c_sandbox.hpp"
#else
// Extra configuration for no-op sandbox
#  define RLBOX_USE_STATIC_CALLS() rlbox_noop_sandbox_lookup_symbol
#  include "mozilla/rlbox/rlbox_noop_sandbox.hpp"
#endif

#include "mozilla/rlbox/rlbox.hpp"

// Struct info needed for rlbox_load_structs_from_library
#include "graphite2/Font.h"
#include "graphite2/GraphiteExtra.h"
#include "graphite2/Segment.h"

#include "graphite2/GraphiteStructsForRLBox.h"
rlbox_load_structs_from_library(graphite);

#endif
