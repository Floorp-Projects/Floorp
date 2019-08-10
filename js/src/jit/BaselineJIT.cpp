/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineJIT.h"

#include "mozilla/BinarySearch.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"

#include "debugger/DebugAPI.h"
#include "gc/FreeOp.h"
#include "jit/BaselineCodeGen.h"
#include "jit/BaselineIC.h"
#include "jit/CompileInfo.h"
#include "jit/IonControlFlow.h"
#include "jit/JitCommon.h"
#include "jit/JitSpewer.h"
#include "util/StructuredSpewer.h"
#include "vm/Interpreter.h"
#include "vm/TraceLogging.h"

#include "debugger/DebugAPI-inl.h"
#include "gc/PrivateIterators-inl.h"
#include "jit/JitFrames-inl.h"
#include "jit/MacroAssembler-inl.h"
#include "vm/BytecodeUtil-inl.h"
#include "vm/GeckoProfiler-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/Stack-inl.h"

using mozilla::BinarySearchIf;
using mozilla::DebugOnly;

using namespace js;
using namespace js::jit;

void ICStubSpace::freeAllAfterMinorGC(Zone* zone) {
  if (zone->isAtomsZone()) {
    MOZ_ASSERT(allocator_.isEmpty());
  } else {
    JSRuntime* rt = zone->runtimeFromMainThread();
    rt->gc.queueAllLifoBlocksForFreeAfterMinorGC(&allocator_);
  }
}

static bool CheckFrame(InterpreterFrame* fp) {
  if (fp->isDebuggerEvalFrame()) {
    // Debugger eval-in-frame. These are likely short-running scripts so
    // don't bother compiling them for now.
    JitSpew(JitSpew_BaselineAbort, "debugger frame");
    return false;
  }

  if (fp->isFunctionFrame() && fp->numActualArgs() > BASELINE_MAX_ARGS_LENGTH) {
    // Fall back to the interpreter to avoid running out of stack space.
    JitSpew(JitSpew_BaselineAbort, "Too many arguments (%u)",
            fp->numActualArgs());
    return false;
  }

  return true;
}

static JitExecStatus EnterBaseline(JSContext* cx, EnterJitData& data) {
  MOZ_ASSERT(data.osrFrame);

  // Check for potential stack overflow before OSR-ing.
  uint8_t spDummy;
  uint32_t extra =
      BaselineFrame::Size() + (data.osrNumStackValues * sizeof(Value));
  uint8_t* checkSp = (&spDummy) - extra;
  if (!CheckRecursionLimitWithStackPointer(cx, checkSp)) {
    return JitExec_Aborted;
  }

#ifdef DEBUG
  // Assert we don't GC before entering JIT code. A GC could discard JIT code
  // or move the function stored in the CalleeToken (it won't be traced at
  // this point). We use Maybe<> here so we can call reset() to call the
  // AutoAssertNoGC destructor before we enter JIT code.
  mozilla::Maybe<JS::AutoAssertNoGC> nogc;
  nogc.emplace(cx);
#endif

  MOZ_ASSERT(IsBaselineInterpreterEnabled());
  MOZ_ASSERT(CheckFrame(data.osrFrame));

  EnterJitCode enter = cx->runtime()->jitRuntime()->enterJit();

  // Caller must construct |this| before invoking the function.
  MOZ_ASSERT_IF(data.constructing,
                data.maxArgv[0].isObject() ||
                    data.maxArgv[0].isMagic(JS_UNINITIALIZED_LEXICAL));

  data.result.setInt32(data.numActualArgs);
  {
    AssertRealmUnchanged aru(cx);
    ActivationEntryMonitor entryMonitor(cx, data.calleeToken);
    JitActivation activation(cx);

    data.osrFrame->setRunningInJit();

#ifdef DEBUG
    nogc.reset();
#endif
    // Single transition point from Interpreter to Baseline.
    CALL_GENERATED_CODE(enter, data.jitcode, data.maxArgc, data.maxArgv,
                        data.osrFrame, data.calleeToken, data.envChain.get(),
                        data.osrNumStackValues, data.result.address());

    data.osrFrame->clearRunningInJit();
  }

  MOZ_ASSERT(!cx->hasIonReturnOverride());

  // Jit callers wrap primitive constructor return, except for derived
  // class constructors, which are forced to do it themselves.
  if (!data.result.isMagic() && data.constructing &&
      data.result.isPrimitive()) {
    MOZ_ASSERT(data.maxArgv[0].isObject());
    data.result = data.maxArgv[0];
  }

  // Release temporary buffer used for OSR into Ion.
  cx->freeOsrTempData();

  MOZ_ASSERT_IF(data.result.isMagic(), data.result.isMagic(JS_ION_ERROR));
  return data.result.isMagic() ? JitExec_Error : JitExec_Ok;
}

JitExecStatus jit::EnterBaselineInterpreterAtBranch(JSContext* cx,
                                                    InterpreterFrame* fp,
                                                    jsbytecode* pc) {
  MOZ_ASSERT(JSOp(*pc) == JSOP_LOOPENTRY);

  EnterJitData data(cx);

  // Use the entry point that skips the debug trap because the C++ interpreter
  // already handled this for the current op.
  const BaselineInterpreter& interp =
      cx->runtime()->jitRuntime()->baselineInterpreter();
  data.jitcode = interp.interpretOpNoDebugTrapAddr().value;

  // Note: keep this in sync with SetEnterJitData.

  data.osrFrame = fp;
  data.osrNumStackValues =
      fp->script()->nfixed() + cx->interpreterRegs().stackDepth();

  RootedValue newTarget(cx);

  if (fp->isFunctionFrame()) {
    data.constructing = fp->isConstructing();
    data.numActualArgs = fp->numActualArgs();
    data.maxArgc = Max(fp->numActualArgs(), fp->numFormalArgs()) +
                   1;               // +1 = include |this|
    data.maxArgv = fp->argv() - 1;  // -1 = include |this|
    data.envChain = nullptr;
    data.calleeToken = CalleeToToken(&fp->callee(), data.constructing);
  } else {
    data.constructing = false;
    data.numActualArgs = 0;
    data.maxArgc = 0;
    data.maxArgv = nullptr;
    data.envChain = fp->environmentChain();

    data.calleeToken = CalleeToToken(fp->script());

    if (fp->isEvalFrame()) {
      newTarget = fp->newTarget();
      data.maxArgc = 1;
      data.maxArgv = newTarget.address();
    }
  }

  TraceLoggerThread* logger = TraceLoggerForCurrentThread(cx);
  TraceLogStopEvent(logger, TraceLogger_Interpreter);
  TraceLogStartEvent(logger, TraceLogger_Baseline);

  JitExecStatus status = EnterBaseline(cx, data);
  if (status != JitExec_Ok) {
    return status;
  }

  fp->setReturnValue(data.result);
  return JitExec_Ok;
}

MethodStatus jit::BaselineCompile(JSContext* cx, JSScript* script,
                                  bool forceDebugInstrumentation) {
  cx->check(script);
  MOZ_ASSERT(!script->hasBaselineScript());
  MOZ_ASSERT(script->canBaselineCompile());
  MOZ_ASSERT(IsBaselineJitEnabled());
  AutoGeckoProfilerEntry pseudoFrame(
      cx, "Baseline script compilation",
      JS::ProfilingCategoryPair::JS_BaselineCompilation);

  script->ensureNonLazyCanonicalFunction();

  TempAllocator temp(&cx->tempLifoAlloc());
  JitContext jctx(cx, nullptr);

  BaselineCompiler compiler(cx, temp, script);
  if (!compiler.init()) {
    ReportOutOfMemory(cx);
    return Method_Error;
  }

  if (forceDebugInstrumentation) {
    compiler.setCompileDebugInstrumentation();
  }

  MethodStatus status = compiler.compile();

  MOZ_ASSERT_IF(status == Method_Compiled, script->hasBaselineScript());
  MOZ_ASSERT_IF(status != Method_Compiled, !script->hasBaselineScript());

  if (status == Method_CantCompile) {
    script->setBaselineScript(cx->runtime(), BASELINE_DISABLED_SCRIPT);
  }

  return status;
}

static MethodStatus CanEnterBaselineJIT(JSContext* cx, HandleScript script,
                                        AbstractFramePtr osrSourceFrame) {
  // Skip if the script has been disabled.
  if (!script->canBaselineCompile()) {
    return Method_Skipped;
  }

  if (!IsBaselineJitEnabled()) {
    script->setBaselineScript(cx->runtime(), BASELINE_DISABLED_SCRIPT);
    return Method_CantCompile;
  }

  // This check is needed in the following corner case. Consider a function h,
  //
  //   function h(x) {
  //      if (!x)
  //        return;
  //      h(false);
  //      for (var i = 0; i < N; i++)
  //         /* do stuff */
  //   }
  //
  // Suppose h is not yet compiled in baseline and is executing in the
  // interpreter. Let this interpreter frame be f_older. The debugger marks
  // f_older as isDebuggee. At the point of the recursive call h(false), h is
  // compiled in baseline without debug instrumentation, pushing a baseline
  // frame f_newer. The debugger never flags f_newer as isDebuggee, and never
  // recompiles h. When the recursive call returns and execution proceeds to
  // the loop, the interpreter attempts to OSR into baseline. Since h is
  // already compiled in baseline, execution jumps directly into baseline
  // code. This is incorrect as h's baseline script does not have debug
  // instrumentation.
  if (osrSourceFrame && osrSourceFrame.isDebuggee() &&
      !DebugAPI::ensureExecutionObservabilityOfOsrFrame(cx, osrSourceFrame)) {
    return Method_Error;
  }

  if (script->length() > BaselineMaxScriptLength) {
    script->setBaselineScript(cx->runtime(), BASELINE_DISABLED_SCRIPT);
    return Method_CantCompile;
  }

  if (script->nslots() > BaselineMaxScriptSlots) {
    script->setBaselineScript(cx->runtime(), BASELINE_DISABLED_SCRIPT);
    return Method_CantCompile;
  }

  if (script->hasBaselineScript()) {
    return Method_Compiled;
  }

  // Check script warm-up counter.
  if (script->getWarmUpCount() <= JitOptions.baselineJitWarmUpThreshold) {
    return Method_Skipped;
  }

  // Check this before calling ensureJitRealmExists, so we're less
  // likely to report OOM in JSRuntime::createJitRuntime.
  if (!CanLikelyAllocateMoreExecutableMemory()) {
    return Method_Skipped;
  }

  if (!cx->realm()->ensureJitRealmExists(cx)) {
    return Method_Error;
  }

  if (script->hasForceInterpreterOp()) {
    script->setBaselineScript(cx->runtime(), BASELINE_DISABLED_SCRIPT);
    return Method_CantCompile;
  }

  // Frames can be marked as debuggee frames independently of its underlying
  // script being a debuggee script, e.g., when performing
  // Debugger.Frame.prototype.eval.
  bool forceDebugInstrumentation =
      osrSourceFrame && osrSourceFrame.isDebuggee();
  return BaselineCompile(cx, script, forceDebugInstrumentation);
}

bool jit::CanBaselineInterpretScript(JSScript* script) {
  MOZ_ASSERT(IsBaselineInterpreterEnabled());

  if (script->hasForceInterpreterOp()) {
    return false;
  }

  if (script->nslots() > BaselineMaxScriptSlots) {
    // Avoid overrecursion exceptions when the script has a ton of stack slots
    // by forcing such scripts to run in the C++ interpreter with heap-allocated
    // stack frames.
    return false;
  }

  return true;
}

static MethodStatus CanEnterBaselineInterpreter(JSContext* cx,
                                                JSScript* script) {
  MOZ_ASSERT(IsBaselineInterpreterEnabled());

  if (script->jitScript()) {
    return Method_Compiled;
  }

  if (!CanBaselineInterpretScript(script)) {
    return Method_CantCompile;
  }

  // Check script warm-up counter.
  if (script->getWarmUpCount() <=
      JitOptions.baselineInterpreterWarmUpThreshold) {
    return Method_Skipped;
  }

  if (!cx->realm()->ensureJitRealmExists(cx)) {
    return Method_Error;
  }

  AutoKeepJitScripts keepJitScript(cx);
  if (!script->ensureHasJitScript(cx, keepJitScript)) {
    return Method_Error;
  }

  return Method_Compiled;
}

MethodStatus jit::CanEnterBaselineInterpreterAtBranch(JSContext* cx,
                                                      InterpreterFrame* fp) {
  if (!CheckFrame(fp)) {
    return Method_CantCompile;
  }

  return CanEnterBaselineInterpreter(cx, fp->script());
}

template <BaselineTier Tier>
MethodStatus jit::CanEnterBaselineMethod(JSContext* cx, RunState& state) {
  if (state.isInvoke()) {
    InvokeState& invoke = *state.asInvoke();
    if (invoke.args().length() > BASELINE_MAX_ARGS_LENGTH) {
      JitSpew(JitSpew_BaselineAbort, "Too many arguments (%u)",
              invoke.args().length());
      return Method_CantCompile;
    }
  } else {
    if (state.asExecute()->isDebuggerEval()) {
      JitSpew(JitSpew_BaselineAbort, "debugger frame");
      return Method_CantCompile;
    }
  }

  RootedScript script(cx, state.script());
  switch (Tier) {
    case BaselineTier::Interpreter:
      return CanEnterBaselineInterpreter(cx, script);

    case BaselineTier::Compiler:
      return CanEnterBaselineJIT(cx, script,
                                 /* osrSourceFrame = */ NullFramePtr());
  }

  MOZ_CRASH("Unexpected tier");
}

template MethodStatus jit::CanEnterBaselineMethod<BaselineTier::Interpreter>(
    JSContext* cx, RunState& state);
template MethodStatus jit::CanEnterBaselineMethod<BaselineTier::Compiler>(
    JSContext* cx, RunState& state);

bool jit::BaselineCompileFromBaselineInterpreter(JSContext* cx,
                                                 BaselineFrame* frame,
                                                 uint8_t** res) {
  MOZ_ASSERT(frame->runningInInterpreter());

  RootedScript script(cx, frame->script());
  jsbytecode* pc = frame->interpreterPC();
  MOZ_ASSERT(pc == script->code() || *pc == JSOP_LOOPENTRY);

  MethodStatus status = CanEnterBaselineJIT(cx, script,
                                            /* osrSourceFrame = */ frame);
  switch (status) {
    case Method_Error:
      return false;

    case Method_CantCompile:
    case Method_Skipped:
      *res = nullptr;
      return true;

    case Method_Compiled: {
      if (*pc == JSOP_LOOPENTRY) {
        PCMappingSlotInfo slotInfo;
        BaselineScript* baselineScript = script->baselineScript();
        *res = baselineScript->nativeCodeForPC(script, pc, &slotInfo);
        MOZ_ASSERT(slotInfo.isStackSynced());
        if (frame->isDebuggee()) {
          // Skip the debug trap emitted by emitInterpreterLoop because the
          // Baseline Interpreter already handled it for the current op.
          MOZ_RELEASE_ASSERT(baselineScript->hasDebugInstrumentation());
          *res += MacroAssembler::ToggledCallSize(*res);
        }
      } else {
        *res = script->baselineScript()->warmUpCheckPrologueAddr();
      }
      frame->prepareForBaselineInterpreterToJitOSR();
      return true;
    }
  }

  MOZ_CRASH("Unexpected status");
}

BaselineScript* BaselineScript::New(
    JSScript* jsscript, uint32_t warmUpCheckPrologueOffset,
    uint32_t profilerEnterToggleOffset, uint32_t profilerExitToggleOffset,
    size_t retAddrEntries, size_t pcMappingIndexEntries, size_t pcMappingSize,
    size_t resumeEntries, size_t traceLoggerToggleOffsetEntries) {
  static const unsigned DataAlignment = sizeof(uintptr_t);

  size_t retAddrEntriesSize = retAddrEntries * sizeof(RetAddrEntry);
  size_t pcMappingIndexEntriesSize =
      pcMappingIndexEntries * sizeof(PCMappingIndexEntry);
  size_t resumeEntriesSize = resumeEntries * sizeof(uintptr_t);
  size_t tlEntriesSize = traceLoggerToggleOffsetEntries * sizeof(uint32_t);

  size_t paddedRetAddrEntriesSize =
      AlignBytes(retAddrEntriesSize, DataAlignment);
  size_t paddedPCMappingIndexEntriesSize =
      AlignBytes(pcMappingIndexEntriesSize, DataAlignment);
  size_t paddedPCMappingSize = AlignBytes(pcMappingSize, DataAlignment);
  size_t paddedResumeEntriesSize = AlignBytes(resumeEntriesSize, DataAlignment);
  size_t paddedTLEntriesSize = AlignBytes(tlEntriesSize, DataAlignment);

  size_t allocBytes = paddedRetAddrEntriesSize +
                      paddedPCMappingIndexEntriesSize + paddedPCMappingSize +
                      paddedResumeEntriesSize + paddedTLEntriesSize;

  BaselineScript* script =
      jsscript->zone()->pod_malloc_with_extra<BaselineScript, uint8_t>(
          allocBytes);
  if (!script) {
    return nullptr;
  }
  new (script)
      BaselineScript(warmUpCheckPrologueOffset, profilerEnterToggleOffset,
                     profilerExitToggleOffset);

  size_t offsetCursor = sizeof(BaselineScript);
  MOZ_ASSERT(offsetCursor == AlignBytes(sizeof(BaselineScript), DataAlignment));

  script->retAddrEntriesOffset_ = offsetCursor;
  script->retAddrEntries_ = retAddrEntries;
  offsetCursor += paddedRetAddrEntriesSize;

  script->pcMappingIndexOffset_ = offsetCursor;
  script->pcMappingIndexEntries_ = pcMappingIndexEntries;
  offsetCursor += paddedPCMappingIndexEntriesSize;

  script->pcMappingOffset_ = offsetCursor;
  script->pcMappingSize_ = pcMappingSize;
  offsetCursor += paddedPCMappingSize;

  script->resumeEntriesOffset_ = resumeEntries ? offsetCursor : 0;
  offsetCursor += paddedResumeEntriesSize;

  script->traceLoggerToggleOffsetsOffset_ = tlEntriesSize ? offsetCursor : 0;
  script->numTraceLoggerToggleOffsets_ = traceLoggerToggleOffsetEntries;
  offsetCursor += paddedTLEntriesSize;

  MOZ_ASSERT(offsetCursor == sizeof(BaselineScript) + allocBytes);
  script->allocBytes_ = allocBytes;

  return script;
}

void BaselineScript::trace(JSTracer* trc) {
  TraceEdge(trc, &method_, "baseline-method");
}

/* static */
void BaselineScript::writeBarrierPre(Zone* zone, BaselineScript* script) {
  if (zone->needsIncrementalBarrier()) {
    script->trace(zone->barrierTracer());
  }
}

void BaselineScript::Trace(JSTracer* trc, BaselineScript* script) {
  script->trace(trc);
}

void BaselineScript::Destroy(FreeOp* fop, BaselineScript* script) {
  MOZ_ASSERT(!script->hasPendingIonBuilder());

  // This allocation is tracked by JSScript::setBaselineScript /
  // clearBaselineScript.
  fop->deleteUntracked(script);
}

void JS::DeletePolicy<js::jit::BaselineScript>::operator()(
    const js::jit::BaselineScript* script) {
  BaselineScript::Destroy(rt_->defaultFreeOp(),
                          const_cast<BaselineScript*>(script));
}

PCMappingIndexEntry& BaselineScript::pcMappingIndexEntry(size_t index) {
  MOZ_ASSERT(index < numPCMappingIndexEntries());
  return pcMappingIndexEntryList()[index];
}

CompactBufferReader BaselineScript::pcMappingReader(size_t indexEntry) {
  PCMappingIndexEntry& entry = pcMappingIndexEntry(indexEntry);

  uint8_t* dataStart = pcMappingData() + entry.bufferOffset;
  uint8_t* dataEnd =
      (indexEntry == numPCMappingIndexEntries() - 1)
          ? pcMappingData() + pcMappingSize_
          : pcMappingData() + pcMappingIndexEntry(indexEntry + 1).bufferOffset;

  return CompactBufferReader(dataStart, dataEnd);
}

const RetAddrEntry& BaselineScript::retAddrEntryFromReturnOffset(
    CodeOffset returnOffset) {
  mozilla::Span<RetAddrEntry> entries = retAddrEntries();
  size_t loc;
#ifdef DEBUG
  bool found =
#endif
      BinarySearchIf(
          entries.data(), 0, entries.size(),
          [&returnOffset](const RetAddrEntry& entry) {
            size_t roffset = returnOffset.offset();
            size_t entryRoffset = entry.returnOffset().offset();
            if (roffset < entryRoffset) {
              return -1;
            }
            if (entryRoffset < roffset) {
              return 1;
            }
            return 0;
          },
          &loc);

  MOZ_ASSERT(found);
  MOZ_ASSERT(entries[loc].returnOffset().offset() == returnOffset.offset());
  return entries[loc];
}

static bool ComputeBinarySearchMid(mozilla::Span<RetAddrEntry> entries,
                                   uint32_t pcOffset, size_t* loc) {
  return BinarySearchIf(
      entries.data(), 0, entries.size(),
      [pcOffset](const RetAddrEntry& entry) {
        uint32_t entryOffset = entry.pcOffset();
        if (pcOffset < entryOffset) {
          return -1;
        }
        if (entryOffset < pcOffset) {
          return 1;
        }
        return 0;
      },
      loc);
}

uint8_t* BaselineScript::returnAddressForEntry(const RetAddrEntry& ent) {
  return method()->raw() + ent.returnOffset().offset();
}

const RetAddrEntry& BaselineScript::retAddrEntryFromPCOffset(
    uint32_t pcOffset, RetAddrEntry::Kind kind) {
  mozilla::Span<RetAddrEntry> entries = retAddrEntries();
  size_t mid;
  MOZ_ALWAYS_TRUE(ComputeBinarySearchMid(entries, pcOffset, &mid));
  MOZ_ASSERT(mid < entries.size());

  // Search for the first entry for this pc.
  size_t first = mid;
  while (first > 0 && entries[first - 1].pcOffset() == pcOffset) {
    first--;
  }

  // Search for the last entry for this pc.
  size_t last = mid;
  while (last + 1 < entries.size() &&
         entries[last + 1].pcOffset() == pcOffset) {
    last++;
  }

  MOZ_ASSERT(first <= last);
  MOZ_ASSERT(entries[first].pcOffset() == pcOffset);
  MOZ_ASSERT(entries[last].pcOffset() == pcOffset);

  for (size_t i = first; i <= last; i++) {
    const RetAddrEntry& entry = entries[i];
    if (entry.kind() != kind) {
      continue;
    }

#ifdef DEBUG
    // There must be a unique entry for this pcOffset and Kind to ensure our
    // return value is well-defined.
    for (size_t j = i + 1; j <= last; j++) {
      MOZ_ASSERT(entries[j].kind() != kind);
    }
#endif

    return entry;
  }

  MOZ_CRASH("Didn't find RetAddrEntry.");
}

const RetAddrEntry& BaselineScript::prologueRetAddrEntry(
    RetAddrEntry::Kind kind) {
  MOZ_ASSERT(kind == RetAddrEntry::Kind::StackCheck ||
             kind == RetAddrEntry::Kind::WarmupCounter);

  // The prologue entries will always be at a very low offset, so just do a
  // linear search from the beginning.
  for (const RetAddrEntry& entry : retAddrEntries()) {
    if (entry.pcOffset() != 0) {
      break;
    }
    if (entry.kind() == kind) {
      return entry;
    }
  }
  MOZ_CRASH("Didn't find prologue RetAddrEntry.");
}

const RetAddrEntry& BaselineScript::retAddrEntryFromReturnAddress(
    uint8_t* returnAddr) {
  MOZ_ASSERT(returnAddr > method_->raw());
  MOZ_ASSERT(returnAddr < method_->raw() + method_->instructionsSize());
  CodeOffset offset(returnAddr - method_->raw());
  return retAddrEntryFromReturnOffset(offset);
}

void BaselineScript::computeResumeNativeOffsets(JSScript* script) {
  // Translate pcOffset to BaselineScript native address. This may return
  // nullptr if compiler decided code was unreachable.
  auto computeNative = [this, script](uint32_t pcOffset) {
    PCMappingSlotInfo slotInfo;
    uint8_t* nativeCode =
        maybeNativeCodeForPC(script, script->offsetToPC(pcOffset), &slotInfo);
    MOZ_ASSERT(slotInfo.isStackSynced());

    return nativeCode;
  };

  mozilla::Span<const uint32_t> pcOffsets = script->resumeOffsets();
  uint8_t** nativeOffsets = resumeEntryList();
  std::transform(pcOffsets.begin(), pcOffsets.end(), nativeOffsets,
                 computeNative);
}

void BaselineScript::copyRetAddrEntries(const RetAddrEntry* entries) {
  std::copy_n(entries, retAddrEntries().size(), retAddrEntries().data());
}

void BaselineScript::copyPCMappingEntries(const CompactBufferWriter& entries) {
  MOZ_ASSERT(entries.length() > 0);
  MOZ_ASSERT(entries.length() == pcMappingSize_);

  memcpy(pcMappingData(), entries.buffer(), entries.length());
}

void BaselineScript::copyPCMappingIndexEntries(
    const PCMappingIndexEntry* entries) {
  for (uint32_t i = 0; i < numPCMappingIndexEntries(); i++) {
    pcMappingIndexEntry(i) = entries[i];
  }
}

uint8_t* BaselineScript::maybeNativeCodeForPC(JSScript* script, jsbytecode* pc,
                                              PCMappingSlotInfo* slotInfo) {
  MOZ_ASSERT_IF(script->hasBaselineScript(), script->baselineScript() == this);

  uint32_t pcOffset = script->pcToOffset(pc);

  // Find PCMappingIndexEntry containing pc. They are in ascedending order
  // with the start of one entry being the end of the previous entry. Find
  // first entry where pcOffset < endOffset.
  uint32_t i = 0;
  for (; (i + 1) < numPCMappingIndexEntries(); i++) {
    uint32_t endOffset = pcMappingIndexEntry(i + 1).pcOffset;
    if (pcOffset < endOffset) {
      break;
    }
  }

  PCMappingIndexEntry& entry = pcMappingIndexEntry(i);
  MOZ_ASSERT(pcOffset >= entry.pcOffset);

  CompactBufferReader reader(pcMappingReader(i));
  MOZ_ASSERT(reader.more());

  jsbytecode* curPC = script->offsetToPC(entry.pcOffset);
  uint32_t curNativeOffset = entry.nativeOffset;
  MOZ_ASSERT(script->containsPC(curPC));

  while (reader.more()) {
    // If the high bit is set, the native offset relative to the
    // previous pc != 0 and comes next.
    uint8_t b = reader.readByte();
    if (b & 0x80) {
      curNativeOffset += reader.readUnsigned();
    }

    if (curPC == pc) {
      *slotInfo = PCMappingSlotInfo(b & 0x7F);
      return method_->raw() + curNativeOffset;
    }

    curPC += GetBytecodeLength(curPC);
  }

  // Code was not generated for this PC because BaselineCompiler believes it
  // is unreachable.
  return nullptr;
}

jsbytecode* BaselineScript::approximatePcForNativeAddress(
    JSScript* script, uint8_t* nativeAddress) {
  MOZ_ASSERT(script->baselineScript() == this);
  MOZ_ASSERT(containsCodeAddress(nativeAddress));

  uint32_t nativeOffset = nativeAddress - method_->raw();

  // The native code address can occur before the start of ops. Associate
  // those with start of bytecode.
  if (nativeOffset < pcMappingIndexEntry(0).nativeOffset) {
    return script->code();
  }

  // Find corresponding PCMappingIndexEntry for native offset. They are in
  // ascedending order with the start of one entry being the end of the
  // previous entry. Find first entry where nativeOffset < endOffset.
  uint32_t i = 0;
  for (; (i + 1) < numPCMappingIndexEntries(); i++) {
    uint32_t endOffset = pcMappingIndexEntry(i + 1).nativeOffset;
    if (nativeOffset < endOffset) {
      break;
    }
  }

  PCMappingIndexEntry& entry = pcMappingIndexEntry(i);
  MOZ_ASSERT(nativeOffset >= entry.nativeOffset);

  CompactBufferReader reader(pcMappingReader(i));
  MOZ_ASSERT(reader.more());

  jsbytecode* curPC = script->offsetToPC(entry.pcOffset);
  uint32_t curNativeOffset = entry.nativeOffset;
  MOZ_ASSERT(script->containsPC(curPC));

  jsbytecode* lastPC = curPC;
  while (reader.more()) {
    // If the high bit is set, the native offset relative to the
    // previous pc != 0 and comes next.
    uint8_t b = reader.readByte();
    if (b & 0x80) {
      curNativeOffset += reader.readUnsigned();
    }

    // Return the last PC that matched nativeOffset. Some bytecode
    // generate no native code (e.g., constant-pushing bytecode like
    // JSOP_INT8), and so their entries share the same nativeOffset as the
    // next op that does generate code.
    if (curNativeOffset > nativeOffset) {
      return lastPC;
    }

    lastPC = curPC;
    curPC += GetBytecodeLength(curPC);
  }

  // Associate all addresses at end of PCMappingIndexEntry with lastPC.
  return lastPC;
}

void BaselineScript::toggleDebugTraps(JSScript* script, jsbytecode* pc) {
  MOZ_ASSERT(script->baselineScript() == this);

  // Only scripts compiled for debug mode have toggled calls.
  if (!hasDebugInstrumentation()) {
    return;
  }

  AutoWritableJitCode awjc(method());

  for (uint32_t i = 0; i < numPCMappingIndexEntries(); i++) {
    PCMappingIndexEntry& entry = pcMappingIndexEntry(i);

    CompactBufferReader reader(pcMappingReader(i));
    jsbytecode* curPC = script->offsetToPC(entry.pcOffset);
    uint32_t nativeOffset = entry.nativeOffset;

    MOZ_ASSERT(script->containsPC(curPC));

    while (reader.more()) {
      uint8_t b = reader.readByte();
      if (b & 0x80) {
        nativeOffset += reader.readUnsigned();
      }

      if (!pc || pc == curPC) {
        bool enabled = DebugAPI::stepModeEnabled(script) ||
                       DebugAPI::hasBreakpointsAt(script, curPC);

        // Patch the trap.
        CodeLocationLabel label(method(), CodeOffset(nativeOffset));
        Assembler::ToggleCall(label, enabled);
      }

      curPC += GetBytecodeLength(curPC);
    }
  }
}

#ifdef JS_TRACE_LOGGING
void BaselineScript::initTraceLogger(JSScript* script,
                                     const Vector<CodeOffset>& offsets) {
#  ifdef DEBUG
  traceLoggerScriptsEnabled_ = TraceLogTextIdEnabled(TraceLogger_Scripts);
  traceLoggerEngineEnabled_ = TraceLogTextIdEnabled(TraceLogger_Engine);
#  endif

  mozilla::Span<uint32_t> scriptOffsets = traceLoggerToggleOffsets();

  MOZ_ASSERT(offsets.length() == scriptOffsets.size());

  for (size_t i = 0; i < offsets.length(); i++) {
    scriptOffsets[i] = offsets[i].offset();
  }

  if (TraceLogTextIdEnabled(TraceLogger_Engine) ||
      TraceLogTextIdEnabled(TraceLogger_Scripts)) {
    traceLoggerScriptEvent_ = TraceLoggerEvent(TraceLogger_Scripts, script);
    for (uint32_t offset : scriptOffsets) {
      CodeLocationLabel label(method_, CodeOffset(offset));
      Assembler::ToggleToCmp(label);
    }
  }
}

void BaselineScript::toggleTraceLoggerScripts(JSScript* script, bool enable) {
  DebugOnly<bool> engineEnabled = TraceLogTextIdEnabled(TraceLogger_Engine);
  MOZ_ASSERT(enable == !traceLoggerScriptsEnabled_);
  MOZ_ASSERT(engineEnabled == traceLoggerEngineEnabled_);

  // Patch the logging script textId to be correct.
  // When logging log the specific textId else the global Scripts textId.
  if (enable && !traceLoggerScriptEvent_.hasTextId()) {
    traceLoggerScriptEvent_ = TraceLoggerEvent(TraceLogger_Scripts, script);
  }

  AutoWritableJitCode awjc(method());

  // Enable/Disable the traceLogger.
  for (uint32_t offset : traceLoggerToggleOffsets()) {
    CodeLocationLabel label(method_, CodeOffset(offset));
    if (enable) {
      Assembler::ToggleToCmp(label);
    } else {
      Assembler::ToggleToJmp(label);
    }
  }

#  if DEBUG
  traceLoggerScriptsEnabled_ = enable;
#  endif
}

void BaselineScript::toggleTraceLoggerEngine(bool enable) {
  DebugOnly<bool> scriptsEnabled = TraceLogTextIdEnabled(TraceLogger_Scripts);
  MOZ_ASSERT(enable == !traceLoggerEngineEnabled_);
  MOZ_ASSERT(scriptsEnabled == traceLoggerScriptsEnabled_);

  AutoWritableJitCode awjc(method());

  // Enable/Disable the traceLogger prologue and epilogue.
  for (size_t i = 0; i < numTraceLoggerToggleOffsets_; i++) {
    CodeLocationLabel label(method_, CodeOffset(traceLoggerToggleOffsets()[i]));
    if (enable) {
      Assembler::ToggleToCmp(label);
    } else {
      Assembler::ToggleToJmp(label);
    }
  }

#  if DEBUG
  traceLoggerEngineEnabled_ = enable;
#  endif
}
#endif

static void ToggleProfilerInstrumentation(JitCode* code,
                                          uint32_t profilerEnterToggleOffset,
                                          uint32_t profilerExitToggleOffset,
                                          bool enable) {
  CodeLocationLabel enterToggleLocation(code,
                                        CodeOffset(profilerEnterToggleOffset));
  CodeLocationLabel exitToggleLocation(code,
                                       CodeOffset(profilerExitToggleOffset));
  if (enable) {
    Assembler::ToggleToCmp(enterToggleLocation);
    Assembler::ToggleToCmp(exitToggleLocation);
  } else {
    Assembler::ToggleToJmp(enterToggleLocation);
    Assembler::ToggleToJmp(exitToggleLocation);
  }
}

void BaselineScript::toggleProfilerInstrumentation(bool enable) {
  if (enable == isProfilerInstrumentationOn()) {
    return;
  }

  JitSpew(JitSpew_BaselineIC, "  toggling profiling %s for BaselineScript %p",
          enable ? "on" : "off", this);

  ToggleProfilerInstrumentation(method_, profilerEnterToggleOffset_,
                                profilerExitToggleOffset_, enable);

  if (enable) {
    flags_ |= uint32_t(PROFILER_INSTRUMENTATION_ON);
  } else {
    flags_ &= ~uint32_t(PROFILER_INSTRUMENTATION_ON);
  }
}

void BaselineInterpreter::toggleProfilerInstrumentation(bool enable) {
  if (!IsBaselineInterpreterEnabled()) {
    return;
  }

  AutoWritableJitCode awjc(code_);
  ToggleProfilerInstrumentation(code_, profilerEnterToggleOffset_,
                                profilerExitToggleOffset_, enable);
}

void BaselineInterpreter::toggleDebuggerInstrumentation(bool enable) {
  if (!IsBaselineInterpreterEnabled()) {
    return;
  }

  AutoWritableJitCode awjc(code_);

  // Toggle jumps for debugger instrumentation.
  for (uint32_t offset : debugInstrumentationOffsets_) {
    CodeLocationLabel label(code_, CodeOffset(offset));
    if (enable) {
      Assembler::ToggleToCmp(label);
    } else {
      Assembler::ToggleToJmp(label);
    }
  }

  // Toggle DebugTrapHandler calls.
  for (uint32_t offset : debugTrapOffsets_) {
    CodeLocationLabel trapLocation(code_, CodeOffset(offset));
    Assembler::ToggleCall(trapLocation, enable);
  }
}

void BaselineInterpreter::toggleCodeCoverageInstrumentationUnchecked(
    bool enable) {
  if (!IsBaselineInterpreterEnabled()) {
    return;
  }

  AutoWritableJitCode awjc(code_);

  for (uint32_t offset : codeCoverageOffsets_) {
    CodeLocationLabel label(code_, CodeOffset(offset));
    if (enable) {
      Assembler::ToggleToCmp(label);
    } else {
      Assembler::ToggleToJmp(label);
    }
  }
}

void BaselineInterpreter::toggleCodeCoverageInstrumentation(bool enable) {
  if (coverage::IsLCovEnabled()) {
    // Instrumentation is enabled no matter what.
    return;
  }

  toggleCodeCoverageInstrumentationUnchecked(enable);
}

void jit::FinishDiscardBaselineScript(FreeOp* fop, JSScript* script) {
  MOZ_ASSERT(script->hasBaselineScript());
  MOZ_ASSERT(!script->jitScript()->active());

  BaselineScript* baseline = script->baselineScript();
  script->setBaselineScript(fop, nullptr);
  BaselineScript::Destroy(fop, baseline);
}

void jit::AddSizeOfBaselineData(JSScript* script,
                                mozilla::MallocSizeOf mallocSizeOf,
                                size_t* data) {
  if (script->hasBaselineScript()) {
    script->baselineScript()->addSizeOfIncludingThis(mallocSizeOf, data);
  }
}

void jit::ToggleBaselineProfiling(JSContext* cx, bool enable) {
  JitRuntime* jrt = cx->runtime()->jitRuntime();
  if (!jrt) {
    return;
  }

  jrt->baselineInterpreter().toggleProfilerInstrumentation(enable);

  for (ZonesIter zone(cx->runtime(), SkipAtoms); !zone.done(); zone.next()) {
    for (auto script = zone->cellIter<JSScript>(); !script.done();
         script.next()) {
      if (enable) {
        if (JitScript* jitScript = script->jitScript()) {
          jitScript->ensureProfileString(cx, script);
        }
      }
      if (!script->hasBaselineScript()) {
        continue;
      }
      AutoWritableJitCode awjc(script->baselineScript()->method());
      script->baselineScript()->toggleProfilerInstrumentation(enable);
    }
  }
}

#ifdef JS_TRACE_LOGGING
void jit::ToggleBaselineTraceLoggerScripts(JSRuntime* runtime, bool enable) {
  for (ZonesIter zone(runtime, SkipAtoms); !zone.done(); zone.next()) {
    for (auto iter = zone->cellIter<JSScript>(); !iter.done(); iter.next()) {
      JSScript* script = iter;
      if (gc::IsAboutToBeFinalizedUnbarriered(&script)) {
        continue;
      }
      if (!script->hasBaselineScript()) {
        continue;
      }
      script->baselineScript()->toggleTraceLoggerScripts(script, enable);
    }
  }
}

void jit::ToggleBaselineTraceLoggerEngine(JSRuntime* runtime, bool enable) {
  for (ZonesIter zone(runtime, SkipAtoms); !zone.done(); zone.next()) {
    for (auto iter = zone->cellIter<JSScript>(); !iter.done(); iter.next()) {
      JSScript* script = iter;
      if (gc::IsAboutToBeFinalizedUnbarriered(&script)) {
        continue;
      }
      if (!script->hasBaselineScript()) {
        continue;
      }
      script->baselineScript()->toggleTraceLoggerEngine(enable);
    }
  }
}
#endif

void BaselineInterpreter::init(JitCode* code, uint32_t interpretOpOffset,
                               uint32_t interpretOpNoDebugTrapOffset,
                               uint32_t bailoutPrologueOffset,
                               uint32_t generatorThrowOrReturnCallOffset,
                               uint32_t profilerEnterToggleOffset,
                               uint32_t profilerExitToggleOffset,
                               CodeOffsetVector&& debugInstrumentationOffsets,
                               CodeOffsetVector&& debugTrapOffsets,
                               CodeOffsetVector&& codeCoverageOffsets,
                               ICReturnOffsetVector&& icReturnOffsets,
                               const CallVMOffsets& callVMOffsets) {
  code_ = code;
  interpretOpOffset_ = interpretOpOffset;
  interpretOpNoDebugTrapOffset_ = interpretOpNoDebugTrapOffset;
  bailoutPrologueOffset_ = bailoutPrologueOffset;
  generatorThrowOrReturnCallOffset_ = generatorThrowOrReturnCallOffset;
  profilerEnterToggleOffset_ = profilerEnterToggleOffset;
  profilerExitToggleOffset_ = profilerExitToggleOffset;
  debugInstrumentationOffsets_ = std::move(debugInstrumentationOffsets);
  debugTrapOffsets_ = std::move(debugTrapOffsets);
  codeCoverageOffsets_ = std::move(codeCoverageOffsets);
  icReturnOffsets_ = std::move(icReturnOffsets);
  callVMOffsets_ = callVMOffsets;
}

uint8_t* BaselineInterpreter::retAddrForIC(JSOp op) const {
  for (const ICReturnOffset& entry : icReturnOffsets_) {
    if (entry.op == op) {
      return codeAtOffset(entry.offset);
    }
  }
  MOZ_CRASH("Unexpected op");
}

bool jit::GenerateBaselineInterpreter(JSContext* cx,
                                      BaselineInterpreter& interpreter) {
  // Temporary IsBaselineInterpreterEnabled check to not generate the
  // interpreter code (until it's enabled by default).
  if (IsBaselineInterpreterEnabled()) {
    BaselineInterpreterGenerator generator(cx);
    return generator.generate(interpreter);
  }

  return true;
}
