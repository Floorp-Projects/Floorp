/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TYPES_H
#define GFX_TYPES_H

#include "prtypes.h"
#include "nsAtomicRefcnt.h"

/**
 * Currently needs to be 'double' for Cairo compatibility. Could
 * become 'float', perhaps, in some configurations.
 */
typedef double gfxFloat;

# define THEBES_API

/**
 * gfx errors
 */

/* nsIDeviceContextSpec.h defines a set of printer errors  */
#define NS_ERROR_GFX_GENERAL_BASE (50) 

/* Font cmap is strangely structured - avoid this font! */
#define NS_ERROR_GFX_CMAP_MALFORMED          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_GENERAL_BASE+1)

/**
 * Priority of a line break opportunity.
 *
 * eNoBreak       The line has no break opportunities
 * eWordWrapBreak The line has a break opportunity only within a word. With
 *                word-wrap: break-word we will break at this point only if
 *                there are no other break opportunities in the line.
 * eNormalBreak   The line has a break opportunity determined by the standard
 *                line-breaking algorithm.
 *
 * Future expansion: split eNormalBreak into multiple priorities, e.g.
 *                    punctuation break and whitespace break (bug 389710).
 *                   As and when we implement it, text-wrap: unrestricted will
 *                    mean that priorities are ignored and all line-break
 *                    opportunities are equal.
 *
 * @see gfxTextRun::BreakAndMeasureText
 * @see nsLineLayout::NotifyOptionalBreakPosition
 */
enum gfxBreakPriority {
    eNoBreak       = 0,
    eWordWrapBreak,
    eNormalBreak
};

#endif /* GFX_TYPES_H */
