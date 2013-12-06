/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CallbackObject.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/DOMErrorBinding.h"
#include "jsfriendapi.h"
#include "nsIScriptGlobalObject.h"
#include "nsIXPConnect.h"
#include "nsIScriptContext.h"
#include "nsPIDOMWindow.h"
#include "nsJSUtils.h"
#include "nsCxPusher.h"
#include "nsIScriptSecurityManager.h"
#include "xpcprivate.h"
#include "WorkerPrivate.h"
#include "nsGlobalWindow.h"
#include "WorkerScope.h"

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
  tmp->DropCallback();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIncumbentGlobal)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(CallbackObject)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIncumbentGlobal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(CallbackObject)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mCallback)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

CallbackObject::CallSetup::CallSetup(CallbackObject* aCallback,
                                     ErrorResult& aRv,
                                     ExceptionHandling aExceptionHandling,
                                     JSCompartment* aCompartment)
  : mCx(nullptr)
  , mCompartment(aCompartment)
  , mErrorResult(aRv)
  , mExceptionHandling(aExceptionHandling)
  , mIsMainThread(NS_IsMainThread())
{
  if (mIsMainThread) {
    nsContentUtils::EnterMicroTask();
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

  if (mIsMainThread) {
    // Now get the global and JSContext for this callback.
    nsGlobalWindow* win = xpc::WindowGlobalOrNull(realCallback);
    if (win) {
      // Make sure that if this is a window it's the current inner, since the
      // nsIScriptContext and hence JSContext are associated with the outer
      // window.  Which means that if someone holds on to a function from a
      // now-unloaded document we'd have the new document as the script entry
      // point...
      MOZ_ASSERT(win->IsInnerWindow());
      nsPIDOMWindow* outer = win->GetOuterWindow();
      if (!outer || win != outer->GetCurrentInnerWindow()) {
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
      globalObject = xpc::GetNativeForGlobal(glob);
      MOZ_ASSERT(globalObject);
      cx = nsContentUtils::GetSafeJSContext();
    }
  } else {
    cx = workers::GetCurrentThreadJSContext();
    globalObject = workers::GetCurrentThreadWorkerPrivate()->GlobalScope();
  }

  mAutoEntryScript.construct(globalObject, mIsMainThread, cx);
  if (aCallback->IncumbentGlobalOrNull()) {
    mAutoIncumbentScript.construct(aCallback->IncumbentGlobalOrNull());
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
  mRootedCallable.construct(cx, aCallback->Callback());

  if (mIsMainThread) {
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
  mAc.construct(cx, mRootedCallable.ref());

  // And now we're ready to go.
  mCx = cx;

  // Make sure the JS engine doesn't report exceptions we want to re-throw
  if (mExceptionHandling == eRethrowContentExceptions ||
      mExceptionHandling == eRethrowExceptions) {
    mSavedJSContextOptions = JS::ContextOptionsRef(cx);
    JS::ContextOptionsRef(cx).setDontReportUncaught(true);
  }
}

bool
CallbackObject::CallSetup::ShouldRethrowException(JS::Handle<JS::Value> aException)
{
  if (mExceptionHandling == eRethrowExceptions) {
    return true;
  }

  MOZ_ASSERT(mExceptionHandling == eRethrowContentExceptions);

  // For eRethrowContentExceptions we only want to throw an exception if the
  // object that was thrown is a DOMError object in the caller compartment
  // (which we stored in mCompartment).

  if (!aException.isObject()) {
    return false;
  }

  JS::Rooted<JSObject*> obj(mCx, &aException.toObject());
  obj = js::UncheckedUnwrap(obj, /* stopAtOuter = */ false);
  if (js::GetObjectCompartment(obj) != mCompartment) {
    return false;
  }

  DOMError* domError;
  return NS_SUCCEEDED(UNWRAP_OBJECT(DOMError, obj, domError));
}

CallbackObject::CallSetup::~CallSetup()
{
  // First things first: if we have a JSContext, report any pending
  // errors on it, unless we were told to re-throw them.
  if (mCx) {
    bool dealtWithPendingException = false;
    if ((mCompartment && mExceptionHandling == eRethrowContentExceptions) ||
        mExceptionHandling == eRethrowExceptions) {
      // Restore the old context options
      JS::ContextOptionsRef(mCx) = mSavedJSContextOptions;
      mErrorResult.MightThrowJSException();
      if (JS_IsExceptionPending(mCx)) {
        JS::Rooted<JS::Value> exn(mCx);
        if (JS_GetPendingException(mCx, &exn) &&
            ShouldRethrowException(exn)) {
          mErrorResult.ThrowJSException(mCx, exn);
          JS_ClearPendingException(mCx);
          dealtWithPendingException = true;
        }
      }
    }

    if (!dealtWithPendingException) {
      // Either we're supposed to report our exceptions, or we're supposed to
      // re-throw them but we failed to JS_GetPendingException.  Either way,
      // just report the pending exception, if any.
      nsJSUtils::ReportPendingException(mCx);
    }
  }

  // To get our nesting right we have to destroy our JSAutoCompartment first.
  // But be careful: it might not have been constructed at all!
  mAc.destroyIfConstructed();

  mAutoIncumbentScript.destroyIfConstructed();
  mAutoEntryScript.destroyIfConstructed();

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
    nsXPCWrappedJS::GetNewOrUsed(callback, aIID,
                                 nullptr, getter_AddRefs(wrappedJS));
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
