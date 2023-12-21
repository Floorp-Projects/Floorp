/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/StreamList.h"

#include <algorithm>
#include "mozilla/dom/cache/CacheStreamControlParent.h"
#include "mozilla/dom/cache/Context.h"
#include "mozilla/dom/cache/Manager.h"
#include "nsIInputStream.h"

namespace mozilla::dom::cache {

namespace {

auto MatchById(const nsID& aId) {
  return [aId](const auto& entry) { return entry.mId == aId; };
}

}  // namespace

StreamList::StreamList(SafeRefPtr<Manager> aManager,
                       SafeRefPtr<Context> aContext)
    : mManager(std::move(aManager)),
      mContext(std::move(aContext)),
      mCacheId(INVALID_CACHE_ID),
      mStreamControl(nullptr),
      mActivated(false) {
  MOZ_DIAGNOSTIC_ASSERT(mManager);
  mContext->AddActivity(*this);
}

Manager& StreamList::GetManager() const {
  MOZ_DIAGNOSTIC_ASSERT(mManager);
  return *mManager;
}

bool StreamList::ShouldOpenStreamFor(const nsID& aId) const {
  NS_ASSERT_OWNINGTHREAD(StreamList);

  return std::any_of(mList.cbegin(), mList.cend(), MatchById(aId));
}

void StreamList::SetStreamControl(CacheStreamControlParent* aStreamControl) {
  NS_ASSERT_OWNINGTHREAD(StreamList);
  MOZ_DIAGNOSTIC_ASSERT(aStreamControl);

  // For cases where multiple streams are serialized for a single list
  // then the control will get passed multiple times.  This is ok, but
  // it should be the same control each time.
  if (mStreamControl) {
    MOZ_DIAGNOSTIC_ASSERT(aStreamControl == mStreamControl);
    return;
  }

  mStreamControl = aStreamControl;
  mStreamControl->SetStreamList(SafeRefPtrFromThis());
}

void StreamList::RemoveStreamControl(CacheStreamControlParent* aStreamControl) {
  NS_ASSERT_OWNINGTHREAD(StreamList);
  MOZ_DIAGNOSTIC_ASSERT(mStreamControl);
  MOZ_DIAGNOSTIC_ASSERT(mStreamControl == aStreamControl);
  mStreamControl = nullptr;
}

void StreamList::Activate(CacheId aCacheId) {
  NS_ASSERT_OWNINGTHREAD(StreamList);
  MOZ_DIAGNOSTIC_ASSERT(!mActivated);
  MOZ_DIAGNOSTIC_ASSERT(mCacheId == INVALID_CACHE_ID);
  mActivated = true;
  mCacheId = aCacheId;
  mManager->AddRefCacheId(mCacheId);
  mManager->AddStreamList(*this);

  for (uint32_t i = 0; i < mList.Length(); ++i) {
    mManager->AddRefBodyId(mList[i].mId);
  }
}

void StreamList::Add(const nsID& aId, nsCOMPtr<nsIInputStream>&& aStream) {
  // All streams should be added on IO thread before we set the stream
  // control on the owning IPC thread.
  MOZ_DIAGNOSTIC_ASSERT(!mStreamControl);
  mList.EmplaceBack(aId, std::move(aStream));
}

already_AddRefed<nsIInputStream> StreamList::Extract(const nsID& aId) {
  NS_ASSERT_OWNINGTHREAD(StreamList);

  const auto it = std::find_if(mList.begin(), mList.end(), MatchById(aId));

  return it != mList.end() ? it->mStream.forget() : nullptr;
}

void StreamList::NoteClosed(const nsID& aId) {
  NS_ASSERT_OWNINGTHREAD(StreamList);

  const auto it = std::find_if(mList.begin(), mList.end(), MatchById(aId));
  if (it != mList.end()) {
    mList.RemoveElementAt(it);
    mManager->ReleaseBodyId(aId);
  }

  if (mList.IsEmpty() && mStreamControl) {
    mStreamControl->Shutdown();
  }
}

void StreamList::NoteClosedAll() {
  NS_ASSERT_OWNINGTHREAD(StreamList);
  for (uint32_t i = 0; i < mList.Length(); ++i) {
    mManager->ReleaseBodyId(mList[i].mId);
  }
  mList.Clear();

  if (mStreamControl) {
    mStreamControl->Shutdown();
  }
}

void StreamList::CloseAll() {
  NS_ASSERT_OWNINGTHREAD(StreamList);
  if (mStreamControl && mStreamControl->CanSend()) {
    auto* streamControl = std::exchange(mStreamControl, nullptr);

    streamControl->CloseAll();

    mStreamControl = std::exchange(streamControl, nullptr);

    mStreamControl->Shutdown();
  } else {
    // We cannot interact with the child, let's just clear our lists of
    // streams to unblock shutdown.
    if (NS_WARN_IF(mStreamControl)) {
      // TODO: Check if this case is actually possible. We might see a late
      // delivery of the CSCP::ActorDestroy? What would that mean for CanSend?
      mStreamControl->LostIPCCleanup(SafeRefPtrFromThis());
      mStreamControl = nullptr;
    } else {
      NoteClosedAll();
    }
  }
}

void StreamList::Cancel() {
  NS_ASSERT_OWNINGTHREAD(StreamList);
  CloseAll();
}

bool StreamList::MatchesCacheId(CacheId aCacheId) const {
  NS_ASSERT_OWNINGTHREAD(StreamList);
  return aCacheId == mCacheId;
}

void StreamList::DoStringify(nsACString& aData) {
  aData.Append("StreamList "_ns + kStringifyStartInstance +
               //
               "List:"_ns +
               IntToCString(static_cast<uint64_t>(mList.Length())) +
               kStringifyDelimiter +
               //
               "Activated:"_ns + IntToCString(mActivated) + ")"_ns +
               kStringifyDelimiter +
               //
               "Manager:"_ns + IntToCString(static_cast<bool>(mManager)));
  if (mManager) {
    aData.Append(" "_ns);
    mManager->Stringify(aData);
  }
  aData.Append(kStringifyDelimiter +
               //
               "Context:"_ns + IntToCString(static_cast<bool>(mContext)));
  if (mContext) {
    aData.Append(" "_ns);
    mContext->Stringify(aData);
  }
  aData.Append(kStringifyEndInstance);
}

StreamList::~StreamList() {
  NS_ASSERT_OWNINGTHREAD(StreamList);
  MOZ_DIAGNOSTIC_ASSERT(!mStreamControl);
  if (mActivated) {
    mManager->RemoveStreamList(*this);
    for (uint32_t i = 0; i < mList.Length(); ++i) {
      mManager->ReleaseBodyId(mList[i].mId);
    }
    mManager->ReleaseCacheId(mCacheId);
  }
  mContext->RemoveActivity(*this);
}

}  // namespace mozilla::dom::cache
