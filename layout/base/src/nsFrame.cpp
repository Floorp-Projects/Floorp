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
#include "nsFrame.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "nsIStyleContext.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIPresContext.h"
#include "nsCRT.h"
#include "nsGUIEvent.h"
#include "nsStyleConsts.h"

static PRBool gShowFrameBorders = PR_FALSE;

NS_LAYOUT void nsIFrame::ShowFrameBorders(PRBool aEnable)
{
  gShowFrameBorders = aEnable;
}

NS_LAYOUT PRBool nsIFrame::GetShowFrameBorders()
{
  return gShowFrameBorders;
}

static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);

nsresult
nsFrame::NewFrame(nsIFrame** aInstancePtrResult,
                  nsIContent* aContent,
                  PRInt32     aIndexInParent,
                  nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsFrame(aContent, aIndexInParent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

void* nsFrame::operator new(size_t size)
{
  void* result = new char[size];

  nsCRT::zero(result, size);
  return result;
}

nsFrame::nsFrame(nsIContent* aContent,
                 PRInt32     aIndexInParent,
                 nsIFrame*   aParent)
  : mContent(aContent), mIndexInParent(aIndexInParent), mContentParent(aParent),
    mGeometricParent(aParent)
{
  NS_ADDREF(mContent);
}

nsFrame::~nsFrame()
{
  NS_RELEASE(mContent);
  NS_IF_RELEASE(mStyleContext);
  if (nsnull != mView) {
    // Break association between view and frame
    mView->SetFrame(nsnull);
    NS_RELEASE(mView);
  }
}

/////////////////////////////////////////////////////////////////////////////
// nsISupports

nsresult nsFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, kIFrameIID);
  if (aIID.Equals(kClassIID) || (aIID.Equals(kISupportsIID))) {
    *aInstancePtr = (void*)this;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsrefcnt nsFrame::AddRef(void)
{
  NS_ERROR("not supported");
  return 0;
}

nsrefcnt nsFrame::Release(void)
{
  NS_ERROR("not supported");
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
// nsIFrame

NS_METHOD nsFrame::DeleteFrame()
{
  nsIView* view = mView;
  if (nsnull == view) {
    nsIFrame* parent;
     
    GetParentWithView(parent);
    if (nsnull != parent) {
      parent->GetView(view);
    }
  }
  if (nsnull != view) {
    nsIViewManager* vm = view->GetViewManager();
    nsIPresContext* cx = vm->GetPresContext();
    // XXX Is this a really good ordering for the releases? MMP
    NS_RELEASE(vm);
    NS_RELEASE(view);
    cx->StopLoadImage(this);
    NS_RELEASE(cx);
  }

  delete this;
  return NS_OK;
}

NS_METHOD nsFrame::GetContent(nsIContent*& aContent) const
{
  if (nsnull != mContent) {
    NS_ADDREF(mContent);
  }
  aContent = mContent;
  return NS_OK;
}

NS_METHOD nsFrame::GetIndexInParent(PRInt32& aIndexInParent) const
{
  aIndexInParent = mIndexInParent;
  return NS_OK;
}

NS_METHOD nsFrame::SetIndexInParent(PRInt32 aIndexInParent)
{
  mIndexInParent = aIndexInParent;
  return NS_OK;
}

NS_METHOD nsFrame::GetStyleContext(nsIPresContext*   aPresContext,
                                   nsIStyleContext*& aStyleContext)
{
  if ((nsnull == mStyleContext) && (nsnull != aPresContext)) {
    mStyleContext = aPresContext->ResolveStyleContextFor(mContent, mGeometricParent); // XXX should be content parent???
  }
  NS_IF_ADDREF(mStyleContext);
  aStyleContext = mStyleContext;
  return NS_OK;
}

NS_METHOD nsFrame::SetStyleContext(nsIStyleContext* aContext)
{
  NS_PRECONDITION(nsnull != aContext, "null ptr");
  if (aContext != mStyleContext) {
    NS_IF_RELEASE(mStyleContext);
    if (nsnull != aContext) {
      mStyleContext = aContext;
      NS_ADDREF(aContext);
    }
  }

  return NS_OK;
}

NS_METHOD nsFrame::GetStyleData(const nsIID& aSID, nsStyleStruct*& aStyleStruct)
{
  NS_ASSERTION(mStyleContext!=nsnull,"null style context");
  if (mStyleContext) {
    aStyleStruct = mStyleContext->GetData(aSID);
  } else {
    aStyleStruct = nsnull;
  }
  return NS_OK;
}

// Geometric and content parent member functions

NS_METHOD nsFrame::GetContentParent(nsIFrame*& aParent) const
{
  aParent = mContentParent;
  return NS_OK;
}

NS_METHOD nsFrame::SetContentParent(const nsIFrame* aParent)
{
  mContentParent = (nsIFrame*)aParent;
  return NS_OK;
}

NS_METHOD nsFrame::GetGeometricParent(nsIFrame*& aParent) const
{
  aParent = mGeometricParent;
  return NS_OK;
}

NS_METHOD nsFrame::SetGeometricParent(const nsIFrame* aParent)
{
  mGeometricParent = (nsIFrame*)aParent;
  return NS_OK;
}

// Bounding rect member functions

NS_METHOD nsFrame::GetRect(nsRect& aRect) const
{
  aRect = mRect;
  return NS_OK;
}

NS_METHOD nsFrame::GetOrigin(nsPoint& aPoint) const
{
  aPoint.x = mRect.x;
  aPoint.y = mRect.y;
  return NS_OK;
}

NS_METHOD nsFrame::GetSize(nsSize& aSize) const
{
  aSize.width = mRect.width;
  aSize.height = mRect.height;
  return NS_OK;
}

NS_METHOD nsFrame::SetRect(const nsRect& aRect)
{
  MoveTo(aRect.x, aRect.y);
  SizeTo(aRect.width, aRect.height);
  return NS_OK;
}

NS_METHOD nsFrame::MoveTo(nscoord aX, nscoord aY)
{
  if ((aX != mRect.x) || (aY != mRect.y)) {
    mRect.x = aX;
    mRect.y = aY;

    // Let the view know
    if (nsnull != mView) {
      mView->SetPosition(aX, aY);
    }
  }

  return NS_OK;
}

NS_METHOD nsFrame::SizeTo(nscoord aWidth, nscoord aHeight)
{
  //I have commented out this if test since we really need to
  //always pass the fact that a resize was attempted to the
  //view system. Today is 23-apr-98. If this change still looks
  //good on or after 23-may-98, I'll kill the commented code
  //altogether. Pretty scientific, huh? MMP

//  if ((aWidth != mRect.width) || (aHeight != mRect.height)) {
    mRect.width = aWidth;
    mRect.height = aHeight;

    // Let the view know
    if (nsnull != mView) {
      mView->SetDimensions(aWidth, aHeight);
    }
//  }

  return NS_OK;
}

// Child frame enumeration

NS_METHOD nsFrame::ChildCount(PRInt32& aChildCount) const
{
  aChildCount = 0;
  return NS_OK;
}

NS_METHOD nsFrame::ChildAt(PRInt32 aIndex, nsIFrame*& aFrame) const
{
  NS_ERROR("not a container");
  aFrame = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::IndexOf(const nsIFrame* aChild, PRInt32& aIndex) const
{
  NS_ERROR("not a container");
  aIndex = -1;
  return NS_OK;
}

NS_METHOD nsFrame::FirstChild(nsIFrame*& aFirstChild) const
{
  aFirstChild = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::NextChild(const nsIFrame* aChild, nsIFrame*& aNextChild) const
{
  NS_ERROR("not a container");
  aNextChild = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::PrevChild(const nsIFrame* aChild, nsIFrame*& aPrevChild) const
{
  NS_ERROR("not a container");
  aPrevChild = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::LastChild(nsIFrame*& aLastChild) const
{
  aLastChild = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::Paint(nsIPresContext&      aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect&        aDirtyRect)
{
  return NS_OK;
}

NS_METHOD nsFrame::HandleEvent(nsIPresContext& aPresContext, 
                               nsGUIEvent*     aEvent,
                               nsEventStatus&  aEventStatus)
{
  aEventStatus = nsEventStatus_eIgnore;
  return NS_OK;
}

NS_METHOD nsFrame::GetCursorAt(nsIPresContext& aPresContext,
                               const nsPoint&  aPoint,
                               nsIFrame**      aFrame,
                               PRInt32&        aCursor)
{
  aCursor = NS_STYLE_CURSOR_INHERIT;
  return NS_OK;
}

// Resize and incremental reflow

NS_METHOD nsFrame::ResizeReflow(nsIPresContext*  aPresContext,
                                nsReflowMetrics& aDesiredSize,
                                const nsSize&    aMaxSize,
                                nsSize*          aMaxElementSize,
                                ReflowStatus&    aStatus)
{
  aDesiredSize.width = 0;
  aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  aDesiredSize.descent = 0;
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->width = 0;
    aMaxElementSize->height = 0;
  }
  aStatus = frComplete;
  return NS_OK;
}

NS_METHOD nsFrame::JustifyReflow(nsIPresContext* aPresContext,
                                 nscoord         aAvailableSpace)
{
  return NS_OK;
}

NS_METHOD nsFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                                     nsReflowMetrics& aDesiredSize,
                                     const nsSize&    aMaxSize,
                                     nsReflowCommand& aReflowCommand,
                                     ReflowStatus&    aStatus)
{
  NS_ERROR("not a reflow command handler");
  aDesiredSize.width = 0;
  aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  aDesiredSize.descent = 0;
  aStatus = frComplete;
  return NS_OK;
}

NS_METHOD nsFrame::ContentAppended(nsIPresShell*   aShell,
                                   nsIPresContext* aPresContext,
                                   nsIContent*     aContainer)
{
  return NS_OK;
}

NS_METHOD nsFrame::ContentInserted(nsIPresShell*   aShell,
                                   nsIPresContext* aPresContext,
                                   nsIContent*     aContainer,
                                   nsIContent*     aChild,
                                   PRInt32         aIndexInParent)
{
  return NS_OK;
}

NS_METHOD nsFrame::ContentReplaced(nsIPresShell*   aShell,
                                   nsIPresContext* aPresContext,
                                   nsIContent*     aContainer,
                                   nsIContent*     aOldChild,
                                   nsIContent*     aNewChild,
                                   PRInt32         aIndexInParent)
{
  return NS_OK;
}

NS_METHOD nsFrame::ContentDeleted(nsIPresShell*   aShell,
                                  nsIPresContext* aPresContext,
                                  nsIContent*     aContainer,
                                  nsIContent*     aChild,
                                  PRInt32         aIndexInParent)
{
  return NS_OK;
}

NS_METHOD nsFrame::GetReflowMetrics(nsIPresContext*  aPresContext,
                                    nsReflowMetrics& aMetrics)
{
  aMetrics.width = mRect.width;
  aMetrics.height = mRect.height;
  aMetrics.ascent = mRect.height;
  aMetrics.descent = 0;
  return NS_OK;
}

// Flow member functions

NS_METHOD nsFrame::IsSplittable(SplittableType& aIsSplittable) const
{
  aIsSplittable = frNotSplittable;
  return NS_OK;
}

NS_METHOD nsFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                         nsIFrame*       aParent,
                                         nsIFrame*&      aContinuingFrame)
{
  NS_ERROR("not splittable");
  aContinuingFrame = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::GetPrevInFlow(nsIFrame*& aPrevInFlow) const
{
  aPrevInFlow = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::SetPrevInFlow(nsIFrame*)
{
  NS_ERROR("not splittable");
  return NS_OK;
}

NS_METHOD nsFrame::GetNextInFlow(nsIFrame*& aNextInFlow) const
{
  aNextInFlow = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::SetNextInFlow(nsIFrame*)
{
  NS_ERROR("not splittable");
  return NS_OK;
}

NS_METHOD nsFrame::AppendToFlow(nsIFrame* aAfterFrame)
{
  NS_ERROR("not splittable");
  return NS_OK;
}

NS_METHOD nsFrame::PrependToFlow(nsIFrame* aBeforeFrame)
{
  NS_ERROR("not splittable");
  return NS_OK;
}

NS_METHOD nsFrame::RemoveFromFlow()
{
  NS_ERROR("not splittable");
  return NS_OK;
}

NS_METHOD nsFrame::BreakFromPrevFlow()
{
  NS_ERROR("not splittable");
  return NS_OK;
}

NS_METHOD nsFrame::BreakFromNextFlow()
{
  NS_ERROR("not splittable");
  return NS_OK;
}

// Associated view object
NS_METHOD nsFrame::GetView(nsIView*& aView) const
{
  aView = mView;
  NS_IF_ADDREF(aView);
  return NS_OK;
}

NS_METHOD nsFrame::SetView(nsIView* aView)
{
  NS_IF_RELEASE(mView);
  if (nsnull != aView) {
    mView = aView;
    aView->SetFrame(this);
    NS_ADDREF(aView);
  }
  return NS_OK;
}

// Find the first geometric parent that has a view
NS_METHOD nsFrame::GetParentWithView(nsIFrame*& aParent) const
{
  aParent = mGeometricParent;

  while (nsnull != aParent) {
    nsIView* parView;
     
    aParent->GetView(parView);
    if (nsnull != parView) {
      NS_RELEASE(parView);
      break;
    }
    aParent->GetGeometricParent(aParent);
  }

  return NS_OK;
}

// Returns the offset from this frame to the closest geometric parent that
// has a view. Also returns the containing view or null in case of error
NS_METHOD nsFrame::GetOffsetFromView(nsPoint& aOffset, nsIView*& aView) const
{
  nsIFrame* frame = (nsIFrame*)this;

  aView = nsnull;
  aOffset.MoveTo(0, 0);
  do {
    nsPoint origin;

    frame->GetOrigin(origin);
    aOffset += origin;
    frame->GetGeometricParent(frame);
    if (nsnull != frame) {
      frame->GetView(aView);
    }
  } while ((nsnull != frame) && (nsnull == aView));

  return NS_OK;
}

NS_METHOD nsFrame::GetWindow(nsIWidget*& aWindow) const
{
  nsIFrame* frame = (nsIFrame*)this;

  aWindow = nsnull;
  while (nsnull != frame) {
    nsIView* view;
     
    frame->GetView(view);
    if (nsnull != view) {
      aWindow = view->GetWidget();
      NS_RELEASE(view);
      if (nsnull != aWindow) {
        break;
      }
    }
    frame->GetParentWithView(frame);
  }
  NS_POSTCONDITION(nsnull != aWindow, "no window in frame tree");
  return NS_OK;
}

// Sibling pointer used to link together frames

NS_METHOD nsFrame::GetNextSibling(nsIFrame*& aNextSibling) const
{
  aNextSibling = mNextSibling;
  return NS_OK;
}

NS_METHOD nsFrame::SetNextSibling(nsIFrame* aNextSibling)
{
  mNextSibling = aNextSibling;
  return NS_OK;
}

// Debugging
NS_METHOD nsFrame::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);

  // Output the tag and rect
  ListTag(out);
  fputs(" ", out);
  out << mRect;
  fputs("<>\n", out);
  return NS_OK;
}

// Output the frame's tag
NS_METHOD nsFrame::ListTag(FILE* out) const
{
  nsIAtom* tag = mContent->GetTag();
  if (tag != nsnull) {
    nsAutoString buf;
    tag->ToString(buf);
    fputs(buf, out);
    NS_RELEASE(tag);
  }
  fprintf(out, "(%d)@%p", mIndexInParent, this);
  return NS_OK;
}

NS_METHOD nsFrame::VerifyTree() const
{
  return NS_OK;
}
