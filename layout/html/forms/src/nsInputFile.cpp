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

#include "nsInputFile.h"
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

class nsInputFileFrame : public nsInputFrame {
public:
  nsInputFileFrame(nsIContent* aContent,
                   PRInt32 aIndexInParent,
                   nsIFrame* aParentFrame);

  virtual void InitializeWidget(nsIView *aView);

protected:
  virtual ~nsInputFileFrame();
};

nsInputFileFrame::nsInputFileFrame(nsIContent* aContent,
                                   PRInt32 aIndexInParent,
                                   nsIFrame* aParentFrame)
  : nsInputFrame(aContent, aIndexInParent, aParentFrame)
{
}

nsInputFileFrame::~nsInputFileFrame()
{
}


void 
nsInputFileFrame::InitializeWidget(nsIView *aView)
{
}

//----------------------------------------------------------------------
// nsInputFile

nsInputFile::nsInputFile(nsIAtom* aTag, nsIFormManager* aManager)
  : nsInput(aTag, aManager) 
{
}

nsInputFile::~nsInputFile()
{
  if (nsnull != mValue) {
    delete mValue;
  }
}

nsIFrame* 
nsInputFile::CreateFrame(nsIPresContext* aPresContext,
                         PRInt32 aIndexInParent,
                         nsIFrame* aParentFrame)
{
  nsIFrame* rv = new nsInputFileFrame(this, aIndexInParent, aParentFrame);
  return rv;
}

void nsInputFile::GetType(nsString& aResult) const
{
  aResult = "file";
}

void nsInputFile::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  if (aAttribute == nsHTMLAtoms::size) {
    nsHTMLValue value;
    ParseValue(aValue, 1, value);
    mSize = value.GetIntValue();
  }
  else if (aAttribute == nsHTMLAtoms::maxlength) {
    nsHTMLValue value;
    ParseValue(aValue, 1, value);/* XXX nav doesn't clamp; what does it do with illegal values? */
    mMaxLength = value.GetIntValue();
  }
  else if (aAttribute == nsHTMLAtoms::value) {
    if (nsnull == mValue) {
      mValue = new nsString(aValue);
    } else {
      *mValue = aValue;
    }
  }
  else {
    nsInput::SetAttribute(aAttribute, aValue);
  }
}

nsContentAttr nsInputFile::GetAttribute(nsIAtom* aAttribute,
                                        nsHTMLValue& aResult) const
{
  nsContentAttr ca = eContentAttr_NotThere;
  aResult.Reset();
  if (aAttribute == nsHTMLAtoms::size) {
    if (0 < mSize) {
      aResult.Set(mSize, eHTMLUnit_Absolute);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::maxlength) {
    if (0 < mMaxLength) {
      aResult.Set(mMaxLength, eHTMLUnit_Absolute);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::value) {
    if (nsnull != mValue) {
      aResult.Set(*mValue);
      ca = eContentAttr_HasValue;
    }
  }
  else {
    ca = nsInput::GetAttribute(aAttribute, aResult);
  }
  return ca;
}

NS_HTML nsresult
NS_NewHTMLInputFile(nsIHTMLContent** aInstancePtrResult,
                    nsIAtom* aTag, nsIFormManager* aManager)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsInputFile(aTag, aManager);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

