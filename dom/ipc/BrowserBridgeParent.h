/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowserBridgeParent_h
#define mozilla_dom_BrowserBridgeParent_h

#include "mozilla/dom/PBrowserBridgeParent.h"
#include "mozilla/Tuple.h"
#include "mozilla/dom/ipc/IdType.h"

namespace mozilla {

namespace a11y {
class DocAccessibleParent;
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

  // Initialize this actor after performing startup.
  nsresult Init(const nsString& aPresentationURL, const nsString& aRemoteType,
                const WindowGlobalInit& aWindowInit,
                const uint32_t& aChromeFlags, TabId aTabId);

  BrowserParent* GetBrowserParent() { return mBrowserParent; }

  CanonicalBrowsingContext* GetBrowsingContext();

  // Get our manager actor.
  BrowserParent* Manager();

#if defined(ACCESSIBILITY)
  /**
   * Get the accessible for this iframe's embedder OuterDocAccessible.
   * This returns the actor for the containing document and the unique id of
   * the embedder accessible within that document.
   */
  Tuple<a11y::DocAccessibleParent*, uint64_t> GetEmbedderAccessible() {
    return Tuple<a11y::DocAccessibleParent*, uint64_t>(mEmbedderAccessibleDoc,
                                                       mEmbedderAccessibleID);
  }
#endif  // defined(ACCESSIBILITY)

  // Tear down this BrowserBridgeParent.
  void Destroy();

 protected:
  friend class PBrowserBridgeParent;

  mozilla::ipc::IPCResult RecvShow(const ScreenIntSize& aSize,
                                   const bool& aParentIsActive,
                                   const nsSizeMode& aSizeMode);
  mozilla::ipc::IPCResult RecvLoadURL(const nsCString& aUrl);
  mozilla::ipc::IPCResult RecvResumeLoad(uint64_t aPendingSwitchID);
  mozilla::ipc::IPCResult RecvUpdateDimensions(
      const DimensionInfo& aDimensions);
  mozilla::ipc::IPCResult RecvUpdateEffects(const EffectsInfo& aEffects);
  mozilla::ipc::IPCResult RecvRenderLayers(const bool& aEnabled,
                                           const bool& aForceRepaint,
                                           const LayersObserverEpoch& aEpoch);

  mozilla::ipc::IPCResult RecvNavigateByKey(const bool& aForward,
                                            const bool& aForDocumentNavigation);

  mozilla::ipc::IPCResult RecvDispatchSynthesizedMouseEvent(
      const WidgetMouseEvent& aEvent);

  mozilla::ipc::IPCResult RecvSkipBrowsingContextDetach();

  mozilla::ipc::IPCResult RecvActivate();

  mozilla::ipc::IPCResult RecvDeactivate(const bool& aWindowLowering);

  mozilla::ipc::IPCResult RecvSetIsUnderHiddenEmbedderElement(
      const bool& aIsUnderHiddenEmbedderElement);

  mozilla::ipc::IPCResult RecvSetEmbedderAccessible(PDocAccessibleParent* aDoc,
                                                    uint64_t aID);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  ~BrowserBridgeParent();

  RefPtr<BrowserParent> mBrowserParent;
#if defined(ACCESSIBILITY)
  RefPtr<a11y::DocAccessibleParent> mEmbedderAccessibleDoc;
  uint64_t mEmbedderAccessibleID;
#endif  // defined(ACCESSIBILITY)
  bool mIPCOpen;
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_BrowserBridgeParent_h)
