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

/* void ensureRowIsVisible (in long rowIndex); */
NS_IMETHODIMP nsTreeBoxObject::EnsureRowIsVisible(PRInt32 aRowIndex)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsCOMPtr<nsITreeFrame> treeFrame(do_QueryInterface(frame));
  if (!treeFrame)
    return NS_OK;

  return treeFrame->EnsureRowIsVisible(aRowIndex);
}

/* void fireOnSelect (); */
NS_IMETHODIMP nsTreeBoxObject::FireOnSelect()
{
  return NS_ERROR_NOT_IMPLEMENTED;
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

