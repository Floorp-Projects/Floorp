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

#ifndef nsFileControlFrame_h___
#define nsFileControlFrame_h___

#include "nsAreaFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIDOMMouseListener.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIStatefulFrame.h"
#include "nsCOMPtr.h"
#include "nsIHTMLContent.h"

#include "nsGfxTextControlFrame2.h"
#include "nsFormControlHelper.h"
typedef   nsGfxTextControlFrame2 nsNewFrame;

class nsIPresState;
class nsFormFrame;
class nsISupportsArray;

class nsFileControlFrame : public nsAreaFrame,
                           public nsIFormControlFrame,
                           public nsIDOMMouseListener,
                           public nsIAnonymousContentCreator,
                           public nsIStatefulFrame

{
public:
  nsFileControlFrame();
  virtual ~nsFileControlFrame();

  // XXX Hack so we can squirrel away the pres context pointer
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow) {
    mPresContext = aPresContext;
    return nsAreaFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  }

  virtual void MouseClicked(nsIPresContext* aPresContext) {}
  virtual void SetFormFrame(nsFormFrame* aFormFrame) { mFormFrame = aFormFrame; }
  virtual nsFormFrame* GetFromFrame() { return mFormFrame; }

  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

      // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAReadableString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsAWritableString& aValue); 
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Reflow(nsIPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
#endif
  NS_IMETHOD SetSuggestedSize(nscoord aWidth, nscoord aHeight) { return NS_OK; };
  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext, const nsPoint& aPoint, nsFramePaintLayer aWhichLayer, nsIFrame** aFrame);
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType, 
                              PRInt32         aHint);

  virtual PRInt32 GetMaxNumValues();

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  NS_IMETHOD     GetName(nsString* aName);
  virtual PRBool IsSuccessful(nsIFormControlFrame* aSubmitter);
  virtual void   Reset(nsIPresContext* aPresContext);
  NS_IMETHOD     GetType(PRInt32* aType) const;
  void           SetFocus(PRBool aOn, PRBool aRepaint);
  void           ScrollIntoView(nsIPresContext* aPresContext);

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

  virtual nsresult RequiresWidget(PRBool &aRequiresWidget);


  // from nsIAnonymousContentCreator
  NS_IMETHOD CreateAnonymousContent(nsIPresContext* aPresContext,
                                    nsISupportsArray& aChildList);
  NS_IMETHOD CreateFrameFor(nsIPresContext*   aPresContext,
                            nsIContent *      aContent,
                            nsIFrame**        aFrame) { if (aFrame) *aFrame = nsnull; return NS_ERROR_FAILURE; }


  // mouse events when out browse button is pressed
  /**
  * Processes a mouse down event
  * @param aMouseEvent @see nsIDOMEvent.h 
  * @returns whether the event was consumed or ignored. @see nsresult
  */
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  /**
   * Processes a mouse up event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  /**
   * Processes a mouse click event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   *
   */
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent); // we only care when the button is clicked

  /**
   * Processes a mouse click event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   *
   */
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  /**
   * Processes a mouse enter event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  /**
   * Processes a mouse leave event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }

  //nsIStatefulFrame
  NS_IMETHOD SaveState(nsIPresContext* aPresContext, nsIPresState** aState);
  NS_IMETHOD RestoreState(nsIPresContext* aPresContext, nsIPresState* aState);

protected:
  virtual PRIntn GetSkipSides() const;

  nsNewFrame* mTextFrame;
  nsFormFrame*        mFormFrame;
  nsIHTMLContent*     mTextContent;
  nsCOMPtr<nsIHTMLContent> mBrowse;
  nsString*           mCachedState;
  // XXX Hack: pres context needed by function MouseClick()
  nsIPresContext*     mPresContext;  // weak reference

private:
  nsNewFrame* GetTextControlFrame(nsIPresContext* aPresContext,
                                          nsIFrame* aStart);

  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
};

#endif


