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
#include "nsIMenuBoxObject.h"
#include "nsBoxObject.h"
#include "nsIPresShell.h"
#include "nsIMenuFrame.h"
#include "nsIFrame.h"

class nsMenuBoxObject : public nsIMenuBoxObject, public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMENUBOXOBJECT

  nsMenuBoxObject();
  virtual ~nsMenuBoxObject();
  
protected:
};

/* Implementation file */
NS_IMPL_ADDREF(nsMenuBoxObject)
NS_IMPL_RELEASE(nsMenuBoxObject)

NS_IMETHODIMP 
nsMenuBoxObject::QueryInterface(REFNSIID iid, void** aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  
  if (iid.Equals(NS_GET_IID(nsIMenuBoxObject))) {
    *aResult = (nsIMenuBoxObject*)this;
    NS_ADDREF(this);
    return NS_OK;
  }

  return nsBoxObject::QueryInterface(iid, aResult);
}
  
nsMenuBoxObject::nsMenuBoxObject()
{
  NS_INIT_ISUPPORTS();
}

nsMenuBoxObject::~nsMenuBoxObject()
{
  /* destructor code */
}

/* void openMenu (in boolean openFlag); */
NS_IMETHODIMP nsMenuBoxObject::OpenMenu(PRBool aOpenFlag)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsCOMPtr<nsIMenuFrame> menuFrame(do_QueryInterface(frame));
  if (!menuFrame)
    return NS_OK;

  return menuFrame->OpenMenu(aOpenFlag);
}

NS_IMETHODIMP nsMenuBoxObject::GetActiveChild(nsIDOMElement** aResult)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsCOMPtr<nsIMenuFrame> menuFrame(do_QueryInterface(frame));
  if (!menuFrame)
    return NS_OK;

  return menuFrame->GetActiveChild(aResult);
}

NS_IMETHODIMP nsMenuBoxObject::SetActiveChild(nsIDOMElement* aResult)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsCOMPtr<nsIMenuFrame> menuFrame(do_QueryInterface(frame));
  if (!menuFrame)
    return NS_OK;

  return menuFrame->SetActiveChild(aResult);
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewMenuBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsMenuBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

