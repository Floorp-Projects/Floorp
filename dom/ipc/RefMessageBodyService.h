/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RefMessageBodyService_h
#define mozilla_dom_RefMessageBodyService_h

#include <cstdint>
#include "js/TypeDecls.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/UniquePtr.h"
#include "nsHashKeys.h"
#include "nsID.h"
#include "nsISupports.h"
#include "nsRefPtrHashtable.h"

namespace JS {
class CloneDataPolicy;
}  // namespace JS

namespace mozilla {

class ErrorResult;
template <class T>
class OwningNonNull;

namespace dom {

class MessagePort;
template <typename T>
class Sequence;

namespace ipc {
class StructuredCloneData;
}

/**
 * At the time a BroadcastChannel or MessagePort sends messages, we don't know
 * which process is going to receive it. Because of this, we need to check if
 * the message is able to cross the process boundary.
 * If the message contains objects such as SharedArrayBuffers, WASM modules or
 * ImageBitmaps, it can be delivered on the current process only.
 * Instead of sending the whole message via IPC, we send a unique ID, while the
 * message is kept alive by RefMessageBodyService, on the current process using
 * a ref-counted RefMessageBody object.
 * When the receiver obtains the message ID, it checks if the local
 * RefMessageBodyService knows that ID. If yes, the sender and the receiver are
 * on the same process and the delivery can be completed; if not, a
 * messageerror event has to be dispatched instead.
 *
 * For MessagePort communication is 1-to-1 and because of this, the
 * receiver takes ownership of the message (RefMessageBodyService::Steal()).
 * If the receiver port is on a different process, RefMessageBodyService will
 * return a nullptr and a messageerror event will be dispatched.

 * For BroadcastChannel, the life-time of a message is a bit different than for
 * MessagePort. It has a 1-to-many communication and we could have multiple
 * receivers. Each one needs to deliver the message without taking full
 * ownership of it.
 * In order to support this feature, BroadcastChannel needs to call
 * RefMessageBodyService::SetMaxCount() to inform how many ports are allowed to
 * retrieve the current message, on the current process. Receivers on other
 * processes are not kept in consideration because they will not be able to
 * retrieve the message from RefMessageBodyService. When the last allowed
 * port has called RefMessageBodyService::GetAndCount(), the message is
 * released.
 */
class RefMessageBody final {
  friend class RefMessageBodyService;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefMessageBody)

  RefMessageBody(const nsID& aPortID,
                 UniquePtr<ipc::StructuredCloneData>&& aCloneData);

  const nsID& PortID() const { return mPortID; }

  void Read(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
            const JS::CloneDataPolicy& aCloneDataPolicy, ErrorResult& aRv);

  // This method can be called only if the RefMessageBody is not supposed to be
  // ref-counted (see mMaxCount).
  bool TakeTransferredPortsAsSequence(
      Sequence<OwningNonNull<mozilla::dom::MessagePort>>& aPorts);

 private:
  ~RefMessageBody();

  const nsID mPortID;

  // In case the RefMessageBody is shared and refcounted (see mCount/mMaxCount),
  // we must enforce that the reading does not happen simultaneously on
  // different threads.
  Mutex mMutex MOZ_UNANNOTATED;

  UniquePtr<ipc::StructuredCloneData> mCloneData;

  // When mCount reaches mMaxCount, this object is released by the service.
  Maybe<uint32_t> mMaxCount;
  uint32_t mCount;
};

class RefMessageBodyService final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefMessageBodyService)

  static already_AddRefed<RefMessageBodyService> GetOrCreate();

  void ForgetPort(const nsID& aPortID);

  const nsID Register(already_AddRefed<RefMessageBody> aBody, ErrorResult& aRv);

  already_AddRefed<RefMessageBody> Steal(const nsID& aID);

  already_AddRefed<RefMessageBody> GetAndCount(const nsID& aID);

  void SetMaxCount(const nsID& aID, uint32_t aMaxCount);

 private:
  explicit RefMessageBodyService(const StaticMutexAutoLock& aProofOfLock);
  ~RefMessageBodyService();

  static RefMessageBodyService* GetOrCreateInternal(
      const StaticMutexAutoLock& aProofOfLock);

  nsRefPtrHashtable<nsIDHashKey, RefMessageBody> mMessages;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_RefMessageBodyService_h
