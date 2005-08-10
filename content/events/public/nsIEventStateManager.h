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
class imgIContainer;

/*
 * Event state manager interface.
 */
// 2270e188-6743-441e-b6e1-af83f1047a53
#define NS_IEVENTSTATEMANAGER_IID \
{ 0x2270e188, 0x6743, 0x441e, \
  { 0xb6, 0xe1, 0xaf, 0x83, 0xf1, 0x04, 0x7a, 0x53 } }


#define NS_EVENT_NEEDS_FRAME(event) (!NS_IS_FOCUS_EVENT(event))

class nsIEventStateManager : public nsISupports {

public:
  enum EFocusedWithType {
    eEventFocusedByUnknown,     // focus gained via unknown method
    eEventFocusedByMouse,       // focus gained via mouse
    eEventFocusedByKey,         // focus gained via key press (like tab)
    eEventFocusedByContextMenu, // focus gained via context menu
    eEventFocusedByApplication  // focus gained via Application (like script)
  };

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

  NS_IMETHOD SetPresContext(nsPresContext* aPresContext) = 0;
  NS_IMETHOD ClearFrameRefs(nsIFrame* aFrame) = 0;

  NS_IMETHOD GetEventTarget(nsIFrame **aFrame) = 0;
  NS_IMETHOD GetEventTargetContent(nsEvent* aEvent, nsIContent** aContent) = 0;
  NS_IMETHOD GetEventRelatedContent(nsIContent** aContent) = 0;

  NS_IMETHOD GetContentState(nsIContent *aContent, PRInt32& aState) = 0;
  NS_IMETHOD SetContentState(nsIContent *aContent, PRInt32 aState) = 0;

  NS_IMETHOD GetFocusedContent(nsIContent **aContent) = 0;
  NS_IMETHOD SetFocusedContent(nsIContent* aContent) = 0;

  // Get the previously-focused content node for this document
  NS_IMETHOD GetLastFocusedContent(nsIContent **aContent) = 0;

  NS_IMETHOD GetFocusedFrame(nsIFrame **aFrame) = 0;

  NS_IMETHOD ContentRemoved(nsIContent* aContent) = 0;
  NS_IMETHOD EventStatusOK(nsGUIEvent* aEvent, PRBool *aOK) = 0;

  // Return whether browse with caret is enabled or not
  virtual PRBool GetBrowseWithCaret() = 0;

  // This is called after find text or when a cursor movement key is pressed
  // If aCanFocusDoc == PR_TRUE, the current document will be focused if caret is not on a focusable element
  NS_IMETHOD MoveFocusToCaret(PRBool aCanFocusDoc, PRBool *aIsSelectionWithFocus) = 0;
  NS_IMETHOD MoveCaretToFocus() = 0;

  // Set focus on any element that can receive focus, or on document via aFocusContent == nsnull
  // Must supply method that focus is being set with
  NS_IMETHOD ChangeFocusWith(nsIContent *aFocusContent, EFocusedWithType aFocusedWith) = 0;

  // This is an experiment and may be temporary
  NS_IMETHOD ConsumeFocusEvents(PRBool aDoConsume) = 0;

  // Access Key Registration
  NS_IMETHOD RegisterAccessKey(nsIContent* aContent, PRUint32 aKey) = 0;
  NS_IMETHOD UnregisterAccessKey(nsIContent* aContent, PRUint32 aKey) = 0;

  NS_IMETHOD SetCursor(PRInt32 aCursor, imgIContainer* aContainer,
                       PRBool aHaveHotspot, float aHotspotX, float aHotspotY,
                       nsIWidget* aWidget, PRBool aLockCursor) = 0;

  //Method for centralized distribution of new DOM events
  NS_IMETHOD DispatchNewEvent(nsISupports* aTarget, nsIDOMEvent* aEvent, PRBool* aDefaultActionEnabled) = 0;

  // Method for moving the focus forward/back.
  NS_IMETHOD ShiftFocus(PRBool aDirection, nsIContent* aStart)=0;
};

#define NS_EVENT_STATE_ACTIVE       0x00000001 // mouse is down on content
#define NS_EVENT_STATE_FOCUS        0x00000002 // content has focus
#define NS_EVENT_STATE_HOVER        0x00000004 // mouse is hovering over content
#define NS_EVENT_STATE_DRAGOVER     0x00000008 // drag  is hovering over content
#define NS_EVENT_STATE_URLTARGET    0x00000010 // content is URL's target (ref)

// The following states are used only for ContentStatesChanged
// CSS 3 Selectors
#define NS_EVENT_STATE_CHECKED      0x00000020
// CSS 3 UI
#define NS_EVENT_STATE_REQUIRED     0x00000040
#define NS_EVENT_STATE_OPTIONAL     0x00000080
#define NS_EVENT_STATE_VISITED      0x00000100

#endif // nsIEventStateManager_h__
