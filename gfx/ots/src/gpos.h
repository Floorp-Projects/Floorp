// Copyright (c) 2010 Mozilla Foundation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_GPOS_H_
#define OTS_GPOS_H_

#include "ots.h"

namespace ots {

struct OpenTypeGPOS {
  const uint8_t *data;
  uint32_t length;
};

}  // namespace ots

#endif  // OTS_GPOS_H_
