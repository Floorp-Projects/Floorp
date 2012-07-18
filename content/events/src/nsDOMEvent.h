/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMEvent_h__
#define nsDOMEvent_h__

#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
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
struct JSObject;
 
class nsDOMEvent : public nsIDOMEvent,
                   public nsIDOMNSEvent,
                   public nsIJSNativeInitializer
{
public:

  // Note: this enum must be kept in sync with sEventNames in nsDOMEvent.cpp
  enum nsDOMEvents {
    eDOMEvents_mousedown=0,
    eDOMEvents_mouseup,
    eDOMEvents_click,
    eDOMEvents_dblclick,
    eDOMEvents_mouseenter,
    eDOMEvents_mouseleave,
    eDOMEvents_mouseover,
    eDOMEvents_mouseout,
    eDOMEvents_MozMouseHittest,
    eDOMEvents_mousemove,
    eDOMEvents_contextmenu,
    eDOMEvents_keydown,
    eDOMEvents_keyup,
    eDOMEvents_keypress,
    eDOMEvents_focus,
    eDOMEvents_blur,
    eDOMEvents_load,
    eDOMEvents_popstate,
    eDOMEvents_beforescriptexecute,
    eDOMEvents_afterscriptexecute,
    eDOMEvents_beforeunload,
    eDOMEvents_unload,
    eDOMEvents_hashchange,
    eDOMEvents_readystatechange,
    eDOMEvents_abort,
    eDOMEvents_error,
    eDOMEvents_submit,
    eDOMEvents_reset,
    eDOMEvents_change,
    eDOMEvents_select,
    eDOMEvents_input,
    eDOMEvents_invalid,
    eDOMEvents_text,
    eDOMEvents_compositionstart,
    eDOMEvents_compositionend,
    eDOMEvents_compositionupdate,
    eDOMEvents_popupShowing,
    eDOMEvents_popupShown,
    eDOMEvents_popupHiding,
    eDOMEvents_popupHidden,
    eDOMEvents_close,
    eDOMEvents_command,
    eDOMEvents_broadcast,
    eDOMEvents_commandupdate,
    eDOMEvents_dragenter,
    eDOMEvents_dragover,
    eDOMEvents_dragexit,
    eDOMEvents_dragdrop,
    eDOMEvents_draggesture,
    eDOMEvents_drag,
    eDOMEvents_dragend,
    eDOMEvents_dragstart,
    eDOMEvents_dragleave,
    eDOMEvents_drop,
    eDOMEvents_resize,
    eDOMEvents_scroll,
    eDOMEvents_overflow,
    eDOMEvents_underflow,
    eDOMEvents_overflowchanged,
    eDOMEvents_subtreemodified,
    eDOMEvents_nodeinserted,
    eDOMEvents_noderemoved,
    eDOMEvents_noderemovedfromdocument,
    eDOMEvents_nodeinsertedintodocument,
    eDOMEvents_attrmodified,
    eDOMEvents_characterdatamodified,
    eDOMEvents_DOMActivate,
    eDOMEvents_DOMFocusIn,
    eDOMEvents_DOMFocusOut,
    eDOMEvents_pageshow,
    eDOMEvents_pagehide,
    eDOMEvents_DOMMouseScroll,
    eDOMEvents_MozMousePixelScroll,
    eDOMEvents_offline,
    eDOMEvents_online,
    eDOMEvents_copy,
    eDOMEvents_cut,
    eDOMEvents_paste,
    eDOMEvents_open,
    eDOMEvents_message,
    eDOMEvents_show,
    eDOMEvents_SVGLoad,
    eDOMEvents_SVGUnload,
    eDOMEvents_SVGAbort,
    eDOMEvents_SVGError,
    eDOMEvents_SVGResize,
    eDOMEvents_SVGScroll,
    eDOMEvents_SVGZoom,
    eDOMEvents_beginEvent,
    eDOMEvents_endEvent,
    eDOMEvents_repeatEvent,
#ifdef MOZ_MEDIA
    eDOMEvents_loadstart,
    eDOMEvents_progress,
    eDOMEvents_suspend,
    eDOMEvents_emptied,
    eDOMEvents_stalled,
    eDOMEvents_play,
    eDOMEvents_pause,
    eDOMEvents_loadedmetadata,
    eDOMEvents_loadeddata,
    eDOMEvents_waiting,
    eDOMEvents_playing,
    eDOMEvents_canplay,
    eDOMEvents_canplaythrough,
    eDOMEvents_seeking,
    eDOMEvents_seeked,
    eDOMEvents_timeupdate,
    eDOMEvents_ended,
    eDOMEvents_ratechange,
    eDOMEvents_durationchange,
    eDOMEvents_volumechange,
    eDOMEvents_mozaudioavailable,
#endif
    eDOMEvents_afterpaint,
    eDOMEvents_beforeresize,
    eDOMEvents_mozfullscreenchange,
    eDOMEvents_mozfullscreenerror,
    eDOMEvents_mozpointerlockchange,
    eDOMEvents_mozpointerlockerror,
    eDOMEvents_MozSwipeGesture,
    eDOMEvents_MozMagnifyGestureStart,
    eDOMEvents_MozMagnifyGestureUpdate,
    eDOMEvents_MozMagnifyGesture,
    eDOMEvents_MozRotateGestureStart,
    eDOMEvents_MozRotateGestureUpdate,
    eDOMEvents_MozRotateGesture,
    eDOMEvents_MozTapGesture,
    eDOMEvents_MozPressTapGesture,
    eDOMEvents_MozEdgeUIGesture,
    eDOMEvents_MozTouchDown,
    eDOMEvents_MozTouchMove,
    eDOMEvents_MozTouchUp,
    eDOMEvents_touchstart,
    eDOMEvents_touchend,
    eDOMEvents_touchmove,
    eDOMEvents_touchcancel,
    eDOMEvents_touchenter,
    eDOMEvents_touchleave,
    eDOMEvents_MozScrolledAreaChanged,
    eDOMEvents_transitionend,
    eDOMEvents_animationstart,
    eDOMEvents_animationend,
    eDOMEvents_animationiteration,
    eDOMEvents_devicemotion,
    eDOMEvents_deviceorientation,
    eDOMEvents_deviceproximity,
    eDOMEvents_userproximity,
    eDOMEvents_devicelight
  };

  nsDOMEvent(nsPresContext* aPresContext, nsEvent* aEvent);
  virtual ~nsDOMEvent();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMEvent, nsIDOMEvent)

  // nsIDOMEvent Interface
  NS_DECL_NSIDOMEVENT

  // nsIDOMNSEvent Interface
  NS_DECL_NSIDOMNSEVENT

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* aCx, JSObject* aObj,
                        PRUint32 aArgc, jsval* aArgv);

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx, jsval* aVal);

  void InitPresContextData(nsPresContext* aPresContext);

  static PopupControlState GetEventPopupControlState(nsEvent *aEvent);

  static void PopupAllowedEventsChanged();

  static void Shutdown();

  static const char* GetEventName(PRUint32 aEventType);
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
  NS_IMETHOD GetEventPhase(PRUint16 *aEventPhase) { return _to GetEventPhase(aEventPhase); } \
  NS_IMETHOD GetBubbles(bool *aBubbles) { return _to GetBubbles(aBubbles); } \
  NS_IMETHOD GetCancelable(bool *aCancelable) { return _to GetCancelable(aCancelable); } \
  NS_IMETHOD GetTimeStamp(DOMTimeStamp *aTimeStamp) { return _to GetTimeStamp(aTimeStamp); } \
  NS_IMETHOD StopPropagation(void) { return _to StopPropagation(); } \
  NS_IMETHOD PreventDefault(void) { return _to PreventDefault(); } \
  NS_IMETHOD InitEvent(const nsAString & eventTypeArg, bool canBubbleArg, bool cancelableArg) { return _to InitEvent(eventTypeArg, canBubbleArg, cancelableArg); } \
  NS_IMETHOD GetDefaultPrevented(bool *aDefaultPrevented) { return _to GetDefaultPrevented(aDefaultPrevented); } \
  NS_IMETHOD StopImmediatePropagation(void) { return _to StopImmediatePropagation(); } \
  NS_IMETHOD SetTarget(nsIDOMEventTarget *aTarget) { return _to SetTarget(aTarget); } \
  NS_IMETHOD_(bool) IsDispatchStopped(void) { return _to IsDispatchStopped(); } \
  NS_IMETHOD_(nsEvent *) GetInternalNSEvent(void) { return _to GetInternalNSEvent(); } \
  NS_IMETHOD SetTrusted(bool aTrusted) { return _to SetTrusted(aTrusted); }

#define NS_FORWARD_TO_NSDOMEVENT_NO_SERIALIZATION_NO_DUPLICATION \
  NS_FORWARD_NSIDOMEVENT_NO_SERIALIZATION_NO_DUPLICATION(nsDOMEvent::)

#endif // nsDOMEvent_h__
