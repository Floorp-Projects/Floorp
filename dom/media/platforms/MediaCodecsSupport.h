/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DOM_MEDIA_PLATFORMS_MEDIACODECSSUPPORT_H_
#define DOM_MEDIA_PLATFORMS_MEDIACODECSSUPPORT_H_
#include "mozilla/EnumSet.h"
#include "nsString.h"

namespace mozilla::media {

enum class DecodeSupport {
  Unsupported = 0,
  SoftwareDecode,
  HardwareDecode,
};

using DecodeSupportSet = EnumSet<DecodeSupport, uint64_t>;
}  // namespace mozilla::media

#endif /* MediaCodecsSupport_h_ */
