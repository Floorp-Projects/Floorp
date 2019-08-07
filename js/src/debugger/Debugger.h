/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef debugger_Debugger_h
#define debugger_Debugger_h

#include "mozilla/DoublyLinkedList.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Range.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Vector.h"

#include "debugger/DebugAPI.h"
#include "ds/TraceableFifo.h"
#include "gc/Barrier.h"
#include "gc/WeakMap.h"
#include "js/Debug.h"
#include "js/GCVariant.h"
#include "js/HashTable.h"
#include "js/Promise.h"
#include "js/Result.h"
#include "js/RootingAPI.h"
#include "js/Utility.h"
#include "js/Wrapper.h"
#include "proxy/DeadObjectProxy.h"
#include "vm/GeneratorObject.h"
#include "vm/Realm.h"
#include "vm/SavedStacks.h"
#include "vm/Stack.h"

/*
 * Windows 3.x used a cooperative multitasking model, with a Yield macro that
 * let you relinquish control to other cooperative threads. Microsoft replaced
 * it with an empty macro long ago. We should be free to use it in our code.
 */
#undef Yield

namespace js {

class Breakpoint;
class DebuggerFrame;
class DebuggerScript;
class DebuggerMemory;
class ScriptedOnStepHandler;
class ScriptedOnPopHandler;
class WasmInstanceObject;

/**
 * A completion value, describing how some sort of JavaScript evaluation
 * completed. This is used to tell an onPop handler what's going on with the
 * frame, and to report the outcome of call, apply, setProperty, and getProperty
 * operations.
 *
 * Local variables of type Completion should be held in Rooted locations,
 * and passed using Handle and MutableHandle.
 */
class Completion {
 public:
  struct Return {
    explicit Return(const Value& value) : value(value) {}
    Value value;

    void trace(JSTracer* trc) {
      JS::UnsafeTraceRoot(trc, &value, "js::Completion::Return::value");
    }
  };

  struct Throw {
    Throw(const Value& exception, SavedFrame* stack)
        : exception(exception), stack(stack) {}
    Value exception;
    SavedFrame* stack;

    void trace(JSTracer* trc) {
      JS::UnsafeTraceRoot(trc, &exception, "js::Completion::Throw::exception");
      JS::UnsafeTraceRoot(trc, &stack, "js::Completion::Throw::stack");
    }
  };

  struct Terminate {
    void trace(JSTracer* trc) {}
  };

  struct InitialYield {
    explicit InitialYield(AbstractGeneratorObject* generatorObject)
        : generatorObject(generatorObject) {}
    AbstractGeneratorObject* generatorObject;

    void trace(JSTracer* trc) {
      JS::UnsafeTraceRoot(trc, &generatorObject,
                          "js::Completion::InitialYield::generatorObject");
    }
  };

  struct Yield {
    Yield(AbstractGeneratorObject* generatorObject, const Value& iteratorResult)
        : generatorObject(generatorObject), iteratorResult(iteratorResult) {}
    AbstractGeneratorObject* generatorObject;
    Value iteratorResult;

    void trace(JSTracer* trc) {
      JS::UnsafeTraceRoot(trc, &generatorObject,
                          "js::Completion::Yield::generatorObject");
      JS::UnsafeTraceRoot(trc, &iteratorResult,
                          "js::Completion::Yield::iteratorResult");
    }
  };

  struct Await {
    Await(AbstractGeneratorObject* generatorObject, const Value& awaitee)
        : generatorObject(generatorObject), awaitee(awaitee) {}
    AbstractGeneratorObject* generatorObject;
    Value awaitee;

    void trace(JSTracer* trc) {
      JS::UnsafeTraceRoot(trc, &generatorObject,
                          "js::Completion::Await::generatorObject");
      JS::UnsafeTraceRoot(trc, &awaitee, "js::Completion::Await::awaitee");
    }
  };

  // The JS::Result macros want to assign to an existing variable, so having a
  // default constructor is handy.
  Completion() : variant(Terminate()) {}

  // Construct a completion from a specific variant.
  //
  // Unfortunately, using a template here would prevent the implicit definitions
  // of the copy and move constructor and assignment operators, which is icky.
  explicit Completion(Return&& variant)
      : variant(std::forward<Return>(variant)) {}
  explicit Completion(Throw&& variant)
      : variant(std::forward<Throw>(variant)) {}
  explicit Completion(Terminate&& variant)
      : variant(std::forward<Terminate>(variant)) {}
  explicit Completion(InitialYield&& variant)
      : variant(std::forward<InitialYield>(variant)) {}
  explicit Completion(Yield&& variant)
      : variant(std::forward<Yield>(variant)) {}
  explicit Completion(Await&& variant)
      : variant(std::forward<Await>(variant)) {}

  // Capture a JavaScript operation result as a Completion value. This clears
  // any exception and stack from cx, taking ownership of them itself.
  static Completion fromJSResult(JSContext* cx, bool ok, const Value& rv);

  // Construct a completion given an AbstractFramePtr that is being popped. This
  // clears any exception and stack from cx, taking ownership of them itself.
  static Completion fromJSFramePop(JSContext* cx, AbstractFramePtr frame,
                                   const jsbytecode* pc, bool ok);

  template <typename V>
  bool is() const {
    return variant.template is<V>();
  }

  template <typename V>
  V& as() {
    return variant.template as<V>();
  }

  template <typename V>
  const V& as() const {
    return variant.template as<V>();
  }

  void trace(JSTracer* trc);

  /* True if this completion is a suspension of a generator or async call. */
  bool suspending() const {
    return variant.is<InitialYield>() || variant.is<Yield>() ||
           variant.is<Await>();
  }

  /*
   * If this completion is a suspension of a generator or async call, return the
   * call's generator object, nullptr otherwise.
   */
  AbstractGeneratorObject* maybeGeneratorObject() const;

  /* Set `result` to a Debugger API completion value describing this completion.
   */
  bool buildCompletionValue(JSContext* cx, Debugger* dbg,
                            MutableHandleValue result) const;

  /*
   * Set `resumeMode`, `value`, and `exnStack` to values describing this
   * completion.
   */
  void toResumeMode(ResumeMode& resumeMode, MutableHandleValue value,
                    MutableHandleSavedFrame exnStack) const;
  /*
   * Given a `ResumeMode` and value (typically derived from a resumption value
   * returned by a Debugger hook), update this completion as requested.
   */
  void updateForNextHandler(ResumeMode resumeMode, HandleValue value);

 private:
  using Variant =
      mozilla::Variant<Return, Throw, Terminate, InitialYield, Yield, Await>;
  struct BuildValueMatcher;
  struct ToResumeModeMatcher;

  Variant variant;
};

typedef HashSet<WeakHeapPtrGlobalObject,
                MovableCellHasher<WeakHeapPtrGlobalObject>, ZoneAllocPolicy>
    WeakGlobalObjectSet;

#ifdef DEBUG
extern void CheckDebuggeeThing(JSScript* script, bool invisibleOk);

extern void CheckDebuggeeThing(LazyScript* script, bool invisibleOk);

extern void CheckDebuggeeThing(JSObject* obj, bool invisibleOk);
#endif

/*
 * [SMDOC] Cross-compartment weakmap entries for Debugger API objects
 *
 * The Debugger API creates objects like Debugger.Object, Debugger.Script,
 * Debugger.Environment, etc. to refer to things in the debuggee. Each Debugger
 * gets at most one Debugger.Mumble for each referent: Debugger.Mumbles are
 * unique per referent per Debugger. This is accomplished by storing the
 * debugger objects in a DebuggerWeakMap, using the debuggee thing as the key.
 *
 * Since a Debugger and its debuggee must be in different compartments, a
 * Debugger.Mumble's pointer to its referent is a cross-compartment edge, from
 * the debugger's compartment into the debuggee compartment. Like any other sort
 * of cross-compartment edge, the GC needs to be able to find all of these edges
 * readily. The GC therefore consults the debugger's weakmap tables as
 * necessary.  This allows the garbage collector to easily find edges between
 * debuggee object compartments and debugger compartments when calculating the
 * zone sweep groups.
 *
 * The current implementation results in all debuggee object compartments being
 * swept in the same group as the debugger. This is a conservative approach, and
 * compartments may be unnecessarily grouped. However this results in a simpler
 * and faster implementation.
 */

/*
 * A weakmap from GC thing keys to JSObject values that supports the keys being
 * in different compartments to the values. All values must be in the same
 * compartment.
 *
 * If InvisibleKeysOk is true, then the map can have keys in invisible-to-
 * debugger compartments. If it is false, we assert that such entries are never
 * created.
 *
 * Note that keys in these weakmaps can be in any compartment, debuggee or not,
 * because they are not deleted when a compartment is no longer a debuggee: the
 * values need to maintain object identity across add/remove/add
 * transitions. (Frames are an exception to the rule. Existing Debugger.Frame
 * objects are killed if their realm is removed as a debugger; if the realm
 * beacomes a debuggee again later, new Frame objects are created.)
 */
template <class UnbarrieredKey, bool InvisibleKeysOk = false>
class DebuggerWeakMap
    : private WeakMap<HeapPtr<UnbarrieredKey>, HeapPtr<JSObject*>> {
 private:
  typedef HeapPtr<UnbarrieredKey> Key;
  typedef HeapPtr<JSObject*> Value;

  typedef HashMap<JS::Zone*, uintptr_t, DefaultHasher<JS::Zone*>,
                  ZoneAllocPolicy>
      CountMap;

  JS::Compartment* compartment;

 public:
  typedef WeakMap<Key, Value> Base;

  explicit DebuggerWeakMap(JSContext* cx)
      : Base(cx), compartment(cx->compartment()) {}

 public:
  // Expose those parts of HashMap public interface that are used by Debugger
  // methods.

  using Entry = typename Base::Entry;
  using Ptr = typename Base::Ptr;
  using AddPtr = typename Base::AddPtr;
  using Range = typename Base::Range;
  using Lookup = typename Base::Lookup;

  // Expose WeakMap public interface.

  using Base::all;
  using Base::has;
  using Base::lookup;
  using Base::lookupForAdd;
  using Base::remove;
  using Base::trace;
#ifdef DEBUG
  using Base::hasEntry;
#endif

  class Enum : public Base::Enum {
   public:
    explicit Enum(DebuggerWeakMap& map) : Base::Enum(map) {}
  };

  template <typename KeyInput, typename ValueInput>
  bool relookupOrAdd(AddPtr& p, const KeyInput& k, const ValueInput& v) {
    MOZ_ASSERT(v->compartment() == this->compartment);
#ifdef DEBUG
    CheckDebuggeeThing(k, InvisibleKeysOk);
#endif
    MOZ_ASSERT(!Base::has(k));
    bool ok = Base::relookupOrAdd(p, k, v);
    return ok;
  }

 public:
  template <void(traceValueEdges)(JSTracer*, JSObject*)>
  void traceCrossCompartmentEdges(JSTracer* tracer) {
    for (Enum e(*this); !e.empty(); e.popFront()) {
      traceValueEdges(tracer, e.front().value());
      Key key = e.front().key();
      TraceEdge(tracer, &key, "Debugger WeakMap key");
      if (key != e.front().key()) {
        e.rekeyFront(key);
      }
      key.unsafeSet(nullptr);
    }
  }

  bool findSweepGroupEdges(JS::Zone* debuggerZone);

 private:
#ifdef JS_GC_ZEAL
  // Let the weak map marking verifier know that this map can
  // contain keys in other zones.
  virtual bool allowKeysInOtherZones() const override { return true; }
#endif
};

class LeaveDebuggeeNoExecute;

class MOZ_RAII EvalOptions {
  JS::UniqueChars filename_;
  unsigned lineno_ = 1;

 public:
  EvalOptions() = default;
  ~EvalOptions() = default;
  const char* filename() const { return filename_.get(); }
  unsigned lineno() const { return lineno_; }
  MOZ_MUST_USE bool setFilename(JSContext* cx, const char* filename);
  void setLineno(unsigned lineno) { lineno_ = lineno; }
};

/*
 * Env is the type of what ES5 calls "lexical environments" (runtime activations
 * of lexical scopes). This is currently just JSObject, and is implemented by
 * CallObject, LexicalEnvironmentObject, and WithEnvironmentObject, among
 * others--but environments and objects are really two different concepts.
 */
typedef JSObject Env;

// The referent of a Debugger.Script.
//
// - For most scripts, we point at their LazyScript, because that address
//   doesn't change as the script is lazified/delazified.
//
// - For scripts that cannot be lazified, and thus have no LazyScript, we point
//   directly to their JSScript.
//
// - For Web Assembly instances for which we are presenting a script-like
//   interface, we point at their WasmInstanceObject.
//
// The DebuggerScript object itself simply stores a Cell* in its private
// pointer, but when we're working with that pointer in C++ code, we'd rather
// not pass around a Cell* and be constantly asserting that, yes, this really
// does point to something okay. Instead, we immediately build an instance of
// this type from the Cell* and use that instead, so we can benefit from
// Variant's static checks.
typedef mozilla::Variant<JSScript*, LazyScript*, WasmInstanceObject*>
    DebuggerScriptReferent;

// The referent of a Debugger.Source.
//
// - For most sources, this is a ScriptSourceObject.
//
// - For Web Assembly instances for which we are presenting a source-like
//   interface, we point at their WasmInstanceObject.
//
// The DebuggerSource object actually simply stores a Cell* in its private
// pointer. See the comments for DebuggerScriptReferent for the rationale for
// this type.
typedef mozilla::Variant<ScriptSourceObject*, WasmInstanceObject*>
    DebuggerSourceReferent;

class Debugger : private mozilla::LinkedListElement<Debugger> {
  friend class DebugAPI;
  friend class Breakpoint;
  friend class DebuggerFrame;
  friend class DebuggerMemory;
  friend struct JSRuntime::GlobalObjectWatchersLinkAccess<Debugger>;
  friend class SavedStacks;
  friend class ScriptedOnStepHandler;
  friend class ScriptedOnPopHandler;
  friend class mozilla::LinkedListElement<Debugger>;
  friend class mozilla::LinkedList<Debugger>;
  friend bool(::JS_DefineDebuggerObject)(JSContext* cx, JS::HandleObject obj);
  friend bool(::JS::dbg::IsDebugger)(JSObject&);
  friend bool(::JS::dbg::GetDebuggeeGlobals)(JSContext*, JSObject&,
                                             MutableHandleObjectVector);
  friend bool JS::dbg::FireOnGarbageCollectionHookRequired(JSContext* cx);
  friend bool JS::dbg::FireOnGarbageCollectionHook(
      JSContext* cx, JS::dbg::GarbageCollectionEvent::Ptr&& data);

 public:
  enum Hook {
    OnDebuggerStatement,
    OnExceptionUnwind,
    OnNewScript,
    OnEnterFrame,
    OnNewGlobalObject,
    OnNewPromise,
    OnPromiseSettled,
    OnGarbageCollection,
    HookCount
  };
  enum {
    JSSLOT_DEBUG_PROTO_START,
    JSSLOT_DEBUG_FRAME_PROTO = JSSLOT_DEBUG_PROTO_START,
    JSSLOT_DEBUG_ENV_PROTO,
    JSSLOT_DEBUG_OBJECT_PROTO,
    JSSLOT_DEBUG_SCRIPT_PROTO,
    JSSLOT_DEBUG_SOURCE_PROTO,
    JSSLOT_DEBUG_MEMORY_PROTO,
    JSSLOT_DEBUG_PROTO_STOP,
    JSSLOT_DEBUG_HOOK_START = JSSLOT_DEBUG_PROTO_STOP,
    JSSLOT_DEBUG_HOOK_STOP = JSSLOT_DEBUG_HOOK_START + HookCount,
    JSSLOT_DEBUG_MEMORY_INSTANCE = JSSLOT_DEBUG_HOOK_STOP,
    JSSLOT_DEBUG_COUNT
  };

  // Bring DebugAPI::IsObserving into the Debugger namespace.
  using IsObserving = DebugAPI::IsObserving;
  static const IsObserving Observing = DebugAPI::Observing;
  static const IsObserving NotObserving = DebugAPI::NotObserving;

  // Return true if the given realm is a debuggee of this debugger,
  // false otherwise.
  bool isDebuggeeUnbarriered(const Realm* realm) const;

  // Return true if this Debugger observed a debuggee that participated in the
  // GC identified by the given GC number. Return false otherwise.
  // May return false negatives if we have hit OOM.
  bool observedGC(uint64_t majorGCNumber) const {
    return observedGCs.has(majorGCNumber);
  }

  // Notify this Debugger that one or more of its debuggees is participating
  // in the GC identified by the given GC number.
  bool debuggeeIsBeingCollected(uint64_t majorGCNumber) {
    return observedGCs.put(majorGCNumber);
  }

  static SavedFrame* getObjectAllocationSite(JSObject& obj);

  struct AllocationsLogEntry {
    AllocationsLogEntry(HandleObject frame, mozilla::TimeStamp when,
                        const char* className, HandleAtom ctorName, size_t size,
                        bool inNursery)
        : frame(frame),
          when(when),
          className(className),
          ctorName(ctorName),
          size(size),
          inNursery(inNursery) {
      MOZ_ASSERT_IF(frame, UncheckedUnwrap(frame)->is<SavedFrame>() ||
                               IsDeadProxyObject(frame));
    }

    HeapPtr<JSObject*> frame;
    mozilla::TimeStamp when;
    const char* className;
    HeapPtr<JSAtom*> ctorName;
    size_t size;
    bool inNursery;

    void trace(JSTracer* trc) {
      TraceNullableEdge(trc, &frame, "Debugger::AllocationsLogEntry::frame");
      TraceNullableEdge(trc, &ctorName,
                        "Debugger::AllocationsLogEntry::ctorName");
    }
  };

  // Barrier methods so we can have WeakHeapPtr<Debugger*>.
  static void readBarrier(Debugger* dbg) {
    InternalBarrierMethods<JSObject*>::readBarrier(dbg->object);
  }
  static void writeBarrierPost(Debugger** vp, Debugger* prev, Debugger* next) {}
#ifdef DEBUG
  static void assertThingIsNotGray(Debugger* dbg) { return; }
#endif

 private:
  GCPtrNativeObject object; /* The Debugger object. Strong reference. */
  WeakGlobalObjectSet
      debuggees; /* Debuggee globals. Cross-compartment weak references. */
  JS::ZoneSet debuggeeZones; /* Set of zones that we have debuggees in. */
  js::GCPtrObject uncaughtExceptionHook; /* Strong reference. */
  bool allowUnobservedAsmJS;

  // Whether to enable code coverage on the Debuggee.
  bool collectCoverageInfo;

  template <typename T>
  struct DebuggerLinkAccess {
    static mozilla::DoublyLinkedListElement<T>& Get(T* aThis) {
      return aThis->debuggerLink;
    }
  };

  // List of all js::Breakpoints in this debugger.
  using BreakpointList =
      mozilla::DoublyLinkedList<js::Breakpoint,
                                DebuggerLinkAccess<js::Breakpoint>>;
  BreakpointList breakpoints;

  // The set of GC numbers for which one or more of this Debugger's observed
  // debuggees participated in.
  using GCNumberSet =
      HashSet<uint64_t, DefaultHasher<uint64_t>, ZoneAllocPolicy>;
  GCNumberSet observedGCs;

  using AllocationsLog = js::TraceableFifo<AllocationsLogEntry>;

  AllocationsLog allocationsLog;
  bool trackingAllocationSites;
  double allocationSamplingProbability;
  size_t maxAllocationsLogLength;
  bool allocationsLogOverflowed;

  static const size_t DEFAULT_MAX_LOG_LENGTH = 5000;

  MOZ_MUST_USE bool appendAllocationSite(JSContext* cx, HandleObject obj,
                                         HandleSavedFrame frame,
                                         mozilla::TimeStamp when);

  /*
   * Recompute the set of debuggee zones based on the set of debuggee globals.
   */
  void recomputeDebuggeeZoneSet();

  /*
   * Return true if there is an existing object metadata callback for the
   * given global's compartment that will prevent our instrumentation of
   * allocations.
   */
  static bool cannotTrackAllocations(const GlobalObject& global);

  /*
   * Add allocations tracking for objects allocated within the given
   * debuggee's compartment. The given debuggee global must be observed by at
   * least one Debugger that is tracking allocations.
   */
  static MOZ_MUST_USE bool addAllocationsTracking(
      JSContext* cx, Handle<GlobalObject*> debuggee);

  /*
   * Remove allocations tracking for objects allocated within the given
   * global's compartment. This is a no-op if there are still Debuggers
   * observing this global and who are tracking allocations.
   */
  static void removeAllocationsTracking(GlobalObject& global);

  /*
   * Add or remove allocations tracking for all debuggees.
   */
  MOZ_MUST_USE bool addAllocationsTrackingForAllDebuggees(JSContext* cx);
  void removeAllocationsTrackingForAllDebuggees();

  /*
   * If this Debugger has a onNewGlobalObject handler, then
   * this link is inserted into the list headed by
   * JSRuntime::onNewGlobalObjectWatchers.
   */
  mozilla::DoublyLinkedListElement<Debugger> onNewGlobalObjectWatchersLink;

  /*
   * Map from stack frames that are currently on the stack to Debugger.Frame
   * instances.
   *
   * The keys are always live stack frames. We drop them from this map as
   * soon as they leave the stack (see slowPathOnLeaveFrame) and in
   * removeDebuggee.
   *
   * We don't trace the keys of this map (the frames are on the stack and
   * thus necessarily live), but we do trace the values. It's like a WeakMap
   * that way, but since stack frames are not gc-things, the implementation
   * has to be different.
   */
  typedef HashMap<AbstractFramePtr, HeapPtr<DebuggerFrame*>,
                  DefaultHasher<AbstractFramePtr>, ZoneAllocPolicy>
      FrameMap;
  FrameMap frames;

  /*
   * Map from generator objects to their Debugger.Frame instances.
   *
   * When a Debugger.Frame is created for a generator frame, it is added to
   * this map and remains there for the lifetime of the generator, whether
   * that frame is on the stack at the moment or not.  This is in addition to
   * the entry in `frames` that exists as long as the generator frame is on
   * the stack.
   *
   * We need to keep the Debugger.Frame object alive to deliver it to the
   * onEnterFrame handler on resume, and to retain onStep and onPop hooks.
   *
   * An entry is present in this table when:
   *  - both the debuggee generator object and the Debugger.Frame object exists
   *  - the debuggee generator object belongs to a relam that is a debuggee of
   *    the Debugger.Frame's owner.
   *
   * regardless of whether the frame is currently suspended. (This list is
   * meant to explain why we update the table in the particular places where
   * we do so.)
   *
   * An entry in this table exists if and only if the Debugger.Frame's
   * GENERATOR_INFO_SLOT is set.
   */
  typedef DebuggerWeakMap<JSObject*> GeneratorWeakMap;
  GeneratorWeakMap generatorFrames;

  /* An ephemeral map from JSScript* to Debugger.Script instances. */
  typedef DebuggerWeakMap<JSScript*> ScriptWeakMap;
  ScriptWeakMap scripts;

  using LazyScriptWeakMap = DebuggerWeakMap<LazyScript*>;
  LazyScriptWeakMap lazyScripts;

  using LazyScriptVector = JS::GCVector<LazyScript*>;

  // The map from debuggee source script objects to their Debugger.Source
  // instances.
  typedef DebuggerWeakMap<JSObject*, true> SourceWeakMap;
  SourceWeakMap sources;

  // The map from debuggee objects to their Debugger.Object instances.
  typedef DebuggerWeakMap<JSObject*> ObjectWeakMap;
  ObjectWeakMap objects;

  // The map from debuggee Envs to Debugger.Environment instances.
  ObjectWeakMap environments;

  // The map from WasmInstanceObjects to synthesized Debugger.Script
  // instances.
  typedef DebuggerWeakMap<WasmInstanceObject*> WasmInstanceWeakMap;
  WasmInstanceWeakMap wasmInstanceScripts;

  // The map from WasmInstanceObjects to synthesized Debugger.Source
  // instances.
  WasmInstanceWeakMap wasmInstanceSources;

  // Keep track of tracelogger last drained identifiers to know if there are
  // lost events.
#ifdef NIGHTLY_BUILD
  uint32_t traceLoggerLastDrainedSize;
  uint32_t traceLoggerLastDrainedIteration;
#endif
  uint32_t traceLoggerScriptedCallsLastDrainedSize;
  uint32_t traceLoggerScriptedCallsLastDrainedIteration;

  class QueryBase;
  class ScriptQuery;
  class SourceQuery;
  class ObjectQuery;

  enum class FromSweep { No, Yes };

  MOZ_MUST_USE bool addDebuggeeGlobal(JSContext* cx, Handle<GlobalObject*> obj);
  void removeDebuggeeGlobal(FreeOp* fop, GlobalObject* global,
                            WeakGlobalObjectSet::Enum* debugEnum,
                            FromSweep fromSweep);

  enum class CallUncaughtExceptionHook { No, Yes };

  /*
   * Apply the resumption information in (resumeMode, vp) to `frame` in
   * anticipation of returning to the debuggee.
   *
   * This is the usual path for returning from the debugger to the debuggee
   * when we have a resumption value to apply. This does final checks on the
   * result value and exits the debugger's realm by calling `ar.reset()`.
   * Some hooks don't call this because they don't allow the debugger to
   * control resumption; those just call `ar.reset()` and return.
   */
  ResumeMode leaveDebugger(mozilla::Maybe<AutoRealm>& ar,
                           AbstractFramePtr frame,
                           const mozilla::Maybe<HandleValue>& maybeThisv,
                           CallUncaughtExceptionHook callHook,
                           ResumeMode resumeMode, MutableHandleValue vp);

  /*
   * Report and clear the pending exception on ar.context, if any, and return
   * ResumeMode::Terminate.
   */
  ResumeMode reportUncaughtException(mozilla::Maybe<AutoRealm>& ar);

  /*
   * Cope with an error or exception in a debugger hook.
   *
   * If callHook is true, then call the uncaughtExceptionHook, if any. If, in
   * addition, vp is given, then parse the value returned by
   * uncaughtExceptionHook as a resumption value.
   *
   * If there is no uncaughtExceptionHook, or if it fails, report and clear
   * the pending exception on ar.context and return ResumeMode::Terminate.
   *
   * This always calls `ar.reset()`; ar is a parameter because this method
   * must do some things in the debugger realm and some things in the
   * debuggee realm.
   */
  ResumeMode handleUncaughtException(mozilla::Maybe<AutoRealm>& ar);
  ResumeMode handleUncaughtException(
      mozilla::Maybe<AutoRealm>& ar, MutableHandleValue vp,
      const mozilla::Maybe<HandleValue>& thisVForCheck = mozilla::Nothing(),
      AbstractFramePtr frame = NullFramePtr());

  ResumeMode handleUncaughtExceptionHelper(
      mozilla::Maybe<AutoRealm>& ar, MutableHandleValue* vp,
      const mozilla::Maybe<HandleValue>& thisVForCheck, AbstractFramePtr frame);

  /*
   * Handle the result of a hook that is expected to return a resumption
   * value <https://wiki.mozilla.org/Debugger#Resumption_Values>. This is
   * called when we return from a debugging hook to debuggee code. The
   * interpreter wants a (ResumeMode, Value) pair telling it how to proceed.
   *
   * Precondition: ar is entered. We are in the debugger compartment.
   *
   * Postcondition: This called ar.reset(). See handleUncaughtException.
   *
   * If `success` is false, the hook failed. If an exception is pending in
   * ar.context(), return handleUncaughtException(ar, vp, callhook).
   * Otherwise just return ResumeMode::Terminate.
   *
   * If `success` is true, there must be no exception pending in ar.context().
   * `rv` may be:
   *
   *     undefined - Return `ResumeMode::Continue` to continue execution
   *         normally.
   *
   *     {return: value} or {throw: value} - Call unwrapDebuggeeValue to
   *         unwrap `value`. Store the result in `*vp` and return
   *         `ResumeMode::Return` or `ResumeMode::Throw`. The interpreter
   *         will force the current frame to return or throw an exception.
   *
   *     null - Return `ResumeMode::Terminate` to terminate the debuggee with
   *         an uncatchable error.
   *
   *     anything else - Make a new TypeError the pending exception and
   *         return handleUncaughtException(ar, vp, callHook).
   */
  ResumeMode processHandlerResult(mozilla::Maybe<AutoRealm>& ar, bool success,
                                  const Value& rv, AbstractFramePtr frame,
                                  jsbytecode* pc, MutableHandleValue vp);

  ResumeMode processParsedHandlerResult(mozilla::Maybe<AutoRealm>& ar,
                                        AbstractFramePtr frame, jsbytecode* pc,
                                        bool success, ResumeMode resumeMode,
                                        MutableHandleValue vp);

  GlobalObject* unwrapDebuggeeArgument(JSContext* cx, const Value& v);

  static void traceObject(JSTracer* trc, JSObject* obj);

  void trace(JSTracer* trc);
  friend struct js::GCManagedDeletePolicy<Debugger>;

  void traceForMovingGC(JSTracer* trc);
  void traceCrossCompartmentEdges(JSTracer* tracer);

  static const ClassOps classOps_;

 public:
  static const Class class_;

 private:
  static MOZ_MUST_USE bool getHookImpl(JSContext* cx, CallArgs& args,
                                       Debugger& dbg, Hook which);
  static MOZ_MUST_USE bool setHookImpl(JSContext* cx, CallArgs& args,
                                       Debugger& dbg, Hook which);

  static bool getOnDebuggerStatement(JSContext* cx, unsigned argc, Value* vp);
  static bool setOnDebuggerStatement(JSContext* cx, unsigned argc, Value* vp);
  static bool getOnExceptionUnwind(JSContext* cx, unsigned argc, Value* vp);
  static bool setOnExceptionUnwind(JSContext* cx, unsigned argc, Value* vp);
  static bool getOnNewScript(JSContext* cx, unsigned argc, Value* vp);
  static bool setOnNewScript(JSContext* cx, unsigned argc, Value* vp);
  static bool getOnEnterFrame(JSContext* cx, unsigned argc, Value* vp);
  static bool setOnEnterFrame(JSContext* cx, unsigned argc, Value* vp);
  static bool getOnNewGlobalObject(JSContext* cx, unsigned argc, Value* vp);
  static bool setOnNewGlobalObject(JSContext* cx, unsigned argc, Value* vp);
  static bool getOnNewPromise(JSContext* cx, unsigned argc, Value* vp);
  static bool setOnNewPromise(JSContext* cx, unsigned argc, Value* vp);
  static bool getOnPromiseSettled(JSContext* cx, unsigned argc, Value* vp);
  static bool setOnPromiseSettled(JSContext* cx, unsigned argc, Value* vp);
  static bool getUncaughtExceptionHook(JSContext* cx, unsigned argc, Value* vp);
  static bool setUncaughtExceptionHook(JSContext* cx, unsigned argc, Value* vp);
  static bool getAllowUnobservedAsmJS(JSContext* cx, unsigned argc, Value* vp);
  static bool setAllowUnobservedAsmJS(JSContext* cx, unsigned argc, Value* vp);
  static bool getCollectCoverageInfo(JSContext* cx, unsigned argc, Value* vp);
  static bool setCollectCoverageInfo(JSContext* cx, unsigned argc, Value* vp);
  static bool getMemory(JSContext* cx, unsigned argc, Value* vp);
  static bool addDebuggee(JSContext* cx, unsigned argc, Value* vp);
  static bool addAllGlobalsAsDebuggees(JSContext* cx, unsigned argc, Value* vp);
  static bool removeDebuggee(JSContext* cx, unsigned argc, Value* vp);
  static bool removeAllDebuggees(JSContext* cx, unsigned argc, Value* vp);
  static bool hasDebuggee(JSContext* cx, unsigned argc, Value* vp);
  static bool getDebuggees(JSContext* cx, unsigned argc, Value* vp);
  static bool getNewestFrame(JSContext* cx, unsigned argc, Value* vp);
  static bool clearAllBreakpoints(JSContext* cx, unsigned argc, Value* vp);
  static bool findScripts(JSContext* cx, unsigned argc, Value* vp);
  static bool findSources(JSContext* cx, unsigned argc, Value* vp);
  static bool findObjects(JSContext* cx, unsigned argc, Value* vp);
  static bool findAllGlobals(JSContext* cx, unsigned argc, Value* vp);
  static bool makeGlobalObjectReference(JSContext* cx, unsigned argc,
                                        Value* vp);
  static bool setupTraceLoggerScriptCalls(JSContext* cx, unsigned argc,
                                          Value* vp);
  static bool drainTraceLoggerScriptCalls(JSContext* cx, unsigned argc,
                                          Value* vp);
  static bool startTraceLogger(JSContext* cx, unsigned argc, Value* vp);
  static bool endTraceLogger(JSContext* cx, unsigned argc, Value* vp);
  static bool isCompilableUnit(JSContext* cx, unsigned argc, Value* vp);
  static bool recordReplayProcessKind(JSContext* cx, unsigned argc, Value* vp);
#ifdef NIGHTLY_BUILD
  static bool setupTraceLogger(JSContext* cx, unsigned argc, Value* vp);
  static bool drainTraceLogger(JSContext* cx, unsigned argc, Value* vp);
#endif
  static bool adoptDebuggeeValue(JSContext* cx, unsigned argc, Value* vp);
  static bool adoptSource(JSContext* cx, unsigned argc, Value* vp);
  static bool construct(JSContext* cx, unsigned argc, Value* vp);
  static const JSPropertySpec properties[];
  static const JSFunctionSpec methods[];
  static const JSFunctionSpec static_methods[];

  static void removeFromFrameMapsAndClearBreakpointsIn(JSContext* cx,
                                                       AbstractFramePtr frame,
                                                       bool suspending = false);
  static bool updateExecutionObservabilityOfFrames(
      JSContext* cx, const DebugAPI::ExecutionObservableSet& obs,
      IsObserving observing);
  static bool updateExecutionObservabilityOfScripts(
      JSContext* cx, const DebugAPI::ExecutionObservableSet& obs,
      IsObserving observing);
  static bool updateExecutionObservability(
      JSContext* cx, DebugAPI::ExecutionObservableSet& obs,
      IsObserving observing);

  template <typename FrameFn /* void (DebuggerFrame*) */>
  static void forEachDebuggerFrame(AbstractFramePtr frame, FrameFn fn);

  /*
   * Return a vector containing all Debugger.Frame instances referring to
   * |frame|. |global| is |frame|'s global object; if nullptr or omitted, we
   * compute it ourselves from |frame|.
   */
  using DebuggerFrameVector = GCVector<DebuggerFrame*>;
  static MOZ_MUST_USE bool getDebuggerFrames(
      AbstractFramePtr frame, MutableHandle<DebuggerFrameVector> frames);

 public:
  // Public for DebuggerScript::setBreakpoint.
  static MOZ_MUST_USE bool ensureExecutionObservabilityOfScript(
      JSContext* cx, JSScript* script);

  // Whether the Debugger instance needs to observe all non-AOT JS
  // execution of its debugees.
  IsObserving observesAllExecution() const;

  // Whether the Debugger instance needs to observe AOT-compiled asm.js
  // execution of its debuggees.
  IsObserving observesAsmJS() const;

  // Whether the Debugger instance needs to observe coverage of any JavaScript
  // execution.
  IsObserving observesCoverage() const;

 private:
  static MOZ_MUST_USE bool ensureExecutionObservabilityOfFrame(
      JSContext* cx, AbstractFramePtr frame);
  static MOZ_MUST_USE bool ensureExecutionObservabilityOfRealm(
      JSContext* cx, JS::Realm* realm);

  static bool hookObservesAllExecution(Hook which);

  MOZ_MUST_USE bool updateObservesAllExecutionOnDebuggees(
      JSContext* cx, IsObserving observing);
  MOZ_MUST_USE bool updateObservesCoverageOnDebuggees(
      JSContext* cx, IsObserving observing);
  void updateObservesAsmJSOnDebuggees(IsObserving observing);

  JSObject* getHook(Hook hook) const;
  bool hasAnyLiveHooks(JSRuntime* rt) const;

  static void slowPathPromiseHook(JSContext* cx, Hook hook,
                                  Handle<PromiseObject*> promise);

  template <typename HookIsEnabledFun /* bool (Debugger*) */,
            typename FireHookFun /* ResumeMode (Debugger*) */>
  static ResumeMode dispatchHook(JSContext* cx, HookIsEnabledFun hookIsEnabled,
                                 FireHookFun fireHook);

  ResumeMode fireDebuggerStatement(JSContext* cx, MutableHandleValue vp);
  ResumeMode fireExceptionUnwind(JSContext* cx, MutableHandleValue vp);
  ResumeMode fireEnterFrame(JSContext* cx, MutableHandleValue vp);
  ResumeMode fireNewGlobalObject(JSContext* cx, Handle<GlobalObject*> global,
                                 MutableHandleValue vp);
  ResumeMode firePromiseHook(JSContext* cx, Hook hook, HandleObject promise,
                             MutableHandleValue vp);

  DebuggerScript* newVariantWrapper(JSContext* cx,
                                    Handle<DebuggerScriptReferent> referent) {
    return newDebuggerScript(cx, referent);
  }
  NativeObject* newVariantWrapper(JSContext* cx,
                                  Handle<DebuggerSourceReferent> referent) {
    return newDebuggerSource(cx, referent);
  }

  /*
   * Helper function to help wrap Debugger objects whose referents may be
   * variants. Currently Debugger.Script and Debugger.Source referents may
   * be variants.
   *
   * Prefer using wrapScript, wrapWasmScript, wrapSource, and wrapWasmSource
   * whenever possible.
   */
  template <typename Wrapper, typename ReferentVariant, typename Referent,
            typename Map>
  Wrapper* wrapVariantReferent(JSContext* cx, Map& map,
                               Handle<ReferentVariant> referent);
  DebuggerScript* wrapVariantReferent(JSContext* cx,
                                      Handle<DebuggerScriptReferent> referent);
  JSObject* wrapVariantReferent(JSContext* cx,
                                Handle<DebuggerSourceReferent> referent);

  /*
   * Allocate and initialize a Debugger.Script instance whose referent is
   * |referent|.
   */
  DebuggerScript* newDebuggerScript(JSContext* cx,
                                    Handle<DebuggerScriptReferent> referent);

  /*
   * Allocate and initialize a Debugger.Source instance whose referent is
   * |referent|.
   */
  NativeObject* newDebuggerSource(JSContext* cx,
                                  Handle<DebuggerSourceReferent> referent);

  /*
   * Receive a "new script" event from the engine. A new script was compiled
   * or deserialized.
   */
  void fireNewScript(JSContext* cx,
                     Handle<DebuggerScriptReferent> scriptReferent);

  /*
   * Receive a "garbage collection" event from the engine. A GC cycle with the
   * given data was recently completed.
   */
  void fireOnGarbageCollectionHook(
      JSContext* cx, const JS::dbg::GarbageCollectionEvent::Ptr& gcData);

  inline Breakpoint* firstBreakpoint() const;

  static MOZ_MUST_USE bool replaceFrameGuts(JSContext* cx,
                                            AbstractFramePtr from,
                                            AbstractFramePtr to,
                                            ScriptFrameIter& iter);

 public:
  Debugger(JSContext* cx, NativeObject* dbg);
  ~Debugger();

  inline const js::GCPtrNativeObject& toJSObject() const;
  inline js::GCPtrNativeObject& toJSObjectRef();
  static inline Debugger* fromJSObject(const JSObject* obj);

#ifdef DEBUG
  static bool isChildJSObject(JSObject* obj);
#endif
  static Debugger* fromChildJSObject(JSObject* obj);

  Zone* zone() const { return toJSObject()->zone(); }

  bool hasMemory() const;
  DebuggerMemory& memory() const;

  WeakGlobalObjectSet::Range allDebuggees() const { return debuggees.all(); }

  static void detachAllDebuggersFromGlobal(FreeOp* fop, GlobalObject* global);
#ifdef DEBUG
  static bool isDebuggerCrossCompartmentEdge(JSObject* obj,
                                             const js::gc::Cell* cell);
#endif

  static bool hasLiveHook(GlobalObject* global, Hook which);

  /*** Functions for use by Debugger.cpp. *********************************/

  inline bool observesEnterFrame() const;
  inline bool observesNewScript() const;
  inline bool observesNewGlobalObject() const;
  inline bool observesGlobal(GlobalObject* global) const;
  bool observesFrame(AbstractFramePtr frame) const;
  bool observesFrame(const FrameIter& iter) const;
  bool observesScript(JSScript* script) const;
  bool observesWasm(wasm::Instance* instance) const;

  /*
   * If env is nullptr, call vp->setNull() and return true. Otherwise, find
   * or create a Debugger.Environment object for the given Env. On success,
   * store the Environment object in *vp and return true.
   */
  MOZ_MUST_USE bool wrapEnvironment(JSContext* cx, Handle<Env*> env,
                                    MutableHandleValue vp);
  MOZ_MUST_USE bool wrapEnvironment(JSContext* cx, Handle<Env*> env,
                                    MutableHandleDebuggerEnvironment result);

  /*
   * Like cx->compartment()->wrap(cx, vp), but for the debugger realm.
   *
   * Preconditions: *vp is a value from a debuggee realm; cx is in the
   * debugger's compartment.
   *
   * If *vp is an object, this produces a (new or existing) Debugger.Object
   * wrapper for it. Otherwise this is the same as Compartment::wrap.
   *
   * If *vp is a magic JS_OPTIMIZED_OUT value, this produces a plain object
   * of the form { optimizedOut: true }.
   *
   * If *vp is a magic JS_OPTIMIZED_ARGUMENTS value signifying missing
   * arguments, this produces a plain object of the form { missingArguments:
   * true }.
   *
   * If *vp is a magic JS_UNINITIALIZED_LEXICAL value signifying an
   * unaccessible uninitialized binding, this produces a plain object of the
   * form { uninitialized: true }.
   */
  MOZ_MUST_USE bool wrapDebuggeeValue(JSContext* cx, MutableHandleValue vp);
  MOZ_MUST_USE bool wrapDebuggeeObject(JSContext* cx, HandleObject obj,
                                       MutableHandleDebuggerObject result);

  /*
   * Unwrap a Debug.Object, without rewrapping it for any particular debuggee
   * compartment.
   *
   * Preconditions: cx is in the debugger compartment. *vp is a value in that
   * compartment. (*vp should be a "debuggee value", meaning it is the
   * debugger's reflection of a value in the debuggee.)
   *
   * If *vp is a Debugger.Object, store the referent in *vp. Otherwise, if *vp
   * is an object, throw a TypeError, because it is not a debuggee
   * value. Otherwise *vp is a primitive, so leave it alone.
   *
   * When passing values from the debuggee to the debugger:
   *     enter debugger compartment;
   *     call wrapDebuggeeValue;  // compartment- and debugger-wrapping
   *
   * When passing values from the debugger to the debuggee:
   *     call unwrapDebuggeeValue;  // debugger-unwrapping
   *     enter debuggee realm;
   *     call cx->compartment()->wrap;  // compartment-rewrapping
   *
   * (Extreme nerd sidebar: Unwrapping happens in two steps because there are
   * two different kinds of symmetry at work: regardless of which direction
   * we're going, we want any exceptions to be created and thrown in the
   * debugger compartment--mirror symmetry. But compartment wrapping always
   * happens in the target compartment--rotational symmetry.)
   */
  MOZ_MUST_USE bool unwrapDebuggeeValue(JSContext* cx, MutableHandleValue vp);
  MOZ_MUST_USE bool unwrapDebuggeeObject(JSContext* cx,
                                         MutableHandleObject obj);
  MOZ_MUST_USE bool unwrapPropertyDescriptor(
      JSContext* cx, HandleObject obj, MutableHandle<PropertyDescriptor> desc);

  /*
   * Store the Debugger.Frame object for iter in *vp/result.
   *
   * If this Debugger does not already have a Frame object for the frame
   * `iter` points to, a new Frame object is created, and `iter`'s private
   * data is copied into it.
   */
  MOZ_MUST_USE bool getFrame(JSContext* cx, const FrameIter& iter,
                             MutableHandleValue vp);
  MOZ_MUST_USE bool getFrame(JSContext* cx, const FrameIter& iter,
                             MutableHandleDebuggerFrame result);

  /*
   * Return the Debugger.Script object for |script|, or create a new one if
   * needed. The context |cx| must be in the debugger realm; |script| must be
   * a script in a debuggee realm.
   */
  DebuggerScript* wrapScript(JSContext* cx, HandleScript script);

  DebuggerScript* wrapLazyScript(JSContext* cx, Handle<LazyScript*> script);

  /*
   * Return the Debugger.Script object for |wasmInstance| (the toplevel
   * script), synthesizing a new one if needed. The context |cx| must be in
   * the debugger compartment; |wasmInstance| must be a WasmInstanceObject in
   * the debuggee realm.
   */
  DebuggerScript* wrapWasmScript(JSContext* cx,
                                 Handle<WasmInstanceObject*> wasmInstance);

  /*
   * Return the Debugger.Source object for |source|, or create a new one if
   * needed. The context |cx| must be in the debugger compartment; |source|
   * must be a script source object in a debuggee realm.
   */
  JSObject* wrapSource(JSContext* cx, js::HandleScriptSourceObject source);

  /*
   * Return the Debugger.Source object for |wasmInstance| (the entire module),
   * synthesizing a new one if needed. The context |cx| must be in the
   * debugger compartment; |wasmInstance| must be a WasmInstanceObject in the
   * debuggee realm.
   */
  JSObject* wrapWasmSource(JSContext* cx,
                           Handle<WasmInstanceObject*> wasmInstance);

 private:
  Debugger(const Debugger&) = delete;
  Debugger& operator=(const Debugger&) = delete;
};

/*
 * A Handler represents a Debugger API reflection object's handler function,
 * like a Debugger.Frame's onStep handler. These handler functions are called by
 * the Debugger API to notify the user of certain events. For each event type,
 * we define a separate subclass of Handler.
 *
 * When a reflection object accepts a Handler, it calls its 'hold' method; and
 * if the Handler is replaced by another, or the reflection object is finalized,
 * the reflection object calls the Handler's 'drop' method. The reflection
 * object does not otherwise manage the Handler's lifetime, say, by calling its
 * destructor or freeing its memory. A simple Handler implementation might have
 * an empty 'hold' method, and have its 'drop' method delete the Handler. A more
 * complex Handler might process many kinds of events, and thus inherit from
 * many Handler subclasses and be held by many reflection objects
 * simultaneously; a handler like this could use 'hold' and 'drop' to manage a
 * reference count.
 *
 * To support SpiderMonkey's memory use tracking, 'hold' and 'drop' also require
 * a pointer to the owning reflection object, so that the Holder implementation
 * can properly report changes in ownership to functions using the
 * js::gc::MemoryUse categories.
 */
struct Handler {
  virtual ~Handler() {}

  /*
   * If this Handler is a reference to a callable JSObject, return that
   * JSObject. Otherwise, this method returns nullptr.
   *
   * The JavaScript getters for handler properties on reflection objects use
   * this method to obtain the callable the handler represents. When a Handler's
   * 'object' method returns nullptr, that handler is simply not visible to
   * JavaScript.
   */
  virtual JSObject* object() const = 0;

  /* Report that this Handler is now held by owner. See comment above. */
  virtual void hold(JSObject* owner) = 0;

  /* Report that this Handler is no longer held by owner. See comment above. */
  virtual void drop(js::FreeOp* fop, JSObject* owner) = 0;

  /*
   * Trace the reference to the handler. This method will be called by the
   * reflection object holding this Handler whenever the former is traced.
   */
  virtual void trace(JSTracer* tracer) = 0;

  /* Allocation size in bytes for memory accounting purposes. */
  virtual size_t allocSize() const = 0;
};

class JSBreakpointSite;
class WasmBreakpoint;
class WasmBreakpointSite;

class BreakpointSite {
  friend class DebugAPI;
  friend class Breakpoint;
  friend class Debugger;

 public:
  enum class Type { JS, Wasm };

 private:
  Type type_;

  template <typename T>
  struct SiteLinkAccess {
    static mozilla::DoublyLinkedListElement<T>& Get(T* aThis) {
      return aThis->siteLink;
    }
  };

  // List of all js::Breakpoints at this instruction.
  using BreakpointList =
      mozilla::DoublyLinkedList<js::Breakpoint, SiteLinkAccess<js::Breakpoint>>;
  BreakpointList breakpoints;
  size_t enabledCount; /* number of breakpoints in the list that are enabled */

  gc::Cell* owningCellUnbarriered();
  size_t allocSize();

 protected:
  virtual void recompile(FreeOp* fop) = 0;
  bool isEnabled() const { return enabledCount > 0; }

 public:
  BreakpointSite(Type type);
  Breakpoint* firstBreakpoint() const;
  virtual ~BreakpointSite() {}
  bool hasBreakpoint(Breakpoint* bp);
  Type type() const { return type_; }

  void inc(FreeOp* fop);
  void dec(FreeOp* fop);
  bool isEmpty() const;
  virtual void destroyIfEmpty(FreeOp* fop) = 0;

  inline JSBreakpointSite* asJS();
  inline WasmBreakpointSite* asWasm();
};

/*
 * Each Breakpoint is a member of two linked lists: its debugger's list and its
 * site's list.
 *
 * GC rules:
 *   - script is live and breakpoint exists
 *      ==> debugger is live
 *   - script is live, breakpoint exists, and debugger is live
 *      ==> retain the breakpoint and the handler object is live
 *
 * Debugger::markIteratively implements these two rules. It uses
 * Debugger::hasAnyLiveHooks to check for rule 1.
 *
 * Nothing else causes a breakpoint to be retained, so if its script or
 * debugger is collected, the breakpoint is destroyed during GC sweep phase,
 * even if the debugger compartment isn't being GC'd. This is implemented in
 * Zone::sweepBreakpoints.
 */
class Breakpoint {
  friend class DebugAPI;
  friend class Debugger;
  friend class BreakpointSite;

 public:
  Debugger* const debugger;
  BreakpointSite* const site;

 private:
  /*
   * |handler| is marked unconditionally during minor GC so a post barrier is
   * not required.
   */
  js::PreBarrieredObject handler;

  /**
   * Link elements for each list this breakpoint can be in.
   */
  mozilla::DoublyLinkedListElement<Breakpoint> debuggerLink;
  mozilla::DoublyLinkedListElement<Breakpoint> siteLink;

 public:
  Breakpoint(Debugger* debugger, BreakpointSite* site, JSObject* handler);

  enum MayDestroySite { False, True };
  void destroy(FreeOp* fop,
               MayDestroySite mayDestroySite = MayDestroySite::True);

  Breakpoint* nextInDebugger();
  Breakpoint* nextInSite();
  JSObject* getHandler() const { return handler; }
  PreBarrieredObject& getHandlerRef() { return handler; }

  inline WasmBreakpoint* asWasm();
};

class JSBreakpointSite : public BreakpointSite {
 public:
  JSScript* script;
  jsbytecode* const pc;

 protected:
  void recompile(FreeOp* fop) override;

 public:
  JSBreakpointSite(JSScript* script, jsbytecode* pc);

  void destroyIfEmpty(FreeOp* fop) override;
};

inline JSBreakpointSite* BreakpointSite::asJS() {
  MOZ_ASSERT(type() == Type::JS);
  return static_cast<JSBreakpointSite*>(this);
}

class WasmBreakpointSite : public BreakpointSite {
 public:
  wasm::Instance* instance;
  uint32_t offset;

 private:
  void recompile(FreeOp* fop) override;

 public:
  WasmBreakpointSite(wasm::Instance* instance, uint32_t offset);

  void destroyIfEmpty(FreeOp* fop) override;
};

inline WasmBreakpointSite* BreakpointSite::asWasm() {
  MOZ_ASSERT(type() == Type::Wasm);
  return static_cast<WasmBreakpointSite*>(this);
}

class WasmBreakpoint : public Breakpoint {
 public:
  WasmInstanceObject* wasmInstance;

  WasmBreakpoint(Debugger* debugger, WasmBreakpointSite* site,
                 JSObject* handler, WasmInstanceObject* wasmInstance_)
      : Breakpoint(debugger, site, handler), wasmInstance(wasmInstance_) {}
};

inline WasmBreakpoint* Breakpoint::asWasm() {
  MOZ_ASSERT(site && site->type() == BreakpointSite::Type::Wasm);
  return static_cast<WasmBreakpoint*>(this);
}

Breakpoint* Debugger::firstBreakpoint() const {
  if (breakpoints.isEmpty()) {
    return nullptr;
  }
  return &(*breakpoints.begin());
}

const js::GCPtrNativeObject& Debugger::toJSObject() const {
  MOZ_ASSERT(object);
  return object;
}

js::GCPtrNativeObject& Debugger::toJSObjectRef() {
  MOZ_ASSERT(object);
  return object;
}

bool Debugger::observesEnterFrame() const { return getHook(OnEnterFrame); }

bool Debugger::observesNewScript() const { return getHook(OnNewScript); }

bool Debugger::observesNewGlobalObject() const {
  return getHook(OnNewGlobalObject);
}

bool Debugger::observesGlobal(GlobalObject* global) const {
  WeakHeapPtr<GlobalObject*> debuggee(global);
  return debuggees.has(debuggee);
}

MOZ_MUST_USE bool ReportObjectRequired(JSContext* cx);

JSObject* IdVectorToArray(JSContext* cx, Handle<IdVector> ids);
bool IsInterpretedNonSelfHostedFunction(JSFunction* fun);
bool EnsureFunctionHasScript(JSContext* cx, HandleFunction fun);
JSScript* GetOrCreateFunctionScript(JSContext* cx, HandleFunction fun);
bool ValueToIdentifier(JSContext* cx, HandleValue v, MutableHandleId id);
bool ValueToStableChars(JSContext* cx, const char* fnname, HandleValue value,
                        JS::AutoStableStringChars& stableChars);
bool ParseEvalOptions(JSContext* cx, HandleValue value, EvalOptions& options);

Result<Completion> DebuggerGenericEval(
    JSContext* cx, const mozilla::Range<const char16_t> chars,
    HandleObject bindings, const EvalOptions& options, Debugger* dbg,
    HandleObject envArg, FrameIter* iter);

bool ParseResumptionValue(JSContext* cx, HandleValue rval,
                          ResumeMode& resumeMode, MutableHandleValue vp);

} /* namespace js */

namespace JS {

template <>
struct DeletePolicy<js::Debugger>
    : public js::GCManagedDeletePolicy<js::Debugger> {};

} /* namespace JS */

#endif /* debugger_Debugger_h */
