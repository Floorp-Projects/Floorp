/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SharedMessagePortMessage_h
#define mozilla_dom_SharedMessagePortMessage_h

#include "mozilla/dom/ipc/StructuredCloneData.h"

namespace mozilla {
namespace dom {

class MessagePortChild;
class MessagePortMessage;
class MessagePortParent;

class SharedMessagePortMessage final : public ipc::StructuredCloneData
{
public:
  NS_INLINE_DECL_REFCOUNTING(SharedMessagePortMessage)

  SharedMessagePortMessage()
    : ipc::StructuredCloneData()
  {}

  // Note that the populated ClonedMessageData borrows the underlying
  // JSStructuredCloneData from the SharedMessagePortMessage, so the caller is
  // required to ensure that the ClonedMessageData instances are destroyed prior
  // to the SharedMessagePortMessage instances.
  static void
  FromSharedToMessagesChild(
                      MessagePortChild* aActor,
                      const nsTArray<RefPtr<SharedMessagePortMessage>>& aData,
                      nsTArray<ClonedMessageData>& aArray);

  static bool
  FromMessagesToSharedChild(
                     nsTArray<ClonedMessageData>& aArray,
                     FallibleTArray<RefPtr<SharedMessagePortMessage>>& aData);

  // Note that the populated ClonedMessageData borrows the underlying
  // JSStructuredCloneData from the SharedMessagePortMessage, so the caller is
  // required to ensure that the ClonedMessageData instances are destroyed prior
  // to the SharedMessagePortMessage instances.
  static bool
  FromSharedToMessagesParent(
                      MessagePortParent* aActor,
                      const nsTArray<RefPtr<SharedMessagePortMessage>>& aData,
                      FallibleTArray<ClonedMessageData>& aArray);

  static bool
  FromMessagesToSharedParent(
                     nsTArray<ClonedMessageData>& aArray,
                     FallibleTArray<RefPtr<SharedMessagePortMessage>>& aData);

private:
  ~SharedMessagePortMessage() {}
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SharedMessagePortMessage_h
