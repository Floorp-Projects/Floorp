/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderTypes.h"

#include "mozilla/ipc/ByteBuf.h"

namespace mozilla {
namespace wr {

Vec_u8::Vec_u8(mozilla::ipc::ByteBuf&& aSrc) {
  inner.data = aSrc.mData;
  inner.length = aSrc.mLen;
  inner.capacity = aSrc.mCapacity;
  aSrc.mData = nullptr;
  aSrc.mLen = 0;
  aSrc.mCapacity = 0;
}

} // namespace wr
} // namespace mozilla
