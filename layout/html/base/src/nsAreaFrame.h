/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsAreaFrame_h___
#define nsAreaFrame_h___

#include "nsBlockFrame.h"
#include "nsVoidArray.h"
#include "nsAbsoluteContainingBlock.h"

class nsSpaceManager;

struct nsStyleDisplay;
struct nsStylePosition;

/**
 * Child list name indices
 * @see #GetAdditionalChildListName()
 */
#define NS_AREA_FRAME_ABSOLUTE_LIST_INDEX   (NS_BLOCK_FRAME_LAST_LIST_INDEX + 1)

/**
 * The area frame has an additional named child list:
 * - "Absolute-list" which contains the absolutely positioned frames
 *
 * @see nsLayoutAtoms::absoluteList
 */
class nsAreaFrame : public nsBlockFrame
{
public:
  friend nsresult NS_NewAreaFrame(nsIPresShell* aPresShell, nsIFrame** aResult, PRUint32 aFlags);
  
  // nsIFrame
  NS_IMETHOD Destroy(nsIPresContext* aPresContext);

  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD AppendFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD InsertFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);
  NS_IMETHOD RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  NS_IMETHOD GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const;

  NS_IMETHOD FirstChild(nsIPresContext* aPresContext,
                        nsIAtom*        aListName,
                        nsIFrame**      aFirstChild) const;

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::areaFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
  
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

protected:
  nsAreaFrame();

private:
  nsAbsoluteContainingBlock mAbsoluteContainer;
};

#endif /* nsAreaFrame_h___ */
