/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "debugger/Debugger-inl.h"

#include "mozilla/Attributes.h"        // for MOZ_STACK_CLASS, MOZ_RAII
#include "mozilla/DebugOnly.h"         // for DebugOnly
#include "mozilla/DoublyLinkedList.h"  // for DoublyLinkedList<>::Iterator
#include "mozilla/GuardObjects.h"      // for MOZ_GUARD_OBJECT_NOTIFIER_PARAM
#include "mozilla/HashTable.h"         // for HashSet<>::Range, HashMapEntry
#include "mozilla/Maybe.h"             // for Maybe, Nothing, Some
#include "mozilla/Move.h"              // for std::move
#include "mozilla/RecordReplay.h"      // for IsMiddleman
#include "mozilla/ScopeExit.h"         // for MakeScopeExit, ScopeExit
#include "mozilla/ThreadLocal.h"       // for ThreadLocal
#include "mozilla/TimeStamp.h"         // for TimeStamp, TimeDuration
#include "mozilla/TypeTraits.h"        // for RemoveConst<>::Type
#include "mozilla/UniquePtr.h"         // for UniquePtr
#include "mozilla/Variant.h"           // for AsVariant, AsVariantTemporary
#include "mozilla/Vector.h"            // for Vector, Vector<>::ConstRange

#include <algorithm>   // for std::max
#include <functional>  // for function
#include <stddef.h>    // for size_t
#include <stdint.h>    // for uint32_t, uint64_t, int32_t
#include <string.h>    // for strlen, strcmp

#include "jsapi.h"        // for CallArgs, CallArgsFromVp
#include "jsfriendapi.h"  // for GetErrorMessage
#include "jstypes.h"      // for JS_PUBLIC_API
#include "jsutil.h"       // for Find

#include "builtin/Array.h"               // for NewDenseFullyAllocatedArray
#include "builtin/Promise.h"             // for PromiseObject
#include "debugger/DebugAPI.h"           // for ResumeMode, DebugAPI
#include "debugger/DebuggerMemory.h"     // for DebuggerMemory
#include "debugger/DebugScript.h"        // for DebugScript
#include "debugger/Environment.h"        // for DebuggerEnvironment
#include "debugger/Frame.h"              // for DebuggerFrame
#include "debugger/NoExecute.h"          // for EnterDebuggeeNoExecute
#include "debugger/Object.h"             // for DebuggerObject
#include "debugger/Script.h"             // for DebuggerScript
#include "debugger/Source.h"             // for DebuggerSource
#include "frontend/BytecodeCompiler.h"   // for CreateScriptSourceObject
#include "frontend/NameAnalysisTypes.h"  // for ParseGoal, ParseGoal::Script
#include "frontend/ParseContext.h"       // for UsedNameTracker
#include "frontend/Parser.h"             // for Parser
#include "gc/Barrier.h"                  // for GCPtrNativeObject
#include "gc/FreeOp.h"                   // for FreeOp
#include "gc/GC.h"                       // for IterateLazyScripts
#include "gc/GCMarker.h"                 // for GCMarker
#include "gc/GCRuntime.h"                // for GCRuntime, AutoEnterIteration
#include "gc/HashUtil.h"                 // for DependentAddPtr
#include "gc/Marking.h"                  // for IsMarkedUnbarriered, IsMarked
#include "gc/PublicIterators.h"          // for RealmsIter, CompartmentsIter
#include "gc/Rooting.h"                  // for RootedNativeObject
#include "gc/Statistics.h"               // for Statistics::SliceData
#include "gc/Tracer.h"                   // for TraceEdge
#include "gc/Zone.h"                     // for Zone
#include "gc/ZoneAllocator.h"            // for ZoneAllocPolicy
#include "jit/BaselineDebugModeOSR.h"  // for RecompileOnStackBaselineScriptsForDebugMode
#include "jit/BaselineJIT.h"           // for FinishDiscardBaselineScript
#include "jit/Ion.h"                   // for JitContext
#include "jit/JitScript.h"             // for JitScript
#include "jit/JSJitFrameIter.h"       // for InlineFrameIterator
#include "jit/RematerializedFrame.h"  // for RematerializedFrame
#include "js/Conversions.h"           // for ToBoolean, ToUint32
#include "js/Debug.h"                 // for Builder::Object, Builder
#include "js/GCAPI.h"                 // for GarbageCollectionEvent
#include "js/HeapAPI.h"               // for ExposeObjectToActiveJS
#include "js/Promise.h"               // for AutoDebuggerJobQueueInterruption
#include "js/Proxy.h"                 // for PropertyDescriptor
#include "js/SourceText.h"            // for SourceOwnership, SourceText
#include "js/StableStringChars.h"     // for AutoStableStringChars
#include "js/UbiNode.h"               // for Node, RootList, Edge
#include "js/UbiNodeBreadthFirst.h"   // for BreadthFirst
#include "js/Warnings.h"              // for AutoSuppressWarningReporter
#include "js/Wrapper.h"               // for CheckedUnwrapStatic
#include "util/Text.h"                // for DuplicateString, js_strlen
#include "vm/ArrayObject.h"           // for ArrayObject
#include "vm/AsyncFunction.h"         // for AsyncFunctionGeneratorObject
#include "vm/AsyncIteration.h"        // for AsyncGeneratorObject
#include "vm/BytecodeUtil.h"          // for JSDVG_IGNORE_STACK
#include "vm/Compartment.h"           // for CrossCompartmentKey
#include "vm/EnvironmentObject.h"     // for IsSyntacticEnvironment
#include "vm/ErrorReporting.h"        // for ReportErrorToGlobal
#include "vm/GeneratorObject.h"       // for AbstractGeneratorObject
#include "vm/GlobalObject.h"          // for GlobalObject
#include "vm/Interpreter.h"           // for Call, ReportIsNotFunction
#include "vm/Iteration.h"             // for CreateIterResultObject
#include "vm/JSAtom.h"                // for Atomize, ClassName
#include "vm/JSContext.h"             // for JSContext
#include "vm/JSFunction.h"            // for JSFunction
#include "vm/JSObject.h"              // for JSObject, RequireObject
#include "vm/ObjectGroup.h"           // for TenuredObject
#include "vm/ObjectOperations.h"      // for DefineDataProperty
#include "vm/ProxyObject.h"           // for ProxyObject, JSObject::is
#include "vm/Realm.h"                 // for AutoRealm, Realm
#include "vm/Runtime.h"               // for ReportOutOfMemory, JSRuntime
#include "vm/SavedFrame.h"            // for SavedFrame
#include "vm/SavedStacks.h"           // for SavedStacks
#include "vm/Scope.h"                 // for Scope
#include "vm/StringType.h"            // for JSString, PropertyName
#include "vm/TraceLogging.h"          // for TraceLoggerForCurrentThread
#include "vm/TypeInference.h"         // for TypeZone
#include "vm/WrapperObject.h"         // for CrossCompartmentWrapperObject
#include "wasm/WasmDebug.h"           // for DebugState
#include "wasm/WasmInstance.h"        // for Instance
#include "wasm/WasmJS.h"              // for WasmInstanceObject
#include "wasm/WasmRealm.h"           // for Realm
#include "wasm/WasmTypes.h"           // for WasmInstanceObjectVector

#include "debugger/DebugAPI-inl.h"
#include "debugger/Frame-inl.h"    // for DebuggerFrame::hasGenerator
#include "debugger/Script-inl.h"   // for DebuggerScript::getReferent
#include "gc/GC-inl.h"             // for ZoneCellIter
#include "gc/Marking-inl.h"        // for MaybeForwarded
#include "gc/WeakMap-inl.h"        // for DebuggerWeakMap::trace
#include "vm/Compartment-inl.h"    // for Compartment::wrap
#include "vm/GeckoProfiler-inl.h"  // for AutoSuppressProfilerSampling
#include "vm/JSAtom-inl.h"         // for AtomToId, ValueToId
#include "vm/JSContext-inl.h"      // for JSContext::check
#include "vm/JSObject-inl.h"       // for JSObject::isCallable
#include "vm/JSScript-inl.h"       // for JSScript::isDebuggee, JSScript
#include "vm/NativeObject-inl.h"  // for NativeObject::ensureDenseInitializedLength
#include "vm/ObjectOperations-inl.h"  // for GetProperty, HasProperty
#include "vm/Realm-inl.h"             // for AutoRealm::AutoRealm
#include "vm/Stack-inl.h"             // for AbstractFramePtr::script
#include "vm/TypeInference-inl.h"     // for AutoEnterAnalysis

namespace js {

namespace frontend {
class FullParseHandler;
}

namespace gc {
struct Cell;
}

namespace jit {
class BaselineFrame;
}

} /* namespace js */

using namespace js;

using JS::AutoStableStringChars;
using JS::CompileOptions;
using JS::SourceOwnership;
using JS::SourceText;
using JS::dbg::AutoEntryMonitor;
using JS::dbg::Builder;
using js::frontend::IsIdentifier;
using mozilla::AsVariant;
using mozilla::DebugOnly;
using mozilla::MakeScopeExit;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;
using mozilla::TimeDuration;
using mozilla::TimeStamp;

/*** Utils ******************************************************************/

bool js::IsInterpretedNonSelfHostedFunction(JSFunction* fun) {
  return fun->isInterpreted() && !fun->isSelfHostedBuiltin();
}

bool js::EnsureFunctionHasScript(JSContext* cx, HandleFunction fun) {
  if (fun->isInterpretedLazy()) {
    AutoRealm ar(cx, fun);
    return !!JSFunction::getOrCreateScript(cx, fun);
  }
  return true;
}

JSScript* js::GetOrCreateFunctionScript(JSContext* cx, HandleFunction fun) {
  MOZ_ASSERT(IsInterpretedNonSelfHostedFunction(fun));
  if (!EnsureFunctionHasScript(cx, fun)) {
    return nullptr;
  }
  return fun->nonLazyScript();
}

bool js::ValueToIdentifier(JSContext* cx, HandleValue v, MutableHandleId id) {
  if (!ValueToId<CanGC>(cx, v, id)) {
    return false;
  }
  if (!JSID_IS_ATOM(id) || !IsIdentifier(JSID_TO_ATOM(id))) {
    RootedValue val(cx, v);
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_SEARCH_STACK, val,
                     nullptr, "not an identifier");
    return false;
  }
  return true;
}

class js::AutoRestoreRealmDebugMode {
  Realm* realm_;
  unsigned bits_;

 public:
  explicit AutoRestoreRealmDebugMode(Realm* realm)
      : realm_(realm), bits_(realm->debugModeBits_) {
    MOZ_ASSERT(realm_);
  }

  ~AutoRestoreRealmDebugMode() {
    if (realm_) {
      realm_->debugModeBits_ = bits_;
    }
  }

  void release() { realm_ = nullptr; }
};

/* static */
bool DebugAPI::slowPathCheckNoExecute(JSContext* cx, HandleScript script) {
  MOZ_ASSERT(cx->realm()->isDebuggee());
  MOZ_ASSERT(cx->noExecuteDebuggerTop);
  return EnterDebuggeeNoExecute::reportIfFoundInStack(cx, script);
}

static inline void NukeDebuggerWrapper(NativeObject* wrapper) {
  // In some OOM failure cases, we need to destroy the edge to the referent,
  // to avoid trying to trace it during untimely collections.
  wrapper->setPrivate(nullptr);
}

bool js::ValueToStableChars(JSContext* cx, const char* fnname,
                            HandleValue value,
                            AutoStableStringChars& stableChars) {
  if (!value.isString()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_EXPECTED_TYPE, fnname, "string",
                              InformalValueTypeName(value));
    return false;
  }
  RootedLinearString linear(cx, value.toString()->ensureLinear(cx));
  if (!linear) {
    return false;
  }
  if (!stableChars.initTwoByte(cx, linear)) {
    return false;
  }
  return true;
}

bool EvalOptions::setFilename(JSContext* cx, const char* filename) {
  JS::UniqueChars copy;
  if (filename) {
    copy = DuplicateString(cx, filename);
    if (!copy) {
      return false;
    }
  }

  filename_ = std::move(copy);
  return true;
}

bool js::ParseEvalOptions(JSContext* cx, HandleValue value,
                          EvalOptions& options) {
  if (!value.isObject()) {
    return true;
  }

  RootedObject opts(cx, &value.toObject());

  RootedValue v(cx);
  if (!JS_GetProperty(cx, opts, "url", &v)) {
    return false;
  }
  if (!v.isUndefined()) {
    RootedString url_str(cx, ToString<CanGC>(cx, v));
    if (!url_str) {
      return false;
    }
    UniqueChars url_bytes = JS_EncodeStringToLatin1(cx, url_str);
    if (!url_bytes) {
      return false;
    }
    if (!options.setFilename(cx, url_bytes.get())) {
      return false;
    }
  }

  if (!JS_GetProperty(cx, opts, "lineNumber", &v)) {
    return false;
  }
  if (!v.isUndefined()) {
    uint32_t lineno;
    if (!ToUint32(cx, v, &lineno)) {
      return false;
    }
    options.setLineno(lineno);
  }

  return true;
}

/*** Breakpoints ************************************************************/

BreakpointSite::BreakpointSite(Type type) : type_(type), enabledCount(0) {}

void BreakpointSite::inc(FreeOp* fop) {
  enabledCount++;
  if (enabledCount == 1) {
    recompile(fop);
  }
}

void BreakpointSite::dec(FreeOp* fop) {
  MOZ_ASSERT(enabledCount > 0);
  enabledCount--;
  if (enabledCount == 0) {
    recompile(fop);
  }
}

bool BreakpointSite::isEmpty() const { return breakpoints.isEmpty(); }

Breakpoint* BreakpointSite::firstBreakpoint() const {
  if (isEmpty()) {
    return nullptr;
  }
  return &(*breakpoints.begin());
}

bool BreakpointSite::hasBreakpoint(Breakpoint* toFind) {
  const BreakpointList::Iterator bp(toFind);
  for (auto p = breakpoints.begin(); p; p++) {
    if (p == bp) {
      return true;
    }
  }
  return false;
}

inline gc::Cell* BreakpointSite::owningCellUnbarriered() {
  if (type() == Type::JS) {
    return asJS()->script;
  }

  return asWasm()->instance->objectUnbarriered();
}

inline size_t BreakpointSite::allocSize() {
  if (type() == Type::JS) {
    return sizeof(Breakpoint);
  }

  return sizeof(WasmBreakpoint);
}

Breakpoint::Breakpoint(Debugger* debugger, BreakpointSite* site,
                       JSObject* handler)
    : debugger(debugger), site(site), handler(handler) {
  MOZ_ASSERT(handler->compartment() == debugger->object->compartment());
  debugger->breakpoints.pushBack(this);
  site->breakpoints.pushBack(this);
}

void Breakpoint::destroy(FreeOp* fop,
                         MayDestroySite mayDestroySite /* true */) {
  if (debugger->enabled) {
    site->dec(fop);
  }
  debugger->breakpoints.remove(this);
  site->breakpoints.remove(this);
  gc::Cell* cell = site->owningCellUnbarriered();
  size_t size = site->allocSize();
  if (mayDestroySite == MayDestroySite::True) {
    site->destroyIfEmpty(fop);
  }
  fop->delete_(cell, this, size, MemoryUse::Breakpoint);
}

Breakpoint* Breakpoint::nextInDebugger() { return debuggerLink.mNext; }

Breakpoint* Breakpoint::nextInSite() { return siteLink.mNext; }

JSBreakpointSite::JSBreakpointSite(JSScript* script, jsbytecode* pc)
    : BreakpointSite(Type::JS), script(script), pc(pc) {
  MOZ_ASSERT(!DebugAPI::hasBreakpointsAt(script, pc));
}

void JSBreakpointSite::recompile(FreeOp* fop) {
  if (script->hasBaselineScript()) {
    script->baselineScript()->toggleDebugTraps(script, pc);
  }
}

void JSBreakpointSite::destroyIfEmpty(FreeOp* fop) {
  if (isEmpty()) {
    DebugScript::destroyBreakpointSite(fop, script, pc);
  }
}

WasmBreakpointSite::WasmBreakpointSite(wasm::Instance* instance_,
                                       uint32_t offset_)
    : BreakpointSite(Type::Wasm), instance(instance_), offset(offset_) {
  MOZ_ASSERT(instance);
  MOZ_ASSERT(instance->debugEnabled());
}

void WasmBreakpointSite::recompile(FreeOp* fop) {
  instance->debug().toggleBreakpointTrap(fop->runtime(), offset, isEnabled());
}

void WasmBreakpointSite::destroyIfEmpty(FreeOp* fop) {
  if (isEmpty()) {
    instance->destroyBreakpointSite(fop, offset);
  }
}

/*** Debugger hook dispatch *************************************************/

Debugger::Debugger(JSContext* cx, NativeObject* dbg)
    : object(dbg),
      debuggees(cx->zone()),
      uncaughtExceptionHook(nullptr),
      enabled(true),
      allowUnobservedAsmJS(false),
      collectCoverageInfo(false),
      observedGCs(cx->zone()),
      allocationsLog(cx),
      trackingAllocationSites(false),
      allocationSamplingProbability(1.0),
      maxAllocationsLogLength(DEFAULT_MAX_LOG_LENGTH),
      allocationsLogOverflowed(false),
      frames(cx->zone()),
      generatorFrames(cx),
      scripts(cx),
      lazyScripts(cx),
      sources(cx),
      objects(cx),
      environments(cx),
      wasmInstanceScripts(cx),
      wasmInstanceSources(cx),
#ifdef NIGHTLY_BUILD
      traceLoggerLastDrainedSize(0),
      traceLoggerLastDrainedIteration(0),
#endif
      traceLoggerScriptedCallsLastDrainedSize(0),
      traceLoggerScriptedCallsLastDrainedIteration(0) {
  cx->check(dbg);

#ifdef JS_TRACE_LOGGING
  TraceLoggerThread* logger = TraceLoggerForCurrentThread(cx);
  if (logger) {
#  ifdef NIGHTLY_BUILD
    logger->getIterationAndSize(&traceLoggerLastDrainedIteration,
                                &traceLoggerLastDrainedSize);
#  endif
    logger->getIterationAndSize(&traceLoggerScriptedCallsLastDrainedIteration,
                                &traceLoggerScriptedCallsLastDrainedSize);
  }
#endif

  cx->runtime()->debuggerList().insertBack(this);
}

Debugger::~Debugger() {
  MOZ_ASSERT(debuggees.empty());
  allocationsLog.clear();

  // We don't have to worry about locking here since Debugger is not
  // background finalized.
  JSContext* cx = TlsContext.get();
  if (onNewGlobalObjectWatchersLink.mPrev ||
      onNewGlobalObjectWatchersLink.mNext ||
      cx->runtime()->onNewGlobalObjectWatchers().begin() ==
          JSRuntime::WatchersList::Iterator(this)) {
    cx->runtime()->onNewGlobalObjectWatchers().remove(this);
  }
}

JS_STATIC_ASSERT(unsigned(DebuggerFrame::OWNER_SLOT) ==
                 unsigned(DebuggerScript::OWNER_SLOT));
JS_STATIC_ASSERT(unsigned(DebuggerFrame::OWNER_SLOT) ==
                 unsigned(DebuggerSource::OWNER_SLOT));
JS_STATIC_ASSERT(unsigned(DebuggerFrame::OWNER_SLOT) ==
                 unsigned(JSSLOT_DEBUGOBJECT_OWNER));
JS_STATIC_ASSERT(unsigned(DebuggerFrame::OWNER_SLOT) ==
                 unsigned(DebuggerEnvironment::OWNER_SLOT));

#ifdef DEBUG
/* static */
bool Debugger::isChildJSObject(JSObject* obj) {
  return obj->getClass() == &DebuggerFrame::class_ ||
         obj->getClass() == &DebuggerScript::class_ ||
         obj->getClass() == &DebuggerSource::class_ ||
         obj->getClass() == &DebuggerObject::class_ ||
         obj->getClass() == &DebuggerEnvironment::class_;
}
#endif

/* static */
Debugger* Debugger::fromChildJSObject(JSObject* obj) {
  MOZ_ASSERT(isChildJSObject(obj));
  JSObject* dbgobj = &obj->as<NativeObject>()
                          .getReservedSlot(JSSLOT_DEBUGOBJECT_OWNER)
                          .toObject();
  return fromJSObject(dbgobj);
}

bool Debugger::hasMemory() const {
  return object->getReservedSlot(JSSLOT_DEBUG_MEMORY_INSTANCE).isObject();
}

DebuggerMemory& Debugger::memory() const {
  MOZ_ASSERT(hasMemory());
  return object->getReservedSlot(JSSLOT_DEBUG_MEMORY_INSTANCE)
      .toObject()
      .as<DebuggerMemory>();
}

/*** DebuggerVectorHolder *****************************************************/

static void GlobalDebuggerVectorHolder_finalize(FreeOp* fop, JSObject* obj) {
  MOZ_ASSERT(fop->maybeOnHelperThread());
  void* ptr = obj->as<NativeObject>().getPrivate();
  auto debuggers = static_cast<GlobalObject::DebuggerVector*>(ptr);
  fop->delete_(obj, debuggers, MemoryUse::GlobalDebuggerVector);
}

static const ClassOps GlobalDebuggerVectorHolder_classOps = {
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  GlobalDebuggerVectorHolder_finalize
};

static const Class GlobalDebuggerVectorHolder_class = {
    "GlobalDebuggerVectorHolder",
    JSCLASS_HAS_PRIVATE | JSCLASS_BACKGROUND_FINALIZE,
    &GlobalDebuggerVectorHolder_classOps};

/* static */
JSObject* DebugAPI::newGlobalDebuggersHolder(JSContext* cx) {
  NativeObject* obj =
      NewNativeObjectWithGivenProto(cx, &GlobalDebuggerVectorHolder_class,
                                    nullptr);
  if (!obj) {
    return nullptr;
  }

  GlobalObject::DebuggerVector* debuggers =
      cx->new_<GlobalObject::DebuggerVector>(cx->zone());
  if (!debuggers) {
    return nullptr;
  }

  InitObjectPrivate(obj, debuggers, MemoryUse::GlobalDebuggerVector);
  return obj;
}

/* static */
GlobalObject::DebuggerVector* DebugAPI::getGlobalDebuggers(JSObject* holder) {
  MOZ_ASSERT(holder->getClass() == &GlobalDebuggerVectorHolder_class);
  return (GlobalObject::DebuggerVector*)holder->as<NativeObject>().getPrivate();
}

/*** Debugger accessors *******************************************************/

bool Debugger::getFrame(JSContext* cx, const FrameIter& iter,
                        MutableHandleValue vp) {
  RootedDebuggerFrame result(cx);
  if (!Debugger::getFrame(cx, iter, &result)) {
    return false;
  }
  vp.setObject(*result);
  return true;
}

bool Debugger::getFrame(JSContext* cx, const FrameIter& iter,
                        MutableHandleDebuggerFrame result) {
  AbstractFramePtr referent = iter.abstractFramePtr();
  MOZ_ASSERT_IF(referent.hasScript(), !referent.script()->selfHosted());

  if (referent.hasScript() &&
      !referent.script()->ensureHasAnalyzedArgsUsage(cx)) {
    return false;
  }

  FrameMap::AddPtr p = frames.lookupForAdd(referent);
  if (!p) {
    RootedDebuggerFrame frame(cx);

    // If this is a generator frame, there may be an existing
    // Debugger.Frame object that isn't in `frames` because the generator
    // was suspended, popping the stack frame, and later resumed (and we
    // were not stepping, so did not pass through slowPathOnResumeFrame).
    Rooted<AbstractGeneratorObject*> genObj(cx);
    GeneratorWeakMap::AddPtr gp;
    if (referent.isGeneratorFrame()) {
      {
        AutoRealm ar(cx, referent.callee());
        genObj = GetGeneratorObjectForFrame(cx, referent);
      }
      if (genObj) {
        gp = generatorFrames.lookupForAdd(genObj);
        if (gp) {
          frame = &gp->value()->as<DebuggerFrame>();
          MOZ_ASSERT(&frame->unwrappedGenerator() == genObj);

          // We have found an existing Debugger.Frame object. But
          // since it was previously popped (see comment above), it
          // is not currently "live". We must revive it.
          if (!frame->resume(iter)) {
            return false;
          }
          if (!ensureExecutionObservabilityOfFrame(cx, referent)) {
            return false;
          }
        }
      }

      // If no AbstractGeneratorObject exists yet, we create a Debugger.Frame
      // below anyway, and Debugger::onNewGenerator() will associate it
      // with the AbstractGeneratorObject later when we hit JSOP_GENERATOR.
    }

    if (!frame) {
      // Create and populate the Debugger.Frame object.
      RootedObject proto(
          cx, &object->getReservedSlot(JSSLOT_DEBUG_FRAME_PROTO).toObject());
      RootedNativeObject debugger(cx, object);

      frame = DebuggerFrame::create(cx, proto, iter, debugger);
      if (!frame) {
        return false;
      }

      if (!ensureExecutionObservabilityOfFrame(cx, referent)) {
        return false;
      }

      if (genObj) {
        if (!frame->setGenerator(cx, genObj)) {
          return false;
        }
      }
    }

    if (!frames.add(p, referent, frame)) {
      frame->freeFrameIterData(cx->defaultFreeOp());
      frame->clearGenerator(cx->runtime()->defaultFreeOp(), this);
      ReportOutOfMemory(cx);
      return false;
    }
  }

  result.set(&p->value()->as<DebuggerFrame>());
  return true;
}

static bool DebuggerExists(GlobalObject* global,
                           const std::function<bool(Debugger* dbg)>& predicate) {
  // The GC analysis can't determine that the predicate can't GC, so let it know
  // explicitly.
  JS::AutoSuppressGCAnalysis nogc;

  if (GlobalObject::DebuggerVector* debuggers = global->getDebuggers()) {
    for (auto p = debuggers->begin(); p != debuggers->end(); p++) {
      // Callbacks should not create new references to the debugger, so don't
      // use a barrier. This allows this method to be called during GC.
      if (predicate(p->unbarrieredGet())) {
        return true;
      }
    }
  }
  return false;
}

/* static */
bool Debugger::hasLiveHook(GlobalObject* global, Hook which) {
  return DebuggerExists(global, [=](Debugger* dbg) {
      return dbg->enabled && dbg->getHook(which);
    });
}

/* static */
bool DebugAPI::debuggerObservesAllExecution(GlobalObject* global) {
  return DebuggerExists(global, [=](Debugger* dbg) {
      return dbg->observesAllExecution();
    });
}

/* static */
bool DebugAPI::debuggerObservesCoverage(GlobalObject* global) {
  return DebuggerExists(global, [=](Debugger* dbg) {
      return dbg->observesCoverage();
    });
}

/* static */
bool DebugAPI::debuggerObservesAsmJS(GlobalObject* global) {
  return DebuggerExists(global, [=](Debugger* dbg) {
      return dbg->observesAsmJS();
    });
}

/* static */
bool DebugAPI::hasExceptionUnwindHook(GlobalObject* global) {
  return Debugger::hasLiveHook(global, Debugger::OnExceptionUnwind);
}

/* static */
bool DebugAPI::hasDebuggerStatementHook(GlobalObject* global) {
  return Debugger::hasLiveHook(global, Debugger::OnDebuggerStatement);
}

JSObject* Debugger::getHook(Hook hook) const {
  MOZ_ASSERT(hook >= 0 && hook < HookCount);
  const Value& v = object->getReservedSlot(JSSLOT_DEBUG_HOOK_START + hook);
  return v.isUndefined() ? nullptr : &v.toObject();
}

bool Debugger::hasAnyLiveHooks(JSRuntime* rt) const {
  if (!enabled) {
    return false;
  }

  // A onNewGlobalObject hook does not hold its Debugger live, so its behavior
  // is nondeterministic. This behavior is not satisfying, but it is at least
  // documented.
  if (getHook(OnDebuggerStatement) || getHook(OnExceptionUnwind) ||
      getHook(OnNewScript) || getHook(OnEnterFrame)) {
    return true;
  }

  // If any breakpoints are in live scripts, return true.
  for (Breakpoint* bp = firstBreakpoint(); bp; bp = bp->nextInDebugger()) {
    switch (bp->site->type()) {
      case BreakpointSite::Type::JS:
        if (IsMarkedUnbarriered(rt, &bp->site->asJS()->script)) {
          return true;
        }
        break;
      case BreakpointSite::Type::Wasm:
        if (IsMarkedUnbarriered(rt, &bp->asWasm()->wasmInstance)) {
          return true;
        }
        break;
    }
  }

  // Check for hooks in live stack frames.
  for (FrameMap::Range r = frames.all(); !r.empty(); r.popFront()) {
    DebuggerFrame& frameObj = r.front().value()->as<DebuggerFrame>();
    if (frameObj.hasAnyLiveHooks()) {
      return true;
    }
  }

  // Check for hooks set on suspended generator frames.
  for (GeneratorWeakMap::Range r = generatorFrames.all(); !r.empty();
       r.popFront()) {
    JSObject* key = r.front().key();
    DebuggerFrame& frameObj = r.front().value()->as<DebuggerFrame>();
    if (IsMarkedUnbarriered(rt, &key) && frameObj.hasAnyLiveHooks()) {
      return true;
    }
  }

  return false;
}

/* static */
ResumeMode DebugAPI::slowPathOnEnterFrame(JSContext* cx,
                                          AbstractFramePtr frame) {
  RootedValue rval(cx);
  ResumeMode resumeMode = Debugger::dispatchHook(
      cx,
      [frame](Debugger* dbg) -> bool {
        return dbg->observesFrame(frame) && dbg->observesEnterFrame();
      },
      [&](Debugger* dbg) -> ResumeMode {
        return dbg->fireEnterFrame(cx, &rval);
      });

  switch (resumeMode) {
    case ResumeMode::Continue:
      break;

    case ResumeMode::Throw:
      cx->setPendingExceptionAndCaptureStack(rval);
      break;

    case ResumeMode::Terminate:
      cx->clearPendingException();
      break;

    case ResumeMode::Return:
      frame.setReturnValue(rval);
      break;

    default:
      MOZ_CRASH("bad Debugger::onEnterFrame resume mode");
  }

  return resumeMode;
}

/* static */
ResumeMode DebugAPI::slowPathOnResumeFrame(JSContext* cx,
                                           AbstractFramePtr frame) {
  // Don't count on this method to be called every time a generator is
  // resumed! This is called only if the frame's debuggee bit is set,
  // i.e. the script has breakpoints or the frame is stepping.
  MOZ_ASSERT(frame.isGeneratorFrame());
  MOZ_ASSERT(frame.isDebuggee());

  Rooted<AbstractGeneratorObject*> genObj(
      cx, GetGeneratorObjectForFrame(cx, frame));
  MOZ_ASSERT(genObj);

  // For each debugger, if there is an existing Debugger.Frame object for the
  // resumed `frame`, update it with the new frame pointer and make sure the
  // frame is observable.
  if (GlobalObject::DebuggerVector* debuggers =
          frame.global()->getDebuggers()) {
    for (Debugger* dbg : *debuggers) {
      if (Debugger::GeneratorWeakMap::Ptr entry =
              dbg->generatorFrames.lookup(genObj)) {
        DebuggerFrame* frameObj = &entry->value()->as<DebuggerFrame>();
        MOZ_ASSERT(&frameObj->unwrappedGenerator() == genObj);
        if (!dbg->frames.putNew(frame, frameObj)) {
          ReportOutOfMemory(cx);
          return ResumeMode::Throw;
        }

        FrameIter iter(cx);
        MOZ_ASSERT(iter.abstractFramePtr() == frame);
        if (!frameObj->resume(iter)) {
          return ResumeMode::Throw;
        }
        if (!Debugger::ensureExecutionObservabilityOfFrame(cx, frame)) {
          return ResumeMode::Throw;
        }
      }
    }
  }

  return slowPathOnEnterFrame(cx, frame);
}

/*
 * RAII class to mark a generator as "running" temporarily while running
 * debugger code.
 *
 * When Debugger::slowPathOnLeaveFrame is called for a frame that is yielding
 * or awaiting, its generator is in the "suspended" state. Letting script
 * observe this state, with the generator on stack yet also reenterable, would
 * be bad, so we mark it running while we fire events.
 */
class MOZ_RAII AutoSetGeneratorRunning {
  int32_t resumeIndex_;
  AsyncGeneratorObject::State asyncGenState_;
  Rooted<AbstractGeneratorObject*> genObj_;

 public:
  AutoSetGeneratorRunning(JSContext* cx,
                          Handle<AbstractGeneratorObject*> genObj)
      : resumeIndex_(0),
        asyncGenState_(static_cast<AsyncGeneratorObject::State>(0)),
        genObj_(cx, genObj) {
    if (genObj) {
      if (!genObj->isClosed() && !genObj->isBeforeInitialYield() &&
          genObj->isSuspended()) {
        // Yielding or awaiting.
        resumeIndex_ = genObj->resumeIndex();
        genObj->setRunning();

        // Async generators have additionally bookkeeping which must be
        // adjusted when switching over to the running state.
        if (genObj->is<AsyncGeneratorObject>()) {
          auto* asyncGenObj = &genObj->as<AsyncGeneratorObject>();
          asyncGenState_ = asyncGenObj->state();
          asyncGenObj->setExecuting();
        }
      } else {
        // Returning or throwing. The generator is already closed, if
        // it was ever exposed at all.
        genObj_ = nullptr;
      }
    }
  }

  ~AutoSetGeneratorRunning() {
    if (genObj_) {
      MOZ_ASSERT(genObj_->isRunning());
      genObj_->setResumeIndex(resumeIndex_);
      if (genObj_->is<AsyncGeneratorObject>()) {
        genObj_->as<AsyncGeneratorObject>().setState(asyncGenState_);
      }
    }
  }
};

/*
 * Handle leaving a frame with debuggers watching. |frameOk| indicates whether
 * the frame is exiting normally or abruptly. Set |cx|'s exception and/or
 * |cx->fp()|'s return value, and return a new success value.
 */
/* static */
bool DebugAPI::slowPathOnLeaveFrame(JSContext* cx, AbstractFramePtr frame,
                                    jsbytecode* pc, bool frameOk) {
  MOZ_ASSERT_IF(!frame.isWasmDebugFrame(), pc);

  mozilla::DebugOnly<Handle<GlobalObject*>> debuggeeGlobal = cx->global();

  // These are updated below, but consulted by the cleanup code we register now,
  // so declare them here, initialized to quiescent values.
  Rooted<Completion> completion(cx);
  bool success = false;

  auto frameMapsGuard = MakeScopeExit([&] {
    // Clean up all Debugger.Frame instances on exit. On suspending, pass the
    // flag that says to leave those frames `.live`. Note that if the completion
    // is a suspension but success is false, the generator gets closed, not
    // suspended.
    Debugger::removeFromFrameMapsAndClearBreakpointsIn(
        cx, frame, success && completion.get().suspending());
  });

  // The onPop handler and associated clean up logic should not run multiple
  // times on the same frame. If slowPathOnLeaveFrame has already been
  // called, the frame will not be present in the Debugger frame maps.
  Rooted<Debugger::DebuggerFrameVector> frames(
      cx, Debugger::DebuggerFrameVector(cx));
  if (!Debugger::getDebuggerFrames(frame, &frames)) {
    return false;
  }
  if (frames.empty()) {
    return frameOk;
  }

  completion = Completion::fromJSFramePop(cx, frame, pc, frameOk);
  Rooted<AbstractGeneratorObject*> genObj(
      cx, completion.get().maybeGeneratorObject());

  // Preserve the debuggee's microtask event queue while we run the hooks, so
  // the debugger's microtask checkpoints don't run from the debuggee's
  // microtasks, and vice versa.
  JS::AutoDebuggerJobQueueInterruption adjqi;
  if (!adjqi.init(cx)) {
    return false;
  }

  // This path can be hit via unwinding the stack due to over-recursion or
  // OOM. In those cases, don't fire the frames' onPop handlers, because
  // invoking JS will only trigger the same condition. See
  // slowPathOnExceptionUnwind.
  if (!cx->isThrowingOverRecursed() && !cx->isThrowingOutOfMemory()) {
    // For each Debugger.Frame, fire its onPop handler, if any.
    for (size_t i = 0; i < frames.length(); i++) {
      HandleDebuggerFrame frameobj = frames[i];
      Debugger* dbg = Debugger::fromChildJSObject(frameobj);
      EnterDebuggeeNoExecute nx(cx, *dbg, adjqi);

      if (dbg->enabled && frameobj->isLive() && frameobj->onPopHandler()) {
        OnPopHandler* handler = frameobj->onPopHandler();

        Maybe<AutoRealm> ar;
        ar.emplace(cx, dbg->object);

        // The resumption requested by the onPop handler we're about to call.
        ResumeMode nextResumeMode;
        RootedValue nextValue(cx);

        // Call the onPop handler.
        bool success;
        {
          // Mark the generator as running, to prevent reentrance.
          //
          // At certain points in a generator's lifetime,
          // GetGeneratorObjectForFrame can return null even when the generator
          // exists, but at those points the generator has not yet been exposed
          // to JavaScript, so reentrance isn't possible anyway. So there's no
          // harm done if this has no effect in that case.
          AutoSetGeneratorRunning asgr(cx, genObj);
          success = handler->onPop(cx, frameobj, completion, nextResumeMode,
                                   &nextValue);
        }
        nextResumeMode = dbg->processParsedHandlerResult(
            ar, frame, pc, success, nextResumeMode, &nextValue);
        adjqi.runJobs();

        // At this point, we are back in the debuggee compartment, and
        // any error has been wrapped up as a completion value.
        MOZ_ASSERT(cx->compartment() == debuggeeGlobal->compartment());
        MOZ_ASSERT(!cx->isExceptionPending());

        completion.get().updateForNextHandler(nextResumeMode, nextValue);
      }
    }
  }

  // Now that we've run all the handlers, extract the final resumption mode. */
  ResumeMode resumeMode;
  RootedValue value(cx);
  RootedSavedFrame exnStack(cx);
  completion.get().toResumeMode(resumeMode, &value, &exnStack);

  switch (resumeMode) {
    case ResumeMode::Return:
      frame.setReturnValue(value);
      success = true;
      return true;

    case ResumeMode::Throw:
      // If we have a stack from the original throw, use it instead of
      // associating the throw with the current execution point.
      if (exnStack) {
        cx->setPendingException(value, exnStack);
      } else {
        cx->setPendingExceptionAndCaptureStack(value);
      }
      return false;

    case ResumeMode::Terminate:
      MOZ_ASSERT(!cx->isExceptionPending());
      return false;

    default:
      MOZ_CRASH("bad final onLeaveFrame resume mode");
  }
}

/* static */
bool DebugAPI::slowPathOnNewGenerator(JSContext* cx, AbstractFramePtr frame,
                                      Handle<AbstractGeneratorObject*> genObj) {
  // This is called from JSOP_GENERATOR, after default parameter expressions
  // are evaluated and well after onEnterFrame, so Debugger.Frame objects for
  // `frame` may already have been exposed to debugger code. The
  // AbstractGeneratorObject for this generator call, though, has just been
  // created. It must be associated with any existing Debugger.Frames.
  bool ok = true;
  Debugger::forEachDebuggerFrame(frame, [&](DebuggerFrame* frameObjPtr) {
    if (!ok) {
      return;
    }

    RootedDebuggerFrame frameObj(cx, frameObjPtr);
    {
      AutoRealm ar(cx, frameObj);

      if (!frameObj->setGenerator(cx, genObj)) {
        ReportOutOfMemory(cx);

        // This leaves `genObj` and `frameObj` unassociated. It's OK
        // because we won't pause again with this generator on the stack:
        // the caller will immediately discard `genObj` and unwind `frame`.
        ok = false;
      }
    }
  });
  return ok;
}

/* static */
ResumeMode DebugAPI::slowPathOnDebuggerStatement(JSContext* cx,
                                                 AbstractFramePtr frame) {
  RootedValue rval(cx);
  ResumeMode resumeMode = Debugger::dispatchHook(
      cx,
      [](Debugger* dbg) -> bool {
        return dbg->getHook(Debugger::OnDebuggerStatement);
      },
      [&](Debugger* dbg) -> ResumeMode {
        return dbg->fireDebuggerStatement(cx, &rval);
      });

  switch (resumeMode) {
    case ResumeMode::Continue:
    case ResumeMode::Terminate:
      break;

    case ResumeMode::Return:
      frame.setReturnValue(rval);
      break;

    case ResumeMode::Throw:
      cx->setPendingExceptionAndCaptureStack(rval);
      break;

    default:
      MOZ_CRASH("Invalid onDebuggerStatement resume mode");
  }

  return resumeMode;
}

/* static */
ResumeMode DebugAPI::slowPathOnExceptionUnwind(JSContext* cx,
                                               AbstractFramePtr frame) {
  // Invoking more JS on an over-recursed stack or after OOM is only going
  // to result in more of the same error.
  if (cx->isThrowingOverRecursed() || cx->isThrowingOutOfMemory()) {
    return ResumeMode::Continue;
  }

  // The Debugger API mustn't muck with frames from self-hosted scripts.
  if (frame.hasScript() && frame.script()->selfHosted()) {
    return ResumeMode::Continue;
  }

  RootedValue rval(cx);
  ResumeMode resumeMode = Debugger::dispatchHook(
      cx, [](Debugger* dbg) -> bool {
        return dbg->getHook(Debugger::OnExceptionUnwind);
      },
      [&](Debugger* dbg) -> ResumeMode {
        return dbg->fireExceptionUnwind(cx, &rval);
      });

  switch (resumeMode) {
    case ResumeMode::Continue:
      break;

    case ResumeMode::Throw:
      cx->setPendingExceptionAndCaptureStack(rval);
      break;

    case ResumeMode::Terminate:
      cx->clearPendingException();
      break;

    case ResumeMode::Return:
      cx->clearPendingException();
      frame.setReturnValue(rval);
      break;

    default:
      MOZ_CRASH("Invalid onExceptionUnwind resume mode");
  }

  return resumeMode;
}

// TODO: Remove Remove this function when all properties/methods returning a
///      DebuggerEnvironment have been given a C++ interface (bug 1271649).
bool Debugger::wrapEnvironment(JSContext* cx, Handle<Env*> env,
                               MutableHandleValue rval) {
  if (!env) {
    rval.setNull();
    return true;
  }

  RootedDebuggerEnvironment envobj(cx);

  if (!wrapEnvironment(cx, env, &envobj)) {
    return false;
  }

  rval.setObject(*envobj);
  return true;
}

bool Debugger::wrapEnvironment(JSContext* cx, Handle<Env*> env,
                               MutableHandleDebuggerEnvironment result) {
  MOZ_ASSERT(env);

  // DebuggerEnv should only wrap a debug scope chain obtained (transitively)
  // from GetDebugEnvironmentFor(Frame|Function).
  MOZ_ASSERT(!IsSyntacticEnvironment(env));

  DependentAddPtr<EnvironmentWeakMap> p(cx, environments, env);
  if (p) {
    result.set(&p->value()->as<DebuggerEnvironment>());
  } else {
    // Create a new Debugger.Environment for env.
    RootedObject proto(
        cx, &object->getReservedSlot(JSSLOT_DEBUG_ENV_PROTO).toObject());
    RootedNativeObject debugger(cx, object);

    RootedDebuggerEnvironment envobj(
        cx, DebuggerEnvironment::create(cx, proto, env, debugger));
    if (!envobj) {
      return false;
    }

    if (!p.add(cx, environments, env, envobj)) {
      NukeDebuggerWrapper(envobj);
      return false;
    }

    result.set(envobj);
  }

  return true;
}

bool Debugger::wrapDebuggeeValue(JSContext* cx, MutableHandleValue vp) {
  cx->check(object.get());

  if (vp.isObject()) {
    RootedObject obj(cx, &vp.toObject());
    RootedDebuggerObject dobj(cx);

    if (!wrapDebuggeeObject(cx, obj, &dobj)) {
      return false;
    }

    vp.setObject(*dobj);
  } else if (vp.isMagic()) {
    RootedPlainObject optObj(cx, NewBuiltinClassInstance<PlainObject>(cx));
    if (!optObj) {
      return false;
    }

    // We handle three sentinel values: missing arguments (overloading
    // JS_OPTIMIZED_ARGUMENTS), optimized out slots (JS_OPTIMIZED_OUT),
    // and uninitialized bindings (JS_UNINITIALIZED_LEXICAL).
    //
    // Other magic values should not have escaped.
    PropertyName* name;
    switch (vp.whyMagic()) {
      case JS_OPTIMIZED_ARGUMENTS:
        name = cx->names().missingArguments;
        break;
      case JS_OPTIMIZED_OUT:
        name = cx->names().optimizedOut;
        break;
      case JS_UNINITIALIZED_LEXICAL:
        name = cx->names().uninitialized;
        break;
      default:
        MOZ_CRASH("Unsupported magic value escaped to Debugger");
    }

    RootedValue trueVal(cx, BooleanValue(true));
    if (!DefineDataProperty(cx, optObj, name, trueVal)) {
      return false;
    }

    vp.setObject(*optObj);
  } else if (!cx->compartment()->wrap(cx, vp)) {
    vp.setUndefined();
    return false;
  }

  return true;
}

bool Debugger::wrapDebuggeeObject(JSContext* cx, HandleObject obj,
                                  MutableHandleDebuggerObject result) {
  MOZ_ASSERT(obj);

  if (obj->is<JSFunction>()) {
    MOZ_ASSERT(!IsInternalFunctionObject(*obj));
    RootedFunction fun(cx, &obj->as<JSFunction>());
    if (!EnsureFunctionHasScript(cx, fun)) {
      return false;
    }
  }

  DependentAddPtr<ObjectWeakMap> p(cx, objects, obj);
  if (p) {
    result.set(&p->value()->as<DebuggerObject>());
  } else {
    // Create a new Debugger.Object for obj.
    RootedNativeObject debugger(cx, object);
    RootedObject proto(
        cx, &object->getReservedSlot(JSSLOT_DEBUG_OBJECT_PROTO).toObject());
    RootedDebuggerObject dobj(cx,
                              DebuggerObject::create(cx, proto, obj, debugger));
    if (!dobj) {
      return false;
    }

    if (!p.add(cx, objects, obj, dobj)) {
      NukeDebuggerWrapper(dobj);
      return false;
    }

    result.set(dobj);
  }

  return true;
}

static NativeObject* ToNativeDebuggerObject(JSContext* cx,
                                            MutableHandleObject obj) {
  if (obj->getClass() != &DebuggerObject::class_) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_EXPECTED_TYPE, "Debugger",
                              "Debugger.Object", obj->getClass()->name);
    return nullptr;
  }

  NativeObject* ndobj = &obj->as<NativeObject>();

  Value owner = ndobj->getReservedSlot(JSSLOT_DEBUGOBJECT_OWNER);
  if (owner.isUndefined()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEBUG_PROTO,
                              "Debugger.Object", "Debugger.Object");
    return nullptr;
  }

  return ndobj;
}

bool Debugger::unwrapDebuggeeObject(JSContext* cx, MutableHandleObject obj) {
  NativeObject* ndobj = ToNativeDebuggerObject(cx, obj);
  if (!ndobj) {
    return false;
  }

  Value owner = ndobj->getReservedSlot(JSSLOT_DEBUGOBJECT_OWNER);
  if (&owner.toObject() != object) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_WRONG_OWNER, "Debugger.Object");
    return false;
  }

  obj.set(static_cast<JSObject*>(ndobj->getPrivate()));
  return true;
}

bool Debugger::unwrapDebuggeeValue(JSContext* cx, MutableHandleValue vp) {
  cx->check(object.get(), vp);
  if (vp.isObject()) {
    RootedObject dobj(cx, &vp.toObject());
    if (!unwrapDebuggeeObject(cx, &dobj)) {
      return false;
    }
    vp.setObject(*dobj);
  }
  return true;
}

static bool CheckArgCompartment(JSContext* cx, JSObject* obj, JSObject* arg,
                                const char* methodname, const char* propname) {
  if (arg->compartment() != obj->compartment()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_COMPARTMENT_MISMATCH, methodname,
                              propname);
    return false;
  }
  return true;
}

static bool CheckArgCompartment(JSContext* cx, JSObject* obj, HandleValue v,
                                const char* methodname, const char* propname) {
  if (v.isObject()) {
    return CheckArgCompartment(cx, obj, &v.toObject(), methodname, propname);
  }
  return true;
}

bool Debugger::unwrapPropertyDescriptor(
    JSContext* cx, HandleObject obj, MutableHandle<PropertyDescriptor> desc) {
  if (desc.hasValue()) {
    RootedValue value(cx, desc.value());
    if (!unwrapDebuggeeValue(cx, &value) ||
        !CheckArgCompartment(cx, obj, value, "defineProperty", "value")) {
      return false;
    }
    desc.setValue(value);
  }

  if (desc.hasGetterObject()) {
    RootedObject get(cx, desc.getterObject());
    if (get) {
      if (!unwrapDebuggeeObject(cx, &get)) {
        return false;
      }
      if (!CheckArgCompartment(cx, obj, get, "defineProperty", "get")) {
        return false;
      }
    }
    desc.setGetterObject(get);
  }

  if (desc.hasSetterObject()) {
    RootedObject set(cx, desc.setterObject());
    if (set) {
      if (!unwrapDebuggeeObject(cx, &set)) {
        return false;
      }
      if (!CheckArgCompartment(cx, obj, set, "defineProperty", "set")) {
        return false;
      }
    }
    desc.setSetterObject(set);
  }

  return true;
}

/*** Debuggee resumption values and debugger error handling *****************/

static bool GetResumptionProperty(JSContext* cx, HandleObject obj,
                                  HandlePropertyName name, ResumeMode namedMode,
                                  ResumeMode& resumeMode, MutableHandleValue vp,
                                  int* hits) {
  bool found;
  if (!HasProperty(cx, obj, name, &found)) {
    return false;
  }
  if (found) {
    ++*hits;
    resumeMode = namedMode;
    if (!GetProperty(cx, obj, obj, name, vp)) {
      return false;
    }
  }
  return true;
}

bool js::ParseResumptionValue(JSContext* cx, HandleValue rval,
                              ResumeMode& resumeMode, MutableHandleValue vp) {
  if (rval.isUndefined()) {
    resumeMode = ResumeMode::Continue;
    vp.setUndefined();
    return true;
  }
  if (rval.isNull()) {
    resumeMode = ResumeMode::Terminate;
    vp.setUndefined();
    return true;
  }

  int hits = 0;
  if (rval.isObject()) {
    RootedObject obj(cx, &rval.toObject());
    if (!GetResumptionProperty(cx, obj, cx->names().return_, ResumeMode::Return,
                               resumeMode, vp, &hits)) {
      return false;
    }
    if (!GetResumptionProperty(cx, obj, cx->names().throw_, ResumeMode::Throw,
                               resumeMode, vp, &hits)) {
      return false;
    }
  }

  if (hits != 1) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_BAD_RESUMPTION);
    return false;
  }
  return true;
}

static bool GetThisValueForCheck(JSContext* cx, AbstractFramePtr frame,
                                 jsbytecode* pc, MutableHandleValue thisv,
                                 Maybe<HandleValue>& maybeThisv) {
  if (frame.debuggerNeedsCheckPrimitiveReturn()) {
    {
      AutoRealm ar(cx, frame.environmentChain());
      if (!GetThisValueForDebuggerMaybeOptimizedOut(cx, frame, pc, thisv)) {
        return false;
      }
    }

    if (!cx->compartment()->wrap(cx, thisv)) {
      return false;
    }

    MOZ_ASSERT_IF(thisv.isMagic(), thisv.isMagic(JS_UNINITIALIZED_LEXICAL));
    maybeThisv.emplace(HandleValue(thisv));
  }

  return true;
}

static bool CheckResumptionValue(JSContext* cx, AbstractFramePtr frame,
                                 const Maybe<HandleValue>& maybeThisv,
                                 ResumeMode resumeMode, MutableHandleValue vp) {
  if (maybeThisv.isSome()) {
    const HandleValue& thisv = maybeThisv.ref();
    if (resumeMode == ResumeMode::Return && vp.isPrimitive()) {
      // Forcing return from a class constructor. There are rules.
      if (vp.isUndefined()) {
        if (thisv.isMagic(JS_UNINITIALIZED_LEXICAL)) {
          return ThrowUninitializedThis(cx, frame);
        }

        vp.set(thisv);
      } else {
        ReportValueError(cx, JSMSG_BAD_DERIVED_RETURN, JSDVG_IGNORE_STACK, vp,
                         nullptr);
        return false;
      }
    }
  }

  // Check for forcing return from a generator before the initial yield. This
  // is not supported because some engine-internal code assumes a call to a
  // generator will return a GeneratorObject; see bug 1477084.
  if (resumeMode == ResumeMode::Return && frame && frame.isFunctionFrame() &&
      frame.callee()->isGenerator()) {
    Rooted<AbstractGeneratorObject*> genObj(cx);
    {
      AutoRealm ar(cx, frame.callee());
      genObj = GetGeneratorObjectForFrame(cx, frame);
    }

    if (!genObj || genObj->isBeforeInitialYield()) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_DEBUG_FORCED_RETURN_DISALLOWED);
      return false;
    }
  }

  return true;
}

// Last-minute sanity adjustments to resumption.
//
// This is called last, as we leave the debugger. It must happen outside the
// control of the uncaughtExceptionHook, because this code assumes we won't
// change our minds and continue execution--we must not close the generator
// object unless we're really going to force-return.
static void AdjustGeneratorResumptionValue(JSContext* cx,
                                           AbstractFramePtr frame,
                                           ResumeMode& resumeMode,
                                           MutableHandleValue vp) {
  if (resumeMode != ResumeMode::Return && resumeMode != ResumeMode::Throw) {
    return;
  }
  if (!frame || !frame.isFunctionFrame()) {
    return;
  }

  // To propagate out of memory in debuggee code.
  auto getAndClearExceptionThenThrow = [&]() {
    MOZ_ALWAYS_TRUE(cx->getPendingException(vp));
    cx->clearPendingException();
    resumeMode = ResumeMode::Throw;
  };

  // Treat `{return: <value>}` like a `return` statement. Simulate what the
  // debuggee would do for an ordinary `return` statement, using a few bytecode
  // instructions. It's simpler to do the work manually than to count on that
  // bytecode sequence existing in the debuggee, somehow jump to it, and then
  // avoid re-entering the debugger from it.
  //
  // Similarly treat `{throw: <value>}` like a `throw` statement.
  if (frame.callee()->isGenerator()) {
    // Throw doesn't require any special processing for (async) generators.
    if (resumeMode == ResumeMode::Throw) {
      return;
    }

    // Forcing return from a (possibly async) generator.
    Rooted<AbstractGeneratorObject*> genObj(
        cx, GetGeneratorObjectForFrame(cx, frame));

    // We already went through CheckResumptionValue, which would have replaced
    // this invalid resumption value with an error if we were trying to force
    // return before the initial yield.
    MOZ_RELEASE_ASSERT(genObj && !genObj->isBeforeInitialYield());

    // 1.  `return <value>` creates and returns a new object,
    //     `{value: <value>, done: true}`.
    //
    // For non-async generators, the iterator result object is created in
    // bytecode, so we have to simulate that here. For async generators, our
    // C++ implementation of AsyncGeneratorResolve will do this. So don't do it
    // twice:
    if (!genObj->is<AsyncGeneratorObject>()) {
      JSObject* pair = CreateIterResultObject(cx, vp, true);
      if (!pair) {
        getAndClearExceptionThenThrow();
        return;
      }
      vp.setObject(*pair);
    }

    // 2.  The generator must be closed.
    genObj->setClosed();

    // Async generators have additionally bookkeeping which must be adjusted
    // when switching over to the closed state.
    if (genObj->is<AsyncGeneratorObject>()) {
      genObj->as<AsyncGeneratorObject>().setCompleted();
    }
  } else if (frame.callee()->isAsync()) {
    if (AbstractGeneratorObject* genObj =
            GetGeneratorObjectForFrame(cx, frame)) {
      // Throw doesn't require any special processing for async functions when
      // the internal generator object is already present.
      if (resumeMode == ResumeMode::Throw) {
        return;
      }

      Rooted<AsyncFunctionGeneratorObject*> asyncGenObj(
          cx, &genObj->as<AsyncFunctionGeneratorObject>());

      // 1.  `return <value>` fulfills and returns the async function's promise.
      Rooted<PromiseObject*> promise(cx, asyncGenObj->promise());
      if (promise->state() == JS::PromiseState::Pending) {
        if (!AsyncFunctionResolve(cx, asyncGenObj, vp,
                                  AsyncFunctionResolveKind::Fulfill)) {
          getAndClearExceptionThenThrow();
          return;
        }
      }
      vp.setObject(*promise);

      // 2.  The generator must be closed.
      asyncGenObj->setClosed();
    } else {
      // We're before entering the actual function code.

      // 1.  `throw <value>` creates a promise rejected with the value *vp.
      // 1.  `return <value>` creates a promise resolved with the value *vp.
      JSObject* promise = resumeMode == ResumeMode::Throw
                              ? PromiseObject::unforgeableReject(cx, vp)
                              : PromiseObject::unforgeableResolve(cx, vp);
      if (!promise) {
        getAndClearExceptionThenThrow();
        return;
      }
      vp.setObject(*promise);

      // 2.  Return normally in both cases.
      resumeMode = ResumeMode::Return;
    }
  }
}

ResumeMode Debugger::reportUncaughtException(Maybe<AutoRealm>& ar) {
  JSContext* cx = ar->context();

  // Uncaught exceptions arise from Debugger code, and so we must already be
  // in an NX section.
  MOZ_ASSERT(EnterDebuggeeNoExecute::isLockedInStack(cx, *this));

  if (cx->isExceptionPending()) {
    // We want to report the pending exception, but we want to let the
    // embedding handle it however it wants to.  So pretend like we're
    // starting a new script execution on our current compartment (which
    // is the debugger compartment, so reported errors won't get
    // reported to various onerror handlers in debuggees) and as part of
    // that "execution" simply throw our exception so the embedding can
    // deal.
    RootedValue exn(cx);
    if (cx->getPendingException(&exn)) {
      // Clear the exception, because ReportErrorToGlobal will assert that
      // we don't have one.
      cx->clearPendingException();
      ReportErrorToGlobal(cx, cx->global(), exn);
    }

    // And if not, or if PrepareScriptEnvironmentAndInvoke somehow left an
    // exception on cx (which it totally shouldn't do), just give up.
    cx->clearPendingException();
  }

  ar.reset();
  return ResumeMode::Terminate;
}

ResumeMode Debugger::handleUncaughtExceptionHelper(
    Maybe<AutoRealm>& ar, MutableHandleValue* vp,
    const Maybe<HandleValue>& thisVForCheck, AbstractFramePtr frame) {
  JSContext* cx = ar->context();

  // Uncaught exceptions arise from Debugger code, and so we must already be in
  // an NX section. This also establishes that we are already within the scope
  // of an AutoDebuggerJobQueueInterruption object.
  MOZ_ASSERT(EnterDebuggeeNoExecute::isLockedInStack(cx, *this));

  if (cx->isExceptionPending()) {
    if (uncaughtExceptionHook) {
      RootedValue exc(cx);
      if (!cx->getPendingException(&exc)) {
        return ResumeMode::Terminate;
      }
      cx->clearPendingException();

      RootedValue fval(cx, ObjectValue(*uncaughtExceptionHook));
      RootedValue rv(cx);
      if (js::Call(cx, fval, object, exc, &rv)) {
        if (vp) {
          ResumeMode resumeMode = ResumeMode::Continue;
          if (!ParseResumptionValue(cx, rv, resumeMode, *vp)) {
            return reportUncaughtException(ar);
          }
          return leaveDebugger(ar, frame, thisVForCheck,
                               CallUncaughtExceptionHook::No, resumeMode, *vp);
        } else {
          // Caller is something like onGarbageCollectionHook that
          // doesn't allow Debugger to control debuggee resumption.
          // The return value from uncaughtExceptionHook is ignored.
          return ResumeMode::Continue;
        }
      }
    }

    return reportUncaughtException(ar);
  }

  ar.reset();
  return ResumeMode::Terminate;
}

ResumeMode Debugger::handleUncaughtException(
    Maybe<AutoRealm>& ar, MutableHandleValue vp,
    const Maybe<HandleValue>& thisVForCheck, AbstractFramePtr frame) {
  return handleUncaughtExceptionHelper(ar, &vp, thisVForCheck, frame);
}

ResumeMode Debugger::handleUncaughtException(Maybe<AutoRealm>& ar) {
  return handleUncaughtExceptionHelper(ar, nullptr, mozilla::Nothing(),
                                       NullFramePtr());
}

ResumeMode Debugger::leaveDebugger(Maybe<AutoRealm>& ar, AbstractFramePtr frame,
                                   const Maybe<HandleValue>& maybeThisv,
                                   CallUncaughtExceptionHook callHook,
                                   ResumeMode resumeMode,
                                   MutableHandleValue vp) {
  JSContext* cx = ar->context();
  if (!unwrapDebuggeeValue(cx, vp) ||
      !CheckResumptionValue(cx, frame, maybeThisv, resumeMode, vp)) {
    if (callHook == CallUncaughtExceptionHook::Yes) {
      return handleUncaughtException(ar, vp, maybeThisv, frame);
    }
    return reportUncaughtException(ar);
  }

  ar.reset();
  if (!cx->compartment()->wrap(cx, vp)) {
    resumeMode = ResumeMode::Terminate;
    vp.setUndefined();
  }
  AdjustGeneratorResumptionValue(cx, frame, resumeMode, vp);

  return resumeMode;
}

ResumeMode Debugger::processParsedHandlerResult(Maybe<AutoRealm>& ar,
                                                AbstractFramePtr frame,
                                                jsbytecode* pc, bool success,
                                                ResumeMode resumeMode,
                                                MutableHandleValue vp) {
  JSContext* cx = ar->context();

  RootedValue thisv(cx);
  Maybe<HandleValue> maybeThisv;
  if (!GetThisValueForCheck(cx, frame, pc, &thisv, maybeThisv)) {
    ar.reset();
    return ResumeMode::Terminate;
  }

  if (!success) {
    return handleUncaughtException(ar, vp, maybeThisv, frame);
  }

  return leaveDebugger(ar, frame, maybeThisv, CallUncaughtExceptionHook::Yes,
                       resumeMode, vp);
}

ResumeMode Debugger::processHandlerResult(Maybe<AutoRealm>& ar, bool success,
                                          const Value& rv,
                                          AbstractFramePtr frame,
                                          jsbytecode* pc,
                                          MutableHandleValue vp) {
  JSContext* cx = ar->context();

  RootedValue thisv(cx);
  Maybe<HandleValue> maybeThisv;
  if (!GetThisValueForCheck(cx, frame, pc, &thisv, maybeThisv)) {
    ar.reset();
    return ResumeMode::Terminate;
  }

  if (!success) {
    return handleUncaughtException(ar, vp, maybeThisv, frame);
  }

  RootedValue rootRv(cx, rv);
  ResumeMode resumeMode = ResumeMode::Continue;
  if (!ParseResumptionValue(cx, rootRv, resumeMode, vp)) {
    return handleUncaughtException(ar, vp, maybeThisv, frame);
  }
  return leaveDebugger(ar, frame, maybeThisv, CallUncaughtExceptionHook::Yes,
                       resumeMode, vp);
}

/*** Debuggee completion values *********************************************/

/* static */
Completion Completion::fromJSResult(JSContext* cx, bool ok, const Value& rv) {
  MOZ_ASSERT_IF(ok, !cx->isExceptionPending());

  if (ok) {
    return Completion(Return(rv));
  }

  if (!cx->isExceptionPending()) {
    return Completion(Terminate());
  }

  RootedValue exception(cx);
  RootedSavedFrame stack(cx, cx->getPendingExceptionStack());
  MOZ_ALWAYS_TRUE(cx->getPendingException(&exception));

  cx->clearPendingException();

  return Completion(Throw(exception, stack));
}

/* static */
Completion Completion::fromJSFramePop(JSContext* cx, AbstractFramePtr frame,
                                      const jsbytecode* pc, bool ok) {
  // Only Wasm frames get a null pc.
  MOZ_ASSERT_IF(!frame.isWasmDebugFrame(), pc);

  // If this isn't a generator suspension, then that's already handled above.
  if (!ok || !frame.isGeneratorFrame()) {
    return fromJSResult(cx, ok, frame.returnValue());
  }

  // A generator is being suspended or returning.

  // Since generators are never wasm, we can assume pc is not nullptr, and
  // that analyzing bytecode is meaningful.
  MOZ_ASSERT(!frame.isWasmDebugFrame());

  // If we're leaving successfully at a yield opcode, we're probably
  // suspending; the `isClosed()` check detects a debugger forced return from
  // an `onStep` handler, which looks almost the same.
  //
  // GetGeneratorObjectForFrame can return nullptr even when a generator
  // object does exist, if the frame is paused between the GENERATOR and
  // SETALIASEDVAR opcodes. But by checking the opcode first we eliminate that
  // possibility, so it's fine to call genObj->isClosed().
  Rooted<AbstractGeneratorObject*> generatorObj(
      cx, GetGeneratorObjectForFrame(cx, frame));
  switch (*pc) {
    case JSOP_INITIALYIELD:
      MOZ_ASSERT(!generatorObj->isClosed());
      return Completion(InitialYield(generatorObj));

    case JSOP_YIELD:
      MOZ_ASSERT(!generatorObj->isClosed());
      return Completion(Yield(generatorObj, frame.returnValue()));

    case JSOP_AWAIT:
      MOZ_ASSERT(!generatorObj->isClosed());
      return Completion(Await(generatorObj, frame.returnValue()));

    default:
      return Completion(Return(frame.returnValue()));
  }
}

void Completion::trace(JSTracer* trc) {
  variant.match([=](auto& var) { var.trace(trc); });
}

struct MOZ_STACK_CLASS Completion::BuildValueMatcher {
  JSContext* cx;
  Debugger* dbg;
  MutableHandleValue result;

  BuildValueMatcher(JSContext* cx, Debugger* dbg, MutableHandleValue result)
      : cx(cx), dbg(dbg), result(result) {
    cx->check(dbg->toJSObject());
  }

  bool operator()(const Completion::Return& ret) {
    RootedNativeObject obj(cx, newObject());
    RootedValue retval(cx, ret.value);
    if (!obj || !wrap(&retval) || !add(obj, cx->names().return_, retval)) {
      return false;
    }
    result.setObject(*obj);
    return true;
  }

  bool operator()(const Completion::Throw& thr) {
    RootedNativeObject obj(cx, newObject());
    RootedValue exc(cx, thr.exception);
    if (!obj || !wrap(&exc) || !add(obj, cx->names().throw_, exc)) {
      return false;
    }
    if (thr.stack) {
      RootedValue stack(cx, ObjectValue(*thr.stack));
      if (!wrapStack(&stack) || !add(obj, cx->names().stack, stack)) {
        return false;
      }
    }
    result.setObject(*obj);
    return true;
  }

  bool operator()(const Completion::Terminate& term) {
    result.setNull();
    return true;
  }

  bool operator()(const Completion::InitialYield& initialYield) {
    RootedNativeObject obj(cx, newObject());
    RootedValue gen(cx, ObjectValue(*initialYield.generatorObject));
    if (!obj || !wrap(&gen) || !add(obj, cx->names().return_, gen) ||
        !add(obj, cx->names().yield, TrueHandleValue) ||
        !add(obj, cx->names().initial, TrueHandleValue)) {
      return false;
    }
    result.setObject(*obj);
    return true;
  }

  bool operator()(const Completion::Yield& yield) {
    RootedNativeObject obj(cx, newObject());
    RootedValue iteratorResult(cx, yield.iteratorResult);
    if (!obj || !wrap(&iteratorResult) ||
        !add(obj, cx->names().return_, iteratorResult) ||
        !add(obj, cx->names().yield, TrueHandleValue)) {
      return false;
    }
    result.setObject(*obj);
    return true;
  }

  bool operator()(const Completion::Await& await) {
    RootedNativeObject obj(cx, newObject());
    RootedValue awaitee(cx, await.awaitee);
    if (!obj || !wrap(&awaitee) || !add(obj, cx->names().return_, awaitee) ||
        !add(obj, cx->names().await, TrueHandleValue)) {
      return false;
    }
    result.setObject(*obj);
    return true;
  }

 private:
  NativeObject* newObject() const {
    return NewBuiltinClassInstance<PlainObject>(cx);
  }

  bool add(HandleNativeObject obj, PropertyName* name,
           HandleValue value) const {
    return NativeDefineDataProperty(cx, obj, name, value, JSPROP_ENUMERATE);
  }

  bool wrap(MutableHandleValue v) const {
    return dbg->wrapDebuggeeValue(cx, v);
  }

  // Saved stacks are wrapped for direct consumption by debugger code.
  bool wrapStack(MutableHandleValue stack) const {
    return cx->compartment()->wrap(cx, stack);
  }
};

bool Completion::buildCompletionValue(JSContext* cx, Debugger* dbg,
                                      MutableHandleValue result) const {
  return variant.match(BuildValueMatcher(cx, dbg, result));
}

AbstractGeneratorObject* Completion::maybeGeneratorObject() const {
  if (variant.is<InitialYield>()) {
    return variant.as<InitialYield>().generatorObject;
  }

  if (variant.is<Yield>()) {
    return variant.as<Yield>().generatorObject;
  }

  if (variant.is<Await>()) {
    return variant.as<Await>().generatorObject;
  }

  return nullptr;
}

void Completion::updateForNextHandler(ResumeMode resumeMode,
                                      HandleValue value) {
  switch (resumeMode) {
    case ResumeMode::Continue:
      // No change to how we'll resume.
      break;

    case ResumeMode::Throw:
      // Since this is a new exception, the stack for the old one may not apply.
      // If we extend resumption values to specify stacks, we could revisit
      // this.
      variant = Variant(Throw(value, nullptr));
      break;

    case ResumeMode::Terminate:
      variant = Variant(Terminate());
      break;

    case ResumeMode::Return:
      variant = Variant(Return(value));
      break;

    default:
      MOZ_CRASH("invalid resumeMode value");
  }
}

struct MOZ_STACK_CLASS Completion::ToResumeModeMatcher {
  MutableHandleValue value;
  MutableHandleSavedFrame exnStack;
  ToResumeModeMatcher(MutableHandleValue value,
                      MutableHandleSavedFrame exnStack)
      : value(value), exnStack(exnStack) {}

  ResumeMode operator()(const Return& ret) {
    value.set(ret.value);
    return ResumeMode::Return;
  }

  ResumeMode operator()(const Throw& thr) {
    value.set(thr.exception);
    exnStack.set(thr.stack);
    return ResumeMode::Throw;
  }

  ResumeMode operator()(const Terminate& term) {
    value.setUndefined();
    return ResumeMode::Terminate;
  }

  ResumeMode operator()(const InitialYield& initialYield) {
    value.setObject(*initialYield.generatorObject);
    return ResumeMode::Return;
  }

  ResumeMode operator()(const Yield& yield) {
    value.set(yield.iteratorResult);
    return ResumeMode::Return;
  }

  ResumeMode operator()(const Await& await) {
    value.set(await.awaitee);
    return ResumeMode::Return;
  }
};

void Completion::toResumeMode(ResumeMode& resumeMode, MutableHandleValue value,
                              MutableHandleSavedFrame exnStack) const {
  resumeMode = variant.match(ToResumeModeMatcher(value, exnStack));
}

/*** Firing debugger hooks **************************************************/

static bool CallMethodIfPresent(JSContext* cx, HandleObject obj,
                                const char* name, size_t argc, Value* argv,
                                MutableHandleValue rval) {
  rval.setUndefined();
  JSAtom* atom = Atomize(cx, name, strlen(name));
  if (!atom) {
    return false;
  }

  RootedId id(cx, AtomToId(atom));
  RootedValue fval(cx);
  if (!GetProperty(cx, obj, obj, id, &fval)) {
    return false;
  }

  if (!IsCallable(fval)) {
    return true;
  }

  InvokeArgs args(cx);
  if (!args.init(cx, argc)) {
    return false;
  }

  for (size_t i = 0; i < argc; i++) {
    args[i].set(argv[i]);
  }

  rval.setObject(*obj);  // overwritten by successful Call
  return js::Call(cx, fval, rval, args, rval);
}

ResumeMode Debugger::fireDebuggerStatement(JSContext* cx,
                                           MutableHandleValue vp) {
  RootedObject hook(cx, getHook(OnDebuggerStatement));
  MOZ_ASSERT(hook);
  MOZ_ASSERT(hook->isCallable());

  Maybe<AutoRealm> ar;
  ar.emplace(cx, object);

  ScriptFrameIter iter(cx);
  RootedValue scriptFrame(cx);
  if (!getFrame(cx, iter, &scriptFrame)) {
    return reportUncaughtException(ar);
  }

  RootedValue fval(cx, ObjectValue(*hook));
  RootedValue rv(cx);
  bool ok = js::Call(cx, fval, object, scriptFrame, &rv);
  return processHandlerResult(ar, ok, rv, iter.abstractFramePtr(), iter.pc(),
                              vp);
}

ResumeMode Debugger::fireExceptionUnwind(JSContext* cx, MutableHandleValue vp) {
  RootedObject hook(cx, getHook(OnExceptionUnwind));
  MOZ_ASSERT(hook);
  MOZ_ASSERT(hook->isCallable());

  RootedValue exc(cx);
  RootedSavedFrame stack(cx, cx->getPendingExceptionStack());
  if (!cx->getPendingException(&exc)) {
    return ResumeMode::Terminate;
  }
  cx->clearPendingException();

  Maybe<AutoRealm> ar;
  ar.emplace(cx, object);

  RootedValue scriptFrame(cx);
  RootedValue wrappedExc(cx, exc);

  FrameIter iter(cx);
  if (!getFrame(cx, iter, &scriptFrame) ||
      !wrapDebuggeeValue(cx, &wrappedExc)) {
    return reportUncaughtException(ar);
  }

  RootedValue fval(cx, ObjectValue(*hook));
  RootedValue rv(cx);
  bool ok = js::Call(cx, fval, object, scriptFrame, wrappedExc, &rv);
  ResumeMode resumeMode =
      processHandlerResult(ar, ok, rv, iter.abstractFramePtr(), iter.pc(), vp);
  if (resumeMode == ResumeMode::Continue) {
    cx->setPendingException(exc, stack);
  }
  return resumeMode;
}

ResumeMode Debugger::fireEnterFrame(JSContext* cx, MutableHandleValue vp) {
  RootedObject hook(cx, getHook(OnEnterFrame));
  MOZ_ASSERT(hook);
  MOZ_ASSERT(hook->isCallable());

  RootedValue scriptFrame(cx);

  FrameIter iter(cx);

#if DEBUG
  // Assert that the hook won't be able to re-enter the generator.
  if (iter.hasScript() && *iter.pc() == JSOP_AFTERYIELD) {
    auto* genObj = GetGeneratorObjectForFrame(cx, iter.abstractFramePtr());
    MOZ_ASSERT(genObj->isRunning());
  }
#endif

  Maybe<AutoRealm> ar;
  ar.emplace(cx, object);

  if (!getFrame(cx, iter, &scriptFrame)) {
    return reportUncaughtException(ar);
  }

  RootedValue fval(cx, ObjectValue(*hook));
  RootedValue rv(cx);
  bool ok = js::Call(cx, fval, object, scriptFrame, &rv);

  return processHandlerResult(ar, ok, rv, iter.abstractFramePtr(), iter.pc(),
                              vp);
}

void Debugger::fireNewScript(JSContext* cx,
                             Handle<DebuggerScriptReferent> scriptReferent) {
  RootedObject hook(cx, getHook(OnNewScript));
  MOZ_ASSERT(hook);
  MOZ_ASSERT(hook->isCallable());

  Maybe<AutoRealm> ar;
  ar.emplace(cx, object);

  JSObject* dsobj = wrapVariantReferent(cx, scriptReferent);
  if (!dsobj) {
    reportUncaughtException(ar);
    return;
  }

  RootedValue fval(cx, ObjectValue(*hook));
  RootedValue dsval(cx, ObjectValue(*dsobj));
  RootedValue rv(cx);
  if (!js::Call(cx, fval, object, dsval, &rv)) {
    handleUncaughtException(ar);
  }
}

void Debugger::fireOnGarbageCollectionHook(
    JSContext* cx, const JS::dbg::GarbageCollectionEvent::Ptr& gcData) {
  MOZ_ASSERT(observedGC(gcData->majorGCNumber()));
  observedGCs.remove(gcData->majorGCNumber());

  RootedObject hook(cx, getHook(OnGarbageCollection));
  MOZ_ASSERT(hook);
  MOZ_ASSERT(hook->isCallable());

  Maybe<AutoRealm> ar;
  ar.emplace(cx, object);

  JSObject* dataObj = gcData->toJSObject(cx);
  if (!dataObj) {
    reportUncaughtException(ar);
    return;
  }

  RootedValue fval(cx, ObjectValue(*hook));
  RootedValue dataVal(cx, ObjectValue(*dataObj));
  RootedValue rv(cx);
  if (!js::Call(cx, fval, object, dataVal, &rv)) {
    handleUncaughtException(ar);
  }
}

template <typename HookIsEnabledFun /* bool (Debugger*) */,
          typename FireHookFun /* ResumeMode (Debugger*) */>
/* static */
ResumeMode Debugger::dispatchHook(JSContext* cx, HookIsEnabledFun hookIsEnabled,
                                  FireHookFun fireHook) {
  // Determine which debuggers will receive this event, and in what order.
  // Make a copy of the list, since the original is mutable and we will be
  // calling into arbitrary JS.
  //
  // Note: In the general case, 'triggered' contains references to objects in
  // different compartments--every compartment *except* this one.
  RootedValueVector triggered(cx);
  Handle<GlobalObject*> global = cx->global();
  if (GlobalObject::DebuggerVector* debuggers = global->getDebuggers()) {
    for (auto p = debuggers->begin(); p != debuggers->end(); p++) {
      Debugger* dbg = *p;
      if (dbg->enabled && hookIsEnabled(dbg)) {
        if (!triggered.append(ObjectValue(*dbg->toJSObject()))) {
          return ResumeMode::Terminate;
        }
      }
    }
  }

  // Preserve the debuggee's microtask event queue while we run the hooks, so
  // the debugger's microtask checkpoints don't run from the debuggee's
  // microtasks, and vice versa.
  JS::AutoDebuggerJobQueueInterruption adjqi;
  if (!adjqi.init(cx)) {
    return ResumeMode::Terminate;
  }

  // Deliver the event to each debugger, checking again to make sure it
  // should still be delivered.
  for (Value* p = triggered.begin(); p != triggered.end(); p++) {
    Debugger* dbg = Debugger::fromJSObject(&p->toObject());
    EnterDebuggeeNoExecute nx(cx, *dbg, adjqi);
    if (dbg->debuggees.has(global) && dbg->enabled && hookIsEnabled(dbg)) {
      ResumeMode resumeMode = fireHook(dbg);
      adjqi.runJobs();
      if (resumeMode != ResumeMode::Continue) {
        return resumeMode;
      }
    }
  }
  return ResumeMode::Continue;
}

void DebugAPI::slowPathOnNewScript(JSContext* cx, HandleScript script) {
  ResumeMode resumeMode = Debugger::dispatchHook(
      cx,
      [script](Debugger* dbg) -> bool {
        return dbg->observesNewScript() && dbg->observesScript(script);
      },
      [&](Debugger* dbg) -> ResumeMode {
        Rooted<DebuggerScriptReferent> scriptReferent(cx, script.get());
        dbg->fireNewScript(cx, scriptReferent);
        return ResumeMode::Continue;
      });

  // dispatchHook may fail due to OOM. This OOM is not handlable at the
  // callsites of onNewScript in the engine.
  if (resumeMode == ResumeMode::Terminate) {
    cx->clearPendingException();
    return;
  }

  MOZ_ASSERT(resumeMode == ResumeMode::Continue);
}

void DebugAPI::slowPathOnNewWasmInstance(
    JSContext* cx, Handle<WasmInstanceObject*> wasmInstance) {
  ResumeMode resumeMode = Debugger::dispatchHook(
      cx,
      [wasmInstance](Debugger* dbg) -> bool {
        return dbg->observesNewScript() &&
               dbg->observesGlobal(&wasmInstance->global());
      },
      [&](Debugger* dbg) -> ResumeMode {
        Rooted<DebuggerScriptReferent> scriptReferent(cx, wasmInstance.get());
        dbg->fireNewScript(cx, scriptReferent);
        return ResumeMode::Continue;
      });

  // dispatchHook may fail due to OOM. This OOM is not handlable at the
  // callsites of onNewWasmInstance in the engine.
  if (resumeMode == ResumeMode::Terminate) {
    cx->clearPendingException();
    return;
  }

  MOZ_ASSERT(resumeMode == ResumeMode::Continue);
}

/* static */
ResumeMode DebugAPI::onTrap(JSContext* cx, MutableHandleValue vp) {
  FrameIter iter(cx);
  JS::AutoSaveExceptionState savedExc(cx);
  Rooted<GlobalObject*> global(cx);
  BreakpointSite* site;
  bool isJS;       // true when iter.hasScript(), false when iter.isWasm()
  jsbytecode* pc;  // valid when isJS == true
  uint32_t bytecodeOffset;  // valid when isJS == false
  if (iter.hasScript()) {
    RootedScript script(cx, iter.script());
    MOZ_ASSERT(script->isDebuggee());
    global.set(&script->global());
    isJS = true;
    pc = iter.pc();
    bytecodeOffset = 0;
    site = DebugScript::getBreakpointSite(script, pc);
  } else {
    MOZ_ASSERT(iter.isWasm());
    global.set(&iter.wasmInstance()->object()->global());
    isJS = false;
    pc = nullptr;
    bytecodeOffset = iter.wasmBytecodeOffset();
    site = iter.wasmInstance()->debug().getBreakpointSite(cx, bytecodeOffset);
  }

  // Build list of breakpoint handlers.
  Vector<Breakpoint*> triggered(cx);
  for (Breakpoint* bp = site->firstBreakpoint(); bp; bp = bp->nextInSite()) {
    // Skip a breakpoint that is not set for the current wasm::Instance --
    // single wasm::Code can handle breakpoints for mutiple instances.
    if (!isJS &&
        &bp->asWasm()->wasmInstance->instance() != iter.wasmInstance()) {
      continue;
    }
    if (!triggered.append(bp)) {
      return ResumeMode::Terminate;
    }
  }

  if (triggered.length() > 0) {
    // Preserve the debuggee's microtask event queue while we run the hooks, so
    // the debugger's microtask checkpoints don't run from the debuggee's
    // microtasks, and vice versa.
    JS::AutoDebuggerJobQueueInterruption adjqi;
    if (!adjqi.init(cx)) {
      return ResumeMode::Terminate;
    }

    for (Breakpoint* bp : triggered) {
      // Handlers can clear breakpoints. Check that bp still exists.
      if (!site || !site->hasBreakpoint(bp)) {
        continue;
      }

      // There are two reasons we have to check whether dbg is enabled and
      // debugging global.
      //
      // One is just that one breakpoint handler can disable other Debuggers
      // or remove debuggees.
      //
      // The other has to do with non-compile-and-go scripts, which have no
      // specific global--until they are executed. Only now do we know which
      // global the script is running against.
      Debugger* dbg = bp->debugger;
      bool hasDebuggee = dbg->enabled && dbg->debuggees.has(global);
      if (hasDebuggee) {
        Maybe<AutoRealm> ar;
        ar.emplace(cx, dbg->object);
        EnterDebuggeeNoExecute nx(cx, *dbg, adjqi);

        RootedValue scriptFrame(cx);
        if (!dbg->getFrame(cx, iter, &scriptFrame)) {
          return dbg->reportUncaughtException(ar);
        }
        RootedValue rv(cx);
        Rooted<JSObject*> handler(cx, bp->handler);
        bool ok = CallMethodIfPresent(cx, handler, "hit", 1,
                                      scriptFrame.address(), &rv);
        ResumeMode resumeMode = dbg->processHandlerResult(
            ar, ok, rv, iter.abstractFramePtr(), iter.pc(), vp);
        adjqi.runJobs();

        if (resumeMode != ResumeMode::Continue) {
          savedExc.drop();
          return resumeMode;
        }

        // Calling JS code invalidates site. Reload it.
        if (isJS) {
          site = DebugScript::getBreakpointSite(iter.script(), pc);
        } else {
          site = iter.wasmInstance()->debug().getBreakpointSite(cx,
                                                                bytecodeOffset);
        }
      }
    }
  }

  // By convention, return the true op to the interpreter in vp, and return
  // undefined in vp to the wasm debug trap.
  if (isJS) {
    vp.setInt32(JSOp(*pc));
  } else {
    vp.set(UndefinedValue());
  }
  return ResumeMode::Continue;
}

/* static */
ResumeMode DebugAPI::onSingleStep(JSContext* cx, MutableHandleValue vp) {
  FrameIter iter(cx);

  // We may be stepping over a JSOP_EXCEPTION, that pushes the context's
  // pending exception for a 'catch' clause to handle. Don't let the onStep
  // handlers mess with that (other than by returning a resumption value).
  JS::AutoSaveExceptionState savedExc(cx);

  // Build list of Debugger.Frame instances referring to this frame with
  // onStep handlers.
  Rooted<Debugger::DebuggerFrameVector> frames(
      cx, Debugger::DebuggerFrameVector(cx));
  if (!Debugger::getDebuggerFrames(iter.abstractFramePtr(), &frames)) {
    return ResumeMode::Terminate;
  }

#ifdef DEBUG
  // Validate the single-step count on this frame's script, to ensure that
  // we're not receiving traps we didn't ask for. Even when frames is
  // non-empty (and thus we know this trap was requested), do the check
  // anyway, to make sure the count has the correct non-zero value.
  //
  // The converse --- ensuring that we do receive traps when we should --- can
  // be done with unit tests.
  if (iter.hasScript()) {
    uint32_t liveStepperCount = 0;
    uint32_t suspendedStepperCount = 0;
    JSScript* trappingScript = iter.script();
    GlobalObject* global = cx->global();
    if (GlobalObject::DebuggerVector* debuggers = global->getDebuggers()) {
      for (auto p = debuggers->begin(); p != debuggers->end(); p++) {
        Debugger* dbg = *p;
        for (Debugger::FrameMap::Range r = dbg->frames.all();
             !r.empty();
             r.popFront()) {
          AbstractFramePtr frame = r.front().key();
          NativeObject* frameobj = r.front().value();
          if (frame.isWasmDebugFrame()) {
            continue;
          }
          if (frame.script() == trappingScript &&
              !frameobj->getReservedSlot(DebuggerFrame::ONSTEP_HANDLER_SLOT)
                   .isUndefined()) {
            liveStepperCount++;
          }
        }

        // Also count hooks set on suspended generator frames.
        for (Debugger::GeneratorWeakMap::Range r = dbg->generatorFrames.all();
             !r.empty();
             r.popFront()) {
          AbstractGeneratorObject& genObj =
              r.front().key()->as<AbstractGeneratorObject>();
          DebuggerFrame& frameObj = r.front().value()->as<DebuggerFrame>();
          MOZ_ASSERT(&frameObj.unwrappedGenerator() == &genObj);

          // Live Debugger.Frames were already counted in dbg->frames loop.
          if (frameObj.isLive()) {
            continue;
          }

          // If a frame isn't live, but it has an entry in generatorFrames,
          // it had better be suspended.
          MOZ_ASSERT(genObj.isSuspended());

          if (!genObj.callee().isInterpretedLazy() &&
              genObj.callee().nonLazyScript() == trappingScript &&
              !frameObj.getReservedSlot(DebuggerFrame::ONSTEP_HANDLER_SLOT)
                   .isUndefined()) {
            suspendedStepperCount++;
          }
        }
      }
    }

    MOZ_ASSERT(liveStepperCount + suspendedStepperCount ==
               DebugScript::getStepperCount(trappingScript));
  }
#endif

  if (frames.length() > 0) {
    // Preserve the debuggee's microtask event queue while we run the hooks, so
    // the debugger's microtask checkpoints don't run from the debuggee's
    // microtasks, and vice versa.
    JS::AutoDebuggerJobQueueInterruption adjqi;
    if (!adjqi.init(cx)) {
      return ResumeMode::Terminate;
    }

    // Call onStep for frames that have the handler set.
    for (size_t i = 0; i < frames.length(); i++) {
      HandleDebuggerFrame frame = frames[i];
      OnStepHandler* handler = frame->onStepHandler();
      if (!handler) {
        continue;
      }

      Debugger* dbg = Debugger::fromChildJSObject(frame);
      EnterDebuggeeNoExecute nx(cx, *dbg, adjqi);

      Maybe<AutoRealm> ar;
      ar.emplace(cx, dbg->object);

      ResumeMode resumeMode = ResumeMode::Continue;
      bool success = handler->onStep(cx, frame, resumeMode, vp);
      resumeMode = dbg->processParsedHandlerResult(
          ar, iter.abstractFramePtr(), iter.pc(), success, resumeMode, vp);
      adjqi.runJobs();

      if (resumeMode != ResumeMode::Continue) {
        savedExc.drop();
        return resumeMode;
      }
    }
  }

  vp.setUndefined();
  return ResumeMode::Continue;
}

ResumeMode Debugger::fireNewGlobalObject(JSContext* cx,
                                         Handle<GlobalObject*> global,
                                         MutableHandleValue vp) {
  RootedObject hook(cx, getHook(OnNewGlobalObject));
  MOZ_ASSERT(hook);
  MOZ_ASSERT(hook->isCallable());

  Maybe<AutoRealm> ar;
  ar.emplace(cx, object);

  RootedValue wrappedGlobal(cx, ObjectValue(*global));
  if (!wrapDebuggeeValue(cx, &wrappedGlobal)) {
    return reportUncaughtException(ar);
  }

  // onNewGlobalObject is infallible, and thus is only allowed to return
  // undefined as a resumption value. If it returns anything else, we throw.
  // And if that happens, or if the hook itself throws, we invoke the
  // uncaughtExceptionHook so that we never leave an exception pending on the
  // cx. This allows JS_NewGlobalObject to avoid handling failures from debugger
  // hooks.
  RootedValue rv(cx);
  RootedValue fval(cx, ObjectValue(*hook));
  bool ok = js::Call(cx, fval, object, wrappedGlobal, &rv);
  if (ok && !rv.isUndefined()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_RESUMPTION_VALUE_DISALLOWED);
    ok = false;
  }
  // NB: Even though we don't care about what goes into it, we have to pass vp
  // to handleUncaughtException so that it parses resumption values from the
  // uncaughtExceptionHook and tells the caller whether we should execute the
  // rest of the onNewGlobalObject hooks or not.
  ResumeMode resumeMode =
      ok ? ResumeMode::Continue : handleUncaughtException(ar, vp);
  MOZ_ASSERT(!cx->isExceptionPending());
  return resumeMode;
}

void DebugAPI::slowPathOnNewGlobalObject(JSContext* cx,
                                         Handle<GlobalObject*> global) {
  MOZ_ASSERT(!cx->runtime()->onNewGlobalObjectWatchers().isEmpty());
  if (global->realm()->creationOptions().invisibleToDebugger()) {
    return;
  }

  // Make a copy of the runtime's onNewGlobalObjectWatchers before running the
  // handlers. Since one Debugger's handler can disable another's, the list
  // can be mutated while we're walking it.
  RootedObjectVector watchers(cx);
  for (auto& dbg : cx->runtime()->onNewGlobalObjectWatchers()) {
    MOZ_ASSERT(dbg.observesNewGlobalObject());
    JSObject* obj = dbg.object;
    JS::ExposeObjectToActiveJS(obj);
    if (!watchers.append(obj)) {
      if (cx->isExceptionPending()) {
        cx->clearPendingException();
      }
      return;
    }
  }

  ResumeMode resumeMode = ResumeMode::Continue;
  RootedValue value(cx);

  // Preserve the debuggee's microtask event queue while we run the hooks, so
  // the debugger's microtask checkpoints don't run from the debuggee's
  // microtasks, and vice versa.
  JS::AutoDebuggerJobQueueInterruption adjqi;
  if (!adjqi.init(cx)) {
    cx->clearPendingException();
    return;
  }

  for (size_t i = 0; i < watchers.length(); i++) {
    Debugger* dbg = Debugger::fromJSObject(watchers[i]);
    EnterDebuggeeNoExecute nx(cx, *dbg, adjqi);

    // We disallow resumption values from onNewGlobalObject hooks, because we
    // want the debugger hooks for global object creation to be infallible.
    // But if an onNewGlobalObject hook throws, and the uncaughtExceptionHook
    // decides to raise an error, we want to at least avoid invoking the rest
    // of the onNewGlobalObject handlers in the list (not for any super
    // compelling reason, just because it seems like the right thing to do).
    // So we ignore whatever comes out in |value|, but break out of the loop
    // if a non-success resume mode is returned.
    if (dbg->observesNewGlobalObject()) {
      resumeMode = dbg->fireNewGlobalObject(cx, global, &value);
      adjqi.runJobs();
      if (resumeMode != ResumeMode::Continue &&
          resumeMode != ResumeMode::Return) {
        break;
      }
    }
  }
  MOZ_ASSERT(!cx->isExceptionPending());
}

/* static */
void DebugAPI::slowPathNotifyParticipatesInGC(
    uint64_t majorGCNumber,
    GlobalObject::DebuggerVector& dbgs) {
  for (GlobalObject::DebuggerVector::Range r = dbgs.all(); !r.empty();
       r.popFront()) {
    if (!r.front().unbarrieredGet()->debuggeeIsBeingCollected(majorGCNumber)) {
#ifdef DEBUG
      fprintf(stderr,
              "OOM while notifying observing Debuggers of a GC: The "
              "onGarbageCollection\n"
              "hook will not be fired for this GC for some Debuggers!\n");
#endif
      return;
    }
  }
}

/* static */
Maybe<double> DebugAPI::allocationSamplingProbability(GlobalObject* global) {
  GlobalObject::DebuggerVector* dbgs = global->getDebuggers();
  if (!dbgs || dbgs->empty()) {
    return Nothing();
  }

  DebugOnly<WeakHeapPtr<Debugger*>*> begin = dbgs->begin();

  double probability = 0;
  bool foundAnyDebuggers = false;
  for (auto p = dbgs->begin(); p < dbgs->end(); p++) {
    // The set of debuggers had better not change while we're iterating,
    // such that the vector gets reallocated.
    MOZ_ASSERT(dbgs->begin() == begin);
    // Use unbarrieredGet() to prevent triggering read barrier while collecting,
    // this is safe as long as dbgp does not escape.
    Debugger* dbgp = p->unbarrieredGet();

    if (dbgp->trackingAllocationSites && dbgp->enabled) {
      foundAnyDebuggers = true;
      probability = std::max(dbgp->allocationSamplingProbability, probability);
    }
  }

  return foundAnyDebuggers ? Some(probability) : Nothing();
}

/* static */
bool DebugAPI::slowPathOnLogAllocationSite(JSContext* cx, HandleObject obj,
                                           HandleSavedFrame frame,
                                           mozilla::TimeStamp when,
                                           GlobalObject::DebuggerVector& dbgs) {
  MOZ_ASSERT(!dbgs.empty());
  mozilla::DebugOnly<WeakHeapPtr<Debugger*>*> begin = dbgs.begin();

  // Root all the Debuggers while we're iterating over them;
  // appendAllocationSite calls Compartment::wrap, and thus can GC.
  //
  // SpiderMonkey protocol is generally for the caller to prove that it has
  // rooted the stuff it's asking you to operate on (i.e. by passing a
  // Handle), but in this case, we're iterating over a global's list of
  // Debuggers, and globals only hold their Debuggers weakly.
  Rooted<GCVector<JSObject*>> activeDebuggers(cx, GCVector<JSObject*>(cx));
  for (auto dbgp = dbgs.begin(); dbgp < dbgs.end(); dbgp++) {
    if (!activeDebuggers.append((*dbgp)->object)) {
      return false;
    }
  }

  for (auto dbgp = dbgs.begin(); dbgp < dbgs.end(); dbgp++) {
    // The set of debuggers had better not change while we're iterating,
    // such that the vector gets reallocated.
    MOZ_ASSERT(dbgs.begin() == begin);

    if ((*dbgp)->trackingAllocationSites && (*dbgp)->enabled &&
        !(*dbgp)->appendAllocationSite(cx, obj, frame, when)) {
      return false;
    }
  }

  return true;
}

bool Debugger::isDebuggeeUnbarriered(const Realm* realm) const {
  MOZ_ASSERT(realm);
  return realm->isDebuggee() &&
         debuggees.has(realm->unsafeUnbarrieredMaybeGlobal());
}

bool Debugger::appendAllocationSite(JSContext* cx, HandleObject obj,
                                    HandleSavedFrame frame,
                                    mozilla::TimeStamp when) {
  MOZ_ASSERT(trackingAllocationSites && enabled);

  AutoRealm ar(cx, object);
  RootedObject wrappedFrame(cx, frame);
  if (!cx->compartment()->wrap(cx, &wrappedFrame)) {
    return false;
  }

  // Try to get the constructor name from the ObjectGroup's TypeNewScript.
  // This is only relevant for native objects.
  RootedAtom ctorName(cx);
  if (obj->is<NativeObject>()) {
    AutoRealm ar(cx, obj);
    if (!JSObject::constructorDisplayAtom(cx, obj, &ctorName)) {
      return false;
    }
  }
  if (ctorName) {
    cx->markAtom(ctorName);
  }

  auto className = obj->getClass()->name;
  auto size =
      JS::ubi::Node(obj.get()).size(cx->runtime()->debuggerMallocSizeOf);
  auto inNursery = gc::IsInsideNursery(obj);

  if (!allocationsLog.emplaceBack(wrappedFrame, when, className, ctorName, size,
                                  inNursery)) {
    ReportOutOfMemory(cx);
    return false;
  }

  if (allocationsLog.length() > maxAllocationsLogLength) {
    allocationsLog.popFront();
    MOZ_ASSERT(allocationsLog.length() == maxAllocationsLogLength);
    allocationsLogOverflowed = true;
  }

  return true;
}

ResumeMode Debugger::firePromiseHook(JSContext* cx, Hook hook,
                                     HandleObject promise,
                                     MutableHandleValue vp) {
  MOZ_ASSERT(hook == OnNewPromise || hook == OnPromiseSettled);

  RootedObject hookObj(cx, getHook(hook));
  MOZ_ASSERT(hookObj);
  MOZ_ASSERT(hookObj->isCallable());

  Maybe<AutoRealm> ar;
  ar.emplace(cx, object);

  RootedValue dbgObj(cx, ObjectValue(*promise));
  if (!wrapDebuggeeValue(cx, &dbgObj)) {
    return reportUncaughtException(ar);
  }

  // Like onNewGlobalObject, the Promise hooks are infallible and the comments
  // in |Debugger::fireNewGlobalObject| apply here as well.
  RootedValue fval(cx, ObjectValue(*hookObj));
  RootedValue rv(cx);
  bool ok = js::Call(cx, fval, object, dbgObj, &rv);
  if (ok && !rv.isUndefined()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_RESUMPTION_VALUE_DISALLOWED);
    ok = false;
  }

  ResumeMode resumeMode =
      ok ? ResumeMode::Continue : handleUncaughtException(ar, vp);
  MOZ_ASSERT(!cx->isExceptionPending());
  return resumeMode;
}

/* static */
void Debugger::slowPathPromiseHook(JSContext* cx, Hook hook,
                                   Handle<PromiseObject*> promise) {
  MOZ_ASSERT(hook == OnNewPromise || hook == OnPromiseSettled);

  if (hook == OnPromiseSettled) {
    // We should be in the right compartment, but for simplicity always enter
    // the promise's realm below.
    cx->check(promise);
  }

  AutoRealm ar(cx, promise);

  RootedValue rval(cx);
  ResumeMode resumeMode = Debugger::dispatchHook(
      cx, [hook](Debugger* dbg) -> bool { return dbg->getHook(hook); },
      [&](Debugger* dbg) -> ResumeMode {
        (void)dbg->firePromiseHook(cx, hook, promise, &rval);
        return ResumeMode::Continue;
      });

  if (resumeMode == ResumeMode::Terminate) {
    // The dispatch hook function might fail to append into the list of
    // Debuggers which are watching for the hook.
    cx->clearPendingException();
    return;
  }

  // Promise hooks are infallible and we ignore errors from uncaught
  // exceptions by design.
  MOZ_ASSERT(resumeMode == ResumeMode::Continue);
}

/* static */
void DebugAPI::slowPathOnNewPromise(JSContext* cx,
                                    Handle<PromiseObject*> promise) {
  Debugger::slowPathPromiseHook(cx, Debugger::OnNewPromise, promise);
}

/* static */
void DebugAPI::slowPathOnPromiseSettled(JSContext* cx,
                                        Handle<PromiseObject*> promise) {
  Debugger::slowPathPromiseHook(cx, Debugger::OnPromiseSettled, promise);
}

/*** Debugger code invalidation for observing execution *********************/

class MOZ_RAII ExecutionObservableRealms
    : public DebugAPI::ExecutionObservableSet {
  HashSet<Realm*> realms_;
  HashSet<Zone*> zones_;

 public:
  explicit ExecutionObservableRealms(
      JSContext* cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : realms_(cx), zones_(cx) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  bool add(Realm* realm) {
    return realms_.put(realm) && zones_.put(realm->zone());
  }

  using RealmRange = HashSet<Realm*>::Range;
  const HashSet<Realm*>* realms() const { return &realms_; }

  const HashSet<Zone*>* zones() const override { return &zones_; }
  bool shouldRecompileOrInvalidate(JSScript* script) const override {
    return script->hasBaselineScript() && realms_.has(script->realm());
  }
  bool shouldMarkAsDebuggee(FrameIter& iter) const override {
    // AbstractFramePtr can't refer to non-remateralized Ion frames or
    // non-debuggee wasm frames, so if iter refers to one such, we know we
    // don't match.
    return iter.hasUsableAbstractFramePtr() && realms_.has(iter.realm());
  }

  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

// Given a particular AbstractFramePtr F that has become observable, this
// represents the stack frames that need to be bailed out or marked as
// debuggees, and the scripts that need to be recompiled, taking inlining into
// account.
class MOZ_RAII ExecutionObservableFrame
    : public DebugAPI::ExecutionObservableSet {
  AbstractFramePtr frame_;

 public:
  explicit ExecutionObservableFrame(
      AbstractFramePtr frame MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : frame_(frame) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  Zone* singleZone() const override {
    // We never inline across realms, let alone across zones, so
    // frames_'s script's zone is the only one of interest.
    return frame_.script()->zone();
  }

  JSScript* singleScriptForZoneInvalidation() const override {
    MOZ_CRASH(
        "ExecutionObservableFrame shouldn't need zone-wide invalidation.");
    return nullptr;
  }

  bool shouldRecompileOrInvalidate(JSScript* script) const override {
    // Normally, *this represents exactly one script: the one frame_ is
    // running.
    //
    // However, debug-mode OSR uses *this for both invalidating Ion frames,
    // and recompiling the Baseline scripts that those Ion frames will bail
    // out into. Suppose frame_ is an inline frame, executing a copy of its
    // JSScript, S_inner, that has been inlined into the IonScript of some
    // other JSScript, S_outer. We must match S_outer, to decide which Ion
    // frame to invalidate; and we must match S_inner, to decide which
    // Baseline script to recompile.
    //
    // Note that this does not, by design, invalidate *all* inliners of
    // frame_.script(), as only frame_ is made observable, not
    // frame_.script().
    if (!script->hasBaselineScript()) {
      return false;
    }

    if (frame_.hasScript() && script == frame_.script()) {
      return true;
    }

    return frame_.isRematerializedFrame() &&
           script == frame_.asRematerializedFrame()->outerScript();
  }

  bool shouldMarkAsDebuggee(FrameIter& iter) const override {
    // AbstractFramePtr can't refer to non-remateralized Ion frames or
    // non-debuggee wasm frames, so if iter refers to one such, we know we
    // don't match.
    //
    // We never use this 'has' overload for frame invalidation, only for
    // frame debuggee marking; so this overload doesn't need a parallel to
    // the just-so inlining logic above.
    return iter.hasUsableAbstractFramePtr() &&
           iter.abstractFramePtr() == frame_;
  }

  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_RAII ExecutionObservableScript
    : public DebugAPI::ExecutionObservableSet {
  RootedScript script_;

 public:
  ExecutionObservableScript(JSContext* cx,
                            JSScript* script MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : script_(cx, script) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  Zone* singleZone() const override { return script_->zone(); }
  JSScript* singleScriptForZoneInvalidation() const override { return script_; }
  bool shouldRecompileOrInvalidate(JSScript* script) const override {
    return script->hasBaselineScript() && script == script_;
  }
  bool shouldMarkAsDebuggee(FrameIter& iter) const override {
    // AbstractFramePtr can't refer to non-remateralized Ion frames, and
    // while a non-rematerialized Ion frame may indeed be running script_,
    // we cannot mark them as debuggees until they bail out.
    //
    // Upon bailing out, any newly constructed Baseline frames that came
    // from Ion frames with scripts that are isDebuggee() is marked as
    // debuggee. This is correct in that the only other way a frame may be
    // marked as debuggee is via Debugger.Frame reflection, which would
    // have rematerialized any Ion frames.
    //
    // Also AbstractFramePtr can't refer to non-debuggee wasm frames, so if
    // iter refers to one such, we know we don't match.
    return iter.hasUsableAbstractFramePtr() && !iter.isWasm() &&
           iter.abstractFramePtr().script() == script_;
  }

  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/* static */
bool Debugger::updateExecutionObservabilityOfFrames(
    JSContext* cx, const DebugAPI::ExecutionObservableSet& obs,
    IsObserving observing) {
  AutoSuppressProfilerSampling suppressProfilerSampling(cx);

  {
    jit::JitContext jctx(cx, nullptr);
    if (!jit::RecompileOnStackBaselineScriptsForDebugMode(cx, obs, observing)) {
      ReportOutOfMemory(cx);
      return false;
    }
  }

  AbstractFramePtr oldestEnabledFrame;
  for (FrameIter iter(cx); !iter.done(); ++iter) {
    if (obs.shouldMarkAsDebuggee(iter)) {
      if (observing) {
        if (!iter.abstractFramePtr().isDebuggee()) {
          oldestEnabledFrame = iter.abstractFramePtr();
          oldestEnabledFrame.setIsDebuggee();
        }
        if (iter.abstractFramePtr().isWasmDebugFrame()) {
          iter.abstractFramePtr().asWasmDebugFrame()->observe(cx);
        }
      } else {
#ifdef DEBUG
        // Debugger.Frame lifetimes are managed by the debug epilogue,
        // so in general it's unsafe to unmark a frame if it has a
        // Debugger.Frame associated with it.
        MOZ_ASSERT(!DebugAPI::inFrameMaps(iter.abstractFramePtr()));
#endif
        iter.abstractFramePtr().unsetIsDebuggee();
      }
    }
  }

  // See comment in unsetPrevUpToDateUntil.
  if (oldestEnabledFrame) {
    AutoRealm ar(cx, oldestEnabledFrame.environmentChain());
    DebugEnvironments::unsetPrevUpToDateUntil(cx, oldestEnabledFrame);
  }

  return true;
}

static inline void MarkJitScriptActiveIfObservable(
    JSScript* script, const DebugAPI::ExecutionObservableSet& obs) {
  if (obs.shouldRecompileOrInvalidate(script)) {
    script->jitScript()->setActive();
  }
}

static bool AppendAndInvalidateScript(JSContext* cx, Zone* zone,
                                      JSScript* script,
                                      Vector<JSScript*>& scripts) {
  // Enter the script's realm as addPendingRecompile attempts to
  // cancel off-thread compilations, whose books are kept on the
  // script's realm.
  MOZ_ASSERT(script->zone() == zone);
  AutoRealm ar(cx, script);
  zone->types.addPendingRecompile(cx, script);
  return scripts.append(script);
}

static bool UpdateExecutionObservabilityOfScriptsInZone(
    JSContext* cx, Zone* zone, const DebugAPI::ExecutionObservableSet& obs,
    Debugger::IsObserving observing) {
  using namespace js::jit;

  AutoSuppressProfilerSampling suppressProfilerSampling(cx);

  FreeOp* fop = cx->runtime()->defaultFreeOp();

  Vector<JSScript*> scripts(cx);

  // Iterate through observable scripts, invalidating their Ion scripts and
  // appending them to a vector for discarding their baseline scripts later.
  {
    AutoEnterAnalysis enter(fop, zone);
    if (JSScript* script = obs.singleScriptForZoneInvalidation()) {
      if (obs.shouldRecompileOrInvalidate(script)) {
        if (!AppendAndInvalidateScript(cx, zone, script, scripts)) {
          return false;
        }
      }
    } else {
      for (auto script = zone->cellIter<JSScript>(); !script.done();
           script.next()) {
        if (obs.shouldRecompileOrInvalidate(script)) {
          if (!AppendAndInvalidateScript(cx, zone, script, scripts)) {
            return false;
          }
        }
      }
    }
  }

  // Code below this point must be infallible to ensure the active bit of
  // BaselineScripts is in a consistent state.
  //
  // Mark active baseline scripts in the observable set so that they don't
  // get discarded. They will be recompiled.
  for (JitActivationIterator actIter(cx); !actIter.done(); ++actIter) {
    if (actIter->compartment()->zone() != zone) {
      continue;
    }

    for (OnlyJSJitFrameIter iter(actIter); !iter.done(); ++iter) {
      const JSJitFrameIter& frame = iter.frame();
      switch (frame.type()) {
        case FrameType::BaselineJS:
          MarkJitScriptActiveIfObservable(frame.script(), obs);
          break;
        case FrameType::IonJS:
          MarkJitScriptActiveIfObservable(frame.script(), obs);
          for (InlineFrameIterator inlineIter(cx, &frame); inlineIter.more();
               ++inlineIter) {
            MarkJitScriptActiveIfObservable(inlineIter.script(), obs);
          }
          break;
        default:;
      }
    }
  }

  // Iterate through the scripts again and finish discarding
  // BaselineScripts. This must be done as a separate phase as we can only
  // discard the BaselineScript on scripts that have no IonScript.
  for (size_t i = 0; i < scripts.length(); i++) {
    MOZ_ASSERT_IF(scripts[i]->isDebuggee(), observing);
    if (!scripts[i]->jitScript()->active()) {
      FinishDiscardBaselineScript(fop, scripts[i]);
    }
    scripts[i]->jitScript()->resetActive();
  }

  // Iterate through all wasm instances to find ones that need to be updated.
  for (RealmsInZoneIter r(zone); !r.done(); r.next()) {
    for (wasm::Instance* instance : r->wasm.instances()) {
      if (!instance->debugEnabled()) {
        continue;
      }

      bool enableTrap = observing == Debugger::Observing;
      instance->debug().ensureEnterFrameTrapsState(cx, enableTrap);
    }
  }

  return true;
}

/* static */
bool Debugger::updateExecutionObservabilityOfScripts(
    JSContext* cx, const DebugAPI::ExecutionObservableSet& obs, IsObserving observing) {
  if (Zone* zone = obs.singleZone()) {
    return UpdateExecutionObservabilityOfScriptsInZone(cx, zone, obs,
                                                       observing);
  }

  typedef DebugAPI::ExecutionObservableSet::ZoneRange ZoneRange;
  for (ZoneRange r = obs.zones()->all(); !r.empty(); r.popFront()) {
    if (!UpdateExecutionObservabilityOfScriptsInZone(cx, r.front(), obs,
                                                     observing)) {
      return false;
    }
  }

  return true;
}

template <typename FrameFn>
/* static */
void Debugger::forEachDebuggerFrame(AbstractFramePtr frame, FrameFn fn) {
  GlobalObject* global = frame.global();
  if (GlobalObject::DebuggerVector* debuggers = global->getDebuggers()) {
    for (auto p = debuggers->begin(); p != debuggers->end(); p++) {
      Debugger* dbg = *p;
      if (FrameMap::Ptr entry = dbg->frames.lookup(frame)) {
        fn(entry->value());
      }
    }
  }
}

/* static */
bool Debugger::getDebuggerFrames(AbstractFramePtr frame,
                                 MutableHandle<DebuggerFrameVector> frames) {
  bool hadOOM = false;
  forEachDebuggerFrame(frame, [&](DebuggerFrame* frameobj) {
    if (!hadOOM && !frames.append(frameobj)) {
      hadOOM = true;
    }
  });
  return !hadOOM;
}

/* static */
bool Debugger::updateExecutionObservability(
    JSContext* cx, DebugAPI::ExecutionObservableSet& obs,
    IsObserving observing) {
  if (!obs.singleZone() && obs.zones()->empty()) {
    return true;
  }

  // Invalidate scripts first so we can set the needsArgsObj flag on scripts
  // before patching frames.
  return updateExecutionObservabilityOfScripts(cx, obs, observing) &&
         updateExecutionObservabilityOfFrames(cx, obs, observing);
}

/* static */
bool Debugger::ensureExecutionObservabilityOfScript(JSContext* cx,
                                                    JSScript* script) {
  if (script->isDebuggee()) {
    return true;
  }
  ExecutionObservableScript obs(cx, script);
  return updateExecutionObservability(cx, obs, Observing);
}

/* static */
bool DebugAPI::ensureExecutionObservabilityOfOsrFrame(
    JSContext* cx, AbstractFramePtr osrSourceFrame) {
  MOZ_ASSERT(osrSourceFrame.isDebuggee());
  if (osrSourceFrame.script()->hasBaselineScript() &&
      osrSourceFrame.script()->baselineScript()->hasDebugInstrumentation()) {
    return true;
  }
  ExecutionObservableFrame obs(osrSourceFrame);
  return Debugger::updateExecutionObservabilityOfFrames(cx, obs, Observing);
}

/* static */
bool Debugger::ensureExecutionObservabilityOfFrame(JSContext* cx,
                                                   AbstractFramePtr frame) {
  MOZ_ASSERT_IF(frame.hasScript() && frame.script()->isDebuggee(),
                frame.isDebuggee());
  MOZ_ASSERT_IF(frame.isWasmDebugFrame(), frame.wasmInstance()->debugEnabled());
  if (frame.isDebuggee()) {
    return true;
  }
  ExecutionObservableFrame obs(frame);
  return updateExecutionObservabilityOfFrames(cx, obs, Observing);
}

/* static */
bool Debugger::ensureExecutionObservabilityOfRealm(JSContext* cx,
                                                   Realm* realm) {
  if (realm->debuggerObservesAllExecution()) {
    return true;
  }
  ExecutionObservableRealms obs(cx);
  if (!obs.add(realm)) {
    return false;
  }
  realm->updateDebuggerObservesAllExecution();
  return updateExecutionObservability(cx, obs, Observing);
}

/* static */
bool Debugger::hookObservesAllExecution(Hook which) {
  return which == OnEnterFrame;
}

Debugger::IsObserving Debugger::observesAllExecution() const {
  if (enabled && !!getHook(OnEnterFrame)) {
    return Observing;
  }
  return NotObserving;
}

Debugger::IsObserving Debugger::observesAsmJS() const {
  if (enabled && !allowUnobservedAsmJS) {
    return Observing;
  }
  return NotObserving;
}

Debugger::IsObserving Debugger::observesCoverage() const {
  if (enabled && collectCoverageInfo) {
    return Observing;
  }
  return NotObserving;
}

// Toggle whether this Debugger's debuggees observe all execution. This is
// called when a hook that observes all execution is set or unset. See
// hookObservesAllExecution.
bool Debugger::updateObservesAllExecutionOnDebuggees(JSContext* cx,
                                                     IsObserving observing) {
  ExecutionObservableRealms obs(cx);

  for (WeakGlobalObjectSet::Range r = debuggees.all(); !r.empty();
       r.popFront()) {
    GlobalObject* global = r.front();
    JS::Realm* realm = global->realm();

    if (realm->debuggerObservesAllExecution() == observing) {
      continue;
    }

    // It's expensive to eagerly invalidate and recompile a realm,
    // so add the realm to the set only if we are observing.
    if (observing && !obs.add(realm)) {
      return false;
    }
  }

  if (!updateExecutionObservability(cx, obs, observing)) {
    return false;
  }

  using RealmRange = ExecutionObservableRealms::RealmRange;
  for (RealmRange r = obs.realms()->all(); !r.empty(); r.popFront()) {
    r.front()->updateDebuggerObservesAllExecution();
  }

  return true;
}

bool Debugger::updateObservesCoverageOnDebuggees(JSContext* cx,
                                                 IsObserving observing) {
  ExecutionObservableRealms obs(cx);

  for (WeakGlobalObjectSet::Range r = debuggees.all(); !r.empty();
       r.popFront()) {
    GlobalObject* global = r.front();
    Realm* realm = global->realm();

    if (realm->debuggerObservesCoverage() == observing) {
      continue;
    }

    // Invalidate and recompile a realm to add or remove PCCounts
    // increments. We have to eagerly invalidate, as otherwise we might have
    // dangling pointers to freed PCCounts.
    if (!obs.add(realm)) {
      return false;
    }
  }

  // If any frame on the stack belongs to the debuggee, then we cannot update
  // the ScriptCounts, because this would imply to invalidate a Debugger.Frame
  // to recompile it with/without ScriptCount support.
  for (FrameIter iter(cx); !iter.done(); ++iter) {
    if (obs.shouldMarkAsDebuggee(iter)) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_DEBUG_NOT_IDLE);
      return false;
    }
  }

  if (!updateExecutionObservability(cx, obs, observing)) {
    return false;
  }

  // All realms can safely be toggled, and all scripts will be recompiled.
  // Thus we can update each realm accordingly.
  using RealmRange = ExecutionObservableRealms::RealmRange;
  for (RealmRange r = obs.realms()->all(); !r.empty(); r.popFront()) {
    r.front()->updateDebuggerObservesCoverage();
  }

  return true;
}

void Debugger::updateObservesAsmJSOnDebuggees(IsObserving observing) {
  for (WeakGlobalObjectSet::Range r = debuggees.all(); !r.empty();
       r.popFront()) {
    GlobalObject* global = r.front();
    Realm* realm = global->realm();

    if (realm->debuggerObservesAsmJS() == observing) {
      continue;
    }

    realm->updateDebuggerObservesAsmJS();
  }
}

/*** Allocations Tracking ***************************************************/

/* static */
bool Debugger::cannotTrackAllocations(const GlobalObject& global) {
  auto existingCallback = global.realm()->getAllocationMetadataBuilder();
  return existingCallback && existingCallback != &SavedStacks::metadataBuilder;
}

/* static */
bool DebugAPI::isObservedByDebuggerTrackingAllocations(
    const GlobalObject& debuggee) {
  if (auto* v = debuggee.getDebuggers()) {
    for (auto p = v->begin(); p != v->end(); p++) {
      // Use unbarrieredGet() to prevent triggering read barrier while
      // collecting, this is safe as long as dbg does not escape.
      Debugger* dbg = p->unbarrieredGet();
      if (dbg->trackingAllocationSites && dbg->enabled) {
        return true;
      }
    }
  }

  return false;
}

/* static */
bool Debugger::addAllocationsTracking(JSContext* cx,
                                      Handle<GlobalObject*> debuggee) {
  // Precondition: the given global object is being observed by at least one
  // Debugger that is tracking allocations.
  MOZ_ASSERT(DebugAPI::isObservedByDebuggerTrackingAllocations(*debuggee));

  if (Debugger::cannotTrackAllocations(*debuggee)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_OBJECT_METADATA_CALLBACK_ALREADY_SET);
    return false;
  }

  debuggee->realm()->setAllocationMetadataBuilder(
      &SavedStacks::metadataBuilder);
  debuggee->realm()->chooseAllocationSamplingProbability();
  return true;
}

/* static */
void Debugger::removeAllocationsTracking(GlobalObject& global) {
  // If there are still Debuggers that are observing allocations, we cannot
  // remove the metadata callback yet. Recompute the sampling probability
  // based on the remaining debuggers' needs.
  if (DebugAPI::isObservedByDebuggerTrackingAllocations(global)) {
    global.realm()->chooseAllocationSamplingProbability();
    return;
  }

  if (!global.realm()->runtimeFromMainThread()->recordAllocationCallback) {
    // Something like the Gecko Profiler could request from the the JS runtime
    // to record allocations. If it is recording allocations, then do not
    // destroy the allocation metadata builder at this time.
    global.realm()->forgetAllocationMetadataBuilder();
  }
}

bool Debugger::addAllocationsTrackingForAllDebuggees(JSContext* cx) {
  MOZ_ASSERT(trackingAllocationSites);

  // We don't want to end up in a state where we added allocations
  // tracking to some of our debuggees, but failed to do so for
  // others. Before attempting to start tracking allocations in *any* of
  // our debuggees, ensure that we will be able to track allocations for
  // *all* of our debuggees.
  for (WeakGlobalObjectSet::Range r = debuggees.all(); !r.empty();
       r.popFront()) {
    if (Debugger::cannotTrackAllocations(*r.front().get())) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_OBJECT_METADATA_CALLBACK_ALREADY_SET);
      return false;
    }
  }

  Rooted<GlobalObject*> g(cx);
  for (WeakGlobalObjectSet::Range r = debuggees.all(); !r.empty();
       r.popFront()) {
    // This should always succeed, since we already checked for the
    // error case above.
    g = r.front().get();
    MOZ_ALWAYS_TRUE(Debugger::addAllocationsTracking(cx, g));
  }

  return true;
}

void Debugger::removeAllocationsTrackingForAllDebuggees() {
  for (WeakGlobalObjectSet::Range r = debuggees.all(); !r.empty();
       r.popFront()) {
    Debugger::removeAllocationsTracking(*r.front().get());
  }

  allocationsLog.clear();
}

/*** Debugger JSObjects *****************************************************/

void Debugger::traceCrossCompartmentEdges(JSTracer* trc) {
  generatorFrames.traceCrossCompartmentEdges<DebuggerFrame::trace>(trc);
  objects.traceCrossCompartmentEdges<DebuggerObject::trace>(trc);
  environments.traceCrossCompartmentEdges<DebuggerEnvironment::trace>(trc);
  scripts.traceCrossCompartmentEdges<DebuggerScript::trace>(trc);
  lazyScripts.traceCrossCompartmentEdges<DebuggerScript::trace>(trc);
  sources.traceCrossCompartmentEdges<DebuggerSource::trace>(trc);
  wasmInstanceScripts.traceCrossCompartmentEdges<DebuggerScript::trace>(trc);
  wasmInstanceSources.traceCrossCompartmentEdges<DebuggerSource::trace>(trc);
}

/*
 * Ordinarily, WeakMap keys and values are marked because at some point it was
 * discovered that the WeakMap was live; that is, some object containing the
 * WeakMap was marked during mark phase.
 *
 * However, during zone GC, we have to do something about cross-compartment
 * edges in non-GC'd compartments. Since the source may be live, we
 * conservatively assume it is and mark the edge.
 *
 * Each Debugger object keeps five cross-compartment WeakMaps: objects, scripts,
 * lazy scripts, script source objects, and environments. They have the property
 * that all their values are in the same compartment as the Debugger object,
 * but we have to mark the keys and the private pointer in the wrapper object.
 *
 * We must scan all Debugger objects regardless of whether they *currently* have
 * any debuggees in a compartment being GC'd, because the WeakMap entries
 * persist even when debuggees are removed.
 *
 * This happens during the initial mark phase, not iterative marking, because
 * all the edges being reported here are strong references.
 *
 * This method is also used during compacting GC to update cross compartment
 * pointers into zones that are being compacted.
 */
/* static */
void DebugAPI::traceCrossCompartmentEdges(JSTracer* trc) {
  JSRuntime* rt = trc->runtime();
  gc::State state = rt->gc.state();
  MOZ_ASSERT(state == gc::State::MarkRoots || state == gc::State::Compact);

  for (Debugger* dbg : rt->debuggerList()) {
    Zone* zone = MaybeForwarded(dbg->object.get())->zone();
    if (!zone->isCollecting() || state == gc::State::Compact) {
      dbg->traceCrossCompartmentEdges(trc);
    }
  }
}

#ifdef DEBUG

static bool RuntimeHasDebugger(JSRuntime* rt, Debugger* dbg) {
  for (Debugger* d : rt->debuggerList()) {
    if (d == dbg) {
      return true;
    }
  }
  return false;
}

/* static */
bool DebugAPI::edgeIsInDebuggerWeakmap(JSRuntime* rt, JSObject* src,
                                       JS::GCCellPtr dst) {
  if (!Debugger::isChildJSObject(src)) {
    return false;
  }

  Debugger* dbg = Debugger::fromChildJSObject(src);
  MOZ_ASSERT(RuntimeHasDebugger(rt, dbg));

  if (src->is<DebuggerFrame>()) {
    if (dst.is<JSScript>()) {
      // The generatorFrames map is not keyed on the associated JSScript. Get
      // the key from the source object and check everything matches.
      DebuggerFrame* frame = &src->as<DebuggerFrame>();
      AbstractGeneratorObject* genObj = &frame->unwrappedGenerator();
      return frame->generatorScript() == &dst.as<JSScript>() &&
             dbg->generatorFrames.hasEntry(genObj, src);
    }
    return dst.is<AbstractGeneratorObject>() &&
           dbg->generatorFrames.hasEntry(&dst.as<AbstractGeneratorObject>(),
                                         src);
  }
  if (src->is<DebuggerObject>()) {
    return dst.is<JSObject>() &&
           dbg->objects.hasEntry(&dst.as<JSObject>(), src);
  }
  if (src->is<DebuggerEnvironment>()) {
    return dst.is<JSObject>() &&
           dbg->environments.hasEntry(&dst.as<JSObject>(), src);
  }
  if (src->is<DebuggerScript>()) {
    return src->as<DebuggerScript>().getReferent().match(
        [=](JSScript* script) {
          return dst.is<JSScript>() && script == &dst.as<JSScript>() &&
                 dbg->scripts.hasEntry(script, src);
        },
        [=](LazyScript* lazy) {
          return dst.is<LazyScript>() && lazy == &dst.as<LazyScript>() &&
                 dbg->lazyScripts.hasEntry(lazy, src);
        },
        [=](WasmInstanceObject* instance) {
          return dst.is<JSObject>() && instance == &dst.as<JSObject>() &&
                 dbg->wasmInstanceScripts.hasEntry(instance, src);
        });
  }
  if (src->is<DebuggerSource>()) {
    return src->as<DebuggerSource>().getReferent().match(
        [=](ScriptSourceObject* sso) {
          return dst.is<JSObject>() && sso == &dst.as<JSObject>() &&
                 dbg->sources.hasEntry(sso, src);
        },
        [=](WasmInstanceObject* instance) {
          return dst.is<JSObject>() && instance == &dst.as<JSObject>() &&
                 dbg->wasmInstanceSources.hasEntry(instance, src);
        });
  }
  MOZ_ASSERT_UNREACHABLE("Unhandled cross-compartment edge");
}

#endif

/*
 * This method has two tasks:
 *   1. Mark Debugger objects that are unreachable except for debugger hooks
 *      that may yet be called.
 *   2. Mark breakpoint handlers.
 *
 * This happens during the iterative part of the GC mark phase. This method
 * returns true if it has to mark anything; GC calls it repeatedly until it
 * returns false.
 */
/* static */
bool DebugAPI::markIteratively(GCMarker* marker) {
  MOZ_ASSERT(JS::RuntimeHeapIsCollecting(),
             "This method should be called during GC.");
  bool markedAny = false;

  // Find all Debugger objects in danger of GC. This code is a little
  // convoluted since the easiest way to find them is via their debuggees.
  JSRuntime* rt = marker->runtime();
  for (RealmsIter r(rt); !r.done(); r.next()) {
    if (r->isDebuggee()) {
      GlobalObject* global = r->unsafeUnbarrieredMaybeGlobal();
      if (!IsMarkedUnbarriered(rt, &global)) {
        continue;
      }

      // Every debuggee has at least one debugger, so in this case
      // getDebuggers can't return nullptr.
      const GlobalObject::DebuggerVector* debuggers = global->getDebuggers();
      MOZ_ASSERT(debuggers);
      for (auto p = debuggers->begin(); p != debuggers->end(); p++) {
        Debugger* dbg = p->unbarrieredGet();

        // dbg is a Debugger with at least one debuggee. Check three things:
        //   - dbg is actually in a compartment that is being marked
        //   - it isn't already marked
        //   - it actually has hooks that might be called
        GCPtrNativeObject& dbgobj = dbg->toJSObjectRef();
        if (!dbgobj->zone()->isGCMarking()) {
          continue;
        }

        bool dbgMarked = IsMarked(rt, &dbgobj);
        if (!dbgMarked && dbg->hasAnyLiveHooks(rt)) {
          // obj could be reachable only via its live, enabled
          // debugger hooks, which may yet be called.
          TraceEdge(marker, &dbgobj, "enabled Debugger");
          markedAny = true;
          dbgMarked = true;
        }

        if (dbgMarked) {
          // Search for breakpoints to mark.
          for (Breakpoint* bp = dbg->firstBreakpoint(); bp;
               bp = bp->nextInDebugger()) {
            switch (bp->site->type()) {
              case BreakpointSite::Type::JS:
                if (IsMarkedUnbarriered(rt, &bp->site->asJS()->script)) {
                  // The debugger and the script are both live.
                  // Therefore the breakpoint handler is live.
                  if (!IsMarked(rt, &bp->getHandlerRef())) {
                    TraceEdge(marker, &bp->getHandlerRef(),
                              "breakpoint handler");
                    markedAny = true;
                  }
                }
                break;
              case BreakpointSite::Type::Wasm:
                if (IsMarkedUnbarriered(rt, &bp->asWasm()->wasmInstance)) {
                  // The debugger and the wasm instance are both live.
                  // Therefore the breakpoint handler is live.
                  if (!IsMarked(rt, &bp->getHandlerRef())) {
                    TraceEdge(marker, &bp->getHandlerRef(),
                              "wasm breakpoint handler");
                    markedAny = true;
                  }
                }
                break;
            }
          }
        }
      }
    }
  }
  return markedAny;
}

/* static */
void DebugAPI::traceAllForMovingGC(JSTracer* trc) {
  JSRuntime* rt = trc->runtime();
  for (Debugger* dbg : rt->debuggerList()) {
    dbg->traceForMovingGC(trc);
  }
}

/*
 * Trace all debugger-owned GC things unconditionally. This is used during
 * compacting GC and in minor GC: the minor GC cannot apply the weak constraints
 * of the full GC because it visits only part of the heap.
 */
void Debugger::traceForMovingGC(JSTracer* trc) {
  trace(trc);

  for (WeakGlobalObjectSet::Enum e(debuggees); !e.empty(); e.popFront()) {
    TraceManuallyBarrieredEdge(trc, e.mutableFront().unsafeGet(),
                               "Global Object");
  }

  for (Breakpoint* bp = firstBreakpoint(); bp; bp = bp->nextInDebugger()) {
    switch (bp->site->type()) {
      case BreakpointSite::Type::JS:
        TraceManuallyBarrieredEdge(trc, &bp->site->asJS()->script,
                                   "breakpoint script");
        break;
      case BreakpointSite::Type::Wasm:
        TraceManuallyBarrieredEdge(trc, &bp->asWasm()->wasmInstance,
                                   "breakpoint wasm instance");
        break;
    }
    TraceEdge(trc, &bp->getHandlerRef(), "breakpoint handler");
  }
}

/* static */
void Debugger::traceObject(JSTracer* trc, JSObject* obj) {
  if (Debugger* dbg = Debugger::fromJSObject(obj)) {
    dbg->trace(trc);
  }
}

void Debugger::trace(JSTracer* trc) {
  TraceEdge(trc, &object, "Debugger Object");

  TraceNullableEdge(trc, &uncaughtExceptionHook, "hooks");

  // Mark Debugger.Frame objects. These are all reachable from JS, because the
  // corresponding JS frames are still on the stack.
  //
  // (We have weakly-referenced Debugger.Frame objects as well, for suspended
  // generator frames; these are traced via generatorFrames just below.)
  for (FrameMap::Range r = frames.all(); !r.empty(); r.popFront()) {
    HeapPtr<DebuggerFrame*>& frameobj = r.front().value();
    TraceEdge(trc, &frameobj, "live Debugger.Frame");
    MOZ_ASSERT(frameobj->getPrivate(frameobj->numFixedSlotsMaybeForwarded()));
  }

  allocationsLog.trace(trc);

  generatorFrames.trace(trc);
  scripts.trace(trc);
  lazyScripts.trace(trc);
  sources.trace(trc);
  objects.trace(trc);
  environments.trace(trc);
  wasmInstanceScripts.trace(trc);
  wasmInstanceSources.trace(trc);
}

/* static */
void DebugAPI::sweepAll(FreeOp* fop) {
  JSRuntime* rt = fop->runtime();

  Debugger* dbg = rt->debuggerList().getFirst();
  while (dbg) {
    Debugger* next = dbg->getNext();

    // Detach dying debuggers and debuggees from each other. Since this
    // requires access to both objects it must be done before either
    // object is finalized.
    bool debuggerDying = IsAboutToBeFinalized(&dbg->object);
    for (WeakGlobalObjectSet::Enum e(dbg->debuggees); !e.empty();
         e.popFront()) {
      GlobalObject* global = e.front().unbarrieredGet();
      if (debuggerDying || IsAboutToBeFinalizedUnbarriered(&global)) {
        dbg->removeDebuggeeGlobal(fop, e.front().unbarrieredGet(), &e,
                                  Debugger::FromSweep::Yes);
      }
    }

    if (debuggerDying) {
      fop->delete_(dbg->object, dbg, MemoryUse::Debugger);
    }

    dbg = next;
  }
}

/* static */
void Debugger::detachAllDebuggersFromGlobal(FreeOp* fop, GlobalObject* global) {
  const GlobalObject::DebuggerVector* debuggers = global->getDebuggers();
  MOZ_ASSERT(!debuggers->empty());
  while (!debuggers->empty()) {
    debuggers->back()->removeDebuggeeGlobal(fop, global, nullptr,
                                            Debugger::FromSweep::No);
  }
}

static inline bool SweepZonesInSameGroup(Zone* a, Zone* b) {
  // Ensure two zones are swept in the same sweep group by adding an edge
  // between them in each direction.
  return a->addSweepGroupEdgeTo(b) && b->addSweepGroupEdgeTo(a);
}

/* static */
bool DebugAPI::findSweepGroupEdges(JSRuntime* rt) {
  // Ensure that debuggers and their debuggees are finalized in the same group
  // by adding edges in both directions for debuggee zones. These are weak
  // references that are not in the cross compartment wrapper map.

  for (Debugger* dbg : rt->debuggerList()) {
    Zone* debuggerZone = dbg->object->zone();
    if (!debuggerZone->isGCMarking()) {
      continue;
    }

    for (auto e = dbg->debuggeeZones.all(); !e.empty(); e.popFront()) {
      Zone* debuggeeZone = e.front();
      if (!debuggeeZone->isGCMarking()) {
        continue;
      }

      if (SweepZonesInSameGroup(debuggerZone, debuggeeZone)) {
        return false;
      }
    }

    dbg->generatorFrames.findSweepGroupEdges(debuggerZone);
    dbg->scripts.findSweepGroupEdges(debuggerZone);
    dbg->lazyScripts.findSweepGroupEdges(debuggerZone);
    dbg->sources.findSweepGroupEdges(debuggerZone);
    dbg->objects.findSweepGroupEdges(debuggerZone);
    dbg->environments.findSweepGroupEdges(debuggerZone);
    dbg->wasmInstanceScripts.findSweepGroupEdges(debuggerZone);
    dbg->wasmInstanceSources.findSweepGroupEdges(debuggerZone);
  }

  return true;
}

template <class UnbarrieredKey, class Wrapper, bool InvisibleKeysOk>
bool DebuggerWeakMap<UnbarrieredKey, Wrapper, InvisibleKeysOk>::
    findSweepGroupEdges(JS::Zone* debuggerZone) {
  MOZ_ASSERT(debuggerZone->isGCMarking());
  for (Enum e(*this); !e.empty(); e.popFront()) {
    MOZ_ASSERT(e.front().value()->zone() == debuggerZone);

    Zone* keyZone = e.front().key()->zone();
    if (keyZone->isGCMarking() &&
        !SweepZonesInSameGroup(debuggerZone, keyZone)) {
      return false;
    }
  }

  return true;
}

const ClassOps Debugger::classOps_ = {nullptr, /* addProperty */
                                      nullptr, /* delProperty */
                                      nullptr, /* enumerate   */
                                      nullptr, /* newEnumerate */
                                      nullptr, /* resolve     */
                                      nullptr, /* mayResolve  */
                                      nullptr, /* finalize    */
                                      nullptr, /* call        */
                                      nullptr, /* hasInstance */
                                      nullptr, /* construct   */
                                      Debugger::traceObject};

const Class Debugger::class_ = {
    "Debugger",
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUG_COUNT),
    &Debugger::classOps_};

static Debugger* Debugger_fromThisValue(JSContext* cx, const CallArgs& args,
                                        const char* fnname) {
  JSObject* thisobj = RequireObject(cx, args.thisv());
  if (!thisobj) {
    return nullptr;
  }
  if (thisobj->getClass() != &Debugger::class_) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INCOMPATIBLE_PROTO, "Debugger", fnname,
                              thisobj->getClass()->name);
    return nullptr;
  }

  // Forbid Debugger.prototype, which is of the Debugger JSClass but isn't
  // really a Debugger object. The prototype object is distinguished by
  // having a nullptr private value.
  Debugger* dbg = Debugger::fromJSObject(thisobj);
  if (!dbg) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INCOMPATIBLE_PROTO, "Debugger", fnname,
                              "prototype object");
  }
  return dbg;
}

#define THIS_DEBUGGER(cx, argc, vp, fnname, args, dbg)      \
  CallArgs args = CallArgsFromVp(argc, vp);                 \
  Debugger* dbg = Debugger_fromThisValue(cx, args, fnname); \
  if (!dbg) return false

/* static */
bool Debugger::getEnabled(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "get enabled", args, dbg);
  args.rval().setBoolean(dbg->enabled);
  return true;
}

/* static */
bool Debugger::setEnabled(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "set enabled", args, dbg);
  if (!args.requireAtLeast(cx, "Debugger.set enabled", 1)) {
    return false;
  }

  bool wasEnabled = dbg->enabled;
  dbg->enabled = ToBoolean(args[0]);

  if (wasEnabled != dbg->enabled) {
    if (dbg->trackingAllocationSites) {
      if (wasEnabled) {
        dbg->removeAllocationsTrackingForAllDebuggees();
      } else {
        if (!dbg->addAllocationsTrackingForAllDebuggees(cx)) {
          dbg->enabled = false;
          return false;
        }
      }
    }

    for (Breakpoint* bp = dbg->firstBreakpoint(); bp;
         bp = bp->nextInDebugger()) {
      if (!wasEnabled) {
        bp->site->inc(cx->runtime()->defaultFreeOp());
      } else {
        bp->site->dec(cx->runtime()->defaultFreeOp());
      }
    }

    // Add or remove ourselves from the runtime's list of Debuggers
    // that care about new globals.
    if (dbg->getHook(OnNewGlobalObject)) {
      if (!wasEnabled) {
        cx->runtime()->onNewGlobalObjectWatchers().pushBack(dbg);
      } else {
        cx->runtime()->onNewGlobalObjectWatchers().remove(dbg);
      }
    }

    // Ensure the compartment is observable if we are re-enabling a
    // Debugger with hooks that observe all execution.
    if (!dbg->updateObservesAllExecutionOnDebuggees(
            cx, dbg->observesAllExecution())) {
      return false;
    }

    // Note: To toogle code coverage, we currently need to have no live
    // stack frame, thus the coverage does not depend on the enabled flag.

    dbg->updateObservesAsmJSOnDebuggees(dbg->observesAsmJS());
  }

  args.rval().setUndefined();
  return true;
}

/* static */
bool Debugger::getHookImpl(JSContext* cx, CallArgs& args, Debugger& dbg,
                           Hook which) {
  MOZ_ASSERT(which >= 0 && which < HookCount);
  args.rval().set(dbg.object->getReservedSlot(JSSLOT_DEBUG_HOOK_START + which));
  return true;
}

/* static */
bool Debugger::setHookImpl(JSContext* cx, CallArgs& args, Debugger& dbg,
                           Hook which) {
  MOZ_ASSERT(which >= 0 && which < HookCount);
  if (!args.requireAtLeast(cx, "Debugger.setHook", 1)) {
    return false;
  }
  if (args[0].isObject()) {
    if (!args[0].toObject().isCallable()) {
      return ReportIsNotFunction(cx, args[0], args.length() - 1);
    }
  } else if (!args[0].isUndefined()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_CALLABLE_OR_UNDEFINED);
    return false;
  }
  uint32_t slot = JSSLOT_DEBUG_HOOK_START + which;
  RootedValue oldHook(cx, dbg.object->getReservedSlot(slot));
  dbg.object->setReservedSlot(slot, args[0]);
  if (hookObservesAllExecution(which)) {
    if (!dbg.updateObservesAllExecutionOnDebuggees(
            cx, dbg.observesAllExecution())) {
      dbg.object->setReservedSlot(slot, oldHook);
      return false;
    }
  }
  args.rval().setUndefined();
  return true;
}

/* static */
bool Debugger::getOnDebuggerStatement(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "(get onDebuggerStatement)", args, dbg);
  return getHookImpl(cx, args, *dbg, OnDebuggerStatement);
}

/* static */
bool Debugger::setOnDebuggerStatement(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "(set onDebuggerStatement)", args, dbg);
  return setHookImpl(cx, args, *dbg, OnDebuggerStatement);
}

/* static */
bool Debugger::getOnExceptionUnwind(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "(get onExceptionUnwind)", args, dbg);
  return getHookImpl(cx, args, *dbg, OnExceptionUnwind);
}

/* static */
bool Debugger::setOnExceptionUnwind(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "(set onExceptionUnwind)", args, dbg);
  return setHookImpl(cx, args, *dbg, OnExceptionUnwind);
}

/* static */
bool Debugger::getOnNewScript(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "(get onNewScript)", args, dbg);
  return getHookImpl(cx, args, *dbg, OnNewScript);
}

/* static */
bool Debugger::setOnNewScript(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "(set onNewScript)", args, dbg);
  return setHookImpl(cx, args, *dbg, OnNewScript);
}

/* static */
bool Debugger::getOnNewPromise(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "(get onNewPromise)", args, dbg);
  return getHookImpl(cx, args, *dbg, OnNewPromise);
}

/* static */
bool Debugger::setOnNewPromise(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "(set onNewPromise)", args, dbg);
  return setHookImpl(cx, args, *dbg, OnNewPromise);
}

/* static */
bool Debugger::getOnPromiseSettled(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "(get onPromiseSettled)", args, dbg);
  return getHookImpl(cx, args, *dbg, OnPromiseSettled);
}

/* static */
bool Debugger::setOnPromiseSettled(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "(set onPromiseSettled)", args, dbg);
  return setHookImpl(cx, args, *dbg, OnPromiseSettled);
}

/* static */
bool Debugger::getOnEnterFrame(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "(get onEnterFrame)", args, dbg);
  return getHookImpl(cx, args, *dbg, OnEnterFrame);
}

/* static */
bool Debugger::setOnEnterFrame(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "(set onEnterFrame)", args, dbg);
  return setHookImpl(cx, args, *dbg, OnEnterFrame);
}

/* static */
bool Debugger::getOnNewGlobalObject(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "(get onNewGlobalObject)", args, dbg);
  return getHookImpl(cx, args, *dbg, OnNewGlobalObject);
}

/* static */
bool Debugger::setOnNewGlobalObject(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "setOnNewGlobalObject", args, dbg);
  RootedObject oldHook(cx, dbg->getHook(OnNewGlobalObject));

  if (!setHookImpl(cx, args, *dbg, OnNewGlobalObject)) {
    return false;
  }

  // Add or remove ourselves from the runtime's list of Debuggers that care
  // about new globals.
  if (dbg->enabled) {
    JSObject* newHook = dbg->getHook(OnNewGlobalObject);
    if (!oldHook && newHook) {
      cx->runtime()->onNewGlobalObjectWatchers().pushBack(dbg);
    } else if (oldHook && !newHook) {
      cx->runtime()->onNewGlobalObjectWatchers().remove(dbg);
    }
  }

  return true;
}

/* static */
bool Debugger::getUncaughtExceptionHook(JSContext* cx, unsigned argc,
                                        Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "get uncaughtExceptionHook", args, dbg);
  args.rval().setObjectOrNull(dbg->uncaughtExceptionHook);
  return true;
}

/* static */
bool Debugger::setUncaughtExceptionHook(JSContext* cx, unsigned argc,
                                        Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "set uncaughtExceptionHook", args, dbg);
  if (!args.requireAtLeast(cx, "Debugger.set uncaughtExceptionHook", 1)) {
    return false;
  }
  if (!args[0].isNull() &&
      (!args[0].isObject() || !args[0].toObject().isCallable())) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_ASSIGN_FUNCTION_OR_NULL,
                              "uncaughtExceptionHook");
    return false;
  }
  dbg->uncaughtExceptionHook = args[0].toObjectOrNull();
  args.rval().setUndefined();
  return true;
}

/* static */
bool Debugger::getAllowUnobservedAsmJS(JSContext* cx, unsigned argc,
                                       Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "get allowUnobservedAsmJS", args, dbg);
  args.rval().setBoolean(dbg->allowUnobservedAsmJS);
  return true;
}

/* static */
bool Debugger::setAllowUnobservedAsmJS(JSContext* cx, unsigned argc,
                                       Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "set allowUnobservedAsmJS", args, dbg);
  if (!args.requireAtLeast(cx, "Debugger.set allowUnobservedAsmJS", 1)) {
    return false;
  }
  dbg->allowUnobservedAsmJS = ToBoolean(args[0]);

  for (WeakGlobalObjectSet::Range r = dbg->debuggees.all(); !r.empty();
       r.popFront()) {
    GlobalObject* global = r.front();
    Realm* realm = global->realm();
    realm->updateDebuggerObservesAsmJS();
  }

  args.rval().setUndefined();
  return true;
}

/* static */
bool Debugger::getCollectCoverageInfo(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "get collectCoverageInfo", args, dbg);
  args.rval().setBoolean(dbg->collectCoverageInfo);
  return true;
}

/* static */
bool Debugger::setCollectCoverageInfo(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "set collectCoverageInfo", args, dbg);
  if (!args.requireAtLeast(cx, "Debugger.set collectCoverageInfo", 1)) {
    return false;
  }
  dbg->collectCoverageInfo = ToBoolean(args[0]);

  IsObserving observing = dbg->collectCoverageInfo ? Observing : NotObserving;
  if (!dbg->updateObservesCoverageOnDebuggees(cx, observing)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/* static */
bool Debugger::getMemory(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "get memory", args, dbg);
  Value memoryValue =
      dbg->object->getReservedSlot(JSSLOT_DEBUG_MEMORY_INSTANCE);

  if (!memoryValue.isObject()) {
    RootedObject memory(cx, DebuggerMemory::create(cx, dbg));
    if (!memory) {
      return false;
    }
    memoryValue = ObjectValue(*memory);
  }

  args.rval().set(memoryValue);
  return true;
}

/*
 * Given a value used to designate a global (there's quite a variety; see the
 * docs), return the actual designee.
 *
 * Note that this does not check whether the designee is marked "invisible to
 * Debugger" or not; different callers need to handle invisible-to-Debugger
 * globals in different ways.
 */
GlobalObject* Debugger::unwrapDebuggeeArgument(JSContext* cx, const Value& v) {
  if (!v.isObject()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_UNEXPECTED_TYPE, "argument",
                              "not a global object");
    return nullptr;
  }

  RootedObject obj(cx, &v.toObject());

  // If it's a Debugger.Object belonging to this debugger, dereference that.
  if (obj->getClass() == &DebuggerObject::class_) {
    RootedValue rv(cx, v);
    if (!unwrapDebuggeeValue(cx, &rv)) {
      return nullptr;
    }
    obj = &rv.toObject();
  }

  // If we have a cross-compartment wrapper, dereference as far as is secure.
  //
  // Since we're dealing with globals, we may have a WindowProxy here.  So we
  // have to make sure to do a dynamic unwrap, and we want to unwrap the
  // WindowProxy too, if we have one.
  obj = CheckedUnwrapDynamic(obj, cx, /* stopAtWindowProxy = */ false);
  if (!obj) {
    ReportAccessDenied(cx);
    return nullptr;
  }

  // If that didn't produce a global object, it's an error.
  if (!obj->is<GlobalObject>()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_UNEXPECTED_TYPE, "argument",
                              "not a global object");
    return nullptr;
  }

  return &obj->as<GlobalObject>();
}

/* static */
bool Debugger::addDebuggee(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "addDebuggee", args, dbg);
  if (!args.requireAtLeast(cx, "Debugger.addDebuggee", 1)) {
    return false;
  }
  Rooted<GlobalObject*> global(cx, dbg->unwrapDebuggeeArgument(cx, args[0]));
  if (!global) {
    return false;
  }

  if (!dbg->addDebuggeeGlobal(cx, global)) {
    return false;
  }

  RootedValue v(cx, ObjectValue(*global));
  if (!dbg->wrapDebuggeeValue(cx, &v)) {
    return false;
  }
  args.rval().set(v);
  return true;
}

/* static */
bool Debugger::addAllGlobalsAsDebuggees(JSContext* cx, unsigned argc,
                                        Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "addAllGlobalsAsDebuggees", args, dbg);
  for (CompartmentsIter comp(cx->runtime()); !comp.done(); comp.next()) {
    if (comp == dbg->object->compartment()) {
      continue;
    }
    for (RealmsInCompartmentIter r(comp); !r.done(); r.next()) {
      if (r->creationOptions().invisibleToDebugger()) {
        continue;
      }
      r->compartment()->gcState.scheduledForDestruction = false;
      GlobalObject* global = r->maybeGlobal();
      if (global) {
        Rooted<GlobalObject*> rg(cx, global);
        if (!dbg->addDebuggeeGlobal(cx, rg)) {
          return false;
        }
      }
    }
  }

  args.rval().setUndefined();
  return true;
}

/* static */
bool Debugger::removeDebuggee(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "removeDebuggee", args, dbg);

  if (!args.requireAtLeast(cx, "Debugger.removeDebuggee", 1)) {
    return false;
  }
  Rooted<GlobalObject*> global(cx, dbg->unwrapDebuggeeArgument(cx, args[0]));
  if (!global) {
    return false;
  }

  ExecutionObservableRealms obs(cx);

  if (dbg->debuggees.has(global)) {
    dbg->removeDebuggeeGlobal(cx->runtime()->defaultFreeOp(), global, nullptr,
                              FromSweep::No);

    // Only update the realm if there are no Debuggers left, as it's
    // expensive to check if no other Debugger has a live script or frame
    // hook on any of the current on-stack debuggee frames.
    if (global->getDebuggers()->empty() && !obs.add(global->realm())) {
      return false;
    }
    if (!updateExecutionObservability(cx, obs, NotObserving)) {
      return false;
    }
  }

  args.rval().setUndefined();
  return true;
}

/* static */
bool Debugger::removeAllDebuggees(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "removeAllDebuggees", args, dbg);

  ExecutionObservableRealms obs(cx);

  for (WeakGlobalObjectSet::Enum e(dbg->debuggees); !e.empty(); e.popFront()) {
    Rooted<GlobalObject*> global(cx, e.front());
    dbg->removeDebuggeeGlobal(cx->runtime()->defaultFreeOp(), global, &e,
                              FromSweep::No);

    // See note about adding to the observable set in removeDebuggee.
    if (global->getDebuggers()->empty() && !obs.add(global->realm())) {
      return false;
    }
  }

  if (!updateExecutionObservability(cx, obs, NotObserving)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/* static */
bool Debugger::hasDebuggee(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "hasDebuggee", args, dbg);
  if (!args.requireAtLeast(cx, "Debugger.hasDebuggee", 1)) {
    return false;
  }
  GlobalObject* global = dbg->unwrapDebuggeeArgument(cx, args[0]);
  if (!global) {
    return false;
  }
  args.rval().setBoolean(!!dbg->debuggees.lookup(global));
  return true;
}

/* static */
bool Debugger::getDebuggees(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "getDebuggees", args, dbg);

  // Obtain the list of debuggees before wrapping each debuggee, as a GC could
  // update the debuggees set while we are iterating it.
  unsigned count = dbg->debuggees.count();
  RootedValueVector debuggees(cx);
  if (!debuggees.resize(count)) {
    return false;
  }
  unsigned i = 0;
  {
    JS::AutoCheckCannotGC nogc;
    for (WeakGlobalObjectSet::Enum e(dbg->debuggees); !e.empty();
         e.popFront()) {
      debuggees[i++].setObject(*e.front().get());
    }
  }

  RootedArrayObject arrobj(cx, NewDenseFullyAllocatedArray(cx, count));
  if (!arrobj) {
    return false;
  }
  arrobj->ensureDenseInitializedLength(cx, 0, count);
  for (i = 0; i < count; i++) {
    RootedValue v(cx, debuggees[i]);
    if (!dbg->wrapDebuggeeValue(cx, &v)) {
      return false;
    }
    arrobj->setDenseElement(i, v);
  }

  args.rval().setObject(*arrobj);
  return true;
}

/* static */
bool Debugger::getNewestFrame(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "getNewestFrame", args, dbg);

  // Since there may be multiple contexts, use AllFramesIter.
  for (AllFramesIter i(cx); !i.done(); ++i) {
    if (dbg->observesFrame(i)) {
      // Ensure that Ion frames are rematerialized. Only rematerialized
      // Ion frames may be used as AbstractFramePtrs.
      if (i.isIon() && !i.ensureHasRematerializedFrame(cx)) {
        return false;
      }
      AbstractFramePtr frame = i.abstractFramePtr();
      FrameIter iter(i.activation()->cx());
      while (!iter.hasUsableAbstractFramePtr() ||
             iter.abstractFramePtr() != frame) {
        ++iter;
      }
      return dbg->getFrame(cx, iter, args.rval());
    }
  }
  args.rval().setNull();
  return true;
}

/* static */
bool Debugger::clearAllBreakpoints(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "clearAllBreakpoints", args, dbg);
  for (WeakGlobalObjectSet::Range r = dbg->debuggees.all(); !r.empty();
       r.popFront()) {
    DebugScript::clearBreakpointsIn(cx->runtime()->defaultFreeOp(),
                                    r.front()->realm(), dbg, nullptr);
  }
  return true;
}

/* static */
bool Debugger::construct(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Check that the arguments, if any, are cross-compartment wrappers.
  for (unsigned i = 0; i < args.length(); i++) {
    JSObject* argobj = RequireObject(cx, args[i]);
    if (!argobj) {
      return false;
    }
    if (!argobj->is<CrossCompartmentWrapperObject>()) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_DEBUG_CCW_REQUIRED, "Debugger");
      return false;
    }
  }

  // Get Debugger.prototype.
  RootedValue v(cx);
  RootedObject callee(cx, &args.callee());
  if (!GetProperty(cx, callee, callee, cx->names().prototype, &v)) {
    return false;
  }
  RootedNativeObject proto(cx, &v.toObject().as<NativeObject>());
  MOZ_ASSERT(proto->getClass() == &Debugger::class_);

  // Make the new Debugger object. Each one has a reference to
  // Debugger.{Frame,Object,Script,Memory}.prototype in reserved slots. The
  // rest of the reserved slots are for hooks; they default to undefined.
  RootedNativeObject obj(cx, NewNativeObjectWithGivenProto(
                                 cx, &Debugger::class_, proto, TenuredObject));
  if (!obj) {
    return false;
  }
  for (unsigned slot = JSSLOT_DEBUG_PROTO_START; slot < JSSLOT_DEBUG_PROTO_STOP;
       slot++) {
    obj->setReservedSlot(slot, proto->getReservedSlot(slot));
  }
  obj->setReservedSlot(JSSLOT_DEBUG_MEMORY_INSTANCE, NullValue());

  Debugger* debugger;
  {
    // Construct the underlying C++ object.
    auto dbg = cx->make_unique<Debugger>(cx, obj.get());
    if (!dbg) {
      return false;
    }

    // The object owns the released pointer.
    debugger = dbg.release();
    InitObjectPrivate(obj, debugger, MemoryUse::Debugger);
  }

  // Add the initial debuggees, if any.
  for (unsigned i = 0; i < args.length(); i++) {
    JSObject& wrappedObj =
        args[i].toObject().as<ProxyObject>().private_().toObject();
    Rooted<GlobalObject*> debuggee(cx, &wrappedObj.nonCCWGlobal());
    if (!debugger->addDebuggeeGlobal(cx, debuggee)) {
      return false;
    }
  }

  args.rval().setObject(*obj);
  return true;
}

bool Debugger::addDebuggeeGlobal(JSContext* cx, Handle<GlobalObject*> global) {
  if (debuggees.has(global)) {
    return true;
  }

  // Callers should generally be unable to get a reference to a debugger-
  // invisible global in order to pass it to addDebuggee. But this is possible
  // with certain testing aides we expose in the shell, so just make addDebuggee
  // throw in that case.
  Realm* debuggeeRealm = global->realm();
  if (debuggeeRealm->creationOptions().invisibleToDebugger()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_CANT_DEBUG_GLOBAL);
    return false;
  }

  // Debugger and debuggee must be in different compartments.
  if (debuggeeRealm->compartment() == object->compartment()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_SAME_COMPARTMENT);
    return false;
  }

  // Check for cycles. If global's realm is reachable from this Debugger
  // object's realm by following debuggee-to-debugger links, then adding
  // global would create a cycle. (Typically nobody is debugging the
  // debugger, in which case we zip through this code without looping.)
  Vector<Realm*> visited(cx);
  if (!visited.append(object->realm())) {
    return false;
  }
  for (size_t i = 0; i < visited.length(); i++) {
    Realm* realm = visited[i];
    if (realm == debuggeeRealm) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEBUG_LOOP);
      return false;
    }

    // Find all realms containing debuggers debugging realm's global object.
    // Add those realms to visited.
    if (realm->isDebuggee()) {
      GlobalObject::DebuggerVector* v = realm->maybeGlobal()->getDebuggers();
      for (auto p = v->begin(); p != v->end(); p++) {
        Realm* next = (*p)->object->realm();
        if (Find(visited, next) == visited.end() && !visited.append(next)) {
          return false;
        }
      }
    }
  }

  // For global to become this js::Debugger's debuggee:
  //
  // 1. this js::Debugger must be in global->getDebuggers(),
  // 2. global must be in this->debuggees,
  // 3. it must be in zone->getDebuggers(),
  // 4. the debuggee's zone must be in this->debuggeeZones,
  // 5. if we are tracking allocations, the SavedStacksMetadataBuilder must be
  //    installed for this realm, and
  // 6. Realm::isDebuggee()'s bit must be set.
  //
  // All six indications must be kept consistent.

  AutoRealm ar(cx, global);
  Zone* zone = global->zone();

  // (1)
  auto* globalDebuggers = GlobalObject::getOrCreateDebuggers(cx, global);
  if (!globalDebuggers) {
    return false;
  }
  if (!globalDebuggers->append(this)) {
    ReportOutOfMemory(cx);
    return false;
  }
  auto globalDebuggersGuard =
      MakeScopeExit([&] { globalDebuggers->popBack(); });

  // (2)
  if (!debuggees.put(global)) {
    ReportOutOfMemory(cx);
    return false;
  }
  auto debuggeesGuard = MakeScopeExit([&] { debuggees.remove(global); });

  bool addingZoneRelation = !debuggeeZones.has(zone);

  // (3)
  auto* zoneDebuggers = zone->getOrCreateDebuggers(cx);
  if (!zoneDebuggers) {
    return false;
  }
  if (addingZoneRelation && !zoneDebuggers->append(this)) {
    ReportOutOfMemory(cx);
    return false;
  }
  auto zoneDebuggersGuard = MakeScopeExit([&] {
    if (addingZoneRelation) {
      zoneDebuggers->popBack();
    }
  });

  // (4)
  if (addingZoneRelation && !debuggeeZones.put(zone)) {
    ReportOutOfMemory(cx);
    return false;
  }
  auto debuggeeZonesGuard = MakeScopeExit([&] {
    if (addingZoneRelation) {
      debuggeeZones.remove(zone);
    }
  });

  // (5)
  if (trackingAllocationSites && enabled &&
      !Debugger::addAllocationsTracking(cx, global)) {
    return false;
  }

  auto allocationsTrackingGuard = MakeScopeExit([&] {
    if (trackingAllocationSites && enabled) {
      Debugger::removeAllocationsTracking(*global);
    }
  });

  // (6)
  AutoRestoreRealmDebugMode debugModeGuard(debuggeeRealm);
  debuggeeRealm->setIsDebuggee();
  debuggeeRealm->updateDebuggerObservesAsmJS();
  debuggeeRealm->updateDebuggerObservesCoverage();
  if (observesAllExecution() &&
      !ensureExecutionObservabilityOfRealm(cx, debuggeeRealm)) {
    return false;
  }

  globalDebuggersGuard.release();
  debuggeesGuard.release();
  zoneDebuggersGuard.release();
  debuggeeZonesGuard.release();
  allocationsTrackingGuard.release();
  debugModeGuard.release();
  return true;
}

void Debugger::recomputeDebuggeeZoneSet() {
  AutoEnterOOMUnsafeRegion oomUnsafe;
  debuggeeZones.clear();
  for (auto range = debuggees.all(); !range.empty(); range.popFront()) {
    if (!debuggeeZones.put(range.front().unbarrieredGet()->zone())) {
      oomUnsafe.crash("Debugger::removeDebuggeeGlobal");
    }
  }
}

template <typename T, typename AP>
static T* findDebuggerInVector(Debugger* dbg, Vector<T, 0, AP>* vec) {
  T* p;
  for (p = vec->begin(); p != vec->end(); p++) {
    if (*p == dbg) {
      break;
    }
  }
  MOZ_ASSERT(p != vec->end());
  return p;
}

// a WeakHeapPtr version for findDebuggerInVector
// TODO: Bug 1515934 - findDebuggerInVector<T> triggers read barriers.
template <typename AP>
static WeakHeapPtr<Debugger*>* findDebuggerInVector(
    Debugger* dbg, Vector<WeakHeapPtr<Debugger*>, 0, AP>* vec) {
  WeakHeapPtr<Debugger*>* p;
  for (p = vec->begin(); p != vec->end(); p++) {
    if (p->unbarrieredGet() == dbg) {
      break;
    }
  }
  MOZ_ASSERT(p != vec->end());
  return p;
}

void Debugger::removeDebuggeeGlobal(FreeOp* fop, GlobalObject* global,
                                    WeakGlobalObjectSet::Enum* debugEnum,
                                    FromSweep fromSweep) {
  // The caller might have found global by enumerating this->debuggees; if
  // so, use HashSet::Enum::removeFront rather than HashSet::remove below,
  // to avoid invalidating the live enumerator.
  MOZ_ASSERT(debuggees.has(global));
  MOZ_ASSERT(debuggeeZones.has(global->zone()));
  MOZ_ASSERT_IF(debugEnum, debugEnum->front().unbarrieredGet() == global);

  // FIXME Debugger::slowPathOnLeaveFrame needs to kill all Debugger.Frame
  // objects referring to a particular JS stack frame. This is hard if
  // Debugger objects that are no longer debugging the relevant global might
  // have live Frame objects. So we take the easy way out and kill them here.
  // This is a bug, since it's observable and contrary to the spec. One
  // possible fix would be to put such objects into a compartment-wide bag
  // which slowPathOnLeaveFrame would have to examine.
  for (FrameMap::Enum e(frames); !e.empty(); e.popFront()) {
    AbstractFramePtr frame = e.front().key();
    DebuggerFrame* frameobj = e.front().value();
    if (frame.hasGlobal(global)) {
      frameobj->freeFrameIterData(fop);
      frameobj->maybeDecrementFrameScriptStepperCount(fop, frame);
      e.removeFront();
    }
  }

  // Clear this global's generators from generatorFrames as well.
  //
  // This method can be called either from script (dbg.removeDebuggee) or during
  // GC sweeping, because the Debugger, debuggee global, or both are being GC'd.
  //
  // When called from script, it's okay to iterate over generatorFrames and
  // touch its keys and values (even when an incremental GC is in progress).
  // When called from GC, it's not okay; the keys and values may be dying. But
  // in that case, we can actually just skip the loop entirely! If the Debugger
  // is going away, it doesn't care about the state of its generatorFrames
  // table, and the Debugger.Frame finalizer will fix up the generator observer
  // counts.
  if (fromSweep == FromSweep::No) {
    for (GeneratorWeakMap::Enum e(generatorFrames); !e.empty(); e.popFront()) {
      auto& genObj = e.front().key()->as<AbstractGeneratorObject>();
      auto& frameObj = e.front().value()->as<DebuggerFrame>();
      if (genObj.isClosed() || &genObj.callee().global() == global) {
        frameObj.clearGenerator(fop, this, &e);
      }
    }
  }

  auto* globalDebuggersVector = global->getDebuggers();
  auto* zoneDebuggersVector = global->zone()->getDebuggers();

  // The relation must be removed from up to three places:
  // globalDebuggersVector and debuggees for sure, and possibly the
  // compartment's debuggee set.
  //
  // The debuggee zone set is recomputed on demand. This avoids refcounting
  // and in practice we have relatively few debuggees that tend to all be in
  // the same zone. If after recomputing the debuggee zone set, this global's
  // zone is not in the set, then we must remove ourselves from the zone's
  // vector of observing debuggers.
  globalDebuggersVector->erase(
      findDebuggerInVector(this, globalDebuggersVector));

  if (debugEnum) {
    debugEnum->removeFront();
  } else {
    debuggees.remove(global);
  }

  recomputeDebuggeeZoneSet();

  if (!debuggeeZones.has(global->zone())) {
    zoneDebuggersVector->erase(findDebuggerInVector(this, zoneDebuggersVector));
  }

  // Remove all breakpoints for the debuggee.
  Breakpoint* nextbp;
  for (Breakpoint* bp = firstBreakpoint(); bp; bp = nextbp) {
    nextbp = bp->nextInDebugger();
    switch (bp->site->type()) {
      case BreakpointSite::Type::JS:
        if (bp->site->asJS()->script->realm() == global->realm()) {
          bp->destroy(fop);
        }
        break;
      case BreakpointSite::Type::Wasm:
        if (bp->asWasm()->wasmInstance->realm() == global->realm()) {
          bp->destroy(fop);
        }
        break;
      default:
        MOZ_CRASH("unknown breakpoint type");
    }
  }
  MOZ_ASSERT_IF(debuggees.empty(), !firstBreakpoint());

  // If we are tracking allocation sites, we need to remove the object
  // metadata callback from this global's realm.
  if (trackingAllocationSites) {
    Debugger::removeAllocationsTracking(*global);
  }

  if (global->getDebuggers()->empty()) {
    global->realm()->unsetIsDebuggee();
  } else {
    global->realm()->updateDebuggerObservesAllExecution();
    global->realm()->updateDebuggerObservesAsmJS();
    global->realm()->updateDebuggerObservesCoverage();
  }
}

class MOZ_STACK_CLASS Debugger::QueryBase {
 protected:
  QueryBase(JSContext* cx, Debugger* dbg)
      : cx(cx),
        debugger(dbg),
        iterMarker(&cx->runtime()->gc),
        realms(cx->zone()),
        oom(false) {}

  // The context in which we should do our work.
  JSContext* cx;

  // The debugger for which we conduct queries.
  Debugger* debugger;

  // Require the set of realms to stay fixed while the query is alive.
  gc::AutoEnterIteration iterMarker;

  using RealmSet = HashSet<Realm*, DefaultHasher<Realm*>, ZoneAllocPolicy>;

  // A script must be in one of these realms to match the query.
  RealmSet realms;

  // Indicates whether OOM has occurred while matching.
  bool oom;

  bool addRealm(Realm* realm) { return realms.put(realm); }

  // Arrange for this query to match only scripts that run in |global|.
  bool matchSingleGlobal(GlobalObject* global) {
    MOZ_ASSERT(realms.count() == 0);
    if (!addRealm(global->realm())) {
      ReportOutOfMemory(cx);
      return false;
    }
    return true;
  }

  // Arrange for this ScriptQuery to match all scripts running in debuggee
  // globals.
  bool matchAllDebuggeeGlobals() {
    MOZ_ASSERT(realms.count() == 0);
    // Build our realm set from the debugger's set of debuggee globals.
    for (WeakGlobalObjectSet::Range r = debugger->debuggees.all(); !r.empty();
         r.popFront()) {
      if (!addRealm(r.front()->realm())) {
        ReportOutOfMemory(cx);
        return false;
      }
    }
    return true;
  }
};

/*
 * A class for parsing 'findScripts' query arguments and searching for
 * scripts that match the criteria they represent.
 */
class MOZ_STACK_CLASS Debugger::ScriptQuery : public Debugger::QueryBase {
 public:
  /* Construct a ScriptQuery to use matching scripts for |dbg|. */
  ScriptQuery(JSContext* cx, Debugger* dbg)
      : QueryBase(cx, dbg),
        url(cx),
        displayURLString(cx),
        hasSource(false),
        source(cx, AsVariant(static_cast<ScriptSourceObject*>(nullptr))),
        hasLine(false),
        line(0),
        innermost(false),
        innermostForRealm(cx, cx->zone()),
        scriptVector(cx, ScriptVector(cx)),
        lazyScriptVector(cx, LazyScriptVector(cx)),
        wasmInstanceVector(cx, WasmInstanceObjectVector(cx)) {}

  /*
   * Parse the query object |query|, and prepare to match only the scripts
   * it specifies.
   */
  bool parseQuery(HandleObject query) {
    // Check for a 'global' property, which limits the results to those
    // scripts scoped to a particular global object.
    RootedValue global(cx);
    if (!GetProperty(cx, query, query, cx->names().global, &global)) {
      return false;
    }
    if (global.isUndefined()) {
      if (!matchAllDebuggeeGlobals()) {
        return false;
      }
    } else {
      GlobalObject* globalObject = debugger->unwrapDebuggeeArgument(cx, global);
      if (!globalObject) {
        return false;
      }

      // If the given global isn't a debuggee, just leave the set of
      // acceptable globals empty; we'll return no scripts.
      if (debugger->debuggees.has(globalObject)) {
        if (!matchSingleGlobal(globalObject)) {
          return false;
        }
      }
    }

    // Check for a 'url' property.
    if (!GetProperty(cx, query, query, cx->names().url, &url)) {
      return false;
    }
    if (!url.isUndefined() && !url.isString()) {
      JS_ReportErrorNumberASCII(
          cx, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
          "query object's 'url' property", "neither undefined nor a string");
      return false;
    }

    // Check for a 'source' property
    RootedValue debuggerSource(cx);
    if (!GetProperty(cx, query, query, cx->names().source, &debuggerSource)) {
      return false;
    }
    if (!debuggerSource.isUndefined()) {
      if (!debuggerSource.isObject() ||
          !debuggerSource.toObject().is<DebuggerSource>()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_UNEXPECTED_TYPE,
                                  "query object's 'source' property",
                                  "not undefined nor a Debugger.Source object");
        return false;
      }

      Value owner =
          debuggerSource.toObject().as<DebuggerSource>().getReservedSlot(
              DebuggerSource::OWNER_SLOT);

      // The given source must have an owner. Otherwise, it's a
      // Debugger.Source.prototype, which would match no scripts, and is
      // probably a mistake.
      if (!owner.isObject()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_DEBUG_PROTO, "Debugger.Source",
                                  "Debugger.Source");
        return false;
      }

      // If it does have an owner, it should match the Debugger we're
      // calling findScripts on. It would work fine even if it didn't,
      // but mixing Debugger.Sources is probably a sign of confusion.
      if (&owner.toObject() != debugger->object) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_DEBUG_WRONG_OWNER, "Debugger.Source");
        return false;
      }

      hasSource = true;
      source = debuggerSource.toObject().as<DebuggerSource>().getReferent();
    }

    // Check for a 'displayURL' property.
    RootedValue displayURL(cx);
    if (!GetProperty(cx, query, query, cx->names().displayURL, &displayURL)) {
      return false;
    }
    if (!displayURL.isUndefined() && !displayURL.isString()) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_UNEXPECTED_TYPE,
                                "query object's 'displayURL' property",
                                "neither undefined nor a string");
      return false;
    }

    if (displayURL.isString()) {
      displayURLString = displayURL.toString()->ensureLinear(cx);
      if (!displayURLString) {
        return false;
      }
    }

    // Check for a 'line' property.
    RootedValue lineProperty(cx);
    if (!GetProperty(cx, query, query, cx->names().line, &lineProperty)) {
      return false;
    }
    if (lineProperty.isUndefined()) {
      hasLine = false;
    } else if (lineProperty.isNumber()) {
      if (displayURL.isUndefined() && url.isUndefined() && !hasSource) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_QUERY_LINE_WITHOUT_URL);
        return false;
      }
      double doubleLine = lineProperty.toNumber();
      if (doubleLine <= 0 || (unsigned int)doubleLine != doubleLine) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_DEBUG_BAD_LINE);
        return false;
      }
      hasLine = true;
      line = doubleLine;
    } else {
      JS_ReportErrorNumberASCII(
          cx, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
          "query object's 'line' property", "neither undefined nor an integer");
      return false;
    }

    // Check for an 'innermost' property.
    PropertyName* innermostName = cx->names().innermost;
    RootedValue innermostProperty(cx);
    if (!GetProperty(cx, query, query, innermostName, &innermostProperty)) {
      return false;
    }
    innermost = ToBoolean(innermostProperty);
    if (innermost) {
      // Technically, we need only check hasLine, but this is clearer.
      if ((displayURL.isUndefined() && url.isUndefined() && !hasSource) ||
          !hasLine) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_QUERY_INNERMOST_WITHOUT_LINE_URL);
        return false;
      }
    }

    return true;
  }

  /* Set up this ScriptQuery appropriately for a missing query argument. */
  bool omittedQuery() {
    url.setUndefined();
    hasLine = false;
    innermost = false;
    displayURLString = nullptr;
    return matchAllDebuggeeGlobals();
  }

  /*
   * Search all relevant realms and the stack for scripts matching
   * this query, and append the matching scripts to |scriptVector|.
   */
  bool findScripts() {
    if (!prepareQuery()) {
      return false;
    }

    bool delazified = false;
    if (needsDelazifyBeforeQuery()) {
      if (!delazifyScripts()) {
        return false;
      }
      delazified = true;
    }

    Realm* singletonRealm = nullptr;
    if (realms.count() == 1) {
      singletonRealm = realms.all().front();
    }

    // Search each realm for debuggee scripts.
    MOZ_ASSERT(scriptVector.empty());
    MOZ_ASSERT(lazyScriptVector.empty());
    oom = false;
    IterateScripts(cx, singletonRealm, this, considerScript);
    if (!delazified) {
      IterateLazyScripts(cx, singletonRealm, this, considerLazyScript);
    }
    if (oom) {
      ReportOutOfMemory(cx);
      return false;
    }

    // For most queries, we just accumulate results in 'scriptVector' and
    // 'lazyScriptVector' as we find them. But if this is an 'innermost'
    // query, then we've accumulated the results in the 'innermostForRealm'
    // map. In that case, we now need to walk that map and
    // populate 'scriptVector'.
    if (innermost) {
      for (RealmToScriptMap::Range r = innermostForRealm.all(); !r.empty();
           r.popFront()) {
        if (!scriptVector.append(r.front().value())) {
          ReportOutOfMemory(cx);
          return false;
        }
      }
    }

    // TODO: Until such time that wasm modules are real ES6 modules,
    // unconditionally consider all wasm toplevel instance scripts.
    for (WeakGlobalObjectSet::Range r = debugger->allDebuggees(); !r.empty();
         r.popFront()) {
      for (wasm::Instance* instance : r.front()->realm()->wasm.instances()) {
        consider(instance->object());
        if (oom) {
          ReportOutOfMemory(cx);
          return false;
        }
      }
    }

    return true;
  }

  Handle<ScriptVector> foundScripts() const { return scriptVector; }
  Handle<LazyScriptVector> foundLazyScripts() const { return lazyScriptVector; }

  Handle<WasmInstanceObjectVector> foundWasmInstances() const {
    return wasmInstanceVector;
  }

 private:
  /* If this is a string, matching scripts have urls equal to it. */
  RootedValue url;

  /* url as a C string. */
  UniqueChars urlCString;

  /* If this is a string, matching scripts' sources have displayURLs equal to
   * it. */
  RootedLinearString displayURLString;

  /*
   * If this is a source referent, matching scripts will have sources equal
   * to this instance. Ideally we'd use a Maybe here, but Maybe interacts
   * very badly with Rooted's LIFO invariant.
   */
  bool hasSource;
  Rooted<DebuggerSourceReferent> source;

  /* True if the query contained a 'line' property. */
  bool hasLine;

  /* The line matching scripts must cover. */
  unsigned int line;

  /* True if the query has an 'innermost' property whose value is true. */
  bool innermost;

  using RealmToScriptMap =
      GCHashMap<Realm*, JSScript*, DefaultHasher<Realm*>, ZoneAllocPolicy>;

  /*
   * For 'innermost' queries, a map from realms to the innermost script
   * we've seen so far in that realm. (Template instantiation code size
   * explosion ho!)
   */
  Rooted<RealmToScriptMap> innermostForRealm;

  /*
   * Accumulate the scripts in an Rooted<ScriptVector> and
   * Rooted<LazyScriptVector>, instead of creating the JS array as we go,
   * because we mustn't allocate JS objects or GC while we use the CellIter.
   */
  Rooted<ScriptVector> scriptVector;
  Rooted<LazyScriptVector> lazyScriptVector;

  /*
   * Like above, but for wasm modules.
   */
  Rooted<WasmInstanceObjectVector> wasmInstanceVector;

  /*
   * Given that parseQuery or omittedQuery has been called, prepare to match
   * scripts. Set urlCString and displayURLChars as appropriate.
   */
  bool prepareQuery() {
    // Compute urlCString and displayURLChars, if a url or displayURL was
    // given respectively.
    if (url.isString()) {
      urlCString = JS_EncodeStringToLatin1(cx, url.toString());
      if (!urlCString) {
        return false;
      }
    }

    return true;
  }

  bool delazifyScripts() {
    // All scripts in debuggee realms must be visible, so delazify
    // everything.
    for (auto r = realms.all(); !r.empty(); r.popFront()) {
      Realm* realm = r.front();
      if (!realm->ensureDelazifyScriptsForDebugger(cx)) {
        return false;
      }
    }
    return true;
  }

  static void considerScript(JSRuntime* rt, void* data, JSScript* script,
                             const JS::AutoRequireNoGC& nogc) {
    ScriptQuery* self = static_cast<ScriptQuery*>(data);
    self->consider(script, nogc);
  }

  static void considerLazyScript(JSRuntime* rt, void* data,
                                 LazyScript* lazyScript,
                                 const JS::AutoRequireNoGC& nogc) {
    ScriptQuery* self = static_cast<ScriptQuery*>(data);
    self->consider(lazyScript, nogc);
  }

  bool needsDelazifyBeforeQuery() const {
    // * innermost
    //   Currently not supported, since this is not used outside of test.
    //
    // * hasLine
    //   Only JSScript supports GetScriptLineExtent.
    return innermost || hasLine;
  }

  template <typename T>
  MOZ_MUST_USE bool commonFilter(T script, const JS::AutoRequireNoGC& nogc) {
    if (urlCString) {
      bool gotFilename = false;
      if (script->filename() &&
          strcmp(script->filename(), urlCString.get()) == 0) {
        gotFilename = true;
      }

      bool gotSourceURL = false;
      if (!gotFilename && script->scriptSource()->introducerFilename() &&
          strcmp(script->scriptSource()->introducerFilename(),
                 urlCString.get()) == 0) {
        gotSourceURL = true;
      }
      if (!gotFilename && !gotSourceURL) {
        return false;
      }
    }
    if (displayURLString) {
      if (!script->scriptSource() || !script->scriptSource()->hasDisplayURL()) {
        return false;
      }

      const char16_t* s = script->scriptSource()->displayURL();
      if (CompareChars(s, js_strlen(s), displayURLString) != 0) {
        return false;
      }
    }
    if (hasSource && !(source.is<ScriptSourceObject*>() &&
                       source.as<ScriptSourceObject*>()->source() ==
                           script->scriptSource())) {
      return false;
    }
    return true;
  }

  /*
   * If |script| matches this query, append it to |scriptVector| or place it
   * in |innermostForRealm|, as appropriate. Set |oom| if an out of memory
   * condition occurred.
   */
  void consider(JSScript* script, const JS::AutoRequireNoGC& nogc) {
    if (oom || script->selfHosted()) {
      return;
    }
    Realm* realm = script->realm();
    if (!realms.has(realm)) {
      return;
    }
    if (!commonFilter(script, nogc)) {
      return;
    }
    if (hasLine) {
      if (line < script->lineno() ||
          script->lineno() + GetScriptLineExtent(script) < line) {
        return;
      }
    }

    if (innermost) {
      // For 'innermost' queries, we don't place scripts in
      // |scriptVector| right away; we may later find another script that
      // is nested inside this one. Instead, we record the innermost
      // script we've found so far for each realm in innermostForRealm,
      // and only populate |scriptVector| at the bottom of findScripts,
      // when we've traversed all the scripts.
      //
      // So: check this script against the innermost one we've found so
      // far (if any), as recorded in innermostForRealm, and replace that
      // if it's better.
      RealmToScriptMap::AddPtr p = innermostForRealm.lookupForAdd(realm);
      if (p) {
        // Is our newly found script deeper than the last one we found?
        JSScript* incumbent = p->value();
        if (script->innermostScope()->chainLength() >
            incumbent->innermostScope()->chainLength()) {
          p->value() = script;
        }
      } else {
        // This is the first matching script we've encountered for this
        // realm, so it is thus the innermost such script.
        if (!innermostForRealm.add(p, realm, script)) {
          oom = true;
          return;
        }
      }
    } else {
      // Record this matching script in the results scriptVector.
      if (!scriptVector.append(script)) {
        oom = true;
        return;
      }
    }
  }

  void consider(LazyScript* lazyScript, const JS::AutoRequireNoGC& nogc) {
    MOZ_ASSERT(!needsDelazifyBeforeQuery());

    if (oom) {
      return;
    }
    Realm* realm = lazyScript->realm();
    if (!realms.has(realm)) {
      return;
    }

    // If the script is already delazified, it should be in scriptVector.
    if (lazyScript->maybeScript()) {
      return;
    }

    if (!commonFilter(lazyScript, nogc)) {
      return;
    }

    /* Record this matching script in the results lazyScriptVector. */
    if (!lazyScriptVector.append(lazyScript)) {
      oom = true;
    }
  }

  /*
   * If |instanceObject| matches this query, append it to |wasmInstanceVector|.
   * Set |oom| if an out of memory condition occurred.
   */
  void consider(WasmInstanceObject* instanceObject) {
    if (oom) {
      return;
    }

    if (hasSource && source != AsVariant(instanceObject)) {
      return;
    }

    if (!wasmInstanceVector.append(instanceObject)) {
      oom = true;
    }
  }
};

/* static */
bool Debugger::findScripts(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "findScripts", args, dbg);

  ScriptQuery query(cx, dbg);

  if (args.length() >= 1) {
    RootedObject queryObject(cx, RequireObject(cx, args[0]));
    if (!queryObject || !query.parseQuery(queryObject)) {
      return false;
    }
  } else {
    if (!query.omittedQuery()) {
      return false;
    }
  }

  if (!query.findScripts()) {
    return false;
  }

  Handle<ScriptVector> scripts(query.foundScripts());
  Handle<LazyScriptVector> lazyScripts(query.foundLazyScripts());
  Handle<WasmInstanceObjectVector> wasmInstances(query.foundWasmInstances());

  size_t resultLength =
      scripts.length() + lazyScripts.length() + wasmInstances.length();
  RootedArrayObject result(cx, NewDenseFullyAllocatedArray(cx, resultLength));
  if (!result) {
    return false;
  }

  result->ensureDenseInitializedLength(cx, 0, resultLength);

  for (size_t i = 0; i < scripts.length(); i++) {
    JSObject* scriptObject = dbg->wrapScript(cx, scripts[i]);
    if (!scriptObject) {
      return false;
    }
    result->setDenseElement(i, ObjectValue(*scriptObject));
  }

  size_t lazyStart = scripts.length();
  for (size_t i = 0; i < lazyScripts.length(); i++) {
    JSObject* scriptObject = dbg->wrapLazyScript(cx, lazyScripts[i]);
    if (!scriptObject) {
      return false;
    }
    result->setDenseElement(lazyStart + i, ObjectValue(*scriptObject));
  }

  size_t wasmStart = scripts.length() + lazyScripts.length();
  for (size_t i = 0; i < wasmInstances.length(); i++) {
    JSObject* scriptObject = dbg->wrapWasmScript(cx, wasmInstances[i]);
    if (!scriptObject) {
      return false;
    }
    result->setDenseElement(wasmStart + i, ObjectValue(*scriptObject));
  }

  args.rval().setObject(*result);
  return true;
}

/*
 * A class for searching sources for 'findSources'.
 */
class MOZ_STACK_CLASS Debugger::SourceQuery : public Debugger::QueryBase {
 public:
  using SourceSet = JS::GCHashSet<JSObject*, js::MovableCellHasher<JSObject*>,
                                  ZoneAllocPolicy>;

  SourceQuery(JSContext* cx, Debugger* dbg)
      : QueryBase(cx, dbg), sources(cx, SourceSet(cx->zone())) {}

  bool findSources() {
    if (!matchAllDebuggeeGlobals()) {
      return false;
    }

    Realm* singletonRealm = nullptr;
    if (realms.count() == 1) {
      singletonRealm = realms.all().front();
    }

    // Search each realm for debuggee scripts.
    MOZ_ASSERT(sources.empty());
    oom = false;
    IterateScripts(cx, singletonRealm, this, considerScript);
    IterateLazyScripts(cx, singletonRealm, this, considerLazyScript);
    if (oom) {
      ReportOutOfMemory(cx);
      return false;
    }

    // TODO: Until such time that wasm modules are real ES6 modules,
    // unconditionally consider all wasm toplevel instance scripts.
    for (WeakGlobalObjectSet::Range r = debugger->allDebuggees(); !r.empty();
         r.popFront()) {
      for (wasm::Instance* instance : r.front()->realm()->wasm.instances()) {
        consider(instance->object());
        if (oom) {
          ReportOutOfMemory(cx);
          return false;
        }
      }
    }

    return true;
  }

  Handle<SourceSet> foundSources() const { return sources; }

 private:
  Rooted<SourceSet> sources;

  static void considerScript(JSRuntime* rt, void* data, JSScript* script,
                             const JS::AutoRequireNoGC& nogc) {
    SourceQuery* self = static_cast<SourceQuery*>(data);
    self->consider(script, nogc);
  }

  static void considerLazyScript(JSRuntime* rt, void* data,
                                 LazyScript* lazyScript,
                                 const JS::AutoRequireNoGC& nogc) {
    SourceQuery* self = static_cast<SourceQuery*>(data);
    self->consider(lazyScript, nogc);
  }

  void consider(JSScript* script, const JS::AutoRequireNoGC& nogc) {
    if (oom || script->selfHosted()) {
      return;
    }
    Realm* realm = script->realm();
    if (!realms.has(realm)) {
      return;
    }

    if (!script->sourceObject()) {
      return;
    }

    ScriptSourceObject* source =
        &UncheckedUnwrap(script->sourceObject())->as<ScriptSourceObject>();
    if (!sources.put(source)) {
      oom = true;
    }
  }

  void consider(LazyScript* lazyScript, const JS::AutoRequireNoGC& nogc) {
    if (oom) {
      return;
    }
    Realm* realm = lazyScript->realm();
    if (!realms.has(realm)) {
      return;
    }

    // If the script is already delazified, it should already be handled.
    if (lazyScript->maybeScript()) {
      return;
    }

    ScriptSourceObject* source = lazyScript->sourceObject();
    if (!sources.put(source)) {
      oom = true;
    }
  }

  void consider(WasmInstanceObject* instanceObject) {
    if (oom) {
      return;
    }

    if (!sources.put(instanceObject)) {
      oom = true;
    }
  }
};

static inline DebuggerSourceReferent AsSourceReferent(JSObject* obj) {
  if (obj->is<ScriptSourceObject>()) {
    return AsVariant(&obj->as<ScriptSourceObject>());
  }
  return AsVariant(&obj->as<WasmInstanceObject>());
}

/* static */
bool Debugger::findSources(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "findSources", args, dbg);

  SourceQuery query(cx, dbg);
  if (!query.findSources()) {
    return false;
  }

  Handle<SourceQuery::SourceSet> sources(query.foundSources());

  size_t resultLength = sources.count();
  RootedArrayObject result(cx, NewDenseFullyAllocatedArray(cx, resultLength));
  if (!result) {
    return false;
  }

  result->ensureDenseInitializedLength(cx, 0, resultLength);

  size_t i = 0;
  for (auto iter = sources.get().iter(); !iter.done(); iter.next()) {
    Rooted<DebuggerSourceReferent> sourceReferent(cx,
                                                  AsSourceReferent(iter.get()));
    RootedObject sourceObject(cx, dbg->wrapVariantReferent(cx, sourceReferent));
    if (!sourceObject) {
      return false;
    }
    result->setDenseElement(i, ObjectValue(*sourceObject));
    i++;
  }

  args.rval().setObject(*result);
  return true;
}

/*
 * A class for parsing 'findObjects' query arguments and searching for objects
 * that match the criteria they represent.
 */
class MOZ_STACK_CLASS Debugger::ObjectQuery {
 public:
  /* Construct an ObjectQuery to use matching scripts for |dbg|. */
  ObjectQuery(JSContext* cx, Debugger* dbg)
      : objects(cx), cx(cx), dbg(dbg), className(cx) {}

  /* The vector that we are accumulating results in. */
  RootedObjectVector objects;

  /* The set of debuggee compartments. */
  JS::CompartmentSet debuggeeCompartments;

  /*
   * Parse the query object |query|, and prepare to match only the objects it
   * specifies.
   */
  bool parseQuery(HandleObject query) {
    // Check for the 'class' property
    RootedValue cls(cx);
    if (!GetProperty(cx, query, query, cx->names().class_, &cls)) {
      return false;
    }
    if (!cls.isUndefined()) {
      if (!cls.isString()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_UNEXPECTED_TYPE,
                                  "query object's 'class' property",
                                  "neither undefined nor a string");
        return false;
      }
      JSLinearString* str = cls.toString()->ensureLinear(cx);
      if (!str) {
        return false;
      }
      if (!StringIsAscii(str)) {
        JS_ReportErrorNumberASCII(
            cx, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
            "query object's 'class' property",
            "not a string containing only ASCII characters");
        return false;
      }
      className = cls;
    }
    return true;
  }

  /* Set up this ObjectQuery appropriately for a missing query argument. */
  void omittedQuery() { className.setUndefined(); }

  /*
   * Traverse the heap to find all relevant objects and add them to the
   * provided vector.
   */
  bool findObjects() {
    if (!prepareQuery()) {
      return false;
    }

    for (WeakGlobalObjectSet::Range r = dbg->allDebuggees(); !r.empty();
         r.popFront()) {
      if (!debuggeeCompartments.put(r.front()->compartment())) {
        ReportOutOfMemory(cx);
        return false;
      }
    }

    {
      // We can't tolerate the GC moving things around while we're
      // searching the heap. Check that nothing we do causes a GC.
      Maybe<JS::AutoCheckCannotGC> maybeNoGC;
      RootedObject dbgObj(cx, dbg->object);
      JS::ubi::RootList rootList(cx, maybeNoGC);
      if (!rootList.init(dbgObj)) {
        ReportOutOfMemory(cx);
        return false;
      }

      Traversal traversal(cx, *this, maybeNoGC.ref());
      traversal.wantNames = false;

      return traversal.addStart(JS::ubi::Node(&rootList)) &&
             traversal.traverse();
    }
  }

  /*
   * |ubi::Node::BreadthFirst| interface.
   */
  class NodeData {};
  typedef JS::ubi::BreadthFirst<ObjectQuery> Traversal;
  bool operator()(Traversal& traversal, JS::ubi::Node origin,
                  const JS::ubi::Edge& edge, NodeData*, bool first) {
    if (!first) {
      return true;
    }

    JS::ubi::Node referent = edge.referent;

    // Only follow edges within our set of debuggee compartments; we don't
    // care about the heap's subgraphs outside of our debuggee compartments,
    // so we abandon the referent. Either (1) there is not a path from this
    // non-debuggee node back to a node in our debuggee compartments, and we
    // don't need to follow edges to or from this node, or (2) there does
    // exist some path from this non-debuggee node back to a node in our
    // debuggee compartments. However, if that were true, then the incoming
    // cross compartment edge back into a debuggee compartment is already
    // listed as an edge in the RootList we started traversal with, and
    // therefore we don't need to follow edges to or from this non-debuggee
    // node.
    JS::Compartment* comp = referent.compartment();
    if (comp && !debuggeeCompartments.has(comp)) {
      traversal.abandonReferent();
      return true;
    }

    // If the referent has an associated realm and it's not a debuggee
    // realm, skip it. Don't abandonReferent() here like above: realms
    // within a compartment can reference each other without going through
    // cross-compartment wrappers.
    Realm* realm = referent.realm();
    if (realm && !dbg->isDebuggeeUnbarriered(realm)) {
      return true;
    }

    // If the referent is an object and matches our query's restrictions,
    // add it to the vector accumulating results. Skip objects that should
    // never be exposed to JS, like EnvironmentObjects and internal
    // functions.

    if (!referent.is<JSObject>() || referent.exposeToJS().isUndefined()) {
      return true;
    }

    JSObject* obj = referent.as<JSObject>();

    if (!className.isUndefined()) {
      const char* objClassName = obj->getClass()->name;
      if (strcmp(objClassName, classNameCString.get()) != 0) {
        return true;
      }
    }

    return objects.append(obj);
  }

 private:
  /* The context in which we should do our work. */
  JSContext* cx;

  /* The debugger for which we conduct queries. */
  Debugger* dbg;

  /*
   * If this is non-null, matching objects will have a class whose name is
   * this property.
   */
  RootedValue className;

  /* The className member, as a C string. */
  UniqueChars classNameCString;

  /*
   * Given that either omittedQuery or parseQuery has been called, prepare the
   * query for matching objects.
   */
  bool prepareQuery() {
    if (className.isString()) {
      classNameCString = JS_EncodeStringToASCII(cx, className.toString());
      if (!classNameCString) {
        return false;
      }
    }

    return true;
  }
};

bool Debugger::findObjects(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "findObjects", args, dbg);

  ObjectQuery query(cx, dbg);

  if (args.length() >= 1) {
    RootedObject queryObject(cx, RequireObject(cx, args[0]));
    if (!queryObject || !query.parseQuery(queryObject)) {
      return false;
    }
  } else {
    query.omittedQuery();
  }

  if (!query.findObjects()) {
    return false;
  }

  size_t length = query.objects.length();
  RootedArrayObject result(cx, NewDenseFullyAllocatedArray(cx, length));
  if (!result) {
    return false;
  }

  result->ensureDenseInitializedLength(cx, 0, length);

  for (size_t i = 0; i < length; i++) {
    RootedValue debuggeeVal(cx, ObjectValue(*query.objects[i]));
    if (!dbg->wrapDebuggeeValue(cx, &debuggeeVal)) {
      return false;
    }
    result->setDenseElement(i, debuggeeVal);
  }

  args.rval().setObject(*result);
  return true;
}

/* static */
bool Debugger::findAllGlobals(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "findAllGlobals", args, dbg);

  RootedObjectVector globals(cx);

  {
    // Accumulate the list of globals before wrapping them, because
    // wrapping can GC and collect realms from under us, while iterating.
    JS::AutoCheckCannotGC nogc;

    for (RealmsIter r(cx->runtime()); !r.done(); r.next()) {
      if (r->creationOptions().invisibleToDebugger()) {
        continue;
      }

      if (!r->hasLiveGlobal()) {
        continue;
      }

      if (JS::RealmBehaviorsRef(r).isNonLive()) {
        continue;
      }

      r->compartment()->gcState.scheduledForDestruction = false;

      GlobalObject* global = r->maybeGlobal();

      if (cx->runtime()->isSelfHostingGlobal(global)) {
        continue;
      }

      // We pulled |global| out of nowhere, so it's possible that it was
      // marked gray by XPConnect. Since we're now exposing it to JS code,
      // we need to mark it black.
      JS::ExposeObjectToActiveJS(global);
      if (!globals.append(global)) {
        return false;
      }
    }
  }

  RootedObject result(cx, NewDenseEmptyArray(cx));
  if (!result) {
    return false;
  }

  for (size_t i = 0; i < globals.length(); i++) {
    RootedValue globalValue(cx, ObjectValue(*globals[i]));
    if (!dbg->wrapDebuggeeValue(cx, &globalValue)) {
      return false;
    }
    if (!NewbornArrayPush(cx, result, globalValue)) {
      return false;
    }
  }

  args.rval().setObject(*result);
  return true;
}

/* static */
bool Debugger::makeGlobalObjectReference(JSContext* cx, unsigned argc,
                                         Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "makeGlobalObjectReference", args, dbg);
  if (!args.requireAtLeast(cx, "Debugger.makeGlobalObjectReference", 1)) {
    return false;
  }

  Rooted<GlobalObject*> global(cx, dbg->unwrapDebuggeeArgument(cx, args[0]));
  if (!global) {
    return false;
  }

  // If we create a D.O referring to a global in an invisible realm,
  // then from it we can reach function objects, scripts, environments, etc.,
  // none of which we're ever supposed to see.
  if (global->realm()->creationOptions().invisibleToDebugger()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_INVISIBLE_COMPARTMENT);
    return false;
  }

  args.rval().setObject(*global);
  return dbg->wrapDebuggeeValue(cx, args.rval());
}

bool Debugger::isCompilableUnit(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.requireAtLeast(cx, "Debugger.isCompilableUnit", 1)) {
    return false;
  }

  if (!args[0].isString()) {
    JS_ReportErrorNumberASCII(
        cx, GetErrorMessage, nullptr, JSMSG_NOT_EXPECTED_TYPE,
        "Debugger.isCompilableUnit", "string", InformalValueTypeName(args[0]));
    return false;
  }

  JSString* str = args[0].toString();
  size_t length = str->length();

  AutoStableStringChars chars(cx);
  if (!chars.initTwoByte(cx, str)) {
    return false;
  }

  bool result = true;

  CompileOptions options(cx);
  frontend::UsedNameTracker usedNames(cx);

  RootedScriptSourceObject sourceObject(
      cx, frontend::CreateScriptSourceObject(cx, options, Nothing()));
  if (!sourceObject) {
    return false;
  }

  JS::AutoSuppressWarningReporter suppressWarnings(cx);
  frontend::Parser<frontend::FullParseHandler, char16_t> parser(
      cx, cx->tempLifoAlloc(), options, chars.twoByteChars(), length,
      /* foldConstants = */ true, usedNames, nullptr, nullptr, sourceObject,
      frontend::ParseGoal::Script);
  if (!parser.checkOptions() || !parser.parse()) {
    // We ran into an error. If it was because we ran out of memory we report
    // it in the usual way.
    if (cx->isThrowingOutOfMemory()) {
      return false;
    }

    // If it was because we ran out of source, we return false so our caller
    // knows to try to collect more [source].
    if (parser.isUnexpectedEOF()) {
      result = false;
    }

    cx->clearPendingException();
  }

  args.rval().setBoolean(result);
  return true;
}

/* static */
bool Debugger::recordReplayProcessKind(JSContext* cx, unsigned argc,
                                       Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (mozilla::recordreplay::IsMiddleman()) {
    JSString* str = JS_NewStringCopyZ(cx, "Middleman");
    if (!str) {
      return false;
    }
    args.rval().setString(str);
  } else if (mozilla::recordreplay::IsRecordingOrReplaying()) {
    JSString* str = JS_NewStringCopyZ(cx, "RecordingReplaying");
    if (!str) {
      return false;
    }
    args.rval().setString(str);
  } else {
    args.rval().setUndefined();
  }
  return true;
}

bool Debugger::adoptDebuggeeValue(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "adoptDebuggeeValue", args, dbg);
  if (!args.requireAtLeast(cx, "Debugger.adoptDebuggeeValue", 1)) {
    return false;
  }

  RootedValue v(cx, args[0]);
  if (v.isObject()) {
    RootedObject obj(cx, &v.toObject());
    NativeObject* ndobj = ToNativeDebuggerObject(cx, &obj);
    if (!ndobj) {
      return false;
    }

    obj.set(static_cast<JSObject*>(ndobj->getPrivate()));
    v = ObjectValue(*obj);

    if (!dbg->wrapDebuggeeValue(cx, &v)) {
      return false;
    }
  }

  args.rval().set(v);
  return true;
}

class DebuggerAdoptSourceMatcher {
  JSContext* cx_;
  Debugger* dbg_;

 public:
  explicit DebuggerAdoptSourceMatcher(JSContext* cx, Debugger* dbg)
      : cx_(cx), dbg_(dbg) {}

  using ReturnType = DebuggerSource*;

  ReturnType match(HandleScriptSourceObject source) {
    if (source->compartment() == cx_->compartment()) {
      JS_ReportErrorASCII(cx_,
                          "Source is in the same compartment as this debugger");
      return nullptr;
    }
    return dbg_->wrapSource(cx_, source);
  }
  ReturnType match(Handle<WasmInstanceObject*> wasmInstance) {
    if (wasmInstance->compartment() == cx_->compartment()) {
      JS_ReportErrorASCII(
          cx_, "WasmInstance is in the same compartment as this debugger");
      return nullptr;
    }
    return dbg_->wrapWasmSource(cx_, wasmInstance);
  }
};

bool Debugger::adoptSource(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER(cx, argc, vp, "adoptSource", args, dbg);
  if (!args.requireAtLeast(cx, "Debugger.adoptSource", 1)) {
    return false;
  }

  RootedObject obj(cx, RequireObject(cx, args[0]));
  if (!obj) {
    return false;
  }

  obj = UncheckedUnwrap(obj);
  if (!obj->is<DebuggerSource>()) {
    JS_ReportErrorASCII(cx, "Argument is not a Debugger.Source");
    return false;
  }

  RootedDebuggerSource sourceObj(cx, &obj->as<DebuggerSource>());
  if (!sourceObj->getReferentRawObject()) {
    JS_ReportErrorASCII(cx, "Argument is Debugger.Source.prototype");
    return false;
  }

  Rooted<DebuggerSourceReferent> referent(cx, sourceObj->getReferent());

  DebuggerAdoptSourceMatcher matcher(cx, dbg);
  DebuggerSource* res = referent.match(matcher);
  if (!res) {
    return false;
  }

  args.rval().setObject(*res);
  return true;
}

const JSPropertySpec Debugger::properties[] = {
    JS_PSGS("enabled", Debugger::getEnabled, Debugger::setEnabled, 0),
    JS_PSGS("onDebuggerStatement", Debugger::getOnDebuggerStatement,
            Debugger::setOnDebuggerStatement, 0),
    JS_PSGS("onExceptionUnwind", Debugger::getOnExceptionUnwind,
            Debugger::setOnExceptionUnwind, 0),
    JS_PSGS("onNewScript", Debugger::getOnNewScript, Debugger::setOnNewScript,
            0),
    JS_PSGS("onNewPromise", Debugger::getOnNewPromise,
            Debugger::setOnNewPromise, 0),
    JS_PSGS("onPromiseSettled", Debugger::getOnPromiseSettled,
            Debugger::setOnPromiseSettled, 0),
    JS_PSGS("onEnterFrame", Debugger::getOnEnterFrame,
            Debugger::setOnEnterFrame, 0),
    JS_PSGS("onNewGlobalObject", Debugger::getOnNewGlobalObject,
            Debugger::setOnNewGlobalObject, 0),
    JS_PSGS("uncaughtExceptionHook", Debugger::getUncaughtExceptionHook,
            Debugger::setUncaughtExceptionHook, 0),
    JS_PSGS("allowUnobservedAsmJS", Debugger::getAllowUnobservedAsmJS,
            Debugger::setAllowUnobservedAsmJS, 0),
    JS_PSGS("collectCoverageInfo", Debugger::getCollectCoverageInfo,
            Debugger::setCollectCoverageInfo, 0),
    JS_PSG("memory", Debugger::getMemory, 0),
    JS_PS_END};

const JSFunctionSpec Debugger::methods[] = {
    JS_FN("addDebuggee", Debugger::addDebuggee, 1, 0),
    JS_FN("addAllGlobalsAsDebuggees", Debugger::addAllGlobalsAsDebuggees, 0, 0),
    JS_FN("removeDebuggee", Debugger::removeDebuggee, 1, 0),
    JS_FN("removeAllDebuggees", Debugger::removeAllDebuggees, 0, 0),
    JS_FN("hasDebuggee", Debugger::hasDebuggee, 1, 0),
    JS_FN("getDebuggees", Debugger::getDebuggees, 0, 0),
    JS_FN("getNewestFrame", Debugger::getNewestFrame, 0, 0),
    JS_FN("clearAllBreakpoints", Debugger::clearAllBreakpoints, 0, 0),
    JS_FN("findScripts", Debugger::findScripts, 1, 0),
    JS_FN("findSources", Debugger::findSources, 1, 0),
    JS_FN("findObjects", Debugger::findObjects, 1, 0),
    JS_FN("findAllGlobals", Debugger::findAllGlobals, 0, 0),
    JS_FN("makeGlobalObjectReference", Debugger::makeGlobalObjectReference, 1,
          0),
    JS_FN("adoptDebuggeeValue", Debugger::adoptDebuggeeValue, 1, 0),
    JS_FN("adoptSource", Debugger::adoptSource, 1, 0),
    JS_FS_END};

const JSFunctionSpec Debugger::static_methods[]{
    JS_FN("isCompilableUnit", Debugger::isCompilableUnit, 1, 0),
    JS_FN("recordReplayProcessKind", Debugger::recordReplayProcessKind, 0, 0),
    JS_FS_END};

DebuggerScript* Debugger::newDebuggerScript(
    JSContext* cx, Handle<DebuggerScriptReferent> referent) {
  cx->check(object.get());

  RootedObject proto(
      cx, &object->getReservedSlot(JSSLOT_DEBUG_SCRIPT_PROTO).toObject());
  MOZ_ASSERT(proto);
  RootedNativeObject debugger(cx, object);

  return DebuggerScript::create(cx, proto, referent, debugger);
}

template <typename Map>
typename Map::WrapperType* Debugger::wrapVariantReferent(
    JSContext* cx, Map& map,
    Handle<typename Map::WrapperType::ReferentVariant> referent) {
  cx->check(object);

  Handle<typename Map::ReferentType*> untaggedReferent =
      referent.template as<typename Map::ReferentType*>();
  MOZ_ASSERT(cx->compartment() != untaggedReferent->compartment());

  DependentAddPtr<Map> p(cx, map, untaggedReferent);
  if (!p) {
    typename Map::WrapperType* wrapper = newVariantWrapper(cx, referent);
    if (!wrapper) {
      return nullptr;
    }

    if (!p.add(cx, map, untaggedReferent, wrapper)) {
      NukeDebuggerWrapper(wrapper);
      return nullptr;
    }
  }

  return &p->value()->template as<typename Map::WrapperType>();
}

DebuggerScript* Debugger::wrapVariantReferent(
    JSContext* cx, Handle<DebuggerScriptReferent> referent) {
  DebuggerScript* obj;
  if (referent.is<JSScript*>()) {
    Handle<JSScript*> untaggedReferent = referent.template as<JSScript*>();
    if (untaggedReferent->maybeLazyScript()) {
      // If the JSScript has corresponding LazyScript, wrap the LazyScript
      // instead.
      //
      // This is necessary for Debugger.Script identity.  If we use both
      // JSScript and LazyScript for same single script, those 2 wrapped
      // scripts become not identical, while the referent script is
      // actually identical.
      //
      // If a script has corresponding LazyScript and JSScript, the
      // lifetime of the LazyScript is always longer than the JSScript.
      // So we can use the LazyScript as a proxy for the JSScript.
      Rooted<LazyScript*> lazyScript(cx, untaggedReferent->maybeLazyScript());
      Rooted<DebuggerScriptReferent> lazyScriptReferent(cx, lazyScript.get());

      obj = wrapVariantReferent(cx, lazyScripts, lazyScriptReferent);
      MOZ_ASSERT_IF(obj, obj->getReferent() == lazyScriptReferent);
      return obj;
    } else {
      // If the JSScript doesn't have corresponding LazyScript, the script
      // is not lazifiable, and we can safely use JSScript as referent.
      obj = wrapVariantReferent(cx, scripts, referent);
    }
  } else if (referent.is<LazyScript*>()) {
    obj = wrapVariantReferent(cx, lazyScripts, referent);
  } else {
        referent.template as<WasmInstanceObject*>();
        obj = wrapVariantReferent(cx, wasmInstanceScripts, referent);
  }
  MOZ_ASSERT_IF(obj, obj->getReferent() == referent);
  return obj;
}

DebuggerScript* Debugger::wrapScript(JSContext* cx, HandleScript script) {
  Rooted<DebuggerScriptReferent> referent(cx, script.get());
  return wrapVariantReferent(cx, referent);
}

DebuggerScript* Debugger::wrapLazyScript(JSContext* cx,
                                         Handle<LazyScript*> lazyScript) {
  Rooted<DebuggerScriptReferent> referent(cx, lazyScript.get());
  return wrapVariantReferent(cx, referent);
}

DebuggerScript* Debugger::wrapWasmScript(
    JSContext* cx, Handle<WasmInstanceObject*> wasmInstance) {
  Rooted<DebuggerScriptReferent> referent(cx, wasmInstance.get());
  return wrapVariantReferent(cx, referent);
}

DebuggerSource* Debugger::newDebuggerSource(
    JSContext* cx, Handle<DebuggerSourceReferent> referent) {
  cx->check(object.get());

  RootedObject proto(
      cx, &object->getReservedSlot(JSSLOT_DEBUG_SOURCE_PROTO).toObject());
  MOZ_ASSERT(proto);
  RootedNativeObject debugger(cx, object);
  return DebuggerSource::create(cx, proto, referent, debugger);
}

DebuggerSource* Debugger::wrapVariantReferent(
    JSContext* cx, Handle<DebuggerSourceReferent> referent) {
  DebuggerSource* obj;
  if (referent.is<ScriptSourceObject*>()) {
    obj = wrapVariantReferent(cx, sources, referent);
  } else {
    obj = wrapVariantReferent(cx, wasmInstanceSources, referent);
  }
  MOZ_ASSERT_IF(obj, obj->getReferent() == referent);
  return obj;
}

DebuggerSource* Debugger::wrapSource(JSContext* cx,
                                     HandleScriptSourceObject source) {
  Rooted<DebuggerSourceReferent> referent(cx, source.get());
  return wrapVariantReferent(cx, referent);
}

DebuggerSource* Debugger::wrapWasmSource(
    JSContext* cx, Handle<WasmInstanceObject*> wasmInstance) {
  Rooted<DebuggerSourceReferent> referent(cx, wasmInstance.get());
  return wrapVariantReferent(cx, referent);
}

bool DebugAPI::getScriptInstrumentationId(JSContext* cx, HandleObject dbgObject,
                                          HandleScript script,
                                          MutableHandleValue rval) {
  Debugger* dbg = Debugger::fromJSObject(dbgObject);
  DebuggerScript* dbgScript = dbg->wrapScript(cx, script);
  if (!dbgScript) {
    return false;
  }
  rval.set(dbgScript->getInstrumentationId());
  return true;
}

bool Debugger::observesFrame(AbstractFramePtr frame) const {
  if (frame.isWasmDebugFrame()) {
    return observesWasm(frame.wasmInstance());
  }

  return observesScript(frame.script());
}

bool Debugger::observesFrame(const FrameIter& iter) const {
  // Skip frames not yet fully initialized during their prologue.
  if (iter.isInterp() && iter.isFunctionFrame()) {
    const Value& thisVal = iter.interpFrame()->thisArgument();
    if (thisVal.isMagic() && thisVal.whyMagic() == JS_IS_CONSTRUCTING) {
      return false;
    }
  }
  if (iter.isWasm()) {
    // Skip frame of wasm instances we cannot observe.
    if (!iter.wasmDebugEnabled()) {
      return false;
    }
    return observesWasm(iter.wasmInstance());
  }
  return observesScript(iter.script());
}

bool Debugger::observesScript(JSScript* script) const {
  if (!enabled) {
    return false;
  }
  // Don't ever observe self-hosted scripts: the Debugger API can break
  // self-hosted invariants.
  return observesGlobal(&script->global()) && !script->selfHosted();
}

bool Debugger::observesWasm(wasm::Instance* instance) const {
  if (!enabled || !instance->debugEnabled()) {
    return false;
  }
  return observesGlobal(&instance->object()->global());
}

/* static */
bool Debugger::replaceFrameGuts(JSContext* cx, AbstractFramePtr from,
                                AbstractFramePtr to, ScriptFrameIter& iter) {
  auto removeFromDebuggerFramesOnExit = MakeScopeExit([&] {
    // Remove any remaining old entries on exit, as the 'from' frame will
    // be gone. This is only done in the failure case. On failure, the
    // removeToDebuggerFramesOnExit lambda below will rollback any frames
    // that were replaced, resulting in !frameMaps(to). On success, the
    // range will be empty, as all from Frame.Debugger instances will have
    // been removed.
    MOZ_ASSERT_IF(DebugAPI::inFrameMaps(to), !DebugAPI::inFrameMaps(from));
    removeFromFrameMapsAndClearBreakpointsIn(cx, from);

    // Rekey missingScopes to maintain Debugger.Environment identity and
    // forward liveScopes to point to the new frame.
    DebugEnvironments::forwardLiveFrame(cx, from, to);
  });

  // Forward live Debugger.Frame objects.
  Rooted<DebuggerFrameVector> frames(cx, DebuggerFrameVector(cx));
  if (!getDebuggerFrames(from, &frames)) {
    // An OOM here means that all Debuggers' frame maps still contain
    // entries for 'from' and no entries for 'to'. Since the 'from' frame
    // will be gone, they are removed by removeFromDebuggerFramesOnExit
    // above.
    return false;
  }

  // If during the loop below we hit an OOM, we must also rollback any of
  // the frames that were successfully replaced. For OSR frames, OOM here
  // means those frames will pop from the OSR trampoline, which does not
  // call Debugger::onLeaveFrame.
  auto removeToDebuggerFramesOnExit =
      MakeScopeExit([&] { removeFromFrameMapsAndClearBreakpointsIn(cx, to); });

  for (size_t i = 0; i < frames.length(); i++) {
    HandleDebuggerFrame frameobj = frames[i];
    Debugger* dbg = Debugger::fromChildJSObject(frameobj);

    // Update frame object's ScriptFrameIter::data pointer.
    frameobj->freeFrameIterData(cx->runtime()->defaultFreeOp());
    ScriptFrameIter::Data* data = iter.copyData();
    if (!data) {
      // An OOM here means that some Debuggers' frame maps may still
      // contain entries for 'from' and some Debuggers' frame maps may
      // also contain entries for 'to'. Thus both
      // removeFromDebuggerFramesOnExit and
      // removeToDebuggerFramesOnExit must both run.
      //
      // The current frameobj in question is still in its Debugger's
      // frame map keyed by 'from', so it will be covered by
      // removeFromDebuggerFramesOnExit.
      return false;
    }
    frameobj->setFrameIterData(data);

    // Remove old frame.
    dbg->frames.remove(from);

    // Add the frame object with |to| as key.
    if (!dbg->frames.putNew(to, frameobj)) {
      // This OOM is subtle. At this point, both
      // removeFromDebuggerFramesOnExit and removeToDebuggerFramesOnExit
      // must both run for the same reason given above.
      //
      // The difference is that the current frameobj is no longer in its
      // Debugger's frame map, so it will not be cleaned up by neither
      // lambda. Manually clean it up here.
      FreeOp* fop = cx->runtime()->defaultFreeOp();
      frameobj->freeFrameIterData(fop);
      frameobj->maybeDecrementFrameScriptStepperCount(fop, to);

      ReportOutOfMemory(cx);
      return false;
    }
  }

  // All frames successfuly replaced, cancel the rollback.
  removeToDebuggerFramesOnExit.release();

  return true;
}

/* static */
bool DebugAPI::inFrameMaps(AbstractFramePtr frame) {
  bool foundAny = false;
  Debugger::forEachDebuggerFrame(frame,
                                 [&](DebuggerFrame* frameobj) {
                                   foundAny = true;
                                 });
  return foundAny;
}

/* static */
void Debugger::removeFromFrameMapsAndClearBreakpointsIn(JSContext* cx,
                                                        AbstractFramePtr frame,
                                                        bool suspending) {
  forEachDebuggerFrame(frame, [&](DebuggerFrame* frameobj) {
    FreeOp* fop = cx->runtime()->defaultFreeOp();
    frameobj->freeFrameIterData(fop);

    Debugger* dbg = Debugger::fromChildJSObject(frameobj);
    dbg->frames.remove(frame);

    if (frameobj->hasGenerator()) {
      // If this is a generator's final pop, remove its entry from
      // generatorFrames. Such an entry exists if and only if the
      // Debugger.Frame's generator has been set.
      if (!suspending) {
        // Terminally exiting a generator.
#if DEBUG
        AbstractGeneratorObject& genObj = frameobj->unwrappedGenerator();
        GeneratorWeakMap::Ptr p = dbg->generatorFrames.lookup(&genObj);
        MOZ_ASSERT(p);
        MOZ_ASSERT(p->value() == frameobj);
#endif
        frameobj->clearGenerator(fop, dbg);
      }
    } else {
      frameobj->maybeDecrementFrameScriptStepperCount(fop, frame);
    }
  });

  // If this is an eval frame, then from the debugger's perspective the
  // script is about to be destroyed. Remove any breakpoints in it.
  if (frame.isEvalFrame()) {
    RootedScript script(cx, frame.script());
    DebugScript::clearBreakpointsIn(cx->runtime()->defaultFreeOp(), script,
                                    nullptr, nullptr);
  }
}

/* static */
bool DebugAPI::handleBaselineOsr(JSContext* cx, InterpreterFrame* from,
                                 jit::BaselineFrame* to) {
  ScriptFrameIter iter(cx);
  MOZ_ASSERT(iter.abstractFramePtr() == to);
  return Debugger::replaceFrameGuts(cx, from, to, iter);
}

/* static */
bool DebugAPI::handleIonBailout(JSContext* cx, jit::RematerializedFrame* from,
                                jit::BaselineFrame* to) {
  // When we return to a bailed-out Ion real frame, we must update all
  // Debugger.Frames that refer to its inline frames. However, since we
  // can't pop individual inline frames off the stack (we can only pop the
  // real frame that contains them all, as a unit), we cannot assume that
  // the frame we're dealing with is the top frame. Advance the iterator
  // across any inlined frames younger than |to|, the baseline frame
  // reconstructed during bailout from the Ion frame corresponding to
  // |from|.
  ScriptFrameIter iter(cx);
  while (iter.abstractFramePtr() != to) {
    ++iter;
  }
  return Debugger::replaceFrameGuts(cx, from, to, iter);
}

/* static */
void DebugAPI::handleUnrecoverableIonBailoutError(
    JSContext* cx, jit::RematerializedFrame* frame) {
  // Ion bailout can fail due to overrecursion. In such cases we cannot
  // honor any further Debugger hooks on the frame, and need to ensure that
  // its Debugger.Frame entry is cleaned up.
  Debugger::removeFromFrameMapsAndClearBreakpointsIn(cx, frame);
}

/* static */
void DebugAPI::propagateForcedReturn(JSContext* cx, AbstractFramePtr frame,
                                     HandleValue rval) {
  // Invoking the interrupt handler is considered a step and invokes the
  // youngest frame's onStep handler, if any. However, we cannot handle
  // { return: ... } resumption values straightforwardly from the interrupt
  // handler. Instead, we set the intended return value in the frame's rval
  // slot and set the propagating-forced-return flag on the JSContext.
  //
  // The interrupt handler then returns false with no exception set,
  // signaling an uncatchable exception. In the exception handlers, we then
  // check for the special propagating-forced-return flag.
  MOZ_ASSERT(!cx->isExceptionPending());
  cx->setPropagatingForcedReturn();
  frame.setReturnValue(rval);
}

/*** JS::dbg::Builder *******************************************************/

Builder::Builder(JSContext* cx, js::Debugger* debugger)
    : debuggerObject(cx, debugger->toJSObject().get()), debugger(debugger) {}

#if DEBUG
void Builder::assertBuilt(JSObject* obj) {
  // We can't use assertSameCompartment here, because that is always keyed to
  // some JSContext's current compartment, whereas BuiltThings can be
  // constructed and assigned to without respect to any particular context;
  // the only constraint is that they should be in their debugger's compartment.
  MOZ_ASSERT_IF(obj, debuggerObject->compartment() == obj->compartment());
}
#endif

bool Builder::Object::definePropertyToTrusted(JSContext* cx, const char* name,
                                              JS::MutableHandleValue trusted) {
  // We should have checked for false Objects before calling this.
  MOZ_ASSERT(value);

  JSAtom* atom = Atomize(cx, name, strlen(name));
  if (!atom) {
    return false;
  }
  RootedId id(cx, AtomToId(atom));

  return DefineDataProperty(cx, value, id, trusted);
}

bool Builder::Object::defineProperty(JSContext* cx, const char* name,
                                     JS::HandleValue propval_) {
  AutoRealm ar(cx, debuggerObject());

  RootedValue propval(cx, propval_);
  if (!debugger()->wrapDebuggeeValue(cx, &propval)) {
    return false;
  }

  return definePropertyToTrusted(cx, name, &propval);
}

bool Builder::Object::defineProperty(JSContext* cx, const char* name,
                                     JS::HandleObject propval_) {
  RootedValue propval(cx, ObjectOrNullValue(propval_));
  return defineProperty(cx, name, propval);
}

bool Builder::Object::defineProperty(JSContext* cx, const char* name,
                                     Builder::Object& propval_) {
  AutoRealm ar(cx, debuggerObject());

  RootedValue propval(cx, ObjectOrNullValue(propval_.value));
  return definePropertyToTrusted(cx, name, &propval);
}

Builder::Object Builder::newObject(JSContext* cx) {
  AutoRealm ar(cx, debuggerObject);

  RootedPlainObject obj(cx, NewBuiltinClassInstance<PlainObject>(cx));

  // If the allocation failed, this will return a false Object, as the spec
  // promises.
  return Object(cx, *this, obj);
}

/*** JS::dbg::AutoEntryMonitor **********************************************/

AutoEntryMonitor::AutoEntryMonitor(JSContext* cx)
    : cx_(cx), savedMonitor_(cx->entryMonitor) {
  cx->entryMonitor = this;
}

AutoEntryMonitor::~AutoEntryMonitor() { cx_->entryMonitor = savedMonitor_; }

/*** Glue *******************************************************************/

extern JS_PUBLIC_API bool JS_DefineDebuggerObject(JSContext* cx,
                                                  HandleObject obj) {
  RootedNativeObject debugCtor(cx), debugProto(cx), frameProto(cx),
      scriptProto(cx), sourceProto(cx), objectProto(cx), envProto(cx),
      memoryProto(cx);
  RootedObject debuggeeWouldRunProto(cx);
  RootedValue debuggeeWouldRunCtor(cx);
  Handle<GlobalObject*> global = obj.as<GlobalObject>();

  debugProto =
      InitClass(cx, global, nullptr, &Debugger::class_, Debugger::construct, 1,
                Debugger::properties, Debugger::methods, nullptr,
                Debugger::static_methods, debugCtor.address());
  if (!debugProto) {
    return false;
  }

  frameProto = DebuggerFrame::initClass(cx, global, debugCtor);
  if (!frameProto) {
    return false;
  }

  scriptProto = DebuggerScript::initClass(cx, global, debugCtor);
  if (!scriptProto) {
    return false;
  }

  sourceProto = DebuggerSource::initClass(cx, global, debugCtor);
  if (!sourceProto) {
    return false;
  }

  objectProto = DebuggerObject::initClass(cx, global, debugCtor);
  if (!objectProto) {
    return false;
  }

  envProto = DebuggerEnvironment::initClass(cx, global, debugCtor);
  if (!envProto) {
    return false;
  }

  memoryProto =
      InitClass(cx, debugCtor, nullptr, &DebuggerMemory::class_,
                DebuggerMemory::construct, 0, DebuggerMemory::properties,
                DebuggerMemory::methods, nullptr, nullptr);
  if (!memoryProto) {
    return false;
  }

  debuggeeWouldRunProto = GlobalObject::getOrCreateCustomErrorPrototype(
      cx, global, JSEXN_DEBUGGEEWOULDRUN);
  if (!debuggeeWouldRunProto) {
    return false;
  }
  debuggeeWouldRunCtor = global->getConstructor(JSProto_DebuggeeWouldRun);
  RootedId debuggeeWouldRunId(
      cx, NameToId(ClassName(JSProto_DebuggeeWouldRun, cx)));
  if (!DefineDataProperty(cx, debugCtor, debuggeeWouldRunId,
                          debuggeeWouldRunCtor, 0)) {
    return false;
  }

  debugProto->setReservedSlot(Debugger::JSSLOT_DEBUG_FRAME_PROTO,
                              ObjectValue(*frameProto));
  debugProto->setReservedSlot(Debugger::JSSLOT_DEBUG_OBJECT_PROTO,
                              ObjectValue(*objectProto));
  debugProto->setReservedSlot(Debugger::JSSLOT_DEBUG_SCRIPT_PROTO,
                              ObjectValue(*scriptProto));
  debugProto->setReservedSlot(Debugger::JSSLOT_DEBUG_SOURCE_PROTO,
                              ObjectValue(*sourceProto));
  debugProto->setReservedSlot(Debugger::JSSLOT_DEBUG_ENV_PROTO,
                              ObjectValue(*envProto));
  debugProto->setReservedSlot(Debugger::JSSLOT_DEBUG_MEMORY_PROTO,
                              ObjectValue(*memoryProto));
  return true;
}

JS_PUBLIC_API bool JS::dbg::IsDebugger(JSObject& obj) {
  /* We only care about debugger objects, so CheckedUnwrapStatic is OK. */
  JSObject* unwrapped = CheckedUnwrapStatic(&obj);
  return unwrapped && unwrapped->getClass() == &Debugger::class_ &&
         js::Debugger::fromJSObject(unwrapped) != nullptr;
}

JS_PUBLIC_API bool JS::dbg::GetDebuggeeGlobals(
    JSContext* cx, JSObject& dbgObj, MutableHandleObjectVector vector) {
  MOZ_ASSERT(IsDebugger(dbgObj));
  /* Since we know we have a debugger object, CheckedUnwrapStatic is fine. */
  js::Debugger* dbg = js::Debugger::fromJSObject(CheckedUnwrapStatic(&dbgObj));

  if (!vector.reserve(vector.length() + dbg->debuggees.count())) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  for (WeakGlobalObjectSet::Range r = dbg->allDebuggees(); !r.empty();
       r.popFront()) {
    vector.infallibleAppend(static_cast<JSObject*>(r.front()));
  }

  return true;
}

#ifdef DEBUG
/* static */
bool Debugger::isDebuggerCrossCompartmentEdge(JSObject* obj,
                                              const gc::Cell* target) {
  MOZ_ASSERT(target);

  const gc::Cell* referent = nullptr;
  if (obj->is<DebuggerScript>()) {
    referent = obj->as<DebuggerScript>().getReferentCell();
  } else if (obj->is<DebuggerSource>()) {
    referent = obj->as<DebuggerSource>().getReferentRawObject();
  } else if (obj->is<DebuggerObject>()) {
    referent = static_cast<gc::Cell*>(obj->as<DebuggerObject>().getPrivate());
  } else if (obj->is<DebuggerEnvironment>()) {
    referent =
        static_cast<gc::Cell*>(obj->as<DebuggerEnvironment>().getPrivate());
  }

  return referent == target;
}

static void CheckDebuggeeThingRealm(Realm* realm, bool invisibleOk) {
  MOZ_ASSERT(!realm->creationOptions().mergeable());
  MOZ_ASSERT_IF(!invisibleOk, !realm->creationOptions().invisibleToDebugger());
}

void js::CheckDebuggeeThing(JSScript* script, bool invisibleOk) {
  CheckDebuggeeThingRealm(script->realm(), invisibleOk);
}

void js::CheckDebuggeeThing(LazyScript* script, bool invisibleOk) {
  CheckDebuggeeThingRealm(script->realm(), invisibleOk);
}

void js::CheckDebuggeeThing(JSObject* obj, bool invisibleOk) {
  if (Realm* realm = JS::GetObjectRealmOrNull(obj)) {
    CheckDebuggeeThingRealm(realm, invisibleOk);
  }
}
#endif  // DEBUG

/*** JS::dbg::GarbageCollectionEvent ****************************************/

namespace JS {
namespace dbg {

/* static */ GarbageCollectionEvent::Ptr GarbageCollectionEvent::Create(
    JSRuntime* rt, ::js::gcstats::Statistics& stats, uint64_t gcNumber) {
  auto data = MakeUnique<GarbageCollectionEvent>(gcNumber);
  if (!data) {
    return nullptr;
  }

  data->nonincrementalReason = stats.nonincrementalReason();

  for (auto& slice : stats.slices()) {
    if (!data->reason) {
      // There is only one GC reason for the whole cycle, but for legacy
      // reasons this data is stored and replicated on each slice. Each
      // slice used to have its own GCReason, but now they are all the
      // same.
      data->reason = ExplainGCReason(slice.reason);
      MOZ_ASSERT(data->reason);
    }

    if (!data->collections.growBy(1)) {
      return nullptr;
    }

    data->collections.back().startTimestamp = slice.start;
    data->collections.back().endTimestamp = slice.end;
  }

  return data;
}

static bool DefineStringProperty(JSContext* cx, HandleObject obj,
                                 PropertyName* propName, const char* strVal) {
  RootedValue val(cx, UndefinedValue());
  if (strVal) {
    JSAtom* atomized = Atomize(cx, strVal, strlen(strVal));
    if (!atomized) {
      return false;
    }
    val = StringValue(atomized);
  }
  return DefineDataProperty(cx, obj, propName, val);
}

JSObject* GarbageCollectionEvent::toJSObject(JSContext* cx) const {
  RootedObject obj(cx, NewBuiltinClassInstance<PlainObject>(cx));
  RootedValue gcCycleNumberVal(cx, NumberValue(majorGCNumber_));
  if (!obj ||
      !DefineStringProperty(cx, obj, cx->names().nonincrementalReason,
                            nonincrementalReason) ||
      !DefineStringProperty(cx, obj, cx->names().reason, reason) ||
      !DefineDataProperty(cx, obj, cx->names().gcCycleNumber,
                          gcCycleNumberVal)) {
    return nullptr;
  }

  RootedArrayObject slicesArray(cx, NewDenseEmptyArray(cx));
  if (!slicesArray) {
    return nullptr;
  }

  TimeStamp originTime = TimeStamp::ProcessCreation();

  size_t idx = 0;
  for (auto range = collections.all(); !range.empty(); range.popFront()) {
    RootedPlainObject collectionObj(cx,
                                    NewBuiltinClassInstance<PlainObject>(cx));
    if (!collectionObj) {
      return nullptr;
    }

    RootedValue start(cx), end(cx);
    start = NumberValue(
        (range.front().startTimestamp - originTime).ToMilliseconds());
    end =
        NumberValue((range.front().endTimestamp - originTime).ToMilliseconds());
    if (!DefineDataProperty(cx, collectionObj, cx->names().startTimestamp,
                            start) ||
        !DefineDataProperty(cx, collectionObj, cx->names().endTimestamp, end)) {
      return nullptr;
    }

    RootedValue collectionVal(cx, ObjectValue(*collectionObj));
    if (!DefineDataElement(cx, slicesArray, idx++, collectionVal)) {
      return nullptr;
    }
  }

  RootedValue slicesValue(cx, ObjectValue(*slicesArray));
  if (!DefineDataProperty(cx, obj, cx->names().collections, slicesValue)) {
    return nullptr;
  }

  return obj;
}

JS_PUBLIC_API bool FireOnGarbageCollectionHookRequired(JSContext* cx) {
  AutoCheckCannotGC noGC;

  for (Debugger* dbg : cx->runtime()->debuggerList()) {
    if (dbg->enabled && dbg->observedGC(cx->runtime()->gc.majorGCCount()) &&
        dbg->getHook(Debugger::OnGarbageCollection)) {
      return true;
    }
  }

  return false;
}

JS_PUBLIC_API bool FireOnGarbageCollectionHook(
    JSContext* cx, JS::dbg::GarbageCollectionEvent::Ptr&& data) {
  RootedObjectVector triggered(cx);

  {
    // We had better not GC (and potentially get a dangling Debugger
    // pointer) while finding all Debuggers observing a debuggee that
    // participated in this GC.
    AutoCheckCannotGC noGC;

    for (Debugger* dbg : cx->runtime()->debuggerList()) {
      if (dbg->enabled && dbg->observedGC(data->majorGCNumber()) &&
          dbg->getHook(Debugger::OnGarbageCollection)) {
        if (!triggered.append(dbg->object)) {
          JS_ReportOutOfMemory(cx);
          return false;
        }
      }
    }
  }

  for (; !triggered.empty(); triggered.popBack()) {
    Debugger* dbg = Debugger::fromJSObject(triggered.back());
    dbg->fireOnGarbageCollectionHook(cx, data);
    MOZ_ASSERT(!cx->isExceptionPending());
  }

  return true;
}

}  // namespace dbg
}  // namespace JS
