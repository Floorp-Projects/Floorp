/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/IterableIterator.h"
#include "mozilla/dom/Promise-inl.h"

namespace mozilla::dom {

// Due to IterableIterator being a templated class, we implement the necessary
// CC bits in a superclass that IterableIterator then inherits from. This allows
// us to put the macros outside of the header. The base class has pure virtual
// functions for Traverse/Unlink that the templated subclasses will override.

NS_IMPL_CYCLE_COLLECTION_CLASS(IterableIteratorBase)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IterableIteratorBase)
  tmp->TraverseHelper(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IterableIteratorBase)
  tmp->UnlinkHelper();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

namespace iterator_utils {

void DictReturn(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                bool aDone, JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
  RootedDictionary<IterableKeyOrValueResult> dict(aCx);
  dict.mDone = aDone;
  dict.mValue = aValue;
  JS::Rooted<JS::Value> dictValue(aCx);
  if (!ToJSValue(aCx, dict, &dictValue)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  aResult.set(dictValue);
}

void DictReturn(JSContext* aCx, JS::MutableHandle<JSObject*> aResult,
                bool aDone, JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
  JS::Rooted<JS::Value> dictValue(aCx);
  DictReturn(aCx, &dictValue, aDone, aValue, aRv);
  if (aRv.Failed()) {
    return;
  }
  aResult.set(&dictValue.toObject());
}

void KeyAndValueReturn(JSContext* aCx, JS::Handle<JS::Value> aKey,
                       JS::Handle<JS::Value> aValue,
                       JS::MutableHandle<JSObject*> aResult, ErrorResult& aRv) {
  RootedDictionary<IterableKeyAndValueResult> dict(aCx);
  dict.mDone = false;
  // Dictionary values are a Sequence, which is a FallibleTArray, so we need
  // to check returns when appending.
  if (!dict.mValue.AppendElement(aKey, mozilla::fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  if (!dict.mValue.AppendElement(aValue, mozilla::fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  JS::Rooted<JS::Value> dictValue(aCx);
  if (!ToJSValue(aCx, dict, &dictValue)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  aResult.set(&dictValue.toObject());
}

}  // namespace iterator_utils

namespace binding_detail {

static already_AddRefed<Promise> PromiseOrErr(
    Result<RefPtr<Promise>, nsresult>&& aResult, ErrorResult& aError) {
  if (aResult.isErr()) {
    aError.Throw(aResult.unwrapErr());
    return nullptr;
  }

  return aResult.unwrap().forget();
}

already_AddRefed<Promise> AsyncIterableNextImpl::NextSteps(
    JSContext* aCx, AsyncIterableIteratorBase* aObject,
    nsIGlobalObject* aGlobalObject, ErrorResult& aRv) {
  // 2. If object’s is finished is true, then:
  if (aObject->mIsFinished) {
    // 1. Let result be CreateIterResultObject(undefined, true).
    JS::Rooted<JS::Value> dict(aCx);
    iterator_utils::DictReturn(aCx, &dict, true, JS::UndefinedHandleValue, aRv);
    if (aRv.Failed()) {
      return Promise::CreateRejectedWithErrorResult(aGlobalObject, aRv);
    }

    // 2. Perform ! Call(nextPromiseCapability.[[Resolve]], undefined,
    //    «result»).
    // 3. Return nextPromiseCapability.[[Promise]].
    return Promise::Resolve(aGlobalObject, aCx, dict, aRv);
  }

  // 4. Let nextPromise be the result of getting the next iteration result with
  //    object’s target and object.
  RefPtr<Promise> nextPromise;
  {
    ErrorResult error;
    nextPromise = GetNextResult(error);

    error.WouldReportJSException();
    if (error.Failed()) {
      nextPromise = Promise::Reject(aGlobalObject, std::move(error), aRv);
    }
  }

  // 5. Let fulfillSteps be the following steps, given next:
  auto fulfillSteps = [](JSContext* aCx, JS::Handle<JS::Value> aNext,
                         ErrorResult& aRv,
                         const RefPtr<AsyncIterableIteratorBase>& aObject,
                         const nsCOMPtr<nsIGlobalObject>& aGlobalObject)
      -> already_AddRefed<Promise> {
    // 1. Set object’s ongoing promise to null.
    aObject->mOngoingPromise = nullptr;

    // 2. If next is end of iteration, then:
    JS::Rooted<JS::Value> dict(aCx);
    if (aNext.isMagic(binding_details::END_OF_ITERATION)) {
      // 1. Set object’s is finished to true.
      aObject->mIsFinished = true;
      // 2. Return CreateIterResultObject(undefined, true).
      iterator_utils::DictReturn(aCx, &dict, true, JS::UndefinedHandleValue,
                                 aRv);
      if (aRv.Failed()) {
        return nullptr;
      }
    } else {
      // 3. Otherwise, if interface has a pair asynchronously iterable
      // declaration:
      //   1. Assert: next is a value pair.
      //   2. Return the iterator result for next and kind.
      // 4. Otherwise:
      //   1. Assert: interface has a value asynchronously iterable declaration.
      //   2. Assert: next is a value of the type that appears in the
      //   declaration.
      //   3. Let value be next, converted to an ECMAScript value.
      //   4. Return CreateIterResultObject(value, false).
      iterator_utils::DictReturn(aCx, &dict, false, aNext, aRv);
      if (aRv.Failed()) {
        return nullptr;
      }
    }
    // Note that ThenCatchWithCycleCollectedArgs expects a Promise, so
    // we use Promise::Resolve here. The specs do convert this to a
    // promise too at another point, but the end result should be the
    // same.
    return Promise::Resolve(aGlobalObject, aCx, dict, aRv);
  };
  // 7. Let rejectSteps be the following steps, given reason:
  auto rejectSteps = [](JSContext* aCx, JS::Handle<JS::Value> aReason,
                        ErrorResult& aRv,
                        const RefPtr<AsyncIterableIteratorBase>& aObject,
                        const nsCOMPtr<nsIGlobalObject>& aGlobalObject) {
    // 1. Set object’s ongoing promise to null.
    aObject->mOngoingPromise = nullptr;
    // 2. Set object’s is finished to true.
    aObject->mIsFinished = true;
    // 3. Throw reason.
    return Promise::Reject(aGlobalObject, aCx, aReason, aRv);
  };
  // 9. Perform PerformPromiseThen(nextPromise, onFulfilled, onRejected,
  //    nextPromiseCapability).
  Result<RefPtr<Promise>, nsresult> result =
      nextPromise->ThenCatchWithCycleCollectedArgs(
          std::move(fulfillSteps), std::move(rejectSteps), RefPtr{aObject},
          nsCOMPtr{aGlobalObject});

  // 10. Return nextPromiseCapability.[[Promise]].
  return PromiseOrErr(std::move(result), aRv);
}

already_AddRefed<Promise> AsyncIterableNextImpl::Next(
    JSContext* aCx, AsyncIterableIteratorBase* aObject,
    nsISupports* aGlobalObject, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> globalObject = do_QueryInterface(aGlobalObject);

  // 3.7.10.2. Asynchronous iterator prototype object
  // …
  // 10. If ongoingPromise is not null, then:
  if (aObject->mOngoingPromise) {
    // 1. Let afterOngoingPromiseCapability be
    //    ! NewPromiseCapability(%Promise%).
    // 2. Let onSettled be CreateBuiltinFunction(nextSteps, « »).

    // aObject is the same object as 'this', so it's fine to capture 'this'
    // without taking a strong reference, because we already take a strong
    // reference to it through aObject.
    auto onSettled = [this](JSContext* aCx, JS::Handle<JS::Value> aValue,
                            ErrorResult& aRv,
                            const RefPtr<AsyncIterableIteratorBase>& aObject,
                            const nsCOMPtr<nsIGlobalObject>& aGlobalObject)
                         MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
                           return NextSteps(aCx, aObject, aGlobalObject, aRv);
                         };

    // 3. Perform PerformPromiseThen(ongoingPromise, onSettled, onSettled,
    //    afterOngoingPromiseCapability).
    Result<RefPtr<Promise>, nsresult> afterOngoingPromise =
        aObject->mOngoingPromise->ThenCatchWithCycleCollectedArgs(
            onSettled, onSettled, RefPtr{aObject}, std::move(globalObject));
    if (afterOngoingPromise.isErr()) {
      aRv.Throw(afterOngoingPromise.unwrapErr());
      return nullptr;
    }

    // 4. Set object’s ongoing promise to
    //    afterOngoingPromiseCapability.[[Promise]].
    aObject->mOngoingPromise = afterOngoingPromise.unwrap().forget();
  } else {
    // 11. Otherwise:
    //   1. Set object’s ongoing promise to the result of running nextSteps.
    aObject->mOngoingPromise = NextSteps(aCx, aObject, globalObject, aRv);
  }

  // 12. Return object’s ongoing promise.
  return do_AddRef(aObject->mOngoingPromise);
}

already_AddRefed<Promise> AsyncIterableReturnImpl::ReturnSteps(
    JSContext* aCx, AsyncIterableIteratorBase* aObject,
    nsIGlobalObject* aGlobalObject, JS::Handle<JS::Value> aValue,
    ErrorResult& aRv) {
  // 2. If object’s is finished is true, then:
  if (aObject->mIsFinished) {
    // 1. Let result be CreateIterResultObject(value, true).
    JS::Rooted<JS::Value> dict(aCx);
    iterator_utils::DictReturn(aCx, &dict, true, aValue, aRv);
    if (aRv.Failed()) {
      return Promise::CreateRejectedWithErrorResult(aGlobalObject, aRv);
    }

    // 2. Perform ! Call(returnPromiseCapability.[[Resolve]], undefined,
    //    «result»).
    // 3. Return returnPromiseCapability.[[Promise]].
    return Promise::Resolve(aGlobalObject, aCx, dict, aRv);
  }

  // 3. Set object’s is finished to true.
  aObject->mIsFinished = true;

  // 4. Return the result of running the asynchronous iterator return algorithm
  // for interface, given object’s target, object, and value.
  ErrorResult error;
  RefPtr<Promise> returnPromise = GetReturnPromise(aCx, aValue, error);

  error.WouldReportJSException();
  if (error.Failed()) {
    return Promise::Reject(aGlobalObject, std::move(error), aRv);
  }

  return returnPromise.forget();
}

already_AddRefed<Promise> AsyncIterableReturnImpl::Return(
    JSContext* aCx, AsyncIterableIteratorBase* aObject,
    nsISupports* aGlobalObject, JS::Handle<JS::Value> aValue,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> globalObject = do_QueryInterface(aGlobalObject);

  // 3.7.10.2. Asynchronous iterator prototype object
  // …
  RefPtr<Promise> returnStepsPromise;
  // 11. If ongoingPromise is not null, then:
  if (aObject->mOngoingPromise) {
    // 1. Let afterOngoingPromiseCapability be
    //    ! NewPromiseCapability(%Promise%).
    // 2. Let onSettled be CreateBuiltinFunction(returnSteps, « »).

    // aObject is the same object as 'this', so it's fine to capture 'this'
    // without taking a strong reference, because we already take a strong
    // reference to it through aObject.
    auto onSettled =
        [this](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv,
               const RefPtr<AsyncIterableIteratorBase>& aObject,
               const nsCOMPtr<nsIGlobalObject>& aGlobalObject,
               JS::Handle<JS::Value> aVal) MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
          return ReturnSteps(aCx, aObject, aGlobalObject, aVal, aRv);
        };

    // 3. Perform PerformPromiseThen(ongoingPromise, onSettled, onSettled,
    //    afterOngoingPromiseCapability).
    Result<RefPtr<Promise>, nsresult> afterOngoingPromise =
        aObject->mOngoingPromise->ThenCatchWithCycleCollectedArgsJS(
            onSettled, onSettled,
            std::make_tuple(RefPtr{aObject}, nsCOMPtr{globalObject}),
            std::make_tuple(aValue));
    if (afterOngoingPromise.isErr()) {
      aRv.Throw(afterOngoingPromise.unwrapErr());
      return nullptr;
    }

    // 4. Set returnStepsPromise to afterOngoingPromiseCapability.[[Promise]].
    returnStepsPromise = afterOngoingPromise.unwrap().forget();
  } else {
    // 12. Otherwise:
    //   1. Set returnStepsPromise to the result of running returnSteps.
    returnStepsPromise = ReturnSteps(aCx, aObject, globalObject, aValue, aRv);
  }

  // 13. Let fulfillSteps be the following steps:
  auto onFullFilled = [](JSContext* aCx, JS::Handle<JS::Value>,
                         ErrorResult& aRv,
                         const nsCOMPtr<nsIGlobalObject>& aGlobalObject,
                         JS::Handle<JS::Value> aVal) {
    // 1. Return CreateIterResultObject(value, true).
    JS::Rooted<JS::Value> dict(aCx);
    iterator_utils::DictReturn(aCx, &dict, true, aVal, aRv);
    return Promise::Resolve(aGlobalObject, aCx, dict, aRv);
  };

  // 14. Let onFulfilled be CreateBuiltinFunction(fulfillSteps, « »).
  // 15. Perform PerformPromiseThen(returnStepsPromise, onFulfilled, undefined,
  // returnPromiseCapability).
  Result<RefPtr<Promise>, nsresult> returnPromise =
      returnStepsPromise->ThenWithCycleCollectedArgsJS(
          onFullFilled, std::make_tuple(std::move(globalObject)),
          std::make_tuple(aValue));

  // 16. Return returnPromiseCapability.[[Promise]].
  return PromiseOrErr(std::move(returnPromise), aRv);
}

}  // namespace binding_detail

}  // namespace mozilla::dom
