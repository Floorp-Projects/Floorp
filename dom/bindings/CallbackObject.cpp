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
#include "nsIScriptSecurityManager.h"
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIncumbentGlobal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(CallbackObject)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mCallback)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mIncumbentJSGlobal)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

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
    webIDLCallerPrincipal = nsContentUtils::SubjectPrincipal();
  }

  // We need to produce a useful JSContext here.  Ideally one that the callback
  // is in some sense associated with, so that we can sort of treat it as a
  // "script entry point".  Though once we actually have script entry points,
  // we'll need to do the script entry point bits once we have an actual
  // callable.

  // First, find the real underlying callback.
  JSObject* realCallback = js::UncheckedUnwrap(aCallback->CallbackPreserveColor());
  JSContext* cx = nullptr;
  nsIGlobalObject* globalObject = nullptr;

  {
    // Bug 955660: we cannot do "proper" rooting here because we need the
    // global to get a context. Everything here is simple getters that cannot
    // GC, so just paper over the necessary dataflow inversion.
    JS::AutoSuppressGCAnalysis nogc;
    if (mIsMainThread) {
      // Now get the global and JSContext for this callback.  Note that for the
      // case of JS-implemented WebIDL we never have a window here.
      nsGlobalWindow* win =
        aIsJSImplementedWebIDL ? nullptr : xpc::WindowGlobalOrNull(realCallback);
      if (win) {
        // Make sure that if this is a window it has an active document, since
        // the nsIScriptContext and hence JSContext are associated with the
        // outer window.  Which means that if someone holds on to a function
        // from a now-unloaded document we'd have the new document as the
        // script entry point...
        MOZ_ASSERT(win->IsInnerWindow());
        if (!win->HasActiveDocument()) {
          // Just bail out from here
          return;
        }
        cx = win->GetContext() ? win->GetContext()->GetNativeContext()
                               // This happens - Removing it causes
                               // test_bug293235.xul to go orange.
                               : nsContentUtils::GetSafeJSContext();
        globalObject = win;
      } else {
        // No DOM Window. Store the global and use the SafeJSContext.
        JSObject* glob = js::GetGlobalForObjectCrossCompartment(realCallback);
        globalObject = xpc::NativeGlobal(glob);
        MOZ_ASSERT(globalObject);
        cx = nsContentUtils::GetSafeJSContext();
      }
    } else {
      cx = workers::GetCurrentThreadJSContext();
      JSObject *global = js::GetGlobalForObjectCrossCompartment(realCallback);
      globalObject = workers::GetGlobalObjectForGlobal(global);
      MOZ_ASSERT(globalObject);
    }

    // Bail out if there's no useful global. This seems to happen intermittently
    // on gaia-ui tests, probably because nsInProcessTabChildGlobal is returning
    // null in some kind of teardown state.
    if (!globalObject->GetGlobalJSObject()) {
      return;
    }

    mAutoEntryScript.emplace(globalObject, aExecutionReason,
                             mIsMainThread, cx);
    mAutoEntryScript->SetWebIDLCallerPrincipal(webIDLCallerPrincipal);
    nsIGlobalObject* incumbent = aCallback->IncumbentGlobalOrNull();
    if (incumbent) {
      // The callback object traces its incumbent JS global, so in general it
      // should be alive here. However, it's possible that we could run afoul
      // of the same IPC global weirdness described above, wherein the
      // nsIGlobalObject has severed its reference to the JS global. Let's just
      // be safe here, so that nobody has to waste a day debugging gaia-ui tests.
      if (!incumbent->GetGlobalJSObject()) {
        return;
      }
      mAutoIncumbentScript.emplace(incumbent);
    }

    // Unmark the callable (by invoking Callback() and not the CallbackPreserveColor()
    // variant), and stick it in a Rooted before it can go gray again.
    // Nothing before us in this function can trigger a CC, so it's safe to wait
    // until here it do the unmark. This allows us to order the following two
    // operations _after_ the Push() above, which lets us take advantage of the
    // JSAutoRequest embedded in the pusher.
    //
    // We can do this even though we're not in the right compartment yet, because
    // Rooted<> does not care about compartments.
    mRootedCallable.emplace(cx, aCallback->Callback());
  }

  // JS-implemented WebIDL is always OK to run, since it runs with Chrome
  // privileges anyway.
  if (mIsMainThread && !aIsJSImplementedWebIDL) {
    // Check that it's ok to run this callback at all.
    // Make sure to use realCallback to get the global of the callback object,
    // not the wrapper.
    bool allowed = nsContentUtils::GetSecurityManager()->
      ScriptAllowed(js::GetGlobalForObjectCrossCompartment(realCallback));

    if (!allowed) {
      return;
    }
  }

  // Enter the compartment of our callback, so we can actually work with it.
  //
  // Note that if the callback is a wrapper, this will not be the same
  // compartment that we ended up in with mAutoEntryScript above, because the
  // entry point is based off of the unwrapped callback (realCallback).
  mAc.emplace(cx, *mRootedCallable);

  // And now we're ready to go.
  mCx = cx;

  // Make sure the JS engine doesn't report exceptions we want to re-throw
  if ((mCompartment && mExceptionHandling == eRethrowContentExceptions) ||
      mExceptionHandling == eRethrowExceptions) {
    mSavedJSContextOptions = JS::ContextOptionsRef(cx);
    JS::ContextOptionsRef(cx).setDontReportUncaught(true);
  }
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
  obj = js::UncheckedUnwrap(obj, /* stopAtOuter = */ false);
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
    bool needToDealWithException = JS_IsExceptionPending(mCx);
    if ((mCompartment && mExceptionHandling == eRethrowContentExceptions) ||
        mExceptionHandling == eRethrowExceptions) {
      // Restore the old context options
      JS::ContextOptionsRef(mCx) = mSavedJSContextOptions;
      mErrorResult.MightThrowJSException();
      if (needToDealWithException) {
        JS::Rooted<JS::Value> exn(mCx);
        if (JS_GetPendingException(mCx, &exn) &&
            ShouldRethrowException(exn)) {
          mErrorResult.ThrowJSException(mCx, exn);
          JS_ClearPendingException(mCx);
          needToDealWithException = false;
        }
      }
    }

    if (needToDealWithException) {
      // Either we're supposed to report our exceptions, or we're supposed to
      // re-throw them but we failed to JS_GetPendingException.  Either way,
      // just report the pending exception, if any.
      //
      // We don't use nsJSUtils::ReportPendingException here because all it
      // does at this point is JS_SaveFrameChain and enter a compartment around
      // a JS_ReportPendingException call.  But our mAutoEntryScript should
      // already do a JS_SaveFrameChain and we are already in the compartment
      // we want to be in, so all nsJSUtils::ReportPendingException would do is
      // screw up our compartment, which is exactly what we do not want.
      //
      // XXXbz FIXME: bug 979525 means we don't always JS_SaveFrameChain here,
      // so we need to go ahead and do that.
      JS::Rooted<JSObject*> oldGlobal(mCx, JS::CurrentGlobalOrNull(mCx));
      MOZ_ASSERT(oldGlobal, "How can we not have a global here??");
      bool saved = JS_SaveFrameChain(mCx);
      // Make sure the JSAutoCompartment goes out of scope before the
      // JS_RestoreFrameChain call!
      {
        JSAutoCompartment ac(mCx, oldGlobal);
        MOZ_ASSERT(!JS::DescribeScriptedCaller(mCx),
                   "Our comment above about JS_SaveFrameChain having been "
                   "called is a lie?");
        JS_ReportPendingException(mCx);
      }
      if (saved) {
        JS_RestoreFrameChain(mCx);
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

  AutoSafeJSContext cx;

  JS::Rooted<JSObject*> callback(cx, aCallback->Callback());

  JSAutoCompartment ac(cx, callback);
  nsRefPtr<nsXPCWrappedJS> wrappedJS;
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
