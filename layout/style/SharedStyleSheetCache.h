/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SharedStyleSheetCache_h__
#define mozilla_SharedStyleSheetCache_h__

// The shared style sheet cache is a cache that allows us to share sheets across
// documents.
//
// It's generally a singleton, but it is different from GlobalStyleSheetCache in
// the sense that:
//
//  * It needs to be cycle-collectable, as it can keep alive style sheets from
//    various documents.
//
//  * It is conceptually a singleton, but given its cycle-collectable nature, we
//    might re-create it.

#include "mozilla/SharedSubResourceCache.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/css/Loader.h"

namespace mozilla {

class StyleSheet;
class SheetLoadDataHashKey;

namespace css {
class SheetLoadData;
class Loader;
}  // namespace css

struct SharedStyleSheetCacheTraits {
  using Loader = css::Loader;
  using Key = SheetLoadDataHashKey;
  using Value = StyleSheet;
  using LoadingValue = css::SheetLoadData;

  static SheetLoadDataHashKey KeyFromLoadingValue(const LoadingValue& aValue) {
    return SheetLoadDataHashKey(aValue);
  }
};

class SharedStyleSheetCache final
    : public SharedSubResourceCache<SharedStyleSheetCacheTraits,
                                    SharedStyleSheetCache>,
      public nsIMemoryReporter {
 public:
  using Base = SharedSubResourceCache<SharedStyleSheetCacheTraits,
                                      SharedStyleSheetCache>;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  SharedStyleSheetCache();
  void Init();

  // This has to be static because it's also called for loaders that don't have
  // a sheet cache (loaders that are not owned by a document).
  static void LoadCompleted(SharedStyleSheetCache*, css::SheetLoadData&,
                            nsresult);
  using Base::LoadCompleted;
  static void LoadCompletedInternal(SharedStyleSheetCache*, css::SheetLoadData&,
                                    nsTArray<RefPtr<css::SheetLoadData>>&);
  static void Clear(nsIPrincipal* aForPrincipal = nullptr,
                    const nsACString* aBaseDomain = nullptr);

 protected:
  void InsertIfNeeded(css::SheetLoadData&);

  ~SharedStyleSheetCache();
};

}  // namespace mozilla

#endif
