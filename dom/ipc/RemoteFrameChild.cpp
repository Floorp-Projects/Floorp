/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/RemoteFrameChild.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

RemoteFrameChild::RemoteFrameChild(nsFrameLoader* aFrameLoader)
    : mLayersId{0}, mIPCOpen(true), mFrameLoader(aFrameLoader) {}

RemoteFrameChild::~RemoteFrameChild() {}

already_AddRefed<RemoteFrameChild> RemoteFrameChild::Create(
    nsFrameLoader* aFrameLoader, const TabContext& aContext,
    const nsString& aRemoteType) {
  MOZ_ASSERT(XRE_IsContentProcess());

  // Determine our embedder's TabChild actor.
  RefPtr<Element> owner = aFrameLoader->GetOwnerContent();
  MOZ_DIAGNOSTIC_ASSERT(owner);

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(owner->GetOwnerGlobal());
  MOZ_DIAGNOSTIC_ASSERT(docShell);

  RefPtr<TabChild> tabChild = TabChild::GetFrom(docShell);
  MOZ_DIAGNOSTIC_ASSERT(tabChild);

  RefPtr<RemoteFrameChild> remoteFrame = new RemoteFrameChild(aFrameLoader);
  // Reference is freed in TabChild::DeallocPRemoteFrameChild.
  tabChild->SendPRemoteFrameConstructor(
      do_AddRef(remoteFrame).take(),
      PromiseFlatString(aContext.PresentationURL()), aRemoteType);
  remoteFrame->mIPCOpen = true;

  return remoteFrame.forget();
}

void RemoteFrameChild::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCOpen = false;
}

}  // namespace dom
}  // namespace mozilla
