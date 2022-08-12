/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowserBridgeParent_h
#define mozilla_dom_BrowserBridgeParent_h

#include "mozilla/dom/PBrowserBridgeParent.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/dom/WindowGlobalTypes.h"

namespace mozilla {

namespace a11y {
class DocAccessibleParent;
}

namespace embedding {
class PrintData;
}

namespace dom {

class BrowserParent;

/**
 * BrowserBridgeParent implements the parent actor part of the PBrowserBridge
 * protocol. See PBrowserBridge for more information.
 */
class BrowserBridgeParent : public PBrowserBridgeParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(BrowserBridgeParent, final);

  BrowserBridgeParent();

  nsresult InitWithProcess(BrowserParent* aParentBrowser,
                           ContentParent* aContentParent,
                           const WindowGlobalInit& aWindowInit,
                           uint32_t aChromeFlags, TabId aTabId);

  BrowserParent* GetBrowserParent() { return mBrowserParent; }

  CanonicalBrowsingContext* GetBrowsingContext();

  // Get our manager actor.
  BrowserParent* Manager();

#if defined(ACCESSIBILITY)
  /**
   * Get the DocAccessibleParent which contains this iframe.
   */
  a11y::DocAccessibleParent* GetEmbedderAccessibleDoc();

  /**
   * Get the unique id of the OuterDocAccessible associated with this iframe.
   * This is the id of the RemoteAccessible inside the document returned by
   * GetEmbedderAccessibleDoc.
   */
  uint64_t GetEmbedderAccessibleId() { return mEmbedderAccessibleID; }

  /**
   * Get the DocAccessibleParent for the embedded document.
   */
  a11y::DocAccessibleParent* GetDocAccessibleParent();
#endif  // defined(ACCESSIBILITY)

  // Tear down this BrowserBridgeParent.
  void Destroy();

 protected:
  friend class PBrowserBridgeParent;

  mozilla::ipc::IPCResult RecvShow(const OwnerShowInfo&);
  mozilla::ipc::IPCResult RecvScrollbarPreferenceChanged(ScrollbarPreference);
  mozilla::ipc::IPCResult RecvLoadURL(nsDocShellLoadState* aLoadState);
  mozilla::ipc::IPCResult RecvResumeLoad(uint64_t aPendingSwitchID);
  mozilla::ipc::IPCResult RecvUpdateDimensions(const nsIntRect& aRect,
                                               const ScreenIntSize& aSize);
  mozilla::ipc::IPCResult RecvUpdateEffects(const EffectsInfo& aEffects);
  mozilla::ipc::IPCResult RecvUpdateRemotePrintSettings(
      const embedding::PrintData&);
  mozilla::ipc::IPCResult RecvRenderLayers(const bool& aEnabled,
                                           const LayersObserverEpoch& aEpoch);

  mozilla::ipc::IPCResult RecvNavigateByKey(const bool& aForward,
                                            const bool& aForDocumentNavigation);
  mozilla::ipc::IPCResult RecvBeginDestroy();

  mozilla::ipc::IPCResult RecvDispatchSynthesizedMouseEvent(
      const WidgetMouseEvent& aEvent);

  mozilla::ipc::IPCResult RecvWillChangeProcess();

  mozilla::ipc::IPCResult RecvActivate(uint64_t aActionId);

  mozilla::ipc::IPCResult RecvDeactivate(const bool& aWindowLowering,
                                         uint64_t aActionId);

  mozilla::ipc::IPCResult RecvSetIsUnderHiddenEmbedderElement(
      const bool& aIsUnderHiddenEmbedderElement);

  mozilla::ipc::IPCResult RecvUpdateRemoteStyle(
      const StyleImageRendering& aImageRendering);

#ifdef ACCESSIBILITY
  mozilla::ipc::IPCResult RecvSetEmbedderAccessible(PDocAccessibleParent* aDoc,
                                                    uint64_t aID);
#endif

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  ~BrowserBridgeParent();

  RefPtr<BrowserParent> mBrowserParent;
#ifdef ACCESSIBILITY
  RefPtr<a11y::DocAccessibleParent> mEmbedderAccessibleDoc;
  uint64_t mEmbedderAccessibleID = 0;
#endif  // ACCESSIBILITY
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_BrowserBridgeParent_h)
