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
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/PWindowGlobalParent.h"
#include "mozilla/dom/WindowContext.h"
#include "nsDataHashtable.h"
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
  CanonicalBrowsingContext* GetBrowsingContext() {
    return CanonicalBrowsingContext::Cast(WindowContext::GetBrowsingContext());
  }

  // Has this actor been shut down
  bool IsClosed() { return !CanSend(); }

  // Get the other side of this actor if it is an in-process actor. Returns
  // |nullptr| if the actor has been torn down, or is not in-process.
  already_AddRefed<WindowGlobalChild> GetChildActor();

  // Get a JS actor object by name.
  already_AddRefed<JSWindowActorParent> GetActor(const nsACString& aName,
                                                 ErrorResult& aRv);

  // Get this actor's manager if it is not an in-process actor. Returns
  // |nullptr| if the actor has been torn down, or is in-process.
  already_AddRefed<BrowserParent> GetBrowserParent();

  ContentParent* GetContentParent();

  void ReceiveRawMessage(const JSActorMessageMeta& aMeta,
                         ipc::StructuredCloneData&& aData,
                         ipc::StructuredCloneData&& aStack);

  // The principal of this WindowGlobal. This value will not change over the
  // lifetime of the WindowGlobal object, even to reflect changes in
  // |document.domain|.
  nsIPrincipal* DocumentPrincipal() { return mDocumentPrincipal; }

  nsIPrincipal* ContentBlockingAllowListPrincipal() {
    return mDocContentBlockingAllowListPrincipal;
  }

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

  void GetDocumentTitle(nsAString& aTitle) const { aTitle = mDocumentTitle; }

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

  bool IsInitialDocument() { return mIsInitialDocument; }

  bool HasBeforeUnload() { return mHasBeforeUnload; }

  already_AddRefed<mozilla::dom::Promise> DrawSnapshot(
      const DOMRect* aRect, double aScale, const nsACString& aBackgroundColor,
      mozilla::ErrorResult& aRv);

  already_AddRefed<Promise> GetSecurityInfo(ErrorResult& aRv);

  static already_AddRefed<WindowGlobalParent> CreateDisconnected(
      const WindowGlobalInit& aInit, bool aInProcess = false);

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
          aReason = Nothing());

  ContentBlockingLog* GetContentBlockingLog() { return &mContentBlockingLog; }

  nsIDOMProcessParent* GetDomProcess();

  nsICookieJarSettings* CookieJarSettings() { return mCookieJarSettings; }

  bool DocumentHasLoaded() { return mDocumentHasLoaded; }

  bool DocumentHasUserInteracted() { return mDocumentHasUserInteracted; }

  uint32_t SandboxFlags() { return mSandboxFlags; }

  bool GetDocumentBlockAllMixedContent() { return mBlockAllMixedContent; }

  bool GetDocumentUpgradeInsecureRequests() { return mUpgradeInsecureRequests; }

  void DidBecomeCurrentWindowGlobal(bool aCurrent);

  uint32_t HttpsOnlyStatus() { return mHttpsOnlyStatus; }

  void AddMixedContentSecurityState(uint32_t aStateFlags);
  uint32_t GetMixedContentSecurityFlags() { return mMixedContentSecurityState; }

  nsITransportSecurityInfo* GetSecurityInfo() { return mSecurityInfo; }

  const nsAString& GetRemoteType() override;

 protected:
  JSActor::Type GetSide() override { return JSActor::Type::Parent; }

  // IPC messages
  mozilla::ipc::IPCResult RecvLoadURI(
      const MaybeDiscarded<dom::BrowsingContext>& aTargetBC,
      nsDocShellLoadState* aLoadState, bool aSetNavigating);
  mozilla::ipc::IPCResult RecvInternalLoad(
      const MaybeDiscarded<dom::BrowsingContext>& aTargetBC,
      nsDocShellLoadState* aLoadState);
  mozilla::ipc::IPCResult RecvUpdateDocumentURI(nsIURI* aURI);
  mozilla::ipc::IPCResult RecvUpdateDocumentPrincipal(
      nsIPrincipal* aNewDocumentPrincipal);
  mozilla::ipc::IPCResult RecvUpdateDocumentHasLoaded(bool aDocumentHasLoaded);
  mozilla::ipc::IPCResult RecvUpdateDocumentHasUserInteracted(
      bool aDocumentHasUserInteracted);
  mozilla::ipc::IPCResult RecvUpdateSandboxFlags(uint32_t aSandboxFlags);
  mozilla::ipc::IPCResult RecvUpdateDocumentCspSettings(
      bool aBlockAllMixedContent, bool aUpgradeInsecureRequests);
  mozilla::ipc::IPCResult RecvUpdateDocumentTitle(const nsString& aTitle);
  mozilla::ipc::IPCResult RecvUpdateHttpsOnlyStatus(uint32_t aHttpsOnlyStatus);
  mozilla::ipc::IPCResult RecvSetIsInitialDocument(bool aIsInitialDocument) {
    mIsInitialDocument = aIsInitialDocument;
    return IPC_OK();
  }
  mozilla::ipc::IPCResult RecvUpdateDocumentSecurityInfo(
      nsITransportSecurityInfo* aSecurityInfo);
  mozilla::ipc::IPCResult RecvSetHasBeforeUnload(bool aHasBeforeUnload);
  mozilla::ipc::IPCResult RecvSetClientInfo(
      const IPCClientInfo& aIPCClientInfo);
  mozilla::ipc::IPCResult RecvDestroy();
  mozilla::ipc::IPCResult RecvRawMessage(const JSActorMessageMeta& aMeta,
                                         const ClonedMessageData& aData,
                                         const ClonedMessageData& aStack);

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

 private:
  WindowGlobalParent(CanonicalBrowsingContext* aBrowsingContext,
                     uint64_t aInnerWindowId, uint64_t aOuterWindowId,
                     bool aInProcess, FieldTuple&& aFields);

  ~WindowGlobalParent();

  bool ShouldTrackSiteOriginTelemetry();

  // NOTE: This document principal doesn't reflect possible |document.domain|
  // mutations which may have been made in the actual document.
  nsCOMPtr<nsIPrincipal> mDocumentPrincipal;
  nsCOMPtr<nsIPrincipal> mDocContentBlockingAllowListPrincipal;
  nsCOMPtr<nsIURI> mDocumentURI;
  nsString mDocumentTitle;

  nsRefPtrHashtable<nsCStringHashKey, JSWindowActorParent> mWindowActors;
  bool mIsInitialDocument;

  // True if this window has a "beforeunload" event listener.
  bool mHasBeforeUnload;

  // The log of all content blocking actions taken on the document related to
  // this WindowGlobalParent. This is only stored on top-level documents and
  // includes the activity log for all of the nested subdocuments as well.
  ContentBlockingLog mContentBlockingLog;

  uint32_t mMixedContentSecurityState = 0;

  Maybe<ClientInfo> mClientInfo;
  // Fields being mirrored from the corresponding document
  nsCOMPtr<nsICookieJarSettings> mCookieJarSettings;
  nsCOMPtr<nsITransportSecurityInfo> mSecurityInfo;

  uint32_t mSandboxFlags;

  struct OriginCounter {
    void UpdateSiteOriginsFrom(WindowGlobalParent* aParent, bool aIncrease);
    void Accumulate();

    nsDataHashtable<nsCStringHashKey, int32_t> mOriginMap;
    uint32_t mMaxOrigins = 0;
  };

  // Used to collect unique site origin telemetry.
  //
  // Is only Some() on top-level content windows.
  Maybe<OriginCounter> mOriginCounter;

  bool mDocumentHasLoaded;
  bool mDocumentHasUserInteracted;
  bool mBlockAllMixedContent;
  bool mUpgradeInsecureRequests;

  // HTTPS-Only Mode flags
  uint32_t mHttpsOnlyStatus;
};

}  // namespace dom
}  // namespace mozilla

inline nsISupports* ToSupports(
    mozilla::dom::WindowGlobalParent* aWindowGlobal) {
  return static_cast<mozilla::dom::WindowContext*>(aWindowGlobal);
}

#endif  // !defined(mozilla_dom_WindowGlobalParent_h)
