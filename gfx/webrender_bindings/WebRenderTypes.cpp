/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderTypes.h"

#include "mozilla/ipc/ByteBuf.h"

namespace mozilla {
namespace wr {

void
Assign_WrVecU8(wr::WrVecU8& aVec, mozilla::ipc::ByteBuf&& aOther)
{
  aVec.data = aOther.mData;
  aVec.length = aOther.mLen;
  aVec.capacity = aOther.mCapacity;
  aOther.mData = nullptr;
  aOther.mLen = 0;
  aOther.mCapacity = 0;
}

} // namespace wr
} // namespace mozilla
