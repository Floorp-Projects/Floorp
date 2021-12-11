/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleWrap.h"
#include "DocAccessible-inl.h"

#import "mozAccessible.h"
#import "MOXTextMarkerDelegate.h"

using namespace mozilla;
using namespace mozilla::a11y;

DocAccessibleWrap::DocAccessibleWrap(dom::Document* aDocument,
                                     PresShell* aPresShell)
    : DocAccessible(aDocument, aPresShell) {}

void DocAccessibleWrap::Shutdown() {
  [MOXTextMarkerDelegate destroyForDoc:this];
  DocAccessible::Shutdown();
}

DocAccessibleWrap::~DocAccessibleWrap() {}

void DocAccessibleWrap::AttributeChanged(dom::Element* aElement,
                                         int32_t aNameSpaceID,
                                         nsAtom* aAttribute, int32_t aModType,
                                         const nsAttrValue* aOldValue) {
  DocAccessible::AttributeChanged(aElement, aNameSpaceID, aAttribute, aModType,
                                  aOldValue);
  if (aAttribute == nsGkAtoms::aria_live) {
    LocalAccessible* accessible =
        mContent != aElement ? GetAccessible(aElement) : this;
    if (!accessible) {
      return;
    }

    static const dom::Element::AttrValuesArray sLiveRegionValues[] = {
        nsGkAtoms::OFF, nsGkAtoms::polite, nsGkAtoms::assertive, nullptr};
    int32_t attrValue =
        aElement->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::aria_live,
                                  sLiveRegionValues, eIgnoreCase);
    if (attrValue > 0) {
      if (!aOldValue || aOldValue->IsEmptyString() ||
          aOldValue->Equals(nsGkAtoms::OFF, eIgnoreCase)) {
        // This element just got an active aria-live attribute value
        FireDelayedEvent(nsIAccessibleEvent::EVENT_LIVE_REGION_ADDED,
                         accessible);
      }
    } else {
      if (aOldValue && (aOldValue->Equals(nsGkAtoms::polite, eIgnoreCase) ||
                        aOldValue->Equals(nsGkAtoms::assertive, eIgnoreCase))) {
        // This element lost an active live region
        FireDelayedEvent(nsIAccessibleEvent::EVENT_LIVE_REGION_REMOVED,
                         accessible);
      } else if (attrValue == 0) {
        // aria-live="off", check if its a role-based live region that
        // needs to be removed.
        if (const nsRoleMapEntry* roleMap = accessible->ARIARoleMap()) {
          // aria role defines it as a live region. It's live!
          if (roleMap->liveAttRule == ePoliteLiveAttr ||
              roleMap->liveAttRule == eAssertiveLiveAttr) {
            FireDelayedEvent(nsIAccessibleEvent::EVENT_LIVE_REGION_REMOVED,
                             accessible);
          }
        } else if (nsStaticAtom* value = GetAccService()->MarkupAttribute(
                       aElement, nsGkAtoms::aria_live)) {
          // HTML element defines it as a live region. It's live!
          if (value == nsGkAtoms::polite || value == nsGkAtoms::assertive) {
            FireDelayedEvent(nsIAccessibleEvent::EVENT_LIVE_REGION_REMOVED,
                             accessible);
          }
        }
      }
    }
  }
}

void DocAccessibleWrap::QueueNewLiveRegion(LocalAccessible* aAccessible) {
  if (!aAccessible) {
    return;
  }

  mNewLiveRegions.Insert(aAccessible->UniqueID());
}

void DocAccessibleWrap::ProcessNewLiveRegions() {
  for (const auto& uniqueID : mNewLiveRegions) {
    if (LocalAccessible* liveRegion =
            GetAccessibleByUniqueID(const_cast<void*>(uniqueID))) {
      FireDelayedEvent(nsIAccessibleEvent::EVENT_LIVE_REGION_ADDED, liveRegion);
    }
  }

  mNewLiveRegions.Clear();
}

void DocAccessibleWrap::DoInitialUpdate() {
  DocAccessible::DoInitialUpdate();
  ProcessNewLiveRegions();
}
