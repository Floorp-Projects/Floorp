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

#ifndef nsNativeTextControlFrame_h___
#define nsNativeTextControlFrame_h___

#include "nsTextControlFrame.h"
class nsIContent;
class nsIFrame;
class nsIPresContext;

class nsNativeTextControlFrame : public nsTextControlFrame
{
public:
  nsNativeTextControlFrame();
  virtual ~nsNativeTextControlFrame();
  
  // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAReadableString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsAWritableString& aValue); 

  virtual nsWidgetInitData* GetWidgetInitData(nsIPresContext* aPresContext);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint);

  virtual void PostCreateWidget(nsIPresContext* aPresContext,
                                nscoord& aWidth,
                                nscoord& aHeight);

  NS_IMETHOD GetText(nsString* aValue, PRBool aInitialValue);

  virtual void EnterPressed(nsIPresContext* aPresContext) ;

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);
  virtual void Reset(nsIPresContext* aPresContext);

  NS_IMETHOD Paint(nsIPresContext* aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);
 
  virtual void PaintTextControlBackground(nsIPresContext* aPresContext,
                                          nsIRenderingContext& aRenderingContext,
                                          const nsRect& aDirtyRect,
                                          nsFramePaintLayer aWhichLayer);
 
  virtual void PaintTextControl(nsIPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect, nsString& aText,
                                nsIStyleContext* aStyleContext,
                                nsRect& aRect);

  // Utility methods to get and set current widget state
  void GetTextControlFrameState(nsString& aValue);
  void SetTextControlFrameState(const nsString& aValue); 
  
  virtual nsresult RequiresWidget(PRBool &aRequiresWidget);

protected:
  nsString* mCachedState;
};

#endif
