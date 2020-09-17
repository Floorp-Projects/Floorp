/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CanonicalBrowsingContext_h
#define mozilla_dom_CanonicalBrowsingContext_h

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/MediaControlKeySource.h"
#include "mozilla/dom/BrowsingContextWebProgress.h"
#include "mozilla/RefPtr.h"
#include "mozilla/MozPromise.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsISecureBrowserUI.h"

class nsISHistory;
class nsSHistory;
class nsBrowserStatusFilter;
class nsSecureBrowserUI;

namespace mozilla {
namespace net {
class DocumentLoadListener;
}

namespace dom {

class BrowserParent;
struct LoadURIOptions;
class MediaController;
struct LoadingSessionHistoryInfo;
class SessionHistoryEntry;
class WindowGlobalParent;

// CanonicalBrowsingContext is a BrowsingContext living in the parent
// process, with whatever extra data that a BrowsingContext in the
// parent needs.
class CanonicalBrowsingContext final : public BrowsingContext {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CanonicalBrowsingContext,
                                           BrowsingContext)

  static already_AddRefed<CanonicalBrowsingContext> Get(uint64_t aId);
  static CanonicalBrowsingContext* Cast(BrowsingContext* aContext);
  static const CanonicalBrowsingContext* Cast(const BrowsingContext* aContext);
  static already_AddRefed<CanonicalBrowsingContext> Cast(
      already_AddRefed<BrowsingContext>&& aContext);

  bool IsOwnedByProcess(uint64_t aProcessId) const {
    return mProcessId == aProcessId;
  }
  bool IsEmbeddedInProcess(uint64_t aProcessId) const {
    return mEmbedderProcessId == aProcessId;
  }
  uint64_t OwnerProcessId() const { return mProcessId; }
  uint64_t EmbedderProcessId() const { return mEmbedderProcessId; }
  ContentParent* GetContentParent() const;

  void GetCurrentRemoteType(nsACString& aRemoteType, ErrorResult& aRv) const;

  void SetOwnerProcessId(uint64_t aProcessId);

  void SetInFlightProcessId(uint64_t aProcessId);
  void ClearInFlightProcessId(uint64_t aProcessId);
  uint64_t GetInFlightProcessId() const { return mInFlightProcessId; }

  void GetWindowGlobals(nsTArray<RefPtr<WindowGlobalParent>>& aWindows);

  // The current active WindowGlobal.
  WindowGlobalParent* GetCurrentWindowGlobal() const;

  // Same as the methods on `BrowsingContext`, but with the types already cast
  // to the parent process type.
  CanonicalBrowsingContext* GetParent() {
    return Cast(BrowsingContext::GetParent());
  }
  CanonicalBrowsingContext* Top() { return Cast(BrowsingContext::Top()); }
  WindowGlobalParent* GetParentWindowContext();
  WindowGlobalParent* GetTopWindowContext();

  already_AddRefed<nsIWidget> GetParentProcessWidgetContaining();

  // Same as `GetParentWindowContext`, but will also cross <browser> and
  // content/chrome boundaries.
  already_AddRefed<WindowGlobalParent> GetEmbedderWindowGlobal() const;

  already_AddRefed<CanonicalBrowsingContext> GetParentCrossChromeBoundary();

  Nullable<WindowProxyHolder> GetTopChromeWindow();

  nsISHistory* GetSessionHistory();
  SessionHistoryEntry* GetActiveSessionHistoryEntry();

  UniquePtr<LoadingSessionHistoryInfo> CreateLoadingSessionHistoryEntryForLoad(
      nsDocShellLoadState* aLoadState, nsIChannel* aChannel);
  void SessionHistoryCommit(uint64_t aLoadId, const nsID& aChangeID);

  // Calls the session history listeners' OnHistoryReload, storing the result in
  // aCanReload. If aCanReload is set to true and we have an active or a loading
  // entry then aLoadState will be initialized from that entry, and
  // aReloadActiveEntry will be true if we have an active entry. If aCanReload
  // is true and aLoadState and aReloadActiveEntry are not set then we should
  // attempt to reload based on the current document in the docshell.
  void NotifyOnHistoryReload(bool& aCanReload,
                             Maybe<RefPtr<nsDocShellLoadState>>& aLoadState,
                             Maybe<bool>& aReloadActiveEntry);

  void SetActiveSessionHistoryEntryForTop(
      const Maybe<nsPoint>& aPreviousScrollPos, SessionHistoryInfo* aInfo,
      uint32_t aLoadType, const nsID& aChangeID);

  void SetActiveSessionHistoryEntryForFrame(
      const Maybe<nsPoint>& aPreviousScrollPos, SessionHistoryInfo* aInfo,
      int32_t aChildOffset, const nsID& aChangeID);

  void ReplaceActiveSessionHistoryEntry(SessionHistoryInfo* aInfo);

  void RemoveDynEntriesFromActiveSessionHistoryEntry();

  void RemoveFromSessionHistory();

  void HistoryGo(int32_t aIndex, std::function<void(int32_t&&)>&& aResolver);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // Dispatches a wheel zoom change to the embedder element.
  void DispatchWheelZoomChange(bool aIncrease);

  // This function is used to start the autoplay media which are delayed to
  // start. If needed, it would also notify the content browsing context which
  // are related with the canonical browsing content tree to start delayed
  // autoplay media.
  void NotifyStartDelayedAutoplayMedia();

  // This function is used to mute or unmute all media within a tab. It would
  // set the media mute property for the top level window and propagate it to
  // other top level windows in other processes.
  void NotifyMediaMutedChanged(bool aMuted, ErrorResult& aRv);

  // Return the number of unique site origins by iterating all given BCs,
  // including their subtrees.
  static uint32_t CountSiteOrigins(
      GlobalObject& aGlobal,
      const Sequence<mozilla::OwningNonNull<BrowsingContext>>& aRoots);

  // This function would propogate the action to its all child browsing contexts
  // in content processes.
  void UpdateMediaControlAction(const MediaControlAction& aAction);

  // Triggers a load in the process
  using BrowsingContext::LoadURI;
  void LoadURI(const nsAString& aURI, const LoadURIOptions& aOptions,
               ErrorResult& aError);

  void GoBack(const Optional<int32_t>& aCancelContentJSEpoch,
              bool aRequireUserInteraction);
  void GoForward(const Optional<int32_t>& aCancelContentJSEpoch,
                 bool aRequireUserInteraction);
  void GoToIndex(int32_t aIndex,
                 const Optional<int32_t>& aCancelContentJSEpoch);
  void Reload(uint32_t aReloadFlags);
  void Stop(uint32_t aStopFlags);

  // Internal method to change which process a BrowsingContext is being loaded
  // in. The returned promise will resolve when the process switch is completed.
  //
  // A NOT_REMOTE_TYPE aRemoteType argument will perform a process switch into
  // the parent process, and the method will resolve with a null BrowserParent.
  using RemotenessPromise = MozPromise<RefPtr<BrowserParent>, nsresult, false>;
  RefPtr<RemotenessPromise> ChangeRemoteness(const nsACString& aRemoteType,
                                             uint64_t aPendingSwitchId,
                                             bool aReplaceBrowsingContext,
                                             uint64_t aSpecificGroupId);

  // Return a media controller from the top-level browsing context that can
  // control all media belonging to this browsing context tree. Return nullptr
  // if the top-level browsing context has been discarded.
  MediaController* GetMediaController();

  // Attempts to start loading the given load state in this BrowsingContext,
  // without requiring any communication from a docshell. This will handle
  // computing the right process to load in, and organising handoff to
  // the right docshell when we get a response.
  bool LoadInParent(nsDocShellLoadState* aLoadState, bool aSetNavigating);

  // Attempts to start loading the given load state in this BrowsingContext,
  // in parallel with a DocumentChannelChild being created in the docshell.
  // Requires the DocumentChannel to connect with this load for it to
  // complete successfully.
  bool AttemptSpeculativeLoadInParent(nsDocShellLoadState* aLoadState);

  // Get or create a secure browser UI for this BrowsingContext
  nsISecureBrowserUI* GetSecureBrowserUI();

  BrowsingContextWebProgress* GetWebProgress() { return mWebProgress; }

  // Called when the current URI changes (from an
  // nsIWebProgressListener::OnLocationChange event, so that we
  // can update our security UI for the new location, or when the
  // mixed content state for our current window is changed.
  void UpdateSecurityStateForLocationOrMixedContentChange();

  void MaybeAddAsProgressListener(nsIWebProgress* aWebProgress);

  // Called when a navigation forces us to recreate our browsing
  // context (for example, when switching in or out of the parent
  // process).
  // aNewContext is the newly created BrowsingContext that is replacing
  // us.
  void ReplacedBy(CanonicalBrowsingContext* aNewContext);

  bool HasHistoryEntry(nsISHEntry* aEntry);

  void SwapHistoryEntries(nsISHEntry* aOldEntry, nsISHEntry* aNewEntry);

  void AddLoadingSessionHistoryEntry(uint64_t aLoadId,
                                     SessionHistoryEntry* aEntry);

  void GetLoadingSessionHistoryInfoFromParent(
      Maybe<LoadingSessionHistoryInfo>& aLoadingInfo, int32_t* aRequestedIndex,
      int32_t* aLength);

 protected:
  // Called when the browsing context is being discarded.
  void CanonicalDiscard();

  using Type = BrowsingContext::Type;
  CanonicalBrowsingContext(WindowContext* aParentWindow,
                           BrowsingContextGroup* aGroup,
                           uint64_t aBrowsingContextId,
                           uint64_t aOwnerProcessId,
                           uint64_t aEmbedderProcessId, Type aType,
                           FieldValues&& aInit);

 private:
  friend class BrowsingContext;

  ~CanonicalBrowsingContext() = default;

  class PendingRemotenessChange {
   public:
    NS_INLINE_DECL_REFCOUNTING(PendingRemotenessChange)

    PendingRemotenessChange(CanonicalBrowsingContext* aTarget,
                            RemotenessPromise::Private* aPromise,
                            uint64_t aPendingSwitchId,
                            bool aReplaceBrowsingContext);

    void Cancel(nsresult aRv);

   private:
    friend class CanonicalBrowsingContext;

    ~PendingRemotenessChange();
    void ProcessReady();
    void Finish();
    void Clear();

    RefPtr<CanonicalBrowsingContext> mTarget;
    RefPtr<RemotenessPromise::Private> mPromise;
    RefPtr<GenericPromise> mPrepareToChangePromise;
    RefPtr<ContentParent> mContentParent;
    RefPtr<BrowsingContextGroup> mSpecificGroup;

    uint64_t mPendingSwitchId;
    bool mReplaceBrowsingContext;
  };

  friend class net::DocumentLoadListener;
  // Called when a DocumentLoadListener is created to start a load for
  // this browsing context. Returns false if a higher priority load is
  // already in-progress and the new one has been rejected.
  bool StartDocumentLoad(net::DocumentLoadListener* aLoad);
  // Called once DocumentLoadListener completes handling a load, and it
  // is either complete, or handed off to the final channel to deliver
  // data to the destination docshell.
  void EndDocumentLoad(bool aForProcessSwitch);

  bool SupportsLoadingInParent(nsDocShellLoadState* aLoadState,
                               uint64_t* aOuterWindowId);

  void HistoryCommitIndexAndLength(const nsID& aChangeID);

  // XXX(farre): Store a ContentParent pointer here rather than mProcessId?
  // Indicates which process owns the docshell.
  uint64_t mProcessId;

  // Indicates which process owns the embedder element.
  uint64_t mEmbedderProcessId;

  // The ID of the former owner process during an ownership change, which may
  // have in-flight messages that assume it is still the owner.
  uint64_t mInFlightProcessId = 0;

  // The current remoteness change which is in a pending state.
  RefPtr<PendingRemotenessChange> mPendingRemotenessChange;

  RefPtr<nsSHistory> mSessionHistory;

  // Tab media controller is used to control all media existing in the same
  // browsing context tree, so it would only exist in the top level browsing
  // context.
  RefPtr<MediaController> mTabMediaController;

  RefPtr<net::DocumentLoadListener> mCurrentLoad;

  struct LoadingSessionHistoryEntry {
    uint64_t mLoadId = 0;
    RefPtr<SessionHistoryEntry> mEntry;
  };
  nsTArray<LoadingSessionHistoryEntry> mLoadingEntries;
  RefPtr<SessionHistoryEntry> mActiveEntry;

  RefPtr<nsSecureBrowserUI> mSecureBrowserUI;
  RefPtr<BrowsingContextWebProgress> mWebProgress;
  RefPtr<nsBrowserStatusFilter> mStatusFilter;
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_CanonicalBrowsingContext_h)
