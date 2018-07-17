/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

typedef uint32_t HashNumber;

#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/Initialization.h"
#include "js/MemoryMetrics.h"
#include "js/StructuredClone.h"

// Replacements for types that are too difficult for rust-bindgen.

/// <div rustbindgen replaces="JS::detail::MaybeWrapped" />
template <typename T>
using replaces_MaybeWrapped = T;
