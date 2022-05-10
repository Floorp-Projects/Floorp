/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_LSWriteOptimizerImpl_h
#define mozilla_dom_localstorage_LSWriteOptimizerImpl_h

#include "LSWriteOptimizer.h"

namespace mozilla::dom {

template <typename T, typename U>
void LSWriteOptimizer<T, U>::InsertItem(const nsAString& aKey, const T& aValue,
                                        int64_t aDelta) {
  AssertIsOnOwningThread();

  mWriteInfos.WithEntryHandle(aKey, [&](auto&& entry) {
    if (entry && entry.Data()->GetType() == WriteInfo::DeleteItem) {
      // We could just simply replace the deletion with ordinary update, but
      // that would preserve item's original position/index. Imagine a case when
      // we have only one existing key k1. Now let's create a new optimizer and
      // remove k1, add k2 and add k1 back. The final order should be k2, k1
      // (ordinary update would produce k1, k2). So we need to differentiate
      // between normal update and "optimized" update which resulted from a
      // deletion followed by an insertion. We use the UpdateWithMove flag for
      // this.

      entry.Update(MakeUnique<UpdateItemInfo>(NextSerialNumber(), aKey, aValue,
                                              /* aUpdateWithMove */ true));
    } else {
      entry.InsertOrUpdate(
          MakeUnique<InsertItemInfo>(NextSerialNumber(), aKey, aValue));
    }
  });

  mTotalDelta += aDelta;
}

template <typename T, typename U>
void LSWriteOptimizer<T, U>::UpdateItem(const nsAString& aKey, const T& aValue,
                                        int64_t aDelta) {
  AssertIsOnOwningThread();

  mWriteInfos.WithEntryHandle(aKey, [&](auto&& entry) {
    if (entry && entry.Data()->GetType() == WriteInfo::InsertItem) {
      entry.Update(
          MakeUnique<InsertItemInfo>(NextSerialNumber(), aKey, aValue));
    } else {
      entry.InsertOrUpdate(
          MakeUnique<UpdateItemInfo>(NextSerialNumber(), aKey, aValue,
                                     /* aUpdateWithMove */ false));
    }
  });

  mTotalDelta += aDelta;
}

}  // namespace mozilla::dom

#endif  // mozilla_dom_localstorage_LSWriteOptimizerImpl_h
