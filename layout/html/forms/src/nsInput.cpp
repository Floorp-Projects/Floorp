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
#include "nsHTMLTagContent.h"
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

// Note: we inherit a base class operator new that zeros our memory
nsInput::nsInput(nsIAtom* aTag, nsIFormManager* aManager)
  : nsHTMLTagContent(aTag), mControl()
{
printf("** nsInput::nsInput this=%d\n", this);
  mFormMan = aManager;
  if (nsnull != mFormMan) {
    NS_ADDREF(mFormMan);
    mFormMan->AddFormControl(&mControl);
  }
}

nsInput::~nsInput()
{
printf("** nsInput::~nsInput()\n");
  NS_IF_RELEASE(mWidget);
  if (nsnull != mName) {
    delete mName;
  }
  if (nsnull != mFormMan) {
    // prevent mFormMan from decrementing its ref count on us
    mFormMan->RemoveFormControl(&mControl, PR_FALSE); 
    NS_RELEASE(mFormMan);
  }
}

static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);

nsresult nsInput::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aIID.Equals(kIFormControlIID)) {
    AddRef();
    *aInstancePtr = (void**) &mControl;
    return NS_OK;
  }
  return nsHTMLTagContent::QueryInterface(aIID, aInstancePtr);
}

nsrefcnt nsInput::Release()
{
  --mRefCnt;
  int debugRefCnt = mRefCnt;
	if (mRefCnt == 0) {
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
  

nsIFrame*
nsInput::CreateFrame(nsIPresContext *aPresContext,
                               PRInt32 aIndexInParent,
                               nsIFrame *aParentFrame)
{
  NS_ASSERTION(0, "frames must be created by subclasses of Input");
  return nsnull;
}

void 
nsInput::SetWidget(nsIWidget* aWidget)
{
  if (aWidget != mWidget) {
	  NS_IF_RELEASE(mWidget);
    NS_IF_ADDREF(aWidget);
    mWidget = aWidget;
  }
}

nsrefcnt nsInput::GetRefCount() const 
{
  return mRefCnt;
}

// this is for internal use and does not do an AddRef
nsIWidget* 
nsInput::GetWidget()
{
  return mWidget;
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
 * Get the name associated with this form element. If there is no name
 * then return PR_FALSE (form elements without names are not submitable).
 */
PRBool
nsInput::GetName(nsString& aName)
{
  if ((nsnull != mName) && (0 != mName->Length())) {
    aName = *mName;
    return PR_TRUE;
  }
  return PR_FALSE;
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
nsInput::GetValues(PRInt32 aMaxNumValues, PRInt32& aNumValues, nsString* aValues)
{
  aNumValues = 0;
  return PR_FALSE;
}

void nsInput::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  if (aAttribute == nsHTMLAtoms::type) {
    // You cannot set the type of a form element
    return;
  }
  else if (aAttribute == nsHTMLAtoms::name) {
    if (nsnull == mName) {
      mName = new nsString(aValue);
    } else {
      mName->SetLength(0);
      mName->Append(aValue);
    }
  }
}

nsContentAttr nsInput::GetAttribute(nsIAtom* aAttribute,
                                    nsHTMLValue& aValue) const
{
  nsContentAttr ca = eContentAttr_NotThere;
  if (aAttribute == nsHTMLAtoms::type) {
    nsAutoString tmp;
    GetType(tmp);
    aValue.Set(tmp);
    ca = eContentAttr_HasValue;
  }
  else if (aAttribute == nsHTMLAtoms::name) {
    aValue.Reset();
    if (nsnull != mName) {
      aValue.Set(*mName);
      ca = eContentAttr_HasValue;
    }
  }
  else {
    ca = nsHTMLTagContent::GetAttribute(aAttribute, aValue);
  }
  return ca;
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

PRBool nsInput::AggInputControl::GetName(nsString& aName)
{
  return GET_OUTER()->GetName(aName);
}

PRInt32 nsInput::AggInputControl::GetMaxNumValues()
{
  return GET_OUTER()->GetMaxNumValues();
}

PRBool nsInput::AggInputControl::GetValues(PRInt32 aMaxNumValues,
                                           PRInt32& aNumValues,
                                           nsString* aValues)
{
  return GET_OUTER()->GetValues(aMaxNumValues, aNumValues, aValues);
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
