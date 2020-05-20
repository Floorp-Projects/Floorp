/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ChildSHistory.h"
#include "mozilla/dom/ChildSHistoryBinding.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentFrameMessageManager.h"
#include "mozilla/StaticPrefs_fission.h"
#include "nsComponentManagerUtils.h"
#include "nsSHEntry.h"
#include "nsSHistory.h"
#include "nsDocShell.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace dom {

ChildSHistory::ChildSHistory(BrowsingContext* aBrowsingContext)
    : mBrowsingContext(aBrowsingContext) {}

void ChildSHistory::SetIsInProcess(bool aIsInProcess) {
  if (!aIsInProcess) {
    mHistory = nullptr;

    return;
  }

  if (mHistory) {
    return;
  }

  mHistory = new nsSHistory(mBrowsingContext);
}

int32_t ChildSHistory::Count() { return mHistory->GetCount(); }

int32_t ChildSHistory::Index() {
  int32_t index;
  mHistory->GetIndex(&index);
  return index;
}

void ChildSHistory::Reload(uint32_t aReloadFlags, ErrorResult& aRv) {
  aRv = mHistory->Reload(aReloadFlags);
}

bool ChildSHistory::CanGo(int32_t aOffset) {
  CheckedInt<int32_t> index = Index();
  index += aOffset;
  if (!index.isValid()) {
    return false;
  }
  return index.value() < Count() && index.value() >= 0;
}

void ChildSHistory::Go(int32_t aOffset, ErrorResult& aRv) {
  CheckedInt<int32_t> index = Index();
  index += aOffset;
  if (!index.isValid()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  aRv = mHistory->GotoIndex(index.value());
}

void ChildSHistory::AsyncGo(int32_t aOffset) {
  if (!CanGo(aOffset)) {
    return;
  }

  RefPtr<PendingAsyncHistoryNavigation> asyncNav =
      new PendingAsyncHistoryNavigation(this, aOffset);
  mPendingNavigations.insertBack(asyncNav);
  NS_DispatchToCurrentThread(asyncNav.forget());
}

void ChildSHistory::RemovePendingHistoryNavigations() {
  mPendingNavigations.clear();
}

void ChildSHistory::EvictLocalContentViewers() {
  mHistory->EvictAllContentViewers();
}

nsISHistory* ChildSHistory::LegacySHistory() {
  MOZ_RELEASE_ASSERT(mHistory);
  return mHistory;
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ChildSHistory)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ChildSHistory)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ChildSHistory)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ChildSHistory, mBrowsingContext, mHistory)

JSObject* ChildSHistory::WrapObject(JSContext* cx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return ChildSHistory_Binding::Wrap(cx, this, aGivenProto);
}

nsISupports* ChildSHistory::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

}  // namespace dom
}  // namespace mozilla
