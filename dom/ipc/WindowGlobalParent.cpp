/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WindowGlobalParent.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ipc/InProcessParent.h"
#include "mozilla/dom/BrowserBridgeParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/WindowGlobalActorsBinding.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozJSComponentLoader.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsError.h"
#include "nsFrameLoader.h"
#include "nsFrameLoaderOwner.h"
#include "nsGlobalWindowInner.h"
#include "nsQueryObject.h"
#include "nsFrameLoaderOwner.h"

#include "mozilla/dom/JSWindowActorBinding.h"
#include "mozilla/dom/JSWindowActorParent.h"
#include "mozilla/dom/JSWindowActorService.h"

using namespace mozilla::ipc;
using namespace mozilla::dom::ipc;

namespace mozilla {
namespace dom {

typedef nsRefPtrHashtable<nsUint64HashKey, WindowGlobalParent> WGPByIdMap;
static StaticAutoPtr<WGPByIdMap> gWindowGlobalParentsById;

WindowGlobalParent::WindowGlobalParent(const WindowGlobalInit& aInit,
                                       bool aInProcess)
    : mDocumentPrincipal(aInit.principal()),
      mDocumentURI(aInit.documentURI()),
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

  // Attach ourself to the browsing context.
  mBrowsingContext->RegisterWindowGlobal(this);

  // If there is no current window global, assume we're about to become it
  // optimistically.
  if (!mBrowsingContext->GetCurrentWindowGlobal()) {
    mBrowsingContext->SetCurrentWindowGlobal(this);
  }

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
    frameElement = static_cast<BrowserParent*>(Manager())->GetOwnerElement();
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

/* static */
already_AddRefed<WindowGlobalParent> WindowGlobalParent::GetByInnerWindowId(
    uint64_t aInnerWindowId) {
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

already_AddRefed<BrowserParent> WindowGlobalParent::GetRemoteTab() {
  if (IsInProcess() || mIPCClosed) {
    return nullptr;
  }
  return do_AddRef(static_cast<BrowserParent*>(Manager()));
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
    RefPtr<BrowserParent> browserParent = GetRemoteTab();
    if (!browserParent || !browserParent->IsDestroyed()) {
      // Make a copy so that we can avoid potential iterator invalidation when
      // calling the user-provided Destroy() methods.
      nsTArray<RefPtr<JSWindowActorParent>> windowActors(mWindowActors.Count());
      for (auto iter = mWindowActors.Iter(); !iter.Done(); iter.Next()) {
        windowActors.AppendElement(iter.UserData());
      }

      for (auto& windowActor : windowActors) {
        windowActor->StartDestroy();
      }
      Unused << Send__delete__(this);
    }
  }
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvRawMessage(
    const JSWindowActorMessageMeta& aMeta, const ClonedMessageData& aData) {
  StructuredCloneData data;
  data.BorrowFromClonedMessageDataForParent(aData);
  ReceiveRawMessage(aMeta, std::move(data));
  return IPC_OK();
}

void WindowGlobalParent::ReceiveRawMessage(
    const JSWindowActorMessageMeta& aMeta, StructuredCloneData&& aData) {
  RefPtr<JSWindowActorParent> actor =
      GetActor(aMeta.actorName(), IgnoreErrors());
  if (actor) {
    actor->ReceiveRawMessage(aMeta, std::move(aData));
  }
}

const nsAString& WindowGlobalParent::GetRemoteType() {
  if (RefPtr<BrowserParent> browserParent = GetRemoteTab()) {
    return browserParent->Manager()->GetRemoteType();
  }

  return VoidString();
}

already_AddRefed<JSWindowActorParent> WindowGlobalParent::GetActor(
    const nsAString& aName, ErrorResult& aRv) {
  if (mIPCClosed) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Check if this actor has already been created, and return it if it has.
  if (mWindowActors.Contains(aName)) {
    return do_AddRef(mWindowActors.GetWeak(aName));
  }

  // Otherwise, we want to create a new instance of this actor.
  JS::RootedObject obj(RootingCx());
  ConstructActor(aName, &obj, aRv);
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
  actor->Init(aName, this);
  mWindowActors.Put(aName, actor);
  return actor.forget();
}

bool WindowGlobalParent::IsCurrentGlobal() {
  return !mIPCClosed && mBrowsingContext->GetCurrentWindowGlobal() == this;
}

IPCResult WindowGlobalParent::RecvDidEmbedBrowsingContext(
    dom::BrowsingContext* aContext) {
  MOZ_ASSERT(aContext);
  aContext->Canonical()->SetEmbedderWindowGlobal(this);
  return IPC_OK();
}

already_AddRefed<Promise> WindowGlobalParent::ChangeFrameRemoteness(
    dom::BrowsingContext* aBc, const nsAString& aRemoteType,
    uint64_t aPendingSwitchId, ErrorResult& aRv) {
  RefPtr<BrowserParent> browserParent = GetRemoteTab();
  if (NS_WARN_IF(!browserParent)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsIGlobalObject* global = xpc::NativeGlobal(xpc::PrivilegedJunkScope());
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // When the reply comes back from content, either resolve or reject.
  auto resolve =
      [=](mozilla::Tuple<nsresult, PBrowserBridgeParent*>&& aResult) {
        nsresult rv = Get<0>(aResult);
        RefPtr<BrowserBridgeParent> bridge =
            static_cast<BrowserBridgeParent*>(Get<1>(aResult));
        if (NS_FAILED(rv)) {
          promise->MaybeReject(rv);
          return;
        }

        // If we got a BrowserBridgeParent, the frame is out-of-process, so pull
        // our target BrowserParent off of it. Otherwise, it's an in-process
        // frame, so we can directly use ours.
        if (bridge) {
          promise->MaybeResolve(bridge->GetBrowserParent());
        } else {
          promise->MaybeResolve(browserParent);
        }
      };

  auto reject = [=](ResponseRejectReason aReason) {
    promise->MaybeReject(NS_ERROR_FAILURE);
  };

  SendChangeFrameRemoteness(aBc, PromiseFlatString(aRemoteType),
                            aPendingSwitchId, resolve, reject);
  return promise.forget();
}

void WindowGlobalParent::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCClosed = true;
  gWindowGlobalParentsById->Remove(mInnerWindowId);
  mBrowsingContext->UnregisterWindowGlobal(this);

  // Destroy our JSWindowActors, and reject any pending queries.
  nsRefPtrHashtable<nsStringHashKey, JSWindowActorParent> windowActors;
  mWindowActors.SwapElements(windowActors);
  for (auto iter = windowActors.Iter(); !iter.Done(); iter.Next()) {
    iter.Data()->RejectPendingQueries();
    iter.Data()->AfterDestroy();
  }
  windowActors.Clear();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(this, "window-global-destroyed", nullptr);
  }
}

WindowGlobalParent::~WindowGlobalParent() {
  MOZ_ASSERT(!gWindowGlobalParentsById ||
             !gWindowGlobalParentsById->Contains(mInnerWindowId));
  MOZ_ASSERT(!mWindowActors.Count());
}

JSObject* WindowGlobalParent::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return WindowGlobalParent_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports* WindowGlobalParent::GetParentObject() {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(WindowGlobalParent, WindowGlobalActor,
                                   mFrameLoader, mBrowsingContext,
                                   mWindowActors)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(WindowGlobalParent,
                                               WindowGlobalActor)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WindowGlobalParent)
NS_INTERFACE_MAP_END_INHERITING(WindowGlobalActor)

NS_IMPL_ADDREF_INHERITED(WindowGlobalParent, WindowGlobalActor)
NS_IMPL_RELEASE_INHERITED(WindowGlobalParent, WindowGlobalActor)

}  // namespace dom
}  // namespace mozilla
