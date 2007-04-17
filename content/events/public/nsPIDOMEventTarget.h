/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     Olli Pettay <Olli.Pettay@helsinki.fi> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsPIDOMEventTarget_h_
#define nsPIDOMEventTarget_h_

#include "nsISupports.h"
#include "nsEvent.h"

class nsIDOMEvent;
class nsPresContext;
class nsEventChainPreVisitor;
class nsEventChainPostVisitor;
class nsIEventListenerManager;

// 360fa72e-c709-42cc-9285-1f755ec90376
#define NS_PIDOMEVENTTARGET_IID \
{ 0x360fa72e, 0xc709, 0x42cc, \
  { 0x92, 0x85, 0x1f, 0x75, 0x5e, 0xc9, 0x03, 0x76 } }

class nsPIDOMEventTarget : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PIDOMEVENTTARGET_IID)

  /**
   * Returns the nsPIDOMEventTarget object which should be used as the target
   * of DOMEvents.
   * Usually |this| is returned, but for example global object returns
   * the outer object.
   */
   virtual nsPIDOMEventTarget* GetTargetForDOMEvent() { return this; }

  /**
   * Returns the nsPIDOMEventTarget object which should be used as the target
   * of the event and when constructing event target chain.
   * Usually |this| is returned, but for example global object returns
   * the inner object.
   */
   virtual nsPIDOMEventTarget* GetTargetForEventTargetChain() { return this; }

  /**
   * Called before the capture phase of the event flow.
   * This is used to create the event target chain and implementations
   * should set the necessary members of nsEventChainPreVisitor.
   * At least aVisitor.mCanHandle must be set,
   * usually also aVisitor.mParentTarget if mCanHandle is PR_TRUE.
   * First one tells that this object can handle the aVisitor.mEvent event and
   * the latter one is the possible parent object for the event target chain.
   * @see nsEventDispatcher.h for more documentation about aVisitor.
   *
   * @param aVisitor the visitor object which is used to create the
   *                 event target chain for event dispatching.
   *
   * @note Only nsEventDispatcher should call this method.
   */
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor) = 0;

  /**
   * Called after the bubble phase of the system event group.
   * The default handling of the event should happen here.
   * @param aVisitor the visitor object which is used during post handling.
   *
   * @see nsEventDispatcher.h for documentation about aVisitor.
   * @note Only nsEventDispatcher should call this method.
   */
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor) = 0;

  /**
   * Dispatch an event.
   * @param aEvent the event that is being dispatched.
   * @param aDOMEvent the event that is being dispatched, use if you want to
   *                  dispatch nsIDOMEvent, not only nsEvent.
   * @param aPresContext the current presentation context, can be nsnull.
   * @param aEventStatus the status returned from the function, can be nsnull.
   *
   * @note If both aEvent and aDOMEvent are used, aEvent must be the internal
   *       event of the aDOMEvent.
   *
   * If aDOMEvent is not nsnull (in which case aEvent can be nsnull) it is used
   * for dispatching, otherwise aEvent is used.
   *
   * @deprecated This method is here just until all the callers outside Gecko
   *             have been converted to use nsIDOMEventTarget::dispatchEvent.
   */
  virtual nsresult DispatchDOMEvent(nsEvent* aEvent,
                                    nsIDOMEvent* aDOMEvent,
                                    nsPresContext* aPresContext,
                                    nsEventStatus* aEventStatus) = 0;

  /**
   * Get the event listener manager, the guy you talk to to register for events
   * on this node.
   * @param aCreateIfNotFound If PR_FALSE, returns a listener manager only if
   *                          one already exists. [IN]
   * @param aResult           The event listener manager [OUT]
   */
  NS_IMETHOD GetListenerManager(PRBool aCreateIfNotFound,
                                nsIEventListenerManager** aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPIDOMEventTarget, NS_PIDOMEVENTTARGET_IID)

#endif // !defined(nsPIDOMEventTarget_h_)
