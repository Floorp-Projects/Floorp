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

#ifndef nsTextControlFrame_h___
#define nsTextControlFrame_h___

#include "nsNativeFormControlFrame.h"
class nsIContent;
class nsIFrame;
class nsIPresContext;

class nsTextControlFrame : public nsNativeFormControlFrame
{
/* ---------- methods implemented by base class ---------- */
public:
  nsTextControlFrame();
  virtual ~nsTextControlFrame();

  virtual const nsIID& GetCID();
  virtual const nsIID& GetIID();

  NS_IMETHOD GetFrameName(nsString& aResult) const;
  
  virtual nscoord GetVerticalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetHorizontalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetVerticalInsidePadding(float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;
  virtual PRInt32 GetMaxNumValues();

  NS_IMETHOD GetCursor(nsIPresContext& aPresContext, nsPoint& aPoint, PRInt32& aCursor);
  NS_IMETHOD SetSuggestedSize(nscoord aWidth, nscoord aHeight);

protected:

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);



/* ---------- abstract methods derived class must implement ---------- */
public:  
  // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue)=0;
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue)=0; 

  virtual nsWidgetInitData* GetWidgetInitData(nsIPresContext& aPresContext)=0;

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint)=0;

  virtual void PostCreateWidget(nsIPresContext* aPresContext,
                                nscoord& aWidth,
                                nscoord& aHeight)=0;

  NS_IMETHOD GetText(nsString* aValue, PRBool aInitialValue)=0;

  virtual void EnterPressed(nsIPresContext& aPresContext)=0;

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames)=0;
  virtual void Reset()=0;

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer)=0;

  virtual void PaintTextControlBackground(nsIPresContext& aPresContext,
                                          nsIRenderingContext& aRenderingContext,
                                          const nsRect& aDirtyRect,
                                          nsFramePaintLayer aWhichLayer)=0;

  virtual void PaintTextControl(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect, nsString& aText,
                                nsIStyleContext* aStyleContext,
                                nsRect& aRect)=0;

  // Utility methods to get and set current widget state
  virtual void GetTextControlFrameState(nsString& aValue)=0;
  virtual void SetTextControlFrameState(const nsString& aValue)=0; 
  
  virtual nsresult RequiresWidget(PRBool &aRequiresWidget)=0;

};

#endif
