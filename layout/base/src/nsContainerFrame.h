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
 * Implementation of a container frame. Supports being used a pseudo-
 * frame (a frame that maps the same content as its parent).
 *
 * Container frame iterates its child content and for each content object creates
 * one or more child frames. There are three member variables (class invariants)
 * used to manage this mapping:
 * <dl>
 * <dt>mFirstContentOffset
 *   <dd>the content offset of the first piece of content mapped by this container.
 * <dt>mLastContentOffset
 *   <dd>the content offset of the last piece of content mapped by this container
 * <dt>mLastContentIsComplete
 *   <dd>a boolean indicating whether the last piece of content mapped by this
 *       container is complete, or whether its frame needs to be continued.
 * </dl>
 *
 * Here are the rules governing the state of the class invariants. The debug member
 * functions PreReflowCheck() and PostReflowCheck() validate these rules. For the
 * purposes of this discussion an <i>empty</i> frame is defined as a container
 * frame with a null child-list, i.e. its mFirstChild is nsnull.
 *
 * <h3>Non-Empty Frames</h3>
 * For a container frame that is not an empty frame the following must be true:
 * <ul>
 * <li>if the first child frame is a pseudo-frame then mFirstContentOffset must be
 *     equal to the first child's mFirstContentOffset; otherwise, mFirstContentOffset
 *     must be equal to the first child frame's index in parent
 * <li>if the last child frame is a pseudo-frame then mLastContentOffset must be
 *     equal to the last child's mLastContentOffset; otherwise, mLastContentOffset
 *     must be equal to the last child frame's index in parent
 * <li>if the last child is a pseudo-frame then mLastContentIsComplete should be
 *     equal to that last child's mLastContentIsComplete; otherwise, if the last
 *     child frame has a next-in-flow then mLastContentIsComplete must be PR_TRUE
 * </ul>
 *
 * <h3>Empty Frames</h3>
 * For an empty container frame the following must be true:
 * <ul>
 * <li>mFirstContentOffset must be equal to the NextContentOffset() of the first
 *     non-empty prev-in-flow
 * <li>mChildCount must be 0
 * </ul>
 *
 * The value of mLastContentOffset and mLastContentIsComplete are <i>undefined</i>
 * for an empty frame.
 *
 * <h3>Next-In-Flow List</h3>
 * The rule is that if a container frame is not complete then the mFirstContentOffset,
 * mLastContentOffset, and mLastContentIsComplete of its next-in-flow frames must
 * <b>always</b> correct. This means that whenever you push/pull child frames from a
 * next-in-flow you <b>must</b> update that next-in-flow's state so that it is
 * consistent with the aforementioned rules for empty and non-empty frames.
 *
 * <h3>Pulling-Up Frames</h3>
 * In the processing of pulling-up child frames from a next-in-flow it's possible
 * to pull-up all the child frames from a next-in-flow thereby leaving the next-in-flow
 * empty. There are two cases to consider:
 * <ol>
 * <li>All of the child frames from all of the next-in-flow have been pulled-up.
 *     This means that all the next-in-flow frames are empty, the container being
 *     reflowed is complete, and the next-in-flows will be deleted.
 *
 *     In this case the next-in-flows' mFirstContentOffset is also undefined.
 * <li>Not all the child frames from all the next-in-flows have been pulled-up.
 *     This means the next-in-flow consists of one or more empty frames followed
 *     by one or more non-empty frames. Note that all the empty frames must be
 *     consecutive. This means it is illegal to have an empty frame followed by
 *     a non-empty frame, followed by an empty frame, followed by a non-empty frame.
 * </ol>
 *
 * <h3>Pseudo-Frames</h3>
 * As stated above, when pulling/pushing child frames from/to a next-in-flow the
 * state of the next-in-flow must be updated.
 *
 * If the next-in-flow is a pseudo-frame then not only does the next-in-flow's state
 * need to be updated, but the state of its geometric parent must be updated as well.
 * See member function PropagateContentOffsets() for details.
 *
 * This rule is applied recursively, so if the next-in-flow's geometric parent is
 * also a pseudo-frame then its geometric parent must be updated.
 */
class nsContainerFrame : public nsSplittableFrame
{
public:
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const;

  /**
   * Default implementation is to use the content delegate to create a new
   * frame. After the frame is created it uses PrepareContinuingFrame() to
   * set the content offsets, mLastContentOffset, and append the continuing
   * frame to the flow.
   */
  NS_IMETHOD CreateContinuingFrame(nsIPresContext*  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);
  NS_IMETHOD DidReflow(nsIPresContext& aPresContext,
                       nsDidReflowStatus aStatus);

  // Painting
  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);

  /**
   * Pass through the event to the correct child frame.
   * Return PR_TRUE if the event is consumed.
   */
  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext,
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus);

  NS_IMETHOD GetCursorAt(nsIPresContext& aPresContext,
                         const nsPoint&  aPoint,
                         nsIFrame**      aFrame,
                         PRInt32&        aCursor);

  // Child frame enumeration.
  // ChildAt() retruns null if the index is not in the range 0 .. ChildCount() - 1.
  // IndexOf() returns -1 if the frame is not in the child list.
  NS_IMETHOD  ChildCount(PRInt32& aChildCount) const;
  NS_IMETHOD  ChildAt(PRInt32 aIndex, nsIFrame*& aFrame) const;
  NS_IMETHOD  IndexOf(const nsIFrame* aChild, PRInt32& aIndex) const;
  NS_IMETHOD  FirstChild(nsIFrame*& aFirstChild) const;
  NS_IMETHOD  NextChild(const nsIFrame* aChild, nsIFrame*& aNextChild) const;
  NS_IMETHOD  PrevChild(const nsIFrame* aChild, nsIFrame*& aPrevChild) const;
  NS_IMETHOD  LastChild(nsIFrame*& aLastChild) const;

  // Access functions for starting and end content offsets. These reflect the
  // range of content mapped by the frame.
  //
  // If the container is empty (has no children) the last content offset is
  // undefined
  PRInt32     GetFirstContentOffset() const {return mFirstContentOffset;}
  void        SetFirstContentOffset(PRInt32 aOffset) {mFirstContentOffset = aOffset;}
  PRInt32     GetLastContentOffset() const {return mLastContentOffset;}
  void        SetLastContentOffset(PRInt32 aOffset) {mLastContentOffset = aOffset;}

  /** return PR_TRUE if the last mapped child is complete */
  PRBool      GetLastContentIsComplete() const {return mLastContentIsComplete;}
  /** set the state indicating whether the last mapped child is complete */
  void        SetLastContentIsComplete(PRBool aLIC) {mLastContentIsComplete = aLIC;}

  // Get the offset for the next child content, i.e. the child after the
  // last child that fit in us
  PRInt32     NextChildOffset() const;

  // Returns true if this frame is being used a pseudo frame
  PRBool      IsPseudoFrame() const;

  // Debugging
  NS_IMETHOD  List(FILE* out = stdout, PRInt32 aIndent = 0) const;
  NS_IMETHOD  ListTag(FILE* out = stdout) const;
  NS_IMETHOD  VerifyTree() const;

  /**
   * When aChild's content mapping offsets change and the child is a
   * pseudo-frame (meaning that it is mapping content on the behalf of its
   * geometric parent) then the geometric needs to have its content
   * mapping offsets updated. This method is used to do just that. The
   * object is responsible for updating its content mapping offsets and
   * then if it is a pseudo-frame propogating the update upwards to its
   * parent.
   */
  virtual void PropagateContentOffsets(nsIFrame* aChild,
                                       PRInt32 aFirstContentOffset,
                                       PRInt32 aLastContentOffset,
                                       PRBool aLastContentIsComplete);

protected:
  // Constructor. Takes as arguments the content object, the index in parent,
  // and the Frame for the content parent
  nsContainerFrame(nsIContent* aContent, nsIFrame* aParent);

  virtual ~nsContainerFrame();

  void SizeOfWithoutThis(nsISizeOfHandler* aHandler) const;

  /**
   * Prepare a continuation frame of this frame for reflow. Appends
   * it to the flow, sets its content offsets, mLastContentIsComplete,
   * and style context.  Subclasses should invoke this method after
   * construction of a continuing frame.
   */
  void PrepareContinuingFrame(nsIPresContext*   aPresContext,
                              nsIFrame*         aParent,
                              nsIStyleContext*  aStyleContext,
                              nsContainerFrame* aContFrame);


  virtual void  PaintChildren(nsIPresContext&      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect&        aDirtyRect);

  /**
   * Reflow a child frame and return the status of the reflow. If the child
   * is complete and it has next-in-flows, then delete the next-in-flows.
   */
  nsReflowStatus ReflowChild(nsIFrame*            aKidFrame,
                             nsIPresContext*      aPresContext,
                             nsReflowMetrics&     aDesiredSize,
                             const nsReflowState& aReflowState);

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
   * Remove and delete aChild's next-in-flow(s). Updates the sibling and flow
   * pointers.
   *
   * Updates the child count and content offsets of all containers that are
   * affected.
   *
   * @param   aChild child this child's next-in-flow
   */
  PRBool  DeleteChildsNextInFlow(nsIFrame* aChild);

 /**
  * Push aFromChild and its next siblings to the next-in-flow. Change the
  * geometric parent of each frame that's pushed. If there is no next-in-flow
  * the frames are placed on the overflow list (and the geometric parent is
  * left unchanged).
  *
  * Updates the next-in-flow's child count and content offsets. Does
  * <b>not</b> update the pusher's child count or last content offset.
  *
  * @param   aFromChild the first child frame to push. It is disconnected from
  *            aPrevSibling
  * @param   aPrevSibling aFromChild's previous sibling. Must not be null. It's
  *            an error to push a parent's first child frame
  * @param   aNextInFlowsLastChildIsComplete the next-in-flow's
  *            mLastContentIsComplete flag. This is used when refilling an
  *            empty next-in-flow that was drained by the caller.
  */
  void PushChildren(nsIFrame* aFromChild, nsIFrame* aPrevSibling,
                    PRBool aNextInFlowsLastChildIsComplete);

  /**
   * Called after pulling-up children from the next-in-flow. Adjusts the first
   * content offset of all the empty next-in-flows
   *
   * It's an error to call this function if all of the next-in-flow frames
   * are empty.
   */
  void AdjustOffsetOfEmptyNextInFlows();

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

  // Returns true if aChild is being used as a pseudo frame
  PRBool ChildIsPseudoFrame(const nsIFrame* aChild) const;

  /**
   * Sets the first content offset based on the first child frame.
   */
  void SetFirstContentOffset(const nsIFrame* aFirstChild);

  /**
   * Sets the last content offset based on the last child frame. If the last
   * child is a pseudo frame then it sets mLastContentIsComplete to be the same
   * as the last child's mLastContentIsComplete
   */
  void SetLastContentOffset(const nsIFrame* aLastChild);

  virtual void WillDeleteNextInFlowFrame(nsIFrame* aNextInFlow);

#ifdef NS_DEBUG
  /**
   * Return the number of children in the sibling list, starting at aChild.
   * Returns zero if aChild is nsnull.
   */
  static PRInt32 LengthOf(nsIFrame* aChild);

  /**
   * Returns PR_TRUE if aChild is a child of this frame.
   */
  PRBool IsChild(const nsIFrame* aChild) const;

  /**
   * Returns PR_TRUE if aChild is the last child of this frame.
   */
  PRBool IsLastChild(const nsIFrame* aChild) const;

  void DumpTree() const;

  /**
   * Before reflow for this container has started we check that all
   * is well.
   */
  void PreReflowCheck();

  /**
   * After reflow for this container has finished we check that all
   * is well.
   *
   * @param aStatus status to be returned from the resize reflow. If the status
   *          is frNotComplete then the next-in-flow content offsets are
   *          validated as well
   */
  void PostReflowCheck(nsReflowStatus aStatus);

  void CheckNextInFlowOffsets();

  PRBool IsEmpty();

  PRBool SafeToCheckLastContentOffset(nsContainerFrame* nextInFlow);

  /**
   * Verify that our mLastContentIsComplete flag is set correctly.
   */
  void VerifyLastIsComplete() const;
#endif

  nsIFrame*   mFirstChild;
  PRInt32     mChildCount;
  PRInt32     mFirstContentOffset; // index of first child we map
  PRInt32     mLastContentOffset;  // index of last child we map
  PRBool      mLastContentIsComplete;
  nsIFrame*   mOverflowList;

private:
#ifdef NS_DEBUG
  /**
   * Helper function to verify that the first/last content offsets
   * are correct.  Note: this call will blow up if you have no
   * children.
   */
  void CheckContentOffsets();
#endif
};

#endif /* nsContainerFrame_h___ */
