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
#include "nsHashtable.h"
#include "nsContentList.h"

static const int NS_FORM_CONTROL_LIST_HASHTABLE_SIZE = 64;

class nsFormControlList;

// nsHTMLFormElement

class nsHTMLFormElement : public nsIDOMHTMLFormElement,
                          public nsIDOMNSHTMLFormElement,
                          public nsIJSScriptObject,
                          public nsIHTMLContent,
                          public nsIForm
{
public:
  nsHTMLFormElement(nsINodeInfo *aNodeInfo);
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
  NS_DECL_IDOMHTMLFORMELEMENT

  // nsIDOMNSHTMLFormElement
  NS_DECL_IDOMNSHTMLFORMELEMENT  

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
  virtual PRBool    Resolve(JSContext *aContext, JSObject *aObj, jsval aID,
                            PRBool *aDidDefineProperty);
  virtual PRBool    Convert(JSContext *aContext, JSObject *aObj, jsval aID);
  virtual void      Finalize(JSContext *aContext, JSObject *aObj);

  // nsIForm
  NS_IMETHOD AddElement(nsIFormControl* aElement);
  NS_IMETHOD AddElementToTable(nsIFormControl* aChild, const nsAReadableString& aName);
  NS_IMETHOD GetElementAt(PRInt32 aIndex, nsIFormControl** aElement) const;
  NS_IMETHOD GetElementCount(PRUint32* aCount) const;
  NS_IMETHOD RemoveElement(nsIFormControl* aElement);
  NS_IMETHOD RemoveElementFromTable(nsIFormControl* aElement, const nsAReadableString& aName);

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

  nsresult GetNamedObject(JSContext* aContext, jsval aID, JSObject** aObj);

  nsresult AddElementToTable(nsIFormControl* aChild, const nsAReadableString& aName);
  nsresult RemoveElementFromTable(nsIFormControl* aChild, const nsAReadableString& aName);

#ifdef DEBUG
  nsresult SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

  void        *mScriptObject;
  nsVoidArray mElements;  // WEAK - bug 36639
  nsIDOMHTMLFormElement* mForm;  // WEAK - the form owns me

protected:
  nsSupportsHashtable* mLookupTable;  // A map from an ID or NAME attribute to the form control(s)
};

// nsHTMLFormElement implementation

// construction, destruction
nsresult
NS_NewHTMLFormElement(nsIHTMLContent** aInstancePtrResult,
                      nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLFormElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIHTMLContent), (void**) aInstancePtrResult);
}


nsHTMLFormElement::nsHTMLFormElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
  mControls = new nsFormControlList(this);
  NS_ADDREF(mControls);
//nsTraceRefcnt::Create((nsIForm*)this, "nsHTMLFormElement", __FILE__, __LINE__);
}

nsHTMLFormElement::~nsHTMLFormElement()
{
  // Null out childrens' pointer to me.  No refcounting here
  PRUint32 numControls;
  GetElementCount(&numControls);
  while (numControls--) {
    nsIFormControl* control = (nsIFormControl*)mControls->mElements.ElementAt(numControls); 
    if (control) {
      control->SetForm(nsnull); 
    }
  }
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
  if (aIID.Equals(NS_GET_IID(nsIJSScriptObject))) {
    nsIJSScriptObject* tmp = this;
    *aInstancePtr = (void*) tmp;
    AddRef();
    return NS_OK;
  }                                                             
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(NS_GET_IID(nsIForm))) {
    *aInstancePtr = (void*)(nsIForm*)this;
    AddRef();
    return NS_OK;
  } 
  else if (aIID.Equals(NS_GET_IID(nsIDOMHTMLFormElement))) {
    *aInstancePtr = (void*)(nsIDOMHTMLFormElement*)this;
    AddRef();
    return NS_OK;
  } 
  else if (aIID.Equals(NS_GET_IID(nsIDOMNSHTMLFormElement))) {
    *aInstancePtr = (void*)(nsIDOMNSHTMLFormElement*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsHTMLFormElement);
NS_IMPL_RELEASE(nsHTMLFormElement);

// nsIDOMHTMLFormElement
nsresult
nsHTMLFormElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLFormElement* it = new nsHTMLFormElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLFormElement::GetElements(nsIDOMHTMLCollection** aElements)
{
  *aElements = mControls;
  NS_ADDREF(mControls);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::GetName(nsAWritableString& aValue)
{
  return mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::name, aValue);
}

NS_IMETHODIMP
nsHTMLFormElement::SetName(const nsAReadableString& aValue)
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
        res = frame->QueryInterface(NS_GET_IID(nsIFormManager), (void**)&formMan);
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
  nsIDocument* doc = nsnull; // Strong
  nsresult res = GetDocument(doc);
  if (NS_SUCCEEDED(res) && doc) {
    PRInt32 numShells = doc->GetNumberOfShells();
    nsCOMPtr<nsIPresContext> context;
    for (PRInt32 i=0; i<numShells; i++) {
      nsCOMPtr<nsIPresShell> shell = getter_AddRefs(doc->GetShellAt(i));
      if (shell) {
        res = shell->GetPresContext(getter_AddRefs(context));
        if (NS_SUCCEEDED(res) && context) {
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event;
          event.eventStructType = NS_GUI_EVENT;
          event.message = NS_FORM_RESET;
          event.isShift = PR_FALSE;
          event.isControl = PR_FALSE;
          event.isAlt = PR_FALSE;
          event.isMeta = PR_FALSE;
          event.clickCount = 0;
          event.widget = nsnull;

          res = HandleDOMEvent(context, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
        }
      }
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
                              const nsAReadableString& aValue,
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
                              nsAWritableString& aResult) const
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
  nsresult ret = mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);

  if ((NS_OK == ret) && (nsEventStatus_eIgnore == *aEventStatus) &&
      !(aFlags & NS_EVENT_FLAG_CAPTURE)) {

    switch (aEvent->message) {
      case NS_FORM_RESET:
      {
       // XXX Need to do something special with mailto: or news: URLs
       nsIDocument* doc = nsnull; // Strong
       nsresult res = GetDocument(doc);
       if (NS_SUCCEEDED(res) && doc) {
         // Make sure the presentation is up-to-date
         doc->FlushPendingNotifications();
         NS_RELEASE(doc);
       }

       nsCOMPtr<nsIPresShell> shell;
       aPresContext->GetShell(getter_AddRefs(shell));
       if (shell) {
         nsIFrame* frame;
         shell->GetPrimaryFrameFor(this, &frame);
         if (frame) {
           nsIFormManager* formMan = nsnull;
           ret = frame->QueryInterface(NS_GET_IID(nsIFormManager), (void**)&formMan);
           if (NS_SUCCEEDED(ret) && formMan) {
             ret = formMan->OnReset(aPresContext);
           }
         }
       }
      }
      break;
    }
  }
  return ret;
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
  // WEAK - don't addref
  return rv;
}

NS_IMETHODIMP
nsHTMLFormElement::AddElementToTable(nsIFormControl* aChild, const nsAReadableString& aName)
{
  return mControls->AddElementToTable(aChild, aName);  
}


NS_IMETHODIMP 
nsHTMLFormElement::RemoveElement(nsIFormControl* aChild) 
{
  mControls->mElements.RemoveElement(aChild);

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFormElement::RemoveElementFromTable(nsIFormControl* aElement,
                                          const nsAReadableString& aName)
{
  return mControls->RemoveElementFromTable(aElement, aName);
}

NS_IMETHODIMP
nsHTMLFormElement::GetEncoding(nsAWritableString& aEncoding)
{
  return mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::enctype, aEncoding);
}
 
NS_IMETHODIMP    
nsHTMLFormElement::GetLength(PRInt32* aLength)
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
    nsAutoString name; name.AssignWithConversion(str); 
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
nsHTMLFormElement::Resolve(JSContext *aContext, JSObject *aObj, jsval aID,
                           PRBool *aDidDefineProperty)
{
  if (!JSVAL_IS_STRING(aID)) {
    return PR_TRUE;
  }

  PRBool ret;
  JSObject* obj;
  char* str = JS_GetStringBytes(JS_ValueToString(aContext, aID));
  nsAutoString name; name.AssignWithConversion(str); 
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

  if (!obj && mInner.mDocument) {
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
 
  if (obj) {
    JSObject* myObj;
    result = mInner.GetScriptObject(scriptContext, (void**)&myObj);
    ret = ::JS_DefineProperty(aContext, myObj,
                              str, OBJECT_TO_JSVAL(obj),
                              nsnull, nsnull, 0);

    *aDidDefineProperty = PR_TRUE;
  }
  else {
    ret = mInner.Resolve(aContext, aObj, aID, aDidDefineProperty);
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
      result = node->QueryInterface(NS_GET_IID(nsIDOMElement), (void **)aReturn);
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
  mLookupTable = nsnull;
}

nsFormControlList::~nsFormControlList()
{
  if (mLookupTable) {
    delete mLookupTable;
    mLookupTable = nsnull;
  }
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
  mElements.Clear();

  if (mLookupTable)
    mLookupTable->Reset();
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
    return control->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aReturn); // keep the ref
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
  NS_ENSURE_ARG_POINTER(aObj);
  *aObj = nsnull;

  if (!mForm) {
    // No form, no named objects
    return NS_OK;
  }
  
  nsresult rv = NS_OK;
  nsCOMPtr<nsIScriptContext> scriptContext;
  nsCOMPtr<nsIScriptObjectOwner> owner;
  char* str = JS_GetStringBytes(JS_ValueToString(aContext, aID));

  if (mLookupTable) {
    // Get the hash entry
    nsAutoString ustr; ustr.AssignWithConversion(str);
    nsStringKey key(ustr);

    nsCOMPtr<nsISupports> tmp = dont_AddRef((nsISupports *)mLookupTable->Get(&key));

    if (tmp) {
      // Found something, we don't care here if it's a element or a node
      // list, we just return the script object
      owner = do_QueryInterface(tmp);
    }
  }

  if (!owner) {
    // No owner means we didn't find anything, at least not something we can
    // return as a JSObject.
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIContent> form = do_QueryInterface(mForm);

  if (form) {
    rv = form->GetDocument(*getter_AddRefs(document));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (document) {
    nsCOMPtr<nsIScriptGlobalObject> globalObject;
    document->GetScriptGlobalObject(getter_AddRefs(globalObject));
    if (globalObject) {
      rv = globalObject->GetContext(getter_AddRefs(scriptContext));
    }
  }

  // If we can't get a script context, there's nothing we can do
  if (!scriptContext) {
    return NS_ERROR_FAILURE;
  }

  return owner->GetScriptObject(scriptContext, (void**)aObj);
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
nsFormControlList::NamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  nsresult rv = NS_OK;
  nsStringKey key(aName);
  nsCOMPtr<nsISupports> supports;
  *aReturn = nsnull;

  if (mLookupTable) {
    supports = dont_AddRef((nsISupports *)mLookupTable->Get(&key));
  }

  if (supports) {
    // We found something, check if it's a node
    rv = supports->QueryInterface(NS_GET_IID(nsIDOMNode), (void **)aReturn);

    if (NS_FAILED(rv)) {
      // If not, we check if it's a node list.
      nsCOMPtr<nsIDOMNodeList> nodeList(do_QueryInterface(supports, &rv));

      if (nodeList) {
        // And since we're only asking for one node here, we return the first
        // one from the list.
        rv = nodeList->Item(0, aReturn);
      }
    }
  }

  return rv;
}

nsresult
nsFormControlList::AddElementToTable(nsIFormControl* aChild, const nsAReadableString& aName)
{
  nsStringKey key(aName);
  if (!mLookupTable) {
    mLookupTable = new nsSupportsHashtable(NS_FORM_CONTROL_LIST_HASHTABLE_SIZE);
    NS_ENSURE_TRUE(mLookupTable, NS_ERROR_OUT_OF_MEMORY);
  }

  nsCOMPtr<nsISupports> supports;
  supports = dont_AddRef((nsISupports *)mLookupTable->Get(&key));

  if (!supports) {
    // No entry found, add the form control
    nsCOMPtr<nsISupports> child(do_QueryInterface(aChild));

    mLookupTable->Put(&key, child.get());
  } else {
    // Found something in the hash, check its type
    nsCOMPtr<nsIContent> content(do_QueryInterface(supports));
    nsCOMPtr<nsIContent> newChild(do_QueryInterface(aChild));

    if (content) {
      // Check if the new content is the same as the one we found in the
      // hash, if it is then we leave it in the hash as it is, this will
      // happen if a form control has both a name and an id with the same
      // value
      if (content == newChild) {
        return NS_OK;
      }

      // Found an element, create a list, add the element to the list and put
      // the list in the hash
      nsContentList *list = new nsContentList(nsnull);
      NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

      list->Add(content);

      // Add the new child too
      list->Add(newChild);

      nsCOMPtr<nsISupports> listSupports;
      list->QueryInterface(NS_GET_IID(nsISupports),
                           getter_AddRefs(listSupports));
      // Replace the element with the list.
      mLookupTable->Put(&key, listSupports.get());
    } else {
      // There's already a list in the hash, add the child to the list
      nsCOMPtr<nsIDOMNodeList> nodeList(do_QueryInterface(supports));
      NS_ENSURE_TRUE(nodeList, NS_ERROR_FAILURE);

      // Upcast, uggly, but it works!
      nsContentList *list = NS_STATIC_CAST(nsContentList *,
                                           (nsIDOMNodeList *)nodeList.get());

      PRInt32 oldIndex = -1;
      list->IndexOf(newChild, oldIndex);

      // Add the new child only if it's not in our list already
      if (oldIndex < 0) {
        list->Add(newChild);
      }
    }
  }

  return NS_OK;
}

nsresult
nsFormControlList::RemoveElementFromTable(nsIFormControl* aChild,
                                          const nsAReadableString& aName)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aChild);  
  if (mLookupTable && content) {
    nsStringKey key(aName);

    nsCOMPtr<nsISupports> supports;
    supports = dont_AddRef((nsISupports *)mLookupTable->Get(&key));

    if (!supports)
      return NS_OK;

    nsCOMPtr<nsIFormControl> fctrl(do_QueryInterface(supports));

    if (fctrl) {
      // Single element in the hash, just remove it...
      mLookupTable->Remove(&key);
    } else {
      nsCOMPtr<nsIDOMNodeList> nodeList(do_QueryInterface(supports));
      NS_ENSURE_TRUE(nodeList, NS_ERROR_FAILURE);

      // Upcast, uggly, but it works!
      nsContentList *list = NS_STATIC_CAST(nsContentList *,
                                           (nsIDOMNodeList *)nodeList.get());

      list->Remove(content);

      PRUint32 length = 0;
      list->GetLength(&length);

      if (!length) {
        // If the list is empty we remove if from our hash, this shouldn't happen tho
        mLookupTable->Remove(&key);
      } else if (length == 1) {
        // Only one element left, replace the list in the hash with the
        // single element.
        nsCOMPtr<nsIDOMNode> node;
        list->Item(0, getter_AddRefs(node));

        if (node) {
          nsCOMPtr<nsISupports> tmp(do_QueryInterface(node));
          mLookupTable->Put(&key, tmp.get());
        }
      }
    }
  }
  return NS_OK;
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
