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
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIXMLContent.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsXMLElement.h"
#include "nsIStyledContent.h"
#include "nsGenericElement.h"
#include "nsIStyleRule.h"
#include "nsHTMLValue.h"
#include "nsIAnonymousContent.h"

static NS_DEFINE_IID(kIAnonymousContentCreatorIID,     NS_IANONYMOUS_CONTENT_CREATOR_IID);
static NS_DEFINE_IID(kIStyledContentIID,               NS_ISTYLEDCONTENT_IID);
static NS_DEFINE_IID(kIAnonymousContentIID,            NS_IANONYMOUS_CONTENT_IID);

class AnonymousElement : public nsXMLElement, nsIStyledContent, nsIAnonymousContent
{
public:
  AnonymousElement(nsIAtom *aTag):nsXMLElement(aTag) {}

  // nsIStyledContent
  NS_IMETHOD GetID(nsIAtom*& aResult) const;
  NS_IMETHOD GetClasses(nsVoidArray& aArray) const;
  NS_IMETHOD HasClass(nsIAtom* aClass) const;

  NS_IMETHOD GetContentStyleRules(nsISupportsArray* aRules);

  NS_IMETHOD GetInlineStyleRules(nsISupportsArray* aRules);

  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, 
                                           PRInt32& aHint) const;

  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }


  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // NS_IMPL_ICONTENT_USING_GENERIC_DOM_DATA(mInner)

};

/*
NS_IMETHODIMP 
AnonymousElement::GetTag(nsIAtom*& aResult) const
{
   return mInner.GetTag(aResult);
}


NS_IMETHODIMP
AnonymousElement::List(FILE* out, PRInt32 aIndent) const
{
  
  NS_PRECONDITION(nsnull != mInner.mDocument, "bad content");

  PRInt32 indx;
  for (indx = aIndent; --indx >= 0; ) fputs("  ", out);

  fprintf(out, "Comment refcount=%d<", mRefCnt);

  nsAutoString tmp;
  mInner.ToCString(tmp, 0, mInner.mText.GetLength());
  fputs(tmp, out);

  fputs(">\n", out);
 
  return mInner.List(out, aIndent);
}
*/

NS_IMETHODIMP
AnonymousElement::HandleDOMEvent(nsIPresContext& aPresContext,
                              nsEvent* aEvent,
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus& aEventStatus)
{
  /*
  // if our parent is not anonymous then we don't want to bubble the event
  // so lets set our parent in nsnull to prevent it. Then we will set it
  // back.
  nsIContent* parent = nsnull;
  GetParent(parent);

  nsCOMPtr<nsIAnonymousContent> anonymousParent(do_QueryInterface(parent));


  if (!anonymousParent) 
    SetParent(nsnull);
*/

  nsresult rv = mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);

  /*
  if (!anonymousParent)
    SetParent(parent);
*/
  return rv;
}


// nsIStyledContent Implementation
NS_IMETHODIMP
AnonymousElement::GetID(nsIAtom*& aResult) const
{
  /*
  nsAutoString value;
  GetAttribute(kNameSpaceID_None, kIdAtom, value);

  aResult = NS_NewAtom(value); // The NewAtom call does the AddRef.
  */

  aResult = nsnull;

  return NS_OK;
}
    
NS_IMETHODIMP
AnonymousElement::GetClasses(nsVoidArray& aArray) const
{
	return NS_OK;
}

NS_IMETHODIMP 
AnonymousElement::HasClass(nsIAtom* aClass) const
{
	return NS_COMFALSE;
}

NS_IMETHODIMP
AnonymousElement::GetContentStyleRules(nsISupportsArray* aRules)
{
  return NS_OK;
}
    
NS_IMETHODIMP
AnonymousElement::GetInlineStyleRules(nsISupportsArray* aRules)
{
  // we don't currently support the style attribute
  return NS_OK;
}

NS_IMETHODIMP
AnonymousElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, 
                                         PRInt32& aHint) const
{
  aHint = NS_STYLE_HINT_CONTENT;  // we never map attribtes to style
  return NS_OK;
}

NS_IMETHODIMP 
AnonymousElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{         
  if (aIID.Equals(kIStyledContentIID)) {
    nsIStyledContent* tmp = this;
    *aInstancePtr = (void*) tmp;
    return NS_OK;
  } else if (aIID.Equals(kIAnonymousContentIID)) {
    nsIAnonymousContent* tmp = this;
    *aInstancePtr = (void*) tmp;
    return NS_OK;
  }

  return nsXMLElement::QueryInterface(aIID, aInstancePtr);                                                                                                                                                       
}


nsresult NS_CreateAnonymousNode(nsIContent* aParent, nsIAtom* aTag, PRInt32 aNameSpaceId, nsCOMPtr<nsIContent>& aNewNode)
{

    // get the document
    nsIDocument* document = nsnull;
    aParent->GetDocument(document);
    
    // create the xml element
    nsCOMPtr<nsIXMLContent> content;
    //NS_NewXMLElement(getter_AddRefs(content), aTag);
    content = new AnonymousElement(aTag);

    content->SetNameSpaceID(aNameSpaceId);
    
    // set the document
    content->SetDocument(document, PR_TRUE);
    
    aNewNode = content;

  /*
    nsCOMPtr<nsIDocument> document;
    aParent->GetDocument(*getter_AddRefs(document));

    nsCOMPtr<nsIDOMDocument> domDocument(do_QueryInterface(document));
    nsCOMPtr<nsIDOMElement> element;
    nsString name;
    aTag->ToString(name);
    domDocument->CreateElement(name, getter_AddRefs(element));
    aNewNode = do_QueryInterface(element);
*/

    return NS_OK;
}


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
 
    nsCOMPtr<nsIContent> content;
    NS_CreateAnonymousNode(mContent, nsXULAtoms::scrollbarbutton, nsXULAtoms::nameSpaceID, content);
    content->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::type, "decrement", PR_FALSE);
    aAnonymousChildren.AppendElement(content);

    NS_CreateAnonymousNode(mContent, nsXULAtoms::slider, nsXULAtoms::nameSpaceID, content);
    content->SetAttribute(kNameSpaceID_None, nsXULAtoms::flex, "100%", PR_FALSE);
    aAnonymousChildren.AppendElement(content);

    NS_CreateAnonymousNode(mContent, nsXULAtoms::scrollbarbutton, nsXULAtoms::nameSpaceID, content);
    content->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::type, "increment", PR_FALSE);
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
  if (       aAttribute == nsXULAtoms::curpos || 
             aAttribute == nsXULAtoms::maxpos || 
             aAttribute == nsXULAtoms::pageincrement ||
             aAttribute == nsXULAtoms::increment) {
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

  if (aIID.Equals(kIStyledContentIID)) {                                         
    *aInstancePtr = (void*)(nsIStyledContent*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }

  return nsBoxFrame::QueryInterface(aIID, aInstancePtr);                                     
}

NS_IMETHODIMP
nsScrollbarFrame::HandlePress(nsIPresContext& aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus&  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsScrollbarFrame::HandleMultiplePress(nsIPresContext& aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus&  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsScrollbarFrame::HandleDrag(nsIPresContext& aPresContext, 
                              nsGUIEvent*     aEvent,
                              nsEventStatus&  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsScrollbarFrame::HandleRelease(nsIPresContext& aPresContext, 
                                 nsGUIEvent*     aEvent,
                                 nsEventStatus&  aEventStatus)
{
  return NS_OK;
}

