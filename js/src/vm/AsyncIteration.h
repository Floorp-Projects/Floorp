/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_AsyncIteration_h
#define vm_AsyncIteration_h

#include "jscntxt.h"
#include "jsobj.h"

#include "builtin/Promise.h"
#include "vm/GeneratorObject.h"

namespace js {

// Async generator consists of 2 functions, |wrapped| and |unwrapped|.
// |unwrapped| is a generator function compiled from async generator script,
// |await| behaves just like |yield| there.  |unwrapped| isn't exposed to user
// script.
// |wrapped| is a native function that is the value of async generator.

JSObject*
WrapAsyncGeneratorWithProto(JSContext* cx, HandleFunction unwrapped, HandleObject proto);

JSObject*
WrapAsyncGenerator(JSContext* cx, HandleFunction unwrapped);

bool
IsWrappedAsyncGenerator(JSFunction* fun);

JSFunction*
GetWrappedAsyncGenerator(JSFunction* unwrapped);

JSFunction*
GetUnwrappedAsyncGenerator(JSFunction* wrapped);

MOZ_MUST_USE bool
AsyncGeneratorAwaitedFulfilled(JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
                               HandleValue value);
MOZ_MUST_USE bool
AsyncGeneratorAwaitedRejected(JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
                              HandleValue reason);
MOZ_MUST_USE bool
AsyncGeneratorYieldReturnAwaitedFulfilled(JSContext* cx,
                                          Handle<AsyncGeneratorObject*> asyncGenObj,
                                          HandleValue value);
MOZ_MUST_USE bool
AsyncGeneratorYieldReturnAwaitedRejected(JSContext* cx,
                                         Handle<AsyncGeneratorObject*> asyncGenObj,
                                         HandleValue reason);

class AsyncGeneratorRequest : public NativeObject
{
  private:
    enum AsyncGeneratorRequestSlots {
        Slot_CompletionKind = 0,
        Slot_CompletionValue,
        Slot_Promise,
        Slots,
    };

    void setCompletionKind(CompletionKind completionKind_) {
        setFixedSlot(Slot_CompletionKind,
                     Int32Value(static_cast<int32_t>(completionKind_)));
    }
    void setCompletionValue(HandleValue completionValue_) {
        setFixedSlot(Slot_CompletionValue, completionValue_);
    }
    void setPromise(HandleObject promise_) {
        setFixedSlot(Slot_Promise, ObjectValue(*promise_));
    }

  public:
    static const Class class_;

    static AsyncGeneratorRequest*
    create(JSContext* cx, CompletionKind completionKind, HandleValue completionValue,
           HandleObject promise);

    CompletionKind completionKind() const {
        return static_cast<CompletionKind>(getFixedSlot(Slot_CompletionKind).toInt32());
    }
    JS::Value completionValue() const {
        return getFixedSlot(Slot_CompletionValue);
    }
    JSObject* promise() const {
        return &getFixedSlot(Slot_Promise).toObject();
    }
};

class AsyncGeneratorObject : public NativeObject
{
  private:
    enum AsyncGeneratorObjectSlots {
        Slot_State = 0,
        Slot_Generator,
        Slot_QueueOrRequest,
        Slots
    };

    enum State {
        State_SuspendedStart,
        State_SuspendedYield,
        State_Executing,
        // State_AwaitingYieldReturn corresponds to the case that
        // AsyncGenerator#return is called while State_Executing,
        // just like the case that AsyncGenerator#return is called
        // while State_Completed.
        State_AwaitingYieldReturn,
        State_AwaitingReturn,
        State_Completed
    };

    State state() const {
        return static_cast<State>(getFixedSlot(Slot_State).toInt32());
    }
    void setState(State state_) {
        setFixedSlot(Slot_State, Int32Value(state_));
    }

    void setGenerator(const Value& value) {
        setFixedSlot(Slot_Generator, value);
    }

    // Queue is implemented in 2 ways.  If only one request is queued ever,
    // request is stored directly to the slot.  Once 2 requests are queued, an
    // array is created and requests are pushed into it, and the array is
    // stored to the slot.

    bool isSingleQueue() const {
        return getFixedSlot(Slot_QueueOrRequest).isNull() ||
               getFixedSlot(Slot_QueueOrRequest).toObject().is<AsyncGeneratorRequest>();
    }
    bool isSingleQueueEmpty() const {
        return getFixedSlot(Slot_QueueOrRequest).isNull();
    }
    void setSingleQueueRequest(AsyncGeneratorRequest* request) {
        setFixedSlot(Slot_QueueOrRequest, ObjectValue(*request));
    }
    void clearSingleQueueRequest() {
        setFixedSlot(Slot_QueueOrRequest, NullHandleValue);
    }
    AsyncGeneratorRequest* singleQueueRequest() const {
        return &getFixedSlot(Slot_QueueOrRequest).toObject().as<AsyncGeneratorRequest>();
    }

    NativeObject* queue() const {
        return &getFixedSlot(Slot_QueueOrRequest).toObject().as<NativeObject>();
    }
    void setQueue(JSObject* queue_) {
        setFixedSlot(Slot_QueueOrRequest, ObjectValue(*queue_));
    }

  public:
    static const Class class_;

    static AsyncGeneratorObject*
    create(JSContext* cx, HandleFunction asyncGen, HandleValue generatorVal);

    bool isSuspendedStart() const {
        return state() == State_SuspendedStart;
    }
    bool isSuspendedYield() const {
        return state() == State_SuspendedYield;
    }
    bool isExecuting() const {
        return state() == State_Executing;
    }
    bool isAwaitingYieldReturn() const {
        return state() == State_AwaitingYieldReturn;
    }
    bool isAwaitingReturn() const {
        return state() == State_AwaitingReturn;
    }
    bool isCompleted() const {
        return state() == State_Completed;
    }

    void setSuspendedStart() {
        setState(State_SuspendedStart);
    }
    void setSuspendedYield() {
        setState(State_SuspendedYield);
    }
    void setExecuting() {
        setState(State_Executing);
    }
    void setAwaitingYieldReturn() {
        setState(State_AwaitingYieldReturn);
    }
    void setAwaitingReturn() {
        setState(State_AwaitingReturn);
    }
    void setCompleted() {
        setState(State_Completed);
    }

    JS::Value generatorVal() const {
        return getFixedSlot(Slot_Generator);
    }
    GeneratorObject* generatorObj() const {
        return &getFixedSlot(Slot_Generator).toObject().as<GeneratorObject>();
    }

    static MOZ_MUST_USE bool
    enqueueRequest(JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
                   Handle<AsyncGeneratorRequest*> request);

    static AsyncGeneratorRequest*
    dequeueRequest(JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj);

    static AsyncGeneratorRequest*
    peekRequest(JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj);

    bool isQueueEmpty() const {
        if (isSingleQueue())
            return isSingleQueueEmpty();
        return queue()->getDenseInitializedLength() == 0;
    }
};

JSObject*
CreateAsyncFromSyncIterator(JSContext* cx, HandleObject iter);

class AsyncFromSyncIteratorObject : public NativeObject
{
  private:
    enum AsyncFromSyncIteratorObjectSlots {
        Slot_Iterator = 0,
        Slots
    };

    void setIterator(HandleObject iterator_) {
        setFixedSlot(Slot_Iterator, ObjectValue(*iterator_));
    }

  public:
    static const Class class_;

    static JSObject*
    create(JSContext* cx, HandleObject iter);

    JSObject* iterator() const {
        return &getFixedSlot(Slot_Iterator).toObject();
    }
};

MOZ_MUST_USE bool
AsyncGeneratorResume(JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
                     CompletionKind completionKind, HandleValue argument);

} // namespace js

#endif /* vm_AsyncIteration_h */
