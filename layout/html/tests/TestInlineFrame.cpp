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
#include <stdio.h>
#include "nscore.h"
#include "nsCRT.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsIDocument.h"
#include "nsVoidArray.h"
#include "nsDocument.h"
#include "nsHTMLTagContent.h"
#include "nsCoord.h"
#include "nsSplittableFrame.h"
#include "nsIContentDelegate.h"
#include "nsIPresContext.h"
#include "nsInlineFrame.h"
#include "nsIAtom.h"


///////////////////////////////////////////////////////////////////////////////
//

class MyDocument : public nsDocument {
public:
  MyDocument();
  NS_IMETHOD StartDocumentLoad(const char* aCommand,
                               nsIChannel* aChannel,
                               nsILoadGroup* aLoadGroup,
                               nsISupports* aContainer,
                               nsIStreamListener **aDocListener)
  {
    return NS_OK;
  }

protected:
  virtual ~MyDocument();
};

MyDocument::MyDocument()
{
}

MyDocument::~MyDocument()
{
}


///////////////////////////////////////////////////////////////////////////////
//

// Frame with a fixed width, but that's optionally splittable
class FixedSizeFrame : public nsSplittableFrame {
public:
  FixedSizeFrame(nsIContent* aContent,
                 nsIFrame* aParentFrame);

  NS_IMETHOD Reflow(nsIPresContext* aPresContext,
                    nsReflowMetrics& aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  PRBool       IsSplittable() const;
};

class FixedSizeContent : public nsHTMLTagContent {
public:
  FixedSizeContent(nscoord aWidth,
                   nscoord aHeight,
                   PRBool  aIsSplittable = PR_FALSE);

  // Accessors
  nscoord   GetWidth() {return mWidth;}
  void      SetWidth(nscoord aWidth);
  nscoord   GetHeight() {return mHeight;}
  void      SetHeight(nscoord aHeight);

  void      ToHTML(nsString& out);
  PRBool    IsSplittable() const {return mIsSplittable;}
  void      SetIsSplittable(PRBool aIsSplittable) {mIsSplittable = aIsSplittable;}


private:
  nscoord mWidth, mHeight;
  PRBool  mIsSplittable;
};

///////////////////////////////////////////////////////////////////////////////
//

FixedSizeFrame::FixedSizeFrame(nsIContent* aContent,
                               nsIFrame* aParentFrame)
  : nsSplittableFrame(aContent, aParentFrame)
{
}

NS_METHOD FixedSizeFrame::Reflow(nsIPresContext*      aPresContext,
                                 nsReflowMetrics&     aDesiredSize,
                                 const nsReflowState& aReflowState,
                                 nsReflowStatus&      aStatus)
{
  NS_PRECONDITION((aReflowState.availableWidth > 0) && (aReflowState.availableHeight > 0),
                  "bad max size");
  FixedSizeContent* content = (FixedSizeContent*)mContent;
  nsReflowStatus    status = NS_FRAME_COMPLETE;
  FixedSizeFrame*   prevInFlow = (FixedSizeFrame*)mPrevInFlow;

  aDesiredSize.width = content->GetWidth();
  aDesiredSize.height = content->GetHeight();

  // We can split once horizontally
  if (nsnull != prevInFlow) {
    aDesiredSize.width -= prevInFlow->mRect.width;
  } else if ((aDesiredSize.width > aReflowState.maxSize.width) && content->IsSplittable()) {
    aDesiredSize.width = aReflowState.maxSize.width;
    status = NS_FRAME_NOT_COMPLETE;
  }

  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  return status;
}

PRBool FixedSizeFrame::IsSplittable() const
{
  FixedSizeContent* content = (FixedSizeContent*)mContent;

  return content->IsSplittable();
}

///////////////////////////////////////////////////////////////////////////////
//

FixedSizeContent::FixedSizeContent(nscoord aWidth,
                                   nscoord aHeight,
                                   PRBool  aIsSplittable)
  : nsHTMLTagContent()
{
  mWidth = aWidth;
  mHeight = aHeight;
  mIsSplittable = aIsSplittable;
}

// Change the width of the content triggering an incremental reflow
void FixedSizeContent::SetWidth(nscoord aWidth)
{
  NS_NOTYETIMPLEMENTED("set width");
}

// Change the width of the content triggering an incremental reflow
void FixedSizeContent::SetHeight(nscoord aHeight)
{
  NS_NOTYETIMPLEMENTED("set height");
}

void FixedSizeContent::ToHTML(nsString& out)
{
}

///////////////////////////////////////////////////////////////////////////////
//

class InlineFrame : public nsInlineFrame
{
public:
  InlineFrame(nsIContent* aContent,
              nsIFrame* aParent);

  // Accessors to return protected state
  nsIFrame* OverflowList() {return mOverflowList;}
  void      ClearOverflowList() {mOverflowList = nsnull;}
  PRBool    LastContentIsComplete() {return mLastContentIsComplete;}

  // Helper member functions
  PRInt32   MaxChildWidth();
  PRInt32   MaxChildHeight();
};

InlineFrame::InlineFrame(nsIContent* aContent,
                         nsIFrame*   aParent)
  : nsInlineFrame(aContent, aParent)
{
}

#if 0
PRInt32 InlineFrame::MaxChildWidth()
{
  PRInt32 maxWidth = 0;

  nsIFrame* f;
  for (FirstChild(f); nsnull != f; f->GetNextSibling(f)) {
    if (f->GetWidth() > maxWidth) {
      maxWidth = f->GetWidth();
    }
  }

  return maxWidth;
}

PRInt32 InlineFrame::MaxChildHeight()
{
  PRInt32 maxHeight = 0;

  for (nsIFrame* f = FirstChild(); nsnull != f; f = f->GetNextSibling()) {
    if (f->GetHeight() > maxHeight) {
      maxHeight = f->GetHeight();
    }
  }

  return maxHeight;
}

///////////////////////////////////////////////////////////////////////////////
// Helper functions

// Compute the number of frames in the list
static PRInt32
LengthOf(nsIFrame* aChildList)
{
  PRInt32 result = 0;

  while (nsnull != aChildList) {
    aChildList = aChildList->GetNextSibling();
    result++;
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////
//

// Test basic reflowing of unmapped children. No borders/padding or child
// margins involved, and no splitting of child frames. b is an empty
// HTML container
//
// Here's what this function tests:
// - reflow unmapped maps all children and returns that it's complete
// - each child frame is placed and sized properly
// - the inline frame's desired width and  height are correct
static PRBool
TestReflowUnmapped(nsIPresContext* presContext)
{
  // Create an HTML container
  nsIHTMLContent* b;
  NS_NewHTMLContainer(&b, NS_NewAtom("span"));

  // Append three fixed width elements.
  b->AppendChildTo(new FixedSizeContent(100, 100));
  b->AppendChildTo(new FixedSizeContent(300, 300));
  b->AppendChildTo(new FixedSizeContent(200, 200));

  // Create an inline frame for the HTML container and set its
  // style context
  InlineFrame*      f = new InlineFrame(b, 0, nsnull);
  nsIStyleContext*  styleContext = presContext->ResolveStyleContextFor(b, nsnull);

  f->SetStyleContext(presContext,styleContext);

  // Reflow the HTML container
  nsReflowMetrics   reflowMetrics;
  nsSize            maxSize(1000, 1000);
  nsReflowStatus    status;

  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);

  // Make sure the frame is complete
  if (NS_FRAME_IS_NOT_COMPLETE(status)) {
    printf("ReflowUnmapped: initial reflow not complete: %d\n", status);
    return PR_FALSE;
  }

  // Verify that all the children were mapped
  if (f->ChildCount() != b->ChildCount()) {
    printf("ReflowUnmapped: wrong number of child frames: %d\n", f->ChildCount());
    return PR_FALSE;
  }

  // and that the last content offset is correct
  if (f->GetLastContentOffset() != (b->ChildCount() - 1)) {
    printf("ReflowUnmapped: wrong last content offset: %d\n", f->GetLastContentOffset());
    return PR_FALSE;
  }

  // Verify that the width/height of each child frame is correct
  for (PRInt32 i = 0; i < b->ChildCount(); i++) {
    FixedSizeContent* childContent = (FixedSizeContent*)b->ChildAt(i);
    nsIFrame*         childFrame = f->ChildAt(i);

    if ((childFrame->GetWidth() != childContent->GetWidth()) ||
        (childFrame->GetHeight() != childContent->GetHeight())) {
      printf("ReflowUnmapped: child frame size incorrect: %d\n", i);
      return PR_FALSE;
    }
  }

  // Verify that the inline frame's desired height is correct
  if (reflowMetrics.height != f->MaxChildHeight()) {
    printf("ReflowUnmapped: wrong frame height: %d\n", reflowMetrics.height);
    return PR_FALSE;
  }

  // Verify that the frames are tiled horizontally with no space between frames
  nscoord x = 0;
  for (nsIFrame* child = f->FirstChild(); child; child = child->GetNextSibling()) {
    nsPoint origin;

    child->GetOrigin(origin);
    if (origin.x != x) {
      printf("ReflowUnmapped: child x-origin is incorrect: %d\n", x);
      return PR_FALSE;
    }

    x += child->GetWidth();
  }

  // and that the inline frame's desired width is correct
  if (reflowMetrics.width != x) {
    printf("ReflowUnmapped: wrong frame width: %d\n", reflowMetrics.width);
    return PR_FALSE;
  }

  NS_RELEASE(b);
  return PR_TRUE;
}

// Test the case where the frame max size is too small for even one child frame.
//
// Here's what this function tests:
// 1. reflow unmapped with a max width narrower than the first child
//   a) when there's only one content child
//   b) when there's more than one content child
// 2. reflow unmapped when the height's too small
// 3. reflow mapped when the height's too small
// 4. reflow mapped with a max width narrower than the first child
static PRBool
TestChildrenThatDontFit(nsIPresContext* presContext)
{
  // Create an HTML container
  nsIHTMLContent* b;
  NS_NewHTMLContainer(&b, NS_NewAtom("span"));

  // Add one fixed width element.
  b->AppendChildTo(new FixedSizeContent(100, 100));

  // Create an inline frame for the HTML container and set its
  // style context
  InlineFrame*      f = new InlineFrame(b, 0, nsnull);
  nsIStyleContext*  styleContext = presContext->ResolveStyleContextFor(b, nsnull);

  f->SetStyleContext(presContext,styleContext);

  ///////////////////////////////////////////////////////////////////////////
  // Test #1a

  // Reflow the frame with a width narrower than the first child frame. This
  // tests how we handle one child that doesn't fit when reflowing unmapped
  nsReflowMetrics   reflowMetrics;
  nsSize            maxSize(10, 1000);
  nsReflowStatus    status;

  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);

  // Verify that the inline frame is complete
  if (NS_FRAME_IS_NOT_COMPLETE(status)) {
    printf("ChildrenThatDontFIt: reflow unmapped isn't complete (#1a): %d\n", status);
    return PR_FALSE;
  }

  // Verify that the desired width is the width of the first frame
  if (reflowMetrics.width != f->FirstChild()->GetWidth()) {
    printf("ChildrenThatDontFit: frame width is wrong (#1a): %d\n", f->FirstChild()->GetWidth());
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #1b

  // Append two more fixed width elements.
  b->AppendChildTo(new FixedSizeContent(300, 300));
  b->AppendChildTo(new FixedSizeContent(200, 200));

  // Create a new inline frame for the HTML container
  InlineFrame*  f1 = new InlineFrame(b, 0, nsnull);
  f1->SetStyleContext(presContext,styleContext);

  // Reflow the frame with a width narrower than the first child frame. This
  // tests how we handle children that don't fit when reflowing unmapped
  maxSize.SizeTo(10, 1000);
  status = f1->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);

  // Verify that the frame is not complete
  if (NS_FRAME_IS_NOT_COMPLETE(status)) {
    printf("ChildrenThatDontFIt: reflow unmapped isn't not complete (#1b): %d\n", status);
    return PR_FALSE;
  }

  // Verify that the desired width is the width of the first frame
  if (reflowMetrics.width != f1->FirstChild()->GetWidth()) {
    printf("ChildrenThatDontFit: frame width is wrong (#1b): %d\n", f1->FirstChild()->GetWidth());
    return PR_FALSE;
  }

  // Verify that the child count is only 1
  if (f1->ChildCount() != 1) {
    printf("ChildrenThatDontFit: more than one child frame (#1b): %d\n", f1->ChildCount());
    return PR_FALSE;
  }

  // and that there's no overflow list. If there's an overflow list that means
  // we reflowed a second frame and we would have passed it a negative max size
  // which is not a very nice thing to do
  if (nsnull != f1->OverflowList()) {
    printf("ChildrenThatDontFit: unexpected overflow list (#1b)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #2

  // Now reflow the frame wide enough so that all the frames fit. Make
  // the height small enough so that no frames fit, so we can test that
  // reflow unmapped handles that check correctly
  maxSize.width = 1000;
  maxSize.height = 10;
  reflowMetrics.height = 0;
  status = f1->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);

  // Verify the frame is complete
  if (NS_FRAME_IS_NOT_COMPLETE(status)) {
    printf("ChildrenThatDontFit: resize isn't complete (#2): %d\n", status);
    return PR_FALSE;
  }

  // Verify that all child frames are now there
  if (f1->ChildCount() != b->ChildCount()) {
    printf("ChildrenThatDontFit: wrong child count (#2): %d\n", f1->ChildCount());
    return PR_FALSE;
  }

  // and that the frame's height is the right size
  if (reflowMetrics.height != f1->MaxChildHeight()) {
    printf("ChildrenThatDontFit: height is wrong (#2): %d\n", reflowMetrics.height);
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #3

  // Now test reflow mapped against a height that's too small
  nscoord oldHeight = reflowMetrics.height;
  maxSize.height = 10;
  reflowMetrics.height = 0;
  status = f1->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);

  // Verify the frame is complete and we still have all the child frames
  if (NS_FRAME_IS_NOT_COMPLETE(status) || (f1->ChildCount() != b->ChildCount())) {
    printf("ChildrenThatDontFit: reflow mapped failed (#3)\n");
    return PR_FALSE;
  }

  // Verify that the desired height of the frame is the same as before
  if (reflowMetrics.height != oldHeight) {
    printf("ChildrenThatDontFit: height is wrong (#3): %d\n", reflowMetrics.height);
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #4

  // And finally test reflowing mapped with a max width narrower than the first
  // child
  maxSize.width = 10;
  maxSize.height = 1000;
  status = f1->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);

  // Verify the frame is not complete
  if (NS_FRAME_IS_NOT_COMPLETE(status)) {
    printf("ChildrenThatDontFIt: reflow mapped isn't not complete (#4): %d\n", status);
    return PR_FALSE;
  }

  // Verify that there's only one child
  if (f1->ChildCount() > 1) {
    printf("ChildrenThatDontFIt: reflow mapped too many children (#4): %d\n", f1->ChildCount());
    return PR_FALSE;
  }

  // and that the desired width is the width of the first frame
  if (reflowMetrics.width != f1->FirstChild()->GetWidth()) {
    printf("ChildrenThatDontFit: frame width is wrong (#4): %d\n", f1->FirstChild()->GetWidth());
    return PR_FALSE;
  }

  NS_RELEASE(b);
  return PR_TRUE;
}

// Test handling of the overflow list
//
// Here's what this function tests:
// 1. reflow unmapped places children whose width doesn't completely fit on
//    the overflow list
// 2. frames use their own overflow list when reflowing mapped children
// 3. continuing frames should use the overflow list from their prev-in-flow
static PRBool
TestOverflow(nsIPresContext* presContext)
{
  // Create an HTML container
  nsIHTMLContent* b;
  NS_NewHTMLContainer(&b, NS_NewAtom("span"));

  // Append three fixed width elements.
  b->AppendChildTo(new FixedSizeContent(100, 100));
  b->AppendChildTo(new FixedSizeContent(300, 300));
  b->AppendChildTo(new FixedSizeContent(200, 200));

  // Create an inline frame for the HTML container and set its
  // style context
  InlineFrame*      f = new InlineFrame(b, 0, nsnull);
  nsIStyleContext*  styleContext = presContext->ResolveStyleContextFor(b, nsnull);

  f->SetStyleContext(presContext,styleContext);

  ///////////////////////////////////////////////////////////////////////////
  // Test #1

  // Reflow the frame so only half the second frame fits
  nsReflowMetrics   reflowMetrics;
  nsSize            maxSize(150, 1000);
  nsReflowStatus    status;

  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");
  NS_ASSERTION(f->ChildCount() == 1, "bad child count");

  // Verify that there's one frame on the overflow list
  if ((nsnull == f->OverflowList()) || (LengthOf(f->OverflowList()) != 1)) {
    printf("Overflow: no overflow list (#1)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #2

  // Reflow the frame again this time so all the child frames fit. The inline
  // frame should use its own overflow list
  maxSize.width = 1000;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frComplete == status, "isn't complete");

  // Verify the overflow list is now empty
  if (nsnull != f->OverflowList()) {
    printf("Overflow: overflow list should be empty (#2)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #3

  // Reflow the frame so that only the first frame fits. The rest of the
  // children should go on the overflow list. This isn't interesting by
  // itself, but it enables the next test
  maxSize.width = f->FirstChild()->GetWidth();
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION((f->ChildCount() == 1) && (LengthOf(f->OverflowList()) == 2), "bad overflow list");

  // Create a continuing frame
  InlineFrame* f1 = new InlineFrame(b, 0, nsnull);

  f1->SetStyleContext(f->GetStyleContext(presContext));
  f->SetNextSibling(f1);
  f->SetNextInFlow(f1);
  f1->SetPrevInFlow(f);

  // Reflow the continuing frame. It should get its children from the overflow list
  maxSize.width = 1000;
  status = f1->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(status), "bad continuing frame");

  // Verify that the overflow list is now empty
  if (nsnull != f->OverflowList()) {
    printf("Overflow: overflow list should be empty (#3)\n");
    return PR_FALSE;
  }

  // and that the continuing frame has the remaining children
  if (f1->ChildCount() != (b->ChildCount() - f->ChildCount())) {
    printf("Overflow: continuing frame child count is wrong (#3): %d\n", f1->ChildCount());
    return PR_FALSE;
  }

  // Verify that the content offsets of the continuing frame are correct
  if (f1->GetFirstContentOffset() != f1->FirstChild()->GetIndexInParent()) {
    printf("Overflow: continuing frame bad first content offset (#3): %d\n", f1->GetFirstContentOffset());
    return PR_FALSE;
  }
  if (f1->GetLastContentOffset() != (f1->LastChild()->GetIndexInParent())) {
    printf("Overflow: continuing frame bad last content offset (#3): %d\n", f1->GetLastContentOffset());
    return PR_FALSE;
  }

  NS_RELEASE(b);
  return PR_TRUE;
}

// Test handling of pushing/pulling children
//
// Here's what this function tests:
// 1. continuing frames pick up where their prev-in-flow left off
// 2. pulling up a child from a next-in-flow
// 3. pushing a child frame to a next-in-flow that already has children
// 4. pushing a child frame to an empty next-in-flow
// 5. pulling up children from a next-in-flow that has an overflow list
// 6. pulling up all the children across an empty frame
// 7. pulling up only some of the children across an empty frame
// 8. partially pulling up a child from a next-in-flow
static PRBool
TestPushingPulling(nsIPresContext* presContext)
{
  // Create an HTML container
  nsIHTMLContent* b;
  NS_NewHTMLContainer(&b, NS_NewAtom("span"));

  // Append three fixed width elements.
  b->AppendChildTo(new FixedSizeContent(100, 100));
  b->AppendChildTo(new FixedSizeContent(300, 300));
  b->AppendChildTo(new FixedSizeContent(200, 200));

  // Create an inline frame for the HTML container and set its
  // style context
  InlineFrame*      f = new InlineFrame(b, 0, nsnull);
  nsIStyleContext*  styleContext = presContext->ResolveStyleContextFor(b, nsnull);

  f->SetStyleContext(presContext,styleContext);

  // Reflow the inline frame so only the first frame fits
  nsReflowMetrics   reflowMetrics;
  nsSize            maxSize(100, 1000);
  nsReflowStatus    status;

  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(NS_FRAME_IS_NOT_COMPLETE(status), "bad status");
  NS_ASSERTION(f->ChildCount() == 1, "bad child count");

  ///////////////////////////////////////////////////////////////////////////
  // Test #1

  // Create a continuing inline frame and test that it picks up with the second
  // child frame. See TestOverflow() for checking the case where the continuing
  // frame uses the overflow list, and TestSplittableChildren() for the
  // case where the last child isn't complete
  NS_ASSERTION(nsnull == f->OverflowList(), "unexpected overflow list");
  InlineFrame* f1 = new InlineFrame(b, 0, nsnull);

  f1->SetStyleContext(f->GetStyleContext(presContext));
  f->SetNextSibling(f1);
  f->SetNextInFlow(f1);
  f1->SetPrevInFlow(f);

  // Reflow the continuing inline frame. It should have the remainder of the
  // children
  maxSize.width = 1000;
  status = f1->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(status), "bad continuing frame");

  // Verify that the continuing inline frame has the remaining children
  if (f1->ChildCount() != (b->ChildCount() - f->ChildCount())) {
    printf("PushPull: continuing frame child count is wrong (#1): %d\n", f1->ChildCount());
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #2

  // Reflow the first inline frame so that two child frames now fit. This tests
  // pulling up a child frame from the next-in-flow
  maxSize.width = 400;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");
  NS_ASSERTION(f->ChildCount() == 2, "bad child count");
  NS_ASSERTION(f1->ChildCount() == 1, "bad continuing child count");

  // Verify the last content offset of the first inline frame
  if (f->GetLastContentOffset() != (f->ChildCount() - 1)) {
    printf("PushPull: bad last content offset (#2): %d\n", f->GetLastContentOffset());
    return PR_FALSE;
  }

  // Verify that the first/last content offsets of the continuing inline frame
  // were updated correctly after pulling up a child
  if (f1->GetFirstContentOffset() != f1->FirstChild()->GetIndexInParent()) {
    printf("PushPull: continuing frame bad first content offset (#2): %d\n", f1->GetFirstContentOffset());
    return PR_FALSE;
  }
  if (f1->GetLastContentOffset() != (f1->LastChild()->GetIndexInParent())) {
    printf("PushPull: continuing frame bad last content offset (#2): %d\n", f1->GetLastContentOffset());
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #3

  // Now reflow the inline frame so only the first child fits and verify the pushing
  // code works correctly.
  //
  // This tests the case where we're pushing to a next-in-flow that already has
  // child frames
  maxSize.width = f->FirstChild()->GetWidth();
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(NS_FRAME_IS_NOT_COMPLETE(status), "bad status");
  NS_ASSERTION(f->ChildCount() == 1, "bad child count");

  // Verify the last content offset of the first inline frame
  if (f->GetLastContentOffset() != 0) {
    printf("PushPull: bad last content offset after push (#3): %d\n",
           f->GetLastContentOffset());
    return PR_FALSE;
  }

  // Verify the continuing inline frame has two children
  if (f1->ChildCount() != 2) {
    printf("PushPull: continuing child bad child count after push (#3): %d\n",
           f1->ChildCount());
    return PR_FALSE;
  }

  // Verify its first content offset is correct
  if (f1->GetFirstContentOffset() != f1->FirstChild()->GetIndexInParent()) {
    printf("PushPull: continuing child bad first content offset after push (#3): %d\n",
           f1->GetFirstContentOffset());
    return PR_FALSE;
  }

  // and that the last content offset is correct
  if (f1->GetLastContentOffset() != (f1->LastChild()->GetIndexInParent())) {
    printf("PushPull: continuing child bad last content offset after push (#3): %d\n",
           f1->GetLastContentOffset());
    return PR_FALSE;
  }

  // Now pull all the frames back to the first inline frame
  maxSize.width = 1000;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frComplete == status, "bad status");

  // Verify the child count and last content offset of the first inline frame
  if ((f->ChildCount() != b->ChildCount()) ||
      (f->GetLastContentOffset() != (f->ChildCount() - 1))) {
    printf("PushPull: failed to pull up all the child frames (#3)\n");
    return PR_FALSE;
  }

  // The continuing inline frame should have no children
  if (f1->ChildCount() != 0) {
    printf("PushPull: continuing child is not empty after pulling up all children (#3)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #4

  // Now reflow the inline frame so only the first child fits and verify the pushing
  // code works correctly.
  //
  // This tests the case where we're pushing to an empty next-in-flow
  maxSize.width = f->FirstChild()->GetWidth();
  f1->SetFirstContentOffset(f->NextChildOffset());  // don't trigger pre-reflow check of next-in-flow
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");
  NS_ASSERTION(f->ChildCount() == 1, "bad child count");

  // Verify the continuing inline frame has two children
  if (f1->ChildCount() != 2) {
    printf("PushPull: continuing child bad child count after push (#4): %d\n",
           f1->ChildCount());
    return PR_FALSE;
  }

  // Verify its first content offset is correct
  if (f1->GetFirstContentOffset() != f1->FirstChild()->GetIndexInParent()) {
    printf("PushPull: continuing child bad first content offset after push (#4): %d\n",
           f1->GetFirstContentOffset());
    return PR_FALSE;
  }

  // and that the last content offset is correct
  if (f1->GetLastContentOffset() != (f1->LastChild()->GetIndexInParent())) {
    printf("PushPull: continuing child bad last content offset after push (#4): %d\n",
           f1->GetLastContentOffset());
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #5

  // Test pulling up children from a next-in-flow that has an overflow list.
  // Reflow f1 so only one child fits
  maxSize.width = 300;
  status = f1->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");
  NS_ASSERTION((f1->ChildCount() == 1) && (f1->GetLastContentOffset() == 1), "bad reflow");
  NS_ASSERTION(nsnull != f1->OverflowList(), "no overflow list");

  // Now reflow the first inline frame so it's wide enough for all the child frames
  maxSize.width = 1000;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frComplete == status, "bad status");
  NS_ASSERTION((f->ChildCount() == 3) && (f->GetLastContentOffset() == 2), "bad mapping");

  // The second frame should be be empty, and it should not have an overflow list
  if (f1->ChildCount() != 0) {
    printf("PushPull: continuing child isn't empty (#5)\n");
    return PR_FALSE;
  }
  if (nsnull != f1->OverflowList()) {
    printf("PushPull: continuing child still has overflow list (#5)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #6

  // Test pulling up all the child frames across an empty frame, i.e. a frame
  // from which we've already pulled up all the children
  //
  // Set it up so that there's one frame in 'f' and one frame in 'f1'
  maxSize.width = 100;
  f1->BreakFromPrevFlow();
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");
  NS_ASSERTION((f->ChildCount() == 1) && (f->GetLastContentOffset() == 0), "bad mapping");

  maxSize.width = 300;
  f1->AppendToFlow(f);
  status = f1->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");
  NS_ASSERTION((f1->ChildCount() == 1) && (f1->GetLastContentOffset() == 1), "bad mapping");
  NS_ASSERTION(nsnull != f1->OverflowList(), "no overflow list");

  // Now create a third inline frame and have it map the third child frame
  InlineFrame* f2 = new InlineFrame(b, 0, nsnull);

  f2->SetStyleContext(f->GetStyleContext(presContext));
  f1->SetNextSibling(f2);
  f1->SetNextInFlow(f2);
  f2->SetPrevInFlow(f1);

  maxSize.width = 1000;
  status = f2->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frComplete == status, "bad status");
  NS_ASSERTION(nsnull == f1->OverflowList(), "overflow list");

  // Now reflow the first inline frame wide enough so all the child frames fit
  // XXX This isn't enough and we need one more test. We need to first
  // pull up just one child, then reflow the first frame again so all
  // the child frames fit.
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);

  // Verify that the inline frame is complete
  if (NS_FRAME_NOT_IS_COMPLETE(status)) {
    printf("PushPull: failed to pull-up across empty frame (#6)\n");
    return PR_FALSE;
  }

  // Verify the inline frame maps all the children
  if ((f->ChildCount() != 3) || (f->GetLastContentOffset() != 2)) {
    printf("PushPull: bad last content offset or child count (#6)\n");
    return PR_FALSE;
  }

  // Verify that the next two inline frames are empty
  if (f1->ChildCount() != 0) {
    printf("PushPull: second frame isn't empty (#6)\n");
    return PR_FALSE;
  }
  if (f2->ChildCount() != 0) {
    printf("PushPull: third frame isn't empty (#6)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #7

  // Test pulling up only some of the child frames across an empty frame, i.e.
  // a frame from which we've already pulled up all the children
  //
  // Set it up so that there's one child frame in the first inline frame
  maxSize.width = 100;
  f1->BreakFromPrevFlow();
  f2->BreakFromPrevFlow();
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");
  NS_ASSERTION(f->ChildCount() == 1, "bad child count");
  NS_ASSERTION(f->GetLastContentOffset() == 0, "bad mapping");

  // And one frame in f1
  maxSize.width = 300;
  f1->AppendToFlow(f);
  status = f1->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");
  NS_ASSERTION(f1->ChildCount() == 1, "bad child count");
  NS_ASSERTION((f1->GetFirstContentOffset() == 1) && (f1->GetLastContentOffset() == 1), "bad mapping");

  // And one frame in f2
  f2->AppendToFlow(f1);
  maxSize.width = 1000;
  status = f2->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frComplete == status, "bad status");
  NS_ASSERTION(f2->ChildCount() == 1, "bad child count");
  NS_ASSERTION((f2->GetFirstContentOffset() == 2) && (f2->GetLastContentOffset() == 2), "bad mapping");

  // Now reflow the first inline frame wide enough so that the first two child
  // frames fit
  maxSize.width = 400;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);

  // Verify that the first inline frame is not complete
  if (NS_FRAME_IS_COMPLETE(status)) {
    printf("PushPull: bad status (#7)\n");
    return PR_FALSE;
  }

  // Verify the first inline frame maps two children
  if ((f->ChildCount() != 2) || (f->GetLastContentOffset() != 1)) {
    printf("PushPull: bad last content offset or child count (#7)\n");
    return PR_FALSE;
  }

  // Verify that the second inline frame is empty
  if (f1->ChildCount() != 0) {
    printf("PushPull: second frame isn't empty (#7)\n");
    return PR_FALSE;
  }

  // and that it's content offset must be correct, because the third inline
  // frame isn't empty
  // Note: don't check the last content offset, because for empty frames the value
  // is undefined...
  if (f1->GetFirstContentOffset() != f->NextChildOffset()) {
    printf("PushPull: second frame bad first content offset (#7): %d\n", f1->GetFirstContentOffset());
    return PR_FALSE;
  }

  // Verify that the third inline frame has one child frame
  if (f2->ChildCount() != 1) {
    printf("PushPull: third frame bad child count (#7): %d\n", f2->ChildCount());
    return PR_FALSE;
  }

  // and that its content offsets are correct
  if ((f2->GetFirstContentOffset() != 2) || (f2->GetLastContentOffset() != 2)) {
    printf("PushPull: third frame bad content mapping (#7)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #8

  // Test partially pulling up a child. First get all the child frames back into
  // the first inline frame, and get rid of the third inline frame
  f2->BreakFromPrevFlow();
  f1->SetNextSibling(nsnull);

  maxSize.width = 1000;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frComplete == status, "bad status");
  NS_ASSERTION(f->ChildCount() == 3, "bad child count");
  f1->SetFirstContentOffset(f->NextChildOffset());  // don't trigger pre-reflow checks below

  // Now reflow the first inline frame so there are two frames pushed to the
  // next-in-flow
  maxSize.width = 100;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");
  NS_ASSERTION((f->ChildCount() == 1) && (f1->ChildCount() == 2), "bad child count");

  // Reflow the first inline frame so that part only part of the second child frame
  // fits. In order to test this we need to make the second piece of content
  // splittable
  FixedSizeContent* c2 = (FixedSizeContent*)b->ChildAt(1);
  c2->SetIsSplittable(PR_TRUE);

  maxSize.width = 250;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");

  // The first inline frame should have two child frames
  if (f->ChildCount() != 2) {
    printf("PushPull: bad child count when pulling up (#8): %d\n", f->ChildCount());
    return PR_FALSE;
  }

  // it should have a last content offset of 1, and it's last content should
  // not be complete
  if ((f->GetLastContentOffset() != 1) || f->LastContentIsComplete()) {
    printf("PushPull: bad content mapping when pulling up (#8)\n");
    return PR_FALSE;
  }

  // The continuing inline frame should also have two child frame
  if (f1->ChildCount() != 2) {
    printf("PushPull: continuing frame bad child count (#8): %d\n", f1->ChildCount());
    return PR_FALSE;
  }

  // and correct content offsets
  if ((f1->GetFirstContentOffset() != f1->FirstChild()->GetIndexInParent()) ||
      (f1->GetLastContentOffset() != (f1->LastChild()->GetIndexInParent()))) {
    printf("PushPull: continuing frame bad mapping (#8)\n");
    return PR_FALSE;
  }

  // The first child of f1 should be in the flow
  if (f1->FirstChild()->GetPrevInFlow() != f->LastChild()) {
    printf("PushPull: continuing frame bad flow (#8)\n");
    return PR_FALSE;
  }

  NS_RELEASE(b);
  return PR_TRUE;
}

// Test handling of splittable children
//
// Here's what this function tests:
// 1. reflow unmapped correctly handles child frames that need to split
// 2. reflow mapped correctly handles when the last child is incomplete
// 3. reflow mapped correctly handles the case of a child that's incomplete
//    and that already has a next-in-flow
// 4. reflow mapped handles the case where a child that's incomplete now fits
//    when reflowed again
//    a) when the child has a next-in-flow
//    b) when the child doesn't have a next-in-flow
// 5. continuing frame checks its prev-in-flow's last child and if it's incomplete
//    creates a continuing frame
// 6. reflow mapped correctly handles child frames that need to be continued
// 7. pulling up across empty frames resulting from deleting a child's next-in-flows
static PRBool
TestSplittableChildren(nsIPresContext* presContext)
{
  // Create an HTML container
  nsIHTMLContent* b;
  NS_NewHTMLContainer(&b, NS_NewAtom("span"));

  // Append three fixed width elements that can split
  b->AppendChildTo(new FixedSizeContent(100, 100, PR_TRUE));
  b->AppendChildTo(new FixedSizeContent(300, 300, PR_TRUE));
  b->AppendChildTo(new FixedSizeContent(200, 200, PR_TRUE));

  // Create an inline frame for the HTML container and set its
  // style context
  InlineFrame*      f = new InlineFrame(b, 0, nsnull);
  nsIStyleContext*  styleContext = presContext->ResolveStyleContextFor(b, nsnull);

  f->SetStyleContext(presContext,styleContext);

  ///////////////////////////////////////////////////////////////////////////
  // Test #1

  // Reflow the inline frame so only half of the first frame fits. This tests
  // reflow unmapped
  nsReflowMetrics         reflowMetrics;
  nsSize                  maxSize(50, 1000);
  nsIFrame::ReflowStatus  status;

  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(f->ChildCount() == 1, "bad child count");

  // Verify that the inline frame isn't complete
  if (nsIFrame::frNotComplete != status) {
    printf("Splittable: inline frame should be incomplete (#1): %d\n", status);
    return PR_FALSE;
  }

  // Verify the last content offset is correct
  if (f->GetLastContentOffset() != 0) {
    printf("Splittable: wrong last content offset (#1): %d\n", f->GetLastContentOffset());
    return PR_FALSE;
  }

  // Verify that the last child isn't complete
  if (f->LastContentIsComplete()) {
    printf("Splittable: child should not be complete (#1)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #2

  // Now reflow the inline frame again and test reflow mapped
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(f->ChildCount() == 1, "bad child count");

  // The inline frame should still be incomplete with a last content offset of 0
  if ((nsIFrame::frNotComplete != status) ||
      (f->GetLastContentOffset() != 0) || (f->LastContentIsComplete())) {
    printf("Splittable: reflow mapped failed (#2)\n");
    return PR_FALSE;
  }

  // There should be an overflow list containing the first child's next-in-flow
  if ((nsnull == f->OverflowList()) ||
      (f->OverflowList() != f->FirstChild()->GetNextInFlow())) {
    printf("Splittable: should be an overflow list (#2)\n");
    return PR_FALSE;
  }

  // Verify that the first child has a null next sibling. Children on the overflow
  // list should be disconnected from the mapped children
  if (nsnull != f->FirstChild()->GetNextSibling()) {
    printf("Splittable: first child has overflow list as next sibling (#2)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #3

  // Reflow it again with the same size and make sure we don't add any more
  // frames to the overflow list
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");
  NS_ASSERTION(f->ChildCount() == 1, "bad child count");

  // The inline frame should still have a last content offset of 0 and
  // the last content child should still be incomplete
  if ((f->GetLastContentOffset() != 0) || (f->LastContentIsComplete())) {
    printf("Splittable: reflow mapped again failed (#3)\n");
    return PR_FALSE;
  }

  // The overflow list should still have only one child
  if (LengthOf(f->OverflowList()) != 1) {
    printf("Splittable: reflow mapped again has bad overflow list (#3)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #4a

  // Now reflow the inline frame so that the the first child now completely fits.
  // This tests how reflow mapped handles the case of a child that's incomplete
  // and has a next-in-flow, and when reflowed now fits.
  maxSize.width = 100;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");
  NS_ASSERTION(f->ChildCount() == 1, "bad child count");

  // The inline frame should have a last content offset of 0
  if (f->GetLastContentOffset() != 0) {
    printf("Splittable: wrong last content offset (#4a): %d\n",
           f->GetLastContentOffset());
    return PR_FALSE;
  }

  // The first child should now be complete
  if (!f->LastContentIsComplete()) {
    printf("Splittable: last child isn't complete (#4a)\n");
    return PR_FALSE;
  }

  // it should not have a next-in-flow
  if (nsnull != f->FirstChild()->GetNextInFlow()) {
    printf("Splittable: unexpected next-in-flow (#4a)\n");
    return PR_FALSE;
  }

  // and there should be no overflow list
  if (nsnull != f->OverflowList()) {
    printf("Splittable: unexpected overflow list (#4a)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #4b

  // This is a variation of the previous test where the child doesn't have
  // a next-in-flow. Reflow the inline frame so the first child is incomplete
  maxSize.width = 50;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(!f->LastContentIsComplete(), "last child should be incomplete");
  NS_ASSERTION(nsnull != f->OverflowList(), "no overflow list");
  NS_ASSERTION(f->FirstChild()->GetNextInFlow() == f->OverflowList(), "bad next-in-flow");

  // Now get rid of the overflow list and the first child's next-in-flow
  f->FirstChild()->SetNextInFlow(nsnull);
  f->ClearOverflowList();

  // Now reflow the inline frame so the first child fits. This tests that we
  // correctly reset mLastContentIsComplete when the frame that was incomplete
  // doesn't have a next-in-flow
  maxSize.width = 100;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");
  NS_ASSERTION(f->ChildCount() == 1, "bad child count");

  // Verify that the first child is complete
  if (!f->LastContentIsComplete()) {
    printf("Splittable: last content is NOT complete (#4b)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #5

  // Create a continuing inline frame and have it pick up with the second
  // child
  InlineFrame* f1 = new InlineFrame(b, 0, nsnull);

  f1->SetStyleContext(f->GetStyleContext(presContext));
  f->SetNextSibling(f1);
  f->SetNextInFlow(f1);
  f1->SetPrevInFlow(f);

  // Reflow the continuing inline frame so its second frame is not complete
  maxSize.width = 150;
  status = f1->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");
  NS_ASSERTION((f1->GetFirstContentOffset() == 1) && (f1->GetLastContentOffset() == 1), "bad offsets");
  NS_ASSERTION(!f1->LastContentIsComplete(), "should not be complete");

  // Create a third continuing inline frame and verify that it picks up by continuing
  // the second child frame. This tests the case where the prev-in-flow's last
  // child isn't complete and there's no overflow list
  NS_ASSERTION(nsnull == f1->OverflowList(), "unexpected overflow list");
  InlineFrame* f2 = new InlineFrame(b, 0, nsnull);

  f2->SetStyleContext(f->GetStyleContext(presContext));
  f1->SetNextSibling(f2);
  f1->SetNextInFlow(f2);
  f2->SetPrevInFlow(f1);

  // Make the width large enough for all the remaining frames to fit
  maxSize.width = 1000;
  status = f2->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frComplete == status, "bad status");

  // Verify the first child is a continuing frame of its prev-in-flow's
  // child
  if (f2->FirstChild()->GetPrevInFlow() != f1->FirstChild()) {
    printf("Splittable: child frame bad prev-in-flow (#5)\n");
    return PR_FALSE;
  }

  // Verify the two child frames have the same content offset and so
  // do their parents
  if ((f2->FirstChild()->GetIndexInParent() != f1->FirstChild()->GetIndexInParent()) ||
      (f2->GetFirstContentOffset() != f1->GetFirstContentOffset())) {
    printf("Splittable: bad content offset (#5)\n");
    return PR_FALSE;
  }

  // Verify that the third continuing inline frame is complete and that it has two children
  if ((nsnull != f2->FirstChild()->GetNextInFlow()) || (f2->ChildCount() != 2)) {
    printf("Splittable: bad continuing frame (#5)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #6

  // Test how reflow mapped handles the case where an existing child doesn't
  // fit when reflowed. We care about the case where the child doesn't have
  // a continuing frame and so we have to create one
  //
  // Reflow the first inline frame so all the child frames fit
  maxSize.width = 1000;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frComplete == status, "bad status");
  NS_ASSERTION((f->ChildCount() == 3) && (f->GetLastContentOffset() == 2), "bad count");

  f1->SetFirstContentOffset(f->NextChildOffset());  // don't trigger pre-reflow checks
  f2->SetFirstContentOffset(f->NextChildOffset());  // don't trigger pre-reflow checks

  // Now reflow it so that the first child needs to be continued
  maxSize.width = 50;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");

  // Verify there's only one child in f
  if (f->ChildCount() != 1) {
    printf("Splittable: bad child count (#6): %d\n", f->ChildCount());
    return PR_FALSE;
  }

  // Verify the last content offset and that the last content is not complete
  if ((f->GetLastContentOffset() != 0) || f->LastContentIsComplete()) {
    printf("Splittable: bad content mapping (#6)\n");
    return PR_FALSE;
  }

  // Verify that there are three children in the next-in-flow, and that
  // the first child is a continuing frame
  if (f1->ChildCount() != 3) {
    printf("Splittable: continuing frame bad child count (#6): %d\n",
           f1->ChildCount());
    return PR_FALSE;
  }
  if (f1->FirstChild()->GetPrevInFlow() != f->LastChild()) {
    printf("Splittable: continuing frame bad child flow (#6)\n");
    return PR_FALSE;
  }

  // Verify the next-in-flow's content offsets
  if ((f1->GetFirstContentOffset() != 0) || (f1->GetLastContentOffset() != 2)) {
    printf("Splittable: continuing frame bad mapping (#6)\n");
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #7

  // Test pulling up across empty frames resulting from deleting a child
  // frame's next-in-flows
  //
  // Reflow the first inline frame so all the child frames fit
  maxSize.width = 1000;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frComplete == status, "bad status");
  NS_ASSERTION((f->ChildCount() == 3) && (f->GetLastContentOffset() == 2), "bad count");

  f1->SetFirstContentOffset(f->NextChildOffset());  // don't trigger pre-reflow checks
  f2->SetFirstContentOffset(f->NextChildOffset());  // don't trigger pre-reflow checks

  // Now reflow the first inline frame so that the second child frame is continued
  maxSize.width = 200;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");

  // Verify the first inline frame's child count is correct
  if (f->ChildCount() != 2) {
    printf("Splittable: bad child count (#7): %d\n", f->ChildCount());
    return PR_FALSE;
  }

  // Verify the last content offset and that the last frame isn't complete
  if ((f->GetLastContentOffset() != 1) || (f->LastContentIsComplete())) {
    printf("Splittable: bad mapping (#7)\n");
    return PR_FALSE;
  }

  // The second inline frame should have two children as well
  if (f1->ChildCount() != 2) {
    printf("Splittable: continuing frame bad child count (#7): %d\n", f1->ChildCount());
    return PR_FALSE;
  }

  // and that its content offsets are correct
  if ((f1->GetFirstContentOffset() != 1) || (f1->GetLastContentOffset() != 2)) {
    printf("Splittable: continuing frame bad mapping (#7)\n");
    return PR_FALSE;
  }

  // Now reflow the second inline frame so that only the continuing child frame
  // fits and the last frame is pushed to the third inline frame
  maxSize.width = 200;
  status = f1->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);
  NS_ASSERTION(nsIFrame::frNotComplete == status, "bad status");

  // Verify the second inline frame's child count and last content offset
  if ((f1->ChildCount() != 1) || (f1->GetLastContentOffset() != 1)) {
    printf("Splittable: continuing frame bad child count or mapping (#7)\n");
    return PR_FALSE;
  }

  // Verify the third inline frame maps the last child frame and has correct
  // content offsets
  if ((f2->ChildCount() != 1) ||
      (f2->GetFirstContentOffset() != 2) || (f2->GetLastContentOffset() != 2)) {
    printf("Splittable: last continuing frame bad child count or mapping (#7)\n");
    return PR_FALSE;
  }

  // Now the real test. Reflow the first inline frame so all the child frames fit.
  // What we're testing is that the pull-up code correctly handles the case where
  // there's an empty frame that results from the second child frame's next-in-flow
  // being deleted
  maxSize.width = 1000;
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, nsnull);

  // The inline frame should be complete and map all three child frames
  if ((nsIFrame::frComplete != status) || (f->ChildCount() != 3)) {
    printf("Splittable: first inline frame does not map all the child frames (#7)\n");
    return PR_FALSE;
  }

  NS_RELEASE(b);
  return PR_TRUE;
}

// Test computing the max element size
//
// Here's what this function tests:
// 1. reflow unmapped correctly computes the max element size
// 2. reflow mapped correctly computes the max element size
// 3. reflow mapped/unmapped work together to compute the correct result
// 4. pulling-up children code computes the correct result
static PRBool
TestMaxElementSize(nsIPresContext* presContext)
{
  // Create an HTML container
  nsIHTMLContent* b;
  NS_NewHTMLContainer(&b, NS_NewAtom("span"));

  // Append three fixed width elements.
  b->AppendChildTo(new FixedSizeContent(100, 100));
  b->AppendChildTo(new FixedSizeContent(300, 300));
  b->AppendChildTo(new FixedSizeContent(200, 200));

  // Create an inline frame for the HTML container and set its
  // style context
  InlineFrame*      f = new InlineFrame(b, 0, nsnull);
  nsIStyleContext*  styleContext = presContext->ResolveStyleContextFor(b, nsnull);

  f->SetStyleContext(presContext,styleContext);

  ///////////////////////////////////////////////////////////////////////////
  // Test #1

  // Reflow the inline frame and check our max element size computation when reflowing
  // unmapped children
  //
  // Note: we've already tested elsewhere that we correctly handle the case
  // where aMaxElementSize is NULL and that we don't GPF...
  nsSize                  maxElementSize(0, 0);
  nsReflowMetrics         reflowMetrics;
  nsSize                  maxSize(1000, 1000);
  nsIFrame::ReflowStatus  status;

  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, &maxElementSize);

  // Verify that the max element size is correct
  NS_ASSERTION(nsIFrame::frComplete == status, "isn't complete");
  if ((maxElementSize.width != f->MaxChildWidth()) ||
      (maxElementSize.height != f->MaxChildHeight())) {
    printf("MaxElementSize: wrong result in reflow unmapped (#1): (%d, %d)\n",
      maxElementSize.width, maxElementSize.height);
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #2

  // Now compute it again and check the computation when reflowing
  // mapped children
  maxElementSize.SizeTo(0, 0);
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, &maxElementSize);
  NS_ASSERTION(nsIFrame::frComplete == status, "isn't complete");

  if ((maxElementSize.width != f->MaxChildWidth()) ||
      (maxElementSize.height != f->MaxChildHeight())) {
    printf("MaxElementSize: wrong result in reflow mapped (#2): (%d, %d)\n",
      maxElementSize.width, maxElementSize.height);
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #3

  // Check that reflow mapped/unmapped work together. Reflow the inline frame
  // so that only the first two frames (including the largest frame) fit
  nsIFrame* maxChild = f->ChildAt(1);
  nsPoint   origin;

  maxChild->GetOrigin(origin);
  maxSize.width = origin.x + maxChild->GetWidth();
  maxElementSize.SizeTo(0, 0);
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, &maxElementSize);

  if ((nsIFrame::frNotComplete != status) || (f->ChildCount() != 2)) {
    printf("MaxElementSize: reflow failed (#3)\n");
    return PR_FALSE;
  }

  // Now reflow it again and check that the maximum element size is correct
  maxSize.width = 1000;
  maxElementSize.SizeTo(0, 0);
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, &maxElementSize);
  NS_ASSERTION(nsIFrame::frComplete == status, "isn't complete");

  if ((maxElementSize.width != f->MaxChildWidth()) ||
      (maxElementSize.height != f->MaxChildHeight())) {
    printf("MaxElementSize: wrong result in reflow mapped/unmapped (#3): (%d, %d)\n",
           maxElementSize.width, maxElementSize.height);
    return PR_FALSE;
  }

  ///////////////////////////////////////////////////////////////////////////
  // Test #4

  // Last thing to check is pulling up children. Reflow it so only the first two
  // frames (including the largest frame) fit
  maxChild = f->ChildAt(1);
  maxChild->GetOrigin(origin);
  maxSize.width = origin.x + maxChild->GetWidth();
  maxElementSize.SizeTo(0, 0);
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, &maxElementSize);
  NS_ASSERTION(f->ChildCount() == 2, "unexpected child count");

  // Create a continuing inline frame and reflow it
  InlineFrame* f1 = new InlineFrame(b, 0, nsnull);

  f1->SetStyleContext(f->GetStyleContext(presContext));
  f->SetNextSibling(f1);
  f->SetNextInFlow(f1);
  f1->SetPrevInFlow(f);
  maxSize.width = 1000;
  status = f1->ResizeReflow(presContext, reflowMetrics, maxSize, &maxElementSize);
  NS_ASSERTION((nsIFrame::frComplete == status) && (f1->ChildCount() == 1), "bad continuing frame");

  // Now reflow the first inline frame so all child frames fit, and verify the
  // max element size is correct
  maxElementSize.SizeTo(0, 0);
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, &maxElementSize);
  NS_ASSERTION(nsIFrame::frComplete == status, "isn't complete");
  NS_ASSERTION(f1->ChildCount() == 0, "pull-up failed");

  if ((maxElementSize.width != f->MaxChildWidth()) ||
      (maxElementSize.height != f->MaxChildHeight())) {
    printf("MaxElementSize: wrong result in reflow mapped/pull-up (#4): (%d, %d)\n",
           maxElementSize.width, maxElementSize.height);
    return PR_FALSE;
  }

  // Now reflow it so that the largest size frame is in the continuing inline frame
  f1->SetFirstContentOffset(f->NextChildOffset());  // don't trigger pre-reflow checks
  maxSize.width = f->FirstChild()->GetWidth();
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, &maxElementSize);
  NS_ASSERTION(f->ChildCount() == 1, "unexpected child count");
  NS_ASSERTION(f1->ChildCount() == 2, "unexpected child count");

  // Now reflow the first inline frame so all child frames fit, and verify the
  // max element size is correct
  maxSize.width = 1000;
  maxElementSize.SizeTo(0, 0);
  status = f->ResizeReflow(presContext, reflowMetrics, maxSize, &maxElementSize);
  NS_ASSERTION(nsIFrame::frComplete == status, "isn't complete");
  NS_ASSERTION(f1->ChildCount() == 0, "pull-up failed");

  if ((maxElementSize.width != f->MaxChildWidth()) ||
      (maxElementSize.height != f->MaxChildHeight())) {
    printf("MaxElementSize: wrong result in reflow mapped/pull-up (#4): (%d, %d)\n",
           maxElementSize.width, maxElementSize.height);
    return PR_FALSE;
  }

  NS_RELEASE(b);
  return PR_TRUE;
}
#endif

int main(int argc, char** argv)
{
#if 0
  // Create test document and presentation context
  MyDocument *myDoc = new MyDocument();
  nsIPresContext* presContext;
  nsIDeviceContext *dx;
  
  static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);

  nsresult rv = nsComponentManager::CreateInstance(kDeviceContextCID, nsnull, NS_GET_IID(nsIDeviceContext), (void **)&dx);

  if (NS_OK == rv) {
    dx->Init(nsull);
    dx->SetDevUnitsToAppUnits(dx->GetDevUnitsToTwips());
    dx->SetAppUnitsToDevUnits(dx->GetTwipsToDevUnits());
  }

  NS_NewGalleyContext(&presContext);

  presContext->Init(dx, nsnull);

  // Test basic reflowing of unmapped children
  if (!TestReflowUnmapped(presContext)) {
    return -1;
  }

  // Test the case where the frame max size is too small for even one child frame
  if (!TestChildrenThatDontFit(presContext)) {
    return -1;
  }

  if (!TestOverflow(presContext)) {
    return -1;
  }

  if (!TestPushingPulling(presContext)) {
    return -1;
  }

  if (!TestSplittableChildren(presContext)) {
    return -1;
  }

  if (!TestMaxElementSize(presContext)) {
    return -1;
  }

  /*
  if (!TestIncremental(presContext)) {
    return -1;
  }

  if (!TestBorderPadding(presContext)) {
    return -1;
  }

  if (!TestChildMargins(presContext)) {
    return -1;
  }

  if (!TestAlignment(presContext)) {
    return -1;
  }

  if (!TestDisplayNone(presContext)) {
    return -1;
  }

  if (!TestRelativePositioning(presContext)) {
    return -1;
  }

  if (!TestAbsolutePositiong(presContext)) {
    return -1;
  }
  */

  presContext->Release();
  myDoc->Release();
#endif
  return 0;
}
