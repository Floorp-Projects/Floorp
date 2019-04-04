/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_AsyncIteration_h
#define vm_AsyncIteration_h

#include "builtin/Promise.h"
#include "js/Class.h"
#include "vm/GeneratorObject.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/List.h"

namespace js {

class AsyncGeneratorObject;

// Resume the async generator when the `await` operand fulfills to `value`.
MOZ_MUST_USE bool AsyncGeneratorAwaitedFulfilled(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    HandleValue value);

// Resume the async generator when the `await` operand rejects with `reason`.
MOZ_MUST_USE bool AsyncGeneratorAwaitedRejected(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    HandleValue reason);

// Resume the async generator after awaiting on the value passed to
// AsyncGenerator#return, when the async generator was still executing.
// Split into two functions depending on whether the awaited value was
// fulfilled or rejected.
MOZ_MUST_USE bool AsyncGeneratorYieldReturnAwaitedFulfilled(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    HandleValue value);
MOZ_MUST_USE bool AsyncGeneratorYieldReturnAwaitedRejected(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    HandleValue reason);

// AsyncGeneratorRequest record in the spec.
// Stores the info from AsyncGenerator#{next,return,throw}.
//
// This object is reused across multiple requests as an optimization, and
// stored in the Slot_CachedRequest slot.
class AsyncGeneratorRequest : public NativeObject {
 private:
  enum AsyncGeneratorRequestSlots {
    // Int32 value with CompletionKind.
    //   Normal: next
    //   Return: return
    //   Throw:  throw
    Slot_CompletionKind = 0,

    // The value passed to AsyncGenerator#{next,return,throw}.
    Slot_CompletionValue,

    // The promise returned by AsyncGenerator#{next,return,throw}.
    Slot_Promise,

    Slots,
  };

  void init(CompletionKind completionKind, const Value& completionValue,
            PromiseObject* promise) {
    setFixedSlot(Slot_CompletionKind,
                 Int32Value(static_cast<int32_t>(completionKind)));
    setFixedSlot(Slot_CompletionValue, completionValue);
    setFixedSlot(Slot_Promise, ObjectValue(*promise));
  }

  // Clear the request data for reuse.
  void clearData() {
    setFixedSlot(Slot_CompletionValue, NullValue());
    setFixedSlot(Slot_Promise, NullValue());
  }

  friend AsyncGeneratorObject;

 public:
  static const Class class_;

  static AsyncGeneratorRequest* create(JSContext* cx,
                                       CompletionKind completionKind,
                                       HandleValue completionValue,
                                       Handle<PromiseObject*> promise);

  CompletionKind completionKind() const {
    return static_cast<CompletionKind>(
        getFixedSlot(Slot_CompletionKind).toInt32());
  }
  JS::Value completionValue() const {
    return getFixedSlot(Slot_CompletionValue);
  }
  PromiseObject* promise() const {
    return &getFixedSlot(Slot_Promise).toObject().as<PromiseObject>();
  }
};

class AsyncGeneratorObject : public AbstractGeneratorObject {
 private:
  enum AsyncGeneratorObjectSlots {
    // Int32 value containing one of the |State| fields from below.
    Slot_State = AbstractGeneratorObject::RESERVED_SLOTS,

    // * null value if this async generator has no requests
    // * AsyncGeneratorRequest if this async generator has only one request
    // * list object if this async generator has 2 or more requests
    Slot_QueueOrRequest,

    // Cached AsyncGeneratorRequest for later use.
    // undefined if there's no cache.
    Slot_CachedRequest,

    Slots
  };

 public:
  enum State {
    // "suspendedStart" in the spec.
    // Suspended after invocation.
    State_SuspendedStart,

    // "suspendedYield" in the spec
    // Suspended with `yield` expression.
    State_SuspendedYield,

    // "executing" in the spec.
    // Resumed from initial suspend or yield, and either running the script
    // or awaiting for `await` expression.
    State_Executing,

    // Part of "executing" in the spec.
    // Awaiting on the value passed by AsyncGenerator#return which is called
    // while executing.
    State_AwaitingYieldReturn,

    // "awaiting-return" in the spec.
    // Awaiting on the value passed by AsyncGenerator#return which is called
    // after completed.
    State_AwaitingReturn,

    // "completed" in the spec.
    // The generator is completed.
    State_Completed
  };

  State state() const {
    return static_cast<State>(getFixedSlot(Slot_State).toInt32());
  }
  void setState(State state_) { setFixedSlot(Slot_State, Int32Value(state_)); }

 private:
  // Queue is implemented in 2 ways.  If only one request is queued ever,
  // request is stored directly to the slot.  Once 2 requests are queued, a
  // list is created and requests are appended into it, and the list is
  // stored to the slot.

  bool isSingleQueue() const {
    return getFixedSlot(Slot_QueueOrRequest).isNull() ||
           getFixedSlot(Slot_QueueOrRequest)
               .toObject()
               .is<AsyncGeneratorRequest>();
  }
  bool isSingleQueueEmpty() const {
    return getFixedSlot(Slot_QueueOrRequest).isNull();
  }
  void setSingleQueueRequest(AsyncGeneratorRequest* request) {
    setFixedSlot(Slot_QueueOrRequest, ObjectValue(*request));
  }
  void clearSingleQueueRequest() {
    setFixedSlot(Slot_QueueOrRequest, NullValue());
  }
  AsyncGeneratorRequest* singleQueueRequest() const {
    return &getFixedSlot(Slot_QueueOrRequest)
                .toObject()
                .as<AsyncGeneratorRequest>();
  }

  ListObject* queue() const {
    return &getFixedSlot(Slot_QueueOrRequest).toObject().as<ListObject>();
  }
  void setQueue(ListObject* queue_) {
    setFixedSlot(Slot_QueueOrRequest, ObjectValue(*queue_));
  }

 public:
  static const Class class_;

  static AsyncGeneratorObject* create(JSContext* cx, HandleFunction asyncGen);

  bool isSuspendedStart() const { return state() == State_SuspendedStart; }
  bool isSuspendedYield() const { return state() == State_SuspendedYield; }
  bool isExecuting() const { return state() == State_Executing; }
  bool isAwaitingYieldReturn() const {
    return state() == State_AwaitingYieldReturn;
  }
  bool isAwaitingReturn() const { return state() == State_AwaitingReturn; }
  bool isCompleted() const { return state() == State_Completed; }

  void setSuspendedStart() { setState(State_SuspendedStart); }
  void setSuspendedYield() { setState(State_SuspendedYield); }
  void setExecuting() { setState(State_Executing); }
  void setAwaitingYieldReturn() { setState(State_AwaitingYieldReturn); }
  void setAwaitingReturn() { setState(State_AwaitingReturn); }
  void setCompleted() { setState(State_Completed); }

  static MOZ_MUST_USE bool enqueueRequest(
      JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
      Handle<AsyncGeneratorRequest*> request);

  static AsyncGeneratorRequest* dequeueRequest(
      JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj);

  static AsyncGeneratorRequest* peekRequest(
      Handle<AsyncGeneratorObject*> asyncGenObj);

  bool isQueueEmpty() const {
    if (isSingleQueue()) {
      return isSingleQueueEmpty();
    }
    return queue()->getDenseInitializedLength() == 0;
  }

  // This function does either of the following:
  //   * return a cached request object with the slots updated
  //   * create a new request object with the slots set
  static AsyncGeneratorRequest* createRequest(
      JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
      CompletionKind completionKind, HandleValue completionValue,
      Handle<PromiseObject*> promise);

  // Stores the given request to the generator's cache after clearing its data
  // slots.  The cached request will be reused in the subsequent createRequest
  // call.
  void cacheRequest(AsyncGeneratorRequest* request) {
    if (hasCachedRequest()) {
      return;
    }

    request->clearData();
    setFixedSlot(Slot_CachedRequest, ObjectValue(*request));
  }

 private:
  bool hasCachedRequest() const {
    return getFixedSlot(Slot_CachedRequest).isObject();
  }

  AsyncGeneratorRequest* takeCachedRequest() {
    auto request = &getFixedSlot(Slot_CachedRequest)
                        .toObject()
                        .as<AsyncGeneratorRequest>();
    clearCachedRequest();
    return request;
  }

  void clearCachedRequest() { setFixedSlot(Slot_CachedRequest, NullValue()); }
};

JSObject* CreateAsyncFromSyncIterator(JSContext* cx, HandleObject iter,
                                      HandleValue nextMethod);

class AsyncFromSyncIteratorObject : public NativeObject {
 private:
  enum AsyncFromSyncIteratorObjectSlots {
    // Object that implements the sync iterator protocol.
    Slot_Iterator = 0,

    // The `next` property of the iterator object.
    Slot_NextMethod = 1,

    Slots
  };

  void init(JSObject* iterator, const Value& nextMethod) {
    setFixedSlot(Slot_Iterator, ObjectValue(*iterator));
    setFixedSlot(Slot_NextMethod, nextMethod);
  }

 public:
  static const Class class_;

  static JSObject* create(JSContext* cx, HandleObject iter,
                          HandleValue nextMethod);

  JSObject* iterator() const { return &getFixedSlot(Slot_Iterator).toObject(); }

  const Value& nextMethod() const { return getFixedSlot(Slot_NextMethod); }
};

MOZ_MUST_USE bool AsyncGeneratorResume(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    CompletionKind completionKind, HandleValue argument);

}  // namespace js

#endif /* vm_AsyncIteration_h */
