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
 *   Kyle Yuan (kyle.yuan@sun.com)
 *   John Sun (john.sun@sun.com)
 *   Alexander Surkov <surkov.alexander@gmail.com>
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

#ifndef _nsAccessibleEventData_H_
#define _nsAccessibleEventData_H_

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIAccessibleEvent.h"
#include "nsIAccessible.h"
#include "nsIAccessibleDocument.h"
#include "nsIDOMNode.h"
#include "nsString.h"
#include "nsCycleCollectionParticipant.h"

class nsIPresShell;

#define NS_ACCEVENT_IMPL_CID                            \
{  /* 55b89892-a83d-4252-ba78-cbdf53a86936 */           \
  0x55b89892,                                           \
  0xa83d,                                               \
  0x4252,                                               \
  { 0xba, 0x78, 0xcb, 0xdf, 0x53, 0xa8, 0x69, 0x36 }    \
}

class nsAccEvent: public nsIAccessibleEvent
{
public:

  // Rule for accessible events.
  // The rule will be applied when flushing pending events.
  enum EEventRule {
     // eAllowDupes : More than one event of the same type is allowed.
     //    This event will always be emitted.
     eAllowDupes,
     // eCoalesceFromSameSubtree : For events of the same type from the same
     //    subtree or the same node, only the umbrelle event on the ancestor
     //    will be emitted.
     eCoalesceFromSameSubtree,
     // eRemoveDupes : For repeat events, only the newest event in queue
     //    will be emitted.
     eRemoveDupes,
     // eDoNotEmit : This event is confirmed as a duplicate, do not emit it.
     eDoNotEmit
   };

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ACCEVENT_IMPL_CID)

  // Initialize with an nsIAccessible
  nsAccEvent(PRUint32 aEventType, nsIAccessible *aAccessible,
             PRBool aIsAsynch = PR_FALSE,
             EEventRule aEventRule = eRemoveDupes);
  // Initialize with an nsIDOMNode
  nsAccEvent(PRUint32 aEventType, nsIDOMNode *aDOMNode,
             PRBool aIsAsynch = PR_FALSE,
             EEventRule aEventRule = eRemoveDupes);
  virtual ~nsAccEvent() {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsAccEvent)

  NS_DECL_NSIACCESSIBLEEVENT

  static void GetLastEventAttributes(nsIDOMNode *aNode,
                                     nsIPersistentProperties *aAttributes);

protected:
  already_AddRefed<nsIAccessible> GetAccessibleByNode();

  void CaptureIsFromUserInput(PRBool aIsAsynch);
  PRBool mIsFromUserInput;

  PRUint32 mEventType;
  EEventRule mEventRule;
  nsCOMPtr<nsIAccessible> mAccessible;
  nsCOMPtr<nsIDOMNode> mDOMNode;
  nsCOMPtr<nsIAccessibleDocument> mDocAccessible;

private:
  static PRBool gLastEventFromUserInput;
  static nsIDOMNode* gLastEventNodeWeak;

public:
  static PRUint32 EventType(nsIAccessibleEvent *aAccEvent) {
    PRUint32 eventType;
    aAccEvent->GetEventType(&eventType);
    return eventType;
  }
  static EEventRule EventRule(nsIAccessibleEvent *aAccEvent) {
    nsRefPtr<nsAccEvent> accEvent = GetAccEventPtr(aAccEvent);
    return accEvent->mEventRule;
  }
  static PRBool IsFromUserInput(nsIAccessibleEvent *aAccEvent) {
    PRBool isFromUserInput;
    aAccEvent->GetIsFromUserInput(&isFromUserInput);
    return isFromUserInput;
  }

  static void ResetLastInputState()
   {gLastEventFromUserInput = PR_FALSE; gLastEventNodeWeak = nsnull; }

  /**
   * Find and cache the last input state. This will be called automatically
   * for synchronous events. For asynchronous events it should be
   * called from the synchronous code which is the true source of the event,
   * before the event is fired.
   * @param aChangeNode that event will be on
   * @param aForceIsFromUserInput  PR_TRUE if the caller knows that this event was
   *                               caused by user input
   */
  static void PrepareForEvent(nsIDOMNode *aChangeNode,
                              PRBool aForceIsFromUserInput = PR_FALSE);

  /**
   * The input state was previously stored with the nsIAccessibleEvent,
   * so use that state now -- call this when about to flush an event that 
   * was waiting in an event queue
   */
  static void PrepareForEvent(nsIAccessibleEvent *aEvent,
                              PRBool aForceIsFromUserInput = PR_FALSE);

  /**
   * Apply event rules to pending events, this method is called in
   * FlushingPendingEvents().
   * Result of this method:
   *   Event rule of filtered events will be set to eDoNotEmit.
   *   Events with other event rule are good to emit.
   */
  static void ApplyEventRules(nsCOMArray<nsIAccessibleEvent> &aEventsToFire);

private:
  static already_AddRefed<nsAccEvent> GetAccEventPtr(nsIAccessibleEvent *aAccEvent) {
    nsAccEvent* accEvent = nsnull;
    aAccEvent->QueryInterface(NS_GET_IID(nsAccEvent), (void**)&accEvent);
    return accEvent;
  }

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
  static void ApplyToSiblings(nsCOMArray<nsIAccessibleEvent> &aEventsToFire,
                              PRUint32 aStart, PRUint32 aEnd,
                              PRUint32 aEventType, nsIDOMNode* aDOMNode,
                              EEventRule aEventRule);

  /**
   * Do not emit one of two given reorder events fired for the same DOM node.
   */
  static void CoalesceReorderEventsFromSameSource(nsAccEvent *aAccEvent1,
                                                  nsAccEvent *aAccEvent2);

  /**
   * Do not emit one of two given reorder events fired for DOM nodes in the case
   * when one DOM node is in parent chain of second one.
   */
  static void CoalesceReorderEventsFromSameTree(nsAccEvent *aAccEvent,
                                                nsAccEvent *aDescendantAccEvent);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsAccEvent, NS_ACCEVENT_IMPL_CID)


#define NS_ACCREORDEREVENT_IMPL_CID                     \
{  /* f2629eb8-2458-4358-868c-3912b15b767a */           \
  0xf2629eb8,                                           \
  0x2458,                                               \
  0x4358,                                               \
  { 0x86, 0x8c, 0x39, 0x12, 0xb1, 0x5b, 0x76, 0x7a }    \
}

class nsAccReorderEvent : public nsAccEvent
{
public:

  nsAccReorderEvent(nsIAccessible *aAccTarget, PRBool aIsAsynch,
                    PRBool aIsUnconditional, nsIDOMNode *aReasonNode);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ACCREORDEREVENT_IMPL_CID)

  NS_DECL_ISUPPORTS_INHERITED

  /**
   * Return true if event is unconditional, i.e. must be fired.
   */
  PRBool IsUnconditionalEvent();

  /**
   * Return true if changed DOM node has accessible in its tree.
   */
  PRBool HasAccessibleInReasonSubtree();

private:
  PRBool mUnconditionalEvent;
  nsCOMPtr<nsIDOMNode> mReasonNode;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsAccReorderEvent, NS_ACCREORDEREVENT_IMPL_CID)


class nsAccStateChangeEvent: public nsAccEvent,
                             public nsIAccessibleStateChangeEvent
{
public:
  nsAccStateChangeEvent(nsIAccessible *aAccessible,
                        PRUint32 aState, PRBool aIsExtraState,
                        PRBool aIsEnabled);

  nsAccStateChangeEvent(nsIDOMNode *aNode,
                        PRUint32 aState, PRBool aIsExtraState,
                        PRBool aIsEnabled);

  nsAccStateChangeEvent(nsIDOMNode *aNode,
                        PRUint32 aState, PRBool aIsExtraState);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIACCESSIBLEEVENT(nsAccEvent::)
  NS_DECL_NSIACCESSIBLESTATECHANGEEVENT

private:
  PRUint32 mState;
  PRBool mIsExtraState;
  PRBool mIsEnabled;
};

class nsAccTextChangeEvent: public nsAccEvent,
                            public nsIAccessibleTextChangeEvent
{
public:
  nsAccTextChangeEvent(nsIAccessible *aAccessible, PRInt32 aStart, PRUint32 aLength,
                       PRBool aIsInserted, PRBool aIsAsynch = PR_FALSE);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIACCESSIBLEEVENT(nsAccEvent::)
  NS_DECL_NSIACCESSIBLETEXTCHANGEEVENT

private:
  PRInt32 mStart;
  PRUint32 mLength;
  PRBool mIsInserted;
  nsString mModifiedText;
};

class nsAccCaretMoveEvent: public nsAccEvent,
                           public nsIAccessibleCaretMoveEvent
{
public:
  nsAccCaretMoveEvent(nsIAccessible *aAccessible, PRInt32 aCaretOffset);
  nsAccCaretMoveEvent(nsIDOMNode *aNode);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIACCESSIBLEEVENT(nsAccEvent::)
  NS_DECL_NSIACCESSIBLECARETMOVEEVENT

private:
  PRInt32 mCaretOffset;
};

class nsAccTableChangeEvent : public nsAccEvent,
                              public nsIAccessibleTableChangeEvent {
public:
  nsAccTableChangeEvent(nsIAccessible *aAccessible, PRUint32 aEventType,
                        PRInt32 aRowOrColIndex, PRInt32 aNumRowsOrCols,
                        PRBool aIsAsynch);

  NS_DECL_ISUPPORTS
  NS_FORWARD_NSIACCESSIBLEEVENT(nsAccEvent::)
  NS_DECL_NSIACCESSIBLETABLECHANGEEVENT

private:
  PRUint32 mRowOrColIndex;   // the start row/column after which the rows are inserted/deleted.
  PRUint32 mNumRowsOrCols;   // the number of inserted/deleted rows/columns
};

#endif

