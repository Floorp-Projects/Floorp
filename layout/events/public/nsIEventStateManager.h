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

#ifndef nsIEventStateManager_h__
#define nsIEventStateManager_h__

#include "nsGUIEvent.h"
#include "nsISupports.h"
#include "nsVoidArray.h"

class nsIContent;
class nsIPresContext;
class nsIDOMEvent;
class nsIFrame;
class nsIView;
class nsIWidget;

/*
 * Event listener manager interface.
 */
#define NS_IEVENTSTATEMANAGER_IID \
{ /* 80a98c80-2036-11d2-bd89-00805f8ae3f4 */ \
0x80a98c80, 0x2036, 0x11d2, \
{0xbd, 0x89, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsIEventStateManager : public nsISupports {

public:
  static const nsIID& GetIID() { static nsIID iid = NS_IEVENTSTATEMANAGER_IID; return iid; }

  NS_IMETHOD Init() = 0;

  NS_IMETHOD PreHandleEvent(nsIPresContext* aPresContext, 
                         nsEvent *aEvent, 
                         nsIFrame* aTargetFrame,
                         nsEventStatus* aStatus,
                         nsIView* aView) = 0;

  NS_IMETHOD PostHandleEvent(nsIPresContext* aPresContext, 
                         nsEvent *aEvent, 
                         nsIFrame* aTargetFrame,
                         nsEventStatus* aStatus,
                         nsIView* aView) = 0;

  NS_IMETHOD SetPresContext(nsIPresContext* aPresContext) = 0;
  NS_IMETHOD ClearFrameRefs(nsIFrame* aFrame) = 0;

  NS_IMETHOD GetEventTarget(nsIFrame **aFrame) = 0;
  NS_IMETHOD GetEventTargetContent(nsEvent* aEvent, nsIContent** aContent) = 0;
  NS_IMETHOD GetEventRelatedContent(nsIContent** aContent) = 0;

  NS_IMETHOD GetContentState(nsIContent *aContent, PRInt32& aState) = 0;
  NS_IMETHOD SetContentState(nsIContent *aContent, PRInt32 aState) = 0;

  NS_IMETHOD GetFocusedContent(nsIContent **aContent) = 0;
  NS_IMETHOD SetFocusedContent(nsIContent* aContent) = 0;

  // This is an experiement and may be temporary
  NS_IMETHOD ConsumeFocusEvents(PRBool aDoConsume) = 0;

  // Access Key Registration
  NS_IMETHOD RegisterAccessKey(nsIFrame * aFrame, PRUint32 aKey) = 0;
  NS_IMETHOD UnregisterAccessKey(nsIFrame * aFrame) = 0;

  NS_IMETHOD SetCursor(PRInt32 aCursor, nsIWidget* aWidget, PRBool aLockCursor) = 0;

};

#define NS_EVENT_STATE_UNSPECIFIED  0x0000
#define NS_EVENT_STATE_ACTIVE       0x0001 // mouse is down on content
#define NS_EVENT_STATE_FOCUS        0x0002 // content has focus
#define NS_EVENT_STATE_HOVER        0x0004 // mouse is hovering over content
#define NS_EVENT_STATE_DRAGOVER     0x0008 // drag  is hovering over content

#endif // nsIEventStateManager_h__
