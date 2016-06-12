/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BorderCache_h_
#define mozilla_BorderCache_h_

#include "mozilla/gfx/2D.h"
#include "mozilla/HashFunctions.h"
#include "nsDataHashtable.h"
#include "PLDHashTable.h"

namespace mozilla {
// Cache for best overlap and best dashLength.

struct FourFloats
{
  typedef mozilla::gfx::Float Float;

  Float n[4];

  FourFloats()
  {
    n[0] = 0.0f;
    n[1] = 0.0f;
    n[2] = 0.0f;
    n[3] = 0.0f;
  }

  FourFloats(Float a, Float b, Float c, Float d)
  {
    n[0] = a;
    n[1] = b;
    n[2] = c;
    n[3] = d;
  }

  bool
  operator==(const FourFloats& aOther) const
  {
    return n[0] == aOther.n[0] &&
           n[1] == aOther.n[1] &&
           n[2] == aOther.n[2] &&
           n[3] == aOther.n[3];
  }
};

class FourFloatsHashKey : public PLDHashEntryHdr
{
public:
  typedef const FourFloats& KeyType;
  typedef const FourFloats* KeyTypePointer;

  explicit FourFloatsHashKey(KeyTypePointer aKey) : mValue(*aKey) {}
  FourFloatsHashKey(const FourFloatsHashKey& aToCopy) : mValue(aToCopy.mValue) {}
  ~FourFloatsHashKey() {}

  KeyType GetKey() const { return mValue; }
  bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mValue; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return HashBytes(aKey->n, sizeof(mozilla::gfx::Float) * 4);
  }
  enum { ALLOW_MEMMOVE = true };

private:
  const FourFloats mValue;
};

} // namespace mozilla

#endif /* mozilla_BorderCache_h_ */
