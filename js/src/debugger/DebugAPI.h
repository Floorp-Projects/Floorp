/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef debugger_DebugAPI_h
#define debugger_DebugAPI_h

#include "vm/GlobalObject.h"
#include "vm/JSContext.h"

namespace js {

// This file contains the API which SpiderMonkey should use to interact with any
// active Debuggers.

class AbstractGeneratorObject;
class PromiseObject;

/**
 * Tells how the JS engine should resume debuggee execution after firing a
 * debugger hook.  Most debugger hooks get to choose how the debuggee proceeds;
 * see js/src/doc/Debugger/Conventions.md under "Resumption Values".
 *
 * Debugger::processHandlerResult() translates between JavaScript values and
 * this enum.
 *
 * The values `ResumeMode::Throw` and `ResumeMode::Return` are always
 * associated with a value (the exception value or return value). Sometimes
 * this is represented as an explicit `JS::Value` variable or parameter,
 * declared alongside the `ResumeMode`. In other cases, especially when
 * ResumeMode is used as a return type (as in Debugger::onEnterFrame), the
 * value is stashed in `cx`'s pending exception slot or the topmost frame's
 * return value slot.
 */
enum class ResumeMode {
  /**
   * The debuggee should continue unchanged.
   *
   * This corresponds to a resumption value of `undefined`.
   */
  Continue,

  /**
   * Throw an exception in the debuggee.
   *
   * This corresponds to a resumption value of `{throw: <value>}`.
   */
  Throw,

  /**
   * Terminate the debuggee, as if it had been cancelled via the "slow
   * script" ribbon.
   *
   * This corresponds to a resumption value of `null`.
   */
  Terminate,

  /**
   * Force the debuggee to return from the current frame.
   *
   * This corresponds to a resumption value of `{return: <value>}`.
   */
  Return,
};

class DebugScript;
class DebuggerVector;

class DebugAPI {
 public:
  friend class Debugger;

  /*** Methods for interaction with the GC. ***********************************/

  /*
   * A Debugger object is live if:
   *   * the Debugger JSObject is live (Debugger::trace handles this case); OR
   *   * it is in the middle of dispatching an event (the event dispatching
   *     code roots it in this case); OR
   *   * it is enabled, and it is debugging at least one live compartment,
   *     and at least one of the following is true:
   *       - it has a debugger hook installed
   *       - it has a breakpoint set on a live script
   *       - it has a watchpoint set on a live object.
   *
   * DebugAPI::markIteratively handles the last case. If it finds any Debugger
   * objects that are definitely live but not yet marked, it marks them and
   * returns true. If not, it returns false.
   */
  static MOZ_MUST_USE bool markIteratively(GCMarker* marker);

  // Trace cross compartment edges in all debuggers relevant to the current GC.
  static void traceCrossCompartmentEdges(JSTracer* tracer);

  // Trace all debugger-owned GC things unconditionally, during a moving GC.
  static void traceAllForMovingGC(JSTracer* trc);

  // Sweep dying debuggers, and detach edges to dying debuggees.
  static void sweepAll(JSFreeOp* fop);

  // Add sweep group edges due to the presence of any debuggers.
  static MOZ_MUST_USE bool findSweepGroupEdges(JSRuntime* rt);

  // Sweep breakpoints in a script associated with any debugger.
  static inline void sweepBreakpoints(JSFreeOp* fop, JSScript* script);

  // Destroy the debugging information associated with a script.
  static void destroyDebugScript(JSFreeOp* fop, JSScript* script);

  // Validate the debugging information in a script after a moving GC>
#ifdef JSGC_HASH_TABLE_CHECKS
  static void checkDebugScriptAfterMovingGC(DebugScript* ds);
#endif

#ifdef DEBUG
  static bool edgeIsInDebuggerWeakmap(JSRuntime* rt, JSObject* src,
                                      JS::GCCellPtr dst);
#endif

  /*** Methods for querying script breakpoint state. **************************/

  // Query information about whether any debuggers are observing a script.
  static inline bool stepModeEnabled(JSScript* script);
  static inline bool hasBreakpointsAt(JSScript* script, jsbytecode* pc);
  static inline bool hasAnyBreakpointsOrStepMode(JSScript* script);

  /*** Methods for interacting with the JITs. *********************************/

  // Update Debugger frames when an interpreter frame is replaced with a
  // baseline frame.
  static MOZ_MUST_USE bool handleBaselineOsr(JSContext* cx,
                                             InterpreterFrame* from,
                                             jit::BaselineFrame* to);

  // Update Debugger frames when an Ion frame bails out and is replaced with a
  // baseline frame.
  static MOZ_MUST_USE bool handleIonBailout(JSContext* cx,
                                            jit::RematerializedFrame* from,
                                            jit::BaselineFrame* to);

  // Detach any Debugger frames from an Ion frame after an error occurred while
  // it bailed out.
  static void handleUnrecoverableIonBailoutError(
      JSContext* cx, jit::RematerializedFrame* frame);

  // When doing on-stack-replacement of a debuggee interpreter frame with a
  // baseline frame, ensure that the resulting frame can be observed by the
  // debugger.
  static MOZ_MUST_USE bool ensureExecutionObservabilityOfOsrFrame(
      JSContext* cx, AbstractFramePtr osrSourceFrame);

  // Describes a set of scripts or frames whose execution observability can
  // change due to debugger activity.
  class ExecutionObservableSet {
   public:
    typedef HashSet<Zone*>::Range ZoneRange;

    virtual Zone* singleZone() const { return nullptr; }
    virtual JSScript* singleScriptForZoneInvalidation() const {
      return nullptr;
    }
    virtual const HashSet<Zone*>* zones() const { return nullptr; }

    virtual bool shouldRecompileOrInvalidate(JSScript* script) const = 0;
    virtual bool shouldMarkAsDebuggee(FrameIter& iter) const = 0;
  };

  // This enum is converted to and compare with bool values; NotObserving
  // must be 0 and Observing must be 1.
  enum IsObserving { NotObserving = 0, Observing = 1 };

  /*** Methods for calling installed debugger handlers. ***********************/

  // Called when a new script becomes accessible to debuggers.
  static inline void onNewScript(JSContext* cx, HandleScript script);

  // Called when a new wasm instance becomes accessible to debuggers.
  static inline void onNewWasmInstance(
      JSContext* cx, Handle<WasmInstanceObject*> wasmInstance);

  /*
   * Announce to the debugger that the context has entered a new JavaScript
   * frame, |frame|. Call whatever hooks have been registered to observe new
   * frames.
   */
  static inline ResumeMode onEnterFrame(JSContext* cx, AbstractFramePtr frame);

  /*
   * Like onEnterFrame, but for resuming execution of a generator or async
   * function. `frame` is a new baseline or interpreter frame, but abstractly
   * it can be identified with a particular generator frame that was
   * suspended earlier.
   *
   * There is no separate user-visible Debugger.onResumeFrame hook; this
   * fires .onEnterFrame (again, since we're re-entering the frame).
   *
   * Unfortunately, the interpreter and the baseline JIT arrange for this to
   * be called in different ways. The interpreter calls it from JSOP_RESUME,
   * immediately after pushing the resumed frame; the JIT calls it from
   * JSOP_AFTERYIELD, just after the generator resumes. The difference
   * should not be user-visible.
   */
  static inline ResumeMode onResumeFrame(JSContext* cx, AbstractFramePtr frame);

  /*
   * Announce to the debugger a |debugger;| statement on has been
   * encountered on the youngest JS frame on |cx|. Call whatever hooks have
   * been registered to observe this.
   *
   * Note that this method is called for all |debugger;| statements,
   * regardless of the frame's debuggee-ness.
   */
  static inline ResumeMode onDebuggerStatement(JSContext* cx,
                                               AbstractFramePtr frame);

  /*
   * Announce to the debugger that an exception has been thrown and propagated
   * to |frame|. Call whatever hooks have been registered to observe this.
   */
  static inline ResumeMode onExceptionUnwind(JSContext* cx,
                                             AbstractFramePtr frame);

  /*
   * Announce to the debugger that the thread has exited a JavaScript frame,
   * |frame|. If |ok| is true, the frame is returning normally; if |ok| is
   * false, the frame is throwing an exception or terminating.
   *
   * Change cx's current exception and |frame|'s return value to reflect the
   * changes in behavior the hooks request, if any. Return the new error/success
   * value.
   *
   * This function may be called twice for the same outgoing frame; only the
   * first call has any effect. (Permitting double calls simplifies some
   * cases where an onPop handler's resumption value changes a return to a
   * throw, or vice versa: we can redirect to a complete copy of the
   * alternative path, containing its own call to onLeaveFrame.)
   */
  static inline MOZ_MUST_USE bool onLeaveFrame(JSContext* cx,
                                               AbstractFramePtr frame,
                                               jsbytecode* pc, bool ok);

  // Call any breakpoint handlers for the current scripted location.
  static ResumeMode onTrap(JSContext* cx, MutableHandleValue vp);

  // Call any stepping handlers for the current scripted location.
  static ResumeMode onSingleStep(JSContext* cx, MutableHandleValue vp);

  // Notify any Debugger instances observing this promise's global that a new
  // promise was allocated.
  static inline void onNewPromise(JSContext* cx,
                                  Handle<PromiseObject*> promise);

  // Notify any Debugger instances observing this promise's global that the
  // promise has settled (ie, it has either been fulfilled or rejected). Note
  // that this is *not* equivalent to the promise resolution (ie, the promise's
  // fate getting locked in) because you can resolve a promise with another
  // pending promise, in which case neither promise has settled yet.
  //
  // This should never be called on the same promise more than once, because a
  // promise can only make the transition from unsettled to settled once.
  static inline void onPromiseSettled(JSContext* cx,
                                      Handle<PromiseObject*> promise);

  // Notify any Debugger instances that a new global object has been created.
  static inline void onNewGlobalObject(JSContext* cx,
                                       Handle<GlobalObject*> global);

  /*** Methods for querying installed debugger handlers. **********************/

  // Whether any debugger is observing execution in a global.
  static bool debuggerObservesAllExecution(GlobalObject* global);

  // Whether any debugger is observing JS execution coverage in a global.
  static bool debuggerObservesCoverage(GlobalObject* global);

  // Whether any Debugger is observing asm.js execution in a global.
  static bool debuggerObservesAsmJS(GlobalObject* global);

  /*
   * Return true if the given global is being observed by at least one
   * Debugger that is tracking allocations.
   */
  static bool isObservedByDebuggerTrackingAllocations(
      const GlobalObject& debuggee);

  // If any debuggers are tracking allocations for a global, return the
  // probability that a given allocation should be tracked. Nothing otherwise.
  static mozilla::Maybe<double> allocationSamplingProbability(
      GlobalObject* global);

  // Whether any debugger is observing exception unwinds in a realm.
  static bool hasExceptionUnwindHook(GlobalObject* global);

  // Whether any debugger is observing debugger statements in a realm.
  static bool hasDebuggerStatementHook(GlobalObject* global);

  /*** Assorted methods for interacting with the runtime. *********************/

  // When a step handler called during the interrupt callback forces the current
  // frame to return, set state in the frame and context so that the exception
  // handler will perform the forced return.
  static void propagateForcedReturn(JSContext* cx, AbstractFramePtr frame,
                                    HandleValue rval);

  // Checks if the current compartment is allowed to execute code.
  static inline MOZ_MUST_USE bool checkNoExecute(JSContext* cx,
                                                 HandleScript script);

  /*
   * Announce to the debugger that a generator object has been created,
   * via JSOP_GENERATOR.
   *
   * This does not fire user hooks, but it's needed for debugger bookkeeping.
   */
  static inline MOZ_MUST_USE bool onNewGenerator(
      JSContext* cx, AbstractFramePtr frame,
      Handle<AbstractGeneratorObject*> genObj);

  // If necessary, record an object that was just allocated for any observing
  // debuggers.
  static inline MOZ_MUST_USE bool onLogAllocationSite(JSContext* cx,
                                                      JSObject* obj,
                                                      HandleSavedFrame frame,
                                                      mozilla::TimeStamp when);

  // Announce to the debugger that a global object is being collected by the
  // specified major GC.
  static inline void notifyParticipatesInGC(GlobalObject* global,
                                            uint64_t majorGCNumber);

  // Allocate an object which holds a GlobalObject::DebuggerVector.
  static JSObject* newGlobalDebuggersHolder(JSContext* cx);

  // Get the GlobalObject::DebuggerVector for an object allocated by
  // newGlobalDebuggersObject.
  static GlobalObject::DebuggerVector* getGlobalDebuggers(JSObject* holder);

  /*
   * Get any instrumentation ID which has been associated with a script using
   * the specified debugger object.
   */
  static bool getScriptInstrumentationId(JSContext* cx, HandleObject dbgObject,
                                         HandleScript script,
                                         MutableHandleValue rval);

 private:
  static bool stepModeEnabledSlow(JSScript* script);
  static bool hasBreakpointsAtSlow(JSScript* script, jsbytecode* pc);
  static void sweepBreakpointsSlow(JSFreeOp* fop, JSScript* script);
  static void slowPathOnNewScript(JSContext* cx, HandleScript script);
  static void slowPathOnNewGlobalObject(JSContext* cx,
                                        Handle<GlobalObject*> global);
  static void slowPathNotifyParticipatesInGC(
      uint64_t majorGCNumber, GlobalObject::DebuggerVector& dbgs);
  static MOZ_MUST_USE bool slowPathOnLogAllocationSite(
      JSContext* cx, HandleObject obj, HandleSavedFrame frame,
      mozilla::TimeStamp when, GlobalObject::DebuggerVector& dbgs);
  static MOZ_MUST_USE bool slowPathOnLeaveFrame(JSContext* cx,
                                                AbstractFramePtr frame,
                                                jsbytecode* pc, bool ok);
  static MOZ_MUST_USE bool slowPathOnNewGenerator(
      JSContext* cx, AbstractFramePtr frame,
      Handle<AbstractGeneratorObject*> genObj);
  static MOZ_MUST_USE bool slowPathCheckNoExecute(JSContext* cx,
                                                  HandleScript script);
  static ResumeMode slowPathOnEnterFrame(JSContext* cx, AbstractFramePtr frame);
  static ResumeMode slowPathOnResumeFrame(JSContext* cx,
                                          AbstractFramePtr frame);
  static ResumeMode slowPathOnDebuggerStatement(JSContext* cx,
                                                AbstractFramePtr frame);
  static ResumeMode slowPathOnExceptionUnwind(JSContext* cx,
                                              AbstractFramePtr frame);
  static void slowPathOnNewWasmInstance(
      JSContext* cx, Handle<WasmInstanceObject*> wasmInstance);
  static void slowPathOnNewPromise(JSContext* cx,
                                   Handle<PromiseObject*> promise);
  static void slowPathOnPromiseSettled(JSContext* cx,
                                       Handle<PromiseObject*> promise);
  static bool inFrameMaps(AbstractFramePtr frame);
};

// Suppresses all debuggee NX checks, i.e., allow all execution. Used to allow
// certain whitelisted operations to execute code.
//
// WARNING
// WARNING Do not use this unless you know what you are doing!
// WARNING
class AutoSuppressDebuggeeNoExecuteChecks {
  EnterDebuggeeNoExecute** stack_;
  EnterDebuggeeNoExecute* prev_;

 public:
  explicit AutoSuppressDebuggeeNoExecuteChecks(JSContext* cx) {
    stack_ = &cx->noExecuteDebuggerTop.ref();
    prev_ = *stack_;
    *stack_ = nullptr;
  }

  ~AutoSuppressDebuggeeNoExecuteChecks() {
    MOZ_ASSERT(!*stack_);
    *stack_ = prev_;
  }
};

} /* namespace js */

#endif /* debugger_DebugAPI_h */
