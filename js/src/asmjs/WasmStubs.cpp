/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2015 Mozilla Foundation
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

#include "asmjs/WasmStubs.h"

#include "mozilla/ArrayUtils.h"

#include "asmjs/WasmCode.h"
#include "asmjs/WasmIonCompile.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::ArrayLength;

static void
AssertStackAlignment(MacroAssembler& masm, uint32_t alignment, uint32_t addBeforeAssert = 0)
{
    MOZ_ASSERT((sizeof(AsmJSFrame) + masm.framePushed() + addBeforeAssert) % alignment == 0);
    masm.assertStackAlignment(alignment, addBeforeAssert);
}

static unsigned
StackDecrementForCall(MacroAssembler& masm, uint32_t alignment, unsigned bytesToPush)
{
    return StackDecrementForCall(alignment, sizeof(AsmJSFrame) + masm.framePushed(), bytesToPush);
}

template <class VectorT>
static unsigned
StackArgBytes(const VectorT& args)
{
    ABIArgIter<VectorT> iter(args);
    while (!iter.done())
        iter++;
    return iter.stackBytesConsumedSoFar();
}

template <class VectorT>
static unsigned
StackDecrementForCall(MacroAssembler& masm, uint32_t alignment, const VectorT& args,
                      unsigned extraBytes = 0)
{
    return StackDecrementForCall(masm, alignment, StackArgBytes(args) + extraBytes);
}

#if defined(JS_CODEGEN_ARM)
// The ARM system ABI also includes d15 & s31 in the non volatile float registers.
// Also exclude lr (a.k.a. r14) as we preserve it manually)
static const LiveRegisterSet NonVolatileRegs =
    LiveRegisterSet(GeneralRegisterSet(Registers::NonVolatileMask&
                                       ~(uint32_t(1) << Registers::lr)),
                    FloatRegisterSet(FloatRegisters::NonVolatileMask
                                     | (1ULL << FloatRegisters::d15)
                                     | (1ULL << FloatRegisters::s31)));
#else
static const LiveRegisterSet NonVolatileRegs =
    LiveRegisterSet(GeneralRegisterSet(Registers::NonVolatileMask),
                    FloatRegisterSet(FloatRegisters::NonVolatileMask));
#endif

#if defined(JS_CODEGEN_MIPS32)
// Mips is using one more double slot due to stack alignment for double values.
// Look at MacroAssembler::PushRegsInMask(RegisterSet set)
static const unsigned FramePushedAfterSave = NonVolatileRegs.gprs().size() * sizeof(intptr_t) +
                                             NonVolatileRegs.fpus().getPushSizeInBytes() +
                                             sizeof(double);
#elif defined(JS_CODEGEN_NONE)
static const unsigned FramePushedAfterSave = 0;
#else
static const unsigned FramePushedAfterSave = NonVolatileRegs.gprs().size() * sizeof(intptr_t)
                                           + NonVolatileRegs.fpus().getPushSizeInBytes();
#endif
static const unsigned FramePushedForEntrySP = FramePushedAfterSave + sizeof(void*);

// Generate a stub that enters wasm from a C++ caller via the native ABI.
// The signature of the entry point is Module::CodePtr. The exported wasm
// function has an ABI derived from its specific signature, so this function
// must map from the ABI of CodePtr to the export's signature's ABI.
Offsets
wasm::GenerateEntry(MacroAssembler& masm, const Export& exp, bool usesHeap)
{
    masm.haltingAlign(CodeAlignment);

    Offsets offsets;
    offsets.begin = masm.currentOffset();

    // Save the return address if it wasn't already saved by the call insn.
#if defined(JS_CODEGEN_ARM)
    masm.push(lr);
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    masm.push(ra);
#elif defined(JS_CODEGEN_X86)
    static const unsigned EntryFrameSize = sizeof(void*);
#endif

    // Save all caller non-volatile registers before we clobber them here and in
    // the asm.js callee (which does not preserve non-volatile registers).
    masm.setFramePushed(0);
    masm.PushRegsInMask(NonVolatileRegs);
    MOZ_ASSERT(masm.framePushed() == FramePushedAfterSave);

    // ARM and MIPS/MIPS64 have a globally-pinned GlobalReg (x64 uses RIP-relative
    // addressing, x86 uses immediates in effective addresses). For the
    // AsmJSGlobalRegBias addition, see Assembler-(mips,arm).h.
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    masm.movePtr(IntArgReg1, GlobalReg);
    masm.addPtr(Imm32(AsmJSGlobalRegBias), GlobalReg);
#endif

    // ARM, MIPS/MIPS64 and x64 have a globally-pinned HeapReg (x86 uses immediates in
    // effective addresses). Loading the heap register depends on the global
    // register already having been loaded.
    if (usesHeap)
        masm.loadAsmJSHeapRegisterFromGlobalData();

    // Put the 'argv' argument into a non-argument/return register so that we
    // can use 'argv' while we fill in the arguments for the asm.js callee.
    // Also, save 'argv' on the stack so that we can recover it after the call.
    // Use a second non-argument/return register as temporary scratch.
    Register argv = ABINonArgReturnReg0;
    Register scratch = ABINonArgReturnReg1;
    Register64 scratch64(scratch);

#if defined(JS_CODEGEN_X86)
    masm.loadPtr(Address(masm.getStackPointer(), EntryFrameSize + masm.framePushed()), argv);
#else
    masm.movePtr(IntArgReg0, argv);
#endif
    masm.Push(argv);

    // Save the stack pointer to the saved non-volatile registers. We will use
    // this on two paths: normal return and exceptional return. Since
    // loadWasmActivation uses GlobalReg, we must do this after loading
    // GlobalReg.
    MOZ_ASSERT(masm.framePushed() == FramePushedForEntrySP);
    masm.loadWasmActivation(scratch);
    masm.storeStackPtr(Address(scratch, WasmActivation::offsetOfEntrySP()));

    // Dynamically align the stack since ABIStackAlignment is not necessarily
    // AsmJSStackAlignment. We'll use entrySP to recover the original stack
    // pointer on return.
    masm.andToStackPtr(Imm32(~(AsmJSStackAlignment - 1)));

    // Bump the stack for the call.
    masm.reserveStack(AlignBytes(StackArgBytes(exp.sig().args()), AsmJSStackAlignment));

    // Copy parameters out of argv and into the registers/stack-slots specified by
    // the system ABI.
    for (ABIArgValTypeIter iter(exp.sig().args()); !iter.done(); iter++) {
        unsigned argOffset = iter.index() * sizeof(ExportArg);
        Address src(argv, argOffset);
        MIRType type = iter.mirType();
        MOZ_ASSERT_IF(type == MIRType::Int64, JitOptions.wasmTestMode);
        switch (iter->kind()) {
          case ABIArg::GPR:
            if (type == MIRType::Int32)
                masm.load32(src, iter->gpr());
            else if (type == MIRType::Int64)
                masm.load64(src, iter->gpr64());
            break;
#ifdef JS_CODEGEN_REGISTER_PAIR
          case ABIArg::GPR_PAIR:
            MOZ_CRASH("wasm uses hardfp for function calls.");
            break;
#endif
          case ABIArg::FPU: {
            static_assert(sizeof(ExportArg) >= jit::Simd128DataSize,
                          "ExportArg must be big enough to store SIMD values");
            switch (type) {
              case MIRType::Int8x16:
              case MIRType::Int16x8:
              case MIRType::Int32x4:
              case MIRType::Bool8x16:
              case MIRType::Bool16x8:
              case MIRType::Bool32x4:
                masm.loadUnalignedSimd128Int(src, iter->fpu());
                break;
              case MIRType::Float32x4:
                masm.loadUnalignedSimd128Float(src, iter->fpu());
                break;
              case MIRType::Double:
                masm.loadDouble(src, iter->fpu());
                break;
              case MIRType::Float32:
                masm.loadFloat32(src, iter->fpu());
                break;
              default:
                MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unexpected FPU type");
                break;
            }
            break;
          }
          case ABIArg::Stack:
            switch (type) {
              case MIRType::Int32:
                masm.load32(src, scratch);
                masm.storePtr(scratch, Address(masm.getStackPointer(), iter->offsetFromArgBase()));
                break;
              case MIRType::Int64:
                masm.load64(src, scratch64);
                masm.store64(scratch64, Address(masm.getStackPointer(), iter->offsetFromArgBase()));
                break;
              case MIRType::Double:
                masm.loadDouble(src, ScratchDoubleReg);
                masm.storeDouble(ScratchDoubleReg,
                                 Address(masm.getStackPointer(), iter->offsetFromArgBase()));
                break;
              case MIRType::Float32:
                masm.loadFloat32(src, ScratchFloat32Reg);
                masm.storeFloat32(ScratchFloat32Reg,
                                  Address(masm.getStackPointer(), iter->offsetFromArgBase()));
                break;
              case MIRType::Int8x16:
              case MIRType::Int16x8:
              case MIRType::Int32x4:
              case MIRType::Bool8x16:
              case MIRType::Bool16x8:
              case MIRType::Bool32x4:
                masm.loadUnalignedSimd128Int(src, ScratchSimd128Reg);
                masm.storeAlignedSimd128Int(
                  ScratchSimd128Reg, Address(masm.getStackPointer(), iter->offsetFromArgBase()));
                break;
              case MIRType::Float32x4:
                masm.loadUnalignedSimd128Float(src, ScratchSimd128Reg);
                masm.storeAlignedSimd128Float(
                  ScratchSimd128Reg, Address(masm.getStackPointer(), iter->offsetFromArgBase()));
                break;
              default:
                MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unexpected stack arg type");
            }
            break;
        }
    }

    // Call into the real function.
    masm.assertStackAlignment(AsmJSStackAlignment);
    masm.call(CallSiteDesc(CallSiteDesc::Relative), exp.funcIndex());

    // Recover the stack pointer value before dynamic alignment.
    masm.loadWasmActivation(scratch);
    masm.loadStackPtr(Address(scratch, WasmActivation::offsetOfEntrySP()));
    masm.setFramePushed(FramePushedForEntrySP);

    // Recover the 'argv' pointer which was saved before aligning the stack.
    masm.Pop(argv);

    // Store the return value in argv[0]
    switch (exp.sig().ret()) {
      case ExprType::Void:
        break;
      case ExprType::I32:
        masm.store32(ReturnReg, Address(argv, 0));
        break;
      case ExprType::I64:
        MOZ_ASSERT(JitOptions.wasmTestMode, "no int64 in asm.js/wasm");
        masm.store64(ReturnReg64, Address(argv, 0));
        break;
      case ExprType::F32:
        masm.convertFloat32ToDouble(ReturnFloat32Reg, ReturnDoubleReg);
        MOZ_FALLTHROUGH; // as ReturnDoubleReg now contains a Double
      case ExprType::F64:
        masm.canonicalizeDouble(ReturnDoubleReg);
        masm.storeDouble(ReturnDoubleReg, Address(argv, 0));
        break;
      case ExprType::I8x16:
      case ExprType::I16x8:
      case ExprType::I32x4:
      case ExprType::B8x16:
      case ExprType::B16x8:
      case ExprType::B32x4:
        // We don't have control on argv alignment, do an unaligned access.
        masm.storeUnalignedSimd128Int(ReturnSimd128Reg, Address(argv, 0));
        break;
      case ExprType::F32x4:
        // We don't have control on argv alignment, do an unaligned access.
        masm.storeUnalignedSimd128Float(ReturnSimd128Reg, Address(argv, 0));
        break;
      case ExprType::Limit:
        MOZ_CRASH("Limit");
    }

    // Restore clobbered non-volatile registers of the caller.
    masm.PopRegsInMask(NonVolatileRegs);
    MOZ_ASSERT(masm.framePushed() == 0);

    masm.move32(Imm32(true), ReturnReg);
    masm.ret();

    offsets.end = masm.currentOffset();
    return offsets;
}

typedef bool ToValue;

static void
FillArgumentArray(MacroAssembler& masm, const ValTypeVector& args, unsigned argOffset,
                  unsigned offsetToCallerStackArgs, Register scratch, ToValue toValue)
{
    Register64 scratch64(scratch);
    for (ABIArgValTypeIter i(args); !i.done(); i++) {
        Address dstAddr(masm.getStackPointer(), argOffset + i.index() * sizeof(Value));

        MIRType type = i.mirType();
        MOZ_ASSERT_IF(type == MIRType::Int64, JitOptions.wasmTestMode);

        switch (i->kind()) {
          case ABIArg::GPR:
            if (type == MIRType::Int32) {
                if (toValue)
                    masm.storeValue(JSVAL_TYPE_INT32, i->gpr(), dstAddr);
                else
                    masm.store32(i->gpr(), dstAddr);
            } else if (type == MIRType::Int64) {
                // We can't box int64 into Values (yet).
                if (toValue)
                    masm.breakpoint();
                else
                    masm.store64(i->gpr64(), dstAddr);
            } else {
                MOZ_CRASH("unexpected input type?");
            }
            break;
#ifdef JS_CODEGEN_REGISTER_PAIR
          case ABIArg::GPR_PAIR:
            MOZ_CRASH("AsmJS uses hardfp for function calls.");
            break;
#endif
          case ABIArg::FPU: {
            MOZ_ASSERT(IsFloatingPointType(type));
            FloatRegister srcReg = i->fpu();
            if (toValue) {
                if (type == MIRType::Float32) {
                    masm.convertFloat32ToDouble(i->fpu(), ScratchDoubleReg);
                    srcReg = ScratchDoubleReg;
                }
                masm.canonicalizeDouble(srcReg);
            }
            masm.storeDouble(srcReg, dstAddr);
            break;
          }
          case ABIArg::Stack:
            if (type == MIRType::Int32) {
                Address src(masm.getStackPointer(), offsetToCallerStackArgs + i->offsetFromArgBase());
                masm.load32(src, scratch);
                if (toValue)
                    masm.storeValue(JSVAL_TYPE_INT32, scratch, dstAddr);
                else
                    masm.store32(scratch, dstAddr);
            } else if (type == MIRType::Int64) {
                // We can't box int64 into Values (yet).
                if (toValue) {
                    masm.breakpoint();
                } else {
                    Address src(masm.getStackPointer(), offsetToCallerStackArgs + i->offsetFromArgBase());
                    masm.load64(src, scratch64);
                    masm.store64(scratch64, dstAddr);
                }
            } else {
                MOZ_ASSERT(IsFloatingPointType(type));
                Address src(masm.getStackPointer(), offsetToCallerStackArgs + i->offsetFromArgBase());
                if (toValue) {
                    if (type == MIRType::Float32) {
                        masm.loadFloat32(src, ScratchFloat32Reg);
                        masm.convertFloat32ToDouble(ScratchFloat32Reg, ScratchDoubleReg);
                    } else {
                        masm.loadDouble(src, ScratchDoubleReg);
                    }
                    masm.canonicalizeDouble(ScratchDoubleReg);
                } else {
                    masm.loadDouble(src, ScratchDoubleReg);
                }
                masm.storeDouble(ScratchDoubleReg, dstAddr);
            }
            break;
        }
    }
}

// Generate a stub that is called via the internal ABI derived from the
// signature of the import and calls into an appropriate callImport C++
// function, having boxed all the ABI arguments into a homogeneous Value array.
ProfilingOffsets
wasm::GenerateInterpExit(MacroAssembler& masm, const Import& import, uint32_t importIndex)
{
    const Sig& sig = import.sig();

    masm.setFramePushed(0);

    // Argument types for Module::callImport_*:
    static const MIRType typeArray[] = { MIRType::Pointer,   // ImportExit
                                         MIRType::Int32,     // argc
                                         MIRType::Pointer }; // argv
    MIRTypeVector invokeArgTypes;
    MOZ_ALWAYS_TRUE(invokeArgTypes.append(typeArray, ArrayLength(typeArray)));

    // At the point of the call, the stack layout shall be (sp grows to the left):
    //   | stack args | padding | Value argv[] | padding | retaddr | caller stack args |
    // The padding between stack args and argv ensures that argv is aligned. The
    // padding between argv and retaddr ensures that sp is aligned.
    unsigned argOffset = AlignBytes(StackArgBytes(invokeArgTypes), sizeof(double));
    unsigned argBytes = Max<size_t>(1, sig.args().length()) * sizeof(Value);
    unsigned framePushed = StackDecrementForCall(masm, ABIStackAlignment, argOffset + argBytes);

    ProfilingOffsets offsets;
    GenerateExitPrologue(masm, framePushed, ExitReason::ImportInterp, &offsets);

    // Fill the argument array.
    unsigned offsetToCallerStackArgs = sizeof(AsmJSFrame) + masm.framePushed();
    Register scratch = ABINonArgReturnReg0;
    FillArgumentArray(masm, sig.args(), argOffset, offsetToCallerStackArgs, scratch, ToValue(false));

    // Prepare the arguments for the call to Module::callImport_*.
    ABIArgMIRTypeIter i(invokeArgTypes);

    // argument 0: importIndex
    if (i->kind() == ABIArg::GPR)
        masm.mov(ImmWord(importIndex), i->gpr());
    else
        masm.store32(Imm32(importIndex), Address(masm.getStackPointer(), i->offsetFromArgBase()));
    i++;

    // argument 1: argc
    unsigned argc = sig.args().length();
    if (i->kind() == ABIArg::GPR)
        masm.mov(ImmWord(argc), i->gpr());
    else
        masm.store32(Imm32(argc), Address(masm.getStackPointer(), i->offsetFromArgBase()));
    i++;

    // argument 2: argv
    Address argv(masm.getStackPointer(), argOffset);
    if (i->kind() == ABIArg::GPR) {
        masm.computeEffectiveAddress(argv, i->gpr());
    } else {
        masm.computeEffectiveAddress(argv, scratch);
        masm.storePtr(scratch, Address(masm.getStackPointer(), i->offsetFromArgBase()));
    }
    i++;
    MOZ_ASSERT(i.done());

    // Make the call, test whether it succeeded, and extract the return value.
    AssertStackAlignment(masm, ABIStackAlignment);
    switch (sig.ret()) {
      case ExprType::Void:
        masm.call(SymbolicAddress::CallImport_Void);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, JumpTarget::Throw);
        break;
      case ExprType::I32:
        masm.call(SymbolicAddress::CallImport_I32);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, JumpTarget::Throw);
        masm.load32(argv, ReturnReg);
        break;
      case ExprType::I64:
        MOZ_ASSERT(JitOptions.wasmTestMode);
        masm.call(SymbolicAddress::CallImport_I64);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, JumpTarget::Throw);
        masm.load64(argv, ReturnReg64);
        break;
      case ExprType::F32:
        masm.call(SymbolicAddress::CallImport_F64);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, JumpTarget::Throw);
        masm.loadDouble(argv, ReturnDoubleReg);
        masm.convertDoubleToFloat32(ReturnDoubleReg, ReturnFloat32Reg);
        break;
      case ExprType::F64:
        masm.call(SymbolicAddress::CallImport_F64);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, JumpTarget::Throw);
        masm.loadDouble(argv, ReturnDoubleReg);
        break;
      case ExprType::I8x16:
      case ExprType::I16x8:
      case ExprType::I32x4:
      case ExprType::F32x4:
      case ExprType::B8x16:
      case ExprType::B16x8:
      case ExprType::B32x4:
        MOZ_CRASH("SIMD types shouldn't be returned from a FFI");
      case ExprType::Limit:
        MOZ_CRASH("Limit");
    }

    GenerateExitEpilogue(masm, framePushed, ExitReason::ImportInterp, &offsets);

    offsets.end = masm.currentOffset();
    return offsets;
}

#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
static const unsigned MaybeSavedGlobalReg = sizeof(void*);
#else
static const unsigned MaybeSavedGlobalReg = 0;
#endif

// Generate a stub that is called via the internal ABI derived from the
// signature of the import and calls into a compatible JIT function,
// having boxed all the ABI arguments into the JIT stack frame layout.
ProfilingOffsets
wasm::GenerateJitExit(MacroAssembler& masm, const Import& import, bool usesHeap)
{
    const Sig& sig = import.sig();

    masm.setFramePushed(0);

    // JIT calls use the following stack layout (sp grows to the left):
    //   | retaddr | descriptor | callee | argc | this | arg1..N |
    // After the JIT frame, the global register (if present) is saved since the
    // JIT's ABI does not preserve non-volatile regs. Also, unlike most ABIs,
    // the JIT ABI requires that sp be JitStackAlignment-aligned *after* pushing
    // the return address.
    static_assert(AsmJSStackAlignment >= JitStackAlignment, "subsumes");
    unsigned sizeOfRetAddr = sizeof(void*);
    unsigned jitFrameBytes = 3 * sizeof(void*) + (1 + sig.args().length()) * sizeof(Value);
    unsigned totalJitFrameBytes = sizeOfRetAddr + jitFrameBytes + MaybeSavedGlobalReg;
    unsigned jitFramePushed = StackDecrementForCall(masm, JitStackAlignment, totalJitFrameBytes) -
                              sizeOfRetAddr;

    ProfilingOffsets offsets;
    GenerateExitPrologue(masm, jitFramePushed, ExitReason::ImportJit, &offsets);

    // 1. Descriptor
    size_t argOffset = 0;
    uint32_t descriptor = MakeFrameDescriptor(jitFramePushed, JitFrame_Entry,
                                              JitFrameLayout::Size());
    masm.storePtr(ImmWord(uintptr_t(descriptor)), Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(size_t);

    // 2. Callee
    Register callee = ABINonArgReturnReg0;   // live until call
    Register scratch = ABINonArgReturnReg1;  // repeatedly clobbered

    // 2.1. Get ExitDatum
    uint32_t globalDataOffset = import.exitGlobalDataOffset();
#if defined(JS_CODEGEN_X64)
    masm.append(AsmJSGlobalAccess(masm.leaRipRelative(callee), globalDataOffset));
#elif defined(JS_CODEGEN_X86)
    masm.append(AsmJSGlobalAccess(masm.movlWithPatch(Imm32(0), callee), globalDataOffset));
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) || \
      defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    masm.computeEffectiveAddress(Address(GlobalReg, globalDataOffset - AsmJSGlobalRegBias), callee);
#endif

    // 2.2. Get callee
    masm.loadPtr(Address(callee, offsetof(ImportExit, fun)), callee);

    // 2.3. Save callee
    masm.storePtr(callee, Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(size_t);

    // 2.4. Load callee executable entry point
    masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrScript()), callee);
    masm.loadBaselineOrIonNoArgCheck(callee, callee, nullptr);

    // 3. Argc
    unsigned argc = sig.args().length();
    masm.storePtr(ImmWord(uintptr_t(argc)), Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(size_t);

    // 4. |this| value
    masm.storeValue(UndefinedValue(), Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(Value);

    // 5. Fill the arguments
    unsigned offsetToCallerStackArgs = jitFramePushed + sizeof(AsmJSFrame);
    FillArgumentArray(masm, sig.args(), argOffset, offsetToCallerStackArgs, scratch, ToValue(true));
    argOffset += sig.args().length() * sizeof(Value);
    MOZ_ASSERT(argOffset == jitFrameBytes);

    // 6. Jit code will clobber all registers, even non-volatiles. GlobalReg and
    //    HeapReg are removed from the general register set for asm.js code, so
    //    these will not have been saved by the caller like all other registers,
    //    so they must be explicitly preserved. Only save GlobalReg since
    //    HeapReg can be reloaded (from global data) after the call.
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    static_assert(MaybeSavedGlobalReg == sizeof(void*), "stack frame accounting");
    masm.storePtr(GlobalReg, Address(masm.getStackPointer(), jitFrameBytes));
#endif

    {
        // Enable Activation.
        //
        // This sequence requires four registers, and needs to preserve the 'callee'
        // register, so there are five live registers.
        MOZ_ASSERT(callee == AsmJSIonExitRegCallee);
        Register reg0 = AsmJSIonExitRegE0;
        Register reg1 = AsmJSIonExitRegE1;
        Register reg2 = AsmJSIonExitRegE2;
        Register reg3 = AsmJSIonExitRegE3;

        // The following is inlined:
        //   JSContext* cx = activation->cx();
        //   Activation* act = cx->runtime()->activation();
        //   act.active_ = true;
        //   act.prevJitTop_ = cx->runtime()->jitTop;
        //   act.prevJitJSContext_ = cx->runtime()->jitJSContext;
        //   cx->runtime()->jitJSContext = cx;
        //   act.prevJitActivation_ = cx->runtime()->jitActivation;
        //   cx->runtime()->jitActivation = act;
        //   act.prevProfilingActivation_ = cx->runtime()->profilingActivation;
        //   cx->runtime()->profilingActivation_ = act;
        // On the ARM store8() uses the secondScratchReg (lr) as a temp.
        size_t offsetOfActivation = JSRuntime::offsetOfActivation();
        size_t offsetOfJitTop = offsetof(JSRuntime, jitTop);
        size_t offsetOfJitJSContext = offsetof(JSRuntime, jitJSContext);
        size_t offsetOfJitActivation = offsetof(JSRuntime, jitActivation);
        size_t offsetOfProfilingActivation = JSRuntime::offsetOfProfilingActivation();
        masm.loadWasmActivation(reg0);
        masm.loadPtr(Address(reg0, WasmActivation::offsetOfContext()), reg3);
        masm.loadPtr(Address(reg3, JSContext::offsetOfRuntime()), reg0);
        masm.loadPtr(Address(reg0, offsetOfActivation), reg1);

        //   act.active_ = true;
        masm.store8(Imm32(1), Address(reg1, JitActivation::offsetOfActiveUint8()));

        //   act.prevJitTop_ = cx->runtime()->jitTop;
        masm.loadPtr(Address(reg0, offsetOfJitTop), reg2);
        masm.storePtr(reg2, Address(reg1, JitActivation::offsetOfPrevJitTop()));

        //   act.prevJitJSContext_ = cx->runtime()->jitJSContext;
        masm.loadPtr(Address(reg0, offsetOfJitJSContext), reg2);
        masm.storePtr(reg2, Address(reg1, JitActivation::offsetOfPrevJitJSContext()));
        //   cx->runtime()->jitJSContext = cx;
        masm.storePtr(reg3, Address(reg0, offsetOfJitJSContext));

        //   act.prevJitActivation_ = cx->runtime()->jitActivation;
        masm.loadPtr(Address(reg0, offsetOfJitActivation), reg2);
        masm.storePtr(reg2, Address(reg1, JitActivation::offsetOfPrevJitActivation()));
        //   cx->runtime()->jitActivation = act;
        masm.storePtr(reg1, Address(reg0, offsetOfJitActivation));

        //   act.prevProfilingActivation_ = cx->runtime()->profilingActivation;
        masm.loadPtr(Address(reg0, offsetOfProfilingActivation), reg2);
        masm.storePtr(reg2, Address(reg1, Activation::offsetOfPrevProfiling()));
        //   cx->runtime()->profilingActivation_ = act;
        masm.storePtr(reg1, Address(reg0, offsetOfProfilingActivation));
    }

    AssertStackAlignment(masm, JitStackAlignment, sizeOfRetAddr);
    masm.callJitNoProfiler(callee);
    AssertStackAlignment(masm, JitStackAlignment, sizeOfRetAddr);

    {
        // Disable Activation.
        //
        // This sequence needs three registers, and must preserve the JSReturnReg_Data and
        // JSReturnReg_Type, so there are five live registers.
        MOZ_ASSERT(JSReturnReg_Data == AsmJSIonExitRegReturnData);
        MOZ_ASSERT(JSReturnReg_Type == AsmJSIonExitRegReturnType);
        Register reg0 = AsmJSIonExitRegD0;
        Register reg1 = AsmJSIonExitRegD1;
        Register reg2 = AsmJSIonExitRegD2;

        // The following is inlined:
        //   rt->profilingActivation = prevProfilingActivation_;
        //   rt->activation()->active_ = false;
        //   rt->jitTop = prevJitTop_;
        //   rt->jitJSContext = prevJitJSContext_;
        //   rt->jitActivation = prevJitActivation_;
        // On the ARM store8() uses the secondScratchReg (lr) as a temp.
        size_t offsetOfActivation = JSRuntime::offsetOfActivation();
        size_t offsetOfJitTop = offsetof(JSRuntime, jitTop);
        size_t offsetOfJitJSContext = offsetof(JSRuntime, jitJSContext);
        size_t offsetOfJitActivation = offsetof(JSRuntime, jitActivation);
        size_t offsetOfProfilingActivation = JSRuntime::offsetOfProfilingActivation();

        masm.movePtr(SymbolicAddress::Runtime, reg0);
        masm.loadPtr(Address(reg0, offsetOfActivation), reg1);

        //   rt->jitTop = prevJitTop_;
        masm.loadPtr(Address(reg1, JitActivation::offsetOfPrevJitTop()), reg2);
        masm.storePtr(reg2, Address(reg0, offsetOfJitTop));

        //   rt->profilingActivation = rt->activation()->prevProfiling_;
        masm.loadPtr(Address(reg1, Activation::offsetOfPrevProfiling()), reg2);
        masm.storePtr(reg2, Address(reg0, offsetOfProfilingActivation));

        //   rt->activation()->active_ = false;
        masm.store8(Imm32(0), Address(reg1, JitActivation::offsetOfActiveUint8()));

        //   rt->jitJSContext = prevJitJSContext_;
        masm.loadPtr(Address(reg1, JitActivation::offsetOfPrevJitJSContext()), reg2);
        masm.storePtr(reg2, Address(reg0, offsetOfJitJSContext));

        //   rt->jitActivation = prevJitActivation_;
        masm.loadPtr(Address(reg1, JitActivation::offsetOfPrevJitActivation()), reg2);
        masm.storePtr(reg2, Address(reg0, offsetOfJitActivation));
    }

    // Reload the global register since JIT code can clobber any register.
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    static_assert(MaybeSavedGlobalReg == sizeof(void*), "stack frame accounting");
    masm.loadPtr(Address(masm.getStackPointer(), jitFrameBytes), GlobalReg);
#endif

    // As explained above, the frame was aligned for the JIT ABI such that
    //   (sp + sizeof(void*)) % JitStackAlignment == 0
    // But now we possibly want to call one of several different C++ functions,
    // so subtract the sizeof(void*) so that sp is aligned for an ABI call.
    static_assert(ABIStackAlignment <= JitStackAlignment, "subsumes");
    masm.reserveStack(sizeOfRetAddr);
    unsigned nativeFramePushed = masm.framePushed();
    AssertStackAlignment(masm, ABIStackAlignment);

    masm.branchTestMagic(Assembler::Equal, JSReturnOperand, JumpTarget::Throw);

    Label oolConvert;
    switch (sig.ret()) {
      case ExprType::Void:
        break;
      case ExprType::I32:
        masm.convertValueToInt32(JSReturnOperand, ReturnDoubleReg, ReturnReg, &oolConvert,
                                 /* -0 check */ false);
        break;
      case ExprType::I64:
        MOZ_ASSERT(JitOptions.wasmTestMode, "no int64 in asm.js/wasm");
        // We don't expect int64 to be returned from Ion yet, because of a
        // guard in callImport.
        masm.breakpoint();
        break;
      case ExprType::F32:
        masm.convertValueToFloat(JSReturnOperand, ReturnFloat32Reg, &oolConvert);
        break;
      case ExprType::F64:
        masm.convertValueToDouble(JSReturnOperand, ReturnDoubleReg, &oolConvert);
        break;
      case ExprType::I8x16:
      case ExprType::I16x8:
      case ExprType::I32x4:
      case ExprType::F32x4:
      case ExprType::B8x16:
      case ExprType::B16x8:
      case ExprType::B32x4:
        MOZ_CRASH("SIMD types shouldn't be returned from an import");
      case ExprType::Limit:
        MOZ_CRASH("Limit");
    }

    Label done;
    masm.bind(&done);

    // Ion code does not respect system callee-saved register conventions so
    // reload the heap register.
    if (usesHeap)
        masm.loadAsmJSHeapRegisterFromGlobalData();

    GenerateExitEpilogue(masm, masm.framePushed(), ExitReason::ImportJit, &offsets);

    if (oolConvert.used()) {
        masm.bind(&oolConvert);
        masm.setFramePushed(nativeFramePushed);

        // Coercion calls use the following stack layout (sp grows to the left):
        //   | args | padding | Value argv[1] | padding | exit AsmJSFrame |
        MIRTypeVector coerceArgTypes;
        JS_ALWAYS_TRUE(coerceArgTypes.append(MIRType::Pointer));
        unsigned offsetToCoerceArgv = AlignBytes(StackArgBytes(coerceArgTypes), sizeof(Value));
        MOZ_ASSERT(nativeFramePushed >= offsetToCoerceArgv + sizeof(Value));
        AssertStackAlignment(masm, ABIStackAlignment);

        // Store return value into argv[0]
        masm.storeValue(JSReturnOperand, Address(masm.getStackPointer(), offsetToCoerceArgv));

        // argument 0: argv
        ABIArgMIRTypeIter i(coerceArgTypes);
        Address argv(masm.getStackPointer(), offsetToCoerceArgv);
        if (i->kind() == ABIArg::GPR) {
            masm.computeEffectiveAddress(argv, i->gpr());
        } else {
            masm.computeEffectiveAddress(argv, scratch);
            masm.storePtr(scratch, Address(masm.getStackPointer(), i->offsetFromArgBase()));
        }
        i++;
        MOZ_ASSERT(i.done());

        // Call coercion function
        AssertStackAlignment(masm, ABIStackAlignment);
        switch (sig.ret()) {
          case ExprType::I32:
            masm.call(SymbolicAddress::CoerceInPlace_ToInt32);
            masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, JumpTarget::Throw);
            masm.unboxInt32(Address(masm.getStackPointer(), offsetToCoerceArgv), ReturnReg);
            break;
          case ExprType::F64:
            masm.call(SymbolicAddress::CoerceInPlace_ToNumber);
            masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, JumpTarget::Throw);
            masm.loadDouble(Address(masm.getStackPointer(), offsetToCoerceArgv), ReturnDoubleReg);
            break;
          case ExprType::F32:
            masm.call(SymbolicAddress::CoerceInPlace_ToNumber);
            masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, JumpTarget::Throw);
            masm.loadDouble(Address(masm.getStackPointer(), offsetToCoerceArgv), ReturnDoubleReg);
            masm.convertDoubleToFloat32(ReturnDoubleReg, ReturnFloat32Reg);
            break;
          default:
            MOZ_CRASH("Unsupported convert type");
        }

        masm.jump(&done);
        masm.setFramePushed(0);
    }

    MOZ_ASSERT(masm.framePushed() == 0);

    offsets.end = masm.currentOffset();
    return offsets;
}

// Generate a stub that is called immediately after the prologue when there is a
// stack overflow. This stub calls a C++ function to report the error and then
// jumps to the throw stub to pop the activation.
static Offsets
GenerateStackOverflow(MacroAssembler& masm)
{
    masm.haltingAlign(CodeAlignment);

    Offsets offsets;
    offsets.begin = masm.currentOffset();

    // If we reach here via the non-profiling prologue, WasmActivation::fp has
    // not been updated. To enable stack unwinding from C++, store to it now. If
    // we reached here via the profiling prologue, we'll just store the same
    // value again. Do not update AsmJSFrame::callerFP as it is not necessary in
    // the non-profiling case (there is no return path from this point) and, in
    // the profiling case, it is already correct.
    Register activation = ABINonArgReturnReg0;
    masm.loadWasmActivation(activation);
    masm.storePtr(masm.getStackPointer(), Address(activation, WasmActivation::offsetOfFP()));

    // Prepare the stack for calling C++.
    if (uint32_t d = StackDecrementForCall(ABIStackAlignment, sizeof(AsmJSFrame), ShadowStackSpace))
        masm.subFromStackPtr(Imm32(d));

    // No need to restore the stack; the throw stub pops everything.
    masm.assertStackAlignment(ABIStackAlignment);
    masm.call(SymbolicAddress::ReportOverRecursed);
    masm.jump(JumpTarget::Throw);

    offsets.end = masm.currentOffset();
    return offsets;
}

// Generate a stub that calls into HandleTrap with the right trap reason.
static Offsets
GenerateTrapStub(MacroAssembler& masm, Trap reason)
{
    masm.haltingAlign(CodeAlignment);

    Offsets offsets;
    offsets.begin = masm.currentOffset();

    // sp can be anything at this point, so ensure it is aligned when calling
    // into C++.  We unconditionally jump to throw so don't worry about
    // restoring sp.
    masm.andToStackPtr(Imm32(~(ABIStackAlignment - 1)));
    if (ShadowStackSpace)
        masm.subFromStackPtr(Imm32(ShadowStackSpace));

    MIRTypeVector args;
    JS_ALWAYS_TRUE(args.append(MIRType::Int32));

    ABIArgMIRTypeIter i(args);
    if (i->kind() == ABIArg::GPR) {
        masm.move32(Imm32(int32_t(reason)), i->gpr());
    } else {
        masm.store32(Imm32(int32_t(reason)),
                     Address(masm.getStackPointer(), i->offsetFromArgBase()));
    }

    i++;
    MOZ_ASSERT(i.done());

    masm.call(SymbolicAddress::HandleTrap);
    masm.jump(JumpTarget::Throw);

    offsets.end = masm.currentOffset();
    return offsets;
}

// If an exception is thrown, simply pop all frames (since asm.js does not
// contain try/catch). To do this:
//  1. Restore 'sp' to it's value right after the PushRegsInMask in GenerateEntry.
//  2. PopRegsInMask to restore the caller's non-volatile registers.
//  3. Return (to CallAsmJS).
static Offsets
GenerateThrow(MacroAssembler& masm)
{
    masm.haltingAlign(CodeAlignment);

    Offsets offsets;
    offsets.begin = masm.currentOffset();

    // We are about to pop all frames in this WasmActivation. Set fp to null to
    // maintain the invariant that fp is either null or pointing to a valid
    // frame.
    Register scratch = ABINonArgReturnReg0;
    masm.loadWasmActivation(scratch);
    masm.storePtr(ImmWord(0), Address(scratch, WasmActivation::offsetOfFP()));

    masm.setFramePushed(FramePushedForEntrySP);
    masm.loadStackPtr(Address(scratch, WasmActivation::offsetOfEntrySP()));
    masm.Pop(scratch);
    masm.PopRegsInMask(NonVolatileRegs);
    MOZ_ASSERT(masm.framePushed() == 0);

    masm.mov(ImmWord(0), ReturnReg);
    masm.ret();

    offsets.end = masm.currentOffset();
    return offsets;
}

Offsets
wasm::GenerateJumpTarget(MacroAssembler& masm, JumpTarget target)
{
    switch (target) {
      case JumpTarget::StackOverflow:
        return GenerateStackOverflow(masm);
      case JumpTarget::Throw:
        return GenerateThrow(masm);
      case JumpTarget::BadIndirectCall:
      case JumpTarget::OutOfBounds:
      case JumpTarget::Unreachable:
      case JumpTarget::IntegerOverflow:
      case JumpTarget::InvalidConversionToInteger:
      case JumpTarget::IntegerDivideByZero:
      case JumpTarget::ImpreciseSimdConversion:
        return GenerateTrapStub(masm, Trap(target));
      case JumpTarget::Limit:
        break;
    }
    MOZ_CRASH("bad JumpTarget");
}

static const LiveRegisterSet AllRegsExceptSP(
    GeneralRegisterSet(Registers::AllMask & ~(uint32_t(1) << Registers::StackPointer)),
    FloatRegisterSet(FloatRegisters::AllMask));

// The async interrupt-callback exit is called from arbitrarily-interrupted wasm
// code. That means we must first save *all* registers and restore *all*
// registers (except the stack pointer) when we resume. The address to resume to
// (assuming that js::HandleExecutionInterrupt doesn't indicate that the
// execution should be aborted) is stored in WasmActivation::resumePC_.
// Unfortunately, loading this requires a scratch register which we don't have
// after restoring all registers. To hack around this, push the resumePC on the
// stack so that it can be popped directly into PC.
Offsets
wasm::GenerateInterruptStub(MacroAssembler& masm)
{
    masm.haltingAlign(CodeAlignment);

    Offsets offsets;
    offsets.begin = masm.currentOffset();

#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    // Be very careful here not to perturb the machine state before saving it
    // to the stack. In particular, add/sub instructions may set conditions in
    // the flags register.
    masm.push(Imm32(0));            // space for resumePC
    masm.pushFlags();               // after this we are safe to use sub
    masm.setFramePushed(0);         // set to zero so we can use masm.framePushed() below
    masm.PushRegsInMask(AllRegsExceptSP); // save all GP/FP registers (except SP)

    Register scratch = ABINonArgReturnReg0;

    // Store resumePC into the reserved space.
    masm.loadWasmActivation(scratch);
    masm.loadPtr(Address(scratch, WasmActivation::offsetOfResumePC()), scratch);
    masm.storePtr(scratch, Address(masm.getStackPointer(), masm.framePushed() + sizeof(void*)));

    // We know that StackPointer is word-aligned, but not necessarily
    // stack-aligned, so we need to align it dynamically.
    masm.moveStackPtrTo(ABINonVolatileReg);
    masm.andToStackPtr(Imm32(~(ABIStackAlignment - 1)));
    if (ShadowStackSpace)
        masm.subFromStackPtr(Imm32(ShadowStackSpace));

    masm.assertStackAlignment(ABIStackAlignment);
    masm.call(SymbolicAddress::HandleExecutionInterrupt);

    masm.branchIfFalseBool(ReturnReg, JumpTarget::Throw);

    // Restore the StackPointer to its position before the call.
    masm.moveToStackPtr(ABINonVolatileReg);

    // Restore the machine state to before the interrupt.
    masm.PopRegsInMask(AllRegsExceptSP); // restore all GP/FP registers (except SP)
    masm.popFlags();              // after this, nothing that sets conditions
    masm.ret();                   // pop resumePC into PC
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    // Reserve space to store resumePC.
    masm.subFromStackPtr(Imm32(sizeof(intptr_t)));
    // set to zero so we can use masm.framePushed() below.
    masm.setFramePushed(0);
    static_assert(!SupportsSimd, "high lanes of SIMD registers need to be saved too.");
    // save all registers,except sp. After this stack is alligned.
    masm.PushRegsInMask(AllRegsExceptSP);

    // Save the stack pointer in a non-volatile register.
    masm.moveStackPtrTo(s0);
    // Align the stack.
    masm.ma_and(StackPointer, StackPointer, Imm32(~(ABIStackAlignment - 1)));

    // Store resumePC into the reserved space.
    masm.loadWasmActivation(IntArgReg0);
    masm.loadPtr(Address(IntArgReg0, WasmActivation::offsetOfResumePC()), IntArgReg1);
    masm.storePtr(IntArgReg1, Address(s0, masm.framePushed()));

# ifdef USES_O32_ABI
    // MIPS ABI requires rewserving stack for registes $a0 to $a3.
    masm.subFromStackPtr(Imm32(4 * sizeof(intptr_t)));
# endif

    masm.assertStackAlignment(ABIStackAlignment);
    masm.call(SymbolicAddress::HandleExecutionInterrupt);

# ifdef USES_O32_ABI
    masm.addToStackPtr(Imm32(4 * sizeof(intptr_t)));
# endif

    masm.branchIfFalseBool(ReturnReg, JumpTarget::Throw);

    // This will restore stack to the address before the call.
    masm.moveToStackPtr(s0);
    masm.PopRegsInMask(AllRegsExceptSP);

    // Pop resumePC into PC. Clobber HeapReg to make the jump and restore it
    // during jump delay slot.
    masm.pop(HeapReg);
    masm.as_jr(HeapReg);
    masm.loadAsmJSHeapRegisterFromGlobalData();
#elif defined(JS_CODEGEN_ARM)
    masm.setFramePushed(0);         // set to zero so we can use masm.framePushed() below

    // Save all GPR, except the stack pointer.
    masm.PushRegsInMask(LiveRegisterSet(
                            GeneralRegisterSet(Registers::AllMask & ~(1<<Registers::sp)),
                            FloatRegisterSet(uint32_t(0))));

    // Save both the APSR and FPSCR in non-volatile registers.
    masm.as_mrs(r4);
    masm.as_vmrs(r5);
    // Save the stack pointer in a non-volatile register.
    masm.mov(sp,r6);
    // Align the stack.
    masm.ma_and(Imm32(~7), sp, sp);

    // Store resumePC into the return PC stack slot.
    masm.loadWasmActivation(IntArgReg0);
    masm.loadPtr(Address(IntArgReg0, WasmActivation::offsetOfResumePC()), IntArgReg1);
    masm.storePtr(IntArgReg1, Address(r6, 14 * sizeof(uint32_t*)));

    // Save all FP registers
    static_assert(!SupportsSimd, "high lanes of SIMD registers need to be saved too.");
    masm.PushRegsInMask(LiveRegisterSet(GeneralRegisterSet(0),
                                        FloatRegisterSet(FloatRegisters::AllDoubleMask)));

    masm.assertStackAlignment(ABIStackAlignment);
    masm.call(SymbolicAddress::HandleExecutionInterrupt);

    masm.branchIfFalseBool(ReturnReg, JumpTarget::Throw);

    // Restore the machine state to before the interrupt. this will set the pc!

    // Restore all FP registers
    masm.PopRegsInMask(LiveRegisterSet(GeneralRegisterSet(0),
                                       FloatRegisterSet(FloatRegisters::AllDoubleMask)));
    masm.mov(r6,sp);
    masm.as_vmsr(r5);
    masm.as_msr(r4);
    // Restore all GP registers
    masm.startDataTransferM(IsLoad, sp, IA, WriteBack);
    masm.transferReg(r0);
    masm.transferReg(r1);
    masm.transferReg(r2);
    masm.transferReg(r3);
    masm.transferReg(r4);
    masm.transferReg(r5);
    masm.transferReg(r6);
    masm.transferReg(r7);
    masm.transferReg(r8);
    masm.transferReg(r9);
    masm.transferReg(r10);
    masm.transferReg(r11);
    masm.transferReg(r12);
    masm.transferReg(lr);
    masm.finishDataTransfer();
    masm.ret();
#elif defined(JS_CODEGEN_ARM64)
    MOZ_CRASH();
#elif defined (JS_CODEGEN_NONE)
    MOZ_CRASH();
#else
# error "Unknown architecture!"
#endif

    offsets.end = masm.currentOffset();
    return offsets;
}
