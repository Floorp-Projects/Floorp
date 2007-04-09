/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsSliderFrame_h__
#define nsSliderFrame_h__


#include "nsBoxFrame.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "nsIDOMMouseListener.h"

class nsString;
class nsIScrollbarListener;
class nsISupportsArray;
class nsITimer;
class nsSliderFrame;

nsIFrame* NS_NewSliderFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

class nsSliderMediator : public nsIDOMMouseListener, 
                         public nsITimerCallback
{
public:

  NS_DECL_ISUPPORTS

  nsSliderFrame* mSlider;

  nsSliderMediator(nsSliderFrame* aSlider) {  mSlider = aSlider; }
  virtual ~nsSliderMediator() {}

  virtual void SetSlider(nsSliderFrame* aSlider) { mSlider = aSlider; }

 /**
  * Processes a mouse down event
  * @param aMouseEvent @see nsIDOMEvent.h 
  * @returns whether the event was consumed or ignored. @see nsresult
  */
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);

  /**
   * Processes a mouse up event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);

  /**
   * Processes a mouse click event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   *
   */
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent) { return NS_OK; }

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

  NS_DECL_NSITIMERCALLBACK


}; // class nsSliderFrame

class nsSliderFrame : public nsBoxFrame
{
public:
  friend class nsSliderMediator;

  nsSliderFrame(nsIPresShell* aShell, nsStyleContext* aContext);
  virtual ~nsSliderFrame();

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
    return MakeFrameName(NS_LITERAL_STRING("SliderFrame"), aResult);
  }
#endif

  // nsIBox
  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);

  // nsIFrame overrides
  NS_IMETHOD  AppendFrames(nsIAtom*        aListName,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  InsertFrames(nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  RemoveFrame(nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);

  virtual void Destroy();

  NS_IMETHOD BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);
 
  NS_IMETHOD AttributeChanged(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType);

  virtual nsresult CurrentPositionChanged(nsPresContext* aPresContext);

  NS_IMETHOD  Init(nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIFrame*        asPrevInFlow);


  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD SetInitialChildList(nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);

  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }

  static PRInt32 GetCurrentPosition(nsIContent* content);
  static PRInt32 GetMinPosition(nsIContent* content);
  static PRInt32 GetMaxPosition(nsIContent* content);
  static PRInt32 GetIncrement(nsIContent* content);
  static PRInt32 GetPageIncrement(nsIContent* content);
  static PRInt32 GetIntegerAttribute(nsIContent* content, nsIAtom* atom, PRInt32 defaultValue);
  void EnsureOrient();

  void SetScrollbarListener(nsIScrollbarListener* aListener);

  virtual nsIView* GetMouseCapturer() const { return GetView(); }

  NS_IMETHOD HandlePress(nsPresContext* aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus*  aEventStatus);

  NS_IMETHOD HandleMultiplePress(nsPresContext* aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus*  aEventStatus)  { return NS_OK; }

  NS_IMETHOD HandleDrag(nsPresContext* aPresContext,
                        nsGUIEvent *    aEvent,
                        nsEventStatus*  aEventStatus)  { return NS_OK; }

  NS_IMETHOD HandleRelease(nsPresContext* aPresContext,
                           nsGUIEvent *    aEvent,
                           nsEventStatus*  aEventStatus);

  NS_IMETHOD_(void) Notify(nsITimer *timer);
 
private:

  nsIBox* GetScrollbar();

  void PageUpDown(nscoord change);
  void SetCurrentPosition(nsIContent* scrollbar, nscoord pos, PRBool aIsSmooth);
  void SetCurrentPositionInternal(nsIContent* scrollbar, nscoord pos, PRBool aIsSmooth);
  void DragThumb(PRBool aGrabMouseEvents);
  void AddListener();
  void RemoveListener();
  PRBool isDraggingThumb();

  float mRatio;

  nscoord mDragStart;
  nscoord mThumbStart;

  PRInt32 mCurPos;

  nsIScrollbarListener* mScrollbarListener;

  nscoord mChange;
  nsPoint mDestinationPoint;
  nsSliderMediator* mMediator;

  PRPackedBool mRedrawImmediate;

  static PRBool gMiddlePref;
  static PRInt32 gSnapMultiplier;
}; // class nsSliderFrame

#endif
