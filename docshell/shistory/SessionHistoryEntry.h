/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionHistoryEntry_h
#define mozilla_dom_SessionHistoryEntry_h

#include "mozilla/UniquePtr.h"
#include "nsILayoutHistoryState.h"
#include "nsISHEntry.h"
#include "nsStructuredCloneContainer.h"
#include "nsDataHashtable.h"

class nsDocShellLoadState;
class nsIChannel;
class nsIInputStream;
class nsIReferrerInfo;
class nsISHistory;
class nsIURI;

namespace mozilla {
namespace dom {

struct LoadingSessionHistoryInfo;
class SessionHistoryEntry;
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

  nsIInputStream* GetPostData() const { return mPostData; }
  void GetScrollPosition(int32_t* aScrollPositionX, int32_t* aScrollPositionY) {
    *aScrollPositionX = mScrollPositionX;
    *aScrollPositionY = mScrollPositionY;
  }
  bool GetScrollRestorationIsManual() const {
    return mScrollRestorationIsManual;
  }
  const nsAString& GetTitle() { return mTitle; }
  void SetTitle(const nsAString& aTitle) { mTitle = aTitle; }

  void SetScrollRestorationIsManual(bool aIsManual) {
    mScrollRestorationIsManual = aIsManual;
  }

  void SetCacheKey(uint32_t aCacheKey) { mCacheKey = aCacheKey; }

  nsIURI* GetURI() const { return mURI; }

  bool GetURIWasModified() const { return mURIWasModified; }

  nsILayoutHistoryState* GetLayoutHistoryState() { return mLayoutHistoryState; }

  void SetLayoutHistoryState(nsILayoutHistoryState* aState) {
    mLayoutHistoryState = aState;
  }

 private:
  friend class SessionHistoryEntry;
  friend struct mozilla::ipc::IPDLParamTraits<LoadingSessionHistoryInfo>;

  void MaybeUpdateTitleFromURI();

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
  // mLayoutHistoryState is used to serialize layout history state across
  // IPC. In the parent process this is then synchronized to
  // SHEntrySharedParentState::mLayoutHistoryState
  nsCOMPtr<nsILayoutHistoryState> mLayoutHistoryState;

  // mCacheKey is handled similar way to mLayoutHistoryState.
  uint32_t mCacheKey = 0;

  bool mLoadReplace = false;
  bool mURIWasModified = false;
  bool mIsSrcdocEntry = false;
  bool mScrollRestorationIsManual = false;
  bool mPersist = false;
};

struct LoadingSessionHistoryInfo {
  LoadingSessionHistoryInfo() = default;
  explicit LoadingSessionHistoryInfo(SessionHistoryEntry* aEntry);

  SessionHistoryInfo mInfo;

  uint64_t mLoadId = 0;

  // The following three member variables are used to inform about a load from
  // the session history. The session-history-in-child approach has just
  // an nsISHEntry in the nsDocShellLoadState and access to the nsISHistory,
  // but session-history-in-parent needs to pass needed information explicitly
  // to the relevant child process.
  bool mIsLoadFromSessionHistory = false;
  // mRequestedIndex and mSessionHistoryLength are relevant
  // only if mIsLoadFromSessionHistory is true.
  int32_t mRequestedIndex = -1;
  int32_t mSessionHistoryLength = 0;
};

// SessionHistoryEntry is used to store session history data in the parent
// process. It holds a SessionHistoryInfo, some state shared amongst multiple
// SessionHistoryEntries, a parent and children.
#define NS_SESSIONHISTORYENTRY_IID                   \
  {                                                  \
    0x5b66a244, 0x8cec, 0x4caa, {                    \
      0xaa, 0x0a, 0x78, 0x92, 0xfd, 0x17, 0xa6, 0x67 \
    }                                                \
  }

class SessionHistoryEntry : public nsISHEntry {
 public:
  SessionHistoryEntry(nsDocShellLoadState* aLoadState, nsIChannel* aChannel);
  SessionHistoryEntry();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISHENTRY
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SESSIONHISTORYENTRY_IID)

  const SessionHistoryInfo& Info() const { return *mInfo; }

  void AddChild(SessionHistoryEntry* aChild, int32_t aOffset,
                bool aUseRemoteSubframes);
  void RemoveChild(SessionHistoryEntry* aChild);
  // Finds the child with the same docshell ID as aNewChild, replaces it with
  // aNewChild and returns true. If there is no child with the same docshell ID
  // then it returns false.
  bool ReplaceChild(SessionHistoryEntry* aNewChild);

  // Get an entry based on LoadingSessionHistoryInfo's mLoadId. Parent process
  // only.
  static SessionHistoryEntry* GetByLoadId(uint64_t aLoadId);
  static void RemoveLoadId(uint64_t aLoadId);

  static void MaybeSynchronizeSharedStateToInfo(nsISHEntry* aEntry);

 private:
  friend struct LoadingSessionHistoryInfo;
  virtual ~SessionHistoryEntry();

  const nsID& DocshellID() const;

  UniquePtr<SessionHistoryInfo> mInfo;
  RefPtr<SHEntrySharedParentState> mSharedInfo;
  nsISHEntry* mParent = nullptr;
  uint32_t mID;
  nsTArray<RefPtr<SessionHistoryEntry>> mChildren;

  static nsDataHashtable<nsUint64HashKey, SessionHistoryEntry*>* sLoadIdToEntry;
};

NS_DEFINE_STATIC_IID_ACCESSOR(SessionHistoryEntry, NS_SESSIONHISTORYENTRY_IID)

}  // namespace dom

namespace ipc {

// Allow sending LoadingSessionHistoryInfo objects over IPC.
template <>
struct IPDLParamTraits<dom::LoadingSessionHistoryInfo> {
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const dom::LoadingSessionHistoryInfo& aParam);
  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, dom::LoadingSessionHistoryInfo* aResult);
};

// Allow sending nsILayoutHistoryState objects over IPC.
template <>
struct IPDLParamTraits<nsILayoutHistoryState*> {
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    nsILayoutHistoryState* aParam);
  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, RefPtr<nsILayoutHistoryState>* aResult);
};

}  // namespace ipc

}  // namespace mozilla

#endif /* mozilla_dom_SessionHistoryEntry_h */
