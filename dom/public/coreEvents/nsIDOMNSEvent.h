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

#ifndef nsIDOMNSEvent_h__
#define nsIDOMNSEvent_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMRenderingContext;

#define NS_IDOMNSEVENT_IID \
 { 0xa6cf90c4, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMNSEvent : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMNSEVENT_IID; return iid; }
  enum {
    EVENT_MOUSEDOWN = 1,
    EVENT_MOUSEUP = 2,
    EVENT_MOUSEOVER = 4,
    EVENT_MOUSEOUT = 8,
    EVENT_MOUSEMOVE = 16,
    EVENT_MOUSEDRAG = 32,
    EVENT_CLICK = 64,
    EVENT_DBLCLICK = 128,
    EVENT_KEYDOWN = 256,
    EVENT_KEYUP = 512,
    EVENT_KEYPRESS = 1024,
    EVENT_DRAGDROP = 2048,
    EVENT_FOCUS = 4096,
    EVENT_BLUR = 8192,
    EVENT_SELECT = 16384,
    EVENT_CHANGE = 32768,
    EVENT_RESET = 65536,
    EVENT_SUBMIT = 131072,
    EVENT_SCROLL = 262144,
    EVENT_LOAD = 524288,
    EVENT_UNLOAD = 1048576,
    EVENT_XFER_DONE = 2097152,
    EVENT_ABORT = 4194304,
    EVENT_ERROR = 8388608,
    EVENT_LOCATE = 16777216,
    EVENT_MOVE = 33554432,
    EVENT_RESIZE = 67108864,
    EVENT_FORWARD = 134217728,
    EVENT_HELP = 268435456,
    EVENT_BACK = 536870912,
    EVENT_TEXT = 1073741824,
    EVENT_ALT_MASK = 1,
    EVENT_CONTROL_MASK = 2,
    EVENT_SHIFT_MASK = 4,
    EVENT_META_MASK = 8
  };

  NS_IMETHOD    GetLayerX(PRInt32* aLayerX)=0;
  NS_IMETHOD    SetLayerX(PRInt32 aLayerX)=0;

  NS_IMETHOD    GetLayerY(PRInt32* aLayerY)=0;
  NS_IMETHOD    SetLayerY(PRInt32 aLayerY)=0;

  NS_IMETHOD    GetPageX(PRInt32* aPageX)=0;
  NS_IMETHOD    SetPageX(PRInt32 aPageX)=0;

  NS_IMETHOD    GetPageY(PRInt32* aPageY)=0;
  NS_IMETHOD    SetPageY(PRInt32 aPageY)=0;

  NS_IMETHOD    GetWhich(PRUint32* aWhich)=0;
  NS_IMETHOD    SetWhich(PRUint32 aWhich)=0;

  NS_IMETHOD    GetRc(nsIDOMRenderingContext** aRc)=0;
};


#define NS_DECL_IDOMNSEVENT   \
  NS_IMETHOD    GetLayerX(PRInt32* aLayerX);  \
  NS_IMETHOD    SetLayerX(PRInt32 aLayerX);  \
  NS_IMETHOD    GetLayerY(PRInt32* aLayerY);  \
  NS_IMETHOD    SetLayerY(PRInt32 aLayerY);  \
  NS_IMETHOD    GetPageX(PRInt32* aPageX);  \
  NS_IMETHOD    SetPageX(PRInt32 aPageX);  \
  NS_IMETHOD    GetPageY(PRInt32* aPageY);  \
  NS_IMETHOD    SetPageY(PRInt32 aPageY);  \
  NS_IMETHOD    GetWhich(PRUint32* aWhich);  \
  NS_IMETHOD    SetWhich(PRUint32 aWhich);  \
  NS_IMETHOD    GetRc(nsIDOMRenderingContext** aRc);  \



#define NS_FORWARD_IDOMNSEVENT(_to)  \
  NS_IMETHOD    GetLayerX(PRInt32* aLayerX) { return _to##GetLayerX(aLayerX); } \
  NS_IMETHOD    SetLayerX(PRInt32 aLayerX) { return _to##SetLayerX(aLayerX); } \
  NS_IMETHOD    GetLayerY(PRInt32* aLayerY) { return _to##GetLayerY(aLayerY); } \
  NS_IMETHOD    SetLayerY(PRInt32 aLayerY) { return _to##SetLayerY(aLayerY); } \
  NS_IMETHOD    GetPageX(PRInt32* aPageX) { return _to##GetPageX(aPageX); } \
  NS_IMETHOD    SetPageX(PRInt32 aPageX) { return _to##SetPageX(aPageX); } \
  NS_IMETHOD    GetPageY(PRInt32* aPageY) { return _to##GetPageY(aPageY); } \
  NS_IMETHOD    SetPageY(PRInt32 aPageY) { return _to##SetPageY(aPageY); } \
  NS_IMETHOD    GetWhich(PRUint32* aWhich) { return _to##GetWhich(aWhich); } \
  NS_IMETHOD    SetWhich(PRUint32 aWhich) { return _to##SetWhich(aWhich); } \
  NS_IMETHOD    GetRc(nsIDOMRenderingContext** aRc) { return _to##GetRc(aRc); } \


#endif // nsIDOMNSEvent_h__
