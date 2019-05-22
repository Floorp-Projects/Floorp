/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WindowGlobalParent_h
#define mozilla_dom_WindowGlobalParent_h

#include "mozilla/RefPtr.h"
#include "mozilla/dom/PWindowGlobalParent.h"
#include "mozilla/dom/BrowserParent.h"
#include "nsRefPtrHashtable.h"
#include "nsWrapperCache.h"
#include "nsISupports.h"
#include "mozilla/dom/WindowGlobalActor.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"

class nsIPrincipal;
class nsIURI;
class nsFrameLoader;

namespace mozilla {
namespace dom {

class WindowGlobalChild;
class JSWindowActorParent;
class JSWindowActorMessageMeta;

/**
 * A handle in the parent process to a specific nsGlobalWindowInner object.
 */
class WindowGlobalParent final : public WindowGlobalActor,
                                 public PWindowGlobalParent {
  friend class PWindowGlobalParent;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(WindowGlobalParent,
                                                         WindowGlobalActor)

  static already_AddRefed<WindowGlobalParent> GetByInnerWindowId(
      uint64_t aInnerWindowId);

  static already_AddRefed<WindowGlobalParent> GetByInnerWindowId(
      const GlobalObject& aGlobal, uint64_t aInnerWindowId) {
    return GetByInnerWindowId(aInnerWindowId);
  }

  // Has this actor been shut down
  bool IsClosed() { return mIPCClosed; }

  // Check if this actor is managed by PInProcess, as-in the document is loaded
  // in-process.
  bool IsInProcess() { return mInProcess; }

  // Get the other side of this actor if it is an in-process actor. Returns
  // |nullptr| if the actor has been torn down, or is not in-process.
  already_AddRefed<WindowGlobalChild> GetChildActor();

  // Get a JS actor object by name.
  already_AddRefed<JSWindowActorParent> GetActor(const nsAString& aName,
                                                 ErrorResult& aRv);

  // Get this actor's manager if it is not an in-process actor. Returns
  // |nullptr| if the actor has been torn down, or is in-process.
  already_AddRefed<BrowserParent> GetRemoteTab();

  void ReceiveRawMessage(const JSWindowActorMessageMeta& aMeta,
                         ipc::StructuredCloneData&& aData);

  // The principal of this WindowGlobal. This value will not change over the
  // lifetime of the WindowGlobal object, even to reflect changes in
  // |document.domain|.
  nsIPrincipal* DocumentPrincipal() { return mDocumentPrincipal; }

  // The BrowsingContext which this WindowGlobal has been loaded into.
  CanonicalBrowsingContext* BrowsingContext() override {
    return mBrowsingContext;
  }

  // Get the root nsFrameLoader object for the tree of BrowsingContext nodes
  // which this WindowGlobal is a part of. This will be the nsFrameLoader
  // holding the BrowserParent for remote tabs, and the root content frameloader
  // for non-remote tabs.
  nsFrameLoader* GetRootFrameLoader() { return mFrameLoader; }

  // The current URI which loaded in the document.
  nsIURI* GetDocumentURI() override { return mDocumentURI; }

  // Window IDs for inner/outer windows.
  uint64_t OuterWindowId() { return mOuterWindowId; }
  uint64_t InnerWindowId() { return mInnerWindowId; }

  bool IsCurrentGlobal();

  already_AddRefed<Promise> ChangeFrameRemoteness(dom::BrowsingContext* aBc,
                                                  const nsAString& aRemoteType,
                                                  uint64_t aPendingSwitchId,
                                                  ErrorResult& aRv);

  // Create a WindowGlobalParent from over IPC. This method should not be called
  // from outside of the IPC constructors.
  WindowGlobalParent(const WindowGlobalInit& aInit, bool aInProcess);

  // Initialize the mFrameLoader fields for a created WindowGlobalParent. Must
  // be called after setting the Manager actor.
  void Init(const WindowGlobalInit& aInit);

  nsISupports* GetParentObject();
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

 protected:
  const nsAString& GetRemoteType() override;
  JSWindowActor::Type GetSide() override { return JSWindowActor::Type::Parent; }

  // IPC messages
  mozilla::ipc::IPCResult RecvUpdateDocumentURI(nsIURI* aURI);
  mozilla::ipc::IPCResult RecvBecomeCurrentWindowGlobal();
  mozilla::ipc::IPCResult RecvDestroy();
  mozilla::ipc::IPCResult RecvRawMessage(const JSWindowActorMessageMeta& aMeta,
                                         const ClonedMessageData& aData);
  mozilla::ipc::IPCResult RecvDidEmbedBrowsingContext(
      dom::BrowsingContext* aContext);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  ~WindowGlobalParent();

  // NOTE: This document principal doesn't reflect possible |document.domain|
  // mutations which may have been made in the actual document.
  nsCOMPtr<nsIPrincipal> mDocumentPrincipal;
  nsCOMPtr<nsIURI> mDocumentURI;
  RefPtr<nsFrameLoader> mFrameLoader;
  RefPtr<CanonicalBrowsingContext> mBrowsingContext;
  nsRefPtrHashtable<nsStringHashKey, JSWindowActorParent> mWindowActors;
  uint64_t mInnerWindowId;
  uint64_t mOuterWindowId;
  bool mInProcess;
  bool mIPCClosed;
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_WindowGlobalParent_h)
