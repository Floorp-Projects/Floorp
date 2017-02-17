/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_Aggregation_h
#define mozilla_mscom_Aggregation_h

#include "mozilla/Attributes.h"

namespace mozilla {
namespace mscom {

/**
 * This is used for stabilizing a COM object's reference count during
 * construction when that object aggregates other objects. Since the aggregated
 * object(s) may AddRef() or Release(), we need to artifically boost the
 * refcount to prevent premature destruction. Note that we increment/decrement
 * instead of AddRef()/Release() in this class because we want to adjust the
 * refcount without causing any other side effects (like object destruction).
 */
template <typename RefCntT>
class MOZ_RAII StabilizedRefCount
{
public:
  explicit StabilizedRefCount(RefCntT& aRefCnt)
    : mRefCnt(aRefCnt)
  {
    ++aRefCnt;
  }

  ~StabilizedRefCount()
  {
    --mRefCnt;
  }

  StabilizedRefCount(const StabilizedRefCount&) = delete;
  StabilizedRefCount(StabilizedRefCount&&) = delete;
  StabilizedRefCount& operator=(const StabilizedRefCount&) = delete;
  StabilizedRefCount& operator=(StabilizedRefCount&&) = delete;

private:
  RefCntT& mRefCnt;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_Aggregation_h

