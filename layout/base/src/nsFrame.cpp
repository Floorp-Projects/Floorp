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

void nsFrame::DeleteFrame()
{
  nsIView* view = mView;
  if (nsnull == view) {
    nsIFrame* parent = GetParentWithView();
    if (nsnull != parent) {
      view = parent->GetView();
    }
  }
  if (nsnull != view) {
    nsIViewManager* vm = view->GetViewManager();
    nsIPresContext* cx = vm->GetPresContext();
    //is this a really good ordering for the releases? MMP
    NS_RELEASE(vm);
    NS_RELEASE(view);
    cx->StopLoadImage(this);
    NS_RELEASE(cx);
  }

  delete this;
}

nsIContent* nsFrame::GetContent() const
{
  if (nsnull != mContent) {
    NS_ADDREF(mContent);
  }
  return mContent;
}

PRInt32 nsFrame::GetIndexInParent() const
{
  return mIndexInParent;
}

void nsFrame::SetIndexInParent(PRInt32 aIndexInParent)
{
  mIndexInParent = aIndexInParent;
}

nsIStyleContext* nsFrame::GetStyleContext(nsIPresContext* aPresContext)
{
  if (nsnull == mStyleContext) {
    mStyleContext = aPresContext->ResolveStyleContextFor(mContent, mGeometricParent); // XXX should be content parent???
  }
  NS_IF_ADDREF(mStyleContext);
  return mStyleContext;
}

void nsFrame::SetStyleContext(nsIStyleContext* aContext)
{
  NS_PRECONDITION(nsnull != aContext, "null ptr");
  if (aContext != mStyleContext) {
    NS_IF_RELEASE(mStyleContext);
    if (nsnull != aContext) {
      mStyleContext = aContext;
      NS_ADDREF(aContext);
    }
  }
}

// Geometric and content parent member functions

nsIFrame* nsFrame::GetContentParent() const
{
  return mContentParent;
}

void nsFrame::SetContentParent(const nsIFrame* aParent)
{
  mContentParent = (nsIFrame*)aParent;
}

nsIFrame* nsFrame::GetGeometricParent() const
{
  return mGeometricParent;
}

void nsFrame::SetGeometricParent(const nsIFrame* aParent)
{
  mGeometricParent = (nsIFrame*)aParent;
}

// Bounding rect member functions

nsRect nsFrame::GetRect() const
{
  return mRect;
}

void nsFrame::GetRect(nsRect& aRect) const
{
  aRect = mRect;
}

void nsFrame::GetOrigin(nsPoint& aPoint) const
{
  aPoint.x = mRect.x;
  aPoint.y = mRect.y;
}

nscoord nsFrame::GetWidth() const
{
  return mRect.width;
}

nscoord nsFrame::GetHeight() const
{
  return mRect.height;
}

void nsFrame::SetRect(const nsRect& aRect)
{
  MoveTo(aRect.x, aRect.y);
  SizeTo(aRect.width, aRect.height);
}

void nsFrame::MoveTo(nscoord aX, nscoord aY)
{
  if ((aX != mRect.x) || (aY != mRect.y)) {
    mRect.x = aX;
    mRect.y = aY;

    // Let the view know
    if (nsnull != mView) {
      mView->SetPosition(aX, aY);
    }
  }
}

void nsFrame::SizeTo(nscoord aWidth, nscoord aHeight)
{
  if ((aWidth != mRect.width) || (aHeight != mRect.height)) {
    mRect.width = aWidth;
    mRect.height = aHeight;

    // Let the view know
    if (nsnull != mView) {
      mView->SetDimensions(aWidth, aHeight);
    }
  }
}

// Child frame enumeration

PRInt32 nsFrame::ChildCount() const
{
  return 0;
}

nsIFrame* nsFrame::ChildAt(PRInt32 aIndex) const
{
  NS_ERROR("not a container");
  return nsnull;
}

PRInt32 nsFrame::IndexOf(const nsIFrame* aChild) const
{
  NS_ERROR("not a container");
  return -1;
}

nsIFrame* nsFrame::FirstChild() const
{
  return nsnull;
}

nsIFrame* nsFrame::NextChild(const nsIFrame* aChild) const
{
  NS_ERROR("not a container");
  return nsnull;
}

nsIFrame* nsFrame::PrevChild(const nsIFrame* aChild) const
{
  NS_ERROR("not a container");
  return nsnull;
}

nsIFrame* nsFrame::LastChild() const
{
  return nsnull;
}

void nsFrame::Paint(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect)
{
}

nsEventStatus nsFrame::HandleEvent(nsIPresContext& aPresContext, 
                            nsGUIEvent* aEvent)
{
  nsEventStatus  retval = nsEventStatus_eIgnore;
  return retval;
}

PRInt32 nsFrame::GetCursorAt(nsIPresContext& aPresContext,
                             const nsPoint& aPoint,
                             nsIFrame** aFrame)
{
  return NS_STYLE_CURSOR_INHERIT;
}

// Resize and incremental reflow

nsIFrame::ReflowStatus
nsFrame::ResizeReflow(nsIPresContext*  aPresContext,
                      nsReflowMetrics& aDesiredSize,
                      const nsSize&    aMaxSize,
                      nsSize*          aMaxElementSize)
{
  aDesiredSize.width = 0;
  aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  aDesiredSize.descent = 0;
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->width = 0;
    aMaxElementSize->height = 0;
  }
  return frComplete;
}

void
nsFrame::JustifyReflow(nsIPresContext* aPresContext,
                       nscoord aAvailableSpace)
{
}

nsIFrame::ReflowStatus
nsFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                           nsReflowMetrics& aDesiredSize,
                           const nsSize&    aMaxSize,
                           nsReflowCommand& aReflowCommand)
{
  NS_ERROR("not a reflow command handler");
  aDesiredSize.width = 0;
  aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  aDesiredSize.descent = 0;
  return frComplete;
}

void nsFrame::ContentAppended(nsIPresShell* aShell,
                              nsIPresContext* aPresContext,
                              nsIContent* aContainer)
{
}

void nsFrame::ContentInserted(nsIPresShell* aShell,
                              nsIPresContext* aPresContext,
                              nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInParent)
{
}

void nsFrame::ContentReplaced(nsIPresShell* aShell,
                              nsIPresContext* aPresContext,
                              nsIContent* aContainer,
                              nsIContent* aOldChild,
                              nsIContent* aNewChild,
                              PRInt32 aIndexInParent)
{
}

void nsFrame::ContentDeleted(nsIPresShell* aShell,
                             nsIPresContext* aPresContext,
                             nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInParent)
{
}

void nsFrame::GetReflowMetrics(nsIPresContext* aPresContext,
                               nsReflowMetrics& aMetrics)
{
  aMetrics.width = mRect.width;
  aMetrics.height = mRect.height;
  aMetrics.ascent = mRect.height;
  aMetrics.descent = 0;
}

// Flow member functions

PRBool nsFrame::IsSplittable() const
{
  return PR_FALSE;
}

nsIFrame* nsFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                         nsIFrame*       aParent)
{
  NS_ERROR("not splittable");
  return nsnull;
}

nsIFrame* nsFrame::GetPrevInFlow() const
{
  return nsnull;
}

void nsFrame::SetPrevInFlow(nsIFrame*)
{
  NS_ERROR("not splittable");
}

nsIFrame* nsFrame::GetNextInFlow() const
{
  return nsnull;
}

void nsFrame::SetNextInFlow(nsIFrame*)
{
  NS_ERROR("not splittable");
}

void nsFrame::AppendToFlow(nsIFrame* aAfterFrame)
{
  NS_ERROR("not splittable");
}

void nsFrame::PrependToFlow(nsIFrame* aBeforeFrame)
{
  NS_ERROR("not splittable");
}

void nsFrame::RemoveFromFlow()
{
  NS_ERROR("not splittable");
}

void nsFrame::BreakFromPrevFlow()
{
  NS_ERROR("not splittable");
}

void nsFrame::BreakFromNextFlow()
{
  NS_ERROR("not splittable");
}

// Associated view object
nsIView* nsFrame::GetView() const
{
  NS_IF_ADDREF(mView);
  return mView;
}

void nsFrame::SetView(nsIView* aView)
{
  NS_IF_RELEASE(mView);
  if (nsnull != aView) {
    mView = aView;
    aView->SetFrame(this);
    NS_ADDREF(aView);
  }
}

// Find the first geometric parent that has a view
nsIFrame* nsFrame::GetParentWithView() const
{
  nsIFrame* parent = mGeometricParent;

  while (nsnull != parent) {
    nsIView* parView = parent->GetView();
    if (nsnull != parView) {
      NS_RELEASE(parView);
      break;
    }
    parent = parent->GetGeometricParent();
  }

  return parent;
}

// Returns the offset from this frame to the closest geometric parent that
// has a view. Also returns the containing view or null in case of error
nsIView* nsFrame::GetOffsetFromView(nsPoint& aOffset) const
{
  const nsIFrame* frame = this;
  nsIView*  result = nsnull;
  nsPoint   origin;

  aOffset.MoveTo(0, 0);
  nsIView* view = nsnull;
  do {
    frame->GetOrigin(origin);
    aOffset += origin;
    frame = frame->GetGeometricParent();
    view = (frame == nsnull) ? nsnull : frame->GetView();
  } while ((nsnull != frame) && (nsnull == view));

  return view;
}

nsIWidget* nsFrame::GetWindow() const
{
  nsIWidget* window = nsnull;
  const nsIFrame* frame = this;
  while (nsnull != frame) {
    nsIView* view = frame->GetView();
    if (nsnull != view) {
      window = view->GetWidget();
      NS_RELEASE(view);
      if (nsnull != window) {
        return window;
      }
    }
    frame = frame->GetParentWithView();
  }
  NS_POSTCONDITION(nsnull != window, "no window in frame tree");
  return nsnull;
}

// Sibling pointer used to link together frames

nsIFrame* nsFrame::GetNextSibling() const
{
  return mNextSibling;
}

void nsFrame::SetNextSibling(nsIFrame* aNextSibling)
{
  mNextSibling = aNextSibling;
}

// Debugging
void nsFrame::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);

  // Output the tag and rect
  ListTag(out);
  fputs(" ", out);
  out << mRect;
  fputs("<>\n", out);
}

// Output the frame's tag
void nsFrame::ListTag(FILE* out) const
{
  nsIAtom* tag = mContent->GetTag();
  if (tag != nsnull) {
    nsAutoString buf;
    tag->ToString(buf);
    fputs(buf, out);
    NS_RELEASE(tag);
  }
  fprintf(out, "(%d)@%p", mIndexInParent, this);
}

void nsFrame::VerifyTree() const
{
}

nsStyleStruct* nsFrame::GetStyleData(const nsIID& aSID)
{
  NS_ASSERTION(mStyleContext!=nsnull,"null style context");
  if (mStyleContext)
    return mStyleContext->GetData(aSID);
  return nsnull;
}
