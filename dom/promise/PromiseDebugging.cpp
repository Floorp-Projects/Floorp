/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
#include "mozilla/dom/PromiseDebugging.h"
#include "mozilla/dom/PromiseDebuggingBinding.h"

namespace mozilla {
namespace dom {

namespace {

class FlushRejections: public nsCancelableRunnable
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
  nsresult Run()
  {
    sDispatched.set(false);

    // Call the callbacks if necessary.
    // Note that these callbacks may in turn cause Promise to turn
    // uncaught or consumed. Since `sDispatched` is `false`,
    // `FlushRejections` will be called once again, on an ulterior
    // tick.
    PromiseDebugging::FlushUncaughtRejections();
    return NS_OK;
  }
private:
  // `true` if an instance of `FlushRejections` is currently dispatched
  // and has not been executed yet.
  static ThreadLocal<bool> sDispatched;
};

/* static */ ThreadLocal<bool>
FlushRejections::sDispatched;

} // namespace

/* static */ void
PromiseDebugging::GetState(GlobalObject&, Promise& aPromise,
                           PromiseDebuggingStateHolder& aState)
{
  switch (aPromise.mState) {
  case Promise::Pending:
    aState.mState = PromiseDebuggingState::Pending;
    break;
  case Promise::Resolved:
    aState.mState = PromiseDebuggingState::Fulfilled;
    JS::ExposeValueToActiveJS(aPromise.mResult);
    aState.mValue = aPromise.mResult;
    break;
  case Promise::Rejected:
    aState.mState = PromiseDebuggingState::Rejected;
    JS::ExposeValueToActiveJS(aPromise.mResult);
    aState.mReason = aPromise.mResult;
    break;
  }
}

/*static */ nsString
PromiseDebugging::sIDPrefix;

/* static */ void
PromiseDebugging::Init()
{
  FlushRejections::Init();

  // Generate a prefix for identifiers: "PromiseDebugging.$processid."
  sIDPrefix = NS_LITERAL_STRING("PromiseDebugging.");
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
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
PromiseDebugging::GetAllocationStack(GlobalObject&, Promise& aPromise,
                                     JS::MutableHandle<JSObject*> aStack)
{
  aStack.set(aPromise.mAllocationStack);
}

/* static */ void
PromiseDebugging::GetRejectionStack(GlobalObject&, Promise& aPromise,
                                    JS::MutableHandle<JSObject*> aStack)
{
  aStack.set(aPromise.mRejectionStack);
}

/* static */ void
PromiseDebugging::GetFullfillmentStack(GlobalObject&, Promise& aPromise,
                                       JS::MutableHandle<JSObject*> aStack)
{
  aStack.set(aPromise.mFullfillmentStack);
}

/* static */ void
PromiseDebugging::GetDependentPromises(GlobalObject&, Promise& aPromise,
                                       nsTArray<nsRefPtr<Promise>>& aPromises)
{
  aPromise.GetDependentPromises(aPromises);
}

/* static */ double
PromiseDebugging::GetPromiseLifetime(GlobalObject&, Promise& aPromise)
{
  return (TimeStamp::Now() - aPromise.mCreationTimestamp).ToMilliseconds();
}

/* static */ double
PromiseDebugging::GetTimeToSettle(GlobalObject&, Promise& aPromise,
                                  ErrorResult& aRv)
{
  if (aPromise.mState == Promise::Pending) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return 0;
  }
  return (aPromise.mSettlementTimestamp -
          aPromise.mCreationTimestamp).ToMilliseconds();
}

/* static */ void
PromiseDebugging::AddUncaughtRejectionObserver(GlobalObject&,
                                               UncaughtRejectionObserver& aObserver)
{
  CycleCollectedJSRuntime* storage = CycleCollectedJSRuntime::Get();
  nsTArray<nsRefPtr<UncaughtRejectionObserver>>& observers = storage->mUncaughtRejectionObservers;
  observers.AppendElement(&aObserver);
}

/* static */ void
PromiseDebugging::RemoveUncaughtRejectionObserver(GlobalObject&,
                                                  UncaughtRejectionObserver& aObserver)
{
  CycleCollectedJSRuntime* storage = CycleCollectedJSRuntime::Get();
  nsTArray<nsRefPtr<UncaughtRejectionObserver>>& observers = storage->mUncaughtRejectionObservers;
  for (size_t i = 0; i < observers.Length(); ++i) {
    if (*observers[i] == aObserver) {
      observers.RemoveElementAt(i);
      return;
    }
  }
}

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
                               Promise& aPromise,
                               nsString& aID)
{
  uint64_t promiseID = aPromise.GetID();
  aID = sIDPrefix;
  aID.AppendInt(promiseID);
}

/* static */ void
PromiseDebugging::FlushUncaughtRejections()
{
  CycleCollectedJSRuntime* storage = CycleCollectedJSRuntime::Get();

  // The Promise that have been left uncaught (rejected and last in
  // their chain) since the last call to this function.
  nsTArray<nsRefPtr<Promise>> uncaught;
  storage->mUncaughtRejections.SwapElements(uncaught);

  // The Promise that have been left uncaught at some point, but that
  // have eventually had their `then` method called.
  nsTArray<nsRefPtr<Promise>> consumed;
  storage->mConsumedRejections.SwapElements(consumed);

  nsTArray<nsRefPtr<UncaughtRejectionObserver>>& observers = storage->mUncaughtRejectionObservers;

  // Notify observers of uncaught Promise.

  for (size_t i = 0; i < uncaught.Length(); ++i) {
    nsRefPtr<Promise> promise = uncaught[i];
    if (!promise->IsLastInChain()) {
      // This promise is not the last in the chain anymore,
      // so the error has been caught at some point.
      continue;
    }

    // For the moment, the Promise is still at the end of the
    // chain. Let's inform observers, so that they may decide whether
    // to report it.
    for (size_t j = 0; j < observers.Length(); ++j) {
      ErrorResult rv;
      observers[j]->OnLeftUncaught(*promise, rv);
      // Ignore errors
    }

    promise->SetNotifiedAsUncaught();
  }

  // Notify observers of consumed Promise.

  for (size_t i = 0; i < consumed.Length(); ++i) {
    nsRefPtr<Promise> promise = consumed[i];
    if (!promise->WasNotifiedAsUncaught()) {
      continue;
    }

    MOZ_ASSERT(!promise->IsLastInChain());
    for (size_t j = 0; j < observers.Length(); ++j) {
      ErrorResult rv;
      observers[j]->OnConsumed(*promise, rv); // Ignore errors
    }
  }
}

} // namespace dom
} // namespace mozilla
