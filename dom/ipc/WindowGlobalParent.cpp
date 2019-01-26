/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/ipc/InProcessParent.h"
#include "mozilla/dom/ChromeBrowsingContext.h"
#include "mozilla/dom/WindowGlobalActorsBinding.h"

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
  MOZ_RELEASE_ASSERT(aInit.browsingContextId() != 0,
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

  mBrowsingContext = ChromeBrowsingContext::Get(aInit.browsingContextId());
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
  nsCOMPtr<nsIFrameLoaderOwner> flOwner = do_QueryInterface(frameElement);
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
                                      mBrowsingContext)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WindowGlobalParent)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WindowGlobalParent)

}  // namespace dom
}  // namespace mozilla
