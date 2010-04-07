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

const PRUint32 kDefaultImageMapCacheSize = 256;

nsHTMLImageMapAccessible::
  nsHTMLImageMapAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell,
                           nsIDOMHTMLMapElement *aMapElm) :
  nsHTMLImageAccessibleWrap(aDOMNode, aShell), mMapElement(aMapElm)
{
  mAreaAccCache.Init(kDefaultImageMapCacheSize);
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible: nsISupports and cycle collector

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLImageMapAccessible)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLImageMapAccessible,
                                                  nsAccessible)
CycleCollectorTraverseCache(tmp->mAreaAccCache, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsHTMLImageMapAccessible,
                                                nsAccessible)
ClearCache(tmp->mAreaAccCache);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsHTMLImageMapAccessible)
NS_INTERFACE_MAP_END_INHERITING(nsHTMLImageAccessible)

NS_IMPL_ADDREF_INHERITED(nsHTMLImageMapAccessible, nsHTMLImageAccessible)
NS_IMPL_RELEASE_INHERITED(nsHTMLImageMapAccessible, nsHTMLImageAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible: nsIAccessibleHyperLink

NS_IMETHODIMP
nsHTMLImageMapAccessible::GetAnchorCount(PRInt32 *aAnchorCount)
{
  NS_ENSURE_ARG_POINTER(aAnchorCount);

  return GetChildCount(aAnchorCount);
}

NS_IMETHODIMP
nsHTMLImageMapAccessible::GetURI(PRInt32 aIndex, nsIURI **aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  *aURI = nsnull;

  nsCOMPtr<nsIDOMHTMLCollection> mapAreas = GetAreaCollection();
  if (!mapAreas)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> domNode;
  mapAreas->Item(aIndex, getter_AddRefs(domNode));
  if (!domNode)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIContent> link(do_QueryInterface(domNode));
  if (link)
    *aURI = link->GetHrefURI().get();

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageMapAccessible::GetAnchor(PRInt32 aIndex, nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  nsCOMPtr<nsIDOMHTMLCollection> mapAreas = GetAreaCollection();
  if (mapAreas) {
    nsRefPtr<nsIAccessible> accessible = GetAreaAccessible(mapAreas, aIndex);
    if (!accessible)
      return NS_ERROR_INVALID_ARG;

    NS_ADDREF(*aAccessible = accessible);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageAccessible: nsAccessNode

nsresult
nsHTMLImageMapAccessible::Shutdown()
{
  nsLinkableAccessible::Shutdown();

  ClearCache(mAreaAccCache);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible: nsAccessible public

nsresult
nsHTMLImageMapAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_IMAGE_MAP;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible: nsAccessible protected

void 
nsHTMLImageMapAccessible::CacheChildren()
{
  nsCOMPtr<nsIDOMHTMLCollection> mapAreas = GetAreaCollection();
  if (!mapAreas)
    return;

  PRUint32 areaCount = 0;
  mapAreas->GetLength(&areaCount);

  nsRefPtr<nsAccessible> areaAcc;
  for (PRUint32 areaIdx = 0; areaIdx < areaCount; areaIdx++) {
    areaAcc = GetAreaAccessible(mapAreas, areaIdx);
    if (!areaAcc)
      return;

    mChildren.AppendElement(areaAcc);
    areaAcc->SetParent(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageAccessible

already_AddRefed<nsIDOMHTMLCollection>
nsHTMLImageMapAccessible::GetAreaCollection()
{
  if (!mMapElement)
    return nsnull;

  nsIDOMHTMLCollection *mapAreas = nsnull;
  mMapElement->GetAreas(&mapAreas);
  return mapAreas;
}

already_AddRefed<nsAccessible>
nsHTMLImageMapAccessible::GetAreaAccessible(nsIDOMHTMLCollection *aAreaCollection,
                                            PRInt32 aAreaNum)
{
  if (!aAreaCollection)
    return nsnull;

  nsCOMPtr<nsIDOMNode> domNode;
  aAreaCollection->Item(aAreaNum,getter_AddRefs(domNode));
  if (!domNode)
    return nsnull;

  void *key = reinterpret_cast<void*>(aAreaNum);
  nsRefPtr<nsAccessible> accessible = mAreaAccCache.GetWeak(key);

  if (!accessible) {
    accessible = new nsHTMLAreaAccessible(domNode, this, mWeakShell);
    if (!accessible)
      return nsnull;

    nsresult rv = accessible->Init();
    if (NS_FAILED(rv)) {
      accessible->Shutdown();
      return nsnull;
    }

    mAreaAccCache.Put(key, accessible);
  }

  return accessible.forget();
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLAreaAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLAreaAccessible::
  nsHTMLAreaAccessible(nsIDOMNode *aDomNode, nsIAccessible *aParent,
                       nsIWeakReference* aShell):
  nsHTMLLinkAccessible(aDomNode, aShell)
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

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::alt,
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
  nsCOMPtr<nsIDOMHTMLAreaElement> area(do_QueryInterface(mDOMNode));
  if (area) 
    area->GetShape(aDescription);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAreaAccessible::GetBounds(PRInt32 *x, PRInt32 *y,
                                PRInt32 *width, PRInt32 *height)
{
  nsresult rv;

  // Essentially this uses GetRect on mAreas of nsImageMap from nsImageFrame

  *x = *y = *width = *height = 0;

  nsPresContext *presContext = GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContent> ourContent(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(ourContent, NS_ERROR_FAILURE);

  nsIFrame *frame = GetFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);
  nsIImageFrame *imageFrame = do_QueryFrame(frame);

  nsCOMPtr<nsIImageMap> map;
  imageFrame->GetImageMap(presContext, getter_AddRefs(map));
  NS_ENSURE_TRUE(map, NS_ERROR_FAILURE);

  nsRect rect;
  nsIntRect orgRectPixels;
  rv = map->GetBoundsForAreaContent(ourContent, rect);
  NS_ENSURE_SUCCESS(rv, rv);

  *x      = presContext->AppUnitsToDevPixels(rect.x); 
  *y      = presContext->AppUnitsToDevPixels(rect.y); 

  // XXX Areas are screwy; they return their rects as a pair of points, one pair
  // stored into the width and height.
  *width  = presContext->AppUnitsToDevPixels(rect.width - rect.x);
  *height = presContext->AppUnitsToDevPixels(rect.height - rect.y);

  // Put coords in absolute screen coords
  orgRectPixels = frame->GetScreenRectExternal();
  *x += orgRectPixels.x;
  *y += orgRectPixels.y;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLAreaAccessible: nsAccessible public

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
// nsHTMLAreaAccessible: nsAccessible protected

void
nsHTMLAreaAccessible::CacheChildren()
{
  // No children for aria accessible.
}
