/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLImageMapAccessible.h"

#include "ARIAMap.h"
#include "nsAccUtils.h"
#include "DocAccessible-inl.h"
#include "Role.h"

#include "nsIServiceManager.h"
#include "nsIDOMElement.h"
#include "nsIFrame.h"
#include "nsImageFrame.h"
#include "nsImageMap.h"
#include "nsIURI.h"
#include "mozilla/dom/HTMLAreaElement.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HTMLImageMapAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLImageMapAccessible::
  HTMLImageMapAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  ImageAccessibleWrap(aContent, aDoc)
{
  mType = eImageMapType;

  UpdateChildAreas(false);
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

  TreeMutation mt(this, TreeMutation::kNoEvents & !aDoFireEvents);

  // Remove areas that are not a valid part of the image map anymore.
  for (int32_t childIdx = mChildren.Length() - 1; childIdx >= 0; childIdx--) {
    Accessible* area = mChildren.ElementAt(childIdx);
    if (area->GetContent()->GetPrimaryFrame())
      continue;

    mt.BeforeRemoval(area);
    RemoveChild(area);
  }

  // Insert new areas into the tree.
  uint32_t areaElmCount = imageMapObj->AreaCount();
  for (uint32_t idx = 0; idx < areaElmCount; idx++) {
    nsIContent* areaContent = imageMapObj->GetAreaAt(idx);
    Accessible* area = mChildren.SafeElementAt(idx);
    if (!area || area->GetContent() != areaContent) {
      RefPtr<Accessible> area = new HTMLAreaAccessible(areaContent, mDoc);
      mDoc->BindToDocument(area, aria::GetRoleMap(areaContent->AsElement()));

      if (!InsertChildAt(idx, area)) {
        mDoc->UnbindFromDocument(area);
        break;
      }

      mt.AfterInsertion(area);
    }
  }

  mt.Done();
}

Accessible*
HTMLImageMapAccessible::GetChildAccessibleFor(const nsINode* aNode) const
{
  uint32_t length = mChildren.Length();
  for (uint32_t i = 0; i < length; i++) {
    Accessible* area = mChildren[i];
    if (area->GetContent() == aNode)
      return area;
  }

  return nullptr;
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
// HTMLAreaAccessible: Accessible

ENameValueFlag
HTMLAreaAccessible::NativeName(nsString& aName)
{
  ENameValueFlag nameFlag = Accessible::NativeName(aName);
  if (!aName.IsEmpty())
    return nameFlag;

  if (!mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::alt, aName))
    Value(aName);

  return eNameOK;
}

void
HTMLAreaAccessible::Description(nsString& aDescription)
{
  aDescription.Truncate();

  // Still to do - follow IE's standard here
  RefPtr<dom::HTMLAreaElement> area =
    dom::HTMLAreaElement::FromContentOrNull(mContent);
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

nsRect
HTMLAreaAccessible::RelativeBounds(nsIFrame** aBoundingFrame) const
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return nsRect();

  nsImageFrame* imageFrame = do_QueryFrame(frame);
  nsImageMap* map = imageFrame->GetImageMap();

  nsRect bounds;
  nsresult rv = map->GetBoundsForAreaContent(mContent, bounds);
  if (NS_FAILED(rv))
    return nsRect();

  // XXX Areas are screwy; they return their rects as a pair of points, one pair
  // stored into the width and height.
  *aBoundingFrame = frame;
  bounds.width -= bounds.x;
  bounds.height -= bounds.y;
  return bounds;
}
