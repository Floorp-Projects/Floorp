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
#include "nsIOutlinerBoxObject.h"
#include "nsBoxObject.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIFrame.h"
#include "nsOutlinerBodyFrame.h"
#include "nsIAtom.h"
#include "nsXULAtoms.h"

class nsOutlinerBoxObject : public nsIOutlinerBoxObject, public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS
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
NS_IMPL_ISUPPORTS_INHERITED(nsOutlinerBoxObject, nsBoxObject, nsIOutlinerBoxObject)


NS_IMETHODIMP
nsOutlinerBoxObject::SetDocument(nsIDocument* aDocument)
{
  // this should only be called with a null document, which indicates
  // that we're being torn down.
  NS_ASSERTION(aDocument == nsnull, "SetDocument called with non-null document");
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
  if (mOutlinerBody)
    return mOutlinerBody;

  nsIFrame* frame = GetFrame();
  if (!frame)
    return nsnull;

  // Iterate over our content model children looking for the body.
  nsCOMPtr<nsIContent> startContent;
  frame->GetContent(getter_AddRefs(startContent));

  nsCOMPtr<nsIContent> content;
  FindBodyElement(startContent, getter_AddRefs(content));

  mPresShell->GetPrimaryFrameFor(content, &frame);

  // It's a frame. Refcounts are irrelevant.
  frame->QueryInterface(NS_GET_IID(nsIOutlinerBoxObject), (void**)&mOutlinerBody);

  nsOutlinerBodyFrame* bodyFrame = NS_STATIC_CAST(nsOutlinerBodyFrame*, frame);
  bodyFrame->SetBoxObject(this);

  return mOutlinerBody;
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
  if (body)
    return body->SetView(aView);
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

NS_IMETHODIMP nsOutlinerBoxObject::GetCellAt(PRInt32 x, PRInt32 y, PRInt32 *row, PRUnichar **colID)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->GetCellAt(x, y, row, colID);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBoxObject::RowCountChanged(PRInt32 aIndex, PRInt32 aDelta)
{
  nsIOutlinerBoxObject* body = GetOutlinerBody();
  if (body)
    return body->RowCountChanged(aIndex, aDelta);
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

