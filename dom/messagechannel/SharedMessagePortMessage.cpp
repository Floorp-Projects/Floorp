/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedMessagePortMessage.h"
#include "MessagePort.h"
#include "MessagePortChild.h"
#include "MessagePortParent.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/PMessagePort.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla {

using namespace ipc;

namespace dom {

/* static */ void
SharedMessagePortMessage::FromSharedToMessagesChild(
                      MessagePortChild* aActor,
                      const nsTArray<RefPtr<SharedMessagePortMessage>>& aData,
                      nsTArray<MessagePortMessage>& aArray)
{
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aArray.IsEmpty());
  aArray.SetCapacity(aData.Length());

  PBackgroundChild* backgroundManager = aActor->Manager();
  MOZ_ASSERT(backgroundManager);

  for (auto& data : aData) {
    MessagePortMessage* message = aArray.AppendElement();
    data->mBuffer->abandon();
    data->mBuffer->steal(&message->data().data);

    const nsTArray<RefPtr<BlobImpl>>& blobImpls = data->BlobImpls();
    if (!blobImpls.IsEmpty()) {
      message->blobsChild().SetCapacity(blobImpls.Length());

      for (uint32_t i = 0, len = blobImpls.Length(); i < len; ++i) {
        PBlobChild* blobChild =
          BackgroundChild::GetOrCreateActorForBlobImpl(backgroundManager,
                                                       blobImpls[i]);
        message->blobsChild().AppendElement(blobChild);
      }
    }

    message->transferredPorts().AppendElements(data->PortIdentifiers());
  }
}

/* static */ bool
SharedMessagePortMessage::FromMessagesToSharedChild(
                      nsTArray<MessagePortMessage>& aArray,
                      FallibleTArray<RefPtr<SharedMessagePortMessage>>& aData)
{
  MOZ_ASSERT(aData.IsEmpty());

  if (NS_WARN_IF(!aData.SetCapacity(aArray.Length(), mozilla::fallible))) {
    return false;
  }

  for (auto& message : aArray) {
    RefPtr<SharedMessagePortMessage> data = new SharedMessagePortMessage();

    data->mBuffer = MakeUnique<JSAutoStructuredCloneBuffer>();
    data->mBuffer->adopt(Move(message.data().data), JS_STRUCTURED_CLONE_VERSION,
                         &StructuredCloneHolder::sCallbacks, data.get());

    const nsTArray<PBlobChild*>& blobs = message.blobsChild();
    if (!blobs.IsEmpty()) {
      data->BlobImpls().SetCapacity(blobs.Length());

      for (uint32_t i = 0, len = blobs.Length(); i < len; ++i) {
        RefPtr<BlobImpl> impl =
          static_cast<BlobChild*>(blobs[i])->GetBlobImpl();
        data->BlobImpls().AppendElement(impl);
      }
    }

    data->PortIdentifiers().AppendElements(message.transferredPorts());

    if (!aData.AppendElement(data, mozilla::fallible)) {
      return false;
    }
  }

  return true;
}

/* static */ bool
SharedMessagePortMessage::FromSharedToMessagesParent(
                      MessagePortParent* aActor,
                      const nsTArray<RefPtr<SharedMessagePortMessage>>& aData,
                      FallibleTArray<MessagePortMessage>& aArray)
{
  MOZ_ASSERT(aArray.IsEmpty());

  if (NS_WARN_IF(!aArray.SetCapacity(aData.Length(), mozilla::fallible))) {
    return false;
  }

  PBackgroundParent* backgroundManager = aActor->Manager();
  MOZ_ASSERT(backgroundManager);

  for (auto& data : aData) {
    MessagePortMessage* message = aArray.AppendElement(mozilla::fallible);
    data->mBuffer->abandon();
    data->mBuffer->steal(&message->data().data);

    const nsTArray<RefPtr<BlobImpl>>& blobImpls = data->BlobImpls();
    if (!blobImpls.IsEmpty()) {
      message->blobsParent().SetCapacity(blobImpls.Length());

      for (uint32_t i = 0, len = blobImpls.Length(); i < len; ++i) {
        PBlobParent* blobParent =
          BackgroundParent::GetOrCreateActorForBlobImpl(backgroundManager,
                                                        blobImpls[i]);
        message->blobsParent().AppendElement(blobParent);
      }
    }

    message->transferredPorts().AppendElements(data->PortIdentifiers());
  }

  return true;
}

/* static */ bool
SharedMessagePortMessage::FromMessagesToSharedParent(
                      nsTArray<MessagePortMessage>& aArray,
                      FallibleTArray<RefPtr<SharedMessagePortMessage>>& aData)
{
  MOZ_ASSERT(aData.IsEmpty());

  if (NS_WARN_IF(!aData.SetCapacity(aArray.Length(), mozilla::fallible))) {
    return false;
  }

  for (auto& message : aArray) {
    RefPtr<SharedMessagePortMessage> data = new SharedMessagePortMessage();

    data->mBuffer = MakeUnique<JSAutoStructuredCloneBuffer>();
    data->mBuffer->adopt(Move(message.data().data), JS_STRUCTURED_CLONE_VERSION,
                         &StructuredCloneHolder::sCallbacks, data.get());

    const nsTArray<PBlobParent*>& blobs = message.blobsParent();
    if (!blobs.IsEmpty()) {
      data->BlobImpls().SetCapacity(blobs.Length());

      for (uint32_t i = 0, len = blobs.Length(); i < len; ++i) {
        RefPtr<BlobImpl> impl =
          static_cast<BlobParent*>(blobs[i])->GetBlobImpl();
        data->BlobImpls().AppendElement(impl);
      }
    }

    data->PortIdentifiers().AppendElements(message.transferredPorts());

    if (!aData.AppendElement(data, mozilla::fallible)) {
      return false;
    }
  }

  return true;
}

} // namespace dom
} // namespace mozilla
