/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "debugger/Frame-inl.h"

#include "mozilla/Assertions.h"   // for AssertionConditionType
#include "mozilla/HashTable.h"    // for HashMapEntry
#include "mozilla/Maybe.h"        // for Maybe
#include "mozilla/Range.h"        // for Range
#include "mozilla/RangedPtr.h"    // for RangedPtr
#include "mozilla/Result.h"       // for Result
#include "mozilla/ScopeExit.h"    // for MakeScopeExit, ScopeExit
#include "mozilla/ThreadLocal.h"  // for ThreadLocal
#include "mozilla/Vector.h"       // for Vector

#include <stddef.h>  // for size_t
#include <stdint.h>  // for int32_t
#include <string.h>  // for strlen
#include <utility>   // for std::move

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
#include "frontend/SharedContext.h"        // for GlobalScharedContext
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
#include "vm/AsyncFunction.h"              // for AsyncFunctionGeneratorObject
#include "vm/AsyncIteration.h"             // for AsyncGeneratorObject
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
#include "gc/WeakMap-inl.h"         // for WeakMap::remove
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

inline bool js::DebuggerFrame::hasIncrementedStepper() const {
  return getReservedSlot(HAS_INCREMENTED_STEPPER_SLOT).toBoolean();
}
inline void js::DebuggerFrame::setHasIncrementedStepper(bool incremented) {
  setReservedSlot(HAS_INCREMENTED_STEPPER_SLOT, BooleanValue(incremented));
}

const JSClassOps DebuggerFrame::classOps_ = {
    nullptr,                         // addProperty
    nullptr,                         // delProperty
    nullptr,                         // enumerate
    nullptr,                         // newEnumerate
    nullptr,                         // resolve
    nullptr,                         // mayResolve
    finalize,                        // finalize
    nullptr,                         // call
    nullptr,                         // hasInstance
    nullptr,                         // construct
    CallTraceMethod<DebuggerFrame>,  // trace
};

const JSClass DebuggerFrame::class_ = {
    "Frame",
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS) |
        // We require foreground finalization so we can destruct GeneratorInfo's
        // HeapPtrs.
        JSCLASS_FOREGROUND_FINALIZE,
    &DebuggerFrame::classOps_};

enum { JSSLOT_DEBUGARGUMENTS_FRAME, JSSLOT_DEBUGARGUMENTS_COUNT };

const JSClass DebuggerArguments::class_ = {
    "Arguments", JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGARGUMENTS_COUNT)};

bool DebuggerFrame::resume(const FrameIter& iter) {
  FrameIter::Data* data = iter.copyData();
  if (!data) {
    return false;
  }
  setFrameIterData(data);
  return true;
}

bool DebuggerFrame::hasAnyHooks() const {
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
DebuggerFrame* DebuggerFrame::create(
    JSContext* cx, HandleObject proto, HandleNativeObject debugger,
    const FrameIter* maybeIter,
    Handle<AbstractGeneratorObject*> maybeGenerator) {
  DebuggerFrame* frame = NewObjectWithGivenProto<DebuggerFrame>(cx, proto);
  if (!frame) {
    return nullptr;
  }

  frame->setReservedSlot(OWNER_SLOT, ObjectValue(*debugger));
  frame->setReservedSlot(HAS_INCREMENTED_STEPPER_SLOT, BooleanValue(false));

  if (maybeIter) {
    FrameIter::Data* data = maybeIter->copyData();
    if (!data) {
      return nullptr;
    }

    frame->setFrameIterData(data);
  }

  if (maybeGenerator) {
    if (!frame->setGenerator(cx, maybeGenerator)) {
      return nullptr;
    }
  }

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
  const HeapPtr<Value> unwrappedGenerator_;

  // A cross-compartment reference to the generator's script.
  const HeapPtr<JSScript*> generatorScript_;

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

  JSScript* generatorScript() { return generatorScript_; }

  bool isGeneratorScriptAboutToBeFinalized() {
    return IsAboutToBeFinalized(&generatorScript_);
  }
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

    // All frames running a debuggee script must themselves be marked as
    // debuggee frames. Bumping a script's generator observer count makes it a
    // debuggee, so we need to mark all frames on the stack running it as
    // debuggees as well, not just this one. This call takes care of all that.
    if (!Debugger::ensureExecutionObservabilityOfScript(cx, script)) {
      return false;
    }

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

  // 3) The generator's script's observer count must be dropped.
  //
  // For ordinary calls, Debugger.Frame objects drop the script's stepper count
  // when the frame is popped, but for generators, they leave the stepper count
  // incremented across suspensions. This means that, whereas ordinary calls
  // never need to drop the stepper count from the D.F finalizer, generator
  // calls may.
  if (!info->isGeneratorScriptAboutToBeFinalized()) {
    JSScript* generatorScript = info->generatorScript();
    DebugScript::decrementGeneratorObserverCount(fop, generatorScript);
    maybeDecrementStepperCounter(fop, generatorScript);
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
  RootedObject callee(cx);
  if (frame->isOnStack()) {
    AbstractFramePtr referent = DebuggerFrame::getReferent(frame);
    if (referent.isFunctionFrame()) {
      callee = referent.callee();
    }
  } else {
    MOZ_ASSERT(frame->hasGenerator());

    callee = &frame->generatorInfo()->unwrappedGenerator().callee();
  }

  return frame->owner()->wrapNullableDebuggeeObject(cx, callee, result);
}

/* static */
bool DebuggerFrame::getIsConstructing(JSContext* cx, HandleDebuggerFrame frame,
                                      bool& result) {
  if (frame->isOnStack()) {
    Maybe<FrameIter> maybeIter;
    if (!DebuggerFrame::getFrameIter(cx, frame, maybeIter)) {
      return false;
    }
    FrameIter& iter = *maybeIter;

    result = iter.isFunctionFrame() && iter.isConstructing();
  } else {
    MOZ_ASSERT(frame->hasGenerator());

    // Generators and async functions can't be constructed.
    result = false;
  }
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
  Debugger* dbg = frame->owner();
  Rooted<Env*> env(cx);

  if (frame->isOnStack()) {
    Maybe<FrameIter> maybeIter;
    if (!DebuggerFrame::getFrameIter(cx, frame, maybeIter)) {
      return false;
    }
    FrameIter& iter = *maybeIter;

    {
      AutoRealm ar(cx, iter.abstractFramePtr().environmentChain());
      UpdateFrameIterPc(iter);
      env = GetDebugEnvironmentForFrame(cx, iter.abstractFramePtr(), iter.pc());
    }
  } else {
    MOZ_ASSERT(frame->hasGenerator());

    AbstractGeneratorObject& genObj =
        frame->generatorInfo()->unwrappedGenerator();
    JSScript* script = frame->generatorInfo()->generatorScript();

    {
      AutoRealm ar(cx, &genObj.environmentChain());
      env = GetDebugEnvironmentForSuspendedGenerator(cx, script, genObj);
    }
  }

  if (!env) {
    return false;
  }

  return dbg->wrapEnvironment(cx, env, result);
}

/* static */
bool DebuggerFrame::getOffset(JSContext* cx, HandleDebuggerFrame frame,
                              size_t& result) {
  if (frame->isOnStack()) {
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
  } else {
    MOZ_ASSERT(frame->hasGenerator());

    AbstractGeneratorObject& genObj =
        frame->generatorInfo()->unwrappedGenerator();
    JSScript* script = frame->generatorInfo()->generatorScript();
    result = script->resumeOffsets()[genObj.resumeIndex()];
  }
  return true;
}

/* static */
bool DebuggerFrame::getOlder(JSContext* cx, HandleDebuggerFrame frame,
                             MutableHandleDebuggerFrame result) {
  if (frame->isOnStack()) {
    Debugger* dbg = frame->owner();

    Maybe<FrameIter> maybeIter;
    if (!DebuggerFrame::getFrameIter(cx, frame, maybeIter)) {
      return false;
    }
    FrameIter& iter = *maybeIter;

    while (true) {
      Activation& activation = *iter.activation();
      ++iter;

      // If the parent frame crosses an explicit async stack boundary, we
      // treat that as an indication to stop traversing sync frames, so that
      // the on-stack Debugger.Frame instances align with what you would
      // see in a stringified stack trace.
      if (iter.activation() != &activation && activation.asyncStack() &&
          activation.asyncCallIsExplicit()) {
        break;
      }

      // If there is no parent frame, we're done.
      if (iter.done()) {
        break;
      }

      if (dbg->observesFrame(iter)) {
        if (iter.isIon() && !iter.ensureHasRematerializedFrame(cx)) {
          return false;
        }
        return dbg->getFrame(cx, iter, result);
      }
    }
  } else {
    MOZ_ASSERT(frame->hasGenerator());

    // If the frame is suspended, there is no older frame.
  }

  result.set(nullptr);
  return true;
}

/* static */
bool DebuggerFrame::getAsyncPromise(JSContext* cx, HandleDebuggerFrame frame,
                                    MutableHandleDebuggerObject result) {
  if (!frame->hasGenerator()) {
    result.set(nullptr);
    return true;
  }

  RootedObject resultObject(cx);
  AbstractGeneratorObject& generator = frame->unwrappedGenerator();
  if (generator.is<AsyncFunctionGeneratorObject>()) {
    resultObject = generator.as<AsyncFunctionGeneratorObject>().promise();
  } else if (generator.is<AsyncGeneratorObject>()) {
    Rooted<AsyncGeneratorObject*> asyncGen(
        cx, &generator.as<AsyncGeneratorObject>());
    // In initial function execution, there is no promise.
    if (!asyncGen->isQueueEmpty()) {
      resultObject = AsyncGeneratorObject::peekRequest(asyncGen)->promise();
    }
  } else {
    MOZ_CRASH("Unknown async generator type");
  }

  return frame->owner()->wrapNullableDebuggeeObject(cx, resultObject, result);
}

/* static */
bool DebuggerFrame::getThis(JSContext* cx, HandleDebuggerFrame frame,
                            MutableHandleValue result) {
  Debugger* dbg = frame->owner();

  if (frame->isOnStack()) {
    if (!requireScriptReferent(cx, frame)) {
      return false;
    }

    Maybe<FrameIter> maybeIter;
    if (!DebuggerFrame::getFrameIter(cx, frame, maybeIter)) {
      return false;
    }
    FrameIter& iter = *maybeIter;

    {
      AbstractFramePtr frame = iter.abstractFramePtr();
      AutoRealm ar(cx, frame.environmentChain());

      UpdateFrameIterPc(iter);

      if (!GetThisValueForDebuggerFrameMaybeOptimizedOut(cx, frame, iter.pc(),
                                                         result)) {
        return false;
      }
    }
  } else {
    MOZ_ASSERT(frame->hasGenerator());

    AbstractGeneratorObject& genObj =
        frame->generatorInfo()->unwrappedGenerator();
    JSScript* script = frame->generatorInfo()->generatorScript();

    if (!GetThisValueForDebuggerSuspendedGeneratorMaybeOptimizedOut(
            cx, genObj, script, result)) {
      return false;
    }
  }

  return dbg->wrapDebuggeeValue(cx, result);
}

/* static */
DebuggerFrameType DebuggerFrame::getType(HandleDebuggerFrame frame) {
  if (frame->isOnStack()) {
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
  } else {
    MOZ_ASSERT(frame->hasGenerator());

    return DebuggerFrameType::Call;
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
  OnStepHandler* prior = frame->onStepHandler();
  if (handler == prior) {
    return true;
  }

  JSFreeOp* fop = cx->defaultFreeOp();
  if (frame->isOnStack()) {
    AbstractFramePtr referent = DebuggerFrame::getReferent(frame);

    // Adjust execution observability and step counts on whatever code (JS or
    // Wasm) this frame is running.
    if (handler) {
      if (!frame->maybeIncrementStepperCounter(cx, referent)) {
        return false;
      }
    } else {
      frame->maybeDecrementStepperCounter(cx->runtime()->defaultFreeOp(),
                                          referent);
    }
  } else if (frame->hasGenerator()) {
    RootedScript script(cx, frame->generatorInfo()->generatorScript());

    if (handler) {
      if (!frame->maybeIncrementStepperCounter(cx, script)) {
        return false;
      }
    } else {
      frame->maybeDecrementStepperCounter(cx->runtime()->defaultFreeOp(),
                                          script);
    }
  } else {
    // If the frame is entirely dead, we still allow setting the onStep
    // handler, but it has no effect.
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

bool DebuggerFrame::maybeIncrementStepperCounter(JSContext* cx,
                                                 AbstractFramePtr referent) {
  if (hasIncrementedStepper()) {
    return true;
  }

  if (!referent.isWasmDebugFrame()) {
    return maybeIncrementStepperCounter(cx, referent.script());
  }

  wasm::Instance* instance = referent.asWasmDebugFrame()->instance();
  wasm::DebugFrame* wasmFrame = referent.asWasmDebugFrame();
  // Single stepping toggled off->on.
  if (!instance->debug().incrementStepperCount(cx, wasmFrame->funcIndex())) {
    return false;
  }

  setHasIncrementedStepper(true);
  return true;
}

bool DebuggerFrame::maybeIncrementStepperCounter(JSContext* cx,
                                                 JSScript* script) {
  if (hasIncrementedStepper()) {
    return true;
  }

  // Single stepping toggled off->on.
  AutoRealm ar(cx, script);
  // Ensure observability *before* incrementing the step mode count.
  // Calling this function after calling incrementStepperCount
  // will make it a no-op.
  if (!Debugger::ensureExecutionObservabilityOfScript(cx, script)) {
    return false;
  }
  if (!DebugScript::incrementStepperCount(cx, script)) {
    return false;
  }

  setHasIncrementedStepper(true);
  return true;
}

void DebuggerFrame::maybeDecrementStepperCounter(JSFreeOp* fop,
                                                 AbstractFramePtr referent) {
  if (!hasIncrementedStepper()) {
    return;
  }

  if (!referent.isWasmDebugFrame()) {
    maybeDecrementStepperCounter(fop, referent.script());
    return;
  }

  wasm::Instance* instance = referent.asWasmDebugFrame()->instance();
  wasm::DebugFrame* wasmFrame = referent.asWasmDebugFrame();
  // Single stepping toggled on->off.
  instance->debug().decrementStepperCount(fop, wasmFrame->funcIndex());
  setHasIncrementedStepper(false);
}

void DebuggerFrame::maybeDecrementStepperCounter(JSFreeOp* fop,
                                                 JSScript* script) {
  if (!hasIncrementedStepper()) {
    return;
  }

  // Single stepping toggled on->off.
  DebugScript::decrementStepperCount(fop, script);
  setHasIncrementedStepper(false);
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
      /* Do not perform the Javascript filename validation security check for
       * javascript executions sent through the debugger. Besides making up
       * a filename for these codepaths, we must allow arbitrary JS execution
       * for the Browser toolbox to function. */
      .setSkipFilenameValidation(true);

  if (frame && frame.hasScript() && frame.script()->strict()) {
    options.setForceStrictMode();
  }

  SourceExtent extent = SourceExtent::makeGlobalExtent(chars.length(), options);

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
    options.setNonSyntacticScope(true);
  }

  if (frame) {
    MOZ_ASSERT(scopeKind == ScopeKind::NonSyntactic);
    RootedScope scope(cx,
                      GlobalScope::createEmpty(cx, ScopeKind::NonSyntactic));
    if (!scope) {
      return false;
    }

    LifoAllocScope allocScope(&cx->tempLifoAlloc());
    frontend::CompilationInfo compilationInfo(cx, allocScope, options);
    if (!compilationInfo.init(cx)) {
      return false;
    }

    frontend::EvalSharedContext evalsc(cx, env, compilationInfo, scope,
                                       compilationInfo.directives, extent);
    script = frontend::CompileEvalScript(compilationInfo, evalsc, env, srcBuf);
    if (!script) {
      return false;
    }
  } else {
    // Do not consider executeInGlobal{WithBindings} as an eval, but instead
    // as executing a series of statements at the global level. This is to
    // circumvent the fresh lexical scope that all eval have, so that the
    // users of executeInGlobal, like the web console, may add new bindings to
    // the global scope.

    LifoAllocScope allocScope(&cx->tempLifoAlloc());
    frontend::CompilationInfo compilationInfo(cx, allocScope, options);
    if (!compilationInfo.init(cx)) {
      return false;
    }
    MOZ_ASSERT(scopeKind == ScopeKind::Global ||
               scopeKind == ScopeKind::NonSyntactic);

    frontend::GlobalSharedContext globalsc(cx, scopeKind, compilationInfo,
                                           compilationInfo.directives, extent);
    script = frontend::CompileGlobalScript(compilationInfo, globalsc, srcBuf);
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

  // Note whether we are in an evaluation that might invoke the OnNativeCall
  // hook, so that the JITs will be disabled.
  AutoNoteDebuggerEvaluationWithOnNativeCallHook noteEvaluation(
      cx, dbg->observesNativeCalls() ? dbg : nullptr);

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
  MOZ_ASSERT(frame->isOnStack());

  Debugger* dbg = frame->owner();

  Maybe<FrameIter> maybeIter;
  if (!DebuggerFrame::getFrameIter(cx, frame, maybeIter)) {
    return cx->alreadyReportedError();
  }
  FrameIter& iter = *maybeIter;

  UpdateFrameIterPc(iter);

  return DebuggerGenericEval(cx, chars, bindings, options, dbg, nullptr, &iter);
}

bool DebuggerFrame::isOnStack() const { return !!getPrivate(); }

bool DebuggerFrame::isOnStackMaybeForwarded() const {
  return !!getPrivate(numFixedSlotsMaybeForwarded());
}

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

/* static */
void DebuggerFrame::finalize(JSFreeOp* fop, JSObject* obj) {
  MOZ_ASSERT(fop->onMainThread());

  DebuggerFrame& frameobj = obj->as<DebuggerFrame>();

  // Connections between dying Debugger.Frames and their
  // AbstractGeneratorObjects should have been broken in DebugAPI::sweepAll.
  MOZ_ASSERT(!frameobj.hasGenerator());

  frameobj.freeFrameIterData(fop);
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
DebuggerFrame* DebuggerFrame::check(JSContext* cx, HandleValue thisv) {
  JSObject* thisobj = RequireObject(cx, thisv);
  if (!thisobj) {
    return nullptr;
  }
  if (thisobj->getClass() != &class_) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INCOMPATIBLE_PROTO, "Debugger.Frame",
                              "method", thisobj->getClass()->name);
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
                              "method", "prototype object");
    return nullptr;
  }

  return frame;
}

struct MOZ_STACK_CLASS DebuggerFrame::CallData {
  JSContext* cx;
  const CallArgs& args;

  HandleDebuggerFrame frame;

  CallData(JSContext* cx, const CallArgs& args, HandleDebuggerFrame frame)
      : cx(cx), args(args), frame(frame) {}

  bool argumentsGetter();
  bool calleeGetter();
  bool constructingGetter();
  bool environmentGetter();
  bool generatorGetter();
  bool asyncPromiseGetter();
  bool olderSavedFrameGetter();
  bool liveGetter();
  bool onStackGetter();
  bool terminatedGetter();
  bool offsetGetter();
  bool olderGetter();
  bool getScript();
  bool thisGetter();
  bool typeGetter();
  bool implementationGetter();
  bool onStepGetter();
  bool onStepSetter();
  bool onPopGetter();
  bool onPopSetter();
  bool evalMethod();
  bool evalWithBindingsMethod();

  using Method = bool (CallData::*)();

  template <Method MyMethod>
  static bool ToNative(JSContext* cx, unsigned argc, Value* vp);

  bool ensureOnStack() const;
  bool ensureOnStackOrSuspended() const;
};

template <DebuggerFrame::CallData::Method MyMethod>
/* static */
bool DebuggerFrame::CallData::ToNative(JSContext* cx, unsigned argc,
                                       Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  RootedDebuggerFrame frame(cx, DebuggerFrame::check(cx, args.thisv()));
  if (!frame) {
    return false;
  }

  CallData data(cx, args, frame);
  return (data.*MyMethod)();
}

static bool EnsureOnStack(JSContext* cx, HandleDebuggerFrame frame) {
  MOZ_ASSERT(frame);
  if (!frame->isOnStack()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_NOT_ON_STACK, "Debugger.Frame");
    return false;
  }

  return true;
}
static bool EnsureOnStackOrSuspended(JSContext* cx, HandleDebuggerFrame frame) {
  MOZ_ASSERT(frame);
  if (!frame->isOnStack() && !frame->hasGenerator()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_NOT_ON_STACK_OR_SUSPENDED,
                              "Debugger.Frame");
    return false;
  }

  return true;
}

bool DebuggerFrame::CallData::ensureOnStack() const {
  return EnsureOnStack(cx, frame);
}
bool DebuggerFrame::CallData::ensureOnStackOrSuspended() const {
  return EnsureOnStackOrSuspended(cx, frame);
}

bool DebuggerFrame::CallData::typeGetter() {
  if (!ensureOnStackOrSuspended()) {
    return false;
  }

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

bool DebuggerFrame::CallData::implementationGetter() {
  if (!ensureOnStack()) {
    return false;
  }

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

bool DebuggerFrame::CallData::environmentGetter() {
  if (!ensureOnStackOrSuspended()) {
    return false;
  }

  RootedDebuggerEnvironment result(cx);
  if (!DebuggerFrame::getEnvironment(cx, frame, &result)) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

bool DebuggerFrame::CallData::calleeGetter() {
  if (!ensureOnStackOrSuspended()) {
    return false;
  }

  RootedDebuggerObject result(cx);
  if (!DebuggerFrame::getCallee(cx, frame, &result)) {
    return false;
  }

  args.rval().setObjectOrNull(result);
  return true;
}

bool DebuggerFrame::CallData::generatorGetter() {
  JS_ReportErrorASCII(cx,
                      "Debugger.Frame.prototype.generator has been removed. "
                      "Use frame.script.isGeneratorFunction instead.");
  return false;
}

bool DebuggerFrame::CallData::constructingGetter() {
  if (!ensureOnStackOrSuspended()) {
    return false;
  }

  bool result;
  if (!DebuggerFrame::getIsConstructing(cx, frame, result)) {
    return false;
  }

  args.rval().setBoolean(result);
  return true;
}

bool DebuggerFrame::CallData::asyncPromiseGetter() {
  if (!ensureOnStackOrSuspended()) {
    return false;
  }

  RootedScript script(cx);
  if (frame->isOnStack()) {
    FrameIter iter(*frame->frameIterData());
    AbstractFramePtr framePtr = iter.abstractFramePtr();

    if (!framePtr.isWasmDebugFrame()) {
      script = framePtr.script();
    }
  } else {
    MOZ_ASSERT(frame->hasGenerator());
    script = frame->generatorInfo()->generatorScript();
  }
  // The async promise value is only provided for async functions and
  // async generator functions.
  if (!script || !script->isAsync()) {
    args.rval().setUndefined();
    return true;
  }

  RootedDebuggerObject result(cx);
  if (!DebuggerFrame::getAsyncPromise(cx, frame, &result)) {
    return false;
  }

  args.rval().setObjectOrNull(result);
  return true;
}

bool DebuggerFrame::CallData::olderSavedFrameGetter() {
  if (!ensureOnStackOrSuspended()) {
    return false;
  }

  RootedSavedFrame result(cx);
  if (!DebuggerFrame::getOlderSavedFrame(cx, frame, &result)) {
    return false;
  }

  args.rval().setObjectOrNull(result);
  return true;
}

/* static */
bool DebuggerFrame::getOlderSavedFrame(JSContext* cx, HandleDebuggerFrame frame,
                                       MutableHandleSavedFrame result) {
  if (frame->isOnStack()) {
    Debugger* dbg = frame->owner();

    Maybe<FrameIter> maybeIter;
    if (!DebuggerFrame::getFrameIter(cx, frame, maybeIter)) {
      return false;
    }
    FrameIter& iter = *maybeIter;

    while (true) {
      Activation& activation = *iter.activation();
      ++iter;

      // If the parent frame crosses an explicit async stack boundary, or we
      // have hit the end of the synchronous frames, we want to switch over
      // to using SavedFrames.
      if (iter.activation() != &activation && activation.asyncStack() &&
          (activation.asyncCallIsExplicit() || iter.done())) {
        const char* cause = activation.asyncCause();
        RootedAtom causeAtom(cx, AtomizeUTF8Chars(cx, cause, strlen(cause)));
        if (!causeAtom) {
          return false;
        }
        RootedSavedFrame stackObj(cx, activation.asyncStack());

        return cx->realm()->savedStacks().copyAsyncStack(
            cx, stackObj, causeAtom, result, mozilla::Nothing());
      }

      // If there are no more parent frames, we're done.
      if (iter.done()) {
        break;
      }

      // If we hit another frame that we observe, then there is no saved
      // frame that we'd want to return.
      if (dbg->observesFrame(iter)) {
        break;
      }
    }
  } else {
    MOZ_ASSERT(frame->hasGenerator());
  }

  result.set(nullptr);
  return true;
}

bool DebuggerFrame::CallData::thisGetter() {
  if (!ensureOnStackOrSuspended()) {
    return false;
  }

  return DebuggerFrame::getThis(cx, frame, args.rval());
}

bool DebuggerFrame::CallData::olderGetter() {
  if (!ensureOnStackOrSuspended()) {
    return false;
  }

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

  RootedValue framev(cx, argsobj->as<NativeObject>().getReservedSlot(
                             JSSLOT_DEBUGARGUMENTS_FRAME));
  RootedDebuggerFrame thisobj(cx, DebuggerFrame::check(cx, framev));
  if (!thisobj || !EnsureOnStack(cx, thisobj)) {
    return false;
  }

  FrameIter frameIter(*thisobj->frameIterData());
  AbstractFramePtr frame = frameIter.abstractFramePtr();

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

bool DebuggerFrame::CallData::argumentsGetter() {
  if (!ensureOnStack()) {
    return false;
  }

  RootedDebuggerArguments result(cx);
  if (!DebuggerFrame::getArguments(cx, frame, &result)) {
    return false;
  }

  args.rval().setObjectOrNull(result);
  return true;
}

bool DebuggerFrame::CallData::getScript() {
  if (!ensureOnStackOrSuspended()) {
    return false;
  }

  RootedDebuggerScript scriptObject(cx);

  Debugger* debug = Debugger::fromChildJSObject(frame);
  if (frame->isOnStack()) {
    FrameIter iter(*frame->frameIterData());
    AbstractFramePtr framePtr = iter.abstractFramePtr();

    if (framePtr.isWasmDebugFrame()) {
      RootedWasmInstanceObject instance(cx, framePtr.wasmInstance()->object());
      scriptObject = debug->wrapWasmScript(cx, instance);
    } else {
      RootedScript script(cx, framePtr.script());
      scriptObject = debug->wrapScript(cx, script);
    }
  } else {
    MOZ_ASSERT(frame->hasGenerator());
    RootedScript script(cx, frame->generatorInfo()->generatorScript());
    scriptObject = debug->wrapScript(cx, script);
  }
  if (!scriptObject) {
    return false;
  }

  args.rval().setObject(*scriptObject);
  return true;
}

bool DebuggerFrame::CallData::offsetGetter() {
  if (!ensureOnStackOrSuspended()) {
    return false;
  }

  size_t result;
  if (!DebuggerFrame::getOffset(cx, frame, result)) {
    return false;
  }

  args.rval().setNumber(double(result));
  return true;
}

bool DebuggerFrame::CallData::liveGetter() {
  JS_ReportErrorASCII(
      cx, "Debugger.Frame.prototype.live has been renamed to .onStack");
  return false;
}

bool DebuggerFrame::CallData::onStackGetter() {
  args.rval().setBoolean(frame->isOnStack());
  return true;
}

bool DebuggerFrame::CallData::terminatedGetter() {
  args.rval().setBoolean(!frame->isOnStack() && !frame->hasGenerator());
  return true;
}

static bool IsValidHook(const Value& v) {
  return v.isUndefined() || (v.isObject() && v.toObject().isCallable());
}

bool DebuggerFrame::CallData::onStepGetter() {
  OnStepHandler* handler = frame->onStepHandler();
  RootedValue value(
      cx, handler ? ObjectOrNullValue(handler->object()) : UndefinedValue());
  MOZ_ASSERT(IsValidHook(value));
  args.rval().set(value);
  return true;
}

bool DebuggerFrame::CallData::onStepSetter() {
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

bool DebuggerFrame::CallData::onPopGetter() {
  OnPopHandler* handler = frame->onPopHandler();
  RootedValue value(
      cx, handler ? ObjectValue(*handler->object()) : UndefinedValue());
  MOZ_ASSERT(IsValidHook(value));
  args.rval().set(value);
  return true;
}

bool DebuggerFrame::CallData::onPopSetter() {
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

bool DebuggerFrame::CallData::evalMethod() {
  if (!ensureOnStack()) {
    return false;
  }

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

bool DebuggerFrame::CallData::evalWithBindingsMethod() {
  if (!ensureOnStack()) {
    return false;
  }

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
    JS_DEBUG_PSG("arguments", argumentsGetter),
    JS_DEBUG_PSG("callee", calleeGetter),
    JS_DEBUG_PSG("constructing", constructingGetter),
    JS_DEBUG_PSG("environment", environmentGetter),
    JS_DEBUG_PSG("generator", generatorGetter),
    JS_DEBUG_PSG("live", liveGetter),
    JS_DEBUG_PSG("onStack", onStackGetter),
    JS_DEBUG_PSG("terminated", terminatedGetter),
    JS_DEBUG_PSG("offset", offsetGetter),
    JS_DEBUG_PSG("older", olderGetter),
    JS_DEBUG_PSG("olderSavedFrame", olderSavedFrameGetter),
    JS_DEBUG_PSG("script", getScript),
    JS_DEBUG_PSG("this", thisGetter),
    JS_DEBUG_PSG("asyncPromise", asyncPromiseGetter),
    JS_DEBUG_PSG("type", typeGetter),
    JS_DEBUG_PSG("implementation", implementationGetter),
    JS_DEBUG_PSGS("onStep", onStepGetter, onStepSetter),
    JS_DEBUG_PSGS("onPop", onPopGetter, onPopSetter),
    JS_PS_END};

const JSFunctionSpec DebuggerFrame::methods_[] = {
    JS_DEBUG_FN("eval", evalMethod, 1),
    JS_DEBUG_FN("evalWithBindings", evalWithBindingsMethod, 1), JS_FS_END};

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
