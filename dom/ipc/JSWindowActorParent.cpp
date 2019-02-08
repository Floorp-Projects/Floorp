/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSWindowActorBinding.h"
#include "mozilla/dom/JSWindowActorParent.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/MessageManagerBinding.h"

namespace mozilla {
namespace dom {

JSObject* JSWindowActorParent::WrapObject(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return JSWindowActorParent_Binding::Wrap(aCx, this, aGivenProto);
}

WindowGlobalParent* JSWindowActorParent::Manager() const { return mManager; }

void JSWindowActorParent::Init(WindowGlobalParent* aManager) {
  MOZ_ASSERT(!mManager, "Cannot Init() a JSWindowActorParent twice!");
  mManager = aManager;
}

nsISupports* JSWindowActorParent::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

namespace {

class AsyncMessageToChild : public Runnable {
 public:
  AsyncMessageToChild(const nsAString& aActorName,
                      const nsAString& aMessageName,
                      ipc::StructuredCloneData&& aData,
                      WindowGlobalChild* aChild)
      : mozilla::Runnable("WindowGlobalChild::HandleAsyncMessage"),
        mActorName(aActorName),
        mMessageName(aMessageName),
        mData(std::move(aData)),
        mChild(aChild) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread(), "Should be called on the main thread.");
    mChild->HandleAsyncMessage(mActorName, mMessageName, mData);
    return NS_OK;
  }

 private:
  nsString mActorName;
  nsString mMessageName;
  ipc::StructuredCloneData mData;
  RefPtr<WindowGlobalChild> mChild;
};

}  // anonymous namespace

void JSWindowActorParent::SendAsyncMessage(JSContext* aCx,
                                           const nsAString& aActorName,
                                           const nsAString& aMessageName,
                                           JS::Handle<JS::Value> aObj,
                                           JS::Handle<JS::Value> aTransfers,
                                           ErrorResult& aRv) {
  // If we've closed our channel already, just raise a warning.
  if (NS_WARN_IF(mManager->IsClosed())) {
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
    RefPtr<WindowGlobalChild> child = mManager->GetChildActor();
    RefPtr<AsyncMessageToChild> ev = new AsyncMessageToChild(
        aActorName, aMessageName, std::move(data), child);
    DebugOnly<nsresult> rv = NS_DispatchToMainThread(ev);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "JS Window Actor AsyncMessageToChild dispatch failed");
    return;
  }

  // If we're a cross-process, send the async message over the corresponding
  // actor.
  ClonedMessageData msgData;
  RefPtr<TabParent> tabParent = mManager->GetTabParent();
  nsIContentParent* cp = tabParent->Manager();
  if (!data.BuildClonedMessageDataForParent(cp, msgData)) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  if (!mManager->SendAsyncMessage(PromiseFlatString(aActorName),
                                  PromiseFlatString(aMessageName), msgData)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(JSWindowActorParent, mManager)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(JSWindowActorParent, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(JSWindowActorParent, Release)

}  // namespace dom
}  // namespace mozilla
