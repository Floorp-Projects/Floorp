/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Event_h_
#define mozilla_dom_Event_h_

#include "mozilla/Attributes.h"
#include "mozilla/BasicEvents.h"
#include "nsIDOMEvent.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsPIDOMWindow.h"
#include "nsPoint.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "mozilla/dom/EventBinding.h"
#include "nsIScriptGlobalObject.h"
#include "Units.h"
#include "js/TypeDecls.h"

class nsIContent;
class nsIDOMEventTarget;
class nsPresContext;

namespace mozilla {
namespace dom {

class EventTarget;
class ErrorEvent;
class ProgressEvent;

// Dummy class so we can cast through it to get from nsISupports to
// Event subclasses with only two non-ambiguous static casts.
class EventBase : public nsIDOMEvent
{
};

class Event : public EventBase,
              public nsWrapperCache
{
public:
  Event(EventTarget* aOwner,
        nsPresContext* aPresContext,
        WidgetEvent* aEvent);
  explicit Event(nsPIDOMWindow* aWindow);

protected:
  virtual ~Event();

private:
  void ConstructorInit(EventTarget* aOwner,
                       nsPresContext* aPresContext,
                       WidgetEvent* aEvent);

public:
  void GetParentObject(nsIScriptGlobalObject** aParentObject)
  {
    if (mOwner) {
      CallQueryInterface(mOwner, aParentObject);
    } else {
      *aParentObject = nullptr;
    }
  }

  static Event* FromSupports(nsISupports* aSupports)
  {
    nsIDOMEvent* event =
      static_cast<nsIDOMEvent*>(aSupports);
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMEvent> target_qi =
        do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMEvent pointer as the
      // nsISupports pointer. That must be fixed, or we'll crash...
      MOZ_ASSERT(target_qi == event, "Uh, fix QI!");
    }
#endif
    return static_cast<Event*>(event);
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Event)

  nsISupports* GetParentObject()
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return EventBinding::Wrap(aCx, this);
  }

  virtual ErrorEvent* AsErrorEvent()
  {
    return nullptr;
  }

  virtual ProgressEvent* AsProgressEvent()
  {
    return nullptr;
  }

  // nsIDOMEvent Interface
  NS_DECL_NSIDOMEVENT

  void InitPresContextData(nsPresContext* aPresContext);

  // Returns true if the event should be trusted.
  bool Init(EventTarget* aGlobal);

  static PopupControlState GetEventPopupControlState(WidgetEvent* aEvent);

  static void PopupAllowedEventsChanged();

  static void Shutdown();

  static const char* GetEventName(uint32_t aEventType);
  static CSSIntPoint GetClientCoords(nsPresContext* aPresContext,
                                     WidgetEvent* aEvent,
                                     LayoutDeviceIntPoint aPoint,
                                     CSSIntPoint aDefaultPoint);
  static CSSIntPoint GetPageCoords(nsPresContext* aPresContext,
                                   WidgetEvent* aEvent,
                                   LayoutDeviceIntPoint aPoint,
                                   CSSIntPoint aDefaultPoint);
  static nsIntPoint GetScreenCoords(nsPresContext* aPresContext,
                                    WidgetEvent* aEvent,
                                    LayoutDeviceIntPoint aPoint);

  static already_AddRefed<Event> Constructor(const GlobalObject& aGlobal,
                                             const nsAString& aType,
                                             const EventInit& aParam,
                                             ErrorResult& aRv);

  // Implemented as xpidl method
  // void GetType(nsString& aRetval) {}

  EventTarget* GetTarget() const;
  EventTarget* GetCurrentTarget() const;

  uint16_t EventPhase() const;

  // xpidl implementation
  // void StopPropagation();

  // xpidl implementation
  // void StopImmediatePropagation();

  bool Bubbles() const
  {
    return mEvent->mFlags.mBubbles;
  }

  bool Cancelable() const
  {
    return mEvent->mFlags.mCancelable;
  }

  // xpidl implementation
  // void PreventDefault();

  // You MUST NOT call PreventDefaultJ(JSContext*) from C++ code.  A call of
  // this method always sets Event.defaultPrevented true for web contents.
  // If default action handler calls this, web applications meet wrong
  // defaultPrevented value.
  void PreventDefault(JSContext* aCx);

  // You MUST NOT call DefaultPrevented(JSContext*) from C++ code.  This may
  // return false even if PreventDefault() has been called.
  // See comments in its implementation for the detail.
  bool DefaultPrevented(JSContext* aCx) const;

  bool DefaultPrevented() const
  {
    return mEvent->mFlags.mDefaultPrevented;
  }

  bool MultipleActionsPrevented() const
  {
    return mEvent->mFlags.mMultipleActionsPrevented;
  }

  bool IsTrusted() const
  {
    return mEvent->mFlags.mIsTrusted;
  }

  bool IsSynthesized() const
  {
    return mEvent->mFlags.mIsSynthesizedForTests;
  }

  double TimeStamp() const;

  void InitEvent(const nsAString& aType, bool aBubbles, bool aCancelable,
                 ErrorResult& aRv)
  {
    aRv = InitEvent(aType, aBubbles, aCancelable);
  }

  EventTarget* GetOriginalTarget() const;
  EventTarget* GetExplicitOriginalTarget() const;

  bool GetPreventDefault() const;

  /**
   * @param aCalledByDefaultHandler     Should be true when this is called by
   *                                    C++ or Chrome.  Otherwise, e.g., called
   *                                    by a call of Event.preventDefault() in
   *                                    content script, false.
   */
  void PreventDefaultInternal(bool aCalledByDefaultHandler);

  bool IsMainThreadEvent()
  {
    return mIsMainThreadEvent;
  }

protected:

  // Internal helper functions
  void SetEventType(const nsAString& aEventTypeArg);
  already_AddRefed<nsIContent> GetTargetFromFrame();

  /**
   * IsChrome() returns true if aCx is chrome context or the event is created
   * in chrome's thread.  Otherwise, false.
   */
  bool IsChrome(JSContext* aCx) const;

  mozilla::WidgetEvent*       mEvent;
  nsRefPtr<nsPresContext>     mPresContext;
  nsCOMPtr<EventTarget>       mExplicitOriginalTarget;
  nsCOMPtr<nsPIDOMWindow>     mOwner; // nsPIDOMWindow for now.
  bool                        mEventIsInternal;
  bool                        mPrivateDataDuplicated;
  bool                        mIsMainThreadEvent;
};

} // namespace dom
} // namespace mozilla

#define NS_FORWARD_TO_EVENT \
  NS_FORWARD_NSIDOMEVENT(Event::)

#define NS_FORWARD_NSIDOMEVENT_NO_SERIALIZATION_NO_DUPLICATION(_to) \
  NS_IMETHOD GetType(nsAString& aType){ return _to GetType(aType); } \
  NS_IMETHOD GetTarget(nsIDOMEventTarget * *aTarget) { return _to GetTarget(aTarget); } \
  NS_IMETHOD GetCurrentTarget(nsIDOMEventTarget * *aCurrentTarget) { return _to GetCurrentTarget(aCurrentTarget); } \
  NS_IMETHOD GetEventPhase(uint16_t *aEventPhase) { return _to GetEventPhase(aEventPhase); } \
  NS_IMETHOD GetBubbles(bool *aBubbles) { return _to GetBubbles(aBubbles); } \
  NS_IMETHOD GetCancelable(bool *aCancelable) { return _to GetCancelable(aCancelable); } \
  NS_IMETHOD GetTimeStamp(DOMTimeStamp *aTimeStamp) { return _to GetTimeStamp(aTimeStamp); } \
  NS_IMETHOD StopPropagation(void) { return _to StopPropagation(); } \
  NS_IMETHOD PreventDefault(void) { return _to PreventDefault(); } \
  NS_IMETHOD InitEvent(const nsAString & eventTypeArg, bool canBubbleArg, bool cancelableArg) { return _to InitEvent(eventTypeArg, canBubbleArg, cancelableArg); } \
  NS_IMETHOD GetDefaultPrevented(bool *aDefaultPrevented) { return _to GetDefaultPrevented(aDefaultPrevented); } \
  NS_IMETHOD StopImmediatePropagation(void) { return _to StopImmediatePropagation(); } \
  NS_IMETHOD GetOriginalTarget(nsIDOMEventTarget** aOriginalTarget) { return _to GetOriginalTarget(aOriginalTarget); } \
  NS_IMETHOD GetExplicitOriginalTarget(nsIDOMEventTarget** aExplicitOriginalTarget) { return _to GetExplicitOriginalTarget(aExplicitOriginalTarget); } \
  NS_IMETHOD GetPreventDefault(bool* aRetval) { return _to GetPreventDefault(aRetval); } \
  NS_IMETHOD GetIsTrusted(bool* aIsTrusted) { return _to GetIsTrusted(aIsTrusted); } \
  NS_IMETHOD SetTarget(nsIDOMEventTarget *aTarget) { return _to SetTarget(aTarget); } \
  NS_IMETHOD_(bool) IsDispatchStopped(void) { return _to IsDispatchStopped(); } \
  NS_IMETHOD_(WidgetEvent*) GetInternalNSEvent(void) { return _to GetInternalNSEvent(); } \
  NS_IMETHOD_(void) SetTrusted(bool aTrusted) { _to SetTrusted(aTrusted); } \
  NS_IMETHOD_(void) SetOwner(EventTarget* aOwner) { _to SetOwner(aOwner); } \
  NS_IMETHOD_(Event*) InternalDOMEvent() { return _to InternalDOMEvent(); }

#define NS_FORWARD_TO_EVENT_NO_SERIALIZATION_NO_DUPLICATION \
  NS_FORWARD_NSIDOMEVENT_NO_SERIALIZATION_NO_DUPLICATION(Event::)

inline nsISupports*
ToSupports(mozilla::dom::Event* e)
{
  return static_cast<nsIDOMEvent*>(e);
}

inline nsISupports*
ToCanonicalSupports(mozilla::dom::Event* e)
{
  return static_cast<nsIDOMEvent*>(e);
}

#endif // mozilla_dom_Event_h_
