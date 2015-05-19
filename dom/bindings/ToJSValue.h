/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ToJSValue_h
#define mozilla_dom_ToJSValue_h

#include "mozilla/TypeTraits.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/TypedArray.h"
#include "jsapi.h"
#include "nsISupports.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

// If ToJSValue returns false, it must set an exception on the
// JSContext.

// Accept strings.
MOZ_WARN_UNUSED_RESULT bool
ToJSValue(JSContext* aCx,
          const nsAString& aArgument,
          JS::MutableHandle<JS::Value> aValue);

// Accept booleans.  But be careful here: if we just have a function that takes
// a boolean argument, then any pointer that doesn't match one of our other
// signatures/templates will get treated as a boolean, which is clearly not
// desirable.  So make this a template that only gets used if the argument type
// is actually boolean
template<typename T>
MOZ_WARN_UNUSED_RESULT
typename EnableIf<IsSame<T, bool>::value, bool>::Type
ToJSValue(JSContext* aCx,
          T aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setBoolean(aArgument);
  return true;
}

// Accept integer types
inline bool
ToJSValue(JSContext* aCx,
          int32_t aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setInt32(aArgument);
  return true;
}

inline bool
ToJSValue(JSContext* aCx,
          uint32_t aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setNumber(aArgument);
  return true;
}

inline bool
ToJSValue(JSContext* aCx,
          int64_t aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setNumber(double(aArgument));
  return true;
}

inline bool
ToJSValue(JSContext* aCx,
          uint64_t aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setNumber(double(aArgument));
  return true;
}

// accept floating point types
inline bool
ToJSValue(JSContext* aCx,
          float aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setNumber(aArgument);
  return true;
}

inline bool
ToJSValue(JSContext* aCx,
          double aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setNumber(aArgument);
  return true;
}

// Accept CallbackObjects
MOZ_WARN_UNUSED_RESULT inline bool
ToJSValue(JSContext* aCx,
          CallbackObject& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setObject(*aArgument.Callback());

  return MaybeWrapValue(aCx, aValue);
}

// Accept objects that inherit from nsWrapperCache (e.g. most
// DOM objects).
template <class T>
MOZ_WARN_UNUSED_RESULT
typename EnableIf<IsBaseOf<nsWrapperCache, T>::value, bool>::Type
ToJSValue(JSContext* aCx,
          T& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));
  // Make sure non-webidl objects don't sneak in here
  MOZ_ASSERT(aArgument.IsDOMBinding());

  return GetOrCreateDOMReflector(aCx, aArgument, aValue);
}

// Accept typed arrays built from appropriate nsTArray values
template<typename T>
MOZ_WARN_UNUSED_RESULT
typename EnableIf<IsBaseOf<AllTypedArraysBase, T>::value, bool>::Type
ToJSValue(JSContext* aCx,
          const TypedArrayCreator<T>& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  JSObject* obj = aArgument.Create(aCx);
  if (!obj) {
    return false;
  }
  aValue.setObject(*obj);
  return true;
}

// Accept objects that inherit from nsISupports but not nsWrapperCache (e.g.
// DOM File).
template <class T>
MOZ_WARN_UNUSED_RESULT
typename EnableIf<!IsBaseOf<nsWrapperCache, T>::value &&
                  !IsBaseOf<CallbackObject, T>::value &&
                  IsBaseOf<nsISupports, T>::value, bool>::Type
ToJSValue(JSContext* aCx,
          T& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  qsObjectHelper helper(ToSupports(&aArgument), nullptr);
  JS::Rooted<JSObject*> scope(aCx, JS::CurrentGlobalOrNull(aCx));
  return XPCOMObjectToJsval(aCx, scope, helper, nullptr, true, aValue);
}

// Accept nsRefPtr/nsCOMPtr
template <typename T>
MOZ_WARN_UNUSED_RESULT bool
ToJSValue(JSContext* aCx,
          const nsCOMPtr<T>& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  return ToJSValue(aCx, *aArgument.get(), aValue);
}

template <typename T>
MOZ_WARN_UNUSED_RESULT bool
ToJSValue(JSContext* aCx,
          const nsRefPtr<T>& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  return ToJSValue(aCx, *aArgument.get(), aValue);
}

// Accept WebIDL dictionaries
template <class T>
MOZ_WARN_UNUSED_RESULT
typename EnableIf<IsBaseOf<DictionaryBase, T>::value, bool>::Type
ToJSValue(JSContext* aCx,
          const T& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  return aArgument.ToObjectInternal(aCx, aValue);
}

// Accept existing JS values (which may not be same-compartment with us
MOZ_WARN_UNUSED_RESULT inline bool
ToJSValue(JSContext* aCx, JS::Handle<JS::Value> aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  aValue.set(aArgument);
  return MaybeWrapValue(aCx, aValue);
}

// Accept existing JS values on the Heap (which may not be same-compartment with us
MOZ_WARN_UNUSED_RESULT inline bool
ToJSValue(JSContext* aCx, const JS::Heap<JS::Value>& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  aValue.set(aArgument);
  return MaybeWrapValue(aCx, aValue);
}

// Accept existing rooted JS values (which may not be same-compartment with us
MOZ_WARN_UNUSED_RESULT inline bool
ToJSValue(JSContext* aCx, const JS::Rooted<JS::Value>& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  aValue.set(aArgument);
  return MaybeWrapValue(aCx, aValue);
}

// Accept existing rooted JS objects (which may not be same-compartment with
// us).
MOZ_WARN_UNUSED_RESULT inline bool
ToJSValue(JSContext* aCx, const JS::Rooted<JSObject*>& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  aValue.setObjectOrNull(aArgument);
  return MaybeWrapObjectOrNullValue(aCx, aValue);
}

// Accept nsresult, for use in rejections, and create an XPCOM
// exception object representing that nsresult.
MOZ_WARN_UNUSED_RESULT bool
ToJSValue(JSContext* aCx,
          nsresult aArgument,
          JS::MutableHandle<JS::Value> aValue);

// Accept ErrorResult, for use in rejections, and create an exception
// representing the failure.  Note, the ErrorResult must indicate a failure
// with aArgument.Failure() returning true.
MOZ_WARN_UNUSED_RESULT bool
ToJSValue(JSContext* aCx,
          ErrorResult& aArgument,
          JS::MutableHandle<JS::Value> aValue);

// Accept pointers to other things we accept
template <typename T>
MOZ_WARN_UNUSED_RESULT
typename EnableIf<IsPointer<T>::value, bool>::Type
ToJSValue(JSContext* aCx,
          T aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  return ToJSValue(aCx, *aArgument, aValue);
}

// Accept arrays of other things we accept
template <typename T>
MOZ_WARN_UNUSED_RESULT bool
ToJSValue(JSContext* aCx,
          T* aArguments,
          size_t aLength,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  JS::AutoValueVector v(aCx);
  if (!v.resize(aLength)) {
    return false;
  }
  for (size_t i = 0; i < aLength; ++i) {
    if (!ToJSValue(aCx, aArguments[i], v[i])) {
      return false;
    }
  }
  JSObject* arrayObj = JS_NewArrayObject(aCx, v);
  if (!arrayObj) {
    return false;
  }
  aValue.setObject(*arrayObj);
  return true;
}

template <typename T>
MOZ_WARN_UNUSED_RESULT bool
ToJSValue(JSContext* aCx,
          const nsTArray<T>& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  return ToJSValue(aCx, aArgument.Elements(),
                   aArgument.Length(), aValue);
}

template <typename T>
MOZ_WARN_UNUSED_RESULT bool
ToJSValue(JSContext* aCx,
          const FallibleTArray<T>& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  return ToJSValue(aCx, aArgument.Elements(),
                   aArgument.Length(), aValue);
}

template <typename T, int N>
MOZ_WARN_UNUSED_RESULT bool
ToJSValue(JSContext* aCx,
          const T(&aArgument)[N],
          JS::MutableHandle<JS::Value> aValue)
{
  return ToJSValue(aCx, aArgument, N, aValue);
}

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_ToJSValue_h */
