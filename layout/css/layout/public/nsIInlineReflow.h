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
  NS_IMETHOD FindTextRuns(nsCSSLineLayout&  aLineLayout,
                          nsIReflowCommand* aReflowCommand) = 0;

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
 *
 * This is an extension of the nsReflowStatus value; it's bits are used
 * in addition the bits that we add.
 */
typedef nsReflowStatus nsInlineReflowStatus;

// This bit is set, when a break is requested. This bit is orthogonal
// to the nsIFrame::nsReflowStatus completion bits.
#define NS_INLINE_BREAK             0x0100

#define NS_INLINE_IS_BREAK(_status) \
  (0 != ((_status) & NS_INLINE_BREAK))

// When a break is requested, this bit when set indicates that the
// break should occur after the frame just reflowed; when the bit is
// clear the break should occur before the frame just reflowed.
#define NS_INLINE_BREAK_SIDE        0x0200      // 0 = before, !0 = after
#define NS_INLINE_BREAK_BEFORE      0x0000
#define NS_INLINE_BREAK_AFTER       NS_INLINE_BREAK_SIDE

#define NS_INLINE_IS_BREAK_AFTER(_status) \
  (0 != ((_status) & NS_INLINE_BREAK_SIDE))

#define NS_INLINE_IS_BREAK_BEFORE(_status) \
  (NS_INLINE_BREAK == ((_status) & (NS_INLINE_BREAK|NS_INLINE_BREAK_SIDE)))

// The type of break requested can be found in these bits.
#define NS_INLINE_BREAK_TYPE_MASK   0xF000

#define NS_INLINE_GET_BREAK_TYPE(_status) (((_status) >> 12) & 0xF)

#define NS_INLINE_MAKE_BREAK_TYPE(_type)  ((_type) << 12)

// Convenience macro's: Take a completion status and add to it
// the desire to have a line-break before.
#define NS_INLINE_LINE_BREAK_BEFORE(_completionStatus)                  \
  ((_completionStatus) | NS_INLINE_BREAK | NS_INLINE_BREAK_BEFORE |     \
   NS_INLINE_MAKE_BREAK_TYPE(NS_STYLE_CLEAR_LINE))

// Convenience macro's: Take a completion status and add to it
// the desire to have a line-break after.
#define NS_INLINE_LINE_BREAK_AFTER(_completionStatus)                   \
  ((_completionStatus) | NS_INLINE_BREAK | NS_INLINE_BREAK_AFTER |      \
   NS_INLINE_MAKE_BREAK_TYPE(NS_STYLE_CLEAR_LINE))

#endif /* nsIInlineReflow_h___ */
