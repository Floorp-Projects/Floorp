/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStyleAutoArray_h_
#define nsStyleAutoArray_h_

#include "nsTArray.h"
#include "mozilla/Assertions.h"

/**
 * An array of objects, similar to AutoTArray<T,1> but which is memmovable. It
 * always has length >= 1.
 */
template <typename T>
class nsStyleAutoArray {
 public:
  // This constructor places a single element in mFirstElement.
  enum WithSingleInitialElement { WITH_SINGLE_INITIAL_ELEMENT };
  explicit nsStyleAutoArray(WithSingleInitialElement) {}
  nsStyleAutoArray(const nsStyleAutoArray& aOther) { *this = aOther; }
  nsStyleAutoArray& operator=(const nsStyleAutoArray& aOther) {
    mFirstElement = aOther.mFirstElement;
    mOtherElements = aOther.mOtherElements.Clone();
    return *this;
  }

  bool operator==(const nsStyleAutoArray& aOther) const {
    return Length() == aOther.Length() &&
           mFirstElement == aOther.mFirstElement &&
           mOtherElements == aOther.mOtherElements;
  }
  bool operator!=(const nsStyleAutoArray& aOther) const {
    return !(*this == aOther);
  }

  nsStyleAutoArray& operator=(nsStyleAutoArray&& aOther) {
    mFirstElement = aOther.mFirstElement;
    mOtherElements.SwapElements(aOther.mOtherElements);

    return *this;
  }

  size_t Length() const { return mOtherElements.Length() + 1; }
  const T& operator[](size_t aIndex) const {
    return aIndex == 0 ? mFirstElement : mOtherElements[aIndex - 1];
  }
  T& operator[](size_t aIndex) {
    return aIndex == 0 ? mFirstElement : mOtherElements[aIndex - 1];
  }

  void EnsureLengthAtLeast(size_t aMinLen) {
    if (aMinLen > 0) {
      mOtherElements.EnsureLengthAtLeast(aMinLen - 1);
    }
  }

  void SetLengthNonZero(size_t aNewLen) {
    MOZ_ASSERT(aNewLen > 0);
    mOtherElements.SetLength(aNewLen - 1);
  }

  void TruncateLengthNonZero(size_t aNewLen) {
    MOZ_ASSERT(aNewLen > 0);
    MOZ_ASSERT(aNewLen <= Length());
    mOtherElements.TruncateLength(aNewLen - 1);
  }

 private:
  T mFirstElement;
  nsTArray<T> mOtherElements;
};

#endif /* nsStyleAutoArray_h_ */
