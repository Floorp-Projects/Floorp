/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class #1 for rendering objects that have child lists */

#ifndef nsContainerFrame_h___
#define nsContainerFrame_h___

#include "nsSplittableFrame.h"
#include "nsFrameList.h"
#include "nsLayoutUtils.h"
#include "nsAutoPtr.h"

// Option flags for ReflowChild() and FinishReflowChild()
// member functions
#define NS_FRAME_NO_MOVE_VIEW         0x0001
#define NS_FRAME_NO_MOVE_FRAME        (0x0002 | NS_FRAME_NO_MOVE_VIEW)
#define NS_FRAME_NO_SIZE_VIEW         0x0004
#define NS_FRAME_NO_VISIBILITY        0x0008
// Only applies to ReflowChild: if true, invalidate the child if it's
// being moved
#define NS_FRAME_INVALIDATE_ON_MOVE   0x0010 

class nsOverflowContinuationTracker;

// Some macros for container classes to do sanity checking on
// width/height/x/y values computed during reflow.
// NOTE: AppUnitsPerCSSPixel value hardwired here to remove the
// dependency on nsDeviceContext.h.  It doesn't matter if it's a
// little off.
#ifdef DEBUG
#define CRAZY_W (1000000*60)
#define CRAZY_H CRAZY_W

#define CRAZY_WIDTH(_x) (((_x) < -CRAZY_W) || ((_x) > CRAZY_W))
#define CRAZY_HEIGHT(_y) (((_y) < -CRAZY_H) || ((_y) > CRAZY_H))
#endif

/**
 * Implementation of a container frame.
 */
class nsContainerFrame : public nsSplittableFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsContainerFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
  NS_IMETHOD Init(nsIContent* aContent,
                  nsIFrame*   aParent,
                  nsIFrame*   aPrevInFlow);
  NS_IMETHOD SetInitialChildList(ChildListID  aListID,
                                 nsFrameList& aChildList);
  NS_IMETHOD AppendFrames(ChildListID  aListID,
                          nsFrameList& aFrameList);
  NS_IMETHOD InsertFrames(ChildListID aListID,
                          nsIFrame* aPrevFrame,
                          nsFrameList& aFrameList);
  NS_IMETHOD RemoveFrame(ChildListID aListID,
                         nsIFrame* aOldFrame);

  virtual const nsFrameList& GetChildList(ChildListID aList) const;
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const;
  virtual void DestroyFrom(nsIFrame* aDestructRoot);
  virtual void ChildIsDirty(nsIFrame* aChild);

  virtual bool IsLeaf() const;
  virtual bool PeekOffsetNoAmount(bool aForward, PRInt32* aOffset);
  virtual bool PeekOffsetCharacter(bool aForward, PRInt32* aOffset,
                                     bool aRespectClusters = true);
  
#ifdef DEBUG
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
#endif  

  // nsContainerFrame methods

  /**
   * Helper method to create next-in-flows if necessary. If aFrame
   * already has a next-in-flow then this method does
   * nothing. Otherwise, a new continuation frame is created and
   * linked into the flow. In addition, the new frame is inserted
   * into the principal child list after aFrame.
   * @note calling this method on a block frame is illegal. Use
   * nsBlockFrame::CreateContinuationFor() instead.
   * @param aNextInFlowResult will contain the next-in-flow
   *        <b>if and only if</b> one is created. If a next-in-flow already
   *        exists aNextInFlowResult is set to nsnull.
   * @return NS_OK if a next-in-flow already exists or is successfully created.
   */
  nsresult CreateNextInFlow(nsPresContext* aPresContext,
                            nsIFrame*       aFrame,
                            nsIFrame*&      aNextInFlowResult);

  /**
   * Delete aNextInFlow and its next-in-flows.
   * @param aDeletingEmptyFrames if set, then the reflow for aNextInFlow's
   * content was complete before aNextInFlow, so aNextInFlow and its
   * next-in-flows no longer map any real content.
   */
  virtual void DeleteNextInFlowChild(nsPresContext* aPresContext,
                                     nsIFrame*      aNextInFlow,
                                     bool           aDeletingEmptyFrames);

  /**
   * Helper method to wrap views around frames. Used by containers
   * under special circumstances (can be used by leaf frames as well)
   */
  static nsresult CreateViewForFrame(nsIFrame* aFrame,
                                     bool aForce);

  // Positions the frame's view based on the frame's origin
  static void PositionFrameView(nsIFrame* aKidFrame);

  static nsresult ReparentFrameView(nsPresContext* aPresContext,
                                    nsIFrame*       aChildFrame,
                                    nsIFrame*       aOldParentFrame,
                                    nsIFrame*       aNewParentFrame);

  static nsresult ReparentFrameViewList(nsPresContext*     aPresContext,
                                        const nsFrameList& aChildFrameList,
                                        nsIFrame*          aOldParentFrame,
                                        nsIFrame*          aNewParentFrame);

  // Set the view's size and position after its frame has been reflowed.
  //
  // Flags:
  // NS_FRAME_NO_MOVE_VIEW - don't position the frame's view. Set this if you
  //    don't want to automatically sync the frame and view
  // NS_FRAME_NO_SIZE_VIEW - don't size the view
  static void SyncFrameViewAfterReflow(nsPresContext* aPresContext,
                                       nsIFrame*       aFrame,
                                       nsIView*        aView,
                                       const nsRect&   aVisualOverflowArea,
                                       PRUint32        aFlags = 0);

  // Syncs properties to the top level view and window, like transparency and
  // shadow.
  static void SyncWindowProperties(nsPresContext*       aPresContext,
                                   nsIFrame*            aFrame,
                                   nsIView*             aView);

  // Sets the view's attributes from the frame style.
  // - visibility
  // - clip
  // Call this when one of these styles changes or when the view has just
  // been created.
  // @param aStyleContext can be null, in which case the frame's style context is used
  static void SyncFrameViewProperties(nsPresContext*  aPresContext,
                                      nsIFrame*        aFrame,
                                      nsStyleContext*  aStyleContext,
                                      nsIView*         aView,
                                      PRUint32         aFlags = 0);

  // Used by both nsInlineFrame and nsFirstLetterFrame.
  void DoInlineIntrinsicWidth(nsRenderingContext *aRenderingContext,
                              InlineIntrinsicWidthData *aData,
                              nsLayoutUtils::IntrinsicWidthType aType);

  /**
   * This is the CSS block concept of computing 'auto' widths, which most
   * classes derived from nsContainerFrame want.
   */
  virtual nsSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, bool aShrinkWrap);

  /**
   * Invokes the WillReflow() function, positions the frame and its view (if
   * requested), and then calls Reflow(). If the reflow succeeds and the child
   * frame is complete, deletes any next-in-flows using DeleteNextInFlowChild()
   *
   * Flags:
   * NS_FRAME_NO_MOVE_VIEW - don't position the frame's view. Set this if you
   *    don't want to automatically sync the frame and view
   * NS_FRAME_NO_MOVE_FRAME - don't move the frame. aX and aY are ignored in this
   *    case. Also implies NS_FRAME_NO_MOVE_VIEW
   */
  nsresult ReflowChild(nsIFrame*                      aKidFrame,
                       nsPresContext*                 aPresContext,
                       nsHTMLReflowMetrics&           aDesiredSize,
                       const nsHTMLReflowState&       aReflowState,
                       nscoord                        aX,
                       nscoord                        aY,
                       PRUint32                       aFlags,
                       nsReflowStatus&                aStatus,
                       nsOverflowContinuationTracker* aTracker = nsnull);

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
  static nsresult FinishReflowChild(nsIFrame*                  aKidFrame,
                                    nsPresContext*             aPresContext,
                                    const nsHTMLReflowState*   aReflowState,
                                    const nsHTMLReflowMetrics& aDesiredSize,
                                    nscoord                    aX,
                                    nscoord                    aY,
                                    PRUint32                   aFlags);

  
  static void PositionChildViews(nsIFrame* aFrame);

  // ==========================================================================
  /* Overflow containers are continuation frames that hold overflow. They
   * are created when the frame runs out of computed height, but still has
   * too much content to fit in the availableHeight. The parent creates a
   * continuation as usual, but marks it as NS_FRAME_IS_OVERFLOW_CONTAINER
   * and adds it to its next-in-flow's overflow container list, either by
   * adding it directly or by putting it in its own excess overflow containers
   * list (to be drained by the next-in-flow when it calls
   * ReflowOverflowContainerChildren). The parent continues reflow as if
   * the frame was complete once it ran out of computed height, but returns
   * either an NS_FRAME_NOT_COMPLETE or NS_FRAME_OVERFLOW_INCOMPLETE reflow
   * status to request a next-in-flow. The parent's next-in-flow is then
   * responsible for calling ReflowOverflowContainerChildren to (drain and)
   * reflow these overflow continuations. Overflow containers do not affect
   * other frames' size or position during reflow (but do affect their
   * parent's overflow area).
   *
   * Overflow container continuations are different from normal continuations
   * in that
   *   - more than one child of the frame can have its next-in-flow broken
   *     off and pushed into the frame's next-in-flow
   *   - new continuations may need to be spliced into the middle of the list
   *     or deleted continuations slipped out
   *     e.g. A, B, C are all fixed-size containers on one page, all have
   *      overflow beyond availableHeight, and content is dynamically added
   *      and removed from B
   * As a result, it is not possible to simply prepend the new continuations
   * to the old list as with the overflowProperty mechanism. To avoid
   * complicated list splicing, the code assumes only one overflow containers
   * list exists for a given frame: either its own overflowContainersProperty
   * or its prev-in-flow's excessOverflowContainersProperty, not both.
   *
   * The nsOverflowContinuationTracker helper class should be used for tracking
   * overflow containers and adding them to the appropriate list.
   * See nsBlockFrame::Reflow for a sample implementation.
   */

  friend class nsOverflowContinuationTracker;

  /**
   * Reflow overflow container children. They are invisible to normal reflow
   * (i.e. don't affect sizing or placement of other children) and inherit
   * width and horizontal position from their prev-in-flow.
   *
   * This method
   *   1. Pulls excess overflow containers from the prev-in-flow and adds
   *      them to our overflow container list
   *   2. Reflows all our overflow container kids
   *   3. Expands aOverflowRect as necessary to accomodate these children.
   *   4. Sets aStatus's NS_FRAME_OVERFLOW_IS_INCOMPLETE flag (along with
   *      NS_FRAME_REFLOW_NEXTINFLOW as necessary) if any overflow children
   *      are incomplete and
   *   5. Prepends a list of their continuations to our excess overflow
   *      container list, to be drained into our next-in-flow when it is
   *      reflowed.
   *
   * The caller is responsible for tracking any new overflow container
   * continuations it makes, removing them from its child list, and
   * making sure they are stored properly in the overflow container lists.
   * The nsOverflowContinuationTracker helper class should be used for this.
   *
   * (aFlags just gets passed through to ReflowChild)
   */
  nsresult ReflowOverflowContainerChildren(nsPresContext*           aPresContext,
                                           const nsHTMLReflowState& aReflowState,
                                           nsOverflowAreas&         aOverflowRects,
                                           PRUint32                 aFlags,
                                           nsReflowStatus&          aStatus);

  /**
   * Removes aChild without destroying it and without requesting reflow.
   * Continuations are not affected. Checks the primary and overflow
   * or overflow containers and excess overflow containers lists, depending
   * on whether the NS_FRAME_IS_OVERFLOW_CONTAINER flag is set. Does not
   * check any other auxiliary lists.
   * Returns NS_ERROR_UNEXPECTED if we failed to remove aChild.
   * Returns other error codes if we failed to put back a proptable list.
   * If aForceNormal is true, only checks the primary and overflow lists
   * even when the NS_FRAME_IS_OVERFLOW_CONTAINER flag is set.
   */
  virtual nsresult StealFrame(nsPresContext* aPresContext,
                              nsIFrame*      aChild,
                              bool           aForceNormal = false);

  /**
   * Removes the next-siblings of aChild without destroying them and without
   * requesting reflow. Checks the principal and overflow lists (not
   * overflow containers / excess overflow containers). Does not check any
   * other auxiliary lists.
   * @param aChild a child frame or nsnull
   * @return If aChild is non-null, the next-siblings of aChild, if any.
   *         If aChild is null, all child frames on the principal list, if any.
   */
  nsFrameList StealFramesAfter(nsIFrame* aChild);

  /**
   * Add overflow containers to the display list
   */
  void DisplayOverflowContainers(nsDisplayListBuilder*   aBuilder,
                                 const nsRect&           aDirtyRect,
                                 const nsDisplayListSet& aLists);

  /**
   * Builds display lists for the children. The background
   * of each child is placed in the Content() list (suitable for inline
   * children and other elements that behave like inlines,
   * but not for in-flow block children of blocks).  DOES NOT
   * paint the background/borders/outline of this frame. This should
   * probably be avoided and eventually removed. It's currently here
   * to emulate what nsContainerFrame::Paint did.
   */
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  // Destructor function for the proptable-stored framelists
  static void DestroyFrameList(void* aPropertyValue)
  {
    if (aPropertyValue) {
      static_cast<nsFrameList*>(aPropertyValue)->Destroy();
    }
  }

  NS_DECLARE_FRAME_PROPERTY(OverflowProperty, DestroyFrameList)
  NS_DECLARE_FRAME_PROPERTY(OverflowContainersProperty, DestroyFrameList)
  NS_DECLARE_FRAME_PROPERTY(ExcessOverflowContainersProperty, DestroyFrameList)

protected:
  nsContainerFrame(nsStyleContext* aContext) : nsSplittableFrame(aContext) {}
  ~nsContainerFrame();

  /**
   * Builds a display list for non-block children that behave like
   * inlines. This puts the background of each child into the
   * Content() list (suitable for inline children but not for
   * in-flow block children of blocks).
   * @param aForcePseudoStack forces each child into a pseudo-stacking-context
   * so its background and all other display items (except for positioned
   * display items) go into the Content() list.
   */
  nsresult BuildDisplayListForNonBlockChildren(nsDisplayListBuilder*   aBuilder,
                                               const nsRect&           aDirtyRect,
                                               const nsDisplayListSet& aLists,
                                               PRUint32                aFlags = 0);

  /**
   * A version of BuildDisplayList that use DISPLAY_CHILD_INLINE.
   * Intended as a convenience for derived classes.
   */
  nsresult BuildDisplayListForInline(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists) {
    nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = BuildDisplayListForNonBlockChildren(aBuilder, aDirtyRect, aLists,
                                             DISPLAY_CHILD_INLINE);
    NS_ENSURE_SUCCESS(rv, rv);
    return rv;
  }


  // ==========================================================================
  /* Overflow Frames are frames that did not fit and must be pulled by
   * our next-in-flow during its reflow. (The same concept for overflow
   * containers is called "excess frames". We should probably make the
   * names match.)
   */

  /**
   * Get the frames on the overflow list.  Can return null if there are no
   * overflow frames.  The caller does NOT take ownership of the list; it's
   * still owned by this frame.  A non-null return value indicates that the
   * list is nonempty.
   */
  inline nsFrameList* GetOverflowFrames() const;

  /**
   * As GetOverflowFrames, but removes the overflow frames property.  The
   * caller is responsible for deleting nsFrameList and either passing
   * ownership of the frames to someone else or destroying the frames.  A
   * non-null return value indicates that the list is nonempty.  The
   * recommended way to use this function it to assign its return value
   * into an nsAutoPtr.
   */
  inline nsFrameList* StealOverflowFrames();
  
  /**
   * Set the overflow list.  aOverflowFrames must not be an empty list.
   */
  void SetOverflowFrames(nsPresContext*  aPresContext,
                         const nsFrameList& aOverflowFrames);

  /**
   * Destroy the overflow list and any frames that are on it.
   * Calls DestructFrom() insead of Destruct() on the frames if
   * aDestructRoot is non-null.
   */
  void DestroyOverflowList(nsPresContext* aPresContext,
                           nsIFrame*      aDestructRoot);

  /**
   * Moves any frames on both the prev-in-flow's overflow list and the
   * receiver's overflow to the receiver's child list.
   *
   * Resets the overlist pointers to nsnull, and updates the receiver's child
   * count and content mapping.
   *
   * @return true if any frames were moved and false otherwise
   */
  bool MoveOverflowToChildList(nsPresContext* aPresContext);

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
  void PushChildren(nsPresContext*  aPresContext,
                    nsIFrame*       aFromChild,
                    nsIFrame*       aPrevSibling);

  // ==========================================================================
  /*
   * Convenience methods for nsFrameLists stored in the
   * PresContext's proptable
   */

  /**
   * Get the PresContext-stored nsFrameList named aPropID for this frame.
   * May return null.
   */
  nsFrameList* GetPropTableFrames(nsPresContext*                 aPresContext,
                                  const FramePropertyDescriptor* aProperty) const;

  /**
   * Remove and return the PresContext-stored nsFrameList named aPropID for
   * this frame. May return null.
   */
  nsFrameList* RemovePropTableFrames(nsPresContext*                 aPresContext,
                                     const FramePropertyDescriptor* aProperty);

  /**
   * Remove aFrame from the PresContext-stored nsFrameList named aPropID
   * for this frame, deleting the list if it is now empty.
   * Return true if the aFrame was successfully removed,
   * Return false otherwise.
   */
  bool RemovePropTableFrame(nsPresContext*                 aPresContext,
                              nsIFrame*                      aFrame,
                              const FramePropertyDescriptor* aProperty);

  /**
   * Set the PresContext-stored nsFrameList named aPropID for this frame
   * to the given aFrameList, which must not be null.
   */
  nsresult SetPropTableFrames(nsPresContext*                 aPresContext,
                              nsFrameList*                   aFrameList,
                              const FramePropertyDescriptor* aProperty);
  // ==========================================================================

  nsFrameList mFrames;
};

// ==========================================================================
/* The out-of-flow-related code below is for a hacky way of splitting
 * absolutely-positioned frames. Basically what we do is split the frame
 * in nsAbsoluteContainingBlock and pretend the continuation is an overflow
 * container. This isn't an ideal solution, but it lets us print the content
 * at least. See bug 154892.
 */

#define IS_TRUE_OVERFLOW_CONTAINER(frame)                      \
  (  (frame->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)  \
  && !( (frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&      \
        frame->GetStyleDisplay()->IsAbsolutelyPositioned()  )  )
//XXXfr This check isn't quite correct, because it doesn't handle cases
//      where the out-of-flow has overflow.. but that's rare.
//      We'll need to revisit the way abspos continuations are handled later
//      for various reasons, this detail is one of them. See bug 154892

/**
 * Helper class for tracking overflow container continuations during reflow.
 *
 * A frame is related to two sets of overflow containers: those that /are/
 * its own children, and those that are /continuations/ of its children.
 * This tracker walks through those continuations (the frame's NIF's children)
 * and their prev-in-flows (a subset of the frame's normal and overflow
 * container children) in parallel. It allows the reflower to synchronously
 * walk its overflow continuations while it loops through and reflows its
 * children. This makes it possible to insert new continuations at the correct
 * place in the overflow containers list.
 *
 * The reflower is expected to loop through its children in the same order it
 * looped through them the last time (if there was a last time).
 * For each child, the reflower should either
 *   - call Skip for the child if was not reflowed in this pass
 *   - call Insert for the overflow continuation if the child was reflowed
 *     but has incomplete overflow
 *   - call Finished for the child if it was reflowed in this pass but
 *     is either complete or has a normal next-in-flow. This call can
 *     be skipped if the child did not previously have an overflow
 *     continuation.
 */
class nsOverflowContinuationTracker {
public:
  /**
   * Initializes an nsOverflowContinuationTracker to help track overflow
   * continuations of aFrame's children. Typically invoked on 'this'.
   *
   * aWalkOOFFrames determines whether the walker skips out-of-flow frames
   * or skips non-out-of-flow frames.
   *
   * Don't set aSkipOverflowContainerChildren to false unless you plan
   * to walk your own overflow container children. (Usually they are handled
   * by calling ReflowOverflowContainerChildren.) aWalkOOFFrames is ignored
   * if aSkipOverflowContainerChildren is false.
   */
  nsOverflowContinuationTracker(nsPresContext*    aPresContext,
                                nsContainerFrame* aFrame,
                                bool              aWalkOOFFrames,
                                bool              aSkipOverflowContainerChildren = true);
  /**
   * This function adds an overflow continuation to our running list and
   * sets its NS_FRAME_IS_OVERFLOW_CONTAINER flag.
   *
   * aReflowStatus should preferably be specific to the recently-reflowed
   * child and not influenced by any of its siblings' statuses. This
   * function sets the NS_FRAME_IS_DIRTY bit on aOverflowCont if it needs
   * to be reflowed. (Its need for reflow depends on changes to its
   * prev-in-flow, not to its parent--for whom it is invisible, reflow-wise.)
   *
   * The caller MUST disconnect the frame from its parent's child list
   * if it was not previously an NS_FRAME_IS_OVERFLOW_CONTAINER (because
   * StealFrame is much more inefficient than disconnecting in place
   * during Reflow, which the caller is able to do but we are not).
   *
   * The caller MUST NOT disconnect the frame from its parent's
   * child list if it is already an NS_FRAME_IS_OVERFLOW_CONTAINER.
   * (In this case we will disconnect and reconnect it ourselves.)
   */
  nsresult Insert(nsIFrame*       aOverflowCont,
                  nsReflowStatus& aReflowStatus);
  /**
   * This function must be called for each child that is reflowed
   * but no longer has an overflow continuation. (It may be called for
   * other children, but in that case has no effect.) It increments our
   * walker and makes sure we drop any dangling pointers to its
   * next-in-flow. This function MUST be called before stealing or
   * deleting aChild's next-in-flow.
   */
  void Finish(nsIFrame* aChild);

  /**
   * This function should be called for each child that isn't reflowed.
   * It increments our walker and sets the NS_FRAME_OVERFLOW_INCOMPLETE
   * reflow flag if it encounters an overflow continuation so that our
   * next-in-flow doesn't get prematurely deleted. It MUST be called on
   * each unreflowed child that has an overflow container continuation;
   * it MAY be called on other children, but it isn't necessary (doesn't
   * do anything).
   */
  void Skip(nsIFrame* aChild, nsReflowStatus& aReflowStatus)
  {
    NS_PRECONDITION(aChild, "null ptr");
    if (aChild == mSentry) {
      StepForward();
      NS_MergeReflowStatusInto(&aReflowStatus, NS_FRAME_OVERFLOW_INCOMPLETE);
    }
  }

private:

  void SetUpListWalker();
  void StepForward();

  /* We hold a pointer to either the next-in-flow's overflow containers list
     or, if that doesn't exist, our frame's excess overflow containers list.
     We need to make sure that we drop that pointer if the list becomes
     empty and is deleted elsewhere. */
  nsFrameList* mOverflowContList;
  /* We hold a pointer to the most recently-reflowed child that has an
     overflow container next-in-flow. We do this because it's a known
     good point; this pointer won't be deleted on us. We can use it to
     recover our place in the list. */
  nsIFrame* mPrevOverflowCont;
  /* This is a pointer to the next overflow container's prev-in-flow, which
     is (or should be) a child of our frame. When we hit this, we will need
     to increment this walker to the next overflow container. */
  nsIFrame* mSentry;
  /* Parent of all frames in mOverflowContList. If our mOverflowContList
     is an excessOverflowContainersProperty, or null, then this is our frame
     (the frame that was passed in to our constructor). Otherwise this is
     that frame's next-in-flow, and our mOverflowContList is mParent's
     overflowContainersProperty */
  nsContainerFrame* mParent;
  /* Tells SetUpListWalker whether or not to walk us past any continuations
     of overflow containers. aWalkOOFFrames is ignored when this is false. */
  bool mSkipOverflowContainerChildren;
  /* Tells us whether to pay attention to OOF frames or non-OOF frames */
  bool mWalkOOFFrames;
};

inline
nsFrameList*
nsContainerFrame::GetOverflowFrames() const
{
  nsFrameList* list =
    static_cast<nsFrameList*>(Properties().Get(OverflowProperty()));
  NS_ASSERTION(!list || !list->IsEmpty(), "Unexpected empty overflow list");
  return list;
}

inline
nsFrameList*
nsContainerFrame::StealOverflowFrames()
{
  nsFrameList* list =
    static_cast<nsFrameList*>(Properties().Remove(OverflowProperty()));
  NS_ASSERTION(!list || !list->IsEmpty(), "Unexpected empty overflow list");
  return list;
}

#endif /* nsContainerFrame_h___ */
