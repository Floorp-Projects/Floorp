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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David W. Hyatt (hyatt@netscape.com)
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
#include "nsCOMPtr.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIOutlinerBoxObject.h"
#include "nsBoxObject.h"
#include "nsIFrame.h"
#include "nsOutlinerBodyFrame.h"
#include "nsIAtom.h"
#include "nsXULAtoms.h"

class nsOutlinerBoxObject : public nsIOutlinerBoxObject, public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOUTLINERBOXOBJECT

  nsOutlinerBoxObject();
  virtual ~nsOutlinerBoxObject();

  nsIOutlinerBoxObject* GetOutlinerBody();

  //NS_PIBOXOBJECT interfaces
  NS_IMETHOD Init(nsIContent* aContent, nsIPresShell* aPresShell);
  NS_IMETHOD SetDocument(nsIDocument* aDocument);

  nsIOutlinerBoxObject* mOutlinerBody;
};

/* Implementation file */
NS_IMPL_ISUPPORTS_INHERITED1(nsOutlinerBoxObject, nsBoxObject, nsIOutlinerBoxObject)


NS_IMETHODIMP
nsOutlinerBoxObject::SetDocument(nsIDocument* aDocument)
{
  // this should only be called with a null document, which indicates
  // that we're being torn down.
  NS_ASSERTION(aDocument == nsnull, "SetDocument called with non-null document");

  // Drop the view's ref to us.
  nsAutoString view; view.AssignWithConversion("view");
  nsCOMPtr<nsISupports> suppView;
  GetPropertyAsSupports(view.get(), getter_AddRefs(suppView));
  nsCOMPtr<nsIOutlinerView> outlinerView(do_QueryInterface(suppView));
  if (outlinerView)
    outlinerView->SetOutliner(nsnull); // Break the circular ref between the view and us.

  mOutlinerBody = nsnull;

  return nsBoxObject::SetDocument(aDocument);
}

  
nsOutlinerBoxObject::nsOutlinerBoxObject()
:mOutlinerBody(nsnull)
{
  NS_INIT_ISUPPORTS();
}

nsOutlinerBoxObject::~nsOutlinerBoxObject()
{
  /* destructor code */
}


NS_IMETHODIMP nsOutlinerBoxObject::Init(nsIContent* aContent, nsIPresShell* aPresShell)
{
  nsresult rv = nsBoxObject::Init(aContent, aPresShell);
  if (NS_FAILED(rv)) return rv;
  return NS_OK;
}

static void FindBodyElement(nsIContent* aParent, nsIContent** aResult)
{
  nsCOMPtr<nsIAtom> tag;
  aParent->GetTag(*getter_AddRefs(tag));
  if (tag.get() == nsXULAtoms::outlinerbody) {
    *aResult = aParent;
    NS_IF_ADDREF(*aResult);
  }
  else {
    PRInt32 count;
    aParent->ChildCount(count);
    for (PRInt32 i = 0; i < count; i++) {
      nsCOMPtr<nsIContent> child;
      aParent->ChildAt(i, *getter_AddRefs(child));
      FindBodyElement(child, aResult);
      if (*aResult)
        break;
    }
  }
}

inline nsIOutlinerBoxObject*
nsOutlinerBoxObject::GetOutlinerBody()
{
  nsAutoString outlinerbody; outlinerbody.AssignWithConversion("outlinerbody");

  nsCOMPtr<nsISupports> supp;
  GetPropertyAsSupports(outlinerbody.get(), getter_AddRefs(supp));

  if (supp) {
    nsCOMPtr<nsIOutlinerBoxObject> body(do_QueryInterface(supp));
    return body;
  }

  nsIFrame* frame = GetFrame();
  if (!frame)
    return nsnull;

  // Iterate over our content model children looking for the body.
  nsCOMPtr<nsIContent> startContent;
  frame->GetContent(getter_AddRefs(startContent));

  nsCOMPtr<nsIContent> content;
  FindBodyElement(startContent, getter_AddRefs(content));

  mPresShell->GetPrimaryFrameFor(content, &frame);
  if (!frame)
     return nsnull;

  // It's a frame. Refcounts are irrelevant.
  nsCOMPtr<nsIOutlinerBoxObject> body;
  frame->QueryInterface(NS_GET_IID(nsIOutlinerBoxObject), getter_AddRefs(body));
  SetPropertyAsSupports(outlinerbody.get(), body);
  return body;
}

NS_IMETHODIMP nsOutlinerBoxObject::GetView(nsIOutlinerView * *aView)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->GetView(aView);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::SetView(nsIOutlinerView * aView)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body) {
    body->SetView(aView);
  
    // only return if the body frame was able to store the view,
    // else we need to cache the property below
    nsCOMPtr<nsIOutlinerView> view;
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

NS_IMETHODIMP nsOutlinerBoxObject::GetFocused(PRBool* aFocused)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->GetFocused(aFocused);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::SetFocused(PRBool aFocused)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->SetFocused(aFocused);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::GetOutlinerBody(nsIDOMElement** aElement)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body) 
    return body->GetOutlinerBody(aElement);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::GetSelection(nsIOutlinerSelection * *aSelection)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->GetSelection(aSelection);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::GetRowHeight(PRInt32* _retval)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body) 
    return body->GetRowHeight(_retval);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::GetFirstVisibleRow(PRInt32 *_retval)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->GetFirstVisibleRow(_retval);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::GetLastVisibleRow(PRInt32 *_retval)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->GetLastVisibleRow(_retval);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::GetPageCount(PRInt32 *_retval)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->GetPageCount(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerBoxObject::EnsureRowIsVisible(PRInt32 aRow)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->EnsureRowIsVisible(aRow);
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerBoxObject::ScrollToRow(PRInt32 aRow)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->ScrollToRow(aRow);
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerBoxObject::ScrollByLines(PRInt32 aNumLines)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->ScrollByLines(aNumLines);
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerBoxObject::ScrollByPages(PRInt32 aNumPages)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->ScrollByPages(aNumPages);
  return NS_OK;
}


NS_IMETHODIMP nsOutlinerBoxObject::Invalidate()
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->Invalidate();
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::InvalidateRow(PRInt32 aIndex)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->InvalidateRow(aIndex);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::InvalidateCell(PRInt32 aRow, const PRUnichar *aColID)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->InvalidateCell(aRow, aColID);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::InvalidateRange(PRInt32 aStart, PRInt32 aEnd)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->InvalidateRange(aStart, aEnd);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::InvalidateScrollbar()
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->InvalidateScrollbar();
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::GetCellAt(PRInt32 x, PRInt32 y, PRInt32 *row, PRUnichar **colID,
                                             PRUnichar** childElt)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->GetCellAt(x, y, row, colID, childElt);
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerBoxObject::GetCoordsForCellItem(PRInt32 aRow, const PRUnichar *aColID, const PRUnichar *aCellItem, 
                                          PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->GetCoordsForCellItem(aRow, aColID, aCellItem, aX, aY, aWidth, aHeight);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::RowCountChanged(PRInt32 aIndex, PRInt32 aDelta)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->RowCountChanged(aIndex, aDelta);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::OnDragEnter(nsIDOMEvent* inEvent)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->OnDragEnter(inEvent);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::OnDragExit(nsIDOMEvent* inEvent)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->OnDragExit(inEvent);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::OnDragOver(nsIDOMEvent* inEvent)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->OnDragOver(inEvent);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::OnDragDrop(nsIDOMEvent* inEvent)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->OnDragDrop(inEvent);
  return NS_OK;
}



// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewOutlinerBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsOutlinerBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

