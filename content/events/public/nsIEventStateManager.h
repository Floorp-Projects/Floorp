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

#ifndef nsIEventStateManager_h__
#define nsIEventStateManager_h__

#include "nsEvent.h"
#include "nsISupports.h"
#include "nsVoidArray.h"

class nsIContent;
class nsPresContext;
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

#define NS_EVENT_NEEDS_FRAME(event) (!NS_IS_FOCUS_EVENT(event))

class nsIEventStateManager : public nsISupports {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IEVENTSTATEMANAGER_IID)

  NS_IMETHOD Init() = 0;

  NS_IMETHOD PreHandleEvent(nsPresContext* aPresContext, 
                            nsEvent *aEvent, 
                            nsIFrame* aTargetFrame,
                            nsEventStatus* aStatus,
                            nsIView* aView) = 0;

  NS_IMETHOD PostHandleEvent(nsPresContext* aPresContext, 
                             nsEvent *aEvent, 
                             nsIFrame* aTargetFrame,
                             nsEventStatus* aStatus,
                             nsIView* aView) = 0;

  // optional notification methods so downstream clients can know
  // what event is being processed
  NS_IMETHOD SetCurrentEvent(nsEvent *aEvent) = 0;
  NS_IMETHOD GetCurrentEvent(nsEvent **aEvent) = 0; // 0 if none

  NS_IMETHOD SetPresContext(nsPresContext* aPresContext) = 0;
  NS_IMETHOD ClearFrameRefs(nsIFrame* aFrame) = 0;

  NS_IMETHOD GetEventTarget(nsIFrame **aFrame) = 0;
  NS_IMETHOD GetEventTargetContent(nsEvent* aEvent, nsIContent** aContent) = 0;
  NS_IMETHOD GetEventRelatedContent(nsIContent** aContent) = 0;

  NS_IMETHOD GetContentState(nsIContent *aContent, PRInt32& aState) = 0;
  NS_IMETHOD SetContentState(nsIContent *aContent, PRInt32 aState) = 0;

  NS_IMETHOD GetFocusedContent(nsIContent **aContent) = 0;
  NS_IMETHOD SetFocusedContent(nsIContent* aContent) = 0;

  NS_IMETHOD GetFocusedFrame(nsIFrame **aFrame) = 0;

  NS_IMETHOD ContentRemoved(nsIContent* aContent) = 0;
  NS_IMETHOD EventStatusOK(nsGUIEvent* aEvent, PRBool *aOK) = 0;

  // Return whether browse with caret is enabled or not
  virtual PRBool GetBrowseWithCaret() = 0;

  // This is called after find text or when a cursor movement key is pressed
  // If aCanFocusDoc == PR_TRUE, the current document will be focused if caret is not on a focusable element
  NS_IMETHOD MoveFocusToCaret(PRBool aCanFocusDoc, PRBool *aIsSelectionWithFocus) = 0;
  NS_IMETHOD MoveCaretToFocus() = 0;

  // This is an experiment and may be temporary
  NS_IMETHOD ConsumeFocusEvents(PRBool aDoConsume) = 0;

  // Access Key Registration
  NS_IMETHOD RegisterAccessKey(nsIContent* aContent, PRUint32 aKey) = 0;
  NS_IMETHOD UnregisterAccessKey(nsIContent* aContent, PRUint32 aKey) = 0;

  NS_IMETHOD SetCursor(PRInt32 aCursor, nsIWidget* aWidget, PRBool aLockCursor) = 0;

  //Method for centralized distribution of new DOM events
  NS_IMETHOD DispatchNewEvent(nsISupports* aTarget, nsIDOMEvent* aEvent, PRBool* aPreventDefault) = 0;

  // Method for moving the focus forward/back.
  NS_IMETHOD ShiftFocus(PRBool aDirection, nsIContent* aStart)=0;

};

#define NS_EVENT_STATE_UNSPECIFIED  0x0000
#define NS_EVENT_STATE_ACTIVE       0x0001 // mouse is down on content
#define NS_EVENT_STATE_FOCUS        0x0002 // content has focus
#define NS_EVENT_STATE_HOVER        0x0004 // mouse is hovering over content
#define NS_EVENT_STATE_DRAGOVER     0x0008 // drag  is hovering over content
#define NS_EVENT_STATE_URLTARGET    0x0010 // content is URL's target (ref)
// The following states are used only for ContentStatesChanged
#define NS_EVENT_STATE_CHECKED      0x0020

enum EFocusedWithType {
  eEventFocusedByUnknown,     // focus gained via unknown method
  eEventFocusedByMouse,       // focus gained via mouse
  eEventFocusedByKey,         // focus gained via key press (like tab)
  eEventFocusedByContextMenu, // focus gained via context menu
  eEventFocusedByApplication  // focus gained via Application (like script)
};

#endif // nsIEventStateManager_h__
