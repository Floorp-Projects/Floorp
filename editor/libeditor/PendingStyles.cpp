/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PendingStyles.h"

#include <stddef.h>

#include "EditAction.h"
#include "EditorBase.h"
#include "HTMLEditor.h"
#include "HTMLEditUtils.h"

#include "mozilla/mozalloc.h"
#include "mozilla/dom/AncestorIterator.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/Selection.h"

#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsINode.h"
#include "nsISupportsBase.h"
#include "nsISupportsImpl.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {

using namespace dom;

/********************************************************************
 * mozilla::PendingStyles
 *******************************************************************/

NS_IMPL_CYCLE_COLLECTION_CLASS(PendingStyles)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(PendingStyles)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLastSelectionPoint)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(PendingStyles)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLastSelectionPoint)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(PendingStyles, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(PendingStyles, Release)

nsresult PendingStyles::UpdateSelState(const HTMLEditor& aHTMLEditor) {
  if (!aHTMLEditor.SelectionRef().IsCollapsed()) {
    return NS_OK;
  }

  mLastSelectionPoint =
      aHTMLEditor.GetFirstSelectionStartPoint<EditorDOMPoint>();
  if (!mLastSelectionPoint.IsSet()) {
    return NS_ERROR_FAILURE;
  }
  // We need to store only offset because referring child may be removed by
  // we'll check the point later.
  AutoEditorDOMPointChildInvalidator saveOnlyOffset(mLastSelectionPoint);
  return NS_OK;
}

void PendingStyles::PreHandleMouseEvent(const MouseEvent& aMouseDownOrUpEvent) {
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
  nsIContent* targetContent = nsIContent::FromEventTarget(target);
  if (NS_WARN_IF(!targetContent)) {
    return;
  }
  eventFiredInLinkElement =
      HTMLEditUtils::IsContentInclusiveDescendantOfLink(*targetContent);
}

void PendingStyles::PreHandleSelectionChangeCommand(Command aCommand) {
  mLastSelectionCommand = aCommand;
}

void PendingStyles::PostHandleSelectionChangeCommand(
    const HTMLEditor& aHTMLEditor, Command aCommand) {
  if (mLastSelectionCommand != aCommand) {
    return;
  }

  // If `OnSelectionChange()` hasn't been called for `mLastSelectionCommand`,
  // it means that it didn't cause selection change.
  if (!aHTMLEditor.SelectionRef().IsCollapsed() ||
      !aHTMLEditor.SelectionRef().RangeCount()) {
    return;
  }

  const auto caretPoint =
      aHTMLEditor.GetFirstSelectionStartPoint<EditorRawDOMPoint>();
  if (NS_WARN_IF(!caretPoint.IsSet())) {
    return;
  }

  if (!HTMLEditUtils::IsPointAtEdgeOfLink(caretPoint)) {
    return;
  }

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
    ClearLinkAndItsSpecifiedStyle();
    return;
  }

  Reset();
  ClearLinkAndItsSpecifiedStyle();
}

void PendingStyles::OnSelectionChange(const HTMLEditor& aHTMLEditor,
                                      int16_t aReason) {
  // XXX: Selection currently generates bogus selection changed notifications
  // XXX: (bug 140303). It can notify us when the selection hasn't actually
  // XXX: changed, and it notifies us more than once for the same change.
  // XXX:
  // XXX: The following code attempts to work around the bogus notifications,
  // XXX: and should probably be removed once bug 140303 is fixed.
  // XXX:
  // XXX: This code temporarily fixes the problem where clicking the mouse in
  // XXX: the same location clears the type-in-state.

  const bool causedByFrameSelectionMoveCaret =
      (aReason & (nsISelectionListener::KEYPRESS_REASON |
                  nsISelectionListener::COLLAPSETOSTART_REASON |
                  nsISelectionListener::COLLAPSETOEND_REASON)) &&
      !(aReason & nsISelectionListener::JS_REASON);

  Command lastSelectionCommand = mLastSelectionCommand;
  if (causedByFrameSelectionMoveCaret) {
    mLastSelectionCommand = Command::DoNothing;
  }

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
  if (aHTMLEditor.SelectionRef().IsCollapsed() &&
      aHTMLEditor.SelectionRef().RangeCount()) {
    const auto selectionStartPoint =
        aHTMLEditor.GetFirstSelectionStartPoint<EditorDOMPoint>();
    if (MOZ_UNLIKELY(NS_WARN_IF(!selectionStartPoint.IsSet()))) {
      return;
    }

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

    RefPtr<Element> linkElement;
    if (HTMLEditUtils::IsPointAtEdgeOfLink(selectionStartPoint,
                                           getter_AddRefs(linkElement))) {
      // If caret comes from outside of <a href> element, we should clear "link"
      // style after reset.
      if (causedByFrameSelectionMoveCaret) {
        MOZ_ASSERT(!(aReason & (nsISelectionListener::MOUSEDOWN_REASON |
                                nsISelectionListener::MOUSEUP_REASON)));
        // If caret is moves in a link per character, we should keep inserting
        // new text to the link because user may want to keep extending the link
        // text.  Otherwise, e.g., using `End` or `Home` key. we should insert
        // new text outside the link because it should be possible to user
        // choose it, and this is similar to the other browsers.
        switch (lastSelectionCommand) {
          case Command::CharNext:
          case Command::CharPrevious:
          case Command::MoveLeft:
          case Command::MoveLeft2:
          case Command::MoveRight:
          case Command::MoveRight2:
            // If selection becomes collapsed, we should unlink new text.
            if (!mLastSelectionPoint.IsSet()) {
              unlink = true;
              break;
            }
            // Special case, if selection isn't moved, it means that caret is
            // positioned at start or end of an editing host.  In this case,
            // we can unlink it even with arrow key press.
            // TODO: This does not work as expected for `ArrowLeft` key press
            //       at start of an editing host.
            if (mLastSelectionPoint == selectionStartPoint) {
              unlink = true;
              break;
            }
            // Otherwise, if selection is moved in a link element, we should
            // keep inserting new text into the link.  Note that this is our
            // traditional behavior, but different from the other browsers.
            // If this breaks some web apps, we should change our behavior,
            // but let's wait a report because our traditional behavior allows
            // user to type text into start/end of a link only when user
            // moves caret inside the link with arrow keys.
            unlink =
                !mLastSelectionPoint.GetContainer()->IsInclusiveDescendantOf(
                    linkElement);
            break;
          default:
            // If selection is moved without arrow keys, e.g., `Home` and
            // `End`, we should not insert new text into the link element.
            // This is important for web-compat especially when the link is
            // the last content in the block.
            unlink = true;
            break;
        }
      } else if (aReason & (nsISelectionListener::MOUSEDOWN_REASON |
                            nsISelectionListener::MOUSEUP_REASON)) {
        // If the corresponding mouse event is fired in a link element,
        // we should keep treating inputting content as content in the link,
        // but otherwise, i.e., clicked outside the link, we should stop
        // treating inputting content as content in the link.
        unlink = !mouseEventFiredInLinkElement;
      } else if (aReason & nsISelectionListener::JS_REASON) {
        // If this is caused by a call of Selection API or something similar
        // API, we should not contain new inserting content to the link.
        unlink = true;
      } else {
        switch (aHTMLEditor.GetEditAction()) {
          case EditAction::eDeleteBackward:
          case EditAction::eDeleteForward:
          case EditAction::eDeleteSelection:
          case EditAction::eDeleteToBeginningOfSoftLine:
          case EditAction::eDeleteToEndOfSoftLine:
          case EditAction::eDeleteWordBackward:
          case EditAction::eDeleteWordForward:
            // This selection change is caused by the editor and the edit
            // action is deleting content at edge of a link, we shouldn't
            // keep the link style for new inserted content.
            unlink = true;
            break;
          default:
            break;
        }
      }
    } else if (mLastSelectionPoint == selectionStartPoint) {
      return;
    }

    mLastSelectionPoint = selectionStartPoint;
    // We need to store only offset because referring child may be removed by
    // we'll check the point later.
    AutoEditorDOMPointChildInvalidator saveOnlyOffset(mLastSelectionPoint);
  } else {
    if (aHTMLEditor.SelectionRef().RangeCount()) {
      // If selection starts from a link, we shouldn't preserve the link style
      // unless the range is entirely in the link.
      EditorRawDOMRange firstRange(*aHTMLEditor.SelectionRef().GetRangeAt(0));
      if (firstRange.StartRef().IsInContentNode() &&
          HTMLEditUtils::IsContentInclusiveDescendantOfLink(
              *firstRange.StartRef().ContainerAs<nsIContent>())) {
        unlink = !HTMLEditUtils::IsRangeEntirelyInLink(firstRange);
      }
    }
    mLastSelectionPoint.Clear();
  }

  if (resetAllStyles) {
    Reset();
    if (unlink) {
      ClearLinkAndItsSpecifiedStyle();
    }
    return;
  }

  if (unlink == IsExplicitlyLinkStyleCleared()) {
    return;
  }

  // Even if we shouldn't touch existing style, we need to set/clear only link
  // style in some cases.
  if (unlink) {
    ClearLinkAndItsSpecifiedStyle();
    return;
  }
  CancelClearingStyle(*nsGkAtoms::a, nullptr);
}

void PendingStyles::PreserveStyles(
    const nsTArray<EditorInlineStyleAndValue>& aStylesToPreserve) {
  for (const EditorInlineStyleAndValue& styleToPreserve : aStylesToPreserve) {
    PreserveStyle(styleToPreserve.HTMLPropertyRef(), styleToPreserve.mAttribute,
                  styleToPreserve.mAttributeValue);
  }
}

void PendingStyles::PreserveStyle(nsStaticAtom& aHTMLProperty,
                                  nsAtom* aAttribute,
                                  const nsAString& aAttributeValueOrCSSValue) {
  // special case for big/small, these nest
  if (nsGkAtoms::big == &aHTMLProperty) {
    mRelativeFontSize++;
    return;
  }
  if (nsGkAtoms::small == &aHTMLProperty) {
    mRelativeFontSize--;
    return;
  }

  Maybe<size_t> index = IndexOfPreservingStyle(aHTMLProperty, aAttribute);
  if (index.isSome()) {
    // If it's already set, update the value
    mPreservingStyles[index.value()]->UpdateAttributeValueOrCSSValue(
        aAttributeValueOrCSSValue);
    return;
  }

  mPreservingStyles.AppendElement(MakeUnique<PendingStyle>(
      &aHTMLProperty, aAttribute, aAttributeValueOrCSSValue));

  CancelClearingStyle(aHTMLProperty, aAttribute);
}

void PendingStyles::ClearStyles(
    const nsTArray<EditorInlineStyle>& aStylesToClear) {
  for (const EditorInlineStyle& styleToClear : aStylesToClear) {
    if (styleToClear.IsStyleToClearAllInlineStyles()) {
      ClearAllStyles();
      return;
    }
    if (styleToClear.mHTMLProperty == nsGkAtoms::href ||
        styleToClear.mHTMLProperty == nsGkAtoms::name) {
      ClearStyleInternal(nsGkAtoms::a, nullptr);
    } else {
      ClearStyleInternal(styleToClear.mHTMLProperty, styleToClear.mAttribute);
    }
  }
}

void PendingStyles::ClearStyleInternal(
    nsStaticAtom* aHTMLProperty, nsAtom* aAttribute,
    SpecifiedStyle aSpecifiedStyle /* = SpecifiedStyle::Preserve */) {
  if (IsStyleCleared(aHTMLProperty, aAttribute)) {
    return;
  }

  CancelPreservingStyle(aHTMLProperty, aAttribute);

  mClearingStyles.AppendElement(MakeUnique<PendingStyle>(
      aHTMLProperty, aAttribute, u""_ns, aSpecifiedStyle));
}

/**
 * TakeRelativeFontSize() hands back relative font value, which is then
 * cleared out.
 */
int32_t PendingStyles::TakeRelativeFontSize() {
  int32_t relSize = mRelativeFontSize;
  mRelativeFontSize = 0;
  return relSize;
}

PendingStyleState PendingStyles::GetStyleState(
    nsStaticAtom& aHTMLProperty, nsAtom* aAttribute /* = nullptr */,
    nsString* aOutNewAttributeValueOrCSSValue /* = nullptr */) const {
  if (IndexOfPreservingStyle(aHTMLProperty, aAttribute,
                             aOutNewAttributeValueOrCSSValue)
          .isSome()) {
    return PendingStyleState::BeingPreserved;
  }

  if (IsStyleCleared(&aHTMLProperty, aAttribute)) {
    return PendingStyleState::BeingCleared;
  }

  return PendingStyleState::NotUpdated;
}

void PendingStyles::CancelPreservingStyle(nsStaticAtom* aHTMLProperty,
                                          nsAtom* aAttribute) {
  if (!aHTMLProperty) {
    mPreservingStyles.Clear();
    mRelativeFontSize = 0;
    return;
  }
  Maybe<size_t> index = IndexOfPreservingStyle(*aHTMLProperty, aAttribute);
  if (index.isSome()) {
    mPreservingStyles.RemoveElementAt(index.value());
  }
}

void PendingStyles::CancelClearingStyle(nsStaticAtom& aHTMLProperty,
                                        nsAtom* aAttribute) {
  Maybe<size_t> index =
      IndexOfStyleInArray(&aHTMLProperty, aAttribute, nullptr, mClearingStyles);
  if (index.isSome()) {
    mClearingStyles.RemoveElementAt(index.value());
  }
}

Maybe<size_t> PendingStyles::IndexOfStyleInArray(
    nsStaticAtom* aHTMLProperty, nsAtom* aAttribute, nsAString* aOutValue,
    const nsTArray<UniquePtr<PendingStyle>>& aArray) {
  if (aAttribute == nsGkAtoms::_empty) {
    aAttribute = nullptr;
  }
  for (size_t i : IntegerRange(aArray.Length())) {
    const UniquePtr<PendingStyle>& item = aArray[i];
    if (item->GetTag() == aHTMLProperty && item->GetAttribute() == aAttribute) {
      if (aOutValue) {
        *aOutValue = item->AttributeValueOrCSSValueRef();
      }
      return Some(i);
    }
  }
  return Nothing();
}

}  // namespace mozilla
