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

#ifndef nsFileControlFrame_h___
#define nsFileControlFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsIFormControlFrame.h"
class nsButtonControlFrame;
class nsTextControlFrame;
class nsFormFrame;

class nsFileControlFrame : public nsHTMLContainerFrame,
                           public nsIFormControlFrame
{
public:
  nsFileControlFrame();

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

      // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Reflow(nsIPresContext&          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD GetFrameName(nsString& aResult) const;

  virtual PRInt32 GetMaxNumValues();

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  nsTextControlFrame* GetTextFrame() { return mTextFrame; }

  void SetTextFrame(nsTextControlFrame* aFrame) { mTextFrame = aFrame; }

  nsButtonControlFrame* GetBrowseFrame() { return mBrowseFrame; }
  void           SetBrowseFrame(nsButtonControlFrame* aFrame) { mBrowseFrame = aFrame; }
  NS_IMETHOD     GetName(nsString* aName);
  virtual void   SetFormFrame(nsFormFrame* aFormFrame) { mFormFrame = aFormFrame; }
  virtual PRBool IsSuccessful(nsIFormControlFrame* aSubmitter);
  virtual void   Reset();
  NS_IMETHOD     GetType(PRInt32* aType) const;
  void           SetFocus(PRBool aOn, PRBool aRepaint);

  NS_IMETHOD GetFont(nsIPresContext* aPresContext, 
                    nsFont&         aFont);

  NS_IMETHOD GetFormContent(nsIContent*& aContent) const;
  virtual nscoord GetVerticalInsidePadding(float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;


  // nsIFormMouseListener
  virtual void MouseClicked(nsIPresContext* aPresContext);

  //static PRInt32 gSpacing;

protected:
  nsIWidget* GetWindowTemp(nsIView *aView); // XXX temporary

  virtual PRIntn GetSkipSides() const;

  nsTextControlFrame*   mTextFrame;
  nsButtonControlFrame* mBrowseFrame; 
  nsFormFrame*          mFormFrame;

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
};

#endif


