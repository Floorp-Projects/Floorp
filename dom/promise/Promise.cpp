/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/PromiseResolver.h"
#include "mozilla/Preferences.h"
#include "PromiseCallback.h"
#include "nsContentUtils.h"
#include "nsPIDOMWindow.h"
#include "WorkerPrivate.h"
#include "nsJSPrincipals.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

// PromiseTask

// This class processes the promise's callbacks with promise's result.
class PromiseTask MOZ_FINAL : public nsRunnable
{
public:
  PromiseTask(Promise* aPromise)
    : mPromise(aPromise)
  {
    MOZ_ASSERT(aPromise);
    MOZ_COUNT_CTOR(PromiseTask);
  }

  ~PromiseTask()
  {
    MOZ_COUNT_DTOR(PromiseTask);
  }

  NS_IMETHOD Run()
  {
    mPromise->mTaskPending = false;
    mPromise->RunTask();
    return NS_OK;
  }

private:
  nsRefPtr<Promise> mPromise;
};

// Promise

NS_IMPL_CYCLE_COLLECTION_CLASS(Promise)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Promise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mResolver)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mResolveCallbacks);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRejectCallbacks);
  tmp->mResult = JSVAL_VOID;
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Promise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResolver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResolveCallbacks);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRejectCallbacks);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Promise)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mResult)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Promise)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Promise)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Promise)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Promise::Promise(nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
  , mResult(JS::UndefinedValue())
  , mState(Pending)
  , mTaskPending(false)
{
  MOZ_COUNT_CTOR(Promise);
  NS_HOLD_JS_OBJECTS(this, Promise);
  SetIsDOMBinding();

  mResolver = new PromiseResolver(this);
}

Promise::~Promise()
{
  mResult = JSVAL_VOID;
  NS_DROP_JS_OBJECTS(this, Promise);
  MOZ_COUNT_DTOR(Promise);
}

JSObject*
Promise::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return PromiseBinding::Wrap(aCx, aScope, this);
}

/* static */ bool
Promise::PrefEnabled()
{
  return Preferences::GetBool("dom.promise.enabled", false);
}

/* static */ bool
Promise::EnabledForScope(JSContext* aCx, JSObject* /* unused */)
{
  // Enable if the pref is enabled or if we're chrome or if we're a
  // certified app.
  if (PrefEnabled()) {
    return true;
  }

  // Note that we have no concept of a certified app in workers.
  // XXXbz well, why not?
  if (!NS_IsMainThread()) {
    return workers::GetWorkerPrivateFromContext(aCx)->IsChromeWorker();
  }

  nsIPrincipal* prin = nsContentUtils::GetSubjectPrincipal();
  return nsContentUtils::IsSystemPrincipal(prin) ||
    prin->GetAppStatus() == nsIPrincipal::APP_STATUS_CERTIFIED;
}

static void
EnterCompartment(Maybe<JSAutoCompartment>& aAc, JSContext* aCx,
                 const Optional<JS::Handle<JS::Value> >& aValue)
{
  // FIXME Bug 878849
  if (aValue.WasPassed() && aValue.Value().isObject()) {
    JS::Rooted<JSObject*> rooted(aCx, &aValue.Value().toObject());
    aAc.construct(aCx, rooted);
  }
}

/* static */ already_AddRefed<Promise>
Promise::Constructor(const GlobalObject& aGlobal,
                     PromiseInit& aInit, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.GetContext();
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> promise = new Promise(window);

  aInit.Call(promise, *promise->mResolver, aRv,
             CallbackObject::eRethrowExceptions);
  aRv.WouldReportJSException();

  if (aRv.IsJSException()) {
    Optional<JS::Handle<JS::Value> > value(cx);
    aRv.StealJSException(cx, &value.Value());

    Maybe<JSAutoCompartment> ac;
    EnterCompartment(ac, cx, value);
    promise->mResolver->Reject(cx, value);
  }

  return promise.forget();
}

/* static */ already_AddRefed<Promise>
Promise::Resolve(const GlobalObject& aGlobal, JSContext* aCx,
                 JS::Handle<JS::Value> aValue, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> promise = new Promise(window);

  Optional<JS::Handle<JS::Value> > value(aCx, aValue);
  promise->mResolver->Resolve(aCx, value);
  return promise.forget();
}

/* static */ already_AddRefed<Promise>
Promise::Reject(const GlobalObject& aGlobal, JSContext* aCx,
                JS::Handle<JS::Value> aValue, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> promise = new Promise(window);

  Optional<JS::Handle<JS::Value> > value(aCx, aValue);
  promise->mResolver->Reject(aCx, value);
  return promise.forget();
}

already_AddRefed<Promise>
Promise::Then(const Optional<OwningNonNull<AnyCallback> >& aResolveCallback,
              const Optional<OwningNonNull<AnyCallback> >& aRejectCallback)
{
  nsRefPtr<Promise> promise = new Promise(GetParentObject());

  nsRefPtr<PromiseCallback> resolveCb =
    PromiseCallback::Factory(promise->mResolver,
                             aResolveCallback.WasPassed()
                               ? &aResolveCallback.Value()
                               : nullptr,
                             PromiseCallback::Resolve);

  nsRefPtr<PromiseCallback> rejectCb =
    PromiseCallback::Factory(promise->mResolver,
                             aRejectCallback.WasPassed()
                               ? &aRejectCallback.Value()
                               : nullptr,
                             PromiseCallback::Reject);

  AppendCallbacks(resolveCb, rejectCb);

  return promise.forget();
}

already_AddRefed<Promise>
Promise::Catch(const Optional<OwningNonNull<AnyCallback> >& aRejectCallback)
{
  Optional<OwningNonNull<AnyCallback> > resolveCb;
  return Then(resolveCb, aRejectCallback);
}

void
Promise::AppendCallbacks(PromiseCallback* aResolveCallback,
                         PromiseCallback* aRejectCallback)
{
  if (aResolveCallback) {
    mResolveCallbacks.AppendElement(aResolveCallback);
  }

  if (aRejectCallback) {
    mRejectCallbacks.AppendElement(aRejectCallback);
  }

  // If promise's state is resolved, queue a task to process promise's resolve
  // callbacks with promise's result. If promise's state is rejected, queue a task
  // to process promise's reject callbacks with promise's result.
  if (mState != Pending && !mTaskPending) {
    nsRefPtr<PromiseTask> task = new PromiseTask(this);
    NS_DispatchToCurrentThread(task);
    mTaskPending = true;
  }
}

void
Promise::RunTask()
{
  MOZ_ASSERT(mState != Pending);

  nsTArray<nsRefPtr<PromiseCallback> > callbacks;
  callbacks.SwapElements(mState == Resolved ? mResolveCallbacks
                                            : mRejectCallbacks);
  mResolveCallbacks.Clear();
  mRejectCallbacks.Clear();

  JSAutoRequest ar(nsContentUtils::GetSafeJSContext());
  Optional<JS::Handle<JS::Value> > value(nsContentUtils::GetSafeJSContext(), mResult);

  for (uint32_t i = 0; i < callbacks.Length(); ++i) {
    callbacks[i]->Call(value);
  }
}

} // namespace dom
} // namespace mozilla
