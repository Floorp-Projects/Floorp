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
#ifndef nsContainerFrame_h___
#define nsContainerFrame_h___

#include "nsSplittableFrame.h"

/**
 * Implementation of a container frame.
 */
class nsContainerFrame : public nsSplittableFrame
{
public:
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const;

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);

  NS_IMETHOD DidReflow(nsIPresContext& aPresContext,
                       nsDidReflowStatus aStatus);

  // Painting
  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);

  /**
   * Find the correct child frame.
   */
  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, 
                             nsIFrame**     aFrame);

  // Child frame enumeration.
  NS_IMETHOD  FirstChild(nsIAtom* aListName, nsIFrame*& aFirstChild) const;

  // re-resolve style context for self and children as necessary
  // Subclasses need to override if they add child lists or
  // if they alter normal style context inheritance
  NS_IMETHOD  ReResolveStyleContext(nsIPresContext* aPresContext,
                                    nsIStyleContext* aParentContext);

  // Debugging
  NS_IMETHOD  List(FILE* out = stdout, PRInt32 aIndent = 0, nsIListFilter *aFilter = nsnull) const;
  NS_IMETHOD  VerifyTree() const;

  /**
   * Return the number of children in the sibling list, starting at aChild.
   * Returns zero if aChild is nsnull.
   */
  static PRInt32 LengthOf(nsIFrame* aChild);

  /**
   * Return the last frame in the sibling list.
   * Returns nsnullif aChild is nsnull.
   */
  static nsIFrame* LastFrame(nsIFrame* aChild);

  /**
   * Returns the frame at the specified index relative to aFrame
   */
  static nsIFrame* FrameAt(nsIFrame* aFrame, PRInt32 aIndex);

  // XXX needs to be virtual so that nsBlockFrame can override it
  virtual PRBool DeleteChildsNextInFlow(nsIPresContext& aPresContext,
                                        nsIFrame* aChild);

protected:
  void DeleteFrameList(nsIPresContext& aPresContext,
                       nsIFrame** aListP);

  void SizeOfWithoutThis(nsISizeOfHandler* aHandler) const;

  nsresult GetFrameForPointUsing(const nsPoint& aPoint,
                                 nsIAtom*       aList,
                                 nsIFrame**     aFrame);

  virtual void  PaintChildren(nsIPresContext&      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect&        aDirtyRect);

  virtual void PaintChild(nsIPresContext&      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsIFrame*            aFrame);

  /**
   * Queries the child frame for the nsIHTMLReflow interface and if it's
   * supported invokes the WillReflow() and Reflow() member functions. If
   * the reflow succeeds and the child frame is complete, deletes any
   * next-in-flows using DeleteChildsNextInFlow()
   */
  nsresult ReflowChild(nsIFrame*                aKidFrame,
                       nsIPresContext&          aPresContext,
                       nsHTMLReflowMetrics&     aDesiredSize,
                       const nsHTMLReflowState& aReflowState,
                       nsReflowStatus&          aStatus);

 /**
  * Moves any frames on both the prev-in-flow's overflow list and the receiver's
  * overflow to the receiver's child list.
  *
  * Resets the overlist pointers to nsnull, and updates the receiver's child
  * count and content mapping.
  *
  * @return  PR_TRUE if any frames were moved and PR_FALSE otherwise
  */
  PRBool MoveOverflowToChildList();

 /**
  * Push aFromChild and its next siblings to the next-in-flow. Change the
  * geometric parent of each frame that's pushed. If there is no next-in-flow
  * the frames are placed on the overflow list (and the geometric parent is
  * left unchanged).
  *
  * Updates the next-in-flow's child count. Does <b>not</b> update the
  * pusher's child count.
  *
  * @param   aFromChild the first child frame to push. It is disconnected from
  *            aPrevSibling
  * @param   aPrevSibling aFromChild's previous sibling. Must not be null. It's
  *            an error to push a parent's first child frame
  */
  void PushChildren(nsIFrame* aFromChild, nsIFrame* aPrevSibling);

  /**
   * Append child list starting at aChild to this frame's child list. Used for
   * processing of the overflow list.
   *
   * Updates this frame's child count and content mapping.
   *
   * @param   aChild the beginning of the child list
   * @param   aSetParent if true each child's geometric (and content parent if
   *            they're the same) parent is set to this frame.
   */
  void AppendChildren(nsIFrame* aChild, PRBool aSetParent = PR_TRUE);

  virtual void WillDeleteNextInFlowFrame(nsIFrame* aNextInFlow);

#ifdef NS_DEBUG
  /**
   * Returns PR_TRUE if aChild is a child of this frame.
   */
  PRBool IsChild(const nsIFrame* aChild) const;
#endif

  nsIFrame*   mFirstChild;
  nsIFrame*   mOverflowList;
};

#endif /* nsContainerFrame_h___ */
