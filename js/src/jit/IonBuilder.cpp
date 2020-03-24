/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonBuilder.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/ScopeExit.h"

#include <algorithm>

#include "builtin/Eval.h"
#include "builtin/TypedObject.h"
#include "frontend/SourceNotes.h"
#include "jit/BaselineFrame.h"
#include "jit/BaselineInspector.h"
#include "jit/CacheIR.h"
#include "jit/Ion.h"
#include "jit/IonOptimizationLevels.h"
#include "jit/JitSpewer.h"
#include "jit/Lowering.h"
#include "jit/MIRGraph.h"
#include "util/CheckedArithmetic.h"
#include "vm/ArgumentsObject.h"
#include "vm/BytecodeIterator.h"
#include "vm/BytecodeLocation.h"
#include "vm/BytecodeUtil.h"
#include "vm/EnvironmentObject.h"
#include "vm/Instrumentation.h"
#include "vm/Opcodes.h"
#include "vm/RegExpStatics.h"
#include "vm/SelfHosting.h"
#include "vm/TraceLogging.h"

#include "gc/Nursery-inl.h"
#include "jit/CompileInfo-inl.h"
#include "jit/shared/Lowering-shared-inl.h"
#include "vm/BytecodeIterator-inl.h"
#include "vm/BytecodeLocation-inl.h"
#include "vm/BytecodeUtil-inl.h"
#include "vm/EnvironmentObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ObjectGroup-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::AssertedCast;
using mozilla::DebugOnly;
using mozilla::Maybe;
using mozilla::Nothing;

class jit::BaselineFrameInspector {
 public:
  TypeSet::Type thisType;
  JSObject* singletonEnvChain;

  Vector<TypeSet::Type, 4, JitAllocPolicy> argTypes;
  Vector<TypeSet::Type, 4, JitAllocPolicy> varTypes;

  explicit BaselineFrameInspector(TempAllocator* temp)
      : thisType(TypeSet::UndefinedType()),
        singletonEnvChain(nullptr),
        argTypes(*temp),
        varTypes(*temp) {}
};

BaselineFrameInspector* jit::NewBaselineFrameInspector(TempAllocator* temp,
                                                       BaselineFrame* frame,
                                                       uint32_t frameSize) {
  MOZ_ASSERT(frame);

  BaselineFrameInspector* inspector =
      temp->lifoAlloc()->new_<BaselineFrameInspector>(temp);
  if (!inspector) {
    return nullptr;
  }

  // Note: copying the actual values into a temporary structure for use
  // during compilation could capture nursery pointers, so the values' types
  // are recorded instead.

  if (frame->isFunctionFrame()) {
    inspector->thisType =
        TypeSet::GetMaybeUntrackedValueType(frame->thisArgument());
  }

  if (frame->environmentChain()->isSingleton()) {
    inspector->singletonEnvChain = frame->environmentChain();
  }

  JSScript* script = frame->script();

  if (script->function()) {
    if (!inspector->argTypes.reserve(frame->numFormalArgs())) {
      return nullptr;
    }
    for (size_t i = 0; i < frame->numFormalArgs(); i++) {
      if (script->formalIsAliased(i)) {
        inspector->argTypes.infallibleAppend(TypeSet::UndefinedType());
      } else if (!script->argsObjAliasesFormals()) {
        TypeSet::Type type =
            TypeSet::GetMaybeUntrackedValueType(frame->unaliasedFormal(i));
        inspector->argTypes.infallibleAppend(type);
      } else if (frame->hasArgsObj()) {
        TypeSet::Type type =
            TypeSet::GetMaybeUntrackedValueType(frame->argsObj().arg(i));
        inspector->argTypes.infallibleAppend(type);
      } else {
        inspector->argTypes.infallibleAppend(TypeSet::UndefinedType());
      }
    }
  }

  uint32_t numValueSlots = frame->numValueSlots(frameSize);
  if (!inspector->varTypes.reserve(numValueSlots)) {
    return nullptr;
  }
  for (size_t i = 0; i < numValueSlots; i++) {
    TypeSet::Type type =
        TypeSet::GetMaybeUntrackedValueType(*frame->valueSlot(i));
    inspector->varTypes.infallibleAppend(type);
  }

  return inspector;
}

IonBuilder::IonBuilder(JSContext* analysisContext, MIRGenerator& mirGen,
                       CompileInfo* info, CompilerConstraintList* constraints,
                       BaselineInspector* inspector,
                       BaselineFrameInspector* baselineFrame,
                       size_t inliningDepth, uint32_t loopDepth)
    : actionableAbortScript_(nullptr),
      actionableAbortPc_(nullptr),
      actionableAbortMessage_(nullptr),
      analysisContext(analysisContext),
      baselineFrame_(baselineFrame),
      constraints_(constraints),
      mirGen_(mirGen),
      tiOracle_(this, constraints),
      realm(mirGen.realm),
      info_(info),
      optimizationInfo_(&mirGen.optimizationInfo()),
      alloc_(&mirGen.alloc()),
      graph_(&mirGen.graph()),
      thisTypes(nullptr),
      argTypes(nullptr),
      typeArray(nullptr),
      typeArrayHint(0),
      bytecodeTypeMap(nullptr),
      loopDepth_(loopDepth),
      loopStack_(*alloc_),
      trackedOptimizationSites_(*alloc_),
      abortedPreliminaryGroups_(*alloc_),
      callerResumePoint_(nullptr),
      callerBuilder_(nullptr),
      iterators_(*alloc_),
      loopHeaders_(*alloc_),
      inspector(inspector),
      inliningDepth_(inliningDepth),
      inlinedBytecodeLength_(0),
      numLoopRestarts_(0),
      failedBoundsCheck_(info_->script()->failedBoundsCheck()),
      failedShapeGuard_(info_->script()->failedShapeGuard()),
      failedLexicalCheck_(info_->script()->failedLexicalCheck()),
#ifdef DEBUG
      hasLazyArguments_(false),
#endif
      inlineCallInfo_(nullptr),
      maybeFallbackFunctionGetter_(nullptr) {
  script_ = info_->script();
  pc = info_->startPC();

  // The script must have a JitScript. Compilation requires a BaselineScript
  // too.
  MOZ_ASSERT(script_->hasJitScript());
  MOZ_ASSERT_IF(!info_->isAnalysis(), script_->hasBaselineScript());

  MOZ_ASSERT(!!analysisContext ==
             (info_->analysisMode() == Analysis_DefiniteProperties));
  MOZ_ASSERT(script_->numBytecodeTypeSets() < JSScript::MaxBytecodeTypeSets);

  if (!info_->isAnalysis()) {
    script()->jitScript()->setIonCompiledOrInlined();
  }
}

mozilla::GenericErrorResult<AbortReason> IonBuilder::abort(AbortReason r) {
  auto res = mirGen_.abort(r);
#ifdef DEBUG
  JitSpew(JitSpew_IonAbort, "aborted @ %s:%d", script()->filename(),
          PCToLineNumber(script(), pc));
#else
  JitSpew(JitSpew_IonAbort, "aborted @ %s", script()->filename());
#endif
  return res;
}

mozilla::GenericErrorResult<AbortReason> IonBuilder::abort(AbortReason r,
                                                           const char* message,
                                                           ...) {
  // Don't call PCToLineNumber in release builds.
  va_list ap;
  va_start(ap, message);
  auto res = mirGen_.abortFmt(r, message, ap);
  va_end(ap);
#ifdef DEBUG
  JitSpew(JitSpew_IonAbort, "aborted @ %s:%d", script()->filename(),
          PCToLineNumber(script(), pc));
#else
  JitSpew(JitSpew_IonAbort, "aborted @ %s", script()->filename());
#endif
  return res;
}

IonBuilder* IonBuilder::outermostBuilder() {
  IonBuilder* builder = this;
  while (builder->callerBuilder_) {
    builder = builder->callerBuilder_;
  }
  return builder;
}

void IonBuilder::spew(const char* message) {
  // Don't call PCToLineNumber in release builds.
#ifdef DEBUG
  JitSpew(JitSpew_IonMIR, "%s @ %s:%d", message, script()->filename(),
          PCToLineNumber(script(), pc));
#endif
}

JSFunction* IonBuilder::getSingleCallTarget(TemporaryTypeSet* calleeTypes) {
  if (!calleeTypes) {
    return nullptr;
  }

  TemporaryTypeSet::ObjectKey* key = calleeTypes->maybeSingleObject();
  if (!key || key->clasp() != &JSFunction::class_) {
    return nullptr;
  }

  if (key->isSingleton()) {
    return &key->singleton()->as<JSFunction>();
  }

  if (JSFunction* fun = key->group()->maybeInterpretedFunction()) {
    return fun;
  }

  return nullptr;
}

AbortReasonOr<Ok> IonBuilder::getPolyCallTargets(TemporaryTypeSet* calleeTypes,
                                                 bool constructing,
                                                 InliningTargets& targets,
                                                 uint32_t maxTargets) {
  MOZ_ASSERT(targets.empty());

  if (!calleeTypes) {
    return Ok();
  }

  if (calleeTypes->baseFlags() != 0) {
    return Ok();
  }

  unsigned objCount = calleeTypes->getObjectCount();

  if (objCount == 0 || objCount > maxTargets) {
    return Ok();
  }

  if (!targets.reserve(objCount)) {
    return abort(AbortReason::Alloc);
  }
  for (unsigned i = 0; i < objCount; i++) {
    JSObject* obj = calleeTypes->getSingleton(i);
    ObjectGroup* group = nullptr;
    if (obj) {
      MOZ_ASSERT(obj->isSingleton());
    } else {
      group = calleeTypes->getGroup(i);
      if (!group) {
        continue;
      }

      obj = group->maybeInterpretedFunction();
      if (!obj) {
        targets.clear();
        return Ok();
      }

      MOZ_ASSERT(!obj->isSingleton());
    }

    // Don't optimize if the callee is not callable or constructable per
    // the manner it is being invoked, so that CallKnown does not have to
    // handle these cases (they will always throw).
    if (constructing ? !obj->isConstructor() : !obj->isCallable()) {
      targets.clear();
      return Ok();
    }

    targets.infallibleAppend(InliningTarget(obj, group));
  }

  return Ok();
}

IonBuilder::InliningDecision IonBuilder::DontInline(JSScript* targetScript,
                                                    const char* reason) {
  if (targetScript) {
    JitSpew(JitSpew_Inlining, "Cannot inline %s:%u:%u %s",
            targetScript->filename(), targetScript->lineno(),
            targetScript->column(), reason);
  } else {
    JitSpew(JitSpew_Inlining, "Cannot inline: %s", reason);
  }

  return InliningDecision_DontInline;
}

/*
 * |hasCommonInliningPath| determines whether the current inlining path has been
 * seen before based on the sequence of scripts in the chain of |IonBuilder|s.
 *
 * An inlining path for a function |f| is the sequence of functions whose
 * inlinings precede |f| up to any previous occurrences of |f|.
 * So, if we have the chain of inlinings
 *
 *          f1 -> f2 -> f -> f3 -> f4 -> f5 -> f
 *          --------         --------------
 *
 * the inlining paths for |f| are [f2, f1] and [f5, f4, f3].
 * When attempting to inline |f|, we find all existing inlining paths for |f|
 * and check whether they share a common prefix with the path created were |f|
 * inlined.
 *
 * For example, given mutually recursive functions |f| and |g|, a possible
 * inlining is
 *
 *                           +---- Inlining stopped here...
 *                           |
 *                           v
 *          a -> f -> g -> f \ -> g -> f -> g -> ...
 *
 * where the vertical bar denotes the termination of inlining.
 * Inlining is terminated because we have already observed the inlining path
 * [f] when inlining function |g|. Note that this will inline recursive
 * functions such as |fib| only one level, as |fib| has a zero length inlining
 * path which trivially prefixes all inlining paths.
 *
 */
bool IonBuilder::hasCommonInliningPath(const JSScript* scriptToInline) {
  // Find all previous inlinings of the |scriptToInline| and check for common
  // inlining paths with the top of the inlining stack.
  for (IonBuilder* it = this->callerBuilder_; it; it = it->callerBuilder_) {
    if (it->script() != scriptToInline) {
      continue;
    }

    // This only needs to check the top of each stack for a match,
    // as a match of length one ensures a common prefix.
    IonBuilder* path = it->callerBuilder_;
    if (!path || this->script() == path->script()) {
      return true;
    }
  }

  return false;
}

IonBuilder::InliningDecision IonBuilder::canInlineTarget(JSFunction* target,
                                                         CallInfo& callInfo) {
  if (!optimizationInfo().inlineInterpreted()) {
    return InliningDecision_DontInline;
  }

  if (TraceLogTextIdEnabled(TraceLogger_InlinedScripts)) {
    return DontInline(nullptr,
                      "Tracelogging of inlined scripts is enabled"
                      "but Tracelogger cannot do that yet.");
  }

  if (!target->isInterpreted()) {
    return DontInline(nullptr, "Non-interpreted target");
  }

  // Never inline scripted cross-realm calls.
  if (target->realm() != script()->realm()) {
    return DontInline(nullptr, "Cross-realm call");
  }

  if (info().analysisMode() != Analysis_DefiniteProperties) {
    // If |this| or an argument has an empty resultTypeSet, don't bother
    // inlining, as the call is currently unreachable due to incomplete type
    // information. This does not apply to the definite properties analysis,
    // in that case we want to inline anyway.

    if (callInfo.thisArg()->emptyResultTypeSet()) {
      return DontInline(nullptr, "Empty TypeSet for |this|");
    }

    for (size_t i = 0; i < callInfo.argc(); i++) {
      if (callInfo.getArg(i)->emptyResultTypeSet()) {
        return DontInline(nullptr, "Empty TypeSet for argument");
      }
    }
  }

  // Allow constructing lazy scripts when performing the definite properties
  // analysis, as baseline has not been used to warm the caller up yet.
  if (target->isInterpreted() &&
      info().analysisMode() == Analysis_DefiniteProperties) {
    RootedFunction fun(analysisContext, target);
    RootedScript script(analysisContext,
                        JSFunction::getOrCreateScript(analysisContext, fun));
    if (!script) {
      return InliningDecision_Error;
    }

    if (CanBaselineInterpretScript(script)) {
      AutoKeepJitScripts keepJitScript(analysisContext);
      if (!script->ensureHasJitScript(analysisContext, keepJitScript)) {
        return InliningDecision_Error;
      }
    }
  }

  if (!target->hasBytecode()) {
    return DontInline(nullptr, "Lazy script");
  }

  JSScript* inlineScript = target->nonLazyScript();
  if (callInfo.constructing()) {
    if (!target->isConstructor()) {
      return DontInline(inlineScript, "Callee is not a constructor");
    }

    // Don't inline if creating |this| for this target is complicated, for
    // example when the newTarget.prototype lookup may be effectful.
    if (!target->constructorNeedsUninitializedThis() &&
        callInfo.getNewTarget() != callInfo.fun()) {
      JSFunction* newTargetFun =
          getSingleCallTarget(callInfo.getNewTarget()->resultTypeSet());
      if (!newTargetFun) {
        return DontInline(inlineScript, "Constructing with unknown newTarget");
      }
      if (!newTargetFun->hasNonConfigurablePrototypeDataProperty()) {
        return DontInline(inlineScript,
                          "Constructing with effectful newTarget.prototype");
      }
    } else {
      // At this point, the target is either a function that requires an
      // uninitialized-this (bound function or derived class constructor) or a
      // scripted constructor with a non-configurable .prototype data property
      // (self-hosted built-in constructor, non-self-hosted scripted function).
      MOZ_ASSERT(target->constructorNeedsUninitializedThis() ||
                 target->hasNonConfigurablePrototypeDataProperty());
    }
  }

  if (!callInfo.constructing() && target->isClassConstructor()) {
    return DontInline(inlineScript, "Not constructing class constructor");
  }

  if (!CanIonInlineScript(inlineScript)) {
    return DontInline(inlineScript, "Disabled Ion compilation");
  }

  if (info().isAnalysis()) {
    // Analysis requires only a JitScript.
    if (!inlineScript->hasJitScript()) {
      return DontInline(inlineScript, "No JitScript");
    }
  } else {
    // Compilation requires a BaselineScript.
    if (!inlineScript->hasBaselineScript()) {
      return DontInline(inlineScript, "No baseline jitcode");
    }
  }

  // Don't inline functions with a higher optimization level.
  if (!isHighestOptimizationLevel()) {
    OptimizationLevel level = optimizationLevel();
    if (inlineScript->hasIonScript() &&
        (inlineScript->ionScript()->isRecompiling() ||
         inlineScript->ionScript()->optimizationLevel() > level)) {
      return DontInline(inlineScript, "More optimized");
    }
    if (IonOptimizations.levelForScript(inlineScript, nullptr) > level) {
      return DontInline(inlineScript, "Should be more optimized");
    }
  }

  if (TooManyFormalArguments(target->nargs())) {
    return DontInline(inlineScript, "Too many args");
  }

  // We check the number of actual arguments against the maximum number of
  // formal arguments as we do not want to encode all actual arguments in the
  // callerResumePoint.
  if (TooManyFormalArguments(callInfo.argc())) {
    return DontInline(inlineScript, "Too many actual args");
  }

  if (hasCommonInliningPath(inlineScript)) {
    return DontInline(inlineScript, "Common inlining path");
  }

  if (inlineScript->uninlineable()) {
    return DontInline(inlineScript, "Uninlineable script");
  }

  if (inlineScript->needsArgsObj()) {
    return DontInline(inlineScript, "Script that needs an arguments object");
  }

  if (inlineScript->isDebuggee()) {
    return DontInline(inlineScript, "Script is debuggee");
  }

  return InliningDecision_Inline;
}

AbortReasonOr<Ok> IonBuilder::analyzeNewLoopTypes(MBasicBlock* entry) {
  MOZ_ASSERT(!entry->isDead());
  MOZ_ASSERT(JSOp(*pc) == JSOp::LoopHead);

  // The phi inputs at the loop head only reflect types for variables that
  // were present at the start of the loop. If the variable changes to a new
  // type within the loop body, and that type is carried around to the loop
  // head, then we need to know about the new type up front.
  //
  // Since SSA information hasn't been constructed for the loop body yet, we
  // need a separate analysis to pick out the types that might flow around
  // the loop header. This is a best-effort analysis that may either over-
  // or under-approximate the set of such types.
  //
  // Over-approximating the types may lead to inefficient generated code, and
  // under-approximating the types will cause the loop body to be analyzed
  // multiple times as the correct types are deduced (see finishLoop).

  // If we restarted processing of an outer loop then get loop header types
  // directly from the last time we have previously processed this loop. This
  // both avoids repeated work from the bytecode traverse below, and will
  // also pick up types discovered while previously building the loop body.
  bool foundEntry = false;
  for (size_t i = 0; i < loopHeaders_.length(); i++) {
    if (loopHeaders_[i].pc == pc) {
      MBasicBlock* oldEntry = loopHeaders_[i].header;

      // If this block has been discarded, its resume points will have
      // already discarded their operands.
      if (oldEntry->isDead()) {
        loopHeaders_[i].header = entry;
        foundEntry = true;
        break;
      }

      MResumePoint* oldEntryRp = oldEntry->entryResumePoint();
      size_t stackDepth = oldEntryRp->stackDepth();
      for (size_t slot = 0; slot < stackDepth; slot++) {
        MDefinition* oldDef = oldEntryRp->getOperand(slot);
        if (!oldDef->isPhi()) {
          MOZ_ASSERT(oldDef->block()->id() < oldEntry->id());
          MOZ_ASSERT(oldDef == entry->getSlot(slot));
          continue;
        }
        MPhi* oldPhi = oldDef->toPhi();
        MPhi* newPhi = entry->getSlot(slot)->toPhi();
        if (!newPhi->addBackedgeType(alloc(), oldPhi->type(),
                                     oldPhi->resultTypeSet())) {
          return abort(AbortReason::Alloc);
        }
      }

      // Update the most recent header for this loop encountered, in case
      // new types flow to the phis and the loop is processed at least
      // three times.
      loopHeaders_[i].header = entry;
      return Ok();
    }
  }
  if (!foundEntry) {
    if (!loopHeaders_.append(LoopHeader(pc, entry))) {
      return abort(AbortReason::Alloc);
    }
  }

  // Get the start and end bytecode locations.
  BytecodeLocation start(script_, pc);
  BytecodeLocation end(script_, script_->codeEnd());

  // Iterate the bytecode quickly to seed possible types in the loopheader.
  Maybe<BytecodeLocation> last;
  Maybe<BytecodeLocation> earlier;

  for (auto it : BytecodeLocationRange(start, end)) {
    if (IsBackedgeForLoopHead(it.toRawBytecode(), pc)) {
      break;
    }
    MOZ_TRY(analyzeNewLoopTypesForLocation(entry, it, last, earlier));
    earlier = last;
    last = mozilla::Some(it);
  }

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::analyzeNewLoopTypesForLocation(
    MBasicBlock* entry, const BytecodeLocation loc,
    const Maybe<BytecodeLocation>& last_,
    const Maybe<BytecodeLocation>& earlier) {
  // Unfortunately Maybe<> cannot be passed as by-value argument so make a copy
  // here.
  Maybe<BytecodeLocation> last = last_;

  // We're only interested in JSOp::SetLocal and JSOp::SetArg.
  uint32_t slot;
  if (loc.is(JSOp::SetLocal)) {
    slot = info().localSlot(loc.local());
  } else if (loc.is(JSOp::SetArg)) {
    slot = info().argSlotUnchecked(loc.arg());
  } else {
    return Ok();
  }
  if (slot >= info().firstStackSlot()) {
    return Ok();
  }

  // Ensure there is a |last| instruction.
  if (!last) {
    return Ok();
  }
  MOZ_ASSERT(last->isValid(script_));

  // Analyze the |last| bytecode instruction to try to dermine the type of this
  // local/argument.

  MPhi* phi = entry->getSlot(slot)->toPhi();

  auto addPhiBackedgeType =
      [&](MIRType type, TemporaryTypeSet* typeSet) -> AbortReasonOr<Ok> {
    if (!phi->addBackedgeType(alloc(), type, typeSet)) {
      return abort(AbortReason::Alloc);
    }
    return Ok();
  };

  // If it's a JSOp::Pos or JSOp::ToNumeric, use its operand instead.
  if (last->is(JSOp::Pos) || last->is(JSOp::ToNumeric)) {
    MOZ_ASSERT(earlier);
    last = earlier;
  }

  // If the |last| op had a TypeSet, use it.
  if (last->opHasTypeSet()) {
    TemporaryTypeSet* typeSet = bytecodeTypes(last->toRawBytecode());
    if (typeSet->empty()) {
      return Ok();
    }
    return addPhiBackedgeType(typeSet->getKnownMIRType(), typeSet);
  }

  // If the |last| op was a JSOp::GetLocal or JSOp::GetArg, use that slot's
  // type.
  if (last->is(JSOp::GetLocal) || last->is(JSOp::GetArg)) {
    uint32_t slot = (last->is(JSOp::GetLocal))
                        ? info().localSlot(last->local())
                        : info().argSlotUnchecked(last->arg());
    if (slot >= info().firstStackSlot()) {
      return Ok();
    }
    MPhi* otherPhi = entry->getSlot(slot)->toPhi();
    if (!otherPhi->hasBackedgeType()) {
      return Ok();
    }
    return addPhiBackedgeType(otherPhi->type(), otherPhi->resultTypeSet());
  }

  // If the |last| op has a known type (determined statically or from
  // BaselineInspector), use that.
  MIRType type = MIRType::None;
  switch (last->getOp()) {
    case JSOp::Void:
    case JSOp::Undefined:
      type = MIRType::Undefined;
      break;
    case JSOp::GImplicitThis:
      if (!script()->hasNonSyntacticScope()) {
        type = MIRType::Undefined;
      }
      break;
    case JSOp::Null:
      type = MIRType::Null;
      break;
    case JSOp::Zero:
    case JSOp::One:
    case JSOp::Int8:
    case JSOp::Int32:
    case JSOp::Uint16:
    case JSOp::Uint24:
    case JSOp::ResumeIndex:
      type = MIRType::Int32;
      break;
    case JSOp::BitAnd:
    case JSOp::BitOr:
    case JSOp::BitXor:
    case JSOp::BitNot:
    case JSOp::Rsh:
    case JSOp::Lsh:
      type = inspector->expectedResultType(last->toRawBytecode());
      break;
    case JSOp::Ursh:
      // Unsigned right shift is not applicable to BigInts, so we don't need
      // to query the baseline inspector for the possible result types.
      type = MIRType::Int32;
      break;
    case JSOp::False:
    case JSOp::True:
    case JSOp::Eq:
    case JSOp::Ne:
    case JSOp::Lt:
    case JSOp::Le:
    case JSOp::Gt:
    case JSOp::Ge:
    case JSOp::Not:
    case JSOp::StrictEq:
    case JSOp::StrictNe:
    case JSOp::In:
    case JSOp::Instanceof:
    case JSOp::HasOwn:
      type = MIRType::Boolean;
      break;
    case JSOp::Double:
      type = MIRType::Double;
      break;
    case JSOp::IterNext:
    case JSOp::String:
    case JSOp::ToString:
    case JSOp::Typeof:
    case JSOp::TypeofExpr:
      type = MIRType::String;
      break;
    case JSOp::Symbol:
      type = MIRType::Symbol;
      break;
    case JSOp::Add:
    case JSOp::Sub:
    case JSOp::Mul:
    case JSOp::Div:
    case JSOp::Mod:
    case JSOp::Neg:
    case JSOp::Inc:
    case JSOp::Dec:
      type = inspector->expectedResultType(last->toRawBytecode());
      break;
    case JSOp::BigInt:
      type = MIRType::BigInt;
      break;
    default:
      break;
  }

  if (type == MIRType::None) {
    return Ok();
  }

  return addPhiBackedgeType(type, /* typeSet = */ nullptr);
}

AbortReasonOr<Ok> IonBuilder::init() {
  {
    LifoAlloc::AutoFallibleScope fallibleAllocator(alloc().lifoAlloc());
    if (!JitScript::FreezeTypeSets(constraints(), script(), &thisTypes,
                                   &argTypes, &typeArray)) {
      return abort(AbortReason::Alloc);
    }
  }

  if (!alloc().ensureBallast()) {
    return abort(AbortReason::Alloc);
  }

  {
    JSContext* cx = TlsContext.get();
    RootedScript rootedScript(cx, script());
    if (!rootedScript->jitScript()->ensureHasCachedIonData(cx, rootedScript)) {
      return abort(AbortReason::Error);
    }
  }

  if (inlineCallInfo_) {
    // If we're inlining, the actual this/argument types are not necessarily
    // a subset of the script's observed types. |argTypes| is never accessed
    // for inlined scripts, so we just null it.
    thisTypes = inlineCallInfo_->thisArg()->resultTypeSet();
    argTypes = nullptr;
  }

  bytecodeTypeMap = script()->jitScript()->bytecodeTypeMap();

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::build() {
  // Spew IC info for inlined script, but only when actually compiling,
  // not when analyzing it.
#ifdef JS_STRUCTURED_SPEW
  if (!info().isAnalysis()) {
    JitSpewBaselineICStats(script(), "To-Be-Compiled");
  }
#endif

  MOZ_TRY(init());

  // The JitScript-based inlining heuristics only affect the highest
  // optimization level. Other levels do almost no inlining and we don't want to
  // overwrite data from the highest optimization tier.
  if (isHighestOptimizationLevel()) {
    script()->jitScript()->resetMaxInliningDepth();
  }

  MBasicBlock* entry;
  MOZ_TRY_VAR(entry, newBlock(info().firstStackSlot(), pc));
  MOZ_TRY(setCurrentAndSpecializePhis(entry));

#ifdef JS_JITSPEW
  if (info().isAnalysis()) {
    JitSpew(JitSpew_IonScripts, "Analyzing script %s:%u:%u (%p) %s",
            script()->filename(), script()->lineno(), script()->column(),
            (void*)script(), AnalysisModeString(info().analysisMode()));
  } else {
    JitSpew(JitSpew_IonScripts,
            "%sompiling script %s:%u:%u (%p) (warmup-counter=%" PRIu32
            ", level=%s)",
            (script()->hasIonScript() ? "Rec" : "C"), script()->filename(),
            script()->lineno(), script()->column(), (void*)script(),
            script()->getWarmUpCount(),
            OptimizationLevelString(optimizationLevel()));
  }
#endif

  MOZ_TRY(initParameters());
  initLocals();

  // Initialize something for the env chain. We can bail out before the
  // start instruction, but the snapshot is encoded *at* the start
  // instruction, which means generating any code that could load into
  // registers is illegal.
  MInstruction* env = MConstant::New(alloc(), UndefinedValue());
  current->add(env);
  current->initSlot(info().environmentChainSlot(), env);

  // Initialize the return value.
  MInstruction* returnValue = MConstant::New(alloc(), UndefinedValue());
  current->add(returnValue);
  current->initSlot(info().returnValueSlot(), returnValue);

  // Initialize the arguments object slot to undefined if necessary.
  if (info().hasArguments()) {
    MInstruction* argsObj = MConstant::New(alloc(), UndefinedValue());
    current->add(argsObj);
    current->initSlot(info().argsObjSlot(), argsObj);
  }

  // Emit the start instruction, so we can begin real instructions.
  current->add(MStart::New(alloc()));

  // Guard against over-recursion. Do this before we start unboxing, since
  // this will create an OSI point that will read the incoming argument
  // values, which is nice to do before their last real use, to minimize
  // register/stack pressure.
  MCheckOverRecursed* check = MCheckOverRecursed::New(alloc());
  current->add(check);
  MResumePoint* entryRpCopy =
      MResumePoint::Copy(alloc(), current->entryResumePoint());
  if (!entryRpCopy) {
    return abort(AbortReason::Alloc);
  }
  check->setResumePoint(entryRpCopy);

  // Parameters have been checked to correspond to the typeset, now we unbox
  // what we can in an infallible manner.
  MOZ_TRY(rewriteParameters());

  // Check for redeclaration errors for global scripts.
  if (!info().funMaybeLazy() && !info().module() &&
      script()->bodyScope()->is<GlobalScope>() &&
      script()->bodyScope()->as<GlobalScope>().hasBindings()) {
    MGlobalNameConflictsCheck* redeclCheck =
        MGlobalNameConflictsCheck::New(alloc());
    current->add(redeclCheck);
    MResumePoint* entryRpCopy =
        MResumePoint::Copy(alloc(), current->entryResumePoint());
    if (!entryRpCopy) {
      return abort(AbortReason::Alloc);
    }
    redeclCheck->setResumePoint(entryRpCopy);
  }

  // It's safe to start emitting actual IR, so now build the env chain.
  MOZ_TRY(initEnvironmentChain());
  if (info().needsArgsObj()) {
    initArgumentsObject();
  }

  // The type analysis phase attempts to insert unbox operations near
  // definitions of values. It also attempts to replace uses in resume points
  // with the narrower, unboxed variants. However, we must prevent this
  // replacement from happening on values in the entry snapshot. Otherwise we
  // could get this:
  //
  //       v0 = MParameter(0)
  //       v1 = MParameter(1)
  //       --   ResumePoint(v2, v3)
  //       v2 = Unbox(v0, INT32)
  //       v3 = Unbox(v1, INT32)
  //
  // So we attach the initial resume point to each parameter, which the type
  // analysis explicitly checks (this is the same mechanism used for
  // effectful operations).
  for (uint32_t i = 0; i < info().endArgSlot(); i++) {
    MInstruction* ins = current->getEntrySlot(i)->toInstruction();
    if (ins->type() != MIRType::Value) {
      continue;
    }

    MResumePoint* entryRpCopy =
        MResumePoint::Copy(alloc(), current->entryResumePoint());
    if (!entryRpCopy) {
      return abort(AbortReason::Alloc);
    }
    ins->setResumePoint(entryRpCopy);
  }

#ifdef DEBUG
  // lazyArguments should never be accessed in |argsObjAliasesFormals| scripts.
  if (info().hasArguments() && !info().argsObjAliasesFormals()) {
    hasLazyArguments_ = true;
  }
#endif

  insertRecompileCheck(pc);

  auto clearLastPriorResumePoint = mozilla::MakeScopeExit([&] {
    // Discard unreferenced & pre-allocated resume points.
    replaceMaybeFallbackFunctionGetter(nullptr);
  });

  MOZ_TRY(traverseBytecode());

  if (isHighestOptimizationLevel() &&
      inlinedBytecodeLength_ > script_->jitScript()->inlinedBytecodeLength()) {
    script_->jitScript()->setInlinedBytecodeLength(inlinedBytecodeLength_);
  }

  MOZ_TRY(maybeAddOsrTypeBarriers());

  if (!MPhi::markIteratorPhis(iterators_)) {
    return abort(AbortReason::Alloc);
  }

  if (!info().isAnalysis() && !abortedPreliminaryGroups().empty()) {
    return abort(AbortReason::PreliminaryObjects);
  }

  MOZ_ASSERT(loopDepth_ == 0);
  MOZ_ASSERT(loopStack_.empty());
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::buildInline(IonBuilder* callerBuilder,
                                          MResumePoint* callerResumePoint,
                                          CallInfo& callInfo) {
  inlineCallInfo_ = &callInfo;

  // Spew IC info for inlined script, but only when actually compiling,
  // not when analyzing it.
#ifdef JS_STRUCTURED_SPEW
  if (!info().isAnalysis()) {
    JitSpewBaselineICStats(script(), "To-Be-Inlined");
  }
#endif

  MOZ_TRY(init());

  JitSpew(JitSpew_IonScripts, "Inlining script %s:%u:%u (%p)",
          script()->filename(), script()->lineno(), script()->column(),
          (void*)script());

  callerBuilder_ = callerBuilder;
  callerResumePoint_ = callerResumePoint;

  if (callerBuilder->failedBoundsCheck_) {
    failedBoundsCheck_ = true;
  }

  if (callerBuilder->failedShapeGuard_) {
    failedShapeGuard_ = true;
  }

  if (callerBuilder->failedLexicalCheck_) {
    failedLexicalCheck_ = true;
  }

  // Generate single entrance block.
  MBasicBlock* entry;
  MOZ_TRY_VAR(entry, newBlock(info().firstStackSlot(), pc));
  MOZ_TRY(setCurrentAndSpecializePhis(entry));

  current->setCallerResumePoint(callerResumePoint);

  // Connect the entrance block to the last block in the caller's graph.
  MBasicBlock* predecessor = callerBuilder->current;
  MOZ_ASSERT(predecessor == callerResumePoint->block());

  predecessor->end(MGoto::New(alloc(), current));
  if (!current->addPredecessorWithoutPhis(predecessor)) {
    return abort(AbortReason::Alloc);
  }

  // Initialize env chain slot to Undefined.  It's set later by
  // |initEnvironmentChain|.
  MInstruction* env = MConstant::New(alloc(), UndefinedValue());
  current->add(env);
  current->initSlot(info().environmentChainSlot(), env);

  // Initialize |return value| slot.
  MInstruction* returnValue = MConstant::New(alloc(), UndefinedValue());
  current->add(returnValue);
  current->initSlot(info().returnValueSlot(), returnValue);

  // Initialize |arguments| slot.
  if (info().hasArguments()) {
    MInstruction* argsObj = MConstant::New(alloc(), UndefinedValue());
    current->add(argsObj);
    current->initSlot(info().argsObjSlot(), argsObj);
  }

  // Initialize |this| slot.
  current->initSlot(info().thisSlot(), callInfo.thisArg());

  JitSpew(JitSpew_Inlining, "Initializing %u arg slots", info().nargs());

  // NB: Ion does not inline functions which |needsArgsObj|.  So using argSlot()
  // instead of argSlotUnchecked() below is OK
  MOZ_ASSERT(!info().needsArgsObj());

  // Initialize actually set arguments.
  uint32_t existing_args = std::min<uint32_t>(callInfo.argc(), info().nargs());
  for (size_t i = 0; i < existing_args; ++i) {
    MDefinition* arg = callInfo.getArg(i);
    current->initSlot(info().argSlot(i), arg);
  }

  // Pass Undefined for missing arguments
  for (size_t i = callInfo.argc(); i < info().nargs(); ++i) {
    MConstant* arg = MConstant::New(alloc(), UndefinedValue());
    current->add(arg);
    current->initSlot(info().argSlot(i), arg);
  }

  JitSpew(JitSpew_Inlining, "Initializing %u locals", info().nlocals());

  initLocals();

  JitSpew(JitSpew_Inlining,
          "Inline entry block MResumePoint %p, %u stack slots",
          (void*)current->entryResumePoint(),
          current->entryResumePoint()->stackDepth());

  // +2 for the env chain and |this|, maybe another +1 for arguments object
  // slot.
  MOZ_ASSERT(current->entryResumePoint()->stackDepth() == info().totalSlots());

#ifdef DEBUG
  if (script_->argumentsHasVarBinding()) {
    hasLazyArguments_ = true;
  }
#endif

  insertRecompileCheck(pc);

  // Initialize the env chain now that all resume points operands are
  // initialized.
  MOZ_TRY(initEnvironmentChain(callInfo.fun()));

  auto clearLastPriorResumePoint = mozilla::MakeScopeExit([&] {
    // Discard unreferenced & pre-allocated resume points.
    replaceMaybeFallbackFunctionGetter(nullptr);
  });

  MOZ_TRY(traverseBytecode());

  MOZ_ASSERT(iterators_.empty(), "Iterators should be added to outer builder");

  if (!info().isAnalysis() && !abortedPreliminaryGroups().empty()) {
    return abort(AbortReason::PreliminaryObjects);
  }

  return Ok();
}

void IonBuilder::rewriteParameter(uint32_t slotIdx, MDefinition* param) {
  MOZ_ASSERT(param->isParameter() || param->isGetArgumentsObjectArg());

  TemporaryTypeSet* types = param->resultTypeSet();
  MDefinition* actual = ensureDefiniteType(param, types->getKnownMIRType());
  if (actual == param) {
    return;
  }

  // Careful! We leave the original MParameter in the entry resume point. The
  // arguments still need to be checked unless proven otherwise at the call
  // site, and these checks can bailout. We can end up:
  //   v0 = Parameter(0)
  //   v1 = Unbox(v0, INT32)
  //   --   ResumePoint(v0)
  //
  // As usual, it would be invalid for v1 to be captured in the initial
  // resume point, rather than v0.
  current->rewriteSlot(slotIdx, actual);
}

// Apply Type Inference information to parameters early on, unboxing them if
// they have a definitive type. The actual guards will be emitted by the code
// generator, explicitly, as part of the function prologue.
AbortReasonOr<Ok> IonBuilder::rewriteParameters() {
  MOZ_ASSERT(info().environmentChainSlot() == 0);

  // If this JSScript is not the code of a function, then skip the
  // initialization of function parameters.
  if (!info().funMaybeLazy()) {
    return Ok();
  }

  for (uint32_t i = info().startArgSlot(); i < info().endArgSlot(); i++) {
    if (!alloc().ensureBallast()) {
      return abort(AbortReason::Alloc);
    }
    MDefinition* param = current->getSlot(i);
    rewriteParameter(i, param);
  }

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::initParameters() {
  // If this JSScript is not the code of a function, then skip the
  // initialization of function parameters.
  if (!info().funMaybeLazy()) {
    return Ok();
  }

  // If we are doing OSR on a frame which initially executed in the
  // interpreter and didn't accumulate type information, try to use that OSR
  // frame to determine possible initial types for 'this' and parameters.

  if (thisTypes->empty() && baselineFrame_) {
    TypeSet::Type type = baselineFrame_->thisType;
    if (type.isSingletonUnchecked()) {
      checkNurseryObject(type.singleton());
    }
    thisTypes->addType(type, alloc_->lifoAlloc());
  }

  MParameter* param =
      MParameter::New(alloc(), MParameter::THIS_SLOT, thisTypes);
  current->add(param);
  current->initSlot(info().thisSlot(), param);

  for (uint32_t i = 0; i < info().nargs(); i++) {
    TemporaryTypeSet* types = &argTypes[i];
    if (types->empty() && baselineFrame_ &&
        !script_->jitScript()->modifiesArguments()) {
      TypeSet::Type type = baselineFrame_->argTypes[i];
      if (type.isSingletonUnchecked()) {
        checkNurseryObject(type.singleton());
      }
      types->addType(type, alloc_->lifoAlloc());
    }

    param = MParameter::New(alloc().fallible(), i, types);
    if (!param) {
      return abort(AbortReason::Alloc);
    }
    current->add(param);
    current->initSlot(info().argSlotUnchecked(i), param);
  }

  return Ok();
}

void IonBuilder::initLocals() {
  // Initialize all frame slots to undefined. Lexical bindings are temporal
  // dead zoned in bytecode.

  if (info().nlocals() == 0) {
    return;
  }

  MConstant* undef = MConstant::New(alloc(), UndefinedValue());
  current->add(undef);

  for (uint32_t i = 0; i < info().nlocals(); i++) {
    current->initSlot(info().localSlot(i), undef);
  }
}

bool IonBuilder::usesEnvironmentChain() {
  return script()->jitScript()->usesEnvironmentChain();
}

AbortReasonOr<Ok> IonBuilder::initEnvironmentChain(MDefinition* callee) {
  MInstruction* env = nullptr;

  // If the script doesn't use the envchain, then it's already initialized
  // from earlier.  However, always make a env chain when |needsArgsObj| is true
  // for the script, since arguments object construction requires the env chain
  // to be passed in.
  if (!info().needsArgsObj() && !usesEnvironmentChain()) {
    return Ok();
  }

  // The env chain is only tracked in scripts that have NAME opcodes which
  // will try to access the env. For other scripts, the env instructions
  // will be held live by resume points and code will still be generated for
  // them, so just use a constant undefined value.

  if (JSFunction* fun = info().funMaybeLazy()) {
    if (!callee) {
      MCallee* calleeIns = MCallee::New(alloc());
      current->add(calleeIns);
      callee = calleeIns;
    }
    env = MFunctionEnvironment::New(alloc(), callee);
    current->add(env);

    // This reproduce what is done in CallObject::createForFunction. Skip
    // this for the arguments analysis, as the script might not have a
    // baseline script with template objects yet.
    if (fun->needsSomeEnvironmentObject() &&
        info().analysisMode() != Analysis_ArgumentsUsage) {
      if (fun->needsNamedLambdaEnvironment()) {
        env = createNamedLambdaObject(callee, env);
      }

      // TODO: Parameter expression-induced extra var environment not
      // yet handled.
      if (fun->needsExtraBodyVarEnvironment()) {
        return abort(AbortReason::Disable, "Extra var environment unsupported");
      }

      if (fun->needsCallObject()) {
        MOZ_TRY_VAR(env, createCallObject(callee, env));
      }
    }
  } else if (ModuleObject* module = info().module()) {
    // Modules use a pre-created env object.
    env = constant(ObjectValue(module->initialEnvironment()));
  } else {
    // For global scripts without a non-syntactic global scope, the env
    // chain is the global lexical env.
    MOZ_ASSERT(!script()->isForEval());
    MOZ_ASSERT(!script()->hasNonSyntacticScope());
    env = constant(ObjectValue(script()->global().lexicalEnvironment()));
  }

  // Update the environment slot from UndefinedValue only after initial
  // environment is created so that bailout doesn't see a partial env.
  // See: |InitFromBailout|
  current->setEnvironmentChain(env);
  return Ok();
}

void IonBuilder::initArgumentsObject() {
  JitSpew(JitSpew_IonMIR,
          "%s:%u:%u - Emitting code to initialize arguments object! block=%p",
          script()->filename(), script()->lineno(), script()->column(),
          current);
  MOZ_ASSERT(info().needsArgsObj());

  bool mapped = script()->hasMappedArgsObj();
  ArgumentsObject* templateObj =
      script()->realm()->maybeArgumentsTemplateObject(mapped);

  MCreateArgumentsObject* argsObj = MCreateArgumentsObject::New(
      alloc(), current->environmentChain(), templateObj);
  current->add(argsObj);
  current->setArgumentsObject(argsObj);
}

AbortReasonOr<Ok> IonBuilder::addOsrValueTypeBarrier(
    uint32_t slot, MInstruction** def_, MIRType type,
    TemporaryTypeSet* typeSet) {
  MInstruction*& def = *def_;
  MBasicBlock* osrBlock = def->block();

  // Clear bogus type information added in newOsrPreheader().
  def->setResultType(MIRType::Value);
  def->setResultTypeSet(nullptr);

  if (typeSet && !typeSet->unknown()) {
    MInstruction* barrier = MTypeBarrier::New(alloc(), def, typeSet);
    osrBlock->insertBefore(osrBlock->lastIns(), barrier);
    osrBlock->rewriteSlot(slot, barrier);
    def = barrier;

    // If the TypeSet is more precise than |type|, adjust |type| for the
    // code below.
    if (type == MIRType::Value) {
      type = barrier->type();
    }
  } else if (type == MIRType::Null || type == MIRType::Undefined ||
             type == MIRType::MagicOptimizedArguments) {
    // No unbox instruction will be added below, so check the type by
    // adding a type barrier for a singleton type set.
    TypeSet::Type ntype = TypeSet::PrimitiveType(type);
    LifoAlloc* lifoAlloc = alloc().lifoAlloc();
    typeSet = lifoAlloc->new_<TemporaryTypeSet>(lifoAlloc, ntype);
    if (!typeSet) {
      return abort(AbortReason::Alloc);
    }
    MInstruction* barrier = MTypeBarrier::New(alloc(), def, typeSet);
    osrBlock->insertBefore(osrBlock->lastIns(), barrier);
    osrBlock->rewriteSlot(slot, barrier);
    def = barrier;
  }

  // The following guards aren't directly linked into the usedef chain,
  // however in the OSR block we need to ensure they're not optimized out, so we
  // mark them as implicitly used.
  switch (type) {
    case MIRType::Null:
    case MIRType::Undefined:
    case MIRType::MagicOptimizedArguments:
      def->setImplicitlyUsed();
      break;
    default:
      break;
  }

  // Unbox the OSR value to the type expected by the loop header.
  //
  // The only specialized types that can show up here are MIRTypes with a
  // corresponding TypeSet::Type because NewBaselineFrameInspector and
  // newPendingLoopHeader use TypeSet::Type for Values from the BaselineFrame.
  // This means magic values other than MagicOptimizedArguments are represented
  // as UnknownType() and MIRType::Value. See also TypeSet::IsUntrackedValue.
  switch (type) {
    case MIRType::Boolean:
    case MIRType::Int32:
    case MIRType::Double:
    case MIRType::String:
    case MIRType::Symbol:
    case MIRType::BigInt:
    case MIRType::Object:
      if (type != def->type()) {
        MUnbox* unbox = MUnbox::New(alloc(), def, type, MUnbox::Fallible);
        osrBlock->insertBefore(osrBlock->lastIns(), unbox);
        osrBlock->rewriteSlot(slot, unbox);
        def = unbox;
      }
      break;

    case MIRType::Value:
      // Nothing to do.
      break;

    case MIRType::Null: {
      MConstant* c = MConstant::New(alloc(), NullValue());
      osrBlock->insertBefore(osrBlock->lastIns(), c);
      osrBlock->rewriteSlot(slot, c);
      def = c;
      break;
    }

    case MIRType::Undefined: {
      MConstant* c = MConstant::New(alloc(), UndefinedValue());
      osrBlock->insertBefore(osrBlock->lastIns(), c);
      osrBlock->rewriteSlot(slot, c);
      def = c;
      break;
    }

    case MIRType::MagicOptimizedArguments: {
      MOZ_ASSERT(hasLazyArguments_);
      MConstant* lazyArg =
          MConstant::New(alloc(), MagicValue(JS_OPTIMIZED_ARGUMENTS));
      osrBlock->insertBefore(osrBlock->lastIns(), lazyArg);
      osrBlock->rewriteSlot(slot, lazyArg);
      def = lazyArg;
      break;
    }

    default:
      MOZ_CRASH("Unexpected type");
  }

  MOZ_ASSERT(def == osrBlock->getSlot(slot));
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::maybeAddOsrTypeBarriers() {
  if (!info().osrPc()) {
    return Ok();
  }

  // The loop has successfully been processed, and the loop header phis
  // have their final type. Add unboxes and type barriers in the OSR
  // block to check that the values have the appropriate type, and update
  // the types in the preheader.

  MBasicBlock* osrBlock = graph().osrBlock();
  if (!osrBlock) {
    // Because IonBuilder does not compile catch blocks, it's possible to
    // end up without an OSR block if the OSR pc is only reachable via a
    // break-statement inside the catch block. For instance:
    //
    //   for (;;) {
    //       try {
    //           throw 3;
    //       } catch(e) {
    //           break;
    //       }
    //   }
    //   while (..) { } // <= OSR here, only reachable via catch block.
    //
    // For now we just abort in this case.
    MOZ_ASSERT(graph().hasTryBlock());
    return abort(AbortReason::Disable,
                 "OSR block only reachable through catch block");
  }

  MBasicBlock* preheader = osrBlock->getSuccessor(0);
  MBasicBlock* header = preheader->getSuccessor(0);
  static const size_t OSR_PHI_POSITION = 1;
  MOZ_ASSERT(preheader->getPredecessor(OSR_PHI_POSITION) == osrBlock);

  MResumePoint* headerRp = header->entryResumePoint();
  size_t stackDepth = headerRp->stackDepth();
  MOZ_ASSERT(stackDepth == osrBlock->stackDepth());
  for (uint32_t slot = info().startArgSlot(); slot < stackDepth; slot++) {
    // Aliased slots are never accessed, since they need to go through
    // the callobject. The typebarriers are added there and can be
    // discarded here.
    if (info().isSlotAliased(slot)) {
      continue;
    }

    if (!alloc().ensureBallast()) {
      return abort(AbortReason::Alloc);
    }

    MInstruction* def = osrBlock->getSlot(slot)->toInstruction();
    MPhi* preheaderPhi = preheader->getSlot(slot)->toPhi();
    MPhi* headerPhi = headerRp->getOperand(slot)->toPhi();

    MIRType type = headerPhi->type();
    TemporaryTypeSet* typeSet = headerPhi->resultTypeSet();

    MOZ_TRY(addOsrValueTypeBarrier(slot, &def, type, typeSet));

    preheaderPhi->replaceOperand(OSR_PHI_POSITION, def);
    preheaderPhi->setResultType(type);
    preheaderPhi->setResultTypeSet(typeSet);
  }

  return Ok();
}

#ifdef DEBUG
// In debug builds, after compiling a bytecode op, this class is used to check
// that all values popped by this opcode either:
//
//   (1) Have the ImplicitlyUsed flag set on them.
//   (2) Have more uses than before compiling this op (the value is
//       used as operand of a new MIR instruction).
//
// This is used to catch problems where IonBuilder pops a value without
// adding any SSA uses and doesn't call setImplicitlyUsedUnchecked on it.
class MOZ_RAII PoppedValueUseChecker {
  Vector<MDefinition*, 4, SystemAllocPolicy> popped_;
  Vector<size_t, 4, SystemAllocPolicy> poppedUses_;
  MBasicBlock* current_;
  jsbytecode* pc_;

 public:
  PoppedValueUseChecker(MBasicBlock* current, jsbytecode* pc)
      : current_(current), pc_(pc) {}

  MOZ_MUST_USE bool init() {
    unsigned nuses = GetUseCount(pc_);

    for (unsigned i = 0; i < nuses; i++) {
      MDefinition* def = current_->peek(-int32_t(i + 1));
      if (!popped_.append(def) || !poppedUses_.append(def->defUseCount())) {
        return false;
      }
    }

    return true;
  }

  void checkAfterOp() {
    JSOp op = JSOp(*pc_);

    // Don't require SSA uses for values popped by these ops.
    switch (op) {
      case JSOp::Pop:
      case JSOp::PopN:
      case JSOp::DupAt:
      case JSOp::Dup:
      case JSOp::Dup2:
      case JSOp::Pick:
      case JSOp::Unpick:
      case JSOp::Swap:
      case JSOp::SetArg:
      case JSOp::SetLocal:
      case JSOp::InitLexical:
      case JSOp::SetRval:
      case JSOp::Void:
        // Basic stack/local/argument management opcodes.
        return;

      case JSOp::Case:
      case JSOp::Default:
        // These ops have to pop the switch value when branching but don't
        // actually use it.
        return;

      default:
        break;
    }

    for (size_t i = 0; i < popped_.length(); i++) {
      switch (op) {
        case JSOp::Pos:
        case JSOp::ToNumeric:
        case JSOp::ToId:
        case JSOp::ToString:
          // These ops may leave their input on the stack without setting
          // the ImplicitlyUsed flag. If this value will be popped immediately,
          // we may replace it with |undefined|, but the difference is
          // not observable.
          MOZ_ASSERT(i == 0);
          if (current_->peek(-1) == popped_[0]) {
            break;
          }
          [[fallthrough]];

        default:
          MOZ_ASSERT(popped_[i]->isImplicitlyUsed() ||
                     // First value popped by JSOp::EndIter is not used at all,
                     // it's similar to JSOp::Pop above.
                     (op == JSOp::EndIter && i == 0) ||
                     popped_[i]->defUseCount() > poppedUses_[i]);
          break;
      }
    }
  }
};
#endif

AbortReasonOr<Ok> IonBuilder::traverseBytecode() {
  // See the "Control Flow handling in IonBuilder" comment in IonBuilder.h for
  // more information.

  MOZ_TRY(startTraversingBlock(current));

  const jsbytecode* const codeEnd = script()->codeEnd();

  while (true) {
    if (!alloc().ensureBallast()) {
      return abort(AbortReason::Alloc);
    }

    // Skip unreachable ops (for example code after a 'return' or 'throw') until
    // we get to the next jump target.
    if (hasTerminatedBlock()) {
      while (!BytecodeIsJumpTarget(JSOp(*pc))) {
        // Finish any "broken" loops with an unreachable backedge. For example:
        //
        //   do {
        //     ...
        //     return;
        //     ...
        //   } while (x);
        //
        // This loop never actually loops.
        if (!loopStack_.empty() &&
            IsBackedgeForLoopHead(pc, loopStack_.back().header()->pc())) {
          MOZ_ASSERT(loopDepth_ > 0);
          loopDepth_--;
          loopStack_.popBack();
        }

        pc = GetNextPc(pc);
        if (pc == codeEnd) {
          return Ok();
        }
      }
    }

#ifdef DEBUG
    PoppedValueUseChecker useChecker(current, pc);
    if (!useChecker.init()) {
      return abort(AbortReason::Alloc);
    }
#endif

    MOZ_ASSERT(script()->containsPC(pc));
    nextpc = GetNextPc(pc);

    // Nothing in inspectOpcode() is allowed to advance the pc.
    JSOp op = JSOp(*pc);
    bool restarted = false;
    MOZ_TRY(inspectOpcode(op, &restarted));

#ifdef DEBUG
    if (!restarted) {
      useChecker.checkAfterOp();
    }
#endif

    if (nextpc == codeEnd) {
      return Ok();
    }

    pc = nextpc;
    MOZ_ASSERT(script()->containsPC(pc));

    if (!hasTerminatedBlock()) {
      current->updateTrackedSite(bytecodeSite(pc));
    }
  }

  // The iloop above never breaks, so this point is unreachable.  Don't add code
  // here, or you'll trigger compile errors about unreachable code with some
  // compilers!
}

AbortReasonOr<Ok> IonBuilder::startTraversingBlock(MBasicBlock* block) {
  block->setLoopDepth(loopDepth_);

  if (block->pc() && script()->hasScriptCounts()) {
    block->setHitCount(script()->getHitCount(block->pc()));
  }

  // Optimization to move a predecessor that only has this block as successor
  // just before this block.  Skip this optimization if the previous block is
  // not part of the same function, as we might have to backtrack on inlining
  // failures.
  if (block->numPredecessors() == 1 &&
      block->getPredecessor(0)->numSuccessors() == 1 &&
      !block->getPredecessor(0)->outerResumePoint()) {
    graph().removeBlockFromList(block->getPredecessor(0));
    graph().addBlock(block->getPredecessor(0));
  }

  MOZ_TRY(setCurrentAndSpecializePhis(block));

  graph().addBlock(block);

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_goto(bool* restarted) {
  MOZ_ASSERT(JSOp(*pc) == JSOp::Goto);

  if (IsBackedgePC(pc)) {
    return visitBackEdge(restarted);
  }

  jsbytecode* target = pc + GET_JUMP_OFFSET(pc);
  return visitGoto(target);
}

AbortReasonOr<Ok> IonBuilder::addPendingEdge(const PendingEdge& edge,
                                             jsbytecode* target) {
  PendingEdgesMap::AddPtr p = pendingEdges_.lookupForAdd(target);
  if (p) {
    if (!p->value().append(edge)) {
      return abort(AbortReason::Alloc);
    }
    return Ok();
  }

  PendingEdges edges;
  static_assert(PendingEdges::InlineLength >= 1,
                "Appending one element should be infallible");
  MOZ_ALWAYS_TRUE(edges.append(edge));

  if (!pendingEdges_.add(p, target, std::move(edges))) {
    return abort(AbortReason::Alloc);
  }
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::visitGoto(jsbytecode* target) {
  current->end(MGoto::New(alloc(), nullptr));
  MOZ_TRY(addPendingEdge(PendingEdge::NewGoto(current), target));
  setTerminatedBlock();
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_loophead() {
  // All loops have the following bytecode structure:
  //
  //    LoopHead
  //    ...
  //    IfNe/Goto to LoopHead

  MOZ_ASSERT(JSOp(*pc) == JSOp::LoopHead);

  if (hasTerminatedBlock()) {
    // The whole loop is unreachable.
    return Ok();
  }

  bool osr = pc == info().osrPc();
  if (osr) {
    MBasicBlock* preheader;
    MOZ_TRY_VAR(preheader, newOsrPreheader(current, pc));
    current->end(MGoto::New(alloc(), preheader));
    MOZ_TRY(setCurrentAndSpecializePhis(preheader));
  }

  loopDepth_++;
  MBasicBlock* header;
  MOZ_TRY_VAR(header, newPendingLoopHeader(current, pc, osr));
  current->end(MGoto::New(alloc(), header));

  if (!loopStack_.emplaceBack(header)) {
    return abort(AbortReason::Alloc);
  }

  MOZ_TRY(analyzeNewLoopTypes(header));

  MOZ_TRY(startTraversingBlock(header));
  return emitLoopHeadInstructions(pc);
}

AbortReasonOr<Ok> IonBuilder::visitBackEdge(bool* restarted) {
  MOZ_ASSERT(loopDepth_ > 0);
  loopDepth_--;

  MBasicBlock* header = loopStack_.back().header();
  current->end(MGoto::New(alloc(), header));

  // Compute phis in the loop header and propagate them throughout the loop,
  // including the successor.
  AbortReason r = header->setBackedge(alloc(), current);
  switch (r) {
    case AbortReason::NoAbort:
      loopStack_.popBack();
      setTerminatedBlock();
      return Ok();

    case AbortReason::Disable:
      // If there are types for variables on the backedge that were not
      // present at the original loop header, then uses of the variables'
      // phis may have generated incorrect nodes. The new types have been
      // incorporated into the header phis, so remove all blocks for the
      // loop body and restart with the new types.
      *restarted = true;
      MOZ_TRY(restartLoop(header));
      return Ok();

    default:
      return abort(r);
  }
}

AbortReasonOr<Ok> IonBuilder::emitLoopHeadInstructions(jsbytecode* pc) {
  MOZ_ASSERT(JSOp(*pc) == JSOp::LoopHead);

  MInterruptCheck* check = MInterruptCheck::New(alloc());
  current->add(check);
  insertRecompileCheck(pc);

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::inspectOpcode(JSOp op, bool* restarted) {
  // Add not yet implemented opcodes at the bottom of the switch!
  switch (op) {
    case JSOp::NopDestructuring:
    case JSOp::TryDestructuring:
    case JSOp::Lineno:
    case JSOp::Nop:
      return Ok();

    case JSOp::LoopHead:
      return jsop_loophead();

    case JSOp::Undefined:
      // If this ever changes, change what JSOp::GImplicitThis does too.
      pushConstant(UndefinedValue());
      return Ok();

    case JSOp::Try:
      return visitTry();

    case JSOp::Default:
      current->pop();
      return visitGoto(pc + GET_JUMP_OFFSET(pc));

    case JSOp::Goto:
      return jsop_goto(restarted);

    case JSOp::IfNe:
    case JSOp::IfEq:
    case JSOp::And:
    case JSOp::Or:
    case JSOp::Case:
      return visitTest(op, restarted);

    case JSOp::Coalesce:
      return jsop_coalesce();

    case JSOp::Return:
    case JSOp::RetRval:
      return visitReturn(op);

    case JSOp::Throw:
      return visitThrow();

    case JSOp::JumpTarget:
      return visitJumpTarget(op);

    case JSOp::TableSwitch:
      return visitTableSwitch();

    case JSOp::BitNot:
      return jsop_bitnot();

    case JSOp::BitAnd:
    case JSOp::BitOr:
    case JSOp::BitXor:
    case JSOp::Lsh:
    case JSOp::Rsh:
    case JSOp::Ursh:
      return jsop_bitop(op);

    case JSOp::Add:
    case JSOp::Sub:
    case JSOp::Mul:
    case JSOp::Div:
    case JSOp::Mod:
      return jsop_binary_arith(op);

    case JSOp::Pow:
      return jsop_pow();

    case JSOp::Pos:
      return jsop_pos();

    case JSOp::ToNumeric:
      return jsop_tonumeric();

    case JSOp::Neg:
      return jsop_neg();

    case JSOp::Inc:
    case JSOp::Dec:
      return jsop_inc_or_dec(op);

    case JSOp::ToString:
      return jsop_tostring();

    case JSOp::DefVar:
      return jsop_defvar();

    case JSOp::DefLet:
    case JSOp::DefConst:
      return jsop_deflexical();

    case JSOp::DefFun:
      return jsop_deffun();

    case JSOp::Eq:
    case JSOp::Ne:
    case JSOp::StrictEq:
    case JSOp::StrictNe:
    case JSOp::Lt:
    case JSOp::Le:
    case JSOp::Gt:
    case JSOp::Ge:
      return jsop_compare(op);

    case JSOp::Double:
      pushConstant(GET_INLINE_VALUE(pc));
      return Ok();

    case JSOp::BigInt:
      pushConstant(BigIntValue(info().getBigInt(pc)));
      return Ok();

    case JSOp::String:
      pushConstant(StringValue(info().getAtom(pc)));
      return Ok();

    case JSOp::Symbol: {
      unsigned which = GET_UINT8(pc);
      JS::Symbol* sym = realm->runtime()->wellKnownSymbols().get(which);
      pushConstant(SymbolValue(sym));
      return Ok();
    }

    case JSOp::Zero:
      pushConstant(Int32Value(0));
      return Ok();

    case JSOp::One:
      pushConstant(Int32Value(1));
      return Ok();

    case JSOp::Null:
      pushConstant(NullValue());
      return Ok();

    case JSOp::Void:
      current->pop();
      pushConstant(UndefinedValue());
      return Ok();

    case JSOp::Hole:
      pushConstant(MagicValue(JS_ELEMENTS_HOLE));
      return Ok();

    case JSOp::False:
      pushConstant(BooleanValue(false));
      return Ok();

    case JSOp::True:
      pushConstant(BooleanValue(true));
      return Ok();

    case JSOp::Arguments:
      return jsop_arguments();

    case JSOp::Rest:
      return jsop_rest();

    case JSOp::GetArg:
      return jsop_getarg(GET_ARGNO(pc));

    case JSOp::SetArg:
      return jsop_setarg(GET_ARGNO(pc));

    case JSOp::GetLocal:
      current->pushLocal(GET_LOCALNO(pc));
      return Ok();

    case JSOp::SetLocal:
      current->setLocal(GET_LOCALNO(pc));
      return Ok();

    case JSOp::ThrowSetConst:
      return jsop_throwsetconst();

    case JSOp::CheckLexical:
    case JSOp::CheckAliasedLexical:
      return jsop_checklexical();

    case JSOp::InitLexical:
      current->setLocal(GET_LOCALNO(pc));
      return Ok();

    case JSOp::InitGLexical: {
      MOZ_ASSERT(!script()->hasNonSyntacticScope());
      MDefinition* value = current->pop();
      current->push(
          constant(ObjectValue(script()->global().lexicalEnvironment())));
      current->push(value);
      return jsop_setprop(info().getAtom(pc)->asPropertyName());
    }

    case JSOp::InitAliasedLexical:
      return jsop_setaliasedvar(EnvironmentCoordinate(pc));

    case JSOp::Uninitialized:
      pushConstant(MagicValue(JS_UNINITIALIZED_LEXICAL));
      return Ok();

    case JSOp::Pop: {
      MDefinition* def = current->pop();

      // Pop opcodes frequently appear where values are killed, e.g. after
      // SET* opcodes. Place a resume point afterwards to avoid capturing
      // the dead value in later snapshots, except in places where that
      // resume point is obviously unnecessary.
      if (JSOp(pc[JSOpLength_Pop]) == JSOp::Pop) {
        return Ok();
      }
      if (def->isConstant()) {
        return Ok();
      }
      return maybeInsertResume();
    }

    case JSOp::PopN:
      for (uint32_t i = 0, n = GET_UINT16(pc); i < n; i++) {
        current->pop();
      }
      return Ok();

    case JSOp::DupAt:
      current->pushSlot(current->stackDepth() - 1 - GET_UINT24(pc));
      return Ok();

    case JSOp::NewArray:
      return jsop_newarray(GET_UINT32(pc));

    case JSOp::NewArrayCopyOnWrite:
      return jsop_newarray_copyonwrite();

    case JSOp::NewInit:
    case JSOp::NewObject:
    case JSOp::NewObjectWithGroup:
      return jsop_newobject();

    case JSOp::InitElem:
    case JSOp::InitHiddenElem:
      return jsop_initelem();

    case JSOp::InitElemInc:
      return jsop_initelem_inc();

    case JSOp::InitElemArray:
      return jsop_initelem_array();

    case JSOp::InitProp:
    case JSOp::InitLockedProp:
    case JSOp::InitHiddenProp: {
      PropertyName* name = info().getAtom(pc)->asPropertyName();
      return jsop_initprop(name);
    }

    case JSOp::MutateProto: {
      return jsop_mutateproto();
    }

    case JSOp::InitPropGetter:
    case JSOp::InitHiddenPropGetter:
    case JSOp::InitPropSetter:
    case JSOp::InitHiddenPropSetter: {
      PropertyName* name = info().getAtom(pc)->asPropertyName();
      return jsop_initprop_getter_setter(name);
    }

    case JSOp::InitElemGetter:
    case JSOp::InitHiddenElemGetter:
    case JSOp::InitElemSetter:
    case JSOp::InitHiddenElemSetter:
      return jsop_initelem_getter_setter();

    case JSOp::FunCall:
      return jsop_funcall(GET_ARGC(pc));

    case JSOp::FunApply:
      return jsop_funapply(GET_ARGC(pc));

    case JSOp::SpreadCall:
      return jsop_spreadcall();

    case JSOp::SpreadNew:
    case JSOp::SpreadSuperCall:
      return jsop_spreadnew();

    case JSOp::Call:
    case JSOp::CallIgnoresRv:
    case JSOp::CallIter:
    case JSOp::New:
    case JSOp::SuperCall:
      MOZ_TRY(jsop_call(GET_ARGC(pc),
                        JSOp(*pc) == JSOp::New || JSOp(*pc) == JSOp::SuperCall,
                        JSOp(*pc) == JSOp::CallIgnoresRv));
      return Ok();

    case JSOp::Eval:
    case JSOp::StrictEval:
      return jsop_eval(GET_ARGC(pc));

    case JSOp::Int8:
      pushConstant(Int32Value(GET_INT8(pc)));
      return Ok();

    case JSOp::Uint16:
      pushConstant(Int32Value(GET_UINT16(pc)));
      return Ok();

    case JSOp::GetGName: {
      PropertyName* name = info().getAtom(pc)->asPropertyName();
      if (!script()->hasNonSyntacticScope()) {
        return jsop_getgname(name);
      }
      return jsop_getname(name);
    }

    case JSOp::SetGName:
    case JSOp::StrictSetGName: {
      PropertyName* name = info().getAtom(pc)->asPropertyName();
      JSObject* obj = nullptr;
      if (!script()->hasNonSyntacticScope()) {
        obj = testGlobalLexicalBinding(name);
      }
      if (obj) {
        return setStaticName(obj, name);
      }
      return jsop_setprop(name);
    }

    case JSOp::GetName: {
      PropertyName* name = info().getAtom(pc)->asPropertyName();
      return jsop_getname(name);
    }

    case JSOp::GetIntrinsic: {
      PropertyName* name = info().getAtom(pc)->asPropertyName();
      return jsop_intrinsic(name);
    }

    case JSOp::GetImport: {
      PropertyName* name = info().getAtom(pc)->asPropertyName();
      return jsop_getimport(name);
    }

    case JSOp::BindGName:
      if (!script()->hasNonSyntacticScope()) {
        if (JSObject* env = testGlobalLexicalBinding(info().getName(pc))) {
          pushConstant(ObjectValue(*env));
          return Ok();
        }
      }
      // Fall through to JSOp::BindName
      [[fallthrough]];
    case JSOp::BindName:
      return jsop_bindname(info().getName(pc));

    case JSOp::BindVar:
      return jsop_bindvar();

    case JSOp::Dup:
      current->pushSlot(current->stackDepth() - 1);
      return Ok();

    case JSOp::Dup2:
      return jsop_dup2();

    case JSOp::Swap:
      current->swapAt(-1);
      return Ok();

    case JSOp::Pick:
      current->pick(-GET_INT8(pc));
      return Ok();

    case JSOp::Unpick:
      current->unpick(-GET_INT8(pc));
      return Ok();

    case JSOp::GetAliasedVar:
      return jsop_getaliasedvar(EnvironmentCoordinate(pc));

    case JSOp::SetAliasedVar:
      return jsop_setaliasedvar(EnvironmentCoordinate(pc));

    case JSOp::Uint24:
    case JSOp::ResumeIndex:
      pushConstant(Int32Value(GET_UINT24(pc)));
      return Ok();

    case JSOp::Int32:
      pushConstant(Int32Value(GET_INT32(pc)));
      return Ok();

    case JSOp::GetElem:
    case JSOp::CallElem:
      MOZ_TRY(jsop_getelem());
      if (op == JSOp::CallElem) {
        MOZ_TRY(improveThisTypesForCall());
      }
      return Ok();

    case JSOp::SetElem:
    case JSOp::StrictSetElem:
      return jsop_setelem();

    case JSOp::Length:
      return jsop_length();

    case JSOp::Not:
      return jsop_not();

    case JSOp::FunctionThis:
      return jsop_functionthis();

    case JSOp::GlobalThis:
      return jsop_globalthis();

    case JSOp::Callee: {
      MDefinition* callee = getCallee();
      current->push(callee);
      return Ok();
    }

    case JSOp::EnvCallee:
      return jsop_envcallee();

    case JSOp::SuperBase:
      return jsop_superbase();

    case JSOp::GetPropSuper: {
      PropertyName* name = info().getAtom(pc)->asPropertyName();
      return jsop_getprop_super(name);
    }

    case JSOp::GetElemSuper:
      return jsop_getelem_super();

    case JSOp::GetProp:
    case JSOp::CallProp: {
      PropertyName* name = info().getAtom(pc)->asPropertyName();
      MOZ_TRY(jsop_getprop(name));
      if (op == JSOp::CallProp) {
        MOZ_TRY(improveThisTypesForCall());
      }
      return Ok();
    }

    case JSOp::SetProp:
    case JSOp::StrictSetProp:
    case JSOp::SetName:
    case JSOp::StrictSetName: {
      PropertyName* name = info().getAtom(pc)->asPropertyName();
      return jsop_setprop(name);
    }

    case JSOp::DelProp:
    case JSOp::StrictDelProp: {
      PropertyName* name = info().getAtom(pc)->asPropertyName();
      return jsop_delprop(name);
    }

    case JSOp::DelElem:
    case JSOp::StrictDelElem:
      return jsop_delelem();

    case JSOp::RegExp:
      return jsop_regexp(info().getRegExp(pc));

    case JSOp::CallSiteObj:
      pushConstant(ObjectValue(*info().getObject(pc)));
      return Ok();

    case JSOp::Object:
      return jsop_object(info().getObject(pc));

    case JSOp::ClassConstructor:
      return jsop_classconstructor();

    case JSOp::DerivedConstructor:
      return jsop_derivedclassconstructor();

    case JSOp::Typeof:
    case JSOp::TypeofExpr:
      return jsop_typeof();

    case JSOp::ToAsyncIter:
      return jsop_toasynciter();

    case JSOp::ToId:
      return jsop_toid();

    case JSOp::IterNext:
      return jsop_iternext();

    case JSOp::Lambda:
      return jsop_lambda(info().getFunction(pc));

    case JSOp::LambdaArrow:
      return jsop_lambda_arrow(info().getFunction(pc));

    case JSOp::SetFunName:
      return jsop_setfunname(GET_UINT8(pc));

    case JSOp::PushLexicalEnv:
      return jsop_pushlexicalenv(GET_UINT32_INDEX(pc));

    case JSOp::PopLexicalEnv:
      current->setEnvironmentChain(walkEnvironmentChain(1));
      return Ok();

    case JSOp::FreshenLexicalEnv:
      return jsop_copylexicalenv(true);

    case JSOp::RecreateLexicalEnv:
      return jsop_copylexicalenv(false);

    case JSOp::Iter:
      return jsop_iter();

    case JSOp::MoreIter:
      return jsop_itermore();

    case JSOp::IsNoIter:
      return jsop_isnoiter();

    case JSOp::EndIter:
      return jsop_iterend();

    case JSOp::In:
      return jsop_in();

    case JSOp::HasOwn:
      return jsop_hasown();

    case JSOp::SetRval:
      MOZ_ASSERT(!script()->noScriptRval());
      current->setSlot(info().returnValueSlot(), current->pop());
      return Ok();

    case JSOp::Instanceof:
      return jsop_instanceof();

    case JSOp::DebugLeaveLexicalEnv:
      return Ok();

    case JSOp::Debugger:
      return jsop_debugger();

    case JSOp::GImplicitThis:
      if (!script()->hasNonSyntacticScope()) {
        pushConstant(UndefinedValue());
        return Ok();
      }
      // Fallthrough to ImplicitThis in non-syntactic scope case
      [[fallthrough]];
    case JSOp::ImplicitThis: {
      PropertyName* name = info().getAtom(pc)->asPropertyName();
      return jsop_implicitthis(name);
    }

    case JSOp::NewTarget:
      return jsop_newtarget();

    case JSOp::CheckIsObj:
      return jsop_checkisobj(GET_UINT8(pc));

    case JSOp::CheckIsCallable:
      return jsop_checkiscallable(GET_UINT8(pc));

    case JSOp::CheckObjCoercible:
      return jsop_checkobjcoercible();

    case JSOp::DebugCheckSelfHosted: {
#ifdef DEBUG
      MDebugCheckSelfHosted* check =
          MDebugCheckSelfHosted::New(alloc(), current->pop());
      current->add(check);
      current->push(check);
      MOZ_TRY(resumeAfter(check));
#endif
      return Ok();
    }

    case JSOp::IsConstructing:
      pushConstant(MagicValue(JS_IS_CONSTRUCTING));
      return Ok();

    case JSOp::OptimizeSpreadCall:
      return jsop_optimize_spreadcall();

    case JSOp::ImportMeta:
      return jsop_importmeta();

    case JSOp::DynamicImport:
      return jsop_dynamic_import();

    case JSOp::InstrumentationActive:
      return jsop_instrumentation_active();

    case JSOp::InstrumentationCallback:
      return jsop_instrumentation_callback();

    case JSOp::InstrumentationScriptId:
      return jsop_instrumentation_scriptid();

    case JSOp::CheckClassHeritage:
      return jsop_checkclassheritage();

    case JSOp::FunWithProto:
      return jsop_funwithproto(info().getFunction(pc));

    case JSOp::ObjWithProto:
      return jsop_objwithproto();

    case JSOp::BuiltinProto:
      return jsop_builtinproto();

    case JSOp::CheckReturn:
      return jsop_checkreturn();

    case JSOp::CheckThis:
      return jsop_checkthis();

    case JSOp::CheckThisReinit:
      return jsop_checkthisreinit();

    case JSOp::SuperFun:
      return jsop_superfun();

    case JSOp::InitHomeObject:
      return jsop_inithomeobject();

    // ===== NOT Yet Implemented =====
    // Read below!

    // With
    case JSOp::EnterWith:
    case JSOp::LeaveWith:

    // Spread
    case JSOp::SpreadEval:
    case JSOp::StrictSpreadEval:

    // Super
    case JSOp::SetPropSuper:
    case JSOp::SetElemSuper:
    case JSOp::StrictSetPropSuper:
    case JSOp::StrictSetElemSuper:

    // Environments (bug 1366470)
    case JSOp::PushVarEnv:

    // Compound assignment
    case JSOp::GetBoundName:

    // Generators / Async (bug 1317690)
    case JSOp::Exception:
    case JSOp::IsGenClosing:
    case JSOp::InitialYield:
    case JSOp::Yield:
    case JSOp::FinalYieldRval:
    case JSOp::Resume:
    case JSOp::ResumeKind:
    case JSOp::CheckResumeKind:
    case JSOp::AfterYield:
    case JSOp::Await:
    case JSOp::TrySkipAwait:
    case JSOp::Generator:
    case JSOp::AsyncAwait:
    case JSOp::AsyncResolve:

    // Misc
    case JSOp::DelName:
    case JSOp::Finally:
    case JSOp::GetRval:
    case JSOp::Gosub:
    case JSOp::Retsub:
    case JSOp::SetIntrinsic:
    case JSOp::ThrowMsg:
      // === !! WARNING WARNING WARNING !! ===
      // Do you really want to sacrifice performance by not implementing this
      // operation in the optimizing compiler?
      break;

    case JSOp::ForceInterpreter:
      // Intentionally not implemented.
      break;
  }

#ifdef DEBUG
  return abort(AbortReason::Disable, "Unsupported opcode: %s", CodeName(op));
#else
  return abort(AbortReason::Disable, "Unsupported opcode: %d",
               int(uint8_t(op)));
#endif
}

AbortReasonOr<Ok> IonBuilder::restartLoop(MBasicBlock* header) {
  AutoTraceLog logCompile(TraceLoggerForCurrentThread(),
                          TraceLogger_IonBuilderRestartLoop);

  spew("New types at loop header, restarting loop body");

  if (JitOptions.limitScriptSize) {
    if (++numLoopRestarts_ >= MAX_LOOP_RESTARTS) {
      return abort(AbortReason::Disable,
                   "Aborted while processing control flow");
    }
  }

  // Restore slots to entry state.
  size_t stackDepth = header->entryResumePoint()->stackDepth();
  for (size_t slot = 0; slot < stackDepth; slot++) {
    MDefinition* loopDef = header->entryResumePoint()->getOperand(slot);
    header->setSlot(slot, loopDef);
  }

  // Remove phi operands.
  for (MPhiIterator phi = header->phisBegin(); phi != header->phisEnd();
       phi++) {
    phi->removeOperand(phi->numOperands() - 1);
  }

  // Discard unreferenced & pre-allocated resume points.
  replaceMaybeFallbackFunctionGetter(nullptr);

  // Remove all blocks in the loop body other than the header, which has phis
  // of the appropriate type and incoming edges to preserve.
  if (!graph().removeSuccessorBlocks(header)) {
    return abort(AbortReason::Alloc);
  }
  graph().removeBlockFromList(header);

  // Remove all instructions from the header itself, and all resume points
  // except the entry resume point.
  header->discardAllInstructions();
  header->discardAllResumePoints(/* discardEntry = */ false);
  header->setStackDepth(header->getPredecessor(0)->stackDepth());

  loopDepth_ = header->loopDepth();

  // Don't specializePhis(), as the header has been visited before and the
  // phis have already had their type set.
  setCurrent(header);
  graph().addBlock(current);

  jsbytecode* loopHead = header->pc();
  MOZ_ASSERT(JSOp(*loopHead) == JSOp::LoopHead);

  // Since we discarded the header's instructions above, emit them again. This
  // includes the interrupt check.
  MOZ_TRY(emitLoopHeadInstructions(loopHead));
  nextpc = GetNextPc(loopHead);

  // Remove loop header and dead blocks from pendingBlocks.
  for (PendingEdgesMap::Range r = pendingEdges_.all(); !r.empty();
       r.popFront()) {
    PendingEdges& blocks = r.front().value();
    for (size_t i = blocks.length(); i > 0; i--) {
      PendingEdge& block = blocks[i - 1];
      if (block.block() == header || block.block()->isDead()) {
        blocks.erase(&block);
      }
    }
  }

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::replaceTypeSet(MDefinition* subject,
                                             TemporaryTypeSet* type,
                                             MTest* test) {
  if (type->unknown()) {
    return Ok();
  }

  // Don't emit MFilterTypeSet if it doesn't improve the typeset.
  if (subject->resultTypeSet()) {
    if (subject->resultTypeSet()->equals(type)) {
      return Ok();
    }
  } else {
    TemporaryTypeSet oldTypes(alloc_->lifoAlloc(), subject->type());
    if (oldTypes.equals(type)) {
      return Ok();
    }
  }

  MInstruction* replace = nullptr;
  MDefinition* ins;

  for (uint32_t i = 0; i < current->stackDepth(); i++) {
    ins = current->getSlot(i);

    // Instead of creating a new MFilterTypeSet, try to update the old one.
    if (ins->isFilterTypeSet() && ins->getOperand(0) == subject &&
        ins->dependency() == test) {
      TemporaryTypeSet* intersect = TypeSet::intersectSets(
          ins->resultTypeSet(), type, alloc_->lifoAlloc());
      if (!intersect) {
        return abort(AbortReason::Alloc);
      }

      ins->toFilterTypeSet()->setResultType(intersect->getKnownMIRType());
      ins->toFilterTypeSet()->setResultTypeSet(intersect);

      if (ins->type() == MIRType::Undefined) {
        current->setSlot(i, constant(UndefinedValue()));
      } else if (ins->type() == MIRType::Null) {
        current->setSlot(i, constant(NullValue()));
      } else if (ins->type() == MIRType::MagicOptimizedArguments) {
        current->setSlot(i, constant(MagicValue(JS_OPTIMIZED_ARGUMENTS)));
      } else {
        MOZ_ASSERT(!IsMagicType(ins->type()));
      }
      continue;
    }

    if (ins == subject) {
      if (!replace) {
        replace = MFilterTypeSet::New(alloc(), subject, type);
        current->add(replace);

        // Make sure we don't hoist it above the MTest, we can use the
        // 'dependency' of an MInstruction. This is normally used by
        // Alias Analysis, but won't get overwritten, since this
        // instruction doesn't have an AliasSet.
        replace->setDependency(test);

        if (replace->type() == MIRType::Undefined) {
          replace = constant(UndefinedValue());
        } else if (replace->type() == MIRType::Null) {
          replace = constant(NullValue());
        } else if (replace->type() == MIRType::MagicOptimizedArguments) {
          replace = constant(MagicValue(JS_OPTIMIZED_ARGUMENTS));
        } else {
          MOZ_ASSERT(!IsMagicType(ins->type()));
        }
      }
      current->setSlot(i, replace);
    }
  }
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::improveTypesAtCompare(MCompare* ins,
                                                    bool trueBranch,
                                                    MTest* test) {
  if (ins->compareType() == MCompare::Compare_Undefined ||
      ins->compareType() == MCompare::Compare_Null) {
    return improveTypesAtNullOrUndefinedCompare(ins, trueBranch, test);
  }

  if ((ins->lhs()->isTypeOf() || ins->rhs()->isTypeOf()) &&
      (ins->lhs()->isConstant() || ins->rhs()->isConstant())) {
    return improveTypesAtTypeOfCompare(ins, trueBranch, test);
  }

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::improveTypesAtTypeOfCompare(MCompare* ins,
                                                          bool trueBranch,
                                                          MTest* test) {
  MTypeOf* typeOf =
      ins->lhs()->isTypeOf() ? ins->lhs()->toTypeOf() : ins->rhs()->toTypeOf();
  MConstant* constant = ins->lhs()->isConstant() ? ins->lhs()->toConstant()
                                                 : ins->rhs()->toConstant();

  if (constant->type() != MIRType::String) {
    return Ok();
  }

  bool equal = ins->jsop() == JSOp::Eq || ins->jsop() == JSOp::StrictEq;
  bool notEqual = ins->jsop() == JSOp::Ne || ins->jsop() == JSOp::StrictNe;

  if (notEqual) {
    trueBranch = !trueBranch;
  }

  // Relational compares not supported.
  if (!equal && !notEqual) {
    return Ok();
  }

  MDefinition* subject = typeOf->input();
  TemporaryTypeSet* inputTypes = subject->resultTypeSet();

  // Create temporary typeset equal to the type if there is no resultTypeSet.
  TemporaryTypeSet tmp;
  if (!inputTypes) {
    if (subject->type() == MIRType::Value) {
      return Ok();
    }
    inputTypes = &tmp;
    tmp.addType(TypeSet::PrimitiveOrAnyObjectType(subject->type()),
                alloc_->lifoAlloc());
  }

  if (inputTypes->unknown()) {
    return Ok();
  }

  // Note: we cannot remove the AnyObject type in the false branch,
  // since there are multiple ways to get an object. That is the reason
  // for the 'trueBranch' test.
  TemporaryTypeSet filter;
  const JSAtomState& names = mirGen_.runtime->names();
  if (constant->toString() == TypeName(JSTYPE_UNDEFINED, names)) {
    filter.addType(TypeSet::UndefinedType(), alloc_->lifoAlloc());
    if (typeOf->inputMaybeCallableOrEmulatesUndefined() && trueBranch) {
      filter.addType(TypeSet::AnyObjectType(), alloc_->lifoAlloc());
    }
  } else if (constant->toString() == TypeName(JSTYPE_BOOLEAN, names)) {
    filter.addType(TypeSet::BooleanType(), alloc_->lifoAlloc());
  } else if (constant->toString() == TypeName(JSTYPE_NUMBER, names)) {
    filter.addType(TypeSet::Int32Type(), alloc_->lifoAlloc());
    filter.addType(TypeSet::DoubleType(), alloc_->lifoAlloc());
  } else if (constant->toString() == TypeName(JSTYPE_STRING, names)) {
    filter.addType(TypeSet::StringType(), alloc_->lifoAlloc());
  } else if (constant->toString() == TypeName(JSTYPE_SYMBOL, names)) {
    filter.addType(TypeSet::SymbolType(), alloc_->lifoAlloc());
  } else if (constant->toString() == TypeName(JSTYPE_BIGINT, names)) {
    filter.addType(TypeSet::BigIntType(), alloc_->lifoAlloc());
  } else if (constant->toString() == TypeName(JSTYPE_OBJECT, names)) {
    filter.addType(TypeSet::NullType(), alloc_->lifoAlloc());
    if (trueBranch) {
      filter.addType(TypeSet::AnyObjectType(), alloc_->lifoAlloc());
    }
  } else if (constant->toString() == TypeName(JSTYPE_FUNCTION, names)) {
    if (typeOf->inputMaybeCallableOrEmulatesUndefined() && trueBranch) {
      filter.addType(TypeSet::AnyObjectType(), alloc_->lifoAlloc());
    }
  } else {
    return Ok();
  }

  TemporaryTypeSet* type;
  if (trueBranch) {
    type = TypeSet::intersectSets(&filter, inputTypes, alloc_->lifoAlloc());
  } else {
    type = TypeSet::removeSet(inputTypes, &filter, alloc_->lifoAlloc());
  }

  if (!type) {
    return abort(AbortReason::Alloc);
  }

  return replaceTypeSet(subject, type, test);
}

AbortReasonOr<Ok> IonBuilder::improveTypesAtNullOrUndefinedCompare(
    MCompare* ins, bool trueBranch, MTest* test) {
  MOZ_ASSERT(ins->compareType() == MCompare::Compare_Undefined ||
             ins->compareType() == MCompare::Compare_Null);

  // altersUndefined/Null represents if we can filter/set Undefined/Null.
  bool altersUndefined, altersNull;
  JSOp op = ins->jsop();

  switch (op) {
    case JSOp::StrictNe:
    case JSOp::StrictEq:
      altersUndefined = ins->compareType() == MCompare::Compare_Undefined;
      altersNull = ins->compareType() == MCompare::Compare_Null;
      break;
    case JSOp::Ne:
    case JSOp::Eq:
      altersUndefined = altersNull = true;
      break;
    default:
      MOZ_CRASH("Relational compares not supported");
  }

  MDefinition* subject = ins->lhs();
  TemporaryTypeSet* inputTypes = subject->resultTypeSet();

  MOZ_ASSERT(IsNullOrUndefined(ins->rhs()->type()));

  // Create temporary typeset equal to the type if there is no resultTypeSet.
  TemporaryTypeSet tmp;
  if (!inputTypes) {
    if (subject->type() == MIRType::Value) {
      return Ok();
    }
    inputTypes = &tmp;
    tmp.addType(TypeSet::PrimitiveOrAnyObjectType(subject->type()),
                alloc_->lifoAlloc());
  }

  if (inputTypes->unknown()) {
    return Ok();
  }

  TemporaryTypeSet* type;

  // Decide if we need to filter the type or set it.
  if ((op == JSOp::StrictEq || op == JSOp::Eq) ^ trueBranch) {
    // Remove undefined/null
    TemporaryTypeSet remove;
    if (altersUndefined) {
      remove.addType(TypeSet::UndefinedType(), alloc_->lifoAlloc());
    }
    if (altersNull) {
      remove.addType(TypeSet::NullType(), alloc_->lifoAlloc());
    }

    type = TypeSet::removeSet(inputTypes, &remove, alloc_->lifoAlloc());
  } else {
    // Set undefined/null.
    TemporaryTypeSet base;
    if (altersUndefined) {
      base.addType(TypeSet::UndefinedType(), alloc_->lifoAlloc());
      // If TypeSet emulates undefined, then we cannot filter the objects.
      if (inputTypes->maybeEmulatesUndefined(constraints())) {
        base.addType(TypeSet::AnyObjectType(), alloc_->lifoAlloc());
      }
    }

    if (altersNull) {
      base.addType(TypeSet::NullType(), alloc_->lifoAlloc());
    }

    type = TypeSet::intersectSets(&base, inputTypes, alloc_->lifoAlloc());
  }

  if (!type) {
    return abort(AbortReason::Alloc);
  }

  return replaceTypeSet(subject, type, test);
}

AbortReasonOr<Ok> IonBuilder::improveTypesAtTestSuccessor(
    MTest* test, MBasicBlock* successor) {
  MOZ_ASSERT(successor->numPredecessors() == 1);
  MOZ_ASSERT(test->block() == successor->getPredecessor(0));

  MOZ_ASSERT(test->ifTrue() == successor || test->ifFalse() == successor);
  bool trueBranch = test->ifTrue() == successor;

  return improveTypesAtTest(test->getOperand(0), trueBranch, test);
}

AbortReasonOr<Ok> IonBuilder::improveTypesAtTest(MDefinition* ins,
                                                 bool trueBranch, MTest* test) {
  // We explore the test condition to try and deduce as much type information
  // as possible.

  // All branches of this switch that don't want to fall through to the
  // default behavior must return.  The default behavior assumes that a true
  // test means the incoming ins is not null or undefined and that a false
  // tests means it's one of null, undefined, false, 0, "", and objects
  // emulating undefined
  switch (ins->op()) {
    case MDefinition::Opcode::Not:
      return improveTypesAtTest(ins->toNot()->getOperand(0), !trueBranch, test);
    case MDefinition::Opcode::IsObject: {
      MDefinition* subject = ins->getOperand(0);
      TemporaryTypeSet* oldType = subject->resultTypeSet();

      // Create temporary typeset equal to the type if there is no
      // resultTypeSet.
      TemporaryTypeSet tmp;
      if (!oldType) {
        if (subject->type() == MIRType::Value) {
          return Ok();
        }
        oldType = &tmp;
        tmp.addType(TypeSet::PrimitiveOrAnyObjectType(subject->type()),
                    alloc_->lifoAlloc());
      }

      if (oldType->unknown()) {
        return Ok();
      }

      TemporaryTypeSet* type = nullptr;
      if (trueBranch) {
        type = oldType->cloneObjectsOnly(alloc_->lifoAlloc());
      } else {
        type = oldType->cloneWithoutObjects(alloc_->lifoAlloc());
      }

      if (!type) {
        return abort(AbortReason::Alloc);
      }

      return replaceTypeSet(subject, type, test);
    }
    case MDefinition::Opcode::IsNullOrUndefined: {
      MDefinition* subject = ins->getOperand(0);
      TemporaryTypeSet* oldType = subject->resultTypeSet();

      // Create temporary typeset equal to the type if there is no
      // resultTypeSet.
      TemporaryTypeSet tmp;
      if (!oldType) {
        if (subject->type() == MIRType::Value) {
          return Ok();
        }
        oldType = &tmp;
        tmp.addType(TypeSet::PrimitiveOrAnyObjectType(subject->type()),
                    alloc_->lifoAlloc());
      }

      // If ins does not have a typeset we return as we cannot optimize.
      if (oldType->unknown()) {
        return Ok();
      }

      // Decide either to set or remove.
      TemporaryTypeSet filter;
      filter.addType(TypeSet::UndefinedType(), alloc_->lifoAlloc());
      filter.addType(TypeSet::NullType(), alloc_->lifoAlloc());

      TemporaryTypeSet* type;
      if (trueBranch) {
        type = TypeSet::intersectSets(&filter, oldType, alloc_->lifoAlloc());
      } else {
        type = TypeSet::removeSet(oldType, &filter, alloc_->lifoAlloc());
      }

      if (!type) {
        return abort(AbortReason::Alloc);
      }

      return replaceTypeSet(subject, type, test);
    }

    case MDefinition::Opcode::Compare:
      return improveTypesAtCompare(ins->toCompare(), trueBranch, test);

    default:
      break;
  }

  // By default MTest tests ToBoolean(input). As a result in the true branch we
  // can filter undefined and null. In false branch we can only encounter
  // undefined, null, false, 0, "" and objects that emulate undefined.

  TemporaryTypeSet* oldType = ins->resultTypeSet();
  TemporaryTypeSet* type;

  // Create temporary typeset equal to the type if there is no resultTypeSet.
  TemporaryTypeSet tmp;
  if (!oldType) {
    if (ins->type() == MIRType::Value) {
      return Ok();
    }
    oldType = &tmp;
    tmp.addType(TypeSet::PrimitiveOrAnyObjectType(ins->type()),
                alloc_->lifoAlloc());
  }

  // If ins does not have a typeset we return as we cannot optimize.
  if (oldType->unknown()) {
    return Ok();
  }

  // Decide either to set or remove.
  if (trueBranch) {
    TemporaryTypeSet remove;
    remove.addType(TypeSet::UndefinedType(), alloc_->lifoAlloc());
    remove.addType(TypeSet::NullType(), alloc_->lifoAlloc());
    type = TypeSet::removeSet(oldType, &remove, alloc_->lifoAlloc());
  } else {
    TemporaryTypeSet base;
    base.addType(TypeSet::UndefinedType(),
                 alloc_->lifoAlloc());  // ToBoolean(undefined) == false
    base.addType(TypeSet::NullType(),
                 alloc_->lifoAlloc());  // ToBoolean(null) == false
    base.addType(TypeSet::BooleanType(),
                 alloc_->lifoAlloc());  // ToBoolean(false) == false
    base.addType(TypeSet::Int32Type(),
                 alloc_->lifoAlloc());  // ToBoolean(0) == false
    base.addType(TypeSet::DoubleType(),
                 alloc_->lifoAlloc());  // ToBoolean(0.0) == false
    base.addType(TypeSet::StringType(),
                 alloc_->lifoAlloc());  // ToBoolean("") == false
    base.addType(TypeSet::BigIntType(),
                 alloc_->lifoAlloc());  // ToBoolean(0n) == false

    // If the typeset does emulate undefined, then we cannot filter out
    // objects.
    if (oldType->maybeEmulatesUndefined(constraints())) {
      base.addType(TypeSet::AnyObjectType(), alloc_->lifoAlloc());
    }

    type = TypeSet::intersectSets(&base, oldType, alloc_->lifoAlloc());
  }

  if (!type) {
    return abort(AbortReason::Alloc);
  }

  return replaceTypeSet(ins, type, test);
}

AbortReasonOr<Ok> IonBuilder::jsop_dup2() {
  uint32_t lhsSlot = current->stackDepth() - 2;
  uint32_t rhsSlot = current->stackDepth() - 1;
  current->pushSlot(lhsSlot);
  current->pushSlot(rhsSlot);
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::visitTestBackedge(JSOp op, bool* restarted) {
  MOZ_ASSERT(op == JSOp::IfNe);
  MOZ_ASSERT(loopDepth_ > 0);

  MDefinition* ins = current->pop();

  jsbytecode* loopHead = pc + GET_JUMP_OFFSET(pc);
  MOZ_ASSERT(JSOp(*loopHead) == JSOp::LoopHead);

  jsbytecode* successorPC = GetNextPc(pc);

  // We can finish the loop now. Use the loophead pc instead of the current pc
  // because the stack depth at the start of that op matches the current stack
  // depth (after popping our operand).
  MBasicBlock* backedge;
  MOZ_TRY_VAR(backedge, newBlock(current, loopHead));

  current->end(newTest(ins, backedge, nullptr));
  MOZ_TRY(addPendingEdge(PendingEdge::NewTestFalse(current, op), successorPC));

  MOZ_TRY(startTraversingBlock(backedge));
  return visitBackEdge(restarted);
}

AbortReasonOr<Ok> IonBuilder::visitTest(JSOp op, bool* restarted) {
  MOZ_ASSERT(op == JSOp::IfEq || op == JSOp::IfNe || op == JSOp::And ||
             op == JSOp::Or || op == JSOp::Case);

  if (IsBackedgePC(pc)) {
    return visitTestBackedge(op, restarted);
  }

  jsbytecode* target1 = GetNextPc(pc);
  jsbytecode* target2 = pc + GET_JUMP_OFFSET(pc);

  // JSOp::And and JSOp::Or inspect the top stack value but don't pop it.
  // Also note that JSOp::Case must pop a second value on the true-branch (the
  // input to the switch-statement). This conditional pop happens in
  // visitJumpTarget.
  bool mustKeepCondition = (op == JSOp::And || op == JSOp::Or);
  MDefinition* ins = mustKeepCondition ? current->peek(-1) : current->pop();

  // If this op always branches to the same pc we treat this as a JSOp::Goto.
  if (target1 == target2) {
    ins->setImplicitlyUsedUnchecked();
    return visitGoto(target1);
  }

  MTest* mir = newTest(ins, nullptr, nullptr);
  current->end(mir);

  if (TestTrueTargetIsJoinPoint(op)) {
    std::swap(target1, target2);
  }

  MOZ_TRY(addPendingEdge(PendingEdge::NewTestTrue(current, op), target1));
  MOZ_TRY(addPendingEdge(PendingEdge::NewTestFalse(current, op), target2));
  setTerminatedBlock();

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_coalesce() {
  jsbytecode* target1 = GetNextPc(pc);
  jsbytecode* target2 = pc + GET_JUMP_OFFSET(pc);
  MOZ_ASSERT(target2 > target1);

  MDefinition* ins = current->peek(-1);

  MIsNullOrUndefined* isNullOrUndefined = MIsNullOrUndefined::New(alloc(), ins);
  current->add(isNullOrUndefined);

  MTest* mir = newTest(isNullOrUndefined, nullptr, nullptr);
  current->end(mir);

  MOZ_TRY(addPendingEdge(PendingEdge::NewTestTrue(current, JSOp::Coalesce),
                         target1));
  MOZ_TRY(addPendingEdge(PendingEdge::NewTestFalse(current, JSOp::Coalesce),
                         target2));
  setTerminatedBlock();

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::visitTry() {
  // We don't support try-finally.
  if (script()->jitScript()->hasTryFinally()) {
    return abort(AbortReason::Disable, "Try-finally not supported");
  }

  // Try-catch within inline frames is not yet supported.
  if (isInlineBuilder()) {
    return abort(AbortReason::Disable, "Try-catch during inlining");
  }

  // Try-catch during analyses is not yet supported. Code within the 'catch'
  // block is not accounted for.
  if (info().isAnalysis()) {
    return abort(AbortReason::Disable, "Try-catch during analysis");
  }

  // Get the pc of the last instruction in the try block. It's a JSOp::Goto to
  // jump over the catch block.
  jsbytecode* endpc = pc + GET_CODE_OFFSET(pc);
  MOZ_ASSERT(JSOp(*endpc) == JSOp::Goto);
  MOZ_ASSERT(GET_JUMP_OFFSET(endpc) > 0);

  jsbytecode* afterTry = endpc + GET_JUMP_OFFSET(endpc);

  // The baseline compiler should not attempt to enter the catch block
  // via OSR.
  MOZ_ASSERT(info().osrPc() < endpc || info().osrPc() >= afterTry);

  // If controlflow in the try body is terminated (by a return or throw
  // statement), the code after the try-statement may still be reachable
  // via the catch block (which we don't compile) and OSR can enter it.
  // For example:
  //
  //     try {
  //         throw 3;
  //     } catch(e) { }
  //
  //     for (var i=0; i<1000; i++) {}
  //
  // To handle this, we create two blocks: one for the try block and one
  // for the code following the try-catch statement.

  graph().setHasTryBlock();

  MBasicBlock* tryBlock;
  MOZ_TRY_VAR(tryBlock, newBlock(current, GetNextPc(pc)));

  current->end(MGotoWithFake::New(alloc(), tryBlock, nullptr));
  MOZ_TRY(addPendingEdge(PendingEdge::NewGotoWithFake(current), afterTry));

  return startTraversingBlock(tryBlock);
}

AbortReasonOr<Ok> IonBuilder::visitJumpTarget(JSOp op) {
  PendingEdgesMap::Ptr p = pendingEdges_.lookup(pc);
  if (!p) {
    // No (reachable) jumps so this is just a no-op.
    return Ok();
  }

  PendingEdges edges(std::move(p->value()));
  pendingEdges_.remove(p);

  // Loop-restarts may clear the list rather than remove the map entry entirely.
  // This is to reduce allocator churn since it is likely the list will be
  // filled in again in the general case.
  if (edges.empty()) {
    return Ok();
  }

  MBasicBlock* joinBlock = nullptr;

  // Create join block if there's fall-through from the previous bytecode op.
  if (!hasTerminatedBlock()) {
    MOZ_TRY_VAR(joinBlock, newBlock(current, pc));
    current->end(MGoto::New(alloc(), joinBlock));
    setTerminatedBlock();
  }

  auto addEdge = [&](MBasicBlock* pred, size_t numToPop) -> AbortReasonOr<Ok> {
    if (joinBlock) {
      MOZ_ASSERT(pred->stackDepth() - numToPop == joinBlock->stackDepth());
      if (!joinBlock->addPredecessorPopN(alloc(), pred, numToPop)) {
        return abort(AbortReason::Alloc);
      }
      return Ok();
    }
    MOZ_TRY_VAR(joinBlock, newBlockPopN(pred, pc, numToPop));
    return Ok();
  };

  // When a block is terminated with an MTest instruction we can end up with the
  // following triangle structure:
  //
  //        testBlock
  //         /    |
  //     block    |
  //         \    |
  //        joinBlock
  //
  // Although this is fine for correctness, it has the following issues:
  //
  // 1) The FoldTests pass is unable to optimize this pattern. This matters for
  //    short-circuit operations (JSOp::And, JSOp::Coalesce, etc).
  //
  // 2) We can't easily use improveTypesAtTest to improve type information in
  //    this case:
  //
  //        var obj = ...;
  //        if (obj === null) {
  //          obj = {};
  //        }
  //        ... obj must be non-null ...
  //
  // To fix these issues, we create an empty block to get a diamond structure:
  //
  //        testBlock
  //         /    |
  //     block  emptyBlock
  //         \    |
  //        joinBlock
  auto createEmptyBlockForTest =
      [&](MBasicBlock* pred, size_t successor,
          size_t numToPop) -> AbortReasonOr<MBasicBlock*> {
    MOZ_ASSERT(joinBlock);

    MBasicBlock* emptyBlock;
    MOZ_TRY_VAR(emptyBlock, newBlockPopN(pred, pc, numToPop));
    MOZ_ASSERT(emptyBlock->stackDepth() == joinBlock->stackDepth());

    MTest* test = pred->lastIns()->toTest();
    test->initSuccessor(successor, emptyBlock);

    MOZ_TRY(startTraversingBlock(emptyBlock));
    MOZ_TRY(improveTypesAtTestSuccessor(test, emptyBlock));

    emptyBlock->end(MGoto::New(alloc(), joinBlock));
    setTerminatedBlock();

    return emptyBlock;
  };

  for (const PendingEdge& edge : edges) {
    MBasicBlock* source = edge.block();
    MControlInstruction* lastIns = source->lastIns();
    switch (edge.kind()) {
      case PendingEdge::Kind::TestTrue: {
        // JSOp::Case must pop the value when branching to the true-target.
        // If we create an empty block, we have to pop the value there instead
        // of as part of the emptyBlock -> joinBlock edge so stack depths match
        // the current depth.
        const size_t numToPop = (edge.testOp() == JSOp::Case) ? 1 : 0;

        const size_t successor = 0;  // true-branch
        if (joinBlock && TestTrueTargetIsJoinPoint(edge.testOp())) {
          MBasicBlock* pred;
          MOZ_TRY_VAR(pred,
                      createEmptyBlockForTest(source, successor, numToPop));
          MOZ_TRY(addEdge(pred, 0));
        } else {
          MOZ_TRY(addEdge(source, numToPop));
          lastIns->toTest()->initSuccessor(successor, joinBlock);
        }
        continue;
      }

      case PendingEdge::Kind::TestFalse: {
        const size_t numToPop = 0;
        const size_t successor = 1;  // false-branch
        if (joinBlock && !TestTrueTargetIsJoinPoint(edge.testOp())) {
          MBasicBlock* pred;
          MOZ_TRY_VAR(pred,
                      createEmptyBlockForTest(source, successor, numToPop));
          MOZ_TRY(addEdge(pred, 0));
        } else {
          MOZ_TRY(addEdge(source, numToPop));
          lastIns->toTest()->initSuccessor(successor, joinBlock);
        }
        continue;
      }

      case PendingEdge::Kind::Goto:
        MOZ_TRY(addEdge(source, 0));
        lastIns->toGoto()->initSuccessor(0, joinBlock);
        continue;

      case PendingEdge::Kind::GotoWithFake:
        MOZ_TRY(addEdge(source, 0));
        lastIns->toGotoWithFake()->initSuccessor(1, joinBlock);
        continue;
    }
    MOZ_CRASH("Invalid kind");
  }

  MOZ_ASSERT(joinBlock);
  MOZ_TRY(startTraversingBlock(joinBlock));

  // If the join block has just one predecessor with an MTest, try to improve
  // type information.
  if (joinBlock->numPredecessors() == 1) {
    MBasicBlock* pred = joinBlock->getPredecessor(0);
    if (pred->lastIns()->isTest()) {
      MTest* test = pred->lastIns()->toTest();
      MOZ_TRY(improveTypesAtTestSuccessor(test, joinBlock));
    }
  }

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::visitReturn(JSOp op) {
  MDefinition* def;
  switch (op) {
    case JSOp::Return:
      // Return the last instruction.
      def = current->pop();
      break;

    case JSOp::RetRval:
      // Return undefined eagerly if script doesn't use return value.
      if (script()->noScriptRval()) {
        MInstruction* ins = MConstant::New(alloc(), UndefinedValue());
        current->add(ins);
        def = ins;
        break;
      }

      def = current->getSlot(info().returnValueSlot());
      break;

    default:
      MOZ_CRASH("unknown return op");
  }

  MReturn* ret = MReturn::New(alloc(), def);
  current->end(ret);

  if (!graph().addReturn(current)) {
    return abort(AbortReason::Alloc);
  }

  setTerminatedBlock();

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::visitThrow() {
  MDefinition* def = current->pop();

  // MThrow is not marked as effectful. This means when it throws and we
  // are inside a try block, we could use an earlier resume point and this
  // resume point may not be up-to-date, for example:
  //
  // (function() {
  //     try {
  //         var x = 1;
  //         foo(); // resume point
  //         x = 2;
  //         throw foo;
  //     } catch(e) {
  //         print(x);
  //     }
  // ])();
  //
  // If we use the resume point after the call, this will print 1 instead
  // of 2. To fix this, we create a resume point right before the MThrow.
  //
  // Note that this is not a problem for instructions other than MThrow
  // because they are either marked as effectful (have their own resume
  // point) or cannot throw a catchable exception.
  //
  // We always install this resume point (instead of only when the function
  // has a try block) in order to handle the Debugger onExceptionUnwind
  // hook. When we need to handle the hook, we bail out to baseline right
  // after the throw and propagate the exception when debug mode is on. This
  // is opposed to the normal behavior of resuming directly in the
  // associated catch block.
  MNop* nop = MNop::New(alloc());
  current->add(nop);

  MOZ_TRY(resumeAfter(nop));

  MThrow* ins = MThrow::New(alloc(), def);
  current->end(ins);

  setTerminatedBlock();

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::visitTableSwitch() {
  jsbytecode* defaultpc = pc + GET_JUMP_OFFSET(pc);

  int32_t low = GET_JUMP_OFFSET(pc + JUMP_OFFSET_LEN * 1);
  int32_t high = GET_JUMP_OFFSET(pc + JUMP_OFFSET_LEN * 2);
  size_t numCases = high - low + 1;

  // Create MIR instruction.
  MDefinition* ins = current->pop();
  MTableSwitch* tableswitch = MTableSwitch::New(alloc(), ins, low, high);
  current->end(tableswitch);

  MBasicBlock* switchBlock = current;

  // Create |default| block.
  {
    MBasicBlock* defaultBlock;
    MOZ_TRY_VAR(defaultBlock, newBlock(switchBlock, defaultpc));

    size_t index;
    if (!tableswitch->addDefault(defaultBlock, &index)) {
      return abort(AbortReason::Alloc);
    }
    MOZ_ASSERT(index == 0);

    MOZ_TRY(startTraversingBlock(defaultBlock));

    defaultBlock->end(MGoto::New(alloc(), nullptr));
    MOZ_TRY(addPendingEdge(PendingEdge::NewGoto(defaultBlock), defaultpc));
  }

  // Create blocks for all cases.
  for (size_t i = 0; i < numCases; i++) {
    jsbytecode* casepc = script()->tableSwitchCasePC(pc, i);

    MBasicBlock* caseBlock;
    MOZ_TRY_VAR(caseBlock, newBlock(switchBlock, casepc));

    size_t index;
    if (!tableswitch->addSuccessor(caseBlock, &index)) {
      return abort(AbortReason::Alloc);
    }
    if (!tableswitch->addCase(index)) {
      return abort(AbortReason::Alloc);
    }

    MOZ_TRY(startTraversingBlock(caseBlock));

    // If this is an actual case statement, optimize by replacing the
    // input to the switch case with the actual number of the case.
    MConstant* constant = MConstant::New(alloc(), Int32Value(low + int32_t(i)));
    caseBlock->add(constant);
    for (uint32_t j = 0; j < caseBlock->stackDepth(); j++) {
      if (ins != caseBlock->getSlot(j)) {
        continue;
      }

      constant->setDependency(ins);
      caseBlock->setSlot(j, constant);
    }

    caseBlock->end(MGoto::New(alloc(), nullptr));
    MOZ_TRY(addPendingEdge(PendingEdge::NewGoto(caseBlock), casepc));
  }

  setTerminatedBlock();

  return Ok();
}

void IonBuilder::pushConstant(const Value& v) { current->push(constant(v)); }

AbortReasonOr<Ok> IonBuilder::bitnotTrySpecialized(bool* emitted,
                                                   MDefinition* input) {
  MOZ_ASSERT(*emitted == false);

  // Try to emit a specialized bitnot instruction based on the input type
  // of the operand.

  if (input->mightBeType(MIRType::Object) ||
      input->mightBeType(MIRType::Symbol) ||
      input->mightBeType(MIRType::BigInt)) {
    return Ok();
  }

  MBitNot* ins = MBitNot::New(alloc(), input);
  ins->setSpecialization(MIRType::Int32);

  current->add(ins);
  current->push(ins);

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_bitnot() {
  bool emitted = false;

  MDefinition* input = current->pop();

  if (!forceInlineCaches()) {
    MOZ_TRY(bitnotTrySpecialized(&emitted, input));
    if (emitted) return Ok();
  }

  MOZ_TRY(arithTryBinaryStub(&emitted, JSOp::BitNot, nullptr, input));
  if (emitted) {
    return Ok();
  }

  // Not possible to optimize. Do a slow vm call.
  MBitNot* ins = MBitNot::New(alloc(), input);

  current->add(ins);
  current->push(ins);
  MOZ_ASSERT(ins->isEffectful());
  return resumeAfter(ins);
}

AbortReasonOr<MBinaryBitwiseInstruction*> IonBuilder::binaryBitOpEmit(
    JSOp op, MIRType specialization, MDefinition* left, MDefinition* right) {
  MOZ_ASSERT(specialization == MIRType::Int32 ||
             specialization == MIRType::None);

  MBinaryBitwiseInstruction* ins;
  switch (op) {
    case JSOp::BitAnd:
      ins = MBitAnd::New(alloc(), left, right);
      break;

    case JSOp::BitOr:
      ins = MBitOr::New(alloc(), left, right);
      break;

    case JSOp::BitXor:
      ins = MBitXor::New(alloc(), left, right);
      break;

    case JSOp::Lsh:
      ins = MLsh::New(alloc(), left, right);
      break;

    case JSOp::Rsh:
      ins = MRsh::New(alloc(), left, right);
      break;

    case JSOp::Ursh:
      ins = MUrsh::New(alloc(), left, right);
      break;

    default:
      MOZ_CRASH("unexpected bitop");
  }

  current->add(ins);
  ins->infer(inspector, pc);

  // The expected specialization should match the inferred specialization.
  MOZ_ASSERT_IF(specialization == MIRType::None,
                ins->specialization() == MIRType::None);
  MOZ_ASSERT_IF(
      specialization == MIRType::Int32,
      ins->specialization() == MIRType::Int32 ||
          (op == JSOp::Ursh && ins->specialization() == MIRType::Double));

  current->push(ins);
  if (ins->isEffectful()) {
    MOZ_TRY(resumeAfter(ins));
  }

  return ins;
}

static inline bool SimpleBitOpOperand(MDefinition* op) {
  return !op->mightBeType(MIRType::Object) &&
         !op->mightBeType(MIRType::Symbol) && !op->mightBeType(MIRType::BigInt);
}

AbortReasonOr<Ok> IonBuilder::binaryBitOpTrySpecialized(bool* emitted, JSOp op,
                                                        MDefinition* left,
                                                        MDefinition* right) {
  MOZ_ASSERT(*emitted == false);

  // Try to emit a specialized binary instruction based on the input types
  // of the operands.

  // Anything complex - objects, symbols, and bigints - are not specialized
  if (!SimpleBitOpOperand(left) || !SimpleBitOpOperand(right)) {
    return Ok();
  }

  MIRType specialization = MIRType::Int32;
  MOZ_TRY(binaryBitOpEmit(op, specialization, left, right));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_bitop(JSOp op) {
  // Pop inputs.
  MDefinition* right = current->pop();
  MDefinition* left = current->pop();

  bool emitted = false;

  if (!forceInlineCaches()) {
    MOZ_TRY(binaryBitOpTrySpecialized(&emitted, op, left, right));
    if (emitted) {
      return Ok();
    }
  }

  MOZ_TRY(arithTryBinaryStub(&emitted, op, left, right));
  if (emitted) {
    return Ok();
  }

  // Not possible to optimize. Do a slow vm call.
  MOZ_TRY(binaryBitOpEmit(op, MIRType::None, left, right));
  return Ok();
}

MDefinition::Opcode BinaryJSOpToMDefinition(JSOp op) {
  switch (op) {
    case JSOp::Add:
      return MDefinition::Opcode::Add;
    case JSOp::Sub:
      return MDefinition::Opcode::Sub;
    case JSOp::Mul:
      return MDefinition::Opcode::Mul;
    case JSOp::Div:
      return MDefinition::Opcode::Div;
    case JSOp::Mod:
      return MDefinition::Opcode::Mod;
    default:
      MOZ_CRASH("unexpected binary opcode");
  }
}

AbortReasonOr<Ok> IonBuilder::binaryArithTryConcat(bool* emitted, JSOp op,
                                                   MDefinition* left,
                                                   MDefinition* right) {
  MOZ_ASSERT(*emitted == false);

  // Try to convert an addition into a concat operation if the inputs
  // indicate this might be a concatenation.

  // Only try to replace this with concat when we have an addition.
  if (op != JSOp::Add) {
    return Ok();
  }

  // Make sure one of the inputs is a string.
  if (left->type() != MIRType::String && right->type() != MIRType::String) {
    return Ok();
  }

  // The non-string input (if present) should be atleast easily coercible to
  // string.
  if (right->type() != MIRType::String &&
      (right->mightBeType(MIRType::Symbol) ||
       right->mightBeType(MIRType::Object) || right->mightBeMagicType())) {
    return Ok();
  }
  if (left->type() != MIRType::String &&
      (left->mightBeType(MIRType::Symbol) ||
       left->mightBeType(MIRType::Object) || left->mightBeMagicType())) {
    return Ok();
  }

  MConcat* ins = MConcat::New(alloc(), left, right);
  current->add(ins);
  current->push(ins);

  MOZ_TRY(maybeInsertResume());

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::powTrySpecialized(bool* emitted,
                                                MDefinition* base,
                                                MDefinition* power,
                                                MIRType outputType) {
  // Typechecking.
  MDefinition* output = nullptr;
  MIRType baseType = base->type();
  MIRType powerType = power->type();

  if (outputType != MIRType::Int32 && outputType != MIRType::Double) {
    return Ok();
  }
  if (!IsNumberType(baseType)) {
    return Ok();
  }
  if (!IsNumberType(powerType)) {
    return Ok();
  }

  if (powerType == MIRType::Float32) {
    powerType = MIRType::Double;
  }

  MPow* pow = MPow::New(alloc(), base, power, powerType);
  current->add(pow);
  output = pow;

  // Cast to the right type
  if (outputType == MIRType::Int32 && output->type() != MIRType::Int32) {
    auto* toInt = MToNumberInt32::New(alloc(), output);
    current->add(toInt);
    output = toInt;
  }
  if (outputType == MIRType::Double && output->type() != MIRType::Double) {
    MToDouble* toDouble = MToDouble::New(alloc(), output);
    current->add(toDouble);
    output = toDouble;
  }

  current->push(output);
  *emitted = true;
  return Ok();
}

MIRType IonBuilder::binaryArithNumberSpecialization(MDefinition* left,
                                                    MDefinition* right) {
  // Try to specialize as int32.
  if (left->type() == MIRType::Int32 && right->type() == MIRType::Int32 &&
      !inspector->hasSeenDoubleResult(pc)) {
    return MIRType::Int32;
  }
  return MIRType::Double;
}

AbortReasonOr<MBinaryArithInstruction*> IonBuilder::binaryArithEmitSpecialized(
    MDefinition::Opcode op, MIRType specialization, MDefinition* left,
    MDefinition* right) {
  MBinaryArithInstruction* ins =
      MBinaryArithInstruction::New(alloc(), op, left, right);
  ins->setSpecialization(specialization);

  if (op == MDefinition::Opcode::Add || op == MDefinition::Opcode::Mul) {
    ins->setCommutative();
  }

  current->add(ins);
  current->push(ins);

  MOZ_ASSERT(!ins->isEffectful());
  MOZ_TRY(maybeInsertResume());

  return ins;
}

AbortReasonOr<Ok> IonBuilder::binaryArithTrySpecialized(bool* emitted, JSOp op,
                                                        MDefinition* left,
                                                        MDefinition* right) {
  MOZ_ASSERT(*emitted == false);

  // Try to emit a specialized binary instruction based on the input types
  // of the operands.

  // Anything complex - strings, symbols, and objects - are not specialized
  if (!SimpleArithOperand(left) || !SimpleArithOperand(right)) {
    return Ok();
  }

  // One of the inputs need to be a number.
  if (!IsNumberType(left->type()) && !IsNumberType(right->type())) {
    return Ok();
  }

  MDefinition::Opcode defOp = BinaryJSOpToMDefinition(op);
  MIRType specialization = binaryArithNumberSpecialization(left, right);
  MBinaryArithInstruction* ins;
  MOZ_TRY_VAR(ins,
              binaryArithEmitSpecialized(defOp, specialization, left, right));

  // Relax int32 to double if, despite the fact that we have int32 operands and
  // we've never seen a double result, we know the result may overflow or be a
  // double.
  if (specialization == MIRType::Int32 && ins->constantDoubleResult(alloc())) {
    ins->setSpecialization(MIRType::Double);
  }

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::binaryArithTrySpecializedOnBaselineInspector(
    bool* emitted, JSOp op, MDefinition* left, MDefinition* right) {
  MOZ_ASSERT(*emitted == false);

  // Try to emit a specialized binary instruction speculating the
  // type using the baseline caches.

  MIRType specialization = inspector->expectedBinaryArithSpecialization(pc);
  if (specialization == MIRType::None) {
    return Ok();
  }

  MDefinition::Opcode defOp = BinaryJSOpToMDefinition(op);
  MOZ_TRY(binaryArithEmitSpecialized(defOp, specialization, left, right));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::arithTryBinaryStub(bool* emitted, JSOp op,
                                                 MDefinition* left,
                                                 MDefinition* right) {
  MOZ_ASSERT(*emitted == false);
  JSOp actualOp = JSOp(*pc);

  // The actual jsop 'jsop_pos' is not supported yet.
  // There's no IC support for JSOp::Pow either.
  if (actualOp == JSOp::Pos || actualOp == JSOp::Pow) {
    return Ok();
  }

  MInstruction* stub = nullptr;
  switch (actualOp) {
    case JSOp::Neg:
    case JSOp::BitNot:
      MOZ_ASSERT_IF(op == JSOp::Mul,
                    left->maybeConstantValue() &&
                        left->maybeConstantValue()->toInt32() == -1);
      MOZ_ASSERT_IF(op != JSOp::Mul, !left);
      stub = MUnaryCache::New(alloc(), right);
      break;
    case JSOp::Add:
    case JSOp::Sub:
    case JSOp::Mul:
    case JSOp::Div:
    case JSOp::Mod:
    case JSOp::BitAnd:
    case JSOp::BitOr:
    case JSOp::BitXor:
    case JSOp::Lsh:
    case JSOp::Rsh:
    case JSOp::Ursh:
      stub = MBinaryCache::New(alloc(), left, right, MIRType::Value);
      break;
    default:
      MOZ_CRASH("unsupported arith");
  }

  current->add(stub);
  current->push(stub);

  // Decrease type from 'any type' to 'empty type' when one of the operands
  // is 'empty typed'.
  maybeMarkEmpty(stub);

  MOZ_TRY(resumeAfter(stub));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_binary_arith(JSOp op, MDefinition* left,
                                                MDefinition* right) {
  bool emitted = false;

  if (!forceInlineCaches()) {
    MOZ_TRY(binaryArithTryConcat(&emitted, op, left, right));
    if (emitted) {
      return Ok();
    }

    MOZ_TRY(binaryArithTrySpecialized(&emitted, op, left, right));
    if (emitted) {
      return Ok();
    }

    MOZ_TRY(binaryArithTrySpecializedOnBaselineInspector(&emitted, op, left,
                                                         right));
    if (emitted) {
      return Ok();
    }
  }

  MOZ_TRY(arithTryBinaryStub(&emitted, op, left, right));
  if (emitted) {
    return Ok();
  }

  MDefinition::Opcode defOp = BinaryJSOpToMDefinition(op);
  MBinaryArithInstruction* ins =
      MBinaryArithInstruction::New(alloc(), defOp, left, right);

  // Decrease type from 'any type' to 'empty type' when one of the operands
  // is 'empty typed'.
  maybeMarkEmpty(ins);

  current->add(ins);
  current->push(ins);
  MOZ_ASSERT(ins->isEffectful());
  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_binary_arith(JSOp op) {
  MDefinition* right = current->pop();
  MDefinition* left = current->pop();

  return jsop_binary_arith(op, left, right);
}

AbortReasonOr<Ok> IonBuilder::jsop_pow() {
  MDefinition* exponent = current->pop();
  MDefinition* base = current->pop();

  bool emitted = false;

  if (!forceInlineCaches()) {
    MOZ_TRY(powTrySpecialized(&emitted, base, exponent, MIRType::Double));
    if (emitted) {
      return Ok();
    }
  }

  MOZ_TRY(arithTryBinaryStub(&emitted, JSOp::Pow, base, exponent));
  if (emitted) {
    return Ok();
  }

  // For now, use MIRType::None as a safe cover-all. See bug 1188079.
  MPow* pow = MPow::New(alloc(), base, exponent, MIRType::None);
  current->add(pow);
  current->push(pow);
  MOZ_ASSERT(pow->isEffectful());
  return resumeAfter(pow);
}

AbortReasonOr<Ok> IonBuilder::jsop_pos() {
  if (IsNumberType(current->peek(-1)->type())) {
    // Already int32 or double. Set the operand as implicitly used so it
    // doesn't get optimized out if it has no other uses, as we could bail
    // out.
    current->peek(-1)->setImplicitlyUsedUnchecked();
    return Ok();
  }

  // Compile +x as x * 1.
  MDefinition* value = current->pop();
  MConstant* one = MConstant::New(alloc(), Int32Value(1));
  current->add(one);

  return jsop_binary_arith(JSOp::Mul, value, one);
}

AbortReasonOr<Ok> IonBuilder::jsop_neg() {
  // Since JSOp::Neg does not use a slot, we cannot push the MConstant.
  // The MConstant is therefore passed to JSOp::Mul without slot traffic.
  MConstant* negator = MConstant::New(alloc(), Int32Value(-1));
  current->add(negator);

  MDefinition* right = current->pop();

  return jsop_binary_arith(JSOp::Mul, negator, right);
}

AbortReasonOr<Ok> IonBuilder::jsop_tonumeric() {
  MDefinition* peeked = current->peek(-1);

  if (IsNumericType(peeked->type())) {
    // Elide the ToNumeric as we already unboxed the value.
    peeked->setImplicitlyUsedUnchecked();
    return Ok();
  }

  LifoAlloc* lifoAlloc = alloc().lifoAlloc();
  TemporaryTypeSet* types = lifoAlloc->new_<TemporaryTypeSet>();
  if (!types) {
    return abort(AbortReason::Alloc);
  }

  types->addType(TypeSet::Int32Type(), lifoAlloc);
  types->addType(TypeSet::DoubleType(), lifoAlloc);
  types->addType(TypeSet::BigIntType(), lifoAlloc);

  if (peeked->type() == MIRType::Value && peeked->resultTypeSet() &&
      peeked->resultTypeSet()->isSubset(types)) {
    // Elide the ToNumeric because the arg is already a boxed numeric.
    peeked->setImplicitlyUsedUnchecked();
    return Ok();
  }

  // Otherwise, pop the value and add an MToNumeric.
  MDefinition* popped = current->pop();
  MToNumeric* ins = MToNumeric::New(alloc(), popped, types);
  current->add(ins);
  current->push(ins);

  // toValue() is effectful, so add a resume point.
  return resumeAfter(ins);
}

MDefinition* IonBuilder::unaryArithConvertToBinary(JSOp op,
                                                   MDefinition::Opcode* defOp) {
  switch (op) {
    case JSOp::Inc: {
      *defOp = MDefinition::Opcode::Add;
      MConstant* right = MConstant::New(alloc(), Int32Value(1));
      current->add(right);
      return right;
    }
    case JSOp::Dec: {
      *defOp = MDefinition::Opcode::Sub;
      MConstant* right = MConstant::New(alloc(), Int32Value(1));
      current->add(right);
      return right;
    }
    default:
      MOZ_CRASH("unexpected unary opcode");
  }
}

AbortReasonOr<Ok> IonBuilder::unaryArithTrySpecialized(bool* emitted, JSOp op,
                                                       MDefinition* value) {
  MOZ_ASSERT(*emitted == false);

  // Try to convert Inc(x) or Dec(x) to Add(x,1) or Sub(x,1) if the operand is a
  // number.

  if (!IsNumberType(value->type())) {
    return Ok();
  }

  MDefinition::Opcode defOp;
  MDefinition* rhs = unaryArithConvertToBinary(op, &defOp);
  MIRType specialization = binaryArithNumberSpecialization(value, rhs);
  MOZ_TRY(binaryArithEmitSpecialized(defOp, specialization, value, rhs));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::unaryArithTrySpecializedOnBaselineInspector(
    bool* emitted, JSOp op, MDefinition* value) {
  MOZ_ASSERT(*emitted == false);

  // Try to emit a specialized binary instruction speculating the
  // type using the baseline caches.

  MIRType specialization = inspector->expectedBinaryArithSpecialization(pc);
  if (specialization == MIRType::None) {
    return Ok();
  }

  MDefinition::Opcode defOp;
  MDefinition* rhs = unaryArithConvertToBinary(op, &defOp);
  MOZ_TRY(binaryArithEmitSpecialized(defOp, specialization, value, rhs));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_inc_or_dec(JSOp op) {
  bool emitted = false;
  MDefinition* value = current->pop();

  MOZ_TRY(unaryArithTrySpecialized(&emitted, op, value));
  if (emitted) {
    return Ok();
  }

  MOZ_TRY(unaryArithTrySpecializedOnBaselineInspector(&emitted, op, value));
  if (emitted) {
    return Ok();
  }

  MInstruction* stub = MUnaryCache::New(alloc(), value);
  current->add(stub);
  current->push(stub);

  // Decrease type from 'any type' to 'empty type' when one of the operands
  // is 'empty typed'.
  maybeMarkEmpty(stub);

  return resumeAfter(stub);
}

AbortReasonOr<Ok> IonBuilder::jsop_tostring() {
  if (current->peek(-1)->type() == MIRType::String) {
    return Ok();
  }

  MDefinition* value = current->pop();
  MToString* ins =
      MToString::New(alloc(), value, MToString::SideEffectHandling::Supported);
  current->add(ins);
  current->push(ins);
  if (ins->isEffectful()) {
    MOZ_TRY(resumeAfter(ins));
  }
  return Ok();
}

class AutoAccumulateReturns {
  MIRGraph& graph_;
  MIRGraphReturns* prev_;

 public:
  AutoAccumulateReturns(MIRGraph& graph, MIRGraphReturns& returns)
      : graph_(graph) {
    prev_ = graph_.returnAccumulator();
    graph_.setReturnAccumulator(&returns);
  }
  ~AutoAccumulateReturns() { graph_.setReturnAccumulator(prev_); }
};

IonBuilder::InliningResult IonBuilder::inlineScriptedCall(CallInfo& callInfo,
                                                          JSFunction* target) {
  MOZ_ASSERT(target->hasBytecode());
  MOZ_ASSERT(IsIonInlinableOp(JSOp(*pc)));

  MBasicBlock::BackupPoint backup(current);
  if (!backup.init(alloc())) {
    return abort(AbortReason::Alloc);
  }

  callInfo.setImplicitlyUsedUnchecked();

  // Create new |this| on the caller-side for inlined constructors.
  if (callInfo.constructing()) {
    MDefinition* thisDefn =
        createThis(target, callInfo.fun(), callInfo.getNewTarget(),
                   /* inlining = */ true);
    callInfo.setThis(thisDefn);
  }

  // Capture formals in the outer resume point.
  MOZ_TRY(callInfo.pushCallStack(&mirGen_, current));

  MResumePoint* outerResumePoint =
      MResumePoint::New(alloc(), current, pc, MResumePoint::Outer);
  if (!outerResumePoint) {
    return abort(AbortReason::Alloc);
  }
  current->setOuterResumePoint(outerResumePoint);

  // Pop formals again, except leave |fun| on stack for duration of call.
  callInfo.popCallStack(current);
  current->push(callInfo.fun());

  JSScript* calleeScript = target->nonLazyScript();
  BaselineInspector inspector(calleeScript);

  // Improve type information of |this| when not set.
  if (callInfo.constructing() && !callInfo.thisArg()->resultTypeSet()) {
    AutoSweepJitScript sweep(calleeScript);
    StackTypeSet* types =
        calleeScript->jitScript()->thisTypes(sweep, calleeScript);
    if (!types->unknown()) {
      TemporaryTypeSet* clonedTypes = types->clone(alloc_->lifoAlloc());
      if (!clonedTypes) {
        return abort(AbortReason::Alloc);
      }
      MTypeBarrier* barrier =
          MTypeBarrier::New(alloc(), callInfo.thisArg(), clonedTypes);
      current->add(barrier);
      if (barrier->type() == MIRType::Undefined) {
        callInfo.setThis(constant(UndefinedValue()));
      } else if (barrier->type() == MIRType::Null) {
        callInfo.setThis(constant(NullValue()));
      } else {
        callInfo.setThis(barrier);
      }
    }
  }

  // Start inlining.
  LifoAlloc* lifoAlloc = alloc_->lifoAlloc();
  InlineScriptTree* inlineScriptTree =
      info().inlineScriptTree()->addCallee(alloc_, pc, calleeScript);
  if (!inlineScriptTree) {
    return abort(AbortReason::Alloc);
  }
  CompileInfo* info = lifoAlloc->new_<CompileInfo>(
      mirGen_.runtime, calleeScript, target, (jsbytecode*)nullptr,
      this->info().analysisMode(),
      /* needsArgsObj = */ false, inlineScriptTree);
  if (!info) {
    return abort(AbortReason::Alloc);
  }

  MIRGraphReturns returns(alloc());
  AutoAccumulateReturns aar(graph(), returns);

  // Build the graph.
  IonBuilder inlineBuilder(analysisContext, mirGen_, info, constraints(),
                           &inspector, nullptr, inliningDepth_ + 1, loopDepth_);
  AbortReasonOr<Ok> result =
      inlineBuilder.buildInline(this, outerResumePoint, callInfo);
  if (result.isErr()) {
    if (analysisContext && analysisContext->isExceptionPending()) {
      JitSpew(JitSpew_IonAbort, "Inline builder raised exception.");
      MOZ_ASSERT(result.unwrapErr() == AbortReason::Error);
      return Err(result.unwrapErr());
    }

    // Inlining the callee failed. Mark the callee as uninlineable only if
    // the inlining was aborted for a non-exception reason.
    switch (result.unwrapErr()) {
      case AbortReason::Disable:
        calleeScript->setUninlineable();
        if (!JitOptions.disableInlineBacktracking) {
          MBasicBlock* block = backup.restore();
          if (!block) {
            return abort(AbortReason::Alloc);
          }
          setCurrent(block);
          return InliningStatus_NotInlined;
        }
        return abort(AbortReason::Inlining);

      case AbortReason::PreliminaryObjects: {
        const ObjectGroupVector& groups =
            inlineBuilder.abortedPreliminaryGroups();
        MOZ_ASSERT(!groups.empty());
        for (size_t i = 0; i < groups.length(); i++) {
          addAbortedPreliminaryGroup(groups[i]);
        }
        return Err(result.unwrapErr());
      }

      case AbortReason::Alloc:
      case AbortReason::Inlining:
      case AbortReason::Error:
        return Err(result.unwrapErr());

      case AbortReason::NoAbort:
        MOZ_CRASH("Abort with AbortReason::NoAbort");
        return abort(AbortReason::Error);
    }
  }

  if (returns.empty()) {
    // Inlining of functions that have no exit is not supported.
    calleeScript->setUninlineable();
    if (!JitOptions.disableInlineBacktracking) {
      MBasicBlock* block = backup.restore();
      if (!block) {
        return abort(AbortReason::Alloc);
      }
      setCurrent(block);
      return InliningStatus_NotInlined;
    }
    return abort(AbortReason::Inlining);
  }

  // Create return block.
  jsbytecode* postCall = GetNextPc(pc);
  MBasicBlock* returnBlock;
  MOZ_TRY_VAR(returnBlock, newBlock(current->stackDepth(), postCall));
  graph().addBlock(returnBlock);
  returnBlock->setCallerResumePoint(callerResumePoint_);

  // Inherit the slots from current and pop |fun|.
  returnBlock->inheritSlots(current);
  returnBlock->pop();

  // Accumulate return values.
  MDefinition* retvalDefn =
      patchInlinedReturns(target, callInfo, returns, returnBlock);
  if (!retvalDefn) {
    return abort(AbortReason::Alloc);
  }
  returnBlock->push(retvalDefn);

  // Initialize entry slots now that the stack has been fixed up.
  if (!returnBlock->initEntrySlots(alloc())) {
    return abort(AbortReason::Alloc);
  }

  MOZ_TRY(setCurrentAndSpecializePhis(returnBlock));

  return InliningStatus_Inlined;
}

MDefinition* IonBuilder::patchInlinedReturn(JSFunction* target,
                                            CallInfo& callInfo,
                                            MBasicBlock* exit,
                                            MBasicBlock* bottom) {
  // Replaces the MReturn in the exit block with an MGoto.
  MDefinition* rdef = exit->lastIns()->toReturn()->input();
  exit->discardLastIns();

  // Constructors must be patched by the caller to always return an object.
  if (callInfo.constructing()) {
    if (target->isDerivedClassConstructor()) {
      // Derived class constructors contain extra bytecode to ensure an object
      // is always returned, so no additional patching is needed.
    } else if (rdef->type() == MIRType::Value) {
      // Unknown return: dynamically detect objects.
      MReturnFromCtor* filter =
          MReturnFromCtor::New(alloc(), rdef, callInfo.thisArg());
      exit->add(filter);
      rdef = filter;
    } else if (rdef->type() != MIRType::Object) {
      // Known non-object return: force |this|.
      rdef = callInfo.thisArg();
    }
  } else if (callInfo.isSetter()) {
    // Setters return their argument, not whatever value is returned.
    rdef = callInfo.getArg(0);
  }

  if (!callInfo.isSetter()) {
    rdef = specializeInlinedReturn(rdef, exit);
  }

  MGoto* replacement = MGoto::New(alloc(), bottom);
  exit->end(replacement);
  if (!bottom->addPredecessorWithoutPhis(exit)) {
    return nullptr;
  }

  return rdef;
}

MDefinition* IonBuilder::specializeInlinedReturn(MDefinition* rdef,
                                                 MBasicBlock* exit) {
  // Remove types from the return definition that weren't observed.
  TemporaryTypeSet* types = bytecodeTypes(pc);

  // The observed typeset doesn't contain extra information.
  if (types->empty() || types->unknown()) {
    return rdef;
  }

  // Decide if specializing is needed using the result typeset if available,
  // else use the result type.

  if (rdef->resultTypeSet()) {
    // Don't specialize if return typeset is a subset of the
    // observed typeset. The return typeset is already more specific.
    if (rdef->resultTypeSet()->isSubset(types)) {
      return rdef;
    }
  } else {
    MIRType observedType = types->getKnownMIRType();

    // Don't specialize if type is MIRType::Float32 and TI reports
    // MIRType::Double. Float is more specific than double.
    if (observedType == MIRType::Double && rdef->type() == MIRType::Float32) {
      return rdef;
    }

    // Don't specialize if types are inaccordance, except for MIRType::Value
    // and MIRType::Object (when not unknown object), since the typeset
    // contains more specific information.
    if (observedType == rdef->type() && observedType != MIRType::Value &&
        (observedType != MIRType::Object || types->unknownObject())) {
      return rdef;
    }
  }

  setCurrent(exit);

  MTypeBarrier* barrier = nullptr;
  rdef = addTypeBarrier(rdef, types, BarrierKind::TypeSet, &barrier);
  if (barrier) {
    barrier->setNotMovable();
  }

  return rdef;
}

MDefinition* IonBuilder::patchInlinedReturns(JSFunction* target,
                                             CallInfo& callInfo,
                                             MIRGraphReturns& returns,
                                             MBasicBlock* bottom) {
  // Replaces MReturns with MGotos, returning the MDefinition
  // representing the return value, or nullptr.
  MOZ_ASSERT(returns.length() > 0);

  if (returns.length() == 1) {
    return patchInlinedReturn(target, callInfo, returns[0], bottom);
  }

  // Accumulate multiple returns with a phi.
  MPhi* phi = MPhi::New(alloc());
  if (!phi->reserveLength(returns.length())) {
    return nullptr;
  }

  for (size_t i = 0; i < returns.length(); i++) {
    MDefinition* rdef =
        patchInlinedReturn(target, callInfo, returns[i], bottom);
    if (!rdef) {
      return nullptr;
    }
    phi->addInput(rdef);
  }

  bottom->addPhi(phi);
  return phi;
}

IonBuilder::InliningDecision IonBuilder::makeInliningDecision(
    JSObject* targetArg, CallInfo& callInfo) {
  // When there is no target, inlining is impossible.
  if (targetArg == nullptr) {
    return InliningDecision_DontInline;
  }

  // Inlining non-function targets is handled by inlineNonFunctionCall().
  if (!targetArg->is<JSFunction>()) {
    return InliningDecision_Inline;
  }

  JSFunction* target = &targetArg->as<JSFunction>();

  // Never inline during the arguments usage analysis.
  if (info().analysisMode() == Analysis_ArgumentsUsage) {
    return InliningDecision_DontInline;
  }

  // Native functions provide their own detection in inlineNativeCall().
  if (target->isNative()) {
    return InliningDecision_Inline;
  }

  // Determine whether inlining is possible at callee site
  InliningDecision decision = canInlineTarget(target, callInfo);
  if (decision != InliningDecision_Inline) {
    return decision;
  }

  // Heuristics!
  JSScript* targetScript = target->nonLazyScript();

  // Callee must not be excessively large.
  // This heuristic also applies to the callsite as a whole.
  bool offThread = mirGen_.options.offThreadCompilationAvailable();
  if (targetScript->length() >
      optimizationInfo().inlineMaxBytecodePerCallSite(offThread)) {
    return DontInline(targetScript, "Vetoed: callee excessively large");
  }

  // Callee must have been called a few times to have somewhat stable
  // type information, except for definite properties analysis,
  // as the caller has not run yet.
  if (targetScript->getWarmUpCount() <
          optimizationInfo().inliningWarmUpThreshold() &&
      !targetScript->jitScript()->ionCompiledOrInlined() &&
      info().analysisMode() != Analysis_DefiniteProperties) {
    JitSpew(JitSpew_Inlining,
            "Cannot inline %s:%u:%u: callee is insufficiently hot.",
            targetScript->filename(), targetScript->lineno(),
            targetScript->column());
    return InliningDecision_WarmUpCountTooLow;
  }

  // Don't inline if the callee is known to inline a lot of code, to avoid
  // huge MIR graphs.
  uint32_t inlinedBytecodeLength =
      targetScript->jitScript()->inlinedBytecodeLength();
  if (inlinedBytecodeLength >
      optimizationInfo().inlineMaxCalleeInlinedBytecodeLength()) {
    return DontInline(targetScript,
                      "Vetoed: callee inlinedBytecodeLength is too big");
  }

  IonBuilder* outerBuilder = outermostBuilder();

  // Cap the total bytecode length we inline under a single script, to avoid
  // excessive inlining in pathological cases.
  size_t totalBytecodeLength =
      outerBuilder->inlinedBytecodeLength_ + targetScript->length();
  if (totalBytecodeLength > optimizationInfo().inlineMaxTotalBytecodeLength()) {
    return DontInline(targetScript,
                      "Vetoed: exceeding max total bytecode length");
  }

  // Cap the inlining depth.

  uint32_t maxInlineDepth;
  if (JitOptions.isSmallFunction(targetScript)) {
    maxInlineDepth = optimizationInfo().smallFunctionMaxInlineDepth();
  } else {
    maxInlineDepth = optimizationInfo().maxInlineDepth();

    // Caller must not be excessively large.
    if (script()->length() >=
        optimizationInfo().inliningMaxCallerBytecodeLength()) {
      return DontInline(targetScript, "Vetoed: caller excessively large");
    }
  }

  JitScript* outerJitScript = outermostBuilder()->script()->jitScript();
  if (inliningDepth_ >= maxInlineDepth) {
    // We hit the depth limit and won't inline this function. Give the
    // outermost script a max inlining depth of 0, so that it won't be
    // inlined in other scripts. This heuristic is currently only used
    // when we're inlining scripts with loops, see the comment below.
    // These heuristics only apply to the highest optimization level.
    if (isHighestOptimizationLevel()) {
      outerJitScript->setMaxInliningDepth(0);
    }

    return DontInline(targetScript, "Vetoed: exceeding allowed inline depth");
  }

  // Inlining functions with loops can be complicated. For instance, if we're
  // close to the inlining depth limit and we inline the function f below, we
  // can no longer inline the call to g:
  //
  //   function f() {
  //      while (cond) {
  //          g();
  //      }
  //   }
  //
  // If the loop has many iterations, it's more efficient to call f and inline
  // g in f.
  //
  // To avoid this problem, we record a separate max inlining depth for each
  // script, indicating at which depth we won't be able to inline all functions
  // we inlined this time. This solves the issue above, because we will only
  // inline f if it means we can also inline g.
  //
  // These heuristics only apply to the highest optimization level: other tiers
  // do very little inlining and performance is not as much of a concern there.
  if (isHighestOptimizationLevel() && targetScript->hasLoops() &&
      inliningDepth_ >= targetScript->jitScript()->maxInliningDepth()) {
    return DontInline(targetScript,
                      "Vetoed: exceeding allowed script inline depth");
  }

  // Update the max depth at which we can inline the outer script.
  MOZ_ASSERT(maxInlineDepth > inliningDepth_);
  uint32_t scriptInlineDepth = maxInlineDepth - inliningDepth_ - 1;
  if (scriptInlineDepth < outerJitScript->maxInliningDepth() &&
      isHighestOptimizationLevel()) {
    outerJitScript->setMaxInliningDepth(scriptInlineDepth);
  }

  // End of heuristics, we will inline this function.

  outerBuilder->inlinedBytecodeLength_ += targetScript->length();

  return InliningDecision_Inline;
}

AbortReasonOr<Ok> IonBuilder::selectInliningTargets(
    const InliningTargets& targets, CallInfo& callInfo, BoolVector& choiceSet,
    uint32_t* numInlineable) {
  *numInlineable = 0;
  uint32_t totalSize = 0;

  // For each target, ask whether it may be inlined.
  if (!choiceSet.reserve(targets.length())) {
    return abort(AbortReason::Alloc);
  }

  // Don't inline polymorphic sites during the definite properties analysis.
  // AddClearDefiniteFunctionUsesInScript depends on this for correctness.
  if (info().analysisMode() == Analysis_DefiniteProperties &&
      targets.length() > 1) {
    return Ok();
  }

  for (size_t i = 0; i < targets.length(); i++) {
    JSObject* target = targets[i].target;

    bool inlineable;
    InliningDecision decision = makeInliningDecision(target, callInfo);
    switch (decision) {
      case InliningDecision_Error:
        return abort(AbortReason::Error);
      case InliningDecision_DontInline:
      case InliningDecision_WarmUpCountTooLow:
        inlineable = false;
        break;
      case InliningDecision_Inline:
        inlineable = true;
        break;
      default:
        MOZ_CRASH("Unhandled InliningDecision value!");
    }

    if (target->is<JSFunction>()) {
      // Enforce a maximum inlined bytecode limit at the callsite.
      if (inlineable && target->as<JSFunction>().isInterpreted()) {
        totalSize += target->as<JSFunction>().nonLazyScript()->length();
        bool offThread = mirGen_.options.offThreadCompilationAvailable();
        if (totalSize >
            optimizationInfo().inlineMaxBytecodePerCallSite(offThread)) {
          inlineable = false;
        }
      }
    } else {
      // Non-function targets are not supported by polymorphic inlining.
      inlineable = false;
    }

    // Only use a group guard and inline the target if we will recompile when
    // the target function gets a new group.
    if (inlineable && targets[i].group) {
      ObjectGroup* group = targets[i].group;
      TypeSet::ObjectKey* key = TypeSet::ObjectKey::get(group);
      if (!key->hasStableClassAndProto(constraints())) {
        inlineable = false;
      }
    }

    choiceSet.infallibleAppend(inlineable);
    if (inlineable) {
      *numInlineable += 1;
    }
  }

  MOZ_ASSERT(choiceSet.length() == targets.length());
  return Ok();
}

static bool CanInlineGetPropertyCache(MGetPropertyCache* cache,
                                      MDefinition* thisDef) {
  if (cache->value()->type() != MIRType::Object) {
    return false;
  }

  if (cache->value() != thisDef) {
    return false;
  }

  InlinePropertyTable* table = cache->propTable();
  if (!table) {
    return false;
  }
  if (table->numEntries() == 0) {
    return false;
  }
  return true;
}

class WrapMGetPropertyCache {
  MGetPropertyCache* cache_;

 private:
  void discardPriorResumePoint() {
    if (!cache_) {
      return;
    }

    InlinePropertyTable* propTable = cache_->propTable();
    if (!propTable) {
      return;
    }
    MResumePoint* rp = propTable->takePriorResumePoint();
    if (!rp) {
      return;
    }
    cache_->block()->discardPreAllocatedResumePoint(rp);
  }

 public:
  explicit WrapMGetPropertyCache(MGetPropertyCache* cache) : cache_(cache) {}

  ~WrapMGetPropertyCache() { discardPriorResumePoint(); }

  MGetPropertyCache* get() { return cache_; }
  MGetPropertyCache* operator->() { return get(); }

  // This function returns the cache given to the constructor if the
  // GetPropertyCache can be moved into the ObjectGroup fallback path.
  MGetPropertyCache* moveableCache(bool hasTypeBarrier, MDefinition* thisDef) {
    // If we have unhandled uses of the MGetPropertyCache, then we cannot
    // move it to the ObjectGroup fallback path.
    if (!hasTypeBarrier) {
      if (cache_->hasUses()) {
        return nullptr;
      }
    } else {
      // There is the TypeBarrier consumer, so we check that this is the
      // only consumer.
      MOZ_ASSERT(cache_->hasUses());
      if (!cache_->hasOneUse()) {
        return nullptr;
      }
    }

    // If the this-object is not identical to the object of the
    // MGetPropertyCache, then we cannot use the InlinePropertyTable, or if
    // we do not yet have enough information from the ObjectGroup.
    if (!CanInlineGetPropertyCache(cache_, thisDef)) {
      return nullptr;
    }

    MGetPropertyCache* ret = cache_;
    cache_ = nullptr;
    return ret;
  }
};

MGetPropertyCache* IonBuilder::getInlineableGetPropertyCache(
    CallInfo& callInfo) {
  if (callInfo.constructing()) {
    return nullptr;
  }

  MDefinition* thisDef = callInfo.thisArg();
  if (thisDef->type() != MIRType::Object) {
    return nullptr;
  }

  MDefinition* funcDef = callInfo.fun();
  if (funcDef->type() != MIRType::Object) {
    return nullptr;
  }

  // MGetPropertyCache with no uses may be optimized away.
  if (funcDef->isGetPropertyCache()) {
    WrapMGetPropertyCache cache(funcDef->toGetPropertyCache());
    return cache.moveableCache(/* hasTypeBarrier = */ false, thisDef);
  }

  // Optimize away the following common pattern:
  // MTypeBarrier[MIRType::Object] <- MGetPropertyCache
  if (funcDef->isTypeBarrier()) {
    MTypeBarrier* barrier = funcDef->toTypeBarrier();
    if (barrier->hasUses()) {
      return nullptr;
    }
    if (barrier->type() != MIRType::Object) {
      return nullptr;
    }
    if (!barrier->input()->isGetPropertyCache()) {
      return nullptr;
    }

    WrapMGetPropertyCache cache(barrier->input()->toGetPropertyCache());
    return cache.moveableCache(/* hasTypeBarrier = */ true, thisDef);
  }

  return nullptr;
}

IonBuilder::InliningResult IonBuilder::inlineSingleCall(CallInfo& callInfo,
                                                        JSObject* targetArg) {
  InliningStatus status;
  if (!targetArg->is<JSFunction>()) {
    return InliningStatus_NotInlined;
  }

  JSFunction* target = &targetArg->as<JSFunction>();
  if (target->isNative()) {
    MOZ_TRY_VAR(status, inlineNativeCall(callInfo, target));
    return status;
  }

  return inlineScriptedCall(callInfo, target);
}

IonBuilder::InliningResult IonBuilder::inlineCallsite(
    const InliningTargets& targets, CallInfo& callInfo) {
  if (targets.empty()) {
    return InliningStatus_NotInlined;
  }

  // Is the function provided by an MGetPropertyCache?
  // If so, the cache may be movable to a fallback path, with a dispatch
  // instruction guarding on the incoming ObjectGroup.
  WrapMGetPropertyCache propCache(getInlineableGetPropertyCache(callInfo));
  keepFallbackFunctionGetter(propCache.get());

  // Inline single targets -- unless they derive from a cache, in which case
  // avoiding the cache and guarding is still faster.
  if (!propCache.get() && targets.length() == 1) {
    JSObject* target = targets[0].target;

    InliningDecision decision = makeInliningDecision(target, callInfo);
    switch (decision) {
      case InliningDecision_Error:
        return abort(AbortReason::Error);
      case InliningDecision_DontInline:
        return InliningStatus_NotInlined;
      case InliningDecision_WarmUpCountTooLow:
        return InliningStatus_WarmUpCountTooLow;
      case InliningDecision_Inline:
        break;
    }

    // Inlining will elminate uses of the original callee, but it needs to
    // be preserved in phis if we bail out.  Mark the old callee definition as
    // implicitly used to ensure this happens.
    callInfo.fun()->setImplicitlyUsedUnchecked();

    // If the callee is not going to be a lambda (which may vary across
    // different invocations), then the callee definition can be replaced by a
    // constant.
    if (target->isSingleton()) {
      // Replace the function with an MConstant.
      MConstant* constFun = constant(ObjectValue(*target));
      if (callInfo.constructing() &&
          callInfo.getNewTarget() == callInfo.fun()) {
        callInfo.setNewTarget(constFun);
      }
      callInfo.setFun(constFun);
    }

    return inlineSingleCall(callInfo, target);
  }

  // Choose a subset of the targets for polymorphic inlining.
  BoolVector choiceSet(alloc());
  uint32_t numInlined;
  MOZ_TRY(selectInliningTargets(targets, callInfo, choiceSet, &numInlined));
  if (numInlined == 0) {
    return InliningStatus_NotInlined;
  }

  // Perform a polymorphic dispatch.
  MOZ_TRY(inlineCalls(callInfo, targets, choiceSet, propCache.get()));

  return InliningStatus_Inlined;
}

AbortReasonOr<Ok> IonBuilder::inlineGenericFallback(
    const Maybe<CallTargets>& targets, CallInfo& callInfo,
    MBasicBlock* dispatchBlock) {
  // Generate a new block with all arguments on-stack.
  MBasicBlock* fallbackBlock;
  MOZ_TRY_VAR(fallbackBlock, newBlock(dispatchBlock, pc));
  graph().addBlock(fallbackBlock);

  // Create a new CallInfo to track modified state within this block.
  CallInfo fallbackInfo(alloc(), pc, callInfo.constructing(),
                        callInfo.ignoresReturnValue());
  if (!fallbackInfo.init(callInfo)) {
    return abort(AbortReason::Alloc);
  }
  fallbackInfo.popCallStack(fallbackBlock);

  // Generate an MCall, which uses stateful |current|.
  MOZ_TRY(setCurrentAndSpecializePhis(fallbackBlock));
  MOZ_TRY(makeCall(targets, fallbackInfo));

  // Pass return block to caller as |current|.
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::inlineObjectGroupFallback(
    const Maybe<CallTargets>& targets, CallInfo& callInfo,
    MBasicBlock* dispatchBlock, MObjectGroupDispatch* dispatch,
    MGetPropertyCache* cache, MBasicBlock** fallbackTarget) {
  // Getting here implies the following:
  // 1. The call function is an MGetPropertyCache, or an MGetPropertyCache
  //    followed by an MTypeBarrier.
  MOZ_ASSERT(callInfo.fun()->isGetPropertyCache() ||
             callInfo.fun()->isTypeBarrier());

  // 2. The MGetPropertyCache has inlineable cases by guarding on the
  // ObjectGroup.
  MOZ_ASSERT(dispatch->numCases() > 0);

  // 3. The MGetPropertyCache (and, if applicable, MTypeBarrier) only
  //    have at most a single use.
  MOZ_ASSERT_IF(callInfo.fun()->isGetPropertyCache(), !cache->hasUses());
  MOZ_ASSERT_IF(callInfo.fun()->isTypeBarrier(), cache->hasOneUse());

  // This means that no resume points yet capture the MGetPropertyCache,
  // so everything from the MGetPropertyCache up until the call is movable.
  // We now move the MGetPropertyCache and friends into a fallback path.
  MOZ_ASSERT(cache->idempotent());

  // Create a new CallInfo to track modified state within the fallback path.
  CallInfo fallbackInfo(alloc(), pc, callInfo.constructing(),
                        callInfo.ignoresReturnValue());
  if (!fallbackInfo.init(callInfo)) {
    return abort(AbortReason::Alloc);
  }

  // Capture stack prior to the call operation. This captures the function.
  MResumePoint* preCallResumePoint =
      MResumePoint::New(alloc(), dispatchBlock, pc, MResumePoint::ResumeAt);
  if (!preCallResumePoint) {
    return abort(AbortReason::Alloc);
  }

  DebugOnly<size_t> preCallFuncIndex =
      preCallResumePoint->stackDepth() - callInfo.numFormals();
  MOZ_ASSERT(preCallResumePoint->getOperand(preCallFuncIndex) ==
             fallbackInfo.fun());

  // In the dispatch block, replace the function's slot entry with Undefined.
  MConstant* undefined = MConstant::New(alloc(), UndefinedValue());
  dispatchBlock->add(undefined);
  dispatchBlock->rewriteAtDepth(-int(callInfo.numFormals()), undefined);

  // Construct a block that does nothing but remove formals from the stack.
  // This is effectively changing the entry resume point of the later fallback
  // block.
  MBasicBlock* prepBlock;
  MOZ_TRY_VAR(prepBlock, newBlock(dispatchBlock, pc));
  graph().addBlock(prepBlock);
  fallbackInfo.popCallStack(prepBlock);

  // Construct a block into which the MGetPropertyCache can be moved.
  // This is subtle: the pc and resume point are those of the MGetPropertyCache!
  InlinePropertyTable* propTable = cache->propTable();
  MResumePoint* priorResumePoint = propTable->takePriorResumePoint();
  MOZ_ASSERT(propTable->pc() != nullptr);
  MOZ_ASSERT(priorResumePoint != nullptr);
  MBasicBlock* getPropBlock;
  MOZ_TRY_VAR(getPropBlock,
              newBlock(prepBlock, propTable->pc(), priorResumePoint));
  graph().addBlock(getPropBlock);

  prepBlock->end(MGoto::New(alloc(), getPropBlock));

  // Since the getPropBlock inherited the stack from right before the
  // MGetPropertyCache, the target of the MGetPropertyCache is still on the
  // stack.
  DebugOnly<MDefinition*> checkObject = getPropBlock->pop();
  MOZ_ASSERT(checkObject == cache->value());

  // Move the MGetPropertyCache and friends into the getPropBlock.
  if (fallbackInfo.fun()->isGetPropertyCache()) {
    MOZ_ASSERT(fallbackInfo.fun()->toGetPropertyCache() == cache);
    getPropBlock->addFromElsewhere(cache);
    getPropBlock->push(cache);
  } else {
    MTypeBarrier* barrier = callInfo.fun()->toTypeBarrier();
    MOZ_ASSERT(barrier->type() == MIRType::Object);
    MOZ_ASSERT(barrier->input()->isGetPropertyCache());
    MOZ_ASSERT(barrier->input()->toGetPropertyCache() == cache);

    getPropBlock->addFromElsewhere(cache);
    getPropBlock->addFromElsewhere(barrier);
    getPropBlock->push(barrier);
  }

  // Construct an end block with the correct resume point.
  MBasicBlock* preCallBlock;
  MOZ_TRY_VAR(preCallBlock, newBlock(getPropBlock, pc, preCallResumePoint));
  graph().addBlock(preCallBlock);
  getPropBlock->end(MGoto::New(alloc(), preCallBlock));

  // Now inline the MCallGeneric, using preCallBlock as the dispatch point.
  MOZ_TRY(inlineGenericFallback(targets, fallbackInfo, preCallBlock));

  // inlineGenericFallback() set the return block as |current|.
  preCallBlock->end(MGoto::New(alloc(), current));
  *fallbackTarget = prepBlock;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::inlineCalls(CallInfo& callInfo,
                                          const InliningTargets& targets,
                                          BoolVector& choiceSet,
                                          MGetPropertyCache* maybeCache) {
  // Only handle polymorphic inlining.
  MOZ_ASSERT(IsIonInlinableOp(JSOp(*pc)));
  MOZ_ASSERT(choiceSet.length() == targets.length());
  MOZ_ASSERT_IF(!maybeCache, targets.length() >= 2);
  MOZ_ASSERT_IF(maybeCache, targets.length() >= 1);
  MOZ_ASSERT_IF(maybeCache, maybeCache->value()->type() == MIRType::Object);

  MBasicBlock* dispatchBlock = current;
  callInfo.setImplicitlyUsedUnchecked();
  MOZ_TRY(callInfo.pushCallStack(&mirGen_, dispatchBlock));

  // Patch any InlinePropertyTable to only contain functions that are
  // inlineable. The InlinePropertyTable will also be patched at the end to
  // exclude native functions that vetoed inlining.
  if (maybeCache) {
    InlinePropertyTable* propTable = maybeCache->propTable();
    propTable->trimToTargets(targets);
    if (propTable->numEntries() == 0) {
      maybeCache = nullptr;
    }
  }

  // Generate a dispatch based on guard kind.
  MDispatchInstruction* dispatch;
  if (maybeCache) {
    dispatch = MObjectGroupDispatch::New(alloc(), maybeCache->value(),
                                         maybeCache->propTable());
    callInfo.fun()->setImplicitlyUsedUnchecked();
  } else {
    dispatch = MFunctionDispatch::New(alloc(), callInfo.fun());
  }

  MOZ_ASSERT(dispatchBlock->stackDepth() >= callInfo.numFormals());
  uint32_t stackDepth = dispatchBlock->stackDepth() - callInfo.numFormals() + 1;

  // Generate a return block to host the rval-collecting MPhi.
  jsbytecode* postCall = GetNextPc(pc);
  MBasicBlock* returnBlock;
  MOZ_TRY_VAR(returnBlock, newBlock(stackDepth, postCall));
  graph().addBlock(returnBlock);
  returnBlock->setCallerResumePoint(callerResumePoint_);

  // Set up stack, used to manually create a post-call resume point.
  returnBlock->inheritSlots(dispatchBlock);
  callInfo.popCallStack(returnBlock);

  MPhi* retPhi = MPhi::New(alloc());
  returnBlock->addPhi(retPhi);
  returnBlock->push(retPhi);

  // Create a resume point from current stack state.
  if (!returnBlock->initEntrySlots(alloc())) {
    return abort(AbortReason::Alloc);
  }

  // Reserve the capacity for the phi.
  // Note: this is an upperbound. Unreachable targets and uninlineable natives
  // are also counted.
  uint32_t count = 1;  // Possible fallback block.
  for (uint32_t i = 0; i < targets.length(); i++) {
    if (choiceSet[i]) {
      count++;
    }
  }
  if (!retPhi->reserveLength(count)) {
    return abort(AbortReason::Alloc);
  }

  // Inline each of the inlineable targets.
  for (uint32_t i = 0; i < targets.length(); i++) {
    // Target must be inlineable.
    if (!choiceSet[i]) {
      continue;
    }

    // Target must be reachable by the MDispatchInstruction.
    JSFunction* target = &targets[i].target->as<JSFunction>();
    if (maybeCache && !maybeCache->propTable()->hasFunction(target)) {
      choiceSet[i] = false;
      continue;
    }

    MBasicBlock* inlineBlock;
    MOZ_TRY_VAR(inlineBlock, newBlock(dispatchBlock, pc));
    graph().addBlock(inlineBlock);

    // Create a function MConstant to use in the entry ResumePoint. If we
    // can't use a constant, add a no-op MPolyInlineGuard, to prevent
    // hoisting env chain gets above the dispatch instruction.
    MInstruction* funcDef;
    if (target->isSingleton()) {
      funcDef = MConstant::New(alloc(), ObjectValue(*target), constraints());
    } else {
      funcDef = MPolyInlineGuard::New(alloc(), callInfo.fun());
    }

    funcDef->setImplicitlyUsedUnchecked();
    dispatchBlock->add(funcDef);

    // Use the inlined callee in the inline resume point and on stack.
    int funIndex =
        inlineBlock->entryResumePoint()->stackDepth() - callInfo.numFormals();
    inlineBlock->entryResumePoint()->replaceOperand(funIndex, funcDef);
    inlineBlock->rewriteSlot(funIndex, funcDef);

    // Create a new CallInfo to track modified state within the inline block.
    CallInfo inlineInfo(alloc(), pc, callInfo.constructing(),
                        callInfo.ignoresReturnValue());
    if (!inlineInfo.init(callInfo)) {
      return abort(AbortReason::Alloc);
    }
    inlineInfo.popCallStack(inlineBlock);
    inlineInfo.setFun(funcDef);

    if (callInfo.constructing() && callInfo.getNewTarget() == callInfo.fun()) {
      inlineInfo.setNewTarget(funcDef);
    }

    if (maybeCache) {
      // Assign the 'this' value a TypeSet specialized to the groups that
      // can generate this inlining target.
      MOZ_ASSERT(callInfo.thisArg() == maybeCache->value());
      TemporaryTypeSet* thisTypes =
          maybeCache->propTable()->buildTypeSetForFunction(alloc(), target);
      if (!thisTypes) {
        return abort(AbortReason::Alloc);
      }

      MFilterTypeSet* filter =
          MFilterTypeSet::New(alloc(), inlineInfo.thisArg(), thisTypes);
      inlineBlock->add(filter);
      inlineInfo.setThis(filter);
    }

    // Inline the call into the inlineBlock.
    MOZ_TRY(setCurrentAndSpecializePhis(inlineBlock));
    InliningStatus status;
    MOZ_TRY_VAR(status, inlineSingleCall(inlineInfo, target));

    // Natives may veto inlining.
    if (status == InliningStatus_NotInlined) {
      MOZ_ASSERT(current == inlineBlock);
      graph().removeBlock(inlineBlock);
      choiceSet[i] = false;
      continue;
    }

    // inlineSingleCall() changed |current| to the inline return block.
    MBasicBlock* inlineReturnBlock = current;
    setCurrent(dispatchBlock);

    // Connect the inline path to the returnBlock.
    if (!dispatch->addCase(target, targets[i].group, inlineBlock)) {
      return abort(AbortReason::Alloc);
    }

    MDefinition* retVal = inlineReturnBlock->peek(-1);
    retPhi->addInput(retVal);
    inlineReturnBlock->end(MGoto::New(alloc(), returnBlock));
    if (!returnBlock->addPredecessorWithoutPhis(inlineReturnBlock)) {
      return abort(AbortReason::Alloc);
    }
  }

  // Patch the InlinePropertyTable to not dispatch to vetoed paths.
  bool useFallback;
  if (maybeCache) {
    InlinePropertyTable* propTable = maybeCache->propTable();
    propTable->trimTo(targets, choiceSet);

    if (propTable->numEntries() == 0 || !propTable->hasPriorResumePoint()) {
      // Output a generic fallback path.
      MOZ_ASSERT_IF(propTable->numEntries() == 0, dispatch->numCases() == 0);
      maybeCache = nullptr;
      useFallback = true;
    } else {
      // We need a fallback path if the ObjectGroup dispatch does not
      // handle all incoming objects.
      useFallback = false;
      TemporaryTypeSet* objectTypes = maybeCache->value()->resultTypeSet();
      for (uint32_t i = 0; i < objectTypes->getObjectCount(); i++) {
        TypeSet::ObjectKey* obj = objectTypes->getObject(i);
        if (!obj) {
          continue;
        }

        if (!obj->isGroup()) {
          useFallback = true;
          break;
        }

        if (!propTable->hasObjectGroup(obj->group())) {
          useFallback = true;
          break;
        }
      }

      if (!useFallback) {
        // The object group dispatch handles all possible incoming
        // objects, so the cache and barrier will not be reached and
        // can be eliminated.
        if (callInfo.fun()->isGetPropertyCache()) {
          MOZ_ASSERT(callInfo.fun() == maybeCache);
        } else {
          MTypeBarrier* barrier = callInfo.fun()->toTypeBarrier();
          MOZ_ASSERT(!barrier->hasUses());
          MOZ_ASSERT(barrier->type() == MIRType::Object);
          MOZ_ASSERT(barrier->input()->isGetPropertyCache());
          MOZ_ASSERT(barrier->input()->toGetPropertyCache() == maybeCache);
          barrier->block()->discard(barrier);
        }

        MOZ_ASSERT(!maybeCache->hasUses());
        maybeCache->block()->discard(maybeCache);
      }
    }
  } else {
    useFallback = dispatch->numCases() < targets.length();
  }

  // If necessary, generate a fallback path.
  if (useFallback) {
    // Annotate the fallback call with the target information.
    Maybe<CallTargets> remainingTargets;
    remainingTargets.emplace(alloc());
    for (uint32_t i = 0; i < targets.length(); i++) {
      if (!maybeCache && choiceSet[i]) {
        continue;
      }

      JSObject* target = targets[i].target;
      if (!target->is<JSFunction>()) {
        remainingTargets = Nothing();
        break;
      }
      if (!remainingTargets->append(&target->as<JSFunction>())) {
        return abort(AbortReason::Alloc);
      }
    }

    // Generate fallback blocks, and set |current| to the fallback return block.
    if (maybeCache) {
      MBasicBlock* fallbackTarget;
      MOZ_TRY(inlineObjectGroupFallback(
          remainingTargets, callInfo, dispatchBlock,
          dispatch->toObjectGroupDispatch(), maybeCache, &fallbackTarget));
      dispatch->addFallback(fallbackTarget);
    } else {
      MOZ_TRY(inlineGenericFallback(remainingTargets, callInfo, dispatchBlock));
      dispatch->addFallback(current);
    }

    MBasicBlock* fallbackReturnBlock = current;

    // Connect fallback case to return infrastructure.
    MDefinition* retVal = fallbackReturnBlock->peek(-1);
    retPhi->addInput(retVal);
    fallbackReturnBlock->end(MGoto::New(alloc(), returnBlock));
    if (!returnBlock->addPredecessorWithoutPhis(fallbackReturnBlock)) {
      return abort(AbortReason::Alloc);
    }
  }

  // Finally add the dispatch instruction.
  // This must be done at the end so that add() may be called above.
  dispatchBlock->end(dispatch);

  // Check the depth change: +1 for retval
  MOZ_ASSERT(returnBlock->stackDepth() ==
             dispatchBlock->stackDepth() - callInfo.numFormals() + 1);

  graph().moveBlockToEnd(returnBlock);
  return setCurrentAndSpecializePhis(returnBlock);
}

MInstruction* IonBuilder::createNamedLambdaObject(MDefinition* callee,
                                                  MDefinition* env) {
  // Get a template CallObject that we'll use to generate inline object
  // creation.
  LexicalEnvironmentObject* templateObj =
      inspector->templateNamedLambdaObject();

  // One field is added to the function to handle its name.  This cannot be a
  // dynamic slot because there is still plenty of room on the NamedLambda
  // object.
  MOZ_ASSERT(!templateObj->hasDynamicSlots());

  // Allocate the actual object. It is important that no intervening
  // instructions could potentially bailout, thus leaking the dynamic slots
  // pointer.
  MInstruction* declEnvObj = MNewNamedLambdaObject::New(alloc(), templateObj);
  current->add(declEnvObj);

  // Initialize the object's reserved slots. No post barrier is needed here:
  // the object will be allocated in the nursery if possible, and if the
  // tenured heap is used instead, a minor collection will have been performed
  // that moved env/callee to the tenured heap.
  current->add(MStoreFixedSlot::New(
      alloc(), declEnvObj, NamedLambdaObject::enclosingEnvironmentSlot(), env));
  current->add(MStoreFixedSlot::New(alloc(), declEnvObj,
                                    NamedLambdaObject::lambdaSlot(), callee));

  return declEnvObj;
}

AbortReasonOr<MInstruction*> IonBuilder::createCallObject(MDefinition* callee,
                                                          MDefinition* env) {
  // Get a template CallObject that we'll use to generate inline object
  // creation.
  CallObject* templateObj = inspector->templateCallObject();
  MConstant* templateCst =
      MConstant::NewConstraintlessObject(alloc(), templateObj);
  current->add(templateCst);

  // Allocate the object.
  MNewCallObject* callObj = MNewCallObject::New(alloc(), templateCst);
  current->add(callObj);

  // Initialize the object's reserved slots. No post barrier is needed here,
  // for the same reason as in createNamedLambdaObject.
  current->add(MStoreFixedSlot::New(
      alloc(), callObj, CallObject::enclosingEnvironmentSlot(), env));
  current->add(
      MStoreFixedSlot::New(alloc(), callObj, CallObject::calleeSlot(), callee));

  // Copy closed-over argument slots if there aren't parameter expressions.
  MSlots* slots = nullptr;
  for (PositionalFormalParameterIter fi(script()); fi; fi++) {
    if (!fi.closedOver()) {
      continue;
    }

    if (!alloc().ensureBallast()) {
      return abort(AbortReason::Alloc);
    }

    unsigned slot = fi.location().slot();
    unsigned formal = fi.argumentSlot();
    unsigned numFixedSlots = templateObj->numFixedSlots();
    MDefinition* param;
    if (script()->functionHasParameterExprs()) {
      param = constant(MagicValue(JS_UNINITIALIZED_LEXICAL));
    } else {
      param = current->getSlot(info().argSlotUnchecked(formal));
    }
    if (slot >= numFixedSlots) {
      if (!slots) {
        slots = MSlots::New(alloc(), callObj);
        current->add(slots);
      }
      current->add(
          MStoreSlot::New(alloc(), slots, slot - numFixedSlots, param));
    } else {
      current->add(MStoreFixedSlot::New(alloc(), callObj, slot, param));
    }
  }

  return AbortReasonOr<MInstruction*>(callObj);
}

MDefinition* IonBuilder::createThisScripted(MDefinition* callee,
                                            MDefinition* newTarget) {
  // Get callee.prototype.
  //
  // This instruction MUST be idempotent: since it does not correspond to an
  // explicit operation in the bytecode, we cannot use resumeAfter().
  // Getters may not override |prototype| fetching, so this operation is
  // indeed idempotent.
  // - First try an idempotent property cache.
  // - Upon failing idempotent property cache, we can't use a non-idempotent
  //   cache, therefore we fallback to CallGetProperty
  //
  // Note: both CallGetProperty and GetPropertyCache can trigger a GC,
  //       and thus invalidation.
  MInstruction* getProto;
  if (!invalidatedIdempotentCache()) {
    MConstant* id = constant(StringValue(names().prototype));
    MGetPropertyCache* getPropCache =
        MGetPropertyCache::New(alloc(), newTarget, id,
                               /* monitored = */ false);
    getPropCache->setIdempotent();
    getProto = getPropCache;
  } else {
    MCallGetProperty* callGetProp =
        MCallGetProperty::New(alloc(), newTarget, names().prototype);
    callGetProp->setIdempotent();
    getProto = callGetProp;
  }
  current->add(getProto);

  // Create this from prototype
  MCreateThisWithProto* createThis =
      MCreateThisWithProto::New(alloc(), callee, newTarget, getProto);
  current->add(createThis);

  return createThis;
}

JSObject* IonBuilder::getSingletonPrototype(JSFunction* target) {
  TypeSet::ObjectKey* targetKey = TypeSet::ObjectKey::get(target);
  if (targetKey->unknownProperties()) {
    return nullptr;
  }

  jsid protoid = NameToId(names().prototype);
  HeapTypeSetKey protoProperty = targetKey->property(protoid);

  return protoProperty.singleton(constraints());
}

MDefinition* IonBuilder::createThisScriptedSingleton(JSFunction* target) {
  if (!target->hasBytecode()) {
    return nullptr;
  }

  // Get the singleton prototype (if exists)
  JSObject* proto = getSingletonPrototype(target);
  if (!proto) {
    return nullptr;
  }

  JSObject* templateObject = inspector->getTemplateObject(pc);
  if (!templateObject) {
    return nullptr;
  }
  if (!templateObject->is<PlainObject>()) {
    return nullptr;
  }
  if (templateObject->staticPrototype() != proto) {
    return nullptr;
  }
  if (templateObject->nonCCWRealm() != target->realm()) {
    return nullptr;
  }

  TypeSet::ObjectKey* templateObjectKey =
      TypeSet::ObjectKey::get(templateObject->group());
  if (templateObjectKey->hasFlags(constraints(),
                                  OBJECT_FLAG_NEW_SCRIPT_CLEARED)) {
    return nullptr;
  }

  JSScript* targetScript = target->nonLazyScript();
  JitScript* jitScript = targetScript->maybeJitScript();
  if (!jitScript) {
    return nullptr;
  }

  AutoSweepJitScript sweep(targetScript);
  StackTypeSet* thisTypes = jitScript->thisTypes(sweep, targetScript);
  if (!thisTypes->hasType(TypeSet::ObjectType(templateObject))) {
    return nullptr;
  }

  // Generate an inline path to create a new |this| object with
  // the given singleton prototype.
  MConstant* templateConst =
      MConstant::NewConstraintlessObject(alloc(), templateObject);
  MCreateThisWithTemplate* createThis = MCreateThisWithTemplate::New(
      alloc(), constraints(), templateConst,
      templateObject->group()->initialHeap(constraints()));
  current->add(templateConst);
  current->add(createThis);

  return createThis;
}

MDefinition* IonBuilder::createThisScriptedBaseline(MDefinition* callee) {
  // Try to inline |this| creation based on Baseline feedback.

  JSFunction* target = inspector->getSingleCallee(pc);
  if (!target || !target->hasBytecode()) {
    return nullptr;
  }

  if (target->constructorNeedsUninitializedThis()) {
    return nullptr;
  }

  JSObject* templateObject = inspector->getTemplateObject(pc);
  if (!templateObject) {
    return nullptr;
  }
  if (!templateObject->is<PlainObject>()) {
    return nullptr;
  }
  if (templateObject->nonCCWRealm() != target->realm()) {
    return nullptr;
  }

  Shape* shape = target->lookupPure(realm->runtime()->names().prototype);
  if (!shape || !shape->isDataProperty()) {
    return nullptr;
  }

  Value protov = target->getSlot(shape->slot());
  if (!protov.isObject()) {
    return nullptr;
  }

  JSObject* proto = checkNurseryObject(&protov.toObject());
  if (proto != templateObject->staticPrototype()) {
    return nullptr;
  }

  TypeSet::ObjectKey* templateObjectKey =
      TypeSet::ObjectKey::get(templateObject->group());
  if (templateObjectKey->hasFlags(constraints(),
                                  OBJECT_FLAG_NEW_SCRIPT_CLEARED)) {
    return nullptr;
  }

  JSScript* targetScript = target->nonLazyScript();
  JitScript* jitScript = targetScript->maybeJitScript();
  if (!jitScript) {
    return nullptr;
  }

  AutoSweepJitScript sweep(targetScript);
  StackTypeSet* thisTypes = jitScript->thisTypes(sweep, targetScript);
  if (!thisTypes->hasType(TypeSet::ObjectType(templateObject))) {
    return nullptr;
  }

  // Shape guard.
  callee = addShapeGuard(callee, target->lastProperty(), Bailout_ShapeGuard);

  // Guard callee.prototype == proto.
  MOZ_ASSERT(shape->numFixedSlots() == 0, "Must be a dynamic slot");
  MSlots* slots = MSlots::New(alloc(), callee);
  current->add(slots);
  MLoadSlot* prototype = MLoadSlot::New(alloc(), slots, shape->slot());
  current->add(prototype);
  MDefinition* protoConst = constant(ObjectValue(*proto));
  MGuardObjectIdentity* guard =
      MGuardObjectIdentity::New(alloc(), prototype, protoConst,
                                /* bailOnEquality = */ false);
  current->add(guard);

  // Generate an inline path to create a new |this| object with
  // the given prototype.
  MConstant* templateConst =
      MConstant::NewConstraintlessObject(alloc(), templateObject);
  MCreateThisWithTemplate* createThis = MCreateThisWithTemplate::New(
      alloc(), constraints(), templateConst,
      templateObject->group()->initialHeap(constraints()));
  current->add(templateConst);
  current->add(createThis);

  return createThis;
}

MDefinition* IonBuilder::createThisSlow(MDefinition* callee,
                                        MDefinition* newTarget, bool inlining) {
  // Call jit::CreateThisFromIon. This may return a NullValue for |this| that
  // LCallGeneric has to check for if we can't create |this| inline.
  MOZ_ASSERT(!inlining);
  MCreateThis* createThis = MCreateThis::New(alloc(), callee, newTarget);
  current->add(createThis);
  return createThis;
}

MDefinition* IonBuilder::createThis(JSFunction* target, MDefinition* callee,
                                    MDefinition* newTarget, bool inlining) {
  // getPolyCallTargets ensures |target| is a constructor.
  MOZ_ASSERT_IF(target, target->isConstructor());

  // Only asm.js natives can be constructors and asm.js natives don't have a
  // JIT entry.
  MOZ_ASSERT_IF(target, !target->isNativeWithJitEntry());

  // Can't inline without a known target function.
  MOZ_ASSERT_IF(inlining, target);

  // Create |this| for unknown target.
  if (!target) {
    if (callee == newTarget) {
      if (MDefinition* createThis = createThisScriptedBaseline(callee)) {
        return createThis;
      }
    }
    return createThisSlow(callee, newTarget, inlining);
  }

  // Handle known native functions, bound functions and derived class
  // constructors. Note: proxies are already excluded since target has type
  // JSFunction.
  if (target->isNative()) {
    return constant(MagicValue(JS_IS_CONSTRUCTING));
  }
  if (target->constructorNeedsUninitializedThis()) {
    return constant(MagicValue(JS_UNINITIALIZED_LEXICAL));
  }

  if (callee == newTarget) {
    // We must not have an effectful .prototype lookup when inlining.
    MOZ_ASSERT_IF(inlining, target->hasNonConfigurablePrototypeDataProperty());

    // Try baking in the prototype.
    if (MDefinition* createThis = createThisScriptedSingleton(target)) {
      return createThis;
    }
    if (MDefinition* createThis = createThisScriptedBaseline(callee)) {
      return createThis;
    }
  }

  // We can use createThisScripted if newTarget is known to be a function with a
  // (builtin, getter-free) .prototype property and the callee is not one of the
  // isNative/constructorNeedsUninitializedThis cases handled above.
  JSFunction* newTargetFun =
      callee == newTarget ? target
                          : getSingleCallTarget(newTarget->resultTypeSet());
  if (newTargetFun && newTargetFun->hasNonConfigurablePrototypeDataProperty()) {
    return createThisScripted(callee, newTarget);
  }

  // The .prototype lookup may be effectful, so we can't inline the call.
  MOZ_ASSERT(!inlining);
  return createThisSlow(callee, newTarget, inlining);
}

AbortReasonOr<Ok> IonBuilder::jsop_funcall(uint32_t argc) {
  // Stack for JSOp::FunCall:
  // 1:      arg0
  // ...
  // argc:   argN
  // argc+1: JSFunction*, the 'f' in |f.call()|, in |this| position.
  // argc+2: The native 'call' function.

  int calleeDepth = -((int)argc + 2);
  int funcDepth = -((int)argc + 1);

  // If |Function.prototype.call| may be overridden, don't optimize callsite.
  TemporaryTypeSet* calleeTypes = current->peek(calleeDepth)->resultTypeSet();
  JSFunction* native = getSingleCallTarget(calleeTypes);
  if (!native || !native->isNative() || native->native() != &fun_call) {
    CallInfo callInfo(alloc(), pc, /* constructing = */ false,
                      /* ignoresReturnValue = */ BytecodeIsPopped(pc));
    if (!callInfo.init(current, argc)) {
      return abort(AbortReason::Alloc);
    }
    return makeCall(native, callInfo);
  }
  current->peek(calleeDepth)->setImplicitlyUsedUnchecked();

  // Extract call target.
  TemporaryTypeSet* funTypes = current->peek(funcDepth)->resultTypeSet();
  JSFunction* target = getSingleCallTarget(funTypes);

  CallInfo callInfo(alloc(), pc, /* constructing = */ false,
                    /* ignoresReturnValue = */ BytecodeIsPopped(pc));

  // Save prior call stack in case we need to resolve during bailout
  // recovery of inner inlined function. This includes the JSFunction and the
  // 'call' native function.
  MOZ_TRY(callInfo.savePriorCallStack(&mirGen_, current, argc + 2));

  // Shimmy the slots down to remove the native 'call' function.
  current->shimmySlots(funcDepth - 1);

  bool zeroArguments = (argc == 0);

  // If no |this| argument was provided, explicitly pass Undefined.
  // Pushing is safe here, since one stack slot has been removed.
  if (zeroArguments) {
    pushConstant(UndefinedValue());
  } else {
    // |this| becomes implicit in the call.
    argc -= 1;
  }

  if (!callInfo.init(current, argc)) {
    return abort(AbortReason::Alloc);
  }

  // Try to inline the call.
  if (!zeroArguments) {
    InliningDecision decision = makeInliningDecision(target, callInfo);
    switch (decision) {
      case InliningDecision_Error:
        return abort(AbortReason::Error);
      case InliningDecision_DontInline:
      case InliningDecision_WarmUpCountTooLow:
        break;
      case InliningDecision_Inline: {
        InliningStatus status;
        MOZ_TRY_VAR(status, inlineSingleCall(callInfo, target));
        if (status == InliningStatus_Inlined) {
          return Ok();
        }
        break;
      }
    }
  }

  // Call without inlining.
  return makeCall(target, callInfo);
}

AbortReasonOr<Ok> IonBuilder::jsop_funapply(uint32_t argc) {
  int calleeDepth = -((int)argc + 2);

  TemporaryTypeSet* calleeTypes = current->peek(calleeDepth)->resultTypeSet();
  JSFunction* native = getSingleCallTarget(calleeTypes);
  if (argc != 2 || info().analysisMode() == Analysis_ArgumentsUsage) {
    CallInfo callInfo(alloc(), pc, /* constructing = */ false,
                      /* ignoresReturnValue = */ BytecodeIsPopped(pc));
    if (!callInfo.init(current, argc)) {
      return abort(AbortReason::Alloc);
    }
    return makeCall(native, callInfo);
  }

  // Disable compilation if the second argument to |apply| cannot be guaranteed
  // to be either definitely |arguments| or definitely not |arguments|.
  MDefinition* argument = current->peek(-1);
  if (script()->argumentsHasVarBinding() &&
      argument->mightBeType(MIRType::MagicOptimizedArguments) &&
      argument->type() != MIRType::MagicOptimizedArguments) {
    return abort(AbortReason::Disable, "fun.apply with MaybeArguments");
  }

  // Fallback to regular call if arg 2 is not definitely |arguments|.
  if (argument->type() != MIRType::MagicOptimizedArguments) {
    // Optimize fun.apply(self, array) if the length is sane and there are no
    // holes.
    TemporaryTypeSet* objTypes = argument->resultTypeSet();
    if (native && native->isNative() && native->native() == fun_apply &&
        objTypes &&
        objTypes->getKnownClass(constraints()) == &ArrayObject::class_ &&
        !objTypes->hasObjectFlags(constraints(), OBJECT_FLAG_LENGTH_OVERFLOW) &&
        ElementAccessIsPacked(constraints(), argument)) {
      return jsop_funapplyarray(argc);
    }

    CallInfo callInfo(alloc(), pc, /* constructing = */ false,
                      /* ignoresReturnValue = */ BytecodeIsPopped(pc));
    if (!callInfo.init(current, argc)) {
      return abort(AbortReason::Alloc);
    }
    return makeCall(native, callInfo);
  }

  if ((!native || !native->isNative() || native->native() != fun_apply) &&
      info().analysisMode() != Analysis_DefiniteProperties) {
    return abort(AbortReason::Disable, "fun.apply speculation failed");
  }

  // Use funapply that definitely uses |arguments|
  return jsop_funapplyarguments(argc);
}

static void AssertSpreadArgIsArray(MDefinition* argument,
                                   CompilerConstraintList* constraints) {
#ifdef DEBUG
  // If we know class, ensure it is what we expected
  if (TemporaryTypeSet* objTypes = argument->resultTypeSet()) {
    if (const JSClass* clasp = objTypes->getKnownClass(constraints)) {
      MOZ_ASSERT(clasp == &ArrayObject::class_);
    }
  }
#endif
}

AbortReasonOr<Ok> IonBuilder::jsop_spreadcall() {
  MDefinition* argArr = current->pop();
  MDefinition* argThis = current->pop();
  MDefinition* argFunc = current->pop();

  // The arguments array is constructed by a JSOp::NewArray and not
  // leaked to user. The complications of spread call iterator behaviour are
  // handled when the user objects are expanded and copied into this hidden
  // array.
  AssertSpreadArgIsArray(argArr, constraints());

  // Extract call target.
  TemporaryTypeSet* funTypes = argFunc->resultTypeSet();
  JSFunction* target = getSingleCallTarget(funTypes);
  WrappedFunction* wrappedTarget =
      target ? new (alloc()) WrappedFunction(target) : nullptr;

  // Dense elements of argument array
  MElements* elements = MElements::New(alloc(), argArr);
  current->add(elements);

  MApplyArray* apply =
      MApplyArray::New(alloc(), wrappedTarget, argFunc, elements, argThis);
  current->add(apply);
  current->push(apply);
  MOZ_TRY(resumeAfter(apply));

  if (target && target->realm() == script()->realm()) {
    apply->setNotCrossRealm();
  }
  if (BytecodeIsPopped(pc)) {
    apply->setIgnoresReturnValue();
  }

  // TypeBarrier the call result
  TemporaryTypeSet* types = bytecodeTypes(pc);
  return pushTypeBarrier(apply, types, BarrierKind::TypeSet);
}

AbortReasonOr<Ok> IonBuilder::jsop_spreadnew() {
  MDefinition* newTarget = current->pop();
  MDefinition* argArr = current->pop();
  MDefinition* thisValue = current->pop();
  MDefinition* callee = current->pop();

  // The arguments array is constructed by JSOp::NewArray and not leaked to the
  // user. The complications of spread call iterator behaviour are handled when
  // the user objects are expanded and copied into this hidden array.
  AssertSpreadArgIsArray(argArr, constraints());

  // Extract call target.
  TemporaryTypeSet* funTypes = callee->resultTypeSet();
  JSFunction* target = getSingleCallTarget(funTypes);
  if (target && !target->isConstructor()) {
    // Don't optimize when the target doesn't support construct calls.
    target = nullptr;
  }
  WrappedFunction* wrappedTarget =
      target ? new (alloc()) WrappedFunction(target) : nullptr;

  // Inline the constructor on the caller-side.
  MDefinition* create = createThis(target, callee, newTarget, false);
  thisValue->setImplicitlyUsedUnchecked();

  // Dense elements of the argument array.
  MElements* elements = MElements::New(alloc(), argArr);
  current->add(elements);

  auto* apply = MConstructArray::New(alloc(), wrappedTarget, callee, elements,
                                     create, newTarget);
  current->add(apply);
  current->push(apply);
  MOZ_TRY(resumeAfter(apply));

  if (target && target->realm() == script()->realm()) {
    apply->setNotCrossRealm();
  }

  // TypeBarrier the call result.
  TemporaryTypeSet* types = bytecodeTypes(pc);
  return pushTypeBarrier(apply, types, BarrierKind::TypeSet);
}

bool IonBuilder::propertyIsConstantFunction(NativeObject* nobj, jsid id,
                                            bool (*test)(IonBuilder* builder,
                                                         JSFunction* fun)) {
  if (!nobj->isSingleton()) {
    return false;
  }

  TypeSet::ObjectKey* objKey = TypeSet::ObjectKey::get(nobj);
  if (analysisContext) {
    objKey->ensureTrackedProperty(analysisContext, id);
  }

  if (objKey->unknownProperties()) {
    return false;
  }

  HeapTypeSetKey property = objKey->property(id);
  Value value = UndefinedValue();
  if (!property.constant(constraints(), &value)) {
    return false;
  }
  return value.isObject() && value.toObject().is<JSFunction>() &&
         test(this, &value.toObject().as<JSFunction>());
}

bool IonBuilder::ensureArrayPrototypeIteratorNotModified() {
  NativeObject* obj = script()->global().maybeGetArrayPrototype();
  if (!obj) {
    return false;
  }

  jsid id = SYMBOL_TO_JSID(realm->runtime()->wellKnownSymbols().iterator);
  return propertyIsConstantFunction(obj, id, [](auto* builder, auto* fun) {
    CompileRuntime* runtime = builder->mirGen().runtime;
    return IsSelfHostedFunctionWithName(fun, runtime->names().ArrayValues);
  });
}

bool IonBuilder::ensureArrayIteratorPrototypeNextNotModified() {
  NativeObject* obj = script()->global().maybeGetArrayIteratorPrototype();
  if (!obj) {
    return false;
  }

  jsid id = NameToId(mirGen_.runtime->names().next);
  return propertyIsConstantFunction(obj, id, [](auto* builder, auto* fun) {
    return IsSelfHostedFunctionWithName(
        fun, builder->mirGen().runtime->names().ArrayIteratorNext);
  });
}

AbortReasonOr<Ok> IonBuilder::jsop_optimize_spreadcall() {
  MDefinition* arr = current->peek(-1);

  // Assuming optimization isn't available doesn't affect correctness.
  // TODO: Investigate dynamic checks.
  bool result = false;
  do {
    // Inline with MIsPackedArray if the conditions described in
    // js::OptimizeSpreadCall() are all met or can be expressed through
    // compiler constraints.

    // The argument is an array.
    TemporaryTypeSet* types = arr->resultTypeSet();
    if (!types || types->getKnownClass(constraints()) != &ArrayObject::class_) {
      break;
    }

    // The array has no hole.
    if (types->hasObjectFlags(constraints(), OBJECT_FLAG_NON_PACKED)) {
      break;
    }

    // The array's prototype is Array.prototype.
    JSObject* proto;
    if (!types->getCommonPrototype(constraints(), &proto)) {
      break;
    }
    NativeObject* arrayProto = script()->global().maybeGetArrayPrototype();
    if (!arrayProto || arrayProto != proto) {
      break;
    }

    // The array doesn't define an own @@iterator property.
    jsid id = SYMBOL_TO_JSID(realm->runtime()->wellKnownSymbols().iterator);
    bool res;
    MOZ_TRY_VAR(res, testNotDefinedProperty(arr, id, true));
    if (!res) {
      break;
    }

    // Array.prototype[@@iterator] is not modified.
    if (!ensureArrayPrototypeIteratorNotModified()) {
      break;
    }

    // %ArrayIteratorPrototype%.next is not modified.
    if (!ensureArrayIteratorPrototypeNextNotModified()) {
      break;
    }

    result = true;
  } while (false);

  if (result) {
    auto* ins = MIsPackedArray::New(alloc(), arr);
    current->add(ins);
    current->push(ins);
  } else {
    arr->setImplicitlyUsedUnchecked();
    pushConstant(BooleanValue(false));
  }
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_funapplyarray(uint32_t argc) {
  MOZ_ASSERT(argc == 2);

  int funcDepth = -((int)argc + 1);

  // Extract call target.
  TemporaryTypeSet* funTypes = current->peek(funcDepth)->resultTypeSet();
  JSFunction* target = getSingleCallTarget(funTypes);

  // Pop the array agument
  MDefinition* argObj = current->pop();

  MElements* elements = MElements::New(alloc(), argObj);
  current->add(elements);

  // Pop the |this| argument.
  MDefinition* argThis = current->pop();

  // Unwrap the (JSFunction *) parameter.
  MDefinition* argFunc = current->pop();

  // Pop apply function.
  MDefinition* nativeFunc = current->pop();
  nativeFunc->setImplicitlyUsedUnchecked();

  WrappedFunction* wrappedTarget =
      target ? new (alloc()) WrappedFunction(target) : nullptr;
  MApplyArray* apply =
      MApplyArray::New(alloc(), wrappedTarget, argFunc, elements, argThis);
  current->add(apply);
  current->push(apply);
  MOZ_TRY(resumeAfter(apply));

  if (target && target->realm() == script()->realm()) {
    apply->setNotCrossRealm();
  }
  if (BytecodeIsPopped(pc)) {
    apply->setIgnoresReturnValue();
  }

  TemporaryTypeSet* types = bytecodeTypes(pc);
  return pushTypeBarrier(apply, types, BarrierKind::TypeSet);
}

AbortReasonOr<Ok> CallInfo::savePriorCallStack(MIRGenerator* mir,
                                               MBasicBlock* current,
                                               size_t peekDepth) {
  MOZ_ASSERT(priorArgs_.empty());
  if (!priorArgs_.reserve(peekDepth)) {
    return mir->abort(AbortReason::Alloc);
  }
  while (peekDepth) {
    priorArgs_.infallibleAppend(current->peek(0 - int32_t(peekDepth)));
    peekDepth--;
  }
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_funapplyarguments(uint32_t argc) {
  // Stack for JSOp::FunApply:
  // 1:      Vp
  // 2:      This
  // argc+1: JSFunction*, the 'f' in |f.call()|, in |this| position.
  // argc+2: The native 'apply' function.

  int funcDepth = -((int)argc + 1);

  // Extract call target.
  TemporaryTypeSet* funTypes = current->peek(funcDepth)->resultTypeSet();
  JSFunction* target = getSingleCallTarget(funTypes);

  // When this script isn't inlined, use MApplyArgs,
  // to copy the arguments from the stack and call the function
  if (inliningDepth_ == 0 &&
      info().analysisMode() != Analysis_DefiniteProperties) {
    // The array argument corresponds to the arguments object. As the JIT
    // is implicitly reading the arguments object in the next instruction,
    // we need to prevent the deletion of the arguments object from resume
    // points, so that Baseline will behave correctly after a bailout.
    MDefinition* vp = current->pop();
    vp->setImplicitlyUsedUnchecked();

    MDefinition* argThis = current->pop();

    // Unwrap the (JSFunction*) parameter.
    MDefinition* argFunc = current->pop();

    // Pop apply function.
    MDefinition* nativeFunc = current->pop();
    nativeFunc->setImplicitlyUsedUnchecked();

    MArgumentsLength* numArgs = MArgumentsLength::New(alloc());
    current->add(numArgs);

    WrappedFunction* wrappedTarget =
        target ? new (alloc()) WrappedFunction(target) : nullptr;
    MApplyArgs* apply =
        MApplyArgs::New(alloc(), wrappedTarget, argFunc, numArgs, argThis);
    current->add(apply);
    current->push(apply);
    MOZ_TRY(resumeAfter(apply));

    if (target && target->realm() == script()->realm()) {
      apply->setNotCrossRealm();
    }
    if (BytecodeIsPopped(pc)) {
      apply->setIgnoresReturnValue();
    }

    TemporaryTypeSet* types = bytecodeTypes(pc);
    return pushTypeBarrier(apply, types, BarrierKind::TypeSet);
  }

  // When inlining we have the arguments the function gets called with
  // and can optimize even more, by just calling the functions with the args.
  // We also try this path when doing the definite properties analysis, as we
  // can inline the apply() target and don't care about the actual arguments
  // that were passed in.

  CallInfo callInfo(alloc(), pc, /* constructing = */ false,
                    /* ignoresReturnValue = */ BytecodeIsPopped(pc));
  MOZ_TRY(callInfo.savePriorCallStack(&mirGen_, current, 4));

  // Vp
  MDefinition* vp = current->pop();
  vp->setImplicitlyUsedUnchecked();

  // Arguments
  if (inliningDepth_) {
    if (!callInfo.setArgs(inlineCallInfo_->argv())) {
      return abort(AbortReason::Alloc);
    }
  }

  // This
  MDefinition* argThis = current->pop();
  callInfo.setThis(argThis);

  // Pop function parameter.
  MDefinition* argFunc = current->pop();
  callInfo.setFun(argFunc);

  // Pop apply function.
  MDefinition* nativeFunc = current->pop();
  nativeFunc->setImplicitlyUsedUnchecked();

  // Try to inline the call.
  InliningDecision decision = makeInliningDecision(target, callInfo);
  switch (decision) {
    case InliningDecision_Error:
      return abort(AbortReason::Error);
    case InliningDecision_DontInline:
    case InliningDecision_WarmUpCountTooLow:
      break;
    case InliningDecision_Inline: {
      InliningStatus status;
      MOZ_TRY_VAR(status, inlineSingleCall(callInfo, target));
      if (status == InliningStatus_Inlined) {
        return Ok();
      }
    }
  }

  return makeCall(target, callInfo);
}

AbortReasonOr<Ok> IonBuilder::jsop_call(uint32_t argc, bool constructing,
                                        bool ignoresReturnValue) {
  // If this call has never executed, try to seed the observed type set
  // based on how the call result is used.
  TemporaryTypeSet* observed = bytecodeTypes(pc);
  if (observed->empty()) {
    if (BytecodeFlowsToBitop(pc)) {
      observed->addType(TypeSet::Int32Type(), alloc_->lifoAlloc());
    } else if (JSOp(*GetNextPc(pc)) == JSOp::Pos) {
      // Note: this is lame, overspecialized on the code patterns used
      // by asm.js and should be replaced by a more general mechanism.
      // See bug 870847.
      observed->addType(TypeSet::DoubleType(), alloc_->lifoAlloc());
    }
  }

  int calleeDepth = -((int)argc + 2 + constructing);

  // Acquire known call target if existent.
  InliningTargets targets(alloc());
  TemporaryTypeSet* calleeTypes = current->peek(calleeDepth)->resultTypeSet();
  if (calleeTypes) {
    MOZ_TRY(getPolyCallTargets(calleeTypes, constructing, targets, 4));
  }

  CallInfo callInfo(alloc(), pc, constructing, ignoresReturnValue);
  if (!callInfo.init(current, argc)) {
    return abort(AbortReason::Alloc);
  }

  // Try inlining
  InliningStatus status;
  MOZ_TRY_VAR(status, inlineCallsite(targets, callInfo));
  if (status == InliningStatus_Inlined) {
    return Ok();
  }

  // Discard unreferenced & pre-allocated resume points.
  replaceMaybeFallbackFunctionGetter(nullptr);

  // No inline, just make the call.
  Maybe<CallTargets> callTargets;
  if (!targets.empty()) {
    callTargets.emplace(alloc());
    for (const InliningTarget& target : targets) {
      if (!target.target->is<JSFunction>()) {
        callTargets = Nothing();
        break;
      }
      if (!callTargets->append(&target.target->as<JSFunction>())) {
        return abort(AbortReason::Alloc);
      }
    }
  }

  if (status == InliningStatus_WarmUpCountTooLow && callTargets &&
      callTargets->length() == 1 && isHighestOptimizationLevel()) {
    JSFunction* target = callTargets.ref()[0];
    MRecompileCheck* check =
        MRecompileCheck::New(alloc(), target->nonLazyScript(),
                             optimizationInfo().inliningRecompileThreshold(),
                             MRecompileCheck::RecompileCheckType::Inlining);
    current->add(check);
  }

  return makeCall(callTargets, callInfo);
}

AbortReasonOr<bool> IonBuilder::testShouldDOMCall(TypeSet* inTypes,
                                                  JSFunction* func,
                                                  JSJitInfo::OpType opType) {
  if (!func->isNative() || !func->hasJitInfo()) {
    return false;
  }

  // If all the DOM objects flowing through are legal with this
  // property, we can bake in a call to the bottom half of the DOM
  // accessor
  DOMInstanceClassHasProtoAtDepth instanceChecker =
      realm->runtime()->DOMcallbacks()->instanceClassMatchesProto;

  const JSJitInfo* jinfo = func->jitInfo();
  if (jinfo->type() != opType) {
    return false;
  }

  for (unsigned i = 0; i < inTypes->getObjectCount(); i++) {
    TypeSet::ObjectKey* key = inTypes->getObject(i);
    if (!key) {
      continue;
    }

    if (!alloc().ensureBallast()) {
      return abort(AbortReason::Alloc);
    }

    if (!key->hasStableClassAndProto(constraints())) {
      return false;
    }

    if (!instanceChecker(key->clasp(), jinfo->protoID, jinfo->depth)) {
      return false;
    }
  }

  return true;
}

static bool ArgumentTypesMatch(MDefinition* def, StackTypeSet* calleeTypes) {
  MOZ_ASSERT(calleeTypes);

  if (calleeTypes->unknown()) {
    return true;
  }

  if (TypeSet::IsUntrackedMIRType(def->type())) {
    // The TypeSet has to be marked as unknown. See JitScript::MonitorThisType.
    return false;
  }

  if (def->resultTypeSet()) {
    MOZ_ASSERT(def->type() == MIRType::Value || def->mightBeType(def->type()));
    return def->resultTypeSet()->isSubset(calleeTypes);
  }

  if (def->type() == MIRType::Value) {
    return false;
  }

  if (def->type() == MIRType::Object) {
    return calleeTypes->unknownObject();
  }

  return calleeTypes->mightBeMIRType(def->type());
}

bool IonBuilder::testNeedsArgumentCheck(JSFunction* target,
                                        CallInfo& callInfo) {
  // If we have a known target, check if the caller arg types are a subset of
  // callee. Since typeset accumulates and can't decrease that means we don't
  // need to check the arguments anymore.

  if (!target->hasBytecode()) {
    return true;
  }

  JSScript* targetScript = target->nonLazyScript();
  JitScript* jitScript = targetScript->maybeJitScript();
  if (!jitScript) {
    return true;
  }

  AutoSweepJitScript sweep(targetScript);
  if (!ArgumentTypesMatch(callInfo.thisArg(),
                          jitScript->thisTypes(sweep, targetScript))) {
    return true;
  }
  uint32_t expected_args = std::min<uint32_t>(callInfo.argc(), target->nargs());
  for (size_t i = 0; i < expected_args; i++) {
    if (!ArgumentTypesMatch(callInfo.getArg(i),
                            jitScript->argTypes(sweep, targetScript, i))) {
      return true;
    }
  }
  for (size_t i = callInfo.argc(); i < target->nargs(); i++) {
    StackTypeSet* types = jitScript->argTypes(sweep, targetScript, i);
    if (!types->mightBeMIRType(MIRType::Undefined)) {
      return true;
    }
  }

  return false;
}

AbortReasonOr<MCall*> IonBuilder::makeCallHelper(
    const Maybe<CallTargets>& targets, CallInfo& callInfo) {
  // This function may be called with mutated stack.
  // Querying TI for popped types is invalid.

  MOZ_ASSERT_IF(targets, !targets->empty());

  JSFunction* target = nullptr;
  if (targets && targets->length() == 1) {
    target = targets.ref()[0];
  }

  bool isDOMCall = false;
  DOMObjectKind objKind = DOMObjectKind::Unknown;
  if (target && !callInfo.constructing()) {
    // We know we have a single call target.  Check whether the "this" types
    // are DOM types and our function a DOM function, and if so flag the
    // MCall accordingly.
    TemporaryTypeSet* thisTypes = callInfo.thisArg()->resultTypeSet();
    if (thisTypes && thisTypes->getKnownMIRType() == MIRType::Object &&
        thisTypes->isDOMClass(constraints(), &objKind)) {
      MOZ_TRY_VAR(isDOMCall,
                  testShouldDOMCall(thisTypes, target, JSJitInfo::Method));
    }
  }

  bool needsThisCheck = false;
  if (callInfo.constructing()) {
    // Inline the this-object allocation on the caller-side.
    MDefinition* create =
        createThis(target, callInfo.fun(), callInfo.getNewTarget(),
                   /* inlining = */ false);
    callInfo.thisArg()->setImplicitlyUsedUnchecked();
    callInfo.setThis(create);
    needsThisCheck = create->isCreateThis();
    if (needsThisCheck) {
      // We have to use the LCallGeneric path.
      target = nullptr;
    }
  }

  uint32_t targetArgs = callInfo.argc();

  // Collect number of missing arguments provided that the target is
  // scripted. Native functions are passed an explicit 'argc' parameter.
  if (target && !target->isBuiltinNative()) {
    targetArgs = std::max<uint32_t>(target->nargs(), callInfo.argc());
  }

  MCall* call =
      MCall::New(alloc(), target, targetArgs + 1 + callInfo.constructing(),
                 callInfo.argc(), callInfo.constructing(),
                 callInfo.ignoresReturnValue(), isDOMCall, objKind);
  if (!call) {
    return abort(AbortReason::Alloc);
  }

  if (callInfo.constructing()) {
    if (needsThisCheck) {
      call->setNeedsThisCheck();
    }
    call->addArg(targetArgs + 1, callInfo.getNewTarget());
  }

  // Explicitly pad any missing arguments with |undefined|.
  // This permits skipping the argumentsRectifier.
  MOZ_ASSERT_IF(target && targetArgs > callInfo.argc(),
                !target->isBuiltinNative());
  for (int i = targetArgs; i > (int)callInfo.argc(); i--) {
    MConstant* undef = constant(UndefinedValue());
    if (!alloc().ensureBallast()) {
      return abort(AbortReason::Alloc);
    }
    call->addArg(i, undef);
  }

  // Add explicit arguments.
  // Skip addArg(0) because it is reserved for this
  for (int32_t i = callInfo.argc() - 1; i >= 0; i--) {
    call->addArg(i + 1, callInfo.getArg(i));
  }

  // Now that we've told it about all the args, compute whether it's movable
  call->computeMovable();

  // Pass |this| and function.
  MDefinition* thisArg = callInfo.thisArg();
  call->addArg(0, thisArg);

  if (targets) {
    // The callee must be one of the target JSFunctions, so we don't need a
    // Class check.
    call->disableClassCheck();

    // Determine whether we can skip the callee's prologue type checks and
    // whether we have to switch realms.
    bool needArgCheck = false;
    bool maybeCrossRealm = false;
    for (JSFunction* target : targets.ref()) {
      if (testNeedsArgumentCheck(target, callInfo)) {
        needArgCheck = true;
      }
      if (target->realm() != script()->realm()) {
        maybeCrossRealm = true;
      }
    }
    if (!needArgCheck) {
      call->disableArgCheck();
    }
    if (!maybeCrossRealm) {
      call->setNotCrossRealm();
    }
  }

  call->initFunction(callInfo.fun());

  current->add(call);
  return call;
}

static bool DOMCallNeedsBarrier(const JSJitInfo* jitinfo,
                                TemporaryTypeSet* types) {
  MOZ_ASSERT(jitinfo->type() != JSJitInfo::InlinableNative);

  // If the return type of our DOM native is in "types" already, we don't
  // actually need a barrier.
  if (jitinfo->returnType() == JSVAL_TYPE_UNKNOWN) {
    return true;
  }

  // JSVAL_TYPE_OBJECT doesn't tell us much; we still have to barrier on the
  // actual type of the object.
  if (jitinfo->returnType() == JSVAL_TYPE_OBJECT) {
    return true;
  }

  // No need for a barrier if we're already expecting the type we'll produce.
  return MIRTypeFromValueType(jitinfo->returnType()) !=
         types->getKnownMIRType();
}

AbortReasonOr<Ok> IonBuilder::makeCall(const Maybe<CallTargets>& targets,
                                       CallInfo& callInfo) {
#ifdef DEBUG
  // Constructor calls to non-constructors should throw. We don't want to use
  // CallKnown in this case.
  if (callInfo.constructing() && targets) {
    for (JSFunction* target : targets.ref()) {
      MOZ_ASSERT(target->isConstructor());
    }
  }
#endif

  MCall* call;
  MOZ_TRY_VAR(call, makeCallHelper(targets, callInfo));

  current->push(call);
  if (call->isEffectful()) {
    MOZ_TRY(resumeAfter(call));
  }

  TemporaryTypeSet* types = bytecodeTypes(pc);

  if (call->isCallDOMNative()) {
    return pushDOMTypeBarrier(call, types,
                              call->getSingleTarget()->rawJSFunction());
  }

  return pushTypeBarrier(call, types, BarrierKind::TypeSet);
}

AbortReasonOr<Ok> IonBuilder::makeCall(JSFunction* target, CallInfo& callInfo) {
  Maybe<CallTargets> targets;
  if (target) {
    targets.emplace(alloc());
    if (!targets->append(target)) {
      return abort(AbortReason::Alloc);
    }
  }
  return makeCall(targets, callInfo);
}

AbortReasonOr<Ok> IonBuilder::jsop_eval(uint32_t argc) {
  int calleeDepth = -((int)argc + 2);
  TemporaryTypeSet* calleeTypes = current->peek(calleeDepth)->resultTypeSet();

  // Emit a normal call if the eval has never executed. This keeps us from
  // disabling compilation for the script when testing with --ion-eager.
  if (calleeTypes && calleeTypes->empty()) {
    return jsop_call(argc, /* constructing = */ false, false);
  }

  JSFunction* target = getSingleCallTarget(calleeTypes);
  if (!target) {
    return abort(AbortReason::Disable, "No single callee for eval()");
  }

  if (script()->global().valueIsEval(ObjectValue(*target))) {
    if (argc != 1) {
      return abort(AbortReason::Disable,
                   "Direct eval with more than one argument");
    }

    if (!info().funMaybeLazy()) {
      return abort(AbortReason::Disable, "Direct eval in global code");
    }

    if (info().funMaybeLazy()->isArrow()) {
      return abort(AbortReason::Disable, "Direct eval from arrow function");
    }

    CallInfo callInfo(alloc(), pc, /* constructing = */ false,
                      /* ignoresReturnValue = */ BytecodeIsPopped(pc));
    if (!callInfo.init(current, argc)) {
      return abort(AbortReason::Alloc);
    }
    callInfo.setImplicitlyUsedUnchecked();

    callInfo.fun()->setImplicitlyUsedUnchecked();

    MDefinition* envChain = current->environmentChain();
    MDefinition* string = callInfo.getArg(0);

    // Direct eval acts as identity on non-string types according to
    // ES5 15.1.2.1 step 1.
    if (!string->mightBeType(MIRType::String)) {
      current->push(string);
      TemporaryTypeSet* types = bytecodeTypes(pc);
      return pushTypeBarrier(string, types, BarrierKind::TypeSet);
    }

    MOZ_TRY(jsop_newtarget());
    MDefinition* newTargetValue = current->pop();

    // Try to pattern match 'eval(v + "()")'. In this case v is likely a
    // name on the env chain and the eval is performing a call on that
    // value. Use an env chain lookup rather than a full eval.
    if (string->isConcat() &&
        string->getOperand(1)->type() == MIRType::String &&
        string->getOperand(1)->maybeConstantValue()) {
      JSAtom* atom =
          &string->getOperand(1)->maybeConstantValue()->toString()->asAtom();

      if (StringEqualsLiteral(atom, "()")) {
        MDefinition* name = string->getOperand(0);
        MInstruction* dynamicName =
            MGetDynamicName::New(alloc(), envChain, name);
        current->add(dynamicName);

        current->push(dynamicName);
        current->push(constant(UndefinedValue()));  // thisv

        CallInfo evalCallInfo(alloc(), pc, /* constructing = */ false,
                              /* ignoresReturnValue = */ BytecodeIsPopped(pc));
        if (!evalCallInfo.init(current, /* argc = */ 0)) {
          return abort(AbortReason::Alloc);
        }

        return makeCall(nullptr, evalCallInfo);
      }
    }

    MInstruction* ins =
        MCallDirectEval::New(alloc(), envChain, string, newTargetValue, pc);
    current->add(ins);
    current->push(ins);

    TemporaryTypeSet* types = bytecodeTypes(pc);
    MOZ_TRY(resumeAfter(ins));
    return pushTypeBarrier(ins, types, BarrierKind::TypeSet);
  }

  return jsop_call(argc, /* constructing = */ false, false);
}

AbortReasonOr<Ok> IonBuilder::jsop_compare(JSOp op) {
  MDefinition* right = current->pop();
  MDefinition* left = current->pop();

  return jsop_compare(op, left, right);
}

AbortReasonOr<Ok> IonBuilder::jsop_compare(JSOp op, MDefinition* left,
                                           MDefinition* right) {
  bool emitted = false;

  if (!forceInlineCaches()) {
    MOZ_TRY(compareTryCharacter(&emitted, op, left, right));
    if (emitted) {
      return Ok();
    }
    MOZ_TRY(compareTrySpecialized(&emitted, op, left, right));
    if (emitted) {
      return Ok();
    }
    MOZ_TRY(compareTryBitwise(&emitted, op, left, right));
    if (emitted) {
      return Ok();
    }
    MOZ_TRY(
        compareTrySpecializedOnBaselineInspector(&emitted, op, left, right));
    if (emitted) {
      return Ok();
    }
  }

  MOZ_TRY(compareTryBinaryStub(&emitted, left, right));
  if (emitted) {
    return Ok();
  }

  // Not possible to optimize. Do a slow vm call.
  MCompare* ins = MCompare::New(alloc(), left, right, op);
  ins->cacheOperandMightEmulateUndefined(constraints());

  current->add(ins);
  current->push(ins);
  if (ins->isEffectful()) {
    MOZ_TRY(resumeAfter(ins));
  }

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::compareTryCharacter(bool* emitted, JSOp op,
                                                  MDefinition* left,
                                                  MDefinition* right) {
  MOZ_ASSERT(*emitted == false);

  // |str[i]| is compiled as |MFromCharCode(MCharCodeAt(str, i))|.
  auto isCharAccess = [](MDefinition* ins) {
    return ins->isFromCharCode() &&
           ins->toFromCharCode()->input()->isCharCodeAt();
  };

  if (left->isConstant() || right->isConstant()) {
    // Try to optimize |MConstant(string) <compare> (MFromCharCode MCharCodeAt)|
    // as |MConstant(charcode) <compare> MCharCodeAt|.
    MConstant* constant;
    MDefinition* operand;
    if (left->isConstant()) {
      constant = left->toConstant();
      operand = right;
    } else {
      constant = right->toConstant();
      operand = left;
    }

    if (constant->type() != MIRType::String ||
        constant->toString()->length() != 1 || !isCharAccess(operand)) {
      return Ok();
    }

    char16_t charCode = constant->toString()->asAtom().latin1OrTwoByteChar(0);
    constant->setImplicitlyUsedUnchecked();

    MConstant* charCodeConst = MConstant::New(alloc(), Int32Value(charCode));
    current->add(charCodeConst);

    MDefinition* charCodeAt = operand->toFromCharCode()->input();
    operand->setImplicitlyUsedUnchecked();

    if (left == constant) {
      left = charCodeConst;
      right = charCodeAt;
    } else {
      left = charCodeAt;
      right = charCodeConst;
    }
  } else if (isCharAccess(left) && isCharAccess(right)) {
    // Try to optimize |(MFromCharCode MCharCodeAt) <compare> (MFromCharCode
    // MCharCodeAt)| as |MCharCodeAt <compare> MCharCodeAt|.

    MDefinition* leftCharCodeAt = left->toFromCharCode()->input();
    left->setImplicitlyUsedUnchecked();

    MDefinition* rightCharCodeAt = right->toFromCharCode()->input();
    right->setImplicitlyUsedUnchecked();

    left = leftCharCodeAt;
    right = rightCharCodeAt;
  } else {
    return Ok();
  }

  MCompare* ins = MCompare::New(alloc(), left, right, op);
  ins->setCompareType(MCompare::Compare_Int32);
  ins->cacheOperandMightEmulateUndefined(constraints());

  current->add(ins);
  current->push(ins);

  MOZ_ASSERT(!ins->isEffectful());
  *emitted = true;
  return Ok();
}

static bool ObjectOrSimplePrimitive(MDefinition* op) {
  // Return true if op is either undefined/null/boolean/int32/symbol or an
  // object.
  return !op->mightBeType(MIRType::String) &&
         !op->mightBeType(MIRType::BigInt) &&
         !op->mightBeType(MIRType::Double) &&
         !op->mightBeType(MIRType::Float32) && !op->mightBeMagicType();
}

AbortReasonOr<Ok> IonBuilder::compareTrySpecialized(bool* emitted, JSOp op,
                                                    MDefinition* left,
                                                    MDefinition* right) {
  MOZ_ASSERT(*emitted == false);

  // Try to emit an compare based on the input types.

  MCompare::CompareType type = MCompare::determineCompareType(op, left, right);
  if (type == MCompare::Compare_Unknown) {
    return Ok();
  }

  MCompare* ins = MCompare::New(alloc(), left, right, op);
  ins->setCompareType(type);
  ins->cacheOperandMightEmulateUndefined(constraints());

  // Some compare types need to have the specific type in the rhs.
  // Swap operands if that is not the case.
  if (type == MCompare::Compare_StrictString &&
      right->type() != MIRType::String) {
    ins->swapOperands();
  } else if (type == MCompare::Compare_Null && right->type() != MIRType::Null) {
    ins->swapOperands();
  } else if (type == MCompare::Compare_Undefined &&
             right->type() != MIRType::Undefined) {
    ins->swapOperands();
  } else if (type == MCompare::Compare_Boolean &&
             right->type() != MIRType::Boolean) {
    ins->swapOperands();
  }

  // Replace inputs with unsigned variants if needed.
  if (type == MCompare::Compare_UInt32) {
    ins->replaceWithUnsignedOperands();
  }

  current->add(ins);
  current->push(ins);

  MOZ_ASSERT(!ins->isEffectful());
  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::compareTryBitwise(bool* emitted, JSOp op,
                                                MDefinition* left,
                                                MDefinition* right) {
  MOZ_ASSERT(*emitted == false);

  // Try to emit a bitwise compare. Check if a bitwise compare equals the wanted
  // result for all observed operand types.

  // Only allow loose and strict equality.
  if (op != JSOp::Eq && op != JSOp::Ne && op != JSOp::StrictEq &&
      op != JSOp::StrictNe) {
    return Ok();
  }

  // Only primitive (not double/string) or objects are supported.
  // I.e. Undefined/Null/Boolean/Int32/Symbol and Object
  if (!ObjectOrSimplePrimitive(left) || !ObjectOrSimplePrimitive(right)) {
    return Ok();
  }

  // In the loose comparison more values could be the same,
  // but value comparison reporting otherwise.
  if (op == JSOp::Eq || op == JSOp::Ne) {
    // Objects that emulate undefined are not supported.
    if (left->maybeEmulatesUndefined(constraints()) ||
        right->maybeEmulatesUndefined(constraints())) {
      return Ok();
    }

    // Undefined compared loosy to Null is not supported,
    // because tag is different, but value can be the same (undefined == null).
    if ((left->mightBeType(MIRType::Undefined) &&
         right->mightBeType(MIRType::Null)) ||
        (left->mightBeType(MIRType::Null) &&
         right->mightBeType(MIRType::Undefined))) {
      return Ok();
    }

    // Int32 compared loosy to Boolean is not supported,
    // because tag is different, but value can be the same (1 == true).
    if ((left->mightBeType(MIRType::Int32) &&
         right->mightBeType(MIRType::Boolean)) ||
        (left->mightBeType(MIRType::Boolean) &&
         right->mightBeType(MIRType::Int32))) {
      return Ok();
    }

    // For loosy comparison of an object with a Boolean/Number/String/Symbol
    // the valueOf the object is taken. Therefore not supported.
    bool simpleLHS = left->mightBeType(MIRType::Boolean) ||
                     left->mightBeType(MIRType::Int32) ||
                     left->mightBeType(MIRType::Symbol);
    bool simpleRHS = right->mightBeType(MIRType::Boolean) ||
                     right->mightBeType(MIRType::Int32) ||
                     right->mightBeType(MIRType::Symbol);
    if ((left->mightBeType(MIRType::Object) && simpleRHS) ||
        (right->mightBeType(MIRType::Object) && simpleLHS)) {
      return Ok();
    }
  }

  MCompare* ins = MCompare::New(alloc(), left, right, op);
  ins->setCompareType(MCompare::Compare_Bitwise);
  ins->cacheOperandMightEmulateUndefined(constraints());

  current->add(ins);
  current->push(ins);

  MOZ_ASSERT(!ins->isEffectful());
  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::compareTrySpecializedOnBaselineInspector(
    bool* emitted, JSOp op, MDefinition* left, MDefinition* right) {
  MOZ_ASSERT(*emitted == false);

  // Not supported for call expressions.
  if (IsInvokePC(pc)) {
    return Ok();
  }

  // Try to specialize based on any baseline caches that have been generated
  // for the opcode. These will cause the instruction's type policy to insert
  // fallible unboxes to the appropriate input types.

  // Strict equality isn't supported.
  if (op == JSOp::StrictEq || op == JSOp::StrictNe) {
    return Ok();
  }

  MCompare::CompareType type = inspector->expectedCompareType(pc);
  if (type == MCompare::Compare_Unknown) {
    return Ok();
  }

  MCompare* ins = MCompare::New(alloc(), left, right, op);
  ins->setCompareType(type);
  ins->cacheOperandMightEmulateUndefined(constraints());

  current->add(ins);
  current->push(ins);

  MOZ_ASSERT(!ins->isEffectful());
  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::compareTryBinaryStub(bool* emitted,
                                                   MDefinition* left,
                                                   MDefinition* right) {
  MOZ_ASSERT(*emitted == false);

  // Try to emit a CacheIR Stub.
  if (JitOptions.disableCacheIR) {
    return Ok();
  }

  if (IsInvokePC(pc)) {
    return Ok();
  }

  MBinaryCache* stub =
      MBinaryCache::New(alloc(), left, right, MIRType::Boolean);
  current->add(stub);
  current->push(stub);
  MOZ_TRY(resumeAfter(stub));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::newArrayTryTemplateObject(
    bool* emitted, JSObject* templateObject, uint32_t length) {
  MOZ_ASSERT(*emitted == false);

  if (!templateObject) {
    return Ok();
  }

  MOZ_ASSERT(length <= NativeObject::MAX_DENSE_ELEMENTS_COUNT);

  size_t arraySlots =
      gc::GetGCKindSlots(templateObject->asTenured().getAllocKind()) -
      ObjectElements::VALUES_PER_HEADER;

  if (length > arraySlots) {
    return Ok();
  }

  // Emit fastpath.

  gc::InitialHeap heap = templateObject->group()->initialHeap(constraints());
  MConstant* templateConst =
      MConstant::NewConstraintlessObject(alloc(), templateObject);
  current->add(templateConst);

  MNewArray* ins =
      MNewArray::New(alloc(), constraints(), length, templateConst, heap, pc);
  current->add(ins);
  current->push(ins);

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::newArrayTryVM(bool* emitted,
                                            JSObject* templateObject,
                                            uint32_t length) {
  MOZ_ASSERT(*emitted == false);

  gc::InitialHeap heap = gc::DefaultHeap;
  MConstant* templateConst = MConstant::New(alloc(), NullValue());

  if (templateObject) {
    heap = templateObject->group()->initialHeap(constraints());
    templateConst = MConstant::NewConstraintlessObject(alloc(), templateObject);
  }

  current->add(templateConst);

  MNewArray* ins =
      MNewArray::NewVM(alloc(), constraints(), length, templateConst, heap, pc);
  current->add(ins);
  current->push(ins);

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_newarray(uint32_t length) {
  JSObject* templateObject = inspector->getTemplateObject(pc);
  MOZ_TRY(jsop_newarray(templateObject, length));

  // Improve resulting typeset.
  ObjectGroup* templateGroup = inspector->getTemplateObjectGroup(pc);
  if (templateGroup) {
    TemporaryTypeSet* types =
        MakeSingletonTypeSet(alloc(), constraints(), templateGroup);
    current->peek(-1)->setResultTypeSet(types);
  }

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_newarray(JSObject* templateObject,
                                            uint32_t length) {
  bool emitted = false;

  MOZ_TRY(newArrayTryTemplateObject(&emitted, templateObject, length));
  if (emitted) {
    return Ok();
  }

  MOZ_TRY(newArrayTryVM(&emitted, templateObject, length));
  if (emitted) {
    return Ok();
  }

  MOZ_CRASH("newarray should have been emited");
}

AbortReasonOr<Ok> IonBuilder::jsop_newarray_copyonwrite() {
  ArrayObject* templateObject = ObjectGroup::getCopyOnWriteObject(script(), pc);

  // The baseline compiler should have ensured the template object has a type
  // with the copy on write flag set already. During the arguments usage
  // analysis the baseline compiler hasn't run yet, however, though in this
  // case the template object's type doesn't matter.
  ObjectGroup* group = templateObject->group();
  MOZ_ASSERT_IF(
      info().analysisMode() != Analysis_ArgumentsUsage,
      group->hasAllFlagsDontCheckGeneration(OBJECT_FLAG_COPY_ON_WRITE));

  MConstant* templateConst =
      MConstant::NewConstraintlessObject(alloc(), templateObject);
  current->add(templateConst);

  MNewArrayCopyOnWrite* ins = MNewArrayCopyOnWrite::New(
      alloc(), constraints(), templateConst, group->initialHeap(constraints()));

  current->add(ins);
  current->push(ins);

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::newObjectTryTemplateObject(
    bool* emitted, JSObject* templateObject) {
  MOZ_ASSERT(*emitted == false);

  if (!templateObject) {
    return Ok();
  }

  // Emit fastpath.

  MNewObject::Mode mode;
  if (JSOp(*pc) == JSOp::NewObject || JSOp(*pc) == JSOp::NewObjectWithGroup ||
      JSOp(*pc) == JSOp::NewInit) {
    mode = MNewObject::ObjectLiteral;
  } else {
    mode = MNewObject::ObjectCreate;
  }

  gc::InitialHeap heap = templateObject->group()->initialHeap(constraints());
  MConstant* templateConst =
      MConstant::NewConstraintlessObject(alloc(), templateObject);
  current->add(templateConst);

  MNewObject* ins =
      MNewObject::New(alloc(), constraints(), templateConst, heap, mode);
  current->add(ins);
  current->push(ins);

  MOZ_TRY(resumeAfter(ins));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::newObjectTryVM(bool* emitted,
                                             JSObject* templateObject) {
  // Emit a VM call.
  MOZ_ASSERT(JSOp(*pc) == JSOp::NewObject ||
             JSOp(*pc) == JSOp::NewObjectWithGroup ||
             JSOp(*pc) == JSOp::NewInit);

  gc::InitialHeap heap = gc::DefaultHeap;
  MConstant* templateConst = MConstant::New(alloc(), NullValue());

  if (templateObject) {
    heap = templateObject->group()->initialHeap(constraints());
    templateConst = MConstant::NewConstraintlessObject(alloc(), templateObject);
  }

  current->add(templateConst);

  MNewObject* ins = MNewObject::NewVM(alloc(), constraints(), templateConst,
                                      heap, MNewObject::ObjectLiteral);
  current->add(ins);
  current->push(ins);

  MOZ_TRY(resumeAfter(ins));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_newobject() {
  bool emitted = false;

  JSObject* templateObject = inspector->getTemplateObject(pc);

  MOZ_TRY(newObjectTryTemplateObject(&emitted, templateObject));
  if (emitted) {
    return Ok();
  }

  MOZ_TRY(newObjectTryVM(&emitted, templateObject));
  if (emitted) {
    return Ok();
  }

  MOZ_CRASH("newobject should have been emited");
}

AbortReasonOr<Ok> IonBuilder::jsop_initelem() {
  MOZ_ASSERT(JSOp(*pc) == JSOp::InitElem || JSOp(*pc) == JSOp::InitHiddenElem);

  MDefinition* value = current->pop();
  MDefinition* id = current->pop();
  MDefinition* obj = current->peek(-1);

  bool emitted = false;

  if (!forceInlineCaches() && JSOp(*pc) == JSOp::InitElem) {
    MOZ_TRY(initOrSetElemTryDense(&emitted, obj, id, value,
                                  /* writeHole = */ true));
    if (emitted) {
      return Ok();
    }
  }

  MOZ_TRY(initOrSetElemTryCache(&emitted, obj, id, value));
  if (emitted) {
    return Ok();
  }

  MInitElem* initElem = MInitElem::New(alloc(), obj, id, value);
  current->add(initElem);

  return resumeAfter(initElem);
}

AbortReasonOr<Ok> IonBuilder::jsop_initelem_inc() {
  MDefinition* value = current->pop();
  MDefinition* id = current->pop();
  MDefinition* obj = current->peek(-1);

  MAdd* nextId = MAdd::New(alloc(), id, constantInt(1), MIRType::Int32);
  current->add(nextId);
  current->push(nextId);

  return initArrayElement(obj, id, value);
}

AbortReasonOr<Ok> IonBuilder::initArrayElement(MDefinition* obj,
                                               MDefinition* id,
                                               MDefinition* value) {
  MOZ_ASSERT(JSOp(*pc) == JSOp::InitElemArray ||
             JSOp(*pc) == JSOp::InitElemInc);

  bool emitted = false;

  if (!forceInlineCaches()) {
    MOZ_TRY(initArrayElemTryFastPath(&emitted, obj, id, value));
    if (emitted) {
      return Ok();
    }

    MOZ_TRY(initOrSetElemTryDense(&emitted, obj, id, value,
                                  /* writeHole = */ true));
    if (emitted) {
      return Ok();
    }
  }

  MOZ_TRY(initOrSetElemTryCache(&emitted, obj, id, value));
  if (emitted) {
    return Ok();
  }

  MCallInitElementArray* initElem =
      MCallInitElementArray::New(alloc(), obj, id, value);
  current->add(initElem);

  return resumeAfter(initElem);
}

AbortReasonOr<Ok> IonBuilder::initArrayElemTryFastPath(bool* emitted,
                                                       MDefinition* obj,
                                                       MDefinition* id,
                                                       MDefinition* value) {
  MOZ_ASSERT(*emitted == false);
  MOZ_ASSERT(JSOp(*pc) == JSOp::InitElemArray ||
             JSOp(*pc) == JSOp::InitElemInc);

  // Make sure that arrays have the type being written to them by the
  // intializer, and that arrays are marked as non-packed when writing holes
  // to them during initialization.

  if (!obj->isNewArray()) {
    return Ok();
  }

  if (shouldAbortOnPreliminaryGroups(obj)) {
    return Ok();
  }

  if (!obj->resultTypeSet() || obj->resultTypeSet()->unknownObject() ||
      obj->resultTypeSet()->getObjectCount() != 1) {
    return Ok();
  }

  TypeSet::ObjectKey* initializer = obj->resultTypeSet()->getObject(0);
  if (value->type() == MIRType::MagicHole) {
    if (!initializer->hasFlags(constraints(), OBJECT_FLAG_NON_PACKED)) {
      return Ok();
    }
  } else if (!initializer->unknownProperties()) {
    HeapTypeSetKey elemTypes = initializer->property(JSID_VOID);
    if (!TypeSetIncludes(elemTypes.maybeTypes(), value->type(),
                         value->resultTypeSet())) {
      elemTypes.freeze(constraints());
      return Ok();
    }
  }

  MOZ_TRY(initArrayElementFastPath(obj->toNewArray(), id, value,
                                   /* addResumePoint = */ true));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_initelem_array() {
  MDefinition* value = current->pop();
  MDefinition* obj = current->peek(-1);

  uint32_t index = GET_UINT32(pc);
  MOZ_ASSERT(index <= INT32_MAX,
             "the bytecode emitter must fail to compile code that would "
             "produce JSOp::InitElemArray with an index exceeding "
             "int32_t range");

  MConstant* id = MConstant::New(alloc(), Int32Value(index));
  current->add(id);

  return initArrayElement(obj, id, value);
}

AbortReasonOr<Ok> IonBuilder::initArrayElementFastPath(
    MNewArray* obj, MDefinition* id, MDefinition* value,
    bool addResumePointAndIncrementInitializedLength) {
  // Get the elements vector.
  MElements* elements = MElements::New(alloc(), obj);
  current->add(elements);

  if (needsPostBarrier(value)) {
    current->add(MPostWriteBarrier::New(alloc(), obj, value));
  }

  if (obj->convertDoubleElements()) {
    MInstruction* valueDouble = MToDouble::New(alloc(), value);
    current->add(valueDouble);
    value = valueDouble;
  }

  // Store the value.
  MStoreElement* store = MStoreElement::New(alloc(), elements, id, value,
                                            /* needsHoleCheck = */ false);
  current->add(store);

  if (addResumePointAndIncrementInitializedLength) {
    // Update the initialized length. (The template object for this
    // array has the array's ultimate length, so the length field is
    // already correct: no updating needed.)
    MSetInitializedLength* initLength =
        MSetInitializedLength::New(alloc(), elements, id);
    current->add(initLength);

    MOZ_TRY(resumeAfter(initLength));
  }

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_mutateproto() {
  MDefinition* value = current->pop();
  MDefinition* obj = current->peek(-1);

  MMutateProto* mutate = MMutateProto::New(alloc(), obj, value);
  current->add(mutate);
  return resumeAfter(mutate);
}

AbortReasonOr<Ok> IonBuilder::jsop_initprop(PropertyName* name) {
  bool useFastPath = false;

  MDefinition* obj = current->peek(-2);
  if (obj->isNewObject()) {
    if (JSObject* templateObject = obj->toNewObject()->templateObject()) {
      if (templateObject->is<PlainObject>()) {
        if (templateObject->as<PlainObject>().containsPure(name)) {
          useFastPath = true;
        }
      }
    }
  }
  MInstructionReverseIterator last = current->rbegin();

  if (useFastPath && !forceInlineCaches()) {
    // This is definitely initializing an 'own' property of the object, treat
    // it as an assignment.
    MOZ_TRY(jsop_setprop(name));
  } else {
    MDefinition* value = current->pop();
    MDefinition* obj = current->pop();

    bool barrier = PropertyWriteNeedsTypeBarrier(alloc(), constraints(),
                                                 current, &obj, name, &value,
                                                 /* canModify = */ true);

    bool emitted = false;
    MOZ_TRY(setPropTryCache(&emitted, obj, name, value, barrier));
    MOZ_ASSERT(emitted == true);
  }

  // SetProp pushed the value, instead of the object. Fix this on the stack,
  // and check the most recent resume point to see if it needs updating too.
  current->pop();
  current->push(obj);
  for (MInstructionReverseIterator riter = current->rbegin(); riter != last;
       riter++) {
    if (MResumePoint* resumePoint = riter->resumePoint()) {
      MOZ_ASSERT(resumePoint->pc() == pc);
      if (resumePoint->mode() == MResumePoint::ResumeAfter) {
        size_t index = resumePoint->numOperands() - 1;
        resumePoint->replaceOperand(index, obj);
      }
      break;
    }
  }

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_initprop_getter_setter(PropertyName* name) {
  MDefinition* value = current->pop();
  MDefinition* obj = current->peek(-1);

  MInitPropGetterSetter* init =
      MInitPropGetterSetter::New(alloc(), obj, name, value);
  current->add(init);
  return resumeAfter(init);
}

AbortReasonOr<Ok> IonBuilder::jsop_initelem_getter_setter() {
  MDefinition* value = current->pop();
  MDefinition* id = current->pop();
  MDefinition* obj = current->peek(-1);

  MInitElemGetterSetter* init =
      MInitElemGetterSetter::New(alloc(), obj, id, value);
  current->add(init);
  return resumeAfter(init);
}

AbortReasonOr<MBasicBlock*> IonBuilder::newBlock(
    size_t stackDepth, jsbytecode* pc, MBasicBlock* maybePredecessor) {
  MOZ_ASSERT_IF(maybePredecessor, maybePredecessor->stackDepth() == stackDepth);

  MBasicBlock* block =
      MBasicBlock::New(graph(), stackDepth, info(), maybePredecessor,
                       bytecodeSite(pc), MBasicBlock::NORMAL);
  if (!block) {
    return abort(AbortReason::Alloc);
  }

  block->setLoopDepth(loopDepth_);
  return block;
}

AbortReasonOr<MBasicBlock*> IonBuilder::newBlock(
    MBasicBlock* predecessor, jsbytecode* pc, MResumePoint* priorResumePoint) {
  MBasicBlock* block = MBasicBlock::NewWithResumePoint(
      graph(), info(), predecessor, bytecodeSite(pc), priorResumePoint);
  if (!block) {
    return abort(AbortReason::Alloc);
  }

  block->setLoopDepth(loopDepth_);
  return block;
}

AbortReasonOr<MBasicBlock*> IonBuilder::newBlockPopN(MBasicBlock* predecessor,
                                                     jsbytecode* pc,
                                                     uint32_t popped) {
  MBasicBlock* block =
      MBasicBlock::NewPopN(graph(), info(), predecessor, bytecodeSite(pc),
                           MBasicBlock::NORMAL, popped);
  if (!block) {
    return abort(AbortReason::Alloc);
  }

  block->setLoopDepth(loopDepth_);
  return block;
}

AbortReasonOr<MBasicBlock*> IonBuilder::newBlockAfter(
    MBasicBlock* at, size_t stackDepth, jsbytecode* pc,
    MBasicBlock* maybePredecessor) {
  MOZ_ASSERT_IF(maybePredecessor, maybePredecessor->stackDepth() == stackDepth);

  MBasicBlock* block =
      MBasicBlock::New(graph(), stackDepth, info(), maybePredecessor,
                       bytecodeSite(pc), MBasicBlock::NORMAL);
  if (!block) {
    return abort(AbortReason::Alloc);
  }

  block->setLoopDepth(loopDepth_);
  block->setHitCount(0);  // osr block
  graph().insertBlockAfter(at, block);
  return block;
}

AbortReasonOr<MBasicBlock*> IonBuilder::newOsrPreheader(
    MBasicBlock* predecessor, jsbytecode* loopHead) {
  MOZ_ASSERT(JSOp(*loopHead) == JSOp::LoopHead);
  MOZ_ASSERT(loopHead == info().osrPc());

  // Create two blocks: one for the OSR entry with no predecessors, one for
  // the preheader, which has the OSR entry block as a predecessor. The
  // OSR block is always the second block (with id 1).
  MBasicBlock* osrBlock;
  MOZ_TRY_VAR(osrBlock, newBlockAfter(*graph().begin(),
                                      predecessor->stackDepth(), loopHead));
  MBasicBlock* preheader;
  MOZ_TRY_VAR(preheader, newBlock(predecessor, loopHead));

  graph().addBlock(preheader);

  // Give the pre-header the same hit count as the code before the loop.
  if (predecessor->getHitState() == MBasicBlock::HitState::Count) {
    preheader->setHitCount(predecessor->getHitCount());
  }

  MOsrEntry* entry = MOsrEntry::New(alloc());
  osrBlock->add(entry);

  // Initialize |envChain|.
  {
    uint32_t slot = info().environmentChainSlot();

    MInstruction* envv;
    if (usesEnvironmentChain()) {
      envv = MOsrEnvironmentChain::New(alloc(), entry);
    } else {
      // Use an undefined value if the script does not need its env
      // chain, to match the type that is already being tracked for the
      // slot.
      envv = MConstant::New(alloc(), UndefinedValue());
    }

    osrBlock->add(envv);
    osrBlock->initSlot(slot, envv);
  }
  // Initialize |return value|
  {
    MInstruction* returnValue;
    if (!script()->noScriptRval()) {
      returnValue = MOsrReturnValue::New(alloc(), entry);
    } else {
      returnValue = MConstant::New(alloc(), UndefinedValue());
    }
    osrBlock->add(returnValue);
    osrBlock->initSlot(info().returnValueSlot(), returnValue);
  }

  // Initialize arguments object.
  bool needsArgsObj = info().needsArgsObj();
  MInstruction* argsObj = nullptr;
  if (info().hasArguments()) {
    if (needsArgsObj) {
      argsObj = MOsrArgumentsObject::New(alloc(), entry);
    } else {
      argsObj = MConstant::New(alloc(), UndefinedValue());
    }
    osrBlock->add(argsObj);
    osrBlock->initSlot(info().argsObjSlot(), argsObj);
  }

  if (info().funMaybeLazy()) {
    // Initialize |this| parameter.
    MParameter* thisv =
        MParameter::New(alloc(), MParameter::THIS_SLOT, nullptr);
    osrBlock->add(thisv);
    osrBlock->initSlot(info().thisSlot(), thisv);

    // Initialize arguments.
    for (uint32_t i = 0; i < info().nargs(); i++) {
      uint32_t slot =
          needsArgsObj ? info().argSlotUnchecked(i) : info().argSlot(i);

      // Only grab arguments from the arguments object if the arguments object
      // aliases formals.  If the argsobj does not alias formals, then the
      // formals may have been assigned to during interpretation, and that
      // change will not be reflected in the argsobj.
      if (needsArgsObj && info().argsObjAliasesFormals()) {
        MOZ_ASSERT(argsObj && argsObj->isOsrArgumentsObject());
        // If this is an aliased formal, then the arguments object
        // contains a hole at this index.  Any references to this
        // variable in the jitcode will come from JSOp::*AliasedVar
        // opcodes, so the slot itself can be set to undefined.  If
        // it's not aliased, it must be retrieved from the arguments
        // object.
        MInstruction* osrv;
        if (script()->formalIsAliased(i)) {
          osrv = MConstant::New(alloc(), UndefinedValue());
        } else {
          osrv = MGetArgumentsObjectArg::New(alloc(), argsObj, i);
        }

        osrBlock->add(osrv);
        osrBlock->initSlot(slot, osrv);
      } else {
        MParameter* arg = MParameter::New(alloc(), i, nullptr);
        osrBlock->add(arg);
        osrBlock->initSlot(slot, arg);
      }
    }
  }

  // Initialize locals.
  for (uint32_t i = 0; i < info().nlocals(); i++) {
    uint32_t slot = info().localSlot(i);
    ptrdiff_t offset = BaselineFrame::reverseOffsetOfLocal(i);

    MOsrValue* osrv = MOsrValue::New(alloc().fallible(), entry, offset);
    if (!osrv) {
      return abort(AbortReason::Alloc);
    }
    osrBlock->add(osrv);
    osrBlock->initSlot(slot, osrv);
  }

  // Initialize stack.
  uint32_t numStackSlots = preheader->stackDepth() - info().firstStackSlot();
  for (uint32_t i = 0; i < numStackSlots; i++) {
    uint32_t slot = info().stackSlot(i);
    ptrdiff_t offset =
        BaselineFrame::reverseOffsetOfLocal(info().nlocals() + i);

    MOsrValue* osrv = MOsrValue::New(alloc().fallible(), entry, offset);
    if (!osrv) {
      return abort(AbortReason::Alloc);
    }
    osrBlock->add(osrv);
    osrBlock->initSlot(slot, osrv);
  }

  // Create an MStart to hold the first valid MResumePoint.
  MStart* start = MStart::New(alloc());
  osrBlock->add(start);

  // MOsrValue instructions are infallible, so the first MResumePoint must
  // occur after they execute, at the point of the MStart.
  MOZ_TRY(resumeAt(start, loopHead));

  // Link the same MResumePoint from the MStart to each MOsrValue.
  // This causes logic in ShouldSpecializeInput() to not replace Uses with
  // Unboxes in the MResumePiont, so that the MStart always sees Values.
  if (!osrBlock->linkOsrValues(start)) {
    return abort(AbortReason::Alloc);
  }

  // Clone types of the other predecessor of the pre-header to the osr block,
  // such as pre-header phi's won't discard specialized type of the
  // predecessor.
  MOZ_ASSERT(predecessor->stackDepth() == osrBlock->stackDepth());
  MOZ_ASSERT(info().environmentChainSlot() == 0);

  // Treat the OSR values as having the same type as the existing values
  // coming in to the loop. These will be fixed up with appropriate
  // unboxing and type barriers in finishLoop, once the possible types
  // at the loop header are known.
  for (uint32_t i = info().startArgSlot(); i < osrBlock->stackDepth(); i++) {
    MDefinition* existing = current->getSlot(i);
    MDefinition* def = osrBlock->getSlot(i);
    MOZ_ASSERT_IF(!needsArgsObj || !info().isSlotAliased(i),
                  def->type() == MIRType::Value);

    // Aliased slots are never accessed, since they need to go through
    // the callobject. No need to type them here.
    if (info().isSlotAliased(i)) {
      continue;
    }

    def->setResultType(existing->type());
    def->setResultTypeSet(existing->resultTypeSet());
  }

  // Finish the osrBlock.
  osrBlock->end(MGoto::New(alloc(), preheader));
  if (!preheader->addPredecessor(alloc(), osrBlock)) {
    return abort(AbortReason::Alloc);
  }
  graph().setOsrBlock(osrBlock);

  return preheader;
}

AbortReasonOr<MBasicBlock*> IonBuilder::newPendingLoopHeader(
    MBasicBlock* predecessor, jsbytecode* pc, bool osr) {
  MBasicBlock* block = MBasicBlock::NewPendingLoopHeader(
      graph(), info(), predecessor, bytecodeSite(pc));
  if (!block) {
    return abort(AbortReason::Alloc);
  }

  if (osr) {
    // Incorporate type information from the OSR frame into the loop
    // header. The OSR frame may have unexpected types due to type changes
    // within the loop body or due to incomplete profiling information,
    // in which case this may avoid restarts of loop analysis or bailouts
    // during the OSR itself.

    MOZ_ASSERT(info().firstLocalSlot() - info().firstArgSlot() ==
               baselineFrame_->argTypes.length());
    MOZ_ASSERT(block->stackDepth() - info().firstLocalSlot() ==
               baselineFrame_->varTypes.length());

    // Unbox the MOsrValue if it is known to be unboxable.
    for (uint32_t i = info().startArgSlot(); i < block->stackDepth(); i++) {
      // The value of aliased args and slots are in the callobject. So we can't
      // the value from the baseline frame.
      if (info().isSlotAliased(i)) {
        continue;
      }

      MPhi* phi = block->getSlot(i)->toPhi();

      // Get the type from the baseline frame.
      TypeSet::Type existingType = TypeSet::UndefinedType();
      uint32_t arg = i - info().firstArgSlot();
      uint32_t var = i - info().firstLocalSlot();
      if (info().funMaybeLazy() && i == info().thisSlot()) {
        existingType = baselineFrame_->thisType;
      } else if (arg < info().nargs()) {
        existingType = baselineFrame_->argTypes[arg];
      } else {
        existingType = baselineFrame_->varTypes[var];
      }

      if (existingType.isSingletonUnchecked()) {
        checkNurseryObject(existingType.singleton());
      }

      // Extract typeset from value.
      LifoAlloc* lifoAlloc = alloc().lifoAlloc();
      LifoAlloc::AutoFallibleScope fallibleAllocator(lifoAlloc);
      TemporaryTypeSet* typeSet =
          lifoAlloc->new_<TemporaryTypeSet>(lifoAlloc, existingType);
      if (!typeSet) {
        return abort(AbortReason::Alloc);
      }
      MIRType type = typeSet->getKnownMIRType();
      if (!phi->addBackedgeType(alloc(), type, typeSet)) {
        return abort(AbortReason::Alloc);
      }
    }
  }

  // The exception handler has code to close iterators for certain loops. We
  // need to mark the phis (and phis these iterators flow into) as having
  // implicit uses so that Ion does not optimize them away (replace with the
  // JS_OPTIMIZED_OUT MagicValue).
  // See ProcessTryNotes in vm/Interpreter.cpp.
  MOZ_ASSERT(block->stackDepth() >= info().firstStackSlot());
  bool emptyStack = block->stackDepth() == info().firstStackSlot();
  if (!emptyStack) {
    for (TryNoteIterAllNoGC tni(script(), pc); !tni.done(); ++tni) {
      const TryNote& tn = **tni;

      // Stop if we reach an outer loop because outer loops were already
      // processed when we visited their loop headers.
      if (tn.isLoop()) {
        BytecodeLocation tnStart = script()->offsetToLocation(tn.start);
        if (tnStart.toRawBytecode() != pc) {
          MOZ_ASSERT(tnStart.is(JSOp::LoopHead));
          MOZ_ASSERT(tnStart.toRawBytecode() < pc);
          break;
        }
      }

      switch (tn.kind) {
        case JSTRY_DESTRUCTURING:
        case JSTRY_FOR_IN: {
          // For for-in loops we add the iterator object to iterators_. For
          // destructuring loops we add the "done" value that's on top of the
          // stack and used in the exception handler.
          MOZ_ASSERT(tn.stackDepth >= 1);
          uint32_t slot = info().stackSlot(tn.stackDepth - 1);
          MPhi* phi = block->getSlot(slot)->toPhi();
          if (!outermostBuilder()->iterators_.append(phi)) {
            return abort(AbortReason::Alloc);
          }
          break;
        }
        default:
          break;
      }
    }
  }

  return block;
}

MTest* IonBuilder::newTest(MDefinition* ins, MBasicBlock* ifTrue,
                           MBasicBlock* ifFalse) {
  MTest* test = MTest::New(alloc(), ins, ifTrue, ifFalse);
  test->cacheOperandMightEmulateUndefined(constraints());
  return test;
}

// A resume point is a mapping of stack slots to MDefinitions. It is used to
// capture the environment such that if a guard fails, and IonMonkey needs
// to exit back to the interpreter, the interpreter state can be
// reconstructed.
//
// We capture stack state at critical points:
//   * (1) At the beginning of every basic block.
//   * (2) After every effectful operation.
//
// As long as these two properties are maintained, instructions can
// be moved, hoisted, or, eliminated without problems, and ops without side
// effects do not need to worry about capturing state at precisely the
// right point in time.
//
// Effectful instructions, of course, need to capture state after completion,
// where the interpreter will not attempt to repeat the operation. For this,
// ResumeAfter must be used. The state is attached directly to the effectful
// instruction to ensure that no intermediate instructions could be injected
// in between by a future analysis pass.
//
// During LIR construction, if an instruction can bail back to the interpreter,
// we create an LSnapshot, which uses the last known resume point to request
// register/stack assignments for every live value.
AbortReasonOr<Ok> IonBuilder::resume(MInstruction* ins, jsbytecode* pc,
                                     MResumePoint::Mode mode) {
  MOZ_ASSERT(ins->isEffectful() || !ins->isMovable());

  MResumePoint* resumePoint =
      MResumePoint::New(alloc(), ins->block(), pc, mode);
  if (!resumePoint) {
    return abort(AbortReason::Alloc);
  }
  ins->setResumePoint(resumePoint);
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::resumeAt(MInstruction* ins, jsbytecode* pc) {
  return resume(ins, pc, MResumePoint::ResumeAt);
}

AbortReasonOr<Ok> IonBuilder::resumeAfter(MInstruction* ins) {
  return resume(ins, pc, MResumePoint::ResumeAfter);
}

AbortReasonOr<Ok> IonBuilder::maybeInsertResume() {
  // Create a resume point at the current position, without an existing
  // effectful instruction. This resume point is not necessary for correct
  // behavior (see above), but is added to avoid holding any values from the
  // previous resume point which are now dead. This shortens the live ranges
  // of such values and improves register allocation.
  //
  // This optimization is not performed outside of loop bodies, where good
  // register allocation is not as critical, in order to avoid creating
  // excessive resume points.

  if (loopDepth_ == 0) {
    return Ok();
  }

  MNop* ins = MNop::New(alloc());
  current->add(ins);

  return resumeAfter(ins);
}

void IonBuilder::maybeMarkEmpty(MDefinition* ins) {
  MOZ_ASSERT(ins->type() == MIRType::Value);

  // When one of the operands has no type information, mark the output
  // as having no possible types too. This is to avoid degrading
  // subsequent analysis.
  for (size_t i = 0; i < ins->numOperands(); i++) {
    if (!ins->getOperand(i)->emptyResultTypeSet()) {
      continue;
    }

    TemporaryTypeSet* types = alloc().lifoAlloc()->new_<TemporaryTypeSet>();
    if (types) {
      ins->setResultTypeSet(types);
      return;
    }
  }
}

// Return whether property lookups can be performed effectlessly on clasp.
static bool ClassHasEffectlessLookup(const JSClass* clasp) {
  return IsTypedObjectClass(clasp) ||
         (clasp->isNative() && !clasp->getOpsLookupProperty());
}

// Return whether an object might have a property for name which is not
// accounted for by type information.
static bool ObjectHasExtraOwnProperty(CompileRealm* realm,
                                      TypeSet::ObjectKey* object, jsid id) {
  // Some typed object properties are not reflected in type information.
  if (object->isGroup() && object->group()->maybeTypeDescr()) {
    return object->group()->typeDescr().hasProperty(realm->runtime()->names(),
                                                    id);
  }

  const JSClass* clasp = object->clasp();

  // Array |length| properties are not reflected in type information.
  if (clasp == &ArrayObject::class_) {
    return JSID_IS_ATOM(id, realm->runtime()->names().length);
  }

  // Resolve hooks can install new properties on objects on demand.
  JSObject* singleton = object->isSingleton() ? object->singleton() : nullptr;
  return ClassMayResolveId(realm->runtime()->names(), clasp, id, singleton);
}

void IonBuilder::insertRecompileCheck(jsbytecode* pc) {
  MOZ_ASSERT(pc == script()->code() || JSOp(*pc) == JSOp::LoopHead);

  // No need for recompile checks if this is the highest optimization level or
  // if we're performing an analysis instead of compilation.
  OptimizationLevel curLevel = optimizationLevel();
  if (IonOptimizations.isLastLevel(curLevel) || info().isAnalysis()) {
    return;
  }

  MOZ_ASSERT(!JitOptions.disableOptimizationLevels);

  // Add recompile check. See MRecompileCheck::RecompileCheckType for how this
  // works.

  MRecompileCheck::RecompileCheckType type;
  if (JSOp(*pc) == JSOp::LoopHead) {
    type = MRecompileCheck::RecompileCheckType::OptimizationLevelOSR;
  } else if (this != outermostBuilder()) {
    type = MRecompileCheck::RecompileCheckType::OptimizationLevelInlined;
  } else {
    type = MRecompileCheck::RecompileCheckType::OptimizationLevel;
  }

  // Add recompile check to recompile when the warm-up count reaches the
  // threshold of the next optimization level.
  OptimizationLevel nextLevel = IonOptimizations.nextLevel(curLevel);
  const OptimizationInfo* info = IonOptimizations.get(nextLevel);
  uint32_t warmUpThreshold = info->recompileWarmUpThreshold(script(), pc);
  MRecompileCheck* check =
      MRecompileCheck::New(alloc(), script(), warmUpThreshold, type);
  current->add(check);
}

JSObject* IonBuilder::testSingletonProperty(JSObject* obj, jsid id) {
  // We would like to completely no-op property/global accesses which can
  // produce only a particular JSObject. When indicating the access result is
  // definitely an object, type inference does not account for the
  // possibility that the property is entirely missing from the input object
  // and its prototypes (if this happens, a semantic trigger would be hit and
  // the pushed types updated, even if there is no type barrier).
  //
  // If the access definitely goes through obj, either directly or on the
  // prototype chain, and the object has singleton type, then the type
  // information for that property reflects the value that will definitely be
  // read on accesses to the object. If the property is later deleted or
  // reconfigured as a getter/setter then the type information for the
  // property will change and trigger invalidation.

  while (obj) {
    if (!alloc().ensureBallast()) {
      return nullptr;
    }

    if (!ClassHasEffectlessLookup(obj->getClass())) {
      return nullptr;
    }

    TypeSet::ObjectKey* objKey = TypeSet::ObjectKey::get(obj);
    if (analysisContext) {
      objKey->ensureTrackedProperty(analysisContext, id);
    }

    if (objKey->unknownProperties()) {
      return nullptr;
    }

    HeapTypeSetKey property = objKey->property(id);
    if (property.isOwnProperty(constraints())) {
      if (obj->isSingleton()) {
        return property.singleton(constraints());
      }
      return nullptr;
    }

    if (ObjectHasExtraOwnProperty(realm, objKey, id)) {
      return nullptr;
    }

    obj = checkNurseryObject(obj->staticPrototype());
  }

  return nullptr;
}

JSObject* IonBuilder::testSingletonPropertyTypes(MDefinition* obj, jsid id) {
  // As for TestSingletonProperty, but the input is any value in a type set
  // rather than a specific object.

  TemporaryTypeSet* types = obj->resultTypeSet();
  if (types && types->unknownObject()) {
    return nullptr;
  }

  JSObject* objectSingleton = types ? types->maybeSingleton() : nullptr;
  if (objectSingleton) {
    return testSingletonProperty(objectSingleton, id);
  }

  MIRType objType = obj->type();
  if (objType == MIRType::Value && types) {
    objType = types->getKnownMIRType();
  }

  JSProtoKey key;
  switch (objType) {
    case MIRType::String:
      key = JSProto_String;
      break;

    case MIRType::Symbol:
      key = JSProto_Symbol;
      break;

    case MIRType::BigInt:
      key = JSProto_BigInt;
      break;

    case MIRType::Int32:
    case MIRType::Double:
      key = JSProto_Number;
      break;

    case MIRType::Boolean:
      key = JSProto_Boolean;
      break;

    case MIRType::Object: {
      if (!types) {
        return nullptr;
      }

      // For property accesses which may be on many objects, we just need to
      // find a prototype common to all the objects; if that prototype
      // has the singleton property, the access will not be on a missing
      // property.
      JSObject* singleton = nullptr;
      for (unsigned i = 0; i < types->getObjectCount(); i++) {
        TypeSet::ObjectKey* key = types->getObject(i);
        if (!key) {
          continue;
        }

        if (!alloc().ensureBallast()) {
          return nullptr;
        }

        if (analysisContext) {
          key->ensureTrackedProperty(analysisContext, id);
        }

        const JSClass* clasp = key->clasp();
        if (!ClassHasEffectlessLookup(clasp) ||
            ObjectHasExtraOwnProperty(realm, key, id)) {
          return nullptr;
        }
        if (key->unknownProperties()) {
          return nullptr;
        }
        HeapTypeSetKey property = key->property(id);
        if (property.isOwnProperty(constraints())) {
          return nullptr;
        }

        if (JSObject* proto =
                checkNurseryObject(key->proto().toObjectOrNull())) {
          // Test this type.
          JSObject* thisSingleton = testSingletonProperty(proto, id);
          if (!thisSingleton) {
            return nullptr;
          }
          if (singleton) {
            if (thisSingleton != singleton) {
              return nullptr;
            }
          } else {
            singleton = thisSingleton;
          }
        } else {
          // Can't be on the prototype chain with no prototypes...
          return nullptr;
        }
      }
      return singleton;
    }
    default:
      return nullptr;
  }

  if (JSObject* proto = script()->global().maybeGetPrototype(key)) {
    return testSingletonProperty(proto, id);
  }

  return nullptr;
}

AbortReasonOr<bool> IonBuilder::testNotDefinedProperty(
    MDefinition* obj, jsid id, bool ownProperty /* = false */) {
  TemporaryTypeSet* types = obj->resultTypeSet();
  if (!types || types->unknownObject() ||
      types->getKnownMIRType() != MIRType::Object) {
    return false;
  }

  for (unsigned i = 0, count = types->getObjectCount(); i < count; i++) {
    TypeSet::ObjectKey* key = types->getObject(i);
    if (!key) {
      continue;
    }

    while (true) {
      if (!alloc().ensureBallast()) {
        return abort(AbortReason::Alloc);
      }

      if (!key->hasStableClassAndProto(constraints()) ||
          key->unknownProperties()) {
        return false;
      }

      const JSClass* clasp = key->clasp();
      if (!ClassHasEffectlessLookup(clasp) ||
          ObjectHasExtraOwnProperty(realm, key, id)) {
        return false;
      }

      // If the object is a singleton, we can do a lookup now to avoid
      // unnecessary invalidations later on, in case the property types
      // have not yet been instantiated.
      if (key->isSingleton() && key->singleton()->is<NativeObject>() &&
          key->singleton()->as<NativeObject>().lookupPure(id)) {
        return false;
      }

      HeapTypeSetKey property = key->property(id);
      if (property.isOwnProperty(constraints())) {
        return false;
      }

      // If we only care about own properties don't check the proto.
      if (ownProperty) {
        break;
      }

      JSObject* proto = checkNurseryObject(key->proto().toObjectOrNull());
      if (!proto) {
        break;
      }
      key = TypeSet::ObjectKey::get(proto);
    }
  }

  return true;
}

AbortReasonOr<Ok> IonBuilder::pushTypeBarrier(MDefinition* def,
                                              TemporaryTypeSet* observed,
                                              BarrierKind kind) {
  MOZ_ASSERT(def == current->peek(-1));

  MDefinition* replace = addTypeBarrier(current->pop(), observed, kind);
  if (!replace) {
    return abort(AbortReason::Alloc);
  }

  current->push(replace);
  return Ok();
}

// Given an observed type set, annotates the IR as much as possible:
// (1) If no type information is provided, the given value is returned.
// (2) If a single type definitely exists, and no type barrier is needed,
//     then an infallible unbox instruction is returned.
// (3) If a type barrier is needed, but has an unknown type set, the given
//     value is returned.
// (4) Lastly, a type barrier instruction is added and returned.
MDefinition* IonBuilder::addTypeBarrier(MDefinition* def,
                                        TemporaryTypeSet* observed,
                                        BarrierKind kind,
                                        MTypeBarrier** pbarrier) {
  // Barriers are never needed for instructions whose result will not be used.
  if (BytecodeIsPopped(pc)) {
    return def;
  }

  // If the instruction has no side effects, we'll resume the entire operation.
  // The actual type barrier will occur in the interpreter. If the
  // instruction is effectful, even if it has a singleton type, there
  // must be a resume point capturing the original def, and resuming
  // to that point will explicitly monitor the new type.
  if (kind == BarrierKind::NoBarrier) {
    MDefinition* replace = ensureDefiniteType(def, observed->getKnownMIRType());
    replace->setResultTypeSet(observed);
    return replace;
  }

  if (observed->unknown()) {
    return def;
  }

  MTypeBarrier* barrier = MTypeBarrier::New(alloc(), def, observed, kind);
  current->add(barrier);

  if (pbarrier) {
    *pbarrier = barrier;
  }

  if (barrier->type() == MIRType::Undefined) {
    return constant(UndefinedValue());
  }
  if (barrier->type() == MIRType::Null) {
    return constant(NullValue());
  }

  return barrier;
}

AbortReasonOr<Ok> IonBuilder::pushDOMTypeBarrier(MInstruction* ins,
                                                 TemporaryTypeSet* observed,
                                                 JSFunction* func) {
  MOZ_ASSERT(func && func->isNative() && func->hasJitInfo());

  const JSJitInfo* jitinfo = func->jitInfo();
  bool barrier = DOMCallNeedsBarrier(jitinfo, observed);
  // Need to be a bit careful: if jitinfo->returnType is JSVAL_TYPE_DOUBLE but
  // types->getKnownMIRType() is MIRType::Int32, then don't unconditionally
  // unbox as a double.  Instead, go ahead and barrier on having an int type,
  // since we know we need a barrier anyway due to the type mismatch.  This is
  // the only situation in which TI actually has more information about the
  // JSValueType than codegen can, short of jitinfo->returnType just being
  // JSVAL_TYPE_UNKNOWN.
  MDefinition* replace = ins;
  if (jitinfo->returnType() != JSVAL_TYPE_DOUBLE ||
      observed->getKnownMIRType() != MIRType::Int32) {
    replace =
        ensureDefiniteType(ins, MIRTypeFromValueType(jitinfo->returnType()));
    if (replace != ins) {
      current->pop();
      current->push(replace);
    }
  } else {
    MOZ_ASSERT(barrier);
  }

  return pushTypeBarrier(
      replace, observed,
      barrier ? BarrierKind::TypeSet : BarrierKind::NoBarrier);
}

MDefinition* IonBuilder::ensureDefiniteType(MDefinition* def,
                                            MIRType definiteType) {
  MInstruction* replace;
  switch (definiteType) {
    case MIRType::Undefined:
      def->setImplicitlyUsedUnchecked();
      replace = MConstant::New(alloc(), UndefinedValue());
      break;

    case MIRType::Null:
      def->setImplicitlyUsedUnchecked();
      replace = MConstant::New(alloc(), NullValue());
      break;

    case MIRType::Value:
      return def;

    default: {
      if (def->type() != MIRType::Value) {
        if (def->type() == MIRType::Int32 && definiteType == MIRType::Double) {
          replace = MToDouble::New(alloc(), def);
          break;
        }
        return def;
      }
      replace = MUnbox::New(alloc(), def, definiteType, MUnbox::Infallible);
      break;
    }
  }

  current->add(replace);
  return replace;
}

static size_t NumFixedSlots(JSObject* object) {
  // Note: we can't use object->numFixedSlots() here, as this will read the
  // shape and can race with the main thread if we are building off thread.
  // The allocation kind and object class (which goes through the type) can
  // be read freely, however.
  gc::AllocKind kind = object->asTenured().getAllocKind();
  return gc::GetGCKindSlots(kind, object->getClass());
}

static bool IsUninitializedGlobalLexicalSlot(JSObject* obj,
                                             PropertyName* name) {
  LexicalEnvironmentObject& globalLexical = obj->as<LexicalEnvironmentObject>();
  MOZ_ASSERT(globalLexical.isGlobal());
  Shape* shape = globalLexical.lookupPure(name);
  if (!shape) {
    return false;
  }
  return globalLexical.getSlot(shape->slot()).isMagic(JS_UNINITIALIZED_LEXICAL);
}

AbortReasonOr<Ok> IonBuilder::getStaticName(bool* emitted,
                                            JSObject* staticObject,
                                            PropertyName* name,
                                            MDefinition* lexicalCheck) {
  MOZ_ASSERT(*emitted == false);

  jsid id = NameToId(name);

  bool isGlobalLexical =
      staticObject->is<LexicalEnvironmentObject>() &&
      staticObject->as<LexicalEnvironmentObject>().isGlobal();
  MOZ_ASSERT(isGlobalLexical || staticObject->is<GlobalObject>());
  MOZ_ASSERT(staticObject->isSingleton());

  // Always emit the lexical check. This could be optimized, but is
  // currently not for simplicity's sake.
  if (lexicalCheck) {
    return Ok();
  }

  TypeSet::ObjectKey* staticKey = TypeSet::ObjectKey::get(staticObject);
  if (analysisContext) {
    staticKey->ensureTrackedProperty(analysisContext, NameToId(name));
  }

  if (staticKey->unknownProperties()) {
    return Ok();
  }

  HeapTypeSetKey property = staticKey->property(id);
  if (!property.maybeTypes() || !property.maybeTypes()->definiteProperty() ||
      property.nonData(constraints())) {
    // The property has been reconfigured as non-configurable, non-enumerable
    // or non-writable.
    return Ok();
  }

  // Don't optimize global lexical bindings if they aren't initialized at
  // compile time.
  if (isGlobalLexical && IsUninitializedGlobalLexicalSlot(staticObject, name)) {
    return Ok();
  }

  *emitted = true;

  TemporaryTypeSet* types = bytecodeTypes(pc);
  BarrierKind barrier = PropertyReadNeedsTypeBarrier(
      analysisContext, alloc(), constraints(), staticKey, name, types,
      /* updateObserved = */ true);

  if (barrier == BarrierKind::NoBarrier) {
    // Try to inline properties holding a known constant object.
    JSObject* singleton = types->maybeSingleton();
    if (singleton) {
      if (testSingletonProperty(staticObject, id) == singleton) {
        pushConstant(ObjectValue(*singleton));
        return Ok();
      }
    }

    // Try to inline properties that have never been overwritten.
    Value constantValue;
    if (property.constant(constraints(), &constantValue)) {
      pushConstant(constantValue);
      return Ok();
    }
  }

  MOZ_TRY(loadStaticSlot(staticObject, barrier, types,
                         property.maybeTypes()->definiteSlot()));

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::loadStaticSlot(JSObject* staticObject,
                                             BarrierKind barrier,
                                             TemporaryTypeSet* types,
                                             uint32_t slot) {
  if (barrier == BarrierKind::NoBarrier) {
    // Try to inline properties that can only have one value.
    MIRType knownType = types->getKnownMIRType();
    if (knownType == MIRType::Undefined) {
      pushConstant(UndefinedValue());
      return Ok();
    }
    if (knownType == MIRType::Null) {
      pushConstant(NullValue());
      return Ok();
    }
  }

  MInstruction* obj = constant(ObjectValue(*staticObject));

  MIRType rvalType = types->getKnownMIRType();
  if (barrier != BarrierKind::NoBarrier) {
    rvalType = MIRType::Value;
  }

  return loadSlot(obj, slot, NumFixedSlots(staticObject), rvalType, barrier,
                  types);
}

// Whether a write of the given value may need a post-write barrier for GC
// purposes.
bool IonBuilder::needsPostBarrier(MDefinition* value) {
  CompileZone* zone = realm->zone();
  if (value->mightBeType(MIRType::Object)) {
    return true;
  }
  if (value->mightBeType(MIRType::String) &&
      zone->canNurseryAllocateStrings()) {
    return true;
  }
  if (value->mightBeType(MIRType::BigInt) &&
      zone->canNurseryAllocateBigInts()) {
    return true;
  }
  return false;
}

AbortReasonOr<Ok> IonBuilder::setStaticName(JSObject* staticObject,
                                            PropertyName* name) {
  jsid id = NameToId(name);

  bool isGlobalLexical =
      staticObject->is<LexicalEnvironmentObject>() &&
      staticObject->as<LexicalEnvironmentObject>().isGlobal();
  MOZ_ASSERT(isGlobalLexical || staticObject->is<GlobalObject>());

  MDefinition* value = current->peek(-1);

  TypeSet::ObjectKey* staticKey = TypeSet::ObjectKey::get(staticObject);
  if (staticKey->unknownProperties()) {
    return jsop_setprop(name);
  }

  HeapTypeSetKey property = staticKey->property(id);
  if (!property.maybeTypes() || !property.maybeTypes()->definiteProperty() ||
      property.nonData(constraints()) || property.nonWritable(constraints())) {
    // The property has been reconfigured as non-configurable, non-enumerable
    // or non-writable.
    return jsop_setprop(name);
  }

  if (!CanWriteProperty(alloc(), constraints(), property, value)) {
    return jsop_setprop(name);
  }

  // Don't optimize global lexical bindings if they aren't initialized at
  // compile time.
  if (isGlobalLexical && IsUninitializedGlobalLexicalSlot(staticObject, name)) {
    return jsop_setprop(name);
  }

  current->pop();

  // Pop the bound object on the stack. This is usually a constant but it can
  // be a phi if loops are involved, for example: x = [...arr];
  MDefinition* obj = current->pop();
  MOZ_ASSERT(obj->isConstant() || obj->isPhi());
  MOZ_ASSERT_IF(obj->isConstant(),
                &obj->toConstant()->toObject() == staticObject);

  if (needsPostBarrier(value)) {
    current->add(MPostWriteBarrier::New(alloc(), obj, value));
  }

  // If the property has a known type, we may be able to optimize typed stores
  // by not storing the type tag.
  MIRType slotType = MIRType::None;
  MIRType knownType = property.knownMIRType(constraints());
  if (knownType != MIRType::Value) {
    slotType = knownType;
  }

  bool needsPreBarrier = property.needsBarrier(constraints());
  return storeSlot(obj, property.maybeTypes()->definiteSlot(),
                   NumFixedSlots(staticObject), value, needsPreBarrier,
                   slotType);
}

JSObject* IonBuilder::testGlobalLexicalBinding(PropertyName* name) {
  MOZ_ASSERT(JSOp(*pc) == JSOp::BindGName || JSOp(*pc) == JSOp::GetGName ||
             JSOp(*pc) == JSOp::SetGName || JSOp(*pc) == JSOp::StrictSetGName);

  // The global isn't the global lexical env's prototype, but its enclosing
  // env. Test for the existence of |name| manually on the global lexical
  // env. If it is not found, look for it on the global itself.

  NativeObject* obj = &script()->global().lexicalEnvironment();
  TypeSet::ObjectKey* lexicalKey = TypeSet::ObjectKey::get(obj);
  jsid id = NameToId(name);
  if (analysisContext) {
    lexicalKey->ensureTrackedProperty(analysisContext, id);
  }

  // If the property is not found on the global lexical env but it is found
  // on the global and is configurable, try to freeze the typeset for its
  // non-existence.  If we don't have type information then fail.
  //
  // In the case that it is found on the global but is non-configurable,
  // the binding cannot be shadowed by a global lexical binding.
  Maybe<HeapTypeSetKey> lexicalProperty;
  if (!lexicalKey->unknownProperties()) {
    lexicalProperty.emplace(lexicalKey->property(id));
  }
  Shape* shape = obj->lookupPure(name);
  if (shape) {
    if ((JSOp(*pc) != JSOp::GetGName && !shape->writable()) ||
        obj->getSlot(shape->slot()).isMagic(JS_UNINITIALIZED_LEXICAL)) {
      return nullptr;
    }
  } else {
    shape = script()->global().lookupPure(name);
    if (!shape || shape->configurable()) {
      if (lexicalProperty.isSome()) {
        MOZ_ALWAYS_FALSE(lexicalProperty->isOwnProperty(constraints()));
      } else {
        return nullptr;
      }
    }
    obj = &script()->global();
  }

  return obj;
}

AbortReasonOr<Ok> IonBuilder::jsop_getgname(PropertyName* name) {
  // Optimize undefined/NaN/Infinity first.
  if (name == names().undefined) {
    pushConstant(UndefinedValue());
    return Ok();
  }
  if (name == names().NaN) {
    pushConstant(JS::NaNValue());
    return Ok();
  }
  if (name == names().Infinity) {
    pushConstant(JS::InfinityValue());
    return Ok();
  }

  if (JSObject* obj = testGlobalLexicalBinding(name)) {
    bool emitted = false;
    MOZ_TRY(getStaticName(&emitted, obj, name));
    if (emitted) {
      return Ok();
    }

    if (!forceInlineCaches() && obj->is<GlobalObject>()) {
      TemporaryTypeSet* types = bytecodeTypes(pc);
      MDefinition* globalObj = constant(ObjectValue(*obj));
      MOZ_TRY(
          getPropTryCommonGetter(&emitted, globalObj, NameToId(name), types));
      if (emitted) {
        return Ok();
      }
    }
  }

  return jsop_getname(name);
}

AbortReasonOr<Ok> IonBuilder::jsop_getname(PropertyName* name) {
  MDefinition* object;
  if (IsGlobalOp(JSOp(*pc)) && !script()->hasNonSyntacticScope()) {
    object = constant(ObjectValue(script()->global().lexicalEnvironment()));
  } else {
    object = current->environmentChain();
  }

  MGetNameCache* ins = MGetNameCache::New(alloc(), object);
  current->add(ins);
  current->push(ins);

  MOZ_TRY(resumeAfter(ins));

  TemporaryTypeSet* types = bytecodeTypes(pc);
  return pushTypeBarrier(ins, types, BarrierKind::TypeSet);
}

AbortReasonOr<Ok> IonBuilder::jsop_intrinsic(PropertyName* name) {
  TemporaryTypeSet* types = bytecodeTypes(pc);

  Value vp = UndefinedValue();
  // If the intrinsic value doesn't yet exist, we generate code to get
  // it and monitor the result.
  if (!script()->global().maybeExistingIntrinsicValue(name, &vp)) {
    MCallGetIntrinsicValue* ins = MCallGetIntrinsicValue::New(alloc(), name);

    current->add(ins);
    current->push(ins);

    MOZ_TRY(resumeAfter(ins));

    return pushTypeBarrier(ins, types, BarrierKind::TypeSet);
  }

  // Otherwise, we can bake in the intrinsic.
  pushConstant(vp);

  // Make sure that TI agrees with us on the type.
  if (!types->hasType(TypeSet::GetValueType(vp))) {
    types->addType(TypeSet::GetValueType(vp), alloc().lifoAlloc());
  }

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_getimport(PropertyName* name) {
  ModuleEnvironmentObject* env = GetModuleEnvironmentForScript(script());
  MOZ_ASSERT(env);

  Shape* shape;
  ModuleEnvironmentObject* targetEnv;
  MOZ_ALWAYS_TRUE(env->lookupImport(NameToId(name), &targetEnv, &shape));

  // We always use a type barrier because the slot's value can be modified by
  // JSOP_SETALIASEDVAR without triggering type updates. This matches
  // JSOP_GETALIASEDVAR.
  TemporaryTypeSet* types = bytecodeTypes(pc);
  BarrierKind barrier = BarrierKind::TypeSet;
  MOZ_TRY(loadStaticSlot(targetEnv, barrier, types, shape->slot()));

  // In the rare case where this import hasn't been initialized already (we
  // have an import cycle where modules reference each other's imports), emit
  // a check.
  if (targetEnv->getSlot(shape->slot()).isMagic(JS_UNINITIALIZED_LEXICAL)) {
    MDefinition* checked;
    MOZ_TRY_VAR(checked, addLexicalCheck(current->pop()));
    current->push(checked);
  }

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_bindname(PropertyName* name) {
  MDefinition* envChain;
  if (IsGlobalOp(JSOp(*pc)) && !script()->hasNonSyntacticScope()) {
    envChain = constant(ObjectValue(script()->global().lexicalEnvironment()));
  } else {
    envChain = current->environmentChain();
  }

  MBindNameCache* ins = MBindNameCache::New(alloc(), envChain);
  current->add(ins);
  current->push(ins);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_bindvar() {
  MOZ_ASSERT(usesEnvironmentChain());
  MCallBindVar* ins = MCallBindVar::New(alloc(), current->environmentChain());
  current->add(ins);
  current->push(ins);
  return Ok();
}

static MIRType GetElemKnownType(bool needsHoleCheck, TemporaryTypeSet* types) {
  MIRType knownType = types->getKnownMIRType();

  // Null and undefined have no payload so they can't be specialized.
  // Since folding null/undefined while building SSA is not safe (see the
  // comment in IsPhiObservable), we just add an untyped load instruction
  // and rely on pushTypeBarrier and DCE to replace it with a null/undefined
  // constant.
  if (knownType == MIRType::Undefined || knownType == MIRType::Null) {
    knownType = MIRType::Value;
  }

  // Different architectures may want typed element reads which require
  // hole checks to be done as either value or typed reads.
  if (needsHoleCheck && !LIRGenerator::allowTypedElementHoleCheck()) {
    knownType = MIRType::Value;
  }

  return knownType;
}

AbortReasonOr<Ok> IonBuilder::jsop_getelem() {
  MDefinition* index = current->pop();
  MDefinition* obj = current->pop();

  // Always use a call if we are performing analysis and not actually
  // emitting code, to simplify later analysis.
  if (info().isAnalysis() || shouldAbortOnPreliminaryGroups(obj)) {
    MInstruction* ins = MCallGetElement::New(alloc(), obj, index);

    current->add(ins);
    current->push(ins);

    MOZ_TRY(resumeAfter(ins));

    TemporaryTypeSet* types = bytecodeTypes(pc);
    return pushTypeBarrier(ins, types, BarrierKind::TypeSet);
  }

  bool emitted = false;

  // Handle lazy-arguments first. We have to do this even if forceInlineCaches
  // is true (lazy arguments cannot escape to the IC). Like the code in
  // IonBuilder::jsop_getprop, we only do this if we're not in analysis mode,
  // to avoid unnecessary analysis aborts.
  if (obj->mightBeType(MIRType::MagicOptimizedArguments) &&
      !info().isAnalysis()) {
    MOZ_TRY(getElemTryArguments(&emitted, obj, index));
    if (emitted) {
      return Ok();
    }

    MOZ_TRY(getElemTryArgumentsInlinedConstant(&emitted, obj, index));
    if (emitted) {
      return Ok();
    }

    MOZ_TRY(getElemTryArgumentsInlinedIndex(&emitted, obj, index));
    if (emitted) {
      return Ok();
    }

    if (script()->argumentsHasVarBinding()) {
      return abort(AbortReason::Disable,
                   "Type is not definitely lazy arguments.");
    }
  }

  obj = maybeUnboxForPropertyAccess(obj);

  if (!forceInlineCaches()) {
    // Note: no trackOptimizationAttempt call is needed, getElemTryGetProp
    // will call it.
    MOZ_TRY(getElemTryGetProp(&emitted, obj, index));
    if (emitted) {
      return Ok();
    }

    MOZ_TRY(getElemTryCallSiteObject(&emitted, obj, index));
    if (emitted) {
      return Ok();
    }

    MOZ_TRY(getElemTryDense(&emitted, obj, index));
    if (emitted) {
      return Ok();
    }

    MOZ_TRY(getElemTryTypedArray(&emitted, obj, index));
    if (emitted) {
      return Ok();
    }

    MOZ_TRY(getElemTryString(&emitted, obj, index));
    if (emitted) {
      return Ok();
    }
  }

  return getElemAddCache(obj, index);
}

AbortReasonOr<Ok> IonBuilder::getElemTryGetProp(bool* emitted, MDefinition* obj,
                                                MDefinition* index) {
  // If index is a constant string or symbol, try to optimize this GetElem
  // as a GetProp.

  MOZ_ASSERT(*emitted == false);

  MConstant* indexConst = index->maybeConstantValue();
  jsid id;
  if (!indexConst || !ValueToIdPure(indexConst->toJSValue(), &id)) {
    return Ok();
  }

  if (id != IdToTypeId(id)) {
    return Ok();
  }

  TemporaryTypeSet* types = bytecodeTypes(pc);

  MOZ_TRY(getPropTryConstant(emitted, obj, id, types));
  if (*emitted) {
    index->setImplicitlyUsedUnchecked();
    return Ok();
  }

  MOZ_TRY(getPropTryNotDefined(emitted, obj, id, types));
  if (*emitted) {
    index->setImplicitlyUsedUnchecked();
    return Ok();
  }

  MOZ_TRY(getPropTryCommonGetter(emitted, obj, id, types));
  if (*emitted) {
    index->setImplicitlyUsedUnchecked();
    return Ok();
  }

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getElemTryDense(bool* emitted, MDefinition* obj,
                                              MDefinition* index) {
  MOZ_ASSERT(*emitted == false);

  if (!ElementAccessIsDenseNative(constraints(), obj, index)) {
    return Ok();
  }

  // Don't generate a fast path if there have been bounds check failures
  // and this access might be on a sparse property.
  bool hasExtraIndexedProperty;
  MOZ_TRY_VAR(hasExtraIndexedProperty,
              ElementAccessHasExtraIndexedProperty(this, obj));
  if (hasExtraIndexedProperty && failedBoundsCheck_) {
    return Ok();
  }

  // Don't generate a fast path if this pc has seen negative
  // or floating-point indexes accessed which will not appear
  // to be extra indexed properties.
  if (inspector->hasSeenNonIntegerIndex(pc)) {
    return Ok();
  }
  if (inspector->hasSeenNegativeIndexGetElement(pc)) {
    return Ok();
  }

  MOZ_TRY(jsop_getelem_dense(obj, index));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getElemTryTypedArray(bool* emitted,
                                                   MDefinition* obj,
                                                   MDefinition* index) {
  MOZ_ASSERT(*emitted == false);

  Scalar::Type arrayType;
  if (!ElementAccessIsTypedArray(constraints(), obj, index, &arrayType)) {
    return Ok();
  }

  // Emit typed getelem variant.
  MOZ_TRY(jsop_getelem_typed(obj, index, arrayType));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getElemTryCallSiteObject(bool* emitted,
                                                       MDefinition* obj,
                                                       MDefinition* index) {
  MOZ_ASSERT(*emitted == false);

  if (!obj->isConstant() || obj->type() != MIRType::Object) {
    return Ok();
  }

  if (!index->isConstant() || index->type() != MIRType::Int32) {
    return Ok();
  }

  JSObject* cst = &obj->toConstant()->toObject();
  if (!cst->is<ArrayObject>()) {
    return Ok();
  }

  // Technically this code would work with any kind of frozen array,
  // in pratice it is usually a CallSiteObject.

  ArrayObject* array = &cst->as<ArrayObject>();
  if (array->lengthIsWritable() || array->hasEmptyElements() ||
      !array->denseElementsAreFrozen()) {
    return Ok();
  }

  int32_t idx = index->toConstant()->toInt32();
  if (idx < 0 || !array->containsDenseElement(uint32_t(idx))) {
    return Ok();
  }

  const Value& v = array->getDenseElement(uint32_t(idx));
  // Strings should have been atomized by the parser.
  if (!v.isString() || !v.toString()->isAtom()) {
    return Ok();
  }

  obj->setImplicitlyUsedUnchecked();
  index->setImplicitlyUsedUnchecked();

  pushConstant(v);

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getElemTryString(bool* emitted, MDefinition* obj,
                                               MDefinition* index) {
  MOZ_ASSERT(*emitted == false);

  if (obj->type() != MIRType::String || !IsNumberType(index->type())) {
    return Ok();
  }

  // If the index is expected to be out-of-bounds, don't optimize to avoid
  // frequent bailouts.
  if (bytecodeTypes(pc)->hasType(TypeSet::UndefinedType())) {
    return Ok();
  }

  // Emit fast path for string[index].
  MInstruction* idInt32 = MToNumberInt32::New(alloc(), index);
  current->add(idInt32);
  index = idInt32;

  MStringLength* length = MStringLength::New(alloc(), obj);
  current->add(length);

  index = addBoundsCheck(index, length);

  MCharCodeAt* charCode = MCharCodeAt::New(alloc(), obj, index);
  current->add(charCode);

  MFromCharCode* result = MFromCharCode::New(alloc(), charCode);
  current->add(result);
  current->push(result);

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getElemTryArguments(bool* emitted,
                                                  MDefinition* obj,
                                                  MDefinition* index) {
  MOZ_ASSERT(*emitted == false);

  if (inliningDepth_ > 0) {
    return Ok();
  }

  if (obj->type() != MIRType::MagicOptimizedArguments) {
    return Ok();
  }

  // Emit GetFrameArgument.

  MOZ_ASSERT(!info().argsObjAliasesFormals());

  // Type Inference has guaranteed this is an optimized arguments object.
  obj->setImplicitlyUsedUnchecked();

  // To ensure that we are not looking above the number of actual arguments.
  MArgumentsLength* length = MArgumentsLength::New(alloc());
  current->add(length);

  // Ensure index is an integer.
  MInstruction* idInt32 = MToNumberInt32::New(alloc(), index);
  current->add(idInt32);
  index = idInt32;

  // Bailouts if we read more than the number of actual arguments.
  index = addBoundsCheck(index, length);

  // Load the argument from the actual arguments.
  auto* load = MGetFrameArgument::New(alloc(), index);
  current->add(load);
  current->push(load);

  TemporaryTypeSet* types = bytecodeTypes(pc);
  MOZ_TRY(pushTypeBarrier(load, types, BarrierKind::TypeSet));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getElemTryArgumentsInlinedConstant(
    bool* emitted, MDefinition* obj, MDefinition* index) {
  MOZ_ASSERT(*emitted == false);

  if (inliningDepth_ == 0) {
    return Ok();
  }

  if (obj->type() != MIRType::MagicOptimizedArguments) {
    return Ok();
  }

  MConstant* indexConst = index->maybeConstantValue();
  if (!indexConst || indexConst->type() != MIRType::Int32) {
    return Ok();
  }

  // Emit inlined arguments.
  obj->setImplicitlyUsedUnchecked();

  MOZ_ASSERT(!info().argsObjAliasesFormals());

  // When the id is constant, we can just return the corresponding inlined
  // argument
  MOZ_ASSERT(inliningDepth_ > 0);

  int32_t id = indexConst->toInt32();
  index->setImplicitlyUsedUnchecked();

  if (id < (int32_t)inlineCallInfo_->argc() && id >= 0) {
    current->push(inlineCallInfo_->getArg(id));
  } else {
    pushConstant(UndefinedValue());
  }

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getElemTryArgumentsInlinedIndex(
    bool* emitted, MDefinition* obj, MDefinition* index) {
  MOZ_ASSERT(*emitted == false);

  if (inliningDepth_ == 0) {
    return Ok();
  }

  if (obj->type() != MIRType::MagicOptimizedArguments) {
    return Ok();
  }

  if (!IsNumberType(index->type())) {
    return Ok();
  }

  // Currently, we do not support any arguments vector larger than 10, as this
  // is being translated into code at the call site, and it would be better to
  // store the arguments contiguously on the stack.
  if (inlineCallInfo_->argc() > 10) {
    return abort(AbortReason::Disable,
                 "NYI get argument element with too many arguments");
  }

  // Emit inlined arguments.
  obj->setImplicitlyUsedUnchecked();

  MOZ_ASSERT(!info().argsObjAliasesFormals());

  // Ensure index is an integer.
  MInstruction* idInt32 = MToNumberInt32::New(alloc(), index);
  current->add(idInt32);
  index = idInt32;

  // Bailout if we read more than the number of actual arguments. This bailout
  // cannot re-enter because reading out of bounds arguments will disable the
  // lazy arguments optimization for this script, when this code would be
  // executed in Baseline. (see GetElemOptimizedArguments)
  index = addBoundsCheck(index, constantInt(inlineCallInfo_->argc()));

  // Get an instruction to represent the state of the argument vector.
  MInstruction* args =
      MArgumentState::New(alloc().fallible(), inlineCallInfo_->argv());
  if (!args) {
    return abort(AbortReason::Alloc);
  }
  current->add(args);

  // Select a value to pick from a vector.
  MInstruction* load = MLoadElementFromState::New(alloc(), args, index);
  current->add(load);
  current->push(load);

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getElemAddCache(MDefinition* obj,
                                              MDefinition* index) {
  // Emit GetPropertyCache.

  MGetPropertyCache* ins = MGetPropertyCache::New(alloc(), obj, index,
                                                  /* monitoredResult = */ true);
  current->add(ins);
  current->push(ins);

  MOZ_TRY(resumeAfter(ins));

  // We always barrier getElem to handle missing elements, as type inference
  // doesn't handle missing properties (see Bug 1488786)
  TemporaryTypeSet* types = bytecodeTypes(pc);
  MOZ_TRY(pushTypeBarrier(ins, types, BarrierKind::TypeSet));

  return Ok();
}

TemporaryTypeSet* IonBuilder::computeHeapType(const TemporaryTypeSet* objTypes,
                                              const jsid id) {
  if (objTypes->unknownObject() || objTypes->getObjectCount() == 0) {
    return nullptr;
  }

  TemporaryTypeSet* acc = nullptr;
  LifoAlloc* lifoAlloc = alloc().lifoAlloc();

  Vector<HeapTypeSetKey, 4, SystemAllocPolicy> properties;
  if (!properties.reserve(objTypes->getObjectCount())) {
    return nullptr;
  }

  for (unsigned i = 0; i < objTypes->getObjectCount(); i++) {
    TypeSet::ObjectKey* key = objTypes->getObject(i);
    if (!key) {
      continue;
    }

    if (key->unknownProperties()) {
      return nullptr;
    }

    HeapTypeSetKey property = key->property(id);
    HeapTypeSet* currentSet = property.maybeTypes();

    if (!currentSet || currentSet->unknown()) {
      return nullptr;
    }

    properties.infallibleAppend(property);

    if (acc) {
      acc = TypeSet::unionSets(acc, currentSet, lifoAlloc);
    } else {
      TemporaryTypeSet empty;
      acc = TypeSet::unionSets(&empty, currentSet, lifoAlloc);
    }

    if (!acc) {
      return nullptr;
    }
  }

  // Freeze all the properties associated with the refined type set.
  for (HeapTypeSetKey* i = properties.begin(); i != properties.end(); i++) {
    i->freeze(constraints());
  }

  return acc;
}

AbortReasonOr<Ok> IonBuilder::jsop_getelem_dense(MDefinition* obj,
                                                 MDefinition* index) {
  TemporaryTypeSet* types = bytecodeTypes(pc);

  MOZ_ASSERT(index->type() == MIRType::Int32 ||
             index->type() == MIRType::Double);

  BarrierKind barrier = PropertyReadNeedsTypeBarrier(
      analysisContext, alloc(), constraints(), obj, nullptr, types);
  bool needsHoleCheck = !ElementAccessIsPacked(constraints(), obj);

  // Reads which are on holes in the object do not have to bail out if
  // undefined values have been observed at this access site and the access
  // cannot hit another indexed property on the object or its prototypes.
  bool readOutOfBounds = false;
  if (types->hasType(TypeSet::UndefinedType())) {
    bool hasExtraIndexedProperty;
    MOZ_TRY_VAR(hasExtraIndexedProperty,
                ElementAccessHasExtraIndexedProperty(this, obj));
    readOutOfBounds = !hasExtraIndexedProperty;
  }

  MIRType knownType = MIRType::Value;
  if (barrier == BarrierKind::NoBarrier) {
    knownType = GetElemKnownType(needsHoleCheck, types);
  }

  // Ensure index is an integer.
  MInstruction* idInt32 = MToNumberInt32::New(alloc(), index);
  current->add(idInt32);
  index = idInt32;

  // Get the elements vector.
  MInstruction* elements = MElements::New(alloc(), obj);
  current->add(elements);

  // Note: to help GVN, use the original MElements instruction and not
  // MConvertElementsToDoubles as operand. This is fine because converting
  // elements to double does not change the initialized length.
  MInstruction* initLength = initializedLength(elements);

  // If we can load the element as a definite double, make sure to check that
  // the array has been converted to homogenous doubles first.
  TemporaryTypeSet* objTypes = obj->resultTypeSet();
  bool inBounds = !readOutOfBounds && !needsHoleCheck;

  if (inBounds) {
    TemporaryTypeSet* heapTypes = computeHeapType(objTypes, JSID_VOID);
    if (heapTypes && heapTypes->isSubset(types)) {
      knownType = heapTypes->getKnownMIRType();
      types = heapTypes;
    }
  }

  bool loadDouble = barrier == BarrierKind::NoBarrier && loopDepth_ &&
                    inBounds && knownType == MIRType::Double && objTypes &&
                    objTypes->convertDoubleElements(constraints()) ==
                        TemporaryTypeSet::AlwaysConvertToDoubles;
  if (loadDouble) {
    elements = addConvertElementsToDoubles(elements);
  }

  MInstruction* load;

  if (!readOutOfBounds) {
    // This load should not return undefined, so likely we're reading
    // in-bounds elements, and the array is packed or its holes are not
    // read. This is the best case: we can separate the bounds check for
    // hoisting.
    index = addBoundsCheck(index, initLength);

    load =
        MLoadElement::New(alloc(), elements, index, needsHoleCheck, loadDouble);
    current->add(load);
  } else {
    // This load may return undefined, so assume that we *can* read holes,
    // or that we can read out-of-bounds accesses. In this case, the bounds
    // check is part of the opcode.
    load = MLoadElementHole::New(alloc(), elements, index, initLength,
                                 needsHoleCheck);
    current->add(load);

    // If maybeUndefined was true, the typeset must have undefined, and
    // then either additional types or a barrier. This means we should
    // never have a typed version of LoadElementHole.
    MOZ_ASSERT(knownType == MIRType::Value);
  }

  if (knownType != MIRType::Value) {
    load->setResultType(knownType);
    load->setResultTypeSet(types);
  }

  current->push(load);
  return pushTypeBarrier(load, types, barrier);
}

MInstruction* IonBuilder::addArrayBufferByteLength(MDefinition* obj) {
  MLoadFixedSlot* ins = MLoadFixedSlot::New(
      alloc(), obj, size_t(ArrayBufferObject::BYTE_LENGTH_SLOT));
  current->add(ins);
  ins->setResultType(MIRType::Int32);
  return ins;
}

TypedArrayObject* IonBuilder::tryTypedArrayEmbedConstantElements(
    MDefinition* obj) {
  JSObject* object = nullptr;
  if (MConstant* objConst = obj->maybeConstantValue()) {
    if (objConst->type() == MIRType::Object) {
      object = &objConst->toObject();
    }
  } else if (TemporaryTypeSet* types = obj->resultTypeSet()) {
    object = types->maybeSingleton();
  }
  if (!object || !object->isSingleton()) {
    return nullptr;
  }

  TypedArrayObject* tarr = &object->as<TypedArrayObject>();

  // TypedArrays are only singletons when created with a (Shared)ArrayBuffer
  // and a length greater or equal to |SINGLETON_BYTE_LENGTH|.
  MOZ_ASSERT(tarr->hasBuffer());
  MOZ_ASSERT(tarr->byteLength() >= TypedArrayObject::SINGLETON_BYTE_LENGTH ||
             tarr->hasDetachedBuffer());

  // TypedArrays using an ArrayBuffer don't have nursery-allocated data, see
  // |ArrayBufferViewObject::init(...)|.
  MOZ_ASSERT(!tarr->runtimeFromMainThread()->gc.nursery().isInside(
      tarr->dataPointerEither()));

  // The 'data' pointer of TypedArrayObject can change in rare circumstances
  // (ArrayBufferObject::setNewData).
  TypeSet::ObjectKey* tarrKey = TypeSet::ObjectKey::get(tarr);
  if (tarrKey->unknownProperties()) {
    return nullptr;
  }
  if (!tarr->isSharedMemory()) {
    tarrKey->watchStateChangeForTypedArrayData(constraints());
  }
  return tarr;
}

void IonBuilder::addTypedArrayLengthAndData(MDefinition* obj,
                                            BoundsChecking checking,
                                            MDefinition** index,
                                            MInstruction** length,
                                            MInstruction** elements) {
  MOZ_ASSERT((index != nullptr) == (elements != nullptr));

  if (TypedArrayObject* tarr = tryTypedArrayEmbedConstantElements(obj)) {
    // Bug 979449 - Optimistically embed the elements and use TI to
    //              invalidate if we move them.

    obj->setImplicitlyUsedUnchecked();

    int32_t len = AssertedCast<int32_t>(tarr->length());
    *length = MConstant::New(alloc(), Int32Value(len));
    current->add(*length);

    if (index) {
      if (checking == DoBoundsCheck) {
        *index = addBoundsCheck(*index, *length);
      }

      *elements = MConstantElements::New(alloc(), tarr->dataPointerEither());
      current->add(*elements);
    }
    return;
  }

  *length = MTypedArrayLength::New(alloc(), obj);
  current->add(*length);

  if (index) {
    if (checking == DoBoundsCheck) {
      *index = addBoundsCheck(*index, *length);
    }

    *elements = MTypedArrayElements::New(alloc(), obj);
    current->add(*elements);
  }
}

MInstruction* IonBuilder::addTypedArrayByteOffset(MDefinition* obj) {
  MInstruction* byteOffset;
  if (TypedArrayObject* tarr = tryTypedArrayEmbedConstantElements(obj)) {
    obj->setImplicitlyUsedUnchecked();

    int32_t offset = AssertedCast<int32_t>(tarr->byteOffset());
    byteOffset = MConstant::New(alloc(), Int32Value(offset));
  } else {
    byteOffset = MTypedArrayByteOffset::New(alloc(), obj);
  }

  current->add(byteOffset);
  return byteOffset;
}

AbortReasonOr<Ok> IonBuilder::jsop_getelem_typed(MDefinition* obj,
                                                 MDefinition* index,
                                                 Scalar::Type arrayType) {
  TemporaryTypeSet* types = bytecodeTypes(pc);

  bool maybeUndefined = types->hasType(TypeSet::UndefinedType());

  // Reading from an Uint32Array will result in a double for values
  // that don't fit in an int32. We have to bailout if this happens
  // and the instruction is not known to return a double.
  bool allowDouble = types->hasType(TypeSet::DoubleType());

  if (!maybeUndefined) {
    // Assume the index is in range, so that we can hoist the length,
    // elements vector and bounds check.

    // Ensure the index is an integer. This is a stricter requirement than
    // enforcing that it is a TypedArray index via MTypedArrayIndexToInt32,
    // because any double which isn't exactly representable as an int32 will
    // lead to a bailout. But it's okay to have this stricter requirement here,
    // since any non-int32 index is equivalent to an out-of-bounds access, which
    // will lead to a bailout anyway.
    MInstruction* indexInt32 = MToNumberInt32::New(alloc(), index);
    current->add(indexInt32);
    index = indexInt32;

    // If we are reading in-bounds elements, we can use knowledge about
    // the array type to determine the result type, even if the opcode has
    // never executed. The known pushed type is only used to distinguish
    // uint32 reads that may produce either doubles or integers.
    MIRType knownType = MIRTypeForTypedArrayRead(arrayType, allowDouble);

    // Get length, bounds-check, then get elements, and add all instructions.
    MInstruction* length;
    MInstruction* elements;
    addTypedArrayLengthAndData(obj, DoBoundsCheck, &index, &length, &elements);

    // Load the element.
    MLoadUnboxedScalar* load =
        MLoadUnboxedScalar::New(alloc(), elements, index, arrayType);
    current->add(load);
    current->push(load);

    // Note: we can ignore the type barrier here, we know the type must
    // be valid and unbarriered.
    load->setResultType(knownType);
    return Ok();
  } else {
    // Ensure the index is a TypedArray index.
    auto* indexInt32 = MTypedArrayIndexToInt32::New(alloc(), index);
    current->add(indexInt32);
    index = indexInt32;

    // We need a type barrier if the array's element type has never been
    // observed (we've only read out-of-bounds values). Note that for
    // Uint32Array, we only check for int32: if allowDouble is false we
    // will bailout when we read a double.
    BarrierKind barrier = BarrierKind::TypeSet;
    switch (arrayType) {
      case Scalar::Int8:
      case Scalar::Uint8:
      case Scalar::Uint8Clamped:
      case Scalar::Int16:
      case Scalar::Uint16:
      case Scalar::Int32:
      case Scalar::Uint32:
        if (types->hasType(TypeSet::Int32Type())) {
          barrier = BarrierKind::NoBarrier;
        }
        break;
      case Scalar::Float32:
      case Scalar::Float64:
        if (allowDouble) {
          barrier = BarrierKind::NoBarrier;
        }
        break;
      default:
        MOZ_CRASH("Unknown typed array type");
    }

    // Assume we will read out-of-bound values. In this case the
    // bounds check will be part of the instruction, and the instruction
    // will always return a Value.
    MLoadTypedArrayElementHole* load = MLoadTypedArrayElementHole::New(
        alloc(), obj, index, arrayType, allowDouble);
    current->add(load);
    current->push(load);

    return pushTypeBarrier(load, types, barrier);
  }
}

AbortReasonOr<Ok> IonBuilder::jsop_setelem() {
  bool emitted = false;

  MDefinition* value = current->pop();
  MDefinition* index = current->pop();
  MDefinition* object = current->pop();

  if (shouldAbortOnPreliminaryGroups(object)) {
    MInstruction* ins =
        MCallSetElement::New(alloc(), object, index, value, IsStrictSetPC(pc));
    current->add(ins);
    current->push(value);
    return resumeAfter(ins);
  }

  if (!forceInlineCaches()) {
    MOZ_TRY(setElemTryTypedArray(&emitted, object, index, value));
    if (emitted) {
      return Ok();
    }

    SetElemICInspector icInspect(inspector->setElemICInspector(pc));
    bool writeHole = icInspect.sawOOBDenseWrite();
    MOZ_TRY(initOrSetElemTryDense(&emitted, object, index, value, writeHole));
    if (emitted) {
      return Ok();
    }

    MOZ_TRY(setElemTryArguments(&emitted, object));
    if (emitted) {
      return Ok();
    }
  }

  if (script()->argumentsHasVarBinding() &&
      object->mightBeType(MIRType::MagicOptimizedArguments) &&
      info().analysisMode() != Analysis_ArgumentsUsage) {
    return abort(AbortReason::Disable,
                 "Type is not definitely lazy arguments.");
  }

  MOZ_TRY(initOrSetElemTryCache(&emitted, object, index, value));
  if (emitted) {
    return Ok();
  }

  // Emit call.
  MInstruction* ins =
      MCallSetElement::New(alloc(), object, index, value, IsStrictSetPC(pc));
  current->add(ins);
  current->push(value);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::setElemTryTypedArray(bool* emitted,
                                                   MDefinition* object,
                                                   MDefinition* index,
                                                   MDefinition* value) {
  MOZ_ASSERT(*emitted == false);

  Scalar::Type arrayType;
  if (!ElementAccessIsTypedArray(constraints(), object, index, &arrayType)) {
    return Ok();
  }

  // Emit typed setelem variant.
  MOZ_TRY(jsop_setelem_typed(arrayType, object, index, value));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::initOrSetElemTryDense(bool* emitted,
                                                    MDefinition* object,
                                                    MDefinition* index,
                                                    MDefinition* value,
                                                    bool writeHole) {
  MOZ_ASSERT(*emitted == false);

  if (value->type() == MIRType::MagicHole) {
    return Ok();
  }

  if (!ElementAccessIsDenseNative(constraints(), object, index)) {
    return Ok();
  }

  if (PropertyWriteNeedsTypeBarrier(alloc(), constraints(), current, &object,
                                    nullptr, &value, /* canModify = */ true)) {
    return Ok();
  }

  if (!object->resultTypeSet()) {
    return Ok();
  }

  TemporaryTypeSet::DoubleConversion conversion =
      object->resultTypeSet()->convertDoubleElements(constraints());

  // If AmbiguousDoubleConversion, only handle int32 values for now.
  if (conversion == TemporaryTypeSet::AmbiguousDoubleConversion &&
      value->type() != MIRType::Int32) {
    return Ok();
  }

  // Don't generate a fast path if there have been bounds check failures
  // and this access might be on a sparse property.
  bool hasExtraIndexedProperty;
  MOZ_TRY_VAR(hasExtraIndexedProperty,
              ElementAccessHasExtraIndexedProperty(this, object));
  if (hasExtraIndexedProperty && failedBoundsCheck_) {
    return Ok();
  }

  // Emit dense setelem variant.
  return initOrSetElemDense(conversion, object, index, value, writeHole,
                            emitted);
}

AbortReasonOr<Ok> IonBuilder::setElemTryArguments(bool* emitted,
                                                  MDefinition* object) {
  MOZ_ASSERT(*emitted == false);

  if (object->type() != MIRType::MagicOptimizedArguments) {
    return Ok();
  }

  // Arguments are not supported yet.
  return abort(AbortReason::Disable, "NYI arguments[]=");
}

AbortReasonOr<Ok> IonBuilder::initOrSetElemTryCache(bool* emitted,
                                                    MDefinition* object,
                                                    MDefinition* index,
                                                    MDefinition* value) {
  MOZ_ASSERT(*emitted == false);

  if (!object->mightBeType(MIRType::Object)) {
    return Ok();
  }

  if (value->type() == MIRType::MagicHole) {
    return Ok();
  }

  bool barrier = true;
  if (index->type() == MIRType::Int32 &&
      !PropertyWriteNeedsTypeBarrier(alloc(), constraints(), current, &object,
                                     nullptr, &value, /* canModify = */ true)) {
    barrier = false;
  }

  // We can avoid worrying about holes in the IC if we know a priori we are safe
  // from them. If TI can guard that there are no indexed properties on the
  // prototype chain, we know that we aren't missing any setters by overwriting
  // the hole with another value.
  bool guardHoles;
  MOZ_TRY_VAR(guardHoles, ElementAccessHasExtraIndexedProperty(this, object));

  // Make sure the object being written to doesn't have copy on write elements.
  const JSClass* clasp =
      object->resultTypeSet()
          ? object->resultTypeSet()->getKnownClass(constraints())
          : nullptr;
  bool checkNative = !clasp || !clasp->isNative();
  object = addMaybeCopyElementsForWrite(object, checkNative);

  // Emit SetPropertyCache.
  bool strict = JSOp(*pc) == JSOp::StrictSetElem;
  MSetPropertyCache* ins =
      MSetPropertyCache::New(alloc(), object, index, value, strict,
                             needsPostBarrier(value), barrier, guardHoles);
  current->add(ins);

  // Push value back onto stack. Init ops keep their object on stack.
  if (!IsPropertyInitOp(JSOp(*pc))) {
    current->push(value);
  }

  MOZ_TRY(resumeAfter(ins));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::initOrSetElemDense(
    TemporaryTypeSet::DoubleConversion conversion, MDefinition* obj,
    MDefinition* id, MDefinition* value, bool writeHole, bool* emitted) {
  MOZ_ASSERT(*emitted == false);

  MIRType elementType = DenseNativeElementType(constraints(), obj);
  bool packed = ElementAccessIsPacked(constraints(), obj);

  // Writes which are on holes in the object do not have to bail out if they
  // cannot hit another indexed property on the object or its prototypes.
  bool hasExtraIndexedProperty;
  MOZ_TRY_VAR(hasExtraIndexedProperty,
              ElementAccessHasExtraIndexedProperty(this, obj));

  bool mayBeNonExtensible =
      ElementAccessMightBeNonExtensible(constraints(), obj);

  if (mayBeNonExtensible) {
    // FallibleStoreElement does not know how to deal with extra indexed
    // properties on the prototype. This case should be rare so we fall back
    // to an IC.
    if (hasExtraIndexedProperty) {
      return Ok();
    }

    // Don't optimize InitElem (DefineProperty) on potentially non-extensible
    // objects: when the array is sealed, we have to throw an exception.
    if (IsPropertyInitOp(JSOp(*pc))) {
      return Ok();
    }
  }

  *emitted = true;

  // Ensure id is an integer.
  MInstruction* idInt32 = MToNumberInt32::New(alloc(), id);
  current->add(idInt32);
  id = idInt32;

  if (needsPostBarrier(value)) {
    current->add(MPostWriteElementBarrier::New(alloc(), obj, value, id));
  }

  // Copy the elements vector if necessary.
  obj = addMaybeCopyElementsForWrite(obj, /* checkNative = */ false);

  // Get the elements vector.
  MElements* elements = MElements::New(alloc(), obj);
  current->add(elements);

  // Ensure the value is a double, if double conversion might be needed.
  MDefinition* newValue = value;
  switch (conversion) {
    case TemporaryTypeSet::AlwaysConvertToDoubles:
    case TemporaryTypeSet::MaybeConvertToDoubles: {
      MInstruction* valueDouble = MToDouble::New(alloc(), value);
      current->add(valueDouble);
      newValue = valueDouble;
      break;
    }

    case TemporaryTypeSet::AmbiguousDoubleConversion: {
      MOZ_ASSERT(value->type() == MIRType::Int32);
      MInstruction* maybeDouble =
          MMaybeToDoubleElement::New(alloc(), elements, value);
      current->add(maybeDouble);
      newValue = maybeDouble;
      break;
    }

    case TemporaryTypeSet::DontConvertToDoubles:
      break;

    default:
      MOZ_CRASH("Unknown double conversion");
  }

  // Use MStoreElementHole if this SetElem has written to out-of-bounds
  // indexes in the past. Otherwise, use MStoreElement so that we can hoist
  // the initialized length and bounds check.
  // If an object may have been made non-extensible, no previous expectations
  // hold and we fallback to MFallibleStoreElement.
  MInstruction* store;
  MStoreElementCommon* common = nullptr;
  if (writeHole && !hasExtraIndexedProperty && !mayBeNonExtensible) {
    MStoreElementHole* ins =
        MStoreElementHole::New(alloc(), obj, elements, id, newValue);
    store = ins;
    common = ins;

    current->add(ins);
  } else if (mayBeNonExtensible) {
    MOZ_ASSERT(
        !hasExtraIndexedProperty,
        "FallibleStoreElement codegen assumes no extra indexed properties");

    bool needsHoleCheck = !packed;
    MFallibleStoreElement* ins = MFallibleStoreElement::New(
        alloc(), obj, elements, id, newValue, needsHoleCheck);
    store = ins;
    common = ins;

    current->add(ins);
  } else {
    MInstruction* initLength = initializedLength(elements);

    id = addBoundsCheck(id, initLength);
    bool needsHoleCheck = !packed && hasExtraIndexedProperty;

    MStoreElement* ins =
        MStoreElement::New(alloc(), elements, id, newValue, needsHoleCheck);
    store = ins;
    common = ins;

    current->add(store);
  }

  // Push value back onto stack. Init ops keep their object on stack.
  if (!IsPropertyInitOp(JSOp(*pc))) {
    current->push(value);
  }

  MOZ_TRY(resumeAfter(store));

  if (common) {
    // Determine whether a write barrier is required.
    if (obj->resultTypeSet()->propertyNeedsBarrier(constraints(), JSID_VOID)) {
      common->setNeedsBarrier();
    }

    if (elementType != MIRType::None && packed) {
      common->setElementType(elementType);
    }
  }

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_setelem_typed(Scalar::Type arrayType,
                                                 MDefinition* obj,
                                                 MDefinition* id,
                                                 MDefinition* value) {
  SetElemICInspector icInspect(inspector->setElemICInspector(pc));
  bool expectOOB = icInspect.sawOOBTypedArrayWrite();

  if (expectOOB) {
    spew("Emitting OOB TypedArray SetElem");
  }

  // Ensure id is an integer. Just as in |jsop_getelem_typed|, use either
  // MTypedArrayIndexToInt32 or MToNumberInt32 depending on whether or not
  // out-of-bounds accesses are to be expected.
  MInstruction* idInt32;
  if (expectOOB) {
    idInt32 = MTypedArrayIndexToInt32::New(alloc(), id);
  } else {
    idInt32 = MToNumberInt32::New(alloc(), id);
  }
  current->add(idInt32);
  id = idInt32;

  // Get length, bounds-check, then get elements, and add all instructions.
  MInstruction* length;
  MInstruction* elements;
  BoundsChecking checking = expectOOB ? SkipBoundsCheck : DoBoundsCheck;
  addTypedArrayLengthAndData(obj, checking, &id, &length, &elements);

  // Clamp value to [0, 255] for Uint8ClampedArray.
  MDefinition* toWrite = value;
  if (arrayType == Scalar::Uint8Clamped) {
    toWrite = MClampToUint8::New(alloc(), value);
    current->add(toWrite->toInstruction());
  }

  // Store the value.
  MInstruction* ins;
  if (expectOOB) {
    ins = MStoreTypedArrayElementHole::New(alloc(), elements, length, id,
                                           toWrite, arrayType);
  } else {
    MStoreUnboxedScalar* store =
        MStoreUnboxedScalar::New(alloc(), elements, id, toWrite, arrayType,
                                 MStoreUnboxedScalar::TruncateInput);
    ins = store;
  }

  current->add(ins);
  current->push(value);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_length() {
  if (jsop_length_fastPath()) {
    return Ok();
  }

  PropertyName* name = info().getAtom(pc)->asPropertyName();
  return jsop_getprop(name);
}

bool IonBuilder::jsop_length_fastPath() {
  TemporaryTypeSet* types = bytecodeTypes(pc);

  if (types->getKnownMIRType() != MIRType::Int32) {
    return false;
  }

  MDefinition* obj = current->peek(-1);

  if (shouldAbortOnPreliminaryGroups(obj)) {
    return false;
  }

  if (obj->mightBeType(MIRType::String)) {
    if (obj->mightBeType(MIRType::Object)) {
      return false;
    }
    current->pop();
    MStringLength* ins = MStringLength::New(alloc(), obj);
    current->add(ins);
    current->push(ins);
    return true;
  }

  if (obj->mightBeType(MIRType::Object)) {
    TemporaryTypeSet* objTypes = obj->resultTypeSet();

    // Compute the length for array objects.
    if (objTypes &&
        objTypes->getKnownClass(constraints()) == &ArrayObject::class_ &&
        !objTypes->hasObjectFlags(constraints(), OBJECT_FLAG_LENGTH_OVERFLOW)) {
      current->pop();
      MElements* elements = MElements::New(alloc(), obj);
      current->add(elements);

      // Read length.
      MArrayLength* length = MArrayLength::New(alloc(), elements);
      current->add(length);
      current->push(length);
      return true;
    }
  }

  return false;
}

AbortReasonOr<Ok> IonBuilder::jsop_arguments() {
  if (info().needsArgsObj()) {
    current->push(current->argumentsObject());
    return Ok();
  }

  MOZ_ASSERT(hasLazyArguments_);
  MConstant* lazyArg =
      MConstant::New(alloc(), MagicValue(JS_OPTIMIZED_ARGUMENTS));
  current->add(lazyArg);
  current->push(lazyArg);
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_newtarget() {
  if (!info().funMaybeLazy()) {
    MOZ_ASSERT(!info().script()->isForEval());
    pushConstant(NullValue());
    return Ok();
  }

  MOZ_ASSERT(info().funMaybeLazy());

  if (info().funMaybeLazy()->isArrow()) {
    MArrowNewTarget* arrowNewTarget =
        MArrowNewTarget::New(alloc(), getCallee());
    current->add(arrowNewTarget);
    current->push(arrowNewTarget);
    return Ok();
  }

  if (inliningDepth_ == 0) {
    MNewTarget* newTarget = MNewTarget::New(alloc());
    current->add(newTarget);
    current->push(newTarget);
    return Ok();
  }

  if (!inlineCallInfo_->constructing()) {
    pushConstant(UndefinedValue());
    return Ok();
  }

  current->push(inlineCallInfo_->getNewTarget());
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_rest() {
  if (info().analysisMode() == Analysis_ArgumentsUsage) {
    // There's no BaselineScript with the template object. Just push a
    // dummy value, it does not affect the arguments analysis.
    MUnknownValue* unknown = MUnknownValue::New(alloc());
    current->add(unknown);
    current->push(unknown);
    return Ok();
  }

  ArrayObject* templateObject =
      &inspector->getTemplateObject(pc)->as<ArrayObject>();

  if (inliningDepth_ == 0) {
    // We don't know anything about the callee.
    MArgumentsLength* numActuals = MArgumentsLength::New(alloc());
    current->add(numActuals);

    // Pass in the number of actual arguments, the number of formals (not
    // including the rest parameter slot itself), and the template object.
    MRest* rest = MRest::New(alloc(), constraints(), numActuals,
                             info().nargs() - 1, templateObject);
    current->add(rest);
    current->push(rest);
    return Ok();
  }

  // We know the exact number of arguments the callee pushed.
  unsigned numActuals = inlineCallInfo_->argc();
  unsigned numFormals = info().nargs() - 1;
  unsigned numRest = numActuals > numFormals ? numActuals - numFormals : 0;

  MOZ_TRY(jsop_newarray(numRest));

  if (numRest == 0) {
    // No more updating to do. (Note that in this one case the length from
    // the template object is already correct.)
    return Ok();
  }

  MDefinition* array = current->peek(-1);
  MElements* elements = MElements::New(alloc(), array);
  current->add(elements);

  // Unroll the argument copy loop. We don't need to do any bounds or hole
  // checking here.
  MConstant* index = nullptr;
  for (unsigned i = numFormals; i < numActuals; i++) {
    if (!alloc().ensureBallast()) {
      return abort(AbortReason::Alloc);
    }

    index = MConstant::New(alloc(), Int32Value(i - numFormals));
    current->add(index);

    MDefinition* arg = inlineCallInfo_->argv()[i];
    MStoreElement* store = MStoreElement::New(alloc(), elements, index, arg,
                                              /* needsHoleCheck = */ false);
    current->add(store);

    if (needsPostBarrier(arg)) {
      current->add(MPostWriteBarrier::New(alloc(), array, arg));
    }
  }

  // The array's length is incorrectly 0 now, from the template object created
  // by BaselineCompiler::emit_Rest() before the actual argument count was
  // known. Set the correct length now that we know that count.
  MSetArrayLength* length = MSetArrayLength::New(alloc(), elements, index);
  current->add(length);

  // Update the initialized length for all the (necessarily non-hole)
  // elements added.
  MSetInitializedLength* initLength =
      MSetInitializedLength::New(alloc(), elements, index);
  current->add(initLength);

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_checkisobj(uint8_t kind) {
  MDefinition* toCheck = current->peek(-1);

  if (toCheck->type() == MIRType::Object) {
    toCheck->setImplicitlyUsedUnchecked();
    return Ok();
  }

  MCheckIsObj* check = MCheckIsObj::New(alloc(), current->pop(), kind);
  current->add(check);
  current->push(check);
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_checkiscallable(uint8_t kind) {
  MCheckIsCallable* check =
      MCheckIsCallable::New(alloc(), current->pop(), kind);
  current->add(check);
  current->push(check);
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_checkobjcoercible() {
  MDefinition* toCheck = current->peek(-1);

  if (!toCheck->mightBeType(MIRType::Undefined) &&
      !toCheck->mightBeType(MIRType::Null)) {
    toCheck->setImplicitlyUsedUnchecked();
    return Ok();
  }

  MOZ_ASSERT(toCheck->type() == MIRType::Value ||
             toCheck->type() == MIRType::Null ||
             toCheck->type() == MIRType::Undefined);

  // If we want to squeeze more perf here, we can throw without checking,
  // if IsNullOrUndefined(toCheck->type()). Since this is a failure case,
  // it should be OK.
  MCheckObjCoercible* check = MCheckObjCoercible::New(alloc(), current->pop());
  current->add(check);
  current->push(check);
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_checkclassheritage() {
  auto* ins = MCheckClassHeritage::New(alloc(), current->pop());
  current->add(ins);
  current->push(ins);
  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_checkthis() {
  auto* ins = MCheckThis::New(alloc(), current->pop());
  current->add(ins);
  current->push(ins);
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_checkthisreinit() {
  auto* ins = MCheckThisReinit::New(alloc(), current->pop());
  current->add(ins);
  current->push(ins);
  return Ok();
}

uint32_t IonBuilder::getDefiniteSlot(TemporaryTypeSet* types, jsid id,
                                     uint32_t* pnfixed) {
  if (!types || types->unknownObject() || !types->objectOrSentinel()) {
    return UINT32_MAX;
  }

  uint32_t slot = UINT32_MAX;

  for (size_t i = 0; i < types->getObjectCount(); i++) {
    TypeSet::ObjectKey* key = types->getObject(i);
    if (!key) {
      continue;
    }

    if (key->unknownProperties()) {
      return UINT32_MAX;
    }

    if (key->isSingleton()) {
      return UINT32_MAX;
    }

    HeapTypeSetKey property = key->property(id);
    if (!property.maybeTypes() || !property.maybeTypes()->definiteProperty() ||
        property.nonData(constraints())) {
      return UINT32_MAX;
    }

    // Definite slots will always be fixed slots when they are in the
    // allowable range for fixed slots.
    size_t nfixed = NativeObject::MAX_FIXED_SLOTS;

    uint32_t propertySlot = property.maybeTypes()->definiteSlot();
    if (slot == UINT32_MAX) {
      slot = propertySlot;
      *pnfixed = nfixed;
    } else if (slot != propertySlot || nfixed != *pnfixed) {
      return UINT32_MAX;
    }
  }

  return slot;
}

AbortReasonOr<Ok> IonBuilder::jsop_not() {
  MDefinition* value = current->pop();

  MNot* ins = MNot::New(alloc(), value, constraints());
  current->add(ins);
  current->push(ins);
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_envcallee() {
  uint8_t numHops = GET_UINT8(pc);
  MDefinition* env = walkEnvironmentChain(numHops);
  MInstruction* callee =
      MLoadFixedSlot::New(alloc(), env, CallObject::calleeSlot());
  current->add(callee);
  current->push(callee);
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_superbase() {
  MDefinition* callee = current->pop();

  auto* homeObject = MHomeObject::New(alloc(), callee);
  current->add(homeObject);

  auto* superBase = MHomeObjectSuperBase::New(alloc(), homeObject);
  current->add(superBase);
  current->push(superBase);

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_getprop_super(PropertyName* name) {
  MDefinition* obj = current->pop();
  MDefinition* receiver = current->pop();

  MConstant* id = constant(StringValue(name));
  auto* ins = MGetPropSuperCache::New(alloc(), obj, receiver, id);
  current->add(ins);
  current->push(ins);

  MOZ_TRY(resumeAfter(ins));

  TemporaryTypeSet* types = bytecodeTypes(pc);
  return pushTypeBarrier(ins, types, BarrierKind::TypeSet);
}

AbortReasonOr<Ok> IonBuilder::jsop_getelem_super() {
  MDefinition* obj = current->pop();
  MDefinition* id = current->pop();
  MDefinition* receiver = current->pop();

#if defined(JS_CODEGEN_X86)
  if (mirGen_.instrumentedProfiling()) {
    return abort(AbortReason::Disable,
                 "profiling functions with GETELEM_SUPER is disabled on x86");
  }
#endif

  auto* ins = MGetPropSuperCache::New(alloc(), obj, receiver, id);
  current->add(ins);
  current->push(ins);

  MOZ_TRY(resumeAfter(ins));

  TemporaryTypeSet* types = bytecodeTypes(pc);
  return pushTypeBarrier(ins, types, BarrierKind::TypeSet);
}

NativeObject* IonBuilder::commonPrototypeWithGetterSetter(
    TemporaryTypeSet* types, jsid id, bool isGetter, JSFunction* getterOrSetter,
    bool* guardGlobal) {
  // If there's a single object on the proto chain of all objects in |types|
  // that contains a property |id| with |getterOrSetter| getter or setter
  // function, return that object.

  // No sense looking if we don't know what's going on.
  if (!types || types->unknownObject()) {
    return nullptr;
  }
  *guardGlobal = false;

  NativeObject* foundProto = nullptr;
  for (unsigned i = 0; i < types->getObjectCount(); i++) {
    TypeSet::ObjectKey* key = types->getObject(i);
    if (!key) {
      continue;
    }

    while (key) {
      if (key->unknownProperties()) {
        return nullptr;
      }

      const JSClass* clasp = key->clasp();
      if (!ClassHasEffectlessLookup(clasp)) {
        return nullptr;
      }
      JSObject* singleton = key->isSingleton() ? key->singleton() : nullptr;
      if (ObjectHasExtraOwnProperty(realm, key, id)) {
        if (!singleton || !singleton->is<GlobalObject>()) {
          return nullptr;
        }
        *guardGlobal = true;
      }

      // Look for a getter/setter on the class itself which may need
      // to be called.
      if (isGetter && clasp->getOpsGetProperty()) {
        return nullptr;
      }
      if (!isGetter && clasp->getOpsSetProperty()) {
        return nullptr;
      }

      // If we have a singleton, check if it contains the getter or
      // setter we're looking for. Note that we don't need to add any
      // type constraints as the caller will add a Shape guard for the
      // holder and type constraints for other objects on the proto
      // chain.
      //
      // If the object is not a singleton, we fall back to the code below
      // and check whether the property is missing. That's fine because
      // we're looking for a getter or setter on the proto chain and
      // these objects are singletons.
      if (singleton) {
        if (!singleton->is<NativeObject>()) {
          return nullptr;
        }

        NativeObject* singletonNative = &singleton->as<NativeObject>();
        if (Shape* propShape = singletonNative->lookupPure(id)) {
          // We found a property. Check if it's the getter or setter
          // we're looking for.
          Value getterSetterVal = ObjectValue(*getterOrSetter);
          if (isGetter) {
            if (propShape->getterOrUndefined() != getterSetterVal) {
              return nullptr;
            }
          } else {
            if (propShape->setterOrUndefined() != getterSetterVal) {
              return nullptr;
            }
          }

          if (!foundProto) {
            foundProto = singletonNative;
          } else if (foundProto != singletonNative) {
            return nullptr;
          }
          break;
        }
      }

      // Test for isOwnProperty() without freezing. If we end up
      // optimizing, freezePropertiesForCommonPropFunc will freeze the
      // property type sets later on.
      HeapTypeSetKey property = key->property(id);
      if (TypeSet* types = property.maybeTypes()) {
        if (!types->empty() || types->nonDataProperty()) {
          return nullptr;
        }
      }
      if (singleton) {
        if (CanHaveEmptyPropertyTypesForOwnProperty(singleton)) {
          MOZ_ASSERT(singleton->is<GlobalObject>());
          *guardGlobal = true;
        }
      }

      JSObject* proto = checkNurseryObject(key->proto().toObjectOrNull());
      if (foundProto && proto == foundProto) {
        // We found an object on the proto chain that's known to have
        // the getter or setter property, so we can stop looking.
        break;
      }

      if (!proto) {
        // The getter or setter being searched for did not show up on
        // the object's prototype chain.
        return nullptr;
      }
      key = TypeSet::ObjectKey::get(proto);
    }
  }

  return foundProto;
}

AbortReasonOr<Ok> IonBuilder::freezePropertiesForCommonPrototype(
    TemporaryTypeSet* types, jsid id, JSObject* foundProto,
    bool allowEmptyTypesforGlobal) {
  for (unsigned i = 0; i < types->getObjectCount(); i++) {
    // If we found a Singleton object's own-property, there's nothing to
    // freeze.
    if (types->getSingleton(i) == foundProto) {
      continue;
    }

    TypeSet::ObjectKey* key = types->getObject(i);
    if (!key) {
      continue;
    }

    while (true) {
      if (!alloc().ensureBallast()) {
        return abort(AbortReason::Alloc);
      }

      HeapTypeSetKey property = key->property(id);
      MOZ_ALWAYS_TRUE(
          !property.isOwnProperty(constraints(), allowEmptyTypesforGlobal));

      // Don't mark the proto. It will be held down by the shape
      // guard. This allows us to use properties found on prototypes
      // with properties unknown to TI.
      if (key->proto() == TaggedProto(foundProto)) {
        break;
      }
      key = TypeSet::ObjectKey::get(key->proto().toObjectOrNull());
    }
  }
  return Ok();
}

AbortReasonOr<bool> IonBuilder::testCommonGetterSetter(
    TemporaryTypeSet* types, jsid id, bool isGetter, JSFunction* getterOrSetter,
    MDefinition** guard, Shape* globalShape /* = nullptr*/,
    MDefinition** globalGuard /* = nullptr */) {
  MOZ_ASSERT(getterOrSetter);
  MOZ_ASSERT_IF(globalShape, globalGuard);
  bool guardGlobal;

  // Check if all objects being accessed will lookup the name through
  // foundProto.
  NativeObject* foundProto = commonPrototypeWithGetterSetter(
      types, id, isGetter, getterOrSetter, &guardGlobal);
  if (!foundProto || (guardGlobal && !globalShape)) {
    return false;
  }

  // We can optimize the getter/setter, so freeze all involved properties to
  // ensure there isn't a lower shadowing getter or setter installed in the
  // future.
  MOZ_TRY(
      freezePropertiesForCommonPrototype(types, id, foundProto, guardGlobal));

  // Add a shape guard on the prototype we found the property on. The rest of
  // the prototype chain is guarded by TI freezes, except when name is a global
  // name. In this case, we also have to guard on the globals shape to be able
  // to optimize, because the way global property sets are handled means
  // freezing doesn't work for what we want here. Note that a shape guard is
  // good enough here, even in the proxy case, because we have ensured there
  // are no lookup hooks for this property.
  if (guardGlobal) {
    JSObject* obj = &script()->global();
    MDefinition* globalObj = constant(ObjectValue(*obj));
    *globalGuard = addShapeGuard(globalObj, globalShape, Bailout_ShapeGuard);
  }

  // If the getter/setter is not configurable we don't have to guard on the
  // proto's shape.
  Shape* propShape = foundProto->lookupPure(id);
  MOZ_ASSERT_IF(isGetter, propShape->getterObject() == getterOrSetter);
  MOZ_ASSERT_IF(!isGetter, propShape->setterObject() == getterOrSetter);
  if (propShape && !propShape->configurable()) {
    return true;
  }

  MInstruction* wrapper = constant(ObjectValue(*foundProto));
  *guard =
      addShapeGuard(wrapper, foundProto->lastProperty(), Bailout_ShapeGuard);
  return true;
}

void IonBuilder::replaceMaybeFallbackFunctionGetter(MGetPropertyCache* cache) {
  // Discard the last prior resume point of the previous MGetPropertyCache.
  WrapMGetPropertyCache rai(maybeFallbackFunctionGetter_);
  maybeFallbackFunctionGetter_ = cache;
}

AbortReasonOr<Ok> IonBuilder::annotateGetPropertyCache(
    MDefinition* obj, PropertyName* name, MGetPropertyCache* getPropCache,
    TemporaryTypeSet* objTypes, TemporaryTypeSet* pushedTypes) {
  // Ensure every pushed value is a singleton.
  if (pushedTypes->unknownObject() || pushedTypes->baseFlags() != 0) {
    return Ok();
  }

  for (unsigned i = 0; i < pushedTypes->getObjectCount(); i++) {
    if (pushedTypes->getGroup(i) != nullptr) {
      return Ok();
    }
  }

  // Object's typeset should be a proper object
  if (!objTypes || objTypes->baseFlags() || objTypes->unknownObject()) {
    return Ok();
  }

  unsigned int objCount = objTypes->getObjectCount();
  if (objCount == 0) {
    return Ok();
  }

  InlinePropertyTable* inlinePropTable =
      getPropCache->initInlinePropertyTable(alloc(), pc);
  if (!inlinePropTable) {
    return abort(AbortReason::Alloc);
  }

  // Ensure that the relevant property typeset for each group is
  // is a single-object typeset containing a JSFunction
  for (unsigned int i = 0; i < objCount; i++) {
    ObjectGroup* group = objTypes->getGroup(i);
    if (!group) {
      continue;
    }
    TypeSet::ObjectKey* key = TypeSet::ObjectKey::get(group);
    if (key->unknownProperties() || !key->proto().isObject()) {
      continue;
    }
    JSObject* proto = checkNurseryObject(key->proto().toObject());

    const JSClass* clasp = key->clasp();
    if (!ClassHasEffectlessLookup(clasp) ||
        ObjectHasExtraOwnProperty(realm, key, NameToId(name))) {
      continue;
    }

    HeapTypeSetKey ownTypes = key->property(NameToId(name));
    if (ownTypes.isOwnProperty(constraints())) {
      continue;
    }

    JSObject* singleton = testSingletonProperty(proto, NameToId(name));
    if (!singleton || !singleton->is<JSFunction>()) {
      continue;
    }

    // Don't add cases corresponding to non-observed pushes
    if (!pushedTypes->hasType(TypeSet::ObjectType(singleton))) {
      continue;
    }

    if (!inlinePropTable->addEntry(alloc(), group,
                                   &singleton->as<JSFunction>())) {
      return abort(AbortReason::Alloc);
    }
  }

  if (inlinePropTable->numEntries() == 0) {
    getPropCache->clearInlinePropertyTable();
    return Ok();
  }

#ifdef JS_JITSPEW
  if (inlinePropTable->numEntries() > 0) {
    JitSpew(JitSpew_Inlining,
            "Annotated GetPropertyCache with %d/%d inline cases",
            (int)inlinePropTable->numEntries(), (int)objCount);
  }
#endif

  // If we successfully annotated the GetPropertyCache and there are inline
  // cases, then keep a resume point of the state right before this instruction
  // for use later when we have to bail out to this point in the fallback case
  // of a PolyInlineDispatch.
  if (inlinePropTable->numEntries() > 0) {
    // Push the object back onto the stack temporarily to capture the resume
    // point.
    current->push(obj);
    MResumePoint* resumePoint =
        MResumePoint::New(alloc(), current, pc, MResumePoint::ResumeAt);
    if (!resumePoint) {
      return abort(AbortReason::Alloc);
    }
    inlinePropTable->setPriorResumePoint(resumePoint);
    replaceMaybeFallbackFunctionGetter(getPropCache);
    current->pop();
  }
  return Ok();
}

// Returns true if an idempotent cache has ever invalidated this script
// or an outer script.
bool IonBuilder::invalidatedIdempotentCache() {
  IonBuilder* builder = this;
  do {
    if (builder->script()->invalidatedIdempotentCache()) {
      return true;
    }
    builder = builder->callerBuilder_;
  } while (builder);

  return false;
}

AbortReasonOr<Ok> IonBuilder::loadSlot(MDefinition* obj, size_t slot,
                                       size_t nfixed, MIRType rvalType,
                                       BarrierKind barrier,
                                       TemporaryTypeSet* types) {
  if (slot < nfixed) {
    MLoadFixedSlot* load = MLoadFixedSlot::New(alloc(), obj, slot);
    current->add(load);
    current->push(load);

    load->setResultType(rvalType);
    return pushTypeBarrier(load, types, barrier);
  }

  MSlots* slots = MSlots::New(alloc(), obj);
  current->add(slots);

  MLoadSlot* load = MLoadSlot::New(alloc(), slots, slot - nfixed);
  current->add(load);
  current->push(load);

  load->setResultType(rvalType);
  return pushTypeBarrier(load, types, barrier);
}

AbortReasonOr<Ok> IonBuilder::loadSlot(MDefinition* obj, Shape* shape,
                                       MIRType rvalType, BarrierKind barrier,
                                       TemporaryTypeSet* types) {
  return loadSlot(obj, shape->slot(), shape->numFixedSlots(), rvalType, barrier,
                  types);
}

AbortReasonOr<Ok> IonBuilder::storeSlot(
    MDefinition* obj, size_t slot, size_t nfixed, MDefinition* value,
    bool needsBarrier, MIRType slotType /* = MIRType::None */) {
  if (slot < nfixed) {
    MStoreFixedSlot* store = MStoreFixedSlot::New(alloc(), obj, slot, value);
    current->add(store);
    current->push(value);
    if (needsBarrier) {
      store->setNeedsBarrier();
    }
    return resumeAfter(store);
  }

  MSlots* slots = MSlots::New(alloc(), obj);
  current->add(slots);

  MStoreSlot* store = MStoreSlot::New(alloc(), slots, slot - nfixed, value);
  current->add(store);
  current->push(value);
  if (needsBarrier) {
    store->setNeedsBarrier();
  }
  if (slotType != MIRType::None) {
    store->setSlotType(slotType);
  }
  return resumeAfter(store);
}

AbortReasonOr<Ok> IonBuilder::storeSlot(
    MDefinition* obj, Shape* shape, MDefinition* value, bool needsBarrier,
    MIRType slotType /* = MIRType::None */) {
  MOZ_ASSERT(shape->writable());
  return storeSlot(obj, shape->slot(), shape->numFixedSlots(), value,
                   needsBarrier, slotType);
}

bool IonBuilder::shouldAbortOnPreliminaryGroups(MDefinition* obj) {
  // Watch for groups which still have preliminary object information and
  // have not had the new script properties or unboxed layout analyses
  // performed. Normally this is done after a small number of the objects
  // have been created, but if only a few have been created we can still
  // perform the analysis with a smaller object population. The analysis can
  // have side effects so we will end up aborting compilation after building
  // finishes and retrying later.
  TemporaryTypeSet* types = obj->resultTypeSet();
  if (!types || types->unknownObject()) {
    return false;
  }

  bool preliminary = false;
  for (size_t i = 0; i < types->getObjectCount(); i++) {
    TypeSet::ObjectKey* key = types->getObject(i);
    if (!key) {
      continue;
    }

    if (ObjectGroup* group = key->maybeGroup()) {
      if (group->hasUnanalyzedPreliminaryObjects()) {
        addAbortedPreliminaryGroup(group);
        preliminary = true;
      }
    }
  }

  return preliminary;
}

MDefinition* IonBuilder::maybeUnboxForPropertyAccess(MDefinition* def) {
  if (def->type() != MIRType::Value) {
    return def;
  }

  MIRType type = inspector->expectedPropertyAccessInputType(pc);
  if (type == MIRType::Value || !def->mightBeType(type)) {
    return def;
  }

  MUnbox* unbox = MUnbox::New(alloc(), def, type, MUnbox::Fallible);
  current->add(unbox);

  // Fixup type information for a common case where a property call
  // is converted to the following bytecodes
  //
  //      a.foo()
  // ================= Compiles to ================
  //      GetLocal "a" (or similar)
  //      Dup
  //      CallProp "foo"
  //      Swap
  //      Call 0
  //
  // If we have better type information to unbox the first copy going into
  // the CallProp operation, we can replace the duplicated copy on the
  // stack as well.
  if (JSOp(*pc) == JSOp::CallProp || JSOp(*pc) == JSOp::CallElem) {
    uint32_t idx = current->stackDepth() - 1;
    MOZ_ASSERT(current->getSlot(idx) == def);
    current->setSlot(idx, unbox);
  }

  return unbox;
}

AbortReasonOr<Ok> IonBuilder::jsop_getprop(PropertyName* name) {
  bool emitted = false;

  MDefinition* obj = current->pop();
  TemporaryTypeSet* types = bytecodeTypes(pc);

  if (!info().isAnalysis()) {
    // The calls below can abort compilation, so we only try this if we're
    // not analyzing.
    // Try to optimize arguments.length.
    MOZ_TRY(getPropTryArgumentsLength(&emitted, obj));
    if (emitted) {
      return Ok();
    }

    // Try to optimize arguments.callee.
    MOZ_TRY(getPropTryArgumentsCallee(&emitted, obj, name));
    if (emitted) {
      return Ok();
    }
  }

  obj = maybeUnboxForPropertyAccess(obj);

  BarrierKind barrier = PropertyReadNeedsTypeBarrier(
      analysisContext, alloc(), constraints(), obj, name, types);

  // Try to optimize to a specific constant.
  if (barrier == BarrierKind::NoBarrier) {
    MOZ_TRY(getPropTryInferredConstant(&emitted, obj, name, types));
    if (emitted) {
      return Ok();
    }
  }

  // Always use a call if we are performing analysis and
  // not actually emitting code, to simplify later analysis. Also skip deeper
  // analysis if there are no known types for this operation, as it will
  // always invalidate when executing.
  if (info().isAnalysis() || types->empty() ||
      shouldAbortOnPreliminaryGroups(obj)) {
    MCallGetProperty* call = MCallGetProperty::New(alloc(), obj, name);
    current->add(call);

    // During the definite properties analysis we can still try to bake in
    // constants read off the prototype chain, to allow inlining later on.
    // In this case we still need the getprop call so that the later
    // analysis knows when the |this| value has been read from.
    if (info().isAnalysis()) {
      MOZ_TRY(getPropTryConstant(&emitted, obj, NameToId(name), types));
      if (emitted) {
        return Ok();
      }
    }

    current->push(call);
    MOZ_TRY(resumeAfter(call));
    return pushTypeBarrier(call, types, BarrierKind::TypeSet);
  }

  // Try to optimize accesses on outer window proxies, for example window.foo.
  // This needs to come before the various strategies getPropTryInnerize tries
  // internally, since some of those strategies will "succeed" in silly ways
  // even for an outer object.
  MOZ_TRY(getPropTryInnerize(&emitted, obj, name, types));
  if (emitted) {
    return Ok();
  }

  if (!forceInlineCaches()) {
    // Try to hardcode known constants.
    MOZ_TRY(getPropTryConstant(&emitted, obj, NameToId(name), types));
    if (emitted) {
      return Ok();
    }

    // Try to hardcode known not-defined
    MOZ_TRY(getPropTryNotDefined(&emitted, obj, NameToId(name), types));
    if (emitted) {
      return Ok();
    }

    // Try to emit loads from definite slots.
    MOZ_TRY(getPropTryDefiniteSlot(&emitted, obj, name, barrier, types));
    if (emitted) {
      return Ok();
    }

    // Try to inline a common property getter, or make a call.
    MOZ_TRY(getPropTryCommonGetter(&emitted, obj, NameToId(name), types));
    if (emitted) {
      return Ok();
    }

    // Try to emit a monomorphic/polymorphic access based on baseline caches.
    MOZ_TRY(getPropTryInlineAccess(&emitted, obj, name, barrier, types));
    if (emitted) {
      return Ok();
    }

    // Try to emit a property access on the prototype based on baseline
    // caches.
    MOZ_TRY(getPropTryInlineProtoAccess(&emitted, obj, name, types));
    if (emitted) {
      return Ok();
    }

    // Try to emit loads from a module namespace.
    MOZ_TRY(getPropTryModuleNamespace(&emitted, obj, name, barrier, types));
    if (emitted) {
      return Ok();
    }
  }

  // Emit a polymorphic cache.
  return getPropAddCache(obj, name, barrier, types);
}

AbortReasonOr<Ok> IonBuilder::improveThisTypesForCall() {
  // After a CallProp (or CallElem) for obj.prop(), the this-value and callee
  // for the call are on top of the stack:
  //
  // ... [this: obj], [callee: obj.prop]
  //
  // If obj is null or undefined, obj.prop would have thrown an exception so
  // at this point we can remove null and undefined from obj's TypeSet, to
  // improve type information for the call that will follow.

  MOZ_ASSERT(JSOp(*pc) == JSOp::CallProp || JSOp(*pc) == JSOp::CallElem);

  // Ensure |this| has types {object, null/undefined}. For simplicity don't
  // optimize if the callee is a Phi (this can happen in rare cases after
  // inlining a scripted getter).
  MDefinition* thisDef = current->peek(-2);
  MDefinition* calleeDef = current->peek(-1);
  if (thisDef->type() != MIRType::Value ||
      !thisDef->mightBeType(MIRType::Object) || !thisDef->resultTypeSet() ||
      !thisDef->resultTypeSet()->objectOrSentinel() || calleeDef->isPhi()) {
    return Ok();
  }

  // Remove null/undefined from the TypeSet.
  TemporaryTypeSet* types =
      thisDef->resultTypeSet()->cloneObjectsOnly(alloc_->lifoAlloc());
  if (!types) {
    return abort(AbortReason::Alloc);
  }

  MFilterTypeSet* filter = MFilterTypeSet::New(alloc(), thisDef, types);
  current->add(filter);
  current->rewriteAtDepth(-2, filter);

  // FilterTypeSetPolicy::adjustInputs will insert an infallible Unbox(Object)
  // for the input. Don't hoist this unbox above the getprop or getelem
  // operation.
  filter->setDependency(calleeDef);
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::checkIsDefinitelyOptimizedArguments(
    MDefinition* obj, bool* isOptimizedArgs) {
  if (obj->type() != MIRType::MagicOptimizedArguments) {
    if (script()->argumentsHasVarBinding() &&
        obj->mightBeType(MIRType::MagicOptimizedArguments)) {
      return abort(AbortReason::Disable,
                   "Type is not definitely lazy arguments.");
    }

    *isOptimizedArgs = false;
    return Ok();
  }

  *isOptimizedArgs = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getPropTryInferredConstant(
    bool* emitted, MDefinition* obj, PropertyName* name,
    TemporaryTypeSet* types) {
  MOZ_ASSERT(*emitted == false);

  // Need a result typeset to optimize.
  TemporaryTypeSet* objTypes = obj->resultTypeSet();
  if (!objTypes) {
    return Ok();
  }

  JSObject* singleton = objTypes->maybeSingleton();
  if (!singleton) {
    return Ok();
  }

  TypeSet::ObjectKey* key = TypeSet::ObjectKey::get(singleton);
  if (key->unknownProperties()) {
    return Ok();
  }

  HeapTypeSetKey property = key->property(NameToId(name));

  Value constantValue = UndefinedValue();
  if (property.constant(constraints(), &constantValue)) {
    spew("Optimized constant property");
    obj->setImplicitlyUsedUnchecked();
    pushConstant(constantValue);
    types->addType(TypeSet::GetValueType(constantValue), alloc_->lifoAlloc());
    *emitted = true;
  }

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getPropTryArgumentsLength(bool* emitted,
                                                        MDefinition* obj) {
  MOZ_ASSERT(*emitted == false);

  if (JSOp(*pc) != JSOp::Length) {
    return Ok();
  }

  bool isOptimizedArgs = false;
  MOZ_TRY(checkIsDefinitelyOptimizedArguments(obj, &isOptimizedArgs));
  if (!isOptimizedArgs) {
    return Ok();
  }

  *emitted = true;

  obj->setImplicitlyUsedUnchecked();

  // We don't know anything from the callee
  if (inliningDepth_ == 0) {
    MInstruction* ins = MArgumentsLength::New(alloc());
    current->add(ins);
    current->push(ins);
    return Ok();
  }

  // We are inlining and know the number of arguments the callee pushed
  pushConstant(Int32Value(inlineCallInfo_->argv().length()));
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getPropTryArgumentsCallee(bool* emitted,
                                                        MDefinition* obj,
                                                        PropertyName* name) {
  MOZ_ASSERT(*emitted == false);

  if (name != names().callee) {
    return Ok();
  }

  bool isOptimizedArgs = false;
  MOZ_TRY(checkIsDefinitelyOptimizedArguments(obj, &isOptimizedArgs));
  if (!isOptimizedArgs) {
    return Ok();
  }

  MOZ_ASSERT(script()->hasMappedArgsObj());

  obj->setImplicitlyUsedUnchecked();
  current->push(getCallee());

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getPropTryConstant(bool* emitted,
                                                 MDefinition* obj, jsid id,
                                                 TemporaryTypeSet* types) {
  MOZ_ASSERT(*emitted == false);

  if (!types->mightBeMIRType(MIRType::Object)) {
    // If we have not observed an object result here, don't look for a
    // singleton constant.
    return Ok();
  }

  JSObject* singleton = testSingletonPropertyTypes(obj, id);
  if (!singleton) {
    return Ok();
  }

  // Property access is a known constant -- safe to emit.
  obj->setImplicitlyUsedUnchecked();

  pushConstant(ObjectValue(*singleton));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getPropTryNotDefined(bool* emitted,
                                                   MDefinition* obj, jsid id,
                                                   TemporaryTypeSet* types) {
  MOZ_ASSERT(*emitted == false);

  if (!types->mightBeMIRType(MIRType::Undefined)) {
    // Only optimize if we expect this property access to return undefined.
    return Ok();
  }

  bool res;
  MOZ_TRY_VAR(res, testNotDefinedProperty(obj, id));
  if (!res) {
    return Ok();
  }

  obj->setImplicitlyUsedUnchecked();
  pushConstant(UndefinedValue());

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getPropTryDefiniteSlot(bool* emitted,
                                                     MDefinition* obj,
                                                     PropertyName* name,
                                                     BarrierKind barrier,
                                                     TemporaryTypeSet* types) {
  MOZ_ASSERT(*emitted == false);

  uint32_t nfixed;
  uint32_t slot =
      getDefiniteSlot(obj->resultTypeSet(), NameToId(name), &nfixed);
  if (slot == UINT32_MAX) {
    return Ok();
  }

  if (obj->type() != MIRType::Object) {
    MGuardObject* guard = MGuardObject::New(alloc(), obj);
    current->add(guard);
    obj = guard;
  }

  MInstruction* load;
  if (slot < nfixed) {
    load = MLoadFixedSlot::New(alloc(), obj, slot);
  } else {
    MInstruction* slots = MSlots::New(alloc(), obj);
    current->add(slots);

    load = MLoadSlot::New(alloc(), slots, slot - nfixed);
  }

  if (barrier == BarrierKind::NoBarrier) {
    load->setResultType(types->getKnownMIRType());
  }

  current->add(load);
  current->push(load);

  MOZ_TRY(pushTypeBarrier(load, types, barrier));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getPropTryModuleNamespace(
    bool* emitted, MDefinition* obj, PropertyName* name, BarrierKind barrier,
    TemporaryTypeSet* types) {
  MOZ_ASSERT(*emitted == false);

  TemporaryTypeSet* objTypes = obj->resultTypeSet();
  if (!objTypes) {
    return Ok();
  }

  JSObject* singleton = objTypes->maybeSingleton();
  if (!singleton) {
    return Ok();
  }

  if (!singleton->is<ModuleNamespaceObject>()) {
    return Ok();
  }

  ModuleNamespaceObject* ns = &singleton->as<ModuleNamespaceObject>();
  ModuleEnvironmentObject* env;
  Shape* shape;
  if (!ns->bindings().lookup(NameToId(name), &env, &shape)) {
    return Ok();
  }

  obj->setImplicitlyUsedUnchecked();
  MConstant* envConst = constant(ObjectValue(*env));
  uint32_t slot = shape->slot();
  uint32_t nfixed = env->numFixedSlots();

  MIRType rvalType = types->getKnownMIRType();
  if (barrier != BarrierKind::NoBarrier || IsNullOrUndefined(rvalType)) {
    rvalType = MIRType::Value;
  }

  MOZ_TRY(loadSlot(envConst, slot, nfixed, rvalType, barrier, types));

  *emitted = true;
  return Ok();
}

MDefinition* IonBuilder::addShapeGuardsForGetterSetter(
    MDefinition* obj, JSObject* holder, Shape* holderShape,
    const BaselineInspector::ReceiverVector& receivers, bool isOwnProperty) {
  MOZ_ASSERT(isOwnProperty == !holder);
  MOZ_ASSERT(holderShape);

  if (isOwnProperty) {
    MOZ_ASSERT(receivers.empty());
    return addShapeGuard(obj, holderShape, Bailout_ShapeGuard);
  }

  MDefinition* holderDef = constant(ObjectValue(*holder));
  addShapeGuard(holderDef, holderShape, Bailout_ShapeGuard);

  return addGuardReceiverPolymorphic(obj, receivers);
}

AbortReasonOr<Ok> IonBuilder::getPropTryCommonGetter(bool* emitted,
                                                     MDefinition* obj, jsid id,
                                                     TemporaryTypeSet* types,
                                                     bool innerized) {
  MOZ_ASSERT(*emitted == false);

  TemporaryTypeSet* objTypes = obj->resultTypeSet();

  JSFunction* commonGetter = nullptr;
  MDefinition* guard = nullptr;
  MDefinition* globalGuard = nullptr;

  {
    Shape* lastProperty = nullptr;
    Shape* globalShape = nullptr;
    JSObject* foundProto = nullptr;
    bool isOwnProperty = false;
    BaselineInspector::ReceiverVector receivers(alloc());
    if (inspector->commonGetPropFunction(
            pc, id, innerized, &foundProto, &lastProperty, &commonGetter,
            &globalShape, &isOwnProperty, receivers)) {
      bool canUseTIForGetter = false;
      if (!isOwnProperty) {
        // If it's not an own property, try to use TI to avoid shape guards.
        // For own properties we use the path below.
        MOZ_TRY_VAR(canUseTIForGetter,
                    testCommonGetterSetter(objTypes, id,
                                           /* isGetter = */ true, commonGetter,
                                           &guard, globalShape, &globalGuard));
      }
      if (!canUseTIForGetter) {
        // If it's an own property or type information is bad, we can still
        // optimize the getter if we shape guard.
        obj = addShapeGuardsForGetterSetter(obj, foundProto, lastProperty,
                                            receivers, isOwnProperty);
        if (!obj) {
          return abort(AbortReason::Alloc);
        }
      }
    } else if (inspector->megamorphicGetterSetterFunction(
                   pc, id, /* isGetter = */ true, &commonGetter)) {
      // Try to use TI to guard on this getter.
      bool canUseTIForGetter = false;
      MOZ_TRY_VAR(canUseTIForGetter,
                  testCommonGetterSetter(objTypes, id, /* isGetter = */ true,
                                         commonGetter, &guard));
      if (!canUseTIForGetter) {
        return Ok();
      }
    } else {
      // The Baseline IC didn't have any information we can use.
      return Ok();
    }
  }

  DOMObjectKind objKind = DOMObjectKind::Unknown;
  bool isDOM = objTypes && objTypes->isDOMClass(constraints(), &objKind);
  if (isDOM) {
    MOZ_TRY_VAR(isDOM,
                testShouldDOMCall(objTypes, commonGetter, JSJitInfo::Getter));
  }

  if (isDOM) {
    const JSJitInfo* jitinfo = commonGetter->jitInfo();
    // We don't support MGetDOMProperty/MGetDOMMember on things that might
    // be proxies when the value might be in a slot, because the
    // CodeGenerator code for LGetDOMProperty/LGetDOMMember doesn't handle
    // that case correctly.
    if (objKind == DOMObjectKind::Native ||
        (!jitinfo->isAlwaysInSlot && !jitinfo->isLazilyCachedInSlot)) {
      MInstruction* get;
      if (jitinfo->isAlwaysInSlot) {
        // If our object is a singleton and we know the property is
        // constant (which is true if and only if the get doesn't alias
        // anything), we can just read the slot here and use that
        // constant.
        JSObject* singleton = objTypes->maybeSingleton();
        if (singleton && jitinfo->aliasSet() == JSJitInfo::AliasNone) {
          size_t slot = jitinfo->slotIndex;
          *emitted = true;
          pushConstant(GetReservedSlot(singleton, slot));
          return Ok();
        }

        // We can't use MLoadFixedSlot here because it might not have
        // the right aliasing behavior; we want to alias DOM setters as
        // needed.
        get = MGetDOMMember::New(alloc(), jitinfo, obj, guard, globalGuard);
      } else {
        get = MGetDOMProperty::New(alloc(), jitinfo, objKind,
                                   commonGetter->realm(), obj, guard,
                                   globalGuard);
      }
      if (!get) {
        return abort(AbortReason::Alloc);
      }
      current->add(get);
      current->push(get);

      if (get->isEffectful()) {
        MOZ_TRY(resumeAfter(get));
      }

      MOZ_TRY(pushDOMTypeBarrier(get, types, commonGetter));

      *emitted = true;
      return Ok();
    }
  }

  // Don't call the getter with a primitive value.
  if (obj->type() != MIRType::Object) {
    MGuardObject* guardObj = MGuardObject::New(alloc(), obj);
    current->add(guardObj);
    obj = guardObj;
  }

  // Spoof stack to expected state for call.

  // Make sure there's enough room
  if (!current->ensureHasSlots(2)) {
    return abort(AbortReason::Alloc);
  }
  current->push(constant(ObjectValue(*commonGetter)));

  current->push(obj);

  CallInfo callInfo(alloc(), pc, /* constructing = */ false,
                    /* ignoresReturnValue = */ BytecodeIsPopped(pc));
  if (!callInfo.init(current, 0)) {
    return abort(AbortReason::Alloc);
  }

  if (commonGetter->isNative()) {
    InliningStatus status;
    MOZ_TRY_VAR(status, inlineNativeGetter(callInfo, commonGetter));
    switch (status) {
      case InliningStatus_WarmUpCountTooLow:
      case InliningStatus_NotInlined:
        break;
      case InliningStatus_Inlined:
        *emitted = true;
        return Ok();
    }
  }

  // Inline if we can, otherwise, forget it and just generate a call.
  if (commonGetter->isInterpreted()) {
    InliningDecision decision = makeInliningDecision(commonGetter, callInfo);
    switch (decision) {
      case InliningDecision_Error:
        return abort(AbortReason::Error);
      case InliningDecision_DontInline:
      case InliningDecision_WarmUpCountTooLow:
        break;
      case InliningDecision_Inline: {
        InliningStatus status;
        MOZ_TRY_VAR(status, inlineScriptedCall(callInfo, commonGetter));
        if (status == InliningStatus_Inlined) {
          *emitted = true;
          return Ok();
        }
        break;
      }
    }
  }

  MOZ_TRY(makeCall(commonGetter, callInfo));

  *emitted = true;
  return Ok();
}

bool IonBuilder::canInlinePropertyOpShapes(
    const BaselineInspector::ReceiverVector& receivers) {
  if (receivers.empty()) {
    return false;
  }

  for (size_t i = 0; i < receivers.length(); i++) {
    // We inline the property access as long as the shape is not in
    // dictionary mode. We cannot be sure that the shape is still a
    // lastProperty, and calling Shape::search() on dictionary mode
    // shapes that aren't lastProperty is invalid.
    if (receivers[i].getShape() && receivers[i].getShape()->inDictionary()) {
      return false;
    }
  }

  return true;
}

static Shape* PropertyShapesHaveSameSlot(
    const BaselineInspector::ReceiverVector& receivers, jsid id) {
  Shape* firstShape = nullptr;
  for (size_t i = 0; i < receivers.length(); i++) {
    if (receivers[i].getGroup()) {
      return nullptr;
    }

    Shape* shape = receivers[i].getShape()->searchLinear(id);
    MOZ_ASSERT(shape);

    if (i == 0) {
      firstShape = shape;
    } else if (shape->slot() != firstShape->slot() ||
               shape->numFixedSlots() != firstShape->numFixedSlots()) {
      return nullptr;
    }
  }

  return firstShape;
}

AbortReasonOr<Ok> IonBuilder::getPropTryInlineAccess(bool* emitted,
                                                     MDefinition* obj,
                                                     PropertyName* name,
                                                     BarrierKind barrier,
                                                     TemporaryTypeSet* types) {
  MOZ_ASSERT(*emitted == false);

  BaselineInspector::ReceiverVector receivers(alloc());
  if (!inspector->maybeInfoForPropertyOp(pc, receivers)) {
    return abort(AbortReason::Alloc);
  }

  if (!canInlinePropertyOpShapes(receivers)) {
    return Ok();
  }

  MIRType rvalType = types->getKnownMIRType();
  if (barrier != BarrierKind::NoBarrier || IsNullOrUndefined(rvalType)) {
    rvalType = MIRType::Value;
  }

  if (receivers.length() == 1) {
    if (!receivers[0].getGroup()) {
      // Monomorphic load from a native object.
      spew("Inlining monomorphic native GETPROP");

      obj = addShapeGuard(obj, receivers[0].getShape(), Bailout_ShapeGuard);

      Shape* shape = receivers[0].getShape()->searchLinear(NameToId(name));
      MOZ_ASSERT(shape);

      MOZ_TRY(loadSlot(obj, shape, rvalType, barrier, types));

      *emitted = true;
      return Ok();
    }

    return Ok();
  }

  MOZ_ASSERT(receivers.length() > 1);
  spew("Inlining polymorphic GETPROP");

  if (Shape* propShape =
          PropertyShapesHaveSameSlot(receivers, NameToId(name))) {
    obj = addGuardReceiverPolymorphic(obj, receivers);
    if (!obj) {
      return abort(AbortReason::Alloc);
    }

    MOZ_TRY(loadSlot(obj, propShape, rvalType, barrier, types));

    *emitted = true;
    return Ok();
  }

  MGetPropertyPolymorphic* load =
      MGetPropertyPolymorphic::New(alloc(), obj, name);
  current->add(load);
  current->push(load);

  for (size_t i = 0; i < receivers.length(); i++) {
    Shape* propShape = nullptr;
    if (receivers[i].getShape()) {
      propShape = receivers[i].getShape()->searchLinear(NameToId(name));
      MOZ_ASSERT(propShape);
    }
    if (!load->addReceiver(receivers[i], propShape)) {
      return abort(AbortReason::Alloc);
    }
  }

  if (failedShapeGuard_) {
    load->setNotMovable();
  }

  load->setResultType(rvalType);
  MOZ_TRY(pushTypeBarrier(load, types, barrier));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getPropTryInlineProtoAccess(
    bool* emitted, MDefinition* obj, PropertyName* name,
    TemporaryTypeSet* types) {
  MOZ_ASSERT(*emitted == false);

  BaselineInspector::ReceiverVector receivers(alloc());
  JSObject* holder = nullptr;
  if (!inspector->maybeInfoForProtoReadSlot(pc, receivers, &holder)) {
    return abort(AbortReason::Alloc);
  }

  if (!canInlinePropertyOpShapes(receivers)) {
    return Ok();
  }

  MOZ_ASSERT(holder);
  holder = checkNurseryObject(holder);

  BarrierKind barrier;
  MOZ_TRY_VAR(barrier,
              PropertyReadOnPrototypeNeedsTypeBarrier(this, obj, name, types));

  MIRType rvalType = types->getKnownMIRType();
  if (barrier != BarrierKind::NoBarrier || IsNullOrUndefined(rvalType)) {
    rvalType = MIRType::Value;
  }

  // Guard on the receiver shapes/groups.
  obj = addGuardReceiverPolymorphic(obj, receivers);
  if (!obj) {
    return abort(AbortReason::Alloc);
  }

  // Guard on the holder's shape.
  MInstruction* holderDef = constant(ObjectValue(*holder));
  Shape* holderShape = holder->as<NativeObject>().shape();
  holderDef = addShapeGuard(holderDef, holderShape, Bailout_ShapeGuard);

  Shape* propShape = holderShape->searchLinear(NameToId(name));
  MOZ_ASSERT(propShape);

  MOZ_TRY(loadSlot(holderDef, propShape, rvalType, barrier, types));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::getPropAddCache(MDefinition* obj,
                                              PropertyName* name,
                                              BarrierKind barrier,
                                              TemporaryTypeSet* types) {
  // PropertyReadNeedsTypeBarrier only accounts for object types, so for now
  // always insert a barrier if the input is not known to be an object.
  if (obj->type() != MIRType::Object) {
    barrier = BarrierKind::TypeSet;
  }

  // Since getters have no guaranteed return values, we must barrier in order to
  // be able to attach stubs for them.
  if (inspector->hasSeenAccessedGetter(pc)) {
    barrier = BarrierKind::TypeSet;
  }

  // Caches can read values from prototypes, so update the barrier to
  // reflect such possible values.
  if (barrier != BarrierKind::TypeSet) {
    BarrierKind protoBarrier;
    MOZ_TRY_VAR(protoBarrier, PropertyReadOnPrototypeNeedsTypeBarrier(
                                  this, obj, name, types));
    if (protoBarrier != BarrierKind::NoBarrier) {
      MOZ_ASSERT(barrier <= protoBarrier);
      barrier = protoBarrier;
    }
  }

  // Ensure we insert a type barrier for reads from typed objects, as type
  // information does not account for the initial undefined/null types.
  if (barrier != BarrierKind::TypeSet && !types->unknown()) {
    MOZ_ASSERT(obj->resultTypeSet());
    switch (obj->resultTypeSet()->forAllClasses(constraints(),
                                                IsTypedObjectClass)) {
      case TemporaryTypeSet::ForAllResult::ALL_FALSE:
      case TemporaryTypeSet::ForAllResult::EMPTY:
        break;
      case TemporaryTypeSet::ForAllResult::ALL_TRUE:
      case TemporaryTypeSet::ForAllResult::MIXED:
        barrier = BarrierKind::TypeSet;
        break;
    }
  }

  MConstant* id = constant(StringValue(name));
  MGetPropertyCache* load =
      MGetPropertyCache::New(alloc(), obj, id, barrier == BarrierKind::TypeSet);

  // Try to mark the cache as idempotent.
  if (obj->type() == MIRType::Object && !invalidatedIdempotentCache()) {
    if (PropertyReadIsIdempotent(constraints(), obj, name)) {
      load->setIdempotent();
    }
  }

  // When we are in the context of making a call from the value returned from
  // a property, we query the typeObject for the given property name to fill
  // the InlinePropertyTable of the GetPropertyCache.  This information is
  // then used in inlineCallsite and inlineCalls, if the "this" definition is
  // matching the "object" definition of the GetPropertyCache (see
  // CanInlineGetPropertyCache).
  //
  // If this GetPropertyCache is idempotent, then we can dispatch to the right
  // function only by checking the typed object, instead of querying the value
  // of the property.  Thus this GetPropertyCache can be moved into the
  // fallback path (see inlineObjectGroupFallback).  Otherwise, we always have
  // to do the GetPropertyCache, and we can dispatch based on the JSFunction
  // value.
  if (JSOp(*pc) == JSOp::CallProp && load->idempotent()) {
    MOZ_TRY(
        annotateGetPropertyCache(obj, name, load, obj->resultTypeSet(), types));
  }

  current->add(load);
  current->push(load);

  if (load->isEffectful()) {
    MOZ_TRY(resumeAfter(load));
  }

  MIRType rvalType = types->getKnownMIRType();
  if (barrier != BarrierKind::NoBarrier) {
    rvalType = MIRType::Value;
  } else {
    load->setResultTypeSet(types);
    if (IsNullOrUndefined(rvalType)) {
      rvalType = MIRType::Value;
    }
  }
  load->setResultType(rvalType);

  if (JSOp(*pc) != JSOp::CallProp || !IsNullOrUndefined(obj->type())) {
    // Due to inlining, it's possible the observed TypeSet is non-empty,
    // even though we know |obj| is null/undefined and the MCallGetProperty
    // will throw. Don't push a TypeBarrier in this case, to avoid
    // inlining the following (unreachable) JSOp::Call.
    MOZ_TRY(pushTypeBarrier(load, types, barrier));
  }

  return Ok();
}

MDefinition* IonBuilder::tryInnerizeWindow(MDefinition* obj) {
  // Try to optimize accesses on outer window proxies (window.foo, for
  // example) to go directly to the inner window, the global.
  //
  // Callers should be careful not to pass the global object to getters or
  // setters that require the WindowProxy.

  if (obj->type() != MIRType::Object) {
    return obj;
  }

  TemporaryTypeSet* types = obj->resultTypeSet();
  if (!types) {
    return obj;
  }

  JSObject* singleton = types->maybeSingleton();
  if (!singleton) {
    return obj;
  }

  if (!IsWindowProxyForScriptGlobal(script(), singleton)) {
    return obj;
  }

  // When we navigate, the WindowProxy is brain transplanted and we'll mark
  // its ObjectGroup as having unknown properties. The type constraint we add
  // here will invalidate JIT code when this happens.
  TypeSet::ObjectKey* key = TypeSet::ObjectKey::get(singleton);
  if (key->hasFlags(constraints(), OBJECT_FLAG_UNKNOWN_PROPERTIES)) {
    return obj;
  }

  obj->setImplicitlyUsedUnchecked();
  return constant(ObjectValue(script()->global()));
}

AbortReasonOr<Ok> IonBuilder::getPropTryInnerize(bool* emitted,
                                                 MDefinition* obj,
                                                 PropertyName* name,
                                                 TemporaryTypeSet* types) {
  // See the comment in tryInnerizeWindow for how this works.

  // Note that it's important that we do this _before_ we'd try to
  // do the optimizations below on obj normally, since some of those
  // optimizations have fallback paths that are slower than the path
  // we'd produce here.

  MOZ_ASSERT(*emitted == false);

  MDefinition* inner = tryInnerizeWindow(obj);
  if (inner == obj) {
    return Ok();
  }

  if (!forceInlineCaches()) {
    MOZ_TRY(getPropTryConstant(emitted, inner, NameToId(name), types));
    if (*emitted) {
      return Ok();
    }
    MOZ_TRY(getStaticName(emitted, &script()->global(), name));
    if (*emitted) {
      return Ok();
    }
    MOZ_TRY(getPropTryCommonGetter(emitted, inner, NameToId(name), types,
                                   /* innerized = */ true));
    if (*emitted) {
      return Ok();
    }
  }

  // Passing the inner object to GetProperty IC is safe, see the
  // needsOuterizedThisObject check in IsCacheableGetPropCallNative.
  BarrierKind barrier = PropertyReadNeedsTypeBarrier(
      analysisContext, alloc(), constraints(), inner, name, types);
  MOZ_TRY(getPropAddCache(inner, name, barrier, types));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_setprop(PropertyName* name) {
  MDefinition* value = current->pop();
  MDefinition* obj = current->pop();

  bool emitted = false;

  // Always use a call if we are doing the definite properties analysis and
  // not actually emitting code, to simplify later analysis.
  if (info().isAnalysis() || shouldAbortOnPreliminaryGroups(obj)) {
    bool strict = IsStrictSetPC(pc);
    MInstruction* ins =
        MCallSetProperty::New(alloc(), obj, value, name, strict);
    current->add(ins);
    current->push(value);
    return resumeAfter(ins);
  }

  if (!forceInlineCaches()) {
    // Try to inline a common property setter, or make a call.
    MOZ_TRY(setPropTryCommonSetter(&emitted, obj, name, value));
    if (emitted) {
      return Ok();
    }
  }

  TemporaryTypeSet* objTypes = obj->resultTypeSet();
  bool barrier = PropertyWriteNeedsTypeBarrier(alloc(), constraints(), current,
                                               &obj, name, &value,
                                               /* canModify = */ true);

  if (!forceInlineCaches()) {
    // Try to emit store from definite slots.
    MOZ_TRY(setPropTryDefiniteSlot(&emitted, obj, name, value, barrier));
    if (emitted) {
      return Ok();
    }

    // Try to emit a monomorphic/polymorphic store based on baseline caches.
    MOZ_TRY(
        setPropTryInlineAccess(&emitted, obj, name, value, barrier, objTypes));
    if (emitted) {
      return Ok();
    }
  }

  // Emit a polymorphic cache.
  MOZ_TRY(setPropTryCache(&emitted, obj, name, value, barrier));
  MOZ_ASSERT(emitted == true);
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::setPropTryCommonSetter(bool* emitted,
                                                     MDefinition* obj,
                                                     PropertyName* name,
                                                     MDefinition* value) {
  MOZ_ASSERT(*emitted == false);

  TemporaryTypeSet* objTypes = obj->resultTypeSet();
  JSFunction* commonSetter = nullptr;
  MDefinition* guard = nullptr;

  {
    Shape* lastProperty = nullptr;
    JSObject* foundProto = nullptr;
    bool isOwnProperty;
    BaselineInspector::ReceiverVector receivers(alloc());
    if (inspector->commonSetPropFunction(pc, &foundProto, &lastProperty,
                                         &commonSetter, &isOwnProperty,
                                         receivers)) {
      bool canUseTIForSetter = false;
      if (!isOwnProperty) {
        // If it's not an own property, try to use TI to avoid shape guards.
        // For own properties we use the path below.
        MOZ_TRY_VAR(canUseTIForSetter,
                    testCommonGetterSetter(objTypes, NameToId(name),
                                           /* isGetter = */ false, commonSetter,
                                           &guard));
      }
      if (!canUseTIForSetter) {
        // If it's an own property or type information is bad, we can still
        // optimize the setter if we shape guard.
        obj = addShapeGuardsForGetterSetter(obj, foundProto, lastProperty,
                                            receivers, isOwnProperty);
        if (!obj) {
          return abort(AbortReason::Alloc);
        }
      }
    } else if (inspector->megamorphicGetterSetterFunction(
                   pc, NameToId(name), /* isGetter = */ false, &commonSetter)) {
      // Try to use TI to guard on this setter.
      bool canUseTIForSetter = false;
      MOZ_TRY_VAR(
          canUseTIForSetter,
          testCommonGetterSetter(objTypes, NameToId(name),
                                 /* isGetter = */ false, commonSetter, &guard));
      if (!canUseTIForSetter) {
        return Ok();
      }
    } else {
      // The Baseline IC didn't have any information we can use.
      return Ok();
    }
  }

  // Emit common setter.

  // Setters can be called even if the property write needs a type
  // barrier, as calling the setter does not actually write any data
  // properties.

  // Try emitting dom call.
  MOZ_TRY(
      setPropTryCommonDOMSetter(emitted, obj, value, commonSetter, objTypes));
  if (*emitted) {
    return Ok();
  }

  // Don't call the setter with a primitive value.
  if (obj->type() != MIRType::Object) {
    MGuardObject* guardObj = MGuardObject::New(alloc(), obj);
    current->add(guardObj);
    obj = guardObj;
  }

  // Dummy up the stack, as in getprop. We are pushing an extra value, so
  // ensure there is enough space.
  if (!current->ensureHasSlots(3)) {
    return abort(AbortReason::Alloc);
  }

  current->push(constant(ObjectValue(*commonSetter)));
  current->push(obj);
  current->push(value);

  // Call the setter. Note that we have to push the original value, not
  // the setter's return value.
  CallInfo callInfo(alloc(), pc, /* constructing = */ false,
                    /* ignoresReturnValue = */ BytecodeIsPopped(pc));
  if (!callInfo.init(current, 1)) {
    return abort(AbortReason::Alloc);
  }

  // Ensure that we know we are calling a setter in case we inline it.
  callInfo.markAsSetter();

  // Inline the setter if we can.
  if (commonSetter->isInterpreted()) {
    InliningDecision decision = makeInliningDecision(commonSetter, callInfo);
    switch (decision) {
      case InliningDecision_Error:
        return abort(AbortReason::Error);
      case InliningDecision_DontInline:
      case InliningDecision_WarmUpCountTooLow:
        break;
      case InliningDecision_Inline: {
        InliningStatus status;
        MOZ_TRY_VAR(status, inlineScriptedCall(callInfo, commonSetter));
        if (status == InliningStatus_Inlined) {
          *emitted = true;
          return Ok();
        }
      }
    }
  }

  Maybe<CallTargets> targets;
  targets.emplace(alloc());
  if (!targets->append(commonSetter)) {
    return abort(AbortReason::Alloc);
  }
  MCall* call;
  MOZ_TRY_VAR(call, makeCallHelper(targets, callInfo));

  current->push(value);
  MOZ_TRY(resumeAfter(call));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::setPropTryCommonDOMSetter(
    bool* emitted, MDefinition* obj, MDefinition* value, JSFunction* setter,
    TemporaryTypeSet* objTypes) {
  MOZ_ASSERT(*emitted == false);

  DOMObjectKind objKind = DOMObjectKind::Unknown;
  if (!objTypes || !objTypes->isDOMClass(constraints(), &objKind)) {
    return Ok();
  }

  bool isDOM = false;
  MOZ_TRY_VAR(isDOM, testShouldDOMCall(objTypes, setter, JSJitInfo::Setter));
  if (!isDOM) {
    return Ok();
  }

  // Emit SetDOMProperty.
  MOZ_ASSERT(setter->jitInfo()->type() == JSJitInfo::Setter);
  MSetDOMProperty* set = MSetDOMProperty::New(
      alloc(), setter->jitInfo()->setter, objKind, setter->realm(), obj, value);

  current->add(set);
  current->push(value);

  MOZ_TRY(resumeAfter(set));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::setPropTryDefiniteSlot(bool* emitted,
                                                     MDefinition* obj,
                                                     PropertyName* name,
                                                     MDefinition* value,
                                                     bool barrier) {
  MOZ_ASSERT(*emitted == false);

  if (barrier) {
    return Ok();
  }

  uint32_t nfixed;
  uint32_t slot =
      getDefiniteSlot(obj->resultTypeSet(), NameToId(name), &nfixed);
  if (slot == UINT32_MAX) {
    return Ok();
  }

  bool writeBarrier = false;
  for (size_t i = 0; i < obj->resultTypeSet()->getObjectCount(); i++) {
    TypeSet::ObjectKey* key = obj->resultTypeSet()->getObject(i);
    if (!key) {
      continue;
    }

    HeapTypeSetKey property = key->property(NameToId(name));
    if (property.nonWritable(constraints())) {
      return Ok();
    }
    writeBarrier |= property.needsBarrier(constraints());
  }

  if (needsPostBarrier(value)) {
    current->add(MPostWriteBarrier::New(alloc(), obj, value));
  }

  MInstruction* store;
  if (slot < nfixed) {
    store = MStoreFixedSlot::New(alloc(), obj, slot, value);
    if (writeBarrier) {
      store->toStoreFixedSlot()->setNeedsBarrier();
    }
  } else {
    MInstruction* slots = MSlots::New(alloc(), obj);
    current->add(slots);

    store = MStoreSlot::New(alloc(), slots, slot - nfixed, value);
    if (writeBarrier) {
      store->toStoreSlot()->setNeedsBarrier();
    }
  }

  current->add(store);
  current->push(value);

  MOZ_TRY(resumeAfter(store));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::setPropTryInlineAccess(
    bool* emitted, MDefinition* obj, PropertyName* name, MDefinition* value,
    bool barrier, TemporaryTypeSet* objTypes) {
  MOZ_ASSERT(*emitted == false);

  if (barrier) {
    return Ok();
  }

  BaselineInspector::ReceiverVector receivers(alloc());
  if (!inspector->maybeInfoForPropertyOp(pc, receivers)) {
    return abort(AbortReason::Alloc);
  }

  if (!canInlinePropertyOpShapes(receivers)) {
    return Ok();
  }

  if (receivers.length() == 1) {
    if (!receivers[0].getGroup()) {
      // Monomorphic store to a native object.
      spew("Inlining monomorphic native SETPROP");

      obj = addShapeGuard(obj, receivers[0].getShape(), Bailout_ShapeGuard);

      Shape* shape = receivers[0].getShape()->searchLinear(NameToId(name));
      MOZ_ASSERT(shape);

      if (needsPostBarrier(value)) {
        current->add(MPostWriteBarrier::New(alloc(), obj, value));
      }

      bool needsPreBarrier =
          objTypes->propertyNeedsBarrier(constraints(), NameToId(name));
      MOZ_TRY(storeSlot(obj, shape, value, needsPreBarrier));

      *emitted = true;
      return Ok();
    }
  }

  MOZ_ASSERT(receivers.length() > 1);
  spew("Inlining polymorphic SETPROP");

  if (Shape* propShape =
          PropertyShapesHaveSameSlot(receivers, NameToId(name))) {
    obj = addGuardReceiverPolymorphic(obj, receivers);
    if (!obj) {
      return abort(AbortReason::Alloc);
    }

    if (needsPostBarrier(value)) {
      current->add(MPostWriteBarrier::New(alloc(), obj, value));
    }

    bool needsPreBarrier =
        objTypes->propertyNeedsBarrier(constraints(), NameToId(name));
    MOZ_TRY(storeSlot(obj, propShape, value, needsPreBarrier));

    *emitted = true;
    return Ok();
  }

  if (needsPostBarrier(value)) {
    current->add(MPostWriteBarrier::New(alloc(), obj, value));
  }

  MSetPropertyPolymorphic* ins =
      MSetPropertyPolymorphic::New(alloc(), obj, value, name);
  current->add(ins);
  current->push(value);

  for (size_t i = 0; i < receivers.length(); i++) {
    Shape* propShape = nullptr;
    if (receivers[i].getShape()) {
      propShape = receivers[i].getShape()->searchLinear(NameToId(name));
      MOZ_ASSERT(propShape);
    }
    if (!ins->addReceiver(receivers[i], propShape)) {
      return abort(AbortReason::Alloc);
    }
  }

  if (objTypes->propertyNeedsBarrier(constraints(), NameToId(name))) {
    ins->setNeedsBarrier();
  }

  MOZ_TRY(resumeAfter(ins));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::setPropTryCache(bool* emitted, MDefinition* obj,
                                              PropertyName* name,
                                              MDefinition* value,
                                              bool barrier) {
  MOZ_ASSERT(*emitted == false);

  bool strict = IsStrictSetPC(pc);

  MConstant* id = constant(StringValue(name));
  MSetPropertyCache* ins = MSetPropertyCache::New(
      alloc(), obj, id, value, strict, needsPostBarrier(value), barrier,
      /* guardHoles = */ false);
  current->add(ins);
  current->push(value);

  MOZ_TRY(resumeAfter(ins));

  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_delprop(PropertyName* name) {
  MDefinition* obj = current->pop();

  bool strict = JSOp(*pc) == JSOp::StrictDelProp;
  MInstruction* ins = MDeleteProperty::New(alloc(), obj, name, strict);

  current->add(ins);
  current->push(ins);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_delelem() {
  MDefinition* index = current->pop();
  MDefinition* obj = current->pop();

  bool strict = JSOp(*pc) == JSOp::StrictDelElem;
  MDeleteElement* ins = MDeleteElement::New(alloc(), obj, index, strict);
  current->add(ins);
  current->push(ins);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_regexp(RegExpObject* reobj) {
  MOZ_ASSERT(!IsInsideNursery(reobj));

  // Determine this while we're still on the main thread to avoid races.
  bool hasShared = reobj->hasShared();

  MRegExp* regexp = MRegExp::New(alloc(), constraints(), reobj, hasShared);
  current->add(regexp);
  current->push(regexp);

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_object(JSObject* obj) {
  if (mirGen_.options.cloneSingletons()) {
    MCloneLiteral* clone =
        MCloneLiteral::New(alloc(), constant(ObjectValue(*obj)));
    current->add(clone);
    current->push(clone);
    return resumeAfter(clone);
  }

  realm->setSingletonsAsValues();
  pushConstant(ObjectValue(*obj));
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_classconstructor() {
  MClassConstructor* constructor = MClassConstructor::New(alloc(), pc);
  current->add(constructor);
  current->push(constructor);
  return resumeAfter(constructor);
}

AbortReasonOr<Ok> IonBuilder::jsop_derivedclassconstructor() {
  MDefinition* prototype = current->pop();

  auto* constructor = MDerivedClassConstructor::New(alloc(), prototype, pc);
  current->add(constructor);
  current->push(constructor);
  return resumeAfter(constructor);
}

static LambdaFunctionInfo LambdaInfoFromFunction(JSFunction* fun) {
  return LambdaFunctionInfo(fun, fun->baseScript(), fun->flags(), fun->nargs(),
                            fun->isSingleton(),
                            ObjectGroup::useSingletonForClone(fun));
}

AbortReasonOr<Ok> IonBuilder::jsop_lambda(JSFunction* fun) {
  MOZ_ASSERT(usesEnvironmentChain());
  MOZ_ASSERT(!fun->isArrow());

  if (IsAsmJSModule(fun)) {
    return abort(AbortReason::Disable, "Lambda is an asm.js module function");
  }

  MConstant* cst = MConstant::NewConstraintlessObject(alloc(), fun);
  current->add(cst);
  auto* ins = MLambda::New(alloc(), constraints(), current->environmentChain(),
                           cst, LambdaInfoFromFunction(fun));
  current->add(ins);
  current->push(ins);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_lambda_arrow(JSFunction* fun) {
  MOZ_ASSERT(usesEnvironmentChain());
  MOZ_ASSERT(fun->isArrow());
  MOZ_ASSERT(!fun->isNative());

  MDefinition* newTargetDef = current->pop();
  MConstant* cst = MConstant::NewConstraintlessObject(alloc(), fun);
  current->add(cst);
  auto* ins =
      MLambdaArrow::New(alloc(), constraints(), current->environmentChain(),
                        newTargetDef, cst, LambdaInfoFromFunction(fun));
  current->add(ins);
  current->push(ins);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_funwithproto(JSFunction* fun) {
  MOZ_ASSERT(usesEnvironmentChain());
  MOZ_ASSERT(!fun->isArrow());
  MOZ_ASSERT(!fun->isNative());

  MDefinition* proto = current->pop();
  MConstant* cst = MConstant::NewConstraintlessObject(alloc(), fun);
  current->add(cst);
  auto* ins =
      MFunctionWithProto::New(alloc(), current->environmentChain(), proto, cst);
  current->add(ins);
  current->push(ins);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_setfunname(uint8_t prefixKind) {
  MDefinition* name = current->pop();
  MDefinition* fun = current->pop();
  MOZ_ASSERT(fun->type() == MIRType::Object);

  MSetFunName* ins = MSetFunName::New(alloc(), fun, name, prefixKind);

  current->add(ins);
  current->push(fun);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_pushlexicalenv(uint32_t index) {
  MOZ_ASSERT(usesEnvironmentChain());

  LexicalScope* scope = &script()->getScope(index)->as<LexicalScope>();
  MNewLexicalEnvironmentObject* ins = MNewLexicalEnvironmentObject::New(
      alloc(), current->environmentChain(), scope);

  current->add(ins);
  current->setEnvironmentChain(ins);

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_copylexicalenv(bool copySlots) {
  MOZ_ASSERT(usesEnvironmentChain());

  MCopyLexicalEnvironmentObject* ins = MCopyLexicalEnvironmentObject::New(
      alloc(), current->environmentChain(), copySlots);

  current->add(ins);
  current->setEnvironmentChain(ins);

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_getarg(uint32_t arg) {
  if (info().argsObjAliasesFormals()) {
    auto* getArg =
        MGetArgumentsObjectArg::New(alloc(), current->argumentsObject(), arg);
    current->add(getArg);
    current->push(getArg);
  } else {
    current->pushArg(arg);
  }
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_setarg(uint32_t arg) {
  MOZ_ASSERT(script()->jitScript()->modifiesArguments());
  MDefinition* val = current->peek(-1);

  if (!info().argumentsAliasesFormals()) {
    MOZ_ASSERT(!info().argsObjAliasesFormals());

    // |arguments| is never referenced within this function. No arguments object
    // is created in this case, so we don't need to worry about synchronizing
    // the argument values when writing to them.
    MOZ_ASSERT_IF(!info().hasArguments(), !info().needsArgsObj());

    // The arguments object doesn't map to the actual argument values, so we
    // also don't need to worry about synchronizing them.
    // Directly writing to a positional formal parameter is only possible when
    // the |arguments| contents are never observed, otherwise we can't
    // reconstruct the original parameter values when we access them through
    // |arguments[i]|. AnalyzeArgumentsUsage ensures this is handled correctly.
    MOZ_ASSERT_IF(info().hasArguments(), !info().hasMappedArgsObj());

    current->setArg(arg);
    return Ok();
  }

  MOZ_ASSERT(info().hasArguments() && info().hasMappedArgsObj(),
             "arguments aliases formals when an arguments binding is present "
             "and the arguments object is mapped");

  // |needsArgsObj()| is true when analyzing arguments usage, so the
  // JSOp::SetArg bytecode will always emit a |MSetArgumentsObjectArg| node.
  MOZ_ASSERT_IF(info().analysisMode() == Analysis_ArgumentsUsage,
                info().needsArgsObj());

  // When |ArgumentsUseCanBeLazy()| in IonAnalysis then inspects all |arguments|
  // uses, the |MSetArgumentsObjectArg| usage will prevent the script from using
  // lazy arguments and an actual arguments object is created.
  MOZ_ASSERT(info().needsArgsObj(),
             "unexpected JSOp::SetArg with lazy arguments");

  MOZ_ASSERT(
      info().argsObjAliasesFormals(),
      "argsObjAliasesFormals() is true iff a mapped arguments object is used");

  // If an arguments object is in use, and it aliases formals, then all SETARGs
  // must go through the arguments object.
  if (needsPostBarrier(val)) {
    current->add(
        MPostWriteBarrier::New(alloc(), current->argumentsObject(), val));
  }
  auto* ins = MSetArgumentsObjectArg::New(alloc(), current->argumentsObject(),
                                          arg, val);
  current->add(ins);
  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_defvar() {
  MOZ_ASSERT(JSOp(*pc) == JSOp::DefVar);

  // Pass the EnvironmentChain.
  MOZ_ASSERT(usesEnvironmentChain());

  MDefVar* defvar = MDefVar::New(alloc(), current->environmentChain());
  current->add(defvar);

  return resumeAfter(defvar);
}

AbortReasonOr<Ok> IonBuilder::jsop_deflexical() {
  MOZ_ASSERT(JSOp(*pc) == JSOp::DefLet || JSOp(*pc) == JSOp::DefConst);
  MOZ_ASSERT(usesEnvironmentChain());

  MDefinition* env = current->environmentChain();
  MDefLexical* defLexical = MDefLexical::New(alloc(), env);
  current->add(defLexical);

  return resumeAfter(defLexical);
}

AbortReasonOr<Ok> IonBuilder::jsop_deffun() {
  MOZ_ASSERT(usesEnvironmentChain());

  MDefFun* deffun =
      MDefFun::New(alloc(), current->pop(), current->environmentChain());
  current->add(deffun);

  return resumeAfter(deffun);
}

AbortReasonOr<Ok> IonBuilder::jsop_throwsetconst() {
  MInstruction* lexicalError =
      MThrowRuntimeLexicalError::New(alloc(), JSMSG_BAD_CONST_ASSIGN);
  current->add(lexicalError);
  return resumeAfter(lexicalError);
}

AbortReasonOr<Ok> IonBuilder::jsop_checklexical() {
  JSOp op = JSOp(*pc);
  MOZ_ASSERT(op == JSOp::CheckLexical || op == JSOp::CheckAliasedLexical);

  MDefinition* val = current->peek(-1);
  MDefinition* lexical;
  MOZ_TRY_VAR(lexical, addLexicalCheck(val));
  current->pop();
  current->push(lexical);

  if (op == JSOp::CheckLexical) {
    // Set the local slot so that a subsequent GetLocal without a CheckLexical
    // (the frontend can elide lexical checks) doesn't let a definition with
    // MIRType::MagicUninitializedLexical escape to arbitrary MIR instructions.
    // Note that in this case the GetLocal would be unreachable because we throw
    // an exception here, but we still generate MIR instructions for it.
    uint32_t slot = info().localSlot(GET_LOCALNO(pc));
    current->setSlot(slot, lexical);
  }

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_functionthis() {
  MOZ_ASSERT(info().funMaybeLazy());
  MOZ_ASSERT(!info().funMaybeLazy()->isArrow());

  if (script()->strict()) {
    // No need to wrap primitive |this| in strict mode.
    current->pushSlot(info().thisSlot());
    return Ok();
  }

  if (thisTypes && (thisTypes->getKnownMIRType() == MIRType::Object ||
                    (thisTypes->empty() && baselineFrame_ &&
                     baselineFrame_->thisType.isSomeObject()))) {
    // This is safe, because if the entry type of |this| is an object, it
    // will necessarily be an object throughout the entire function. OSR
    // can introduce a phi, but this phi will be specialized.
    current->pushSlot(info().thisSlot());
    return Ok();
  }

  // If we are doing an analysis, we might not yet know the type of |this|.
  // Instead of bailing out just push the |this| slot, as this code won't
  // actually execute and it does not matter whether |this| is primitive.
  if (info().isAnalysis()) {
    current->pushSlot(info().thisSlot());
    return Ok();
  }

  // Hard case: |this| may be a primitive we have to wrap.
  MDefinition* def = current->getSlot(info().thisSlot());

  if (def->type() == MIRType::Object) {
    current->push(def);
    return Ok();
  }

  // Beyond this point we may need to access non-syntactic global. Ion doesn't
  // currently support this so just abort.
  if (script()->hasNonSyntacticScope()) {
    return abort(AbortReason::Disable,
                 "JSOp::FunctionThis would need non-syntactic global");
  }

  if (IsNullOrUndefined(def->type())) {
    LexicalEnvironmentObject* globalLexical =
        &script()->global().lexicalEnvironment();
    pushConstant(globalLexical->thisValue());
    return Ok();
  }

  MComputeThis* thisObj = MComputeThis::New(alloc(), def);
  current->add(thisObj);
  current->push(thisObj);

  return resumeAfter(thisObj);
}

AbortReasonOr<Ok> IonBuilder::jsop_globalthis() {
  if (script()->hasNonSyntacticScope()) {
    // Ion does not compile global scripts with a non-syntactic scope, but
    // we can end up here when we're compiling an arrow function.
    return abort(AbortReason::Disable,
                 "JSOp::GlobalThis in script with non-syntactic scope");
  }

  LexicalEnvironmentObject* globalLexical =
      &script()->global().lexicalEnvironment();
  pushConstant(globalLexical->thisValue());
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_typeof() {
  MDefinition* input = current->pop();
  MTypeOf* ins = MTypeOf::New(alloc(), input, input->type());

  ins->cacheInputMaybeCallableOrEmulatesUndefined(constraints());

  current->add(ins);
  current->push(ins);

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_toasynciter() {
  MDefinition* nextMethod = current->pop();
  MDefinition* iterator = current->pop();
  MOZ_ASSERT(iterator->type() == MIRType::Object);

  MToAsyncIter* ins = MToAsyncIter::New(alloc(), iterator, nextMethod);

  current->add(ins);
  current->push(ins);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_toid() {
  // No-op if the index is trivally convertable to an id.
  MIRType type = current->peek(-1)->type();
  if (type == MIRType::Int32 || type == MIRType::String ||
      type == MIRType::Symbol) {
    return Ok();
  }

  MDefinition* index = current->pop();
  MToId* ins = MToId::New(alloc(), index);

  current->add(ins);
  current->push(ins);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_iter() {
  MDefinition* obj = current->pop();
  MInstruction* ins = MGetIteratorCache::New(alloc(), obj);

  current->add(ins);
  current->push(ins);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_itermore() {
  MDefinition* iter = current->peek(-1);
  MInstruction* ins = MIteratorMore::New(alloc(), iter);

  current->add(ins);
  current->push(ins);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_isnoiter() {
  MDefinition* def = current->peek(-1);
  MOZ_ASSERT(def->isIteratorMore());

  MInstruction* ins = MIsNoIter::New(alloc(), def);
  current->add(ins);
  current->push(ins);

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_iterend() {
  current->pop();  // Iterator value is not used.
  MDefinition* iter = current->pop();

  MInstruction* ins = MIteratorEnd::New(alloc(), iter);
  current->add(ins);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_iternext() {
  MDefinition* def = current->pop();
  MOZ_ASSERT(def->type() == MIRType::Value);

  // The value must be a string.
  MInstruction* unbox =
      MUnbox::New(alloc(), def, MIRType::String, MUnbox::Infallible);
  current->add(unbox);
  current->push(unbox);

  return Ok();
}

MDefinition* IonBuilder::walkEnvironmentChain(unsigned hops) {
  MDefinition* env = current->getSlot(info().environmentChainSlot());

  for (unsigned i = 0; i < hops; i++) {
    MInstruction* ins = MEnclosingEnvironment::New(alloc(), env);
    current->add(ins);
    env = ins;
  }

  return env;
}

MDefinition* IonBuilder::getAliasedVar(EnvironmentCoordinate ec) {
  MDefinition* obj = walkEnvironmentChain(ec.hops());

  MInstruction* load;
  if (EnvironmentObject::nonExtensibleIsFixedSlot(ec)) {
    load = MLoadFixedSlot::New(alloc(), obj, ec.slot());
  } else {
    MInstruction* slots = MSlots::New(alloc(), obj);
    current->add(slots);

    uint32_t slot = EnvironmentObject::nonExtensibleDynamicSlotIndex(ec);
    load = MLoadSlot::New(alloc(), slots, slot);
  }

  current->add(load);
  return load;
}

AbortReasonOr<Ok> IonBuilder::jsop_getaliasedvar(EnvironmentCoordinate ec) {
  MDefinition* load = getAliasedVar(ec);
  current->push(load);

  TemporaryTypeSet* types = bytecodeTypes(pc);
  return pushTypeBarrier(load, types, BarrierKind::TypeSet);
}

AbortReasonOr<Ok> IonBuilder::jsop_setaliasedvar(EnvironmentCoordinate ec) {
  MDefinition* rval = current->peek(-1);
  MDefinition* obj = walkEnvironmentChain(ec.hops());

  if (needsPostBarrier(rval)) {
    current->add(MPostWriteBarrier::New(alloc(), obj, rval));
  }

  MInstruction* store;
  if (EnvironmentObject::nonExtensibleIsFixedSlot(ec)) {
    store = MStoreFixedSlot::NewBarriered(alloc(), obj, ec.slot(), rval);
  } else {
    MInstruction* slots = MSlots::New(alloc(), obj);
    current->add(slots);

    uint32_t slot = EnvironmentObject::nonExtensibleDynamicSlotIndex(ec);
    store = MStoreSlot::NewBarriered(alloc(), slots, slot, rval);
  }

  current->add(store);
  return resumeAfter(store);
}

AbortReasonOr<Ok> IonBuilder::jsop_in() {
  MDefinition* obj = current->pop();
  MDefinition* id = current->pop();

  if (!forceInlineCaches()) {
    bool emitted = false;

    MOZ_TRY(inTryDense(&emitted, obj, id));
    if (emitted) {
      return Ok();
    }

    MOZ_TRY(hasTryNotDefined(&emitted, obj, id, /* ownProperty = */ false));
    if (emitted) {
      return Ok();
    }

    MOZ_TRY(hasTryDefiniteSlotOrUnboxed(&emitted, obj, id));
    if (emitted) {
      return Ok();
    }
  }

  MInCache* ins = MInCache::New(alloc(), id, obj);

  current->add(ins);
  current->push(ins);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::inTryDense(bool* emitted, MDefinition* obj,
                                         MDefinition* id) {
  MOZ_ASSERT(!*emitted);

  if (shouldAbortOnPreliminaryGroups(obj)) {
    return Ok();
  }

  if (!ElementAccessIsDenseNative(constraints(), obj, id)) {
    return Ok();
  }

  bool hasExtraIndexedProperty;
  MOZ_TRY_VAR(hasExtraIndexedProperty,
              ElementAccessHasExtraIndexedProperty(this, obj));
  if (hasExtraIndexedProperty) {
    return Ok();
  }

  *emitted = true;

  bool needsHoleCheck = !ElementAccessIsPacked(constraints(), obj);

  // Ensure id is an integer.
  MInstruction* idInt32 = MToNumberInt32::New(alloc(), id);
  current->add(idInt32);
  id = idInt32;

  // Get the elements vector.
  MElements* elements = MElements::New(alloc(), obj);
  current->add(elements);

  MInstruction* initLength = initializedLength(elements);

  // If there are no holes, speculate the InArray check will not fail.
  if (!needsHoleCheck && !failedBoundsCheck_) {
    addBoundsCheck(idInt32, initLength);
    pushConstant(BooleanValue(true));
    return Ok();
  }

  // Check if id < initLength and elem[id] not a hole.
  MInArray* ins =
      MInArray::New(alloc(), elements, id, initLength, obj, needsHoleCheck);

  current->add(ins);
  current->push(ins);

  return Ok();
}

AbortReasonOr<Ok> IonBuilder::hasTryNotDefined(bool* emitted, MDefinition* obj,
                                               MDefinition* id,
                                               bool ownProperty) {
  // Fold |id in obj| to |false|, if we know the object (or an object on its
  // prototype chain) does not have this property.

  MOZ_ASSERT(!*emitted);

  MConstant* idConst = id->maybeConstantValue();
  jsid propId;
  if (!idConst || !ValueToIdPure(idConst->toJSValue(), &propId)) {
    return Ok();
  }

  if (propId != IdToTypeId(propId)) {
    return Ok();
  }

  bool res;
  MOZ_TRY_VAR(res, testNotDefinedProperty(obj, propId, ownProperty));
  if (!res) {
    return Ok();
  }

  *emitted = true;

  pushConstant(BooleanValue(false));
  obj->setImplicitlyUsedUnchecked();
  id->setImplicitlyUsedUnchecked();
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::hasTryDefiniteSlotOrUnboxed(bool* emitted,
                                                          MDefinition* obj,
                                                          MDefinition* id) {
  // Fold |id in obj| to |true|, when obj definitely contains a property with
  // that name.
  MOZ_ASSERT(!*emitted);

  if (obj->type() != MIRType::Object) {
    return Ok();
  }

  MConstant* idConst = id->maybeConstantValue();
  jsid propId;
  if (!idConst || !ValueToIdPure(idConst->toJSValue(), &propId)) {
    return Ok();
  }

  if (propId != IdToTypeId(propId)) {
    return Ok();
  }

  // Try finding a native definite slot first.
  uint32_t nfixed;
  uint32_t slot = getDefiniteSlot(obj->resultTypeSet(), propId, &nfixed);
  if (slot == UINT32_MAX) {
    return Ok();
  }

  *emitted = true;

  pushConstant(BooleanValue(true));
  obj->setImplicitlyUsedUnchecked();
  id->setImplicitlyUsedUnchecked();
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_hasown() {
  MDefinition* obj = current->pop();
  MDefinition* id = current->pop();

  if (!forceInlineCaches()) {
    bool emitted = false;

    MOZ_TRY(hasTryNotDefined(&emitted, obj, id, /* ownProperty = */ true));
    if (emitted) {
      return Ok();
    }

    MOZ_TRY(hasTryDefiniteSlotOrUnboxed(&emitted, obj, id));
    if (emitted) {
      return Ok();
    }
  }

  MHasOwnCache* ins = MHasOwnCache::New(alloc(), obj, id);
  current->add(ins);
  current->push(ins);

  MOZ_TRY(resumeAfter(ins));
  return Ok();
}

AbortReasonOr<bool> IonBuilder::hasOnProtoChain(TypeSet::ObjectKey* key,
                                                JSObject* protoObject,
                                                bool* onProto) {
  MOZ_ASSERT(protoObject);

  while (true) {
    if (!alloc().ensureBallast()) {
      return abort(AbortReason::Alloc);
    }

    if (!key->hasStableClassAndProto(constraints()) ||
        !key->clasp()->isNative()) {
      return false;
    }

    JSObject* proto = checkNurseryObject(key->proto().toObjectOrNull());
    if (!proto) {
      *onProto = false;
      return true;
    }

    if (proto == protoObject) {
      *onProto = true;
      return true;
    }

    key = TypeSet::ObjectKey::get(proto);
  }

  MOZ_CRASH("Unreachable");
}

AbortReasonOr<Ok> IonBuilder::tryFoldInstanceOf(bool* emitted, MDefinition* lhs,
                                                JSObject* protoObject) {
  // Try to fold the js::IsPrototypeOf part of the instanceof operation.
  MOZ_ASSERT(*emitted == false);

  if (!lhs->mightBeType(MIRType::Object)) {
    // If the lhs is a primitive, the result is false.
    lhs->setImplicitlyUsedUnchecked();
    pushConstant(BooleanValue(false));
    *emitted = true;
    return Ok();
  }

  TemporaryTypeSet* lhsTypes = lhs->resultTypeSet();
  if (!lhsTypes || lhsTypes->unknownObject()) {
    return Ok();
  }

  // We can fold if either all objects have protoObject on their proto chain
  // or none have.
  bool isFirst = true;
  bool knownIsInstance = false;

  for (unsigned i = 0; i < lhsTypes->getObjectCount(); i++) {
    TypeSet::ObjectKey* key = lhsTypes->getObject(i);
    if (!key) {
      continue;
    }

    bool checkSucceeded;
    bool isInstance;
    MOZ_TRY_VAR(checkSucceeded, hasOnProtoChain(key, protoObject, &isInstance));
    if (!checkSucceeded) {
      return Ok();
    }

    if (isFirst) {
      knownIsInstance = isInstance;
      isFirst = false;
    } else if (knownIsInstance != isInstance) {
      // Some of the objects have protoObject on their proto chain and
      // others don't, so we can't optimize this.
      return Ok();
    }
  }

  if (knownIsInstance && lhsTypes->getKnownMIRType() != MIRType::Object) {
    // The result is true for all objects, but the lhs might be a primitive.
    // We can't fold this completely but we can use a much faster IsObject
    // test.
    MIsObject* isObject = MIsObject::New(alloc(), lhs);
    current->add(isObject);
    current->push(isObject);
    *emitted = true;
    return Ok();
  }

  lhs->setImplicitlyUsedUnchecked();
  pushConstant(BooleanValue(knownIsInstance));
  *emitted = true;
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_instanceof() {
  MDefinition* rhs = current->pop();
  MDefinition* obj = current->pop();
  bool emitted = false;

  // If this is an 'x instanceof function' operation and we can determine the
  // exact function and prototype object being tested for, use a typed path.
  do {
    TemporaryTypeSet* rhsTypes = rhs->resultTypeSet();
    JSObject* rhsObject = rhsTypes ? rhsTypes->maybeSingleton() : nullptr;
    if (!rhsObject || !rhsObject->is<JSFunction>() ||
        rhsObject->isBoundFunction()) {
      break;
    }

    // Refuse to optimize anything whose [[Prototype]] isn't Function.prototype
    // since we can't guarantee that it uses the default @@hasInstance method.
    if (rhsObject->hasUncacheableProto() || !rhsObject->hasStaticPrototype()) {
      break;
    }

    Value funProto = script()->global().getPrototype(JSProto_Function);
    if (!funProto.isObject() ||
        rhsObject->staticPrototype() != &funProto.toObject()) {
      break;
    }

    // If the user has supplied their own @@hasInstance method we shouldn't
    // clobber it.
    JSFunction* fun = &rhsObject->as<JSFunction>();
    const WellKnownSymbols* symbols = &realm->runtime()->wellKnownSymbols();
    if (!js::FunctionHasDefaultHasInstance(fun, *symbols)) {
      break;
    }

    // Ensure that we will bail if the @@hasInstance property or [[Prototype]]
    // change.
    TypeSet::ObjectKey* rhsKey = TypeSet::ObjectKey::get(rhsObject);
    if (!rhsKey->hasStableClassAndProto(constraints())) {
      break;
    }

    if (rhsKey->unknownProperties()) {
      break;
    }

    HeapTypeSetKey hasInstanceObject =
        rhsKey->property(SYMBOL_TO_JSID(symbols->hasInstance));
    if (hasInstanceObject.isOwnProperty(constraints())) {
      break;
    }

    HeapTypeSetKey protoProperty =
        rhsKey->property(NameToId(names().prototype));
    JSObject* protoObject = protoProperty.singleton(constraints());
    if (!protoObject) {
      break;
    }

    rhs->setImplicitlyUsedUnchecked();

    MOZ_TRY(tryFoldInstanceOf(&emitted, obj, protoObject));
    if (emitted) {
      return Ok();
    }

    MInstanceOf* ins = MInstanceOf::New(alloc(), obj, protoObject);

    current->add(ins);
    current->push(ins);

    return resumeAfter(ins);
  } while (false);

  // Try to inline a fast path based on Baseline ICs.
  do {
    Shape* shape;
    uint32_t slot;
    JSObject* protoObject;
    if (!inspector->instanceOfData(pc, &shape, &slot, &protoObject)) {
      break;
    }

    // Shape guard.
    rhs = addShapeGuard(rhs, shape, Bailout_ShapeGuard);

    // Guard .prototype == protoObject.
    MOZ_ASSERT(shape->numFixedSlots() == 0, "Must be a dynamic slot");
    MSlots* slots = MSlots::New(alloc(), rhs);
    current->add(slots);
    MLoadSlot* prototype = MLoadSlot::New(alloc(), slots, slot);
    current->add(prototype);
    MConstant* protoConst =
        MConstant::NewConstraintlessObject(alloc(), protoObject);
    current->add(protoConst);
    MGuardObjectIdentity* guard =
        MGuardObjectIdentity::New(alloc(), prototype, protoConst,
                                  /* bailOnEquality = */ false);
    current->add(guard);

    MOZ_TRY(tryFoldInstanceOf(&emitted, obj, protoObject));
    if (emitted) {
      return Ok();
    }

    MInstanceOf* ins = MInstanceOf::New(alloc(), obj, protoObject);
    current->add(ins);
    current->push(ins);
    return resumeAfter(ins);
  } while (false);

  MInstanceOfCache* ins = MInstanceOfCache::New(alloc(), obj, rhs);

  current->add(ins);
  current->push(ins);

  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_debugger() {
  MDebugger* debugger = MDebugger::New(alloc());
  current->add(debugger);

  // The |debugger;| statement will always bail out to baseline if
  // cx->compartment()->isDebuggee(). Resume in-place and have baseline
  // handle the details.
  return resumeAt(debugger, pc);
}

AbortReasonOr<Ok> IonBuilder::jsop_implicitthis(PropertyName* name) {
  MOZ_ASSERT(usesEnvironmentChain());

  MImplicitThis* implicitThis =
      MImplicitThis::New(alloc(), current->environmentChain(), name);
  current->add(implicitThis);
  current->push(implicitThis);

  return resumeAfter(implicitThis);
}

AbortReasonOr<Ok> IonBuilder::jsop_importmeta() {
  if (info().analysisMode() == Analysis_ArgumentsUsage) {
    // The meta object may not have been created yet. Just push a dummy
    // value, it does not affect the arguments analysis.
    MUnknownValue* unknown = MUnknownValue::New(alloc());
    current->add(unknown);
    current->push(unknown);
    return Ok();
  }

  ModuleObject* module = GetModuleObjectForScript(script());
  MOZ_ASSERT(module);

  MModuleMetadata* meta = MModuleMetadata::New(alloc(), module);
  current->add(meta);
  current->push(meta);
  return resumeAfter(meta);
}

AbortReasonOr<Ok> IonBuilder::jsop_dynamic_import() {
  MDefinition* specifier = current->pop();

  MDynamicImport* ins = MDynamicImport::New(alloc(), specifier);
  current->add(ins);
  current->push(ins);
  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_instrumentation_active() {
  // All IonScripts in the realm are discarded when instrumentation activity
  // changes, so we can treat the value we get as a constant.
  bool active = RealmInstrumentation::isActive(&script()->global());
  pushConstant(BooleanValue(active));
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_instrumentation_callback() {
  JSObject* obj = RealmInstrumentation::getCallback(&script()->global());
  MOZ_ASSERT(obj);
  pushConstant(ObjectValue(*obj));
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_instrumentation_scriptid() {
  // Getting the script ID requires interacting with the Debugger used for
  // instrumentation, but cannot run script.
  JSContext* cx = TlsContext.get();

  int32_t scriptId;
  RootedScript script(cx, this->script());
  if (!RealmInstrumentation::getScriptId(cx, cx->global(), script, &scriptId)) {
    return abort(AbortReason::Error);
  }
  pushConstant(Int32Value(scriptId));
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_objwithproto() {
  auto* ins = MObjectWithProto::New(alloc(), current->pop());
  current->add(ins);
  current->push(ins);
  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_builtinproto() {
  MOZ_ASSERT(GET_UINT8(pc) < JSProto_LIMIT);
  JSProtoKey key = static_cast<JSProtoKey>(GET_UINT8(pc));

  // Bake in the prototype if it exists.
  if (JSObject* proto = script()->global().maybeGetPrototype(key)) {
    pushConstant(ObjectValue(*proto));
    return Ok();
  }

  // Otherwise emit code to generate it.
  auto* ins = MBuiltinProto::New(alloc(), pc);
  current->add(ins);
  current->push(ins);
  return resumeAfter(ins);
}

AbortReasonOr<Ok> IonBuilder::jsop_checkreturn() {
  MOZ_ASSERT(!script()->noScriptRval());

  MDefinition* returnValue = current->getSlot(info().returnValueSlot());
  MDefinition* thisValue = current->pop();

  if (returnValue->type() == MIRType::Object) {
    thisValue->setImplicitlyUsedUnchecked();
    return Ok();
  }

  if (returnValue->type() == MIRType::Undefined &&
      !thisValue->mightBeMagicType()) {
    // The return value mustn't be optimized out, otherwise baseline may see the
    // optimized-out magic value when it re-executes this op after a bailout.
    returnValue->setImplicitlyUsedUnchecked();
    thisValue->setImplicitlyUsedUnchecked();
    current->setSlot(info().returnValueSlot(), thisValue);
    return Ok();
  }

  auto* ins = MCheckReturn::New(alloc(), returnValue, thisValue);
  current->add(ins);
  current->setSlot(info().returnValueSlot(), ins);
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_superfun() {
  MDefinition* callee = current->pop();

  do {
    TemporaryTypeSet* calleeTypes = callee->resultTypeSet();
    if (!calleeTypes) {
      break;
    }

    TemporaryTypeSet::ObjectKey* calleeKey = calleeTypes->maybeSingleObject();
    if (!calleeKey) {
      break;
    }

    JSObject* calleeObj;
    if (calleeKey->isSingleton()) {
      calleeObj = calleeKey->singleton();
    } else {
      calleeObj = calleeKey->group()->maybeInterpretedFunction();
    }
    if (!calleeObj) {
      break;
    }

    // Refuse to optimize if the prototype is uncacheable.
    if (calleeObj->hasUncacheableProto() || !calleeObj->hasStaticPrototype()) {
      break;
    }

    // The prototype must be a constructor.
    JSObject* proto = calleeObj->staticPrototype();
    if (!proto || !proto->isConstructor()) {
      break;
    }

    // Add a constraint to ensure we're notified when the prototype changes.
    if (!calleeKey->hasStableClassAndProto(constraints())) {
      break;
    }

    callee->setImplicitlyUsedUnchecked();

    pushConstant(ObjectValue(*proto));

    return Ok();
  } while (false);

  auto* ins = MSuperFunction::New(alloc(), callee);
  current->add(ins);
  current->push(ins);
  return Ok();
}

AbortReasonOr<Ok> IonBuilder::jsop_inithomeobject() {
  MDefinition* homeObject = current->pop();
  MDefinition* function = current->pop();

  if (needsPostBarrier(homeObject)) {
    current->add(MPostWriteBarrier::New(alloc(), function, homeObject));
  }

  auto* ins = MInitHomeObject::New(alloc(), function, homeObject);
  current->add(ins);
  current->push(ins);
  return Ok();
}

MInstruction* IonBuilder::addConvertElementsToDoubles(MDefinition* elements) {
  MInstruction* convert = MConvertElementsToDoubles::New(alloc(), elements);
  current->add(convert);
  return convert;
}

MDefinition* IonBuilder::addMaybeCopyElementsForWrite(MDefinition* object,
                                                      bool checkNative) {
  if (!ElementAccessMightBeCopyOnWrite(constraints(), object)) {
    return object;
  }
  MInstruction* copy =
      MMaybeCopyElementsForWrite::New(alloc(), object, checkNative);
  current->add(copy);
  return copy;
}

MInstruction* IonBuilder::addBoundsCheck(MDefinition* index,
                                         MDefinition* length) {
  MInstruction* check = MBoundsCheck::New(alloc(), index, length);
  current->add(check);

  // If a bounds check failed in the past, don't optimize bounds checks.
  if (failedBoundsCheck_) {
    check->setNotMovable();
  }

  if (JitOptions.spectreIndexMasking) {
    // Use a separate MIR instruction for the index masking. Doing this as
    // part of MBoundsCheck would be unsound because bounds checks can be
    // optimized or eliminated completely. Consider this:
    //
    //   for (var i = 0; i < x; i++)
    //        res = arr[i];
    //
    // If we can prove |x < arr.length|, we are able to eliminate the bounds
    // check, but we should not get rid of the index masking because the
    // |i < x| branch could still be mispredicted.
    //
    // Using a separate instruction lets us eliminate the bounds check
    // without affecting the index masking.
    check = MSpectreMaskIndex::New(alloc(), check, length);
    current->add(check);
  }

  return check;
}

MInstruction* IonBuilder::addShapeGuard(MDefinition* obj, Shape* const shape,
                                        BailoutKind bailoutKind) {
  MGuardShape* guard = MGuardShape::New(alloc(), obj, shape, bailoutKind);
  current->add(guard);

  // If a shape guard failed in the past, don't optimize shape guard.
  if (failedShapeGuard_) {
    guard->setNotMovable();
  }

  return guard;
}

MInstruction* IonBuilder::addGroupGuard(MDefinition* obj, ObjectGroup* group,
                                        BailoutKind bailoutKind) {
  MGuardObjectGroup* guard =
      MGuardObjectGroup::New(alloc(), obj, group,
                             /* bailOnEquality = */ false, bailoutKind);
  current->add(guard);

  // If a shape guard failed in the past, don't optimize group guards.
  if (failedShapeGuard_) {
    guard->setNotMovable();
  }

  LifoAlloc* lifoAlloc = alloc().lifoAlloc();
  guard->setResultTypeSet(
      lifoAlloc->new_<TemporaryTypeSet>(lifoAlloc, TypeSet::ObjectType(group)));

  return guard;
}

MInstruction* IonBuilder::addGuardReceiverPolymorphic(
    MDefinition* obj, const BaselineInspector::ReceiverVector& receivers) {
  if (receivers.length() == 1) {
    if (!receivers[0].getGroup()) {
      // Monomorphic guard on a native object.
      return addShapeGuard(obj, receivers[0].getShape(), Bailout_ShapeGuard);
    }
  }

  MGuardReceiverPolymorphic* guard =
      MGuardReceiverPolymorphic::New(alloc(), obj);
  current->add(guard);

  if (failedShapeGuard_) {
    guard->setNotMovable();
  }

  for (size_t i = 0; i < receivers.length(); i++) {
    if (!guard->addReceiver(receivers[i])) {
      return nullptr;
    }
  }

  return guard;
}

MInstruction* IonBuilder::addSharedTypedArrayGuard(MDefinition* obj) {
  MGuardSharedTypedArray* guard = MGuardSharedTypedArray::New(alloc(), obj);
  current->add(guard);
  return guard;
}

TemporaryTypeSet* IonBuilder::bytecodeTypes(jsbytecode* pc) {
  return JitScript::BytecodeTypes(script(), pc, bytecodeTypeMap, &typeArrayHint,
                                  typeArray);
}

void IonBuilder::checkNurseryCell(gc::Cell* cell) {
  // If we try to use any nursery pointers during compilation, make sure that
  // the main thread will cancel this compilation before performing a minor
  // GC. All constants used during compilation should either go through this
  // function or should come from a type set (which has a similar barrier).
  if (cell && IsInsideNursery(cell)) {
    realm->zone()->setMinorGCShouldCancelIonCompilations();
    mirGen().setNotSafeForMinorGC();
  }
}

JSObject* IonBuilder::checkNurseryObject(JSObject* obj) {
  checkNurseryCell(obj);
  return obj;
}

MConstant* IonBuilder::constant(const Value& v) {
  MOZ_ASSERT(!v.isString() || v.toString()->isAtom(),
             "Handle non-atomized strings outside IonBuilder.");

  if (v.isGCThing()) {
    checkNurseryCell(v.toGCThing());
  }

  MConstant* c = MConstant::New(alloc(), v, constraints());
  current->add(c);
  return c;
}

MConstant* IonBuilder::constantInt(int32_t i) {
  return constant(Int32Value(i));
}

MInstruction* IonBuilder::initializedLength(MDefinition* elements) {
  MInstruction* res = MInitializedLength::New(alloc(), elements);
  current->add(res);
  return res;
}

MInstruction* IonBuilder::setInitializedLength(MDefinition* obj, size_t count) {
  MOZ_ASSERT(count);

  // MSetInitializedLength takes the index of the last element, rather
  // than the count itself.
  MInstruction* elements = MElements::New(alloc(), obj);
  current->add(elements);
  MInstruction* res = MSetInitializedLength::New(
      alloc(), elements, constant(Int32Value(count - 1)));
  current->add(res);
  return res;
}

MDefinition* IonBuilder::getCallee() {
  if (inliningDepth_ == 0) {
    MInstruction* callee = MCallee::New(alloc());
    current->add(callee);
    return callee;
  }

  return inlineCallInfo_->fun();
}

void IonBuilder::addAbortedPreliminaryGroup(ObjectGroup* group) {
  for (size_t i = 0; i < abortedPreliminaryGroups_.length(); i++) {
    if (group == abortedPreliminaryGroups_[i]) {
      return;
    }
  }
  AutoEnterOOMUnsafeRegion oomUnsafe;
  if (!abortedPreliminaryGroups_.append(group)) {
    oomUnsafe.crash("addAbortedPreliminaryGroup");
  }
}

AbortReasonOr<MDefinition*> IonBuilder::addLexicalCheck(MDefinition* input) {
  MOZ_ASSERT(JSOp(*pc) == JSOp::CheckLexical ||
             JSOp(*pc) == JSOp::CheckAliasedLexical ||
             JSOp(*pc) == JSOp::GetImport);

  // If we're guaranteed to not be JS_UNINITIALIZED_LEXICAL, no need to check.
  if (input->type() == MIRType::MagicUninitializedLexical) {
    // Mark the input as implicitly used so the JS_UNINITIALIZED_LEXICAL
    // magic value will be preserved on bailout.
    input->setImplicitlyUsedUnchecked();
    MInstruction* lexicalCheck =
        MThrowRuntimeLexicalError::New(alloc(), JSMSG_UNINITIALIZED_LEXICAL);
    current->add(lexicalCheck);
    MOZ_TRY(resumeAfter(lexicalCheck));
    return constant(UndefinedValue());
  }

  if (input->type() == MIRType::Value) {
    MInstruction* lexicalCheck = MLexicalCheck::New(alloc(), input);
    current->add(lexicalCheck);
    if (failedLexicalCheck_) {
      lexicalCheck->setNotMovableUnchecked();
    }
    return lexicalCheck;
  }

  input->setImplicitlyUsedUnchecked();
  return input;
}

MDefinition* IonBuilder::convertToBoolean(MDefinition* input) {
  // Convert to bool with the '!!' idiom
  MNot* resultInverted = MNot::New(alloc(), input, constraints());
  current->add(resultInverted);
  MNot* result = MNot::New(alloc(), resultInverted, constraints());
  current->add(result);

  return result;
}
