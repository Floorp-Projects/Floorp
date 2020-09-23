/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

#include "RTCStatsIdGenerator.h"
#include "mozilla/RandomNum.h"
#include <iostream>
namespace mozilla {

RTCStatsIdGenerator::RTCStatsIdGenerator()
    : mSalt(RandomUint64().valueOr(0xa5a5a5a5)), mCounter(0) {}

nsString RTCStatsIdGenerator::Id(const nsString& aKey) {
  if (!aKey.Length()) {
    MOZ_ASSERT(aKey.Length(), "Stats IDs should never be empty.");
    return aKey;
  }
  if (mAllocated.find(aKey) == mAllocated.end()) {
    mAllocated[aKey] = Generate();
  }
  return mAllocated[aKey];
}

nsString RTCStatsIdGenerator::Generate() {
  auto random = RandomUint64().valueOr(0x1a22);
  auto idNum = static_cast<uint32_t>(mSalt ^ ((mCounter++ << 16) | random));
  nsString id;
  id.AppendInt(idNum, 16);  // Append as hex
  return id;
}

}  // namespace mozilla
