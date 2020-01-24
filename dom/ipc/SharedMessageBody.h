/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SharedMessageBody_h
#define mozilla_dom_SharedMessageBody_h

#include "mozilla/dom/ipc/StructuredCloneData.h"

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

  SharedMessageBody();

  // Note that the populated MessageData borrows the underlying
  // JSStructuredCloneData from the SharedMessageBody, so the caller is
  // required to ensure that the MessageData instances are destroyed prior to
  // the SharedMessageBody instances.
  static void FromSharedToMessagesChild(
      mozilla::ipc::PBackgroundChild* aBackgroundManager,
      const nsTArray<RefPtr<SharedMessageBody>>& aData,
      nsTArray<MessageData>& aArray);

  static bool FromMessagesToSharedChild(
      nsTArray<MessageData>& aArray,
      FallibleTArray<RefPtr<SharedMessageBody>>& aData);

  // Note that the populated MessageData borrows the underlying
  // JSStructuredCloneData from the SharedMessageBody, so the caller is
  // required to ensure that the MessageData instances are destroyed prior to
  // the SharedMessageBody instances.
  static bool FromSharedToMessagesParent(
      mozilla::ipc::PBackgroundParent* aManager,
      const nsTArray<RefPtr<SharedMessageBody>>& aData,
      FallibleTArray<MessageData>& aArray);

  static bool FromMessagesToSharedParent(
      nsTArray<MessageData>& aArray,
      FallibleTArray<RefPtr<SharedMessageBody>>& aData);

  void Read(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
            RefMessageBodyService* aRefMessageBodyService, ErrorResult& aRv);

  void Write(JSContext* aCx, JS::Handle<JS::Value> aValue,
             JS::Handle<JS::Value> aTransfers, nsID& aPortID,
             RefMessageBodyService* aRefMessageBodyService, ErrorResult& aRv);

  bool TakeTransferredPortsAsSequence(
      Sequence<OwningNonNull<mozilla::dom::MessagePort>>& aPorts);

 private:
  ~SharedMessageBody() = default;

  UniquePtr<ipc::StructuredCloneData> mCloneData;

  RefPtr<RefMessageBody> mRefData;
  nsID mRefDataId;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SharedMessageBody_h
