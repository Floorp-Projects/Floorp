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

// The uint32_t version is disabled for now because on the super-old b2g
// compiler nsresult and uint32_t are the same type.  If someone needs this at
// some point we'll need to figure out how to make it work (e.g. by switching to
// traits structs and using the trick IPC's ParamTraits uses, where a traits
// struct templated on the type inherits from a base traits struct of some sort,
// templated on the same type, or something).  Maybe b2g will update to a modern
// compiler before that happens....
#if 0
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
#endif

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
inline bool
ToJSValue(JSContext* aCx,
          CallbackObject& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

  aValue.setObject(*aArgument.Callback());

  return MaybeWrapValue(aCx, aValue);
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
                  !IsBaseOf<CallbackObject, T>::value &&
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
template <typename T>
bool
ToJSValue(JSContext* aCx,
          const nsCOMPtr<T>& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  return ToJSValue(aCx, *aArgument.get(), aValue);
}

template <typename T>
bool
ToJSValue(JSContext* aCx,
          const nsRefPtr<T>& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  return ToJSValue(aCx, *aArgument.get(), aValue);
}

// Accept WebIDL dictionaries
template <class T>
typename EnableIf<IsBaseOf<DictionaryBase, T>::value, bool>::Type
ToJSValue(JSContext* aCx,
          const T& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  return aArgument.ToObjectInternal(aCx, aValue);
}

// Accept existing JS values (which may not be same-compartment with us
inline bool
ToJSValue(JSContext* aCx, JS::Handle<JS::Value> aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  aValue.set(aArgument);
  return MaybeWrapValue(aCx, aValue);
}

// Accept nsresult, for use in rejections, and create an XPCOM
// exception object representing that nsresult.
bool
ToJSValue(JSContext* aCx,
          nsresult aArgument,
          JS::MutableHandle<JS::Value> aValue);

// Accept pointers to other things we accept
template <typename T>
typename EnableIf<IsPointer<T>::value, bool>::Type
ToJSValue(JSContext* aCx,
          T aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  return ToJSValue(aCx, *aArgument, aValue);
}

// Accept arrays of other things we accept
template <typename T>
bool
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
bool
ToJSValue(JSContext* aCx,
          const nsTArray<T>& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  return ToJSValue(aCx, aArgument.Elements(),
                   aArgument.Length(), aValue);
}

template <typename T>
bool
ToJSValue(JSContext* aCx,
          const FallibleTArray<T>& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  return ToJSValue(aCx, aArgument.Elements(),
                   aArgument.Length(), aValue);
}

template <typename T, int N>
bool
ToJSValue(JSContext* aCx,
          const T(&aArgument)[N],
          JS::MutableHandle<JS::Value> aValue)
{
  return ToJSValue(aCx, aArgument, N, aValue);
}

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_ToJSValue_h */
