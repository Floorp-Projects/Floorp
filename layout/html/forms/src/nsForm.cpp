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
#include "nsHTMLForms.h"
#include "nsIFormManager.h"
#include "nsIFormControl.h"
#include "nsIAtom.h"
#include "nsHTMLIIDs.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "nsVoidArray.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLAttributes.h"
#include "nsCRT.h"

// netlib has a general function (netlib\modules\liburl\src\escape.c) 
// which does url encoding. Since netlib is not yet available for raptor,
// the following will suffice. Convert space to +, don't convert alphanumeric,
// conver each non alphanumeric char to %XY where XY is the hexadecimal 
// equavalent of the binary representation of the character. 
// 
void EscapeURLString(char* aInString, char* aOutString)
{
  if (nsnull == aInString) {
	return;
  }
  static char *toHex = "0123456789ABCDEF";
  char* outChar = aOutString;
  for (char* inChar = aInString; *inChar; inChar++) {
    if(' ' == *inChar) {                                     // convert space to +
	  *outChar++ = '+';
	} else if ( (((*inChar - '0') >= 0) && (('9' - *inChar) >= 0)) || // don't conver 
                (((*inChar - 'a') >= 0) && (('z' - *inChar) >= 0)) || // alphanumeric
				(((*inChar - 'A') >= 0) && (('Z' - *inChar) >= 0)) ) {
	  *outChar++ = *inChar;
	} else {                                                 // convert all else to hex
	  *outChar++ = '%';
      *outChar++ = toHex[(*inChar >> 4) & 0x0F];
      *outChar++ = toHex[*inChar & 0x0F];
	}
  }
  *outChar = 0;  // terminate the string
}

nsString* EscapeURLString(nsString& aString) 
{  
  char* inBuf  = aString.ToNewCString();
	char* outBuf = new char[ (strlen(inBuf) * 3) + 1 ];
	EscapeURLString(inBuf, outBuf);
	nsString* result = new nsString(outBuf);
	delete [] outBuf;
	delete [] inBuf;
	return result;
}


//----------------------------------------------------------------------

static NS_DEFINE_IID(kIFormManagerIID, NS_IFORMMANAGER_IID);

class nsForm : public nsIFormManager
{
public:
  // Construct a new Form Element with no attributes. This needs to be
  // made private and have a static COM create method.
  nsForm(nsIAtom* aTag);
  ~nsForm();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  // callback for reset button controls. 
  virtual void OnReset();

  // callback for text and textarea controls. If there is a single
  // text/textarea and a return is entered, then this is equavalent to
  // a submit.
  virtual void OnReturn();

  // callback for submit button controls.
  virtual void OnSubmit();

  // callback for tabs on controls that can gain focus. This will
  // eventually need to be handled at the document level to support
  // the tabindex attribute.
  virtual void OnTab();

  virtual PRInt32 GetFormControlCount() const;
  virtual nsIFormControl* GetFormControlAt(PRInt32 aIndex) const;
  virtual PRBool AddFormControl(nsIFormControl* aFormControl);
  virtual PRBool RemoveFormControl(nsIFormControl* aFormControl, 
                                   PRBool aChildIsRef = PR_TRUE);

  virtual void SetAttribute(const nsString& aName, const nsString& aValue);

  virtual PRBool GetAttribute(const nsString& aName,
                              nsString& aResult) const;

  virtual nsresult GetRefCount() const;

protected:
  nsIAtom* mTag;
  nsIHTMLAttributes* mAttributes;
  nsVoidArray mChildren;
  nsString* mAction;
  nsString* mEncoding;
  nsString* mTarget;
  PRInt32 mMethod;
};

#define METHOD_UNSET    0
#define METHOD_GET      1
#define METHOD_POST     2

// CLASS nsForm

// Note: operator new zeros our memory
nsForm::nsForm(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mTag = aTag;
  NS_IF_ADDREF(aTag);
}

nsForm::~nsForm()
{
  NS_IF_RELEASE(mTag);
  int numChildren = GetFormControlCount();
  for (int i = 0; i < numChildren; i++) {
    nsIFormControl* child = GetFormControlAt(i);
    RemoveFormControl(child, PR_FALSE);
    child->SetFormManager(nsnull, PR_FALSE);
    NS_RELEASE(child);
  }
  if (nsnull != mAction) delete mAction;
  if (nsnull != mEncoding) delete mEncoding;
  if (nsnull != mTarget) delete mTarget;
}

NS_IMPL_QUERY_INTERFACE(nsForm,kIFormManagerIID);
NS_IMPL_ADDREF(nsForm);

nsrefcnt nsForm::GetRefCount() const
{
  return mRefCnt;
}

nsrefcnt nsForm::Release()
{
  --mRefCnt;
  int numChildren = GetFormControlCount();
  PRBool externalRefsToChildren = PR_FALSE;  // are there refs to any children besides me
  for (int i = 0; i < numChildren; i++) {
    nsIFormControl* child = GetFormControlAt(i);
    if (child->GetRefCount() > 1) {
      externalRefsToChildren = PR_TRUE;
      break;
    }
  }
  if (!externalRefsToChildren && ((int)mRefCnt == numChildren)) {
    mRefCnt = 0;
    delete this; 
    return 0;
  } 
  return mRefCnt;
}

PRInt32 
nsForm::GetFormControlCount() const 
{ 
  return mChildren.Count(); 
}

nsIFormControl* 
nsForm::GetFormControlAt(PRInt32 aIndex) const 
{ 
  nsIFormControl* ctl = (nsIFormControl*) mChildren.ElementAt(aIndex);
  NS_IF_ADDREF(ctl);
  return ctl;
}

PRBool 
nsForm::AddFormControl(nsIFormControl* aChild) 
{ 
  PRBool rv = mChildren.AppendElement(aChild);
  if (rv) {
    NS_ADDREF(aChild);
  }
  return rv;
}

PRBool 
nsForm::RemoveFormControl(nsIFormControl* aChild, PRBool aChildIsRef) 
{ 
  PRBool rv = mChildren.RemoveElement(aChild);
  if (rv && aChildIsRef) {
    NS_RELEASE(aChild);
  }
  return rv;
}

void 
nsForm::OnReset()
{
  PRInt32 numChildren = mChildren.Count();
  for (int childX = 0; childX < numChildren; childX++) {
	  nsIFormControl* child = (nsIFormControl*) mChildren.ElementAt(childX);
	  child->Reset();
	}
}

void 
nsForm::OnReturn()
{
}

void 
nsForm::OnSubmit()
{
  nsString data(""); // this could be more efficient, by allocating a larger buffer
  PRBool firstTime = PR_TRUE;

  PRInt32 numChildren = mChildren.Count();
  for (PRInt32 childX = 0; childX < numChildren; childX++) {
	  nsIFormControl* child = (nsIFormControl*) mChildren.ElementAt(childX);
	  nsString childName;
		if (PR_TRUE == child->GetName(childName)) {
		  PRInt32 numValues = 0;
		  PRInt32 maxNumValues = child->GetMaxNumValues();
			if (maxNumValues <= 0) {
				continue;
			}
		  nsString* values = new nsString[maxNumValues];
			if (PR_TRUE == child->GetValues(maxNumValues, numValues, values)) {
				for (int valueX = 0; valueX < numValues; valueX++) {
				  if (PR_TRUE == firstTime) {
					  firstTime = PR_FALSE;
				  } else {
				    data += "&";
				  }
					nsString* convName = EscapeURLString(childName);
				  data += *convName;
					delete convName;
					data += "=";
					nsString* convValue = EscapeURLString(values[valueX]);
					data += *convValue;
					delete convValue;
				}
			}
			delete [] values;
		}
	}
  char* out = data.ToNewCString();
	printf("\nsubmit data =\n%s\n", out);
	delete [] out;
}

void 
nsForm::OnTab()
{
}

void nsForm::SetAttribute(const nsString& aName, const nsString& aValue)
{
  nsAutoString tmp(aName);
  tmp.ToUpperCase();
  nsIAtom* atom = NS_NewAtom(aName);

  if (atom == nsHTMLAtoms::action) {
    nsAutoString url(aValue);
    url.StripWhitespace();
    if (nsnull == mAction) {
      mAction = new nsString(url);
    }
    else {
      *mAction = url;
    }
  }
  else if (atom == nsHTMLAtoms::encoding) {
    if (nsnull == mEncoding) {
      mEncoding = new nsString(aValue);
    }
    else {
      *mEncoding = aValue;
    }
  }
  else if (atom == nsHTMLAtoms::target) {
    if (nsnull == mTarget) {
      mTarget = new nsString(aValue);
    }
    else {
      *mTarget = aValue;
    }
  }
  else if (atom == nsHTMLAtoms::method) {
    if (aValue.EqualsIgnoreCase("post")) {
      mMethod = METHOD_POST;
    }
    else {
      mMethod = METHOD_GET;
    }
  }
  else {
    // Use default storage for unknown attributes
    if (nsnull == mAttributes) {
      NS_NewHTMLAttributes(&mAttributes, nsnull);
    }
    if (nsnull != mAttributes) {
      mAttributes->SetAttribute(atom, aValue);
    }
  }
  NS_RELEASE(atom);
}

PRBool nsForm::GetAttribute(const nsString& aName,
                            nsString& aResult) const
{
  nsAutoString tmp(aName);
  tmp.ToUpperCase();
  nsIAtom* atom = NS_NewAtom(aName);
  PRBool rv = PR_FALSE;
  if (atom == nsHTMLAtoms::action) {
    if (nsnull != mAction) {
      aResult = *mAction;
      rv = PR_TRUE;
    }
  }
  else if (atom == nsHTMLAtoms::encoding) {
    if (nsnull != mEncoding) {
      aResult = *mEncoding;
      rv = PR_TRUE;
    }
  }
  else if (atom == nsHTMLAtoms::target) {
    if (nsnull != mTarget) {
      aResult = *mTarget;
      rv = PR_TRUE;
    }
  }
  else if (atom == nsHTMLAtoms::method) {
    if (METHOD_UNSET != mMethod) {
      if (METHOD_POST == mMethod) {
        aResult = "post";
      }
      else {
        aResult = "get";
      }
      rv = PR_TRUE;
    }
  }
  else {
    // Use default storage for unknown attributes
    if (nsnull != mAttributes) {
      nsHTMLValue value;
      if (eContentAttr_HasValue == mAttributes->GetAttribute(atom, value)) {
        if (value.GetUnit() == eHTMLUnit_String) {
          value.GetStringValue(aResult);
          rv = PR_TRUE;
        }
      }
    }
  }

  NS_RELEASE(atom);
  return rv;
}

nsresult
NS_NewHTMLForm(nsIFormManager** aInstancePtrResult,
               nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsForm* it = new nsForm(aTag);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult result = it->QueryInterface(kIFormManagerIID, (void**) aInstancePtrResult);
  return result;
}
