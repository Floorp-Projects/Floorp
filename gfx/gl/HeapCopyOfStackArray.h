/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HEAPCOPYOFSTACKARRAY_H_
#define HEAPCOPYOFSTACKARRAY_H_

#include "mozilla/Attributes.h"
#include "mozilla/Scoped.h"

#include <string.h>

namespace mozilla {

// Takes a stack array and copies it into a heap buffer.
// Useful to retain the convenience of declaring static arrays, while
// avoiding passing stack pointers to the GL (see bug 1005658).

template <typename ElemType>
class HeapCopyOfStackArray
{
public:
  template<size_t N>
  HeapCopyOfStackArray(ElemType (&array)[N])
    : mArrayLength(N)
    , mArrayData(new ElemType[N])
  {
    memcpy(mArrayData, &array[0], N * sizeof(ElemType));
  }

  ElemType* Data() const { return mArrayData; }
  size_t ArrayLength() const { return mArrayLength; }
  size_t ByteLength() const { return mArrayLength * sizeof(ElemType); }

private:
  HeapCopyOfStackArray() MOZ_DELETE;
  HeapCopyOfStackArray(const HeapCopyOfStackArray&) MOZ_DELETE;

  const size_t mArrayLength;
  ScopedDeletePtr<ElemType> const mArrayData;
};

}

#endif // HEAPCOPYOFSTACKARRAY_H_