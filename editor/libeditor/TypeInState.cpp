/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TypeInState.h"

#include <stddef.h>

#include "HTMLEditUtils.h"
#include "mozilla/EditorBase.h"
#include "mozilla/mozalloc.h"
#include "mozilla/dom/AncestorIterator.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/Selection.h"
#include "nsAString.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsINode.h"
#include "nsISupportsBase.h"
#include "nsISupportsImpl.h"
#include "nsReadableUtils.h"
#include "nsStringFwd.h"

// Workaround for windows headers
#ifdef SetProp
#  undef SetProp
#endif

class nsAtom;

namespace mozilla {

using namespace dom;

/********************************************************************
 * mozilla::TypeInState
 *******************************************************************/

NS_IMPL_CYCLE_COLLECTION_CLASS(TypeInState)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(TypeInState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLastSelectionPoint)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(TypeInState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLastSelectionPoint)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(TypeInState, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(TypeInState, Release)

TypeInState::TypeInState()
    : mRelativeFontSize(0),
      mMouseDownFiredInLinkElement(false),
      mMouseUpFiredInLinkElement(false) {
  Reset();
}

TypeInState::~TypeInState() {
  // Call Reset() to release any data that may be in
  // mClearedArray and mSetArray.

  Reset();
}

nsresult TypeInState::UpdateSelState(Selection* aSelection) {
  if (NS_WARN_IF(!aSelection)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!aSelection->IsCollapsed()) {
    return NS_OK;
  }

  mLastSelectionPoint = EditorBase::GetStartPoint(*aSelection);
  if (!mLastSelectionPoint.IsSet()) {
    return NS_ERROR_FAILURE;
  }
  // We need to store only offset because referring child may be removed by
  // we'll check the point later.
  AutoEditorDOMPointChildInvalidator saveOnlyOffset(mLastSelectionPoint);
  return NS_OK;
}

void TypeInState::PreHandleMouseEvent(const MouseEvent& aMouseDownOrUpEvent) {
  MOZ_ASSERT(aMouseDownOrUpEvent.WidgetEventPtr()->mMessage == eMouseDown ||
             aMouseDownOrUpEvent.WidgetEventPtr()->mMessage == eMouseUp);
  bool& eventFiredInLinkElement =
      aMouseDownOrUpEvent.WidgetEventPtr()->mMessage == eMouseDown
          ? mMouseDownFiredInLinkElement
          : mMouseUpFiredInLinkElement;
  eventFiredInLinkElement = false;
  if (aMouseDownOrUpEvent.DefaultPrevented()) {
    return;
  }
  // If mouse button is down or up in a link element, we shouldn't unlink
  // it when we get a notification of selection change.
  EventTarget* target = aMouseDownOrUpEvent.GetExplicitOriginalTarget();
  if (NS_WARN_IF(!target)) {
    return;
  }
  nsCOMPtr<nsIContent> targetContent = do_QueryInterface(target);
  if (NS_WARN_IF(!targetContent)) {
    return;
  }
  eventFiredInLinkElement =
      HTMLEditUtils::IsContentInclusiveDescendantOfLink(*targetContent);
}

void TypeInState::OnSelectionChange(Selection& aSelection, int16_t aReason) {
  // XXX: Selection currently generates bogus selection changed notifications
  // XXX: (bug 140303). It can notify us when the selection hasn't actually
  // XXX: changed, and it notifies us more than once for the same change.
  // XXX:
  // XXX: The following code attempts to work around the bogus notifications,
  // XXX: and should probably be removed once bug 140303 is fixed.
  // XXX:
  // XXX: This code temporarily fixes the problem where clicking the mouse in
  // XXX: the same location clears the type-in-state.

  bool mouseEventFiredInLinkElement = false;
  if (aReason & (nsISelectionListener::MOUSEDOWN_REASON |
                 nsISelectionListener::MOUSEUP_REASON)) {
    MOZ_ASSERT((aReason & (nsISelectionListener::MOUSEDOWN_REASON |
                           nsISelectionListener::MOUSEUP_REASON)) !=
               (nsISelectionListener::MOUSEDOWN_REASON |
                nsISelectionListener::MOUSEUP_REASON));
    bool& eventFiredInLinkElement =
        aReason & nsISelectionListener::MOUSEDOWN_REASON
            ? mMouseDownFiredInLinkElement
            : mMouseUpFiredInLinkElement;
    mouseEventFiredInLinkElement = eventFiredInLinkElement;
    eventFiredInLinkElement = false;
  }

  bool unlink = false;
  bool resetAllStyles = true;
  if (aSelection.IsCollapsed() && aSelection.RangeCount()) {
    EditorRawDOMPoint selectionStartPoint(
        EditorBase::GetStartPoint(aSelection));
    if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
      return;
    }

    RefPtr<Element> linkElement;
    if (HTMLEditUtils::IsPointAtEdgeOfLink(selectionStartPoint,
                                           getter_AddRefs(linkElement))) {
      // If caret comes from outside of <a href> element, we should clear "link"
      // style after reset.
      if (aReason == nsISelectionListener::KEYPRESS_REASON) {
        MOZ_ASSERT(!(aReason & (nsISelectionListener::MOUSEDOWN_REASON |
                                nsISelectionListener::MOUSEUP_REASON)));
        if (mLastSelectionPoint == selectionStartPoint) {
          // We got a bogus selection changed notification!
          return;
        }
        if (mLastSelectionPoint.IsSet() && selectionStartPoint.IsInTextNode() &&
            // If we're moving in same text node, we can assume that we should
            // stay in the <a href>.
            mLastSelectionPoint.GetContainer() !=
                selectionStartPoint.GetContainer()) {
          // XXX Assuming it's not empty text node because it's unrealistic edge
          //     case.
          bool maybeStartOfAnchor = selectionStartPoint.IsStartOfContainer();
          for (EditorRawDOMPoint point(selectionStartPoint.GetContainer());
               point.IsSet() && (maybeStartOfAnchor ? point.IsStartOfContainer()
                                                    : point.IsAtLastContent());
               point.Set(point.GetContainer())) {
            // TODO: We should check editing host boundary here.
            if (HTMLEditUtils::IsLink(point.GetContainer())) {
              // Now, we're at start or end of <a href>.
              unlink =
                  !mLastSelectionPoint.GetContainer()->IsInclusiveDescendantOf(
                      point.GetContainer());
              break;
            }
          }
        }
      } else if (aReason & (nsISelectionListener::MOUSEDOWN_REASON |
                            nsISelectionListener::MOUSEUP_REASON)) {
        if (mLastSelectionPoint == selectionStartPoint) {
          // If all styles are cleared or link style is explicitly set, we
          // shouldn't reset them without caret move.
          if (AreAllStylesCleared() || IsLinkStyleSet()) {
            return;
          }
          // And if non-link styles are cleared or some styles are set, we
          // shouldn't reset them too, but we may need to change the link
          // style.
          if (AreSomeStylesSet() ||
              (AreSomeStylesCleared() && !IsOnlyLinkStyleCleared())) {
            resetAllStyles = false;
          }
        }

        // If the corresponding mouse event is fired in a link element,
        // we should keep treating inputting content as content in the link,
        // but otherwise, i.e., clicked outside the link, we should stop
        // treating inputting content as content in the link.
        unlink = !mouseEventFiredInLinkElement;
      }
    } else if (mLastSelectionPoint == selectionStartPoint) {
      return;
    }

    mLastSelectionPoint = selectionStartPoint;
    // We need to store only offset because referring child may be removed by
    // we'll check the point later.
    AutoEditorDOMPointChildInvalidator saveOnlyOffset(mLastSelectionPoint);
  } else {
    mLastSelectionPoint.Clear();
  }

  if (resetAllStyles) {
    Reset();
    if (unlink) {
      ClearProp(nsGkAtoms::a, nullptr);
    }
    return;
  }

  if (unlink == IsExplicitlyLinkStyleCleared()) {
    return;
  }

  // Even if we shouldn't touch existing style, we need to set/clear only link
  // style in some cases.
  if (unlink) {
    ClearProp(nsGkAtoms::a, nullptr);
  } else if (!unlink) {
    RemovePropFromClearedList(nsGkAtoms::a, nullptr);
  }
}

void TypeInState::Reset() {
  for (size_t i = 0, n = mClearedArray.Length(); i < n; i++) {
    delete mClearedArray[i];
  }
  mClearedArray.Clear();
  for (size_t i = 0, n = mSetArray.Length(); i < n; i++) {
    delete mSetArray[i];
  }
  mSetArray.Clear();
}

void TypeInState::SetProp(nsAtom* aProp, nsAtom* aAttr,
                          const nsAString& aValue) {
  // special case for big/small, these nest
  if (nsGkAtoms::big == aProp) {
    mRelativeFontSize++;
    return;
  }
  if (nsGkAtoms::small == aProp) {
    mRelativeFontSize--;
    return;
  }

  int32_t index;
  if (IsPropSet(aProp, aAttr, nullptr, index)) {
    // if it's already set, update the value
    mSetArray[index]->value = aValue;
    return;
  }

  // Make a new propitem and add it to the list of set properties.
  mSetArray.AppendElement(new PropItem(aProp, aAttr, aValue));

  // remove it from the list of cleared properties, if we have a match
  RemovePropFromClearedList(aProp, aAttr);
}

void TypeInState::ClearAllProps() {
  // null prop means "all" props
  ClearProp(nullptr, nullptr);
}

void TypeInState::ClearProp(nsAtom* aProp, nsAtom* aAttr) {
  // if it's already cleared we are done
  if (IsPropCleared(aProp, aAttr)) {
    return;
  }

  // make a new propitem
  PropItem* item = new PropItem(aProp, aAttr, u""_ns);

  // remove it from the list of set properties, if we have a match
  RemovePropFromSetList(aProp, aAttr);

  // add it to the list of cleared properties
  mClearedArray.AppendElement(item);
}

/**
 * TakeClearProperty() hands back next property item on the clear list.
 * Caller assumes ownership of PropItem and must delete it.
 */
UniquePtr<PropItem> TypeInState::TakeClearProperty() {
  return mClearedArray.Length()
             ? UniquePtr<PropItem>{mClearedArray.PopLastElement()}
             : nullptr;
}

/**
 * TakeSetProperty() hands back next poroperty item on the set list.
 * Caller assumes ownership of PropItem and must delete it.
 */
UniquePtr<PropItem> TypeInState::TakeSetProperty() {
  return mSetArray.Length() ? UniquePtr<PropItem>{mSetArray.PopLastElement()}
                            : nullptr;
}

/**
 * TakeRelativeFontSize() hands back relative font value, which is then
 * cleared out.
 */
int32_t TypeInState::TakeRelativeFontSize() {
  int32_t relSize = mRelativeFontSize;
  mRelativeFontSize = 0;
  return relSize;
}

void TypeInState::GetTypingState(bool& isSet, bool& theSetting, nsAtom* aProp,
                                 nsAtom* aAttr, nsString* aValue) {
  if (IsPropSet(aProp, aAttr, aValue)) {
    isSet = true;
    theSetting = true;
  } else if (IsPropCleared(aProp, aAttr)) {
    isSet = true;
    theSetting = false;
  } else {
    isSet = false;
  }
}

void TypeInState::RemovePropFromSetList(nsAtom* aProp, nsAtom* aAttr) {
  int32_t index;
  if (!aProp) {
    // clear _all_ props
    for (size_t i = 0, n = mSetArray.Length(); i < n; i++) {
      delete mSetArray[i];
    }
    mSetArray.Clear();
    mRelativeFontSize = 0;
  } else if (FindPropInList(aProp, aAttr, nullptr, mSetArray, index)) {
    delete mSetArray[index];
    mSetArray.RemoveElementAt(index);
  }
}

void TypeInState::RemovePropFromClearedList(nsAtom* aProp, nsAtom* aAttr) {
  int32_t index;
  if (FindPropInList(aProp, aAttr, nullptr, mClearedArray, index)) {
    delete mClearedArray[index];
    mClearedArray.RemoveElementAt(index);
  }
}

bool TypeInState::IsPropSet(nsAtom* aProp, nsAtom* aAttr, nsAString* outValue) {
  int32_t i;
  return IsPropSet(aProp, aAttr, outValue, i);
}

bool TypeInState::IsPropSet(nsAtom* aProp, nsAtom* aAttr, nsAString* outValue,
                            int32_t& outIndex) {
  if (aAttr == nsGkAtoms::_empty) {
    aAttr = nullptr;
  }
  // linear search.  list should be short.
  size_t count = mSetArray.Length();
  for (size_t i = 0; i < count; i++) {
    PropItem* item = mSetArray[i];
    if (item->tag == aProp && item->attr == aAttr) {
      if (outValue) {
        *outValue = item->value;
      }
      outIndex = i;
      return true;
    }
  }
  return false;
}

bool TypeInState::IsPropCleared(nsAtom* aProp, nsAtom* aAttr) {
  int32_t i;
  return IsPropCleared(aProp, aAttr, i);
}

bool TypeInState::IsPropCleared(nsAtom* aProp, nsAtom* aAttr,
                                int32_t& outIndex) {
  if (FindPropInList(aProp, aAttr, nullptr, mClearedArray, outIndex)) {
    return true;
  }
  if (AreAllStylesCleared()) {
    // special case for all props cleared
    outIndex = -1;
    return true;
  }
  return false;
}

bool TypeInState::FindPropInList(nsAtom* aProp, nsAtom* aAttr,
                                 nsAString* outValue,
                                 const nsTArray<PropItem*>& aList,
                                 int32_t& outIndex) {
  if (aAttr == nsGkAtoms::_empty) {
    aAttr = nullptr;
  }
  // linear search.  list should be short.
  size_t count = aList.Length();
  for (size_t i = 0; i < count; i++) {
    PropItem* item = aList[i];
    if (item->tag == aProp && item->attr == aAttr) {
      if (outValue) {
        *outValue = item->value;
      }
      outIndex = i;
      return true;
    }
  }
  return false;
}

}  // namespace mozilla
