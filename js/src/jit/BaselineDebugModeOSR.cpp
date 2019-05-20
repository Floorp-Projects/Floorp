/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineDebugModeOSR.h"

#include "jit/BaselineIC.h"
#include "jit/JitcodeMap.h"
#include "jit/Linker.h"
#include "jit/PerfSpewer.h"

#include "jit/JitFrames-inl.h"
#include "jit/MacroAssembler-inl.h"
#include "vm/Stack-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;
using namespace js::jit;

struct DebugModeOSREntry {
  JSScript* script;
  BaselineScript* oldBaselineScript;
  BaselineDebugModeOSRInfo* recompInfo;
  uint32_t pcOffset;
  RetAddrEntry::Kind frameKind;

  explicit DebugModeOSREntry(JSScript* script)
      : script(script),
        oldBaselineScript(script->baselineScript()),
        recompInfo(nullptr),
        pcOffset(uint32_t(-1)),
        frameKind(RetAddrEntry::Kind::Invalid) {}

  DebugModeOSREntry(JSScript* script, uint32_t pcOffset)
      : script(script),
        oldBaselineScript(script->baselineScript()),
        recompInfo(nullptr),
        pcOffset(pcOffset),
        frameKind(RetAddrEntry::Kind::Invalid) {}

  DebugModeOSREntry(JSScript* script, const RetAddrEntry& retAddrEntry)
      : script(script),
        oldBaselineScript(script->baselineScript()),
        recompInfo(nullptr),
        pcOffset(retAddrEntry.pcOffset()),
        frameKind(retAddrEntry.kind()) {
#ifdef DEBUG
    MOZ_ASSERT(pcOffset == retAddrEntry.pcOffset());
    MOZ_ASSERT(frameKind == retAddrEntry.kind());
#endif
  }

  DebugModeOSREntry(JSScript* script, BaselineDebugModeOSRInfo* info)
      : script(script),
        oldBaselineScript(script->baselineScript()),
        recompInfo(nullptr),
        pcOffset(script->pcToOffset(info->pc)),
        frameKind(info->frameKind) {
#ifdef DEBUG
    MOZ_ASSERT(pcOffset == script->pcToOffset(info->pc));
    MOZ_ASSERT(frameKind == info->frameKind);
#endif
  }

  DebugModeOSREntry(DebugModeOSREntry&& other)
      : script(other.script),
        oldBaselineScript(other.oldBaselineScript),
        recompInfo(other.recompInfo ? other.takeRecompInfo() : nullptr),
        pcOffset(other.pcOffset),
        frameKind(other.frameKind) {}

  ~DebugModeOSREntry() {
    // Note that this is nulled out when the recompInfo is taken by the
    // frame. The frame then has the responsibility of freeing the
    // recompInfo.
    js_delete(recompInfo);
  }

  bool needsRecompileInfo() const {
    return frameKind == RetAddrEntry::Kind::CallVM ||
           frameKind == RetAddrEntry::Kind::WarmupCounter ||
           frameKind == RetAddrEntry::Kind::StackCheck ||
           frameKind == RetAddrEntry::Kind::DebugTrap ||
           frameKind == RetAddrEntry::Kind::DebugPrologue ||
           frameKind == RetAddrEntry::Kind::DebugAfterYield ||
           frameKind == RetAddrEntry::Kind::DebugEpilogue;
  }

  bool recompiled() const {
    return oldBaselineScript != script->baselineScript();
  }

  BaselineDebugModeOSRInfo* takeRecompInfo() {
    MOZ_ASSERT(needsRecompileInfo() && recompInfo);
    BaselineDebugModeOSRInfo* tmp = recompInfo;
    recompInfo = nullptr;
    return tmp;
  }

  bool allocateRecompileInfo(JSContext* cx) {
    MOZ_ASSERT(script);
    MOZ_ASSERT(needsRecompileInfo());

    // If we are returning to a frame which needs a continuation fixer,
    // allocate the recompile info up front so that the patching function
    // is infallible.
    jsbytecode* pc = script->offsetToPC(pcOffset);

    // XXX: Work around compiler error disallowing using bitfields
    // with the template magic of new_.
    RetAddrEntry::Kind kind = frameKind;
    recompInfo = cx->new_<BaselineDebugModeOSRInfo>(pc, kind);
    return !!recompInfo;
  }
};

typedef Vector<DebugModeOSREntry> DebugModeOSREntryVector;

class UniqueScriptOSREntryIter {
  const DebugModeOSREntryVector& entries_;
  size_t index_;

 public:
  explicit UniqueScriptOSREntryIter(const DebugModeOSREntryVector& entries)
      : entries_(entries), index_(0) {}

  bool done() { return index_ == entries_.length(); }

  const DebugModeOSREntry& entry() {
    MOZ_ASSERT(!done());
    return entries_[index_];
  }

  UniqueScriptOSREntryIter& operator++() {
    MOZ_ASSERT(!done());
    while (++index_ < entries_.length()) {
      bool unique = true;
      for (size_t i = 0; i < index_; i++) {
        if (entries_[i].script == entries_[index_].script) {
          unique = false;
          break;
        }
      }
      if (unique) {
        break;
      }
    }
    return *this;
  }
};

static bool CollectJitStackScripts(JSContext* cx,
                                   const Debugger::ExecutionObservableSet& obs,
                                   const ActivationIterator& activation,
                                   DebugModeOSREntryVector& entries) {
  bool needsRecompileHandler = false;
  for (OnlyJSJitFrameIter iter(activation); !iter.done(); ++iter) {
    const JSJitFrameIter& frame = iter.frame();
    switch (frame.type()) {
      case FrameType::BaselineJS: {
        JSScript* script = frame.script();

        if (!obs.shouldRecompileOrInvalidate(script)) {
          break;
        }

        BaselineFrame* baselineFrame = frame.baselineFrame();

        if (baselineFrame->runningInInterpreter()) {
          // Baseline Interpreter frames for scripts that have a BaselineScript
          // or IonScript don't need to be patched but they do need to be
          // invalidated and recompiled. See also CollectInterpreterStackScripts
          // for C++ interpreter frames.
          if (!entries.append(DebugModeOSREntry(script))) {
            return false;
          }
        } else if (BaselineDebugModeOSRInfo* info =
                       baselineFrame->getDebugModeOSRInfo()) {
          // If patching a previously patched yet unpopped frame, we can
          // use the BaselineDebugModeOSRInfo on the frame directly to
          // patch. Indeed, we cannot use frame.resumePCinCurrentFrame(), as
          // it points into the debug mode OSR handler and cannot be
          // used to look up a corresponding RetAddrEntry.
          //
          // See case F in PatchBaselineFramesForDebugMode.
          if (!entries.append(DebugModeOSREntry(script, info))) {
            return false;
          }
        } else if (baselineFrame->hasOverridePc()) {
          // If the frame is not settled on a pc with a RetAddrEntry,
          // overridePc will contain an explicit bytecode offset. We can
          // (and must) use that.
          uint32_t offset = script->pcToOffset(baselineFrame->overridePc());
          if (!entries.append(DebugModeOSREntry(script, offset))) {
            return false;
          }
        } else {
          // The frame must be settled on a pc with a RetAddrEntry.
          uint8_t* retAddr = frame.resumePCinCurrentFrame();
          RetAddrEntry& retAddrEntry =
              script->baselineScript()->retAddrEntryFromReturnAddress(retAddr);
          if (!entries.append(DebugModeOSREntry(script, retAddrEntry))) {
            return false;
          }
        }

        if (entries.back().needsRecompileInfo()) {
          if (!entries.back().allocateRecompileInfo(cx)) {
            return false;
          }

          needsRecompileHandler |= true;
        }
        break;
      }

      case FrameType::BaselineStub:
        break;

      case FrameType::IonJS: {
        InlineFrameIterator inlineIter(cx, &frame);
        while (true) {
          if (obs.shouldRecompileOrInvalidate(inlineIter.script())) {
            if (!entries.append(DebugModeOSREntry(inlineIter.script()))) {
              return false;
            }
          }
          if (!inlineIter.more()) {
            break;
          }
          ++inlineIter;
        }
        break;
      }

      default:;
    }
  }

  // Initialize the on-stack recompile handler, which may fail, so that
  // patching the stack is infallible.
  if (needsRecompileHandler) {
    JitRuntime* rt = cx->runtime()->jitRuntime();
    if (!rt->getBaselineDebugModeOSRHandlerAddress(cx, true)) {
      return false;
    }
  }

  return true;
}

static bool CollectInterpreterStackScripts(
    JSContext* cx, const Debugger::ExecutionObservableSet& obs,
    const ActivationIterator& activation, DebugModeOSREntryVector& entries) {
  // Collect interpreter frame stacks with IonScript or BaselineScript as
  // well. These do not need to be patched, but do need to be invalidated
  // and recompiled.
  InterpreterActivation* act = activation.activation()->asInterpreter();
  for (InterpreterFrameIterator iter(act); !iter.done(); ++iter) {
    JSScript* script = iter.frame()->script();
    if (obs.shouldRecompileOrInvalidate(script)) {
      if (!entries.append(DebugModeOSREntry(iter.frame()->script()))) {
        return false;
      }
    }
  }
  return true;
}

#ifdef JS_JITSPEW
static const char* RetAddrEntryKindToString(RetAddrEntry::Kind kind) {
  switch (kind) {
    case RetAddrEntry::Kind::IC:
      return "IC";
    case RetAddrEntry::Kind::PrologueIC:
      return "prologue IC";
    case RetAddrEntry::Kind::CallVM:
      return "callVM";
    case RetAddrEntry::Kind::WarmupCounter:
      return "warmup counter";
    case RetAddrEntry::Kind::StackCheck:
      return "stack check";
    case RetAddrEntry::Kind::DebugTrap:
      return "debug trap";
    case RetAddrEntry::Kind::DebugPrologue:
      return "debug prologue";
    case RetAddrEntry::Kind::DebugAfterYield:
      return "debug after yield";
    case RetAddrEntry::Kind::DebugEpilogue:
      return "debug epilogue";
    default:
      MOZ_CRASH("bad RetAddrEntry kind");
  }
}
#endif  // JS_JITSPEW

static void SpewPatchBaselineFrame(const uint8_t* oldReturnAddress,
                                   const uint8_t* newReturnAddress,
                                   JSScript* script,
                                   RetAddrEntry::Kind frameKind,
                                   const jsbytecode* pc) {
  JitSpew(JitSpew_BaselineDebugModeOSR,
          "Patch return %p -> %p on BaselineJS frame (%s:%u:%u) from %s at %s",
          oldReturnAddress, newReturnAddress, script->filename(),
          script->lineno(), script->column(),
          RetAddrEntryKindToString(frameKind), CodeName[(JSOp)*pc]);
}

static void SpewPatchBaselineFrameFromExceptionHandler(
    uint8_t* oldReturnAddress, uint8_t* newReturnAddress, JSScript* script,
    jsbytecode* pc) {
  JitSpew(JitSpew_BaselineDebugModeOSR,
          "Patch return %p -> %p on BaselineJS frame (%s:%u:%u) from exception "
          "handler at %s",
          oldReturnAddress, newReturnAddress, script->filename(),
          script->lineno(), script->column(), CodeName[(JSOp)*pc]);
}

static void PatchBaselineFramesForDebugMode(
    JSContext* cx, const Debugger::ExecutionObservableSet& obs,
    const ActivationIterator& activation, DebugModeOSREntryVector& entries,
    size_t* start) {
  //
  // Recompile Patching Overview
  //
  // When toggling debug mode with live baseline scripts on the stack, we
  // could have entered the VM via the following ways from the baseline
  // script.
  //
  // Off to On:
  //  A. From a "can call" IC stub.
  //  B. From a VM call.
  //  H. From inside HandleExceptionBaseline
  //  I. From inside the interrupt handler via the prologue stack check.
  //  J. From the warmup counter in the prologue.
  //
  // On to Off:
  //  - All the ways above.
  //  C. From the debug trap handler.
  //  D. From the debug prologue.
  //  E. From the debug epilogue.
  //  G. From GeneratorThrowOrReturn
  //  K. From a JSOP_AFTERYIELD instruction.
  //
  // Cycles (On to Off to On)+ or (Off to On to Off)+:
  //  F. Undo cases B, C, D, E, I or J above on previously patched yet unpopped
  //     frames.
  //
  // In general, we patch the return address from the VM call to return to a
  // "continuation fixer" to fix up machine state (registers and stack
  // state). Specifics on what needs to be done are documented below.
  //

  CommonFrameLayout* prev = nullptr;
  size_t entryIndex = *start;

  for (OnlyJSJitFrameIter iter(activation); !iter.done(); ++iter) {
    const JSJitFrameIter& frame = iter.frame();
    switch (frame.type()) {
      case FrameType::BaselineJS: {
        // If the script wasn't recompiled or is not observed, there's
        // nothing to patch.
        if (!obs.shouldRecompileOrInvalidate(frame.script())) {
          break;
        }

        DebugModeOSREntry& entry = entries[entryIndex];

        if (!entry.recompiled()) {
          entryIndex++;
          break;
        }

        BaselineFrame* baselineFrame = frame.baselineFrame();
        if (baselineFrame->runningInInterpreter()) {
          // We recompiled the script's BaselineScript but Baseline Interpreter
          // frames don't need to be patched.
          entryIndex++;
          break;
        }

        JSScript* script = entry.script;
        uint32_t pcOffset = entry.pcOffset;
        jsbytecode* pc = script->offsetToPC(pcOffset);

        MOZ_ASSERT(script == frame.script());
        MOZ_ASSERT(pcOffset < script->length());

        BaselineScript* bl = script->baselineScript();
        RetAddrEntry::Kind kind = entry.frameKind;

        if (kind == RetAddrEntry::Kind::IC) {
          // Case A above.
          //
          // For the baseline frame here, we resume right after the IC
          // returns. Since we're using the same IC stubs and stub code,
          // we don't have to patch the stub or stub frame.
          RetAddrEntry& retAddrEntry =
              bl->retAddrEntryFromPCOffset(pcOffset, kind);
          uint8_t* retAddr = bl->returnAddressForEntry(retAddrEntry);
          SpewPatchBaselineFrame(prev->returnAddress(), retAddr, script, kind,
                                 pc);
          DebugModeOSRVolatileJitFrameIter::forwardLiveIterators(
              cx, prev->returnAddress(), retAddr);
          prev->setReturnAddress(retAddr);
          entryIndex++;
          break;
        }

        if (kind == RetAddrEntry::Kind::Invalid) {
          // Cases G and H above.
          //
          // We are recompiling a frame with an override pc.
          // This may occur from inside the exception handler,
          // by way of an onExceptionUnwind invocation, on a pc
          // without a RetAddrEntry. It may also happen if we call
          // GeneratorThrowOrReturn and trigger onEnterFrame.
          //
          // If profiling is off, patch the resume address to nullptr,
          // to ensure the old address is not used anywhere.
          // If profiling is on, JSJitProfilingFrameIterator requires a
          // valid return address.
          MOZ_ASSERT(frame.baselineFrame()->overridePc() == pc);
          uint8_t* retAddr;
          if (cx->runtime()->geckoProfiler().enabled()) {
            // Won't actually jump to this address so we can ignore the
            // register state in the slot info.
            PCMappingSlotInfo unused;
            retAddr = bl->nativeCodeForPC(script, pc, &unused);
          } else {
            retAddr = nullptr;
          }
          SpewPatchBaselineFrameFromExceptionHandler(prev->returnAddress(),
                                                     retAddr, script, pc);
          DebugModeOSRVolatileJitFrameIter::forwardLiveIterators(
              cx, prev->returnAddress(), retAddr);
          prev->setReturnAddress(retAddr);
          entryIndex++;
          break;
        }

        // Case F above.
        //
        // We undo a previous recompile by handling cases B, C, D, E, I or J
        // like normal, except that we retrieve the pc information via
        // the previous OSR debug info stashed on the frame.
        BaselineDebugModeOSRInfo* info =
            frame.baselineFrame()->getDebugModeOSRInfo();
        if (info) {
          MOZ_ASSERT(info->pc == pc);
          MOZ_ASSERT(info->frameKind == kind);
          MOZ_ASSERT(kind == RetAddrEntry::Kind::CallVM ||
                     kind == RetAddrEntry::Kind::WarmupCounter ||
                     kind == RetAddrEntry::Kind::StackCheck ||
                     kind == RetAddrEntry::Kind::DebugTrap ||
                     kind == RetAddrEntry::Kind::DebugPrologue ||
                     kind == RetAddrEntry::Kind::DebugAfterYield ||
                     kind == RetAddrEntry::Kind::DebugEpilogue);

          // We will have allocated a new recompile info, so delete the
          // existing one.
          frame.baselineFrame()->deleteDebugModeOSRInfo();
        }

        // The RecompileInfo must already be allocated so that this
        // function may be infallible.
        BaselineDebugModeOSRInfo* recompInfo = entry.takeRecompInfo();

        bool popFrameReg;
        switch (kind) {
          case RetAddrEntry::Kind::CallVM: {
            // Case B above.
            //
            // Patching returns from a VM call. After fixing up the the
            // continuation for unsynced values (the frame register is
            // popped by the callVM trampoline), we resume at the
            // return-from-callVM address. The assumption here is that all
            // callVMs which can trigger debug mode OSR are the *only*
            // callVMs generated for their respective pc locations in the
            // baseline JIT code.
            RetAddrEntry& retAddrEntry =
                bl->retAddrEntryFromPCOffset(pcOffset, kind);
            recompInfo->resumeAddr = bl->returnAddressForEntry(retAddrEntry);
            popFrameReg = false;
            break;
          }

          case RetAddrEntry::Kind::WarmupCounter:
          case RetAddrEntry::Kind::StackCheck: {
            // Cases I and J above.
            //
            // Patching mechanism is identical to a CallVM. This is
            // handled especially only because these VM calls are part of
            // the prologue, and not tied to an opcode.
            RetAddrEntry& entry = bl->prologueRetAddrEntry(kind);
            recompInfo->resumeAddr = bl->returnAddressForEntry(entry);
            popFrameReg = false;
            break;
          }

          case RetAddrEntry::Kind::DebugTrap:
            // Case C above.
            //
            // Debug traps are emitted before each op, so we resume at the
            // same op. Calling debug trap handlers is done via a toggled
            // call to a thunk (DebugTrapHandler) that takes care tearing
            // down its own stub frame so we don't need to worry about
            // popping the frame reg.
            recompInfo->resumeAddr =
                bl->nativeCodeForPC(script, pc, &recompInfo->slotInfo);
            popFrameReg = false;
            break;

          case RetAddrEntry::Kind::DebugPrologue:
            // Case D above.
            //
            // We patch a jump directly to the right place in the prologue
            // after popping the frame reg and checking for forced return.
            recompInfo->resumeAddr = bl->debugOsrPrologueEntryAddr();
            popFrameReg = true;
            break;

          case RetAddrEntry::Kind::DebugAfterYield:
            // Case K above.
            //
            // Resume at the next instruction.
            MOZ_ASSERT(*pc == JSOP_AFTERYIELD);
            recompInfo->resumeAddr = bl->nativeCodeForPC(
                script, pc + JSOP_AFTERYIELD_LENGTH, &recompInfo->slotInfo);
            popFrameReg = true;
            break;

          default:
            // Case E above.
            //
            // We patch a jump directly to the epilogue after popping the
            // frame reg and checking for forced return.
            MOZ_ASSERT(kind == RetAddrEntry::Kind::DebugEpilogue);
            recompInfo->resumeAddr = bl->debugOsrEpilogueEntryAddr();
            popFrameReg = true;
            break;
        }

        SpewPatchBaselineFrame(prev->returnAddress(), recompInfo->resumeAddr,
                               script, kind, recompInfo->pc);

        // The recompile handler must already be created so that this
        // function may be infallible.
        JitRuntime* rt = cx->runtime()->jitRuntime();
        void* handlerAddr =
            rt->getBaselineDebugModeOSRHandlerAddress(cx, popFrameReg);
        MOZ_ASSERT(handlerAddr);

        prev->setReturnAddress(reinterpret_cast<uint8_t*>(handlerAddr));
        frame.baselineFrame()->setDebugModeOSRInfo(recompInfo);
        frame.baselineFrame()->setOverridePc(recompInfo->pc);

        entryIndex++;
        break;
      }

      case FrameType::IonJS: {
        // Nothing to patch.
        InlineFrameIterator inlineIter(cx, &frame);
        while (true) {
          if (obs.shouldRecompileOrInvalidate(inlineIter.script())) {
            entryIndex++;
          }
          if (!inlineIter.more()) {
            break;
          }
          ++inlineIter;
        }
        break;
      }

      default:;
    }

    prev = frame.current();
  }

  *start = entryIndex;
}

static void SkipInterpreterFrameEntries(
    const Debugger::ExecutionObservableSet& obs,
    const ActivationIterator& activation, size_t* start) {
  size_t entryIndex = *start;

  // Skip interpreter frames, which do not need patching.
  InterpreterActivation* act = activation.activation()->asInterpreter();
  for (InterpreterFrameIterator iter(act); !iter.done(); ++iter) {
    if (obs.shouldRecompileOrInvalidate(iter.frame()->script())) {
      entryIndex++;
    }
  }

  *start = entryIndex;
}

static bool RecompileBaselineScriptForDebugMode(
    JSContext* cx, JSScript* script, Debugger::IsObserving observing) {
  BaselineScript* oldBaselineScript = script->baselineScript();

  // If a script is on the stack multiple times, it may have already
  // been recompiled.
  if (oldBaselineScript->hasDebugInstrumentation() == observing) {
    return true;
  }

  JitSpew(JitSpew_BaselineDebugModeOSR, "Recompiling (%s:%u:%u) for %s",
          script->filename(), script->lineno(), script->column(),
          observing ? "DEBUGGING" : "NORMAL EXECUTION");

  AutoKeepTypeScripts keepTypes(cx);
  script->setBaselineScript(cx->runtime(), nullptr);

  MethodStatus status =
      BaselineCompile(cx, script, /* forceDebugMode = */ observing);
  if (status != Method_Compiled) {
    // We will only fail to recompile for debug mode due to OOM. Restore
    // the old baseline script in case something doesn't properly
    // propagate OOM.
    MOZ_ASSERT(status == Method_Error);
    script->setBaselineScript(cx->runtime(), oldBaselineScript);
    return false;
  }

  // Don't destroy the old baseline script yet, since if we fail any of the
  // recompiles we need to rollback all the old baseline scripts.
  MOZ_ASSERT(script->baselineScript()->hasDebugInstrumentation() == observing);
  return true;
}

static bool InvalidateScriptsInZone(JSContext* cx, Zone* zone,
                                    const Vector<DebugModeOSREntry>& entries) {
  RecompileInfoVector invalid;
  for (UniqueScriptOSREntryIter iter(entries); !iter.done(); ++iter) {
    JSScript* script = iter.entry().script;
    if (script->zone() != zone) {
      continue;
    }

    if (script->hasIonScript()) {
      if (!invalid.emplaceBack(script, script->ionScript()->compilationId())) {
        ReportOutOfMemory(cx);
        return false;
      }
    }

    // Cancel off-thread Ion compile for anything that has a
    // BaselineScript. If we relied on the call to Invalidate below to
    // cancel off-thread Ion compiles, only those with existing IonScripts
    // would be cancelled.
    if (script->hasBaselineScript()) {
      CancelOffThreadIonCompile(script);
    }
  }

  // No need to cancel off-thread Ion compiles again, we already did it
  // above.
  Invalidate(zone->types, cx->runtime()->defaultFreeOp(), invalid,
             /* resetUses = */ true, /* cancelOffThread = */ false);
  return true;
}

static void UndoRecompileBaselineScriptsForDebugMode(
    JSContext* cx, const DebugModeOSREntryVector& entries) {
  // In case of failure, roll back the entire set of active scripts so that
  // we don't have to patch return addresses on the stack.
  for (UniqueScriptOSREntryIter iter(entries); !iter.done(); ++iter) {
    const DebugModeOSREntry& entry = iter.entry();
    JSScript* script = entry.script;
    BaselineScript* baselineScript = script->baselineScript();
    if (entry.recompiled()) {
      script->setBaselineScript(cx->runtime(), entry.oldBaselineScript);
      BaselineScript::Destroy(cx->runtime()->defaultFreeOp(), baselineScript);
    }
  }
}

bool jit::RecompileOnStackBaselineScriptsForDebugMode(
    JSContext* cx, const Debugger::ExecutionObservableSet& obs,
    Debugger::IsObserving observing) {
  // First recompile the active scripts on the stack and patch the live
  // frames.
  Vector<DebugModeOSREntry> entries(cx);

  for (ActivationIterator iter(cx); !iter.done(); ++iter) {
    if (iter->isJit()) {
      if (!CollectJitStackScripts(cx, obs, iter, entries)) {
        return false;
      }
    } else if (iter->isInterpreter()) {
      if (!CollectInterpreterStackScripts(cx, obs, iter, entries)) {
        return false;
      }
    }
  }

  if (entries.empty()) {
    return true;
  }

  // When the profiler is enabled, we need to have suppressed sampling,
  // since the basline jit scripts are in a state of flux.
  MOZ_ASSERT(!cx->isProfilerSamplingEnabled());

  // Invalidate all scripts we are recompiling.
  if (Zone* zone = obs.singleZone()) {
    if (!InvalidateScriptsInZone(cx, zone, entries)) {
      return false;
    }
  } else {
    typedef Debugger::ExecutionObservableSet::ZoneRange ZoneRange;
    for (ZoneRange r = obs.zones()->all(); !r.empty(); r.popFront()) {
      if (!InvalidateScriptsInZone(cx, r.front(), entries)) {
        return false;
      }
    }
  }

  // Try to recompile all the scripts. If we encounter an error, we need to
  // roll back as if none of the compilations happened, so that we don't
  // crash.
  for (size_t i = 0; i < entries.length(); i++) {
    JSScript* script = entries[i].script;
    AutoRealm ar(cx, script);
    if (!RecompileBaselineScriptForDebugMode(cx, script, observing)) {
      UndoRecompileBaselineScriptsForDebugMode(cx, entries);
      return false;
    }
  }

  // If all recompiles succeeded, destroy the old baseline scripts and patch
  // the live frames.
  //
  // After this point the function must be infallible.

  for (UniqueScriptOSREntryIter iter(entries); !iter.done(); ++iter) {
    const DebugModeOSREntry& entry = iter.entry();
    if (entry.recompiled()) {
      BaselineScript::Destroy(cx->runtime()->defaultFreeOp(),
                              entry.oldBaselineScript);
    }
  }

  size_t processed = 0;
  for (ActivationIterator iter(cx); !iter.done(); ++iter) {
    if (iter->isJit()) {
      PatchBaselineFramesForDebugMode(cx, obs, iter, entries, &processed);
    } else if (iter->isInterpreter()) {
      SkipInterpreterFrameEntries(obs, iter, &processed);
    }
  }
  MOZ_ASSERT(processed == entries.length());

  return true;
}

void BaselineDebugModeOSRInfo::popValueInto(PCMappingSlotInfo::SlotLocation loc,
                                            Value* vp) {
  switch (loc) {
    case PCMappingSlotInfo::SlotInR0:
      valueR0 = vp[stackAdjust];
      break;
    case PCMappingSlotInfo::SlotInR1:
      valueR1 = vp[stackAdjust];
      break;
    case PCMappingSlotInfo::SlotIgnore:
      break;
    default:
      MOZ_CRASH("Bad slot location");
  }

  stackAdjust++;
}

static inline bool HasForcedReturn(BaselineDebugModeOSRInfo* info, bool rv) {
  RetAddrEntry::Kind kind = info->frameKind;

  // The debug epilogue always checks its resumption value, so we don't need
  // to check rv.
  if (kind == RetAddrEntry::Kind::DebugEpilogue) {
    return true;
  }

  // |rv| is the value in ReturnReg. If true, in the case of the prologue or
  // after yield, it means a forced return.
  if (kind == RetAddrEntry::Kind::DebugPrologue ||
      kind == RetAddrEntry::Kind::DebugAfterYield) {
    return rv;
  }

  // N.B. The debug trap handler handles its own forced return, so no
  // need to deal with it here.
  return false;
}

static inline bool IsReturningFromCallVM(BaselineDebugModeOSRInfo* info) {
  // Keep this in sync with EmitBranchIsReturningFromCallVM.
  //
  // The stack check entries are returns from a callVM, but have a special
  // kind because they do not exist in a 1-1 relationship with a pc offset.
  return info->frameKind == RetAddrEntry::Kind::CallVM ||
         info->frameKind == RetAddrEntry::Kind::WarmupCounter ||
         info->frameKind == RetAddrEntry::Kind::StackCheck;
}

static void EmitBranchRetAddrEntryKind(MacroAssembler& masm, Register entry,
                                       RetAddrEntry::Kind kind, Label* label) {
  masm.branch32(MacroAssembler::Equal,
                Address(entry, offsetof(BaselineDebugModeOSRInfo, frameKind)),
                Imm32(uint32_t(kind)), label);
}

static void EmitBranchIsReturningFromCallVM(MacroAssembler& masm,
                                            Register entry, Label* label) {
  // Keep this in sync with IsReturningFromCallVM.
  EmitBranchRetAddrEntryKind(masm, entry, RetAddrEntry::Kind::CallVM, label);
  EmitBranchRetAddrEntryKind(masm, entry, RetAddrEntry::Kind::WarmupCounter,
                             label);
  EmitBranchRetAddrEntryKind(masm, entry, RetAddrEntry::Kind::StackCheck,
                             label);
}

static void SyncBaselineDebugModeOSRInfo(BaselineFrame* frame, Value* vp,
                                         bool rv) {
  AutoUnsafeCallWithABI unsafe;
  BaselineDebugModeOSRInfo* info = frame->debugModeOSRInfo();
  MOZ_ASSERT(info);
  MOZ_ASSERT(
      frame->script()->baselineScript()->containsCodeAddress(info->resumeAddr));

  if (HasForcedReturn(info, rv)) {
    // Load the frame's rval and overwrite the resume address to go to the
    // epilogue.
    MOZ_ASSERT(R0 == JSReturnOperand);
    info->valueR0 = frame->returnValue();
    info->resumeAddr =
        frame->script()->baselineScript()->debugOsrEpilogueEntryAddr();
    return;
  }

  // Read stack values and make sure R0 and R1 have the right values if we
  // aren't returning from a callVM.
  //
  // In the case of returning from a callVM, we don't need to restore R0 and
  // R1 ourself since we'll return into code that does it if needed.
  if (!IsReturningFromCallVM(info)) {
    unsigned numUnsynced = info->slotInfo.numUnsynced();
    MOZ_ASSERT(numUnsynced <= 2);
    if (numUnsynced > 0) {
      info->popValueInto(info->slotInfo.topSlotLocation(), vp);
    }
    if (numUnsynced > 1) {
      info->popValueInto(info->slotInfo.nextSlotLocation(), vp);
    }
  }

  // Scale stackAdjust.
  info->stackAdjust *= sizeof(Value);
}

static void FinishBaselineDebugModeOSR(BaselineFrame* frame) {
  AutoUnsafeCallWithABI unsafe;
  frame->deleteDebugModeOSRInfo();

  // We will return to JIT code now so we have to clear the override pc.
  frame->clearOverridePc();
}

void BaselineFrame::deleteDebugModeOSRInfo() {
  js_delete(getDebugModeOSRInfo());
  flags_ &= ~HAS_DEBUG_MODE_OSR_INFO;
}

JitCode* JitRuntime::getBaselineDebugModeOSRHandler(JSContext* cx) {
  if (!baselineDebugModeOSRHandler_) {
    MOZ_ASSERT(js::CurrentThreadCanAccessRuntime(cx->runtime()));
    AutoAllocInAtomsZone az(cx);
    uint32_t offset;
    if (JitCode* code = generateBaselineDebugModeOSRHandler(cx, &offset)) {
      baselineDebugModeOSRHandler_ = code;
      baselineDebugModeOSRHandlerNoFrameRegPopAddr_ = code->raw() + offset;
    }
  }

  return baselineDebugModeOSRHandler_;
}

void* JitRuntime::getBaselineDebugModeOSRHandlerAddress(JSContext* cx,
                                                        bool popFrameReg) {
  if (!getBaselineDebugModeOSRHandler(cx)) {
    return nullptr;
  }
  return popFrameReg ? baselineDebugModeOSRHandler_->raw()
                     : baselineDebugModeOSRHandlerNoFrameRegPopAddr_.ref();
}

static void PushCallVMOutputRegisters(MacroAssembler& masm) {
  // callVMs can use several different output registers, depending on the
  // type of their outparam.
  masm.push(ReturnReg);
  masm.push(ReturnDoubleReg);
  masm.Push(JSReturnOperand);
}

static void PopCallVMOutputRegisters(MacroAssembler& masm) {
  masm.Pop(JSReturnOperand);
  masm.pop(ReturnDoubleReg);
  masm.pop(ReturnReg);
}

static void TakeCallVMOutputRegisters(AllocatableGeneralRegisterSet& regs) {
  regs.take(ReturnReg);
  regs.take(JSReturnOperand);
}

static void EmitBaselineDebugModeOSRHandlerTail(MacroAssembler& masm,
                                                Register temp,
                                                bool returnFromCallVM) {
  // Save real return address on the stack temporarily.
  //
  // If we're returning from a callVM, we don't need to worry about R0 and
  // R1 but do need to propagate the registers the call might write to.
  // Otherwise we need to worry about R0 and R1 but can clobber ReturnReg.
  // Indeed, on x86, R1 contains ReturnReg.
  if (returnFromCallVM) {
    PushCallVMOutputRegisters(masm);
  } else {
    masm.pushValue(Address(temp, offsetof(BaselineDebugModeOSRInfo, valueR0)));
    masm.pushValue(Address(temp, offsetof(BaselineDebugModeOSRInfo, valueR1)));
  }
  masm.push(BaselineFrameReg);
  masm.push(Address(temp, offsetof(BaselineDebugModeOSRInfo, resumeAddr)));

  // Call a stub to free the allocated info.
  masm.setupUnalignedABICall(temp);
  masm.loadBaselineFramePtr(BaselineFrameReg, temp);
  masm.passABIArg(temp);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, FinishBaselineDebugModeOSR));

  // Restore saved values.
  AllocatableGeneralRegisterSet jumpRegs(GeneralRegisterSet::All());
  if (returnFromCallVM) {
    TakeCallVMOutputRegisters(jumpRegs);
  } else {
    jumpRegs.take(R0);
    jumpRegs.take(R1);
  }
  jumpRegs.take(BaselineFrameReg);
  Register target = jumpRegs.takeAny();

  masm.pop(target);
  masm.pop(BaselineFrameReg);
  if (returnFromCallVM) {
    PopCallVMOutputRegisters(masm);
  } else {
    masm.popValue(R1);
    masm.popValue(R0);
  }

  masm.jump(target);
}

JitCode* JitRuntime::generateBaselineDebugModeOSRHandler(
    JSContext* cx, uint32_t* noFrameRegPopOffsetOut) {
  StackMacroAssembler masm(cx);

  AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
  regs.take(BaselineFrameReg);
  TakeCallVMOutputRegisters(regs);
  Register temp = regs.takeAny();
  Register syncedStackStart = regs.takeAny();

  // Pop the frame reg.
  masm.pop(BaselineFrameReg);

  // Not all patched baseline frames are returning from a situation where
  // the frame reg is already fixed up.
  CodeOffset noFrameRegPopOffset(masm.currentOffset());

  // Record the stack pointer for syncing.
  masm.moveStackPtrTo(syncedStackStart);
  PushCallVMOutputRegisters(masm);
  masm.push(BaselineFrameReg);

  // Call a stub to fully initialize the info.
  masm.setupUnalignedABICall(temp);
  masm.loadBaselineFramePtr(BaselineFrameReg, temp);
  masm.passABIArg(temp);
  masm.passABIArg(syncedStackStart);
  masm.passABIArg(ReturnReg);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, SyncBaselineDebugModeOSRInfo));

  // Discard stack values depending on how many were unsynced, as we always
  // have a fully synced stack in the recompile handler. We arrive here via
  // a callVM, and prepareCallVM in BaselineCompiler always fully syncs the
  // stack.
  masm.pop(BaselineFrameReg);
  PopCallVMOutputRegisters(masm);
  masm.loadPtr(
      Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfScratchValue()),
      temp);
  masm.addToStackPtr(
      Address(temp, offsetof(BaselineDebugModeOSRInfo, stackAdjust)));

  // Emit two tails for the case of returning from a callVM and all other
  // cases, as the state we need to restore differs depending on the case.
  Label returnFromCallVM, end;
  EmitBranchIsReturningFromCallVM(masm, temp, &returnFromCallVM);

  EmitBaselineDebugModeOSRHandlerTail(masm, temp,
                                      /* returnFromCallVM = */ false);
  masm.jump(&end);
  masm.bind(&returnFromCallVM);
  EmitBaselineDebugModeOSRHandlerTail(masm, temp,
                                      /* returnFromCallVM = */ true);
  masm.bind(&end);

  Linker linker(masm, "BaselineDebugModeOSRHandler");
  JitCode* code = linker.newCode(cx, CodeKind::Other);
  if (!code) {
    return nullptr;
  }

  *noFrameRegPopOffsetOut = noFrameRegPopOffset.offset();

#ifdef JS_ION_PERF
  writePerfSpewerJitCodeProfile(code, "BaselineDebugModeOSRHandler");
#endif

  return code;
}

/* static */
void DebugModeOSRVolatileJitFrameIter::forwardLiveIterators(JSContext* cx,
                                                            uint8_t* oldAddr,
                                                            uint8_t* newAddr) {
  DebugModeOSRVolatileJitFrameIter* iter;
  for (iter = cx->liveVolatileJitFrameIter_; iter; iter = iter->prev) {
    if (iter->isWasm()) {
      continue;
    }
    iter->asJSJit().exchangeReturnAddressIfMatch(oldAddr, newAddr);
  }
}
