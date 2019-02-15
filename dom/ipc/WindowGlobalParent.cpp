/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/ipc/InProcessParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/WindowGlobalActorsBinding.h"
#include "mozilla/dom/ChromeUtils.h"
#include "mozJSComponentLoader.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsQueryObject.h"

#include "mozilla/dom/JSWindowActorBinding.h"
#include "mozilla/dom/JSWindowActorParent.h"
#include "mozilla/dom/JSWindowActorService.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

typedef nsRefPtrHashtable<nsUint64HashKey, WindowGlobalParent> WGPByIdMap;
static StaticAutoPtr<WGPByIdMap> gWindowGlobalParentsById;

WindowGlobalParent::WindowGlobalParent(const WindowGlobalInit& aInit,
                                       bool aInProcess)
    : mDocumentPrincipal(aInit.principal()),
      mInnerWindowId(aInit.innerWindowId()),
      mOuterWindowId(aInit.outerWindowId()),
      mInProcess(aInProcess),
      mIPCClosed(true)  // Closed until WGP::Init
{
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(), "Parent process only");
  MOZ_RELEASE_ASSERT(mDocumentPrincipal, "Must have a valid principal");

  // NOTE: mBrowsingContext initialized in Init()
  MOZ_RELEASE_ASSERT(aInit.browsingContext(),
                     "Must be made in BrowsingContext");
}

void WindowGlobalParent::Init(const WindowGlobalInit& aInit) {
  MOZ_ASSERT(Manager(), "Should have a manager!");
  MOZ_ASSERT(!mFrameLoader, "Cannot Init() a WindowGlobalParent twice!");

  MOZ_ASSERT(mIPCClosed, "IPC shouldn't be open yet");
  mIPCClosed = false;

  // Register this WindowGlobal in the gWindowGlobalParentsById map.
  if (!gWindowGlobalParentsById) {
    gWindowGlobalParentsById = new WGPByIdMap();
    ClearOnShutdown(&gWindowGlobalParentsById);
  }
  auto entry = gWindowGlobalParentsById->LookupForAdd(mInnerWindowId);
  MOZ_RELEASE_ASSERT(!entry, "Duplicate WindowGlobalParent entry for ID!");
  entry.OrInsert([&] { return this; });

  // Determine which content process the window global is coming from.
  ContentParentId processId(0);
  if (!mInProcess) {
    processId = static_cast<ContentParent*>(Manager()->Manager())->ChildID();
  }

  mBrowsingContext = CanonicalBrowsingContext::Cast(aInit.browsingContext());
  MOZ_ASSERT(mBrowsingContext);

  // XXX(nika): This won't be the case soon, but for now this is a good
  // assertion as we can't switch processes. We should relax this eventually.
  MOZ_ASSERT(mBrowsingContext->IsOwnedByProcess(processId));

  // Attach ourself to the browsing context.
  mBrowsingContext->RegisterWindowGlobal(this);

  // Determine what toplevel frame element our WindowGlobalParent is being
  // embedded in.
  RefPtr<Element> frameElement;
  if (mInProcess) {
    // In the in-process case, we can get it from the other side's
    // WindowGlobalChild.
    MOZ_ASSERT(Manager()->GetProtocolTypeId() == PInProcessMsgStart);
    RefPtr<WindowGlobalChild> otherSide = GetChildActor();
    if (otherSide && otherSide->WindowGlobal()) {
      // Get the toplevel window from the other side.
      RefPtr<nsDocShell> docShell =
          nsDocShell::Cast(otherSide->WindowGlobal()->GetDocShell());
      if (docShell) {
        docShell->GetTopFrameElement(getter_AddRefs(frameElement));
      }
    }
  } else {
    // In the cross-process case, we can get the frame element from our manager.
    MOZ_ASSERT(Manager()->GetProtocolTypeId() == PBrowserMsgStart);
    frameElement = static_cast<TabParent*>(Manager())->GetOwnerElement();
  }

  // Extract the nsFrameLoader from the current frame element. We may not have a
  // nsFrameLoader if we are a chrome document.
  RefPtr<nsFrameLoaderOwner> flOwner = do_QueryObject(frameElement);
  if (flOwner) {
    mFrameLoader = flOwner->GetFrameLoader();
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(this, "window-global-created", nullptr);
  }
}

/* static */ already_AddRefed<WindowGlobalParent>
WindowGlobalParent::GetByInnerWindowId(uint64_t aInnerWindowId) {
  if (!gWindowGlobalParentsById) {
    return nullptr;
  }
  return gWindowGlobalParentsById->Get(aInnerWindowId);
}

already_AddRefed<WindowGlobalChild> WindowGlobalParent::GetChildActor() {
  if (mIPCClosed) {
    return nullptr;
  }
  IProtocol* otherSide = InProcessParent::ChildActorFor(this);
  return do_AddRef(static_cast<WindowGlobalChild*>(otherSide));
}

already_AddRefed<TabParent> WindowGlobalParent::GetTabParent() {
  if (IsInProcess() || mIPCClosed) {
    return nullptr;
  }
  return do_AddRef(static_cast<TabParent*>(Manager()));
}

IPCResult WindowGlobalParent::RecvUpdateDocumentURI(nsIURI* aURI) {
  // XXX(nika): Assert that the URI change was one which makes sense (either
  // about:blank -> a real URI, or a legal push/popstate URI change?)
  mDocumentURI = aURI;
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvBecomeCurrentWindowGlobal() {
  mBrowsingContext->SetCurrentWindowGlobal(this);
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvDestroy() {
  if (!mIPCClosed) {
    RefPtr<TabParent> tabParent = GetTabParent();
    if (!tabParent || !tabParent->IsDestroyed()) {
      Unused << Send__delete__(this);
    }
  }
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvAsyncMessage(const nsString& aActorName,
                                               const nsString& aMessageName,
                                               const ClonedMessageData& aData) {
  StructuredCloneData data;
  data.BorrowFromClonedMessageDataForParent(aData);
  HandleAsyncMessage(aActorName, aMessageName, data);
  return IPC_OK();
}

void WindowGlobalParent::HandleAsyncMessage(const nsString& aActorName,
                                            const nsString& aMessageName,
                                            StructuredCloneData& aData) {
  if (NS_WARN_IF(mIPCClosed)) {
    return;
  }

  // Force creation of the actor if it hasn't been created yet.
  IgnoredErrorResult rv;
  RefPtr<JSWindowActorParent> actor = GetActor(aActorName, rv);
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

  actorSvc->ReceiveMessage(obj, aMessageName, aData);
}

already_AddRefed<JSWindowActorParent> WindowGlobalParent::GetActor(
    const nsAString& aName, ErrorResult& aRv) {
  // Check if this actor has already been created, and return it if it has.
  if (mWindowActors.Contains(aName)) {
    return do_AddRef(mWindowActors.GetWeak(aName));
  }

  // Otherwise, we want to create a new instance of this actor. Call into the
  // JSWindowActorService to trigger construction
  RefPtr<JSWindowActorService> actorSvc = JSWindowActorService::GetSingleton();
  if (!actorSvc) {
    return nullptr;
  }

  JS::RootedObject obj(RootingCx());
  actorSvc->ConstructActor(aName, /* aParentSide */ true, &obj, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Unwrap our actor to a JSWindowActorParent object.
  RefPtr<JSWindowActorParent> actor;
  if (NS_FAILED(UNWRAP_OBJECT(JSWindowActorParent, &obj, actor))) {
    return nullptr;
  }

  MOZ_RELEASE_ASSERT(!actor->Manager(),
                     "mManager was already initialized once!");
  actor->Init(this);
  mWindowActors.Put(aName, actor);
  return actor.forget();
}

bool WindowGlobalParent::IsCurrentGlobal() {
  return !mIPCClosed && mBrowsingContext->GetCurrentWindowGlobal() == this;
}

void WindowGlobalParent::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCClosed = true;
  gWindowGlobalParentsById->Remove(mInnerWindowId);
  mBrowsingContext->UnregisterWindowGlobal(this);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(this, "window-global-destroyed", nullptr);
  }
}

WindowGlobalParent::~WindowGlobalParent() {
  MOZ_ASSERT(!gWindowGlobalParentsById ||
             !gWindowGlobalParentsById->Contains(mInnerWindowId));
}

JSObject* WindowGlobalParent::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return WindowGlobalParent_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports* WindowGlobalParent::GetParentObject() {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WindowGlobalParent)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WindowGlobalParent, mFrameLoader,
                                      mBrowsingContext, mWindowActors)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WindowGlobalParent)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WindowGlobalParent)

}  // namespace dom
}  // namespace mozilla
