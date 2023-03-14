/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_QueueWithSizes_h
#define mozilla_dom_QueueWithSizes_h

#include <cmath>
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

namespace mozilla::dom {

// Note: Consumers of QueueWithSize need to ensure it is traced. See
// ReadableStreamDefaultController.cpp for an example.

struct ValueWithSize : LinkedListElement<ValueWithSize> {
  ValueWithSize(JS::Handle<JS::Value> aValue, double aSize)
      : mValue(aValue), mSize(aSize){};

  JS::Heap<JS::Value> mValue;
  double mSize = 0.0f;
};

// This type is a little tricky lifetime wise: Despite the fact that we're
// talking about linked list of VaueWithSize, what is actually stored in the
// list are dynamically allocated ValueWithSize*. As a result, these have to be
// deleted. There are two ways this lifetime is managed:
//
// 1. In DequeueValue we pop the first element into a UniquePtr, so that it is
//    correctly cleaned up at destruction time.
// 2. We use an AutoCleanLinkedList which will delete elements when destroyed
//    or `clear`ed.
using QueueWithSizes = AutoCleanLinkedList<ValueWithSize>;

// https://streams.spec.whatwg.org/#is-non-negative-number
inline bool IsNonNegativeNumber(double v) {
  // Step 1. Implicit.
  // Step 2.
  if (std::isnan(v)) {
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
  // Step 1. Assert: container has [[queue]] and [[queueTotalSize]] internal
  // slots. (Implicit by template instantiation.)
  // Step 2. If ! IsNonNegativeNumber(size) is false, throw a RangeError
  // exception.
  if (!IsNonNegativeNumber(aSize)) {
    aRv.ThrowRangeError("invalid size");
    return;
  }

  // Step 3. If size is +∞, throw a RangeError exception.
  if (std::isinf(aSize)) {
    aRv.ThrowRangeError("Infinite queue size");
    return;
  }

  // Step 4. Append a new value-with-size with value value and size size to
  // container.[[queue]].
  // (See the comment on QueueWithSizes for the lifetime reasoning around this
  // allocation.)
  ValueWithSize* valueWithSize = new ValueWithSize(aValue, aSize);
  aContainer->Queue().insertBack(valueWithSize);
  // Step 5. Set container.[[queueTotalSize]] to container.[[queueTotalSize]] +
  // size.
  aContainer->SetQueueTotalSize(aContainer->QueueTotalSize() + aSize);
}

// https://streams.spec.whatwg.org/#dequeue-value
template <class QueueContainingClass>
inline void DequeueValue(QueueContainingClass aContainer,
                         JS::MutableHandle<JS::Value> aResultValue) {
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

// https://streams.spec.whatwg.org/#peek-queue-value
template <class QueueContainingClass>
inline void PeekQueueValue(QueueContainingClass aContainer,
                           JS::MutableHandle<JS::Value> aResultValue) {
  // Step 1. Assert: container has [[queue]] and [[queueTotalSize]] internal
  // slots.
  // Step 2. Assert: container.[[queue]] is not empty.
  MOZ_ASSERT(!aContainer->Queue().isEmpty());

  // Step 3. Let valueWithSize be container.[[queue]][0].
  ValueWithSize* valueWithSize = aContainer->Queue().getFirst();

  // Step 4. Return valueWithSize’s value.
  aResultValue.set(valueWithSize->mValue);
}

// https://streams.spec.whatwg.org/#reset-queue
template <class QueueContainingClass>
inline void ResetQueue(QueueContainingClass aContainer) {
  // Step 1. Assert: container has [[queue]] and [[queueTotalSize]] internal
  // slots. (implicit)

  // Step 2. Set container.[[queue]] to a new empty list.
  aContainer->Queue().clear();

  // Step 3. Set container.[[queueTotalSize]] to 0.
  aContainer->SetQueueTotalSize(0.0);
}

}  // namespace mozilla::dom

#endif
