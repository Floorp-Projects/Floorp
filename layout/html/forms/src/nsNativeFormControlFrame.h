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

#ifndef nsNativeFormControlFrame_h___
#define nsNativeFormControlFrame_h___

#include "nsFormControlFrame.h"


/** 
  * nsFormControlFrame is the base class for frames of form controls. It
  * provides a uniform way of creating widgets, resizing, and painting.
  * @see nsLeafFrame and its base classes for more info
  */
class nsNativeFormControlFrame : public nsFormControlFrame
{
public:
  nsNativeFormControlFrame();


  /**
    * Respond to the request to resize and/or reflow
    * @see nsIFrame::Reflow
    */
  NS_IMETHOD Reflow(nsIPresContext&      aCX,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&      aStatus);


  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint);
  /** 
    * Respond to a gui event
    * @see nsIFrame::HandleEvent
    */
  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  /**
    * Get the widget associated with this frame
    * @param aView the view associated with the frame. It is a convience parm.
    * @param aWidget the address of address of where the widget will be placed.
    * This method doses an AddRef on the widget.
    */
  nsresult GetWidget(nsIView* aView, nsIWidget** aWidget);
  nsresult GetWidget(nsIWidget** aWidget);

	PRBool	 HasNativeWidget() 		{ return (nsnull != mWidget);}



  virtual void SetFocus(PRBool aOn = PR_TRUE, PRBool aRepaint = PR_FALSE);

  void SetColors(nsIPresContext& aPresContext);

protected:

  virtual ~nsNativeFormControlFrame();

  nsMouseState mLastMouseState;
  nsIWidget*   mWidget;

};

#endif

