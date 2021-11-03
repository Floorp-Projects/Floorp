/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowserHost_h
#define mozilla_dom_BrowserHost_h

#include "nsIRemoteTab.h"
#include "mozilla/dom/RemoteBrowser.h"
#include "mozilla/dom/BrowserParent.h"

class nsPIDOMWindowOuter;

namespace mozilla {

namespace a11y {
class DocAccessibleParent;
}  // namespace a11y

namespace dom {

class Element;

/**
 * BrowserHost manages a remote browser from the chrome process.
 *
 * It is used via the RemoteBrowser interface in nsFrameLoader and supports
 * operations over the tree of BrowserParent/BrowserBridgeParent's.
 *
 * See `dom/docs/Fission-IPC-Diagram.svg` for an overview of the DOM IPC
 * actors.
 */
class BrowserHost : public RemoteBrowser,
                    public nsIRemoteTab,
                    public nsSupportsWeakReference {
 public:
  typedef mozilla::layers::LayersId LayersId;

  explicit BrowserHost(BrowserParent* aParent);

  static BrowserHost* GetFrom(nsIRemoteTab* aRemoteTab);

  // NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  // nsIRemoteTab
  NS_DECL_NSIREMOTETAB

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(BrowserHost, RemoteBrowser)

  // Get the IPDL actor for the root BrowserParent. This method should be
  // avoided and consumers migrated to use this class.
  BrowserParent* GetActor() { return mRoot; }
  ContentParent* GetContentParent() const {
    return mRoot ? mRoot->Manager() : nullptr;
  }

  BrowserHost* AsBrowserHost() override { return this; }
  BrowserBridgeHost* AsBrowserBridgeHost() override { return nullptr; }

  TabId GetTabId() const override;
  LayersId GetLayersId() const override;
  BrowsingContext* GetBrowsingContext() const override;
  nsILoadContext* GetLoadContext() const override;
  bool CanRecv() const override;

  Element* GetOwnerElement() const { return mRoot->GetOwnerElement(); }
  already_AddRefed<nsPIDOMWindowOuter> GetParentWindowOuter() const {
    return mRoot->GetParentWindowOuter();
  }
  a11y::DocAccessibleParent* GetTopLevelDocAccessible() const;

  // Visit each BrowserParent in the tree formed by PBrowser and
  // PBrowserBridge that is anchored by `mRoot`.
  template <typename Callback>
  void VisitAll(Callback aCallback) {
    if (!mRoot) {
      return;
    }
    mRoot->VisitAll(aCallback);
  }

  void LoadURL(nsDocShellLoadState* aLoadState) override;
  void ResumeLoad(uint64_t aPendingSwitchId) override;
  void DestroyStart() override;
  void DestroyComplete() override;

  bool Show(const OwnerShowInfo&) override;
  void UpdateDimensions(const nsIntRect& aRect,
                        const ScreenIntSize& aSize) override;

  void UpdateEffects(EffectsInfo aInfo) override;

 private:
  virtual ~BrowserHost() = default;

  // The TabID for the root BrowserParent, we cache this so that we can access
  // it after the remote browser has been destroyed
  TabId mId;
  // The root BrowserParent of this remote browser
  RefPtr<BrowserParent> mRoot;
  EffectsInfo mEffectsInfo;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_BrowserHost_h
