/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Nullable_h
#define mozilla_dom_Nullable_h

#include "mozilla/Assertions.h"
#include "nsTArrayForwardDeclare.h"

namespace mozilla {
namespace dom {

// Support for nullable types
template <typename T>
struct Nullable
{
private:
  // mIsNull MUST COME FIRST because otherwise the casting in our array
  // conversion operators would shift where it is found in the struct.
  bool mIsNull;
  T mValue;

public:
  Nullable()
    : mIsNull(true)
  {}

  explicit Nullable(T aValue)
    : mIsNull(false)
    , mValue(aValue)
  {}

  void SetValue(T aValue) {
    mValue = aValue;
    mIsNull = false;
  }

  // For cases when |T| is some type with nontrivial copy behavior, we may want
  // to get a reference to our internal copy of T and work with it directly
  // instead of relying on the copying version of SetValue().
  T& SetValue() {
    mIsNull = false;
    return mValue;
  }

  void SetNull() {
    mIsNull = true;
  }

  const T& Value() const {
    MOZ_ASSERT(!mIsNull);
    return mValue;
  }

  T& Value() {
    MOZ_ASSERT(!mIsNull);
    return mValue;
  }

  bool IsNull() const {
    return mIsNull;
  }

  bool Equals(const Nullable<T>& aOtherNullable) const
  {
    return (mIsNull && aOtherNullable.mIsNull) ||
           (!mIsNull && !aOtherNullable.mIsNull &&
            mValue == aOtherNullable.mValue);
  }

  bool operator==(const Nullable<T>& aOtherNullable) const
  {
    return Equals(aOtherNullable);
  }

  bool operator!=(const Nullable<T>& aOtherNullable) const
  {
    return !Equals(aOtherNullable);
  }

  // Make it possible to use a const Nullable of an array type with other
  // array types.
  template<typename U>
  operator const Nullable< nsTArray<U> >&() const {
    // Make sure that T is ok to reinterpret to nsTArray<U>
    const nsTArray<U>& arr = mValue;
    (void)arr;
    return *reinterpret_cast<const Nullable< nsTArray<U> >*>(this);
  }
  template<typename U>
  operator const Nullable< FallibleTArray<U> >&() const {
    // Make sure that T is ok to reinterpret to FallibleTArray<U>
    const FallibleTArray<U>& arr = mValue;
    (void)arr;
    return *reinterpret_cast<const Nullable< FallibleTArray<U> >*>(this);
  }
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_Nullable_h */
