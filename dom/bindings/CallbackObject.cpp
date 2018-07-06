/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CallbackObject.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/BindingUtils.h"
#include "jsfriendapi.h"
#include "nsIScriptGlobalObject.h"
#include "nsIXPConnect.h"
#include "nsIScriptContext.h"
#include "nsPIDOMWindow.h"
#include "nsJSUtils.h"
#include "xpcprivate.h"
#include "WorkerPrivate.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "WorkerScope.h"
#include "jsapi.h"
#include "nsJSPrincipals.h"

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CallbackObject)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::CallbackObject)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(CallbackObject)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CallbackObject)

NS_IMPL_CYCLE_COLLECTION_CLASS(CallbackObject)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CallbackObject)
  tmp->ClearJSReferences();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIncumbentGlobal)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(CallbackObject)
  JSObject* callback = tmp->CallbackPreserveColor();

  if (!aRemovingAllowed) {
    // If our callback has been cleared, we can't be part of a garbage cycle.
    return !callback;
  }

  // mCallback is always wrapped for the CallbackObject's incumbent global. In
  // the case where the real callback is in a different compartment, we have a
  // cross-compartment wrapper, and it will automatically be cut when its
  // compartment is nuked. In the case where it is in the same compartment, we
  // have a reference to the real function. Since that means there are no
  // wrappers to cut, we need to check whether the compartment is still alive,
  // and drop the references if it is not.

  if (MOZ_UNLIKELY(!callback)) {
    return true;
  }
  auto pvt = xpc::CompartmentPrivate::Get(callback);
  if (MOZ_LIKELY(tmp->mIncumbentGlobal && pvt) && MOZ_UNLIKELY(pvt->wasNuked)) {
    // It's not safe to release our global reference or drop our JS objects at
    // this point, so defer their finalization until CC is finished.
    AddForDeferredFinalization(new JSObjectsDropper(tmp));
    DeferredFinalize(tmp->mIncumbentGlobal.forget().take());
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(CallbackObject)
  return !tmp->mCallback;
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(CallbackObject)
  return !tmp->mCallback;
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(CallbackObject)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIncumbentGlobal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(CallbackObject)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mCallback)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mCreationStack)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mIncumbentJSGlobal)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

void
CallbackObject::Trace(JSTracer* aTracer)
{
  JS::TraceEdge(aTracer, &mCallback, "CallbackObject.mCallback");
  JS::TraceEdge(aTracer, &mCreationStack, "CallbackObject.mCreationStack");
  JS::TraceEdge(aTracer, &mIncumbentJSGlobal,
                "CallbackObject.mIncumbentJSGlobal");
}

void
CallbackObject::FinishSlowJSInitIfMoreThanOneOwner(JSContext* aCx)
{
  MOZ_ASSERT(mRefCnt.get() > 0);
  if (mRefCnt.get() > 1) {
    mozilla::HoldJSObjects(this);
    if (JS::ContextOptionsRef(aCx).asyncStack()) {
      JS::RootedObject stack(aCx);
      if (!JS::CaptureCurrentStack(aCx, &stack)) {
        JS_ClearPendingException(aCx);
      }
      mCreationStack = stack;
    }
    mIncumbentGlobal = GetIncumbentGlobal();
    if (mIncumbentGlobal) {
      mIncumbentJSGlobal = mIncumbentGlobal->GetGlobalJSObject();
    }
  } else {
    // We can just forget all our stuff.
    ClearJSReferences();
  }
}

JSObject*
CallbackObject::Callback(JSContext* aCx)
{
  JSObject* callback = CallbackOrNull();
  if (!callback) {
    callback = JS_NewDeadWrapper(aCx);
  }

  MOZ_DIAGNOSTIC_ASSERT(callback);
  return callback;
}

CallbackObject::CallSetup::CallSetup(CallbackObject* aCallback,
                                     ErrorResult& aRv,
                                     const char* aExecutionReason,
                                     ExceptionHandling aExceptionHandling,
                                     JS::Realm* aRealm,
                                     bool aIsJSImplementedWebIDL)
  : mCx(nullptr)
  , mRealm(aRealm)
  , mErrorResult(aRv)
  , mExceptionHandling(aExceptionHandling)
  , mIsMainThread(NS_IsMainThread())
{
  CycleCollectedJSContext* ccjs = CycleCollectedJSContext::Get();
  if (ccjs) {
    ccjs->EnterMicroTask();
  }

  // Compute the caller's subject principal (if necessary) early, before we
  // do anything that might perturb the relevant state.
  nsIPrincipal* webIDLCallerPrincipal = nullptr;
  if (aIsJSImplementedWebIDL) {
    webIDLCallerPrincipal = nsContentUtils::SubjectPrincipalOrSystemIfNativeCaller();
  }

  JSObject* wrappedCallback = aCallback->CallbackPreserveColor();
  if (!wrappedCallback) {
    aRv.ThrowDOMException(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
      NS_LITERAL_CSTRING("Cannot execute callback from a nuked compartment."));
    return;
  }

  nsIGlobalObject* globalObject = nullptr;

  {
    // First, find the real underlying callback.
    JSObject* realCallback = js::UncheckedUnwrap(wrappedCallback);

    // Check that it's ok to run this callback. JS-implemented WebIDL is always
    // OK to run, since it runs with Chrome privileges anyway.
    if (mIsMainThread && !aIsJSImplementedWebIDL) {
      // Make sure to use realCallback to get the global of the callback
      // object, not the wrapper.
      if (!xpc::Scriptability::Get(realCallback).Allowed()) {
        aRv.ThrowDOMException(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
          NS_LITERAL_CSTRING("Refusing to execute function from global in which "
                             "script is disabled."));
        return;
      }
    }

    // Now get the global for this callback. Note that for the case of
    // JS-implemented WebIDL we never have a window here.
    nsGlobalWindowInner* win = mIsMainThread && !aIsJSImplementedWebIDL
                            ? xpc::WindowGlobalOrNull(realCallback)
                            : nullptr;
    if (win) {
      // We don't want to run script in windows that have been navigated away
      // from.
      if (!win->AsInner()->HasActiveDocument()) {
        aRv.ThrowDOMException(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
          NS_LITERAL_CSTRING("Refusing to execute function from window "
                             "whose document is no longer active."));
        return;
      }
      globalObject = win;
    } else {
      // No DOM Window. Store the global.
      JSObject* global = JS::GetNonCCWObjectGlobal(realCallback);
      globalObject = xpc::NativeGlobal(global);
      MOZ_ASSERT(globalObject);
    }
  }

  // Bail out if there's no useful global. This seems to happen intermittently
  // on gaia-ui tests, probably because nsInProcessTabChildGlobal is returning
  // null in some kind of teardown state.
  if (!globalObject->GetGlobalJSObject()) {
    aRv.ThrowDOMException(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
      NS_LITERAL_CSTRING("Refusing to execute function from global which is "
                         "being torn down."));
    return;
  }

  mAutoEntryScript.emplace(globalObject, aExecutionReason, mIsMainThread);
  mAutoEntryScript->SetWebIDLCallerPrincipal(webIDLCallerPrincipal);
  nsIGlobalObject* incumbent = aCallback->IncumbentGlobalOrNull();
  if (incumbent) {
    // The callback object traces its incumbent JS global, so in general it
    // should be alive here. However, it's possible that we could run afoul
    // of the same IPC global weirdness described above, wherein the
    // nsIGlobalObject has severed its reference to the JS global. Let's just
    // be safe here, so that nobody has to waste a day debugging gaia-ui tests.
    if (!incumbent->GetGlobalJSObject()) {
      aRv.ThrowDOMException(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
        NS_LITERAL_CSTRING("Refusing to execute function because our "
                           "incumbent global is being torn down."));
      return;
    }
    mAutoIncumbentScript.emplace(incumbent);
  }

  JSContext* cx = mAutoEntryScript->cx();

  // Unmark the callable (by invoking CallbackOrNull() and not the
  // CallbackPreserveColor() variant), and stick it in a Rooted before it can
  // go gray again.
  // Nothing before us in this function can trigger a CC, so it's safe to wait
  // until here it do the unmark. This allows us to construct mRootedCallable
  // with the cx from mAutoEntryScript, avoiding the cost of finding another
  // JSContext. (Rooted<> does not care about requests or compartments.)
  mRootedCallable.emplace(cx, aCallback->CallbackOrNull());

  mAsyncStack.emplace(cx, aCallback->GetCreationStack());
  if (*mAsyncStack) {
    mAsyncStackSetter.emplace(cx, *mAsyncStack, aExecutionReason);
  }

  // Enter the realm of our callback, so we can actually work with it.
  //
  // Note that if the callback is a wrapper, this will not be the same
  // realm that we ended up in with mAutoEntryScript above, because the
  // entry point is based off of the unwrapped callback (realCallback).
  mAr.emplace(cx, *mRootedCallable);

  // And now we're ready to go.
  mCx = cx;
}

bool
CallbackObject::CallSetup::ShouldRethrowException(JS::Handle<JS::Value> aException)
{
  if (mExceptionHandling == eRethrowExceptions) {
    if (!mRealm) {
      // Caller didn't ask us to filter for only exceptions we subsume.
      return true;
    }

    // On workers, we don't have nsIPrincipals to work with.  But we also only
    // have one realm, so check whether mRealm is the same as the current realm
    // of mCx.
    if (mRealm == js::GetContextRealm(mCx)) {
      return true;
    }

    MOZ_ASSERT(NS_IsMainThread());

    // At this point mCx is in the realm of our unwrapped callback, so just
    // check whether the principal of mRealm subsumes that of the current
    // realm/global of mCx.
    nsIPrincipal* callerPrincipal =
      nsJSPrincipals::get(JS::GetRealmPrincipals(mRealm));
    nsIPrincipal* calleePrincipal = nsContentUtils::SubjectPrincipal();
    if (callerPrincipal->SubsumesConsideringDomain(calleePrincipal)) {
      return true;
    }
  }

  MOZ_ASSERT(mRealm);

  // Now we only want to throw an exception to the caller if the object that was
  // thrown is in the caller realm (which we stored in mRealm).

  if (!aException.isObject()) {
    return false;
  }

  JS::Rooted<JSObject*> obj(mCx, &aException.toObject());
  obj = js::UncheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
  return js::GetNonCCWObjectRealm(obj) == mRealm;
}

CallbackObject::CallSetup::~CallSetup()
{
  // To get our nesting right we have to destroy our JSAutoRealm first.
  // In particular, we want to do this before we try reporting any exceptions,
  // so we end up reporting them while in the realm of our entry point,
  // not whatever cross-compartment wrappper mCallback might be.
  // Be careful: the JSAutoRealm might not have been constructed at all!
  mAr.reset();

  // Now, if we have a JSContext, report any pending errors on it, unless we
  // were told to re-throw them.
  if (mCx) {
    bool needToDealWithException = mAutoEntryScript->HasException();
    if ((mRealm && mExceptionHandling == eRethrowContentExceptions) ||
        mExceptionHandling == eRethrowExceptions) {
      mErrorResult.MightThrowJSException();
      if (needToDealWithException) {
        JS::Rooted<JS::Value> exn(mCx);
        if (mAutoEntryScript->PeekException(&exn) &&
            ShouldRethrowException(exn)) {
          mAutoEntryScript->ClearException();
          MOZ_ASSERT(!mAutoEntryScript->HasException());
          mErrorResult.ThrowJSException(mCx, exn);
          needToDealWithException = false;
        }
      }
    }

    if (needToDealWithException) {
      // Either we're supposed to report our exceptions, or we're supposed to
      // re-throw them but we failed to get the exception value.  Either way,
      // we'll just report the pending exception, if any, once ~mAutoEntryScript
      // runs.  Note that we've already run ~mAr, effectively, so we don't have
      // to worry about ordering here.
      if (mErrorResult.IsJSContextException()) {
        // XXXkhuey bug 1117269.  When this is fixed, please consider fixing
        // ThrowExceptionValueIfSafe over in Exceptions.cpp in the same way.

        // IsJSContextException shouldn't be true anymore because we will report
        // the exception on the JSContext ... so throw something else.
        mErrorResult.ThrowWithCustomCleanup(NS_ERROR_UNEXPECTED);
      }
    }
  }

  mAutoIncumbentScript.reset();
  mAutoEntryScript.reset();

  // It is important that this is the last thing we do, after leaving the
  // realm and undoing all our entry/incumbent script changes
  CycleCollectedJSContext* ccjs = CycleCollectedJSContext::Get();
  if (ccjs) {
    ccjs->LeaveMicroTask();
  }
}

already_AddRefed<nsISupports>
CallbackObjectHolderBase::ToXPCOMCallback(CallbackObject* aCallback,
                                          const nsIID& aIID) const
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aCallback) {
    return nullptr;
  }

  // We don't init the AutoJSAPI with our callback because we don't want it
  // reporting errors to its global's onerror handlers.
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();

  JS::Rooted<JSObject*> callback(cx, aCallback->CallbackOrNull());
  if (!callback) {
    return nullptr;
  }

  JSAutoRealm ar(cx, callback);
  RefPtr<nsXPCWrappedJS> wrappedJS;
  nsresult rv =
    nsXPCWrappedJS::GetNewOrUsed(callback, aIID, getter_AddRefs(wrappedJS));
  if (NS_FAILED(rv) || !wrappedJS) {
    return nullptr;
  }

  nsCOMPtr<nsISupports> retval;
  rv = wrappedJS->QueryInterface(aIID, getter_AddRefs(retval));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return retval.forget();
}

} // namespace dom
} // namespace mozilla
