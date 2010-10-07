// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_HHEA_H_
#define OTS_HHEA_H_

#include "ots.h"

namespace ots {

struct OpenTypeHHEA {
  int16_t ascent;
  int16_t descent;
  int16_t linegap;
  uint16_t adv_width_max;
  int16_t min_lsb;
  int16_t min_rsb;
  int16_t x_max_extent;
  int16_t caret_slope_rise;
  int16_t caret_slope_run;
  int16_t caret_offset;
  uint16_t num_hmetrics;
};

}  // namespace ots

#endif  // OTS_HHEA_H_
