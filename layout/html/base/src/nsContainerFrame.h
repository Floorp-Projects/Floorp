/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsContainerFrame_h___
#define nsContainerFrame_h___

#include "nsSplittableFrame.h"
#include "nsFrameList.h"

// Option flags for ReflowChild() and FinishReflowChild()
// member functions
#define NS_FRAME_NO_MOVE_VIEW         0x0001
#define NS_FRAME_NO_MOVE_FRAME        (0x0002 | NS_FRAME_NO_MOVE_VIEW)
#define NS_FRAME_NO_SIZE_VIEW         0x0004
#define NS_FRAME_NO_VISIBILITY        0x0008

/**
 * Implementation of a container frame.
 */
class nsContainerFrame : public nsSplittableFrame
{
public:
  // nsIFrame overrides
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD FirstChild(nsIPresContext* aPresContext,
                        nsIAtom*        aListName,
                        nsIFrame**      aFirstChild) const;
  NS_IMETHOD Destroy(nsIPresContext* aPresContext);
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);
  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**     aFrame);
  NS_IMETHOD ReplaceFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame,
                          nsIFrame*       aNewFrame);
  NS_IMETHOD ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild);

#ifdef DEBUG
  NS_IMETHOD List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif  

  // nsContainerFrame methods
  virtual void DeleteChildsNextInFlow(nsIPresContext* aPresContext,
                                      nsIFrame* aChild);

  static PRInt32 LengthOf(nsIFrame* aFrameList) {
    nsFrameList tmp(aFrameList);
    return tmp.GetLength();
  }

  // Positions the frame's view based on the frame's origin
  static void PositionFrameView(nsIPresContext* aPresContext,
                                nsIFrame*       aKidFrame);

  // Sets several view attributes:
  // - if requested sizes the frame's view based on the current size and origin.
  //   Takes into account the combined area and (if overflow is visible) whether
  //   the frame has children that extend outside
  // - opacity
  // - visibility
  // - content transparency
  // - clip
  //
  // Flags:
  // NS_FRAME_NO_MOVE_VIEW - don't position the frame's view. Set this if you
  //    don't want to automatically sync the frame and view
  // NS_FRAME_NO_SIZE_VIEW - don't size the view
  static void SyncFrameViewAfterReflow(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame,
                                       nsIView*        aView,
                                       const nsRect*   aCombinedArea,
                                       PRUint32        aFlags = 0);
  
  /**
   * Invokes the WillReflow() function, positions the frame and its view (if
   * requested), and then calls Reflow(). If the reflow succeeds and the child
   * frame is complete, deletes any next-in-flows using DeleteChildsNextInFlow()
   *
   * Flags:
   * NS_FRAME_NO_MOVE_VIEW - don't position the frame's view. Set this if you
   *    don't want to automatically sync the frame and view
   * NS_FRAME_NO_MOVE_VIEW - don't move the frame. aX and aY are ignored in this
   *    case. Also implies NS_FRAME_NO_MOVE_VIEW
   */
  nsresult ReflowChild(nsIFrame*                aKidFrame,
                       nsIPresContext*          aPresContext,
                       nsHTMLReflowMetrics&     aDesiredSize,
                       const nsHTMLReflowState& aReflowState,
                       nscoord                  aX,
                       nscoord                  aY,
                       PRUint32                 aFlags,
                       nsReflowStatus&          aStatus);

  /**
   * The second half of frame reflow. Does the following:
   * - sets the frame's bounds
   * - sizes and positions (if requested) the frame's view. If the frame's final
   *   position differs from the current position and the frame itself does not
   *   have a view, then any child frames with views are positioned so they stay
   *   in sync
   * - sets the view's visibility, opacity, content transparency, and clip
   * - invoked the DidReflow() function
   *
   * Flags:
   * NS_FRAME_NO_MOVE_FRAME - don't move the frame. aX and aY are ignored in this
   *    case. Also implies NS_FRAME_NO_MOVE_VIEW
   * NS_FRAME_NO_MOVE_VIEW - don't position the frame's view. Set this if you
   *    don't want to automatically sync the frame and view
   * NS_FRAME_NO_SIZE_VIEW - don't size the frame's view
   */
  static nsresult FinishReflowChild(nsIFrame*            aKidFrame,
                                    nsIPresContext*      aPresContext,
                                    nsHTMLReflowMetrics& aDesiredSize,
                                    nscoord              aX,
                                    nscoord              aY,
                                    PRUint32             aFlags);

  
  static void PositionChildViews(nsIPresContext* aPresContext,
                                 nsIFrame*       aFrame);

protected:
  nsContainerFrame();
  ~nsContainerFrame();

  nsresult GetFrameForPointUsing(nsIPresContext* aPresContext,
                                 const nsPoint& aPoint,
                                 nsIAtom*       aList,
                                 nsFramePaintLayer aWhichLayer,
                                 PRBool         aConsiderSelf,
                                 nsIFrame**     aFrame);

  virtual void PaintChildren(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer,
                             PRUint32             aFlags = 0);

  virtual void PaintChild(nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsIFrame*            aFrame,
                          nsFramePaintLayer    aWhichLayer,
                          PRUint32             aFlags = 0);

  /**
   * Get the frames on the overflow list
   */
  nsIFrame* GetOverflowFrames(nsIPresContext* aPresContext,
                              PRBool          aRemoveProperty) const;

  /**
   * Set the overflow list
   */
  nsresult SetOverflowFrames(nsIPresContext* aPresContext,
                             nsIFrame*       aOverflowFrames);

  /**
   * Moves any frames on both the prev-in-flow's overflow list and the
   * receiver's overflow to the receiver's child list.
   *
   * Resets the overlist pointers to nsnull, and updates the receiver's child
   * count and content mapping.
   *
   * @return PR_TRUE if any frames were moved and PR_FALSE otherwise
   */
  PRBool MoveOverflowToChildList(nsIPresContext* aPresContext);

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
  void PushChildren(nsIPresContext* aPresContext,
                    nsIFrame*       aFromChild,
                    nsIFrame*       aPrevSibling);

  nsFrameList mFrames;
};

#endif /* nsContainerFrame_h___ */
