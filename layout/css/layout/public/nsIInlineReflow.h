/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsIInlineReflow_h___
#define nsIInlineReflow_h___

#include "nsISupports.h"
#include "nsStyleConsts.h"
class nsCSSLineLayout;

/* d76e29b0-ff56-11d1-89e7-006008911b81 */
#define NS_IINLINE_REFLOW_IID \
 {0xd76e29b0, 0xff56, 0x11d1, {0x89, 0xe7, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81}}

class nsIInlineReflow {
public:
  /**
   * Recursively find all of the text runs contained in an outer
   * block container. Inline frames implement this by recursing over
   * their children; note that inlines frames may need to create
   * missing child frames before proceeding (e.g. when a tree
   * containing inlines is appended/inserted into a block container
   */
  NS_IMETHOD FindTextRuns(nsCSSLineLayout& aLineLayout) = 0;

  /**
   * InlineReflow method. See below for how to interpret the return value.
   */
  NS_IMETHOD InlineReflow(nsCSSLineLayout&     aLineLayout,
                          nsReflowMetrics&     aDesiredSize,
                          const nsReflowState& aReflowState) = 0;
};

/**
 * For InlineReflow the return value (an nsresult) indicates the
 * status of the reflow operation. If the return value is negative
 * then some sort of catastrophic error has occured (e.g. out of memory).
 * If the return value is non-negative then the macros below can be
 * used to interpret it.
 */
typedef nsresult nsInlineReflowStatus;

// The low 3 bits of the nsInlineReflowStatus indicate what happened
// to the child during it's reflow.
#define NS_INLINE_REFLOW_COMPLETE     0             // note: not a bit!
#define NS_INLINE_REFLOW_NOT_COMPLETE 1         
#define NS_INLINE_REFLOW_BREAK_AFTER  2
#define NS_INLINE_REFLOW_BREAK_BEFORE 3
#define NS_INLINE_REFLOW_REFLOW_MASK  0x3

// The inline reflow status may need to indicate that the next-in-flow
// must be reflowed. This bit is or'd in in that case.
#define NS_INLINE_REFLOW_NEXT_IN_FLOW 0x4

// When a break is indicated (break-before/break-after) the type of
// break requested is indicate in bits 4-7.
#define NS_INLINE_REFLOW_BREAK_MASK   0xF0
#define NS_INLINE_REFLOW_GET_BREAK_TYPE(_status) (((_status) >> 4) & 0xF)
#define NS_INLINE_REFLOW_MAKE_BREAK_TYPE(_type)  ((_type) << 4)

// This macro maps an nsIFrame nsReflowStatus value into an
// nsInlineReflowStatus value.
#define NS_FRAME_REFLOW_STATUS_2_INLINE_REFLOW_STATUS(_status) \
  (((_status) & 0x1) | (((_status) & NS_FRAME_REFLOW_NEXTINFLOW) << 2))

// Convenience macro's
#define NS_INLINE_REFLOW_LINE_BREAK_BEFORE      \
  (NS_INLINE_REFLOW_BREAK_BEFORE |              \
   NS_INLINE_REFLOW_MAKE_BREAK_TYPE(NS_STYLE_CLEAR_LINE))

#define NS_INLINE_REFLOW_LINE_BREAK_AFTER       \
  (NS_INLINE_REFLOW_BREAK_AFTER |               \
   NS_INLINE_REFLOW_MAKE_BREAK_TYPE(NS_STYLE_CLEAR_LINE))

// This macro tests to see if an nsInlineReflowStatus is an error value
// or just a regular return value
#define NS_INLINE_REFLOW_ERROR(_status) (PRInt32(_status) < 0)

// XXX Need a better home for this
#define IS_REFLOW_ERROR(_status) (PRInt32(_status) < 0)

#endif /* nsIInlineReflow_h___ */
