/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEventListenerManager_h__
#define nsEventListenerManager_h__

#include "mozilla/BasicEvents.h"
#include "mozilla/dom/EventListenerBinding.h"
#include "mozilla/MemoryReporting.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGkAtoms.h"
#include "nsIDOMEventListener.h"
#include "nsIJSEventListener.h"
#include "nsTObserverArray.h"

class nsIDOMEvent;
struct EventTypeData;
class nsEventTargetChainItem;
class nsPIDOMWindow;
class nsIEventListenerInfo;
class nsIScriptContext;

struct nsListenerStruct;
class nsEventListenerManager;

template<class T> class nsCOMArray;

namespace mozilla {
namespace dom {

class EventTarget;
class Element;

typedef CallbackObjectHolder<EventListener, nsIDOMEventListener>
  EventListenerHolder;

struct EventListenerFlags
{
  friend struct ::nsListenerStruct;
  friend class  ::nsEventListenerManager;
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

} // namespace dom
} // namespace mozilla

typedef enum
{
    eNativeListener = 0,
    eJSEventListener,
    eWrappedJSListener,
    eWebIDLListener,
    eListenerTypeCount
} nsListenerType;

struct nsListenerStruct
{
  mozilla::dom::EventListenerHolder mListener;
  nsCOMPtr<nsIAtom>             mTypeAtom;   // for the main thread
  nsString                      mTypeString; // for non-main-threads
  uint16_t                      mEventType;
  uint8_t                       mListenerType;
  bool                          mListenerIsHandler : 1;
  bool                          mHandlerIsString : 1;
  bool                          mAllEvents : 1;

  mozilla::dom::EventListenerFlags mFlags;

  nsIJSEventListener* GetJSListener() const {
    return (mListenerType == eJSEventListener) ?
      static_cast<nsIJSEventListener *>(mListener.GetXPCOMCallback()) : nullptr;
  }

  nsListenerStruct()
  {
    MOZ_ASSERT(sizeof(mListenerType) == 1);
    MOZ_ASSERT(eListenerTypeCount < 255);
  }

  ~nsListenerStruct()
  {
    if ((mListenerType == eJSEventListener) && mListener) {
      static_cast<nsIJSEventListener*>(mListener.GetXPCOMCallback())->Disconnect();
    }
  }

  MOZ_ALWAYS_INLINE bool IsListening(const mozilla::WidgetEvent* aEvent) const
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

/*
 * Event listener manager
 */

class nsEventListenerManager
{

public:
  nsEventListenerManager(mozilla::dom::EventTarget* aTarget);
  virtual ~nsEventListenerManager();

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsEventListenerManager)

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsEventListenerManager)

  void AddEventListener(const nsAString& aType,
                        nsIDOMEventListener* aListener,
                        bool aUseCapture,
                        bool aWantsUntrusted)
  {
    mozilla::dom::EventListenerHolder holder(aListener);
    AddEventListener(aType, holder, aUseCapture, aWantsUntrusted);
  }
  void AddEventListener(const nsAString& aType,
                        mozilla::dom::EventListener* aListener,
                        bool aUseCapture,
                        bool aWantsUntrusted)
  {
    mozilla::dom::EventListenerHolder holder(aListener);
    AddEventListener(aType, holder, aUseCapture, aWantsUntrusted);
  }
  void RemoveEventListener(const nsAString& aType,
                           nsIDOMEventListener* aListener,
                           bool aUseCapture)
  {
    mozilla::dom::EventListenerHolder holder(aListener);
    RemoveEventListener(aType, holder, aUseCapture);
  }
  void RemoveEventListener(const nsAString& aType,
                           mozilla::dom::EventListener* aListener,
                           bool aUseCapture)
  {
    mozilla::dom::EventListenerHolder holder(aListener);
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
                              const mozilla::dom::EventListenerFlags& aFlags)
  {
    mozilla::dom::EventListenerHolder holder(aListener);
    AddEventListenerByType(holder, type, aFlags);
  }
  void AddEventListenerByType(const mozilla::dom::EventListenerHolder& aListener,
                              const nsAString& type,
                              const mozilla::dom::EventListenerFlags& aFlags);
  void RemoveEventListenerByType(nsIDOMEventListener *aListener,
                                 const nsAString& type,
                                 const mozilla::dom::EventListenerFlags& aFlags)
  {
    mozilla::dom::EventListenerHolder holder(aListener);
    RemoveEventListenerByType(holder, type, aFlags);
  }
  void RemoveEventListenerByType(const mozilla::dom::EventListenerHolder& aListener,
                                 const nsAString& type,
                                 const mozilla::dom::EventListenerFlags& aFlags);

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
                           uint32_t aLanguage,
                           bool aDeferCompilation,
                           bool aPermitUntrustedEvents,
                           mozilla::dom::Element* aElement);
  /**
   * Remove the current "inline" event listener for aName.
   */
  void RemoveEventHandler(nsIAtom *aName, const nsAString& aTypeString);

  void HandleEvent(nsPresContext* aPresContext,
                   mozilla::WidgetEvent* aEvent, 
                   nsIDOMEvent** aDOMEvent,
                   mozilla::dom::EventTarget* aCurrentTarget,
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
   * Returns true if there may be a MozAudioAvailable event listener registered,
   * false if there definitely isn't.
   */
  bool MayHaveAudioAvailableEventListener() { return mMayHaveAudioAvailableEventListener; }

  /**
   * Returns true if there may be a touch event listener registered,
   * false if there definitely isn't.
   */
  bool MayHaveTouchEventListener() { return mMayHaveTouchEventListener; }

  bool MayHaveMouseEnterLeaveEventListener() { return mMayHaveMouseEnterLeaveEventListener; }
  bool MayHavePointerEnterLeaveEventListener() { return mMayHavePointerEnterLeaveEventListener; }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  uint32_t ListenerCount() const
  {
    return mListeners.Length();
  }

  void MarkForCC();

  mozilla::dom::EventTarget* GetTarget() { return mTarget; }
protected:
  void HandleEventInternal(nsPresContext* aPresContext,
                           mozilla::WidgetEvent* aEvent,
                           nsIDOMEvent** aDOMEvent,
                           mozilla::dom::EventTarget* aCurrentTarget,
                           nsEventStatus* aEventStatus);

  nsresult HandleEventSubType(nsListenerStruct* aListenerStruct,
                              nsIDOMEvent* aDOMEvent,
                              mozilla::dom::EventTarget* aCurrentTarget);

  /**
   * Compile the "inline" event listener for aListenerStruct.  The
   * body of the listener can be provided in aBody; if this is null we
   * will look for it on mTarget.  If aBody is provided, aElement should be
   * as well; otherwise it will also be inferred from mTarget.
   */
  nsresult CompileEventHandlerInternal(nsListenerStruct *aListenerStruct,
                                       const nsAString* aBody,
                                       mozilla::dom::Element* aElement);

  /**
   * Find the nsListenerStruct for the "inline" event listener for aTypeAtom.
   */
  nsListenerStruct* FindEventHandler(uint32_t aEventType, nsIAtom* aTypeAtom,
                                     const nsAString& aTypeString);

  /**
   * Set the "inline" event listener for aName to aHandler.  aHandler may be
   * have no actual handler set to indicate that we should lazily get and
   * compile the string for this listener, but in that case aContext and
   * aScopeGlobal must be non-null.  Otherwise, aContext and aScopeGlobal are
   * allowed to be null.  The nsListenerStruct that results, if any, is returned
   * in aListenerStruct.
   */
  nsListenerStruct* SetEventHandlerInternal(JS::Handle<JSObject*> aScopeGlobal,
                                            nsIAtom* aName,
                                            const nsAString& aTypeString,
                                            const nsEventHandler& aHandler,
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
                       mozilla::dom::EventHandlerNonNull* aHandler);
  void SetEventHandler(mozilla::dom::OnErrorEventHandlerNonNull* aHandler);
  void SetEventHandler(mozilla::dom::OnBeforeUnloadEventHandlerNonNull* aHandler);

  /**
   * Get the value of the "inline" event listener for aEventName.
   * This may cause lazy compilation if the listener is uncompiled.
   *
   * Note: It's the caller's responsibility to make sure to call the right one
   * of these methods.  In particular, "onerror" events use
   * OnErrorEventHandlerNonNull for some event targets and EventHandlerNonNull
   * for others.
   */
  mozilla::dom::EventHandlerNonNull* GetEventHandler(nsIAtom *aEventName,
                                                     const nsAString& aTypeString)
  {
    const nsEventHandler* handler =
      GetEventHandlerInternal(aEventName, aTypeString);
    return handler ? handler->EventHandler() : nullptr;
  }
  mozilla::dom::OnErrorEventHandlerNonNull* GetOnErrorEventHandler()
  {
    const nsEventHandler* handler;
    if (mIsMainThreadELM) {
      handler = GetEventHandlerInternal(nsGkAtoms::onerror, EmptyString());
    } else {
      handler = GetEventHandlerInternal(nullptr, NS_LITERAL_STRING("error"));
    }
    return handler ? handler->OnErrorEventHandler() : nullptr;
  }
  mozilla::dom::OnBeforeUnloadEventHandlerNonNull* GetOnBeforeUnloadEventHandler()
  {
    const nsEventHandler* handler =
      GetEventHandlerInternal(nsGkAtoms::onbeforeunload, EmptyString());
    return handler ? handler->OnBeforeUnloadEventHandler() : nullptr;
  }

protected:
  /**
   * Helper method for implementing the various Get*EventHandler above.  Will
   * return null if we don't have an event handler for this event name.
   */
  const nsEventHandler* GetEventHandlerInternal(nsIAtom* aEventName,
                                                const nsAString& aTypeString);

  void AddEventListener(const nsAString& aType,
                        const mozilla::dom::EventListenerHolder& aListener,
                        bool aUseCapture,
                        bool aWantsUntrusted);
  void RemoveEventListener(const nsAString& aType,
                           const mozilla::dom::EventListenerHolder& aListener,
                           bool aUseCapture);

  void AddEventListenerInternal(
         const mozilla::dom::EventListenerHolder& aListener,
         uint32_t aType,
         nsIAtom* aTypeAtom,
         const nsAString& aTypeString,
         const mozilla::dom::EventListenerFlags& aFlags,
         bool aHandler = false,
         bool aAllEvents = false);
  void RemoveEventListenerInternal(
         const mozilla::dom::EventListenerHolder& aListener,
         uint32_t aType,
         nsIAtom* aUserType,
         const nsAString& aTypeString,
         const mozilla::dom::EventListenerFlags& aFlags,
         bool aAllEvents = false);
  void RemoveAllListeners();
  const EventTypeData* GetTypeDataForIID(const nsIID& aIID);
  const EventTypeData* GetTypeDataForEventName(nsIAtom* aName);
  nsPIDOMWindow* GetInnerWindowForTarget();
  already_AddRefed<nsPIDOMWindow> GetTargetAsInnerWindow() const;

  bool ListenerCanHandle(nsListenerStruct* aLs, mozilla::WidgetEvent* aEvent);

  already_AddRefed<nsIScriptGlobalObject>
  GetScriptGlobalAndDocument(nsIDocument** aDoc);

  uint32_t mMayHavePaintEventListener : 1;
  uint32_t mMayHaveMutationListeners : 1;
  uint32_t mMayHaveCapturingListeners : 1;
  uint32_t mMayHaveSystemGroupListeners : 1;
  uint32_t mMayHaveAudioAvailableEventListener : 1;
  uint32_t mMayHaveTouchEventListener : 1;
  uint32_t mMayHaveMouseEnterLeaveEventListener : 1;
  uint32_t mMayHavePointerEnterLeaveEventListener : 1;
  uint32_t mClearingListeners : 1;
  uint32_t mIsMainThreadELM : 1;
  uint32_t mNoListenerForEvent : 22;

  nsAutoTObserverArray<nsListenerStruct, 2> mListeners;
  mozilla::dom::EventTarget*                mTarget;  //WEAK
  nsCOMPtr<nsIAtom>                         mNoListenerForEventAtom;

  friend class ELMCreationDetector;
  static uint32_t                           sMainThreadCreatedCount;
};

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
  nsEventListenerManager* listenerManager =
    aTarget->GetOrCreateListenerManager();
  NS_ENSURE_STATE(listenerManager);
  mozilla::dom::EventListenerFlags flags;
  flags.mInSystemGroup = true;
  flags.mCapture = aUseCapture;
  flags.mAllowUntrustedEvents = aWantsUntrusted;
  listenerManager->AddEventListenerByType(aListener, aType, flags);
  return NS_OK;
}

#endif // nsEventListenerManager_h__
