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
#include "nsIDOMHTMLInputElement.h"
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
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFormControlFrame.h"
#include "nsIFrame.h"
#include "nsIFocusableContent.h"
#include "nsIEventStateManager.h"

// XXX align=left, hspace, vspace, border? other nav4 attrs

static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIFormIID, NS_IFORM_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID); 
static NS_DEFINE_IID(kIFocusableContentIID, NS_IFOCUSABLECONTENT_IID);

class nsHTMLInputElement : public nsIDOMHTMLInputElement,
                           public nsIScriptObjectOwner,
                           public nsIDOMEventReceiver,
                           public nsIHTMLContent,
                           public nsIFormControl,
                           public nsIFocusableContent
{
public:
  nsHTMLInputElement(nsIAtom* aTag);
  virtual ~nsHTMLInputElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLInputElement
  NS_IMETHOD GetDefaultValue(nsString& aDefaultValue);
  NS_IMETHOD SetDefaultValue(const nsString& aDefaultValue);
  NS_IMETHOD GetDefaultChecked(PRBool* aDefaultChecked);
  NS_IMETHOD SetDefaultChecked(PRBool aDefaultChecked);
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
  NS_IMETHOD GetAccept(nsString& aAccept);
  NS_IMETHOD SetAccept(const nsString& aAccept);
  NS_IMETHOD GetAccessKey(nsString& aAccessKey);
  NS_IMETHOD SetAccessKey(const nsString& aAccessKey);
  NS_IMETHOD GetAlign(nsString& aAlign);
  NS_IMETHOD SetAlign(const nsString& aAlign);
  NS_IMETHOD GetAlt(nsString& aAlt);
  NS_IMETHOD SetAlt(const nsString& aAlt);
  NS_IMETHOD GetChecked(PRBool* aChecked);
  NS_IMETHOD SetChecked(PRBool aChecked);
  NS_IMETHOD GetDisabled(PRBool* aDisabled);
  NS_IMETHOD SetDisabled(PRBool aDisabled);
  NS_IMETHOD GetMaxLength(PRInt32* aMaxLength);
  NS_IMETHOD SetMaxLength(PRInt32 aMaxLength);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD SetName(const nsString& aName);
  NS_IMETHOD GetReadOnly(PRBool* aReadOnly);
  NS_IMETHOD SetReadOnly(PRBool aReadOnly);
  NS_IMETHOD GetSize(nsString& aSize);
  NS_IMETHOD SetSize(const nsString& aSize);
  NS_IMETHOD GetSrc(nsString& aSrc);
  NS_IMETHOD SetSrc(const nsString& aSrc);
  NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_IMETHOD GetType(nsString& aType);
  NS_IMETHOD GetUseMap(nsString& aUseMap);
  NS_IMETHOD SetUseMap(const nsString& aUseMap);
  NS_IMETHOD GetValue(nsString& aValue);
  NS_IMETHOD SetValue(const nsString& aValue);
  NS_IMETHOD GetAutocomplete(nsString& aAutocomplete);
  NS_IMETHOD SetAutocomplete(const nsString& aAutocomplete);
  NS_IMETHOD Blur();
  NS_IMETHOD Focus();
  NS_IMETHOD Select();
  NS_IMETHOD Click();

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_NO_SETPARENT_NO_SETDOCUMENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

  // nsIFormControl
  NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm);
  NS_IMETHOD GetType(PRInt32* aType);
  NS_IMETHOD Init() { return NS_OK; }

  // nsIFocusableContent
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);

protected:
  nsGenericHTMLLeafElement mInner;
  nsIForm*                 mForm;
  PRInt32                  mType;

  PRBool IsImage() const {
    nsAutoString tmp;
    mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::type, tmp);
    return tmp.EqualsIgnoreCase("image");
  }
};

// construction, destruction

nsresult
NS_NewHTMLInputElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLInputElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLInputElement::nsHTMLInputElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
  mType = NS_FORM_INPUT_TEXT; // default value
  mForm = nsnull;
  //nsTraceRefcnt::Create((nsIFormControl*)this, "nsHTMLFormControlElement", __FILE__, __LINE__);

}

nsHTMLInputElement::~nsHTMLInputElement()
{
  if (nsnull != mForm) {
    // prevent mForm from decrementing its ref count on us
    mForm->RemoveElement(this, PR_FALSE); 
    NS_RELEASE(mForm);
  }
}

// nsISupports

NS_IMETHODIMP_(nsrefcnt) 
nsHTMLInputElement::AddRef(void)
{
  return ++mRefCnt; 
}


NS_IMETHODIMP
nsHTMLInputElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLInputElementIID)) {
    *aInstancePtr = (void*)(nsIDOMHTMLInputElement*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(kIFormControlIID)) {
    *aInstancePtr = (void*)(nsIFormControl*) this;
    NS_ADDREF_THIS();
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
nsHTMLInputElement::Release()
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

// nsIDOMNode

nsresult
nsHTMLInputElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLInputElement* it = new nsHTMLInputElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

// nsIContent

NS_IMETHODIMP
nsHTMLInputElement::SetParent(nsIContent* aParent)
{
  return mInner.SetParentForFormControls(aParent, this, mForm);
}

NS_IMETHODIMP
nsHTMLInputElement::SetDocument(nsIDocument* aDocument, PRBool aDeep)
{
  return mInner.SetDocumentForFormControls(aDocument, aDeep, this, mForm);
}

NS_IMETHODIMP
nsHTMLInputElement::GetForm(nsIDOMHTMLFormElement** aForm)
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
nsHTMLInputElement::GetDefaultValue(nsString& aDefaultValue)
{
  return mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::value, aDefaultValue);
}

NS_IMETHODIMP 
nsHTMLInputElement::SetDefaultValue(const nsString& aDefaultValue)
{
  return mInner.SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::value, aDefaultValue, PR_TRUE); 
}

NS_IMETHODIMP 
nsHTMLInputElement::GetDefaultChecked(PRBool* aDefaultChecked)
{
  nsHTMLValue val;                                                 
  nsresult rv = mInner.GetHTMLAttribute(nsHTMLAtoms::checked, val);       
  *aDefaultChecked = (NS_CONTENT_ATTR_NOT_THERE != rv);                        
  return NS_OK;                                                     
}

NS_IMETHODIMP
nsHTMLInputElement::SetDefaultChecked(PRBool aDefaultChecked)
{
  nsHTMLValue empty(eHTMLUnit_Empty);
  if (aDefaultChecked) {                                                     
    return mInner.SetHTMLAttribute(nsHTMLAtoms::checked, empty, PR_TRUE); 
  } else {                                                            
    mInner.UnsetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::checked, PR_TRUE);             
    return NS_OK;                                                   
  }                                                                 
}
  
//NS_IMPL_STRING_ATTR(nsHTMLInputElement, DefaultValue, defaultvalue)
//NS_IMPL_BOOL_ATTR(nsHTMLInputElement, DefaultChecked, defaultchecked)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Accept, accept)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, AccessKey, accesskey)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Alt, alt)
//NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Checked, checked)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Disabled, disabled)
NS_IMPL_INT_ATTR(nsHTMLInputElement, MaxLength, maxlength)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Name, name)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, ReadOnly, readonly)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Size, size)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Src, src)
NS_IMPL_INT_ATTR(nsHTMLInputElement, TabIndex, tabindex)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, UseMap, usemap)
//NS_IMPL_STRING_ATTR(nsHTMLInputElement, Value, value)

NS_IMETHODIMP
nsHTMLInputElement::GetType(nsString& aValue)
{
  mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::type, aValue);
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLInputElement::GetValue(nsString& aValue)
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_TEXT == type || NS_FORM_INPUT_PASSWORD == type || NS_FORM_INPUT_FILE == type) {
    nsIFormControlFrame* formControlFrame = nsnull;
    if (NS_SUCCEEDED(nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame))) {
      if (nsnull != formControlFrame) {
        formControlFrame->GetProperty(nsHTMLAtoms::value, aValue);
      }
      return NS_OK;
    }
  }
  // Treat value == defaultValue for other input elements
  return mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::value, aValue); 
}


NS_IMETHODIMP 
nsHTMLInputElement::SetValue(const nsString& aValue)
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_TEXT == type || NS_FORM_INPUT_PASSWORD == type || NS_FORM_INPUT_FILE == type) {
    nsIFormControlFrame* formControlFrame = nsnull;
    if (NS_SUCCEEDED(nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame))) {
      if (nsnull != formControlFrame ) { 
        formControlFrame->SetProperty(nsHTMLAtoms::value, aValue);
      }
      return NS_OK;
    }
  }
  // Treat value == defaultValue for other input elements.
  return mInner.SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::value, aValue, PR_TRUE);
}

NS_IMETHODIMP 
nsHTMLInputElement::GetAutocomplete(nsString& aAutocomplete)
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_TEXT == type) {
    nsIFormControlFrame* formControlFrame = nsnull;
    if (NS_SUCCEEDED(nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame))) {
      if (nsnull != formControlFrame) {
        formControlFrame->GetProperty(nsHTMLAtoms::autocomplete, aAutocomplete);
      }
    }
  }
  else
  	aAutocomplete = "";
  
  return NS_OK;
}


NS_IMETHODIMP 
nsHTMLInputElement::SetAutocomplete(const nsString& aAutocomplete)
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_TEXT == type) {
    nsIFormControlFrame* formControlFrame = nsnull;
    if (NS_SUCCEEDED(nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame))) {
      if (nsnull != formControlFrame ) { 
        formControlFrame->SetProperty(nsHTMLAtoms::autocomplete, aAutocomplete);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLInputElement::GetChecked(PRBool* aValue)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  if (NS_OK == nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame)) {
      if (nsnull != formControlFrame) {
      nsString value("0");
      formControlFrame->GetProperty(nsHTMLAtoms::checked, value);
      if (value == "1")
        *aValue = PR_TRUE;
      else
        *aValue = PR_FALSE;
    }
  }
  return NS_OK;      
}


NS_IMETHODIMP 
nsHTMLInputElement::SetChecked(PRBool aValue)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  if (NS_OK == nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame)) {
    if (PR_TRUE == aValue) {
     formControlFrame->SetProperty(nsHTMLAtoms::checked, "1");
    }
    else {
      formControlFrame->SetProperty(nsHTMLAtoms::checked, "0");
    }
  }
  return NS_OK;     
}

NS_IMETHODIMP
nsHTMLInputElement::Blur()
{
  nsIFormControlFrame* formControlFrame = nsnull;
  nsresult rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv)) {
     // Ask the frame to Deselect focus (i.e Blur).
    formControlFrame->SetFocus(PR_FALSE, PR_TRUE);
    return NS_OK;
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::Focus()
{
  nsIFormControlFrame* formControlFrame = nsnull;
  nsresult rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv)) {
    formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
    return NS_OK;
  }
  return rv;

}

NS_IMETHODIMP
nsHTMLInputElement::SetFocus(nsIPresContext* aPresContext)
{
  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;
  if (NS_CONTENT_ATTR_HAS_VALUE == mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::disabled, disabled))
      return NS_OK;
 
  nsIEventStateManager* esm;
  if (NS_OK == aPresContext->GetEventStateManager(&esm)) {
    esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
    NS_RELEASE(esm);
  }
  
  // XXX Should focus only this presContext
  Focus();

  nsIFormControlFrame* formControlFrame = nsnull;
  nsresult rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv)) {
    formControlFrame->ScrollIntoView(aPresContext);
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::RemoveFocus(nsIPresContext* aPresContext)
{
  // XXX Should focus only this presContext
  Blur();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::Select()
{
  nsIFormControlFrame* formControlFrame = nsnull;
  nsresult rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv)) {
    if (nsnull != formControlFrame ) { 
      formControlFrame->SetProperty(nsHTMLAtoms::select, "");
      return NS_OK;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::Click()
{
  PRInt32 type;
  GetType(&type);
  switch(type) {
  case NS_FORM_INPUT_CHECKBOX:
    {
      PRBool checked;
      GetChecked(&checked);
      SetChecked(!checked);
    }
    break;

  case NS_FORM_INPUT_RADIO:
    SetChecked(PR_TRUE);
    break;

#if 0
  case NS_FORM_INPUT_BUTTON:
  case NS_FORM_INPUT_RESET:
  case NS_FORM_INPUT_SUBMIT:
    {
      nsIFormControlFrame* formControlFrame = nsnull;
      if (NS_OK == nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame)) {
        if (formControlFrame) {
          formControlFrame->MouseClicked(XXXpresContextXXX);
        }
      }
    }
    break;
#endif
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::HandleDOMEvent(nsIPresContext& aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus)
{
  // Try script event handlers first
  nsresult ret = mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);

  if ((NS_OK == ret) && (nsEventStatus_eIgnore == aEventStatus)) {
    switch (aEvent->message) {
      case NS_KEY_PRESS:
      {
        nsKeyEvent * keyEvent = (nsKeyEvent *)aEvent;
        if (keyEvent->keyCode == NS_VK_RETURN || keyEvent->charCode == 0x20) {
          PRInt32 type;
          GetType(&type);
          switch(type) {
          case NS_FORM_INPUT_CHECKBOX:
          case NS_FORM_INPUT_RADIO:
            ret = Click();
            break;
          case NS_FORM_INPUT_BUTTON:
          case NS_FORM_INPUT_RESET:
          case NS_FORM_INPUT_SUBMIT:
            {
              //Checkboxes and radio trigger off return or space but buttons
              //just trigger of of space, go figure.
              if (keyEvent->charCode == 0x20) {
                //XXX We should just be able to call Click() here but then
                //Click wouldn't have a PresContext.
                nsIFormControlFrame* formControlFrame = nsnull;
                if (NS_OK == nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame)) {
                  if (formControlFrame) {
                    formControlFrame->MouseClicked(&aPresContext);
                  }
                }
              }
            }
            break;
          }
        }
      }
    }
  }

  return ret;
}


// nsIHTMLContent

static nsGenericHTMLElement::EnumTable kInputTypeTable[] = {
  { "browse", NS_FORM_BROWSE }, // XXX not valid html, but it is convient
  { "button", NS_FORM_INPUT_BUTTON },
  { "checkbox", NS_FORM_INPUT_CHECKBOX },
  { "file", NS_FORM_INPUT_FILE },
  { "hidden", NS_FORM_INPUT_HIDDEN },
  { "reset", NS_FORM_INPUT_RESET },
  { "image", NS_FORM_INPUT_IMAGE },
  { "password", NS_FORM_INPUT_PASSWORD },
  { "radio", NS_FORM_INPUT_RADIO },
  { "submit", NS_FORM_INPUT_SUBMIT },
  { "text", NS_FORM_INPUT_TEXT },
  { 0 }
};

NS_IMETHODIMP
nsHTMLInputElement::StringToAttribute(nsIAtom* aAttribute,
                                      const nsString& aValue,
                                      nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::type) {
    nsGenericHTMLElement::EnumTable *table = kInputTypeTable;
    while (nsnull != table->tag) { 
      if (aValue.EqualsIgnoreCase(table->tag)) {
        aResult.SetIntValue(table->value, eHTMLUnit_Enumerated);
        mType = table->value;  // set the type of this input 
        return NS_CONTENT_ATTR_HAS_VALUE;
      }
      table++;
    }
  }
  else if (aAttribute == nsHTMLAtoms::checked) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::disabled) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::readonly) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::width) {
    if (nsGenericHTMLElement::ParseValueOrPercent(aValue, aResult,
                                                  eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  } 
  else if (aAttribute == nsHTMLAtoms::height) {
    if (nsGenericHTMLElement::ParseValueOrPercent(aValue, aResult,
                                                  eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  } 
  else if (aAttribute == nsHTMLAtoms::maxlength) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::size) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::tabindex) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (IsImage()) {
    if (nsGenericHTMLElement::ParseImageAttribute(aAttribute,
                                                  aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::border) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLInputElement::AttributeToString(nsIAtom* aAttribute,
                                      const nsHTMLValue& aValue,
                                      nsString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::type) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      nsGenericHTMLElement::EnumValueToString(aValue, kInputTypeTable, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (IsImage() &&
           nsGenericHTMLElement::ImageAttributeToString(aAttribute,
                                                        aValue, aResult)) {
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
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

  aAttributes->GetAttribute(nsHTMLAtoms::type, value);
  if (eHTMLUnit_Enumerated == value.GetUnit()) {  
    switch (value.GetIntValue()) {
//XXX when there exists both a Standard and Quirks ua.css, remove this code 
//XXX it may be needed again if we don't have 2 ua.css files
//XXX this is now handled by attribute selectors in ua.css
#if 0
    case NS_FORM_INPUT_CHECKBOX:
      case NS_FORM_INPUT_RADIO:
      {
        float p2t;
        aPresContext->GetScaledPixelsToTwips(&p2t);
        nscoord pad = NSIntPixelsToTwips(3, p2t);

        // add left and right padding around the radio button via css
        nsStyleSpacing* spacing = (nsStyleSpacing*) aContext->GetMutableStyleData(eStyleStruct_Spacing);
        if (eStyleUnit_Null == spacing->mMargin.GetLeftUnit()) {
          nsStyleCoord left(pad);
          spacing->mMargin.SetLeft(left);
        }
        if (eStyleUnit_Null == spacing->mMargin.GetRightUnit()) {
          nsStyleCoord right(NSIntPixelsToTwips(5, p2t));
          spacing->mMargin.SetRight(right);
        }
        // add bottom padding if backward mode
        // XXX why isn't this working?
        nsCompatibility mode;
        aPresContext->GetCompatibilityMode(&mode);
        if (eCompatibility_NavQuirks == mode) {
          if (eStyleUnit_Null == spacing->mMargin.GetBottomUnit()) {
            nsStyleCoord bottom(pad);
            spacing->mMargin.SetBottom(bottom);
          }
        }
        break;
      }
#endif
      case NS_FORM_INPUT_IMAGE:
      {
        nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aContext, aPresContext, nsnull);
        nsGenericHTMLElement::MapImageAttributesInto(aAttributes, aContext, aPresContext);
        break;
      }
    }
  }
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLInputElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                             PRInt32& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::align)) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if ((aAttribute == nsHTMLAtoms::type)) {
    aHint = NS_STYLE_HINT_FRAMECHANGE;
  }
  else if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (! nsGenericHTMLElement::GetImageMappedAttributesImpact(aAttribute, aHint)) {
      if (! nsGenericHTMLElement::GetImageBorderAttributeImpact(aAttribute, aHint)) {
        aHint = NS_STYLE_HINT_CONTENT;
      }
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLInputElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                 nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}

// nsIFormControl

// An important assumption is that if aForm is null, the previous mForm will not be released
// This allows nsHTMLFormElement to deal with circular references.
NS_IMETHODIMP
nsHTMLInputElement::SetForm(nsIDOMHTMLFormElement* aForm)
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

NS_IMETHODIMP
nsHTMLInputElement::GetType(PRInt32* aType)
{
  if (aType) {
    *aType = mType;
    return NS_OK;
  } else {
    return NS_FORM_NOTOK;
  }
}


