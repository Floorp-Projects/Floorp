/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsOutlinerColFrame.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIContent.h"
#include "nsIStyleContext.h"
#include "nsINameSpaceManager.h" 
#include "nsIDOMNSDocument.h"
#include "nsIDocument.h"
#include "nsIBoxObject.h"
#include "nsIDOMElement.h"
#include "nsOutlinerBodyFrame.h"

//
// NS_NewOutlinerColFrame
//
// Creates a new col frame
//
nsresult
NS_NewOutlinerColFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, 
                        nsIBoxLayout* aLayoutManager)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsOutlinerColFrame* it = new (aPresShell) nsOutlinerColFrame(aPresShell, aIsRoot, aLayoutManager);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewOutlinerColFrame

NS_IMETHODIMP_(nsrefcnt) 
nsOutlinerColFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsOutlinerColFrame::Release(void)
{
  return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsOutlinerColFrame)
  NS_INTERFACE_MAP_ENTRY(nsIOutlinerColFrame)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)
// Constructor
nsOutlinerColFrame::nsOutlinerColFrame(nsIPresShell* aPresShell, PRBool aIsRoot, nsIBoxLayout* aLayoutManager)
:nsBoxFrame(aPresShell, aIsRoot, aLayoutManager) 
{}

// Destructor
nsOutlinerColFrame::~nsOutlinerColFrame()
{
}

NS_IMETHODIMP
nsOutlinerColFrame::Init(nsIPresContext*  aPresContext,
                         nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIStyleContext* aContext,
                         nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  InvalidateColumnCache(aPresContext);  
  return rv;
}

NS_IMETHODIMP
nsOutlinerColFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                     const nsPoint& aPoint, 
                                     nsFramePaintLayer aWhichLayer,
                                     nsIFrame**     aFrame)
{
  if (! ( mRect.Contains(aPoint) || ( mState & NS_FRAME_OUTSIDE_CHILDREN)) )
  {
    return NS_ERROR_FAILURE;
  }

  // If we are in either the first 2 pixels or the last 2 pixels, we're going to
  // do something really strange.  Check for an adjacent splitter.
  PRBool left = PR_FALSE;
  PRBool right = PR_FALSE;
  if (mRect.x + mRect.width - 60 < aPoint.x)
    right = PR_TRUE;
  else if (mRect.x + 60 > aPoint.x)
    left = PR_TRUE;

  if (left || right) {
    // We are a header. Look for the correct splitter.
    nsIFrame* firstChild;
    mParent->FirstChild(aPresContext, nsnull, &firstChild);
    nsFrameList frames(firstChild);
    nsIFrame* child;
    if (left)
      child = frames.GetPrevSiblingFor(this);
    else GetNextSibling(&child);

    nsCOMPtr<nsIAtom> tag;
    nsCOMPtr<nsIContent> content;
    if (child) {
      child->GetContent(getter_AddRefs(content));
      content->GetTag(*getter_AddRefs(tag));
      if (tag.get() == nsXULAtoms::splitter) {
        *aFrame = child;
        return NS_OK;
      }
    }
  }

  nsresult result = nsBoxFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
  nsCOMPtr<nsIContent> content;
  if (result == NS_OK) {
    (*aFrame)->GetContent(getter_AddRefs(content));
    if (content) {
      // This allows selective overriding for subcontent.
      nsAutoString value;
      content->GetAttr(kNameSpaceID_None, nsXULAtoms::allowevents, value);
      if (value.EqualsWithConversion("true"))
        return result;
    }
  }
  if (mRect.Contains(aPoint)) {
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
    if (vis->IsVisible()) {
      *aFrame = this; // Capture all events.
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsOutlinerColFrame::AttributeChanged(nsIPresContext* aPresContext,
                                     nsIContent* aChild,
                                     PRInt32 aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     PRInt32 aModType, 
                                     PRInt32 aHint)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aPresContext, aChild,
                                               aNameSpaceID, aAttribute, aModType, aHint);

  if (aAttribute == nsHTMLAtoms::width || aAttribute == nsHTMLAtoms::hidden) {
    // Invalidate the outliner.
    EnsureOutliner();
    if (mOutliner)
      mOutliner->Invalidate();
  } else if (aAttribute == nsXULAtoms::ordinal) {
    InvalidateColumnCache(aPresContext);
  }

  return rv;
}

void
nsOutlinerColFrame::EnsureOutliner()
{
  if (!mOutliner && mContent) {
    // Get our parent node.
    nsCOMPtr<nsIContent> parent;
    mContent->GetParent(*getter_AddRefs(parent));
    nsCOMPtr<nsIDocument> doc;
    mContent->GetDocument(*getter_AddRefs(doc));
    nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(doc));
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(parent));

    nsCOMPtr<nsIBoxObject> boxObject;
    nsDoc->GetBoxObjectFor(elt, getter_AddRefs(boxObject));

    mOutliner = do_QueryInterface(boxObject);
  }
}

void
nsOutlinerColFrame::InvalidateColumnCache(nsIPresContext* aPresContext)
{
  EnsureOutliner();  
  if (mOutliner) {
    nsCOMPtr<nsIDOMElement> bodyEl;
    mOutliner->GetOutlinerBody(getter_AddRefs(bodyEl));
    nsCOMPtr<nsIContent> bodyContent = do_QueryInterface(bodyEl);
    if (bodyContent) {
      nsCOMPtr<nsIPresShell> shell;
      aPresContext->GetShell(getter_AddRefs(shell));
      nsIFrame* frame;
      shell->GetPrimaryFrameFor(bodyContent, &frame);
      if (frame) {
        nsOutlinerBodyFrame* oframe = NS_STATIC_CAST(nsOutlinerBodyFrame*, frame);
        oframe->InvalidateColumnCache();
      }
    }
  }
}
