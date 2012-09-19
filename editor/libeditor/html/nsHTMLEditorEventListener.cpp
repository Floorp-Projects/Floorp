/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsEditor.h"
#include "nsError.h"
#include "nsHTMLEditUtils.h"
#include "nsHTMLEditor.h"
#include "nsHTMLEditorEventListener.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMNode.h"
#include "nsIDOMRange.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIHTMLInlineTableEditor.h"
#include "nsIHTMLObjectResizer.h"
#include "nsISelection.h"
#include "nsISupportsImpl.h"
#include "nsLiteralString.h"

/*
 * nsHTMLEditorEventListener implementation
 *
 */

#ifdef DEBUG
nsresult
nsHTMLEditorEventListener::Connect(nsEditor* aEditor)
{
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryObject(aEditor);
  nsCOMPtr<nsIHTMLInlineTableEditor> htmlInlineTableEditor = do_QueryObject(aEditor);
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

NS_IMETHODIMP
nsHTMLEditorEventListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  nsHTMLEditor* htmlEditor = GetHTMLEditor();

  nsCOMPtr<nsIDOMEventTarget> target;
  nsresult res = aMouseEvent->GetTarget(getter_AddRefs(target));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(target, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(target);

  int32_t clientX, clientY;
  mouseEvent->GetClientX(&clientX);
  mouseEvent->GetClientY(&clientY);
  htmlEditor->MouseUp(clientX, clientY, element);

  return nsEditorEventListener::MouseUp(aMouseEvent);
}

NS_IMETHODIMP
nsHTMLEditorEventListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  nsHTMLEditor* htmlEditor = GetHTMLEditor();

  // Detect only "context menu" click
  //XXX This should be easier to do!
  // But eDOMEvents_contextmenu and NS_CONTEXTMENU is not exposed in any event interface :-(
  uint16_t buttonNumber;
  nsresult res = mouseEvent->GetButton(&buttonNumber);
  NS_ENSURE_SUCCESS(res, res);

  bool isContextClick = buttonNumber == 2;

  int32_t clickCount;
  res = mouseEvent->GetDetail(&clickCount);
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMEventTarget> target;
  res = aMouseEvent->GetExplicitOriginalTarget(getter_AddRefs(target));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(target, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(target);

  // Contenteditable should disregard mousedowns outside it
  if (element && !htmlEditor->IsDescendantOfEditorRoot(element)) {
    return NS_OK;
  }

  if (isContextClick || (buttonNumber == 0 && clickCount == 2))
  {
    nsCOMPtr<nsISelection> selection;
    mEditor->GetSelection(getter_AddRefs(selection));
    NS_ENSURE_TRUE(selection, NS_OK);

    // Get location of mouse within target node
    nsCOMPtr<nsIDOMNode> parent;
    res = mouseEvent->GetRangeParent(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(parent, NS_ERROR_FAILURE);

    int32_t offset = 0;
    res = mouseEvent->GetRangeOffset(&offset);
    NS_ENSURE_SUCCESS(res, res);

    // Detect if mouse point is within current selection for context click
    bool nodeIsInSelection = false;
    if (isContextClick && !selection->Collapsed()) {
      int32_t rangeCount;
      res = selection->GetRangeCount(&rangeCount);
      NS_ENSURE_SUCCESS(res, res);

      for (int32_t i = 0; i < rangeCount; i++) {
        nsCOMPtr<nsIDOMRange> range;

        res = selection->GetRangeAt(i, getter_AddRefs(range));
        if (NS_FAILED(res) || !range)
          continue;//don't bail yet, iterate through them all

        res = range->IsPointInRange(parent, offset, &nodeIsInSelection);

        // Done when we find a range that we are in
        if (nodeIsInSelection)
          break;
      }
    }
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(target);
    if (node && !nodeIsInSelection)
    {
      if (!element)
      {
        if (isContextClick)
        {
          // Set the selection to the point under the mouse cursor:
          selection->Collapse(parent, offset);
        }
        else
        {
          // Get enclosing link if in text so we can select the link
          nsCOMPtr<nsIDOMElement> linkElement;
          res = htmlEditor->GetElementOrParentByTagName(NS_LITERAL_STRING("href"), node, getter_AddRefs(linkElement));
          NS_ENSURE_SUCCESS(res, res);
          if (linkElement)
            element = linkElement;
        }
      }
      // Select entire element clicked on if NOT within an existing selection
      //   and not the entire body, or table-related elements
      if (element)
      {
        nsCOMPtr<nsIDOMNode> selectAllNode =
          htmlEditor->FindUserSelectAllNode(element);

        if (selectAllNode)
        {
          nsCOMPtr<nsIDOMElement> newElement = do_QueryInterface(selectAllNode);
          if (newElement)
          {
            node = selectAllNode;
            element = newElement;
          }
        }

        if (isContextClick && !nsHTMLEditUtils::IsImage(node))
        {
          selection->Collapse(parent, offset);
        }
        else
        {
          htmlEditor->SelectElement(element);
        }
      }
    }
    // HACK !!! Context click places the caret but the context menu consumes
    // the event; so we need to check resizing state ourselves
    htmlEditor->CheckSelectionStateForAnonymousButtons(selection);

    // Prevent bubbling if we changed selection or 
    //   for all context clicks
    if (element || isContextClick)
    {
    #ifndef XP_OS2
      mouseEvent->PreventDefault();
    #endif
      return NS_OK;
    }
  }
  else if (!isContextClick && buttonNumber == 0 && clickCount == 1)
  {
    // if the target element is an image, we have to display resizers
    int32_t clientX, clientY;
    mouseEvent->GetClientX(&clientX);
    mouseEvent->GetClientY(&clientY);
    htmlEditor->MouseDown(clientX, clientY, element, aMouseEvent);
  }

  return nsEditorEventListener::MouseDown(aMouseEvent);
}

NS_IMETHODIMP
nsHTMLEditorEventListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  nsCOMPtr<nsIDOMEventTarget> target;
  nsresult res = aMouseEvent->GetTarget(getter_AddRefs(target));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(target, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(target);

  GetHTMLEditor()->DoInlineTableEditingAction(element);

  return nsEditorEventListener::MouseClick(aMouseEvent);
}
