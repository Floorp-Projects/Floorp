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


#include "nsIDOMProcessingInstruction.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsGenericDOMDataNode.h"
#include "nsGenericElement.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsLayoutAtoms.h"
#include "nsString.h"
#include "nsIXMLContent.h"

static NS_DEFINE_IID(kIDOMProcessingInstructionIID, NS_IDOMPROCESSINGINSTRUCTION_IID);

class nsXMLProcessingInstruction : public nsIDOMProcessingInstruction,
                                   public nsIScriptObjectOwner,
                                   public nsIDOMEventReceiver,
                                   public nsIContent
{
public:
  nsXMLProcessingInstruction(const nsString& aTarget, const nsString& aData);
  virtual ~nsXMLProcessingInstruction();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMProcessingInstruction
  NS_IMETHOD    GetTarget(nsString& aTarget);
  NS_IMETHOD    GetData(nsString& aData);
  NS_IMETHOD    SetData(const nsString& aData);

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
  nsString mTarget;
  void* mScriptObject;
};

nsresult
NS_NewXMLProcessingInstruction(nsIContent** aInstancePtrResult,
                               const nsString& aTarget,
                               const nsString& aData)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIContent* it = new nsXMLProcessingInstruction(aTarget, aData);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIContentIID, (void **) aInstancePtrResult);
}

nsXMLProcessingInstruction::nsXMLProcessingInstruction(const nsString& aTarget,
                                                       const nsString& aData) :
  mTarget(aTarget)
{
  NS_INIT_REFCNT();
  mInner.Init(this);
  mInner.SetData(aData);
  mScriptObject = nsnull;
}

nsXMLProcessingInstruction::~nsXMLProcessingInstruction()
{
}

NS_IMPL_ADDREF(nsXMLProcessingInstruction)
NS_IMPL_RELEASE(nsXMLProcessingInstruction)

nsresult 
nsXMLProcessingInstruction::QueryInterface(REFNSIID aIID, void** aInstancePtrResult)
{
  if (NULL == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(kISupportsIID)) {                          
    nsIDOMProcessingInstruction* tmp = this;                                
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
  if (aIID.Equals(kIDOMProcessingInstructionIID)) {
    nsIDOMProcessingInstruction* tmp = this;
    *aInstancePtrResult = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP    
nsXMLProcessingInstruction::GetTarget(nsString& aTarget)
{
  aTarget.SetString(mTarget);

  return NS_OK;
}

NS_IMETHODIMP    
nsXMLProcessingInstruction::GetData(nsString& aData)
{
  return mInner.GetData(aData);
}

NS_IMETHODIMP    
nsXMLProcessingInstruction::SetData(const nsString& aData)
{
  // XXX Check if this is a stylesheet PI. If so, we may need
  // to parse the contents and see if anything has changed.
  return mInner.SetData(aData);
}

NS_IMETHODIMP 
nsXMLProcessingInstruction::GetScriptObject(nsIScriptContext* aContext, 
                                            void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIDOMScriptObjectFactory *factory;
    
    res = nsGenericElement::GetScriptObjectFactory(&factory);
    if (NS_OK != res) {
      return res;
    }
    
    res = factory->NewScriptProcessingInstruction(aContext, 
                                                  (nsISupports*)(nsIDOMProcessingInstruction*)this,
                                                  mInner.mParent, 
                                                  (void**)&mScriptObject);

    NS_RELEASE(factory);
  }
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP 
nsXMLProcessingInstruction::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}


NS_IMETHODIMP 
nsXMLProcessingInstruction::GetTag(nsIAtom*& aResult) const
{
  aResult = nsLayoutAtoms::processingInstructionTagName;
  NS_ADDREF(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetNodeName(nsString& aNodeName)
{
  aNodeName.SetString(mTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::PROCESSING_INSTRUCTION_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsString data;
  mInner.GetData(data);

  nsXMLProcessingInstruction* it = new nsXMLProcessingInstruction(mTarget,
                                                                  data);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsXMLProcessingInstruction::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mInner.mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Processing instruction refcount=%d<", mRefCnt);

  nsAutoString tmp;
  mInner.ToCString(tmp, 0, mInner.mText.GetLength());
  tmp.Insert(mTarget.GetUnicode(), 0);
  fputs(tmp, out);

  fputs(">\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::HandleDOMEvent(nsIPresContext& aPresContext,
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

