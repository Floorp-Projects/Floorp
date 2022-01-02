/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsapi_tests_testsJit_h
#define jsapi_tests_testsJit_h

#include "jit/MacroAssembler.h"

typedef void (*EnterTest)();

// On entry to the JIT code, save every register.
void PrepareJit(js::jit::MacroAssembler& masm);

// Generate the exit path of the JIT code, which restores every register. Then,
// make it executable and run it.
bool ExecuteJit(JSContext* cx, js::jit::MacroAssembler& masm);

#endif /* !jsapi_tests_testsJit_h */
