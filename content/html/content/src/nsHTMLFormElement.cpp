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
#include "nsCOMPtr.h"
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsIFormManager.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNSHTMLFormElement.h"
#include "nsIDOMHTMLFormControlList.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"   
#include "nsIFrame.h"
#include "nsISizeOfHandler.h"
#include "nsIScriptGlobalObject.h"
#include "nsDOMError.h"
#include "nsLayoutUtils.h"

static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFormIID, NS_IFORM_IID);
static NS_DEFINE_IID(kIFormManagerIID, NS_IFORMMANAGER_IID);
static NS_DEFINE_IID(kIDOMNSHTMLFormElementIID, NS_IDOMNSHTMLFORMELEMENT_IID);

class nsFormControlList;

// nsHTMLFormElement

class nsHTMLFormElement : public nsIDOMHTMLFormElement,
                          public nsIDOMNSHTMLFormElement,
                          public nsIJSScriptObject,
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
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMElement** aReturn);
  NS_IMETHOD    NamedItem(JSContext* cx, jsval* argv, PRUint32 argc, jsval* aReturn);
  

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

  // nsIJSScriptObject
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)
  virtual PRBool    AddProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  virtual PRBool    DeleteProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  virtual PRBool    GetProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  virtual PRBool    SetProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  virtual PRBool    EnumerateProperty(JSContext *aContext, JSObject *aObj);
  virtual PRBool    Resolve(JSContext *aContext, JSObject *aObj, jsval aID);
  virtual PRBool    Convert(JSContext *aContext, JSObject *aObj, jsval aID);
  virtual void      Finalize(JSContext *aContext, JSObject *aObj);

  // nsIForm
  NS_IMETHOD AddElement(nsIFormControl* aElement);
  NS_IMETHOD GetElementAt(PRInt32 aIndex, nsIFormControl** aElement) const;
  NS_IMETHOD GetElementCount(PRUint32* aCount) const;
  NS_IMETHOD RemoveElement(nsIFormControl* aElement, PRBool aChildIsRef = PR_TRUE);

protected:
  nsFormControlList*       mControls;
  nsGenericHTMLContainerElement mInner;
};

// nsFormControlList
class nsFormControlList : public nsIDOMHTMLFormControlList, public nsIScriptObjectOwner {
public:
  nsFormControlList(nsIDOMHTMLFormElement* aForm);
  virtual ~nsFormControlList();

  void Clear();
  void SetForm(nsIDOMHTMLFormElement* aForm);

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);
  NS_IMETHOD ResetScriptObject();

  // nsIDOMHTMLCollection interface
  NS_DECL_IDOMHTMLCOLLECTION
  NS_DECL_IDOMHTMLFORMCONTROLLIST

  nsresult      GetNamedObject(JSContext* aContext, jsval aID, JSObject** aObj);
#ifdef DEBUG
  nsresult SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

  void        *mScriptObject;
  nsVoidArray  mElements;
  nsIDOMHTMLFormElement* mForm;  // WEAK - the form owns me
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
  mControls = new nsFormControlList(this);
  NS_ADDREF(mControls);
//nsTraceRefcnt::Create((nsIForm*)this, "nsHTMLFormElement", __FILE__, __LINE__);
}

nsHTMLFormElement::~nsHTMLFormElement()
{
  // set the controls to have no form
  PRUint32 numControls;
  GetElementCount(&numControls);
  do {
    if (numControls-- == 0)
      break;
    // avoid addref to child
    nsIFormControl* control = (nsIFormControl*)mControls->mElements.ElementAt(numControls); 
    if (control) {
      // it is assummed that passing in nsnull will not release formControl's previous form
      control->SetForm(nsnull); 
    }
  } while(1);

  mControls->SetForm(nsnull);
  NS_RELEASE(mControls);

//nsTraceRefcnt::Destroy((nsIForm*)this, __FILE__, __LINE__);
}

// nsISupports
NS_IMETHODIMP
nsHTMLFormElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  // Note that this has to stay above the generic element
  // QI macro, since it overrides the nsIJSScriptObject implementation
  // from the generic element.
  if (aIID.Equals(kIJSScriptObjectIID)) {
    nsIJSScriptObject* tmp = this;
    *aInstancePtr = (void*) tmp;
    AddRef();
    return NS_OK;
  }                                                             
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

NS_IMPL_ADDREF(nsHTMLFormElement);

NS_IMETHODIMP_(nsrefcnt)
nsHTMLFormElement::Release()
{
  --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "nsHTMLFormElement");
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
  nsresult res = GetDocument(doc);
  if (NS_SUCCEEDED(res) && doc) {
    // Make sure the presentation is up-to-date
    doc->FlushPendingNotifications();

    nsIPresShell *shell = doc->GetShellAt(0);
    if (nsnull != shell) {
      nsIFrame* frame;
      shell->GetPrimaryFrameFor(this, &frame);
      if (frame) {
        nsIFormManager* formMan = nsnull;
        res = frame->QueryInterface(kIFormManagerIID, (void**)&formMan);
        if (NS_SUCCEEDED(res) && formMan) {
          nsCOMPtr<nsIPresContext> context;
          shell->GetPresContext(getter_AddRefs(context));
          if (context) {
            // XXX We're currently passing in null for the frame.
            // It works for now, but might not always
            // be correct. In the future, we might not need the 
            // frame to be passed to the link handler.
            res = formMan->OnSubmit(context, nsnull);
          }
        }
      }
      NS_RELEASE(shell);
    }
    NS_RELEASE(doc);
  }

  return res;
}

NS_IMETHODIMP
nsHTMLFormElement::Reset()
{
  // XXX Need to do something special with mailto: or news: URLs
  nsIDocument* doc = nsnull; // Strong
  nsresult res = GetDocument(doc);
  if (NS_SUCCEEDED(res) && doc) {
    // Make sure the presentation is up-to-date
    doc->FlushPendingNotifications();

    nsIPresShell *shell = doc->GetShellAt(0); // Strong
    if (nsnull != shell) {
      nsIFrame* frame;
      shell->GetPrimaryFrameFor(this, &frame);
      if (frame) {
        nsIFormManager* formMan = nsnull;
        res = frame->QueryInterface(kIFormManagerIID, (void**)&formMan);
        if (NS_SUCCEEDED(res) && formMan) {
          nsCOMPtr<nsIPresContext> presContext;
          shell->GetPresContext(getter_AddRefs(presContext));
          
          res = formMan->OnReset(presContext);
        }
      }
      NS_RELEASE(shell);
    }
    NS_RELEASE(doc);
  }

  return res;
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
    if (nsGenericHTMLElement::ParseEnumValue(aValue, kFormMethodTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::enctype) {
    if (nsGenericHTMLElement::ParseEnumValue(aValue, kFormEnctypeTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
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
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  // XXX write me
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLFormElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                            PRInt32& aHint) const
{
  if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
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
nsHTMLFormElement::HandleDOMEvent(nsIPresContext* aPresContext,
                           nsEvent* aEvent,
                           nsIDOMEvent** aDOMEvent,
                           PRUint32 aFlags,
                           nsEventStatus* aEventStatus)
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
  return mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::enctype, aEncoding);
}
 
NS_IMETHODIMP    
nsHTMLFormElement::GetLength(PRUint32* aLength)
{
  *aLength = mControls->mElements.Count();
  
  return NS_OK;
}


NS_IMETHODIMP    
nsHTMLFormElement::NamedItem(JSContext* cx, jsval* argv, PRUint32 argc, jsval* aReturn)
{
  nsresult result = mControls->NamedItem(cx, argv, argc, aReturn);
  if (NS_FAILED(result)) {
    return result;
  }

  // If we couldn't find it in our controls list, it may be
  // a different type of element (IMG, OBJECT, etc.)
  if ((nsnull == *aReturn) && (nsnull != mInner.mDocument) && (argc > 0)) {
    char* str = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
    nsAutoString name(str); 
    nsCOMPtr<nsIScriptContext> scriptContext;
    nsCOMPtr<nsIScriptGlobalObject> globalObject;
    mInner.mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
    if (globalObject) {
      result = globalObject->GetContext(getter_AddRefs(scriptContext));
    }
    
    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(mInner.mDocument);
    if (htmlDoc) {
      nsCOMPtr<nsIDOMNodeList> list;
      result = htmlDoc->GetElementsByName(name, getter_AddRefs(list));
      if (NS_FAILED(result)) {
        return result;
      }
    
      if (list) {
        PRUint32 count;
        list->GetLength(&count);
        if (count > 0) {
          nsCOMPtr<nsIDOMNode> node;
          
          result = list->Item(0, getter_AddRefs(node));
          if (NS_FAILED(result)) {
            return result;
          }
          if (node) {
            nsCOMPtr<nsIScriptObjectOwner> owner = do_QueryInterface(node);
            JSObject* obj;
            
            result = owner->GetScriptObject(scriptContext, (void**)&obj);
            if (NS_FAILED(result)) {
              return result;
            }
            *aReturn = OBJECT_TO_JSVAL(obj);
          }
        }
      }
    }
  }

  return NS_OK;
}

PRBool    
nsHTMLFormElement::AddProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return mInner.AddProperty(aContext, aObj, aID, aVp);
}

PRBool    
nsHTMLFormElement::DeleteProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return mInner.DeleteProperty(aContext, aObj, aID, aVp);
}

PRBool    
nsHTMLFormElement::GetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return mInner.GetProperty(aContext, aObj, aID, aVp);
}

PRBool    
nsHTMLFormElement::SetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return mInner.SetProperty(aContext, aObj, aID, aVp);
}

PRBool    
nsHTMLFormElement::EnumerateProperty(JSContext *aContext, JSObject *aObj)
{
  return mInner.EnumerateProperty(aContext, aObj);
}


PRBool    
nsHTMLFormElement::Resolve(JSContext *aContext, JSObject *aObj, jsval aID)
{
  if (!JSVAL_IS_STRING(aID)) {
    return PR_TRUE;
  }

  PRBool ret;
  JSObject* obj;
  char* str = JS_GetStringBytes(JS_ValueToString(aContext, aID));
  nsAutoString name(str); 
  nsCOMPtr<nsIScriptContext> scriptContext;
  nsresult result = NS_OK;

  result = nsLayoutUtils::GetStaticScriptContext(aContext, aObj, getter_AddRefs(scriptContext));

  // If we can't get a script context, there's nothing we can do
  if (!scriptContext || NS_FAILED(result)) {
    return PR_FALSE;
  }
  
  result = mControls->GetNamedObject(aContext, aID, &obj);
  if (NS_FAILED(result)) {
    return PR_FALSE;
  }

  if ((nsnull == obj) && (nsnull != mInner.mDocument)) {
    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(mInner.mDocument);
    if (htmlDoc) {
      nsCOMPtr<nsIDOMNodeList> list;
      result = htmlDoc->GetElementsByName(name, getter_AddRefs(list));
      if (NS_FAILED(result)) {
        return PR_FALSE;
      }
    
      if (list) {
        PRUint32 count;
        list->GetLength(&count);
        if (count > 0) {
          nsCOMPtr<nsIDOMNode> node;
          
          result = list->Item(0, getter_AddRefs(node));
          if (NS_FAILED(result)) {
            return PR_FALSE;
          }
          if (node) {
            nsCOMPtr<nsIScriptObjectOwner> owner = do_QueryInterface(node);
            
            result = owner->GetScriptObject(scriptContext, (void**)&obj);
            if (NS_FAILED(result)) {
              return PR_FALSE;
            }
          }
        }
      }
    }
  }

  if (nsnull != obj) {
    JSObject* myObj;
    result = mInner.GetScriptObject(scriptContext, (void**)&myObj);
    ret = ::JS_DefineProperty(aContext, myObj,
                              str, OBJECT_TO_JSVAL(obj),
                              nsnull, nsnull, 0);
  }
  else {
    ret = mInner.Resolve(aContext, aObj, aID);
  }
  
  return ret;
}

PRBool    
nsHTMLFormElement::Convert(JSContext *aContext, JSObject *aObj, jsval aID)
{
  return mInner.Convert(aContext, aObj, aID);
}

void      
nsHTMLFormElement::Finalize(JSContext *aContext, JSObject *aObj)
{
  mInner.Finalize(aContext, aObj);
}

NS_IMETHODIMP 
nsHTMLFormElement::Item(PRUint32 aIndex, nsIDOMElement** aReturn)
{
  if (mControls) {
    nsIDOMNode *node;
    nsresult result = mControls->Item(aIndex, &node);
    if ((NS_OK == result) && (nsnull != node)) {
      result = node->QueryInterface(kIDOMElementIID, (void **)aReturn);
      NS_RELEASE(node);
    }
    else {
      *aReturn = nsnull;
    }
    return result;
  }
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

// nsFormControlList implementation, this could go away if there were a lightweight collection implementation somewhere


nsFormControlList::nsFormControlList(nsIDOMHTMLFormElement* aForm) 
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
  mForm = aForm; // WEAK - the form owns me
}

nsFormControlList::~nsFormControlList()
{
  mForm = nsnull;
  Clear();
}

NS_IMETHODIMP
nsFormControlList::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

void
nsFormControlList::SetForm(nsIDOMHTMLFormElement* aForm)
{
  mForm = aForm; // WEAK - the form owns me
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

NS_IMPL_ISUPPORTS3(nsFormControlList, nsIDOMHTMLCollection, nsIDOMHTMLFormControlList, nsIScriptObjectOwner)

nsresult nsFormControlList::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptHTMLFormControlList(aContext, (nsISupports *)(nsIDOMHTMLCollection *)this, nsnull, (void**)&mScriptObject);
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
nsFormControlList::Item(JSContext* cx, jsval* argv, PRUint32 argc, jsval* aReturn)
{
  nsCOMPtr<nsIDOMNode> element;
  nsresult result;
  nsCOMPtr<nsIScriptContext> scriptContext;
  nsCOMPtr<nsIScriptObjectOwner> owner;
  PRInt32 index;
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIContent> content;

  if (argc < 1) {
    return NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR;
  }
  
  *aReturn = nsnull;
  if (!JS_ValueToInt32(cx, argv[0], (int32*)&index)) {
    return NS_ERROR_FAILURE;
  }

  if (nsnull == mForm) {
    return NS_OK;
  }
  
  content = do_QueryInterface(mForm);
  if (content) {
    result = content->GetDocument(*getter_AddRefs(document));
    if (NS_FAILED(result)) {
      return result;
    }
  }

  if (document) {
    nsCOMPtr<nsIScriptGlobalObject> globalObject;
    document->GetScriptGlobalObject(getter_AddRefs(globalObject));
    if (globalObject) {
      result = globalObject->GetContext(getter_AddRefs(scriptContext));
    }
  }

  // If we can't get a script context, there's nothing we can do
  if (!scriptContext) {
    return NS_ERROR_FAILURE;
  }

  result = Item((PRUint32)index, getter_AddRefs(element));
  if (NS_FAILED(result)) {
    return result;
  }

  if (element) {
    owner = do_QueryInterface(element);

    if (owner) {
      JSObject* obj;
      result = owner->GetScriptObject(scriptContext, (void**)&obj);
      if (NS_FAILED(result)) {
        return result;
      }
      *aReturn = OBJECT_TO_JSVAL(obj);
    }
  }

  return NS_OK;
}

nsresult
nsFormControlList::GetNamedObject(JSContext* aContext, jsval aID, JSObject** aObj) 
{
  nsCOMPtr<nsIDOMNode> element;
  nsresult result = NS_OK;
  nsCOMPtr<nsIScriptContext> scriptContext;
  nsCOMPtr<nsIScriptObjectOwner> owner;
  char* str = JS_GetStringBytes(JS_ValueToString(aContext, aID));
  nsAutoString name(str); 
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIContent> form;

  *aObj = nsnull;
  if (nsnull == mForm) {
    return NS_OK;
  }
  
  form = do_QueryInterface(mForm);
  if (form) {
    result = form->GetDocument(*getter_AddRefs(document));
    if (NS_FAILED(result)) {
      return result;
    }
  }

  if (document) {
    nsCOMPtr<nsIScriptGlobalObject> globalObject;
    document->GetScriptGlobalObject(getter_AddRefs(globalObject));
    if (globalObject) {
      result = globalObject->GetContext(getter_AddRefs(scriptContext));
    }
  }

  // If we can't get a script context, there's nothing we can do
  if (!scriptContext) {
    return NS_ERROR_FAILURE;
  }

  // First see if it's a named element in the form
  result = NamedItem(name, getter_AddRefs(element));
  if (NS_FAILED(result)) {
    return result;
  }

  if (element) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(element);
    nsAutoString type;

    // See if it is radio button
    result = content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::type, type);
    if (NS_FAILED(result)) {
      return result;
    }

    // If it is, then get all radio buttons or checkboxes with the same name
    if (document && (type.Equals("Radio") || type.Equals("Checkbox"))) {
      nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(document);
      if (htmlDoc) {
        nsCOMPtr<nsIDOMNodeList> list;
        result = htmlDoc->GetElementsByName(name, getter_AddRefs(list));
        if (NS_FAILED(result)) {
          return result;
        }

        // Make sure that we have more than one element in the list
        if (list) {
          PRUint32 count;
          list->GetLength(&count);
          if (count > 1) {
            owner = do_QueryInterface(list);
          }
        }
      }
    }

    if (!owner) {
      owner = do_QueryInterface(element);
    }
  }

  if (owner) {
    result = owner->GetScriptObject(scriptContext, (void**)aObj);
    if (NS_FAILED(result)) {
      return result;
    }
  }  

  return result;
}

NS_IMETHODIMP    
nsFormControlList::NamedItem(JSContext* cx, jsval* argv, PRUint32 argc, jsval* aReturn)
{
  JSObject *obj;
  nsresult result = NS_OK;

  if (argc > 0) {
    result = GetNamedObject(cx, argv[0], &obj);
    if (NS_SUCCEEDED(result) && (nsnull != obj)) {
      *aReturn = OBJECT_TO_JSVAL(obj);
    }
  }
  else {
    result = NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR;
  }

  return result;
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

#ifdef DEBUG
nsresult
nsFormControlList::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
  PRUint32 asize;
  mElements.SizeOf(aSizer, &asize);
  *aResult = sizeof(*this) - sizeof(mElements) + asize;
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsHTMLFormElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
#ifdef DEBUG
  mInner.SizeOf(aSizer, aResult, sizeof(*this));
  if (mControls) {
    PRBool recorded;
    aSizer->RecordObject((void*) mControls, &recorded);
    if (!recorded) {
      PRUint32 controlSize;
      mControls->SizeOf(aSizer, &controlSize);
      aSizer->AddSize(nsHTMLAtoms::form_control_list, controlSize);
    }
  }
#endif
  return NS_OK;
}
