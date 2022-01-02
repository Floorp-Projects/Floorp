/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2016 Mozilla Foundation
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

/*
 * [SMDOC] WebAssembly baseline compiler (RabaldrMonkey)
 *
 * For now, see WasmBCClass.h for general comments about the compiler's
 * structure.
 *
 * ----------------
 *
 * General assumptions for 32-bit vs 64-bit code:
 *
 * - A 32-bit register can be extended in-place to a 64-bit register on 64-bit
 *   systems.
 *
 * - Code that knows that Register64 has a '.reg' member on 64-bit systems and
 *   '.high' and '.low' members on 32-bit systems, or knows the implications
 *   thereof, is #ifdef JS_PUNBOX64.  All other code is #if(n)?def JS_64BIT.
 *
 * Coding standards are a little fluid:
 *
 * - In "small" code generating functions (eg emitMultiplyF64, emitQuotientI32,
 *   and surrounding functions; most functions fall into this class) where the
 *   meaning is obvious:
 *
 *   Old school:
 *   - if there is a single source + destination register, it is called 'r'
 *   - if there is one source and a different destination, they are called 'rs'
 *     and 'rd'
 *   - if there is one source + destination register and another source register
 *     they are called 'r' and 'rs'
 *   - if there are two source registers and a destination register they are
 *     called 'rs0', 'rs1', and 'rd'.
 *
 *   The new thing:
 *   - what is called 'r' in the old-school naming scheme is increasingly called
 *     'rsd' in source+dest cases.
 *
 * - Generic temp registers are named /temp[0-9]?/ not /tmp[0-9]?/.
 *
 * - Registers can be named non-generically for their function ('rp' for the
 *   'pointer' register and 'rv' for the 'value' register are typical) and those
 *   names may or may not have an 'r' prefix.
 *
 * - "Larger" code generating functions make their own rules.
 */

#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmBCClass.h"
#include "wasm/WasmBCDefs.h"
#include "wasm/WasmBCFrame.h"
#include "wasm/WasmBCRegDefs.h"
#include "wasm/WasmBCStk.h"

#include "jit/MacroAssembler-inl.h"
#include "wasm/WasmBCClass-inl.h"
#include "wasm/WasmBCCodegen-inl.h"
#include "wasm/WasmBCRegDefs-inl.h"
#include "wasm/WasmBCRegMgmt-inl.h"
#include "wasm/WasmBCStkMgmt-inl.h"

namespace js {
namespace wasm {

using namespace js::jit;

////////////////////////////////////////////////////////////
//
// Out of line code management.

// The baseline compiler will use OOL code more sparingly than Ion since our
// code is not high performance and frills like code density and branch
// prediction friendliness will be less important.
class OutOfLineCode : public TempObject {
 private:
  NonAssertingLabel entry_;
  NonAssertingLabel rejoin_;
  StackHeight stackHeight_;

 public:
  OutOfLineCode() : stackHeight_(StackHeight::Invalid()) {}

  Label* entry() { return &entry_; }
  Label* rejoin() { return &rejoin_; }

  void setStackHeight(StackHeight stackHeight) {
    MOZ_ASSERT(!stackHeight_.isValid());
    stackHeight_ = stackHeight;
  }

  void bind(BaseStackFrame* fr, MacroAssembler* masm) {
    MOZ_ASSERT(stackHeight_.isValid());
    masm->bind(&entry_);
    fr->setStackHeight(stackHeight_);
  }

  // The generate() method must be careful about register use because it will be
  // invoked when there is a register assignment in the BaseCompiler that does
  // not correspond to the available registers when the generated OOL code is
  // executed.  The register allocator *must not* be called.
  //
  // The best strategy is for the creator of the OOL object to allocate all
  // temps that the OOL code will need.
  //
  // Input, output, and temp registers are embedded in the OOL object and are
  // known to the code generator.
  //
  // Scratch registers are available to use in OOL code.
  //
  // All other registers must be explicitly saved and restored by the OOL code
  // before being used.

  virtual void generate(MacroAssembler* masm) = 0;
};

OutOfLineCode* BaseCompiler::addOutOfLineCode(OutOfLineCode* ool) {
  if (!ool || !outOfLine_.append(ool)) {
    return nullptr;
  }
  ool->setStackHeight(fr.stackHeight());
  return ool;
}

bool BaseCompiler::generateOutOfLineCode() {
  for (auto* ool : outOfLine_) {
    if (!ool->entry()->used()) {
      continue;
    }
    ool->bind(&fr, &masm);
    ool->generate(&masm);
  }

  return !masm.oom();
}

//////////////////////////////////////////////////////////////////////////////
//
// Sundry code generation.

bool BaseCompiler::addInterruptCheck() {
  ScratchI32 tmp(*this);
  fr.loadTlsPtr(tmp);
  masm.wasmInterruptCheck(tmp, bytecodeOffset());
  return createStackMap("addInterruptCheck");
}

void BaseCompiler::checkDivideByZero(RegI32 rhs) {
  Label nonZero;
  masm.branchTest32(Assembler::NonZero, rhs, rhs, &nonZero);
  trap(Trap::IntegerDivideByZero);
  masm.bind(&nonZero);
}

void BaseCompiler::checkDivideByZero(RegI64 r) {
  Label nonZero;
  ScratchI32 scratch(*this);
  masm.branchTest64(Assembler::NonZero, r, r, scratch, &nonZero);
  trap(Trap::IntegerDivideByZero);
  masm.bind(&nonZero);
}

void BaseCompiler::checkDivideSignedOverflow(RegI32 rhs, RegI32 srcDest,
                                             Label* done, bool zeroOnOverflow) {
  Label notMin;
  masm.branch32(Assembler::NotEqual, srcDest, Imm32(INT32_MIN), &notMin);
  if (zeroOnOverflow) {
    masm.branch32(Assembler::NotEqual, rhs, Imm32(-1), &notMin);
    moveImm32(0, srcDest);
    masm.jump(done);
  } else {
    masm.branch32(Assembler::NotEqual, rhs, Imm32(-1), &notMin);
    trap(Trap::IntegerOverflow);
  }
  masm.bind(&notMin);
}

void BaseCompiler::checkDivideSignedOverflow(RegI64 rhs, RegI64 srcDest,
                                             Label* done, bool zeroOnOverflow) {
  Label notmin;
  masm.branch64(Assembler::NotEqual, srcDest, Imm64(INT64_MIN), &notmin);
  masm.branch64(Assembler::NotEqual, rhs, Imm64(-1), &notmin);
  if (zeroOnOverflow) {
    masm.xor64(srcDest, srcDest);
    masm.jump(done);
  } else {
    trap(Trap::IntegerOverflow);
  }
  masm.bind(&notmin);
}

void BaseCompiler::jumpTable(const LabelVector& labels, Label* theTable) {
  // Flush constant pools to ensure that the table is never interrupted by
  // constant pool entries.
  masm.flush();

#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64)
  // Prevent nop sequences to appear in the jump table.
  AutoForbidNops afn(&masm);
#endif
  masm.bind(theTable);

  for (const auto& label : labels) {
    CodeLabel cl;
    masm.writeCodePointer(&cl);
    cl.target()->bind(label.offset());
    masm.addCodeLabel(cl);
  }
}

void BaseCompiler::tableSwitch(Label* theTable, RegI32 switchValue,
                               Label* dispatchCode) {
  masm.bind(dispatchCode);

#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
  ScratchI32 scratch(*this);
  CodeLabel tableCl;

  masm.mov(&tableCl, scratch);

  tableCl.target()->bind(theTable->offset());
  masm.addCodeLabel(tableCl);

  masm.jmp(Operand(scratch, switchValue, ScalePointer));
#elif defined(JS_CODEGEN_ARM)
  // Flush constant pools: offset must reflect the distance from the MOV
  // to the start of the table; as the address of the MOV is given by the
  // label, nothing must come between the bind() and the ma_mov().
  AutoForbidPoolsAndNops afp(&masm,
                             /* number of instructions in scope = */ 5);

  ScratchI32 scratch(*this);

  // Compute the offset from the ma_mov instruction to the jump table.
  Label here;
  masm.bind(&here);
  uint32_t offset = here.offset() - theTable->offset();

  // Read PC+8
  masm.ma_mov(pc, scratch);

  // ARM scratch register is required by ma_sub.
  ScratchRegisterScope arm_scratch(*this);

  // Compute the absolute table base pointer into `scratch`, offset by 8
  // to account for the fact that ma_mov read PC+8.
  masm.ma_sub(Imm32(offset + 8), scratch, arm_scratch);

  // Jump indirect via table element.
  masm.ma_ldr(DTRAddr(scratch, DtrRegImmShift(switchValue, LSL, 2)), pc, Offset,
              Assembler::Always);
#elif defined(JS_CODEGEN_MIPS64)
  ScratchI32 scratch(*this);
  CodeLabel tableCl;

  masm.ma_li(scratch, &tableCl);

  tableCl.target()->bind(theTable->offset());
  masm.addCodeLabel(tableCl);

  masm.branchToComputedAddress(BaseIndex(scratch, switchValue, ScalePointer));
#elif defined(JS_CODEGEN_ARM64)
  AutoForbidPoolsAndNops afp(&masm,
                             /* number of instructions in scope = */ 4);

  ScratchI32 scratch(*this);

  ARMRegister s(scratch, 64);
  ARMRegister v(switchValue, 64);
  masm.Adr(s, theTable);
  masm.Add(s, s, Operand(v, vixl::LSL, 3));
  masm.Ldr(s, MemOperand(s, 0));
  masm.Br(s);
#else
  MOZ_CRASH("BaseCompiler platform hook: tableSwitch");
#endif
}

#ifdef JS_CODEGEN_X86
void BaseCompiler::stashI64(RegPtr regForTls, RegI64 r) {
  MOZ_ASSERT(sizeof(TlsData::baselineScratch) >= 8);
  MOZ_ASSERT(regForTls != r.low && regForTls != r.high);
  fr.loadTlsPtr(regForTls);
  masm.store32(r.low, Address(regForTls, offsetof(TlsData, baselineScratch)));
  masm.store32(r.high,
               Address(regForTls, offsetof(TlsData, baselineScratch) + 4));
}

void BaseCompiler::unstashI64(RegPtr regForTls, RegI64 r) {
  MOZ_ASSERT(sizeof(TlsData::baselineScratch) >= 8);
  fr.loadTlsPtr(regForTls);
  if (regForTls == r.low) {
    masm.load32(Address(regForTls, offsetof(TlsData, baselineScratch) + 4),
                r.high);
    masm.load32(Address(regForTls, offsetof(TlsData, baselineScratch)), r.low);
  } else {
    masm.load32(Address(regForTls, offsetof(TlsData, baselineScratch)), r.low);
    masm.load32(Address(regForTls, offsetof(TlsData, baselineScratch) + 4),
                r.high);
  }
}
#endif

//////////////////////////////////////////////////////////////////////////////
//
// Function entry and exit

bool BaseCompiler::beginFunction() {
  AutoCreatedBy acb(masm, "(wasm)BaseCompiler::beginFunction");

  JitSpew(JitSpew_Codegen, "# ========================================");
  JitSpew(JitSpew_Codegen, "# Emitting wasm baseline code");
  JitSpew(JitSpew_Codegen,
          "# beginFunction: start of function prologue for index %d",
          (int)func_.index);

  // Make a start on the stackmap for this function.  Inspect the args so
  // as to determine which of them are both in-memory and pointer-typed, and
  // add entries to machineStackTracker as appropriate.

  ArgTypeVector args(funcType());
  size_t inboundStackArgBytes = StackArgAreaSizeUnaligned(args);
  MOZ_ASSERT(inboundStackArgBytes % sizeof(void*) == 0);
  stackMapGenerator_.numStackArgWords = inboundStackArgBytes / sizeof(void*);

  MOZ_ASSERT(stackMapGenerator_.machineStackTracker.length() == 0);
  if (!stackMapGenerator_.machineStackTracker.pushNonGCPointers(
          stackMapGenerator_.numStackArgWords)) {
    return false;
  }

  // Identify GC-managed pointers passed on the stack.
  for (WasmABIArgIter i(args); !i.done(); i++) {
    ABIArg argLoc = *i;
    if (argLoc.kind() == ABIArg::Stack &&
        args[i.index()] == MIRType::RefOrNull) {
      uint32_t offset = argLoc.offsetFromArgBase();
      MOZ_ASSERT(offset < inboundStackArgBytes);
      MOZ_ASSERT(offset % sizeof(void*) == 0);
      stackMapGenerator_.machineStackTracker.setGCPointer(offset /
                                                          sizeof(void*));
    }
  }

  GenerateFunctionPrologue(
      masm, *moduleEnv_.funcs[func_.index].typeId,
      compilerEnv_.mode() == CompileMode::Tier1 ? Some(func_.index) : Nothing(),
      &offsets_);

  // GenerateFunctionPrologue pushes exactly one wasm::Frame's worth of
  // stuff, and none of the values are GC pointers.  Hence:
  if (!stackMapGenerator_.machineStackTracker.pushNonGCPointers(
          sizeof(Frame) / sizeof(void*))) {
    return false;
  }

  // Initialize DebugFrame fields before the stack overflow trap so that
  // we have the invariant that all observable Frames in a debugEnabled
  // Module have valid DebugFrames.
  if (compilerEnv_.debugEnabled()) {
#ifdef JS_CODEGEN_ARM64
    static_assert(DebugFrame::offsetOfFrame() % WasmStackAlignment == 0,
                  "aligned");
#endif
    masm.reserveStack(DebugFrame::offsetOfFrame());
    if (!stackMapGenerator_.machineStackTracker.pushNonGCPointers(
            DebugFrame::offsetOfFrame() / sizeof(void*))) {
      return false;
    }

    masm.store32(Imm32(func_.index), Address(masm.getStackPointer(),
                                             DebugFrame::offsetOfFuncIndex()));
    masm.store32(Imm32(0),
                 Address(masm.getStackPointer(), DebugFrame::offsetOfFlags()));

    // No need to initialize cachedReturnJSValue_ or any ref-typed spilled
    // register results, as they are traced if and only if a corresponding
    // flag (hasCachedReturnJSValue or hasSpilledRefRegisterResult) is set.
  }

  // Generate a stack-overflow check and its associated stackmap.

  fr.checkStack(ABINonArgReg0, BytecodeOffset(func_.lineOrBytecode));

  ExitStubMapVector extras;
  if (!stackMapGenerator_.generateStackmapEntriesForTrapExit(args, &extras)) {
    return false;
  }
  if (!createStackMap("stack check", extras, masm.currentOffset(),
                      HasDebugFrameWithLiveRefs::No)) {
    return false;
  }

  size_t reservedBytes = fr.fixedAllocSize() - masm.framePushed();
  MOZ_ASSERT(0 == (reservedBytes % sizeof(void*)));

  masm.reserveStack(reservedBytes);
  fr.onFixedStackAllocated();
  if (!stackMapGenerator_.machineStackTracker.pushNonGCPointers(
          reservedBytes / sizeof(void*))) {
    return false;
  }

  // Locals are stack allocated.  Mark ref-typed ones in the stackmap
  // accordingly.
  for (const Local& l : localInfo_) {
    // Locals that are stack arguments were already added to the stackmap
    // before pushing the frame.
    if (l.type == MIRType::RefOrNull && !l.isStackArgument()) {
      uint32_t offs = fr.localOffsetFromSp(l);
      MOZ_ASSERT(0 == (offs % sizeof(void*)));
      stackMapGenerator_.machineStackTracker.setGCPointer(offs / sizeof(void*));
    }
  }

  // Copy arguments from registers to stack.
  for (WasmABIArgIter i(args); !i.done(); i++) {
    if (args.isSyntheticStackResultPointerArg(i.index())) {
      // If there are stack results and the pointer to stack results
      // was passed in a register, store it to the stack.
      if (i->argInRegister()) {
        fr.storeIncomingStackResultAreaPtr(RegPtr(i->gpr()));
      }
      // If we're in a debug frame, copy the stack result pointer arg
      // to a well-known place.
      if (compilerEnv_.debugEnabled()) {
        Register target = ABINonArgReturnReg0;
        fr.loadIncomingStackResultAreaPtr(RegPtr(target));
        size_t debugFrameOffset =
            masm.framePushed() - DebugFrame::offsetOfFrame();
        size_t debugStackResultsPointerOffset =
            debugFrameOffset + DebugFrame::offsetOfStackResultsPointer();
        masm.storePtr(target, Address(masm.getStackPointer(),
                                      debugStackResultsPointerOffset));
      }
      continue;
    }
    if (!i->argInRegister()) {
      continue;
    }
    Local& l = localInfo_[args.naturalIndex(i.index())];
    switch (i.mirType()) {
      case MIRType::Int32:
        fr.storeLocalI32(RegI32(i->gpr()), l);
        break;
      case MIRType::Int64:
        fr.storeLocalI64(RegI64(i->gpr64()), l);
        break;
      case MIRType::RefOrNull: {
        DebugOnly<uint32_t> offs = fr.localOffsetFromSp(l);
        MOZ_ASSERT(0 == (offs % sizeof(void*)));
        fr.storeLocalRef(RegRef(i->gpr()), l);
        // We should have just visited this local in the preceding loop.
        MOZ_ASSERT(stackMapGenerator_.machineStackTracker.isGCPointer(
            offs / sizeof(void*)));
        break;
      }
      case MIRType::Double:
        fr.storeLocalF64(RegF64(i->fpu()), l);
        break;
      case MIRType::Float32:
        fr.storeLocalF32(RegF32(i->fpu()), l);
        break;
#ifdef ENABLE_WASM_SIMD
      case MIRType::Simd128:
        fr.storeLocalV128(RegV128(i->fpu()), l);
        break;
#endif
      default:
        MOZ_CRASH("Function argument type");
    }
  }

  fr.zeroLocals(&ra);
  fr.storeTlsPtr(WasmTlsReg);

  if (compilerEnv_.debugEnabled()) {
    insertBreakablePoint(CallSiteDesc::EnterFrame);
    if (!createStackMap("debug: enter-frame breakpoint")) {
      return false;
    }
  }

  JitSpew(JitSpew_Codegen,
          "# beginFunction: enter body with masm.framePushed = %u",
          masm.framePushed());
  MOZ_ASSERT(stackMapGenerator_.framePushedAtEntryToBody.isNothing());
  stackMapGenerator_.framePushedAtEntryToBody.emplace(masm.framePushed());

  return true;
}

bool BaseCompiler::endFunction() {
  AutoCreatedBy acb(masm, "(wasm)BaseCompiler::endFunction");

  JitSpew(JitSpew_Codegen, "# endFunction: start of function epilogue");

  // Always branch to returnLabel_.
  masm.breakpoint();

  // Patch the add in the prologue so that it checks against the correct
  // frame size. Flush the constant pool in case it needs to be patched.
  masm.flush();

  // Precondition for patching.
  if (masm.oom()) {
    return false;
  }

  fr.patchCheckStack();

  masm.bind(&returnLabel_);

  ResultType resultType(ResultType::Vector(funcType().results()));

  popStackReturnValues(resultType);

  if (compilerEnv_.debugEnabled()) {
    // Store and reload the return value from DebugFrame::return so that
    // it can be clobbered, and/or modified by the debug trap.
    saveRegisterReturnValues(resultType);
    insertBreakablePoint(CallSiteDesc::Breakpoint);
    if (!createStackMap("debug: return-point breakpoint",
                        HasDebugFrameWithLiveRefs::Maybe)) {
      return false;
    }
    insertBreakablePoint(CallSiteDesc::LeaveFrame);
    if (!createStackMap("debug: leave-frame breakpoint",
                        HasDebugFrameWithLiveRefs::Maybe)) {
      return false;
    }
    restoreRegisterReturnValues(resultType);
  }

  // To satisy Tls extent invariant we need to reload WasmTlsReg because
  // baseline can clobber it.
  fr.loadTlsPtr(WasmTlsReg);
  GenerateFunctionEpilogue(masm, fr.fixedAllocSize(), &offsets_);

#if defined(JS_ION_PERF)
  // FIXME - profiling code missing.  No bug for this.

  // Note the end of the inline code and start of the OOL code.
  // gen->perfSpewer().noteEndInlineCode(masm);
#endif

  JitSpew(JitSpew_Codegen, "# endFunction: end of function epilogue");
  JitSpew(JitSpew_Codegen, "# endFunction: start of OOL code");
  if (!generateOutOfLineCode()) {
    return false;
  }

  offsets_.end = masm.currentOffset();

  if (!fr.checkStackHeight()) {
    return decoder_.fail(decoder_.beginOffset(), "stack frame is too large");
  }

  JitSpew(JitSpew_Codegen, "# endFunction: end of OOL code for index %d",
          (int)func_.index);
  return !masm.oom();
}

void BaseCompiler::popStackReturnValues(const ResultType& resultType) {
  uint32_t bytes = ABIResultIter::MeasureStackBytes(resultType);
  if (bytes == 0) {
    return;
  }
  Register target = ABINonArgReturnReg0;
  Register temp = ABINonArgReturnReg1;
  fr.loadIncomingStackResultAreaPtr(RegPtr(target));
  fr.popStackResultsToMemory(target, bytes, temp);
}

void BaseCompiler::saveRegisterReturnValues(const ResultType& resultType) {
  MOZ_ASSERT(compilerEnv_.debugEnabled());
  size_t debugFrameOffset = masm.framePushed() - DebugFrame::offsetOfFrame();
  size_t registerResultIdx = 0;
  for (ABIResultIter i(resultType); !i.done(); i.next()) {
    const ABIResult result = i.cur();
    if (!result.inRegister()) {
#ifdef DEBUG
      for (i.next(); !i.done(); i.next()) {
        MOZ_ASSERT(!i.cur().inRegister());
      }
#endif
      break;
    }

    size_t resultOffset = DebugFrame::offsetOfRegisterResult(registerResultIdx);
    Address dest(masm.getStackPointer(), debugFrameOffset + resultOffset);
    switch (result.type().kind()) {
      case ValType::I32:
        masm.store32(RegI32(result.gpr()), dest);
        break;
      case ValType::I64:
        masm.store64(RegI64(result.gpr64()), dest);
        break;
      case ValType::F64:
        masm.storeDouble(RegF64(result.fpr()), dest);
        break;
      case ValType::F32:
        masm.storeFloat32(RegF32(result.fpr()), dest);
        break;
      case ValType::Rtt:
      case ValType::Ref: {
        uint32_t flag =
            DebugFrame::hasSpilledRegisterRefResultBitMask(registerResultIdx);
        // Tell Instance::traceFrame that we have a pointer to trace.
        masm.or32(Imm32(flag),
                  Address(masm.getStackPointer(),
                          debugFrameOffset + DebugFrame::offsetOfFlags()));
        masm.storePtr(RegRef(result.gpr()), dest);
        break;
      }
      case ValType::V128:
#ifdef ENABLE_WASM_SIMD
        masm.storeUnalignedSimd128(RegV128(result.fpr()), dest);
        break;
#else
        MOZ_CRASH("No SIMD support");
#endif
    }
    registerResultIdx++;
  }
}

void BaseCompiler::restoreRegisterReturnValues(const ResultType& resultType) {
  MOZ_ASSERT(compilerEnv_.debugEnabled());
  size_t debugFrameOffset = masm.framePushed() - DebugFrame::offsetOfFrame();
  size_t registerResultIdx = 0;
  for (ABIResultIter i(resultType); !i.done(); i.next()) {
    const ABIResult result = i.cur();
    if (!result.inRegister()) {
#ifdef DEBUG
      for (i.next(); !i.done(); i.next()) {
        MOZ_ASSERT(!i.cur().inRegister());
      }
#endif
      break;
    }
    size_t resultOffset =
        DebugFrame::offsetOfRegisterResult(registerResultIdx++);
    Address src(masm.getStackPointer(), debugFrameOffset + resultOffset);
    switch (result.type().kind()) {
      case ValType::I32:
        masm.load32(src, RegI32(result.gpr()));
        break;
      case ValType::I64:
        masm.load64(src, RegI64(result.gpr64()));
        break;
      case ValType::F64:
        masm.loadDouble(src, RegF64(result.fpr()));
        break;
      case ValType::F32:
        masm.loadFloat32(src, RegF32(result.fpr()));
        break;
      case ValType::Rtt:
      case ValType::Ref:
        masm.loadPtr(src, RegRef(result.gpr()));
        break;
      case ValType::V128:
#ifdef ENABLE_WASM_SIMD
        masm.loadUnalignedSimd128(src, RegV128(result.fpr()));
        break;
#else
        MOZ_CRASH("No SIMD support");
#endif
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
//
// Results and block parameters

// TODO / OPTIMIZE (Bug 1316818): At the moment we use the Wasm
// inter-procedure ABI for block returns, which allocates ReturnReg as the
// single block result register.  It is possible other choices would lead to
// better register allocation, as ReturnReg is often first in the register set
// and will be heavily wanted by the register allocator that uses takeFirst().
//
// Obvious options:
//  - pick a register at the back of the register set
//  - pick a random register per block (different blocks have
//    different join regs)

void BaseCompiler::popRegisterResults(ABIResultIter& iter) {
  // Pop register results.  Note that in the single-value case, popping to a
  // register may cause a sync(); for multi-value we sync'd already.
  for (; !iter.done(); iter.next()) {
    const ABIResult& result = iter.cur();
    if (!result.inRegister()) {
      // TODO / OPTIMIZE: We sync here to avoid solving the general parallel
      // move problem in popStackResults.  However we could avoid syncing the
      // values that are going to registers anyway, if they are already in
      // registers.
      sync();
      break;
    }
    switch (result.type().kind()) {
      case ValType::I32:
        popI32(RegI32(result.gpr()));
        break;
      case ValType::I64:
        popI64(RegI64(result.gpr64()));
        break;
      case ValType::F32:
        popF32(RegF32(result.fpr()));
        break;
      case ValType::F64:
        popF64(RegF64(result.fpr()));
        break;
      case ValType::Rtt:
      case ValType::Ref:
        popRef(RegRef(result.gpr()));
        break;
      case ValType::V128:
#ifdef ENABLE_WASM_SIMD
        popV128(RegV128(result.fpr()));
#else
        MOZ_CRASH("No SIMD support");
#endif
    }
  }
}

void BaseCompiler::popStackResults(ABIResultIter& iter, StackHeight stackBase) {
  MOZ_ASSERT(!iter.done());

  // The iterator should be advanced beyond register results, and register
  // results should be popped already from the value stack.
  uint32_t alreadyPopped = iter.index();

  // At this point, only stack arguments are remaining.  Iterate through them
  // to measure how much stack space they will take up.
  for (; !iter.done(); iter.next()) {
    MOZ_ASSERT(iter.cur().onStack());
  }

  // Calculate the space needed to store stack results, in bytes.
  uint32_t stackResultBytes = iter.stackBytesConsumedSoFar();
  MOZ_ASSERT(stackResultBytes);

  // Compute the stack height including the stack results.  Note that it's
  // possible that this call expands the stack, for example if some of the
  // results are supplied by constants and so are not already on the machine
  // stack.
  uint32_t endHeight = fr.prepareStackResultArea(stackBase, stackResultBytes);

  // Find a free GPR to use when shuffling stack values.  If none is
  // available, push ReturnReg and restore it after we're done.
  bool saved = false;
  RegPtr temp = ra.needTempPtr(RegPtr(ReturnReg), &saved);

  // The sequence of Stk values is in the same order on the machine stack as
  // the result locations, but there is a complication: constant values are
  // not actually pushed on the machine stack.  (At this point registers and
  // locals have been spilled already.)  So, moving the Stk values into place
  // isn't simply a shuffle-down or shuffle-up operation.  There is a part of
  // the Stk sequence that shuffles toward the FP, a part that's already in
  // place, and a part that shuffles toward the SP.  After shuffling, we have
  // to materialize the constants.

  // Shuffle mem values toward the frame pointer, copying deepest values
  // first.  Stop when we run out of results, get to a register result, or
  // find a Stk value that is closer to the FP than the result.
  for (iter.switchToPrev(); !iter.done(); iter.prev()) {
    const ABIResult& result = iter.cur();
    if (!result.onStack()) {
      break;
    }
    MOZ_ASSERT(result.stackOffset() < stackResultBytes);
    uint32_t destHeight = endHeight - result.stackOffset();
    uint32_t stkBase = stk_.length() - (iter.count() - alreadyPopped);
    Stk& v = stk_[stkBase + iter.index()];
    if (v.isMem()) {
      uint32_t srcHeight = v.offs();
      if (srcHeight <= destHeight) {
        break;
      }
      fr.shuffleStackResultsTowardFP(srcHeight, destHeight, result.size(),
                                     temp);
    }
  }

  // Reset iterator and skip register results.
  for (iter.reset(); !iter.done(); iter.next()) {
    if (iter.cur().onStack()) {
      break;
    }
  }

  // Revisit top stack values, shuffling mem values toward the stack pointer,
  // copying shallowest values first.
  for (; !iter.done(); iter.next()) {
    const ABIResult& result = iter.cur();
    MOZ_ASSERT(result.onStack());
    MOZ_ASSERT(result.stackOffset() < stackResultBytes);
    uint32_t destHeight = endHeight - result.stackOffset();
    Stk& v = stk_[stk_.length() - (iter.index() - alreadyPopped) - 1];
    if (v.isMem()) {
      uint32_t srcHeight = v.offs();
      if (srcHeight >= destHeight) {
        break;
      }
      fr.shuffleStackResultsTowardSP(srcHeight, destHeight, result.size(),
                                     temp);
    }
  }

  // Reset iterator and skip register results, which are already popped off
  // the value stack.
  for (iter.reset(); !iter.done(); iter.next()) {
    if (iter.cur().onStack()) {
      break;
    }
  }

  // Materialize constants and pop the remaining items from the value stack.
  for (; !iter.done(); iter.next()) {
    const ABIResult& result = iter.cur();
    uint32_t resultHeight = endHeight - result.stackOffset();
    Stk& v = stk_.back();
    switch (v.kind()) {
      case Stk::ConstI32:
#if defined(JS_CODEGEN_MIPS64)
        fr.storeImmediatePtrToStack(v.i32val_, resultHeight, temp);
#else
        fr.storeImmediatePtrToStack(uint32_t(v.i32val_), resultHeight, temp);
#endif
        break;
      case Stk::ConstF32:
        fr.storeImmediateF32ToStack(v.f32val_, resultHeight, temp);
        break;
      case Stk::ConstI64:
        fr.storeImmediateI64ToStack(v.i64val_, resultHeight, temp);
        break;
      case Stk::ConstF64:
        fr.storeImmediateF64ToStack(v.f64val_, resultHeight, temp);
        break;
#ifdef ENABLE_WASM_SIMD
      case Stk::ConstV128:
        fr.storeImmediateV128ToStack(v.v128val_, resultHeight, temp);
        break;
#endif
      case Stk::ConstRef:
        fr.storeImmediatePtrToStack(v.refval_, resultHeight, temp);
        break;
      case Stk::MemRef:
        // Update bookkeeping as we pop the Stk entry.
        stackMapGenerator_.memRefsOnStk--;
        break;
      default:
        MOZ_ASSERT(v.isMem());
        break;
    }
    stk_.popBack();
  }

  ra.freeTempPtr(temp, saved);

  // This will pop the stack if needed.
  fr.finishStackResultArea(stackBase, stackResultBytes);
}

void BaseCompiler::popBlockResults(ResultType type, StackHeight stackBase,
                                   ContinuationKind kind) {
  if (!type.empty()) {
    ABIResultIter iter(type);
    popRegisterResults(iter);
    if (!iter.done()) {
      popStackResults(iter, stackBase);
      // Because popStackResults might clobber the stack, it leaves the stack
      // pointer already in the right place for the continuation, whether the
      // continuation is a jump or fallthrough.
      return;
    }
  }
  // We get here if there are no stack results.  For a fallthrough, the stack
  // is already at the right height.  For a jump, we may need to pop the stack
  // pointer if the continuation's stack height is lower than the current
  // stack height.
  if (kind == ContinuationKind::Jump) {
    fr.popStackBeforeBranch(stackBase, type);
  }
}

#ifdef ENABLE_WASM_EXCEPTIONS
// This function is similar to popBlockResults, but additionally handles the
// implicit exception pointer that is pushed to the value stack on entry to
// a catch handler by dropping it appropriately.
void BaseCompiler::popCatchResults(ResultType type, StackHeight stackBase) {
  if (!type.empty()) {
    ABIResultIter iter(type);
    popRegisterResults(iter);
    if (!iter.done()) {
      popStackResults(iter, stackBase);
      // Since popStackResults clobbers the stack, we only need to free the
      // exception off of the value stack.
      popValueStackBy(1);
    } else {
      // If there are no stack results, we have to adjust the stack by
      // dropping the exception reference that's now on the stack.
      dropValue();
    }
  } else {
    dropValue();
  }
  fr.popStackBeforeBranch(stackBase, type);
}
#endif

Stk BaseCompiler::captureStackResult(const ABIResult& result,
                                     StackHeight resultsBase,
                                     uint32_t stackResultBytes) {
  MOZ_ASSERT(result.onStack());
  uint32_t offs = fr.locateStackResult(result, resultsBase, stackResultBytes);
  return Stk::StackResult(result.type(), offs);
}

// TODO: It may be fruitful to inline the fast path here, as it will be common.

bool BaseCompiler::pushResults(ResultType type, StackHeight resultsBase) {
  if (type.empty()) {
    return true;
  }

  if (type.length() > 1) {
    // Reserve extra space on the stack for all the values we'll push.
    // Multi-value push is not accounted for by the pre-sizing of the stack in
    // the decoding loop.
    //
    // Also make sure we leave headroom for other pushes that will occur after
    // pushing results, just to be safe.
    if (!stk_.reserve(stk_.length() + type.length() + MaxPushesPerOpcode)) {
      return false;
    }
  }

  // We need to push the results in reverse order, so first iterate through
  // all results to determine the locations of stack result types.
  ABIResultIter iter(type);
  while (!iter.done()) {
    iter.next();
  }
  uint32_t stackResultBytes = iter.stackBytesConsumedSoFar();
  for (iter.switchToPrev(); !iter.done(); iter.prev()) {
    const ABIResult& result = iter.cur();
    if (!result.onStack()) {
      break;
    }
    Stk v = captureStackResult(result, resultsBase, stackResultBytes);
    push(v);
    if (v.kind() == Stk::MemRef) {
      stackMapGenerator_.memRefsOnStk++;
    }
  }

  for (; !iter.done(); iter.prev()) {
    const ABIResult& result = iter.cur();
    MOZ_ASSERT(result.inRegister());
    switch (result.type().kind()) {
      case ValType::I32:
        pushI32(RegI32(result.gpr()));
        break;
      case ValType::I64:
        pushI64(RegI64(result.gpr64()));
        break;
      case ValType::V128:
#ifdef ENABLE_WASM_SIMD
        pushV128(RegV128(result.fpr()));
        break;
#else
        MOZ_CRASH("No SIMD support");
#endif
      case ValType::F32:
        pushF32(RegF32(result.fpr()));
        break;
      case ValType::F64:
        pushF64(RegF64(result.fpr()));
        break;
      case ValType::Rtt:
      case ValType::Ref:
        pushRef(RegRef(result.gpr()));
        break;
    }
  }

  return true;
}

bool BaseCompiler::pushBlockResults(ResultType type) {
  return pushResults(type, controlItem().stackHeight);
}

// A combination of popBlockResults + pushBlockResults, used when entering a
// block with a control-flow join (loops) or split (if) to shuffle the
// fallthrough block parameters into the locations expected by the
// continuation.
bool BaseCompiler::topBlockParams(ResultType type) {
  // This function should only be called when entering a block with a
  // control-flow join at the entry, where there are no live temporaries in
  // the current block.
  StackHeight base = controlItem().stackHeight;
  MOZ_ASSERT(fr.stackResultsBase(stackConsumed(type.length())) == base);
  popBlockResults(type, base, ContinuationKind::Fallthrough);
  return pushBlockResults(type);
}

// A combination of popBlockResults + pushBlockResults, used before branches
// where we don't know the target (br_if / br_table).  If and when the branch
// is taken, the stack results will be shuffled down into place.  For br_if
// that has fallthrough, the parameters for the untaken branch flow through to
// the continuation.
bool BaseCompiler::topBranchParams(ResultType type, StackHeight* height) {
  if (type.empty()) {
    *height = fr.stackHeight();
    return true;
  }
  // There may be temporary values that need spilling; delay computation of
  // the stack results base until after the popRegisterResults(), which spills
  // if needed.
  ABIResultIter iter(type);
  popRegisterResults(iter);
  StackHeight base = fr.stackResultsBase(stackConsumed(iter.remaining()));
  if (!iter.done()) {
    popStackResults(iter, base);
  }
  if (!pushResults(type, base)) {
    return false;
  }
  *height = base;
  return true;
}

// Conditional branches with fallthrough are preceded by a topBranchParams, so
// we know that there are no stack results that need to be materialized.  In
// that case, we can just shuffle the whole block down before popping the
// stack.
void BaseCompiler::shuffleStackResultsBeforeBranch(StackHeight srcHeight,
                                                   StackHeight destHeight,
                                                   ResultType type) {
  uint32_t stackResultBytes = 0;

  if (ABIResultIter::HasStackResults(type)) {
    MOZ_ASSERT(stk_.length() >= type.length());
    ABIResultIter iter(type);
    for (; !iter.done(); iter.next()) {
#ifdef DEBUG
      const ABIResult& result = iter.cur();
      const Stk& v = stk_[stk_.length() - iter.index() - 1];
      MOZ_ASSERT(v.isMem() == result.onStack());
#endif
    }

    stackResultBytes = iter.stackBytesConsumedSoFar();
    MOZ_ASSERT(stackResultBytes > 0);

    if (srcHeight != destHeight) {
      // Find a free GPR to use when shuffling stack values.  If none
      // is available, push ReturnReg and restore it after we're done.
      bool saved = false;
      RegPtr temp = ra.needTempPtr(RegPtr(ReturnReg), &saved);
      fr.shuffleStackResultsTowardFP(srcHeight, destHeight, stackResultBytes,
                                     temp);
      ra.freeTempPtr(temp, saved);
    }
  }

  fr.popStackBeforeBranch(destHeight, stackResultBytes);
}

//////////////////////////////////////////////////////////////////////////////
//
// Function calls.

void BaseCompiler::beginCall(FunctionCall& call, UseABI useABI,
                             InterModule interModule) {
  MOZ_ASSERT_IF(useABI == UseABI::Builtin, interModule == InterModule::False);

  call.isInterModule = interModule == InterModule::True;
  call.usesSystemAbi = useABI == UseABI::System;

  if (call.usesSystemAbi) {
    // Call-outs need to use the appropriate system ABI.
#if defined(JS_CODEGEN_ARM)
    call.hardFP = UseHardFpABI();
    call.abi.setUseHardFp(call.hardFP);
#endif
  } else {
#if defined(JS_CODEGEN_ARM)
    MOZ_ASSERT(call.hardFP, "All private ABIs pass FP arguments in registers");
#endif
  }

  // Use masm.framePushed() because the value we want here does not depend
  // on the height of the frame's stack area, but the actual size of the
  // allocated frame.
  call.frameAlignAdjustment = ComputeByteAlignment(
      masm.framePushed() + sizeof(Frame), JitStackAlignment);
}

void BaseCompiler::endCall(FunctionCall& call, size_t stackSpace) {
  size_t adjustment = call.stackArgAreaSize + call.frameAlignAdjustment;
  fr.freeArgAreaAndPopBytes(adjustment, stackSpace);

  MOZ_ASSERT(stackMapGenerator_.framePushedExcludingOutboundCallArgs.isSome());
  stackMapGenerator_.framePushedExcludingOutboundCallArgs.reset();

  if (call.isInterModule) {
    fr.loadTlsPtr(WasmTlsReg);
    masm.loadWasmPinnedRegsFromTls();
    masm.switchToWasmTlsRealm(ABINonArgReturnReg0, ABINonArgReturnReg1);
  } else if (call.usesSystemAbi) {
    // On x86 there are no pinned registers, so don't waste time
    // reloading the Tls.
#ifndef JS_CODEGEN_X86
    fr.loadTlsPtr(WasmTlsReg);
    masm.loadWasmPinnedRegsFromTls();
#endif
  }
}

void BaseCompiler::startCallArgs(size_t stackArgAreaSizeUnaligned,
                                 FunctionCall* call) {
  size_t stackArgAreaSizeAligned =
      AlignStackArgAreaSize(stackArgAreaSizeUnaligned);
  MOZ_ASSERT(stackArgAreaSizeUnaligned <= stackArgAreaSizeAligned);

  // Record the masm.framePushed() value at this point, before we push args
  // for the call, but including the alignment space placed above the args.
  // This defines the lower limit of the stackmap that will be created for
  // this call.
  MOZ_ASSERT(
      stackMapGenerator_.framePushedExcludingOutboundCallArgs.isNothing());
  stackMapGenerator_.framePushedExcludingOutboundCallArgs.emplace(
      // However much we've pushed so far
      masm.framePushed() +
      // Extra space we'll push to get the frame aligned
      call->frameAlignAdjustment +
      // Extra space we'll push to get the outbound arg area 16-aligned
      (stackArgAreaSizeAligned - stackArgAreaSizeUnaligned));

  call->stackArgAreaSize = stackArgAreaSizeAligned;

  size_t adjustment = call->stackArgAreaSize + call->frameAlignAdjustment;
  fr.allocArgArea(adjustment);
}

ABIArg BaseCompiler::reservePointerArgument(FunctionCall* call) {
  return call->abi.next(MIRType::Pointer);
}

// TODO / OPTIMIZE (Bug 1316821): Note passArg is used only in one place.
// (Or it was, until Luke wandered through, but that can be fixed again.)
// I'm not saying we should manually inline it, but we could hoist the
// dispatch into the caller and have type-specific implementations of
// passArg: passArgI32(), etc.  Then those might be inlined, at least in PGO
// builds.
//
// The bulk of the work here (60%) is in the next() call, though.
//
// Notably, since next() is so expensive, StackArgAreaSizeUnaligned()
// becomes expensive too.
//
// Somehow there could be a trick here where the sequence of argument types
// (read from the input stream) leads to a cached entry for
// StackArgAreaSizeUnaligned() and for how to pass arguments...
//
// But at least we could reduce the cost of StackArgAreaSizeUnaligned() by
// first reading the argument types into a (reusable) vector, then we have
// the outgoing size at low cost, and then we can pass args based on the
// info we read.

void BaseCompiler::passArg(ValType type, const Stk& arg, FunctionCall* call) {
  switch (type.kind()) {
    case ValType::I32: {
      ABIArg argLoc = call->abi.next(MIRType::Int32);
      if (argLoc.kind() == ABIArg::Stack) {
        ScratchI32 scratch(*this);
        loadI32(arg, scratch);
        masm.store32(scratch, Address(masm.getStackPointer(),
                                      argLoc.offsetFromArgBase()));
      } else {
        loadI32(arg, RegI32(argLoc.gpr()));
      }
      break;
    }
    case ValType::I64: {
      ABIArg argLoc = call->abi.next(MIRType::Int64);
      if (argLoc.kind() == ABIArg::Stack) {
        ScratchI32 scratch(*this);
#ifdef JS_PUNBOX64
        loadI64(arg, fromI32(scratch));
        masm.storePtr(scratch, Address(masm.getStackPointer(),
                                       argLoc.offsetFromArgBase()));
#else
        loadI64Low(arg, scratch);
        masm.store32(scratch, LowWord(Address(masm.getStackPointer(),
                                              argLoc.offsetFromArgBase())));
        loadI64High(arg, scratch);
        masm.store32(scratch, HighWord(Address(masm.getStackPointer(),
                                               argLoc.offsetFromArgBase())));
#endif
      } else {
        loadI64(arg, RegI64(argLoc.gpr64()));
      }
      break;
    }
    case ValType::V128: {
#ifdef ENABLE_WASM_SIMD
      ABIArg argLoc = call->abi.next(MIRType::Simd128);
      switch (argLoc.kind()) {
        case ABIArg::Stack: {
          ScratchV128 scratch(*this);
          loadV128(arg, scratch);
          masm.storeUnalignedSimd128(
              (RegV128)scratch,
              Address(masm.getStackPointer(), argLoc.offsetFromArgBase()));
          break;
        }
        case ABIArg::GPR: {
          MOZ_CRASH("Unexpected parameter passing discipline");
        }
        case ABIArg::FPU: {
          loadV128(arg, RegV128(argLoc.fpu()));
          break;
        }
#  if defined(JS_CODEGEN_REGISTER_PAIR)
        case ABIArg::GPR_PAIR: {
          MOZ_CRASH("Unexpected parameter passing discipline");
        }
#  endif
        case ABIArg::Uninitialized:
          MOZ_CRASH("Uninitialized ABIArg kind");
      }
      break;
#else
      MOZ_CRASH("No SIMD support");
#endif
    }
    case ValType::F64: {
      ABIArg argLoc = call->abi.next(MIRType::Double);
      switch (argLoc.kind()) {
        case ABIArg::Stack: {
          ScratchF64 scratch(*this);
          loadF64(arg, scratch);
          masm.storeDouble(scratch, Address(masm.getStackPointer(),
                                            argLoc.offsetFromArgBase()));
          break;
        }
#if defined(JS_CODEGEN_REGISTER_PAIR)
        case ABIArg::GPR_PAIR: {
#  if defined(JS_CODEGEN_ARM)
          ScratchF64 scratch(*this);
          loadF64(arg, scratch);
          masm.ma_vxfer(scratch, argLoc.evenGpr(), argLoc.oddGpr());
          break;
#  else
          MOZ_CRASH("BaseCompiler platform hook: passArg F64 pair");
#  endif
        }
#endif
        case ABIArg::FPU: {
          loadF64(arg, RegF64(argLoc.fpu()));
          break;
        }
        case ABIArg::GPR: {
          MOZ_CRASH("Unexpected parameter passing discipline");
        }
        case ABIArg::Uninitialized:
          MOZ_CRASH("Uninitialized ABIArg kind");
      }
      break;
    }
    case ValType::F32: {
      ABIArg argLoc = call->abi.next(MIRType::Float32);
      switch (argLoc.kind()) {
        case ABIArg::Stack: {
          ScratchF32 scratch(*this);
          loadF32(arg, scratch);
          masm.storeFloat32(scratch, Address(masm.getStackPointer(),
                                             argLoc.offsetFromArgBase()));
          break;
        }
        case ABIArg::GPR: {
          ScratchF32 scratch(*this);
          loadF32(arg, scratch);
          masm.moveFloat32ToGPR(scratch, argLoc.gpr());
          break;
        }
        case ABIArg::FPU: {
          loadF32(arg, RegF32(argLoc.fpu()));
          break;
        }
#if defined(JS_CODEGEN_REGISTER_PAIR)
        case ABIArg::GPR_PAIR: {
          MOZ_CRASH("Unexpected parameter passing discipline");
        }
#endif
        case ABIArg::Uninitialized:
          MOZ_CRASH("Uninitialized ABIArg kind");
      }
      break;
    }
    case ValType::Rtt:
    case ValType::Ref: {
      ABIArg argLoc = call->abi.next(MIRType::RefOrNull);
      if (argLoc.kind() == ABIArg::Stack) {
        ScratchRef scratch(*this);
        loadRef(arg, scratch);
        masm.storePtr(scratch, Address(masm.getStackPointer(),
                                       argLoc.offsetFromArgBase()));
      } else {
        loadRef(arg, RegRef(argLoc.gpr()));
      }
      break;
    }
  }
}

CodeOffset BaseCompiler::callDefinition(uint32_t funcIndex,
                                        const FunctionCall& call) {
  CallSiteDesc desc(call.lineOrBytecode, CallSiteDesc::Func);
  return masm.call(desc, funcIndex);
}

CodeOffset BaseCompiler::callSymbolic(SymbolicAddress callee,
                                      const FunctionCall& call) {
  CallSiteDesc desc(call.lineOrBytecode, CallSiteDesc::Symbolic);
  return masm.call(desc, callee);
}

// Precondition: sync()

CodeOffset BaseCompiler::callIndirect(uint32_t funcTypeIndex,
                                      uint32_t tableIndex, const Stk& indexVal,
                                      const FunctionCall& call) {
  const TypeIdDesc& funcTypeId = moduleEnv_.typeIds[funcTypeIndex];
  MOZ_ASSERT(funcTypeId.kind() != TypeIdDescKind::None);

  const TableDesc& table = moduleEnv_.tables[tableIndex];

  loadI32(indexVal, RegI32(WasmTableCallIndexReg));

  CallSiteDesc desc(call.lineOrBytecode, CallSiteDesc::Indirect);
  CalleeDesc callee = CalleeDesc::wasmTable(table, funcTypeId);
  return masm.wasmCallIndirect(desc, callee, NeedsBoundsCheck(true));
}

// Precondition: sync()

CodeOffset BaseCompiler::callImport(unsigned globalDataOffset,
                                    const FunctionCall& call) {
  CallSiteDesc desc(call.lineOrBytecode, CallSiteDesc::Import);
  CalleeDesc callee = CalleeDesc::import(globalDataOffset);
  return masm.wasmCallImport(desc, callee);
}

CodeOffset BaseCompiler::builtinCall(SymbolicAddress builtin,
                                     const FunctionCall& call) {
  return callSymbolic(builtin, call);
}

CodeOffset BaseCompiler::builtinInstanceMethodCall(
    const SymbolicAddressSignature& builtin, const ABIArg& instanceArg,
    const FunctionCall& call) {
  // Builtin method calls assume the TLS register has been set.
  fr.loadTlsPtr(WasmTlsReg);

  CallSiteDesc desc(call.lineOrBytecode, CallSiteDesc::Symbolic);
  return masm.wasmCallBuiltinInstanceMethod(desc, instanceArg, builtin.identity,
                                            builtin.failureMode);
}

bool BaseCompiler::pushCallResults(const FunctionCall& call, ResultType type,
                                   const StackResultsLoc& loc) {
#if defined(JS_CODEGEN_ARM)
  // pushResults currently bypasses special case code in captureReturnedFxx()
  // that converts GPR results to FPR results for systemABI+softFP.  If we
  // ever start using that combination for calls we need more code.  This
  // assert is stronger than we need - we only care about results in return
  // registers - but that's OK.
  MOZ_ASSERT(!call.usesSystemAbi || call.hardFP);
#endif
  return pushResults(type, fr.stackResultsBase(loc.bytes()));
}

//////////////////////////////////////////////////////////////////////////////
//
// Exception handling
#ifdef ENABLE_WASM_EXCEPTIONS
// Abstracted helper for throwing, used for throw, rethrow, and rethrowing
// at the end of a series of catch blocks (if none matched the exception).
bool BaseCompiler::throwFrom(RegRef exn, uint32_t lineOrBytecode) {
  pushRef(exn);

  // ThrowException invokes a trap, and the rest is dead code.
  if (!emitInstanceCall(lineOrBytecode, SASigThrowException)) {
    return false;
  }
  freeRef(popRef());

  return true;
}

void BaseCompiler::loadPendingException(Register dest) {
  masm.loadPtr(Address(WasmTlsReg, offsetof(TlsData, pendingException)), dest);
}
#endif

////////////////////////////////////////////////////////////
//
// Platform-specific popping and register targeting.

// The simple popping methods pop values into targeted registers; the caller
// can free registers using standard functions.  These are always called
// popXForY where X says something about types and Y something about the
// operation being targeted.

RegI32 BaseCompiler::needRotate64Temp() {
#if defined(JS_CODEGEN_X86)
  return needI32();
#elif defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_ARM) || \
    defined(JS_CODEGEN_ARM64) || defined(JS_CODEGEN_MIPS64)
  return RegI32::Invalid();
#else
  MOZ_CRASH("BaseCompiler platform hook: needRotate64Temp");
#endif
}

void BaseCompiler::popAndAllocateForDivAndRemI32(RegI32* r0, RegI32* r1,
                                                 RegI32* reserved) {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
  // r0 must be eax, and edx will be clobbered.
  need2xI32(specific_.eax, specific_.edx);
  *r1 = popI32();
  *r0 = popI32ToSpecific(specific_.eax);
  *reserved = specific_.edx;
#else
  pop2xI32(r0, r1);
#endif
}

static void QuotientI32(MacroAssembler& masm, RegI32 rs, RegI32 rsd,
                        RegI32 reserved, IsUnsigned isUnsigned) {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
  masm.quotient32(rs, rsd, reserved, isUnsigned);
#else
  masm.quotient32(rs, rsd, isUnsigned);
#endif
}

static void RemainderI32(MacroAssembler& masm, RegI32 rs, RegI32 rsd,
                         RegI32 reserved, IsUnsigned isUnsigned) {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
  masm.remainder32(rs, rsd, reserved, isUnsigned);
#else
  masm.remainder32(rs, rsd, isUnsigned);
#endif
}

void BaseCompiler::popAndAllocateForMulI64(RegI64* r0, RegI64* r1,
                                           RegI32* temp) {
#if defined(JS_CODEGEN_X64)
  pop2xI64(r0, r1);
#elif defined(JS_CODEGEN_X86)
  // lhsDest must be edx:eax and rhs must not be that.
  needI64(specific_.edx_eax);
  *r1 = popI64();
  *r0 = popI64ToSpecific(specific_.edx_eax);
  *temp = needI32();
#elif defined(JS_CODEGEN_MIPS64)
  pop2xI64(r0, r1);
#elif defined(JS_CODEGEN_ARM)
  pop2xI64(r0, r1);
  *temp = needI32();
#elif defined(JS_CODEGEN_ARM64)
  pop2xI64(r0, r1);
#else
  MOZ_CRASH("BaseCompiler porting interface: popAndAllocateForMulI64");
#endif
}

#ifndef RABALDR_INT_DIV_I64_CALLOUT

void BaseCompiler::popAndAllocateForDivAndRemI64(RegI64* r0, RegI64* r1,
                                                 RegI64* reserved,
                                                 IsRemainder isRemainder) {
#  if defined(JS_CODEGEN_X64)
  // r0 must be rax, and rdx will be clobbered.
  need2xI64(specific_.rax, specific_.rdx);
  *r1 = popI64();
  *r0 = popI64ToSpecific(specific_.rax);
  *reserved = specific_.rdx;
#  elif defined(JS_CODEGEN_ARM64)
  pop2xI64(r0, r1);
  if (isRemainder) {
    *reserved = needI64();
  }
#  else
  pop2xI64(r0, r1);
#  endif
}

static void QuotientI64(MacroAssembler& masm, RegI64 rhs, RegI64 srcDest,
                        RegI64 reserved, IsUnsigned isUnsigned) {
#  if defined(JS_CODEGEN_X64)
  // The caller must set up the following situation.
  MOZ_ASSERT(srcDest.reg == rax);
  MOZ_ASSERT(reserved.reg == rdx);
  if (isUnsigned) {
    masm.xorq(rdx, rdx);
    masm.udivq(rhs.reg);
  } else {
    masm.cqo();
    masm.idivq(rhs.reg);
  }
#  elif defined(JS_CODEGEN_MIPS64)
  MOZ_ASSERT(reserved.isInvalid());
  if (isUnsigned) {
    masm.as_ddivu(srcDest.reg, rhs.reg);
  } else {
    masm.as_ddiv(srcDest.reg, rhs.reg);
  }
  masm.as_mflo(srcDest.reg);
#  elif defined(JS_CODEGEN_ARM64)
  MOZ_ASSERT(reserved.isInvalid());
  ARMRegister sd(srcDest.reg, 64);
  ARMRegister r(rhs.reg, 64);
  if (isUnsigned) {
    masm.Udiv(sd, sd, r);
  } else {
    masm.Sdiv(sd, sd, r);
  }
#  else
  MOZ_CRASH("BaseCompiler platform hook: quotientI64");
#  endif
}

static void RemainderI64(MacroAssembler& masm, RegI64 rhs, RegI64 srcDest,
                         RegI64 reserved, IsUnsigned isUnsigned) {
#  if defined(JS_CODEGEN_X64)
  // The caller must set up the following situation.
  MOZ_ASSERT(srcDest.reg == rax);
  MOZ_ASSERT(reserved.reg == rdx);

  if (isUnsigned) {
    masm.xorq(rdx, rdx);
    masm.udivq(rhs.reg);
  } else {
    masm.cqo();
    masm.idivq(rhs.reg);
  }
  masm.movq(rdx, rax);
#  elif defined(JS_CODEGEN_MIPS64)
  MOZ_ASSERT(reserved.isInvalid());
  if (isUnsigned) {
    masm.as_ddivu(srcDest.reg, rhs.reg);
  } else {
    masm.as_ddiv(srcDest.reg, rhs.reg);
  }
  masm.as_mfhi(srcDest.reg);
#  elif defined(JS_CODEGEN_ARM64)
  ARMRegister sd(srcDest.reg, 64);
  ARMRegister r(rhs.reg, 64);
  ARMRegister t(reserved.reg, 64);
  if (isUnsigned) {
    masm.Udiv(t, sd, r);
  } else {
    masm.Sdiv(t, sd, r);
  }
  masm.Mul(t, t, r);
  masm.Sub(sd, sd, t);
#  else
  MOZ_CRASH("BaseCompiler platform hook: remainderI64");
#  endif
}

#endif  // RABALDR_INT_DIV_I64_CALLOUT

RegI32 BaseCompiler::popI32RhsForShift() {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
  // r1 must be ecx for a variable shift, unless BMI2 is available.
  if (!Assembler::HasBMI2()) {
    return popI32(specific_.ecx);
  }
#endif
  RegI32 r = popI32();
#if defined(JS_CODEGEN_ARM)
  masm.and32(Imm32(31), r);
#endif
  return r;
}

RegI32 BaseCompiler::popI32RhsForShiftI64() {
#if defined(JS_CODEGEN_X86)
  // A limitation in the x86 masm requires ecx here
  return popI32(specific_.ecx);
#elif defined(JS_CODEGEN_X64)
  if (!Assembler::HasBMI2()) {
    return popI32(specific_.ecx);
  }
  return popI32();
#else
  return popI32();
#endif
}

RegI64 BaseCompiler::popI64RhsForShift() {
#if defined(JS_CODEGEN_X86)
  // r1 must be ecx for a variable shift.
  needI32(specific_.ecx);
  return popI64ToSpecific(widenI32(specific_.ecx));
#else
#  if defined(JS_CODEGEN_X64)
  // r1 must be rcx for a variable shift, unless BMI2 is available.
  if (!Assembler::HasBMI2()) {
    needI64(specific_.rcx);
    return popI64ToSpecific(specific_.rcx);
  }
#  endif
  // No masking is necessary on 64-bit platforms, and on arm32 the masm
  // implementation masks.
  return popI64();
#endif
}

RegI32 BaseCompiler::popI32RhsForRotate() {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
  // r1 must be ecx for a variable rotate.
  return popI32(specific_.ecx);
#else
  return popI32();
#endif
}

RegI64 BaseCompiler::popI64RhsForRotate() {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
  // r1 must be ecx for a variable rotate.
  needI32(specific_.ecx);
  return popI64ToSpecific(widenI32(specific_.ecx));
#else
  return popI64();
#endif
}

void BaseCompiler::popI32ForSignExtendI64(RegI64* r0) {
#if defined(JS_CODEGEN_X86)
  // r0 must be edx:eax for cdq
  need2xI32(specific_.edx, specific_.eax);
  *r0 = specific_.edx_eax;
  popI32ToSpecific(specific_.eax);
#else
  *r0 = widenI32(popI32());
#endif
}

void BaseCompiler::popI64ForSignExtendI64(RegI64* r0) {
#if defined(JS_CODEGEN_X86)
  // r0 must be edx:eax for cdq
  need2xI32(specific_.edx, specific_.eax);
  // Low on top, high underneath
  *r0 = popI64ToSpecific(specific_.edx_eax);
#else
  *r0 = popI64();
#endif
}

class OutOfLineTruncateCheckF32OrF64ToI32 : public OutOfLineCode {
  AnyReg src;
  RegI32 dest;
  TruncFlags flags;
  BytecodeOffset off;

 public:
  OutOfLineTruncateCheckF32OrF64ToI32(AnyReg src, RegI32 dest, TruncFlags flags,
                                      BytecodeOffset off)
      : src(src), dest(dest), flags(flags), off(off) {}

  virtual void generate(MacroAssembler* masm) override {
    if (src.tag == AnyReg::F32) {
      masm->oolWasmTruncateCheckF32ToI32(src.f32(), dest, flags, off, rejoin());
    } else if (src.tag == AnyReg::F64) {
      masm->oolWasmTruncateCheckF64ToI32(src.f64(), dest, flags, off, rejoin());
    } else {
      MOZ_CRASH("unexpected type");
    }
  }
};

bool BaseCompiler::truncateF32ToI32(RegF32 src, RegI32 dest, TruncFlags flags) {
  BytecodeOffset off = bytecodeOffset();
  OutOfLineCode* ool =
      addOutOfLineCode(new (alloc_) OutOfLineTruncateCheckF32OrF64ToI32(
          AnyReg(src), dest, flags, off));
  if (!ool) {
    return false;
  }
  bool isSaturating = flags & TRUNC_SATURATING;
  if (flags & TRUNC_UNSIGNED) {
    masm.wasmTruncateFloat32ToUInt32(src, dest, isSaturating, ool->entry());
  } else {
    masm.wasmTruncateFloat32ToInt32(src, dest, isSaturating, ool->entry());
  }
  masm.bind(ool->rejoin());
  return true;
}

bool BaseCompiler::truncateF64ToI32(RegF64 src, RegI32 dest, TruncFlags flags) {
  BytecodeOffset off = bytecodeOffset();
  OutOfLineCode* ool =
      addOutOfLineCode(new (alloc_) OutOfLineTruncateCheckF32OrF64ToI32(
          AnyReg(src), dest, flags, off));
  if (!ool) {
    return false;
  }
  bool isSaturating = flags & TRUNC_SATURATING;
  if (flags & TRUNC_UNSIGNED) {
    masm.wasmTruncateDoubleToUInt32(src, dest, isSaturating, ool->entry());
  } else {
    masm.wasmTruncateDoubleToInt32(src, dest, isSaturating, ool->entry());
  }
  masm.bind(ool->rejoin());
  return true;
}

class OutOfLineTruncateCheckF32OrF64ToI64 : public OutOfLineCode {
  AnyReg src;
  RegI64 dest;
  TruncFlags flags;
  BytecodeOffset off;

 public:
  OutOfLineTruncateCheckF32OrF64ToI64(AnyReg src, RegI64 dest, TruncFlags flags,
                                      BytecodeOffset off)
      : src(src), dest(dest), flags(flags), off(off) {}

  virtual void generate(MacroAssembler* masm) override {
    if (src.tag == AnyReg::F32) {
      masm->oolWasmTruncateCheckF32ToI64(src.f32(), dest, flags, off, rejoin());
    } else if (src.tag == AnyReg::F64) {
      masm->oolWasmTruncateCheckF64ToI64(src.f64(), dest, flags, off, rejoin());
    } else {
      MOZ_CRASH("unexpected type");
    }
  }
};

#ifndef RABALDR_FLOAT_TO_I64_CALLOUT

RegF64 BaseCompiler::needTempForFloatingToI64(TruncFlags flags) {
#  if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
  if (flags & TRUNC_UNSIGNED) {
    return needF64();
  }
#  endif
  return RegF64::Invalid();
}

bool BaseCompiler::truncateF32ToI64(RegF32 src, RegI64 dest, TruncFlags flags,
                                    RegF64 temp) {
  OutOfLineCode* ool =
      addOutOfLineCode(new (alloc_) OutOfLineTruncateCheckF32OrF64ToI64(
          AnyReg(src), dest, flags, bytecodeOffset()));
  if (!ool) {
    return false;
  }
  bool isSaturating = flags & TRUNC_SATURATING;
  if (flags & TRUNC_UNSIGNED) {
    masm.wasmTruncateFloat32ToUInt64(src, dest, isSaturating, ool->entry(),
                                     ool->rejoin(), temp);
  } else {
    masm.wasmTruncateFloat32ToInt64(src, dest, isSaturating, ool->entry(),
                                    ool->rejoin(), temp);
  }
  return true;
}

bool BaseCompiler::truncateF64ToI64(RegF64 src, RegI64 dest, TruncFlags flags,
                                    RegF64 temp) {
  OutOfLineCode* ool =
      addOutOfLineCode(new (alloc_) OutOfLineTruncateCheckF32OrF64ToI64(
          AnyReg(src), dest, flags, bytecodeOffset()));
  if (!ool) {
    return false;
  }
  bool isSaturating = flags & TRUNC_SATURATING;
  if (flags & TRUNC_UNSIGNED) {
    masm.wasmTruncateDoubleToUInt64(src, dest, isSaturating, ool->entry(),
                                    ool->rejoin(), temp);
  } else {
    masm.wasmTruncateDoubleToInt64(src, dest, isSaturating, ool->entry(),
                                   ool->rejoin(), temp);
  }
  return true;
}

#endif  // RABALDR_FLOAT_TO_I64_CALLOUT

#ifndef RABALDR_I64_TO_FLOAT_CALLOUT

RegI32 BaseCompiler::needConvertI64ToFloatTemp(ValType to, bool isUnsigned) {
  bool needs = false;
  if (to == ValType::F64) {
    needs = isUnsigned && masm.convertUInt64ToDoubleNeedsTemp();
  } else {
#  if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    needs = true;
#  endif
  }
  return needs ? needI32() : RegI32::Invalid();
}

void BaseCompiler::convertI64ToF32(RegI64 src, bool isUnsigned, RegF32 dest,
                                   RegI32 temp) {
  if (isUnsigned) {
    masm.convertUInt64ToFloat32(src, dest, temp);
  } else {
    masm.convertInt64ToFloat32(src, dest);
  }
}

void BaseCompiler::convertI64ToF64(RegI64 src, bool isUnsigned, RegF64 dest,
                                   RegI32 temp) {
  if (isUnsigned) {
    masm.convertUInt64ToDouble(src, dest, temp);
  } else {
    masm.convertInt64ToDouble(src, dest);
  }
}

#endif  // RABALDR_I64_TO_FLOAT_CALLOUT

//////////////////////////////////////////////////////////////////////
//
// Global variable access.

Address BaseCompiler::addressOfGlobalVar(const GlobalDesc& global, RegI32 tmp) {
  uint32_t globalToTlsOffset = offsetof(TlsData, globalArea) + global.offset();
  fr.loadTlsPtr(tmp);
  if (global.isIndirect()) {
    masm.loadPtr(Address(tmp, globalToTlsOffset), tmp);
    return Address(tmp, 0);
  }
  return Address(tmp, globalToTlsOffset);
}

//////////////////////////////////////////////////////////////////////////////
//
// Basic emitters for simple operators.

static void AddI32(MacroAssembler& masm, RegI32 rs, RegI32 rsd) {
  masm.add32(rs, rsd);
}

static void AddImmI32(MacroAssembler& masm, int32_t c, RegI32 rsd) {
  masm.add32(Imm32(c), rsd);
}

static void SubI32(MacroAssembler& masm, RegI32 rs, RegI32 rsd) {
  masm.sub32(rs, rsd);
}

static void SubImmI32(MacroAssembler& masm, int32_t c, RegI32 rsd) {
  masm.sub32(Imm32(c), rsd);
}

static void MulI32(MacroAssembler& masm, RegI32 rs, RegI32 rsd) {
  masm.mul32(rs, rsd);
}

static void OrI32(MacroAssembler& masm, RegI32 rs, RegI32 rsd) {
  masm.or32(rs, rsd);
}

static void OrImmI32(MacroAssembler& masm, int32_t c, RegI32 rsd) {
  masm.or32(Imm32(c), rsd);
}

static void AndI32(MacroAssembler& masm, RegI32 rs, RegI32 rsd) {
  masm.and32(rs, rsd);
}

static void AndImmI32(MacroAssembler& masm, int32_t c, RegI32 rsd) {
  masm.and32(Imm32(c), rsd);
}

static void XorI32(MacroAssembler& masm, RegI32 rs, RegI32 rsd) {
  masm.xor32(rs, rsd);
}

static void XorImmI32(MacroAssembler& masm, int32_t c, RegI32 rsd) {
  masm.xor32(Imm32(c), rsd);
}

static void ClzI32(MacroAssembler& masm, RegI32 rsd) {
  masm.clz32(rsd, rsd, IsKnownNotZero(false));
}

static void CtzI32(MacroAssembler& masm, RegI32 rsd) {
  masm.ctz32(rsd, rsd, IsKnownNotZero(false));
}

// Currently common to PopcntI32 and PopcntI64
static RegI32 PopcntTemp(BaseCompiler& bc) {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
  return AssemblerX86Shared::HasPOPCNT() ? RegI32::Invalid() : bc.needI32();
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) || \
    defined(JS_CODEGEN_MIPS64)
  return bc.needI32();
#else
  MOZ_CRASH("BaseCompiler platform hook: PopcntTemp");
#endif
}

static void PopcntI32(BaseCompiler& bc, RegI32 rsd, RegI32 temp) {
  bc.masm.popcnt32(rsd, rsd, temp);
}

static void ShlI32(MacroAssembler& masm, RegI32 rs, RegI32 rsd) {
  masm.lshift32(rs, rsd);
}

static void ShlImmI32(MacroAssembler& masm, int32_t c, RegI32 rsd) {
  masm.lshift32(Imm32(c & 31), rsd);
}

static void ShrI32(MacroAssembler& masm, RegI32 rs, RegI32 rsd) {
  masm.rshift32Arithmetic(rs, rsd);
}

static void ShrImmI32(MacroAssembler& masm, int32_t c, RegI32 rsd) {
  masm.rshift32Arithmetic(Imm32(c & 31), rsd);
}

static void ShrUI32(MacroAssembler& masm, RegI32 rs, RegI32 rsd) {
  masm.rshift32(rs, rsd);
}

static void ShrUImmI32(MacroAssembler& masm, int32_t c, RegI32 rsd) {
  masm.rshift32(Imm32(c & 31), rsd);
}

static void RotlI32(MacroAssembler& masm, RegI32 rs, RegI32 rsd) {
  masm.rotateLeft(rs, rsd, rsd);
}

static void RotlImmI32(MacroAssembler& masm, int32_t c, RegI32 rsd) {
  masm.rotateLeft(Imm32(c & 31), rsd, rsd);
}

static void RotrI32(MacroAssembler& masm, RegI32 rs, RegI32 rsd) {
  masm.rotateRight(rs, rsd, rsd);
}

static void RotrImmI32(MacroAssembler& masm, int32_t c, RegI32 rsd) {
  masm.rotateRight(Imm32(c & 31), rsd, rsd);
}

static void EqzI32(MacroAssembler& masm, RegI32 rsd) {
  masm.cmp32Set(Assembler::Equal, rsd, Imm32(0), rsd);
}

static void WrapI64ToI32(MacroAssembler& masm, RegI64 rs, RegI32 rd) {
  masm.move64To32(rs, rd);
}

static void AddI64(MacroAssembler& masm, RegI64 rs, RegI64 rsd) {
  masm.add64(rs, rsd);
}

static void AddImmI64(MacroAssembler& masm, int64_t c, RegI64 rsd) {
  masm.add64(Imm64(c), rsd);
}

static void SubI64(MacroAssembler& masm, RegI64 rs, RegI64 rsd) {
  masm.sub64(rs, rsd);
}

static void SubImmI64(MacroAssembler& masm, int64_t c, RegI64 rsd) {
  masm.sub64(Imm64(c), rsd);
}

static void OrI64(MacroAssembler& masm, RegI64 rs, RegI64 rsd) {
  masm.or64(rs, rsd);
}

static void OrImmI64(MacroAssembler& masm, int64_t c, RegI64 rsd) {
  masm.or64(Imm64(c), rsd);
}

static void AndI64(MacroAssembler& masm, RegI64 rs, RegI64 rsd) {
  masm.and64(rs, rsd);
}

static void AndImmI64(MacroAssembler& masm, int64_t c, RegI64 rsd) {
  masm.and64(Imm64(c), rsd);
}

static void XorI64(MacroAssembler& masm, RegI64 rs, RegI64 rsd) {
  masm.xor64(rs, rsd);
}

static void XorImmI64(MacroAssembler& masm, int64_t c, RegI64 rsd) {
  masm.xor64(Imm64(c), rsd);
}

static void ClzI64(BaseCompiler& bc, RegI64 rsd) {
  bc.masm.clz64(rsd, bc.lowPart(rsd));
  bc.maybeClearHighPart(rsd);
}

static void CtzI64(BaseCompiler& bc, RegI64 rsd) {
  bc.masm.ctz64(rsd, bc.lowPart(rsd));
  bc.maybeClearHighPart(rsd);
}

static void PopcntI64(BaseCompiler& bc, RegI64 rsd, RegI32 temp) {
  bc.masm.popcnt64(rsd, rsd, temp);
}

static void ShlI64(BaseCompiler& bc, RegI64 rs, RegI64 rsd) {
  bc.masm.lshift64(bc.lowPart(rs), rsd);
}

static void ShlImmI64(MacroAssembler& masm, int64_t c, RegI64 rsd) {
  masm.lshift64(Imm32(c & 63), rsd);
}

static void ShrI64(BaseCompiler& bc, RegI64 rs, RegI64 rsd) {
  bc.masm.rshift64Arithmetic(bc.lowPart(rs), rsd);
}

static void ShrImmI64(MacroAssembler& masm, int64_t c, RegI64 rsd) {
  masm.rshift64Arithmetic(Imm32(c & 63), rsd);
}

static void ShrUI64(BaseCompiler& bc, RegI64 rs, RegI64 rsd) {
  bc.masm.rshift64(bc.lowPart(rs), rsd);
}

static void ShrUImmI64(MacroAssembler& masm, int64_t c, RegI64 rsd) {
  masm.rshift64(Imm32(c & 63), rsd);
}

static void EqzI64(MacroAssembler& masm, RegI64 rs, RegI32 rd) {
#ifdef JS_PUNBOX64
  masm.cmpPtrSet(Assembler::Equal, rs.reg, ImmWord(0), rd);
#else
  MOZ_ASSERT(rs.low == rd);
  masm.or32(rs.high, rs.low);
  masm.cmp32Set(Assembler::Equal, rs.low, Imm32(0), rd);
#endif
}

static void AddF64(MacroAssembler& masm, RegF64 rs, RegF64 rsd) {
  masm.addDouble(rs, rsd);
}

static void SubF64(MacroAssembler& masm, RegF64 rs, RegF64 rsd) {
  masm.subDouble(rs, rsd);
}

static void MulF64(MacroAssembler& masm, RegF64 rs, RegF64 rsd) {
  masm.mulDouble(rs, rsd);
}

static void DivF64(MacroAssembler& masm, RegF64 rs, RegF64 rsd) {
  masm.divDouble(rs, rsd);
}

static void MinF64(BaseCompiler& bc, RegF64 rs, RegF64 rsd) {
  // Convert signaling NaN to quiet NaNs.
  //
  // TODO / OPTIMIZE (bug 1316824): see comment in MinF32.
#ifdef RABALDR_SCRATCH_F64
  ScratchF64 zero(bc.ra);
#else
  ScratchF64 zero(bc.masm);
#endif
  bc.masm.loadConstantDouble(0, zero);
  bc.masm.subDouble(zero, rsd);
  bc.masm.subDouble(zero, rs);
  bc.masm.minDouble(rs, rsd, HandleNaNSpecially(true));
}

static void MaxF64(BaseCompiler& bc, RegF64 rs, RegF64 rsd) {
  // Convert signaling NaN to quiet NaNs.
  //
  // TODO / OPTIMIZE (bug 1316824): see comment in MinF32.
#ifdef RABALDR_SCRATCH_F64
  ScratchF64 zero(bc.ra);
#else
  ScratchF64 zero(bc.masm);
#endif
  bc.masm.loadConstantDouble(0, zero);
  bc.masm.subDouble(zero, rsd);
  bc.masm.subDouble(zero, rs);
  bc.masm.maxDouble(rs, rsd, HandleNaNSpecially(true));
}

static void CopysignF64(MacroAssembler& masm, RegF64 rs, RegF64 rsd,
                        RegI64 temp0, RegI64 temp1) {
  masm.moveDoubleToGPR64(rsd, temp0);
  masm.moveDoubleToGPR64(rs, temp1);
  masm.and64(Imm64(INT64_MAX), temp0);
  masm.and64(Imm64(INT64_MIN), temp1);
  masm.or64(temp1, temp0);
  masm.moveGPR64ToDouble(temp0, rsd);
}

static void AbsF64(MacroAssembler& masm, RegF64 rsd) {
  masm.absDouble(rsd, rsd);
}

static void NegateF64(MacroAssembler& masm, RegF64 rsd) {
  masm.negateDouble(rsd);
}

static void SqrtF64(MacroAssembler& masm, RegF64 rsd) {
  masm.sqrtDouble(rsd, rsd);
}

static void AddF32(MacroAssembler& masm, RegF32 rs, RegF32 rsd) {
  masm.addFloat32(rs, rsd);
}

static void SubF32(MacroAssembler& masm, RegF32 rs, RegF32 rsd) {
  masm.subFloat32(rs, rsd);
}

static void MulF32(MacroAssembler& masm, RegF32 rs, RegF32 rsd) {
  masm.mulFloat32(rs, rsd);
}

static void DivF32(MacroAssembler& masm, RegF32 rs, RegF32 rsd) {
  masm.divFloat32(rs, rsd);
}

static void MinF32(BaseCompiler& bc, RegF32 rs, RegF32 rsd) {
  // Convert signaling NaN to quiet NaNs.
  //
  // TODO / OPTIMIZE (bug 1316824): Don't do this if one of the operands
  // is known to be a constant.
#ifdef RABALDR_SCRATCH_F32
  ScratchF32 zero(bc.ra);
#else
  ScratchF32 zero(bc.masm);
#endif
  bc.masm.loadConstantFloat32(0.f, zero);
  bc.masm.subFloat32(zero, rsd);
  bc.masm.subFloat32(zero, rs);
  bc.masm.minFloat32(rs, rsd, HandleNaNSpecially(true));
}

static void MaxF32(BaseCompiler& bc, RegF32 rs, RegF32 rsd) {
  // Convert signaling NaN to quiet NaNs.
  //
  // TODO / OPTIMIZE (bug 1316824): see comment in MinF32.
#ifdef RABALDR_SCRATCH_F32
  ScratchF32 zero(bc.ra);
#else
  ScratchF32 zero(bc.masm);
#endif
  bc.masm.loadConstantFloat32(0.f, zero);
  bc.masm.subFloat32(zero, rsd);
  bc.masm.subFloat32(zero, rs);
  bc.masm.maxFloat32(rs, rsd, HandleNaNSpecially(true));
}

static void CopysignF32(MacroAssembler& masm, RegF32 rs, RegF32 rsd,
                        RegI32 temp0, RegI32 temp1) {
  masm.moveFloat32ToGPR(rsd, temp0);
  masm.moveFloat32ToGPR(rs, temp1);
  masm.and32(Imm32(INT32_MAX), temp0);
  masm.and32(Imm32(INT32_MIN), temp1);
  masm.or32(temp1, temp0);
  masm.moveGPRToFloat32(temp0, rsd);
}

static void AbsF32(MacroAssembler& masm, RegF32 rsd) {
  masm.absFloat32(rsd, rsd);
}

static void NegateF32(MacroAssembler& masm, RegF32 rsd) {
  masm.negateFloat(rsd);
}

static void SqrtF32(MacroAssembler& masm, RegF32 rsd) {
  masm.sqrtFloat32(rsd, rsd);
}

#ifndef RABALDR_I64_TO_FLOAT_CALLOUT
static void ConvertI64ToF32(MacroAssembler& masm, RegI64 rs, RegF32 rd) {
  masm.convertInt64ToFloat32(rs, rd);
}

static void ConvertI64ToF64(MacroAssembler& masm, RegI64 rs, RegF64 rd) {
  masm.convertInt64ToDouble(rs, rd);
}
#endif

static void ReinterpretF32AsI32(MacroAssembler& masm, RegF32 rs, RegI32 rd) {
  masm.moveFloat32ToGPR(rs, rd);
}

static void ReinterpretF64AsI64(MacroAssembler& masm, RegF64 rs, RegI64 rd) {
  masm.moveDoubleToGPR64(rs, rd);
}

static void ConvertF64ToF32(MacroAssembler& masm, RegF64 rs, RegF32 rd) {
  masm.convertDoubleToFloat32(rs, rd);
}

static void ConvertI32ToF32(MacroAssembler& masm, RegI32 rs, RegF32 rd) {
  masm.convertInt32ToFloat32(rs, rd);
}

static void ConvertU32ToF32(MacroAssembler& masm, RegI32 rs, RegF32 rd) {
  masm.convertUInt32ToFloat32(rs, rd);
}

static void ConvertF32ToF64(MacroAssembler& masm, RegF32 rs, RegF64 rd) {
  masm.convertFloat32ToDouble(rs, rd);
}

static void ConvertI32ToF64(MacroAssembler& masm, RegI32 rs, RegF64 rd) {
  masm.convertInt32ToDouble(rs, rd);
}

static void ConvertU32ToF64(MacroAssembler& masm, RegI32 rs, RegF64 rd) {
  masm.convertUInt32ToDouble(rs, rd);
}

static void ReinterpretI32AsF32(MacroAssembler& masm, RegI32 rs, RegF32 rd) {
  masm.moveGPRToFloat32(rs, rd);
}

static void ReinterpretI64AsF64(MacroAssembler& masm, RegI64 rs, RegF64 rd) {
  masm.moveGPR64ToDouble(rs, rd);
}

static void ExtendI32_8(BaseCompiler& bc, RegI32 rsd) {
#ifdef JS_CODEGEN_X86
  if (!bc.ra.isSingleByteI32(rsd)) {
    ScratchI8 scratch(bc.ra);
    bc.masm.move32(rsd, scratch);
    bc.masm.move8SignExtend(scratch, rsd);
    return;
  }
#endif
  bc.masm.move8SignExtend(rsd, rsd);
}

static void ExtendI32_16(MacroAssembler& masm, RegI32 rsd) {
  masm.move16SignExtend(rsd, rsd);
}

void BaseCompiler::emitMultiplyI64() {
  RegI64 r, rs;
  RegI32 temp;
  popAndAllocateForMulI64(&r, &rs, &temp);
  masm.mul64(rs, r, temp);
  maybeFree(temp);
  freeI64(rs);
  pushI64(r);
}

template <typename RegType, typename IntType>
void BaseCompiler::quotientOrRemainder(
    RegType rs, RegType rsd, RegType reserved, IsUnsigned isUnsigned,
    ZeroOnOverflow zeroOnOverflow, bool isConst, IntType c,
    void (*operate)(MacroAssembler& masm, RegType rs, RegType rsd,
                    RegType reserved, IsUnsigned isUnsigned)) {
  Label done;
  if (!isConst || c == 0) {
    checkDivideByZero(rs);
  }
  if (!isUnsigned && (!isConst || c == -1)) {
    checkDivideSignedOverflow(rs, rsd, &done, zeroOnOverflow);
  }
  operate(masm, rs, rsd, reserved, isUnsigned);
  masm.bind(&done);
}

void BaseCompiler::emitQuotientI32() {
  int32_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwo(&c, &power, 0)) {
    if (power != 0) {
      RegI32 r = popI32();
      Label positive;
      masm.branchTest32(Assembler::NotSigned, r, r, &positive);
      masm.add32(Imm32(c - 1), r);
      masm.bind(&positive);

      masm.rshift32Arithmetic(Imm32(power & 31), r);
      pushI32(r);
    }
  } else {
    bool isConst = peekConst(&c);
    RegI32 r, rs, reserved;
    popAndAllocateForDivAndRemI32(&r, &rs, &reserved);
    quotientOrRemainder(rs, r, reserved, IsUnsigned(false),
                        ZeroOnOverflow(false), isConst, c, QuotientI32);
    maybeFree(reserved);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitQuotientU32() {
  int32_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwo(&c, &power, 0)) {
    if (power != 0) {
      RegI32 r = popI32();
      masm.rshift32(Imm32(power & 31), r);
      pushI32(r);
    }
  } else {
    bool isConst = peekConst(&c);
    RegI32 r, rs, reserved;
    popAndAllocateForDivAndRemI32(&r, &rs, &reserved);
    quotientOrRemainder(rs, r, reserved, IsUnsigned(true),
                        ZeroOnOverflow(false), isConst, c, QuotientI32);
    maybeFree(reserved);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitRemainderI32() {
  int32_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwo(&c, &power, 1)) {
    RegI32 r = popI32();
    RegI32 temp = needI32();
    moveI32(r, temp);

    Label positive;
    masm.branchTest32(Assembler::NotSigned, temp, temp, &positive);
    masm.add32(Imm32(c - 1), temp);
    masm.bind(&positive);

    masm.rshift32Arithmetic(Imm32(power & 31), temp);
    masm.lshift32(Imm32(power & 31), temp);
    masm.sub32(temp, r);
    freeI32(temp);

    pushI32(r);
  } else {
    bool isConst = peekConst(&c);
    RegI32 r, rs, reserved;
    popAndAllocateForDivAndRemI32(&r, &rs, &reserved);
    quotientOrRemainder(rs, r, reserved, IsUnsigned(false),
                        ZeroOnOverflow(true), isConst, c, RemainderI32);
    maybeFree(reserved);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitRemainderU32() {
  int32_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwo(&c, &power, 1)) {
    RegI32 r = popI32();
    masm.and32(Imm32(c - 1), r);
    pushI32(r);
  } else {
    bool isConst = peekConst(&c);
    RegI32 r, rs, reserved;
    popAndAllocateForDivAndRemI32(&r, &rs, &reserved);
    quotientOrRemainder(rs, r, reserved, IsUnsigned(true), ZeroOnOverflow(true),
                        isConst, c, RemainderI32);
    maybeFree(reserved);
    freeI32(rs);
    pushI32(r);
  }
}

#ifndef RABALDR_INT_DIV_I64_CALLOUT
void BaseCompiler::emitQuotientI64() {
  int64_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwo(&c, &power, 0)) {
    if (power != 0) {
      RegI64 r = popI64();
      Label positive;
      masm.branchTest64(Assembler::NotSigned, r, r, RegI32::Invalid(),
                        &positive);
      masm.add64(Imm64(c - 1), r);
      masm.bind(&positive);

      masm.rshift64Arithmetic(Imm32(power & 63), r);
      pushI64(r);
    }
  } else {
    bool isConst = peekConst(&c);
    RegI64 r, rs, reserved;
    popAndAllocateForDivAndRemI64(&r, &rs, &reserved, IsRemainder(false));
    quotientOrRemainder(rs, r, reserved, IsUnsigned(false),
                        ZeroOnOverflow(false), isConst, c, QuotientI64);
    maybeFree(reserved);
    freeI64(rs);
    pushI64(r);
  }
}

void BaseCompiler::emitQuotientU64() {
  int64_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwo(&c, &power, 0)) {
    if (power != 0) {
      RegI64 r = popI64();
      masm.rshift64(Imm32(power & 63), r);
      pushI64(r);
    }
  } else {
    bool isConst = peekConst(&c);
    RegI64 r, rs, reserved;
    popAndAllocateForDivAndRemI64(&r, &rs, &reserved, IsRemainder(false));
    quotientOrRemainder(rs, r, reserved, IsUnsigned(true),
                        ZeroOnOverflow(false), isConst, c, QuotientI64);
    maybeFree(reserved);
    freeI64(rs);
    pushI64(r);
  }
}

void BaseCompiler::emitRemainderI64() {
  int64_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwo(&c, &power, 1)) {
    RegI64 r = popI64();
    RegI64 temp = needI64();
    moveI64(r, temp);

    Label positive;
    masm.branchTest64(Assembler::NotSigned, temp, temp, RegI32::Invalid(),
                      &positive);
    masm.add64(Imm64(c - 1), temp);
    masm.bind(&positive);

    masm.rshift64Arithmetic(Imm32(power & 63), temp);
    masm.lshift64(Imm32(power & 63), temp);
    masm.sub64(temp, r);
    freeI64(temp);

    pushI64(r);
  } else {
    bool isConst = peekConst(&c);
    RegI64 r, rs, reserved;
    popAndAllocateForDivAndRemI64(&r, &rs, &reserved, IsRemainder(true));
    quotientOrRemainder(rs, r, reserved, IsUnsigned(false),
                        ZeroOnOverflow(true), isConst, c, RemainderI64);
    maybeFree(reserved);
    freeI64(rs);
    pushI64(r);
  }
}

void BaseCompiler::emitRemainderU64() {
  int64_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwo(&c, &power, 1)) {
    RegI64 r = popI64();
    masm.and64(Imm64(c - 1), r);
    pushI64(r);
  } else {
    bool isConst = peekConst(&c);
    RegI64 r, rs, reserved;
    popAndAllocateForDivAndRemI64(&r, &rs, &reserved, IsRemainder(true));
    quotientOrRemainder(rs, r, reserved, IsUnsigned(true), ZeroOnOverflow(true),
                        isConst, c, RemainderI64);
    maybeFree(reserved);
    freeI64(rs);
    pushI64(r);
  }
}
#endif  // RABALDR_INT_DIV_I64_CALLOUT

void BaseCompiler::emitRotrI64() {
  int64_t c;
  if (popConst(&c)) {
    RegI64 r = popI64();
    RegI32 temp = needRotate64Temp();
    masm.rotateRight64(Imm32(c & 63), r, r, temp);
    maybeFree(temp);
    pushI64(r);
  } else {
    RegI64 rs = popI64RhsForRotate();
    RegI64 r = popI64();
    masm.rotateRight64(lowPart(rs), r, r, maybeHighPart(rs));
    freeI64(rs);
    pushI64(r);
  }
}

void BaseCompiler::emitRotlI64() {
  int64_t c;
  if (popConst(&c)) {
    RegI64 r = popI64();
    RegI32 temp = needRotate64Temp();
    masm.rotateLeft64(Imm32(c & 63), r, r, temp);
    maybeFree(temp);
    pushI64(r);
  } else {
    RegI64 rs = popI64RhsForRotate();
    RegI64 r = popI64();
    masm.rotateLeft64(lowPart(rs), r, r, maybeHighPart(rs));
    freeI64(rs);
    pushI64(r);
  }
}

void BaseCompiler::emitEqzI32() {
  if (sniffConditionalControlEqz(ValType::I32)) {
    return;
  }
  emitUnop(EqzI32);
}

void BaseCompiler::emitEqzI64() {
  if (sniffConditionalControlEqz(ValType::I64)) {
    return;
  }
  emitUnop(EqzI64);
}

template <TruncFlags flags>
bool BaseCompiler::emitTruncateF32ToI32() {
  RegF32 rs = popF32();
  RegI32 rd = needI32();
  if (!truncateF32ToI32(rs, rd, flags)) {
    return false;
  }
  freeF32(rs);
  pushI32(rd);
  return true;
}

template <TruncFlags flags>
bool BaseCompiler::emitTruncateF64ToI32() {
  RegF64 rs = popF64();
  RegI32 rd = needI32();
  if (!truncateF64ToI32(rs, rd, flags)) {
    return false;
  }
  freeF64(rs);
  pushI32(rd);
  return true;
}

#ifndef RABALDR_FLOAT_TO_I64_CALLOUT
template <TruncFlags flags>
bool BaseCompiler::emitTruncateF32ToI64() {
  RegF32 rs = popF32();
  RegI64 rd = needI64();
  RegF64 temp = needTempForFloatingToI64(flags);
  if (!truncateF32ToI64(rs, rd, flags, temp)) {
    return false;
  }
  maybeFree(temp);
  freeF32(rs);
  pushI64(rd);
  return true;
}

template <TruncFlags flags>
bool BaseCompiler::emitTruncateF64ToI64() {
  RegF64 rs = popF64();
  RegI64 rd = needI64();
  RegF64 temp = needTempForFloatingToI64(flags);
  if (!truncateF64ToI64(rs, rd, flags, temp)) {
    return false;
  }
  maybeFree(temp);
  freeF64(rs);
  pushI64(rd);
  return true;
}
#endif  // RABALDR_FLOAT_TO_I64_CALLOUT

void BaseCompiler::emitExtendI64_8() {
  RegI64 r;
  popI64ForSignExtendI64(&r);
  masm.move8To64SignExtend(lowPart(r), r);
  pushI64(r);
}

void BaseCompiler::emitExtendI64_16() {
  RegI64 r;
  popI64ForSignExtendI64(&r);
  masm.move16To64SignExtend(lowPart(r), r);
  pushI64(r);
}

void BaseCompiler::emitExtendI64_32() {
  RegI64 r;
  popI64ForSignExtendI64(&r);
  masm.move32To64SignExtend(lowPart(r), r);
  pushI64(r);
}

void BaseCompiler::emitExtendI32ToI64() {
  RegI64 r;
  popI32ForSignExtendI64(&r);
  masm.move32To64SignExtend(lowPart(r), r);
  pushI64(r);
}

void BaseCompiler::emitExtendU32ToI64() {
  RegI32 rs = popI32();
  RegI64 rd = widenI32(rs);
  masm.move32To64ZeroExtend(rs, rd);
  pushI64(rd);
}

#ifndef RABALDR_I64_TO_FLOAT_CALLOUT
void BaseCompiler::emitConvertU64ToF32() {
  RegI64 rs = popI64();
  RegF32 rd = needF32();
  RegI32 temp = needConvertI64ToFloatTemp(ValType::F32, IsUnsigned(true));
  convertI64ToF32(rs, IsUnsigned(true), rd, temp);
  maybeFree(temp);
  freeI64(rs);
  pushF32(rd);
}

void BaseCompiler::emitConvertU64ToF64() {
  RegI64 rs = popI64();
  RegF64 rd = needF64();
  RegI32 temp = needConvertI64ToFloatTemp(ValType::F64, IsUnsigned(true));
  convertI64ToF64(rs, IsUnsigned(true), rd, temp);
  maybeFree(temp);
  freeI64(rs);
  pushF64(rd);
}
#endif  // RABALDR_I64_TO_FLOAT_CALLOUT

////////////////////////////////////////////////////////////
//
// Machinery for optimized conditional branches.
//
// To disable this optimization it is enough always to return false from
// sniffConditionalControl{Cmp,Eqz}.

struct BranchState {
  union {
    struct {
      RegI32 lhs;
      RegI32 rhs;
      int32_t imm;
      bool rhsImm;
    } i32;
    struct {
      RegI64 lhs;
      RegI64 rhs;
      int64_t imm;
      bool rhsImm;
    } i64;
    struct {
      RegF32 lhs;
      RegF32 rhs;
    } f32;
    struct {
      RegF64 lhs;
      RegF64 rhs;
    } f64;
  };

  Label* const label;             // The target of the branch, never NULL
  const StackHeight stackHeight;  // The stack base above which to place
  // stack-spilled block results, if
  // hasBlockResults().
  const bool invertBranch;      // If true, invert the sense of the branch
  const ResultType resultType;  // The result propagated along the edges

  explicit BranchState(Label* label)
      : label(label),
        stackHeight(StackHeight::Invalid()),
        invertBranch(false),
        resultType(ResultType::Empty()) {}

  BranchState(Label* label, bool invertBranch)
      : label(label),
        stackHeight(StackHeight::Invalid()),
        invertBranch(invertBranch),
        resultType(ResultType::Empty()) {}

  BranchState(Label* label, StackHeight stackHeight, bool invertBranch,
              ResultType resultType)
      : label(label),
        stackHeight(stackHeight),
        invertBranch(invertBranch),
        resultType(resultType) {}

  bool hasBlockResults() const { return stackHeight.isValid(); }
};

void BaseCompiler::setLatentCompare(Assembler::Condition compareOp,
                                    ValType operandType) {
  latentOp_ = LatentOp::Compare;
  latentType_ = operandType;
  latentIntCmp_ = compareOp;
}

void BaseCompiler::setLatentCompare(Assembler::DoubleCondition compareOp,
                                    ValType operandType) {
  latentOp_ = LatentOp::Compare;
  latentType_ = operandType;
  latentDoubleCmp_ = compareOp;
}

void BaseCompiler::setLatentEqz(ValType operandType) {
  latentOp_ = LatentOp::Eqz;
  latentType_ = operandType;
}

bool BaseCompiler::hasLatentOp() const { return latentOp_ != LatentOp::None; }

void BaseCompiler::resetLatentOp() { latentOp_ = LatentOp::None; }

// Emit a conditional branch that optionally and optimally cleans up the CPU
// stack before we branch.
//
// Cond is either Assembler::Condition or Assembler::DoubleCondition.
//
// Lhs is RegI32, RegI64, or RegF32, RegF64, or RegRef.
//
// Rhs is either the same as Lhs, or an immediate expression compatible with
// Lhs "when applicable".

template <typename Cond, typename Lhs, typename Rhs>
bool BaseCompiler::jumpConditionalWithResults(BranchState* b, Cond cond,
                                              Lhs lhs, Rhs rhs) {
  if (b->hasBlockResults()) {
    StackHeight resultsBase(0);
    if (!topBranchParams(b->resultType, &resultsBase)) {
      return false;
    }
    if (b->stackHeight != resultsBase) {
      Label notTaken;
      branchTo(b->invertBranch ? cond : Assembler::InvertCondition(cond), lhs,
               rhs, &notTaken);

      // Shuffle stack args.
      shuffleStackResultsBeforeBranch(resultsBase, b->stackHeight,
                                      b->resultType);
      masm.jump(b->label);
      masm.bind(&notTaken);
      return true;
    }
  }

  branchTo(b->invertBranch ? Assembler::InvertCondition(cond) : cond, lhs, rhs,
           b->label);
  return true;
}

// sniffConditionalControl{Cmp,Eqz} may modify the latentWhatever_ state in
// the BaseCompiler so that a subsequent conditional branch can be compiled
// optimally.  emitBranchSetup() and emitBranchPerform() will consume that
// state.  If the latter methods are not called because deadCode_ is true
// then the compiler MUST instead call resetLatentOp() to reset the state.

template <typename Cond>
bool BaseCompiler::sniffConditionalControlCmp(Cond compareOp,
                                              ValType operandType) {
  MOZ_ASSERT(latentOp_ == LatentOp::None,
             "Latent comparison state not properly reset");

#ifdef JS_CODEGEN_X86
  // On x86, latent i64 binary comparisons use too many registers: the
  // reserved join register and the lhs and rhs operands require six, but we
  // only have five.
  if (operandType == ValType::I64) {
    return false;
  }
#endif

  // No optimization for pointer compares yet.
  if (operandType.isRefRepr()) {
    return false;
  }

  OpBytes op{};
  iter_.peekOp(&op);
  switch (op.b0) {
    case uint16_t(Op::BrIf):
    case uint16_t(Op::If):
    case uint16_t(Op::SelectNumeric):
    case uint16_t(Op::SelectTyped):
      setLatentCompare(compareOp, operandType);
      return true;
    default:
      return false;
  }
}

bool BaseCompiler::sniffConditionalControlEqz(ValType operandType) {
  MOZ_ASSERT(latentOp_ == LatentOp::None,
             "Latent comparison state not properly reset");

  OpBytes op{};
  iter_.peekOp(&op);
  switch (op.b0) {
    case uint16_t(Op::BrIf):
    case uint16_t(Op::SelectNumeric):
    case uint16_t(Op::SelectTyped):
    case uint16_t(Op::If):
      setLatentEqz(operandType);
      return true;
    default:
      return false;
  }
}

void BaseCompiler::emitBranchSetup(BranchState* b) {
  // Avoid allocating operands to latentOp_ to result registers.
  if (b->hasBlockResults()) {
    needResultRegisters(b->resultType);
  }

  // Set up fields so that emitBranchPerform() need not switch on latentOp_.
  switch (latentOp_) {
    case LatentOp::None: {
      latentIntCmp_ = Assembler::NotEqual;
      latentType_ = ValType::I32;
      b->i32.lhs = popI32();
      b->i32.rhsImm = true;
      b->i32.imm = 0;
      break;
    }
    case LatentOp::Compare: {
      switch (latentType_.kind()) {
        case ValType::I32: {
          if (popConst(&b->i32.imm)) {
            b->i32.lhs = popI32();
            b->i32.rhsImm = true;
          } else {
            pop2xI32(&b->i32.lhs, &b->i32.rhs);
            b->i32.rhsImm = false;
          }
          break;
        }
        case ValType::I64: {
          pop2xI64(&b->i64.lhs, &b->i64.rhs);
          b->i64.rhsImm = false;
          break;
        }
        case ValType::F32: {
          pop2xF32(&b->f32.lhs, &b->f32.rhs);
          break;
        }
        case ValType::F64: {
          pop2xF64(&b->f64.lhs, &b->f64.rhs);
          break;
        }
        default: {
          MOZ_CRASH("Unexpected type for LatentOp::Compare");
        }
      }
      break;
    }
    case LatentOp::Eqz: {
      switch (latentType_.kind()) {
        case ValType::I32: {
          latentIntCmp_ = Assembler::Equal;
          b->i32.lhs = popI32();
          b->i32.rhsImm = true;
          b->i32.imm = 0;
          break;
        }
        case ValType::I64: {
          latentIntCmp_ = Assembler::Equal;
          b->i64.lhs = popI64();
          b->i64.rhsImm = true;
          b->i64.imm = 0;
          break;
        }
        default: {
          MOZ_CRASH("Unexpected type for LatentOp::Eqz");
        }
      }
      break;
    }
  }

  if (b->hasBlockResults()) {
    freeResultRegisters(b->resultType);
  }
}

bool BaseCompiler::emitBranchPerform(BranchState* b) {
  switch (latentType_.kind()) {
    case ValType::I32: {
      if (b->i32.rhsImm) {
        if (!jumpConditionalWithResults(b, latentIntCmp_, b->i32.lhs,
                                        Imm32(b->i32.imm))) {
          return false;
        }
      } else {
        if (!jumpConditionalWithResults(b, latentIntCmp_, b->i32.lhs,
                                        b->i32.rhs)) {
          return false;
        }
        freeI32(b->i32.rhs);
      }
      freeI32(b->i32.lhs);
      break;
    }
    case ValType::I64: {
      if (b->i64.rhsImm) {
        if (!jumpConditionalWithResults(b, latentIntCmp_, b->i64.lhs,
                                        Imm64(b->i64.imm))) {
          return false;
        }
      } else {
        if (!jumpConditionalWithResults(b, latentIntCmp_, b->i64.lhs,
                                        b->i64.rhs)) {
          return false;
        }
        freeI64(b->i64.rhs);
      }
      freeI64(b->i64.lhs);
      break;
    }
    case ValType::F32: {
      if (!jumpConditionalWithResults(b, latentDoubleCmp_, b->f32.lhs,
                                      b->f32.rhs)) {
        return false;
      }
      freeF32(b->f32.lhs);
      freeF32(b->f32.rhs);
      break;
    }
    case ValType::F64: {
      if (!jumpConditionalWithResults(b, latentDoubleCmp_, b->f64.lhs,
                                      b->f64.rhs)) {
        return false;
      }
      freeF64(b->f64.lhs);
      freeF64(b->f64.rhs);
      break;
    }
    default: {
      MOZ_CRASH("Unexpected type for LatentOp::Compare");
    }
  }
  resetLatentOp();
  return true;
}

// For blocks and loops and ifs:
//
//  - Sync the value stack before going into the block in order to simplify exit
//    from the block: all exits from the block can assume that there are no
//    live registers except the one carrying the exit value.
//  - The block can accumulate a number of dead values on the stacks, so when
//    branching out of the block or falling out at the end be sure to
//    pop the appropriate stacks back to where they were on entry, while
//    preserving the exit value.
//  - A continue branch in a loop is much like an exit branch, but the branch
//    value must not be preserved.
//  - The exit value is always in a designated join register (type dependent).

bool BaseCompiler::emitBlock() {
  ResultType params;
  if (!iter_.readBlock(&params)) {
    return false;
  }

  if (!deadCode_) {
    sync();  // Simplifies branching out from block
  }

  initControl(controlItem(), params);

  return true;
}

bool BaseCompiler::endBlock(ResultType type) {
  Control& block = controlItem();

  if (deadCode_) {
    // Block does not fall through; reset stack.
    fr.resetStackHeight(block.stackHeight, type);
    popValueStackTo(block.stackSize);
  } else {
    // If the block label is used, we have a control join, so we need to shuffle
    // fallthrough values into place.  Otherwise if it's not a control join, we
    // can leave the value stack alone.
    MOZ_ASSERT(stk_.length() == block.stackSize + type.length());
    if (block.label.used()) {
      popBlockResults(type, block.stackHeight, ContinuationKind::Fallthrough);
    }
    block.bceSafeOnExit &= bceSafe_;
  }

  // Bind after cleanup: branches out will have popped the stack.
  if (block.label.used()) {
    masm.bind(&block.label);
    if (deadCode_) {
      captureResultRegisters(type);
      deadCode_ = false;
    }
    if (!pushBlockResults(type)) {
      return false;
    }
  }

  bceSafe_ = block.bceSafeOnExit;

  return true;
}

bool BaseCompiler::emitLoop() {
  ResultType params;
  if (!iter_.readLoop(&params)) {
    return false;
  }

  if (!deadCode_) {
    sync();  // Simplifies branching out from block
  }

  initControl(controlItem(), params);
  bceSafe_ = 0;

  if (!deadCode_) {
    // Loop entry is a control join, so shuffle the entry parameters into the
    // well-known locations.
    if (!topBlockParams(params)) {
      return false;
    }
    masm.nopAlign(CodeAlignment);
    masm.bind(&controlItem(0).label);
    // The interrupt check barfs if there are live registers.
    sync();
    if (!addInterruptCheck()) {
      return false;
    }
  }

  return true;
}

// The bodies of the "then" and "else" arms can be arbitrary sequences
// of expressions, they push control and increment the nesting and can
// even be targeted by jumps.  A branch to the "if" block branches to
// the exit of the if, ie, it's like "break".  Consider:
//
//      (func (result i32)
//       (if (i32.const 1)
//           (begin (br 1) (unreachable))
//           (begin (unreachable)))
//       (i32.const 1))
//
// The branch causes neither of the unreachable expressions to be
// evaluated.

bool BaseCompiler::emitIf() {
  ResultType params;
  Nothing unused_cond;
  if (!iter_.readIf(&params, &unused_cond)) {
    return false;
  }

  BranchState b(&controlItem().otherLabel, InvertBranch(true));
  if (!deadCode_) {
    needResultRegisters(params);
    emitBranchSetup(&b);
    freeResultRegisters(params);
    sync();
  } else {
    resetLatentOp();
  }

  initControl(controlItem(), params);

  if (!deadCode_) {
    // Because params can flow immediately to results in the case of an empty
    // "then" or "else" block, and the result of an if/then is a join in
    // general, we shuffle params eagerly to the result allocations.
    if (!topBlockParams(params)) {
      return false;
    }
    if (!emitBranchPerform(&b)) {
      return false;
    }
  }

  return true;
}

bool BaseCompiler::endIfThen(ResultType type) {
  Control& ifThen = controlItem();

  // The parameters to the "if" logically flow to both the "then" and "else"
  // blocks, but the "else" block is empty.  Since we know that the "if"
  // type-checks, that means that the "else" parameters are the "else" results,
  // and that the "if"'s result type is the same as its parameter type.

  if (deadCode_) {
    // "then" arm does not fall through; reset stack.
    fr.resetStackHeight(ifThen.stackHeight, type);
    popValueStackTo(ifThen.stackSize);
    if (!ifThen.deadOnArrival) {
      captureResultRegisters(type);
    }
  } else {
    MOZ_ASSERT(stk_.length() == ifThen.stackSize + type.length());
    // Assume we have a control join, so place results in block result
    // allocations.
    popBlockResults(type, ifThen.stackHeight, ContinuationKind::Fallthrough);
    MOZ_ASSERT(!ifThen.deadOnArrival);
  }

  if (ifThen.otherLabel.used()) {
    masm.bind(&ifThen.otherLabel);
  }

  if (ifThen.label.used()) {
    masm.bind(&ifThen.label);
  }

  if (!deadCode_) {
    ifThen.bceSafeOnExit &= bceSafe_;
  }

  deadCode_ = ifThen.deadOnArrival;
  if (!deadCode_) {
    if (!pushBlockResults(type)) {
      return false;
    }
  }

  bceSafe_ = ifThen.bceSafeOnExit & ifThen.bceSafeOnEntry;

  return true;
}

bool BaseCompiler::emitElse() {
  ResultType params, results;
  BaseNothingVector unused_thenValues{};

  if (!iter_.readElse(&params, &results, &unused_thenValues)) {
    return false;
  }

  Control& ifThenElse = controlItem(0);

  // See comment in endIfThenElse, below.

  // Exit the "then" branch.

  ifThenElse.deadThenBranch = deadCode_;

  if (deadCode_) {
    fr.resetStackHeight(ifThenElse.stackHeight, results);
    popValueStackTo(ifThenElse.stackSize);
  } else {
    MOZ_ASSERT(stk_.length() == ifThenElse.stackSize + results.length());
    popBlockResults(results, ifThenElse.stackHeight, ContinuationKind::Jump);
    freeResultRegisters(results);
    MOZ_ASSERT(!ifThenElse.deadOnArrival);
  }

  if (!deadCode_) {
    masm.jump(&ifThenElse.label);
  }

  if (ifThenElse.otherLabel.used()) {
    masm.bind(&ifThenElse.otherLabel);
  }

  // Reset to the "else" branch.

  if (!deadCode_) {
    ifThenElse.bceSafeOnExit &= bceSafe_;
  }

  deadCode_ = ifThenElse.deadOnArrival;
  bceSafe_ = ifThenElse.bceSafeOnEntry;

  fr.resetStackHeight(ifThenElse.stackHeight, params);

  if (!deadCode_) {
    captureResultRegisters(params);
    if (!pushBlockResults(params)) {
      return false;
    }
  }

  return true;
}

bool BaseCompiler::endIfThenElse(ResultType type) {
  Control& ifThenElse = controlItem();

  // The expression type is not a reliable guide to what we'll find
  // on the stack, we could have (if E (i32.const 1) (unreachable))
  // in which case the "else" arm is AnyType but the type of the
  // full expression is I32.  So restore whatever's there, not what
  // we want to find there.  The "then" arm has the same constraint.

  if (deadCode_) {
    // "then" arm does not fall through; reset stack.
    fr.resetStackHeight(ifThenElse.stackHeight, type);
    popValueStackTo(ifThenElse.stackSize);
  } else {
    MOZ_ASSERT(stk_.length() == ifThenElse.stackSize + type.length());
    // Assume we have a control join, so place results in block result
    // allocations.
    popBlockResults(type, ifThenElse.stackHeight,
                    ContinuationKind::Fallthrough);
    ifThenElse.bceSafeOnExit &= bceSafe_;
    MOZ_ASSERT(!ifThenElse.deadOnArrival);
  }

  if (ifThenElse.label.used()) {
    masm.bind(&ifThenElse.label);
  }

  bool joinLive =
      !ifThenElse.deadOnArrival &&
      (!ifThenElse.deadThenBranch || !deadCode_ || ifThenElse.label.bound());

  if (joinLive) {
    // No values were provided by the "then" path, but capture the values
    // provided by the "else" path.
    if (deadCode_) {
      captureResultRegisters(type);
    }
    deadCode_ = false;
  }

  bceSafe_ = ifThenElse.bceSafeOnExit;

  if (!deadCode_) {
    if (!pushBlockResults(type)) {
      return false;
    }
  }

  return true;
}

bool BaseCompiler::emitEnd() {
  LabelKind kind;
  ResultType type;
  BaseNothingVector unused_values{};
  if (!iter_.readEnd(&kind, &type, &unused_values, &unused_values)) {
    return false;
  }

  switch (kind) {
    case LabelKind::Body:
      if (!endBlock(type)) {
        return false;
      }
      doReturn(ContinuationKind::Fallthrough);
      // This is emitted here after `doReturn` to avoid being executed in the
      // normal return path of a function, and instead only when a `delegate`
      // jumps to it.
#ifdef ENABLE_WASM_EXCEPTIONS
      if (!emitBodyDelegateThrowPad()) {
        return false;
      }
#endif
      iter_.popEnd();
      MOZ_ASSERT(iter_.controlStackEmpty());
      return iter_.endFunction(iter_.end());
    case LabelKind::Block:
      if (!endBlock(type)) {
        return false;
      }
      break;
    case LabelKind::Loop:
      // The end of a loop isn't a branch target, so we can just leave its
      // results on the expression stack to be consumed by the outer block.
      break;
    case LabelKind::Then:
      if (!endIfThen(type)) {
        return false;
      }
      break;
    case LabelKind::Else:
      if (!endIfThenElse(type)) {
        return false;
      }
      break;
#ifdef ENABLE_WASM_EXCEPTIONS
    case LabelKind::Try:
    case LabelKind::Catch:
    case LabelKind::CatchAll:
      if (!endTryCatch(type)) {
        return false;
      }
      break;
#endif
  }

  iter_.popEnd();

  return true;
}

bool BaseCompiler::emitBr() {
  uint32_t relativeDepth;
  ResultType type;
  BaseNothingVector unused_values{};
  if (!iter_.readBr(&relativeDepth, &type, &unused_values)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  Control& target = controlItem(relativeDepth);
  target.bceSafeOnExit &= bceSafe_;

  // Save any values in the designated join registers, as if the target block
  // returned normally.

  popBlockResults(type, target.stackHeight, ContinuationKind::Jump);
  masm.jump(&target.label);

  // The registers holding the join values are free for the remainder of this
  // block.

  freeResultRegisters(type);

  deadCode_ = true;

  return true;
}

bool BaseCompiler::emitBrIf() {
  uint32_t relativeDepth;
  ResultType type;
  BaseNothingVector unused_values{};
  Nothing unused_condition;
  if (!iter_.readBrIf(&relativeDepth, &type, &unused_values,
                      &unused_condition)) {
    return false;
  }

  if (deadCode_) {
    resetLatentOp();
    return true;
  }

  Control& target = controlItem(relativeDepth);
  target.bceSafeOnExit &= bceSafe_;

  BranchState b(&target.label, target.stackHeight, InvertBranch(false), type);
  emitBranchSetup(&b);
  return emitBranchPerform(&b);
}

#ifdef ENABLE_WASM_FUNCTION_REFERENCES
bool BaseCompiler::emitBrOnNull() {
  MOZ_ASSERT(!hasLatentOp());

  uint32_t relativeDepth;
  ResultType type;
  BaseNothingVector unused_values{};
  Nothing unused_condition;
  if (!iter_.readBrOnNull(&relativeDepth, &type, &unused_values,
                          &unused_condition)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  Control& target = controlItem(relativeDepth);
  target.bceSafeOnExit &= bceSafe_;

  BranchState b(&target.label, target.stackHeight, InvertBranch(false), type);
  if (b.hasBlockResults()) {
    needResultRegisters(b.resultType);
  }
  RegRef rp = popRef();
  if (b.hasBlockResults()) {
    freeResultRegisters(b.resultType);
  }
  if (!jumpConditionalWithResults(&b, Assembler::Equal, rp,
                                  ImmWord(NULLREF_VALUE))) {
    return false;
  }
  pushRef(rp);

  return true;
}
#endif

bool BaseCompiler::emitBrTable() {
  Uint32Vector depths;
  uint32_t defaultDepth;
  ResultType branchParams;
  BaseNothingVector unused_values{};
  Nothing unused_index;
  // N.B., `branchParams' gets set to the type of the default branch target.  In
  // the presence of subtyping, it could be that the different branch targets
  // have different types.  Here we rely on the assumption that the value
  // representations (e.g. Stk value types) of all branch target types are the
  // same, in the baseline compiler.  Notably, this means that all Ref types
  // should be represented the same.
  if (!iter_.readBrTable(&depths, &defaultDepth, &branchParams, &unused_values,
                         &unused_index)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  // Don't use param registers for rc
  needIntegerResultRegisters(branchParams);

  // Table switch value always on top.
  RegI32 rc = popI32();

  freeIntegerResultRegisters(branchParams);

  StackHeight resultsBase(0);
  if (!topBranchParams(branchParams, &resultsBase)) {
    return false;
  }

  Label dispatchCode;
  masm.branch32(Assembler::Below, rc, Imm32(depths.length()), &dispatchCode);

  // This is the out-of-range stub.  rc is dead here but we don't need it.

  shuffleStackResultsBeforeBranch(
      resultsBase, controlItem(defaultDepth).stackHeight, branchParams);
  controlItem(defaultDepth).bceSafeOnExit &= bceSafe_;
  masm.jump(&controlItem(defaultDepth).label);

  // Emit stubs.  rc is dead in all of these but we don't need it.
  //
  // The labels in the vector are in the TempAllocator and will
  // be freed by and by.
  //
  // TODO / OPTIMIZE (Bug 1316804): Branch directly to the case code if we
  // can, don't emit an intermediate stub.

  LabelVector stubs;
  if (!stubs.reserve(depths.length())) {
    return false;
  }

  for (uint32_t depth : depths) {
    stubs.infallibleEmplaceBack(NonAssertingLabel());
    masm.bind(&stubs.back());
    shuffleStackResultsBeforeBranch(resultsBase, controlItem(depth).stackHeight,
                                    branchParams);
    controlItem(depth).bceSafeOnExit &= bceSafe_;
    masm.jump(&controlItem(depth).label);
  }

  // Emit table.

  Label theTable;
  jumpTable(stubs, &theTable);

  // Emit indirect jump.  rc is live here.

  tableSwitch(&theTable, rc, &dispatchCode);

  deadCode_ = true;

  // Clean up.

  freeI32(rc);
  popValueStackBy(branchParams.length());

  return true;
}

#ifdef ENABLE_WASM_EXCEPTIONS
bool BaseCompiler::emitTry() {
  ResultType params;
  if (!iter_.readTry(&params)) {
    return false;
  }

  if (!deadCode_) {
    // Simplifies jumping out, but it is also necessary so that control
    // can re-enter the catch handler without restoring registers.
    sync();
  }

  initControl(controlItem(), params);

  if (!deadCode_) {
    // Be conservative for BCE due to complex control flow in try blocks.
    controlItem().bceSafeOnExit = 0;
    // Mark the beginning of the try block, the rest is filled in by catch.
    controlItem().tryNoteIndex = masm.wasmStartTry();
  }

  return true;
}

void BaseCompiler::emitCatchSetup(LabelKind kind, Control& tryCatch,
                                  const ResultType& resultType) {
  // Catch ends the try or last catch, so we finish this like endIfThen.
  if (deadCode_) {
    fr.resetStackHeight(tryCatch.stackHeight, resultType);
    popValueStackTo(tryCatch.stackSize);
  } else {
    // If the previous block is a catch, we need to handle the extra exception
    // reference on the stack (for rethrow) and thus the stack size is 1 more.
    MOZ_ASSERT(stk_.length() == tryCatch.stackSize + resultType.length() +
                                    (kind == LabelKind::Try ? 0 : 1));
    // Try jumps to the end of the try-catch block unless a throw is done.
    if (kind == LabelKind::Try) {
      popBlockResults(resultType, tryCatch.stackHeight, ContinuationKind::Jump);
    } else {
      popCatchResults(resultType, tryCatch.stackHeight);
    }
    MOZ_ASSERT(stk_.length() == tryCatch.stackSize);
    freeResultRegisters(resultType);
    MOZ_ASSERT(!tryCatch.deadOnArrival);
  }

  // Reset to this "catch" branch.
  deadCode_ = tryCatch.deadOnArrival;

  // We use the empty result type here because catch does *not* take the
  // try-catch block parameters.
  fr.resetStackHeight(tryCatch.stackHeight, ResultType::Empty());

  if (deadCode_) {
    return;
  }

  bceSafe_ = 0;

  // The end of the previous try/catch jumps to the join point.
  masm.jump(&tryCatch.label);

  // Note end of try block for finding the catch block target. This needs
  // to happen after the stack is reset to the correct height.
  if (kind == LabelKind::Try) {
    WasmTryNoteVector& tryNotes = masm.tryNotes();
    WasmTryNote& tryNote = tryNotes[controlItem().tryNoteIndex];
    tryNote.end = masm.currentOffset();
  }
}

bool BaseCompiler::emitCatch() {
  LabelKind kind;
  uint32_t tagIndex;
  ResultType paramType, resultType;
  BaseNothingVector unused_tryValues{};

  if (!iter_.readCatch(&kind, &tagIndex, &paramType, &resultType,
                       &unused_tryValues)) {
    return false;
  }

  Control& tryCatch = controlItem();

  emitCatchSetup(kind, tryCatch, resultType);

  if (deadCode_) {
    return true;
  }

  // Construct info used for the exception landing pad.
  CatchInfo catchInfo(tagIndex);
  if (!tryCatch.catchInfos.emplaceBack(catchInfo)) {
    return false;
  }

  masm.bind(&tryCatch.catchInfos.back().label);

  // Extract the arguments in the exception package and push them.
  const TagType& tagType = moduleEnv_.tags[tagIndex].type;
  const ValTypeVector& params = tagType.argTypes;
  const TagOffsetVector& offsets = tagType.argOffsets;

  const uint32_t dataOffset =
      NativeObject::getFixedSlotOffset(ArrayBufferObject::DATA_SLOT);

  // The landing pad uses the block return protocol to communicate the
  // exception object pointer to the catch block.
  ResultType exnResult = ResultType::Single(RefType::extern_());
  captureResultRegisters(exnResult);
  if (!pushBlockResults(exnResult)) {
    return false;
  }
  RegRef exn = popRef();
  RegRef values = needRef();
  RegRef refs = needRef();

  masm.unboxObject(Address(exn, WasmExceptionObject::offsetOfValues()), values);
  masm.unboxObject(Address(exn, WasmExceptionObject::offsetOfRefs()), refs);

#  ifdef DEBUG
  Label ok;
  RegI32 scratch = needI32();
  masm.load32(Address(refs, NativeObject::offsetOfFixedElements() +
                                ObjectElements::offsetOfLength()),
              scratch);
  masm.branch32(Assembler::Equal, scratch, Imm32(tagType.refCount), &ok);
  masm.assumeUnreachable("Array length should be equal to exn ref count.");
  masm.bind(&ok);
  freeI32(scratch);
#  endif
  masm.loadPtr(Address(refs, NativeObject::offsetOfElements()), refs);

  // This reference is pushed onto the stack because a potential rethrow
  // may need to access it. It is always popped at the end of the block.
  pushRef(exn);

  masm.loadPtr(Address(values, dataOffset), values);
  for (uint32_t i = 0; i < params.length(); i++) {
    int32_t offset = offsets[i];
    switch (params[i].kind()) {
      case ValType::I32: {
        RegI32 reg = needI32();
        masm.load32(Address(values, offset), reg);
        pushI32(reg);
        break;
      }
      case ValType::I64: {
        RegI64 reg = needI64();
        masm.load64(Address(values, offset), reg);
        pushI64(reg);
        break;
      }
      case ValType::F32: {
        RegF32 reg = needF32();
        masm.loadFloat32(Address(values, offset), reg);
        pushF32(reg);
        break;
      }
      case ValType::F64: {
        RegF64 reg = needF64();
        masm.loadDouble(Address(values, offset), reg);
        pushF64(reg);
        break;
      }
      case ValType::V128: {
#  ifdef ENABLE_WASM_SIMD
        RegV128 reg = needV128();
        masm.loadUnalignedSimd128(Address(values, offset), reg);
        pushV128(reg);
        break;
#  else
        MOZ_CRASH("No SIMD support");
#  endif
      }
      case ValType::Rtt:
      case ValType::Ref: {
        // TODO/AnyRef-boxing: With boxed immediates and strings, this may need
        // to handle other kinds of values.
        ASSERT_ANYREF_IS_JSOBJECT;
        RegRef reg = needRef();
        masm.unboxObjectOrNull(Address(refs, offset), reg);
        pushRef(reg);
        break;
      }
    }
  }
  freeRef(values);
  freeRef(refs);

  return true;
}

bool BaseCompiler::emitCatchAll() {
  LabelKind kind;
  ResultType paramType, resultType;
  BaseNothingVector unused_tryValues{};

  if (!iter_.readCatchAll(&kind, &paramType, &resultType, &unused_tryValues)) {
    return false;
  }

  Control& tryCatch = controlItem();

  emitCatchSetup(kind, tryCatch, resultType);

  if (deadCode_) {
    return true;
  }

  CatchInfo catchInfo(CatchInfo::CATCH_ALL_INDEX);
  if (!tryCatch.catchInfos.emplaceBack(catchInfo)) {
    return false;
  }

  masm.bind(&tryCatch.catchInfos.back().label);

  // The landing pad uses the block return protocol to communicate the
  // exception object pointer to the catch block.
  ResultType exnResult = ResultType::Single(RefType::extern_());
  captureResultRegisters(exnResult);
  // This reference is pushed onto the stack because a potential rethrow
  // may need to access it. It is always popped at the end of the block.
  if (!pushBlockResults(exnResult)) {
    return false;
  }

  return true;
}

bool BaseCompiler::emitBodyDelegateThrowPad() {
  Control& block = controlItem();

  // Only emit a landing pad if a `delegate` has generated a jump to here.
  if (block.otherLabel.used()) {
    uint32_t lineOrBytecode = readCallSiteLineOrBytecode();
    StackHeight savedHeight = fr.stackHeight();
    fr.setStackHeight(block.stackHeight);
    masm.bind(&block.otherLabel);

    // Try-delegate does not restore the TlsData on throw, so it needs to be
    // done here as is done in endTryCatch().
    fr.loadTlsPtr(WasmTlsReg);
    masm.loadWasmPinnedRegsFromTls();
    RegRef scratch = needRef();
    RegRef scratch2 = needRef();
    masm.switchToWasmTlsRealm(scratch, scratch2);
    freeRef(scratch);
    freeRef(scratch2);

    // Try-delegate keeps the pending exception in the TlsData, so we extract
    // it here rather than relying on an ABI register.
    RegRef exn = needRef();
    loadPendingException(exn);
    pushRef(exn);

    // Called only to clear the pending exception, the result is not used.
    if (!emitInstanceCall(lineOrBytecode, SASigConsumePendingException)) {
      return false;
    }
    freeI32(popI32());
    exn = popRef();

    if (!throwFrom(exn, lineOrBytecode)) {
      return false;
    }
    fr.setStackHeight(savedHeight);
  }

  return true;
}

bool BaseCompiler::emitDelegate() {
  uint32_t relativeDepth;
  ResultType resultType;
  BaseNothingVector unused_tryValues{};

  if (!iter_.readDelegate(&relativeDepth, &resultType, &unused_tryValues)) {
    return false;
  }

  Control& tryDelegate = controlItem();

  // End the try branch like a plain catch block without exception ref handling.
  if (deadCode_) {
    fr.resetStackHeight(tryDelegate.stackHeight, resultType);
    popValueStackTo(tryDelegate.stackSize);
  } else {
    MOZ_ASSERT(stk_.length() == tryDelegate.stackSize + resultType.length());
    popBlockResults(resultType, tryDelegate.stackHeight,
                    ContinuationKind::Jump);
    freeResultRegisters(resultType);
    masm.jump(&tryDelegate.label);
    MOZ_ASSERT(!tryDelegate.deadOnArrival);
  }

  deadCode_ = tryDelegate.deadOnArrival;

  if (deadCode_) {
    return true;
  }

  // Create an exception landing pad that immediately branches to the landing
  // pad of the delegated try block.
  masm.bind(&tryDelegate.otherLabel);

  StackHeight savedHeight = fr.stackHeight();
  fr.setStackHeight(tryDelegate.stackHeight);

  WasmTryNoteVector& tryNotes = masm.tryNotes();
  WasmTryNote& tryNote = tryNotes[controlItem().tryNoteIndex];
  tryNote.end = masm.currentOffset();
  tryNote.entryPoint = tryNote.end;
  tryNote.framePushed = masm.framePushed();

  // If the target block is a non-try block, skip over it and find the next
  // try block or the very last block (to re-throw out of the function).
  Control& lastBlock = controlOutermost();
  while (controlKind(relativeDepth) != LabelKind::Try &&
         &controlItem(relativeDepth) != &lastBlock) {
    relativeDepth++;
  }
  Control& target = controlItem(relativeDepth);

  popBlockResults(ResultType::Empty(), target.stackHeight,
                  ContinuationKind::Jump);
  masm.jump(&target.otherLabel);

  fr.setStackHeight(savedHeight);

  // Where the try branch jumps to, if it's not dead.
  if (tryDelegate.label.used()) {
    masm.bind(&tryDelegate.label);
  }

  captureResultRegisters(resultType);
  bceSafe_ = tryDelegate.bceSafeOnExit;

  return pushBlockResults(resultType);
}

bool BaseCompiler::endTryCatch(ResultType type) {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  Control& tryCatch = controlItem();
  LabelKind tryKind = controlKind(0);

  if (deadCode_) {
    fr.resetStackHeight(tryCatch.stackHeight, type);
    popValueStackTo(tryCatch.stackSize);
  } else {
    // If the previous block is a catch, we must handle the extra exception
    // reference on the stack (for rethrow) and thus the stack size is 1 more.
    MOZ_ASSERT(stk_.length() == tryCatch.stackSize + type.length() +
                                    (tryKind == LabelKind::Try ? 0 : 1));
    // Assume we have a control join, so place results in block result
    // allocations and also handle the implicit exception reference if needed.
    if (tryKind == LabelKind::Try) {
      popBlockResults(type, tryCatch.stackHeight, ContinuationKind::Jump);
    } else {
      popCatchResults(type, tryCatch.stackHeight);
    }
    MOZ_ASSERT(stk_.length() == tryCatch.stackSize);
    // Since we will emit a landing pad after this and jump over it to get to
    // the control join, we free these here and re-capture at the join.
    freeResultRegisters(type);
    masm.jump(&tryCatch.label);
    MOZ_ASSERT(!tryCatch.bceSafeOnExit);
    MOZ_ASSERT(!tryCatch.deadOnArrival);
  }

  deadCode_ = tryCatch.deadOnArrival;

  if (deadCode_) {
    return true;
  }

  // Create landing pad for all catch handlers in this block.
  // When used for a catchless try block, this will generate a landing pad
  // with no handlers and only the fall-back rethrow.
  masm.bind(&tryCatch.otherLabel);

  // The stack height also needs to be set not for a block result, but for the
  // entry to the exception handlers. This is reset again below for the join.
  StackHeight prePadHeight = fr.stackHeight();
  fr.setStackHeight(tryCatch.stackHeight);

  WasmTryNoteVector& tryNotes = masm.tryNotes();
  WasmTryNote& tryNote = tryNotes[controlItem().tryNoteIndex];
  tryNote.entryPoint = masm.currentOffset();
  tryNote.framePushed = masm.framePushed();

  // If we are in a catchless try block, then there were no catch blocks to
  // mark the end of the try note, so we need to end it here.
  if (tryKind == LabelKind::Try) {
    tryNote.end = tryNote.entryPoint;
  }

  // Explicitly restore the TlsData in case the throw was across instances.
  fr.loadTlsPtr(WasmTlsReg);
  masm.loadWasmPinnedRegsFromTls();
  RegRef scratch = needRef();
  RegRef scratch2 = needRef();
  masm.switchToWasmTlsRealm(scratch, scratch2);
  freeRef(scratch);
  freeRef(scratch2);

  // Load exception pointer from TlsData and make sure that it is
  // saved before the following call will clear it.
  RegRef exn = needRef();
  loadPendingException(exn);
  pushRef(exn);

  if (!emitInstanceCall(lineOrBytecode, SASigConsumePendingException)) {
    return false;
  }

  // Prevent conflict with exn register when popping this result.
  RegI32 temp = popI32();
  RegI32 index = needI32();
  moveI32(temp, index);
  freeI32(temp);

  // Ensure that the exception is assigned to the block return register
  // before branching to a handler.
  ResultType exnResult = ResultType::Single(RefType::extern_());
  popBlockResults(exnResult, tryCatch.stackHeight, ContinuationKind::Jump);
  freeResultRegisters(exnResult);

  bool hasCatchAll = false;
  for (CatchInfo& info : tryCatch.catchInfos) {
    if (info.tagIndex != CatchInfo::CATCH_ALL_INDEX) {
      MOZ_ASSERT(!hasCatchAll);
      masm.branch32(Assembler::Equal, index, Imm32(info.tagIndex), &info.label);
    } else {
      masm.jump(&info.label);
      hasCatchAll = true;
    }
  }
  freeI32(index);

  // If none of the tag checks succeed and there is no catch_all,
  // then we rethrow the exception.
  if (!hasCatchAll) {
    captureResultRegisters(exnResult);
    if (!pushBlockResults(exnResult) || !throwFrom(popRef(), lineOrBytecode)) {
      return false;
    }
  }

  // Reset stack height for join.
  fr.setStackHeight(prePadHeight);

  // Create join point.
  if (tryCatch.label.used()) {
    masm.bind(&tryCatch.label);
  }

  captureResultRegisters(type);
  deadCode_ = tryCatch.deadOnArrival;
  bceSafe_ = tryCatch.bceSafeOnExit;

  return pushBlockResults(type);
}

bool BaseCompiler::emitThrow() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();
  uint32_t exnIndex;
  BaseNothingVector unused_argValues{};

  if (!iter_.readThrow(&exnIndex, &unused_argValues)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  const TagDesc& tagDesc = moduleEnv_.tags[exnIndex];
  const ResultType& params = tagDesc.resultType();
  const TagOffsetVector& offsets = tagDesc.type.argOffsets;

  // Create the new exception object that we will throw.
  pushI32(exnIndex);
  pushI32(tagDesc.type.bufferSize);
  if (!emitInstanceCall(lineOrBytecode, SASigExceptionNew)) {
    return false;
  }

  RegRef exn = popRef();
  // Create scratch register, to store the exception package values.
  RegRef scratch = needRef();
  const uint32_t dataOffset =
      NativeObject::getFixedSlotOffset(ArrayBufferObject::DATA_SLOT);

  Address exnValuesAddress(exn, WasmExceptionObject::offsetOfValues());
  masm.unboxObject(exnValuesAddress, scratch);
  masm.loadPtr(Address(scratch, dataOffset), scratch);

  for (int32_t i = params.length() - 1; i >= 0; i--) {
    int32_t offset = offsets[i];
    switch (params[i].kind()) {
      case ValType::I32: {
        RegI32 reg = popI32();
        masm.store32(reg, Address(scratch, offset));
        freeI32(reg);
        break;
      }
      case ValType::I64: {
        RegI64 reg = popI64();
        masm.store64(reg, Address(scratch, offset));
        freeI64(reg);
        break;
      }
      case ValType::F32: {
        RegF32 reg = popF32();
        masm.storeFloat32(reg, Address(scratch, offset));
        freeF32(reg);
        break;
      }
      case ValType::F64: {
        RegF64 reg = popF64();
        masm.storeDouble(reg, Address(scratch, offset));
        freeF64(reg);
        break;
      }
      case ValType::V128: {
#  ifdef ENABLE_WASM_SIMD
        RegV128 reg = popV128();
        masm.storeUnalignedSimd128(reg, Address(scratch, offset));
        freeV128(reg);
        break;
#  else
        MOZ_CRASH("No SIMD support");
#  endif
      }
      case ValType::Rtt:
      case ValType::Ref: {
        RegRef refArg = popRef();

        // Keep exn on the stack to preserve it across the call.
        RegRef tmp = needRef();
        moveRef(exn, tmp);
        pushRef(tmp);

        // Arguments to the instance call start here.
        pushRef(exn);
        pushRef(refArg);

        if (!emitInstanceCall(lineOrBytecode, SASigPushRefIntoExn)) {
          return false;
        }

        // The call result is checked by the instance call failure handling,
        // so we do not need to use the result here.
        freeI32(popI32());

        exn = popRef();

        // Restore scratch register contents that got clobbered.
        masm.unboxObject(exnValuesAddress, scratch);
        masm.loadPtr(Address(scratch, dataOffset), scratch);
        break;
      }
    }
  }
  freeRef(scratch);

  deadCode_ = true;

  return throwFrom(exn, lineOrBytecode);
}

bool BaseCompiler::emitRethrow() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  uint32_t relativeDepth;
  if (!iter_.readRethrow(&relativeDepth)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  Control& tryCatch = controlItem(relativeDepth);
  RegRef exn = needRef();
  peekRefAt(tryCatch.stackSize, exn);

  deadCode_ = true;

  return throwFrom(exn, lineOrBytecode);
}
#endif

bool BaseCompiler::emitDrop() {
  if (!iter_.readDrop()) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  dropValue();
  return true;
}

void BaseCompiler::doReturn(ContinuationKind kind) {
  if (deadCode_) {
    return;
  }

  StackHeight height = controlOutermost().stackHeight;
  ResultType type = ResultType::Vector(funcType().results());
  popBlockResults(type, height, kind);
  masm.jump(&returnLabel_);
  freeResultRegisters(type);
}

bool BaseCompiler::emitReturn() {
  BaseNothingVector unused_values{};
  if (!iter_.readReturn(&unused_values)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  doReturn(ContinuationKind::Jump);
  deadCode_ = true;

  return true;
}

bool BaseCompiler::emitCallArgs(const ValTypeVector& argTypes,
                                const StackResultsLoc& results,
                                FunctionCall* baselineCall,
                                CalleeOnStack calleeOnStack) {
  MOZ_ASSERT(!deadCode_);

  ArgTypeVector args(argTypes, results.stackResults());
  uint32_t naturalArgCount = argTypes.length();
  uint32_t abiArgCount = args.lengthWithStackResults();
  startCallArgs(StackArgAreaSizeUnaligned(args), baselineCall);

  // Args are deeper on the stack than the stack result area, if any.
  size_t argsDepth = results.count();
  // They're deeper than the callee too, for callIndirect.
  if (calleeOnStack == CalleeOnStack::True) {
    argsDepth++;
  }

  for (size_t i = 0; i < abiArgCount; ++i) {
    if (args.isNaturalArg(i)) {
      size_t naturalIndex = args.naturalIndex(i);
      size_t stackIndex = naturalArgCount - 1 - naturalIndex + argsDepth;
      passArg(argTypes[naturalIndex], peek(stackIndex), baselineCall);
    } else {
      // The synthetic stack result area pointer.
      ABIArg argLoc = baselineCall->abi.next(MIRType::Pointer);
      if (argLoc.kind() == ABIArg::Stack) {
        ScratchPtr scratch(*this);
        fr.computeOutgoingStackResultAreaPtr(results, scratch);
        masm.storePtr(scratch, Address(masm.getStackPointer(),
                                       argLoc.offsetFromArgBase()));
      } else {
        fr.computeOutgoingStackResultAreaPtr(results, RegPtr(argLoc.gpr()));
      }
    }
  }

  fr.loadTlsPtr(WasmTlsReg);
  return true;
}

void BaseCompiler::pushReturnValueOfCall(const FunctionCall& call,
                                         MIRType type) {
  switch (type) {
    case MIRType::Int32: {
      RegI32 rv = captureReturnedI32();
      pushI32(rv);
      break;
    }
    case MIRType::Int64: {
      RegI64 rv = captureReturnedI64();
      pushI64(rv);
      break;
    }
    case MIRType::Float32: {
      RegF32 rv = captureReturnedF32(call);
      pushF32(rv);
      break;
    }
    case MIRType::Double: {
      RegF64 rv = captureReturnedF64(call);
      pushF64(rv);
      break;
    }
#ifdef ENABLE_WASM_SIMD
    case MIRType::Simd128: {
      RegV128 rv = captureReturnedV128(call);
      pushV128(rv);
      break;
    }
#endif
    case MIRType::RefOrNull: {
      RegRef rv = captureReturnedRef();
      pushRef(rv);
      break;
    }
    default:
      // In particular, passing |type| as MIRType::Void or MIRType::Pointer to
      // this function is an error.
      MOZ_CRASH("Function return type");
  }
}

bool BaseCompiler::pushStackResultsForCall(const ResultType& type, RegPtr temp,
                                           StackResultsLoc* loc) {
  if (!ABIResultIter::HasStackResults(type)) {
    return true;
  }

  // This method is the only one in the class that can increase stk_.length() by
  // an unbounded amount, so it's the only one that requires an allocation.
  // (The general case is handled in emitBody.)
  if (!stk_.reserve(stk_.length() + type.length())) {
    return false;
  }

  // Measure stack results.
  ABIResultIter i(type);
  size_t count = 0;
  for (; !i.done(); i.next()) {
    if (i.cur().onStack()) {
      count++;
    }
  }
  uint32_t bytes = i.stackBytesConsumedSoFar();

  // Reserve space for the stack results.
  StackHeight resultsBase = fr.stackHeight();
  uint32_t height = fr.prepareStackResultArea(resultsBase, bytes);

  // Push Stk values onto the value stack, and zero out Ref values.
  for (i.switchToPrev(); !i.done(); i.prev()) {
    const ABIResult& result = i.cur();
    if (result.onStack()) {
      Stk v = captureStackResult(result, resultsBase, bytes);
      push(v);
      if (v.kind() == Stk::MemRef) {
        stackMapGenerator_.memRefsOnStk++;
        fr.storeImmediatePtrToStack(intptr_t(0), v.offs(), temp);
      }
    }
  }

  *loc = StackResultsLoc(bytes, count, height);

  return true;
}

// After a call, some results may be written to the stack result locations that
// are pushed on the machine stack after any stack args.  If there are stack
// args and stack results, these results need to be shuffled down, as the args
// are "consumed" by the call.
void BaseCompiler::popStackResultsAfterCall(const StackResultsLoc& results,
                                            uint32_t stackArgBytes) {
  if (results.bytes() != 0) {
    popValueStackBy(results.count());
    if (stackArgBytes != 0) {
      uint32_t srcHeight = results.height();
      MOZ_ASSERT(srcHeight >= stackArgBytes + results.bytes());
      uint32_t destHeight = srcHeight - stackArgBytes;

      fr.shuffleStackResultsTowardFP(srcHeight, destHeight, results.bytes(),
                                     ABINonArgReturnVolatileReg);
    }
  }
}

// For now, always sync() at the beginning of the call to easily save live
// values.
//
// TODO / OPTIMIZE (Bug 1316806): We may be able to avoid a full sync(), since
// all we want is to save live registers that won't be saved by the callee or
// that we need for outgoing args - we don't need to sync the locals.  We can
// just push the necessary registers, it'll be like a lightweight sync.
//
// Even some of the pushing may be unnecessary if the registers will be consumed
// by the call, because then what we want is parallel assignment to the argument
// registers or onto the stack for outgoing arguments.  A sync() is just
// simpler.

bool BaseCompiler::emitCall() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  uint32_t funcIndex;
  BaseNothingVector args_{};
  if (!iter_.readCall(&funcIndex, &args_)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  sync();

  const FuncType& funcType = *moduleEnv_.funcs[funcIndex].type;
  bool import = moduleEnv_.funcIsImport(funcIndex);

  uint32_t numArgs = funcType.args().length();
  size_t stackArgBytes = stackConsumed(numArgs);

  ResultType resultType(ResultType::Vector(funcType.results()));
  StackResultsLoc results;
  if (!pushStackResultsForCall(resultType, RegPtr(ABINonArgReg0), &results)) {
    return false;
  }

  FunctionCall baselineCall(lineOrBytecode);
  beginCall(baselineCall, UseABI::Wasm,
            import ? InterModule::True : InterModule::False);

  if (!emitCallArgs(funcType.args(), results, &baselineCall,
                    CalleeOnStack::False)) {
    return false;
  }

  CodeOffset raOffset;
  if (import) {
    raOffset = callImport(moduleEnv_.funcImportGlobalDataOffsets[funcIndex],
                          baselineCall);
  } else {
    raOffset = callDefinition(funcIndex, baselineCall);
  }

  if (!createStackMap("emitCall", raOffset)) {
    return false;
  }

  popStackResultsAfterCall(results, stackArgBytes);

  endCall(baselineCall, stackArgBytes);

  popValueStackBy(numArgs);

  captureCallResultRegisters(resultType);
  return pushCallResults(baselineCall, resultType, results);
}

bool BaseCompiler::emitCallIndirect() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  uint32_t funcTypeIndex;
  uint32_t tableIndex;
  Nothing callee_;
  BaseNothingVector args_{};
  if (!iter_.readCallIndirect(&funcTypeIndex, &tableIndex, &callee_, &args_)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  sync();

  const FuncType& funcType = (*moduleEnv_.types)[funcTypeIndex].funcType();

  // Stack: ... arg1 .. argn callee

  uint32_t numArgs = funcType.args().length() + 1;
  size_t stackArgBytes = stackConsumed(numArgs);

  ResultType resultType(ResultType::Vector(funcType.results()));
  StackResultsLoc results;
  if (!pushStackResultsForCall(resultType, RegPtr(ABINonArgReg0), &results)) {
    return false;
  }

  FunctionCall baselineCall(lineOrBytecode);
  beginCall(baselineCall, UseABI::Wasm, InterModule::True);

  if (!emitCallArgs(funcType.args(), results, &baselineCall,
                    CalleeOnStack::True)) {
    return false;
  }

  const Stk& callee = peek(results.count());
  CodeOffset raOffset =
      callIndirect(funcTypeIndex, tableIndex, callee, baselineCall);
  if (!createStackMap("emitCallIndirect", raOffset)) {
    return false;
  }

  popStackResultsAfterCall(results, stackArgBytes);

  endCall(baselineCall, stackArgBytes);

  popValueStackBy(numArgs);

  captureCallResultRegisters(resultType);
  return pushCallResults(baselineCall, resultType, results);
}

void BaseCompiler::emitRound(RoundingMode roundingMode, ValType operandType) {
  if (operandType == ValType::F32) {
    RegF32 f0 = popF32();
    roundF32(roundingMode, f0);
    pushF32(f0);
  } else if (operandType == ValType::F64) {
    RegF64 f0 = popF64();
    roundF64(roundingMode, f0);
    pushF64(f0);
  } else {
    MOZ_CRASH("unexpected type");
  }
}

bool BaseCompiler::emitUnaryMathBuiltinCall(SymbolicAddress callee,
                                            ValType operandType) {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  Nothing operand_;
  if (!iter_.readUnary(operandType, &operand_)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  RoundingMode roundingMode;
  if (IsRoundingFunction(callee, &roundingMode) &&
      supportsRoundInstruction(roundingMode)) {
    emitRound(roundingMode, operandType);
    return true;
  }

  sync();

  ValTypeVector& signature = operandType == ValType::F32 ? SigF_ : SigD_;
  ValType retType = operandType;
  uint32_t numArgs = signature.length();
  size_t stackSpace = stackConsumed(numArgs);
  StackResultsLoc noStackResults;

  FunctionCall baselineCall(lineOrBytecode);
  beginCall(baselineCall, UseABI::Builtin, InterModule::False);

  if (!emitCallArgs(signature, noStackResults, &baselineCall,
                    CalleeOnStack::False)) {
    return false;
  }

  CodeOffset raOffset = builtinCall(callee, baselineCall);
  if (!createStackMap("emitUnaryMathBuiltin[..]", raOffset)) {
    return false;
  }

  endCall(baselineCall, stackSpace);

  popValueStackBy(numArgs);

  pushReturnValueOfCall(baselineCall, ToMIRType(retType));

  return true;
}

#ifdef RABALDR_INT_DIV_I64_CALLOUT
bool BaseCompiler::emitDivOrModI64BuiltinCall(SymbolicAddress callee,
                                              ValType operandType) {
  MOZ_ASSERT(operandType == ValType::I64);
  MOZ_ASSERT(!deadCode_);

  sync();

  needI64(specific_.abiReturnRegI64);

  RegI64 rhs = popI64();
  RegI64 srcDest = popI64ToSpecific(specific_.abiReturnRegI64);

  Label done;

  checkDivideByZero(rhs);

  if (callee == SymbolicAddress::DivI64) {
    checkDivideSignedOverflow(rhs, srcDest, &done, ZeroOnOverflow(false));
  } else if (callee == SymbolicAddress::ModI64) {
    checkDivideSignedOverflow(rhs, srcDest, &done, ZeroOnOverflow(true));
  }

  masm.setupWasmABICall();
  masm.passABIArg(srcDest.high);
  masm.passABIArg(srcDest.low);
  masm.passABIArg(rhs.high);
  masm.passABIArg(rhs.low);
  CodeOffset raOffset = masm.callWithABI(bytecodeOffset(), callee,
                                         mozilla::Some(fr.getTlsPtrOffset()));
  if (!createStackMap("emitDivOrModI64Bui[..]", raOffset)) {
    return false;
  }

  masm.bind(&done);

  freeI64(rhs);
  pushI64(srcDest);
  return true;
}
#endif  // RABALDR_INT_DIV_I64_CALLOUT

#ifdef RABALDR_I64_TO_FLOAT_CALLOUT
bool BaseCompiler::emitConvertInt64ToFloatingCallout(SymbolicAddress callee,
                                                     ValType operandType,
                                                     ValType resultType) {
  sync();

  RegI64 input = popI64();

  FunctionCall call(0);

  masm.setupWasmABICall();
#  ifdef JS_PUNBOX64
  MOZ_CRASH("BaseCompiler platform hook: emitConvertInt64ToFloatingCallout");
#  else
  masm.passABIArg(input.high);
  masm.passABIArg(input.low);
#  endif
  CodeOffset raOffset = masm.callWithABI(
      bytecodeOffset(), callee, mozilla::Some(fr.getTlsPtrOffset()),
      resultType == ValType::F32 ? MoveOp::FLOAT32 : MoveOp::DOUBLE);
  if (!createStackMap("emitConvertInt64To[..]", raOffset)) {
    return false;
  }

  freeI64(input);

  if (resultType == ValType::F32) {
    pushF32(captureReturnedF32(call));
  } else {
    pushF64(captureReturnedF64(call));
  }

  return true;
}
#endif  // RABALDR_I64_TO_FLOAT_CALLOUT

#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
// `Callee` always takes a double, so a float32 input must be converted.
bool BaseCompiler::emitConvertFloatingToInt64Callout(SymbolicAddress callee,
                                                     ValType operandType,
                                                     ValType resultType) {
  RegF64 doubleInput;
  if (operandType == ValType::F32) {
    doubleInput = needF64();
    RegF32 input = popF32();
    masm.convertFloat32ToDouble(input, doubleInput);
    freeF32(input);
  } else {
    doubleInput = popF64();
  }

  // We may need the value after the call for the ool check.
  RegF64 otherReg = needF64();
  moveF64(doubleInput, otherReg);
  pushF64(otherReg);

  sync();

  FunctionCall call(0);

  masm.setupWasmABICall();
  masm.passABIArg(doubleInput, MoveOp::DOUBLE);
  CodeOffset raOffset = masm.callWithABI(bytecodeOffset(), callee,
                                         mozilla::Some(fr.getTlsPtrOffset()));
  if (!createStackMap("emitConvertFloatin[..]", raOffset)) {
    return false;
  }

  freeF64(doubleInput);

  RegI64 rv = captureReturnedI64();

  RegF64 inputVal = popF64();

  TruncFlags flags = 0;
  if (callee == SymbolicAddress::TruncateDoubleToUint64) {
    flags |= TRUNC_UNSIGNED;
  }
  if (callee == SymbolicAddress::SaturatingTruncateDoubleToInt64 ||
      callee == SymbolicAddress::SaturatingTruncateDoubleToUint64) {
    flags |= TRUNC_SATURATING;
  }

  // If we're saturating, the callout will always produce the final result
  // value. Otherwise, the callout value will return 0x8000000000000000
  // and we need to produce traps.
  OutOfLineCode* ool = nullptr;
  if (!(flags & TRUNC_SATURATING)) {
    // The OOL check just succeeds or fails, it does not generate a value.
    ool = addOutOfLineCode(new (alloc_) OutOfLineTruncateCheckF32OrF64ToI64(
        AnyReg(inputVal), rv, flags, bytecodeOffset()));
    if (!ool) {
      return false;
    }

    masm.branch64(Assembler::Equal, rv, Imm64(0x8000000000000000),
                  ool->entry());
    masm.bind(ool->rejoin());
  }

  pushI64(rv);
  freeF64(inputVal);

  return true;
}
#endif  // RABALDR_FLOAT_TO_I64_CALLOUT

bool BaseCompiler::emitGetLocal() {
  uint32_t slot;
  if (!iter_.readGetLocal(locals_, &slot)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  // Local loads are pushed unresolved, ie, they may be deferred
  // until needed, until they may be affected by a store, or until a
  // sync.  This is intended to reduce register pressure.

  switch (locals_[slot].kind()) {
    case ValType::I32:
      pushLocalI32(slot);
      break;
    case ValType::I64:
      pushLocalI64(slot);
      break;
    case ValType::V128:
#ifdef ENABLE_WASM_SIMD
      pushLocalV128(slot);
      break;
#else
      MOZ_CRASH("No SIMD support");
#endif
    case ValType::F64:
      pushLocalF64(slot);
      break;
    case ValType::F32:
      pushLocalF32(slot);
      break;
    case ValType::Rtt:
    case ValType::Ref:
      pushLocalRef(slot);
      break;
  }

  return true;
}

template <bool isSetLocal>
bool BaseCompiler::emitSetOrTeeLocal(uint32_t slot) {
  if (deadCode_) {
    return true;
  }

  bceLocalIsUpdated(slot);
  switch (locals_[slot].kind()) {
    case ValType::I32: {
      RegI32 rv = popI32();
      syncLocal(slot);
      fr.storeLocalI32(rv, localFromSlot(slot, MIRType::Int32));
      if (isSetLocal) {
        freeI32(rv);
      } else {
        pushI32(rv);
      }
      break;
    }
    case ValType::I64: {
      RegI64 rv = popI64();
      syncLocal(slot);
      fr.storeLocalI64(rv, localFromSlot(slot, MIRType::Int64));
      if (isSetLocal) {
        freeI64(rv);
      } else {
        pushI64(rv);
      }
      break;
    }
    case ValType::F64: {
      RegF64 rv = popF64();
      syncLocal(slot);
      fr.storeLocalF64(rv, localFromSlot(slot, MIRType::Double));
      if (isSetLocal) {
        freeF64(rv);
      } else {
        pushF64(rv);
      }
      break;
    }
    case ValType::F32: {
      RegF32 rv = popF32();
      syncLocal(slot);
      fr.storeLocalF32(rv, localFromSlot(slot, MIRType::Float32));
      if (isSetLocal) {
        freeF32(rv);
      } else {
        pushF32(rv);
      }
      break;
    }
    case ValType::V128: {
#ifdef ENABLE_WASM_SIMD
      RegV128 rv = popV128();
      syncLocal(slot);
      fr.storeLocalV128(rv, localFromSlot(slot, MIRType::Simd128));
      if (isSetLocal) {
        freeV128(rv);
      } else {
        pushV128(rv);
      }
      break;
#else
      MOZ_CRASH("No SIMD support");
#endif
    }
    case ValType::Rtt:
    case ValType::Ref: {
      RegRef rv = popRef();
      syncLocal(slot);
      fr.storeLocalRef(rv, localFromSlot(slot, MIRType::RefOrNull));
      if (isSetLocal) {
        freeRef(rv);
      } else {
        pushRef(rv);
      }
      break;
    }
  }

  return true;
}

bool BaseCompiler::emitSetLocal() {
  uint32_t slot;
  Nothing unused_value;
  if (!iter_.readSetLocal(locals_, &slot, &unused_value)) {
    return false;
  }
  return emitSetOrTeeLocal<true>(slot);
}

bool BaseCompiler::emitTeeLocal() {
  uint32_t slot;
  Nothing unused_value;
  if (!iter_.readTeeLocal(locals_, &slot, &unused_value)) {
    return false;
  }
  return emitSetOrTeeLocal<false>(slot);
}

bool BaseCompiler::emitGetGlobal() {
  uint32_t id;
  if (!iter_.readGetGlobal(&id)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  const GlobalDesc& global = moduleEnv_.globals[id];

  if (global.isConstant()) {
    LitVal value = global.constantValue();
    switch (value.type().kind()) {
      case ValType::I32:
        pushI32(value.i32());
        break;
      case ValType::I64:
        pushI64(value.i64());
        break;
      case ValType::F32:
        pushF32(value.f32());
        break;
      case ValType::F64:
        pushF64(value.f64());
        break;
      case ValType::Ref:
        pushRef(intptr_t(value.ref().forCompiledCode()));
        break;
#ifdef ENABLE_WASM_SIMD
      case ValType::V128:
        pushV128(value.v128());
        break;
#endif
      default:
        MOZ_CRASH("Global constant type");
    }
    return true;
  }

  switch (global.type().kind()) {
    case ValType::I32: {
      RegI32 rv = needI32();
      ScratchI32 tmp(*this);
      masm.load32(addressOfGlobalVar(global, tmp), rv);
      pushI32(rv);
      break;
    }
    case ValType::I64: {
      RegI64 rv = needI64();
      ScratchI32 tmp(*this);
      masm.load64(addressOfGlobalVar(global, tmp), rv);
      pushI64(rv);
      break;
    }
    case ValType::F32: {
      RegF32 rv = needF32();
      ScratchI32 tmp(*this);
      masm.loadFloat32(addressOfGlobalVar(global, tmp), rv);
      pushF32(rv);
      break;
    }
    case ValType::F64: {
      RegF64 rv = needF64();
      ScratchI32 tmp(*this);
      masm.loadDouble(addressOfGlobalVar(global, tmp), rv);
      pushF64(rv);
      break;
    }
    case ValType::Rtt:
    case ValType::Ref: {
      RegRef rv = needRef();
      ScratchI32 tmp(*this);
      masm.loadPtr(addressOfGlobalVar(global, tmp), rv);
      pushRef(rv);
      break;
    }
#ifdef ENABLE_WASM_SIMD
    case ValType::V128: {
      RegV128 rv = needV128();
      ScratchI32 tmp(*this);
      masm.loadUnalignedSimd128(addressOfGlobalVar(global, tmp), rv);
      pushV128(rv);
      break;
    }
#endif
    default:
      MOZ_CRASH("Global variable type");
      break;
  }
  return true;
}

bool BaseCompiler::emitSetGlobal() {
  uint32_t id;
  Nothing unused_value;
  if (!iter_.readSetGlobal(&id, &unused_value)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  const GlobalDesc& global = moduleEnv_.globals[id];

  switch (global.type().kind()) {
    case ValType::I32: {
      RegI32 rv = popI32();
      ScratchI32 tmp(*this);
      masm.store32(rv, addressOfGlobalVar(global, tmp));
      freeI32(rv);
      break;
    }
    case ValType::I64: {
      RegI64 rv = popI64();
      ScratchI32 tmp(*this);
      masm.store64(rv, addressOfGlobalVar(global, tmp));
      freeI64(rv);
      break;
    }
    case ValType::F32: {
      RegF32 rv = popF32();
      ScratchI32 tmp(*this);
      masm.storeFloat32(rv, addressOfGlobalVar(global, tmp));
      freeF32(rv);
      break;
    }
    case ValType::F64: {
      RegF64 rv = popF64();
      ScratchI32 tmp(*this);
      masm.storeDouble(rv, addressOfGlobalVar(global, tmp));
      freeF64(rv);
      break;
    }
    case ValType::Rtt:
    case ValType::Ref: {
      RegPtr valueAddr(PreBarrierReg);
      needPtr(valueAddr);
      {
        ScratchI32 tmp(*this);
        masm.computeEffectiveAddress(addressOfGlobalVar(global, tmp),
                                     valueAddr);
      }
      RegRef rv = popRef();
      // emitBarrieredStore preserves rv
      if (!emitBarrieredStore(Nothing(), valueAddr, rv)) {
        return false;
      }
      freeRef(rv);
      break;
    }
#ifdef ENABLE_WASM_SIMD
    case ValType::V128: {
      RegV128 rv = popV128();
      ScratchI32 tmp(*this);
      masm.storeUnalignedSimd128(rv, addressOfGlobalVar(global, tmp));
      freeV128(rv);
      break;
    }
#endif
    default:
      MOZ_CRASH("Global variable type");
      break;
  }
  return true;
}

bool BaseCompiler::emitLoad(ValType type, Scalar::Type viewType) {
  LinearMemoryAddress<Nothing> addr;
  if (!iter_.readLoad(type, Scalar::byteSize(viewType), &addr)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset());
  loadCommon(&access, AccessCheck(), type);
  return true;
}

bool BaseCompiler::emitStore(ValType resultType, Scalar::Type viewType) {
  LinearMemoryAddress<Nothing> addr;
  Nothing unused_value;
  if (!iter_.readStore(resultType, Scalar::byteSize(viewType), &addr,
                       &unused_value)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset());
  storeCommon(&access, AccessCheck(), resultType);
  return true;
}

bool BaseCompiler::emitSelect(bool typed) {
  StackType type;
  Nothing unused_trueValue;
  Nothing unused_falseValue;
  Nothing unused_condition;
  if (!iter_.readSelect(typed, &type, &unused_trueValue, &unused_falseValue,
                        &unused_condition)) {
    return false;
  }

  if (deadCode_) {
    resetLatentOp();
    return true;
  }

  // I32 condition on top, then false, then true.

  Label done;
  BranchState b(&done);
  emitBranchSetup(&b);

  switch (type.valType().kind()) {
    case ValType::I32: {
      RegI32 r, rs;
      pop2xI32(&r, &rs);
      if (!emitBranchPerform(&b)) {
        return false;
      }
      moveI32(rs, r);
      masm.bind(&done);
      freeI32(rs);
      pushI32(r);
      break;
    }
    case ValType::I64: {
#ifdef JS_CODEGEN_X86
      // There may be as many as four Int64 values in registers at a time: two
      // for the latent branch operands, and two for the true/false values we
      // normally pop before executing the branch.  On x86 this is one value
      // too many, so we need to generate more complicated code here, and for
      // simplicity's sake we do so even if the branch operands are not Int64.
      // However, the resulting control flow diamond is complicated since the
      // arms of the diamond will have to stay synchronized with respect to
      // their evaluation stack and regalloc state.  To simplify further, we
      // use a double branch and a temporary boolean value for now.
      RegI32 temp = needI32();
      moveImm32(0, temp);
      if (!emitBranchPerform(&b)) {
        return false;
      }
      moveImm32(1, temp);
      masm.bind(&done);

      Label trueValue;
      RegI64 r, rs;
      pop2xI64(&r, &rs);
      masm.branch32(Assembler::Equal, temp, Imm32(0), &trueValue);
      moveI64(rs, r);
      masm.bind(&trueValue);
      freeI32(temp);
      freeI64(rs);
      pushI64(r);
#else
      RegI64 r, rs;
      pop2xI64(&r, &rs);
      if (!emitBranchPerform(&b)) {
        return false;
      }
      moveI64(rs, r);
      masm.bind(&done);
      freeI64(rs);
      pushI64(r);
#endif
      break;
    }
    case ValType::F32: {
      RegF32 r, rs;
      pop2xF32(&r, &rs);
      if (!emitBranchPerform(&b)) {
        return false;
      }
      moveF32(rs, r);
      masm.bind(&done);
      freeF32(rs);
      pushF32(r);
      break;
    }
    case ValType::F64: {
      RegF64 r, rs;
      pop2xF64(&r, &rs);
      if (!emitBranchPerform(&b)) {
        return false;
      }
      moveF64(rs, r);
      masm.bind(&done);
      freeF64(rs);
      pushF64(r);
      break;
    }
#ifdef ENABLE_WASM_SIMD
    case ValType::V128: {
      RegV128 r, rs;
      pop2xV128(&r, &rs);
      if (!emitBranchPerform(&b)) {
        return false;
      }
      moveV128(rs, r);
      masm.bind(&done);
      freeV128(rs);
      pushV128(r);
      break;
    }
#endif
    case ValType::Ref: {
      RegRef r, rs;
      pop2xRef(&r, &rs);
      if (!emitBranchPerform(&b)) {
        return false;
      }
      moveRef(rs, r);
      masm.bind(&done);
      freeRef(rs);
      pushRef(r);
      break;
    }
    default: {
      MOZ_CRASH("select type");
    }
  }

  return true;
}

void BaseCompiler::emitCompareI32(Assembler::Condition compareOp,
                                  ValType compareType) {
  MOZ_ASSERT(compareType == ValType::I32);

  if (sniffConditionalControlCmp(compareOp, compareType)) {
    return;
  }

  int32_t c;
  if (popConst(&c)) {
    RegI32 r = popI32();
    masm.cmp32Set(compareOp, r, Imm32(c), r);
    pushI32(r);
  } else {
    RegI32 r, rs;
    pop2xI32(&r, &rs);
    masm.cmp32Set(compareOp, r, rs, r);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitCompareI64(Assembler::Condition compareOp,
                                  ValType compareType) {
  MOZ_ASSERT(compareType == ValType::I64);

  if (sniffConditionalControlCmp(compareOp, compareType)) {
    return;
  }

  RegI64 rs0, rs1;
  pop2xI64(&rs0, &rs1);
  RegI32 rd(fromI64(rs0));
  cmp64Set(compareOp, rs0, rs1, rd);
  freeI64(rs1);
  freeI64Except(rs0, rd);
  pushI32(rd);
}

void BaseCompiler::emitCompareF32(Assembler::DoubleCondition compareOp,
                                  ValType compareType) {
  MOZ_ASSERT(compareType == ValType::F32);

  if (sniffConditionalControlCmp(compareOp, compareType)) {
    return;
  }

  Label across;
  RegF32 rs0, rs1;
  pop2xF32(&rs0, &rs1);
  RegI32 rd = needI32();
  moveImm32(1, rd);
  masm.branchFloat(compareOp, rs0, rs1, &across);
  moveImm32(0, rd);
  masm.bind(&across);
  freeF32(rs0);
  freeF32(rs1);
  pushI32(rd);
}

void BaseCompiler::emitCompareF64(Assembler::DoubleCondition compareOp,
                                  ValType compareType) {
  MOZ_ASSERT(compareType == ValType::F64);

  if (sniffConditionalControlCmp(compareOp, compareType)) {
    return;
  }

  Label across;
  RegF64 rs0, rs1;
  pop2xF64(&rs0, &rs1);
  RegI32 rd = needI32();
  moveImm32(1, rd);
  masm.branchDouble(compareOp, rs0, rs1, &across);
  moveImm32(0, rd);
  masm.bind(&across);
  freeF64(rs0);
  freeF64(rs1);
  pushI32(rd);
}

void BaseCompiler::emitCompareRef(Assembler::Condition compareOp,
                                  ValType compareType) {
  MOZ_ASSERT(!sniffConditionalControlCmp(compareOp, compareType));

  RegRef rs1, rs2;
  pop2xRef(&rs1, &rs2);
  RegI32 rd = needI32();
  masm.cmpPtrSet(compareOp, rs1, rs2, rd);
  freeRef(rs1);
  freeRef(rs2);
  pushI32(rd);
}

bool BaseCompiler::emitInstanceCall(uint32_t lineOrBytecode,
                                    const SymbolicAddressSignature& builtin) {
  const MIRType* argTypes = builtin.argTypes;
  MOZ_ASSERT(argTypes[0] == MIRType::Pointer);

  sync();

  uint32_t numNonInstanceArgs = builtin.numArgs - 1 /* instance */;
  size_t stackSpace = stackConsumed(numNonInstanceArgs);

  FunctionCall baselineCall(lineOrBytecode);
  beginCall(baselineCall, UseABI::System, InterModule::True);

  ABIArg instanceArg = reservePointerArgument(&baselineCall);

  startCallArgs(StackArgAreaSizeUnaligned(builtin), &baselineCall);
  for (uint32_t i = 1; i < builtin.numArgs; i++) {
    ValType t;
    switch (argTypes[i]) {
      case MIRType::Int32:
        t = ValType::I32;
        break;
      case MIRType::Int64:
        t = ValType::I64;
        break;
      case MIRType::RefOrNull:
        t = RefType::extern_();
        break;
      case MIRType::Pointer:
        // Instance function args can now be uninterpreted pointers (eg, for
        // the cases PostBarrier and PostBarrierFilter) so we simply treat
        // them like the equivalently sized integer.
        t = ValType::hostPtr();
        break;
      default:
        MOZ_CRASH("Unexpected type");
    }
    passArg(t, peek(numNonInstanceArgs - i), &baselineCall);
  }
  CodeOffset raOffset =
      builtinInstanceMethodCall(builtin, instanceArg, baselineCall);
  if (!createStackMap("emitInstanceCall", raOffset)) {
    return false;
  }

  endCall(baselineCall, stackSpace);

  popValueStackBy(numNonInstanceArgs);

  // Note, many clients of emitInstanceCall currently assume that pushing the
  // result here does not destroy ReturnReg.
  //
  // Furthermore, clients assume that if builtin.retType != MIRType::None, the
  // callee will have returned a result and left it in ReturnReg for us to
  // find, and that that register will not be destroyed here (or above).

  // For the return type only, MIRType::None is used to indicate that the
  // call doesn't return a result, that is, returns a C/C++ "void".

  if (builtin.retType != MIRType::None) {
    pushReturnValueOfCall(baselineCall, builtin.retType);
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
// Reference types.

bool BaseCompiler::emitRefFunc() {
  return emitInstanceCallOp<uint32_t>(SASigRefFunc,
                                      [this](uint32_t* funcIndex) -> bool {
                                        return iter_.readRefFunc(funcIndex);
                                      });
}

bool BaseCompiler::emitRefNull() {
  RefType type;
  if (!iter_.readRefNull(&type)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  pushRef(NULLREF_VALUE);
  return true;
}

bool BaseCompiler::emitRefIsNull() {
  Nothing nothing;
  if (!iter_.readRefIsNull(&nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  RegRef r = popRef();
  RegI32 rd = narrowRef(r);

  masm.cmpPtrSet(Assembler::Equal, r, ImmWord(NULLREF_VALUE), rd);
  pushI32(rd);
  return true;
}

#ifdef ENABLE_WASM_FUNCTION_REFERENCES
bool BaseCompiler::emitRefAsNonNull() {
  Nothing nothing;
  if (!iter_.readRefAsNonNull(&nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  RegRef rp = popRef();
  Label ok;
  masm.branchTestPtr(Assembler::NonZero, rp, rp, &ok);
  trap(Trap::NullPointerDereference);
  masm.bind(&ok);
  pushRef(rp);

  return true;
}
#endif

//////////////////////////////////////////////////////////////////////////////
//
// Atomic operations.

bool BaseCompiler::emitAtomicCmpXchg(ValType type, Scalar::Type viewType) {
  LinearMemoryAddress<Nothing> addr;
  Nothing unused{};
  if (!iter_.readAtomicCmpXchg(&addr, type, Scalar::byteSize(viewType), &unused,
                               &unused)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset(),
                          Synchronization::Full());
  atomicCmpXchg(&access, type);
  return true;
}

bool BaseCompiler::emitAtomicLoad(ValType type, Scalar::Type viewType) {
  LinearMemoryAddress<Nothing> addr;
  if (!iter_.readAtomicLoad(&addr, type, Scalar::byteSize(viewType))) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset(),
                          Synchronization::Load());
  atomicLoad(&access, type);
  return true;
}

bool BaseCompiler::emitAtomicRMW(ValType type, Scalar::Type viewType,
                                 AtomicOp op) {
  LinearMemoryAddress<Nothing> addr;
  Nothing unused_value;
  if (!iter_.readAtomicRMW(&addr, type, Scalar::byteSize(viewType),
                           &unused_value)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset(),
                          Synchronization::Full());
  atomicRMW(&access, type, op);
  return true;
}

bool BaseCompiler::emitAtomicStore(ValType type, Scalar::Type viewType) {
  LinearMemoryAddress<Nothing> addr;
  Nothing unused_value;
  if (!iter_.readAtomicStore(&addr, type, Scalar::byteSize(viewType),
                             &unused_value)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset(),
                          Synchronization::Store());
  atomicStore(&access, type);
  return true;
}

bool BaseCompiler::emitAtomicXchg(ValType type, Scalar::Type viewType) {
  LinearMemoryAddress<Nothing> addr;
  Nothing unused_value;
  if (!iter_.readAtomicRMW(&addr, type, Scalar::byteSize(viewType),
                           &unused_value)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset(),
                          Synchronization::Full());
  atomicXchg(&access, type);
  return true;
}

bool BaseCompiler::emitWait(ValType type, uint32_t byteSize) {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();
  Nothing nothing;
  LinearMemoryAddress<Nothing> addr;
  if (!iter_.readWait(&addr, type, byteSize, &nothing, &nothing)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  MemoryAccessDesc access(
      type.kind() == ValType::I32 ? Scalar::Int32 : Scalar::Int64, addr.align,
      addr.offset, bytecodeOffset());
  return atomicWait(type, &access, lineOrBytecode);
}

bool BaseCompiler::emitWake() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();
  Nothing nothing;
  LinearMemoryAddress<Nothing> addr;
  if (!iter_.readWake(&addr, &nothing)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  MemoryAccessDesc access(Scalar::Int32, addr.align, addr.offset,
                          bytecodeOffset());
  return atomicWake(&access, lineOrBytecode);
}

bool BaseCompiler::emitFence() {
  if (!iter_.readFence()) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  masm.memoryBarrier(MembarFull);
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
// Bulk memory operations.

bool BaseCompiler::emitMemoryGrow() {
  return emitInstanceCallOp(
      !usesMemory() || isMem32() ? SASigMemoryGrowM32 : SASigMemoryGrowM64,
      [this]() -> bool {
        Nothing arg;
        return iter_.readMemoryGrow(&arg);
      });
}

bool BaseCompiler::emitMemorySize() {
  return emitInstanceCallOp(
      !usesMemory() || isMem32() ? SASigMemorySizeM32 : SASigMemorySizeM64,
      [this]() -> bool { return iter_.readMemorySize(); });
}

bool BaseCompiler::emitMemCopy() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();
  uint32_t dstMemOrTableIndex = 0;
  uint32_t srcMemOrTableIndex = 0;
  Nothing nothing;
  if (!iter_.readMemOrTableCopy(true, &dstMemOrTableIndex, &nothing,
                                &srcMemOrTableIndex, &nothing, &nothing)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }

  if (isMem32()) {
    int32_t signedLength;
    if (peekConst(&signedLength) && signedLength != 0 &&
        uint32_t(signedLength) <= MaxInlineMemoryCopyLength) {
      memCopyInlineM32();
      return true;
    }
  }

  return memCopyCall(lineOrBytecode);
}

bool BaseCompiler::memCopyCall(uint32_t lineOrBytecode) {
  pushHeapBase();
  return emitInstanceCall(
      lineOrBytecode,
      usesSharedMemory()
          ? (isMem32() ? SASigMemCopySharedM32 : SASigMemCopySharedM64)
          : (isMem32() ? SASigMemCopyM32 : SASigMemCopyM64));
}

bool BaseCompiler::emitMemFill() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();
  Nothing nothing;
  if (!iter_.readMemFill(&nothing, &nothing, &nothing)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }

  if (isMem32()) {
    int32_t signedLength;
    int32_t signedValue;
    if (peek2xConst(&signedLength, &signedValue) && signedLength != 0 &&
        uint32_t(signedLength) <= MaxInlineMemoryFillLength) {
      memFillInlineM32();
      return true;
    }
  }
  return memFillCall(lineOrBytecode);
}

bool BaseCompiler::memFillCall(uint32_t lineOrBytecode) {
  pushHeapBase();
  return emitInstanceCall(
      lineOrBytecode,
      usesSharedMemory()
          ? (isMem32() ? SASigMemFillSharedM32 : SASigMemFillSharedM64)
          : (isMem32() ? SASigMemFillM32 : SASigMemFillM64));
}

bool BaseCompiler::emitMemInit() {
  return emitInstanceCallOp<uint32_t>(
      (!usesMemory() || isMem32() ? SASigMemInitM32 : SASigMemInitM64),
      [this](uint32_t* segIndex) -> bool {
        Nothing nothing;
        if (iter_.readMemOrTableInit(/*isMem*/ true, segIndex, nullptr,
                                     &nothing, &nothing, &nothing)) {
          return true;
        }
        return false;
      });
}

//////////////////////////////////////////////////////////////////////////////
//
// Bulk table operations.

bool BaseCompiler::emitTableCopy() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  uint32_t dstMemOrTableIndex = 0;
  uint32_t srcMemOrTableIndex = 0;
  Nothing nothing;
  if (!iter_.readMemOrTableCopy(false, &dstMemOrTableIndex, &nothing,
                                &srcMemOrTableIndex, &nothing, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  pushI32(dstMemOrTableIndex);
  pushI32(srcMemOrTableIndex);
  return emitInstanceCall(lineOrBytecode, SASigTableCopy);
}

bool BaseCompiler::emitTableInit() {
  return emitInstanceCallOp<uint32_t, uint32_t>(
      SASigTableInit,
      [this](uint32_t* segIndex, uint32_t* dstTableIndex) -> bool {
        Nothing nothing;
        return iter_.readMemOrTableInit(/*isMem*/ false, segIndex,
                                        dstTableIndex, &nothing, &nothing,
                                        &nothing);
      });
}

bool BaseCompiler::emitTableFill() {
  // fill(start:u32, val:ref, len:u32, table:u32) -> void
  return emitInstanceCallOp<uint32_t>(
      SASigTableFill, [this](uint32_t* tableIndex) -> bool {
        Nothing nothing;
        return iter_.readTableFill(tableIndex, &nothing, &nothing, &nothing);
      });
}

bool BaseCompiler::emitTableGet() {
  // get(index:u32, table:u32) -> AnyRef
  return emitInstanceCallOp<uint32_t>(
      SASigTableGet, [this](uint32_t* tableIndex) -> bool {
        Nothing nothing;
        return iter_.readTableGet(tableIndex, &nothing);
      });
}

bool BaseCompiler::emitTableGrow() {
  // grow(initValue:anyref, delta:u32, table:u32) -> u32
  return emitInstanceCallOp<uint32_t>(
      SASigTableGrow, [this](uint32_t* tableIndex) -> bool {
        Nothing nothing;
        return iter_.readTableGrow(tableIndex, &nothing, &nothing);
      });
}

bool BaseCompiler::emitTableSet() {
  // set(index:u32, value:ref, table:u32) -> void
  return emitInstanceCallOp<uint32_t>(
      SASigTableSet, [this](uint32_t* tableIndex) -> bool {
        Nothing nothing;
        return iter_.readTableSet(tableIndex, &nothing, &nothing);
      });
}

bool BaseCompiler::emitTableSize() {
  // size(table:u32) -> u32
  return emitInstanceCallOp<uint32_t>(SASigTableSize,
                                      [this](uint32_t* tableIndex) -> bool {
                                        return iter_.readTableSize(tableIndex);
                                      });
}

//////////////////////////////////////////////////////////////////////////////
//
// Data and element segment management.

bool BaseCompiler::emitDataOrElemDrop(bool isData) {
  return emitInstanceCallOp<uint32_t>(
      isData ? SASigDataDrop : SASigElemDrop, [&](uint32_t* segIndex) -> bool {
        return iter_.readDataOrElemDrop(isData, segIndex);
      });
}

//////////////////////////////////////////////////////////////////////////////
//
// General object support.

void BaseCompiler::emitPreBarrier(RegPtr valueAddr) {
  Label skipBarrier;
  ScratchPtr scratch(*this);

  fr.loadTlsPtr(scratch);
  EmitWasmPreBarrierGuard(masm, scratch, scratch, valueAddr, &skipBarrier);

  fr.loadTlsPtr(scratch);
#ifdef JS_CODEGEN_ARM64
  // The prebarrier stub assumes the PseudoStackPointer is set up.  It is OK
  // to just move the sp to x28 here because x28 is not being used by the
  // baseline compiler and need not be saved or restored.
  MOZ_ASSERT(!GeneralRegisterSet::All().hasRegisterIndex(x28.asUnsized()));
  masm.Mov(x28, sp);
#endif
  // The prebarrier call preserves all volatile registers
  EmitWasmPreBarrierCall(masm, scratch, scratch, valueAddr);

  masm.bind(&skipBarrier);
}

// This frees the register `valueAddr`.

bool BaseCompiler::emitPostBarrierCall(RegPtr valueAddr) {
  uint32_t bytecodeOffset = iter_.lastOpcodeOffset();

  // The `valueAddr` is a raw pointer to the cell within some GC object or
  // TLS area, and we guarantee that the GC will not run while the
  // postbarrier call is active, so push a uintptr_t value.
  pushPtr(valueAddr);
  return emitInstanceCall(bytecodeOffset, SASigPostBarrier);
}

// Emits a store to a JS object pointer at the address valueAddr, which is
// inside the GC cell `object`. Preserves `object` and `value`.
bool BaseCompiler::emitBarrieredStore(const Maybe<RegRef>& object,
                                      RegPtr valueAddr, RegRef value) {
  // TODO/AnyRef-boxing: With boxed immediates and strings, the write
  // barrier is going to have to be more complicated.
  ASSERT_ANYREF_IS_JSOBJECT;

  emitPreBarrier(valueAddr);  // Preserves valueAddr
  masm.storePtr(value, Address(valueAddr, 0));

  Label skipBarrier;
  sync();

  RegRef otherScratch = needRef();
  EmitWasmPostBarrierGuard(masm, object, otherScratch, value, &skipBarrier);
  freeRef(otherScratch);

  if (object) {
    pushRef(*object);
  }
  pushRef(value);

  // Consumes valueAddr
  if (!emitPostBarrierCall(valueAddr)) {
    return false;
  }

  // Consume all other operands as they may have been clobbered by the post
  // barrier call
  popRef(value);
  if (object) {
    popRef(*object);
  }

  masm.bind(&skipBarrier);
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
// GC proposal.

#ifdef ENABLE_WASM_GC

void BaseCompiler::emitGcCanon(uint32_t typeIndex) {
  const TypeIdDesc& typeId = moduleEnv_.typeIds[typeIndex];
  RegRef rp = needRef();
  fr.loadTlsPtr(WasmTlsReg);
  masm.loadWasmGlobalPtr(typeId.globalDataOffset(), rp);
  pushRef(rp);
}

void BaseCompiler::emitGcNullCheck(RegRef rp) {
  Label ok;
  masm.branchTestPtr(Assembler::NonZero, rp, rp, &ok);
  trap(Trap::NullPointerDereference);
  masm.bind(&ok);
}

RegPtr BaseCompiler::emitGcArrayGetData(RegRef rp) {
  RegPtr rdata = needPtr();
  // An array is always an outline typed object
  masm.loadPtr(Address(rp, OutlineTypedObject::offsetOfData()), rdata);
  return rdata;
}

RegI32 BaseCompiler::emitGcArrayGetLength(RegPtr rdata,
                                          bool adjustDataPointer) {
  STATIC_ASSERT_ARRAYLENGTH_IS_U32;
  RegI32 length = needI32();
  masm.load32(Address(rdata, OutlineTypedObject::offsetOfArrayLength()),
              length);
  if (adjustDataPointer) {
    masm.addPtr(ImmWord(OutlineTypedObject::offsetOfArrayLength() +
                        sizeof(OutlineTypedObject::ArrayLength)),
                rdata);
  }
  return length;
}

void BaseCompiler::emitGcArrayBoundsCheck(RegI32 index, RegI32 length) {
  Label inBounds;
  masm.branch32(Assembler::Below, index, length, &inBounds);
  masm.wasmTrap(Trap::OutOfBounds, bytecodeOffset());
  masm.bind(&inBounds);
}

template <typename T>
void BaseCompiler::emitGcGet(FieldType type, FieldExtension extension,
                             const T& src) {
  switch (type.kind()) {
    case FieldType::I8: {
      MOZ_ASSERT(extension != FieldExtension::None);
      RegI32 r = needI32();
      if (extension == FieldExtension::Unsigned) {
        masm.load8ZeroExtend(src, r);
      } else {
        masm.load8SignExtend(src, r);
      }
      pushI32(r);
      break;
    }
    case FieldType::I16: {
      MOZ_ASSERT(extension != FieldExtension::None);
      RegI32 r = needI32();
      if (extension == FieldExtension::Unsigned) {
        masm.load16ZeroExtend(src, r);
      } else {
        masm.load16SignExtend(src, r);
      }
      pushI32(r);
      break;
    }
    case FieldType::I32: {
      MOZ_ASSERT(extension == FieldExtension::None);
      RegI32 r = needI32();
      masm.load32(src, r);
      pushI32(r);
      break;
    }
    case FieldType::I64: {
      MOZ_ASSERT(extension == FieldExtension::None);
      RegI64 r = needI64();
      masm.load64(src, r);
      pushI64(r);
      break;
    }
    case FieldType::F32: {
      MOZ_ASSERT(extension == FieldExtension::None);
      RegF32 r = needF32();
      masm.loadFloat32(src, r);
      pushF32(r);
      break;
    }
    case FieldType::F64: {
      MOZ_ASSERT(extension == FieldExtension::None);
      RegF64 r = needF64();
      masm.loadDouble(src, r);
      pushF64(r);
      break;
    }
#  ifdef ENABLE_WASM_SIMD
    case FieldType::V128: {
      MOZ_ASSERT(extension == FieldExtension::None);
      RegV128 r = needV128();
      masm.loadUnalignedSimd128(src, r);
      pushV128(r);
      break;
    }
#  endif
    case FieldType::Ref: {
      MOZ_ASSERT(extension == FieldExtension::None);
      RegRef r = needRef();
      masm.loadPtr(src, r);
      pushRef(r);
      break;
    }
    default: {
      MOZ_CRASH("Unexpected field type");
    }
  }
}

template <typename T>
void BaseCompiler::emitGcSetScalar(const T& dst, FieldType type, AnyReg value) {
  switch (type.kind()) {
    case FieldType::I8: {
      masm.store8(value.i32(), dst);
      break;
    }
    case FieldType::I16: {
      masm.store16(value.i32(), dst);
      break;
    }
    case FieldType::I32: {
      masm.store32(value.i32(), dst);
      break;
    }
    case FieldType::I64: {
      masm.store64(value.i64(), dst);
      break;
    }
    case FieldType::F32: {
      masm.storeFloat32(value.f32(), dst);
      break;
    }
    case FieldType::F64: {
      masm.storeDouble(value.f64(), dst);
      break;
    }
#  ifdef ENABLE_WASM_SIMD
    case FieldType::V128: {
      masm.storeUnalignedSimd128(value.v128(), dst);
      break;
    }
#  endif
    default: {
      MOZ_CRASH("Unexpected field type");
    }
  }
}

bool BaseCompiler::emitGcStructSet(RegRef object, RegPtr data,
                                   const StructField& field, AnyReg value) {
  // Easy path if the field is a scalar
  if (!field.type.isRefRepr()) {
    emitGcSetScalar(Address(data, field.offset), field.type, value);
    freeAny(value);
    return true;
  }

  // Create temporary for the valueAddr that is not in the prebarrier register
  // and can be consumed by the barrier operation
  RegPtr valueAddr = RegPtr(PreBarrierReg);
  needPtr(valueAddr);
  masm.computeEffectiveAddress(Address(data, field.offset), valueAddr);

  // Save state for after barriered write
  pushPtr(data);

  // emitBarrieredStore preserves object and value
  if (!emitBarrieredStore(Some(object), valueAddr, value.ref())) {
    return false;
  }
  freeRef(value.ref());

  // Restore state
  popPtr(data);

  return true;
}

bool BaseCompiler::emitGcArraySet(RegRef object, RegPtr data, RegI32 index,
                                  const ArrayType& arrayType, AnyReg value) {
  // Try to use a base index store instruction if the field type fits in a
  // shift immediate. If not we shift the index manually and then unshift
  // it after the store. We don't use an extra register for this because we
  // don't have any to spare on x86.
  uint32_t shift = arrayType.elementType_.indexingShift();
  Scale scale;
  bool shiftedIndex = false;
  if (IsShiftInScaleRange(shift)) {
    scale = ShiftToScale(shift);
  } else {
    masm.lshiftPtr(Imm32(shift), index);
    scale = TimesOne;
    shiftedIndex = true;
  }
  auto unshiftIndex = mozilla::MakeScopeExit([&] {
    if (shiftedIndex) {
      masm.rshiftPtr(Imm32(shift), index);
    }
  });

  // Easy path if the field is a scalar
  if (!arrayType.elementType_.isRefRepr()) {
    emitGcSetScalar(BaseIndex(data, index, scale, 0), arrayType.elementType_,
                    value);
    return true;
  }

  // Create temporaries for valueAddr that is not in the prebarrier register
  // and can be consumed by the barrier operation
  RegPtr valueAddr = RegPtr(PreBarrierReg);
  needPtr(valueAddr);
  masm.computeEffectiveAddress(BaseIndex(data, index, scale, 0), valueAddr);

  // Save state for after barriered write
  pushPtr(data);
  pushI32(index);

  // emitBarrieredStore preserves object and value
  if (!emitBarrieredStore(Some(object), valueAddr, value.ref())) {
    return false;
  }

  // Restore state
  popI32(index);
  popPtr(data);

  return true;
}

bool BaseCompiler::emitStructNewWithRtt() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  uint32_t typeIndex;
  Nothing rtt;
  BaseNothingVector args{};
  if (!iter_.readStructNewWithRtt(&typeIndex, &rtt, &args)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  const StructType& structType = (*moduleEnv_.types)[typeIndex].structType();

  // Allocate zeroed storage.  The parameter to StructNew is a rtt value that is
  // guaranteed to be at the top of the stack by validation.
  //
  // Traps on OOM.
  if (!emitInstanceCall(lineOrBytecode, SASigStructNew)) {
    return false;
  }

  // Optimization opportunity: Iterate backward to pop arguments off the
  // stack.  This will generate more instructions than we want, since we
  // really only need to pop the stack once at the end, not for every element,
  // but to do better we need a bit more machinery to load elements off the
  // stack into registers.

  // Reserve this register early if we will need it so that it is not taken by
  // any register used in this function.
  needPtr(RegPtr(PreBarrierReg));

  RegRef rp = popRef();
  RegPtr rdata = needPtr();

  // Free the barrier reg after we've allocated all registers
  freePtr(RegPtr(PreBarrierReg));

  // The struct allocated above is guaranteed to have the exact shape of
  // structType, we don't need to branch on whether it's inline or not.
  if (InlineTypedObject::canAccommodateSize(structType.size_)) {
    masm.computeEffectiveAddress(
        Address(rp, InlineTypedObject::offsetOfDataStart()), rdata);
  } else {
    masm.loadPtr(Address(rp, OutlineTypedObject::offsetOfData()), rdata);
  }

  // Optimization opportunity: when the value being stored is a known
  // zero/null we need store nothing.  This case may be somewhat common
  // because struct.new forces a value to be specified for every field.

  uint32_t fieldIndex = structType.fields_.length();
  while (fieldIndex-- > 0) {
    const StructField& structField = structType.fields_[fieldIndex];
    // Reserve the barrier reg if we might need it for this store
    if (structField.type.isRefRepr()) {
      needPtr(RegPtr(PreBarrierReg));
    }

    AnyReg value = popAny();

    // Free the barrier reg now that we've loaded the value
    if (structField.type.isRefRepr()) {
      freePtr(RegPtr(PreBarrierReg));
    }

    // Consumes value. rp, and rdata are preserved
    if (!emitGcStructSet(rp, rdata, structField, value)) {
      return false;
    }
  }

  freePtr(rdata);
  pushRef(rp);

  return true;
}

bool BaseCompiler::emitStructNewDefaultWithRtt() {
  // Allocate zeroed storage.  The parameter to StructNew is a rtt value that is
  // guaranteed to be at the top of the stack by validation.
  //
  // Traps on OOM.

  // (rtt) -> ref; the type index is just dropped on the floor
  return emitInstanceCallOp(SASigStructNew, [this]() -> bool {
    uint32_t unusedTypeIndex;
    Nothing unusedRtt;
    return iter_.readStructNewDefaultWithRtt(&unusedTypeIndex, &unusedRtt);
  });
}

bool BaseCompiler::emitStructGet(FieldExtension extension) {
  uint32_t typeIndex;
  uint32_t fieldIndex;
  Nothing nothing;
  if (!iter_.readStructGet(&typeIndex, &fieldIndex, extension, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  const StructType& structType = (*moduleEnv_.types)[typeIndex].structType();

  RegRef rp = popRef();

  // Check for null
  emitGcNullCheck(rp);

  // Acquire the data pointer from the object
  RegPtr rdata = needPtr();
  {
    RegPtr scratch = needPtr();
    RegPtr clasp = needPtr();
    masm.movePtr(SymbolicAddress::InlineTypedObjectClass, clasp);
    Label join;
    Label isInline;
    masm.branchTestObjClass(Assembler::Equal, rp, clasp, scratch, rp,
                            &isInline);
    freePtr(clasp);
    freePtr(scratch);

    masm.loadPtr(Address(rp, OutlineTypedObject::offsetOfData()), rdata);
    masm.jump(&join);

    masm.bind(&isInline);
    masm.computeEffectiveAddress(
        Address(rp, InlineTypedObject::offsetOfDataStart()), rdata);
    masm.bind(&join);
  }

  // Load the value
  FieldType type = structType.fields_[fieldIndex].type;
  uint32_t offset = structType.fields_[fieldIndex].offset;
  emitGcGet(type, extension, Address(rdata, offset));

  freePtr(rdata);
  freeRef(rp);

  return true;
}

bool BaseCompiler::emitStructSet() {
  uint32_t typeIndex;
  uint32_t fieldIndex;
  Nothing nothing;
  if (!iter_.readStructSet(&typeIndex, &fieldIndex, &nothing, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  const StructType& structType = (*moduleEnv_.types)[typeIndex].structType();
  const StructField& structField = structType.fields_[fieldIndex];

  // Reserve this register early if we will need it so that it is not taken by
  // any register used in this function.
  if (structField.type.isRefRepr()) {
    needPtr(RegPtr(PreBarrierReg));
  }

  AnyReg value = popAny();
  RegRef rp = popRef();
  RegPtr rdata = needPtr();

  // Free the barrier reg after we've allocated all registers
  if (structField.type.isRefRepr()) {
    freePtr(RegPtr(PreBarrierReg));
  }

  // Check for null
  emitGcNullCheck(rp);

  // Acquire the data pointer from the object.
  {
    // We don't have a register to spare at this point on x86, so carefully
    // borrow the `rdata` as a scratch pointer during the instruction sequence
    // that loads `rdata`.
    RegPtr scratch = rdata;
    RegPtr clasp = needPtr();
    masm.movePtr(SymbolicAddress::InlineTypedObjectClass, clasp);
    Label join;
    Label isInline;
    masm.branchTestObjClass(Assembler::Equal, rp, clasp, scratch, rp,
                            &isInline);
    freePtr(clasp);
    masm.loadPtr(Address(rp, OutlineTypedObject::offsetOfData()), rdata);
    masm.jump(&join);

    masm.bind(&isInline);
    masm.computeEffectiveAddress(
        Address(rp, InlineTypedObject::offsetOfDataStart()), rdata);
    masm.bind(&join);
  }

  // Consumes value. rp, and rdata are preserved
  if (!emitGcStructSet(rp, rdata, structField, value)) {
    return false;
  }

  freePtr(rdata);
  freeRef(rp);

  return true;
}

bool BaseCompiler::emitArrayNewWithRtt() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  uint32_t typeIndex;
  Nothing nothing;
  if (!iter_.readArrayNewWithRtt(&typeIndex, &nothing, &nothing, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  const ArrayType& arrayType = (*moduleEnv_.types)[typeIndex].arrayType();

  // Allocate zeroed storage.  The parameter to ArrayNew is a rtt value and
  // length that are guaranteed to be at the top of the stack by validation.
  //
  // Traps on OOM.
  if (!emitInstanceCall(lineOrBytecode, SASigArrayNew)) {
    return false;
  }

  // Reserve this register early if we will need it so that it is not taken by
  // any register used in this function.
  if (arrayType.elementType_.isRefRepr()) {
    needPtr(RegPtr(PreBarrierReg));
  }

  RegRef rp = popRef();
  AnyReg value = popAny();

  // Acquire the data pointers from the object
  RegPtr rdata = emitGcArrayGetData(rp);

  // Acquire the array length and adjust the data pointer to be immediately
  // after the array length header
  RegI32 length = emitGcArrayGetLength(rdata, true);

  // Free the barrier reg after we've allocated all registers
  if (arrayType.elementType_.isRefRepr()) {
    freePtr(RegPtr(PreBarrierReg));
  }

  // Perform an initialization loop using `length` as the loop variable,
  // counting down to zero.
  Label done;
  Label loop;
  // Skip initialization if length = 0
  masm.branch32(Assembler::Equal, length, Imm32(0), &done);
  masm.bind(&loop);

  // Move to the next element
  masm.sub32(Imm32(1), length);

  // Assign value to array[length]. All registers are preserved
  if (!emitGcArraySet(rp, rdata, length, arrayType, value)) {
    return false;
  }

  // Loop back if there are still elements to initialize
  masm.branch32(Assembler::GreaterThan, length, Imm32(0), &loop);
  masm.bind(&done);

  freeI32(length);
  freeAny(value);
  freePtr(rdata);
  pushRef(rp);

  return true;
}

bool BaseCompiler::emitArrayNewDefaultWithRtt() {
  // Allocate zeroed storage. The parameter to ArrayNew is a rtt value that is
  // guaranteed to be at the top of the stack by validation.
  //
  // Traps on OOM.

  // (rtt) -> ref; the type index is dropped on the floor.
  return emitInstanceCallOp(SASigArrayNew, [this]() -> bool {
    uint32_t unusedTypeIndex;
    Nothing nothing;
    return iter_.readArrayNewDefaultWithRtt(&unusedTypeIndex, &nothing,
                                            &nothing);
  });
}

bool BaseCompiler::emitArrayGet(FieldExtension extension) {
  uint32_t typeIndex;
  Nothing nothing;
  if (!iter_.readArrayGet(&typeIndex, extension, &nothing, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  const ArrayType& arrayType = (*moduleEnv_.types)[typeIndex].arrayType();

  RegI32 index = popI32();
  RegRef rp = popRef();

  // Check for null
  emitGcNullCheck(rp);

  // Acquire the data pointer from the object
  RegPtr rdata = emitGcArrayGetData(rp);

  // Acquire the array length and adjust the data pointer to be immediately
  // after the array length header
  RegI32 length = emitGcArrayGetLength(rdata, true);

  // Bounds check the index
  emitGcArrayBoundsCheck(index, length);
  freeI32(length);

  // Load the value
  uint32_t shift = arrayType.elementType_.indexingShift();
  if (IsShiftInScaleRange(shift)) {
    emitGcGet(arrayType.elementType_, extension,
              BaseIndex(rdata, index, ShiftToScale(shift), 0));
  } else {
    masm.lshiftPtr(Imm32(shift), index);
    emitGcGet(arrayType.elementType_, extension,
              BaseIndex(rdata, index, TimesOne, 0));
  }

  freePtr(rdata);
  freeRef(rp);
  freeI32(index);

  return true;
}

bool BaseCompiler::emitArraySet() {
  uint32_t typeIndex;
  Nothing nothing;
  if (!iter_.readArraySet(&typeIndex, &nothing, &nothing, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  const ArrayType& arrayType = (*moduleEnv_.types)[typeIndex].arrayType();

  // Reserve this register early if we will need it so that it is not taken by
  // any register used in this function.
  if (arrayType.elementType_.isRefRepr()) {
    needPtr(RegPtr(PreBarrierReg));
  }

  AnyReg value = popAny();
  RegI32 index = popI32();
  RegRef rp = popRef();

  // We run out of registers on x86 with this instruction, so stash `value` on
  // the stack until it is needed later.
  pushAny(value);

  // Check for null
  emitGcNullCheck(rp);

  // Acquire the data pointer from the object
  RegPtr rdata = emitGcArrayGetData(rp);

  // Acquire the array length and adjust the data pointer to be immediately
  // after the array length header
  RegI32 length = emitGcArrayGetLength(rdata, true);

  // Free the barrier reg after we've allocated all registers
  if (arrayType.elementType_.isRefRepr()) {
    freePtr(RegPtr(PreBarrierReg));
  }

  // Bounds check the index
  emitGcArrayBoundsCheck(index, length);
  freeI32(length);

  // Pull the value out of the stack now that we need it.
  popAny(value);

  // All registers are preserved. This isn't strictly necessary, as we'll just
  // be freeing them all after this is done. But this is needed for repeated
  // assignments used in array.new/new_default.
  if (!emitGcArraySet(rp, rdata, index, arrayType, value)) {
    return false;
  }

  freePtr(rdata);
  freeRef(rp);
  freeI32(index);
  freeAny(value);

  return true;
}

bool BaseCompiler::emitArrayLen() {
  uint32_t typeIndex;
  Nothing nothing;
  if (!iter_.readArrayLen(&typeIndex, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  RegRef rp = popRef();

  // Check for null
  emitGcNullCheck(rp);

  // Acquire the data pointer from the object
  RegPtr rdata = emitGcArrayGetData(rp);
  freeRef(rp);

  // Acquire the length from the array
  pushI32(emitGcArrayGetLength(rdata, false));
  freePtr(rdata);

  return true;
}

bool BaseCompiler::emitRttCanon() {
  ValType rttType;
  if (!iter_.readRttCanon(&rttType)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  emitGcCanon(rttType.typeIndex());
  return true;
}

bool BaseCompiler::emitRttSub() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  Nothing nothing;
  uint32_t rttSubTypeIndex;
  if (!iter_.readRttSub(&nothing, &rttSubTypeIndex)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  // rtt.sub $t; [(rtt $t' _)] -> [(rtt $t _)]
  // Push (rtt.canon $t) so that we can cache the result and yield the
  // same rtt value for the same $t operand.
  emitGcCanon(rttSubTypeIndex);

  if (!emitInstanceCall(lineOrBytecode, SASigRttSub)) {
    return false;
  }
  return true;
}

bool BaseCompiler::emitRefTest() {
  // refTest builtin has same signature as ref.test instruction, stack is
  // guaranteed to be in the right condition due to validation.
  return emitInstanceCallOp(SASigRefTest, [this]() -> bool {
    Nothing nothing;
    uint32_t unusedRttTypeIndex;
    uint32_t unusedRttDepth;
    return iter_.readRefTest(&nothing, &unusedRttTypeIndex, &unusedRttDepth,
                             &nothing);
  });
}

bool BaseCompiler::emitRefCast() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  Nothing nothing;
  uint32_t rttTypeIndex;
  uint32_t rttDepth;
  if (!iter_.readRefCast(&nothing, &rttTypeIndex, &rttDepth, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  RegRef rttPtr = popRef();
  RegRef refPtr = popRef();

  // 1. duplicate and shuffle from [ref, rtt] to [ref, ref, rtt]
  RegRef castedPtr = needRef();
  moveRef(refPtr, castedPtr);
  pushRef(castedPtr);
  pushRef(refPtr);
  pushRef(rttPtr);

  // 2. ref.test : [ref, rtt] -> [i32]
  if (!emitInstanceCall(lineOrBytecode, SASigRefTest)) {
    return false;
  }

  // 3. trap if result is zero, leaving [ref] as result
  RegI32 result = popI32();
  Label nonZero;
  masm.branchTest32(Assembler::NonZero, result, result, &nonZero);
  masm.wasmTrap(Trap::BadCast, bytecodeOffset());
  masm.bind(&nonZero);
  freeI32(result);

  return true;
}

bool BaseCompiler::emitBrOnCast() {
  MOZ_ASSERT(!hasLatentOp());

  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();
  uint32_t relativeDepth;
  Nothing unused{};
  BaseNothingVector unused_values{};
  uint32_t rttTypeIndex;
  uint32_t rttDepth;
  ResultType branchTargetType;
  if (!iter_.readBrOnCast(&relativeDepth, &unused, &rttTypeIndex, &rttDepth,
                          &branchTargetType, &unused_values)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  Control& target = controlItem(relativeDepth);
  target.bceSafeOnExit &= bceSafe_;

  RegRef rttPtr = popRef();
  RegRef refPtr = popRef();

  // 1. duplicate and shuffle from [T*, ref, rtt] to [T*, ref, ref, rtt]
  RegRef castedPtr = needRef();
  moveRef(refPtr, castedPtr);
  pushRef(castedPtr);
  pushRef(refPtr);
  pushRef(rttPtr);

  // 2. ref.test : [ref, rtt] -> [i32]
  if (!emitInstanceCall(lineOrBytecode, SASigRefTest)) {
    return false;
  }

  // 3. br_if $l : [T*, ref, i32] -> [T*, ref]
  BranchState b(&target.label, target.stackHeight, InvertBranch(false),
                branchTargetType);
  if (b.hasBlockResults()) {
    needResultRegisters(b.resultType);
  }
  RegI32 condition = popI32();
  if (b.hasBlockResults()) {
    freeResultRegisters(b.resultType);
  }
  if (!jumpConditionalWithResults(&b, Assembler::NotEqual, condition,
                                  Imm32(0))) {
    return false;
  }
  freeI32(condition);

  return true;
}

#endif  // ENABLE_WASM_GC

//////////////////////////////////////////////////////////////////////////////
//
// SIMD and Relaxed SIMD.

#ifdef ENABLE_WASM_SIMD

// Emitter trampolines used by abstracted SIMD operations.  Naming here follows
// the SIMD spec pretty closely.

static void AndV128(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.bitwiseAndSimd128(rs, rsd);
}

static void OrV128(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.bitwiseOrSimd128(rs, rsd);
}

static void XorV128(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.bitwiseXorSimd128(rs, rsd);
}

static void AddI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.addInt8x16(rs, rsd);
}

static void AddI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.addInt16x8(rs, rsd);
}

static void AddI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.addInt32x4(rs, rsd);
}

static void AddF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.addFloat32x4(rs, rsd);
}

static void AddI64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.addInt64x2(rs, rsd);
}

static void AddF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.addFloat64x2(rs, rsd);
}

static void AddSatI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.addSatInt8x16(rs, rsd);
}

static void AddSatUI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedAddSatInt8x16(rs, rsd);
}

static void AddSatI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.addSatInt16x8(rs, rsd);
}

static void AddSatUI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedAddSatInt16x8(rs, rsd);
}

static void SubI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.subInt8x16(rs, rsd);
}

static void SubI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.subInt16x8(rs, rsd);
}

static void SubI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.subInt32x4(rs, rsd);
}

static void SubF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.subFloat32x4(rs, rsd);
}

static void SubI64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.subInt64x2(rs, rsd);
}

static void SubF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.subFloat64x2(rs, rsd);
}

static void SubSatI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.subSatInt8x16(rs, rsd);
}

static void SubSatUI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedSubSatInt8x16(rs, rsd);
}

static void SubSatI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.subSatInt16x8(rs, rsd);
}

static void SubSatUI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedSubSatInt16x8(rs, rsd);
}

static void MulI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.mulInt16x8(rs, rsd);
}

static void MulI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.mulInt32x4(rs, rsd);
}

static void MulF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.mulFloat32x4(rs, rsd);
}

#  if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
static void MulI64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd,
                     RegV128 temp) {
  masm.mulInt64x2(rsd, rs, rsd, temp);
}
#  elif defined(JS_CODEGEN_ARM64)
static void MulI64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd,
                     RegV128 temp1, RegV128 temp2) {
  masm.mulInt64x2(rsd, rs, rsd, temp1, temp2);
}
#  endif

static void MulF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.mulFloat64x2(rs, rsd);
}

static void DivF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.divFloat32x4(rs, rsd);
}

static void DivF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.divFloat64x2(rs, rsd);
}

#  if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
static void MinF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd,
                     RegV128 temp1, RegV128 temp2) {
  masm.minFloat32x4(rs, rsd, temp1, temp2);
}

static void MinF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd,
                     RegV128 temp1, RegV128 temp2) {
  masm.minFloat64x2(rs, rsd, temp1, temp2);
}

static void MaxF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd,
                     RegV128 temp1, RegV128 temp2) {
  masm.maxFloat32x4(rs, rsd, temp1, temp2);
}

static void MaxF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd,
                     RegV128 temp1, RegV128 temp2) {
  masm.maxFloat64x2(rs, rsd, temp1, temp2);
}

static void PMinF32x4(MacroAssembler& masm, RegV128 rsd, RegV128 rs,
                      RhsDestOp) {
  masm.pseudoMinFloat32x4(rsd, rs);
}

static void PMinF64x2(MacroAssembler& masm, RegV128 rsd, RegV128 rs,
                      RhsDestOp) {
  masm.pseudoMinFloat64x2(rsd, rs);
}

static void PMaxF32x4(MacroAssembler& masm, RegV128 rsd, RegV128 rs,
                      RhsDestOp) {
  masm.pseudoMaxFloat32x4(rsd, rs);
}

static void PMaxF64x2(MacroAssembler& masm, RegV128 rsd, RegV128 rs,
                      RhsDestOp) {
  masm.pseudoMaxFloat64x2(rsd, rs);
}
#  elif defined(JS_CODEGEN_ARM64)
static void MinF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.minFloat32x4(rs, rsd);
}

static void MinF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.minFloat64x2(rs, rsd);
}

static void MaxF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.maxFloat32x4(rs, rsd);
}

static void MaxF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.maxFloat64x2(rs, rsd);
}

static void PMinF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.pseudoMinFloat32x4(rs, rsd);
}

static void PMinF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.pseudoMinFloat64x2(rs, rsd);
}

static void PMaxF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.pseudoMaxFloat32x4(rs, rsd);
}

static void PMaxF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.pseudoMaxFloat64x2(rs, rsd);
}
#  endif

static void DotI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.widenDotInt16x8(rs, rsd);
}

static void ExtMulLowI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.extMulLowInt8x16(rs, rsd);
}

static void ExtMulHighI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.extMulHighInt8x16(rs, rsd);
}

static void ExtMulLowUI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedExtMulLowInt8x16(rs, rsd);
}

static void ExtMulHighUI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedExtMulHighInt8x16(rs, rsd);
}

static void ExtMulLowI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.extMulLowInt16x8(rs, rsd);
}

static void ExtMulHighI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.extMulHighInt16x8(rs, rsd);
}

static void ExtMulLowUI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedExtMulLowInt16x8(rs, rsd);
}

static void ExtMulHighUI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedExtMulHighInt16x8(rs, rsd);
}

static void ExtMulLowI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.extMulLowInt32x4(rs, rsd);
}

static void ExtMulHighI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.extMulHighInt32x4(rs, rsd);
}

static void ExtMulLowUI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedExtMulLowInt32x4(rs, rsd);
}

static void ExtMulHighUI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedExtMulHighInt32x4(rs, rsd);
}

static void Q15MulrSatS(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.q15MulrSatInt16x8(rs, rsd);
}

static void CmpI8x16(MacroAssembler& masm, Assembler::Condition cond,
                     RegV128 rs, RegV128 rsd) {
  masm.compareInt8x16(cond, rs, rsd);
}

static void CmpI16x8(MacroAssembler& masm, Assembler::Condition cond,
                     RegV128 rs, RegV128 rsd) {
  masm.compareInt16x8(cond, rs, rsd);
}

static void CmpI32x4(MacroAssembler& masm, Assembler::Condition cond,
                     RegV128 rs, RegV128 rsd) {
  masm.compareInt32x4(cond, rs, rsd);
}

#  if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
static void CmpI64x2ForEquality(MacroAssembler& masm, Assembler::Condition cond,
                                RegV128 rs, RegV128 rsd) {
  masm.compareForEqualityInt64x2(cond, rs, rsd);
}

static void CmpI64x2ForOrdering(MacroAssembler& masm, Assembler::Condition cond,
                                RegV128 rs, RegV128 rsd, RegV128 temp1,
                                RegV128 temp2) {
  masm.compareForOrderingInt64x2(cond, rs, rsd, temp1, temp2);
}
#  else
static void CmpI64x2ForEquality(MacroAssembler& masm, Assembler::Condition cond,
                                RegV128 rs, RegV128 rsd) {
  masm.compareInt64x2(cond, rs, rsd);
}

static void CmpI64x2ForOrdering(MacroAssembler& masm, Assembler::Condition cond,
                                RegV128 rs, RegV128 rsd) {
  masm.compareInt64x2(cond, rs, rsd);
}
#  endif  // JS_CODEGEN_X86 || JS_CODEGEN_X64

static void CmpUI8x16(MacroAssembler& masm, Assembler::Condition cond,
                      RegV128 rs, RegV128 rsd) {
  masm.compareInt8x16(cond, rs, rsd);
}

static void CmpUI16x8(MacroAssembler& masm, Assembler::Condition cond,
                      RegV128 rs, RegV128 rsd) {
  masm.compareInt16x8(cond, rs, rsd);
}

static void CmpUI32x4(MacroAssembler& masm, Assembler::Condition cond,
                      RegV128 rs, RegV128 rsd) {
  masm.compareInt32x4(cond, rs, rsd);
}

static void CmpF32x4(MacroAssembler& masm, Assembler::Condition cond,
                     RegV128 rs, RegV128 rsd) {
  masm.compareFloat32x4(cond, rs, rsd);
}

static void CmpF64x2(MacroAssembler& masm, Assembler::Condition cond,
                     RegV128 rs, RegV128 rsd) {
  masm.compareFloat64x2(cond, rs, rsd);
}

static void NegI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.negInt8x16(rs, rd);
}

static void NegI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.negInt16x8(rs, rd);
}

static void NegI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.negInt32x4(rs, rd);
}

static void NegI64x2(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.negInt64x2(rs, rd);
}

static void NegF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.negFloat32x4(rs, rd);
}

static void NegF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.negFloat64x2(rs, rd);
}

static void AbsF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.absFloat32x4(rs, rd);
}

static void AbsF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.absFloat64x2(rs, rd);
}

static void SqrtF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.sqrtFloat32x4(rs, rd);
}

static void SqrtF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.sqrtFloat64x2(rs, rd);
}

static void CeilF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.ceilFloat32x4(rs, rd);
}

static void FloorF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.floorFloat32x4(rs, rd);
}

static void TruncF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.truncFloat32x4(rs, rd);
}

static void NearestF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.nearestFloat32x4(rs, rd);
}

static void CeilF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.ceilFloat64x2(rs, rd);
}

static void FloorF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.floorFloat64x2(rs, rd);
}

static void TruncF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.truncFloat64x2(rs, rd);
}

static void NearestF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.nearestFloat64x2(rs, rd);
}

static void NotV128(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.bitwiseNotSimd128(rs, rd);
}

static void ExtAddPairwiseI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.extAddPairwiseInt8x16(rs, rsd);
}

static void ExtAddPairwiseUI8x16(MacroAssembler& masm, RegV128 rs,
                                 RegV128 rsd) {
  masm.unsignedExtAddPairwiseInt8x16(rs, rsd);
}

static void ExtAddPairwiseI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.extAddPairwiseInt16x8(rs, rsd);
}

static void ExtAddPairwiseUI16x8(MacroAssembler& masm, RegV128 rs,
                                 RegV128 rsd) {
  masm.unsignedExtAddPairwiseInt16x8(rs, rsd);
}

static void ShiftOpMask(MacroAssembler& masm, SimdOp op, RegI32 in,
                        RegI32 out) {
  int32_t maskBits;

  masm.mov(in, out);
  if (MacroAssembler::MustMaskShiftCountSimd128(op, &maskBits)) {
    masm.and32(Imm32(maskBits), out);
  }
}

#  if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
static void ShiftLeftI8x16(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                           RegI32 temp1, RegV128 temp2) {
  ShiftOpMask(masm, SimdOp::I8x16Shl, rs, temp1);
  masm.leftShiftInt8x16(temp1, rsd, temp2);
}

static void ShiftLeftI16x8(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                           RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I16x8Shl, rs, temp);
  masm.leftShiftInt16x8(temp, rsd);
}

static void ShiftLeftI32x4(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                           RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I32x4Shl, rs, temp);
  masm.leftShiftInt32x4(temp, rsd);
}

static void ShiftLeftI64x2(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                           RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I64x2Shl, rs, temp);
  masm.leftShiftInt64x2(temp, rsd);
}

static void ShiftRightI8x16(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                            RegI32 temp1, RegV128 temp2) {
  ShiftOpMask(masm, SimdOp::I8x16ShrS, rs, temp1);
  masm.rightShiftInt8x16(temp1, rsd, temp2);
}

static void ShiftRightUI8x16(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                             RegI32 temp1, RegV128 temp2) {
  ShiftOpMask(masm, SimdOp::I8x16ShrU, rs, temp1);
  masm.unsignedRightShiftInt8x16(temp1, rsd, temp2);
}

static void ShiftRightI16x8(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                            RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I16x8ShrS, rs, temp);
  masm.rightShiftInt16x8(temp, rsd);
}

static void ShiftRightUI16x8(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                             RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I16x8ShrU, rs, temp);
  masm.unsignedRightShiftInt16x8(temp, rsd);
}

static void ShiftRightI32x4(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                            RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I32x4ShrS, rs, temp);
  masm.rightShiftInt32x4(temp, rsd);
}

static void ShiftRightUI32x4(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                             RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I32x4ShrU, rs, temp);
  masm.unsignedRightShiftInt32x4(temp, rsd);
}

static void ShiftRightUI64x2(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                             RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I64x2ShrU, rs, temp);
  masm.unsignedRightShiftInt64x2(temp, rsd);
}
#  elif defined(JS_CODEGEN_ARM64)
static void ShiftLeftI8x16(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                           RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I8x16Shl, rs, temp);
  masm.leftShiftInt8x16(rsd, temp, rsd);
}

static void ShiftLeftI16x8(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                           RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I16x8Shl, rs, temp);
  masm.leftShiftInt16x8(rsd, temp, rsd);
}

static void ShiftLeftI32x4(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                           RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I32x4Shl, rs, temp);
  masm.leftShiftInt32x4(rsd, temp, rsd);
}

static void ShiftLeftI64x2(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                           RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I64x2Shl, rs, temp);
  masm.leftShiftInt64x2(rsd, temp, rsd);
}

static void ShiftRightI8x16(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                            RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I8x16ShrS, rs, temp);
  masm.rightShiftInt8x16(rsd, temp, rsd);
}

static void ShiftRightUI8x16(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                             RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I8x16ShrU, rs, temp);
  masm.unsignedRightShiftInt8x16(rsd, temp, rsd);
}

static void ShiftRightI16x8(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                            RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I16x8ShrS, rs, temp);
  masm.rightShiftInt16x8(rsd, temp, rsd);
}

static void ShiftRightUI16x8(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                             RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I16x8ShrU, rs, temp);
  masm.unsignedRightShiftInt16x8(rsd, temp, rsd);
}

static void ShiftRightI32x4(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                            RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I32x4ShrS, rs, temp);
  masm.rightShiftInt32x4(rsd, temp, rsd);
}

static void ShiftRightUI32x4(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                             RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I32x4ShrU, rs, temp);
  masm.unsignedRightShiftInt32x4(rsd, temp, rsd);
}

static void ShiftRightI64x2(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                            RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I64x2ShrS, rs, temp);
  masm.rightShiftInt64x2(rsd, temp, rsd);
}

static void ShiftRightUI64x2(MacroAssembler& masm, RegI32 rs, RegV128 rsd,
                             RegI32 temp) {
  ShiftOpMask(masm, SimdOp::I64x2ShrU, rs, temp);
  masm.unsignedRightShiftInt64x2(rsd, temp, rsd);
}
#  endif

static void AverageUI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedAverageInt8x16(rs, rsd);
}

static void AverageUI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedAverageInt16x8(rs, rsd);
}

static void MinI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.minInt8x16(rs, rsd);
}

static void MinUI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedMinInt8x16(rs, rsd);
}

static void MaxI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.maxInt8x16(rs, rsd);
}

static void MaxUI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedMaxInt8x16(rs, rsd);
}

static void MinI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.minInt16x8(rs, rsd);
}

static void MinUI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedMinInt16x8(rs, rsd);
}

static void MaxI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.maxInt16x8(rs, rsd);
}

static void MaxUI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedMaxInt16x8(rs, rsd);
}

static void MinI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.minInt32x4(rs, rsd);
}

static void MinUI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedMinInt32x4(rs, rsd);
}

static void MaxI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.maxInt32x4(rs, rsd);
}

static void MaxUI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedMaxInt32x4(rs, rsd);
}

static void NarrowI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.narrowInt16x8(rs, rsd);
}

static void NarrowUI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedNarrowInt16x8(rs, rsd);
}

static void NarrowI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.narrowInt32x4(rs, rsd);
}

static void NarrowUI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.unsignedNarrowInt32x4(rs, rsd);
}

static void WidenLowI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.widenLowInt8x16(rs, rd);
}

static void WidenHighI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.widenHighInt8x16(rs, rd);
}

static void WidenLowUI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.unsignedWidenLowInt8x16(rs, rd);
}

static void WidenHighUI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.unsignedWidenHighInt8x16(rs, rd);
}

static void WidenLowI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.widenLowInt16x8(rs, rd);
}

static void WidenHighI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.widenHighInt16x8(rs, rd);
}

static void WidenLowUI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.unsignedWidenLowInt16x8(rs, rd);
}

static void WidenHighUI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.unsignedWidenHighInt16x8(rs, rd);
}

static void WidenLowI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.widenLowInt32x4(rs, rd);
}

static void WidenHighI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.widenHighInt32x4(rs, rd);
}

static void WidenLowUI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.unsignedWidenLowInt32x4(rs, rd);
}

static void WidenHighUI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.unsignedWidenHighInt32x4(rs, rd);
}

#  if defined(JS_CODEGEN_ARM64)
static void PopcntI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.popcntInt8x16(rs, rd);
}
#  else
static void PopcntI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rd,
                        RegV128 temp) {
  masm.popcntInt8x16(rs, rd, temp);
}
#  endif  // JS_CODEGEN_ARM64

static void AbsI8x16(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.absInt8x16(rs, rd);
}

static void AbsI16x8(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.absInt16x8(rs, rd);
}

static void AbsI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.absInt32x4(rs, rd);
}

static void AbsI64x2(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.absInt64x2(rs, rd);
}

static void ExtractLaneI8x16(MacroAssembler& masm, uint32_t laneIndex,
                             RegV128 rs, RegI32 rd) {
  masm.extractLaneInt8x16(laneIndex, rs, rd);
}

static void ExtractLaneUI8x16(MacroAssembler& masm, uint32_t laneIndex,
                              RegV128 rs, RegI32 rd) {
  masm.unsignedExtractLaneInt8x16(laneIndex, rs, rd);
}

static void ExtractLaneI16x8(MacroAssembler& masm, uint32_t laneIndex,
                             RegV128 rs, RegI32 rd) {
  masm.extractLaneInt16x8(laneIndex, rs, rd);
}

static void ExtractLaneUI16x8(MacroAssembler& masm, uint32_t laneIndex,
                              RegV128 rs, RegI32 rd) {
  masm.unsignedExtractLaneInt16x8(laneIndex, rs, rd);
}

static void ExtractLaneI32x4(MacroAssembler& masm, uint32_t laneIndex,
                             RegV128 rs, RegI32 rd) {
  masm.extractLaneInt32x4(laneIndex, rs, rd);
}

static void ExtractLaneI64x2(MacroAssembler& masm, uint32_t laneIndex,
                             RegV128 rs, RegI64 rd) {
  masm.extractLaneInt64x2(laneIndex, rs, rd);
}

static void ExtractLaneF32x4(MacroAssembler& masm, uint32_t laneIndex,
                             RegV128 rs, RegF32 rd) {
  masm.extractLaneFloat32x4(laneIndex, rs, rd);
}

static void ExtractLaneF64x2(MacroAssembler& masm, uint32_t laneIndex,
                             RegV128 rs, RegF64 rd) {
  masm.extractLaneFloat64x2(laneIndex, rs, rd);
}

static void ReplaceLaneI8x16(MacroAssembler& masm, uint32_t laneIndex,
                             RegI32 rs, RegV128 rsd) {
  masm.replaceLaneInt8x16(laneIndex, rs, rsd);
}

static void ReplaceLaneI16x8(MacroAssembler& masm, uint32_t laneIndex,
                             RegI32 rs, RegV128 rsd) {
  masm.replaceLaneInt16x8(laneIndex, rs, rsd);
}

static void ReplaceLaneI32x4(MacroAssembler& masm, uint32_t laneIndex,
                             RegI32 rs, RegV128 rsd) {
  masm.replaceLaneInt32x4(laneIndex, rs, rsd);
}

static void ReplaceLaneI64x2(MacroAssembler& masm, uint32_t laneIndex,
                             RegI64 rs, RegV128 rsd) {
  masm.replaceLaneInt64x2(laneIndex, rs, rsd);
}

static void ReplaceLaneF32x4(MacroAssembler& masm, uint32_t laneIndex,
                             RegF32 rs, RegV128 rsd) {
  masm.replaceLaneFloat32x4(laneIndex, rs, rsd);
}

static void ReplaceLaneF64x2(MacroAssembler& masm, uint32_t laneIndex,
                             RegF64 rs, RegV128 rsd) {
  masm.replaceLaneFloat64x2(laneIndex, rs, rsd);
}

static void SplatI8x16(MacroAssembler& masm, RegI32 rs, RegV128 rd) {
  masm.splatX16(rs, rd);
}

static void SplatI16x8(MacroAssembler& masm, RegI32 rs, RegV128 rd) {
  masm.splatX8(rs, rd);
}

static void SplatI32x4(MacroAssembler& masm, RegI32 rs, RegV128 rd) {
  masm.splatX4(rs, rd);
}

static void SplatI64x2(MacroAssembler& masm, RegI64 rs, RegV128 rd) {
  masm.splatX2(rs, rd);
}

static void SplatF32x4(MacroAssembler& masm, RegF32 rs, RegV128 rd) {
  masm.splatX4(rs, rd);
}

static void SplatF64x2(MacroAssembler& masm, RegF64 rs, RegV128 rd) {
  masm.splatX2(rs, rd);
}

// This is the same op independent of lanes: it tests for any nonzero bit.
static void AnyTrue(MacroAssembler& masm, RegV128 rs, RegI32 rd) {
  masm.anyTrueSimd128(rs, rd);
}

static void AllTrueI8x16(MacroAssembler& masm, RegV128 rs, RegI32 rd) {
  masm.allTrueInt8x16(rs, rd);
}

static void AllTrueI16x8(MacroAssembler& masm, RegV128 rs, RegI32 rd) {
  masm.allTrueInt16x8(rs, rd);
}

static void AllTrueI32x4(MacroAssembler& masm, RegV128 rs, RegI32 rd) {
  masm.allTrueInt32x4(rs, rd);
}

static void AllTrueI64x2(MacroAssembler& masm, RegV128 rs, RegI32 rd) {
  masm.allTrueInt64x2(rs, rd);
}

#  if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
static void BitmaskI8x16(MacroAssembler& masm, RegV128 rs, RegI32 rd) {
  masm.bitmaskInt8x16(rs, rd);
}

static void BitmaskI16x8(MacroAssembler& masm, RegV128 rs, RegI32 rd) {
  masm.bitmaskInt16x8(rs, rd);
}

static void BitmaskI32x4(MacroAssembler& masm, RegV128 rs, RegI32 rd) {
  masm.bitmaskInt32x4(rs, rd);
}

static void BitmaskI64x2(MacroAssembler& masm, RegV128 rs, RegI32 rd) {
  masm.bitmaskInt64x2(rs, rd);
}
#  elif defined(JS_CODEGEN_ARM64)
static void BitmaskI8x16(MacroAssembler& masm, RegV128 rs, RegI32 rd,
                         RegV128 temp) {
  masm.bitmaskInt8x16(rs, rd, temp);
}

static void BitmaskI16x8(MacroAssembler& masm, RegV128 rs, RegI32 rd,
                         RegV128 temp) {
  masm.bitmaskInt16x8(rs, rd, temp);
}

static void BitmaskI32x4(MacroAssembler& masm, RegV128 rs, RegI32 rd,
                         RegV128 temp) {
  masm.bitmaskInt32x4(rs, rd, temp);
}

static void BitmaskI64x2(MacroAssembler& masm, RegV128 rs, RegI32 rd,
                         RegV128 temp) {
  masm.bitmaskInt64x2(rs, rd, temp);
}
#  endif

static void Swizzle(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.swizzleInt8x16(rs, rsd);
}

static void ConvertI32x4ToF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.convertInt32x4ToFloat32x4(rs, rd);
}

static void ConvertUI32x4ToF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.unsignedConvertInt32x4ToFloat32x4(rs, rd);
}

static void ConvertF32x4ToI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.truncSatFloat32x4ToInt32x4(rs, rd);
}

#  if defined(JS_CODEGEN_ARM64)
static void ConvertF32x4ToUI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.unsignedTruncSatFloat32x4ToInt32x4(rs, rd);
}
#  else
static void ConvertF32x4ToUI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd,
                                 RegV128 temp) {
  masm.unsignedTruncSatFloat32x4ToInt32x4(rs, rd, temp);
}
#  endif  // JS_CODEGEN_ARM64

static void ConvertI32x4ToF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.convertInt32x4ToFloat64x2(rs, rd);
}

static void ConvertUI32x4ToF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.unsignedConvertInt32x4ToFloat64x2(rs, rd);
}

static void ConvertF64x2ToI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd,
                                RegV128 temp) {
  masm.truncSatFloat64x2ToInt32x4(rs, rd, temp);
}

static void ConvertF64x2ToUI32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd,
                                 RegV128 temp) {
  masm.unsignedTruncSatFloat64x2ToInt32x4(rs, rd, temp);
}

static void DemoteF64x2ToF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.convertFloat64x2ToFloat32x4(rs, rd);
}

static void PromoteF32x4ToF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rd) {
  masm.convertFloat32x4ToFloat64x2(rs, rd);
}

#  if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
static void BitselectV128(MacroAssembler& masm, RegV128 rhs, RegV128 control,
                          RegV128 lhsDest, RegV128 temp) {
  // Ideally, we would have temp=control, and we can probably get away with
  // just doing that, but don't worry about it yet.
  masm.bitwiseSelectSimd128(control, lhsDest, rhs, lhsDest, temp);
}
#  elif defined(JS_CODEGEN_ARM64)
static void BitselectV128(MacroAssembler& masm, RegV128 rhs, RegV128 control,
                          RegV128 lhsDest, RegV128 temp) {
  // The masm interface is not great for the baseline compiler here, but it's
  // optimal for Ion, so just work around it.
  masm.moveSimd128(control, temp);
  masm.bitwiseSelectSimd128(lhsDest, rhs, temp);
  masm.moveSimd128(temp, lhsDest);
}
#  endif

#  ifdef ENABLE_WASM_RELAXED_SIMD
static void RelaxedFmaF32x4(MacroAssembler& masm, RegV128 rs1, RegV128 rs2,
                            RegV128 rsd) {
  masm.fmaFloat32x4(rs1, rs2, rsd);
}

static void RelaxedFmsF32x4(MacroAssembler& masm, RegV128 rs1, RegV128 rs2,
                            RegV128 rsd) {
  masm.fmsFloat32x4(rs1, rs2, rsd);
}

static void RelaxedFmaF64x2(MacroAssembler& masm, RegV128 rs1, RegV128 rs2,
                            RegV128 rsd) {
  masm.fmaFloat64x2(rs1, rs2, rsd);
}

static void RelaxedFmsF64x2(MacroAssembler& masm, RegV128 rs1, RegV128 rs2,
                            RegV128 rsd) {
  masm.fmsFloat64x2(rs1, rs2, rsd);
}

static void RelaxedSwizzle(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.swizzleInt8x16Relaxed(rs, rsd);
}

static void RelaxedMinF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.minFloat32x4Relaxed(rs, rsd);
}

static void RelaxedMaxF32x4(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.maxFloat32x4Relaxed(rs, rsd);
}

static void RelaxedMinF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.minFloat64x2Relaxed(rs, rsd);
}

static void RelaxedMaxF64x2(MacroAssembler& masm, RegV128 rs, RegV128 rsd) {
  masm.maxFloat64x2Relaxed(rs, rsd);
}

static void RelaxedConvertF32x4ToI32x4(MacroAssembler& masm, RegV128 rs,
                                       RegV128 rd) {
  masm.truncSatFloat32x4ToInt32x4Relaxed(rs, rd);
}

static void RelaxedConvertF32x4ToUI32x4(MacroAssembler& masm, RegV128 rs,
                                        RegV128 rd) {
  masm.unsignedTruncSatFloat32x4ToInt32x4Relaxed(rs, rd);
}

static void RelaxedConvertF64x2ToI32x4(MacroAssembler& masm, RegV128 rs,
                                       RegV128 rd) {
  masm.truncSatFloat64x2ToInt32x4Relaxed(rs, rd);
}

static void RelaxedConvertF64x2ToUI32x4(MacroAssembler& masm, RegV128 rs,
                                        RegV128 rd) {
  masm.unsignedTruncSatFloat64x2ToInt32x4Relaxed(rs, rd);
}
#  endif

void BaseCompiler::emitVectorAndNot() {
  // We want x & ~y but the available operation is ~x & y, so reverse the
  // operands.
  RegV128 r, rs;
  pop2xV128(&r, &rs);
  masm.bitwiseNotAndSimd128(r, rs);
  freeV128(r);
  pushV128(rs);
}

bool BaseCompiler::emitLoadSplat(Scalar::Type viewType) {
  LinearMemoryAddress<Nothing> addr;
  if (!iter_.readLoadSplat(Scalar::byteSize(viewType), &addr)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset());
  loadSplat(&access);
  return true;
}

bool BaseCompiler::emitLoadZero(Scalar::Type viewType) {
  // LoadZero has the structure of LoadSplat, so reuse the reader.
  LinearMemoryAddress<Nothing> addr;
  if (!iter_.readLoadSplat(Scalar::byteSize(viewType), &addr)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset());
  loadZero(&access);
  return true;
}

bool BaseCompiler::emitLoadExtend(Scalar::Type viewType) {
  LinearMemoryAddress<Nothing> addr;
  if (!iter_.readLoadExtend(&addr)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  MemoryAccessDesc access(Scalar::Int64, addr.align, addr.offset,
                          bytecodeOffset());
  loadExtend(&access, viewType);
  return true;
}

bool BaseCompiler::emitLoadLane(uint32_t laneSize) {
  Nothing nothing;
  LinearMemoryAddress<Nothing> addr;
  uint32_t laneIndex;
  if (!iter_.readLoadLane(laneSize, &addr, &laneIndex, &nothing)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  Scalar::Type viewType;
  switch (laneSize) {
    case 1:
      viewType = Scalar::Uint8;
      break;
    case 2:
      viewType = Scalar::Uint16;
      break;
    case 4:
      viewType = Scalar::Int32;
      break;
    case 8:
      viewType = Scalar::Int64;
      break;
    default:
      MOZ_CRASH("unsupported laneSize");
  }
  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset());
  loadLane(&access, laneIndex);
  return true;
}

bool BaseCompiler::emitStoreLane(uint32_t laneSize) {
  Nothing nothing;
  LinearMemoryAddress<Nothing> addr;
  uint32_t laneIndex;
  if (!iter_.readStoreLane(laneSize, &addr, &laneIndex, &nothing)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  Scalar::Type viewType;
  switch (laneSize) {
    case 1:
      viewType = Scalar::Uint8;
      break;
    case 2:
      viewType = Scalar::Uint16;
      break;
    case 4:
      viewType = Scalar::Int32;
      break;
    case 8:
      viewType = Scalar::Int64;
      break;
    default:
      MOZ_CRASH("unsupported laneSize");
  }
  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset());
  storeLane(&access, laneIndex);
  return true;
}

bool BaseCompiler::emitVectorShuffle() {
  Nothing unused_a, unused_b;
  V128 shuffleMask;

  if (!iter_.readVectorShuffle(&unused_a, &unused_b, &shuffleMask)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  RegV128 rd, rs;
  pop2xV128(&rd, &rs);

  bool emitShuffle = true;

#  ifdef ENABLE_WASM_SIMD_WORMHOLE
  if (moduleEnv_.simdWormholeEnabled() && IsWormholeTrigger(shuffleMask)) {
    emitShuffle = false;
    switch (shuffleMask.bytes[15]) {
      case 0:
        masm.loadConstantSimd128(WormholeSignature(), rd);
        break;
#    if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
      case 1:
        masm.vpmaddubsw(rs, rd, rd);
        break;
      case 2:
        masm.vpmaddwd(Operand(rs), rd, rd);
        break;
#    endif
      default:
        return iter_.fail("Unrecognized wormhole opcode");
    }
  }
#  endif

  if (emitShuffle) {
    masm.shuffleInt8x16(shuffleMask.bytes, rs, rd);
  }

  freeV128(rs);
  pushV128(rd);

  return true;
}

#  if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
bool BaseCompiler::emitVectorShiftRightI64x2() {
  Nothing unused_a, unused_b;

  if (!iter_.readVectorShift(&unused_a, &unused_b)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  RegI32 count = popI32RhsForShiftI64();
  RegV128 lhsDest = popV128();
  RegI64 tmp = needI64();
  masm.and32(Imm32(63), count);
  masm.extractLaneInt64x2(0, lhsDest, tmp);
  masm.rshift64Arithmetic(count, tmp);
  masm.replaceLaneInt64x2(0, tmp, lhsDest);
  masm.extractLaneInt64x2(1, lhsDest, tmp);
  masm.rshift64Arithmetic(count, tmp);
  masm.replaceLaneInt64x2(1, tmp, lhsDest);
  freeI64(tmp);
  freeI32(count);
  pushV128(lhsDest);

  return true;
}
#  endif
#endif  // ENABLE_WASM_SIMD

#ifdef ENABLE_WASM_RELAXED_SIMD
bool BaseCompiler::emitVectorLaneSelect() {
  Nothing unused_a, unused_b, unused_c;

  if (!iter_.readTernary(ValType::V128, &unused_a, &unused_b, &unused_c)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

#  if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
  RegV128 mask = popV128(RegV128(vmm0));
  RegV128 rhsDest = popV128();
  RegV128 lhs = popV128();
  masm.laneSelectSimd128(mask, rhsDest, lhs);
  freeV128(lhs);
  freeV128(mask);
  pushV128(rhsDest);
#  elif defined(JS_CODEGEN_ARM64)
  RegV128 maskDest = popV128();
  RegV128 rhs = popV128();
  RegV128 lhs = popV128();
  masm.laneSelectSimd128(maskDest, lhs, rhs, maskDest);
  freeV128(lhs);
  freeV128(rhs);
  pushV128(maskDest);
#  endif

  return true;
}
#endif  // ENABLE_WASM_RELAXED_SIMD

//////////////////////////////////////////////////////////////////////////////
//
// "Intrinsics" - magically imported functions for internal use.

bool BaseCompiler::emitIntrinsic(IntrinsicOp op) {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();
  const Intrinsic& intrinsic = Intrinsic::getFromOp(op);

  BaseNothingVector params;
  if (!iter_.readIntrinsic(intrinsic, &params)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  // The final parameter of an intrinsic is implicitly the heap base
  pushHeapBase();

  // Call the intrinsic
  return emitInstanceCall(lineOrBytecode, intrinsic.signature);
}

//////////////////////////////////////////////////////////////////////////////
//
// Function bodies - main opcode dispatch loop.

bool BaseCompiler::emitBody() {
  AutoCreatedBy acb(masm, "(wasm)BaseCompiler::emitBody");

  MOZ_ASSERT(stackMapGenerator_.framePushedAtEntryToBody.isSome());

  if (!iter_.startFunction(func_.index)) {
    return false;
  }

  initControl(controlItem(), ResultType::Empty());

  for (;;) {
    Nothing unused_a, unused_b, unused_c;
    (void)unused_a;
    (void)unused_b;
    (void)unused_c;

#ifdef DEBUG
    performRegisterLeakCheck();
    assertStackInvariants();
#endif

#define dispatchBinary0(doEmit, type)             \
  iter_.readBinary(type, &unused_a, &unused_b) && \
      (deadCode_ || (doEmit(), true))

#define dispatchBinary1(arg1, type)               \
  iter_.readBinary(type, &unused_a, &unused_b) && \
      (deadCode_ || (emitBinop(arg1), true))

#define dispatchBinary2(arg1, arg2, type)         \
  iter_.readBinary(type, &unused_a, &unused_b) && \
      (deadCode_ || (emitBinop(arg1, arg2), true))

#define dispatchBinary3(arg1, arg2, arg3, type)   \
  iter_.readBinary(type, &unused_a, &unused_b) && \
      (deadCode_ || (emitBinop(arg1, arg2, arg3), true))

#define dispatchUnary0(doEmit, type) \
  iter_.readUnary(type, &unused_a) && (deadCode_ || (doEmit(), true))

#define dispatchUnary1(arg1, type) \
  iter_.readUnary(type, &unused_a) && (deadCode_ || (emitUnop(arg1), true))

#define dispatchUnary2(arg1, arg2, type) \
  iter_.readUnary(type, &unused_a) &&    \
      (deadCode_ || (emitUnop(arg1, arg2), true))

#define dispatchTernary1(arg1, type)                          \
  iter_.readTernary(type, &unused_a, &unused_b, &unused_c) && \
      (deadCode_ || (emitTernary(arg1), true))

#define dispatchComparison0(doEmit, operandType, compareOp)  \
  iter_.readComparison(operandType, &unused_a, &unused_b) && \
      (deadCode_ || (doEmit(compareOp, operandType), true))

#define dispatchConversion0(doEmit, inType, outType)  \
  iter_.readConversion(inType, outType, &unused_a) && \
      (deadCode_ || (doEmit(), true))

#define dispatchConversion1(arg1, inType, outType)    \
  iter_.readConversion(inType, outType, &unused_a) && \
      (deadCode_ || (emitUnop(arg1), true))

#define dispatchConversionOOM(doEmit, inType, outType) \
  iter_.readConversion(inType, outType, &unused_a) && (deadCode_ || doEmit())

#define dispatchCalloutConversionOOM(doEmit, symbol, inType, outType) \
  iter_.readConversion(inType, outType, &unused_a) &&                 \
      (deadCode_ || doEmit(symbol, inType, outType))

#define dispatchIntDivCallout(doEmit, symbol, type) \
  iter_.readBinary(type, &unused_a, &unused_b) &&   \
      (deadCode_ || doEmit(symbol, type))

#define dispatchVectorBinary(op)                           \
  iter_.readBinary(ValType::V128, &unused_a, &unused_b) && \
      (deadCode_ || (emitBinop(op), true))

#define dispatchVectorUnary(op)                \
  iter_.readUnary(ValType::V128, &unused_a) && \
      (deadCode_ || (emitUnop(op), true))

#define dispatchVectorComparison(op, compareOp)            \
  iter_.readBinary(ValType::V128, &unused_a, &unused_b) && \
      (deadCode_ || (emitBinop(compareOp, op), true))

#define dispatchVectorVariableShift(op)          \
  iter_.readVectorShift(&unused_a, &unused_b) && \
      (deadCode_ || (emitBinop(op), true))

#define dispatchExtractLane(op, outType, laneLimit)                   \
  iter_.readExtractLane(outType, laneLimit, &laneIndex, &unused_a) && \
      (deadCode_ || (emitUnop(laneIndex, op), true))

#define dispatchReplaceLane(op, inType, laneLimit)                \
  iter_.readReplaceLane(inType, laneLimit, &laneIndex, &unused_a, \
                        &unused_b) &&                             \
      (deadCode_ || (emitBinop(laneIndex, op), true))

#define dispatchSplat(op, inType)                           \
  iter_.readConversion(inType, ValType::V128, &unused_a) && \
      (deadCode_ || (emitUnop(op), true))

#define dispatchVectorReduction(op)                               \
  iter_.readConversion(ValType::V128, ValType::I32, &unused_a) && \
      (deadCode_ || (emitUnop(op), true))

#ifdef DEBUG
    // Check that the number of ref-typed entries in the operand stack matches
    // reality.
#  define CHECK_POINTER_COUNT                                             \
    do {                                                                  \
      MOZ_ASSERT(countMemRefsOnStk() == stackMapGenerator_.memRefsOnStk); \
    } while (0)
#else
#  define CHECK_POINTER_COUNT \
    do {                      \
    } while (0)
#endif

#define CHECK(E) \
  if (!(E)) return false
#define NEXT()           \
  {                      \
    CHECK_POINTER_COUNT; \
    continue;            \
  }
#define CHECK_NEXT(E)     \
  if (!(E)) return false; \
  {                       \
    CHECK_POINTER_COUNT;  \
    continue;             \
  }

    // Opcodes that push more than MaxPushesPerOpcode (anything with multiple
    // results) will perform additional reservation.
    CHECK(stk_.reserve(stk_.length() + MaxPushesPerOpcode));

    OpBytes op{};
    CHECK(iter_.readOp(&op));

    // When compilerEnv_.debugEnabled(), every operator has a breakpoint site
    // except Op::End.
    if (compilerEnv_.debugEnabled() && op.b0 != (uint16_t)Op::End) {
      // TODO sync only registers that can be clobbered by the exit
      // prologue/epilogue or disable these registers for use in
      // baseline compiler when compilerEnv_.debugEnabled() is set.
      sync();

      insertBreakablePoint(CallSiteDesc::Breakpoint);
      if (!createStackMap("debug: per-insn breakpoint")) {
        return false;
      }
    }

    // Going below framePushedAtEntryToBody would imply that we've
    // popped off the machine stack, part of the frame created by
    // beginFunction().
    MOZ_ASSERT(masm.framePushed() >=
               stackMapGenerator_.framePushedAtEntryToBody.value());

    // At this point we're definitely not generating code for a function call.
    MOZ_ASSERT(
        stackMapGenerator_.framePushedExcludingOutboundCallArgs.isNothing());

    switch (op.b0) {
      case uint16_t(Op::End):
        if (!emitEnd()) {
          return false;
        }
        if (iter_.controlStackEmpty()) {
          return true;
        }
        NEXT();

      // Control opcodes
      case uint16_t(Op::Nop):
        CHECK_NEXT(iter_.readNop());
      case uint16_t(Op::Drop):
        CHECK_NEXT(emitDrop());
      case uint16_t(Op::Block):
        CHECK_NEXT(emitBlock());
      case uint16_t(Op::Loop):
        CHECK_NEXT(emitLoop());
      case uint16_t(Op::If):
        CHECK_NEXT(emitIf());
      case uint16_t(Op::Else):
        CHECK_NEXT(emitElse());
#ifdef ENABLE_WASM_EXCEPTIONS
      case uint16_t(Op::Try):
        if (!moduleEnv_.exceptionsEnabled()) {
          return iter_.unrecognizedOpcode(&op);
        }
        CHECK_NEXT(emitTry());
      case uint16_t(Op::Catch):
        if (!moduleEnv_.exceptionsEnabled()) {
          return iter_.unrecognizedOpcode(&op);
        }
        CHECK_NEXT(emitCatch());
      case uint16_t(Op::CatchAll):
        if (!moduleEnv_.exceptionsEnabled()) {
          return iter_.unrecognizedOpcode(&op);
        }
        CHECK_NEXT(emitCatchAll());
      case uint16_t(Op::Delegate):
        if (!moduleEnv_.exceptionsEnabled()) {
          return iter_.unrecognizedOpcode(&op);
        }
        CHECK(emitDelegate());
        iter_.popDelegate();
        NEXT();
      case uint16_t(Op::Throw):
        if (!moduleEnv_.exceptionsEnabled()) {
          return iter_.unrecognizedOpcode(&op);
        }
        CHECK_NEXT(emitThrow());
      case uint16_t(Op::Rethrow):
        if (!moduleEnv_.exceptionsEnabled()) {
          return iter_.unrecognizedOpcode(&op);
        }
        CHECK_NEXT(emitRethrow());
#endif
      case uint16_t(Op::Br):
        CHECK_NEXT(emitBr());
      case uint16_t(Op::BrIf):
        CHECK_NEXT(emitBrIf());
      case uint16_t(Op::BrTable):
        CHECK_NEXT(emitBrTable());
      case uint16_t(Op::Return):
        CHECK_NEXT(emitReturn());
      case uint16_t(Op::Unreachable):
        CHECK(iter_.readUnreachable());
        if (!deadCode_) {
          trap(Trap::Unreachable);
          deadCode_ = true;
        }
        NEXT();

      // Calls
      case uint16_t(Op::Call):
        CHECK_NEXT(emitCall());
      case uint16_t(Op::CallIndirect):
        CHECK_NEXT(emitCallIndirect());

      // Locals and globals
      case uint16_t(Op::LocalGet):
        CHECK_NEXT(emitGetLocal());
      case uint16_t(Op::LocalSet):
        CHECK_NEXT(emitSetLocal());
      case uint16_t(Op::LocalTee):
        CHECK_NEXT(emitTeeLocal());
      case uint16_t(Op::GlobalGet):
        CHECK_NEXT(emitGetGlobal());
      case uint16_t(Op::GlobalSet):
        CHECK_NEXT(emitSetGlobal());
      case uint16_t(Op::TableGet):
        CHECK_NEXT(emitTableGet());
      case uint16_t(Op::TableSet):
        CHECK_NEXT(emitTableSet());

      // Select
      case uint16_t(Op::SelectNumeric):
        CHECK_NEXT(emitSelect(/*typed*/ false));
      case uint16_t(Op::SelectTyped):
        CHECK_NEXT(emitSelect(/*typed*/ true));

      // I32
      case uint16_t(Op::I32Const): {
        int32_t i32;
        CHECK(iter_.readI32Const(&i32));
        if (!deadCode_) {
          pushI32(i32);
        }
        NEXT();
      }
      case uint16_t(Op::I32Add):
        CHECK_NEXT(dispatchBinary2(AddI32, AddImmI32, ValType::I32));
      case uint16_t(Op::I32Sub):
        CHECK_NEXT(dispatchBinary2(SubI32, SubImmI32, ValType::I32));
      case uint16_t(Op::I32Mul):
        CHECK_NEXT(dispatchBinary1(MulI32, ValType::I32));
      case uint16_t(Op::I32DivS):
        CHECK_NEXT(dispatchBinary0(emitQuotientI32, ValType::I32));
      case uint16_t(Op::I32DivU):
        CHECK_NEXT(dispatchBinary0(emitQuotientU32, ValType::I32));
      case uint16_t(Op::I32RemS):
        CHECK_NEXT(dispatchBinary0(emitRemainderI32, ValType::I32));
      case uint16_t(Op::I32RemU):
        CHECK_NEXT(dispatchBinary0(emitRemainderU32, ValType::I32));
      case uint16_t(Op::I32Eqz):
        CHECK_NEXT(dispatchConversion0(emitEqzI32, ValType::I32, ValType::I32));
      case uint16_t(Op::I32TruncF32S):
        CHECK_NEXT(dispatchConversionOOM(emitTruncateF32ToI32<0>, ValType::F32,
                                         ValType::I32));
      case uint16_t(Op::I32TruncF32U):
        CHECK_NEXT(dispatchConversionOOM(emitTruncateF32ToI32<TRUNC_UNSIGNED>,
                                         ValType::F32, ValType::I32));
      case uint16_t(Op::I32TruncF64S):
        CHECK_NEXT(dispatchConversionOOM(emitTruncateF64ToI32<0>, ValType::F64,
                                         ValType::I32));
      case uint16_t(Op::I32TruncF64U):
        CHECK_NEXT(dispatchConversionOOM(emitTruncateF64ToI32<TRUNC_UNSIGNED>,
                                         ValType::F64, ValType::I32));
      case uint16_t(Op::I32WrapI64):
        CHECK_NEXT(
            dispatchConversion1(WrapI64ToI32, ValType::I64, ValType::I32));
      case uint16_t(Op::I32ReinterpretF32):
        CHECK_NEXT(dispatchConversion1(ReinterpretF32AsI32, ValType::F32,
                                       ValType::I32));
      case uint16_t(Op::I32Clz):
        CHECK_NEXT(dispatchUnary1(ClzI32, ValType::I32));
      case uint16_t(Op::I32Ctz):
        CHECK_NEXT(dispatchUnary1(CtzI32, ValType::I32));
      case uint16_t(Op::I32Popcnt):
        CHECK_NEXT(dispatchUnary2(PopcntI32, PopcntTemp, ValType::I32));
      case uint16_t(Op::I32Or):
        CHECK_NEXT(dispatchBinary2(OrI32, OrImmI32, ValType::I32));
      case uint16_t(Op::I32And):
        CHECK_NEXT(dispatchBinary2(AndI32, AndImmI32, ValType::I32));
      case uint16_t(Op::I32Xor):
        CHECK_NEXT(dispatchBinary2(XorI32, XorImmI32, ValType::I32));
      case uint16_t(Op::I32Shl):
        CHECK_NEXT(dispatchBinary3(
            ShlI32, ShlImmI32, &BaseCompiler::popI32RhsForShift, ValType::I32));
      case uint16_t(Op::I32ShrS):
        CHECK_NEXT(dispatchBinary3(
            ShrI32, ShrImmI32, &BaseCompiler::popI32RhsForShift, ValType::I32));
      case uint16_t(Op::I32ShrU):
        CHECK_NEXT(dispatchBinary3(ShrUI32, ShrUImmI32,
                                   &BaseCompiler::popI32RhsForShift,
                                   ValType::I32));
      case uint16_t(Op::I32Load8S):
        CHECK_NEXT(emitLoad(ValType::I32, Scalar::Int8));
      case uint16_t(Op::I32Load8U):
        CHECK_NEXT(emitLoad(ValType::I32, Scalar::Uint8));
      case uint16_t(Op::I32Load16S):
        CHECK_NEXT(emitLoad(ValType::I32, Scalar::Int16));
      case uint16_t(Op::I32Load16U):
        CHECK_NEXT(emitLoad(ValType::I32, Scalar::Uint16));
      case uint16_t(Op::I32Load):
        CHECK_NEXT(emitLoad(ValType::I32, Scalar::Int32));
      case uint16_t(Op::I32Store8):
        CHECK_NEXT(emitStore(ValType::I32, Scalar::Int8));
      case uint16_t(Op::I32Store16):
        CHECK_NEXT(emitStore(ValType::I32, Scalar::Int16));
      case uint16_t(Op::I32Store):
        CHECK_NEXT(emitStore(ValType::I32, Scalar::Int32));
      case uint16_t(Op::I32Rotr):
        CHECK_NEXT(dispatchBinary3(RotrI32, RotrImmI32,
                                   &BaseCompiler::popI32RhsForRotate,
                                   ValType::I32));
      case uint16_t(Op::I32Rotl):
        CHECK_NEXT(dispatchBinary3(RotlI32, RotlImmI32,
                                   &BaseCompiler::popI32RhsForRotate,
                                   ValType::I32));

      // I64
      case uint16_t(Op::I64Const): {
        int64_t i64;
        CHECK(iter_.readI64Const(&i64));
        if (!deadCode_) {
          pushI64(i64);
        }
        NEXT();
      }
      case uint16_t(Op::I64Add):
        CHECK_NEXT(dispatchBinary2(AddI64, AddImmI64, ValType::I64));
      case uint16_t(Op::I64Sub):
        CHECK_NEXT(dispatchBinary2(SubI64, SubImmI64, ValType::I64));
      case uint16_t(Op::I64Mul):
        CHECK_NEXT(dispatchBinary0(emitMultiplyI64, ValType::I64));
      case uint16_t(Op::I64DivS):
#ifdef RABALDR_INT_DIV_I64_CALLOUT
        CHECK_NEXT(dispatchIntDivCallout(
            emitDivOrModI64BuiltinCall, SymbolicAddress::DivI64, ValType::I64));
#else
        CHECK_NEXT(dispatchBinary0(emitQuotientI64, ValType::I64));
#endif
      case uint16_t(Op::I64DivU):
#ifdef RABALDR_INT_DIV_I64_CALLOUT
        CHECK_NEXT(dispatchIntDivCallout(emitDivOrModI64BuiltinCall,
                                         SymbolicAddress::UDivI64,
                                         ValType::I64));
#else
        CHECK_NEXT(dispatchBinary0(emitQuotientU64, ValType::I64));
#endif
      case uint16_t(Op::I64RemS):
#ifdef RABALDR_INT_DIV_I64_CALLOUT
        CHECK_NEXT(dispatchIntDivCallout(
            emitDivOrModI64BuiltinCall, SymbolicAddress::ModI64, ValType::I64));
#else
        CHECK_NEXT(dispatchBinary0(emitRemainderI64, ValType::I64));
#endif
      case uint16_t(Op::I64RemU):
#ifdef RABALDR_INT_DIV_I64_CALLOUT
        CHECK_NEXT(dispatchIntDivCallout(emitDivOrModI64BuiltinCall,
                                         SymbolicAddress::UModI64,
                                         ValType::I64));
#else
        CHECK_NEXT(dispatchBinary0(emitRemainderU64, ValType::I64));
#endif
      case uint16_t(Op::I64TruncF32S):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
        CHECK_NEXT(
            dispatchCalloutConversionOOM(emitConvertFloatingToInt64Callout,
                                         SymbolicAddress::TruncateDoubleToInt64,
                                         ValType::F32, ValType::I64));
#else
        CHECK_NEXT(dispatchConversionOOM(emitTruncateF32ToI64<0>, ValType::F32,
                                         ValType::I64));
#endif
      case uint16_t(Op::I64TruncF32U):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
        CHECK_NEXT(dispatchCalloutConversionOOM(
            emitConvertFloatingToInt64Callout,
            SymbolicAddress::TruncateDoubleToUint64, ValType::F32,
            ValType::I64));
#else
        CHECK_NEXT(dispatchConversionOOM(emitTruncateF32ToI64<TRUNC_UNSIGNED>,
                                         ValType::F32, ValType::I64));
#endif
      case uint16_t(Op::I64TruncF64S):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
        CHECK_NEXT(
            dispatchCalloutConversionOOM(emitConvertFloatingToInt64Callout,
                                         SymbolicAddress::TruncateDoubleToInt64,
                                         ValType::F64, ValType::I64));
#else
        CHECK_NEXT(dispatchConversionOOM(emitTruncateF64ToI64<0>, ValType::F64,
                                         ValType::I64));
#endif
      case uint16_t(Op::I64TruncF64U):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
        CHECK_NEXT(dispatchCalloutConversionOOM(
            emitConvertFloatingToInt64Callout,
            SymbolicAddress::TruncateDoubleToUint64, ValType::F64,
            ValType::I64));
#else
        CHECK_NEXT(dispatchConversionOOM(emitTruncateF64ToI64<TRUNC_UNSIGNED>,
                                         ValType::F64, ValType::I64));
#endif
      case uint16_t(Op::I64ExtendI32S):
        CHECK_NEXT(dispatchConversion0(emitExtendI32ToI64, ValType::I32,
                                       ValType::I64));
      case uint16_t(Op::I64ExtendI32U):
        CHECK_NEXT(dispatchConversion0(emitExtendU32ToI64, ValType::I32,
                                       ValType::I64));
      case uint16_t(Op::I64ReinterpretF64):
        CHECK_NEXT(dispatchConversion1(ReinterpretF64AsI64, ValType::F64,
                                       ValType::I64));
      case uint16_t(Op::I64Or):
        CHECK_NEXT(dispatchBinary2(OrI64, OrImmI64, ValType::I64));
      case uint16_t(Op::I64And):
        CHECK_NEXT(dispatchBinary2(AndI64, AndImmI64, ValType::I64));
      case uint16_t(Op::I64Xor):
        CHECK_NEXT(dispatchBinary2(XorI64, XorImmI64, ValType::I64));
      case uint16_t(Op::I64Shl):
        CHECK_NEXT(dispatchBinary3(
            ShlI64, ShlImmI64, &BaseCompiler::popI64RhsForShift, ValType::I64));
      case uint16_t(Op::I64ShrS):
        CHECK_NEXT(dispatchBinary3(
            ShrI64, ShrImmI64, &BaseCompiler::popI64RhsForShift, ValType::I64));
      case uint16_t(Op::I64ShrU):
        CHECK_NEXT(dispatchBinary3(ShrUI64, ShrUImmI64,
                                   &BaseCompiler::popI64RhsForShift,
                                   ValType::I64));
      case uint16_t(Op::I64Rotr):
        CHECK_NEXT(dispatchBinary0(emitRotrI64, ValType::I64));
      case uint16_t(Op::I64Rotl):
        CHECK_NEXT(dispatchBinary0(emitRotlI64, ValType::I64));
      case uint16_t(Op::I64Clz):
        CHECK_NEXT(dispatchUnary1(ClzI64, ValType::I64));
      case uint16_t(Op::I64Ctz):
        CHECK_NEXT(dispatchUnary1(CtzI64, ValType::I64));
      case uint16_t(Op::I64Popcnt):
        CHECK_NEXT(dispatchUnary2(PopcntI64, PopcntTemp, ValType::I64));
      case uint16_t(Op::I64Eqz):
        CHECK_NEXT(dispatchConversion0(emitEqzI64, ValType::I64, ValType::I32));
      case uint16_t(Op::I64Load8S):
        CHECK_NEXT(emitLoad(ValType::I64, Scalar::Int8));
      case uint16_t(Op::I64Load16S):
        CHECK_NEXT(emitLoad(ValType::I64, Scalar::Int16));
      case uint16_t(Op::I64Load32S):
        CHECK_NEXT(emitLoad(ValType::I64, Scalar::Int32));
      case uint16_t(Op::I64Load8U):
        CHECK_NEXT(emitLoad(ValType::I64, Scalar::Uint8));
      case uint16_t(Op::I64Load16U):
        CHECK_NEXT(emitLoad(ValType::I64, Scalar::Uint16));
      case uint16_t(Op::I64Load32U):
        CHECK_NEXT(emitLoad(ValType::I64, Scalar::Uint32));
      case uint16_t(Op::I64Load):
        CHECK_NEXT(emitLoad(ValType::I64, Scalar::Int64));
      case uint16_t(Op::I64Store8):
        CHECK_NEXT(emitStore(ValType::I64, Scalar::Int8));
      case uint16_t(Op::I64Store16):
        CHECK_NEXT(emitStore(ValType::I64, Scalar::Int16));
      case uint16_t(Op::I64Store32):
        CHECK_NEXT(emitStore(ValType::I64, Scalar::Int32));
      case uint16_t(Op::I64Store):
        CHECK_NEXT(emitStore(ValType::I64, Scalar::Int64));

      // F32
      case uint16_t(Op::F32Const): {
        float f32;
        CHECK(iter_.readF32Const(&f32));
        if (!deadCode_) {
          pushF32(f32);
        }
        NEXT();
      }
      case uint16_t(Op::F32Add):
        CHECK_NEXT(dispatchBinary1(AddF32, ValType::F32))
      case uint16_t(Op::F32Sub):
        CHECK_NEXT(dispatchBinary1(SubF32, ValType::F32));
      case uint16_t(Op::F32Mul):
        CHECK_NEXT(dispatchBinary1(MulF32, ValType::F32));
      case uint16_t(Op::F32Div):
        CHECK_NEXT(dispatchBinary1(DivF32, ValType::F32));
      case uint16_t(Op::F32Min):
        CHECK_NEXT(dispatchBinary1(MinF32, ValType::F32));
      case uint16_t(Op::F32Max):
        CHECK_NEXT(dispatchBinary1(MaxF32, ValType::F32));
      case uint16_t(Op::F32Neg):
        CHECK_NEXT(dispatchUnary1(NegateF32, ValType::F32));
      case uint16_t(Op::F32Abs):
        CHECK_NEXT(dispatchUnary1(AbsF32, ValType::F32));
      case uint16_t(Op::F32Sqrt):
        CHECK_NEXT(dispatchUnary1(SqrtF32, ValType::F32));
      case uint16_t(Op::F32Ceil):
        CHECK_NEXT(
            emitUnaryMathBuiltinCall(SymbolicAddress::CeilF, ValType::F32));
      case uint16_t(Op::F32Floor):
        CHECK_NEXT(
            emitUnaryMathBuiltinCall(SymbolicAddress::FloorF, ValType::F32));
      case uint16_t(Op::F32DemoteF64):
        CHECK_NEXT(
            dispatchConversion1(ConvertF64ToF32, ValType::F64, ValType::F32));
      case uint16_t(Op::F32ConvertI32S):
        CHECK_NEXT(
            dispatchConversion1(ConvertI32ToF32, ValType::I32, ValType::F32));
      case uint16_t(Op::F32ConvertI32U):
        CHECK_NEXT(
            dispatchConversion1(ConvertU32ToF32, ValType::I32, ValType::F32));
      case uint16_t(Op::F32ConvertI64S):
#ifdef RABALDR_I64_TO_FLOAT_CALLOUT
        CHECK_NEXT(dispatchCalloutConversionOOM(
            emitConvertInt64ToFloatingCallout, SymbolicAddress::Int64ToFloat32,
            ValType::I64, ValType::F32));
#else
        CHECK_NEXT(
            dispatchConversion1(ConvertI64ToF32, ValType::I64, ValType::F32));
#endif
      case uint16_t(Op::F32ConvertI64U):
#ifdef RABALDR_I64_TO_FLOAT_CALLOUT
        CHECK_NEXT(dispatchCalloutConversionOOM(
            emitConvertInt64ToFloatingCallout, SymbolicAddress::Uint64ToFloat32,
            ValType::I64, ValType::F32));
#else
        CHECK_NEXT(dispatchConversion0(emitConvertU64ToF32, ValType::I64,
                                       ValType::F32));
#endif
      case uint16_t(Op::F32ReinterpretI32):
        CHECK_NEXT(dispatchConversion1(ReinterpretI32AsF32, ValType::I32,
                                       ValType::F32));
      case uint16_t(Op::F32Load):
        CHECK_NEXT(emitLoad(ValType::F32, Scalar::Float32));
      case uint16_t(Op::F32Store):
        CHECK_NEXT(emitStore(ValType::F32, Scalar::Float32));
      case uint16_t(Op::F32CopySign):
        CHECK_NEXT(dispatchBinary1(CopysignF32, ValType::F32));
      case uint16_t(Op::F32Nearest):
        CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::NearbyIntF,
                                            ValType::F32));
      case uint16_t(Op::F32Trunc):
        CHECK_NEXT(
            emitUnaryMathBuiltinCall(SymbolicAddress::TruncF, ValType::F32));

      // F64
      case uint16_t(Op::F64Const): {
        double f64;
        CHECK(iter_.readF64Const(&f64));
        if (!deadCode_) {
          pushF64(f64);
        }
        NEXT();
      }
      case uint16_t(Op::F64Add):
        CHECK_NEXT(dispatchBinary1(AddF64, ValType::F64))
      case uint16_t(Op::F64Sub):
        CHECK_NEXT(dispatchBinary1(SubF64, ValType::F64));
      case uint16_t(Op::F64Mul):
        CHECK_NEXT(dispatchBinary1(MulF64, ValType::F64));
      case uint16_t(Op::F64Div):
        CHECK_NEXT(dispatchBinary1(DivF64, ValType::F64));
      case uint16_t(Op::F64Min):
        CHECK_NEXT(dispatchBinary1(MinF64, ValType::F64));
      case uint16_t(Op::F64Max):
        CHECK_NEXT(dispatchBinary1(MaxF64, ValType::F64));
      case uint16_t(Op::F64Neg):
        CHECK_NEXT(dispatchUnary1(NegateF64, ValType::F64));
      case uint16_t(Op::F64Abs):
        CHECK_NEXT(dispatchUnary1(AbsF64, ValType::F64));
      case uint16_t(Op::F64Sqrt):
        CHECK_NEXT(dispatchUnary1(SqrtF64, ValType::F64));
      case uint16_t(Op::F64Ceil):
        CHECK_NEXT(
            emitUnaryMathBuiltinCall(SymbolicAddress::CeilD, ValType::F64));
      case uint16_t(Op::F64Floor):
        CHECK_NEXT(
            emitUnaryMathBuiltinCall(SymbolicAddress::FloorD, ValType::F64));
      case uint16_t(Op::F64PromoteF32):
        CHECK_NEXT(
            dispatchConversion1(ConvertF32ToF64, ValType::F32, ValType::F64));
      case uint16_t(Op::F64ConvertI32S):
        CHECK_NEXT(
            dispatchConversion1(ConvertI32ToF64, ValType::I32, ValType::F64));
      case uint16_t(Op::F64ConvertI32U):
        CHECK_NEXT(
            dispatchConversion1(ConvertU32ToF64, ValType::I32, ValType::F64));
      case uint16_t(Op::F64ConvertI64S):
#ifdef RABALDR_I64_TO_FLOAT_CALLOUT
        CHECK_NEXT(dispatchCalloutConversionOOM(
            emitConvertInt64ToFloatingCallout, SymbolicAddress::Int64ToDouble,
            ValType::I64, ValType::F64));
#else
        CHECK_NEXT(
            dispatchConversion1(ConvertI64ToF64, ValType::I64, ValType::F64));
#endif
      case uint16_t(Op::F64ConvertI64U):
#ifdef RABALDR_I64_TO_FLOAT_CALLOUT
        CHECK_NEXT(dispatchCalloutConversionOOM(
            emitConvertInt64ToFloatingCallout, SymbolicAddress::Uint64ToDouble,
            ValType::I64, ValType::F64));
#else
        CHECK_NEXT(dispatchConversion0(emitConvertU64ToF64, ValType::I64,
                                       ValType::F64));
#endif
      case uint16_t(Op::F64Load):
        CHECK_NEXT(emitLoad(ValType::F64, Scalar::Float64));
      case uint16_t(Op::F64Store):
        CHECK_NEXT(emitStore(ValType::F64, Scalar::Float64));
      case uint16_t(Op::F64ReinterpretI64):
        CHECK_NEXT(dispatchConversion1(ReinterpretI64AsF64, ValType::I64,
                                       ValType::F64));
      case uint16_t(Op::F64CopySign):
        CHECK_NEXT(dispatchBinary1(CopysignF64, ValType::F64));
      case uint16_t(Op::F64Nearest):
        CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::NearbyIntD,
                                            ValType::F64));
      case uint16_t(Op::F64Trunc):
        CHECK_NEXT(
            emitUnaryMathBuiltinCall(SymbolicAddress::TruncD, ValType::F64));

      // Comparisons
      case uint16_t(Op::I32Eq):
        CHECK_NEXT(dispatchComparison0(emitCompareI32, ValType::I32,
                                       Assembler::Equal));
      case uint16_t(Op::I32Ne):
        CHECK_NEXT(dispatchComparison0(emitCompareI32, ValType::I32,
                                       Assembler::NotEqual));
      case uint16_t(Op::I32LtS):
        CHECK_NEXT(dispatchComparison0(emitCompareI32, ValType::I32,
                                       Assembler::LessThan));
      case uint16_t(Op::I32LeS):
        CHECK_NEXT(dispatchComparison0(emitCompareI32, ValType::I32,
                                       Assembler::LessThanOrEqual));
      case uint16_t(Op::I32GtS):
        CHECK_NEXT(dispatchComparison0(emitCompareI32, ValType::I32,
                                       Assembler::GreaterThan));
      case uint16_t(Op::I32GeS):
        CHECK_NEXT(dispatchComparison0(emitCompareI32, ValType::I32,
                                       Assembler::GreaterThanOrEqual));
      case uint16_t(Op::I32LtU):
        CHECK_NEXT(dispatchComparison0(emitCompareI32, ValType::I32,
                                       Assembler::Below));
      case uint16_t(Op::I32LeU):
        CHECK_NEXT(dispatchComparison0(emitCompareI32, ValType::I32,
                                       Assembler::BelowOrEqual));
      case uint16_t(Op::I32GtU):
        CHECK_NEXT(dispatchComparison0(emitCompareI32, ValType::I32,
                                       Assembler::Above));
      case uint16_t(Op::I32GeU):
        CHECK_NEXT(dispatchComparison0(emitCompareI32, ValType::I32,
                                       Assembler::AboveOrEqual));
      case uint16_t(Op::I64Eq):
        CHECK_NEXT(dispatchComparison0(emitCompareI64, ValType::I64,
                                       Assembler::Equal));
      case uint16_t(Op::I64Ne):
        CHECK_NEXT(dispatchComparison0(emitCompareI64, ValType::I64,
                                       Assembler::NotEqual));
      case uint16_t(Op::I64LtS):
        CHECK_NEXT(dispatchComparison0(emitCompareI64, ValType::I64,
                                       Assembler::LessThan));
      case uint16_t(Op::I64LeS):
        CHECK_NEXT(dispatchComparison0(emitCompareI64, ValType::I64,
                                       Assembler::LessThanOrEqual));
      case uint16_t(Op::I64GtS):
        CHECK_NEXT(dispatchComparison0(emitCompareI64, ValType::I64,
                                       Assembler::GreaterThan));
      case uint16_t(Op::I64GeS):
        CHECK_NEXT(dispatchComparison0(emitCompareI64, ValType::I64,
                                       Assembler::GreaterThanOrEqual));
      case uint16_t(Op::I64LtU):
        CHECK_NEXT(dispatchComparison0(emitCompareI64, ValType::I64,
                                       Assembler::Below));
      case uint16_t(Op::I64LeU):
        CHECK_NEXT(dispatchComparison0(emitCompareI64, ValType::I64,
                                       Assembler::BelowOrEqual));
      case uint16_t(Op::I64GtU):
        CHECK_NEXT(dispatchComparison0(emitCompareI64, ValType::I64,
                                       Assembler::Above));
      case uint16_t(Op::I64GeU):
        CHECK_NEXT(dispatchComparison0(emitCompareI64, ValType::I64,
                                       Assembler::AboveOrEqual));
      case uint16_t(Op::F32Eq):
        CHECK_NEXT(dispatchComparison0(emitCompareF32, ValType::F32,
                                       Assembler::DoubleEqual));
      case uint16_t(Op::F32Ne):
        CHECK_NEXT(dispatchComparison0(emitCompareF32, ValType::F32,
                                       Assembler::DoubleNotEqualOrUnordered));
      case uint16_t(Op::F32Lt):
        CHECK_NEXT(dispatchComparison0(emitCompareF32, ValType::F32,
                                       Assembler::DoubleLessThan));
      case uint16_t(Op::F32Le):
        CHECK_NEXT(dispatchComparison0(emitCompareF32, ValType::F32,
                                       Assembler::DoubleLessThanOrEqual));
      case uint16_t(Op::F32Gt):
        CHECK_NEXT(dispatchComparison0(emitCompareF32, ValType::F32,
                                       Assembler::DoubleGreaterThan));
      case uint16_t(Op::F32Ge):
        CHECK_NEXT(dispatchComparison0(emitCompareF32, ValType::F32,
                                       Assembler::DoubleGreaterThanOrEqual));
      case uint16_t(Op::F64Eq):
        CHECK_NEXT(dispatchComparison0(emitCompareF64, ValType::F64,
                                       Assembler::DoubleEqual));
      case uint16_t(Op::F64Ne):
        CHECK_NEXT(dispatchComparison0(emitCompareF64, ValType::F64,
                                       Assembler::DoubleNotEqualOrUnordered));
      case uint16_t(Op::F64Lt):
        CHECK_NEXT(dispatchComparison0(emitCompareF64, ValType::F64,
                                       Assembler::DoubleLessThan));
      case uint16_t(Op::F64Le):
        CHECK_NEXT(dispatchComparison0(emitCompareF64, ValType::F64,
                                       Assembler::DoubleLessThanOrEqual));
      case uint16_t(Op::F64Gt):
        CHECK_NEXT(dispatchComparison0(emitCompareF64, ValType::F64,
                                       Assembler::DoubleGreaterThan));
      case uint16_t(Op::F64Ge):
        CHECK_NEXT(dispatchComparison0(emitCompareF64, ValType::F64,
                                       Assembler::DoubleGreaterThanOrEqual));

      // Sign extensions
      case uint16_t(Op::I32Extend8S):
        CHECK_NEXT(
            dispatchConversion1(ExtendI32_8, ValType::I32, ValType::I32));
      case uint16_t(Op::I32Extend16S):
        CHECK_NEXT(
            dispatchConversion1(ExtendI32_16, ValType::I32, ValType::I32));
      case uint16_t(Op::I64Extend8S):
        CHECK_NEXT(
            dispatchConversion0(emitExtendI64_8, ValType::I64, ValType::I64));
      case uint16_t(Op::I64Extend16S):
        CHECK_NEXT(
            dispatchConversion0(emitExtendI64_16, ValType::I64, ValType::I64));
      case uint16_t(Op::I64Extend32S):
        CHECK_NEXT(
            dispatchConversion0(emitExtendI64_32, ValType::I64, ValType::I64));

      // Memory Related
      case uint16_t(Op::MemoryGrow):
        CHECK_NEXT(emitMemoryGrow());
      case uint16_t(Op::MemorySize):
        CHECK_NEXT(emitMemorySize());

#ifdef ENABLE_WASM_FUNCTION_REFERENCES
      case uint16_t(Op::RefAsNonNull):
        if (!moduleEnv_.functionReferencesEnabled()) {
          return iter_.unrecognizedOpcode(&op);
        }
        CHECK_NEXT(emitRefAsNonNull());
      case uint16_t(Op::BrOnNull):
        if (!moduleEnv_.functionReferencesEnabled()) {
          return iter_.unrecognizedOpcode(&op);
        }
        CHECK_NEXT(emitBrOnNull());
#endif
#ifdef ENABLE_WASM_GC
      case uint16_t(Op::RefEq):
        if (!moduleEnv_.gcEnabled()) {
          return iter_.unrecognizedOpcode(&op);
        }
        CHECK_NEXT(dispatchComparison0(emitCompareRef, RefType::eq(),
                                       Assembler::Equal));
#endif
      case uint16_t(Op::RefFunc):
        CHECK_NEXT(emitRefFunc());
        break;
      case uint16_t(Op::RefNull):
        CHECK_NEXT(emitRefNull());
        break;
      case uint16_t(Op::RefIsNull):
        CHECK_NEXT(emitRefIsNull());
        break;

#ifdef ENABLE_WASM_GC
      // "GC" operations
      case uint16_t(Op::GcPrefix): {
        if (!moduleEnv_.gcEnabled()) {
          return iter_.unrecognizedOpcode(&op);
        }
        switch (op.b1) {
          case uint32_t(GcOp::StructNewWithRtt):
            CHECK_NEXT(emitStructNewWithRtt());
          case uint32_t(GcOp::StructNewDefaultWithRtt):
            CHECK_NEXT(emitStructNewDefaultWithRtt());
          case uint32_t(GcOp::StructGet):
            CHECK_NEXT(emitStructGet(FieldExtension::None));
          case uint32_t(GcOp::StructGetS):
            CHECK_NEXT(emitStructGet(FieldExtension::Signed));
          case uint32_t(GcOp::StructGetU):
            CHECK_NEXT(emitStructGet(FieldExtension::Unsigned));
          case uint32_t(GcOp::StructSet):
            CHECK_NEXT(emitStructSet());
          case uint32_t(GcOp::ArrayNewWithRtt):
            CHECK_NEXT(emitArrayNewWithRtt());
          case uint32_t(GcOp::ArrayNewDefaultWithRtt):
            CHECK_NEXT(emitArrayNewDefaultWithRtt());
          case uint32_t(GcOp::ArrayGet):
            CHECK_NEXT(emitArrayGet(FieldExtension::None));
          case uint32_t(GcOp::ArrayGetS):
            CHECK_NEXT(emitArrayGet(FieldExtension::Signed));
          case uint32_t(GcOp::ArrayGetU):
            CHECK_NEXT(emitArrayGet(FieldExtension::Unsigned));
          case uint32_t(GcOp::ArraySet):
            CHECK_NEXT(emitArraySet());
          case uint32_t(GcOp::ArrayLen):
            CHECK_NEXT(emitArrayLen());
          case uint32_t(GcOp::RttCanon):
            CHECK_NEXT(emitRttCanon());
          case uint32_t(GcOp::RttSub):
            CHECK_NEXT(emitRttSub());
          case uint32_t(GcOp::RefTest):
            CHECK_NEXT(emitRefTest());
          case uint32_t(GcOp::RefCast):
            CHECK_NEXT(emitRefCast());
          case uint32_t(GcOp::BrOnCast):
            CHECK_NEXT(emitBrOnCast());
          default:
            break;
        }  // switch (op.b1)
        return iter_.unrecognizedOpcode(&op);
      }
#endif

#ifdef ENABLE_WASM_SIMD
      // SIMD operations
      case uint16_t(Op::SimdPrefix): {
        uint32_t laneIndex;
        if (!moduleEnv_.v128Enabled()) {
          return iter_.unrecognizedOpcode(&op);
        }
        switch (op.b1) {
          case uint32_t(SimdOp::I8x16ExtractLaneS):
            CHECK_NEXT(dispatchExtractLane(ExtractLaneI8x16, ValType::I32, 16));
          case uint32_t(SimdOp::I8x16ExtractLaneU):
            CHECK_NEXT(
                dispatchExtractLane(ExtractLaneUI8x16, ValType::I32, 16));
          case uint32_t(SimdOp::I16x8ExtractLaneS):
            CHECK_NEXT(dispatchExtractLane(ExtractLaneI16x8, ValType::I32, 8));
          case uint32_t(SimdOp::I16x8ExtractLaneU):
            CHECK_NEXT(dispatchExtractLane(ExtractLaneUI16x8, ValType::I32, 8));
          case uint32_t(SimdOp::I32x4ExtractLane):
            CHECK_NEXT(dispatchExtractLane(ExtractLaneI32x4, ValType::I32, 4));
          case uint32_t(SimdOp::I64x2ExtractLane):
            CHECK_NEXT(dispatchExtractLane(ExtractLaneI64x2, ValType::I64, 2));
          case uint32_t(SimdOp::F32x4ExtractLane):
            CHECK_NEXT(dispatchExtractLane(ExtractLaneF32x4, ValType::F32, 4));
          case uint32_t(SimdOp::F64x2ExtractLane):
            CHECK_NEXT(dispatchExtractLane(ExtractLaneF64x2, ValType::F64, 2));
          case uint32_t(SimdOp::I8x16Splat):
            CHECK_NEXT(dispatchSplat(SplatI8x16, ValType::I32));
          case uint32_t(SimdOp::I16x8Splat):
            CHECK_NEXT(dispatchSplat(SplatI16x8, ValType::I32));
          case uint32_t(SimdOp::I32x4Splat):
            CHECK_NEXT(dispatchSplat(SplatI32x4, ValType::I32));
          case uint32_t(SimdOp::I64x2Splat):
            CHECK_NEXT(dispatchSplat(SplatI64x2, ValType::I64));
          case uint32_t(SimdOp::F32x4Splat):
            CHECK_NEXT(dispatchSplat(SplatF32x4, ValType::F32));
          case uint32_t(SimdOp::F64x2Splat):
            CHECK_NEXT(dispatchSplat(SplatF64x2, ValType::F64));
          case uint32_t(SimdOp::V128AnyTrue):
            CHECK_NEXT(dispatchVectorReduction(AnyTrue));
          case uint32_t(SimdOp::I8x16AllTrue):
            CHECK_NEXT(dispatchVectorReduction(AllTrueI8x16));
          case uint32_t(SimdOp::I16x8AllTrue):
            CHECK_NEXT(dispatchVectorReduction(AllTrueI16x8));
          case uint32_t(SimdOp::I32x4AllTrue):
            CHECK_NEXT(dispatchVectorReduction(AllTrueI32x4));
          case uint32_t(SimdOp::I64x2AllTrue):
            CHECK_NEXT(dispatchVectorReduction(AllTrueI64x2));
          case uint32_t(SimdOp::I8x16Bitmask):
            CHECK_NEXT(dispatchVectorReduction(BitmaskI8x16));
          case uint32_t(SimdOp::I16x8Bitmask):
            CHECK_NEXT(dispatchVectorReduction(BitmaskI16x8));
          case uint32_t(SimdOp::I32x4Bitmask):
            CHECK_NEXT(dispatchVectorReduction(BitmaskI32x4));
          case uint32_t(SimdOp::I64x2Bitmask):
            CHECK_NEXT(dispatchVectorReduction(BitmaskI64x2));
          case uint32_t(SimdOp::I8x16ReplaceLane):
            CHECK_NEXT(dispatchReplaceLane(ReplaceLaneI8x16, ValType::I32, 16));
          case uint32_t(SimdOp::I16x8ReplaceLane):
            CHECK_NEXT(dispatchReplaceLane(ReplaceLaneI16x8, ValType::I32, 8));
          case uint32_t(SimdOp::I32x4ReplaceLane):
            CHECK_NEXT(dispatchReplaceLane(ReplaceLaneI32x4, ValType::I32, 4));
          case uint32_t(SimdOp::I64x2ReplaceLane):
            CHECK_NEXT(dispatchReplaceLane(ReplaceLaneI64x2, ValType::I64, 2));
          case uint32_t(SimdOp::F32x4ReplaceLane):
            CHECK_NEXT(dispatchReplaceLane(ReplaceLaneF32x4, ValType::F32, 4));
          case uint32_t(SimdOp::F64x2ReplaceLane):
            CHECK_NEXT(dispatchReplaceLane(ReplaceLaneF64x2, ValType::F64, 2));
          case uint32_t(SimdOp::I8x16Eq):
            CHECK_NEXT(dispatchVectorComparison(CmpI8x16, Assembler::Equal));
          case uint32_t(SimdOp::I8x16Ne):
            CHECK_NEXT(dispatchVectorComparison(CmpI8x16, Assembler::NotEqual));
          case uint32_t(SimdOp::I8x16LtS):
            CHECK_NEXT(dispatchVectorComparison(CmpI8x16, Assembler::LessThan));
          case uint32_t(SimdOp::I8x16LtU):
            CHECK_NEXT(dispatchVectorComparison(CmpUI8x16, Assembler::Below));
          case uint32_t(SimdOp::I8x16GtS):
            CHECK_NEXT(
                dispatchVectorComparison(CmpI8x16, Assembler::GreaterThan));
          case uint32_t(SimdOp::I8x16GtU):
            CHECK_NEXT(dispatchVectorComparison(CmpUI8x16, Assembler::Above));
          case uint32_t(SimdOp::I8x16LeS):
            CHECK_NEXT(
                dispatchVectorComparison(CmpI8x16, Assembler::LessThanOrEqual));
          case uint32_t(SimdOp::I8x16LeU):
            CHECK_NEXT(
                dispatchVectorComparison(CmpUI8x16, Assembler::BelowOrEqual));
          case uint32_t(SimdOp::I8x16GeS):
            CHECK_NEXT(dispatchVectorComparison(CmpI8x16,
                                                Assembler::GreaterThanOrEqual));
          case uint32_t(SimdOp::I8x16GeU):
            CHECK_NEXT(
                dispatchVectorComparison(CmpUI8x16, Assembler::AboveOrEqual));
          case uint32_t(SimdOp::I16x8Eq):
            CHECK_NEXT(dispatchVectorComparison(CmpI16x8, Assembler::Equal));
          case uint32_t(SimdOp::I16x8Ne):
            CHECK_NEXT(dispatchVectorComparison(CmpI16x8, Assembler::NotEqual));
          case uint32_t(SimdOp::I16x8LtS):
            CHECK_NEXT(dispatchVectorComparison(CmpI16x8, Assembler::LessThan));
          case uint32_t(SimdOp::I16x8LtU):
            CHECK_NEXT(dispatchVectorComparison(CmpUI16x8, Assembler::Below));
          case uint32_t(SimdOp::I16x8GtS):
            CHECK_NEXT(
                dispatchVectorComparison(CmpI16x8, Assembler::GreaterThan));
          case uint32_t(SimdOp::I16x8GtU):
            CHECK_NEXT(dispatchVectorComparison(CmpUI16x8, Assembler::Above));
          case uint32_t(SimdOp::I16x8LeS):
            CHECK_NEXT(
                dispatchVectorComparison(CmpI16x8, Assembler::LessThanOrEqual));
          case uint32_t(SimdOp::I16x8LeU):
            CHECK_NEXT(
                dispatchVectorComparison(CmpUI16x8, Assembler::BelowOrEqual));
          case uint32_t(SimdOp::I16x8GeS):
            CHECK_NEXT(dispatchVectorComparison(CmpI16x8,
                                                Assembler::GreaterThanOrEqual));
          case uint32_t(SimdOp::I16x8GeU):
            CHECK_NEXT(
                dispatchVectorComparison(CmpUI16x8, Assembler::AboveOrEqual));
          case uint32_t(SimdOp::I32x4Eq):
            CHECK_NEXT(dispatchVectorComparison(CmpI32x4, Assembler::Equal));
          case uint32_t(SimdOp::I32x4Ne):
            CHECK_NEXT(dispatchVectorComparison(CmpI32x4, Assembler::NotEqual));
          case uint32_t(SimdOp::I32x4LtS):
            CHECK_NEXT(dispatchVectorComparison(CmpI32x4, Assembler::LessThan));
          case uint32_t(SimdOp::I32x4LtU):
            CHECK_NEXT(dispatchVectorComparison(CmpUI32x4, Assembler::Below));
          case uint32_t(SimdOp::I32x4GtS):
            CHECK_NEXT(
                dispatchVectorComparison(CmpI32x4, Assembler::GreaterThan));
          case uint32_t(SimdOp::I32x4GtU):
            CHECK_NEXT(dispatchVectorComparison(CmpUI32x4, Assembler::Above));
          case uint32_t(SimdOp::I32x4LeS):
            CHECK_NEXT(
                dispatchVectorComparison(CmpI32x4, Assembler::LessThanOrEqual));
          case uint32_t(SimdOp::I32x4LeU):
            CHECK_NEXT(
                dispatchVectorComparison(CmpUI32x4, Assembler::BelowOrEqual));
          case uint32_t(SimdOp::I32x4GeS):
            CHECK_NEXT(dispatchVectorComparison(CmpI32x4,
                                                Assembler::GreaterThanOrEqual));
          case uint32_t(SimdOp::I32x4GeU):
            CHECK_NEXT(
                dispatchVectorComparison(CmpUI32x4, Assembler::AboveOrEqual));
          case uint32_t(SimdOp::I64x2Eq):
            CHECK_NEXT(dispatchVectorComparison(CmpI64x2ForEquality,
                                                Assembler::Equal));
          case uint32_t(SimdOp::I64x2Ne):
            CHECK_NEXT(dispatchVectorComparison(CmpI64x2ForEquality,
                                                Assembler::NotEqual));
          case uint32_t(SimdOp::I64x2LtS):
            CHECK_NEXT(dispatchVectorComparison(CmpI64x2ForOrdering,
                                                Assembler::LessThan));
          case uint32_t(SimdOp::I64x2GtS):
            CHECK_NEXT(dispatchVectorComparison(CmpI64x2ForOrdering,
                                                Assembler::GreaterThan));
          case uint32_t(SimdOp::I64x2LeS):
            CHECK_NEXT(dispatchVectorComparison(CmpI64x2ForOrdering,
                                                Assembler::LessThanOrEqual));
          case uint32_t(SimdOp::I64x2GeS):
            CHECK_NEXT(dispatchVectorComparison(CmpI64x2ForOrdering,
                                                Assembler::GreaterThanOrEqual));
          case uint32_t(SimdOp::F32x4Eq):
            CHECK_NEXT(dispatchVectorComparison(CmpF32x4, Assembler::Equal));
          case uint32_t(SimdOp::F32x4Ne):
            CHECK_NEXT(dispatchVectorComparison(CmpF32x4, Assembler::NotEqual));
          case uint32_t(SimdOp::F32x4Lt):
            CHECK_NEXT(dispatchVectorComparison(CmpF32x4, Assembler::LessThan));
          case uint32_t(SimdOp::F32x4Gt):
            CHECK_NEXT(
                dispatchVectorComparison(CmpF32x4, Assembler::GreaterThan));
          case uint32_t(SimdOp::F32x4Le):
            CHECK_NEXT(
                dispatchVectorComparison(CmpF32x4, Assembler::LessThanOrEqual));
          case uint32_t(SimdOp::F32x4Ge):
            CHECK_NEXT(dispatchVectorComparison(CmpF32x4,
                                                Assembler::GreaterThanOrEqual));
          case uint32_t(SimdOp::F64x2Eq):
            CHECK_NEXT(dispatchVectorComparison(CmpF64x2, Assembler::Equal));
          case uint32_t(SimdOp::F64x2Ne):
            CHECK_NEXT(dispatchVectorComparison(CmpF64x2, Assembler::NotEqual));
          case uint32_t(SimdOp::F64x2Lt):
            CHECK_NEXT(dispatchVectorComparison(CmpF64x2, Assembler::LessThan));
          case uint32_t(SimdOp::F64x2Gt):
            CHECK_NEXT(
                dispatchVectorComparison(CmpF64x2, Assembler::GreaterThan));
          case uint32_t(SimdOp::F64x2Le):
            CHECK_NEXT(
                dispatchVectorComparison(CmpF64x2, Assembler::LessThanOrEqual));
          case uint32_t(SimdOp::F64x2Ge):
            CHECK_NEXT(dispatchVectorComparison(CmpF64x2,
                                                Assembler::GreaterThanOrEqual));
          case uint32_t(SimdOp::V128And):
            CHECK_NEXT(dispatchVectorBinary(AndV128));
          case uint32_t(SimdOp::V128Or):
            CHECK_NEXT(dispatchVectorBinary(OrV128));
          case uint32_t(SimdOp::V128Xor):
            CHECK_NEXT(dispatchVectorBinary(XorV128));
          case uint32_t(SimdOp::V128AndNot):
            CHECK_NEXT(dispatchBinary0(emitVectorAndNot, ValType::V128));
          case uint32_t(SimdOp::I8x16AvgrU):
            CHECK_NEXT(dispatchVectorBinary(AverageUI8x16));
          case uint32_t(SimdOp::I16x8AvgrU):
            CHECK_NEXT(dispatchVectorBinary(AverageUI16x8));
          case uint32_t(SimdOp::I8x16Add):
            CHECK_NEXT(dispatchVectorBinary(AddI8x16));
          case uint32_t(SimdOp::I8x16AddSatS):
            CHECK_NEXT(dispatchVectorBinary(AddSatI8x16));
          case uint32_t(SimdOp::I8x16AddSatU):
            CHECK_NEXT(dispatchVectorBinary(AddSatUI8x16));
          case uint32_t(SimdOp::I8x16Sub):
            CHECK_NEXT(dispatchVectorBinary(SubI8x16));
          case uint32_t(SimdOp::I8x16SubSatS):
            CHECK_NEXT(dispatchVectorBinary(SubSatI8x16));
          case uint32_t(SimdOp::I8x16SubSatU):
            CHECK_NEXT(dispatchVectorBinary(SubSatUI8x16));
          case uint32_t(SimdOp::I8x16MinS):
            CHECK_NEXT(dispatchVectorBinary(MinI8x16));
          case uint32_t(SimdOp::I8x16MinU):
            CHECK_NEXT(dispatchVectorBinary(MinUI8x16));
          case uint32_t(SimdOp::I8x16MaxS):
            CHECK_NEXT(dispatchVectorBinary(MaxI8x16));
          case uint32_t(SimdOp::I8x16MaxU):
            CHECK_NEXT(dispatchVectorBinary(MaxUI8x16));
          case uint32_t(SimdOp::I16x8Add):
            CHECK_NEXT(dispatchVectorBinary(AddI16x8));
          case uint32_t(SimdOp::I16x8AddSatS):
            CHECK_NEXT(dispatchVectorBinary(AddSatI16x8));
          case uint32_t(SimdOp::I16x8AddSatU):
            CHECK_NEXT(dispatchVectorBinary(AddSatUI16x8));
          case uint32_t(SimdOp::I16x8Sub):
            CHECK_NEXT(dispatchVectorBinary(SubI16x8));
          case uint32_t(SimdOp::I16x8SubSatS):
            CHECK_NEXT(dispatchVectorBinary(SubSatI16x8));
          case uint32_t(SimdOp::I16x8SubSatU):
            CHECK_NEXT(dispatchVectorBinary(SubSatUI16x8));
          case uint32_t(SimdOp::I16x8Mul):
            CHECK_NEXT(dispatchVectorBinary(MulI16x8));
          case uint32_t(SimdOp::I16x8MinS):
            CHECK_NEXT(dispatchVectorBinary(MinI16x8));
          case uint32_t(SimdOp::I16x8MinU):
            CHECK_NEXT(dispatchVectorBinary(MinUI16x8));
          case uint32_t(SimdOp::I16x8MaxS):
            CHECK_NEXT(dispatchVectorBinary(MaxI16x8));
          case uint32_t(SimdOp::I16x8MaxU):
            CHECK_NEXT(dispatchVectorBinary(MaxUI16x8));
          case uint32_t(SimdOp::I32x4Add):
            CHECK_NEXT(dispatchVectorBinary(AddI32x4));
          case uint32_t(SimdOp::I32x4Sub):
            CHECK_NEXT(dispatchVectorBinary(SubI32x4));
          case uint32_t(SimdOp::I32x4Mul):
            CHECK_NEXT(dispatchVectorBinary(MulI32x4));
          case uint32_t(SimdOp::I32x4MinS):
            CHECK_NEXT(dispatchVectorBinary(MinI32x4));
          case uint32_t(SimdOp::I32x4MinU):
            CHECK_NEXT(dispatchVectorBinary(MinUI32x4));
          case uint32_t(SimdOp::I32x4MaxS):
            CHECK_NEXT(dispatchVectorBinary(MaxI32x4));
          case uint32_t(SimdOp::I32x4MaxU):
            CHECK_NEXT(dispatchVectorBinary(MaxUI32x4));
          case uint32_t(SimdOp::I64x2Add):
            CHECK_NEXT(dispatchVectorBinary(AddI64x2));
          case uint32_t(SimdOp::I64x2Sub):
            CHECK_NEXT(dispatchVectorBinary(SubI64x2));
          case uint32_t(SimdOp::I64x2Mul):
            CHECK_NEXT(dispatchVectorBinary(MulI64x2));
          case uint32_t(SimdOp::F32x4Add):
            CHECK_NEXT(dispatchVectorBinary(AddF32x4));
          case uint32_t(SimdOp::F32x4Sub):
            CHECK_NEXT(dispatchVectorBinary(SubF32x4));
          case uint32_t(SimdOp::F32x4Mul):
            CHECK_NEXT(dispatchVectorBinary(MulF32x4));
          case uint32_t(SimdOp::F32x4Div):
            CHECK_NEXT(dispatchVectorBinary(DivF32x4));
          case uint32_t(SimdOp::F32x4Min):
            CHECK_NEXT(dispatchVectorBinary(MinF32x4));
          case uint32_t(SimdOp::F32x4Max):
            CHECK_NEXT(dispatchVectorBinary(MaxF32x4));
          case uint32_t(SimdOp::F64x2Add):
            CHECK_NEXT(dispatchVectorBinary(AddF64x2));
          case uint32_t(SimdOp::F64x2Sub):
            CHECK_NEXT(dispatchVectorBinary(SubF64x2));
          case uint32_t(SimdOp::F64x2Mul):
            CHECK_NEXT(dispatchVectorBinary(MulF64x2));
          case uint32_t(SimdOp::F64x2Div):
            CHECK_NEXT(dispatchVectorBinary(DivF64x2));
          case uint32_t(SimdOp::F64x2Min):
            CHECK_NEXT(dispatchVectorBinary(MinF64x2));
          case uint32_t(SimdOp::F64x2Max):
            CHECK_NEXT(dispatchVectorBinary(MaxF64x2));
          case uint32_t(SimdOp::I8x16NarrowI16x8S):
            CHECK_NEXT(dispatchVectorBinary(NarrowI16x8));
          case uint32_t(SimdOp::I8x16NarrowI16x8U):
            CHECK_NEXT(dispatchVectorBinary(NarrowUI16x8));
          case uint32_t(SimdOp::I16x8NarrowI32x4S):
            CHECK_NEXT(dispatchVectorBinary(NarrowI32x4));
          case uint32_t(SimdOp::I16x8NarrowI32x4U):
            CHECK_NEXT(dispatchVectorBinary(NarrowUI32x4));
          case uint32_t(SimdOp::I8x16Swizzle):
            CHECK_NEXT(dispatchVectorBinary(Swizzle));
          case uint32_t(SimdOp::F32x4PMax):
            CHECK_NEXT(dispatchVectorBinary(PMaxF32x4));
          case uint32_t(SimdOp::F32x4PMin):
            CHECK_NEXT(dispatchVectorBinary(PMinF32x4));
          case uint32_t(SimdOp::F64x2PMax):
            CHECK_NEXT(dispatchVectorBinary(PMaxF64x2));
          case uint32_t(SimdOp::F64x2PMin):
            CHECK_NEXT(dispatchVectorBinary(PMinF64x2));
          case uint32_t(SimdOp::I32x4DotI16x8S):
            CHECK_NEXT(dispatchVectorBinary(DotI16x8));
          case uint32_t(SimdOp::I16x8ExtmulLowI8x16S):
            CHECK_NEXT(dispatchVectorBinary(ExtMulLowI8x16));
          case uint32_t(SimdOp::I16x8ExtmulHighI8x16S):
            CHECK_NEXT(dispatchVectorBinary(ExtMulHighI8x16));
          case uint32_t(SimdOp::I16x8ExtmulLowI8x16U):
            CHECK_NEXT(dispatchVectorBinary(ExtMulLowUI8x16));
          case uint32_t(SimdOp::I16x8ExtmulHighI8x16U):
            CHECK_NEXT(dispatchVectorBinary(ExtMulHighUI8x16));
          case uint32_t(SimdOp::I32x4ExtmulLowI16x8S):
            CHECK_NEXT(dispatchVectorBinary(ExtMulLowI16x8));
          case uint32_t(SimdOp::I32x4ExtmulHighI16x8S):
            CHECK_NEXT(dispatchVectorBinary(ExtMulHighI16x8));
          case uint32_t(SimdOp::I32x4ExtmulLowI16x8U):
            CHECK_NEXT(dispatchVectorBinary(ExtMulLowUI16x8));
          case uint32_t(SimdOp::I32x4ExtmulHighI16x8U):
            CHECK_NEXT(dispatchVectorBinary(ExtMulHighUI16x8));
          case uint32_t(SimdOp::I64x2ExtmulLowI32x4S):
            CHECK_NEXT(dispatchVectorBinary(ExtMulLowI32x4));
          case uint32_t(SimdOp::I64x2ExtmulHighI32x4S):
            CHECK_NEXT(dispatchVectorBinary(ExtMulHighI32x4));
          case uint32_t(SimdOp::I64x2ExtmulLowI32x4U):
            CHECK_NEXT(dispatchVectorBinary(ExtMulLowUI32x4));
          case uint32_t(SimdOp::I64x2ExtmulHighI32x4U):
            CHECK_NEXT(dispatchVectorBinary(ExtMulHighUI32x4));
          case uint32_t(SimdOp::I16x8Q15MulrSatS):
            CHECK_NEXT(dispatchVectorBinary(Q15MulrSatS));
          case uint32_t(SimdOp::I8x16Neg):
            CHECK_NEXT(dispatchVectorUnary(NegI8x16));
          case uint32_t(SimdOp::I16x8Neg):
            CHECK_NEXT(dispatchVectorUnary(NegI16x8));
          case uint32_t(SimdOp::I16x8ExtendLowI8x16S):
            CHECK_NEXT(dispatchVectorUnary(WidenLowI8x16));
          case uint32_t(SimdOp::I16x8ExtendHighI8x16S):
            CHECK_NEXT(dispatchVectorUnary(WidenHighI8x16));
          case uint32_t(SimdOp::I16x8ExtendLowI8x16U):
            CHECK_NEXT(dispatchVectorUnary(WidenLowUI8x16));
          case uint32_t(SimdOp::I16x8ExtendHighI8x16U):
            CHECK_NEXT(dispatchVectorUnary(WidenHighUI8x16));
          case uint32_t(SimdOp::I32x4Neg):
            CHECK_NEXT(dispatchVectorUnary(NegI32x4));
          case uint32_t(SimdOp::I32x4ExtendLowI16x8S):
            CHECK_NEXT(dispatchVectorUnary(WidenLowI16x8));
          case uint32_t(SimdOp::I32x4ExtendHighI16x8S):
            CHECK_NEXT(dispatchVectorUnary(WidenHighI16x8));
          case uint32_t(SimdOp::I32x4ExtendLowI16x8U):
            CHECK_NEXT(dispatchVectorUnary(WidenLowUI16x8));
          case uint32_t(SimdOp::I32x4ExtendHighI16x8U):
            CHECK_NEXT(dispatchVectorUnary(WidenHighUI16x8));
          case uint32_t(SimdOp::I32x4TruncSatF32x4S):
            CHECK_NEXT(dispatchVectorUnary(ConvertF32x4ToI32x4));
          case uint32_t(SimdOp::I32x4TruncSatF32x4U):
            CHECK_NEXT(dispatchVectorUnary(ConvertF32x4ToUI32x4));
          case uint32_t(SimdOp::I64x2Neg):
            CHECK_NEXT(dispatchVectorUnary(NegI64x2));
          case uint32_t(SimdOp::I64x2ExtendLowI32x4S):
            CHECK_NEXT(dispatchVectorUnary(WidenLowI32x4));
          case uint32_t(SimdOp::I64x2ExtendHighI32x4S):
            CHECK_NEXT(dispatchVectorUnary(WidenHighI32x4));
          case uint32_t(SimdOp::I64x2ExtendLowI32x4U):
            CHECK_NEXT(dispatchVectorUnary(WidenLowUI32x4));
          case uint32_t(SimdOp::I64x2ExtendHighI32x4U):
            CHECK_NEXT(dispatchVectorUnary(WidenHighUI32x4));
          case uint32_t(SimdOp::F32x4Abs):
            CHECK_NEXT(dispatchVectorUnary(AbsF32x4));
          case uint32_t(SimdOp::F32x4Neg):
            CHECK_NEXT(dispatchVectorUnary(NegF32x4));
          case uint32_t(SimdOp::F32x4Sqrt):
            CHECK_NEXT(dispatchVectorUnary(SqrtF32x4));
          case uint32_t(SimdOp::F32x4ConvertI32x4S):
            CHECK_NEXT(dispatchVectorUnary(ConvertI32x4ToF32x4));
          case uint32_t(SimdOp::F32x4ConvertI32x4U):
            CHECK_NEXT(dispatchVectorUnary(ConvertUI32x4ToF32x4));
          case uint32_t(SimdOp::F32x4DemoteF64x2Zero):
            CHECK_NEXT(dispatchVectorUnary(DemoteF64x2ToF32x4));
          case uint32_t(SimdOp::F64x2PromoteLowF32x4):
            CHECK_NEXT(dispatchVectorUnary(PromoteF32x4ToF64x2));
          case uint32_t(SimdOp::F64x2ConvertLowI32x4S):
            CHECK_NEXT(dispatchVectorUnary(ConvertI32x4ToF64x2));
          case uint32_t(SimdOp::F64x2ConvertLowI32x4U):
            CHECK_NEXT(dispatchVectorUnary(ConvertUI32x4ToF64x2));
          case uint32_t(SimdOp::I32x4TruncSatF64x2SZero):
            CHECK_NEXT(dispatchVectorUnary(ConvertF64x2ToI32x4));
          case uint32_t(SimdOp::I32x4TruncSatF64x2UZero):
            CHECK_NEXT(dispatchVectorUnary(ConvertF64x2ToUI32x4));
          case uint32_t(SimdOp::F64x2Abs):
            CHECK_NEXT(dispatchVectorUnary(AbsF64x2));
          case uint32_t(SimdOp::F64x2Neg):
            CHECK_NEXT(dispatchVectorUnary(NegF64x2));
          case uint32_t(SimdOp::F64x2Sqrt):
            CHECK_NEXT(dispatchVectorUnary(SqrtF64x2));
          case uint32_t(SimdOp::V128Not):
            CHECK_NEXT(dispatchVectorUnary(NotV128));
          case uint32_t(SimdOp::I8x16Popcnt):
            CHECK_NEXT(dispatchVectorUnary(PopcntI8x16));
          case uint32_t(SimdOp::I8x16Abs):
            CHECK_NEXT(dispatchVectorUnary(AbsI8x16));
          case uint32_t(SimdOp::I16x8Abs):
            CHECK_NEXT(dispatchVectorUnary(AbsI16x8));
          case uint32_t(SimdOp::I32x4Abs):
            CHECK_NEXT(dispatchVectorUnary(AbsI32x4));
          case uint32_t(SimdOp::I64x2Abs):
            CHECK_NEXT(dispatchVectorUnary(AbsI64x2));
          case uint32_t(SimdOp::F32x4Ceil):
            CHECK_NEXT(dispatchVectorUnary(CeilF32x4));
          case uint32_t(SimdOp::F32x4Floor):
            CHECK_NEXT(dispatchVectorUnary(FloorF32x4));
          case uint32_t(SimdOp::F32x4Trunc):
            CHECK_NEXT(dispatchVectorUnary(TruncF32x4));
          case uint32_t(SimdOp::F32x4Nearest):
            CHECK_NEXT(dispatchVectorUnary(NearestF32x4));
          case uint32_t(SimdOp::F64x2Ceil):
            CHECK_NEXT(dispatchVectorUnary(CeilF64x2));
          case uint32_t(SimdOp::F64x2Floor):
            CHECK_NEXT(dispatchVectorUnary(FloorF64x2));
          case uint32_t(SimdOp::F64x2Trunc):
            CHECK_NEXT(dispatchVectorUnary(TruncF64x2));
          case uint32_t(SimdOp::F64x2Nearest):
            CHECK_NEXT(dispatchVectorUnary(NearestF64x2));
          case uint32_t(SimdOp::I16x8ExtaddPairwiseI8x16S):
            CHECK_NEXT(dispatchVectorUnary(ExtAddPairwiseI8x16));
          case uint32_t(SimdOp::I16x8ExtaddPairwiseI8x16U):
            CHECK_NEXT(dispatchVectorUnary(ExtAddPairwiseUI8x16));
          case uint32_t(SimdOp::I32x4ExtaddPairwiseI16x8S):
            CHECK_NEXT(dispatchVectorUnary(ExtAddPairwiseI16x8));
          case uint32_t(SimdOp::I32x4ExtaddPairwiseI16x8U):
            CHECK_NEXT(dispatchVectorUnary(ExtAddPairwiseUI16x8));
          case uint32_t(SimdOp::I8x16Shl):
            CHECK_NEXT(dispatchVectorVariableShift(ShiftLeftI8x16));
          case uint32_t(SimdOp::I8x16ShrS):
            CHECK_NEXT(dispatchVectorVariableShift(ShiftRightI8x16));
          case uint32_t(SimdOp::I8x16ShrU):
            CHECK_NEXT(dispatchVectorVariableShift(ShiftRightUI8x16));
          case uint32_t(SimdOp::I16x8Shl):
            CHECK_NEXT(dispatchVectorVariableShift(ShiftLeftI16x8));
          case uint32_t(SimdOp::I16x8ShrS):
            CHECK_NEXT(dispatchVectorVariableShift(ShiftRightI16x8));
          case uint32_t(SimdOp::I16x8ShrU):
            CHECK_NEXT(dispatchVectorVariableShift(ShiftRightUI16x8));
          case uint32_t(SimdOp::I32x4Shl):
            CHECK_NEXT(dispatchVectorVariableShift(ShiftLeftI32x4));
          case uint32_t(SimdOp::I32x4ShrS):
            CHECK_NEXT(dispatchVectorVariableShift(ShiftRightI32x4));
          case uint32_t(SimdOp::I32x4ShrU):
            CHECK_NEXT(dispatchVectorVariableShift(ShiftRightUI32x4));
          case uint32_t(SimdOp::I64x2Shl):
            CHECK_NEXT(dispatchVectorVariableShift(ShiftLeftI64x2));
          case uint32_t(SimdOp::I64x2ShrS):
#  if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
            CHECK_NEXT(emitVectorShiftRightI64x2());
#  else
            CHECK_NEXT(dispatchVectorVariableShift(ShiftRightI64x2));
#  endif
          case uint32_t(SimdOp::I64x2ShrU):
            CHECK_NEXT(dispatchVectorVariableShift(ShiftRightUI64x2));
          case uint32_t(SimdOp::V128Bitselect):
            CHECK_NEXT(dispatchTernary1(BitselectV128, ValType::V128));
          case uint32_t(SimdOp::I8x16Shuffle):
            CHECK_NEXT(emitVectorShuffle());
          case uint32_t(SimdOp::V128Const): {
            V128 v128;
            CHECK(iter_.readV128Const(&v128));
            if (!deadCode_) {
              pushV128(v128);
            }
            NEXT();
          }
          case uint32_t(SimdOp::V128Load):
            CHECK_NEXT(emitLoad(ValType::V128, Scalar::Simd128));
          case uint32_t(SimdOp::V128Load8Splat):
            CHECK_NEXT(emitLoadSplat(Scalar::Uint8));
          case uint32_t(SimdOp::V128Load16Splat):
            CHECK_NEXT(emitLoadSplat(Scalar::Uint16));
          case uint32_t(SimdOp::V128Load32Splat):
            CHECK_NEXT(emitLoadSplat(Scalar::Uint32));
          case uint32_t(SimdOp::V128Load64Splat):
            CHECK_NEXT(emitLoadSplat(Scalar::Int64));
          case uint32_t(SimdOp::V128Load8x8S):
            CHECK_NEXT(emitLoadExtend(Scalar::Int8));
          case uint32_t(SimdOp::V128Load8x8U):
            CHECK_NEXT(emitLoadExtend(Scalar::Uint8));
          case uint32_t(SimdOp::V128Load16x4S):
            CHECK_NEXT(emitLoadExtend(Scalar::Int16));
          case uint32_t(SimdOp::V128Load16x4U):
            CHECK_NEXT(emitLoadExtend(Scalar::Uint16));
          case uint32_t(SimdOp::V128Load32x2S):
            CHECK_NEXT(emitLoadExtend(Scalar::Int32));
          case uint32_t(SimdOp::V128Load32x2U):
            CHECK_NEXT(emitLoadExtend(Scalar::Uint32));
          case uint32_t(SimdOp::V128Load32Zero):
            CHECK_NEXT(emitLoadZero(Scalar::Float32));
          case uint32_t(SimdOp::V128Load64Zero):
            CHECK_NEXT(emitLoadZero(Scalar::Float64));
          case uint32_t(SimdOp::V128Store):
            CHECK_NEXT(emitStore(ValType::V128, Scalar::Simd128));
          case uint32_t(SimdOp::V128Load8Lane):
            CHECK_NEXT(emitLoadLane(1));
          case uint32_t(SimdOp::V128Load16Lane):
            CHECK_NEXT(emitLoadLane(2));
          case uint32_t(SimdOp::V128Load32Lane):
            CHECK_NEXT(emitLoadLane(4));
          case uint32_t(SimdOp::V128Load64Lane):
            CHECK_NEXT(emitLoadLane(8));
          case uint32_t(SimdOp::V128Store8Lane):
            CHECK_NEXT(emitStoreLane(1));
          case uint32_t(SimdOp::V128Store16Lane):
            CHECK_NEXT(emitStoreLane(2));
          case uint32_t(SimdOp::V128Store32Lane):
            CHECK_NEXT(emitStoreLane(4));
          case uint32_t(SimdOp::V128Store64Lane):
            CHECK_NEXT(emitStoreLane(8));
#  ifdef ENABLE_WASM_RELAXED_SIMD
          case uint32_t(SimdOp::F32x4RelaxedFma):
            if (!moduleEnv_.v128RelaxedEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(dispatchTernary1(RelaxedFmaF32x4, ValType::V128));
          case uint32_t(SimdOp::F32x4RelaxedFms):
            if (!moduleEnv_.v128RelaxedEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(dispatchTernary1(RelaxedFmsF32x4, ValType::V128));
          case uint32_t(SimdOp::F64x2RelaxedFma):
            if (!moduleEnv_.v128RelaxedEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(dispatchTernary1(RelaxedFmaF64x2, ValType::V128));
          case uint32_t(SimdOp::F64x2RelaxedFms):
            if (!moduleEnv_.v128RelaxedEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(dispatchTernary1(RelaxedFmsF64x2, ValType::V128));
            break;
          case uint32_t(SimdOp::I8x16LaneSelect):
          case uint32_t(SimdOp::I16x8LaneSelect):
          case uint32_t(SimdOp::I32x4LaneSelect):
          case uint32_t(SimdOp::I64x2LaneSelect):
            if (!moduleEnv_.v128RelaxedEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(emitVectorLaneSelect());
          case uint32_t(SimdOp::F32x4RelaxedMin):
            if (!moduleEnv_.v128RelaxedEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(dispatchVectorBinary(RelaxedMinF32x4));
          case uint32_t(SimdOp::F32x4RelaxedMax):
            if (!moduleEnv_.v128RelaxedEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(dispatchVectorBinary(RelaxedMaxF32x4));
          case uint32_t(SimdOp::F64x2RelaxedMin):
            if (!moduleEnv_.v128RelaxedEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(dispatchVectorBinary(RelaxedMinF64x2));
          case uint32_t(SimdOp::F64x2RelaxedMax):
            if (!moduleEnv_.v128RelaxedEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(dispatchVectorBinary(RelaxedMaxF64x2));
          case uint32_t(SimdOp::I32x4RelaxedTruncSSatF32x4):
            if (!moduleEnv_.v128RelaxedEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(dispatchVectorUnary(RelaxedConvertF32x4ToI32x4));
          case uint32_t(SimdOp::I32x4RelaxedTruncUSatF32x4):
            if (!moduleEnv_.v128RelaxedEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(dispatchVectorUnary(RelaxedConvertF32x4ToUI32x4));
          case uint32_t(SimdOp::I32x4RelaxedTruncSatF64x2SZero):
            if (!moduleEnv_.v128RelaxedEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(dispatchVectorUnary(RelaxedConvertF64x2ToI32x4));
          case uint32_t(SimdOp::I32x4RelaxedTruncSatF64x2UZero):
            if (!moduleEnv_.v128RelaxedEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(dispatchVectorUnary(RelaxedConvertF64x2ToUI32x4));
          case uint32_t(SimdOp::V8x16RelaxedSwizzle):
            if (!moduleEnv_.v128RelaxedEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(dispatchVectorBinary(RelaxedSwizzle));
#  endif
          default:
            break;
        }  // switch (op.b1)
        return iter_.unrecognizedOpcode(&op);
      }
#endif  // ENABLE_WASM_SIMD

      // "Miscellaneous" operations
      case uint16_t(Op::MiscPrefix): {
        switch (op.b1) {
          case uint32_t(MiscOp::I32TruncSatF32S):
            CHECK_NEXT(
                dispatchConversionOOM(emitTruncateF32ToI32<TRUNC_SATURATING>,
                                      ValType::F32, ValType::I32));
          case uint32_t(MiscOp::I32TruncSatF32U):
            CHECK_NEXT(dispatchConversionOOM(
                emitTruncateF32ToI32<TRUNC_UNSIGNED | TRUNC_SATURATING>,
                ValType::F32, ValType::I32));
          case uint32_t(MiscOp::I32TruncSatF64S):
            CHECK_NEXT(
                dispatchConversionOOM(emitTruncateF64ToI32<TRUNC_SATURATING>,
                                      ValType::F64, ValType::I32));
          case uint32_t(MiscOp::I32TruncSatF64U):
            CHECK_NEXT(dispatchConversionOOM(
                emitTruncateF64ToI32<TRUNC_UNSIGNED | TRUNC_SATURATING>,
                ValType::F64, ValType::I32));
          case uint32_t(MiscOp::I64TruncSatF32S):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
            CHECK_NEXT(dispatchCalloutConversionOOM(
                emitConvertFloatingToInt64Callout,
                SymbolicAddress::SaturatingTruncateDoubleToInt64, ValType::F32,
                ValType::I64));
#else
            CHECK_NEXT(
                dispatchConversionOOM(emitTruncateF32ToI64<TRUNC_SATURATING>,
                                      ValType::F32, ValType::I64));
#endif
          case uint32_t(MiscOp::I64TruncSatF32U):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
            CHECK_NEXT(dispatchCalloutConversionOOM(
                emitConvertFloatingToInt64Callout,
                SymbolicAddress::SaturatingTruncateDoubleToUint64, ValType::F32,
                ValType::I64));
#else
            CHECK_NEXT(dispatchConversionOOM(
                emitTruncateF32ToI64<TRUNC_UNSIGNED | TRUNC_SATURATING>,
                ValType::F32, ValType::I64));
#endif
          case uint32_t(MiscOp::I64TruncSatF64S):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
            CHECK_NEXT(dispatchCalloutConversionOOM(
                emitConvertFloatingToInt64Callout,
                SymbolicAddress::SaturatingTruncateDoubleToInt64, ValType::F64,
                ValType::I64));
#else
            CHECK_NEXT(
                dispatchConversionOOM(emitTruncateF64ToI64<TRUNC_SATURATING>,
                                      ValType::F64, ValType::I64));
#endif
          case uint32_t(MiscOp::I64TruncSatF64U):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
            CHECK_NEXT(dispatchCalloutConversionOOM(
                emitConvertFloatingToInt64Callout,
                SymbolicAddress::SaturatingTruncateDoubleToUint64, ValType::F64,
                ValType::I64));
#else
            CHECK_NEXT(dispatchConversionOOM(
                emitTruncateF64ToI64<TRUNC_UNSIGNED | TRUNC_SATURATING>,
                ValType::F64, ValType::I64));
#endif
          case uint32_t(MiscOp::MemoryCopy):
            CHECK_NEXT(emitMemCopy());
          case uint32_t(MiscOp::DataDrop):
            CHECK_NEXT(emitDataOrElemDrop(/*isData=*/true));
          case uint32_t(MiscOp::MemoryFill):
            CHECK_NEXT(emitMemFill());
          case uint32_t(MiscOp::MemoryInit):
            CHECK_NEXT(emitMemInit());
          case uint32_t(MiscOp::TableCopy):
            CHECK_NEXT(emitTableCopy());
          case uint32_t(MiscOp::ElemDrop):
            CHECK_NEXT(emitDataOrElemDrop(/*isData=*/false));
          case uint32_t(MiscOp::TableInit):
            CHECK_NEXT(emitTableInit());
          case uint32_t(MiscOp::TableFill):
            CHECK_NEXT(emitTableFill());
          case uint32_t(MiscOp::TableGrow):
            CHECK_NEXT(emitTableGrow());
          case uint32_t(MiscOp::TableSize):
            CHECK_NEXT(emitTableSize());
          default:
            break;
        }  // switch (op.b1)
        return iter_.unrecognizedOpcode(&op);
      }

      // Thread operations
      case uint16_t(Op::ThreadPrefix): {
        // Though thread ops can be used on nonshared memories, we make them
        // unavailable if shared memory has been disabled in the prefs, for
        // maximum predictability and safety and consistency with JS.
        if (moduleEnv_.sharedMemoryEnabled() == Shareable::False) {
          return iter_.unrecognizedOpcode(&op);
        }
        switch (op.b1) {
          case uint32_t(ThreadOp::Wake):
            CHECK_NEXT(emitWake());

          case uint32_t(ThreadOp::I32Wait):
            CHECK_NEXT(emitWait(ValType::I32, 4));
          case uint32_t(ThreadOp::I64Wait):
            CHECK_NEXT(emitWait(ValType::I64, 8));
          case uint32_t(ThreadOp::Fence):
            CHECK_NEXT(emitFence());

          case uint32_t(ThreadOp::I32AtomicLoad):
            CHECK_NEXT(emitAtomicLoad(ValType::I32, Scalar::Int32));
          case uint32_t(ThreadOp::I64AtomicLoad):
            CHECK_NEXT(emitAtomicLoad(ValType::I64, Scalar::Int64));
          case uint32_t(ThreadOp::I32AtomicLoad8U):
            CHECK_NEXT(emitAtomicLoad(ValType::I32, Scalar::Uint8));
          case uint32_t(ThreadOp::I32AtomicLoad16U):
            CHECK_NEXT(emitAtomicLoad(ValType::I32, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicLoad8U):
            CHECK_NEXT(emitAtomicLoad(ValType::I64, Scalar::Uint8));
          case uint32_t(ThreadOp::I64AtomicLoad16U):
            CHECK_NEXT(emitAtomicLoad(ValType::I64, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicLoad32U):
            CHECK_NEXT(emitAtomicLoad(ValType::I64, Scalar::Uint32));

          case uint32_t(ThreadOp::I32AtomicStore):
            CHECK_NEXT(emitAtomicStore(ValType::I32, Scalar::Int32));
          case uint32_t(ThreadOp::I64AtomicStore):
            CHECK_NEXT(emitAtomicStore(ValType::I64, Scalar::Int64));
          case uint32_t(ThreadOp::I32AtomicStore8U):
            CHECK_NEXT(emitAtomicStore(ValType::I32, Scalar::Uint8));
          case uint32_t(ThreadOp::I32AtomicStore16U):
            CHECK_NEXT(emitAtomicStore(ValType::I32, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicStore8U):
            CHECK_NEXT(emitAtomicStore(ValType::I64, Scalar::Uint8));
          case uint32_t(ThreadOp::I64AtomicStore16U):
            CHECK_NEXT(emitAtomicStore(ValType::I64, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicStore32U):
            CHECK_NEXT(emitAtomicStore(ValType::I64, Scalar::Uint32));

          case uint32_t(ThreadOp::I32AtomicAdd):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Int32, AtomicFetchAddOp));
          case uint32_t(ThreadOp::I64AtomicAdd):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Int64, AtomicFetchAddOp));
          case uint32_t(ThreadOp::I32AtomicAdd8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint8, AtomicFetchAddOp));
          case uint32_t(ThreadOp::I32AtomicAdd16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint16, AtomicFetchAddOp));
          case uint32_t(ThreadOp::I64AtomicAdd8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint8, AtomicFetchAddOp));
          case uint32_t(ThreadOp::I64AtomicAdd16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint16, AtomicFetchAddOp));
          case uint32_t(ThreadOp::I64AtomicAdd32U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint32, AtomicFetchAddOp));

          case uint32_t(ThreadOp::I32AtomicSub):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Int32, AtomicFetchSubOp));
          case uint32_t(ThreadOp::I64AtomicSub):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Int64, AtomicFetchSubOp));
          case uint32_t(ThreadOp::I32AtomicSub8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint8, AtomicFetchSubOp));
          case uint32_t(ThreadOp::I32AtomicSub16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint16, AtomicFetchSubOp));
          case uint32_t(ThreadOp::I64AtomicSub8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint8, AtomicFetchSubOp));
          case uint32_t(ThreadOp::I64AtomicSub16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint16, AtomicFetchSubOp));
          case uint32_t(ThreadOp::I64AtomicSub32U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint32, AtomicFetchSubOp));

          case uint32_t(ThreadOp::I32AtomicAnd):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Int32, AtomicFetchAndOp));
          case uint32_t(ThreadOp::I64AtomicAnd):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Int64, AtomicFetchAndOp));
          case uint32_t(ThreadOp::I32AtomicAnd8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint8, AtomicFetchAndOp));
          case uint32_t(ThreadOp::I32AtomicAnd16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint16, AtomicFetchAndOp));
          case uint32_t(ThreadOp::I64AtomicAnd8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint8, AtomicFetchAndOp));
          case uint32_t(ThreadOp::I64AtomicAnd16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint16, AtomicFetchAndOp));
          case uint32_t(ThreadOp::I64AtomicAnd32U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint32, AtomicFetchAndOp));

          case uint32_t(ThreadOp::I32AtomicOr):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Int32, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I64AtomicOr):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Int64, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I32AtomicOr8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint8, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I32AtomicOr16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint16, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I64AtomicOr8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint8, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I64AtomicOr16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint16, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I64AtomicOr32U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint32, AtomicFetchOrOp));

          case uint32_t(ThreadOp::I32AtomicXor):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Int32, AtomicFetchXorOp));
          case uint32_t(ThreadOp::I64AtomicXor):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Int64, AtomicFetchXorOp));
          case uint32_t(ThreadOp::I32AtomicXor8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint8, AtomicFetchXorOp));
          case uint32_t(ThreadOp::I32AtomicXor16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint16, AtomicFetchXorOp));
          case uint32_t(ThreadOp::I64AtomicXor8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint8, AtomicFetchXorOp));
          case uint32_t(ThreadOp::I64AtomicXor16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint16, AtomicFetchXorOp));
          case uint32_t(ThreadOp::I64AtomicXor32U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint32, AtomicFetchXorOp));

          case uint32_t(ThreadOp::I32AtomicXchg):
            CHECK_NEXT(emitAtomicXchg(ValType::I32, Scalar::Int32));
          case uint32_t(ThreadOp::I64AtomicXchg):
            CHECK_NEXT(emitAtomicXchg(ValType::I64, Scalar::Int64));
          case uint32_t(ThreadOp::I32AtomicXchg8U):
            CHECK_NEXT(emitAtomicXchg(ValType::I32, Scalar::Uint8));
          case uint32_t(ThreadOp::I32AtomicXchg16U):
            CHECK_NEXT(emitAtomicXchg(ValType::I32, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicXchg8U):
            CHECK_NEXT(emitAtomicXchg(ValType::I64, Scalar::Uint8));
          case uint32_t(ThreadOp::I64AtomicXchg16U):
            CHECK_NEXT(emitAtomicXchg(ValType::I64, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicXchg32U):
            CHECK_NEXT(emitAtomicXchg(ValType::I64, Scalar::Uint32));

          case uint32_t(ThreadOp::I32AtomicCmpXchg):
            CHECK_NEXT(emitAtomicCmpXchg(ValType::I32, Scalar::Int32));
          case uint32_t(ThreadOp::I64AtomicCmpXchg):
            CHECK_NEXT(emitAtomicCmpXchg(ValType::I64, Scalar::Int64));
          case uint32_t(ThreadOp::I32AtomicCmpXchg8U):
            CHECK_NEXT(emitAtomicCmpXchg(ValType::I32, Scalar::Uint8));
          case uint32_t(ThreadOp::I32AtomicCmpXchg16U):
            CHECK_NEXT(emitAtomicCmpXchg(ValType::I32, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicCmpXchg8U):
            CHECK_NEXT(emitAtomicCmpXchg(ValType::I64, Scalar::Uint8));
          case uint32_t(ThreadOp::I64AtomicCmpXchg16U):
            CHECK_NEXT(emitAtomicCmpXchg(ValType::I64, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicCmpXchg32U):
            CHECK_NEXT(emitAtomicCmpXchg(ValType::I64, Scalar::Uint32));

          default:
            return iter_.unrecognizedOpcode(&op);
        }
        break;
      }

      // asm.js and other private operations
      case uint16_t(Op::MozPrefix):
        return iter_.unrecognizedOpcode(&op);

      // private intrinsic operations
      case uint16_t(Op::IntrinsicPrefix): {
        if (!moduleEnv_.intrinsicsEnabled() ||
            op.b1 >= uint32_t(IntrinsicOp::Limit)) {
          return iter_.unrecognizedOpcode(&op);
        }
        CHECK_NEXT(emitIntrinsic(IntrinsicOp(op.b1)));
      }

      default:
        return iter_.unrecognizedOpcode(&op);
    }

#undef CHECK
#undef NEXT
#undef CHECK_NEXT
#undef CHECK_POINTER_COUNT
#undef dispatchBinary0
#undef dispatchBinary1
#undef dispatchBinary2
#undef dispatchBinary3
#undef dispatchUnary0
#undef dispatchUnary1
#undef dispatchUnary2
#undef dispatchComparison0
#undef dispatchConversion0
#undef dispatchConversion1
#undef dispatchConversionOOM
#undef dispatchCalloutConversionOOM
#undef dispatchIntDivCallout
#undef dispatchVectorBinary
#undef dispatchVectorUnary
#undef dispatchVectorComparison
#undef dispatchExtractLane
#undef dispatchReplaceLane
#undef dispatchSplat
#undef dispatchVectorReduction

    MOZ_CRASH("unreachable");
  }

  MOZ_CRASH("unreachable");
}

//////////////////////////////////////////////////////////////////////////////
//
// Various helpers.

void BaseCompiler::assertResultRegistersAvailable(ResultType type) {
#ifdef DEBUG
  for (ABIResultIter iter(type); !iter.done(); iter.next()) {
    ABIResult result = iter.cur();
    if (!result.inRegister()) {
      return;
    }
    switch (result.type().kind()) {
      case ValType::I32:
        MOZ_ASSERT(isAvailableI32(RegI32(result.gpr())));
        break;
      case ValType::I64:
        MOZ_ASSERT(isAvailableI64(RegI64(result.gpr64())));
        break;
      case ValType::V128:
#  ifdef ENABLE_WASM_SIMD
        MOZ_ASSERT(isAvailableV128(RegV128(result.fpr())));
        break;
#  else
        MOZ_CRASH("No SIMD support");
#  endif
      case ValType::F32:
        MOZ_ASSERT(isAvailableF32(RegF32(result.fpr())));
        break;
      case ValType::F64:
        MOZ_ASSERT(isAvailableF64(RegF64(result.fpr())));
        break;
      case ValType::Rtt:
      case ValType::Ref:
        MOZ_ASSERT(isAvailableRef(RegRef(result.gpr())));
        break;
    }
  }
#endif
}

void BaseCompiler::saveTempPtr(const RegPtr& r) {
  MOZ_ASSERT(!ra.isAvailablePtr(r));
  fr.pushGPR(r);
  ra.freePtr(r);
  MOZ_ASSERT(ra.isAvailablePtr(r));
}

void BaseCompiler::restoreTempPtr(const RegPtr& r) {
  MOZ_ASSERT(ra.isAvailablePtr(r));
  ra.needPtr(r);
  fr.popGPR(r);
  MOZ_ASSERT(!ra.isAvailablePtr(r));
}

#ifdef DEBUG
void BaseCompiler::performRegisterLeakCheck() {
  BaseRegAlloc::LeakCheck check(ra);
  for (auto& item : stk_) {
    switch (item.kind_) {
      case Stk::RegisterI32:
        check.addKnownI32(item.i32reg());
        break;
      case Stk::RegisterI64:
        check.addKnownI64(item.i64reg());
        break;
      case Stk::RegisterF32:
        check.addKnownF32(item.f32reg());
        break;
      case Stk::RegisterF64:
        check.addKnownF64(item.f64reg());
        break;
#  ifdef ENABLE_WASM_SIMD
      case Stk::RegisterV128:
        check.addKnownV128(item.v128reg());
        break;
#  endif
      case Stk::RegisterRef:
        check.addKnownRef(item.refReg());
        break;
      default:
        break;
    }
  }
}

void BaseCompiler::assertStackInvariants() const {
  if (deadCode_) {
    // Nonlocal control flow can pass values in stack locations in a way that
    // isn't accounted for by the value stack.  In dead code, which occurs
    // after unconditional non-local control flow, there is no invariant to
    // assert.
    return;
  }
  size_t size = 0;
  for (const Stk& v : stk_) {
    switch (v.kind()) {
      case Stk::MemRef:
        size += BaseStackFrame::StackSizeOfPtr;
        break;
      case Stk::MemI32:
        size += BaseStackFrame::StackSizeOfPtr;
        break;
      case Stk::MemI64:
        size += BaseStackFrame::StackSizeOfInt64;
        break;
      case Stk::MemF64:
        size += BaseStackFrame::StackSizeOfDouble;
        break;
      case Stk::MemF32:
        size += BaseStackFrame::StackSizeOfFloat;
        break;
#  ifdef ENABLE_WASM_SIMD
      case Stk::MemV128:
        size += BaseStackFrame::StackSizeOfV128;
        break;
#  endif
      default:
        MOZ_ASSERT(!v.isMem());
        break;
    }
  }
  MOZ_ASSERT(size == fr.dynamicHeight());
}
#endif

//////////////////////////////////////////////////////////////////////////////
//
// Main compilation logic.

bool BaseCompiler::emitFunction() {
  AutoCreatedBy acb(masm, "(wasm)BaseCompiler::emitFunction");

  if (!beginFunction()) {
    return false;
  }

  if (!emitBody()) {
    return false;
  }

  if (!endFunction()) {
    return false;
  }

  return true;
}

BaseCompiler::BaseCompiler(const ModuleEnvironment& moduleEnv,
                           const CompilerEnvironment& compilerEnv,
                           const FuncCompileInput& func,
                           const ValTypeVector& locals,
                           const MachineState& trapExitLayout,
                           size_t trapExitLayoutNumWords, Decoder& decoder,
                           StkVector& stkSource, TempAllocator* alloc,
                           MacroAssembler* masm, StackMaps* stackMaps)
    :  // Environment
      moduleEnv_(moduleEnv),
      compilerEnv_(compilerEnv),
      func_(func),
      locals_(locals),
      stkSource_(stkSource),
      // Output-only data structures
      alloc_(alloc->fallible()),
      masm(*masm),
      // Compilation state
      decoder_(decoder),
      iter_(moduleEnv, decoder),
      fr(*masm),
      stackMapGenerator_(stackMaps, trapExitLayout, trapExitLayoutNumWords,
                         *masm),
      deadCode_(false),
      lastReadCallSite_(0),
      bceSafe_(0),
      latentOp_(LatentOp::None),
      latentType_(ValType::I32),
      latentIntCmp_(Assembler::Equal),
      latentDoubleCmp_(Assembler::DoubleEqual) {
  // Our caller, BaselineCompileFunctions, will lend us the vector contents to
  // use for the eval stack.  To get hold of those contents, we'll temporarily
  // installing an empty one in its place.
  MOZ_ASSERT(stk_.empty());
  stk_.swap(stkSource_);

  // Assuming that previously processed wasm functions are well formed, the
  // eval stack should now be empty.  But empty it anyway; any non-emptyness
  // at this point will cause chaos.
  stk_.clear();
}

BaseCompiler::~BaseCompiler() {
  stk_.swap(stkSource_);
  // We've returned the eval stack vector contents to our caller,
  // BaselineCompileFunctions.  We expect the vector we get in return to be
  // empty since that's what we swapped for the stack vector in our
  // constructor.
  MOZ_ASSERT(stk_.empty());
}

bool BaseCompiler::init() {
  // We may lift this restriction in the future.
  MOZ_ASSERT_IF(usesMemory() && isMem64(), !moduleEnv_.hugeMemoryEnabled());

  ra.init(this);

  if (!SigD_.append(ValType::F64)) {
    return false;
  }
  if (!SigF_.append(ValType::F32)) {
    return false;
  }

  ArgTypeVector args(funcType());
  return fr.setupLocals(locals_, args, compilerEnv_.debugEnabled(),
                        &localInfo_);
}

FuncOffsets BaseCompiler::finish() {
  MOZ_ASSERT(iter_.done(), "all bytes must be consumed");
  MOZ_ASSERT(func_.callSiteLineNums.length() == lastReadCallSite_);

  MOZ_ASSERT(stk_.empty());
  MOZ_ASSERT(stackMapGenerator_.memRefsOnStk == 0);

  masm.flushBuffer();

  return offsets_;
}

}  // namespace wasm
}  // namespace js

bool js::wasm::BaselinePlatformSupport() {
#if defined(JS_CODEGEN_ARM)
  // Simplifying assumption: require SDIV and UDIV.
  //
  // I have no good data on ARM populations allowing me to say that
  // X% of devices in the market implement SDIV and UDIV.  However,
  // they are definitely implemented on the Cortex-A7 and Cortex-A15
  // and on all ARMv8 systems.
  if (!HasIDIV()) {
    return false;
  }
#endif
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86) ||   \
    defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) || \
    defined(JS_CODEGEN_MIPS64)
  return true;
#else
  return false;
#endif
}

bool js::wasm::BaselineCompileFunctions(const ModuleEnvironment& moduleEnv,
                                        const CompilerEnvironment& compilerEnv,
                                        LifoAlloc& lifo,
                                        const FuncCompileInputVector& inputs,
                                        CompiledCode* code,
                                        UniqueChars* error) {
  MOZ_ASSERT(compilerEnv.tier() == Tier::Baseline);
  MOZ_ASSERT(moduleEnv.kind == ModuleKind::Wasm);

  // The MacroAssembler will sometimes access the jitContext.

  TempAllocator alloc(&lifo);
  JitContext jitContext(&alloc);
  MOZ_ASSERT(IsCompilingWasm());
  WasmMacroAssembler masm(alloc, moduleEnv);

  // Swap in already-allocated empty vectors to avoid malloc/free.
  MOZ_ASSERT(code->empty());
  if (!code->swap(masm)) {
    return false;
  }

  // Create a description of the stack layout created by GenerateTrapExit().
  MachineState trapExitLayout;
  size_t trapExitLayoutNumWords;
  GenerateTrapExitMachineState(&trapExitLayout, &trapExitLayoutNumWords);

  // The compiler's operand stack.  We reuse it across all functions so as to
  // avoid malloc/free.  Presize it to 128 elements in the hope of avoiding
  // reallocation later.
  StkVector stk;
  if (!stk.reserve(128)) {
    return false;
  }

  for (const FuncCompileInput& func : inputs) {
    Decoder d(func.begin, func.end, func.lineOrBytecode, error);

    // Build the local types vector.

    ValTypeVector locals;
    if (!locals.appendAll(moduleEnv.funcs[func.index].type->args())) {
      return false;
    }
    if (!DecodeLocalEntries(d, *moduleEnv.types, moduleEnv.features, &locals)) {
      return false;
    }

    // One-pass baseline compilation.

    BaseCompiler f(moduleEnv, compilerEnv, func, locals, trapExitLayout,
                   trapExitLayoutNumWords, d, stk, &alloc, &masm,
                   &code->stackMaps);
    if (!f.init()) {
      return false;
    }
    if (!f.emitFunction()) {
      return false;
    }
    if (!code->codeRanges.emplaceBack(func.index, func.lineOrBytecode,
                                      f.finish())) {
      return false;
    }
  }

  masm.finish();
  if (masm.oom()) {
    return false;
  }

  return code->swap(masm);
}
