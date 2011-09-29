/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* base class of all rendering objects */

#ifndef nsFrame_h___
#define nsFrame_h___

#include "nsBox.h"
#include "nsRect.h"
#include "nsString.h"
#include "prlog.h"

#include "nsIPresShell.h"
#include "nsFrameSelection.h"
#include "nsHTMLReflowState.h"
#include "nsHTMLReflowMetrics.h"
#include "nsHTMLParts.h"

/**
 * nsFrame logging constants. We redefine the nspr
 * PRLogModuleInfo.level field to be a bitfield.  Each bit controls a
 * specific type of logging. Each logging operation has associated
 * inline methods defined below.
 */
#define NS_FRAME_TRACE_CALLS        0x1
#define NS_FRAME_TRACE_PUSH_PULL    0x2
#define NS_FRAME_TRACE_CHILD_REFLOW 0x4
#define NS_FRAME_TRACE_NEW_FRAMES   0x8

#define NS_FRAME_LOG_TEST(_lm,_bit) (PRIntn((_lm)->level) & (_bit))

#ifdef NS_DEBUG
#define NS_FRAME_LOG(_bit,_args)                                \
  PR_BEGIN_MACRO                                                \
    if (NS_FRAME_LOG_TEST(nsFrame::GetLogModuleInfo(),_bit)) {  \
      PR_LogPrint _args;                                        \
    }                                                           \
  PR_END_MACRO
#else
#define NS_FRAME_LOG(_bit,_args)
#endif

// XXX Need to rework this so that logging is free when it's off
#ifdef NS_DEBUG
#define NS_FRAME_TRACE_IN(_method) Trace(_method, PR_TRUE)

#define NS_FRAME_TRACE_OUT(_method) Trace(_method, PR_FALSE)

// XXX remove me
#define NS_FRAME_TRACE_MSG(_bit,_args)                          \
  PR_BEGIN_MACRO                                                \
    if (NS_FRAME_LOG_TEST(nsFrame::GetLogModuleInfo(),_bit)) {  \
      TraceMsg _args;                                           \
    }                                                           \
  PR_END_MACRO

#define NS_FRAME_TRACE(_bit,_args)                              \
  PR_BEGIN_MACRO                                                \
    if (NS_FRAME_LOG_TEST(nsFrame::GetLogModuleInfo(),_bit)) {  \
      TraceMsg _args;                                           \
    }                                                           \
  PR_END_MACRO

#define NS_FRAME_TRACE_REFLOW_IN(_method) Trace(_method, PR_TRUE)

#define NS_FRAME_TRACE_REFLOW_OUT(_method, _status) \
  Trace(_method, PR_FALSE, _status)

#else
#define NS_FRAME_TRACE(_bits,_args)
#define NS_FRAME_TRACE_IN(_method)
#define NS_FRAME_TRACE_OUT(_method)
#define NS_FRAME_TRACE_MSG(_bits,_args)
#define NS_FRAME_TRACE_REFLOW_IN(_method)
#define NS_FRAME_TRACE_REFLOW_OUT(_method, _status)
#endif

// Frame allocation boilerplate macros.  Every subclass of nsFrame
// must define its own operator new and GetAllocatedSize.  If they do
// not, the per-frame recycler lists in nsPresArena will not work
// correctly, with potentially catastrophic consequences (not enough
// memory is allocated for a frame object).

#define NS_DECL_FRAMEARENA_HELPERS                                \
  NS_MUST_OVERRIDE void* operator new(size_t, nsIPresShell*);     \
  virtual NS_MUST_OVERRIDE nsQueryFrame::FrameIID GetFrameId();

#define NS_IMPL_FRAMEARENA_HELPERS(class)                         \
  void* class::operator new(size_t sz, nsIPresShell* aShell)      \
  { return aShell->AllocateFrame(nsQueryFrame::class##_id, sz); } \
  nsQueryFrame::FrameIID class::GetFrameId()                      \
  { return nsQueryFrame::class##_id; }

//----------------------------------------------------------------------

struct nsBoxLayoutMetrics;

/**
 * Implementation of a simple frame that's not splittable and has no
 * child frames.
 *
 * Sets the NS_FRAME_SYNCHRONIZE_FRAME_AND_VIEW bit, so the default
 * behavior is to keep the frame and view position and size in sync.
 */
class nsFrame : public nsBox
{
public:
  /**
   * Create a new "empty" frame that maps a given piece of content into a
   * 0,0 area.
   */
  friend nsIFrame* NS_NewEmptyFrame(nsIPresShell* aShell,
                                    nsStyleContext* aContext);

private:
  // Left undefined; nsFrame objects are never allocated from the heap.
  void* operator new(size_t sz) CPP_THROW_NEW;

protected:
  // Overridden to prevent the global delete from being called, since
  // the memory came out of an arena instead of the heap.
  //
  // Ideally this would be private and undefined, like the normal
  // operator new.  Unfortunately, the C++ standard requires an
  // overridden operator delete to be accessible to any subclass that
  // defines a virtual destructor, so we can only make it protected;
  // worse, some C++ compilers will synthesize calls to this function
  // from the "deleting destructors" that they emit in case of
  // delete-expressions, so it can't even be undefined.
  void operator delete(void* aPtr, size_t sz);

public:

  // nsQueryFrame
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame
  NS_IMETHOD  Init(nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIFrame*        asPrevInFlow);
  NS_IMETHOD  SetInitialChildList(ChildListID        aListID,
                                  nsFrameList&       aChildList);
  NS_IMETHOD  AppendFrames(ChildListID     aListID,
                           nsFrameList&    aFrameList);
  NS_IMETHOD  InsertFrames(ChildListID     aListID,
                           nsIFrame*       aPrevFrame,
                           nsFrameList&    aFrameList);
  NS_IMETHOD  RemoveFrame(ChildListID     aListID,
                          nsIFrame*       aOldFrame);
  virtual void DestroyFrom(nsIFrame* aDestructRoot);
  virtual nsStyleContext* GetAdditionalStyleContext(PRInt32 aIndex) const;
  virtual void SetAdditionalStyleContext(PRInt32 aIndex,
                                         nsStyleContext* aStyleContext);
  virtual void SetParent(nsIFrame* aParent);
  virtual nscoord GetBaseline() const;
  virtual nsFrameList GetChildList(ChildListID aListID) const {
    return nsFrameList::EmptyList();
  }
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const {}

  NS_IMETHOD  HandleEvent(nsPresContext* aPresContext, 
                          nsGUIEvent*     aEvent,
                          nsEventStatus*  aEventStatus);
  NS_IMETHOD  GetContentForEvent(nsEvent* aEvent,
                                 nsIContent** aContent);
  NS_IMETHOD  GetCursor(const nsPoint&    aPoint,
                        nsIFrame::Cursor& aCursor);

  NS_IMETHOD  GetPointFromOffset(PRInt32                inOffset,
                                 nsPoint*               outPoint);

  NS_IMETHOD  GetChildFrameContainingOffset(PRInt32     inContentOffset,
                                 bool                   inHint,
                                 PRInt32*               outFrameContentOffset,
                                 nsIFrame*              *outChildFrame);

  static nsresult  GetNextPrevLineFromeBlockFrame(nsPresContext* aPresContext,
                                        nsPeekOffsetStruct *aPos, 
                                        nsIFrame *aBlockFrame, 
                                        PRInt32 aLineStart, 
                                        PRInt8 aOutSideLimit
                                        );

  NS_IMETHOD  CharacterDataChanged(CharacterDataChangeInfo* aInfo);
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);
  virtual nsSplittableType GetSplittableType() const;
  virtual nsIFrame* GetPrevContinuation() const;
  NS_IMETHOD  SetPrevContinuation(nsIFrame*);
  virtual nsIFrame* GetNextContinuation() const;
  NS_IMETHOD  SetNextContinuation(nsIFrame*);
  virtual nsIFrame* GetPrevInFlowVirtual() const;
  NS_IMETHOD  SetPrevInFlow(nsIFrame*);
  virtual nsIFrame* GetNextInFlowVirtual() const;
  NS_IMETHOD  SetNextInFlow(nsIFrame*);
  NS_IMETHOD  GetOffsetFromView(nsPoint& aOffset, nsIView** aView) const;
  virtual nsIAtom* GetType() const;
  virtual bool IsContainingBlock() const;

  NS_IMETHOD  GetSelected(bool *aSelected) const;
  NS_IMETHOD  IsSelectable(bool* aIsSelectable, PRUint8* aSelectStyle) const;

  NS_IMETHOD  GetSelectionController(nsPresContext *aPresContext, nsISelectionController **aSelCon);

  virtual bool PeekOffsetNoAmount(bool aForward, PRInt32* aOffset);
  virtual bool PeekOffsetCharacter(bool aForward, PRInt32* aOffset,
                                     bool aRespectClusters = true);
  virtual bool PeekOffsetWord(bool aForward, bool aWordSelectEatSpace, bool aIsKeyboardSelect,
                                PRInt32* aOffset, PeekWordState *aState);
  /**
   * Check whether we should break at a boundary between punctuation and
   * non-punctuation. Only call it at a punctuation boundary
   * (i.e. exactly one of the previous and next characters are punctuation).
   * @param aForward true if we're moving forward in content order
   * @param aPunctAfter true if the next character is punctuation
   * @param aWhitespaceAfter true if the next character is whitespace
   */
  bool BreakWordBetweenPunctuation(const PeekWordState* aState,
                                     bool aForward,
                                     bool aPunctAfter, bool aWhitespaceAfter,
                                     bool aIsKeyboardSelect);

  NS_IMETHOD  CheckVisibility(nsPresContext* aContext, PRInt32 aStartIndex, PRInt32 aEndIndex, bool aRecurse, bool *aFinished, bool *_retval);

  NS_IMETHOD  GetOffsets(PRInt32 &aStart, PRInt32 &aEnd) const;
  virtual void ChildIsDirty(nsIFrame* aChild);

#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#endif

  virtual nsIFrame* GetParentStyleContextFrame() {
    return DoGetParentStyleContextFrame();
  }

  /**
   * Do the work for getting the parent style context frame so that
   * other frame's |GetParentStyleContextFrame| methods can call this
   * method on *another* frame.  (This function handles out-of-flow
   * frames by using the frame manager's placeholder map and it also
   * handles block-within-inline and generated content wrappers.)
   */
  nsIFrame* DoGetParentStyleContextFrame();

  virtual bool IsEmpty();
  virtual bool IsSelfEmpty();

  virtual void MarkIntrinsicWidthsDirty();
  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext);
  virtual void AddInlineMinWidth(nsRenderingContext *aRenderingContext,
                                 InlineMinWidthData *aData);
  virtual void AddInlinePrefWidth(nsRenderingContext *aRenderingContext,
                                  InlinePrefWidthData *aData);
  virtual IntrinsicWidthOffsetData
    IntrinsicWidthOffsets(nsRenderingContext* aRenderingContext);
  virtual IntrinsicSize GetIntrinsicSize();
  virtual nsSize GetIntrinsicRatio();

  virtual nsSize ComputeSize(nsRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             bool aShrinkWrap);

  // Compute tight bounds assuming this frame honours its border, background
  // and outline, its children's tight bounds, and nothing else.
  nsRect ComputeSimpleTightBounds(gfxContext* aContext) const;
  
  /**
   * A helper, used by |nsFrame::ComputeSize| (for frames that need to
   * override only this part of ComputeSize), that computes the size
   * that should be returned when 'width', 'height', and
   * min/max-width/height are all 'auto' or equivalent.
   *
   * In general, frames that can accept any computed width/height should
   * override only ComputeAutoSize, and frames that cannot do so need to
   * override ComputeSize to enforce their width/height invariants.
   *
   * Implementations may optimize by returning a garbage width if
   * GetStylePosition()->mWidth.GetUnit() != eStyleUnit_Auto, and
   * likewise for height, since in such cases the result is guaranteed
   * to be unused.
   */
  virtual nsSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, bool aShrinkWrap);

  /**
   * Utility function for ComputeAutoSize implementations.  Return
   * max(GetMinWidth(), min(aWidthInCB, GetPrefWidth()))
   */
  nscoord ShrinkWidthToFit(nsRenderingContext *aRenderingContext,
                           nscoord aWidthInCB);

  NS_IMETHOD  WillReflow(nsPresContext* aPresContext);
  NS_IMETHOD  Reflow(nsPresContext*          aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus);
  NS_IMETHOD  DidReflow(nsPresContext*           aPresContext,
                        const nsHTMLReflowState*  aReflowState,
                        nsDidReflowStatus         aStatus);
  virtual bool CanContinueTextRun() const;

  // Selection Methods
  // XXX Doc me... (in nsIFrame.h puhleeze)
  // XXX If these are selection specific, then the name should imply selection
  // rather than generic event processing, e.g., SelectionHandlePress...
  NS_IMETHOD HandlePress(nsPresContext* aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus*  aEventStatus);

  NS_IMETHOD HandleMultiplePress(nsPresContext* aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus*  aEventStatus,
                         bool            aControlHeld);

  NS_IMETHOD HandleDrag(nsPresContext* aPresContext,
                        nsGUIEvent *    aEvent,
                        nsEventStatus*  aEventStatus);

  NS_IMETHOD HandleRelease(nsPresContext* aPresContext,
                           nsGUIEvent *    aEvent,
                           nsEventStatus*  aEventStatus);

  NS_IMETHOD PeekBackwardAndForward(nsSelectionAmount aAmountBack,
                                    nsSelectionAmount aAmountForward,
                                    PRInt32 aStartPos,
                                    nsPresContext* aPresContext,
                                    bool aJumpLines,
                                    bool aMultipleSelection);


  // Helper for GetContentAndOffsetsFromPoint; calculation of content offsets
  // in this function assumes there is no child frame that can be targeted.
  virtual ContentOffsets CalcContentOffsetsFromFramePoint(nsPoint aPoint);

  // Box layout methods
  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetFlex(nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState);

  // We compute and store the HTML content's overflow area. So don't
  // try to compute it in the box code.
  virtual bool ComputesOwnOverflowArea() { return true; }

  //--------------------------------------------------
  // Additional methods

  /**
   * Helper method to invalidate portions of a standard container frame if the
   * desired size indicates that the size has changed (specifically border,
   * background and outline).
   * We assume that the difference between the old frame area and the new
   * frame area is invalidated by some other means.
   * @param aDesiredSize the new size of the frame
   */
  void CheckInvalidateSizeChange(nsHTMLReflowMetrics&     aNewDesiredSize);

  // Helper function that tests if the frame tree is too deep; if it is
  // it marks the frame as "unflowable", zeroes out the metrics, sets
  // the reflow status, and returns PR_TRUE. Otherwise, the frame is
  // unmarked "unflowable" and the metrics and reflow status are not
  // touched and PR_FALSE is returned.
  bool IsFrameTreeTooDeep(const nsHTMLReflowState& aReflowState,
                            nsHTMLReflowMetrics& aMetrics,
                            nsReflowStatus& aStatus);

  // Incorporate the child overflow areas into aOverflowAreas.
  // If the child does not have a overflow, use the child area.
  void ConsiderChildOverflow(nsOverflowAreas& aOverflowAreas,
                             nsIFrame* aChildFrame);

  virtual const void* GetStyleDataExternal(nsStyleStructID aSID) const;


#ifdef NS_DEBUG
  /**
   * Tracing method that writes a method enter/exit routine to the
   * nspr log using the nsIFrame log module. The tracing is only
   * done when the NS_FRAME_TRACE_CALLS bit is set in the log module's
   * level field.
   */
  void Trace(const char* aMethod, bool aEnter);
  void Trace(const char* aMethod, bool aEnter, nsReflowStatus aStatus);
  void TraceMsg(const char* fmt, ...);

  // Helper function that verifies that each frame in the list has the
  // NS_FRAME_IS_DIRTY bit set
  static void VerifyDirtyBitSet(const nsFrameList& aFrameList);

  // Helper function to return the index in parent of the frame's content
  // object. Returns -1 on error or if the frame doesn't have a content object
  static PRInt32 ContentIndexInContainer(const nsIFrame* aFrame);

  static void IndentBy(FILE* out, PRInt32 aIndent) {
    while (--aIndent >= 0) fputs("  ", out);
  }
  
  void ListTag(FILE* out) const {
    ListTag(out, this);
  }

  static void ListTag(FILE* out, const nsIFrame* aFrame) {
    nsAutoString tmp;
    aFrame->GetFrameName(tmp);
    fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);
    fprintf(out, "@%p", static_cast<const void*>(aFrame));
  }

  static void XMLQuote(nsString& aString);

  /**
   * Dump out the "base classes" regression data. This should dump
   * out the interior data, not the "frame" XML container. And it
   * should call the base classes same named method before doing
   * anything specific in a derived class. This means that derived
   * classes need not override DumpRegressionData unless they need
   * some custom behavior that requires changing how the outer "frame"
   * XML container is dumped.
   */
  virtual void DumpBaseRegressionData(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent);
  
  nsresult MakeFrameName(const nsAString& aKind, nsAString& aResult) const;

  // Display Reflow Debugging 
  static void* DisplayReflowEnter(nsPresContext*          aPresContext,
                                  nsIFrame*                aFrame,
                                  const nsHTMLReflowState& aReflowState);
  static void* DisplayLayoutEnter(nsIFrame* aFrame);
  static void* DisplayIntrinsicWidthEnter(nsIFrame* aFrame,
                                          const char* aType);
  static void* DisplayIntrinsicSizeEnter(nsIFrame* aFrame,
                                         const char* aType);
  static void  DisplayReflowExit(nsPresContext*      aPresContext,
                                 nsIFrame*            aFrame,
                                 nsHTMLReflowMetrics& aMetrics,
                                 PRUint32             aStatus,
                                 void*                aFrameTreeNode);
  static void  DisplayLayoutExit(nsIFrame* aFrame,
                                 void* aFrameTreeNode);
  static void  DisplayIntrinsicWidthExit(nsIFrame* aFrame,
                                         const char* aType,
                                         nscoord aResult,
                                         void* aFrameTreeNode);
  static void  DisplayIntrinsicSizeExit(nsIFrame* aFrame,
                                        const char* aType,
                                        nsSize aResult,
                                        void* aFrameTreeNode);

  static void DisplayReflowStartup();
  static void DisplayReflowShutdown();
#endif

  static void ShutdownLayerActivityTimer();

  /**
   * Adds display item for standard CSS background if necessary.
   * Does not check IsVisibleForPainting.
   * @param aForceBackground draw the background even if the frame
   * background style appears to have no background --- this is useful
   * for frames that might receive a propagated background via
   * nsCSSRendering::FindBackground
   */
  nsresult DisplayBackgroundUnconditional(nsDisplayListBuilder*   aBuilder,
                                          const nsDisplayListSet& aLists,
                                          bool aForceBackground = false);
  /**
   * Adds display items for standard CSS borders, background and outline for
   * for this frame, as necessary. Checks IsVisibleForPainting and won't
   * display anything if the frame is not visible.
   * @param aForceBackground draw the background even if the frame
   * background style appears to have no background --- this is useful
   * for frames that might receive a propagated background via
   * nsCSSRendering::FindBackground
   */
  nsresult DisplayBorderBackgroundOutline(nsDisplayListBuilder*   aBuilder,
                                          const nsDisplayListSet& aLists,
                                          bool aForceBackground = false);
  /**
   * Add a display item for the CSS outline. Does not check visibility.
   */
  nsresult DisplayOutlineUnconditional(nsDisplayListBuilder*   aBuilder,
                                       const nsDisplayListSet& aLists);
  /**
   * Add a display item for the CSS outline, after calling
   * IsVisibleForPainting to confirm we are visible.
   */
  nsresult DisplayOutline(nsDisplayListBuilder*   aBuilder,
                          const nsDisplayListSet& aLists);

  /**
   * Adjust the given parent frame to the right style context parent frame for
   * the child, given the pseudo-type of the prospective child.  This handles
   * things like walking out of table pseudos and so forth.
   *
   * @param aProspectiveParent what GetParent() on the child returns.
   *                           Must not be null.
   * @param aChildPseudo the child's pseudo type, if any.
   */
  static nsIFrame*
  CorrectStyleParentFrame(nsIFrame* aProspectiveParent, nsIAtom* aChildPseudo);

protected:
  // Protected constructor and destructor
  nsFrame(nsStyleContext* aContext);
  virtual ~nsFrame();

  /**
   * To be called by |BuildDisplayLists| of this class or derived classes to add
   * a translucent overlay if this frame's content is selected.
   * @param aContentType an nsISelectionDisplay DISPLAY_ constant identifying
   * which kind of content this is for
   */
  nsresult DisplaySelectionOverlay(nsDisplayListBuilder* aBuilder,
      nsDisplayList* aList, PRUint16 aContentType = nsISelectionDisplay::DISPLAY_FRAMES);

  PRInt16 DisplaySelection(nsPresContext* aPresContext, bool isOkToTurnOn = false);
  
  // Style post processing hook
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext);

public:
  //given a frame five me the first/last leaf available
  //XXX Robert O'Callahan wants to move these elsewhere
  static void GetLastLeaf(nsPresContext* aPresContext, nsIFrame **aFrame);
  static void GetFirstLeaf(nsPresContext* aPresContext, nsIFrame **aFrame);

  // Return the line number of the aFrame, and (optionally) the containing block
  // frame.
  // If aScrollLock is true, don't break outside scrollframes when looking for a
  // containing block frame.
  static PRInt32 GetLineNumber(nsIFrame *aFrame,
                               bool aLockScroll,
                               nsIFrame** aContainingBlock = nsnull);

  // test whether aFrame should apply paginated overflow clipping.
  static bool ApplyPaginatedOverflowClipping(const nsIFrame* aFrame)
  {
    // If we're paginated and a block, and have NS_BLOCK_CLIP_PAGINATED_OVERFLOW
    // set, then we want to clip our overflow.
    return
      aFrame->PresContext()->IsPaginated() &&
      aFrame->GetType() == nsGkAtoms::blockFrame &&
      (aFrame->GetStateBits() & NS_BLOCK_CLIP_PAGINATED_OVERFLOW) != 0;
  }

protected:

  // Test if we are selecting a table object:
  //  Most table/cell selection requires that Ctrl (Cmd on Mac) key is down 
  //   during a mouse click or drag. Exception is using Shift+click when
  //   already in "table/cell selection mode" to extend a block selection
  //  Get the parent content node and offset of the frame 
  //   of the enclosing cell or table (if not inside a cell)
  //  aTarget tells us what table element to select (currently only cell and table supported)
  //  (enums for this are defined in nsIFrame.h)
  NS_IMETHOD GetDataForTableSelection(const nsFrameSelection *aFrameSelection,
                                      nsIPresShell *aPresShell, nsMouseEvent *aMouseEvent, 
                                      nsIContent **aParentContent, PRInt32 *aContentOffset, 
                                      PRInt32 *aTarget);

  // Fills aCursor with the appropriate information from ui
  static void FillCursorInformationFromStyle(const nsStyleUserInterface* ui,
                                             nsIFrame::Cursor& aCursor);
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);

#ifdef DEBUG_LAYOUT
  virtual void GetBoxName(nsAutoString& aName);
#endif

  void InitBoxMetrics(bool aClear);
  nsBoxLayoutMetrics* BoxMetrics() const;

  // Fire DOM event. If no aContent argument use frame's mContent.
  void FireDOMEvent(const nsAString& aDOMEventName, nsIContent *aContent = nsnull);

private:
  nsresult BoxReflow(nsBoxLayoutState& aState,
                     nsPresContext*    aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     nsRenderingContext* aRenderingContext,
                     nscoord aX,
                     nscoord aY,
                     nscoord aWidth,
                     nscoord aHeight,
                     bool aMoveFrame = true);

  NS_IMETHODIMP RefreshSizeCache(nsBoxLayoutState& aState);

  virtual nsILineIterator* GetLineIterator();

#ifdef NS_DEBUG
public:
  // Formerly the nsIFrameDebug interface

  NS_IMETHOD  List(FILE* out, PRInt32 aIndent) const;
  /**
   * lists the frames beginning from the root frame
   * - calls root frame's List(...)
   */
  static void RootFrameList(nsPresContext* aPresContext,
                            FILE* out, PRInt32 aIndent);

  static void DumpFrameTree(nsIFrame* aFrame);

  /**
   * Get a printable from of the name of the frame type.
   * XXX This should be eliminated and we use GetType() instead...
   */
  NS_IMETHOD  GetFrameName(nsAString& aResult) const;
  /**
   * Return the state bits that are relevant to regression tests (that
   * is, those bits which indicate a real difference when they differ
   */
  NS_IMETHOD_(nsFrameState)  GetDebugStateBits() const;
  /**
   * Called to dump out regression data that describes the layout
   * of the frame and its children, and so on. The format of the
   * data is dictated to be XML (using a specific DTD); the
   * specific kind of data dumped is up to the frame itself, with
   * the caveat that some base types are defined.
   * For more information, see XXX.
   */
  NS_IMETHOD  DumpRegressionData(nsPresContext* aPresContext,
                                 FILE* out, PRInt32 aIndent);

  /**
   * See if style tree verification is enabled. To enable style tree
   * verification add "styleverifytree:1" to your NSPR_LOG_MODULES
   * environment variable (any non-zero debug level will work). Or,
   * call SetVerifyStyleTreeEnable with PR_TRUE.
   */
  static bool GetVerifyStyleTreeEnable();

  /**
   * Set the verify-style-tree enable flag.
   */
  static void SetVerifyStyleTreeEnable(bool aEnabled);

  /**
   * The frame class and related classes share an nspr log module
   * for logging frame activity.
   *
   * Note: the log module is created during library initialization which
   * means that you cannot perform logging before then.
   */
  static PRLogModuleInfo* GetLogModuleInfo();

  // Show frame borders when rendering
  static void ShowFrameBorders(bool aEnable);
  static bool GetShowFrameBorders();

  // Show frame border of event target
  static void ShowEventTargetFrameBorder(bool aEnable);
  static bool GetShowEventTargetFrameBorder();

  static void PrintDisplayList(nsDisplayListBuilder* aBuilder,
                               const nsDisplayList& aList);

#endif
};

// Start Display Reflow Debugging
#ifdef DEBUG

  struct DR_cookie {
    DR_cookie(nsPresContext*          aPresContext,
              nsIFrame*                aFrame, 
              const nsHTMLReflowState& aReflowState,
              nsHTMLReflowMetrics&     aMetrics,
              nsReflowStatus&          aStatus);     
    ~DR_cookie();
    void Change() const;

    nsPresContext*          mPresContext;
    nsIFrame*                mFrame;
    const nsHTMLReflowState& mReflowState;
    nsHTMLReflowMetrics&     mMetrics;
    nsReflowStatus&          mStatus;    
    void*                    mValue;
  };

  struct DR_layout_cookie {
    DR_layout_cookie(nsIFrame* aFrame);
    ~DR_layout_cookie();

    nsIFrame* mFrame;
    void* mValue;
  };
  
  struct DR_intrinsic_width_cookie {
    DR_intrinsic_width_cookie(nsIFrame* aFrame, const char* aType,
                              nscoord& aResult);
    ~DR_intrinsic_width_cookie();

    nsIFrame* mFrame;
    const char* mType;
    nscoord& mResult;
    void* mValue;
  };
  
  struct DR_intrinsic_size_cookie {
    DR_intrinsic_size_cookie(nsIFrame* aFrame, const char* aType,
                             nsSize& aResult);
    ~DR_intrinsic_size_cookie();

    nsIFrame* mFrame;
    const char* mType;
    nsSize& mResult;
    void* mValue;
  };

  struct DR_init_constraints_cookie {
    DR_init_constraints_cookie(nsIFrame* aFrame, nsHTMLReflowState* aState,
                               nscoord aCBWidth, nscoord aCBHeight,
                               const nsMargin* aBorder,
                               const nsMargin* aPadding);
    ~DR_init_constraints_cookie();

    nsIFrame* mFrame;
    nsHTMLReflowState* mState;
    void* mValue;
  };

  struct DR_init_offsets_cookie {
    DR_init_offsets_cookie(nsIFrame* aFrame, nsCSSOffsetState* aState,
                           nscoord aCBWidth, const nsMargin* aBorder,
                           const nsMargin* aPadding);
    ~DR_init_offsets_cookie();

    nsIFrame* mFrame;
    nsCSSOffsetState* mState;
    void* mValue;
  };

  struct DR_init_type_cookie {
    DR_init_type_cookie(nsIFrame* aFrame, nsHTMLReflowState* aState);
    ~DR_init_type_cookie();

    nsIFrame* mFrame;
    nsHTMLReflowState* mState;
    void* mValue;
  };

#define DISPLAY_REFLOW(dr_pres_context, dr_frame, dr_rf_state, dr_rf_metrics, dr_rf_status) \
  DR_cookie dr_cookie(dr_pres_context, dr_frame, dr_rf_state, dr_rf_metrics, dr_rf_status); 
#define DISPLAY_REFLOW_CHANGE() \
  dr_cookie.Change();
#define DISPLAY_LAYOUT(dr_frame) \
  DR_layout_cookie dr_cookie(dr_frame); 
#define DISPLAY_MIN_WIDTH(dr_frame, dr_result) \
  DR_intrinsic_width_cookie dr_cookie(dr_frame, "Min", dr_result)
#define DISPLAY_PREF_WIDTH(dr_frame, dr_result) \
  DR_intrinsic_width_cookie dr_cookie(dr_frame, "Pref", dr_result)
#define DISPLAY_PREF_SIZE(dr_frame, dr_result) \
  DR_intrinsic_size_cookie dr_cookie(dr_frame, "Pref", dr_result)
#define DISPLAY_MIN_SIZE(dr_frame, dr_result) \
  DR_intrinsic_size_cookie dr_cookie(dr_frame, "Min", dr_result)
#define DISPLAY_MAX_SIZE(dr_frame, dr_result) \
  DR_intrinsic_size_cookie dr_cookie(dr_frame, "Max", dr_result)
#define DISPLAY_INIT_CONSTRAINTS(dr_frame, dr_state, dr_cbw, dr_cbh,       \
                                 dr_bdr, dr_pad)                           \
  DR_init_constraints_cookie dr_cookie(dr_frame, dr_state, dr_cbw, dr_cbh, \
                                       dr_bdr, dr_pad)
#define DISPLAY_INIT_OFFSETS(dr_frame, dr_state, dr_cbw, dr_bdr, dr_pad)  \
  DR_init_offsets_cookie dr_cookie(dr_frame, dr_state, dr_cbw, dr_bdr, dr_pad)
#define DISPLAY_INIT_TYPE(dr_frame, dr_result) \
  DR_init_type_cookie dr_cookie(dr_frame, dr_result)

#else

#define DISPLAY_REFLOW(dr_pres_context, dr_frame, dr_rf_state, dr_rf_metrics, dr_rf_status) 
#define DISPLAY_REFLOW_CHANGE() 
#define DISPLAY_LAYOUT(dr_frame) PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_MIN_WIDTH(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_PREF_WIDTH(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_PREF_SIZE(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_MIN_SIZE(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_MAX_SIZE(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_INIT_CONSTRAINTS(dr_frame, dr_state, dr_cbw, dr_cbh,       \
                                 dr_bdr, dr_pad)                           \
  PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_INIT_OFFSETS(dr_frame, dr_state, dr_cbw, dr_bdr, dr_pad)  \
  PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_INIT_TYPE(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO

#endif
// End Display Reflow Debugging

// similar to NS_ENSURE_TRUE but with no return value
#define ENSURE_TRUE(x)                                        \
  PR_BEGIN_MACRO                                              \
    if (!(x)) {                                               \
       NS_WARNING("ENSURE_TRUE(" #x ") failed");              \
       return;                                                \
    }                                                         \
  PR_END_MACRO
#endif /* nsFrame_h___ */
