/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_RemoteBrowser_h
#define mozilla_dom_ipc_RemoteBrowser_h

#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsISupports.h"
#include "nsRect.h"
#include "Units.h"

class nsDocShellLoadState;
class nsFrameLoader;
class nsILoadContext;
class nsIContent;

namespace mozilla::dom {

class BrowserHost;
class BrowserBridgeHost;
class BrowsingContext;
class EffectsInfo;
class OwnerShowInfo;

/**
 * An interface to control a browser hosted in another process.
 *
 * This is used by nsFrameLoader to abstract between hosting a top-level remote
 * browser in the chrome process and hosting an OOP-iframe in a content process.
 *
 * There are two concrete implementations that are used depending on whether
 * the nsFrameLoader is in the chrome or content process. A chrome process
 * nsFrameLoader will use BrowserHost, and a content process nsFrameLoader will
 * use BrowserBridgeHost.
 */
class RemoteBrowser : public nsISupports {
 public:
  using LayersId = mozilla::layers::LayersId;

  static RemoteBrowser* GetFrom(nsFrameLoader* aFrameLoader);
  static RemoteBrowser* GetFrom(nsIContent* aContent);

  // Try to cast this RemoteBrowser to a BrowserHost, may return null
  virtual BrowserHost* AsBrowserHost() = 0;
  // Try to cast this RemoteBrowser to a BrowserBridgeHost, may return null
  virtual BrowserBridgeHost* AsBrowserBridgeHost() = 0;

  virtual TabId GetTabId() const = 0;
  virtual LayersId GetLayersId() const = 0;
  virtual BrowsingContext* GetBrowsingContext() const = 0;
  virtual nsILoadContext* GetLoadContext() const = 0;
  virtual bool CanRecv() const = 0;

  virtual void LoadURL(nsDocShellLoadState* aLoadState) = 0;
  virtual void ResumeLoad(uint64_t aPendingSwitchId) = 0;
  virtual void DestroyStart() = 0;
  virtual void DestroyComplete() = 0;

  virtual bool Show(const OwnerShowInfo&) = 0;
  virtual void UpdateDimensions(const nsIntRect& aRect,
                                const ScreenIntSize& aSize) = 0;

  virtual void UpdateEffects(EffectsInfo aInfo) = 0;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_ipc_RemoteBrowser_h
