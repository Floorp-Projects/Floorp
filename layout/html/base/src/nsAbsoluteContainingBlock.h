/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsAbsoluteContainingBlock_h___
#define nsAbsoluteContainingBlock_h___

#include "nslayout.h"
#include "nsIHTMLReflow.h"
#include "nsFrameList.h"

class nsIAtom;
class nsIFrame;
class nsIPresContext;

/**
 * This class contains the logic for being an absolutely containing block.
 *
 * There is no principal child list, just a named child list which contains
 * the absolutely positioned frames
 *
 * @see nsLayoutAtoms::absoluteList
 */
class nsAbsoluteContainingBlock
{
public:
  nsresult FirstChild(nsIAtom* aListName, nsIFrame** aFirstChild) const;
  
  nsresult SetInitialChildList(nsIPresContext& aPresContext,
                               nsIAtom*        aListName,
                               nsIFrame*       aChildList);
  
  // Called by the delegating frame after it has done its reflow first. This
  // function will reflow any absolutely positioned child frames that need to
  // be reflowed, e.g., because the absolutely positioned child frame has
  // 'auto' for an offset, or a percentage based width or height
  nsresult Reflow(nsIPresContext&          aPresContext,
                  const nsHTMLReflowState& aReflowState);

  // Called only for a reflow reason of eReflowReason_Incremental. The
  // aWasHandled return value indicates whether the reflow command was
  // handled (i.e., the reflow command involved an absolutely positioned
  // child element), or whether the caller should handle it
  nsresult IncrementalReflow(nsIPresContext&          aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             PRBool&                  aWasHandled);

  void DeleteFrames(nsIPresContext& aPresContext);

  nsresult GetPositionedInfo(nscoord& aXMost, nscoord& aYMost) const;

protected:
  nsresult ReflowAbsoluteFrame(nsIPresContext&          aPresContext,
                               const nsHTMLReflowState& aReflowState,
                               nsIFrame*                aKidFrame,
                               PRBool                   aInitialReflow,
                               nsReflowStatus&          aStatus);

protected:
  nsFrameList mAbsoluteFrames;  // additional named child list
};

#endif /* nsnsAbsoluteContainingBlock_h___ */

