/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_arraycluster_h__
#define mozilla_dom_quota_arraycluster_h__

#include "mozilla/dom/quota/QuotaCommon.h"

#include "Client.h"

BEGIN_QUOTA_NAMESPACE

template <class ValueType, uint32_t Length = Client::TYPE_MAX>
class ArrayCluster
{
public:
  ArrayCluster()
  {
    mArrays.AppendElements(Length);
  }

  nsTArray<ValueType>&
  ArrayAt(uint32_t aIndex)
  {
    MOZ_ASSERT(aIndex < Length, "Bad index!");
    return mArrays[aIndex];
  }

  nsTArray<ValueType>&
  operator[](uint32_t aIndex)
  {
    return ArrayAt(aIndex);
  }

  bool
  IsEmpty()
  {
    for (uint32_t index = 0; index < Length; index++) {
      if (!mArrays[index].IsEmpty()) {
        return false;
      }
    }
    return true;
  }

  template <class T>
  void
  AppendElementsTo(uint32_t aIndex, nsTArray<T>& aArray)
  {
    NS_ASSERTION(aIndex < Length, "Bad index!");
    aArray.AppendElements(mArrays[aIndex]);
  }

  template <class T>
  void
  AppendElementsTo(uint32_t aIndex, ArrayCluster<T, Length>& aArrayCluster)
  {
    NS_ASSERTION(aIndex < Length, "Bad index!");
    aArrayCluster[aIndex].AppendElements(mArrays[aIndex]);
  }

  template <class T>
  void
  AppendElementsTo(nsTArray<T>& aArray)
  {
    for (uint32_t index = 0; index < Length; index++) {
      aArray.AppendElements(mArrays[index]);
    }
  }

  template<class T>
  void
  AppendElementsTo(ArrayCluster<T, Length>& aArrayCluster)
  {
    for (uint32_t index = 0; index < Length; index++) {
      aArrayCluster[index].AppendElements(mArrays[index]);
    }
  }

  template<class T>
  void
  SwapElements(ArrayCluster<T, Length>& aArrayCluster)
  {
    for (uint32_t index = 0; index < Length; index++) {
      mArrays[index].SwapElements(aArrayCluster.mArrays[index]);
    }
  }

  void
  Clear()
  {
    for (uint32_t index = 0; index < Length; index++) {
      mArrays[index].Clear();
    }
  }

private:
  nsAutoTArray<nsTArray<ValueType>, Length> mArrays;
};

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_arraycluster_h__
