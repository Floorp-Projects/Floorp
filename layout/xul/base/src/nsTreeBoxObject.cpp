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
#include "nsITreeBoxObject.h"
#include "nsBoxObject.h"
#include "nsIPresShell.h"
#include "nsITreeFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIPresContext.h"
#include "nsIFrame.h"

class nsTreeBoxObject : public nsITreeBoxObject, public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITREEBOXOBJECT

  nsTreeBoxObject();
  virtual ~nsTreeBoxObject();
  
protected:
};

/* Implementation file */
NS_IMPL_ADDREF(nsTreeBoxObject)
NS_IMPL_RELEASE(nsTreeBoxObject)

NS_IMETHODIMP 
nsTreeBoxObject::QueryInterface(REFNSIID iid, void** aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  
  if (iid.Equals(NS_GET_IID(nsITreeBoxObject))) {
    *aResult = (nsITreeBoxObject*)this;
    NS_ADDREF(this);
    return NS_OK;
  }

  return nsBoxObject::QueryInterface(iid, aResult);
}
  
nsTreeBoxObject::nsTreeBoxObject()
{
  NS_INIT_ISUPPORTS();
}

nsTreeBoxObject::~nsTreeBoxObject()
{
  /* destructor code */
}

/* void ensureIndexIsVisible (in long rowIndex); */
NS_IMETHODIMP nsTreeBoxObject::EnsureIndexIsVisible(PRInt32 aRowIndex)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;
  
  nsCOMPtr<nsITreeFrame> treeFrame(do_QueryInterface(frame));
  return treeFrame->EnsureRowIsVisible(aRowIndex);
}

/* void scrollToIndex (in long rowIndex); */
NS_IMETHODIMP nsTreeBoxObject::ScrollToIndex(PRInt32 aRowIndex)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;
  
  nsCOMPtr<nsITreeFrame> treeFrame(do_QueryInterface(frame));
  return treeFrame->ScrollToIndex(aRowIndex);
}

/* nsIDOMElement getNextItem (in nsIDOMElement startItem, in long delta); */
NS_IMETHODIMP nsTreeBoxObject::GetNextItem(nsIDOMElement *aStartItem, PRInt32 aDelta, nsIDOMElement **aResult)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsCOMPtr<nsITreeFrame> treeFrame(do_QueryInterface(frame));
  
  return treeFrame->GetNextItem(aStartItem, aDelta, aResult);
}

/* nsIDOMElement getPreviousItem (in nsIDOMElement startItem, in long delta); */
NS_IMETHODIMP nsTreeBoxObject::GetPreviousItem(nsIDOMElement *aStartItem, PRInt32 aDelta, nsIDOMElement **aResult)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsCOMPtr<nsITreeFrame> treeFrame(do_QueryInterface(frame));
  return treeFrame->GetPreviousItem(aStartItem, aDelta, aResult);
}

/* nsIDOMElement getItemAtIndex (in long index); */
NS_IMETHODIMP nsTreeBoxObject::GetItemAtIndex(PRInt32 index, nsIDOMElement **_retval)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsCOMPtr<nsITreeFrame> treeFrame(do_QueryInterface(frame));
  return treeFrame->GetItemAtIndex(index, _retval);
}

/* long getIndexOfItem (in nsIDOMElement item); */
NS_IMETHODIMP nsTreeBoxObject::GetIndexOfItem(nsIDOMElement* aElement, PRInt32 *aResult)
{
  *aResult = 0;

  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsCOMPtr<nsITreeFrame> treeFrame(do_QueryInterface(frame));
  nsCOMPtr<nsIPresContext> presContext;
  mPresShell->GetPresContext(getter_AddRefs(presContext));
  return treeFrame->GetIndexOfItem(presContext, aElement, aResult);
}

/* void getNumberOfVisibleRows (); */
NS_IMETHODIMP nsTreeBoxObject::GetNumberOfVisibleRows(PRInt32 *aResult)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;
  
  nsCOMPtr<nsITreeFrame> treeFrame(do_QueryInterface(frame));
  return treeFrame->GetNumberOfVisibleRows(aResult);
}

/* void getIndexOfFirstVisibleRow (); */
NS_IMETHODIMP nsTreeBoxObject::GetIndexOfFirstVisibleRow(PRInt32 *aResult)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;
  
  nsCOMPtr<nsITreeFrame> treeFrame(do_QueryInterface(frame));
  return treeFrame->GetIndexOfFirstVisibleRow(aResult);
}

NS_IMETHODIMP nsTreeBoxObject::GetRowCount(PRInt32 *aResult)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;
  
  nsCOMPtr<nsITreeFrame> treeFrame(do_QueryInterface(frame));
  return treeFrame->GetRowCount(aResult);
}

NS_IMETHODIMP nsTreeBoxObject::BeginBatch()
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;
  
  nsCOMPtr<nsITreeFrame> treeFrame(do_QueryInterface(frame));
  return treeFrame->BeginBatch();
}

NS_IMETHODIMP nsTreeBoxObject::EndBatch()
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;
  
  nsCOMPtr<nsITreeFrame> treeFrame(do_QueryInterface(frame));
  return treeFrame->EndBatch();
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
