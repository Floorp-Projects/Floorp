/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"

#include "jsfriendapi.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/OwningNonNull.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/Preferences.h"
#include "PromiseCallback.h"
#include "PromiseNativeHandler.h"
#include "PromiseWorkerProxy.h"
#include "nsContentUtils.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "nsJSPrincipals.h"
#include "nsJSUtils.h"
#include "nsPIDOMWindow.h"
#include "nsJSEnvironment.h"
#include "nsIScriptObjectPrincipal.h"
#include "xpcpublic.h"
#include "nsGlobalWindow.h"

namespace mozilla {
namespace dom {

using namespace workers;

NS_IMPL_ISUPPORTS0(PromiseNativeHandler)

// This class processes the promise's callbacks with promise's result.
class PromiseCallbackTask MOZ_FINAL : public nsRunnable
{
public:
  PromiseCallbackTask(Promise* aPromise,
                      PromiseCallback* aCallback,
                      const JS::Value& aValue)
    : mPromise(aPromise)
    , mCallback(aCallback)
    , mValue(CycleCollectedJSRuntime::Get()->Runtime(), aValue)
  {
    MOZ_ASSERT(aPromise);
    MOZ_ASSERT(aCallback);
    MOZ_COUNT_CTOR(PromiseCallbackTask);
  }

  virtual
  ~PromiseCallbackTask()
  {
    NS_ASSERT_OWNINGTHREAD(PromiseCallbackTask);
    MOZ_COUNT_DTOR(PromiseCallbackTask);
  }

protected:
  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    NS_ASSERT_OWNINGTHREAD(PromiseCallbackTask);
    ThreadsafeAutoJSContext cx;
    JS::Rooted<JSObject*> wrapper(cx, mPromise->GetWrapper());
    MOZ_ASSERT(wrapper); // It was preserved!
    JSAutoCompartment ac(cx, wrapper);

    JS::Rooted<JS::Value> value(cx, mValue);
    if (!MaybeWrapValue(cx, &value)) {
      NS_WARNING("Failed to wrap value into the right compartment.");
      JS_ClearPendingException(cx);
      return NS_OK;
    }

    mCallback->Call(cx, value);

    return NS_OK;
  }

private:
  nsRefPtr<Promise> mPromise;
  nsRefPtr<PromiseCallback> mCallback;
  JS::PersistentRooted<JS::Value> mValue;
  NS_DECL_OWNINGTHREAD;
};

enum {
  SLOT_PROMISE = 0,
  SLOT_DATA
};

/*
 * Utilities for thenable callbacks.
 *
 * A thenable is a { then: function(resolve, reject) { } }.
 * `then` is called with a resolve and reject callback pair.
 * Since only one of these should be called at most once (first call wins), the
 * two keep a reference to each other in SLOT_DATA. When either of them is
 * called, the references are cleared. Further calls are ignored.
 */
namespace {
void
LinkThenableCallables(JSContext* aCx, JS::Handle<JSObject*> aResolveFunc,
                      JS::Handle<JSObject*> aRejectFunc)
{
  js::SetFunctionNativeReserved(aResolveFunc, SLOT_DATA,
                                JS::ObjectValue(*aRejectFunc));
  js::SetFunctionNativeReserved(aRejectFunc, SLOT_DATA,
                                JS::ObjectValue(*aResolveFunc));
}

/*
 * Returns false if callback was already called before, otherwise breaks the
 * links and returns true.
 */
bool
MarkAsCalledIfNotCalledBefore(JSContext* aCx, JS::Handle<JSObject*> aFunc)
{
  JS::Value otherFuncVal = js::GetFunctionNativeReserved(aFunc, SLOT_DATA);

  if (!otherFuncVal.isObject()) {
    return false;
  }

  JSObject* otherFuncObj = &otherFuncVal.toObject();
  MOZ_ASSERT(js::GetFunctionNativeReserved(otherFuncObj, SLOT_DATA).isObject());

  // Break both references.
  js::SetFunctionNativeReserved(aFunc, SLOT_DATA, JS::UndefinedValue());
  js::SetFunctionNativeReserved(otherFuncObj, SLOT_DATA, JS::UndefinedValue());

  return true;
}

Promise*
GetPromise(JSContext* aCx, JS::Handle<JSObject*> aFunc)
{
  JS::Value promiseVal = js::GetFunctionNativeReserved(aFunc, SLOT_PROMISE);

  MOZ_ASSERT(promiseVal.isObject());

  Promise* promise;
  UNWRAP_OBJECT(Promise, &promiseVal.toObject(), promise);
  return promise;
}
};

// Main thread runnable to resolve thenables.
// Equivalent to the specification's ResolvePromiseViaThenableTask.
class ThenableResolverTask MOZ_FINAL : public nsRunnable
{
public:
  ThenableResolverTask(Promise* aPromise,
                        JS::Handle<JSObject*> aThenable,
                        PromiseInit* aThen)
    : mPromise(aPromise)
    , mThenable(CycleCollectedJSRuntime::Get()->Runtime(), aThenable)
    , mThen(aThen)
  {
    MOZ_ASSERT(aPromise);
    MOZ_COUNT_CTOR(ThenableResolverTask);
  }

  virtual
  ~ThenableResolverTask()
  {
    NS_ASSERT_OWNINGTHREAD(ThenableResolverTask);
    MOZ_COUNT_DTOR(ThenableResolverTask);
  }

protected:
  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    NS_ASSERT_OWNINGTHREAD(ThenableResolverTask);
    ThreadsafeAutoJSContext cx;
    JS::Rooted<JSObject*> wrapper(cx, mPromise->GetWrapper());
    MOZ_ASSERT(wrapper); // It was preserved!
    JSAutoCompartment ac(cx, wrapper);

    JS::Rooted<JSObject*> resolveFunc(cx,
      mPromise->CreateThenableFunction(cx, mPromise, PromiseCallback::Resolve));

    if (!resolveFunc) {
      mPromise->HandleException(cx);
      return NS_OK;
    }

    JS::Rooted<JSObject*> rejectFunc(cx,
      mPromise->CreateThenableFunction(cx, mPromise, PromiseCallback::Reject));
    if (!rejectFunc) {
      mPromise->HandleException(cx);
      return NS_OK;
    }

    LinkThenableCallables(cx, resolveFunc, rejectFunc);

    ErrorResult rv;

    JS::Rooted<JSObject*> rootedThenable(cx, mThenable);

    mThen->Call(rootedThenable, resolveFunc, rejectFunc, rv,
                CallbackObject::eRethrowExceptions);

    rv.WouldReportJSException();
    if (rv.IsJSException()) {
      JS::Rooted<JS::Value> exn(cx);
      rv.StealJSException(cx, &exn);

      bool couldMarkAsCalled = MarkAsCalledIfNotCalledBefore(cx, resolveFunc);

      // If we could mark as called, neither of the callbacks had been called
      // when the exception was thrown. So we can reject the Promise.
      if (couldMarkAsCalled) {
        bool ok = JS_WrapValue(cx, &exn);
        MOZ_ASSERT(ok);
        if (!ok) {
          NS_WARNING("Failed to wrap value into the right compartment.");
        }

        mPromise->RejectInternal(cx, exn);
      }
      // At least one of resolveFunc or rejectFunc have been called, so ignore
      // the exception. FIXME(nsm): This should be reported to the error
      // console though, for debugging.
    }

    return NS_OK;
  }

private:
  nsRefPtr<Promise> mPromise;
  JS::PersistentRooted<JSObject*> mThenable;
  nsRefPtr<PromiseInit> mThen;
  NS_DECL_OWNINGTHREAD;
};

// Promise

NS_IMPL_CYCLE_COLLECTION_CLASS(Promise)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Promise)
  tmp->MaybeReportRejectedOnce();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mResolveCallbacks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRejectCallbacks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Promise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResolveCallbacks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRejectCallbacks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Promise)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mResult)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mAllocationStack)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mRejectionStack)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mFullfillmentStack)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Promise)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Promise)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Promise)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Promise::Promise(nsIGlobalObject* aGlobal)
  : mGlobal(aGlobal)
  , mResult(JS::UndefinedValue())
  , mAllocationStack(nullptr)
  , mRejectionStack(nullptr)
  , mFullfillmentStack(nullptr)
  , mState(Pending)
  , mHadRejectCallback(false)
  , mResolvePending(false)
{
  MOZ_ASSERT(mGlobal);

  mozilla::HoldJSObjects(this);

  mCreationTimestamp = TimeStamp::Now();
}

Promise::~Promise()
{
  MaybeReportRejectedOnce();
  mozilla::DropJSObjects(this);
}

JSObject*
Promise::WrapObject(JSContext* aCx)
{
  return PromiseBinding::Wrap(aCx, this);
}

already_AddRefed<Promise>
Promise::Create(nsIGlobalObject* aGlobal, ErrorResult& aRv)
{
  nsRefPtr<Promise> p = new Promise(aGlobal);
  p->CreateWrapper(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return p.forget();
}

void
Promise::CreateWrapper(ErrorResult& aRv)
{
  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobal)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  JSContext* cx = jsapi.cx();

  JS::Rooted<JS::Value> ignored(cx);
  if (!WrapNewBindingObject(cx, this, &ignored)) {
    JS_ClearPendingException(cx);
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  dom::PreserveWrapper(this);

  // Now grab our allocation stack
  if (!CaptureStack(cx, mAllocationStack)) {
    JS_ClearPendingException(cx);
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
}

void
Promise::MaybeResolve(JSContext* aCx,
                      JS::Handle<JS::Value> aValue)
{
  MaybeResolveInternal(aCx, aValue);
}

void
Promise::MaybeReject(JSContext* aCx,
                     JS::Handle<JS::Value> aValue)
{
  MaybeRejectInternal(aCx, aValue);
}

/* static */ bool
Promise::JSCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);

  JS::Rooted<JS::Value> v(aCx,
                          js::GetFunctionNativeReserved(&args.callee(),
                                                        SLOT_PROMISE));
  MOZ_ASSERT(v.isObject());

  Promise* promise;
  if (NS_FAILED(UNWRAP_OBJECT(Promise, &v.toObject(), promise))) {
    return Throw(aCx, NS_ERROR_UNEXPECTED);
  }

  v = js::GetFunctionNativeReserved(&args.callee(), SLOT_DATA);
  PromiseCallback::Task task = static_cast<PromiseCallback::Task>(v.toInt32());

  if (task == PromiseCallback::Resolve) {
    promise->MaybeResolveInternal(aCx, args.get(0));
    if (!promise->CaptureStack(aCx, promise->mFullfillmentStack)) {
      return false;
    }
  } else {
    promise->MaybeRejectInternal(aCx, args.get(0));
    if (!promise->CaptureStack(aCx, promise->mRejectionStack)) {
      return false;
    }
  }

  return true;
}

/*
 * Common bits of (JSCallbackThenableResolver/JSCallbackThenableRejecter).
 * Resolves/rejects the Promise if it is ok to do so, based on whether either of
 * the callbacks have been called before or not.
 */
/* static */ bool
Promise::ThenableResolverCommon(JSContext* aCx, uint32_t aTask,
                                unsigned aArgc, JS::Value* aVp)
{
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);
  JS::Rooted<JSObject*> thisFunc(aCx, &args.callee());
  if (!MarkAsCalledIfNotCalledBefore(aCx, thisFunc)) {
    // A function from this pair has been called before.
    return true;
  }

  Promise* promise = GetPromise(aCx, thisFunc);
  MOZ_ASSERT(promise);

  if (aTask == PromiseCallback::Resolve) {
    promise->ResolveInternal(aCx, args.get(0));
  } else {
    promise->RejectInternal(aCx, args.get(0));
  }
  return true;
}

/* static */ bool
Promise::JSCallbackThenableResolver(JSContext* aCx,
                                    unsigned aArgc, JS::Value* aVp)
{
  return ThenableResolverCommon(aCx, PromiseCallback::Resolve, aArgc, aVp);
}

/* static */ bool
Promise::JSCallbackThenableRejecter(JSContext* aCx,
                                    unsigned aArgc, JS::Value* aVp)
{
  return ThenableResolverCommon(aCx, PromiseCallback::Reject, aArgc, aVp);
}

/* static */ JSObject*
Promise::CreateFunction(JSContext* aCx, JSObject* aParent, Promise* aPromise,
                        int32_t aTask)
{
  JSFunction* func = js::NewFunctionWithReserved(aCx, JSCallback,
                                                 1 /* nargs */, 0 /* flags */,
                                                 aParent, nullptr);
  if (!func) {
    return nullptr;
  }

  JS::Rooted<JSObject*> obj(aCx, JS_GetFunctionObject(func));

  JS::Rooted<JS::Value> promiseObj(aCx);
  if (!dom::WrapNewBindingObject(aCx, aPromise, &promiseObj)) {
    return nullptr;
  }

  js::SetFunctionNativeReserved(obj, SLOT_PROMISE, promiseObj);
  js::SetFunctionNativeReserved(obj, SLOT_DATA, JS::Int32Value(aTask));

  return obj;
}

/* static */ JSObject*
Promise::CreateThenableFunction(JSContext* aCx, Promise* aPromise, uint32_t aTask)
{
  JSNative whichFunc =
    aTask == PromiseCallback::Resolve ? JSCallbackThenableResolver :
                                        JSCallbackThenableRejecter ;

  JSFunction* func = js::NewFunctionWithReserved(aCx, whichFunc,
                                                 1 /* nargs */, 0 /* flags */,
                                                 nullptr, nullptr);
  if (!func) {
    return nullptr;
  }

  JS::Rooted<JSObject*> obj(aCx, JS_GetFunctionObject(func));

  JS::Rooted<JS::Value> promiseObj(aCx);
  if (!dom::WrapNewBindingObject(aCx, aPromise, &promiseObj)) {
    return nullptr;
  }

  js::SetFunctionNativeReserved(obj, SLOT_PROMISE, promiseObj);

  return obj;
}

/* static */ already_AddRefed<Promise>
Promise::Constructor(const GlobalObject& aGlobal,
                     PromiseInit& aInit, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global;
  global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  promise->CallInitFunction(aGlobal, aInit, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return promise.forget();
}

void
Promise::CallInitFunction(const GlobalObject& aGlobal,
                          PromiseInit& aInit, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();

  JS::Rooted<JSObject*> resolveFunc(cx,
                                    CreateFunction(cx, aGlobal.Get(), this,
                                                   PromiseCallback::Resolve));
  if (!resolveFunc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  JS::Rooted<JSObject*> rejectFunc(cx,
                                   CreateFunction(cx, aGlobal.Get(), this,
                                                  PromiseCallback::Reject));
  if (!rejectFunc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  aInit.Call(resolveFunc, rejectFunc, aRv, CallbackObject::eRethrowExceptions);
  aRv.WouldReportJSException();

  if (aRv.IsJSException()) {
    JS::Rooted<JS::Value> value(cx);
    aRv.StealJSException(cx, &value);

    // we want the same behavior as this JS implementation:
    // function Promise(arg) { try { arg(a, b); } catch (e) { this.reject(e); }}
    if (!JS_WrapValue(cx, &value)) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }

    MaybeRejectInternal(cx, value);
  }
}

/* static */ already_AddRefed<Promise>
Promise::Resolve(const GlobalObject& aGlobal,
                 JS::Handle<JS::Value> aValue, ErrorResult& aRv)
{
  // If a Promise was passed, just return it.
  if (aValue.isObject()) {
    JS::Rooted<JSObject*> valueObj(aGlobal.Context(), &aValue.toObject());
    Promise* nextPromise;
    nsresult rv = UNWRAP_OBJECT(Promise, valueObj, nextPromise);

    if (NS_SUCCEEDED(rv)) {
      nsRefPtr<Promise> addRefed = nextPromise;
      return addRefed.forget();
    }
  }

  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> p = Resolve(global, aGlobal.Context(), aValue, aRv);
  if (p) {
    p->mFullfillmentStack = p->mAllocationStack;
  }
  return p.forget();
}

/* static */ already_AddRefed<Promise>
Promise::Resolve(nsIGlobalObject* aGlobal, JSContext* aCx,
                 JS::Handle<JS::Value> aValue, ErrorResult& aRv)
{
  nsRefPtr<Promise> promise = Create(aGlobal, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  promise->MaybeResolveInternal(aCx, aValue);
  return promise.forget();
}

/* static */ already_AddRefed<Promise>
Promise::Reject(const GlobalObject& aGlobal,
                JS::Handle<JS::Value> aValue, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> p = Reject(global, aGlobal.Context(), aValue, aRv);
  if (p) {
    p->mRejectionStack = p->mAllocationStack;
  }
  return p.forget();
}

/* static */ already_AddRefed<Promise>
Promise::Reject(nsIGlobalObject* aGlobal, JSContext* aCx,
                JS::Handle<JS::Value> aValue, ErrorResult& aRv)
{
  nsRefPtr<Promise> promise = Create(aGlobal, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  promise->MaybeRejectInternal(aCx, aValue);
  return promise.forget();
}

already_AddRefed<Promise>
Promise::Then(JSContext* aCx, AnyCallback* aResolveCallback,
              AnyCallback* aRejectCallback, ErrorResult& aRv)
{
  nsRefPtr<Promise> promise = Create(GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));

  nsRefPtr<PromiseCallback> resolveCb =
    PromiseCallback::Factory(promise, global, aResolveCallback,
                             PromiseCallback::Resolve);

  nsRefPtr<PromiseCallback> rejectCb =
    PromiseCallback::Factory(promise, global, aRejectCallback,
                             PromiseCallback::Reject);

  AppendCallbacks(resolveCb, rejectCb);

  return promise.forget();
}

already_AddRefed<Promise>
Promise::Catch(JSContext* aCx, AnyCallback* aRejectCallback, ErrorResult& aRv)
{
  nsRefPtr<AnyCallback> resolveCb;
  return Then(aCx, resolveCb, aRejectCallback, aRv);
}

/**
 * The CountdownHolder class encapsulates Promise.all countdown functions and
 * the countdown holder parts of the Promises spec. It maintains the result
 * array and AllResolveHandlers use SetValue() to set the array indices.
 */
class CountdownHolder MOZ_FINAL : public nsISupports
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CountdownHolder)

  CountdownHolder(const GlobalObject& aGlobal, Promise* aPromise,
                  uint32_t aCountdown)
    : mPromise(aPromise), mCountdown(aCountdown)
  {
    MOZ_ASSERT(aCountdown != 0);
    JSContext* cx = aGlobal.Context();

    // The only time aGlobal.Context() and aGlobal.Get() are not
    // same-compartment is when we're called via Xrays, and in that situation we
    // in fact want to create the array in the callee compartment

    JSAutoCompartment ac(cx, aGlobal.Get());
    mValues = JS_NewArrayObject(cx, aCountdown);
    mozilla::HoldJSObjects(this);
  }

private:
  ~CountdownHolder()
  {
    mozilla::DropJSObjects(this);
  }

public:
  void SetValue(uint32_t index, const JS::Handle<JS::Value> aValue)
  {
    MOZ_ASSERT(mCountdown > 0);

    ThreadsafeAutoSafeJSContext cx;
    JSAutoCompartment ac(cx, mValues);
    {

      AutoDontReportUncaught silenceReporting(cx);
      JS::Rooted<JS::Value> value(cx, aValue);
      JS::Rooted<JSObject*> values(cx, mValues);
      if (!JS_WrapValue(cx, &value) ||
          !JS_DefineElement(cx, values, index, value, JSPROP_ENUMERATE)) {
        MOZ_ASSERT(JS_IsExceptionPending(cx));
        JS::Rooted<JS::Value> exn(cx);
        JS_GetPendingException(cx, &exn);

        mPromise->MaybeReject(cx, exn);
      }
    }

    --mCountdown;
    if (mCountdown == 0) {
      JS::Rooted<JS::Value> result(cx, JS::ObjectValue(*mValues));
      mPromise->MaybeResolve(cx, result);
    }
  }

private:
  nsRefPtr<Promise> mPromise;
  uint32_t mCountdown;
  JS::Heap<JSObject*> mValues;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(CountdownHolder)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CountdownHolder)
NS_IMPL_CYCLE_COLLECTION_CLASS(CountdownHolder)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CountdownHolder)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(CountdownHolder)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mValues)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(CountdownHolder)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromise)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CountdownHolder)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromise)
  tmp->mValues = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

/**
 * An AllResolveHandler is the per-promise part of the Promise.all() algorithm.
 * Every Promise in the handler is handed an instance of this as a resolution
 * handler and it sets the relevant index in the CountdownHolder.
 */
class AllResolveHandler MOZ_FINAL : public PromiseNativeHandler
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(AllResolveHandler)

  AllResolveHandler(CountdownHolder* aHolder, uint32_t aIndex)
    : mCountdownHolder(aHolder), mIndex(aIndex)
  {
    MOZ_ASSERT(aHolder);
  }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
  {
    mCountdownHolder->SetValue(mIndex, aValue);
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
  {
    // Should never be attached to Promise as a reject handler.
    MOZ_ASSERT(false, "AllResolveHandler should never be attached to a Promise's reject handler!");
  }

private:
  ~AllResolveHandler()
  {
  }

  nsRefPtr<CountdownHolder> mCountdownHolder;
  uint32_t mIndex;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(AllResolveHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AllResolveHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AllResolveHandler)
NS_INTERFACE_MAP_END_INHERITING(PromiseNativeHandler)

NS_IMPL_CYCLE_COLLECTION(AllResolveHandler, mCountdownHolder)

/* static */ already_AddRefed<Promise>
Promise::All(const GlobalObject& aGlobal,
             const Sequence<JS::Value>& aIterable, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  JSContext* cx = aGlobal.Context();

  if (aIterable.Length() == 0) {
    JS::Rooted<JSObject*> empty(cx, JS_NewArrayObject(cx, 0));
    if (!empty) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
    JS::Rooted<JS::Value> value(cx, JS::ObjectValue(*empty));
    // We know "value" is not a promise, so call the Resolve function
    // that doesn't have to check for that.
    return Promise::Resolve(global, cx, value, aRv);
  }

  nsRefPtr<Promise> promise = Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  nsRefPtr<CountdownHolder> holder =
    new CountdownHolder(aGlobal, promise, aIterable.Length());

  JS::Rooted<JSObject*> obj(cx, JS::CurrentGlobalOrNull(cx));
  if (!obj) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<PromiseCallback> rejectCb = new RejectPromiseCallback(promise, obj);

  for (uint32_t i = 0; i < aIterable.Length(); ++i) {
    JS::Rooted<JS::Value> value(cx, aIterable.ElementAt(i));
    nsRefPtr<Promise> nextPromise = Promise::Resolve(aGlobal, value, aRv);

    MOZ_ASSERT(!aRv.Failed());

    nsRefPtr<PromiseNativeHandler> resolveHandler =
      new AllResolveHandler(holder, i);

    nsRefPtr<PromiseCallback> resolveCb =
      new NativePromiseCallback(resolveHandler, Resolved);
    // Every promise gets its own resolve callback, which will set the right
    // index in the array to the resolution value.
    nextPromise->AppendCallbacks(resolveCb, rejectCb);
  }

  return promise.forget();
}

/* static */ already_AddRefed<Promise>
Promise::Race(const GlobalObject& aGlobal,
              const Sequence<JS::Value>& aIterable, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  JSContext* cx = aGlobal.Context();

  JS::Rooted<JSObject*> obj(cx, JS::CurrentGlobalOrNull(cx));
  if (!obj) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<PromiseCallback> resolveCb =
    new ResolvePromiseCallback(promise, obj);

  nsRefPtr<PromiseCallback> rejectCb = new RejectPromiseCallback(promise, obj);

  for (uint32_t i = 0; i < aIterable.Length(); ++i) {
    JS::Rooted<JS::Value> value(cx, aIterable.ElementAt(i));
    nsRefPtr<Promise> nextPromise = Promise::Resolve(aGlobal, value, aRv);
    // According to spec, Resolve can throw, but our implementation never does.
    // Well it does when window isn't passed on the main thread, but that is an
    // implementation detail which should never be reached since we are checking
    // for window above. Remove this when subclassing is supported.
    MOZ_ASSERT(!aRv.Failed());
    nextPromise->AppendCallbacks(resolveCb, rejectCb);
  }

  return promise.forget();
}

void
Promise::AppendNativeHandler(PromiseNativeHandler* aRunnable)
{
  nsRefPtr<PromiseCallback> resolveCb =
    new NativePromiseCallback(aRunnable, Resolved);

  nsRefPtr<PromiseCallback> rejectCb =
    new NativePromiseCallback(aRunnable, Rejected);

  AppendCallbacks(resolveCb, rejectCb);
}

void
Promise::AppendCallbacks(PromiseCallback* aResolveCallback,
                         PromiseCallback* aRejectCallback)
{
  if (aResolveCallback) {
    mResolveCallbacks.AppendElement(aResolveCallback);
  }

  if (aRejectCallback) {
    mHadRejectCallback = true;
    mRejectCallbacks.AppendElement(aRejectCallback);

    // Now that there is a callback, we don't need to report anymore.
    RemoveFeature();
  }

  // If promise's state is fulfilled, queue a task to process our fulfill
  // callbacks with promise's result. If promise's state is rejected, queue a
  // task to process our reject callbacks with promise's result.
  if (mState != Pending) {
    EnqueueCallbackTasks();
  }
}

class WrappedWorkerRunnable MOZ_FINAL : public WorkerSameThreadRunnable
{
public:
  WrappedWorkerRunnable(WorkerPrivate* aWorkerPrivate, nsIRunnable* aRunnable)
    : WorkerSameThreadRunnable(aWorkerPrivate)
    , mRunnable(aRunnable)
  {
    MOZ_ASSERT(aRunnable);
    MOZ_COUNT_CTOR(WrappedWorkerRunnable);
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    NS_ASSERT_OWNINGTHREAD(WrappedWorkerRunnable);
    mRunnable->Run();
    return true;
  }

private:
  virtual
  ~WrappedWorkerRunnable()
  {
    MOZ_COUNT_DTOR(WrappedWorkerRunnable);
    NS_ASSERT_OWNINGTHREAD(WrappedWorkerRunnable);
  }

  nsCOMPtr<nsIRunnable> mRunnable;
  NS_DECL_OWNINGTHREAD
};

/* static */ void
Promise::DispatchToMainOrWorkerThread(nsIRunnable* aRunnable)
{
  MOZ_ASSERT(aRunnable);
  if (NS_IsMainThread()) {
    NS_DispatchToCurrentThread(aRunnable);
    return;
  }
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  nsRefPtr<WrappedWorkerRunnable> task = new WrappedWorkerRunnable(worker, aRunnable);
  task->Dispatch(worker->GetJSContext());
}

void
Promise::MaybeReportRejected()
{
  if (mState != Rejected || mHadRejectCallback || mResult.isUndefined()) {
    return;
  }

  AutoJSAPI jsapi;
  // We may not have a usable global by now (if it got unlinked
  // already), so don't init with it.
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> obj(cx, GetWrapper());
  MOZ_ASSERT(obj); // We preserve our wrapper, so should always have one here.
  JS::Rooted<JS::Value> val(cx, mResult);
  JS::ExposeValueToActiveJS(val);

  JSAutoCompartment ac(cx, obj);
  if (!JS_WrapValue(cx, &val)) {
    JS_ClearPendingException(cx);
    return;
  }

  js::ErrorReport report(cx);
  if (!report.init(cx, val)) {
    JS_ClearPendingException(cx);
    return;
  }

  nsRefPtr<xpc::ErrorReport> xpcReport = new xpc::ErrorReport();
  bool isMainThread = MOZ_LIKELY(NS_IsMainThread());
  bool isChrome = isMainThread ? nsContentUtils::IsSystemPrincipal(nsContentUtils::ObjectPrincipal(obj))
                               : GetCurrentThreadWorkerPrivate()->IsChromeWorker();
  nsPIDOMWindow* win = isMainThread ? xpc::WindowGlobalOrNull(obj) : nullptr;
  xpcReport->Init(report.report(), report.message(), isChrome, win ? win->WindowID() : 0);

  // Now post an event to do the real reporting async
  // Since Promises preserve their wrapper, it is essential to nsRefPtr<> the
  // AsyncErrorReporter, otherwise if the call to DispatchToMainThread fails, it
  // will leak. See Bug 958684.
  nsRefPtr<AsyncErrorReporter> r =
    new AsyncErrorReporter(CycleCollectedJSRuntime::Get()->Runtime(), xpcReport);
  NS_DispatchToMainThread(r);
}

void
Promise::MaybeResolveInternal(JSContext* aCx,
                              JS::Handle<JS::Value> aValue)
{
  if (mResolvePending) {
    return;
  }

  ResolveInternal(aCx, aValue);
}

void
Promise::MaybeRejectInternal(JSContext* aCx,
                             JS::Handle<JS::Value> aValue)
{
  if (mResolvePending) {
    return;
  }

  RejectInternal(aCx, aValue);
}

void
Promise::HandleException(JSContext* aCx)
{
  JS::Rooted<JS::Value> exn(aCx);
  if (JS_GetPendingException(aCx, &exn)) {
    JS_ClearPendingException(aCx);
    RejectInternal(aCx, exn);
  }
}

void
Promise::ResolveInternal(JSContext* aCx,
                         JS::Handle<JS::Value> aValue)
{
  mResolvePending = true;

  if (aValue.isObject()) {
    AutoDontReportUncaught silenceReporting(aCx);
    JS::Rooted<JSObject*> valueObj(aCx, &aValue.toObject());

    // Thenables.
    JS::Rooted<JS::Value> then(aCx);
    if (!JS_GetProperty(aCx, valueObj, "then", &then)) {
      HandleException(aCx);
      return;
    }

    if (then.isObject() && JS::IsCallable(&then.toObject())) {
      // This is the then() function of the thenable aValueObj.
      JS::Rooted<JSObject*> thenObj(aCx, &then.toObject());
      nsRefPtr<PromiseInit> thenCallback =
        new PromiseInit(thenObj, mozilla::dom::GetIncumbentGlobal());
      nsRefPtr<ThenableResolverTask> task =
        new ThenableResolverTask(this, valueObj, thenCallback);
      DispatchToMainOrWorkerThread(task);
      return;
    }
  }

  MaybeSettle(aValue, Resolved);
}

void
Promise::RejectInternal(JSContext* aCx,
                        JS::Handle<JS::Value> aValue)
{
  mResolvePending = true;

  MaybeSettle(aValue, Rejected);
}

void
Promise::MaybeSettle(JS::Handle<JS::Value> aValue,
                     PromiseState aState)
{
  // Promise.all() or Promise.race() implementations will repeatedly call
  // Resolve/RejectInternal rather than using the Maybe... forms. Stop SetState
  // from asserting.
  if (mState != Pending) {
    return;
  }

  SetResult(aValue);
  SetState(aState);
  mSettlementTimestamp = TimeStamp::Now();

  // If the Promise was rejected, and there is no reject handler already setup,
  // watch for thread shutdown.
  if (aState == PromiseState::Rejected &&
      !mHadRejectCallback &&
      !NS_IsMainThread()) {
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    worker->AssertIsOnWorkerThread();

    mFeature = new PromiseReportRejectFeature(this);
    if (NS_WARN_IF(!worker->AddFeature(worker->GetJSContext(), mFeature))) {
      // To avoid a false RemoveFeature().
      mFeature = nullptr;
      // Worker is shutting down, report rejection immediately since it is
      // unlikely that reject callbacks will be added after this point.
      MaybeReportRejectedOnce();
    }
  }

  EnqueueCallbackTasks();
}

void
Promise::EnqueueCallbackTasks()
{
  nsTArray<nsRefPtr<PromiseCallback>> callbacks;
  callbacks.SwapElements(mState == Resolved ? mResolveCallbacks
                                            : mRejectCallbacks);
  mResolveCallbacks.Clear();
  mRejectCallbacks.Clear();

  for (uint32_t i = 0; i < callbacks.Length(); ++i) {
    nsRefPtr<PromiseCallbackTask> task =
      new PromiseCallbackTask(this, callbacks[i], mResult);
    DispatchToMainOrWorkerThread(task);
  }
}

void
Promise::RemoveFeature()
{
  if (mFeature) {
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    worker->RemoveFeature(worker->GetJSContext(), mFeature);
    mFeature = nullptr;
  }
}

bool
PromiseReportRejectFeature::Notify(JSContext* aCx, workers::Status aStatus)
{
  MOZ_ASSERT(aStatus > workers::Running);
  mPromise->MaybeReportRejectedOnce();
  // After this point, `this` has been deleted by RemoveFeature!
  return true;
}

bool
Promise::CaptureStack(JSContext* aCx, JS::Heap<JSObject*>& aTarget)
{
  JS::Rooted<JSObject*> stack(aCx);
  if (!JS::CaptureCurrentStack(aCx, &stack)) {
    return false;
  }
  aTarget = stack;
  return true;
}

void
Promise::GetDependentPromises(nsTArray<nsRefPtr<Promise>>& aPromises)
{
  // We want to return promises that correspond to then() calls, Promise.all()
  // calls, and Promise.race() calls.
  //
  // For the then() case, we have both resolve and reject callbacks that know
  // what the next promise is.
  //
  // For the race() case, likewise.
  //
  // For the all() case, our reject callback knows what the next promise is, but
  // our resolve callback just knows it needs to notify some
  // PromiseNativeHandler, which itself only has an indirect relationship to the
  // next promise.
  //
  // So we walk over our _reject_ callbacks and ask each of them what promise
  // its dependent promise is.
  for (size_t i = 0; i < mRejectCallbacks.Length(); ++i) {
    Promise* p = mRejectCallbacks[i]->GetDependentPromise();
    if (p) {
      aPromises.AppendElement(p);
    }
  }
}

// A WorkerRunnable to resolve/reject the Promise on the worker thread.

class PromiseWorkerProxyRunnable : public workers::WorkerRunnable
{
public:
  PromiseWorkerProxyRunnable(PromiseWorkerProxy* aPromiseWorkerProxy,
                             JSStructuredCloneCallbacks* aCallbacks,
                             JSAutoStructuredCloneBuffer&& aBuffer,
                             PromiseWorkerProxy::RunCallbackFunc aFunc)
    : WorkerRunnable(aPromiseWorkerProxy->GetWorkerPrivate(),
                     WorkerThreadUnchangedBusyCount)
    , mPromiseWorkerProxy(aPromiseWorkerProxy)
    , mCallbacks(aCallbacks)
    , mBuffer(Move(aBuffer))
    , mFunc(aFunc)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mPromiseWorkerProxy);
  }

  virtual bool
  WorkerRun(JSContext* aCx, workers::WorkerPrivate* aWorkerPrivate)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aWorkerPrivate == mWorkerPrivate);

    MOZ_ASSERT(mPromiseWorkerProxy);
    nsRefPtr<Promise> workerPromise = mPromiseWorkerProxy->GetWorkerPromise();
    MOZ_ASSERT(workerPromise);

    // Here we convert the buffer to a JS::Value.
    JS::Rooted<JS::Value> value(aCx);
    if (!mBuffer.read(aCx, &value, mCallbacks, mPromiseWorkerProxy)) {
      JS_ClearPendingException(aCx);
      return false;
    }

    // TODO Bug 975246 - nsRefPtr should support operator |nsRefPtr->*funcType|.
    (workerPromise.get()->*mFunc)(aCx,
                                  value);

    // Release the Promise because it has been resolved/rejected for sure.
    mPromiseWorkerProxy->CleanUp(aCx);
    return true;
  }

protected:
  ~PromiseWorkerProxyRunnable() {}

private:
  nsRefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;
  JSStructuredCloneCallbacks* mCallbacks;
  JSAutoStructuredCloneBuffer mBuffer;

  // Function pointer for calling Promise::{ResolveInternal,RejectInternal}.
  PromiseWorkerProxy::RunCallbackFunc mFunc;
};

PromiseWorkerProxy::PromiseWorkerProxy(WorkerPrivate* aWorkerPrivate,
                                       Promise* aWorkerPromise,
                                       JSStructuredCloneCallbacks* aCallbacks)
  : mWorkerPrivate(aWorkerPrivate)
  , mWorkerPromise(aWorkerPromise)
  , mCleanedUp(false)
  , mCallbacks(aCallbacks)
  , mCleanUpLock("cleanUpLock")
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPromise);

  // We do this to make sure the worker thread won't shut down before the
  // promise is resolved/rejected on the worker thread.
  if (!mWorkerPrivate->AddFeature(mWorkerPrivate->GetJSContext(), this)) {
    MOZ_ASSERT(false, "cannot add the worker feature!");
    return;
  }
}

PromiseWorkerProxy::~PromiseWorkerProxy()
{
  MOZ_ASSERT(mCleanedUp);
  MOZ_ASSERT(!mWorkerPromise);
}

WorkerPrivate*
PromiseWorkerProxy::GetWorkerPrivate() const
{
  // It's ok to race on |mCleanedUp|, because it will never cause us to fire
  // the assertion when we should not.
  MOZ_ASSERT(!mCleanedUp);

  return mWorkerPrivate;
}

Promise*
PromiseWorkerProxy::GetWorkerPromise() const
{
  return mWorkerPromise;
}

void
PromiseWorkerProxy::StoreISupports(nsISupports* aSupports)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsMainThreadPtrHandle<nsISupports> supports(
    new nsMainThreadPtrHolder<nsISupports>(aSupports));
  mSupportsArray.AppendElement(supports);
}

void
PromiseWorkerProxy::RunCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue,
                                RunCallbackFunc aFunc)
{
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mCleanUpLock);
  // If the worker thread's been cancelled we don't need to resolve the Promise.
  if (mCleanedUp) {
    return;
  }

  // The |aValue| is written into the buffer. Note that we also pass |this|
  // into the structured-clone write in order to set its |mSupportsArray| to
  // keep objects alive until the structured-clone read/write is done.
  JSAutoStructuredCloneBuffer buffer;
  if (!buffer.write(aCx, aValue, mCallbacks, this)) {
    JS_ClearPendingException(aCx);
    MOZ_ASSERT(false, "cannot write the JSAutoStructuredCloneBuffer!");
  }

  nsRefPtr<PromiseWorkerProxyRunnable> runnable =
    new PromiseWorkerProxyRunnable(this,
                                   mCallbacks,
                                   Move(buffer),
                                   aFunc);

  runnable->Dispatch(aCx);
}

void
PromiseWorkerProxy::ResolvedCallback(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue)
{
  RunCallback(aCx, aValue, &Promise::ResolveInternal);
}

void
PromiseWorkerProxy::RejectedCallback(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue)
{
  RunCallback(aCx, aValue, &Promise::RejectInternal);
}

bool
PromiseWorkerProxy::Notify(JSContext* aCx, Status aStatus)
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->GetJSContext() == aCx);

  if (aStatus >= Canceling) {
    CleanUp(aCx);
  }

  return true;
}

void
PromiseWorkerProxy::CleanUp(JSContext* aCx)
{
  MutexAutoLock lock(mCleanUpLock);

  // |mWorkerPrivate| might not be safe to use anymore if we have already
  // cleaned up and RemoveFeature(), so we need to check |mCleanedUp| first.
  if (mCleanedUp) {
    return;
  }

  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->GetJSContext() == aCx);

  // Release the Promise and remove the PromiseWorkerProxy from the features of
  // the worker thread since the Promise has been resolved/rejected or the
  // worker thread has been cancelled.
  mWorkerPromise = nullptr;
  mWorkerPrivate->RemoveFeature(aCx, this);
  mCleanedUp = true;
}

// Specializations of MaybeRejectBrokenly we actually support.
template<>
void Promise::MaybeRejectBrokenly(const nsRefPtr<DOMError>& aArg) {
  MaybeSomething(aArg, &Promise::MaybeReject);
}
template<>
void Promise::MaybeRejectBrokenly(const nsAString& aArg) {
  MaybeSomething(aArg, &Promise::MaybeReject);
}

} // namespace dom
} // namespace mozilla
