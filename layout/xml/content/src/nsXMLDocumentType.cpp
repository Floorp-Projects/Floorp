/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
#include "nsISizeOfHandler.h"
#include "nsDOMAttributeMap.h"


class nsXMLDocumentType : public nsIDOMDocumentType,
                          public nsIScriptObjectOwner,
                          public nsIContent
{
public:
  nsXMLDocumentType(const nsAReadableString& aName,
                    nsIDOMNamedNodeMap *aEntities,
                    nsIDOMNamedNodeMap *aNotations,
                    const nsAReadableString& aPublicId,
                    const nsAReadableString& aSystemId,
                    const nsAReadableString& aInternalSubset);

  virtual ~nsXMLDocumentType();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMDocumentType
  NS_DECL_IDOMDOCUMENTTYPE

  // nsIScriptObjectOwner interface
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC_DOM_DATA(mInner)

  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

protected:
  // XXX Processing instructions are currently implemented by using
  // the generic CharacterData inner object, even though PIs are not
  // character data. This is done simply for convenience and should
  // be changed if this restricts what should be done for character data.
  nsGenericDOMDataNode mInner;
  nsString mName;
  nsIDOMNamedNodeMap* mEntities;
  nsIDOMNamedNodeMap* mNotations;
  nsString mPublicId;
  nsString mSystemId;
  nsString mInternalSubset;
  void* mScriptObject;
};

nsresult
NS_NewXMLDocumentType(nsIContent** aInstancePtrResult,
                      const nsAReadableString& aName,
                      nsIDOMNamedNodeMap *aEntities,
                      nsIDOMNamedNodeMap *aNotations,
                      const nsAReadableString& aPublicId,
                      const nsAReadableString& aSystemId,
                      const nsAReadableString& aInternalSubset)

{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIContent* it = new nsXMLDocumentType(aName, aEntities, aNotations,
                                         aPublicId, aSystemId, aInternalSubset);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIContent), (void **) aInstancePtrResult);
}

nsXMLDocumentType::nsXMLDocumentType(const nsAReadableString& aName,
                                     nsIDOMNamedNodeMap *aEntities,
                                     nsIDOMNamedNodeMap *aNotations,
                                     const nsAReadableString& aPublicId,
                                     const nsAReadableString& aSystemId,
                                     const nsAReadableString& aInternalSubset) :
  mName(aName),
  mPublicId(aPublicId),
  mSystemId(aSystemId),
  mInternalSubset(aInternalSubset)
{
  NS_INIT_REFCNT();
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

  if (aIID.Equals(NS_GET_IID(nsISupports))) {                          
    nsIDOMDocumentType* tmp = this;                                
    nsISupports* tmp2 = tmp;                                
    *aInstancePtrResult = (void*) tmp2;                                  
    NS_ADDREF_THIS();                                       
    return NS_OK;                                           
  }                                                         
  if (aIID.Equals(NS_GET_IID(nsIDOMNode))) {                           
    nsIDOMNode* tmp = this;                                
    *aInstancePtrResult = (void*) tmp;                                   
    NS_ADDREF_THIS();                                       
    return NS_OK;                                           
  }                                                         
  if (aIID.Equals(NS_GET_IID(nsIDOMEventReceiver))) {                  
    nsCOMPtr<nsIEventListenerManager> man;
    if (NS_SUCCEEDED(mInner.GetListenerManager(this, getter_AddRefs(man)))){
      return man->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), (void**)aInstancePtrResult);
    }     
    return NS_NOINTERFACE;
  }                                                         
  if (aIID.Equals(NS_GET_IID(nsIScriptObjectOwner))) {                 
    nsIScriptObjectOwner* tmp = this;                      
    *aInstancePtrResult = (void*) tmp;                                   
    NS_ADDREF_THIS();                                       
    return NS_OK;                                           
  }                                                         
  if (aIID.Equals(NS_GET_IID(nsIContent))) {                           
    nsIContent* tmp = this;                                
    *aInstancePtrResult = (void*) tmp;                                   
    NS_ADDREF_THIS();                                       
    return NS_OK;                                           
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMDocumentType))) {
    nsIDOMDocumentType* tmp = this;
    *aInstancePtrResult = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP    
nsXMLDocumentType::GetName(nsAWritableString& aName)
{
  aName.Assign(mName);

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
nsXMLDocumentType::GetPublicId(nsAWritableString& aPublicId)
{
  aPublicId.Assign(mPublicId);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLDocumentType::GetSystemId(nsAWritableString& aSystemId)
{
  aSystemId.Assign(mSystemId);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLDocumentType::GetInternalSubset(nsAWritableString& aInternalSubset)
{
  aInternalSubset.Assign(mInternalSubset);

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
nsXMLDocumentType::GetNodeInfo(nsINodeInfo*& aResult) const
{
  aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLDocumentType::GetNodeName(nsAWritableString& aNodeName)
{
  aNodeName.Assign(mName);
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
                                                mNotations,
                                                mPublicId,
                                                mSystemId,
                                                mInternalSubset);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
}

NS_IMETHODIMP
nsXMLDocumentType::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mInner.mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Document type refcount: %d\n", mRefCnt);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLDocumentType::DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const
{
  return NS_OK;
}

NS_IMETHODIMP
nsXMLDocumentType::HandleDOMEvent(nsIPresContext* aPresContext,
                                           nsEvent* aEvent,
                                           nsIDOMEvent** aDOMEvent,
                                           PRUint32 aFlags,
                                           nsEventStatus* aEventStatus)
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

NS_IMETHODIMP
nsXMLDocumentType::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
#ifdef DEBUG
  PRUint32 sum;
  mInner.SizeOf(aSizer, &sum, sizeof(*this));
  PRUint32 ssize;
  mName.SizeOf(aSizer, &ssize);
  sum = sum - sizeof(mName) + ssize;
  if (mEntities) {
    PRBool recorded;
    aSizer->RecordObject((void*) mEntities, &recorded);
    if (!recorded) {
      PRUint32 size;
      nsDOMAttributeMap::SizeOfNamedNodeMap(mEntities, aSizer, &size);
      aSizer->AddSize(nsLayoutAtoms::xml_document_entities, size);
    }
  }
  if (mNotations) {
    PRBool recorded;
    aSizer->RecordObject((void*) mNotations, &recorded);
    if (!recorded) {
      PRUint32 size;
      nsDOMAttributeMap::SizeOfNamedNodeMap(mNotations, aSizer, &size);
      aSizer->AddSize(nsLayoutAtoms::xml_document_notations, size);
    }
  }
#endif
  return NS_OK;
}
