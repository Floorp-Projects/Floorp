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

#include "nsEventDispatcher.h"
#include "nsIDocument.h"
#include "nsIAtom.h"
#include "nsDOMEvent.h"
#include "nsINode.h"
#include "nsIChromeEventHandler.h"
#include "nsPresContext.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventTarget.h"
#include "nsIEventListenerManager.h"
#include "nsPIDOMWindow.h"
#include "nsIScriptSecurityManager.h"
#include "nsContentUtils.h"
#include "nsDOMError.h"
#include "nsMutationEvent.h"

#define NS_TARGET_CHAIN_IS_NODE                (1 << 0)
#define NS_TARGET_CHAIN_IS_WINDOW              (1 << 1)
#define NS_TARGET_CHAIN_IS_CHROMEHANDLER       (1 << 2)

#define NS_TARGET_CHAIN_FORCE_CONTENT_DISPATCH (1 << 3)

#define NS_TARGET_CHAIN_TYPE_MASK \
  (NS_TARGET_CHAIN_IS_NODE | NS_TARGET_CHAIN_IS_WINDOW | \
   NS_TARGET_CHAIN_IS_CHROMEHANDLER)

// nsEventTargetChainItem is a stack-allocated object represeting a
// single item in the event target chain.
class nsEventTargetChainItem
{
public:
  nsEventTargetChainItem(nsISupports* aTarget,
                         PRBool aTargetIsChromeHandler,
                         nsEventTargetChainItem* aChild = nsnull);

  ~nsEventTargetChainItem();

  PRBool IsValid()
  {
    return !!(mFlags & NS_TARGET_CHAIN_TYPE_MASK);
  }

  nsISupports* GetNewTarget()
  {
    return mNewTarget;
  }

  void SetNewTarget(nsISupports* aNewTarget)
  {
    mNewTarget = aNewTarget;
  }

  void SetForceContentDispatch(PRBool aForce) {
    if (aForce) {
      mFlags |= NS_TARGET_CHAIN_FORCE_CONTENT_DISPATCH;
    } else {
      mFlags &= ~NS_TARGET_CHAIN_FORCE_CONTENT_DISPATCH;
    }
  }

  PRBool ForceContentDispatch() {
    return !!(mFlags & NS_TARGET_CHAIN_FORCE_CONTENT_DISPATCH);
  }

  nsISupports* CurrentTarget();

  already_AddRefed<nsIEventListenerManager> GetListenerManager(
    PRBool aCreateIfNotFound);

  /**
   * Creates event target chain and calls HandleEventTargetChain.
   */
  nsresult CreateChainAndHandleEvent(nsEventChainPreVisitor& aVisitor,
                                     nsDispatchingCallback* aCallback);

  /**
   * Dispatches event through the event target chain.
   * Handles capture, target and bubble phases both in default
   * and system event group and calls also PostHandleEvent for each
   * item in the chain.
   */
  nsresult HandleEventTargetChain(nsEventChainPostVisitor& aVisitor,
                                  PRUint32 aFlags,
                                  nsDispatchingCallback* aCallback);

  /**
   * Resets aVisitor object and calls PreHandleEvent
   * (or PreHandleChromeEvent). Copies mItemFlags and mItemData to the
   * current nsEventTargetChainItem.
   */
  nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);

  /**
   * If the current item in the event target chain has an event listener
   * manager, this method sets the .currentTarget to the CurrentTarget()
   * and calls nsIEventListenerManager::HandleEvent().
   */
  nsresult HandleEvent(nsEventChainPostVisitor& aVisitor, PRUint32 aFlags);

  /**
   * Copies mItemFlags and mItemData to aVisitor and calls PostHandleEvent
   * (or PostHandleChromeEvent).
   */
  nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);


  union {
    nsINode*                  mNode;
    nsPIDOMWindow*            mWindow;
    nsIChromeEventHandler*    mChromeHandler;
  };
  nsEventTargetChainItem*     mChild;
  nsEventTargetChainItem*     mParent;
  PRUint16                    mFlags;
  PRUint16                    mItemFlags;
  nsCOMPtr<nsISupports>       mItemData;
  // Event retargeting must happen whenever mNewTarget is non-null.
  nsCOMPtr<nsISupports>       mNewTarget;
};

nsEventTargetChainItem::nsEventTargetChainItem(nsISupports* aTarget,
                                               PRBool aTargetIsChromeHandler,
                                               nsEventTargetChainItem* aChild)
: mNode(nsnull), mChild(aChild), mParent(nsnull), mFlags(0), mItemFlags(0)
{
  if (mChild) {
    mChild->mParent = this;
  }

  // If the target is explicitly marked to be a chrome handler.
  if (aTargetIsChromeHandler) {
    nsCOMPtr<nsIChromeEventHandler> ceh = do_QueryInterface(aTarget);
    if (ceh) {
      ceh.swap(mChromeHandler);
      mFlags |= NS_TARGET_CHAIN_IS_CHROMEHANDLER;
    }
  } else {
    nsCOMPtr<nsINode> node = do_QueryInterface(aTarget);
    if (node) {
      node.swap(mNode);
      mFlags |= NS_TARGET_CHAIN_IS_NODE;
    }  else {
      nsCOMPtr<nsPIDOMWindow> window =
        do_QueryInterface(aTarget);
      if (window) {
        window.swap(mWindow);
        mFlags |= NS_TARGET_CHAIN_IS_WINDOW;
      } else {
        nsCOMPtr<nsIChromeEventHandler> ceh = do_QueryInterface(aTarget);
        if (ceh) {
          ceh.swap(mChromeHandler);
          mFlags |= NS_TARGET_CHAIN_IS_CHROMEHANDLER;
        }
      }
    }
  }
  NS_POSTCONDITION((mFlags & NS_TARGET_CHAIN_TYPE_MASK),
                   "No event target in event target chain!");
}

nsEventTargetChainItem::~nsEventTargetChainItem()
{
  if (mChild) {
    mChild->mParent = nsnull;
    mChild = nsnull;
  }
  mParent = nsnull;

  switch (mFlags & NS_TARGET_CHAIN_TYPE_MASK) {
    case NS_TARGET_CHAIN_IS_NODE:
      NS_RELEASE(mNode);
      break;
    case NS_TARGET_CHAIN_IS_WINDOW:
      NS_RELEASE(mWindow);
      break;
    case NS_TARGET_CHAIN_IS_CHROMEHANDLER:
      NS_RELEASE(mChromeHandler);
      break;
    default:
      NS_WARNING("Unknown type in event target chain!!!");
      break;
  }
}

already_AddRefed<nsIEventListenerManager>
nsEventTargetChainItem::GetListenerManager(PRBool aCreateIfNotFound)
{
  nsIEventListenerManager* manager = nsnull;
  switch (mFlags & NS_TARGET_CHAIN_TYPE_MASK) {
    case NS_TARGET_CHAIN_IS_NODE:
    {
      mNode->GetListenerManager(aCreateIfNotFound, &manager);
      break;
    }
    case NS_TARGET_CHAIN_IS_WINDOW:
    {
      nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(mWindow));
      if (receiver) {
        receiver->GetListenerManager(aCreateIfNotFound, &manager);
      }
      break;
    }
    case NS_TARGET_CHAIN_IS_CHROMEHANDLER:
    {
      nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(mChromeHandler));
      if (receiver) {
        receiver->GetListenerManager(aCreateIfNotFound, &manager);
      }
      break;
    }
    default:
    {
      NS_WARNING("Unknown type in event target chain!!!");
      break;
    }
  }

  return manager;
}

nsISupports*
nsEventTargetChainItem::CurrentTarget()
{
  nsCOMPtr<nsIDOMEventTarget> eventTarget;
  switch (mFlags & NS_TARGET_CHAIN_TYPE_MASK) {
    case NS_TARGET_CHAIN_IS_NODE:
    {
      return mNode;
    }
    case NS_TARGET_CHAIN_IS_WINDOW:
    {
      return mWindow;
      break;
    }
    case NS_TARGET_CHAIN_IS_CHROMEHANDLER:
    {
      return mChromeHandler;
      break;
    }
    default:
    {
      NS_WARNING("Unknown type in event target chain!!!");
      break;
    }
  }

  return nsnull;
}

nsresult
nsEventTargetChainItem::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.Reset();
  nsresult rv = NS_OK;
  switch (mFlags & NS_TARGET_CHAIN_TYPE_MASK) {
    case NS_TARGET_CHAIN_IS_NODE:
    {
      rv = mNode->PreHandleEvent(aVisitor);
      break;
    }
    case NS_TARGET_CHAIN_IS_WINDOW:
    {
      rv = mWindow->PreHandleEvent(aVisitor);
      break;
    }
    case NS_TARGET_CHAIN_IS_CHROMEHANDLER:
    {
      rv = mChromeHandler->PreHandleChromeEvent(aVisitor);
      break;
    }
    default:
    {
      NS_WARNING("Unknown type in event target chain!!!");
      break;
    }
  }

  SetForceContentDispatch(aVisitor.mForceContentDispatch);
  mItemFlags = aVisitor.mItemFlags;
  mItemData = aVisitor.mItemData;

  return rv;
}

nsresult
nsEventTargetChainItem::HandleEvent(nsEventChainPostVisitor& aVisitor,
                                    PRUint32 aFlags)
{
  nsCOMPtr<nsIEventListenerManager> lm =
    nsEventTargetChainItem::GetListenerManager(PR_FALSE);

  if (lm) {
    aVisitor.mEvent->currentTarget = CurrentTarget();
    lm->HandleEvent(aVisitor.mPresContext, aVisitor.mEvent, &aVisitor.mDOMEvent,
                    aVisitor.mEvent->currentTarget, aFlags,
                    &aVisitor.mEventStatus);
    aVisitor.mEvent->currentTarget = nsnull;
  }
  return NS_OK;
}

nsresult
nsEventTargetChainItem::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  aVisitor.mItemFlags = mItemFlags;
  aVisitor.mItemData = mItemData;
  switch (mFlags & NS_TARGET_CHAIN_TYPE_MASK) {
    case NS_TARGET_CHAIN_IS_NODE:
    {
      return mNode->PostHandleEvent(aVisitor);
    }
    case NS_TARGET_CHAIN_IS_WINDOW:
    {
      return mWindow->PostHandleEvent(aVisitor);
    }
    case NS_TARGET_CHAIN_IS_CHROMEHANDLER:
    {
      return mChromeHandler->PostHandleChromeEvent(aVisitor);
    }
    default:
    {
      NS_WARNING("Unknown type in event target chain!!!");
      break;
    }
  }

  return NS_OK;
}

nsresult
nsEventTargetChainItem::CreateChainAndHandleEvent(nsEventChainPreVisitor& aVisitor,
                                                  nsDispatchingCallback* aCallback)
{
  if (aVisitor.mParentTarget) {
    // There is something which should be added to the event target chain.
    nsEventTargetChainItem parentEtci(aVisitor.mParentTarget,
                                      aVisitor.mParentIsChromeHandler,
                                      this);
    NS_ENSURE_TRUE(parentEtci.IsValid(), NS_ERROR_FAILURE);

    // Item needs event retargetting.
    if (aVisitor.mEventTargetAtParent) {
      // Need to set the target of the event
      // so that also the next retargeting works.
      aVisitor.mEvent->target = aVisitor.mEventTargetAtParent;
      parentEtci.SetNewTarget(aVisitor.mEventTargetAtParent);
    }

    parentEtci.PreHandleEvent(aVisitor);

    if (aVisitor.mCanHandle) {
      // The parent event target chain item can handle the event. Continue
      // chain creation.
      return parentEtci.CreateChainAndHandleEvent(aVisitor, aCallback);
    }
  }

  // Event target chain is created. Handle the chain.
  nsEventChainPostVisitor postVisitor(aVisitor);
  nsresult rv =  HandleEventTargetChain(postVisitor,
                                        NS_EVENT_FLAG_BUBBLE |
                                        NS_EVENT_FLAG_CAPTURE,
                                        aCallback);

  aVisitor.mEventStatus = postVisitor.mEventStatus;
  // If the DOM event was created during event flow.
  if (!aVisitor.mDOMEvent && postVisitor.mDOMEvent) {
    aVisitor.mDOMEvent = postVisitor.mDOMEvent;
  }
  return rv;
}

nsresult
nsEventTargetChainItem::HandleEventTargetChain(nsEventChainPostVisitor& aVisitor, PRUint32 aFlags,
                                               nsDispatchingCallback* aCallback)
{
  // Save the target so that it can be restored later.
  nsCOMPtr<nsISupports> firstTarget = aVisitor.mEvent->target;

  // Capture
  nsEventTargetChainItem* item = this;
  aVisitor.mEvent->flags |= NS_EVENT_FLAG_CAPTURE;
  aVisitor.mEvent->flags &= ~NS_EVENT_FLAG_BUBBLE;
  while (item->mChild) {
    if ((!(aVisitor.mEvent->flags & NS_EVENT_FLAG_NO_CONTENT_DISPATCH) ||
         item->ForceContentDispatch()) &&
        !(aVisitor.mEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH)) {
      item->HandleEvent(aVisitor, aFlags & NS_EVENT_CAPTURE_MASK);
    }

    if (item->GetNewTarget()) {
      // item is at anonymous boundary. Need to retarget for the child items.
      nsEventTargetChainItem* nextTarget = item->mChild;
      while (nextTarget) {
        nsISupports* newTarget = nextTarget->GetNewTarget();
        if (newTarget) {
          aVisitor.mEvent->target = newTarget;
          break;
        }
        nextTarget = nextTarget->mChild;
      }
    }

    item = item->mChild;
  }

  // Target
  aVisitor.mEvent->flags |= NS_EVENT_FLAG_BUBBLE;
  if (!(aVisitor.mEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH) &&
      (!(aVisitor.mEvent->flags & NS_EVENT_FLAG_NO_CONTENT_DISPATCH) ||
       item->ForceContentDispatch())) {
    // FIXME Should use aFlags & NS_EVENT_BUBBLE_MASK because capture phase
    //       event listeners should not be fired. But it breaks at least
    //       <xul:dialog>'s buttons. Bug 235441.
    item->HandleEvent(aVisitor, aFlags);
  }
  if (aFlags & NS_EVENT_FLAG_SYSTEM_EVENT) {
    item->PostHandleEvent(aVisitor);
  }

  // Bubble
  aVisitor.mEvent->flags &= ~NS_EVENT_FLAG_CAPTURE;
  item = item->mParent;
  while (item) {
    nsISupports* newTarget = item->GetNewTarget();
    if (newTarget) {
      // Item is at anonymous boundary. Need to retarget for the current item
      // and for parent items.
      aVisitor.mEvent->target = newTarget;
    }

    if (!(aVisitor.mEvent->flags & NS_EVENT_FLAG_CANT_BUBBLE) || newTarget) {
      if ((!(aVisitor.mEvent->flags & NS_EVENT_FLAG_NO_CONTENT_DISPATCH) ||
           item->ForceContentDispatch()) &&
          (!(aFlags & NS_EVENT_FLAG_SYSTEM_EVENT) ||
           aVisitor.mEventStatus != nsEventStatus_eConsumeNoDefault) &&
          !(aVisitor.mEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH)) {
        item->HandleEvent(aVisitor, aFlags & NS_EVENT_BUBBLE_MASK);
      }
      if (aFlags & NS_EVENT_FLAG_SYSTEM_EVENT) {
        item->PostHandleEvent(aVisitor);
      }
    }
    item = item->mParent;
  }
  aVisitor.mEvent->flags &= ~NS_EVENT_FLAG_BUBBLE;

  if (!(aFlags & NS_EVENT_FLAG_SYSTEM_EVENT)) {
    // Dispatch to the system event group.  Make sure to clear the
    // STOP_DISPATCH flag since this resets for each event group
    // per DOM3 Events.
    aVisitor.mEvent->flags &= ~NS_EVENT_FLAG_STOP_DISPATCH;

    // Setting back the original target of the event.
    aVisitor.mEvent->target = aVisitor.mEvent->originalTarget;

    // Special handling if PresShell (or some other caller)
    // used a callback object.
    if (aCallback) {
      aCallback->HandleEvent(aVisitor);
    }

    // Retarget for system event group (which does the default handling too).
    // Setting back the target which was used also for default event group.
    aVisitor.mEvent->target = firstTarget;
    HandleEventTargetChain(aVisitor, aFlags | NS_EVENT_FLAG_SYSTEM_EVENT,
                           aCallback);
  }

  return NS_OK;
}

/* static */ nsresult
nsEventDispatcher::Dispatch(nsISupports* aTarget,
                            nsPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent* aDOMEvent,
                            nsEventStatus* aEventStatus,
                            nsDispatchingCallback* aCallback,
                            PRBool aTargetIsChromeHandler)
{
  NS_ASSERTION(aEvent, "Trying to dispatch without nsEvent!");
  NS_ENSURE_TRUE(!NS_IS_EVENT_IN_DISPATCH(aEvent),
                 NS_ERROR_ILLEGAL_VALUE);
  // This is strange, but nsEvents are sometimes reused and they don't need
  // re-initialization.
  NS_ENSURE_TRUE(!(aDOMEvent &&
                   (aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH_IMMEDIATELY)),
                 NS_ERROR_ILLEGAL_VALUE);

#ifdef DEBUG
  if (aDOMEvent) {
    nsCOMPtr<nsIPrivateDOMEvent> privEvt(do_QueryInterface(aDOMEvent));
    if (privEvt) {
      nsEvent* innerEvent = nsnull;
      privEvt->GetInternalNSEvent(&innerEvent);
      NS_ASSERTION(innerEvent == aEvent,
                    "The inner event of aDOMEvent is not the same as aEvent!");
    }
  }
#endif

  nsresult rv = NS_OK;
  PRBool externalDOMEvent = !!(aDOMEvent);

  // If we have a PresContext, make sure it doesn't die before
  // event dispatching is finished.
  nsCOMPtr<nsPresContext> kungFuDeathGrip(aPresContext);

  // Create the event target chain item for the event target.
  nsEventTargetChainItem targetEtci(aTarget, aTargetIsChromeHandler);
  NS_ENSURE_TRUE(targetEtci.IsValid(), NS_ERROR_FAILURE);

  // Make sure that nsIDOMEvent::target and nsIDOMNSEvent::originalTarget
  // point to the last item in the chain.
  // XXX But if the target is already set, use that. This is a hack
  //     for the 'load', 'beforeunload' and 'unload' events,
  //     which are dispatched to |window| but have document as their target.
  if (!aEvent->target) {
    aEvent->target = aTarget;
  }
  aEvent->originalTarget = aEvent->target;

  NS_MARK_EVENT_DISPATCH_STARTED(aEvent);

  // Create visitor object and start event dispatching.
  // PreHandleEvent for the original target.
  nsEventStatus status = aEventStatus ? *aEventStatus : nsEventStatus_eIgnore;
  nsEventChainPreVisitor preVisitor(aPresContext, aEvent, aDOMEvent, status);
  targetEtci.PreHandleEvent(preVisitor);

  if (preVisitor.mCanHandle) {
    // At least the original target can handle the event.
    // Setting the retarget to the |target| simplifies retargeting code.
    targetEtci.SetNewTarget(aEvent->target);
    // Create rest of the event target chain and handle event.
    rv = targetEtci.CreateChainAndHandleEvent(preVisitor, aCallback);
  }

  NS_MARK_EVENT_DISPATCH_DONE(aEvent);

  if (!externalDOMEvent && preVisitor.mDOMEvent) {
    // An nsDOMEvent was created while dispatching the event.
    // Duplicate private data if someone holds a pointer to it.
    nsrefcnt rc = 0;
    NS_RELEASE2(preVisitor.mDOMEvent, rc);
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent =
      do_QueryInterface(preVisitor.mDOMEvent);
    if (privateEvent) {
      privateEvent->DuplicatePrivateData();
    }
  }

  if (aEventStatus) {
    *aEventStatus = preVisitor.mEventStatus;
  }
  return rv;
}

/* static */ nsresult
nsEventDispatcher::DispatchDOMEvent(nsISupports* aTarget,
                                    nsEvent* aEvent,
                                    nsIDOMEvent* aDOMEvent,
                                    nsPresContext* aPresContext,
                                    nsEventStatus* aEventStatus)
{
  if (aDOMEvent) {
    nsCOMPtr<nsIPrivateDOMEvent> privEvt(do_QueryInterface(aDOMEvent));
    if (privEvt) {
      nsEvent* innerEvent = nsnull;
      privEvt->GetInternalNSEvent(&innerEvent);
      NS_ENSURE_TRUE(innerEvent, NS_ERROR_ILLEGAL_VALUE);

      PRBool trusted;
      nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(privEvt));

      nsevent->GetIsTrusted(&trusted);

      if (!trusted) {
        //Check security state to determine if dispatcher is trusted
        nsIScriptSecurityManager *securityManager =
          nsContentUtils::GetSecurityManager();

        PRBool enabled;
        nsresult rv =
          securityManager->IsCapabilityEnabled("UniversalBrowserWrite",
                                               &enabled);
        privEvt->SetTrusted(NS_SUCCEEDED(rv) && enabled);
      }

      return nsEventDispatcher::Dispatch(aTarget, aPresContext, innerEvent,
                                         aDOMEvent, aEventStatus);
    }
  } else if (aEvent) {
    return nsEventDispatcher::Dispatch(aTarget, aPresContext, aEvent,
                                       aDOMEvent, aEventStatus);
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

/* static */ nsresult
nsEventDispatcher::CreateEvent(nsPresContext* aPresContext,
                               nsEvent* aEvent,
                               const nsAString& aEventType,
                               nsIDOMEvent** aDOMEvent)
{
  *aDOMEvent = nsnull;

  if (aEvent) {
    switch(aEvent->eventStructType) {
    case NS_MUTATION_EVENT:
      return NS_NewDOMMutationEvent(aDOMEvent, aPresContext,
                                    NS_STATIC_CAST(nsMutationEvent*,aEvent));
    case NS_GUI_EVENT:
    case NS_COMPOSITION_EVENT:
    case NS_RECONVERSION_EVENT:
    case NS_QUERYCARETRECT_EVENT:
    case NS_SCROLLPORT_EVENT:
      return NS_NewDOMUIEvent(aDOMEvent, aPresContext,
                              NS_STATIC_CAST(nsGUIEvent*,aEvent));
    case NS_KEY_EVENT:
      return NS_NewDOMKeyboardEvent(aDOMEvent, aPresContext,
                                    NS_STATIC_CAST(nsKeyEvent*,aEvent));
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_POPUP_EVENT:
      return NS_NewDOMMouseEvent(aDOMEvent, aPresContext,
                                 NS_STATIC_CAST(nsInputEvent*,aEvent));
    case NS_POPUPBLOCKED_EVENT:
      return NS_NewDOMPopupBlockedEvent(aDOMEvent, aPresContext,
                                        NS_STATIC_CAST(nsPopupBlockedEvent*,
                                                       aEvent));
    case NS_TEXT_EVENT:
      return NS_NewDOMTextEvent(aDOMEvent, aPresContext,
                                NS_STATIC_CAST(nsTextEvent*,aEvent));
    case NS_BEFORE_PAGE_UNLOAD_EVENT:
      return
        NS_NewDOMBeforeUnloadEvent(aDOMEvent, aPresContext,
                                   NS_STATIC_CAST(nsBeforePageUnloadEvent*,
                                                  aEvent));
    case NS_PAGETRANSITION_EVENT:
      return NS_NewDOMPageTransitionEvent(aDOMEvent, aPresContext,
                                          NS_STATIC_CAST(nsPageTransitionEvent*,
                                                         aEvent));
#ifdef MOZ_SVG
    case NS_SVG_EVENT:
      return NS_NewDOMSVGEvent(aDOMEvent, aPresContext,
                               aEvent);
    case NS_SVGZOOM_EVENT:
      return NS_NewDOMSVGZoomEvent(aDOMEvent, aPresContext,
                                   NS_STATIC_CAST(nsGUIEvent*,aEvent));
#endif // MOZ_SVG

    case NS_XUL_COMMAND_EVENT:
      return NS_NewDOMXULCommandEvent(aDOMEvent, aPresContext,
                                      NS_STATIC_CAST(nsXULCommandEvent*,
                                                     aEvent));
    }

    // For all other types of events, create a vanilla event object.
    return NS_NewDOMEvent(aDOMEvent, aPresContext, aEvent);
  }

  // And if we didn't get an event, check the type argument.

  if (aEventType.LowerCaseEqualsLiteral("mouseevent") ||
      aEventType.LowerCaseEqualsLiteral("mouseevents") ||
      aEventType.LowerCaseEqualsLiteral("mousescrollevents") ||
      aEventType.LowerCaseEqualsLiteral("popupevents"))
    return NS_NewDOMMouseEvent(aDOMEvent, aPresContext, nsnull);
  if (aEventType.LowerCaseEqualsLiteral("keyboardevent") ||
      aEventType.LowerCaseEqualsLiteral("keyevents"))
    return NS_NewDOMKeyboardEvent(aDOMEvent, aPresContext, nsnull);
  if (aEventType.LowerCaseEqualsLiteral("mutationevent") ||
        aEventType.LowerCaseEqualsLiteral("mutationevents"))
    return NS_NewDOMMutationEvent(aDOMEvent, aPresContext, nsnull);
  if (aEventType.LowerCaseEqualsLiteral("textevent") ||
      aEventType.LowerCaseEqualsLiteral("textevents"))
    return NS_NewDOMTextEvent(aDOMEvent, aPresContext, nsnull);
  if (aEventType.LowerCaseEqualsLiteral("popupblockedevents"))
    return NS_NewDOMPopupBlockedEvent(aDOMEvent, aPresContext, nsnull);
  if (aEventType.LowerCaseEqualsLiteral("uievent") ||
      aEventType.LowerCaseEqualsLiteral("uievents"))
    return NS_NewDOMUIEvent(aDOMEvent, aPresContext, nsnull);
  if (aEventType.LowerCaseEqualsLiteral("event") ||
      aEventType.LowerCaseEqualsLiteral("events") ||
      aEventType.LowerCaseEqualsLiteral("htmlevents"))
    return NS_NewDOMEvent(aDOMEvent, aPresContext, nsnull);
#ifdef MOZ_SVG
  if (aEventType.LowerCaseEqualsLiteral("svgevent") ||
      aEventType.LowerCaseEqualsLiteral("svgevents"))
    return NS_NewDOMSVGEvent(aDOMEvent, aPresContext, nsnull);
  if (aEventType.LowerCaseEqualsLiteral("svgzoomevent") ||
      aEventType.LowerCaseEqualsLiteral("svgzoomevents"))
    return NS_NewDOMSVGZoomEvent(aDOMEvent, aPresContext, nsnull);
#endif // MOZ_SVG
  if (aEventType.LowerCaseEqualsLiteral("xulcommandevent") ||
      aEventType.LowerCaseEqualsLiteral("xulcommandevents"))
    return NS_NewDOMXULCommandEvent(aDOMEvent, aPresContext, nsnull);

  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}
