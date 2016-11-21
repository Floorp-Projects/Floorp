/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BorderConsts_h_
#define mozilla_BorderConsts_h_

// thickness of dashed line relative to dotted line
#define DOT_LENGTH  1           // square
#define DASH_LENGTH 3           // 3 times longer than dot

#define C_TL NS_CORNER_TOP_LEFT
#define C_TR NS_CORNER_TOP_RIGHT
#define C_BR NS_CORNER_BOTTOM_RIGHT
#define C_BL NS_CORNER_BOTTOM_LEFT

#define BORDER_SEGMENT_COUNT_MAX 100
#define BORDER_DOTTED_CORNER_MAX_RADIUS 100000

#endif /* mozilla_BorderConsts_h_ */
