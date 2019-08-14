/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "debugger/Frame-inl.h"

#include "mozilla/Assertions.h"   // for AssertionConditionType
#include "mozilla/HashTable.h"    // for HashMapEntry
#include "mozilla/Maybe.h"        // for Maybe
#include "mozilla/Move.h"         // for std::move
#include "mozilla/Range.h"        // for Range
#include "mozilla/RangedPtr.h"    // for RangedPtr
#include "mozilla/Result.h"       // for Result
#include "mozilla/ScopeExit.h"    // for MakeScopeExit, ScopeExit
#include "mozilla/ThreadLocal.h"  // for ThreadLocal
#include "mozilla/Vector.h"       // for Vector

#include <stddef.h>  // for size_t
#include <stdint.h>  // for int32_t
#include <string.h>  // for strlen

#include "jsapi.h"        // for CallArgs, Handle
#include "jsfriendapi.h"  // for GetErrorMessage
#include "jsnum.h"        // for Int32ToString

#include "builtin/Array.h"      // for NewDenseCopiedArray
#include "debugger/Debugger.h"  // for Completion, Debugger
#include "debugger/DebugScript.h"
#include "debugger/Environment.h"          // for DebuggerEnvironment
#include "debugger/NoExecute.h"            // for LeaveDebuggeeNoExecute
#include "debugger/Object.h"               // for DebuggerObject
#include "debugger/Script.h"               // for DebuggerScript
#include "frontend/BytecodeCompilation.h"  // for CompileEvalScript
#include "gc/Barrier.h"                    // for HeapPtr
#include "gc/FreeOp.h"                     // for JSFreeOp
#include "gc/GC.h"                         // for MemoryUse
#include "gc/Marking.h"                    // for IsAboutToBeFinalized
#include "gc/Rooting.h"                    // for RootedDebuggerFrame
#include "gc/Tracer.h"                     // for TraceCrossCompartmentEdge
#include "gc/ZoneAllocator.h"              // for AddCellMemory
#include "jit/JSJitFrameIter.h"            // for InlineFrameIterator
#include "jit/RematerializedFrame.h"       // for RematerializedFrame
#include "js/Proxy.h"                      // for PrivateValue
#include "js/SourceText.h"                 // for SourceText, SourceOwnership
#include "js/StableStringChars.h"          // for AutoStableStringChars
#include "vm/ArgumentsObject.h"            // for ArgumentsObject
#include "vm/ArrayObject.h"                // for ArrayObject
#include "vm/BytecodeUtil.h"               // for JSDVG_SEARCH_STACK
#include "vm/Compartment.h"                // for Compartment
#include "vm/EnvironmentObject.h"          // for IsGlobalLexicalEnvironment
#include "vm/GeneratorObject.h"            // for AbstractGeneratorObject
#include "vm/GlobalObject.h"               // for GlobalObject
#include "vm/Interpreter.h"                // for Call, ExecuteKernel
#include "vm/JSAtom.h"                     // for Atomize
#include "vm/JSContext.h"                  // for JSContext, ReportValueError
#include "vm/JSFunction.h"                 // for JSFunction, NewNativeFunction
#include "vm/JSObject.h"                   // for JSObject, RequireObject
#include "vm/JSScript.h"                   // for JSScript
#include "vm/NativeObject.h"               // for NativeDefineDataProperty
#include "vm/Realm.h"                      // for AutoRealm
#include "vm/Runtime.h"                    // for JSAtomState
#include "vm/Scope.h"                      // for PositionalFormalParameterIter
#include "vm/Stack.h"                      // for AbstractFramePtr, FrameIter
#include "vm/StringType.h"                 // for PropertyName, JSString
#include "wasm/WasmDebug.h"                // for DebugState
#include "wasm/WasmInstance.h"             // for Instance
#include "wasm/WasmJS.h"                   // for WasmInstanceObject
#include "wasm/WasmTypes.h"                // for DebugFrame

#include "debugger/Debugger-inl.h"  // for Debugger::fromJSObject
#include "vm/Compartment-inl.h"     // for Compartment::wrap
#include "vm/JSContext-inl.h"       // for JSContext::check
#include "vm/JSObject-inl.h"        // for NewObjectWithGivenProto
#include "vm/JSScript-inl.h"        // for JSScript::ensureHasAnalyzedArgsUsage
#include "vm/NativeObject-inl.h"    // for NativeObject::global
#include "vm/ObjectOperations-inl.h"  // for GetProperty
#include "vm/Realm-inl.h"             // for AutoRealm::AutoRealm
#include "vm/Stack-inl.h"             // for AbstractFramePtr::script

namespace js {
namespace jit {
class JitFrameLayout;
} /* namespace jit */
} /* namespace js */

using namespace js;

using JS::AutoStableStringChars;
using JS::CompileOptions;
using JS::SourceOwnership;
using JS::SourceText;
using mozilla::MakeScopeExit;
using mozilla::Maybe;

ScriptedOnStepHandler::ScriptedOnStepHandler(JSObject* object)
    : object_(object) {
  MOZ_ASSERT(object_->isCallable());
}

JSObject* ScriptedOnStepHandler::object() const { return object_; }

void ScriptedOnStepHandler::hold(JSObject* owner) {
  AddCellMemory(owner, allocSize(), MemoryUse::DebuggerOnStepHandler);
}

void ScriptedOnStepHandler::drop(JSFreeOp* fop, JSObject* owner) {
  fop->delete_(owner, this, allocSize(), MemoryUse::DebuggerOnStepHandler);
}

void ScriptedOnStepHandler::trace(JSTracer* tracer) {
  TraceEdge(tracer, &object_, "OnStepHandlerFunction.object");
}

bool ScriptedOnStepHandler::onStep(JSContext* cx, HandleDebuggerFrame frame,
                                   ResumeMode& resumeMode,
                                   MutableHandleValue vp) {
  RootedValue fval(cx, ObjectValue(*object_));
  RootedValue rval(cx);
  if (!js::Call(cx, fval, frame, &rval)) {
    return false;
  }

  return ParseResumptionValue(cx, rval, resumeMode, vp);
};

size_t ScriptedOnStepHandler::allocSize() const { return sizeof(*this); }

ScriptedOnPopHandler::ScriptedOnPopHandler(JSObject* object) : object_(object) {
  MOZ_ASSERT(object->isCallable());
}

JSObject* ScriptedOnPopHandler::object() const { return object_; }

void ScriptedOnPopHandler::hold(JSObject* owner) {
  AddCellMemory(owner, allocSize(), MemoryUse::DebuggerOnPopHandler);
}

void ScriptedOnPopHandler::drop(JSFreeOp* fop, JSObject* owner) {
  fop->delete_(owner, this, allocSize(), MemoryUse::DebuggerOnPopHandler);
}

void ScriptedOnPopHandler::trace(JSTracer* tracer) {
  TraceEdge(tracer, &object_, "OnStepHandlerFunction.object");
}

bool ScriptedOnPopHandler::onPop(JSContext* cx, HandleDebuggerFrame frame,
                                 const Completion& completion,
                                 ResumeMode& resumeMode,
                                 MutableHandleValue vp) {
  Debugger* dbg = Debugger::fromChildJSObject(frame);

  RootedValue completionValue(cx);
  if (!completion.buildCompletionValue(cx, dbg, &completionValue)) {
    return false;
  }

  RootedValue fval(cx, ObjectValue(*object_));
  RootedValue rval(cx);
  if (!js::Call(cx, fval, frame, completionValue, &rval)) {
    return false;
  }

  return ParseResumptionValue(cx, rval, resumeMode, vp);
};

size_t ScriptedOnPopHandler::allocSize() const { return sizeof(*this); }

inline js::Debugger* js::DebuggerFrame::owner() const {
  JSObject* dbgobj = &getReservedSlot(OWNER_SLOT).toObject();
  return Debugger::fromJSObject(dbgobj);
}

const JSClassOps DebuggerFrame::classOps_ = {
    nullptr,                        /* addProperty */
    nullptr,                        /* delProperty */
    nullptr,                        /* enumerate   */
    nullptr,                        /* newEnumerate */
    nullptr,                        /* resolve     */
    nullptr,                        /* mayResolve  */
    finalize,                       /* finalize */
    nullptr,                        /* call        */
    nullptr,                        /* hasInstance */
    nullptr,                        /* construct   */
    CallTraceMethod<DebuggerFrame>, /* trace */
};

const Class DebuggerFrame::class_ = {
    "Frame",
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS) |
        // We require foreground finalization so we can destruct GeneratorInfo's
        // HeapPtrs.
        JSCLASS_FOREGROUND_FINALIZE,
    &DebuggerFrame::classOps_};

enum { JSSLOT_DEBUGARGUMENTS_FRAME, JSSLOT_DEBUGARGUMENTS_COUNT };

const Class DebuggerArguments::class_ = {
    "Arguments", JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGARGUMENTS_COUNT)};

bool DebuggerFrame::resume(const FrameIter& iter) {
  FrameIter::Data* data = iter.copyData();
  if (!data) {
    return false;
  }
  setFrameIterData(data);
  return true;
}

bool DebuggerFrame::hasAnyLiveHooks() const {
  return !getReservedSlot(ONSTEP_HANDLER_SLOT).isUndefined() ||
         !getReservedSlot(ONPOP_HANDLER_SLOT).isUndefined();
}

/* static */
NativeObject* DebuggerFrame::initClass(JSContext* cx,
                                       Handle<GlobalObject*> global,
                                       HandleObject dbgCtor) {
  return InitClass(cx, dbgCtor, nullptr, &class_, construct, 0, properties_,
                   methods_, nullptr, nullptr);
}

/* static */
DebuggerFrame* DebuggerFrame::create(JSContext* cx, HandleObject proto,
                                     const FrameIter& iter,
                                     HandleNativeObject debugger) {
  DebuggerFrame* frame = NewObjectWithGivenProto<DebuggerFrame>(cx, proto);
  if (!frame) {
    return nullptr;
  }

  FrameIter::Data* data = iter.copyData();
  if (!data) {
    return nullptr;
  }
  frame->setFrameIterData(data);

  frame->setReservedSlot(OWNER_SLOT, ObjectValue(*debugger));

  return frame;
}

/**
 * Information held by a DebuggerFrame about a generator/async call. A
 * Debugger.Frame's GENERATOR_INFO_SLOT, if set, holds a PrivateValue pointing
 * to one of these.
 *
 * This is created and attached as soon as a generator object is created for a
 * debuggee generator/async frame, retained across suspensions and resumptions,
 * and cleared when the generator call ends permanently.
 *
 * It may seem like this information might belong in ordinary reserved slots on
 * the DebuggerFrame object. But that isn't possible:
 *
 * 1) Slots cannot contain cross-compartment references directly.
 * 2) Ordinary cross-compartment wrappers aren't good enough, because the
 *    debugger must create its own magic entries in the wrapper table for the GC
 *    to get zone collection groups right.
 * 3) Even if we make debugger wrapper table entries by hand, hiding
 *    cross-compartment edges as PrivateValues doesn't call post-barriers, and
 *    the generational GC won't update our pointer when the generator object
 *    gets tenured.
 *
 * Yes, officer, I definitely knew all this in advance and designed it this way
 * the first time.
 *
 * Note that it is not necessary to have a second cross-compartment wrapper
 * table entry to cover the pointer to the generator's script. The wrapper table
 * entries play two roles: they help the GC put a debugger zone in the same zone
 * group as its debuggee, and they serve as roots when collecting the debuggee
 * zone, but not the debugger zone. Since an AbstractGeneratorObject holds a
 * strong reference to its callee's script (via the callee), and the AGO and the
 * script are always in the same compartment, it suffices to add a
 * cross-compartment wrapper table entry for the Debugger.Frame -> AGO edge.
 */
class DebuggerFrame::GeneratorInfo {
  // An unwrapped cross-compartment reference to the generator object.
  //
  // Always an object.
  //
  // This cannot be GCPtr because we are not always destructed during sweeping;
  // a Debugger.Frame's generator is also cleared when the generator returns
  // permanently.
  HeapPtr<Value> unwrappedGenerator_;

  // A cross-compartment reference to the generator's script.
  HeapPtr<JSScript*> generatorScript_;

 public:
  GeneratorInfo(Handle<AbstractGeneratorObject*> unwrappedGenerator,
                HandleScript generatorScript)
      : unwrappedGenerator_(ObjectValue(*unwrappedGenerator)),
        generatorScript_(generatorScript) {}

  void trace(JSTracer* tracer, DebuggerFrame& frameObj) {
    TraceCrossCompartmentEdge(tracer, &frameObj, &unwrappedGenerator_,
                              "Debugger.Frame generator object");
    TraceCrossCompartmentEdge(tracer, &frameObj, &generatorScript_,
                              "Debugger.Frame generator script");
  }

  AbstractGeneratorObject& unwrappedGenerator() const {
    return unwrappedGenerator_.toObject().as<AbstractGeneratorObject>();
  }

  HeapPtr<JSScript*>& generatorScript() { return generatorScript_; }
};

js::AbstractGeneratorObject& js::DebuggerFrame::unwrappedGenerator() const {
  return generatorInfo()->unwrappedGenerator();
}

#ifdef DEBUG
JSScript* js::DebuggerFrame::generatorScript() const {
  return generatorInfo()->generatorScript();
}
#endif

bool DebuggerFrame::setGenerator(JSContext* cx,
                                 Handle<AbstractGeneratorObject*> genObj) {
  cx->check(this);

  Debugger::GeneratorWeakMap::AddPtr p =
      owner()->generatorFrames.lookupForAdd(genObj);
  if (p) {
    MOZ_ASSERT(p->value() == this);
    MOZ_ASSERT(&unwrappedGenerator() == genObj);
    return true;
  }

  // There are three relations we must establish:
  //
  // 1) The DebuggerFrame must point to the AbstractGeneratorObject.
  //
  // 2) generatorFrames must map the AbstractGeneratorObject to the
  //    DebuggerFrame.
  //
  // 3) The generator's script's observer count must be bumped.
  RootedScript script(cx, genObj->callee().nonLazyScript());
  auto* info = cx->new_<GeneratorInfo>(genObj, script);
  if (!info) {
    ReportOutOfMemory(cx);
    return false;
  }
  auto infoGuard = MakeScopeExit([&] { js_delete(info); });

  if (!owner()->generatorFrames.relookupOrAdd(p, genObj, this)) {
    ReportOutOfMemory(cx);
    return false;
  }
  auto generatorFramesGuard =
      MakeScopeExit([&] { owner()->generatorFrames.remove(genObj); });

  {
    AutoRealm ar(cx, script);
    if (!DebugScript::incrementGeneratorObserverCount(cx, script)) {
      return false;
    }
  }

  InitReservedSlot(this, GENERATOR_INFO_SLOT, info,
                   MemoryUse::DebuggerFrameGeneratorInfo);

  generatorFramesGuard.release();
  infoGuard.release();

  return true;
}

void DebuggerFrame::clearGenerator(JSFreeOp* fop) {
  if (!hasGenerator()) {
    return;
  }

  GeneratorInfo* info = generatorInfo();

  // 4) The generator's script's observer count must be dropped.
  //
  // For ordinary calls, Debugger.Frame objects drop the script's stepper count
  // when the frame is popped, but for generators, they leave the stepper count
  // incremented across suspensions. This means that, whereas ordinary calls
  // never need to drop the stepper count from the D.F finalizer, generator
  // calls may.
  HeapPtr<JSScript*>& generatorScript = info->generatorScript();
  if (!IsAboutToBeFinalized(&generatorScript)) {
    DebugScript::decrementGeneratorObserverCount(fop, generatorScript);

    OnStepHandler* handler = onStepHandler();
    if (handler) {
      DebugScript::decrementStepperCount(fop, generatorScript);
      handler->drop(fop, this);
      setReservedSlot(ONSTEP_HANDLER_SLOT, UndefinedValue());
    }
  }

  // 1) The DebuggerFrame must no longer point to the AbstractGeneratorObject.
  setReservedSlot(GENERATOR_INFO_SLOT, UndefinedValue());
  fop->delete_(this, info, MemoryUse::DebuggerFrameGeneratorInfo);
}

void DebuggerFrame::clearGenerator(
    JSFreeOp* fop, Debugger* owner,
    Debugger::GeneratorWeakMap::Enum* maybeGeneratorFramesEnum) {
  if (!hasGenerator()) {
    return;
  }

  // 2) generatorFrames must no longer map the AbstractGeneratorObject to the
  // DebuggerFrame.
  GeneratorInfo* info = generatorInfo();
  if (maybeGeneratorFramesEnum) {
    maybeGeneratorFramesEnum->removeFront();
  } else {
    owner->generatorFrames.remove(&info->unwrappedGenerator());
  }

  clearGenerator(fop);
}

/* static */
bool DebuggerFrame::getCallee(JSContext* cx, HandleDebuggerFrame frame,
                              MutableHandleDebuggerObject result) {
  MOZ_ASSERT(frame->isLive());

  AbstractFramePtr referent = DebuggerFrame::getReferent(frame);
  if (!referent.isFunctionFrame()) {
    result.set(nullptr);
    return true;
  }

  Debugger* dbg = frame->owner();

  RootedObject callee(cx, referent.callee());
  return dbg->wrapDebuggeeObject(cx, callee, result);
}

/* static */
bool DebuggerFrame::getIsConstructing(JSContext* cx, HandleDebuggerFrame frame,
                                      bool& result) {
  MOZ_ASSERT(frame->isLive());

  Maybe<FrameIter> maybeIter;
  if (!DebuggerFrame::getFrameIter(cx, frame, maybeIter)) {
    return false;
  }
  FrameIter& iter = *maybeIter;

  result = iter.isFunctionFrame() && iter.isConstructing();
  return true;
}

static void UpdateFrameIterPc(FrameIter& iter) {
  if (iter.abstractFramePtr().isWasmDebugFrame()) {
    // Wasm debug frames don't need their pc updated -- it's null.
    return;
  }

  if (iter.abstractFramePtr().isRematerializedFrame()) {
#ifdef DEBUG
    // Rematerialized frames don't need their pc updated. The reason we
    // need to update pc is because we might get the same Debugger.Frame
    // object for multiple re-entries into debugger code from debuggee
    // code. This reentrancy is not possible with rematerialized frames,
    // because when returning to debuggee code, we would have bailed out
    // to baseline.
    //
    // We walk the stack to assert that it doesn't need updating.
    jit::RematerializedFrame* frame =
        iter.abstractFramePtr().asRematerializedFrame();
    jit::JitFrameLayout* jsFrame = (jit::JitFrameLayout*)frame->top();
    jit::JitActivation* activation = iter.activation()->asJit();

    JSContext* cx = TlsContext.get();
    MOZ_ASSERT(cx == activation->cx());

    ActivationIterator activationIter(cx);
    while (activationIter.activation() != activation) {
      ++activationIter;
    }

    OnlyJSJitFrameIter jitIter(activationIter);
    while (!jitIter.frame().isIonJS() || jitIter.frame().jsFrame() != jsFrame) {
      ++jitIter;
    }

    jit::InlineFrameIterator ionInlineIter(cx, &jitIter.frame());
    while (ionInlineIter.frameNo() != frame->frameNo()) {
      ++ionInlineIter;
    }

    MOZ_ASSERT(ionInlineIter.pc() == iter.pc());
#endif
    return;
  }

  iter.updatePcQuadratic();
}

/* static */
bool DebuggerFrame::getEnvironment(JSContext* cx, HandleDebuggerFrame frame,
                                   MutableHandleDebuggerEnvironment result) {
  MOZ_ASSERT(frame->isLive());

  Debugger* dbg = frame->owner();

  Maybe<FrameIter> maybeIter;
  if (!DebuggerFrame::getFrameIter(cx, frame, maybeIter)) {
    return false;
  }
  FrameIter& iter = *maybeIter;

  Rooted<Env*> env(cx);
  {
    AutoRealm ar(cx, iter.abstractFramePtr().environmentChain());
    UpdateFrameIterPc(iter);
    env = GetDebugEnvironmentForFrame(cx, iter.abstractFramePtr(), iter.pc());
    if (!env) {
      return false;
    }
  }

  return dbg->wrapEnvironment(cx, env, result);
}

/* static */
bool DebuggerFrame::getIsGenerator(HandleDebuggerFrame frame) {
  AbstractFramePtr referent = DebuggerFrame::getReferent(frame);
  return referent.hasScript() && referent.script()->isGenerator();
}

/* static */
bool DebuggerFrame::getOffset(JSContext* cx, HandleDebuggerFrame frame,
                              size_t& result) {
  MOZ_ASSERT(frame->isLive());

  Maybe<FrameIter> maybeIter;
  if (!DebuggerFrame::getFrameIter(cx, frame, maybeIter)) {
    return false;
  }
  FrameIter& iter = *maybeIter;

  AbstractFramePtr referent = DebuggerFrame::getReferent(frame);
  if (referent.isWasmDebugFrame()) {
    iter.wasmUpdateBytecodeOffset();
    result = iter.wasmBytecodeOffset();
  } else {
    JSScript* script = iter.script();
    UpdateFrameIterPc(iter);
    jsbytecode* pc = iter.pc();
    result = script->pcToOffset(pc);
  }
  return true;
}

/* static */
bool DebuggerFrame::getOlder(JSContext* cx, HandleDebuggerFrame frame,
                             MutableHandleDebuggerFrame result) {
  MOZ_ASSERT(frame->isLive());

  Debugger* dbg = frame->owner();

  Maybe<FrameIter> maybeIter;
  if (!DebuggerFrame::getFrameIter(cx, frame, maybeIter)) {
    return false;
  }
  FrameIter& iter = *maybeIter;

  for (++iter; !iter.done(); ++iter) {
    if (dbg->observesFrame(iter)) {
      if (iter.isIon() && !iter.ensureHasRematerializedFrame(cx)) {
        return false;
      }
      return dbg->getFrame(cx, iter, result);
    }
  }

  result.set(nullptr);
  return true;
}

/* static */
bool DebuggerFrame::getThis(JSContext* cx, HandleDebuggerFrame frame,
                            MutableHandleValue result) {
  MOZ_ASSERT(frame->isLive());

  if (!requireScriptReferent(cx, frame)) {
    return false;
  }

  Debugger* dbg = frame->owner();

  Maybe<FrameIter> maybeIter;
  if (!DebuggerFrame::getFrameIter(cx, frame, maybeIter)) {
    return false;
  }
  FrameIter& iter = *maybeIter;

  {
    AbstractFramePtr frame = iter.abstractFramePtr();
    AutoRealm ar(cx, frame.environmentChain());

    UpdateFrameIterPc(iter);

    if (!GetThisValueForDebuggerMaybeOptimizedOut(cx, frame, iter.pc(),
                                                  result)) {
      return false;
    }
  }

  return dbg->wrapDebuggeeValue(cx, result);
}

/* static */
DebuggerFrameType DebuggerFrame::getType(HandleDebuggerFrame frame) {
  AbstractFramePtr referent = DebuggerFrame::getReferent(frame);

  // Indirect eval frames are both isGlobalFrame() and isEvalFrame(), so the
  // order of checks here is significant.
  if (referent.isEvalFrame()) {
    return DebuggerFrameType::Eval;
  }

  if (referent.isGlobalFrame()) {
    return DebuggerFrameType::Global;
  }

  if (referent.isFunctionFrame()) {
    return DebuggerFrameType::Call;
  }

  if (referent.isModuleFrame()) {
    return DebuggerFrameType::Module;
  }

  if (referent.isWasmDebugFrame()) {
    return DebuggerFrameType::WasmCall;
  }

  MOZ_CRASH("Unknown frame type");
}

/* static */
DebuggerFrameImplementation DebuggerFrame::getImplementation(
    HandleDebuggerFrame frame) {
  AbstractFramePtr referent = DebuggerFrame::getReferent(frame);

  if (referent.isBaselineFrame()) {
    return DebuggerFrameImplementation::Baseline;
  }

  if (referent.isRematerializedFrame()) {
    return DebuggerFrameImplementation::Ion;
  }

  if (referent.isWasmDebugFrame()) {
    return DebuggerFrameImplementation::Wasm;
  }

  return DebuggerFrameImplementation::Interpreter;
}

/*
 * If succesful, transfers the ownership of the given `handler` to this
 * Debugger.Frame. Note that on failure, the ownership of `handler` is not
 * transferred, and the caller is responsible for cleaning it up.
 */
/* static */
bool DebuggerFrame::setOnStepHandler(JSContext* cx, HandleDebuggerFrame frame,
                                     OnStepHandler* handler) {
  MOZ_ASSERT(frame->isLive());

  OnStepHandler* prior = frame->onStepHandler();
  if (handler == prior) {
    return true;
  }

  JSFreeOp* fop = cx->defaultFreeOp();
  AbstractFramePtr referent = DebuggerFrame::getReferent(frame);

  // Adjust execution observability and step counts on whatever code (JS or
  // Wasm) this frame is running.
  if (referent.isWasmDebugFrame()) {
    wasm::Instance* instance = referent.asWasmDebugFrame()->instance();
    wasm::DebugFrame* wasmFrame = referent.asWasmDebugFrame();
    if (handler && !prior) {
      // Single stepping toggled off->on.
      if (!instance->debug().incrementStepperCount(cx,
                                                   wasmFrame->funcIndex())) {
        return false;
      }
    } else if (!handler && prior) {
      // Single stepping toggled on->off.
      JSFreeOp* fop = cx->runtime()->defaultFreeOp();
      if (!instance->debug().decrementStepperCount(fop,
                                                   wasmFrame->funcIndex())) {
        return false;
      }
    }
  } else {
    if (handler && !prior) {
      // Single stepping toggled off->on.
      AutoRealm ar(cx, referent.environmentChain());
      // Ensure observability *before* incrementing the step mode count.
      // Calling this function after calling incrementStepperCount
      // will make it a no-op.
      if (!Debugger::ensureExecutionObservabilityOfScript(cx,
                                                          referent.script())) {
        return false;
      }
      if (!DebugScript::incrementStepperCount(cx, referent.script())) {
        return false;
      }
    } else if (!handler && prior) {
      // Single stepping toggled on->off.
      DebugScript::decrementStepperCount(cx->runtime()->defaultFreeOp(),
                                         referent.script());
    }
  }

  // Now that the stepper counts and observability are set correctly, we can
  // actually switch the handler.
  if (prior) {
    prior->drop(fop, frame);
  }

  if (handler) {
    frame->setReservedSlot(ONSTEP_HANDLER_SLOT, PrivateValue(handler));
    handler->hold(frame);
  } else {
    frame->setReservedSlot(ONSTEP_HANDLER_SLOT, UndefinedValue());
  }

  return true;
}

/* static */
bool DebuggerFrame::getArguments(JSContext* cx, HandleDebuggerFrame frame,
                                 MutableHandleDebuggerArguments result) {
  Value argumentsv = frame->getReservedSlot(ARGUMENTS_SLOT);
  if (!argumentsv.isUndefined()) {
    result.set(argumentsv.isObject()
                   ? &argumentsv.toObject().as<DebuggerArguments>()
                   : nullptr);
    return true;
  }

  AbstractFramePtr referent = DebuggerFrame::getReferent(frame);

  RootedDebuggerArguments arguments(cx);
  if (referent.hasArgs()) {
    Rooted<GlobalObject*> global(cx, &frame->global());
    RootedObject proto(cx, GlobalObject::getOrCreateArrayPrototype(cx, global));
    if (!proto) {
      return false;
    }
    arguments = DebuggerArguments::create(cx, proto, frame);
    if (!arguments) {
      return false;
    }
  } else {
    arguments = nullptr;
  }

  result.set(arguments);
  frame->setReservedSlot(ARGUMENTS_SLOT, ObjectOrNullValue(result));
  return true;
}

/*
 * Evaluate |chars[0..length-1]| in the environment |env|, treating that
 * source as appearing starting at |lineno| in |filename|. Store the return
 * value in |*rval|. Use |thisv| as the 'this' value.
 *
 * If |frame| is non-nullptr, evaluate as for a direct eval in that frame; |env|
 * must be either |frame|'s DebugScopeObject, or some extension of that
 * environment; either way, |frame|'s scope is where newly declared variables
 * go. In this case, |frame| must have a computed 'this' value, equal to
 * |thisv|.
 */
static bool EvaluateInEnv(JSContext* cx, Handle<Env*> env,
                          AbstractFramePtr frame,
                          mozilla::Range<const char16_t> chars,
                          const char* filename, unsigned lineno,
                          MutableHandleValue rval) {
  cx->check(env, frame);

  CompileOptions options(cx);
  options.setIsRunOnce(true)
      .setNoScriptRval(false)
      .setFileAndLine(filename, lineno)
      .setIntroductionType("debugger eval")
      .maybeMakeStrictMode(frame && frame.hasScript() ? frame.script()->strict()
                                                      : false);

  SourceText<char16_t> srcBuf;
  if (!srcBuf.init(cx, chars.begin().get(), chars.length(),
                   SourceOwnership::Borrowed)) {
    return false;
  }

  RootedScript callerScript(
      cx, frame && frame.hasScript() ? frame.script() : nullptr);
  RootedScript script(cx);

  ScopeKind scopeKind;
  if (IsGlobalLexicalEnvironment(env)) {
    scopeKind = ScopeKind::Global;
  } else {
    scopeKind = ScopeKind::NonSyntactic;
  }

  if (frame) {
    MOZ_ASSERT(scopeKind == ScopeKind::NonSyntactic);
    RootedScope scope(cx,
                      GlobalScope::createEmpty(cx, ScopeKind::NonSyntactic));
    if (!scope) {
      return false;
    }

    frontend::EvalScriptInfo info(cx, options, env, scope);
    script = frontend::CompileEvalScript(info, srcBuf);
    if (!script) {
      return false;
    }
  } else {
    // Do not consider executeInGlobal{WithBindings} as an eval, but instead
    // as executing a series of statements at the global level. This is to
    // circumvent the fresh lexical scope that all eval have, so that the
    // users of executeInGlobal, like the web console, may add new bindings to
    // the global scope.
    frontend::GlobalScriptInfo info(cx, options, scopeKind);
    script = frontend::CompileGlobalScript(info, srcBuf);
    if (!script) {
      return false;
    }
  }

  return ExecuteKernel(cx, script, *env, NullValue(), frame, rval.address());
}

Result<Completion> js::DebuggerGenericEval(
    JSContext* cx, const mozilla::Range<const char16_t> chars,
    HandleObject bindings, const EvalOptions& options, Debugger* dbg,
    HandleObject envArg, FrameIter* iter) {
  // Either we're specifying the frame, or a global.
  MOZ_ASSERT_IF(iter, !envArg);
  MOZ_ASSERT_IF(!iter, envArg && IsGlobalLexicalEnvironment(envArg));

  // Gather keys and values of bindings, if any. This must be done in the
  // debugger compartment, since that is where any exceptions must be thrown.
  RootedIdVector keys(cx);
  RootedValueVector values(cx);
  if (bindings) {
    if (!GetPropertyKeys(cx, bindings, JSITER_OWNONLY, &keys) ||
        !values.growBy(keys.length())) {
      return cx->alreadyReportedError();
    }
    for (size_t i = 0; i < keys.length(); i++) {
      MutableHandleValue valp = values[i];
      if (!GetProperty(cx, bindings, bindings, keys[i], valp) ||
          !dbg->unwrapDebuggeeValue(cx, valp)) {
        return cx->alreadyReportedError();
      }
    }
  }

  Maybe<AutoRealm> ar;
  if (iter) {
    ar.emplace(cx, iter->environmentChain(cx));
  } else {
    ar.emplace(cx, envArg);
  }

  Rooted<Env*> env(cx);
  if (iter) {
    env = GetDebugEnvironmentForFrame(cx, iter->abstractFramePtr(), iter->pc());
    if (!env) {
      return cx->alreadyReportedError();
    }
  } else {
    env = envArg;
  }

  // If evalWithBindings, create the inner environment.
  if (bindings) {
    RootedPlainObject nenv(cx,
                           NewObjectWithGivenProto<PlainObject>(cx, nullptr));
    if (!nenv) {
      return cx->alreadyReportedError();
    }
    RootedId id(cx);
    for (size_t i = 0; i < keys.length(); i++) {
      id = keys[i];
      cx->markId(id);
      MutableHandleValue val = values[i];
      if (!cx->compartment()->wrap(cx, val) ||
          !NativeDefineDataProperty(cx, nenv, id, val, 0)) {
        return cx->alreadyReportedError();
      }
    }

    RootedObjectVector envChain(cx);
    if (!envChain.append(nenv)) {
      return cx->alreadyReportedError();
    }

    RootedObject newEnv(cx);
    if (!CreateObjectsForEnvironmentChain(cx, envChain, env, &newEnv)) {
      return cx->alreadyReportedError();
    }

    env = newEnv;
  }

  // Run the code and produce the completion value.
  LeaveDebuggeeNoExecute nnx(cx);
  RootedValue rval(cx);
  AbstractFramePtr frame = iter ? iter->abstractFramePtr() : NullFramePtr();

  bool ok = EvaluateInEnv(
      cx, env, frame, chars,
      options.filename() ? options.filename() : "debugger eval code",
      options.lineno(), &rval);
  Rooted<Completion> completion(cx, Completion::fromJSResult(cx, ok, rval));
  ar.reset();
  return completion.get();
}

/* static */
Result<Completion> DebuggerFrame::eval(JSContext* cx, HandleDebuggerFrame frame,
                                       mozilla::Range<const char16_t> chars,
                                       HandleObject bindings,
                                       const EvalOptions& options) {
  MOZ_ASSERT(frame->isLive());

  Debugger* dbg = frame->owner();

  Maybe<FrameIter> maybeIter;
  if (!DebuggerFrame::getFrameIter(cx, frame, maybeIter)) {
    return cx->alreadyReportedError();
  }
  FrameIter& iter = *maybeIter;

  UpdateFrameIterPc(iter);

  return DebuggerGenericEval(cx, chars, bindings, options, dbg, nullptr, &iter);
}

/* static */
bool DebuggerFrame::isLive() const { return !!getPrivate(); }

OnStepHandler* DebuggerFrame::onStepHandler() const {
  Value value = getReservedSlot(ONSTEP_HANDLER_SLOT);
  return value.isUndefined() ? nullptr
                             : static_cast<OnStepHandler*>(value.toPrivate());
}

OnPopHandler* DebuggerFrame::onPopHandler() const {
  Value value = getReservedSlot(ONPOP_HANDLER_SLOT);
  return value.isUndefined() ? nullptr
                             : static_cast<OnPopHandler*>(value.toPrivate());
}

void DebuggerFrame::setOnPopHandler(JSContext* cx, OnPopHandler* handler) {
  MOZ_ASSERT(isLive());

  OnPopHandler* prior = onPopHandler();
  if (handler == prior) {
    return;
  }

  JSFreeOp* fop = cx->defaultFreeOp();

  if (prior) {
    prior->drop(fop, this);
  }

  if (handler) {
    setReservedSlot(ONPOP_HANDLER_SLOT, PrivateValue(handler));
    handler->hold(this);
  } else {
    setReservedSlot(ONPOP_HANDLER_SLOT, UndefinedValue());
  }
}

bool DebuggerFrame::requireLive(JSContext* cx) {
  if (!isLive()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_NOT_LIVE, "Debugger.Frame");
    return false;
  }

  return true;
}

FrameIter::Data* DebuggerFrame::frameIterData() const {
  return static_cast<FrameIter::Data*>(getPrivate());
}

/* static */
AbstractFramePtr DebuggerFrame::getReferent(HandleDebuggerFrame frame) {
  FrameIter iter(*frame->frameIterData());
  return iter.abstractFramePtr();
}

/* static */
bool DebuggerFrame::getFrameIter(JSContext* cx, HandleDebuggerFrame frame,
                                 Maybe<FrameIter>& result) {
  result.emplace(*frame->frameIterData());
  return true;
}

/* static */
bool DebuggerFrame::requireScriptReferent(JSContext* cx,
                                          HandleDebuggerFrame frame) {
  AbstractFramePtr referent = DebuggerFrame::getReferent(frame);
  if (!referent.hasScript()) {
    RootedValue frameobj(cx, ObjectValue(*frame));
    ReportValueError(cx, JSMSG_DEBUG_BAD_REFERENT, JSDVG_SEARCH_STACK, frameobj,
                     nullptr, "a script frame");
    return false;
  }
  return true;
}

void DebuggerFrame::setFrameIterData(FrameIter::Data* data) {
  MOZ_ASSERT(data);
  MOZ_ASSERT(!frameIterData());
  InitObjectPrivate(this, data, MemoryUse::DebuggerFrameIterData);
}

void DebuggerFrame::freeFrameIterData(JSFreeOp* fop) {
  if (FrameIter::Data* data = frameIterData()) {
    fop->delete_(this, data, MemoryUse::DebuggerFrameIterData);
    setPrivate(nullptr);
  }
}

void DebuggerFrame::maybeDecrementFrameScriptStepperCount(
    JSFreeOp* fop, AbstractFramePtr frame) {
  // If this frame has an onStep handler, decrement the script's count.
  OnStepHandler* handler = onStepHandler();
  if (!handler) {
    return;
  }

  if (frame.isWasmDebugFrame()) {
    wasm::Instance* instance = frame.wasmInstance();
    instance->debug().decrementStepperCount(
        fop, frame.asWasmDebugFrame()->funcIndex());
  } else {
    DebugScript::decrementStepperCount(fop, frame.script());
  }

  // In the case of generator frames, we may end up trying to clean up the step
  // count in more than one place, so make this method idempotent.
  handler->drop(fop, this);
  setReservedSlot(ONSTEP_HANDLER_SLOT, UndefinedValue());
}

/* static */
void DebuggerFrame::finalize(JSFreeOp* fop, JSObject* obj) {
  MOZ_ASSERT(fop->onMainThread());
  DebuggerFrame& frameobj = obj->as<DebuggerFrame>();
  frameobj.freeFrameIterData(fop);
  frameobj.clearGenerator(fop);
  OnStepHandler* onStepHandler = frameobj.onStepHandler();
  if (onStepHandler) {
    onStepHandler->drop(fop, &frameobj);
  }
  OnPopHandler* onPopHandler = frameobj.onPopHandler();
  if (onPopHandler) {
    onPopHandler->drop(fop, &frameobj);
  }
}

void DebuggerFrame::trace(JSTracer* trc) {
  OnStepHandler* onStepHandler = this->onStepHandler();
  if (onStepHandler) {
    onStepHandler->trace(trc);
  }
  OnPopHandler* onPopHandler = this->onPopHandler();
  if (onPopHandler) {
    onPopHandler->trace(trc);
  }

  if (hasGenerator()) {
    generatorInfo()->trace(trc, *this);
  }
}

/* static */
DebuggerFrame* DebuggerFrame::checkThis(JSContext* cx, const CallArgs& args,
                                        const char* fnname, bool checkLive) {
  JSObject* thisobj = RequireObject(cx, args.thisv());
  if (!thisobj) {
    return nullptr;
  }
  if (thisobj->getClass() != &class_) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INCOMPATIBLE_PROTO, "Debugger.Frame",
                              fnname, thisobj->getClass()->name);
    return nullptr;
  }

  RootedDebuggerFrame frame(cx, &thisobj->as<DebuggerFrame>());

  // Forbid Debugger.Frame.prototype, which is of class DebuggerFrame::class_
  // but isn't really a working Debugger.Frame object. The prototype object
  // is distinguished by having a nullptr private value. Also, forbid popped
  // frames.
  if (!frame->getPrivate() &&
      frame->getReservedSlot(OWNER_SLOT).isUndefined()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INCOMPATIBLE_PROTO, "Debugger.Frame",
                              fnname, "prototype object");
    return nullptr;
  }

  if (checkLive) {
    if (!frame->requireLive(cx)) {
      return nullptr;
    }
  }

  return frame;
}

/*
 * Methods can use THIS_DEBUGGER_FRAME to check that `this` is a Debugger.Frame
 * object and get it in a local Rooted.
 *
 * Methods that need the AbstractFramePtr should use THIS_FRAME.
 */
#define THIS_DEBUGGER_FRAME(cx, argc, vp, fnname, args, frame)                 \
  CallArgs args = CallArgsFromVp(argc, vp);                                    \
  RootedDebuggerFrame frame(cx,                                                \
                            DebuggerFrame::checkThis(cx, args, fnname, true)); \
  if (!frame) return false;

#define THIS_FRAME(cx, argc, vp, fnname, args, thisobj, iter, frame) \
  THIS_DEBUGGER_FRAME(cx, argc, vp, fnname, args, thisobj);          \
  FrameIter iter(*thisobj->frameIterData());                         \
  AbstractFramePtr frame = iter.abstractFramePtr()

/* static */
bool DebuggerFrame::typeGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "get type", args, frame);

  DebuggerFrameType type = DebuggerFrame::getType(frame);

  JSString* str;
  switch (type) {
    case DebuggerFrameType::Eval:
      str = cx->names().eval;
      break;
    case DebuggerFrameType::Global:
      str = cx->names().global;
      break;
    case DebuggerFrameType::Call:
      str = cx->names().call;
      break;
    case DebuggerFrameType::Module:
      str = cx->names().module;
      break;
    case DebuggerFrameType::WasmCall:
      str = cx->names().wasmcall;
      break;
    default:
      MOZ_CRASH("bad DebuggerFrameType value");
  }

  args.rval().setString(str);
  return true;
}

/* static */
bool DebuggerFrame::implementationGetter(JSContext* cx, unsigned argc,
                                         Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "get implementation", args, frame);

  DebuggerFrameImplementation implementation =
      DebuggerFrame::getImplementation(frame);

  const char* s;
  switch (implementation) {
    case DebuggerFrameImplementation::Baseline:
      s = "baseline";
      break;
    case DebuggerFrameImplementation::Ion:
      s = "ion";
      break;
    case DebuggerFrameImplementation::Interpreter:
      s = "interpreter";
      break;
    case DebuggerFrameImplementation::Wasm:
      s = "wasm";
      break;
    default:
      MOZ_CRASH("bad DebuggerFrameImplementation value");
  }

  JSAtom* str = Atomize(cx, s, strlen(s));
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/* static */
bool DebuggerFrame::environmentGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "get environment", args, frame);

  RootedDebuggerEnvironment result(cx);
  if (!DebuggerFrame::getEnvironment(cx, frame, &result)) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/* static */
bool DebuggerFrame::calleeGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "get callee", args, frame);

  RootedDebuggerObject result(cx);
  if (!DebuggerFrame::getCallee(cx, frame, &result)) {
    return false;
  }

  args.rval().setObjectOrNull(result);
  return true;
}

/* static */
bool DebuggerFrame::generatorGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "get callee", args, frame);

  args.rval().setBoolean(DebuggerFrame::getIsGenerator(frame));
  return true;
}

/* static */
bool DebuggerFrame::constructingGetter(JSContext* cx, unsigned argc,
                                       Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "get callee", args, frame);

  bool result;
  if (!DebuggerFrame::getIsConstructing(cx, frame, result)) {
    return false;
  }

  args.rval().setBoolean(result);
  return true;
}

/* static */
bool DebuggerFrame::thisGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "get this", args, frame);

  return DebuggerFrame::getThis(cx, frame, args.rval());
}

/* static */
bool DebuggerFrame::olderGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "get older", args, frame);

  RootedDebuggerFrame result(cx);
  if (!DebuggerFrame::getOlder(cx, frame, &result)) {
    return false;
  }

  args.rval().setObjectOrNull(result);
  return true;
}

// The getter used for each element of frame.arguments.
// See DebuggerFrame::getArguments.
static bool DebuggerArguments_getArg(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  int32_t i = args.callee().as<JSFunction>().getExtendedSlot(0).toInt32();

  // Check that the this value is an Arguments object.
  RootedObject argsobj(cx, RequireObject(cx, args.thisv()));
  if (!argsobj) {
    return false;
  }
  if (argsobj->getClass() != &DebuggerArguments::class_) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INCOMPATIBLE_PROTO, "Arguments",
                              "getArgument", argsobj->getClass()->name);
    return false;
  }

  // Put the Debugger.Frame into the this-value slot, then use THIS_FRAME
  // to check that it is still live and get the fp.
  args.setThis(
      argsobj->as<NativeObject>().getReservedSlot(JSSLOT_DEBUGARGUMENTS_FRAME));
  THIS_FRAME(cx, argc, vp, "get argument", ca2, thisobj, frameIter, frame);

  // TODO handle wasm frame arguments -- they are not yet reflectable.
  MOZ_ASSERT(!frame.isWasmDebugFrame(), "a wasm frame args");

  // Since getters can be extracted and applied to other objects,
  // there is no guarantee this object has an ith argument.
  MOZ_ASSERT(i >= 0);
  RootedValue arg(cx);
  RootedScript script(cx);
  if (unsigned(i) < frame.numActualArgs()) {
    script = frame.script();
    {
      AutoRealm ar(cx, script);
      if (!script->ensureHasAnalyzedArgsUsage(cx)) {
        return false;
      }
    }
    if (unsigned(i) < frame.numFormalArgs()) {
      for (PositionalFormalParameterIter fi(script); fi; fi++) {
        if (fi.argumentSlot() == unsigned(i)) {
          // We might've been called before the CallObject was
          // created.
          if (fi.closedOver() && frame.hasInitialEnvironment()) {
            arg = frame.callObj().aliasedBinding(fi);
          } else {
            arg = frame.unaliasedActual(i, DONT_CHECK_ALIASING);
          }
          break;
        }
      }
    } else if (script->argsObjAliasesFormals() && frame.hasArgsObj()) {
      arg = frame.argsObj().arg(i);
    } else {
      arg = frame.unaliasedActual(i, DONT_CHECK_ALIASING);
    }
  } else {
    arg.setUndefined();
  }

  if (!Debugger::fromChildJSObject(thisobj)->wrapDebuggeeValue(cx, &arg)) {
    return false;
  }
  args.rval().set(arg);
  return true;
}

/* static */
DebuggerArguments* DebuggerArguments::create(JSContext* cx, HandleObject proto,
                                             HandleDebuggerFrame frame) {
  AbstractFramePtr referent = DebuggerFrame::getReferent(frame);

  Rooted<DebuggerArguments*> obj(
      cx, NewObjectWithGivenProto<DebuggerArguments>(cx, proto));
  if (!obj) {
    return nullptr;
  }

  SetReservedSlot(obj, FRAME_SLOT, ObjectValue(*frame));

  MOZ_ASSERT(referent.numActualArgs() <= 0x7fffffff);
  unsigned fargc = referent.numActualArgs();
  RootedValue fargcVal(cx, Int32Value(fargc));
  if (!NativeDefineDataProperty(cx, obj, cx->names().length, fargcVal,
                                JSPROP_PERMANENT | JSPROP_READONLY)) {
    return nullptr;
  }

  Rooted<jsid> id(cx);
  for (unsigned i = 0; i < fargc; i++) {
    RootedFunction getobj(cx);
    getobj = NewNativeFunction(cx, DebuggerArguments_getArg, 0, nullptr,
                               gc::AllocKind::FUNCTION_EXTENDED);
    if (!getobj) {
      return nullptr;
    }
    id = INT_TO_JSID(i);
    if (!NativeDefineAccessorProperty(cx, obj, id, getobj, nullptr,
                                      JSPROP_ENUMERATE | JSPROP_GETTER)) {
      return nullptr;
    }
    getobj->setExtendedSlot(0, Int32Value(i));
  }

  return obj;
}

/* static */
bool DebuggerFrame::argumentsGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "get arguments", args, frame);

  RootedDebuggerArguments result(cx);
  if (!DebuggerFrame::getArguments(cx, frame, &result)) {
    return false;
  }

  args.rval().setObjectOrNull(result);
  return true;
}

/* static */
bool DebuggerFrame::getScript(JSContext* cx, unsigned argc, Value* vp) {
  THIS_FRAME(cx, argc, vp, "get script", args, thisobj, frameIter, frame);
  Debugger* debug = Debugger::fromChildJSObject(thisobj);

  RootedDebuggerScript scriptObject(cx);
  if (frame.isWasmDebugFrame()) {
    RootedWasmInstanceObject instance(cx, frame.wasmInstance()->object());
    scriptObject = debug->wrapWasmScript(cx, instance);
    if (!scriptObject) {
      return false;
    }
  } else {
    RootedScript script(cx, frame.script());
    scriptObject = debug->wrapScript(cx, script);
    if (!scriptObject) {
      return false;
    }
  }

  MOZ_ASSERT(scriptObject);
  args.rval().setObject(*scriptObject);
  return true;
}

/* static */
bool DebuggerFrame::offsetGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "get offset", args, frame);

  size_t result;
  if (!DebuggerFrame::getOffset(cx, frame, result)) {
    return false;
  }

  args.rval().setNumber(double(result));
  return true;
}

/* static */
bool DebuggerFrame::liveGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedDebuggerFrame frame(cx, checkThis(cx, args, "get live", false));
  if (!frame) {
    return false;
  }

  args.rval().setBoolean(frame->isLive());
  return true;
}

static bool IsValidHook(const Value& v) {
  return v.isUndefined() || (v.isObject() && v.toObject().isCallable());
}

/* static */
bool DebuggerFrame::onStepGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "get onStep", args, frame);

  OnStepHandler* handler = frame->onStepHandler();
  RootedValue value(
      cx, handler ? ObjectOrNullValue(handler->object()) : UndefinedValue());
  MOZ_ASSERT(IsValidHook(value));
  args.rval().set(value);
  return true;
}

/* static */
bool DebuggerFrame::onStepSetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "set onStep", args, frame);
  if (!args.requireAtLeast(cx, "Debugger.Frame.set onStep", 1)) {
    return false;
  }
  if (!IsValidHook(args[0])) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_CALLABLE_OR_UNDEFINED);
    return false;
  }

  ScriptedOnStepHandler* handler = nullptr;
  if (!args[0].isUndefined()) {
    handler = cx->new_<ScriptedOnStepHandler>(&args[0].toObject());
    if (!handler) {
      return false;
    }
  }

  if (!DebuggerFrame::setOnStepHandler(cx, frame, handler)) {
    // Handler has never been successfully associated with the frame so just
    // delete it rather than calling drop().
    js_delete(handler);
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/* static */
bool DebuggerFrame::onPopGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "get onPop", args, frame);

  OnPopHandler* handler = frame->onPopHandler();
  RootedValue value(
      cx, handler ? ObjectValue(*handler->object()) : UndefinedValue());
  MOZ_ASSERT(IsValidHook(value));
  args.rval().set(value);
  return true;
}

/* static */
bool DebuggerFrame::onPopSetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "set onPop", args, frame);
  if (!args.requireAtLeast(cx, "Debugger.Frame.set onPop", 1)) {
    return false;
  }
  if (!IsValidHook(args[0])) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_CALLABLE_OR_UNDEFINED);
    return false;
  }

  ScriptedOnPopHandler* handler = nullptr;
  if (!args[0].isUndefined()) {
    handler = cx->new_<ScriptedOnPopHandler>(&args[0].toObject());
    if (!handler) {
      return false;
    }
  }

  frame->setOnPopHandler(cx, handler);

  args.rval().setUndefined();
  return true;
}

/* static */
bool DebuggerFrame::evalMethod(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "eval", args, frame);
  if (!args.requireAtLeast(cx, "Debugger.Frame.prototype.eval", 1)) {
    return false;
  }

  AutoStableStringChars stableChars(cx);
  if (!ValueToStableChars(cx, "Debugger.Frame.prototype.eval", args[0],
                          stableChars)) {
    return false;
  }
  mozilla::Range<const char16_t> chars = stableChars.twoByteRange();

  EvalOptions options;
  if (!ParseEvalOptions(cx, args.get(1), options)) {
    return false;
  }

  Rooted<Completion> comp(cx);
  JS_TRY_VAR_OR_RETURN_FALSE(
      cx, comp, DebuggerFrame::eval(cx, frame, chars, nullptr, options));
  return comp.get().buildCompletionValue(cx, frame->owner(), args.rval());
}

/* static */
bool DebuggerFrame::evalWithBindingsMethod(JSContext* cx, unsigned argc,
                                           Value* vp) {
  THIS_DEBUGGER_FRAME(cx, argc, vp, "evalWithBindings", args, frame);
  if (!args.requireAtLeast(cx, "Debugger.Frame.prototype.evalWithBindings",
                           2)) {
    return false;
  }

  AutoStableStringChars stableChars(cx);
  if (!ValueToStableChars(cx, "Debugger.Frame.prototype.evalWithBindings",
                          args[0], stableChars)) {
    return false;
  }
  mozilla::Range<const char16_t> chars = stableChars.twoByteRange();

  RootedObject bindings(cx, RequireObject(cx, args[1]));
  if (!bindings) {
    return false;
  }

  EvalOptions options;
  if (!ParseEvalOptions(cx, args.get(2), options)) {
    return false;
  }

  Rooted<Completion> comp(cx);
  JS_TRY_VAR_OR_RETURN_FALSE(
      cx, comp, DebuggerFrame::eval(cx, frame, chars, bindings, options));
  return comp.get().buildCompletionValue(cx, frame->owner(), args.rval());
}

/* static */
bool DebuggerFrame::construct(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NO_CONSTRUCTOR,
                            "Debugger.Frame");
  return false;
}

const JSPropertySpec DebuggerFrame::properties_[] = {
    JS_PSG("arguments", DebuggerFrame::argumentsGetter, 0),
    JS_PSG("callee", DebuggerFrame::calleeGetter, 0),
    JS_PSG("constructing", DebuggerFrame::constructingGetter, 0),
    JS_PSG("environment", DebuggerFrame::environmentGetter, 0),
    JS_PSG("generator", DebuggerFrame::generatorGetter, 0),
    JS_PSG("live", DebuggerFrame::liveGetter, 0),
    JS_PSG("offset", DebuggerFrame::offsetGetter, 0),
    JS_PSG("older", DebuggerFrame::olderGetter, 0),
    JS_PSG("script", DebuggerFrame::getScript, 0),
    JS_PSG("this", DebuggerFrame::thisGetter, 0),
    JS_PSG("type", DebuggerFrame::typeGetter, 0),
    JS_PSG("implementation", DebuggerFrame::implementationGetter, 0),
    JS_PSGS("onStep", DebuggerFrame::onStepGetter, DebuggerFrame::onStepSetter,
            0),
    JS_PSGS("onPop", DebuggerFrame::onPopGetter, DebuggerFrame::onPopSetter, 0),
    JS_PS_END};

const JSFunctionSpec DebuggerFrame::methods_[] = {
    JS_FN("eval", DebuggerFrame::evalMethod, 1, 0),
    JS_FN("evalWithBindings", DebuggerFrame::evalWithBindingsMethod, 1, 0),
    JS_FS_END};

JSObject* js::IdVectorToArray(JSContext* cx, Handle<IdVector> ids) {
  Rooted<ValueVector> vals(cx, ValueVector(cx));
  if (!vals.growBy(ids.length())) {
    return nullptr;
  }

  for (size_t i = 0, len = ids.length(); i < len; i++) {
    jsid id = ids[i];
    if (JSID_IS_INT(id)) {
      JSString* str = Int32ToString<CanGC>(cx, JSID_TO_INT(id));
      if (!str) {
        return nullptr;
      }
      vals[i].setString(str);
    } else if (JSID_IS_ATOM(id)) {
      vals[i].setString(JSID_TO_STRING(id));
    } else if (JSID_IS_SYMBOL(id)) {
      vals[i].setSymbol(JSID_TO_SYMBOL(id));
    } else {
      MOZ_ASSERT_UNREACHABLE(
          "IdVector must contain only string, int, and Symbol jsids");
    }
  }

  return NewDenseCopiedArray(cx, vals.length(), vals.begin());
}
