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
// selection. Clicking a third time would get back into the mixed state.
//
// Note that the user can only get into the mixed state when the control
// has been in that state at some previous time during its lifetime. That
// means that it must be explicitly set to "mixed" at some point in order
// for the user to get there by clicking. This is done by setting the "value"
// attribute to "2". If this is not done, this checkbox behaves just like
// the normal checkbox.
//
// The only DOM APIs that this checkbox supports are the generic XML DOM APIs.
// This is mainly a result of the fact that our content node is a XUL content
// node, and we (read: hyatt) would have to go off and implement these
// extra HTMLInputElement APIs to match the API set of the normal checkbox.
// We're not going to do that, so you're just going to have to live with
// getting and setting the "value" attribute ;)
//

#ifndef nsTriStateCheckboxFrame_h__
#define nsTriStateCheckboxFrame_h__


#include "nsLeafFrame.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"

class nsIPresContext;
class nsString;
class nsIContent;


nsresult NS_NewTriStateCheckboxFrame(nsIFrame*& aResult) ;


class nsTriStateCheckboxFrame : public nsLeafFrame
{
public:
  nsTriStateCheckboxFrame();

    // nsIFrame overrides
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("TriStateCheckboxFrame", aResult);
  }
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent*     aChild,
                               nsIAtom*        aAttribute,
                               PRInt32         aHint) ;
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);
  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);
  
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
  virtual void PaintMixedMark(nsIRenderingContext& aRenderingContext,
                              float aPixelsToTwips, PRUint32 aWidth, PRUint32 aHeight) ;

  void DisplayDepressed ( ) ;
  void DisplayNormal ( ) ;

    // utility routine for converting from DOM values to internal enum
  void CheckStateToString ( CheckState inState, nsString& outStateAsString ) ;
  CheckState StringToCheckState ( const nsString & aStateAsString ) ;

  PRBool mMouseDownOnCheckbox;       // for tracking clicks
  PRBool mHasOnceBeenInMixedState;   // since we only want to show the 
  
    // atom for the "depress" attribute. We will have a CSS rule that
    // when this is set, draws the button depressed.
  static void GetDepressAtom(nsCOMPtr<nsIAtom>* outAtom) ;

}; // class nsTriStateCheckboxFrame

#endif
