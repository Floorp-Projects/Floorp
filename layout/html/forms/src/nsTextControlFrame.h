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

#ifndef nsTextControlFrame_h___
#define nsTextControlFrame_h___

#include "nsNativeFormControlFrame.h"
#include "nsIStatefulFrame.h"
#include "nsIPresState.h"

class nsIContent;
class nsIFrame;
class nsIPresContext;

class nsTextControlFrame : public nsNativeFormControlFrame,
                           public nsIStatefulFrame
{
/* ---------- methods implemented by base class ---------- */
public:
  nsTextControlFrame();
  virtual ~nsTextControlFrame();

  virtual const nsIID& GetCID();
  virtual const nsIID& GetIID();

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
#endif
  
  virtual nscoord GetVerticalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetHorizontalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                           float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext* aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;
  virtual PRInt32 GetMaxNumValues();

  NS_IMETHOD GetCursor(nsIPresContext* aPresContext, nsPoint& aPoint, PRInt32& aCursor);
  NS_IMETHOD SetSuggestedSize(nscoord aWidth, nscoord aHeight);

  //nsIStatefulFrame
  NS_IMETHOD GetStateType(nsIPresContext* aPresContext, nsIStatefulFrame::StateType* aStateType);
  NS_IMETHOD SaveState(nsIPresContext* aPresContext, nsIPresState** aState);
  NS_IMETHOD RestoreState(nsIPresContext* aPresContext, nsIPresState* aState);

protected:

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredLayoutSize);

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);

  PRInt32 GetDefaultColumnWidth() const { return (PRInt32)(20); } // this was DEFAULT_PIXEL_WIDTH

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }


/* ---------- abstract methods derived class must implement ---------- */
public:  
  // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAReadableString& aValue)=0;
  NS_IMETHOD GetProperty(nsIAtom* aName, nsAWritableString& aValue)=0; 

  virtual nsWidgetInitData* GetWidgetInitData(nsIPresContext* aPresContext)=0;

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint)=0;

  virtual void PostCreateWidget(nsIPresContext* aPresContext,
                                nscoord& aWidth,
                                nscoord& aHeight)=0;

  NS_IMETHOD GetText(nsString* aValue, PRBool aInitialValue)=0;

  virtual void EnterPressed(nsIPresContext* aPresContext)=0;

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames)=0;
  virtual void Reset(nsIPresContext* aPresContext)=0;

  NS_IMETHOD Paint(nsIPresContext* aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer)=0;

  virtual void PaintTextControlBackground(nsIPresContext* aPresContext,
                                          nsIRenderingContext& aRenderingContext,
                                          const nsRect& aDirtyRect,
                                          nsFramePaintLayer aWhichLayer)=0;

  virtual void PaintTextControl(nsIPresContext* aPresContext,
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
