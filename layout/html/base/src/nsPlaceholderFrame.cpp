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
#include "nsPlaceholderFrame.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsIFloaterContainer.h"

static NS_DEFINE_IID(kIFloaterContainerIID, NS_IFLOATERCONTAINER_IID);

nsresult
PlaceholderFrame::NewFrame(nsIFrame**  aInstancePtrResult,
                           nsIContent* aContent,
                           nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new PlaceholderFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

PlaceholderFrame::PlaceholderFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsFrame(aContent, aParent)
{
  mAnchoredItem = nsnull;
}

PlaceholderFrame::~PlaceholderFrame()
{
}

NS_METHOD PlaceholderFrame::Reflow(nsIPresContext*      aPresContext,
                                   nsReflowMetrics&     aDesiredSize,
                                   const nsReflowState& aReflowState,
                                   nsReflowStatus&      aStatus)
{
  // Get the floater container in which we're inserted
  nsIFloaterContainer*  container = nsnull;

  for (nsIFrame* parent = mGeometricParent; parent; parent->GetGeometricParent(parent)) {
    if (NS_OK == parent->QueryInterface(kIFloaterContainerIID, (void**)&container)) {
      break;
    }
  }
  NS_ASSERTION(nsnull != container, "no floater container");

  // Have we created the anchored item yet?
  if (nsnull == mAnchoredItem) {
    // Create the anchored item
    nsIContentDelegate* delegate = mContent->GetDelegate(aPresContext);
    nsresult rv = delegate->CreateFrame(aPresContext, mContent,
                                        mGeometricParent, mStyleContext,
                                        mAnchoredItem);
    NS_RELEASE(delegate);
    if (NS_OK != rv) {
      return rv;
    }

    // Resize reflow the anchored item into the available space
    // XXX Check for complete?
    nsReflowMetrics desiredSize(nsnull);
    nsReflowState   reflowState(eReflowReason_Initial, aReflowState.maxSize);
    mAnchoredItem->Reflow(aPresContext, desiredSize, reflowState, aStatus);
    mAnchoredItem->SizeTo(desiredSize.width, desiredSize.height);

    // Now notify our containing block that there's a new floater
    container->AddFloater(aPresContext, mAnchoredItem, this);
  } else {
    // XXX This causes anchored-items sizes to get fixed up; this is
    // not quite right because this class should be implementing one
    // of the incremental reflow methods and propagating things down
    // properly to the contained frame.
    nsReflowMetrics desiredSize(nsnull);
    nsReflowState   reflowState(eReflowReason_Resize, aReflowState.maxSize);
    mAnchoredItem->Reflow(aPresContext, desiredSize, reflowState, aStatus);
    mAnchoredItem->SizeTo(desiredSize.width, desiredSize.height);
    container->PlaceFloater(aPresContext, mAnchoredItem, this);
  }

  return nsFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
}

NS_METHOD
PlaceholderFrame::ChildCount(PRInt32& aChildCount) const
{
  aChildCount = 1;
  return NS_OK;
}

NS_METHOD
PlaceholderFrame::ChildAt(PRInt32 aIndex, nsIFrame*& aFrame) const
{
  aFrame = (0 == aIndex) ? mAnchoredItem : nsnull;
  return NS_OK;
}

NS_METHOD
PlaceholderFrame::IndexOf(const nsIFrame* aChild, PRInt32& aIndex) const
{
  aIndex = (aChild == mAnchoredItem) ? 0 : -1;
  return NS_OK;
}

NS_METHOD
PlaceholderFrame::FirstChild(nsIFrame*& aFirstChild) const
{
  aFirstChild = mAnchoredItem;
  return NS_OK;
}

NS_METHOD
PlaceholderFrame::NextChild(const nsIFrame* aChild, nsIFrame*& aNextChild) const
{
  aNextChild = nsnull;
  return NS_OK;
}

NS_METHOD
PlaceholderFrame::PrevChild(const nsIFrame* aChild, nsIFrame*& aPrevChild) const
{
  aPrevChild = nsnull;
  return NS_OK;
}

NS_METHOD
PlaceholderFrame::LastChild(nsIFrame*& aLastChild) const
{
  aLastChild = mAnchoredItem;
  return NS_OK;
}

NS_METHOD PlaceholderFrame::ListTag(FILE* out) const
{
  fputs("*placeholder", out);
  PRInt32 contentIndex;
  GetContentIndex(contentIndex);
  fprintf(out, "(%d)@%p", contentIndex, this);
  return NS_OK;
}

NS_METHOD
PlaceholderFrame::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 i;

  // Indent
  for (i = aIndent; --i >= 0; ) fputs("  ", out);

  // Output the tag
  ListTag(out);

  // Output the rect
  out << mRect;

  // Output the children
  if (nsnull != mAnchoredItem) {
    if (0 != mState) {
      fprintf(out, " [state=%08x]", mState);
    }
    fputs("<\n", out);
    mAnchoredItem->List(out, aIndent + 1);
    for (i = aIndent; --i >= 0; ) fputs("  ", out);
    fputs(">\n", out);
  } else {
    if (0 != mState) {
      fprintf(out, " [state=%08x]", mState);
    }
    fputs("<>\n", out);
  }

  return NS_OK;
}
