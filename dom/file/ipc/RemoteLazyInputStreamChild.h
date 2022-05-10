/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RemoteLazyInputStreamChild_h
#define mozilla_RemoteLazyInputStreamChild_h

#include "mozilla/PRemoteLazyInputStreamChild.h"

namespace mozilla {

class RemoteLazyInputStream;

class RemoteLazyInputStreamChild final : public PRemoteLazyInputStreamChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteLazyInputStreamChild, final)

  explicit RemoteLazyInputStreamChild(const nsID& aID);

  const nsID& StreamID() const { return mID; }

  // Manage the count of streams registered on this actor. When the count
  // reaches 0 the connection to our remote process will be closed.
  void StreamCreated();
  void StreamConsumed();

  void ActorDestroy(ActorDestroyReason aReason) override;

 private:
  ~RemoteLazyInputStreamChild() override;

  const nsID mID;

  std::atomic<size_t> mStreamCount{0};
};

}  // namespace mozilla

#endif  // mozilla_RemoteLazyInputStreamChild_h
