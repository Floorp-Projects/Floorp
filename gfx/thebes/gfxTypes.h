/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GFX_TYPES_H
#define GFX_TYPES_H

#include "prtypes.h"
#include "nsAtomicRefcnt.h"

/**
 * Currently needs to be 'double' for Cairo compatibility. Could
 * become 'float', perhaps, in some configurations.
 */
typedef double gfxFloat;

#if defined(IMPL_THEBES)
# define THEBES_API NS_EXPORT
#else
# define THEBES_API NS_IMPORT
#endif

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

#define THEBES_INLINE_DECL_THREADSAFE_REFCOUNTING(_class)                     \
public:                                                                       \
    nsrefcnt AddRef(void) {                                                   \
        NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");             \
        nsrefcnt count = NS_AtomicIncrementRefcnt(mRefCnt);                   \
        NS_LOG_ADDREF(this, count, #_class, sizeof(*this));                   \
        return count;                                                         \
    }                                                                         \
    nsrefcnt Release(void) {                                                  \
        NS_PRECONDITION(0 != mRefCnt, "dup release");                         \
        nsrefcnt count = NS_AtomicDecrementRefcnt(mRefCnt);                   \
        NS_LOG_RELEASE(this, count, #_class);                                 \
        if (count == 0) {                                                     \
            mRefCnt = 1; /* stabilize */                                      \
            delete this;                                                      \
            return 0;                                                         \
        }                                                                     \
        return count;                                                         \
    }                                                                         \
protected:                                                                    \
    nsAutoRefCnt mRefCnt;                                                     \
public:

#endif /* GFX_TYPES_H */
