// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ATOMIC_SEQUENCE_NUM_H_
#define BASE_ATOMIC_SEQUENCE_NUM_H_

#include "base/atomicops.h"
#include "base/basictypes.h"

namespace base {

class AtomicSequenceNumber {
 public:
  AtomicSequenceNumber() : seq_(0) { }
  explicit AtomicSequenceNumber(base::LinkerInitialized x) { /* seq_ is 0 */ }

  int GetNext() {
    return static_cast<int>(
        base::subtle::NoBarrier_AtomicIncrement(&seq_, 1) - 1);
  }

 private:
  base::subtle::Atomic32 seq_;
  DISALLOW_COPY_AND_ASSIGN(AtomicSequenceNumber);
};

}  // namespace base

#endif  // BASE_ATOMIC_SEQUENCE_NUM_H_
