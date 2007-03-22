/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsFormControlFrame_h___
#define nsFormControlFrame_h___

#include "nsIFormControlFrame.h"
#include "nsLeafFrame.h"

/** 
  * nsFormControlFrame is the base class for frames of form controls. It
  * provides a uniform way of creating widgets, resizing, and painting.
  * @see nsLeafFrame and its base classes for more info
  */
class nsFormControlFrame : public nsLeafFrame,
                           public nsIFormControlFrame
{

public:
  /**
    * Main constructor
    * @param aContent the content representing this frame
    * @param aParentFrame the parent frame
    */
  nsFormControlFrame(nsStyleContext*);

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsLeafFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  /** 
    * Respond to a gui event
    * @see nsIFrame::HandleEvent
    */
  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  /**
    * Respond to the request to resize and/or reflow
    * @see nsIFrame::Reflow
    */
  NS_IMETHOD Reflow(nsPresContext*      aCX,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  virtual void Destroy();

  // new behavior

  virtual void SetFocus(PRBool aOn = PR_TRUE, PRBool aRepaint = PR_FALSE);

  // nsIFormControlFrame
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue);

  virtual nsresult GetFormProperty(nsIAtom* aName, nsAString& aValue) const; 
  
  // AccessKey Helper function
  static nsresult RegUnRegAccessKey(nsIFrame * aFrame, PRBool aDoReg);

  /**
   * Helper routine to that returns the height of the screen
   *
   */
  static nsresult GetScreenHeight(nsPresContext* aPresContext, nscoord& aHeight);

protected:

  virtual ~nsFormControlFrame();

  virtual nscoord GetIntrinsicWidth();
  virtual nscoord GetIntrinsicHeight();

//
//-------------------------------------------------------------------------------------
//  Utility methods for managing checkboxes and radiobuttons
//-------------------------------------------------------------------------------------
//   
   /**
    * Get the state of the checked attribute.
    * @param aState set to PR_TRUE if the checked attribute is set,
    * PR_FALSE if the checked attribute has been removed
    */

  void GetCurrentCheckState(PRBool* aState);

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

};

#endif

