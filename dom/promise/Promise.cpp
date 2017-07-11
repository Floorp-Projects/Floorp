/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"

#include "js/Debug.h"

#include "mozilla/Atomics.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/Preferences.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"
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

// Promise

NS_IMPL_CYCLE_COLLECTION_CLASS(Promise)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Promise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  tmp->mPromiseObj = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Promise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Promise)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mPromiseObj);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Promise)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Promise)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Promise)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(Promise)
NS_INTERFACE_MAP_END

Promise::Promise(nsIGlobalObject* aGlobal)
  : mGlobal(aGlobal)
  , mPromiseObj(nullptr)
{
  MOZ_ASSERT(mGlobal);

  mozilla::HoldJSObjects(this);
}

Promise::~Promise()
{
  mozilla::DropJSObjects(this);
}

// static
already_AddRefed<Promise>
Promise::Create(nsIGlobalObject* aGlobal, ErrorResult& aRv)
{
  if (!aGlobal) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
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
  NS_ASSERT_OWNINGTHREAD(Promise);

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
    resolveCallback = aResolveCallback->CallbackOrNull();
    if (!JS_WrapObject(aCx, &resolveCallback)) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }

  JS::Rooted<JSObject*> rejectCallback(aCx);
  if (aRejectCallback) {
    rejectCallback = aRejectCallback->CallbackOrNull();
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

void
Promise::CreateWrapper(JS::Handle<JSObject*> aDesiredProto, ErrorResult& aRv)
{
  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobal)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  JSContext* cx = jsapi.cx();
  mPromiseObj = JS::NewPromiseObject(cx, nullptr, aDesiredProto);
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
  NS_ASSERT_OWNINGTHREAD(Promise);

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
  NS_ASSERT_OWNINGTHREAD(Promise);

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

  JS::Value v = js::GetFunctionNativeReserved(&args.callee(),
                                              SLOT_NATIVEHANDLER);
  MOZ_ASSERT(v.isObject());

  JS::Rooted<JSObject*> obj(aCx, &v.toObject());
  PromiseNativeHandler* handler = nullptr;
  if (NS_FAILED(UNWRAP_OBJECT(PromiseNativeHandler, &obj, handler))) {
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

namespace {

class PromiseNativeHandlerShim final : public PromiseNativeHandler
{
  RefPtr<PromiseNativeHandler> mInner;

  ~PromiseNativeHandlerShim()
  {
  }

public:
  explicit PromiseNativeHandlerShim(PromiseNativeHandler* aInner)
    : mInner(aInner)
  {
    MOZ_ASSERT(mInner);
  }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    mInner->ResolvedCallback(aCx, aValue);
    mInner = nullptr;
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    mInner->RejectedCallback(aCx, aValue);
    mInner = nullptr;
  }

  bool
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
             JS::MutableHandle<JSObject*> aWrapper)
  {
    return PromiseNativeHandlerBinding::Wrap(aCx, this, aGivenProto, aWrapper);
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(PromiseNativeHandlerShim)
};

NS_IMPL_CYCLE_COLLECTION(PromiseNativeHandlerShim, mInner)

NS_IMPL_CYCLE_COLLECTING_ADDREF(PromiseNativeHandlerShim)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PromiseNativeHandlerShim)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PromiseNativeHandlerShim)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

} // anonymous namespace

void
Promise::AppendNativeHandler(PromiseNativeHandler* aRunnable)
{
  NS_ASSERT_OWNINGTHREAD(Promise);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mGlobal))) {
    // Our API doesn't allow us to return a useful error.  Not like this should
    // happen anyway.
    return;
  }

  // The self-hosted promise js may keep the object we pass to it alive
  // for quite a while depending on when GC runs.  Therefore, pass a shim
  // object instead.  The shim will free its inner PromiseNativeHandler
  // after the promise has settled just like our previous c++ promises did.
  RefPtr<PromiseNativeHandlerShim> shim =
    new PromiseNativeHandlerShim(aRunnable);

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> handlerWrapper(cx);
  // Note: PromiseNativeHandler is NOT wrappercached.  So we can't use
  // ToJSValue here, because it will try to do XPConnect wrapping on it, sadly.
  if (NS_WARN_IF(!shim->WrapObject(cx, nullptr, &handlerWrapper))) {
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
    // This is only called from MaybeSomething, so it's OK to MaybeReject here.
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


void
Promise::MaybeResolveWithUndefined()
{
  NS_ASSERT_OWNINGTHREAD(Promise);

  MaybeResolve(JS::UndefinedHandleValue);
}

void
Promise::MaybeReject(const RefPtr<MediaStreamError>& aArg) {
  NS_ASSERT_OWNINGTHREAD(Promise);

  MaybeSomething(aArg, &Promise::MaybeReject);
}

void
Promise::MaybeRejectWithUndefined()
{
  NS_ASSERT_OWNINGTHREAD(Promise);

  MaybeSomething(JS::UndefinedHandleValue, &Promise::MaybeReject);
}

void
Promise::ReportRejectedPromise(JSContext* aCx, JS::HandleObject aPromise)
{
  MOZ_ASSERT(!js::IsWrapper(aPromise));

  MOZ_ASSERT(JS::GetPromiseState(aPromise) == JS::PromiseState::Rejected);

  JS::Rooted<JS::Value> result(aCx, JS::GetPromiseResult(aPromise));

  js::ErrorReport report(aCx);
  if (!report.init(aCx, result, js::ErrorReport::NoSideEffects)) {
    JS_ClearPendingException(aCx);
    return;
  }

  RefPtr<xpc::ErrorReport> xpcReport = new xpc::ErrorReport();
  bool isMainThread = MOZ_LIKELY(NS_IsMainThread());
  bool isChrome = isMainThread ? nsContentUtils::IsSystemPrincipal(nsContentUtils::ObjectPrincipal(aPromise))
                               : GetCurrentThreadWorkerPrivate()->IsChromeWorker();
  nsGlobalWindow* win = isMainThread ? xpc::WindowGlobalOrNull(aPromise) : nullptr;
  xpcReport->Init(report.report(), report.toStringResult().c_str(), isChrome,
                  win ? win->AsInner()->WindowID() : 0);

  // Now post an event to do the real reporting async
  NS_DispatchToMainThread(new AsyncErrorReporter(xpcReport));
}

bool
Promise::PerformMicroTaskCheckpoint()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  CycleCollectedJSContext* context = CycleCollectedJSContext::Get();

  // On the main thread, we always use the main promise micro task queue.
  std::queue<nsCOMPtr<nsIRunnable>>& microtaskQueue =
    context->GetPromiseMicroTaskQueue();

  if (microtaskQueue.empty()) {
    return false;
  }

  AutoSlowOperation aso;

  do {
    nsCOMPtr<nsIRunnable> runnable = microtaskQueue.front().forget();
    MOZ_ASSERT(runnable);

    // This function can re-enter, so we remove the element before calling.
    microtaskQueue.pop();
    nsresult rv = runnable->Run();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
    aso.CheckForInterrupt();
    context->AfterProcessMicrotask();
  } while (!microtaskQueue.empty());

  return true;
}

void
Promise::PerformWorkerMicroTaskCheckpoint()
{
  MOZ_ASSERT(!NS_IsMainThread(), "Wrong thread!");

  CycleCollectedJSContext* context = CycleCollectedJSContext::Get();
  if (!context) {
    return;
  }

  for (;;) {
    // For a normal microtask checkpoint, we try to use the debugger microtask
    // queue first. If the debugger queue is empty, we use the normal microtask
    // queue instead.
    std::queue<nsCOMPtr<nsIRunnable>>* microtaskQueue =
      &context->GetDebuggerPromiseMicroTaskQueue();

    if (microtaskQueue->empty()) {
      microtaskQueue = &context->GetPromiseMicroTaskQueue();
      if (microtaskQueue->empty()) {
        break;
      }
    }

    nsCOMPtr<nsIRunnable> runnable = microtaskQueue->front().forget();
    MOZ_ASSERT(runnable);

    // This function can re-enter, so we remove the element before calling.
    microtaskQueue->pop();
    nsresult rv = runnable->Run();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
    context->AfterProcessMicrotask();
  }
}

void
Promise::PerformWorkerDebuggerMicroTaskCheckpoint()
{
  MOZ_ASSERT(!NS_IsMainThread(), "Wrong thread!");

  CycleCollectedJSContext* context = CycleCollectedJSContext::Get();
  if (!context) {
    return;
  }

  for (;;) {
    // For a debugger microtask checkpoint, we always use the debugger microtask
    // queue.
    std::queue<nsCOMPtr<nsIRunnable>>* microtaskQueue =
      &context->GetDebuggerPromiseMicroTaskQueue();

    if (microtaskQueue->empty()) {
      break;
    }

    nsCOMPtr<nsIRunnable> runnable = microtaskQueue->front().forget();
    MOZ_ASSERT(runnable);

    // This function can re-enter, so we remove the element before calling.
    microtaskQueue->pop();
    nsresult rv = runnable->Run();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
    context->AfterProcessMicrotask();
  }
}

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

// A WorkerRunnable to resolve/reject the Promise on the worker thread.
// Calling thread MUST hold PromiseWorkerProxy's mutex before creating this.
class PromiseWorkerProxyRunnable : public WorkerRunnable
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
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
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

class PromiseWorkerHolder final : public WorkerHolder
{
  // RawPointer because this proxy keeps alive the holder.
  PromiseWorkerProxy* mProxy;

public:
  explicit PromiseWorkerHolder(PromiseWorkerProxy* aProxy)
    : mProxy(aProxy)
  {
    MOZ_ASSERT(aProxy);
  }

  bool
  Notify(Status aStatus) override
  {
    if (aStatus >= Canceling) {
      mProxy->CleanUp();
    }

    return true;
  }
};

/* static */
already_AddRefed<PromiseWorkerProxy>
PromiseWorkerProxy::Create(WorkerPrivate* aWorkerPrivate,
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

PromiseWorkerProxy::PromiseWorkerProxy(WorkerPrivate* aWorkerPrivate,
                                       Promise* aWorkerPromise,
                                       const PromiseWorkerProxyStructuredCloneCallbacks* aCallbacks)
  : mWorkerPrivate(aWorkerPrivate)
  , mWorkerPromise(aWorkerPromise)
  , mCleanedUp(false)
  , mCallbacks(aCallbacks)
  , mCleanUpLock("cleanUpLock")
{
}

PromiseWorkerProxy::~PromiseWorkerProxy()
{
  MOZ_ASSERT(mCleanedUp);
  MOZ_ASSERT(!mWorkerHolder);
  MOZ_ASSERT(!mWorkerPromise);
  MOZ_ASSERT(!mWorkerPrivate);
}

void
PromiseWorkerProxy::CleanProperties()
{
#ifdef DEBUG
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
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

  MOZ_ASSERT(!mWorkerHolder);
  mWorkerHolder.reset(new PromiseWorkerHolder(this));
  if (NS_WARN_IF(!mWorkerHolder->HoldWorker(mWorkerPrivate, Canceling))) {
    mWorkerHolder = nullptr;
    return false;
  }

  // Maintain a reference so that we have a valid object to clean up when
  // removing the feature.
  AddRef();
  return true;
}

WorkerPrivate*
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
  MOZ_ASSERT(mWorkerHolder);

  return mWorkerPrivate;
}

Promise*
PromiseWorkerProxy::WorkerPromise() const
{
#ifdef DEBUG
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();
#endif
  MOZ_ASSERT(mWorkerPromise);
  return mWorkerPromise;
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

void
PromiseWorkerProxy::CleanUp()
{
  // Can't release Mutex while it is still locked, so scope the lock.
  {
    MutexAutoLock lock(Lock());

    // |mWorkerPrivate| is not safe to use anymore if we have already
    // cleaned up and RemoveWorkerHolder(), so we need to check |mCleanedUp|
    // first.
    if (CleanedUp()) {
      return;
    }

    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();

    // Release the Promise and remove the PromiseWorkerProxy from the holders of
    // the worker thread since the Promise has been resolved/rejected or the
    // worker thread has been cancelled.
    MOZ_ASSERT(mWorkerHolder);
    mWorkerHolder = nullptr;

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

Promise::PromiseState
Promise::State() const
{
  JS::Rooted<JSObject*> p(RootingCx(), PromiseObj());
  const JS::PromiseState state = JS::GetPromiseState(p);

  if (state == JS::PromiseState::Fulfilled) {
      return PromiseState::Resolved;
  }

  if (state == JS::PromiseState::Rejected) {
      return PromiseState::Rejected;
  }

  return PromiseState::Pending;
}

} // namespace dom
} // namespace mozilla
