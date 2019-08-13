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
#include "mozilla/dom/BrowserHost.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/WindowGlobalActorsBinding.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/ServoCSSParser.h"
#include "mozilla/ServoStyleSet.h"
#include "mozJSComponentLoader.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsError.h"
#include "nsFrameLoader.h"
#include "nsFrameLoaderOwner.h"
#include "nsGlobalWindowInner.h"
#include "nsQueryObject.h"
#include "nsFrameLoaderOwner.h"
#include "nsSerializationHelper.h"
#include "nsITransportSecurityInfo.h"

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
      mIsInitialDocument(false),
      mHasBeforeUnload(false) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(), "Parent process only");
  MOZ_RELEASE_ASSERT(mDocumentPrincipal, "Must have a valid principal");

  // NOTE: mBrowsingContext initialized in Init()
  MOZ_RELEASE_ASSERT(aInit.browsingContext(),
                     "Must be made in BrowsingContext");
}

void WindowGlobalParent::Init(const WindowGlobalInit& aInit) {
  MOZ_ASSERT(Manager(), "Should have a manager!");

  // Register this WindowGlobal in the gWindowGlobalParentsById map.
  if (!gWindowGlobalParentsById) {
    gWindowGlobalParentsById = new WGPByIdMap();
    ClearOnShutdown(&gWindowGlobalParentsById);
  }
  auto entry = gWindowGlobalParentsById->LookupForAdd(mInnerWindowId);
  MOZ_RELEASE_ASSERT(!entry, "Duplicate WindowGlobalParent entry for ID!");
  entry.OrInsert([&] { return this; });

  // Determine which content process the window global is coming from.
  dom::ContentParentId processId(0);
  if (!mInProcess) {
    ContentParent* cp = static_cast<ContentParent*>(Manager()->Manager());
    processId = cp->ChildID();

    // Ensure the content process has permissions for this principal.
    cp->TransmitPermissionsForPrincipal(mDocumentPrincipal);
  }

  mBrowsingContext = CanonicalBrowsingContext::Cast(aInit.browsingContext());
  MOZ_ASSERT(mBrowsingContext);

  MOZ_DIAGNOSTIC_ASSERT(
      !mBrowsingContext->GetParent() ||
          mBrowsingContext->GetEmbedderWindowGlobal(),
      "When creating a non-root WindowGlobalParent, the WindowGlobalParent "
      "for our embedder should've already been created.");

  // Attach ourself to the browsing context.
  mBrowsingContext->RegisterWindowGlobal(this);

  // If there is no current window global, assume we're about to become it
  // optimistically.
  if (!mBrowsingContext->GetCurrentWindowGlobal()) {
    mBrowsingContext->SetCurrentWindowGlobal(this);
  }

  // Ensure we have a document URI
  if (!mDocumentURI) {
    NS_NewURI(getter_AddRefs(mDocumentURI), "about:blank");
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
  if (!CanSend()) {
    return nullptr;
  }
  IProtocol* otherSide = InProcessParent::ChildActorFor(this);
  return do_AddRef(static_cast<WindowGlobalChild*>(otherSide));
}

already_AddRefed<BrowserParent> WindowGlobalParent::GetBrowserParent() {
  if (IsInProcess() || !CanSend()) {
    return nullptr;
  }
  return do_AddRef(static_cast<BrowserParent*>(Manager()));
}

already_AddRefed<nsFrameLoader> WindowGlobalParent::GetRootFrameLoader() {
  dom::BrowsingContext* top = BrowsingContext()->Top();

  RefPtr<nsFrameLoaderOwner> frameLoaderOwner =
      do_QueryObject(top->GetEmbedderElement());
  if (frameLoaderOwner) {
    return frameLoaderOwner->GetFrameLoader();
  }
  return nullptr;
}

uint64_t WindowGlobalParent::ContentParentId() {
  RefPtr<BrowserParent> browserParent = GetBrowserParent();
  return browserParent ? browserParent->Manager()->ChildID() : 0;
}

int32_t WindowGlobalParent::OsPid() {
  RefPtr<BrowserParent> browserParent = GetBrowserParent();
  return browserParent ? browserParent->Manager()->Pid() : -1;
}

// A WindowGlobalPaernt is the root in its process if it has no parent, or its
// embedder is in a different process.
bool WindowGlobalParent::IsProcessRoot() {
  if (!BrowsingContext()->GetParent()) {
    return true;
  }

  auto* embedder = BrowsingContext()->GetEmbedderWindowGlobal();
  if (NS_WARN_IF(!embedder)) {
    return false;
  }

  return ContentParentId() != embedder->ContentParentId();
}

IPCResult WindowGlobalParent::RecvUpdateDocumentURI(nsIURI* aURI) {
  // XXX(nika): Assert that the URI change was one which makes sense (either
  // about:blank -> a real URI, or a legal push/popstate URI change?)
  mDocumentURI = aURI;
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvSetHasBeforeUnload(bool aHasBeforeUnload) {
  mHasBeforeUnload = aHasBeforeUnload;
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvBecomeCurrentWindowGlobal() {
  mBrowsingContext->SetCurrentWindowGlobal(this);
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvDestroy() {
  if (CanSend()) {
    RefPtr<BrowserParent> browserParent = GetBrowserParent();
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
  if (RefPtr<BrowserParent> browserParent = GetBrowserParent()) {
    return browserParent->Manager()->GetRemoteType();
  }

  return VoidString();
}

already_AddRefed<JSWindowActorParent> WindowGlobalParent::GetActor(
    const nsAString& aName, ErrorResult& aRv) {
  if (!CanSend()) {
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

  MOZ_RELEASE_ASSERT(!actor->GetManager(),
                     "mManager was already initialized once!");
  actor->Init(aName, this);
  mWindowActors.Put(aName, actor);
  return actor.forget();
}

bool WindowGlobalParent::IsCurrentGlobal() {
  return CanSend() && mBrowsingContext->GetCurrentWindowGlobal() == this;
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
  RefPtr<BrowserParent> embedderBrowserParent = GetBrowserParent();
  if (NS_WARN_IF(!embedderBrowserParent)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsIGlobalObject* global = GetParentObject();
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<CanonicalBrowsingContext> browsingContext =
      CanonicalBrowsingContext::Cast(aBc);

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

        // If we got a `BrowserBridgeParent`, the frame is out-of-process, so we
        // can get the target off of it. Otherwise, it's an in-process frame, so
        // we can use the embedder `BrowserParent`.
        RefPtr<BrowserParent> browserParent;
        if (bridge) {
          browserParent = bridge->GetBrowserParent();
        } else {
          browserParent = embedderBrowserParent;
        }
        MOZ_ASSERT(browserParent);

        if (!browserParent || !browserParent->CanSend()) {
          promise->MaybeReject(NS_ERROR_FAILURE);
          return;
        }

        // Update our BrowsingContext to its new owner, if it hasn't been
        // updated yet. This can happen when switching from a out-of-process to
        // in-process frame. For remote frames, the BrowserBridgeParent::Init
        // method should've already set up the OwnerProcessId.
        uint64_t childId = browserParent->Manager()->ChildID();
        MOZ_ASSERT_IF(bridge,
                      browsingContext == browserParent->GetBrowsingContext());
        MOZ_ASSERT_IF(bridge, browsingContext->IsOwnedByProcess(childId));
        browsingContext->SetOwnerProcessId(childId);

        promise->MaybeResolve(childId);
      };

  auto reject = [=](ResponseRejectReason aReason) {
    promise->MaybeReject(NS_ERROR_FAILURE);
  };

  SendChangeFrameRemoteness(aBc, PromiseFlatString(aRemoteType),
                            aPendingSwitchId, resolve, reject);
  return promise.forget();
}

already_AddRefed<mozilla::dom::Promise> WindowGlobalParent::DrawSnapshot(
    const DOMRect* aRect, double aScale, const nsAString& aBackgroundColor,
    mozilla::ErrorResult& aRv) {
  nsIGlobalObject* global = GetParentObject();
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nscolor color;
  if (NS_WARN_IF(!ServoCSSParser::ComputeColor(nullptr, NS_RGB(0, 0, 0),
                                               aBackgroundColor, &color,
                                               nullptr, nullptr))) {
    aRv = NS_ERROR_FAILURE;
    return nullptr;
  }

  if (!gfx::CrossProcessPaint::Start(this, aRect, (float)aScale, color,
                                     promise)) {
    aRv = NS_ERROR_FAILURE;
    return nullptr;
  }
  return promise.forget();
}

void WindowGlobalParent::DrawSnapshotInternal(gfx::CrossProcessPaint* aPaint,
                                              const Maybe<IntRect>& aRect,
                                              float aScale,
                                              nscolor aBackgroundColor) {
  auto promise = SendDrawSnapshot(aRect, aScale, aBackgroundColor);

  RefPtr<gfx::CrossProcessPaint> paint(aPaint);
  RefPtr<WindowGlobalParent> wgp(this);
  promise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [paint, wgp](PaintFragment&& aFragment) {
        paint->ReceiveFragment(wgp, std::move(aFragment));
      },
      [paint, wgp](ResponseRejectReason&& aReason) {
        paint->LostFragment(wgp);
      });
}

already_AddRefed<Promise> WindowGlobalParent::GetSecurityInfo(
    ErrorResult& aRv) {
  RefPtr<BrowserParent> browserParent = GetBrowserParent();
  if (NS_WARN_IF(!browserParent)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsIGlobalObject* global = GetParentObject();
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  SendGetSecurityInfo(
      [promise](Maybe<nsCString>&& aResult) {
        if (aResult) {
          nsCOMPtr<nsISupports> infoObj;
          nsresult rv =
              NS_DeserializeObject(aResult.value(), getter_AddRefs(infoObj));
          if (NS_WARN_IF(NS_FAILED(rv))) {
            promise->MaybeReject(NS_ERROR_FAILURE);
          }
          nsCOMPtr<nsITransportSecurityInfo> info = do_QueryInterface(infoObj);
          if (!info) {
            promise->MaybeReject(NS_ERROR_FAILURE);
          }
          promise->MaybeResolve(info);
        } else {
          promise->MaybeResolveWithUndefined();
        }
      },
      [promise](ResponseRejectReason&& aReason) {
        promise->MaybeReject(NS_ERROR_FAILURE);
      });

  return promise.forget();
}

void WindowGlobalParent::ActorDestroy(ActorDestroyReason aWhy) {
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

nsIGlobalObject* WindowGlobalParent::GetParentObject() {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(WindowGlobalParent, WindowGlobalActor,
                                   mBrowsingContext, mWindowActors)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(WindowGlobalParent,
                                               WindowGlobalActor)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WindowGlobalParent)
NS_INTERFACE_MAP_END_INHERITING(WindowGlobalActor)

NS_IMPL_ADDREF_INHERITED(WindowGlobalParent, WindowGlobalActor)
NS_IMPL_RELEASE_INHERITED(WindowGlobalParent, WindowGlobalActor)

}  // namespace dom
}  // namespace mozilla
