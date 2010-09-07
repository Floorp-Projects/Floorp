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
 *   Aaron Leventhal <aaronl@netscape.com> (original author)
 *   Alexander Surkov <surkov.alexander@gmail.com>
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

#include "nsHTMLImageMapAccessible.h"

#include "nsAccUtils.h"

#include "nsIDOMHTMLCollection.h"
#include "nsIServiceManager.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIFrame.h"
#include "nsIImageFrame.h"
#include "nsIImageMap.h"

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLImageMapAccessible::
  nsHTMLImageMapAccessible(nsIContent *aContent, nsIWeakReference *aShell,
                           nsIDOMHTMLMapElement *aMapElm) :
  nsHTMLImageAccessibleWrap(aContent, aShell), mMapElement(aMapElm)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible: nsISupports

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLImageMapAccessible, nsHTMLImageAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible: nsAccessible public

PRUint32
nsHTMLImageMapAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_IMAGE_MAP;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible: HyperLinkAccessible

PRUint32
nsHTMLImageMapAccessible::AnchorCount()
{
  return GetChildCount();
}

nsAccessible*
nsHTMLImageMapAccessible::GetAnchor(PRUint32 aAnchorIndex)
{
  return GetChildAt(aAnchorIndex);
}

already_AddRefed<nsIURI>
nsHTMLImageMapAccessible::GetAnchorURI(PRUint32 aAnchorIndex)
{
  nsAccessible* area = GetChildAt(aAnchorIndex);
  if (!area)
    return nsnull;

  nsIContent* linkContent = area->GetContent();
  return linkContent ? linkContent->GetHrefURI() : nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible: nsAccessible protected

void 
nsHTMLImageMapAccessible::CacheChildren()
{
  if (!mMapElement)
    return;

  nsCOMPtr<nsIDOMHTMLCollection> mapAreas;
  mMapElement->GetAreas(getter_AddRefs(mapAreas));
  if (!mapAreas)
    return;

  PRUint32 areaCount = 0;
  mapAreas->GetLength(&areaCount);

  for (PRUint32 areaIdx = 0; areaIdx < areaCount; areaIdx++) {
    nsCOMPtr<nsIDOMNode> areaNode;
    mapAreas->Item(areaIdx, getter_AddRefs(areaNode));
    if (!areaNode)
      return;

    nsCOMPtr<nsIContent> areaContent(do_QueryInterface(areaNode));
    nsRefPtr<nsAccessible> areaAcc =
      new nsHTMLAreaAccessible(areaContent, mWeakShell);
    if (!areaAcc)
      return;

    if (!areaAcc->Init()) {
      areaAcc->Shutdown();
      return;
    }

    // We must respect ARIA on area elements (for the canvas map technique)
    areaAcc->SetRoleMapEntry(nsAccUtils::GetRoleMapEntry(areaContent));

    if (!AppendChild(areaAcc))
      return;
  }
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLAreaAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLAreaAccessible::
  nsHTMLAreaAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsHTMLLinkAccessible(aContent, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLAreaAccessible: nsIAccessible

nsresult
nsHTMLAreaAccessible::GetNameInternal(nsAString & aName)
{
  nsresult rv = nsAccessible::GetNameInternal(aName);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aName.IsEmpty())
    return NS_OK;

  if (!mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::alt,
                         aName)) {
    return GetValue(aName);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAreaAccessible::GetDescription(nsAString& aDescription)
{
  aDescription.Truncate();

  // Still to do - follow IE's standard here
  nsCOMPtr<nsIDOMHTMLAreaElement> area(do_QueryInterface(mContent));
  if (area) 
    area->GetShape(aDescription);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAreaAccessible::GetBounds(PRInt32 *aX, PRInt32 *aY,
                                PRInt32 *aWidth, PRInt32 *aHeight)
{
  NS_ENSURE_ARG_POINTER(aX);
  *aX = 0;
  NS_ENSURE_ARG_POINTER(aY);
  *aY = 0;
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = 0;
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // Essentially this uses GetRect on mAreas of nsImageMap from nsImageFrame.
  nsPresContext *presContext = GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  nsIFrame *frame = GetFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);
  nsIImageFrame *imageFrame = do_QueryFrame(frame);

  nsCOMPtr<nsIImageMap> map;
  imageFrame->GetImageMap(presContext, getter_AddRefs(map));
  NS_ENSURE_TRUE(map, NS_ERROR_FAILURE);

  nsRect rect;
  nsIntRect orgRectPixels;
  nsresult rv = map->GetBoundsForAreaContent(mContent, rect);
  NS_ENSURE_SUCCESS(rv, rv);

  *aX = presContext->AppUnitsToDevPixels(rect.x);
  *aY = presContext->AppUnitsToDevPixels(rect.y);

  // XXX Areas are screwy; they return their rects as a pair of points, one pair
  // stored into the width and height.
  *aWidth  = presContext->AppUnitsToDevPixels(rect.width - rect.x);
  *aHeight = presContext->AppUnitsToDevPixels(rect.height - rect.y);

  // Put coords in absolute screen coords
  orgRectPixels = frame->GetScreenRectExternal();
  *aX += orgRectPixels.x;
  *aY += orgRectPixels.y;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLAreaAccessible: nsAccessible public

nsresult
nsHTMLAreaAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  // Bypass the link states specialization for non links.
  if (mRoleMapEntry &&
      mRoleMapEntry->role != nsIAccessibleRole::ROLE_NOTHING &&
      mRoleMapEntry->role != nsIAccessibleRole::ROLE_LINK) {
    return nsAccessible::GetStateInternal(aState,aExtraState);
  }

  return nsHTMLLinkAccessible::GetStateInternal(aState, aExtraState);
}

nsresult
nsHTMLAreaAccessible::GetChildAtPoint(PRInt32 aX, PRInt32 aY,
                                      PRBool aDeepestChild,
                                      nsIAccessible **aChild)
{
  // Don't walk into area accessibles.
  NS_ADDREF(*aChild = this);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible: HyperLinkAccessible

PRUint32
nsHTMLAreaAccessible::StartOffset()
{
  // Image map accessible is not hypertext accessible therefore
  // StartOffset/EndOffset implementations of nsAccessible doesn't work here.
  // We return index in parent because image map contains area links only which
  // are embedded objects.
  // XXX: image map should be a hypertext accessible.
  return GetIndexInParent();
}

PRUint32
nsHTMLAreaAccessible::EndOffset()
{
  return GetIndexInParent() + 1;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLAreaAccessible: nsAccessible protected

void
nsHTMLAreaAccessible::CacheChildren()
{
  // No children for aria accessible.
}
