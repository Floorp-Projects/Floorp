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
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIHTMLAttributes.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIWidget.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIFocusableContent.h"
#include "nsIEventStateManager.h"

// Notify/query select frame for selectedIndex
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFormControlFrame.h"
#include "nsIFrame.h"

static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFormIID, NS_IFORM_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID); 
static NS_DEFINE_IID(kIFocusableContentIID, NS_IFOCUSABLECONTENT_IID);

class nsHTMLSelectElement;

// nsOptionList
class nsOptionList : public nsIDOMHTMLCollection, public nsIScriptObjectOwner {
public:
  nsOptionList();
  nsOptionList(nsIDOMHTMLCollection* aCollection);
  virtual ~nsOptionList();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);
  NS_IMETHOD ResetScriptObject();

  // nsIDOMHTMLCollection interface
  NS_DECL_IDOMHTMLCOLLECTION
  
  nsVoidArray& GetElements() { return mElements; }
  void Clear();

private:
  nsVoidArray  mElements;
  void*        mScriptObject;
};

class nsHTMLSelectElement : public nsIDOMHTMLSelectElement,
                            public nsIScriptObjectOwner,
                            public nsIDOMEventReceiver,
                            public nsIHTMLContent,
                            public nsIFormControl,
                            public nsIFocusableContent
{
public:
  nsHTMLSelectElement(nsIAtom* aTag);
  virtual ~nsHTMLSelectElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLSelectElement
  NS_IMETHOD GetType(nsString& aType);
  NS_IMETHOD GetSelectedIndex(PRInt32* aSelectedIndex);
  NS_IMETHOD SetSelectedIndex(PRInt32 aSelectedIndex);
  NS_IMETHOD GetValue(nsString& aValue);
  NS_IMETHOD SetValue(const nsString& aValue);
  NS_IMETHOD GetLength(PRUint32* aLength);
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
  NS_IMETHOD GetOptions(nsIDOMHTMLCollection** aOptions);
  NS_IMETHOD GetDisabled(PRBool* aDisabled);
  NS_IMETHOD SetDisabled(PRBool aDisabled);
  NS_IMETHOD GetMultiple(PRBool* aMultiple);
  NS_IMETHOD SetMultiple(PRBool aMultiple);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD SetName(const nsString& aName);
  NS_IMETHOD GetSize(PRInt32* aSize);
  NS_IMETHOD SetSize(PRInt32 aSize);
  NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_IMETHOD Add(nsIDOMHTMLElement* aElement, nsIDOMHTMLElement* aBefore);
  NS_IMETHOD Remove(PRInt32 aIndex);
  NS_IMETHOD Blur();
  NS_IMETHOD Focus();

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

  // nsIFormControl
  NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm);
  NS_IMETHOD GetType(PRInt32* aType);
  NS_IMETHOD SetWidget(nsIWidget* aWidget);
  NS_IMETHOD Init();

  NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);

protected:
  nsGenericHTMLContainerElement mInner;
  nsIWidget*    mWidget; // XXX this needs to go away when FindFrameWithContent is efficient
  nsIForm*      mForm;
  nsOptionList* mOptions;
};


// nsHTMLSelectElement

// construction, destruction

nsresult
NS_NewHTMLSelectElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLSelectElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLSelectElement::nsHTMLSelectElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
  mOptions = nsnull;
  mForm = nsnull;
  mWidget = nsnull;
}

nsHTMLSelectElement::~nsHTMLSelectElement()
{
  NS_IF_RELEASE(mWidget);
  if (nsnull != mForm) {
    // prevent mForm from decrementing its ref count on us
    mForm->RemoveElement(this, PR_FALSE); 
    NS_RELEASE(mForm);
  }
  NS_IF_RELEASE(mOptions);
}

// ISupports

NS_IMETHODIMP
nsHTMLSelectElement::AddRef(void)
{
  return ++mRefCnt; 
}

nsresult
nsHTMLSelectElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLSelectElementIID)) {
    *aInstancePtr = (void*)(nsIDOMHTMLSelectElement*)this;
    mRefCnt++;
    return NS_OK;
  }
  else if (aIID.Equals(kIFormControlIID)) {
    *aInstancePtr = (void*)(nsIFormControl*)this;
    mRefCnt++;
    return NS_OK;
  }
  else if (aIID.Equals(kIFocusableContentIID)) {
    *aInstancePtr = (void*)(nsIFocusableContent*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP_(nsrefcnt)
nsHTMLSelectElement::Release()
{
  --mRefCnt;
	if (mRefCnt <= 0) {
    delete this;                                       
    return 0;                                          
  } else if ((1 == mRefCnt) && mForm) { 
    mRefCnt = 0;
    delete this;
    return 0;
  } else {
    return mRefCnt;
  }
}

// nsIDOMHTMLSelectElement

nsresult
nsHTMLSelectElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLSelectElement* it = new nsHTMLSelectElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLSelectElement::Add(nsIDOMHTMLElement* aElement, nsIDOMHTMLElement* aBefore)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLSelectElement::Remove(PRInt32 aIndex) 
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  nsresult result = NS_OK;
  *aForm = nsnull;
  if (nsnull != mForm) {
    nsIDOMHTMLFormElement* formElem = nsnull;
    result = mForm->QueryInterface(kIDOMHTMLFormElementIID, (void**)&formElem);
    if (NS_OK == result) {
      *aForm = formElem;
    }
  }
  return result;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetOptions(nsIDOMHTMLCollection** aValue)
{
  if (!mOptions) {
    Init();
  }
  NS_ADDREF(mOptions);
  *aValue = mOptions;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetType(nsString& aType)
{
  PRBool isMultiple;
  nsresult result = NS_OK;

  result = GetMultiple(&isMultiple);
  if (NS_OK == result) {
    if (isMultiple) {
      aType.SetString("select-multiple");
    }
    else {
      aType.SetString("select-one");
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetLength(PRUint32* aLength)
{
  if (nsnull != mOptions) {
    return mOptions->GetLength(aLength);
  }
  else {
    *aLength = 0;
    return NS_OK;
  }
}

//NS_IMPL_INT_ATTR(nsHTMLSelectElement, SelectedIndex, selectedindex)
NS_IMETHODIMP
nsHTMLSelectElement::GetSelectedIndex(PRInt32* aValue)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  if (NS_OK == nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame)) {
    nsString value;
    formControlFrame->GetProperty(nsHTMLAtoms::selectedindex, value);
    NS_RELEASE(formControlFrame);
    if (value.Length() > 0) {
      PRInt32 retval = 0;
      PRInt32 error = 0;
      retval = value.ToInteger(&error, 10); // Convert to integer, base 10
      if (!error) {
        *aValue = retval;
        return NS_OK;
      }
    }
  } else { // The frame hasn't been created yet.  Use the options array
    *aValue = -1;
    nsIDOMHTMLCollection* options;
    if (NS_OK == GetOptions(&options)) {
      PRUint32 numOptions;
      options->GetLength(&numOptions);

      for (PRUint32 i = 0; i < numOptions; i++) {
        nsIDOMNode* node = nsnull;
        if ((NS_OK == options->Item(i, &node)) && node) {
          nsIDOMHTMLOptionElement* option = nsnull;
          if (NS_OK == node->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&option)) {
            PRBool selected;
            option->GetDefaultSelected(&selected); // DefaultSelected == HTML Selected
            NS_RELEASE(option);
            if (selected) {
              *aValue = i;
              NS_RELEASE(node); // Have to release this as the call below is skipped.
              break;
            }
          }
          NS_RELEASE(node);
        }
      }
      NS_RELEASE(options);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::SetSelectedIndex(PRInt32 aValue)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  if (NS_OK == nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame)) {
    nsString value;
    value.Append(aValue, 10);
    formControlFrame->SetProperty(nsHTMLAtoms::selectedindex, value);
    NS_RELEASE(formControlFrame);
  }
  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLSelectElement, Value, value)
NS_IMPL_BOOL_ATTR(nsHTMLSelectElement, Disabled, disabled)
NS_IMPL_BOOL_ATTR(nsHTMLSelectElement, Multiple, multiple)
NS_IMPL_STRING_ATTR(nsHTMLSelectElement, Name, name)
NS_IMPL_INT_ATTR(nsHTMLSelectElement, Size, size)
NS_IMPL_INT_ATTR(nsHTMLSelectElement, TabIndex, tabindex)

NS_IMETHODIMP
nsHTMLSelectElement::Blur() // XXX not tested
{
  if (nsnull != mWidget) {
    nsIWidget *mParentWidget = mWidget->GetParent();
    if (nsnull != mParentWidget) {
      mParentWidget->SetFocus();
      NS_RELEASE(mParentWidget);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::Focus()
{
  if (nsnull != mWidget) { // XXX not tested
    mWidget->SetFocus();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::SetFocus(nsIPresContext* aPresContext)
{
  nsIEventStateManager* esm;
  if (NS_OK == aPresContext->GetEventStateManager(&esm)) {
    esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
    NS_RELEASE(esm);
  }

  // XXX Should focus only this presContext
  Focus();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::RemoveFocus(nsIPresContext* aPresContext)
{
  // XXX Should focus only this presContext
  Blur();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::StringToAttribute(nsIAtom* aAttribute,
                              const nsString& aValue,
                              nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::disabled) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::multiple) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::size) {
    nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::tabindex) {
    nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLSelectElement::AttributeToString(nsIAtom* aAttribute,
                              const nsHTMLValue& aValue,
                              nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsHTMLValue value;

  aAttributes->GetAttribute(nsHTMLAtoms::align, value);
  if (eHTMLUnit_Enumerated == value.GetUnit()) {
    nsStyleDisplay* display = (nsStyleDisplay*)
      aContext->GetMutableStyleData(eStyleStruct_Display);
    nsStyleText* text = (nsStyleText*)
      aContext->GetMutableStyleData(eStyleStruct_Text);
    switch (value.GetIntValue()) {
    case NS_STYLE_TEXT_ALIGN_LEFT:
      display->mFloats = NS_STYLE_FLOAT_LEFT;
      break;
    case NS_STYLE_TEXT_ALIGN_RIGHT:
      display->mFloats = NS_STYLE_FLOAT_RIGHT;
      break;
    default:
      text->mVerticalAlign.SetIntValue(value.GetIntValue(), eStyleUnit_Enumerated);
      break;
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLSelectElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLSelectElement::HandleDOMEvent(nsIPresContext& aPresContext,
                           nsEvent* aEvent,
                           nsIDOMEvent** aDOMEvent,
                           PRUint32 aFlags,
                           nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

// nsIFormControl

NS_IMETHODIMP
nsHTMLSelectElement::GetType(PRInt32* aType)
{
  if (aType) {
    *aType = NS_FORM_SELECT;
    return NS_OK;
  } else {
    return NS_FORM_NOTOK;
  }
}

NS_IMETHODIMP
nsHTMLSelectElement::SetWidget(nsIWidget* aWidget)
{
  if (aWidget != mWidget) {
	  NS_IF_RELEASE(mWidget);
    NS_IF_ADDREF(aWidget);
    mWidget = aWidget;
  }
  return NS_OK;
}

// An important assumption is that if aForm is null, the previous mForm will not be released
// This allows nsHTMLFormElement to deal with circular references.
NS_IMETHODIMP
nsHTMLSelectElement::SetForm(nsIDOMHTMLFormElement* aForm)
{
  nsresult result = NS_OK;
  if (nsnull == aForm) {
    mForm = nsnull;
    return NS_OK;
  } else {
    NS_IF_RELEASE(mForm);
    nsIFormControl* formControl = nsnull;
    result = QueryInterface(kIFormControlIID, (void**)&formControl);
    if ((NS_OK == result) && formControl) {
      result = aForm->QueryInterface(kIFormIID, (void**)&mForm); // keep the ref
      if ((NS_OK == result) && mForm) {
        mForm->AddElement(formControl);
      }
      NS_RELEASE(formControl);
    }
  }
  return result;
}

static
void GetOptionsRecurse(nsIContent* aContent, nsVoidArray& aOptions)
{
  PRInt32 numChildren;
  aContent->ChildCount(numChildren);
  nsIContent* child = nsnull;
  nsIDOMHTMLOptionElement* option = nsnull;
  for (int i = 0; i < numChildren; i++) {
    aContent->ChildAt(i, child);
    if (child) {
      nsresult result = child->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&option);
      if ((NS_OK == result) && option) {
        aOptions.AppendElement(option); // keep the ref count
      } else {
        GetOptionsRecurse(child, aOptions);
      }
      NS_RELEASE(child);
    }
  }
}

NS_IMETHODIMP
nsHTMLSelectElement::Init()
{
  if (mOptions) {
    mOptions->Clear();
    NS_RELEASE(mOptions);
  }
  mOptions = new nsOptionList();
  NS_ADDREF(mOptions);
  GetOptionsRecurse(this, mOptions->GetElements());

  return NS_OK;
}

// nsOptionList implementation
// XXX this was modified form nsHTMLFormElement.cpp. We need a base class implementation

nsOptionList::nsOptionList() 
{
  mRefCnt = 0;
  mScriptObject = nsnull;
}

nsOptionList::nsOptionList(nsIDOMHTMLCollection* aCollection) 
{
  mRefCnt = 0;
  mScriptObject = nsnull;
  if (aCollection) {
    PRUint32 length = 0;
    aCollection->GetLength(&length);
    nsIDOMNode* node;
    for (PRUint32 elemX = 0; elemX < length; elemX++) {
      nsresult result = aCollection->Item(elemX, &node); // this assumes an ADDREF
      if ((NS_OK == result) && node) {
        nsIDOMHTMLOptionElement* option = nsnull;
        result = node->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&option);
        if ((NS_OK == result) && option) {
          mElements.AppendElement(node); // keep the node ref count
          NS_RELEASE(option);
        }
      }
    }
  }
}

nsOptionList::~nsOptionList()
{
  Clear();
}


nsresult nsOptionList::QueryInterface(REFNSIID aIID, void** aInstancePtr)
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

NS_IMPL_ADDREF(nsOptionList)
NS_IMPL_RELEASE(nsOptionList)


nsresult nsOptionList::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptHTMLCollection(aContext, (nsISupports *)(nsIDOMHTMLCollection *)this, nsnull, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP
nsOptionList::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

nsresult nsOptionList::ResetScriptObject()
{
  mScriptObject = nsnull;
  return NS_OK;
}

// nsIDOMHTMLCollection interface
NS_IMETHODIMP    
nsOptionList::GetLength(PRUint32* aLength)
{
  *aLength = mElements.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsOptionList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  PRUint32 length = 0;
  GetLength(&length);
  if (aIndex >= length) {
    *aReturn = nsnull;
  } else {
    *aReturn = (nsIDOMNode*)mElements.ElementAt(aIndex);
    NS_ADDREF(*aReturn);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsOptionList::NamedItem(const nsString& aName, nsIDOMNode** aReturn)
{
  PRUint32 count = mElements.Count();
  nsresult result = NS_OK;

  *aReturn = nsnull;
  for (PRUint32 i = 0; i < count && *aReturn == nsnull; i++) {
    nsIDOMHTMLOptionElement *option;
    option = (nsIDOMHTMLOptionElement*)mElements.ElementAt(i);
    if (nsnull != option) {
      nsIContent *content;
      
      result = option->QueryInterface(kIContentIID, (void **)&content);
      if (NS_OK == result) {
        nsAutoString name;
        // XXX Should it be an EqualsIgnoreCase?
        if (((content->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::name, name) == NS_CONTENT_ATTR_HAS_VALUE) &&
             (aName.Equals(name))) ||
            ((content->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::id, name) == NS_CONTENT_ATTR_HAS_VALUE) &&
             (aName.Equals(name)))) {
          result = option->QueryInterface(kIDOMNodeIID, (void **)aReturn);
        }
        NS_RELEASE(content);
      }
    }
  }
  
  return result;
}

void
nsOptionList::Clear()
{
  PRUint32 numOptions = mElements.Count();
  for (PRUint32 i = 0; i < numOptions; i++) {
    nsIDOMHTMLOptionElement* option = (nsIDOMHTMLOptionElement*)mElements.ElementAt(i);
    NS_ASSERTION(option,"option already released");
    NS_RELEASE(option);
  }
  mElements.Clear();
}

NS_IMETHODIMP
nsHTMLSelectElement::GetStyleHintForAttributeChange(
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const
{
  nsGenericHTMLElement::GetStyleHintForCommonAttributes(this, aAttribute, aHint);
  return NS_OK;
}
