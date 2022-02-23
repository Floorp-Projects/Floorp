/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2021 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* [SMDOC] The WASM ABIs
 *
 * Wasm-internal ABI.
 *
 * This section pertains to calls from one wasm function to another, and to
 * wasm's view of the situation when calling into wasm from JS and C++.  Calls
 * to JS and to C++ have other conventions.
 *
 * We pass the first function arguments in registers (GPR and FPU both) and the
 * rest on the stack, generally according to platform ABI conventions (which can
 * be hairy).  On x86-32 there are no register arguments.
 *
 * We have no callee-saves registers in the wasm-internal ABI, regardless of the
 * platform ABI conventions, though see below about TlsReg or HeapReg.
 *
 * We return the last return value in the first return register, according to
 * platform ABI conventions.  If there is more than one return value, an area is
 * allocated in the caller's frame to receive the other return values, and the
 * address of this area is passed to the callee as the last argument.  Return
 * values except the last are stored in ascending order within this area.  Also
 * see below about alignment of this area and the values in it.
 *
 * When a function is entered, there are two incoming register values in
 * addition to the function's declared parameters: TlsReg must have the correct
 * TLS pointer, and HeapReg the correct memoryBase, for the function.  (On
 * x86-32 there is no HeapReg.)  From the TLS we can get to the JSContext, the
 * instance, the MemoryBase, and many other things.  The TLS maps one-to-one
 * with an instance.
 *
 * HeapReg and TlsReg are not parameters in the usual sense, nor are they
 * callee-saves registers.  Instead they constitute global register state, the
 * purpose of which is to bias the call ABI in favor of intra-instance calls,
 * the predominant case where the caller and the callee have the same TlsReg and
 * HeapReg values.
 *
 * With this global register state, literally no work needs to take place to
 * save and restore the TLS and MemoryBase values across intra-instance call
 * boundaries.
 *
 * For inter-instance calls, in contrast, there must be an instance switch at
 * the call boundary: Before the call, the callee's TLS must be loaded (from a
 * closure or from the import table), and from the TLS we load the callee's
 * MemoryBase, the realm, and the JSContext.  The caller's and callee's TLS
 * values must be stored into the frame (to aid unwinding), the callee's realm
 * must be stored into the JSContext, and the callee's TLS and MemoryBase values
 * must be moved to appropriate registers.  After the call, the caller's TLS
 * must be loaded, and from it the caller's MemoryBase and realm, and the
 * JSContext.  The realm must be stored into the JSContext and the caller's TLS
 * and MemoryBase values must be moved to appropriate registers.
 *
 * Direct calls to functions within the same module are always intra-instance,
 * while direct calls to imported functions are always inter-instance.  Indirect
 * calls -- call_indirect in the MVP, future call_ref and call_funcref -- may or
 * may not be intra-instance.
 *
 * call_indirect, and future call_funcref, also pass a signature value in a
 * register (even on x86-32), this is a small integer or a pointer value
 * denoting the caller's expected function signature.  The callee must compare
 * it to the value or pointer that denotes its actual signature, and trap on
 * mismatch.
 *
 * This is what the stack looks like during a call, after the callee has
 * completed the prologue:
 *
 *     |                                   |
 *     +-----------------------------------+ <-+
 *     |               ...                 |   |
 *     |      Caller's private frame       |   |
 *     +-----------------------------------+   |
 *     |   Multi-value return (optional)   |   |
 *     |               ...                 |   |
 *     +-----------------------------------+   |
 *     |       Stack args (optional)       |   |
 *     |               ...                 |   |
 *     +-----------------------------------+ -+|
 *     |          Caller TLS slot          |   \
 *     |          Callee TLS slot          |   | \
 *     +-----------------------------------+   |  \
 *     |       Shadowstack area (Win64)    |   |  wasm::FrameWithTls
 *     |            (32 bytes)             |   |  /
 *     +-----------------------------------+   | /  <= SP "Before call"
 *     |          Return address           |   //   <= SP "After call"
 *     |             Saved FP          ----|--+/
 *     +-----------------------------------+ -+ <=  FP (a wasm::Frame*)
 *     |  DebugFrame, Locals, spills, etc  |
 *     |   (i.e., callee's private frame)  |
 *     |             ....                  |
 *     +-----------------------------------+    <=  SP
 *
 * The FrameWithTls is a struct with four fields: the saved FP, the return
 * address, and the two TLS slots; the shadow stack area is there only on Win64
 * and is unused by wasm but is part of the native ABI, with which the wasm ABI
 * is mostly compatible.  The slots for caller and callee TLS are only populated
 * by the instance switching code in inter-instance calls so that stack
 * unwinding can keep track of the correct TLS value for each frame, the TLS not
 * being obtainable from anywhere else.  Nothing in the frame itself indicates
 * directly whether the TLS slots are valid - for that, the return address must
 * be used to look up a CallSite structure that carries that information.
 *
 * The stack area above the return address is owned by the caller, which may
 * deallocate the area on return or choose to reuse it for subsequent calls.
 * (The baseline compiler allocates and frees the stack args area and the
 * multi-value result area per call.  Ion reuses the areas and allocates them as
 * part of the overall activation frame when the procedure is entered; indeed,
 * the multi-value return area can be anywhere within the caller's private
 * frame, not necessarily directly above the stack args.)
 *
 * If the stack args area contain references, it is up to the callee's stack map
 * to name the locations where those references exist, and the caller's stack
 * map must not (redundantly) name those locations.  (The callee's ownership of
 * this area will be crucial for making tail calls work, as the types of the
 * locations can change if the callee makes a tail call.)  If pointer values are
 * spilled by anyone into the Shadowstack area they will not be traced.
 *
 * References in the multi-return area are covered by the caller's map, as these
 * slots outlive the call.
 *
 * The address "Before call", ie the part of the FrameWithTls above the Frame,
 * must be aligned to WasmStackAlignment, and everything follows from that, with
 * padding inserted for alignment as required for stack arguments.  In turn
 * WasmStackAlignment is at least as large as the largest parameter type.
 *
 * The address of the multiple-results area is currently 8-byte aligned by Ion
 * and its alignment in baseline is uncertain, see bug 1747787.  Result values
 * are stored packed within the area in fields whose size is given by
 * ResultStackSize(ValType), this breaks alignment too.  This all seems
 * underdeveloped.
 *
 * In the wasm-internal ABI, the ARM64 PseudoStackPointer (PSP) is garbage on
 * entry but must be synced with the real SP at the point the function returns.
 */

#ifndef wasm_frame_h
#define wasm_frame_h

#include "mozilla/Assertions.h"

#include <stddef.h>
#include <stdint.h>
#include <type_traits>

#include "jit/Registers.h"  // For js::jit::ShadowStackSpace

namespace js {
namespace wasm {

struct TlsData;

// Bit set as the lowest bit of a frame pointer, used in two different mutually
// exclusive situations:
// - either it's a low bit tag in a FramePointer value read from the
// Frame::callerFP of an inner wasm frame. This indicates the previous call
// frame has been set up by a JIT caller that directly called into a wasm
// function's body. This is only stored in Frame::callerFP for a wasm frame
// called from JIT code, and thus it can not appear in a JitActivation's
// exitFP.
// - or it's the low big tag set when exiting wasm code in JitActivation's
// exitFP.

constexpr uintptr_t ExitOrJitEntryFPTag = 0x1;

// Tag for determining whether trampoline frame is on the stack or not.
constexpr uintptr_t TrampolineFpTag = 0x2;

#if defined(JS_CODEGEN_X86)
// In a normal call without an indirect stub, SP is 16-byte aligned before the
// call, and after the call SP is (16 - 4) bytes aligned because the call
// instruction pushes the return address. The callee expects SP to be (16 - 4)
// bytes aligned for the checkedCallEntry and to be (16 - 8) byte aligned for
// the tailCheckedEntry.
//
// When we call the indirect stub instead of the callee, SP is still 16 bytes
// aligned before the call.  Then we push two Frames - one for the stub and one
// on the callee's behalf. Since each frame is (8) bytes aligned, the resulting
// SP will be also aligned on (16) when we jump into callee, so we will hit the
// alignment assert.
//
// To prevent this we allocate additional (8) bytes to satisfy the callee's
// expectations about SP.  This allocation is implemented locally right at the
// call site in the generated stub.  Every computation of the size of a
// trampoline's interstitial frame must take this extra space into account.
//
// (TODO: There are too many constants involved in these calculations, some of
// the values the constants represent could change.  It would be nice to derive
// some of these constants instead of using them literally.  But if the
// underlying values change incompatibly, then the alignment assertions in
// generated code will save us.)
constexpr uint32_t IndirectStubAdditionalAlignment = 8u;
#else
constexpr uint32_t IndirectStubAdditionalAlignment = 0u;
#endif

// wasm::Frame represents the bytes pushed by the call instruction and the
// fixed prologue generated by wasm::GenerateCallablePrologue.
//
// Across all architectures it is assumed that, before the call instruction, the
// stack pointer is WasmStackAlignment-aligned. Thus after the prologue, and
// before the function has made its stack reservation, the stack alignment is
// sizeof(Frame) % WasmStackAlignment.
//
// During MacroAssembler code generation, the bytes pushed after the wasm::Frame
// are counted by masm.framePushed. Thus, the stack alignment at any point in
// time is (sizeof(wasm::Frame) + masm.framePushed) % WasmStackAlignment.

class Frame {
  // See GenerateCallableEpilogue for why this must be
  // the first field of wasm::Frame (in a downward-growing stack).
  // It's either the caller's Frame*, for wasm callers, or the JIT caller frame
  // plus a tag otherwise.
  uint8_t* callerFP_;

  // The return address pushed by the call (in the case of ARM/MIPS the return
  // address is pushed by the first instruction of the prologue).
  void* returnAddress_;

 public:
  static constexpr uint32_t callerFPOffset() {
    return offsetof(Frame, callerFP_);
  }
  static constexpr uint32_t returnAddressOffset() {
    return offsetof(Frame, returnAddress_);
  }

  uint8_t* returnAddress() const {
    return reinterpret_cast<uint8_t*>(returnAddress_);
  }

  void** addressOfReturnAddress() {
    return reinterpret_cast<void**>(&returnAddress_);
  }

  uint8_t* rawCaller() const { return callerFP_; }

  Frame* wasmCaller() const {
    MOZ_ASSERT(!callerIsExitOrJitEntryFP() && !callerIsTrampolineFP());
    return reinterpret_cast<Frame*>(callerFP_);
  }

  Frame* trampolineCaller() const {
    MOZ_ASSERT(callerIsTrampolineFP());
    return reinterpret_cast<Frame*>(reinterpret_cast<uintptr_t>(callerFP_) &
                                    ~TrampolineFpTag);
  }

  bool callerIsExitOrJitEntryFP() const {
    return isExitOrJitEntryFP(callerFP_);
  }

  bool callerIsTrampolineFP() const { return isTrampolineFP(callerFP_); }

  size_t numberOfTrampolineSlots() const {
    return callerIsTrampolineFP()
               ? ((sizeof(Frame) + IndirectStubAdditionalAlignment) /
                  sizeof(void*))
               : 0;
  }

  uint8_t* jitEntryCaller() const { return toJitEntryCaller(callerFP_); }

  static const Frame* fromUntaggedWasmExitFP(const void* savedFP) {
    MOZ_ASSERT(!isExitOrJitEntryFP(savedFP));
    return reinterpret_cast<const Frame*>(savedFP);
  }

  static bool isExitOrJitEntryFP(const void* fp) {
    return reinterpret_cast<uintptr_t>(fp) & ExitOrJitEntryFPTag;
  }

  static uint8_t* toJitEntryCaller(const void* fp) {
    MOZ_ASSERT(isExitOrJitEntryFP(fp));
    return reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(fp) &
                                      ~ExitOrJitEntryFPTag);
  }

  static bool isTrampolineFP(const void* fp) {
    return reinterpret_cast<uintptr_t>(fp) & TrampolineFpTag;
  }

  static uint8_t* addExitOrJitEntryFPTag(const Frame* fp) {
    MOZ_ASSERT(!isExitOrJitEntryFP(fp));
    return reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(fp) |
                                      ExitOrJitEntryFPTag);
  }
};

static_assert(!std::is_polymorphic_v<Frame>, "Frame doesn't need a vtable.");
static_assert(sizeof(Frame) == 2 * sizeof(void*),
              "Frame is a two pointer structure");

class FrameWithTls : public Frame {
  // `ShadowStackSpace` bytes will be allocated here on Win64, at higher
  // addresses than Frame and at lower addresses than the TLS fields.

  // The TLS area MUST be two pointers exactly.
  TlsData* calleeTls_;
  TlsData* callerTls_;

 public:
  TlsData* calleeTls() { return calleeTls_; }
  TlsData* callerTls() { return callerTls_; }

  constexpr static uint32_t sizeOf() {
    return sizeof(wasm::FrameWithTls) + js::jit::ShadowStackSpace;
  }

  constexpr static uint32_t sizeOfTlsFields() {
    return sizeof(wasm::FrameWithTls) - sizeof(wasm::Frame);
  }

  constexpr static uint32_t calleeTlsOffset() {
    return offsetof(FrameWithTls, calleeTls_) + js::jit::ShadowStackSpace;
  }

  constexpr static uint32_t calleeTlsOffsetWithoutFrame() {
    return calleeTlsOffset() - sizeof(wasm::Frame);
  }

  constexpr static uint32_t callerTlsOffset() {
    return offsetof(FrameWithTls, callerTls_) + js::jit::ShadowStackSpace;
  }

  constexpr static uint32_t callerTlsOffsetWithoutFrame() {
    return callerTlsOffset() - sizeof(wasm::Frame);
  }
};

static_assert(FrameWithTls::calleeTlsOffsetWithoutFrame() ==
                  js::jit::ShadowStackSpace,
              "Callee tls stored right above the return address.");
static_assert(FrameWithTls::callerTlsOffsetWithoutFrame() ==
                  js::jit::ShadowStackSpace + sizeof(void*),
              "Caller tls stored right above the callee tls.");

static_assert(FrameWithTls::sizeOfTlsFields() == 2 * sizeof(void*),
              "There are only two additional slots");

#if defined(JS_CODEGEN_ARM64)
static_assert(sizeof(Frame) % 16 == 0, "frame is aligned");
#endif

}  // namespace wasm
}  // namespace js

#endif  // wasm_frame_h
