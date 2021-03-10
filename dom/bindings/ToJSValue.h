/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ToJSValue_h
#define mozilla_dom_ToJSValue_h

#include <cstddef>  // for size_t
#include <cstdint>  // for int32_t, int64_t, uint32_t, uint64_t
#include <type_traits>  // for is_base_of, enable_if_t, enable_if, is_pointer, is_same, void_t
#include <utility>          // for forward
#include "ErrorList.h"      // for nsresult
#include "js/Array.h"       // for NewArrayObject
#include "js/GCVector.h"    // for RootedVector, MutableWrappedPtrOperations
#include "js/RootingAPI.h"  // for MutableHandle, Rooted, Handle, Heap
#include "js/Value.h"       // for Value
#include "js/ValueArray.h"  // for HandleValueArray
#include "jsapi.h"          // for CurrentGlobalOrNull
#include "mozilla/Assertions.h"  // for AssertionConditionType, MOZ_ASSERT, MOZ_ASSERT_HELPER1
#include "mozilla/UniquePtr.h"         // for UniquePtr
#include "mozilla/Unused.h"            // for Unused
#include "mozilla/dom/BindingUtils.h"  // for MaybeWrapValue, MaybeWrapObjectOrNullValue, XPCOMObjectToJsval, GetOrCreateDOMReflector
#include "mozilla/dom/CallbackObject.h"  // for CallbackObject
#include "nsID.h"                        // for NS_GET_TEMPLATE_IID, nsIID
#include "nsISupports.h"                 // for nsISupports
#include "nsStringFwd.h"                 // for nsAString
#include "nsTArrayForwardDeclare.h"
#include "xpcObjectHelper.h"  // for xpcObjectHelper

namespace mozilla {
namespace dom {

class CallbackObject;
class Promise;
class WindowProxyHolder;
template <typename TypedArrayType>
class TypedArrayCreator;

// If ToJSValue returns false, it must set an exception on the
// JSContext.

// Accept strings.
[[nodiscard]] bool ToJSValue(JSContext* aCx, const nsAString& aArgument,
                             JS::MutableHandle<JS::Value> aValue);

// Treats the input as UTF-8, and throws otherwise.
[[nodiscard]] bool ToJSValue(JSContext* aCx, const nsACString& aArgument,
                             JS::MutableHandle<JS::Value> aValue);

// Accept booleans.  But be careful here: if we just have a function that takes
// a boolean argument, then any pointer that doesn't match one of our other
// signatures/templates will get treated as a boolean, which is clearly not
// desirable.  So make this a template that only gets used if the argument type
// is actually boolean
template <typename T>
[[nodiscard]] std::enable_if_t<std::is_same<T, bool>::value, bool> ToJSValue(
    JSContext* aCx, T aArgument, JS::MutableHandle<JS::Value> aValue) {
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setBoolean(aArgument);
  return true;
}

// Accept integer types
inline bool ToJSValue(JSContext* aCx, int32_t aArgument,
                      JS::MutableHandle<JS::Value> aValue) {
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setInt32(aArgument);
  return true;
}

inline bool ToJSValue(JSContext* aCx, uint32_t aArgument,
                      JS::MutableHandle<JS::Value> aValue) {
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setNumber(aArgument);
  return true;
}

inline bool ToJSValue(JSContext* aCx, int64_t aArgument,
                      JS::MutableHandle<JS::Value> aValue) {
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setNumber(double(aArgument));
  return true;
}

inline bool ToJSValue(JSContext* aCx, uint64_t aArgument,
                      JS::MutableHandle<JS::Value> aValue) {
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setNumber(double(aArgument));
  return true;
}

// accept floating point types
inline bool ToJSValue(JSContext* aCx, float aArgument,
                      JS::MutableHandle<JS::Value> aValue) {
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setNumber(aArgument);
  return true;
}

inline bool ToJSValue(JSContext* aCx, double aArgument,
                      JS::MutableHandle<JS::Value> aValue) {
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setNumber(aArgument);
  return true;
}

// Accept CallbackObjects
[[nodiscard]] inline bool ToJSValue(JSContext* aCx, CallbackObject& aArgument,
                                    JS::MutableHandle<JS::Value> aValue) {
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setObjectOrNull(aArgument.Callback(aCx));

  return MaybeWrapValue(aCx, aValue);
}

// Accept objects that inherit from nsWrapperCache (e.g. most
// DOM objects).
template <class T>
[[nodiscard]] std::enable_if_t<std::is_base_of<nsWrapperCache, T>::value, bool>
ToJSValue(JSContext* aCx, T& aArgument, JS::MutableHandle<JS::Value> aValue) {
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  return GetOrCreateDOMReflector(aCx, aArgument, aValue);
}

// Accept non-refcounted DOM objects that do not inherit from
// nsWrapperCache.  Refcounted ones would be too much of a footgun:
// you could convert them to JS twice and get two different objects.
namespace binding_detail {
template <class T>
[[nodiscard]] std::enable_if_t<
    std::is_base_of<NonRefcountedDOMObject, T>::value, bool>
ToJSValueFromPointerHelper(JSContext* aCx, T* aArgument,
                           JS::MutableHandle<JS::Value> aValue) {
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  // This is a cut-down version of
  // WrapNewBindingNonWrapperCachedObject that doesn't need to deal
  // with nearly as many cases.
  if (!aArgument) {
    aValue.setNull();
    return true;
  }

  JS::Rooted<JSObject*> obj(aCx);
  if (!aArgument->WrapObject(aCx, nullptr, &obj)) {
    return false;
  }

  aValue.setObject(*obj);
  return true;
}
}  // namespace binding_detail

// We can take a non-refcounted non-wrapper-cached DOM object that lives in a
// UniquePtr.
template <class T>
[[nodiscard]] std::enable_if_t<
    std::is_base_of<NonRefcountedDOMObject, T>::value, bool>
ToJSValue(JSContext* aCx, UniquePtr<T>&& aArgument,
          JS::MutableHandle<JS::Value> aValue) {
  if (!binding_detail::ToJSValueFromPointerHelper(aCx, aArgument.get(),
                                                  aValue)) {
    return false;
  }

  // JS object took ownership
  Unused << aArgument.release();
  return true;
}

// Accept typed arrays built from appropriate nsTArray values
template <typename T>
[[nodiscard]]
typename std::enable_if<std::is_base_of<AllTypedArraysBase, T>::value,
                        bool>::type
ToJSValue(JSContext* aCx, const TypedArrayCreator<T>& aArgument,
          JS::MutableHandle<JS::Value> aValue) {
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  JSObject* obj = aArgument.Create(aCx);
  if (!obj) {
    return false;
  }
  aValue.setObject(*obj);
  return true;
}

namespace binding_detail {
// Helper type alias for picking a script-exposable non-wrappercached XPIDL
// interface to expose to JS code. Falls back to `nsISupports` if the specific
// interface type is ambiguous.
template <typename T, typename = void>
struct GetScriptableInterfaceType {
  using Type = nsISupports;

  static_assert(std::is_base_of_v<nsISupports, T>,
                "T must inherit from nsISupports");
};
template <typename T>
struct GetScriptableInterfaceType<
    T, std::void_t<typename T::ScriptableInterfaceType>> {
  using Type = typename T::ScriptableInterfaceType;

  static_assert(std::is_base_of_v<Type, T>,
                "T must inherit from ScriptableInterfaceType");
  static_assert(std::is_base_of_v<nsISupports, Type>,
                "ScriptableInterfaceType must inherit from nsISupports");
};

template <typename T>
using ScriptableInterfaceType = typename GetScriptableInterfaceType<T>::Type;
}  // namespace binding_detail

// Accept objects that inherit from nsISupports but not nsWrapperCache (e.g.
// DOM File).
template <class T>
[[nodiscard]] std::enable_if_t<!std::is_base_of<nsWrapperCache, T>::value &&
                                   !std::is_base_of<CallbackObject, T>::value &&
                                   std::is_base_of<nsISupports, T>::value,
                               bool>
ToJSValue(JSContext* aCx, T& aArgument, JS::MutableHandle<JS::Value> aValue) {
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  xpcObjectHelper helper(ToSupports(&aArgument));
  JS::Rooted<JSObject*> scope(aCx, JS::CurrentGlobalOrNull(aCx));
  const nsIID& iid =
      NS_GET_TEMPLATE_IID(binding_detail::ScriptableInterfaceType<T>);
  return XPCOMObjectToJsval(aCx, scope, helper, &iid, true, aValue);
}

[[nodiscard]] bool ToJSValue(JSContext* aCx, const WindowProxyHolder& aArgument,
                             JS::MutableHandle<JS::Value> aValue);

// Accept nsRefPtr/nsCOMPtr
template <typename T>
[[nodiscard]] bool ToJSValue(JSContext* aCx, const nsCOMPtr<T>& aArgument,
                             JS::MutableHandle<JS::Value> aValue) {
  return ToJSValue(aCx, *aArgument.get(), aValue);
}

template <typename T>
[[nodiscard]] bool ToJSValue(JSContext* aCx, const RefPtr<T>& aArgument,
                             JS::MutableHandle<JS::Value> aValue) {
  return ToJSValue(aCx, *aArgument.get(), aValue);
}

template <typename T>
[[nodiscard]] bool ToJSValue(JSContext* aCx, const NonNull<T>& aArgument,
                             JS::MutableHandle<JS::Value> aValue) {
  return ToJSValue(aCx, *aArgument.get(), aValue);
}

// Accept WebIDL dictionaries
template <class T>
[[nodiscard]] std::enable_if_t<std::is_base_of<DictionaryBase, T>::value, bool>
ToJSValue(JSContext* aCx, const T& aArgument,
          JS::MutableHandle<JS::Value> aValue) {
  return aArgument.ToObjectInternal(aCx, aValue);
}

// Accept existing JS values (which may not be same-compartment with us
[[nodiscard]] inline bool ToJSValue(JSContext* aCx, const JS::Value& aArgument,
                                    JS::MutableHandle<JS::Value> aValue) {
  aValue.set(aArgument);
  return MaybeWrapValue(aCx, aValue);
}
[[nodiscard]] inline bool ToJSValue(JSContext* aCx,
                                    JS::Handle<JS::Value> aArgument,
                                    JS::MutableHandle<JS::Value> aValue) {
  aValue.set(aArgument);
  return MaybeWrapValue(aCx, aValue);
}

// Accept existing JS values on the Heap (which may not be same-compartment with
// us
[[nodiscard]] inline bool ToJSValue(JSContext* aCx,
                                    const JS::Heap<JS::Value>& aArgument,
                                    JS::MutableHandle<JS::Value> aValue) {
  aValue.set(aArgument);
  return MaybeWrapValue(aCx, aValue);
}

// Accept existing rooted JS values (which may not be same-compartment with us
[[nodiscard]] inline bool ToJSValue(JSContext* aCx,
                                    const JS::Rooted<JS::Value>& aArgument,
                                    JS::MutableHandle<JS::Value> aValue) {
  aValue.set(aArgument);
  return MaybeWrapValue(aCx, aValue);
}

// Accept existing rooted JS objects (which may not be same-compartment with
// us).
[[nodiscard]] inline bool ToJSValue(JSContext* aCx,
                                    const JS::Rooted<JSObject*>& aArgument,
                                    JS::MutableHandle<JS::Value> aValue) {
  aValue.setObjectOrNull(aArgument);
  return MaybeWrapObjectOrNullValue(aCx, aValue);
}

// Accept nsresult, for use in rejections, and create an XPCOM
// exception object representing that nsresult.
[[nodiscard]] bool ToJSValue(JSContext* aCx, nsresult aArgument,
                             JS::MutableHandle<JS::Value> aValue);

// Accept ErrorResult, for use in rejections, and create an exception
// representing the failure.  Note, the ErrorResult must indicate a failure
// with aArgument.Failure() returning true.
[[nodiscard]] bool ToJSValue(JSContext* aCx, ErrorResult&& aArgument,
                             JS::MutableHandle<JS::Value> aValue);

// Accept owning WebIDL unions.
template <typename T>
[[nodiscard]] std::enable_if_t<std::is_base_of<AllOwningUnionBase, T>::value,
                               bool>
ToJSValue(JSContext* aCx, const T& aArgument,
          JS::MutableHandle<JS::Value> aValue) {
  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
  return aArgument.ToJSVal(aCx, global, aValue);
}

// Accept pointers to other things we accept
template <typename T>
[[nodiscard]] std::enable_if_t<std::is_pointer<T>::value, bool> ToJSValue(
    JSContext* aCx, T aArgument, JS::MutableHandle<JS::Value> aValue) {
  return ToJSValue(aCx, *aArgument, aValue);
}

// Accept Promise objects, which need special handling.
[[nodiscard]] bool ToJSValue(JSContext* aCx, Promise& aArgument,
                             JS::MutableHandle<JS::Value> aValue);

// Accept arrays (and nested arrays) of other things we accept
template <typename T>
[[nodiscard]] bool ToJSValue(JSContext* aCx, T* aArguments, size_t aLength,
                             JS::MutableHandle<JS::Value> aValue);

template <typename T>
[[nodiscard]] bool ToJSValue(JSContext* aCx, const nsTArray<T>& aArgument,
                             JS::MutableHandle<JS::Value> aValue) {
  return ToJSValue(aCx, aArgument.Elements(), aArgument.Length(), aValue);
}

template <typename T>
[[nodiscard]] bool ToJSValue(JSContext* aCx, const FallibleTArray<T>& aArgument,
                             JS::MutableHandle<JS::Value> aValue) {
  return ToJSValue(aCx, aArgument.Elements(), aArgument.Length(), aValue);
}

template <typename T, int N>
[[nodiscard]] bool ToJSValue(JSContext* aCx, const T (&aArgument)[N],
                             JS::MutableHandle<JS::Value> aValue) {
  return ToJSValue(aCx, aArgument, N, aValue);
}

// Accept arrays of other things we accept
template <typename T>
[[nodiscard]] bool ToJSValue(JSContext* aCx, T* aArguments, size_t aLength,
                             JS::MutableHandle<JS::Value> aValue) {
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  JS::RootedVector<JS::Value> v(aCx);
  if (!v.resize(aLength)) {
    return false;
  }
  for (size_t i = 0; i < aLength; ++i) {
    if (!ToJSValue(aCx, aArguments[i], v[i])) {
      return false;
    }
  }
  JSObject* arrayObj = JS::NewArrayObject(aCx, v);
  if (!arrayObj) {
    return false;
  }
  aValue.setObject(*arrayObj);
  return true;
}

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_ToJSValue_h */
