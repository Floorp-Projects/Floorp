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

#define LOG(...) MOZ_LOG(sCssLoaderLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {

NS_IMPL_ISUPPORTS(SharedStyleSheetCache, nsIMemoryReporter)

MOZ_DEFINE_MALLOC_SIZE_OF(SharedStyleSheetCacheMallocSizeOf)

SharedStyleSheetCache::SharedStyleSheetCache() = default;

void SharedStyleSheetCache::Init() { RegisterWeakMemoryReporter(this); }

SharedStyleSheetCache::~SharedStyleSheetCache() {
  UnregisterWeakMemoryReporter(this);
}

void SharedStyleSheetCache::LoadCompleted(SharedStyleSheetCache* aCache,
                                          StyleSheetLoadData& aData,
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
    css::SheetLoadData* data = &aData;
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
  AutoTArray<RefPtr<css::SheetLoadData>, 8> datasToNotify;
  LoadCompletedInternal(aCache, aData, datasToNotify);

  // Now it's safe to go ahead and notify observers
  for (RefPtr<css::SheetLoadData>& data : datasToNotify) {
    auto status = data->mIsCancelled ? cancelledStatus : aStatus;
    data->mLoader->NotifyObservers(*data, status);
  }
}

void SharedStyleSheetCache::InsertIfNeeded(css::SheetLoadData& aData) {
  MOZ_ASSERT(aData.mLoader->IsDocumentAssociated(),
             "We only cache document-associated sheets");
  LOG("SharedStyleSheetCache::InsertIfNeeded");
  // If we ever start doing this for failed loads, we'll need to adjust the
  // PostLoadEvent code that thinks anything already complete must have loaded
  // succesfully.
  if (aData.mLoadFailed) {
    LOG("  Load failed, bailing");
    return;
  }

  // If this sheet came from the cache already, there's no need to override
  // anything.
  if (aData.mSheetAlreadyComplete) {
    LOG("  Sheet came from the cache, bailing");
    return;
  }

  if (!aData.mURI) {
    LOG("  Inline or constructable style sheet, bailing");
    // Inline sheet caching happens in Loader::mInlineSheets.
    // Constructable sheets are not worth caching, they're always unique.
    return;
  }

  LOG("  Putting style sheet in shared cache: %s",
      aData.mURI->GetSpecOrDefault().get());
  Insert(aData);
}

void SharedStyleSheetCache::LoadCompletedInternal(
    SharedStyleSheetCache* aCache, css::SheetLoadData& aData,
    nsTArray<RefPtr<css::SheetLoadData>>& aDatasToNotify) {
  if (aCache) {
    aCache->LoadCompleted(aData);
  }

  // Go through and deal with the whole linked list.
  auto* data = &aData;
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
        if (data->mOwningNodeBeforeLoadEvent != data->mSheet->GetOwnerNode()) {
          // The sheet was already removed from the tree and is no longer the
          // current sheet of the owning node, we can bail.
          return false;
        }
        return true;
      }();

      if (needInsertIntoTree) {
        data->mLoader->InsertSheetInTree(*data->mSheet);
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
    aCache->InsertIfNeeded(aData);
  }
}

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

  if (sInstance) {
    sInstance->ClearInProcess(aForPrincipal, aBaseDomain);
  }
}

}  // namespace mozilla

#undef LOG
