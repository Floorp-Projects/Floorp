/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BenchmarkStorageParent.h"
#include "KeyValueStorage.h"

namespace mozilla {

/* Moving average window size. */
const int32_t AVG_WINDOW = 20;
/* Calculate the moving average for the new value aValue for the current window
 * aWindow and the already existing average aAverage. When the method returns
 * aAverage will contain the new average and aWindow will contain the new
 * window.*/
void BenchmarkStorageParent::MovingAverage(int32_t& aAverage, int32_t& aWindow,
                                           const int32_t aValue) {
  if (aWindow < AVG_WINDOW) {
    aAverage = (aAverage * aWindow + aValue) / (aWindow + 1);
    aWindow++;
    return;
  }
  MOZ_ASSERT(aWindow == AVG_WINDOW);
  aAverage = (aAverage - aAverage / aWindow) + (aValue / aWindow);
}

/* In order to decrease the number of times the database is accessed when the
 * moving average is stored or retrieved we use the same value to store _both_
 * the window and the average. The range of the average is limited since it is
 * a percentage (0-100), and the range of the window is limited
 * (1-20). Thus the number that is stored in the database is in the form
 * (digits): WWAAA. For example, the value stored when an average(A) of 88
 * corresponds to a window(W) 7 is 7088. The average of 100 that corresponds to
 * a window of 20 is 20100. The following methods are helpers to extract or
 * construct the stored value according to the above. */

/* Stored value will be in the form WWAAA(19098). We need to extract the window
 * (19) and the average score (98). The aValue will
 * be parsed, the aWindow will contain the window (of the moving average) and
 * the return value will contain the average itself. */
int32_t BenchmarkStorageParent::ParseStoredValue(int32_t aValue,
                                                 int32_t& aWindow) {
  MOZ_ASSERT(aValue > 999);
  MOZ_ASSERT(aValue < 100000);

  int32_t score = aValue % 1000;
  aWindow = (aValue / 1000) % 100;
  return score;
}

int32_t BenchmarkStorageParent::PrepareStoredValue(int32_t aScore,
                                                   int32_t aWindow) {
  MOZ_ASSERT(aScore >= 0);
  MOZ_ASSERT(aScore <= 100);
  MOZ_ASSERT(aWindow > 0);
  MOZ_ASSERT(aWindow < 21);

  return aWindow * 1000 + aScore;
}

BenchmarkStorageParent::BenchmarkStorageParent()
    : mStorage(new KeyValueStorage) {}

IPCResult BenchmarkStorageParent::RecvPut(const nsCString& aDbName,
                                          const nsCString& aKey,
                                          const int32_t& aValue) {
  // In order to calculate and store the new moving average, we need to get the
  // stored value and window first, to calculate the new score and window, and
  // then to store the new aggregated value.
  mStorage->Get(aDbName, aKey)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [storage = mStorage, aDbName, aKey, aValue](int32_t aResult) {
            int32_t window = 0;
            int32_t average = 0;
            if (aResult >= 0) {
              // The key found.
              average = ParseStoredValue(aResult, window);
            }
            MovingAverage(average, window, aValue);
            int32_t newValue = PrepareStoredValue(average, window);
            storage->Put(aDbName, aKey, newValue);
          },
          [](nsresult rv) { /*do nothing*/ });

  return IPC_OK();
}

IPCResult BenchmarkStorageParent::RecvGet(const nsCString& aDbName,
                                          const nsCString& aKey,
                                          GetResolver&& aResolve) {
  mStorage->Get(aDbName, aKey)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [aResolve](int32_t aResult) {
            int32_t window = 0;  // not used
            aResolve(aResult < 0 ? -1 : ParseStoredValue(aResult, window));
          },
          [aResolve](nsresult rv) { aResolve(-1); });

  return IPC_OK();
}

IPCResult BenchmarkStorageParent::RecvCheckVersion(const nsCString& aDbName,
                                                   int32_t aVersion) {
  mStorage->Get(aDbName, NS_LITERAL_CSTRING("Version"))
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [storage = mStorage, aDbName, aVersion](int32_t aResult) {
            if (aVersion != aResult) {
              storage->Clear(aDbName)->Then(
                  GetCurrentThreadSerialEventTarget(), __func__,
                  [storage, aDbName, aVersion](bool) {
                    storage->Put(aDbName, NS_LITERAL_CSTRING("Version"),
                                 aVersion);
                  },
                  [](nsresult rv) { /*do nothing*/ });
            }
          },
          [](nsresult rv) { /*do nothing*/ });

  return IPC_OK();
}

};  // namespace mozilla
