/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RemoteLazyInputStreamParent_h
#define mozilla_RemoteLazyInputStreamParent_h

#include "mozilla/PRemoteLazyInputStreamParent.h"

class nsIInputStream;

namespace mozilla {

namespace dom {
class ContentParent;
}

namespace net {
class SocketProcessParent;
}

class NS_NO_VTABLE RemoteLazyInputStreamParentCallback {
 public:
  virtual void ActorDestroyed(const nsID& aID) = 0;

  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

 protected:
  virtual ~RemoteLazyInputStreamParentCallback() = default;
};

class RemoteLazyInputStreamParent final : public PRemoteLazyInputStreamParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteLazyInputStreamParent, final)

  // The size of the inputStream must be passed as argument in order to avoid
  // the use of nsIInputStream::Available() which could open a fileDescriptor in
  // case the stream is a nsFileStream.
  static already_AddRefed<RemoteLazyInputStreamParent> Create(
      nsIInputStream* aInputStream, uint64_t aSize, uint64_t aChildID,
      nsresult* aRv, mozilla::dom::ContentParent* aManager);

  static already_AddRefed<RemoteLazyInputStreamParent> Create(
      nsIInputStream* aInputStream, uint64_t aSize, uint64_t aChildID,
      nsresult* aRv, mozilla::ipc::PBackgroundParent* aManager);

  static already_AddRefed<RemoteLazyInputStreamParent> Create(
      nsIInputStream* aInputStream, uint64_t aSize, uint64_t aChildID,
      nsresult* aRv, mozilla::net::SocketProcessParent* aManager);

  static already_AddRefed<RemoteLazyInputStreamParent> Create(
      const nsID& aID, uint64_t aSize,
      mozilla::ipc::PBackgroundParent* aManager);

  static already_AddRefed<RemoteLazyInputStreamParent> Create(
      const nsID& aID, uint64_t aSize,
      mozilla::net::SocketProcessParent* aManager);

  void ActorDestroy(IProtocol::ActorDestroyReason aReason) override;

  const nsID& ID() const { return mID; }

  uint64_t Size() const { return mSize; }

  void SetCallback(RemoteLazyInputStreamParentCallback* aCallback);

  mozilla::ipc::IPCResult RecvStreamNeeded();

  mozilla::ipc::IPCResult RecvLengthNeeded();

  mozilla::ipc::IPCResult RecvClose();

  mozilla::ipc::IPCResult Recv__delete__() override;

  bool HasValidStream() const;

 private:
  template <typename M>
  static already_AddRefed<RemoteLazyInputStreamParent> CreateCommon(
      nsIInputStream* aInputStream, uint64_t aSize, uint64_t aChildID,
      nsresult* aRv, M* aManager);

  RemoteLazyInputStreamParent(const nsID& aID, uint64_t aSize,
                              mozilla::dom::ContentParent* aManager);

  RemoteLazyInputStreamParent(const nsID& aID, uint64_t aSize,
                              mozilla::ipc::PBackgroundParent* aManager);

  RemoteLazyInputStreamParent(const nsID& aID, uint64_t aSize,
                              mozilla::net::SocketProcessParent* aManager);

  ~RemoteLazyInputStreamParent() = default;

  const nsID mID;
  const uint64_t mSize;

  // Only 1 of these is set. Raw pointer because these managers are keeping
  // the parent actor alive. The pointers will be nullified in ActorDestroyed.
  mozilla::dom::ContentParent* mContentManager;
  mozilla::ipc::PBackgroundParent* mPBackgroundManager;
  mozilla::net::SocketProcessParent* mSocketProcessManager;

  RefPtr<RemoteLazyInputStreamParentCallback> mCallback;

  bool mMigrating;
};

}  // namespace mozilla

#endif  // mozilla_RemoteLazyInputStreamParent_h
