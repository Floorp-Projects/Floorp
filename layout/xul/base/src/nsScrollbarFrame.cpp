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

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsScrollbarFrame.h"
#include "nsScrollbarButtonFrame.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsISupportsArray.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsDocument.h"
#include "nsINameSpaceManager.h"

static NS_DEFINE_IID(kIAnonymousContentCreatorIID,     NS_IANONYMOUS_CONTENT_CREATOR_IID);

//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewScrollbarFrame ( nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsScrollbarFrame* it = new nsScrollbarFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewScrollbarFrame

/**
 * Anonymous interface
 */
NS_IMETHODIMP
nsScrollbarFrame::CreateAnonymousContent(nsISupportsArray& aAnonymousChildren)
{
  // if not content the create some anonymous content
  PRInt32 count = 0;
  mContent->ChildCount(count); 

  if (count == 0) {
    // get the document
    nsCOMPtr<nsIDocument> idocument;
    mContent->GetDocument(*getter_AddRefs(idocument));

    nsCOMPtr<nsIDOMDocument> document(do_QueryInterface(idocument));

    // create a decrement button
    nsCOMPtr<nsIDOMElement> node;
    document->CreateElement("scrollbarbutton",getter_AddRefs(node));
    nsCOMPtr<nsIContent> content;

    content = do_QueryInterface(node);
    content->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::kClass, "decrement", PR_TRUE);
    aAnonymousChildren.AppendElement(content);

    // a slider
    document->CreateElement("slider",getter_AddRefs(node));
    content = do_QueryInterface(node);
    content->SetAttribute(kNameSpaceID_None, nsXULAtoms::flex, "100%", PR_TRUE);
    aAnonymousChildren.AppendElement(content);

    // and increment button
    document->CreateElement("scrollbarbutton",getter_AddRefs(node));
    content = do_QueryInterface(node);
    content->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::kClass, "increment", PR_TRUE);
    aAnonymousChildren.AppendElement(content);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsScrollbarFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aPresContext, aChild,
                                              aAttribute, aHint);
  // if the current position changes
  if (aAttribute == nsXULAtoms::curpos) {
     // tell the slider its attribute changed so it can 
     // update itself
     nsIFrame* slider;
     nsScrollbarButtonFrame::GetChildWithTag(nsXULAtoms::slider, this, slider);
     if (slider)
        slider->AttributeChanged(aPresContext, aChild, aAttribute, aHint);
  }

  return rv;
}

NS_IMETHODIMP 
nsScrollbarFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIAnonymousContentCreatorIID)) {                                         
    *aInstancePtr = (void*)(nsIAnonymousContentCreator*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }

  return nsBoxFrame::QueryInterface(aIID, aInstancePtr);                                     
}

