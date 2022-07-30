/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleWrap.h"
#include "nsObjCExceptions.h"
#include "nsCocoaUtils.h"
#include "nsUnicharUtils.h"

#include "LocalAccessible-inl.h"
#include "nsAccUtils.h"
#include "Role.h"
#include "TextRange.h"
#include "gfxPlatform.h"

#import "MOXLandmarkAccessibles.h"
#import "MOXMathAccessibles.h"
#import "MOXTextMarkerDelegate.h"
#import "MOXWebAreaAccessible.h"
#import "mozAccessible.h"
#import "mozActionElements.h"
#import "mozHTMLAccessible.h"
#import "mozSelectableElements.h"
#import "mozTableAccessible.h"
#import "mozTextAccessible.h"

using namespace mozilla;
using namespace mozilla::a11y;

AccessibleWrap::AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc)
    : LocalAccessible(aContent, aDoc),
      mNativeObject(nil),
      mNativeInited(false) {
  if (aContent && aContent->IsElement() && aDoc) {
    // Check if this accessible is a live region and queue it
    // it for dispatching an event after it has been inserted.
    DocAccessibleWrap* doc = static_cast<DocAccessibleWrap*>(aDoc);
    static const dom::Element::AttrValuesArray sLiveRegionValues[] = {
        nsGkAtoms::OFF, nsGkAtoms::polite, nsGkAtoms::assertive, nullptr};
    int32_t attrValue = aContent->AsElement()->FindAttrValueIn(
        kNameSpaceID_None, nsGkAtoms::aria_live, sLiveRegionValues,
        eIgnoreCase);
    if (attrValue == 0) {
      // aria-live is "off", do nothing.
    } else if (attrValue > 0) {
      // aria-live attribute is polite or assertive. It's live!
      doc->QueueNewLiveRegion(this);
    } else if (const nsRoleMapEntry* roleMap =
                   aria::GetRoleMap(aContent->AsElement())) {
      // aria role defines it as a live region. It's live!
      if (roleMap->liveAttRule == ePoliteLiveAttr ||
          roleMap->liveAttRule == eAssertiveLiveAttr) {
        doc->QueueNewLiveRegion(this);
      }
    } else if (nsStaticAtom* value = GetAccService()->MarkupAttribute(
                   aContent, nsGkAtoms::aria_live)) {
      // HTML element defines it as a live region. It's live!
      if (value == nsGkAtoms::polite || value == nsGkAtoms::assertive) {
        doc->QueueNewLiveRegion(this);
      }
    }
  }
}

AccessibleWrap::~AccessibleWrap() {}

mozAccessible* AccessibleWrap::GetNativeObject() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (!mNativeInited && !mNativeObject) {
    // We don't creat OSX accessibles for xul tooltips, defunct accessibles,
    // <br> (whitespace) elements, or pruned children.
    //
    // To maintain a scripting environment where the XPCOM accessible hierarchy
    // look the same on all platforms, we still let the C++ objects be created
    // though.
    if (!IsXULTooltip() && !IsDefunct() && Role() != roles::WHITESPACE) {
      mNativeObject = [[GetNativeType() alloc] initWithAccessible:this];
    }
  }

  mNativeInited = true;

  return mNativeObject;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

void AccessibleWrap::GetNativeInterface(void** aOutInterface) {
  *aOutInterface = static_cast<void*>(GetNativeObject());
}

// overridden in subclasses to create the right kind of object. by default we
// create a generic 'mozAccessible' node.
Class AccessibleWrap::GetNativeType() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (IsXULTabpanels()) {
    return [mozPaneAccessible class];
  }

  if (IsTable()) {
    return [mozTableAccessible class];
  }

  if (IsTableRow()) {
    return [mozTableRowAccessible class];
  }

  if (IsTableCell()) {
    return [mozTableCellAccessible class];
  }

  if (IsDoc()) {
    return [MOXWebAreaAccessible class];
  }

  return GetTypeFromRole(Role());

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

// this method is very important. it is fired when an accessible object "dies".
// after this point the object might still be around (because some 3rd party
// still has a ref to it), but it is in fact 'dead'.
void AccessibleWrap::Shutdown() {
  // this ensure we will not try to re-create the native object.
  mNativeInited = true;

  // we really intend to access the member directly.
  if (mNativeObject) {
    [mNativeObject expire];
    [mNativeObject release];
    mNativeObject = nil;
  }

  LocalAccessible::Shutdown();
}

nsresult AccessibleWrap::HandleAccEvent(AccEvent* aEvent) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  nsresult rv = LocalAccessible::HandleAccEvent(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsDefunct()) {
    // The accessible can become defunct after their events are handled.
    return NS_OK;
  }

  uint32_t eventType = aEvent->GetEventType();

  if (eventType == nsIAccessibleEvent::EVENT_SHOW) {
    DocAccessibleWrap* doc = static_cast<DocAccessibleWrap*>(Document());
    doc->ProcessNewLiveRegions();
  }

  if (IPCAccessibilityActive()) {
    return NS_OK;
  }

  LocalAccessible* eventTarget = nullptr;

  switch (eventType) {
    case nsIAccessibleEvent::EVENT_SELECTION:
    case nsIAccessibleEvent::EVENT_SELECTION_ADD:
    case nsIAccessibleEvent::EVENT_SELECTION_REMOVE: {
      AccSelChangeEvent* selEvent = downcast_accEvent(aEvent);
      // The "widget" is the selected widget's container. In OSX
      // it is the target of the selection changed event.
      eventTarget = selEvent->Widget();
      break;
    }
    case nsIAccessibleEvent::EVENT_TEXT_INSERTED:
    case nsIAccessibleEvent::EVENT_TEXT_REMOVED: {
      LocalAccessible* acc = aEvent->GetAccessible();
      // If there is a text input ancestor, use it as the event source.
      while (acc && GetTypeFromRole(acc->Role()) != [mozTextAccessible class]) {
        acc = acc->LocalParent();
      }
      eventTarget = acc ? acc : aEvent->GetAccessible();
      break;
    }
    default:
      eventTarget = aEvent->GetAccessible();
      break;
  }

  mozAccessible* nativeAcc = nil;
  eventTarget->GetNativeInterface((void**)&nativeAcc);
  if (!nativeAcc) {
    return NS_ERROR_FAILURE;
  }

  switch (eventType) {
    case nsIAccessibleEvent::EVENT_STATE_CHANGE: {
      AccStateChangeEvent* event = downcast_accEvent(aEvent);
      [nativeAcc stateChanged:event->GetState()
                    isEnabled:event->IsStateEnabled()];
      break;
    }

    case nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED: {
      MOXTextMarkerDelegate* delegate =
          [MOXTextMarkerDelegate getOrCreateForDoc:aEvent->Document()];
      AccTextSelChangeEvent* event = downcast_accEvent(aEvent);
      AutoTArray<TextRange, 1> ranges;
      event->SelectionRanges(&ranges);

      if (ranges.Length()) {
        // Cache selection in delegate.
        [delegate setSelectionFrom:ranges[0].StartContainer()
                                at:ranges[0].StartOffset()
                                to:ranges[0].EndContainer()
                                at:ranges[0].EndOffset()];
      }

      [nativeAcc handleAccessibleEvent:eventType];
      break;
    }

    case nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED: {
      AccCaretMoveEvent* event = downcast_accEvent(aEvent);
      int32_t caretOffset = event->GetCaretOffset();
      MOXTextMarkerDelegate* delegate =
          [MOXTextMarkerDelegate getOrCreateForDoc:aEvent->Document()];
      [delegate setCaretOffset:eventTarget
                            at:caretOffset
               moveGranularity:event->GetGranularity()];
      if (event->IsSelectionCollapsed()) {
        // If the selection is collapsed, invalidate our text selection cache.
        [delegate setSelectionFrom:eventTarget
                                at:caretOffset
                                to:eventTarget
                                at:caretOffset];
      }

      if (mozTextAccessible* textAcc = static_cast<mozTextAccessible*>(
              [nativeAcc moxEditableAncestor])) {
        [textAcc
            handleAccessibleEvent:nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED];
      } else {
        [nativeAcc
            handleAccessibleEvent:nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED];
      }
      break;
    }

    case nsIAccessibleEvent::EVENT_TEXT_INSERTED:
    case nsIAccessibleEvent::EVENT_TEXT_REMOVED: {
      AccTextChangeEvent* tcEvent = downcast_accEvent(aEvent);
      [nativeAcc handleAccessibleTextChangeEvent:nsCocoaUtils::ToNSString(
                                                     tcEvent->ModifiedText())
                                        inserted:tcEvent->IsTextInserted()
                                     inContainer:aEvent->GetAccessible()
                                              at:tcEvent->GetStartOffset()];
      break;
    }

    case nsIAccessibleEvent::EVENT_ALERT:
    case nsIAccessibleEvent::EVENT_FOCUS:
    case nsIAccessibleEvent::EVENT_TEXT_VALUE_CHANGE:
    case nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE:
    case nsIAccessibleEvent::EVENT_MENUPOPUP_START:
    case nsIAccessibleEvent::EVENT_MENUPOPUP_END:
    case nsIAccessibleEvent::EVENT_REORDER:
    case nsIAccessibleEvent::EVENT_SELECTION:
    case nsIAccessibleEvent::EVENT_SELECTION_ADD:
    case nsIAccessibleEvent::EVENT_SELECTION_REMOVE:
    case nsIAccessibleEvent::EVENT_LIVE_REGION_ADDED:
    case nsIAccessibleEvent::EVENT_LIVE_REGION_REMOVED:
    case nsIAccessibleEvent::EVENT_NAME_CHANGE:
    case nsIAccessibleEvent::EVENT_OBJECT_ATTRIBUTE_CHANGED:
    case nsIAccessibleEvent::EVENT_TABLE_STYLING_CHANGED:
      [nativeAcc handleAccessibleEvent:eventType];
      break;

    default:
      break;
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

bool AccessibleWrap::ApplyPostFilter(const EWhichPostFilter& aSearchKey,
                                     const nsString& aSearchText) {
  // We currently only support the eContainsText post filter.
  MOZ_ASSERT(aSearchKey == EWhichPostFilter::eContainsText,
             "Only search text supported");
  nsAutoString name;
  Name(name);
  return CaseInsensitiveFindInReadable(aSearchText, name);
}

////////////////////////////////////////////////////////////////////////////////
// AccessibleWrap protected

Class a11y::GetTypeFromRole(roles::Role aRole) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  switch (aRole) {
    case roles::COMBOBOX:
      return [mozPopupButtonAccessible class];

    case roles::PUSHBUTTON:
      return [mozButtonAccessible class];

    case roles::PAGETAB:
      return [mozTabAccessible class];

    case roles::CHECKBUTTON:
    case roles::TOGGLE_BUTTON:
    case roles::SWITCH:
      return [mozCheckboxAccessible class];

    case roles::RADIOBUTTON:
      return [mozRadioButtonAccessible class];

    case roles::SPINBUTTON:
    case roles::SLIDER:
      return [mozIncrementableAccessible class];

    case roles::HEADING:
      return [mozHeadingAccessible class];

    case roles::PAGETABLIST:
      return [mozTabGroupAccessible class];

    case roles::ENTRY:
    case roles::CAPTION:
    case roles::ACCEL_LABEL:
    case roles::EDITCOMBOBOX:
    case roles::PASSWORD_TEXT:
      // normal textfield (static or editable)
      return [mozTextAccessible class];

    case roles::TEXT_LEAF:
    case roles::STATICTEXT:
      return [mozTextLeafAccessible class];

    case roles::LANDMARK:
      return [MOXLandmarkAccessible class];

    case roles::LINK:
      return [mozLinkAccessible class];

    case roles::LISTBOX:
      return [mozListboxAccessible class];

    case roles::LISTITEM:
      return [MOXListItemAccessible class];

    case roles::OPTION: {
      return [mozOptionAccessible class];
    }

    case roles::RICH_OPTION: {
      return [mozSelectableChildAccessible class];
    }

    case roles::COMBOBOX_LIST:
    case roles::MENUBAR:
    case roles::MENUPOPUP: {
      return [mozMenuAccessible class];
    }

    case roles::COMBOBOX_OPTION:
    case roles::PARENT_MENUITEM:
    case roles::MENUITEM: {
      return [mozMenuItemAccessible class];
    }

    case roles::MATHML_ROOT:
      return [MOXMathRootAccessible class];

    case roles::MATHML_SQUARE_ROOT:
      return [MOXMathSquareRootAccessible class];

    case roles::MATHML_FRACTION:
      return [MOXMathFractionAccessible class];

    case roles::MATHML_SUB:
    case roles::MATHML_SUP:
    case roles::MATHML_SUB_SUP:
      return [MOXMathSubSupAccessible class];

    case roles::MATHML_UNDER:
    case roles::MATHML_OVER:
    case roles::MATHML_UNDER_OVER:
      return [MOXMathUnderOverAccessible class];

    case roles::OUTLINE:
    case roles::TREE_TABLE:
      return [mozOutlineAccessible class];

    case roles::OUTLINEITEM:
      return [mozOutlineRowAccessible class];

    default:
      return [mozAccessible class];
  }

  return nil;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}
