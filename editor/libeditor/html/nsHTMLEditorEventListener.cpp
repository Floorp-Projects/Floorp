/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Charles Manske (cmanske@netscape.com)
 *   Daniel Glazman (glazman@netscape.com)
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsHTMLEditorEventListener.h"
#include "nsHTMLEditor.h"
#include "nsString.h"

#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMElement.h"
#include "nsIDOMMouseEvent.h"
#include "nsISelection.h"
#include "nsIDOMRange.h"
#include "nsIDOMNSRange.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIContent.h"

#include "nsIHTMLObjectResizer.h"
#include "nsEditProperty.h"
#include "nsTextEditUtils.h"
#include "nsHTMLEditUtils.h"

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

  PRInt32 clientX, clientY;
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
  PRUint16 buttonNumber;
  nsresult res = mouseEvent->GetButton(&buttonNumber);
  NS_ENSURE_SUCCESS(res, res);

  bool isContextClick = buttonNumber == 2;

  PRInt32 clickCount;
  res = mouseEvent->GetDetail(&clickCount);
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMEventTarget> target;
  nsCOMPtr<nsIDOMNSEvent> internalEvent = do_QueryInterface(aMouseEvent);
  res = internalEvent->GetExplicitOriginalTarget(getter_AddRefs(target));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(target, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(target);

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

    PRInt32 offset = 0;
    res = mouseEvent->GetRangeOffset(&offset);
    NS_ENSURE_SUCCESS(res, res);

    // Detect if mouse point is within current selection for context click
    bool nodeIsInSelection = false;
    if (isContextClick)
    {
      bool isCollapsed;
      selection->GetIsCollapsed(&isCollapsed);
      if (!isCollapsed)
      {
        PRInt32 rangeCount;
        res = selection->GetRangeCount(&rangeCount);
        NS_ENSURE_SUCCESS(res, res);

        for (PRInt32 i = 0; i < rangeCount; i++)
        {
          nsCOMPtr<nsIDOMRange> range;

          res = selection->GetRangeAt(i, getter_AddRefs(range));
          if (NS_FAILED(res) || !range) 
            continue;//don't bail yet, iterate through them all

          nsCOMPtr<nsIDOMNSRange> nsrange(do_QueryInterface(range));
          if (NS_FAILED(res) || !nsrange) 
            continue;//don't bail yet, iterate through them all

          res = nsrange->IsPointInRange(parent, offset, &nodeIsInSelection);

          // Done when we find a range that we are in
          if (nodeIsInSelection)
            break;
        }
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
    PRInt32 clientX, clientY;
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
