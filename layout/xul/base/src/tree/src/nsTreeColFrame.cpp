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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Hyatt <hyatt@mozilla.org> (Original Author)
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

#include "nsCOMPtr.h"
#include "nsTreeColFrame.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIContent.h"
#include "nsStyleContext.h"
#include "nsINameSpaceManager.h" 
#include "nsIDOMNSDocument.h"
#include "nsIDocument.h"
#include "nsIBoxObject.h"
#include "nsIDOMElement.h"
#include "nsITreeBoxObject.h"
#include "nsIDOMXULTreeElement.h"
#include "nsINodeInfo.h"

//
// NS_NewTreeColFrame
//
// Creates a new col frame
//
nsresult
NS_NewTreeColFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, 
                   nsIBoxLayout* aLayoutManager)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTreeColFrame* it = new (aPresShell) nsTreeColFrame(aPresShell, aIsRoot, aLayoutManager);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewTreeColFrame

NS_IMETHODIMP_(nsrefcnt) 
nsTreeColFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsTreeColFrame::Release(void)
{
  return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsTreeColFrame)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)

// Constructor
nsTreeColFrame::nsTreeColFrame(nsIPresShell* aPresShell, PRBool aIsRoot, nsIBoxLayout* aLayoutManager)
  : nsBoxFrame(aPresShell, aIsRoot, aLayoutManager) 
{
}

// Destructor
nsTreeColFrame::~nsTreeColFrame()
{
}

NS_IMETHODIMP
nsTreeColFrame::Init(nsIPresContext*  aPresContext,
                     nsIContent*      aContent,
                     nsIFrame*        aParent,
                     nsStyleContext*  aContext,
                     nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  EnsureColumns();
  if (mColumns)
    mColumns->InvalidateColumns();
  return rv;
}

NS_IMETHODIMP                                                                   
nsTreeColFrame::Destroy(nsIPresContext* aPresContext)                          
{
  if (mColumns)
    mColumns->InvalidateColumns();
  return nsBoxFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsTreeColFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                     const nsPoint& aPoint, 
                                     nsFramePaintLayer aWhichLayer,
                                     nsIFrame**     aFrame)
{
  if (!(mRect.Contains(aPoint) || (mState & NS_FRAME_OUTSIDE_CHILDREN)))
    return NS_ERROR_FAILURE;

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
    nsFrameList frames(mParent->GetFirstChild(nsnull));
    nsIFrame* child;
    if (left)
      child = frames.GetPrevSiblingFor(this);
    else
      child = GetNextSibling();

    if (child) {
      nsINodeInfo *ni = child->GetContent()->GetNodeInfo();
      if (ni && ni->Equals(nsXULAtoms::splitter, kNameSpaceID_XUL)) {
        *aFrame = child;
        return NS_OK;
      }
    }
  }

  nsresult result = nsBoxFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
  if (result == NS_OK) {
    nsIContent* content = (*aFrame)->GetContent();
    if (content) {
      // This allows selective overriding for subcontent.
      nsAutoString value;
      content->GetAttr(kNameSpaceID_None, nsXULAtoms::allowevents, value);
      if (value.EqualsLiteral("true"))
        return result;
    }
  }
  if (mRect.Contains(aPoint)) {
    if (GetStyleVisibility()->IsVisible()) {
      *aFrame = this; // Capture all events.
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTreeColFrame::AttributeChanged(nsIPresContext* aPresContext,
                                 nsIContent* aChild,
                                 PRInt32 aNameSpaceID,
                                 nsIAtom* aAttribute,
                                 PRInt32 aModType)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aPresContext, aChild,
                                             aNameSpaceID, aAttribute,
                                             aModType);

  if (aAttribute == nsHTMLAtoms::width || aAttribute == nsHTMLAtoms::hidden) {
    EnsureColumns();
    if (mColumns) {
      nsCOMPtr<nsITreeBoxObject> tree;
      mColumns->GetTree(getter_AddRefs(tree));
      if (tree)
        tree->Invalidate();
    }
  }
  else if (aAttribute == nsXULAtoms::ordinal || aAttribute == nsXULAtoms::primary) {
    EnsureColumns();
    if (mColumns)
      mColumns->InvalidateColumns();
  }

  return rv;
}

void
nsTreeColFrame::EnsureColumns()
{
  if (!mColumns) {
    // Get our parent node.
    nsIContent* parent = mContent->GetParent();
    if (parent) {
      nsIContent* grandParent = parent->GetParent();
      if (grandParent) {
        nsCOMPtr<nsIDOMXULTreeElement> treeElement = do_QueryInterface(grandParent);
        if (treeElement)
          treeElement->GetColumns(getter_AddRefs(mColumns));
      }
    }
  }
}
