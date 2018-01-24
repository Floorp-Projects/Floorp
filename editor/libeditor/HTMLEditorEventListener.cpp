/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditorEventListener.h"

#include "HTMLEditUtils.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Selection.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMNode.h"
#include "nsISupportsImpl.h"
#include "nsLiteralString.h"
#include "nsQueryObject.h"
#include "nsRange.h"

namespace mozilla {

using namespace dom;

nsresult
HTMLEditorEventListener::Connect(EditorBase* aEditorBase)
{
  if (NS_WARN_IF(!aEditorBase)) {
    return NS_ERROR_INVALID_ARG;
  }
  // Guarantee that mEditorBase is always HTMLEditor.
  HTMLEditor* htmlEditor = aEditorBase->AsHTMLEditor();
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  return EditorEventListener::Connect(htmlEditor);
}

nsresult
HTMLEditorEventListener::MouseUp(nsIDOMMouseEvent* aMouseEvent)
{
  if (DetachedFromEditor()) {
    return NS_OK;
  }

  // FYI: We need to notify HTML editor of mouseup even if it's consumed
  //      because HTML editor always needs to release grabbing resizer.
  HTMLEditor* htmlEditor = mEditorBase->AsHTMLEditor();
  MOZ_ASSERT(htmlEditor);

  nsCOMPtr<nsIDOMEventTarget> target;
  nsresult rv = aMouseEvent->AsEvent()->GetTarget(getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(target, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(target);

  int32_t clientX, clientY;
  aMouseEvent->GetClientX(&clientX);
  aMouseEvent->GetClientY(&clientY);
  htmlEditor->OnMouseUp(clientX, clientY, element);

  return EditorEventListener::MouseUp(aMouseEvent);
}

nsresult
HTMLEditorEventListener::MouseDown(nsIDOMMouseEvent* aMouseEvent)
{
  if (NS_WARN_IF(!aMouseEvent) || DetachedFromEditor()) {
    return NS_OK;
  }

  // Even if it's not acceptable mousedown event (i.e., when mousedown
  // event is fired outside of the active editing host), we need to commit
  // composition because it will be change the selection to the clicked
  // point.  Then, we won't be able to commit the composition.
  if (!EnsureCommitCompoisition()) {
    return NS_OK;
  }

  WidgetMouseEvent* mousedownEvent =
    aMouseEvent->AsEvent()->WidgetEventPtr()->AsMouseEvent();

  HTMLEditor* htmlEditor = mEditorBase->AsHTMLEditor();
  MOZ_ASSERT(htmlEditor);

  // Contenteditable should disregard mousedowns outside it.
  // IsAcceptableInputEvent() checks it for a mouse event.
  if (!htmlEditor->IsAcceptableInputEvent(mousedownEvent)) {
    return EditorEventListener::MouseDown(aMouseEvent);
  }

  // Detect only "context menu" click
  // XXX This should be easier to do!
  // But eDOMEvents_contextmenu and eContextMenu is not exposed in any event
  // interface :-(
  int16_t buttonNumber;
  nsresult rv = aMouseEvent->GetButton(&buttonNumber);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isContextClick = buttonNumber == 2;

  int32_t clickCount;
  rv = aMouseEvent->GetDetail(&clickCount);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMEventTarget> target;
  rv = aMouseEvent->AsEvent()->GetExplicitOriginalTarget(getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(target, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(target);

  if (isContextClick || (buttonNumber == 0 && clickCount == 2)) {
    RefPtr<Selection> selection = htmlEditor->GetSelection();
    NS_ENSURE_TRUE(selection, NS_OK);

    // Get location of mouse within target node
    nsCOMPtr<nsIDOMNode> parent;
    rv = aMouseEvent->GetRangeParent(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(parent, NS_ERROR_FAILURE);

    int32_t offset = 0;
    rv = aMouseEvent->GetRangeOffset(&offset);
    NS_ENSURE_SUCCESS(rv, rv);

    // Detect if mouse point is within current selection for context click
    bool nodeIsInSelection = false;
    if (isContextClick && !selection->Collapsed()) {
      uint32_t rangeCount = selection->RangeCount();

      for (uint32_t i = 0; i < rangeCount; i++) {
        RefPtr<nsRange> range = selection->GetRangeAt(i);
        if (!range) {
          // Don't bail yet, iterate through them all
          continue;
        }

        range->IsPointInRange(parent, offset, &nodeIsInSelection);

        // Done when we find a range that we are in
        if (nodeIsInSelection) {
          break;
        }
      }
    }
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(target);
    if (node && !nodeIsInSelection) {
      if (!element) {
        if (isContextClick) {
          // Set the selection to the point under the mouse cursor:
          selection->Collapse(parent, offset);
        } else {
          // Get enclosing link if in text so we can select the link
          nsCOMPtr<nsIDOMElement> linkElement;
          rv = htmlEditor->GetElementOrParentByTagName(
                             NS_LITERAL_STRING("href"), node,
                             getter_AddRefs(linkElement));
          NS_ENSURE_SUCCESS(rv, rv);
          if (linkElement) {
            element = linkElement;
          }
        }
      }
      // Select entire element clicked on if NOT within an existing selection
      //   and not the entire body, or table-related elements
      if (element) {
        if (isContextClick && !HTMLEditUtils::IsImage(node)) {
          selection->Collapse(parent, offset);
        } else {
          htmlEditor->SelectElement(element);
        }
        if (DetachedFromEditor()) {
          return NS_OK;
        }
      }
    }
    // HACK !!! Context click places the caret but the context menu consumes
    // the event; so we need to check resizing state ourselves
    htmlEditor->CheckSelectionStateForAnonymousButtons(selection);

    // Prevent bubbling if we changed selection or
    //   for all context clicks
    if (element || isContextClick) {
      aMouseEvent->AsEvent()->PreventDefault();
      return NS_OK;
    }
  } else if (!isContextClick && buttonNumber == 0 && clickCount == 1) {
    // if the target element is an image, we have to display resizers
    int32_t clientX, clientY;
    aMouseEvent->GetClientX(&clientX);
    aMouseEvent->GetClientY(&clientY);
    htmlEditor->OnMouseDown(clientX, clientY, element, aMouseEvent->AsEvent());
  }

  return EditorEventListener::MouseDown(aMouseEvent);
}

nsresult
HTMLEditorEventListener::MouseClick(nsIDOMMouseEvent* aMouseEvent)
{
  if (NS_WARN_IF(DetachedFromEditor())) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMEventTarget> target;
  nsresult rv = aMouseEvent->AsEvent()->GetTarget(getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(target, NS_ERROR_NULL_POINTER);
  nsCOMPtr<Element> element = do_QueryInterface(target);
  if (NS_WARN_IF(!element)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<HTMLEditor> htmlEditor = mEditorBase->AsHTMLEditor();
  MOZ_ASSERT(htmlEditor);
  htmlEditor->DoInlineTableEditingAction(*element);
  // DoInlineTableEditingAction might cause reframe
  // Editor is destroyed.
  if (htmlEditor->Destroyed()) {
    return NS_OK;
  }

  return EditorEventListener::MouseClick(aMouseEvent);
}

} // namespace mozilla
