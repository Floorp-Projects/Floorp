/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSWindowActorBinding.h"
#include "mozilla/dom/JSWindowActorChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/WindowProxyHolder.h"
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

WindowGlobalChild* JSWindowActorChild::GetManager() const { return mManager; }

void JSWindowActorChild::Init(const nsACString& aName,
                              WindowGlobalChild* aManager) {
  MOZ_ASSERT(!mManager, "Cannot Init() a JSWindowActorChild twice!");
  SetName(aName);
  mManager = aManager;

  InvokeCallback(CallbackFunction::ActorCreated);
}

void JSWindowActorChild::SendRawMessage(const JSActorMessageMeta& aMeta,
                                        ipc::StructuredCloneData&& aData,
                                        ipc::StructuredCloneData&& aStack,
                                        ErrorResult& aRv) {
  if (NS_WARN_IF(!CanSend() || !mManager || !mManager->CanSend())) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (mManager->IsInProcess()) {
    SendRawMessageInProcess(
        aMeta, std::move(aData), std::move(aStack),
        [manager{mManager}]() { return manager->GetParentActor(); });
    return;
  }

  if (NS_WARN_IF(
          !AllowMessage(aMeta, aData.DataLength() + aStack.DataLength()))) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  // Cross-process case - send data over WindowGlobalChild to other side.
  ClonedMessageData msgData;
  ClonedMessageData stackData;
  ContentChild* cc = ContentChild::GetSingleton();
  if (NS_WARN_IF(!aData.BuildClonedMessageDataForChild(cc, msgData)) ||
      NS_WARN_IF(!aStack.BuildClonedMessageDataForChild(cc, stackData))) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  if (NS_WARN_IF(!mManager->SendRawMessage(aMeta, msgData, stackData))) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
}

Document* JSWindowActorChild::GetDocument(ErrorResult& aRv) {
  if (!mManager) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsGlobalWindowInner* window = mManager->GetWindowGlobal();
  return window ? window->GetDocument() : nullptr;
}

BrowsingContext* JSWindowActorChild::GetBrowsingContext(ErrorResult& aRv) {
  if (!mManager) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return mManager->BrowsingContext();
}

nsIDocShell* JSWindowActorChild::GetDocShell(ErrorResult& aRv) {
  if (BrowsingContext* bc = GetBrowsingContext(aRv)) {
    return bc->GetDocShell();
  }

  return nullptr;
}

Nullable<WindowProxyHolder> JSWindowActorChild::GetContentWindow(
    ErrorResult& aRv) {
  if (BrowsingContext* bc = GetBrowsingContext(aRv)) {
    return WindowProxyHolder(bc);
  }
  return nullptr;
}

void JSWindowActorChild::ClearManager() { mManager = nullptr; }

NS_IMPL_CYCLE_COLLECTION_INHERITED(JSWindowActorChild, JSActor, mManager)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(JSWindowActorChild, JSActor)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JSWindowActorChild)
NS_INTERFACE_MAP_END_INHERITING(JSActor)

NS_IMPL_ADDREF_INHERITED(JSWindowActorChild, JSActor)
NS_IMPL_RELEASE_INHERITED(JSWindowActorChild, JSActor)

}  // namespace dom
}  // namespace mozilla
