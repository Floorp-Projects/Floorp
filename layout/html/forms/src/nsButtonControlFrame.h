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

#ifndef nsButtonControlFrame_h___
#define nsButtonControlFrame_h___

#include "nsNativeFormControlFrame.h"
#include "nsButtonFrameRenderer.h"

class nsButtonControlFrame : public nsNativeFormControlFrame {
private:
	typedef nsNativeFormControlFrame Inherited;

public:

   // nsFormControlFrame overrides
  nsresult RequiresWidget(PRBool &aHasWidget);

   // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 

  NS_IMETHOD GetFrameName(nsString& aResult) const;

  virtual void MouseClicked(nsIPresContext* aPresContext);

  virtual const nsIID& GetCID();

  virtual const nsIID& GetIID();

  virtual nscoord GetVerticalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetHorizontalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetVerticalInsidePadding(float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;
  //nsFileControlFrame* GetFileControlFrame() { return mFileControlFrame; }
  void GetDefaultLabel(nsString& aLabel);

  PRBool IsSuccessful(nsIFormControlFrame* aSubmitter);
  virtual PRInt32 GetMaxNumValues();

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  // Sets listener for button click
  void SetMouseListener(nsIFormControlFrame* aListener) { mMouseListener = aListener; }

protected:
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);


  
  virtual void Redraw();
  virtual void SetFocus(PRBool aOn, PRBool aRepaint);


  nsIFormControlFrame* mMouseListener; // for browse buttons only
};


#endif

