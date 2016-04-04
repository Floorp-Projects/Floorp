/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"

#include "js/Debug.h"

#include "mozilla/Atomics.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/Preferences.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/MediaStreamError.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/ScriptSettings.h"

#include "jsfriendapi.h"
#include "js/StructuredClone.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsJSEnvironment.h"
#include "nsJSPrincipals.h"
#include "nsJSUtils.h"
#include "nsPIDOMWindow.h"
#include "PromiseCallback.h"
#include "PromiseDebugging.h"
#include "PromiseNativeHandler.h"
#include "PromiseWorkerProxy.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WrapperFactory.h"
#include "xpcpublic.h"
#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

namespace mozilla {
namespace dom {

namespace {
// Generator used by Promise::GetID.
Atomic<uintptr_t> gIDGenerator(0);
} // namespace

using namespace workers;

#ifndef SPIDERMONKEY_PROMISE
// This class processes the promise's callbacks with promise's result.
class PromiseReactionJob final : public nsRunnable
{
public:
  PromiseReactionJob(Promise* aPromise,
                     PromiseCallback* aCallback,
                     const JS::Value& aValue)
    : mPromise(aPromise)
    , mCallback(aCallback)
    , mValue(CycleCollectedJSRuntime::Get()->Runtime(), aValue)
  {
    MOZ_ASSERT(aPromise);
    MOZ_ASSERT(aCallback);
    MOZ_COUNT_CTOR(PromiseReactionJob);
  }

  virtual
  ~PromiseReactionJob()
  {
    NS_ASSERT_OWNINGTHREAD(PromiseReactionJob);
    MOZ_COUNT_DTOR(PromiseReactionJob);
  }

protected:
  NS_IMETHOD
  Run() override
  {
    NS_ASSERT_OWNINGTHREAD(PromiseReactionJob);

    MOZ_ASSERT(mPromise->GetWrapper()); // It was preserved!

    AutoJSAPI jsapi;
    if (!jsapi.Init(mPromise->GetWrapper())) {
      return NS_ERROR_FAILURE;
    }
    JSContext* cx = jsapi.cx();

    JS::Rooted<JS::Value> value(cx, mValue);
    if (!MaybeWrapValue(cx, &value)) {
      NS_WARNING("Failed to wrap value into the right compartment.");
      JS_ClearPendingException(cx);
      return NS_OK;
    }

    JS::Rooted<JSObject*> asyncStack(cx, mPromise->mAllocationStack);

    {
      Maybe<JS::AutoSetAsyncStackForNewCalls> sas;
      if (asyncStack) {
        sas.emplace(cx, asyncStack, "Promise");
      }
      mCallback->Call(cx, value);
    }

    return NS_OK;
  }

private:
  RefPtr<Promise> mPromise;
  RefPtr<PromiseCallback> mCallback;
  JS::PersistentRooted<JS::Value> mValue;
  NS_DECL_OWNINGTHREAD;
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
  js::SetFunctionNativeReserved(aResolveFunc, Promise::SLOT_DATA,
                                JS::ObjectValue(*aRejectFunc));
  js::SetFunctionNativeReserved(aRejectFunc, Promise::SLOT_DATA,
                                JS::ObjectValue(*aResolveFunc));
}

/*
 * Returns false if callback was already called before, otherwise breaks the
 * links and returns true.
 */
bool
MarkAsCalledIfNotCalledBefore(JSContext* aCx, JS::Handle<JSObject*> aFunc)
{
  JS::Value otherFuncVal =
    js::GetFunctionNativeReserved(aFunc, Promise::SLOT_DATA);

  if (!otherFuncVal.isObject()) {
    return false;
  }

  JSObject* otherFuncObj = &otherFuncVal.toObject();
  MOZ_ASSERT(js::GetFunctionNativeReserved(otherFuncObj,
                                           Promise::SLOT_DATA).isObject());

  // Break both references.
  js::SetFunctionNativeReserved(aFunc, Promise::SLOT_DATA,
                                JS::UndefinedValue());
  js::SetFunctionNativeReserved(otherFuncObj, Promise::SLOT_DATA,
                                JS::UndefinedValue());

  return true;
}

Promise*
GetPromise(JSContext* aCx, JS::Handle<JSObject*> aFunc)
{
  JS::Value promiseVal = js::GetFunctionNativeReserved(aFunc,
                                                       Promise::SLOT_PROMISE);

  MOZ_ASSERT(promiseVal.isObject());

  Promise* promise;
  UNWRAP_OBJECT(Promise, &promiseVal.toObject(), promise);
  return promise;
}
} // namespace

// Runnable to resolve thenables.
// Equivalent to the specification's ResolvePromiseViaThenableTask.
class PromiseResolveThenableJob final : public nsRunnable
{
public:
  PromiseResolveThenableJob(Promise* aPromise,
                            JS::Handle<JSObject*> aThenable,
                            PromiseInit* aThen)
    : mPromise(aPromise)
    , mThenable(CycleCollectedJSRuntime::Get()->Runtime(), aThenable)
    , mThen(aThen)
  {
    MOZ_ASSERT(aPromise);
    MOZ_COUNT_CTOR(PromiseResolveThenableJob);
  }

  virtual
  ~PromiseResolveThenableJob()
  {
    NS_ASSERT_OWNINGTHREAD(PromiseResolveThenableJob);
    MOZ_COUNT_DTOR(PromiseResolveThenableJob);
  }

protected:
  NS_IMETHOD
  Run() override
  {
    NS_ASSERT_OWNINGTHREAD(PromiseResolveThenableJob);

    MOZ_ASSERT(mPromise->GetWrapper()); // It was preserved!

    AutoJSAPI jsapi;
    // If we ever change which compartment we're working in here, make sure to
    // fix the fast-path for resolved-with-a-Promise in ResolveInternal.
    if (!jsapi.Init(mPromise->GetWrapper())) {
      return NS_ERROR_FAILURE;
    }
    JSContext* cx = jsapi.cx();

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
                "promise thenable", CallbackObject::eRethrowExceptions,
                mPromise->Compartment());

    rv.WouldReportJSException();
    if (rv.Failed()) {
      JS::Rooted<JS::Value> exn(cx);
      { // Scope for JSAutoCompartment

        // Convert the ErrorResult to a JS exception object that we can reject
        // ourselves with.  This will be exactly the exception that would get
        // thrown from a binding method whose ErrorResult ended up with
        // whatever is on "rv" right now.
        JSAutoCompartment ac(cx, mPromise->GlobalJSObject());
        DebugOnly<bool> conversionResult = ToJSValue(cx, rv, &exn);
        MOZ_ASSERT(conversionResult);
      }

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

    return rv.StealNSResult();
  }

private:
  RefPtr<Promise> mPromise;
  JS::PersistentRooted<JSObject*> mThenable;
  RefPtr<PromiseInit> mThen;
  NS_DECL_OWNINGTHREAD;
};

// A struct implementing
// <http://www.ecma-international.org/ecma-262/6.0/#sec-promisecapability-records>.
// While the spec holds on to these in some places, in practice those places
// don't actually need everything from this struct, so we explicitly grab
// members from it as needed in those situations.  That allows us to make this a
// stack-only struct and keep the rooting simple.
//
// We also add an optimization for the (common) case when we discover that the
// Promise constructor we're supposed to use is in fact the canonical Promise
// constructor.  In that case we will just set mNativePromise in our
// PromiseCapability and not set mPromise/mResolve/mReject; the correct
// callbacks will be the standard Promise ones, and we don't really want to
// synthesize JSFunctions for them in that situation.
struct MOZ_STACK_CLASS Promise::PromiseCapability
{
  explicit PromiseCapability(JSContext* aCx)
    : mPromise(aCx)
    , mResolve(aCx)
    , mReject(aCx)
  {}

  // Take an exception on aCx and try to convert it into a promise rejection.
  // Note that this can result in a new exception being thrown on aCx, or an
  // exception getting thrown on aRv.  On entry to this method, aRv is assumed
  // to not be a failure.  This should only be called if NewPromiseCapability
  // succeeded on this PromiseCapability.
  void RejectWithException(JSContext* aCx, ErrorResult& aRv);

  // Return a JS::Value representing the promise.  This should only be called if
  // NewPromiseCapability succeeded on this PromiseCapability.  It makes no
  // guarantees about compartments (e.g. in the mNativePromise case it's in the
  // compartment of the reflector, but in the mPromise case it might be in the
  // compartment of some cross-compartment wrapper for a reflector).
  JS::Value PromiseValue() const;

  // All the JS::Value fields of this struct are actually objects, but for our
  // purposes it's simpler to store them as JS::Value.

  // [[Promise]].
  JS::Rooted<JSObject*> mPromise;
  // [[Resolve]].  Value in the context compartment.
  JS::Rooted<JS::Value> mResolve;
  // [[Reject]].  Value in the context compartment.
  JS::Rooted<JS::Value> mReject;
  // If mNativePromise is non-null, we should use it, not mPromise.
  RefPtr<Promise> mNativePromise;

private:
  // We don't want to allow creation of temporaries of this type, ever.
  PromiseCapability(const PromiseCapability&) = delete;
  PromiseCapability(PromiseCapability&&) = delete;
};

void
Promise::PromiseCapability::RejectWithException(JSContext* aCx,
                                                ErrorResult& aRv)
{
  // This method basically implements
  // http://www.ecma-international.org/ecma-262/6.0/#sec-ifabruptrejectpromise
  // or at least the parts of it that happen if we have an abrupt completion.

  MOZ_ASSERT(!aRv.Failed());
  MOZ_ASSERT(mNativePromise || mPromise,
             "NewPromiseCapability didn't succeed");

  JS::Rooted<JS::Value> exn(aCx);
  if (!JS_GetPendingException(aCx, &exn)) {
    // This is an uncatchable exception, so can't be converted into a rejection.
    // Just rethrow that on aRv.
    aRv.ThrowUncatchableException();
    return;
  }

  JS_ClearPendingException(aCx);

  // If we have a native promise, just reject it without trying to call out into
  // JS.
  if (mNativePromise) {
    mNativePromise->MaybeRejectInternal(aCx, exn);
    return;
  }

  JS::Rooted<JS::Value> ignored(aCx);
  if (!JS::Call(aCx, JS::UndefinedHandleValue, mReject, JS::HandleValueArray(exn),
                &ignored)) {
    aRv.NoteJSContextException(aCx);
  }
}

JS::Value
Promise::PromiseCapability::PromiseValue() const
{
  MOZ_ASSERT(mNativePromise || mPromise,
             "NewPromiseCapability didn't succeed");

  if (mNativePromise) {
    return JS::ObjectValue(*mNativePromise->GetWrapper());
  }

  return JS::ObjectValue(*mPromise);
}

#endif // SPIDERMONKEY_PROMISE

// Promise

NS_IMPL_CYCLE_COLLECTION_CLASS(Promise)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Promise)
#ifndef SPIDERMONKEY_PROMISE
#if defined(DOM_PROMISE_DEPRECATED_REPORTING)
  tmp->MaybeReportRejectedOnce();
#else
  tmp->mResult = JS::UndefinedValue();
#endif // defined(DOM_PROMISE_DEPRECATED_REPORTING)
#endif // SPIDERMONKEY_PROMISE
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
#ifndef SPIDERMONKEY_PROMISE
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mResolveCallbacks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRejectCallbacks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
#else // SPIDERMONKEY_PROMISE
  tmp->mPromiseObj = nullptr;
#endif // SPIDERMONKEY_PROMISE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Promise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
#ifndef SPIDERMONKEY_PROMISE
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResolveCallbacks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRejectCallbacks)
#endif // SPIDERMONKEY_PROMISE
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Promise)
#ifndef SPIDERMONKEY_PROMISE
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mResult)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mAllocationStack)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mRejectionStack)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mFullfillmentStack)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
#else // SPIDERMONKEY_PROMISE
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mPromiseObj);
#endif // SPIDERMONKEY_PROMISE
NS_IMPL_CYCLE_COLLECTION_TRACE_END

#ifndef SPIDERMONKEY_PROMISE
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(Promise)
  if (tmp->IsBlack()) {
    JS::ExposeValueToActiveJS(tmp->mResult);
    if (tmp->mAllocationStack) {
      JS::ExposeObjectToActiveJS(tmp->mAllocationStack);
    }
    if (tmp->mRejectionStack) {
      JS::ExposeObjectToActiveJS(tmp->mRejectionStack);
    }
    if (tmp->mFullfillmentStack) {
      JS::ExposeObjectToActiveJS(tmp->mFullfillmentStack);
    }
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(Promise)
  return tmp->IsBlackAndDoesNotNeedTracing(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(Promise)
  return tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END
#endif // SPIDERMONKEY_PROMISE

NS_IMPL_CYCLE_COLLECTING_ADDREF(Promise)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Promise)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Promise)
#ifndef SPIDERMONKEY_PROMISE
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
#endif // SPIDERMONKEY_PROMISE
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(Promise)
NS_INTERFACE_MAP_END

Promise::Promise(nsIGlobalObject* aGlobal)
  : mGlobal(aGlobal)
#ifndef SPIDERMONKEY_PROMISE
  , mResult(JS::UndefinedValue())
  , mAllocationStack(nullptr)
  , mRejectionStack(nullptr)
  , mFullfillmentStack(nullptr)
  , mState(Pending)
#if defined(DOM_PROMISE_DEPRECATED_REPORTING)
  , mHadRejectCallback(false)
#endif // defined(DOM_PROMISE_DEPRECATED_REPORTING)
  , mTaskPending(false)
  , mResolvePending(false)
  , mIsLastInChain(true)
  , mWasNotifiedAsUncaught(false)
  , mID(0)
#else // SPIDERMONKEY_PROMISE
  , mPromiseObj(nullptr)
#endif // SPIDERMONKEY_PROMISE
{
  MOZ_ASSERT(mGlobal);

  mozilla::HoldJSObjects(this);

#ifndef SPIDERMONKEY_PROMISE
  mCreationTimestamp = TimeStamp::Now();
#endif // SPIDERMONKEY_PROMISE
}

Promise::~Promise()
{
#ifndef SPIDERMONKEY_PROMISE
#if defined(DOM_PROMISE_DEPRECATED_REPORTING)
  MaybeReportRejectedOnce();
#endif // defined(DOM_PROMISE_DEPRECATED_REPORTING)
#endif // SPIDERMONKEY_PROMISE
  mozilla::DropJSObjects(this);
}

#ifdef SPIDERMONKEY_PROMISE

bool
Promise::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                    JS::MutableHandle<JSObject*> aWrapper)
{
#ifdef DEBUG
  binding_detail::AssertReflectorHasGivenProto(aCx, mPromiseObj, aGivenProto);
#endif // DEBUG
  JS::ExposeObjectToActiveJS(mPromiseObj);
  aWrapper.set(mPromiseObj);
  return true;
}

// static
already_AddRefed<Promise>
Promise::Create(nsIGlobalObject* aGlobal, ErrorResult& aRv)
{
  RefPtr<Promise> p = new Promise(aGlobal);
  p->CreateWrapper(nullptr, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return p.forget();
}

// static
already_AddRefed<Promise>
Promise::Resolve(nsIGlobalObject* aGlobal, JSContext* aCx,
                 JS::Handle<JS::Value> aValue, ErrorResult& aRv)
{
  JSAutoCompartment ac(aCx, aGlobal->GetGlobalJSObject());
  JS::Rooted<JSObject*> p(aCx,
                          JS::CallOriginalPromiseResolve(aCx, aValue));
  if (!p) {
    aRv.NoteJSContextException(aCx);
    return nullptr;
  }

  return CreateFromExisting(aGlobal, p);
}

// static
already_AddRefed<Promise>
Promise::Reject(nsIGlobalObject* aGlobal, JSContext* aCx,
                JS::Handle<JS::Value> aValue, ErrorResult& aRv)
{
  JSAutoCompartment ac(aCx, aGlobal->GetGlobalJSObject());
  JS::Rooted<JSObject*> p(aCx,
                          JS::CallOriginalPromiseReject(aCx, aValue));
  if (!p) {
    aRv.NoteJSContextException(aCx);
    return nullptr;
  }

  return CreateFromExisting(aGlobal, p);
}

// static
already_AddRefed<Promise>
Promise::All(const GlobalObject& aGlobal,
             const nsTArray<RefPtr<Promise>>& aPromiseList, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global;
  global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  JSContext* cx = aGlobal.Context();

  JS::AutoObjectVector promises(cx);
  if (!promises.reserve(aPromiseList.Length())) {
    aRv.NoteJSContextException(cx);
    return nullptr;
  }

  for (auto& promise : aPromiseList) {
    JS::Rooted<JSObject*> promiseObj(cx, promise->PromiseObj());
    // Just in case, make sure these are all in the context compartment.
    if (!JS_WrapObject(cx, &promiseObj)) {
      aRv.NoteJSContextException(cx);
      return nullptr;
    }
    promises.infallibleAppend(promiseObj);
  }

  JS::Rooted<JSObject*> result(cx, JS::GetWaitForAllPromise(cx, promises));
  if (!result) {
    aRv.NoteJSContextException(cx);
    return nullptr;
  }

  return CreateFromExisting(global, result);
}

void
Promise::Then(JSContext* aCx,
              // aCalleeGlobal may not be in the compartment of aCx, when called over
              // Xrays.
              JS::Handle<JSObject*> aCalleeGlobal,
              AnyCallback* aResolveCallback, AnyCallback* aRejectCallback,
              JS::MutableHandle<JS::Value> aRetval,
              ErrorResult& aRv)
{
  // Let's hope this does the right thing with Xrays...  Ensure everything is
  // just in the caller compartment; that ought to do the trick.  In theory we
  // should consider aCalleeGlobal, but in practice our only caller is
  // DOMRequest::Then, which is not working with a Promise subclass, so things
  // should be OK.
  JS::Rooted<JSObject*> promise(aCx, PromiseObj());
  if (!JS_WrapObject(aCx, &promise)) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  JS::Rooted<JSObject*> resolveCallback(aCx);
  if (aResolveCallback) {
    resolveCallback = aResolveCallback->Callback();
    if (!JS_WrapObject(aCx, &resolveCallback)) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }

  JS::Rooted<JSObject*> rejectCallback(aCx);
  if (aRejectCallback) {
    rejectCallback = aRejectCallback->Callback();
    if (!JS_WrapObject(aCx, &rejectCallback)) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }

  JS::Rooted<JSObject*> retval(aCx);
  retval = JS::CallOriginalPromiseThen(aCx, promise, resolveCallback,
                                       rejectCallback);
  if (!retval) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  aRetval.setObject(*retval);
}

// We need a dummy function to pass to JS::NewPromiseObject.
static bool
DoNothingPromiseExecutor(JSContext*, unsigned aArgc, JS::Value* aVp)
{
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);
  args.rval().setUndefined();
  return true;
}

void
Promise::CreateWrapper(JS::Handle<JSObject*> aDesiredProto, ErrorResult& aRv)
{
  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobal)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  JSContext* cx = jsapi.cx();

  JSFunction* doNothingFunc =
    JS_NewFunction(cx, DoNothingPromiseExecutor, /* nargs = */ 2,
                   /* flags = */ 0, nullptr);
  if (!doNothingFunc) {
    JS_ClearPendingException(cx);
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  JS::Rooted<JSObject*> doNothingObj(cx, JS_GetFunctionObject(doNothingFunc));
  mPromiseObj = JS::NewPromiseObject(cx, doNothingObj, aDesiredProto);
  if (!mPromiseObj) {
    JS_ClearPendingException(cx);
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
}

void
Promise::MaybeResolve(JSContext* aCx,
                      JS::Handle<JS::Value> aValue)
{
  JS::Rooted<JSObject*> p(aCx, PromiseObj());
  if (!JS::ResolvePromise(aCx, p, aValue)) {
    // Now what?  There's nothing sane to do here.
    JS_ClearPendingException(aCx);
  }
}

void
Promise::MaybeReject(JSContext* aCx,
                     JS::Handle<JS::Value> aValue)
{
  JS::Rooted<JSObject*> p(aCx, PromiseObj());
  if (!JS::RejectPromise(aCx, p, aValue)) {
    // Now what?  There's nothing sane to do here.
    JS_ClearPendingException(aCx);
  }
}

#define SLOT_NATIVEHANDLER 0
#define SLOT_NATIVEHANDLER_TASK 1

enum class NativeHandlerTask : int32_t {
  Resolve,
  Reject
};

static bool
NativeHandlerCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);

  JS::Rooted<JS::Value> v(aCx,
                          js::GetFunctionNativeReserved(&args.callee(),
                                                        SLOT_NATIVEHANDLER));
  MOZ_ASSERT(v.isObject());

  PromiseNativeHandler* handler;
  if (NS_FAILED(UNWRAP_OBJECT(PromiseNativeHandler, &v.toObject(),
                              handler))) {
    return Throw(aCx, NS_ERROR_UNEXPECTED);
  }

  v = js::GetFunctionNativeReserved(&args.callee(), SLOT_NATIVEHANDLER_TASK);
  NativeHandlerTask task = static_cast<NativeHandlerTask>(v.toInt32());

  if (task == NativeHandlerTask::Resolve) {
    handler->ResolvedCallback(aCx, args.get(0));
  } else {
    MOZ_ASSERT(task == NativeHandlerTask::Reject);
    handler->RejectedCallback(aCx, args.get(0));
  }

  return true;
}

static JSObject*
CreateNativeHandlerFunction(JSContext* aCx, JS::Handle<JSObject*> aHolder,
                            NativeHandlerTask aTask)
{
  JSFunction* func = js::NewFunctionWithReserved(aCx, NativeHandlerCallback,
                                                 /* nargs = */ 1,
                                                 /* flags = */ 0, nullptr);
  if (!func) {
    return nullptr;
  }

  JS::Rooted<JSObject*> obj(aCx, JS_GetFunctionObject(func));

  JS::ExposeObjectToActiveJS(aHolder);
  js::SetFunctionNativeReserved(obj, SLOT_NATIVEHANDLER,
                                JS::ObjectValue(*aHolder));
  js::SetFunctionNativeReserved(obj, SLOT_NATIVEHANDLER_TASK,
                                JS::Int32Value(static_cast<int32_t>(aTask)));

  return obj;
}

void
Promise::AppendNativeHandler(PromiseNativeHandler* aRunnable)
{
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mGlobal))) {
    // Our API doesn't allow us to return a useful error.  Not like this should
    // happen anyway.
    return;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> handlerWrapper(cx);
  // Note: PromiseNativeHandler is NOT wrappercached.  So we can't use
  // ToJSValue here, because it will try to do XPConnect wrapping on it, sadly.
  if (NS_WARN_IF(!aRunnable->WrapObject(cx, nullptr, &handlerWrapper))) {
    // Again, no way to report errors.
    jsapi.ClearException();
    return;
  }

  JS::Rooted<JSObject*> resolveFunc(cx);
  resolveFunc =
    CreateNativeHandlerFunction(cx, handlerWrapper, NativeHandlerTask::Resolve);
  if (NS_WARN_IF(!resolveFunc)) {
    jsapi.ClearException();
    return;
  }

  JS::Rooted<JSObject*> rejectFunc(cx);
  rejectFunc =
    CreateNativeHandlerFunction(cx, handlerWrapper, NativeHandlerTask::Reject);
  if (NS_WARN_IF(!rejectFunc)) {
    jsapi.ClearException();
    return;
  }

  JS::Rooted<JSObject*> promiseObj(cx, PromiseObj());
  if (NS_WARN_IF(!JS::AddPromiseReactions(cx, promiseObj, resolveFunc,
                                          rejectFunc))) {
    jsapi.ClearException();
    return;
  }
}

void
Promise::HandleException(JSContext* aCx)
{
  JS::Rooted<JS::Value> exn(aCx);
  if (JS_GetPendingException(aCx, &exn)) {
    JS_ClearPendingException(aCx);
    // This is only called from MaybeSomething, so it's OK to MaybeReject here,
    // unlike in the version that's used when !SPIDERMONKEY_PROMISE.
    MaybeReject(aCx, exn);
  }
}

// static
already_AddRefed<Promise>
Promise::CreateFromExisting(nsIGlobalObject* aGlobal,
                            JS::Handle<JSObject*> aPromiseObj)
{
  MOZ_ASSERT(js::GetObjectCompartment(aGlobal->GetGlobalJSObject()) ==
             js::GetObjectCompartment(aPromiseObj));
  RefPtr<Promise> p = new Promise(aGlobal);
  p->mPromiseObj = aPromiseObj;
  return p.forget();
}

#else // SPIDERMONKEY_PROMISE

JSObject*
Promise::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PromiseBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
Promise::Create(nsIGlobalObject* aGlobal, ErrorResult& aRv,
                JS::Handle<JSObject*> aDesiredProto)
{
  RefPtr<Promise> p = new Promise(aGlobal);
  p->CreateWrapper(aDesiredProto, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return p.forget();
}

void
Promise::CreateWrapper(JS::Handle<JSObject*> aDesiredProto, ErrorResult& aRv)
{
  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobal)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  JSContext* cx = jsapi.cx();

  JS::Rooted<JS::Value> wrapper(cx);
  if (!GetOrCreateDOMReflector(cx, this, &wrapper, aDesiredProto)) {
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

  JS::RootedObject obj(cx, &wrapper.toObject());
  JS::dbg::onNewPromise(cx, obj);
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

#endif // SPIDERMONKEY_PROMISE

void
Promise::MaybeReject(const RefPtr<MediaStreamError>& aArg) {
  MaybeSomething(aArg, &Promise::MaybeReject);
}

void
Promise::MaybeRejectWithNull()
{
  MaybeSomething(JS::NullHandleValue, &Promise::MaybeReject);
}

bool
Promise::PerformMicroTaskCheckpoint()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  CycleCollectedJSRuntime* runtime = CycleCollectedJSRuntime::Get();

  // On the main thread, we always use the main promise micro task queue.
  std::queue<nsCOMPtr<nsIRunnable>>& microtaskQueue =
    runtime->GetPromiseMicroTaskQueue();

  if (microtaskQueue.empty()) {
    return false;
  }

  AutoSafeJSContext cx;

  do {
    nsCOMPtr<nsIRunnable> runnable = microtaskQueue.front();
    MOZ_ASSERT(runnable);

    // This function can re-enter, so we remove the element before calling.
    microtaskQueue.pop();
    nsresult rv = runnable->Run();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
    JS_CheckForInterrupt(cx);
    runtime->AfterProcessMicrotask();
  } while (!microtaskQueue.empty());

  return true;
}

void
Promise::PerformWorkerMicroTaskCheckpoint()
{
  MOZ_ASSERT(!NS_IsMainThread(), "Wrong thread!");

  CycleCollectedJSRuntime* runtime = CycleCollectedJSRuntime::Get();

  for (;;) {
    // For a normal microtask checkpoint, we try to use the debugger microtask
    // queue first. If the debugger queue is empty, we use the normal microtask
    // queue instead.
    std::queue<nsCOMPtr<nsIRunnable>>* microtaskQueue =
      &runtime->GetDebuggerPromiseMicroTaskQueue();

    if (microtaskQueue->empty()) {
      microtaskQueue = &runtime->GetPromiseMicroTaskQueue();
      if (microtaskQueue->empty()) {
        break;
      }
    }

    nsCOMPtr<nsIRunnable> runnable = microtaskQueue->front();
    MOZ_ASSERT(runnable);

    // This function can re-enter, so we remove the element before calling.
    microtaskQueue->pop();
    nsresult rv = runnable->Run();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
    runtime->AfterProcessMicrotask();
  }
}

void
Promise::PerformWorkerDebuggerMicroTaskCheckpoint()
{
  MOZ_ASSERT(!NS_IsMainThread(), "Wrong thread!");

  CycleCollectedJSRuntime* runtime = CycleCollectedJSRuntime::Get();

  for (;;) {
    // For a debugger microtask checkpoint, we always use the debugger microtask
    // queue.
    std::queue<nsCOMPtr<nsIRunnable>>* microtaskQueue =
      &runtime->GetDebuggerPromiseMicroTaskQueue();

    if (microtaskQueue->empty()) {
      break;
    }

    nsCOMPtr<nsIRunnable> runnable = microtaskQueue->front();
    MOZ_ASSERT(runnable);

    // This function can re-enter, so we remove the element before calling.
    microtaskQueue->pop();
    nsresult rv = runnable->Run();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
    runtime->AfterProcessMicrotask();
  }
}

#ifndef SPIDERMONKEY_PROMISE

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
    if (!promise->CaptureStack(aCx, promise->mFullfillmentStack)) {
      return false;
    }
    promise->MaybeResolveInternal(aCx, args.get(0));
  } else {
    promise->MaybeRejectInternal(aCx, args.get(0));
    if (!promise->CaptureStack(aCx, promise->mRejectionStack)) {
      return false;
    }
  }

  args.rval().setUndefined();
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
    args.rval().setUndefined();
    return true;
  }

  Promise* promise = GetPromise(aCx, thisFunc);
  MOZ_ASSERT(promise);

  if (aTask == PromiseCallback::Resolve) {
    promise->ResolveInternal(aCx, args.get(0));
  } else {
    promise->RejectInternal(aCx, args.get(0));
  }

  args.rval().setUndefined();
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
Promise::CreateFunction(JSContext* aCx, Promise* aPromise, int32_t aTask)
{
  // If this function ever changes, make sure to update
  // WrapperPromiseCallback::GetDependentPromise.
  JSFunction* func = js::NewFunctionWithReserved(aCx, JSCallback,
                                                 1 /* nargs */, 0 /* flags */,
                                                 nullptr);
  if (!func) {
    return nullptr;
  }

  JS::Rooted<JSObject*> obj(aCx, JS_GetFunctionObject(func));

  JS::Rooted<JS::Value> promiseObj(aCx);
  if (!dom::GetOrCreateDOMReflector(aCx, aPromise, &promiseObj)) {
    return nullptr;
  }

  JS::ExposeValueToActiveJS(promiseObj);
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
                                                 nullptr);
  if (!func) {
    return nullptr;
  }

  JS::Rooted<JSObject*> obj(aCx, JS_GetFunctionObject(func));

  JS::Rooted<JS::Value> promiseObj(aCx);
  if (!dom::GetOrCreateDOMReflector(aCx, aPromise, &promiseObj)) {
    return nullptr;
  }

  JS::ExposeValueToActiveJS(promiseObj);
  js::SetFunctionNativeReserved(obj, SLOT_PROMISE, promiseObj);

  return obj;
}

/* static */ already_AddRefed<Promise>
Promise::Constructor(const GlobalObject& aGlobal, PromiseInit& aInit,
                     ErrorResult& aRv, JS::Handle<JSObject*> aDesiredProto)
{
  nsCOMPtr<nsIGlobalObject> global;
  global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<Promise> promise = Create(global, aRv, aDesiredProto);
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
                                    CreateFunction(cx, this,
                                                   PromiseCallback::Resolve));
  if (!resolveFunc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  JS::Rooted<JSObject*> rejectFunc(cx,
                                   CreateFunction(cx, this,
                                                  PromiseCallback::Reject));
  if (!rejectFunc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  aInit.Call(resolveFunc, rejectFunc, aRv, "promise initializer",
             CallbackObject::eRethrowExceptions, Compartment());
  aRv.WouldReportJSException();

  if (aRv.Failed()) {
    if (aRv.IsUncatchableException()) {
      // Just propagate this to the caller.
      return;
    }

    // There are two possibilities here.  Either we've got a rethrown exception,
    // or we reported that already and synthesized a generic NS_ERROR_FAILURE on
    // the ErrorResult.  In the former case, it doesn't much matter how we get
    // the exception JS::Value from the ErrorResult to us, since we'll just end
    // up wrapping it into the right compartment as needed if we hand it to
    // someone.  But in the latter case we have to ensure that the new exception
    // object we create is created in our reflector compartment, not in our
    // current compartment, because in the case when we're a Promise constructor
    // called over Xrays creating it in the current compartment would mean
    // rejecting with a value that can't be accessed by code that can call
    // then() on this Promise.
    //
    // Luckily, MaybeReject(aRv) does exactly what we want here: it enters our
    // reflector compartment before trying to produce a JS::Value from the
    // ErrorResult.
    MaybeReject(aRv);
  }
}

#define GET_CAPABILITIES_EXECUTOR_RESOLVE_SLOT 0
#define GET_CAPABILITIES_EXECUTOR_REJECT_SLOT 1

namespace {
bool
GetCapabilitiesExecutor(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  // Implements
  // http://www.ecma-international.org/ecma-262/6.0/#sec-getcapabilitiesexecutor-functions
  // except we store the [[Resolve]] and [[Reject]] in our own internal slots,
  // not in a PromiseCapability.  The PromiseCapability will then read them from
  // us.
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);

  // Step 1 is an assert.

  // Step 2 doesn't need to be done, because it's just giving a name to the
  // PromiseCapability record which is supposed to be stored in an internal
  // slot.  But we don't store that at all, per the comment above; we just
  // directly store its [[Resolve]] and [[Reject]] members.

  // Steps 3 and 4.
  if (!js::GetFunctionNativeReserved(&args.callee(),
                                     GET_CAPABILITIES_EXECUTOR_RESOLVE_SLOT).isUndefined() ||
      !js::GetFunctionNativeReserved(&args.callee(),
                                     GET_CAPABILITIES_EXECUTOR_REJECT_SLOT).isUndefined()) {
    ErrorResult rv;
    rv.ThrowTypeError<MSG_PROMISE_CAPABILITY_HAS_SOMETHING_ALREADY>();
    return !rv.MaybeSetPendingException(aCx);
  }

  // Step 5.
  js::SetFunctionNativeReserved(&args.callee(),
                                GET_CAPABILITIES_EXECUTOR_RESOLVE_SLOT,
                                args.get(0));

  // Step 6.
  js::SetFunctionNativeReserved(&args.callee(),
                                GET_CAPABILITIES_EXECUTOR_REJECT_SLOT,
                                args.get(1));

  // Step 7.
  args.rval().setUndefined();
  return true;
}
} // anonymous namespace

/* static */ void
Promise::NewPromiseCapability(JSContext* aCx, nsIGlobalObject* aGlobal,
                              JS::Handle<JS::Value> aConstructor,
                              bool aForceCallbackCreation,
                              PromiseCapability& aCapability,
                              ErrorResult& aRv)
{
  // Implements
  // http://www.ecma-international.org/ecma-262/6.0/#sec-newpromisecapability

  if (!aConstructor.isObject() ||
      !JS::IsConstructor(&aConstructor.toObject())) {
    aRv.ThrowTypeError<MSG_ILLEGAL_PROMISE_CONSTRUCTOR>();
    return;
  }

  // Step 2 is a note.
  // Step 3 is already done because we got the PromiseCapability passed in.

  // Optimization: Check whether constructor is in fact the canonical
  // Promise constructor for aGlobal.
  JS::Rooted<JSObject*> global(aCx, aGlobal->GetGlobalJSObject());
  {
    // Scope for the JSAutoCompartment, since we need to enter the compartment
    // of global to get constructors from it.  Save the compartment we used to
    // be in, though; we'll need it later.
    JS::Rooted<JSObject*> callerGlobal(aCx, JS::CurrentGlobalOrNull(aCx));
    JSAutoCompartment ac(aCx, global);

    // Now wrap aConstructor into the compartment of aGlobal, so comparing it to
    // the canonical Promise for that compartment actually makes sense.
    JS::Rooted<JS::Value> constructorValue(aCx, aConstructor);
    if (!MaybeWrapObjectValue(aCx, &constructorValue)) {
      aRv.NoteJSContextException(aCx);
      return;
    }

    JSObject* defaultCtor = PromiseBinding::GetConstructorObject(aCx, global);
    if (!defaultCtor) {
      aRv.NoteJSContextException(aCx);
      return;
    }
    if (defaultCtor == &constructorValue.toObject()) {
      // This is the canonical Promise constructor.
      aCapability.mNativePromise = Promise::Create(aGlobal, aRv);
      if (aForceCallbackCreation) {
        // We have to be a bit careful here.  We want to create these functions
        // in the compartment in which they would be created if we actually
        // invoked the constructor via JS::Construct below.  That means our
        // callerGlobal compartment if aConstructor is an Xray and the reflector
        // compartment of the promise we're creating otherwise.  But note that
        // our callerGlobal compartment is precisely the reflector compartment
        // unless the call was done over Xrays, because the reflector
        // compartment comes from xpc::XrayAwareCalleeGlobal.  So we really just
        // want to create these functions in the callerGlobal compartment.
        MOZ_ASSERT(xpc::WrapperFactory::IsXrayWrapper(&aConstructor.toObject()) ||
                   callerGlobal == global);
        JSAutoCompartment ac2(aCx, callerGlobal);

        JSObject* resolveFuncObj =
          CreateFunction(aCx, aCapability.mNativePromise,
                         PromiseCallback::Resolve);
        if (!resolveFuncObj) {
          aRv.NoteJSContextException(aCx);
          return;
        }
        aCapability.mResolve.setObject(*resolveFuncObj);

        JSObject* rejectFuncObj =
          CreateFunction(aCx, aCapability.mNativePromise,
                         PromiseCallback::Reject);
        if (!rejectFuncObj) {
          aRv.NoteJSContextException(aCx);
          return;
        }
        aCapability.mReject.setObject(*rejectFuncObj);
      }
      return;
    }
  }

  // Step 4.
  // We can create our get-capabilities function in the calling compartment.  It
  // will work just as if we did |new promiseConstructor(function(a,b){}).
  // Notably, if we're called over Xrays that's all fine, because we will end up
  // creating the callbacks in the caller compartment in that case.
  JSFunction* getCapabilitiesFunc =
    js::NewFunctionWithReserved(aCx, GetCapabilitiesExecutor,
                                2 /* nargs */,
                                0 /* flags */,
                                nullptr);
  if (!getCapabilitiesFunc) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  JS::Rooted<JSObject*> getCapabilitiesObj(aCx);
  getCapabilitiesObj = JS_GetFunctionObject(getCapabilitiesFunc);

  // Step 5 doesn't need to be done, since we're not actually storing a
  // PromiseCapability in the executor; see the comments in
  // GetCapabilitiesExecutor above.

  // Step 6 and step 7.
  JS::Rooted<JS::Value> getCapabilities(aCx,
                                        JS::ObjectValue(*getCapabilitiesObj));
  JS::Rooted<JSObject*> promiseObj(aCx);
  if (!JS::Construct(aCx, aConstructor,
                     JS::HandleValueArray(getCapabilities),
                     &promiseObj)) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  // Step 8 plus copying over the value to the PromiseCapability.
  JS::Rooted<JS::Value> v(aCx);
  v = js::GetFunctionNativeReserved(getCapabilitiesObj,
                                    GET_CAPABILITIES_EXECUTOR_RESOLVE_SLOT);
  if (!v.isObject() || !JS::IsCallable(&v.toObject())) {
    aRv.ThrowTypeError<MSG_PROMISE_RESOLVE_FUNCTION_NOT_CALLABLE>();
    return;
  }
  aCapability.mResolve = v;

  // Step 9 plus copying over the value to the PromiseCapability.
  v = js::GetFunctionNativeReserved(getCapabilitiesObj,
                                    GET_CAPABILITIES_EXECUTOR_REJECT_SLOT);
  if (!v.isObject() || !JS::IsCallable(&v.toObject())) {
    aRv.ThrowTypeError<MSG_PROMISE_REJECT_FUNCTION_NOT_CALLABLE>();
    return;
  }
  aCapability.mReject = v;

  // Step 10.
  aCapability.mPromise = promiseObj;

  // Step 11 doesn't need anything, since the PromiseCapability was passed in.
}

/* static */ void
Promise::Resolve(const GlobalObject& aGlobal, JS::Handle<JS::Value> aThisv,
                 JS::Handle<JS::Value> aValue,
                 JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv)
{
  // Implementation of
  // http://www.ecma-international.org/ecma-262/6.0/#sec-promise.resolve

  JSContext* cx = aGlobal.Context();

  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  // Steps 1 and 2.
  if (!aThisv.isObject()) {
    aRv.ThrowTypeError<MSG_ILLEGAL_PROMISE_CONSTRUCTOR>();
    return;
  }

  // Step 3.  If a Promise was passed and matches our constructor, just return it.
  if (aValue.isObject()) {
    JS::Rooted<JSObject*> valueObj(cx, &aValue.toObject());
    Promise* nextPromise;
    nsresult rv = UNWRAP_OBJECT(Promise, valueObj, nextPromise);

    if (NS_SUCCEEDED(rv)) {
      JS::Rooted<JS::Value> constructor(cx);
      if (!JS_GetProperty(cx, valueObj, "constructor", &constructor)) {
        aRv.NoteJSContextException(cx);
        return;
      }

      // Cheat instead of calling JS_SameValue, since we know one's an object.
      if (aThisv == constructor) {
        aRetval.setObject(*valueObj);
        return;
      }
    }
  }

  // Step 4.
  PromiseCapability capability(cx);
  NewPromiseCapability(cx, global, aThisv, false, capability, aRv);
  // Step 5.
  if (aRv.Failed()) {
    return;
  }

  // Step 6.
  Promise* p = capability.mNativePromise;
  if (p) {
    p->MaybeResolveInternal(cx, aValue);
    p->mFullfillmentStack = p->mAllocationStack;
  } else {
    JS::Rooted<JS::Value> value(cx, aValue);
    JS::Rooted<JS::Value> ignored(cx);
    if (!JS::Call(cx, JS::UndefinedHandleValue /* thisVal */,
                  capability.mResolve, JS::HandleValueArray(value),
                  &ignored)) {
      // Step 7.
      aRv.NoteJSContextException(cx);
      return;
    }
  }

  // Step 8.
  aRetval.set(capability.PromiseValue());
}

/* static */ already_AddRefed<Promise>
Promise::Resolve(nsIGlobalObject* aGlobal, JSContext* aCx,
                 JS::Handle<JS::Value> aValue, ErrorResult& aRv)
{
  RefPtr<Promise> promise = Create(aGlobal, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  promise->MaybeResolveInternal(aCx, aValue);
  return promise.forget();
}

/* static */ void
Promise::Reject(const GlobalObject& aGlobal, JS::Handle<JS::Value> aThisv,
                JS::Handle<JS::Value> aValue,
                JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv)
{
  // Implementation of
  // http://www.ecma-international.org/ecma-262/6.0/#sec-promise.reject

  JSContext* cx = aGlobal.Context();

  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  // Steps 1 and 2.
  if (!aThisv.isObject()) {
    aRv.ThrowTypeError<MSG_ILLEGAL_PROMISE_CONSTRUCTOR>();
    return;
  }

  // Step 3.
  PromiseCapability capability(cx);
  NewPromiseCapability(cx, global, aThisv, false, capability, aRv);
  // Step 4.
  if (aRv.Failed()) {
    return;
  }

  // Step 5.
  Promise* p = capability.mNativePromise;
  if (p) {
    p->MaybeRejectInternal(cx, aValue);
    p->mRejectionStack = p->mAllocationStack;
  } else {
    JS::Rooted<JS::Value> value(cx, aValue);
    JS::Rooted<JS::Value> ignored(cx);
    if (!JS::Call(cx, JS::UndefinedHandleValue /* thisVal */,
                  capability.mReject, JS::HandleValueArray(value),
                  &ignored)) {
      // Step 6.
      aRv.NoteJSContextException(cx);
      return;
    }
  }

  // Step 7.
  aRetval.set(capability.PromiseValue());
}

/* static */ already_AddRefed<Promise>
Promise::Reject(nsIGlobalObject* aGlobal, JSContext* aCx,
                JS::Handle<JS::Value> aValue, ErrorResult& aRv)
{
  RefPtr<Promise> promise = Create(aGlobal, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  promise->MaybeRejectInternal(aCx, aValue);
  return promise.forget();
}

namespace {
void
SpeciesConstructor(JSContext* aCx,
                   JS::Handle<JSObject*> promise,
                   JS::Handle<JS::Value> defaultCtor,
                   JS::MutableHandle<JS::Value> ctor,
                   ErrorResult& aRv)
{
  // Implements
  // http://www.ecma-international.org/ecma-262/6.0/#sec-speciesconstructor

  // Step 1.
  MOZ_ASSERT(promise);

  // Step 2.
  JS::Rooted<JS::Value> constructorVal(aCx);
  if (!JS_GetProperty(aCx, promise, "constructor", &constructorVal)) {
    // Step 3.
    aRv.NoteJSContextException(aCx);
    return;
  }

  // Step 4.
  if (constructorVal.isUndefined()) {
    ctor.set(defaultCtor);
    return;
  }

  // Step 5.
  if (!constructorVal.isObject()) {
    aRv.ThrowTypeError<MSG_ILLEGAL_PROMISE_CONSTRUCTOR>();
    return;
  }

  // Step 6.
  JS::Rooted<jsid> species(aCx,
    SYMBOL_TO_JSID(JS::GetWellKnownSymbol(aCx, JS::SymbolCode::species)));
  JS::Rooted<JS::Value> speciesVal(aCx);
  JS::Rooted<JSObject*> constructorObj(aCx, &constructorVal.toObject());
  if (!JS_GetPropertyById(aCx, constructorObj, species, &speciesVal)) {
    // Step 7.
    aRv.NoteJSContextException(aCx);
    return;
  }

  // Step 8.
  if (speciesVal.isNullOrUndefined()) {
    ctor.set(defaultCtor);
    return;
  }

  // Step 9.
  if (speciesVal.isObject() && JS::IsConstructor(&speciesVal.toObject())) {
    ctor.set(speciesVal);
    return;
  }

  // Step 10.
  aRv.ThrowTypeError<MSG_ILLEGAL_PROMISE_CONSTRUCTOR>();
}
} // anonymous namespace

void
Promise::Then(JSContext* aCx, JS::Handle<JSObject*> aCalleeGlobal,
              AnyCallback* aResolveCallback, AnyCallback* aRejectCallback,
              JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv)
{
  // Implements
  // http://www.ecma-international.org/ecma-262/6.0/#sec-promise.prototype.then

  // Step 1.
  JS::Rooted<JS::Value> promiseVal(aCx, JS::ObjectValue(*GetWrapper()));
  if (!MaybeWrapObjectValue(aCx, &promiseVal)) {
    aRv.NoteJSContextException(aCx);
    return;
  }
  JS::Rooted<JSObject*> promiseObj(aCx, &promiseVal.toObject());
  MOZ_ASSERT(promiseObj);

  // Step 2 was done by the bindings.

  // Step 3.  We want to use aCalleeGlobal here because it will do the
  // right thing for us via Xrays (where we won't find @@species on
  // our promise constructor for now).
  JS::Rooted<JSObject*> calleeGlobal(aCx, aCalleeGlobal);
  JS::Rooted<JS::Value> defaultCtorVal(aCx);
  { // Scope for JSAutoCompartment
    JSAutoCompartment ac(aCx, aCalleeGlobal);
    JSObject* defaultCtor =
      PromiseBinding::GetConstructorObject(aCx, calleeGlobal);
    if (!defaultCtor) {
      aRv.NoteJSContextException(aCx);
      return;
    }
    defaultCtorVal.setObject(*defaultCtor);
  }
  if (!MaybeWrapObjectValue(aCx, &defaultCtorVal)) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  JS::Rooted<JS::Value> constructor(aCx);
  SpeciesConstructor(aCx, promiseObj, defaultCtorVal, &constructor, aRv);
  if (aRv.Failed()) {
    // Step 4.
    return;
  }

  // Step 5.
  GlobalObject globalObj(aCx, GetWrapper());
  if (globalObj.Failed()) {
    aRv.NoteJSContextException(aCx);
    return;
  }
  nsCOMPtr<nsIGlobalObject> globalObject =
    do_QueryInterface(globalObj.GetAsSupports());
  if (!globalObject) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  PromiseCapability capability(aCx);
  NewPromiseCapability(aCx, globalObject, constructor, false, capability, aRv);
  if (aRv.Failed()) {
    // Step 6.
    return;
  }

  // Now step 7: start
  // http://www.ecma-international.org/ecma-262/6.0/#sec-performpromisethen

  // Step 1 and step 2 are just assertions.

  // Step 3 and step 4 are kinda handled for us already; we use null
  // to represent "Identity" and "Thrower".

  // Steps 5 and 6.  These branch based on whether we know we have a
  // vanilla Promise or not.
  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
  if (capability.mNativePromise) {
    Promise* promise = capability.mNativePromise;

    RefPtr<PromiseCallback> resolveCb =
      PromiseCallback::Factory(promise, global, aResolveCallback,
                               PromiseCallback::Resolve);

    RefPtr<PromiseCallback> rejectCb =
      PromiseCallback::Factory(promise, global, aRejectCallback,
                               PromiseCallback::Reject);

    AppendCallbacks(resolveCb, rejectCb);
  } else {
    JS::Rooted<JSObject*> resolveObj(aCx, &capability.mResolve.toObject());
    RefPtr<AnyCallback> resolveFunc =
      new AnyCallback(aCx, resolveObj, GetIncumbentGlobal());

    JS::Rooted<JSObject*> rejectObj(aCx, &capability.mReject.toObject());
    RefPtr<AnyCallback> rejectFunc =
      new AnyCallback(aCx, rejectObj, GetIncumbentGlobal());

    if (!capability.mPromise) {
      aRv.ThrowTypeError<MSG_ILLEGAL_PROMISE_CONSTRUCTOR>();
      return;
    }
    JS::Rooted<JSObject*> newPromiseObj(aCx, capability.mPromise);
    // We want to store the reflector itself.
    newPromiseObj = js::CheckedUnwrap(newPromiseObj);
    if (!newPromiseObj) {
      // Just throw something.
      aRv.ThrowTypeError<MSG_ILLEGAL_PROMISE_CONSTRUCTOR>();
      return;
    }

    RefPtr<PromiseCallback> resolveCb;
    if (aResolveCallback) {
      resolveCb = new WrapperPromiseCallback(global, aResolveCallback,
                                             newPromiseObj,
                                             resolveFunc, rejectFunc);
    } else {
      resolveCb = new InvokePromiseFuncCallback(global, newPromiseObj,
                                                resolveFunc);
    }

    RefPtr<PromiseCallback> rejectCb;
    if (aRejectCallback) {
      rejectCb = new WrapperPromiseCallback(global, aRejectCallback,
                                            newPromiseObj,
                                            resolveFunc, rejectFunc);
    } else {
      rejectCb = new InvokePromiseFuncCallback(global, newPromiseObj,
                                               rejectFunc);
    }

    AppendCallbacks(resolveCb, rejectCb);
  }

  aRetval.set(capability.PromiseValue());
}

void
Promise::Catch(JSContext* aCx, AnyCallback* aRejectCallback,
               JS::MutableHandle<JS::Value> aRetval,
               ErrorResult& aRv)
{
  // Implements
  // http://www.ecma-international.org/ecma-262/6.0/#sec-promise.prototype.catch

  // We can't call Promise::Then directly, because someone might have
  // overridden Promise.prototype.then.
  JS::Rooted<JS::Value> promiseVal(aCx, JS::ObjectValue(*GetWrapper()));
  if (!MaybeWrapObjectValue(aCx, &promiseVal)) {
    aRv.NoteJSContextException(aCx);
    return;
  }
  JS::Rooted<JSObject*> promiseObj(aCx, &promiseVal.toObject());
  MOZ_ASSERT(promiseObj);
  JS::AutoValueArray<2> callbacks(aCx);
  callbacks[0].setUndefined();
  if (aRejectCallback) {
    callbacks[1].setObject(*aRejectCallback->Callable());
    // It could be in any compartment, so put it in ours.
    if (!MaybeWrapObjectValue(aCx, callbacks[1])) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  } else {
    callbacks[1].setNull();
  }
  if (!JS_CallFunctionName(aCx, promiseObj, "then", callbacks, aRetval)) {
    aRv.NoteJSContextException(aCx);
  }
}

/**
 * The CountdownHolder class encapsulates Promise.all countdown functions and
 * the countdown holder parts of the Promises spec. It maintains the result
 * array and AllResolveElementFunctions use SetValue() to set the array indices.
 */
class CountdownHolder final : public nsISupports
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

    AutoJSAPI jsapi;
    if (!jsapi.Init(mValues)) {
      return;
    }
    JSContext* cx = jsapi.cx();

    JS::Rooted<JS::Value> value(cx, aValue);
    JS::Rooted<JSObject*> values(cx, mValues);
    if (!JS_WrapValue(cx, &value) ||
        !JS_DefineElement(cx, values, index, value, JSPROP_ENUMERATE)) {
      MOZ_ASSERT(JS_IsExceptionPending(cx));
      JS::Rooted<JS::Value> exn(cx);
      jsapi.StealException(&exn);
      mPromise->MaybeReject(cx, exn);
    }

    --mCountdown;
    if (mCountdown == 0) {
      JS::Rooted<JS::Value> result(cx, JS::ObjectValue(*mValues));
      mPromise->MaybeResolve(cx, result);
    }
  }

private:
  RefPtr<Promise> mPromise;
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
 * An AllResolveElementFunction is the per-promise
 * part of the Promise.all() algorithm.
 * Every Promise in the handler is handed an instance of this as a resolution
 * handler and it sets the relevant index in the CountdownHolder.
 */
class AllResolveElementFunction final : public PromiseNativeHandler
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(AllResolveElementFunction)

  AllResolveElementFunction(CountdownHolder* aHolder, uint32_t aIndex)
    : mCountdownHolder(aHolder), mIndex(aIndex)
  {
    MOZ_ASSERT(aHolder);
  }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    mCountdownHolder->SetValue(mIndex, aValue);
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    // Should never be attached to Promise as a reject handler.
    MOZ_CRASH("AllResolveElementFunction should never be attached to a Promise's reject handler!");
  }

private:
  ~AllResolveElementFunction()
  {
  }

  RefPtr<CountdownHolder> mCountdownHolder;
  uint32_t mIndex;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(AllResolveElementFunction)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AllResolveElementFunction)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AllResolveElementFunction)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(AllResolveElementFunction, mCountdownHolder)

static const JSClass PromiseAllDataHolderClass = {
  "PromiseAllDataHolder", JSCLASS_HAS_RESERVED_SLOTS(3)
};

// Slot indices for objects of class PromiseAllDataHolderClass.
#define DATA_HOLDER_REMAINING_ELEMENTS_SLOT 0
#define DATA_HOLDER_VALUES_ARRAY_SLOT 1
#define DATA_HOLDER_RESOLVE_FUNCTION_SLOT 2

// Slot indices for PromiseAllResolveElement.
// The RESOLVE_ELEMENT_INDEX_SLOT stores our index unless we've already been
// called.  Then it stores INT32_MIN (which is never a valid index value).
#define RESOLVE_ELEMENT_INDEX_SLOT 0
// The RESOLVE_ELEMENT_DATA_HOLDER_SLOT slot stores an object of class
// PromiseAllDataHolderClass.
#define RESOLVE_ELEMENT_DATA_HOLDER_SLOT 1

static bool
PromiseAllResolveElement(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  // Implements
  // http://www.ecma-international.org/ecma-262/6.0/#sec-promise.all-resolve-element-functions
  //
  // See the big comment about compartments in Promise::All "Substep 4" that
  // explains what compartments the various stuff here lives in.
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);

  // Step 1.
  int32_t index =
    js::GetFunctionNativeReserved(&args.callee(),
                                  RESOLVE_ELEMENT_INDEX_SLOT).toInt32();
  // Step 2.
  if (index == INT32_MIN) {
    args.rval().setUndefined();
    return true;
  }

  // Step 3.
  js::SetFunctionNativeReserved(&args.callee(),
                                RESOLVE_ELEMENT_INDEX_SLOT,
                                JS::Int32Value(INT32_MIN));

  // Step 4 already done.

  // Step 5.
  JS::Rooted<JSObject*> dataHolder(aCx,
    &js::GetFunctionNativeReserved(&args.callee(),
                                   RESOLVE_ELEMENT_DATA_HOLDER_SLOT).toObject());

  JS::Rooted<JS::Value> values(aCx,
    js::GetReservedSlot(dataHolder, DATA_HOLDER_VALUES_ARRAY_SLOT));

  // Step 6, effectively.
  JS::Rooted<JS::Value> resolveFunc(aCx,
    js::GetReservedSlot(dataHolder, DATA_HOLDER_RESOLVE_FUNCTION_SLOT));

  // Step 7.
  int32_t remainingElements =
    js::GetReservedSlot(dataHolder, DATA_HOLDER_REMAINING_ELEMENTS_SLOT).toInt32();

  // Step 8.
  JS::Rooted<JSObject*> valuesObj(aCx, &values.toObject());
  if (!JS_DefineElement(aCx, valuesObj, index, args.get(0), JSPROP_ENUMERATE)) {
    return false;
  }

  // Step 9.
  remainingElements -= 1;
  js::SetReservedSlot(dataHolder, DATA_HOLDER_REMAINING_ELEMENTS_SLOT,
                      JS::Int32Value(remainingElements));

  // Step 10.
  if (remainingElements == 0) {
    return JS::Call(aCx, JS::UndefinedHandleValue, resolveFunc,
                    JS::HandleValueArray(values), args.rval());
  }

  // Step 11.
  args.rval().setUndefined();
  return true;
}


/* static */ void
Promise::All(const GlobalObject& aGlobal, JS::Handle<JS::Value> aThisv,
             JS::Handle<JS::Value> aIterable,
             JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv)
{
  // Implements http://www.ecma-international.org/ecma-262/6.0/#sec-promise.all
  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  JSContext* cx = aGlobal.Context();

  // Steps 1-5: nothing to do.  Note that the @@species bits got removed in
  // https://github.com/tc39/ecma262/pull/211

  // Step 6.
  PromiseCapability capability(cx);
  NewPromiseCapability(cx, global, aThisv, true, capability, aRv);
  // Step 7.
  if (aRv.Failed()) {
    return;
  }

  MOZ_ASSERT(aThisv.isObject(), "How did NewPromiseCapability succeed?");
  JS::Rooted<JSObject*> constructorObj(cx, &aThisv.toObject());

  // After this point we have a useful promise value in "capability", so just go
  // ahead and put it in our retval now.  Every single return path below would
  // want to do that anyway.
  aRetval.set(capability.PromiseValue());
  if (!MaybeWrapValue(cx, aRetval)) {
    aRv.NoteJSContextException(cx);
    return;
  }

  // The arguments we're going to be passing to "then" on each loop iteration.
  // The second one we know already; the first one will be created on each
  // iteration of the loop.
  JS::AutoValueArray<2> callbackFunctions(cx);
  callbackFunctions[1].set(capability.mReject);

  // Steps 8 and 9.
  JS::ForOfIterator iter(cx);
  if (!iter.init(aIterable, JS::ForOfIterator::AllowNonIterable)) {
    capability.RejectWithException(cx, aRv);
    return;
  }

  if (!iter.valueIsIterable()) {
    ThrowErrorMessage(cx, MSG_PROMISE_ARG_NOT_ITERABLE,
                      "Argument of Promise.all");
    capability.RejectWithException(cx, aRv);
    return;
  }

  // Step 10 doesn't need to be done, because ForOfIterator handles it
  // for us.

  // Now we jump over to
  // http://www.ecma-international.org/ecma-262/6.0/#sec-performpromiseall
  // and do its steps.

  // Substep 4. Create our data holder that holds all the things shared across
  // every step of the iterator.  In particular, this holds the
  // remainingElementsCount (as an integer reserved slot), the array of values,
  // and the resolve function from our PromiseCapability.
  //
  // We have to be very careful about which compartments we create things in
  // here.  In particular, we have to maintain the invariant that anything
  // stored in a reserved slot is same-compartment with the object whose
  // reserved slot it's in.  But we want to create the values array in the
  // Promise reflector compartment, because that array can get exposed to code
  // that has access to the Promise reflector (in particular code from that
  // compartment), and that should work, even if the Promise reflector
  // compartment is less-privileged than our caller compartment.
  //
  // So the plan is as follows: Create the values array in the promise reflector
  // compartment.  Create the PromiseAllResolveElement function and the data
  // holder in our current compartment.  Store a cross-compartment wrapper to
  // the values array in the holder.  This should be OK because the only things
  // we hand the PromiseAllResolveElement function to are the "then" calls we do
  // and in the case when the reflector compartment is not the current
  // compartment those are happening over Xrays anyway, which means they get the
  // canonical "then" function and content can't see our
  // PromiseAllResolveElement.
  JS::Rooted<JSObject*> dataHolder(cx);
  dataHolder = JS_NewObjectWithGivenProto(cx, &PromiseAllDataHolderClass,
                                          nullptr);
  if (!dataHolder) {
    capability.RejectWithException(cx, aRv);
    return;
  }

  JS::Rooted<JSObject*> reflectorGlobal(cx, global->GetGlobalJSObject());
  JS::Rooted<JSObject*> valuesArray(cx);
  { // Scope for JSAutoCompartment.
    JSAutoCompartment ac(cx, reflectorGlobal);
    valuesArray = JS_NewArrayObject(cx, 0);
  }
  if (!valuesArray) {
    // It's important that we've exited the JSAutoCompartment by now, before
    // calling RejectWithException and possibly invoking capability.mReject.
    capability.RejectWithException(cx, aRv);
    return;
  }

  // The values array as a value we can pass to a function in our current
  // compartment, or store in the holder's reserved slot.
  JS::Rooted<JS::Value> valuesArrayVal(cx, JS::ObjectValue(*valuesArray));
  if (!MaybeWrapObjectValue(cx, &valuesArrayVal)) {
    capability.RejectWithException(cx, aRv);
    return;
  }

  js::SetReservedSlot(dataHolder, DATA_HOLDER_REMAINING_ELEMENTS_SLOT,
                      JS::Int32Value(1));
  js::SetReservedSlot(dataHolder, DATA_HOLDER_VALUES_ARRAY_SLOT,
                      valuesArrayVal);
  js::SetReservedSlot(dataHolder, DATA_HOLDER_RESOLVE_FUNCTION_SLOT,
                      capability.mResolve);

  // Substep 5.
  CheckedInt32 index = 0;

  // Substep 6.
  JS::Rooted<JS::Value> nextValue(cx);
  while (true) {
    bool done;
    // Steps a, b, c, e, f, g.
    if (!iter.next(&nextValue, &done)) {
      capability.RejectWithException(cx, aRv);
      return;
    }

    // Step d.
    if (done) {
      int32_t remainingCount =
        js::GetReservedSlot(dataHolder,
                            DATA_HOLDER_REMAINING_ELEMENTS_SLOT).toInt32();
      remainingCount -= 1;
      if (remainingCount == 0) {
        JS::Rooted<JS::Value> ignored(cx);
        if (!JS::Call(cx, JS::UndefinedHandleValue, capability.mResolve,
                      JS::HandleValueArray(valuesArrayVal), &ignored)) {
          capability.RejectWithException(cx, aRv);
        }
        return;
      }
      js::SetReservedSlot(dataHolder, DATA_HOLDER_REMAINING_ELEMENTS_SLOT,
                          JS::Int32Value(remainingCount));
      // We're all set for now!
      return;
    }

    // Step h.
    { // Scope for the JSAutoCompartment we need to work with valuesArray.  We
      // mostly do this for performance; we could go ahead and do the define via
      // a cross-compartment proxy instead...
      JSAutoCompartment ac(cx, valuesArray);
      if (!JS_DefineElement(cx, valuesArray, index.value(),
                            JS::UndefinedHandleValue, JSPROP_ENUMERATE)) {
        // Have to go back into the caller compartment before we try to touch
        // capability.mReject.  Luckily, capability.mReject is guaranteed to be
        // an object in the right compartment here.
        JSAutoCompartment ac2(cx, &capability.mReject.toObject());
        capability.RejectWithException(cx, aRv);
        return;
      }
    }

    // Step i.  Sadly, we can't take a shortcut here even if
    // capability.mNativePromise exists, because someone could have overridden
    // "resolve" on the canonical Promise constructor.
    JS::Rooted<JS::Value> nextPromise(cx);
    if (!JS_CallFunctionName(cx, constructorObj, "resolve",
                             JS::HandleValueArray(nextValue),
                             &nextPromise)) {
      // Step j.
      capability.RejectWithException(cx, aRv);
      return;
    }

    // Step k.
    JS::Rooted<JSObject*> resolveElement(cx);
    JSFunction* resolveFunc =
      js::NewFunctionWithReserved(cx, PromiseAllResolveElement,
                                  1 /* nargs */, 0 /* flags */, nullptr);
    if (!resolveFunc) {
      capability.RejectWithException(cx, aRv);
      return;
    }

    resolveElement = JS_GetFunctionObject(resolveFunc);
    // Steps l-p.
    js::SetFunctionNativeReserved(resolveElement,
                                  RESOLVE_ELEMENT_INDEX_SLOT,
                                  JS::Int32Value(index.value()));
    js::SetFunctionNativeReserved(resolveElement,
                                  RESOLVE_ELEMENT_DATA_HOLDER_SLOT,
                                  JS::ObjectValue(*dataHolder));

    // Step q.
    int32_t remainingElements =
      js::GetReservedSlot(dataHolder, DATA_HOLDER_REMAINING_ELEMENTS_SLOT).toInt32();
    js::SetReservedSlot(dataHolder, DATA_HOLDER_REMAINING_ELEMENTS_SLOT,
                        JS::Int32Value(remainingElements + 1));

    // Step r.  And now we don't know whether nextPromise has an overridden
    // "then" method, so no shortcuts here either.
    callbackFunctions[0].setObject(*resolveElement);
    JS::Rooted<JSObject*> nextPromiseObj(cx);
    JS::Rooted<JS::Value> ignored(cx);
    if (!JS_ValueToObject(cx, nextPromise, &nextPromiseObj) ||
        !JS_CallFunctionName(cx, nextPromiseObj, "then", callbackFunctions,
                             &ignored)) {
      // Step s.
      capability.RejectWithException(cx, aRv);
    }

    // Step t.
    index += 1;
    if (!index.isValid()) {
      // Let's just claim OOM.
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      capability.RejectWithException(cx, aRv);
    }
  }
}

/* static */ already_AddRefed<Promise>
Promise::All(const GlobalObject& aGlobal,
             const nsTArray<RefPtr<Promise>>& aPromiseList, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  JSContext* cx = aGlobal.Context();

  if (aPromiseList.IsEmpty()) {
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

  RefPtr<Promise> promise = Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  RefPtr<CountdownHolder> holder =
    new CountdownHolder(aGlobal, promise, aPromiseList.Length());

  JS::Rooted<JSObject*> obj(cx, JS::CurrentGlobalOrNull(cx));
  if (!obj) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<PromiseCallback> rejectCb = new RejectPromiseCallback(promise, obj);

  for (uint32_t i = 0; i < aPromiseList.Length(); ++i) {
    RefPtr<PromiseNativeHandler> resolveHandler =
      new AllResolveElementFunction(holder, i);

    RefPtr<PromiseCallback> resolveCb =
      new NativePromiseCallback(resolveHandler, Resolved);

    // Every promise gets its own resolve callback, which will set the right
    // index in the array to the resolution value.
    aPromiseList[i]->AppendCallbacks(resolveCb, rejectCb);
  }

  return promise.forget();
}

/* static */ void
Promise::Race(const GlobalObject& aGlobal, JS::Handle<JS::Value> aThisv,
              JS::Handle<JS::Value> aIterable, JS::MutableHandle<JS::Value> aRetval,
              ErrorResult& aRv)
{
  // Implements http://www.ecma-international.org/ecma-262/6.0/#sec-promise.race
  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  JSContext* cx = aGlobal.Context();

  // Steps 1-5: nothing to do.  Note that the @@species bits got removed in
  // https://github.com/tc39/ecma262/pull/211
  PromiseCapability capability(cx);

  // Step 6.
  NewPromiseCapability(cx, global, aThisv, true, capability, aRv);
  // Step 7.
  if (aRv.Failed()) {
    return;
  }

  MOZ_ASSERT(aThisv.isObject(), "How did NewPromiseCapability succeed?");
  JS::Rooted<JSObject*> constructorObj(cx, &aThisv.toObject());

  // After this point we have a useful promise value in "capability", so just go
  // ahead and put it in our retval now.  Every single return path below would
  // want to do that anyway.
  aRetval.set(capability.PromiseValue());
  if (!MaybeWrapValue(cx, aRetval)) {
    aRv.NoteJSContextException(cx);
    return;
  }

  // The arguments we're going to be passing to "then" on each loop iteration.
  JS::AutoValueArray<2> callbackFunctions(cx);
  callbackFunctions[0].set(capability.mResolve);
  callbackFunctions[1].set(capability.mReject);

  // Steps 8 and 9.
  JS::ForOfIterator iter(cx);
  if (!iter.init(aIterable, JS::ForOfIterator::AllowNonIterable)) {
    capability.RejectWithException(cx, aRv);
    return;
  }

  if (!iter.valueIsIterable()) {
    ThrowErrorMessage(cx, MSG_PROMISE_ARG_NOT_ITERABLE,
                      "Argument of Promise.race");
    capability.RejectWithException(cx, aRv);
    return;
  }

  // Step 10 doesn't need to be done, because ForOfIterator handles it
  // for us.

  // Now we jump over to
  // http://www.ecma-international.org/ecma-262/6.0/#sec-performpromiserace
  // and do its steps.
  JS::Rooted<JS::Value> nextValue(cx);
  while (true) {
    bool done;
    // Steps a, b, c, e, f, g.
    if (!iter.next(&nextValue, &done)) {
      capability.RejectWithException(cx, aRv);
      return;
    }

    // Step d.
    if (done) {
      // We're all set!
      return;
    }

    // Step h.  Sadly, we can't take a shortcut here even if
    // capability.mNativePromise exists, because someone could have overridden
    // "resolve" on the canonical Promise constructor.
    JS::Rooted<JS::Value> nextPromise(cx);
    if (!JS_CallFunctionName(cx, constructorObj, "resolve",
                             JS::HandleValueArray(nextValue), &nextPromise)) {
      // Step i.
      capability.RejectWithException(cx, aRv);
      return;
    }

    // Step j.  And now we don't know whether nextPromise has an overridden
    // "then" method, so no shortcuts here either.
    JS::Rooted<JSObject*> nextPromiseObj(cx);
    JS::Rooted<JS::Value> ignored(cx);
    if (!JS_ValueToObject(cx, nextPromise, &nextPromiseObj) ||
        !JS_CallFunctionName(cx, nextPromiseObj, "then", callbackFunctions,
                             &ignored)) {
      // Step k.
      capability.RejectWithException(cx, aRv);
    }
  }
}

/* static */
bool
Promise::PromiseSpecies(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);
  args.rval().set(args.thisv());
  return true;
}

void
Promise::AppendNativeHandler(PromiseNativeHandler* aRunnable)
{
  RefPtr<PromiseCallback> resolveCb =
    new NativePromiseCallback(aRunnable, Resolved);

  RefPtr<PromiseCallback> rejectCb =
    new NativePromiseCallback(aRunnable, Rejected);

  AppendCallbacks(resolveCb, rejectCb);
}

#endif // SPIDERMONKEY_PROMISE

JSObject*
Promise::GlobalJSObject() const
{
  return mGlobal->GetGlobalJSObject();
}

JSCompartment*
Promise::Compartment() const
{
  return js::GetObjectCompartment(GlobalJSObject());
}

#ifndef SPIDERMONKEY_PROMISE
void
Promise::AppendCallbacks(PromiseCallback* aResolveCallback,
                         PromiseCallback* aRejectCallback)
{
  if (!mGlobal || mGlobal->IsDying()) {
    return;
  }

  MOZ_ASSERT(aResolveCallback);
  MOZ_ASSERT(aRejectCallback);

  if (mIsLastInChain && mState == PromiseState::Rejected) {
    // This rejection is now consumed.
    PromiseDebugging::AddConsumedRejection(*this);
    // Note that we may not have had the opportunity to call
    // RunResolveTask() yet, so we may never have called
    // `PromiseDebugging:AddUncaughtRejection`.
  }
  mIsLastInChain = false;

#if defined(DOM_PROMISE_DEPRECATED_REPORTING)
  // Now that there is a callback, we don't need to report anymore.
  mHadRejectCallback = true;
  RemoveFeature();
#endif // defined(DOM_PROMISE_DEPRECATED_REPORTING)

  mResolveCallbacks.AppendElement(aResolveCallback);
  mRejectCallbacks.AppendElement(aRejectCallback);

  // If promise's state is fulfilled, queue a task to process our fulfill
  // callbacks with promise's result. If promise's state is rejected, queue a
  // task to process our reject callbacks with promise's result.
  if (mState != Pending) {
    TriggerPromiseReactions();
  }
}
#endif // SPIDERMONKEY_PROMISE

#ifndef SPIDERMONKEY_PROMISE
#if defined(DOM_PROMISE_DEPRECATED_REPORTING)
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

  RefPtr<xpc::ErrorReport> xpcReport = new xpc::ErrorReport();
  bool isMainThread = MOZ_LIKELY(NS_IsMainThread());
  bool isChrome = isMainThread ? nsContentUtils::IsSystemPrincipal(nsContentUtils::ObjectPrincipal(obj))
                               : GetCurrentThreadWorkerPrivate()->IsChromeWorker();
  nsGlobalWindow* win = isMainThread ? xpc::WindowGlobalOrNull(obj) : nullptr;
  xpcReport->Init(report.report(), report.message(), isChrome, win ? win->AsInner()->WindowID() : 0);

  // Now post an event to do the real reporting async
  // Since Promises preserve their wrapper, it is essential to RefPtr<> the
  // AsyncErrorReporter, otherwise if the call to DispatchToMainThread fails, it
  // will leak. See Bug 958684.  So... don't use DispatchToMainThread()
  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  if (NS_WARN_IF(!mainThread)) {
    // Would prefer NS_ASSERTION, but that causes failure in xpcshell tests
    NS_WARNING("!!! Trying to report rejected Promise after MainThread shutdown");
  }
  if (mainThread) {
    RefPtr<AsyncErrorReporter> r = new AsyncErrorReporter(xpcReport);
    mainThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  }
}
#endif // defined(DOM_PROMISE_DEPRECATED_REPORTING)

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
  CycleCollectedJSRuntime* runtime = CycleCollectedJSRuntime::Get();

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

      // We used to have a fast path here for the case when the following
      // requirements held:
      //
      // 1) valueObj is a Promise.
      // 2) thenObj is a JSFunction backed by our actual Promise::Then
      //    implementation.
      //
      // But now that we're doing subclassing in Promise.prototype.then we would
      // also need the following requirements:
      //
      // 3) Getting valueObj.constructor has no side-effects.
      // 4) Getting valueObj.constructor[@@species] has no side-effects.
      // 5) valueObj.constructor[@@species] is a function and calling it has no
      //    side-effects (e.g. it's the canonical Promise constructor) and it
      //    provides some callback functions to call as arguments to its
      //    argument.
      //
      // Ensuring that stuff while not inside SpiderMonkey is painful, so let's
      // drop the fast path for now.

      RefPtr<PromiseInit> thenCallback =
        new PromiseInit(nullptr, thenObj, mozilla::dom::GetIncumbentGlobal());
      RefPtr<PromiseResolveThenableJob> task =
        new PromiseResolveThenableJob(this, valueObj, thenCallback);
      runtime->DispatchToMicroTask(task);
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
Promise::Settle(JS::Handle<JS::Value> aValue, PromiseState aState)
{
  MOZ_ASSERT(mGlobal,
             "We really should have a global here.  Except we sometimes don't "
             "in the wild for some odd reason");
  if (!mGlobal || mGlobal->IsDying()) {
    return;
  }

  mSettlementTimestamp = TimeStamp::Now();

  SetResult(aValue);
  SetState(aState);

  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  JS::RootedObject wrapper(cx, GetWrapper());
  MOZ_ASSERT(wrapper); // We preserved it
  JSAutoCompartment ac(cx, wrapper);
  JS::dbg::onPromiseSettled(cx, wrapper);

  if (aState == PromiseState::Rejected &&
      mIsLastInChain) {
    // The Promise has just been rejected, and it is last in chain.
    // We need to inform PromiseDebugging.
    // If the Promise is eventually not the last in chain anymore,
    // we will need to inform PromiseDebugging again.
    PromiseDebugging::AddUncaughtRejection(*this);
  }

#if defined(DOM_PROMISE_DEPRECATED_REPORTING)
  // If the Promise was rejected, and there is no reject handler already setup,
  // watch for thread shutdown.
  if (aState == PromiseState::Rejected &&
      !mHadRejectCallback &&
      !NS_IsMainThread()) {
    workers::WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    worker->AssertIsOnWorkerThread();

    mFeature = new PromiseReportRejectFeature(this);
    if (NS_WARN_IF(!worker->AddFeature(mFeature))) {
      // To avoid a false RemoveFeature().
      mFeature = nullptr;
      // Worker is shutting down, report rejection immediately since it is
      // unlikely that reject callbacks will be added after this point.
      MaybeReportRejectedOnce();
    }
  }
#endif // defined(DOM_PROMISE_DEPRECATED_REPORTING)

  TriggerPromiseReactions();
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

  Settle(aValue, aState);
}

void
Promise::TriggerPromiseReactions()
{
  CycleCollectedJSRuntime* runtime = CycleCollectedJSRuntime::Get();

  nsTArray<RefPtr<PromiseCallback>> callbacks;
  callbacks.SwapElements(mState == Resolved ? mResolveCallbacks
                                            : mRejectCallbacks);
  mResolveCallbacks.Clear();
  mRejectCallbacks.Clear();

  for (uint32_t i = 0; i < callbacks.Length(); ++i) {
    RefPtr<PromiseReactionJob> task =
      new PromiseReactionJob(this, callbacks[i], mResult);
    runtime->DispatchToMicroTask(task);
  }
}

#if defined(DOM_PROMISE_DEPRECATED_REPORTING)
void
Promise::RemoveFeature()
{
  if (mFeature) {
    workers::WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    worker->RemoveFeature(mFeature);
    mFeature = nullptr;
  }
}

bool
PromiseReportRejectFeature::Notify(workers::Status aStatus)
{
  MOZ_ASSERT(aStatus > workers::Running);
  mPromise->MaybeReportRejectedOnce();
  // After this point, `this` has been deleted by RemoveFeature!
  return true;
}
#endif // defined(DOM_PROMISE_DEPRECATED_REPORTING)

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
Promise::GetDependentPromises(nsTArray<RefPtr<Promise>>& aPromises)
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

#endif // SPIDERMONKEY_PROMISE

// A WorkerRunnable to resolve/reject the Promise on the worker thread.
// Calling thread MUST hold PromiseWorkerProxy's mutex before creating this.
class PromiseWorkerProxyRunnable : public workers::WorkerRunnable
{
public:
  PromiseWorkerProxyRunnable(PromiseWorkerProxy* aPromiseWorkerProxy,
                             PromiseWorkerProxy::RunCallbackFunc aFunc)
    : WorkerRunnable(aPromiseWorkerProxy->GetWorkerPrivate(),
                     WorkerThreadUnchangedBusyCount)
    , mPromiseWorkerProxy(aPromiseWorkerProxy)
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
    RefPtr<Promise> workerPromise = mPromiseWorkerProxy->WorkerPromise();

    // Here we convert the buffer to a JS::Value.
    JS::Rooted<JS::Value> value(aCx);
    if (!mPromiseWorkerProxy->Read(aCx, &value)) {
      JS_ClearPendingException(aCx);
      return false;
    }

    (workerPromise->*mFunc)(aCx, value);

    // Release the Promise because it has been resolved/rejected for sure.
    mPromiseWorkerProxy->CleanUp();
    return true;
  }

protected:
  ~PromiseWorkerProxyRunnable() {}

private:
  RefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;

  // Function pointer for calling Promise::{ResolveInternal,RejectInternal}.
  PromiseWorkerProxy::RunCallbackFunc mFunc;
};

/* static */
already_AddRefed<PromiseWorkerProxy>
PromiseWorkerProxy::Create(workers::WorkerPrivate* aWorkerPrivate,
                           Promise* aWorkerPromise,
                           const PromiseWorkerProxyStructuredCloneCallbacks* aCb)
{
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(aWorkerPromise);
  MOZ_ASSERT_IF(aCb, !!aCb->Write && !!aCb->Read);

  RefPtr<PromiseWorkerProxy> proxy =
    new PromiseWorkerProxy(aWorkerPrivate, aWorkerPromise, aCb);

  // We do this to make sure the worker thread won't shut down before the
  // promise is resolved/rejected on the worker thread.
  if (!proxy->AddRefObject()) {
    // Probably the worker is terminating. We cannot complete the operation
    // and we have to release all the resources.
    proxy->CleanProperties();
    return nullptr;
  }

  return proxy.forget();
}

NS_IMPL_ISUPPORTS0(PromiseWorkerProxy)

PromiseWorkerProxy::PromiseWorkerProxy(workers::WorkerPrivate* aWorkerPrivate,
                                       Promise* aWorkerPromise,
                                       const PromiseWorkerProxyStructuredCloneCallbacks* aCallbacks)
  : mWorkerPrivate(aWorkerPrivate)
  , mWorkerPromise(aWorkerPromise)
  , mCleanedUp(false)
  , mCallbacks(aCallbacks)
  , mCleanUpLock("cleanUpLock")
#ifdef DEBUG
  , mFeatureAdded(false)
#endif
{
}

PromiseWorkerProxy::~PromiseWorkerProxy()
{
  MOZ_ASSERT(mCleanedUp);
  MOZ_ASSERT(!mFeatureAdded);
  MOZ_ASSERT(!mWorkerPromise);
  MOZ_ASSERT(!mWorkerPrivate);
}

void
PromiseWorkerProxy::CleanProperties()
{
#ifdef DEBUG
  workers::WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();
#endif
  // Ok to do this unprotected from Create().
  // CleanUp() holds the lock before calling this.
  mCleanedUp = true;
  mWorkerPromise = nullptr;
  mWorkerPrivate = nullptr;

  // Clear the StructuredCloneHolderBase class.
  Clear();
}

bool
PromiseWorkerProxy::AddRefObject()
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(!mFeatureAdded);
  if (!mWorkerPrivate->AddFeature(this)) {
    return false;
  }

#ifdef DEBUG
  mFeatureAdded = true;
#endif
  // Maintain a reference so that we have a valid object to clean up when
  // removing the feature.
  AddRef();
  return true;
}

workers::WorkerPrivate*
PromiseWorkerProxy::GetWorkerPrivate() const
{
#ifdef DEBUG
  if (NS_IsMainThread()) {
    mCleanUpLock.AssertCurrentThreadOwns();
  }
#endif
  // Safe to check this without a lock since we assert lock ownership on the
  // main thread above.
  MOZ_ASSERT(!mCleanedUp);
  MOZ_ASSERT(mFeatureAdded);

  return mWorkerPrivate;
}

Promise*
PromiseWorkerProxy::WorkerPromise() const
{
#ifdef DEBUG
  workers::WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();
#endif
  MOZ_ASSERT(mWorkerPromise);
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

  MutexAutoLock lock(Lock());
  // If the worker thread's been cancelled we don't need to resolve the Promise.
  if (CleanedUp()) {
    return;
  }

  // The |aValue| is written into the StructuredCloneHolderBase.
  if (!Write(aCx, aValue)) {
    JS_ClearPendingException(aCx);
    MOZ_ASSERT(false, "cannot serialize the value with the StructuredCloneAlgorithm!");
  }

  RefPtr<PromiseWorkerProxyRunnable> runnable =
    new PromiseWorkerProxyRunnable(this, aFunc);

  runnable->Dispatch();
}

void
PromiseWorkerProxy::ResolvedCallback(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue)
{
  RunCallback(aCx, aValue, &Promise::MaybeResolve);
}

void
PromiseWorkerProxy::RejectedCallback(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue)
{
  RunCallback(aCx, aValue, &Promise::MaybeReject);
}

bool
PromiseWorkerProxy::Notify(Status aStatus)
{
  if (aStatus >= Canceling) {
    CleanUp();
  }

  return true;
}

void
PromiseWorkerProxy::CleanUp()
{
  // Can't release Mutex while it is still locked, so scope the lock.
  {
    MutexAutoLock lock(Lock());

    // |mWorkerPrivate| is not safe to use anymore if we have already
    // cleaned up and RemoveFeature(), so we need to check |mCleanedUp| first.
    if (CleanedUp()) {
      return;
    }

    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();

    // Release the Promise and remove the PromiseWorkerProxy from the features of
    // the worker thread since the Promise has been resolved/rejected or the
    // worker thread has been cancelled.
    MOZ_ASSERT(mFeatureAdded);
    mWorkerPrivate->RemoveFeature(this);
#ifdef DEBUG
    mFeatureAdded = false;
#endif
    CleanProperties();
  }
  Release();
}

JSObject*
PromiseWorkerProxy::CustomReadHandler(JSContext* aCx,
                                      JSStructuredCloneReader* aReader,
                                      uint32_t aTag,
                                      uint32_t aIndex)
{
  if (NS_WARN_IF(!mCallbacks)) {
    return nullptr;
  }

  return mCallbacks->Read(aCx, aReader, this, aTag, aIndex);
}

bool
PromiseWorkerProxy::CustomWriteHandler(JSContext* aCx,
                                       JSStructuredCloneWriter* aWriter,
                                       JS::Handle<JSObject*> aObj)
{
  if (NS_WARN_IF(!mCallbacks)) {
    return false;
  }

  return mCallbacks->Write(aCx, aWriter, this, aObj);
}

// Specializations of MaybeRejectBrokenly we actually support.
template<>
void Promise::MaybeRejectBrokenly(const RefPtr<DOMError>& aArg) {
  MaybeSomething(aArg, &Promise::MaybeReject);
}
template<>
void Promise::MaybeRejectBrokenly(const nsAString& aArg) {
  MaybeSomething(aArg, &Promise::MaybeReject);
}

#ifndef SPIDERMONKEY_PROMISE
uint64_t
Promise::GetID() {
  if (mID != 0) {
    return mID;
  }
  return mID = ++gIDGenerator;
}
#endif // SPIDERMONKEY_PROMISE

} // namespace dom
} // namespace mozilla
