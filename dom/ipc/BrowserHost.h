/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowserHost_h
#define mozilla_dom_BrowserHost_h

#include "mozilla/dom/RemoteBrowser.h"
#include "mozilla/dom/BrowserParent.h"

namespace mozilla {
namespace dom {

/**
 * BrowserHost manages a remote browser from the chrome process.
 *
 * It is used via the RemoteBrowser interface in nsFrameLoader and supports
 * operations over the tree of BrowserParent/BrowserBridgeParent's.
 *
 * See `dom/docs/Fission-IPC-Diagram.svg` for an overview of the DOM IPC
 * actors.
 */
class BrowserHost : public RemoteBrowser {
 public:
  typedef mozilla::layers::LayersId LayersId;

  explicit BrowserHost(BrowserParent* aParent);

  NS_INLINE_DECL_REFCOUNTING(BrowserHost, override);

  // Get the IPDL actor for the root BrowserParent. This method should be
  // avoided and consumers migrated to use this class.
  BrowserParent* GetActor() { return mRoot; }

  BrowserHost* AsBrowserHost() override { return this; }
  BrowserBridgeHost* AsBrowserBridgeHost() override { return nullptr; }

  LayersId GetLayersId() const override;
  BrowsingContext* GetBrowsingContext() const override;
  nsILoadContext* GetLoadContext() const override;

  void LoadURL(nsIURI* aURI) override;
  void ResumeLoad(uint64_t aPendingSwitchId) override;
  void DestroyStart() override;
  void DestroyComplete() override;

  bool Show(const ScreenIntSize& aSize, bool aParentIsActive) override;
  void UpdateDimensions(const nsIntRect& aRect,
                        const ScreenIntSize& aSize) override;

 private:
  virtual ~BrowserHost() = default;

  // The root BrowserParent of this remote browser
  RefPtr<BrowserParent> mRoot;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_BrowserHost_h
