/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SharedMessageBody_h
#define mozilla_dom_SharedMessageBody_h

#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/Maybe.h"

namespace mozilla {

namespace ipc {
class PBackgroundChild;
}

namespace dom {

class MessagePort;
class RefMessageBody;
class RefMessageBodyService;

class SharedMessageBody final {
 public:
  NS_INLINE_DECL_REFCOUNTING(SharedMessageBody)

  SharedMessageBody(
      StructuredCloneHolder::TransferringSupport aSupportsTransferring,
      const Maybe<nsID>& aAgentClusterId);

  // Note that the populated MessageData borrows the underlying
  // JSStructuredCloneData from the SharedMessageBody, so the caller is
  // required to ensure that the MessageData instances are destroyed prior to
  // the SharedMessageBody instances.
  static void FromSharedToMessageChild(
      mozilla::ipc::PBackgroundChild* aBackgroundManager,
      SharedMessageBody* aData, MessageData& aMessage);
  static void FromSharedToMessagesChild(
      mozilla::ipc::PBackgroundChild* aBackgroundManager,
      const nsTArray<RefPtr<SharedMessageBody>>& aData,
      nsTArray<MessageData>& aArray);

  // Const MessageData.
  static already_AddRefed<SharedMessageBody> FromMessageToSharedChild(
      MessageData& aMessage,
      StructuredCloneHolder::TransferringSupport aSupportsTransferring =
          StructuredCloneHolder::TransferringSupported);
  // Non-const MessageData.
  static already_AddRefed<SharedMessageBody> FromMessageToSharedChild(
      const MessageData& aMessage,
      StructuredCloneHolder::TransferringSupport aSupportsTransferring =
          StructuredCloneHolder::TransferringSupported);
  // Array of MessageData objects
  static bool FromMessagesToSharedChild(
      nsTArray<MessageData>& aArray,
      FallibleTArray<RefPtr<SharedMessageBody>>& aData,
      StructuredCloneHolder::TransferringSupport aSupportsTransferring =
          StructuredCloneHolder::TransferringSupported);

  // Note that the populated MessageData borrows the underlying
  // JSStructuredCloneData from the SharedMessageBody, so the caller is
  // required to ensure that the MessageData instances are destroyed prior to
  // the SharedMessageBody instances.
  static bool FromSharedToMessagesParent(
      mozilla::ipc::PBackgroundParent* aManager,
      const nsTArray<RefPtr<SharedMessageBody>>& aData,
      nsTArray<MessageData>& aArray);

  static already_AddRefed<SharedMessageBody> FromMessageToSharedParent(
      MessageData& aMessage,
      StructuredCloneHolder::TransferringSupport aSupportsTransferring =
          StructuredCloneHolder::TransferringSupported);
  static bool FromMessagesToSharedParent(
      nsTArray<MessageData>& aArray,
      FallibleTArray<RefPtr<SharedMessageBody>>& aData,
      StructuredCloneHolder::TransferringSupport aSupportsTransferring =
          StructuredCloneHolder::TransferringSupported);

  enum ReadMethod {
    StealRefMessageBody,
    KeepRefMessageBody,
  };

  void Read(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
            RefMessageBodyService* aRefMessageBodyService,
            ReadMethod aReadMethod, ErrorResult& aRv);

  void Write(JSContext* aCx, JS::Handle<JS::Value> aValue,
             JS::Handle<JS::Value> aTransfers, nsID& aPortID,
             RefMessageBodyService* aRefMessageBodyService, ErrorResult& aRv);

  bool TakeTransferredPortsAsSequence(
      Sequence<OwningNonNull<mozilla::dom::MessagePort>>& aPorts);

 private:
  ~SharedMessageBody() = default;

  UniquePtr<ipc::StructuredCloneData> mCloneData;

  RefPtr<RefMessageBody> mRefData;
  Maybe<nsID> mRefDataId;

  const StructuredCloneHolder::TransferringSupport mSupportsTransferring;
  const Maybe<nsID> mAgentClusterId;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SharedMessageBody_h
