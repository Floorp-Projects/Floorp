/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
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
bool
ToJSValue(JSContext* aCx,
          const nsAString& aArgument,
          JS::MutableHandle<JS::Value> aValue);

// Accept booleans.
inline bool
ToJSValue(JSContext* aCx,
          bool aArgument,
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

// Accept objects that inherit from nsWrapperCache and nsISupports (e.g. most
// DOM objects).
template <class T>
typename EnableIf<IsBaseOf<nsWrapperCache, T>::value &&
                  IsBaseOf<nsISupports, T>::value, bool>::Type
ToJSValue(JSContext* aCx,
          T& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));
  // Make sure non-webidl objects don't sneak in here
  MOZ_ASSERT(aArgument.IsDOMBinding());

  return WrapNewBindingObject(aCx, aArgument, aValue);
}

// Accept typed arrays built from appropriate nsTArray values
template<typename T>
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

// We don't want to include nsContentUtils here, so use a helper
// function for the nsISupports case.
namespace tojsvalue_detail {
bool
ISupportsToJSValue(JSContext* aCx,
                   nsISupports* aArgument,
                   JS::MutableHandle<JS::Value> aValue);
} // namespace tojsvalue_detail

// Accept objects that inherit from nsISupports but not nsWrapperCache (e.g.
// nsIDOMFile).
template <class T>
typename EnableIf<!IsBaseOf<nsWrapperCache, T>::value &&
                  IsBaseOf<nsISupports, T>::value, bool>::Type
ToJSValue(JSContext* aCx,
          T& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  return tojsvalue_detail::ISupportsToJSValue(aCx, &aArgument, aValue);
}

// Accept nsRefPtr/nsCOMPtr
template <template <typename> class SmartPtr, typename T>
bool
ToJSValue(JSContext* aCx,
          const SmartPtr<T>& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  return ToJSValue(aCx, *aArgument.get(), aValue);
}

// Accept arrays of other things we accept
template <typename T>
bool
ToJSValue(JSContext* aCx,
          const nsTArray<T>& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  JS::AutoValueVector v(aCx);
  if (!v.resize(aArgument.Length())) {
    return false;
  }
  for (uint32_t i = 0; i < aArgument.Length(); ++i) {
    if (!ToJSValue(aCx, aArgument[i], v.handleAt(i))) {
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

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_ToJSValue_h */
