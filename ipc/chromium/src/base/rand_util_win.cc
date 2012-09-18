// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <stdlib.h>

#include "base/basictypes.h"
#include "base/logging.h"

namespace {

uint32_t RandUint32() {
  uint32_t number;
  CHECK(rand_s(&number) == 0);
  return number;
}

}  // namespace

namespace base {

uint64_t RandUint64() {
  uint32_t first_half = RandUint32();
  uint32_t second_half = RandUint32();
  return (static_cast<uint64_t>(first_half) << 32) + second_half;
}

}  // namespace base
