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

#include "nsInputHidden.h"
#include "nsInputFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"

class nsInputHiddenFrame : public nsInputFrame {
public:
  nsInputHiddenFrame(nsIContent* aContent,
                   PRInt32 aIndexInParent,
                   nsIFrame* aParentFrame);

  virtual void InitializeWidget(nsIView *aView);

protected:
  virtual ~nsInputHiddenFrame();
};

nsInputHiddenFrame::nsInputHiddenFrame(nsIContent* aContent,
                                   PRInt32 aIndexInParent,
                                   nsIFrame* aParentFrame)
  : nsInputFrame(aContent, aIndexInParent, aParentFrame)
{
}

nsInputHiddenFrame::~nsInputHiddenFrame()
{
}


void 
nsInputHiddenFrame::InitializeWidget(nsIView *aView)
{
}

// nsInputHidden

nsInputHidden::nsInputHidden(nsIAtom* aTag, nsIFormManager* aManager)
  : nsInput(aTag, aManager) 
{
}

nsInputHidden::~nsInputHidden()
{
}

nsIFrame* 
nsInputHidden::CreateFrame(nsIPresContext* aPresContext,
                         PRInt32 aIndexInParent,
                         nsIFrame* aParentFrame)
{
  nsIFrame* rv = new nsInputHiddenFrame(this, aIndexInParent, aParentFrame);
  return rv;
}

void nsInputHidden::GetType(nsString& aResult) const
{
  aResult = "hidden";
}

void nsInputHidden::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  if (aAttribute == nsHTMLAtoms::value) {
    if (nsnull == mValue) {
      mValue = new nsString(aValue);
    } else {
      mValue->SetLength(0);
      mValue->Append(aValue);
    }
  }
  else if (aAttribute == nsHTMLAtoms::width) {
    nsHTMLValue value;
    ParseValue(aValue, 0, 1000, value);
    mWidth = value.GetIntValue();
  }
  else if (aAttribute == nsHTMLAtoms::height) {
    nsHTMLValue value;
    ParseValue(aValue, 0, 1000, value);
    mHeight = value.GetIntValue();
  }
  else {
    nsInput::SetAttribute(aAttribute, aValue);
  }
}

nsContentAttr nsInputHidden::GetAttribute(nsIAtom* aAttribute,
                                          nsHTMLValue& aResult) const
{
  nsContentAttr ca = eContentAttr_NotThere;
  aResult.Reset();
  if (aAttribute == nsHTMLAtoms::value) {
    if (nsnull != mValue) {
      aResult.Set(*mValue);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::width) {
    if (0 <= mWidth) {
      aResult.Set(mWidth, eHTMLUnit_Absolute);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::height) {
    if (0 <= mHeight) {
      aResult.Set(mHeight, eHTMLUnit_Absolute);
      ca = eContentAttr_HasValue;
    }
  }
  else {
    ca = nsInput::GetAttribute(aAttribute, aResult);
  }
  return ca;
}

NS_HTML nsresult
NS_NewHTMLInputHidden(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag, nsIFormManager* aManager)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsInputHidden(aTag, aManager);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

