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

//
// nsTriStateCheckboxFrame
//
// A sibling of a checkbox, but with an extra state, mixed, that represents
// something in between on and off. 
//
// An example will explain it best. Say you
// select some text, part of which is bold, and go to a dialog that lets you
// customize the styles of that text. The checkboxes for each style should
// represent the current state of the text, but what value should the "Bold"
// checkbox have? All the text isn't bold, so it shouldn't be on, but some
// if it is, so you want to indicate that to the user. Hence, a 3rd state.
//
// Clicking the control when it is mixed would uncheck the control, as if
// it is totally off. In the above example, the entire selection would be
// unbolded. Clicking it again would check the control and bold the entire
// selection. Note that there is no way to get back to the mixed state. This
// is by design. It just doesn't make any sense. What that action really means
// would be satisfied by "Cancel" or "Undo" which is beyond the scope of
// the control.
//

#ifndef nsTriStateCheckboxFrame_h__
#define nsTriStateCheckboxFrame_h__


#include "nsFormControlFrame.h"
#include "nsLeafFrame.h"
#include "prtypes.h"

class nsIPresContext;
class nsString;
class nsIContent;
class nsIAtom;


nsresult NS_NewTriStateCheckboxFrame(nsIFrame*& aResult) ;


class nsTriStateCheckboxFrame : public nsLeafFrame
{
public:
  nsTriStateCheckboxFrame();

  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("TriStateCheckboxFrame", aResult);
  }


  virtual PRInt32 GetMaxNumValues();

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  //End of GFX-rendering methods
  
protected:

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize) ;

  enum CheckState { eOff, eOn, eMixed } ;

  CheckState GetCurrentCheckState() ;
  void SetCurrentCheckState(CheckState aState) ;
  
  virtual void MouseClicked(const nsIPresContext & aPresContext);
 
  virtual void PaintCheckBox(nsIPresContext& aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect& aDirtyRect,
                             nsFramePaintLayer aWhichLayer);

    // utility routine for converting from DOM values to internal enum
  void CheckStateToString ( CheckState inState, nsString& outStateAsString ) ;
  CheckState StringToCheckState ( const nsString & aStateAsString ) ;

    //GFX-rendered state variables
  PRBool mMouseDownOnCheckbox;

}; // class nsTriStateCheckboxFrame

#endif
