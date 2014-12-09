/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EventListenerManager_h_
#define mozilla_EventListenerManager_h_

#include "mozilla/BasicEvents.h"
#include "mozilla/dom/EventListenerBinding.h"
#include "mozilla/JSEventHandler.h"
#include "mozilla/MemoryReporting.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGkAtoms.h"
#include "nsIDOMEventListener.h"
#include "nsTObserverArray.h"

class nsIDocShell;
class nsIDOMEvent;
class nsIEventListenerInfo;
class nsIScriptContext;
class nsPIDOMWindow;
class JSTracer;

struct EventTypeData;

template<class T> class nsCOMArray;

namespace mozilla {

class ELMCreationDetector;
class EventListenerManager;

namespace dom {
class EventTarget;
class Element;
} // namespace dom

typedef dom::CallbackObjectHolder<dom::EventListener,
                                  nsIDOMEventListener> EventListenerHolder;

struct EventListenerFlags
{
  friend class EventListenerManager;
private:
  // If mListenerIsJSListener is true, the listener is implemented by JS.
  // Otherwise, it's implemented by native code or JS but it's wrapped.
  bool mListenerIsJSListener : 1;

public:
  // If mCapture is true, it means the listener captures the event.  Otherwise,
  // it's listening at bubbling phase.
  bool mCapture : 1;
  // If mInSystemGroup is true, the listener is listening to the events in the
  // system group.
  bool mInSystemGroup : 1;
  // If mAllowUntrustedEvents is true, the listener is listening to the
  // untrusted events too.
  bool mAllowUntrustedEvents : 1;

  EventListenerFlags() :
    mListenerIsJSListener(false),
    mCapture(false), mInSystemGroup(false), mAllowUntrustedEvents(false)
  {
  }

  bool Equals(const EventListenerFlags& aOther) const
  {
    return (mCapture == aOther.mCapture &&
            mInSystemGroup == aOther.mInSystemGroup &&
            mListenerIsJSListener == aOther.mListenerIsJSListener &&
            mAllowUntrustedEvents == aOther.mAllowUntrustedEvents);
  }

  bool EqualsIgnoringTrustness(const EventListenerFlags& aOther) const
  {
    return (mCapture == aOther.mCapture &&
            mInSystemGroup == aOther.mInSystemGroup &&
            mListenerIsJSListener == aOther.mListenerIsJSListener);
  }

  bool operator==(const EventListenerFlags& aOther) const
  {
    return Equals(aOther);
  }
};

inline EventListenerFlags TrustedEventsAtBubble()
{
  EventListenerFlags flags;
  return flags;
}

inline EventListenerFlags TrustedEventsAtCapture()
{
  EventListenerFlags flags;
  flags.mCapture = true;
  return flags;
}

inline EventListenerFlags AllEventsAtBubbe()
{
  EventListenerFlags flags;
  flags.mAllowUntrustedEvents = true;
  return flags;
}

inline EventListenerFlags AllEventsAtCapture()
{
  EventListenerFlags flags;
  flags.mCapture = true;
  flags.mAllowUntrustedEvents = true;
  return flags;
}

inline EventListenerFlags TrustedEventsAtSystemGroupBubble()
{
  EventListenerFlags flags;
  flags.mInSystemGroup = true;
  return flags;
}

inline EventListenerFlags TrustedEventsAtSystemGroupCapture()
{
  EventListenerFlags flags;
  flags.mCapture = true;
  flags.mInSystemGroup = true;
  return flags;
}

inline EventListenerFlags AllEventsAtSystemGroupBubble()
{
  EventListenerFlags flags;
  flags.mInSystemGroup = true;
  flags.mAllowUntrustedEvents = true;
  return flags;
}

inline EventListenerFlags AllEventsAtSystemGroupCapture()
{
  EventListenerFlags flags;
  flags.mCapture = true;
  flags.mInSystemGroup = true;
  flags.mAllowUntrustedEvents = true;
  return flags;
}

/*
 * Event listener manager
 */

class EventListenerManager MOZ_FINAL
{
  ~EventListenerManager();

public:
  struct Listener
  {
    EventListenerHolder mListener;
    nsCOMPtr<nsIAtom> mTypeAtom; // for the main thread
    nsString mTypeString; // for non-main-threads
    uint16_t mEventType;

    enum ListenerType MOZ_ENUM_TYPE(uint8_t)
    {
      eNativeListener = 0,
      eJSEventListener,
      eWrappedJSListener,
      eWebIDLListener,
      eListenerTypeCount
    };
    uint8_t mListenerType;

    bool mListenerIsHandler : 1;
    bool mHandlerIsString : 1;
    bool mAllEvents : 1;

    EventListenerFlags mFlags;

    JSEventHandler* GetJSEventHandler() const
    {
      return (mListenerType == eJSEventListener) ?
        static_cast<JSEventHandler*>(mListener.GetXPCOMCallback()) :
        nullptr;
    }

    Listener()
    {
      MOZ_ASSERT(sizeof(mListenerType) == 1);
      MOZ_ASSERT(eListenerTypeCount < 255);
    }

    ~Listener()
    {
      if ((mListenerType == eJSEventListener) && mListener) {
        static_cast<JSEventHandler*>(
          mListener.GetXPCOMCallback())->Disconnect();
      }
    }

    MOZ_ALWAYS_INLINE bool IsListening(const WidgetEvent* aEvent) const
    {
      if (mFlags.mInSystemGroup != aEvent->mFlags.mInSystemGroup) {
        return false;
      }
      // FIXME Should check !mFlags.mCapture when the event is in target
      //       phase because capture phase event listeners should not be fired.
      //       But it breaks at least <xul:dialog>'s buttons. Bug 235441.
      return ((mFlags.mCapture && aEvent->mFlags.mInCapturePhase) ||
              (!mFlags.mCapture && aEvent->mFlags.mInBubblingPhase));
    }
  };

  explicit EventListenerManager(dom::EventTarget* aTarget);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(EventListenerManager)

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(EventListenerManager)

  void AddEventListener(const nsAString& aType,
                        nsIDOMEventListener* aListener,
                        bool aUseCapture,
                        bool aWantsUntrusted)
  {
    EventListenerHolder holder(aListener);
    AddEventListener(aType, holder, aUseCapture, aWantsUntrusted);
  }
  void AddEventListener(const nsAString& aType,
                        dom::EventListener* aListener,
                        bool aUseCapture,
                        bool aWantsUntrusted)
  {
    EventListenerHolder holder(aListener);
    AddEventListener(aType, holder, aUseCapture, aWantsUntrusted);
  }
  void RemoveEventListener(const nsAString& aType,
                           nsIDOMEventListener* aListener,
                           bool aUseCapture)
  {
    EventListenerHolder holder(aListener);
    RemoveEventListener(aType, holder, aUseCapture);
  }
  void RemoveEventListener(const nsAString& aType,
                           dom::EventListener* aListener,
                           bool aUseCapture)
  {
    EventListenerHolder holder(aListener);
    RemoveEventListener(aType, holder, aUseCapture);
  }

  void AddListenerForAllEvents(nsIDOMEventListener* aListener,
                               bool aUseCapture,
                               bool aWantsUntrusted,
                               bool aSystemEventGroup);
  void RemoveListenerForAllEvents(nsIDOMEventListener* aListener,
                                  bool aUseCapture,
                                  bool aSystemEventGroup);

  /**
  * Sets events listeners of all types. 
  * @param an event listener
  */
  void AddEventListenerByType(nsIDOMEventListener *aListener,
                              const nsAString& type,
                              const EventListenerFlags& aFlags)
  {
    EventListenerHolder holder(aListener);
    AddEventListenerByType(holder, type, aFlags);
  }
  void AddEventListenerByType(const EventListenerHolder& aListener,
                              const nsAString& type,
                              const EventListenerFlags& aFlags);
  void RemoveEventListenerByType(nsIDOMEventListener *aListener,
                                 const nsAString& type,
                                 const EventListenerFlags& aFlags)
  {
    EventListenerHolder holder(aListener);
    RemoveEventListenerByType(holder, type, aFlags);
  }
  void RemoveEventListenerByType(const EventListenerHolder& aListener,
                                 const nsAString& type,
                                 const EventListenerFlags& aFlags);

  /**
   * Sets the current "inline" event listener for aName to be a
   * function compiled from aFunc if !aDeferCompilation.  If
   * aDeferCompilation, then we assume that we can get the string from
   * mTarget later and compile lazily.
   *
   * aElement, if not null, is the element the string is associated with.
   */
  // XXXbz does that play correctly with nodes being adopted across
  // documents?  Need to double-check the spec here.
  nsresult SetEventHandler(nsIAtom *aName,
                           const nsAString& aFunc,
                           bool aDeferCompilation,
                           bool aPermitUntrustedEvents,
                           dom::Element* aElement);
  /**
   * Remove the current "inline" event listener for aName.
   */
  void RemoveEventHandler(nsIAtom *aName, const nsAString& aTypeString);

  void HandleEvent(nsPresContext* aPresContext,
                   WidgetEvent* aEvent, 
                   nsIDOMEvent** aDOMEvent,
                   dom::EventTarget* aCurrentTarget,
                   nsEventStatus* aEventStatus)
  {
    if (mListeners.IsEmpty() || aEvent->mFlags.mPropagationStopped) {
      return;
    }

    if (!mMayHaveCapturingListeners && !aEvent->mFlags.mInBubblingPhase) {
      return;
    }

    if (!mMayHaveSystemGroupListeners && aEvent->mFlags.mInSystemGroup) {
      return;
    }

    // Check if we already know that there is no event listener for the event.
    if (mNoListenerForEvent == aEvent->message &&
        (mNoListenerForEvent != NS_USER_DEFINED_EVENT ||
         mNoListenerForEventAtom == aEvent->userType)) {
      return;
    }
    HandleEventInternal(aPresContext, aEvent, aDOMEvent, aCurrentTarget,
                        aEventStatus);
  }

  /**
   * Tells the event listener manager that its target (which owns it) is
   * no longer using it (and could go away).
   */
  void Disconnect();

  /**
   * Allows us to quickly determine if we have mutation listeners registered.
   */
  bool HasMutationListeners();

  /**
   * Allows us to quickly determine whether we have unload or beforeunload
   * listeners registered.
   */
  bool HasUnloadListeners();

  /**
   * Returns the mutation bits depending on which mutation listeners are
   * registered to this listener manager.
   * @note If a listener is an nsIDOMMutationListener, all possible mutation
   *       event bits are returned. All bits are also returned if one of the
   *       event listeners is registered to handle DOMSubtreeModified events.
   */
  uint32_t MutationListenerBits();

  /**
   * Returns true if there is at least one event listener for aEventName.
   */
  bool HasListenersFor(const nsAString& aEventName);

  /**
   * Returns true if there is at least one event listener for aEventNameWithOn.
   * Note that aEventNameWithOn must start with "on"!
   */
  bool HasListenersFor(nsIAtom* aEventNameWithOn);

  /**
   * Returns true if there is at least one event listener.
   */
  bool HasListeners();

  /**
   * Sets aList to the list of nsIEventListenerInfo objects representing the
   * listeners managed by this listener manager.
   */
  nsresult GetListenerInfo(nsCOMArray<nsIEventListenerInfo>* aList);

  uint32_t GetIdentifierForEvent(nsIAtom* aEvent);

  static void Shutdown();

  /**
   * Returns true if there may be a paint event listener registered,
   * false if there definitely isn't.
   */
  bool MayHavePaintEventListener() { return mMayHavePaintEventListener; }

  /**
   * Returns true if there may be a touch event listener registered,
   * false if there definitely isn't.
   */
  bool MayHaveTouchEventListener() { return mMayHaveTouchEventListener; }

  /**
   * Returns true if there may be a scroll wheel listener registered,
   * false if there definitely isn't.
   */
  bool MayHaveScrollWheelEventListener() { return mMayHaveScrollWheelEventListener; }

  bool MayHaveMouseEnterLeaveEventListener() { return mMayHaveMouseEnterLeaveEventListener; }
  bool MayHavePointerEnterLeaveEventListener() { return mMayHavePointerEnterLeaveEventListener; }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  uint32_t ListenerCount() const
  {
    return mListeners.Length();
  }

  void MarkForCC();

  void TraceListeners(JSTracer* aTrc);

  dom::EventTarget* GetTarget() { return mTarget; }

protected:
  void HandleEventInternal(nsPresContext* aPresContext,
                           WidgetEvent* aEvent,
                           nsIDOMEvent** aDOMEvent,
                           dom::EventTarget* aCurrentTarget,
                           nsEventStatus* aEventStatus);

  nsresult HandleEventSubType(Listener* aListener,
                              nsIDOMEvent* aDOMEvent,
                              dom::EventTarget* aCurrentTarget);

  nsIDocShell* GetDocShellForTarget();

  /**
   * Compile the "inline" event listener for aListener.  The
   * body of the listener can be provided in aBody; if this is null we
   * will look for it on mTarget.  If aBody is provided, aElement should be
   * as well; otherwise it will also be inferred from mTarget.
   */
  nsresult CompileEventHandlerInternal(Listener* aListener,
                                       const nsAString* aBody,
                                       dom::Element* aElement);

  /**
   * Find the Listener for the "inline" event listener for aTypeAtom.
   */
  Listener* FindEventHandler(uint32_t aEventType,
                             nsIAtom* aTypeAtom,
                             const nsAString& aTypeString);

  /**
   * Set the "inline" event listener for aName to aHandler.  aHandler may be
   * have no actual handler set to indicate that we should lazily get and
   * compile the string for this listener, but in that case aContext and
   * aScopeGlobal must be non-null.  Otherwise, aContext and aScopeGlobal are
   * allowed to be null.
   */
  Listener* SetEventHandlerInternal(nsIAtom* aName,
                                    const nsAString& aTypeString,
                                    const TypedEventHandler& aHandler,
                                    bool aPermitUntrustedEvents);

  bool IsDeviceType(uint32_t aType);
  void EnableDevice(uint32_t aType);
  void DisableDevice(uint32_t aType);

public:
  /**
   * Set the "inline" event listener for aEventName to aHandler.  If
   * aHandler is null, this will actually remove the event listener
   */
  void SetEventHandler(nsIAtom* aEventName,
                       const nsAString& aTypeString,
                       dom::EventHandlerNonNull* aHandler);
  void SetEventHandler(dom::OnErrorEventHandlerNonNull* aHandler);
  void SetEventHandler(dom::OnBeforeUnloadEventHandlerNonNull* aHandler);

  /**
   * Get the value of the "inline" event listener for aEventName.
   * This may cause lazy compilation if the listener is uncompiled.
   *
   * Note: It's the caller's responsibility to make sure to call the right one
   * of these methods.  In particular, "onerror" events use
   * OnErrorEventHandlerNonNull for some event targets and EventHandlerNonNull
   * for others.
   */
  dom::EventHandlerNonNull* GetEventHandler(nsIAtom* aEventName,
                                            const nsAString& aTypeString)
  {
    const TypedEventHandler* typedHandler =
      GetTypedEventHandler(aEventName, aTypeString);
    return typedHandler ? typedHandler->NormalEventHandler() : nullptr;
  }

  dom::OnErrorEventHandlerNonNull* GetOnErrorEventHandler()
  {
    const TypedEventHandler* typedHandler = mIsMainThreadELM ?
      GetTypedEventHandler(nsGkAtoms::onerror, EmptyString()) :
      GetTypedEventHandler(nullptr, NS_LITERAL_STRING("error"));
    return typedHandler ? typedHandler->OnErrorEventHandler() : nullptr;
  }

  dom::OnBeforeUnloadEventHandlerNonNull* GetOnBeforeUnloadEventHandler()
  {
    const TypedEventHandler* typedHandler =
      GetTypedEventHandler(nsGkAtoms::onbeforeunload, EmptyString());
    return typedHandler ? typedHandler->OnBeforeUnloadEventHandler() : nullptr;
  }

protected:
  /**
   * Helper method for implementing the various Get*EventHandler above.  Will
   * return null if we don't have an event handler for this event name.
   */
  const TypedEventHandler* GetTypedEventHandler(nsIAtom* aEventName,
                                                const nsAString& aTypeString);

  void AddEventListener(const nsAString& aType,
                        const EventListenerHolder& aListener,
                        bool aUseCapture,
                        bool aWantsUntrusted);
  void RemoveEventListener(const nsAString& aType,
                           const EventListenerHolder& aListener,
                           bool aUseCapture);

  void AddEventListenerInternal(const EventListenerHolder& aListener,
                                uint32_t aType,
                                nsIAtom* aTypeAtom,
                                const nsAString& aTypeString,
                                const EventListenerFlags& aFlags,
                                bool aHandler = false,
                                bool aAllEvents = false);
  void RemoveEventListenerInternal(const EventListenerHolder& aListener,
                                   uint32_t aType,
                                   nsIAtom* aUserType,
                                   const nsAString& aTypeString,
                                   const EventListenerFlags& aFlags,
                                   bool aAllEvents = false);
  void RemoveAllListeners();
  const EventTypeData* GetTypeDataForIID(const nsIID& aIID);
  const EventTypeData* GetTypeDataForEventName(nsIAtom* aName);
  nsPIDOMWindow* GetInnerWindowForTarget();
  already_AddRefed<nsPIDOMWindow> GetTargetAsInnerWindow() const;

  bool ListenerCanHandle(Listener* aListener, WidgetEvent* aEvent);

  already_AddRefed<nsIScriptGlobalObject>
  GetScriptGlobalAndDocument(nsIDocument** aDoc);

  uint32_t mMayHavePaintEventListener : 1;
  uint32_t mMayHaveMutationListeners : 1;
  uint32_t mMayHaveCapturingListeners : 1;
  uint32_t mMayHaveSystemGroupListeners : 1;
  uint32_t mMayHaveTouchEventListener : 1;
  uint32_t mMayHaveScrollWheelEventListener : 1;
  uint32_t mMayHaveMouseEnterLeaveEventListener : 1;
  uint32_t mMayHavePointerEnterLeaveEventListener : 1;
  uint32_t mClearingListeners : 1;
  uint32_t mIsMainThreadELM : 1;
  uint32_t mNoListenerForEvent : 23;

  nsAutoTObserverArray<Listener, 2> mListeners;
  dom::EventTarget* mTarget;  // WEAK
  nsCOMPtr<nsIAtom> mNoListenerForEventAtom;

  friend class ELMCreationDetector;
  static uint32_t sMainThreadCreatedCount;
};

} // namespace mozilla

/**
 * NS_AddSystemEventListener() is a helper function for implementing
 * EventTarget::AddSystemEventListener().
 */
inline nsresult
NS_AddSystemEventListener(mozilla::dom::EventTarget* aTarget,
                          const nsAString& aType,
                          nsIDOMEventListener *aListener,
                          bool aUseCapture,
                          bool aWantsUntrusted)
{
  mozilla::EventListenerManager* listenerManager =
    aTarget->GetOrCreateListenerManager();
  NS_ENSURE_STATE(listenerManager);
  mozilla::EventListenerFlags flags;
  flags.mInSystemGroup = true;
  flags.mCapture = aUseCapture;
  flags.mAllowUntrustedEvents = aWantsUntrusted;
  listenerManager->AddEventListenerByType(aListener, aType, flags);
  return NS_OK;
}

#endif // mozilla_EventListenerManager_h_
