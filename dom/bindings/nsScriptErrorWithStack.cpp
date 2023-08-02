/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsScriptErrorWithStack implementation.
 * a main-thread-only, cycle-collected subclass of nsScriptErrorBase
 * that can store a SavedFrame stack trace object.
 */

#include "nsScriptError.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/ScriptSettings.h"
#include "js/Wrapper.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGlobalWindowInner.h"
#include "nsJSUtils.h"

using namespace mozilla::dom;

namespace {

static nsCString FormatStackString(JSContext* cx, JSPrincipals* aPrincipals,
                                   JS::Handle<JSObject*> aStack) {
  JS::Rooted<JSString*> formattedStack(cx);
  if (!JS::BuildStackString(cx, aPrincipals, aStack, &formattedStack)) {
    return nsCString();
  }

  nsAutoJSString stackJSString;
  if (!stackJSString.init(cx, formattedStack)) {
    return nsCString();
  }

  return NS_ConvertUTF16toUTF8(stackJSString.get());
}

}  // namespace

NS_IMPL_CYCLE_COLLECTION_CLASS(nsScriptErrorWithStack)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsScriptErrorWithStack)
  tmp->mException.setUndefined();
  tmp->mStack = nullptr;
  tmp->mStackGlobal = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsScriptErrorWithStack)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsScriptErrorWithStack)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mException)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mStack)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mStackGlobal)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsScriptErrorWithStack)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsScriptErrorWithStack)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsScriptErrorWithStack)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIConsoleMessage)
  NS_INTERFACE_MAP_ENTRY(nsIScriptError)
NS_INTERFACE_MAP_END

nsScriptErrorWithStack::nsScriptErrorWithStack(
    JS::Handle<mozilla::Maybe<JS::Value>> aException,
    JS::Handle<JSObject*> aStack, JS::Handle<JSObject*> aStackGlobal)
    : mStack(aStack), mStackGlobal(aStackGlobal) {
  MOZ_ASSERT(NS_IsMainThread(), "You can't use this class on workers.");

  if (aException.isSome()) {
    mHasException = true;
    mException.set(*aException);
  } else {
    mHasException = false;
    mException.setUndefined();
  }

  if (mStack) {
    MOZ_ASSERT(JS_IsGlobalObject(mStackGlobal));
    js::AssertSameCompartment(mStack, mStackGlobal);
  } else {
    MOZ_ASSERT(!mStackGlobal);
  }

  mozilla::HoldJSObjects(this);
}

nsScriptErrorWithStack::~nsScriptErrorWithStack() {
  mozilla::DropJSObjects(this);
}

NS_IMETHODIMP
nsScriptErrorWithStack::GetHasException(bool* aHasException) {
  *aHasException = mHasException;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorWithStack::GetException(JS::MutableHandle<JS::Value> aException) {
  aException.set(mException);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorWithStack::GetStack(JS::MutableHandle<JS::Value> aStack) {
  aStack.setObjectOrNull(mStack);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorWithStack::GetStackGlobal(
    JS::MutableHandle<JS::Value> aStackGlobal) {
  aStackGlobal.setObjectOrNull(mStackGlobal);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptErrorWithStack::ToString(nsACString& /*UTF8*/ aResult) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCString message;
  nsresult rv = nsScriptErrorBase::ToString(message);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mStack) {
    aResult.Assign(message);
    return NS_OK;
  }

  AutoJSAPI jsapi;
  if (!jsapi.Init(mStackGlobal)) {
    return NS_ERROR_FAILURE;
  }

  JSPrincipals* principals =
      JS::GetRealmPrincipals(js::GetNonCCWObjectRealm(mStackGlobal));

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> stack(cx, mStack);
  nsCString stackString = FormatStackString(cx, principals, stack);
  nsCString combined = message + "\n"_ns + stackString;
  aResult.Assign(combined);

  return NS_OK;
}

static bool IsObjectGlobalDying(JSObject* aObj) {
  // CCWs are not associated with a single global
  if (js::IsCrossCompartmentWrapper(aObj)) {
    return false;
  }

  nsGlobalWindowInner* win = xpc::WindowGlobalOrNull(aObj);
  return win && win->IsDying();
}

already_AddRefed<nsScriptErrorBase> CreateScriptError(
    nsGlobalWindowInner* win, JS::Handle<mozilla::Maybe<JS::Value>> aException,
    JS::Handle<JSObject*> aStack, JS::Handle<JSObject*> aStackGlobal) {
  bool createWithStack = true;
  if (aException.isNothing() && !aStack) {
    // Neither stack nor exception, do not need nsScriptErrorWithStack.
    createWithStack = false;
  } else if (win && (win->IsDying() || !win->WindowID())) {
    // The window is already dying or we don't have a WindowID,
    // this means nsConsoleService::ClearMessagesForWindowID
    // would be unable to cleanup this error.
    createWithStack = false;
  } else if ((aStackGlobal && IsObjectGlobalDying(aStackGlobal)) ||
             (aException.isSome() && aException.value().isObject() &&
              IsObjectGlobalDying(&aException.value().toObject()))) {
    // Prevent leaks by not creating references to already dying globals.
    createWithStack = false;
  }

  if (!createWithStack) {
    RefPtr<nsScriptErrorBase> error = new nsScriptError();
    return error.forget();
  }

  RefPtr<nsScriptErrorBase> error =
      new nsScriptErrorWithStack(aException, aStack, aStackGlobal);
  return error.forget();
}
