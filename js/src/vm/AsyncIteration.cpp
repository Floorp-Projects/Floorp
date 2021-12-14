/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/AsyncIteration.h"

#include "builtin/Array.h"

#include "builtin/Promise.h"  // js::PromiseHandler, js::CreatePromiseObjectForAsyncGenerator, js::AsyncFromSyncIteratorMethod, js::ResolvePromiseInternal, js::RejectPromiseInternal, js::InternalAsyncGeneratorAwait
#include "js/friend/ErrorMessages.h"  // js::GetErrorMessage, JSMSG_*
#include "js/PropertySpec.h"
#include "vm/CompletionKind.h"
#include "vm/FunctionFlags.h"  // js::FunctionFlags
#include "vm/GeneratorObject.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/PlainObject.h"    // js::PlainObject
#include "vm/PromiseObject.h"  // js::PromiseObject
#include "vm/Realm.h"
#include "vm/SelfHosting.h"
#include "vm/WellKnownAtom.h"  // js_*_str

#include "vm/JSContext-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/List-inl.h"

using namespace js;

[[nodiscard]] static bool AsyncGeneratorResume(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    CompletionKind completionKind, HandleValue argument);

enum class ResumeNextKind { Enqueue, Reject, Resolve };

[[nodiscard]] static bool AsyncGeneratorDrainQueue(
    JSContext* cx, Handle<AsyncGeneratorObject*> generator);

[[nodiscard]] static bool AsyncGeneratorCompleteStepNormal(
    JSContext* cx, Handle<AsyncGeneratorObject*> generator, HandleValue value,
    bool done);

[[nodiscard]] static bool AsyncGeneratorCompleteStepThrow(
    JSContext* cx, Handle<AsyncGeneratorObject*> generator,
    HandleValue exception);

const JSClass AsyncFromSyncIteratorObject::class_ = {
    "AsyncFromSyncIteratorObject",
    JSCLASS_HAS_RESERVED_SLOTS(AsyncFromSyncIteratorObject::Slots)};

// ES2019 draft rev c012f9c70847559a1d9dc0d35d35b27fec42911e
// 25.1.4.1 CreateAsyncFromSyncIterator
JSObject* js::CreateAsyncFromSyncIterator(JSContext* cx, HandleObject iter,
                                          HandleValue nextMethod) {
  // Steps 1-3.
  return AsyncFromSyncIteratorObject::create(cx, iter, nextMethod);
}

// ES2019 draft rev c012f9c70847559a1d9dc0d35d35b27fec42911e
// 25.1.4.1 CreateAsyncFromSyncIterator
/* static */
JSObject* AsyncFromSyncIteratorObject::create(JSContext* cx, HandleObject iter,
                                              HandleValue nextMethod) {
  // Step 1.
  RootedObject proto(cx,
                     GlobalObject::getOrCreateAsyncFromSyncIteratorPrototype(
                         cx, cx->global()));
  if (!proto) {
    return nullptr;
  }

  AsyncFromSyncIteratorObject* asyncIter =
      NewObjectWithGivenProto<AsyncFromSyncIteratorObject>(cx, proto);
  if (!asyncIter) {
    return nullptr;
  }

  // Step 2.
  asyncIter->init(iter, nextMethod);

  // Step 3 (Call to 7.4.1 GetIterator).
  // 7.4.1 GetIterator, steps 1-5 are a no-op (*).
  // 7.4.1 GetIterator, steps 6-8 are implemented in bytecode.
  //
  // (*) With <https://github.com/tc39/ecma262/issues/1172> fixed.
  return asyncIter;
}

// ES2019 draft rev c012f9c70847559a1d9dc0d35d35b27fec42911e
// 25.1.4.2.1 %AsyncFromSyncIteratorPrototype%.next
static bool AsyncFromSyncIteratorNext(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return AsyncFromSyncIteratorMethod(cx, args, CompletionKind::Normal);
}

// ES2019 draft rev c012f9c70847559a1d9dc0d35d35b27fec42911e
// 25.1.4.2.2 %AsyncFromSyncIteratorPrototype%.return
static bool AsyncFromSyncIteratorReturn(JSContext* cx, unsigned argc,
                                        Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return AsyncFromSyncIteratorMethod(cx, args, CompletionKind::Return);
}

// ES2019 draft rev c012f9c70847559a1d9dc0d35d35b27fec42911e
// 25.1.4.2.3 %AsyncFromSyncIteratorPrototype%.throw
static bool AsyncFromSyncIteratorThrow(JSContext* cx, unsigned argc,
                                       Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return AsyncFromSyncIteratorMethod(cx, args, CompletionKind::Throw);
}

const JSClass AsyncGeneratorObject::class_ = {
    "AsyncGenerator",
    JSCLASS_HAS_RESERVED_SLOTS(AsyncGeneratorObject::Slots),
    &classOps_,
};

const JSClassOps AsyncGeneratorObject::classOps_ = {
    nullptr,                                   // addProperty
    nullptr,                                   // delProperty
    nullptr,                                   // enumerate
    nullptr,                                   // newEnumerate
    nullptr,                                   // resolve
    nullptr,                                   // mayResolve
    nullptr,                                   // finalize
    nullptr,                                   // call
    nullptr,                                   // hasInstance
    nullptr,                                   // construct
    CallTraceMethod<AbstractGeneratorObject>,  // trace
};

// ES 2017 draft 9.1.13.
// OrdinaryCreateFromConstructor specialized for AsyncGeneratorObjects.
static AsyncGeneratorObject* OrdinaryCreateFromConstructorAsynGen(
    JSContext* cx, HandleFunction fun) {
  // Step 1 (skipped).

  // Step 2.
  RootedValue protoVal(cx);
  if (!GetProperty(cx, fun, fun, cx->names().prototype, &protoVal)) {
    return nullptr;
  }

  RootedObject proto(cx, protoVal.isObject() ? &protoVal.toObject() : nullptr);
  if (!proto) {
    proto = GlobalObject::getOrCreateAsyncGeneratorPrototype(cx, cx->global());
    if (!proto) {
      return nullptr;
    }
  }

  // Step 3.
  return NewObjectWithGivenProto<AsyncGeneratorObject>(cx, proto);
}

/* static */
AsyncGeneratorObject* AsyncGeneratorObject::create(JSContext* cx,
                                                   HandleFunction asyncGen) {
  MOZ_ASSERT(asyncGen->isAsync() && asyncGen->isGenerator());

  AsyncGeneratorObject* asyncGenObj =
      OrdinaryCreateFromConstructorAsynGen(cx, asyncGen);
  if (!asyncGenObj) {
    return nullptr;
  }

  // ES2019 draft rev c2aad21fee7f5ddc89fdf7d3d305618ca3a13242
  // 25.5.3.2 AsyncGeneratorStart.

  // Step 7.
  asyncGenObj->setSuspendedStart();

  // Step 8.
  asyncGenObj->clearSingleQueueRequest();

  asyncGenObj->clearCachedRequest();

  return asyncGenObj;
}

/* static */
AsyncGeneratorRequest* AsyncGeneratorObject::createRequest(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    CompletionKind completionKind, HandleValue completionValue,
    Handle<PromiseObject*> promise) {
  if (!asyncGenObj->hasCachedRequest()) {
    return AsyncGeneratorRequest::create(cx, completionKind, completionValue,
                                         promise);
  }

  AsyncGeneratorRequest* request = asyncGenObj->takeCachedRequest();
  request->init(completionKind, completionValue, promise);
  return request;
}

/* static */ [[nodiscard]] bool AsyncGeneratorObject::enqueueRequest(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    Handle<AsyncGeneratorRequest*> request) {
  if (asyncGenObj->isSingleQueue()) {
    if (asyncGenObj->isSingleQueueEmpty()) {
      asyncGenObj->setSingleQueueRequest(request);
      return true;
    }

    Rooted<ListObject*> queue(cx, ListObject::create(cx));
    if (!queue) {
      return false;
    }

    RootedValue requestVal(cx, ObjectValue(*asyncGenObj->singleQueueRequest()));
    if (!queue->append(cx, requestVal)) {
      return false;
    }
    requestVal = ObjectValue(*request);
    if (!queue->append(cx, requestVal)) {
      return false;
    }

    asyncGenObj->setQueue(queue);
    return true;
  }

  Rooted<ListObject*> queue(cx, asyncGenObj->queue());
  RootedValue requestVal(cx, ObjectValue(*request));
  return queue->append(cx, requestVal);
}

/* static */
AsyncGeneratorRequest* AsyncGeneratorObject::dequeueRequest(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj) {
  if (asyncGenObj->isSingleQueue()) {
    AsyncGeneratorRequest* request = asyncGenObj->singleQueueRequest();
    asyncGenObj->clearSingleQueueRequest();
    return request;
  }

  Rooted<ListObject*> queue(cx, asyncGenObj->queue());
  return &queue->popFirstAs<AsyncGeneratorRequest>(cx);
}

/* static */
AsyncGeneratorRequest* AsyncGeneratorObject::peekRequest(
    Handle<AsyncGeneratorObject*> asyncGenObj) {
  if (asyncGenObj->isSingleQueue()) {
    return asyncGenObj->singleQueueRequest();
  }

  return &asyncGenObj->queue()->getAs<AsyncGeneratorRequest>(0);
}

const JSClass AsyncGeneratorRequest::class_ = {
    "AsyncGeneratorRequest",
    JSCLASS_HAS_RESERVED_SLOTS(AsyncGeneratorRequest::Slots)};

// ES2019 draft rev c012f9c70847559a1d9dc0d35d35b27fec42911e
// 25.5.3.1 AsyncGeneratorRequest Records
/* static */
AsyncGeneratorRequest* AsyncGeneratorRequest::create(
    JSContext* cx, CompletionKind completionKind, HandleValue completionValue,
    Handle<PromiseObject*> promise) {
  AsyncGeneratorRequest* request =
      NewObjectWithGivenProto<AsyncGeneratorRequest>(cx, nullptr);
  if (!request) {
    return nullptr;
  }

  request->init(completionKind, completionValue, promise);
  return request;
}

// ES2019 draft rev c012f9c70847559a1d9dc0d35d35b27fec42911e
// 25.5.3.2 AsyncGeneratorStart
[[nodiscard]] static bool AsyncGeneratorReturned(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    HandleValue value) {
  // Step 5.d.
  asyncGenObj->setCompleted();

  // Step 5.e (done in bytecode).
  // Step 5.f.i (implicit).

  // Step 5.g.
  if (!AsyncGeneratorCompleteStepNormal(cx, asyncGenObj, value, true)) {
    return false;
  }

  return AsyncGeneratorDrainQueue(cx, asyncGenObj);
}

// ES2019 draft rev c012f9c70847559a1d9dc0d35d35b27fec42911e
// 25.5.3.2 AsyncGeneratorStart
[[nodiscard]] static bool AsyncGeneratorThrown(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj) {
  // Step 5.d.
  asyncGenObj->setCompleted();

  // Not much we can do about uncatchable exceptions, so just bail.
  if (!cx->isExceptionPending()) {
    return false;
  }

  // Step 5.f.i.
  RootedValue value(cx);
  if (!GetAndClearException(cx, &value)) {
    return false;
  }

  // Step 5.f.ii.
  if (!AsyncGeneratorCompleteStepThrow(cx, asyncGenObj, value)) {
    return false;
  }
  return AsyncGeneratorDrainQueue(cx, asyncGenObj);
}

[[nodiscard]] static bool AsyncGeneratorUnwrapYieldResumptionAndResume(
    JSContext* cx, Handle<AsyncGeneratorObject*> generator,
    CompletionKind completionKind, HandleValue resumptionValue);

// ES2019 draft rev c012f9c70847559a1d9dc0d35d35b27fec42911e
// 25.5.3.7 AsyncGeneratorYield (partially)
// Most steps are done in generator.
[[nodiscard]] static bool AsyncGeneratorYield(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    HandleValue value) {
  // Step 5 is done in bytecode.

  // Step 6.
  asyncGenObj->setSuspendedYield();

  // Step 9.
  if (!AsyncGeneratorCompleteStepNormal(cx, asyncGenObj, value, false)) {
    return false;
  }

  if (asyncGenObj->isQueueEmpty()) {
    return true;
  }

  Rooted<AsyncGeneratorRequest*> request(
      cx, AsyncGeneratorObject::peekRequest(asyncGenObj));
  if (!request) {
    return false;
  }

  CompletionKind completionKind = request->completionKind();
  RootedValue resumptionValue(cx, request->completionValue());
  return AsyncGeneratorUnwrapYieldResumptionAndResume(
      cx, asyncGenObj, completionKind, resumptionValue);
}

// Resume the async generator when the `await` operand fulfills to `value`.
[[nodiscard]] static bool AsyncGeneratorAwaitedFulfilled(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    HandleValue value) {
  MOZ_ASSERT(asyncGenObj->isExecuting(),
             "Await fulfilled when not in 'Executing' state");

  return AsyncGeneratorResume(cx, asyncGenObj, CompletionKind::Normal, value);
}

// Resume the async generator when the `await` operand rejects with `reason`.
[[nodiscard]] static bool AsyncGeneratorAwaitedRejected(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    HandleValue reason) {
  MOZ_ASSERT(asyncGenObj->isExecuting(),
             "Await rejected when not in 'Executing' state");

  return AsyncGeneratorResume(cx, asyncGenObj, CompletionKind::Throw, reason);
}

// 6.2.3.1 Await(promise) steps 2-10 when the running execution context is
// evaluating an `await` expression in an async generator.
[[nodiscard]] static bool AsyncGeneratorAwait(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    HandleValue value) {
  return InternalAsyncGeneratorAwait(
      cx, asyncGenObj, value, PromiseHandler::AsyncGeneratorAwaitedFulfilled,
      PromiseHandler::AsyncGeneratorAwaitedRejected);
}

[[nodiscard]] static bool AsyncGeneratorCompleteStepNormal(
    JSContext* cx, Handle<AsyncGeneratorObject*> generator, HandleValue value,
    bool done) {
  MOZ_ASSERT(!generator->isQueueEmpty());

  AsyncGeneratorRequest* request =
      AsyncGeneratorObject::dequeueRequest(cx, generator);
  if (!request) {
    return false;
  }

  Rooted<PromiseObject*> resultPromise(cx, request->promise());

  generator->cacheRequest(request);

  JSObject* resultObj = CreateIterResultObject(cx, value, done);
  if (!resultObj) {
    return false;
  }

  RootedValue resultValue(cx, ObjectValue(*resultObj));

  if (!ResolvePromiseInternal(cx, resultPromise, resultValue)) {
    return false;
  }

  return true;
}

[[nodiscard]] static bool AsyncGeneratorCompleteStepThrow(
    JSContext* cx, Handle<AsyncGeneratorObject*> generator,
    HandleValue exception) {
  MOZ_ASSERT(!generator->isQueueEmpty());

  AsyncGeneratorRequest* request =
      AsyncGeneratorObject::dequeueRequest(cx, generator);
  if (!request) {
    return false;
  }

  Rooted<PromiseObject*> resultPromise(cx, request->promise());

  generator->cacheRequest(request);

  if (!RejectPromiseInternal(cx, resultPromise, exception)) {
    return false;
  }

  return true;
}

[[nodiscard]] static bool AsyncGeneratorYieldReturnAwaitedFulfilled(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    HandleValue value) {
  MOZ_ASSERT(asyncGenObj->isAwaitingYieldReturn(),
             "YieldReturn-Await fulfilled when not in "
             "'AwaitingYieldReturn' state");

  return AsyncGeneratorResume(cx, asyncGenObj, CompletionKind::Return, value);
}

[[nodiscard]] static bool AsyncGeneratorYieldReturnAwaitedRejected(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    HandleValue reason) {
  MOZ_ASSERT(
      asyncGenObj->isAwaitingYieldReturn(),
      "YieldReturn-Await rejected when not in 'AwaitingYieldReturn' state");

  return AsyncGeneratorResume(cx, asyncGenObj, CompletionKind::Throw, reason);
}

[[nodiscard]] static bool AsyncGeneratorUnwrapYieldResumptionAndResume(
    JSContext* cx, Handle<AsyncGeneratorObject*> generator,
    CompletionKind completionKind, HandleValue resumptionValue) {
  if (completionKind == CompletionKind::Return) {
    generator->setAwaitingYieldReturn();

    const PromiseHandler onFulfilled =
        PromiseHandler::AsyncGeneratorYieldReturnAwaitedFulfilled;
    const PromiseHandler onRejected =
        PromiseHandler::AsyncGeneratorYieldReturnAwaitedRejected;

    return InternalAsyncGeneratorAwait(cx, generator, resumptionValue,
                                       onFulfilled, onRejected);
  }

  return AsyncGeneratorResume(cx, generator, completionKind, resumptionValue);
}

[[nodiscard]] static bool AsyncGeneratorResumeNextReturnFulfilled(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    HandleValue value) {
  MOZ_ASSERT(asyncGenObj->isAwaitingReturn(),
             "AsyncGeneratorResumeNext-Return fulfilled when not in "
             "'AwaitingReturn' state");

  asyncGenObj->setCompleted();

  if (!AsyncGeneratorCompleteStepNormal(cx, asyncGenObj, value, true)) {
    return false;
  }
  return AsyncGeneratorDrainQueue(cx, asyncGenObj);
}

[[nodiscard]] static bool AsyncGeneratorResumeNextReturnRejected(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    HandleValue value) {
  MOZ_ASSERT(asyncGenObj->isAwaitingReturn(),
             "AsyncGeneratorResumeNext-Return rejected when not in "
             "'AwaitingReturn' state");

  // Steps 1-2.
  asyncGenObj->setCompleted();

  // Step 3.
  if (!AsyncGeneratorCompleteStepThrow(cx, asyncGenObj, value)) {
    return false;
  }
  return AsyncGeneratorDrainQueue(cx, asyncGenObj);
}

[[nodiscard]] static bool AsyncGeneratorAwaitReturn(
    JSContext* cx, Handle<AsyncGeneratorObject*> generator, HandleValue next) {
  return InternalAsyncGeneratorAwait(
      cx, generator, next,
      PromiseHandler::AsyncGeneratorResumeNextReturnFulfilled,
      PromiseHandler::AsyncGeneratorResumeNextReturnRejected);
}

[[nodiscard]] static bool AsyncGeneratorDrainQueue(
    JSContext* cx, Handle<AsyncGeneratorObject*> generator) {
  MOZ_ASSERT(generator->isCompleted());

  while (true) {
    if (generator->isQueueEmpty()) {
      return true;
    }

    Rooted<AsyncGeneratorRequest*> request(
        cx, AsyncGeneratorObject::peekRequest(generator));
    if (!request) {
      return false;
    }

    CompletionKind completionKind = request->completionKind();

    if (completionKind != CompletionKind::Normal) {
      if (generator->isCompleted()) {
        RootedValue value(cx, request->completionValue());

        if (completionKind == CompletionKind::Return) {
          generator->setAwaitingReturn();

          return AsyncGeneratorAwaitReturn(cx, generator, value);
        }

        MOZ_ASSERT(completionKind == CompletionKind::Throw);

        if (!AsyncGeneratorCompleteStepThrow(cx, generator, value)) {
          return false;
        }
        continue;
      }
    }

    if (!AsyncGeneratorCompleteStepNormal(cx, generator, UndefinedHandleValue,
                                          true)) {
      return false;
    }
  }
}

[[nodiscard]] static bool IsAsyncGeneratorValid(HandleValue asyncGenVal) {
  return asyncGenVal.isObject() &&
         asyncGenVal.toObject().canUnwrapAs<AsyncGeneratorObject>();
}

[[nodiscard]] static bool AsyncGeneratorValidateThrow(
    JSContext* cx, MutableHandleValue result) {
  Rooted<PromiseObject*> resultPromise(
      cx, CreatePromiseObjectForAsyncGenerator(cx));
  if (!resultPromise) {
    return false;
  }

  RootedValue badGeneratorError(cx);
  if (!GetTypeError(cx, JSMSG_NOT_AN_ASYNC_GENERATOR, &badGeneratorError)) {
    return false;
  }

  if (!RejectPromiseInternal(cx, resultPromise, badGeneratorError)) {
    return false;
  }

  result.setObject(*resultPromise);
  return true;
}

[[nodiscard]] static bool AsyncGeneratorEnqueue(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    CompletionKind completionKind, HandleValue completionValue,
    Handle<PromiseObject*> resultPromise) {
  Rooted<AsyncGeneratorRequest*> request(
      cx, AsyncGeneratorObject::createRequest(cx, asyncGenObj, completionKind,
                                              completionValue, resultPromise));
  if (!request) {
    return false;
  }

  return AsyncGeneratorObject::enqueueRequest(cx, asyncGenObj, request);
}

class MOZ_STACK_CLASS MaybeEnterAsyncGeneratorRealm {
  mozilla::Maybe<AutoRealm> ar_;

 public:
  MaybeEnterAsyncGeneratorRealm() = default;
  ~MaybeEnterAsyncGeneratorRealm() = default;

  // Enter async generator's realm, and wrap the method's argument value if
  // necessary.
  [[nodiscard]] bool maybeEnterAndWrap(
      JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
      MutableHandleValue value) {
    if (asyncGenObj->compartment() == cx->compartment()) {
      return true;
    }

    ar_.emplace(cx, asyncGenObj);
    return cx->compartment()->wrap(cx, value);
  }

  // Leave async generator's realm, and wrap the method's result value if
  // necessary.
  [[nodiscard]] bool maybeLeaveAndWrap(JSContext* cx,
                                       MutableHandleValue result) {
    if (!ar_) {
      return true;
    }
    ar_.reset();

    return cx->compartment()->wrap(cx, result);
  }
};

// AsyncGenerator.prototype.next
bool js::AsyncGeneratorNext(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!IsAsyncGeneratorValid(args.thisv())) {
    return AsyncGeneratorValidateThrow(cx, args.rval());
  }

  Rooted<AsyncGeneratorObject*> asyncGenObj(
      cx, &args.thisv().toObject().unwrapAs<AsyncGeneratorObject>());

  MaybeEnterAsyncGeneratorRealm maybeEnterRealm;

  RootedValue completionValue(cx, args.get(0));
  if (!maybeEnterRealm.maybeEnterAndWrap(cx, asyncGenObj, &completionValue)) {
    return false;
  }

  Rooted<PromiseObject*> resultPromise(
      cx, CreatePromiseObjectForAsyncGenerator(cx));
  if (!resultPromise) {
    return false;
  }

  MOZ_ASSERT_IF(asyncGenObj->isCompleted() || asyncGenObj->isSuspendedStart() ||
                    asyncGenObj->isSuspendedYield(),
                asyncGenObj->isQueueEmpty());

  if (asyncGenObj->isCompleted()) {
    JSObject* resultObj =
        CreateIterResultObject(cx, UndefinedHandleValue, true);
    if (!resultObj) {
      return false;
    }

    RootedValue resultValue(cx, ObjectValue(*resultObj));
    if (!ResolvePromiseInternal(cx, resultPromise, resultValue)) {
      return false;
    }
  } else {
    if (!AsyncGeneratorEnqueue(cx, asyncGenObj, CompletionKind::Normal,
                               completionValue, resultPromise)) {
      return false;
    }

    if (asyncGenObj->isSuspendedStart() || asyncGenObj->isSuspendedYield()) {
      RootedValue resumptionValue(cx, completionValue);
      if (!AsyncGeneratorResume(cx, asyncGenObj, CompletionKind::Normal,
                                resumptionValue)) {
        return false;
      }
    }
  }

  args.rval().setObject(*resultPromise);

  return maybeEnterRealm.maybeLeaveAndWrap(cx, args.rval());
}

// AsyncGenerator.prototype.return
bool js::AsyncGeneratorReturn(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!IsAsyncGeneratorValid(args.thisv())) {
    return AsyncGeneratorValidateThrow(cx, args.rval());
  }

  Rooted<AsyncGeneratorObject*> asyncGenObj(
      cx, &args.thisv().toObject().unwrapAs<AsyncGeneratorObject>());

  MaybeEnterAsyncGeneratorRealm maybeEnterRealm;

  RootedValue completionValue(cx, args.get(0));
  if (!maybeEnterRealm.maybeEnterAndWrap(cx, asyncGenObj, &completionValue)) {
    return false;
  }

  Rooted<PromiseObject*> resultPromise(
      cx, CreatePromiseObjectForAsyncGenerator(cx));
  if (!resultPromise) {
    return false;
  }

  MOZ_ASSERT_IF(asyncGenObj->isCompleted() || asyncGenObj->isSuspendedStart() ||
                    asyncGenObj->isSuspendedYield(),
                asyncGenObj->isQueueEmpty());

  if (!AsyncGeneratorEnqueue(cx, asyncGenObj, CompletionKind::Return,
                             completionValue, resultPromise)) {
    return false;
  }

  if (asyncGenObj->isSuspendedStart()) {
    asyncGenObj->setCompleted();
  }

  if (asyncGenObj->isCompleted()) {
    if (!AsyncGeneratorDrainQueue(cx, asyncGenObj)) {
      return false;
    }
  } else if (asyncGenObj->isSuspendedYield()) {
    if (!AsyncGeneratorUnwrapYieldResumptionAndResume(
            cx, asyncGenObj, CompletionKind::Return, completionValue)) {
      return false;
    }
  }

  args.rval().setObject(*resultPromise);

  return maybeEnterRealm.maybeLeaveAndWrap(cx, args.rval());
}

// AsyncGenerator.prototype.throw
bool js::AsyncGeneratorThrow(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!IsAsyncGeneratorValid(args.thisv())) {
    return AsyncGeneratorValidateThrow(cx, args.rval());
  }

  Rooted<AsyncGeneratorObject*> asyncGenObj(
      cx, &args.thisv().toObject().unwrapAs<AsyncGeneratorObject>());

  MaybeEnterAsyncGeneratorRealm maybeEnterRealm;

  RootedValue completionValue(cx, args.get(0));
  if (!maybeEnterRealm.maybeEnterAndWrap(cx, asyncGenObj, &completionValue)) {
    return false;
  }

  Rooted<PromiseObject*> resultPromise(
      cx, CreatePromiseObjectForAsyncGenerator(cx));
  if (!resultPromise) {
    return false;
  }

  MOZ_ASSERT_IF(asyncGenObj->isCompleted() || asyncGenObj->isSuspendedStart() ||
                    asyncGenObj->isSuspendedYield(),
                asyncGenObj->isQueueEmpty());

  if (!AsyncGeneratorEnqueue(cx, asyncGenObj, CompletionKind::Throw,
                             completionValue, resultPromise)) {
    return false;
  }

  if (asyncGenObj->isSuspendedStart()) {
    asyncGenObj->setCompleted();
  }

  if (asyncGenObj->isCompleted()) {
    if (!AsyncGeneratorDrainQueue(cx, asyncGenObj)) {
      return false;
    }
  } else if (asyncGenObj->isSuspendedYield()) {
    if (!AsyncGeneratorResume(cx, asyncGenObj, CompletionKind::Throw,
                              completionValue)) {
      return false;
    }
  }

  args.rval().setObject(*resultPromise);

  return maybeEnterRealm.maybeLeaveAndWrap(cx, args.rval());
}

// ES2019 draft rev c012f9c70847559a1d9dc0d35d35b27fec42911e
// 6.2.3.1 Await, steps 2-9.
// 14.4.13 RS: Evaluation, yield*, steps 7.a.vi, 7.b.ii.7, 7.c.ix.
// 25.5.3.2 AsyncGeneratorStart, steps 5.d-g.
// 25.5.3.5 AsyncGeneratorResumeNext, steps 12-20.
// 25.5.3.7 AsyncGeneratorYield, steps 5-6, 9.
//
// Note: Execution context switching is handled in generator.
[[nodiscard]] static bool AsyncGeneratorResume(
    JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
    CompletionKind completionKind, HandleValue argument) {
  MOZ_ASSERT(!asyncGenObj->isClosed(),
             "closed generator when resuming async generator");
  MOZ_ASSERT(asyncGenObj->isSuspended(),
             "non-suspended generator when resuming async generator");

  asyncGenObj->setExecuting();

  // 25.5.3.5, steps 12-14, 16-20.
  HandlePropertyName funName = completionKind == CompletionKind::Normal
                                   ? cx->names().AsyncGeneratorNext
                               : completionKind == CompletionKind::Throw
                                   ? cx->names().AsyncGeneratorThrow
                                   : cx->names().AsyncGeneratorReturn;
  FixedInvokeArgs<1> args(cx);
  args[0].set(argument);
  RootedValue thisOrRval(cx, ObjectValue(*asyncGenObj));
  if (!CallSelfHostedFunction(cx, funName, thisOrRval, args, &thisOrRval)) {
    // 25.5.3.2, steps 5.f, 5.g.
    if (!asyncGenObj->isClosed()) {
      asyncGenObj->setClosed();
    }
    return AsyncGeneratorThrown(cx, asyncGenObj);
  }

  // 6.2.3.1, steps 2-9.
  if (asyncGenObj->isAfterAwait()) {
    return AsyncGeneratorAwait(cx, asyncGenObj, thisOrRval);
  }

  // 25.5.3.7, steps 5-6, 9.
  if (asyncGenObj->isAfterYield()) {
    return AsyncGeneratorYield(cx, asyncGenObj, thisOrRval);
  }

  // 25.5.3.2, steps 5.d-g.
  return AsyncGeneratorReturned(cx, asyncGenObj, thisOrRval);
}

static const JSFunctionSpec async_iterator_proto_methods[] = {
    JS_SELF_HOSTED_SYM_FN(asyncIterator, "AsyncIteratorIdentity", 0, 0),
    JS_FS_END};

static const JSFunctionSpec async_iterator_proto_methods_with_helpers[] = {
    JS_SELF_HOSTED_FN("map", "AsyncIteratorMap", 1, 0),
    JS_SELF_HOSTED_FN("filter", "AsyncIteratorFilter", 1, 0),
    JS_SELF_HOSTED_FN("take", "AsyncIteratorTake", 1, 0),
    JS_SELF_HOSTED_FN("drop", "AsyncIteratorDrop", 1, 0),
    JS_SELF_HOSTED_FN("asIndexedPairs", "AsyncIteratorAsIndexedPairs", 0, 0),
    JS_SELF_HOSTED_FN("flatMap", "AsyncIteratorFlatMap", 1, 0),
    JS_SELF_HOSTED_FN("reduce", "AsyncIteratorReduce", 1, 0),
    JS_SELF_HOSTED_FN("toArray", "AsyncIteratorToArray", 0, 0),
    JS_SELF_HOSTED_FN("forEach", "AsyncIteratorForEach", 1, 0),
    JS_SELF_HOSTED_FN("some", "AsyncIteratorSome", 1, 0),
    JS_SELF_HOSTED_FN("every", "AsyncIteratorEvery", 1, 0),
    JS_SELF_HOSTED_FN("find", "AsyncIteratorFind", 1, 0),
    JS_SELF_HOSTED_SYM_FN(asyncIterator, "AsyncIteratorIdentity", 0, 0),
    JS_FS_END};

static const JSFunctionSpec async_from_sync_iter_methods[] = {
    JS_FN("next", AsyncFromSyncIteratorNext, 1, 0),
    JS_FN("throw", AsyncFromSyncIteratorThrow, 1, 0),
    JS_FN("return", AsyncFromSyncIteratorReturn, 1, 0), JS_FS_END};

static const JSFunctionSpec async_generator_methods[] = {
    JS_FN("next", js::AsyncGeneratorNext, 1, 0),
    JS_FN("throw", js::AsyncGeneratorThrow, 1, 0),
    JS_FN("return", js::AsyncGeneratorReturn, 1, 0), JS_FS_END};

bool GlobalObject::initAsyncIteratorProto(JSContext* cx,
                                          Handle<GlobalObject*> global) {
  if (global->hasBuiltinProto(ProtoKind::AsyncIteratorProto)) {
    return true;
  }

  // 25.1.3 The %AsyncIteratorPrototype% Object
  RootedObject asyncIterProto(
      cx, GlobalObject::createBlankPrototype<PlainObject>(cx, global));
  if (!asyncIterProto) {
    return false;
  }
  if (!DefinePropertiesAndFunctions(cx, asyncIterProto, nullptr,
                                    async_iterator_proto_methods)) {
    return false;
  }

  global->initBuiltinProto(ProtoKind::AsyncIteratorProto, asyncIterProto);
  return true;
}

bool GlobalObject::initAsyncFromSyncIteratorProto(
    JSContext* cx, Handle<GlobalObject*> global) {
  if (global->hasBuiltinProto(ProtoKind::AsyncFromSyncIteratorProto)) {
    return true;
  }

  RootedObject asyncIterProto(
      cx, GlobalObject::getOrCreateAsyncIteratorPrototype(cx, global));
  if (!asyncIterProto) {
    return false;
  }

  // 25.1.4.2 The %AsyncFromSyncIteratorPrototype% Object
  RootedObject asyncFromSyncIterProto(
      cx, GlobalObject::createBlankPrototypeInheriting(cx, &PlainObject::class_,
                                                       asyncIterProto));
  if (!asyncFromSyncIterProto) {
    return false;
  }
  if (!DefinePropertiesAndFunctions(cx, asyncFromSyncIterProto, nullptr,
                                    async_from_sync_iter_methods) ||
      !DefineToStringTag(cx, asyncFromSyncIterProto,
                         cx->names().AsyncFromSyncIterator)) {
    return false;
  }

  global->initBuiltinProto(ProtoKind::AsyncFromSyncIteratorProto,
                           asyncFromSyncIterProto);
  return true;
}

static JSObject* CreateAsyncGeneratorFunction(JSContext* cx, JSProtoKey key) {
  RootedObject proto(
      cx, GlobalObject::getOrCreateFunctionConstructor(cx, cx->global()));
  if (!proto) {
    return nullptr;
  }
  HandlePropertyName name = cx->names().AsyncGeneratorFunction;

  // 25.3.1 The AsyncGeneratorFunction Constructor
  return NewFunctionWithProto(cx, AsyncGeneratorConstructor, 1,
                              FunctionFlags::NATIVE_CTOR, nullptr, name, proto,
                              gc::AllocKind::FUNCTION, TenuredObject);
}

static JSObject* CreateAsyncGeneratorFunctionPrototype(JSContext* cx,
                                                       JSProtoKey key) {
  return NewTenuredObjectWithFunctionPrototype(cx, cx->global());
}

static bool AsyncGeneratorFunctionClassFinish(JSContext* cx,
                                              HandleObject asyncGenFunction,
                                              HandleObject asyncGenerator) {
  Handle<GlobalObject*> global = cx->global();

  // Change the "constructor" property to non-writable before adding any other
  // properties, so it's still the last property and can be modified without a
  // dictionary-mode transition.
  MOZ_ASSERT(asyncGenerator->as<NativeObject>().getLastProperty().key() ==
             NameToId(cx->names().constructor));
  MOZ_ASSERT(!asyncGenerator->as<NativeObject>().inDictionaryMode());

  RootedValue asyncGenFunctionVal(cx, ObjectValue(*asyncGenFunction));
  if (!DefineDataProperty(cx, asyncGenerator, cx->names().constructor,
                          asyncGenFunctionVal, JSPROP_READONLY)) {
    return false;
  }
  MOZ_ASSERT(!asyncGenerator->as<NativeObject>().inDictionaryMode());

  RootedObject asyncIterProto(
      cx, GlobalObject::getOrCreateAsyncIteratorPrototype(cx, global));
  if (!asyncIterProto) {
    return false;
  }

  // 25.5 AsyncGenerator Objects
  RootedObject asyncGenProto(cx, GlobalObject::createBlankPrototypeInheriting(
                                     cx, &PlainObject::class_, asyncIterProto));
  if (!asyncGenProto) {
    return false;
  }
  if (!DefinePropertiesAndFunctions(cx, asyncGenProto, nullptr,
                                    async_generator_methods) ||
      !DefineToStringTag(cx, asyncGenProto, cx->names().AsyncGenerator)) {
    return false;
  }

  // 25.3.3 Properties of the AsyncGeneratorFunction Prototype Object
  if (!LinkConstructorAndPrototype(cx, asyncGenerator, asyncGenProto,
                                   JSPROP_READONLY, JSPROP_READONLY) ||
      !DefineToStringTag(cx, asyncGenerator,
                         cx->names().AsyncGeneratorFunction)) {
    return false;
  }

  global->setAsyncGeneratorPrototype(asyncGenProto);

  return true;
}

static const ClassSpec AsyncGeneratorFunctionClassSpec = {
    CreateAsyncGeneratorFunction,
    CreateAsyncGeneratorFunctionPrototype,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    AsyncGeneratorFunctionClassFinish,
    ClassSpec::DontDefineConstructor};

const JSClass js::AsyncGeneratorFunctionClass = {
    "AsyncGeneratorFunction", 0, JS_NULL_CLASS_OPS,
    &AsyncGeneratorFunctionClassSpec};

[[nodiscard]] bool js::AsyncGeneratorPromiseReactionJob(
    JSContext* cx, PromiseHandler handler,
    Handle<AsyncGeneratorObject*> asyncGenObj, HandleValue argument) {
  // Await's handlers don't return a value, nor throw any exceptions.
  // They fail only on OOM.
  switch (handler) {
    case PromiseHandler::AsyncGeneratorAwaitedFulfilled:
      return AsyncGeneratorAwaitedFulfilled(cx, asyncGenObj, argument);

    case PromiseHandler::AsyncGeneratorAwaitedRejected:
      return AsyncGeneratorAwaitedRejected(cx, asyncGenObj, argument);

    case PromiseHandler::AsyncGeneratorResumeNextReturnFulfilled:
      return AsyncGeneratorResumeNextReturnFulfilled(cx, asyncGenObj, argument);

    case PromiseHandler::AsyncGeneratorResumeNextReturnRejected:
      return AsyncGeneratorResumeNextReturnRejected(cx, asyncGenObj, argument);

    case PromiseHandler::AsyncGeneratorYieldReturnAwaitedFulfilled:
      return AsyncGeneratorYieldReturnAwaitedFulfilled(cx, asyncGenObj,
                                                       argument);

    case PromiseHandler::AsyncGeneratorYieldReturnAwaitedRejected:
      return AsyncGeneratorYieldReturnAwaitedRejected(cx, asyncGenObj,
                                                      argument);

    default:
      MOZ_CRASH("Bad handler in AsyncGeneratorPromiseReactionJob");
  }
}

// https://tc39.es/proposal-iterator-helpers/#sec-asynciterator as of revision
// 8f10db5.
static bool AsyncIteratorConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, js_AsyncIterator_str)) {
    return false;
  }
  // Throw TypeError if NewTarget is the active function object, preventing the
  // Iterator constructor from being used directly.
  if (args.callee() == args.newTarget().toObject()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BOGUS_CONSTRUCTOR, js_AsyncIterator_str);
    return false;
  }

  // Step 2.
  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_AsyncIterator,
                                          &proto)) {
    return false;
  }

  JSObject* obj = NewObjectWithClassProto<AsyncIteratorObject>(cx, proto);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

static const ClassSpec AsyncIteratorObjectClassSpec = {
    GenericCreateConstructor<AsyncIteratorConstructor, 0,
                             gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<AsyncIteratorObject>,
    nullptr,
    nullptr,
    async_iterator_proto_methods_with_helpers,
    nullptr,
    nullptr,
};

const JSClass AsyncIteratorObject::class_ = {
    js_AsyncIterator_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_AsyncIterator),
    JS_NULL_CLASS_OPS,
    &AsyncIteratorObjectClassSpec,
};

const JSClass AsyncIteratorObject::protoClass_ = {
    "AsyncIterator.prototype",
    JSCLASS_HAS_CACHED_PROTO(JSProto_AsyncIterator),
    JS_NULL_CLASS_OPS,
    &AsyncIteratorObjectClassSpec,
};

// Iterator Helper proposal
static const JSFunctionSpec async_iterator_helper_methods[] = {
    JS_SELF_HOSTED_FN("next", "AsyncIteratorHelperNext", 1, 0),
    JS_SELF_HOSTED_FN("return", "AsyncIteratorHelperReturn", 1, 0),
    JS_SELF_HOSTED_FN("throw", "AsyncIteratorHelperThrow", 1, 0),
    JS_FS_END,
};

static const JSClass AsyncIteratorHelperPrototypeClass = {
    "Async Iterator Helper", 0};

const JSClass AsyncIteratorHelperObject::class_ = {
    "Async Iterator Helper",
    JSCLASS_HAS_RESERVED_SLOTS(AsyncIteratorHelperObject::SlotCount),
};

/* static */
NativeObject* GlobalObject::getOrCreateAsyncIteratorHelperPrototype(
    JSContext* cx, Handle<GlobalObject*> global) {
  return MaybeNativeObject(
      getOrCreateBuiltinProto(cx, global, ProtoKind::AsyncIteratorHelperProto,
                              initAsyncIteratorHelperProto));
}

/* static */
bool GlobalObject::initAsyncIteratorHelperProto(JSContext* cx,
                                                Handle<GlobalObject*> global) {
  if (global->hasBuiltinProto(ProtoKind::AsyncIteratorHelperProto)) {
    return true;
  }

  RootedObject asyncIterProto(
      cx, GlobalObject::getOrCreateAsyncIteratorPrototype(cx, global));
  if (!asyncIterProto) {
    return false;
  }

  RootedObject asyncIteratorHelperProto(
      cx, GlobalObject::createBlankPrototypeInheriting(
              cx, &AsyncIteratorHelperPrototypeClass, asyncIterProto));
  if (!asyncIteratorHelperProto) {
    return false;
  }
  if (!DefinePropertiesAndFunctions(cx, asyncIteratorHelperProto, nullptr,
                                    async_iterator_helper_methods)) {
    return false;
  }

  global->initBuiltinProto(ProtoKind::AsyncIteratorHelperProto,
                           asyncIteratorHelperProto);
  return true;
}

AsyncIteratorHelperObject* js::NewAsyncIteratorHelper(JSContext* cx) {
  RootedObject proto(cx, GlobalObject::getOrCreateAsyncIteratorHelperPrototype(
                             cx, cx->global()));
  if (!proto) {
    return nullptr;
  }
  return NewObjectWithGivenProto<AsyncIteratorHelperObject>(cx, proto);
}
