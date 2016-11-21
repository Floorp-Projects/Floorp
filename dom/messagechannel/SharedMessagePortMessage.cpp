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
                      nsTArray<ClonedMessageData>& aArray)
{
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aArray.IsEmpty());
  aArray.SetCapacity(aData.Length());

  PBackgroundChild* backgroundManager = aActor->Manager();
  MOZ_ASSERT(backgroundManager);

  for (auto& data : aData) {
    ClonedMessageData* message = aArray.AppendElement();
    data->BuildClonedMessageDataForBackgroundChild(backgroundManager, *message);
  }
}

/* static */ bool
SharedMessagePortMessage::FromMessagesToSharedChild(
                      nsTArray<ClonedMessageData>& aArray,
                      FallibleTArray<RefPtr<SharedMessagePortMessage>>& aData)
{
  MOZ_ASSERT(aData.IsEmpty());

  if (NS_WARN_IF(!aData.SetCapacity(aArray.Length(), mozilla::fallible))) {
    return false;
  }

  for (auto& message : aArray) {
    RefPtr<SharedMessagePortMessage> data = new SharedMessagePortMessage();
    data->StealFromClonedMessageDataForBackgroundChild(message);

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
                      FallibleTArray<ClonedMessageData>& aArray)
{
  MOZ_ASSERT(aArray.IsEmpty());

  if (NS_WARN_IF(!aArray.SetCapacity(aData.Length(), mozilla::fallible))) {
    return false;
  }

  PBackgroundParent* backgroundManager = aActor->Manager();
  MOZ_ASSERT(backgroundManager);

  for (auto& data : aData) {
    ClonedMessageData* message = aArray.AppendElement(mozilla::fallible);
    data->BuildClonedMessageDataForBackgroundParent(backgroundManager,
                                                    *message);
  }

  return true;
}

/* static */ bool
SharedMessagePortMessage::FromMessagesToSharedParent(
                      nsTArray<ClonedMessageData>& aArray,
                      FallibleTArray<RefPtr<SharedMessagePortMessage>>& aData)
{
  MOZ_ASSERT(aData.IsEmpty());

  if (NS_WARN_IF(!aData.SetCapacity(aArray.Length(), mozilla::fallible))) {
    return false;
  }

  for (auto& message : aArray) {
    RefPtr<SharedMessagePortMessage> data = new SharedMessagePortMessage();
    data->StealFromClonedMessageDataForBackgroundParent(message);

    if (!aData.AppendElement(data, mozilla::fallible)) {
      return false;
    }
  }

  return true;
}

} // namespace dom
} // namespace mozilla
