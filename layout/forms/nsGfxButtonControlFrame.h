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

#ifndef nsGfxButtonControlFrame_h___
#define nsGfxButtonControlFrame_h___

#include "nsFormControlFrame.h"
#include "nsHTMLButtonControlFrame.h"

// Class which implements the input[type=button, reset, submit] and
// browse button for input[type=file].
// The label for button is specified through generated content
// in the ua.css file.

class nsGfxButtonControlFrame : public nsHTMLButtonControlFrame {
public:
  nsGfxButtonControlFrame();

     //nsIFrame
  NS_IMETHOD Reflow(nsIPresContext&          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  virtual const nsIID& GetCID();
  virtual const nsIID& GetIID();

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

 
   // nsFormControlFrame
  NS_IMETHOD SetSuggestedSize(nscoord aWidth, nscoord aHeight);
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  PRBool IsSuccessful(nsIFormControlFrame* aSubmitter);
  virtual PRInt32 GetMaxNumValues();
  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);
  virtual void MouseClicked(nsIPresContext* aPresContext);

protected:
  NS_IMETHOD AddComputedBorderPaddingToDesiredSize(nsHTMLReflowMetrics& aDesiredSize,
                                                   const nsHTMLReflowState& aSuggestedReflowState);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  nscoord mSuggestedWidth;
  nscoord mSuggestedHeight;
};


#endif

