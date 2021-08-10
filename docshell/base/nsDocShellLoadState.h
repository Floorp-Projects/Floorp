/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDocShellLoadState_h__
#define nsDocShellLoadState_h__

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/SessionHistoryEntry.h"

// Helper Classes
#include "mozilla/Maybe.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsDocShellLoadTypes.h"
#include "nsTArrayForwardDeclare.h"

class nsIContentSecurityPolicy;
class nsIInputStream;
class nsISHEntry;
class nsIURI;
class nsIDocShell;
class nsIChannel;
class nsIReferrerInfo;
class OriginAttibutes;
namespace mozilla {
template <typename, class>
class UniquePtr;
namespace dom {
class DocShellLoadStateInit;
}  // namespace dom
}  // namespace mozilla

/**
 * nsDocShellLoadState contains setup information used in a nsIDocShell::loadURI
 * call.
 */
class nsDocShellLoadState final {
  using BrowsingContext = mozilla::dom::BrowsingContext;
  template <typename T>
  using MaybeDiscarded = mozilla::dom::MaybeDiscarded<T>;

 public:
  NS_INLINE_DECL_REFCOUNTING(nsDocShellLoadState);

  explicit nsDocShellLoadState(nsIURI* aURI);
  explicit nsDocShellLoadState(
      const mozilla::dom::DocShellLoadStateInit& aLoadState);
  explicit nsDocShellLoadState(const nsDocShellLoadState& aOther);
  nsDocShellLoadState(nsIURI* aURI, uint64_t aLoadIdentifier);

  static nsresult CreateFromPendingChannel(nsIChannel* aPendingChannel,
                                           uint64_t aLoadIdentifier,
                                           uint64_t aRegistarId,
                                           nsDocShellLoadState** aResult);

  static nsresult CreateFromLoadURIOptions(
      BrowsingContext* aBrowsingContext, const nsAString& aURI,
      const mozilla::dom::LoadURIOptions& aLoadURIOptions,
      nsDocShellLoadState** aResult);

  // Getters and Setters

  nsIReferrerInfo* GetReferrerInfo() const;

  void SetReferrerInfo(nsIReferrerInfo* aReferrerInfo);

  nsIURI* URI() const;

  void SetURI(nsIURI* aURI);

  nsIURI* OriginalURI() const;

  void SetOriginalURI(nsIURI* aOriginalURI);

  nsIURI* ResultPrincipalURI() const;

  void SetResultPrincipalURI(nsIURI* aResultPrincipalURI);

  bool ResultPrincipalURIIsSome() const;

  void SetResultPrincipalURIIsSome(bool aIsSome);

  bool KeepResultPrincipalURIIfSet() const;

  void SetKeepResultPrincipalURIIfSet(bool aKeep);

  nsIPrincipal* PrincipalToInherit() const;

  void SetPrincipalToInherit(nsIPrincipal* aPrincipalToInherit);

  nsIPrincipal* PartitionedPrincipalToInherit() const;

  void SetPartitionedPrincipalToInherit(
      nsIPrincipal* aPartitionedPrincipalToInherit);

  bool LoadReplace() const;

  void SetLoadReplace(bool aLoadReplace);

  nsIPrincipal* TriggeringPrincipal() const;

  void SetTriggeringPrincipal(nsIPrincipal* aTriggeringPrincipal);

  uint32_t TriggeringSandboxFlags() const;

  void SetTriggeringSandboxFlags(uint32_t aTriggeringSandboxFlags);

  nsIContentSecurityPolicy* Csp() const;

  void SetCsp(nsIContentSecurityPolicy* aCsp);

  bool InheritPrincipal() const;

  void SetInheritPrincipal(bool aInheritPrincipal);

  bool PrincipalIsExplicit() const;

  void SetPrincipalIsExplicit(bool aPrincipalIsExplicit);

  // If true, "beforeunload" event listeners were notified by the creater of the
  // LoadState and given the chance to abort the navigation, and should not be
  // notified again.
  bool NotifiedBeforeUnloadListeners() const;

  void SetNotifiedBeforeUnloadListeners(bool aNotifiedBeforeUnloadListeners);

  bool ForceAllowDataURI() const;

  void SetForceAllowDataURI(bool aForceAllowDataURI);

  bool IsExemptFromHTTPSOnlyMode() const;

  void SetIsExemptFromHTTPSOnlyMode(bool aIsExemptFromHTTPSOnlyMode);

  bool OriginalFrameSrc() const;

  void SetOriginalFrameSrc(bool aOriginalFrameSrc);

  bool IsFormSubmission() const;

  void SetIsFormSubmission(bool aIsFormSubmission);

  uint32_t LoadType() const;

  void SetLoadType(uint32_t aLoadType);

  nsISHEntry* SHEntry() const;

  void SetSHEntry(nsISHEntry* aSHEntry);

  const mozilla::dom::LoadingSessionHistoryInfo* GetLoadingSessionHistoryInfo()
      const;

  // Copies aLoadingInfo and stores the copy in this nsDocShellLoadState.
  void SetLoadingSessionHistoryInfo(
      const mozilla::dom::LoadingSessionHistoryInfo& aLoadingInfo);

  // Stores aLoadingInfo in this nsDocShellLoadState.
  void SetLoadingSessionHistoryInfo(
      mozilla::UniquePtr<mozilla::dom::LoadingSessionHistoryInfo> aLoadingInfo);

  bool LoadIsFromSessionHistory() const;

  const nsString& Target() const;

  void SetTarget(const nsAString& aTarget);

  nsIInputStream* PostDataStream() const;

  void SetPostDataStream(nsIInputStream* aStream);

  nsIInputStream* HeadersStream() const;

  void SetHeadersStream(nsIInputStream* aHeadersStream);

  bool IsSrcdocLoad() const;

  const nsString& SrcdocData() const;

  void SetSrcdocData(const nsAString& aSrcdocData);

  const MaybeDiscarded<BrowsingContext>& SourceBrowsingContext() const {
    return mSourceBrowsingContext;
  }

  void SetSourceBrowsingContext(BrowsingContext*);

  void SetAllowFocusMove(bool aAllow) { mAllowFocusMove = aAllow; }

  bool AllowFocusMove() const { return mAllowFocusMove; }

  const MaybeDiscarded<BrowsingContext>& TargetBrowsingContext() const {
    return mTargetBrowsingContext;
  }

  void SetTargetBrowsingContext(BrowsingContext* aTargetBrowsingContext);

  nsIURI* BaseURI() const;

  void SetBaseURI(nsIURI* aBaseURI);

  // Helper function allowing convenient work with mozilla::Maybe in C++, hiding
  // resultPrincipalURI and resultPrincipalURIIsSome attributes from the
  // consumer.
  void GetMaybeResultPrincipalURI(
      mozilla::Maybe<nsCOMPtr<nsIURI>>& aRPURI) const;

  void SetMaybeResultPrincipalURI(
      mozilla::Maybe<nsCOMPtr<nsIURI>> const& aRPURI);

  uint32_t LoadFlags() const;

  void SetLoadFlags(uint32_t aFlags);

  void SetLoadFlag(uint32_t aFlag);

  void UnsetLoadFlag(uint32_t aFlag);

  bool HasLoadFlags(uint32_t aFlag);

  uint32_t InternalLoadFlags() const;

  void SetInternalLoadFlags(uint32_t aFlags);

  void SetInternalLoadFlag(uint32_t aFlag);

  void UnsetInternalLoadFlag(uint32_t aFlag);

  bool HasInternalLoadFlags(uint32_t aFlag);

  bool FirstParty() const;

  void SetFirstParty(bool aFirstParty);

  bool HasValidUserGestureActivation() const;

  void SetHasValidUserGestureActivation(bool HasValidUserGestureActivation);

  const nsCString& TypeHint() const;

  void SetTypeHint(const nsCString& aTypeHint);

  const nsString& FileName() const;

  void SetFileName(const nsAString& aFileName);

  nsIURI* GetUnstrippedURI() const;

  // Give the type of DocShell we're loading into (chrome/content/etc) and
  // origin attributes for the URI we're loading, figure out if we should
  // inherit our principal from the document the load was requested from, or
  // else if the principal should be set up later in the process (after loads).
  // See comments in function for more info on principal selection algorithm
  nsresult SetupInheritingPrincipal(
      mozilla::dom::BrowsingContext::Type aType,
      const mozilla::OriginAttributes& aOriginAttributes);

  // If no triggering principal exists at the moment, create one using referrer
  // information and origin attributes.
  nsresult SetupTriggeringPrincipal(
      const mozilla::OriginAttributes& aOriginAttributes);

  void SetIsFromProcessingFrameAttributes() {
    mIsFromProcessingFrameAttributes = true;
  }
  bool GetIsFromProcessingFrameAttributes() const {
    return mIsFromProcessingFrameAttributes;
  }

  nsIChannel* GetPendingRedirectedChannel() {
    return mPendingRedirectedChannel;
  }

  uint64_t GetPendingRedirectChannelRegistrarId() const {
    return mChannelRegistrarId;
  }

  void SetOriginalURIString(const nsCString& aOriginalURI) {
    mOriginalURIString.emplace(aOriginalURI);
  }
  const mozilla::Maybe<nsCString>& GetOriginalURIString() const {
    return mOriginalURIString;
  }

  void SetCancelContentJSEpoch(int32_t aCancelEpoch) {
    mCancelContentJSEpoch.emplace(aCancelEpoch);
  }
  const mozilla::Maybe<int32_t>& GetCancelContentJSEpoch() const {
    return mCancelContentJSEpoch;
  }

  uint64_t GetLoadIdentifier() const { return mLoadIdentifier; }

  void SetChannelInitialized(bool aInitilized) {
    mChannelInitialized = aInitilized;
  }

  bool GetChannelInitialized() const { return mChannelInitialized; }

  void SetIsMetaRefresh(bool aMetaRefresh) { mIsMetaRefresh = aMetaRefresh; }

  bool IsMetaRefresh() const { return mIsMetaRefresh; }

  const mozilla::Maybe<nsCString>& GetRemoteTypeOverride() const {
    return mRemoteTypeOverride;
  }

  void SetRemoteTypeOverride(const nsCString& aRemoteTypeOverride) {
    mRemoteTypeOverride = mozilla::Some(aRemoteTypeOverride);
  }

  // When loading a document through nsDocShell::LoadURI(), a special set of
  // flags needs to be set based on other values in nsDocShellLoadState. This
  // function calculates those flags, before the LoadState is passed to
  // nsDocShell::InternalLoad.
  void CalculateLoadURIFlags();

  // Compute the load flags to be used by creating channel.  aUriModified and
  // aIsXFOError are expected to be Nothing when called from Parent process.
  nsLoadFlags CalculateChannelLoadFlags(
      mozilla::dom::BrowsingContext* aBrowsingContext,
      mozilla::Maybe<bool> aUriModified, mozilla::Maybe<bool> aIsXFOError);

  mozilla::dom::DocShellLoadStateInit Serialize();

  void SetLoadIsFromSessionHistory(int32_t aRequestedIndex,
                                   int32_t aSessionHistoryLength,
                                   bool aLoadingFromActiveEntry);
  void ClearLoadIsFromSessionHistory();

  void MaybeStripTrackerQueryStrings(mozilla::dom::BrowsingContext* aContext,
                                     nsIURI* aCurrentUnstrippedURI = nullptr);

 protected:
  // Destructor can't be defaulted or inlined, as header doesn't have all type
  // includes it needs to do so.
  ~nsDocShellLoadState();

 protected:
  // This is the referrer for the load.
  nsCOMPtr<nsIReferrerInfo> mReferrerInfo;

  // The URI we are navigating to. Will not be null once set.
  nsCOMPtr<nsIURI> mURI;

  // The URI to set as the originalURI on the channel that does the load. If
  // null, aURI will be set as the originalURI.
  nsCOMPtr<nsIURI> mOriginalURI;

  // The URI to be set to loadInfo.resultPrincipalURI
  // - When Nothing, there will be no change
  // - When Some, the principal URI will overwrite even
  //   with a null value.
  //
  // Valid only if mResultPrincipalURIIsSome is true (has the same meaning as
  // isSome() on mozilla::Maybe.)
  nsCOMPtr<nsIURI> mResultPrincipalURI;
  bool mResultPrincipalURIIsSome;

  // The principal of the load, that is, the entity responsible for causing the
  // load to occur. In most cases the referrer and the triggeringPrincipal's URI
  // will be identical.
  //
  // Please note that this is the principal that is used for security checks. If
  // the argument aURI is provided by the web, then please do not pass a
  // SystemPrincipal as the triggeringPrincipal.
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;

  // The SandboxFlags of the load, that are, the SandboxFlags of the entity
  // responsible for causing the load to occur. Most likely this are the
  // SandboxFlags of the document that started the load.
  uint32_t mTriggeringSandboxFlags;

  // The CSP of the load, that is, the CSP of the entity responsible for causing
  // the load to occur. Most likely this is the CSP of the document that started
  // the load. In case the entity starting the load did not use a CSP, then mCsp
  // can be null. Please note that this is also the CSP that will be applied to
  // the load in case the load encounters a server side redirect.
  nsCOMPtr<nsIContentSecurityPolicy> mCsp;

  // If a refresh is caused by http-equiv="refresh" we want to set
  // aResultPrincipalURI, but we do not want to overwrite the channel's
  // ResultPrincipalURI, if it has already been set on the channel by a protocol
  // handler.
  bool mKeepResultPrincipalURIIfSet;

  // If set LOAD_REPLACE flag will be set on the channel. If aOriginalURI is
  // null, this argument is ignored.
  bool mLoadReplace;

  // If this attribute is true and no triggeringPrincipal is specified,
  // copy the principal from the referring document.
  bool mInheritPrincipal;

  // If this attribute is true only ever use the principal specified
  // by the triggeringPrincipal and inheritPrincipal attributes.
  // If there are security reasons for why this is unsafe, such
  // as trying to use a systemprincipal as the triggeringPrincipal
  // for a content docshell the load fails.
  bool mPrincipalIsExplicit;

  bool mNotifiedBeforeUnloadListeners;

  // Principal we're inheriting. If null, this means the principal should be
  // inherited from the current document. If set to NullPrincipal, the channel
  // will fill in principal information later in the load. See internal comments
  // of SetupInheritingPrincipal for more info.
  //
  // When passed to InternalLoad, If this argument is null then
  // principalToInherit is computed differently. See nsDocShell::InternalLoad
  // for more comments.

  nsCOMPtr<nsIPrincipal> mPrincipalToInherit;

  nsCOMPtr<nsIPrincipal> mPartitionedPrincipalToInherit;

  // If this attribute is true, then a top-level navigation
  // to a data URI will be allowed.
  bool mForceAllowDataURI;

  // If this attribute is true, then the top-level navigaion
  // will be exempt from HTTPS-Only-Mode upgrades.
  bool mIsExemptFromHTTPSOnlyMode;

  // If this attribute is true, this load corresponds to a frame
  // element loading its original src (or srcdoc) attribute.
  bool mOriginalFrameSrc;

  // If this attribute is true, then the load was initiated by a
  // form submission. This is important to know for the CSP directive
  // navigate-to.
  bool mIsFormSubmission;

  // Contains a load type as specified by the nsDocShellLoadTypes::load*
  // constants
  uint32_t mLoadType;

  // Active Session History entry (if loading from SH)
  nsCOMPtr<nsISHEntry> mSHEntry;

  // Loading session history info for the load
  mozilla::UniquePtr<mozilla::dom::LoadingSessionHistoryInfo>
      mLoadingSessionHistoryInfo;

  // Target for load, like _content, _blank etc.
  nsString mTarget;

  // When set, this is the Target Browsing Context for the navigation
  // after retargeting.
  MaybeDiscarded<BrowsingContext> mTargetBrowsingContext;

  // Post data stream (if POSTing)
  nsCOMPtr<nsIInputStream> mPostDataStream;

  // Additional Headers
  nsCOMPtr<nsIInputStream> mHeadersStream;

  // When set, the load will be interpreted as a srcdoc load, where contents of
  // this string will be loaded instead of the URI. Setting srcdocData sets
  // isSrcdocLoad to true
  nsString mSrcdocData;

  // When set, this is the Source Browsing Context for the navigation.
  MaybeDiscarded<BrowsingContext> mSourceBrowsingContext;

  // Used for srcdoc loads to give view-source knowledge of the load's base URI
  // as this information isn't embedded in the load's URI.
  nsCOMPtr<nsIURI> mBaseURI;

  // Set of Load Flags, taken from nsDocShellLoadTypes.h and nsIWebNavigation
  uint32_t mLoadFlags;

  // Set of internal load flags
  uint32_t mInternalLoadFlags;

  // Is this a First Party Load?
  bool mFirstParty;

  // Is this load triggered by a user gesture?
  bool mHasValidUserGestureActivation;

  // Whether this load can steal the focus from the source browsing context.
  bool mAllowFocusMove;

  // A hint as to the content-type of the resulting data. If no hint, IsVoid()
  // should return true.
  nsCString mTypeHint;

  // Non-void when the link should be downloaded as the given filename.
  // mFileName being non-void but empty means that no filename hint was
  // specified, but link should still trigger a download. If not a download,
  // mFileName.IsVoid() should return true.
  nsString mFileName;

  // This will be true if this load is triggered by attribute changes.
  // See nsILoadInfo.isFromProcessingFrameAttributes
  bool mIsFromProcessingFrameAttributes;

  // If set, a pending cross-process redirected channel should be used to
  // perform the load. The channel will be stored in this value.
  nsCOMPtr<nsIChannel> mPendingRedirectedChannel;

  // An optional string representation of mURI, before any
  // fixups were applied, so that we can send it to a search
  // engine service if needed.
  mozilla::Maybe<nsCString> mOriginalURIString;

  // An optional value to pass to nsIDocShell::setCancelJSEpoch
  // when initiating the load.
  mozilla::Maybe<int32_t> mCancelContentJSEpoch;

  // If mPendingRedirectChannel is set, then this is the identifier
  // that the parent-process equivalent channel has been registered
  // with using RedirectChannelRegistrar.
  uint64_t mChannelRegistrarId;

  // An identifier to make it possible to examine if two loads are
  // equal, and which browsing context they belong to (see
  // BrowsingContext::{Get, Set}CurrentLoadIdentifier)
  const uint64_t mLoadIdentifier;

  // Optional value to indicate that a channel has been
  // pre-initialized in the parent process.
  bool mChannelInitialized;

  // True if the load was triggered by a meta refresh.
  bool mIsMetaRefresh;

  // The original URI before query stripping happened. If it's present, it shows
  // the query stripping happened. Otherwise, it will be a nullptr.
  nsCOMPtr<nsIURI> mUnstrippedURI;

  // If set, the remote type which the load should be completed within.
  mozilla::Maybe<nsCString> mRemoteTypeOverride;
};

#endif /* nsDocShellLoadState_h__ */
