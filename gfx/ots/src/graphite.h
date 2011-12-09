// Copyright (c) 2010 Mozilla Foundation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_GRAPHITE_H_
#define OTS_GRAPHITE_H_

#include "ots.h"

namespace ots {

struct OpenTypeSILF {
  const uint8_t *data;
  uint32_t length;
};

struct OpenTypeSILL {
  const uint8_t *data;
  uint32_t length;
};

struct OpenTypeGLOC {
  const uint8_t *data;
  uint32_t length;
};

struct OpenTypeGLAT {
  const uint8_t *data;
  uint32_t length;
};

struct OpenTypeFEAT {
  const uint8_t *data;
  uint32_t length;
};

}  // namespace ots

#endif  // OTS_GRAPHITE_H_
