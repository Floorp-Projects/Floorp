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
#ifndef nsFrame_h___
#define nsFrame_h___

#include "nsIFrame.h"
#include "nsIHTMLReflow.h"
#include "nsRect.h"
#include "nsString.h"
#include "prlog.h"

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
    if (NS_FRAME_LOG_TEST(nsIFrame::GetLogModuleInfo(),_bit)) { \
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
    if (NS_FRAME_LOG_TEST(nsIFrame::GetLogModuleInfo(),_bit)) { \
      TraceMsg _args;                                           \
    }                                                           \
  PR_END_MACRO

#define NS_FRAME_TRACE(_bit,_args)                              \
  PR_BEGIN_MACRO                                                \
    if (NS_FRAME_LOG_TEST(nsIFrame::GetLogModuleInfo(),_bit)) { \
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
class nsFrame : public nsIFrame, public nsIHTMLReflow
{
public:
  /**
   * Create a new "empty" frame that maps a given piece of content into a
   * 0,0 area.
   */
  friend nsresult NS_NewEmptyFrame(nsIFrame** aInstancePtrResult);

  // Overloaded new operator. Initializes the memory to 0
  NS_DECL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD  Init(nsIPresContext&  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        asPrevInFlow);
  NS_IMETHOD  SetInitialChildList(nsIPresContext& aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList);
  NS_IMETHOD  AppendFrames(nsIPresContext& aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  InsertFrames(nsIPresContext& aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  RemoveFrame(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);
  NS_IMETHOD  DeleteFrame(nsIPresContext& aPresContext);
  NS_IMETHOD  GetContent(nsIContent** aContent) const;
  NS_IMETHOD  GetStyleContext(nsIStyleContext** aStyleContext) const;
  NS_IMETHOD  SetStyleContext(nsIPresContext* aPresContext,
                              nsIStyleContext* aContext);
  NS_IMETHOD  GetStyleData(nsStyleStructID aSID,
                           const nsStyleStruct*& aStyleStruct) const;
  NS_IMETHOD  ReResolveStyleContext(nsIPresContext* aPresContext,
                                    nsIStyleContext* aParentContext,
                                    PRInt32 aParentChange,
                                    nsStyleChangeList* aChangeList,
                                    PRInt32* aLocalChange);
  NS_IMETHOD  GetParent(nsIFrame** aParent) const;
  NS_IMETHOD  SetParent(const nsIFrame* aParent);
  NS_IMETHOD  GetRect(nsRect& aRect) const;
  NS_IMETHOD  GetOrigin(nsPoint& aPoint) const;
  NS_IMETHOD  GetSize(nsSize& aSize) const;
  NS_IMETHOD  SetRect(const nsRect& aRect);
  NS_IMETHOD  MoveTo(nscoord aX, nscoord aY);
  NS_IMETHOD  SizeTo(nscoord aWidth, nscoord aHeight);
  NS_IMETHOD  GetAdditionalChildListName(PRInt32 aIndex, nsIAtom** aListName) const;
  NS_IMETHOD  FirstChild(nsIAtom* aListName, nsIFrame** aFirstChild) const;
  NS_IMETHOD  Paint(nsIPresContext&      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer);
  NS_IMETHOD  HandleEvent(nsIPresContext& aPresContext, 
                          nsGUIEvent*     aEvent,
                          nsEventStatus&  aEventStatus);
  NS_IMETHOD  GetCursor(nsIPresContext& aPresContext,
                        nsPoint&        aPoint,
                        PRInt32&        aCursor);
  NS_IMETHOD  GetFrameForPoint(const nsPoint& aPoint, 
                              nsIFrame**     aFrame);

  NS_IMETHOD  GetPointFromOffset(nsIPresContext*        inPresContext,
                                 nsIRenderingContext*   inRendContext,
                                 PRInt32                inOffset,
                                 nsPoint*               outPoint);

  NS_IMETHOD  GetChildFrameContainingOffset(PRInt32     inContentOffset,
                                 PRInt32*               outFrameContentOffset,
                                 nsIFrame*              *outChildFrame);

  NS_IMETHOD  GetFrameState(nsFrameState* aResult);
  NS_IMETHOD  SetFrameState(nsFrameState aNewState);

  NS_IMETHOD  ContentChanged(nsIPresContext* aPresContext,
                             nsIContent*     aChild,
                             nsISupports*    aSubContent);
  NS_IMETHOD  AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent*     aChild,
                               nsIAtom*        aAttribute,
                               PRInt32         aHint);
  NS_IMETHOD  IsSplittable(nsSplittableType& aIsSplittable) const;
  NS_IMETHOD  GetPrevInFlow(nsIFrame** aPrevInFlow) const;
  NS_IMETHOD  SetPrevInFlow(nsIFrame*);
  NS_IMETHOD  GetNextInFlow(nsIFrame** aNextInFlow) const;
  NS_IMETHOD  SetNextInFlow(nsIFrame*);
  NS_IMETHOD  AppendToFlow(nsIFrame* aAfterFrame);
  NS_IMETHOD  PrependToFlow(nsIFrame* aAfterFrame);
  NS_IMETHOD  RemoveFromFlow();
  NS_IMETHOD  BreakFromPrevFlow();
  NS_IMETHOD  BreakFromNextFlow();
  NS_IMETHOD  GetView(nsIView** aView) const;
  NS_IMETHOD  SetView(nsIView* aView);
  NS_IMETHOD  GetParentWithView(nsIFrame** aParent) const;
  NS_IMETHOD  GetOffsetFromView(nsPoint& aOffset, nsIView** aView) const;
  NS_IMETHOD  GetWindow(nsIWidget**) const;
  NS_IMETHOD  GetFrameType(nsIAtom** aType) const;
  NS_IMETHOD  IsPercentageBase(PRBool& aBase) const;
  NS_IMETHOD  GetNextSibling(nsIFrame** aNextSibling) const;
  NS_IMETHOD  SetNextSibling(nsIFrame* aNextSibling);
  NS_IMETHOD  IsTransparent(PRBool& aTransparent) const;
  NS_IMETHOD  Scrolled(nsIView *aView);
  NS_IMETHOD  List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD  GetFrameName(nsString& aResult) const;
  NS_IMETHOD  DumpRegressionData(FILE* out, PRInt32 aIndent);
  NS_IMETHOD  VerifyTree() const;
  NS_IMETHOD  SetSelected(nsSelectionStruct *);
  NS_IMETHOD  SetSelectedContentOffsets(nsSelectionStruct *aSS, 
                                        nsIFocusTracker *aTracker,
                                        nsIFrame **aActualSelected);
  NS_IMETHOD  GetSelected(PRBool *aSelected, PRInt32 *aBeginOffset, PRInt32 *aEndOffset, PRInt32 *aBeginContentOffset);
  NS_IMETHOD  PeekOffset(nsSelectionAmount aAmount, nsDirection aDirection, PRInt32 aStartOffset, 
                           nsIFrame **aResultFrame, PRInt32 *aFrameOffset, PRInt32 *aContentOffset,
                           PRBool aEatingWS);

  NS_IMETHOD  GetOffsets(PRInt32 &aStart, PRInt32 &aEnd) const;

  // nsIHTMLReflow
  NS_IMETHOD  WillReflow(nsIPresContext& aPresContext);
  NS_IMETHOD  Reflow(nsIPresContext&          aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus);
  NS_IMETHOD  DidReflow(nsIPresContext& aPresContext,
                        nsDidReflowStatus aStatus);
  NS_IMETHOD FindTextRuns(nsLineLayout& aLineLayout);
  NS_IMETHOD AdjustFrameSize(nscoord aExtraSpace, nscoord& aUsedSpace);
  NS_IMETHOD TrimTrailingWhiteSpace(nsIPresContext* aPresContext,
                                    nsIRenderingContext& aRC,
                                    nscoord& aDeltaWidth);
  NS_IMETHOD MoveInSpaceManager(nsIPresContext* aPresContext,
                                nsISpaceManager* aSpaceManager,
                                nscoord aDeltaX, nscoord aDeltaY);

  // Selection Methods
  // XXX Doc me... (in nsIFrame.h puhleeze)
  // XXX If these are selection specific, then the name should imply selection
  // rather than generic event processing, e.g., SelectionHandlePress...
  NS_IMETHOD HandlePress(nsIPresContext& aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus&  aEventStatus);

  NS_IMETHOD HandleDrag(nsIPresContext& aPresContext,
                        nsGUIEvent *    aEvent,
                        nsEventStatus&  aEventStatus);

  NS_IMETHOD HandleRelease(nsIPresContext& aPresContext,
                           nsGUIEvent *    aEvent,
                           nsEventStatus&  aEventStatus);

  NS_IMETHOD GetPosition(nsIPresContext&       aPresContext,
                         nsIRenderingContext * aRendContext,
                         nsGUIEvent*           aEvent,
                         nsIFrame *            aNewFrame,
                         PRUint32&             aAcutalContentOffset,
                         PRInt32&              aOffset);

  //--------------------------------------------------
  // Additional methods

  // Invalidate part of the frame by asking the view manager to repaint.
  // aDamageRect is in the frame's local coordinate space
  void        Invalidate(const nsRect& aDamageRect,
                         PRBool aImmediate = PR_FALSE) const;

  // Helper function to return the index in parent of the frame's content
  // object. Returns -1 on error or if the frame doesn't have a content object
  static PRInt32 ContentIndexInContainer(const nsIFrame* aFrame);

  // Helper function to compute an capture style change information
  // call this when replacing style contexts within ReResilveStyleContext
  static void CaptureStyleChangeFor(nsIFrame* aFrame,
                                    nsIStyleContext* aOldContext, 
                                    nsIStyleContext* aNewContext,
                                    PRInt32 aParentChange,
                                    nsStyleChangeList* aChangeList,
                                    PRInt32* aLocalChange);

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
#endif

  void ListTag(FILE* out) const {
    ListTag(out, this);
  }

  static void ListTag(FILE* out, const nsIFrame* aFrame) {
    nsAutoString tmp;
    aFrame->GetFrameName(tmp);
    fputs(tmp, out);
    fprintf(out, "@%p", aFrame);
  }

  static void IndentBy(FILE* out, PRInt32 aIndent) {
    while (--aIndent >= 0) fputs("  ", out);
  }

protected:
  // Protected constructor and destructor
  nsFrame();
  virtual ~nsFrame();

  virtual void AdjustPointsInNewContent(nsIPresContext& aPresContext,
                                nsIRenderingContext * aRendContext,
                                nsGUIEvent    * aEvent,
                                nsIFrame       * aNewFrame);

  virtual void AdjustPointsInSameContent(nsIPresContext& aPresContext,
                                 nsIRenderingContext * aRendContext,
                                 nsGUIEvent    * aEvent);

  PRBool DisplaySelection(nsIPresContext& aPresContext, PRBool isOkToTurnOn = PR_FALSE);

  // Style post processing hook
  NS_IMETHOD DidSetStyleContext(nsIPresContext* aPresContext);

  /**
   * Dump out the "base classes" regression data. This should dump
   * out the interior data, not the "frame" XML container. And it
   * should call the base classes same named method before doing
   * anything specific in a derived class. This means that derived
   * classes need not override DumpRegressionData unless they need
   * some custom behavior that requires changing how the outer "frame"
   * XML container is dumped.
   */
  virtual void DumpBaseRegressionData(FILE* out, PRInt32 aIndent);

  nsresult MakeFrameName(const char* aKind, nsString& aResult) const;

  static void XMLQuote(nsString& aString);

  // Set the clip rect into the rendering-context after applying CSS's
  // clip property. This method assumes that the caller has checked
  // that the clip property applies to its situation.
  void SetClipRect(nsIRenderingContext& aRenderingContext);

  nsRect           mRect;
  nsIContent*      mContent;
  nsIStyleContext* mStyleContext;
  nsIFrame*        mParent;
  nsIFrame*        mNextSibling;  // singly linked list of frames
  nsFrameState     mState;
  PRBool           mSelected;

  ///////////////////////////////////
  // Important Selection Variables
  ///////////////////////////////////
  static PRBool     mDoingSelection;
  static PRBool     mDidDrag;
  static PRInt32    mStartPos;

  // Selection data is valid only from the Mouse Press to the Mouse Release
  ///////////////////////////////////


private:
  nsIView*         mView;  // must use accessor member functions
protected:
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
};

#endif /* nsFrame_h___ */
