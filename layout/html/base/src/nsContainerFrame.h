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
#include "nsFrameList.h"

/**
 * Implementation of a container frame.
 */
class nsContainerFrame : public nsSplittableFrame
{
public:
  // nsIFrame overrides
  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD FirstChild(nsIAtom* aListName, nsIFrame** aFirstChild) const;
  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);
  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer);
  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, 
                             nsIFrame**     aFrame);
  NS_IMETHOD ReResolveStyleContext(nsIPresContext* aPresContext,
                                   nsIStyleContext* aParentContext,
                                   PRInt32 aParentChange,
                                   nsStyleChangeList* aChangeList,
                                   PRInt32* aLocalChange);
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;

  // nsIHTMLReflow overrides
  NS_IMETHOD DidReflow(nsIPresContext& aPresContext,
                       nsDidReflowStatus aStatus);

  // nsContainerFrame methods
  virtual void DeleteChildsNextInFlow(nsIPresContext& aPresContext,
                                      nsIFrame* aChild);

  static PRInt32 LengthOf(nsIFrame* aFrameList) {
    nsFrameList tmp(aFrameList);
    return tmp.GetLength();
  }

#if XXX
  static nsIFrame* FrameAt(nsIFrame* aFrameList, PRInt32 aIndex) {
    nsFrameList tmp(aFrameList);
    return tmp.FrameAt(aIndex);
  }
#endif

protected:
  nsContainerFrame();
  ~nsContainerFrame();

  nsresult GetFrameForPointUsing(const nsPoint& aPoint,
                                 nsIAtom*       aList,
                                 nsIFrame**     aFrame);

  virtual void PaintChildren(nsIPresContext&      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer);

  virtual void PaintChild(nsIPresContext&      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsIFrame*            aFrame,
                          nsFramePaintLayer    aWhichLayer);

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
   * Moves any frames on both the prev-in-flow's overflow list and the
   * receiver's overflow to the receiver's child list.
   *
   * Resets the overlist pointers to nsnull, and updates the receiver's child
   * count and content mapping.
   *
   * @return PR_TRUE if any frames were moved and PR_FALSE otherwise
   */
  PRBool MoveOverflowToChildList();

  /**
   * Push aFromChild and its next siblings to the next-in-flow. Change
   * the geometric parent of each frame that's pushed. If there is no
   * next-in-flow the frames are placed on the overflow list (and the
   * geometric parent is left unchanged).
   *
   * Updates the next-in-flow's child count. Does <b>not</b> update the
   * pusher's child count.
   *
   * @param   aFromChild the first child frame to push. It is disconnected from
   *            aPrevSibling
   * @param   aPrevSibling aFromChild's previous sibling. Must not be null.
   *            It's an error to push a parent's first child frame
   */
  void PushChildren(nsIFrame* aFromChild, nsIFrame* aPrevSibling);

  /**
   * Append child list starting at aChild to this frame's child list.
   *
   * @param   aChild the beginning of the child list
   * @param   aSetParent if true each child's parent is set to this frame.
   */
  void AppendChildren(nsIFrame* aChild, PRBool aSetParent) {
    mFrames.AppendFrames(aSetParent ? this : nsnull, aChild);
  }

  /**
   */
  nsresult AddFrame(const nsHTMLReflowState& aReflowState,
                    nsIFrame *               aAddedFrame);

  /**
   */
  nsresult RemoveAFrame(nsIFrame* aRemovedFrame);

  /**
   * Returns PR_TRUE if aChild is a child of this frame.
   */
  PRBool IsChild(const nsIFrame* aChild) const {
    return mFrames.ContainsFrame(aChild);
  }

  nsFrameList mFrames;
  nsFrameList mOverflowFrames;
};

#endif /* nsContainerFrame_h___ */
