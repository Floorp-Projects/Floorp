/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedStyleSheetCache.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/css/SheetLoadData.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Document.h"
#include "mozilla/ServoBindings.h"
#include "nsContentUtils.h"
#include "nsXULPrototypeCache.h"

extern mozilla::LazyLogModule sCssLoaderLog;

#define LOG(args) MOZ_LOG(sCssLoaderLog, mozilla::LogLevel::Debug, args)

namespace mozilla {

using css::SheetLoadData;
using SheetState = css::Loader::SheetState;
using LoadDataArray = css::Loader::LoadDataArray;
using IsAlternate = css::Loader::IsAlternate;

SharedStyleSheetCache* SharedStyleSheetCache::sInstance;

void SharedStyleSheetCache::Clear(nsIPrincipal* aForPrincipal,
                                  const nsACString* aBaseDomain) {
  using ContentParent = dom::ContentParent;

  if (XRE_IsParentProcess()) {
    auto forPrincipal = aForPrincipal ? Some(RefPtr(aForPrincipal)) : Nothing();
    auto baseDomain = aBaseDomain ? Some(nsCString(*aBaseDomain)) : Nothing();

    for (auto* cp : ContentParent::AllProcesses(ContentParent::eLive)) {
      Unused << cp->SendClearStyleSheetCache(forPrincipal, baseDomain);
    }
  }

  if (!sInstance) {
    return;
  }

  // No filter, clear all.
  if (!aForPrincipal && !aBaseDomain) {
    sInstance->mCompleteSheets.Clear();
    return;
  }

  for (auto iter = sInstance->mCompleteSheets.Iter(); !iter.Done();
       iter.Next()) {
    const bool shouldRemove = [&] {
      if (aForPrincipal && iter.Key().Principal()->Equals(aForPrincipal)) {
        return true;
      }
      if (!aBaseDomain) {
        return false;
      }
      // Clear by baseDomain.
      nsIPrincipal* partitionPrincipal = iter.Key().PartitionPrincipal();

      // Clear entries with matching base domain. This includes entries
      // which are partitioned under other top level sites (= have a
      // partitionKey set).
      nsAutoCString principalBaseDomain;
      nsresult rv = partitionPrincipal->GetBaseDomain(principalBaseDomain);
      if (NS_SUCCEEDED(rv) && principalBaseDomain.Equals(*aBaseDomain)) {
        return true;
      }

      // Clear entries partitioned under aBaseDomain.
      nsAutoString partitionKeyBaseDomain;
      bool ok = StoragePrincipalHelper::GetBaseDomainFromPartitionKey(
          partitionPrincipal->OriginAttributesRef().mPartitionKey,
          partitionKeyBaseDomain);
      return ok &&
             NS_ConvertUTF16toUTF8(partitionKeyBaseDomain).Equals(*aBaseDomain);
    }();

    if (shouldRemove) {
      iter.Remove();
    }
  }
}

already_AddRefed<SharedStyleSheetCache> SharedStyleSheetCache::Create() {
  MOZ_DIAGNOSTIC_ASSERT(!sInstance);
  RefPtr<SharedStyleSheetCache> cache = new SharedStyleSheetCache();
  sInstance = cache.get();
  RegisterWeakMemoryReporter(cache.get());
  return cache.forget();
}

SharedStyleSheetCache::~SharedStyleSheetCache() {
  MOZ_DIAGNOSTIC_ASSERT(sInstance == this);
  UnregisterWeakMemoryReporter(this);
  sInstance = nullptr;
}

NS_IMPL_ISUPPORTS(SharedStyleSheetCache, nsIMemoryReporter)

MOZ_DEFINE_MALLOC_SIZE_OF(SharedStyleSheetCacheMallocSizeOf)

NS_IMETHODIMP
SharedStyleSheetCache::CollectReports(nsIHandleReportCallback* aHandleReport,
                                      nsISupports* aData, bool aAnonymize) {
  MOZ_COLLECT_REPORT("explicit/layout/style-sheet-cache/document-shared",
                     KIND_HEAP, UNITS_BYTES,
                     SizeOfIncludingThis(SharedStyleSheetCacheMallocSizeOf),
                     "Memory used for SharedStyleSheetCache to share style "
                     "sheets across documents (not to be confused with "
                     "GlobalStyleSheetCache)");
  return NS_OK;
}

static RefPtr<StyleSheet> CloneSheet(StyleSheet& aSheet) {
  return aSheet.Clone(nullptr, nullptr, nullptr, nullptr);
}

static void AssertComplete(const StyleSheet& aSheet) {
  // This sheet came from the XUL cache or SharedStyleSheetCache; it better be a
  // complete sheet.
  MOZ_ASSERT(aSheet.IsComplete(),
             "Sheet thinks it's not complete while we think it is");
}

static void AssertIncompleteSheetMatches(const SheetLoadData& aData,
                                         const SheetLoadDataHashKey& aKey) {
  MOZ_ASSERT(aKey.Principal()->Equals(aData.mTriggeringPrincipal),
             "Principals should be the same");
  MOZ_ASSERT(!aData.mSheet->HasForcedUniqueInner(),
             "CSSOM shouldn't allow access to incomplete sheets");
}

bool SharedStyleSheetCache::CompleteSheet::Expired() const {
  return mExpirationTime &&
         mExpirationTime <= nsContentUtils::SecondsFromPRTime(PR_Now());
}

SharedStyleSheetCache::CacheResult SharedStyleSheetCache::Lookup(
    css::Loader& aLoader, const SheetLoadDataHashKey& aKey, bool aSyncLoad) {
  nsIURI* uri = aKey.URI();
  LOG(("SharedStyleSheetCache::Lookup(%s)", uri->GetSpecOrDefault().get()));

  // Try to find first in the XUL prototype cache.
  if (dom::IsChromeURI(uri)) {
    nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
    if (cache && cache->IsEnabled()) {
      if (StyleSheet* sheet = cache->GetStyleSheet(uri)) {
        LOG(("  From XUL cache: %p", sheet));
        AssertComplete(*sheet);

        // See below, we always clone on insertion so we can guarantee the
        // stylesheet is not modified.
        MOZ_ASSERT(!sheet->HasForcedUniqueInner());

        // We need to check the parsing mode manually because the XUL cache only
        // keys off the URI. But we should fix that!
        if (sheet->ParsingMode() == aKey.ParsingMode()) {
          aLoader.DidHitCompleteSheetCache(aKey, nullptr);
          return {CloneSheet(*sheet), SheetState::Complete};
        }

        LOG(("    Not cloning due to mismatched parsing mode"));
      }
    }
  }

  // Now complete sheets.
  if (auto lookup = mCompleteSheets.Lookup(aKey)) {
    const CompleteSheet& completeSheet = lookup.Data();
    // We can assert the stylesheet has not been modified, as we clone it on
    // insertion.
    StyleSheet& cachedSheet = *completeSheet.mSheet;
    LOG(("  From completed: %p, bypass: %d, expired: %d", &cachedSheet,
         aLoader.ShouldBypassCache(), completeSheet.Expired()));

    if ((!aLoader.ShouldBypassCache() && !completeSheet.Expired()) ||
        aLoader.mLoadsPerformed.Contains(aKey)) {
      LOG(
          ("    Not expired yet, or previously loaded already in "
           "that document"));

      AssertComplete(cachedSheet);
      MOZ_ASSERT(cachedSheet.ParsingMode() == aKey.ParsingMode());
      MOZ_ASSERT(!cachedSheet.HasForcedUniqueInner());
      MOZ_ASSERT(!cachedSheet.HasModifiedRules());

      RefPtr<StyleSheet> clone = CloneSheet(cachedSheet);
      MOZ_ASSERT(!clone->HasForcedUniqueInner());
      MOZ_ASSERT(!clone->HasModifiedRules());

      aLoader.DidHitCompleteSheetCache(aKey, completeSheet.mUseCounters.get());
      return {std::move(clone), SheetState::Complete};
    }
  }

  if (aSyncLoad) {
    return {};
  }

  if (SheetLoadData* data = mLoadingDatas.Get(aKey)) {
    LOG(("  From loading: %p", data->mSheet.get()));
    AssertIncompleteSheetMatches(*data, aKey);
    return {CloneSheet(*data->mSheet), SheetState::Loading};
  }

  if (SheetLoadData* data = mPendingDatas.GetWeak(aKey)) {
    LOG(("  From pending: %p", data->mSheet.get()));
    AssertIncompleteSheetMatches(*data, aKey);
    return {CloneSheet(*data->mSheet), SheetState::Pending};
  }

  return {};
}

void SharedStyleSheetCache::WillStartPendingLoad(SheetLoadData& aData) {
  SheetLoadData* curr = &aData;
  do {
    MOZ_DIAGNOSTIC_ASSERT(curr->mLoader->mPendingLoadCount,
                          "Where did this pending load come from?");
    --curr->mLoader->mPendingLoadCount;
  } while ((curr = curr->mNext));
}

bool SharedStyleSheetCache::CoalesceLoad(const SheetLoadDataHashKey& aKey,
                                         SheetLoadData& aNewLoad,
                                         SheetState aExistingLoadState) {
  MOZ_ASSERT(SheetLoadDataHashKey(aNewLoad).KeyEquals(aKey));
  SheetLoadData* existingData = nullptr;
  if (aExistingLoadState == SheetState::Loading) {
    existingData = mLoadingDatas.Get(aKey);
    MOZ_ASSERT(existingData, "CreateSheet lied about the state");
  } else if (aExistingLoadState == SheetState::Pending) {
    existingData = mPendingDatas.GetWeak(aKey);
    MOZ_ASSERT(existingData, "CreateSheet lied about the state");
  }

  if (!existingData) {
    return false;
  }

  if (aExistingLoadState == SheetState::Pending && !aNewLoad.ShouldDefer()) {
    // Kick the load off; someone cares about it right away
    RefPtr<SheetLoadData> removedData;
    mPendingDatas.Remove(aKey, getter_AddRefs(removedData));
    MOZ_ASSERT(removedData == existingData, "Bad loading table");

    WillStartPendingLoad(*removedData);

    // We insert to the front instead of the back, to keep the invariant that
    // the front sheet always is the one that triggers the load.
    aNewLoad.mNext = std::move(removedData);
    LOG(("  Forcing load of pending data"));
    return false;
  }

  LOG(("  Glomming on to existing load"));
  SheetLoadData* data = existingData;
  while (data->mNext) {
    data = data->mNext;
  }
  data->mNext = &aNewLoad;

  return true;
}

size_t SharedStyleSheetCache::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  n += mCompleteSheets.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (const auto& data : mCompleteSheets.Values()) {
    n += data.mSheet->SizeOfIncludingThis(aMallocSizeOf);
    n += aMallocSizeOf(data.mUseCounters.get());
  }

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mLoadingDatas: transient, and should be small
  // - mPendingDatas: transient, and should be small
  return n;
}

void SharedStyleSheetCache::DeferSheetLoad(const SheetLoadDataHashKey& aKey,
                                           SheetLoadData& aData) {
  MOZ_ASSERT(SheetLoadDataHashKey(aData).KeyEquals(aKey));
  MOZ_DIAGNOSTIC_ASSERT(!aData.mNext, "Should only defer loads once");

  aData.mMustNotify = true;
  mPendingDatas.InsertOrUpdate(aKey, RefPtr{&aData});
}

void SharedStyleSheetCache::LoadStarted(const SheetLoadDataHashKey& aKey,
                                        SheetLoadData& aData) {
  MOZ_ASSERT(aData.mURI, "No load required?");
  MOZ_ASSERT(!aData.mIsLoading, "Already loading? How?");
  MOZ_ASSERT(SheetLoadDataHashKey(aData).KeyEquals(aKey));
  aData.mIsLoading = true;
  mLoadingDatas.InsertOrUpdate(aKey, &aData);
}

void SharedStyleSheetCache::LoadCompleted(SharedStyleSheetCache* aCache,
                                          SheetLoadData& aData,
                                          nsresult aStatus) {
  // If aStatus is a failure we need to mark this data failed.  We also need to
  // mark any ancestors of a failing data as failed and any sibling of a
  // failing data as failed.  Note that SheetComplete is never called on a
  // SheetLoadData that is the mNext of some other SheetLoadData.
  nsresult cancelledStatus = aStatus;
  if (NS_FAILED(aStatus)) {
    css::Loader::MarkLoadTreeFailed(aData);
  } else {
    cancelledStatus = NS_BINDING_ABORTED;
    SheetLoadData* data = &aData;
    do {
      if (data->mIsCancelled) {
        // We only need to mark loads for this loader as cancelled, so as to not
        // fire error events in unrelated documents.
        css::Loader::MarkLoadTreeFailed(*data, data->mLoader);
      }
    } while ((data = data->mNext));
  }

  // 8 is probably big enough for all our common cases.  It's not likely that
  // imports will nest more than 8 deep, and multiple sheets with the same URI
  // are rare.
  AutoTArray<RefPtr<SheetLoadData>, 8> datasToNotify;
  LoadCompletedInternal(aCache, aData, datasToNotify);

  // Now it's safe to go ahead and notify observers
  for (RefPtr<SheetLoadData>& data : datasToNotify) {
    auto status = data->mIsCancelled ? cancelledStatus : aStatus;
    data->mLoader->NotifyObservers(*data, status);
  }
}

void SharedStyleSheetCache::LoadCompletedInternal(
    SharedStyleSheetCache* aCache, SheetLoadData& aData,
    nsTArray<RefPtr<SheetLoadData>>& aDatasToNotify) {
  if (aData.mIsLoading) {
    MOZ_ASSERT(aCache);
    SheetLoadDataHashKey key(aData);
    Maybe<SheetLoadData*> loadingData = aCache->mLoadingDatas.Extract(key);
    MOZ_DIAGNOSTIC_ASSERT(loadingData);
    MOZ_DIAGNOSTIC_ASSERT(loadingData.value() == &aData);
    Unused << loadingData;
    aData.mIsLoading = false;
  }

  // Go through and deal with the whole linked list.
  SheetLoadData* data = &aData;
  do {
    MOZ_DIAGNOSTIC_ASSERT(!data->mSheetCompleteCalled);
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    data->mSheetCompleteCalled = true;
#endif

    if (!data->mSheetAlreadyComplete) {
      // If mSheetAlreadyComplete, then the sheet could well be modified between
      // when we posted the async call to SheetComplete and now, since the sheet
      // was page-accessible during that whole time.

      // HasForcedUniqueInner() is okay if the sheet is constructed, because
      // constructed sheets are always unique and they may be set to complete
      // multiple times if their rules are replaced via Replace()
      MOZ_ASSERT(data->mSheet->IsConstructed() ||
                     !data->mSheet->HasForcedUniqueInner(),
                 "should not get a forced unique inner during parsing");
      // Insert the sheet into the tree now the sheet has loaded, but only if
      // the sheet is still relevant, and if this is a top-level sheet.
      const bool needInsertIntoTree = [&] {
        if (!data->mLoader->GetDocument()) {
          // Not a document load, nothing to do.
          return false;
        }
        if (data->IsPreload()) {
          // Preloads are not supposed to be observable.
          return false;
        }
        if (data->mSheet->IsConstructed()) {
          // Constructable sheets are not in the regular stylesheet tree.
          return false;
        }
        if (data->mIsChildSheet) {
          // A child sheet, those will get exposed from the parent, no need to
          // insert them into the tree.
          return false;
        }
        if (data->mOwningNode != data->mSheet->GetOwnerNode()) {
          // The sheet was already removed from the tree and is no longer the
          // current sheet of the owning node, we can bail.
          return false;
        }
        return true;
      }();

      if (needInsertIntoTree) {
        data->mLoader->InsertSheetInTree(*data->mSheet, data->mOwningNode);
      }
      data->mSheet->SetComplete();
      data->ScheduleLoadEventIfNeeded();
    } else if (data->mSheet->IsApplicable()) {
      if (dom::Document* doc = data->mLoader->GetDocument()) {
        // We post these events for devtools, even though the applicable state
        // has not actually changed, to make the cache not observable.
        doc->PostStyleSheetApplicableStateChangeEvent(*data->mSheet);
      }
    }

    aDatasToNotify.AppendElement(data);

    NS_ASSERTION(!data->mParentData || data->mParentData->mPendingChildren != 0,
                 "Broken pending child count on our parent");

    // If we have a parent, our parent is no longer being parsed, and
    // we are the last pending child, then our load completion
    // completes the parent too.  Note that the parent _can_ still be
    // being parsed (eg if the child (us) failed to open the channel
    // or some such).
    if (data->mParentData && --(data->mParentData->mPendingChildren) == 0 &&
        !data->mParentData->mIsBeingParsed) {
      LoadCompletedInternal(aCache, *data->mParentData, aDatasToNotify);
    }

    data = data->mNext;
  } while (data);

  if (aCache) {
    aCache->InsertIntoCompleteCacheIfNeeded(aData);
  }
}

void SharedStyleSheetCache::InsertIntoCompleteCacheIfNeeded(
    SheetLoadData& aData) {
  MOZ_ASSERT(aData.mLoader->GetDocument(),
             "We only cache document-associated sheets");
  LOG(("SharedStyleSheetCache::InsertIntoCompleteCacheIfNeeded"));
  // If we ever start doing this for failed loads, we'll need to adjust the
  // PostLoadEvent code that thinks anything already complete must have loaded
  // succesfully.
  if (aData.mLoadFailed) {
    LOG(("  Load failed, bailing"));
    return;
  }

  // If this sheet came from the cache already, there's no need to override
  // anything.
  if (aData.mSheetAlreadyComplete) {
    LOG(("  Sheet came from the cache, bailing"));
    return;
  }

  if (!aData.mURI) {
    LOG(("  Inline or constructable style sheet, bailing"));
    // Inline sheet caching happens in Loader::mInlineSheets.
    // Constructable sheets are not worth caching, they're always unique.
    return;
  }

  // We need to clone the sheet on insertion to the cache because otherwise the
  // stylesheets can keep alive full windows alive via either their JS wrapper,
  // or via StyleSheet::mRelevantGlobal.
  //
  // If this ever changes, then you also need to fix up the memory reporting in
  // both SizeOfIncludingThis and nsXULPrototypeCache::CollectMemoryReports.
  RefPtr<StyleSheet> sheet = CloneSheet(*aData.mSheet);

  if (dom::IsChromeURI(aData.mURI)) {
    nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
    if (cache && cache->IsEnabled()) {
      if (!cache->GetStyleSheet(aData.mURI)) {
        LOG(("  Putting sheet in XUL prototype cache"));
        NS_ASSERTION(sheet->IsComplete(),
                     "Should only be caching complete sheets");

        // NOTE: If we stop cloning sheets before insertion, we need to change
        // nsXULPrototypeCache::CollectMemoryReports() to stop using
        // SizeOfIncludingThis() because it will no longer own the sheets.
        cache->PutStyleSheet(std::move(sheet));
      }
    }
  } else {
    LOG(("  Putting style sheet in shared cache: %s",
         aData.mURI->GetSpecOrDefault().get()));
    SheetLoadDataHashKey key(aData);
    MOZ_ASSERT(sheet->IsComplete(), "Should only be caching complete sheets");

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    for (const auto& entry : mCompleteSheets) {
      if (!key.KeyEquals(entry.GetKey())) {
        MOZ_DIAGNOSTIC_ASSERT(entry.GetData().mSheet != sheet,
                              "Same sheet, different keys?");
      } else {
        MOZ_ASSERT(
            entry.GetData().Expired() || aData.mLoader->ShouldBypassCache(),
            "Overriding existing complete entry?");
      }
    }
#endif

    UniquePtr<StyleUseCounters> counters;
    if (aData.mUseCounters) {
      // TODO(emilio): Servo_UseCounters_Clone() or something?
      counters = Servo_UseCounters_Create().Consume();
      Servo_UseCounters_Merge(counters.get(), aData.mUseCounters.get());
    }

    mCompleteSheets.InsertOrUpdate(
        key, CompleteSheet{aData.mExpirationTime, std::move(counters),
                           std::move(sheet)});
  }
}

void SharedStyleSheetCache::StartDeferredLoadsForLoader(
    css::Loader& aLoader, StartLoads aStartLoads) {
  using PendingLoad = css::Loader::PendingLoad;

  LoadDataArray arr;
  for (auto iter = mPendingDatas.Iter(); !iter.Done(); iter.Next()) {
    bool startIt = false;
    SheetLoadData* data = iter.Data();
    do {
      if (data->mLoader == &aLoader) {
        // Note that we don't want to affect what the selected style set is, so
        // use true for aHasAlternateRel.
        if (aStartLoads != StartLoads::IfNonAlternate ||
            aLoader.IsAlternateSheet(iter.Data()->mTitle, true) !=
                IsAlternate::Yes) {
          startIt = true;
          break;
        }
      }
    } while ((data = data->mNext));
    if (startIt) {
      arr.AppendElement(std::move(iter.Data()));
      iter.Remove();
    }
  }
  for (auto& data : arr) {
    WillStartPendingLoad(*data);
    data->mLoader->LoadSheet(*data, SheetState::NeedsParser, PendingLoad::Yes);
  }
}

void SharedStyleSheetCache::CancelDeferredLoadsForLoader(css::Loader& aLoader) {
  LoadDataArray arr;

  for (auto iter = mPendingDatas.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<SheetLoadData>& first = iter.Data();
    SheetLoadData* prev = nullptr;
    SheetLoadData* current = iter.Data();
    do {
      if (current->mLoader != &aLoader) {
        prev = current;
        current = current->mNext;
        continue;
      }
      // Detach the load from the list, mark it as cancelled, and then below
      // call SheetComplete on it.
      RefPtr<SheetLoadData> strong =
          prev ? std::move(prev->mNext) : std::move(first);
      MOZ_ASSERT(strong == current);
      if (prev) {
        prev->mNext = std::move(strong->mNext);
        current = prev->mNext;
      } else {
        first = std::move(strong->mNext);
        current = first;
      }
      strong->mIsCancelled = true;
      arr.AppendElement(std::move(strong));
    } while (current);

    if (!first) {
      iter.Remove();
    }
  }

  for (auto& data : arr) {
    aLoader.SheetComplete(*data, NS_BINDING_ABORTED);
  }
}

void SharedStyleSheetCache::CancelLoadsForLoader(css::Loader& aLoader) {
  CancelDeferredLoadsForLoader(aLoader);

  // We can't stop in-progress loads because some other loader may care about
  // them.
  for (SheetLoadData* data : mLoadingDatas.Values()) {
    MOZ_DIAGNOSTIC_ASSERT(data,
                          "We weren't properly notified and the load was "
                          "incorrectly dropped on the floor");
    for (; data; data = data->mNext) {
      if (data->mLoader == &aLoader) {
        data->mIsCancelled = true;
      }
    }
  }
}

void SharedStyleSheetCache::RegisterLoader(css::Loader& aLoader) {
  MOZ_ASSERT(aLoader.GetDocument());
  mLoaderPrincipalRefCnt.LookupOrInsert(aLoader.GetDocument()->NodePrincipal(),
                                        0) += 1;
}

void SharedStyleSheetCache::UnregisterLoader(css::Loader& aLoader) {
  MOZ_ASSERT(aLoader.GetDocument());
  nsIPrincipal* prin = aLoader.GetDocument()->NodePrincipal();
  auto lookup = mLoaderPrincipalRefCnt.Lookup(prin);
  MOZ_RELEASE_ASSERT(lookup);
  MOZ_RELEASE_ASSERT(lookup.Data());
  if (!--lookup.Data()) {
    lookup.Remove();
    // TODO(emilio): Do this off a timer or something maybe.
    for (auto iter = mCompleteSheets.Iter(); !iter.Done(); iter.Next()) {
      if (iter.Key().LoaderPrincipal()->Equals(prin)) {
        iter.Remove();
      }
    }
  }
}

}  // namespace mozilla

#undef LOG
