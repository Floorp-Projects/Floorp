/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocShellLoadState.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIScriptSecurityManager.h"
#include "nsIWebNavigation.h"
#include "nsIChildChannel.h"
#include "ReferrerInfo.h"

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

nsDocShellLoadState::nsDocShellLoadState(DocShellLoadStateInit& aLoadState) {
  MOZ_ASSERT(aLoadState.URI(), "Cannot create a LoadState with a null URI!");
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
  mCsp = aLoadState.Csp();
}

nsDocShellLoadState::~nsDocShellLoadState() {}

nsresult nsDocShellLoadState::CreateFromPendingChannel(
    nsIChildChannel* aPendingChannel, nsDocShellLoadState** aResult) {
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aPendingChannel);
  if (NS_WARN_IF(!channel)) {
    return NS_ERROR_UNEXPECTED;
  }

  // Create the nsDocShellLoadState object with default state pulled from the
  // passed-in channel.
  nsCOMPtr<nsIURI> uri;
  nsresult rv = channel->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState(uri);
  loadState->mPendingRedirectedChannel = aPendingChannel;

  // Pull relevant state from the channel, and store it on the
  // nsDocShellLoadState.
  nsCOMPtr<nsIURI> originalUri;
  rv = channel->GetOriginalURI(getter_AddRefs(originalUri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  loadState->SetOriginalURI(originalUri);

  nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
  loadState->SetTriggeringPrincipal(loadInfo->TriggeringPrincipal());

  // Return the newly created loadState.
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

nsIDocShell* nsDocShellLoadState::SourceDocShell() const {
  return mSourceDocShell;
}

void nsDocShellLoadState::SetSourceDocShell(nsIDocShell* aSourceDocShell) {
  mSourceDocShell = aSourceDocShell;
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
    uint32_t aItemType, const mozilla::OriginAttributes& aOriginAttributes) {
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
  if (mPrincipalToInherit && aItemType != nsIDocShellTreeItem::typeChrome) {
    if (nsContentUtils::IsSystemPrincipal(mPrincipalToInherit)) {
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
    MOZ_ASSERT(!nsContentUtils::IsSystemPrincipal(mPrincipalToInherit),
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
  loadState.Csp() = mCsp;
  loadState.ReferrerInfo() = mReferrerInfo;
  return loadState;
}
