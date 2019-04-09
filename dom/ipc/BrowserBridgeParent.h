/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowserBridgeParent_h
#define mozilla_dom_BrowserBridgeParent_h

#include "mozilla/dom/PBrowserBridgeParent.h"
#include "mozilla/dom/TabParent.h"

namespace mozilla {
namespace dom {

class BrowserBridgeParent : public PBrowserBridgeParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(BrowserBridgeParent);

  BrowserBridgeParent();

  // Initialize this actor after performing startup.
  nsresult Init(const nsString& aPresentationURL, const nsString& aRemoteType,
                CanonicalBrowsingContext* aBrowsingContext);

  TabParent* GetTabParent() { return mTabParent; }

  CanonicalBrowsingContext* GetBrowsingContext() {
    return mTabParent->GetBrowsingContext();
  }

  // Get our manager actor.
  TabParent* Manager() {
    MOZ_ASSERT(mIPCOpen);
    return static_cast<TabParent*>(PBrowserBridgeParent::Manager());
  }

 protected:
  friend class PBrowserBridgeParent;

  mozilla::ipc::IPCResult RecvShow(const ScreenIntSize& aSize,
                                   const bool& aParentIsActive,
                                   const nsSizeMode& aSizeMode);
  mozilla::ipc::IPCResult RecvLoadURL(const nsCString& aUrl);
  mozilla::ipc::IPCResult RecvUpdateDimensions(
      const DimensionInfo& aDimensions);
  mozilla::ipc::IPCResult RecvRenderLayers(const bool& aEnabled,
                                           const bool& aForceRepaint,
                                           const LayersObserverEpoch& aEpoch);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  ~BrowserBridgeParent();

  RefPtr<TabParent> mTabParent;
  bool mIPCOpen;
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_BrowserBridgeParent_h)
