/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSProcessActorBinding.h"
#include "mozilla/dom/JSProcessActorChild.h"
#include "mozilla/ipc/InProcessChild.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(JSProcessActorChild, JSActor, mManager)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(JSProcessActorChild, JSActor)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JSProcessActorChild)
NS_INTERFACE_MAP_END_INHERITING(JSActor)

NS_IMPL_ADDREF_INHERITED(JSProcessActorChild, JSActor)
NS_IMPL_RELEASE_INHERITED(JSProcessActorChild, JSActor)

JSObject* JSProcessActorChild::WrapObject(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return JSProcessActorChild_Binding::Wrap(aCx, this, aGivenProto);
}

void JSProcessActorChild::SendRawMessage(const JSActorMessageMeta& aMeta,
                                         ipc::StructuredCloneData&& aData,
                                         ipc::StructuredCloneData&& aStack,
                                         ErrorResult& aRv) {
  // We only ever have JSProcessActorChild in a multi-process scenario.
  MOZ_ASSERT(XRE_IsContentProcess());

  if (NS_WARN_IF(!mCanSend || !mManager || !mManager->CanSend())) {
    aRv.ThrowInvalidStateError("JSProcessActorChild cannot send at the moment");
    return;
  }

  if (NS_WARN_IF(
          !AllowMessage(aMeta, aData.DataLength() + aStack.DataLength()))) {
    aRv.ThrowDataCloneError(
        nsPrintfCString("JSProcessActorChild serialization error: data too "
                        "large, in actor '%s'",
                        PromiseFlatCString(aMeta.actorName()).get()));
    return;
  }

  ClonedMessageData msgData;
  ClonedMessageData stackData;
  if (NS_WARN_IF(!aData.BuildClonedMessageDataForChild(mManager, msgData)) ||
      NS_WARN_IF(!aStack.BuildClonedMessageDataForChild(mManager, stackData))) {
    aRv.ThrowDataCloneError(nsPrintfCString(
        "JSProcessActorChild serialization error: cannot clone, in actor '%s'",
        PromiseFlatCString(aMeta.actorName()).get()));
    return;
  }

  if (NS_WARN_IF(!mManager->SendRawMessage(aMeta, msgData, stackData))) {
    aRv.ThrowOperationError(
        nsPrintfCString("JSProcessActorChild send error in actor '%s'",
                        PromiseFlatCString(aMeta.actorName()).get()));
    return;
  }
}

void JSProcessActorChild::Init(const nsACString& aName,
                               ContentChild* aManager) {
  MOZ_ASSERT(!mManager, "Cannot Init() a JSProcessActorChild twice!");
  SetName(aName);
  mManager = aManager;

  InvokeCallback(CallbackFunction::ActorCreated);
}

void JSProcessActorChild::AfterDestroy() {
  JSActor::AfterDestroy();
  mManager = nullptr;
}

}  // namespace dom
}  // namespace mozilla
