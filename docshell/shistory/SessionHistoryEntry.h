/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionHistoryEntry_h
#define mozilla_dom_SessionHistoryEntry_h

#include "mozilla/UniquePtr.h"
#include "nsISHEntry.h"
#include "nsStructuredCloneContainer.h"

class nsDocShellLoadState;
class nsIChannel;
class nsIInputStream;
class nsIReferrerInfo;
class nsISHistory;
class nsIURI;

namespace mozilla {
namespace dom {

struct SessionHistoryInfoAndId;
class SHEntrySharedParentState;

// SessionHistoryInfo stores session history data for a load. It can be sent
// over IPC and is used in both the parent and the child processes.
class SessionHistoryInfo {
 public:
  SessionHistoryInfo() = default;
  SessionHistoryInfo(nsDocShellLoadState* aLoadState, nsIChannel* aChannel);

  bool operator==(const SessionHistoryInfo& aInfo) const {
    return false;  // FIXME
  }

  nsIURI* GetURI() const { return mURI; }

 private:
  friend class SessionHistoryEntry;
  friend struct mozilla::ipc::IPDLParamTraits<SessionHistoryInfoAndId>;

  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsCOMPtr<nsIURI> mResultPrincipalURI;
  nsCOMPtr<nsIReferrerInfo> mReferrerInfo;
  nsString mTitle;
  nsCOMPtr<nsIInputStream> mPostData;
  uint32_t mLoadType = 0;
  int32_t mScrollPositionX = 0;
  int32_t mScrollPositionY = 0;
  RefPtr<nsStructuredCloneContainer> mStateData;
  nsString mSrcdocData;
  nsCOMPtr<nsIURI> mBaseURI;
  bool mLoadReplace = false;
  bool mURIWasModified = false;
  bool mIsSrcdocEntry = false;
  bool mScrollRestorationIsManual = false;
  bool mPersist = false;
};

// XXX Not sure that the id shouldn't just live in SessionHistoryInfo.
struct SessionHistoryInfoAndId {
  SessionHistoryInfoAndId() = default;
  SessionHistoryInfoAndId(uint64_t aId,
                          UniquePtr<mozilla::dom::SessionHistoryInfo> aInfo)
      : mId(aId), mInfo(std::move(aInfo)) {}
  SessionHistoryInfoAndId(SessionHistoryInfoAndId&& aInfoAndId) = default;
  SessionHistoryInfoAndId& operator=(
      const SessionHistoryInfoAndId& aInfoAndId) {
    mId = aInfoAndId.mId;
    mInfo = MakeUnique<SessionHistoryInfo>(*aInfoAndId.mInfo);
    return *this;
  }
  bool operator==(const SessionHistoryInfoAndId& aInfoAndId) const {
    return mId == aInfoAndId.mId && !mInfo == !aInfoAndId.mInfo &&
           *mInfo == *aInfoAndId.mInfo;
  }

  uint64_t mId = 0;
  UniquePtr<mozilla::dom::SessionHistoryInfo> mInfo;
};

// SessionHistoryEntry is used to store session history data in the parent
// process. It holds a SessionHistoryInfo, some state shared amongst multiple
// SessionHistoryEntries, a parent and children.
class SessionHistoryEntry : public nsISHEntry {
 public:
  SessionHistoryEntry(nsISHistory* aSessionHistory,
                      nsDocShellLoadState* aLoadState, nsIChannel* aChannel);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISHENTRY

  const SessionHistoryInfo& GetInfo() const { return *mInfo; }

 private:
  virtual ~SessionHistoryEntry() = default;

  UniquePtr<SessionHistoryInfo> mInfo;
  RefPtr<SHEntrySharedParentState> mSharedInfo;
  nsISHEntry* mParent = nullptr;
  uint32_t mID;
  nsTArray<RefPtr<SessionHistoryEntry>> mChildren;
};

}  // namespace dom

// Allow sending SessionHistoryInfo objects over IPC.
namespace ipc {

template <>
struct IPDLParamTraits<dom::SessionHistoryInfoAndId> {
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const dom::SessionHistoryInfoAndId& aParam);
  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, dom::SessionHistoryInfoAndId* aResult);
};

}  // namespace ipc

}  // namespace mozilla

#endif /* mozilla_dom_SessionHistoryEntry_h */
