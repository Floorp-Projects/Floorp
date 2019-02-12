/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteFrameChild_h
#define mozilla_dom_RemoteFrameChild_h

#include "mozilla/dom/PRemoteFrameChild.h"
#include "mozilla/dom/TabChild.h"

namespace mozilla {
namespace dom {

/**
 * Child side for a remote frame.
 */
class RemoteFrameChild : public PRemoteFrameChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(RemoteFrameChild);

  TabChild* Manager() {
    MOZ_ASSERT(mIPCOpen);
    return static_cast<TabChild*>(PRemoteFrameChild::Manager());
  }

  mozilla::layers::LayersId GetLayersId() { return mLayersId; }

  static already_AddRefed<RemoteFrameChild> Create(nsFrameLoader* aFrameLoader,
                                                   const TabContext& aContext,
                                                   const nsString& aRemoteType);

  void UpdateDimensions(const nsIntRect& aRect,
                        const mozilla::ScreenIntSize& aSize);

 protected:
  friend class PRemoteFrameChild;

  mozilla::ipc::IPCResult RecvSetLayersId(
      const mozilla::layers::LayersId& aLayersId);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  explicit RemoteFrameChild(nsFrameLoader* aFrameLoader);
  ~RemoteFrameChild();

  mozilla::layers::LayersId mLayersId;
  bool mIPCOpen;
  RefPtr<nsFrameLoader> mFrameLoader;
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_RemoteFrameParent_h)
