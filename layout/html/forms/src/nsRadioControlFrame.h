/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsRadioControlFrame_h___
#define nsRadioControlFrame_h___

#include "nsIRadioControlFrame.h"
#include "nsNativeFormControlFrame.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIStatefulFrame.h"
#include "nsIPresState.h"
class nsIAtom;

// nsRadioControlFrame

class nsRadioControlFrame : public nsNativeFormControlFrame,
                            public nsIRadioControlFrame,
			    public nsIStatefulFrame
{
public:
    // nsFormControlFrame overrides
  nsresult RequiresWidget(PRBool &aHasWidget);

       // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAReadableString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsAWritableString& aValue); 


  virtual void PostCreateWidget(nsIPresContext* aPresContext,
                                nscoord& aWidth,
                                nscoord& aHeight);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
#endif

   //nsIRadioControlFrame methods
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD SetRadioButtonFaceStyleContext(nsIStyleContext *aRadioButtonFaceStyleContext);

  virtual PRBool GetChecked(PRBool aGetInitialValue);
  virtual void   SetChecked(nsIPresContext* aPresContext, PRBool aValue, PRBool aSetInitialValue);

  virtual PRInt32 GetMaxNumValues() { return 1; }
  
  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  virtual void MouseUp(nsIPresContext* aPresContext);
  virtual void Reset(nsIPresContext* aPresContext);
  virtual const nsIID& GetCID();

  //
  // XXX: The following paint methods are TEMPORARY. It is being used to get printing working
  // under windows. Later it may be used to GFX-render the controls to the display. 
  // Expect this code to repackaged and moved to a new location in the future.
  //

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  ///XXX: End o the temporary methods

  //nsIStatefulFrame
  NS_IMETHOD GetStateType(nsIPresContext* aPresContext, nsIStatefulFrame::StateType* aStateType);
  NS_IMETHOD SaveState(nsIPresContext* aPresContext, nsIPresState** aState);
  NS_IMETHOD RestoreState(nsIPresContext* aPresContext, nsIPresState* aState);

protected:

   // Utility methods for implementing SetProperty/GetProperty
  void SetRadioControlFrameState(nsIPresContext* aPresContext, const nsString& aValue);
  void GetRadioControlFrameState(nsString& aValue);             

protected:
	virtual PRBool	GetRadioState() = 0;
	virtual void 		SetRadioState(nsIPresContext* aPresContext, PRBool aValue) = 0;

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
};


#endif


