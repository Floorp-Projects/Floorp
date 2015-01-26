/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GraphicsFilter_h
#define GraphicsFilter_h

enum class GraphicsFilter : int {
  FILTER_FAST,
  FILTER_GOOD,
  FILTER_BEST,
  FILTER_NEAREST,
  FILTER_BILINEAR,
  FILTER_GAUSSIAN,
  FILTER_SENTINEL
};

#endif

