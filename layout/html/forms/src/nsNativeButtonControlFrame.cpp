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

#include "nsNativeButtonControlFrame.h"
#include "nsHTMLAtoms.h"
#include "nsFormFrame.h"
#include "nsIFormControl.h"
#include "nsIContent.h"
#include "nsIButton.h"

static NS_DEFINE_IID(kIButtonIID,      NS_IBUTTON_IID);

nsresult
NS_NewNativeButtonControlFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsNativeButtonControlFrame* it = new nsNativeButtonControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}


NS_IMETHODIMP
nsNativeButtonControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                       nsIContent*     aChild,
                                       nsIAtom*        aAttribute,
                                       PRInt32         aHint)
{
  nsresult result = NS_OK;
  if (mWidget) {
    if (nsHTMLAtoms::value == aAttribute) {
      nsIButton* button = nsnull;
      result = mWidget->QueryInterface(kIButtonIID, (void**)&button);
      if ((NS_SUCCEEDED(result)) && (nsnull != button)) {
        nsString value;
        /*XXXnsresult result = */GetValue(&value);
        button->SetLabel(value);
        NS_RELEASE(button);
        if (aHint != NS_STYLE_HINT_REFLOW) 
          nsFormFrame::StyleChangeReflow(aPresContext, this);
      }
    } else if (nsHTMLAtoms::size == aAttribute &&
               aHint != NS_STYLE_HINT_REFLOW) {
      nsFormFrame::StyleChangeReflow(aPresContext, this);
    }
    // Allow the base class to handle common attributes supported
    // by all form elements... 
    else {
      result = Inherited::AttributeChanged(aPresContext, aChild, aAttribute, aHint);
    }
  }
  return result;
}


void 
nsNativeButtonControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
 	nsIButton* button = nsnull;
  if (mWidget && (NS_OK == mWidget->QueryInterface(kIButtonIID,(void**)&button))) {
    nsFont font(aPresContext->GetDefaultFixedFontDeprecated()); 
    GetFont(aPresContext, font);
    mWidget->SetFont(font);
    SetColors(*aPresContext);

    nsAutoString value;
    nsresult result = GetValue(&value);

    if (NS_CONTENT_ATTR_HAS_VALUE != result) {  
      GetDefaultLabel(value);
    }
    button->SetLabel(value);
    NS_RELEASE(button);

    mWidget->Enable(!nsFormFrame::GetDisabled(this));
  }
}

