/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineCompiler.h"

#include "mozilla/Casting.h"

#include "jit/BaselineIC.h"
#include "jit/BaselineJIT.h"
#include "jit/FixedList.h"
#include "jit/IonAnalysis.h"
#include "jit/JitcodeMap.h"
#include "jit/JitSpewer.h"
#include "jit/Linker.h"
#ifdef JS_ION_PERF
#  include "jit/PerfSpewer.h"
#endif
#include "jit/SharedICHelpers.h"
#include "jit/VMFunctions.h"
#include "js/UniquePtr.h"
#include "vm/AsyncFunction.h"
#include "vm/AsyncIteration.h"
#include "vm/EnvironmentObject.h"
#include "vm/Interpreter.h"
#include "vm/JSFunction.h"
#include "vm/TraceLogging.h"
#include "vtune/VTuneWrapper.h"

#include "jit/BaselineFrameInfo-inl.h"
#include "jit/MacroAssembler-inl.h"
#include "jit/SharedICHelpers-inl.h"
#include "jit/VMFunctionList-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::AssertedCast;
using mozilla::Maybe;

namespace js {
namespace jit {

// When generating the Baseline Interpreter, this register is guaranteed to hold
// the bytecode pc at the start of a bytecode instruction. Instructions are free
// to clobber this register: the frame's interpreterPC is the canonical
// location for the pc. This register is useful to avoid loading the pc when
// compiling simple ops like JSOP_INT8 or JSOP_GETLOCAL.
static constexpr Register PCRegAtStart = R2.scratchReg();

BaselineCompilerHandler::BaselineCompilerHandler(JSContext* cx,
                                                 MacroAssembler& masm,
                                                 TempAllocator& alloc,
                                                 JSScript* script)
    : frame_(script, masm),
      alloc_(alloc),
      analysis_(alloc, script),
      script_(script),
      pc_(script->code()),
      icEntryIndex_(0),
      compileDebugInstrumentation_(script->isDebuggee()),
      ionCompileable_(jit::IsIonEnabled(cx) &&
                      CanIonCompileScript(cx, script)) {}

BaselineInterpreterHandler::BaselineInterpreterHandler(JSContext* cx,
                                                       MacroAssembler& masm)
    : frame_(masm) {}

template <typename Handler>
template <typename... HandlerArgs>
BaselineCodeGen<Handler>::BaselineCodeGen(JSContext* cx, HandlerArgs&&... args)
    : handler(cx, masm, std::forward<HandlerArgs>(args)...),
      cx(cx),
      frame(handler.frame()),
      traceLoggerToggleOffsets_(cx),
      profilerEnterFrameToggleOffset_(),
      profilerExitFrameToggleOffset_(),
      pushedBeforeCall_(0),
#ifdef DEBUG
      inCall_(false),
#endif
      modifiesArguments_(false) {
}

BaselineCompiler::BaselineCompiler(JSContext* cx, TempAllocator& alloc,
                                   JSScript* script)
    : BaselineCodeGen(cx, /* HandlerArgs = */ alloc, script),
      pcMappingEntries_(),
      profilerPushToggleOffset_(),
      traceLoggerScriptTextIdOffset_() {
#ifdef JS_CODEGEN_NONE
  MOZ_CRASH();
#endif
}

BaselineInterpreterGenerator::BaselineInterpreterGenerator(JSContext* cx)
    : BaselineCodeGen(cx /* no handlerArgs */) {}

bool BaselineCompilerHandler::init(JSContext* cx) {
  if (!analysis_.init(alloc_, cx->caches().gsnCache)) {
    return false;
  }

  uint32_t len = script_->length();

  if (!labels_.init(alloc_, len)) {
    return false;
  }

  for (size_t i = 0; i < len; i++) {
    new (&labels_[i]) Label();
  }

  if (!frame_.init(alloc_)) {
    return false;
  }

  return true;
}

bool BaselineCompiler::init() {
  if (!handler.init(cx)) {
    return false;
  }

  return true;
}

bool BaselineCompiler::addPCMappingEntry(bool addIndexEntry) {
  // Don't add multiple entries for a single pc.
  size_t nentries = pcMappingEntries_.length();
  uint32_t pcOffset = handler.script()->pcToOffset(handler.pc());
  if (nentries > 0 && pcMappingEntries_[nentries - 1].pcOffset == pcOffset) {
    return true;
  }

  PCMappingEntry entry;
  entry.pcOffset = pcOffset;
  entry.nativeOffset = masm.currentOffset();
  entry.slotInfo = getStackTopSlotInfo();
  entry.addIndexEntry = addIndexEntry;

  return pcMappingEntries_.append(entry);
}

MethodStatus BaselineCompiler::compile() {
  JSScript* script = handler.script();
  JitSpew(JitSpew_BaselineScripts, "Baseline compiling script %s:%u:%u (%p)",
          script->filename(), script->lineno(), script->column(), script);

  JitSpew(JitSpew_Codegen, "# Emitting baseline code for script %s:%u:%u",
          script->filename(), script->lineno(), script->column());

  TraceLoggerThread* logger = TraceLoggerForCurrentThread(cx);
  TraceLoggerEvent scriptEvent(TraceLogger_AnnotateScripts, script);
  AutoTraceLog logScript(logger, scriptEvent);
  AutoTraceLog logCompile(logger, TraceLogger_BaselineCompilation);

  AutoKeepTypeScripts keepTypes(cx);
  if (!script->ensureHasTypes(cx, keepTypes)) {
    return Method_Error;
  }

  // When code coverage is only enabled for optimizations, or when a Debugger
  // set the collectCoverageInfo flag, we have to create the ScriptCounts if
  // they do not exist.
  if (!script->hasScriptCounts() && cx->realm()->collectCoverage()) {
    if (!script->initScriptCounts(cx)) {
      return Method_Error;
    }
  }

  // Pin analysis info during compilation.
  AutoEnterAnalysis autoEnterAnalysis(cx);

  MOZ_ASSERT(!script->hasBaselineScript());

  if (!emitPrologue()) {
    return Method_Error;
  }

  MethodStatus status = emitBody();
  if (status != Method_Compiled) {
    return status;
  }

  if (!emitEpilogue()) {
    return Method_Error;
  }

  if (!emitOutOfLinePostBarrierSlot()) {
    return Method_Error;
  }

  Linker linker(masm, "Baseline");
  if (masm.oom()) {
    ReportOutOfMemory(cx);
    return Method_Error;
  }

  JitCode* code = linker.newCode(cx, CodeKind::Baseline);
  if (!code) {
    return Method_Error;
  }

  Rooted<EnvironmentObject*> templateEnv(cx);
  if (script->functionNonDelazifying()) {
    RootedFunction fun(cx, script->functionNonDelazifying());

    if (fun->needsNamedLambdaEnvironment()) {
      templateEnv =
          NamedLambdaObject::createTemplateObject(cx, fun, gc::TenuredHeap);
      if (!templateEnv) {
        return Method_Error;
      }
    }

    if (fun->needsCallObject()) {
      RootedScript scriptRoot(cx, script);
      templateEnv = CallObject::createTemplateObject(
          cx, scriptRoot, templateEnv, gc::TenuredHeap);
      if (!templateEnv) {
        return Method_Error;
      }
    }
  }

  // Encode the pc mapping table. See PCMappingIndexEntry for
  // more information.
  Vector<PCMappingIndexEntry> pcMappingIndexEntries(cx);
  CompactBufferWriter pcEntries;
  uint32_t previousOffset = 0;

  for (size_t i = 0; i < pcMappingEntries_.length(); i++) {
    PCMappingEntry& entry = pcMappingEntries_[i];

    if (entry.addIndexEntry) {
      PCMappingIndexEntry indexEntry;
      indexEntry.pcOffset = entry.pcOffset;
      indexEntry.nativeOffset = entry.nativeOffset;
      indexEntry.bufferOffset = pcEntries.length();
      if (!pcMappingIndexEntries.append(indexEntry)) {
        ReportOutOfMemory(cx);
        return Method_Error;
      }
      previousOffset = entry.nativeOffset;
    }

    // Use the high bit of the SlotInfo byte to indicate the
    // native code offset (relative to the previous op) > 0 and
    // comes next in the buffer.
    MOZ_ASSERT((entry.slotInfo.toByte() & 0x80) == 0);

    if (entry.nativeOffset == previousOffset) {
      pcEntries.writeByte(entry.slotInfo.toByte());
    } else {
      MOZ_ASSERT(entry.nativeOffset > previousOffset);
      pcEntries.writeByte(0x80 | entry.slotInfo.toByte());
      pcEntries.writeUnsigned(entry.nativeOffset - previousOffset);
    }

    previousOffset = entry.nativeOffset;
  }

  if (pcEntries.oom()) {
    ReportOutOfMemory(cx);
    return Method_Error;
  }

  size_t resumeEntries =
      script->hasResumeOffsets() ? script->resumeOffsets().size() : 0;
  UniquePtr<BaselineScript> baselineScript(
      BaselineScript::New(script, bailoutPrologueOffset_.offset(),
                          warmUpCheckPrologueOffset_.offset(),
                          debugOsrPrologueOffset_.offset(),
                          debugOsrEpilogueOffset_.offset(),
                          profilerEnterFrameToggleOffset_.offset(),
                          profilerExitFrameToggleOffset_.offset(),
                          handler.retAddrEntries().length(),
                          pcMappingIndexEntries.length(), pcEntries.length(),
                          resumeEntries, traceLoggerToggleOffsets_.length()),
      JS::DeletePolicy<BaselineScript>(cx->runtime()));
  if (!baselineScript) {
    ReportOutOfMemory(cx);
    return Method_Error;
  }

  baselineScript->setMethod(code);
  baselineScript->setTemplateEnvironment(templateEnv);

  JitSpew(JitSpew_BaselineScripts,
          "Created BaselineScript %p (raw %p) for %s:%u:%u",
          (void*)baselineScript.get(), (void*)code->raw(), script->filename(),
          script->lineno(), script->column());

  MOZ_ASSERT(pcMappingIndexEntries.length() > 0);
  baselineScript->copyPCMappingIndexEntries(&pcMappingIndexEntries[0]);

  MOZ_ASSERT(pcEntries.length() > 0);
  baselineScript->copyPCMappingEntries(pcEntries);

  // Copy RetAddrEntries.
  if (handler.retAddrEntries().length() > 0) {
    baselineScript->copyRetAddrEntries(script,
                                       handler.retAddrEntries().begin());
  }

  // If profiler instrumentation is enabled, toggle instrumentation on.
  if (cx->runtime()->jitRuntime()->isProfilerInstrumentationEnabled(
          cx->runtime())) {
    baselineScript->toggleProfilerInstrumentation(true);
  }

  if (modifiesArguments_) {
    baselineScript->setModifiesArguments();
  }
  if (handler.analysis().usesEnvironmentChain()) {
    baselineScript->setUsesEnvironmentChain();
  }

#ifdef JS_TRACE_LOGGING
  // Initialize the tracelogger instrumentation.
  if (JS::TraceLoggerSupported()) {
    baselineScript->initTraceLogger(script, traceLoggerToggleOffsets_);
  }
#endif

  // Compute yield/await native resume addresses.
  baselineScript->computeResumeNativeOffsets(script);

  if (compileDebugInstrumentation()) {
    baselineScript->setHasDebugInstrumentation();
  }

  // Always register a native => bytecode mapping entry, since profiler can be
  // turned on with baseline jitcode on stack, and baseline jitcode cannot be
  // invalidated.
  {
    JitSpew(JitSpew_Profiling,
            "Added JitcodeGlobalEntry for baseline script %s:%u:%u (%p)",
            script->filename(), script->lineno(), script->column(),
            baselineScript.get());

    // Generate profiling string.
    UniqueChars str = GeckoProfilerRuntime::allocProfileString(cx, script);
    if (!str) {
      return Method_Error;
    }

    JitcodeGlobalEntry::BaselineEntry entry;
    entry.init(code, code->raw(), code->rawEnd(), script, str.release());

    JitcodeGlobalTable* globalTable =
        cx->runtime()->jitRuntime()->getJitcodeGlobalTable();
    if (!globalTable->addEntry(entry)) {
      entry.destroy();
      ReportOutOfMemory(cx);
      return Method_Error;
    }

    // Mark the jitcode as having a bytecode map.
    code->setHasBytecodeMap();
  }

  script->setBaselineScript(cx->runtime(), baselineScript.release());

#ifdef JS_ION_PERF
  writePerfSpewerBaselineProfile(script, code);
#endif

#ifdef MOZ_VTUNE
  vtune::MarkScript(code, script, "baseline");
#endif

  return Method_Compiled;
}

static void LoadInt8Operand(MacroAssembler& masm, Register pc, Register dest) {
  masm.load8SignExtend(Address(pc, sizeof(jsbytecode)), dest);
}

static void LoadUint8Operand(MacroAssembler& masm, Register pc, Register dest) {
  masm.load8ZeroExtend(Address(pc, sizeof(jsbytecode)), dest);
}

static void LoadUint16Operand(MacroAssembler& masm, Register pc,
                              Register dest) {
  masm.load16ZeroExtend(Address(pc, sizeof(jsbytecode)), dest);
}

static void LoadInt32Operand(MacroAssembler& masm, Register pc, Register dest) {
  masm.load32(Address(pc, sizeof(jsbytecode)), dest);
}

static void LoadInt32OperandSignExtendToPtr(MacroAssembler& masm, Register pc,
                                            Register dest) {
  masm.load32SignExtendToPtr(Address(pc, sizeof(jsbytecode)), dest);
}

static void LoadUint24Operand(MacroAssembler& masm, Register pc, size_t offset,
                              Register dest) {
  // Load the opcode and operand, then left shift to discard the opcode.
  masm.load32(Address(pc, offset), dest);
  masm.rshift32(Imm32(8), dest);
}

static void LoadInlineValueOperand(MacroAssembler& masm, Register pc,
                                   ValueOperand dest) {
  // Note: the Value might be unaligned but as above we rely on all our
  // platforms having appropriate support for unaligned accesses (except for
  // floating point instructions on ARM).
  masm.loadUnalignedValue(Address(pc, sizeof(jsbytecode)), dest);
}

template <>
void BaselineCompilerCodeGen::loadScript(Register dest) {
  masm.movePtr(ImmGCPtr(handler.script()), dest);
}

template <>
void BaselineInterpreterCodeGen::loadScript(Register dest) {
  masm.loadPtr(frame.addressOfInterpreterScript(), dest);
}

template <>
void BaselineCompilerCodeGen::loadScriptAtom(Register index, Register dest) {
  MOZ_CRASH("BaselineCompiler shouldn't call loadScriptAtom");
}

template <>
void BaselineInterpreterCodeGen::loadScriptAtom(Register index, Register dest) {
  MOZ_ASSERT(index != dest);
  loadScript(dest);
  masm.loadPtr(Address(dest, JSScript::offsetOfScriptData()), dest);
  masm.loadPtr(
      BaseIndex(dest, index, ScalePointer, SharedScriptData::offsetOfAtoms()),
      dest);
}

template <>
void BaselineCompilerCodeGen::emitInitializeLocals() {
  // Initialize all locals to |undefined|. Lexical bindings are temporal
  // dead zoned in bytecode.

  size_t n = frame.nlocals();
  if (n == 0) {
    return;
  }

  // Use R0 to minimize code size. If the number of locals to push is <
  // LOOP_UNROLL_FACTOR, then the initialization pushes are emitted directly
  // and inline.  Otherwise, they're emitted in a partially unrolled loop.
  static const size_t LOOP_UNROLL_FACTOR = 4;
  size_t toPushExtra = n % LOOP_UNROLL_FACTOR;

  masm.moveValue(UndefinedValue(), R0);

  // Handle any extra pushes left over by the optional unrolled loop below.
  for (size_t i = 0; i < toPushExtra; i++) {
    masm.pushValue(R0);
  }

  // Partially unrolled loop of pushes.
  if (n >= LOOP_UNROLL_FACTOR) {
    size_t toPush = n - toPushExtra;
    MOZ_ASSERT(toPush % LOOP_UNROLL_FACTOR == 0);
    MOZ_ASSERT(toPush >= LOOP_UNROLL_FACTOR);
    masm.move32(Imm32(toPush), R1.scratchReg());
    // Emit unrolled loop with 4 pushes per iteration.
    Label pushLoop;
    masm.bind(&pushLoop);
    for (size_t i = 0; i < LOOP_UNROLL_FACTOR; i++) {
      masm.pushValue(R0);
    }
    masm.branchSub32(Assembler::NonZero, Imm32(LOOP_UNROLL_FACTOR),
                     R1.scratchReg(), &pushLoop);
  }
}

template <>
void BaselineInterpreterCodeGen::emitInitializeLocals() {
  // Push |undefined| for all locals.

  Register scratch = R0.scratchReg();
  loadScript(scratch);
  masm.loadPtr(Address(scratch, JSScript::offsetOfScriptData()), scratch);
  masm.load32(Address(scratch, SharedScriptData::offsetOfNfixed()), scratch);

  Label top, done;
  masm.bind(&top);
  masm.branchTest32(Assembler::Zero, scratch, scratch, &done);
  {
    masm.pushValue(UndefinedValue());
    masm.sub32(Imm32(1), scratch);
    masm.jump(&top);
  }

  masm.bind(&done);
}

// On input:
//  R2.scratchReg() contains object being written to.
//  Called with the baseline stack synced, except for R0 which is preserved.
//  All other registers are usable as scratch.
// This calls:
//    void PostWriteBarrier(JSRuntime* rt, JSObject* obj);
template <typename Handler>
bool BaselineCodeGen<Handler>::emitOutOfLinePostBarrierSlot() {
  masm.bind(&postBarrierSlot_);

  Register objReg = R2.scratchReg();
  AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
  regs.take(R0);
  regs.take(objReg);
  regs.take(BaselineFrameReg);
  Register scratch = regs.takeAny();
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64)
  // On ARM, save the link register before calling.  It contains the return
  // address.  The |masm.ret()| later will pop this into |pc| to return.
  masm.push(lr);
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
  masm.push(ra);
#endif
  masm.pushValue(R0);

  masm.setupUnalignedABICall(scratch);
  masm.movePtr(ImmPtr(cx->runtime()), scratch);
  masm.passABIArg(scratch);
  masm.passABIArg(objReg);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, PostWriteBarrier));

  masm.popValue(R0);
  masm.ret();
  return true;
}

template <>
bool BaselineCompilerCodeGen::emitNextIC() {
  // Emit a call to an IC stored in ICScript. Calls to this must match the
  // ICEntry order in ICScript: first the non-op IC entries for |this| and
  // formal arguments, then the for-op IC entries for JOF_IC ops.

  JSScript* script = handler.script();
  uint32_t pcOffset = script->pcToOffset(handler.pc());

  // We don't use every ICEntry and we can skip unreachable ops, so we have
  // to loop until we find an ICEntry for the current pc.
  const ICEntry* entry;
  do {
    entry = &script->icScript()->icEntry(handler.icEntryIndex());
    handler.moveToNextICEntry();
  } while (entry->pcOffset() < pcOffset);

  MOZ_RELEASE_ASSERT(entry->pcOffset() == pcOffset);
  MOZ_ASSERT_IF(!entry->isForPrologue(), BytecodeOpHasIC(JSOp(*handler.pc())));

  CodeOffset callOffset;
  EmitCallIC(masm, entry, &callOffset);

  RetAddrEntry::Kind kind = entry->isForPrologue()
                                ? RetAddrEntry::Kind::PrologueIC
                                : RetAddrEntry::Kind::IC;

  if (!handler.retAddrEntries().emplaceBack(pcOffset, kind, callOffset)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

template <>
bool BaselineInterpreterCodeGen::emitNextIC() {
  masm.loadPtr(frame.addressOfInterpreterICEntry(), ICStubReg);
  masm.loadPtr(Address(ICStubReg, ICEntry::offsetOfFirstStub()), ICStubReg);
  masm.call(Address(ICStubReg, ICStub::offsetOfStubCode()));
  return true;
}

template <typename Handler>
void BaselineCodeGen<Handler>::prepareVMCall() {
  pushedBeforeCall_ = masm.framePushed();
#ifdef DEBUG
  inCall_ = true;
#endif

  // Ensure everything is synced.
  frame.syncStack(0);

  // Save the frame pointer.
  masm.Push(BaselineFrameReg);
}

template <>
void BaselineCompilerCodeGen::storeFrameSizeAndPushDescriptor(
    uint32_t frameBaseSize, uint32_t argSize, const Address& frameSizeAddr,
    Register scratch1, Register scratch2) {
  uint32_t frameVals = frame.nlocals() + frame.stackDepth();

  uint32_t frameFullSize = frameBaseSize + (frameVals * sizeof(Value));
  masm.store32(Imm32(frameFullSize), frameSizeAddr);

  uint32_t descriptor = MakeFrameDescriptor(
      frameFullSize + argSize, FrameType::BaselineJS, ExitFrameLayout::Size());
  masm.push(Imm32(descriptor));
}

template <>
void BaselineInterpreterCodeGen::storeFrameSizeAndPushDescriptor(
    uint32_t frameBaseSize, uint32_t argSize, const Address& frameSizeAddr,
    Register scratch1, Register scratch2) {
  // scratch1 = FramePointer + BaselineFrame::FramePointerOffset - StackPointer.
  masm.computeEffectiveAddress(
      Address(BaselineFrameReg, BaselineFrame::FramePointerOffset), scratch1);
  masm.subStackPtrFrom(scratch1);

  // Store the frame size without VMFunction arguments. Use
  // computeEffectiveAddress instead of sub32 to avoid an extra move.
  masm.computeEffectiveAddress(Address(scratch1, -int32_t(argSize)), scratch2);
  masm.store32(scratch2, frameSizeAddr);

  // Push frame descriptor based on the full frame size.
  masm.makeFrameDescriptor(scratch1, FrameType::BaselineJS,
                           ExitFrameLayout::Size());
  masm.push(scratch1);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::callVMInternal(VMFunctionId id,
                                              CallVMPhase phase) {
#ifdef DEBUG
  // Assert prepareVMCall() has been called.
  MOZ_ASSERT(inCall_);
  inCall_ = false;

  // Assert the frame does not have an override pc when we're executing JIT
  // code.
  {
    Label ok;
    masm.branchTest32(Assembler::Zero, frame.addressOfFlags(),
                      Imm32(BaselineFrame::HAS_OVERRIDE_PC), &ok);
    masm.assumeUnreachable(
        "BaselineFrame shouldn't override pc when executing JIT code");
    masm.bind(&ok);
  }
#endif

  TrampolinePtr code = cx->runtime()->jitRuntime()->getVMWrapper(id);
  const VMFunctionData& fun = GetVMFunction(id);

  // Compute argument size. Note that this include the size of the frame pointer
  // pushed by prepareVMCall.
  uint32_t argSize = fun.explicitStackSlots() * sizeof(void*) + sizeof(void*);

  // Assert all arguments were pushed.
  MOZ_ASSERT(masm.framePushed() - pushedBeforeCall_ == argSize);

  Address frameSizeAddress(BaselineFrameReg,
                           BaselineFrame::reverseOffsetOfFrameSize());
  uint32_t frameBaseSize =
      BaselineFrame::FramePointerOffset + BaselineFrame::Size();
  if (phase == POST_INITIALIZE) {
    storeFrameSizeAndPushDescriptor(frameBaseSize, argSize, frameSizeAddress,
                                    R0.scratchReg(), R1.scratchReg());
  } else {
    MOZ_ASSERT(phase == CHECK_OVER_RECURSED);
    Label done, pushedFrameLocals;

    // If OVER_RECURSED is set, then frame locals haven't been pushed yet.
    masm.branchTest32(Assembler::Zero, frame.addressOfFlags(),
                      Imm32(BaselineFrame::OVER_RECURSED), &pushedFrameLocals);
    {
      masm.store32(Imm32(frameBaseSize), frameSizeAddress);
      uint32_t descriptor =
          MakeFrameDescriptor(frameBaseSize + argSize, FrameType::BaselineJS,
                              ExitFrameLayout::Size());
      masm.push(Imm32(descriptor));
      masm.jump(&done);
    }
    masm.bind(&pushedFrameLocals);
    {
      storeFrameSizeAndPushDescriptor(frameBaseSize, argSize, frameSizeAddress,
                                      R0.scratchReg(), R1.scratchReg());
    }
    masm.bind(&done);
  }
  MOZ_ASSERT(fun.expectTailCall == NonTailCall);
  // Perform the call.
  masm.call(code);
  uint32_t callOffset = masm.currentOffset();
  masm.Pop(BaselineFrameReg);

  // Pop arguments from framePushed.
  masm.implicitPop(fun.explicitStackSlots() * sizeof(void*));

#ifdef DEBUG
  // Assert the frame does not have an override pc when we're executing JIT
  // code.
  {
    Label ok;
    masm.branchTest32(Assembler::Zero, frame.addressOfFlags(),
                      Imm32(BaselineFrame::HAS_OVERRIDE_PC), &ok);
    masm.assumeUnreachable("BaselineFrame shouldn't override pc after VM call");
    masm.bind(&ok);
  }
#endif

  return handler.appendRetAddrEntry(cx, RetAddrEntry::Kind::CallVM, callOffset);
}

template <typename Handler>
template <typename Fn, Fn fn>
bool BaselineCodeGen<Handler>::callVM(CallVMPhase phase) {
  VMFunctionId fnId = VMFunctionToId<Fn, fn>::id;
  return callVMInternal(fnId, phase);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitStackCheck() {
  // If this is the late stack check for a frame which contains an early stack
  // check, then the early stack check might have failed and skipped past the
  // pushing of locals on the stack.
  //
  // If this is a possibility, then the OVER_RECURSED flag should be checked,
  // and the VMCall to CheckOverRecursedBaseline done unconditionally if it's
  // set.
  Label forceCall;
  if (handler.needsEarlyStackCheck()) {
    masm.branchTest32(Assembler::NonZero, frame.addressOfFlags(),
                      Imm32(BaselineFrame::OVER_RECURSED), &forceCall);
  }

  Label skipCall;
  masm.branchStackPtrRhs(Assembler::BelowOrEqual,
                         AbsoluteAddress(cx->addressOfJitStackLimit()),
                         &skipCall);

  if (handler.needsEarlyStackCheck()) {
    masm.bind(&forceCall);
  }

  prepareVMCall();
  masm.loadBaselineFramePtr(BaselineFrameReg, R1.scratchReg());
  pushArg(R1.scratchReg());

  CallVMPhase phase = POST_INITIALIZE;
  if (handler.needsEarlyStackCheck()) {
    phase = CHECK_OVER_RECURSED;
  }

  using Fn = bool (*)(JSContext*, BaselineFrame*);
  if (!callVMNonOp<Fn, CheckOverRecursedBaseline>(phase)) {
    return false;
  }

  handler.markLastRetAddrEntryKind(RetAddrEntry::Kind::StackCheck);

  masm.bind(&skipCall);
  return true;
}

static void EmitCallFrameIsDebuggeeCheck(MacroAssembler& masm) {
  masm.Push(BaselineFrameReg);
  masm.setupUnalignedABICall(R0.scratchReg());
  masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());
  masm.passABIArg(R0.scratchReg());
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, jit::FrameIsDebuggeeCheck));
  masm.Pop(BaselineFrameReg);
}

template <>
void BaselineCompilerCodeGen::emitIsDebuggeeCheck() {
  if (handler.compileDebugInstrumentation()) {
    EmitCallFrameIsDebuggeeCheck(masm);
  }
}

template <>
void BaselineInterpreterCodeGen::emitIsDebuggeeCheck() {
  // Use a toggled jump to call FrameIsDebuggeeCheck only if the debugger is
  // enabled.
  //
  // TODO(bug 1522394): consider having a cx->realm->isDebuggee guard before the
  // call. Consider moving the callWithABI out-of-line.

  Label skipCheck;
  CodeOffset toggleOffset = masm.toggledJump(&skipCheck);
  EmitCallFrameIsDebuggeeCheck(masm);
  masm.bind(&skipCheck);
  handler.setDebuggeeCheckOffset(toggleOffset);
}

static void MaybeIncrementCodeCoverageCounter(MacroAssembler& masm,
                                              JSScript* script,
                                              jsbytecode* pc) {
  if (!script->hasScriptCounts()) {
    return;
  }
  PCCounts* counts = script->maybeGetPCCounts(pc);
  uint64_t* counterAddr = &counts->numExec();
  masm.inc64(AbsoluteAddress(counterAddr));
}

template <>
bool BaselineCompilerCodeGen::emitHandleCodeCoverageAtPrologue() {
  // If the main instruction is not a jump target, then we emit the
  // corresponding code coverage counter.
  JSScript* script = handler.script();
  jsbytecode* main = script->main();
  if (!BytecodeIsJumpTarget(JSOp(*main))) {
    MaybeIncrementCodeCoverageCounter(masm, script, main);
  }
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emitHandleCodeCoverageAtPrologue() {
  Label skipCoverage;
  CodeOffset toggleOffset = masm.toggledJump(&skipCoverage);
  masm.call(handler.codeCoverageAtPrologueLabel());
  masm.bind(&skipCoverage);
  return handler.codeCoverageOffsets().append(toggleOffset.offset());
}

template <>
void BaselineCompilerCodeGen::subtractScriptSlotsSize(Register reg,
                                                      Register scratch) {
  uint32_t slotsSize = handler.script()->nslots() * sizeof(Value);
  masm.subPtr(Imm32(slotsSize), reg);
}

template <>
void BaselineInterpreterCodeGen::subtractScriptSlotsSize(Register reg,
                                                         Register scratch) {
  // reg = reg - script->nslots() * sizeof(Value)
  MOZ_ASSERT(reg != scratch);
  loadScript(scratch);
  masm.loadPtr(Address(scratch, JSScript::offsetOfScriptData()), scratch);
  masm.load32(Address(scratch, SharedScriptData::offsetOfNslots()), scratch);
  static_assert(sizeof(Value) == 8,
                "shift by 3 below assumes Value is 8 bytes");
  masm.lshiftPtr(Imm32(3), scratch);
  masm.subPtr(scratch, reg);
}

template <>
void BaselineCompilerCodeGen::loadGlobalLexicalEnvironment(Register dest) {
  masm.movePtr(ImmGCPtr(&cx->global()->lexicalEnvironment()), dest);
}

template <>
void BaselineInterpreterCodeGen::loadGlobalLexicalEnvironment(Register dest) {
  masm.loadPtr(AbsoluteAddress(cx->addressOfRealm()), dest);
  masm.loadPtr(Address(dest, Realm::offsetOfActiveLexicalEnvironment()), dest);
}

template <>
void BaselineCompilerCodeGen::pushGlobalLexicalEnvironmentValue(
    ValueOperand scratch) {
  frame.push(ObjectValue(cx->global()->lexicalEnvironment()));
}

template <>
void BaselineInterpreterCodeGen::pushGlobalLexicalEnvironmentValue(
    ValueOperand scratch) {
  loadGlobalLexicalEnvironment(scratch.scratchReg());
  masm.tagValue(JSVAL_TYPE_OBJECT, scratch.scratchReg(), scratch);
  frame.push(scratch);
}

template <>
void BaselineCompilerCodeGen::loadGlobalThisValue(ValueOperand dest) {
  masm.moveValue(cx->global()->lexicalEnvironment().thisValue(), dest);
}

template <>
void BaselineInterpreterCodeGen::loadGlobalThisValue(ValueOperand dest) {
  Register scratch = dest.scratchReg();
  loadGlobalLexicalEnvironment(scratch);
  static constexpr size_t SlotOffset =
      LexicalEnvironmentObject::offsetOfThisValueOrScopeSlot();
  masm.loadValue(Address(scratch, SlotOffset), dest);
}

template <>
void BaselineCompilerCodeGen::pushScriptArg() {
  pushArg(ImmGCPtr(handler.script()));
}

template <>
void BaselineInterpreterCodeGen::pushScriptArg() {
  pushArg(frame.addressOfInterpreterScript());
}

template <>
void BaselineCompilerCodeGen::pushBytecodePCArg() {
  pushArg(ImmPtr(handler.pc()));
}

template <>
void BaselineInterpreterCodeGen::pushBytecodePCArg() {
  pushArg(frame.addressOfInterpreterPC());
}

template <>
void BaselineCompilerCodeGen::pushScriptNameArg(Register scratch1,
                                                Register scratch2) {
  pushArg(ImmGCPtr(handler.script()->getName(handler.pc())));
}

template <>
void BaselineInterpreterCodeGen::pushScriptNameArg(Register scratch1,
                                                   Register scratch2) {
  MOZ_ASSERT(scratch1 != scratch2);

  masm.loadPtr(frame.addressOfInterpreterPC(), scratch1);
  LoadInt32Operand(masm, scratch1, scratch1);

  loadScriptAtom(scratch1, scratch2);
  pushArg(scratch2);
}

template <>
void BaselineCompilerCodeGen::pushScriptObjectArg(ScriptObjectType type) {
  JSScript* script = handler.script();
  switch (type) {
    case ScriptObjectType::RegExp:
      pushArg(ImmGCPtr(script->getRegExp(handler.pc())));
      return;
    case ScriptObjectType::Function:
      pushArg(ImmGCPtr(script->getFunction(handler.pc())));
      return;
  }
  MOZ_CRASH("Unexpected object type");
}

template <>
void BaselineInterpreterCodeGen::pushScriptObjectArg(ScriptObjectType type) {
  MOZ_CRASH("NYI: interpreter pushScriptObjectArg");
}

template <>
void BaselineCompilerCodeGen::pushScriptScopeArg() {
  pushArg(ImmGCPtr(handler.script()->getScope(handler.pc())));
}

template <>
void BaselineInterpreterCodeGen::pushScriptScopeArg() {
  MOZ_CRASH("NYI: interpreter pushScriptScopeArg");
}

template <>
void BaselineCompilerCodeGen::pushUint8BytecodeOperandArg(Register) {
  MOZ_ASSERT(JOF_OPTYPE(JSOp(*handler.pc())) == JOF_UINT8);
  pushArg(Imm32(GET_UINT8(handler.pc())));
}

template <>
void BaselineInterpreterCodeGen::pushUint8BytecodeOperandArg(Register scratch) {
  masm.loadPtr(frame.addressOfInterpreterPC(), scratch);
  LoadUint8Operand(masm, scratch, scratch);
  pushArg(scratch);
}

template <>
void BaselineCompilerCodeGen::pushUint16BytecodeOperandArg(Register) {
  MOZ_ASSERT(JOF_OPTYPE(JSOp(*handler.pc())) == JOF_UINT16);
  pushArg(Imm32(GET_UINT16(handler.pc())));
}

template <>
void BaselineInterpreterCodeGen::pushUint16BytecodeOperandArg(
    Register scratch) {
  masm.loadPtr(frame.addressOfInterpreterPC(), scratch);
  LoadUint16Operand(masm, scratch, scratch);
  pushArg(scratch);
}

template <>
void BaselineCompilerCodeGen::loadInt32LengthBytecodeOperand(Register dest) {
  uint32_t length = GET_UINT32(handler.pc());
  MOZ_ASSERT(length <= INT32_MAX,
             "the bytecode emitter must fail to compile code that would "
             "produce a length exceeding int32_t range");
  masm.move32(Imm32(AssertedCast<int32_t>(length)), dest);
}

template <>
void BaselineInterpreterCodeGen::loadInt32LengthBytecodeOperand(Register dest) {
  masm.loadPtr(frame.addressOfInterpreterPC(), dest);
  LoadInt32Operand(masm, dest, dest);
}

template <>
void BaselineCompilerCodeGen::loadInt32IndexBytecodeOperand(ValueOperand dest) {
  uint32_t index = GET_UINT32(handler.pc());
  MOZ_ASSERT(index <= INT32_MAX,
             "the bytecode emitter must fail to compile code that would "
             "produce an index exceeding int32_t range");
  masm.moveValue(Int32Value(AssertedCast<int32_t>(index)), dest);
}

template <>
void BaselineInterpreterCodeGen::loadInt32IndexBytecodeOperand(
    ValueOperand dest) {
  Register scratch = dest.scratchReg();
  masm.loadPtr(frame.addressOfInterpreterPC(), scratch);
  LoadInt32Operand(masm, scratch, scratch);
  masm.tagValue(JSVAL_TYPE_INT32, scratch, dest);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitDebugPrologue() {
  auto ifDebuggee = [this]() {
    // Load pointer to BaselineFrame in R0.
    masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

    prepareVMCall();
    pushBytecodePCArg();
    pushArg(R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*, jsbytecode*, bool*);
    if (!callVM<Fn, jit::DebugPrologue>()) {
      return false;
    }

    // Fix up the RetAddrEntry appended by callVM for on-stack recompilation.
    handler.markLastRetAddrEntryKind(RetAddrEntry::Kind::DebugPrologue);

    // If the stub returns |true|, we have to return the value stored in the
    // frame's return value slot.
    Label done;
    masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, &done);
    {
      masm.loadValue(frame.addressOfReturnValue(), JSReturnOperand);
      masm.jump(&return_);
    }
    masm.bind(&done);
    return true;
  };
  if (!emitDebugInstrumentation(ifDebuggee)) {
    return false;
  }

  debugOsrPrologueOffset_ = CodeOffset(masm.currentOffset());

  return true;
}

template <>
void BaselineCompilerCodeGen::emitPreInitEnvironmentChain(
    Register nonFunctionEnv) {
  if (handler.function()) {
    masm.storePtr(ImmPtr(nullptr), frame.addressOfEnvironmentChain());
  } else {
    masm.storePtr(nonFunctionEnv, frame.addressOfEnvironmentChain());
  }
}

template <>
void BaselineInterpreterCodeGen::emitPreInitEnvironmentChain(
    Register nonFunctionEnv) {
  Label notFunction, done;
  masm.branchTestPtr(Assembler::NonZero, frame.addressOfCalleeToken(),
                     Imm32(CalleeTokenScriptBit), &notFunction);
  {
    masm.storePtr(ImmPtr(nullptr), frame.addressOfEnvironmentChain());
    masm.jump(&done);
  }
  masm.bind(&notFunction);
  { masm.storePtr(nonFunctionEnv, frame.addressOfEnvironmentChain()); }
  masm.bind(&done);
}

template <>
void BaselineCompilerCodeGen::emitInitFrameFields() {
  masm.store32(Imm32(0), frame.addressOfFlags());
}

template <>
void BaselineInterpreterCodeGen::emitInitFrameFields() {
  Register scratch1 = R0.scratchReg();
  Register scratch2 = R2.scratchReg();

  masm.store32(Imm32(BaselineFrame::RUNNING_IN_INTERPRETER),
               frame.addressOfFlags());

  // Initialize interpreterScript.
  Label notFunction, done;
  masm.loadPtr(frame.addressOfCalleeToken(), scratch1);
  masm.branchTestPtr(Assembler::NonZero, scratch1, Imm32(CalleeTokenScriptBit),
                     &notFunction);
  {
    // CalleeToken_Function or CalleeToken_FunctionConstructing.
    masm.andPtr(Imm32(uint32_t(CalleeTokenMask)), scratch1);
    masm.loadPtr(Address(scratch1, JSFunction::offsetOfScript()), scratch1);
    masm.jump(&done);
  }
  masm.bind(&notFunction);
  {
    // CalleeToken_Script.
    masm.andPtr(Imm32(uint32_t(CalleeTokenMask)), scratch1);
  }
  masm.bind(&done);
  masm.storePtr(scratch1, frame.addressOfInterpreterScript());

  // Initialize interpreterICEntry.
  masm.loadPtr(Address(scratch1, JSScript::offsetOfTypes()), scratch2);
  masm.loadPtr(Address(scratch2, TypeScript::offsetOfICScript()), scratch2);
  masm.computeEffectiveAddress(Address(scratch2, ICScript::offsetOfICEntries()),
                               scratch2);
  masm.storePtr(scratch2, frame.addressOfInterpreterICEntry());

  // Initialize interpreterPC.
  masm.loadPtr(Address(scratch1, JSScript::offsetOfScriptData()), scratch1);
  masm.load32(Address(scratch1, SharedScriptData::offsetOfCodeOffset()),
              scratch2);
  masm.addPtr(scratch2, scratch1);
  masm.storePtr(scratch1, frame.addressOfInterpreterPC());
}

template <>
template <typename F1, typename F2>
bool BaselineCompilerCodeGen::initEnvironmentChainHelper(
    const F1& initFunctionEnv, const F2& initGlobalOrEvalEnv,
    Register scratch) {
  if (handler.function()) {
    return initFunctionEnv();
  }
  if (handler.module()) {
    return true;
  }
  return initGlobalOrEvalEnv();
}

template <>
template <typename F1, typename F2>
bool BaselineInterpreterCodeGen::initEnvironmentChainHelper(
    const F1& initFunctionEnv, const F2& initGlobalOrEvalEnv,
    Register scratch) {
  // There are three cases:
  //
  // 1) Function script: use code emitted by initFunctionEnv.
  // 2) Module script: no-op.
  // 3) Global or eval script: use code emitted by initGlobalOrEvalEnv.

  Label notFunction, done;
  masm.loadPtr(frame.addressOfCalleeToken(), scratch);
  masm.branchTestPtr(Assembler::NonZero, scratch, Imm32(CalleeTokenScriptBit),
                     &notFunction);
  {
    if (!initFunctionEnv()) {
      return false;
    }
    masm.jump(&done);
  }
  masm.bind(&notFunction);
  {
    masm.andPtr(Imm32(uint32_t(CalleeTokenMask)), scratch);
    masm.branchTest32(Assembler::NonZero,
                      Address(scratch, JSScript::offsetOfImmutableFlags()),
                      Imm32(uint32_t(JSScript::ImmutableFlags::IsModule)),
                      &done);
    {
      if (!initGlobalOrEvalEnv()) {
        return false;
      }
    }
  }

  masm.bind(&done);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::initEnvironmentChain() {
  CallVMPhase phase = POST_INITIALIZE;
  if (handler.needsEarlyStackCheck()) {
    phase = CHECK_OVER_RECURSED;
  }

  auto initFunctionEnv = [this, phase]() {
    // Use callee->environment as env chain. Note that we do this also
    // for needsSomeEnvironmentObject functions, so that the env chain
    // slot is properly initialized if the call triggers GC.
    Register callee = R0.scratchReg();
    Register scope = R1.scratchReg();
    masm.loadFunctionFromCalleeToken(frame.addressOfCalleeToken(), callee);
    masm.loadPtr(Address(callee, JSFunction::offsetOfEnvironment()), scope);
    masm.storePtr(scope, frame.addressOfEnvironmentChain());

    auto initEnv = [this, phase]() {
      // Call into the VM to create the proper environment objects.
      prepareVMCall();

      masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());
      pushArg(R0.scratchReg());

      using Fn = bool (*)(JSContext*, BaselineFrame*);
      return callVMNonOp<Fn, jit::InitFunctionEnvironmentObjects>(phase);
    };
    return emitTestScriptFlag(
        JSScript::ImmutableFlags::NeedsFunctionEnvironmentObjects, true,
        initEnv, R2.scratchReg());
  };

  auto initGlobalOrEvalEnv = [this, phase]() {
    // EnvironmentChain pointer in BaselineFrame has already been initialized
    // in prologue, but we need to check for redeclaration errors in global and
    // eval scripts.

    prepareVMCall();

    pushScriptArg();
    masm.loadPtr(frame.addressOfEnvironmentChain(), R0.scratchReg());
    pushArg(R0.scratchReg());

    using Fn = bool (*)(JSContext*, HandleObject, HandleScript);
    return callVMNonOp<Fn, js::CheckGlobalOrEvalDeclarationConflicts>(phase);
  };

  return initEnvironmentChainHelper(initFunctionEnv, initGlobalOrEvalEnv,
                                    R2.scratchReg());
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitInterruptCheck() {
  frame.syncStack(0);

  Label done;
  masm.branch32(Assembler::Equal, AbsoluteAddress(cx->addressOfInterruptBits()),
                Imm32(0), &done);

  prepareVMCall();

  using Fn = bool (*)(JSContext*);
  if (!callVM<Fn, InterruptCheck>()) {
    return false;
  }

  masm.bind(&done);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emitWarmUpCounterIncrement() {
  // Emit no warm-up counter increments or bailouts if Ion is not
  // enabled, or if the script will never be Ion-compileable

  if (!handler.maybeIonCompileable()) {
    return true;
  }

  frame.assertSyncedStack();

  Register scriptReg = R2.scratchReg();
  Register countReg = R0.scratchReg();
  Address warmUpCounterAddr(scriptReg, JSScript::offsetOfWarmUpCounter());

  JSScript* script = handler.script();
  masm.movePtr(ImmGCPtr(script), scriptReg);
  masm.load32(warmUpCounterAddr, countReg);
  masm.add32(Imm32(1), countReg);
  masm.store32(countReg, warmUpCounterAddr);

  jsbytecode* pc = handler.pc();
  if (JSOp(*pc) == JSOP_LOOPENTRY) {
    // If this is a loop inside a catch or finally block, increment the warmup
    // counter but don't attempt OSR (Ion only compiles the try block).
    if (handler.analysis().info(pc).loopEntryInCatchOrFinally) {
      return true;
    }

    if (!LoopEntryCanIonOsr(pc)) {
      // OSR into Ion not possible at this loop entry.
      return true;
    }
  }

  Label skipCall;

  const OptimizationInfo* info =
      IonOptimizations.get(IonOptimizations.firstLevel());
  uint32_t warmUpThreshold = info->compilerWarmUpThreshold(script, pc);
  masm.branch32(Assembler::LessThan, countReg, Imm32(warmUpThreshold),
                &skipCall);

  masm.branchPtr(Assembler::Equal,
                 Address(scriptReg, JSScript::offsetOfIonScript()),
                 ImmPtr(ION_COMPILING_SCRIPT), &skipCall);

  // Try to compile and/or finish a compilation.
  if (JSOp(*pc) == JSOP_LOOPENTRY) {
    // During the loop entry we can try to OSR into ion.
    // The ic has logic for this.
    if (!emitNextIC()) {
      return false;
    }
  } else {
    // To call stubs we need to have an opcode. This code handles the
    // prologue and there is no dedicatd opcode present. Therefore use an
    // annotated vm call.
    prepareVMCall();

    pushBytecodePCArg();
    masm.PushBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*, jsbytecode*);
    if (!callVM<Fn, IonCompileScriptForBaseline>()) {
      return false;
    }

    // Annotate the RetAddrEntry as warmup counter.
    handler.markLastRetAddrEntryKind(RetAddrEntry::Kind::WarmupCounter);
  }
  masm.bind(&skipCall);

  return true;
}

template <>
bool BaselineInterpreterCodeGen::emitWarmUpCounterIncrement() {
  Register scriptReg = R2.scratchReg();
  Register countReg = R0.scratchReg();

  // Bump warm-up counter.
  Address warmUpCounterAddr(scriptReg, JSScript::offsetOfWarmUpCounter());
  loadScript(scriptReg);
  masm.load32(warmUpCounterAddr, countReg);
  masm.add32(Imm32(1), countReg);
  masm.store32(countReg, warmUpCounterAddr);

  // If the script is warm enough for Baseline compilation, call into the VM to
  // compile it.
  Label done;
  masm.branch32(Assembler::BelowOrEqual, countReg,
                Imm32(JitOptions.baselineWarmUpThreshold), &done);
  masm.branchPtr(Assembler::Equal,
                 Address(scriptReg, JSScript::offsetOfBaselineScript()),
                 ImmPtr(BASELINE_DISABLED_SCRIPT), &done);
  {
    prepareVMCall();

    masm.PushBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*, uint8_t**);
    if (!callVM<Fn, BaselineCompileFromBaselineInterpreter>()) {
      return false;
    }

    // If the function returned nullptr we either skipped compilation or were
    // unable to compile the script. Continue running in the interpreter.
    masm.branchTestPtr(Assembler::Zero, ReturnReg, ReturnReg, &done);

    // Success! Switch from interpreter to JIT code by jumping to the
    // corresponding code in the BaselineScript.
    //
    // This works because BaselineCompiler uses the same frame layout (stack is
    // synced at OSR points) and BaselineCompileFromBaselineInterpreter has
    // already cleared the RUNNING_IN_INTERPRETER flag for us.
    // See BaselineFrame::prepareForBaselineInterpreterToJitOSR.
    masm.jump(ReturnReg);
  }

  masm.bind(&done);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emitArgumentTypeChecks() {
  if (!handler.function()) {
    return true;
  }

  frame.pushThis();
  frame.popRegsAndSync(1);

  if (!emitNextIC()) {
    return false;
  }

  size_t nargs = handler.function()->nargs();

  for (size_t i = 0; i < nargs; i++) {
    frame.pushArg(i);
    frame.popRegsAndSync(1);

    if (!emitNextIC()) {
      return false;
    }
  }

  return true;
}

template <>
bool BaselineInterpreterCodeGen::emitArgumentTypeChecks() {
  Register scratch1 = R1.scratchReg();

  // If the script is not a function, we're done.
  Label done;
  masm.loadPtr(frame.addressOfCalleeToken(), scratch1);
  masm.branchTestPtr(Assembler::NonZero, scratch1, Imm32(CalleeTokenScriptBit),
                     &done);

  // CalleeToken_Function or CalleeToken_FunctionConstructing.
  masm.andPtr(Imm32(uint32_t(CalleeTokenMask)), scratch1);

  // Store nargs in the frame's scratch slot.
  masm.load16ZeroExtend(Address(scratch1, JSFunction::offsetOfNargs()),
                        scratch1);
  masm.store32(scratch1, frame.addressOfScratchValue());

  // Type check |this|.
  masm.loadValue(frame.addressOfThis(), R0);
  if (!emitNextIC()) {
    return false;
  }
  frame.bumpInterpreterICEntry();

  // Type check arguments. Scratch1 holds the next argument's index.
  masm.move32(Imm32(0), scratch1);

  // Bounds check.
  Label top;
  masm.bind(&top);
  masm.branch32(Assembler::Equal, frame.addressOfScratchValue(), scratch1,
                &done);
  {
    // Load the argument, increment argument index. Use the frame's return value
    // slot to store this index across the IC call.
    BaseValueIndex addr(BaselineFrameReg, scratch1,
                        BaselineFrame::offsetOfArg(0));
    masm.loadValue(addr, R0);
    masm.add32(Imm32(1), scratch1);
    masm.store32(scratch1, frame.addressOfReturnValue());

    // Type check the argument.
    if (!emitNextIC()) {
      return false;
    }
    frame.bumpInterpreterICEntry();

    // Restore argument index.
    masm.load32(frame.addressOfReturnValue(), scratch1);
    masm.jump(&top);
  }

  masm.bind(&done);
  return true;
}

bool BaselineCompiler::emitDebugTrap() {
  MOZ_ASSERT(compileDebugInstrumentation());
  MOZ_ASSERT(frame.numUnsyncedSlots() == 0);

  JSScript* script = handler.script();
  bool enabled =
      script->stepModeEnabled() || script->hasBreakpointsAt(handler.pc());

#if defined(JS_CODEGEN_ARM64)
  // Flush any pending constant pools to prevent incorrect
  // PCMappingEntry offsets. See Bug 1446819.
  masm.flush();
  // Fix up the PCMappingEntry to avoid any constant pool.
  pcMappingEntries_.back().nativeOffset = masm.currentOffset();
#endif

  // Emit patchable call to debug trap handler.
  JitCode* handlerCode = cx->runtime()->jitRuntime()->debugTrapHandler(
      cx, DebugTrapHandlerKind::Compiler);
  if (!handlerCode) {
    return false;
  }
  mozilla::DebugOnly<CodeOffset> offset =
      masm.toggledCall(handlerCode, enabled);

#ifdef DEBUG
  // Patchable call offset has to match the pc mapping offset.
  PCMappingEntry& entry = pcMappingEntries_.back();
  MOZ_ASSERT((&offset)->offset() == entry.nativeOffset);
#endif

  // Add a RetAddrEntry for the return offset -> pc mapping.
  return handler.appendRetAddrEntry(cx, RetAddrEntry::Kind::DebugTrap,
                                    masm.currentOffset());
}

#ifdef JS_TRACE_LOGGING
template <>
bool BaselineCompilerCodeGen::emitTraceLoggerEnter() {
  AllocatableRegisterSet regs(RegisterSet::Volatile());
  Register loggerReg = regs.takeAnyGeneral();
  Register scriptReg = regs.takeAnyGeneral();

  Label noTraceLogger;
  if (!traceLoggerToggleOffsets_.append(masm.toggledJump(&noTraceLogger))) {
    return false;
  }

  masm.Push(loggerReg);
  masm.Push(scriptReg);

  masm.loadTraceLogger(loggerReg);

  // Script start.
  masm.movePtr(ImmGCPtr(handler.script()), scriptReg);
  masm.loadPtr(Address(scriptReg, JSScript::offsetOfBaselineScript()),
               scriptReg);
  Address scriptEvent(scriptReg,
                      BaselineScript::offsetOfTraceLoggerScriptEvent());
  masm.computeEffectiveAddress(scriptEvent, scriptReg);
  masm.tracelogStartEvent(loggerReg, scriptReg);

  // Engine start.
  masm.tracelogStartId(loggerReg, TraceLogger_Baseline, /* force = */ true);

  masm.Pop(scriptReg);
  masm.Pop(loggerReg);

  masm.bind(&noTraceLogger);

  return true;
}

template <>
bool BaselineInterpreterCodeGen::emitTraceLoggerEnter() {
  MOZ_CRASH("NYI: interpreter emitTraceLoggerEnter");
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitTraceLoggerExit() {
  AllocatableRegisterSet regs(RegisterSet::Volatile());
  Register loggerReg = regs.takeAnyGeneral();

  Label noTraceLogger;
  if (!traceLoggerToggleOffsets_.append(masm.toggledJump(&noTraceLogger))) {
    return false;
  }

  masm.Push(loggerReg);
  masm.loadTraceLogger(loggerReg);

  masm.tracelogStopId(loggerReg, TraceLogger_Baseline, /* force = */ true);
  masm.tracelogStopId(loggerReg, TraceLogger_Scripts, /* force = */ true);

  masm.Pop(loggerReg);

  masm.bind(&noTraceLogger);

  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitTraceLoggerResume(
    Register baselineScript, AllocatableGeneralRegisterSet& regs) {
  Register scriptId = regs.takeAny();
  Register loggerReg = regs.takeAny();

  Label noTraceLogger;
  if (!traceLoggerToggleOffsets_.append(masm.toggledJump(&noTraceLogger))) {
    return false;
  }

  masm.loadTraceLogger(loggerReg);

  Address scriptEvent(baselineScript,
                      BaselineScript::offsetOfTraceLoggerScriptEvent());
  masm.computeEffectiveAddress(scriptEvent, scriptId);
  masm.tracelogStartEvent(loggerReg, scriptId);
  masm.tracelogStartId(loggerReg, TraceLogger_Baseline, /* force = */ true);

  regs.add(loggerReg);
  regs.add(scriptId);

  masm.bind(&noTraceLogger);

  return true;
}
#endif

template <typename Handler>
void BaselineCodeGen<Handler>::emitProfilerEnterFrame() {
  // Store stack position to lastProfilingFrame variable, guarded by a toggled
  // jump. Starts off initially disabled.
  Label noInstrument;
  CodeOffset toggleOffset = masm.toggledJump(&noInstrument);
  masm.profilerEnterFrame(masm.getStackPointer(), R0.scratchReg());
  masm.bind(&noInstrument);

  // Store the start offset in the appropriate location.
  MOZ_ASSERT(!profilerEnterFrameToggleOffset_.bound());
  profilerEnterFrameToggleOffset_ = toggleOffset;
}

template <typename Handler>
void BaselineCodeGen<Handler>::emitProfilerExitFrame() {
  // Store previous frame to lastProfilingFrame variable, guarded by a toggled
  // jump. Starts off initially disabled.
  Label noInstrument;
  CodeOffset toggleOffset = masm.toggledJump(&noInstrument);
  masm.profilerExitFrame();
  masm.bind(&noInstrument);

  // Store the start offset in the appropriate location.
  MOZ_ASSERT(!profilerExitFrameToggleOffset_.bound());
  profilerExitFrameToggleOffset_ = toggleOffset;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_NOP() {
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_ITERNEXT() {
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_NOP_DESTRUCTURING() {
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_TRY_DESTRUCTURING() {
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_LABEL() {
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_POP() {
  frame.pop();
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_POPN() {
  frame.popn(GET_UINT16(handler.pc()));
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_POPN() {
  LoadUint16Operand(masm, PCRegAtStart, R0.scratchReg());
  frame.popn(R0.scratchReg());
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_DUPAT() {
  frame.syncStack(0);

  // DUPAT takes a value on the stack and re-pushes it on top.  It's like
  // GETLOCAL but it addresses from the top of the stack instead of from the
  // stack frame.

  int depth = -(GET_UINT24(handler.pc()) + 1);
  masm.loadValue(frame.addressOfStackValue(depth), R0);
  frame.push(R0);
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_DUPAT() {
  LoadUint24Operand(masm, PCRegAtStart, 0, R0.scratchReg());
  masm.loadValue(frame.addressOfStackValue(R0.scratchReg()), R0);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DUP() {
  // Keep top stack value in R0, sync the rest so that we can use R1. We use
  // separate registers because every register can be used by at most one
  // StackValue.
  frame.popRegsAndSync(1);
  masm.moveValue(R0, R1);

  // inc/dec ops use DUP followed by ONE, ADD. Push R0 last to avoid a move.
  frame.push(R1);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DUP2() {
  frame.syncStack(0);

  masm.loadValue(frame.addressOfStackValue(-2), R0);
  masm.loadValue(frame.addressOfStackValue(-1), R1);

  frame.push(R0);
  frame.push(R1);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SWAP() {
  // Keep top stack values in R0 and R1.
  frame.popRegsAndSync(2);

  frame.push(R1);
  frame.push(R0);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_PICK() {
  frame.syncStack(0);

  // Pick takes a value on the stack and moves it to the top.
  // For instance, pick 2:
  //     before: A B C D E
  //     after : A B D E C

  // First, move value at -(amount + 1) into R0.
  int32_t depth = -(GET_INT8(handler.pc()) + 1);
  masm.loadValue(frame.addressOfStackValue(depth), R0);

  // Move the other values down.
  depth++;
  for (; depth < 0; depth++) {
    Address source = frame.addressOfStackValue(depth);
    Address dest = frame.addressOfStackValue(depth - 1);
    masm.loadValue(source, R1);
    masm.storeValue(R1, dest);
  }

  // Push R0.
  frame.pop();
  frame.push(R0);
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_PICK() {
  // First, move the value to move up into R0.
  LoadUint8Operand(masm, PCRegAtStart, PCRegAtStart);
  masm.loadValue(frame.addressOfStackValue(PCRegAtStart), R0);

  // Move the other values down.
  Label top, done;
  masm.bind(&top);
  masm.sub32(Imm32(1), PCRegAtStart);
  masm.branchTest32(Assembler::Signed, PCRegAtStart, PCRegAtStart, &done);
  {
    masm.loadValue(frame.addressOfStackValue(PCRegAtStart), R1);
    masm.storeValue(R1, frame.addressOfStackValue(PCRegAtStart, sizeof(Value)));
    masm.jump(&top);
  }

  masm.bind(&done);

  // Replace value on top of the stack with R0.
  masm.storeValue(R0, frame.addressOfStackValue(-1));
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_UNPICK() {
  frame.syncStack(0);

  // Pick takes the top of the stack value and moves it under the nth value.
  // For instance, unpick 2:
  //     before: A B C D E
  //     after : A B E C D

  // First, move value at -1 into R0.
  masm.loadValue(frame.addressOfStackValue(-1), R0);

  MOZ_ASSERT(GET_INT8(handler.pc()) > 0,
             "Interpreter code assumes JSOP_UNPICK operand > 0");

  // Move the other values up.
  int32_t depth = -(GET_INT8(handler.pc()) + 1);
  for (int32_t i = -1; i > depth; i--) {
    Address source = frame.addressOfStackValue(i - 1);
    Address dest = frame.addressOfStackValue(i);
    masm.loadValue(source, R1);
    masm.storeValue(R1, dest);
  }

  // Store R0 under the nth value.
  Address dest = frame.addressOfStackValue(depth);
  masm.storeValue(R0, dest);
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_UNPICK() {
  LoadUint8Operand(masm, PCRegAtStart, PCRegAtStart);

  // Move the top value into R0.
  masm.loadValue(frame.addressOfStackValue(-1), R0);

  // Overwrite the nth stack value with R0 but first save the old value in R1.
  masm.loadValue(frame.addressOfStackValue(PCRegAtStart), R1);
  masm.storeValue(R0, frame.addressOfStackValue(PCRegAtStart));

  // Now for each slot x in [n-1, 1] do the following:
  //
  // * Store the value in slot x in R0.
  // * Store the value in the previous slot (now in R1) in slot x.
  // * Move R0 to R1.

#ifdef DEBUG
  // Assert the operand > 0 so the sub32 below doesn't "underflow" to negative
  // values.
  {
    Label ok;
    masm.branch32(Assembler::GreaterThan, PCRegAtStart, Imm32(0), &ok);
    masm.assumeUnreachable("JSOP_UNPICK with operand <= 0?");
    masm.bind(&ok);
  }
#endif

  Label top, done;
  masm.bind(&top);
  masm.sub32(Imm32(1), PCRegAtStart);
  masm.branchTest32(Assembler::Zero, PCRegAtStart, PCRegAtStart, &done);
  {
    // Overwrite stack slot x with slot x + 1, saving the old value in R1.
    masm.loadValue(frame.addressOfStackValue(PCRegAtStart), R0);
    masm.storeValue(R1, frame.addressOfStackValue(PCRegAtStart));
    masm.moveValue(R0, R1);
    masm.jump(&top);
  }

  // Finally, replace the value on top of the stack (slot 0) with R1. This is
  // the value that used to be in slot 1.
  masm.bind(&done);
  masm.storeValue(R1, frame.addressOfStackValue(-1));
  return true;
}

template <>
void BaselineCompilerCodeGen::emitJump() {
  jsbytecode* pc = handler.pc();
  MOZ_ASSERT(IsJumpOpcode(JSOp(*pc)));
  frame.assertSyncedStack();

  jsbytecode* target = pc + GET_JUMP_OFFSET(pc);
  masm.jump(handler.labelOf(target));
}

template <>
void BaselineInterpreterCodeGen::emitJump() {
  // We have to add the current pc's jump offset to the frame's pc. We can use
  // R0 as scratch because we jump to the "next op" label and that assumes a
  // synced stack.
  Register scratch = R0.scratchReg();
  masm.loadPtr(frame.addressOfInterpreterPC(), scratch);
  LoadInt32OperandSignExtendToPtr(masm, scratch, scratch);
  masm.addPtr(frame.addressOfInterpreterPC(), scratch);
  masm.storePtr(scratch, frame.addressOfInterpreterPC());
  masm.jump(handler.interpretOpLabel());
}

template <>
void BaselineCompilerCodeGen::emitTestBooleanTruthy(bool branchIfTrue,
                                                    ValueOperand val) {
  jsbytecode* pc = handler.pc();
  MOZ_ASSERT(IsJumpOpcode(JSOp(*pc)));
  frame.assertSyncedStack();

  jsbytecode* target = pc + GET_JUMP_OFFSET(pc);
  masm.branchTestBooleanTruthy(branchIfTrue, val, handler.labelOf(target));
}

template <>
void BaselineInterpreterCodeGen::emitTestBooleanTruthy(bool branchIfTrue,
                                                       ValueOperand val) {
  Label done;
  masm.branchTestBooleanTruthy(!branchIfTrue, val, &done);
  emitJump();
  masm.bind(&done);
}

template <>
template <typename F1, typename F2>
MOZ_MUST_USE bool BaselineCompilerCodeGen::emitTestScriptFlag(
    JSScript::ImmutableFlags flag, const F1& ifSet, const F2& ifNotSet,
    Register scratch) {
  return handler.script()->hasFlag(flag) ? ifSet() : ifNotSet();
}

template <>
template <typename F1, typename F2>
MOZ_MUST_USE bool BaselineInterpreterCodeGen::emitTestScriptFlag(
    JSScript::ImmutableFlags flag, const F1& ifSet, const F2& ifNotSet,
    Register scratch) {
  Label flagNotSet, done;
  loadScript(scratch);
  masm.branchTest32(Assembler::Zero,
                    Address(scratch, JSScript::offsetOfImmutableFlags()),
                    Imm32(uint32_t(flag)), &flagNotSet);
  {
    if (!ifSet()) {
      return false;
    }
    masm.jump(&done);
  }
  masm.bind(&flagNotSet);
  {
    if (!ifNotSet()) {
      return false;
    }
  }

  masm.bind(&done);
  return true;
}

template <>
template <typename F>
MOZ_MUST_USE bool BaselineCompilerCodeGen::emitTestScriptFlag(
    JSScript::ImmutableFlags flag, bool value, const F& emit,
    Register scratch) {
  if (handler.script()->hasFlag(flag) == value) {
    return emit();
  }
  return true;
}

template <>
template <typename F>
MOZ_MUST_USE bool BaselineCompilerCodeGen::emitTestScriptFlag(
    JSScript::MutableFlags flag, bool value, const F& emit, Register scratch) {
  if (handler.script()->hasFlag(flag) == value) {
    return emit();
  }
  return true;
}

template <>
template <typename F>
MOZ_MUST_USE bool BaselineInterpreterCodeGen::emitTestScriptFlag(
    JSScript::ImmutableFlags flag, bool value, const F& emit,
    Register scratch) {
  Label done;
  loadScript(scratch);
  masm.branchTest32(value ? Assembler::Zero : Assembler::NonZero,
                    Address(scratch, JSScript::offsetOfImmutableFlags()),
                    Imm32(uint32_t(flag)), &done);
  {
    if (!emit()) {
      return false;
    }
  }

  masm.bind(&done);
  return true;
}

template <>
template <typename F>
MOZ_MUST_USE bool BaselineInterpreterCodeGen::emitTestScriptFlag(
    JSScript::MutableFlags flag, bool value, const F& emit, Register scratch) {
  Label done;
  loadScript(scratch);
  masm.branchTest32(value ? Assembler::Zero : Assembler::NonZero,
                    Address(scratch, JSScript::offsetOfMutableFlags()),
                    Imm32(uint32_t(flag)), &done);
  {
    if (!emit()) {
      return false;
    }
  }

  masm.bind(&done);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GOTO() {
  frame.syncStack(0);
  emitJump();
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitToBoolean() {
  Label skipIC;
  masm.branchTestBoolean(Assembler::Equal, R0, &skipIC);

  // Call IC
  if (!emitNextIC()) {
    return false;
  }

  masm.bind(&skipIC);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitTest(bool branchIfTrue) {
  bool knownBoolean = frame.stackValueHasKnownType(-1, JSVAL_TYPE_BOOLEAN);

  // Keep top stack value in R0.
  frame.popRegsAndSync(1);

  if (!knownBoolean && !emitToBoolean()) {
    return false;
  }

  // IC will leave a BooleanValue in R0, just need to branch on it.
  emitTestBooleanTruthy(branchIfTrue, R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_IFEQ() {
  return emitTest(false);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_IFNE() {
  return emitTest(true);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitAndOr(bool branchIfTrue) {
  bool knownBoolean = frame.stackValueHasKnownType(-1, JSVAL_TYPE_BOOLEAN);

  // AND and OR leave the original value on the stack.
  frame.syncStack(0);

  masm.loadValue(frame.addressOfStackValue(-1), R0);
  if (!knownBoolean && !emitToBoolean()) {
    return false;
  }

  emitTestBooleanTruthy(branchIfTrue, R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_AND() {
  return emitAndOr(false);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_OR() {
  return emitAndOr(true);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_NOT() {
  bool knownBoolean = frame.stackValueHasKnownType(-1, JSVAL_TYPE_BOOLEAN);

  // Keep top stack value in R0.
  frame.popRegsAndSync(1);

  if (!knownBoolean && !emitToBoolean()) {
    return false;
  }

  masm.notBoolean(R0);

  frame.push(R0, JSVAL_TYPE_BOOLEAN);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_POS() {
  // Keep top stack value in R0.
  frame.popRegsAndSync(1);

  // Inline path for int32 and double; otherwise call VM.
  Label done;
  masm.branchTestNumber(Assembler::Equal, R0, &done);

  prepareVMCall();
  pushArg(R0);

  using Fn = bool (*)(JSContext*, HandleValue, MutableHandleValue);
  if (!callVM<Fn, DoToNumber>()) {
    return false;
  }

  masm.bind(&done);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_TONUMERIC() {
  // Keep top stack value in R0.
  frame.popRegsAndSync(1);

  // Inline path for int32 and double; otherwise call VM.
  Label done;
  masm.branchTestNumber(Assembler::Equal, R0, &done);

  prepareVMCall();
  pushArg(R0);

  using Fn = bool (*)(JSContext*, HandleValue, MutableHandleValue);
  if (!callVM<Fn, DoToNumeric>()) {
    return false;
  }

  masm.bind(&done);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_LOOPHEAD() {
  if (!emit_JSOP_JUMPTARGET()) {
    return false;
  }
  return emitInterruptCheck();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitIncExecutionProgressCounter(
    Register scratch) {
  if (!mozilla::recordreplay::IsRecordingOrReplaying()) {
    return true;
  }

  auto incCounter = [this]() {
    masm.inc64(
        AbsoluteAddress(mozilla::recordreplay::ExecutionProgressCounter()));
    return true;
  };
  return emitTestScriptFlag(JSScript::MutableFlags::TrackRecordReplayProgress,
                            true, incCounter, scratch);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_LOOPENTRY() {
  if (!emit_JSOP_JUMPTARGET()) {
    return false;
  }
  frame.syncStack(0);
  if (!emitWarmUpCounterIncrement()) {
    return false;
  }

  return emitIncExecutionProgressCounter(R0.scratchReg());
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_VOID() {
  frame.pop();
  frame.push(UndefinedValue());
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_UNDEFINED() {
  // If this ever changes, change what JSOP_GIMPLICITTHIS does too.
  frame.push(UndefinedValue());
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_HOLE() {
  frame.push(MagicValue(JS_ELEMENTS_HOLE));
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_NULL() {
  frame.push(NullValue());
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CHECKISOBJ() {
  frame.syncStack(0);
  masm.loadValue(frame.addressOfStackValue(-1), R0);

  Label ok;
  masm.branchTestObject(Assembler::Equal, R0, &ok);

  prepareVMCall();

  pushUint8BytecodeOperandArg(R0.scratchReg());

  using Fn = bool (*)(JSContext*, CheckIsObjectKind);
  if (!callVM<Fn, ThrowCheckIsObject>()) {
    return false;
  }

  masm.bind(&ok);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CHECKISCALLABLE() {
  frame.syncStack(0);
  masm.loadValue(frame.addressOfStackValue(-1), R0);

  prepareVMCall();

  pushUint8BytecodeOperandArg(R1.scratchReg());
  pushArg(R0);

  using Fn = bool (*)(JSContext*, HandleValue, CheckIsCallableKind);
  if (!callVM<Fn, CheckIsCallable>()) {
    return false;
  }

  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CHECKTHIS() {
  frame.syncStack(0);
  masm.loadValue(frame.addressOfStackValue(-1), R0);

  return emitCheckThis(R0);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CHECKTHISREINIT() {
  frame.syncStack(0);
  masm.loadValue(frame.addressOfStackValue(-1), R0);

  return emitCheckThis(R0, /* reinit = */ true);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitCheckThis(ValueOperand val, bool reinit) {
  Label thisOK;
  if (reinit) {
    masm.branchTestMagic(Assembler::Equal, val, &thisOK);
  } else {
    masm.branchTestMagic(Assembler::NotEqual, val, &thisOK);
  }

  prepareVMCall();

  if (reinit) {
    using Fn = bool (*)(JSContext*);
    if (!callVM<Fn, BaselineThrowInitializedThis>()) {
      return false;
    }
  } else {
    masm.loadBaselineFramePtr(BaselineFrameReg, val.scratchReg());
    pushArg(val.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*);
    if (!callVM<Fn, BaselineThrowUninitializedThis>()) {
      return false;
    }
  }

  masm.bind(&thisOK);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CHECKRETURN() {
  MOZ_ASSERT_IF(handler.maybeScript(),
                handler.maybeScript()->isDerivedClassConstructor());

  // Load |this| in R0, return value in R1.
  frame.popRegsAndSync(1);
  emitLoadReturnValue(R1);

  Label done, returnOK;
  masm.branchTestObject(Assembler::Equal, R1, &done);
  masm.branchTestUndefined(Assembler::Equal, R1, &returnOK);

  prepareVMCall();
  pushArg(R1);

  using Fn = bool (*)(JSContext*, HandleValue);
  if (!callVM<Fn, ThrowBadDerivedReturn>()) {
    return false;
  }
  masm.assumeUnreachable("Should throw on bad derived constructor return");

  masm.bind(&returnOK);

  if (!emitCheckThis(R0)) {
    return false;
  }

  // Store |this| in the return value slot.
  masm.storeValue(R0, frame.addressOfReturnValue());
  masm.or32(Imm32(BaselineFrame::HAS_RVAL), frame.addressOfFlags());

  masm.bind(&done);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_FUNCTIONTHIS() {
  MOZ_ASSERT_IF(handler.maybeFunction(), !handler.maybeFunction()->isArrow());

  frame.pushThis();

  auto boxThis = [this]() {
    // Load |thisv| in R0. Skip the call if it's already an object.
    Label skipCall;
    frame.popRegsAndSync(1);
    masm.branchTestObject(Assembler::Equal, R0, &skipCall);

    prepareVMCall();
    masm.loadBaselineFramePtr(BaselineFrameReg, R1.scratchReg());

    pushArg(R1.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*, MutableHandleValue);
    if (!callVM<Fn, BaselineGetFunctionThis>()) {
      return false;
    }

    masm.bind(&skipCall);
    frame.push(R0);
    return true;
  };

  // In strict mode code, |this| is left alone.
  return emitTestScriptFlag(JSScript::ImmutableFlags::Strict, false, boxThis,
                            R2.scratchReg());
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GLOBALTHIS() {
  frame.syncStack(0);

  auto getNonSyntacticThis = [this]() {
    prepareVMCall();

    masm.loadPtr(frame.addressOfEnvironmentChain(), R0.scratchReg());
    pushArg(R0.scratchReg());

    using Fn = void (*)(JSContext*, HandleObject, MutableHandleValue);
    if (!callVM<Fn, GetNonSyntacticGlobalThis>()) {
      return false;
    }

    frame.push(R0);
    return true;
  };
  auto getGlobalThis = [this]() {
    loadGlobalThisValue(R0);
    frame.push(R0);
    return true;
  };
  return emitTestScriptFlag(JSScript::ImmutableFlags::HasNonSyntacticScope,
                            getNonSyntacticThis, getGlobalThis,
                            R2.scratchReg());
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_TRUE() {
  frame.push(BooleanValue(true));
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_FALSE() {
  frame.push(BooleanValue(false));
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_ZERO() {
  frame.push(Int32Value(0));
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_ONE() {
  frame.push(Int32Value(1));
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_INT8() {
  frame.push(Int32Value(GET_INT8(handler.pc())));
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_INT8() {
  LoadInt8Operand(masm, PCRegAtStart, R0.scratchReg());
  masm.tagValue(JSVAL_TYPE_INT32, R0.scratchReg(), R0);
  frame.push(R0);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_INT32() {
  frame.push(Int32Value(GET_INT32(handler.pc())));
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_INT32() {
  LoadInt32Operand(masm, PCRegAtStart, R0.scratchReg());
  masm.tagValue(JSVAL_TYPE_INT32, R0.scratchReg(), R0);
  frame.push(R0);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_UINT16() {
  frame.push(Int32Value(GET_UINT16(handler.pc())));
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_UINT16() {
  LoadUint16Operand(masm, PCRegAtStart, R0.scratchReg());
  masm.tagValue(JSVAL_TYPE_INT32, R0.scratchReg(), R0);
  frame.push(R0);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_UINT24() {
  frame.push(Int32Value(GET_UINT24(handler.pc())));
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_UINT24() {
  LoadUint24Operand(masm, PCRegAtStart, 0, R0.scratchReg());
  masm.tagValue(JSVAL_TYPE_INT32, R0.scratchReg(), R0);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_RESUMEINDEX() {
  return emit_JSOP_UINT24();
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_DOUBLE() {
  frame.push(GET_INLINE_VALUE(handler.pc()));
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_DOUBLE() {
  LoadInlineValueOperand(masm, PCRegAtStart, R0);
  frame.push(R0);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_BIGINT() {
  frame.push(handler.script()->getConst(GET_UINT32_INDEX(handler.pc())));
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_BIGINT() {
  MOZ_CRASH("NYI: interpreter JSOP_BIGINT");
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_STRING() {
  frame.push(StringValue(handler.script()->getAtom(handler.pc())));
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_STRING() {
  Register scratch1 = R0.scratchReg();
  Register scratch2 = R1.scratchReg();
  LoadInt32Operand(masm, PCRegAtStart, scratch1);

  loadScriptAtom(scratch1, scratch2);

  masm.tagValue(JSVAL_TYPE_STRING, scratch2, R0);
  frame.push(R0);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_SYMBOL() {
  unsigned which = GET_UINT8(handler.pc());
  JS::Symbol* sym = cx->runtime()->wellKnownSymbols->get(which);
  frame.push(SymbolValue(sym));
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_SYMBOL() {
  Register scratch1 = R0.scratchReg();
  Register scratch2 = R1.scratchReg();
  LoadUint8Operand(masm, PCRegAtStart, scratch1);

  masm.movePtr(ImmPtr(cx->runtime()->wellKnownSymbols), scratch2);
  masm.loadPtr(BaseIndex(scratch2, scratch1, ScalePointer), scratch1);

  masm.tagValue(JSVAL_TYPE_SYMBOL, scratch1, R0);
  frame.push(R0);
  return true;
}

JSObject* BaselineCompilerHandler::maybeNoCloneSingletonObject() {
  Realm* realm = script()->realm();
  if (realm->creationOptions().cloneSingletons()) {
    return nullptr;
  }

  realm->behaviors().setSingletonsAsValues();
  return script()->getObject(pc());
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_OBJECT() {
  // If we know we don't have to clone the object literal, just push it
  // directly. Note that the interpreter always does the VM call; that's fine
  // because this op is only used in run-once code.
  if (JSObject* obj = handler.maybeNoCloneSingletonObject()) {
    frame.push(ObjectValue(*obj));
    return true;
  }

  prepareVMCall();

  pushBytecodePCArg();
  pushScriptArg();

  using Fn = JSObject* (*)(JSContext*, HandleScript, jsbytecode*);
  if (!callVM<Fn, SingletonObjectLiteralOperation>()) {
    return false;
  }

  // Box and push return value.
  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_CALLSITEOBJ() {
  RootedScript script(cx, handler.script());
  JSObject* cso = ProcessCallSiteObjOperation(cx, script, handler.pc());
  if (!cso) {
    return false;
  }

  frame.push(ObjectValue(*cso));
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_CALLSITEOBJ() {
  prepareVMCall();

  pushBytecodePCArg();
  pushScriptArg();

  using Fn = ArrayObject* (*)(JSContext*, HandleScript, jsbytecode*);
  if (!callVM<Fn, ProcessCallSiteObjOperation>()) {
    return false;
  }

  // Box and push return value.
  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_REGEXP() {
  prepareVMCall();
  pushScriptObjectArg(ScriptObjectType::RegExp);

  using Fn = JSObject* (*)(JSContext*, Handle<RegExpObject*>);
  if (!callVM<Fn, CloneRegExpObject>()) {
    return false;
  }

  // Box and push return value.
  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_LAMBDA() {
  prepareVMCall();
  masm.loadPtr(frame.addressOfEnvironmentChain(), R0.scratchReg());

  pushArg(R0.scratchReg());
  pushScriptObjectArg(ScriptObjectType::Function);

  using Fn = JSObject* (*)(JSContext*, HandleFunction, HandleObject);
  if (!callVM<Fn, js::Lambda>()) {
    return false;
  }

  // Box and push return value.
  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_LAMBDA_ARROW() {
  // Keep pushed newTarget in R0.
  frame.popRegsAndSync(1);

  prepareVMCall();
  masm.loadPtr(frame.addressOfEnvironmentChain(), R2.scratchReg());

  pushArg(R0);
  pushArg(R2.scratchReg());
  pushScriptObjectArg(ScriptObjectType::Function);

  using Fn =
      JSObject* (*)(JSContext*, HandleFunction, HandleObject, HandleValue);
  if (!callVM<Fn, js::LambdaArrow>()) {
    return false;
  }

  // Box and push return value.
  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SETFUNNAME() {
  frame.popRegsAndSync(2);

  frame.push(R0);
  frame.syncStack(0);

  masm.unboxObject(R0, R0.scratchReg());

  prepareVMCall();

  pushUint8BytecodeOperandArg(R2.scratchReg());
  pushArg(R1);
  pushArg(R0.scratchReg());

  using Fn =
      bool (*)(JSContext*, HandleFunction, HandleValue, FunctionPrefixKind);
  return callVM<Fn, SetFunctionName>();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_BITOR() {
  return emitBinaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_BITXOR() {
  return emitBinaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_BITAND() {
  return emitBinaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_LSH() {
  return emitBinaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_RSH() {
  return emitBinaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_URSH() {
  return emitBinaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_ADD() {
  return emitBinaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SUB() {
  return emitBinaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_MUL() {
  return emitBinaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DIV() {
  return emitBinaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_MOD() {
  return emitBinaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_POW() {
  return emitBinaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitBinaryArith() {
  // Keep top JSStack value in R0 and R2
  frame.popRegsAndSync(2);

  // Call IC
  if (!emitNextIC()) {
    return false;
  }

  // Mark R0 as pushed stack value.
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitUnaryArith() {
  // Keep top stack value in R0.
  frame.popRegsAndSync(1);

  // Call IC
  if (!emitNextIC()) {
    return false;
  }

  // Mark R0 as pushed stack value.
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_BITNOT() {
  return emitUnaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_NEG() {
  return emitUnaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INC() {
  return emitUnaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DEC() {
  return emitUnaryArith();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_LT() {
  return emitCompare();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_LE() {
  return emitCompare();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GT() {
  return emitCompare();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GE() {
  return emitCompare();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_EQ() {
  return emitCompare();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_NE() {
  return emitCompare();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitCompare() {
  // CODEGEN

  // Keep top JSStack value in R0 and R1.
  frame.popRegsAndSync(2);

  // Call IC.
  if (!emitNextIC()) {
    return false;
  }

  // Mark R0 as pushed stack value.
  frame.push(R0, JSVAL_TYPE_BOOLEAN);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_STRICTEQ() {
  return emitCompare();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_STRICTNE() {
  return emitCompare();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CONDSWITCH() {
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CASE() {
  frame.popRegsAndSync(1);

  Label done;
  masm.branchTestBooleanTruthy(/* branchIfTrue */ false, R0, &done);
  {
    // Pop the switch value if the case matches.
    masm.addToStackPtr(Imm32(sizeof(Value)));
    emitJump();
  }
  masm.bind(&done);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DEFAULT() {
  frame.pop();
  return emit_JSOP_GOTO();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_LINENO() {
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_NEWARRAY() {
  frame.syncStack(0);

  // Pass length in R0.
  loadInt32LengthBytecodeOperand(R0.scratchReg());

  if (!emitNextIC()) {
    return false;
  }

  frame.push(R0);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_NEWARRAY_COPYONWRITE() {
  // This is like the interpreter implementation, but we can call
  // getOrFixupCopyOnWriteObject at compile-time.

  RootedScript scriptRoot(cx, handler.script());
  JSObject* obj =
      ObjectGroup::getOrFixupCopyOnWriteObject(cx, scriptRoot, handler.pc());
  if (!obj) {
    return false;
  }

  prepareVMCall();

  pushArg(ImmGCPtr(obj));

  using Fn = ArrayObject* (*)(JSContext*, HandleArrayObject);
  if (!callVM<Fn, js::NewDenseCopyOnWriteArray>()) {
    return false;
  }

  // Box and push return value.
  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_NEWARRAY_COPYONWRITE() {
  prepareVMCall();

  pushBytecodePCArg();
  pushScriptArg();

  using Fn = ArrayObject* (*)(JSContext*, HandleScript, jsbytecode*);
  if (!callVM<Fn, NewArrayCopyOnWriteOperation>()) {
    return false;
  }

  // Box and push return value.
  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITELEM_ARRAY() {
  // Keep the object and rhs on the stack.
  frame.syncStack(0);

  // Load object in R0, index in R1.
  masm.loadValue(frame.addressOfStackValue(-2), R0);
  loadInt32IndexBytecodeOperand(R1);

  // Call IC.
  if (!emitNextIC()) {
    return false;
  }

  // Pop the rhs, so that the object is on the top of the stack.
  frame.pop();
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_NEWOBJECT() {
  frame.syncStack(0);

  if (!emitNextIC()) {
    return false;
  }

  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_NEWINIT() {
  frame.syncStack(0);

  if (!emitNextIC()) {
    return false;
  }

  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITELEM() {
  // Store RHS in the scratch slot.
  frame.storeStackValue(-1, frame.addressOfScratchValue(), R2);
  frame.pop();

  // Keep object and index in R0 and R1.
  frame.popRegsAndSync(2);

  // Push the object to store the result of the IC.
  frame.push(R0);
  frame.syncStack(0);

  // Keep RHS on the stack.
  frame.pushScratchValue();

  // Call IC.
  if (!emitNextIC()) {
    return false;
  }

  // Pop the rhs, so that the object is on the top of the stack.
  frame.pop();
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITHIDDENELEM() {
  return emit_JSOP_INITELEM();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_MUTATEPROTO() {
  // Keep values on the stack for the decompiler.
  frame.syncStack(0);

  masm.unboxObject(frame.addressOfStackValue(-2), R0.scratchReg());
  masm.loadValue(frame.addressOfStackValue(-1), R1);

  prepareVMCall();

  pushArg(R1);
  pushArg(R0.scratchReg());

  using Fn = bool (*)(JSContext*, HandlePlainObject, HandleValue);
  if (!callVM<Fn, MutatePrototype>()) {
    return false;
  }

  frame.pop();
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITPROP() {
  // Load lhs in R0, rhs in R1.
  frame.syncStack(0);
  masm.loadValue(frame.addressOfStackValue(-2), R0);
  masm.loadValue(frame.addressOfStackValue(-1), R1);

  // Call IC.
  if (!emitNextIC()) {
    return false;
  }

  // Leave the object on the stack.
  frame.pop();
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITLOCKEDPROP() {
  return emit_JSOP_INITPROP();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITHIDDENPROP() {
  return emit_JSOP_INITPROP();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GETELEM() {
  // Keep top two stack values in R0 and R1.
  frame.popRegsAndSync(2);

  // Call IC.
  if (!emitNextIC()) {
    return false;
  }

  // Mark R0 as pushed stack value.
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GETELEM_SUPER() {
  // Store obj in the scratch slot.
  frame.storeStackValue(-1, frame.addressOfScratchValue(), R2);
  frame.pop();

  // Keep receiver and index in R0 and R1.
  frame.popRegsAndSync(2);

  // Keep obj on the stack.
  frame.pushScratchValue();

  if (!emitNextIC()) {
    return false;
  }

  frame.pop();  // This value is also popped in InitFromBailout.
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CALLELEM() {
  return emit_JSOP_GETELEM();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SETELEM() {
  // Store RHS in the scratch slot.
  frame.storeStackValue(-1, frame.addressOfScratchValue(), R2);
  frame.pop();

  // Keep object and index in R0 and R1.
  frame.popRegsAndSync(2);

  // Keep RHS on the stack.
  frame.pushScratchValue();

  // Call IC.
  if (!emitNextIC()) {
    return false;
  }

  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_STRICTSETELEM() {
  return emit_JSOP_SETELEM();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitSetElemSuper(bool strict) {
  // Incoming stack is |receiver, propval, obj, rval|. We need to shuffle
  // stack to leave rval when operation is complete.

  // Pop rval into R0, then load receiver into R1 and replace with rval.
  frame.popRegsAndSync(1);
  masm.loadValue(frame.addressOfStackValue(-3), R1);
  masm.storeValue(R0, frame.addressOfStackValue(-3));

  prepareVMCall();

  pushArg(Imm32(strict));
  pushArg(R1);  // receiver
  pushArg(R0);  // rval
  masm.loadValue(frame.addressOfStackValue(-2), R0);
  pushArg(R0);  // propval
  masm.unboxObject(frame.addressOfStackValue(-1), R0.scratchReg());
  pushArg(R0.scratchReg());  // obj

  using Fn = bool (*)(JSContext*, HandleObject, HandleValue, HandleValue,
                      HandleValue, bool);
  if (!callVM<Fn, js::SetObjectElementWithReceiver>()) {
    return false;
  }

  frame.popn(2);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SETELEM_SUPER() {
  return emitSetElemSuper(/* strict = */ false);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_STRICTSETELEM_SUPER() {
  return emitSetElemSuper(/* strict = */ true);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitDelElem(bool strict) {
  // Keep values on the stack for the decompiler.
  frame.syncStack(0);
  masm.loadValue(frame.addressOfStackValue(-2), R0);
  masm.loadValue(frame.addressOfStackValue(-1), R1);

  prepareVMCall();

  pushArg(R1);
  pushArg(R0);

  using Fn = bool (*)(JSContext*, HandleValue, HandleValue, bool*);
  if (strict) {
    if (!callVM<Fn, DeleteElementJit<true>>()) {
      return false;
    }
  } else {
    if (!callVM<Fn, DeleteElementJit<false>>()) {
      return false;
    }
  }

  masm.boxNonDouble(JSVAL_TYPE_BOOLEAN, ReturnReg, R1);
  frame.popn(2);
  frame.push(R1);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DELELEM() {
  return emitDelElem(/* strict = */ false);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_STRICTDELELEM() {
  return emitDelElem(/* strict = */ true);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_IN() {
  frame.popRegsAndSync(2);

  if (!emitNextIC()) {
    return false;
  }

  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_HASOWN() {
  frame.popRegsAndSync(2);

  if (!emitNextIC()) {
    return false;
  }

  frame.push(R0);
  return true;
}

template <>
bool BaselineCompilerCodeGen::tryOptimizeGetGlobalName() {
  PropertyName* name = handler.script()->getName(handler.pc());

  // These names are non-configurable on the global and cannot be shadowed.
  if (name == cx->names().undefined) {
    frame.push(UndefinedValue());
    return true;
  }
  if (name == cx->names().NaN) {
    frame.push(cx->runtime()->NaNValue);
    return true;
  }
  if (name == cx->names().Infinity) {
    frame.push(cx->runtime()->positiveInfinityValue);
    return true;
  }

  return false;
}

template <>
bool BaselineInterpreterCodeGen::tryOptimizeGetGlobalName() {
  // Interpreter doesn't optimize simple GETGNAMEs.
  return false;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GETGNAME() {
  auto getName = [this]() { return emit_JSOP_GETNAME(); };

  auto getGlobalName = [this]() {
    if (tryOptimizeGetGlobalName()) {
      return true;
    }

    frame.syncStack(0);

    loadGlobalLexicalEnvironment(R0.scratchReg());

    // Call IC.
    if (!emitNextIC()) {
      return false;
    }

    // Mark R0 as pushed stack value.
    frame.push(R0);
    return true;
  };
  return emitTestScriptFlag(JSScript::ImmutableFlags::HasNonSyntacticScope,
                            getName, getGlobalName, R2.scratchReg());
}

template <>
bool BaselineCompilerCodeGen::tryOptimizeBindGlobalName() {
  JSScript* script = handler.script();
  if (script->hasNonSyntacticScope()) {
    return false;
  }

  // We can bind name to the global lexical scope if the binding already
  // exists, is initialized, and is writable (i.e., an initialized
  // 'let') at compile time.
  RootedPropertyName name(cx, script->getName(handler.pc()));
  Rooted<LexicalEnvironmentObject*> env(cx,
                                        &script->global().lexicalEnvironment());
  if (Shape* shape = env->lookup(cx, name)) {
    if (shape->writable() &&
        !env->getSlot(shape->slot()).isMagic(JS_UNINITIALIZED_LEXICAL)) {
      frame.push(ObjectValue(*env));
      return true;
    }
    return false;
  }

  if (Shape* shape = script->global().lookup(cx, name)) {
    // If the property does not currently exist on the global lexical
    // scope, we can bind name to the global object if the property
    // exists on the global and is non-configurable, as then it cannot
    // be shadowed.
    if (!shape->configurable()) {
      frame.push(ObjectValue(script->global()));
      return true;
    }
  }

  return false;
}

template <>
bool BaselineInterpreterCodeGen::tryOptimizeBindGlobalName() {
  // Interpreter doesn't optimize simple BINDGNAMEs.
  return false;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_BINDGNAME() {
  if (tryOptimizeBindGlobalName()) {
    return true;
  }
  return emitBindName(JSOP_BINDGNAME);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_BINDVAR() {
  frame.syncStack(0);
  masm.loadPtr(frame.addressOfEnvironmentChain(), R0.scratchReg());

  prepareVMCall();
  pushArg(R0.scratchReg());

  using Fn = JSObject* (*)(JSContext*, JSObject*);
  if (!callVM<Fn, BindVarOperation>()) {
    return false;
  }

  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SETPROP() {
  // Keep lhs in R0, rhs in R1.
  frame.popRegsAndSync(2);

  // Keep RHS on the stack.
  frame.push(R1);
  frame.syncStack(0);

  // Call IC.
  if (!emitNextIC()) {
    return false;
  }

  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_STRICTSETPROP() {
  return emit_JSOP_SETPROP();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SETNAME() {
  return emit_JSOP_SETPROP();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_STRICTSETNAME() {
  return emit_JSOP_SETPROP();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SETGNAME() {
  return emit_JSOP_SETPROP();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_STRICTSETGNAME() {
  return emit_JSOP_SETPROP();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitSetPropSuper(bool strict) {
  // Incoming stack is |receiver, obj, rval|. We need to shuffle stack to
  // leave rval when operation is complete.

  // Pop rval into R0, then load receiver into R1 and replace with rval.
  frame.popRegsAndSync(1);
  masm.loadValue(frame.addressOfStackValue(-2), R1);
  masm.storeValue(R0, frame.addressOfStackValue(-2));

  prepareVMCall();

  pushArg(Imm32(strict));
  pushArg(R0);  // rval
  pushScriptNameArg(R0.scratchReg(), R2.scratchReg());
  pushArg(R1);  // receiver
  masm.unboxObject(frame.addressOfStackValue(-1), R0.scratchReg());
  pushArg(R0.scratchReg());  // obj

  using Fn = bool (*)(JSContext*, HandleObject, HandleValue, HandlePropertyName,
                      HandleValue, bool);
  if (!callVM<Fn, js::SetPropertySuper>()) {
    return false;
  }

  frame.pop();
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SETPROP_SUPER() {
  return emitSetPropSuper(/* strict = */ false);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_STRICTSETPROP_SUPER() {
  return emitSetPropSuper(/* strict = */ true);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GETPROP() {
  // Keep object in R0.
  frame.popRegsAndSync(1);

  // Call IC.
  if (!emitNextIC()) {
    return false;
  }

  // Mark R0 as pushed stack value.
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CALLPROP() {
  return emit_JSOP_GETPROP();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_LENGTH() {
  return emit_JSOP_GETPROP();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GETBOUNDNAME() {
  return emit_JSOP_GETPROP();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GETPROP_SUPER() {
  // Receiver -> R1, Object -> R0
  frame.popRegsAndSync(1);
  masm.loadValue(frame.addressOfStackValue(-1), R1);
  frame.pop();

  if (!emitNextIC()) {
    return false;
  }

  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitDelProp(bool strict) {
  // Keep value on the stack for the decompiler.
  frame.syncStack(0);
  masm.loadValue(frame.addressOfStackValue(-1), R0);

  prepareVMCall();

  pushScriptNameArg(R1.scratchReg(), R2.scratchReg());
  pushArg(R0);

  using Fn = bool (*)(JSContext*, HandleValue, HandlePropertyName, bool*);
  if (strict) {
    if (!callVM<Fn, DeletePropertyJit<true>>()) {
      return false;
    }
  } else {
    if (!callVM<Fn, DeletePropertyJit<false>>()) {
      return false;
    }
  }

  masm.boxNonDouble(JSVAL_TYPE_BOOLEAN, ReturnReg, R1);
  frame.pop();
  frame.push(R1);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DELPROP() {
  return emitDelProp(/* strict = */ false);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_STRICTDELPROP() {
  return emitDelProp(/* strict = */ true);
}

template <>
void BaselineCompilerCodeGen::getEnvironmentCoordinateObject(Register reg) {
  EnvironmentCoordinate ec(handler.pc());

  masm.loadPtr(frame.addressOfEnvironmentChain(), reg);
  for (unsigned i = ec.hops(); i; i--) {
    masm.unboxObject(
        Address(reg, EnvironmentObject::offsetOfEnclosingEnvironment()), reg);
  }
}

template <>
void BaselineInterpreterCodeGen::getEnvironmentCoordinateObject(Register reg) {
  MOZ_CRASH("Shouldn't call this for interpreter");
}

template <>
Address BaselineCompilerCodeGen::getEnvironmentCoordinateAddressFromObject(
    Register objReg, Register reg) {
  EnvironmentCoordinate ec(handler.pc());

  if (EnvironmentObject::nonExtensibleIsFixedSlot(ec)) {
    return Address(objReg, NativeObject::getFixedSlotOffset(ec.slot()));
  }

  uint32_t slot = EnvironmentObject::nonExtensibleDynamicSlotIndex(ec);
  masm.loadPtr(Address(objReg, NativeObject::offsetOfSlots()), reg);
  return Address(reg, slot * sizeof(Value));
}

template <>
Address BaselineInterpreterCodeGen::getEnvironmentCoordinateAddressFromObject(
    Register objReg, Register reg) {
  MOZ_CRASH("Shouldn't call this for interpreter");
}

template <typename Handler>
Address BaselineCodeGen<Handler>::getEnvironmentCoordinateAddress(
    Register reg) {
  getEnvironmentCoordinateObject(reg);
  return getEnvironmentCoordinateAddressFromObject(reg, reg);
}

// For a JOF_ENVCOORD op load the number of hops from the bytecode and skip this
// number of environment objects.
static void LoadAliasedVarEnv(MacroAssembler& masm, Register pc, Register env,
                              Register scratch) {
  static_assert(ENVCOORD_HOPS_LEN == 1,
                "Code assumes number of hops is stored in uint8 operand");
  LoadUint8Operand(masm, pc, scratch);

  Label top, done;
  masm.bind(&top);
  masm.branchTest32(Assembler::Zero, scratch, scratch, &done);
  {
    Address nextEnv(env, EnvironmentObject::offsetOfEnclosingEnvironment());
    masm.unboxObject(nextEnv, env);
    masm.sub32(Imm32(1), scratch);
    masm.jump(&top);
  }
  masm.bind(&done);
}

template <>
void BaselineCompilerCodeGen::emitGetAliasedVar(ValueOperand dest) {
  frame.syncStack(0);

  Address address = getEnvironmentCoordinateAddress(R0.scratchReg());
  masm.loadValue(address, dest);
}

template <>
void BaselineInterpreterCodeGen::emitGetAliasedVar(ValueOperand dest) {
  Register env = R0.scratchReg();
  Register scratch = R1.scratchReg();

  // Load the right environment object.
  masm.loadPtr(frame.addressOfEnvironmentChain(), env);
  LoadAliasedVarEnv(masm, PCRegAtStart, env, scratch);

  // Load the slot index.
  static_assert(ENVCOORD_SLOT_LEN == 3,
                "Code assumes slot is stored in uint24 operand");
  LoadUint24Operand(masm, PCRegAtStart, ENVCOORD_HOPS_LEN, scratch);

  // Load the Value from a fixed or dynamic slot.
  // See EnvironmentObject::nonExtensibleIsFixedSlot.
  Label isDynamic, done;
  masm.branch32(Assembler::AboveOrEqual, scratch,
                Imm32(NativeObject::MAX_FIXED_SLOTS), &isDynamic);
  {
    uint32_t offset = NativeObject::getFixedSlotOffset(0);
    masm.loadValue(BaseValueIndex(env, scratch, offset), dest);
    masm.jump(&done);
  }
  masm.bind(&isDynamic);
  {
    masm.loadPtr(Address(env, NativeObject::offsetOfSlots()), env);

    // Use an offset to subtract the number of fixed slots.
    int32_t offset = -int32_t(NativeObject::MAX_FIXED_SLOTS * sizeof(Value));
    masm.loadValue(BaseValueIndex(env, scratch, offset), dest);
  }
  masm.bind(&done);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GETALIASEDVAR() {
  emitGetAliasedVar(R0);

  if (handler.maybeIonCompileable()) {
    // No need to monitor types if we know Ion can't compile this script.
    if (!emitNextIC()) {
      return false;
    }
  }

  frame.push(R0);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_SETALIASEDVAR() {
  // Keep rvalue in R0.
  frame.popRegsAndSync(1);
  Register objReg = R2.scratchReg();

  getEnvironmentCoordinateObject(objReg);
  Address address =
      getEnvironmentCoordinateAddressFromObject(objReg, R1.scratchReg());
  masm.guardedCallPreBarrier(address, MIRType::Value);
  masm.storeValue(R0, address);
  frame.push(R0);

  // Only R0 is live at this point.
  // Scope coordinate object is already in R2.scratchReg().
  Register temp = R1.scratchReg();

  Label skipBarrier;
  masm.branchPtrInNurseryChunk(Assembler::Equal, objReg, temp, &skipBarrier);
  masm.branchValueIsNurseryCell(Assembler::NotEqual, R0, temp, &skipBarrier);

  masm.call(&postBarrierSlot_);  // Won't clobber R0

  masm.bind(&skipBarrier);
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_SETALIASEDVAR() {
  AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
  regs.take(BaselineFrameReg);
  regs.take(R2);
  MOZ_ASSERT(!regs.has(PCRegAtStart), "R2 contains PCRegAtStart");

  Register env = regs.takeAny();
  Register scratch1 = regs.takeAny();
  Register scratch2 = regs.takeAny();
  Register scratch3 = regs.takeAny();

  // Load the right environment object.
  masm.loadPtr(frame.addressOfEnvironmentChain(), env);
  LoadAliasedVarEnv(masm, PCRegAtStart, env, scratch1);

  // Load the slot index.
  static_assert(ENVCOORD_SLOT_LEN == 3,
                "Code assumes slot is stored in uint24 operand");
  LoadUint24Operand(masm, PCRegAtStart, ENVCOORD_HOPS_LEN, scratch1);

  // Store the RHS Value in R2.
  masm.loadValue(frame.addressOfStackValue(-1), R2);

  // Load a pointer to the fixed or dynamic slot into scratch2. We want to call
  // guardedCallPreBarrierAnyZone once to avoid code bloat.

  // See EnvironmentObject::nonExtensibleIsFixedSlot.
  Label isDynamic, done;
  masm.branch32(Assembler::AboveOrEqual, scratch1,
                Imm32(NativeObject::MAX_FIXED_SLOTS), &isDynamic);
  {
    uint32_t offset = NativeObject::getFixedSlotOffset(0);
    BaseValueIndex slotAddr(env, scratch1, offset);
    masm.computeEffectiveAddress(slotAddr, scratch2);
    masm.jump(&done);
  }
  masm.bind(&isDynamic);
  {
    masm.loadPtr(Address(env, NativeObject::offsetOfSlots()), scratch2);

    // Use an offset to subtract the number of fixed slots.
    int32_t offset = -int32_t(NativeObject::MAX_FIXED_SLOTS * sizeof(Value));
    BaseValueIndex slotAddr(scratch2, scratch1, offset);
    masm.computeEffectiveAddress(slotAddr, scratch2);
  }
  masm.bind(&done);

  // Pre-barrier and store.
  Address slotAddr(scratch2, 0);
  masm.guardedCallPreBarrierAnyZone(slotAddr, MIRType::Value, scratch3);
  masm.storeValue(R2, slotAddr);

  // Post barrier.
  Label skipBarrier;
  masm.branchPtrInNurseryChunk(Assembler::Equal, env, scratch1, &skipBarrier);
  masm.branchValueIsNurseryCell(Assembler::NotEqual, R2, scratch1,
                                &skipBarrier);
  {
    // Post barrier code expects the object in R2.
    masm.movePtr(env, R2.scratchReg());
    masm.call(&postBarrierSlot_);
  }
  masm.bind(&skipBarrier);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GETNAME() {
  frame.syncStack(0);

  masm.loadPtr(frame.addressOfEnvironmentChain(), R0.scratchReg());

  // Call IC.
  if (!emitNextIC()) {
    return false;
  }

  // Mark R0 as pushed stack value.
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitBindName(JSOp op) {
  // If we have a BINDGNAME without a non-syntactic scope, we pass the global
  // lexical environment to the IC instead of the frame's environment.

  frame.syncStack(0);

  auto loadGlobalLexical = [this]() {
    loadGlobalLexicalEnvironment(R0.scratchReg());
    return true;
  };
  auto loadFrameEnv = [this]() {
    masm.loadPtr(frame.addressOfEnvironmentChain(), R0.scratchReg());
    return true;
  };

  if (op == JSOP_BINDNAME) {
    if (!loadFrameEnv()) {
      return false;
    }
  } else {
    MOZ_ASSERT(op == JSOP_BINDGNAME);
    if (!emitTestScriptFlag(JSScript::ImmutableFlags::HasNonSyntacticScope,
                            loadFrameEnv, loadGlobalLexical, R2.scratchReg())) {
      return false;
    }
  }

  // Call IC.
  if (!emitNextIC()) {
    return false;
  }

  // Mark R0 as pushed stack value.
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_BINDNAME() {
  return emitBindName(JSOP_BINDNAME);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DELNAME() {
  frame.syncStack(0);
  masm.loadPtr(frame.addressOfEnvironmentChain(), R0.scratchReg());

  prepareVMCall();

  pushArg(R0.scratchReg());
  pushScriptNameArg(R1.scratchReg(), R2.scratchReg());

  using Fn = bool (*)(JSContext*, HandlePropertyName, HandleObject,
                      MutableHandleValue);
  if (!callVM<Fn, js::DeleteNameOperation>()) {
    return false;
  }

  frame.push(R0);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_GETIMPORT() {
  JSScript* script = handler.script();
  ModuleEnvironmentObject* env = GetModuleEnvironmentForScript(script);
  MOZ_ASSERT(env);

  jsid id = NameToId(script->getName(handler.pc()));
  ModuleEnvironmentObject* targetEnv;
  Shape* shape;
  MOZ_ALWAYS_TRUE(env->lookupImport(id, &targetEnv, &shape));

  EnsureTrackPropertyTypes(cx, targetEnv, shape->propid());

  frame.syncStack(0);

  uint32_t slot = shape->slot();
  Register scratch = R0.scratchReg();
  masm.movePtr(ImmGCPtr(targetEnv), scratch);
  if (slot < targetEnv->numFixedSlots()) {
    masm.loadValue(Address(scratch, NativeObject::getFixedSlotOffset(slot)),
                   R0);
  } else {
    masm.loadPtr(Address(scratch, NativeObject::offsetOfSlots()), scratch);
    masm.loadValue(
        Address(scratch, (slot - targetEnv->numFixedSlots()) * sizeof(Value)),
        R0);
  }

  // Imports are initialized by this point except in rare circumstances, so
  // don't emit a check unless we have to.
  if (targetEnv->getSlot(shape->slot()).isMagic(JS_UNINITIALIZED_LEXICAL)) {
    if (!emitUninitializedLexicalCheck(R0)) {
      return false;
    }
  }

  if (handler.maybeIonCompileable()) {
    // No need to monitor types if we know Ion can't compile this script.
    if (!emitNextIC()) {
      return false;
    }
  }

  frame.push(R0);
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_GETIMPORT() {
  frame.syncStack(0);

  masm.loadPtr(frame.addressOfEnvironmentChain(), R0.scratchReg());

  prepareVMCall();

  pushBytecodePCArg();
  pushScriptArg();
  pushArg(R0.scratchReg());

  using Fn = bool (*)(JSContext*, HandleObject, HandleScript, jsbytecode*,
                      MutableHandleValue);
  if (!callVM<Fn, GetImportOperation>()) {
    return false;
  }

  // Enter the type monitor IC.
  if (!emitNextIC()) {
    return false;
  }

  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GETINTRINSIC() {
  frame.syncStack(0);

  if (!emitNextIC()) {
    return false;
  }

  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SETINTRINSIC() {
  frame.syncStack(0);
  masm.loadValue(frame.addressOfStackValue(-1), R0);

  prepareVMCall();

  pushArg(R0);
  pushBytecodePCArg();
  pushScriptArg();

  using Fn = bool (*)(JSContext*, JSScript*, jsbytecode*, HandleValue);
  return callVM<Fn, SetIntrinsicOperation>();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DEFVAR() {
  frame.syncStack(0);

  masm.loadPtr(frame.addressOfEnvironmentChain(), R0.scratchReg());

  prepareVMCall();

  pushBytecodePCArg();
  pushScriptArg();
  pushArg(R0.scratchReg());

  using Fn = bool (*)(JSContext*, HandleObject, HandleScript, jsbytecode*);
  return callVM<Fn, DefVarOperation>();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitDefLexical(JSOp op) {
  MOZ_ASSERT(op == JSOP_DEFCONST || op == JSOP_DEFLET);

  frame.syncStack(0);

  masm.loadPtr(frame.addressOfEnvironmentChain(), R0.scratchReg());

  prepareVMCall();

  pushBytecodePCArg();
  pushScriptArg();
  pushArg(R0.scratchReg());

  using Fn = bool (*)(JSContext*, HandleObject, HandleScript, jsbytecode*);
  return callVM<Fn, DefLexicalOperation>();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DEFCONST() {
  return emitDefLexical(JSOP_DEFCONST);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DEFLET() {
  return emitDefLexical(JSOP_DEFLET);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DEFFUN() {
  frame.popRegsAndSync(1);
  masm.unboxObject(R0, R0.scratchReg());
  masm.loadPtr(frame.addressOfEnvironmentChain(), R1.scratchReg());

  prepareVMCall();

  pushArg(R0.scratchReg());
  pushArg(R1.scratchReg());
  pushScriptArg();

  using Fn = bool (*)(JSContext*, HandleScript, HandleObject, HandleFunction);
  return callVM<Fn, DefFunOperation>();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitInitPropGetterSetter() {
  // Keep values on the stack for the decompiler.
  frame.syncStack(0);

  prepareVMCall();

  masm.unboxObject(frame.addressOfStackValue(-1), R0.scratchReg());
  masm.unboxObject(frame.addressOfStackValue(-2), R1.scratchReg());

  pushArg(R0.scratchReg());
  pushScriptNameArg(R0.scratchReg(), R2.scratchReg());
  pushArg(R1.scratchReg());
  pushBytecodePCArg();

  using Fn = bool (*)(JSContext*, jsbytecode*, HandleObject, HandlePropertyName,
                      HandleObject);
  if (!callVM<Fn, InitPropGetterSetterOperation>()) {
    return false;
  }

  frame.pop();
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITPROP_GETTER() {
  return emitInitPropGetterSetter();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITHIDDENPROP_GETTER() {
  return emitInitPropGetterSetter();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITPROP_SETTER() {
  return emitInitPropGetterSetter();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITHIDDENPROP_SETTER() {
  return emitInitPropGetterSetter();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitInitElemGetterSetter() {
  // Load index and value in R0 and R1, but keep values on the stack for the
  // decompiler.
  frame.syncStack(0);
  masm.loadValue(frame.addressOfStackValue(-2), R0);
  masm.unboxObject(frame.addressOfStackValue(-1), R1.scratchReg());

  prepareVMCall();

  pushArg(R1.scratchReg());
  pushArg(R0);
  masm.unboxObject(frame.addressOfStackValue(-3), R0.scratchReg());
  pushArg(R0.scratchReg());
  pushBytecodePCArg();

  using Fn = bool (*)(JSContext*, jsbytecode*, HandleObject, HandleValue,
                      HandleObject);
  if (!callVM<Fn, InitElemGetterSetterOperation>()) {
    return false;
  }

  frame.popn(2);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITELEM_GETTER() {
  return emitInitElemGetterSetter();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITHIDDENELEM_GETTER() {
  return emitInitElemGetterSetter();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITELEM_SETTER() {
  return emitInitElemGetterSetter();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITHIDDENELEM_SETTER() {
  return emitInitElemGetterSetter();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITELEM_INC() {
  // Keep the object and rhs on the stack.
  frame.syncStack(0);

  // Load object in R0, index in R1.
  masm.loadValue(frame.addressOfStackValue(-3), R0);
  masm.loadValue(frame.addressOfStackValue(-2), R1);

  // Call IC.
  if (!emitNextIC()) {
    return false;
  }

  // Pop the rhs
  frame.pop();

  // Increment index
  Address indexAddr = frame.addressOfStackValue(-1);
#ifdef DEBUG
  Label isInt32;
  masm.branchTestInt32(Assembler::Equal, indexAddr, &isInt32);
  masm.assumeUnreachable("INITELEM_INC index must be Int32");
  masm.bind(&isInt32);
#endif
  masm.incrementInt32Value(indexAddr);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_GETLOCAL() {
  frame.pushLocal(GET_LOCALNO(handler.pc()));
  return true;
}

static BaseValueIndex ComputeAddressOfLocal(MacroAssembler& masm,
                                            Register indexScratch) {
  // Locals are stored in memory at a negative offset from the frame pointer. We
  // negate the index first to effectively subtract it.
  masm.negPtr(indexScratch);
  return BaseValueIndex(BaselineFrameReg, indexScratch,
                        BaselineFrame::reverseOffsetOfLocal(0));
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_GETLOCAL() {
  Register scratch = R0.scratchReg();
  LoadUint24Operand(masm, PCRegAtStart, 0, scratch);
  BaseValueIndex addr = ComputeAddressOfLocal(masm, scratch);
  masm.loadValue(addr, R0);
  frame.push(R0);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_SETLOCAL() {
  // Ensure no other StackValue refers to the old value, for instance i + (i =
  // 3). This also allows us to use R0 as scratch below.
  frame.syncStack(1);

  uint32_t local = GET_LOCALNO(handler.pc());
  frame.storeStackValue(-1, frame.addressOfLocal(local), R0);
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_SETLOCAL() {
  Register scratch = R0.scratchReg();
  LoadUint24Operand(masm, PCRegAtStart, 0, scratch);
  BaseValueIndex addr = ComputeAddressOfLocal(masm, scratch);
  masm.loadValue(frame.addressOfStackValue(-1), R1);
  masm.storeValue(R1, addr);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emitFormalArgAccess(JSOp op) {
  MOZ_ASSERT(op == JSOP_GETARG || op == JSOP_SETARG);

  uint32_t arg = GET_ARGNO(handler.pc());

  // Fast path: the script does not use |arguments| or formals don't
  // alias the arguments object.
  if (!handler.script()->argumentsAliasesFormals()) {
    if (op == JSOP_GETARG) {
      frame.pushArg(arg);
    } else {
      // See the comment in emit_JSOP_SETLOCAL.
      frame.syncStack(1);
      frame.storeStackValue(-1, frame.addressOfArg(arg), R0);
    }

    return true;
  }

  // Sync so that we can use R0.
  frame.syncStack(0);

  // If the script is known to have an arguments object, we can just use it.
  // Else, we *may* have an arguments object (because we can't invalidate
  // when needsArgsObj becomes |true|), so we have to test HAS_ARGS_OBJ.
  Label done;
  if (!handler.script()->needsArgsObj()) {
    Label hasArgsObj;
    masm.branchTest32(Assembler::NonZero, frame.addressOfFlags(),
                      Imm32(BaselineFrame::HAS_ARGS_OBJ), &hasArgsObj);
    if (op == JSOP_GETARG) {
      masm.loadValue(frame.addressOfArg(arg), R0);
    } else {
      frame.storeStackValue(-1, frame.addressOfArg(arg), R0);
    }
    masm.jump(&done);
    masm.bind(&hasArgsObj);
  }

  // Load the arguments object data vector.
  Register reg = R2.scratchReg();
  masm.loadPtr(frame.addressOfArgsObj(), reg);
  masm.loadPrivate(Address(reg, ArgumentsObject::getDataSlotOffset()), reg);

  // Load/store the argument.
  Address argAddr(reg, ArgumentsData::offsetOfArgs() + arg * sizeof(Value));
  if (op == JSOP_GETARG) {
    masm.loadValue(argAddr, R0);
    frame.push(R0);
  } else {
    Register temp = R1.scratchReg();
    masm.guardedCallPreBarrierAnyZone(argAddr, MIRType::Value, temp);
    masm.loadValue(frame.addressOfStackValue(-1), R0);
    masm.storeValue(R0, argAddr);

    MOZ_ASSERT(frame.numUnsyncedSlots() == 0);

    // Reload the arguments object.
    Register reg = R2.scratchReg();
    masm.loadPtr(frame.addressOfArgsObj(), reg);

    Label skipBarrier;

    masm.branchPtrInNurseryChunk(Assembler::Equal, reg, temp, &skipBarrier);
    masm.branchValueIsNurseryCell(Assembler::NotEqual, R0, temp, &skipBarrier);

    masm.call(&postBarrierSlot_);

    masm.bind(&skipBarrier);
  }

  masm.bind(&done);
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emitFormalArgAccess(JSOp op) {
  MOZ_ASSERT(op == JSOP_GETARG || op == JSOP_SETARG);

  // Load the index.
  Register argReg = R1.scratchReg();
  LoadUint16Operand(masm, PCRegAtStart, argReg);

  // If the frame has no arguments object, this must be an unaliased access.
  Label isUnaliased, done;
  masm.branchTest32(Assembler::Zero, frame.addressOfFlags(),
                    Imm32(BaselineFrame::HAS_ARGS_OBJ), &isUnaliased);
  {
    Register reg = R2.scratchReg();

    // If it's an unmapped arguments object, this is an unaliased access.
    loadScript(reg);
    masm.branchTest32(
        Assembler::Zero, Address(reg, JSScript::offsetOfImmutableFlags()),
        Imm32(uint32_t(JSScript::ImmutableFlags::HasMappedArgsObj)),
        &isUnaliased);

    // Load the arguments object data vector.
    masm.loadPtr(frame.addressOfArgsObj(), reg);
    masm.loadPrivate(Address(reg, ArgumentsObject::getDataSlotOffset()), reg);

    // Load/store the argument.
    BaseValueIndex argAddr(reg, argReg, ArgumentsData::offsetOfArgs());
    if (op == JSOP_GETARG) {
      masm.loadValue(argAddr, R0);
      frame.push(R0);
    } else {
      masm.guardedCallPreBarrierAnyZone(argAddr, MIRType::Value,
                                        R0.scratchReg());
      masm.loadValue(frame.addressOfStackValue(-1), R0);
      masm.storeValue(R0, argAddr);

      // Reload the arguments object.
      masm.loadPtr(frame.addressOfArgsObj(), reg);

      Register temp = R1.scratchReg();
      masm.branchPtrInNurseryChunk(Assembler::Equal, reg, temp, &done);
      masm.branchValueIsNurseryCell(Assembler::NotEqual, R0, temp, &done);

      masm.call(&postBarrierSlot_);
    }
    masm.jump(&done);
  }
  masm.bind(&isUnaliased);
  {
    BaseValueIndex addr(BaselineFrameReg, argReg,
                        BaselineFrame::offsetOfArg(0));
    if (op == JSOP_GETARG) {
      masm.loadValue(addr, R0);
      frame.push(R0);
    } else {
      masm.loadValue(frame.addressOfStackValue(-1), R0);
      masm.storeValue(R0, addr);
    }
  }

  masm.bind(&done);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GETARG() {
  return emitFormalArgAccess(JSOP_GETARG);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SETARG() {
  // Ionmonkey can't inline functions with SETARG with magic arguments.
  if (JSScript* script = handler.maybeScript()) {
    if (!script->argsObjAliasesFormals() && script->argumentsAliasesFormals()) {
      script->setUninlineable();
    }
  }

  modifiesArguments_ = true;

  return emitFormalArgAccess(JSOP_SETARG);
}

template <>
void BaselineCompilerCodeGen::loadNumFormalArguments(Register dest) {
  masm.move32(Imm32(handler.function()->nargs()), dest);
}

template <>
void BaselineInterpreterCodeGen::loadNumFormalArguments(Register dest) {
  masm.loadFunctionFromCalleeToken(frame.addressOfCalleeToken(), dest);
  masm.load16ZeroExtend(Address(dest, JSFunction::offsetOfNargs()), dest);
}

template <typename Handler>
void BaselineCodeGen<Handler>::emitPushNonArrowFunctionNewTarget() {
  // if (isConstructing()) push(argv[Max(numActualArgs, numFormalArgs)])
  Label notConstructing, done;
  masm.branchTestPtr(Assembler::Zero, frame.addressOfCalleeToken(),
                     Imm32(CalleeToken_FunctionConstructing), &notConstructing);
  {
    Register argvLen = R0.scratchReg();
    Register nformals = R1.scratchReg();
    Address actualArgs(BaselineFrameReg,
                       BaselineFrame::offsetOfNumActualArgs());
    masm.loadPtr(actualArgs, argvLen);

    // If argvLen < nformals, set argvlen := nformals.
    loadNumFormalArguments(nformals);
    masm.cmp32Move32(Assembler::Below, argvLen, nformals, nformals, argvLen);

    BaseValueIndex newTarget(BaselineFrameReg, argvLen,
                             BaselineFrame::offsetOfArg(0));
    masm.loadValue(newTarget, R0);
    masm.jump(&done);
  }
  // else push(undefined)
  masm.bind(&notConstructing);
  masm.moveValue(UndefinedValue(), R0);

  masm.bind(&done);
  frame.push(R0);
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_NEWTARGET() {
  if (handler.script()->isForEval()) {
    frame.pushEvalNewTarget();
    return true;
  }

  MOZ_ASSERT(handler.function());
  frame.syncStack(0);

  if (handler.function()->isArrow()) {
    // Arrow functions store their |new.target| value in an
    // extended slot.
    Register scratch = R0.scratchReg();
    masm.loadFunctionFromCalleeToken(frame.addressOfCalleeToken(), scratch);
    masm.loadValue(
        Address(scratch, FunctionExtended::offsetOfArrowNewTargetSlot()), R0);
    frame.push(R0);
    return true;
  }

  emitPushNonArrowFunctionNewTarget();
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_NEWTARGET() {
  Register scratch1 = R0.scratchReg();
  Register scratch2 = R1.scratchReg();

  Label isFunction, done;
  masm.loadPtr(frame.addressOfCalleeToken(), scratch1);
  masm.branchTestPtr(Assembler::Zero, scratch1, Imm32(CalleeTokenScriptBit),
                     &isFunction);
  {
    // Case 1: eval.
    frame.pushEvalNewTarget();
    masm.jump(&done);
  }

  masm.bind(&isFunction);

  Label notArrow;
  masm.andPtr(Imm32(uint32_t(CalleeTokenMask)), scratch1);
  masm.branchFunctionKind(Assembler::NotEqual, JSFunction::FunctionKind::Arrow,
                          scratch1, scratch2, &notArrow);
  {
    // Case 2: arrow function.
    masm.pushValue(
        Address(scratch1, FunctionExtended::offsetOfArrowNewTargetSlot()));
    masm.jump(&done);
  }

  masm.bind(&notArrow);

  // Case 3: non-arrow function.
  emitPushNonArrowFunctionNewTarget();

  masm.bind(&done);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitThrowConstAssignment() {
  prepareVMCall();
  pushArg(Imm32(JSMSG_BAD_CONST_ASSIGN));

  using Fn = bool (*)(JSContext*, unsigned);
  return callVM<Fn, jit::ThrowRuntimeLexicalError>();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_THROWSETCONST() {
  return emitThrowConstAssignment();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_THROWSETALIASEDCONST() {
  return emitThrowConstAssignment();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_THROWSETCALLEE() {
  return emitThrowConstAssignment();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitUninitializedLexicalCheck(
    const ValueOperand& val) {
  Label done;
  masm.branchTestMagicValue(Assembler::NotEqual, val, JS_UNINITIALIZED_LEXICAL,
                            &done);

  prepareVMCall();
  pushArg(Imm32(JSMSG_UNINITIALIZED_LEXICAL));

  using Fn = bool (*)(JSContext*, unsigned);
  if (!callVM<Fn, jit::ThrowRuntimeLexicalError>()) {
    return false;
  }

  masm.bind(&done);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_CHECKLEXICAL() {
  frame.syncStack(0);
  masm.loadValue(frame.addressOfLocal(GET_LOCALNO(handler.pc())), R0);
  return emitUninitializedLexicalCheck(R0);
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_CHECKLEXICAL() {
  Register scratch = R0.scratchReg();
  LoadUint24Operand(masm, PCRegAtStart, 0, scratch);
  BaseValueIndex addr = ComputeAddressOfLocal(masm, scratch);
  masm.loadValue(addr, R0);
  return emitUninitializedLexicalCheck(R0);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITLEXICAL() {
  return emit_JSOP_SETLOCAL();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITGLEXICAL() {
  frame.popRegsAndSync(1);
  pushGlobalLexicalEnvironmentValue(R1);
  frame.push(R0);
  return emit_JSOP_SETPROP();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CHECKALIASEDLEXICAL() {
  frame.syncStack(0);
  emitGetAliasedVar(R0);
  return emitUninitializedLexicalCheck(R0);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITALIASEDLEXICAL() {
  return emit_JSOP_SETALIASEDVAR();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_UNINITIALIZED() {
  frame.push(MagicValue(JS_UNINITIALIZED_LEXICAL));
  return true;
}

template <>
bool BaselineCompilerCodeGen::emitCall(JSOp op) {
  MOZ_ASSERT(IsCallOp(op));

  frame.syncStack(0);

  uint32_t argc = GET_ARGC(handler.pc());
  masm.move32(Imm32(argc), R0.scratchReg());

  // Call IC
  if (!emitNextIC()) {
    return false;
  }

  // Update FrameInfo.
  bool construct = IsConstructorCallOp(op);
  frame.popn(2 + argc + construct);
  frame.push(R0);
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emitCall(JSOp op) {
  MOZ_ASSERT(IsCallOp(op));

  // The IC expects argc in R0.
  LoadUint16Operand(masm, PCRegAtStart, R0.scratchReg());
  if (!emitNextIC()) {
    return false;
  }

  // Pop the arguments. We have to reload pc/argc because the IC clobbers them.
  // The return value is in R0 so we can't use that.
  Register scratch = R1.scratchReg();
  uint32_t extraValuesToPop = IsConstructorCallOp(op) ? 3 : 2;
  Register spReg = AsRegister(masm.getStackPointer());
  masm.loadPtr(frame.addressOfInterpreterPC(), scratch);
  LoadUint16Operand(masm, scratch, scratch);
  masm.computeEffectiveAddress(
      BaseValueIndex(spReg, scratch, extraValuesToPop * sizeof(Value)), spReg);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitSpreadCall(JSOp op) {
  MOZ_ASSERT(IsCallOp(op));

  frame.syncStack(0);
  masm.move32(Imm32(1), R0.scratchReg());

  // Call IC
  if (!emitNextIC()) {
    return false;
  }

  // Update FrameInfo.
  bool construct = op == JSOP_SPREADNEW || op == JSOP_SPREADSUPERCALL;
  frame.popn(3 + construct);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CALL() {
  return emitCall(JSOP_CALL);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CALL_IGNORES_RV() {
  return emitCall(JSOP_CALL_IGNORES_RV);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CALLITER() {
  return emitCall(JSOP_CALLITER);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_NEW() {
  return emitCall(JSOP_NEW);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SUPERCALL() {
  return emitCall(JSOP_SUPERCALL);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_FUNCALL() {
  return emitCall(JSOP_FUNCALL);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_FUNAPPLY() {
  return emitCall(JSOP_FUNAPPLY);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_EVAL() {
  return emitCall(JSOP_EVAL);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_STRICTEVAL() {
  return emitCall(JSOP_STRICTEVAL);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SPREADCALL() {
  return emitSpreadCall(JSOP_SPREADCALL);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SPREADNEW() {
  return emitSpreadCall(JSOP_SPREADNEW);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SPREADSUPERCALL() {
  return emitSpreadCall(JSOP_SPREADSUPERCALL);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SPREADEVAL() {
  return emitSpreadCall(JSOP_SPREADEVAL);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_STRICTSPREADEVAL() {
  return emitSpreadCall(JSOP_STRICTSPREADEVAL);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_OPTIMIZE_SPREADCALL() {
  frame.syncStack(0);
  masm.loadValue(frame.addressOfStackValue(-1), R0);

  prepareVMCall();
  pushArg(R0);

  using Fn = bool (*)(JSContext*, HandleValue, bool*);
  if (!callVM<Fn, OptimizeSpreadCall>()) {
    return false;
  }

  masm.boxNonDouble(JSVAL_TYPE_BOOLEAN, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_IMPLICITTHIS() {
  frame.syncStack(0);
  masm.loadPtr(frame.addressOfEnvironmentChain(), R0.scratchReg());

  prepareVMCall();

  pushScriptNameArg(R1.scratchReg(), R2.scratchReg());
  pushArg(R0.scratchReg());

  using Fn = bool (*)(JSContext*, HandleObject, HandlePropertyName,
                      MutableHandleValue);
  if (!callVM<Fn, ImplicitThisOperation>()) {
    return false;
  }

  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GIMPLICITTHIS() {
  auto pushUndefined = [this]() {
    frame.push(UndefinedValue());
    return true;
  };
  auto emitImplicitThis = [this]() { return emit_JSOP_IMPLICITTHIS(); };

  return emitTestScriptFlag(JSScript::ImmutableFlags::HasNonSyntacticScope,
                            emitImplicitThis, pushUndefined, R2.scratchReg());
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INSTANCEOF() {
  frame.popRegsAndSync(2);

  if (!emitNextIC()) {
    return false;
  }

  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_TYPEOF() {
  frame.popRegsAndSync(1);

  if (!emitNextIC()) {
    return false;
  }

  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_TYPEOFEXPR() {
  return emit_JSOP_TYPEOF();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_THROWMSG() {
  prepareVMCall();
  pushUint16BytecodeOperandArg(R2.scratchReg());

  using Fn = bool (*)(JSContext*, const unsigned);
  return callVM<Fn, js::ThrowMsgOperation>();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_THROW() {
  // Keep value to throw in R0.
  frame.popRegsAndSync(1);

  prepareVMCall();
  pushArg(R0);

  using Fn = bool (*)(JSContext*, HandleValue);
  return callVM<Fn, js::ThrowOperation>();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_TRY() {
  // Ionmonkey can't inline function with JSOP_TRY.
  if (JSScript* script = handler.maybeScript()) {
    script->setUninlineable();
  }

  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_FINALLY() {
  // JSOP_FINALLY has a def count of 2, but these values are already on the
  // stack (they're pushed by JSOP_GOSUB). Update the compiler's stack state.
  frame.incStackDepth(2);

  // To match the interpreter, emit an interrupt check at the start of the
  // finally block.
  return emitInterruptCheck();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GOSUB() {
  // Jump to the finally block.
  frame.syncStack(0);
  emitJump();
  return true;
}

static void LoadBaselineScriptResumeEntries(MacroAssembler& masm,
                                            JSScript* script, Register dest,
                                            Register scratch) {
  MOZ_ASSERT(dest != scratch);

  masm.movePtr(ImmGCPtr(script), dest);
  masm.loadPtr(Address(dest, JSScript::offsetOfBaselineScript()), dest);
  masm.load32(Address(dest, BaselineScript::offsetOfResumeEntriesOffset()),
              scratch);
  masm.addPtr(scratch, dest);
}

template <>
void BaselineCompilerCodeGen::jumpToResumeEntry(Register resumeIndex,
                                                Register scratch1,
                                                Register scratch2) {
  LoadBaselineScriptResumeEntries(masm, handler.script(), scratch1, scratch2);
  masm.loadPtr(
      BaseIndex(scratch1, resumeIndex, ScaleFromElemWidth(sizeof(uintptr_t))),
      scratch1);
  masm.jump(scratch1);
}

template <>
void BaselineInterpreterCodeGen::jumpToResumeEntry(Register resumeIndex,
                                                   Register scratch1,
                                                   Register scratch2) {
  MOZ_CRASH("NYI: interpreter jumpToResumeEntry");
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_RETSUB() {
  frame.popRegsAndSync(2);

  Label isReturn;
  masm.branchTestBooleanTruthy(/* branchIfTrue = */ false, R0, &isReturn);

  // R0 is |true|. We need to throw R1.
  prepareVMCall();
  pushArg(R1);

  using Fn = bool (*)(JSContext*, HandleValue);
  if (!callVM<Fn, js::ThrowOperation>()) {
    return false;
  }

  masm.bind(&isReturn);

  // R0 is |false|. R1 contains the resumeIndex to jump to.
  Register resumeIndexReg = R1.scratchReg();
  masm.unboxInt32(R1, resumeIndexReg);

  Register scratch1 = R2.scratchReg();
  Register scratch2 = R0.scratchReg();
  jumpToResumeEntry(resumeIndexReg, scratch1, scratch2);
  return true;
}

template <>
template <typename F1, typename F2>
MOZ_MUST_USE bool BaselineCompilerCodeGen::emitDebugInstrumentation(
    const F1& ifDebuggee, const Maybe<F2>& ifNotDebuggee) {
  // The JIT calls either ifDebuggee or (if present) ifNotDebuggee, because it
  // knows statically whether we're compiling with debug instrumentation.

  if (handler.compileDebugInstrumentation()) {
    return ifDebuggee();
  }

  if (ifNotDebuggee) {
    return (*ifNotDebuggee)();
  }

  return true;
}

template <>
template <typename F1, typename F2>
MOZ_MUST_USE bool BaselineInterpreterCodeGen::emitDebugInstrumentation(
    const F1& ifDebuggee, const Maybe<F2>& ifNotDebuggee) {
  // The interpreter emits both ifDebuggee and (if present) ifNotDebuggee
  // paths, with a branch based on the frame's DEBUGGEE flag.

  Label isNotDebuggee, done;
  masm.branchTest32(Assembler::Zero, frame.addressOfFlags(),
                    Imm32(BaselineFrame::DEBUGGEE), &isNotDebuggee);

  if (!ifDebuggee()) {
    return false;
  }

  if (ifNotDebuggee) {
    masm.jump(&done);
  }

  masm.bind(&isNotDebuggee);

  if (ifNotDebuggee && !(*ifNotDebuggee)()) {
    return false;
  }

  masm.bind(&done);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_PUSHLEXICALENV() {
  // Call a stub to push the block on the block chain.
  prepareVMCall();
  masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

  pushScriptScopeArg();
  pushArg(R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, Handle<LexicalScope*>);
  return callVM<Fn, jit::PushLexicalEnv>();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_POPLEXICALENV() {
  frame.syncStack(0);

  masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

  auto ifDebuggee = [this]() {
    prepareVMCall();
    pushBytecodePCArg();
    pushArg(R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*, jsbytecode*);
    return callVM<Fn, jit::DebugLeaveThenPopLexicalEnv>();
  };
  auto ifNotDebuggee = [this]() {
    prepareVMCall();
    pushArg(R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*);
    return callVM<Fn, jit::PopLexicalEnv>();
  };
  return emitDebugInstrumentation(ifDebuggee, mozilla::Some(ifNotDebuggee));
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_FRESHENLEXICALENV() {
  frame.syncStack(0);

  masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

  auto ifDebuggee = [this]() {
    prepareVMCall();
    pushBytecodePCArg();
    pushArg(R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*, jsbytecode*);
    return callVM<Fn, jit::DebugLeaveThenFreshenLexicalEnv>();
  };
  auto ifNotDebuggee = [this]() {
    prepareVMCall();
    pushArg(R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*);
    return callVM<Fn, jit::FreshenLexicalEnv>();
  };
  return emitDebugInstrumentation(ifDebuggee, mozilla::Some(ifNotDebuggee));
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_RECREATELEXICALENV() {
  frame.syncStack(0);

  masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

  auto ifDebuggee = [this]() {
    prepareVMCall();
    pushBytecodePCArg();
    pushArg(R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*, jsbytecode*);
    return callVM<Fn, jit::DebugLeaveThenRecreateLexicalEnv>();
  };
  auto ifNotDebuggee = [this]() {
    prepareVMCall();
    pushArg(R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*);
    return callVM<Fn, jit::RecreateLexicalEnv>();
  };
  return emitDebugInstrumentation(ifDebuggee, mozilla::Some(ifNotDebuggee));
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DEBUGLEAVELEXICALENV() {
  auto ifDebuggee = [this]() {
    prepareVMCall();
    masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());
    pushBytecodePCArg();
    pushArg(R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*, jsbytecode*);
    return callVM<Fn, jit::DebugLeaveLexicalEnv>();
  };
  return emitDebugInstrumentation(ifDebuggee);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_PUSHVARENV() {
  prepareVMCall();
  masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());
  pushScriptScopeArg();
  pushArg(R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, HandleScope);
  return callVM<Fn, jit::PushVarEnv>();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_POPVARENV() {
  prepareVMCall();
  masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());
  pushArg(R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*);
  return callVM<Fn, jit::PopVarEnv>();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_ENTERWITH() {
  // Pop "with" object to R0.
  frame.popRegsAndSync(1);

  // Call a stub to push the object onto the environment chain.
  prepareVMCall();
  masm.loadBaselineFramePtr(BaselineFrameReg, R1.scratchReg());

  pushScriptScopeArg();
  pushArg(R0);
  pushArg(R1.scratchReg());

  using Fn =
      bool (*)(JSContext*, BaselineFrame*, HandleValue, Handle<WithScope*>);
  return callVM<Fn, jit::EnterWith>();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_LEAVEWITH() {
  // Call a stub to pop the with object from the environment chain.
  prepareVMCall();

  masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());
  pushArg(R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*);
  return callVM<Fn, jit::LeaveWith>();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_EXCEPTION() {
  prepareVMCall();

  using Fn = bool (*)(JSContext*, MutableHandleValue);
  if (!callVM<Fn, GetAndClearException>()) {
    return false;
  }

  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DEBUGGER() {
  prepareVMCall();
  pushBytecodePCArg();

  frame.assertSyncedStack();
  masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());
  pushArg(R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, jsbytecode*, bool*);
  if (!callVM<Fn, jit::OnDebuggerStatement>()) {
    return false;
  }

  // If the stub returns |true|, return the frame's return value.
  Label done;
  masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, &done);
  {
    masm.loadValue(frame.addressOfReturnValue(), JSReturnOperand);
    masm.jump(&return_);
  }
  masm.bind(&done);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitReturn() {
  auto ifDebuggee = [this]() {
    // Move return value into the frame's rval slot.
    masm.storeValue(JSReturnOperand, frame.addressOfReturnValue());
    masm.or32(Imm32(BaselineFrame::HAS_RVAL), frame.addressOfFlags());

    // Load BaselineFrame pointer in R0.
    frame.syncStack(0);
    masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

    prepareVMCall();
    pushBytecodePCArg();
    pushArg(R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*, jsbytecode*);
    if (!callVM<Fn, jit::DebugEpilogueOnBaselineReturn>()) {
      return false;
    }

    // Fix up the RetAddrEntry appended by callVM for on-stack recompilation.
    handler.markLastRetAddrEntryKind(RetAddrEntry::Kind::DebugEpilogue);

    masm.loadValue(frame.addressOfReturnValue(), JSReturnOperand);
    return true;
  };
  if (!emitDebugInstrumentation(ifDebuggee)) {
    return false;
  }

  // Only emit the jump if this JSOP_RETRVAL is not the last instruction.
  // Not needed for last instruction, because last instruction flows
  // into return label.
  if (!handler.isDefinitelyLastOp()) {
    masm.jump(&return_);
  }

  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_RETURN() {
  frame.assertStackDepth(1);

  frame.popValue(JSReturnOperand);
  return emitReturn();
}

template <typename Handler>
void BaselineCodeGen<Handler>::emitLoadReturnValue(ValueOperand val) {
  Label done, noRval;
  masm.branchTest32(Assembler::Zero, frame.addressOfFlags(),
                    Imm32(BaselineFrame::HAS_RVAL), &noRval);
  masm.loadValue(frame.addressOfReturnValue(), val);
  masm.jump(&done);

  masm.bind(&noRval);
  masm.moveValue(UndefinedValue(), val);

  masm.bind(&done);
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_RETRVAL() {
  frame.assertStackDepth(0);

  masm.moveValue(UndefinedValue(), JSReturnOperand);

  if (!handler.maybeScript() || !handler.maybeScript()->noScriptRval()) {
    // Return the value in the return value slot, if any.
    Label done;
    Address flags = frame.addressOfFlags();
    masm.branchTest32(Assembler::Zero, flags, Imm32(BaselineFrame::HAS_RVAL),
                      &done);
    masm.loadValue(frame.addressOfReturnValue(), JSReturnOperand);
    masm.bind(&done);
  }

  return emitReturn();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_TOID() {
  // Load index in R0, but keep values on the stack for the decompiler.
  frame.syncStack(0);
  masm.loadValue(frame.addressOfStackValue(-1), R0);

  // No-op if the index is trivally convertable to an id.
  Label done;
  masm.branchTestInt32(Assembler::Equal, R0, &done);
  masm.branchTestString(Assembler::Equal, R0, &done);
  masm.branchTestSymbol(Assembler::Equal, R0, &done);

  prepareVMCall();

  pushArg(R0);

  using Fn = bool (*)(JSContext*, HandleValue, MutableHandleValue);
  if (!callVM<Fn, js::ToIdOperation>()) {
    return false;
  }

  masm.bind(&done);
  frame.pop();  // Pop index.
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_TOASYNCITER() {
  frame.syncStack(0);
  masm.unboxObject(frame.addressOfStackValue(-2), R0.scratchReg());
  masm.loadValue(frame.addressOfStackValue(-1), R1);

  prepareVMCall();
  pushArg(R1);
  pushArg(R0.scratchReg());

  using Fn = JSObject* (*)(JSContext*, HandleObject, HandleValue);
  if (!callVM<Fn, js::CreateAsyncFromSyncIterator>()) {
    return false;
  }

  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.popn(2);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_TRYSKIPAWAIT() {
  frame.syncStack(0);
  masm.loadValue(frame.addressOfStackValue(-1), R0);

  prepareVMCall();
  pushArg(R0);

  using Fn = bool (*)(JSContext*, HandleValue, MutableHandleValue);
  if (!callVM<Fn, jit::TrySkipAwait>()) {
    return false;
  }

  Label cannotSkip, done;
  masm.branchTestMagicValue(Assembler::Equal, R0, JS_CANNOT_SKIP_AWAIT,
                            &cannotSkip);
  masm.moveValue(BooleanValue(true), R1);
  masm.jump(&done);

  masm.bind(&cannotSkip);
  masm.loadValue(frame.addressOfStackValue(-1), R0);
  masm.moveValue(BooleanValue(false), R1);

  masm.bind(&done);

  frame.pop();
  frame.push(R0);
  frame.push(R1);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_ASYNCAWAIT() {
  frame.syncStack(0);
  masm.loadValue(frame.addressOfStackValue(-2), R1);
  masm.unboxObject(frame.addressOfStackValue(-1), R0.scratchReg());

  prepareVMCall();
  pushArg(R1);
  pushArg(R0.scratchReg());

  using Fn = JSObject* (*)(JSContext*, Handle<AsyncFunctionGeneratorObject*>,
                           HandleValue);
  if (!callVM<Fn, js::AsyncFunctionAwait>()) {
    return false;
  }

  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.popn(2);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_ASYNCRESOLVE() {
  frame.syncStack(0);
  masm.loadValue(frame.addressOfStackValue(-2), R1);
  masm.unboxObject(frame.addressOfStackValue(-1), R0.scratchReg());

  prepareVMCall();
  pushUint8BytecodeOperandArg(R2.scratchReg());
  pushArg(R1);
  pushArg(R0.scratchReg());

  using Fn = JSObject* (*)(JSContext*, Handle<AsyncFunctionGeneratorObject*>,
                           HandleValue, AsyncFunctionResolveKind);
  if (!callVM<Fn, js::AsyncFunctionResolve>()) {
    return false;
  }

  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.popn(2);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CHECKOBJCOERCIBLE() {
  frame.syncStack(0);
  masm.loadValue(frame.addressOfStackValue(-1), R0);

  Label fail, done;

  masm.branchTestUndefined(Assembler::Equal, R0, &fail);
  masm.branchTestNull(Assembler::NotEqual, R0, &done);

  masm.bind(&fail);
  prepareVMCall();

  pushArg(R0);

  using Fn = bool (*)(JSContext*, HandleValue);
  if (!callVM<Fn, ThrowObjectCoercible>()) {
    return false;
  }

  masm.bind(&done);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_TOSTRING() {
  // Keep top stack value in R0.
  frame.popRegsAndSync(1);

  // Inline path for string.
  Label done;
  masm.branchTestString(Assembler::Equal, R0, &done);

  prepareVMCall();

  pushArg(R0);

  // Call ToStringSlow which doesn't handle string inputs.
  using Fn = JSString* (*)(JSContext*, HandleValue);
  if (!callVM<Fn, ToStringSlow<CanGC>>()) {
    return false;
  }

  masm.tagValue(JSVAL_TYPE_STRING, ReturnReg, R0);

  masm.bind(&done);
  frame.push(R0);
  return true;
}

template <>
void BaselineCompilerCodeGen::emitGetTableSwitchIndex(ValueOperand val,
                                                      Register dest) {
  jsbytecode* pc = handler.pc();
  jsbytecode* defaultpc = pc + GET_JUMP_OFFSET(pc);
  Label* defaultLabel = handler.labelOf(defaultpc);

  int32_t low = GET_JUMP_OFFSET(pc + 1 * JUMP_OFFSET_LEN);
  int32_t high = GET_JUMP_OFFSET(pc + 2 * JUMP_OFFSET_LEN);
  int32_t length = high - low + 1;

  // Jump to the 'default' pc if not int32 (tableswitch is only used when
  // all cases are int32).
  masm.branchTestInt32(Assembler::NotEqual, val, defaultLabel);
  masm.unboxInt32(val, dest);

  // Subtract 'low'. Bounds check.
  if (low != 0) {
    masm.sub32(Imm32(low), dest);
  }
  masm.branch32(Assembler::AboveOrEqual, dest, Imm32(length), defaultLabel);
}

template <>
void BaselineInterpreterCodeGen::emitGetTableSwitchIndex(ValueOperand val,
                                                         Register dest) {
  MOZ_CRASH("NYI: interpreter emitTableSwitchJumpTableIndex");
}

template <>
void BaselineCompilerCodeGen::emitTableSwitchJump(Register key,
                                                  Register scratch1,
                                                  Register scratch2) {
  // Jump to resumeEntries[firstResumeIndex + key].

  // Note: BytecodeEmitter::allocateResumeIndex static_asserts
  // |firstResumeIndex * sizeof(uintptr_t)| fits in int32_t.
  uint32_t firstResumeIndex =
      GET_RESUMEINDEX(handler.pc() + 3 * JUMP_OFFSET_LEN);
  LoadBaselineScriptResumeEntries(masm, handler.script(), scratch1, scratch2);
  masm.loadPtr(BaseIndex(scratch1, key, ScaleFromElemWidth(sizeof(uintptr_t)),
                         firstResumeIndex * sizeof(uintptr_t)),
               scratch1);
  masm.jump(scratch1);
}

template <>
void BaselineInterpreterCodeGen::emitTableSwitchJump(Register key,
                                                     Register scratch1,
                                                     Register scratch2) {
  MOZ_CRASH("NYI: interpreter emitTableSwitchJump");
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_TABLESWITCH() {
  frame.popRegsAndSync(1);

  Register key = R0.scratchReg();
  Register scratch1 = R1.scratchReg();
  Register scratch2 = R2.scratchReg();

  // Call a stub to convert R0 from double to int32 if needed.
  // Note: this stub may clobber scratch1.
  masm.call(cx->runtime()->jitRuntime()->getDoubleToInt32ValueStub());

  // Load the index in the jump table in |key|, or branch to default pc if not
  // int32 or out-of-range.
  emitGetTableSwitchIndex(R0, key);

  // Jump to the target pc.
  emitTableSwitchJump(key, scratch1, scratch2);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_ITER() {
  frame.popRegsAndSync(1);

  if (!emitNextIC()) {
    return false;
  }

  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_MOREITER() {
  frame.syncStack(0);

  masm.unboxObject(frame.addressOfStackValue(-1), R1.scratchReg());

  masm.iteratorMore(R1.scratchReg(), R0, R2.scratchReg());
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitIsMagicValue() {
  frame.syncStack(0);

  Label isMagic, done;
  masm.branchTestMagic(Assembler::Equal, frame.addressOfStackValue(-1),
                       &isMagic);
  masm.moveValue(BooleanValue(false), R0);
  masm.jump(&done);

  masm.bind(&isMagic);
  masm.moveValue(BooleanValue(true), R0);

  masm.bind(&done);
  frame.push(R0, JSVAL_TYPE_BOOLEAN);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_ISNOITER() {
  return emitIsMagicValue();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_ENDITER() {
  frame.popRegsAndSync(1);

  AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
  regs.take(BaselineFrameReg);

  Register obj = R0.scratchReg();
  regs.take(obj);
  masm.unboxObject(R0, obj);

  Register temp1 = regs.takeAny();
  Register temp2 = regs.takeAny();
  Register temp3 = regs.takeAny();
  masm.iteratorClose(obj, temp1, temp2, temp3);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_ISGENCLOSING() {
  return emitIsMagicValue();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GETRVAL() {
  frame.syncStack(0);

  emitLoadReturnValue(R0);

  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SETRVAL() {
  // Store to the frame's return value slot.
  frame.storeStackValue(-1, frame.addressOfReturnValue(), R2);
  masm.or32(Imm32(BaselineFrame::HAS_RVAL), frame.addressOfFlags());
  frame.pop();
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CALLEE() {
  MOZ_ASSERT_IF(handler.maybeScript(),
                handler.maybeScript()->functionNonDelazifying());
  frame.syncStack(0);
  masm.loadFunctionFromCalleeToken(frame.addressOfCalleeToken(),
                                   R0.scratchReg());
  masm.tagValue(JSVAL_TYPE_OBJECT, R0.scratchReg(), R0);
  frame.push(R0);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_ENVCALLEE() {
  frame.syncStack(0);
  uint8_t numHops = GET_UINT8(handler.pc());
  Register scratch = R0.scratchReg();

  masm.loadPtr(frame.addressOfEnvironmentChain(), scratch);
  for (unsigned i = 0; i < numHops; i++) {
    Address nextAddr(scratch,
                     EnvironmentObject::offsetOfEnclosingEnvironment());
    masm.unboxObject(nextAddr, scratch);
  }

  masm.loadValue(Address(scratch, CallObject::offsetOfCallee()), R0);
  frame.push(R0);
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_ENVCALLEE() {
  Register numHops = R0.scratchReg();
  LoadUint8Operand(masm, PCRegAtStart, numHops);

  Register env = R1.scratchReg();
  masm.loadPtr(frame.addressOfEnvironmentChain(), env);

  // Skip numHops environment objects.
  Label top, done;
  masm.bind(&top);
  masm.branchTest32(Assembler::Zero, numHops, numHops, &done);
  {
    Address nextAddr(env, EnvironmentObject::offsetOfEnclosingEnvironment());
    masm.unboxObject(nextAddr, env);
    masm.sub32(Imm32(1), numHops);
    masm.jump(&top);
  }

  masm.bind(&done);
  masm.pushValue(Address(env, CallObject::offsetOfCallee()));
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SUPERBASE() {
  frame.popRegsAndSync(1);

  Register scratch = R0.scratchReg();
  Register proto = R1.scratchReg();

  // Unbox callee.
  masm.unboxObject(R0, scratch);

  // Load [[HomeObject]]
  Address homeObjAddr(scratch,
                      FunctionExtended::offsetOfMethodHomeObjectSlot());
#ifdef DEBUG
  Label isObject;
  masm.branchTestObject(Assembler::Equal, homeObjAddr, &isObject);
  masm.assumeUnreachable("[[HomeObject]] must be Object");
  masm.bind(&isObject);
#endif
  masm.unboxObject(homeObjAddr, scratch);

  // Load prototype from [[HomeObject]]
  masm.loadObjProto(scratch, proto);

  Label hasProto;
  MOZ_ASSERT(uintptr_t(TaggedProto::LazyProto) == 1);
  masm.branchPtr(Assembler::Above, proto, ImmWord(1), &hasProto);

  // Use VMCall for missing or lazy proto
  prepareVMCall();
  pushArg(scratch);  // [[HomeObject]]

  using Fn = JSObject* (*)(JSContext*, HandleObject);
  if (!callVM<Fn, HomeObjectSuperBase>()) {
    return false;
  }
  masm.movePtr(ReturnReg, proto);

  // Box prototype and return
  masm.bind(&hasProto);
  masm.tagValue(JSVAL_TYPE_OBJECT, proto, R1);
  frame.push(R1);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_SUPERFUN() {
  frame.popRegsAndSync(1);

  Register callee = R0.scratchReg();
  Register proto = R1.scratchReg();
  Register scratch = R2.scratchReg();

  // Unbox callee.
  masm.unboxObject(R0, callee);

  // Load prototype of callee
  masm.loadObjProto(callee, proto);

  // Use VMCall for missing or lazy proto
  Label needVMCall;
  MOZ_ASSERT(uintptr_t(TaggedProto::LazyProto) == 1);
  masm.branchPtr(Assembler::BelowOrEqual, proto, ImmWord(1), &needVMCall);

  // Use VMCall for non-JSFunction objects (eg. Proxy)
  masm.branchTestObjClass(Assembler::NotEqual, proto, &JSFunction::class_,
                          scratch, proto, &needVMCall);

  // Use VMCall if not constructor
  masm.load16ZeroExtend(Address(proto, JSFunction::offsetOfFlags()), scratch);
  masm.branchTest32(Assembler::Zero, scratch, Imm32(JSFunction::CONSTRUCTOR),
                    &needVMCall);

  // Valid constructor
  Label hasSuperFun;
  masm.jump(&hasSuperFun);

  // Slow path VM Call
  masm.bind(&needVMCall);
  prepareVMCall();
  pushArg(callee);

  using Fn = JSObject* (*)(JSContext*, HandleObject);
  if (!callVM<Fn, js::SuperFunOperation>()) {
    return false;
  }
  masm.movePtr(ReturnReg, proto);

  // Box prototype and return
  masm.bind(&hasSuperFun);
  masm.tagValue(JSVAL_TYPE_OBJECT, proto, R1);
  frame.push(R1);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_ARGUMENTS() {
  frame.syncStack(0);

  MOZ_ASSERT_IF(handler.maybeScript(),
                handler.maybeScript()->argumentsHasVarBinding());

  Label done;
  if (!handler.maybeScript() || !handler.maybeScript()->needsArgsObj()) {
    // We assume the script does not need an arguments object. However, this
    // assumption can be invalidated later, see argumentsOptimizationFailed
    // in JSScript. Guard on the script's NeedsArgsObj flag.
    masm.moveValue(MagicValue(JS_OPTIMIZED_ARGUMENTS), R0);

    // If we don't need an arguments object, skip the VM call.
    Register scratch = R1.scratchReg();
    loadScript(scratch);
    masm.branchTest32(
        Assembler::Zero, Address(scratch, JSScript::offsetOfMutableFlags()),
        Imm32(uint32_t(JSScript::MutableFlags::NeedsArgsObj)), &done);
  }

  prepareVMCall();

  masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());
  pushArg(R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, MutableHandleValue);
  if (!callVM<Fn, jit::NewArgumentsObject>()) {
    return false;
  }

  masm.bind(&done);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_REST() {
  frame.syncStack(0);

  if (!emitNextIC()) {
    return false;
  }

  // Mark R0 as pushed stack value.
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_GENERATOR() {
  frame.assertStackDepth(0);

  masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

  prepareVMCall();
  pushArg(R0.scratchReg());

  using Fn = JSObject* (*)(JSContext*, BaselineFrame*);
  if (!callVM<Fn, jit::CreateGenerator>()) {
    return false;
  }

  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITIALYIELD() {
  frame.syncStack(0);
  frame.assertStackDepth(1);

  Register genObj = R2.scratchReg();
  masm.unboxObject(frame.addressOfStackValue(-1), genObj);

  MOZ_ASSERT_IF(handler.maybePC(), GET_RESUMEINDEX(handler.maybePC()) == 0);
  masm.storeValue(
      Int32Value(0),
      Address(genObj, AbstractGeneratorObject::offsetOfResumeIndexSlot()));

  Register envObj = R0.scratchReg();
  Register temp = R1.scratchReg();
  Address envChainSlot(genObj,
                       AbstractGeneratorObject::offsetOfEnvironmentChainSlot());
  masm.loadPtr(frame.addressOfEnvironmentChain(), envObj);
  masm.guardedCallPreBarrierAnyZone(envChainSlot, MIRType::Value, temp);
  masm.storeValue(JSVAL_TYPE_OBJECT, envObj, envChainSlot);

  Label skipBarrier;
  masm.branchPtrInNurseryChunk(Assembler::Equal, genObj, temp, &skipBarrier);
  masm.branchPtrInNurseryChunk(Assembler::NotEqual, envObj, temp, &skipBarrier);
  masm.push(genObj);
  MOZ_ASSERT(genObj == R2.scratchReg());
  masm.call(&postBarrierSlot_);
  masm.pop(genObj);
  masm.bind(&skipBarrier);

  masm.tagValue(JSVAL_TYPE_OBJECT, genObj, JSReturnOperand);
  return emitReturn();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_YIELD() {
  // Store generator in R0.
  frame.popRegsAndSync(1);

  Register genObj = R2.scratchReg();
  masm.unboxObject(R0, genObj);

  if (frame.hasKnownStackDepth(1)) {
    // If the expression stack is empty, we can inline the YIELD. Note that this
    // branch is never taken for the interpreter because it doesn't know static
    // stack depths.

    Register temp = R1.scratchReg();
    Address resumeIndexSlot(genObj,
                            AbstractGeneratorObject::offsetOfResumeIndexSlot());
    jsbytecode* pc = handler.maybePC();
    MOZ_ASSERT(pc, "compiler-only code never has a null pc");
    masm.move32(Imm32(GET_RESUMEINDEX(pc)), temp);
    masm.storeValue(JSVAL_TYPE_INT32, temp, resumeIndexSlot);

    Register envObj = R0.scratchReg();
    Address envChainSlot(
        genObj, AbstractGeneratorObject::offsetOfEnvironmentChainSlot());
    masm.loadPtr(frame.addressOfEnvironmentChain(), envObj);
    masm.guardedCallPreBarrier(envChainSlot, MIRType::Value);
    masm.storeValue(JSVAL_TYPE_OBJECT, envObj, envChainSlot);

    Label skipBarrier;
    masm.branchPtrInNurseryChunk(Assembler::Equal, genObj, temp, &skipBarrier);
    masm.branchPtrInNurseryChunk(Assembler::NotEqual, envObj, temp,
                                 &skipBarrier);
    MOZ_ASSERT(genObj == R2.scratchReg());
    masm.call(&postBarrierSlot_);
    masm.bind(&skipBarrier);
  } else {
    masm.loadBaselineFramePtr(BaselineFrameReg, R1.scratchReg());

    prepareVMCall();
    pushBytecodePCArg();
    pushArg(R1.scratchReg());
    pushArg(genObj);

    using Fn = bool (*)(JSContext*, HandleObject, BaselineFrame*, jsbytecode*);
    if (!callVM<Fn, jit::NormalSuspend>()) {
      return false;
    }
  }

  masm.loadValue(frame.addressOfStackValue(-1), JSReturnOperand);
  return emitReturn();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_AWAIT() {
  return emit_JSOP_YIELD();
}

template <>
template <typename F>
bool BaselineCompilerCodeGen::emitAfterYieldDebugInstrumentation(
    const F& ifDebuggee, Register) {
  if (handler.compileDebugInstrumentation()) {
    return ifDebuggee();
  }
  return true;
}

template <>
template <typename F>
bool BaselineInterpreterCodeGen::emitAfterYieldDebugInstrumentation(
    const F& ifDebuggee, Register scratch) {
  // Note that we can't use emitDebugInstrumentation here because the frame's
  // DEBUGGEE flag hasn't been initialized yet.

  // If the current Realm is not a debuggee we're done.
  Label done;
  masm.loadPtr(AbsoluteAddress(cx->addressOfRealm()), scratch);
  masm.branchTest32(Assembler::Zero,
                    Address(scratch, Realm::offsetOfDebugModeBits()),
                    Imm32(Realm::debugModeIsDebuggeeBit()), &done);

  if (!ifDebuggee()) {
    return false;
  }

  masm.bind(&done);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_AFTERYIELD() {
  if (!emit_JSOP_JUMPTARGET()) {
    return false;
  }

  auto ifDebuggee = [this]() {
    frame.assertSyncedStack();
    masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());
    prepareVMCall();
    pushBytecodePCArg();
    pushArg(R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*, jsbytecode*, bool*);
    if (!callVM<Fn, jit::DebugAfterYield>()) {
      return false;
    }

    handler.markLastRetAddrEntryKind(RetAddrEntry::Kind::DebugAfterYield);

    Label done;
    masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, &done);
    {
      masm.loadValue(frame.addressOfReturnValue(), JSReturnOperand);
      masm.jump(&return_);
    }
    masm.bind(&done);
    return true;
  };
  return emitAfterYieldDebugInstrumentation(ifDebuggee, R0.scratchReg());
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_FINALYIELDRVAL() {
  // Store generator in R0.
  frame.popRegsAndSync(1);
  masm.unboxObject(R0, R0.scratchReg());

  prepareVMCall();
  pushBytecodePCArg();
  pushArg(R0.scratchReg());

  using Fn = bool (*)(JSContext*, HandleObject, jsbytecode*);
  if (!callVM<Fn, jit::FinalSuspend>()) {
    return false;
  }

  masm.loadValue(frame.addressOfReturnValue(), JSReturnOperand);
  return emitReturn();
}

template <>
void BaselineCompilerCodeGen::emitJumpToInterpretOpLabel() {
  TrampolinePtr code =
      cx->runtime()->jitRuntime()->baselineInterpreter().interpretOpAddr();
  masm.jump(code);
}

template <>
void BaselineInterpreterCodeGen::emitJumpToInterpretOpLabel() {
  masm.jump(handler.interpretOpLabel());
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitEnterGeneratorCode(Register script,
                                                      Register resumeIndex,
                                                      Register scratch) {
  Address baselineAddr(script, JSScript::offsetOfBaselineScript());

  auto emitEnterBaseline = [&]() {
    masm.loadPtr(baselineAddr, script);
    masm.load32(Address(script, BaselineScript::offsetOfResumeEntriesOffset()),
                scratch);
    masm.addPtr(scratch, script);
    masm.loadPtr(
        BaseIndex(script, resumeIndex, ScaleFromElemWidth(sizeof(uintptr_t))),
        scratch);
    masm.jump(scratch);
  };

  if (!JitOptions.baselineInterpreter) {
    // We must have a BaselineScript.
    emitEnterBaseline();
    return true;
  }

  // If the Baseline Interpreter is enabled we resume in either the
  // BaselineScript (if present) or Baseline Interpreter.
  Label noBaselineScript;
  masm.branchPtr(Assembler::BelowOrEqual, baselineAddr,
                 ImmPtr(BASELINE_DISABLED_SCRIPT), &noBaselineScript);
  emitEnterBaseline();

  masm.bind(&noBaselineScript);
  MOZ_CRASH("NYI: enter interpreted generator");
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitGeneratorResume(
    GeneratorResumeKind resumeKind) {
  frame.syncStack(0);
  masm.assertStackAlignment(sizeof(Value), 0);

  AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
  regs.take(BaselineFrameReg);

  // Load generator object.
  Register genObj = regs.takeAny();
  masm.unboxObject(frame.addressOfStackValue(-2), genObj);

  // Load callee.
  Register callee = regs.takeAny();
  masm.unboxObject(
      Address(genObj, AbstractGeneratorObject::offsetOfCalleeSlot()), callee);

  // Load the return value.
  ValueOperand retVal = regs.takeAnyValue();
  masm.loadValue(frame.addressOfStackValue(-1), retVal);

  // Branch to interpret if the script does not have a TypeScript or
  // BaselineScript (depending on whether the Baseline Interpreter is enabled).
  // Note that we don't relazify generator scripts, so the function is
  // guaranteed to be non-lazy.
  Label interpret;
  Register scratch1 = regs.takeAny();
  masm.loadPtr(Address(callee, JSFunction::offsetOfScript()), scratch1);
  Address baselineAddr(scratch1, JSScript::offsetOfBaselineScript());
  if (JitOptions.baselineInterpreter) {
    Address typesAddr(scratch1, JSScript::offsetOfTypes());
    masm.branchPtr(Assembler::Equal, typesAddr, ImmPtr(nullptr), &interpret);
  } else {
    masm.branchPtr(Assembler::BelowOrEqual, baselineAddr,
                   ImmPtr(BASELINE_DISABLED_SCRIPT), &interpret);
  }

#ifdef JS_TRACE_LOGGING
  if (JS::TraceLoggerSupported()) {
    masm.loadPtr(baselineAddr, scratch1);
    if (!emitTraceLoggerResume(scratch1, regs)) {
      return false;
    }
  }
#endif

  // Push |undefined| for all formals.
  Register scratch2 = regs.takeAny();
  Label loop, loopDone;
  masm.load16ZeroExtend(Address(callee, JSFunction::offsetOfNargs()), scratch2);
  masm.bind(&loop);
  masm.branchTest32(Assembler::Zero, scratch2, scratch2, &loopDone);
  {
    masm.pushValue(UndefinedValue());
    masm.sub32(Imm32(1), scratch2);
    masm.jump(&loop);
  }
  masm.bind(&loopDone);

  // Push |undefined| for |this|.
  masm.pushValue(UndefinedValue());

  // Update BaselineFrame frameSize field and create the frame descriptor.
  masm.computeEffectiveAddress(
      Address(BaselineFrameReg, BaselineFrame::FramePointerOffset), scratch2);
  masm.subStackPtrFrom(scratch2);
  masm.store32(scratch2, Address(BaselineFrameReg,
                                 BaselineFrame::reverseOffsetOfFrameSize()));
  masm.makeFrameDescriptor(scratch2, FrameType::BaselineJS,
                           JitFrameLayout::Size());

  masm.push(Imm32(0));  // actual argc
  masm.PushCalleeToken(callee, /* constructing = */ false);
  masm.push(scratch2);  // frame descriptor

  // PushCalleeToken bumped framePushed. Reset it.
  MOZ_ASSERT(masm.framePushed() == sizeof(uintptr_t));
  masm.setFramePushed(0);

  regs.add(callee);

  // Push a fake return address on the stack. We will resume here when the
  // generator returns.
  Label genStart, returnTarget;
#ifdef JS_USE_LINK_REGISTER
  masm.call(&genStart);
#else
  masm.callAndPushReturnAddress(&genStart);
#endif

  // Add a RetAddrEntry so the return offset -> pc mapping works.
  if (!handler.appendRetAddrEntry(cx, RetAddrEntry::Kind::IC,
                                  masm.currentOffset())) {
    return false;
  }

  masm.jump(&returnTarget);
  masm.bind(&genStart);
#ifdef JS_USE_LINK_REGISTER
  masm.pushReturnAddress();
#endif

  // If profiler instrumentation is on, update lastProfilingFrame on
  // current JitActivation
  {
    Register scratchReg = scratch2;
    Label skip;
    AbsoluteAddress addressOfEnabled(
        cx->runtime()->geckoProfiler().addressOfEnabled());
    masm.branch32(Assembler::Equal, addressOfEnabled, Imm32(0), &skip);
    masm.loadJSContext(scratchReg);
    masm.loadPtr(Address(scratchReg, JSContext::offsetOfProfilingActivation()),
                 scratchReg);
    masm.storeStackPtr(
        Address(scratchReg, JitActivation::offsetOfLastProfilingFrame()));
    masm.bind(&skip);
  }

  // Construct BaselineFrame.
  masm.push(BaselineFrameReg);
  masm.moveStackPtrTo(BaselineFrameReg);
  masm.subFromStackPtr(Imm32(BaselineFrame::Size()));
  masm.assertStackAlignment(sizeof(Value), 0);

  // Store flags and env chain.
  masm.store32(Imm32(BaselineFrame::HAS_INITIAL_ENV), frame.addressOfFlags());
  masm.unboxObject(
      Address(genObj, AbstractGeneratorObject::offsetOfEnvironmentChainSlot()),
      scratch2);
  masm.storePtr(scratch2, frame.addressOfEnvironmentChain());

  // Store the arguments object if there is one.
  Label noArgsObj;
  Address argsObjSlot(genObj, AbstractGeneratorObject::offsetOfArgsObjSlot());
  masm.branchTestUndefined(Assembler::Equal, argsObjSlot, &noArgsObj);
  masm.unboxObject(argsObjSlot, scratch2);
  {
    masm.storePtr(scratch2, frame.addressOfArgsObj());
    masm.or32(Imm32(BaselineFrame::HAS_ARGS_OBJ), frame.addressOfFlags());
  }
  masm.bind(&noArgsObj);

  // Push expression slots if needed.
  Label noExprStack;
  Address exprStackSlot(genObj,
                        AbstractGeneratorObject::offsetOfExpressionStackSlot());
  masm.branchTestNull(Assembler::Equal, exprStackSlot, &noExprStack);
  {
    masm.unboxObject(exprStackSlot, scratch2);

    Register initLength = regs.takeAny();
    masm.loadPtr(Address(scratch2, NativeObject::offsetOfElements()), scratch2);
    masm.load32(Address(scratch2, ObjectElements::offsetOfInitializedLength()),
                initLength);
    masm.store32(
        Imm32(0),
        Address(scratch2, ObjectElements::offsetOfInitializedLength()));

    Label loop, loopDone;
    masm.bind(&loop);
    masm.branchTest32(Assembler::Zero, initLength, initLength, &loopDone);
    {
      masm.pushValue(Address(scratch2, 0));
      masm.guardedCallPreBarrierAnyZone(Address(scratch2, 0), MIRType::Value,
                                        scratch1);
      masm.addPtr(Imm32(sizeof(Value)), scratch2);
      masm.sub32(Imm32(1), initLength);
      masm.jump(&loop);
    }
    masm.bind(&loopDone);
    regs.add(initLength);
  }

  masm.bind(&noExprStack);
  masm.pushValue(retVal);

  masm.switchToObjectRealm(genObj, scratch2);

  if (resumeKind == GeneratorResumeKind::Next) {
    // Load script in scratch1.
    masm.unboxObject(
        Address(genObj, AbstractGeneratorObject::offsetOfCalleeSlot()),
        scratch1);
    masm.loadPtr(Address(scratch1, JSFunction::offsetOfScript()), scratch1);

    // Load resume index in scratch2 and mark generator as running.
    Address resumeIndexSlot(genObj,
                            AbstractGeneratorObject::offsetOfResumeIndexSlot());
    masm.unboxInt32(resumeIndexSlot, scratch2);
    masm.storeValue(Int32Value(AbstractGeneratorObject::RESUME_INDEX_RUNNING),
                    resumeIndexSlot);

    if (!emitEnterGeneratorCode(scratch1, scratch2, regs.getAny())) {
      return false;
    }
  } else {
    MOZ_ASSERT(resumeKind == GeneratorResumeKind::Throw ||
               resumeKind == GeneratorResumeKind::Return);

    // Update the frame's frameSize field.
    masm.computeEffectiveAddress(
        Address(BaselineFrameReg, BaselineFrame::FramePointerOffset), scratch2);
    masm.movePtr(scratch2, scratch1);
    masm.subStackPtrFrom(scratch2);
    masm.store32(scratch2, Address(BaselineFrameReg,
                                   BaselineFrame::reverseOffsetOfFrameSize()));
    masm.loadBaselineFramePtr(BaselineFrameReg, scratch2);

    prepareVMCall();
    pushArg(Imm32(int32_t(resumeKind)));
    pushArg(retVal);
    pushArg(genObj);
    pushArg(scratch2);

    using Fn =
        bool (*)(JSContext*, BaselineFrame*, Handle<AbstractGeneratorObject*>,
                 HandleValue, uint32_t);
    TailCallVMFunctionId id =
        TailCallVMFunctionToId<Fn, jit::GeneratorThrowOrReturn>::id;
    TrampolinePtr code = cx->runtime()->jitRuntime()->getVMWrapper(id);
    const VMFunctionData& fun = GetVMFunction(id);

    // Create and push the frame descriptor.
    masm.subStackPtrFrom(scratch1);
    masm.makeFrameDescriptor(scratch1, FrameType::BaselineJS,
                             ExitFrameLayout::Size());
    masm.push(scratch1);

    // We have created a baseline frame as if we were the
    // callee. However, if we just did a regular call at this
    // point, our return address would be bogus: it would point at
    // self-hosted code, instead of the generator code that we are
    // pretending we are already executing. Instead, we push a
    // dummy return address. In jit::GeneratorThrowOrReturn,
    // we will set the baseline frame's overridePc. Frame iterators
    // will use the override pc instead of relying on the return
    // address.

    // On ARM64, the callee will push a bogus return address. On
    // other architectures, we push a null return address.
#ifndef JS_CODEGEN_ARM64
    masm.push(ImmWord(0));
#endif
    masm.jump(code);

    // Pop arguments and frame pointer (pushed by prepareVMCall) from
    // framePushed.
    masm.implicitPop((fun.explicitStackSlots() + 1) * sizeof(void*));
  }

  // Call into the VM to run in the C++ interpreter if there's no TypeScript or
  // BaselineScript.
  masm.bind(&interpret);

  prepareVMCall();
  if (resumeKind == GeneratorResumeKind::Next) {
    pushArg(ImmGCPtr(cx->names().next));
  } else if (resumeKind == GeneratorResumeKind::Throw) {
    pushArg(ImmGCPtr(cx->names().throw_));
  } else {
    MOZ_ASSERT(resumeKind == GeneratorResumeKind::Return);
    pushArg(ImmGCPtr(cx->names().return_));
  }

  masm.loadValue(frame.addressOfStackValue(-1), retVal);
  pushArg(retVal);
  pushArg(genObj);

  using Fn = bool (*)(JSContext*, HandleObject, HandleValue, HandlePropertyName,
                      MutableHandleValue);
  if (!callVM<Fn, jit::InterpretResume>()) {
    return false;
  }

  // After the generator returns, we restore the stack pointer, switch back to
  // the current realm, push the return value, and we're done.
  masm.bind(&returnTarget);
  masm.computeEffectiveAddress(frame.addressOfStackValue(-1),
                               masm.getStackPointer());
  if (JSScript* script = handler.maybeScript()) {
    masm.switchToRealm(script->realm(), R2.scratchReg());
  } else {
    masm.switchToBaselineFrameRealm(R2.scratchReg());
  }
  frame.popn(2);
  frame.push(R0);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_RESUME() {
  auto resumeKind = AbstractGeneratorObject::getResumeKind(handler.pc());
  return emitGeneratorResume(resumeKind);
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_RESUME() {
  Register scratch = R0.scratchReg();

  LoadUint8Operand(masm, PCRegAtStart, scratch);

  Label throw_, return_, done;
  masm.branch32(Assembler::Equal, scratch,
                Imm32(int32_t(GeneratorResumeKind::Throw)), &throw_);
  masm.branch32(Assembler::Equal, scratch,
                Imm32(int32_t(GeneratorResumeKind::Return)), &return_);

#ifdef DEBUG
  Label ok;
  masm.branch32(Assembler::Equal, scratch,
                Imm32(int32_t(GeneratorResumeKind::Next)), &ok);
  masm.assumeUnreachable("JSOP_RESUME invalid ResumeKind");
  masm.bind(&ok);
#endif
  if (!emitGeneratorResume(GeneratorResumeKind::Next)) {
    return false;
  }
  masm.jump(&done);

  masm.bind(&throw_);
  if (!emitGeneratorResume(GeneratorResumeKind::Throw)) {
    return false;
  }
  masm.jump(&done);

  masm.bind(&return_);
  if (!emitGeneratorResume(GeneratorResumeKind::Return)) {
    return false;
  }

  masm.bind(&done);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DEBUGCHECKSELFHOSTED() {
#ifdef DEBUG
  frame.syncStack(0);

  masm.loadValue(frame.addressOfStackValue(-1), R0);

  prepareVMCall();
  pushArg(R0);

  using Fn = bool (*)(JSContext*, HandleValue);
  if (!callVM<Fn, js::Debug_CheckSelfHosted>()) {
    return false;
  }
#endif
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_IS_CONSTRUCTING() {
  frame.push(MagicValue(JS_IS_CONSTRUCTING));
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_JUMPTARGET() {
  MaybeIncrementCodeCoverageCounter(masm, handler.script(), handler.pc());
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_JUMPTARGET() {
  Register scratch1 = R0.scratchReg();
  Register scratch2 = R1.scratchReg();

  Label skipCoverage;
  CodeOffset toggleOffset = masm.toggledJump(&skipCoverage);
  masm.call(handler.codeCoverageAtPCLabel());
  masm.bind(&skipCoverage);
  if (!handler.codeCoverageOffsets().append(toggleOffset.offset())) {
    return false;
  }

  // Load icIndex in scratch1.
  LoadInt32Operand(masm, PCRegAtStart, scratch1);

  // scratch1 := scratch1 * sizeof(ICEntry)
  static_assert(sizeof(ICEntry) == 8 || sizeof(ICEntry) == 16,
                "shift below depends on ICEntry size");
  uint32_t shift = (sizeof(ICEntry) == 16) ? 4 : 3;
  masm.lshiftPtr(Imm32(shift), scratch1);

  // Compute ICEntry* and store to frame->interpreterICEntry.
  loadScript(scratch2);
  masm.loadPtr(Address(scratch2, JSScript::offsetOfTypes()), scratch2);
  masm.loadPtr(Address(scratch2, TypeScript::offsetOfICScript()), scratch2);
  masm.computeEffectiveAddress(
      BaseIndex(scratch2, scratch1, TimesOne, ICScript::offsetOfICEntries()),
      scratch2);
  masm.storePtr(scratch2, frame.addressOfInterpreterICEntry());
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CHECKCLASSHERITAGE() {
  frame.syncStack(0);

  // Leave the heritage value on the stack.
  masm.loadValue(frame.addressOfStackValue(-1), R0);

  prepareVMCall();
  pushArg(R0);

  using Fn = bool (*)(JSContext*, HandleValue);
  return callVM<Fn, js::CheckClassHeritageOperation>();
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_INITHOMEOBJECT() {
  // Load HomeObject in R0.
  frame.popRegsAndSync(1);

  // Load function off stack
  Register func = R2.scratchReg();
  masm.unboxObject(frame.addressOfStackValue(-1), func);

  // Set HOMEOBJECT_SLOT
  Register temp = R1.scratchReg();
  Address addr(func, FunctionExtended::offsetOfMethodHomeObjectSlot());
  masm.guardedCallPreBarrierAnyZone(addr, MIRType::Value, temp);
  masm.storeValue(R0, addr);

  Label skipBarrier;
  masm.branchPtrInNurseryChunk(Assembler::Equal, func, temp, &skipBarrier);
  masm.branchValueIsNurseryObject(Assembler::NotEqual, R0, temp, &skipBarrier);
  masm.call(&postBarrierSlot_);
  masm.bind(&skipBarrier);

  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_BUILTINPROTO() {
  // The builtin prototype is a constant for a given global.
  JSObject* builtin = BuiltinProtoOperation(cx, handler.pc());
  if (!builtin) {
    return false;
  }
  frame.push(ObjectValue(*builtin));
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_BUILTINPROTO() {
  prepareVMCall();

  pushBytecodePCArg();

  using Fn = JSObject* (*)(JSContext*, jsbytecode*);
  if (!callVM<Fn, BuiltinProtoOperation>()) {
    return false;
  }

  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_OBJWITHPROTO() {
  frame.syncStack(0);

  // Leave the proto value on the stack for the decompiler
  masm.loadValue(frame.addressOfStackValue(-1), R0);

  prepareVMCall();
  pushArg(R0);

  using Fn = JSObject* (*)(JSContext*, HandleValue);
  if (!callVM<Fn, js::ObjectWithProtoOperation>()) {
    return false;
  }

  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.pop();
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_FUNWITHPROTO() {
  frame.popRegsAndSync(1);

  masm.unboxObject(R0, R0.scratchReg());
  masm.loadPtr(frame.addressOfEnvironmentChain(), R1.scratchReg());

  prepareVMCall();
  pushArg(R0.scratchReg());
  pushArg(R1.scratchReg());
  pushScriptObjectArg(ScriptObjectType::Function);

  using Fn =
      JSObject* (*)(JSContext*, HandleFunction, HandleObject, HandleObject);
  if (!callVM<Fn, js::FunWithProtoOperation>()) {
    return false;
  }

  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_CLASSCONSTRUCTOR() {
  frame.syncStack(0);

  // Pass nullptr as prototype to MakeDefaultConstructor
  prepareVMCall();
  pushArg(ImmPtr(nullptr));
  pushBytecodePCArg();
  pushScriptArg();

  using Fn =
      JSFunction* (*)(JSContext*, HandleScript, jsbytecode*, HandleObject);
  if (!callVM<Fn, js::MakeDefaultConstructor>()) {
    return false;
  }

  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DERIVEDCONSTRUCTOR() {
  frame.popRegsAndSync(1);

  masm.unboxObject(R0, R0.scratchReg());

  prepareVMCall();
  pushArg(R0.scratchReg());
  pushBytecodePCArg();
  pushScriptArg();

  using Fn =
      JSFunction* (*)(JSContext*, HandleScript, jsbytecode*, HandleObject);
  if (!callVM<Fn, js::MakeDefaultConstructor>()) {
    return false;
  }

  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <>
bool BaselineCompilerCodeGen::emit_JSOP_IMPORTMETA() {
  // Note: this is like the interpreter implementation, but optimized a bit by
  // calling GetModuleObjectForScript at compile-time.

  RootedModuleObject module(cx, GetModuleObjectForScript(handler.script()));
  MOZ_ASSERT(module);

  frame.syncStack(0);

  prepareVMCall();
  pushArg(ImmGCPtr(module));

  using Fn = JSObject* (*)(JSContext*, HandleObject);
  if (!callVM<Fn, js::GetOrCreateModuleMetaObject>()) {
    return false;
  }

  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <>
bool BaselineInterpreterCodeGen::emit_JSOP_IMPORTMETA() {
  prepareVMCall();

  pushScriptArg();

  using Fn = JSObject* (*)(JSContext*, HandleScript);
  if (!callVM<Fn, ImportMetaOperation>()) {
    return false;
  }

  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emit_JSOP_DYNAMIC_IMPORT() {
  // Put specifier value in R0.
  frame.popRegsAndSync(1);

  prepareVMCall();
  pushArg(R0);
  pushScriptArg();

  using Fn = JSObject* (*)(JSContext*, HandleScript, HandleValue);
  if (!callVM<Fn, js::StartDynamicModuleImport>()) {
    return false;
  }

  masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
  frame.push(R0);
  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitPrologue() {
#ifdef JS_USE_LINK_REGISTER
  // Push link register from generateEnterJIT()'s BLR.
  masm.pushReturnAddress();
  masm.checkStackAlignment();
#endif
  emitProfilerEnterFrame();

  masm.push(BaselineFrameReg);
  masm.moveStackPtrTo(BaselineFrameReg);
  masm.subFromStackPtr(Imm32(BaselineFrame::Size()));

  // Initialize BaselineFrame. For eval scripts, the env chain
  // is passed in R1, so we have to be careful not to clobber it.

  // Initialize BaselineFrame::flags and interpreter fields.
  emitInitFrameFields();

  // Handle env chain pre-initialization (in case GC gets run
  // during stack check).  For global and eval scripts, the env
  // chain is in R1.  For function scripts, the env chain is in
  // the callee, nullptr is stored for now so that GC doesn't choke
  // on a bogus EnvironmentChain value in the frame.
  emitPreInitEnvironmentChain(R1.scratchReg());

  if (!emitIncExecutionProgressCounter(R2.scratchReg())) {
    return false;
  }

  // Functions with a large number of locals require two stack checks.
  // The VMCall for a fallible stack check can only occur after the
  // env chain has been initialized, as that is required for proper
  // exception handling if the VMCall returns false.  The env chain
  // initialization can only happen after the UndefinedValues for the
  // local slots have been pushed. However by that time, the stack might
  // have grown too much.
  //
  // In these cases, we emit an extra, early, infallible check before pushing
  // the locals. The early check just sets a flag on the frame if the stack
  // check fails. If the flag is set, then the jitcode skips past the pushing
  // of the locals, and directly to env chain initialization followed by the
  // actual stack check, which will throw the correct exception.
  Label earlyStackCheckFailed;
  if (handler.needsEarlyStackCheck()) {
    // Subtract the size of script->nslots() from the stack pointer.
    Register scratch = R1.scratchReg();
    masm.moveStackPtrTo(scratch);
    subtractScriptSlotsSize(scratch, R2.scratchReg());

    // Set the OVER_RECURSED flag on the frame if the computed stack pointer
    // overflows the stack limit. We have to use the actual (*NoInterrupt)
    // stack limit here because we don't want to set the flag and throw an
    // overrecursion exception later in the interrupt case.
    Label stackCheckOk;
    masm.branchPtr(Assembler::BelowOrEqual,
                   AbsoluteAddress(cx->addressOfJitStackLimitNoInterrupt()),
                   scratch, &stackCheckOk);
    {
      masm.or32(Imm32(BaselineFrame::OVER_RECURSED), frame.addressOfFlags());
      masm.jump(&earlyStackCheckFailed);
    }
    masm.bind(&stackCheckOk);
  }

  emitInitializeLocals();

  if (handler.needsEarlyStackCheck()) {
    masm.bind(&earlyStackCheckFailed);
  }

#ifdef JS_TRACE_LOGGING
  if (JS::TraceLoggerSupported() && !emitTraceLoggerEnter()) {
    return false;
  }
#endif

  // Record the offset of the prologue, because Ion can bailout before
  // the env chain is initialized.
  bailoutPrologueOffset_ = CodeOffset(masm.currentOffset());

  // When compiling with Debugger instrumentation, set the debuggeeness of
  // the frame before any operation that can call into the VM.
  emitIsDebuggeeCheck();

  // Initialize the env chain before any operation that may
  // call into the VM and trigger a GC.
  if (!initEnvironmentChain()) {
    return false;
  }

  frame.assertSyncedStack();

  if (JSScript* script = handler.maybeScript()) {
    masm.debugAssertContextRealm(script->realm(), R1.scratchReg());
  }

  if (!emitStackCheck()) {
    return false;
  }

  if (!emitDebugPrologue()) {
    return false;
  }

  if (!emitHandleCodeCoverageAtPrologue()) {
    return false;
  }

  if (!emitWarmUpCounterIncrement()) {
    return false;
  }

  warmUpCheckPrologueOffset_ = CodeOffset(masm.currentOffset());

  if (!emitArgumentTypeChecks()) {
    return false;
  }

  return true;
}

template <typename Handler>
bool BaselineCodeGen<Handler>::emitEpilogue() {
  // Record the offset of the epilogue, so we can do early return from
  // Debugger handlers during on-stack recompile.
  debugOsrEpilogueOffset_ = CodeOffset(masm.currentOffset());

  masm.bind(&return_);

#ifdef JS_TRACE_LOGGING
  if (JS::TraceLoggerSupported() && !emitTraceLoggerExit()) {
    return false;
  }
#endif

  masm.moveToStackPtr(BaselineFrameReg);
  masm.pop(BaselineFrameReg);

  emitProfilerExitFrame();

  masm.ret();
  return true;
}

MethodStatus BaselineCompiler::emitBody() {
  JSScript* script = handler.script();
  MOZ_ASSERT(handler.pc() == script->code());

  bool lastOpUnreachable = false;
  uint32_t emittedOps = 0;
  mozilla::DebugOnly<jsbytecode*> prevpc = handler.pc();

  while (true) {
    JSOp op = JSOp(*handler.pc());
    JitSpew(JitSpew_BaselineOp, "Compiling op @ %d: %s",
            int(script->pcToOffset(handler.pc())), CodeName[op]);

    BytecodeInfo* info = handler.analysis().maybeInfo(handler.pc());

    // Skip unreachable ops.
    if (!info) {
      // Test if last instructions and stop emitting in that case.
      handler.moveToNextPC();
      if (handler.pc() >= script->codeEnd()) {
        break;
      }

      lastOpUnreachable = true;
      prevpc = handler.pc();
      continue;
    }

    if (info->jumpTarget) {
      // Fully sync the stack if there are incoming jumps.
      frame.syncStack(0);
      frame.setStackDepth(info->stackDepth);
      masm.bind(handler.labelOf(handler.pc()));
    } else if (MOZ_UNLIKELY(compileDebugInstrumentation())) {
      // Also fully sync the stack if the debugger is enabled.
      frame.syncStack(0);
    } else {
      // At the beginning of any op, at most the top 2 stack-values are
      // unsynced.
      if (frame.stackDepth() > 2) {
        frame.syncStack(2);
      }
    }

    frame.assertValidState(*info);

    // Add a PC -> native mapping entry for the current op. These entries are
    // used when we need the native code address for a given pc, for instance
    // for bailouts from Ion, the debugger and exception handling. See
    // PCMappingIndexEntry for more information.
    bool addIndexEntry = (handler.pc() == script->code() || lastOpUnreachable ||
                          emittedOps > 100);
    if (addIndexEntry) {
      emittedOps = 0;
    }
    if (MOZ_UNLIKELY(!addPCMappingEntry(addIndexEntry))) {
      ReportOutOfMemory(cx);
      return Method_Error;
    }

    // Emit traps for breakpoints and step mode.
    if (MOZ_UNLIKELY(compileDebugInstrumentation()) && !emitDebugTrap()) {
      return Method_Error;
    }

    switch (op) {
      case JSOP_FORCEINTERPRETER:
        // Caller must have checked script->hasForceInterpreterOp().
      case JSOP_UNUSED71:
      case JSOP_UNUSED149:
      case JSOP_LIMIT:
        MOZ_CRASH("Unexpected op");

#define EMIT_OP(OP)                                            \
  case OP:                                                     \
    if (MOZ_UNLIKELY(!this->emit_##OP())) return Method_Error; \
    break;
        OPCODE_LIST(EMIT_OP)
#undef EMIT_OP
    }

    MOZ_ASSERT(masm.framePushed() == 0);

    // Test if last instructions and stop emitting in that case.
    handler.moveToNextPC();
    if (handler.pc() >= script->codeEnd()) {
      break;
    }

    emittedOps++;
    lastOpUnreachable = false;
#ifdef DEBUG
    prevpc = handler.pc();
#endif
  }

  MOZ_ASSERT(JSOp(*prevpc) == JSOP_RETRVAL);
  return Method_Compiled;
}

bool BaselineInterpreterGenerator::emitDebugTrap() {
  JitRuntime* jrt = cx->runtime()->jitRuntime();

  JitCode* handlerCode =
      jrt->debugTrapHandler(cx, DebugTrapHandlerKind::Interpreter);
  if (!handlerCode) {
    return false;
  }

  CodeOffset offset = masm.toggledCall(handlerCode, /* enabled = */ false);
  if (!debugTrapOffsets_.append(offset.offset())) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

bool BaselineInterpreterGenerator::emitInterpreterLoop() {
  Register scratch1 = R0.scratchReg();
  Register scratch2 = R1.scratchReg();

  Address pcAddr = frame.addressOfInterpreterPC();

  // Entry point for interpreting a bytecode op. No registers are live. PC is
  // loaded from frame->interpreterPC.
  masm.bind(handler.interpretOpLabel());
  interpretOpOffset_ = masm.currentOffset();

  // Emit a patchable call for debugger breakpoints/stepping. Note: there must
  // be no code between interpretOpOffset_ and this debug trap. EnterBaseline
  // and BaselineCompileFromBaselineInterpreter depend on this.
  if (!emitDebugTrap()) {
    return false;
  }

  // Load pc, bytecode op.
  masm.loadPtr(pcAddr, PCRegAtStart);
  masm.load8ZeroExtend(Address(PCRegAtStart, 0), scratch1);

  // Jump to table[op].
  {
    CodeOffset label = masm.movWithPatch(ImmWord(uintptr_t(-1)), scratch2);
    if (!tableLabels_.append(label)) {
      return false;
    }
    BaseIndex pointer(scratch2, scratch1, ScalePointer);
    masm.branchToComputedAddress(pointer);
  }

  // At the end of each op, emit code to bump the pc and jump to the
  // next op (this is also known as a threaded interpreter).
  auto opEpilogue = [&](JSOp op, size_t opLength) -> bool {
    MOZ_ASSERT(masm.framePushed() == 0);

    if (!BytecodeFallsThrough(op)) {
      // Nothing to do.
      masm.assumeUnreachable("unexpected fall through");
      return true;
    }

    // Bump frame->interpreterICEntry if needed.
    if (BytecodeOpHasIC(op)) {
      frame.bumpInterpreterICEntry();
    }

    // Bump frame->interpreterPC, keep pc in PCRegAtStart.
    masm.loadPtr(pcAddr, PCRegAtStart);
    masm.addPtr(Imm32(opLength), PCRegAtStart);
    masm.storePtr(PCRegAtStart, pcAddr);

    if (!emitDebugTrap()) {
      return false;
    }

    // Load the opcode, jump to table[op].
    masm.load8ZeroExtend(Address(PCRegAtStart, 0), scratch1);
    CodeOffset label = masm.movWithPatch(ImmWord(uintptr_t(-1)), scratch2);
    if (!tableLabels_.append(label)) {
      return false;
    }
    BaseIndex pointer(scratch2, scratch1, ScalePointer);
    masm.branchToComputedAddress(pointer);
    return true;
  };

  // Emit code for each bytecode op.
  Label opLabels[JSOP_LIMIT];
#define EMIT_OP(OP)                     \
  {                                     \
    masm.bind(&opLabels[OP]);           \
    if (!this->emit_##OP()) {           \
      return false;                     \
    }                                   \
    if (!opEpilogue(OP, OP##_LENGTH)) { \
      return false;                     \
    }                                   \
  }
  OPCODE_LIST(EMIT_OP)
#undef EMIT_OP

  // Emit code for JSOP_UNUSED* ops.
  Label invalidOp;
  masm.bind(&invalidOp);
  masm.assumeUnreachable("Invalid op");

  // Emit the table.
  masm.haltingAlign(sizeof(void*));

#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64)
  size_t numInstructions = JSOP_LIMIT * (sizeof(uintptr_t) / sizeof(uint32_t));
  AutoForbidPoolsAndNops afp(&masm, numInstructions);
#endif

  tableOffset_ = masm.currentOffset();

  for (size_t i = 0; i < JSOP_LIMIT; i++) {
    // Store a pointer to the code for the current op. If the op's label is not
    // bound it must be a JSOP_UNUSED* op and we use |invalidOp| instead.
    const Label& opLabel = opLabels[i];
    uint32_t opOffset = opLabel.bound() ? opLabel.offset() : invalidOp.offset();
    CodeLabel cl;
    masm.writeCodePointer(&cl);
    cl.target()->bind(opOffset);
    masm.addCodeLabel(cl);
  }

  return true;
}

void BaselineInterpreterGenerator::emitOutOfLineCodeCoverageInstrumentation() {
  masm.bind(handler.codeCoverageAtPrologueLabel());
#ifdef JS_USE_LINK_REGISTER
  masm.pushReturnAddress();
#endif

  masm.Push(BaselineFrameReg);
  masm.setupUnalignedABICall(R0.scratchReg());
  masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());
  masm.passABIArg(R0.scratchReg());
  masm.callWithABI(
      JS_FUNC_TO_DATA_PTR(void*, jit::HandleCodeCoverageAtPrologue));
  masm.Pop(BaselineFrameReg);

  masm.ret();

  masm.bind(handler.codeCoverageAtPCLabel());
#ifdef JS_USE_LINK_REGISTER
  masm.pushReturnAddress();
#endif

  masm.Push(BaselineFrameReg);
  masm.Push(PCRegAtStart);
  masm.setupUnalignedABICall(R0.scratchReg());
  masm.loadBaselineFramePtr(BaselineFrameReg, R0.scratchReg());
  masm.passABIArg(R0.scratchReg());
  masm.passABIArg(PCRegAtStart);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, jit::HandleCodeCoverageAtPC));
  masm.Pop(PCRegAtStart);
  masm.Pop(BaselineFrameReg);

  masm.ret();
}

bool BaselineInterpreterGenerator::generate(BaselineInterpreter& interpreter) {
  if (!emitPrologue()) {
    return false;
  }

  if (!emitInterpreterLoop()) {
    return false;
  }

  if (!emitEpilogue()) {
    return false;
  }

  if (!emitOutOfLinePostBarrierSlot()) {
    return false;
  }

  emitOutOfLineCodeCoverageInstrumentation();

  {
    Linker linker(masm, "BaselineInterpreter");
    if (masm.oom()) {
      ReportOutOfMemory(cx);
      return false;
    }

    JitCode* code = linker.newCode(cx, CodeKind::Other);
    if (!code) {
      return false;
    }

    // Patch loads now that we know the tableswitch base address.
    for (CodeOffset off : tableLabels_) {
      Assembler::PatchDataWithValueCheck(CodeLocationLabel(code, off),
                                         ImmPtr(code->raw() + tableOffset_),
                                         ImmPtr((void*)-1));
    }

#ifdef JS_ION_PERF
    writePerfSpewerJitCodeProfile(code, "BaselineInterpreter");
#endif

#ifdef MOZ_VTUNE
    vtune::MarkStub(code, "BaselineInterpreter");
#endif

    interpreter.init(
        code, interpretOpOffset_, profilerEnterFrameToggleOffset_.offset(),
        profilerExitFrameToggleOffset_.offset(),
        handler.debuggeeCheckOffset().offset(), std::move(debugTrapOffsets_),
        std::move(handler.codeCoverageOffsets()));
  }

  if (coverage::IsLCovEnabled()) {
    interpreter.toggleCodeCoverageInstrumentationUnchecked(true);
  }

  return true;
}

JitCode* JitRuntime::generateDebugTrapHandler(JSContext* cx,
                                              DebugTrapHandlerKind kind) {
  StackMacroAssembler masm;

  AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
  regs.takeUnchecked(BaselineFrameReg);
  regs.takeUnchecked(ICStubReg);
  regs.takeUnchecked(PCRegAtStart);
#ifdef JS_CODEGEN_ARM
  regs.takeUnchecked(BaselineSecondScratchReg);
  masm.setSecondScratchReg(BaselineSecondScratchReg);
#endif
  Register scratch1 = regs.takeAny();
  Register scratch2 = regs.takeAny();
  Register scratch3 = regs.takeAny();

  if (kind == DebugTrapHandlerKind::Interpreter) {
    // The interpreter calls this for every script when debugging, so check if
    // the script has any breakpoints or is in step mode before calling into
    // C++.
    Label hasDebugScript;
    Address scriptAddr(BaselineFrameReg,
                       BaselineFrame::reverseOffsetOfInterpreterScript());
    masm.loadPtr(scriptAddr, scratch1);
    masm.branchTest32(Assembler::NonZero,
                      Address(scratch1, JSScript::offsetOfMutableFlags()),
                      Imm32(int32_t(JSScript::MutableFlags::HasDebugScript)),
                      &hasDebugScript);
    masm.abiret();
    masm.bind(&hasDebugScript);
  }

  // Load the return address in scratch1.
  masm.loadAbiReturnAddress(scratch1);

  // Load BaselineFrame pointer in scratch2.
  masm.loadBaselineFramePtr(BaselineFrameReg, scratch2);

  // Enter a stub frame and call the HandleDebugTrap VM function. Ensure
  // the stub frame has a nullptr ICStub pointer, since this pointer is marked
  // during GC.
  masm.movePtr(ImmPtr(nullptr), ICStubReg);
  EmitBaselineEnterStubFrame(masm, scratch3);

  using Fn = bool (*)(JSContext*, BaselineFrame*, uint8_t*, bool*);
  VMFunctionId id = VMFunctionToId<Fn, jit::HandleDebugTrap>::id;
  TrampolinePtr code = cx->runtime()->jitRuntime()->getVMWrapper(id);

  masm.push(scratch1);
  masm.push(scratch2);
  EmitBaselineCallVM(code, masm);

  EmitBaselineLeaveStubFrame(masm);

  // If the stub returns |true|, we have to perform a forced return
  // (return from the JS frame). If the stub returns |false|, just return
  // from the trap stub so that execution continues at the current pc.
  Label forcedReturn;
  masm.branchIfTrueBool(ReturnReg, &forcedReturn);

  if (kind == DebugTrapHandlerKind::Interpreter) {
    // We have to reload the bytecode pc register.
    Address pcAddr(BaselineFrameReg,
                   BaselineFrame::reverseOffsetOfInterpreterPC());
    masm.loadPtr(pcAddr, PCRegAtStart);
  }
  masm.abiret();

  masm.bind(&forcedReturn);
  masm.loadValue(
      Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfReturnValue()),
      JSReturnOperand);
  masm.moveToStackPtr(BaselineFrameReg);
  masm.pop(BaselineFrameReg);

  // Before returning, if profiling is turned on, make sure that
  // lastProfilingFrame is set to the correct caller frame.
  {
    Label skipProfilingInstrumentation;
    AbsoluteAddress addressOfEnabled(
        cx->runtime()->geckoProfiler().addressOfEnabled());
    masm.branch32(Assembler::Equal, addressOfEnabled, Imm32(0),
                  &skipProfilingInstrumentation);
    masm.profilerExitFrame();
    masm.bind(&skipProfilingInstrumentation);
  }

  masm.ret();

  Linker linker(masm, "DebugTrapHandler");
  JitCode* handlerCode = linker.newCode(cx, CodeKind::Other);
  if (!handlerCode) {
    return nullptr;
  }

#ifdef JS_ION_PERF
  writePerfSpewerJitCodeProfile(handlerCode, "DebugTrapHandler");
#endif
#ifdef MOZ_VTUNE
  vtune::MarkStub(handlerCode, "DebugTrapHandler");
#endif

  return handlerCode;
}

}  // namespace jit
}  // namespace js
