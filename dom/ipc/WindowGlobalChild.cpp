/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/ipc/InProcessChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/WindowGlobalActorsBinding.h"

namespace mozilla {
namespace dom {

typedef nsRefPtrHashtable<nsUint64HashKey, WindowGlobalChild> WGCByIdMap;
static StaticAutoPtr<WGCByIdMap> gWindowGlobalChildById;

WindowGlobalChild::WindowGlobalChild(nsGlobalWindowInner* aWindow,
                                     dom::BrowsingContext* aBrowsingContext)
    : mWindowGlobal(aWindow),
      mBrowsingContext(aBrowsingContext),
      mInnerWindowId(aWindow->WindowID()),
      mOuterWindowId(aWindow->GetOuterWindow()->WindowID()),
      mIPCClosed(true) {}

already_AddRefed<WindowGlobalChild> WindowGlobalChild::Create(
    nsGlobalWindowInner* aWindow) {
  nsCOMPtr<nsIPrincipal> principal = aWindow->GetPrincipal();
  MOZ_ASSERT(principal);

  RefPtr<nsDocShell> docshell = nsDocShell::Cast(aWindow->GetDocShell());
  MOZ_ASSERT(docshell);

  // Initalize our WindowGlobalChild object.
  RefPtr<dom::BrowsingContext> bc = docshell->GetBrowsingContext();
  RefPtr<WindowGlobalChild> wgc = new WindowGlobalChild(aWindow, bc);

  WindowGlobalInit init(principal,
                        BrowsingContextId(wgc->BrowsingContext()->Id()),
                        wgc->mInnerWindowId, wgc->mOuterWindowId);

  // Send the link constructor over PInProcessChild or PBrowser.
  if (XRE_IsParentProcess()) {
    InProcessChild* ipc = InProcessChild::Singleton();
    if (!ipc) {
      return nullptr;
    }

    // Note: ref is released in DeallocPWindowGlobalChild
    ipc->SendPWindowGlobalConstructor(do_AddRef(wgc).take(), init);
  } else {
    RefPtr<TabChild> tabChild =
        TabChild::GetFrom(static_cast<mozIDOMWindow*>(aWindow));
    MOZ_ASSERT(tabChild);

    // Note: ref is released in DeallocPWindowGlobalChild
    tabChild->SendPWindowGlobalConstructor(do_AddRef(wgc).take(), init);
  }
  wgc->mIPCClosed = false;

  // Register this WindowGlobal in the gWindowGlobalParentsById map.
  if (!gWindowGlobalChildById) {
    gWindowGlobalChildById = new WGCByIdMap();
    ClearOnShutdown(&gWindowGlobalChildById);
  }
  auto entry = gWindowGlobalChildById->LookupForAdd(wgc->mInnerWindowId);
  MOZ_RELEASE_ASSERT(!entry, "Duplicate WindowGlobalChild entry for ID!");
  entry.OrInsert([&] { return wgc; });

  return wgc.forget();
}

/* static */ already_AddRefed<WindowGlobalChild>
WindowGlobalChild::GetByInnerWindowId(uint64_t aInnerWindowId) {
  if (!gWindowGlobalChildById) {
    return nullptr;
  }
  return gWindowGlobalChildById->Get(aInnerWindowId);
}

bool WindowGlobalChild::IsCurrentGlobal() {
  return !mIPCClosed && mWindowGlobal->IsCurrentInnerWindow();
}

already_AddRefed<WindowGlobalParent> WindowGlobalChild::GetParentActor() {
  if (mIPCClosed) {
    return nullptr;
  }
  IProtocol* otherSide = InProcessChild::ParentActorFor(this);
  return do_AddRef(static_cast<WindowGlobalParent*>(otherSide));
}

void WindowGlobalChild::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCClosed = true;
  gWindowGlobalChildById->Remove(mInnerWindowId);
}

WindowGlobalChild::~WindowGlobalChild() {
  MOZ_ASSERT(!gWindowGlobalChildById ||
             !gWindowGlobalChildById->Contains(mInnerWindowId));
}

JSObject* WindowGlobalChild::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return WindowGlobalChild_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports* WindowGlobalChild::GetParentObject() {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WindowGlobalChild, mWindowGlobal,
                                      mBrowsingContext)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WindowGlobalChild, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WindowGlobalChild, Release)

}  // namespace dom
}  // namespace mozilla
