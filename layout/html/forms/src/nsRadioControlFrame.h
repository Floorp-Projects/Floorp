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

#ifndef nsRadioControlFrame_h___
#define nsRadioControlFrame_h___

#include "nsFormControlFrame.h"
#include "nsVoidArray.h"
#include "nsString.h"
class nsIAtom;

// nsRadioControlFrame

class nsRadioControlFrame : public nsFormControlFrame 
{
public:
  nsRadioControlFrame();
    // nsFormControlFrame overrides
  nsresult RequiresWidget(PRBool &aHasWidget);

       // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 


  virtual void PostCreateWidget(nsIPresContext* aPresContext,
                                nscoord& aWidth,
                                nscoord& aHeight);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint);

  NS_IMETHOD GetFrameName(nsString& aResult) const;

  virtual PRBool GetChecked(PRBool aGetInitialValue);
  virtual void   SetChecked(PRBool aValue, PRBool aSetInitialValue);

  virtual PRInt32 GetMaxNumValues() { return 1; }
  
  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  virtual void MouseClicked(nsIPresContext* aPresContext);
  virtual void Reset();
  virtual const nsIID& GetCID();

  virtual const nsIID& GetIID();

  //
  // XXX: The following paint methods are TEMPORARY. It is being used to get printing working
  // under windows. Later it may be used to GFX-render the controls to the display. 
  // Expect this code to repackaged and moved to a new location in the future.
  //

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  virtual void PaintRadioButton(nsIPresContext& aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect& aDirtyRect);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  ///XXX: End o the temporary methods

protected:

   // Utility methods for implementing SetProperty/GetProperty
  void SetRadioControlFrameState(const nsString& aValue);
  void GetRadioControlFrameState(nsString& aValue);             

  virtual nscoord GetRadioboxSize(float aPixToTwip) const;
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
    //GFX-rendered state variables
  PRBool mChecked;
};

// nsRadioControlGroup

class nsRadioControlGroup
{
public:
  nsRadioControlGroup(nsString& aName);
  virtual ~nsRadioControlGroup();

  PRBool               AddRadio(nsRadioControlFrame* aRadio);
  PRInt32              GetRadioCount() const;
  nsRadioControlFrame* GetRadioAt(PRInt32 aIndex) const;
  PRBool               RemoveRadio(nsRadioControlFrame* aRadio);

  nsRadioControlFrame* GetCheckedRadio();
  void                 SetCheckedRadio(nsRadioControlFrame* aRadio);
  void                 GetName(nsString& aNameResult) const;

protected:
  nsString             mName;
  nsVoidArray          mRadios;
  nsRadioControlFrame* mCheckedRadio;
};

#endif


