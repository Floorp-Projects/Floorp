/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSProcessActorBinding.h"
#include "mozilla/dom/JSProcessActorParent.h"
#include "mozilla/dom/InProcessChild.h"
#include "mozilla/dom/InProcessParent.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(JSProcessActorParent, JSActor, mManager)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(JSProcessActorParent, JSActor)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JSProcessActorParent)
NS_INTERFACE_MAP_END_INHERITING(JSActor)

NS_IMPL_ADDREF_INHERITED(JSProcessActorParent, JSActor)
NS_IMPL_RELEASE_INHERITED(JSProcessActorParent, JSActor)

JSObject* JSProcessActorParent::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return JSProcessActorParent_Binding::Wrap(aCx, this, aGivenProto);
}

void JSProcessActorParent::Init(const nsACString& aName,
                                nsIDOMProcessParent* aManager) {
  MOZ_ASSERT(!mManager, "Cannot Init() a JSProcessActorParent twice!");
  SetName(aName);
  mManager = aManager;

  InvokeCallback(CallbackFunction::ActorCreated);
}

JSProcessActorParent::~JSProcessActorParent() { MOZ_ASSERT(!mManager); }

void JSProcessActorParent::SendRawMessage(const JSActorMessageMeta& aMeta,
                                          ipc::StructuredCloneData&& aData,
                                          ipc::StructuredCloneData&& aStack,
                                          ErrorResult& aRv) {
  if (NS_WARN_IF(!CanSend() || !mManager || !mManager->GetCanSend())) {
    aRv.ThrowInvalidStateError(
        nsPrintfCString("Actor '%s' cannot send message '%s' during shutdown.",
                        PromiseFlatCString(aMeta.actorName()).get(),
                        NS_ConvertUTF16toUTF8(aMeta.messageName()).get()));
    return;
  }

  if (NS_WARN_IF(
          !AllowMessage(aMeta, aData.DataLength() + aStack.DataLength()))) {
    aRv.ThrowDataError(nsPrintfCString(
        "Actor '%s' cannot send message '%s': message too long.",
        PromiseFlatCString(aMeta.actorName()).get(),
        NS_ConvertUTF16toUTF8(aMeta.messageName()).get()));
    return;
  }

  // If the parent side is in the same process, we have a PInProcess manager,
  // and can dispatch the message directly to the event loop.
  ContentParent* contentParent = mManager->AsContentParent();
  if (!contentParent) {
    SendRawMessageInProcess(aMeta, std::move(aData), std::move(aStack), []() {
      return do_AddRef(InProcessChild::Singleton());
    });
    return;
  }

  // Cross-process case - send data over ContentParent to other side.
  ClonedMessageData msgData;
  ClonedMessageData stackData;
  if (NS_WARN_IF(
          !aData.BuildClonedMessageDataForParent(contentParent, msgData)) ||
      NS_WARN_IF(
          !aStack.BuildClonedMessageDataForParent(contentParent, stackData))) {
    aRv.ThrowDataCloneError(
        nsPrintfCString("Actor '%s' cannot send message '%s': cannot clone.",
                        PromiseFlatCString(aMeta.actorName()).get(),
                        NS_ConvertUTF16toUTF8(aMeta.messageName()).get()));
    return;
  }

  if (NS_WARN_IF(!contentParent->SendRawMessage(aMeta, msgData, stackData))) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
}

void JSProcessActorParent::ClearManager() { mManager = nullptr; }

}  // namespace dom
}  // namespace mozilla
