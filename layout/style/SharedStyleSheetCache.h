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

#include "mozilla/PrincipalHashKey.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/css/Loader.h"
#include "nsTHashMap.h"
#include "nsIMemoryReporter.h"
#include "nsRefPtrHashtable.h"
#include "nsURIHashKey.h"

namespace mozilla {

namespace css {
class SheetLoadData;
}

class SharedStyleSheetCache final : public nsIMemoryReporter {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  SharedStyleSheetCache(const SharedStyleSheetCache&) = delete;
  SharedStyleSheetCache(SharedStyleSheetCache&&) = delete;

  static already_AddRefed<SharedStyleSheetCache> Get() {
    if (sInstance) {
      return do_AddRef(sInstance);
    }
    return Create();
  }

 public:
  // TODO(emilio): Maybe cache inline sheets here rather than in the loader?
  // But that is a bit dubious, because for inline sheets, the sheet URI is the
  // document URI, so it is basically document-specific... Maybe we can cache
  // the RawServoStyleSheetContents or something, but...

  // A cache hit or miss. It is a miss if the `StyleSheet` is null.
  using CacheResult = std::tuple<RefPtr<StyleSheet>, css::Loader::SheetState>;
  CacheResult Lookup(css::Loader&, const SheetLoadDataHashKey&, bool aSyncLoad);

  // Tries to coalesce with an already existing load. The sheet state must be
  // the one that Lookup returned, if it returned a sheet.
  //
  // TODO(emilio): Maybe try to merge this with the lookup? Most consumers could
  // have a data there already.
  MOZ_MUST_USE bool CoalesceLoad(const SheetLoadDataHashKey&,
                                 css::SheetLoadData& aNewLoad,
                                 css::Loader::SheetState aExistingLoadState);

  size_t SizeOfIncludingThis(MallocSizeOf) const;

  // Puts the load into the "loading" set.
  void LoadStarted(const SheetLoadDataHashKey&, css::SheetLoadData&);
  // Puts a load into the "pending" set.
  void DeferSheetLoad(const SheetLoadDataHashKey&, css::SheetLoadData&);

  enum class StartLoads {
    Always,
    IfNonAlternate,
  };

  void StartDeferredLoadsForLoader(css::Loader&, StartLoads);
  void CancelLoadsForLoader(css::Loader&);

  // This has to be static because it's also called for loaders that don't have
  // a sheet cache (loaders that are not owned by a document).
  static void LoadCompleted(SharedStyleSheetCache*, css::SheetLoadData&,
                            nsresult);

  // Register a loader into the cache. This has the effect of keeping alive all
  // stylesheets for the origin of the loader's document until UnregisterLoader
  // is called.
  void RegisterLoader(css::Loader&);

  // Unregister a loader from the cache.
  //
  // If this is the loader for the last document of a given origin, then all the
  // stylesheets for that document will be removed from the cache. This needs to
  // be called when the document goes away, or when its principal changes.
  void UnregisterLoader(css::Loader&);

  static void Clear(nsIPrincipal* aForPrincipal = nullptr);

 private:
  static already_AddRefed<SharedStyleSheetCache> Create();
  void CancelDeferredLoadsForLoader(css::Loader&);

  static void LoadCompletedInternal(SharedStyleSheetCache*, css::SheetLoadData&,
                                    nsTArray<RefPtr<css::SheetLoadData>>&);
  void InsertIntoCompleteCacheIfNeeded(css::SheetLoadData&);

  SharedStyleSheetCache() = default;

  ~SharedStyleSheetCache();

  struct CompleteSheet {
    uint32_t mExpirationTime = 0;
    UniquePtr<StyleUseCounters> mUseCounters;
    RefPtr<StyleSheet> mSheet;

    bool Expired() const;
  };

  void WillStartPendingLoad(css::SheetLoadData&);

  nsTHashMap<SheetLoadDataHashKey, CompleteSheet> mCompleteSheets;
  nsRefPtrHashtable<SheetLoadDataHashKey, css::SheetLoadData> mPendingDatas;
  // The SheetLoadData pointers in mLoadingDatas below are weak references that
  // get cleaned up when StreamLoader::OnStopRequest gets called.
  //
  // Note that we hold on to all sheet loads, even if in the end they happen not
  // to be cacheable.
  nsTHashMap<SheetLoadDataHashKey, WeakPtr<css::SheetLoadData>> mLoadingDatas;

  // An origin-to-number-of-registered-documents count, in order to manage cache
  // eviction as described in RegisterLoader / UnregisterLoader.
  nsTHashMap<PrincipalHashKey, uint32_t> mLoaderPrincipalRefCnt;

  static SharedStyleSheetCache* sInstance;
};

}  // namespace mozilla

#endif
