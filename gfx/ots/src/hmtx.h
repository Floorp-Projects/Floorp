// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_HMTX_H_
#define OTS_HMTX_H_

#include <utility>  // std::pair
#include <vector>

#include "ots.h"

namespace ots {

struct OpenTypeHMTX {
  std::vector<std::pair<uint16_t, int16_t> > metrics;
  std::vector<int16_t> lsbs;
};

}  // namespace ots

#endif  // OTS_HMTX_H_
