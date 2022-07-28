/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/testsJit.h"

#include "jit/JitCommon.h"
#include "jit/Linker.h"

#include "jit/MacroAssembler-inl.h"

// On entry to the JIT code, save every register.
void PrepareJit(js::jit::MacroAssembler& masm) {
  using namespace js::jit;
#if defined(JS_CODEGEN_ARM64)
  masm.Mov(PseudoStackPointer64, sp);
  masm.SetStackPointer64(PseudoStackPointer64);
#endif
  AllocatableRegisterSet regs(RegisterSet::All());
  LiveRegisterSet save(regs.asLiveSet());
#if defined(JS_CODEGEN_ARM)
  save.add(js::jit::d15);
#endif
#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64) || \
    defined(JS_CODEGEN_LOONG64)
  save.add(js::jit::ra);
#elif defined(JS_USE_LINK_REGISTER)
  save.add(js::jit::lr);
#endif
  masm.PushRegsInMask(save);
}

// Generate the exit path of the JIT code, which restores every register. Then,
// make it executable and run it.
bool ExecuteJit(JSContext* cx, js::jit::MacroAssembler& masm) {
  using namespace js::jit;
  AllocatableRegisterSet regs(RegisterSet::All());
  LiveRegisterSet save(regs.asLiveSet());
#if defined(JS_CODEGEN_ARM)
  save.add(js::jit::d15);
#endif
#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64) || \
    defined(JS_CODEGEN_LOONG64)
  save.add(js::jit::ra);
#elif defined(JS_USE_LINK_REGISTER)
  save.add(js::jit::lr);
#endif
  masm.PopRegsInMask(save);
#if defined(JS_CODEGEN_ARM64)
  // Return using the value popped into x30.
  masm.abiret();

  // Reset stack pointer.
  masm.SetStackPointer64(PseudoStackPointer64);
#else
  // Exit the JIT-ed code using the ABI return style.
  masm.abiret();
#endif

  if (masm.oom()) {
    return false;
  }

  Linker linker(masm);
  JitCode* code = linker.newCode(cx, CodeKind::Other);
  if (!code) {
    return false;
  }
  if (!ExecutableAllocator::makeExecutableAndFlushICache(code->raw(),
                                                         code->bufferSize())) {
    return false;
  }

  JS::AutoSuppressGCAnalysis suppress;
  EnterTest test = code->as<EnterTest>();
  CALL_GENERATED_0(test);
  return true;
}
