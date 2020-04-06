/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WindowGlobalParent_h
#define mozilla_dom_WindowGlobalParent_h

#include "mozilla/ContentBlockingLog.h"
#include "mozilla/ContentBlockingNotifier.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/PWindowGlobalParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/WindowContext.h"
#include "nsRefPtrHashtable.h"
#include "nsWrapperCache.h"
#include "nsISupports.h"
#include "nsIContentParent.h"
#include "mozilla/dom/WindowGlobalActor.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"

class nsIPrincipal;
class nsIURI;
class nsFrameLoader;

namespace mozilla {

namespace gfx {
class CrossProcessPaint;
}  // namespace gfx

namespace dom {

class WindowGlobalChild;
class JSWindowActorParent;
class JSWindowActorMessageMeta;

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

  // Has this actor been shut down
  bool IsClosed() { return !CanSend(); }

  // Check if this actor is managed by PInProcess, as-in the document is loaded
  // in-process.
  bool IsInProcess() { return mInProcess; }

  // Get the other side of this actor if it is an in-process actor. Returns
  // |nullptr| if the actor has been torn down, or is not in-process.
  already_AddRefed<WindowGlobalChild> GetChildActor();

  // Get a JS actor object by name.
  already_AddRefed<JSWindowActorParent> GetActor(const nsAString& aName,
                                                 ErrorResult& aRv);

  // Get the ContentParent which hosts this actor. Returns |nullptr| if the
  // actor has been torn down, or is in-process.
  ContentParent* GetContentParent();

  // Get the BrowserParent which hosts this actor. Returns |nullptr| if the
  // actor hasn't been fully initialized, or is in-process.
  BrowserParent* GetBrowserParent() const { return mBrowserParent; }

  void ReceiveRawMessage(const JSWindowActorMessageMeta& aMeta,
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
    return CanonicalBrowsingContext::Cast(WindowContext::GetBrowsingContext());
  }

  // Get the root nsFrameLoader object for the tree of BrowsingContext nodes
  // which this WindowGlobal is a part of. This will be the nsFrameLoader
  // holding the BrowserParent for remote tabs, and the root content frameloader
  // for non-remote tabs.
  already_AddRefed<nsFrameLoader> GetRootFrameLoader();

  // The current URI which loaded in the document.
  nsIURI* GetDocumentURI() override { return mDocumentURI; }

  const nsString& GetDocumentTitle() const { return mDocumentTitle; }

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

  // Create a WindowGlobalParent from over IPC. This method should not be called
  // from outside of the IPC constructors.
  WindowGlobalParent(const WindowGlobalInit& aInit, bool aInProcess);

  // Initialize the mFrameLoader fields for a created WindowGlobalParent.
  void Init(const WindowGlobalInit& aInit, BrowserParent* aBrowserParent);

  nsIGlobalObject* GetParentObject();
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void NotifyContentBlockingEvent(
      uint32_t aEvent, nsIRequest* aRequest, bool aBlocked,
      const nsACString& aTrackingOrigin,
      const nsTArray<nsCString>& aTrackingFullHashes,
      const Maybe<ContentBlockingNotifier::StorageAccessGrantedReason>&
          aReason = Nothing());

  ContentBlockingLog* GetContentBlockingLog() { return &mContentBlockingLog; }

 protected:
  const nsAString& GetRemoteType() override;
  JSWindowActor::Type GetSide() override { return JSWindowActor::Type::Parent; }

  // IPC messages
  mozilla::ipc::IPCResult RecvLoadURI(
      const MaybeDiscarded<dom::BrowsingContext>& aTargetBC,
      nsDocShellLoadState* aLoadState, bool aSetNavigating);
  mozilla::ipc::IPCResult RecvInternalLoad(
      const MaybeDiscarded<dom::BrowsingContext>& aTargetBC,
      nsDocShellLoadState* aLoadState);
  mozilla::ipc::IPCResult RecvUpdateDocumentURI(nsIURI* aURI);
  mozilla::ipc::IPCResult RecvUpdateDocumentTitle(const nsString& aTitle);
  mozilla::ipc::IPCResult RecvSetIsInitialDocument(bool aIsInitialDocument) {
    mIsInitialDocument = aIsInitialDocument;
    return IPC_OK();
  }
  mozilla::ipc::IPCResult RecvSetHasBeforeUnload(bool aHasBeforeUnload);
  mozilla::ipc::IPCResult RecvSetClientInfo(
      const IPCClientInfo& aIPCClientInfo);
  mozilla::ipc::IPCResult RecvDestroy();
  mozilla::ipc::IPCResult RecvRawMessage(const JSWindowActorMessageMeta& aMeta,
                                         const ClonedMessageData& aData,
                                         const ClonedMessageData& aStack);

  mozilla::ipc::IPCResult RecvGetContentBlockingEvents(
      GetContentBlockingEventsResolver&& aResolver);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  void DrawSnapshotInternal(gfx::CrossProcessPaint* aPaint,
                            const Maybe<IntRect>& aRect, float aScale,
                            nscolor aBackgroundColor, uint32_t aFlags);

  // WebShare API - try to share
  mozilla::ipc::IPCResult RecvShare(IPCWebShareData&& aData,
                                    ShareResolver&& aResolver);

 private:
  ~WindowGlobalParent();

  RefPtr<BrowserParent> mBrowserParent;

  // NOTE: This document principal doesn't reflect possible |document.domain|
  // mutations which may have been made in the actual document.
  nsCOMPtr<nsIPrincipal> mDocumentPrincipal;
  nsCOMPtr<nsIPrincipal> mDocContentBlockingAllowListPrincipal;
  nsCOMPtr<nsIURI> mDocumentURI;
  nsString mDocumentTitle;

  nsRefPtrHashtable<nsStringHashKey, JSWindowActorParent> mWindowActors;
  bool mInProcess;
  bool mIsInitialDocument;

  // True if this window has a "beforeunload" event listener.
  bool mHasBeforeUnload;

  // The log of all content blocking actions taken on the document related to
  // this WindowGlobalParent. This is only stored on top-level documents and
  // includes the activity log for all of the nested subdocuments as well.
  ContentBlockingLog mContentBlockingLog;

  Maybe<ClientInfo> mClientInfo;
};

}  // namespace dom
}  // namespace mozilla

inline nsISupports* ToSupports(
    mozilla::dom::WindowGlobalParent* aWindowGlobal) {
  return static_cast<mozilla::dom::WindowContext*>(aWindowGlobal);
}

#endif  // !defined(mozilla_dom_WindowGlobalParent_h)
