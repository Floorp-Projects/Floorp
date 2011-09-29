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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#ifndef nsDOMEvent_h__
#define nsDOMEvent_h__

#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsISupports.h"
#include "nsIPrivateDOMEvent.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventTarget.h"
#include "nsPIDOMWindow.h"
#include "nsPoint.h"
#include "nsGUIEvent.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"

class nsIContent;
class nsPresContext;
 
class nsDOMEvent : public nsIDOMEvent,
                   public nsIDOMNSEvent,
                   public nsIPrivateDOMEvent
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
#ifdef MOZ_SMIL
    eDOMEvents_beginEvent,
    eDOMEvents_endEvent,
    eDOMEvents_repeatEvent,
#endif // MOZ_SMIL
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
    eDOMEvents_beforepaint,
    eDOMEvents_beforeresize,
    eDOMEvents_mozfullscreenchange,
    eDOMEvents_MozSwipeGesture,
    eDOMEvents_MozMagnifyGestureStart,
    eDOMEvents_MozMagnifyGestureUpdate,
    eDOMEvents_MozMagnifyGesture,
    eDOMEvents_MozRotateGestureStart,
    eDOMEvents_MozRotateGestureUpdate,
    eDOMEvents_MozRotateGesture,
    eDOMEvents_MozTapGesture,
    eDOMEvents_MozPressTapGesture,
    eDOMEvents_MozTouchDown,
    eDOMEvents_MozTouchMove,
    eDOMEvents_MozTouchUp,
    eDOMEvents_MozScrolledAreaChanged,
    eDOMEvents_transitionend,
    eDOMEvents_animationstart,
    eDOMEvents_animationend,
    eDOMEvents_animationiteration,
    eDOMEvents_devicemotion,
    eDOMEvents_deviceorientation
  };

  nsDOMEvent(nsPresContext* aPresContext, nsEvent* aEvent);
  virtual ~nsDOMEvent();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMEvent, nsIDOMEvent)

  // nsIDOMEvent Interface
  NS_DECL_NSIDOMEVENT

  // nsIDOMNSEvent Interface
  NS_DECL_NSIDOMNSEVENT

  // nsIPrivateDOMEvent interface
  NS_IMETHOD    DuplicatePrivateData();
  NS_IMETHOD    SetTarget(nsIDOMEventTarget* aTarget);
  NS_IMETHOD_(bool)    IsDispatchStopped();
  NS_IMETHOD_(nsEvent*)    GetInternalNSEvent();
  NS_IMETHOD    SetTrusted(bool aTrusted);

  virtual void Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType);
  virtual bool Deserialize(const IPC::Message* aMsg, void** aIter);

  static PopupControlState GetEventPopupControlState(nsEvent *aEvent);

  static void PopupAllowedEventsChanged();

  static void Shutdown();

  static const char* GetEventName(PRUint32 aEventType);
protected:

  // Internal helper functions
  nsresult SetEventType(const nsAString& aEventTypeArg);
  already_AddRefed<nsIContent> GetTargetFromFrame();

  nsEvent*                    mEvent;
  nsRefPtr<nsPresContext>     mPresContext;
  nsCOMPtr<nsIDOMEventTarget> mTmpRealOriginalTarget;
  nsIDOMEventTarget*          mExplicitOriginalTarget;
  nsString                    mCachedType;
  bool                        mEventIsInternal;
  bool                        mPrivateDataDuplicated;
};

#define NS_FORWARD_TO_NSDOMEVENT \
  NS_FORWARD_NSIDOMEVENT(nsDOMEvent::)

#endif // nsDOMEvent_h__
