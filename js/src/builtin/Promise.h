/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Promise_h
#define builtin_Promise_h

#include "js/Promise.h"

#include "builtin/SelfHostingDefines.h"
#include "ds/Fifo.h"
#include "threading/ConditionVariable.h"
#include "threading/Mutex.h"
#include "vm/NativeObject.h"

namespace js {

enum PromiseSlots {
  // Int32 value with PROMISE_FLAG_* flags below.
  PromiseSlot_Flags = 0,

  // * if this promise is pending, reaction objects
  //     * undefined if there's no reaction
  //     * maybe-wrapped PromiseReactionRecord if there's only one reacion
  //     * dense array if there are two or more more reactions
  // * if this promise is fulfilled, the resolution value
  // * if this promise is rejected, the reason for the rejection
  PromiseSlot_ReactionsOrResult,

  // * if this promise is pending, resolve/reject functions.
  //   This slot holds only the reject function. The resolve function is
  //   reachable from the reject function's extended slot.
  // * if this promise is either fulfilled or rejected, undefined
  PromiseSlot_RejectFunction,

  // Promise object's debug info, which is created on demand.
  // * if this promise has no debug info, undefined
  // * if this promise contains only its process-unique ID, the ID's number
  //   value
  // * otherwise a PromiseDebugInfo object
  PromiseSlot_DebugInfo,

  PromiseSlots,
};

// This promise is either fulfilled or rejected.
// If this flag is not set, this promise is pending.
#define PROMISE_FLAG_RESOLVED 0x1

// If this flag and PROMISE_FLAG_RESOLVED are set, this promise is fulfilled.
// If only PROMISE_FLAG_RESOLVED is set, this promise is rejected.
#define PROMISE_FLAG_FULFILLED 0x2

// Indicates the promise has ever had a fulfillment or rejection handler;
// used in unhandled rejection tracking.
#define PROMISE_FLAG_HANDLED 0x4

// This promise uses the default resolving functions.
// The PromiseSlot_RejectFunction slot is not used.
#define PROMISE_FLAG_DEFAULT_RESOLVING_FUNCTIONS 0x08

// This promise is the return value of an async function invocation.
#define PROMISE_FLAG_ASYNC 0x10

// This promise knows how to propagate information required to keep track of
// whether an activation behavior was in progress when the original promise in
// the promise chain was created.  This is a concept defined in the HTML spec:
// https://html.spec.whatwg.org/multipage/interaction.html#triggered-by-user-activation
// It is used by the embedder in order to request SpiderMonkey to keep track of
// this information in a Promise, and also to propagate it to newly created
// promises while processing Promise#then.
#define PROMISE_FLAG_REQUIRES_USER_INTERACTION_HANDLING 0x20

// This flag indicates whether an activation behavior was in progress when the
// original promise in the promise chain was created.  Activation behavior is a
// concept defined by the HTML spec:
// https://html.spec.whatwg.org/multipage/interaction.html#triggered-by-user-activation
// This flag is only effective when the
// PROMISE_FLAG_REQUIRES_USER_INTERACTION_HANDLING is set.
#define PROMISE_FLAG_HAD_USER_INTERACTION_UPON_CREATION 0x40

class AutoSetNewObjectMetadata;

class PromiseObject : public NativeObject {
 public:
  static const unsigned RESERVED_SLOTS = PromiseSlots;
  static const Class class_;
  static const Class protoClass_;
  static PromiseObject* create(JSContext* cx, HandleObject executor,
                               HandleObject proto = nullptr,
                               bool needsWrapping = false);

  static PromiseObject* createSkippingExecutor(JSContext* cx);

  static JSObject* unforgeableResolve(JSContext* cx, HandleValue value);
  static JSObject* unforgeableReject(JSContext* cx, HandleValue value);

  int32_t flags() { return getFixedSlot(PromiseSlot_Flags).toInt32(); }
  void setHandled() {
    setFixedSlot(PromiseSlot_Flags, Int32Value(flags() | PROMISE_FLAG_HANDLED));
  }
  JS::PromiseState state() {
    int32_t flags = this->flags();
    if (!(flags & PROMISE_FLAG_RESOLVED)) {
      MOZ_ASSERT(!(flags & PROMISE_FLAG_FULFILLED));
      return JS::PromiseState::Pending;
    }
    if (flags & PROMISE_FLAG_FULFILLED) {
      return JS::PromiseState::Fulfilled;
    }
    return JS::PromiseState::Rejected;
  }
  Value reactions() {
    MOZ_ASSERT(state() == JS::PromiseState::Pending);
    return getFixedSlot(PromiseSlot_ReactionsOrResult);
  }
  Value value() {
    MOZ_ASSERT(state() == JS::PromiseState::Fulfilled);
    return getFixedSlot(PromiseSlot_ReactionsOrResult);
  }
  Value reason() {
    MOZ_ASSERT(state() == JS::PromiseState::Rejected);
    return getFixedSlot(PromiseSlot_ReactionsOrResult);
  }
  Value valueOrReason() {
    MOZ_ASSERT(state() != JS::PromiseState::Pending);
    return getFixedSlot(PromiseSlot_ReactionsOrResult);
  }

  static MOZ_MUST_USE bool resolve(JSContext* cx,
                                   Handle<PromiseObject*> promise,
                                   HandleValue resolutionValue);
  static MOZ_MUST_USE bool reject(JSContext* cx, Handle<PromiseObject*> promise,
                                  HandleValue rejectionValue);

  static void onSettled(JSContext* cx, Handle<PromiseObject*> promise);

  double allocationTime();
  double resolutionTime();
  JSObject* allocationSite();
  JSObject* resolutionSite();
  double lifetime();
  double timeToResolution() {
    MOZ_ASSERT(state() != JS::PromiseState::Pending);
    return resolutionTime() - allocationTime();
  }
  MOZ_MUST_USE bool dependentPromises(JSContext* cx,
                                      MutableHandle<GCVector<Value>> values);

  // Return the process-unique ID of this promise. Only used by the debugger.
  uint64_t getID();

  bool isUnhandled() {
    MOZ_ASSERT(state() == JS::PromiseState::Rejected);
    return !(flags() & PROMISE_FLAG_HANDLED);
  }

  bool requiresUserInteractionHandling() {
    return (flags() & PROMISE_FLAG_REQUIRES_USER_INTERACTION_HANDLING);
  }

  void setRequiresUserInteractionHandling(bool state);

  bool hadUserInteractionUponCreation() {
    return (flags() & PROMISE_FLAG_HAD_USER_INTERACTION_UPON_CREATION);
  }

  void setHadUserInteractionUponCreation(bool state);

  void copyUserInteractionFlagsFrom(PromiseObject& rhs);
};

/**
 * Unforgeable version of the JS builtin Promise.all.
 *
 * Takes a HandleValueVector of Promise objects and returns a promise that's
 * resolved with an array of resolution values when all those promises have
 * been resolved, or rejected with the rejection value of the first rejected
 * promise.
 *
 * Asserts that all objects in the `promises` vector are, maybe wrapped,
 * instances of `Promise` or a subclass of `Promise`.
 */
MOZ_MUST_USE JSObject* GetWaitForAllPromise(JSContext* cx,
                                            JS::HandleObjectVector promises);

// Whether to create a promise as the return value of Promise#{then,catch}.
// If the return value is known to be unused, and if the operation is known
// to be unobservable, we can skip creating the promise.
enum class CreateDependentPromise {
  // The return value is not known to be unused.
  Always,

  // The return value is known to be unused.
  SkipIfCtorUnobservable,

  // The return value is known to be unused, and the operation is known
  // to be unobservable.
  Never
};

/**
 * Enqueues resolve/reject reactions in the given Promise's reactions lists
 * as though calling the original value of Promise.prototype.then.
 *
 * If the `createDependent` flag is not set, no dependent Promise will be
 * created. This is used internally to implement DOM functionality.
 * Note: In this case, the reactions pushed using this function contain a
 * `promise` field that can contain null. That field is only ever used by
 * devtools, which have to treat these reactions specially.
 *
 * Asserts that `promiseObj` is a, maybe wrapped, instance of Promise.
 */
MOZ_MUST_USE bool OriginalPromiseThen(JSContext* cx, HandleObject promiseObj,
                                      HandleValue onFulfilled,
                                      HandleValue onRejected,
                                      MutableHandleObject dependent,
                                      CreateDependentPromise createDependent);

/**
 * PromiseResolve ( C, x )
 *
 * The abstract operation PromiseResolve, given a constructor and a value,
 * returns a new promise resolved with that value.
 */
MOZ_MUST_USE JSObject* PromiseResolve(JSContext* cx, HandleObject constructor,
                                      HandleValue value);

MOZ_MUST_USE bool RejectPromiseWithPendingError(JSContext* cx,
                                                Handle<PromiseObject*> promise);

/**
 * Create the promise object which will be used as the return value of an async
 * function.
 */
MOZ_MUST_USE PromiseObject* CreatePromiseObjectForAsync(JSContext* cx);

/**
 * Returns true if the given object is a promise created by
 * CreatePromiseObjectForAsync function.
 */
MOZ_MUST_USE bool IsPromiseForAsync(JSObject* promise);

class AsyncFunctionGeneratorObject;

MOZ_MUST_USE bool AsyncFunctionReturned(JSContext* cx,
                                        Handle<PromiseObject*> resultPromise,
                                        HandleValue value);

MOZ_MUST_USE bool AsyncFunctionThrown(JSContext* cx,
                                      Handle<PromiseObject*> resultPromise,
                                      HandleValue reason);

// Start awaiting `value` in an async function (, but doesn't suspend the
// async function's execution!). Returns the async function's result promise.
MOZ_MUST_USE JSObject* AsyncFunctionAwait(
    JSContext* cx, Handle<AsyncFunctionGeneratorObject*> genObj,
    HandleValue value);

// If the await operation can be skipped and the resolution value for `val` can
// be acquired, stored the resolved value to `resolved` and `true` to
// `*canSkip`.  Otherwise, stores `false` to `*canSkip`.
MOZ_MUST_USE bool TrySkipAwait(JSContext* cx, HandleValue val, bool* canSkip,
                               MutableHandleValue resolved);

class AsyncGeneratorObject;

MOZ_MUST_USE bool AsyncGeneratorAwait(JSContext* cx,
                                      Handle<AsyncGeneratorObject*> asyncGenObj,
                                      HandleValue value);

MOZ_MUST_USE bool AsyncGeneratorResolve(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj, HandleValue value,
    bool done);

MOZ_MUST_USE bool AsyncGeneratorReject(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    HandleValue exception);

MOZ_MUST_USE bool AsyncGeneratorEnqueue(JSContext* cx, HandleValue asyncGenVal,
                                        CompletionKind completionKind,
                                        HandleValue completionValue,
                                        MutableHandleValue result);

bool AsyncFromSyncIteratorMethod(JSContext* cx, CallArgs& args,
                                 CompletionKind completionKind);

class MOZ_NON_TEMPORARY_CLASS PromiseLookup final {
  // clang-format off
    /*
     * A PromiseLookup holds the following:
     *
     *  Promise's shape (promiseConstructorShape_)
     *       To ensure that Promise has not been modified.
     *
     *  Promise.prototype's shape (promiseProtoShape_)
     *      To ensure that Promise.prototype has not been modified.
     *
     *  Promise's shape for the @@species getter. (promiseSpeciesShape_)
     *      To quickly retrieve the @@species getter for Promise.
     *
     *  Promise's slot number for resolve (promiseResolveSlot_)
     *      To quickly retrieve the Promise.resolve function.
     *
     *  Promise.prototype's slot number for constructor (promiseProtoConstructorSlot_)
     *      To quickly retrieve the Promise.prototype.constructor property.
     *
     *  Promise.prototype's slot number for then (promiseProtoThenSlot_)
     *      To quickly retrieve the Promise.prototype.then function.
     *
     * MOZ_INIT_OUTSIDE_CTOR fields below are set in |initialize()|.  The
     * constructor only initializes a |state_| field, that defines whether the
     * other fields are accessible.
     */
  // clang-format on

  // Shape of matching Promise object.
  MOZ_INIT_OUTSIDE_CTOR Shape* promiseConstructorShape_;

#ifdef DEBUG
  // Accessor Shape containing the @@species property.
  // See isPromiseStateStillSane() for why this field is debug-only.
  MOZ_INIT_OUTSIDE_CTOR Shape* promiseSpeciesShape_;
#endif

  // Shape of matching Promise.prototype object.
  MOZ_INIT_OUTSIDE_CTOR Shape* promiseProtoShape_;

  // Slots Promise.resolve, Promise.prototype.constructor, and
  // Promise.prototype.then.
  MOZ_INIT_OUTSIDE_CTOR uint32_t promiseResolveSlot_;
  MOZ_INIT_OUTSIDE_CTOR uint32_t promiseProtoConstructorSlot_;
  MOZ_INIT_OUTSIDE_CTOR uint32_t promiseProtoThenSlot_;

  enum class State : uint8_t {
    // Flags marking the lazy initialization of the above fields.
    Uninitialized,
    Initialized,

    // The disabled flag is set when we don't want to try optimizing
    // anymore because core objects were changed.
    Disabled
  };

  State state_ = State::Uninitialized;

  // Initialize the internal fields.
  //
  // The cache is successfully initialized iff
  // 1. Promise and Promise.prototype classes are initialized.
  // 2. Promise.prototype.constructor is equal to Promise.
  // 3. Promise.prototype.then is the original `then` function.
  // 4. Promise[@@species] is the original @@species getter.
  // 5. Promise.resolve is the original `resolve` function.
  void initialize(JSContext* cx);

  // Reset the cache.
  void reset();

  // Check if the global promise-related objects have not been messed with
  // in a way that would disable this cache.
  bool isPromiseStateStillSane(JSContext* cx);

  // Flags to control whether or not ensureInitialized() is allowed to
  // reinitialize the cache when the Promise state is no longer sane.
  enum class Reinitialize : bool { Allowed, Disallowed };

  // Return true if the lookup cache is properly initialized for usage.
  bool ensureInitialized(JSContext* cx, Reinitialize reinitialize);

  // Return true if the prototype of the given Promise object is
  // Promise.prototype and the object doesn't shadow properties from
  // Promise.prototype.
  bool hasDefaultProtoAndNoShadowedProperties(JSContext* cx,
                                              PromiseObject* promise);

  // Return true if the given Promise object uses the default @@species,
  // "constructor", and "then" properties.
  bool isDefaultInstance(JSContext* cx, PromiseObject* promise,
                         Reinitialize reinitialize);

  // Return the built-in Promise constructor or null if not yet initialized.
  static JSFunction* getPromiseConstructor(JSContext* cx);

  // Return the built-in Promise prototype or null if not yet initialized.
  static NativeObject* getPromisePrototype(JSContext* cx);

  // Return true if the slot contains the given native.
  static bool isDataPropertyNative(JSContext* cx, NativeObject* obj,
                                   uint32_t slot, JSNative native);

  // Return true if the accessor shape contains the given native.
  static bool isAccessorPropertyNative(JSContext* cx, Shape* shape,
                                       JSNative native);

 public:
  /** Construct a |PromiseSpeciesLookup| in the uninitialized state. */
  PromiseLookup() { reset(); }

  // Return true if the Promise constructor and Promise.prototype still use
  // the default built-in functions.
  bool isDefaultPromiseState(JSContext* cx);

  // Return true if the given Promise object uses the default @@species,
  // "constructor", and "then" properties.
  bool isDefaultInstance(JSContext* cx, PromiseObject* promise) {
    return isDefaultInstance(cx, promise, Reinitialize::Allowed);
  }

  // Return true if the given Promise object uses the default @@species,
  // "constructor", and "then" properties.
  bool isDefaultInstanceWhenPromiseStateIsSane(JSContext* cx,
                                               PromiseObject* promise) {
    return isDefaultInstance(cx, promise, Reinitialize::Disallowed);
  }

  // Purge the cache and all info associated with it.
  void purge() {
    if (state_ == State::Initialized) {
      reset();
    }
  }
};

class OffThreadPromiseRuntimeState;

// [SMDOC] OffThreadPromiseTask: an off-main-thread task that resolves a promise
//
// An OffThreadPromiseTask is an abstract base class holding a JavaScript
// promise that will be resolved (fulfilled or rejected) with the results of a
// task possibly performed by some other thread.
//
// An OffThreadPromiseTask's lifecycle is as follows:
//
// - Some JavaScript native wishes to return a promise of the result of some
//   computation that might be performed by other threads (say, helper threads
//   or the embedding's I/O threads), so it creates a PromiseObject to represent
//   the result, and an OffThreadPromiseTask referring to it. After handing the
//   OffThreadPromiseTask to the code doing the actual work, the native is free
//   to return the PromiseObject to its caller.
//
// - When the computation is done, successfully or otherwise, it populates the
//   OffThreadPromiseTask—which is actually an instance of some concrete
//   subclass specific to the task—with the information needed to resolve the
//   promise, and calls OffThreadPromiseTask::dispatchResolveAndDestroy. This
//   enqueues a runnable on the JavaScript thread to which the promise belongs.
//
// - When it gets around to the runnable, the JavaScript thread calls the
//   OffThreadPromiseTask's `resolve` method, which the concrete subclass has
//   overriden to resolve the promise appropriately. This probably enqueues a
//   promise reaction job.
//
// - The JavaScript thread then deletes the OffThreadPromiseTask.
//
// During shutdown, the process is slightly different. Enqueuing runnables to
// the JavaScript thread begins to fail. JSRuntime shutdown waits for all
// outstanding tasks to call dispatchResolveAndDestroy, and then deletes them on
// the main thread, without calling `resolve`.
//
// For example, the JavaScript function WebAssembly.compile uses
// OffThreadPromiseTask to manage the result of a helper thread task, accepting
// binary WebAssembly code and returning a promise of a compiled
// WebAssembly.Module. It would like to do this compilation work on a helper
// thread. When called by JavaScript, WebAssembly.compile creates a promise,
// builds a CompileBufferTask (the OffThreadPromiseTask concrete subclass) to
// keep track of it, and then hands that to a helper thread. When the helper
// thread is done, successfully or otherwise, it calls the CompileBufferTask's
// dispatchResolveAndDestroy method, which enqueues a runnable to the JavaScript
// thread to resolve the promise and delete the CompileBufferTask.
// (CompileBufferTask actually implements PromiseHelperTask, which implements
// OffThreadPromiseTask; PromiseHelperTask is what our helper thread scheduler
// requires.)
//
// OffThreadPromiseTasks are not limited to use with helper threads. For
// example, a function returning a promise of the result of a network operation
// could provide the code collecting the incoming data with an
// OffThreadPromiseTask for the promise, and let the embedding's network I/O
// threads call dispatchResolveAndDestroy.
//
// OffThreadPromiseTask may also be used purely on the main thread, as a way to
// "queue a task" in HTML terms. Note that a "task" is not the same as a
// "microtask" and there are separate queues for tasks and microtasks that are
// drained at separate times in the browser. The task queue is implemented by
// the browser's main event loop. The microtask queue is implemented
// by JS::JobQueue, used for promises and gets drained before returning to
// the event loop. Thus OffThreadPromiseTask can only be used when the spec
// says "queue a task", as the WebAssembly APIs do.
//
// An OffThreadPromiseTask has a JSContext, and must be constructed and have its
// 'init' method called on that JSContext's thread. Once initialized, its
// dispatchResolveAndDestroy method may be called from any thread. This is the
// only safe way to destruct an OffThreadPromiseTask; doing so ensures the
// OffThreadPromiseTask's destructor will run on the JSContext's thread, either
// from the event loop or during shutdown.
//
// OffThreadPromiseTask::dispatchResolveAndDestroy uses the
// JS::DispatchToEventLoopCallback provided by the embedding to enqueue
// runnables on the JavaScript thread. See the comments for
// DispatchToEventLoopCallback for details.

class OffThreadPromiseTask : public JS::Dispatchable {
  friend class OffThreadPromiseRuntimeState;

  JSRuntime* runtime_;
  PersistentRooted<PromiseObject*> promise_;
  bool registered_;

  void operator=(const OffThreadPromiseTask&) = delete;
  OffThreadPromiseTask(const OffThreadPromiseTask&) = delete;

  void unregister(OffThreadPromiseRuntimeState& state);

 protected:
  OffThreadPromiseTask(JSContext* cx, Handle<PromiseObject*> promise);

  // To be called by OffThreadPromiseTask and implemented by the derived class.
  virtual bool resolve(JSContext* cx, Handle<PromiseObject*> promise) = 0;

  // JS::Dispatchable implementation. Ends with 'delete this'.
  void run(JSContext* cx, MaybeShuttingDown maybeShuttingDown) final;

 public:
  ~OffThreadPromiseTask() override;

  // Initializing an OffThreadPromiseTask informs the runtime that it must
  // wait on shutdown for this task to rejoin the active JSContext by calling
  // dispatchResolveAndDestroy().
  bool init(JSContext* cx);

  // An initialized OffThreadPromiseTask can be dispatched to an active
  // JSContext of its Promise's JSRuntime from any thread. Normally, this will
  // lead to resolve() being called on JSContext thread, given the Promise.
  // However, if shutdown interrupts, resolve() may not be called, though the
  // OffThreadPromiseTask will be destroyed on a JSContext thread.
  void dispatchResolveAndDestroy();
};

using OffThreadPromiseTaskSet =
    HashSet<OffThreadPromiseTask*, DefaultHasher<OffThreadPromiseTask*>,
            SystemAllocPolicy>;

using DispatchableFifo = Fifo<JS::Dispatchable*, 0, SystemAllocPolicy>;

class OffThreadPromiseRuntimeState {
  friend class OffThreadPromiseTask;

  // These fields are initialized once before any off-thread usage and thus do
  // not require a lock.
  JS::DispatchToEventLoopCallback dispatchToEventLoopCallback_;
  void* dispatchToEventLoopClosure_;

  // All following fields are mutated by any thread and are guarded by mutex_.
  Mutex mutex_;

  // A set of all OffThreadPromiseTasks that have successfully called 'init'.
  // OffThreadPromiseTask's destructor removes them from the set.
  OffThreadPromiseTaskSet live_;

  // The allCancelled_ condition is waited on and notified during engine
  // shutdown, communicating when all off-thread tasks in live_ are safe to be
  // destroyed from the (shutting down) main thread. This condition is met when
  // live_.count() == numCanceled_ where "canceled" means "the
  // DispatchToEventLoopCallback failed after this task finished execution".
  ConditionVariable allCanceled_;
  size_t numCanceled_;

  // The queue of JS::Dispatchables used by the DispatchToEventLoopCallback that
  // calling js::UseInternalJobQueues installs.
  DispatchableFifo internalDispatchQueue_;
  ConditionVariable internalDispatchQueueAppended_;
  bool internalDispatchQueueClosed_;

  static bool internalDispatchToEventLoop(void*, JS::Dispatchable*);
  bool usingInternalDispatchQueue() const;

  void operator=(const OffThreadPromiseRuntimeState&) = delete;
  OffThreadPromiseRuntimeState(const OffThreadPromiseRuntimeState&) = delete;

 public:
  OffThreadPromiseRuntimeState();
  ~OffThreadPromiseRuntimeState();
  void init(JS::DispatchToEventLoopCallback callback, void* closure);
  void initInternalDispatchQueue();
  bool initialized() const;

  // If initInternalDispatchQueue() was called, internalDrain() can be
  // called to periodically drain the dispatch queue before shutdown.
  void internalDrain(JSContext* cx);
  bool internalHasPending();

  // shutdown() must be called by the JSRuntime while the JSRuntime is valid.
  void shutdown(JSContext* cx);
};

}  // namespace js

#endif /* builtin_Promise_h */
