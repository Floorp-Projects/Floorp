// Copyright (c) 2010 Mozilla Foundation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_GSUB_H_
#define OTS_GSUB_H_

#include "ots.h"

namespace ots {

struct OpenTypeGSUB {
  const uint8_t *data;
  uint32_t length;
};

}  // namespace ots

#endif  // OTS_GSUB_H_
