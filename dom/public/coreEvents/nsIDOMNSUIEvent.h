/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMNSUIEvent_h__
#define nsIDOMNSUIEvent_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMNode;

#define NS_IDOMNSUIEVENT_IID \
 { 0xa6cf90c4, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMNSUIEvent : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMNSUIEVENT_IID; return iid; }
  enum {
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

  NS_IMETHOD    GetLayerX(PRInt32* aLayerX)=0;

  NS_IMETHOD    GetLayerY(PRInt32* aLayerY)=0;

  NS_IMETHOD    GetPageX(PRInt32* aPageX)=0;

  NS_IMETHOD    GetPageY(PRInt32* aPageY)=0;

  NS_IMETHOD    GetWhich(PRUint32* aWhich)=0;

  NS_IMETHOD    GetRangeParent(nsIDOMNode** aRangeParent)=0;

  NS_IMETHOD    GetRangeOffset(PRInt32* aRangeOffset)=0;

  NS_IMETHOD    GetCancelBubble(PRBool* aCancelBubble)=0;
  NS_IMETHOD    SetCancelBubble(PRBool aCancelBubble)=0;
};


#define NS_DECL_IDOMNSUIEVENT   \
  NS_IMETHOD    GetLayerX(PRInt32* aLayerX);  \
  NS_IMETHOD    GetLayerY(PRInt32* aLayerY);  \
  NS_IMETHOD    GetPageX(PRInt32* aPageX);  \
  NS_IMETHOD    GetPageY(PRInt32* aPageY);  \
  NS_IMETHOD    GetWhich(PRUint32* aWhich);  \
  NS_IMETHOD    GetRangeParent(nsIDOMNode** aRangeParent);  \
  NS_IMETHOD    GetRangeOffset(PRInt32* aRangeOffset);  \
  NS_IMETHOD    GetCancelBubble(PRBool* aCancelBubble);  \
  NS_IMETHOD    SetCancelBubble(PRBool aCancelBubble);  \



#define NS_FORWARD_IDOMNSUIEVENT(_to)  \
  NS_IMETHOD    GetLayerX(PRInt32* aLayerX) { return _to GetLayerX(aLayerX); } \
  NS_IMETHOD    GetLayerY(PRInt32* aLayerY) { return _to GetLayerY(aLayerY); } \
  NS_IMETHOD    GetPageX(PRInt32* aPageX) { return _to GetPageX(aPageX); } \
  NS_IMETHOD    GetPageY(PRInt32* aPageY) { return _to GetPageY(aPageY); } \
  NS_IMETHOD    GetWhich(PRUint32* aWhich) { return _to GetWhich(aWhich); } \
  NS_IMETHOD    GetRangeParent(nsIDOMNode** aRangeParent) { return _to GetRangeParent(aRangeParent); } \
  NS_IMETHOD    GetRangeOffset(PRInt32* aRangeOffset) { return _to GetRangeOffset(aRangeOffset); } \
  NS_IMETHOD    GetCancelBubble(PRBool* aCancelBubble) { return _to GetCancelBubble(aCancelBubble); } \
  NS_IMETHOD    SetCancelBubble(PRBool aCancelBubble) { return _to SetCancelBubble(aCancelBubble); } \


#endif // nsIDOMNSUIEvent_h__
