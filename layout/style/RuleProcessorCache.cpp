/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * cache of re-usable nsCSSRuleProcessors for given sets of style sheets
 */

#include "RuleProcessorCache.h"

#include <algorithm>
#include "nsCSSRuleProcessor.h"
#include "nsThreadUtils.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(RuleProcessorCache, nsIMemoryReporter)

MOZ_DEFINE_MALLOC_SIZE_OF(RuleProcessorCacheMallocSizeOf)

NS_IMETHODIMP
RuleProcessorCache::CollectReports(nsIHandleReportCallback* aHandleReport,
                                   nsISupports* aData, bool aAnonymize)
{
  MOZ_COLLECT_REPORT(
    "explicit/layout/rule-processor-cache", KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(RuleProcessorCacheMallocSizeOf),
    "Memory used for cached rule processors.");

  return NS_OK;
}

RuleProcessorCache::~RuleProcessorCache()
{
  UnregisterWeakMemoryReporter(this);

  for (Entry& e : mEntries) {
    for (DocumentEntry& de : e.mDocumentEntries) {
      if (de.mRuleProcessor->GetExpirationState()->IsTracked()) {
        mExpirationTracker.RemoveObject(de.mRuleProcessor);
      }
      de.mRuleProcessor->SetInRuleProcessorCache(false);
    }
  }
}

void
RuleProcessorCache::InitMemoryReporter()
{
  RegisterWeakMemoryReporter(this);
}

/* static */ bool
RuleProcessorCache::EnsureGlobal()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gShutdown) {
    return false;
  }

  if (!gRuleProcessorCache) {
    gRuleProcessorCache = new RuleProcessorCache;
    gRuleProcessorCache->InitMemoryReporter();
  }
  return true;
}

/* static */ void
RuleProcessorCache::RemoveSheet(CSSStyleSheet* aSheet)
{
  if (!EnsureGlobal()) {
    return;
  }
  gRuleProcessorCache->DoRemoveSheet(aSheet);
}

#ifdef DEBUG
/* static */ bool
RuleProcessorCache::HasRuleProcessor(nsCSSRuleProcessor* aRuleProcessor)
{
  if (!EnsureGlobal()) {
    return false;
  }
  return gRuleProcessorCache->DoHasRuleProcessor(aRuleProcessor);
}
#endif

/* static */ void
RuleProcessorCache::RemoveRuleProcessor(nsCSSRuleProcessor* aRuleProcessor)
{
  if (!EnsureGlobal()) {
    return;
  }
  gRuleProcessorCache->DoRemoveRuleProcessor(aRuleProcessor);
}

/* static */ nsCSSRuleProcessor*
RuleProcessorCache::GetRuleProcessor(const nsTArray<CSSStyleSheet*>& aSheets,
                                     nsPresContext* aPresContext)
{
  if (!EnsureGlobal()) {
    return nullptr;
  }
  return gRuleProcessorCache->DoGetRuleProcessor(aSheets, aPresContext);
}

/* static */ void
RuleProcessorCache::PutRuleProcessor(
    const nsTArray<CSSStyleSheet*>& aSheets,
    nsTArray<css::DocumentRule*>&& aDocumentRulesInSheets,
    const nsDocumentRuleResultCacheKey& aCacheKey,
    nsCSSRuleProcessor* aRuleProcessor)
{
  if (!EnsureGlobal()) {
    return;
  }
  gRuleProcessorCache->DoPutRuleProcessor(aSheets, Move(aDocumentRulesInSheets),
                                          aCacheKey, aRuleProcessor);
}

/* static */ void
RuleProcessorCache::StartTracking(nsCSSRuleProcessor* aRuleProcessor)
{
  if (!EnsureGlobal()) {
    return;
  }
  return gRuleProcessorCache->DoStartTracking(aRuleProcessor);
}

/* static */ void
RuleProcessorCache::StopTracking(nsCSSRuleProcessor* aRuleProcessor)
{
  if (!EnsureGlobal()) {
    return;
  }
  return gRuleProcessorCache->DoStopTracking(aRuleProcessor);
}

void
RuleProcessorCache::DoRemoveSheet(CSSStyleSheet* aSheet)
{
  Entry* last = std::remove_if(mEntries.begin(), mEntries.end(),
                               HasSheet_ThenRemoveRuleProcessors(this, aSheet));
  mEntries.TruncateLength(last - mEntries.begin());
}

nsCSSRuleProcessor*
RuleProcessorCache::DoGetRuleProcessor(const nsTArray<CSSStyleSheet*>& aSheets,
                                       nsPresContext* aPresContext)
{
  for (Entry& e : mEntries) {
    if (e.mSheets == aSheets) {
      for (DocumentEntry& de : e.mDocumentEntries) {
        if (de.mCacheKey.Matches(aPresContext, e.mDocumentRulesInSheets)) {
          return de.mRuleProcessor;
        }
      }
      // Entry::mSheets is unique; if we matched aSheets but didn't
      // find a matching DocumentEntry, we won't find one later in
      // mEntries.
      return nullptr;
    }
  }
  return nullptr;
}

void
RuleProcessorCache::DoPutRuleProcessor(
    const nsTArray<CSSStyleSheet*>& aSheets,
    nsTArray<css::DocumentRule*>&& aDocumentRulesInSheets,
    const nsDocumentRuleResultCacheKey& aCacheKey,
    nsCSSRuleProcessor* aRuleProcessor)
{
  MOZ_ASSERT(!aRuleProcessor->IsInRuleProcessorCache());

  Entry* entry = nullptr;
  for (Entry& e : mEntries) {
    if (e.mSheets == aSheets) {
      entry = &e;
      break;
    }
  }

  if (!entry) {
    entry = mEntries.AppendElement();
    entry->mSheets = aSheets;
    entry->mDocumentRulesInSheets = aDocumentRulesInSheets;
    for (CSSStyleSheet* sheet : aSheets) {
      sheet->SetInRuleProcessorCache();
    }
  } else {
    MOZ_ASSERT(entry->mDocumentRulesInSheets == aDocumentRulesInSheets,
               "DocumentRule array shouldn't have changed");
  }

#ifdef DEBUG
  for (DocumentEntry& de : entry->mDocumentEntries) {
    MOZ_ASSERT(de.mCacheKey != aCacheKey,
               "should not have duplicate document cache keys");
  }
#endif

  DocumentEntry* documentEntry = entry->mDocumentEntries.AppendElement();
  documentEntry->mCacheKey = aCacheKey;
  documentEntry->mRuleProcessor = aRuleProcessor;
  aRuleProcessor->SetInRuleProcessorCache(true);
}

#ifdef DEBUG
bool
RuleProcessorCache::DoHasRuleProcessor(nsCSSRuleProcessor* aRuleProcessor)
{
  for (Entry& e : mEntries) {
    for (DocumentEntry& de : e.mDocumentEntries) {
      if (de.mRuleProcessor == aRuleProcessor) {
        return true;
      }
    }
  }
  return false;
}
#endif

void
RuleProcessorCache::DoRemoveRuleProcessor(nsCSSRuleProcessor* aRuleProcessor)
{
  MOZ_ASSERT(aRuleProcessor->IsInRuleProcessorCache());

  aRuleProcessor->SetInRuleProcessorCache(false);
  mExpirationTracker.RemoveObjectIfTracked(aRuleProcessor);
  for (Entry& e : mEntries) {
    for (size_t i = 0; i < e.mDocumentEntries.Length(); i++) {
      if (e.mDocumentEntries[i].mRuleProcessor == aRuleProcessor) {
        e.mDocumentEntries.RemoveElementAt(i);
        return;
      }
    }
  }

  MOZ_ASSERT_UNREACHABLE("should have found rule processor");
}

void
RuleProcessorCache::DoStartTracking(nsCSSRuleProcessor* aRuleProcessor)
{
  mExpirationTracker.AddObject(aRuleProcessor);
}

void
RuleProcessorCache::DoStopTracking(nsCSSRuleProcessor* aRuleProcessor)
{
  mExpirationTracker.RemoveObjectIfTracked(aRuleProcessor);
}

size_t
RuleProcessorCache::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  size_t n = aMallocSizeOf(this);

  int count = 0;
  n += mEntries.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (Entry& e : mEntries) {
    n += e.mDocumentEntries.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (DocumentEntry& de : e.mDocumentEntries) {
      count++;
      n += de.mRuleProcessor->SizeOfIncludingThis(aMallocSizeOf);
    }
  }

  return n;
}

void
RuleProcessorCache::ExpirationTracker::RemoveObjectIfTracked(
    nsCSSRuleProcessor* aRuleProcessor)
{
  if (aRuleProcessor->GetExpirationState()->IsTracked()) {
    RemoveObject(aRuleProcessor);
  }
}

bool RuleProcessorCache::gShutdown = false;
mozilla::StaticRefPtr<RuleProcessorCache> RuleProcessorCache::gRuleProcessorCache;
