/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsDOMEvent_h__
#define nsDOMEvent_h__

#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMNSUIEvent.h"
#include "nsISupports.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIPrivateCompositionEvent.h"
#include "nsIPrivateTextEvent.h"
#include "nsIPrivateTextRange.h"
#include "nsIDOMEvent.h"

#include "nsIPresContext.h"
#include "nsPoint.h"
#include "nsGUIEvent.h"
class nsIContent;
class nsIScrollableView;

class nsDOMEvent : public nsIDOMKeyEvent, 
                   public nsIDOMMouseEvent,
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
    eDOMEvents_unload,
    eDOMEvents_abort,
    eDOMEvents_error,
    eDOMEvents_submit,
    eDOMEvents_reset,
    eDOMEvents_change,
    eDOMEvents_select,
    eDOMEvents_input,
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
    eDOMEvents_characterdatamodified
  };

  nsDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
             const nsAReadableString& aEventType);
  virtual ~nsDOMEvent();

  NS_DECL_ISUPPORTS

  // nsIDOMEvent Interface
  NS_IMETHOD    GetType(nsAWritableString& aType);
  NS_IMETHOD    GetTarget(nsIDOMEventTarget** aTarget);
  NS_IMETHOD    GetCurrentTarget(nsIDOMEventTarget** aCurrentTarget);
  NS_IMETHOD    GetOriginalTarget(nsIDOMEventTarget** aOriginalTarget);
  NS_IMETHOD    GetEventPhase(PRUint16* aEventPhase);
  NS_IMETHOD    GetBubbles(PRBool* aBubbles);
  NS_IMETHOD    GetCancelable(PRBool* aCancelable);
  NS_IMETHOD    GetTimeStamp(PRUint64* aTimestamp);
  NS_IMETHOD    StopPropagation();
  NS_IMETHOD    PreventBubble();
  NS_IMETHOD    PreventCapture();
  NS_IMETHOD    PreventDefault();
  NS_IMETHOD    InitEvent(const nsAReadableString& aEventTypeArg,
                          PRBool aCanBubbleArg, PRBool aCancelableArg);

  // nsIDOMUIEvent Interface
  NS_IMETHOD    GetView(nsIDOMAbstractView** aView);
  NS_IMETHOD    GetDetail(PRInt32* aDetail);
  NS_IMETHOD    InitUIEvent(const nsAReadableString& aTypeArg,
                            PRBool aCanBubbleArg, PRBool aCancelableArg,
                            nsIDOMAbstractView* aViewArg, PRInt32 aDetailArg);

  // nsIDOMMouseEvent Interface and nsIDOMKeyEvent Interface
  NS_IMETHOD    GetScreenX(PRInt32* aScreenX);
  NS_IMETHOD    GetScreenY(PRInt32* aScreenY);
  NS_IMETHOD    GetClientX(PRInt32* aClientX);
  NS_IMETHOD    GetClientY(PRInt32* aClientY);
  NS_IMETHOD    GetAltKey(PRBool* aAltKey);
  NS_IMETHOD    GetCtrlKey(PRBool* aCtrlKey);
  NS_IMETHOD    GetShiftKey(PRBool* aShiftKey);
  NS_IMETHOD    GetMetaKey(PRBool* aMetaKey);
  NS_IMETHOD    GetButton(PRUint16* aButton);
  NS_IMETHOD    GetRelatedTarget(nsIDOMEventTarget** aRelatedTarget);
  NS_IMETHOD    GetCharCode(PRUint32* aCharCode);
  NS_IMETHOD    GetKeyCode(PRUint32* aKeyCode);
  NS_IMETHOD    InitMouseEvent(const nsAReadableString & aTypeArg, 
                               PRBool aCanBubbleArg, PRBool aCancelableArg, 
                               nsIDOMAbstractView *aViewArg, PRUint16 aDetailArg, 
                               PRInt32 aScreenXArg, PRInt32 aDcreenYArg, 
                               PRInt32 aClientXArg, PRInt32 aClientYArg, 
                               PRBool aCtrlKeyArg, PRBool aAltKeyArg, 
                               PRBool aShiftKeyArg, PRBool aMetaKeyArg, 
                               PRUint16 aButtonArg, nsIDOMEventTarget *aRelatedTargetArg);
  NS_IMETHOD    InitKeyEvent(const nsAReadableString& aTypeArg,
                             PRBool aCanBubbleArg, PRBool aCancelableArg,
                             nsIDOMAbstractView* aViewArg, 
                             PRBool aCtrlKeyArg, PRBool aAltKeyArg,
                             PRBool aShiftKeyArg, PRBool aMetaKeyArg,
                             PRUint32 aKeyCodeArg, PRUint32 aCharCodeArg);

  // nsIDOMNSUIEvent interface
  NS_IMETHOD    GetLayerX(PRInt32* aLayerX);
  NS_IMETHOD    GetLayerY(PRInt32* aLayerY);
  NS_IMETHOD    GetPageX(PRInt32* aClientX);
  NS_IMETHOD    GetPageY(PRInt32* aClientY);
  NS_IMETHOD    GetWhich(PRUint32* aKeyCode);
  NS_IMETHOD    GetRangeParent(nsIDOMNode** aRangeParent);
  NS_IMETHOD    GetRangeOffset(PRInt32* aRangeOffset);
  NS_IMETHOD    GetCancelBubble(PRBool* aCancelBubble);
  NS_IMETHOD    SetCancelBubble(PRBool aCancelBubble);
  NS_IMETHOD    GetIsChar(PRBool* aIsChar);
  NS_IMETHOD    GetPreventDefault(PRBool* aReturn);

  // nsIPrivateDOMEvent interface
  NS_IMETHOD    DuplicatePrivateData();
  NS_IMETHOD    SetTarget(nsIDOMEventTarget* aTarget);
  NS_IMETHOD    SetCurrentTarget(nsIDOMEventTarget* aCurrentTarget);
  NS_IMETHOD    SetOriginalTarget(nsIDOMEventTarget* aOriginalTarget);
  NS_IMETHOD    IsDispatchStopped(PRBool* aIsDispatchStopped);
  NS_IMETHOD    GetInternalNSEvent(nsEvent** aNSEvent);
  NS_IMETHOD    HasOriginalTarget(PRBool* aResult);

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
  void* operator new(size_t sz);

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
  nsresult SetEventType(const nsAReadableString& aEventTypeArg);
  const char* GetEventName(PRUint32 aEventType);

  nsEvent* mEvent;
  nsIPresContext* mPresContext;
  nsIDOMEventTarget* mTarget;
  nsIDOMEventTarget* mCurrentTarget;
  nsIDOMEventTarget* mOriginalTarget;
  nsString*	mText;
  nsIPrivateTextRangeList*	mTextRange;
  PRPackedBool mEventIsInternal;

  //These are use for internal data for user created events
  PRInt16 mButton;
  nsPoint mScreenPoint;               
  nsPoint mClientPoint;               

  void* mScriptObject;
};

#endif // nsDOMEvent_h__
