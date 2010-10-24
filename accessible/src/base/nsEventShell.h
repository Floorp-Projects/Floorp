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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#ifndef _nsEventShell_H_
#define _nsEventShell_H_

#include "AccEvent.h"

#include "a11yGeneric.h"

#include "nsAutoPtr.h"

#include "nsRefreshDriver.h"

class nsIPersistentProperties;

/**
 * Used for everything about events.
 */
class nsEventShell
{
public:

  /**
   * Fire the accessible event.
   */
  static void FireEvent(AccEvent* aEvent);

  /**
   * Fire accessible event of the given type for the given accessible.
   *
   * @param  aEventType   [in] the event type
   * @param  aAccessible  [in] the event target
   */
  static void FireEvent(PRUint32 aEventType, nsAccessible *aAccessible,
                        EIsFromUserInput aIsFromUserInput = eAutoDetect);

  /**
   * Append 'event-from-input' object attribute if the accessible event has
   * been fired just now for the given node.
   *
   * @param  aNode        [in] the DOM node
   * @param  aAttributes  [in, out] the attributes
   */
  static void GetEventAttributes(nsINode *aNode,
                                 nsIPersistentProperties *aAttributes);

private:
  static nsCOMPtr<nsINode> sEventTargetNode;
  static PRBool sEventFromUserInput;
};


/**
 * Event queue.
 */
class nsAccEventQueue : public nsISupports,
                        public nsARefreshObserver
{
public:
  nsAccEventQueue(nsDocAccessible *aDocument);
  ~nsAccEventQueue();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsAccEventQueue)

  /**
   * Push event to queue, coalesce it if necessary. Start pending processing.
   */
  void Push(AccEvent* aEvent);

  /**
   * Shutdown the queue.
   */
  void Shutdown();

private:

  /**
   * Start pending events processing asynchronously.
   */
  void PrepareFlush();
  
  /**
   * Process pending events. It calls nsDocAccessible::ProcessPendingEvent()
   * where the real event processing is happen.
   */
  virtual void WillRefresh(mozilla::TimeStamp aTime);

  /**
   * Coalesce redundant events from the queue.
   */
  void CoalesceEvents();

  /**
   * Apply aEventRule to same type event that from sibling nodes of aDOMNode.
   * @param aEventsToFire    array of pending events
   * @param aStart           start index of pending events to be scanned
   * @param aEnd             end index to be scanned (not included)
   * @param aEventType       target event type
   * @param aDOMNode         target are siblings of this node
   * @param aEventRule       the event rule to be applied
   *                         (should be eDoNotEmit or eAllowDupes)
   */
  void ApplyToSiblings(PRUint32 aStart, PRUint32 aEnd,
                       PRUint32 aEventType, nsINode* aNode,
                       AccEvent::EEventRule aEventRule);

  /**
   * Do not emit one of two given reorder events fired for DOM nodes in the case
   * when one DOM node is in parent chain of second one.
   */
  void CoalesceReorderEventsFromSameTree(AccEvent* aAccEvent,
                                         AccEvent* aDescendantAccEvent);

  /**
   * Coalesce text change events caused by sibling hide events.
   */
  void CoalesceTextChangeEventsFor(AccHideEvent* aTailEvent,
                                   AccHideEvent* aThisEvent);
  void CoalesceTextChangeEventsFor(AccShowEvent* aTailEvent,
                                   AccShowEvent* aThisEvent);

  /**
   * Create text change event caused by hide or show event. When a node is
   * hidden/removed or shown/appended, the text in an ancestor hyper text will
   * lose or get new characters.
   */
  void CreateTextChangeEventFor(AccMutationEvent* aEvent);

  /**
   * Indicates whether we're waiting on a refresh notification from our
   * presshell to flush events
   */
  PRBool mObservingRefresh;

  /**
   * The document accessible reference owning this queue.
   */
  nsRefPtr<nsDocAccessible> mDocument;

  /**
   * Pending events array.  Don't make this an nsAutoTArray; we use
   * SwapElements() on it.
   */
  nsTArray<nsRefPtr<AccEvent> > mEvents;
};

#endif
