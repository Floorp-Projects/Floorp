/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "nsAString.h"
#include "nsContentUtils.h"
#include "nsStringBuffer.h"
#include "xpcpublic.h"

namespace mozilla {
namespace dom {

bool ToJSValue(JSContext* aCx, const nsAString& aArgument,
               JS::MutableHandle<JS::Value> aValue) {
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  // XXXkhuey I'd love to use xpc::NonVoidStringToJsval here, but it requires
  // a non-const nsAString for silly reasons.
  nsStringBuffer* sharedBuffer;
  if (!XPCStringConvert::ReadableToJSVal(aCx, aArgument, &sharedBuffer,
                                         aValue)) {
    return false;
  }

  if (sharedBuffer) {
    NS_ADDREF(sharedBuffer);
  }

  return true;
}

bool ToJSValue(JSContext* aCx, nsresult aArgument,
               JS::MutableHandle<JS::Value> aValue) {
  RefPtr<Exception> exception = CreateException(aArgument);
  return ToJSValue(aCx, exception, aValue);
}

bool ToJSValue(JSContext* aCx, ErrorResult&& aArgument,
               JS::MutableHandle<JS::Value> aValue) {
  MOZ_ASSERT(aArgument.Failed());
  MOZ_ASSERT(
      !aArgument.IsUncatchableException(),
      "Doesn't make sense to convert uncatchable exception to a JS value!");
  MOZ_ALWAYS_TRUE(aArgument.MaybeSetPendingException(aCx));
  MOZ_ALWAYS_TRUE(JS_GetPendingException(aCx, aValue));
  JS_ClearPendingException(aCx);
  return true;
}

bool ToJSValue(JSContext* aCx, Promise& aArgument,
               JS::MutableHandle<JS::Value> aValue) {
  aValue.setObject(*aArgument.PromiseObj());
  return MaybeWrapObjectValue(aCx, aValue);
}

bool ToJSValue(JSContext* aCx, const WindowProxyHolder& aArgument,
               JS::MutableHandle<JS::Value> aValue) {
  BrowsingContext* bc = aArgument.get();
  if (!bc) {
    aValue.setNull();
    return true;
  }
  JS::Rooted<JSObject*> windowProxy(aCx);
  if (bc->IsInProcess()) {
    windowProxy = bc->GetWindowProxy();
    if (!windowProxy) {
      nsPIDOMWindowOuter* window = bc->GetDOMWindow();
      if (!window) {
        // Torn down enough that we should just return null.
        aValue.setNull();
        return true;
      }
      if (!window->EnsureInnerWindow()) {
        return Throw(aCx, NS_ERROR_UNEXPECTED);
      }
      windowProxy = bc->GetWindowProxy();
    }
    return ToJSValue(aCx, windowProxy, aValue);
  }

  if (!GetRemoteOuterWindowProxy(aCx, bc, /* aTransplantTo = */ nullptr,
                                 &windowProxy)) {
    return false;
  }
  aValue.setObjectOrNull(windowProxy);
  return true;
}

// Static assertion tests for the `binding_detail::ScriptableInterfaceType`
// helper template, used by `ToJSValue`.
namespace binding_detail {
static_assert(std::is_same_v<ScriptableInterfaceType<nsISupports>, nsISupports>,
              "nsISupports works with ScriptableInterfaceType");
static_assert(
    std::is_same_v<ScriptableInterfaceType<nsIGlobalObject>, nsISupports>,
    "non-scriptable interfaces get a fallback");
static_assert(std::is_same_v<ScriptableInterfaceType<nsIObserver>, nsIObserver>,
              "scriptable interfaces should get the correct type");
static_assert(std::is_same_v<ScriptableInterfaceType<nsIRunnable>, nsIRunnable>,
              "scriptable interfaces should get the correct type");
class SingleScriptableInterface : public nsIObserver {};
static_assert(
    std::is_same_v<ScriptableInterfaceType<SingleScriptableInterface>,
                   nsIObserver>,
    "Concrete type with one scriptable interface picks the correct interface");
class MultiScriptableInterface : public nsIObserver, public nsIRunnable {};
static_assert(std::is_same_v<ScriptableInterfaceType<MultiScriptableInterface>,
                             nsISupports>,
              "Concrete type with multiple scriptable interfaces falls back");
}  // namespace binding_detail
}  // namespace dom
}  // namespace mozilla
