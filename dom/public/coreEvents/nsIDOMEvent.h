/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMEvent_h__
#define nsIDOMEvent_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMEventTarget;

#define NS_IDOMEVENT_IID \
 { 0xa66b7b80, 0xff46, 0xbd97, \
  { 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0x8a, 0xdd, 0x32 } } 

class nsIDOMEvent : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMEVENT_IID; return iid; }
  enum {
    CAPTURING_PHASE = 1,
    AT_TARGET = 2,
    BUBBLING_PHASE = 3,
    MOUSEDOWN = 1,
    MOUSEUP = 2,
    MOUSEOVER = 4,
    MOUSEOUT = 8,
    MOUSEMOVE = 16,
    MOUSEDRAG = 32,
    CLICK = 64,
    DBLCLICK = 128,
    KEYDOWN = 256,
    KEYUP = 512,
    KEYPRESS = 1024,
    DRAGDROP = 2048,
    FOCUS = 4096,
    BLUR = 8192,
    SELECT = 16384,
    CHANGE = 32768,
    RESET = 65536,
    SUBMIT = 131072,
    SCROLL = 262144,
    LOAD = 524288,
    UNLOAD = 1048576,
    XFER_DONE = 2097152,
    ABORT = 4194304,
    ERROR = 8388608,
    LOCATE = 16777216,
    MOVE = 33554432,
    RESIZE = 67108864,
    FORWARD = 134217728,
    HELP = 268435456,
    BACK = 536870912,
    TEXT = 1073741824,
    ALT_MASK = 1,
    CONTROL_MASK = 2,
    SHIFT_MASK = 4,
    META_MASK = 8
  };

  NS_IMETHOD    GetType(nsAWritableString& aType)=0;

  NS_IMETHOD    GetTarget(nsIDOMEventTarget** aTarget)=0;

  NS_IMETHOD    GetCurrentTarget(nsIDOMEventTarget** aCurrentTarget)=0;

  NS_IMETHOD    GetOriginalTarget(nsIDOMEventTarget** aOriginalTarget)=0;

  NS_IMETHOD    GetEventPhase(PRUint16* aEventPhase)=0;

  NS_IMETHOD    GetBubbles(PRBool* aBubbles)=0;

  NS_IMETHOD    GetCancelable(PRBool* aCancelable)=0;

  NS_IMETHOD    GetTimeStamp(PRUint64* aTimeStamp)=0;

  NS_IMETHOD    StopPropagation()=0;

  NS_IMETHOD    PreventBubble()=0;

  NS_IMETHOD    PreventCapture()=0;

  NS_IMETHOD    PreventDefault()=0;

  NS_IMETHOD    InitEvent(const nsAReadableString& aEventTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg)=0;
};


#define NS_DECL_IDOMEVENT   \
  NS_IMETHOD    GetType(nsAWritableString& aType);  \
  NS_IMETHOD    GetTarget(nsIDOMEventTarget** aTarget);  \
  NS_IMETHOD    GetCurrentTarget(nsIDOMEventTarget** aCurrentTarget);  \
  NS_IMETHOD    GetOriginalTarget(nsIDOMEventTarget** aOriginalTarget);  \
  NS_IMETHOD    GetEventPhase(PRUint16* aEventPhase);  \
  NS_IMETHOD    GetBubbles(PRBool* aBubbles);  \
  NS_IMETHOD    GetCancelable(PRBool* aCancelable);  \
  NS_IMETHOD    GetTimeStamp(PRUint64* aTimeStamp);  \
  NS_IMETHOD    StopPropagation();  \
  NS_IMETHOD    PreventBubble();  \
  NS_IMETHOD    PreventCapture();  \
  NS_IMETHOD    PreventDefault();  \
  NS_IMETHOD    InitEvent(const nsAReadableString& aEventTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg);  \



#define NS_FORWARD_IDOMEVENT(_to)  \
  NS_IMETHOD    GetType(nsAWritableString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    GetTarget(nsIDOMEventTarget** aTarget) { return _to GetTarget(aTarget); } \
  NS_IMETHOD    GetCurrentTarget(nsIDOMEventTarget** aCurrentTarget) { return _to GetCurrentTarget(aCurrentTarget); } \
  NS_IMETHOD    GetOriginalTarget(nsIDOMEventTarget** aOriginalTarget) { return _to GetOriginalTarget(aOriginalTarget); } \
  NS_IMETHOD    GetEventPhase(PRUint16* aEventPhase) { return _to GetEventPhase(aEventPhase); } \
  NS_IMETHOD    GetBubbles(PRBool* aBubbles) { return _to GetBubbles(aBubbles); } \
  NS_IMETHOD    GetCancelable(PRBool* aCancelable) { return _to GetCancelable(aCancelable); } \
  NS_IMETHOD    GetTimeStamp(PRUint64* aTimeStamp) { return _to GetTimeStamp(aTimeStamp); } \
  NS_IMETHOD    StopPropagation() { return _to StopPropagation(); }  \
  NS_IMETHOD    PreventBubble() { return _to PreventBubble(); }  \
  NS_IMETHOD    PreventCapture() { return _to PreventCapture(); }  \
  NS_IMETHOD    PreventDefault() { return _to PreventDefault(); }  \
  NS_IMETHOD    InitEvent(const nsAReadableString& aEventTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg) { return _to InitEvent(aEventTypeArg, aCanBubbleArg, aCancelableArg); }  \


extern "C" NS_DOM nsresult NS_InitEventClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptEvent(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMEvent_h__
