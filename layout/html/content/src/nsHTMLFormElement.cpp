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
#include "nsCOMPtr.h"
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsIFormManager.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNSHTMLFormElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"   
#include "nsIFrame.h"

static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFormIID, NS_IFORM_IID);
static NS_DEFINE_IID(kIFormManagerIID, NS_IFORMMANAGER_IID);
static NS_DEFINE_IID(kIDOMNSHTMLFormElementIID, NS_IDOMNSHTMLFORMELEMENT_IID);

class nsFormControlList;

// nsHTMLFormElement

class nsHTMLFormElement : public nsIDOMHTMLFormElement,
                          public nsIDOMNSHTMLFormElement,
                          public nsIScriptObjectOwner,
                          public nsIDOMEventReceiver,
                          public nsIHTMLContent,
                          public nsIForm
{
public:
  nsHTMLFormElement(nsIAtom* aTag);
  virtual ~nsHTMLFormElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLFormElement
  NS_IMETHOD GetElements(nsIDOMHTMLCollection** aElements);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD SetName(const nsString& aName);
  NS_IMETHOD GetAcceptCharset(nsString& aAcceptCharset);
  NS_IMETHOD SetAcceptCharset(const nsString& aAcceptCharset);
  NS_IMETHOD GetAction(nsString& aAction);
  NS_IMETHOD SetAction(const nsString& aAction);
  NS_IMETHOD GetEnctype(nsString& aEnctype);
  NS_IMETHOD SetEnctype(const nsString& aEnctype);
  NS_IMETHOD GetMethod(nsString& aMethod);
  NS_IMETHOD SetMethod(const nsString& aMethod);
  NS_IMETHOD GetTarget(nsString& aTarget);
  NS_IMETHOD SetTarget(const nsString& aTarget);
  NS_IMETHOD GetLength(PRUint32* aLength);
  NS_IMETHOD Reset();
  NS_IMETHOD Submit();

  // nsIDOMNSHTMLFormElement
  NS_IMETHOD    GetEncoding(nsString& aEncoding);
  NS_IMETHOD    NamedItem(const nsString& aName, nsIDOMElement** aReturn);
  
  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

  // nsIForm
  NS_IMETHOD AddElement(nsIFormControl* aElement);
  NS_IMETHOD GetElementAt(PRInt32 aIndex, nsIFormControl** aElement) const;
  NS_IMETHOD GetElementCount(PRUint32* aCount) const;
  NS_IMETHOD RemoveElement(nsIFormControl* aElement, PRBool aChildIsRef = PR_TRUE);

protected:
  nsFormControlList*       mControls;
  nsGenericHTMLLeafElement mInner;
};

// nsFormControlList
class nsFormControlList : public nsIDOMHTMLCollection, public nsIScriptObjectOwner {
public:
  nsFormControlList();
  virtual ~nsFormControlList();

  void Clear();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);
  NS_IMETHOD ResetScriptObject();

  // nsIDOMHTMLCollection interface
  NS_DECL_IDOMHTMLCOLLECTION
  
  // Called to tell us that the form is going away and that we
  // should drop our (non ref-counted) reference to it
  void ReleaseForm();

  void        *mScriptObject;
  nsVoidArray  mElements;
};

// nsHTMLFormElement implementation

// construction, destruction
nsresult
NS_NewHTMLFormElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLFormElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLFormElement::nsHTMLFormElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
  mControls = new nsFormControlList();
  NS_ADDREF(mControls);
nsTraceRefcnt::Create((nsIForm*)this, "nsHTMLFormElement", __FILE__, __LINE__);
}

nsHTMLFormElement::~nsHTMLFormElement()
{
  // set the controls to have no form
  PRUint32 numControls;
  GetElementCount(&numControls);
  for (PRUint32 i = 0; i < numControls; i++) {
    // avoid addref to child
    nsIFormControl* control = (nsIFormControl*)mControls->mElements.ElementAt(i); 
    if (control) {
      // it is assummed that passing in nsnull will not release formControl's previous form
      control->SetForm(nsnull); 
    }
  }

  NS_RELEASE(mControls);

nsTraceRefcnt::Destroy((nsIForm*)this, __FILE__, __LINE__);
}

// nsISupports
NS_IMETHODIMP
nsHTMLFormElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIFormIID)) {
    *aInstancePtr = (void*)(nsIForm*)this;
    AddRef();
    return NS_OK;
  } 
  else if (aIID.Equals(kIDOMHTMLFormElementIID)) {
    *aInstancePtr = (void*)(nsIDOMHTMLFormElement*)this;
    AddRef();
    return NS_OK;
  } 
  else if (aIID.Equals(kIDOMNSHTMLFormElementIID)) {
    *aInstancePtr = (void*)(nsIDOMNSHTMLFormElement*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsHTMLFormElement::AddRef(void)                                
{ 
  nsTraceRefcnt::AddRef((nsIForm*)this, mRefCnt+1, __FILE__, __LINE__);
  return ++mRefCnt;                                          
}

NS_IMETHODIMP_(nsrefcnt)
nsHTMLFormElement::Release()
{
  nsTraceRefcnt::Release((nsIForm*)this, mRefCnt-1, __FILE__, __LINE__);
  --mRefCnt;
  PRUint32 numChildren;
  GetElementCount(&numChildren);
  if (mRefCnt == nsrefcnt(numChildren)) {
    mRefCnt = 0;
    delete this; 
    return 0;
  } 
  return mRefCnt;
}

// nsIDOMHTMLFormElement
nsresult
nsHTMLFormElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLFormElement* it = new nsHTMLFormElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLFormElement::GetElements(nsIDOMHTMLCollection** aElements)
{
  *aElements = mControls;
  NS_ADDREF(mControls);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::GetName(nsString& aValue)
{
  return mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::name, aValue);
}

NS_IMETHODIMP
nsHTMLFormElement::SetName(const nsString& aValue)
{
  return mInner.SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::name, aValue, PR_TRUE);
}

NS_IMPL_STRING_ATTR(nsHTMLFormElement, AcceptCharset, acceptcharset)
NS_IMPL_STRING_ATTR(nsHTMLFormElement, Action, action)
NS_IMPL_STRING_ATTR(nsHTMLFormElement, Enctype, enctype)
NS_IMPL_STRING_ATTR(nsHTMLFormElement, Method, method)
NS_IMPL_STRING_ATTR(nsHTMLFormElement, Target, target)

NS_IMETHODIMP
nsHTMLFormElement::Submit()
{
  // XXX Need to do something special with mailto: or news: URLs
  nsIDocument* doc = nsnull;
  nsresult result = GetDocument(doc);
  if ((NS_OK == result) && doc) {
    nsIPresShell *shell = doc->GetShellAt(0);
    if (nsnull != shell) {
      nsIFrame* frame;
      shell->GetPrimaryFrameFor(this, &frame);
      if (frame) {
        nsIFormManager* formMan = nsnull;
        nsresult result = frame->QueryInterface(kIFormManagerIID, (void**)&formMan);
        if ((NS_OK == result) && formMan) {
          nsCOMPtr<nsIPresContext> context;
          shell->GetPresContext(getter_AddRefs(context));
          if (context) {
            // XXX We're currently passing in null for the frame.
            // It works for now, but might not always
            // be correct. In the future, we might not need the 
            // frame to be passed to the link handler.
            result = formMan->OnSubmit(context, nsnull);
          }
        }
      }
      NS_RELEASE(shell);
    }
    NS_RELEASE(doc);
  }

  return result;
}

NS_IMETHODIMP
nsHTMLFormElement::Reset()
{
  // XXX Need to do something special with mailto: or news: URLs
  nsIDocument* doc = nsnull;
  nsresult result = GetDocument(doc);
  if ((NS_OK == result) && doc) {
    nsIPresShell *shell = doc->GetShellAt(0);
    if (nsnull != shell) {
      nsIFrame* frame;
      shell->GetPrimaryFrameFor(this, &frame);
      if (frame) {
        nsIFormManager* formMan = nsnull;
        nsresult result = frame->QueryInterface(kIFormManagerIID, (void**)&formMan);
        if ((NS_OK == result) && formMan) {
          result = formMan->OnReset();
        }
      }
      NS_RELEASE(shell);
    }
  }

  return result;
}
static nsGenericHTMLElement::EnumTable kFormMethodTable[] = {
  { "get", NS_FORM_METHOD_GET },
  { "post", NS_FORM_METHOD_POST },
  { 0 }
};
static nsGenericHTMLElement::EnumTable kFormEnctypeTable[] = {
  { "multipart/form-data", NS_FORM_ENCTYPE_MULTIPART },
  { "application/x-www-form-urlencoded", NS_FORM_ENCTYPE_URLENCODED },
  { 0 }
};

NS_IMETHODIMP
nsHTMLFormElement::StringToAttribute(nsIAtom* aAttribute,
                              const nsString& aValue,
                              nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::method) {
    nsGenericHTMLElement::ParseEnumValue(aValue, kFormMethodTable, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::enctype) {
    nsGenericHTMLElement::ParseEnumValue(aValue, kFormEnctypeTable, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLFormElement::AttributeToString(nsIAtom* aAttribute,
                              const nsHTMLValue& aValue,
                              nsString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::method) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      nsGenericHTMLElement::EnumValueToString(aValue, kFormMethodTable, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::enctype) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      nsGenericHTMLElement::EnumValueToString(aValue, kFormEnctypeTable, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  // XXX write me
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLFormElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}



NS_IMETHODIMP
nsHTMLFormElement::HandleDOMEvent(nsIPresContext& aPresContext,
                           nsEvent* aEvent,
                           nsIDOMEvent** aDOMEvent,
                           PRUint32 aFlags,
                           nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

// nsIForm

NS_IMETHODIMP
nsHTMLFormElement::GetElementCount(PRUint32* aCount) const 
{
  mControls->GetLength(aCount); 
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFormElement::GetElementAt(PRInt32 aIndex, nsIFormControl** aFormControl) const 
{ 
  *aFormControl = (nsIFormControl*) mControls->mElements.ElementAt(aIndex);
  NS_IF_ADDREF(*aFormControl);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::AddElement(nsIFormControl* aChild) 
{ 
  PRBool rv = mControls->mElements.AppendElement(aChild);
  if (rv) {
    NS_ADDREF(aChild);
  }
  return rv;
}

NS_IMETHODIMP 
nsHTMLFormElement::RemoveElement(nsIFormControl* aChild, PRBool aChildIsRef) 
{ 
  PRBool rv = mControls->mElements.RemoveElement(aChild);
  if (rv && aChildIsRef) {
    NS_RELEASE(aChild);
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLFormElement::GetEncoding(nsString& aEncoding)
{
  return mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::encoding, aEncoding);
}
 
NS_IMETHODIMP    
nsHTMLFormElement::GetLength(PRUint32* aLength)
{
  *aLength = mControls->mElements.Count();
  
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLFormElement::NamedItem(const nsString& aName, nsIDOMElement** aReturn)
{
  // XXX For now we just search our element list. In reality, we'll have
  // to look at all of our children, including images, objects, etc.
  nsIDOMNode *node;
  nsresult result = mControls->NamedItem(aName, &node);
  
  if ((NS_OK == result) && (nsnull != node)) {
    result = node->QueryInterface(kIDOMElementIID, (void **)aReturn);
    NS_RELEASE(node);
  }
  else {
    *aReturn = nsnull;
  }

  return result;
}


// nsFormControlList implementation, this could go away if there were a lightweight collection implementation somewhere

nsFormControlList::nsFormControlList() 
{
  mRefCnt = 0;
  mScriptObject = nsnull;
}

nsFormControlList::~nsFormControlList()
{
  Clear();
}

NS_IMETHODIMP
nsFormControlList::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

void
nsFormControlList::Clear()
{
  PRUint32 numElements = mElements.Count();
  for (PRUint32 i = 0; i < numElements; i++) {
    nsIFormControl* elem = (nsIFormControl*) mElements.ElementAt(i);
    NS_IF_RELEASE(elem);
  }
}

nsresult nsFormControlList::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kIDOMHTMLCollectionIID, NS_IDOMHTMLCOLLECTION_IID);
  static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
  if (aIID.Equals(kIDOMHTMLCollectionIID)) {
    *aInstancePtr = (void*)(nsIDOMHTMLCollection*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtr = (void*)(nsIScriptObjectOwner*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIDOMHTMLCollection*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsFormControlList)
NS_IMPL_RELEASE(nsFormControlList)


nsresult nsFormControlList::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptHTMLCollection(aContext, (nsISupports *)(nsIDOMHTMLCollection *)this, nsnull, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult nsFormControlList::ResetScriptObject()
{
  mScriptObject = nsnull;
  return NS_OK;
}

// nsIDOMHTMLCollection interface
NS_IMETHODIMP    
nsFormControlList::GetLength(PRUint32* aLength)
{
  *aLength = mElements.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsFormControlList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsIFormControl *control = (nsIFormControl*)mElements.ElementAt(aIndex);
  if (control) {
    return control->QueryInterface(kIDOMNodeIID, (void**)aReturn); // keep the ref
  }
  *aReturn = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsFormControlList::NamedItem(const nsString& aName, nsIDOMNode** aReturn)
{
  PRUint32 count = mElements.Count();
  nsresult result = NS_OK;

  *aReturn = nsnull;
  for (PRUint32 i = 0; i < count && *aReturn == nsnull; i++) {
    nsIFormControl *control = (nsIFormControl*)mElements.ElementAt(i);
    if (nsnull != control) {
      nsIContent *content;
      
      result = control->QueryInterface(kIContentIID, (void **)&content);
      if (NS_OK == result) {
        nsAutoString name;
        // XXX Should it be an EqualsIgnoreCase?
        if (((content->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::name, name) == NS_CONTENT_ATTR_HAS_VALUE) &&
             (aName.Equals(name))) ||
            ((content->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::id, name) == NS_CONTENT_ATTR_HAS_VALUE) &&
             (aName.Equals(name)))) {
          result = control->QueryInterface(kIDOMNodeIID, (void **)aReturn);
        }
        NS_RELEASE(content);
      }
    }
  }
  
  return result;
}

NS_IMETHODIMP
nsHTMLFormElement::GetStyleHintForAttributeChange(
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const
{
  nsGenericHTMLElement::GetStyleHintForCommonAttributes(this, aAttribute, aHint);
  return NS_OK;
}
