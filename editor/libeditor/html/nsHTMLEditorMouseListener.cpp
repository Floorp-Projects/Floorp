/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *    Charles Manske (cmanske@netscape.com)
 *    Daniel Glazman (glazman@netscape.com)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsHTMLEditorMouseListener.h"
#include "nsString.h"

#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMElement.h"
#include "nsIDOMMouseEvent.h"
#include "nsISelection.h"
#include "nsIDOMRange.h"
#include "nsIDOMNSRange.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIContent.h"

#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIHTMLObjectResizer.h"
#include "nsEditProperty.h"
#include "nsTextEditUtils.h"
#include "nsHTMLEditUtils.h"
#include "nsIHTMLInlineTableEditor.h"

/*
 * nsHTMLEditorMouseListener implementation
 *
 * The only reason we need this is so a context mouse-click
 *  moves the caret or selects an element as it does for normal click
 */

nsHTMLEditorMouseListener::nsHTMLEditorMouseListener(nsHTMLEditor *aHTMLEditor)
  : mHTMLEditor(aHTMLEditor)
{
  SetEditor(mHTMLEditor); // Tell the base class about the editor.
}

nsHTMLEditorMouseListener::~nsHTMLEditorMouseListener() 
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLEditorMouseListener, nsTextEditorMouseListener, nsIDOMMouseListener)

nsresult
nsHTMLEditorMouseListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  // Don't do anything special if not an HTML editor
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
  if (htmlEditor)
  {
    nsCOMPtr<nsIDOMEventTarget> target;
    nsresult res = aMouseEvent->GetTarget(getter_AddRefs(target));
    if (NS_FAILED(res)) return res;
    if (!target) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(target);

    nsCOMPtr<nsIHTMLObjectResizer> objectResizer = do_QueryInterface(htmlEditor);
    PRInt32 clientX, clientY;
    mouseEvent->GetClientX(&clientX);
    mouseEvent->GetClientY(&clientY);
    objectResizer->MouseUp(clientX, clientY, element);
  }

  return nsTextEditorMouseListener::MouseUp(aMouseEvent);
}

nsresult
nsHTMLEditorMouseListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  // Don't do anything special if not an HTML editor
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
  if (htmlEditor)
  {
    // Detect only "context menu" click
    //XXX This should be easier to do!
    // But eDOMEvents_contextmenu and NS_CONTEXTMENU is not exposed in any event interface :-(
    PRUint16 buttonNumber;
    nsresult res = mouseEvent->GetButton(&buttonNumber);
    if (NS_FAILED(res)) return res;

    PRBool isContextClick;

#if defined(XP_MAC) || defined(XP_MACOSX)
    // Ctrl+Click for context menu
    res = mouseEvent->GetCtrlKey(&isContextClick);
    if (NS_FAILED(res)) return res;
#else
    // Right mouse button for Windows, UNIX
    isContextClick = buttonNumber == 2;
#endif
    
    PRInt32 clickCount;
    res = mouseEvent->GetDetail(&clickCount);
    if (NS_FAILED(res)) return res;

    nsCOMPtr<nsIDOMEventTarget> target;
    nsCOMPtr<nsIDOMNSEvent> internalEvent = do_QueryInterface(aMouseEvent);
    res = internalEvent->GetExplicitOriginalTarget(getter_AddRefs(target));
    if (NS_FAILED(res)) return res;
    if (!target) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(target);

    if (isContextClick || (buttonNumber == 0 && clickCount == 2))
    {
      nsCOMPtr<nsISelection> selection;
      mEditor->GetSelection(getter_AddRefs(selection));
      if (!selection) return NS_OK;

      // Get location of mouse within target node
      nsCOMPtr<nsIDOMNSUIEvent> uiEvent = do_QueryInterface(aMouseEvent);
      if (!uiEvent) return NS_ERROR_FAILURE;

      nsCOMPtr<nsIDOMNode> parent;
      PRInt32 offset = 0;

      res = uiEvent->GetRangeParent(getter_AddRefs(parent));
      if (NS_FAILED(res)) return res;
      if (!parent) return NS_ERROR_FAILURE;

      res = uiEvent->GetRangeOffset(&offset);
      if (NS_FAILED(res)) return res;

      // Detect if mouse point is within current selection for context click
      PRBool nodeIsInSelection = PR_FALSE;
      if (isContextClick)
      {
        PRBool isCollapsed;
        selection->GetIsCollapsed(&isCollapsed);
        if (!isCollapsed)
        {
          PRInt32 rangeCount;
          res = selection->GetRangeCount(&rangeCount);
          if (NS_FAILED(res)) return res;

          for (PRInt32 i = 0; i < rangeCount; i++)
          {
            nsCOMPtr<nsIDOMRange> range;

            res = selection->GetRangeAt(i, getter_AddRefs(range));
            if (NS_FAILED(res) || !range) 
              continue;//dont bail yet, iterate through them all

            nsCOMPtr<nsIDOMNSRange> nsrange(do_QueryInterface(range));
            if (NS_FAILED(res) || !nsrange) 
              continue;//dont bail yet, iterate through them all

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
            if (NS_FAILED(res)) return res;
            if (linkElement)
              element = linkElement;
          }
        }
        // Select entire element clicked on if NOT within an existing selection
        //   and not the entire body, or table-related elements
        if (element)
        {
          nsCOMPtr<nsIDOMNode> selectAllNode = mHTMLEditor->FindUserSelectAllNode(element);

          if (selectAllNode)
          {
            nsCOMPtr<nsIDOMElement> newElement = do_QueryInterface(selectAllNode);
            if (newElement)
            {
              node = selectAllNode;
              element = newElement;
            }
          }

// XXX: should we call nsHTMLEditUtils::IsTableElement here?
// that also checks for thead, tbody, tfoot
          if (nsTextEditUtils::IsBody(node) ||
              nsHTMLEditUtils::IsTableCellOrCaption(node) ||
              nsHTMLEditUtils::IsTableRow(node) ||
              nsHTMLEditUtils::IsTable(node))
          {
            // This will place caret just inside table cell or at start of body
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
        mouseEvent->PreventDefault();
        return NS_OK;
      }
    }
    else if (!isContextClick && buttonNumber == 0 && clickCount == 1)
    {
      // if the target element is an image, we have to display resizers
      nsCOMPtr<nsIHTMLObjectResizer> objectResizer = do_QueryInterface(htmlEditor);
      PRInt32 clientX, clientY;
      mouseEvent->GetClientX(&clientX);
      mouseEvent->GetClientY(&clientY);
      objectResizer->MouseDown(clientX, clientY, element);
    }
  }

  return nsTextEditorMouseListener::MouseDown(aMouseEvent);
}

nsresult
nsHTMLEditorMouseListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  // Don't do anything special if not an HTML inline table editor
  nsCOMPtr<nsIHTMLInlineTableEditor> inlineTableEditing = do_QueryInterface(mEditor);
  if (inlineTableEditing)
  {
    nsCOMPtr<nsIDOMEventTarget> target;
    nsresult res = aMouseEvent->GetTarget(getter_AddRefs(target));
    if (NS_FAILED(res)) return res;
    if (!target) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(target);

    inlineTableEditing->DoInlineTableEditingAction(element);
  }

  return nsTextEditorMouseListener::MouseClick(aMouseEvent);
}

nsresult
NS_NewHTMLEditorMouseListener(nsIDOMEventListener ** aInstancePtrResult, 
                              nsHTMLEditor *aHTMLEditor)
{
  nsHTMLEditorMouseListener* listener = new nsHTMLEditorMouseListener(aHTMLEditor);
  if (!listener)
    return NS_ERROR_OUT_OF_MEMORY;

  return listener->QueryInterface(NS_GET_IID(nsIDOMEventListener), (void **) aInstancePtrResult);   
}
