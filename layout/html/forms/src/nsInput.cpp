/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsInput.h"
#include "nsIFormManager.h"
#include "nsInputFrame.h"
#include "nsHTMLParts.h"
#include "nsHTMLContainer.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsCoord.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "nsHTMLForms.h"
#include "nsStyleConsts.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsITextWidget.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"

#define ALIGN_UNSET PRUint8(-1)

static NS_DEFINE_IID(kSupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);

// Note: we inherit a base class operator new that zeros our memory
nsInput::nsInput(nsIAtom* aTag, nsIFormManager* aManager)
  : nsHTMLContainer(aTag), mControl()
{
  mFormMan = aManager;
  if (nsnull != mFormMan) {
    NS_ADDREF(mFormMan);
    mFormMan->AddFormControl(&mControl);
  }
  mSize  = ATTR_NOTSET; 
  mAlign = ALIGN_UNSET;
  mLastClickPoint.x = -1;
  mLastClickPoint.y = -1;
  mCanSubmit = PR_FALSE;
}

nsInput::~nsInput()
{
  NS_IF_RELEASE(mWidget);
  NS_IF_RELEASE(mWidgetSupports);
  if (nsnull != mName) {
    delete mName;
  }
  if (nsnull != mValue) {
    delete mValue;
  }
  if (nsnull != mFormMan) {
    // prevent mFormMan from decrementing its ref count on us
    mFormMan->RemoveFormControl(&mControl, PR_FALSE); 
    NS_RELEASE(mFormMan);
  }
}

void nsInput::SetClickPoint(nscoord aX, nscoord aY)
{
  mLastClickPoint.x = aX;
  mLastClickPoint.y = aY;
}

PRBool nsInput::GetCanSubmit() const
{
  return mCanSubmit;
}

void nsInput::SetCanSubmit(PRBool aFlag)
{
  mCanSubmit = aFlag;
}

void nsInput::SetContent(const nsString& aValue)
{
  if (nsnull == mValue) {
    mValue = new nsString(aValue);
  } else {
    *mValue = aValue;
  }
}

void
nsInput::MapAttributesInto(nsIHTMLAttributes* aAttributes,
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
  nsGenericHTMLElement::MapImageAttributesInto(aAttributes, aContext, aPresContext);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsInput::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &nsInput::MapAttributesInto;
  return NS_OK;
}


static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);

nsresult nsInput::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aIID.Equals(kIFormControlIID)) {
    AddRef();
    *aInstancePtr = (void*) (nsIFormControl *)&mControl;
    return NS_OK;
  }
  if (aIID.Equals(kIDOMHTMLInputElementIID)) {
    AddRef();
    *aInstancePtr = (void*) (nsIDOMHTMLInputElement *)this;
    return NS_OK;
  }
  return nsHTMLContainer::QueryInterface(aIID, aInstancePtr);
}

nsrefcnt nsInput::AddRef(void)
{
  PRInt32 refCnt = mRefCnt;  // debugging 
  return nsHTMLContainer::AddRef(); 
}

PRBool nsInput::IsSuccessful(nsIFormControl* aSubmitter) const
{
  if (nsnull == mName) {
    return PR_FALSE;
  }
  else {
    return PR_TRUE;
  }
}

nsrefcnt nsInput::Release()
{
  --mRefCnt;
	if (mRefCnt <= 0) {
    delete this;                                       
    return 0;                                          
  }
  if ((mFormMan == nsnull) || (mRefCnt > 1)) {
    return mRefCnt;
  }

 	int numSiblings = mFormMan->GetFormControlCount();
  PRBool externalRefs = PR_FALSE;  // are there external refs to dad or any siblings
  if ((int)mFormMan->GetRefCount() > numSiblings) {
    externalRefs = PR_TRUE;
  } else {
    for (int i = 0; i < numSiblings; i++) {
      nsIFormControl* sibling = mFormMan->GetFormControlAt(i);
      if (sibling->GetRefCount() > 1) {
        externalRefs = PR_TRUE;
        break;
      }
    }
  }

  if (!externalRefs) {
    mRefCnt = 0;
    delete this;                                       
    return 0;                                          
  } 
  
  return mRefCnt;
}
  
PRBool 
nsInput::IsHidden()
{
  return PR_FALSE;
}

void 
nsInput::SetWidget(nsIWidget* aWidget)
{
  if (aWidget != mWidget) {
	  NS_IF_RELEASE(mWidget);
    NS_IF_ADDREF(aWidget);
    mWidget = aWidget;

    NS_IF_RELEASE(mWidgetSupports);
    mWidget->QueryInterface(kSupportsIID, (void**)&mWidgetSupports);
  }
}

nsrefcnt nsInput::GetRefCount() const 
{
  return mRefCnt;
}

// these do not AddRef
nsIWidget* nsInput::GetWidget()
{
  return mWidget;
}
nsISupports* nsInput::GetWidgetSupports()
{
  return mWidgetSupports;
}

PRBool nsInput::GetContent(nsString& aResult) const
{
  if (nsnull == mValue) {
    aResult.SetLength(0);
    return PR_FALSE;
  } else {
    aResult = *mValue;
    return PR_TRUE;
  }
}

void 
nsInput::SetFormManager(nsIFormManager* aFormMan, PRBool aDecrementRef)
{
	if (aDecrementRef) {
    NS_IF_RELEASE(mFormMan);
	}
  mFormMan = aFormMan;
  NS_IF_ADDREF(aFormMan);
}

nsIFormManager* 
nsInput::GetFormManager() const
{
  NS_IF_ADDREF(mFormMan);
  return mFormMan;
}

/**
 * Get the name associated with this form element. 
 * (note that form elements without names are not submitable).
 */
NS_IMETHODIMP
nsInput::GetName(nsString& aName)
{
  if ((nsnull != mName) && (0 != mName->Length())) {
    aName = *mName;
  }
  else {
    aName.SetLength(0);
  }

  return NS_OK;
}

void
nsInput::Reset()
{
}

PRInt32
nsInput::GetMaxNumValues()
{
  return 0;
}

PRBool
nsInput::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues, 
                        nsString* aValues, nsString* aNames)
{
  aNumValues = 0;
  return PR_FALSE;
}

void nsInput::CacheAttribute(const nsString& aValue, nsString*& aLoc)
{
  if (nsnull == aLoc) {
    aLoc = new nsString(aValue);
  } else {
    aLoc->SetLength(0);
    aLoc->Append(aValue);
  }
}

void nsInput::CacheAttribute(const nsString& aValue, PRInt32 aMinValue, PRInt32& aLoc)
{
  PRInt32 status;
  PRInt32 intVal = aValue.ToInteger(&status);
  aLoc = ((NS_OK == status) && (intVal >= aMinValue)) ? intVal : aMinValue;
}

NS_IMETHODIMP
nsInput::SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                      PRBool aNotify)
{
  if (aAttribute == nsHTMLAtoms::type) { // You cannot set the type of a form element
    ;
  } 
  else if (aAttribute == nsHTMLAtoms::name) {
    CacheAttribute(aValue, mName);
  } 
  else if (aAttribute == nsHTMLAtoms::size) {
    CacheAttribute(aValue, ATTR_NOTSET, mSize);
  } 
  else if (aAttribute == nsHTMLAtoms::value) {
    CacheAttribute(aValue, mValue);
  } 
  else if (aAttribute == nsHTMLAtoms::align) {
    nsHTMLValue val;
    if (ParseAlignParam(aValue, val)) {
      mAlign = val.GetIntValue();
      // Reflect the attribute into the syle system
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);  // is this needed? YES
    } else {
      mAlign = ALIGN_UNSET;
    }
  }

  // XXX the following is necessary so that MapAttributesInto gets called
  return nsInputSuper::SetAttribute(aAttribute, aValue, aNotify);
}

nsresult nsInput::GetCacheAttribute(nsString* const& aLoc, nsHTMLValue& aValue) const
{
  aValue.Reset();
  if (nsnull == aLoc) {
    return NS_CONTENT_ATTR_NOT_THERE;
  } 
  else {
    aValue.SetStringValue(*aLoc);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
}

nsresult
nsInput::GetCacheAttribute(PRInt32 aLoc, nsHTMLValue& aValue,
                           nsHTMLUnit aUnit) const
{
  aValue.Reset();
  if (aLoc <= ATTR_NOTSET) {
    return NS_CONTENT_ATTR_NOT_THERE;
  } 
  else {
    if (eHTMLUnit_Pixel == aUnit) {
      aValue.SetPixelValue(aLoc);
    }
    else if (eHTMLUnit_Empty == aUnit) {
      if (PRBool(aLoc)) {
        aValue.SetEmptyValue();
      }
      else {
        return NS_CONTENT_ATTR_NOT_THERE;
      }
    }
    else {
      aValue.SetIntValue(aLoc, aUnit);
    }

    return NS_CONTENT_ATTR_HAS_VALUE;
  }
}

NS_IMETHODIMP
nsInput::GetAttribute(nsIAtom* aAttribute, PRInt32& aValue) const
{
  nsHTMLValue htmlValue;
  nsresult result = GetAttribute(aAttribute, htmlValue);
  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    if (eHTMLUnit_Empty == htmlValue.GetUnit()) {
      aValue = 1;
    }
    else if (eHTMLUnit_Pixel == htmlValue.GetUnit()) {
      aValue = htmlValue.GetPixelValue();
    }
    else {
      aValue = htmlValue.GetIntValue();
    }
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else {
    aValue = ATTR_NOTSET;
    // XXX for bool values, this should return 0
    return NS_CONTENT_ATTR_NO_VALUE;
  }
}

NS_IMETHODIMP
nsInput::GetAttribute(nsIAtom* aAttribute,
                      nsHTMLValue& aValue) const
{
  if (aAttribute == nsHTMLAtoms::type) {
    nsAutoString tmp;
    GetType(tmp);
    if (tmp.Length() == 0) {    // derivatives that don't support type return zero length string
      return NS_CONTENT_ATTR_NOT_THERE;
    }
    else {
      aValue.SetStringValue(tmp);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  } 
  else if (aAttribute == nsHTMLAtoms::name) {
    return GetCacheAttribute(mName, aValue);
  } 
  else if (aAttribute == nsHTMLAtoms::size) {
    return GetCacheAttribute(mSize, aValue, eHTMLUnit_Pixel); // XXX pixel or percent??
  }
  else if (aAttribute == nsHTMLAtoms::value) {
    return GetCacheAttribute(mValue, aValue);
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    return GetCacheAttribute(mAlign, aValue, eHTMLUnit_Enumerated);
  }
  else {
    return nsInputSuper::GetAttribute(aAttribute, aValue);
  }
}

PRBool nsInput::GetChecked(PRBool aGetInitialValue) const
{
  return PR_FALSE;
}

void nsInput::SetChecked(PRBool aState, PRBool aSetInitialValue)
{
}

NS_IMETHODIMP    
nsInput::GetDefaultValue(nsString& aDefaultValue)
{
  ((nsHTMLContainer *)this)->GetAttribute(nsHTMLAtoms::value, aDefaultValue);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetDefaultValue(const nsString& aDefaultValue)
{
  ((nsHTMLContainer *)this)->SetAttribute(nsHTMLAtoms::value, aDefaultValue, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetDefaultChecked(PRBool* aDefaultChecked)
{
  *aDefaultChecked = GetChecked(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetDefaultChecked(PRBool aDefaultChecked)
{
  SetChecked(aDefaultChecked, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetForm(nsIDOMHTMLFormElement** aForm)
{
  static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
  if (nsnull != mFormMan) {
    return mFormMan->QueryInterface(kIDOMHTMLFormElementIID, (void **)aForm);
  }
  
  *aForm = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetAccept(nsString& aAccept)
{
  ((nsHTMLContainer *)this)->GetAttribute(nsHTMLAtoms::accept, aAccept);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetAccept(const nsString& aAccept)
{
  ((nsHTMLContainer *)this)->SetAttribute(nsHTMLAtoms::accept, aAccept, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetAccessKey(nsString& aAccessKey)
{
  ((nsHTMLContainer *)this)->GetAttribute(nsHTMLAtoms::accesskey, aAccessKey);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetAccessKey(const nsString& aAccessKey)
{
  ((nsHTMLContainer *)this)->SetAttribute(nsHTMLAtoms::accesskey, aAccessKey, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetAlign(nsString& aAlign)
{
  ((nsHTMLContainer *)this)->GetAttribute(nsHTMLAtoms::align, aAlign);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetAlign(const nsString& aAlign)
{
  ((nsHTMLContainer *)this)->SetAttribute(nsHTMLAtoms::align, aAlign, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetAlt(nsString& aAlt)
{
  ((nsHTMLContainer *)this)->GetAttribute(nsHTMLAtoms::alt, aAlt);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetAlt(const nsString& aAlt)
{
  ((nsHTMLContainer *)this)->SetAttribute(nsHTMLAtoms::alt, aAlt, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetChecked(PRBool* aChecked)
{
  *aChecked = GetChecked(PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetChecked(PRBool aChecked)
{
  SetChecked(aChecked, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetDisabled(PRBool* aDisabled)
{
  nsAutoString result;

  *aDisabled = (PRBool)(NS_CONTENT_ATTR_HAS_VALUE == ((nsHTMLContainer *)this)->GetAttribute(nsHTMLAtoms::disabled, result)); 
  
  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetDisabled(PRBool aDisabled)
{
  if (PR_TRUE == aDisabled) {
    ((nsHTMLContainer *)this)->SetAttribute(nsHTMLAtoms::disabled, "", PR_TRUE);
  }
  else {
    UnsetAttribute(nsHTMLAtoms::disabled);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetMaxLength(PRInt32* aMaxLength)
{
  nsHTMLValue val;

  ((nsHTMLContainer *)this)->GetAttribute(nsHTMLAtoms::maxlength, val);
  *aMaxLength = val.GetIntValue();

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetMaxLength(PRInt32 aMaxLength)
{
  nsHTMLValue val;
  
  val.SetIntValue(aMaxLength, eHTMLUnit_Integer);
  ((nsHTMLTagContent *)this)->SetAttribute(nsHTMLAtoms::maxlength, val, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetName(const nsString& aName)
{
  ((nsHTMLContainer *)this)->SetAttribute(nsHTMLAtoms::name, aName, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetReadOnly(PRBool* aReadOnly)
{
  nsAutoString result;

  *aReadOnly = (PRBool)(NS_CONTENT_ATTR_HAS_VALUE == ((nsHTMLContainer *)this)->GetAttribute(nsHTMLAtoms::readonly, result)); 

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetReadOnly(PRBool aReadOnly)
{
  if (PR_TRUE == aReadOnly) {
    ((nsHTMLContainer *)this)->SetAttribute(nsHTMLAtoms::readonly, "", PR_TRUE);
  }
  else {
    UnsetAttribute(nsHTMLAtoms::readonly);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetSize(nsString& aSize)
{
  ((nsHTMLContainer *)this)->GetAttribute(nsHTMLAtoms::size, aSize);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetSize(const nsString& aSize)
{
  ((nsHTMLContainer *)this)->SetAttribute(nsHTMLAtoms::size, aSize, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetSrc(nsString& aSrc)
{
  ((nsHTMLContainer *)this)->GetAttribute(nsHTMLAtoms::src, aSrc);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetSrc(const nsString& aSrc)
{
  ((nsHTMLContainer *)this)->SetAttribute(nsHTMLAtoms::src, aSrc, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetTabIndex(PRInt32* aTabIndex)
{
  nsHTMLValue val;

  ((nsHTMLContainer *)this)->GetAttribute(nsHTMLAtoms::tabindex, val);
  *aTabIndex = val.GetIntValue();

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetTabIndex(PRInt32 aTabIndex)
{
  nsHTMLValue val;
  
  val.SetIntValue(aTabIndex, eHTMLUnit_Integer);
  ((nsHTMLTagContent *)this)->SetAttribute(nsHTMLAtoms::tabindex, val, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetType(nsString& aType)
{
  ((nsHTMLContainer *)this)->GetAttribute(nsHTMLAtoms::type, aType);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetUseMap(nsString& aUseMap)
{
  ((nsHTMLContainer *)this)->GetAttribute(nsHTMLAtoms::usemap, aUseMap);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetUseMap(const nsString& aUseMap)
{
  ((nsHTMLContainer *)this)->SetAttribute(nsHTMLAtoms::usemap, aUseMap, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::GetValue(nsString& aValue)
{
  ((nsHTMLContainer *)this)->GetAttribute(nsHTMLAtoms::value, aValue);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::SetValue(const nsString& aValue)
{
  ((nsHTMLContainer *)this)->SetAttribute(nsHTMLAtoms::value, aValue, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsInput::Blur()
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
nsInput::Focus()
{
  if (nsnull != mWidget) {
    mWidget->SetFocus();
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsInput::Select()
{
  if (nsnull != mWidget) {
    nsITextWidget *mTextWidget;
    if (NS_OK == mWidget->QueryInterface(kITextWidgetIID, (void**)&mTextWidget)) {
      mTextWidget->SelectAll();
      NS_RELEASE(mTextWidget);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsInput::Click()
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsInput::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptHTMLInputElement(aContext, (nsISupports *)(nsIDOMHTMLInputElement *)this, mParent, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;  
}

//----------------------------------------------------------------------

#define GET_OUTER() ((nsInput*) ((char*)this - nsInput::GetOuterOffset()))

nsInput::AggInputControl::AggInputControl()
{
}

nsInput::AggInputControl::~AggInputControl()
{
}

nsrefcnt nsInput::AggInputControl::AddRef()
{
  return GET_OUTER()->AddRef();
}

nsrefcnt nsInput::AggInputControl::Release()
{
  return GET_OUTER()->Release();
}

nsresult nsInput::AggInputControl::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  return GET_OUTER()->QueryInterface(aIID, aInstancePtr);
}

nsresult nsInput::AggInputControl::GetName(nsString& aName)
{
  return GET_OUTER()->GetName(aName);
}

PRInt32 nsInput::AggInputControl::GetMaxNumValues()
{
  return GET_OUTER()->GetMaxNumValues();
}

PRBool nsInput::AggInputControl::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                                nsString* aValues, nsString* aNames)
{
  return GET_OUTER()->GetNamesValues(aMaxNumValues, aNumValues, aValues, aNames);
}

void nsInput::AggInputControl::Reset()
{
  GET_OUTER()->Reset();
}

void nsInput::AggInputControl::SetFormManager(nsIFormManager* aFormMan, PRBool aDecrementRef)
{
  GET_OUTER()->SetFormManager(aFormMan, aDecrementRef);
}

nsIFormManager* nsInput::AggInputControl::GetFormManager() const
{
  return GET_OUTER()->GetFormManager();
}

nsrefcnt nsInput::AggInputControl::GetRefCount() const
{
  return GET_OUTER()->GetRefCount();
}

PRBool nsInput::AggInputControl::GetChecked(PRBool aGetInitialValue) const
{
  return GET_OUTER()->GetChecked(aGetInitialValue);
}

void nsInput::AggInputControl::SetChecked(PRBool aState, PRBool aSetInitialValue)
{
  GET_OUTER()->SetChecked(aState, aSetInitialValue);
}

void nsInput::AggInputControl::GetType(nsString& aName) const
{
  GET_OUTER()->GetType(aName);
}

PRBool nsInput::AggInputControl::IsSuccessful(nsIFormControl* aSubmitter) const
{
  return GET_OUTER()->IsSuccessful(aSubmitter);
}

void nsInput::AggInputControl::SetContent(const nsString& aValue)
{
  GET_OUTER()->SetContent(aValue);
}

PRBool nsInput::AggInputControl::GetContent(nsString& aResult) const
{
  return GET_OUTER()->GetContent(aResult);
}

void nsInput::AggInputControl::SetCanSubmit(PRBool aFlag)
{
  GET_OUTER()->SetCanSubmit(aFlag);
}

PRBool nsInput::AggInputControl::GetCanSubmit() const
{
  return GET_OUTER()->GetCanSubmit();
}
