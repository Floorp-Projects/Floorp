/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsEditorShellMouseListener.h"
#include "nsEditorShell.h"
#include "nsString.h"

#include "nsIDOMEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIDOMElement.h"
#include "nsIDOMCharacterData.h"
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

/*
 * nsEditorShellMouseListener implementation
 */
NS_IMPL_ADDREF(nsEditorShellMouseListener)

NS_IMPL_RELEASE(nsEditorShellMouseListener)

nsEditorShellMouseListener::nsEditorShellMouseListener() 
{
  NS_INIT_REFCNT();
}

nsEditorShellMouseListener::~nsEditorShellMouseListener() 
{
}

nsresult
nsEditorShellMouseListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)this;
    *aInstancePtr = (nsISupports*)*aInstancePtr;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMEventListener))) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMMouseListener))) {
    *aInstancePtr = (void*)(nsIDOMMouseListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsISupportsWeakReference))) {
    *aInstancePtr = (void*)(nsISupportsWeakReference*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}
// Helpers to test if in a table

static
PRBool GetParentTable(nsIDOMEvent* aMouseEvent, nsIDOMElement **aTableElement)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent) return PR_FALSE;
  nsCOMPtr<nsIDOMEventTarget> target;
  if (NS_SUCCEEDED(aMouseEvent->GetTarget(getter_AddRefs(target))) && target)
  {
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(target);
    
    while (node)
    {
      nsCOMPtr<nsIDOMHTMLTableElement> table = do_QueryInterface(node);
      if (table)
      {
        nsCOMPtr<nsIDOMElement> tableElement = do_QueryInterface(table);
        if (tableElement)
        {
          *aTableElement = tableElement;
          NS_ADDREF(*aTableElement);
          return PR_TRUE;
        }
      }
      nsCOMPtr<nsIDOMNode>parent;
      if (NS_FAILED(node->GetParentNode(getter_AddRefs(parent))) || !parent)
        return PR_FALSE;
      node = parent;
    }
  }
  return PR_FALSE;
}

static
PRBool GetParentCell(nsIDOMEvent* aMouseEvent, nsIDOMElement **aCellElement)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent) return PR_FALSE;
  nsCOMPtr<nsIDOMEventTarget> target;
  if (NS_SUCCEEDED(aMouseEvent->GetTarget(getter_AddRefs(target))) && target)
  {
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(target);
    while (node)
    {
      nsCOMPtr<nsIDOMHTMLTableCellElement> cell = do_QueryInterface(node);
      if (cell)
      {
        nsCOMPtr<nsIDOMElement> cellElement = do_QueryInterface(cell);
        if (cellElement)
        {
          *aCellElement = cellElement;
          NS_ADDREF(*aCellElement);
          return PR_TRUE;
        }
      }
      nsCOMPtr<nsIDOMNode>parent;
      if (NS_FAILED(node->GetParentNode(getter_AddRefs(parent))) || !parent)
        return PR_FALSE;
      node = parent;
    }
  }
  return PR_FALSE;
}

static 
PRBool ElementIsType(nsIDOMElement *aElement, const nsAReadableString& aTag)
{
  if (aElement)
  {
    nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
    if (content)
    {
      nsCOMPtr<nsIAtom> atom;
      content->GetTag(*getter_AddRefs(atom));
      if (atom)
      {
        nsAutoString tag;
        atom->ToString(tag);
        tag.ToLowerCase();
        if (tag.Equals(aTag))
          return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

nsresult
nsEditorShellMouseListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

nsresult
nsEditorShellMouseListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent || !mEditorShell) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }
  // Don't do anything special if not an HTML editor
  nsCOMPtr<nsIEditor> editor;
  nsresult res = mEditorShell->GetEditor(getter_AddRefs(editor));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (htmlEditor)
  {
    PRUint16 buttonNumber;
    res = mouseEvent->GetButton(&buttonNumber);
    if (NS_FAILED(res)) return res;

    PRBool isContextClick;
    // Test if special 'table selection' key is pressed when double-clicking
    //  so we look for an enclosing cell or table
    PRBool tableMode = PR_FALSE;

  #ifdef XP_MAC
    // Cmd is Mac table-select key
    res = mouseEvent->GetMetaKey(&tableMode);
    if (NS_FAILED(res)) return res;

    // Ctrl+Click for context menu
    res = mouseEvent->GetCtrlKey(&isContextClick);
  #else
    // Right mouse button for Windows, UNIX
    isContextClick = buttonNumber == 2;
    res = mouseEvent->GetCtrlKey(&tableMode);
  #endif
    if (NS_FAILED(res)) return res;

    nsCOMPtr<nsIDOMEventTarget> target;
    res = aMouseEvent->GetTarget(getter_AddRefs(target));
    if (NS_FAILED(res)) return res;
    if (!target) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(target);

    PRInt32 clickCount;
    res = mouseEvent->GetDetail(&clickCount);
    if (NS_FAILED(res)) return res;

    nsCOMPtr<nsISelection> selection;
    mEditorShell->GetEditorSelection(getter_AddRefs(selection));
    if (!selection) return NS_OK;

    nsCOMPtr<nsIDOMNode> parent;
    PRInt32 offset = 0;

    // Get location of mouse within target node
    nsCOMPtr<nsIDOMNSUIEvent> uiEvent = do_QueryInterface(aMouseEvent);
    if (!uiEvent) return NS_ERROR_FAILURE;

    res = uiEvent->GetRangeParent(getter_AddRefs(parent));
    if (NS_FAILED(res)) return res;
    if (!parent) return NS_ERROR_FAILURE;

    res = uiEvent->GetRangeOffset(&offset);
    if (NS_FAILED(res)) return res;

    // Detect if mouse point is withing current selection
    PRBool NodeIsInSelection = PR_FALSE;
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

        res = nsrange->IsPointInRange(parent, offset, &NodeIsInSelection);

        // Done when we find a range that we are in
        if (NodeIsInSelection)
          break;
      }
    }

    if (isContextClick || (buttonNumber == 0 && clickCount == 2))
    {
      // Context menu or double click
      nsCOMPtr<nsIDOMNode> node = do_QueryInterface(target);
      if (node && !NodeIsInSelection)
      {

        // Get enclosing link
        nsCOMPtr<nsIDOMElement> linkElement;
        res = mEditorShell->GetElementOrParentByTagName(NS_LITERAL_STRING("href").get(), node, getter_AddRefs(linkElement));
        if (NS_FAILED(res)) return res;
        if (linkElement)
          element = linkElement;
        if (element)
          selection->Collapse(parent, offset);
      }
    }

    if (isContextClick)
    {
      // Set selection to node clicked on if NOT within an existing selection
      //   and not the entire body or table-related elements
      if (element && !NodeIsInSelection && 
          !ElementIsType(element, NS_LITERAL_STRING("body")) &&
          !ElementIsType(element, NS_LITERAL_STRING("td")) &&
          !ElementIsType(element, NS_LITERAL_STRING("th")) &&
          !ElementIsType(element, NS_LITERAL_STRING("caption")) &&
          !ElementIsType(element, NS_LITERAL_STRING("tr")) &&
          !ElementIsType(element, NS_LITERAL_STRING("table")))
      {
        mEditorShell->SelectElement(element);
      }
    }
    else if (buttonNumber == 0)
    {
      if (tableMode && clickCount == 2)
      {
        if (!GetParentCell(aMouseEvent, getter_AddRefs(element)))
          GetParentTable(aMouseEvent, getter_AddRefs(element));
      }
      // No table or cell -- look for other element (ignore text nodes)
      if (element)
      {
        PRInt32 x,y;
        res = mouseEvent->GetClientX(&x);
        if (NS_FAILED(res)) return res;

        res = mouseEvent->GetClientY(&y);
        if (NS_FAILED(res)) return res;

        // KLUDGE to work around bug 50703: Prevent drag/drop events until we are done
        if (clickCount == 2)
          htmlEditor->IgnoreSpuriousDragEvent(PR_TRUE);

        // Let editor decide what to do with this
        PRBool handled = PR_FALSE;
        mEditorShell->HandleMouseClickOnElement(element, clickCount, x, y, &handled);

        if (handled)
          mouseEvent->PreventDefault();
      }
    }
  }

  return NS_OK;
}

nsresult
nsEditorShellMouseListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

nsresult
nsEditorShellMouseListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

nsresult
nsEditorShellMouseListener::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

nsresult
nsEditorShellMouseListener::MouseOver(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

nsresult
nsEditorShellMouseListener::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

nsresult
NS_NewEditorShellMouseListener(nsIDOMEventListener ** aInstancePtrResult, 
                               nsIEditorShell *aEditorShell)
{
  nsEditorShellMouseListener* listener = new nsEditorShellMouseListener();
  if (!listener) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  listener->SetEditorShell(aEditorShell);

  return listener->QueryInterface(NS_GET_IID(nsIDOMEventListener), (void **) aInstancePtrResult);   
}
