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
#include "nsDocAccessible.h"
#include "Role.h"

#include "nsIDOMHTMLCollection.h"
#include "nsIServiceManager.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIFrame.h"
#include "nsImageFrame.h"
#include "nsImageMap.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLImageMapAccessible::
  nsHTMLImageMapAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsHTMLImageAccessibleWrap(aContent, aDoc)
{
  mFlags |= eImageMapAccessible;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible: nsISupports

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLImageMapAccessible, nsHTMLImageAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible: nsAccessible public

role
nsHTMLImageMapAccessible::NativeRole()
{
  return roles::IMAGE_MAP;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible: HyperLinkAccessible

PRUint32
nsHTMLImageMapAccessible::AnchorCount()
{
  return GetChildCount();
}

nsAccessible*
nsHTMLImageMapAccessible::AnchorAt(PRUint32 aAnchorIndex)
{
  return GetChildAt(aAnchorIndex);
}

already_AddRefed<nsIURI>
nsHTMLImageMapAccessible::AnchorURIAt(PRUint32 aAnchorIndex)
{
  nsAccessible* area = GetChildAt(aAnchorIndex);
  if (!area)
    return nsnull;

  nsIContent* linkContent = area->GetContent();
  return linkContent ? linkContent->GetHrefURI() : nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible: public

void
nsHTMLImageMapAccessible::UpdateChildAreas(bool aDoFireEvents)
{
  nsImageFrame* imageFrame = do_QueryFrame(mContent->GetPrimaryFrame());

  // If image map is not initialized yet then we trigger one time more later.
  nsImageMap* imageMapObj = imageFrame->GetExistingImageMap();
  if (!imageMapObj)
    return;

  bool doReorderEvent = false;

  // Remove areas that are not a valid part of the image map anymore.
  for (PRInt32 childIdx = mChildren.Length() - 1; childIdx >= 0; childIdx--) {
    nsAccessible* area = mChildren.ElementAt(childIdx);
    if (area->GetContent()->GetPrimaryFrame())
      continue;

    if (aDoFireEvents) {
      nsRefPtr<AccEvent> event = new AccHideEvent(area, area->GetContent());
      mDoc->FireDelayedAccessibleEvent(event);
      doReorderEvent = true;
    }

    RemoveChild(area);
  }

  // Insert new areas into the tree.
  PRUint32 areaElmCount = imageMapObj->AreaCount();
  for (PRUint32 idx = 0; idx < areaElmCount; idx++) {
    nsIContent* areaContent = imageMapObj->GetAreaAt(idx);

    nsAccessible* area = mChildren.SafeElementAt(idx);
    if (!area || area->GetContent() != areaContent) {
      nsRefPtr<nsAccessible> area = new nsHTMLAreaAccessible(areaContent, mDoc);
      if (!mDoc->BindToDocument(area, nsAccUtils::GetRoleMapEntry(areaContent)))
        break;

      if (!InsertChildAt(idx, area)) {
        mDoc->UnbindFromDocument(area);
        break;
      }

      if (aDoFireEvents) {
        nsRefPtr<AccEvent> event = new AccShowEvent(area, areaContent);
        mDoc->FireDelayedAccessibleEvent(event);
        doReorderEvent = true;
      }
    }
  }

  // Fire reorder event if needed.
  if (doReorderEvent) {
    nsRefPtr<AccEvent> reorderEvent =
      new AccEvent(nsIAccessibleEvent::EVENT_REORDER, mContent,
                   eAutoDetect, AccEvent::eCoalesceFromSameSubtree);
    mDoc->FireDelayedAccessibleEvent(reorderEvent);
  }
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLImageMapAccessible: nsAccessible protected

void
nsHTMLImageMapAccessible::CacheChildren()
{
  UpdateChildAreas(false);
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLAreaAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLAreaAccessible::
  nsHTMLAreaAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsHTMLLinkAccessible(aContent, aDoc)
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

  if (!mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::alt, aName))
    return GetValue(aName);

  return NS_OK;
}

void
nsHTMLAreaAccessible::Description(nsString& aDescription)
{
  aDescription.Truncate();

  // Still to do - follow IE's standard here
  nsCOMPtr<nsIDOMHTMLAreaElement> area(do_QueryInterface(mContent));
  if (area) 
    area->GetShape(aDescription);
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLAreaAccessible: nsAccessNode public

bool
nsHTMLAreaAccessible::IsPrimaryForNode() const
{
  // Make HTML area DOM element not accessible. HTML image map accessible
  // manages its tree itself.
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLAreaAccessible: nsAccessible public

PRUint64
nsHTMLAreaAccessible::NativeState()
{
  // Bypass the link states specialization for non links.
  if (mRoleMapEntry &&
      mRoleMapEntry->role != roles::NOTHING &&
      mRoleMapEntry->role != roles::LINK) {
    return nsAccessible::NativeState();
  }

  return nsHTMLLinkAccessible::NativeState();
}

nsAccessible*
nsHTMLAreaAccessible::ChildAtPoint(PRInt32 aX, PRInt32 aY,
                                   EWhichChildAtPoint aWhichChild)
{
  // Don't walk into area accessibles.
  return this;
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
  return IndexInParent();
}

PRUint32
nsHTMLAreaAccessible::EndOffset()
{
  return IndexInParent() + 1;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLAreaAccessible: nsAccessible protected

void
nsHTMLAreaAccessible::CacheChildren()
{
  // No children for aria accessible.
}

void
nsHTMLAreaAccessible::GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return;

  nsImageFrame* imageFrame = do_QueryFrame(frame);
  nsImageMap* map = imageFrame->GetImageMap();

  nsresult rv = map->GetBoundsForAreaContent(mContent, aBounds);
  if (NS_FAILED(rv))
    return;

  // XXX Areas are screwy; they return their rects as a pair of points, one pair
  // stored into the width and height.
  aBounds.width -= aBounds.x;
  aBounds.height -= aBounds.y;

  *aBoundingFrame = frame;
}
