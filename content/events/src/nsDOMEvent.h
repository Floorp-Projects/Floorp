/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMEvent_h__
#define nsDOMEvent_h__

#include "nsIDOMEvent.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventTarget.h"
#include "nsPIDOMWindow.h"
#include "nsPoint.h"
#include "nsGUIEvent.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "nsIJSNativeInitializer.h"

class nsIContent;
class nsPresContext;
struct JSContext;
class JSObject;
 
class nsDOMEvent : public nsIDOMEvent,
                   public nsIJSNativeInitializer
{
public:

  nsDOMEvent(nsPresContext* aPresContext, nsEvent* aEvent);
  virtual ~nsDOMEvent();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMEvent, nsIDOMEvent)

  // nsIDOMEvent Interface
  NS_DECL_NSIDOMEVENT

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* aCx, JSObject* aObj,
                        uint32_t aArgc, jsval* aArgv);

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx, jsval* aVal);

  void InitPresContextData(nsPresContext* aPresContext);

  static PopupControlState GetEventPopupControlState(nsEvent *aEvent);

  static void PopupAllowedEventsChanged();

  static void Shutdown();

  static const char* GetEventName(uint32_t aEventType);
  static nsIntPoint GetClientCoords(nsPresContext* aPresContext,
                                    nsEvent* aEvent,
                                    nsIntPoint aPoint,
                                    nsIntPoint aDefaultPoint);
  static nsIntPoint GetPageCoords(nsPresContext* aPresContext,
                                  nsEvent* aEvent,
                                  nsIntPoint aPoint,
                                  nsIntPoint aDefaultPoint);
  static nsIntPoint GetScreenCoords(nsPresContext* aPresContext,
                                    nsEvent* aEvent,
                                    nsIntPoint aPoint);
protected:

  // Internal helper functions
  void SetEventType(const nsAString& aEventTypeArg);
  already_AddRefed<nsIContent> GetTargetFromFrame();

  nsEvent*                    mEvent;
  nsRefPtr<nsPresContext>     mPresContext;
  nsCOMPtr<nsIDOMEventTarget> mExplicitOriginalTarget;
  nsString                    mCachedType;
  bool                        mEventIsInternal;
  bool                        mPrivateDataDuplicated;
};

#define NS_FORWARD_TO_NSDOMEVENT \
  NS_FORWARD_NSIDOMEVENT(nsDOMEvent::)

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
  NS_IMETHOD PreventBubble() { return _to PreventBubble(); } \
  NS_IMETHOD PreventCapture() { return _to PreventCapture(); } \
  NS_IMETHOD GetPreventDefault(bool* aRetval) { return _to GetPreventDefault(aRetval); } \
  NS_IMETHOD GetIsTrusted(bool* aIsTrusted) { return _to GetIsTrusted(aIsTrusted); } \
  NS_IMETHOD SetTarget(nsIDOMEventTarget *aTarget) { return _to SetTarget(aTarget); } \
  NS_IMETHOD_(bool) IsDispatchStopped(void) { return _to IsDispatchStopped(); } \
  NS_IMETHOD_(nsEvent *) GetInternalNSEvent(void) { return _to GetInternalNSEvent(); } \
  NS_IMETHOD_(void) SetTrusted(bool aTrusted) { _to SetTrusted(aTrusted); }

#define NS_FORWARD_TO_NSDOMEVENT_NO_SERIALIZATION_NO_DUPLICATION \
  NS_FORWARD_NSIDOMEVENT_NO_SERIALIZATION_NO_DUPLICATION(nsDOMEvent::)

#endif // nsDOMEvent_h__
