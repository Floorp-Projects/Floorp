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
#ifndef nsCheckboxControlFrame_h___
#define nsCheckboxControlFrame_h___

#include "nsNativeFormControlFrame.h"


class nsCheckboxControlFrame : public nsNativeFormControlFrame {
private:
	typedef nsNativeFormControlFrame Inherited;

public:
  virtual void PostCreateWidget(nsIPresContext* aPresContext,
                                nscoord& aWidth,
                                nscoord& aHeight);

  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("CheckboxControl", aResult);
  }

  virtual const nsIID& GetCID();

  virtual const nsIID& GetIID();

  virtual void MouseClicked(nsIPresContext* aPresContext);

  virtual PRInt32 GetMaxNumValues();

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  virtual void Reset();

    // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 

   // Utility methods for implementing SetProperty/GetProperty
  void SetCheckboxControlFrameState(const nsString& aValue);
  void GetCheckboxControlFrameState(nsString& aValue);  

   // nsFormControlFrame overrides
  nsresult RequiresWidget(PRBool &aHasWidget);

  //
  // Methods used to GFX-render the checkbox
  // 

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  //End of GFX-rendering methods
  
protected:
	virtual PRBool	GetCheckboxState() = 0;
	virtual void 		SetCheckboxState(PRBool aValue) = 0;
};

#endif
