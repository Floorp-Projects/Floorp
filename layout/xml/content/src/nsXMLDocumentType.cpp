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


#include "nsIDOMDocumentType.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsGenericDOMDataNode.h"
#include "nsGenericElement.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsLayoutAtoms.h"
#include "nsString.h"
#include "nsIXMLContent.h"

static NS_DEFINE_IID(kIDOMDocumentTypeIID, NS_IDOMDOCUMENTTYPE_IID);

class nsXMLDocumentType : public nsIDOMDocumentType,
                          public nsIScriptObjectOwner,
                          public nsIDOMEventReceiver,
                          public nsIContent
{
public:
  nsXMLDocumentType(const nsString& aTarget,
                    nsIDOMNamedNodeMap *aEntities,
                    nsIDOMNamedNodeMap *aNotations);
  virtual ~nsXMLDocumentType();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMDocumentType
  NS_IMETHOD    GetName(nsString& aName);
  NS_IMETHOD    GetEntities(nsIDOMNamedNodeMap** aEntities);
  NS_IMETHOD    GetNotations(nsIDOMNamedNodeMap** aNotations);

  // nsIScriptObjectOwner interface
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC_DOM_DATA(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC_DOM_DATA(mInner)

protected:
  // XXX Processing instructions are currently implemented by using
  // the generic CharacterData inner object, even though PIs are not
  // character data. This is done simply for convenience and should
  // be changed if this restricts what should be done for character data.
  nsGenericDOMDataNode mInner;
  nsString mName;
  nsIDOMNamedNodeMap* mEntities;
  nsIDOMNamedNodeMap* mNotations;
  void* mScriptObject;
};

nsresult
NS_NewXMLDocumentType(nsIContent** aInstancePtrResult,
                      const nsString& aName,
                      nsIDOMNamedNodeMap *aEntities,
                      nsIDOMNamedNodeMap *aNotations)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIContent* it = new nsXMLDocumentType(aName, aEntities, aNotations);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIContentIID, (void **) aInstancePtrResult);
}

nsXMLDocumentType::nsXMLDocumentType(const nsString& aName,
                                     nsIDOMNamedNodeMap *aEntities,
                                     nsIDOMNamedNodeMap *aNotations) :
  mName(aName)
{
  NS_INIT_REFCNT();
  mInner.Init(this);
  mScriptObject = nsnull;

  mEntities = aEntities;
  mNotations = aNotations;

  NS_IF_ADDREF(mEntities);
  NS_IF_ADDREF(mNotations);
}

nsXMLDocumentType::~nsXMLDocumentType()
{
  NS_IF_RELEASE(mEntities);
  NS_IF_RELEASE(mNotations);
}

NS_IMPL_ADDREF(nsXMLDocumentType)
NS_IMPL_RELEASE(nsXMLDocumentType)

nsresult 
nsXMLDocumentType::QueryInterface(REFNSIID aIID, void** aInstancePtrResult)
{
  if (NULL == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(kISupportsIID)) {                          
    nsIDOMDocumentType* tmp = this;                                
    nsISupports* tmp2 = tmp;                                
    *aInstancePtrResult = (void*) tmp2;                                  
    NS_ADDREF_THIS();                                       
    return NS_OK;                                           
  }                                                         
  if (aIID.Equals(kIDOMNodeIID)) {                           
    nsIDOMNode* tmp = this;                                
    *aInstancePtrResult = (void*) tmp;                                   
    NS_ADDREF_THIS();                                       
    return NS_OK;                                           
  }                                                         
  if (aIID.Equals(kIDOMEventReceiverIID)) {                  
    nsIDOMEventReceiver* tmp = this;                       
    *aInstancePtrResult = (void*) tmp;                                   
    NS_ADDREF_THIS();                                       
    return NS_OK;                                           
  }                                                         
  if (aIID.Equals(kIScriptObjectOwnerIID)) {                 
    nsIScriptObjectOwner* tmp = this;                      
    *aInstancePtrResult = (void*) tmp;                                   
    NS_ADDREF_THIS();                                       
    return NS_OK;                                           
  }                                                         
  if (aIID.Equals(kIContentIID)) {                           
    nsIContent* tmp = this;                                
    *aInstancePtrResult = (void*) tmp;                                   
    NS_ADDREF_THIS();                                       
    return NS_OK;                                           
  }
  if (aIID.Equals(kIDOMDocumentTypeIID)) {
    nsIDOMDocumentType* tmp = this;
    *aInstancePtrResult = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP    
nsXMLDocumentType::GetName(nsString& aName)
{
  aName=mName;

  return NS_OK;
}

NS_IMETHODIMP    
nsXMLDocumentType::GetEntities(nsIDOMNamedNodeMap** aEntities)
{
  if (!aEntities)
    return NS_ERROR_FAILURE;

  *aEntities = mEntities;

  NS_IF_ADDREF(*aEntities);

  return NS_OK;
}

NS_IMETHODIMP    
nsXMLDocumentType::GetNotations(nsIDOMNamedNodeMap** aNotations)
{
  if (!aNotations)
    return NS_ERROR_FAILURE;

  *aNotations = mNotations;

  NS_IF_ADDREF(*aNotations);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocumentType::GetScriptObject(nsIScriptContext* aContext, 
                                            void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIDOMScriptObjectFactory *factory;
    
    res = nsGenericElement::GetScriptObjectFactory(&factory);
    if (NS_OK != res) {
      return res;
    }
    
    res = factory->NewScriptDocumentType(aContext, 
                                         (nsISupports*)(nsIDOMDocumentType*)this,
                                         mInner.mParent, 
                                         (void**)&mScriptObject);

    NS_RELEASE(factory);
  }
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP 
nsXMLDocumentType::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}


NS_IMETHODIMP 
nsXMLDocumentType::GetTag(nsIAtom*& aResult) const
{
//  aResult = nsLayoutAtoms::processingInstructionTagName;
//  NS_ADDREF(aResult);

  aResult = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsXMLDocumentType::GetNodeName(nsString& aNodeName)
{
  aNodeName=mName;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLDocumentType::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::DOCUMENT_TYPE_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLDocumentType::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsString data;
  mInner.GetData(data);

  nsXMLDocumentType* it = new nsXMLDocumentType(mName,
                                                mEntities,
                                                mNotations);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsXMLDocumentType::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mInner.mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Document type...\n", mRefCnt);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLDocumentType::HandleDOMEvent(nsIPresContext& aPresContext,
                                           nsEvent* aEvent,
                                           nsIDOMEvent** aDOMEvent,
                                           PRUint32 aFlags,
                                           nsEventStatus& aEventStatus)
{
  // We should never be getting events
  NS_ASSERTION(0, "event handler called for processing instruction");
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsXMLDocumentType::GetContentID(PRUint32* aID) 
{
  *aID = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXMLDocumentType::SetContentID(PRUint32 aID) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
