/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZILLA_INTERNAL_API
#ifndef mozilla_EventDispatcher_h_
#define mozilla_EventDispatcher_h_

#include "mozilla/EventForwards.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

// Microsoft's API Name hackery sucks
#undef CreateEvent

class nsIContent;
class nsIDOMEvent;
class nsIScriptGlobalObject;
class nsPresContext;

template<class E> class nsCOMArray;

namespace mozilla {
namespace dom {
class EventTarget;
} // namespace dom

/**
 * About event dispatching:
 * When either EventDispatcher::Dispatch or
 * EventDispatcher::DispatchDOMEvent is called an event target chain is
 * created. EventDispatcher creates the chain by calling PreHandleEvent 
 * on each event target and the creation continues until either the mCanHandle
 * member of the EventChainPreVisitor object is false or the mParentTarget
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

class EventChainVisitor
{
public:
  EventChainVisitor(nsPresContext* aPresContext,
                    WidgetEvent* aEvent,
                    nsIDOMEvent* aDOMEvent,
                    nsEventStatus aEventStatus = nsEventStatus_eIgnore)
    : mPresContext(aPresContext)
    , mEvent(aEvent)
    , mDOMEvent(aDOMEvent)
    , mEventStatus(aEventStatus)
    , mItemFlags(0)
  {
  }

  /**
   * The prescontext, possibly nullptr.
   */
  nsPresContext* const  mPresContext;

  /**
   * The WidgetEvent which is being dispatched. Never nullptr.
   */
  WidgetEvent* const mEvent;

  /**
   * The DOM Event assiciated with the mEvent. Possibly nullptr if a DOM Event
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
   * @note Using uint16_t because that is used also in EventTargetChainItem.
   */
  uint16_t              mItemFlags;

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

class EventChainPreVisitor : public EventChainVisitor
{
public:
  EventChainPreVisitor(nsPresContext* aPresContext,
                       WidgetEvent* aEvent,
                       nsIDOMEvent* aDOMEvent,
                       nsEventStatus aEventStatus,
                       bool aIsInAnon)
    : EventChainVisitor(aPresContext, aEvent, aDOMEvent, aEventStatus)
    , mCanHandle(true)
    , mAutomaticChromeDispatch(true)
    , mForceContentDispatch(false)
    , mRelatedTargetIsInAnon(false)
    , mOriginalTargetIsInAnon(aIsInAnon)
    , mWantsWillHandleEvent(false)
    , mMayHaveListenerManager(true)
    , mParentTarget(nullptr)
    , mEventTargetAtParent(nullptr)
  {
  }

  void Reset()
  {
    mItemFlags = 0;
    mItemData = nullptr;
    mCanHandle = true;
    mAutomaticChromeDispatch = true;
    mForceContentDispatch = false;
    mWantsWillHandleEvent = false;
    mMayHaveListenerManager = true;
    mParentTarget = nullptr;
    mEventTargetAtParent = nullptr;
  }

  /**
   * Member that must be set in PreHandleEvent by event targets. If set to false,
   * indicates that this event target will not be handling the event and
   * construction of the event target chain is complete. The target that sets
   * mCanHandle to false is NOT included in the event target chain.
   */
  bool                  mCanHandle;

  /**
   * If mCanHandle is false and mAutomaticChromeDispatch is also false
   * event will not be dispatched to the chrome event handler.
   */
  bool                  mAutomaticChromeDispatch;

  /**
   * If mForceContentDispatch is set to true,
   * content dispatching is not disabled for this event target.
   * FIXME! This is here for backward compatibility. Bug 329119
   */
  bool                  mForceContentDispatch;

  /**
   * true if it is known that related target is or is a descendant of an
   * element which is anonymous for events.
   */
  bool                  mRelatedTargetIsInAnon;

  /**
   * true if the original target of the event is inside anonymous content.
   * This is set before calling PreHandleEvent on event targets.
   */
  bool                  mOriginalTargetIsInAnon;

  /**
   * Whether or not nsIDOMEventTarget::WillHandleEvent will be
   * called. Default is false;
   */
  bool                  mWantsWillHandleEvent;

  /**
   * If it is known that the current target doesn't have a listener manager
   * when PreHandleEvent is called, set this to false.
   */
  bool                  mMayHaveListenerManager;

  /**
   * Parent item in the event target chain.
   */
  dom::EventTarget* mParentTarget;

  /**
   * If the event needs to be retargeted, this is the event target,
   * which should be used when the event is handled at mParentTarget.
   */
  dom::EventTarget* mEventTargetAtParent;

  /**
   * An array of destination insertion points that need to be inserted
   * into the event path of nodes that are distributed by the
   * web components distribution algorithm.
   */
  nsTArray<nsIContent*> mDestInsertionPoints;
};

class EventChainPostVisitor : public mozilla::EventChainVisitor
{
public:
  explicit EventChainPostVisitor(EventChainVisitor& aOther)
    : EventChainVisitor(aOther.mPresContext, aOther.mEvent,
                        aOther.mDOMEvent, aOther.mEventStatus)
  {
  }
};

/**
 * If an EventDispatchingCallback object is passed to Dispatch,
 * its HandleEvent method is called after handling the default event group,
 * before handling the system event group.
 * This is used in nsPresShell.
 */
class MOZ_STACK_CLASS EventDispatchingCallback
{
public:
  virtual void HandleEvent(EventChainPostVisitor& aVisitor) = 0;
};

/**
 * The generic class for event dispatching.
 * Must not be used outside Gecko!
 */
class EventDispatcher
{
public:
  /**
   * aTarget should QI to EventTarget.
   * If the target of aEvent is set before calling this method, the target of 
   * aEvent is used as the target (unless there is event
   * retargeting) and the originalTarget of the DOM Event.
   * aTarget is always used as the starting point for constructing the event
   * target chain, no matter what the value of aEvent->target is.
   * In other words, aEvent->target is only a property of the event and it has
   * nothing to do with the construction of the event target chain.
   * Neither aTarget nor aEvent is allowed to be nullptr.
   *
   * If aTargets is non-null, event target chain will be created, but
   * event won't be handled. In this case aEvent->message should be
   * NS_EVENT_NULL.
   * @note Use this method when dispatching a WidgetEvent.
   */
  static nsresult Dispatch(nsISupports* aTarget,
                           nsPresContext* aPresContext,
                           WidgetEvent* aEvent,
                           nsIDOMEvent* aDOMEvent = nullptr,
                           nsEventStatus* aEventStatus = nullptr,
                           EventDispatchingCallback* aCallback = nullptr,
                           nsCOMArray<dom::EventTarget>* aTargets = nullptr);

  /**
   * Dispatches an event.
   * If aDOMEvent is not nullptr, it is used for dispatching
   * (aEvent can then be nullptr) and (if aDOMEvent is not |trusted| already),
   * the |trusted| flag is set based on the UniversalXPConnect capability.
   * Otherwise this works like EventDispatcher::Dispatch.
   * @note Use this method when dispatching nsIDOMEvent.
   */
  static nsresult DispatchDOMEvent(nsISupports* aTarget,
                                   WidgetEvent* aEvent,
                                   nsIDOMEvent* aDOMEvent,
                                   nsPresContext* aPresContext,
                                   nsEventStatus* aEventStatus);

  /**
   * Creates a DOM Event.
   */
  static nsresult CreateEvent(dom::EventTarget* aOwner,
                              nsPresContext* aPresContext,
                              WidgetEvent* aEvent,
                              const nsAString& aEventType,
                              nsIDOMEvent** aDOMEvent);

  /**
   * Called at shutting down.
   */
  static void Shutdown();
};

} // namespace mozilla

#endif // mozilla_EventDispatcher_h_
#endif
