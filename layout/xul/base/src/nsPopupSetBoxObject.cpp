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
#include "nsIPopupSetBoxObject.h"
#include "nsBoxObject.h"
#include "nsIPresShell.h"
#include "nsIPopupSetFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIFrame.h"

class nsPopupSetBoxObject : public nsIPopupSetBoxObject, public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPOPUPSETBOXOBJECT

  nsPopupSetBoxObject();
  virtual ~nsPopupSetBoxObject();
  
protected:
};

/* Implementation file */
NS_IMPL_ADDREF(nsPopupSetBoxObject)
NS_IMPL_RELEASE(nsPopupSetBoxObject)

NS_IMETHODIMP 
nsPopupSetBoxObject::QueryInterface(REFNSIID iid, void** aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  
  if (iid.Equals(NS_GET_IID(nsIPopupSetBoxObject))) {
    *aResult = (nsIPopupSetBoxObject*)this;
    NS_ADDREF(this);
    return NS_OK;
  }

  return nsBoxObject::QueryInterface(iid, aResult);
}
  
nsPopupSetBoxObject::nsPopupSetBoxObject()
{
  NS_INIT_ISUPPORTS();
}

nsPopupSetBoxObject::~nsPopupSetBoxObject()
{
  /* destructor code */
}

/* void openPopupSet (in boolean openFlag); */

NS_IMETHODIMP
nsPopupSetBoxObject::HidePopup()
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsCOMPtr<nsIPopupSetFrame> popupFrame(do_QueryInterface(frame));
  if (!popupFrame)
    return NS_OK;

  return popupFrame->HidePopup();
}

NS_IMETHODIMP
nsPopupSetBoxObject::DestroyPopup()
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsCOMPtr<nsIPopupSetFrame> popupFrame(do_QueryInterface(frame));
  if (!popupFrame)
    return NS_OK;

  return popupFrame->DestroyPopup();
}

NS_IMETHODIMP
nsPopupSetBoxObject::CreatePopup(nsIDOMElement* aSrcContent, 
                                 nsIDOMElement* aPopupContent, 
                                 PRInt32 aXPos, PRInt32 aYPos, 
                                 const PRUnichar *aPopupType, const PRUnichar *anAnchorAlignment, 
                                 const PRUnichar *aPopupAlignment)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsCOMPtr<nsIPopupSetFrame> popupFrame(do_QueryInterface(frame));
  if (!popupFrame)
    return NS_OK;

  nsCOMPtr<nsIContent> popupContent(do_QueryInterface(aPopupContent));
  if (!popupContent)
    return NS_OK;

  nsCOMPtr<nsIContent> srcContent(do_QueryInterface(aSrcContent));
  if (!srcContent)
    return NS_OK;

  nsCOMPtr<nsIDOMDocument> doc;
  aSrcContent->GetOwnerDocument(getter_AddRefs(doc));
  if (!doc)
    return NS_OK;

  nsCOMPtr<nsIDocument> document(do_QueryInterface(doc));
  nsCOMPtr<nsIPresShell> shell = getter_AddRefs(document->GetShellAt(0));
  if (!shell)
    return NS_OK;

  return popupFrame->CreatePopup(srcContent, popupContent, aXPos, aYPos, nsAutoString(aPopupType), nsAutoString(anAnchorAlignment), nsAutoString(aPopupAlignment));
}

NS_IMETHODIMP nsPopupSetBoxObject::GetActiveChild(nsIDOMElement** aResult)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsCOMPtr<nsIPopupSetFrame> popupFrame(do_QueryInterface(frame));
  if (!popupFrame)
    return NS_OK;

  return popupFrame->GetActiveChild(aResult);
}

NS_IMETHODIMP nsPopupSetBoxObject::SetActiveChild(nsIDOMElement* aResult)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsCOMPtr<nsIPopupSetFrame> popupFrame(do_QueryInterface(frame));
  if (!popupFrame)
    return NS_OK;

  return popupFrame->SetActiveChild(aResult);
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewPopupSetBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsPopupSetBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

