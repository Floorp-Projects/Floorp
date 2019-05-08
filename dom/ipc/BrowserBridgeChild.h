/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowserBridgeChild_h
#define mozilla_dom_BrowserBridgeChild_h

#include "mozilla/dom/PBrowserBridgeChild.h"
#include "mozilla/dom/BrowserChild.h"

namespace mozilla {
namespace dom {
class BrowsingContext;
class ContentChild;

/**
 * BrowserBridgeChild implements the child actor part of the PBrowserBridge
 * protocol. See PBrowserBridge for more information.
 */
class BrowserBridgeChild : public PBrowserBridgeChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(BrowserBridgeChild);

  BrowserChild* Manager() {
    MOZ_ASSERT(mIPCOpen);
    return static_cast<BrowserChild*>(PBrowserBridgeChild::Manager());
  }

  mozilla::layers::LayersId GetLayersId() { return mLayersId; }
  nsFrameLoader* GetFrameLoader() const { return mFrameLoader; }

  BrowsingContext* GetBrowsingContext() { return mBrowsingContext; }

  // XXX(nika): We should have a load context here. (bug 1532664)
  nsILoadContext* GetLoadContext() { return nullptr; }

  void NavigateByKey(bool aForward, bool aForDocumentNavigation);

  void Activate();

  void Deactivate();

  void SetIsUnderHiddenEmbedderElement(bool aIsUnderHiddenEmbedderElement);

  static BrowserBridgeChild* GetFrom(nsFrameLoader* aFrameLoader);

  static BrowserBridgeChild* GetFrom(nsIContent* aContent);

 protected:
  friend class ContentChild;
  friend class PBrowserBridgeChild;

  BrowserBridgeChild(nsFrameLoader* aFrameLoader,
                     BrowsingContext* aBrowsingContext);

  mozilla::ipc::IPCResult RecvSetLayersId(
      const mozilla::layers::LayersId& aLayersId);

  mozilla::ipc::IPCResult RecvRequestFocus(const bool& aCanRaise);

  mozilla::ipc::IPCResult RecvMoveFocus(const bool& aForward,
                                        const bool& aForDocumentNavigation);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  ~BrowserBridgeChild();

  mozilla::layers::LayersId mLayersId;
  bool mIPCOpen;
  RefPtr<nsFrameLoader> mFrameLoader;
  RefPtr<BrowsingContext> mBrowsingContext;
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_BrowserBridgeParent_h)
