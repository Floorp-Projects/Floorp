/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLImageMapAccessible.h"

#include "ARIAMap.h"
#include "nsAccUtils.h"
#include "DocAccessible-inl.h"
#include "Role.h"

#include "nsIFrame.h"
#include "nsImageFrame.h"
#include "nsImageMap.h"
#include "nsIURI.h"
#include "nsLayoutUtils.h"
#include "mozilla/dom/HTMLAreaElement.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HTMLImageMapAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLImageMapAccessible::HTMLImageMapAccessible(nsIContent* aContent,
                                               DocAccessible* aDoc)
    : ImageAccessibleWrap(aContent, aDoc) {
  mType = eImageMapType;

  UpdateChildAreas(false);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLImageMapAccessible: LocalAccessible public

role HTMLImageMapAccessible::NativeRole() const { return roles::IMAGE_MAP; }

////////////////////////////////////////////////////////////////////////////////
// HTMLImageMapAccessible: HyperLinkAccessible

uint32_t HTMLImageMapAccessible::AnchorCount() { return ChildCount(); }

LocalAccessible* HTMLImageMapAccessible::AnchorAt(uint32_t aAnchorIndex) {
  return LocalChildAt(aAnchorIndex);
}

already_AddRefed<nsIURI> HTMLImageMapAccessible::AnchorURIAt(
    uint32_t aAnchorIndex) const {
  LocalAccessible* area = LocalChildAt(aAnchorIndex);
  if (!area) return nullptr;

  nsIContent* linkContent = area->GetContent();
  return linkContent ? linkContent->GetHrefURI() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLImageMapAccessible: public

void HTMLImageMapAccessible::UpdateChildAreas(bool aDoFireEvents) {
  nsImageFrame* imageFrame = do_QueryFrame(mContent->GetPrimaryFrame());

  // If image map is not initialized yet then we trigger one time more later.
  nsImageMap* imageMapObj = imageFrame->GetExistingImageMap();
  if (!imageMapObj) return;

  TreeMutation mt(this, TreeMutation::kNoEvents & !aDoFireEvents);

  // Remove areas that are not a valid part of the image map anymore.
  for (int32_t childIdx = mChildren.Length() - 1; childIdx >= 0; childIdx--) {
    LocalAccessible* area = mChildren.ElementAt(childIdx);
    if (area->GetContent()->GetPrimaryFrame()) continue;

    mt.BeforeRemoval(area);
    RemoveChild(area);
  }

  // Insert new areas into the tree.
  uint32_t areaElmCount = imageMapObj->AreaCount();
  for (uint32_t idx = 0; idx < areaElmCount; idx++) {
    nsIContent* areaContent = imageMapObj->GetAreaAt(idx);
    LocalAccessible* area = mChildren.SafeElementAt(idx);
    if (!area || area->GetContent() != areaContent) {
      RefPtr<LocalAccessible> area = new HTMLAreaAccessible(areaContent, mDoc);
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

LocalAccessible* HTMLImageMapAccessible::GetChildAccessibleFor(
    const nsINode* aNode) const {
  uint32_t length = mChildren.Length();
  for (uint32_t i = 0; i < length; i++) {
    LocalAccessible* area = mChildren[i];
    if (area->GetContent() == aNode) return area;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLAreaAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLAreaAccessible::HTMLAreaAccessible(nsIContent* aContent,
                                       DocAccessible* aDoc)
    : HTMLLinkAccessible(aContent, aDoc) {
  // Make HTML area DOM element not accessible. HTML image map accessible
  // manages its tree itself.
  mStateFlags |= eNotNodeMapEntry;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLAreaAccessible: LocalAccessible

ENameValueFlag HTMLAreaAccessible::NativeName(nsString& aName) const {
  ENameValueFlag nameFlag = LocalAccessible::NativeName(aName);
  if (!aName.IsEmpty()) return nameFlag;

  if (!mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::alt,
                                      aName)) {
    Value(aName);
  }

  return eNameOK;
}

void HTMLAreaAccessible::Description(nsString& aDescription) const {
  aDescription.Truncate();

  // Still to do - follow IE's standard here
  RefPtr<dom::HTMLAreaElement> area =
      dom::HTMLAreaElement::FromNodeOrNull(mContent);
  if (area) area->GetShape(aDescription);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLAreaAccessible: LocalAccessible public

LocalAccessible* HTMLAreaAccessible::LocalChildAtPoint(
    int32_t aX, int32_t aY, EWhichChildAtPoint aWhichChild) {
  // Don't walk into area accessibles.
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLImageMapAccessible: HyperLinkAccessible

uint32_t HTMLAreaAccessible::StartOffset() {
  // Image map accessible is not hypertext accessible therefore
  // StartOffset/EndOffset implementations of LocalAccessible doesn't work here.
  // We return index in parent because image map contains area links only which
  // are embedded objects.
  // XXX: image map should be a hypertext accessible.
  return IndexInParent();
}

uint32_t HTMLAreaAccessible::EndOffset() { return IndexInParent() + 1; }

nsRect HTMLAreaAccessible::RelativeBounds(nsIFrame** aBoundingFrame) const {
  nsIFrame* frame = GetFrame();
  if (!frame) return nsRect();

  nsImageFrame* imageFrame = do_QueryFrame(frame);
  nsImageMap* map = imageFrame->GetImageMap();

  nsRect bounds;
  nsresult rv = map->GetBoundsForAreaContent(mContent, bounds);

  if (NS_FAILED(rv)) return nsRect();

  // XXX Areas are screwy; they return their rects as a pair of points, one pair
  // stored into the width and height.
  *aBoundingFrame = frame;
  bounds.SizeTo(bounds.Width() - bounds.X(), bounds.Height() - bounds.Y());
  return bounds;
}

nsRect HTMLAreaAccessible::ParentRelativeBounds() {
  nsIFrame* boundingFrame = nullptr;
  nsRect relativeBoundsRect = RelativeBounds(&boundingFrame);

  nsIFrame* parentBoundingFrame = nullptr;
  if (mParent) {
    parentBoundingFrame = mParent->GetFrame();
  }

  if (!parentBoundingFrame) {
    // if we can't get the bounding frame, use the pres shell root for the
    // bounding frame RelativeBounds returned
    parentBoundingFrame =
        nsLayoutUtils::GetContainingBlockForClientRect(boundingFrame);
  }

  nsLayoutUtils::TransformRect(boundingFrame, parentBoundingFrame,
                               relativeBoundsRect);

  return relativeBoundsRect;
}
