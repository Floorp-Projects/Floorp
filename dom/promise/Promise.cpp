/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"

#include "js/Debug.h"

#include "mozilla/Atomics.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/Preferences.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Unused.h"

#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/MediaStreamError.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkletImpl.h"
#include "mozilla/dom/WorkletGlobalScope.h"

#include "jsfriendapi.h"
#include "js/Exception.h"  // JS::ExceptionStack
#include "js/Object.h"     // JS::GetCompartment
#include "js/StructuredClone.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGlobalWindow.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsJSEnvironment.h"
#include "nsJSPrincipals.h"
#include "nsJSUtils.h"
#include "nsPIDOMWindow.h"
#include "PromiseDebugging.h"
#include "PromiseNativeHandler.h"
#include "PromiseWorkerProxy.h"
#include "WrapperFactory.h"
#include "xpcpublic.h"
#include "xpcprivate.h"

namespace mozilla::dom {

// Promise

NS_IMPL_CYCLE_COLLECTION_SINGLE_ZONE_SCRIPT_HOLDER_CLASS(Promise)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Promise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR
  tmp->mPromiseObj = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Promise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Promise)
  // If you add new JS member variables, you may need to stop using
  // NS_IMPL_CYCLE_COLLECTION_SINGLE_ZONE_SCRIPT_HOLDER_CLASS.
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mPromiseObj);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(Promise, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(Promise, Release)

Promise::Promise(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal), mPromiseObj(nullptr) {
  MOZ_ASSERT(mGlobal);

  mozilla::HoldJSObjects(this);
}

Promise::~Promise() { mozilla::DropJSObjects(this); }

// static
already_AddRefed<Promise> Promise::Create(
    nsIGlobalObject* aGlobal, ErrorResult& aRv,
    PropagateUserInteraction aPropagateUserInteraction) {
  if (!aGlobal) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  RefPtr<Promise> p = new Promise(aGlobal);
  p->CreateWrapper(aRv, aPropagateUserInteraction);
  if (aRv.Failed()) {
    return nullptr;
  }
  return p.forget();
}

bool Promise::MaybePropagateUserInputEventHandling() {
  JS::PromiseUserInputEventHandlingState state =
      UserActivation::IsHandlingUserInput()
          ? JS::PromiseUserInputEventHandlingState::HadUserInteractionAtCreation
          : JS::PromiseUserInputEventHandlingState::
                DidntHaveUserInteractionAtCreation;
  JS::Rooted<JSObject*> p(RootingCx(), mPromiseObj);
  return JS::SetPromiseUserInputEventHandlingState(p, state);
}

// static
already_AddRefed<Promise> Promise::Resolve(
    nsIGlobalObject* aGlobal, JSContext* aCx, JS::Handle<JS::Value> aValue,
    ErrorResult& aRv, PropagateUserInteraction aPropagateUserInteraction) {
  JSAutoRealm ar(aCx, aGlobal->GetGlobalJSObject());
  JS::Rooted<JSObject*> p(aCx, JS::CallOriginalPromiseResolve(aCx, aValue));
  if (!p) {
    aRv.NoteJSContextException(aCx);
    return nullptr;
  }

  return CreateFromExisting(aGlobal, p, aPropagateUserInteraction);
}

// static
already_AddRefed<Promise> Promise::Reject(nsIGlobalObject* aGlobal,
                                          JSContext* aCx,
                                          JS::Handle<JS::Value> aValue,
                                          ErrorResult& aRv) {
  JSAutoRealm ar(aCx, aGlobal->GetGlobalJSObject());
  JS::Rooted<JSObject*> p(aCx, JS::CallOriginalPromiseReject(aCx, aValue));
  if (!p) {
    aRv.NoteJSContextException(aCx);
    return nullptr;
  }

  // This promise will never be resolved, so we pass
  // eDontPropagateUserInteraction for aPropagateUserInteraction
  // unconditionally.
  return CreateFromExisting(aGlobal, p, eDontPropagateUserInteraction);
}

// static
already_AddRefed<Promise> Promise::All(
    JSContext* aCx, const nsTArray<RefPtr<Promise>>& aPromiseList,
    ErrorResult& aRv, PropagateUserInteraction aPropagateUserInteraction) {
  JS::Rooted<JSObject*> globalObj(aCx, JS::CurrentGlobalOrNull(aCx));
  if (!globalObj) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = xpc::NativeGlobal(globalObj);
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  JS::RootedVector<JSObject*> promises(aCx);
  if (!promises.reserve(aPromiseList.Length())) {
    aRv.NoteJSContextException(aCx);
    return nullptr;
  }

  for (auto& promise : aPromiseList) {
    JS::Rooted<JSObject*> promiseObj(aCx, promise->PromiseObj());
    // Just in case, make sure these are all in the context compartment.
    if (!JS_WrapObject(aCx, &promiseObj)) {
      aRv.NoteJSContextException(aCx);
      return nullptr;
    }
    promises.infallibleAppend(promiseObj);
  }

  JS::Rooted<JSObject*> result(aCx, JS::GetWaitForAllPromise(aCx, promises));
  if (!result) {
    aRv.NoteJSContextException(aCx);
    return nullptr;
  }

  return CreateFromExisting(global, result, aPropagateUserInteraction);
}

void Promise::Then(JSContext* aCx,
                   // aCalleeGlobal may not be in the compartment of aCx, when
                   // called over Xrays.
                   JS::Handle<JSObject*> aCalleeGlobal,
                   AnyCallback* aResolveCallback, AnyCallback* aRejectCallback,
                   JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv) {
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

static void SettlePromise(Promise* aSettlingPromise, Promise* aCallbackPromise,
                          ErrorResult& aRv) {
  if (!aSettlingPromise) {
    return;
  }
  if (aRv.Failed()) {
    aSettlingPromise->MaybeReject(std::move(aRv));
    return;
  }
  if (aCallbackPromise) {
    aSettlingPromise->MaybeResolve(aCallbackPromise);
  } else {
    aSettlingPromise->MaybeResolveWithUndefined();
  }
}

void PromiseNativeThenHandlerBase::ResolvedCallback(
    JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
  if (!HasResolvedCallback()) {
    mPromise->MaybeResolve(aValue);
    return;
  }
  RefPtr<Promise> promise = CallResolveCallback(aCx, aValue, aRv);
  SettlePromise(mPromise, promise, aRv);
}

void PromiseNativeThenHandlerBase::RejectedCallback(
    JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
  if (!HasRejectedCallback()) {
    mPromise->MaybeReject(aValue);
    return;
  }
  RefPtr<Promise> promise = CallRejectCallback(aCx, aValue, aRv);
  SettlePromise(mPromise, promise, aRv);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(PromiseNativeThenHandlerBase)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(PromiseNativeThenHandlerBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromise)
  tmp->Traverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(PromiseNativeThenHandlerBase)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromise)
  tmp->Unlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PromiseNativeThenHandlerBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(PromiseNativeThenHandlerBase)
  tmp->Trace(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PromiseNativeThenHandlerBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PromiseNativeThenHandlerBase)

Result<RefPtr<Promise>, nsresult> Promise::ThenWithoutCycleCollection(
    const std::function<already_AddRefed<Promise>(
        JSContext* aCx, JS::HandleValue aValue, ErrorResult& aRv)>& aCallback) {
  return ThenWithCycleCollectedArgs(aCallback);
}

void Promise::CreateWrapper(
    ErrorResult& aRv, PropagateUserInteraction aPropagateUserInteraction) {
  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobal)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  JSContext* cx = jsapi.cx();
  mPromiseObj = JS::NewPromiseObject(cx, nullptr);
  if (!mPromiseObj) {
    JS_ClearPendingException(cx);
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  if (aPropagateUserInteraction == ePropagateUserInteraction) {
    Unused << MaybePropagateUserInputEventHandling();
  }
}

void Promise::MaybeResolve(JSContext* aCx, JS::Handle<JS::Value> aValue) {
  NS_ASSERT_OWNINGTHREAD(Promise);

  JS::Rooted<JSObject*> p(aCx, PromiseObj());
  if (!JS::ResolvePromise(aCx, p, aValue)) {
    // Now what?  There's nothing sane to do here.
    JS_ClearPendingException(aCx);
  }
}

void Promise::MaybeReject(JSContext* aCx, JS::Handle<JS::Value> aValue) {
  NS_ASSERT_OWNINGTHREAD(Promise);

  JS::Rooted<JSObject*> p(aCx, PromiseObj());
  if (!JS::RejectPromise(aCx, p, aValue)) {
    // Now what?  There's nothing sane to do here.
    JS_ClearPendingException(aCx);
  }
}

#define SLOT_NATIVEHANDLER 0
#define SLOT_NATIVEHANDLER_TASK 1

enum class NativeHandlerTask : int32_t { Resolve, Reject };

MOZ_CAN_RUN_SCRIPT
static bool NativeHandlerCallback(JSContext* aCx, unsigned aArgc,
                                  JS::Value* aVp) {
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);

  JS::Value v =
      js::GetFunctionNativeReserved(&args.callee(), SLOT_NATIVEHANDLER);
  MOZ_ASSERT(v.isObject());

  JS::Rooted<JSObject*> obj(aCx, &v.toObject());
  PromiseNativeHandler* handler = nullptr;
  if (NS_FAILED(UNWRAP_OBJECT(PromiseNativeHandler, &obj, handler))) {
    return Throw(aCx, NS_ERROR_UNEXPECTED);
  }

  v = js::GetFunctionNativeReserved(&args.callee(), SLOT_NATIVEHANDLER_TASK);
  NativeHandlerTask task = static_cast<NativeHandlerTask>(v.toInt32());

  ErrorResult rv;
  if (task == NativeHandlerTask::Resolve) {
    // handler is kept alive by "obj" on the stack.
    MOZ_KnownLive(handler)->ResolvedCallback(aCx, args.get(0), rv);
  } else {
    MOZ_ASSERT(task == NativeHandlerTask::Reject);
    // handler is kept alive by "obj" on the stack.
    MOZ_KnownLive(handler)->RejectedCallback(aCx, args.get(0), rv);
  }

  return !rv.MaybeSetPendingException(aCx);
}

static JSObject* CreateNativeHandlerFunction(JSContext* aCx,
                                             JS::Handle<JSObject*> aHolder,
                                             NativeHandlerTask aTask) {
  JSFunction* func = js::NewFunctionWithReserved(aCx, NativeHandlerCallback,
                                                 /* nargs = */ 1,
                                                 /* flags = */ 0, nullptr);
  if (!func) {
    return nullptr;
  }

  JS::Rooted<JSObject*> obj(aCx, JS_GetFunctionObject(func));

  JS::AssertObjectIsNotGray(aHolder);
  js::SetFunctionNativeReserved(obj, SLOT_NATIVEHANDLER,
                                JS::ObjectValue(*aHolder));
  js::SetFunctionNativeReserved(obj, SLOT_NATIVEHANDLER_TASK,
                                JS::Int32Value(static_cast<int32_t>(aTask)));

  return obj;
}

namespace {

class PromiseNativeHandlerShim final : public PromiseNativeHandler {
  RefPtr<PromiseNativeHandler> mInner;
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  enum InnerState{
      NotCleared,
      ClearedFromResolve,
      ClearedFromReject,
      ClearedFromCC,
  };
  InnerState mState = NotCleared;
#endif

  ~PromiseNativeHandlerShim() = default;

 public:
  explicit PromiseNativeHandlerShim(PromiseNativeHandler* aInner)
      : mInner(aInner) {
    MOZ_DIAGNOSTIC_ASSERT(mInner);
  }

  MOZ_CAN_RUN_SCRIPT
  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    MOZ_DIAGNOSTIC_ASSERT(mState != ClearedFromResolve);
    MOZ_DIAGNOSTIC_ASSERT(mState != ClearedFromReject);
    MOZ_DIAGNOSTIC_ASSERT(mState != ClearedFromCC);
#else
    if (!mInner) {
      return;
    }
#endif
    RefPtr<PromiseNativeHandler> inner = std::move(mInner);
    inner->ResolvedCallback(aCx, aValue, aRv);
    MOZ_ASSERT(!mInner);
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mState = ClearedFromResolve;
#endif
  }

  MOZ_CAN_RUN_SCRIPT
  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    MOZ_DIAGNOSTIC_ASSERT(mState != ClearedFromResolve);
    MOZ_DIAGNOSTIC_ASSERT(mState != ClearedFromReject);
    MOZ_DIAGNOSTIC_ASSERT(mState != ClearedFromCC);
#else
    if (!mInner) {
      return;
    }
#endif
    RefPtr<PromiseNativeHandler> inner = std::move(mInner);
    inner->RejectedCallback(aCx, aValue, aRv);
    MOZ_ASSERT(!mInner);
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mState = ClearedFromReject;
#endif
  }

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aWrapper) {
    return PromiseNativeHandler_Binding::Wrap(aCx, this, aGivenProto, aWrapper);
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(PromiseNativeHandlerShim)
};

NS_IMPL_CYCLE_COLLECTION_CLASS(PromiseNativeHandlerShim)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(PromiseNativeHandlerShim)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInner)
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  tmp->mState = ClearedFromCC;
#endif
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(PromiseNativeHandlerShim)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInner)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PromiseNativeHandlerShim)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PromiseNativeHandlerShim)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PromiseNativeHandlerShim)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

}  // anonymous namespace

void Promise::AppendNativeHandler(PromiseNativeHandler* aRunnable) {
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
  resolveFunc = CreateNativeHandlerFunction(cx, handlerWrapper,
                                            NativeHandlerTask::Resolve);
  if (NS_WARN_IF(!resolveFunc)) {
    jsapi.ClearException();
    return;
  }

  JS::Rooted<JSObject*> rejectFunc(cx);
  rejectFunc = CreateNativeHandlerFunction(cx, handlerWrapper,
                                           NativeHandlerTask::Reject);
  if (NS_WARN_IF(!rejectFunc)) {
    jsapi.ClearException();
    return;
  }

  JS::Rooted<JSObject*> promiseObj(cx, PromiseObj());
  if (NS_WARN_IF(
          !JS::AddPromiseReactions(cx, promiseObj, resolveFunc, rejectFunc))) {
    jsapi.ClearException();
    return;
  }
}

void Promise::HandleException(JSContext* aCx) {
  JS::Rooted<JS::Value> exn(aCx);
  if (JS_GetPendingException(aCx, &exn)) {
    JS_ClearPendingException(aCx);
    // Always reject even if this was called in *Resolve.
    MaybeReject(aCx, exn);
  }
}

// static
already_AddRefed<Promise> Promise::CreateFromExisting(
    nsIGlobalObject* aGlobal, JS::Handle<JSObject*> aPromiseObj,
    PropagateUserInteraction aPropagateUserInteraction) {
  MOZ_ASSERT(JS::GetCompartment(aGlobal->GetGlobalJSObjectPreserveColor()) ==
             JS::GetCompartment(aPromiseObj));
  RefPtr<Promise> p = new Promise(aGlobal);
  p->mPromiseObj = aPromiseObj;
  if (aPropagateUserInteraction == ePropagateUserInteraction &&
      !p->MaybePropagateUserInputEventHandling()) {
    return nullptr;
  }
  return p.forget();
}

void Promise::MaybeResolveWithUndefined() {
  NS_ASSERT_OWNINGTHREAD(Promise);

  MaybeResolve(JS::UndefinedHandleValue);
}

void Promise::MaybeReject(const RefPtr<MediaStreamError>& aArg) {
  NS_ASSERT_OWNINGTHREAD(Promise);

  MaybeSomething(aArg, &Promise::MaybeReject);
}

void Promise::MaybeRejectWithUndefined() {
  NS_ASSERT_OWNINGTHREAD(Promise);

  MaybeSomething(JS::UndefinedHandleValue, &Promise::MaybeReject);
}

void Promise::ReportRejectedPromise(JSContext* aCx, JS::HandleObject aPromise) {
  MOZ_ASSERT(!js::IsWrapper(aPromise));

  MOZ_ASSERT(JS::GetPromiseState(aPromise) == JS::PromiseState::Rejected);

  bool isChrome = false;
  uint64_t innerWindowID = 0;
  nsGlobalWindowInner* winForDispatch = nullptr;
  if (MOZ_LIKELY(NS_IsMainThread())) {
    isChrome = nsContentUtils::ObjectPrincipal(aPromise)->IsSystemPrincipal();

    if (nsGlobalWindowInner* win = xpc::WindowGlobalOrNull(aPromise)) {
      winForDispatch = win;
      innerWindowID = win->WindowID();
    } else if (nsGlobalWindowInner* win = xpc::SandboxWindowOrNull(
                   JS::GetNonCCWObjectGlobal(aPromise), aCx)) {
      // Don't dispatch rejections from the sandbox to the associated DOM
      // window.
      innerWindowID = win->WindowID();
    }
  } else if (const WorkerPrivate* wp = GetCurrentThreadWorkerPrivate()) {
    isChrome = wp->UsesSystemPrincipal();
    innerWindowID = wp->WindowID();
  } else if (nsCOMPtr<nsIGlobalObject> global = xpc::NativeGlobal(aPromise)) {
    if (nsCOMPtr<WorkletGlobalScope> workletGlobal =
            do_QueryInterface(global)) {
      WorkletImpl* impl = workletGlobal->Impl();
      isChrome = impl->PrincipalInfo().type() ==
                 mozilla::ipc::PrincipalInfo::TSystemPrincipalInfo;
      innerWindowID = impl->LoadInfo().InnerWindowID();
    }
  }

  JS::Rooted<JS::Value> result(aCx, JS::GetPromiseResult(aPromise));
  // resolutionSite can be null if async stacks are disabled.
  JS::Rooted<JSObject*> resolutionSite(aCx,
                                       JS::GetPromiseResolutionSite(aPromise));

  // We're inspecting the rejection value only to report it to the console, and
  // we do so without side-effects, so we can safely unwrap it without regard to
  // the privileges of the Promise object that holds it. If we don't unwrap
  // before trying to create the error report, we wind up reporting any
  // cross-origin objects as "uncaught exception: Object".
  RefPtr<xpc::ErrorReport> xpcReport = new xpc::ErrorReport();
  {
    Maybe<JSAutoRealm> ar;
    JS::Rooted<JS::Value> unwrapped(aCx, result);
    if (unwrapped.isObject()) {
      unwrapped.setObject(*js::UncheckedUnwrap(&unwrapped.toObject()));
      ar.emplace(aCx, &unwrapped.toObject());
    }

    JS::ErrorReportBuilder report(aCx);
    RefPtr<Exception> exn;
    if (unwrapped.isObject() &&
        (NS_SUCCEEDED(UNWRAP_OBJECT(DOMException, &unwrapped, exn)) ||
         NS_SUCCEEDED(UNWRAP_OBJECT(Exception, &unwrapped, exn)))) {
      xpcReport->Init(aCx, exn, isChrome, innerWindowID);
    } else {
      // Use the resolution site as the exception stack
      JS::ExceptionStack exnStack(aCx, unwrapped, resolutionSite);
      if (!report.init(aCx, exnStack, JS::ErrorReportBuilder::NoSideEffects)) {
        JS_ClearPendingException(aCx);
        return;
      }

      xpcReport->Init(report.report(), report.toStringResult().c_str(),
                      isChrome, innerWindowID);
    }
  }

  // Used to initialize the similarly named nsISciptError attribute.
  xpcReport->mIsPromiseRejection = true;

  // Now post an event to do the real reporting async
  RefPtr<AsyncErrorReporter> event = new AsyncErrorReporter(xpcReport);
  if (winForDispatch) {
    if (!winForDispatch->IsDying()) {
      // Exceptions from a dying window will cause the window to leak.
      event->SetException(aCx, result);
      if (resolutionSite) {
        event->SerializeStack(aCx, resolutionSite);
      }
    }
    winForDispatch->Dispatch(mozilla::TaskCategory::Other, event.forget());
  } else {
    NS_DispatchToMainThread(event);
  }
}

void Promise::MaybeResolveWithClone(JSContext* aCx,
                                    JS::Handle<JS::Value> aValue) {
  JS::Rooted<JSObject*> sourceScope(aCx, JS::CurrentGlobalOrNull(aCx));
  AutoEntryScript aes(GetParentObject(), "Promise resolution");
  JSContext* cx = aes.cx();
  JS::Rooted<JS::Value> value(cx, aValue);

  xpc::StackScopedCloneOptions options;
  options.wrapReflectors = true;
  if (!StackScopedClone(cx, options, sourceScope, &value)) {
    HandleException(cx);
    return;
  }
  MaybeResolve(aCx, value);
}

void Promise::MaybeRejectWithClone(JSContext* aCx,
                                   JS::Handle<JS::Value> aValue) {
  JS::Rooted<JSObject*> sourceScope(aCx, JS::CurrentGlobalOrNull(aCx));
  AutoEntryScript aes(GetParentObject(), "Promise rejection");
  JSContext* cx = aes.cx();
  JS::Rooted<JS::Value> value(cx, aValue);

  xpc::StackScopedCloneOptions options;
  options.wrapReflectors = true;
  if (!StackScopedClone(cx, options, sourceScope, &value)) {
    HandleException(cx);
    return;
  }
  MaybeReject(aCx, value);
}

// A WorkerRunnable to resolve/reject the Promise on the worker thread.
// Calling thread MUST hold PromiseWorkerProxy's mutex before creating this.
class PromiseWorkerProxyRunnable : public WorkerRunnable {
 public:
  PromiseWorkerProxyRunnable(PromiseWorkerProxy* aPromiseWorkerProxy,
                             PromiseWorkerProxy::RunCallbackFunc aFunc)
      : WorkerRunnable(aPromiseWorkerProxy->GetWorkerPrivate(),
                       WorkerThreadUnchangedBusyCount),
        mPromiseWorkerProxy(aPromiseWorkerProxy),
        mFunc(aFunc) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mPromiseWorkerProxy);
  }

  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
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
  ~PromiseWorkerProxyRunnable() = default;

 private:
  RefPtr<PromiseWorkerProxy> mPromiseWorkerProxy;

  // Function pointer for calling Promise::{ResolveInternal,RejectInternal}.
  PromiseWorkerProxy::RunCallbackFunc mFunc;
};

/* static */
already_AddRefed<PromiseWorkerProxy> PromiseWorkerProxy::Create(
    WorkerPrivate* aWorkerPrivate, Promise* aWorkerPromise,
    const PromiseWorkerProxyStructuredCloneCallbacks* aCb) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(aWorkerPromise);
  MOZ_ASSERT_IF(aCb, !!aCb->Write && !!aCb->Read);

  RefPtr<PromiseWorkerProxy> proxy =
      new PromiseWorkerProxy(aWorkerPromise, aCb);

  // Maintain a reference so that we have a valid object to clean up when
  // removing the feature.
  proxy.get()->AddRef();

  // We do this to make sure the worker thread won't shut down before the
  // promise is resolved/rejected on the worker thread.
  RefPtr<StrongWorkerRef> workerRef = StrongWorkerRef::Create(
      aWorkerPrivate, "PromiseWorkerProxy", [proxy]() { proxy->CleanUp(); });

  if (NS_WARN_IF(!workerRef)) {
    // Probably the worker is terminating. We cannot complete the operation
    // and we have to release all the resources.  CleanUp releases the extra
    // ref, too
    proxy->CleanUp();
    return nullptr;
  }

  proxy->mWorkerRef = new ThreadSafeWorkerRef(workerRef);

  return proxy.forget();
}

NS_IMPL_ISUPPORTS0(PromiseWorkerProxy)

PromiseWorkerProxy::PromiseWorkerProxy(
    Promise* aWorkerPromise,
    const PromiseWorkerProxyStructuredCloneCallbacks* aCallbacks)
    : mWorkerPromise(aWorkerPromise),
      mCleanedUp(false),
      mCallbacks(aCallbacks),
      mCleanUpLock("cleanUpLock") {}

PromiseWorkerProxy::~PromiseWorkerProxy() {
  MOZ_ASSERT(mCleanedUp);
  MOZ_ASSERT(!mWorkerPromise);
  MOZ_ASSERT(!mWorkerRef);
}

WorkerPrivate* PromiseWorkerProxy::GetWorkerPrivate() const {
#ifdef DEBUG
  if (NS_IsMainThread()) {
    mCleanUpLock.AssertCurrentThreadOwns();
  }
#endif
  // Safe to check this without a lock since we assert lock ownership on the
  // main thread above.
  MOZ_ASSERT(!mCleanedUp);
  MOZ_ASSERT(mWorkerRef);

  return mWorkerRef->Private();
}

bool PromiseWorkerProxy::OnWritingThread() const {
  return IsCurrentThreadRunningWorker();
}

Promise* PromiseWorkerProxy::WorkerPromise() const {
  MOZ_ASSERT(IsCurrentThreadRunningWorker());
  MOZ_ASSERT(mWorkerPromise);
  return mWorkerPromise;
}

void PromiseWorkerProxy::RunCallback(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue,
                                     RunCallbackFunc aFunc) {
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(Lock());
  // If the worker thread's been cancelled we don't need to resolve the Promise.
  if (CleanedUp()) {
    return;
  }

  // The |aValue| is written into the StructuredCloneHolderBase.
  if (!Write(aCx, aValue)) {
    JS_ClearPendingException(aCx);
    MOZ_ASSERT(false,
               "cannot serialize the value with the StructuredCloneAlgorithm!");
  }

  RefPtr<PromiseWorkerProxyRunnable> runnable =
      new PromiseWorkerProxyRunnable(this, aFunc);

  runnable->Dispatch();
}

void PromiseWorkerProxy::ResolvedCallback(JSContext* aCx,
                                          JS::Handle<JS::Value> aValue,
                                          ErrorResult& aRv) {
  RunCallback(aCx, aValue, &Promise::MaybeResolve);
}

void PromiseWorkerProxy::RejectedCallback(JSContext* aCx,
                                          JS::Handle<JS::Value> aValue,
                                          ErrorResult& aRv) {
  RunCallback(aCx, aValue, &Promise::MaybeReject);
}

void PromiseWorkerProxy::CleanUp() {
  // Can't release Mutex while it is still locked, so scope the lock.
  {
    MutexAutoLock lock(Lock());

    if (CleanedUp()) {
      return;
    }

    // We can be called if we failed to get a WorkerRef
    if (mWorkerRef) {
      mWorkerRef->Private()->AssertIsOnWorkerThread();
    }

    // Release the Promise and remove the PromiseWorkerProxy from the holders of
    // the worker thread since the Promise has been resolved/rejected or the
    // worker thread has been cancelled.
    mCleanedUp = true;
    mWorkerPromise = nullptr;
    mWorkerRef = nullptr;

    // Clear the StructuredCloneHolderBase class.
    Clear();
  }
  Release();
}

JSObject* PromiseWorkerProxy::CustomReadHandler(
    JSContext* aCx, JSStructuredCloneReader* aReader,
    const JS::CloneDataPolicy& aCloneDataPolicy, uint32_t aTag,
    uint32_t aIndex) {
  if (NS_WARN_IF(!mCallbacks)) {
    return nullptr;
  }

  return mCallbacks->Read(aCx, aReader, this, aTag, aIndex);
}

bool PromiseWorkerProxy::CustomWriteHandler(JSContext* aCx,
                                            JSStructuredCloneWriter* aWriter,
                                            JS::Handle<JSObject*> aObj,
                                            bool* aSameProcessScopeRequired) {
  if (NS_WARN_IF(!mCallbacks)) {
    return false;
  }

  return mCallbacks->Write(aCx, aWriter, this, aObj);
}

// Specializations of MaybeRejectBrokenly we actually support.
template <>
void Promise::MaybeRejectBrokenly(const RefPtr<DOMException>& aArg) {
  MaybeSomething(aArg, &Promise::MaybeReject);
}
template <>
void Promise::MaybeRejectBrokenly(const nsAString& aArg) {
  MaybeSomething(aArg, &Promise::MaybeReject);
}

Promise::PromiseState Promise::State() const {
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

bool Promise::SetSettledPromiseIsHandled() {
  AutoAllowLegacyScriptExecution exemption;
  AutoEntryScript aes(mGlobal, "Set settled promise handled");
  JSContext* cx = aes.cx();
  JS::RootedObject promiseObj(cx, mPromiseObj);
  return JS::SetSettledPromiseIsHandled(cx, promiseObj);
}

bool Promise::SetAnyPromiseIsHandled() {
  AutoAllowLegacyScriptExecution exemption;
  AutoEntryScript aes(mGlobal, "Set any promise handled");
  JSContext* cx = aes.cx();
  JS::RootedObject promiseObj(cx, mPromiseObj);
  return JS::SetAnyPromiseIsHandled(cx, promiseObj);
}

/* static */
already_AddRefed<Promise> Promise::CreateResolvedWithUndefined(
    nsIGlobalObject* global, ErrorResult& aRv) {
  RefPtr<Promise> returnPromise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  returnPromise->MaybeResolveWithUndefined();
  return returnPromise.forget();
}

}  // namespace mozilla::dom

extern "C" {

// These functions are used in the implementation of ffi bindings for
// dom::Promise from Rust.

void DomPromise_AddRef(mozilla::dom::Promise* aPromise) {
  MOZ_ASSERT(aPromise);
  aPromise->AddRef();
}

void DomPromise_Release(mozilla::dom::Promise* aPromise) {
  MOZ_ASSERT(aPromise);
  aPromise->Release();
}

#define DOM_PROMISE_FUNC_WITH_VARIANT(name, func)                         \
  void name(mozilla::dom::Promise* aPromise, nsIVariant* aVariant) {      \
    MOZ_ASSERT(aPromise);                                                 \
    MOZ_ASSERT(aVariant);                                                 \
    mozilla::dom::AutoEntryScript aes(aPromise->GetGlobalObject(),        \
                                      "Promise resolution or rejection"); \
    JSContext* cx = aes.cx();                                             \
                                                                          \
    JS::Rooted<JS::Value> val(cx);                                        \
    nsresult rv = NS_OK;                                                  \
    if (!XPCVariant::VariantDataToJS(cx, aVariant, &rv, &val)) {          \
      aPromise->MaybeRejectWithTypeError(                                 \
          "Failed to convert nsIVariant to JS");                          \
      return;                                                             \
    }                                                                     \
    aPromise->func(val);                                                  \
  }

DOM_PROMISE_FUNC_WITH_VARIANT(DomPromise_RejectWithVariant, MaybeReject)
DOM_PROMISE_FUNC_WITH_VARIANT(DomPromise_ResolveWithVariant, MaybeResolve)

#undef DOM_PROMISE_FUNC_WITH_VARIANT
}
