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

#include "wasm/WasmStubs.h"

#include "mozilla/ArrayUtils.h"

#include "wasm/WasmCode.h"
#include "wasm/WasmGenerator.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::ArrayLength;

static void
FinishOffsets(MacroAssembler& masm, Offsets* offsets)
{
    // On old ARM hardware, constant pools could be inserted and they need to
    // be flushed before considering the size of the masm.
    masm.flushBuffer();
    offsets->end = masm.size();
}

static void
AssertStackAlignment(MacroAssembler& masm, uint32_t alignment, uint32_t addBeforeAssert = 0)
{
    MOZ_ASSERT((sizeof(Frame) + masm.framePushed() + addBeforeAssert) % alignment == 0);
    masm.assertStackAlignment(alignment, addBeforeAssert);
}

static unsigned
StackDecrementForCall(MacroAssembler& masm, uint32_t alignment, unsigned bytesToPush)
{
    return StackDecrementForCall(alignment, sizeof(Frame) + masm.framePushed(), bytesToPush);
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

// Generate a stub that enters wasm from a C++ caller via the native ABI. The
// signature of the entry point is Module::ExportFuncPtr. The exported wasm
// function has an ABI derived from its specific signature, so this function
// must map from the ABI of ExportFuncPtr to the export's signature's ABI.
Offsets
wasm::GenerateEntry(MacroAssembler& masm, const FuncExport& fe)
{
    masm.haltingAlign(CodeAlignment);

    Offsets offsets;
    offsets.begin = masm.currentOffset();

    // Save the return address if it wasn't already saved by the call insn.
#if defined(JS_CODEGEN_ARM)
    masm.push(lr);
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    masm.push(ra);
#endif

    // Save all caller non-volatile registers before we clobber them here and in
    // the asm.js callee (which does not preserve non-volatile registers).
    masm.setFramePushed(0);
    masm.PushRegsInMask(NonVolatileRegs);
    MOZ_ASSERT(masm.framePushed() == FramePushedAfterSave);

    // Put the 'argv' argument into a non-argument/return/TLS register so that
    // we can use 'argv' while we fill in the arguments for the asm.js callee.
    Register argv = ABINonArgReturnReg0;
    Register scratch = ABINonArgReturnReg1;

    // Read the arguments of wasm::ExportFuncPtr according to the native ABI.
    // The entry stub's frame is 1 word.
    const unsigned argBase = sizeof(void*) + masm.framePushed();
    ABIArgGenerator abi;
    ABIArg arg;

    // arg 1: ExportArg*
    arg = abi.next(MIRType::Pointer);
    if (arg.kind() == ABIArg::GPR)
        masm.movePtr(arg.gpr(), argv);
    else
        masm.loadPtr(Address(masm.getStackPointer(), argBase + arg.offsetFromArgBase()), argv);

    // Arg 2: TlsData*
    arg = abi.next(MIRType::Pointer);
    if (arg.kind() == ABIArg::GPR)
        masm.movePtr(arg.gpr(), WasmTlsReg);
    else
        masm.loadPtr(Address(masm.getStackPointer(), argBase + arg.offsetFromArgBase()), WasmTlsReg);

    // Setup pinned registers that are assumed throughout wasm code.
    masm.loadWasmPinnedRegsFromTls();

    // Save 'argv' on the stack so that we can recover it after the call. Use
    // a second non-argument/return register as temporary scratch.
    masm.Push(argv);

    // Save the stack pointer in the WasmActivation right before dynamically
    // aligning the stack so that it may be recovered on return or throw.
    MOZ_ASSERT(masm.framePushed() == FramePushedForEntrySP);
    masm.loadWasmActivationFromTls(scratch);
    masm.storeStackPtr(Address(scratch, WasmActivation::offsetOfEntrySP()));

    // Dynamically align the stack since ABIStackAlignment is not necessarily
    // WasmStackAlignment. We'll use entrySP to recover the original stack
    // pointer on return.
    masm.andToStackPtr(Imm32(~(WasmStackAlignment - 1)));

    // Bump the stack for the call.
    masm.reserveStack(AlignBytes(StackArgBytes(fe.sig().args()), WasmStackAlignment));

    // Copy parameters out of argv and into the registers/stack-slots specified by
    // the system ABI.
    for (ABIArgValTypeIter iter(fe.sig().args()); !iter.done(); iter++) {
        unsigned argOffset = iter.index() * sizeof(ExportArg);
        Address src(argv, argOffset);
        MIRType type = iter.mirType();
        switch (iter->kind()) {
          case ABIArg::GPR:
            if (type == MIRType::Int32)
                masm.load32(src, iter->gpr());
            else if (type == MIRType::Int64)
                masm.load64(src, iter->gpr64());
            break;
#ifdef JS_CODEGEN_REGISTER_PAIR
          case ABIArg::GPR_PAIR:
            if (type == MIRType::Int64)
                masm.load64(src, iter->gpr64());
            else
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
              case MIRType::Int64: {
                Register sp = masm.getStackPointer();
#if JS_BITS_PER_WORD == 32
                masm.load32(Address(src.base, src.offset + INT64LOW_OFFSET), scratch);
                masm.store32(scratch, Address(sp, iter->offsetFromArgBase() + INT64LOW_OFFSET));
                masm.load32(Address(src.base, src.offset + INT64HIGH_OFFSET), scratch);
                masm.store32(scratch, Address(sp, iter->offsetFromArgBase() + INT64HIGH_OFFSET));
#else
                Register64 scratch64(scratch);
                masm.load64(src, scratch64);
                masm.store64(scratch64, Address(sp, iter->offsetFromArgBase()));
#endif
                break;
              }
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

    // Set the FramePointer to null for the benefit of debugging.
    masm.movePtr(ImmWord(0), FramePointer);

    // Call into the real function.
    masm.assertStackAlignment(WasmStackAlignment);
    masm.call(CallSiteDesc(CallSiteDesc::Func), fe.funcIndex());
    masm.assertStackAlignment(WasmStackAlignment);

#ifdef DEBUG
    // Assert FramePointer was returned to null by the callee.
    Label ok;
    masm.branchTestPtr(Assembler::Zero, FramePointer, FramePointer, &ok);
    masm.breakpoint();
    masm.bind(&ok);
#endif

    // Recover the stack pointer value before dynamic alignment.
    masm.loadWasmActivationFromTls(scratch);
    masm.wasmAssertNonExitInvariants(scratch);
    masm.loadStackPtr(Address(scratch, WasmActivation::offsetOfEntrySP()));
    masm.setFramePushed(FramePushedForEntrySP);

    // Recover the 'argv' pointer which was saved before aligning the stack.
    masm.Pop(argv);

    // Store the return value in argv[0]
    switch (fe.sig().ret()) {
      case ExprType::Void:
        break;
      case ExprType::I32:
        masm.store32(ReturnReg, Address(argv, 0));
        break;
      case ExprType::I64:
        masm.store64(ReturnReg64, Address(argv, 0));
        break;
      case ExprType::F32:
        if (!JitOptions.wasmTestMode)
            masm.canonicalizeFloat(ReturnFloat32Reg);
        masm.storeFloat32(ReturnFloat32Reg, Address(argv, 0));
        break;
      case ExprType::F64:
        if (!JitOptions.wasmTestMode)
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

    FinishOffsets(masm, &offsets);
    return offsets;
}

static void
StackCopy(MacroAssembler& masm, MIRType type, Register scratch, Address src, Address dst)
{
    if (type == MIRType::Int32) {
        masm.load32(src, scratch);
        masm.store32(scratch, dst);
    } else if (type == MIRType::Int64) {
#if JS_BITS_PER_WORD == 32
        masm.load32(Address(src.base, src.offset + INT64LOW_OFFSET), scratch);
        masm.store32(scratch, Address(dst.base, dst.offset + INT64LOW_OFFSET));
        masm.load32(Address(src.base, src.offset + INT64HIGH_OFFSET), scratch);
        masm.store32(scratch, Address(dst.base, dst.offset + INT64HIGH_OFFSET));
#else
        Register64 scratch64(scratch);
        masm.load64(src, scratch64);
        masm.store64(scratch64, dst);
#endif
    } else if (type == MIRType::Float32) {
        masm.loadFloat32(src, ScratchFloat32Reg);
        masm.storeFloat32(ScratchFloat32Reg, dst);
    } else {
        MOZ_ASSERT(type == MIRType::Double);
        masm.loadDouble(src, ScratchDoubleReg);
        masm.storeDouble(ScratchDoubleReg, dst);
    }
}

typedef bool ToValue;

static void
FillArgumentArray(MacroAssembler& masm, const ValTypeVector& args, unsigned argOffset,
                  unsigned offsetToCallerStackArgs, Register scratch, ToValue toValue)
{
    for (ABIArgValTypeIter i(args); !i.done(); i++) {
        Address dst(masm.getStackPointer(), argOffset + i.index() * sizeof(Value));

        MIRType type = i.mirType();
        switch (i->kind()) {
          case ABIArg::GPR:
            if (type == MIRType::Int32) {
                if (toValue)
                    masm.storeValue(JSVAL_TYPE_INT32, i->gpr(), dst);
                else
                    masm.store32(i->gpr(), dst);
            } else if (type == MIRType::Int64) {
                // We can't box int64 into Values (yet).
                if (toValue)
                    masm.breakpoint();
                else
                    masm.store64(i->gpr64(), dst);
            } else {
                MOZ_CRASH("unexpected input type?");
            }
            break;
#ifdef JS_CODEGEN_REGISTER_PAIR
          case ABIArg::GPR_PAIR:
            if (type == MIRType::Int64)
                masm.store64(i->gpr64(), dst);
            else
                MOZ_CRASH("wasm uses hardfp for function calls.");
            break;
#endif
          case ABIArg::FPU: {
            MOZ_ASSERT(IsFloatingPointType(type));
            FloatRegister srcReg = i->fpu();
            if (type == MIRType::Double) {
                if (toValue) {
                    // Preserve the NaN pattern in the input.
                    masm.moveDouble(srcReg, ScratchDoubleReg);
                    srcReg = ScratchDoubleReg;
                    masm.canonicalizeDouble(srcReg);
                }
                masm.storeDouble(srcReg, dst);
            } else {
                MOZ_ASSERT(type == MIRType::Float32);
                if (toValue) {
                    // JS::Values can't store Float32, so convert to a Double.
                    masm.convertFloat32ToDouble(srcReg, ScratchDoubleReg);
                    masm.canonicalizeDouble(ScratchDoubleReg);
                    masm.storeDouble(ScratchDoubleReg, dst);
                } else {
                    // Preserve the NaN pattern in the input.
                    masm.moveFloat32(srcReg, ScratchFloat32Reg);
                    masm.canonicalizeFloat(ScratchFloat32Reg);
                    masm.storeFloat32(ScratchFloat32Reg, dst);
                }
            }
            break;
          }
          case ABIArg::Stack: {
            Address src(masm.getStackPointer(), offsetToCallerStackArgs + i->offsetFromArgBase());
            if (toValue) {
                if (type == MIRType::Int32) {
                    masm.load32(src, scratch);
                    masm.storeValue(JSVAL_TYPE_INT32, scratch, dst);
                } else if (type == MIRType::Int64) {
                    // We can't box int64 into Values (yet).
                    masm.breakpoint();
                } else {
                    MOZ_ASSERT(IsFloatingPointType(type));
                    if (type == MIRType::Float32) {
                        masm.loadFloat32(src, ScratchFloat32Reg);
                        masm.convertFloat32ToDouble(ScratchFloat32Reg, ScratchDoubleReg);
                    } else {
                        masm.loadDouble(src, ScratchDoubleReg);
                    }
                    masm.canonicalizeDouble(ScratchDoubleReg);
                    masm.storeDouble(ScratchDoubleReg, dst);
                }
            } else {
                StackCopy(masm, type, scratch, src, dst);
            }
            break;
          }
        }
    }
}

// Generate a wrapper function with the standard intra-wasm call ABI which simply
// calls an import. This wrapper function allows any import to be treated like a
// normal wasm function for the purposes of exports and table calls. In
// particular, the wrapper function provides:
//  - a table entry, so JS imports can be put into tables
//  - normal entries, so that, if the import is re-exported, an entry stub can
//    be generated and called without any special cases
FuncOffsets
wasm::GenerateImportFunction(jit::MacroAssembler& masm, const FuncImport& fi, SigIdDesc sigId)
{
    masm.setFramePushed(0);

    unsigned framePushed = StackDecrementForCall(masm, WasmStackAlignment, fi.sig().args());

    FuncOffsets offsets;
    GenerateFunctionPrologue(masm, framePushed, sigId, &offsets);

    // The argument register state is already setup by our caller. We just need
    // to be sure not to clobber it before the call.
    Register scratch = ABINonArgReg0;

    // Copy our frame's stack arguments to the callee frame's stack argument.
    unsigned offsetToCallerStackArgs = sizeof(Frame) + masm.framePushed();
    ABIArgValTypeIter i(fi.sig().args());
    for (; !i.done(); i++) {
        if (i->kind() != ABIArg::Stack)
            continue;

        Address src(masm.getStackPointer(), offsetToCallerStackArgs + i->offsetFromArgBase());
        Address dst(masm.getStackPointer(), i->offsetFromArgBase());
        StackCopy(masm, i.mirType(), scratch, src, dst);
    }

    // Call the import exit stub.
    CallSiteDesc desc(CallSiteDesc::Dynamic);
    masm.wasmCallImport(desc, CalleeDesc::import(fi.tlsDataOffset()));

    // Restore the TLS register and pinned regs, per wasm function ABI.
    masm.loadWasmTlsRegFromFrame();
    masm.loadWasmPinnedRegsFromTls();

    GenerateFunctionEpilogue(masm, framePushed, &offsets);

    masm.wasmEmitTrapOutOfLineCode();

    FinishOffsets(masm, &offsets);
    return offsets;
}

// Generate a stub that is called via the internal ABI derived from the
// signature of the import and calls into an appropriate callImport C++
// function, having boxed all the ABI arguments into a homogeneous Value array.
CallableOffsets
wasm::GenerateImportInterpExit(MacroAssembler& masm, const FuncImport& fi, uint32_t funcImportIndex,
                               Label* throwLabel)
{
    masm.setFramePushed(0);

    // Argument types for Instance::callImport_*:
    static const MIRType typeArray[] = { MIRType::Pointer,   // Instance*
                                         MIRType::Pointer,   // funcImportIndex
                                         MIRType::Int32,     // argc
                                         MIRType::Pointer }; // argv
    MIRTypeVector invokeArgTypes;
    MOZ_ALWAYS_TRUE(invokeArgTypes.append(typeArray, ArrayLength(typeArray)));

    // At the point of the call, the stack layout shall be (sp grows to the left):
    //   | stack args | padding | Value argv[] | padding | retaddr | caller stack args |
    // The padding between stack args and argv ensures that argv is aligned. The
    // padding between argv and retaddr ensures that sp is aligned.
    unsigned argOffset = AlignBytes(StackArgBytes(invokeArgTypes), sizeof(double));
    unsigned argBytes = Max<size_t>(1, fi.sig().args().length()) * sizeof(Value);
    unsigned framePushed = StackDecrementForCall(masm, ABIStackAlignment, argOffset + argBytes);

    CallableOffsets offsets;
    GenerateExitPrologue(masm, framePushed, ExitReason::Fixed::ImportInterp, &offsets);

    // Fill the argument array.
    unsigned offsetToCallerStackArgs = sizeof(Frame) + masm.framePushed();
    Register scratch = ABINonArgReturnReg0;
    FillArgumentArray(masm, fi.sig().args(), argOffset, offsetToCallerStackArgs, scratch, ToValue(false));

    // Prepare the arguments for the call to Instance::callImport_*.
    ABIArgMIRTypeIter i(invokeArgTypes);

    // argument 0: Instance*
    Address instancePtr(WasmTlsReg, offsetof(TlsData, instance));
    if (i->kind() == ABIArg::GPR) {
        masm.loadPtr(instancePtr, i->gpr());
    } else {
        masm.loadPtr(instancePtr, scratch);
        masm.storePtr(scratch, Address(masm.getStackPointer(), i->offsetFromArgBase()));
    }
    i++;

    // argument 1: funcImportIndex
    if (i->kind() == ABIArg::GPR)
        masm.mov(ImmWord(funcImportIndex), i->gpr());
    else
        masm.store32(Imm32(funcImportIndex), Address(masm.getStackPointer(), i->offsetFromArgBase()));
    i++;

    // argument 2: argc
    unsigned argc = fi.sig().args().length();
    if (i->kind() == ABIArg::GPR)
        masm.mov(ImmWord(argc), i->gpr());
    else
        masm.store32(Imm32(argc), Address(masm.getStackPointer(), i->offsetFromArgBase()));
    i++;

    // argument 3: argv
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
    switch (fi.sig().ret()) {
      case ExprType::Void:
        masm.call(SymbolicAddress::CallImport_Void);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        break;
      case ExprType::I32:
        masm.call(SymbolicAddress::CallImport_I32);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        masm.load32(argv, ReturnReg);
        break;
      case ExprType::I64:
        masm.call(SymbolicAddress::CallImport_I64);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        masm.load64(argv, ReturnReg64);
        break;
      case ExprType::F32:
        masm.call(SymbolicAddress::CallImport_F64);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        masm.loadDouble(argv, ReturnDoubleReg);
        masm.convertDoubleToFloat32(ReturnDoubleReg, ReturnFloat32Reg);
        break;
      case ExprType::F64:
        masm.call(SymbolicAddress::CallImport_F64);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
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

    // The native ABI preserves the TLS, heap and global registers since they
    // are non-volatile.
    MOZ_ASSERT(NonVolatileRegs.has(WasmTlsReg));
#if defined(JS_CODEGEN_X64) || \
    defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) || \
    defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    MOZ_ASSERT(NonVolatileRegs.has(HeapReg));
#endif

    GenerateExitEpilogue(masm, framePushed, ExitReason::Fixed::ImportInterp, &offsets);

    FinishOffsets(masm, &offsets);
    return offsets;
}

// Generate a stub that is called via the internal ABI derived from the
// signature of the import and calls into a compatible JIT function,
// having boxed all the ABI arguments into the JIT stack frame layout.
CallableOffsets
wasm::GenerateImportJitExit(MacroAssembler& masm, const FuncImport& fi, Label* throwLabel)
{
    masm.setFramePushed(0);

    // JIT calls use the following stack layout (sp grows to the left):
    //   | retaddr | descriptor | callee | argc | this | arg1..N |
    // After the JIT frame, the global register (if present) is saved since the
    // JIT's ABI does not preserve non-volatile regs. Also, unlike most ABIs,
    // the JIT ABI requires that sp be JitStackAlignment-aligned *after* pushing
    // the return address.
    static_assert(WasmStackAlignment >= JitStackAlignment, "subsumes");
    unsigned sizeOfRetAddr = sizeof(void*);
    unsigned jitFrameBytes = 3 * sizeof(void*) + (1 + fi.sig().args().length()) * sizeof(Value);
    unsigned totalJitFrameBytes = sizeOfRetAddr + jitFrameBytes;
    unsigned jitFramePushed = StackDecrementForCall(masm, JitStackAlignment, totalJitFrameBytes) -
                              sizeOfRetAddr;

    CallableOffsets offsets;
    GenerateExitPrologue(masm, jitFramePushed, ExitReason::Fixed::ImportJit, &offsets);

    // 1. Descriptor
    size_t argOffset = 0;
    uint32_t descriptor = MakeFrameDescriptor(jitFramePushed, JitFrame_Entry,
                                              JitFrameLayout::Size());
    masm.storePtr(ImmWord(uintptr_t(descriptor)), Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(size_t);

    // 2. Callee
    Register callee = ABINonArgReturnReg0;   // live until call
    Register scratch = ABINonArgReturnReg1;  // repeatedly clobbered

    // 2.1. Get callee
    masm.loadWasmGlobalPtr(fi.tlsDataOffset() + offsetof(FuncImportTls, obj), callee);

    // 2.2. Save callee
    masm.storePtr(callee, Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(size_t);

    // 2.3. Load callee executable entry point
    masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrScript()), callee);
    masm.loadBaselineOrIonNoArgCheck(callee, callee, nullptr);

    // 3. Argc
    unsigned argc = fi.sig().args().length();
    masm.storePtr(ImmWord(uintptr_t(argc)), Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(size_t);

    // 4. |this| value
    masm.storeValue(UndefinedValue(), Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(Value);

    // 5. Fill the arguments
    unsigned offsetToCallerStackArgs = jitFramePushed + sizeof(Frame);
    FillArgumentArray(masm, fi.sig().args(), argOffset, offsetToCallerStackArgs, scratch, ToValue(true));
    argOffset += fi.sig().args().length() * sizeof(Value);
    MOZ_ASSERT(argOffset == jitFrameBytes);

    {
        // Enable Activation.
        //
        // This sequence requires two registers, and needs to preserve the
        // 'callee' register, so there are three live registers.
        MOZ_ASSERT(callee == WasmIonExitRegCallee);
        Register cx = WasmIonExitRegE0;
        Register act = WasmIonExitRegE1;

        // JitActivation* act = cx->activation();
        masm.loadPtr(Address(WasmTlsReg, offsetof(TlsData, cx)), cx);
        masm.loadPtr(Address(cx, JSContext::offsetOfActivation()), act);

        // act.active_ = true;
        masm.store8(Imm32(1), Address(act, JitActivation::offsetOfActiveUint8()));

        // cx->jitActivation = act;
        masm.storePtr(act, Address(cx, offsetof(JSContext, jitActivation)));

        // cx->profilingActivation_ = act;
        masm.storePtr(act, Address(cx, JSContext::offsetOfProfilingActivation()));
    }

    AssertStackAlignment(masm, JitStackAlignment, sizeOfRetAddr);
    masm.callJitNoProfiler(callee);
    AssertStackAlignment(masm, JitStackAlignment, sizeOfRetAddr);

    // The JIT callee clobbers all registers, including WasmTlsReg and
    // FramePointer, so restore those here.
    masm.loadWasmTlsRegFromFrame();
    masm.moveStackPtrTo(FramePointer);
    masm.addPtr(Imm32(masm.framePushed()), FramePointer);

    {
        // Disable Activation.
        //
        // This sequence needs three registers and must preserve WasmTlsReg,
        // JSReturnReg_Data and JSReturnReg_Type.
        MOZ_ASSERT(JSReturnReg_Data == WasmIonExitRegReturnData);
        MOZ_ASSERT(JSReturnReg_Type == WasmIonExitRegReturnType);
        MOZ_ASSERT(WasmTlsReg == WasmIonExitTlsReg);
        Register cx = WasmIonExitRegD0;
        Register act = WasmIonExitRegD1;
        Register tmp = WasmIonExitRegD2;

        // JitActivation* act = cx->activation();
        masm.loadPtr(Address(WasmTlsReg, offsetof(TlsData, cx)), cx);
        masm.loadPtr(Address(cx, JSContext::offsetOfActivation()), act);

        // cx->jitTop = act->prevJitTop_;
        masm.loadPtr(Address(act, JitActivation::offsetOfPrevJitTop()), tmp);
        masm.storePtr(tmp, Address(cx, offsetof(JSContext, jitTop)));

        // cx->jitActivation = act->prevJitActivation_;
        masm.loadPtr(Address(act, JitActivation::offsetOfPrevJitActivation()), tmp);
        masm.storePtr(tmp, Address(cx, offsetof(JSContext, jitActivation)));

        // cx->profilingActivation = act->prevProfilingActivation_;
        masm.loadPtr(Address(act, Activation::offsetOfPrevProfiling()), tmp);
        masm.storePtr(tmp, Address(cx, JSContext::offsetOfProfilingActivation()));

        // act->active_ = false;
        masm.store8(Imm32(0), Address(act, JitActivation::offsetOfActiveUint8()));
    }

    // As explained above, the frame was aligned for the JIT ABI such that
    //   (sp + sizeof(void*)) % JitStackAlignment == 0
    // But now we possibly want to call one of several different C++ functions,
    // so subtract the sizeof(void*) so that sp is aligned for an ABI call.
    static_assert(ABIStackAlignment <= JitStackAlignment, "subsumes");
    masm.reserveStack(sizeOfRetAddr);
    unsigned nativeFramePushed = masm.framePushed();
    AssertStackAlignment(masm, ABIStackAlignment);

    masm.branchTestMagic(Assembler::Equal, JSReturnOperand, throwLabel);

    Label oolConvert;
    switch (fi.sig().ret()) {
      case ExprType::Void:
        break;
      case ExprType::I32:
        masm.convertValueToInt32(JSReturnOperand, ReturnDoubleReg, ReturnReg, &oolConvert,
                                 /* -0 check */ false);
        break;
      case ExprType::I64:
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

    GenerateExitEpilogue(masm, masm.framePushed(), ExitReason::Fixed::ImportJit, &offsets);

    if (oolConvert.used()) {
        masm.bind(&oolConvert);
        masm.setFramePushed(nativeFramePushed);

        // Coercion calls use the following stack layout (sp grows to the left):
        //   | args | padding | Value argv[1] | padding | exit Frame |
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
        switch (fi.sig().ret()) {
          case ExprType::I32:
            masm.call(SymbolicAddress::CoerceInPlace_ToInt32);
            masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
            masm.unboxInt32(Address(masm.getStackPointer(), offsetToCoerceArgv), ReturnReg);
            break;
          case ExprType::F64:
            masm.call(SymbolicAddress::CoerceInPlace_ToNumber);
            masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
            masm.loadDouble(Address(masm.getStackPointer(), offsetToCoerceArgv), ReturnDoubleReg);
            break;
          case ExprType::F32:
            masm.call(SymbolicAddress::CoerceInPlace_ToNumber);
            masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
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

    FinishOffsets(masm, &offsets);
    return offsets;
}

struct ABIFunctionArgs
{
    ABIFunctionType abiType;
    size_t len;

    explicit ABIFunctionArgs(ABIFunctionType sig)
      : abiType(ABIFunctionType(sig >> ArgType_Shift))
    {
        len = 0;
        uint32_t i = uint32_t(abiType);
        while (i) {
            i = i >> ArgType_Shift;
            len++;
        }
    }

    size_t length() const { return len; }

    MIRType operator[](size_t i) const {
        MOZ_ASSERT(i < len);
        uint32_t abi = uint32_t(abiType);
        while (i--)
            abi = abi >> ArgType_Shift;
        return ToMIRType(ABIArgType(abi));
    }
};

CallableOffsets
wasm::GenerateBuiltinThunk(MacroAssembler& masm, ABIFunctionType abiType, ExitReason exitReason,
                           void* funcPtr)
{
    masm.setFramePushed(0);

    ABIFunctionArgs args(abiType);
    uint32_t framePushed = StackDecrementForCall(masm, ABIStackAlignment, args);

    CallableOffsets offsets;
    GenerateExitPrologue(masm, framePushed, exitReason, &offsets);

    // Copy out and convert caller arguments, if needed.
    unsigned offsetToCallerStackArgs = sizeof(Frame) + masm.framePushed();
    Register scratch = ABINonArgReturnReg0;
    for (ABIArgIter<ABIFunctionArgs> i(args); !i.done(); i++) {
        if (i->argInRegister()) {
#ifdef JS_CODEGEN_ARM
            // Non hard-fp passes the args values in GPRs.
            if (!UseHardFpABI() && IsFloatingPointType(i.mirType())) {
                FloatRegister input = i->fpu();
                if (i.mirType() == MIRType::Float32) {
                    masm.ma_vxfer(input, Register::FromCode(input.id()));
                } else if (i.mirType() == MIRType::Double) {
                    uint32_t regId = input.singleOverlay().id();
                    masm.ma_vxfer(input, Register::FromCode(regId), Register::FromCode(regId + 1));
                }
            }
#endif
            continue;
        }

        Address src(masm.getStackPointer(), offsetToCallerStackArgs + i->offsetFromArgBase());
        Address dst(masm.getStackPointer(), i->offsetFromArgBase());
        StackCopy(masm, i.mirType(), scratch, src, dst);
    }

    AssertStackAlignment(masm, ABIStackAlignment);
    masm.call(ImmPtr(funcPtr, ImmPtr::NoCheckToken()));

#if defined(JS_CODEGEN_X86)
    // x86 passes the return value on the x87 FP stack.
    Operand op(esp, 0);
    MIRType retType = ToMIRType(ABIArgType(abiType & ArgType_Mask));
    if (retType == MIRType::Float32) {
        masm.fstp32(op);
        masm.loadFloat32(op, ReturnFloat32Reg);
    } else if (retType == MIRType::Double) {
        masm.fstp(op);
        masm.loadDouble(op, ReturnDoubleReg);
    }
#elif defined(JS_CODEGEN_ARM)
    // Non hard-fp passes the return values in GPRs.
    MIRType retType = ToMIRType(ABIArgType(abiType & ArgType_Mask));
    if (!UseHardFpABI() && IsFloatingPointType(retType))
        masm.ma_vxfer(r0, r1, d0);
#endif

    GenerateExitEpilogue(masm, framePushed, exitReason, &offsets);
    offsets.end = masm.currentOffset();
    return offsets;
}

// Generate a stub that calls into ReportTrap with the right trap reason.
// This stub is called with ABIStackAlignment by a trap out-of-line path. An
// exit prologue/epilogue is used so that stack unwinding picks up the
// current WasmActivation. Unwinding will begin at the caller of this trap exit.
CallableOffsets
wasm::GenerateTrapExit(MacroAssembler& masm, Trap trap, Label* throwLabel)
{
    masm.haltingAlign(CodeAlignment);

    masm.setFramePushed(0);

    MIRTypeVector args;
    MOZ_ALWAYS_TRUE(args.append(MIRType::Int32));

    uint32_t framePushed = StackDecrementForCall(masm, ABIStackAlignment, args);

    CallableOffsets offsets;
    GenerateExitPrologue(masm, framePushed, ExitReason::Fixed::Trap, &offsets);

    ABIArgMIRTypeIter i(args);
    if (i->kind() == ABIArg::GPR)
        masm.move32(Imm32(int32_t(trap)), i->gpr());
    else
        masm.store32(Imm32(int32_t(trap)), Address(masm.getStackPointer(), i->offsetFromArgBase()));
    i++;
    MOZ_ASSERT(i.done());

    masm.assertStackAlignment(ABIStackAlignment);
    masm.call(SymbolicAddress::ReportTrap);

    masm.jump(throwLabel);

    GenerateExitEpilogue(masm, framePushed, ExitReason::Fixed::Trap, &offsets);

    FinishOffsets(masm, &offsets);
    return offsets;
}

// Generate a stub which is only used by the signal handlers to handle out of
// bounds access by experimental SIMD.js and Atomics and unaligned accesses on
// ARM. This stub is executed by direct PC transfer from the faulting memory
// access and thus the stack depth is unknown. Since WasmActivation::exitFP is
// not set before calling the error reporter, the current wasm activation will
// be lost. This stub should be removed when SIMD.js and Atomics are moved to
// wasm and given proper traps and when we use a non-faulting strategy for
// unaligned ARM access.
static Offsets
GenerateGenericMemoryAccessTrap(MacroAssembler& masm, SymbolicAddress reporter, Label* throwLabel)
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

    masm.call(reporter);
    masm.jump(throwLabel);

    FinishOffsets(masm, &offsets);
    return offsets;
}

Offsets
wasm::GenerateOutOfBoundsExit(MacroAssembler& masm, Label* throwLabel)
{
    return GenerateGenericMemoryAccessTrap(masm, SymbolicAddress::ReportOutOfBounds, throwLabel);
}

Offsets
wasm::GenerateUnalignedExit(MacroAssembler& masm, Label* throwLabel)
{
    return GenerateGenericMemoryAccessTrap(masm, SymbolicAddress::ReportUnalignedAccess, throwLabel);
}

#if defined(JS_CODEGEN_ARM)
static const LiveRegisterSet AllRegsExceptPCSP(
    GeneralRegisterSet(Registers::AllMask & ~((uint32_t(1) << Registers::sp) |
                                              (uint32_t(1) << Registers::pc))),
    FloatRegisterSet(FloatRegisters::AllDoubleMask));
static_assert(!SupportsSimd, "high lanes of SIMD registers need to be saved too.");
#else
static const LiveRegisterSet AllRegsExceptSP(
    GeneralRegisterSet(Registers::AllMask & ~(uint32_t(1) << Registers::StackPointer)),
    FloatRegisterSet(FloatRegisters::AllMask));
#endif

// The async interrupt-callback exit is called from arbitrarily-interrupted wasm
// code. It calls into the WasmHandleExecutionInterrupt to determine whether we must
// really halt execution which can reenter the VM (e.g., to display the slow
// script dialog). If execution is not interrupted, this stub must carefully
// preserve *all* register state. If execution is interrupted, the entire
// activation will be popped by the throw stub, so register state does not need
// to be restored.
Offsets
wasm::GenerateInterruptExit(MacroAssembler& masm, Label* throwLabel)
{
    masm.haltingAlign(CodeAlignment);

    Offsets offsets;
    offsets.begin = masm.currentOffset();

#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    // Be very careful here not to perturb the machine state before saving it
    // to the stack. In particular, add/sub instructions may set conditions in
    // the flags register.
    masm.push(Imm32(0));            // space used as return address, updated below
    masm.setFramePushed(0);         // set to 0 now so that framePushed is offset of return address
    masm.PushFlags();               // after this we are safe to use sub
    masm.PushRegsInMask(AllRegsExceptSP); // save all GP/FP registers (except SP)

    // We know that StackPointer is word-aligned, but not necessarily
    // stack-aligned, so we need to align it dynamically.
    masm.moveStackPtrTo(ABINonVolatileReg);
    masm.andToStackPtr(Imm32(~(ABIStackAlignment - 1)));
    if (ShadowStackSpace)
        masm.subFromStackPtr(Imm32(ShadowStackSpace));

    // Make the call to C++, which preserves ABINonVolatileReg.
    masm.assertStackAlignment(ABIStackAlignment);
    masm.call(SymbolicAddress::HandleExecutionInterrupt);

    // HandleExecutionInterrupt returns null if execution is interrupted and
    // the resumption pc otherwise.
    masm.branchTestPtr(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);

    // Restore the stack pointer then store resumePC into the stack slow that
    // will be popped by the 'ret' below.
    masm.moveToStackPtr(ABINonVolatileReg);
    masm.storePtr(ReturnReg, Address(StackPointer, masm.framePushed()));

    // Restore the machine state to before the interrupt. After popping flags,
    // no instructions can be executed which set flags.
    masm.PopRegsInMask(AllRegsExceptSP);
    masm.PopFlags();

    // Return to the resumePC stored into this stack slot above.
    MOZ_ASSERT(masm.framePushed() == 0);
    masm.ret();
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    // Reserve space to store resumePC and HeapReg.
    masm.subFromStackPtr(Imm32(2 * sizeof(intptr_t)));
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
    masm.loadWasmActivationFromSymbolicAddress(IntArgReg0);
    masm.loadPtr(Address(IntArgReg0, WasmActivation::offsetOfResumePC()), IntArgReg1);
    masm.storePtr(IntArgReg1, Address(s0, masm.framePushed()));
    // Store HeapReg into the reserved space.
    masm.storePtr(HeapReg, Address(s0, masm.framePushed() + sizeof(intptr_t)));

# ifdef USES_O32_ABI
    // MIPS ABI requires rewserving stack for registes $a0 to $a3.
    masm.subFromStackPtr(Imm32(4 * sizeof(intptr_t)));
# endif

    masm.assertStackAlignment(ABIStackAlignment);
    masm.call(SymbolicAddress::HandleExecutionInterrupt);

# ifdef USES_O32_ABI
    masm.addToStackPtr(Imm32(4 * sizeof(intptr_t)));
# endif

    masm.branchIfFalseBool(ReturnReg, throwLabel);

    // This will restore stack to the address before the call.
    masm.moveToStackPtr(s0);
    masm.PopRegsInMask(AllRegsExceptSP);

    // Pop resumePC into PC. Clobber HeapReg to make the jump and restore it
    // during jump delay slot.
    masm.loadPtr(Address(StackPointer, 0), HeapReg);
    // Reclaim the reserve space.
    masm.addToStackPtr(Imm32(2 * sizeof(intptr_t)));
    masm.as_jr(HeapReg);
    masm.loadPtr(Address(StackPointer, -sizeof(intptr_t)), HeapReg);
#elif defined(JS_CODEGEN_ARM)
    {
        // Be careful not to clobber scratch registers before they are saved.
        ScratchRegisterScope scratch(masm);
        SecondScratchRegisterScope secondScratch(masm);

        // Reserve a word to receive the return address.
        masm.as_alu(StackPointer, StackPointer, Imm8(4), OpSub);

        // Set framePushed to 0 now so that framePushed can be used later as the
        // stack offset to the return-address space reserved above.
        masm.setFramePushed(0);

        // Save all GP/FP registers (except PC and SP).
        masm.PushRegsInMask(AllRegsExceptPCSP);
    }

    // Save SP, APSR and FPSCR in non-volatile registers.
    masm.as_mrs(r4);
    masm.as_vmrs(r5);
    masm.mov(sp, r6);

    // We know that StackPointer is word-aligned, but not necessarily
    // stack-aligned, so we need to align it dynamically.
    masm.andToStackPtr(Imm32(~(ABIStackAlignment - 1)));

    // Make the call to C++, which preserves the non-volatile registers.
    masm.assertStackAlignment(ABIStackAlignment);
    masm.call(SymbolicAddress::HandleExecutionInterrupt);

    // HandleExecutionInterrupt returns null if execution is interrupted and
    // the resumption pc otherwise.
    masm.branchTestPtr(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);

    // Restore the stack pointer then store resumePC into the stack slot that
    // will be popped by the 'ret' below.
    masm.mov(r6, sp);
    masm.storePtr(ReturnReg, Address(sp, masm.framePushed()));

    // Restore the machine state to before the interrupt. After popping flags,
    // no instructions can be executed which set flags.
    masm.as_vmsr(r5);
    masm.as_msr(r4);
    masm.PopRegsInMask(AllRegsExceptPCSP);

    // Return to the resumePC stored into this stack slot above.
    MOZ_ASSERT(masm.framePushed() == 0);
    masm.ret();
#elif defined(JS_CODEGEN_ARM64)
    MOZ_CRASH();
#elif defined (JS_CODEGEN_NONE)
    MOZ_CRASH();
#else
# error "Unknown architecture!"
#endif

    FinishOffsets(masm, &offsets);
    return offsets;
}

// Generate a stub that restores the stack pointer to what it was on entry to
// the wasm activation, sets the return register to 'false' and then executes a
// return which will return from this wasm activation to the caller. This stub
// should only be called after the caller has reported an error (or, in the case
// of the interrupt stub, intends to interrupt execution).
Offsets
wasm::GenerateThrowStub(MacroAssembler& masm, Label* throwLabel)
{
    masm.haltingAlign(CodeAlignment);

    masm.bind(throwLabel);

    Offsets offsets;
    offsets.begin = masm.currentOffset();

    // The following HandleThrow call sets fp of this WasmActivation to null.
    masm.andToStackPtr(Imm32(~(ABIStackAlignment - 1)));
    if (ShadowStackSpace)
        masm.subFromStackPtr(Imm32(ShadowStackSpace));
    masm.call(SymbolicAddress::HandleThrow);

    // HandleThrow returns the innermost WasmActivation* in ReturnReg.
    Register act = ReturnReg;
    masm.wasmAssertNonExitInvariants(act);

    masm.setFramePushed(FramePushedForEntrySP);
    masm.loadStackPtr(Address(act, WasmActivation::offsetOfEntrySP()));
    masm.Pop(ReturnReg);
    masm.PopRegsInMask(NonVolatileRegs);
    MOZ_ASSERT(masm.framePushed() == 0);

    masm.mov(ImmWord(0), ReturnReg);
    masm.ret();

    FinishOffsets(masm, &offsets);
    return offsets;
}

static const LiveRegisterSet AllAllocatableRegs = LiveRegisterSet(
    GeneralRegisterSet(Registers::AllocatableMask),
    FloatRegisterSet(FloatRegisters::AllMask));

// Generate a stub that handle toggable enter/leave frame traps or breakpoints.
// The trap records frame pointer (via GenerateExitPrologue) and saves most of
// registers to not affect the code generated by WasmBaselineCompile.
Offsets
wasm::GenerateDebugTrapStub(MacroAssembler& masm, Label* throwLabel)
{
    masm.haltingAlign(CodeAlignment);

    masm.setFramePushed(0);

    CallableOffsets offsets;
    GenerateExitPrologue(masm, 0, ExitReason::Fixed::DebugTrap, &offsets);

    // Save all registers used between baseline compiler operations.
    masm.PushRegsInMask(AllAllocatableRegs);

    uint32_t framePushed = masm.framePushed();

    // This method might be called with unaligned stack -- aligning and
    // saving old stack pointer at the top.
    Register scratch = ABINonArgReturnReg0;
    masm.moveStackPtrTo(scratch);
    masm.subFromStackPtr(Imm32(sizeof(intptr_t)));
    masm.andToStackPtr(Imm32(~(ABIStackAlignment - 1)));
    masm.storePtr(scratch, Address(masm.getStackPointer(), 0));

    if (ShadowStackSpace)
        masm.subFromStackPtr(Imm32(ShadowStackSpace));
    masm.assertStackAlignment(ABIStackAlignment);
    masm.call(SymbolicAddress::HandleDebugTrap);

    masm.branchIfFalseBool(ReturnReg, throwLabel);

    if (ShadowStackSpace)
        masm.addToStackPtr(Imm32(ShadowStackSpace));
    masm.Pop(scratch);
    masm.moveToStackPtr(scratch);

    masm.setFramePushed(framePushed);
    masm.PopRegsInMask(AllAllocatableRegs);

    GenerateExitEpilogue(masm, 0, ExitReason::Fixed::DebugTrap, &offsets);

    FinishOffsets(masm, &offsets);
    return offsets;
}
