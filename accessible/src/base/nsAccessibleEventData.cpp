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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Aaron Leventhal (aaronl@netscape.com)
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

#include "nsAccessibleEventData.h"
#include "nsAccessibilityAtoms.h"
#include "nsCoreUtils.h"
#include "nsIAccessibilityService.h"
#include "nsIAccessNode.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIEventStateManager.h"
#include "nsIPersistentProperties2.h"
#include "nsIServiceManager.h"
#ifdef MOZ_XUL
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsXULTreeAccessible.h"
#endif
#include "nsIAccessibleText.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"

PRBool nsAccEvent::gLastEventFromUserInput = PR_FALSE;
nsIDOMNode* nsAccEvent::gLastEventNodeWeak = 0;

////////////////////////////////////////////////////////////////////////////////
// nsAccEvent. nsISupports

NS_IMPL_CYCLE_COLLECTION_2(nsAccEvent, mAccessible, mDocAccessible)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsAccEvent)
  NS_INTERFACE_MAP_ENTRY(nsIAccessibleEvent)
  NS_INTERFACE_MAP_ENTRY(nsAccEvent)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsAccEvent)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsAccEvent)

////////////////////////////////////////////////////////////////////////////////
// nsAccEvent. Constructors

nsAccEvent::nsAccEvent(PRUint32 aEventType, nsIAccessible *aAccessible,
                       PRBool aIsAsynch, EEventRule aEventRule):
  mEventType(aEventType), mAccessible(aAccessible), mEventRule(aEventRule)
{
  CaptureIsFromUserInput(aIsAsynch);
}

nsAccEvent::nsAccEvent(PRUint32 aEventType, nsIDOMNode *aDOMNode,
                       PRBool aIsAsynch, EEventRule aEventRule):
  mEventType(aEventType), mDOMNode(aDOMNode), mEventRule(aEventRule)
{
  CaptureIsFromUserInput(aIsAsynch);
}

void nsAccEvent::GetLastEventAttributes(nsIDOMNode *aNode,
                                        nsIPersistentProperties *aAttributes)
{
  if (aNode == gLastEventNodeWeak) {
    // Only provide event-from-input for last event's node
    nsAutoString oldValueUnused;
    aAttributes->SetStringProperty(NS_LITERAL_CSTRING("event-from-input"),
                                   gLastEventFromUserInput ? NS_LITERAL_STRING("true") :
                                                             NS_LITERAL_STRING("false"),
                                   oldValueUnused);
  }
}

void nsAccEvent::CaptureIsFromUserInput(PRBool aIsAsynch)
{
  nsCOMPtr<nsIDOMNode> eventNode;
  GetDOMNode(getter_AddRefs(eventNode));
  if (!eventNode) {
    NS_NOTREACHED("There should always be a DOM node for an event");
    return;
  }

  if (!aIsAsynch) {
    PrepareForEvent(eventNode);
    mIsFromUserInput = gLastEventFromUserInput;
  }
  // For asynch, cannot calculate if from user input.
  // Don't reset global last input state here, do it
  // at the end of FlushPendingEvents()

  mIsFromUserInput = gLastEventFromUserInput;
}

NS_IMETHODIMP
nsAccEvent::GetIsFromUserInput(PRBool *aIsFromUserInput)
{
  *aIsFromUserInput = mIsFromUserInput;
  return NS_OK;
}

NS_IMETHODIMP
nsAccEvent::SetIsFromUserInput(PRBool aIsFromUserInput)
{
  mIsFromUserInput = aIsFromUserInput;
  return NS_OK;
}

void nsAccEvent::PrepareForEvent(nsIAccessibleEvent *aEvent,
                                 PRBool aForceIsFromUserInput)
{
  gLastEventFromUserInput = aForceIsFromUserInput;
  nsCOMPtr<nsIDOMNode> eventNode;
  aEvent->GetDOMNode(getter_AddRefs(eventNode));
  if (!gLastEventFromUserInput) {  // Caller is not forcing user input flag
    aEvent->GetIsFromUserInput(&gLastEventFromUserInput);
    if (!gLastEventFromUserInput) {
      // Event does not have user input flag set to true
      // One more try -- check to see if we are currently responding to user input
      PrepareForEvent(eventNode);  
    }
  }
  gLastEventNodeWeak = eventNode;
  // Make sure this event remembers whether it is from user input
  aEvent->SetIsFromUserInput(gLastEventFromUserInput);
}

void nsAccEvent::PrepareForEvent(nsIDOMNode *aEventNode,
                                 PRBool aForceIsFromUserInput)
{
  if (!aEventNode) {
    return;
  }

  gLastEventNodeWeak = aEventNode;
  if (aForceIsFromUserInput) {
    gLastEventFromUserInput = PR_TRUE;
    return;
  }

  nsCOMPtr<nsIDOMDocument> domDoc;
  aEventNode->GetOwnerDocument(getter_AddRefs(domDoc));
  if (!domDoc) {  // IF the node is a document itself
    domDoc = do_QueryInterface(aEventNode);
  }
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (!doc) {
    NS_NOTREACHED("There should always be a document for an event");
    return;
  }

  nsCOMPtr<nsIPresShell> presShell = doc->GetPrimaryShell();
  if (!presShell) {
    NS_NOTREACHED("Threre should always be an pres shell for an event");
    return;
  }

  nsIEventStateManager *esm = presShell->GetPresContext()->EventStateManager();
  if (!esm) {
    NS_NOTREACHED("There should always be an ESM for an event");
    return;
  }

  gLastEventFromUserInput = esm->IsHandlingUserInputExternal();
}

NS_IMETHODIMP
nsAccEvent::GetEventType(PRUint32 *aEventType)
{
  *aEventType = mEventType;
  return NS_OK;
}

NS_IMETHODIMP
nsAccEvent::GetAccessible(nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  if (!mAccessible)
    mAccessible = GetAccessibleByNode();

  NS_IF_ADDREF(*aAccessible = mAccessible);
  return NS_OK;
}

NS_IMETHODIMP
nsAccEvent::GetDOMNode(nsIDOMNode **aDOMNode)
{
  NS_ENSURE_ARG_POINTER(aDOMNode);
  *aDOMNode = nsnull;

  if (!mDOMNode) {
    nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(mAccessible));
    NS_ENSURE_TRUE(accessNode, NS_ERROR_FAILURE);
    accessNode->GetDOMNode(getter_AddRefs(mDOMNode));
  }

  NS_IF_ADDREF(*aDOMNode = mDOMNode);
  return NS_OK;
}

NS_IMETHODIMP
nsAccEvent::GetAccessibleDocument(nsIAccessibleDocument **aDocAccessible)
{
  NS_ENSURE_ARG_POINTER(aDocAccessible);
  *aDocAccessible = nsnull;

  if (!mDocAccessible) {
    if (!mAccessible) {
      nsCOMPtr<nsIAccessible> accessible;
      GetAccessible(getter_AddRefs(accessible));
    }

    nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(mAccessible));
    NS_ENSURE_TRUE(accessNode, NS_ERROR_FAILURE);
    accessNode->GetAccessibleDocument(getter_AddRefs(mDocAccessible));
  }

  NS_IF_ADDREF(*aDocAccessible = mDocAccessible);
  return NS_OK;
}

already_AddRefed<nsIAccessible>
nsAccEvent::GetAccessibleByNode()
{
  if (!mDOMNode)
    return nsnull;

  nsCOMPtr<nsIAccessibilityService> accService = 
    do_GetService("@mozilla.org/accessibilityService;1");
  if (!accService)
    return nsnull;

  nsIAccessible *accessible = nsnull;
  accService->GetAccessibleFor(mDOMNode, &accessible);
#ifdef MOZ_XUL
  // hack for xul tree table. We need a better way for firing delayed event
  // against xul tree table. see bug 386821.
  // There will be problem if some day we want to fire delayed event against
  // the xul tree itself or an unselected treeitem.
  nsAutoString localName;
  mDOMNode->GetLocalName(localName);
  if (localName.EqualsLiteral("tree")) {
    nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelect =
      do_QueryInterface(mDOMNode);
    if (multiSelect) {
      PRInt32 treeIndex = -1;
      multiSelect->GetCurrentIndex(&treeIndex);
      if (treeIndex >= 0) {
        nsCOMPtr<nsIAccessibleTreeCache> treeCache(do_QueryInterface(accessible));
        NS_IF_RELEASE(accessible);
        nsCOMPtr<nsIAccessible> treeItemAccessible;
        if (!treeCache ||
            NS_FAILED(treeCache->GetCachedTreeitemAccessible(
                      treeIndex,
                      nsnull,
                      getter_AddRefs(treeItemAccessible))) ||
                      !treeItemAccessible) {
          return nsnull;
        }
        NS_IF_ADDREF(accessible = treeItemAccessible);
      }
    }
  }
#endif

  return accessible;
}

/* static */
void
nsAccEvent::ApplyEventRules(nsCOMArray<nsIAccessibleEvent> &aEventsToFire)
{
  PRUint32 numQueuedEvents = aEventsToFire.Count();
  for (PRInt32 tail = numQueuedEvents - 1; tail >= 0; tail --) {
    nsRefPtr<nsAccEvent> tailEvent = GetAccEventPtr(aEventsToFire[tail]);
    switch(tailEvent->mEventRule) {
      case nsAccEvent::eCoalesceFromSameSubtree:
      {
        for (PRInt32 index = 0; index < tail; index ++) {
          nsRefPtr<nsAccEvent> thisEvent = GetAccEventPtr(aEventsToFire[index]);
          if (thisEvent->mEventType != tailEvent->mEventType)
            continue; // Different type

          if (thisEvent->mEventRule == nsAccEvent::eAllowDupes ||
              thisEvent->mEventRule == nsAccEvent::eDoNotEmit)
            continue; //  Do not need to check

          if (thisEvent->mDOMNode == tailEvent->mDOMNode) {
            if (thisEvent->mEventType == nsIAccessibleEvent::EVENT_REORDER) {
              CoalesceReorderEventsFromSameSource(thisEvent, tailEvent);
              continue;
            }

            // Dupe
            thisEvent->mEventRule = nsAccEvent::eDoNotEmit;
            continue;
          }
          if (nsCoreUtils::IsAncestorOf(tailEvent->mDOMNode,
                                        thisEvent->mDOMNode)) {
            // thisDOMNode is a descendant of tailDOMNode
            if (thisEvent->mEventType == nsIAccessibleEvent::EVENT_REORDER) {
              CoalesceReorderEventsFromSameTree(tailEvent, thisEvent);
              continue;
            }

            // Do not emit thisEvent, also apply this result to sibling
            // nodes of thisDOMNode.
            thisEvent->mEventRule = nsAccEvent::eDoNotEmit;
            ApplyToSiblings(aEventsToFire, 0, index, thisEvent->mEventType,
                            thisEvent->mDOMNode, nsAccEvent::eDoNotEmit);
            continue;
          }
          if (nsCoreUtils::IsAncestorOf(thisEvent->mDOMNode,
                                        tailEvent->mDOMNode)) {
            // tailDOMNode is a descendant of thisDOMNode
            if (thisEvent->mEventType == nsIAccessibleEvent::EVENT_REORDER) {
              CoalesceReorderEventsFromSameTree(thisEvent, tailEvent);
              continue;
            }

            // Do not emit tailEvent, also apply this result to sibling
            // nodes of tailDOMNode.
            tailEvent->mEventRule = nsAccEvent::eDoNotEmit;
            ApplyToSiblings(aEventsToFire, 0, tail, tailEvent->mEventType,
                            tailEvent->mDOMNode, nsAccEvent::eDoNotEmit);
            break;
          }
        } // for (index)

        if (tailEvent->mEventRule != nsAccEvent::eDoNotEmit) {
          // Not in another event node's subtree, and no other event is in
          // this event node's subtree.
          // This event should be emitted
          // Apply this result to sibling nodes of tailDOMNode
          ApplyToSiblings(aEventsToFire, 0, tail, tailEvent->mEventType,
                          tailEvent->mDOMNode, nsAccEvent::eAllowDupes);
        }
      } break; // case eCoalesceFromSameSubtree

      case nsAccEvent::eRemoveDupes:
      {
        // Check for repeat events.
        for (PRInt32 index = 0; index < tail; index ++) {
          nsRefPtr<nsAccEvent> accEvent = GetAccEventPtr(aEventsToFire[index]);
          if (accEvent->mEventType == tailEvent->mEventType &&
              accEvent->mEventRule == tailEvent->mEventRule &&
              accEvent->mDOMNode == tailEvent->mDOMNode) {
            accEvent->mEventRule = nsAccEvent::eDoNotEmit;
          }
        }
      } break; // case eRemoveDupes
    } // switch
  } // for (tail)
}

/* static */
void
nsAccEvent::ApplyToSiblings(nsCOMArray<nsIAccessibleEvent> &aEventsToFire,
                            PRUint32 aStart, PRUint32 aEnd,
                             PRUint32 aEventType, nsIDOMNode* aDOMNode,
                             EEventRule aEventRule)
{
  for (PRUint32 index = aStart; index < aEnd; index ++) {
    nsRefPtr<nsAccEvent> accEvent = GetAccEventPtr(aEventsToFire[index]);
    if (accEvent->mEventType == aEventType &&
        accEvent->mEventRule != nsAccEvent::eDoNotEmit &&
        nsCoreUtils::AreSiblings(accEvent->mDOMNode, aDOMNode)) {
      accEvent->mEventRule = aEventRule;
    }
  }
}

/* static */
void
nsAccEvent::CoalesceReorderEventsFromSameSource(nsAccEvent *aAccEvent1,
                                                nsAccEvent *aAccEvent2)
{
  // Do not emit event2 if event1 is unconditional.
  nsCOMPtr<nsAccReorderEvent> reorderEvent1 = do_QueryInterface(aAccEvent1);
  if (reorderEvent1->IsUnconditionalEvent()) {
    aAccEvent2->mEventRule = nsAccEvent::eDoNotEmit;
    return;
  }

  // Do not emit event1 if event2 is unconditional.
  nsCOMPtr<nsAccReorderEvent> reorderEvent2 = do_QueryInterface(aAccEvent2);
  if (reorderEvent2->IsUnconditionalEvent()) {
    aAccEvent1->mEventRule = nsAccEvent::eDoNotEmit;
    return;
  }

  // Do not emit event2 if event1 is valid, otherwise do not emit event1.
  if (reorderEvent1->HasAccessibleInReasonSubtree())
    aAccEvent2->mEventRule = nsAccEvent::eDoNotEmit;
  else
    aAccEvent1->mEventRule = nsAccEvent::eDoNotEmit;
}

void
nsAccEvent::CoalesceReorderEventsFromSameTree(nsAccEvent *aAccEvent,
                                              nsAccEvent *aDescendantAccEvent)
{
  // Do not emit descendant event if this event is unconditional.
  nsCOMPtr<nsAccReorderEvent> reorderEvent = do_QueryInterface(aAccEvent);
  if (reorderEvent->IsUnconditionalEvent()) {
    aDescendantAccEvent->mEventRule = nsAccEvent::eDoNotEmit;
    return;
  }

  // Do not emit descendant event if this event is valid otherwise do not emit
  // this event.
  if (reorderEvent->HasAccessibleInReasonSubtree())
    aDescendantAccEvent->mEventRule = nsAccEvent::eDoNotEmit;
  else
    aAccEvent->mEventRule = nsAccEvent::eDoNotEmit;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccReorderEvent

NS_IMPL_ISUPPORTS_INHERITED1(nsAccReorderEvent, nsAccEvent,
                             nsAccReorderEvent)

nsAccReorderEvent::nsAccReorderEvent(nsIAccessible *aAccTarget,
                                     PRBool aIsAsynch,
                                     PRBool aIsUnconditional,
                                     nsIDOMNode *aReasonNode) :
  nsAccEvent(::nsIAccessibleEvent::EVENT_REORDER, aAccTarget,
             aIsAsynch, nsAccEvent::eCoalesceFromSameSubtree),
  mUnconditionalEvent(aIsUnconditional), mReasonNode(aReasonNode)
{
}

PRBool
nsAccReorderEvent::IsUnconditionalEvent()
{
  return mUnconditionalEvent;
}

PRBool
nsAccReorderEvent::HasAccessibleInReasonSubtree()
{
  if (!mReasonNode)
    return PR_FALSE;

  nsCOMPtr<nsIAccessible> accessible;
  nsAccessNode::GetAccService()->GetAccessibleFor(mReasonNode,
                                                  getter_AddRefs(accessible));

  return accessible || nsAccUtils::HasAccessibleChildren(mReasonNode);
}

////////////////////////////////////////////////////////////////////////////////
// nsAccStateChangeEvent

NS_IMPL_ISUPPORTS_INHERITED1(nsAccStateChangeEvent, nsAccEvent,
                             nsIAccessibleStateChangeEvent)

nsAccStateChangeEvent::
  nsAccStateChangeEvent(nsIAccessible *aAccessible,
                        PRUint32 aState, PRBool aIsExtraState,
                        PRBool aIsEnabled):
  nsAccEvent(::nsIAccessibleEvent::EVENT_STATE_CHANGE, aAccessible),
  mState(aState), mIsExtraState(aIsExtraState), mIsEnabled(aIsEnabled)
{
}

nsAccStateChangeEvent::
  nsAccStateChangeEvent(nsIDOMNode *aNode,
                        PRUint32 aState, PRBool aIsExtraState,
                        PRBool aIsEnabled):
  nsAccEvent(::nsIAccessibleEvent::EVENT_STATE_CHANGE, aNode),
  mState(aState), mIsExtraState(aIsExtraState), mIsEnabled(aIsEnabled)
{
}

nsAccStateChangeEvent::
  nsAccStateChangeEvent(nsIDOMNode *aNode,
                        PRUint32 aState, PRBool aIsExtraState):
  nsAccEvent(::nsIAccessibleEvent::EVENT_STATE_CHANGE, aNode),
  mState(aState), mIsExtraState(aIsExtraState)
{
  // Use GetAccessibleByNode() because we do not want to store an accessible
  // since it leads to problems with delayed events in the case when
  // an accessible gets reorder event before delayed event is processed.
  nsCOMPtr<nsIAccessible> accessible(GetAccessibleByNode());
  if (accessible) {
    PRUint32 state = 0, extraState = 0;
    accessible->GetState(&state, mIsExtraState ? &extraState : nsnull);
    mIsEnabled = ((mIsExtraState ? extraState : state) & mState) != 0;
  } else {
    mIsEnabled = PR_FALSE;
  }
}

NS_IMETHODIMP
nsAccStateChangeEvent::GetState(PRUint32 *aState)
{
  *aState = mState;
  return NS_OK;
}

NS_IMETHODIMP
nsAccStateChangeEvent::IsExtraState(PRBool *aIsExtraState)
{
  *aIsExtraState = mIsExtraState;
  return NS_OK;
}

NS_IMETHODIMP
nsAccStateChangeEvent::IsEnabled(PRBool *aIsEnabled)
{
  *aIsEnabled = mIsEnabled;
  return NS_OK;
}


// nsAccTextChangeEvent
NS_IMPL_ISUPPORTS_INHERITED1(nsAccTextChangeEvent, nsAccEvent,
                             nsIAccessibleTextChangeEvent)

nsAccTextChangeEvent::
  nsAccTextChangeEvent(nsIAccessible *aAccessible,
                       PRInt32 aStart, PRUint32 aLength, PRBool aIsInserted, PRBool aIsAsynch):
  nsAccEvent(aIsInserted ? nsIAccessibleEvent::EVENT_TEXT_INSERTED : nsIAccessibleEvent::EVENT_TEXT_REMOVED,
             aAccessible, aIsAsynch),
  mStart(aStart), mLength(aLength), mIsInserted(aIsInserted)
{
  nsCOMPtr<nsIAccessibleText> textAccessible = do_QueryInterface(aAccessible);
  NS_ASSERTION(textAccessible, "Should not be firing test change event for non-text accessible!!!");
  if (textAccessible) {
    textAccessible->GetText(aStart, aStart + aLength, mModifiedText);
  }
}

NS_IMETHODIMP
nsAccTextChangeEvent::GetStart(PRInt32 *aStart)
{
  *aStart = mStart;
  return NS_OK;
}

NS_IMETHODIMP
nsAccTextChangeEvent::GetLength(PRUint32 *aLength)
{
  *aLength = mLength;
  return NS_OK;
}

NS_IMETHODIMP
nsAccTextChangeEvent::IsInserted(PRBool *aIsInserted)
{
  *aIsInserted = mIsInserted;
  return NS_OK;
}

NS_IMETHODIMP
nsAccTextChangeEvent::GetModifiedText(nsAString& aModifiedText)
{
  aModifiedText = mModifiedText;
  return NS_OK;
}

// nsAccCaretMoveEvent
NS_IMPL_ISUPPORTS_INHERITED1(nsAccCaretMoveEvent, nsAccEvent,
                             nsIAccessibleCaretMoveEvent)

nsAccCaretMoveEvent::
  nsAccCaretMoveEvent(nsIAccessible *aAccessible, PRInt32 aCaretOffset) :
  nsAccEvent(::nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED, aAccessible, PR_TRUE), // Currently always asynch
  mCaretOffset(aCaretOffset)
{
}

nsAccCaretMoveEvent::
  nsAccCaretMoveEvent(nsIDOMNode *aNode) :
  nsAccEvent(::nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED, aNode, PR_TRUE), // Currently always asynch
  mCaretOffset(-1)
{
}

NS_IMETHODIMP
nsAccCaretMoveEvent::GetCaretOffset(PRInt32* aCaretOffset)
{
  NS_ENSURE_ARG_POINTER(aCaretOffset);

  *aCaretOffset = mCaretOffset;
  return NS_OK;
}

// nsAccTableChangeEvent 
NS_IMPL_ISUPPORTS_INHERITED1(nsAccTableChangeEvent, nsAccEvent,
                             nsIAccessibleTableChangeEvent)

nsAccTableChangeEvent::
  nsAccTableChangeEvent(nsIAccessible *aAccessible, PRUint32 aEventType,
                        PRInt32 aRowOrColIndex, PRInt32 aNumRowsOrCols, PRBool aIsAsynch):
  nsAccEvent(aEventType, aAccessible, aIsAsynch), 
  mRowOrColIndex(aRowOrColIndex), mNumRowsOrCols(aNumRowsOrCols)
{
}

NS_IMETHODIMP
nsAccTableChangeEvent::GetRowOrColIndex(PRInt32* aRowOrColIndex)
{
  NS_ENSURE_ARG_POINTER(aRowOrColIndex);

  *aRowOrColIndex = mRowOrColIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsAccTableChangeEvent::GetNumRowsOrCols(PRInt32* aNumRowsOrCols)
{
  NS_ENSURE_ARG_POINTER(aNumRowsOrCols);

  *aNumRowsOrCols = mNumRowsOrCols;
  return NS_OK;
}

