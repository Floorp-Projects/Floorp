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

//
// NS_NewTreeColFrame
//
// Creates a new col frame
//
nsIFrame*
NS_NewTreeColFrame(nsIPresShell* aPresShell, PRBool aIsRoot, 
                   nsIBoxLayout* aLayoutManager)
{
  return new (aPresShell) nsTreeColFrame(aPresShell, aIsRoot, aLayoutManager);
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
nsTreeColFrame::Init(nsPresContext*  aPresContext,
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
nsTreeColFrame::Destroy(nsPresContext* aPresContext)                          
{
  if (mColumns)
    mColumns->InvalidateColumns();
  return nsBoxFrame::Destroy(aPresContext);
}

nsIFrame*
nsTreeColFrame::GetFrameForPoint(const nsPoint& aPoint,
                                 nsFramePaintLayer aWhichLayer)
{
  nsRect thisRect(nsPoint(0,0), GetSize());
  if (!(thisRect.Contains(aPoint) || (mState & NS_FRAME_OUTSIDE_CHILDREN)))
    return nsnull;

  // If we are in either the first 2 pixels or the last 2 pixels, we're going to
  // do something really strange.  Check for an adjacent splitter.
  PRBool left = PR_FALSE;
  PRBool right = PR_FALSE;
  if (mRect.width - 60 < aPoint.x)
    right = PR_TRUE;
  else if (60 > aPoint.x)
    left = PR_TRUE;

  if (left || right) {
    // We are a header. Look for the correct splitter.
    nsFrameList frames(mParent->GetFirstChild(nsnull));
    nsIFrame* child;
    if (left)
      child = frames.GetPrevSiblingFor(this);
    else
      child = GetNextSibling();

    if (child && child->GetContent()->NodeInfo()->Equals(nsXULAtoms::splitter,
                                                         kNameSpaceID_XUL)) {
      return child;
    }
  }

  nsIFrame* frame = nsBoxFrame::GetFrameForPoint(aPoint, aWhichLayer);
  if (frame) {
    nsIContent* content = frame->GetContent();
    if (content) {
      // This allows selective overriding for subcontent.
      nsAutoString value;
      content->GetAttr(kNameSpaceID_None, nsXULAtoms::allowevents, value);
      if (value.EqualsLiteral("true"))
        return frame;
    }
  }

  if (thisRect.Contains(aPoint) && GetStyleVisibility()->IsVisible()) {
    return this; // Capture all events.
  }
  return nsnull;
}

NS_IMETHODIMP
nsTreeColFrame::AttributeChanged(PRInt32 aNameSpaceID,
                                 nsIAtom* aAttribute,
                                 PRInt32 aModType)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);

  if (aAttribute == nsXULAtoms::ordinal || aAttribute == nsXULAtoms::primary) {
    EnsureColumns();
    if (mColumns)
      mColumns->InvalidateColumns();
  }

  return rv;
}

NS_IMETHODIMP
nsTreeColFrame::SetBounds(nsBoxLayoutState& aBoxLayoutState,
                          const nsRect& aRect,
                          PRBool aRemoveOverflowArea) {
  nscoord oldWidth = mRect.width;

  nsresult rv = nsBoxFrame::SetBounds(aBoxLayoutState, aRect,
                                      aRemoveOverflowArea);
  if (mRect.width != oldWidth) {
    EnsureColumns();
    if (mColumns) {
      nsCOMPtr<nsITreeBoxObject> tree;
      mColumns->GetTree(getter_AddRefs(tree));
      if (tree)
        tree->Invalidate();
    }
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
