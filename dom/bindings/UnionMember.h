/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A class for holding the members of a union. */

#ifndef mozilla_dom_UnionMember_h
#define mozilla_dom_UnionMember_h

#include "mozilla/Alignment.h"

namespace mozilla {
namespace dom {

// The union type has an enum to keep track of which of its UnionMembers has
// been constructed.
template<class T>
class UnionMember
{
  AlignedStorage2<T> mStorage;

public:
  T& SetValue()
  {
    new (mStorage.addr()) T();
    return *mStorage.addr();
  }
  template <typename T1>
  T& SetValue(const T1& aValue)
  {
    new (mStorage.addr()) T(aValue);
    return *mStorage.addr();
  }
  template<typename T1, typename T2>
  T& SetValue(const T1& aValue1, const T2& aValue2)
  {
    new (mStorage.addr()) T(aValue1, aValue2);
    return *mStorage.addr();
  }
  T& Value()
  {
    return *mStorage.addr();
  }
  const T& Value() const
  {
    return *mStorage.addr();
  }
  void Destroy()
  {
    mStorage.addr()->~T();
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_UnionMember_h
