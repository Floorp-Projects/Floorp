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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#ifndef nsFrame_h___
#define nsFrame_h___

#include "nsIFrame.h"
#include "nsRect.h"
#include "nsString.h"
#include "prlog.h"
#ifdef NS_DEBUG
#include "nsIFrameDebug.h"
#endif

#include "nsIPresShell.h"
#include "nsIReflowCommand.h"
#include "nsIFrameSelection.h"
#include "nsHTMLReflowState.h"
#include "nsHTMLReflowMetrics.h"

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
    if (NS_FRAME_LOG_TEST(nsIFrameDebug::GetLogModuleInfo(),_bit)) { \
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
    if (NS_FRAME_LOG_TEST(nsIFrameDebug::GetLogModuleInfo(),_bit)) { \
      TraceMsg _args;                                           \
    }                                                           \
  PR_END_MACRO

#define NS_FRAME_TRACE(_bit,_args)                              \
  PR_BEGIN_MACRO                                                \
    if (NS_FRAME_LOG_TEST(nsIFrameDebug::GetLogModuleInfo(),_bit)) { \
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


//----------------------------------------------------------------------

/**
 * Implementation of a simple frame that's not splittable and has no
 * child frames.
 *
 * Sets the NS_FRAME_SYNCHRONIZE_FRAME_AND_VIEW bit, so the default
 * behavior is to keep the frame and view position and size in sync.
 */
class nsFrame : public nsIFrame
#ifdef NS_DEBUG
  , public nsIFrameDebug
#endif
{
public:
  /**
   * Create a new "empty" frame that maps a given piece of content into a
   * 0,0 area.
   */
  friend nsresult NS_NewEmptyFrame(nsIPresShell* aShell, nsIFrame** aInstancePtrResult);

  // Overloaded new operator. Initializes the memory to 0 and relies on an arena
  // (which comes from the presShell) to perform the allocation.
  void* operator new(size_t sz, nsIPresShell* aPresShell);

  // Overridden to prevent the global delete from being called, since the memory
  // came out of an nsIArena instead of the global delete operator's heap.  
  // XXX Would like to make this private some day, but our UNIX compilers can't 
  // deal with it.
  void operator delete(void* aPtr, size_t sz);

private:
  // The normal operator new is disallowed on nsFrames.
#if (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95))
  void* operator new(size_t sz) throw () { return nsnull; };
#else
  void* operator new(size_t sz) { return nsnull; };
#endif

public:

  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD  Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        asPrevInFlow);
  NS_IMETHOD  SetInitialChildList(nsIPresContext* aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList);
  NS_IMETHOD  AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  InsertFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  RemoveFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);
  NS_IMETHOD  ReplaceFrame(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame,
                           nsIFrame*       aNewFrame);
  NS_IMETHOD  Destroy(nsIPresContext* aPresContext);
  NS_IMETHOD  CalcBorderPadding(nsMargin& aBorderPadding) const;
  NS_IMETHOD  GetAdditionalStyleContext(PRInt32 aIndex, 
                                        nsIStyleContext** aStyleContext) const;
  NS_IMETHOD  SetAdditionalStyleContext(PRInt32 aIndex, 
                                        nsIStyleContext* aStyleContext);
  NS_IMETHOD  GetAdditionalChildListName(PRInt32 aIndex, nsIAtom** aListName) const;
  NS_IMETHOD  FirstChild(nsIPresContext* aPresContext,
                         nsIAtom*        aListName,
                         nsIFrame**      aFirstChild) const;
  NS_IMETHOD  Paint(nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer,
                    PRUint32             aFlags = 0);
  NS_IMETHOD  HandleEvent(nsIPresContext* aPresContext, 
                          nsGUIEvent*     aEvent,
                          nsEventStatus*  aEventStatus);
  NS_IMETHOD  GetContentForEvent(nsIPresContext* aPresContext,
                                 nsEvent* aEvent,
                                 nsIContent** aContent);
  NS_IMETHOD  GetCursor(nsIPresContext* aPresContext,
                        nsPoint&        aPoint,
                        PRInt32&        aCursor);
  NS_IMETHOD  GetFrameForPoint(nsIPresContext* aPresContext,
                               const nsPoint& aPoint, 
                               nsFramePaintLayer aWhichLayer,
                               nsIFrame**     aFrame);

  NS_IMETHOD  GetPointFromOffset(nsIPresContext*        inPresContext,
                                 nsIRenderingContext*   inRendContext,
                                 PRInt32                inOffset,
                                 nsPoint*               outPoint);

  NS_IMETHOD  GetChildFrameContainingOffset(PRInt32     inContentOffset,
                                 PRBool                 inHint,
                                 PRInt32*               outFrameContentOffset,
                                 nsIFrame*              *outChildFrame);

  static nsresult  GetNextPrevLineFromeBlockFrame(nsIPresContext* aPresContext,
                                        nsPeekOffsetStruct *aPos, 
                                        nsIFrame *aBlockFrame, 
                                        PRInt32 aLineStart, 
                                        PRInt8 aOutSideLimit
                                        );
  NS_IMETHOD  ContentChanged(nsIPresContext* aPresContext,
                             nsIContent*     aChild,
                             nsISupports*    aSubContent);
  NS_IMETHOD  AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent*     aChild,
                               PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType, 
                               PRInt32         aHint);
  NS_IMETHOD  ContentStateChanged(nsIPresContext* aPresContext,
                                  nsIContent*     aChild,
                                  PRInt32         aHint);
  NS_IMETHOD  IsSplittable(nsSplittableType& aIsSplittable) const;
  NS_IMETHOD  GetPrevInFlow(nsIFrame** aPrevInFlow) const;
  NS_IMETHOD  SetPrevInFlow(nsIFrame*);
  NS_IMETHOD  GetNextInFlow(nsIFrame** aNextInFlow) const;
  NS_IMETHOD  SetNextInFlow(nsIFrame*);
  NS_IMETHOD  GetView(nsIPresContext* aPresContext, nsIView** aView) const;
  NS_IMETHOD  SetView(nsIPresContext* aPresContext, nsIView* aView);
  NS_IMETHOD  GetParentWithView(nsIPresContext* aPresContext, nsIFrame** aParent) const;
  NS_IMETHOD  GetOffsetFromView(nsIPresContext* aPresContext, nsPoint& aOffset, nsIView** aView) const;
  NS_IMETHOD  GetWindow(nsIPresContext* aPresContext, nsIWidget**) const;
  NS_IMETHOD  GetFrameType(nsIAtom** aType) const;
  NS_IMETHOD  IsPercentageBase(PRBool& aBase) const;
  NS_IMETHOD  Scrolled(nsIView *aView);
#ifdef NS_DEBUG
  NS_IMETHOD  List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD  GetFrameName(nsAString& aResult) const;
  NS_IMETHOD  DumpRegressionData(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent, PRBool aIncludeStyleData);
  NS_IMETHOD  SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
  NS_IMETHOD  VerifyTree() const;
#endif

  NS_IMETHOD  SetSelected(nsIPresContext* aPresContext, nsIDOMRange *aRange,PRBool aSelected, nsSpread aSpread);
  NS_IMETHOD  GetSelected(PRBool *aSelected) const;
  NS_IMETHOD  IsSelectable(PRBool* aIsSelectable, PRUint8* aSelectStyle) const;

  NS_IMETHOD  GetSelectionController(nsIPresContext *aPresContext, nsISelectionController **aSelCon);
  NS_IMETHOD  PeekOffset(nsIPresContext* aPresContext, nsPeekOffsetStruct *aPos) ;
  NS_IMETHOD  CheckVisibility(nsIPresContext* aContext, PRInt32 aStartIndex, PRInt32 aEndIndex, PRBool aRecurse, PRBool *aFinished, PRBool *_retval);

  NS_IMETHOD  PeekOffsetParagraph(nsIPresContext* aPresContext,
                                  nsPeekOffsetStruct *aPos);
  NS_IMETHOD  GetOffsets(PRInt32 &aStart, PRInt32 &aEnd) const;
  NS_IMETHOD  ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild);
  NS_IMETHOD  ReflowCommandNotify(nsIPresShell*     aPresShell,
                                  nsIReflowCommand* aRC,
                                  PRBool            aCommandAdded);

#ifdef ACCESSIBILITY
  NS_IMETHOD  GetAccessible(nsIAccessible** aAccessible);
#endif

  NS_IMETHOD GetParentStyleContextProvider(nsIPresContext* aPresContext,
                                           nsIFrame** aProviderFrame, 
                                           nsContextProviderRelationship& aRelationship);

  // Check Style Visibility and mState for Selection (when printing)
  NS_IMETHOD IsVisibleForPainting(nsIPresContext *     aPresContext, 
                                  nsIRenderingContext& aRenderingContext,
                                  PRBool               aCheckVis,
                                  PRBool*              aIsVisible);

  NS_IMETHOD IsEmpty(PRBool aIsQuirkMode,
                     PRBool aIsPre,
                     PRBool* aResult);

  // nsIHTMLReflow
  NS_IMETHOD  WillReflow(nsIPresContext* aPresContext);
  NS_IMETHOD  Reflow(nsIPresContext*          aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus);
  NS_IMETHOD  DidReflow(nsIPresContext* aPresContext,
                        nsDidReflowStatus aStatus);
  NS_IMETHOD CanContinueTextRun(PRBool& aContinueTextRun) const;
  NS_IMETHOD AdjustFrameSize(nscoord aExtraSpace, nscoord& aUsedSpace);
  NS_IMETHOD TrimTrailingWhiteSpace(nsIPresContext* aPresContext,
                                    nsIRenderingContext& aRC,
                                    nscoord& aDeltaWidth);

  // Selection Methods
  // XXX Doc me... (in nsIFrame.h puhleeze)
  // XXX If these are selection specific, then the name should imply selection
  // rather than generic event processing, e.g., SelectionHandlePress...
  NS_IMETHOD HandlePress(nsIPresContext* aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus*  aEventStatus);

  NS_IMETHOD HandleMultiplePress(nsIPresContext* aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus*  aEventStatus);

  NS_IMETHOD HandleDrag(nsIPresContext* aPresContext,
                        nsGUIEvent *    aEvent,
                        nsEventStatus*  aEventStatus);

  NS_IMETHOD HandleRelease(nsIPresContext* aPresContext,
                           nsGUIEvent *    aEvent,
                           nsEventStatus*  aEventStatus);

  NS_IMETHOD GetContentAndOffsetsFromPoint(nsIPresContext* aCX,
                                           const nsPoint& aPoint,
                                           nsIContent **   aNewContent,
                                           PRInt32&        aContentOffset,
                                           PRInt32&        aContentOffsetEnd,
                                           PRBool&         aBeginFrameContent);
  NS_IMETHOD PeekBackwardAndForward(nsSelectionAmount aAmountBack,
                                    nsSelectionAmount aAmountForward,
                                    PRInt32 aStartPos,
                                    nsIPresContext* aPresContext,
                                    PRBool aJumpLines);


  //--------------------------------------------------
  // Additional methods

  // Invalidate part of the frame by asking the view manager to repaint.
  // aDamageRect is in the frame's local coordinate space
  void        Invalidate(nsIPresContext* aPresContext,
                         const nsRect& aDamageRect,
                         PRBool aImmediate = PR_FALSE) const;

  // Helper function to return the index in parent of the frame's content
  // object. Returns -1 on error or if the frame doesn't have a content object
  static PRInt32 ContentIndexInContainer(const nsIFrame* aFrame);

  // Helper function that tests if the frame tree is too deep; if it
  // is it marks the frame as "unflowable" and zeros out the metrics
  // and returns PR_TRUE. Otherwise, the frame is unmarked
  // "unflowable" and the metrics are not touched and PR_FALSE is
  // returned.
  PRBool IsFrameTreeTooDeep(const nsHTMLReflowState& aReflowState,
                            nsHTMLReflowMetrics& aMetrics);

  virtual nsresult GetClosestViewForFrame(nsIPresContext* aPresContext,
                                          nsIFrame *aFrame,
                                          nsIView **aView);

  static nsresult CreateAndPostReflowCommand(nsIPresShell*                aPresShell,
                                             nsIFrame*                    aTargetFrame,
                                             nsIReflowCommand::ReflowType aReflowType,
                                             nsIFrame*                    aChildFrame,
                                             nsIAtom*                     aAttribute,
                                             nsIAtom*                     aListName);

  //Mouse Capturing code used by the frames to tell the view to capture all the following events
  NS_IMETHOD CaptureMouse(nsIPresContext* aPresContext, PRBool aGrabMouseEvents);
  PRBool   IsMouseCaptured(nsIPresContext* aPresContext);
#ifdef IBMBIDI
  NS_IMETHOD GetBidiProperty(nsIPresContext* aPresContext,
                             nsIAtom*        aPropertyName,
                             void**          aPropertyValue,
                             size_t          aSize ) const;
  NS_IMETHOD SetBidiProperty(nsIPresContext* aPresContext,
                             nsIAtom*        aPropertyName,
                             void*           aPropertyValue) ;
#endif // IBMBIDI

#ifdef NS_DEBUG
  /**
   * Tracing method that writes a method enter/exit routine to the
   * nspr log using the nsIFrame log module. The tracing is only
   * done when the NS_FRAME_TRACE_CALLS bit is set in the log module's
   * level field.
   */
  void Trace(const char* aMethod, PRBool aEnter);
  void Trace(const char* aMethod, PRBool aEnter, nsReflowStatus aStatus);
  void TraceMsg(const char* fmt, ...);

  // Helper function that verifies that each frame in the list has the
  // NS_FRAME_IS_DIRTY bit set
  static void VerifyDirtyBitSet(nsIFrame* aFrameList);

  void ListTag(FILE* out) const {
    ListTag(out, (nsIFrame*)this);
  }

  static void ListTag(FILE* out, nsIFrame* aFrame) {
    nsAutoString tmp;
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(aFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      frameDebug->GetFrameName(tmp);
    }
    fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);
    fprintf(out, "@%p", NS_STATIC_CAST(void*, aFrame));
  }

  static void IndentBy(FILE* out, PRInt32 aIndent) {
    while (--aIndent >= 0) fputs("  ", out);
  }
  
  /**
   * Dump out the "base classes" regression data. This should dump
   * out the interior data, not the "frame" XML container. And it
   * should call the base classes same named method before doing
   * anything specific in a derived class. This means that derived
   * classes need not override DumpRegressionData unless they need
   * some custom behavior that requires changing how the outer "frame"
   * XML container is dumped.
   */
  virtual void DumpBaseRegressionData(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent, PRBool aIncludeStyleData);
  
  nsresult MakeFrameName(const nsAString& aKind, nsAString& aResult) const;

  // Display Reflow Debugging 
  static void* DisplayReflowEnter(nsIFrame*                aFrame,
                                  const nsHTMLReflowState& aReflowState);
  static void  DisplayReflowExit(nsIFrame*            aFrame,
                                 nsHTMLReflowMetrics& aMetrics,
                                 PRUint32             aStatus,
                                 void*                aFrameTreeNode);
#endif

protected:
  // Protected constructor and destructor
  nsFrame();
  virtual ~nsFrame();

  PRInt16 DisplaySelection(nsIPresContext* aPresContext, PRBool isOkToTurnOn = PR_FALSE);
  
  //this will modify aPos and return the next frame ect.
  NS_IMETHOD GetFrameFromDirection(nsIPresContext* aPresContext, nsPeekOffsetStruct *aPos);

  // Style post processing hook
  NS_IMETHOD DidSetStyleContext(nsIPresContext* aPresContext);

  // Helper routine for determining whether to print selection
  nsresult GetSelectionForVisCheck(nsIPresContext * aPresContext, nsISelection** aSelection);

  //return the line number of the aFrame
  static PRInt32 GetLineNumber(nsIFrame *aFrame);
  //given a frame five me the first/last leaf available
  static void GetLastLeaf(nsIPresContext* aPresContext, nsIFrame **aFrame);
  static void GetFirstLeaf(nsIPresContext* aPresContext, nsIFrame **aFrame);

  // Test if we are selecting a table object:
  //  Most table/cell selection requires that Ctrl (Cmd on Mac) key is down 
  //   during a mouse click or drag. Exception is using Shift+click when
  //   already in "table/cell selection mode" to extend a block selection
  //  Get the parent content node and offset of the frame 
  //   of the enclosing cell or table (if not inside a cell)
  //  aTarget tells us what table element to select (currently only cell and table supported)
  //  (enums for this are defined in nsIFrame.h)
  NS_IMETHOD GetDataForTableSelection(nsIFrameSelection *aFrameSelection, nsMouseEvent *aMouseEvent, 
                                      nsIContent **aParentContent, PRInt32 *aContentOffset, PRInt32 *aTarget);

  static void XMLQuote(nsString& aString);

  // Helper function that sets the view manager's default background to be
  // the background of this frame.
  void SetDefaultBackgroundColor(nsIPresContext* aPresContext);

  virtual PRBool ParentDisablesSelection() const;

  // Set the overflow clip rect into the rendering-context. Used for block-level
  // elements and replaced elements that have 'overflow' set to 'hidden'. This
  // member function assumes that the caller has checked that the clip property
  // applies to its situation.
  void SetOverflowClipRect(nsIRenderingContext& aRenderingContext);

protected:
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
};

// Start Display Reflow Debuggin
#ifdef DEBUG

  struct DR_cookie {
    DR_cookie(nsIFrame*                aFrame, 
              const nsHTMLReflowState& mReflowState,
              nsHTMLReflowMetrics&     aMetrics,
              nsReflowStatus&          aStatus);     
    ~DR_cookie();

    nsIFrame*                mFrame;
    const nsHTMLReflowState& mReflowState;
    nsHTMLReflowMetrics&     mMetrics;
    nsReflowStatus&          mStatus;    
    void*                    mValue;
  };
  
#define DISPLAY_REFLOW(dr_frame, dr_rf_state, dr_rf_metrics, dr_rf_status) \
  DR_cookie dr_cookie(dr_frame, dr_rf_state, dr_rf_metrics, dr_rf_status); 

#else

#define DISPLAY_REFLOW(dr_frame, dr_rf_state, dr_rf_metrics, dr_rf_status) 
  
#endif
// End Display Reflow Debugging

#endif /* nsFrame_h___ */
