/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NS_FONTCACHE_H_
#define _NS_FONTCACHE_H_

#include <stdint.h>
#include <sys/types.h>
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsFontMetrics.h"
#include "nsIObserver.h"
#include "nsISupports.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

class gfxUserFontSet;
class nsAtom;
class nsPresContext;
struct nsFont;

class nsFontCache final : public nsIObserver {
 public:
  nsFontCache() : mContext(nullptr) {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  void Init(nsPresContext* aContext);
  void Destroy();

  already_AddRefed<nsFontMetrics> GetMetricsFor(
      const nsFont& aFont, const nsFontMetrics::Params& aParams);

  void FontMetricsDeleted(const nsFontMetrics* aFontMetrics);
  void Compact();

  // Flush aFlushCount oldest entries, or all if aFlushCount is negative
  void Flush(int32_t aFlushCount = -1);

  void UpdateUserFonts(gfxUserFontSet* aUserFontSet);

 protected:
  // If the array of cached entries is about to exceed this threshold,
  // we'll discard the oldest ones so as to keep the size reasonable.
  // In practice, the great majority of cache hits are among the last
  // few entries; keeping thousands of older entries becomes counter-
  // productive because it can then take too long to scan the cache.
  static const int32_t kMaxCacheEntries = 128;

  ~nsFontCache() = default;

  nsPresContext* mContext;  // owner
  RefPtr<nsAtom> mLocaleLanguage;

  // We may not flush older entries immediately the array reaches
  // kMaxCacheEntries length, because this usually happens on a stylo
  // thread where we can't safely delete metrics objects. So we allocate an
  // oversized autoarray buffer here, so that we're unlikely to overflow
  // it and need separate heap allocation before the flush happens on the
  // main thread.
  AutoTArray<nsFontMetrics*, kMaxCacheEntries * 2> mFontMetrics;

  bool mFlushPending = false;

  class FlushFontMetricsTask : public mozilla::Runnable {
   public:
    explicit FlushFontMetricsTask(nsFontCache* aCache)
        : mozilla::Runnable("FlushFontMetricsTask"), mCache(aCache) {}
    NS_IMETHOD Run() override {
      // Partially flush the cache, leaving the kMaxCacheEntries/2 most
      // recent entries.
      mCache->Flush(mCache->mFontMetrics.Length() - kMaxCacheEntries / 2);
      mCache->mFlushPending = false;
      return NS_OK;
    }

   private:
    RefPtr<nsFontCache> mCache;
  };
};

#endif /* _NS_FONTCACHE_H_ */
