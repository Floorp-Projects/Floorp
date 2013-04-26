/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLImageMapAccessible.h"

#include "ARIAMap.h"
#include "nsAccUtils.h"
#include "DocAccessible-inl.h"
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
// HTMLImageMapAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLImageMapAccessible::
  HTMLImageMapAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  ImageAccessibleWrap(aContent, aDoc)
{
  mType = eImageMapType;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLImageMapAccessible: nsISupports

NS_IMPL_ISUPPORTS_INHERITED0(HTMLImageMapAccessible, ImageAccessible)

////////////////////////////////////////////////////////////////////////////////
// HTMLImageMapAccessible: Accessible public

role
HTMLImageMapAccessible::NativeRole()
{
  return roles::IMAGE_MAP;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLImageMapAccessible: HyperLinkAccessible

uint32_t
HTMLImageMapAccessible::AnchorCount()
{
  return ChildCount();
}

Accessible*
HTMLImageMapAccessible::AnchorAt(uint32_t aAnchorIndex)
{
  return GetChildAt(aAnchorIndex);
}

already_AddRefed<nsIURI>
HTMLImageMapAccessible::AnchorURIAt(uint32_t aAnchorIndex)
{
  Accessible* area = GetChildAt(aAnchorIndex);
  if (!area)
    return nullptr;

  nsIContent* linkContent = area->GetContent();
  return linkContent ? linkContent->GetHrefURI() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLImageMapAccessible: public

void
HTMLImageMapAccessible::UpdateChildAreas(bool aDoFireEvents)
{
  nsImageFrame* imageFrame = do_QueryFrame(mContent->GetPrimaryFrame());

  // If image map is not initialized yet then we trigger one time more later.
  nsImageMap* imageMapObj = imageFrame->GetExistingImageMap();
  if (!imageMapObj)
    return;

  bool doReorderEvent = false;
  nsRefPtr<AccReorderEvent> reorderEvent = new AccReorderEvent(this);

  // Remove areas that are not a valid part of the image map anymore.
  for (int32_t childIdx = mChildren.Length() - 1; childIdx >= 0; childIdx--) {
    Accessible* area = mChildren.ElementAt(childIdx);
    if (area->GetContent()->GetPrimaryFrame())
      continue;

    if (aDoFireEvents) {
      nsRefPtr<AccHideEvent> event = new AccHideEvent(area, area->GetContent());
      mDoc->FireDelayedEvent(event);
      reorderEvent->AddSubMutationEvent(event);
      doReorderEvent = true;
    }

    RemoveChild(area);
  }

  // Insert new areas into the tree.
  uint32_t areaElmCount = imageMapObj->AreaCount();
  for (uint32_t idx = 0; idx < areaElmCount; idx++) {
    nsIContent* areaContent = imageMapObj->GetAreaAt(idx);

    Accessible* area = mChildren.SafeElementAt(idx);
    if (!area || area->GetContent() != areaContent) {
      nsRefPtr<Accessible> area = new HTMLAreaAccessible(areaContent, mDoc);
      if (!mDoc->BindToDocument(area, aria::GetRoleMap(areaContent)))
        break;

      if (!InsertChildAt(idx, area)) {
        mDoc->UnbindFromDocument(area);
        break;
      }

      if (aDoFireEvents) {
        nsRefPtr<AccShowEvent> event = new AccShowEvent(area, areaContent);
        mDoc->FireDelayedEvent(event);
        reorderEvent->AddSubMutationEvent(event);
        doReorderEvent = true;
      }
    }
  }

  // Fire reorder event if needed.
  if (doReorderEvent)
    mDoc->FireDelayedEvent(reorderEvent);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLImageMapAccessible: Accessible protected

void
HTMLImageMapAccessible::CacheChildren()
{
  UpdateChildAreas(false);
}


////////////////////////////////////////////////////////////////////////////////
// HTMLAreaAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLAreaAccessible::
  HTMLAreaAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HTMLLinkAccessible(aContent, aDoc)
{
  // Make HTML area DOM element not accessible. HTML image map accessible
  // manages its tree itself.
  mStateFlags |= eNotNodeMapEntry;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLAreaAccessible: nsIAccessible

ENameValueFlag
HTMLAreaAccessible::NativeName(nsString& aName)
{
  ENameValueFlag nameFlag = Accessible::NativeName(aName);
  if (!aName.IsEmpty())
    return nameFlag;

  if (!mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::alt, aName))
    GetValue(aName);

  return eNameOK;
}

void
HTMLAreaAccessible::Description(nsString& aDescription)
{
  aDescription.Truncate();

  // Still to do - follow IE's standard here
  nsCOMPtr<nsIDOMHTMLAreaElement> area(do_QueryInterface(mContent));
  if (area)
    area->GetShape(aDescription);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLAreaAccessible: Accessible public

Accessible*
HTMLAreaAccessible::ChildAtPoint(int32_t aX, int32_t aY,
                                 EWhichChildAtPoint aWhichChild)
{
  // Don't walk into area accessibles.
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLImageMapAccessible: HyperLinkAccessible

uint32_t
HTMLAreaAccessible::StartOffset()
{
  // Image map accessible is not hypertext accessible therefore
  // StartOffset/EndOffset implementations of Accessible doesn't work here.
  // We return index in parent because image map contains area links only which
  // are embedded objects.
  // XXX: image map should be a hypertext accessible.
  return IndexInParent();
}

uint32_t
HTMLAreaAccessible::EndOffset()
{
  return IndexInParent() + 1;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLAreaAccessible: Accessible protected

void
HTMLAreaAccessible::CacheChildren()
{
  // No children for aria accessible.
}

void
HTMLAreaAccessible::GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame)
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
