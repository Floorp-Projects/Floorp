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

#ifndef nsEventStateManager_h__
#define nsEventStateManager_h__

#include "nsIEventStateManager.h"
#include "nsGUIEvent.h"
class nsIDocument;

/*
 * Event listener manager
 */

class nsEventStateManager : public nsIEventStateManager {

public:
  nsEventStateManager();
  virtual ~nsEventStateManager();

  NS_DECL_ISUPPORTS

  NS_IMETHOD PreHandleEvent(nsIPresContext& aPresContext,
                         nsGUIEvent *aEvent,
                         nsIFrame* aTargetFrame,
                         nsEventStatus& aStatus);

  NS_IMETHOD PostHandleEvent(nsIPresContext& aPresContext,
                         nsGUIEvent *aEvent,
                         nsIFrame* aTargetFrame,
                         nsEventStatus& aStatus);

  NS_IMETHOD SetPresContext(nsIPresContext* aPresContext);
  NS_IMETHOD ClearFrameRefs(nsIFrame* aFrame);

  NS_IMETHOD GetEventTarget(nsIFrame **aFrame);

  NS_IMETHOD GetActiveLink(nsIContent **aLink);
  NS_IMETHOD SetActiveLink(nsIContent *aLink);


protected:
  void UpdateCursor(nsIPresContext& aPresContext, nsPoint& aPoint, nsIFrame* aTargetFrame);
  void GenerateMouseEnterExit(nsIPresContext& aPresContext, nsGUIEvent* aEvent);
  NS_IMETHOD DispatchKeyPressEvent(nsIPresContext& aPresContext, nsKeyEvent *aEvent, nsEventStatus& aStatus);  
  NS_IMETHOD CheckForAndDispatchClick(nsIPresContext& aPresContext, nsMouseEvent *aEvent, nsEventStatus& aStatus);  
  void ShiftFocus();
  nsIContent* GetNextTabbableContent(nsIContent* aParent, nsIContent* aChild, nsIContent* aTop);

  //Any frames here must be checked for validity in ClearFrameRefs
  nsIFrame* mCurrentTarget;
  nsIFrame* mLastMouseOverFrame;
  nsIFrame* mLastLeftMouseDownFrame;
  nsIFrame* mLastMiddleMouseDownFrame;
  nsIFrame* mLastRightMouseDownFrame;

  nsIContent* mActiveLink;
  nsIContent* mCurrentFocus;

  //Not refcnted
  nsIPresContext* mPresContext;
  nsIDocument* mDocument;
};

extern nsresult NS_NewEventStateManager(nsIEventStateManager** aInstancePtrResult);

#endif // nsEventStateManager_h__
