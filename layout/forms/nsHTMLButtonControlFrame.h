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

#ifndef nsHTMLButtonControlFrame_h___
#define nsHTMLButtonControlFrame_h___

#include "nsCOMPtr.h"
#include "nsHTMLContainerFrame.h"
#include "nsFormControlHelper.h"
#include "nsIFormControlFrame.h"
#include "nsHTMLParts.h"
#include "nsIFormControl.h"
#include "nsFormFrame.h"

#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"
#include "nsISupports.h"
#include "nsHTMLAtoms.h"
#include "nsIImage.h"
#include "nsStyleUtil.h"
#include "nsDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsViewsCID.h"
#include "nsColor.h"
#include "nsIDocument.h"
#include "nsButtonFrameRenderer.h"

class nsHTMLButtonControlFrame : public nsHTMLContainerFrame,
                                 public nsIFormControlFrame 
{
public:
  nsHTMLButtonControlFrame();



  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame);

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD  Init(nsIPresContext&  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        asPrevInFlow);

  NS_IMETHOD  ReResolveStyleContext ( nsIPresContext* aPresContext, 
                                      nsIStyleContext* aParentContext,
                                      PRInt32 aParentChange,
                                      nsStyleChangeList* aChangeList,
                                      PRInt32* aLocalChange) ;

 

  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("ButtonControl", aResult);
  }

  virtual nsresult RequiresWidget(PRBool &aRequiresWidget);


  virtual PRBool IsSuccessful(nsIFormControlFrame* aSubmitter);
  NS_IMETHOD GetType(PRInt32* aType) const;
  NS_IMETHOD GetName(nsString* aName);
  NS_IMETHOD GetValue(nsString* aName);
  virtual PRInt32 GetMaxNumValues();
  virtual PRBool  GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                 nsString* aValues, nsString* aNames);
  virtual void MouseClicked(nsIPresContext* aPresContext);
  virtual void Reset() {};
  virtual void SetFormFrame(nsFormFrame* aFormFrame) { mFormFrame = aFormFrame; }

  void SetFocus(PRBool aOn, PRBool aRepaint);

  NS_IMETHOD GetFont(nsIPresContext* aPresContext, 
                    nsFont&         aFont);

  NS_IMETHOD GetFormContent(nsIContent*& aContent) const;
  virtual nscoord GetVerticalInsidePadding(float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;

  void GetDefaultLabel(nsString& aLabel);

       // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 

protected:
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
  void GetTranslatedRect(nsRect& aRect);

  PRIntn GetSkipSides() const;
  PRBool mInline;
  nsFormFrame* mFormFrame;
  nsCursor mPreviousCursor;
  nsRect mTranslatedRect;
  PRBool mDidInit;
  nsButtonFrameRenderer mRenderer;
};

#endif




