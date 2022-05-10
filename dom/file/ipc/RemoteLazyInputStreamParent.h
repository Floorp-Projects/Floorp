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

class RemoteLazyInputStreamParent final : public PRemoteLazyInputStreamParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteLazyInputStreamParent, final)

  explicit RemoteLazyInputStreamParent(const nsID& aID);

  const nsID& ID() const { return mID; }

  mozilla::ipc::IPCResult RecvClone(
      mozilla::ipc::Endpoint<PRemoteLazyInputStreamParent>&& aCloneEndpoint);

  mozilla::ipc::IPCResult RecvStreamNeeded(uint64_t aStart, uint64_t aLength,
                                           StreamNeededResolver&& aResolver);

  mozilla::ipc::IPCResult RecvLengthNeeded(LengthNeededResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGoodbye();

  void ActorDestroy(IProtocol::ActorDestroyReason aReason) override;

 private:
  ~RemoteLazyInputStreamParent() override = default;

  const nsID mID;
};

}  // namespace mozilla

#endif  // mozilla_RemoteLazyInputStreamParent_h
