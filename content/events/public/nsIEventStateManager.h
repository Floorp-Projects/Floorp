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

class nsIContent;
class nsIDocument;
class nsPresContext;
class nsIDOMEvent;
class nsIFrame;
class nsIView;
class nsIWidget;
class imgIContainer;

/*
 * Event state manager interface.
 */
// {92EDD580-062E-4471-ADEB-68329B0EC2E4}
#define NS_IEVENTSTATEMANAGER_IID \
{ 0x92edd580, 0x062e, 0x4471, \
  { 0xad, 0xeb, 0x68, 0x32, 0x9b, 0x0e, 0xc2, 0xe4 } }

#define NS_EVENT_NEEDS_FRAME(event) (!NS_IS_ACTIVATION_EVENT(event))

class nsIEventStateManager : public nsISupports {

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IEVENTSTATEMANAGER_IID)

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

  /**
   * Returns the content state of aContent.
   * @param aContent      The control whose state is requested.
   * @param aFollowLabels Whether to reflect a label's content state on its
   *                      associated control. If aFollowLabels is true and
   *                      aContent is a control which has a label that has the 
   *                      hover or active content state set, GetContentState
   *                      will pretend that those states are also set on aContent.
   * @return              The content state.
   */
  virtual PRInt32 GetContentState(nsIContent *aContent,
                                  PRBool aFollowLabels = PR_FALSE) = 0;

  /**
   * Notify that the given NS_EVENT_STATE_* bit has changed for this content.
   * @param aContent Content which has changed states
   * @param aState   Corresponding state flags such as NS_EVENT_STATE_FOCUS 
   *                 defined in the nsIEventStateManager interface
   * @return  Whether the content was able to change all states. Returns PR_FALSE
   *                  if a resulting DOM event causes the content node passed in
   *                  to not change states. Note, the frame for the content may
   *                  change as a result of the content state change, because of
   *                  frame reconstructions that may occur, but this does not
   *                  affect the return value.
   */
  virtual PRBool SetContentState(nsIContent *aContent, PRInt32 aState) = 0;

  NS_IMETHOD ContentRemoved(nsIDocument* aDocument, nsIContent* aContent) = 0;
  NS_IMETHOD EventStatusOK(nsGUIEvent* aEvent, PRBool *aOK) = 0;

  // Access Key Registration

  /**
   * Register accesskey on the given element. When accesskey is activated then
   * the element will be notified via nsIContent::PerformAccesskey() method.
   *
   * @param  aContent  the given element
   * @param  aKey      accesskey
   */
  NS_IMETHOD RegisterAccessKey(nsIContent* aContent, PRUint32 aKey) = 0;

  /**
   * Unregister accesskey for the given element.
   *
   * @param  aContent  the given element
   * @param  aKey      accesskey
   */
  NS_IMETHOD UnregisterAccessKey(nsIContent* aContent, PRUint32 aKey) = 0;

  /**
   * Get accesskey registered on the given element or 0 if there is none.
   *
   * @param  aContent  the given element
   * @param  aKey      registered accesskey
   * @return           NS_OK
   */
  NS_IMETHOD GetRegisteredAccessKey(nsIContent* aContent, PRUint32* aKey) = 0;

  NS_IMETHOD SetCursor(PRInt32 aCursor, imgIContainer* aContainer,
                       PRBool aHaveHotspot, float aHotspotX, float aHotspotY,
                       nsIWidget* aWidget, PRBool aLockCursor) = 0;

  NS_IMETHOD NotifyDestroyPresContext(nsPresContext* aPresContext) = 0;
  
  /**
   * Returns true if the current code is being executed as a result of user input.
   * This includes timers or anything else that is initiated from user input.
   * However, mouse hover events are not counted as user input, nor are
   * page load events. If this method is called from asynchronously executed code,
   * such as during layout reflows, it will return false.
   */
  NS_IMETHOD_(PRBool) IsHandlingUserInputExternal() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIEventStateManager, NS_IEVENTSTATEMANAGER_IID)

#define NS_EVENT_STATE_ACTIVE        (1 << 0)  // mouse is down on content
#define NS_EVENT_STATE_FOCUS         (1 << 1)  // content has focus
#define NS_EVENT_STATE_HOVER         (1 << 2)  // mouse is hovering over content
#define NS_EVENT_STATE_DRAGOVER      (1 << 3)  // drag  is hovering over content
#define NS_EVENT_STATE_URLTARGET     (1 << 4)  // content is URL's target (ref)

// The following states are used only for ContentStatesChanged

#define NS_EVENT_STATE_CHECKED       (1 << 5)  // CSS3-Selectors
#define NS_EVENT_STATE_ENABLED       (1 << 6)  // CSS3-Selectors
#define NS_EVENT_STATE_DISABLED      (1 << 7)  // CSS3-Selectors
#define NS_EVENT_STATE_REQUIRED      (1 << 8)  // CSS3-UI
#define NS_EVENT_STATE_OPTIONAL      (1 << 9)  // CSS3-UI
#define NS_EVENT_STATE_VISITED       (1 << 10) // CSS2
#define NS_EVENT_STATE_UNVISITED     (1 << 11)
#define NS_EVENT_STATE_VALID         (1 << 12) // CSS3-UI
#define NS_EVENT_STATE_INVALID       (1 << 13) // CSS3-UI
#define NS_EVENT_STATE_INRANGE       (1 << 14) // CSS3-UI
#define NS_EVENT_STATE_OUTOFRANGE    (1 << 15) // CSS3-UI
// these two are temporary (see bug 302188)
#define NS_EVENT_STATE_MOZ_READONLY  (1 << 16) // CSS3-UI
#define NS_EVENT_STATE_MOZ_READWRITE (1 << 17) // CSS3-UI
#define NS_EVENT_STATE_DEFAULT       (1 << 18) // CSS3-UI

// Content could not be rendered (image/object/etc).
#define NS_EVENT_STATE_BROKEN        (1 << 19)
// Content disabled by the user (images turned off, say)
#define NS_EVENT_STATE_USERDISABLED  (1 << 20)
// Content suppressed by the user (ad blocking, etc)
#define NS_EVENT_STATE_SUPPRESSED    (1 << 21)
// Content is still loading such that there is nothing to show the
// user (eg an image which hasn't started coming in yet)
#define NS_EVENT_STATE_LOADING       (1 << 22)
// Content is of a type that gecko can't handle
#define NS_EVENT_STATE_TYPE_UNSUPPORTED \
                                     (1 << 23)
#ifdef MOZ_MATHML
#define NS_EVENT_STATE_INCREMENT_SCRIPT_LEVEL \
                                     (1 << 24)
#endif
// Handler for the content has been blocked
#define NS_EVENT_STATE_HANDLER_BLOCKED \
                                     (1 << 25)
// Handler for the content has been disabled
#define NS_EVENT_STATE_HANDLER_DISABLED \
                                     (1 << 26)

#define NS_EVENT_STATE_INDETERMINATE (1 << 27) // CSS3-Selectors

// Handler for the content has crashed
#define NS_EVENT_STATE_HANDLER_CRASHED \
                                     (1 << 28)

// content has focus and should show a ring
#define NS_EVENT_STATE_FOCUSRING     (1 << 29)

// Content shows its placeholder
#define NS_EVENT_STATE_MOZ_PLACEHOLDER (1 << 30)

// Content is a submit control and the form isn't valid.
#define NS_EVENT_STATE_MOZ_SUBMITINVALID (1U << 31)

/**
 * WARNING:
 * (1U << 31) should work but we currently handle event states with PRInt32
 * so it's an edge case.
 * DO NOT ADD AN EVENT STATE after NS_EVENT_STATE_MOZ_SUBMITINVALID until we
 * move to PRUint64 and we introduce a type to handle event states.
 * See bug 595036.
 */

#endif // nsIEventStateManager_h__
