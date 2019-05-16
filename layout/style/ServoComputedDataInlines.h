/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoComputedDataInlines_h
#define mozilla_ServoComputedDataInlines_h

#include "mozilla/ServoComputedData.h"
#include "nsStyleStruct.h"

namespace mozilla {
#define STYLE_STRUCT(name_) \
  struct Gecko##name_ {     \
    ServoManuallyDrop<nsStyle##name_> gecko;   \
  };
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
}  // namespace mozilla

#define STYLE_STRUCT(name_)                                          \
  const nsStyle##name_* ServoComputedData::GetStyle##name_() const { \
    return &name_.mPtr->gecko.mInner;                                \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

#endif  // mozilla_ServoComputedDataInlines_h
