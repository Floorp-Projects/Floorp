/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_AsyncInterrupt_h
#define jit_AsyncInterrupt_h

#include "NamespaceImports.h"

namespace js {
namespace jit {

// Ensure the given JSRuntime is set up to use async interrupts. Failure to
// enable signal handlers indicates some catastrophic failure and creation of
// the runtime must fail.
void
EnsureAsyncInterrupt(JSContext* cx);

// Return whether the async interrupt can be used to interrupt Ion code.
bool
HaveAsyncInterrupt();

// Force any currently-executing JIT code to call HandleExecutionInterrupt.
extern void
InterruptRunningCode(JSContext* cx);

} // namespace jit
} // namespace js

#endif // jit_AsyncInterrupt_h
