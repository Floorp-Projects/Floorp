/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/IntegerRange.h"
#include "js/Vector.h"
#include "jsapi-tests/tests.h"
#include "threading/ExclusiveData.h"
#include "threading/Thread.h"

// One thread for each bit in our counter.
const static uint8_t NumThreads = 64;
const static bool ShowDiagnostics = false;

struct CounterAndBit {
  uint8_t bit;
  const js::ExclusiveData<uint64_t>& counter;

  CounterAndBit(uint8_t bit, const js::ExclusiveData<uint64_t>& counter)
      : bit(bit), counter(counter) {
    MOZ_ASSERT(bit < NumThreads);
  }
};

void printDiagnosticMessage(uint8_t bit, uint64_t seen) {
  if (!ShowDiagnostics) {
    return;
  }

  fprintf(stderr, "Thread %d saw ", bit);
  for (auto i : mozilla::IntegerRange(NumThreads)) {
    if (seen & (uint64_t(1) << i)) {
      fprintf(stderr, "1");
    } else {
      fprintf(stderr, "0");
    }
  }
  fprintf(stderr, "\n");
}

void setBitAndCheck(CounterAndBit* counterAndBit) {
  while (true) {
    {
      // Set our bit. Repeatedly setting it is idempotent.
      auto guard = counterAndBit->counter.lock();
      printDiagnosticMessage(counterAndBit->bit, guard);
      guard |= (uint64_t(1) << counterAndBit->bit);
    }

    {
      // Check to see if we have observed all the other threads setting
      // their bit as well.
      auto guard = counterAndBit->counter.lock();
      printDiagnosticMessage(counterAndBit->bit, guard);
      if (guard == UINT64_MAX) {
        js_delete(counterAndBit);
        return;
      }
    }
  }
}

BEGIN_TEST(testExclusiveData) {
  js::ExclusiveData<uint64_t> counter(js::mutexid::TestMutex, 0);

  js::Vector<js::Thread> threads(cx);
  CHECK(threads.reserve(NumThreads));

  for (auto i : mozilla::IntegerRange(NumThreads)) {
    auto counterAndBit = js_new<CounterAndBit>(i, counter);
    CHECK(counterAndBit);
    CHECK(threads.emplaceBack());
    CHECK(threads.back().init(setBitAndCheck, counterAndBit));
  }

  for (auto& thread : threads) {
    thread.join();
  }

  return true;
}
END_TEST(testExclusiveData)
