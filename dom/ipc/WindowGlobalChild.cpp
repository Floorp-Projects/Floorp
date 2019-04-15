/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/ipc/InProcessChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/WindowGlobalActorsBinding.h"

#include "mozilla/dom/JSWindowActorBinding.h"
#include "mozilla/dom/JSWindowActorChild.h"
#include "mozilla/dom/JSWindowActorService.h"

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

  // If we have already closed our browsing context, return a pre-closed
  // WindowGlobalChild actor.
  if (bc->GetClosed()) {
    wgc->ActorDestroy(FailedConstructor);
    return wgc.forget();
  }

  WindowGlobalInit init(principal, bc, wgc->mInnerWindowId,
                        wgc->mOuterWindowId);
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

/* static */
already_AddRefed<WindowGlobalChild> WindowGlobalChild::GetByInnerWindowId(
    uint64_t aInnerWindowId) {
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

already_AddRefed<TabChild> WindowGlobalChild::GetTabChild() {
  if (IsInProcess() || mIPCClosed) {
    return nullptr;
  }
  return do_AddRef(static_cast<TabChild*>(Manager()));
}

void WindowGlobalChild::Destroy() {
  // Perform async IPC shutdown unless we're not in-process, and our TabChild is
  // in the process of being destroyed, which will destroy us as well.
  RefPtr<TabChild> tabChild = GetTabChild();
  if (!tabChild || !tabChild->IsDestroyed()) {
    SendDestroy();
  }

  mIPCClosed = true;
}

IPCResult WindowGlobalChild::RecvAsyncMessage(const nsString& aActorName,
                                              const nsString& aMessageName,
                                              const ClonedMessageData& aData) {
  StructuredCloneData data;
  data.BorrowFromClonedMessageDataForChild(aData);
  HandleAsyncMessage(aActorName, aMessageName, data);
  return IPC_OK();
}

void WindowGlobalChild::HandleAsyncMessage(const nsString& aActorName,
                                           const nsString& aMessageName,
                                           StructuredCloneData& aData) {
  if (NS_WARN_IF(mIPCClosed)) {
    return;
  }

  // Force creation of the actor if it hasn't been created yet.
  IgnoredErrorResult rv;
  RefPtr<JSWindowActorChild> actor = GetActor(aActorName, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return;
  }

  // Get the JSObject for the named actor.
  JS::RootedObject obj(RootingCx(), actor->GetWrapper());
  if (NS_WARN_IF(!obj)) {
    // If we don't have a preserved wrapper, there won't be any receiver
    // method to call.
    return;
  }

  RefPtr<JSWindowActorService> actorSvc = JSWindowActorService::GetSingleton();
  if (NS_WARN_IF(!actorSvc)) {
    return;
  }

  actorSvc->ReceiveMessage(actor, obj, aMessageName, aData);
}

already_AddRefed<JSWindowActorChild> WindowGlobalChild::GetActor(
    const nsAString& aName, ErrorResult& aRv) {
  // Check if this actor has already been created, and return it if it has.
  if (mWindowActors.Contains(aName)) {
    return do_AddRef(mWindowActors.GetWeak(aName));
  }

  // Otherwise, we want to create a new instance of this actor. Call into the
  // JSWindowActorService to trigger construction.
  RefPtr<JSWindowActorService> actorSvc = JSWindowActorService::GetSingleton();
  if (!actorSvc) {
    return nullptr;
  }

  JS::RootedObject obj(RootingCx());
  actorSvc->ConstructActor(aName, /* aChildSide */ false, mBrowsingContext,
                           &obj, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Unwrap our actor to a JSWindowActorChild object.
  RefPtr<JSWindowActorChild> actor;
  if (NS_FAILED(UNWRAP_OBJECT(JSWindowActorChild, &obj, actor))) {
    return nullptr;
  }

  MOZ_RELEASE_ASSERT(!actor->Manager(),
                     "mManager was already initialized once!");
  actor->Init(aName, this);
  mWindowActors.Put(aName, actor);
  return actor.forget();
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
                                      mBrowsingContext, mWindowActors)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WindowGlobalChild, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WindowGlobalChild, Release)

}  // namespace dom
}  // namespace mozilla
