/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SharedMessagePortMessage_h
#define mozilla_dom_SharedMessagePortMessage_h

#include "mozilla/dom/StructuredCloneHolder.h"

namespace mozilla {
namespace dom {

class MessagePortChild;
class MessagePortMessage;
class MessagePortParent;

class SharedMessagePortMessage final : public StructuredCloneHolder
{
public:
  NS_INLINE_DECL_REFCOUNTING(SharedMessagePortMessage)

  nsTArray<uint8_t> mData;

  SharedMessagePortMessage()
    : StructuredCloneHolder(CloningSupported, TransferringSupported,
                            StructuredCloneScope::DifferentProcess)
  {}

  void Read(nsISupports* aParent,
            JSContext* aCx,
            JS::MutableHandle<JS::Value> aValue,
            ErrorResult& aRv);

  void Write(JSContext* aCx,
             JS::Handle<JS::Value> aValue,
             JS::Handle<JS::Value> aTransfer,
             ErrorResult& aRv);

  void Free();

  static void
  FromSharedToMessagesChild(
                      MessagePortChild* aActor,
                      const nsTArray<RefPtr<SharedMessagePortMessage>>& aData,
                      nsTArray<MessagePortMessage>& aArray);

  static bool
  FromMessagesToSharedChild(
                     nsTArray<MessagePortMessage>& aArray,
                     FallibleTArray<RefPtr<SharedMessagePortMessage>>& aData);

  static bool
  FromSharedToMessagesParent(
                      MessagePortParent* aActor,
                      const nsTArray<RefPtr<SharedMessagePortMessage>>& aData,
                      FallibleTArray<MessagePortMessage>& aArray);

  static bool
  FromMessagesToSharedParent(
                     nsTArray<MessagePortMessage>& aArray,
                     FallibleTArray<RefPtr<SharedMessagePortMessage>>& aData);

private:
  ~SharedMessagePortMessage();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SharedMessagePortMessage_h
