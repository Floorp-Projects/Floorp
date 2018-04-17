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
#include "wasm/WasmInstance.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::ArrayLength;

typedef Vector<jit::MIRType, 8, SystemAllocPolicy> MIRTypeVector;
typedef jit::ABIArgIter<MIRTypeVector> ABIArgMIRTypeIter;
typedef jit::ABIArgIter<ValTypeVector> ABIArgValTypeIter;

static bool
FinishOffsets(MacroAssembler& masm, Offsets* offsets)
{
    // On old ARM hardware, constant pools could be inserted and they need to
    // be flushed before considering the size of the masm.
    masm.flushBuffer();
    offsets->end = masm.size();
    return !masm.oom();
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

static void
SetupABIArguments(MacroAssembler& masm, const FuncExport& fe, Register argv, Register scratch)
{
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
            else if (type == MIRType::Pointer)
                masm.loadPtr(src, iter->gpr());
            else
                MOZ_CRASH("unknown GPR type");
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
                RegisterOrSP sp = masm.getStackPointer();
#if JS_BITS_PER_WORD == 32
                masm.load32(LowWord(src), scratch);
                masm.store32(scratch, LowWord(Address(sp, iter->offsetFromArgBase())));
                masm.load32(HighWord(src), scratch);
                masm.store32(scratch, HighWord(Address(sp, iter->offsetFromArgBase())));
#else
                Register64 scratch64(scratch);
                masm.load64(src, scratch64);
                masm.store64(scratch64, Address(sp, iter->offsetFromArgBase()));
#endif
                break;
              }
              case MIRType::Pointer:
                masm.loadPtr(src, scratch);
                masm.storePtr(scratch, Address(masm.getStackPointer(), iter->offsetFromArgBase()));
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
          case ABIArg::Uninitialized:
            MOZ_CRASH("Uninitialized ABIArg kind");
        }
    }
}

static void
StoreABIReturn(MacroAssembler& masm, const FuncExport& fe, Register argv)
{
    // Store the return value in argv[0].
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
        masm.canonicalizeFloat(ReturnFloat32Reg);
        masm.storeFloat32(ReturnFloat32Reg, Address(argv, 0));
        break;
      case ExprType::F64:
        masm.canonicalizeDouble(ReturnDoubleReg);
        masm.storeDouble(ReturnDoubleReg, Address(argv, 0));
        break;
      case ExprType::AnyRef:
        masm.storePtr(ReturnReg, Address(argv, 0));
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
}

#if defined(JS_CODEGEN_ARM)
// The ARM system ABI also includes d15 & s31 in the non volatile float registers.
// Also exclude lr (a.k.a. r14) as we preserve it manually.
static const LiveRegisterSet NonVolatileRegs =
    LiveRegisterSet(GeneralRegisterSet(Registers::NonVolatileMask &
                                       ~(uint32_t(1) << Registers::lr)),
                    FloatRegisterSet(FloatRegisters::NonVolatileMask
                                     | (1ULL << FloatRegisters::d15)
                                     | (1ULL << FloatRegisters::s31)));
#elif defined(JS_CODEGEN_ARM64)
// Exclude the Link Register (x30) because it is preserved manually.
//
// Include x16 (scratch) to make a 16-byte aligned amount of integer registers.
// Include d31 (scratch) to make a 16-byte aligned amount of floating registers.
static const LiveRegisterSet NonVolatileRegs =
    LiveRegisterSet(GeneralRegisterSet((Registers::NonVolatileMask &
                                        ~(uint32_t(1) << Registers::lr)) |
                                       (uint32_t(1) << Registers::x16)),
                    FloatRegisterSet(FloatRegisters::NonVolatileMask |
                                     FloatRegisters::NonAllocatableMask));
#else
static const LiveRegisterSet NonVolatileRegs =
    LiveRegisterSet(GeneralRegisterSet(Registers::NonVolatileMask),
                    FloatRegisterSet(FloatRegisters::NonVolatileMask));
#endif

#if defined(JS_CODEGEN_NONE)
static const unsigned NonVolatileRegsPushSize = 0;
#else
static const unsigned NonVolatileRegsPushSize = NonVolatileRegs.gprs().size() * sizeof(intptr_t) +
                                                NonVolatileRegs.fpus().getPushSizeInBytes();
#endif

#ifdef ENABLE_WASM_GC
static const unsigned NumExtraPushed = 2; // tls and argv
#else
static const unsigned NumExtraPushed = 1; // argv
#endif

#ifdef JS_CODEGEN_ARM64
static const unsigned WasmPushSize = 16;
#else
static const unsigned WasmPushSize = sizeof(void*);
#endif

static const unsigned FramePushedBeforeAlign = NonVolatileRegsPushSize + NumExtraPushed * WasmPushSize;

static void
AssertExpectedSP(const MacroAssembler& masm)
{
#ifdef JS_CODEGEN_ARM64
    MOZ_ASSERT(sp.Is(masm.GetStackPointer64()));
#endif
}

template <class Operand>
static void
WasmPush(MacroAssembler& masm, const Operand& op)
{
#ifdef JS_CODEGEN_ARM64
    // Allocate a pad word so that SP can remain properly aligned.
    masm.reserveStack(WasmPushSize);
    masm.storePtr(op, Address(masm.getStackPointer(), 0));
#else
    masm.Push(op);
#endif
}

static void
WasmPop(MacroAssembler& masm, Register r)
{
#ifdef JS_CODEGEN_ARM64
    // Also pop the pad word allocated by WasmPush.
    masm.loadPtr(Address(masm.getStackPointer(), 0), r);
    masm.freeStack(WasmPushSize);
#else
    masm.Pop(r);
#endif
}

static void
MoveSPForJitABI(MacroAssembler& masm)
{
#ifdef JS_CODEGEN_ARM64
    masm.moveStackPtrTo(PseudoStackPointer);
#endif
}

#ifdef ENABLE_WASM_GC
static void
SuppressGC(MacroAssembler& masm, int32_t increment, Register scratch)
{
    masm.loadPtr(Address(WasmTlsReg, offsetof(TlsData, cx)), scratch);
    masm.add32(Imm32(increment),
               Address(scratch, offsetof(JSContext, suppressGC) +
                                js::ThreadData<int32_t>::offsetOfValue()));
}
#endif

static void
CallFuncExport(MacroAssembler& masm, const FuncExport& fe, const Maybe<ImmPtr>& funcPtr)
{
    MOZ_ASSERT(fe.hasEagerStubs() == !funcPtr);
    if (funcPtr)
        masm.call(*funcPtr);
    else
        masm.call(CallSiteDesc(CallSiteDesc::Func), fe.funcIndex());
}

// Generate a stub that enters wasm from a C++ caller via the native ABI. The
// signature of the entry point is Module::ExportFuncPtr. The exported wasm
// function has an ABI derived from its specific signature, so this function
// must map from the ABI of ExportFuncPtr to the export's signature's ABI.
static bool
GenerateInterpEntry(MacroAssembler& masm, const FuncExport& fe, const Maybe<ImmPtr>& funcPtr,
                    HasGcTypes gcTypesEnabled, Offsets* offsets)
{
    AssertExpectedSP(masm);
    masm.haltingAlign(CodeAlignment);

    offsets->begin = masm.currentOffset();

    // Save the return address if it wasn't already saved by the call insn.
#ifdef JS_USE_LINK_REGISTER
# if defined(JS_CODEGEN_ARM) || \
     defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    masm.pushReturnAddress();
# elif defined(JS_CODEGEN_ARM64)
    // WasmPush updates framePushed() unlike pushReturnAddress(), but that's
    // cancelled by the setFramePushed() below.
    WasmPush(masm, lr);
# else
    MOZ_CRASH("Implement this");
# endif
#endif

    // Save all caller non-volatile registers before we clobber them here and in
    // the wasm callee (which does not preserve non-volatile registers).
    masm.setFramePushed(0);
    masm.PushRegsInMask(NonVolatileRegs);
    MOZ_ASSERT(masm.framePushed() == NonVolatileRegsPushSize);

    // Put the 'argv' argument into a non-argument/return/TLS register so that
    // we can use 'argv' while we fill in the arguments for the wasm callee.
    // Use a second non-argument/return register as temporary scratch.
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

#ifdef ENABLE_WASM_GC
    WasmPush(masm, WasmTlsReg);
    if (gcTypesEnabled == HasGcTypes::True)
        SuppressGC(masm, 1, scratch);
#endif

    // Save 'argv' on the stack so that we can recover it after the call.
    WasmPush(masm, argv);

    // Since we're about to dynamically align the stack, reset the frame depth
    // so we can still assert static stack depth balancing.
    MOZ_ASSERT(masm.framePushed() == FramePushedBeforeAlign);
    masm.setFramePushed(0);

    // Dynamically align the stack since ABIStackAlignment is not necessarily
    // WasmStackAlignment. Preserve SP so it can be restored after the call.
#ifdef JS_CODEGEN_ARM64
    static_assert(WasmStackAlignment == 16, "ARM64 SP alignment");
#else
    masm.moveStackPtrTo(scratch);
    masm.andToStackPtr(Imm32(~(WasmStackAlignment - 1)));
    masm.Push(scratch);
#endif

    // Reserve stack space for the call.
    unsigned argDecrement = StackDecrementForCall(WasmStackAlignment,
                                                  masm.framePushed(),
                                                  StackArgBytes(fe.sig().args()));
    masm.reserveStack(argDecrement);

    // Copy parameters out of argv and into the wasm ABI registers/stack-slots.
    SetupABIArguments(masm, fe, argv, scratch);

    // Setup wasm register state. The nullness of the frame pointer is used to
    // determine whether the call ended in success or failure.
    masm.movePtr(ImmWord(0), FramePointer);
    masm.loadWasmPinnedRegsFromTls();

    // Call into the real function. Note that, due to the throw stub, fp, tls
    // and pinned registers may be clobbered.
    masm.assertStackAlignment(WasmStackAlignment);
    CallFuncExport(masm, fe, funcPtr);
    masm.assertStackAlignment(WasmStackAlignment);

    // Pop the arguments pushed after the dynamic alignment.
    masm.freeStack(argDecrement);

    // Pop the stack pointer to its value right before dynamic alignment.
#ifdef JS_CODEGEN_ARM64
    static_assert(WasmStackAlignment == 16, "ARM64 SP alignment");
#else
    masm.PopStackPtr();
#endif
    MOZ_ASSERT(masm.framePushed() == 0);
    masm.setFramePushed(FramePushedBeforeAlign);

    // Recover the 'argv' pointer which was saved before aligning the stack.
    WasmPop(masm, argv);

#ifdef ENABLE_WASM_GC
    WasmPop(masm, WasmTlsReg);
    if (gcTypesEnabled == HasGcTypes::True)
        SuppressGC(masm, -1, WasmTlsReg);
#endif

    // Store the return value in argv[0].
    StoreABIReturn(masm, fe, argv);

    // After the ReturnReg is stored into argv[0] but before fp is clobbered by
    // the PopRegsInMask(NonVolatileRegs) below, set the return value based on
    // whether fp is null (which is the case for successful returns) or the
    // FailFP magic value (set by the throw stub);
    Label success, join;
    masm.branchTestPtr(Assembler::Zero, FramePointer, FramePointer, &success);
#ifdef DEBUG
    Label ok;
    masm.branchPtr(Assembler::Equal, FramePointer, Imm32(FailFP), &ok);
    masm.breakpoint();
    masm.bind(&ok);
#endif
    masm.move32(Imm32(false), ReturnReg);
    masm.jump(&join);
    masm.bind(&success);
    masm.move32(Imm32(true), ReturnReg);
    masm.bind(&join);

    // Restore clobbered non-volatile registers of the caller.
    masm.PopRegsInMask(NonVolatileRegs);
    MOZ_ASSERT(masm.framePushed() == 0);

#if defined(JS_CODEGEN_ARM64)
    masm.setFramePushed(WasmPushSize);
    WasmPop(masm, lr);
    masm.abiret();
#else
    masm.ret();
#endif

    return FinishOffsets(masm, offsets);
}

#ifdef JS_PUNBOX64
static const ValueOperand ScratchValIonEntry = ValueOperand(ABINonArgReg0);
#else
static const ValueOperand ScratchValIonEntry = ValueOperand(ABINonArgReg0, ABINonArgReg1);
#endif
static const Register ScratchIonEntry = ABINonArgReg2;

static void
CallSymbolicAddress(MacroAssembler& masm, bool isAbsolute, SymbolicAddress sym)
{
    if (isAbsolute)
        masm.call(ImmPtr(SymbolicAddressTarget(sym), ImmPtr::NoCheckToken()));
    else
        masm.call(sym);
}

// Load instance's TLS from the callee.
static void
GenerateJitEntryLoadTls(MacroAssembler& masm, unsigned frameSize)
{
    AssertExpectedSP(masm);

    // ScratchIonEntry := callee => JSFunction*
    unsigned offset = frameSize + JitFrameLayout::offsetOfCalleeToken();
    masm.loadFunctionFromCalleeToken(Address(masm.getStackPointer(), offset), ScratchIonEntry);

    // ScratchValIonEntry := callee->getExtendedSlot(WASM_TLSDATA_SLOT)
    //                    => Private(TlsData*)
    offset = FunctionExtended::offsetOfExtendedSlot(FunctionExtended::WASM_TLSDATA_SLOT);
    masm.loadValue(Address(ScratchIonEntry, offset), ScratchValIonEntry);

    // ScratchIonEntry := ScratchIonEntry->toPrivate() => TlsData*
    masm.unboxPrivate(ScratchValIonEntry, WasmTlsReg);
    // \o/
}

// Creates a JS fake exit frame for wasm, so the frame iterators just use
// JSJit frame iteration.
static void
GenerateJitEntryThrow(MacroAssembler& masm, unsigned frameSize)
{
    AssertExpectedSP(masm);

    MOZ_ASSERT(masm.framePushed() == frameSize);

    GenerateJitEntryLoadTls(masm, frameSize);

    masm.freeStack(frameSize);
    MoveSPForJitABI(masm);

    masm.loadPtr(Address(WasmTlsReg, offsetof(TlsData, cx)), ScratchIonEntry);
    masm.enterFakeExitFrameForWasm(ScratchIonEntry, ScratchIonEntry, ExitFrameType::WasmJitEntry);

    masm.loadPtr(Address(WasmTlsReg, offsetof(TlsData, instance)), ScratchIonEntry);
    masm.loadPtr(Address(ScratchIonEntry, Instance::offsetOfJSJitExceptionHandler()),
                 ScratchIonEntry);
    masm.jump(ScratchIonEntry);
}

// Generate a stub that enters wasm from a jit code caller via the jit ABI.
//
// ARM64 note: This does not save the PseudoStackPointer so we must be sure to
// recompute it on every return path, be it normal return or exception return.
// The JIT code we return to assumes it is correct.

static bool
GenerateJitEntry(MacroAssembler& masm, size_t funcExportIndex, const FuncExport& fe,
                 const Maybe<ImmPtr>& funcPtr, HasGcTypes gcTypesEnabled, Offsets* offsets)
{
    AssertExpectedSP(masm);

    RegisterOrSP sp = masm.getStackPointer();

    GenerateJitEntryPrologue(masm, offsets);

    // The jit caller has set up the following stack layout (sp grows to the
    // left):
    // <-- retAddr | descriptor | callee | argc | this | arg1..N

#ifdef ENABLE_WASM_GC
    // Save WasmTlsReg in the uppermost part of the reserved area, because we
    // need it directly after the call.
    unsigned savedTlsSize = AlignBytes(sizeof(void*), WasmStackAlignment);
#else
    unsigned savedTlsSize = 0;
#endif

    unsigned normalBytesNeeded = StackArgBytes(fe.sig().args()) + savedTlsSize;

    MIRTypeVector coerceArgTypes;
    MOZ_ALWAYS_TRUE(coerceArgTypes.append(MIRType::Int32));
    MOZ_ALWAYS_TRUE(coerceArgTypes.append(MIRType::Pointer));
    MOZ_ALWAYS_TRUE(coerceArgTypes.append(MIRType::Pointer));
    unsigned oolBytesNeeded = StackArgBytes(coerceArgTypes);

    unsigned bytesNeeded = Max(normalBytesNeeded, oolBytesNeeded);

    // Note the jit caller ensures the stack is aligned *after* the call
    // instruction.
    unsigned frameSize = StackDecrementForCall(WasmStackAlignment, 0, bytesNeeded);

#ifdef ENABLE_WASM_GC
    unsigned savedTlsOffset = frameSize - sizeof(void*);
#endif

    // Reserve stack space for wasm ABI arguments, set up like this:
    // <-- ABI args | padding
    masm.reserveStack(frameSize);

    GenerateJitEntryLoadTls(masm, frameSize);

    if (fe.sig().hasI64ArgOrRet()) {
        CallSymbolicAddress(masm, !fe.hasEagerStubs(), SymbolicAddress::ReportInt64JSCall);
        GenerateJitEntryThrow(masm, frameSize);
        return FinishOffsets(masm, offsets);
    }

    FloatRegister scratchF = ABINonArgDoubleReg;
    Register scratchG = ScratchIonEntry;
    ValueOperand scratchV = ScratchValIonEntry;

    // We do two loops:
    // - one loop up-front will make sure that all the Value tags fit the
    // expected signature argument types. If at least one inline conversion
    // fails, we just jump to the OOL path which will call into C++. Inline
    // conversions are ordered in the way we expect them to happen the most.
    // - the second loop will unbox the arguments into the right registers.
    Label oolCall;
    for (size_t i = 0; i < fe.sig().args().length(); i++) {
        unsigned jitArgOffset = frameSize + JitFrameLayout::offsetOfActualArg(i);
        Address jitArgAddr(sp, jitArgOffset);
        masm.loadValue(jitArgAddr, scratchV);

        Label next;
        switch (fe.sig().args()[i]) {
          case ValType::I32: {
            ScratchTagScope tag(masm, scratchV);
            masm.splitTagForTest(scratchV, tag);

            // For int32 inputs, just skip.
            masm.branchTestInt32(Assembler::Equal, tag, &next);

            // For double inputs, unbox, truncate and store back.
            Label storeBack, notDouble;
            masm.branchTestDouble(Assembler::NotEqual, tag, &notDouble);
            {
                ScratchTagScopeRelease _(&tag);
                masm.unboxDouble(scratchV, scratchF);
                masm.branchTruncateDoubleMaybeModUint32(scratchF, scratchG, &oolCall);
                masm.jump(&storeBack);
            }
            masm.bind(&notDouble);

            // For null or undefined, store 0.
            Label nullOrUndefined, notNullOrUndefined;
            masm.branchTestUndefined(Assembler::Equal, tag, &nullOrUndefined);
            masm.branchTestNull(Assembler::NotEqual, tag, &notNullOrUndefined);
            masm.bind(&nullOrUndefined);
            {
                ScratchTagScopeRelease _(&tag);
                masm.storeValue(Int32Value(0), jitArgAddr);
            }
            masm.jump(&next);
            masm.bind(&notNullOrUndefined);

            // For booleans, store the number value back. Other types (symbol,
            // object, strings) go to the C++ call.
            masm.branchTestBoolean(Assembler::NotEqual, tag, &oolCall);
            masm.unboxBoolean(scratchV, scratchG);
            // fallthrough:

            masm.bind(&storeBack);
            {
                ScratchTagScopeRelease _(&tag);
                masm.storeValue(JSVAL_TYPE_INT32, scratchG, jitArgAddr);
            }
            break;
          }
          case ValType::F32:
          case ValType::F64: {
            // Note we can reuse the same code for f32/f64 here, since for the
            // case of f32, the conversion of f64 to f32 will happen in the
            // second loop.
            ScratchTagScope tag(masm, scratchV);
            masm.splitTagForTest(scratchV, tag);

            // For double inputs, just skip.
            masm.branchTestDouble(Assembler::Equal, tag, &next);

            // For int32 inputs, convert and rebox.
            Label storeBack, notInt32;
            {
                ScratchTagScopeRelease _(&tag);
                masm.branchTestInt32(Assembler::NotEqual, scratchV, &notInt32);
                masm.int32ValueToDouble(scratchV, scratchF);
                masm.jump(&storeBack);
            }
            masm.bind(&notInt32);

            // For undefined (missing argument), store NaN.
            Label notUndefined;
            masm.branchTestUndefined(Assembler::NotEqual, tag, &notUndefined);
            {
                ScratchTagScopeRelease _(&tag);
                masm.storeValue(DoubleValue(JS::GenericNaN()), jitArgAddr);
                masm.jump(&next);
            }
            masm.bind(&notUndefined);

            // +null is 0.
            Label notNull;
            masm.branchTestNull(Assembler::NotEqual, tag, &notNull);
            {
                ScratchTagScopeRelease _(&tag);
                masm.storeValue(DoubleValue(0.), jitArgAddr);
            }
            masm.jump(&next);
            masm.bind(&notNull);

            // For booleans, store the number value back. Other types (symbol,
            // object, strings) go to the C++ call.
            masm.branchTestBoolean(Assembler::NotEqual, tag, &oolCall);
            masm.boolValueToDouble(scratchV, scratchF);
            // fallthrough:

            masm.bind(&storeBack);
            masm.boxDouble(scratchF, jitArgAddr);
            break;
          }
          default: {
            MOZ_CRASH("unexpected argument type when calling from the jit");
          }
        }
        masm.nopAlign(CodeAlignment);
        masm.bind(&next);
    }

    Label rejoinBeforeCall;
    masm.bind(&rejoinBeforeCall);

    // Convert all the expected values to unboxed values on the stack.
    for (ABIArgValTypeIter iter(fe.sig().args()); !iter.done(); iter++) {
        unsigned jitArgOffset = frameSize + JitFrameLayout::offsetOfActualArg(iter.index());
        Address argv(sp, jitArgOffset);
        bool isStackArg = iter->kind() == ABIArg::Stack;
        switch (iter.mirType()) {
          case MIRType::Int32: {
            Register target = isStackArg ? ScratchIonEntry : iter->gpr();
            masm.unboxInt32(argv, target);
            if (isStackArg)
                masm.storePtr(target, Address(sp, iter->offsetFromArgBase()));
            break;
          }
          case MIRType::Float32: {
            FloatRegister target = isStackArg ? ABINonArgDoubleReg : iter->fpu();
            masm.unboxDouble(argv, ABINonArgDoubleReg);
            masm.convertDoubleToFloat32(ABINonArgDoubleReg, target);
            if (isStackArg)
                masm.storeFloat32(target, Address(sp, iter->offsetFromArgBase()));
            break;
          }
          case MIRType::Double: {
            FloatRegister target = isStackArg ? ABINonArgDoubleReg : iter->fpu();
            masm.unboxDouble(argv, target);
            if (isStackArg)
                masm.storeDouble(target, Address(sp, iter->offsetFromArgBase()));
            break;
          }
          default: {
            MOZ_CRASH("unexpected input argument when calling from jit");
          }
        }
    }

    // Setup wasm register state.
    masm.loadWasmPinnedRegsFromTls();

#ifdef ENABLE_WASM_GC
    if (gcTypesEnabled == HasGcTypes::True) {
        masm.storePtr(WasmTlsReg, Address(sp, savedTlsOffset));
        SuppressGC(masm, 1, ScratchIonEntry);
    }
#endif

    // Call into the real function. Note that, due to the throw stub, fp, tls
    // and pinned registers may be clobbered.
    masm.assertStackAlignment(WasmStackAlignment);
    CallFuncExport(masm, fe, funcPtr);
    masm.assertStackAlignment(WasmStackAlignment);

#ifdef ENABLE_WASM_GC
    if (gcTypesEnabled == HasGcTypes::True) {
        masm.loadPtr(Address(sp, savedTlsOffset), WasmTlsReg);
        SuppressGC(masm, -1, WasmTlsReg);
    }
#endif

    // If fp is equal to the FailFP magic value (set by the throw stub), then
    // report the exception to the JIT caller by jumping into the exception
    // stub; otherwise the FP value is still set to the parent ion frame value.
    Label exception;
    masm.branchPtr(Assembler::Equal, FramePointer, Imm32(FailFP), &exception);

    // Pop arguments.
    masm.freeStack(frameSize);

    // Store the return value in the JSReturnOperand.
    switch (fe.sig().ret()) {
      case ExprType::Void:
        masm.moveValue(UndefinedValue(), JSReturnOperand);
        break;
      case ExprType::I32:
        masm.boxNonDouble(JSVAL_TYPE_INT32, ReturnReg, JSReturnOperand);
        break;
      case ExprType::F32:
        masm.canonicalizeFloat(ReturnFloat32Reg);
        masm.convertFloat32ToDouble(ReturnFloat32Reg, ReturnDoubleReg);
        masm.boxDouble(ReturnDoubleReg, JSReturnOperand, ScratchDoubleReg);
        break;
      case ExprType::F64:
        masm.canonicalizeDouble(ReturnDoubleReg);
        masm.boxDouble(ReturnDoubleReg, JSReturnOperand, ScratchDoubleReg);
        break;
      case ExprType::AnyRef:
        MOZ_CRASH("return anyref in jitentry NYI");
        break;
      case ExprType::I64:
      case ExprType::I8x16:
      case ExprType::I16x8:
      case ExprType::I32x4:
      case ExprType::B8x16:
      case ExprType::B16x8:
      case ExprType::B32x4:
      case ExprType::F32x4:
        MOZ_CRASH("unexpected return type when calling from ion to wasm");
      case ExprType::Limit:
        MOZ_CRASH("Limit");
    }

    MOZ_ASSERT(masm.framePushed() == 0);
#ifdef JS_CODEGEN_ARM64
    masm.loadPtr(Address(sp, 0), lr);
    masm.addToStackPtr(Imm32(8));
    masm.moveStackPtrTo(PseudoStackPointer);
    masm.abiret();
#else
    masm.ret();
#endif

    // Generate an OOL call to the C++ conversion path.
    if (fe.sig().args().length()) {
        masm.bind(&oolCall);
        masm.setFramePushed(frameSize);

        ABIArgMIRTypeIter argsIter(coerceArgTypes);

        // argument 0: function export index.
        if (argsIter->kind() == ABIArg::GPR)
            masm.movePtr(ImmWord(funcExportIndex), argsIter->gpr());
        else
            masm.storePtr(ImmWord(funcExportIndex), Address(sp, argsIter->offsetFromArgBase()));
        argsIter++;

        // argument 1: tlsData
        if (argsIter->kind() == ABIArg::GPR)
            masm.movePtr(WasmTlsReg, argsIter->gpr());
        else
            masm.storePtr(WasmTlsReg, Address(sp, argsIter->offsetFromArgBase()));
        argsIter++;

        // argument 2: effective address of start of argv
        Address argv(sp, masm.framePushed() + JitFrameLayout::offsetOfActualArg(0));
        if (argsIter->kind() == ABIArg::GPR) {
            masm.computeEffectiveAddress(argv, argsIter->gpr());
        } else {
            masm.computeEffectiveAddress(argv, ScratchIonEntry);
            masm.storePtr(ScratchIonEntry, Address(sp, argsIter->offsetFromArgBase()));
        }
        argsIter++;
        MOZ_ASSERT(argsIter.done());

        masm.assertStackAlignment(ABIStackAlignment);
        CallSymbolicAddress(masm, !fe.hasEagerStubs(), SymbolicAddress::CoerceInPlace_JitEntry);
        masm.assertStackAlignment(ABIStackAlignment);

        masm.branchTest32(Assembler::NonZero, ReturnReg, ReturnReg, &rejoinBeforeCall);
    }

    // Prepare to throw: reload WasmTlsReg from the frame.
    masm.bind(&exception);
    masm.setFramePushed(frameSize);
    GenerateJitEntryThrow(masm, frameSize);

    return FinishOffsets(masm, offsets);
}

static void
StackCopy(MacroAssembler& masm, MIRType type, Register scratch, Address src, Address dst)
{
    if (type == MIRType::Int32) {
        masm.load32(src, scratch);
        masm.store32(scratch, dst);
    } else if (type == MIRType::Int64) {
#if JS_BITS_PER_WORD == 32
        masm.load32(LowWord(src), scratch);
        masm.store32(scratch, LowWord(dst));
        masm.load32(HighWord(src), scratch);
        masm.store32(scratch, HighWord(dst));
#else
        Register64 scratch64(scratch);
        masm.load64(src, scratch64);
        masm.store64(scratch64, dst);
#endif
    } else if (type == MIRType::Pointer) {
        masm.loadPtr(src, scratch);
        masm.storePtr(scratch, dst);
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
            } else if (type == MIRType::Pointer) {
                if (toValue)
                    MOZ_CRASH("generating a jit exit for anyref NYI");
                masm.storePtr(i->gpr(), dst);
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
                } else if (type == MIRType::Pointer) {
                    MOZ_CRASH("generating a jit exit for anyref NYI");
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
          case ABIArg::Uninitialized:
            MOZ_CRASH("Uninitialized ABIArg kind");
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
static bool
GenerateImportFunction(jit::MacroAssembler& masm, const FuncImport& fi, SigIdDesc sigId,
                       FuncOffsets* offsets)
{
    AssertExpectedSP(masm);

    GenerateFunctionPrologue(masm, sigId, Nothing(), offsets);

    unsigned framePushed = StackDecrementForCall(masm, WasmStackAlignment, fi.sig().args());
    masm.wasmReserveStackChecked(framePushed, BytecodeOffset(0));
    MOZ_ASSERT(masm.framePushed() == framePushed);

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
    MoveSPForJitABI(masm);
    masm.wasmCallImport(desc, CalleeDesc::import(fi.tlsDataOffset()));

    // Restore the TLS register and pinned regs, per wasm function ABI.
    masm.loadWasmTlsRegFromFrame();
    masm.loadWasmPinnedRegsFromTls();

    GenerateFunctionEpilogue(masm, framePushed, offsets);
    return FinishOffsets(masm, offsets);
}

static const unsigned STUBS_LIFO_DEFAULT_CHUNK_SIZE = 4 * 1024;

bool
wasm::GenerateImportFunctions(const ModuleEnvironment& env, const FuncImportVector& imports,
                              CompiledCode* code)
{
    LifoAlloc lifo(STUBS_LIFO_DEFAULT_CHUNK_SIZE);
    TempAllocator alloc(&lifo);
    WasmMacroAssembler masm(alloc);

    for (uint32_t funcIndex = 0; funcIndex < imports.length(); funcIndex++) {
        const FuncImport& fi = imports[funcIndex];

        FuncOffsets offsets;
        if (!GenerateImportFunction(masm, fi, env.funcSigs[funcIndex]->id, &offsets))
            return false;
        if (!code->codeRanges.emplaceBack(funcIndex, /* bytecodeOffset = */ 0, offsets))
            return false;
    }

    masm.finish();
    if (masm.oom())
        return false;

    return code->swap(masm);
}

// Generate a stub that is called via the internal ABI derived from the
// signature of the import and calls into an appropriate callImport C++
// function, having boxed all the ABI arguments into a homogeneous Value array.
static bool
GenerateImportInterpExit(MacroAssembler& masm, const FuncImport& fi, uint32_t funcImportIndex,
                         Label* throwLabel, CallableOffsets* offsets)
{
    AssertExpectedSP(masm);
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

    GenerateExitPrologue(masm, framePushed, ExitReason::Fixed::ImportInterp, offsets);

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
        masm.jump(throwLabel);
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
      case ExprType::AnyRef:
        masm.call(SymbolicAddress::CallImport_Ref);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        masm.loadPtr(argv, ReturnReg);
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

    GenerateExitEpilogue(masm, framePushed, ExitReason::Fixed::ImportInterp, offsets);

    return FinishOffsets(masm, offsets);
}

// Generate a stub that is called via the internal ABI derived from the
// signature of the import and calls into a compatible JIT function,
// having boxed all the ABI arguments into the JIT stack frame layout.
static bool
GenerateImportJitExit(MacroAssembler& masm, const FuncImport& fi, Label* throwLabel,
                      JitExitOffsets* offsets)
{
    AssertExpectedSP(masm);
    masm.setFramePushed(0);

    // JIT calls use the following stack layout (sp grows to the left):
    //   | retaddr | descriptor | callee | argc | this | arg1..N |
    // After the JIT frame, the global register (if present) is saved since the
    // JIT's ABI does not preserve non-volatile regs. Also, unlike most ABIs,
    // the JIT ABI requires that sp be JitStackAlignment-aligned *after* pushing
    // the return address.
    static_assert(WasmStackAlignment >= JitStackAlignment, "subsumes");
    const unsigned sizeOfRetAddr = sizeof(void*);
    const unsigned sizeOfPreFrame = WasmToJSJitFrameLayout::Size() - sizeOfRetAddr;
    const unsigned sizeOfThisAndArgs = (1 + fi.sig().args().length()) * sizeof(Value);
    const unsigned totalJitFrameBytes = sizeOfRetAddr + sizeOfPreFrame + sizeOfThisAndArgs;
    const unsigned jitFramePushed = StackDecrementForCall(masm, JitStackAlignment, totalJitFrameBytes) -
                                    sizeOfRetAddr;
    const unsigned sizeOfThisAndArgsAndPadding = jitFramePushed - sizeOfPreFrame;

    // On ARM64 we must align the SP to a 16-byte boundary.
#ifdef JS_CODEGEN_ARM64
    const unsigned frameAlignExtra = sizeof(void*);
#else
    const unsigned frameAlignExtra = 0;
#endif

    GenerateJitExitPrologue(masm, jitFramePushed + frameAlignExtra, offsets);

    // 1. Descriptor
    size_t argOffset = frameAlignExtra;
    uint32_t descriptor = MakeFrameDescriptor(sizeOfThisAndArgsAndPadding, JitFrame_WasmToJSJit,
                                              WasmToJSJitFrameLayout::Size());
    masm.storePtr(ImmWord(uintptr_t(descriptor)), Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(size_t);

    // 2. Callee
    Register callee = ABINonArgReturnReg0;   // live until call
    Register scratch = ABINonArgReturnReg1;  // repeatedly clobbered

    // 2.1. Get JSFunction callee
    masm.loadWasmGlobalPtr(fi.tlsDataOffset() + offsetof(FuncImportTls, obj), callee);

    // 2.2. Save callee
    masm.storePtr(callee, Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(size_t);

    // 3. Argc
    unsigned argc = fi.sig().args().length();
    masm.storePtr(ImmWord(uintptr_t(argc)), Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(size_t);
    MOZ_ASSERT(argOffset == sizeOfPreFrame + frameAlignExtra);

    // 4. |this| value
    masm.storeValue(UndefinedValue(), Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(Value);

    // 5. Fill the arguments
    unsigned offsetToCallerStackArgs = jitFramePushed + sizeof(Frame);
    FillArgumentArray(masm, fi.sig().args(), argOffset, offsetToCallerStackArgs, scratch, ToValue(true));
    argOffset += fi.sig().args().length() * sizeof(Value);
    MOZ_ASSERT(argOffset == sizeOfThisAndArgs + sizeOfPreFrame + frameAlignExtra);

    // 6. Check if we need to rectify arguments
    masm.load16ZeroExtend(Address(callee, JSFunction::offsetOfNargs()), scratch);

    Label rectify;
    masm.branch32(Assembler::Above, scratch, Imm32(fi.sig().args().length()), &rectify);

    // 7. If we haven't rectified arguments, load callee executable entry point
    masm.loadJitCodeNoArgCheck(callee, callee);

    Label rejoinBeforeCall;
    masm.bind(&rejoinBeforeCall);

    AssertStackAlignment(masm, JitStackAlignment, sizeOfRetAddr + frameAlignExtra);
#ifdef JS_CODEGEN_ARM64
    // Conform to JIT ABI.
    masm.addToStackPtr(Imm32(8));
#endif
    MoveSPForJitABI(masm);
    masm.callJitNoProfiler(callee);
#ifdef JS_CODEGEN_ARM64
    // Conform to platform conventions - align the SP.
    masm.subFromStackPtr(Imm32(8));
#endif

    // Note that there might be a GC thing in the JSReturnOperand now.
    // In all the code paths from here:
    // - either the value is unboxed because it was a primitive and we don't
    //   need to worry about rooting anymore.
    // - or the value needs to be rooted, but nothing can cause a GC between
    //   here and CoerceInPlace, which roots before coercing to a primitive.

    // The JIT callee clobbers all registers, including WasmTlsReg and
    // FramePointer, so restore those here. During this sequence of
    // instructions, FP can't be trusted by the profiling frame iterator.
    offsets->untrustedFPStart = masm.currentOffset();
    AssertStackAlignment(masm, JitStackAlignment, sizeOfRetAddr + frameAlignExtra);

    masm.loadWasmTlsRegFromFrame();
    masm.moveStackPtrTo(FramePointer);
    masm.addPtr(Imm32(masm.framePushed()), FramePointer);
    offsets->untrustedFPEnd = masm.currentOffset();

    // As explained above, the frame was aligned for the JIT ABI such that
    //   (sp + sizeof(void*)) % JitStackAlignment == 0
    // But now we possibly want to call one of several different C++ functions,
    // so subtract the sizeof(void*) so that sp is aligned for an ABI call.
    static_assert(ABIStackAlignment <= JitStackAlignment, "subsumes");
#ifdef JS_CODEGEN_ARM64
    // We've already allocated the extra space for frame alignment.
    static_assert(sizeOfRetAddr == frameAlignExtra, "ARM64 SP alignment");
#else
    masm.reserveStack(sizeOfRetAddr);
#endif
    unsigned nativeFramePushed = masm.framePushed();
    AssertStackAlignment(masm, ABIStackAlignment);

#ifdef DEBUG
    {
        Label ok;
        masm.branchTestMagic(Assembler::NotEqual, JSReturnOperand, &ok);
        masm.breakpoint();
        masm.bind(&ok);
    }
#endif

    Label oolConvert;
    switch (fi.sig().ret()) {
      case ExprType::Void:
        break;
      case ExprType::I32:
        masm.truncateValueToInt32(JSReturnOperand, ReturnDoubleReg, ReturnReg, &oolConvert);
        break;
      case ExprType::I64:
        masm.breakpoint();
        break;
      case ExprType::F32:
        masm.convertValueToFloat(JSReturnOperand, ReturnFloat32Reg, &oolConvert);
        break;
      case ExprType::F64:
        masm.convertValueToDouble(JSReturnOperand, ReturnDoubleReg, &oolConvert);
        break;
      case ExprType::AnyRef:
        MOZ_CRASH("anyref returned by import (jit exit) NYI");
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

    GenerateJitExitEpilogue(masm, masm.framePushed(), offsets);

    {
        // Call the arguments rectifier.
        masm.bind(&rectify);
        masm.loadPtr(Address(WasmTlsReg, offsetof(TlsData, instance)), callee);
        masm.loadPtr(Address(callee, Instance::offsetOfJSJitArgsRectifier()), callee);
        masm.jump(&rejoinBeforeCall);
    }

    if (oolConvert.used()) {
        masm.bind(&oolConvert);
        masm.setFramePushed(nativeFramePushed);

        // Coercion calls use the following stack layout (sp grows to the left):
        //   | args | padding | Value argv[1] | padding | exit Frame |
        MIRTypeVector coerceArgTypes;
        MOZ_ALWAYS_TRUE(coerceArgTypes.append(MIRType::Pointer));
        unsigned offsetToCoerceArgv = AlignBytes(StackArgBytes(coerceArgTypes), sizeof(Value));
        MOZ_ASSERT(nativeFramePushed >= offsetToCoerceArgv + sizeof(Value));
        AssertStackAlignment(masm, ABIStackAlignment);

        // Store return value into argv[0]
        masm.storeValue(JSReturnOperand, Address(masm.getStackPointer(), offsetToCoerceArgv));

        // From this point, it's safe to reuse the scratch register (which
        // might be part of the JSReturnOperand).

        // The JIT might have clobbered exitFP at this point. Since there's
        // going to be a CoerceInPlace call, pretend we're still doing the JIT
        // call by restoring our tagged exitFP.
        SetExitFP(masm, ExitReason::Fixed::ImportJit, scratch);

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

        // Call coercion function. Note that right after the call, the value of
        // FP is correct because FP is non-volatile in the native ABI.
        AssertStackAlignment(masm, ABIStackAlignment);
        switch (fi.sig().ret()) {
          case ExprType::I32:
            masm.call(SymbolicAddress::CoerceInPlace_ToInt32);
            masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
            masm.unboxInt32(Address(masm.getStackPointer(), offsetToCoerceArgv), ReturnReg);
            break;
          case ExprType::F64:
          case ExprType::F32:
            masm.call(SymbolicAddress::CoerceInPlace_ToNumber);
            masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
            masm.loadDouble(Address(masm.getStackPointer(), offsetToCoerceArgv), ReturnDoubleReg);
            if (fi.sig().ret() == ExprType::F32)
                masm.convertDoubleToFloat32(ReturnDoubleReg, ReturnFloat32Reg);
            break;
          default:
            MOZ_CRASH("Unsupported convert type");
        }

        // Maintain the invariant that exitFP is either unset or not set to a
        // wasm tagged exitFP, per the jit exit contract.
        ClearExitFP(masm, scratch);

        masm.jump(&done);
        masm.setFramePushed(0);
    }

    MOZ_ASSERT(masm.framePushed() == 0);

    return FinishOffsets(masm, offsets);
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
        return ToMIRType(ABIArgType(abi & ArgType_Mask));
    }
};

bool
wasm::GenerateBuiltinThunk(MacroAssembler& masm, ABIFunctionType abiType, ExitReason exitReason,
                           void* funcPtr, CallableOffsets* offsets)
{
    AssertExpectedSP(masm);
    masm.setFramePushed(0);

    ABIFunctionArgs args(abiType);
    uint32_t framePushed = StackDecrementForCall(masm, ABIStackAlignment, args);

    GenerateExitPrologue(masm, framePushed, exitReason, offsets);

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
    MoveSPForJitABI(masm);
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

    GenerateExitEpilogue(masm, framePushed, exitReason, offsets);
    return FinishOffsets(masm, offsets);
}

#if defined(JS_CODEGEN_ARM)
static const LiveRegisterSet RegsToPreserve(
    GeneralRegisterSet(Registers::AllMask & ~((uint32_t(1) << Registers::sp) |
                                              (uint32_t(1) << Registers::pc))),
    FloatRegisterSet(FloatRegisters::AllDoubleMask));
static_assert(!SupportsSimd, "high lanes of SIMD registers need to be saved too.");
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
static const LiveRegisterSet RegsToPreserve(
    GeneralRegisterSet(Registers::AllMask & ~((uint32_t(1) << Registers::k0) |
                                              (uint32_t(1) << Registers::k1) |
                                              (uint32_t(1) << Registers::sp) |
                                              (uint32_t(1) << Registers::zero))),
    FloatRegisterSet(FloatRegisters::AllDoubleMask));
static_assert(!SupportsSimd, "high lanes of SIMD registers need to be saved too.");
#elif defined(JS_CODEGEN_ARM64)
// We assume that traps do not happen while lr is live. This both ensures that
// the size of RegsToPreserve is a multiple of 2 (preserving WasmStackAlignment)
// and gives us a register to clobber in the return path.
static const LiveRegisterSet RegsToPreserve(
    GeneralRegisterSet(Registers::AllMask & ~((uint32_t(1) << Registers::StackPointer) |
                                              (uint32_t(1) << Registers::lr))),
    FloatRegisterSet(FloatRegisters::AllMask));
#else
static const LiveRegisterSet RegsToPreserve(
    GeneralRegisterSet(Registers::AllMask & ~(uint32_t(1) << Registers::StackPointer)),
    FloatRegisterSet(FloatRegisters::AllMask));
#endif

// Generate a stub which calls WasmReportTrap() and can be executed by having
// the signal handler redirect PC from any trapping instruction.
static bool
GenerateTrapExit(MacroAssembler& masm, Label* throwLabel, Offsets* offsets)
{
    AssertExpectedSP(masm);
    masm.haltingAlign(CodeAlignment);

    masm.setFramePushed(0);

    offsets->begin = masm.currentOffset();

    // Traps can only happen at well-defined program points. However, since
    // traps may resume and the optimal assumption for the surrounding code is
    // that registers are not clobbered, we need to preserve all registers in
    // the trap exit. One simplifying assumption is that flags may be clobbered.
    // Push a dummy word to use as return address below.
    WasmPush(masm, ImmWord(0));
    unsigned framePushedBeforePreserve = masm.framePushed();
    masm.PushRegsInMask(RegsToPreserve);
    unsigned offsetOfReturnWord = masm.framePushed() - framePushedBeforePreserve;

    // We know that StackPointer is word-aligned, but not necessarily
    // stack-aligned, so we need to align it dynamically.
    Register preAlignStackPointer = ABINonVolatileReg;
    masm.moveStackPtrTo(preAlignStackPointer);
    masm.andToStackPtr(Imm32(~(ABIStackAlignment - 1)));
    if (ShadowStackSpace)
        masm.subFromStackPtr(Imm32(ShadowStackSpace));

    masm.assertStackAlignment(ABIStackAlignment);
    masm.call(SymbolicAddress::HandleTrap);

    // WasmHandleTrap returns null if control should transfer to the throw stub.
    masm.branchTestPtr(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);

    // Otherwise, the return value is the TrapData::resumePC we must jump to.
    // We must restore register state before jumping, which will clobber
    // ReturnReg, so store ReturnReg in the above-reserved stack slot which we
    // use to jump to via ret.
    masm.moveToStackPtr(preAlignStackPointer);
    masm.storePtr(ReturnReg, Address(masm.getStackPointer(), offsetOfReturnWord));
    masm.PopRegsInMask(RegsToPreserve);
#ifdef JS_CODEGEN_ARM64
    WasmPop(masm, lr);
    masm.abiret();
#else
    masm.ret();
#endif

    return FinishOffsets(masm, offsets);
}

// Generate a stub which is only used by the signal handlers to handle out of
// bounds access by experimental SIMD.js and Atomics and unaligned accesses on
// ARM. This stub is executed by direct PC transfer from the faulting memory
// access and thus the stack depth is unknown. Since
// JitActivation::packedExitFP() is not set before calling the error reporter,
// the current wasm activation will be lost. This stub should be removed when
// SIMD.js and Atomics are moved to wasm and given proper traps and when we use
// a non-faulting strategy for unaligned ARM access.
static bool
GenerateGenericMemoryAccessTrap(MacroAssembler& masm, SymbolicAddress reporter, Label* throwLabel,
                                Offsets* offsets)
{
    AssertExpectedSP(masm);
    masm.haltingAlign(CodeAlignment);

    offsets->begin = masm.currentOffset();

    // sp can be anything at this point, so ensure it is aligned when calling
    // into C++.  We unconditionally jump to throw so don't worry about
    // restoring sp.
    masm.andToStackPtr(Imm32(~(ABIStackAlignment - 1)));
    if (ShadowStackSpace)
        masm.subFromStackPtr(Imm32(ShadowStackSpace));

    masm.call(reporter);
    masm.jump(throwLabel);

    return FinishOffsets(masm, offsets);
}

static bool
GenerateOutOfBoundsExit(MacroAssembler& masm, Label* throwLabel, Offsets* offsets)
{
    return GenerateGenericMemoryAccessTrap(masm, SymbolicAddress::ReportOutOfBounds, throwLabel,
                                           offsets);
}

static bool
GenerateUnalignedExit(MacroAssembler& masm, Label* throwLabel, Offsets* offsets)
{
    return GenerateGenericMemoryAccessTrap(masm, SymbolicAddress::ReportUnalignedAccess, throwLabel,
                                           offsets);
}

// Generate a stub that restores the stack pointer to what it was on entry to
// the wasm activation, sets the return register to 'false' and then executes a
// return which will return from this wasm activation to the caller. This stub
// should only be called after the caller has reported an error.
static bool
GenerateThrowStub(MacroAssembler& masm, Label* throwLabel, Offsets* offsets)
{
    AssertExpectedSP(masm);
    masm.haltingAlign(CodeAlignment);

    masm.bind(throwLabel);

    offsets->begin = masm.currentOffset();

    // Conservatively, the stack pointer can be unaligned and we must align it
    // dynamically.
    masm.andToStackPtr(Imm32(~(ABIStackAlignment - 1)));
    if (ShadowStackSpace)
        masm.subFromStackPtr(Imm32(ShadowStackSpace));

    // WasmHandleThrow unwinds JitActivation::wasmExitFP() and returns the
    // address of the return address on the stack this stub should return to.
    // Set the FramePointer to a magic value to indicate a return by throw.
    masm.call(SymbolicAddress::HandleThrow);
    masm.moveToStackPtr(ReturnReg);
    masm.move32(Imm32(FailFP), FramePointer);
#ifdef JS_CODEGEN_ARM64
    masm.loadPtr(Address(ReturnReg, 0), lr);
    masm.addToStackPtr(Imm32(8));
    masm.abiret();
#else
    masm.ret();
#endif

    return FinishOffsets(masm, offsets);
}

static const LiveRegisterSet AllAllocatableRegs = LiveRegisterSet(
    GeneralRegisterSet(Registers::AllocatableMask),
    FloatRegisterSet(FloatRegisters::AllMask));

// Generate a stub that handle toggable enter/leave frame traps or breakpoints.
// The trap records frame pointer (via GenerateExitPrologue) and saves most of
// registers to not affect the code generated by WasmBaselineCompile.
static bool
GenerateDebugTrapStub(MacroAssembler& masm, Label* throwLabel, CallableOffsets* offsets)
{
    AssertExpectedSP(masm);
    masm.haltingAlign(CodeAlignment);
    masm.setFramePushed(0);

    GenerateExitPrologue(masm, 0, ExitReason::Fixed::DebugTrap, offsets);

    // Save all registers used between baseline compiler operations.
    masm.PushRegsInMask(AllAllocatableRegs);

    uint32_t framePushed = masm.framePushed();

    // This method might be called with unaligned stack -- aligning and
    // saving old stack pointer at the top.
#ifdef JS_CODEGEN_ARM64
    // On ARM64 however the stack is always aligned.
    static_assert(ABIStackAlignment == 16, "ARM64 SP alignment");
#else
    Register scratch = ABINonArgReturnReg0;
    masm.moveStackPtrTo(scratch);
    masm.subFromStackPtr(Imm32(sizeof(intptr_t)));
    masm.andToStackPtr(Imm32(~(ABIStackAlignment - 1)));
    masm.storePtr(scratch, Address(masm.getStackPointer(), 0));
#endif

    if (ShadowStackSpace)
        masm.subFromStackPtr(Imm32(ShadowStackSpace));
    masm.assertStackAlignment(ABIStackAlignment);
    masm.call(SymbolicAddress::HandleDebugTrap);

    masm.branchIfFalseBool(ReturnReg, throwLabel);

    if (ShadowStackSpace)
        masm.addToStackPtr(Imm32(ShadowStackSpace));
#ifndef JS_CODEGEN_ARM64
    masm.Pop(scratch);
    masm.moveToStackPtr(scratch);
#endif

    masm.setFramePushed(framePushed);
    masm.PopRegsInMask(AllAllocatableRegs);

    GenerateExitEpilogue(masm, 0, ExitReason::Fixed::DebugTrap, offsets);

    return FinishOffsets(masm, offsets);
}

bool
wasm::GenerateEntryStubs(MacroAssembler& masm, size_t funcExportIndex, const FuncExport& fe,
                         const Maybe<ImmPtr>& callee, bool isAsmJS, HasGcTypes gcTypesEnabled,
                         CodeRangeVector* codeRanges)
{
    MOZ_ASSERT(!callee == fe.hasEagerStubs());
    MOZ_ASSERT_IF(isAsmJS, fe.hasEagerStubs());

    Offsets offsets;
    if (!GenerateInterpEntry(masm, fe, callee, gcTypesEnabled, &offsets))
        return false;
    if (!codeRanges->emplaceBack(CodeRange::InterpEntry, fe.funcIndex(), offsets))
        return false;

    if (isAsmJS || fe.sig().temporarilyUnsupportedAnyRef())
        return true;

    if (!GenerateJitEntry(masm, funcExportIndex, fe, callee, gcTypesEnabled, &offsets))
        return false;
    if (!codeRanges->emplaceBack(CodeRange::JitEntry, fe.funcIndex(), offsets))
        return false;

    return true;
}

bool
wasm::GenerateStubs(const ModuleEnvironment& env, const FuncImportVector& imports,
                    const FuncExportVector& exports, CompiledCode* code)
{
    LifoAlloc lifo(STUBS_LIFO_DEFAULT_CHUNK_SIZE);
    TempAllocator alloc(&lifo);
    WasmMacroAssembler masm(alloc);

    // Swap in already-allocated empty vectors to avoid malloc/free.
    if (!code->swap(masm))
        return false;

    Label throwLabel;

    JitSpew(JitSpew_Codegen, "# Emitting wasm import stubs");

    for (uint32_t funcIndex = 0; funcIndex < imports.length(); funcIndex++) {
        const FuncImport& fi = imports[funcIndex];

        CallableOffsets interpOffsets;
        if (!GenerateImportInterpExit(masm, fi, funcIndex, &throwLabel, &interpOffsets))
            return false;
        if (!code->codeRanges.emplaceBack(CodeRange::ImportInterpExit, funcIndex, interpOffsets))
            return false;

        if (fi.sig().temporarilyUnsupportedAnyRef())
            continue;

        JitExitOffsets jitOffsets;
        if (!GenerateImportJitExit(masm, fi, &throwLabel, &jitOffsets))
            return false;
        if (!code->codeRanges.emplaceBack(funcIndex, jitOffsets))
            return false;
    }

    JitSpew(JitSpew_Codegen, "# Emitting wasm export stubs");

    Maybe<ImmPtr> noAbsolute;
    for (size_t i = 0; i < exports.length(); i++) {
        const FuncExport& fe = exports[i];
        if (!fe.hasEagerStubs())
            continue;
        if (!GenerateEntryStubs(masm, i, fe, noAbsolute, env.isAsmJS(),
                                env.gcTypesEnabled, &code->codeRanges))
        {
            return false;
        }
    }

    JitSpew(JitSpew_Codegen, "# Emitting wasm exit stubs");

    Offsets offsets;

    if (!GenerateOutOfBoundsExit(masm, &throwLabel, &offsets))
        return false;
    if (!code->codeRanges.emplaceBack(CodeRange::OutOfBoundsExit, offsets))
        return false;

    if (!GenerateUnalignedExit(masm, &throwLabel, &offsets))
        return false;
    if (!code->codeRanges.emplaceBack(CodeRange::UnalignedExit, offsets))
        return false;

    if (!GenerateTrapExit(masm, &throwLabel, &offsets))
        return false;
    if (!code->codeRanges.emplaceBack(CodeRange::TrapExit, offsets))
        return false;

    CallableOffsets callableOffsets;
    if (!GenerateDebugTrapStub(masm, &throwLabel, &callableOffsets))
        return false;
    if (!code->codeRanges.emplaceBack(CodeRange::DebugTrap, callableOffsets))
        return false;

    if (!GenerateThrowStub(masm, &throwLabel, &offsets))
        return false;
    if (!code->codeRanges.emplaceBack(CodeRange::Throw, offsets))
        return false;

    masm.finish();
    if (masm.oom())
        return false;

    return code->swap(masm);
}
