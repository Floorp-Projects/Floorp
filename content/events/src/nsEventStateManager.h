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

#ifndef nsEventStateManager_h__
#define nsEventStateManager_h__

#include "nsIEventStateManager.h"
#include "nsGUIEvent.h"
class nsIDocument;
class nsIScrollableView;
class nsISelfScrollingFrame;

/*
 * Event listener manager
 */

class nsEventStateManager : public nsIEventStateManager {

public:
  nsEventStateManager();
  virtual ~nsEventStateManager();

  NS_DECL_ISUPPORTS

  NS_IMETHOD PreHandleEvent(nsIPresContext* aPresContext,
                         nsGUIEvent *aEvent,
                         nsIFrame* aTargetFrame,
                         nsEventStatus* aStatus,
                         nsIView* aView);

  NS_IMETHOD PostHandleEvent(nsIPresContext* aPresContext,
                         nsGUIEvent *aEvent,
                         nsIFrame* aTargetFrame,
                         nsEventStatus* aStatus,
                         nsIView* aView);

  NS_IMETHOD SetPresContext(nsIPresContext* aPresContext);
  NS_IMETHOD ClearFrameRefs(nsIFrame* aFrame);

  NS_IMETHOD GetEventTarget(nsIFrame **aFrame);
  NS_IMETHOD GetEventTargetContent(nsIContent** aContent);
  NS_IMETHOD GetEventRelatedContent(nsIContent** aContent);

  NS_IMETHOD GetContentState(nsIContent *aContent, PRInt32& aState);
  NS_IMETHOD SetContentState(nsIContent *aContent, PRInt32 aState);
  NS_IMETHOD GetFocusedContent(nsIContent **aContent);

  // This is an experiement and may be temporary
  NS_IMETHOD ConsumeFocusEvents(PRBool aDoConsume) { mConsumeFocusEvents = aDoConsume; return NS_OK; }

protected:
  void UpdateCursor(nsIPresContext* aPresContext, nsPoint& aPoint, nsIFrame* aTargetFrame, nsEventStatus* aStatus);
  void GenerateMouseEnterExit(nsIPresContext* aPresContext, nsGUIEvent* aEvent);
  void GenerateDragDropEnterExit(nsIPresContext* aPresContext, nsGUIEvent* aEvent);
  NS_IMETHOD DispatchKeyPressEvent(nsIPresContext* aPresContext, nsKeyEvent *aEvent, nsEventStatus* aStatus);  
  NS_IMETHOD SetClickCount(nsIPresContext* aPresContext, nsMouseEvent *aEvent, nsEventStatus* aStatus);  
  NS_IMETHOD CheckForAndDispatchClick(nsIPresContext* aPresContext, nsMouseEvent *aEvent, nsEventStatus* aStatus);  
  PRBool ChangeFocus(nsIContent* aFocus, nsIFrame* aFocusFrame, PRBool aSetFocus);
  void ShiftFocus(PRBool foward);
  nsIContent* GetNextTabbableContent(nsIContent* aParent, nsIContent* aChild, nsIContent* aTop, PRBool foward);
  PRInt32 GetNextTabIndex(nsIContent* aParent, PRBool foward);
  NS_IMETHOD SendFocusBlur(nsIPresContext* aPresContext, nsIContent *aContent);
  nsIScrollableView* GetNearestScrollingView(nsIView* aView);
  nsISelfScrollingFrame* GetNearestSelfScrollingFrame(nsIFrame* aFrame);

  nsIFrame* GetDocumentFrame(nsIPresContext* aPresContext);

  // routines for the d&d gesture tracking state machine
  void BeginTrackingDragGesture ( nsGUIEvent* inDownEvent, nsIFrame* inDownFrame ) ;
  void StopTrackingDragGesture ( ) ;
  void GenerateDragGesture ( nsIPresContext* aPresContext, nsGUIEvent *aEvent ) ;
  PRBool IsTrackingDragGesture ( ) const { return mIsTrackingDragGesture; }

  //Any frames here must be checked for validity in ClearFrameRefs
  nsIFrame* mCurrentTarget;
  nsIContent* mCurrentTargetContent;
  nsIContent* mCurrentRelatedContent;
  nsIFrame* mLastMouseOverFrame;
  nsIFrame* mLastDragOverFrame;
  
  // member variables for the d&d gesture state machine
  PRBool mIsTrackingDragGesture;
  nsPoint mGestureDownPoint;
  nsIFrame* mGestureDownFrame;

  nsIContent* mLastLeftMouseDownContent;
  nsIContent* mLastMiddleMouseDownContent;
  nsIContent* mLastRightMouseDownContent;

  nsIContent* mActiveContent;
  nsIContent* mHoverContent;
  nsIContent* mDragOverContent;
  nsIContent* mCurrentFocus;
  PRInt32 mCurrentTabIndex;
  nsIWidget * mLastWindowToHaveFocus; // last native window to get focus via the evs
  PRBool      mConsumeFocusEvents;

  nsIPresContext* mPresContext;      // Not refcnted
  nsIDocument* mDocument;            // [OWNER], but doesn't need to be.

  PRUint32 mLClickCount;
  PRUint32 mMClickCount;
  PRUint32 mRClickCount;

  static PRUint32 mInstanceCount;

};

extern nsresult NS_NewEventStateManager(nsIEventStateManager** aInstancePtrResult);

#endif // nsEventStateManager_h__
