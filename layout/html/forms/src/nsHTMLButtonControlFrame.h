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
#include "nsStyleConsts.h"
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
  ~nsHTMLButtonControlFrame();


  NS_IMETHOD  Destroy(nsIPresContext *aPresContext);

  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext, const nsPoint& aPoint, nsFramePaintLayer aWhichLayer, nsIFrame** aFrame);

  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD  Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        asPrevInFlow);

  NS_IMETHOD  GetAdditionalStyleContext(PRInt32 aIndex, 
                                        nsIStyleContext** aStyleContext) const;
  NS_IMETHOD  SetAdditionalStyleContext(PRInt32 aIndex, 
                                        nsIStyleContext* aStyleContext);
 
  NS_IMETHOD  AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);

#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("ButtonControl", aResult);
  }
#endif

  virtual nsresult RequiresWidget(PRBool &aRequiresWidget);


  virtual PRBool IsSuccessful(nsIFormControlFrame* aSubmitter);
  NS_IMETHOD GetType(PRInt32* aType) const;
  NS_IMETHOD GetName(nsString* aName);
  NS_IMETHOD GetValue(nsString* aName);
  virtual PRInt32 GetMaxNumValues();
  virtual PRBool  GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                 nsString* aValues, nsString* aNames);
  virtual void MouseClicked(nsIPresContext* aPresContext);
  virtual void Reset(nsIPresContext*) {};
  virtual void SetFormFrame(nsFormFrame* aFormFrame) { mFormFrame = aFormFrame; }

  void SetFocus(PRBool aOn, PRBool aRepaint);
  void ScrollIntoView(nsIPresContext* aPresContext);

  NS_IMETHOD GetFont(nsIPresContext* aPresContext, 
                     const nsFont*&  aFont);

  NS_IMETHOD GetFormContent(nsIContent*& aContent) const;
  virtual nscoord GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                           float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext* aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;

  void GetDefaultLabel(nsString& aLabel);

       // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAReadableString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsAWritableString& aValue); 
  NS_IMETHOD SetSuggestedSize(nscoord aWidth, nscoord aHeight);

protected:
  virtual PRBool IsReset(PRInt32 type);
  virtual PRBool IsSubmit(nsIPresContext* aPresContext, PRInt32 type);
  NS_IMETHOD AddComputedBorderPaddingToDesiredSize(nsHTMLReflowMetrics& aDesiredSize,
                                                   const nsHTMLReflowState& aSuggestedReflowState);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
  void GetTranslatedRect(nsIPresContext* aPresContext, nsRect& aRect);

  PRIntn GetSkipSides() const;
  PRBool mInline;
  nsFormFrame* mFormFrame;
  nsCursor mPreviousCursor;
  nsRect mTranslatedRect;
  PRBool mDidInit;
  nsButtonFrameRenderer mRenderer;

  //Resize Reflow OpitmizationSize;
  nsSize                mCacheSize;
  nsSize                mCachedMaxElementSize;
};

#endif




