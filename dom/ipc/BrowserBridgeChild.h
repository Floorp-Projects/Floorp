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

namespace mozilla::dom {
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

  nsILoadContext* GetLoadContext();

  void NavigateByKey(bool aForward, bool aForDocumentNavigation);

  void Activate(uint64_t aActionId);

  void Deactivate(bool aWindowLowering, uint64_t aActionId);

  void SetIsUnderHiddenEmbedderElement(bool aIsUnderHiddenEmbedderElement);

  already_AddRefed<BrowserBridgeHost> FinishInit(nsFrameLoader* aFrameLoader);

#if defined(ACCESSIBILITY)
  void SetEmbedderAccessible(PDocAccessibleChild* aDoc, uint64_t aID) {
    MOZ_ASSERT((aDoc && aID) || (!aDoc && !aID));
    mEmbedderAccessibleID = aID;
    Unused << SendSetEmbedderAccessible(aDoc, aID);
  }

  uint64_t GetEmbedderAccessibleID() { return mEmbedderAccessibleID; }

#  if defined(XP_WIN)
  already_AddRefed<IDispatch> GetEmbeddedDocAccessible() {
    return RefPtr{mEmbeddedDocAccessible}.forget();
  }
#  endif  // defined(XP_WIN)
#endif    // defined(ACCESSIBILITY)

  static BrowserBridgeChild* GetFrom(nsFrameLoader* aFrameLoader);

  static BrowserBridgeChild* GetFrom(nsIContent* aContent);

  BrowserBridgeChild(BrowsingContext* aBrowsingContext, TabId aId,
                     const LayersId& aLayersId);

 protected:
  friend class ContentChild;
  friend class PBrowserBridgeChild;

  mozilla::ipc::IPCResult RecvRequestFocus(const bool& aCanRaise,
                                           const CallerType aCallerType);

  mozilla::ipc::IPCResult RecvMoveFocus(const bool& aForward,
                                        const bool& aForDocumentNavigation);

  mozilla::ipc::IPCResult RecvSetEmbeddedDocAccessibleCOMProxy(
      const IDispatchHolder& aCOMProxy);

  // TODO: Use MOZ_CAN_RUN_SCRIPT when it gains IPDL support (bug 1539864)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY mozilla::ipc::IPCResult
  RecvMaybeFireEmbedderLoadEvents(
      EmbedderElementEventType aFireEventAtEmbeddingElement);

  mozilla::ipc::IPCResult RecvIntrinsicSizeOrRatioChanged(
      const Maybe<IntrinsicSize>& aIntrinsicSize,
      const Maybe<AspectRatio>& aIntrinsicRatio);

  mozilla::ipc::IPCResult RecvImageLoadComplete(const nsresult& aResult);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  mozilla::ipc::IPCResult RecvScrollRectIntoView(
      const nsRect& aRect, const ScrollAxis& aVertical,
      const ScrollAxis& aHorizontal, const ScrollFlags& aScrollFlags,
      const int32_t& aAppUnitsPerDevPixel);

  mozilla::ipc::IPCResult RecvSubFrameCrashed();

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  ~BrowserBridgeChild();

  void UnblockOwnerDocsLoadEvent();

  TabId mId;
  LayersId mLayersId;
  bool mHadInitialLoad = false;
  RefPtr<nsFrameLoader> mFrameLoader;
  RefPtr<BrowsingContext> mBrowsingContext;
#if defined(ACCESSIBILITY)
  // We need to keep track of the embedder accessible id we last sent to the
  // parent process.
  uint64_t mEmbedderAccessibleID = 0;
#  if defined(XP_WIN)
  RefPtr<IDispatch> mEmbeddedDocAccessible;
#  endif  // defined(XP_WIN)
#endif    // defined(ACCESSIBILITY)
};

}  // namespace mozilla::dom

#endif  // !defined(mozilla_dom_BrowserBridgeParent_h)
