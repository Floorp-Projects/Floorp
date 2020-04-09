/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessible.h"
#include "nsObjCExceptions.h"

#include "Accessible-inl.h"
#include "nsAccUtils.h"
#include "Role.h"
#include "gfxPlatform.h"

#import "mozAccessible.h"
#import "mozActionElements.h"
#import "mozHTMLAccessible.h"
#import "mozSelectableElements.h"
#import "mozTableAccessible.h"
#import "mozTextAccessible.h"

using namespace mozilla;
using namespace mozilla::a11y;

AccessibleWrap::AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc)
    : Accessible(aContent, aDoc), mNativeObject(nil), mNativeInited(false) {}

AccessibleWrap::~AccessibleWrap() {}

mozAccessible* AccessibleWrap::GetNativeObject() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (!mNativeInited && !mNativeObject) {
    // We don't creat OSX accessibles for xul tooltips, defunct accessibles,
    // or pruned children.
    if (!IsXULTooltip() && !IsDefunct() && !AncestorIsFlat()) {
      uintptr_t accWrap = reinterpret_cast<uintptr_t>(this);
      mNativeObject = [[GetNativeType() alloc] initWithAccessible:accWrap];
    }
  }

  mNativeInited = true;

  return mNativeObject;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

void AccessibleWrap::GetNativeInterface(void** aOutInterface) {
  *aOutInterface = static_cast<void*>(GetNativeObject());
}

// overridden in subclasses to create the right kind of object. by default we create a generic
// 'mozAccessible' node.
Class AccessibleWrap::GetNativeType() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (IsXULTabpanels()) return [mozPaneAccessible class];

  if (IsTable()) return [mozTableAccessible class];

  if (IsTableRow()) return [mozTableRowAccessible class];

  if (IsTableCell()) return [mozTableCellAccessible class];

  return GetTypeFromRole(Role());

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// this method is very important. it is fired when an accessible object "dies". after this point
// the object might still be around (because some 3rd party still has a ref to it), but it is
// in fact 'dead'.
void AccessibleWrap::Shutdown() {
  // this ensure we will not try to re-create the native object.
  mNativeInited = true;

  // we really intend to access the member directly.
  if (mNativeObject) {
    [mNativeObject expire];
    [mNativeObject release];
    mNativeObject = nil;
  }

  Accessible::Shutdown();
}

nsresult AccessibleWrap::HandleAccEvent(AccEvent* aEvent) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult rv = Accessible::HandleAccEvent(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IPCAccessibilityActive()) {
    return NS_OK;
  }

  uint32_t eventType = aEvent->GetEventType();

  mozAccessible* nativeAcc = nil;

  switch (eventType) {
    case nsIAccessibleEvent::EVENT_FOCUS:
    case nsIAccessibleEvent::EVENT_VALUE_CHANGE:
    case nsIAccessibleEvent::EVENT_TEXT_VALUE_CHANGE:
    case nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED:
    case nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED:
    case nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE:
    case nsIAccessibleEvent::EVENT_MENUPOPUP_START:
    case nsIAccessibleEvent::EVENT_MENUPOPUP_END:
      if (Accessible* accessible = aEvent->GetAccessible()) {
        accessible->GetNativeInterface((void**)&nativeAcc);
        if (!nativeAcc) {
          return NS_ERROR_FAILURE;
        }
      }
      break;
    case nsIAccessibleEvent::EVENT_SELECTION:
    case nsIAccessibleEvent::EVENT_SELECTION_ADD:
    case nsIAccessibleEvent::EVENT_SELECTION_REMOVE: {
      AccSelChangeEvent* selEvent = downcast_accEvent(aEvent);
      // The "widget" is the selected widget's container. In OSX
      // it is the target of the selection changed event.
      if (Accessible* accessible = selEvent->Widget()) {
        accessible->GetNativeInterface((void**)&nativeAcc);
        if (!nativeAcc) {
          return NS_ERROR_FAILURE;
        }
      }
      break;
    }
    case nsIAccessibleEvent::EVENT_STATE_CHANGE:
      if (Accessible* accessible = aEvent->GetAccessible()) {
        accessible->GetNativeInterface((void**)&nativeAcc);
        if (nativeAcc) {
          AccStateChangeEvent* event = downcast_accEvent(aEvent);
          [nativeAcc stateChanged:event->GetState() isEnabled:event->IsStateEnabled()];
          return NS_OK;
        } else {
          return NS_ERROR_FAILURE;
        }
      }
      break;
    default:
      break;
  }

  if (nativeAcc) {
    [nativeAcc handleAccessibleEvent:eventType];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

bool AccessibleWrap::InsertChildAt(uint32_t aIdx, Accessible* aAccessible) {
  bool inserted = Accessible::InsertChildAt(aIdx, aAccessible);
  if (inserted && mNativeObject) [mNativeObject appendChild:aAccessible];

  return inserted;
}

bool AccessibleWrap::RemoveChild(Accessible* aAccessible) {
  bool removed = Accessible::RemoveChild(aAccessible);

  if (removed && mNativeObject) [mNativeObject invalidateChildren];

  return removed;
}

////////////////////////////////////////////////////////////////////////////////
// AccessibleWrap protected

bool AccessibleWrap::AncestorIsFlat() {
  // We don't create a native object if we're child of a "flat" accessible;
  // for example, on OS X buttons shouldn't have any children, because that
  // makes the OS confused.
  //
  // To maintain a scripting environment where the XPCOM accessible hierarchy
  // look the same on all platforms, we still let the C++ objects be created
  // though.

  Accessible* parent = Parent();
  while (parent) {
    if (nsAccUtils::MustPrune(parent)) return true;

    parent = parent->Parent();
  }
  // no parent was flat
  return false;
}

Class a11y::GetTypeFromRole(roles::Role aRole) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  switch (aRole) {
    case roles::COMBOBOX:
    case roles::PUSHBUTTON:
    case roles::SPLITBUTTON: {
      return [mozButtonAccessible class];
    }

    case roles::PAGETAB:
      return [mozTabAccessible class];

    case roles::CHECKBUTTON:
    case roles::TOGGLE_BUTTON:
    case roles::RADIOBUTTON:
      return [mozCheckboxAccessible class];

    case roles::SLIDER:
      return [mozSliderAccessible class];

    case roles::HEADING:
      return [mozHeadingAccessible class];

    case roles::PAGETABLIST:
      return [mozTabGroupAccessible class];

    case roles::ENTRY:
    case roles::STATICTEXT:
    case roles::CAPTION:
    case roles::ACCEL_LABEL:
    case roles::PASSWORD_TEXT:
      // normal textfield (static or editable)
      return [mozTextAccessible class];

    case roles::TEXT_LEAF:
      return [mozTextLeafAccessible class];

    case roles::LINK:
      return [mozLinkAccessible class];

    default:
      return [mozAccessible class];
  }

  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}
