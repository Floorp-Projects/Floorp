/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Value.h"
#include "nsThreadUtils.h"

#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/TimeStamp.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/PromiseDebugging.h"
#include "mozilla/dom/PromiseDebuggingBinding.h"

namespace mozilla {
namespace dom {

class FlushRejections: public CancelableRunnable
{
public:
  static void Init() {
    if (!sDispatched.init()) {
      MOZ_CRASH("Could not initialize FlushRejections::sDispatched");
    }
    sDispatched.set(false);
  }

  static void DispatchNeeded() {
    if (sDispatched.get()) {
      // An instance of `FlushRejections` has already been dispatched
      // and not run yet. No need to dispatch another one.
      return;
    }
    sDispatched.set(true);
    NS_DispatchToCurrentThread(new FlushRejections());
  }

  static void FlushSync() {
    sDispatched.set(false);

    // Call the callbacks if necessary.
    // Note that these callbacks may in turn cause Promise to turn
    // uncaught or consumed. Since `sDispatched` is `false`,
    // `FlushRejections` will be called once again, on an ulterior
    // tick.
    PromiseDebugging::FlushUncaughtRejectionsInternal();
  }

  NS_IMETHOD Run() {
    FlushSync();
    return NS_OK;
  }

private:
  // `true` if an instance of `FlushRejections` is currently dispatched
  // and has not been executed yet.
  static MOZ_THREAD_LOCAL(bool) sDispatched;
};

/* static */ MOZ_THREAD_LOCAL(bool)
FlushRejections::sDispatched;

#ifndef SPIDERMONKEY_PROMISE
static Promise*
UnwrapPromise(JS::Handle<JSObject*> aPromise, ErrorResult& aRv)
{
  Promise* promise;
  if (NS_WARN_IF(NS_FAILED(UNWRAP_OBJECT(Promise, aPromise, promise)))) {
    aRv.ThrowTypeError<MSG_IS_NOT_PROMISE>(NS_LITERAL_STRING("Argument"));
    return nullptr;
  }
  return promise;
}
#endif // SPIDERMONKEY_PROMISE

#ifdef SPIDERMONKEY_PROMISE
/* static */ void
PromiseDebugging::GetState(GlobalObject& aGlobal, JS::Handle<JSObject*> aPromise,
                           PromiseDebuggingStateHolder& aState,
                           ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  JS::Rooted<JSObject*> obj(cx, js::CheckedUnwrap(aPromise));
  if (!obj || !JS::IsPromiseObject(obj)) {
    aRv.ThrowTypeError<MSG_IS_NOT_PROMISE>(NS_LITERAL_STRING(
        "Argument of PromiseDebugging.getState"));
    return;
  }
  switch (JS::GetPromiseState(obj)) {
  case JS::PromiseState::Pending:
    aState.mState = PromiseDebuggingState::Pending;
    break;
  case JS::PromiseState::Fulfilled:
    aState.mState = PromiseDebuggingState::Fulfilled;
    aState.mValue = JS::GetPromiseResult(obj);
    break;
  case JS::PromiseState::Rejected:
    aState.mState = PromiseDebuggingState::Rejected;
    aState.mReason = JS::GetPromiseResult(obj);
    break;
  }
}

/* static */ void
PromiseDebugging::GetPromiseID(GlobalObject& aGlobal,
                               JS::Handle<JSObject*> aPromise,
                               nsString& aID,
                               ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  JS::Rooted<JSObject*> obj(cx, js::CheckedUnwrap(aPromise));
  if (!obj || !JS::IsPromiseObject(obj)) {
    aRv.ThrowTypeError<MSG_IS_NOT_PROMISE>(NS_LITERAL_STRING(
        "Argument of PromiseDebugging.getState"));
    return;
  }
  uint64_t promiseID = JS::GetPromiseID(obj);
  aID = sIDPrefix;
  aID.AppendInt(promiseID);
}

/* static */ void
PromiseDebugging::GetAllocationStack(GlobalObject& aGlobal,
                                     JS::Handle<JSObject*> aPromise,
                                     JS::MutableHandle<JSObject*> aStack,
                                     ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  JS::Rooted<JSObject*> obj(cx, js::CheckedUnwrap(aPromise));
  if (!obj || !JS::IsPromiseObject(obj)) {
    aRv.ThrowTypeError<MSG_IS_NOT_PROMISE>(NS_LITERAL_STRING(
        "Argument of PromiseDebugging.getAllocationStack"));
    return;
  }
  aStack.set(JS::GetPromiseAllocationSite(obj));
}

/* static */ void
PromiseDebugging::GetRejectionStack(GlobalObject& aGlobal,
                                    JS::Handle<JSObject*> aPromise,
                                    JS::MutableHandle<JSObject*> aStack,
                                    ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  JS::Rooted<JSObject*> obj(cx, js::CheckedUnwrap(aPromise));
  if (!obj || !JS::IsPromiseObject(obj)) {
    aRv.ThrowTypeError<MSG_IS_NOT_PROMISE>(NS_LITERAL_STRING(
        "Argument of PromiseDebugging.getRejectionStack"));
    return;
  }
  aStack.set(JS::GetPromiseResolutionSite(obj));
}

/* static */ void
PromiseDebugging::GetFullfillmentStack(GlobalObject& aGlobal,
                                       JS::Handle<JSObject*> aPromise,
                                       JS::MutableHandle<JSObject*> aStack,
                                       ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  JS::Rooted<JSObject*> obj(cx, js::CheckedUnwrap(aPromise));
  if (!obj || !JS::IsPromiseObject(obj)) {
    aRv.ThrowTypeError<MSG_IS_NOT_PROMISE>(NS_LITERAL_STRING(
        "Argument of PromiseDebugging.getFulfillmentStack"));
    return;
  }
  aStack.set(JS::GetPromiseResolutionSite(obj));
}

#else

/* static */ void
PromiseDebugging::GetState(GlobalObject&, JS::Handle<JSObject*> aPromise,
                           PromiseDebuggingStateHolder& aState,
                           ErrorResult& aRv)
{
  Promise* promise = UnwrapPromise(aPromise, aRv);
  if (aRv.Failed()) {
    return;
  }
  switch (promise->mState) {
  case Promise::Pending:
    aState.mState = PromiseDebuggingState::Pending;
    break;
  case Promise::Resolved:
    aState.mState = PromiseDebuggingState::Fulfilled;
    JS::ExposeValueToActiveJS(promise->mResult);
    aState.mValue = promise->mResult;
    break;
  case Promise::Rejected:
    aState.mState = PromiseDebuggingState::Rejected;
    JS::ExposeValueToActiveJS(promise->mResult);
    aState.mReason = promise->mResult;
    break;
  }
}

#endif // SPIDERMONKEY_PROMISE

/*static */ nsString
PromiseDebugging::sIDPrefix;

/* static */ void
PromiseDebugging::Init()
{
  FlushRejections::Init();

  // Generate a prefix for identifiers: "PromiseDebugging.$processid."
  sIDPrefix = NS_LITERAL_STRING("PromiseDebugging.");
  if (XRE_IsContentProcess()) {
    sIDPrefix.AppendInt(ContentChild::GetSingleton()->GetID());
    sIDPrefix.Append('.');
  } else {
    sIDPrefix.AppendLiteral("0.");
  }
}

/* static */ void
PromiseDebugging::Shutdown()
{
  sIDPrefix.SetIsVoid(true);
}

/* static */ void
PromiseDebugging::FlushUncaughtRejections()
{
  MOZ_ASSERT(!NS_IsMainThread());
  FlushRejections::FlushSync();
}

#ifndef SPIDERMONKEY_PROMISE

/* static */ void
PromiseDebugging::GetAllocationStack(GlobalObject&, JS::Handle<JSObject*> aPromise,
                                     JS::MutableHandle<JSObject*> aStack,
                                     ErrorResult& aRv)
{
  Promise* promise = UnwrapPromise(aPromise, aRv);
  if (aRv.Failed()) {
    return;
  }
  aStack.set(promise->mAllocationStack);
}

/* static */ void
PromiseDebugging::GetRejectionStack(GlobalObject&, JS::Handle<JSObject*> aPromise,
                                    JS::MutableHandle<JSObject*> aStack,
                                    ErrorResult& aRv)
{
  Promise* promise = UnwrapPromise(aPromise, aRv);
  if (aRv.Failed()) {
    return;
  }
  aStack.set(promise->mRejectionStack);
}

/* static */ void
PromiseDebugging::GetFullfillmentStack(GlobalObject&, JS::Handle<JSObject*> aPromise,
                                       JS::MutableHandle<JSObject*> aStack,
                                       ErrorResult& aRv)
{
  Promise* promise = UnwrapPromise(aPromise, aRv);
  if (aRv.Failed()) {
    return;
  }
  aStack.set(promise->mFullfillmentStack);
}

/* static */ void
PromiseDebugging::GetDependentPromises(GlobalObject&, JS::Handle<JSObject*> aPromise,
                                       nsTArray<RefPtr<Promise>>& aPromises,
                                       ErrorResult& aRv)
{
  Promise* promise = UnwrapPromise(aPromise, aRv);
  if (aRv.Failed()) {
    return;
  }
  promise->GetDependentPromises(aPromises);
}

/* static */ double
PromiseDebugging::GetPromiseLifetime(GlobalObject&,
                                     JS::Handle<JSObject*> aPromise,
                                     ErrorResult& aRv)
{
  Promise* promise = UnwrapPromise(aPromise, aRv);
  if (aRv.Failed()) {
    return 0;
  }
  return (TimeStamp::Now() - promise->mCreationTimestamp).ToMilliseconds();
}

/* static */ double
PromiseDebugging::GetTimeToSettle(GlobalObject&, JS::Handle<JSObject*> aPromise,
                                  ErrorResult& aRv)
{
  Promise* promise = UnwrapPromise(aPromise, aRv);
  if (aRv.Failed()) {
    return 0;
  }
  if (promise->mState == Promise::Pending) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return 0;
  }
  return (promise->mSettlementTimestamp -
          promise->mCreationTimestamp).ToMilliseconds();
}

#endif // SPIDERMONKEY_PROMISE

/* static */ void
PromiseDebugging::AddUncaughtRejectionObserver(GlobalObject&,
                                               UncaughtRejectionObserver& aObserver)
{
  CycleCollectedJSRuntime* storage = CycleCollectedJSRuntime::Get();
  nsTArray<nsCOMPtr<nsISupports>>& observers = storage->mUncaughtRejectionObservers;
  observers.AppendElement(&aObserver);
}

/* static */ bool
PromiseDebugging::RemoveUncaughtRejectionObserver(GlobalObject&,
                                                  UncaughtRejectionObserver& aObserver)
{
  CycleCollectedJSRuntime* storage = CycleCollectedJSRuntime::Get();
  nsTArray<nsCOMPtr<nsISupports>>& observers = storage->mUncaughtRejectionObservers;
  for (size_t i = 0; i < observers.Length(); ++i) {
    UncaughtRejectionObserver* observer = static_cast<UncaughtRejectionObserver*>(observers[i].get());
    if (*observer == aObserver) {
      observers.RemoveElementAt(i);
      return true;
    }
  }
  return false;
}

#ifdef SPIDERMONKEY_PROMISE

/* static */ void
PromiseDebugging::AddUncaughtRejection(JS::HandleObject aPromise)
{
  // This might OOM, but won't set a pending exception, so we'll just ignore it.
  if (CycleCollectedJSRuntime::Get()->mUncaughtRejections.append(aPromise)) {
    FlushRejections::DispatchNeeded();
  }
}

/* void */ void
PromiseDebugging::AddConsumedRejection(JS::HandleObject aPromise)
{
  // If the promise is in our list of uncaught rejections, we haven't yet
  // reported it as unhandled. In that case, just remove it from the list
  // and don't add it to the list of consumed rejections.
  auto& uncaughtRejections = CycleCollectedJSRuntime::Get()->mUncaughtRejections;
  for (size_t i = 0; i < uncaughtRejections.length(); i++) {
    if (uncaughtRejections[i] == aPromise) {
      // To avoid large amounts of memmoves, we don't shrink the vector here.
      // Instead, we filter out nullptrs when iterating over the vector later.
      uncaughtRejections[i].set(nullptr);
      return;
    }
  }
  // This might OOM, but won't set a pending exception, so we'll just ignore it.
  if (CycleCollectedJSRuntime::Get()->mConsumedRejections.append(aPromise)) {
    FlushRejections::DispatchNeeded();
  }
}

/* static */ void
PromiseDebugging::FlushUncaughtRejectionsInternal()
{
  CycleCollectedJSRuntime* storage = CycleCollectedJSRuntime::Get();

  auto& uncaught = storage->mUncaughtRejections;
  auto& consumed = storage->mConsumedRejections;

  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();

  // Notify observers of uncaught Promise.
  auto& observers = storage->mUncaughtRejectionObservers;

  for (size_t i = 0; i < uncaught.length(); i++) {
    JS::RootedObject promise(cx, uncaught[i]);
    // Filter out nullptrs which might've been added by
    // PromiseDebugging::AddConsumedRejection.
    if (!promise) {
      continue;
    }

    for (size_t j = 0; j < observers.Length(); ++j) {
      RefPtr<UncaughtRejectionObserver> obs =
        static_cast<UncaughtRejectionObserver*>(observers[j].get());

      IgnoredErrorResult err;
      obs->OnLeftUncaught(promise, err);
    }
    JSAutoCompartment ac(cx, promise);
    Promise::ReportRejectedPromise(cx, promise);
  }
  storage->mUncaughtRejections.clear();

  // Notify observers of consumed Promise.

  for (size_t i = 0; i < consumed.length(); i++) {
    JS::RootedObject promise(cx, consumed[i]);

    for (size_t j = 0; j < observers.Length(); ++j) {
      RefPtr<UncaughtRejectionObserver> obs =
        static_cast<UncaughtRejectionObserver*>(observers[j].get());

      IgnoredErrorResult err;
      obs->OnConsumed(promise, err);
    }
  }
  storage->mConsumedRejections.clear();
}

#else

/* static */ void
PromiseDebugging::AddUncaughtRejection(Promise& aPromise)
{
  CycleCollectedJSRuntime::Get()->mUncaughtRejections.AppendElement(&aPromise);
  FlushRejections::DispatchNeeded();
}

/* void */ void
PromiseDebugging::AddConsumedRejection(Promise& aPromise)
{
  CycleCollectedJSRuntime::Get()->mConsumedRejections.AppendElement(&aPromise);
  FlushRejections::DispatchNeeded();
}

/* static */ void
PromiseDebugging::GetPromiseID(GlobalObject&,
                               JS::Handle<JSObject*> aPromise,
                               nsString& aID,
                               ErrorResult& aRv)
{
  Promise* promise = UnwrapPromise(aPromise, aRv);
  if (aRv.Failed()) {
    return;
  }
  uint64_t promiseID = promise->GetID();
  aID = sIDPrefix;
  aID.AppendInt(promiseID);
}

/* static */ void
PromiseDebugging::FlushUncaughtRejectionsInternal()
{
  CycleCollectedJSRuntime* storage = CycleCollectedJSRuntime::Get();

  // The Promise that have been left uncaught (rejected and last in
  // their chain) since the last call to this function.
  nsTArray<nsCOMPtr<nsISupports>> uncaught;
  storage->mUncaughtRejections.SwapElements(uncaught);

  // The Promise that have been left uncaught at some point, but that
  // have eventually had their `then` method called.
  nsTArray<nsCOMPtr<nsISupports>> consumed;
  storage->mConsumedRejections.SwapElements(consumed);

  nsTArray<nsCOMPtr<nsISupports>>& observers = storage->mUncaughtRejectionObservers;

  nsresult rv;
  // Notify observers of uncaught Promise.

  for (size_t i = 0; i < uncaught.Length(); ++i) {
    nsCOMPtr<Promise> promise = do_QueryInterface(uncaught[i], &rv);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    if (!promise->IsLastInChain()) {
      // This promise is not the last in the chain anymore,
      // so the error has been caught at some point.
      continue;
    }

    // For the moment, the Promise is still at the end of the
    // chain. Let's inform observers, so that they may decide whether
    // to report it.
    for (size_t j = 0; j < observers.Length(); ++j) {
      ErrorResult err;
      RefPtr<UncaughtRejectionObserver> obs =
        static_cast<UncaughtRejectionObserver*>(observers[j].get());

      obs->OnLeftUncaught(*promise, err); // Ignore errors
    }

    promise->SetNotifiedAsUncaught();
  }

  // Notify observers of consumed Promise.

  for (size_t i = 0; i < consumed.Length(); ++i) {
    nsCOMPtr<Promise> promise = do_QueryInterface(consumed[i], &rv);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    if (!promise->WasNotifiedAsUncaught()) {
      continue;
    }

    MOZ_ASSERT(!promise->IsLastInChain());
    for (size_t j = 0; j < observers.Length(); ++j) {
      ErrorResult err;
      RefPtr<UncaughtRejectionObserver> obs =
        static_cast<UncaughtRejectionObserver*>(observers[j].get());

      obs->OnConsumed(*promise, err); // Ignore errors
    }
  }
}
#endif // SPIDERMONKEY_PROMISE

} // namespace dom
} // namespace mozilla
