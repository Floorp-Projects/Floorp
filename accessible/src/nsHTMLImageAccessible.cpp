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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Author: Aaron Leventhal (aaronl@netscape.com)
 * Contributor(s): 
 */

#include "nsGenericAccessible.h"
#include "nsHTMLImageAccessible.h"
#include "nsReadableUtils.h"
#include "nsAccessible.h"
#include "nsIHTMLDocument.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIAccessibilityService.h"
#include "nsIServiceManager.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "nsIImageFrame.h"

// --- image -----

nsHTMLImageAccessible::nsHTMLImageAccessible(nsIDOMNode* aDOMNode, nsIImageFrame *aImageFrame, nsIWeakReference* aShell):
nsLinkableAccessible(aDOMNode, aShell)
{ 
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(aDOMNode));
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  NS_ASSERTION(shell,"Shell is gone!!! What are we doing here?");

  shell->GetDocument(getter_AddRefs(doc));
  nsAutoString mapElementName;

  if (doc && element) {
    nsCOMPtr<nsIHTMLDocument> htmlDoc(do_QueryInterface(doc));
    element->GetAttribute(NS_LITERAL_STRING("usemap"),mapElementName);
    if (htmlDoc && !mapElementName.IsEmpty()) {
      if (mapElementName.CharAt(0) == '#')
        mapElementName.Cut(0,1);
      htmlDoc->GetImageMap(mapElementName, getter_AddRefs(mMapElement));      
    }
  }
}

NS_IMETHODIMP nsHTMLImageAccessible::GetAccState(PRUint32 *_retval)
{
  // The state is a bitfield, get our inherited state, then logically OR it with STATE_ANIMATED if this
  // is an animated image.

  nsLinkableAccessible::GetAccState(_retval);

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  nsIFrame *frame = nsnull;
  if (content && shell) 
    shell->GetPrimaryFrameFor(content, &frame);

  nsCOMPtr<nsIImageFrame> imageFrame(do_QueryInterface(frame));

  nsCOMPtr<imgIRequest> imageRequest;
  if (imageFrame) 
    imageFrame->GetImageRequest(getter_AddRefs(imageRequest));
  
  nsCOMPtr<imgIContainer> imgContainer;
  if (imageRequest) 
    imageRequest->GetImage(getter_AddRefs(imgContainer));

  if (imgContainer) {
    PRUint32 numFrames;
    imgContainer->GetNumFrames(&numFrames);
    if (numFrames > 1)
      *_retval |= STATE_ANIMATED;
  }

  return NS_OK;
}


/* wstring getAccName (); */
NS_IMETHODIMP nsHTMLImageAccessible::GetAccName(nsAWritableString& _retval)
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> imageContent(do_QueryInterface(mDOMNode));
  if (imageContent) {
    nsAutoString name;
    rv = AppendFlatStringFromContentNode(imageContent, &name);
    if (NS_SUCCEEDED(rv)) {
      // Temp var needed until CompressWhitespace built for nsAWritableString
      name.CompressWhitespace();
      _retval.Assign(name);
    }
  }
  return rv;
}

/* wstring getAccRole (); */
NS_IMETHODIMP nsHTMLImageAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_GRAPHIC;
  return NS_OK;
}


nsIAccessible *nsHTMLImageAccessible::CreateAreaAccessible(PRUint32 areaNum)
{
  if (!mMapElement) 
    return nsnull;

   if (areaNum == -1) {
    PRInt32 numAreaMaps;
    GetAccChildCount(&numAreaMaps);
    if (numAreaMaps<=0)
      return nsnull;
    areaNum = NS_STATIC_CAST(PRUint32,numAreaMaps-1);
  }

  nsIDOMHTMLCollection *mapAreas;
  mMapElement->GetAreas(&mapAreas);
  if (!mapAreas) 
    return nsnull;

  nsIDOMNode *domNode = nsnull;
  mapAreas->Item(areaNum,&domNode);
  if (!domNode)
    return nsnull;

  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  if (!accService)
    return nsnull;
  if (accService) {
    nsIAccessible* acc = nsnull;
    accService->CreateHTMLAreaAccessible(mPresShell, domNode, this, &acc);
    return acc;
  }
  return nsnull;
}


/* nsIAccessible getAccFirstChild (); */
NS_IMETHODIMP nsHTMLImageAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = CreateAreaAccessible(0);
  return NS_OK;
}


/* nsIAccessible getAccLastChild (); */
NS_IMETHODIMP nsHTMLImageAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = CreateAreaAccessible(-1);
  return NS_OK;
}


/* long getAccChildCount (); */
NS_IMETHODIMP nsHTMLImageAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 0;
  if (mMapElement) {
    nsIDOMHTMLCollection *mapAreas;
    mMapElement->GetAreas(&mapAreas);
    if (mapAreas) {
      PRUint32 length;
      mapAreas->GetLength(&length);
      *_retval = NS_STATIC_CAST(PRInt32, length);
    }
  }

  return NS_OK;
}

