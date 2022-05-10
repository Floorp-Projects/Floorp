/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowserBridgeHost_h
#define mozilla_dom_BrowserBridgeHost_h

#include "mozilla/dom/RemoteBrowser.h"
#include "mozilla/dom/BrowserBridgeChild.h"

namespace mozilla::dom {

/**
 * BrowserBridgeHost manages a remote browser from a content process.
 *
 * It is used via the RemoteBrowser interface in nsFrameLoader and proxies
 * work to the chrome process via PBrowserBridge.
 *
 * See `dom/docs/Fission-IPC-Diagram.svg` for an overview of the DOM IPC
 * actors.
 */
class BrowserBridgeHost : public RemoteBrowser {
 public:
  typedef mozilla::layers::LayersId LayersId;

  explicit BrowserBridgeHost(BrowserBridgeChild* aChild);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(BrowserBridgeHost)

  // Get the IPDL actor for the BrowserBridgeChild.
  BrowserBridgeChild* GetActor() { return mBridge; }

  BrowserHost* AsBrowserHost() override { return nullptr; }
  BrowserBridgeHost* AsBrowserBridgeHost() override { return this; }

  TabId GetTabId() const override;
  LayersId GetLayersId() const override;
  BrowsingContext* GetBrowsingContext() const override;
  nsILoadContext* GetLoadContext() const override;
  bool CanRecv() const override;

  void LoadURL(nsDocShellLoadState* aLoadState) override;
  void ResumeLoad(uint64_t aPendingSwitchId) override;
  void DestroyStart() override;
  void DestroyComplete() override;

  bool Show(const OwnerShowInfo&) override;
  void UpdateDimensions(const nsIntRect& aRect,
                        const ScreenIntSize& aSize) override;

  void UpdateEffects(EffectsInfo aInfo) override;

 private:
  virtual ~BrowserBridgeHost() = default;

  already_AddRefed<nsIWidget> GetWidget() const;

  // The IPDL actor for proxying browser operations
  RefPtr<BrowserBridgeChild> mBridge;
  EffectsInfo mEffectsInfo;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_BrowserBridgeHost_h
