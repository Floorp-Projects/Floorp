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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Author: Aaron Leventhal (aaronl@netscape.com)
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

#include "nsHTMLAreaAccessible.h"
#include "nsIAccessibilityService.h"
#include "nsIServiceManager.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIFrame.h"
#include "nsIImageFrame.h"
#include "nsIImageMap.h"


// --- area -----

nsHTMLAreaAccessible::nsHTMLAreaAccessible(nsIDOMNode *aDomNode, nsIAccessible *aParent, nsIWeakReference* aShell):
nsLinkableAccessible(aDomNode, aShell)
{ 
  Init(); // Make sure we're in cache
  mParent = aParent;
}

/* wstring getName (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetName(nsAString & _retval)
{
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
  if (elt) {
    nsAutoString hrefString;
    elt->GetAttribute(NS_LITERAL_STRING("title"), _retval);
    if (_retval.IsEmpty())
      GetValue(_retval);
  }
  return NS_OK;
}

/* unsigned long getRole (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_LINK;
  return NS_OK;
}

/* wstring getDescription (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetDescription(nsAString& _retval)
{
  // Still to do - follow IE's standard here
  nsCOMPtr<nsIDOMHTMLAreaElement> area(do_QueryInterface(mDOMNode));
  if (area) 
    area->GetShape(_retval);
  return NS_OK;
}


/* nsIAccessible getFirstChild (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetFirstChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/* nsIAccessible getLastChild (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetLastChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/* long getAccChildCount (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetChildCount(PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}

/* void accGetBounds (out long x, out long y, out long width, out long height); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  // Essentially this uses GetRect on mAreas of nsImageMap from nsImageFrame

  *x = *y = *width = *height = 0;

  nsCOMPtr<nsIPresContext> presContext(GetPresContext());
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContent> ourContent(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(ourContent, NS_ERROR_FAILURE);

  nsIFrame *frame = GetFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);
  nsIImageFrame *imageFrame;
  nsresult rv = frame->QueryInterface(NS_GET_IID(nsIImageFrame), (void**)&imageFrame);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIImageMap> map;
  imageFrame->GetImageMap(presContext, getter_AddRefs(map));
  NS_ENSURE_TRUE(map, NS_ERROR_FAILURE);

  nsRect rect, orgRectPixels, pageRectPixels;
  rv = map->GetBoundsForAreaContent(ourContent, presContext, rect);
  NS_ENSURE_SUCCESS(rv, rv);

  // Convert from twips to pixels
  float t2p;
  t2p = presContext->TwipsToPixels();   // Get pixels conversion factor
  *x      = NSTwipsToIntPixels(rect.x, t2p); 
  *y      = NSTwipsToIntPixels(rect.y, t2p); 

  // XXX aaronl not sure why we have to subtract the x,y from the width, height
  //     -- but it works perfectly!
  *width  = NSTwipsToIntPixels(rect.width, t2p) - *x;
  *height = NSTwipsToIntPixels(rect.height, t2p) - *y;

  // Put coords in absolute screen coords
  GetScreenOrigin(presContext, frame, &orgRectPixels);
  GetScrollOffset(&pageRectPixels);
  *x += orgRectPixels.x - pageRectPixels.x;
  *y += orgRectPixels.y - pageRectPixels.y;

  return NS_OK;
}

