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

//
// nsSliderFrame
//

#ifndef nsSliderFrame_h__
#define nsSliderFrame_h__


#include "nsHTMLContainerFrame.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"
#include "nsIDOMMouseListener.h"
#include "nsIAnonymousContentCreator.h"
#include "nsITimerCallback.h"

class nsString;
class nsIScrollbarListener;
class nsISupportsArray;
class nsITimer;

#define INITAL_REPEAT_DELAY 500
#define REPEAT_DELAY        50

nsresult NS_NewSliderFrame(nsIFrame** aResult) ;


class nsSliderFrame : public nsHTMLContainerFrame, 
                             nsIDOMMouseListener, 
                             nsIAnonymousContentCreator,
                             nsITimerCallback
{
public:
  nsSliderFrame();
  virtual ~nsSliderFrame();

    // nsIFrame overrides
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("SliderFrame", aResult);
  }

  NS_IMETHOD Destroy(nsIPresContext& aPresContext);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);
 
    NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);

    virtual void CurrentPositionChanged(nsIPresContext* aPresContext);

     NS_IMETHOD  Init(nsIPresContext&  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        asPrevInFlow);

       NS_IMETHOD  CreateAnonymousContent(nsISupportsArray& aAnonymousItems);

   NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

     NS_IMETHOD  AppendFrames(nsIPresContext& aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  InsertFrames(nsIPresContext& aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  RemoveFrame(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);


   NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

    NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame);

    NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

 /**
  * Processes a mouse down event
  * @param aMouseEvent @see nsIDOMEvent.h 
  * @returns whether the event was consumed or ignored. @see nsresult
  */
  virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);

  /**
   * Processes a mouse up event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent);

  /**
   * Processes a mouse click event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   *
   */
  virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  /**
   * Processes a mouse click event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   *
   */
  virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  /**
   * Processes a mouse enter event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  /**
   * Processes a mouse leave event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr); 
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  virtual nsresult HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }

  static PRInt32 GetCurrentPosition(nsIContent* content);
  static PRInt32 GetMaxPosition(nsIContent* content);
  static PRInt32 GetIncrement(nsIContent* content);
  static PRInt32 GetPageIncrement(nsIContent* content);
  static PRInt32 GetIntegerAttribute(nsIContent* content, nsIAtom* atom, PRInt32 defaultValue);
  static PRInt32 IsHorizontal(nsIContent* content);

  void SetScrollbarListener(nsIScrollbarListener* aListener);


  NS_IMETHOD HandlePress(nsIPresContext& aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus&  aEventStatus);

  NS_IMETHOD HandleMultiplePress(nsIPresContext& aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus&  aEventStatus)  { return NS_OK; }

  NS_IMETHOD HandleDrag(nsIPresContext& aPresContext,
                        nsGUIEvent *    aEvent,
                        nsEventStatus&  aEventStatus)  { return NS_OK; }

  NS_IMETHOD HandleRelease(nsIPresContext& aPresContext,
                           nsGUIEvent *    aEvent,
                           nsEventStatus&  aEventStatus);

  virtual void Notify(nsITimer *timer);

protected:

  virtual void ReflowThumb(nsIPresContext&   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     nsIFrame* thumbFrame,
                     nsSize available,
                     nsSize computed);

  virtual PRIntn GetSkipSides() const { return 0; }

 
private:

  nsIContent* GetScrollBar();
  void PageUpDown(nsIFrame* aThumbFrame, nscoord change);
  void SetCurrentPosition(nsIContent* scrollbar, nsIFrame* aThumbFrame, nscoord pos);
  NS_IMETHOD DragThumb(PRBool aGrabMouseEvents);
  void AddListener();
  void RemoveListener();
  PRBool isDraggingThumb();

  float mRatio;

  nscoord mDragStartPx;
  nscoord mThumbStart;

  PRInt32 mCurPos;

  nsIScrollbarListener* mScrollbarListener;

  static nscoord gChange;
}; // class nsSliderFrame

#endif
