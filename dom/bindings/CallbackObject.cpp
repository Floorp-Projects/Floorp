/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CallbackObject.h"
#include "mozilla/dom/BindingUtils.h"
#include "jsfriendapi.h"
#include "nsIScriptGlobalObject.h"
#include "nsIXPConnect.h"
#include "nsIScriptContext.h"
#include "nsPIDOMWindow.h"
#include "nsJSUtils.h"
#include "xpcprivate.h"
#include "WorkerPrivate.h"
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
  tmp->DropJSObjects();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIncumbentGlobal)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
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

CallbackObject::CallSetup::CallSetup(CallbackObject* aCallback,
                                     ErrorResult& aRv,
                                     const char* aExecutionReason,
                                     ExceptionHandling aExceptionHandling,
                                     JSCompartment* aCompartment,
                                     bool aIsJSImplementedWebIDL)
  : mCx(nullptr)
  , mCompartment(aCompartment)
  , mErrorResult(aRv)
  , mExceptionHandling(aExceptionHandling)
  , mIsMainThread(NS_IsMainThread())
{
  if (mIsMainThread) {
    nsContentUtils::EnterMicroTask();
  }

  // Compute the caller's subject principal (if necessary) early, before we
  // do anything that might perturb the relevant state.
  nsIPrincipal* webIDLCallerPrincipal = nullptr;
  if (aIsJSImplementedWebIDL) {
    webIDLCallerPrincipal = nsContentUtils::SubjectPrincipalOrSystemIfNativeCaller();
  }

  // First, find the real underlying callback.
  JSObject* realCallback = js::UncheckedUnwrap(aCallback->CallbackPreserveColor());
  nsIGlobalObject* globalObject = nullptr;

  JSContext* cx;
  {
    // Bug 955660: we cannot do "proper" rooting here because we need the
    // global to get a context. Everything here is simple getters that cannot
    // GC, so just paper over the necessary dataflow inversion.
    JS::AutoSuppressGCAnalysis nogc;

    // Now get the global for this callback. Note that for the case of
    // JS-implemented WebIDL we never have a window here.
    nsGlobalWindow* win = mIsMainThread && !aIsJSImplementedWebIDL
                        ? xpc::WindowGlobalOrNull(realCallback)
                        : nullptr;
    if (win) {
      MOZ_ASSERT(win->IsInnerWindow());
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
      JSObject* global = js::GetGlobalForObjectCrossCompartment(realCallback);
      globalObject = xpc::NativeGlobal(global);
      MOZ_ASSERT(globalObject);
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

    cx = mAutoEntryScript->cx();

    // Unmark the callable (by invoking Callback() and not the CallbackPreserveColor()
    // variant), and stick it in a Rooted before it can go gray again.
    // Nothing before us in this function can trigger a CC, so it's safe to wait
    // until here it do the unmark. This allows us to construct mRootedCallable
    // with the cx from mAutoEntryScript, avoiding the cost of finding another
    // JSContext. (Rooted<> does not care about requests or compartments.)
    mRootedCallable.emplace(cx, aCallback->Callback());
  }

  // JS-implemented WebIDL is always OK to run, since it runs with Chrome
  // privileges anyway.
  if (mIsMainThread && !aIsJSImplementedWebIDL) {
    // Check that it's ok to run this callback at all.
    // Make sure to use realCallback to get the global of the callback object,
    // not the wrapper.
    bool allowed = xpc::Scriptability::Get(realCallback).Allowed();

    if (!allowed) {
      aRv.ThrowDOMException(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
        NS_LITERAL_CSTRING("Refusing to execute function from global in which "
                           "script is disabled."));
      return;
    }
  }

  mAsyncStack.emplace(cx, aCallback->GetCreationStack());
  if (*mAsyncStack) {
    mAsyncStackSetter.emplace(cx, *mAsyncStack, aExecutionReason);
  }

  // Enter the compartment of our callback, so we can actually work with it.
  //
  // Note that if the callback is a wrapper, this will not be the same
  // compartment that we ended up in with mAutoEntryScript above, because the
  // entry point is based off of the unwrapped callback (realCallback).
  mAc.emplace(cx, *mRootedCallable);

  // And now we're ready to go.
  mCx = cx;
}

bool
CallbackObject::CallSetup::ShouldRethrowException(JS::Handle<JS::Value> aException)
{
  if (mExceptionHandling == eRethrowExceptions) {
    if (!mCompartment) {
      // Caller didn't ask us to filter for only exceptions we subsume.
      return true;
    }

    // On workers, we don't have nsIPrincipals to work with.  But we also only
    // have one compartment, so check whether mCompartment is the same as the
    // current compartment of mCx.
    if (mCompartment == js::GetContextCompartment(mCx)) {
      return true;
    }

    MOZ_ASSERT(NS_IsMainThread());

    // At this point mCx is in the compartment of our unwrapped callback, so
    // just check whether the principal of mCompartment subsumes that of the
    // current compartment/global of mCx.
    nsIPrincipal* callerPrincipal =
      nsJSPrincipals::get(JS_GetCompartmentPrincipals(mCompartment));
    nsIPrincipal* calleePrincipal = nsContentUtils::SubjectPrincipal();
    if (callerPrincipal->SubsumesConsideringDomain(calleePrincipal)) {
      return true;
    }
  }

  MOZ_ASSERT(mCompartment);

  // Now we only want to throw an exception to the caller if the object that was
  // thrown is in the caller compartment (which we stored in mCompartment).

  if (!aException.isObject()) {
    return false;
  }

  JS::Rooted<JSObject*> obj(mCx, &aException.toObject());
  obj = js::UncheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
  return js::GetObjectCompartment(obj) == mCompartment;
}

CallbackObject::CallSetup::~CallSetup()
{
  // To get our nesting right we have to destroy our JSAutoCompartment first.
  // In particular, we want to do this before we try reporting any exceptions,
  // so we end up reporting them while in the compartment of our entry point,
  // not whatever cross-compartment wrappper mCallback might be.
  // Be careful: the JSAutoCompartment might not have been constructed at all!
  mAc.reset();

  // Now, if we have a JSContext, report any pending errors on it, unless we
  // were told to re-throw them.
  if (mCx) {
    bool needToDealWithException = mAutoEntryScript->HasException();
    if ((mCompartment && mExceptionHandling == eRethrowContentExceptions) ||
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
      // runs.  Note that we've already run ~mAc, effectively, so we don't have
      // to worry about ordering here.
      if (mErrorResult.IsJSContextException()) {
        // XXXkhuey bug 1117269.  When this is fixed, please consider fixing
        // ThrowExceptionValueIfSafe over in Exceptions.cpp in the same way.

        // IsJSContextException shouldn't be true anymore because we will report
        // the exception on the JSContext ... so throw something else.
        mErrorResult.Throw(NS_ERROR_UNEXPECTED);
      }
    }
  }

  mAutoIncumbentScript.reset();
  mAutoEntryScript.reset();

  // It is important that this is the last thing we do, after leaving the
  // compartment and undoing all our entry/incumbent script changes
  if (mIsMainThread) {
    nsContentUtils::LeaveMicroTask();
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

  JS::Rooted<JSObject*> callback(cx, aCallback->Callback());

  JSAutoCompartment ac(cx, callback);
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
