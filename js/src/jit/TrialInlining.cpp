/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/TrialInlining.h"

#include "jit/BaselineCacheIRCompiler.h"
#include "jit/BaselineIC.h"
#include "jit/Ion.h"  // TooManyFormalArguments

#include "jit/BaselineFrame-inl.h"
#include "vm/BytecodeIterator-inl.h"
#include "vm/BytecodeLocation-inl.h"

using mozilla::Maybe;

namespace js {
namespace jit {

bool DoTrialInlining(JSContext* cx, BaselineFrame* frame) {
  MOZ_ASSERT(JitOptions.warpBuilder);

  RootedScript script(cx, frame->script());
  ICScript* icScript = frame->icScript();
  bool isRecursive = icScript->depth() > 0;

  if (!script->canIonCompile()) {
    return true;
  }

  // Baseline shouldn't attempt trial inlining in scripts that are too large.
  MOZ_ASSERT(script->length() <= JitOptions.ionMaxScriptSize);

  const uint32_t MAX_INLINING_DEPTH = 4;
  if (icScript->depth() > MAX_INLINING_DEPTH) {
    return true;
  }

  InliningRoot* root =
      isRecursive ? icScript->inliningRoot()
                  : script->jitScript()->getOrCreateInliningRoot(cx, script);
  if (!root) {
    return false;
  }

  JitSpew(JitSpew_WarpTrialInlining,
          "Trial inlining for %s script %s:%u:%u (%p) (inliningRoot=%p)",
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
  fallbackStub->discardStubs(cx(), root_->owningScript());

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
  // Look for a single non-fallback stub followed by stubs with entered-count 0.
  // Allow one optimized stub before the fallback stub to support the
  // CallIRGenerator::emitCalleeGuard optimization where we first try a
  // GuardSpecificFunction guard before falling back to GuardFunctionHasScript.
  ICStub* stub = entry.firstStub();
  if (stub->isFallback()) {
    return nullptr;
  }
  ICStub* next = stub->next();
  if (next->getEnteredCount() != 0) {
    return nullptr;
  }
  if (!next->isFallback()) {
    ICStub* nextNext = next->next();
    if (!nextNext->isFallback() || nextNext->getEnteredCount() != 0) {
      return nullptr;
    }
  }
  return stub;
}

Maybe<InlinableCallData> FindInlinableCallData(ICStub* stub) {
  Maybe<InlinableCallData> data;

  const CacheIRStubInfo* stubInfo = stub->cacheIRStubInfo();
  const uint8_t* stubData = stub->cacheIRStubData();

  ObjOperandId calleeGuardOperand;
  CallFlags flags;
  JSFunction* target = nullptr;

  CacheIRReader reader(stubInfo);
  while (reader.more()) {
    const uint8_t* opStart = reader.currentPosition();

    CacheOp op = reader.readOp();
    uint32_t argLength = CacheIROpArgLengths[size_t(op)];
    mozilla::DebugOnly<const uint8_t*> argStart = reader.currentPosition();

    switch (op) {
      case CacheOp::GuardSpecificFunction: {
        // If we see a guard, remember which operand we are guarding.
        MOZ_ASSERT(data.isNothing());
        calleeGuardOperand = reader.objOperandId();
        uint32_t targetOffset = reader.stubOffset();
        mozilla::Unused << reader.stubOffset();  // nargsAndFlags
        uintptr_t rawTarget = stubInfo->getStubRawWord(stubData, targetOffset);
        target = reinterpret_cast<JSFunction*>(rawTarget);
        break;
      }
      case CacheOp::GuardFunctionScript: {
        MOZ_ASSERT(data.isNothing());
        calleeGuardOperand = reader.objOperandId();
        uint32_t targetOffset = reader.stubOffset();
        uintptr_t rawTarget = stubInfo->getStubRawWord(stubData, targetOffset);
        target = reinterpret_cast<BaseScript*>(rawTarget)->function();
        mozilla::Unused << reader.stubOffset();  // nargsAndFlags
        break;
      }
      case CacheOp::CallScriptedFunction: {
        // If we see a call, check if `callee` is the previously guarded
        // operand. If it is, we know the target and can inline.
        ObjOperandId calleeOperand = reader.objOperandId();
        mozilla::DebugOnly<Int32OperandId> argcId = reader.int32OperandId();
        flags = reader.callFlags();

        if (calleeOperand == calleeGuardOperand) {
          MOZ_ASSERT(static_cast<OperandId&>(argcId).id() == 0);
          MOZ_ASSERT(data.isNothing());
          data.emplace();
          data->endOfSharedPrefix = opStart;
        }
        break;
      }
      case CacheOp::CallInlinedFunction: {
        ObjOperandId calleeOperand = reader.objOperandId();
        mozilla::DebugOnly<Int32OperandId> argcId = reader.int32OperandId();
        uint32_t icScriptOffset = reader.stubOffset();
        flags = reader.callFlags();

        if (calleeOperand == calleeGuardOperand) {
          MOZ_ASSERT(static_cast<OperandId&>(argcId).id() == 0);
          MOZ_ASSERT(data.isNothing());
          data.emplace();
          data->endOfSharedPrefix = opStart;
          uintptr_t rawICScript =
              stubInfo->getStubRawWord(stubData, icScriptOffset);
          data->icScript = reinterpret_cast<ICScript*>(rawICScript);
        }
        break;
      }
      default:
        if (data.isSome()) {
          MOZ_ASSERT(op == CacheOp::ReturnFromIC ||
                     op == CacheOp::TypeMonitorResult);
        }
        reader.skip(argLength);
        break;
    }
    MOZ_ASSERT(argStart + argLength == reader.currentPosition());
  }

  if (data.isSome()) {
    data->calleeOperand = calleeGuardOperand;
    data->callFlags = flags;
    data->target = target;
  }
  return data;
}

/*static*/
bool TrialInliner::canInline(JSFunction* target, HandleScript caller) {
  if (!target->hasJitScript()) {
    return false;
  }
  JSScript* script = target->nonLazyScript();
  if (!script->jitScript()->hasBaselineScript() || script->uninlineable() ||
      !script->canIonCompile() || script->needsArgsObj() ||
      script->isDebuggee()) {
    return false;
  }
  // Don't inline cross-realm calls.
  if (target->realm() != caller->realm()) {
    return false;
  }
  return true;
}

bool TrialInliner::shouldInline(JSFunction* target, ICStub* stub,
                                BytecodeLocation loc) {
  if (!canInline(target, script_)) {
    return false;
  }
  JitSpew(JitSpew_WarpTrialInlining,
          "Inlining candidate JSOp::%s: callee script %s:%u:%u",
          CodeName(loc.getOp()), target->nonLazyScript()->filename(),
          target->nonLazyScript()->lineno(), target->nonLazyScript()->column());

  // Don't inline (direct) recursive calls. This still allows recursion if
  // called through another function (f => g => f).
  JSScript* targetScript = target->nonLazyScript();
  if (script_ == targetScript) {
    JitSpew(JitSpew_WarpTrialInlining, "SKIP: recursion");
    return false;
  }

  // Don't inline if the callee has a loop that was hot enough to enter Warp
  // via OSR. This helps prevent getting stuck in Baseline code for a long time.
  if (targetScript->jitScript()->hadIonOSR()) {
    JitSpew(JitSpew_WarpTrialInlining, "SKIP: had OSR");
    return false;
  }

  // Ensure the total bytecode size does not exceed ionMaxScriptSize.
  size_t newTotalSize = root_->totalBytecodeSize() + targetScript->length();
  if (newTotalSize > JitOptions.ionMaxScriptSize) {
    JitSpew(JitSpew_WarpTrialInlining, "SKIP: total size too big");
    return false;
  }

  uint32_t entryCount = stub->getEnteredCount();
  if (entryCount < JitOptions.inliningEntryThreshold) {
    JitSpew(JitSpew_WarpTrialInlining, "SKIP: Entry count is %u (minimum %u)",
            unsigned(entryCount), unsigned(JitOptions.inliningEntryThreshold));
    return false;
  }

  if (!JitOptions.isSmallFunction(targetScript)) {
    JitSpew(JitSpew_WarpTrialInlining, "SKIP: Length is %u (maximum %u)",
            unsigned(targetScript->length()),
            unsigned(JitOptions.smallFunctionMaxBytecodeLength));
    return false;
  }

  if (TooManyFormalArguments(target->nargs())) {
    JitSpew(JitSpew_WarpTrialInlining, "SKIP: Too many formal arguments: %u",
            unsigned(target->nargs()));
    return false;
  }

  if (TooManyFormalArguments(loc.getCallArgc())) {
    JitSpew(JitSpew_WarpTrialInlining, "SKIP: argc too large: %u",
            unsigned(loc.getCallArgc()));
    return false;
  }

  return true;
}

ICScript* TrialInliner::createInlinedICScript(JSFunction* target,
                                              BytecodeLocation loc) {
  MOZ_ASSERT(target->hasJitEntry());
  MOZ_ASSERT(target->hasJitScript());

  JSScript* targetScript = target->baseScript()->asJSScript();

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

  uint32_t initialWarmUpCount = JitOptions.trialInliningInitialWarmUpCount;

  uint32_t depth = icScript_->depth() + 1;
  UniquePtr<ICScript> inlinedICScript(
      new (raw) ICScript(initialWarmUpCount, allocSize, depth, root_));

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

  uint32_t pcOffset = loc.bytecodeToOffset(script_);
  ICScript* result = inlinedICScript.get();
  if (!icScript_->addInlinedChild(cx(), std::move(inlinedICScript), pcOffset)) {
    return nullptr;
  }
  MOZ_ASSERT(result->numICEntries() == targetScript->numICEntries());

  root_->addToTotalBytecodeSize(targetScript->length());

  JitSpew(JitSpew_WarpTrialInlining,
          "Outer ICScript: %p Inner ICScript: %p pcOffset: %u", icScript_,
          result, pcOffset);

  return result;
}

bool TrialInliner::maybeInlineCall(const ICEntry& entry, BytecodeLocation loc) {
  ICStub* stub = maybeSingleStub(entry);
  if (!stub) {
    return true;
  }

  // Ensure that we haven't already trial-inlined a different stub.
  uint32_t pcOffset = loc.bytecodeToOffset(script_);
  if (!stub->next()->isFallback()) {
    if (icScript_->hasInlinedChild(pcOffset)) {
      return true;
    }
  }

  // Look for a CallScriptedFunction with a known target.
  Maybe<InlinableCallData> data = FindInlinableCallData(stub);
  if (data.isNothing()) {
    return true;
  }
  // Ensure that we haven't already trial-inlined this stub.
  if (data->icScript) {
    return true;
  }

  MOZ_ASSERT(!icScript_->hasInlinedChild(pcOffset));

  // Decide whether to inline the target.
  if (!shouldInline(data->target, stub, loc)) {
    return true;
  }

  // We only inline FunCall if we are calling the js::fun_call builtin.
  MOZ_ASSERT_IF(loc.getOp() == JSOp::FunCall,
                data->callFlags.getArgFormat() == CallFlags::FunCall);

  ICScript* newICScript = createInlinedICScript(data->target, loc);
  if (!newICScript) {
    return false;
  }

  CacheIRWriter writer(cx());
  Int32OperandId argcId(writer.setInputOperandId(0));
  cloneSharedPrefix(stub, data->endOfSharedPrefix, writer);

  writer.callInlinedFunction(data->calleeOperand, argcId, newICScript,
                             data->callFlags);
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
      case JSOp::CallIter:
      case JSOp::FunCall:
      case JSOp::New:
      case JSOp::SuperCall:
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

void InliningRoot::removeInlinedScript(ICScript* icScript) {
  inlinedScripts_.eraseIf(
      [icScript](const UniquePtr<ICScript>& script) -> bool {
        return script.get() == icScript;
      });
}

void InliningRoot::trace(JSTracer* trc) {
  TraceEdge(trc, &owningScript_, "inlining-root-owning-script");
  for (auto& inlinedScript : inlinedScripts_) {
    inlinedScript->trace(trc);
  }
}

void InliningRoot::purgeOptimizedStubs(Zone* zone) {
  for (auto& inlinedScript : inlinedScripts_) {
    inlinedScript->purgeOptimizedStubs(zone);
  }
}

void InliningRoot::resetWarmUpCounts(uint32_t count) {
  for (auto& inlinedScript : inlinedScripts_) {
    inlinedScript->resetWarmUpCount(count);
  }
}

}  // namespace jit
}  // namespace js
