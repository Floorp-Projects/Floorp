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

#include "nsInputRadio.h"
#include "nsInputFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsIRadioButton.h"
#include "nsWidgetsCID.h"
#include "nsSize.h"

class nsInputRadioFrame : public nsInputFrame {
public:
  nsInputRadioFrame(nsIContent* aContent,
                   PRInt32 aIndexInParent,
                   nsIFrame* aParentFrame);

  virtual void PreInitializeWidget(nsIPresContext* aPresContext, 
	                               nsSize& aBounds);

  virtual void InitializeWidget(nsIView *aView);

  virtual const nsIID GetCID();

  virtual const nsIID GetIID();

protected:
  virtual ~nsInputRadioFrame();
};

nsInputRadioFrame::nsInputRadioFrame(nsIContent* aContent,
                                   PRInt32 aIndexInParent,
                                   nsIFrame* aParentFrame)
  : nsInputFrame(aContent, aIndexInParent, aParentFrame)
{
}

nsInputRadioFrame::~nsInputRadioFrame()
{
}


const nsIID
nsInputRadioFrame::GetIID()
{
  static NS_DEFINE_IID(kRadioIID, NS_IRADIOBUTTON_IID);
  return kRadioIID;
}
  
const nsIID
nsInputRadioFrame::GetCID()
{
  static NS_DEFINE_IID(kRadioCID, NS_RADIOBUTTON_CID);
  return kRadioCID;
}

void 
nsInputRadioFrame::PreInitializeWidget(nsIPresContext* aPresContext, 
								       nsSize& aBounds)
{
  float p2t = aPresContext->GetPixelsToTwips();
  aBounds.width  = (int)(12 * p2t);
  aBounds.height = (int)(12 * p2t);
}

void 
nsInputRadioFrame::InitializeWidget(nsIView *aView)
{
}

// nsInputRadio

nsInputRadio::nsInputRadio(nsIAtom* aTag, nsIFormManager* aManager)
  : nsInputCheckbox(aTag, aManager) 
{
}

nsInputRadio::~nsInputRadio()
{
}

nsIFrame* 
nsInputRadio::CreateFrame(nsIPresContext* aPresContext,
                         PRInt32 aIndexInParent,
                         nsIFrame* aParentFrame)
{
  nsIFrame* rv = new nsInputRadioFrame(this, aIndexInParent, aParentFrame);
  return rv;
}

void nsInputRadio::GetType(nsString& aResult) const
{
  aResult = "radio";
}

NS_HTML nsresult
NS_NewHTMLInputRadio(nsIHTMLContent** aInstancePtrResult,
                     nsIAtom* aTag, nsIFormManager* aManager)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsInputRadio(aTag, aManager);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

