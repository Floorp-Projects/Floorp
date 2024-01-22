/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WindowGlobalParent_h
#define mozilla_dom_WindowGlobalParent_h

#include "mozilla/ContentBlockingLog.h"
#include "mozilla/ContentBlockingNotifier.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/PWindowGlobalParent.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/dom/WindowGlobalActorsBinding.h"
#include "nsTHashMap.h"
#include "nsRefPtrHashtable.h"
#include "nsWrapperCache.h"
#include "nsISupports.h"
#include "nsIDOMProcessParent.h"
#include "mozilla/dom/WindowGlobalActor.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/net/CookieJarSettings.h"

class nsIPrincipal;
class nsIURI;
class nsFrameLoader;

namespace mozilla {

namespace gfx {
class CrossProcessPaint;
}  // namespace gfx

namespace dom {

class BrowserParent;
class WindowGlobalChild;
class JSWindowActorParent;
class JSActorMessageMeta;
struct PageUseCounters;
class WindowSessionStoreState;
struct WindowSessionStoreUpdate;
class SSCacheQueryResult;

/**
 * A handle in the parent process to a specific nsGlobalWindowInner object.
 */
class WindowGlobalParent final : public WindowContext,
                                 public WindowGlobalActor,
                                 public PWindowGlobalParent {
  friend class gfx::CrossProcessPaint;
  friend class PWindowGlobalParent;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(WindowGlobalParent,
                                                         WindowContext)

  static already_AddRefed<WindowGlobalParent> GetByInnerWindowId(
      uint64_t aInnerWindowId);

  static already_AddRefed<WindowGlobalParent> GetByInnerWindowId(
      const GlobalObject& aGlobal, uint64_t aInnerWindowId) {
    return GetByInnerWindowId(aInnerWindowId);
  }

  // The same as the corresponding methods on `WindowContext`, except that the
  // return types are already cast to their parent-process type variants, such
  // as `WindowGlobalParent` or `CanonicalBrowsingContext`.
  WindowGlobalParent* GetParentWindowContext() {
    return static_cast<WindowGlobalParent*>(
        WindowContext::GetParentWindowContext());
  }
  WindowGlobalParent* TopWindowContext() {
    return static_cast<WindowGlobalParent*>(WindowContext::TopWindowContext());
  }
  CanonicalBrowsingContext* GetBrowsingContext() const {
    return CanonicalBrowsingContext::Cast(WindowContext::GetBrowsingContext());
  }

  Element* GetRootOwnerElement();

  // Has this actor been shut down
  bool IsClosed() { return !CanSend(); }

  // Get the other side of this actor if it is an in-process actor. Returns
  // |nullptr| if the actor has been torn down, or is not in-process.
  already_AddRefed<WindowGlobalChild> GetChildActor();

  // Get a JS actor object by name.
  already_AddRefed<JSWindowActorParent> GetActor(JSContext* aCx,
                                                 const nsACString& aName,
                                                 ErrorResult& aRv);
  already_AddRefed<JSWindowActorParent> GetExistingActor(
      const nsACString& aName);

  // Get this actor's manager if it is not an in-process actor. Returns
  // |nullptr| if the actor has been torn down, or is in-process.
  BrowserParent* GetBrowserParent();

  ContentParent* GetContentParent();

  // The principal of this WindowGlobal. This value will not change over the
  // lifetime of the WindowGlobal object, even to reflect changes in
  // |document.domain|.
  nsIPrincipal* DocumentPrincipal() { return mDocumentPrincipal; }

  nsIPrincipal* DocumentStoragePrincipal() { return mDocumentStoragePrincipal; }

  // The BrowsingContext which this WindowGlobal has been loaded into.
  // FIXME: It's quite awkward that this method has a slightly different name
  // than the one on WindowContext.
  CanonicalBrowsingContext* BrowsingContext() override {
    return GetBrowsingContext();
  }

  // Get the root nsFrameLoader object for the tree of BrowsingContext nodes
  // which this WindowGlobal is a part of. This will be the nsFrameLoader
  // holding the BrowserParent for remote tabs, and the root content frameloader
  // for non-remote tabs.
  already_AddRefed<nsFrameLoader> GetRootFrameLoader();

  // The current URI which loaded in the document.
  nsIURI* GetDocumentURI() override { return mDocumentURI; }

  void GetDocumentTitle(nsAString& aTitle) const {
    aTitle = mDocumentTitle.valueOr(nsString());
  }

  nsIPrincipal* GetContentBlockingAllowListPrincipal() const {
    return mDocContentBlockingAllowListPrincipal;
  }

  Maybe<ClientInfo> GetClientInfo() { return mClientInfo; }

  uint64_t ContentParentId();

  int32_t OsPid();

  bool IsCurrentGlobal();

  bool IsProcessRoot();

  uint32_t ContentBlockingEvents();

  void GetContentBlockingLog(nsAString& aLog);

  bool IsInitialDocument() {
    return mIsInitialDocument.isSome() && mIsInitialDocument.value();
  }

  already_AddRefed<mozilla::dom::Promise> PermitUnload(
      PermitUnloadAction aAction, uint32_t aTimeout, mozilla::ErrorResult& aRv);

  void PermitUnload(std::function<void(bool)>&& aResolver);

  already_AddRefed<mozilla::dom::Promise> DrawSnapshot(
      const DOMRect* aRect, double aScale, const nsACString& aBackgroundColor,
      bool aResetScrollPosition, mozilla::ErrorResult& aRv);

  static already_AddRefed<WindowGlobalParent> CreateDisconnected(
      const WindowGlobalInit& aInit);

  // Initialize the mFrameLoader fields for a created WindowGlobalParent. Must
  // be called after setting the Manager actor.
  void Init() final;

  nsIGlobalObject* GetParentObject();
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void NotifyContentBlockingEvent(
      uint32_t aEvent, nsIRequest* aRequest, bool aBlocked,
      const nsACString& aTrackingOrigin,
      const nsTArray<nsCString>& aTrackingFullHashes,
      const Maybe<
          ContentBlockingNotifier::StorageAccessPermissionGrantedReason>&
          aReason,
      const Maybe<ContentBlockingNotifier::CanvasFingerprinter>&
          aCanvasFingerprinter,
      const Maybe<bool> aCanvasFingerprinterKnownText);

  ContentBlockingLog* GetContentBlockingLog() { return &mContentBlockingLog; }

  nsIDOMProcessParent* GetDomProcess();

  nsICookieJarSettings* CookieJarSettings() { return mCookieJarSettings; }

  nsICookieJarSettings* GetCookieJarSettings() const {
    return mCookieJarSettings;
  }

  bool DocumentHasLoaded() { return mDocumentHasLoaded; }

  bool DocumentHasUserInteracted() { return mDocumentHasUserInteracted; }

  uint32_t SandboxFlags() { return mSandboxFlags; }

  bool GetDocumentBlockAllMixedContent() { return mBlockAllMixedContent; }

  bool GetDocumentUpgradeInsecureRequests() { return mUpgradeInsecureRequests; }

  void DidBecomeCurrentWindowGlobal(bool aCurrent);

  uint32_t HttpsOnlyStatus() { return mHttpsOnlyStatus; }

  void AddSecurityState(uint32_t aStateFlags);
  uint32_t GetSecurityFlags() { return mSecurityState; }

  nsITransportSecurityInfo* GetSecurityInfo() { return mSecurityInfo; }

  const nsACString& GetRemoteType() override;

  void NotifySessionStoreUpdatesComplete(Element* aEmbedder);

  Maybe<uint64_t> GetSingleChannelId() { return mSingleChannelId; }

  uint32_t GetBFCacheStatus() { return mBFCacheStatus; }

  bool HasActivePeerConnections();

  bool Fullscreen() { return mFullscreen; }
  void SetFullscreen(bool aFullscreen) { mFullscreen = aFullscreen; }

  void ExitTopChromeDocumentFullscreen();

  void SetShouldReportHasBlockedOpaqueResponse(
      nsContentPolicyType aContentPolicy);

 protected:
  already_AddRefed<JSActor> InitJSActor(JS::Handle<JSObject*> aMaybeActor,
                                        const nsACString& aName,
                                        ErrorResult& aRv) override;
  mozilla::ipc::IProtocol* AsNativeActor() override { return this; }

  // IPC messages
  mozilla::ipc::IPCResult RecvLoadURI(
      const MaybeDiscarded<dom::BrowsingContext>& aTargetBC,
      nsDocShellLoadState* aLoadState, bool aSetNavigating);
  mozilla::ipc::IPCResult RecvInternalLoad(nsDocShellLoadState* aLoadState);
  mozilla::ipc::IPCResult RecvUpdateDocumentURI(NotNull<nsIURI*> aURI);
  mozilla::ipc::IPCResult RecvUpdateDocumentPrincipal(
      nsIPrincipal* aNewDocumentPrincipal,
      nsIPrincipal* aNewDocumentStoragePrincipal);
  mozilla::ipc::IPCResult RecvUpdateDocumentHasLoaded(bool aDocumentHasLoaded);
  mozilla::ipc::IPCResult RecvUpdateDocumentHasUserInteracted(
      bool aDocumentHasUserInteracted);
  mozilla::ipc::IPCResult RecvUpdateSandboxFlags(uint32_t aSandboxFlags);
  mozilla::ipc::IPCResult RecvUpdateDocumentCspSettings(
      bool aBlockAllMixedContent, bool aUpgradeInsecureRequests);
  mozilla::ipc::IPCResult RecvUpdateDocumentTitle(const nsString& aTitle);
  mozilla::ipc::IPCResult RecvUpdateHttpsOnlyStatus(uint32_t aHttpsOnlyStatus);
  mozilla::ipc::IPCResult RecvSetIsInitialDocument(bool aIsInitialDocument) {
    if (aIsInitialDocument && mIsInitialDocument.isSome() &&
        (mIsInitialDocument.value() != aIsInitialDocument)) {
      return IPC_FAIL_NO_REASON(this);
    }

    mIsInitialDocument = Some(aIsInitialDocument);
    return IPC_OK();
  }
  mozilla::ipc::IPCResult RecvUpdateDocumentSecurityInfo(
      nsITransportSecurityInfo* aSecurityInfo);
  mozilla::ipc::IPCResult RecvSetClientInfo(
      const IPCClientInfo& aIPCClientInfo);
  mozilla::ipc::IPCResult RecvDestroy();
  mozilla::ipc::IPCResult RecvRawMessage(
      const JSActorMessageMeta& aMeta, const Maybe<ClonedMessageData>& aData,
      const Maybe<ClonedMessageData>& aStack);

  mozilla::ipc::IPCResult RecvGetContentBlockingEvents(
      GetContentBlockingEventsResolver&& aResolver);
  mozilla::ipc::IPCResult RecvUpdateCookieJarSettings(
      const CookieJarSettingsArgs& aCookieJarSettingsArgs);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  void DrawSnapshotInternal(gfx::CrossProcessPaint* aPaint,
                            const Maybe<IntRect>& aRect, float aScale,
                            nscolor aBackgroundColor, uint32_t aFlags);

  // WebShare API - try to share
  mozilla::ipc::IPCResult RecvShare(IPCWebShareData&& aData,
                                    ShareResolver&& aResolver);

  mozilla::ipc::IPCResult RecvCheckPermitUnload(
      bool aHasInProcessBlocker, XPCOMPermitUnloadAction aAction,
      CheckPermitUnloadResolver&& aResolver);

  mozilla::ipc::IPCResult RecvExpectPageUseCounters(
      const MaybeDiscarded<dom::WindowContext>& aTop);
  mozilla::ipc::IPCResult RecvAccumulatePageUseCounters(
      const UseCounters& aUseCounters);

  mozilla::ipc::IPCResult RecvRequestRestoreTabContent();

  mozilla::ipc::IPCResult RecvUpdateBFCacheStatus(const uint32_t& aOnFlags,
                                                  const uint32_t& aOffFlags);

  // This IPC method is to notify the parent process that the caller process
  // creates the first active peer connection (aIsAdded = true) or closes the
  // last active peer connection (aIsAdded = false).
  mozilla::ipc::IPCResult RecvUpdateActivePeerConnectionStatus(bool aIsAdded);

 public:
  mozilla::ipc::IPCResult RecvSetSingleChannelId(
      const Maybe<uint64_t>& aSingleChannelId);

  mozilla::ipc::IPCResult RecvSetDocumentDomain(NotNull<nsIURI*> aDomain);

  mozilla::ipc::IPCResult RecvReloadWithHttpsOnlyException();

  mozilla::ipc::IPCResult RecvDiscoverIdentityCredentialFromExternalSource(
      const IdentityCredentialRequestOptions& aOptions,
      const DiscoverIdentityCredentialFromExternalSourceResolver& aResolver);

  mozilla::ipc::IPCResult RecvGetStorageAccessPermission(
      GetStorageAccessPermissionResolver&& aResolve);

  mozilla::ipc::IPCResult RecvSetCookies(
      const nsCString& aBaseDomain, const OriginAttributes& aOriginAttributes,
      nsIURI* aHost, bool aFromHttp, const nsTArray<CookieStruct>& aCookies);

 private:
  WindowGlobalParent(CanonicalBrowsingContext* aBrowsingContext,
                     uint64_t aInnerWindowId, uint64_t aOuterWindowId,
                     FieldValues&& aInit);

  ~WindowGlobalParent();

  bool ShouldTrackSiteOriginTelemetry();
  void FinishAccumulatingPageUseCounters();

  // Returns failure if the new storage principal cannot be validated
  // against the current document principle.
  nsresult SetDocumentStoragePrincipal(
      nsIPrincipal* aNewDocumentStoragePrincipal);

  // NOTE: Neither this document principal nor the document storage
  // principal doesn't reflect possible |document.domain| mutations
  // which may have been made in the actual document.
  nsCOMPtr<nsIPrincipal> mDocumentPrincipal;
  nsCOMPtr<nsIPrincipal> mDocumentStoragePrincipal;

  // The principal to use for the content blocking allow list.
  nsCOMPtr<nsIPrincipal> mDocContentBlockingAllowListPrincipal;

  nsCOMPtr<nsIURI> mDocumentURI;
  Maybe<nsString> mDocumentTitle;

  Maybe<bool> mIsInitialDocument;

  // True if this window has a "beforeunload" event listener.
  bool mHasBeforeUnload;

  // The log of all content blocking actions taken on the document related to
  // this WindowGlobalParent. This is only stored on top-level documents and
  // includes the activity log for all of the nested subdocuments as well.
  ContentBlockingLog mContentBlockingLog;

  uint32_t mSecurityState = 0;

  Maybe<ClientInfo> mClientInfo;
  // Fields being mirrored from the corresponding document
  nsCOMPtr<nsICookieJarSettings> mCookieJarSettings;
  nsCOMPtr<nsITransportSecurityInfo> mSecurityInfo;

  uint32_t mSandboxFlags;

  struct OriginCounter {
    void UpdateSiteOriginsFrom(WindowGlobalParent* aParent, bool aIncrease);
    void Accumulate();

    nsTHashMap<nsCStringHashKey, int32_t> mOriginMap;
    uint32_t mMaxOrigins = 0;
  };

  // Used to collect unique site origin telemetry.
  //
  // Is only Some() on top-level content windows.
  Maybe<OriginCounter> mOriginCounter;

  bool mDocumentHasLoaded;
  bool mDocumentHasUserInteracted;
  bool mDocumentTreeWouldPreloadResources = false;
  bool mBlockAllMixedContent;
  bool mUpgradeInsecureRequests;

  // HTTPS-Only Mode flags
  uint32_t mHttpsOnlyStatus;

  // The window of the document whose page use counters our document's use
  // counters will contribute to.  (If we are a top-level document, this
  // will point to ourselves.)
  RefPtr<WindowGlobalParent> mPageUseCountersWindow;

  // Our page use counters, if we are a top-level document.
  UniquePtr<PageUseCounters> mPageUseCounters;

  // Whether we have sent our page use counters, and so should ignore any
  // subsequent ExpectPageUseCounters calls.
  bool mSentPageUseCounters = false;

  uint32_t mBFCacheStatus = 0;

  // If this WindowGlobalParent is a top window, this field indicates how many
  // child processes have active peer connections for this window and its
  // descendants.
  uint32_t mNumOfProcessesWithActivePeerConnections = 0;

  // mSingleChannelId records whether the loadgroup contains a single request
  // with an id. If there is one channel in the loadgroup and it has an id then
  // mSingleChannelId is set to Some(id) (ids are non-zero). If there is one
  // request in the loadgroup and it's not a channel or it doesn't have an id,
  // or there are multiple requests in the loadgroup, then mSingleChannelId is
  // set to Some(0). If there are no requests in the loadgroup then
  // mSingleChannelId is set to Nothing().
  // Note: We ignore favicon loads when considering the requests in the
  // loadgroup.
  Maybe<uint64_t> mSingleChannelId;

  // True if the current loaded document is in fullscreen.
  bool mFullscreen = false;

  bool mShouldReportHasBlockedOpaqueResponse = false;
};

}  // namespace dom
}  // namespace mozilla

inline nsISupports* ToSupports(
    mozilla::dom::WindowGlobalParent* aWindowGlobal) {
  return static_cast<mozilla::dom::WindowContext*>(aWindowGlobal);
}

#endif  // !defined(mozilla_dom_WindowGlobalParent_h)
