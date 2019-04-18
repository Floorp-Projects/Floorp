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

JSObject* JSWindowActorChild::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return JSWindowActorChild_Binding::Wrap(aCx, this, aGivenProto);
}

WindowGlobalChild* JSWindowActorChild::Manager() const { return mManager; }

void JSWindowActorChild::Init(const nsAString& aName,
                              WindowGlobalChild* aManager) {
  MOZ_ASSERT(!mManager, "Cannot Init() a JSWindowActorChild twice!");
  mName = aName;
  mManager = aManager;
}

nsISupports* JSWindowActorChild::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

namespace {

class AsyncMessageToParent : public Runnable {
 public:
  AsyncMessageToParent(const nsAString& aActorName,
                       const nsAString& aMessageName,
                       ipc::StructuredCloneData&& aData,
                       WindowGlobalParent* aParent)
      : mozilla::Runnable("WindowGlobalParent::HandleAsyncMessage"),
        mActorName(aActorName),
        mMessageName(aMessageName),
        mData(std::move(aData)),
        mParent(aParent) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread(), "Should be called on the main thread.");
    mParent->HandleAsyncMessage(mActorName, mMessageName, mData);
    return NS_OK;
  }

 private:
  nsString mActorName;
  nsString mMessageName;
  ipc::StructuredCloneData mData;
  RefPtr<WindowGlobalParent> mParent;
};

}  // anonymous namespace

void JSWindowActorChild::SendAsyncMessage(JSContext* aCx,
                                          const nsAString& aMessageName,
                                          JS::Handle<JS::Value> aObj,
                                          JS::Handle<JS::Value> aTransfers,
                                          ErrorResult& aRv) {
  // If we've closed our channel already, just raise an exception.
  if (NS_WARN_IF(!mManager || mManager->IsClosed())) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // Serialize our object out to a StructuredCloneData.
  ipc::StructuredCloneData data;
  if (!aObj.isUndefined() && !nsFrameMessageManager::GetParamsForMessage(
                                 aCx, aObj, aTransfers, data)) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  // If we're an in-process, asynchronously fire off the callback for in-process
  // loads.
  if (mManager->IsInProcess()) {
    RefPtr<WindowGlobalParent> parent = mManager->GetParentActor();
    RefPtr<AsyncMessageToParent> ev =
        new AsyncMessageToParent(mName, aMessageName, std::move(data), parent);
    DebugOnly<nsresult> rv = NS_DispatchToMainThread(ev);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "JS Window Actor AsyncMessageToParent dispatch failed");
    return;
  }

  // If we're a cross-process, send the async message over the corresponding
  // actor.
  ClonedMessageData msgData;
  ContentChild* cc = ContentChild::GetSingleton();
  if (!data.BuildClonedMessageDataForChild(cc, msgData)) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  if (!mManager->SendAsyncMessage(mName, PromiseFlatString(aMessageName),
                                  msgData)) {
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

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JSWindowActorChild)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(JSWindowActorChild)
NS_IMPL_CYCLE_COLLECTING_RELEASE(JSWindowActorChild)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(JSWindowActorChild, mManager)

}  // namespace dom
}  // namespace mozilla
