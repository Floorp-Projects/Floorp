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

#ifndef nsNativeTextControlFrame_h___
#define nsNativeTextControlFrame_h___

#include "nsTextControlFrame.h"
class nsIContent;
class nsIFrame;
class nsIPresContext;

class nsNativeTextControlFrame : public nsTextControlFrame
{
private:
  typedef nsTextControlFrame Inherited;

public:
       // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 

  virtual nsWidgetInitData* GetWidgetInitData(nsIPresContext& aPresContext);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint);

  virtual void PostCreateWidget(nsIPresContext* aPresContext,
                                nscoord& aWidth,
                                nscoord& aHeight);

  NS_IMETHOD GetText(nsString* aValue, PRBool aInitialValue);

  virtual void EnterPressed(nsIPresContext& aPresContext) ;

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);
  virtual void Reset();

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);
 
  virtual void PaintTextControlBackground(nsIPresContext& aPresContext,
                                          nsIRenderingContext& aRenderingContext,
                                          const nsRect& aDirtyRect,
                                          nsFramePaintLayer aWhichLayer);
 
  virtual void PaintTextControl(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect, nsString& aText,
                                nsIStyleContext* aStyleContext,
                                nsRect& aRect);

  // Utility methods to get and set current widget state
  void GetTextControlFrameState(nsString& aValue);
  void SetTextControlFrameState(const nsString& aValue); 
  
  virtual nsresult RequiresWidget(PRBool &aRequiresWidget);

};

#endif
