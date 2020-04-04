/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocShellLoadState.h"
#include "nsIDocShell.h"
#include "SHEntryParent.h"
#include "SHEntryChild.h"
#include "nsISHEntry.h"
#include "nsIWebNavigation.h"
#include "nsIChannel.h"
#include "ReferrerInfo.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/LoadURIOptionsBinding.h"
#include "mozilla/StaticPrefs_fission.h"

#include "mozilla/OriginAttributes.h"
#include "mozilla/NullPrincipal.h"

#include "mozilla/dom/PContent.h"

nsDocShellLoadState::nsDocShellLoadState(nsIURI* aURI)
    : mURI(aURI),
      mResultPrincipalURIIsSome(false),
      mKeepResultPrincipalURIIfSet(false),
      mLoadReplace(false),
      mInheritPrincipal(false),
      mPrincipalIsExplicit(false),
      mForceAllowDataURI(false),
      mOriginalFrameSrc(false),
      mIsFormSubmission(false),
      mLoadType(LOAD_NORMAL),
      mTarget(),
      mSrcdocData(VoidString()),
      mLoadFlags(0),
      mFirstParty(false),
      mTypeHint(VoidCString()),
      mFileName(VoidString()),
      mIsFromProcessingFrameAttributes(false) {
  MOZ_ASSERT(aURI, "Cannot create a LoadState with a null URI!");
}

nsDocShellLoadState::nsDocShellLoadState(
    const DocShellLoadStateInit& aLoadState) {
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
  mLoadFlags = aLoadState.LoadFlags();
  mFirstParty = aLoadState.FirstParty();
  mTypeHint = aLoadState.TypeHint();
  mFileName = aLoadState.FileName();
  mIsFromProcessingFrameAttributes =
      aLoadState.IsFromProcessingFrameAttributes();
  mReferrerInfo = aLoadState.ReferrerInfo();
  mURI = aLoadState.URI();
  mOriginalURI = aLoadState.OriginalURI();
  mBaseURI = aLoadState.BaseURI();
  mTriggeringPrincipal = aLoadState.TriggeringPrincipal();
  mPrincipalToInherit = aLoadState.PrincipalToInherit();
  mStoragePrincipalToInherit = aLoadState.StoragePrincipalToInherit();
  mCsp = aLoadState.Csp();
  mOriginalURIString = aLoadState.OriginalURIString();
  mCancelContentJSEpoch = aLoadState.CancelContentJSEpoch();
  mPostDataStream = aLoadState.PostDataStream();
  mHeadersStream = aLoadState.HeadersStream();
  mSrcdocData = aLoadState.SrcdocData();
  mResultPrincipalURI = aLoadState.ResultPrincipalURI();
  if (!aLoadState.SHEntry() || !StaticPrefs::fission_sessionHistoryInParent()) {
    return;
  }
  if (XRE_IsParentProcess()) {
    mSHEntry = static_cast<LegacySHEntry*>(aLoadState.SHEntry());
  } else {
    mSHEntry = static_cast<SHEntryChild*>(aLoadState.SHEntry());
  }
}

nsDocShellLoadState::~nsDocShellLoadState() {}

nsresult nsDocShellLoadState::CreateFromPendingChannel(
    nsIChannel* aPendingChannel, nsDocShellLoadState** aResult) {
  // Create the nsDocShellLoadState object with default state pulled from the
  // passed-in channel.
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPendingChannel->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState(uri);
  loadState->mPendingRedirectedChannel = aPendingChannel;

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

nsresult nsDocShellLoadState::CreateFromLoadURIOptions(
    nsISupports* aConsumer, nsIURIFixup* aURIFixup, const nsAString& aURI,
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

  nsCOMPtr<nsIURIFixupInfo> fixupInfo;
  if (aURIFixup) {
    uint32_t fixupFlags;
    rv = aURIFixup->WebNavigationFlagsToFixupFlags(uriString, loadFlags,
                                                   &fixupFlags);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // If we don't allow keyword lookups for this URL string, make sure to
    // update loadFlags to indicate this as well.
    if (!(fixupFlags & nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP)) {
      loadFlags &= ~nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
    }
    // The consumer is either a DocShell or an Element.
    nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(aConsumer);
    if (!loadContext) {
      if (RefPtr<Element> element = do_QueryObject(aConsumer)) {
        loadContext = do_QueryInterface(element->OwnerDoc()->GetDocShell());
      }
    }
    // Ensure URIFixup will use the right search engine in Private Browsing.
    MOZ_ASSERT(loadContext, "We should always have a LoadContext here.");
    if (loadContext && loadContext->UsePrivateBrowsing()) {
      fixupFlags |= nsIURIFixup::FIXUP_FLAG_PRIVATE_CONTEXT;
    }
    nsCOMPtr<nsIInputStream> fixupStream;
    rv = aURIFixup->GetFixupURIInfo(uriString, fixupFlags,
                                    getter_AddRefs(fixupStream),
                                    getter_AddRefs(fixupInfo));

    if (NS_SUCCEEDED(rv)) {
      fixupInfo->GetPreferredURI(getter_AddRefs(uri));
      fixupInfo->SetConsumer(aConsumer);
    }

    if (fixupStream) {
      // GetFixupURIInfo only returns a post data stream if it succeeded
      // and changed the URI, in which case we should override the
      // passed-in post data.
      postData = fixupStream;
    }

    if (loadFlags & nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) {
      nsCOMPtr<nsIObserverService> serv = services::GetObserverService();
      if (serv) {
        serv->NotifyObservers(fixupInfo, "keyword-uri-fixup",
                              PromiseFlatString(aURI).get());
      }
    }
  } else {
    // No fixup service so just create a URI and see what happens...
    rv = NS_NewURI(getter_AddRefs(uri), uriString);
    loadFlags &= ~nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
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

  /*
   * If the user "Disables Protection on This Page", we have to make sure to
   * remember the users decision when opening links in child tabs [Bug 906190]
   */
  if (loadFlags & nsIWebNavigation::LOAD_FLAGS_ALLOW_MIXED_CONTENT) {
    loadState->SetLoadType(
        MAKE_LOAD_TYPE(LOAD_NORMAL_ALLOW_MIXED_CONTENT, loadFlags));
  } else {
    loadState->SetLoadType(MAKE_LOAD_TYPE(LOAD_NORMAL, loadFlags));
  }

  loadState->SetLoadFlags(extraFlags);
  loadState->SetFirstParty(true);
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

  if (fixupInfo) {
    nsAutoString searchProvider, keyword;
    fixupInfo->GetKeywordProviderName(searchProvider);
    fixupInfo->GetKeywordAsSent(keyword);
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

nsIPrincipal* nsDocShellLoadState::StoragePrincipalToInherit() const {
  return mStoragePrincipalToInherit;
}

void nsDocShellLoadState::SetStoragePrincipalToInherit(
    nsIPrincipal* aStoragePrincipalToInherit) {
  mStoragePrincipalToInherit = aStoragePrincipalToInherit;
}

void nsDocShellLoadState::SetCsp(nsIContentSecurityPolicy* aCsp) {
  mCsp = aCsp;
}

nsIContentSecurityPolicy* nsDocShellLoadState::Csp() const { return mCsp; }

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

bool nsDocShellLoadState::FirstParty() const { return mFirstParty; }

void nsDocShellLoadState::SetFirstParty(bool aFirstParty) {
  mFirstParty = aFirstParty;
}

const nsCString& nsDocShellLoadState::TypeHint() const { return mTypeHint; }

void nsDocShellLoadState::SetTypeHint(const nsCString& aTypeHint) {
  mTypeHint = aTypeHint;
}

const nsString& nsDocShellLoadState::FileName() const { return mFileName; }

void nsDocShellLoadState::SetFileName(const nsAString& aFileName) {
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
  uint32_t oldLoadFlags = mLoadFlags;
  mLoadFlags = 0;

  if (mInheritPrincipal) {
    MOZ_ASSERT(
        !mPrincipalToInherit || !mPrincipalToInherit->IsSystemPrincipal(),
        "Should not inherit SystemPrincipal");
    mLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_INHERIT_PRINCIPAL;
  }

  if (mReferrerInfo && !mReferrerInfo->GetSendReferrer()) {
    mLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER;
  }
  if (oldLoadFlags & nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) {
    mLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
  }

  if (oldLoadFlags & nsIWebNavigation::LOAD_FLAGS_FIRST_LOAD) {
    mLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_FIRST_LOAD;
  }

  if (oldLoadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_CLASSIFIER) {
    mLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_BYPASS_CLASSIFIER;
  }

  if (oldLoadFlags & nsIWebNavigation::LOAD_FLAGS_FORCE_ALLOW_COOKIES) {
    mLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_FORCE_ALLOW_COOKIES;
  }

  if (oldLoadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE) {
    mLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE;
  }

  if (!mSrcdocData.IsVoid()) {
    mLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_IS_SRCDOC;
  }

  if (mForceAllowDataURI) {
    mLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_FORCE_ALLOW_DATA_URI;
  }

  if (mOriginalFrameSrc) {
    mLoadFlags |= nsDocShell::INTERNAL_LOAD_FLAGS_ORIGINAL_FRAME_SRC;
  }
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
  loadState.LoadFlags() = mLoadFlags;
  loadState.FirstParty() = mFirstParty;
  loadState.TypeHint() = mTypeHint;
  loadState.FileName() = mFileName;
  loadState.IsFromProcessingFrameAttributes() =
      mIsFromProcessingFrameAttributes;
  loadState.URI() = mURI;
  loadState.OriginalURI() = mOriginalURI;
  loadState.BaseURI() = mBaseURI;
  loadState.TriggeringPrincipal() = mTriggeringPrincipal;
  loadState.PrincipalToInherit() = mPrincipalToInherit;
  loadState.StoragePrincipalToInherit() = mStoragePrincipalToInherit;
  loadState.Csp() = mCsp;
  loadState.OriginalURIString() = mOriginalURIString;
  loadState.CancelContentJSEpoch() = mCancelContentJSEpoch;
  loadState.ReferrerInfo() = mReferrerInfo;
  loadState.PostDataStream() = mPostDataStream;
  loadState.HeadersStream() = mHeadersStream;
  loadState.SrcdocData() = mSrcdocData;
  loadState.ResultPrincipalURI() = mResultPrincipalURI;
  if (!mSHEntry || !StaticPrefs::fission_sessionHistoryInParent()) {
    // Without the pref, we don't have an actor for shentry and thus
    // we can't serialize it. We could write custom (de)serializers,
    // but a session history rewrite is on the way anyway.
    return loadState;
  }
  if (XRE_IsParentProcess()) {
    loadState.SHEntry() = static_cast<CrossProcessSHEntry*>(
        static_cast<LegacySHEntry*>(mSHEntry.get()));
  } else {
    loadState.SHEntry() = static_cast<CrossProcessSHEntry*>(
        static_cast<SHEntryChild*>(mSHEntry.get()));
  }
  return loadState;
}
