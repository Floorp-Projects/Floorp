/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Atomics.h"

// This is an attempt to hide pointer values from the C++ compiler.  This might
// not stand up in the presence of PGO but that's probably not important.

// g_hidden_pointer is public so that it's limited what the compiler can assume
// about it, and atomic so that we don't run afoul of the compiler's UB
// analysis.

mozilla::Atomic<void*> g_hidden_pointer;

// Call this to install a pointer into the global.

MOZ_NEVER_INLINE void setHiddenPointer(void* p) { g_hidden_pointer = p; }

// Call this to retrieve the pointer.

MOZ_NEVER_INLINE void* getHiddenPointer() { return g_hidden_pointer; }
