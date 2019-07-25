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
  uint32_t pcOffset;
  RetAddrEntry::Kind frameKind;

  explicit DebugModeOSREntry(JSScript* script)
      : script(script),
        oldBaselineScript(script->baselineScript()),
        pcOffset(uint32_t(-1)),
        frameKind(RetAddrEntry::Kind::Invalid) {}

  DebugModeOSREntry(JSScript* script, uint32_t pcOffset)
      : script(script),
        oldBaselineScript(script->baselineScript()),
        pcOffset(pcOffset),
        frameKind(RetAddrEntry::Kind::Invalid) {}

  DebugModeOSREntry(JSScript* script, const RetAddrEntry& retAddrEntry)
      : script(script),
        oldBaselineScript(script->baselineScript()),
        pcOffset(retAddrEntry.pcOffset()),
        frameKind(retAddrEntry.kind()) {
#ifdef DEBUG
    MOZ_ASSERT(pcOffset == retAddrEntry.pcOffset());
    MOZ_ASSERT(frameKind == retAddrEntry.kind());
#endif
  }

  DebugModeOSREntry(DebugModeOSREntry&& other)
      : script(other.script),
        oldBaselineScript(other.oldBaselineScript),
        pcOffset(other.pcOffset),
        frameKind(other.frameKind) {}

  bool recompiled() const {
    return oldBaselineScript != script->baselineScript();
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
                                   const DebugAPI::ExecutionObservableSet& obs,
                                   const ActivationIterator& activation,
                                   DebugModeOSREntryVector& entries) {
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

  return true;
}

static bool CollectInterpreterStackScripts(
    JSContext* cx, const DebugAPI::ExecutionObservableSet& obs,
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
    JSContext* cx, const DebugAPI::ExecutionObservableSet& obs,
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
  //  C. From inside the interrupt handler via the prologue stack check.
  //  D. From the warmup counter in the prologue.
  //  E. From inside HandleExceptionBaseline
  //
  // On to Off:
  //  - All the ways above.
  //  F. From the debug trap handler.
  //  G. From the debug prologue.
  //  H. From the debug epilogue.
  //  I. From a JSOP_AFTERYIELD instruction.
  //  J. From GeneratorThrowOrReturn
  //
  // In general, we patch the return address from VM calls and ICs to the
  // corresponding entry in the recompiled BaselineScript. For entries that are
  // not present in the recompiled script (cases F to I above) we switch the
  // frame to interpreter mode and resume in the Baseline Interpreter.
  //
  // Specifics on what needs to be done are documented below.
  //

  const BaselineInterpreter& baselineInterp =
      cx->runtime()->jitRuntime()->baselineInterpreter();

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
        uint8_t* retAddr = nullptr;
        switch (kind) {
          case RetAddrEntry::Kind::IC:
          case RetAddrEntry::Kind::CallVM:
          case RetAddrEntry::Kind::WarmupCounter:
          case RetAddrEntry::Kind::StackCheck: {
            // Cases A, B, C, D above.
            //
            // For the baseline frame here, we resume right after the CallVM or
            // IC returns.
            //
            // For CallVM (case B) the assumption is that all callVMs which can
            // trigger debug mode OSR are the *only* callVMs generated for their
            // respective pc locations in the Baseline JIT code.
            RetAddrEntry* retAddrEntry = nullptr;
            switch (kind) {
              case RetAddrEntry::Kind::IC:
              case RetAddrEntry::Kind::CallVM:
                retAddrEntry = &bl->retAddrEntryFromPCOffset(pcOffset, kind);
                break;
              case RetAddrEntry::Kind::WarmupCounter:
              case RetAddrEntry::Kind::StackCheck:
                retAddrEntry = &bl->prologueRetAddrEntry(kind);
                break;
              default:
                MOZ_CRASH("Unexpected kind");
            }
            retAddr = bl->returnAddressForEntry(*retAddrEntry);
            SpewPatchBaselineFrame(prev->returnAddress(), retAddr, script, kind,
                                   pc);
            break;
          }
          case RetAddrEntry::Kind::DebugPrologue:
          case RetAddrEntry::Kind::DebugEpilogue:
          case RetAddrEntry::Kind::DebugTrap:
          case RetAddrEntry::Kind::DebugAfterYield: {
            // Cases F, G, H, I above.
            //
            // Resume in the Baseline Interpreter because these callVMs are not
            // present in the new BaselineScript if we recompiled without debug
            // instrumentation.
            frame.baselineFrame()->switchFromJitToInterpreter(pc);
            switch (kind) {
              case RetAddrEntry::Kind::DebugTrap:
                // DebugTrap handling is different from the ones below because
                // it's not a callVM but a trampoline call at the start of the
                // bytecode op. When we return to the frame we can resume at the
                // interpretOp label.
                retAddr = baselineInterp.interpretOpAddr().value;
                break;
              case RetAddrEntry::Kind::DebugPrologue:
                retAddr = baselineInterp.retAddrForDebugPrologueCallVM();
                break;
              case RetAddrEntry::Kind::DebugEpilogue:
                retAddr = baselineInterp.retAddrForDebugEpilogueCallVM();
                break;
              case RetAddrEntry::Kind::DebugAfterYield:
                retAddr = baselineInterp.retAddrForDebugAfterYieldCallVM();
                break;
              default:
                MOZ_CRASH("Unexpected kind");
            }
            SpewPatchBaselineFrame(prev->returnAddress(), retAddr, script, kind,
                                   pc);
            break;
          }
          case RetAddrEntry::Kind::Invalid: {
            // Cases E and J above.
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
            break;
          }
          case RetAddrEntry::Kind::PrologueIC:
          case RetAddrEntry::Kind::NonOpCallVM:
            // These cannot trigger BaselineDebugModeOSR.
            MOZ_CRASH("Unexpected RetAddrEntry Kind");
        }

        DebugModeOSRVolatileJitFrameIter::forwardLiveIterators(
            cx, prev->returnAddress(), retAddr);
        prev->setReturnAddress(retAddr);
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
    const DebugAPI::ExecutionObservableSet& obs,
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
    JSContext* cx, JSScript* script, bool observing) {
  BaselineScript* oldBaselineScript = script->baselineScript();

  // If a script is on the stack multiple times, it may have already
  // been recompiled.
  if (oldBaselineScript->hasDebugInstrumentation() == observing) {
    return true;
  }

  JitSpew(JitSpew_BaselineDebugModeOSR, "Recompiling (%s:%u:%u) for %s",
          script->filename(), script->lineno(), script->column(),
          observing ? "DEBUGGING" : "NORMAL EXECUTION");

  AutoKeepJitScripts keepJitScripts(cx);
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
    JSContext* cx, const DebugAPI::ExecutionObservableSet& obs,
    bool observing) {
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
    typedef DebugAPI::ExecutionObservableSet::ZoneRange ZoneRange;
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
