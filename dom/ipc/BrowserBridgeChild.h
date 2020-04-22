/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowserBridgeChild_h
#define mozilla_dom_BrowserBridgeChild_h

#include "mozilla/dom/PBrowserBridgeChild.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/ipc/IdType.h"

namespace mozilla {

namespace a11y {
class RemoteIframeDocProxyAccessibleWrap;
}

namespace dom {
class BrowsingContext;
class ContentChild;
class BrowserBridgeHost;

/**
 * BrowserBridgeChild implements the child actor part of the PBrowserBridge
 * protocol. See PBrowserBridge for more information.
 */
class BrowserBridgeChild : public PBrowserBridgeChild {
 public:
  typedef mozilla::layers::LayersId LayersId;

  NS_INLINE_DECL_REFCOUNTING(BrowserBridgeChild, final);

  BrowserChild* Manager() {
    MOZ_ASSERT(CanSend());
    return static_cast<BrowserChild*>(PBrowserBridgeChild::Manager());
  }

  TabId GetTabId() { return mId; }

  LayersId GetLayersId() { return mLayersId; }

  nsFrameLoader* GetFrameLoader() const { return mFrameLoader; }

  BrowsingContext* GetBrowsingContext() { return mBrowsingContext; }

  // XXX(nika): We should have a load context here. (bug 1532664)
  nsILoadContext* GetLoadContext() { return nullptr; }

  void NavigateByKey(bool aForward, bool aForDocumentNavigation);

  void Activate();

  void Deactivate(bool aWindowLowering);

  void SetIsUnderHiddenEmbedderElement(bool aIsUnderHiddenEmbedderElement);

  already_AddRefed<BrowserBridgeHost> FinishInit(nsFrameLoader* aFrameLoader);

#if defined(ACCESSIBILITY) && defined(XP_WIN)
  a11y::RemoteIframeDocProxyAccessibleWrap* GetEmbeddedDocAccessible() {
    return mEmbeddedDocAccessible;
  }
#endif

  static BrowserBridgeChild* GetFrom(nsFrameLoader* aFrameLoader);

  static BrowserBridgeChild* GetFrom(nsIContent* aContent);

  BrowserBridgeChild(BrowsingContext* aBrowsingContext, TabId aId);

 protected:
  friend class ContentChild;
  friend class PBrowserBridgeChild;

  mozilla::ipc::IPCResult RecvSetLayersId(
      const mozilla::layers::LayersId& aLayersId);

  mozilla::ipc::IPCResult RecvRequestFocus(const bool& aCanRaise,
                                           const CallerType aCallerType);

  mozilla::ipc::IPCResult RecvMoveFocus(const bool& aForward,
                                        const bool& aForDocumentNavigation);

  mozilla::ipc::IPCResult RecvSetEmbeddedDocAccessibleCOMProxy(
      const IDispatchHolder& aCOMProxy);

  mozilla::ipc::IPCResult RecvMaybeFireEmbedderLoadEvents(
      EmbedderElementEventType aFireEventAtEmbeddingElement);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  mozilla::ipc::IPCResult RecvScrollRectIntoView(
      const nsRect& aRect, const ScrollAxis& aVertical,
      const ScrollAxis& aHorizontal, const ScrollFlags& aScrollFlags,
      const int32_t& aAppUnitsPerDevPixel);

  mozilla::ipc::IPCResult RecvSubFrameCrashed(
      const MaybeDiscarded<BrowsingContext>& aContext);

  mozilla::ipc::IPCResult RecvAddBlockedNodeByClassifier();

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  ~BrowserBridgeChild();

  void UnblockOwnerDocsLoadEvent();

  TabId mId;
  LayersId mLayersId;
  bool mHadInitialLoad = false;
  RefPtr<nsFrameLoader> mFrameLoader;
  RefPtr<BrowsingContext> mBrowsingContext;
#if defined(ACCESSIBILITY) && defined(XP_WIN)
  RefPtr<a11y::RemoteIframeDocProxyAccessibleWrap> mEmbeddedDocAccessible;
#endif
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_BrowserBridgeParent_h)
