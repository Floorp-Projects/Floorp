/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsTreeTwistyListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"

#include "nsCOMPtr.h"
#include "nsIDOMUIEvent.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsXULAtoms.h"

/*
 * nsTreeTwistyListener implementation
 */

NS_IMPL_ADDREF(nsTreeTwistyListener)
NS_IMPL_RELEASE(nsTreeTwistyListener)


////////////////////////////////////////////////////////////////////////
nsTreeTwistyListener::nsTreeTwistyListener()
{
  NS_INIT_REFCNT();
}

////////////////////////////////////////////////////////////////////////
nsTreeTwistyListener::~nsTreeTwistyListener() 
{
}

////////////////////////////////////////////////////////////////////////
nsresult
nsTreeTwistyListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(nsCOMTypeInfo<nsIDOMEventReceiver>::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)(nsIDOMKeyListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsIDOMMouseListener>::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMMouseListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {                                      
    *aInstancePtr = (void*)(nsISupports*)(nsIDOMKeyListener*)this;                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }
  return NS_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////

static void GetTreeItem(nsIDOMElement* aElement, nsIDOMElement** aResult)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  while (content) {
    nsCOMPtr<nsIAtom> tag;
    content->GetTag(*getter_AddRefs(tag));
    if (tag == nsXULAtoms::treeitem) {
      nsCOMPtr<nsIDOMElement> result = do_QueryInterface(content);
      *aResult = result.get();
      NS_IF_ADDREF(*aResult);
      return;
    }

    nsCOMPtr<nsIContent> parent;
    content->GetParent(*getter_AddRefs(parent));
    content = parent;
  }
}

nsresult
nsTreeTwistyListener::MouseDown(nsIDOMEvent* aEvent)
{  
  // Get the target of the event. If it's a titledbutton, we care.
  nsCOMPtr<nsIDOMNode> target;
  aEvent->GetTarget(getter_AddRefs(target));

  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(target);
  if (!element)
    return NS_OK;

  nsAutoString tagName;
  element->GetTagName(tagName);
  if (tagName == "titledbutton") {
    // Find out if we're the twisty.
    nsAutoString classAttr;
    element->GetAttribute("class", classAttr);
    if (classAttr == "twisty") {
      // Retrieve the parent treeitem.
      nsCOMPtr<nsIDOMElement> treeItem;
      GetTreeItem(element, getter_AddRefs(treeItem));

      if (!treeItem)
        return NS_OK;

      // Eat the event.
      aEvent->PreventCapture();
      aEvent->PreventBubble();
      
      nsAutoString open;
      treeItem->GetAttribute("open", open);
      if (open == "true")
        treeItem->RemoveAttribute("open");
      else treeItem->SetAttribute("open", "true");
    }
  }
  return NS_OK;
}
