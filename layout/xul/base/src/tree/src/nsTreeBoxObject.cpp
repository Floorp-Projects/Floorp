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
 *   Brian Ryner <bryner@brianryner.com>
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
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsITreeBoxObject.h"
#include "nsITreeView.h"
#include "nsITreeSelection.h"
#include "nsBoxObject.h"
#include "nsIFrame.h"
#include "nsIAtom.h"
#include "nsINodeInfo.h"
#include "nsXULAtoms.h"
#include "nsChildIterator.h"

class nsTreeBoxObject : public nsITreeBoxObject, public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSITREEBOXOBJECT

  nsTreeBoxObject();
  ~nsTreeBoxObject();

  nsITreeBoxObject* GetTreeBody();

  //NS_PIBOXOBJECT interfaces
  NS_IMETHOD Init(nsIContent* aContent, nsIPresShell* aPresShell);
  NS_IMETHOD SetDocument(nsIDocument* aDocument);
  NS_IMETHOD InvalidatePresentationStuff();
};

/* Implementation file */
NS_IMPL_ISUPPORTS_INHERITED1(nsTreeBoxObject, nsBoxObject, nsITreeBoxObject)


NS_IMETHODIMP
nsTreeBoxObject::SetDocument(nsIDocument* aDocument)
{
  // this should only be called with a null document, which indicates
  // that we're being torn down.
  NS_ASSERTION(aDocument == nsnull, "SetDocument called with non-null document");

  // Drop the view's ref to us.
  nsCOMPtr<nsISupports> suppView;
  GetPropertyAsSupports(NS_LITERAL_STRING("view").get(), getter_AddRefs(suppView));
  nsCOMPtr<nsITreeView> treeView(do_QueryInterface(suppView));
  if (treeView) {
    nsCOMPtr<nsITreeSelection> sel;
    treeView->GetSelection(getter_AddRefs(sel));
    if (sel)
      sel->SetTree(nsnull);
    treeView->SetTree(nsnull); // Break the circular ref between the view and us.
  }

  return nsBoxObject::SetDocument(aDocument);
}


NS_IMETHODIMP
nsTreeBoxObject::InvalidatePresentationStuff()
{
  SetPropertyAsSupports(NS_LITERAL_STRING("treebody").get(), nsnull);
  SetPropertyAsSupports(NS_LITERAL_STRING("view").get(), nsnull);

  return nsBoxObject::InvalidatePresentationStuff();
}
  
nsTreeBoxObject::nsTreeBoxObject()
{
}

nsTreeBoxObject::~nsTreeBoxObject()
{
  /* destructor code */
}


NS_IMETHODIMP nsTreeBoxObject::Init(nsIContent* aContent, nsIPresShell* aPresShell)
{
  nsresult rv = nsBoxObject::Init(aContent, aPresShell);
  if (NS_FAILED(rv)) return rv;
  return NS_OK;
}

static void FindBodyElement(nsIContent* aParent, nsIContent** aResult)
{
  *aResult = nsnull;
  ChildIterator iter, last;
  for (ChildIterator::Init(aParent, &iter, &last); iter != last; ++iter) {
    nsCOMPtr<nsIContent> content = *iter;

    nsINodeInfo *ni = content->GetNodeInfo();
    if (ni && ni->Equals(nsXULAtoms::treechildren, kNameSpaceID_XUL)) {
      *aResult = content;
      NS_ADDREF(*aResult);
      break;
    }
    else if (ni && !ni->Equals(nsXULAtoms::templateAtom, kNameSpaceID_XUL)) {
      FindBodyElement(content, aResult);
      if (*aResult)
        break;
    }
  }
}

inline nsITreeBoxObject*
nsTreeBoxObject::GetTreeBody()
{
  nsCOMPtr<nsISupports> supp;
  GetPropertyAsSupports(NS_LITERAL_STRING("treebody").get(), getter_AddRefs(supp));

  if (supp) {
    nsCOMPtr<nsITreeBoxObject> body(do_QueryInterface(supp));
    return body;
  }

  nsIFrame* frame = GetFrame();
  if (!frame)
    return nsnull;

  // Iterate over our content model children looking for the body.
  nsCOMPtr<nsIContent> content;
  FindBodyElement(frame->GetContent(), getter_AddRefs(content));

  mPresShell->GetPrimaryFrameFor(content, &frame);
  if (!frame)
     return nsnull;

  // It's a frame. Refcounts are irrelevant.
  nsCOMPtr<nsITreeBoxObject> body;
  frame->QueryInterface(NS_GET_IID(nsITreeBoxObject), getter_AddRefs(body));
  SetPropertyAsSupports(NS_LITERAL_STRING("treebody").get(), body);
  return body;
}

NS_IMETHODIMP nsTreeBoxObject::GetView(nsITreeView * *aView)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->GetView(aView);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::SetView(nsITreeView * aView)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body) {
    body->SetView(aView);
  
    // only return if the body frame was able to store the view,
    // else we need to cache the property below
    nsCOMPtr<nsITreeView> view;
    body->GetView(getter_AddRefs(view));
    if (view)
      return NS_OK;
  }
  
  nsCOMPtr<nsISupports> suppView(do_QueryInterface(aView));
  if (suppView)
    SetPropertyAsSupports(NS_LITERAL_STRING("view").get(), suppView);
  else
    RemoveProperty(NS_LITERAL_STRING("view").get());

  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetFocused(PRBool* aFocused)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->GetFocused(aFocused);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::SetFocused(PRBool aFocused)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->SetFocused(aFocused);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetTreeBody(nsIDOMElement** aElement)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body) 
    return body->GetTreeBody(aElement);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetColumns(nsITreeColumns** aColumns)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body) 
    return body->GetColumns(aColumns);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetRowHeight(PRInt32* _retval)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body) 
    return body->GetRowHeight(_retval);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetFirstVisibleRow(PRInt32 *_retval)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->GetFirstVisibleRow(_retval);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetLastVisibleRow(PRInt32 *_retval)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->GetLastVisibleRow(_retval);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetPageLength(PRInt32 *_retval)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->GetPageLength(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::EnsureRowIsVisible(PRInt32 aRow)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->EnsureRowIsVisible(aRow);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::ScrollToRow(PRInt32 aRow)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->ScrollToRow(aRow);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::ScrollByLines(PRInt32 aNumLines)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->ScrollByLines(aNumLines);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::ScrollByPages(PRInt32 aNumPages)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->ScrollByPages(aNumPages);
  return NS_OK;
}


NS_IMETHODIMP nsTreeBoxObject::Invalidate()
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->Invalidate();
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::InvalidateColumn(nsITreeColumn* aCol)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->InvalidateColumn(aCol);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::InvalidateRow(PRInt32 aIndex)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->InvalidateRow(aIndex);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::InvalidateCell(PRInt32 aRow, nsITreeColumn* aCol)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->InvalidateCell(aRow, aCol);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::InvalidateRange(PRInt32 aStart, PRInt32 aEnd)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->InvalidateRange(aStart, aEnd);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetRowAt(PRInt32 x, PRInt32 y, PRInt32 *_retval)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->GetRowAt(x, y, _retval);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetCellAt(PRInt32 x, PRInt32 y, PRInt32 *row, nsITreeColumn** col,
                                         nsACString& childElt)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->GetCellAt(x, y, row, col, childElt);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::GetCoordsForCellItem(PRInt32 aRow, nsITreeColumn* aCol, const nsACString& aElement, 
                                      PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->GetCoordsForCellItem(aRow, aCol, aElement, aX, aY, aWidth, aHeight);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::IsCellCropped(PRInt32 aRow, nsITreeColumn* aCol, PRBool *_retval)
{  
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->IsCellCropped(aRow, aCol, _retval);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::RowCountChanged(PRInt32 aIndex, PRInt32 aDelta)
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->RowCountChanged(aIndex, aDelta);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::BeginUpdateBatch()
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->BeginUpdateBatch();
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::EndUpdateBatch()
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->EndUpdateBatch();
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::ClearStyleAndImageCaches()
{
  nsITreeBoxObject* body = GetTreeBody();
  if (body)
    return body->ClearStyleAndImageCaches();
  return NS_OK;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewTreeBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsTreeBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

