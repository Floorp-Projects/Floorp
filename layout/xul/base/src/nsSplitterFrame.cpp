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

#include "nsSplitterFrame.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsISupportsArray.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIXMLContent.h"
#include "nsDocument.h"
#include "nsINameSpaceManager.h"
#include "nsScrollbarButtonFrame.h"

static NS_DEFINE_IID(kIAnonymousContentCreatorIID,     NS_IANONYMOUS_CONTENT_CREATOR_IID);

//
// NS_NewSplitterFrame
//
// Creates a new Toolbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewSplitterFrame ( nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsSplitterFrame* it = new nsSplitterFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewSplitterFrame

nsresult NS_CreateAnonymousNode(nsIContent* aParent, nsIAtom* aTag, PRInt32 aNameSpaceId, nsCOMPtr<nsIContent>& aNewNode);

/**
 * Anonymous interface
 */
NS_IMETHODIMP
nsSplitterFrame::CreateAnonymousContent(nsISupportsArray& aAnonymousChildren)
{
  // if not content the create some anonymous content
  PRInt32 count = 0;
  mContent->ChildCount(count); 

  if (count == 0) {

    nsCOMPtr<nsIContent> content;
    NS_CreateAnonymousNode(mContent, nsXULAtoms::spring, nsXULAtoms::nameSpaceID, content);
    content->SetAttribute(kNameSpaceID_None, nsXULAtoms::flex, "100%", PR_FALSE);
    aAnonymousChildren.AppendElement(content);

    // a grippy
    NS_CreateAnonymousNode(mContent, nsXULAtoms::grippy, nsXULAtoms::nameSpaceID, content);
    aAnonymousChildren.AppendElement(content);

    // create a spring
    NS_CreateAnonymousNode(mContent, nsXULAtoms::spring, nsXULAtoms::nameSpaceID, content);
    content->SetAttribute(kNameSpaceID_None, nsXULAtoms::flex, "100%", PR_FALSE);
    aAnonymousChildren.AppendElement(content);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSplitterFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aPresContext, aChild,
                                              aAttribute, aHint);
  // if the alignment changed. Let the grippy know
  if (aAttribute == nsHTMLAtoms::align) {
     // tell the slider its attribute changed so it can 
     // update itself
     nsIFrame* grippy = nsnull;
     nsScrollbarButtonFrame::GetChildWithTag(nsXULAtoms::grippy, this, grippy);
     if (grippy)
        grippy->AttributeChanged(aPresContext, aChild, aAttribute, aHint);
  }

  return rv;
}

/**
 * Initialize us. If we are in a box get our alignment so we know what direction we are
 */
NS_IMETHODIMP
nsSplitterFrame::Init(nsIPresContext&  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // find the box we are in
  nsIFrame* box = nsnull;
  nsScrollbarButtonFrame::GetParentWithTag(nsXULAtoms::box, this, box);

  // if no box get the window because it is a box.
  if (box == nsnull)
      nsScrollbarButtonFrame::GetParentWithTag(nsXULAtoms::window, this, box);

  // see if the box is horizontal or vertical
  if (box) {
    nsCOMPtr<nsIContent> content;  
    box->GetContent(getter_AddRefs(content));

    nsString value;
    content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, value);
    if (value.EqualsIgnoreCase("vertical"))
      mHorizontal = PR_TRUE;
    else 
      mHorizontal = PR_FALSE;
  }
   
  return rv;
}

NS_IMETHODIMP 
nsSplitterFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
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

