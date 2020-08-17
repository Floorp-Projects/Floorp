/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* constants used throughout the Layout module */

#ifndef LayoutConstants_h___
#define LayoutConstants_h___

#include "nsSize.h"  // for NS_MAXSIZE

/**
 * Constant used to indicate an unconstrained size.
 *
 * NOTE: The constants defined in this file are semantically used as symbolic
 *       values, so user should not depend on the underlying numeric values. If
 *       new specific use cases arise, define a new constant here.
 */
#define NS_UNCONSTRAINEDSIZE NS_MAXSIZE

// NS_AUTOOFFSET is assumed to have the same value as NS_UNCONSTRAINEDSIZE.
#define NS_AUTOOFFSET NS_UNCONSTRAINEDSIZE

// +1 is to avoid clamped huge margin values being processed as auto margins
#define NS_AUTOMARGIN (NS_UNCONSTRAINEDSIZE + 1)

#define NS_INTRINSIC_ISIZE_UNKNOWN nscoord_MIN

// The fallback size of width is 300px and the aspect-ratio is 2:1, based on the
// spec definition in CSS2 section 10.3.2:
// https://drafts.csswg.org/css2/visudet.html#inline-replaced-width
#define REPLACED_ELEM_FALLBACK_PX_WIDTH 300
#define REPLACED_ELEM_FALLBACK_PX_HEIGHT 150

#endif  // LayoutConstants_h___
