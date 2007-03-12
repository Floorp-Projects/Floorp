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
 * Olli Pettay (Olli.Pettay@helsinki.fi)
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#ifdef MOZILLA_INTERNAL_API
#ifndef nsEventDispatcher_h___
#define nsEventDispatcher_h___

#include "nsGUIEvent.h"

class nsIContent;
class nsIDocument;
class nsPresContext;
class nsPIDOMEventTarget;
class nsIScriptGlobalObject;
class nsEventTargetChainItem;


/**
 * About event dispatching:
 * When either nsEventDispatcher::Dispatch or
 * nsEventDispatcher::DispatchDOMEvent is called an event target chain is
 * created. nsEventDispatcher creates the chain by calling PreHandleEvent 
 * on each event target and the creation continues until either the mCanHandle
 * member of the nsEventChainPreVisitor object is PR_FALSE or the mParentTarget
 * does not point to a new target. The event target chain is created in the
 * heap.
 *
 * If the event needs retargeting, mEventTargetAtParent must be set in
 * PreHandleEvent.
 *
 * The capture, target and bubble phases of the event dispatch are handled
 * by iterating through the event target chain. Iteration happens twice,
 * first for the default event group and then for the system event group.
 * While dispatching the event for the system event group PostHandleEvent
 * is called right after calling event listener for the current event target.
 */

class nsEventChainVisitor {
public:
  nsEventChainVisitor(nsPresContext* aPresContext,
                      nsEvent* aEvent,
                      nsIDOMEvent* aDOMEvent,
                      nsEventStatus aEventStatus = nsEventStatus_eIgnore)
  : mPresContext(aPresContext), mEvent(aEvent), mDOMEvent(aDOMEvent),
    mEventStatus(aEventStatus), mItemFlags(0)
  {}

  /**
   * The prescontext, possibly nsnull.
   */
  nsPresContext* const  mPresContext;

  /**
   * The nsEvent which is being dispatched. Never nsnull.
   */
  nsEvent* const        mEvent;

  /**
   * The DOM Event assiciated with the mEvent. Possibly nsnull if a DOM Event
   * is not (yet) created.
   */
  nsIDOMEvent*          mDOMEvent;

  /**
   * The status of the event.
   * @see nsEventStatus.h
   */
  nsEventStatus         mEventStatus;

  /**
   * Bits for items in the event target chain.
   * Set in PreHandleEvent() and used in PostHandleEvent().
   *
   * @note These bits are different for each item in the event target chain.
   *       It is up to the Pre/PostHandleEvent implementation to decide how to
   *       use these bits.
   *
   * @note Using PRUint16 because that is used also in nsEventTargetChainItem.
   */
  PRUint16              mItemFlags;

  /**
   * Data for items in the event target chain.
   * Set in PreHandleEvent() and used in PostHandleEvent().
   *
   * @note This data is different for each item in the event target chain.
   *       It is up to the Pre/PostHandleEvent implementation to decide how to
   *       use this.
   */
  nsCOMPtr<nsISupports> mItemData;
};

class nsEventChainPreVisitor : public nsEventChainVisitor {
public:
  nsEventChainPreVisitor(nsPresContext* aPresContext,
                         nsEvent* aEvent,
                         nsIDOMEvent* aDOMEvent,
                         nsEventStatus aEventStatus = nsEventStatus_eIgnore)
  : nsEventChainVisitor(aPresContext, aEvent, aDOMEvent, aEventStatus),
    mCanHandle(PR_TRUE), mForceContentDispatch(PR_FALSE) {}

  void Reset() {
    mItemFlags = 0;
    mItemData = nsnull;
    mCanHandle = PR_TRUE;
    mForceContentDispatch = PR_FALSE;
    mParentTarget = nsnull;
    mEventTargetAtParent = nsnull;
  }

  /**
   * Member that must be set in PreHandleEvent by event targets. If set to false,
   * indicates that this event target will not be handling the event and
   * construction of the event target chain is complete. The target that sets
   * mCanHandle to false is NOT included in the event target chain.
   */
  PRPackedBool          mCanHandle;

  /**
   * If mForceContentDispatch is set to PR_TRUE,
   * content dispatching is not disabled for this event target.
   * FIXME! This is here for backward compatibility. Bug 329119
   */
  PRPackedBool          mForceContentDispatch;

  /**
   * Parent item in the event target chain.
   */
  nsCOMPtr<nsISupports> mParentTarget;

  /**
   * If the event needs to be retargeted, this is the event target,
   * which should be used when the event is handled at mParentTarget.
   */
  nsCOMPtr<nsISupports> mEventTargetAtParent;
};

class nsEventChainPostVisitor : public nsEventChainVisitor {
public:
  nsEventChainPostVisitor(nsEventChainVisitor& aOther)
  : nsEventChainVisitor(aOther.mPresContext, aOther.mEvent, aOther.mDOMEvent,
                        aOther.mEventStatus)
  {}
};

/**
 * If an nsDispatchingCallback object is passed to Dispatch,
 * its HandleEvent method is called after handling the default event group,
 * before handling the system event group.
 * This is used in nsPresShell.
 */
class nsDispatchingCallback {
public:
  virtual void HandleEvent(nsEventChainPostVisitor& aVisitor) = 0;
};

/**
 * The generic class for event dispatching.
 * Must not be used outside Gecko!
 */
class nsEventDispatcher
{
public:
  /**
   * aTarget should QI to nsPIDOMEventTarget.
   * If the target of aEvent is set before calling this method, the target of 
   * aEvent is used as the target (unless there is event
   * retargeting) and the originalTarget of the DOM Event.
   * aTarget is always used as the starting point for constructing the event
   * target chain, no matter what the value of aEvent->target is.
   * In other words, aEvent->target is only a property of the event and it has
   * nothing to do with the construction of the event target chain.
   * Neither aTarget nor aEvent is allowed to be nsnull.
   * @note Use this method when dispatching an nsEvent.
   */
  static nsresult Dispatch(nsISupports* aTarget,
                           nsPresContext* aPresContext,
                           nsEvent* aEvent,
                           nsIDOMEvent* aDOMEvent = nsnull,
                           nsEventStatus* aEventStatus = nsnull,
                           nsDispatchingCallback* aCallback = nsnull);

  /**
   * Dispatches an event.
   * If aDOMEvent is not nsnull, it is used for dispatching
   * (aEvent can then be nsnull) and (if aDOMEvent is not |trusted| already),
   * the |trusted| flag is set based on the UniversalBrowserWrite capability.
   * Otherwise this works like nsEventDispatcher::Dispatch.
   * @note Use this method when dispatching nsIDOMEvent.
   */
  static nsresult DispatchDOMEvent(nsISupports* aTarget,
                                   nsEvent* aEvent, nsIDOMEvent* aDOMEvent,
                                   nsPresContext* aPresContext,
                                   nsEventStatus* aEventStatus);

  /**
   * Creates a DOM Event.
   */
  static nsresult CreateEvent(nsPresContext* aPresContext,
                              nsEvent* aEvent,
                              const nsAString& aEventType,
                              nsIDOMEvent** aDOMEvent);

};

#endif
#endif
