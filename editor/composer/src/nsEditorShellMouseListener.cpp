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
#include "nsIDOMSelection.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableCellElement.h"

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

  PRUint16 buttonNumber;
  nsresult res = mouseEvent->GetButton(&buttonNumber);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMEventTarget> target;
  res = aMouseEvent->GetTarget(getter_AddRefs(target));
  if (NS_FAILED(res)) return res;
  if (!target) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(target);

  PRInt32 clickCount;
  res = mouseEvent->GetDetail(&clickCount);
  if (NS_FAILED(res)) return res;

  if (buttonNumber == 3 || (buttonNumber == 1 && clickCount == 2))
  {
    /**XXX Context menu design is flawed:
      *    Mouse message arrives here first,
      *    then to XULPopupListenerImpl::MouseDown,
      *    and never bubbles to frame.
      *    So we don't reposition the caret correctly
      *    within a text node and don't detect if clicking 
      *    on a selection. (Logic for both is in frame code.)
      *    Kludge: Set selection to beginning of text node.
      *    TODO: try to solve click within selection with a new 
      *    nsSelection method.
      *    We also want to do this for double-click to detect
      *    a link enclosing the text node
      */  

    nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(target);
    if (textNode)
    {
      //XXX We should do this only if not clicking inside an existing selection!
      nsCOMPtr<nsIDOMSelection> selection;
      mEditorShell->GetEditorSelection(getter_AddRefs(selection));
      if (selection)
        selection->Collapse(textNode, 0);
      
      // Get enclosing link
      res = mEditorShell->GetElementOrParentByTagName(NS_LITERAL_STRING("href"), textNode, getter_AddRefs(element));
      if (NS_FAILED(res)) return res;
    }
  }

  if (buttonNumber == 1)
  {
#ifdef DEBUG_cmanske
printf("nsEditorShellMouseListener::MouseDown: clickCount=%d\n",clickCount);
#endif
    // Test if special 'table selection' key is pressed when double-clicking
    //  so we look for an enclosing cell or table
    PRBool tableMode = PR_FALSE;

#ifdef XP_MAC
    res = mouseEvent->GetMetaKey(&tableMode);
#else
    res = mouseEvent->GetCtrlKey(&tableMode);
#endif
    if (NS_FAILED(res)) return res;
    if (tableMode && clickCount == 2)
    {
#ifdef DEBUG_cmanske
printf("nsEditorShellMouseListener:MouseDown-DoubleClick in TableMode\n");
#endif
      if (!GetParentCell(aMouseEvent, getter_AddRefs(element)))
        GetParentTable(aMouseEvent, getter_AddRefs(element));
#ifdef DEBUG_cmanske
      else 
printf("nsEditorShellMouseListener::MouseDown-DoubleClick in cell\n");
#endif
    }
    // No table or cell -- look for other element (ignore text nodes)
    if (element)
    {
      PRInt32 x,y;
      res = mouseEvent->GetClientX(&x);
      if (NS_FAILED(res)) return res;

      res = mouseEvent->GetClientY(&y);
      if (NS_FAILED(res)) return res;

      // Let editor decide what to do with this
      PRBool handled;
      mEditorShell->HandleMouseClickOnElement(element, clickCount, x, y, &handled);

      if (handled)
        mouseEvent->PreventDefault();
    }
  }
  // Should we do this only for "right" mouse button?
  // What about Mac?
  else if (buttonNumber == 3)
  {
    if (element)
      // Set selection to node clicked on
      mEditorShell->SelectElement(element);
      // Always fall through to do other actions, such as context menu
  }
  return NS_OK;
}

nsresult
nsEditorShellMouseListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // didn't handle event
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
