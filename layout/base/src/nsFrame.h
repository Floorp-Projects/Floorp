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
#include "nsRect.h"
#include "nsISelection.h"
#include "nsSelectionRange.h"
#include "nsSelectionPoint.h"
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
#define NS_FRAME_TRACE_IN(_method)
#define NS_FRAME_TRACE_OUT(_method)
#define NS_FRAME_TRACE_MSG(_bits,_args)
#define NS_FRAME_TRACE_REFLOW_IN(_method)
#define NS_FRAME_TRACE_REFLOW_OUT(_method, _status)
#endif

//----------------------------------------------------------------------

// Implementation of a simple frame with no children and that isn't splittable
class nsFrame : public nsIFrame
{
public:
  /**
   * Create a new "empty" frame that maps a given piece of content into a
   * 0,0 area.
   */
  static nsresult NewFrame(nsIFrame**  aInstancePtrResult,
                           nsIContent* aContent,
                           nsIFrame*   aParent);

  // Overloaded new operator. Initializes the memory to 0
  void* operator new(size_t size);

  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD  DeleteFrame();
  NS_IMETHOD  SizeOf(nsISizeOfHandler* aHandler) const;
  NS_IMETHOD  GetContent(nsIContent*& aContent) const;
  NS_IMETHOD  GetContentIndex(PRInt32& aIndexInParent) const;
  NS_IMETHOD  GetStyleContext(nsIPresContext* aContext,
                              nsIStyleContext*& aStyleContext);
  NS_IMETHOD  SetStyleContext(nsIPresContext* aPresContext,
                              nsIStyleContext* aContext);
  NS_IMETHOD  GetStyleData(nsStyleStructID aSID,
                           const nsStyleStruct*& aStyleStruct) const;
  NS_IMETHOD  GetContentParent(nsIFrame*& aParent) const;
  NS_IMETHOD  SetContentParent(const nsIFrame* aParent);
  NS_IMETHOD  GetGeometricParent(nsIFrame*& aParent) const;
  NS_IMETHOD  SetGeometricParent(const nsIFrame* aParent);
  NS_IMETHOD  GetRect(nsRect& aRect) const;
  NS_IMETHOD  GetOrigin(nsPoint& aPoint) const;
  NS_IMETHOD  GetSize(nsSize& aSize) const;
  NS_IMETHOD  SetRect(const nsRect& aRect);
  NS_IMETHOD  MoveTo(nscoord aX, nscoord aY);
  NS_IMETHOD  SizeTo(nscoord aWidth, nscoord aHeight);
  NS_IMETHOD  ChildCount(PRInt32& aChildCount) const;
  NS_IMETHOD  ChildAt(PRInt32 aIndex, nsIFrame*& aFrame) const;
  NS_IMETHOD  IndexOf(const nsIFrame* aChild, PRInt32& aIndex) const;
  NS_IMETHOD  FirstChild(nsIFrame*& aFirstChild) const;
  NS_IMETHOD  NextChild(const nsIFrame* aChild, nsIFrame*& aNextChild) const;
  NS_IMETHOD  PrevChild(const nsIFrame* aChild, nsIFrame*& aPrevChild) const;
  NS_IMETHOD  LastChild(nsIFrame*& aLastChild) const;
  NS_IMETHOD  Paint(nsIPresContext&      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect);
  NS_IMETHOD  HandleEvent(nsIPresContext& aPresContext, 
                          nsGUIEvent*     aEvent,
                          nsEventStatus&  aEventStatus);
  NS_IMETHOD  GetCursorAt(nsIPresContext& aPresContext,
                          const nsPoint&  aPoint,
                          nsIFrame**      aFrame,
                          PRInt32&        aCursor);
  NS_IMETHOD  GetFrameState(nsFrameState& aResult);
  NS_IMETHOD  SetFrameState(nsFrameState aNewState);
  NS_IMETHOD  WillReflow(nsIPresContext& aPresContext);
  NS_IMETHOD  DidReflow(nsIPresContext& aPresContext,
                        nsDidReflowStatus aStatus);
  NS_IMETHOD  Reflow(nsIPresContext*      aPresContext,
                     nsReflowMetrics&     aDesiredSize,
                     const nsReflowState& aReflowState,
                     nsReflowStatus&      aStatus);
  NS_IMETHOD  ContentAppended(nsIPresShell*   aShell,
                              nsIPresContext* aPresContext,
                              nsIContent*     aContainer);
  NS_IMETHOD  ContentInserted(nsIPresShell*   aShell,
                              nsIPresContext* aPresContext,
                              nsIContent*     aContainer,
                              nsIContent*     aChild,
                              PRInt32         aIndexInParent);
  NS_IMETHOD  ContentReplaced(nsIPresShell*   aShell,
                              nsIPresContext* aPresContext,
                              nsIContent*     aContainer,
                              nsIContent*     aOldChild,
                              nsIContent*     aNewChild,
                              PRInt32         aIndexInParent);
  NS_IMETHOD  ContentDeleted(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInParent);
  NS_IMETHOD  ContentChanged(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aChild,
                             nsISupports*    aSubContent);
  NS_IMETHOD  GetReflowMetrics(nsIPresContext*  aPresContext,
                               nsReflowMetrics& aMetrics);
  NS_IMETHOD  IsSplittable(nsSplittableType& aIsSplittable) const;
  NS_IMETHOD  CreateContinuingFrame(nsIPresContext*  aPresContext,
                                    nsIFrame*        aParent,
                                    nsIStyleContext* aStyleContext,
                                    nsIFrame*&       aContinuingFrame);
  NS_IMETHOD  GetPrevInFlow(nsIFrame*& aPrevInFlow) const;
  NS_IMETHOD  SetPrevInFlow(nsIFrame*);
  NS_IMETHOD  GetNextInFlow(nsIFrame*& aNextInFlow) const;
  NS_IMETHOD  SetNextInFlow(nsIFrame*);
  NS_IMETHOD  AppendToFlow(nsIFrame* aAfterFrame);
  NS_IMETHOD  PrependToFlow(nsIFrame* aAfterFrame);
  NS_IMETHOD  RemoveFromFlow();
  NS_IMETHOD  BreakFromPrevFlow();
  NS_IMETHOD  BreakFromNextFlow();
  NS_IMETHOD  GetView(nsIView*& aView) const;
  NS_IMETHOD  SetView(nsIView* aView);
  NS_IMETHOD  GetParentWithView(nsIFrame*& aParent) const;
  NS_IMETHOD  GetOffsetFromView(nsPoint& aOffset, nsIView*& aView) const;
  NS_IMETHOD  GetWindow(nsIWidget*&) const;
  NS_IMETHOD  IsPercentageBase(PRBool& aBase) const;
  NS_IMETHOD  GetAutoMarginSize(PRUint8 aSide, nscoord& aSize) const;
  NS_IMETHOD  GetNextSibling(nsIFrame*& aNextSibling) const;
  NS_IMETHOD  SetNextSibling(nsIFrame* aNextSibling);
  NS_IMETHOD  IsTransparent(PRBool& aTransparent) const;
  NS_IMETHOD  List(FILE* out = stdout, PRInt32 aIndent = 0) const;
  NS_IMETHOD  ListTag(FILE* out = stdout) const;
  NS_IMETHOD  VerifyTree() const;

  //--------------------------------------------------
  // Additional methods

  // Invalidate part of the frame by asking the view manager to repaint.
  // aDamageRect is in the frame's local coordinate space
  void        Invalidate(const nsRect& aDamageRect) const;

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

protected:
    // Selection Methods
  NS_METHOD HandlePress(nsIPresContext& aPresContext,
                   nsGUIEvent *    aEvent,
                   nsEventStatus&  aEventStatus,
                   nsFrame  *      aFrame);

  NS_METHOD HandleDrag(nsIPresContext& aPresContext,
                   nsGUIEvent *    aEvent,
                   nsEventStatus&  aEventStatus,
                   nsFrame  *      aFrame);

  NS_METHOD HandleRelease(nsIPresContext& aPresContext,
                   nsGUIEvent *    aEvent,
                   nsEventStatus&  aEventStatus,
                   nsFrame  *      aFrame);

  virtual void NewContentIsBefore(nsIPresContext& aPresContext,
                          nsGUIEvent * aEvent,
                          nsIContent * aNewContent,
                          nsIContent * aCurrentContent,
                          nsIFrame   * aNewFrame);

  virtual void NewContentIsAfter(nsIPresContext& aPresContext,
                         nsGUIEvent * aEvent,
                         nsIContent * aNewContent,
                         nsIContent * aCurrentContent,
                         nsIFrame   * aNewFrame);

  virtual PRInt32 GetPosition(nsIPresContext& aPresContext,
                              nsGUIEvent*     aEvent,
                              nsIFrame *      aNewFrame);

  virtual void AdjustPointsInNewContent(nsIPresContext& aPresContext,
                                nsGUIEvent    * aEvent,
                                nsIFrame       * aNewFrame);

  virtual void AdjustPointsInSameContent(nsIPresContext& aPresContext,
                                 nsGUIEvent    * aEvent);


  // Style post processing hook
  NS_IMETHOD DidSetStyleContext(nsIPresContext* aPresContext);



  // Constructor. Takes as arguments the content object, the index in parent,
  // and the Frame for the content parent
  nsFrame(nsIContent* aContent, nsIFrame* aParent);

  virtual ~nsFrame();

  void SizeOfWithoutThis(nsISizeOfHandler* aHandler) const;

  nsRect           mRect;
  nsIContent*      mContent;
  nsIStyleContext* mStyleContext;
  nsIFrame*        mContentParent;
  nsIFrame*        mGeometricParent;
  nsIFrame*        mNextSibling;  // singly linked list of frames
  nsFrameState     mState;

  ///////////////////////////////////
  // Important Selection Variables
  ///////////////////////////////////
  static nsIFrame * mCurrentFrame;
  static PRBool     mDoingSelection;
  static PRBool     mDidDrag;
  static PRInt32    mStartPos;

  // Selection data is valid only from the Mouse Press to the Mouse Release
  static nsSelectionRange * mSelectionRange;
  static nsISelection     * mSelection;

  static nsSelectionPoint * mStartSelectionPoint;
  static nsSelectionPoint * mEndSelectionPoint;
  ///////////////////////////////////


private:
  nsIView*         mView;  // must use accessor member functions

  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
};

#endif /* nsFrame_h___ */
