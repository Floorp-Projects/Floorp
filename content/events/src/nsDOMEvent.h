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

#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMPopupBlockedEvent.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsISupports.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIPrivateCompositionEvent.h"
#include "nsIPrivateTextEvent.h"
#include "nsIPrivateTextRange.h"
#include "nsIDOMEvent.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventTarget.h"
#include "nsIPresContext.h"
#include "nsPoint.h"
#include "nsGUIEvent.h"

class nsIContent;
class nsIScrollableView;

class nsDOMEvent : public nsIDOMKeyEvent,
                   public nsIDOMNSEvent,
                   public nsIDOMMouseEvent,
                   public nsIDOMPopupBlockedEvent,
                   public nsIDOMNSUIEvent,
                   public nsIPrivateDOMEvent,
                   public nsIPrivateTextEvent,
                   public nsIPrivateCompositionEvent
{
public:

  // Note: this enum must be kept in sync with mEventNames in nsDOMEvent.cpp
  enum nsDOMEvents {
    eDOMEvents_mousedown=0,
    eDOMEvents_mouseup,
    eDOMEvents_click,
    eDOMEvents_dblclick,
    eDOMEvents_mouseover,
    eDOMEvents_mouseout,
    eDOMEvents_mousemove,
    eDOMEvents_contextmenu,
    eDOMEvents_keydown,
    eDOMEvents_keyup,
    eDOMEvents_keypress,
    eDOMEvents_focus,
    eDOMEvents_blur,
    eDOMEvents_load,
    eDOMEvents_beforeunload,
    eDOMEvents_unload,
    eDOMEvents_abort,
    eDOMEvents_error,
    eDOMEvents_submit,
    eDOMEvents_reset,
    eDOMEvents_change,
    eDOMEvents_input,
    eDOMEvents_formchange,
    eDOMEvents_forminput,
    eDOMEvents_select,
    eDOMEvents_paint,
    eDOMEvents_text,
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
    eDOMEvents_popupBlocked,
    eDOMEvents_DOMActivate,
    eDOMEvents_DOMFocusIn,
    eDOMEvents_DOMFocusOut
  };

  nsDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
             const nsAString& aEventType);
  virtual ~nsDOMEvent();

  NS_DECL_ISUPPORTS

  // nsIDOMEvent Interface
  NS_DECL_NSIDOMEVENT

  // nsIDOMNSEvent Interface
  NS_DECL_NSIDOMNSEVENT

  // nsIDOMUIEvent Interface
  NS_DECL_NSIDOMUIEVENT

  // nsIDOMPopupBlockedEvent interface
  NS_DECL_NSIDOMPOPUPBLOCKEDEVENT

  // nsIDOMMouseEvent interface
  NS_DECL_NSIDOMMOUSEEVENT

  // nsIDOMKeyEvent interface
  NS_IMETHOD GetCharCode(PRUint32 *aCharCode);
  NS_IMETHOD GetKeyCode(PRUint32 *aKeyCode);
  // defined in nsIDOMMouseEvent
  // NS_IMETHOD GetAltKey(PRBool *aAltKey);
  // NS_IMETHOD GetCtrlKey(PRBool *aCtrlKey);
  // NS_IMETHOD GetShiftKey(PRBool *aShiftKey);
  // NS_IMETHOD GetMetaKey(PRBool *aMetaKey);
  NS_IMETHOD InitKeyEvent(const nsAString &aTypeTag, PRBool aCanBubbleArg,
                          PRBool aCancelableArg, nsIDOMAbstractView *aViewArg,
                          PRBool aCtrlKeyArg, PRBool aAltKeyArg,
                          PRBool aShiftKeyArg, PRBool aMetaKeyArg,
                          PRUint32 aKeyCodeArg, PRUint32 aCharCodeArg);

  // nsIDOMNSUIEvent interface
  NS_DECL_NSIDOMNSUIEVENT

  // nsIPrivateDOMEvent interface
  NS_IMETHOD    DuplicatePrivateData();
  NS_IMETHOD    SetTarget(nsIDOMEventTarget* aTarget);
  NS_IMETHOD    SetCurrentTarget(nsIDOMEventTarget* aCurrentTarget);
  NS_IMETHOD    SetOriginalTarget(nsIDOMEventTarget* aOriginalTarget);
  NS_IMETHOD    IsDispatchStopped(PRBool* aIsDispatchStopped);
  NS_IMETHOD    GetInternalNSEvent(nsEvent** aNSEvent);
  NS_IMETHOD    HasOriginalTarget(PRBool* aResult);
  NS_IMETHOD    IsTrustedEvent(PRBool* aResult);
  NS_IMETHOD    SetTrusted(PRBool aTrusted);

  NS_IMETHOD    IsHandled(PRBool* aHandled);
  NS_IMETHOD    SetHandled(PRBool aHandled);

  // nsIPrivateTextEvent interface
	NS_IMETHOD GetText(nsString& aText);
	NS_IMETHOD GetInputRange(nsIPrivateTextRangeList** aInputRange);
	NS_IMETHOD GetEventReply(nsTextEventReply** aReply);

  // nsIPrivateCompositionEvent interface
  NS_IMETHOD GetCompositionReply(nsTextEventReply** aReply);
  NS_IMETHOD GetReconversionReply(nsReconversionEventReply** aReply);

  /** Overloaded new operator. Initializes the memory to 0. 
   *  Relies on a recycler to perform the allocation, 
   *  optionally from a pool.
   */
  void* operator new(size_t sz) CPP_THROW_NEW;

  /** Overloaded delete operator. Relies on a recycler to either
    * recycle the object or call the global delete operator, as needed.
    */
  void operator delete(void* aPtr);

protected:

  nsDOMEvent() {}; // private constructor for pool, not for general use

  /** bit to say whether the event pool is in use or not.
    * note that it would be trivial to make this a bitmap if we ever
    * wanted to increase the size of the pool from one.  But with our
    * current usage pattern, we almost never have more than a single
    * nsDOMEvent active in memory at a time under normal circumstances.
    */
  static PRBool gEventPoolInUse;

  //Internal helper funcs
  nsresult GetScrollInfo(nsIScrollableView** aScrollableView, float* aP2T,
                         float* aT2P);
  nsresult SetEventType(const nsAString& aEventTypeArg);
  const char* GetEventName(PRUint32 aEventType);
  already_AddRefed<nsIDOMEventTarget> GetTargetFromFrame();
  void AllocateEvent(const nsAString& aEventType);
  nsPoint GetClientPoint();
  nsPoint GetScreenPoint();

  nsEvent* mEvent;
  nsCOMPtr<nsIPresContext> mPresContext;
  nsCOMPtr<nsIDOMEventTarget> mTarget;
  nsCOMPtr<nsIDOMEventTarget> mCurrentTarget;
  nsCOMPtr<nsIDOMEventTarget> mOriginalTarget;
  nsCOMPtr<nsIDOMEventTarget> mExplicitOriginalTarget;
  nsCOMPtr<nsIDOMEventTarget> mTmpRealOriginalTarget;
  nsString*	mText;
  nsCOMPtr<nsIPrivateTextRangeList> mTextRange;
  PRPackedBool mEventIsInternal;
  PRPackedBool mEventIsTrusted;

  //These are use for internal data for user created events
  PRInt16 mButton;
  nsPoint mScreenPoint;               
  nsPoint mClientPoint;               

  void* mScriptObject;
};

#endif // nsDOMEvent_h__
