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
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsStyleContext.h"
#include "nsINameSpaceManager.h" 
#include "nsIDOMNSDocument.h"
#include "nsIDocument.h"
#include "nsIBoxObject.h"
#include "nsIDOMElement.h"
#include "nsITreeBoxObject.h"
#include "nsITreeColumns.h"
#include "nsIDOMXULTreeElement.h"
#include "nsDisplayList.h"

//
// NS_NewTreeColFrame
//
// Creates a new col frame
//
nsIFrame*
NS_NewTreeColFrame(nsIPresShell* aPresShell, nsStyleContext* aContext,
                   PRBool aIsRoot, nsIBoxLayout* aLayoutManager)
{
  return new (aPresShell) nsTreeColFrame(aPresShell, aContext, aIsRoot, aLayoutManager);
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

// Destructor
nsTreeColFrame::~nsTreeColFrame()
{
}

NS_IMETHODIMP
nsTreeColFrame::Init(nsIContent*      aContent,
                     nsIFrame*        aParent,
                     nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);
  InvalidateColumns();
  return rv;
}

void                                                                
nsTreeColFrame::Destroy()                          
{
  InvalidateColumns();
  nsBoxFrame::Destroy();
}

class nsDisplayXULTreeColSplitterTarget : public nsDisplayItem {
public:
  nsDisplayXULTreeColSplitterTarget(nsIFrame* aFrame) : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayXULTreeColSplitterTarget);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayXULTreeColSplitterTarget() {
    MOZ_COUNT_DTOR(nsDisplayXULTreeColSplitterTarget);
  }
#endif

  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt);
  NS_DISPLAY_DECL_NAME("XULTreeColSplitterTarget")
};

nsIFrame* 
nsDisplayXULTreeColSplitterTarget::HitTest(nsDisplayListBuilder* aBuilder,
                                           nsPoint aPt)
{
  nsPoint pt = aPt - aBuilder->ToReferenceFrame(mFrame);
  // If we are in either the first 2 pixels or the last 2 pixels, we're going to
  // do something really strange.  Check for an adjacent splitter.
  PRBool left = PR_FALSE;
  PRBool right = PR_FALSE;
  if (mFrame->GetSize().width - 60 < pt.x)
    right = PR_TRUE;
  else if (60 > pt.x)
    left = PR_TRUE;

  if (left || right) {
    // We are a header. Look for the correct splitter.
    nsFrameList frames(mFrame->GetParent()->GetFirstChild(nsnull));
    nsIFrame* child;
    if (left)
      child = frames.GetPrevSiblingFor(mFrame);
    else
      child = mFrame->GetNextSibling();

    if (child && child->GetContent()->NodeInfo()->Equals(nsGkAtoms::splitter,
                                                         kNameSpaceID_XUL)) {
      return child;
    }
  }
  
  return nsnull;
}

nsresult
nsTreeColFrame::BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                            const nsRect&           aDirtyRect,
                                            const nsDisplayListSet& aLists)
{
  if (!aBuilder->IsForEventDelivery())
    return nsBoxFrame::BuildDisplayListForChildren(aBuilder, aDirtyRect, aLists);
  
  nsDisplayListCollection set;
  nsresult rv = nsBoxFrame::BuildDisplayListForChildren(aBuilder, aDirtyRect, set);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = WrapListsInRedirector(aBuilder, set, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  return aLists.Content()->AppendNewToTop(new (aBuilder)
      nsDisplayXULTreeColSplitterTarget(this));
}

NS_IMETHODIMP
nsTreeColFrame::AttributeChanged(PRInt32 aNameSpaceID,
                                 nsIAtom* aAttribute,
                                 PRInt32 aModType)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);

  if (aAttribute == nsGkAtoms::ordinal || aAttribute == nsGkAtoms::primary) {
    InvalidateColumns();
  }

  return rv;
}

void
nsTreeColFrame::SetBounds(nsBoxLayoutState& aBoxLayoutState,
                          const nsRect& aRect,
                          PRBool aRemoveOverflowArea) {
  nscoord oldWidth = mRect.width;

  nsBoxFrame::SetBounds(aBoxLayoutState, aRect, aRemoveOverflowArea);
  if (mRect.width != oldWidth) {
    nsITreeBoxObject* treeBoxObject = GetTreeBoxObject();
    if (treeBoxObject) {
      treeBoxObject->Invalidate();
    }
  }
}

nsITreeBoxObject*
nsTreeColFrame::GetTreeBoxObject()
{
  nsITreeBoxObject* result = nsnull;

  nsIContent* parent = mContent->GetParent();
  if (parent) {
    nsIContent* grandParent = parent->GetParent();
    nsCOMPtr<nsIDOMXULElement> treeElement = do_QueryInterface(grandParent);
    if (treeElement) {
      nsCOMPtr<nsIBoxObject> boxObject;
      treeElement->GetBoxObject(getter_AddRefs(boxObject));

      nsCOMPtr<nsITreeBoxObject> treeBoxObject = do_QueryInterface(boxObject);
      result = treeBoxObject.get();
    }
  }
  return result;
}

void
nsTreeColFrame::InvalidateColumns()
{
  nsITreeBoxObject* treeBoxObject = GetTreeBoxObject();
  if (treeBoxObject) {
    nsCOMPtr<nsITreeColumns> columns;
    treeBoxObject->GetColumns(getter_AddRefs(columns));

    if (columns)
      columns->InvalidateColumns();
  }
}
