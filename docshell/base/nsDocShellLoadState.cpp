/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocShellLoadState.h"
#include "nsIDocShell.h"
#include "nsDocShell.h"
#include "nsISHEntry.h"
#include "nsIURIFixup.h"
#include "nsIWebNavigation.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"
#include "nsQueryObject.h"
#include "ReferrerInfo.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Components.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/LoadURIOptionsBinding.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_fission.h"

#include "mozilla/OriginAttributes.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/StaticPtr.h"

#include "mozilla/dom/PContent.h"

using namespace mozilla;
using namespace mozilla::dom;

// Global reference to the URI fixup service.
static mozilla::StaticRefPtr<nsIURIFixup> sURIFixup;

nsDocShellLoadState::nsDocShellLoadState(nsIURI* aURI)
    : nsDocShellLoadState(aURI, nsContentUtils::GenerateLoadIdentifier()) {}

nsDocShellLoadState::nsDocShellLoadState(
    const DocShellLoadStateInit& aLoadState)
    : mNotifiedBeforeUnloadListeners(false),
      mLoadIdentifier(aLoadState.LoadIdentifier()) {
  MOZ_ASSERT(aLoadState.URI(), "Cannot create a LoadState with a null URI!");
  mResultPrincipalURI = aLoadState.ResultPrincipalURI();
  mResultPrincipalURIIsSome = aLoadState.ResultPrincipalURIIsSome();
  mKeepResultPrincipalURIIfSet = aLoadState.KeepResultPrincipalURIIfSet();
  mLoadReplace = aLoadState.LoadReplace();
  mInheritPrincipal = aLoadState.InheritPrincipal();
  mPrincipalIsExplicit = aLoadState.PrincipalIsExplicit();
  mForceAllowDataURI = aLoadState.ForceAllowDataURI();
  mOriginalFrameSrc = aLoadState.OriginalFrameSrc();
  mIsFormSubmission = aLoadState.IsFormSubmission();
  mLoadType = aLoadState.LoadType();
  mTarget = aLoadState.Target();
  mTargetBrowsingContext = aLoadState.TargetBrowsingContext();
  mLoadFlags = aLoadState.LoadFlags();
  mInternalLoadFlags = aLoadState.InternalLoadFlags();
  mFirstParty = aLoadState.FirstParty();
  mHasValidUserGestureActivation = aLoadState.HasValidUserGestureActivation();
  mAllowFocusMove = aLoadState.AllowFocusMove();
  mTypeHint = aLoadState.TypeHint();
  mFileName = aLoadState.FileName();
  mIsFromProcessingFrameAttributes =
      aLoadState.IsFromProcessingFrameAttributes();
  mReferrerInfo = aLoadState.ReferrerInfo();
  mURI = aLoadState.URI();
  mOriginalURI = aLoadState.OriginalURI();
  mSourceBrowsingContext = aLoadState.SourceBrowsingContext();
  mBaseURI = aLoadState.BaseURI();
  mTriggeringPrincipal = aLoadState.TriggeringPrincipal();
  mPrincipalToInherit = aLoadState.PrincipalToInherit();
  mPartitionedPrincipalToInherit = aLoadState.PartitionedPrincipalToInherit();
  mTriggeringSandboxFlags = aLoadState.TriggeringSandboxFlags();
  mCsp = aLoadState.Csp();
  mOriginalURIString = aLoadState.OriginalURIString();
  mCancelContentJSEpoch = aLoadState.CancelContentJSEpoch();
  mPostDataStream = aLoadState.PostDataStream();
  mHeadersStream = aLoadState.HeadersStream();
  mSrcdocData = aLoadState.SrcdocData();
  mChannelInitialized = aLoadState.ChannelInitialized();
  mIsMetaRefresh = aLoadState.IsMetaRefresh();
  if (aLoadState.loadingSessionHistoryInfo().isSome()) {
    mLoadingSessionHistoryInfo = MakeUnique<LoadingSessionHistoryInfo>(
        aLoadState.loadingSessionHistoryInfo().ref());
  }
}

nsDocShellLoadState::nsDocShellLoadState(const nsDocShellLoadState& aOther)
    : mReferrerInfo(aOther.mReferrerInfo),
      mURI(aOther.mURI),
      mOriginalURI(aOther.mOriginalURI),
      mResultPrincipalURI(aOther.mResultPrincipalURI),
      mResultPrincipalURIIsSome(aOther.mResultPrincipalURIIsSome),
      mTriggeringPrincipal(aOther.mTriggeringPrincipal),
      mTriggeringSandboxFlags(aOther.mTriggeringSandboxFlags),
      mCsp(aOther.mCsp),
      mKeepResultPrincipalURIIfSet(aOther.mKeepResultPrincipalURIIfSet),
      mLoadReplace(aOther.mLoadReplace),
      mInheritPrincipal(aOther.mInheritPrincipal),
      mPrincipalIsExplicit(aOther.mPrincipalIsExplicit),
      mNotifiedBeforeUnloadListeners(aOther.mNotifiedBeforeUnloadListeners),
      mPrincipalToInherit(aOther.mPrincipalToInherit),
      mPartitionedPrincipalToInherit(aOther.mPartitionedPrincipalToInherit),
      mForceAllowDataURI(aOther.mForceAllowDataURI),
      mOriginalFrameSrc(aOther.mOriginalFrameSrc),
      mIsFormSubmission(aOther.mIsFormSubmission),
      mLoadType(aOther.mLoadType),
      mSHEntry(aOther.mSHEntry),
      mTarget(aOther.mTarget),
      mTargetBrowsingContext(aOther.mTargetBrowsingContext),
      mPostDataStream(aOther.mPostDataStream),
      mHeadersStream(aOther.mHeadersStream),
      mSrcdocData(aOther.mSrcdocData),
      mSourceBrowsingContext(aOther.mSourceBrowsingContext),
      mBaseURI(aOther.mBaseURI),
      mLoadFlags(aOther.mLoadFlags),
      mInternalLoadFlags(aOther.mInternalLoadFlags),
      mFirstParty(aOther.mFirstParty),
      mHasValidUserGestureActivation(aOther.mHasValidUserGestureActivation),
      mAllowFocusMove(aOther.mAllowFocusMove),
      mTypeHint(aOther.mTypeHint),
      mFileName(aOther.mFileName),
      mIsFromProcessingFrameAttributes(aOther.mIsFromProcessingFrameAttributes),
      mPendingRedirectedChannel(aOther.mPendingRedirectedChannel),
      mOriginalURIString(aOther.mOriginalURIString),
      mCancelContentJSEpoch(aOther.mCancelContentJSEpoch),
      mLoadIdentifier(aOther.mLoadIdentifier),
      mChannelInitialized(aOther.mChannelInitialized),
      mIsMetaRefresh(aOther.mIsMetaRefresh) {
  if (aOther.mLoadingSessionHistoryInfo) {
    mLoadingSessionHistoryInfo = MakeUnique<LoadingSessionHistoryInfo>(
        *aOther.mLoadingSessionHistoryInfo);
  }
}

nsDocShellLoadState::nsDocShellLoadState(nsIURI* aURI, uint64_t aLoadIdentifier)
    : mURI(aURI),
      mResultPrincipalURIIsSome(false),
      mTriggeringSandboxFlags(0),
      mKeepResultPrincipalURIIfSet(false),
      mLoadReplace(false),
      mInheritPrincipal(false),
      mPrincipalIsExplicit(false),
      mNotifiedBeforeUnloadListeners(false),
      mForceAllowDataURI(false),
      mOriginalFrameSrc(false),
      mIsFormSubmission(false),
      mLoadType(LOAD_NORMAL),
      mTarget(),
      mSrcdocData(VoidString()),
      mLoadFlags(0),
      mInternalLoadFlags(0),
      mFirstParty(false),
      mHasValidUserGestureActivation(false),
      mAllowFocusMove(false),
      mTypeHint(VoidCString()),
      mFileName(VoidString()),
      mIsFromProcessingFrameAttributes(false),
      mLoadIdentifier(aLoadIdentifier),
      mChannelInitialized(false),
      mIsMetaRefresh(false) {
  MOZ_ASSERT(aURI, "Cannot create a LoadState with a null URI!");
}

nsDocShellLoadState::~nsDocShellLoadState() {}

nsresult nsDocShellLoadState::CreateFromPendingChannel(
    nsIChannel* aPendingChannel, uint64_t aLoadIdentifier,
    uint64_t aRegistrarId, nsDocShellLoadState** aResult) {
  // Create the nsDocShellLoadState object with default state pulled from the
  // passed-in channel.
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPendingChannel->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<nsDocShellLoadState> loadState =
      new nsDocShellLoadState(uri, aLoadIdentifier);
  loadState->mPendingRedirectedChannel = aPendingChannel;
  loadState->mChannelRegistrarId = aRegistrarId;

  // Pull relevant state from the channel, and store it on the
  // nsDocShellLoadState.
  nsCOMPtr<nsIURI> originalUri;
  rv = aPendingChannel->GetOriginalURI(getter_AddRefs(originalUri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  loadState->SetOriginalURI(originalUri);

  nsCOMPtr<nsILoadInfo> loadInfo = aPendingChannel->LoadInfo();
  loadState->SetTriggeringPrincipal(loadInfo->TriggeringPrincipal());

  // Return the newly created loadState.
  loadState.forget(aResult);
  return NS_OK;
}

static uint32_t WebNavigationFlagsToFixupFlags(nsIURI* aURI,
                                               const nsACString& aURIString,
                                               uint32_t aNavigationFlags) {
  if (aURI) {
    aNavigationFlags &= ~nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
  }
  uint32_t fixupFlags = nsIURIFixup::FIXUP_FLAG_NONE;
  if (aNavigationFlags & nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) {
    fixupFlags |= nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
  }
  if (aNavigationFlags & nsIWebNavigation::LOAD_FLAGS_FIXUP_SCHEME_TYPOS) {
    fixupFlags |= nsIURIFixup::FIXUP_FLAG_FIX_SCHEME_TYPOS;
  }
  return fixupFlags;
};

nsresult nsDocShellLoadState::CreateFromLoadURIOptions(
    BrowsingContext* aBrowsingContext, const nsAString& aURI,
    const LoadURIOptions& aLoadURIOptions, nsDocShellLoadState** aResult) {
  uint32_t loadFlags = aLoadURIOptions.mLoadFlags;

  NS_ASSERTION(
      (loadFlags & nsDocShell::INTERNAL_LOAD_FLAGS_LOADURI_SETUP_FLAGS) == 0,
      "Unexpected flags");

  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIInputStream> postData(aLoadURIOptions.mPostData);
  nsresult rv = NS_OK;

  NS_ConvertUTF16toUTF8 uriString(aURI);
  // Cleanup the empty spaces that might be on each end.
  uriString.Trim(" ");
  // Eliminate embedded newlines, which single-line text fields now allow:
  uriString.StripCRLF();
  NS_ENSURE_TRUE(!uriString.IsEmpty(), NS_ERROR_FAILURE);

  // Just create a URI and see what happens...
  rv = NS_NewURI(getter_AddRefs(uri), uriString);
  bool fixup = true;
  if (NS_SUCCEEDED(rv) && uri &&
      (uri->SchemeIs("about") || uri->SchemeIs("chrome"))) {
    // Avoid third party fixup as a performance optimization.
    loadFlags &= ~nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
    fixup = false;
  } else if (!sURIFixup && !XRE_IsContentProcess()) {
    nsCOMPtr<nsIURIFixup> uriFixup = components::URIFixup::Service();
    if (uriFixup) {
      sURIFixup = uriFixup;
      ClearOnShutdown(&sURIFixup);
    } else {
      fixup = false;
    }
  }

  nsAutoString searchProvider, keyword;
  bool didFixup = false;
  if (fixup) {
    uint32_t fixupFlags =
        WebNavigationFlagsToFixupFlags(uri, uriString, loadFlags);

    // If we don't allow keyword lookups for this URL string, make sure to
    // update loadFlags to indicate this as well.
    if (!(fixupFlags & nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP)) {
      loadFlags &= ~nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
    }
    // Ensure URIFixup will use the right search engine in Private Browsing.
    if (aBrowsingContext->UsePrivateBrowsing()) {
      fixupFlags |= nsIURIFixup::FIXUP_FLAG_PRIVATE_CONTEXT;
    }

    RefPtr<nsIInputStream> fixupStream;
    if (!XRE_IsContentProcess()) {
      nsCOMPtr<nsIURIFixupInfo> fixupInfo;
      sURIFixup->GetFixupURIInfo(uriString, fixupFlags,
                                 getter_AddRefs(fixupInfo));
      if (fixupInfo) {
        // We could fix the uri, clear NS_ERROR_MALFORMED_URI.
        rv = NS_OK;
        fixupInfo->GetPreferredURI(getter_AddRefs(uri));
        fixupInfo->SetConsumer(aBrowsingContext);
        fixupInfo->GetKeywordProviderName(searchProvider);
        fixupInfo->GetKeywordAsSent(keyword);
        fixupInfo->GetPostData(getter_AddRefs(fixupStream));
        didFixup = true;

        if (fixupInfo &&
            loadFlags & nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) {
          nsCOMPtr<nsIObserverService> serv = services::GetObserverService();
          if (serv) {
            serv->NotifyObservers(fixupInfo, "keyword-uri-fixup",
                                  PromiseFlatString(aURI).get());
          }
        }
      }
    }

    if (fixupStream) {
      // GetFixupURIInfo only returns a post data stream if it succeeded
      // and changed the URI, in which case we should override the
      // passed-in post data.
      postData = fixupStream;
    }
  }

  if (rv == NS_ERROR_MALFORMED_URI) {
    MOZ_ASSERT(!uri);
    return rv;
  }

  if (NS_FAILED(rv) || !uri) {
    return NS_ERROR_FAILURE;
  }

  uint64_t available;
  if (postData) {
    rv = postData->Available(&available);
    NS_ENSURE_SUCCESS(rv, rv);
    if (available == 0) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  if (aLoadURIOptions.mHeaders) {
    rv = aLoadURIOptions.mHeaders->Available(&available);
    NS_ENSURE_SUCCESS(rv, rv);
    if (available == 0) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  bool forceAllowDataURI =
      loadFlags & nsIWebNavigation::LOAD_FLAGS_FORCE_ALLOW_DATA_URI;

  // Don't pass certain flags that aren't needed and end up confusing
  // ConvertLoadTypeToDocShellInfoLoadType.  We do need to ensure that they are
  // passed to LoadURI though, since it uses them.
  uint32_t extraFlags = (loadFlags & EXTRA_LOAD_FLAGS);
  loadFlags &= ~EXTRA_LOAD_FLAGS;

  RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState(uri);
  loadState->SetReferrerInfo(aLoadURIOptions.mReferrerInfo);

  loadState->SetLoadType(MAKE_LOAD_TYPE(LOAD_NORMAL, loadFlags));

  loadState->SetLoadFlags(extraFlags);
  loadState->SetFirstParty(true);
  loadState->SetHasValidUserGestureActivation(
      aLoadURIOptions.mHasValidUserGestureActivation);
  loadState->SetTriggeringSandboxFlags(aLoadURIOptions.mTriggeringSandboxFlags);
  loadState->SetPostDataStream(postData);
  loadState->SetHeadersStream(aLoadURIOptions.mHeaders);
  loadState->SetBaseURI(aLoadURIOptions.mBaseURI);
  loadState->SetTriggeringPrincipal(aLoadURIOptions.mTriggeringPrincipal);
  loadState->SetCsp(aLoadURIOptions.mCsp);
  loadState->SetForceAllowDataURI(forceAllowDataURI);
  loadState->SetOriginalURIString(uriString);
  if (aLoadURIOptions.mCancelContentJSEpoch) {
    loadState->SetCancelContentJSEpoch(aLoadURIOptions.mCancelContentJSEpoch);
  }

  if (didFixup) {
    nsDocShell::MaybeNotifyKeywordSearchLoading(searchProvider, keyword);
  }

  loadState.forget(aResult);
  return NS_OK;
}

nsIReferrerInfo* nsDocShellLoadState::GetReferrerInfo() const {
  return mReferrerInfo;
}

void nsDocShellLoadState::SetReferrerInfo(nsIReferrerInfo* aReferrerInfo) {
  mReferrerInfo = aReferrerInfo;
}

nsIURI* nsDocShellLoadState::URI() const { return mURI; }

void nsDocShellLoadState::SetURI(nsIURI* aURI) { mURI = aURI; }

nsIURI* nsDocShellLoadState::OriginalURI() const { return mOriginalURI; }

void nsDocShellLoadState::SetOriginalURI(nsIURI* aOriginalURI) {
  mOriginalURI = aOriginalURI;
}

nsIURI* nsDocShellLoadState::ResultPrincipalURI() const {
  return mResultPrincipalURI;
}

void nsDocShellLoadState::SetResultPrincipalURI(nsIURI* aResultPrincipalURI) {
  mResultPrincipalURI = aResultPrincipalURI;
}

bool nsDocShellLoadState::ResultPrincipalURIIsSome() const {
  return mResultPrincipalURIIsSome;
}

void nsDocShellLoadState::SetResultPrincipalURIIsSome(bool aIsSome) {
  mResultPrincipalURIIsSome = aIsSome;
}

bool nsDocShellLoadState::KeepResultPrincipalURIIfSet() const {
  return mKeepResultPrincipalURIIfSet;
}

void nsDocShellLoadState::SetKeepResultPrincipalURIIfSet(bool aKeep) {
  mKeepResultPrincipalURIIfSet = aKeep;
}

bool nsDocShellLoadState::LoadReplace() const { return mLoadReplace; }

void nsDocShellLoadState::SetLoadReplace(bool aLoadReplace) {
  mLoadReplace = aLoadReplace;
}

nsIPrincipal* nsDocShellLoadState::TriggeringPrincipal() const {
  return mTriggeringPrincipal;
}

void nsDocShellLoadState::SetTriggeringPrincipal(
    nsIPrincipal* aTriggeringPrincipal) {
  mTriggeringPrincipal = aTriggeringPrincipal;
}

nsIPrincipal* nsDocShellLoadState::PrincipalToInherit() const {
  return mPrincipalToInherit;
}

void nsDocShellLoadState::SetPrincipalToInherit(
    nsIPrincipal* aPrincipalToInherit) {
  mPrincipalToInherit = aPrincipalToInherit;
}

nsIPrincipal* nsDocShellLoadState::PartitionedPrincipalToInherit() const {
  return mPartitionedPrincipalToInherit;
}

void nsDocShellLoadState::SetPartitionedPrincipalToInherit(
    nsIPrincipal* aPartitionedPrincipalToInherit) {
  mPartitionedPrincipalToInherit = aPartitionedPrincipalToInherit;
}

void nsDocShellLoadState::SetCsp(nsIContentSecurityPolicy* aCsp) {
  mCsp = aCsp;
}

nsIContentSecurityPolicy* nsDocShellLoadState::Csp() const { return mCsp; }

void nsDocShellLoadState::SetTriggeringSandboxFlags(uint32_t flags) {
  mTriggeringSandboxFlags = flags;
}

uint32_t nsDocShellLoadState::TriggeringSandboxFlags() const {
  return mTriggeringSandboxFlags;
}

bool nsDocShellLoadState::InheritPrincipal() const { return mInheritPrincipal; }

void nsDocShellLoadState::SetInheritPrincipal(bool aInheritPrincipal) {
  mInheritPrincipal = aInheritPrincipal;
}

bool nsDocShellLoadState::PrincipalIsExplicit() const {
  return mPrincipalIsExplicit;
}

void nsDocShellLoadState::SetPrincipalIsExplicit(bool aPrincipalIsExplicit) {
  mPrincipalIsExplicit = aPrincipalIsExplicit;
}

bool nsDocShellLoadState::NotifiedBeforeUnloadListeners() const {
  return mNotifiedBeforeUnloadListeners;
}

void nsDocShellLoadState::SetNotifiedBeforeUnloadListeners(
    bool aNotifiedBeforeUnloadListeners) {
  mNotifiedBeforeUnloadListeners = aNotifiedBeforeUnloadListeners;
}

bool nsDocShellLoadState::ForceAllowDataURI() const {
  return mForceAllowDataURI;
}

void nsDocShellLoadState::SetForceAllowDataURI(bool aForceAllowDataURI) {
  mForceAllowDataURI = aForceAllowDataURI;
}

bool nsDocShellLoadState::OriginalFrameSrc() const { return mOriginalFrameSrc; }

void nsDocShellLoadState::SetOriginalFrameSrc(bool aOriginalFrameSrc) {
  mOriginalFrameSrc = aOriginalFrameSrc;
}

bool nsDocShellLoadState::IsFormSubmission() const { return mIsFormSubmission; }

void nsDocShellLoadState::SetIsFormSubmission(bool aIsFormSubmission) {
  mIsFormSubmission = aIsFormSubmission;
}

uint32_t nsDocShellLoadState::LoadType() const { return mLoadType; }

void nsDocShellLoadState::SetLoadType(uint32_t aLoadType) {
  mLoadType = aLoadType;
}

nsISHEntry* nsDocShellLoadState::SHEntry() const { return mSHEntry; }

void nsDocShellLoadState::SetSHEntry(nsISHEntry* aSHEntry) {
  mSHEntry = aSHEntry;
  nsCOMPtr<SessionHistoryEntry> she = do_QueryInterface(aSHEntry);
  if (she) {
    mLoadingSessionHistoryInfo = MakeUnique<LoadingSessionHistoryInfo>(she);
  } else {
    mLoadingSessionHistoryInfo = nullptr;
  }
}

void nsDocShellLoadState::SetLoadingSessionHistoryInfo(
    const mozilla::dom::LoadingSessionHistoryInfo& aLoadingInfo) {
  SetLoadingSessionHistoryInfo(
      MakeUnique<mozilla::dom::LoadingSessionHistoryInfo>(aLoadingInfo));
}

void nsDocShellLoadState::SetLoadingSessionHistoryInfo(
    mozilla::UniquePtr<mozilla::dom::LoadingSessionHistoryInfo> aLoadingInfo) {
  mLoadingSessionHistoryInfo = std::move(aLoadingInfo);
}

const mozilla::dom::LoadingSessionHistoryInfo*
nsDocShellLoadState::GetLoadingSessionHistoryInfo() const {
  return mLoadingSessionHistoryInfo.get();
}

void nsDocShellLoadState::SetLoadIsFromSessionHistory(
    int32_t aRequestedIndex, int32_t aSessionHistoryLength,
    bool aLoadingFromActiveEntry) {
  if (mLoadingSessionHistoryInfo) {
    mLoadingSessionHistoryInfo->mLoadIsFromSessionHistory = true;
    mLoadingSessionHistoryInfo->mRequestedIndex = aRequestedIndex;
    mLoadingSessionHistoryInfo->mSessionHistoryLength = aSessionHistoryLength;
    mLoadingSessionHistoryInfo->mLoadingCurrentActiveEntry =
        aLoadingFromActiveEntry;
  }
}

void nsDocShellLoadState::ClearLoadIsFromSessionHistory() {
  if (mLoadingSessionHistoryInfo) {
    mLoadingSessionHistoryInfo->mLoadIsFromSessionHistory = false;
  }
  mSHEntry = nullptr;
}

bool nsDocShellLoadState::LoadIsFromSessionHistory() const {
  return mLoadingSessionHistoryInfo
             ? mLoadingSessionHistoryInfo->mLoadIsFromSessionHistory
             : !!mSHEntry;
}

const nsString& nsDocShellLoadState::Target() const { return mTarget; }

void nsDocShellLoadState::SetTarget(const nsAString& aTarget) {
  mTarget = aTarget;
}

nsIInputStream* nsDocShellLoadState::PostDataStream() const {
  return mPostDataStream;
}

void nsDocShellLoadState::SetPostDataStream(nsIInputStream* aStream) {
  mPostDataStream = aStream;
}

nsIInputStream* nsDocShellLoadState::HeadersStream() const {
  return mHeadersStream;
}

void nsDocShellLoadState::SetHeadersStream(nsIInputStream* aHeadersStream) {
  mHeadersStream = aHeadersStream;
}

const nsString& nsDocShellLoadState::SrcdocData() const { return mSrcdocData; }

void nsDocShellLoadState::SetSrcdocData(const nsAString& aSrcdocData) {
  mSrcdocData = aSrcdocData;
}

void nsDocShellLoadState::SetSourceBrowsingContext(
    BrowsingContext* aSourceBrowsingContext) {
  mSourceBrowsingContext = aSourceBrowsingContext;
}

void nsDocShellLoadState::SetTargetBrowsingContext(
    BrowsingContext* aTargetBrowsingContext) {
  mTargetBrowsingContext = aTargetBrowsingContext;
}

nsIURI* nsDocShellLoadState::BaseURI() const { return mBaseURI; }

void nsDocShellLoadState::SetBaseURI(nsIURI* aBaseURI) { mBaseURI = aBaseURI; }

void nsDocShellLoadState::GetMaybeResultPrincipalURI(
    mozilla::Maybe<nsCOMPtr<nsIURI>>& aRPURI) const {
  bool isSome = ResultPrincipalURIIsSome();
  aRPURI.reset();

  if (!isSome) {
    return;
  }

  nsCOMPtr<nsIURI> uri = ResultPrincipalURI();
  aRPURI.emplace(std::move(uri));
}

void nsDocShellLoadState::SetMaybeResultPrincipalURI(
    mozilla::Maybe<nsCOMPtr<nsIURI>> const& aRPURI) {
  SetResultPrincipalURI(aRPURI.refOr(nullptr));
  SetResultPrincipalURIIsSome(aRPURI.isSome());
}

uint32_t nsDocShellLoadState::LoadFlags() const { return mLoadFlags; }

void nsDocShellLoadState::SetLoadFlags(uint32_t aLoadFlags) {
  mLoadFlags = aLoadFlags;
}

void nsDocShellLoadState::SetLoadFlag(uint32_t aFlag) { mLoadFlags |= aFlag; }

void nsDocShellLoadState::UnsetLoadFlag(uint32_t aFlag) {
  mLoadFlags &= ~aFlag;
}

bool nsDocShellLoadState::HasLoadFlags(uint32_t aFlags) {
  return (mLoadFlags & aFlags) == aFlags;
}

uint32_t nsDocShellLoadState::InternalLoadFlags() const {
  return mInternalLoadFlags;
}

void nsDocShellLoadState::SetInternalLoadFlags(uint32_t aLoadFlags) {
  mInternalLoadFlags = aLoadFlags;
}

void nsDocShellLoadState::SetInternalLoadFlag(uint32_t aFlag) {
  mInternalLoadFlags |= aFlag;
}

void nsDocShellLoadState::UnsetInternalLoadFlag(uint32_t aFlag) {
  mInternalLoadFlags &= ~aFlag;
}

bool nsDocShellLoadState::HasInternalLoadFlags(uint32_t aFlags) {
  return (mInternalLoadFlags & aFlags) == aFlags;
}

bool nsDocShellLoadState::FirstParty() const { return mFirstParty; }

void nsDocShellLoadState::SetFirstParty(bool aFirstParty) {
  mFirstParty = aFirstParty;
}

bool nsDocShellLoadState::HasValidUserGestureActivation() const {
  return mHasValidUserGestureActivation;
}

void nsDocShellLoadState::SetHasValidUserGestureActivation(
    bool aHasValidUserGestureActivation) {
  mHasValidUserGestureActivation = aHasValidUserGestureActivation;
}

const nsCString& nsDocShellLoadState::TypeHint() const { return mTypeHint; }

void nsDocShellLoadState::SetTypeHint(const nsCString& aTypeHint) {
  mTypeHint = aTypeHint;
}

const nsString& nsDocShellLoadState::FileName() const { return mFileName; }

void nsDocShellLoadState::SetFileName(const nsAString& aFileName) {
  MOZ_DIAGNOSTIC_ASSERT(aFileName.FindChar(char16_t(0)) == kNotFound,
                        "The filename should never contain null characters");
  mFileName = aFileName;
}

nsresult nsDocShellLoadState::SetupInheritingPrincipal(
    BrowsingContext::Type aType,
    const mozilla::OriginAttributes& aOriginAttributes) {
  // We need a principalToInherit.
  //
  // If principalIsExplicit is not set there are 4 possibilities:
  // (1) If the system principal or an expanded principal was passed
  //     in and we're a typeContent docshell, inherit the principal
  //     from the current document instead.
  // (2) In all other cases when the principal passed in is not null,
  //     use that principal.
  // (3) If the caller has allowed inheriting from the current document,
  //     or if we're being called from system code (eg chrome JS or pure
  //     C++) then inheritPrincipal should be true and InternalLoad will get
  //     a principal from the current document. If none of these things are
  //     true, then
  // (4) we don't pass a principal into the channel, and a principal will be
  //     created later from the channel's internal data.
  //
  // If principalIsExplicit *is* set, there are 4 possibilities
  // (1) If the system principal or an expanded principal was passed in
  //     and we're a typeContent docshell, return an error.
  // (2) In all other cases when the principal passed in is not null,
  //     use that principal.
  // (3) If the caller has allowed inheriting from the current document,
  //     then inheritPrincipal should be true and InternalLoad will get
  //     a principal from the current document. If none of these things are
  //     true, then
  // (4) we dont' pass a principal into the channel, and a principal will be
  //     created later from the channel's internal data.
  mPrincipalToInherit = mTriggeringPrincipal;
  if (mPrincipalToInherit && aType != BrowsingContext::Type::Chrome) {
    if (mPrincipalToInherit->IsSystemPrincipal()) {
      if (mPrincipalIsExplicit) {
        return NS_ERROR_DOM_SECURITY_ERR;
      }
      mPrincipalToInherit = nullptr;
      mInheritPrincipal = true;
    } else if (nsContentUtils::IsExpandedPrincipal(mPrincipalToInherit)) {
      if (mPrincipalIsExplicit) {
        return NS_ERROR_DOM_SECURITY_ERR;
      }
      // Don't inherit from the current page.  Just do the safe thing
      // and pretend that we were loaded by a nullprincipal.
      //
      // We didn't inherit OriginAttributes here as ExpandedPrincipal doesn't
      // have origin attributes.
      mPrincipalToInherit = NullPrincipal::CreateWithInheritedAttributes(
          aOriginAttributes, false);
      mInheritPrincipal = false;
    }
  }

  if (!mPrincipalToInherit && !mInheritPrincipal && !mPrincipalIsExplicit) {
    // See if there's system or chrome JS code running
    mInheritPrincipal = nsContentUtils::LegacyIsCallerChromeOrNativeCode();
  }

  if (mLoadFlags & nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL) {
    mInheritPrincipal = false;
    // If mFirstParty is true and the pref 'privacy.firstparty.isolate' is
    // enabled, we will set firstPartyDomain on the origin attributes.
    mPrincipalToInherit = NullPrincipal::CreateWithInheritedAttributes(
        aOriginAttributes, mFirstParty);
  }

  return NS_OK;
}

nsresult nsDocShellLoadState::SetupTriggeringPrincipal(
    const mozilla::OriginAttributes& aOriginAttributes) {
  // If the triggeringPrincipal is not set, we first try to create a principal
  // from the referrer, since the referrer URI reflects the web origin that
  // triggered the load. If there is no referrer URI, we fall back to using the
  // SystemPrincipal. It's safe to assume that no provided triggeringPrincipal
  // and no referrer simulate a load that was triggered by the system. It's
  // important to note that this block of code needs to appear *after* the block
  // where we munge the principalToInherit, because otherwise we would never
  // enter code blocks checking if the principalToInherit is null and we will
  // end up with a wrong inheritPrincipal flag.
  if (!mTriggeringPrincipal) {
    if (mReferrerInfo) {
      nsCOMPtr<nsIURI> referrer = mReferrerInfo->GetOriginalReferrer();
      mTriggeringPrincipal =
          BasePrincipal::CreateContentPrincipal(referrer, aOriginAttributes);

      if (!mTriggeringPrincipal) {
        return NS_ERROR_FAILURE;
      }
    } else {
      mTriggeringPrincipal = nsContentUtils::GetSystemPrincipal();
    }
  }
  return NS_OK;
}

void nsDocShellLoadState::CalculateLoadURIFlags() {
  if (mInheritPrincipal) {
    MOZ_ASSERT(
        !mPrincipalToInherit || !mPrincipalToInherit->IsSystemPrincipal(),
        "Should not inherit SystemPrincipal");
    mInternalLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_INHERIT_PRINCIPAL;
  }

  if (mReferrerInfo && !mReferrerInfo->GetSendReferrer()) {
    mInternalLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER;
  }
  if (mLoadFlags & nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) {
    mInternalLoadFlags |=
        nsDocShell::INTERNAL_LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
  }

  if (mLoadFlags & nsIWebNavigation::LOAD_FLAGS_FIRST_LOAD) {
    mInternalLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_FIRST_LOAD;
  }

  if (mLoadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_CLASSIFIER) {
    mInternalLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_BYPASS_CLASSIFIER;
  }

  if (mLoadFlags & nsIWebNavigation::LOAD_FLAGS_FORCE_ALLOW_COOKIES) {
    mInternalLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_FORCE_ALLOW_COOKIES;
  }

  if (mLoadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE) {
    mInternalLoadFlags |=
        nsDocShell::INTERNAL_LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE;
  }

  if (!mSrcdocData.IsVoid()) {
    mInternalLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_IS_SRCDOC;
  }

  if (mForceAllowDataURI) {
    mInternalLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_FORCE_ALLOW_DATA_URI;
  }

  if (mOriginalFrameSrc) {
    mInternalLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_ORIGINAL_FRAME_SRC;
  }
}

nsLoadFlags nsDocShellLoadState::CalculateChannelLoadFlags(
    BrowsingContext* aBrowsingContext, Maybe<bool> aUriModified,
    Maybe<bool> aIsXFOError) {
  MOZ_ASSERT(aBrowsingContext);

  nsLoadFlags loadFlags = aBrowsingContext->GetDefaultLoadFlags();

  if (FirstParty()) {
    // tag first party URL loads
    loadFlags |= nsIChannel::LOAD_INITIAL_DOCUMENT_URI;
  }

  const uint32_t loadType = LoadType();

  // These values aren't available for loads initiated in the Parent process.
  MOZ_ASSERT_IF(loadType == LOAD_HISTORY, aUriModified.isSome());
  MOZ_ASSERT_IF(loadType == LOAD_ERROR_PAGE, aIsXFOError.isSome());

  if (loadType == LOAD_ERROR_PAGE) {
    // Error pages are LOAD_BACKGROUND, unless it's an
    // XFO error for which we want an error page to load
    // but additionally want the onload() event to fire.
    if (!*aIsXFOError) {
      loadFlags |= nsIChannel::LOAD_BACKGROUND;
    }
  }

  // Mark the channel as being a document URI and allow content sniffing...
  loadFlags |=
      nsIChannel::LOAD_DOCUMENT_URI | nsIChannel::LOAD_CALL_CONTENT_SNIFFERS;

  if (nsDocShell::SandboxFlagsImplyCookies(
          aBrowsingContext->GetSandboxFlags())) {
    loadFlags |= nsIRequest::LOAD_DOCUMENT_NEEDS_COOKIE;
  }

  // Load attributes depend on load type...
  switch (loadType) {
    case LOAD_HISTORY: {
      // Only send VALIDATE_NEVER if mLSHE's URI was never changed via
      // push/replaceState (bug 669671).
      if (!*aUriModified) {
        loadFlags |= nsIRequest::VALIDATE_NEVER;
      }
      break;
    }

    case LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE:
    case LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE:
      loadFlags |=
          nsIRequest::LOAD_BYPASS_CACHE | nsIRequest::LOAD_FRESH_CONNECTION;
      [[fallthrough]];

    case LOAD_RELOAD_NORMAL:
    case LOAD_REFRESH:
      loadFlags |= nsIRequest::VALIDATE_ALWAYS;
      break;

    case LOAD_NORMAL_BYPASS_CACHE:
    case LOAD_NORMAL_BYPASS_PROXY:
    case LOAD_NORMAL_BYPASS_PROXY_AND_CACHE:
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
    case LOAD_REPLACE_BYPASS_CACHE:
      loadFlags |=
          nsIRequest::LOAD_BYPASS_CACHE | nsIRequest::LOAD_FRESH_CONNECTION;
      break;

    case LOAD_NORMAL:
    case LOAD_LINK:
      // Set cache checking flags
      switch (StaticPrefs::browser_cache_check_doc_frequency()) {
        case 0:
          loadFlags |= nsIRequest::VALIDATE_ONCE_PER_SESSION;
          break;
        case 1:
          loadFlags |= nsIRequest::VALIDATE_ALWAYS;
          break;
        case 2:
          loadFlags |= nsIRequest::VALIDATE_NEVER;
          break;
      }
      break;
  }

  if (HasInternalLoadFlags(nsDocShell::INTERNAL_LOAD_FLAGS_BYPASS_CLASSIFIER)) {
    loadFlags |= nsIChannel::LOAD_BYPASS_URL_CLASSIFIER;
  }

  // If the user pressed shift-reload, then do not allow ServiceWorker
  // interception to occur. See step 12.1 of the SW HandleFetch algorithm.
  if (IsForceReloadType(loadType)) {
    loadFlags |= nsIChannel::LOAD_BYPASS_SERVICE_WORKER;
  }

  return loadFlags;
}

DocShellLoadStateInit nsDocShellLoadState::Serialize() {
  DocShellLoadStateInit loadState;
  loadState.ResultPrincipalURI() = mResultPrincipalURI;
  loadState.ResultPrincipalURIIsSome() = mResultPrincipalURIIsSome;
  loadState.KeepResultPrincipalURIIfSet() = mKeepResultPrincipalURIIfSet;
  loadState.LoadReplace() = mLoadReplace;
  loadState.InheritPrincipal() = mInheritPrincipal;
  loadState.PrincipalIsExplicit() = mPrincipalIsExplicit;
  loadState.ForceAllowDataURI() = mForceAllowDataURI;
  loadState.OriginalFrameSrc() = mOriginalFrameSrc;
  loadState.IsFormSubmission() = mIsFormSubmission;
  loadState.LoadType() = mLoadType;
  loadState.Target() = mTarget;
  loadState.TargetBrowsingContext() = mTargetBrowsingContext;
  loadState.LoadFlags() = mLoadFlags;
  loadState.InternalLoadFlags() = mInternalLoadFlags;
  loadState.FirstParty() = mFirstParty;
  loadState.HasValidUserGestureActivation() = mHasValidUserGestureActivation;
  loadState.AllowFocusMove() = mAllowFocusMove;
  loadState.TypeHint() = mTypeHint;
  loadState.FileName() = mFileName;
  loadState.IsFromProcessingFrameAttributes() =
      mIsFromProcessingFrameAttributes;
  loadState.URI() = mURI;
  loadState.OriginalURI() = mOriginalURI;
  loadState.SourceBrowsingContext() = mSourceBrowsingContext;
  loadState.BaseURI() = mBaseURI;
  loadState.TriggeringPrincipal() = mTriggeringPrincipal;
  loadState.PrincipalToInherit() = mPrincipalToInherit;
  loadState.PartitionedPrincipalToInherit() = mPartitionedPrincipalToInherit;
  loadState.TriggeringSandboxFlags() = mTriggeringSandboxFlags;
  loadState.Csp() = mCsp;
  loadState.OriginalURIString() = mOriginalURIString;
  loadState.CancelContentJSEpoch() = mCancelContentJSEpoch;
  loadState.ReferrerInfo() = mReferrerInfo;
  loadState.PostDataStream() = mPostDataStream;
  loadState.HeadersStream() = mHeadersStream;
  loadState.SrcdocData() = mSrcdocData;
  loadState.ResultPrincipalURI() = mResultPrincipalURI;
  loadState.LoadIdentifier() = mLoadIdentifier;
  loadState.ChannelInitialized() = mChannelInitialized;
  loadState.IsMetaRefresh() = mIsMetaRefresh;
  if (mLoadingSessionHistoryInfo) {
    loadState.loadingSessionHistoryInfo().emplace(*mLoadingSessionHistoryInfo);
  }
  return loadState;
}
