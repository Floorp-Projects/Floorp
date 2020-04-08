/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CanonicalBrowsingContext_h
#define mozilla_dom_CanonicalBrowsingContext_h

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/MediaControlKeysEvent.h"
#include "mozilla/RefPtr.h"
#include "mozilla/MozPromise.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsISHistory.h"
#include "nsISHEntry.h"

namespace mozilla {
namespace dom {

class WindowGlobalParent;
class BrowserParent;
class MediaController;
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

  bool IsOwnedByProcess(uint64_t aProcessId) const {
    return mProcessId == aProcessId;
  }
  bool IsEmbeddedInProcess(uint64_t aProcessId) const {
    return mEmbedderProcessId == aProcessId;
  }
  uint64_t OwnerProcessId() const { return mProcessId; }
  uint64_t EmbedderProcessId() const { return mEmbedderProcessId; }
  ContentParent* GetContentParent() const;

  void GetCurrentRemoteType(nsAString& aRemoteType, ErrorResult& aRv) const;

  void SetOwnerProcessId(uint64_t aProcessId);

  void SetInFlightProcessId(uint64_t aProcessId);
  uint64_t GetInFlightProcessId() const { return mInFlightProcessId; }

  void GetWindowGlobals(nsTArray<RefPtr<WindowGlobalParent>>& aWindows);

  // The current active WindowGlobal.
  WindowGlobalParent* GetCurrentWindowGlobal() const;

  already_AddRefed<nsIWidget> GetParentProcessWidgetContaining();

  already_AddRefed<WindowGlobalParent> GetEmbedderWindowGlobal() const;

  nsISHistory* GetSessionHistory();
  void SetSessionHistory(nsISHistory* aSHistory) {
    mSessionHistory = aSHistory;
  }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // This function is used to start the autoplay media which are delayed to
  // start. If needed, it would also notify the content browsing context which
  // are related with the canonical browsing content tree to start delayed
  // autoplay media.
  void NotifyStartDelayedAutoplayMedia();

  // This function is used to mute or unmute all media within a tab. It would
  // set the media mute property for the top level window and propagate it to
  // other top level windows in other processes.
  void NotifyMediaMutedChanged(bool aMuted);

  // Return the number of unique site origins by iterating all given BCs,
  // including their subtrees.
  static uint32_t CountSiteOrigins(
      GlobalObject& aGlobal,
      const Sequence<mozilla::OwningNonNull<BrowsingContext>>& aRoots);

  // This function would update the media action for the current outer window
  // and propogate the action to other browsing contexts in content processes.
  void UpdateMediaControlKeysEvent(MediaControlKeysEvent aEvent);

  // Triggers a load in the process
  using BrowsingContext::LoadURI;
  void LoadURI(const nsAString& aURI, const LoadURIOptions& aOptions,
               ErrorResult& aError);

  using RemotenessPromise = MozPromise<RefPtr<BrowserParent>, nsresult, false>;
  RefPtr<RemotenessPromise> ChangeFrameRemoteness(const nsAString& aRemoteType,
                                                  uint64_t aPendingSwitchId);

  // Return a media controller from the top-level browsing context that can
  // control all media belonging to this browsing context tree. Return nullptr
  // if the top-level browsing context has been discarded.
  MediaController* GetMediaController();

  bool HasHistoryEntry(nsISHEntry* aEntry) const {
    return aEntry && (aEntry == mOSHE || aEntry == mLSHE);
  }

  void UpdateSHEntries(nsISHEntry* aNewLSHE, nsISHEntry* aNewOSHE) {
    mLSHE = aNewLSHE;
    mOSHE = aNewOSHE;
  }

  void SwapHistoryEntries(nsISHEntry* aOldEntry, nsISHEntry* aNewEntry) {
    if (aOldEntry == mOSHE) {
      mOSHE = aNewEntry;
    }

    if (aOldEntry == mLSHE) {
      mLSHE = aNewEntry;
    }
  }

 protected:
  // Called when the browsing context is being discarded.
  void CanonicalDiscard();

  using Type = BrowsingContext::Type;
  CanonicalBrowsingContext(BrowsingContext* aParent,
                           BrowsingContextGroup* aGroup,
                           uint64_t aBrowsingContextId,
                           uint64_t aOwnerProcessId,
                           uint64_t aEmbedderProcessId, Type aType,
                           FieldTuple&& aFields);

 private:
  friend class BrowsingContext;

  ~CanonicalBrowsingContext() = default;

  class PendingRemotenessChange {
   public:
    NS_INLINE_DECL_REFCOUNTING(PendingRemotenessChange)

    PendingRemotenessChange(CanonicalBrowsingContext* aTarget,
                            RemotenessPromise::Private* aPromise,
                            uint64_t aPendingSwitchId)
        : mTarget(aTarget),
          mPromise(aPromise),
          mPendingSwitchId(aPendingSwitchId) {}

    void Cancel(nsresult aRv);
    void Complete(ContentParent* aContentParent);

   private:
    ~PendingRemotenessChange();
    void Clear();

    RefPtr<CanonicalBrowsingContext> mTarget;
    RefPtr<RemotenessPromise::Private> mPromise;

    uint64_t mPendingSwitchId;
  };

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

  nsCOMPtr<nsISHistory> mSessionHistory;

  // Tab media controller is used to control all media existing in the same
  // browsing context tree, so it would only exist in the top level browsing
  // context.
  RefPtr<MediaController> mTabMediaController;

  // These are being mirrored from docshell
  nsCOMPtr<nsISHEntry> mOSHE;
  nsCOMPtr<nsISHEntry> mLSHE;
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_CanonicalBrowsingContext_h)
