/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_QueueWithSizes_h
#define mozilla_dom_QueueWithSizes_h

#include "js/TypeDecls.h"
#include "js/Value.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

namespace mozilla::dom {

// Note: Consumers of QueueWithSize need to ensure it is traced. See
// ReadableStreamDefaultController.cpp for an example.

struct ValueWithSize : LinkedListElement<ValueWithSize> {
  ValueWithSize(JS::HandleValue aValue, double aSize)
      : mValue(aValue), mSize(aSize){};

  JS::Heap<JS::Value> mValue;
  double mSize = 0.0f;
};

using QueueWithSizes = LinkedList<ValueWithSize>;

// https://streams.spec.whatwg.org/#is-non-negative-number
inline bool IsNonNegativeNumber(double v) {
  // Step 1. Implicit.
  // Step 2.
  if (mozilla::IsNaN(v)) {
    return false;
  }

  // Step 3.
  return !(v < 0);
}

// https://streams.spec.whatwg.org/#enqueue-value-with-size
template <class QueueContainingClass>
inline void EnqueueValueWithSize(QueueContainingClass aContainer,
                                 JS::Handle<JS::Value> aValue, double aSize,
                                 ErrorResult& aRv) {
  // Step 1. Implicit by template instantiation.
  // Step 2.
  if (!IsNonNegativeNumber(aSize)) {
    aRv.ThrowRangeError("invalid size");
    return;
  }

  // Step 3.
  if (mozilla::IsInfinite(aSize)) {
    aRv.ThrowRangeError("Infinite queue size");
    return;
  }

  // Step 4.
  ValueWithSize* valueWithSize = new ValueWithSize(aValue, aSize);
  aContainer->Queue().insertBack(valueWithSize);
  // Step 5.
  aContainer->SetQueueTotalSize(aContainer->QueueTotalSize() + aSize);
}

// https://streams.spec.whatwg.org/#dequeue-value
template <class QueueContainingClass>
inline void DequeueValue(QueueContainingClass aContainer,
                         JS::MutableHandleValue aResultValue) {
  // Step 1. Implicit via template instantiation.
  // Step 2.
  MOZ_ASSERT(!aContainer->Queue().isEmpty());

  // Step 3+4
  // UniquePtr to ensure memory is freed.
  UniquePtr<ValueWithSize> valueWithSize(aContainer->Queue().popFirst());

  // Step 5.
  aContainer->SetQueueTotalSize(aContainer->QueueTotalSize() -
                                valueWithSize->mSize);

  // Step 6.
  if (aContainer->QueueTotalSize() < 0) {
    aContainer->SetQueueTotalSize(0);
  }

  // Step 7.
  aResultValue.set(valueWithSize->mValue);
}

}  // namespace mozilla::dom

#endif
