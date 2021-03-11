/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionHistoryEntry.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "nsDocShell.h"
#include "nsDocShellLoadState.h"
#include "nsFrameLoader.h"
#include "nsIHttpChannel.h"
#include "nsIXULRuntime.h"
#include "nsSHEntryShared.h"
#include "nsSHistory.h"
#include "nsStructuredCloneContainer.h"
#include "nsXULAppAPI.h"
#include "mozilla/PresState.h"
#include "mozilla/StaticPrefs_fission.h"
#include "mozilla/Tuple.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/CSPMessageUtils.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/nsCSPContext.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/ReferrerInfoUtils.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/URIUtils.h"

extern mozilla::LazyLogModule gSHLog;

namespace mozilla {
namespace dom {

SessionHistoryInfo::SessionHistoryInfo(nsDocShellLoadState* aLoadState,
                                       nsIChannel* aChannel)
    : mURI(aLoadState->URI()),
      mOriginalURI(aLoadState->OriginalURI()),
      mResultPrincipalURI(aLoadState->ResultPrincipalURI()),
      mPostData(aLoadState->PostDataStream()),
      mLoadType(aLoadState->LoadType()),
      mSrcdocData(aLoadState->SrcdocData()),
      mBaseURI(aLoadState->BaseURI()),
      mLoadReplace(aLoadState->LoadReplace()),
      /* FIXME Should this be aLoadState->IsSrcdocLoad()? */
      mIsSrcdocEntry(!aLoadState->SrcdocData().IsEmpty()),
      mHasUserInteraction(false),
      mSharedState(SharedState::Create(
          aLoadState->TriggeringPrincipal(), aLoadState->PrincipalToInherit(),
          aLoadState->PartitionedPrincipalToInherit(), aLoadState->Csp(),
          /* FIXME Is this correct? */
          aLoadState->TypeHint())) {
  if (nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel)) {
    mReferrerInfo = httpChannel->GetReferrerInfo();
  }

  MaybeUpdateTitleFromURI();
}

SessionHistoryInfo::SessionHistoryInfo(
    const SessionHistoryInfo& aSharedStateFrom, nsIURI* aURI)
    : mURI(aURI), mSharedState(aSharedStateFrom.mSharedState) {
  MaybeUpdateTitleFromURI();
}

SessionHistoryInfo::SessionHistoryInfo(
    nsIURI* aURI, nsIPrincipal* aTriggeringPrincipal,
    nsIPrincipal* aPrincipalToInherit,
    nsIPrincipal* aPartitionedPrincipalToInherit,
    nsIContentSecurityPolicy* aCsp, const nsACString& aContentType)
    : mURI(aURI),
      mSharedState(SharedState::Create(
          aTriggeringPrincipal, aPrincipalToInherit,
          aPartitionedPrincipalToInherit, aCsp, aContentType)) {
  MaybeUpdateTitleFromURI();
}

SessionHistoryInfo::SessionHistoryInfo(
    nsIChannel* aChannel, uint32_t aLoadType,
    nsIPrincipal* aPartitionedPrincipalToInherit,
    nsIContentSecurityPolicy* aCsp) {
  aChannel->GetURI(getter_AddRefs(mURI));
  mLoadType = aLoadType;

  nsCOMPtr<nsILoadInfo> loadInfo;
  aChannel->GetLoadInfo(getter_AddRefs(loadInfo));

  loadInfo->GetResultPrincipalURI(getter_AddRefs(mResultPrincipalURI));
  loadInfo->GetTriggeringPrincipal(
      getter_AddRefs(mSharedState.Get()->mTriggeringPrincipal));
  loadInfo->GetPrincipalToInherit(
      getter_AddRefs(mSharedState.Get()->mPrincipalToInherit));

  mSharedState.Get()->mPartitionedPrincipalToInherit =
      aPartitionedPrincipalToInherit;
  mSharedState.Get()->mCsp = aCsp;
  aChannel->GetContentType(mSharedState.Get()->mContentType);
  aChannel->GetOriginalURI(getter_AddRefs(mOriginalURI));

  uint32_t loadFlags;
  aChannel->GetLoadFlags(&loadFlags);
  mLoadReplace = !!(loadFlags & nsIChannel::LOAD_REPLACE);

  MaybeUpdateTitleFromURI();

  if (nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel)) {
    mReferrerInfo = httpChannel->GetReferrerInfo();
  }
}

void SessionHistoryInfo::Reset(nsIURI* aURI, const nsID& aDocShellID,
                               bool aDynamicCreation,
                               nsIPrincipal* aTriggeringPrincipal,
                               nsIPrincipal* aPrincipalToInherit,
                               nsIPrincipal* aPartitionedPrincipalToInherit,
                               nsIContentSecurityPolicy* aCsp,
                               const nsACString& aContentType) {
  mURI = aURI;
  mOriginalURI = nullptr;
  mResultPrincipalURI = nullptr;
  mReferrerInfo = nullptr;
  // Default title is the URL.
  nsAutoCString spec;
  if (NS_SUCCEEDED(mURI->GetSpec(spec))) {
    CopyUTF8toUTF16(spec, mTitle);
  }
  mPostData = nullptr;
  mLoadType = 0;
  mScrollPositionX = 0;
  mScrollPositionY = 0;
  mStateData = nullptr;
  mSrcdocData.Truncate();
  mBaseURI = nullptr;
  mLoadReplace = false;
  mURIWasModified = false;
  mIsSrcdocEntry = false;
  mScrollRestorationIsManual = false;
  mPersist = false;
  mHasUserInteraction = false;

  mSharedState.Get()->mTriggeringPrincipal = aTriggeringPrincipal;
  mSharedState.Get()->mPrincipalToInherit = aPrincipalToInherit;
  mSharedState.Get()->mPartitionedPrincipalToInherit =
      aPartitionedPrincipalToInherit;
  mSharedState.Get()->mCsp = aCsp;
  mSharedState.Get()->mContentType = aContentType;
  mSharedState.Get()->mLayoutHistoryState = nullptr;
}

void SessionHistoryInfo::MaybeUpdateTitleFromURI() {
  if (mTitle.IsEmpty() && mURI) {
    // Default title is the URL.
    nsAutoCString spec;
    if (NS_SUCCEEDED(mURI->GetSpec(spec))) {
      AppendUTF8toUTF16(spec, mTitle);
    }
  }
}

uint64_t SessionHistoryInfo::SharedId() const {
  return mSharedState.Get()->mId;
}

nsILayoutHistoryState* SessionHistoryInfo::GetLayoutHistoryState() {
  return mSharedState.Get()->mLayoutHistoryState;
}

void SessionHistoryInfo::SetLayoutHistoryState(nsILayoutHistoryState* aState) {
  mSharedState.Get()->mLayoutHistoryState = aState;
}

nsIPrincipal* SessionHistoryInfo::GetTriggeringPrincipal() const {
  return mSharedState.Get()->mTriggeringPrincipal;
}

nsIPrincipal* SessionHistoryInfo::GetPrincipalToInherit() const {
  return mSharedState.Get()->mPrincipalToInherit;
}

nsIPrincipal* SessionHistoryInfo::GetPartitionedPrincipalToInherit() const {
  return mSharedState.Get()->mPartitionedPrincipalToInherit;
}

nsIContentSecurityPolicy* SessionHistoryInfo::GetCsp() const {
  return mSharedState.Get()->mCsp;
}

uint32_t SessionHistoryInfo::GetCacheKey() const {
  return mSharedState.Get()->mCacheKey;
}

void SessionHistoryInfo::SetCacheKey(uint32_t aCacheKey) {
  mSharedState.Get()->mCacheKey = aCacheKey;
}

bool SessionHistoryInfo::IsSubFrame() const {
  return mSharedState.Get()->mIsFrameNavigation;
}

void SessionHistoryInfo::SetSaveLayoutStateFlag(bool aSaveLayoutStateFlag) {
  MOZ_ASSERT(XRE_IsParentProcess());
  static_cast<SHEntrySharedParentState*>(mSharedState.Get())->mSaveLayoutState =
      aSaveLayoutStateFlag;
}

void SessionHistoryInfo::FillLoadInfo(nsDocShellLoadState& aLoadState) const {
  aLoadState.SetOriginalURI(mOriginalURI);
  aLoadState.SetMaybeResultPrincipalURI(Some(mResultPrincipalURI));
  aLoadState.SetLoadReplace(mLoadReplace);
  aLoadState.SetPostDataStream(mPostData);
  aLoadState.SetReferrerInfo(mReferrerInfo);

  aLoadState.SetTypeHint(mSharedState.Get()->mContentType);
  aLoadState.SetTriggeringPrincipal(mSharedState.Get()->mTriggeringPrincipal);
  aLoadState.SetPrincipalToInherit(mSharedState.Get()->mPrincipalToInherit);
  aLoadState.SetPartitionedPrincipalToInherit(
      mSharedState.Get()->mPartitionedPrincipalToInherit);
  aLoadState.SetCsp(mSharedState.Get()->mCsp);

  // Do not inherit principal from document (security-critical!);
  uint32_t flags = nsDocShell::InternalLoad::INTERNAL_LOAD_FLAGS_NONE;

  // Passing nullptr as aSourceDocShell gives the same behaviour as before
  // aSourceDocShell was introduced. According to spec we should be passing
  // the source browsing context that was used when the history entry was
  // first created. bug 947716 has been created to address this issue.
  nsAutoString srcdoc;
  nsCOMPtr<nsIURI> baseURI;
  if (mIsSrcdocEntry) {
    srcdoc = mSrcdocData;
    baseURI = mBaseURI;
    flags |= nsDocShell::InternalLoad::INTERNAL_LOAD_FLAGS_IS_SRCDOC;
  } else {
    srcdoc = VoidString();
  }
  aLoadState.SetSrcdocData(srcdoc);
  aLoadState.SetBaseURI(baseURI);
  aLoadState.SetInternalLoadFlags(flags);

  aLoadState.SetFirstParty(true);
}
/* static */
SessionHistoryInfo::SharedState SessionHistoryInfo::SharedState::Create(
    nsIPrincipal* aTriggeringPrincipal, nsIPrincipal* aPrincipalToInherit,
    nsIPrincipal* aPartitionedPrincipalToInherit,
    nsIContentSecurityPolicy* aCsp, const nsACString& aContentType) {
  if (XRE_IsParentProcess()) {
    return SharedState(new SHEntrySharedParentState(
        aTriggeringPrincipal, aPrincipalToInherit,
        aPartitionedPrincipalToInherit, aCsp, aContentType));
  }

  return SharedState(MakeUnique<SHEntrySharedState>(
      aTriggeringPrincipal, aPrincipalToInherit, aPartitionedPrincipalToInherit,
      aCsp, aContentType));
}

SessionHistoryInfo::SharedState::SharedState() { Init(); }

SessionHistoryInfo::SharedState::SharedState(
    const SessionHistoryInfo::SharedState& aOther) {
  Init(aOther);
}

SessionHistoryInfo::SharedState::SharedState(
    const Maybe<const SessionHistoryInfo::SharedState&>& aOther) {
  if (aOther.isSome()) {
    Init(aOther.ref());
  } else {
    Init();
  }
}

SessionHistoryInfo::SharedState::~SharedState() {
  if (XRE_IsParentProcess()) {
    mParent
        .RefPtr<SHEntrySharedParentState>::~RefPtr<SHEntrySharedParentState>();
  } else {
    mChild.UniquePtr<SHEntrySharedState>::~UniquePtr<SHEntrySharedState>();
  }
}

SessionHistoryInfo::SharedState& SessionHistoryInfo::SharedState::operator=(
    const SessionHistoryInfo::SharedState& aOther) {
  if (this != &aOther) {
    if (XRE_IsParentProcess()) {
      mParent = aOther.mParent;
    } else {
      mChild = MakeUnique<SHEntrySharedState>(*aOther.mChild);
    }
  }
  return *this;
}

SHEntrySharedState* SessionHistoryInfo::SharedState::Get() const {
  if (XRE_IsParentProcess()) {
    return mParent;
  }

  return mChild.get();
}

void SessionHistoryInfo::SharedState::ChangeId(uint64_t aId) {
  if (XRE_IsParentProcess()) {
    mParent->ChangeId(aId);
  } else {
    mChild->mId = aId;
  }
}

void SessionHistoryInfo::SharedState::Init() {
  if (XRE_IsParentProcess()) {
    new (&mParent)
        RefPtr<SHEntrySharedParentState>(new SHEntrySharedParentState());
  } else {
    new (&mChild)
        UniquePtr<SHEntrySharedState>(MakeUnique<SHEntrySharedState>());
  }
}

void SessionHistoryInfo::SharedState::Init(
    const SessionHistoryInfo::SharedState& aOther) {
  if (XRE_IsParentProcess()) {
    new (&mParent) RefPtr<SHEntrySharedParentState>(aOther.mParent);
  } else {
    new (&mChild) UniquePtr<SHEntrySharedState>(
        MakeUnique<SHEntrySharedState>(*aOther.mChild));
  }
}

static uint64_t gLoadingSessionHistoryInfoLoadId = 0;

nsTHashMap<nsUint64HashKey, SessionHistoryEntry*>*
    SessionHistoryEntry::sLoadIdToEntry = nullptr;

LoadingSessionHistoryInfo::LoadingSessionHistoryInfo(
    SessionHistoryEntry* aEntry)
    : mInfo(aEntry->Info()), mLoadId(++gLoadingSessionHistoryInfoLoadId) {
  SessionHistoryEntry::SetByLoadId(mLoadId, aEntry);
}

LoadingSessionHistoryInfo::LoadingSessionHistoryInfo(
    SessionHistoryEntry* aEntry, uint64_t aLoadId)
    : mInfo(aEntry->Info()), mLoadId(aLoadId) {
  MOZ_ASSERT(SessionHistoryEntry::sLoadIdToEntry &&
             SessionHistoryEntry::sLoadIdToEntry->Get(aLoadId) == aEntry);
}

already_AddRefed<nsDocShellLoadState>
LoadingSessionHistoryInfo::CreateLoadInfo() const {
  RefPtr<nsDocShellLoadState> loadState(
      new nsDocShellLoadState(mInfo.GetURI()));

  mInfo.FillLoadInfo(*loadState);

  loadState->SetLoadingSessionHistoryInfo(*this);

  return loadState.forget();
}

static uint32_t gEntryID;

SessionHistoryEntry* SessionHistoryEntry::GetByLoadId(uint64_t aLoadId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!sLoadIdToEntry) {
    return nullptr;
  }

  return sLoadIdToEntry->Get(aLoadId);
}

void SessionHistoryEntry::SetByLoadId(uint64_t aLoadId,
                                      SessionHistoryEntry* aEntry) {
  if (!sLoadIdToEntry) {
    sLoadIdToEntry = new nsTHashMap<nsUint64HashKey, SessionHistoryEntry*>();
  }

  MOZ_LOG(
      gSHLog, LogLevel::Verbose,
      ("SessionHistoryEntry::SetByLoadId(%" PRIu64 " - %p)", aLoadId, aEntry));
  sLoadIdToEntry->InsertOrUpdate(aLoadId, aEntry);
}

void SessionHistoryEntry::RemoveLoadId(uint64_t aLoadId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!sLoadIdToEntry) {
    return;
  }

  MOZ_LOG(gSHLog, LogLevel::Verbose,
          ("SHEntry::RemoveLoadId(%" PRIu64 ")", aLoadId));
  sLoadIdToEntry->Remove(aLoadId);
}

SessionHistoryEntry::SessionHistoryEntry()
    : mInfo(new SessionHistoryInfo()), mID(++gEntryID) {
  MOZ_ASSERT(mozilla::SessionHistoryInParent());
}

SessionHistoryEntry::SessionHistoryEntry(nsDocShellLoadState* aLoadState,
                                         nsIChannel* aChannel)
    : mInfo(new SessionHistoryInfo(aLoadState, aChannel)), mID(++gEntryID) {
  MOZ_ASSERT(mozilla::SessionHistoryInParent());
}

SessionHistoryEntry::SessionHistoryEntry(SessionHistoryInfo* aInfo)
    : mInfo(MakeUnique<SessionHistoryInfo>(*aInfo)), mID(++gEntryID) {
  MOZ_ASSERT(mozilla::SessionHistoryInParent());
}

SessionHistoryEntry::SessionHistoryEntry(const SessionHistoryEntry& aEntry)
    : mInfo(MakeUnique<SessionHistoryInfo>(*aEntry.mInfo)),
      mParent(aEntry.mParent),
      mID(aEntry.mID),
      mBCHistoryLength(aEntry.mBCHistoryLength) {
  MOZ_ASSERT(mozilla::SessionHistoryInParent());
}

SessionHistoryEntry::~SessionHistoryEntry() {
  // Null out the mParent pointers on all our kids.
  for (nsISHEntry* entry : mChildren) {
    if (entry) {
      entry->SetParent(nullptr);
    }
  }

  if (sLoadIdToEntry) {
    sLoadIdToEntry->RemoveIf(
        [this](auto& aIter) { return aIter.Data() == this; });
    if (sLoadIdToEntry->IsEmpty()) {
      delete sLoadIdToEntry;
      sLoadIdToEntry = nullptr;
    }
  }
}

NS_IMPL_ISUPPORTS(SessionHistoryEntry, nsISHEntry, SessionHistoryEntry)

NS_IMETHODIMP
SessionHistoryEntry::GetURI(nsIURI** aURI) {
  nsCOMPtr<nsIURI> uri = mInfo->mURI;
  uri.forget(aURI);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetURI(nsIURI* aURI) {
  mInfo->mURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetOriginalURI(nsIURI** aOriginalURI) {
  nsCOMPtr<nsIURI> originalURI = mInfo->mOriginalURI;
  originalURI.forget(aOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetOriginalURI(nsIURI* aOriginalURI) {
  mInfo->mOriginalURI = aOriginalURI;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetResultPrincipalURI(nsIURI** aResultPrincipalURI) {
  nsCOMPtr<nsIURI> resultPrincipalURI = mInfo->mResultPrincipalURI;
  resultPrincipalURI.forget(aResultPrincipalURI);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetResultPrincipalURI(nsIURI* aResultPrincipalURI) {
  mInfo->mResultPrincipalURI = aResultPrincipalURI;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetLoadReplace(bool* aLoadReplace) {
  *aLoadReplace = mInfo->mLoadReplace;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetLoadReplace(bool aLoadReplace) {
  mInfo->mLoadReplace = aLoadReplace;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetTitle(nsAString& aTitle) {
  aTitle = mInfo->mTitle;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetTitle(const nsAString& aTitle) {
  mInfo->SetTitle(aTitle);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetName(nsAString& aName) {
  aName = mInfo->mName;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetName(const nsAString& aName) {
  mInfo->mName = aName;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetIsSubFrame(bool* aIsSubFrame) {
  *aIsSubFrame = SharedInfo()->mIsFrameNavigation;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetIsSubFrame(bool aIsSubFrame) {
  SharedInfo()->mIsFrameNavigation = aIsSubFrame;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetHasUserInteraction(bool* aFlag) {
  // The back button and menulist deal with root/top-level
  // session history entries, thus we annotate only the root entry.
  if (!mParent) {
    *aFlag = mInfo->mHasUserInteraction;
  } else {
    nsCOMPtr<nsISHEntry> root = nsSHistory::GetRootSHEntry(this);
    root->GetHasUserInteraction(aFlag);
  }
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetHasUserInteraction(bool aFlag) {
  // The back button and menulist deal with root/top-level
  // session history entries, thus we annotate only the root entry.
  if (!mParent) {
    mInfo->mHasUserInteraction = aFlag;
  } else {
    nsCOMPtr<nsISHEntry> root = nsSHistory::GetRootSHEntry(this);
    root->SetHasUserInteraction(aFlag);
  }
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetReferrerInfo(nsIReferrerInfo** aReferrerInfo) {
  nsCOMPtr<nsIReferrerInfo> referrerInfo = mInfo->mReferrerInfo;
  referrerInfo.forget(aReferrerInfo);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetReferrerInfo(nsIReferrerInfo* aReferrerInfo) {
  mInfo->mReferrerInfo = aReferrerInfo;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetContentViewer(nsIContentViewer** aContentViewer) {
  *aContentViewer = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetContentViewer(nsIContentViewer* aContentViewer) {
  MOZ_CRASH("This lives in the child process");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SessionHistoryEntry::GetSticky(bool* aSticky) {
  *aSticky = SharedInfo()->mSticky;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetSticky(bool aSticky) {
  SharedInfo()->mSticky = aSticky;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetWindowState(nsISupports** aWindowState) {
  MOZ_CRASH("This lives in the child process");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SessionHistoryEntry::SetWindowState(nsISupports* aWindowState) {
  MOZ_CRASH("This lives in the child process");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SessionHistoryEntry::GetRefreshURIList(nsIMutableArray** aRefreshURIList) {
  MOZ_CRASH("This lives in the child process");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SessionHistoryEntry::SetRefreshURIList(nsIMutableArray* aRefreshURIList) {
  MOZ_CRASH("This lives in the child process");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SessionHistoryEntry::GetPostData(nsIInputStream** aPostData) {
  nsCOMPtr<nsIInputStream> postData = mInfo->mPostData;
  postData.forget(aPostData);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetPostData(nsIInputStream* aPostData) {
  mInfo->mPostData = aPostData;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetLayoutHistoryState(
    nsILayoutHistoryState** aLayoutHistoryState) {
  nsCOMPtr<nsILayoutHistoryState> layoutHistoryState =
      SharedInfo()->mLayoutHistoryState;
  layoutHistoryState.forget(aLayoutHistoryState);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetLayoutHistoryState(
    nsILayoutHistoryState* aLayoutHistoryState) {
  SharedInfo()->mLayoutHistoryState = aLayoutHistoryState;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetParent(nsISHEntry** aParent) {
  nsCOMPtr<nsISHEntry> parent = mParent;
  parent.forget(aParent);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetParent(nsISHEntry* aParent) {
  mParent = aParent;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetLoadType(uint32_t* aLoadType) {
  *aLoadType = mInfo->mLoadType;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetLoadType(uint32_t aLoadType) {
  mInfo->mLoadType = aLoadType;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetID(uint32_t* aID) {
  *aID = mID;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetID(uint32_t aID) {
  mID = aID;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetCacheKey(uint32_t* aCacheKey) {
  *aCacheKey = SharedInfo()->mCacheKey;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetCacheKey(uint32_t aCacheKey) {
  SharedInfo()->mCacheKey = aCacheKey;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetSaveLayoutStateFlag(bool* aSaveLayoutStateFlag) {
  *aSaveLayoutStateFlag = SharedInfo()->mSaveLayoutState;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetSaveLayoutStateFlag(bool aSaveLayoutStateFlag) {
  SharedInfo()->mSaveLayoutState = aSaveLayoutStateFlag;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetContentType(nsACString& aContentType) {
  aContentType = SharedInfo()->mContentType;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetContentType(const nsACString& aContentType) {
  SharedInfo()->mContentType = aContentType;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetURIWasModified(bool* aURIWasModified) {
  *aURIWasModified = mInfo->mURIWasModified;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetURIWasModified(bool aURIWasModified) {
  mInfo->mURIWasModified = aURIWasModified;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetTriggeringPrincipal(
    nsIPrincipal** aTriggeringPrincipal) {
  nsCOMPtr<nsIPrincipal> triggeringPrincipal =
      SharedInfo()->mTriggeringPrincipal;
  triggeringPrincipal.forget(aTriggeringPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetTriggeringPrincipal(
    nsIPrincipal* aTriggeringPrincipal) {
  SharedInfo()->mTriggeringPrincipal = aTriggeringPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetPrincipalToInherit(nsIPrincipal** aPrincipalToInherit) {
  nsCOMPtr<nsIPrincipal> principalToInherit = SharedInfo()->mPrincipalToInherit;
  principalToInherit.forget(aPrincipalToInherit);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetPrincipalToInherit(nsIPrincipal* aPrincipalToInherit) {
  SharedInfo()->mPrincipalToInherit = aPrincipalToInherit;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetPartitionedPrincipalToInherit(
    nsIPrincipal** aPartitionedPrincipalToInherit) {
  nsCOMPtr<nsIPrincipal> partitionedPrincipalToInherit =
      SharedInfo()->mPartitionedPrincipalToInherit;
  partitionedPrincipalToInherit.forget(aPartitionedPrincipalToInherit);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetPartitionedPrincipalToInherit(
    nsIPrincipal* aPartitionedPrincipalToInherit) {
  SharedInfo()->mPartitionedPrincipalToInherit = aPartitionedPrincipalToInherit;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetCsp(nsIContentSecurityPolicy** aCsp) {
  nsCOMPtr<nsIContentSecurityPolicy> csp = SharedInfo()->mCsp;
  csp.forget(aCsp);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetCsp(nsIContentSecurityPolicy* aCsp) {
  SharedInfo()->mCsp = aCsp;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetStateData(nsIStructuredCloneContainer** aStateData) {
  RefPtr<nsStructuredCloneContainer> stateData = mInfo->mStateData;
  stateData.forget(aStateData);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetStateData(nsIStructuredCloneContainer* aStateData) {
  mInfo->mStateData = static_cast<nsStructuredCloneContainer*>(aStateData);
  return NS_OK;
}

const nsID& SessionHistoryEntry::DocshellID() const {
  return SharedInfo()->mDocShellID;
}

NS_IMETHODIMP
SessionHistoryEntry::GetDocshellID(nsID& aDocshellID) {
  aDocshellID = DocshellID();
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetDocshellID(const nsID& aDocshellID) {
  SharedInfo()->mDocShellID = aDocshellID;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetIsSrcdocEntry(bool* aIsSrcdocEntry) {
  *aIsSrcdocEntry = mInfo->mIsSrcdocEntry;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetSrcdocData(nsAString& aSrcdocData) {
  aSrcdocData = mInfo->mSrcdocData;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetSrcdocData(const nsAString& aSrcdocData) {
  mInfo->mSrcdocData = aSrcdocData;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetBaseURI(nsIURI** aBaseURI) {
  nsCOMPtr<nsIURI> baseURI = mInfo->mBaseURI;
  baseURI.forget(aBaseURI);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetBaseURI(nsIURI* aBaseURI) {
  mInfo->mBaseURI = aBaseURI;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetScrollRestorationIsManual(
    bool* aScrollRestorationIsManual) {
  *aScrollRestorationIsManual = mInfo->mScrollRestorationIsManual;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetScrollRestorationIsManual(
    bool aScrollRestorationIsManual) {
  mInfo->mScrollRestorationIsManual = aScrollRestorationIsManual;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetLoadedInThisProcess(bool* aLoadedInThisProcess) {
  // FIXME
  //*aLoadedInThisProcess = mInfo->mLoadedInThisProcess;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetShistory(nsISHistory** aShistory) {
  nsCOMPtr<nsISHistory> sHistory = do_QueryReferent(SharedInfo()->mSHistory);
  sHistory.forget(aShistory);
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetShistory(nsISHistory* aShistory) {
  nsWeakPtr shistory = do_GetWeakReference(aShistory);
  // mSHistory can not be changed once it's set
  MOZ_ASSERT(!SharedInfo()->mSHistory || (SharedInfo()->mSHistory == shistory));
  SharedInfo()->mSHistory = shistory;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetLastTouched(uint32_t* aLastTouched) {
  *aLastTouched = SharedInfo()->mLastTouched;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetLastTouched(uint32_t aLastTouched) {
  SharedInfo()->mLastTouched = aLastTouched;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetChildCount(int32_t* aChildCount) {
  *aChildCount = mChildren.Length();
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetPersist(bool* aPersist) {
  *aPersist = mInfo->mPersist;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetPersist(bool aPersist) {
  mInfo->mPersist = aPersist;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetScrollPosition(int32_t* aX, int32_t* aY) {
  *aX = mInfo->mScrollPositionX;
  *aY = mInfo->mScrollPositionY;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetScrollPosition(int32_t aX, int32_t aY) {
  mInfo->mScrollPositionX = aX;
  mInfo->mScrollPositionY = aY;
  return NS_OK;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::GetViewerBounds(nsIntRect& bounds) {
  bounds = SharedInfo()->mViewerBounds;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::SetViewerBounds(const nsIntRect& bounds) {
  SharedInfo()->mViewerBounds = bounds;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::AddChildShell(nsIDocShellTreeItem* shell) {
  MOZ_CRASH("This lives in the child process");
}

NS_IMETHODIMP
SessionHistoryEntry::ChildShellAt(int32_t index,
                                  nsIDocShellTreeItem** _retval) {
  MOZ_CRASH("This lives in the child process");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::ClearChildShells() {
  MOZ_CRASH("This lives in the child process");
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::SyncPresentationState() {
  MOZ_CRASH("This lives in the child process");
}

NS_IMETHODIMP
SessionHistoryEntry::InitLayoutHistoryState(
    nsILayoutHistoryState** aLayoutHistoryState) {
  if (!SharedInfo()->mLayoutHistoryState) {
    nsCOMPtr<nsILayoutHistoryState> historyState;
    historyState = NS_NewLayoutHistoryState();
    SetLayoutHistoryState(historyState);
  }

  return GetLayoutHistoryState(aLayoutHistoryState);
}

NS_IMETHODIMP
SessionHistoryEntry::Create(
    nsIURI* aURI, const nsAString& aTitle, nsIInputStream* aInputStream,
    uint32_t aCacheKey, const nsACString& aContentType,
    nsIPrincipal* aTriggeringPrincipal, nsIPrincipal* aPrincipalToInherit,
    nsIPrincipal* aPartitionedPrincipalToInherit,
    nsIContentSecurityPolicy* aCsp, const nsID& aDocshellID,
    bool aDynamicCreation, nsIURI* aOriginalURI, nsIURI* aResultPrincipalURI,
    bool aLoadReplace, nsIReferrerInfo* aReferrerInfo, const nsAString& aSrcdoc,
    bool aSrcdocEntry, nsIURI* aBaseURI, bool aSaveLayoutState, bool aExpired) {
  MOZ_CRASH("Might need to implement this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SessionHistoryEntry::Clone(nsISHEntry** aEntry) {
  RefPtr<SessionHistoryEntry> entry = new SessionHistoryEntry(*this);

  // These are not copied for some reason, we're not sure why.
  entry->mInfo->mLoadType = 0;
  entry->mInfo->mScrollPositionX = 0;
  entry->mInfo->mScrollPositionY = 0;
  entry->mInfo->mScrollRestorationIsManual = false;

  entry->mInfo->mHasUserInteraction = false;

  entry.forget(aEntry);

  return NS_OK;
}

NS_IMETHODIMP_(nsDocShellEditorData*)
SessionHistoryEntry::ForgetEditorData() {
  MOZ_CRASH("This lives in the child process");
  return nullptr;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::SetEditorData(nsDocShellEditorData* aData) {
  NS_WARNING("This lives in the child process");
}

NS_IMETHODIMP_(bool)
SessionHistoryEntry::HasDetachedEditor() {
  NS_WARNING("This lives in the child process");
  return false;
}

NS_IMETHODIMP_(bool)
SessionHistoryEntry::IsDynamicallyAdded() {
  return SharedInfo()->mDynamicallyCreated;
}

void SessionHistoryEntry::SetIsDynamicallyAdded(bool aDynamic) {
  MOZ_ASSERT_IF(SharedInfo()->mDynamicallyCreated, aDynamic);
  SharedInfo()->mDynamicallyCreated = aDynamic;
}

NS_IMETHODIMP
SessionHistoryEntry::HasDynamicallyAddedChild(bool* aHasDynamicallyAddedChild) {
  for (const auto& child : mChildren) {
    if (child && child->IsDynamicallyAdded()) {
      *aHasDynamicallyAddedChild = true;
      return NS_OK;
    }
  }
  *aHasDynamicallyAddedChild = false;
  return NS_OK;
}

NS_IMETHODIMP_(bool)
SessionHistoryEntry::HasBFCacheEntry(nsIBFCacheEntry* aEntry) {
  MOZ_CRASH("This lives in the child process");
  return false;
}

NS_IMETHODIMP
SessionHistoryEntry::AdoptBFCacheEntry(nsISHEntry* aEntry) {
  nsCOMPtr<SessionHistoryEntry> she = do_QueryInterface(aEntry);
  NS_ENSURE_STATE(she && she->mInfo->mSharedState.Get());

  mInfo->mSharedState =
      static_cast<SessionHistoryEntry*>(aEntry)->mInfo->mSharedState;

  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::AbandonBFCacheEntry() {
  MOZ_CRASH("This lives in the child process");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SessionHistoryEntry::SharesDocumentWith(nsISHEntry* aEntry,
                                        bool* aSharesDocumentWith) {
  SessionHistoryEntry* entry = static_cast<SessionHistoryEntry*>(aEntry);

  MOZ_ASSERT_IF(entry->SharedInfo() != SharedInfo(),
                entry->SharedInfo()->GetId() != SharedInfo()->GetId());

  *aSharesDocumentWith = entry->SharedInfo() == SharedInfo();
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::SetLoadTypeAsHistory() {
  mInfo->mLoadType = LOAD_HISTORY;
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::AddChild(nsISHEntry* aChild, int32_t aOffset,
                              bool aUseRemoteSubframes) {
  nsCOMPtr<SessionHistoryEntry> child = do_QueryInterface(aChild);
  MOZ_ASSERT_IF(aChild, child);
  AddChild(child, aOffset, aUseRemoteSubframes);

  return NS_OK;
}

void SessionHistoryEntry::AddChild(SessionHistoryEntry* aChild, int32_t aOffset,
                                   bool aUseRemoteSubframes) {
  if (aChild) {
    aChild->SetParent(this);
  }

  if (aOffset < 0) {
    mChildren.AppendElement(aChild);
    return;
  }

  //
  // Bug 52670: Ensure children are added in order.
  //
  //  Later frames in the child list may load faster and get appended
  //  before earlier frames, causing session history to be scrambled.
  //  By growing the list here, they are added to the right position.

  int32_t length = mChildren.Length();

  //  Assert that aOffset will not be so high as to grow us a lot.
  NS_ASSERTION(aOffset < length + 1023, "Large frames array!\n");

  // If the new child is dynamically added, try to add it to aOffset, but if
  // there are non-dynamically added children, the child must be after those.
  if (aChild && aChild->IsDynamicallyAdded()) {
    int32_t lastNonDyn = aOffset - 1;
    for (int32_t i = aOffset; i < length; ++i) {
      SessionHistoryEntry* entry = mChildren[i];
      if (entry) {
        if (entry->IsDynamicallyAdded()) {
          break;
        }

        lastNonDyn = i;
      }
    }

    // If aOffset is larger than Length(), we must first truncate the array.
    if (aOffset > length) {
      mChildren.SetLength(aOffset);
    }

    mChildren.InsertElementAt(lastNonDyn + 1, aChild);

    return;
  }

  // If the new child isn't dynamically added, it should be set to aOffset.
  // If there are dynamically added children before that, those must be moved
  // to be after aOffset.
  if (length > 0) {
    int32_t start = std::min(length - 1, aOffset);
    int32_t dynEntryIndex = -1;
    DebugOnly<SessionHistoryEntry*> dynEntry = nullptr;
    for (int32_t i = start; i >= 0; --i) {
      SessionHistoryEntry* entry = mChildren[i];
      if (entry) {
        if (!entry->IsDynamicallyAdded()) {
          break;
        }

        dynEntryIndex = i;
        dynEntry = entry;
      }
    }

    if (dynEntryIndex >= 0) {
      mChildren.InsertElementsAt(dynEntryIndex, aOffset - dynEntryIndex + 1);
      NS_ASSERTION(mChildren[aOffset + 1] == dynEntry, "Whaat?");
    }
  }

  // Make sure there isn't anything at aOffset.
  if ((uint32_t)aOffset < mChildren.Length()) {
    SessionHistoryEntry* oldChild = mChildren[aOffset];
    if (oldChild && oldChild != aChild) {
      // Under Fission, this can happen when a network-created iframe starts
      // out in-process, moves out-of-process, and then switches back. At that
      // point, we'll create a new network-created DocShell at the same index
      // where we already have an entry for the original network-created
      // DocShell.
      //
      // This should ideally stop being an issue once the Fission-aware
      // session history rewrite is complete.
      NS_ASSERTION(
          aUseRemoteSubframes,
          "Adding a child where we already have a child? This may misbehave");
      oldChild->SetParent(nullptr);
    }
  } else {
    mChildren.SetLength(aOffset + 1);
  }

  mChildren.ReplaceElementAt(aOffset, aChild);
}

NS_IMETHODIMP
SessionHistoryEntry::RemoveChild(nsISHEntry* aChild) {
  NS_ENSURE_TRUE(aChild, NS_ERROR_FAILURE);

  nsCOMPtr<SessionHistoryEntry> child = do_QueryInterface(aChild);
  MOZ_ASSERT(child);
  RemoveChild(child);

  return NS_OK;
}

void SessionHistoryEntry::RemoveChild(SessionHistoryEntry* aChild) {
  bool childRemoved = false;
  if (aChild->IsDynamicallyAdded()) {
    childRemoved = mChildren.RemoveElement(aChild);
  } else {
    int32_t index = mChildren.IndexOf(aChild);
    if (index >= 0) {
      // Other alive non-dynamic child docshells still keep mChildOffset,
      // so we don't want to change the indices here.
      mChildren.ReplaceElementAt(index, nullptr);
      childRemoved = true;
    }
  }

  if (childRemoved) {
    aChild->SetParent(nullptr);

    // reduce the child count, i.e. remove empty children at the end
    for (int32_t i = mChildren.Length() - 1; i >= 0 && !mChildren[i]; --i) {
      mChildren.RemoveElementAt(i);
    }
  }
}

NS_IMETHODIMP
SessionHistoryEntry::GetChildAt(int32_t aIndex, nsISHEntry** aChild) {
  nsCOMPtr<nsISHEntry> child = mChildren.SafeElementAt(aIndex);
  child.forget(aChild);
  return NS_OK;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::GetChildSHEntryIfHasNoDynamicallyAddedChild(
    int32_t aChildOffset, nsISHEntry** aChild) {
  *aChild = nullptr;

  bool dynamicallyAddedChild = false;
  HasDynamicallyAddedChild(&dynamicallyAddedChild);
  if (dynamicallyAddedChild) {
    return;
  }

  // If the user did a shift-reload on this frameset page,
  // we don't want to load the subframes from history.
  if (IsForceReloadType(mInfo->mLoadType) || mInfo->mLoadType == LOAD_REFRESH) {
    return;
  }

  /* Before looking for the subframe's url, check
   * the expiration status of the parent. If the parent
   * has expired from cache, then subframes will not be
   * loaded from history in certain situations.
   * If the user pressed reload and the parent frame has expired
   *  from cache, we do not want to load the child frame from history.
   */
  if (SharedInfo()->mExpired && (mInfo->mLoadType == LOAD_RELOAD_NORMAL)) {
    // The parent has expired. Return null.
    *aChild = nullptr;
    return;
  }
  // Get the child subframe from session history.
  GetChildAt(aChildOffset, aChild);
  if (*aChild) {
    // Set the parent's Load Type on the child
    (*aChild)->SetLoadType(mInfo->mLoadType);
  }
}

NS_IMETHODIMP
SessionHistoryEntry::ReplaceChild(nsISHEntry* aNewChild) {
  NS_ENSURE_STATE(aNewChild);

  nsCOMPtr<SessionHistoryEntry> newChild = do_QueryInterface(aNewChild);
  MOZ_ASSERT(newChild);
  return ReplaceChild(newChild) ? NS_OK : NS_ERROR_FAILURE;
}

bool SessionHistoryEntry::ReplaceChild(SessionHistoryEntry* aNewChild) {
  const nsID& docshellID = aNewChild->DocshellID();

  for (uint32_t i = 0; i < mChildren.Length(); ++i) {
    if (mChildren[i] && docshellID == mChildren[i]->DocshellID()) {
      mChildren[i]->SetParent(nullptr);
      mChildren.ReplaceElementAt(i, aNewChild);
      aNewChild->SetParent(this);

      return true;
    }
  }

  return false;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::ClearEntry() {
  int32_t childCount = GetChildCount();
  // Remove all children of this entry
  for (int32_t i = childCount; i > 0; --i) {
    nsCOMPtr<nsISHEntry> child;
    GetChildAt(i - 1, getter_AddRefs(child));
    RemoveChild(child);
  }
}

NS_IMETHODIMP
SessionHistoryEntry::CreateLoadInfo(nsDocShellLoadState** aLoadState) {
  NS_WARNING("We shouldn't be calling this!");
  return NS_OK;
}

NS_IMETHODIMP
SessionHistoryEntry::GetBfcacheID(uint64_t* aBfcacheID) {
  *aBfcacheID = SharedInfo()->mId;
  return NS_OK;
}

NS_IMETHODIMP_(void)
SessionHistoryEntry::SyncTreesForSubframeNavigation(
    nsISHEntry* aEntry, mozilla::dom::BrowsingContext* aTopBC,
    mozilla::dom::BrowsingContext* aIgnoreBC) {
  // XXX Keep this in sync with nsSHEntry::SyncTreesForSubframeNavigation.
  //
  // We need to sync up the browsing context and session history trees for
  // subframe navigation.  If the load was in a subframe, we forward up to
  // the top browsing context, which will then recursively sync up all browsing
  // contexts to their corresponding entries in the new session history tree. If
  // we don't do this, then we can cache a content viewer on the wrong cloned
  // entry, and subsequently restore it at the wrong time.
  nsCOMPtr<nsISHEntry> newRootEntry = nsSHistory::GetRootSHEntry(aEntry);
  if (newRootEntry) {
    // newRootEntry is now the new root entry.
    // Find the old root entry as well.

    // Need a strong ref. on |oldRootEntry| so it isn't destroyed when
    // SetChildHistoryEntry() does SwapHistoryEntries() (bug 304639).
    nsCOMPtr<nsISHEntry> oldRootEntry = nsSHistory::GetRootSHEntry(this);

    if (oldRootEntry) {
      nsSHistory::SwapEntriesData data = {aIgnoreBC, newRootEntry, nullptr};
      nsSHistory::SetChildHistoryEntry(oldRootEntry, aTopBC, 0, &data);
    }
  }
}

void SessionHistoryEntry::ReplaceWith(const SessionHistoryEntry& aSource) {
  mInfo = MakeUnique<SessionHistoryInfo>(*aSource.mInfo);
  mChildren.Clear();
}

SHEntrySharedParentState* SessionHistoryEntry::SharedInfo() const {
  return static_cast<SHEntrySharedParentState*>(mInfo->mSharedState.Get());
}

void SessionHistoryEntry::SetFrameLoader(nsFrameLoader* aFrameLoader) {
  MOZ_ASSERT_IF(aFrameLoader, !SharedInfo()->mFrameLoader);
  // If the pref is disabled, we still allow evicting the existing entries.
  MOZ_RELEASE_ASSERT(!aFrameLoader || mozilla::BFCacheInParent());
  SharedInfo()->mFrameLoader = aFrameLoader;
  if (aFrameLoader) {
    if (BrowserParent* bp = aFrameLoader->GetBrowserParent()) {
      bp->Deactivated();
    }

    // When a new frameloader is stored, try to evict some older
    // frameloaders. Non-SHIP session history has a similar call in
    // nsDocumentViewer::Show.
    nsCOMPtr<nsISHistory> shistory;
    GetShistory(getter_AddRefs(shistory));
    if (shistory) {
      int32_t index = 0;
      shistory->GetIndex(&index);
      shistory->EvictOutOfRangeContentViewers(index);
    }
  }
}

nsFrameLoader* SessionHistoryEntry::GetFrameLoader() {
  return SharedInfo()->mFrameLoader;
}

void SessionHistoryEntry::SetInfo(SessionHistoryInfo* aInfo) {
  // FIXME Assert that we're not changing shared state!
  mInfo = MakeUnique<SessionHistoryInfo>(*aInfo);
}

}  // namespace dom

namespace ipc {

void IPDLParamTraits<dom::SessionHistoryInfo>::Write(
    IPC::Message* aMsg, IProtocol* aActor,
    const dom::SessionHistoryInfo& aParam) {
  Maybe<Tuple<uint32_t, dom::ClonedMessageData>> stateData;
  if (aParam.mStateData) {
    stateData.emplace();
    uint32_t version;
    NS_ENSURE_SUCCESS_VOID(aParam.mStateData->GetFormatVersion(&version));
    Get<0>(*stateData) = version;

    IToplevelProtocol* topLevel = aActor->ToplevelProtocol();
    MOZ_RELEASE_ASSERT(topLevel->GetProtocolId() == PContentMsgStart);
    if (topLevel->GetSide() == ChildSide) {
      auto* contentChild = static_cast<dom::ContentChild*>(topLevel);
      if (NS_WARN_IF(!aParam.mStateData->BuildClonedMessageDataForChild(
              contentChild, Get<1>(*stateData)))) {
        return;
      }
    } else {
      auto* contentParent = static_cast<dom::ContentParent*>(topLevel);
      if (NS_WARN_IF(!aParam.mStateData->BuildClonedMessageDataForParent(
              contentParent, Get<1>(*stateData)))) {
        return;
      }
    }
  }

  WriteIPDLParam(aMsg, aActor, aParam.mURI);
  WriteIPDLParam(aMsg, aActor, aParam.mOriginalURI);
  WriteIPDLParam(aMsg, aActor, aParam.mResultPrincipalURI);
  WriteIPDLParam(aMsg, aActor, aParam.mReferrerInfo);
  WriteIPDLParam(aMsg, aActor, aParam.mTitle);
  WriteIPDLParam(aMsg, aActor, aParam.mName);
  WriteIPDLParam(aMsg, aActor, aParam.mPostData);
  WriteIPDLParam(aMsg, aActor, aParam.mLoadType);
  WriteIPDLParam(aMsg, aActor, aParam.mScrollPositionX);
  WriteIPDLParam(aMsg, aActor, aParam.mScrollPositionY);
  WriteIPDLParam(aMsg, aActor, stateData);
  WriteIPDLParam(aMsg, aActor, aParam.mSrcdocData);
  WriteIPDLParam(aMsg, aActor, aParam.mBaseURI);
  WriteIPDLParam(aMsg, aActor, aParam.mLoadReplace);
  WriteIPDLParam(aMsg, aActor, aParam.mURIWasModified);
  WriteIPDLParam(aMsg, aActor, aParam.mIsSrcdocEntry);
  WriteIPDLParam(aMsg, aActor, aParam.mScrollRestorationIsManual);
  WriteIPDLParam(aMsg, aActor, aParam.mPersist);
  WriteIPDLParam(aMsg, aActor, aParam.mHasUserInteraction);
  WriteIPDLParam(aMsg, aActor, aParam.mSharedState.Get()->mId);
  WriteIPDLParam(aMsg, aActor, aParam.mSharedState.Get()->mTriggeringPrincipal);
  WriteIPDLParam(aMsg, aActor, aParam.mSharedState.Get()->mPrincipalToInherit);
  WriteIPDLParam(aMsg, aActor,
                 aParam.mSharedState.Get()->mPartitionedPrincipalToInherit);
  WriteIPDLParam(aMsg, aActor, aParam.mSharedState.Get()->mCsp);
  WriteIPDLParam(aMsg, aActor, aParam.mSharedState.Get()->mContentType);
  WriteIPDLParam(aMsg, aActor, aParam.mSharedState.Get()->mLayoutHistoryState);
  WriteIPDLParam(aMsg, aActor, aParam.mSharedState.Get()->mCacheKey);
  WriteIPDLParam(aMsg, aActor, aParam.mSharedState.Get()->mIsFrameNavigation);
}

bool IPDLParamTraits<dom::SessionHistoryInfo>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    dom::SessionHistoryInfo* aResult) {
  Maybe<Tuple<uint32_t, dom::ClonedMessageData>> stateData;
  uint64_t sharedId;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mURI) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mOriginalURI) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mResultPrincipalURI) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mReferrerInfo) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mTitle) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mName) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mPostData) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mLoadType) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mScrollPositionX) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mScrollPositionY) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &stateData) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSrcdocData) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mBaseURI) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mLoadReplace) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mURIWasModified) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mIsSrcdocEntry) ||
      !ReadIPDLParam(aMsg, aIter, aActor,
                     &aResult->mScrollRestorationIsManual) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mPersist) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mHasUserInteraction) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &sharedId)) {
    aActor->FatalError("Error reading fields for SessionHistoryInfo");
    return false;
  }

  nsCOMPtr<nsIPrincipal> triggeringPrincipal;
  nsCOMPtr<nsIPrincipal> principalToInherit;
  nsCOMPtr<nsIPrincipal> partitionedPrincipalToInherit;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  nsCString contentType;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &triggeringPrincipal) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &principalToInherit) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &partitionedPrincipalToInherit) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &csp) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &contentType)) {
    aActor->FatalError("Error reading fields for SessionHistoryInfo");
    return false;
  }

  dom::SHEntrySharedParentState* sharedState = nullptr;
  if (XRE_IsParentProcess()) {
    sharedState = dom::SHEntrySharedParentState::Lookup(sharedId);
  }

  if (sharedState) {
    aResult->mSharedState.Set(sharedState);

    MOZ_ASSERT(triggeringPrincipal
                   ? triggeringPrincipal->Equals(
                         aResult->mSharedState.Get()->mTriggeringPrincipal)
                   : !aResult->mSharedState.Get()->mTriggeringPrincipal,
               "We don't expect this to change!");
    MOZ_ASSERT(principalToInherit
                   ? principalToInherit->Equals(
                         aResult->mSharedState.Get()->mPrincipalToInherit)
                   : !aResult->mSharedState.Get()->mPrincipalToInherit,
               "We don't expect this to change!");
    MOZ_ASSERT(
        partitionedPrincipalToInherit
            ? partitionedPrincipalToInherit->Equals(
                  aResult->mSharedState.Get()->mPartitionedPrincipalToInherit)
            : !aResult->mSharedState.Get()->mPartitionedPrincipalToInherit,
        "We don't expect this to change!");
    MOZ_ASSERT(
        csp ? nsCSPContext::Equals(csp, aResult->mSharedState.Get()->mCsp)
            : !aResult->mSharedState.Get()->mCsp,
        "We don't expect this to change!");
    MOZ_ASSERT(contentType.Equals(aResult->mSharedState.Get()->mContentType),
               "We don't expect this to change!");
  } else {
    aResult->mSharedState.ChangeId(sharedId);
    aResult->mSharedState.Get()->mTriggeringPrincipal =
        triggeringPrincipal.forget();
    aResult->mSharedState.Get()->mPrincipalToInherit =
        principalToInherit.forget();
    aResult->mSharedState.Get()->mPartitionedPrincipalToInherit =
        partitionedPrincipalToInherit.forget();
    aResult->mSharedState.Get()->mCsp = csp.forget();
    aResult->mSharedState.Get()->mContentType = contentType;
  }

  if (!ReadIPDLParam(aMsg, aIter, aActor,
                     &aResult->mSharedState.Get()->mLayoutHistoryState) ||
      !ReadIPDLParam(aMsg, aIter, aActor,
                     &aResult->mSharedState.Get()->mCacheKey) ||
      !ReadIPDLParam(aMsg, aIter, aActor,
                     &aResult->mSharedState.Get()->mIsFrameNavigation)) {
    aActor->FatalError("Error reading fields for SessionHistoryInfo");
    return false;
  }

  if (stateData.isSome()) {
    uint32_t version = Get<0>(*stateData);
    aResult->mStateData = new nsStructuredCloneContainer(version);
    if (aActor->GetSide() == ChildSide) {
      aResult->mStateData->StealFromClonedMessageDataForChild(
          Get<1>(*stateData));
    } else {
      aResult->mStateData->StealFromClonedMessageDataForParent(
          Get<1>(*stateData));
    }
  }
  MOZ_ASSERT_IF(stateData.isNothing(), !aResult->mStateData);
  return true;
}

void IPDLParamTraits<dom::LoadingSessionHistoryInfo>::Write(
    IPC::Message* aMsg, IProtocol* aActor,
    const dom::LoadingSessionHistoryInfo& aParam) {
  WriteIPDLParam(aMsg, aActor, aParam.mInfo);
  WriteIPDLParam(aMsg, aActor, aParam.mLoadId);
  WriteIPDLParam(aMsg, aActor, aParam.mLoadIsFromSessionHistory);
  WriteIPDLParam(aMsg, aActor, aParam.mRequestedIndex);
  WriteIPDLParam(aMsg, aActor, aParam.mSessionHistoryLength);
}

bool IPDLParamTraits<dom::LoadingSessionHistoryInfo>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    dom::LoadingSessionHistoryInfo* aResult) {
  if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mInfo) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mLoadId) ||
      !ReadIPDLParam(aMsg, aIter, aActor,
                     &aResult->mLoadIsFromSessionHistory) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mRequestedIndex) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSessionHistoryLength)) {
    aActor->FatalError("Error reading fields for LoadingSessionHistoryInfo");
    return false;
  }

  return true;
}

void IPDLParamTraits<nsILayoutHistoryState*>::Write(
    IPC::Message* aMsg, IProtocol* aActor, nsILayoutHistoryState* aParam) {
  if (aParam) {
    WriteIPDLParam(aMsg, aActor, true);
    bool scrollPositionOnly = false;
    nsTArray<nsCString> keys;
    nsTArray<mozilla::PresState> states;
    aParam->GetContents(&scrollPositionOnly, keys, states);
    WriteIPDLParam(aMsg, aActor, scrollPositionOnly);
    WriteIPDLParam(aMsg, aActor, keys);
    WriteIPDLParam(aMsg, aActor, states);
  } else {
    WriteIPDLParam(aMsg, aActor, false);
  }
}

bool IPDLParamTraits<nsILayoutHistoryState*>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    RefPtr<nsILayoutHistoryState>* aResult) {
  bool hasLayoutHistoryState = false;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &hasLayoutHistoryState)) {
    aActor->FatalError("Error reading fields for nsILayoutHistoryState");
    return false;
  }

  if (hasLayoutHistoryState) {
    bool scrollPositionOnly = false;
    nsTArray<nsCString> keys;
    nsTArray<mozilla::PresState> states;
    if (!ReadIPDLParam(aMsg, aIter, aActor, &scrollPositionOnly) ||
        !ReadIPDLParam(aMsg, aIter, aActor, &keys) ||
        !ReadIPDLParam(aMsg, aIter, aActor, &states)) {
      aActor->FatalError("Error reading fields for nsILayoutHistoryState");
    }

    if (keys.Length() != states.Length()) {
      aActor->FatalError("Error reading fields for nsILayoutHistoryState");
      return false;
    }

    *aResult = NS_NewLayoutHistoryState();
    (*aResult)->SetScrollPositionOnly(scrollPositionOnly);
    for (uint32_t i = 0; i < keys.Length(); ++i) {
      PresState& state = states[i];
      UniquePtr<PresState> newState = MakeUnique<PresState>(state);
      (*aResult)->AddState(keys[i], std::move(newState));
    }
  }
  return true;
}

}  // namespace ipc

}  // namespace mozilla
