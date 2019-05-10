/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSWindowActorBinding.h"
#include "mozilla/dom/JSWindowActorChild.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/MessageManagerBinding.h"
#include "mozilla/dom/BrowsingContext.h"
#include "nsGlobalWindowInner.h"

namespace mozilla {
namespace dom {

JSWindowActorChild::~JSWindowActorChild() { MOZ_ASSERT(!mManager); }

JSObject* JSWindowActorChild::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return JSWindowActorChild_Binding::Wrap(aCx, this, aGivenProto);
}

WindowGlobalChild* JSWindowActorChild::Manager() const { return mManager; }

void JSWindowActorChild::Init(const nsAString& aName,
                              WindowGlobalChild* aManager) {
  MOZ_ASSERT(!mManager, "Cannot Init() a JSWindowActorChild twice!");
  SetName(aName);
  mManager = aManager;
}

namespace {

class AsyncMessageToParent : public Runnable {
 public:
  AsyncMessageToParent(const JSWindowActorMessageMeta& aMetadata,
                       ipc::StructuredCloneData&& aData,
                       WindowGlobalParent* aParent)
      : mozilla::Runnable("WindowGlobalParent::HandleAsyncMessage"),
        mMetadata(aMetadata),
        mData(std::move(aData)),
        mParent(aParent) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread(), "Should be called on the main thread.");
    mParent->ReceiveRawMessage(mMetadata, std::move(mData));
    return NS_OK;
  }

 private:
  JSWindowActorMessageMeta mMetadata;
  ipc::StructuredCloneData mData;
  RefPtr<WindowGlobalParent> mParent;
};

}  // anonymous namespace

void JSWindowActorChild::SendRawMessage(const JSWindowActorMessageMeta& aMeta,
                                        ipc::StructuredCloneData&& aData,
                                        ErrorResult& aRv) {
  if (NS_WARN_IF(!mCanSend || !mManager || mManager->IsClosed())) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (mManager->IsInProcess()) {
    RefPtr<WindowGlobalParent> wgp = mManager->GetParentActor();
    nsCOMPtr<nsIRunnable> runnable =
        new AsyncMessageToParent(aMeta, std::move(aData), wgp);
    NS_DispatchToMainThread(runnable.forget());
    return;
  }

  // Cross-process case - send data over WindowGlobalChild to other side.
  ClonedMessageData msgData;
  ContentChild* cc = ContentChild::GetSingleton();
  if (NS_WARN_IF(!aData.BuildClonedMessageDataForChild(cc, msgData))) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  if (NS_WARN_IF(!mManager->SendRawMessage(aMeta, msgData))) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
}

Document* JSWindowActorChild::GetDocument(ErrorResult& aRv) {
  if (!mManager) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsGlobalWindowInner* window = mManager->WindowGlobal();
  return window ? window->GetDocument() : nullptr;
}

BrowsingContext* JSWindowActorChild::GetBrowsingContext(ErrorResult& aRv) {
  if (!mManager) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return mManager->BrowsingContext();
}

Nullable<WindowProxyHolder> JSWindowActorChild::GetContentWindow(
    ErrorResult& aRv) {
  if (BrowsingContext* bc = GetBrowsingContext(aRv)) {
    return WindowProxyHolder(bc);
  }
  return nullptr;
}

void JSWindowActorChild::StartDestroy() {
  JSWindowActor::StartDestroy();
  mCanSend = false;
}

void JSWindowActorChild::AfterDestroy() {
  JSWindowActor::AfterDestroy();
  mManager = nullptr;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(JSWindowActorChild, JSWindowActor, mManager)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(JSWindowActorChild,
                                               JSWindowActor)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JSWindowActorChild)
NS_INTERFACE_MAP_END_INHERITING(JSWindowActor)

NS_IMPL_ADDREF_INHERITED(JSWindowActorChild, JSWindowActor)
NS_IMPL_RELEASE_INHERITED(JSWindowActorChild, JSWindowActor)

}  // namespace dom
}  // namespace mozilla
