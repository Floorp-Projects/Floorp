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
#ifndef nsCheckboxControlFrame_h___
#define nsCheckboxControlFrame_h___

#include "nsFormControlFrame.h"
#include "nsIStatefulFrame.h"

class nsIPresState;

//
// nsCheckboxControlFrame
//
// This class implements the logic for both regular checkboxes _and_
// tri-state checkboxes (used in XUL). By default, this is a regular checkbox
// and is visibly no different from what is described by the HTML4 standard. When
// the attribute "moz-tristate" is set (value is unimportant), this transforms
// into a tri-state checkbox whose value is get/set via the attribute
// "moz-tristatevalue."
//
// Why not use "checked?" The HTML4 standard declares it to be a boolean (a fact
// enforced by our parser), so in order to not break anything, we use a different 
// attribute to store the value.
//
// The checkbox can switch between being regular and tri-state on-the-fly at any time
// without losing the value of the checkbox. However, if the checkbox is in the
// "mixed" state when it is transformed back into a normal checkbox, it will
// become checked since "mixed" doesn't exist on normal checkboxes.
//

class nsCheckboxControlFrame : public nsFormControlFrame,
                               public nsIStatefulFrame
{
public:

  nsCheckboxControlFrame ( ) ;
  
  virtual void PostCreateWidget(nsIPresContext* aPresContext,
                                nscoord& aWidth,
                                nscoord& aHeight);
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow) ;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("CheckboxControl", aResult);
  }
#endif

  virtual const nsIID& GetCID();
  virtual const nsIID& GetIID();

    // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAReadableString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsAWritableString& aValue); 
  virtual void Reset(nsIPresContext* aPresContext);
  virtual PRInt32 GetMaxNumValues();
  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);
  virtual void MouseUp(nsIPresContext* aPresContext);

   // nsFormControlFrame overrides
  nsresult RequiresWidget(PRBool &aHasWidget);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint) ;
  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

   // this should be protected, but VC6 is lame.
  enum CheckState { eOff, eOn, eMixed } ;

   // nsIStatefulFrame
  NS_IMETHOD GetStateType(nsIPresContext* aPresContext, nsIStatefulFrame::StateType* aStateType);
  NS_IMETHOD SaveState(nsIPresContext* aPresContext, nsIPresState** aState);
  NS_IMETHOD RestoreState(nsIPresContext* aPresContext, nsIPresState* aState);
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

protected:

    // native/gfx implementations need to implement needs.
  virtual CheckState GetCheckboxState() = 0;
  virtual void SetCheckboxState(nsIPresContext* aPresContext, CheckState aValue) = 0;

   // Utility methods for implementing SetProperty/GetProperty
  void SetCheckboxControlFrameState(nsIPresContext* aPresContext,
                                    const nsString& aValue);
  void GetCheckboxControlFrameState(nsString& aValue);  

    // utility routine for converting from DOM values to internal enum
  void CheckStateToString ( CheckState inState, nsString& outStateAsString ) ;
  CheckState StringToCheckState ( const nsString & aStateAsString ) ;

    // figure out if we're a tri-state checkbox.
  PRBool IsTristateCheckbox ( ) const { return mIsTristate; }

    // we just became a tri-state, or we just lost tri-state status. fix up
    // the attributes for the new mode.
  void SwitchModesWithEmergencyBrake ( nsIPresContext* aPresContext,
                                       PRBool inIsNowTristate ) ;
  
    // for tri-state checkbox. meaningless for normal HTML
  PRBool mIsTristate;
  
  static nsIAtom* GetTristateAtom() ;
  static nsIAtom* GetTristateValueAtom() ;

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
}; // class nsCheckboxControlFrame

#endif
