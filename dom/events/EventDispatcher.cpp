/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPresContext.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include <new>
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsINode.h"
#include "nsPIDOMWindow.h"
#include "GeckoProfiler.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/dom/CloseEvent.h"
#include "mozilla/dom/CustomEvent.h"
#include "mozilla/dom/DeviceOrientationEvent.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/FocusEvent.h"
#include "mozilla/dom/HashChangeEvent.h"
#include "mozilla/dom/InputEvent.h"
#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MouseScrollEvent.h"
#include "mozilla/dom/MutationEvent.h"
#include "mozilla/dom/NotifyPaintEvent.h"
#include "mozilla/dom/PageTransitionEvent.h"
#include "mozilla/dom/PointerEvent.h"
#include "mozilla/dom/PopStateEvent.h"
#include "mozilla/dom/ScrollAreaEvent.h"
#include "mozilla/dom/SimpleGestureEvent.h"
#include "mozilla/dom/StorageEvent.h"
#include "mozilla/dom/SVGZoomEvent.h"
#include "mozilla/dom/TimeEvent.h"
#include "mozilla/dom/TouchEvent.h"
#include "mozilla/dom/TransitionEvent.h"
#include "mozilla/dom/WheelEvent.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/XULCommandEvent.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/unused.h"

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#include "mozilla/dom/Element.h"
using namespace mozilla::tasktracer;
#endif

namespace mozilla {

using namespace dom;

class ELMCreationDetector
{
public:
  ELMCreationDetector()
    // We can do this optimization only in the main thread.
    : mNonMainThread(!NS_IsMainThread())
    , mInitialCount(mNonMainThread ?
                      0 : EventListenerManager::sMainThreadCreatedCount)
  {
  }

  bool MayHaveNewListenerManager()
  {
    return mNonMainThread ||
           mInitialCount != EventListenerManager::sMainThreadCreatedCount;
  }

  bool IsMainThread()
  {
    return !mNonMainThread;
  }

private:
  bool mNonMainThread;
  uint32_t mInitialCount;
};

#define NS_TARGET_CHAIN_FORCE_CONTENT_DISPATCH  (1 << 0)
#define NS_TARGET_CHAIN_WANTS_WILL_HANDLE_EVENT (1 << 1)
#define NS_TARGET_CHAIN_MAY_HAVE_MANAGER        (1 << 2)

// EventTargetChainItem represents a single item in the event target chain.
class EventTargetChainItem
{
private:
  explicit EventTargetChainItem(EventTarget* aTarget);
public:
  EventTargetChainItem()
    : mFlags(0)
    , mItemFlags(0)
  {
  }

  static EventTargetChainItem* Create(nsTArray<EventTargetChainItem>& aChain,
                                      EventTarget* aTarget,
                                      EventTargetChainItem* aChild = nullptr)
  {
    MOZ_ASSERT(!aChild || &aChain.ElementAt(aChain.Length() - 1) == aChild);
    return new (aChain.AppendElement()) EventTargetChainItem(aTarget);
  }

  static void DestroyLast(nsTArray<EventTargetChainItem>& aChain,
                          EventTargetChainItem* aItem)
  {
    uint32_t lastIndex = aChain.Length() - 1;
    MOZ_ASSERT(&aChain[lastIndex] == aItem);
    aChain.RemoveElementAt(lastIndex);
  }

  bool IsValid()
  {
    NS_WARN_IF_FALSE(!!(mTarget), "Event target is not valid!");
    return !!(mTarget);
  }

  EventTarget* GetNewTarget()
  {
    return mNewTarget;
  }

  void SetNewTarget(EventTarget* aNewTarget)
  {
    mNewTarget = aNewTarget;
  }

  void SetForceContentDispatch(bool aForce)
  {
    if (aForce) {
      mFlags |= NS_TARGET_CHAIN_FORCE_CONTENT_DISPATCH;
    } else {
      mFlags &= ~NS_TARGET_CHAIN_FORCE_CONTENT_DISPATCH;
    }
  }

  bool ForceContentDispatch()
  {
    return !!(mFlags & NS_TARGET_CHAIN_FORCE_CONTENT_DISPATCH);
  }

  void SetWantsWillHandleEvent(bool aWants)
  {
    if (aWants) {
      mFlags |= NS_TARGET_CHAIN_WANTS_WILL_HANDLE_EVENT;
    } else {
      mFlags &= ~NS_TARGET_CHAIN_WANTS_WILL_HANDLE_EVENT;
    }
  }

  bool WantsWillHandleEvent()
  {
    return !!(mFlags & NS_TARGET_CHAIN_WANTS_WILL_HANDLE_EVENT);
  }

  void SetMayHaveListenerManager(bool aMayHave)
  {
    if (aMayHave) {
      mFlags |= NS_TARGET_CHAIN_MAY_HAVE_MANAGER;
    } else {
      mFlags &= ~NS_TARGET_CHAIN_MAY_HAVE_MANAGER;
    }
  }

  bool MayHaveListenerManager()
  {
    return !!(mFlags & NS_TARGET_CHAIN_MAY_HAVE_MANAGER);
  }
  
  EventTarget* CurrentTarget()
  {
    return mTarget;
  }

  /**
   * Dispatches event through the event target chain.
   * Handles capture, target and bubble phases both in default
   * and system event group and calls also PostHandleEvent for each
   * item in the chain.
   */
  static void HandleEventTargetChain(nsTArray<EventTargetChainItem>& aChain,
                                     EventChainPostVisitor& aVisitor,
                                     EventDispatchingCallback* aCallback,
                                     ELMCreationDetector& aCd);

  /**
   * Resets aVisitor object and calls PreHandleEvent.
   * Copies mItemFlags and mItemData to the current EventTargetChainItem.
   */
  void PreHandleEvent(EventChainPreVisitor& aVisitor);

  /**
   * If the current item in the event target chain has an event listener
   * manager, this method calls EventListenerManager::HandleEvent().
   */
  void HandleEvent(EventChainPostVisitor& aVisitor,
                   ELMCreationDetector& aCd)
  {
    if (WantsWillHandleEvent()) {
      mTarget->WillHandleEvent(aVisitor);
    }
    if (aVisitor.mEvent->mFlags.mPropagationStopped) {
      return;
    }
    if (!mManager) {
      if (!MayHaveListenerManager() && !aCd.MayHaveNewListenerManager()) {
        return;
      }
      mManager = mTarget->GetExistingListenerManager();
    }
    if (mManager) {
      NS_ASSERTION(aVisitor.mEvent->currentTarget == nullptr,
                   "CurrentTarget should be null!");
      mManager->HandleEvent(aVisitor.mPresContext, aVisitor.mEvent,
                            &aVisitor.mDOMEvent,
                            CurrentTarget(),
                            &aVisitor.mEventStatus);
      NS_ASSERTION(aVisitor.mEvent->currentTarget == nullptr,
                   "CurrentTarget should be null!");
    }
  }

  /**
   * Copies mItemFlags and mItemData to aVisitor and calls PostHandleEvent.
   */
  void PostHandleEvent(EventChainPostVisitor& aVisitor);

  nsCOMPtr<EventTarget>             mTarget;
  uint16_t                          mFlags;
  uint16_t                          mItemFlags;
  nsCOMPtr<nsISupports>             mItemData;
  // Event retargeting must happen whenever mNewTarget is non-null.
  nsCOMPtr<EventTarget>             mNewTarget;
  // Cache mTarget's event listener manager.
  nsRefPtr<EventListenerManager>    mManager;
};

EventTargetChainItem::EventTargetChainItem(EventTarget* aTarget)
  : mTarget(aTarget)
  , mFlags(0)
  , mItemFlags(0)
{
  MOZ_ASSERT(!aTarget || mTarget == aTarget->GetTargetForEventTargetChain());
}

void
EventTargetChainItem::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  aVisitor.Reset();
  unused << mTarget->PreHandleEvent(aVisitor);
  SetForceContentDispatch(aVisitor.mForceContentDispatch);
  SetWantsWillHandleEvent(aVisitor.mWantsWillHandleEvent);
  SetMayHaveListenerManager(aVisitor.mMayHaveListenerManager);
  mItemFlags = aVisitor.mItemFlags;
  mItemData = aVisitor.mItemData;
}

void
EventTargetChainItem::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  aVisitor.mItemFlags = mItemFlags;
  aVisitor.mItemData = mItemData;
  mTarget->PostHandleEvent(aVisitor);
}

void
EventTargetChainItem::HandleEventTargetChain(
                        nsTArray<EventTargetChainItem>& aChain,
                        EventChainPostVisitor& aVisitor,
                        EventDispatchingCallback* aCallback,
                        ELMCreationDetector& aCd)
{
  // Save the target so that it can be restored later.
  nsCOMPtr<EventTarget> firstTarget = aVisitor.mEvent->target;
  uint32_t chainLength = aChain.Length();

  // Capture
  aVisitor.mEvent->mFlags.mInCapturePhase = true;
  aVisitor.mEvent->mFlags.mInBubblingPhase = false;
  for (uint32_t i = chainLength - 1; i > 0; --i) {
    EventTargetChainItem& item = aChain[i];
    if ((!aVisitor.mEvent->mFlags.mNoContentDispatch ||
         item.ForceContentDispatch()) &&
        !aVisitor.mEvent->mFlags.mPropagationStopped) {
      item.HandleEvent(aVisitor, aCd);
    }

    if (item.GetNewTarget()) {
      // item is at anonymous boundary. Need to retarget for the child items.
      for (uint32_t j = i; j > 0; --j) {
        uint32_t childIndex = j - 1;
        EventTarget* newTarget = aChain[childIndex].GetNewTarget();
        if (newTarget) {
          aVisitor.mEvent->target = newTarget;
          break;
        }
      }
    }
  }

  // Target
  aVisitor.mEvent->mFlags.mInBubblingPhase = true;
  EventTargetChainItem& targetItem = aChain[0];
  if (!aVisitor.mEvent->mFlags.mPropagationStopped &&
      (!aVisitor.mEvent->mFlags.mNoContentDispatch ||
       targetItem.ForceContentDispatch())) {
    targetItem.HandleEvent(aVisitor, aCd);
  }
  if (aVisitor.mEvent->mFlags.mInSystemGroup) {
    targetItem.PostHandleEvent(aVisitor);
  }

  // Bubble
  aVisitor.mEvent->mFlags.mInCapturePhase = false;
  for (uint32_t i = 1; i < chainLength; ++i) {
    EventTargetChainItem& item = aChain[i];
    EventTarget* newTarget = item.GetNewTarget();
    if (newTarget) {
      // Item is at anonymous boundary. Need to retarget for the current item
      // and for parent items.
      aVisitor.mEvent->target = newTarget;
    }

    if (aVisitor.mEvent->mFlags.mBubbles || newTarget) {
      if ((!aVisitor.mEvent->mFlags.mNoContentDispatch ||
           item.ForceContentDispatch()) &&
          !aVisitor.mEvent->mFlags.mPropagationStopped) {
        item.HandleEvent(aVisitor, aCd);
      }
      if (aVisitor.mEvent->mFlags.mInSystemGroup) {
        item.PostHandleEvent(aVisitor);
      }
    }
  }
  aVisitor.mEvent->mFlags.mInBubblingPhase = false;

  if (!aVisitor.mEvent->mFlags.mInSystemGroup) {
    // Dispatch to the system event group.  Make sure to clear the
    // STOP_DISPATCH flag since this resets for each event group.
    aVisitor.mEvent->mFlags.mPropagationStopped = false;
    aVisitor.mEvent->mFlags.mImmediatePropagationStopped = false;

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
    aVisitor.mEvent->mFlags.mInSystemGroup = true;
    HandleEventTargetChain(aChain,
                           aVisitor,
                           aCallback,
                           aCd);
    aVisitor.mEvent->mFlags.mInSystemGroup = false;

    // After dispatch, clear all the propagation flags so that
    // system group listeners don't affect to the event.
    aVisitor.mEvent->mFlags.mPropagationStopped = false;
    aVisitor.mEvent->mFlags.mImmediatePropagationStopped = false;
  }
}

static nsTArray<EventTargetChainItem>* sCachedMainThreadChain = nullptr;

/* static */ void
EventDispatcher::Shutdown()
{
  delete sCachedMainThreadChain;
  sCachedMainThreadChain = nullptr;
}

EventTargetChainItem*
EventTargetChainItemForChromeTarget(nsTArray<EventTargetChainItem>& aChain,
                                    nsINode* aNode,
                                    EventTargetChainItem* aChild = nullptr)
{
  if (!aNode->IsInComposedDoc()) {
    return nullptr;
  }
  nsPIDOMWindow* win = aNode->OwnerDoc()->GetInnerWindow();
  EventTarget* piTarget = win ? win->GetParentTarget() : nullptr;
  NS_ENSURE_TRUE(piTarget, nullptr);

  EventTargetChainItem* etci =
    EventTargetChainItem::Create(aChain,
                                 piTarget->GetTargetForEventTargetChain(),
                                 aChild);
  if (!etci->IsValid()) {
    EventTargetChainItem::DestroyLast(aChain, etci);
    return nullptr;
  }
  return etci;
}

/* static */ nsresult
EventDispatcher::Dispatch(nsISupports* aTarget,
                          nsPresContext* aPresContext,
                          WidgetEvent* aEvent,
                          nsIDOMEvent* aDOMEvent,
                          nsEventStatus* aEventStatus,
                          EventDispatchingCallback* aCallback,
                          nsTArray<EventTarget*>* aTargets)
{
  PROFILER_LABEL("EventDispatcher", "Dispatch",
    js::ProfileEntry::Category::EVENTS);

  NS_ASSERTION(aEvent, "Trying to dispatch without WidgetEvent!");
  NS_ENSURE_TRUE(!aEvent->mFlags.mIsBeingDispatched,
                 NS_ERROR_DOM_INVALID_STATE_ERR);
  NS_ASSERTION(!aTargets || !aEvent->mMessage, "Wrong parameters!");

  // If we're dispatching an already created DOMEvent object, make
  // sure it is initialized!
  // If aTargets is non-null, the event isn't going to be dispatched.
  NS_ENSURE_TRUE(aEvent->mMessage || !aDOMEvent || aTargets,
                 NS_ERROR_DOM_INVALID_STATE_ERR);

#ifdef MOZ_TASK_TRACER
  {
    if (aDOMEvent) {
      nsAutoString eventType;
      aDOMEvent->GetType(eventType);

      nsCOMPtr<Element> element = do_QueryInterface(aTarget);
      nsAutoString elementId;
      nsAutoString elementTagName;
      if (element) {
        element->GetId(elementId);
        element->GetTagName(elementTagName);
      }
      AddLabel("Event [%s] dispatched at target [id:%s tag:%s]",
               NS_ConvertUTF16toUTF8(eventType).get(),
               NS_ConvertUTF16toUTF8(elementId).get(),
               NS_ConvertUTF16toUTF8(elementTagName).get());
    }
  }
#endif

  nsCOMPtr<EventTarget> target = do_QueryInterface(aTarget);

  bool retargeted = false;

  if (aEvent->mFlags.mRetargetToNonNativeAnonymous) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(target);
    if (content && content->IsInNativeAnonymousSubtree()) {
      nsCOMPtr<EventTarget> newTarget =
        do_QueryInterface(content->FindFirstNonChromeOnlyAccessContent());
      NS_ENSURE_STATE(newTarget);

      aEvent->originalTarget = target;
      target = newTarget;
      retargeted = true;
    }
  }

  if (aEvent->mFlags.mOnlyChromeDispatch) {
    nsCOMPtr<nsINode> node = do_QueryInterface(aTarget);
    if (!node) {
      nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aTarget);
      if (win) {
        node = win->GetExtantDoc();
      }
    }

    NS_ENSURE_STATE(node);
    nsIDocument* doc = node->OwnerDoc();
    if (!nsContentUtils::IsChromeDoc(doc)) {
      nsPIDOMWindow* win = doc ? doc->GetInnerWindow() : nullptr;
      // If we can't dispatch the event to chrome, do nothing.
      EventTarget* piTarget = win ? win->GetParentTarget() : nullptr;
      if (!piTarget) {
        return NS_OK;
      }

      // Set the target to be the original dispatch target,
      aEvent->target = target;
      // but use chrome event handler or TabChildGlobal for event target chain.
      target = piTarget;
    }
  }

#ifdef DEBUG
  if (aEvent->mMessage != eVoidEvent &&
      !nsContentUtils::IsSafeToRunScript()) {
    nsresult rv = NS_ERROR_FAILURE;
    if (target->GetContextForEventHandlers(&rv) ||
        NS_FAILED(rv)) {
      nsCOMPtr<nsINode> node = do_QueryInterface(target);
      if (node && nsContentUtils::IsChromeDoc(node->OwnerDoc())) {
        NS_WARNING("Fix the caller!");
      } else {
        NS_ERROR("This is unsafe! Fix the caller!");
      }
    }
  }

  if (aDOMEvent) {
    WidgetEvent* innerEvent = aDOMEvent->GetInternalNSEvent();
    NS_ASSERTION(innerEvent == aEvent,
                  "The inner event of aDOMEvent is not the same as aEvent!");
  }
#endif

  nsresult rv = NS_OK;
  bool externalDOMEvent = !!(aDOMEvent);

  // If we have a PresContext, make sure it doesn't die before
  // event dispatching is finished.
  nsRefPtr<nsPresContext> kungFuDeathGrip(aPresContext);

  ELMCreationDetector cd;
  nsTArray<EventTargetChainItem> chain;
  if (cd.IsMainThread()) {
    if (!sCachedMainThreadChain) {
      sCachedMainThreadChain = new nsTArray<EventTargetChainItem>();
    }
    chain.SwapElements(*sCachedMainThreadChain);
    chain.SetCapacity(128);
  }

  // Create the event target chain item for the event target.
  EventTargetChainItem* targetEtci =
    EventTargetChainItem::Create(chain, target->GetTargetForEventTargetChain());
  MOZ_ASSERT(&chain[0] == targetEtci);
  if (!targetEtci->IsValid()) {
    EventTargetChainItem::DestroyLast(chain, targetEtci);
    return NS_ERROR_FAILURE;
  }

  // Make sure that nsIDOMEvent::target and nsIDOMEvent::originalTarget
  // point to the last item in the chain.
  if (!aEvent->target) {
    // Note, CurrentTarget() points always to the object returned by
    // GetTargetForEventTargetChain().
    aEvent->target = targetEtci->CurrentTarget();
  } else {
    // XXX But if the target is already set, use that. This is a hack
    //     for the 'load', 'beforeunload' and 'unload' events,
    //     which are dispatched to |window| but have document as their target.
    //
    // Make sure that the event target points to the right object.
    aEvent->target = aEvent->target->GetTargetForEventTargetChain();
    NS_ENSURE_STATE(aEvent->target);
  }

  if (retargeted) {
    aEvent->originalTarget =
      aEvent->originalTarget->GetTargetForEventTargetChain();
    NS_ENSURE_STATE(aEvent->originalTarget);
  }
  else {
    aEvent->originalTarget = aEvent->target;
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(aEvent->originalTarget);
  bool isInAnon = (content && (content->IsInAnonymousSubtree() ||
                               content->IsInShadowTree()));

  aEvent->mFlags.mIsBeingDispatched = true;

  // Create visitor object and start event dispatching.
  // PreHandleEvent for the original target.
  nsEventStatus status = aEventStatus ? *aEventStatus : nsEventStatus_eIgnore;
  EventChainPreVisitor preVisitor(aPresContext, aEvent, aDOMEvent, status,
                                  isInAnon);
  targetEtci->PreHandleEvent(preVisitor);

  if (!preVisitor.mCanHandle && preVisitor.mAutomaticChromeDispatch && content) {
    // Event target couldn't handle the event. Try to propagate to chrome.
    EventTargetChainItem::DestroyLast(chain, targetEtci);
    targetEtci = EventTargetChainItemForChromeTarget(chain, content);
    NS_ENSURE_STATE(targetEtci);
    MOZ_ASSERT(&chain[0] == targetEtci);
    targetEtci->PreHandleEvent(preVisitor);
  }
  if (preVisitor.mCanHandle) {
    // At least the original target can handle the event.
    // Setting the retarget to the |target| simplifies retargeting code.
    nsCOMPtr<EventTarget> t = do_QueryInterface(aEvent->target);
    targetEtci->SetNewTarget(t);
    EventTargetChainItem* topEtci = targetEtci;
    targetEtci = nullptr;
    while (preVisitor.mParentTarget) {
      EventTarget* parentTarget = preVisitor.mParentTarget;
      EventTargetChainItem* parentEtci =
        EventTargetChainItem::Create(chain, preVisitor.mParentTarget, topEtci);
      if (!parentEtci->IsValid()) {
        EventTargetChainItem::DestroyLast(chain, parentEtci);
        rv = NS_ERROR_FAILURE;
        break;
      }

      // Item needs event retargetting.
      if (preVisitor.mEventTargetAtParent) {
        // Need to set the target of the event
        // so that also the next retargeting works.
        preVisitor.mEvent->target = preVisitor.mEventTargetAtParent;
        parentEtci->SetNewTarget(preVisitor.mEventTargetAtParent);
      }

      parentEtci->PreHandleEvent(preVisitor);
      if (preVisitor.mCanHandle) {
        topEtci = parentEtci;
      } else {
        EventTargetChainItem::DestroyLast(chain, parentEtci);
        parentEtci = nullptr;
        if (preVisitor.mAutomaticChromeDispatch && content) {
          // Even if the current target can't handle the event, try to
          // propagate to chrome.
          nsCOMPtr<nsINode> disabledTarget = do_QueryInterface(parentTarget);
          if (disabledTarget) {
            parentEtci = EventTargetChainItemForChromeTarget(chain,
                                                             disabledTarget,
                                                             topEtci);
            if (parentEtci) {
              parentEtci->PreHandleEvent(preVisitor);
              if (preVisitor.mCanHandle) {
                chain[0].SetNewTarget(parentTarget);
                topEtci = parentEtci;
                continue;
              }
            }
          }
        }
        break;
      }
    }
    if (NS_SUCCEEDED(rv)) {
      if (aTargets) {
        aTargets->Clear();
        aTargets->SetCapacity(chain.Length());
        for (uint32_t i = 0; i < chain.Length(); ++i) {
          aTargets->AppendElement(chain[i].CurrentTarget()->GetTargetForDOMEvent());
        }
      } else {
        // Event target chain is created. Handle the chain.
        EventChainPostVisitor postVisitor(preVisitor);
        EventTargetChainItem::HandleEventTargetChain(chain, postVisitor,
                                                     aCallback, cd);

        preVisitor.mEventStatus = postVisitor.mEventStatus;
        // If the DOM event was created during event flow.
        if (!preVisitor.mDOMEvent && postVisitor.mDOMEvent) {
          preVisitor.mDOMEvent = postVisitor.mDOMEvent;
        }
      }
    }
  }

  // Note, EventTargetChainItem objects are deleted when the chain goes out of
  // the scope.

  aEvent->mFlags.mIsBeingDispatched = false;
  aEvent->mFlags.mDispatchedAtLeastOnce = true;

  if (!externalDOMEvent && preVisitor.mDOMEvent) {
    // An dom::Event was created while dispatching the event.
    // Duplicate private data if someone holds a pointer to it.
    nsrefcnt rc = 0;
    NS_RELEASE2(preVisitor.mDOMEvent, rc);
    if (preVisitor.mDOMEvent) {
      preVisitor.mDOMEvent->DuplicatePrivateData();
    }
  }

  if (aEventStatus) {
    *aEventStatus = preVisitor.mEventStatus;
  }

  if (cd.IsMainThread() && chain.Capacity() == 128 && sCachedMainThreadChain) {
    chain.ClearAndRetainStorage();
    chain.SwapElements(*sCachedMainThreadChain);
  }

  return rv;
}

/* static */ nsresult
EventDispatcher::DispatchDOMEvent(nsISupports* aTarget,
                                  WidgetEvent* aEvent,
                                  nsIDOMEvent* aDOMEvent,
                                  nsPresContext* aPresContext,
                                  nsEventStatus* aEventStatus)
{
  if (aDOMEvent) {
    WidgetEvent* innerEvent = aDOMEvent->GetInternalNSEvent();
    NS_ENSURE_TRUE(innerEvent, NS_ERROR_ILLEGAL_VALUE);

    bool dontResetTrusted = false;
    if (innerEvent->mFlags.mDispatchedAtLeastOnce) {
      innerEvent->target = nullptr;
      innerEvent->originalTarget = nullptr;
    } else {
      aDOMEvent->GetIsTrusted(&dontResetTrusted);
    }

    if (!dontResetTrusted) {
      //Check security state to determine if dispatcher is trusted
      bool trusted = NS_IsMainThread() ? nsContentUtils::LegacyIsCallerChromeOrNativeCode()
                                       : mozilla::dom::workers::IsCurrentThreadRunningChromeWorker();
      aDOMEvent->SetTrusted(trusted);
    }

    return EventDispatcher::Dispatch(aTarget, aPresContext, innerEvent,
                                     aDOMEvent, aEventStatus);
  } else if (aEvent) {
    return EventDispatcher::Dispatch(aTarget, aPresContext, aEvent,
                                     aDOMEvent, aEventStatus);
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

/* static */ already_AddRefed<dom::Event>
EventDispatcher::CreateEvent(EventTarget* aOwner,
                             nsPresContext* aPresContext,
                             WidgetEvent* aEvent,
                             const nsAString& aEventType)
{
  if (aEvent) {
    switch(aEvent->mClass) {
    case eMutationEventClass:
      return NS_NewDOMMutationEvent(aOwner, aPresContext,
                                    aEvent->AsMutationEvent());
    case eGUIEventClass:
    case eScrollPortEventClass:
    case eUIEventClass:
      return NS_NewDOMUIEvent(aOwner, aPresContext, aEvent->AsGUIEvent());
    case eScrollAreaEventClass:
      return NS_NewDOMScrollAreaEvent(aOwner, aPresContext,
                                      aEvent->AsScrollAreaEvent());
    case eKeyboardEventClass:
      return NS_NewDOMKeyboardEvent(aOwner, aPresContext,
                                    aEvent->AsKeyboardEvent());
    case eBeforeAfterKeyboardEventClass:
      return NS_NewDOMBeforeAfterKeyboardEvent(aOwner, aPresContext,
                                               aEvent->AsBeforeAfterKeyboardEvent());
    case eCompositionEventClass:
      return NS_NewDOMCompositionEvent(aOwner, aPresContext,
                                       aEvent->AsCompositionEvent());
    case eMouseEventClass:
      return NS_NewDOMMouseEvent(aOwner, aPresContext, aEvent->AsMouseEvent());
    case eFocusEventClass:
      return NS_NewDOMFocusEvent(aOwner, aPresContext, aEvent->AsFocusEvent());
    case eMouseScrollEventClass:
      return NS_NewDOMMouseScrollEvent(aOwner, aPresContext,
                                       aEvent->AsMouseScrollEvent());
    case eWheelEventClass:
      return NS_NewDOMWheelEvent(aOwner, aPresContext, aEvent->AsWheelEvent());
    case eEditorInputEventClass:
      return NS_NewDOMInputEvent(aOwner, aPresContext,
                                 aEvent->AsEditorInputEvent());
    case eDragEventClass:
      return NS_NewDOMDragEvent(aOwner, aPresContext, aEvent->AsDragEvent());
    case eClipboardEventClass:
      return NS_NewDOMClipboardEvent(aOwner, aPresContext,
                                     aEvent->AsClipboardEvent());
    case eSVGZoomEventClass:
      return NS_NewDOMSVGZoomEvent(aOwner, aPresContext,
                                   aEvent->AsSVGZoomEvent());
    case eSMILTimeEventClass:
      return NS_NewDOMTimeEvent(aOwner, aPresContext,
                                aEvent->AsSMILTimeEvent());
    case eCommandEventClass:
      return NS_NewDOMCommandEvent(aOwner, aPresContext,
                                   aEvent->AsCommandEvent());
    case eSimpleGestureEventClass:
      return NS_NewDOMSimpleGestureEvent(aOwner, aPresContext,
                                         aEvent->AsSimpleGestureEvent());
    case ePointerEventClass:
      return NS_NewDOMPointerEvent(aOwner, aPresContext,
                                   aEvent->AsPointerEvent());
    case eTouchEventClass:
      return NS_NewDOMTouchEvent(aOwner, aPresContext, aEvent->AsTouchEvent());
    case eTransitionEventClass:
      return NS_NewDOMTransitionEvent(aOwner, aPresContext,
                                      aEvent->AsTransitionEvent());
    case eAnimationEventClass:
      return NS_NewDOMAnimationEvent(aOwner, aPresContext,
                                     aEvent->AsAnimationEvent());
    default:
      // For all other types of events, create a vanilla event object.
      return NS_NewDOMEvent(aOwner, aPresContext, aEvent);
    }
  }

  // And if we didn't get an event, check the type argument.

  if (aEventType.LowerCaseEqualsLiteral("mouseevent") ||
      aEventType.LowerCaseEqualsLiteral("mouseevents") ||
      aEventType.LowerCaseEqualsLiteral("popupevents"))
    return NS_NewDOMMouseEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("mousescrollevents"))
    return NS_NewDOMMouseScrollEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("dragevent") ||
      aEventType.LowerCaseEqualsLiteral("dragevents"))
    return NS_NewDOMDragEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("keyboardevent") ||
      aEventType.LowerCaseEqualsLiteral("keyevents"))
    return NS_NewDOMKeyboardEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("compositionevent") ||
      aEventType.LowerCaseEqualsLiteral("textevent") ||
      aEventType.LowerCaseEqualsLiteral("textevents")) {
    return NS_NewDOMCompositionEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("mutationevent") ||
        aEventType.LowerCaseEqualsLiteral("mutationevents"))
    return NS_NewDOMMutationEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("deviceorientationevent")) {
    DeviceOrientationEventInit init;
    return DeviceOrientationEvent::Constructor(aOwner, EmptyString(), init);
  }
  if (aEventType.LowerCaseEqualsLiteral("devicemotionevent"))
    return NS_NewDOMDeviceMotionEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("uievent") ||
      aEventType.LowerCaseEqualsLiteral("uievents"))
    return NS_NewDOMUIEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("event") ||
      aEventType.LowerCaseEqualsLiteral("events") ||
      aEventType.LowerCaseEqualsLiteral("htmlevents") ||
      aEventType.LowerCaseEqualsLiteral("svgevent") ||
      aEventType.LowerCaseEqualsLiteral("svgevents"))
    return NS_NewDOMEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("svgzoomevent") ||
      aEventType.LowerCaseEqualsLiteral("svgzoomevents"))
    return NS_NewDOMSVGZoomEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("timeevent") ||
      aEventType.LowerCaseEqualsLiteral("timeevents"))
    return NS_NewDOMTimeEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("xulcommandevent") ||
      aEventType.LowerCaseEqualsLiteral("xulcommandevents"))
    return NS_NewDOMXULCommandEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("commandevent") ||
      aEventType.LowerCaseEqualsLiteral("commandevents"))
    return NS_NewDOMCommandEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("datacontainerevent") ||
      aEventType.LowerCaseEqualsLiteral("datacontainerevents"))
    return NS_NewDOMDataContainerEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("messageevent"))
    return NS_NewDOMMessageEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("notifypaintevent"))
    return NS_NewDOMNotifyPaintEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("simplegestureevent"))
    return NS_NewDOMSimpleGestureEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("beforeunloadevent"))
    return NS_NewDOMBeforeUnloadEvent(aOwner, aPresContext, nullptr);
  // XXXkhuey this is broken
  if (aEventType.LowerCaseEqualsLiteral("pagetransition")) {
    PageTransitionEventInit init;
    return PageTransitionEvent::Constructor(aOwner, EmptyString(), init);
  }
  if (aEventType.LowerCaseEqualsLiteral("scrollareaevent"))
    return NS_NewDOMScrollAreaEvent(aOwner, aPresContext, nullptr);
  // XXXkhuey Chrome supports popstateevent here, even though it provides no
  // initPopStateEvent method.  This is nuts ... but copying it is unlikely to
  // break the web.
  if (aEventType.LowerCaseEqualsLiteral("popstateevent")) {
    AutoJSContext cx;
    RootedDictionary<PopStateEventInit> init(cx);
    return PopStateEvent::Constructor(aOwner, EmptyString(), init);
  }
  if (aEventType.LowerCaseEqualsLiteral("touchevent") &&
      TouchEvent::PrefEnabled())
    return NS_NewDOMTouchEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("hashchangeevent")) {
    HashChangeEventInit init;
    return HashChangeEvent::Constructor(aOwner, EmptyString(), init);
  }
  if (aEventType.LowerCaseEqualsLiteral("customevent"))
    return NS_NewDOMCustomEvent(aOwner, aPresContext, nullptr);
  if (aEventType.LowerCaseEqualsLiteral("storageevent")) {
    return NS_NewDOMStorageEvent(aOwner);
  }


  // NEW EVENT TYPES SHOULD NOT BE ADDED HERE; THEY SHOULD USE ONLY EVENT
  // CONSTRUCTORS

  return nullptr;
}

} // namespace mozilla
