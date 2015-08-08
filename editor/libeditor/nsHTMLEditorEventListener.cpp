/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLEditorEventListener.h"

#include "mozilla/dom/Selection.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsEditor.h"
#include "nsError.h"
#include "nsHTMLEditUtils.h"
#include "nsHTMLEditor.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMNode.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIHTMLInlineTableEditor.h"
#include "nsIHTMLObjectResizer.h"
#include "nsISupportsImpl.h"
#include "nsLiteralString.h"
#include "nsQueryObject.h"
#include "nsRange.h"

using namespace mozilla;
using namespace mozilla::dom;

/*
 * nsHTMLEditorEventListener implementation
 *
 */

#ifdef DEBUG
nsresult
nsHTMLEditorEventListener::Connect(nsEditor* aEditor)
{
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryObject(aEditor);
  nsCOMPtr<nsIHTMLInlineTableEditor> htmlInlineTableEditor =
    do_QueryObject(aEditor);
  NS_PRECONDITION(htmlEditor && htmlInlineTableEditor,
                  "Set nsHTMLEditor or its sub class");
  return nsEditorEventListener::Connect(aEditor);
}
#endif

nsHTMLEditor*
nsHTMLEditorEventListener::GetHTMLEditor()
{
  // mEditor must be nsHTMLEditor or its subclass.
  return static_cast<nsHTMLEditor*>(mEditor);
}

nsresult
nsHTMLEditorEventListener::MouseUp(nsIDOMMouseEvent* aMouseEvent)
{
  nsHTMLEditor* htmlEditor = GetHTMLEditor();

  nsCOMPtr<nsIDOMEventTarget> target;
  nsresult rv = aMouseEvent->GetTarget(getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(target, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(target);

  int32_t clientX, clientY;
  aMouseEvent->GetClientX(&clientX);
  aMouseEvent->GetClientY(&clientY);
  htmlEditor->MouseUp(clientX, clientY, element);

  return nsEditorEventListener::MouseUp(aMouseEvent);
}

nsresult
nsHTMLEditorEventListener::MouseDown(nsIDOMMouseEvent* aMouseEvent)
{
  nsHTMLEditor* htmlEditor = GetHTMLEditor();
  // Contenteditable should disregard mousedowns outside it.
  // IsAcceptableInputEvent() checks it for a mouse event.
  if (!htmlEditor->IsAcceptableInputEvent(aMouseEvent)) {
    // If it's not acceptable mousedown event (including when mousedown event
    // is fired outside of the active editing host), we need to commit
    // composition because it will be change the selection to the clicked
    // point.  Then, we won't be able to commit the composition.
    return nsEditorEventListener::MouseDown(aMouseEvent);
  }

  // Detect only "context menu" click
  // XXX This should be easier to do!
  // But eDOMEvents_contextmenu and NS_CONTEXTMENU is not exposed in any event
  // interface :-(
  int16_t buttonNumber;
  nsresult rv = aMouseEvent->GetButton(&buttonNumber);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isContextClick = buttonNumber == 2;

  int32_t clickCount;
  rv = aMouseEvent->GetDetail(&clickCount);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMEventTarget> target;
  rv = aMouseEvent->GetExplicitOriginalTarget(getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(target, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(target);

  if (isContextClick || (buttonNumber == 0 && clickCount == 2)) {
    nsRefPtr<Selection> selection = mEditor->GetSelection();
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
      int32_t rangeCount;
      rv = selection->GetRangeCount(&rangeCount);
      NS_ENSURE_SUCCESS(rv, rv);

      for (int32_t i = 0; i < rangeCount; i++) {
        nsRefPtr<nsRange> range = selection->GetRangeAt(i);
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
        nsCOMPtr<nsIDOMNode> selectAllNode =
          htmlEditor->FindUserSelectAllNode(element);

        if (selectAllNode) {
          nsCOMPtr<nsIDOMElement> newElement = do_QueryInterface(selectAllNode);
          if (newElement) {
            node = selectAllNode;
            element = newElement;
          }
        }

        if (isContextClick && !nsHTMLEditUtils::IsImage(node)) {
          selection->Collapse(parent, offset);
        } else {
          htmlEditor->SelectElement(element);
        }
      }
    }
    // HACK !!! Context click places the caret but the context menu consumes
    // the event; so we need to check resizing state ourselves
    htmlEditor->CheckSelectionStateForAnonymousButtons(selection);

    // Prevent bubbling if we changed selection or
    //   for all context clicks
    if (element || isContextClick) {
      aMouseEvent->PreventDefault();
      return NS_OK;
    }
  } else if (!isContextClick && buttonNumber == 0 && clickCount == 1) {
    // if the target element is an image, we have to display resizers
    int32_t clientX, clientY;
    aMouseEvent->GetClientX(&clientX);
    aMouseEvent->GetClientY(&clientY);
    htmlEditor->MouseDown(clientX, clientY, element, aMouseEvent);
  }

  return nsEditorEventListener::MouseDown(aMouseEvent);
}

nsresult
nsHTMLEditorEventListener::MouseClick(nsIDOMMouseEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMEventTarget> target;
  nsresult rv = aMouseEvent->GetTarget(getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(target, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(target);

  GetHTMLEditor()->DoInlineTableEditingAction(element);

  return nsEditorEventListener::MouseClick(aMouseEvent);
}
