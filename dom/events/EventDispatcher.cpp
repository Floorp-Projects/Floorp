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
#include "AnimationEvent.h"
#include "BeforeAfterKeyboardEvent.h"
#include "BeforeUnloadEvent.h"
#include "ClipboardEvent.h"
#include "CommandEvent.h"
#include "CompositionEvent.h"
#include "DataContainerEvent.h"
#include "DeviceMotionEvent.h"
#include "DragEvent.h"
#include "GeckoProfiler.h"
#include "KeyboardEvent.h"
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
#include "mozilla/Telemetry.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/Unused.h"

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

static bool IsEventTargetChrome(EventTarget* aEventTarget,
                                nsIDocument** aDocument = nullptr)
{
  if (aDocument) {
    *aDocument = nullptr;
  }

  if (NS_WARN_IF(!aEventTarget)) {
    return false;
  }

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aEventTarget);
  if (!doc) {
    nsCOMPtr<nsINode> node = do_QueryInterface(aEventTarget);
    if (node) {
      doc = node->OwnerDoc();
    } else {
      nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aEventTarget);
      if (!window) {
        return false;
      }
      doc = window->GetExtantDoc();
    }
    if (!doc) {
      return false;
    }
  }
  bool isChrome = nsContentUtils::IsChromeDoc(doc);
  if (aDocument) {
    doc.swap(*aDocument);
  }
  return isChrome;
}


// EventTargetChainItem represents a single item in the event target chain.
class EventTargetChainItem
{
private:
  explicit EventTargetChainItem(EventTarget* aTarget);
public:
  EventTargetChainItem()
    : mItemFlags(0)
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

  static EventTargetChainItem* GetFirstCanHandleEventTarget(
                                 nsTArray<EventTargetChainItem>& aChain)
  {
    return &aChain[GetFirstCanHandleEventTargetIdx(aChain)];
  }

  static uint32_t GetFirstCanHandleEventTargetIdx(nsTArray<EventTargetChainItem>& aChain)
  {
    // aChain[i].PreHandleEventOnly() = true only when the target element wants
    // PreHandleEvent and set mCanHandle=false. So we find the first element
    // which can handle the event.
    for (uint32_t i = 0; i < aChain.Length(); ++i) {
      if (!aChain[i].PreHandleEventOnly()) {
        return i;
      }
    }
    MOZ_ASSERT(false);
    return 0;
  }

  bool IsValid()
  {
    NS_WARNING_ASSERTION(!!(mTarget), "Event target is not valid!");
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
    mFlags.mForceContentDispatch = aForce;
  }

  bool ForceContentDispatch()
  {
    return mFlags.mForceContentDispatch;
  }

  void SetWantsWillHandleEvent(bool aWants)
  {
    mFlags.mWantsWillHandleEvent = aWants;
  }

  bool WantsWillHandleEvent()
  {
    return mFlags.mWantsWillHandleEvent;
  }

  void SetWantsPreHandleEvent(bool aWants)
  {
    mFlags.mWantsPreHandleEvent = aWants;
  }

  bool WantsPreHandleEvent()
  {
    return mFlags.mWantsPreHandleEvent;
  }

  void SetPreHandleEventOnly(bool aWants)
  {
    mFlags.mPreHandleEventOnly = aWants;
  }

  bool PreHandleEventOnly()
  {
    return mFlags.mPreHandleEventOnly;
  }

  void SetMayHaveListenerManager(bool aMayHave)
  {
    mFlags.mMayHaveManager = aMayHave;
  }

  bool MayHaveListenerManager()
  {
    return mFlags.mMayHaveManager;
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
   * Resets aVisitor object and calls GetEventTargetParent.
   * Copies mItemFlags and mItemData to the current EventTargetChainItem.
   */
  void GetEventTargetParent(EventChainPreVisitor& aVisitor);

  /**
   * Calls PreHandleEvent for those items which called SetWantsPreHandleEvent.
   */
  void PreHandleEvent(EventChainVisitor& aVisitor);

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
    if (aVisitor.mEvent->PropagationStopped()) {
      return;
    }
    if (aVisitor.mEvent->mFlags.mOnlySystemGroupDispatchInContent &&
        !aVisitor.mEvent->mFlags.mInSystemGroup &&
        !IsCurrentTargetChrome()) {
      return;
    }
    if (!mManager) {
      if (!MayHaveListenerManager() && !aCd.MayHaveNewListenerManager()) {
        return;
      }
      mManager = mTarget->GetExistingListenerManager();
    }
    if (mManager) {
      NS_ASSERTION(aVisitor.mEvent->mCurrentTarget == nullptr,
                   "CurrentTarget should be null!");
      mManager->HandleEvent(aVisitor.mPresContext, aVisitor.mEvent,
                            &aVisitor.mDOMEvent,
                            CurrentTarget(),
                            &aVisitor.mEventStatus);
      NS_ASSERTION(aVisitor.mEvent->mCurrentTarget == nullptr,
                   "CurrentTarget should be null!");
    }
  }

  /**
   * Copies mItemFlags and mItemData to aVisitor and calls PostHandleEvent.
   */
  void PostHandleEvent(EventChainPostVisitor& aVisitor);

private:
  nsCOMPtr<EventTarget>             mTarget;

  class EventTargetChainFlags
  {
  public:
    explicit EventTargetChainFlags()
    {
      SetRawFlags(0);
    }
    // Cached flags for each EventTargetChainItem which are set when calling
    // GetEventTargetParent to create event target chain. They are used to
    // manage or speedup event dispatching.
    bool mForceContentDispatch : 1;
    bool mWantsWillHandleEvent : 1;
    bool mMayHaveManager : 1;
    bool mChechedIfChrome : 1;
    bool mIsChromeContent : 1;
    bool mWantsPreHandleEvent : 1;
    bool mPreHandleEventOnly : 1;
  private:
    typedef uint32_t RawFlags;
    void SetRawFlags(RawFlags aRawFlags)
    {
      static_assert(sizeof(EventTargetChainFlags) <= sizeof(RawFlags),
        "EventTargetChainFlags must not be bigger than the RawFlags");
      memcpy(this, &aRawFlags, sizeof(EventTargetChainFlags));
    }
  } mFlags;

  uint16_t                          mItemFlags;
  nsCOMPtr<nsISupports>             mItemData;
  // Event retargeting must happen whenever mNewTarget is non-null.
  nsCOMPtr<EventTarget>             mNewTarget;
  // Cache mTarget's event listener manager.
  RefPtr<EventListenerManager>    mManager;

  bool IsCurrentTargetChrome()
  {
    if (!mFlags.mChechedIfChrome) {
      mFlags.mChechedIfChrome = true;
      if (IsEventTargetChrome(mTarget)) {
        mFlags.mIsChromeContent = true;
      }
    }
    return mFlags.mIsChromeContent;
  }
};

EventTargetChainItem::EventTargetChainItem(EventTarget* aTarget)
  : mTarget(aTarget)
  , mItemFlags(0)
{
  MOZ_ASSERT(!aTarget || mTarget == aTarget->GetTargetForEventTargetChain());
}

void
EventTargetChainItem::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  aVisitor.Reset();
  Unused << mTarget->GetEventTargetParent(aVisitor);
  SetForceContentDispatch(aVisitor.mForceContentDispatch);
  SetWantsWillHandleEvent(aVisitor.mWantsWillHandleEvent);
  SetMayHaveListenerManager(aVisitor.mMayHaveListenerManager);
  SetWantsPreHandleEvent(aVisitor.mWantsPreHandleEvent);
  SetPreHandleEventOnly(aVisitor.mWantsPreHandleEvent && !aVisitor.mCanHandle);
  mItemFlags = aVisitor.mItemFlags;
  mItemData = aVisitor.mItemData;
}

void
EventTargetChainItem::PreHandleEvent(EventChainVisitor& aVisitor)
{
  if (!WantsPreHandleEvent()) {
    return;
  }
  aVisitor.mItemFlags = mItemFlags;
  aVisitor.mItemData = mItemData;
  Unused << mTarget->PreHandleEvent(aVisitor);
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
  nsCOMPtr<EventTarget> firstTarget = aVisitor.mEvent->mTarget;
  uint32_t chainLength = aChain.Length();
  uint32_t firstCanHandleEventTargetIdx =
    EventTargetChainItem::GetFirstCanHandleEventTargetIdx(aChain);

  // Capture
  aVisitor.mEvent->mFlags.mInCapturePhase = true;
  aVisitor.mEvent->mFlags.mInBubblingPhase = false;
  for (uint32_t i = chainLength - 1; i > firstCanHandleEventTargetIdx; --i) {
    EventTargetChainItem& item = aChain[i];
    if (item.PreHandleEventOnly()) {
      continue;
    }
    if ((!aVisitor.mEvent->mFlags.mNoContentDispatch ||
         item.ForceContentDispatch()) &&
        !aVisitor.mEvent->PropagationStopped()) {
      item.HandleEvent(aVisitor, aCd);
    }

    if (item.GetNewTarget()) {
      // item is at anonymous boundary. Need to retarget for the child items.
      for (uint32_t j = i; j > 0; --j) {
        uint32_t childIndex = j - 1;
        EventTarget* newTarget = aChain[childIndex].GetNewTarget();
        if (newTarget) {
          aVisitor.mEvent->mTarget = newTarget;
          break;
        }
      }
    }
  }

  // Target
  aVisitor.mEvent->mFlags.mInBubblingPhase = true;
  EventTargetChainItem& targetItem = aChain[firstCanHandleEventTargetIdx];
  if (!aVisitor.mEvent->PropagationStopped() &&
      (!aVisitor.mEvent->mFlags.mNoContentDispatch ||
       targetItem.ForceContentDispatch())) {
    targetItem.HandleEvent(aVisitor, aCd);
  }
  if (aVisitor.mEvent->mFlags.mInSystemGroup) {
    targetItem.PostHandleEvent(aVisitor);
  }

  // Bubble
  aVisitor.mEvent->mFlags.mInCapturePhase = false;
  for (uint32_t i = firstCanHandleEventTargetIdx + 1; i < chainLength; ++i) {
    EventTargetChainItem& item = aChain[i];
    if (item.PreHandleEventOnly()) {
      continue;
    }
    EventTarget* newTarget = item.GetNewTarget();
    if (newTarget) {
      // Item is at anonymous boundary. Need to retarget for the current item
      // and for parent items.
      aVisitor.mEvent->mTarget = newTarget;
    }

    if (aVisitor.mEvent->mFlags.mBubbles || newTarget) {
      if ((!aVisitor.mEvent->mFlags.mNoContentDispatch ||
           item.ForceContentDispatch()) &&
          !aVisitor.mEvent->PropagationStopped()) {
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
    aVisitor.mEvent->mTarget = aVisitor.mEvent->mOriginalTarget;

    // Special handling if PresShell (or some other caller)
    // used a callback object.
    if (aCallback) {
      aCallback->HandleEvent(aVisitor);
    }

    // Retarget for system event group (which does the default handling too).
    // Setting back the target which was used also for default event group.
    aVisitor.mEvent->mTarget = firstTarget;
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
  nsPIDOMWindowInner* win = aNode->OwnerDoc()->GetInnerWindow();
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

/* static */ EventTargetChainItem*
MayRetargetToChromeIfCanNotHandleEvent(
  nsTArray<EventTargetChainItem>& aChain, EventChainPreVisitor& aPreVisitor,
  EventTargetChainItem* aTargetEtci, EventTargetChainItem* aChildEtci,
  nsINode* aContent)
{
  if (!aPreVisitor.mWantsPreHandleEvent) {
    // Keep EventTargetChainItem if we need to call PreHandleEvent on it.
    EventTargetChainItem::DestroyLast(aChain, aTargetEtci);
  }
  if (aPreVisitor.mAutomaticChromeDispatch && aContent) {
    // Event target couldn't handle the event. Try to propagate to chrome.
    EventTargetChainItem* chromeTargetEtci =
      EventTargetChainItemForChromeTarget(aChain, aContent, aChildEtci);
    if (chromeTargetEtci) {
      chromeTargetEtci->GetEventTargetParent(aPreVisitor);
      return chromeTargetEtci;
    }
  }
  return nullptr;
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

      aEvent->mOriginalTarget = target;
      target = newTarget;
      retargeted = true;
    }
  }

  if (aEvent->mFlags.mOnlyChromeDispatch) {
    nsCOMPtr<nsIDocument> doc;
    if (!IsEventTargetChrome(target, getter_AddRefs(doc)) && doc) {
      nsPIDOMWindowInner* win = doc->GetInnerWindow();
      // If we can't dispatch the event to chrome, do nothing.
      EventTarget* piTarget = win ? win->GetParentTarget() : nullptr;
      if (!piTarget) {
        return NS_OK;
      }

      // Set the target to be the original dispatch target,
      aEvent->mTarget = target;
      // but use chrome event handler or TabChildGlobal for event target chain.
      target = piTarget;
    } else if (NS_WARN_IF(!doc)) {
      return NS_ERROR_UNEXPECTED;
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
    WidgetEvent* innerEvent = aDOMEvent->WidgetEventPtr();
    NS_ASSERTION(innerEvent == aEvent,
                  "The inner event of aDOMEvent is not the same as aEvent!");
  }
#endif

  nsresult rv = NS_OK;
  bool externalDOMEvent = !!(aDOMEvent);

  // If we have a PresContext, make sure it doesn't die before
  // event dispatching is finished.
  RefPtr<nsPresContext> kungFuDeathGrip(aPresContext);

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
  if (!aEvent->mTarget) {
    // Note, CurrentTarget() points always to the object returned by
    // GetTargetForEventTargetChain().
    aEvent->mTarget = targetEtci->CurrentTarget();
  } else {
    // XXX But if the target is already set, use that. This is a hack
    //     for the 'load', 'beforeunload' and 'unload' events,
    //     which are dispatched to |window| but have document as their target.
    //
    // Make sure that the event target points to the right object.
    aEvent->mTarget = aEvent->mTarget->GetTargetForEventTargetChain();
    NS_ENSURE_STATE(aEvent->mTarget);
  }

  if (retargeted) {
    aEvent->mOriginalTarget =
      aEvent->mOriginalTarget->GetTargetForEventTargetChain();
    NS_ENSURE_STATE(aEvent->mOriginalTarget);
  }
  else {
    aEvent->mOriginalTarget = aEvent->mTarget;
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(aEvent->mOriginalTarget);
  bool isInAnon = (content && (content->IsInAnonymousSubtree() ||
                               content->IsInShadowTree()));

  aEvent->mFlags.mIsBeingDispatched = true;

  // Create visitor object and start event dispatching.
  // GetEventTargetParent for the original target.
  nsEventStatus status = aEventStatus ? *aEventStatus : nsEventStatus_eIgnore;
  EventChainPreVisitor preVisitor(aPresContext, aEvent, aDOMEvent, status,
                                  isInAnon);
  targetEtci->GetEventTargetParent(preVisitor);

  if (!preVisitor.mCanHandle) {
    targetEtci = MayRetargetToChromeIfCanNotHandleEvent(chain, preVisitor,
                                                        targetEtci, nullptr,
                                                        content);
  }
  if (!preVisitor.mCanHandle) {
    // The original target and chrome target (mAutomaticChromeDispatch=true)
    // can not handle the event but we still have to call their PreHandleEvent.
    for (uint32_t i = 0; i < chain.Length(); ++i) {
      chain[i].PreHandleEvent(preVisitor);
    }
  } else {
    // At least the original target can handle the event.
    // Setting the retarget to the |target| simplifies retargeting code.
    nsCOMPtr<EventTarget> t = do_QueryInterface(aEvent->mTarget);
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
        preVisitor.mEvent->mTarget = preVisitor.mEventTargetAtParent;
        parentEtci->SetNewTarget(preVisitor.mEventTargetAtParent);
      }

      parentEtci->GetEventTargetParent(preVisitor);
      if (preVisitor.mCanHandle) {
        topEtci = parentEtci;
      } else {
        nsCOMPtr<nsINode> disabledTarget = do_QueryInterface(parentTarget);
        parentEtci = MayRetargetToChromeIfCanNotHandleEvent(chain,
                                                            preVisitor,
                                                            parentEtci,
                                                            topEtci,
                                                            disabledTarget);
        if (parentEtci && preVisitor.mCanHandle) {
          EventTargetChainItem* item =
            EventTargetChainItem::GetFirstCanHandleEventTarget(chain);
          item->SetNewTarget(parentTarget);
          topEtci = parentEtci;
          continue;
        }
        break;
      }
    }
    if (NS_SUCCEEDED(rv)) {
      if (aTargets) {
        aTargets->Clear();
        uint32_t numTargets = chain.Length();
        EventTarget** targets = aTargets->AppendElements(numTargets);
        for (uint32_t i = 0; i < numTargets; ++i) {
          targets[i] = chain[i].CurrentTarget()->GetTargetForDOMEvent();
        }
      } else {
        // Event target chain is created. PreHandle the chain.
        for (uint32_t i = 0; i < chain.Length(); ++i) {
          chain[i].PreHandleEvent(preVisitor);
        }
        // Handle the chain.
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
    WidgetEvent* innerEvent = aDOMEvent->WidgetEventPtr();
    NS_ENSURE_TRUE(innerEvent, NS_ERROR_ILLEGAL_VALUE);

    bool dontResetTrusted = false;
    if (innerEvent->mFlags.mDispatchedAtLeastOnce) {
      innerEvent->mTarget = nullptr;
      innerEvent->mOriginalTarget = nullptr;
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

#define LOG_EVENT_CREATION(name) mozilla::Telemetry::Accumulate( \
    mozilla::Telemetry::CREATE_EVENT_##name, true);

  if (aEventType.LowerCaseEqualsLiteral("mouseevent")) {
    LOG_EVENT_CREATION(MOUSEEVENT);
    return NS_NewDOMMouseEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("mouseevents")) {
    LOG_EVENT_CREATION(MOUSEEVENTS);
    return NS_NewDOMMouseEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("popupevents")) {
    LOG_EVENT_CREATION(POPUPEVENTS);
    return NS_NewDOMMouseEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("mousescrollevents")) {
    LOG_EVENT_CREATION(MOUSESCROLLEVENTS);
    return NS_NewDOMMouseScrollEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("dragevent")) {
    LOG_EVENT_CREATION(DRAGEVENT);
    return NS_NewDOMDragEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("dragevents")) {
    LOG_EVENT_CREATION(DRAGEVENTS);
    return NS_NewDOMDragEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("keyboardevent")) {
    LOG_EVENT_CREATION(KEYBOARDEVENT);
    return NS_NewDOMKeyboardEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("keyevents")) {
    LOG_EVENT_CREATION(KEYEVENTS);
    return NS_NewDOMKeyboardEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("compositionevent")) {
    LOG_EVENT_CREATION(COMPOSITIONEVENT);
    return NS_NewDOMCompositionEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("textevent")) {
    LOG_EVENT_CREATION(TEXTEVENT);
    return NS_NewDOMCompositionEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("textevents")) {
    LOG_EVENT_CREATION(TEXTEVENTS);
    return NS_NewDOMCompositionEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("mutationevent")) {
    LOG_EVENT_CREATION(MUTATIONEVENT);
    return NS_NewDOMMutationEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("mutationevents")) {
    LOG_EVENT_CREATION(MUTATIONEVENTS);
    return NS_NewDOMMutationEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("deviceorientationevent")) {
    LOG_EVENT_CREATION(DEVICEORIENTATIONEVENT);
    DeviceOrientationEventInit init;
    RefPtr<Event> event =
      DeviceOrientationEvent::Constructor(aOwner, EmptyString(), init);
    event->MarkUninitialized();
    return event.forget();
  }
  if (aEventType.LowerCaseEqualsLiteral("devicemotionevent")) {
    LOG_EVENT_CREATION(DEVICEMOTIONEVENT);
    return NS_NewDOMDeviceMotionEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("uievent")) {
    LOG_EVENT_CREATION(UIEVENT);
    return NS_NewDOMUIEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("uievents")) {
    LOG_EVENT_CREATION(UIEVENTS);
    return NS_NewDOMUIEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("event")) {
    LOG_EVENT_CREATION(EVENT);
    return NS_NewDOMEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("events")) {
    LOG_EVENT_CREATION(EVENTS);
    return NS_NewDOMEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("htmlevents")) {
    LOG_EVENT_CREATION(HTMLEVENTS);
    return NS_NewDOMEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("svgevent")) {
    LOG_EVENT_CREATION(SVGEVENT);
    return NS_NewDOMEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("svgevents")) {
    LOG_EVENT_CREATION(SVGEVENTS);
    return NS_NewDOMEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("svgzoomevent")) {
    LOG_EVENT_CREATION(SVGZOOMEVENT);
    return NS_NewDOMSVGZoomEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("svgzoomevents")) {
    LOG_EVENT_CREATION(SVGZOOMEVENTS);
    return NS_NewDOMSVGZoomEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("timeevent")) {
    LOG_EVENT_CREATION(TIMEEVENT);
    return NS_NewDOMTimeEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("timeevents")) {
    LOG_EVENT_CREATION(TIMEEVENTS);
    return NS_NewDOMTimeEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("xulcommandevent")) {
    LOG_EVENT_CREATION(XULCOMMANDEVENT);
    return NS_NewDOMXULCommandEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("xulcommandevents")) {
    LOG_EVENT_CREATION(XULCOMMANDEVENTS);
    return NS_NewDOMXULCommandEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("commandevent")) {
    LOG_EVENT_CREATION(COMMANDEVENT);
    return NS_NewDOMCommandEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("commandevents")) {
    LOG_EVENT_CREATION(COMMANDEVENTS);
    return NS_NewDOMCommandEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("datacontainerevent")) {
    LOG_EVENT_CREATION(DATACONTAINEREVENT);
    return NS_NewDOMDataContainerEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("datacontainerevents")) {
    LOG_EVENT_CREATION(DATACONTAINEREVENTS);
    return NS_NewDOMDataContainerEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("messageevent")) {
    LOG_EVENT_CREATION(MESSAGEEVENT);
    RefPtr<Event> event = new MessageEvent(aOwner, aPresContext, nullptr);
    return event.forget();
  }
  if (aEventType.LowerCaseEqualsLiteral("notifypaintevent")) {
    LOG_EVENT_CREATION(NOTIFYPAINTEVENT);
    return NS_NewDOMNotifyPaintEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("simplegestureevent")) {
    LOG_EVENT_CREATION(SIMPLEGESTUREEVENT);
    return NS_NewDOMSimpleGestureEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("beforeunloadevent")) {
    LOG_EVENT_CREATION(BEFOREUNLOADEVENT);
    return NS_NewDOMBeforeUnloadEvent(aOwner, aPresContext, nullptr);
  }
  // XXXkhuey this is broken
  if (aEventType.LowerCaseEqualsLiteral("pagetransition")) {
    LOG_EVENT_CREATION(PAGETRANSITION);
    PageTransitionEventInit init;
    RefPtr<Event> event =
      PageTransitionEvent::Constructor(aOwner, EmptyString(), init);
    event->MarkUninitialized();
    return event.forget();
  }
  if (aEventType.LowerCaseEqualsLiteral("scrollareaevent")) {
    LOG_EVENT_CREATION(SCROLLAREAEVENT);
    return NS_NewDOMScrollAreaEvent(aOwner, aPresContext, nullptr);
  }
  // XXXkhuey Chrome supports popstateevent here, even though it provides no
  // initPopStateEvent method.  This is nuts ... but copying it is unlikely to
  // break the web.
  if (aEventType.LowerCaseEqualsLiteral("popstateevent")) {
    LOG_EVENT_CREATION(POPSTATEEVENT);
    AutoJSContext cx;
    RootedDictionary<PopStateEventInit> init(cx);
    RefPtr<Event> event =
      PopStateEvent::Constructor(aOwner, EmptyString(), init);
    event->MarkUninitialized();
    return event.forget();
  }
  if (aEventType.LowerCaseEqualsLiteral("touchevent") &&
      TouchEvent::PrefEnabled(nsContentUtils::GetDocShellForEventTarget(aOwner))) {
    LOG_EVENT_CREATION(TOUCHEVENT);
    return NS_NewDOMTouchEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("hashchangeevent")) {
    LOG_EVENT_CREATION(HASHCHANGEEVENT);
    HashChangeEventInit init;
    RefPtr<Event> event =
      HashChangeEvent::Constructor(aOwner, EmptyString(), init);
    event->MarkUninitialized();
    return event.forget();
  }
  if (aEventType.LowerCaseEqualsLiteral("customevent")) {
    LOG_EVENT_CREATION(CUSTOMEVENT);
    return NS_NewDOMCustomEvent(aOwner, aPresContext, nullptr);
  }
  if (aEventType.LowerCaseEqualsLiteral("storageevent")) {
    LOG_EVENT_CREATION(STORAGEEVENT);
    return NS_NewDOMStorageEvent(aOwner);
  }

#undef LOG_EVENT_CREATION

  // NEW EVENT TYPES SHOULD NOT BE ADDED HERE; THEY SHOULD USE ONLY EVENT
  // CONSTRUCTORS

  return nullptr;
}

} // namespace mozilla
