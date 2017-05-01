/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class #1 for rendering objects that have child lists */

#ifndef nsContainerFrame_h___
#define nsContainerFrame_h___

#include "mozilla/Attributes.h"
#include "nsSplittableFrame.h"
#include "nsFrameList.h"
#include "nsLayoutUtils.h"

// Option flags for ReflowChild() and FinishReflowChild()
// member functions
#define NS_FRAME_NO_MOVE_VIEW         0x0001
#define NS_FRAME_NO_MOVE_FRAME        (0x0002 | NS_FRAME_NO_MOVE_VIEW)
#define NS_FRAME_NO_SIZE_VIEW         0x0004
#define NS_FRAME_NO_VISIBILITY        0x0008
// Only applies to ReflowChild; if true, don't delete the next-in-flow, even
// if the reflow is fully complete.
#define NS_FRAME_NO_DELETE_NEXT_IN_FLOW_CHILD 0x0010

class nsOverflowContinuationTracker;
namespace mozilla {
class FramePropertyTable;
} // namespace mozilla

// Some macros for container classes to do sanity checking on
// width/height/x/y values computed during reflow.
// NOTE: AppUnitsPerCSSPixel value hardwired here to remove the
// dependency on nsDeviceContext.h.  It doesn't matter if it's a
// little off.
#ifdef DEBUG
// 10 million pixels, converted to app units. Note that this a bit larger
// than 1/4 of nscoord_MAX. So, if any content gets to be this large, we're
// definitely in danger of grazing up against nscoord_MAX; hence, it's CRAZY.
#define CRAZY_COORD (10000000*60)
#define CRAZY_SIZE(_x) (((_x) < -CRAZY_COORD) || ((_x) > CRAZY_COORD))
#endif

/**
 * Implementation of a container frame.
 */
class nsContainerFrame : public nsSplittableFrame
{
public:
  NS_DECL_ABSTRACT_FRAME(nsContainerFrame)
  NS_DECL_QUERYFRAME_TARGET(nsContainerFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
  virtual nsContainerFrame* GetContentInsertionFrame() override
  {
    return this;
  }

  virtual const nsFrameList& GetChildList(ChildListID aList) const override;
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const override;
  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;
  virtual void ChildIsDirty(nsIFrame* aChild) override;

  virtual bool IsLeaf() const override;
  virtual FrameSearchResult PeekOffsetNoAmount(bool aForward, int32_t* aOffset) override;
  virtual FrameSearchResult PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                                     bool aRespectClusters = true) override;

  virtual nsresult AttributeChanged(int32_t         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    int32_t         aModType) override;

#ifdef DEBUG_FRAME_DUMP
  void List(FILE* out = stderr, const char* aPrefix = "", uint32_t aFlags = 0) const override;
#endif  

  // nsContainerFrame methods

  /**
   * Called to set the initial list of frames. This happens after the frame
   * has been initialized.
   *
   * This is only called once for a given child list, and won't be called
   * at all for child lists with no initial list of frames.
   *
   * @param   aListID the child list identifier.
   * @param   aChildList list of child frames. Each of the frames has its
   *            NS_FRAME_IS_DIRTY bit set.  Must not be empty.
   *            This method cannot handle the child list returned by
   *            GetAbsoluteListID().
   * @see     #Init()
   */
  virtual void SetInitialChildList(ChildListID aListID,
                                   nsFrameList& aChildList);

  /**
   * This method is responsible for appending frames to the frame
   * list.  The implementation should append the frames to the specified
   * child list and then generate a reflow command.
   *
   * @param   aListID the child list identifier.
   * @param   aFrameList list of child frames to append. Each of the frames has
   *            its NS_FRAME_IS_DIRTY bit set.  Must not be empty.
   */
  virtual void AppendFrames(ChildListID aListID, nsFrameList& aFrameList);

  /**
   * This method is responsible for inserting frames into the frame
   * list.  The implementation should insert the new frames into the specified
   * child list and then generate a reflow command.
   *
   * @param   aListID the child list identifier.
   * @param   aPrevFrame the frame to insert frames <b>after</b>
   * @param   aFrameList list of child frames to insert <b>after</b> aPrevFrame.
   *            Each of the frames has its NS_FRAME_IS_DIRTY bit set
   */
  virtual void InsertFrames(ChildListID  aListID,
                            nsIFrame*    aPrevFrame,
                            nsFrameList& aFrameList);

  /**
   * This method is responsible for removing a frame in the frame
   * list.  The implementation should do something with the removed frame
   * and then generate a reflow command. The implementation is responsible
   * for destroying aOldFrame (the caller mustn't destroy aOldFrame).
   *
   * @param   aListID the child list identifier.
   * @param   aOldFrame the frame to remove
   */
  virtual void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame);

  /**
   * Helper method to create next-in-flows if necessary. If aFrame
   * already has a next-in-flow then this method does
   * nothing. Otherwise, a new continuation frame is created and
   * linked into the flow. In addition, the new frame is inserted
   * into the principal child list after aFrame.
   * @note calling this method on a block frame is illegal. Use
   * nsBlockFrame::CreateContinuationFor() instead.
   * @return the next-in-flow <b>if and only if</b> one is created. If
   *         a next-in-flow already exists, nullptr will be returned.
   */
  nsIFrame* CreateNextInFlow(nsIFrame* aFrame);

  /**
   * Delete aNextInFlow and its next-in-flows.
   * @param aDeletingEmptyFrames if set, then the reflow for aNextInFlow's
   * content was complete before aNextInFlow, so aNextInFlow and its
   * next-in-flows no longer map any real content.
   */
  virtual void DeleteNextInFlowChild(nsIFrame* aNextInFlow,
                                     bool      aDeletingEmptyFrames);

  // Positions the frame's view based on the frame's origin
  static void PositionFrameView(nsIFrame* aKidFrame);

  static nsresult ReparentFrameView(nsIFrame* aChildFrame,
                                    nsIFrame* aOldParentFrame,
                                    nsIFrame* aNewParentFrame);

  static nsresult ReparentFrameViewList(const nsFrameList& aChildFrameList,
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
                                       nsView*        aView,
                                       const nsRect&   aVisualOverflowArea,
                                       uint32_t        aFlags = 0);

  // Syncs properties to the top level view and window, like transparency and
  // shadow.
  // The SET_ASYNC indicates that the actual nsIWidget calls to sync the window
  // properties should be done async.
  enum {
    SET_ASYNC = 0x01,
  };
  static void SyncWindowProperties(nsPresContext*       aPresContext,
                                   nsIFrame*            aFrame,
                                   nsView*              aView,
                                   nsRenderingContext*  aRC,
                                   uint32_t             aFlags);

  /**
   * Converts the minimum and maximum sizes given in inner window app units to
   * outer window device pixel sizes and assigns these constraints to the widget.
   *
   * @param aPresContext pres context
   * @param aWidget widget for this frame
   * @param minimum size of the window in app units
   * @param maxmimum size of the window in app units
   */
  static void SetSizeConstraints(nsPresContext* aPresContext,
                                 nsIWidget* aWidget,
                                 const nsSize& aMinSize,
                                 const nsSize& aMaxSize);

  // Used by both nsInlineFrame and nsFirstLetterFrame.
  void DoInlineIntrinsicISize(nsRenderingContext *aRenderingContext,
                              InlineIntrinsicISizeData *aData,
                              nsLayoutUtils::IntrinsicISizeType aType);

  /**
   * This is the CSS block concept of computing 'auto' widths, which most
   * classes derived from nsContainerFrame want.
   */
  virtual mozilla::LogicalSize
  ComputeAutoSize(nsRenderingContext*         aRenderingContext,
                  mozilla::WritingMode        aWM,
                  const mozilla::LogicalSize& aCBSize,
                  nscoord                     aAvailableISize,
                  const mozilla::LogicalSize& aMargin,
                  const mozilla::LogicalSize& aBorder,
                  const mozilla::LogicalSize& aPadding,
                  ComputeSizeFlags            aFlags) override;

  /**
   * Positions aChildFrame and its view (if requested), and then calls Reflow().
   * If the reflow status after reflowing the child is FULLY_COMPLETE then any
   * next-in-flows are deleted using DeleteNextInFlowChild().
   *
   * @param aContainerSize  size of the border-box of the containing frame
   *
   * Flags:
   * NS_FRAME_NO_MOVE_VIEW - don't position the frame's view. Set this if you
   *    don't want to automatically sync the frame and view
   * NS_FRAME_NO_MOVE_FRAME - don't move the frame. aPos is ignored in this
   *    case. Also implies NS_FRAME_NO_MOVE_VIEW
   */
  void ReflowChild(nsIFrame*                      aChildFrame,
                   nsPresContext*                 aPresContext,
                   ReflowOutput&           aDesiredSize,
                   const ReflowInput&       aReflowInput,
                   const mozilla::WritingMode&    aWM,
                   const mozilla::LogicalPoint&   aPos,
                   const nsSize&                  aContainerSize,
                   uint32_t                       aFlags,
                   nsReflowStatus&                aStatus,
                   nsOverflowContinuationTracker* aTracker = nullptr);

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
   * @param aContainerSize  size of the border-box of the containing frame
   *
   * Flags:
   * NS_FRAME_NO_MOVE_FRAME - don't move the frame. aPos is ignored in this
   *    case. Also implies NS_FRAME_NO_MOVE_VIEW
   * NS_FRAME_NO_MOVE_VIEW - don't position the frame's view. Set this if you
   *    don't want to automatically sync the frame and view
   * NS_FRAME_NO_SIZE_VIEW - don't size the frame's view
   */
  static void FinishReflowChild(nsIFrame*                    aKidFrame,
                                nsPresContext*               aPresContext,
                                const ReflowOutput&   aDesiredSize,
                                const ReflowInput*     aReflowInput,
                                const mozilla::WritingMode&  aWM,
                                const mozilla::LogicalPoint& aPos,
                                const nsSize&                aContainerSize,
                                uint32_t                     aFlags);

  //XXX temporary: hold on to a copy of the old physical versions of
  //    ReflowChild and FinishReflowChild so that we can convert callers
  //    incrementally.
  void ReflowChild(nsIFrame*                      aKidFrame,
                   nsPresContext*                 aPresContext,
                   ReflowOutput&           aDesiredSize,
                   const ReflowInput&       aReflowInput,
                   nscoord                        aX,
                   nscoord                        aY,
                   uint32_t                       aFlags,
                   nsReflowStatus&                aStatus,
                   nsOverflowContinuationTracker* aTracker = nullptr);

  static void FinishReflowChild(nsIFrame*                  aKidFrame,
                                nsPresContext*             aPresContext,
                                const ReflowOutput& aDesiredSize,
                                const ReflowInput*   aReflowInput,
                                nscoord                    aX,
                                nscoord                    aY,
                                uint32_t                   aFlags);

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
   * the frame was complete once it ran out of computed height, but returns a
   * reflow status with either IsIncomplete() or IsOverflowIncomplete() equal
   * to true to request a next-in-flow. The parent's next-in-flow is then
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

  typedef void (*ChildFrameMerger)(nsFrameList& aDest, nsFrameList& aSrc,
                                   nsContainerFrame* aParent);
  static inline void DefaultChildFrameMerge(nsFrameList& aDest,
                                            nsFrameList& aSrc,
                                            nsContainerFrame* aParent)
  {
    aDest.AppendFrames(nullptr, aSrc);
  }

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
   *   4. Sets aStatus's mOverflowIncomplete flag (along with
   *      mNextInFlowNeedsReflow as necessary) if any overflow children
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
   * @param aFlags is passed through to ReflowChild
   * @param aMergeFunc is passed to DrainExcessOverflowContainersList
   */
  void ReflowOverflowContainerChildren(nsPresContext*           aPresContext,
                                       const ReflowInput& aReflowInput,
                                       nsOverflowAreas&         aOverflowRects,
                                       uint32_t                 aFlags,
                                       nsReflowStatus&          aStatus,
                                       ChildFrameMerger aMergeFunc =
                                         DefaultChildFrameMerge);

  /**
   * Move any frames on our overflow list to the end of our principal list.
   * @return true if there were any overflow frames
   */
  virtual bool DrainSelfOverflowList() override;

  
  /**
   * Move all frames on our prev-in-flow's and our own ExcessOverflowContainers
   * lists to our OverflowContainers list.  If there are frames on multiple
   * lists they are merged using aMergeFunc.
   * @return a pointer to our OverflowContainers list, if any
   */
  nsFrameList* DrainExcessOverflowContainersList(ChildFrameMerger aMergeFunc =
                                                   DefaultChildFrameMerge);

  /**
   * Removes aChild without destroying it and without requesting reflow.
   * Continuations are not affected.  Checks the principal and overflow lists,
   * and also the [excess] overflow containers lists if the frame bit
   * NS_FRAME_IS_OVERFLOW_CONTAINER is set.  It does not check any other lists.
   * Returns NS_ERROR_UNEXPECTED if aChild wasn't found on any of the lists
   * mentioned above.
   */
  virtual nsresult StealFrame(nsIFrame* aChild);

  /**
   * Removes the next-siblings of aChild without destroying them and without
   * requesting reflow. Checks the principal and overflow lists (not
   * overflow containers / excess overflow containers). Does not check any
   * other auxiliary lists.
   * @param aChild a child frame or nullptr
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
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  static void PlaceFrameView(nsIFrame* aFrame)
  {
    if (aFrame->HasView())
      nsContainerFrame::PositionFrameView(aFrame);
    else
      nsContainerFrame::PositionChildViews(aFrame);
  }

  static bool FrameStartsCounterScope(nsIFrame* aFrame);

  /**
   * Renumber the list of the counter scope started by this frame, if any.
   * If this returns true, the frame it's called on should get the
   * NS_FRAME_HAS_DIRTY_CHILDREN bit set on it by the caller; either directly
   * if it's already in reflow, or via calling FrameNeedsReflow() to schedule
   * a reflow.
   */
  bool RenumberList();

  /**
   * Renumber this frame if it's a list-item, then call RenumberChildFrames.
   * @param aOrdinal Ordinal number to start counting at.
   *        Modifies this number for each associated list
   *        item. Changes in the numbering due to setting
   *        the |value| attribute are included if |aForCounting|
   *        is false. This value is both an input and output
   *        of this function, with the output value being the
   *        next ordinal number to be used.
   * @param aDepth Current depth in frame tree from root list element.
   * @param aIncrement Amount to increase by after visiting each associated
   *        list item, unless overridden by |value|.
   * @param aForCounting Whether we are counting the elements or actually
   *        restyling them. When true, this simply visits all children,
   *        ignoring |<li value="..">| changes, effectively counting them
   *        and storing the result in |aOrdinal|. This is useful for
   *        |<ol reversed>|, where we need to count the number of
   *        applicable child list elements before numbering. When false,
   *        this will restyle all applicable descendants, and the next
   *        ordinal value will be stored in |aOrdinal|, taking into account
   *        any changes from |<li value="..">|.
   */
  bool RenumberFrameAndDescendants(int32_t* aOrdinal,
                                   int32_t aDepth,
                                   int32_t aIncrement,
                                   bool aForCounting) override;
  /**
   * Renumber the child frames using RenumberFrameAndDescendants.
   * See RenumberFrameAndDescendants for description of parameters.
   */
  virtual bool RenumberChildFrames(int32_t* aOrdinal,
                                   int32_t aDepth,
                                   int32_t aIncrement,
                                   bool aForCounting);

  /**
   * Returns a CSS Box Alignment constant which the caller can use to align
   * the absolutely-positioned child (whose ReflowInput is aChildRI) within
   * a CSS Box Alignment area associated with this container.
   *
   * The lower 8 bits of the returned value are guaranteed to form a valid
   * argument for CSSAlignUtils::AlignJustifySelf(). (The upper 8 bits may
   * encode an <overflow-position>.)
   *
   * NOTE: This default nsContainerFrame implementation is a stub, and isn't
   * meant to be called.  Subclasses must provide their own implementations, if
   * they use CSS Box Alignment to determine the static position of their
   * absolutely-positioned children. (Though: if subclasses share enough code,
   * maybe this nsContainerFrame impl should include some shared code.)
   *
   * @param aChildRI A ReflowInput for the positioned child frame that's being
   *                 aligned.
   * @param aLogicalAxis The axis (of this container frame) in which the caller
   *                     would like to align the child frame.
   */
  virtual uint16_t CSSAlignmentForAbsPosChild(
                     const ReflowInput& aChildRI,
                     mozilla::LogicalAxis aLogicalAxis) const;

#define NS_DECLARE_FRAME_PROPERTY_FRAMELIST(prop) \
  NS_DECLARE_FRAME_PROPERTY_WITH_DTOR_NEVER_CALLED(prop, nsFrameList)

  typedef PropertyDescriptor<nsFrameList> FrameListPropertyDescriptor;

  NS_DECLARE_FRAME_PROPERTY_FRAMELIST(OverflowProperty)
  NS_DECLARE_FRAME_PROPERTY_FRAMELIST(OverflowContainersProperty)
  NS_DECLARE_FRAME_PROPERTY_FRAMELIST(ExcessOverflowContainersProperty)
  NS_DECLARE_FRAME_PROPERTY_FRAMELIST(BackdropProperty)

#ifdef DEBUG
  // Use this to suppress the CRAZY_SIZE assertions.
  NS_DECLARE_FRAME_PROPERTY_SMALL_VALUE(DebugReflowingWithInfiniteISize, bool)
  bool IsCrazySizeAssertSuppressed() const {
    return Properties().Get(DebugReflowingWithInfiniteISize());
  }
#endif

protected:
  nsContainerFrame(nsStyleContext* aContext, mozilla::LayoutFrameType aType)
    : nsSplittableFrame(aContext, aType)
  {}

  ~nsContainerFrame();

  /**
   * Helper for DestroyFrom. DestroyAbsoluteFrames is called before
   * destroying frames on lists that can contain placeholders.
   * Derived classes must do that too, if they destroy such frame lists.
   * See nsBlockFrame::DestroyFrom for an example.
   */
  void DestroyAbsoluteFrames(nsIFrame* aDestructRoot);

  /**
   * Helper for StealFrame.  Returns true if aChild was removed from its list.
   */
  bool MaybeStealOverflowContainerFrame(nsIFrame* aChild);

  /**
   * Builds a display list for non-block children that behave like
   * inlines. This puts the background of each child into the
   * Content() list (suitable for inline children but not for
   * in-flow block children of blocks).
   * @param aForcePseudoStack forces each child into a pseudo-stacking-context
   * so its background and all other display items (except for positioned
   * display items) go into the Content() list.
   */
  void BuildDisplayListForNonBlockChildren(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists,
                                           uint32_t                aFlags = 0);

  /**
   * A version of BuildDisplayList that use DISPLAY_CHILD_INLINE.
   * Intended as a convenience for derived classes.
   */
  void BuildDisplayListForInline(nsDisplayListBuilder*   aBuilder,
                                 const nsRect&           aDirtyRect,
                                 const nsDisplayListSet& aLists) {
    DisplayBorderBackgroundOutline(aBuilder, aLists);
    BuildDisplayListForNonBlockChildren(aBuilder, aDirtyRect, aLists,
                                        DISPLAY_CHILD_INLINE);
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
   * ownership of the frames to someone else or destroying the frames.
   * A non-null return value indicates that the list is nonempty.  The
   * recommended way to use this function it to assign its return value
   * into an AutoFrameListPtr.
   */
  inline nsFrameList* StealOverflowFrames();
  
  /**
   * Set the overflow list.  aOverflowFrames must not be an empty list.
   */
  void SetOverflowFrames(const nsFrameList& aOverflowFrames);

  /**
   * Destroy the overflow list, which must be empty.
   */
  inline void DestroyOverflowList();

  /**
   * Moves any frames on both the prev-in-flow's overflow list and the
   * receiver's overflow to the receiver's child list.
   *
   * Resets the overlist pointers to nullptr, and updates the receiver's child
   * count and content mapping.
   *
   * @return true if any frames were moved and false otherwise
   */
  bool MoveOverflowToChildList();

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

  // ==========================================================================
  /*
   * Convenience methods for traversing continuations
   */

  struct ContinuationTraversingState
  {
    nsContainerFrame* mNextInFlow;
    explicit ContinuationTraversingState(nsContainerFrame* aFrame)
      : mNextInFlow(static_cast<nsContainerFrame*>(aFrame->GetNextInFlow()))
    { }
  };

  /**
   * Find the first frame that is a child of this frame's next-in-flows,
   * considering both their principal child lists and overflow lists.
   */
  nsIFrame* GetNextInFlowChild(ContinuationTraversingState& aState,
                               bool* aIsInOverflow = nullptr);

  /**
   * Remove the result of GetNextInFlowChild from its current parent and
   * append it to this frame's principal child list.
   */
  nsIFrame* PullNextInFlowChild(ContinuationTraversingState& aState);

  // ==========================================================================
  /*
   * Convenience methods for nsFrameLists stored in the
   * PresContext's proptable
   */

  /**
   * Get the PresContext-stored nsFrameList named aPropID for this frame.
   * May return null.
   */
  nsFrameList* GetPropTableFrames(FrameListPropertyDescriptor aProperty) const;

  /**
   * Remove and return the PresContext-stored nsFrameList named aPropID for
   * this frame. May return null.
   */
  nsFrameList* RemovePropTableFrames(FrameListPropertyDescriptor aProperty);

  /**
   * Set the PresContext-stored nsFrameList named aPropID for this frame
   * to the given aFrameList, which must not be null.
   */
  void SetPropTableFrames(nsFrameList* aFrameList,
                          FrameListPropertyDescriptor aProperty);

  /**
   * Safely destroy the frames on the nsFrameList stored on aProp for this
   * frame then remove the property and delete the frame list.
   * Nothing happens if the property doesn't exist.
   */
  void SafelyDestroyFrameListProp(nsIFrame* aDestructRoot,
                                  nsIPresShell* aPresShell,
                                  mozilla::FramePropertyTable* aPropTable,
                                  FrameListPropertyDescriptor aProp);

  // ==========================================================================

  // Helper used by Progress and Meter frames. Returns true if the bar should
  // be rendered vertically, based on writing-mode and -moz-orient properties.
  bool ResolvedOrientationIsVertical();

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
        frame->IsAbsolutelyPositioned()  )  )
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
  nsOverflowContinuationTracker(nsContainerFrame* aFrame,
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
   * Begin/EndFinish() must be called for each child that is reflowed
   * but no longer has an overflow continuation. (It may be called for
   * other children, but in that case has no effect.) It increments our
   * walker and makes sure we drop any dangling pointers to its
   * next-in-flow. This function MUST be called before stealing or
   * deleting aChild's next-in-flow.
   * The AutoFinish helper object does that for you. Use it like so:
   * if (kidNextInFlow) {
   *   nsOverflowContinuationTracker::AutoFinish fini(tracker, kid);
   *   ... DeleteNextInFlowChild/StealFrame(kidNextInFlow) here ...
   * }
   */
  class MOZ_RAII AutoFinish {
  public:
    AutoFinish(nsOverflowContinuationTracker* aTracker, nsIFrame* aChild)
      : mTracker(aTracker), mChild(aChild)
    {
      if (mTracker) mTracker->BeginFinish(mChild);
    }
    ~AutoFinish() 
    {
      if (mTracker) mTracker->EndFinish(mChild);
    }
  private:
    nsOverflowContinuationTracker* mTracker;
    nsIFrame* mChild;
  };

  /**
   * This function should be called for each child that isn't reflowed.
   * It increments our walker and sets the mOverflowIncomplete
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
      if (aReflowStatus.IsComplete()) {
        aReflowStatus.SetOverflowIncomplete();
      }
    }
  }

private:

  /**
   * @see class AutoFinish
   */
  void BeginFinish(nsIFrame* aChild);
  void EndFinish(nsIFrame* aChild);

  void SetupOverflowContList();
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
  nsFrameList* list = Properties().Get(OverflowProperty());
  NS_ASSERTION(!list || !list->IsEmpty(), "Unexpected empty overflow list");
  return list;
}

inline
nsFrameList*
nsContainerFrame::StealOverflowFrames()
{
  nsFrameList* list = Properties().Remove(OverflowProperty());
  NS_ASSERTION(!list || !list->IsEmpty(), "Unexpected empty overflow list");
  return list;
}

inline void
nsContainerFrame::DestroyOverflowList()
{
  nsFrameList* list = RemovePropTableFrames(OverflowProperty());
  MOZ_ASSERT(list && list->IsEmpty());
  list->Delete(PresContext()->PresShell());
}

#endif /* nsContainerFrame_h___ */
