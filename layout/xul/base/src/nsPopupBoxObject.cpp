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
 * Original Authors: 
 *   David W. Hyatt <hyatt@netscape.com>
 *   Ben Goodger <ben@netscape.com>
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "nsIPopupBoxObject.h"
#include "nsIPopupSetFrame.h"
#include "nsIRootBox.h"
#include "nsBoxObject.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIFrame.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsMenuPopupFrame.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"


class nsPopupBoxObject : public nsIPopupBoxObject, public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPOPUPBOXOBJECT

  nsPopupBoxObject();
  virtual ~nsPopupBoxObject();

};

/* Implementation file */
NS_IMPL_ADDREF(nsPopupBoxObject)
NS_IMPL_RELEASE(nsPopupBoxObject)

NS_IMETHODIMP 
nsPopupBoxObject::QueryInterface(REFNSIID iid, void** aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  
  if (iid.Equals(NS_GET_IID(nsIPopupBoxObject))) {
    *aResult = (nsIPopupBoxObject*)this;
    NS_ADDREF(this);
    return NS_OK;
  }

  return nsBoxObject::QueryInterface(iid, aResult);
}
  
nsPopupBoxObject::nsPopupBoxObject()
{
  NS_INIT_ISUPPORTS();
}

nsPopupBoxObject::~nsPopupBoxObject()
{
  /* destructor code */
}

/* void openPopup (in boolean openFlag); */

NS_IMETHODIMP
nsPopupBoxObject::HidePopup()
{
  nsIFrame* ourFrame = GetFrame();
  if (!ourFrame)
    return NS_OK;
  
  nsIFrame* rootFrame;
  mPresShell->GetRootFrame(&rootFrame);
  if (!rootFrame)
    return NS_OK;

  if (rootFrame) {
    nsCOMPtr<nsIPresContext> presContext;
    mPresShell->GetPresContext(getter_AddRefs(presContext));
    rootFrame->FirstChild(presContext, nsnull, &rootFrame);   
  }

  nsCOMPtr<nsIRootBox> rootBox(do_QueryInterface(rootFrame));
  if (!rootBox)
    return NS_OK;

  nsIFrame* popupSetFrame;
  rootBox->GetPopupSetFrame(&popupSetFrame);
  if (!popupSetFrame)
    return NS_OK;

  nsCOMPtr<nsIPopupSetFrame> popupSet(do_QueryInterface(popupSetFrame));
  if (!popupSet)
    return NS_OK;

  popupSet->HidePopup(ourFrame);
  popupSet->DestroyPopup(ourFrame);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::ShowPopup(nsIDOMElement* aSrcContent, 
                            nsIDOMElement* aPopupContent, 
                            PRInt32 aXPos, PRInt32 aYPos, 
                            const PRUnichar *aPopupType, const PRUnichar *anAnchorAlignment, 
                            const PRUnichar *aPopupAlignment)
{
  nsIFrame* rootFrame;
  mPresShell->GetRootFrame(&rootFrame);
  if (!rootFrame)
    return NS_OK;

  if (rootFrame) {
    nsCOMPtr<nsIPresContext> presContext;
    mPresShell->GetPresContext(getter_AddRefs(presContext));
    rootFrame->FirstChild(presContext, nsnull, &rootFrame);   
  }

  nsCOMPtr<nsIRootBox> rootBox(do_QueryInterface(rootFrame));
  if (!rootBox)
    return NS_OK;

  nsIFrame* popupSetFrame;
  rootBox->GetPopupSetFrame(&popupSetFrame);
  if (!popupSetFrame)
    return NS_OK;

  nsCOMPtr<nsIPopupSetFrame> popupSet(do_QueryInterface(popupSetFrame));
  if (!popupSet)
    return NS_OK;

  nsCOMPtr<nsIContent> srcContent(do_QueryInterface(aSrcContent));
  nsCOMPtr<nsIContent> popupContent(do_QueryInterface(aPopupContent));

  nsAutoString popupType(aPopupType);
  nsAutoString anchorAlign(anAnchorAlignment);
  nsAutoString popupAlign(aPopupAlignment);

  // Use |left| and |top| dimension attributes to position the popup if
  // present, as they may have been persisted. 
  nsAutoString left, top;
  popupContent->GetAttr(kNameSpaceID_None, nsXULAtoms::left, left);
  popupContent->GetAttr(kNameSpaceID_None, nsXULAtoms::top, top);
  
  PRInt32 err;
  if (!left.IsEmpty()) {
    aXPos = left.ToInteger(&err);
    if (NS_FAILED(err))
      return err;
  }
  if (!top.IsEmpty()) {
    aYPos = top.ToInteger(&err);
    if (NS_FAILED(err))
      return err;
  }

  return popupSet->ShowPopup(srcContent, popupContent, aXPos, aYPos, 
                             popupType, anchorAlign, popupAlign);
}

NS_IMETHODIMP
nsPopupBoxObject::MoveTo(PRInt32 aLeft, PRInt32 aTop)
{
  nsIFrame* frame = GetFrame();
  if (!frame) return NS_OK;

  nsMenuPopupFrame* menuPopupFrame = NS_STATIC_CAST(nsMenuPopupFrame*, frame);
  if (!menuPopupFrame) return NS_OK;

  menuPopupFrame->MoveTo(aLeft, aTop);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::SizeTo(PRInt32 aWidth, PRInt32 aHeight)
{
  nsAutoString width, height;
  width.AppendInt(aWidth);
  height.AppendInt(aHeight);
  
  mContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::width, width, PR_FALSE);
  mContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::height, height, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::GetAutoPosition(PRBool* aShouldAutoPosition)
{
  nsIFrame* frame = GetFrame();
  if (!frame) return NS_OK;

  nsMenuPopupFrame* menuPopupFrame = NS_STATIC_CAST(nsMenuPopupFrame*, frame);
  if (!menuPopupFrame) return NS_OK;

  menuPopupFrame->GetAutoPosition(aShouldAutoPosition);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::SetAutoPosition(PRBool aShouldAutoPosition)
{
  nsIFrame* frame = GetFrame();
  if (!frame) return NS_OK;

  nsMenuPopupFrame* menuPopupFrame = NS_STATIC_CAST(nsMenuPopupFrame*, frame);
  if (!menuPopupFrame) return NS_OK;

  menuPopupFrame->SetAutoPosition(aShouldAutoPosition);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::EnableRollup(PRBool aShouldRollup)
{
  nsIFrame* frame = GetFrame();
  if (!frame) return NS_OK;

  nsMenuPopupFrame* menuPopupFrame = NS_STATIC_CAST(nsMenuPopupFrame*, frame);
  if (!menuPopupFrame) return NS_OK;

  menuPopupFrame->EnableRollup(aShouldRollup);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::EnableKeyboardNavigator(PRBool aEnableKeyboardNavigator)
{
  nsIFrame* frame = GetFrame();
  if (!frame) return NS_OK;

  nsMenuPopupFrame* menuPopupFrame = NS_STATIC_CAST(nsMenuPopupFrame*, frame);
  if (!menuPopupFrame) return NS_OK;

  if (aEnableKeyboardNavigator)
    menuPopupFrame->InstallKeyboardNavigator();
  else
    menuPopupFrame->RemoveKeyboardNavigator();
  
  return NS_OK;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewPopupBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsPopupBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

