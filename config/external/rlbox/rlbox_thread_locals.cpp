/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Load general firefox configuration of RLBox
#include "mozilla/rlbox/rlbox_config.h"

#define RLBOX_USE_STATIC_CALLS() rlbox_noop_sandbox_lookup_symbol
#include "mozilla/rlbox/rlbox_noop_sandbox.hpp"

#include "mozilla/rlbox/rlbox.hpp"

// The MingW compiler does not correctly handle static thread_local inline
// members. We instead TLS storage via functions. This can be removed if the
// MingW bug is fixed.
RLBOX_NOOP_SANDBOX_STATIC_VARIABLES();
