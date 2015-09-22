/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Nullable_h
#define mozilla_dom_Nullable_h

#include "mozilla/Assertions.h"
#include "nsTArrayForwardDeclare.h"
#include "mozilla/Move.h"
#include "mozilla/Maybe.h"

class nsCycleCollectionTraversalCallback;

namespace mozilla {
namespace dom {

// Support for nullable types
template <typename T>
struct Nullable
{
private:
  Maybe<T> mValue;

public:
  Nullable()
    : mValue()
  {}

  explicit Nullable(const T& aValue)
    : mValue()
  {
    mValue.emplace(aValue);
  }

  explicit Nullable(T&& aValue)
    : mValue()
  {
    mValue.emplace(mozilla::Move(aValue));
  }

  Nullable(Nullable<T>&& aOther)
    : mValue(mozilla::Move(aOther.mValue))
  {}

  Nullable(const Nullable<T>& aOther)
    : mValue(aOther.mValue)
  {}

  void operator=(const Nullable<T>& aOther)
  {
    mValue = aOther.mValue;
  }

  void SetValue(const T& aArgs)
  {
    mValue.reset();
    mValue.emplace(aArgs);
  }

  void SetValue(T&& aArgs)
  {
    mValue.reset();
    mValue.emplace(mozilla::Move(aArgs));
  }

  // For cases when |T| is some type with nontrivial copy behavior, we may want
  // to get a reference to our internal copy of T and work with it directly
  // instead of relying on the copying version of SetValue().
  T& SetValue() {
    if (mValue.isNothing()) {
      mValue.emplace();
    }
    return mValue.ref();
  }

  void SetNull() {
    mValue.reset();
  }

  const T& Value() const {
    return mValue.ref();
  }

  T& Value() {
    return mValue.ref();
  }

  bool IsNull() const {
    return mValue.isNothing();
  }

  bool Equals(const Nullable<T>& aOtherNullable) const
  {
    return mValue == aOtherNullable.mValue;
  }

  bool operator==(const Nullable<T>& aOtherNullable) const
  {
    return Equals(aOtherNullable);
  }

  bool operator!=(const Nullable<T>& aOtherNullable) const
  {
    return !Equals(aOtherNullable);
  }
};


template<typename T>
void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            Nullable<T>& aNullable,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  if (!aNullable.IsNull()) {
    ImplCycleCollectionTraverse(aCallback, aNullable.Value(), aName, aFlags);
  }
}

template<typename T>
void
ImplCycleCollectionUnlink(Nullable<T>& aNullable)
{
  if (!aNullable.IsNull()) {
    ImplCycleCollectionUnlink(aNullable.Value());
  }
}

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_Nullable_h */
