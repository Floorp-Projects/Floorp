/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/MacroAssembler.h"

#include "jsapi-tests/tests.h"
#include "jsapi-tests/testsJit.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;

#if defined(ENABLE_WASM_TAIL_CALLS) && !defined(JS_CODEGEN_NONE)

// Check if wasmMarkSlowCall produces the byte sequence that can
// wasmCheckSlowCallsite detect.
BEGIN_TEST(testWasmCheckSlowCallMarkerHit) {
  js::LifoAlloc lifo(4096);
  TempAllocator alloc(&lifo);
  JitContext jc(cx);
  StackMacroAssembler masm(cx, alloc);
  AutoCreatedBy acb(masm, __func__);

  PrepareJit(masm);

  Label check, fail, end;
  masm.call(&check);
  masm.wasmMarkSlowCall();
  masm.jump(&end);

  masm.bind(&check);
#  ifdef JS_USE_LINK_REGISTER
#    if !defined(JS_CODEGEN_LOONG64) && !defined(JS_CODEGEN_MIPS64) && \
        !defined(JS_CODEGEN_RISCV64)
  static constexpr Register ra = lr;
#    endif
#  else
  static constexpr Register ra = ABINonArgReg2;
  masm.loadPtr(Address(StackPointer, 0), ra);
#  endif

  masm.wasmCheckSlowCallsite(ra, &fail, ABINonArgReg1, ABINonArgReg2);
  masm.abiret();

  masm.bind(&fail);
  masm.printf("Failed\n");
  masm.breakpoint();

  masm.bind(&end);
  return ExecuteJit(cx, masm);
}
END_TEST(testWasmCheckSlowCallMarkerHit)

// Check if wasmCheckSlowCallsite does not detect non-marked slow calls.
BEGIN_TEST(testWasmCheckSlowCallMarkerMiss) {
  js::LifoAlloc lifo(4096);
  TempAllocator alloc(&lifo);
  JitContext jc(cx);
  StackMacroAssembler masm(cx, alloc);
  AutoCreatedBy acb(masm, __func__);

  PrepareJit(masm);

  Label check, fast, end;
  masm.call(&check);
  masm.nop();
  masm.jump(&end);

  masm.bind(&check);
#  ifdef JS_USE_LINK_REGISTER
#    if !defined(JS_CODEGEN_LOONG64) && !defined(JS_CODEGEN_MIPS64) && \
        !defined(JS_CODEGEN_RISCV64)
  static constexpr Register ra = lr;
#    endif
#  else
  static constexpr Register ra = ABINonArgReg2;
  masm.loadPtr(Address(StackPointer, 0), ra);
#  endif

  masm.wasmCheckSlowCallsite(ra, &fast, ABINonArgReg1, ABINonArgReg2);
  masm.printf("Failed\n");
  masm.breakpoint();

  masm.bind(&fast);
  masm.abiret();

  masm.bind(&end);
  return ExecuteJit(cx, masm);
}
END_TEST(testWasmCheckSlowCallMarkerMiss)

#endif  // ENABLE_WASM_TAIL_CALLS
