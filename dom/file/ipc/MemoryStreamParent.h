/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_MemoryStreamParent_h
#define mozilla_dom_ipc_MemoryStreamParent_h

#include "mozilla/ipc/PMemoryStreamParent.h"
#include "mozilla/dom/MemoryBlobImpl.h"

namespace mozilla {
namespace dom {

class MemoryStreamParent final : public mozilla::ipc::PMemoryStreamParent
{
public:
  explicit MemoryStreamParent(uint64_t aSize);

  mozilla::ipc::IPCResult
  RecvAddChunk(nsTArray<unsigned char>&& aData) override;

  void
  ActorDestroy(IProtocol::ActorDestroyReason) override;

  void
  GetStream(nsIInputStream** aInputStream);

private:
  uint64_t mSize;
  uint64_t mCurSize;

  nsTArray<RefPtr<MemoryBlobImpl::DataOwner>> mData;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_MemoryStreamParent_h
