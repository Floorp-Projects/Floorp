/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_SavedTypes_h
#define mozilla_dom_cache_SavedTypes_h

// NOTE: This cannot be rolled into Types.h because the IPC dependency.
//       breaks webidl unified builds.

#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/dom/cache/Types.h"
#include "nsCOMPtr.h"
#include "nsID.h"
#include "nsIOutputStream.h"

namespace mozilla {
namespace dom {
namespace cache {

struct SavedRequest
{
  SavedRequest()
    : mHasBodyId(false)
    , mCacheId(0)
  {
    mBodyId.m0 = 0;
    mBodyId.m1 = 0;
    mBodyId.m2 = 0;
    memset(mBodyId.m3, 0, sizeof(mBodyId.m3));
    mValue.body() = void_t();
  }
  CacheRequest mValue;
  bool mHasBodyId;
  nsID mBodyId;
  CacheId mCacheId;
};

struct SavedResponse
{
  SavedResponse()
    : mHasBodyId(false)
    , mCacheId(0)
  {
    mBodyId.m0 = 0;
    mBodyId.m1 = 0;
    mBodyId.m2 = 0;
    memset(mBodyId.m3, 0, sizeof(mBodyId.m3));
    mValue.body() = void_t();
  }
  CacheResponse mValue;
  bool mHasBodyId;
  nsID mBodyId;
  CacheId mCacheId;
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_SavedTypes_h
