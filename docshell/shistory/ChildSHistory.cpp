/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ChildSHistory.h"
#include "mozilla/dom/ChildSHistoryBinding.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentFrameMessageManager.h"
#include "nsIXULRuntime.h"
#include "nsComponentManagerUtils.h"
#include "nsSHEntry.h"
#include "nsSHistory.h"
#include "nsDocShell.h"
#include "nsXULAppAPI.h"

extern mozilla::LazyLogModule gSHLog;

namespace mozilla {
namespace dom {

ChildSHistory::ChildSHistory(BrowsingContext* aBrowsingContext)
    : mBrowsingContext(aBrowsingContext) {}

ChildSHistory::~ChildSHistory() {
  if (mHistory) {
    static_cast<nsSHistory*>(mHistory.get())->SetBrowsingContext(nullptr);
  }
}

void ChildSHistory::SetBrowsingContext(BrowsingContext* aBrowsingContext) {
  mBrowsingContext = aBrowsingContext;
}

void ChildSHistory::SetIsInProcess(bool aIsInProcess) {
  if (!aIsInProcess) {
    MOZ_ASSERT_IF(mozilla::SessionHistoryInParent(), !mHistory);
    if (!mozilla::SessionHistoryInParent()) {
      RemovePendingHistoryNavigations();
      if (mHistory) {
        static_cast<nsSHistory*>(mHistory.get())->SetBrowsingContext(nullptr);
        mHistory = nullptr;
      }
    }

    return;
  }

  if (mHistory || mozilla::SessionHistoryInParent()) {
    return;
  }

  mHistory = new nsSHistory(mBrowsingContext);
}

int32_t ChildSHistory::Count() {
  if (mozilla::SessionHistoryInParent()) {
    uint32_t length = mLength;
    for (uint32_t i = 0; i < mPendingSHistoryChanges.Length(); ++i) {
      length += mPendingSHistoryChanges[i].mLengthDelta;
    }

    return length;
  }
  return mHistory->GetCount();
}

int32_t ChildSHistory::Index() {
  if (mozilla::SessionHistoryInParent()) {
    uint32_t index = mIndex;
    for (uint32_t i = 0; i < mPendingSHistoryChanges.Length(); ++i) {
      index += mPendingSHistoryChanges[i].mIndexDelta;
    }

    return index;
  }
  int32_t index;
  mHistory->GetIndex(&index);
  return index;
}

nsID ChildSHistory::AddPendingHistoryChange() {
  int32_t indexDelta = 1;
  int32_t lengthDelta = (Index() + indexDelta) - (Count() - 1);
  return AddPendingHistoryChange(indexDelta, lengthDelta);
}

nsID ChildSHistory::AddPendingHistoryChange(int32_t aIndexDelta,
                                            int32_t aLengthDelta) {
  nsID changeID = nsID::GenerateUUID();
  PendingSHistoryChange change = {changeID, aIndexDelta, aLengthDelta};
  mPendingSHistoryChanges.AppendElement(change);
  return changeID;
}

void ChildSHistory::SetIndexAndLength(uint32_t aIndex, uint32_t aLength,
                                      const nsID& aChangeID) {
  mIndex = aIndex;
  mLength = aLength;
  mPendingSHistoryChanges.RemoveElementsBy(
      [aChangeID](const PendingSHistoryChange& aChange) {
        return aChange.mChangeID == aChangeID;
      });
}

void ChildSHistory::Reload(uint32_t aReloadFlags, ErrorResult& aRv) {
  if (mozilla::SessionHistoryInParent()) {
    if (XRE_IsParentProcess()) {
      nsCOMPtr<nsISHistory> shistory =
          mBrowsingContext->Canonical()->GetSessionHistory();
      if (shistory) {
        aRv = shistory->Reload(aReloadFlags);
      }
    } else {
      ContentChild::GetSingleton()->SendHistoryReload(mBrowsingContext,
                                                      aReloadFlags);
    }

    return;
  }
  nsCOMPtr<nsISHistory> shistory = mHistory;
  aRv = shistory->Reload(aReloadFlags);
}

bool ChildSHistory::CanGo(int32_t aOffset) {
  CheckedInt<int32_t> index = Index();
  index += aOffset;
  if (!index.isValid()) {
    return false;
  }
  return index.value() < Count() && index.value() >= 0;
}

void ChildSHistory::Go(int32_t aOffset, bool aRequireUserInteraction,
                       bool aUserActivation, ErrorResult& aRv) {
  CheckedInt<int32_t> index = Index();
  MOZ_LOG(
      gSHLog, LogLevel::Debug,
      ("ChildSHistory::Go(%d), current index = %d", aOffset, index.value()));
  if (aRequireUserInteraction && aOffset != -1 && aOffset != 1) {
    NS_ERROR(
        "aRequireUserInteraction may only be used with an offset of -1 or 1");
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  while (true) {
    index += aOffset;
    if (!index.isValid()) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    // Check for user interaction if desired, except for the first and last
    // history entries. We compare with >= to account for the case where
    // aOffset >= Count().
    if (!aRequireUserInteraction || index.value() >= Count() - 1 ||
        index.value() <= 0) {
      break;
    }
    if (mHistory && mHistory->HasUserInteractionAtIndex(index.value())) {
      break;
    }
  }

  GotoIndex(index.value(), aOffset, aRequireUserInteraction, aUserActivation,
            aRv);
}

void ChildSHistory::AsyncGo(int32_t aOffset, bool aRequireUserInteraction,
                            bool aUserActivation, CallerType aCallerType,
                            ErrorResult& aRv) {
  CheckedInt<int32_t> index = Index();
  MOZ_LOG(gSHLog, LogLevel::Debug,
          ("ChildSHistory::AsyncGo(%d), current index = %d", aOffset,
           index.value()));
  nsresult rv = mBrowsingContext->CheckLocationChangeRateLimit(aCallerType);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gSHLog, LogLevel::Debug, ("Rejected"));
    aRv.Throw(rv);
    return;
  }

  RefPtr<PendingAsyncHistoryNavigation> asyncNav =
      new PendingAsyncHistoryNavigation(this, aOffset, aRequireUserInteraction,
                                        aUserActivation);
  mPendingNavigations.insertBack(asyncNav);
  NS_DispatchToCurrentThread(asyncNav.forget());
}

void ChildSHistory::GotoIndex(int32_t aIndex, int32_t aOffset,
                              bool aRequireUserInteraction,
                              bool aUserActivation, ErrorResult& aRv) {
  MOZ_LOG(gSHLog, LogLevel::Debug,
          ("ChildSHistory::GotoIndex(%d, %d), epoch %" PRIu64, aIndex, aOffset,
           mHistoryEpoch));
  if (mozilla::SessionHistoryInParent()) {
    if (!mPendingEpoch) {
      mPendingEpoch = true;
      RefPtr<ChildSHistory> self(this);
      NS_DispatchToCurrentThread(
          NS_NewRunnableFunction("UpdateEpochRunnable", [self] {
            self->mHistoryEpoch++;
            self->mPendingEpoch = false;
          }));
    }

    nsCOMPtr<nsISHistory> shistory = mHistory;
    RefPtr<BrowsingContext> bc = mBrowsingContext;
    bc->HistoryGo(
        aOffset, mHistoryEpoch, aRequireUserInteraction, aUserActivation,
        [shistory](Maybe<int32_t>&& aRequestedIndex) {
          // FIXME Should probably only do this for non-fission.
          if (aRequestedIndex.isSome() && shistory) {
            shistory->InternalSetRequestedIndex(aRequestedIndex.value());
          }
        });
  } else {
    nsCOMPtr<nsISHistory> shistory = mHistory;
    aRv = shistory->GotoIndex(aIndex, aUserActivation);
  }
}

void ChildSHistory::RemovePendingHistoryNavigations() {
  // Per the spec, this generally shouldn't remove all navigations - it
  // depends if they're in the same document family or not.  We don't do
  // that.  Also with SessionHistoryInParent, this can only abort AsyncGo's
  // that have not yet been sent to the parent - see discussion of point
  // 2.2 in comments in nsDocShell::UpdateURLAndHistory()
  MOZ_LOG(gSHLog, LogLevel::Debug,
          ("ChildSHistory::RemovePendingHistoryNavigations: %zu",
           mPendingNavigations.length()));
  mPendingNavigations.clear();
}

void ChildSHistory::EvictLocalDocumentViewers() {
  if (!mozilla::SessionHistoryInParent()) {
    mHistory->EvictAllDocumentViewers();
  }
}

nsISHistory* ChildSHistory::GetLegacySHistory(ErrorResult& aError) {
  if (mozilla::SessionHistoryInParent()) {
    aError.ThrowTypeError(
        "legacySHistory is not available with session history in the parent.");
    return nullptr;
  }

  MOZ_RELEASE_ASSERT(mHistory);
  return mHistory;
}

nsISHistory* ChildSHistory::LegacySHistory() {
  IgnoredErrorResult ignore;
  nsISHistory* shistory = GetLegacySHistory(ignore);
  MOZ_RELEASE_ASSERT(shistory);
  return shistory;
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ChildSHistory)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ChildSHistory)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ChildSHistory)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(ChildSHistory)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ChildSHistory)
  if (tmp->mHistory) {
    static_cast<nsSHistory*>(tmp->mHistory.get())->SetBrowsingContext(nullptr);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowsingContext, mHistory)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ChildSHistory)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowsingContext, mHistory)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

JSObject* ChildSHistory::WrapObject(JSContext* cx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return ChildSHistory_Binding::Wrap(cx, this, aGivenProto);
}

nsISupports* ChildSHistory::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

}  // namespace dom
}  // namespace mozilla
