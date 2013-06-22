/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Future.h"
#include "mozilla/dom/FutureBinding.h"
#include "mozilla/dom/FutureResolver.h"
#include "mozilla/Preferences.h"
#include "FutureCallback.h"
#include "nsContentUtils.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

// FutureTask

// This class processes the future's callbacks with future's result.
class FutureTask MOZ_FINAL : public nsRunnable
{
public:
  FutureTask(Future* aFuture)
    : mFuture(aFuture)
  {
    MOZ_ASSERT(aFuture);
    MOZ_COUNT_CTOR(FutureTask);
  }

  ~FutureTask()
  {
    MOZ_COUNT_DTOR(FutureTask);
  }

  NS_IMETHOD Run()
  {
    mFuture->mTaskPending = false;
    mFuture->RunTask();
    return NS_OK;
  }

private:
  nsRefPtr<Future> mFuture;
};

// Future

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Future)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mResolver)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mResolveCallbacks);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRejectCallbacks);
  tmp->mResult = JSVAL_VOID;
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Future)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResolver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResolveCallbacks);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRejectCallbacks);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Future)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mResult)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Future)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Future)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Future)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Future::Future(nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
  , mResult(JS::UndefinedValue())
  , mState(Pending)
  , mTaskPending(false)
{
  MOZ_COUNT_CTOR(Future);
  NS_HOLD_JS_OBJECTS(this, Future);
  SetIsDOMBinding();

  mResolver = new FutureResolver(this);
}

Future::~Future()
{
  mResult = JSVAL_VOID;
  NS_DROP_JS_OBJECTS(this, Future);
  MOZ_COUNT_DTOR(Future);
}

JSObject*
Future::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return FutureBinding::Wrap(aCx, aScope, this);
}

/* static */ bool
Future::PrefEnabled()
{
  return Preferences::GetBool("dom.future.enabled", false);
}

/* static */ already_AddRefed<Future>
Future::Constructor(const GlobalObject& aGlobal, JSContext* aCx,
                    FutureInit& aInit, ErrorResult& aRv)
{
  MOZ_ASSERT(PrefEnabled());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.Get());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Future> future = new Future(window);

  aInit.Call(future, *future->mResolver, aRv,
             CallbackObject::eRethrowExceptions);
  aRv.WouldReportJSException();

  if (aRv.IsJSException()) {
    Optional<JS::Handle<JS::Value> > value(aCx);
    aRv.StealJSException(aCx, &value.Value());
    future->mResolver->Reject(aCx, value);
  }

  return future.forget();
}

/* static */ already_AddRefed<Future>
Future::Resolve(const GlobalObject& aGlobal, JSContext* aCx,
                JS::Handle<JS::Value> aValue, ErrorResult& aRv)
{
  MOZ_ASSERT(PrefEnabled());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.Get());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Future> future = new Future(window);

  Optional<JS::Handle<JS::Value> > value(aCx, aValue);
  future->mResolver->Resolve(aCx, value);
  return future.forget();
}

/* static */ already_AddRefed<Future>
Future::Reject(const GlobalObject& aGlobal, JSContext* aCx,
               JS::Handle<JS::Value> aValue, ErrorResult& aRv)
{
  MOZ_ASSERT(PrefEnabled());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.Get());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Future> future = new Future(window);

  Optional<JS::Handle<JS::Value> > value(aCx, aValue);
  future->mResolver->Reject(aCx, value);
  return future.forget();
}

already_AddRefed<Future>
Future::Then(AnyCallback* aResolveCallback, AnyCallback* aRejectCallback)
{
  nsRefPtr<Future> future = new Future(GetParentObject());

  nsRefPtr<FutureCallback> resolveCb =
    FutureCallback::Factory(future->mResolver,
                            aResolveCallback,
                            FutureCallback::Resolve);

  nsRefPtr<FutureCallback> rejectCb =
    FutureCallback::Factory(future->mResolver,
                            aRejectCallback,
                            FutureCallback::Reject);

  AppendCallbacks(resolveCb, rejectCb);

  return future.forget();
}

already_AddRefed<Future>
Future::Catch(AnyCallback* aRejectCallback)
{
  return Then(nullptr, aRejectCallback);
}

void
Future::Done(AnyCallback* aResolveCallback, AnyCallback* aRejectCallback)
{
  if (!aResolveCallback && !aRejectCallback) {
    return;
  }

  nsRefPtr<FutureCallback> resolveCb;
  if (aResolveCallback) {
    resolveCb = new SimpleWrapperFutureCallback(this, aResolveCallback);
  }

  nsRefPtr<FutureCallback> rejectCb;
  if (aRejectCallback) {
    rejectCb = new SimpleWrapperFutureCallback(this, aRejectCallback);
  }

  AppendCallbacks(resolveCb, rejectCb);
}

void
Future::AppendCallbacks(FutureCallback* aResolveCallback,
                        FutureCallback* aRejectCallback)
{
  if (aResolveCallback) {
    mResolveCallbacks.AppendElement(aResolveCallback);
  }

  if (aRejectCallback) {
    mRejectCallbacks.AppendElement(aRejectCallback);
  }

  // If future's state is resolved, queue a task to process future's resolve
  // callbacks with future's result. If future's state is rejected, queue a task
  // to process future's reject callbacks with future's result.
  if (mState != Pending && !mTaskPending) {
    nsRefPtr<FutureTask> task = new FutureTask(this);
    NS_DispatchToCurrentThread(task);
    mTaskPending = true;
  }
}

void
Future::RunTask()
{
  MOZ_ASSERT(mState != Pending);

  nsTArray<nsRefPtr<FutureCallback> > callbacks;
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
