/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/TrialInlining.h"

#include "jit/BaselineCacheIRCompiler.h"
#include "jit/BaselineIC.h"

#include "jit/BaselineFrame-inl.h"
#include "vm/BytecodeIterator-inl.h"
#include "vm/BytecodeLocation-inl.h"

namespace js {
namespace jit {

bool DoTrialInlining(JSContext* cx, BaselineFrame* frame) {
  MOZ_ASSERT(JitOptions.warpBuilder);

  RootedScript script(cx, frame->script());
  ICScript* icScript = frame->icScript();
  bool isRecursive = !!icScript->inliningRoot();

  InliningRoot* root = isRecursive
                           ? icScript->inliningRoot()
                           : script->jitScript()->getOrCreateInliningRoot(cx);
  JitSpew(JitSpew_WarpTrialInlining,
          "Trial inlining for %s script %s:%u%u (%p) (inliningRoot=%p)",
          (isRecursive ? "inner" : "outer"), script->filename(),
          script->lineno(), script->column(), frame->script(), root);

  TrialInliner inliner(cx, script, icScript, root);
  return inliner.tryInlining();
}

void TrialInliner::cloneSharedPrefix(ICStub* stub, const uint8_t* endOfPrefix,
                                     CacheIRWriter& writer) {
  CacheIRReader reader(stub->cacheIRStubInfo());
  CacheIRCloner cloner(stub);
  while (reader.currentPosition() < endOfPrefix) {
    CacheOp op = reader.readOp();
    cloner.cloneOp(op, reader, writer);
  }
}

void TrialInliner::replaceICStub(const ICEntry& entry, CacheIRWriter& writer,
                                 CacheKind kind) {
  ICFallbackStub* fallbackStub = entry.fallbackStub();
  fallbackStub->discardStubs(cx(), script_);

  // Note: AttachBaselineCacheIRStub never throws an exception.
  bool attached = false;
  ICStub* newStub = AttachBaselineCacheIRStub(
      cx(), writer, kind, BaselineCacheIRStubKind::Regular, script_, icScript_,
      fallbackStub, &attached);
  if (newStub) {
    MOZ_ASSERT(attached);
    JitSpew(JitSpew_WarpTrialInlining, "Attached new stub %p", newStub);
  }
}

ICStub* TrialInliner::maybeSingleStub(const ICEntry& entry) {
  ICStub* stub = entry.firstStub();
  if (stub->isFallback() || !stub->next()->isFallback()) {
    return nullptr;
  }
  return stub;
}

bool TrialInliner::shouldInline(JSFunction* target) {
  // TODO: Actual heuristics
  return target->hasJitScript() &&
         target->baseScript()->jitScript()->hasBaselineScript();
}

ICScript* TrialInliner::createInlinedICScript(JSFunction* target) {
  MOZ_ASSERT(target->hasJitEntry());
  MOZ_ASSERT(target->hasJitScript());

  JSScript* targetScript = target->baseScript()->asJSScript();
  JitScript* targetJitScript = targetScript->jitScript();

  // We don't have to check for overflow here because we have already
  // successfully allocated an ICScript with this number of entries
  // when creating the JitScript for the target function, and we
  // checked for overflow then.
  uint32_t allocSize =
      sizeof(ICScript) + targetScript->numICEntries() * sizeof(ICEntry);

  void* raw = cx()->pod_malloc<uint8_t>(allocSize);
  MOZ_ASSERT(uintptr_t(raw) % alignof(ICScript) == 0);
  if (!raw) {
    return nullptr;
  }

  // TODO: Increase this to make recursive inlining more aggressive.
  // Alternatively, we could add a trialInliningThreshold field to
  // ICScript to give more precise control.
  const uint32_t InitialWarmUpCount = 0;

  UniquePtr<ICScript> inlinedICScript(new (raw) ICScript(
      targetJitScript, InitialWarmUpCount, allocSize, root_));

  {
    // Suppress GC. This matches the AutoEnterAnalysis in
    // JSScript::createJitScript. It is needed for allocating the
    // template object for JSOp::Rest and the object group for
    // JSOp::NewArray.
    gc::AutoSuppressGC suppress(cx());
    if (!inlinedICScript->initICEntries(cx(), targetScript)) {
      return nullptr;
    }
  }

  ICScript* result = inlinedICScript.get();
  if (!root_->addInlinedScript(std::move(inlinedICScript))) {
    return nullptr;
  }
  MOZ_ASSERT(result->numICEntries() == targetScript->numICEntries());

  return result;
}

bool TrialInliner::maybeInlineCall(const ICEntry& entry, BytecodeLocation loc) {
  ICStub* stub = maybeSingleStub(entry);
  if (!stub) {
    return true;
  }
  const CacheIRStubInfo* stubInfo = stub->cacheIRStubInfo();

  ObjOperandId calleeGuardOperand;
  CallFlags callFlags;
  uint32_t targetOffset = 0;
  bool isInlinableCall = false;
  const uint8_t* endOfSharedPrefix = nullptr;

  // Look for a CallScriptedFunction with a known target.
  CacheIRReader reader(stubInfo);
  while (reader.more()) {
    const uint8_t* opStart = reader.currentPosition();

    CacheOp op = reader.readOp();
    uint32_t argLength = CacheIROpArgLengths[size_t(op)];

    switch (op) {
      case CacheOp::GuardSpecificFunction:
        // If we see a guard, remember which operand we are guarding.
        MOZ_ASSERT(!isInlinableCall && !calleeGuardOperand.valid());
        calleeGuardOperand = reader.objOperandId();
        targetOffset = reader.stubOffset();
        mozilla::Unused << reader.stubOffset();  // nargsAndFlags
        break;
      case CacheOp::CallScriptedFunction: {
        // If we see a call, check if `callee` is the previously guarded
        // operand. If it is, we know the target and can inline.
        ObjOperandId calleeOperand = reader.objOperandId();
        mozilla::DebugOnly<Int32OperandId> argcId = reader.int32OperandId();
        callFlags = reader.callFlags();
        if (calleeOperand == calleeGuardOperand) {
          MOZ_ASSERT(!isInlinableCall);
          MOZ_ASSERT(static_cast<OperandId&>(argcId).id() == 0);
          isInlinableCall = true;
          endOfSharedPrefix = opStart;
        }
        break;
      }
      default:
        if (isInlinableCall) {
          MOZ_ASSERT(op == CacheOp::ReturnFromIC ||
                     op == CacheOp::TypeMonitorResult);
        }
        reader.skip(argLength);
        break;
    }
    MOZ_ASSERT(opStart + 1 + argLength == reader.currentPosition());
  }

  if (!isInlinableCall) {
    return true;
  }

  const uint8_t* stubData = stub->cacheIRStubData();
  uintptr_t rawTarget = stubInfo->getStubRawWord(stubData, targetOffset);
  JSFunction* target = reinterpret_cast<JSFunction*>(rawTarget);

  JitSpew(JitSpew_WarpTrialInlining,
          "Inlining candidate JSOp::%s: callee function %p",
          CodeName(loc.getOp()), target);

  // Decide whether to inline the target.
  if (!shouldInline(target)) {
    return true;
  }

  // TODO: The arguments rectifier is not yet supported.
  if (loc.getCallArgc() < target->nargs()) {
    return true;
  }

  ICScript* newICScript = createInlinedICScript(target);
  if (!newICScript) {
    return false;
  }

  CacheIRWriter writer(cx());
  Int32OperandId argcId(writer.setInputOperandId(0));
  cloneSharedPrefix(stub, endOfSharedPrefix, writer);

  writer.callInlinedFunction(calleeGuardOperand, argcId, newICScript,
                             callFlags);
  writer.returnFromIC();

  replaceICStub(entry, writer, CacheKind::Call);
  return true;
}

bool TrialInliner::tryInlining() {
  uint32_t icIndex = 0;
  for (BytecodeLocation loc : AllBytecodesIterable(script_)) {
    JSOp op = loc.getOp();
    switch (op) {
      case JSOp::Call:
      case JSOp::CallIgnoresRv:
        if (!maybeInlineCall(icScript_->icEntry(icIndex), loc)) {
          return false;
        }
        break;
      default:
        break;
    }
    if (loc.opHasIC()) {
      icIndex++;
    }
  }

  return true;
}

bool InliningRoot::addInlinedScript(UniquePtr<ICScript> icScript) {
  return inlinedScripts_.append(std::move(icScript));
}

void InliningRoot::trace(JSTracer* trc) {
  for (auto& inlinedScript : inlinedScripts_) {
    inlinedScript->trace(trc);
  }
}

void InliningRoot::purgeOptimizedStubs(Zone* zone) {
  for (auto& inlinedScript : inlinedScripts_) {
    inlinedScript->purgeOptimizedStubs(zone);
  }
}

}  // namespace jit
}  // namespace js
