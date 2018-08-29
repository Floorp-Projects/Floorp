/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/ipc/InProcessChild.h"
#include "mozilla/dom/BrowsingContext.h"

namespace mozilla {
namespace dom {

WindowGlobalChild::WindowGlobalChild(nsGlobalWindowInner* aWindow, dom::BrowsingContext* aBrowsingContext)
  : mWindowGlobal(aWindow)
  , mBrowsingContext(aBrowsingContext)
  , mIPCClosed(false)
{
}

already_AddRefed<WindowGlobalChild>
WindowGlobalChild::Create(nsGlobalWindowInner* aWindow)
{
  nsCOMPtr<nsIPrincipal> principal = aWindow->GetPrincipal();
  MOZ_ASSERT(principal);

  RefPtr<nsDocShell> docshell = nsDocShell::Cast(aWindow->GetDocShell());
  MOZ_ASSERT(docshell);

  // Initalize our WindowGlobalChild object.
  RefPtr<dom::BrowsingContext> bc = docshell->GetBrowsingContext();
  RefPtr<WindowGlobalChild> wgc = new WindowGlobalChild(aWindow, bc);

  WindowGlobalInit init(principal, BrowsingContextId(wgc->BrowsingContext()->Id()));

  // Send the link constructor over PInProcessChild or PBrowser.
  if (XRE_IsParentProcess()) {
    InProcessChild* ipc = InProcessChild::Singleton();
    if (!ipc) {
      return nullptr;
    }

    // Note: ref is released in DeallocPWindowGlobalChild
    ipc->SendPWindowGlobalConstructor(do_AddRef(wgc).take(), init);
  } else {
    RefPtr<TabChild> tabChild = TabChild::GetFrom(static_cast<mozIDOMWindow*>(aWindow));
    MOZ_ASSERT(tabChild);

    // Note: ref is released in DeallocPWindowGlobalChild
    tabChild->SendPWindowGlobalConstructor(do_AddRef(wgc).take(), init);
  }

  return wgc.forget();
}

already_AddRefed<WindowGlobalParent>
WindowGlobalChild::GetOtherSide()
{
  if (mIPCClosed) {
    return nullptr;
  }
  IProtocol* otherSide = InProcessChild::ParentActorFor(this);
  return do_AddRef(static_cast<WindowGlobalParent*>(otherSide));
}

void
WindowGlobalChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mIPCClosed = true;
}

WindowGlobalChild::~WindowGlobalChild()
{
}

NS_IMPL_CYCLE_COLLECTION(WindowGlobalChild, mWindowGlobal, mBrowsingContext)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WindowGlobalChild, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WindowGlobalChild, Release)

} // namespace dom
} // namespace mozilla
