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
#include "nsIDOMMouseEvent.h"
#include "nsIDOMSelection.h"

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

nsresult
nsEditorShellMouseListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

nsresult
nsEditorShellMouseListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }
  PRUint16 buttonNumber;
  nsresult res = mouseEvent->GetButton(&buttonNumber);
  if (NS_FAILED(res)) return res;

  // Should we do this only for "right" mouse button?
  // What about Mac?
  if (mEditorShell && buttonNumber == 3)
  {
    nsCOMPtr<nsIDOMNode> node;
    if (NS_SUCCEEDED(aMouseEvent->GetTarget(getter_AddRefs(node))) && node)
    {
      // We are only interested in elements, not text nodes
      nsCOMPtr<nsIDOMElement> element = do_QueryInterface(node);
      if (element)
      {
        // Set selection to node clicked on
        mEditorShell->SelectElement(element);
        return NS_ERROR_BASE; // consumed
      }
    }
  }
  return NS_OK;
}

nsresult
nsEditorShellMouseListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }
  // Detect double click message:
  PRUint16 clickCount;
  nsresult res = mouseEvent->GetClickCount(&clickCount);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> node;
  if (NS_SUCCEEDED(aMouseEvent->GetTarget(getter_AddRefs(node))) && node)
  {
    // We are only interested in elements, not text nodes
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(node);
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
        return NS_ERROR_BASE; // consumed
    }
  }
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
