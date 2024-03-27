/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/TrampolineNatives.h"

#include "jit/CalleeToken.h"
#include "jit/Ion.h"
#include "jit/JitCommon.h"
#include "jit/JitRuntime.h"
#include "jit/MacroAssembler.h"
#include "jit/PerfSpewer.h"
#include "js/CallArgs.h"
#include "js/experimental/JitInfo.h"

#include "jit/MacroAssembler-inl.h"
#include "vm/Activation-inl.h"

using namespace js;
using namespace js::jit;

#define ADD_NATIVE(native)                   \
  const JSJitInfo js::jit::JitInfo_##native{ \
      {nullptr},                             \
      {uint16_t(TrampolineNative::native)},  \
      {0},                                   \
      JSJitInfo::TrampolineNative};
TRAMPOLINE_NATIVE_LIST(ADD_NATIVE)
#undef ADD_NATIVE

void js::jit::SetTrampolineNativeJitEntry(JSContext* cx, JSFunction* fun,
                                          TrampolineNative native) {
  if (!cx->runtime()->jitRuntime()) {
    // No JIT support so there's no trampoline.
    return;
  }
  void** entry = cx->runtime()->jitRuntime()->trampolineNativeJitEntry(native);
  MOZ_ASSERT(entry);
  MOZ_ASSERT(*entry);
  fun->setTrampolineNativeJitEntry(entry);
}

uint32_t JitRuntime::generateArraySortTrampoline(MacroAssembler& masm) {
  AutoCreatedBy acb(masm, "JitRuntime::generateArraySortTrampoline");

  const uint32_t offset = startTrampolineCode(masm);

  masm.assumeUnreachable("NYI");

  return offset;
}

void JitRuntime::generateTrampolineNatives(
    MacroAssembler& masm, TrampolineNativeJitEntryOffsets& offsets,
    PerfSpewerRangeRecorder& rangeRecorder) {
  offsets[TrampolineNative::ArraySort] = generateArraySortTrampoline(masm);
  rangeRecorder.recordOffset("Trampoline: ArraySort");
}

bool jit::CallTrampolineNativeJitCode(JSContext* cx, TrampolineNative native,
                                      CallArgs& args) {
  // Use the EnterJit trampoline to enter the native's trampoline code.

  AutoCheckRecursionLimit recursion(cx);
  if (!recursion.check(cx)) {
    return false;
  }

  MOZ_ASSERT(!args.isConstructing());
  CalleeToken calleeToken = CalleeToToken(&args.callee().as<JSFunction>(),
                                          /* constructing = */ false);

  Value* maxArgv = args.array() - 1;  // -1 to include |this|
  size_t maxArgc = args.length() + 1;

  Rooted<Value> result(cx, Int32Value(args.length()));

  AssertRealmUnchanged aru(cx);
  ActivationEntryMonitor entryMonitor(cx, calleeToken);
  JitActivation activation(cx);

  EnterJitCode enter = cx->runtime()->jitRuntime()->enterJit();
  void* code = *cx->runtime()->jitRuntime()->trampolineNativeJitEntry(native);

  CALL_GENERATED_CODE(enter, code, maxArgc, maxArgv, /* osrFrame = */ nullptr,
                      calleeToken, /* envChain = */ nullptr,
                      /* osrNumStackValues = */ 0, result.address());

  // Ensure the counter was reset to zero after exiting from JIT code.
  MOZ_ASSERT(!cx->isInUnsafeRegion());

  // Release temporary buffer used for OSR into Ion.
  cx->runtime()->jitRuntime()->freeIonOsrTempData();

  if (result.isMagic()) {
    MOZ_ASSERT(result.isMagic(JS_ION_ERROR));
    return false;
  }

  args.rval().set(result);
  return true;
}
