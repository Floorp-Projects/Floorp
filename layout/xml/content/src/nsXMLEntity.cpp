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


#include "nsIDOMEntity.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsGenericDOMDataNode.h"
#include "nsGenericElement.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsLayoutAtoms.h"
#include "nsString.h"
#include "nsIXMLContent.h"

static NS_DEFINE_IID(kIDOMEntityIID, NS_IDOMENTITY_IID);

class nsXMLEntity : public nsIDOMEntity,
                    public nsIScriptObjectOwner,
                    public nsIDOMEventReceiver,
                    public nsIContent
{
public:
  nsXMLEntity(const nsString& aName, const nsString& aPublicId,
              const nsString& aSystemId, const nsString aNotationName);
  virtual ~nsXMLEntity();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMEntity
  NS_IMETHOD    GetPublicId(nsString& aPublicId);
  NS_IMETHOD    GetSystemId(nsString& aSystemId);
  NS_IMETHOD    GetNotationName(nsString& aNotationName);

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
  nsString mPublicId;
  nsString mSystemId;
  nsString mNotationName;
  void* mScriptObject;
};

nsresult
NS_NewXMLEntity(nsIContent** aInstancePtrResult,
                const nsString& aName,
                const nsString& aPublicId,
                const nsString& aSystemId,
                const nsString aNotationName)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIContent* it = new nsXMLEntity(aName, aPublicId, aSystemId, aNotationName);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIContentIID, (void **) aInstancePtrResult);
}

nsXMLEntity::nsXMLEntity(const nsString& aName,
                         const nsString& aPublicId,
                         const nsString& aSystemId,
                         const nsString aNotationName) :
  mName(aName), mPublicId(aPublicId), mSystemId(aSystemId), mNotationName(aNotationName)
{
  NS_INIT_REFCNT();
  mInner.Init(this);
  mScriptObject = nsnull;
}

nsXMLEntity::~nsXMLEntity()
{
}

NS_IMPL_ADDREF(nsXMLEntity)
NS_IMPL_RELEASE(nsXMLEntity)

nsresult 
nsXMLEntity::QueryInterface(REFNSIID aIID, void** aInstancePtrResult)
{
  if (NULL == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(kISupportsIID)) {                          
    nsIDOMEntity* tmp = this;                                
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
  if (aIID.Equals(kIDOMEntityIID)) {
    nsIDOMEntity* tmp = this;
    *aInstancePtrResult = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP    
nsXMLEntity::GetPublicId(nsString& aPublicId)
{
  aPublicId=mPublicId;

  return NS_OK;
}

NS_IMETHODIMP    
nsXMLEntity::GetSystemId(nsString& aSystemId)
{
  aSystemId=mSystemId;

  return NS_OK;
}

NS_IMETHODIMP    
nsXMLEntity::GetNotationName(nsString& aNotationName)
{
  aNotationName=mNotationName;

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLEntity::GetScriptObject(nsIScriptContext* aContext, 
                             void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIDOMScriptObjectFactory *factory;
    
    res = nsGenericElement::GetScriptObjectFactory(&factory);
    if (NS_OK != res) {
      return res;
    }
    
    res = factory->NewScriptEntity(aContext, 
                                   (nsISupports*)(nsIDOMEntity*)this,
                                   mInner.mParent, 
                                   (void**)&mScriptObject);

    NS_RELEASE(factory);
  }
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP 
nsXMLEntity::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}


NS_IMETHODIMP 
nsXMLEntity::GetTag(nsIAtom*& aResult) const
{
//  aResult = nsLayoutAtoms::EntityTagName;
//  NS_ADDREF(aResult);

  aResult = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsXMLEntity::GetNodeName(nsString& aNodeName)
{
  aNodeName=mName;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLEntity::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::ENTITY_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLEntity::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsXMLEntity* it = new nsXMLEntity(mName,
                                    mSystemId,
                                    mPublicId,
                                    mNotationName);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsXMLEntity::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mInner.mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Entity refcount=%d<!ENTITY ", mRefCnt);

  nsAutoString tmp(mName);
  if (mPublicId.Length()) {
    tmp.Append(" PUBLIC \"");
    tmp.Append(mPublicId);
    tmp.Append("\"");
  }

  if (mSystemId.Length()) {
    tmp.Append(" SYSTEM \"");
    tmp.Append(mSystemId);
    tmp.Append("\"");
  }

  if (mNotationName.Length()) {
    tmp.Append(" NDATA ");
    tmp.Append(mNotationName);
  }

  fputs(tmp, out);

  fputs(">\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLEntity::HandleDOMEvent(nsIPresContext& aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus)
{
  // We should never be getting events
  NS_ASSERTION(0, "event handler called for entity");
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsXMLEntity::GetContentID(PRUint32* aID) 
{
  *aID = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXMLEntity::SetContentID(PRUint32 aID) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}