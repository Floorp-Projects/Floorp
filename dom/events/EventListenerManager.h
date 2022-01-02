/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EventListenerManager_h_
#define mozilla_EventListenerManager_h_

#include "mozilla/BasicEvents.h"
#include "mozilla/dom/AbortFollower.h"
#include "mozilla/dom/EventListenerBinding.h"
#include "mozilla/JSEventHandler.h"
#include "mozilla/MemoryReporting.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGkAtoms.h"
#include "nsIDOMEventListener.h"
#include "nsTObserverArray.h"
#include "nsTArray.h"

class nsIEventListenerInfo;
class nsPIDOMWindowInner;
class JSTracer;

struct EventTypeData;

namespace mozilla {

class ELMCreationDetector;
class EventListenerManager;
class ListenerSignalFollower;

namespace dom {
class Event;
class EventTarget;
class Element;
}  // namespace dom

using EventListenerHolder =
    dom::CallbackObjectHolder<dom::EventListener, nsIDOMEventListener>;

struct EventListenerFlags {
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
  // If mPassive is true, the listener will not be calling preventDefault on the
  // event. (If it does call preventDefault, we should ignore it).
  bool mPassive : 1;
  // If mOnce is true, the listener will be removed from the manager before it
  // is invoked, so that it would only be invoked once.
  bool mOnce : 1;

  EventListenerFlags()
      : mListenerIsJSListener(false),
        mCapture(false),
        mInSystemGroup(false),
        mAllowUntrustedEvents(false),
        mPassive(false),
        mOnce(false) {}

  bool EqualsForAddition(const EventListenerFlags& aOther) const {
    return (mCapture == aOther.mCapture &&
            mInSystemGroup == aOther.mInSystemGroup &&
            mListenerIsJSListener == aOther.mListenerIsJSListener &&
            mAllowUntrustedEvents == aOther.mAllowUntrustedEvents);
    // Don't compare mPassive or mOnce
  }

  bool EqualsForRemoval(const EventListenerFlags& aOther) const {
    return (mCapture == aOther.mCapture &&
            mInSystemGroup == aOther.mInSystemGroup &&
            mListenerIsJSListener == aOther.mListenerIsJSListener);
    // Don't compare mAllowUntrustedEvents, mPassive, or mOnce
  }
};

inline EventListenerFlags TrustedEventsAtBubble() {
  EventListenerFlags flags;
  return flags;
}

inline EventListenerFlags TrustedEventsAtCapture() {
  EventListenerFlags flags;
  flags.mCapture = true;
  return flags;
}

inline EventListenerFlags AllEventsAtBubble() {
  EventListenerFlags flags;
  flags.mAllowUntrustedEvents = true;
  return flags;
}

inline EventListenerFlags AllEventsAtCapture() {
  EventListenerFlags flags;
  flags.mCapture = true;
  flags.mAllowUntrustedEvents = true;
  return flags;
}

inline EventListenerFlags TrustedEventsAtSystemGroupBubble() {
  EventListenerFlags flags;
  flags.mInSystemGroup = true;
  return flags;
}

inline EventListenerFlags TrustedEventsAtSystemGroupCapture() {
  EventListenerFlags flags;
  flags.mCapture = true;
  flags.mInSystemGroup = true;
  return flags;
}

inline EventListenerFlags AllEventsAtSystemGroupBubble() {
  EventListenerFlags flags;
  flags.mInSystemGroup = true;
  flags.mAllowUntrustedEvents = true;
  return flags;
}

inline EventListenerFlags AllEventsAtSystemGroupCapture() {
  EventListenerFlags flags;
  flags.mCapture = true;
  flags.mInSystemGroup = true;
  flags.mAllowUntrustedEvents = true;
  return flags;
}

class EventListenerManagerBase {
 protected:
  EventListenerManagerBase();

  EventMessage mNoListenerForEvent;
  uint16_t mMayHavePaintEventListener : 1;
  uint16_t mMayHaveMutationListeners : 1;
  uint16_t mMayHaveCapturingListeners : 1;
  uint16_t mMayHaveSystemGroupListeners : 1;
  uint16_t mMayHaveTouchEventListener : 1;
  uint16_t mMayHaveMouseEnterLeaveEventListener : 1;
  uint16_t mMayHavePointerEnterLeaveEventListener : 1;
  uint16_t mMayHaveKeyEventListener : 1;
  uint16_t mMayHaveInputOrCompositionEventListener : 1;
  uint16_t mMayHaveSelectionChangeEventListener : 1;
  uint16_t mMayHaveFormSelectEventListener : 1;
  uint16_t mClearingListeners : 1;
  uint16_t mIsMainThreadELM : 1;
  uint16_t mHasNonPrivilegedClickListeners : 1;
  uint16_t mUnknownNonPrivilegedClickListeners : 1;
  // uint16_t mUnused : 1;
};

/*
 * Event listener manager
 */

class EventListenerManager final : public EventListenerManagerBase {
  ~EventListenerManager();

 public:
  struct Listener;
  class ListenerSignalFollower : public dom::AbortFollower {
   public:
    explicit ListenerSignalFollower(EventListenerManager* aListenerManager,
                                    Listener* aListener);

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(ListenerSignalFollower)

    void RunAbortAlgorithm() override;

    void Disconnect() {
      mListenerManager = nullptr;
      mListener.Reset();
      Unfollow();
    }

   protected:
    ~ListenerSignalFollower() = default;

    EventListenerManager* mListenerManager;
    EventListenerHolder mListener;
    RefPtr<nsAtom> mTypeAtom;
    EventMessage mEventMessage;
    bool mAllEvents;
    EventListenerFlags mFlags;
  };

  struct Listener {
    RefPtr<ListenerSignalFollower> mSignalFollower;
    EventListenerHolder mListener;
    RefPtr<nsAtom> mTypeAtom;
    EventMessage mEventMessage;

    enum ListenerType : uint8_t {
      // No listener.
      eNoListener,
      // A generic C++ implementation of nsIDOMEventListener.
      eNativeListener,
      // An event handler attribute using JSEventHandler.
      eJSEventListener,
      // A scripted EventListener.
      eWebIDLListener,
    };
    ListenerType mListenerType;

    bool mListenerIsHandler : 1;
    bool mHandlerIsString : 1;
    bool mAllEvents : 1;
    bool mIsChrome : 1;
    bool mEnabled : 1;

    EventListenerFlags mFlags;

    JSEventHandler* GetJSEventHandler() const {
      return (mListenerType == eJSEventListener)
                 ? static_cast<JSEventHandler*>(mListener.GetXPCOMCallback())
                 : nullptr;
    }

    Listener()
        : mEventMessage(eVoidEvent),
          mListenerType(eNoListener),
          mListenerIsHandler(false),
          mHandlerIsString(false),
          mAllEvents(false),
          mIsChrome(false),
          mEnabled(true) {}

    Listener(Listener&& aOther)
        : mSignalFollower(std::move(aOther.mSignalFollower)),
          mListener(std::move(aOther.mListener)),
          mTypeAtom(std::move(aOther.mTypeAtom)),
          mEventMessage(aOther.mEventMessage),
          mListenerType(aOther.mListenerType),
          mListenerIsHandler(aOther.mListenerIsHandler),
          mHandlerIsString(aOther.mHandlerIsString),
          mAllEvents(aOther.mAllEvents),
          mIsChrome(aOther.mIsChrome),
          mEnabled(aOther.mEnabled) {
      aOther.mEventMessage = eVoidEvent;
      aOther.mListenerType = eNoListener;
      aOther.mListenerIsHandler = false;
      aOther.mHandlerIsString = false;
      aOther.mAllEvents = false;
      aOther.mIsChrome = false;
      aOther.mEnabled = true;
    }

    ~Listener() {
      if ((mListenerType == eJSEventListener) && mListener) {
        static_cast<JSEventHandler*>(mListener.GetXPCOMCallback())
            ->Disconnect();
      }
      if (mSignalFollower) {
        mSignalFollower->Disconnect();
      }
    }

    MOZ_ALWAYS_INLINE bool IsListening(const WidgetEvent* aEvent) const {
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

  void AddEventListener(const nsAString& aType, nsIDOMEventListener* aListener,
                        bool aUseCapture, bool aWantsUntrusted) {
    AddEventListener(aType, EventListenerHolder(aListener), aUseCapture,
                     aWantsUntrusted);
  }
  void AddEventListener(const nsAString& aType, dom::EventListener* aListener,
                        const dom::AddEventListenerOptionsOrBoolean& aOptions,
                        bool aWantsUntrusted) {
    AddEventListener(aType, EventListenerHolder(aListener), aOptions,
                     aWantsUntrusted);
  }
  void RemoveEventListener(const nsAString& aType,
                           nsIDOMEventListener* aListener, bool aUseCapture) {
    RemoveEventListener(aType, EventListenerHolder(aListener), aUseCapture);
  }
  void RemoveEventListener(const nsAString& aType,
                           dom::EventListener* aListener,
                           const dom::EventListenerOptionsOrBoolean& aOptions) {
    RemoveEventListener(aType, EventListenerHolder(aListener), aOptions);
  }

  void AddListenerForAllEvents(dom::EventListener* aListener, bool aUseCapture,
                               bool aWantsUntrusted, bool aSystemEventGroup);
  void RemoveListenerForAllEvents(dom::EventListener* aListener,
                                  bool aUseCapture, bool aSystemEventGroup);

  /**
   * Sets events listeners of all types.
   * @param an event listener
   */
  void AddEventListenerByType(nsIDOMEventListener* aListener,
                              const nsAString& type,
                              const EventListenerFlags& aFlags) {
    AddEventListenerByType(EventListenerHolder(aListener), type, aFlags);
  }
  void AddEventListenerByType(dom::EventListener* aListener,
                              const nsAString& type,
                              const EventListenerFlags& aFlags) {
    AddEventListenerByType(EventListenerHolder(aListener), type, aFlags);
  }
  void AddEventListenerByType(
      EventListenerHolder aListener, const nsAString& type,
      const EventListenerFlags& aFlags,
      const dom::Optional<bool>& aPassive = dom::Optional<bool>(),
      dom::AbortSignal* aSignal = nullptr);
  void RemoveEventListenerByType(nsIDOMEventListener* aListener,
                                 const nsAString& type,
                                 const EventListenerFlags& aFlags) {
    RemoveEventListenerByType(EventListenerHolder(aListener), type, aFlags);
  }
  void RemoveEventListenerByType(dom::EventListener* aListener,
                                 const nsAString& type,
                                 const EventListenerFlags& aFlags) {
    RemoveEventListenerByType(EventListenerHolder(aListener), type, aFlags);
  }
  void RemoveEventListenerByType(EventListenerHolder aListener,
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
  nsresult SetEventHandler(nsAtom* aName, const nsAString& aFunc,
                           bool aDeferCompilation, bool aPermitUntrustedEvents,
                           dom::Element* aElement);
  /**
   * Remove the current "inline" event listener for aName.
   */
  void RemoveEventHandler(nsAtom* aName);

  // We only get called from the event dispatch code, which knows to be careful
  // with what it's doing.  We could annotate ourselves as MOZ_CAN_RUN_SCRIPT,
  // but then the event dispatch code would need a ton of MOZ_KnownLive for
  // things that come from slightly complicated stack-lifetime data structures.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void HandleEvent(nsPresContext* aPresContext, WidgetEvent* aEvent,
                   dom::Event** aDOMEvent, dom::EventTarget* aCurrentTarget,
                   nsEventStatus* aEventStatus, bool aItemInShadowTree) {
    if (mListeners.IsEmpty() || aEvent->PropagationStopped()) {
      return;
    }

    if (!mMayHaveCapturingListeners && !aEvent->mFlags.mInBubblingPhase) {
      return;
    }

    if (!mMayHaveSystemGroupListeners && aEvent->mFlags.mInSystemGroup) {
      return;
    }

    // Check if we already know that there is no event listener for the event.
    if (mNoListenerForEvent == aEvent->mMessage &&
        (mNoListenerForEvent != eUnidentifiedEvent ||
         mNoListenerForEventAtom == aEvent->mSpecifiedEventType)) {
      return;
    }
    HandleEventInternal(aPresContext, aEvent, aDOMEvent, aCurrentTarget,
                        aEventStatus, aItemInShadowTree);
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
   * Allows us to quickly determine whether we have unload listeners registered.
   */
  bool HasUnloadListeners();

  /**
   * Allows us to quickly determine whether we have beforeunload listeners
   * registered.
   */
  bool HasBeforeUnloadListeners();

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
  bool HasListenersFor(const nsAString& aEventName) const;

  /**
   * Returns true if there is at least one event listener for aEventNameWithOn.
   * Note that aEventNameWithOn must start with "on"!
   */
  bool HasListenersFor(nsAtom* aEventNameWithOn) const;

  /**
   * Similar to HasListenersFor, but ignores system group listeners.
   */
  bool HasNonSystemGroupListenersFor(nsAtom* aEventNameWithOn) const;

  /**
   * Returns true if there is at least one event listener.
   */
  bool HasListeners() const;

  /**
   * Sets aList to the list of nsIEventListenerInfo objects representing the
   * listeners managed by this listener manager.
   */
  nsresult GetListenerInfo(nsTArray<RefPtr<nsIEventListenerInfo>>& aList);

  nsresult IsListenerEnabled(nsAString& aType, JSObject* aListener,
                             bool aCapturing, bool aAllowsUntrusted,
                             bool aInSystemEventGroup, bool aIsHandler,
                             bool* aEnabled);

  nsresult SetListenerEnabled(nsAString& aType, JSObject* aListener,
                              bool aCapturing, bool aAllowsUntrusted,
                              bool aInSystemEventGroup, bool aIsHandler,
                              bool aEnabled);

  uint32_t GetIdentifierForEvent(nsAtom* aEvent);

  /**
   * Returns true if there may be a paint event listener registered,
   * false if there definitely isn't.
   */
  bool MayHavePaintEventListener() const { return mMayHavePaintEventListener; }

  /**
   * Returns true if there may be a touch event listener registered,
   * false if there definitely isn't.
   */
  bool MayHaveTouchEventListener() const { return mMayHaveTouchEventListener; }

  bool MayHaveMouseEnterLeaveEventListener() const {
    return mMayHaveMouseEnterLeaveEventListener;
  }
  bool MayHavePointerEnterLeaveEventListener() const {
    return mMayHavePointerEnterLeaveEventListener;
  }
  bool MayHaveSelectionChangeEventListener() const {
    return mMayHaveSelectionChangeEventListener;
  }
  bool MayHaveFormSelectEventListener() const {
    return mMayHaveFormSelectEventListener;
  }

  bool HasNonPrivilegedClickListeners();

  /**
   * Returns true if there may be a key event listener (keydown, keypress,
   * or keyup) registered, or false if there definitely isn't.
   */
  bool MayHaveKeyEventListener() const { return mMayHaveKeyEventListener; }

  /**
   * Returns true if there may be an advanced input event listener (input,
   * compositionstart, compositionupdate, or compositionend) registered,
   * or false if there definitely isn't.
   */
  bool MayHaveInputOrCompositionEventListener() const {
    return mMayHaveInputOrCompositionEventListener;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  uint32_t ListenerCount() const { return mListeners.Length(); }

  void MarkForCC();

  void TraceListeners(JSTracer* aTrc);

  dom::EventTarget* GetTarget() { return mTarget; }

  bool HasNonSystemGroupListenersForUntrustedKeyEvents();
  bool HasNonPassiveNonSystemGroupListenersForUntrustedKeyEvents();

  bool HasApzAwareListeners();
  bool IsApzAwareListener(Listener* aListener);
  bool IsApzAwareEvent(nsAtom* aEvent);

  bool HasNonPassiveWheelListener();

  // Return true if aListener is a non-chrome-privileged click event listner
  bool IsNonChromeClickListener(Listener* aListener);
  /**
   * Remove all event listeners from the event target this EventListenerManager
   * is for.
   */
  void RemoveAllListeners();

 protected:
  MOZ_CAN_RUN_SCRIPT
  void HandleEventInternal(nsPresContext* aPresContext, WidgetEvent* aEvent,
                           dom::Event** aDOMEvent,
                           dom::EventTarget* aCurrentTarget,
                           nsEventStatus* aEventStatus, bool aItemInShadowTree);

  MOZ_CAN_RUN_SCRIPT
  nsresult HandleEventSubType(Listener* aListener, dom::Event* aDOMEvent,
                              dom::EventTarget* aCurrentTarget);

  /**
   * If the given EventMessage has a legacy version that we support, then this
   * function returns that legacy version. Otherwise, this function simply
   * returns the passed-in EventMessage.
   */
  EventMessage GetLegacyEventMessage(EventMessage aEventMessage) const;

  /**
   * Get the event message for the given event name.
   */
  EventMessage GetEventMessage(nsAtom* aEventName) const;

  /**
   * Get the event message and atom for the given event type.
   */
  EventMessage GetEventMessageAndAtomForListener(const nsAString& aType,
                                                 nsAtom** aAtom);

  void ProcessApzAwareEventListenerAdd();

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
  Listener* FindEventHandler(EventMessage aEventMessage, nsAtom* aTypeAtom);

  /**
   * Set the "inline" event listener for aName to aHandler.  aHandler may be
   * have no actual handler set to indicate that we should lazily get and
   * compile the string for this listener, but in that case aContext and
   * aScopeGlobal must be non-null.  Otherwise, aContext and aScopeGlobal are
   * allowed to be null.
   */
  Listener* SetEventHandlerInternal(nsAtom* aName,
                                    const TypedEventHandler& aHandler,
                                    bool aPermitUntrustedEvents);

  bool IsDeviceType(EventMessage aEventMessage);
  void EnableDevice(EventMessage aEventMessage);
  void DisableDevice(EventMessage aEventMessage);

  bool HasListenersForInternal(nsAtom* aEventNameWithOn,
                               bool aIgnoreSystemGroup) const;

  Listener* GetListenerFor(nsAString& aType, JSObject* aListener,
                           bool aCapturing, bool aAllowsUntrusted,
                           bool aInSystemEventGroup, bool aIsHandler);

 public:
  /**
   * Set the "inline" event listener for aEventName to aHandler.  If
   * aHandler is null, this will actually remove the event listener
   */
  void SetEventHandler(nsAtom* aEventName, dom::EventHandlerNonNull* aHandler);
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
  dom::EventHandlerNonNull* GetEventHandler(nsAtom* aEventName) {
    const TypedEventHandler* typedHandler = GetTypedEventHandler(aEventName);
    return typedHandler ? typedHandler->NormalEventHandler() : nullptr;
  }

  dom::OnErrorEventHandlerNonNull* GetOnErrorEventHandler() {
    const TypedEventHandler* typedHandler =
        GetTypedEventHandler(nsGkAtoms::onerror);
    return typedHandler ? typedHandler->OnErrorEventHandler() : nullptr;
  }

  dom::OnBeforeUnloadEventHandlerNonNull* GetOnBeforeUnloadEventHandler() {
    const TypedEventHandler* typedHandler =
        GetTypedEventHandler(nsGkAtoms::onbeforeunload);
    return typedHandler ? typedHandler->OnBeforeUnloadEventHandler() : nullptr;
  }

 private:
  already_AddRefed<nsPIDOMWindowInner> WindowFromListener(
      Listener* aListener, bool aItemInShadowTree);

 protected:
  /**
   * Helper method for implementing the various Get*EventHandler above.  Will
   * return null if we don't have an event handler for this event name.
   */
  const TypedEventHandler* GetTypedEventHandler(nsAtom* aEventName);

  void AddEventListener(const nsAString& aType, EventListenerHolder aListener,
                        const dom::AddEventListenerOptionsOrBoolean& aOptions,
                        bool aWantsUntrusted);
  void AddEventListener(const nsAString& aType, EventListenerHolder aListener,
                        bool aUseCapture, bool aWantsUntrusted);
  void RemoveEventListener(const nsAString& aType,
                           EventListenerHolder aListener,
                           const dom::EventListenerOptionsOrBoolean& aOptions);
  void RemoveEventListener(const nsAString& aType,
                           EventListenerHolder aListener, bool aUseCapture);

  void AddEventListenerInternal(EventListenerHolder aListener,
                                EventMessage aEventMessage, nsAtom* aTypeAtom,
                                const EventListenerFlags& aFlags,
                                bool aHandler = false, bool aAllEvents = false,
                                dom::AbortSignal* aSignal = nullptr);
  void RemoveEventListenerInternal(EventListenerHolder aListener,
                                   EventMessage aEventMessage,
                                   nsAtom* aUserType,
                                   const EventListenerFlags& aFlags,
                                   bool aAllEvents = false);
  void RemoveAllListenersSilently();
  void NotifyEventListenerRemoved(nsAtom* aUserType);
  const EventTypeData* GetTypeDataForIID(const nsIID& aIID);
  const EventTypeData* GetTypeDataForEventName(nsAtom* aName);
  nsPIDOMWindowInner* GetInnerWindowForTarget();
  already_AddRefed<nsPIDOMWindowInner> GetTargetAsInnerWindow() const;

  bool ListenerCanHandle(const Listener* aListener, const WidgetEvent* aEvent,
                         EventMessage aEventMessage) const;

  // BE AWARE, a lot of instances of EventListenerManager will be created.
  // Therefor, we need to keep this class compact.  When you add integer
  // members, please add them to EventListemerManagerBase and check the size
  // at build time.

  already_AddRefed<nsIScriptGlobalObject> GetScriptGlobalAndDocument(
      mozilla::dom::Document** aDoc);

  void MaybeMarkPassive(EventMessage aMessage, EventListenerFlags& aFlags);

  nsAutoTObserverArray<Listener, 2> mListeners;
  dom::EventTarget* MOZ_NON_OWNING_REF mTarget;
  RefPtr<nsAtom> mNoListenerForEventAtom;

  friend class ELMCreationDetector;
  static uint32_t sMainThreadCreatedCount;
};

}  // namespace mozilla

#endif  // mozilla_EventListenerManager_h_
