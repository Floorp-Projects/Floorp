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

#ifndef nsIDOMEvent_h__
#define nsIDOMEvent_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMNode;

#define NS_IDOMEVENT_IID \
 { 0xa6cf90c3, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMEvent : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMEVENT_IID; return iid; }
  enum {
    VK_CANCEL = 3,
    VK_BACK = 8,
    VK_TAB = 9,
    VK_CLEAR = 12,
    VK_RETURN = 13,
    VK_ENTER = 14,
    VK_SHIFT = 16,
    VK_CONTROL = 17,
    VK_ALT = 18,
    VK_PAUSE = 19,
    VK_CAPS_LOCK = 20,
    VK_ESCAPE = 27,
    VK_SPACE = 32,
    VK_PAGE_UP = 33,
    VK_PAGE_DOWN = 34,
    VK_END = 35,
    VK_HOME = 36,
    VK_LEFT = 37,
    VK_UP = 38,
    VK_RIGHT = 39,
    VK_DOWN = 40,
    VK_PRINTSCREEN = 44,
    VK_INSERT = 45,
    VK_DELETE = 46,
    VK_0 = 48,
    VK_1 = 49,
    VK_2 = 50,
    VK_3 = 51,
    VK_4 = 52,
    VK_5 = 53,
    VK_6 = 54,
    VK_7 = 55,
    VK_8 = 56,
    VK_9 = 57,
    VK_SEMICOLON = 59,
    VK_EQUALS = 61,
    VK_A = 65,
    VK_B = 66,
    VK_C = 67,
    VK_D = 68,
    VK_E = 69,
    VK_F = 70,
    VK_G = 71,
    VK_H = 72,
    VK_I = 73,
    VK_J = 74,
    VK_K = 75,
    VK_L = 76,
    VK_M = 77,
    VK_N = 78,
    VK_O = 79,
    VK_P = 80,
    VK_Q = 81,
    VK_R = 82,
    VK_S = 83,
    VK_T = 84,
    VK_U = 85,
    VK_V = 86,
    VK_W = 87,
    VK_X = 88,
    VK_Y = 89,
    VK_Z = 90,
    VK_NUMPAD0 = 96,
    VK_NUMPAD1 = 97,
    VK_NUMPAD2 = 98,
    VK_NUMPAD3 = 99,
    VK_NUMPAD4 = 100,
    VK_NUMPAD5 = 101,
    VK_NUMPAD6 = 102,
    VK_NUMPAD7 = 103,
    VK_NUMPAD8 = 104,
    VK_NUMPAD9 = 105,
    VK_MULTIPLY = 106,
    VK_ADD = 107,
    VK_SEPARATOR = 108,
    VK_SUBTRACT = 109,
    VK_DECIMAL = 110,
    VK_DIVIDE = 111,
    VK_F1 = 112,
    VK_F2 = 113,
    VK_F3 = 114,
    VK_F4 = 115,
    VK_F5 = 116,
    VK_F6 = 117,
    VK_F7 = 118,
    VK_F8 = 119,
    VK_F9 = 120,
    VK_F10 = 121,
    VK_F11 = 122,
    VK_F12 = 123,
    VK_F13 = 124,
    VK_F14 = 125,
    VK_F15 = 126,
    VK_F16 = 127,
    VK_F17 = 128,
    VK_F18 = 129,
    VK_F19 = 130,
    VK_F20 = 131,
    VK_F21 = 132,
    VK_F22 = 133,
    VK_F23 = 134,
    VK_F24 = 135,
    VK_NUM_LOCK = 144,
    VK_SCROLL_LOCK = 145,
    VK_COMMA = 188,
    VK_PERIOD = 190,
    VK_SLASH = 191,
    VK_BACK_QUOTE = 192,
    VK_OPEN_BRACKET = 219,
    VK_BACK_SLASH = 220,
    VK_CLOSE_BRACKET = 221,
    VK_QUOTE = 222
  };

  NS_IMETHOD    GetType(nsString& aType)=0;
  NS_IMETHOD    SetType(const nsString& aType)=0;

  NS_IMETHOD    GetText(nsString& aText)=0;

  NS_IMETHOD    GetCommitText(PRBool* aCommitText)=0;
  NS_IMETHOD    SetCommitText(PRBool aCommitText)=0;

  NS_IMETHOD    GetTarget(nsIDOMNode** aTarget)=0;
  NS_IMETHOD    SetTarget(nsIDOMNode* aTarget)=0;

  NS_IMETHOD    GetScreenX(PRInt32* aScreenX)=0;
  NS_IMETHOD    SetScreenX(PRInt32 aScreenX)=0;

  NS_IMETHOD    GetScreenY(PRInt32* aScreenY)=0;
  NS_IMETHOD    SetScreenY(PRInt32 aScreenY)=0;

  NS_IMETHOD    GetClientX(PRInt32* aClientX)=0;
  NS_IMETHOD    SetClientX(PRInt32 aClientX)=0;

  NS_IMETHOD    GetClientY(PRInt32* aClientY)=0;
  NS_IMETHOD    SetClientY(PRInt32 aClientY)=0;

  NS_IMETHOD    GetAltKey(PRBool* aAltKey)=0;
  NS_IMETHOD    SetAltKey(PRBool aAltKey)=0;

  NS_IMETHOD    GetCtrlKey(PRBool* aCtrlKey)=0;
  NS_IMETHOD    SetCtrlKey(PRBool aCtrlKey)=0;

  NS_IMETHOD    GetShiftKey(PRBool* aShiftKey)=0;
  NS_IMETHOD    SetShiftKey(PRBool aShiftKey)=0;

  NS_IMETHOD    GetMetaKey(PRBool* aMetaKey)=0;
  NS_IMETHOD    SetMetaKey(PRBool aMetaKey)=0;

  NS_IMETHOD    GetCancelBubble(PRBool* aCancelBubble)=0;
  NS_IMETHOD    SetCancelBubble(PRBool aCancelBubble)=0;

  NS_IMETHOD    GetCharCode(PRUint32* aCharCode)=0;
  NS_IMETHOD    SetCharCode(PRUint32 aCharCode)=0;

  NS_IMETHOD    GetKeyCode(PRUint32* aKeyCode)=0;
  NS_IMETHOD    SetKeyCode(PRUint32 aKeyCode)=0;

  NS_IMETHOD    GetButton(PRUint32* aButton)=0;
  NS_IMETHOD    SetButton(PRUint32 aButton)=0;
};


#define NS_DECL_IDOMEVENT   \
  NS_IMETHOD    GetType(nsString& aType);  \
  NS_IMETHOD    SetType(const nsString& aType);  \
  NS_IMETHOD    GetText(nsString& aText);  \
  NS_IMETHOD    GetCommitText(PRBool* aCommitText);  \
  NS_IMETHOD    SetCommitText(PRBool aCommitText);  \
  NS_IMETHOD    GetTarget(nsIDOMNode** aTarget);  \
  NS_IMETHOD    SetTarget(nsIDOMNode* aTarget);  \
  NS_IMETHOD    GetScreenX(PRInt32* aScreenX);  \
  NS_IMETHOD    SetScreenX(PRInt32 aScreenX);  \
  NS_IMETHOD    GetScreenY(PRInt32* aScreenY);  \
  NS_IMETHOD    SetScreenY(PRInt32 aScreenY);  \
  NS_IMETHOD    GetClientX(PRInt32* aClientX);  \
  NS_IMETHOD    SetClientX(PRInt32 aClientX);  \
  NS_IMETHOD    GetClientY(PRInt32* aClientY);  \
  NS_IMETHOD    SetClientY(PRInt32 aClientY);  \
  NS_IMETHOD    GetAltKey(PRBool* aAltKey);  \
  NS_IMETHOD    SetAltKey(PRBool aAltKey);  \
  NS_IMETHOD    GetCtrlKey(PRBool* aCtrlKey);  \
  NS_IMETHOD    SetCtrlKey(PRBool aCtrlKey);  \
  NS_IMETHOD    GetShiftKey(PRBool* aShiftKey);  \
  NS_IMETHOD    SetShiftKey(PRBool aShiftKey);  \
  NS_IMETHOD    GetMetaKey(PRBool* aMetaKey);  \
  NS_IMETHOD    SetMetaKey(PRBool aMetaKey);  \
  NS_IMETHOD    GetCancelBubble(PRBool* aCancelBubble);  \
  NS_IMETHOD    SetCancelBubble(PRBool aCancelBubble);  \
  NS_IMETHOD    GetCharCode(PRUint32* aCharCode);  \
  NS_IMETHOD    SetCharCode(PRUint32 aCharCode);  \
  NS_IMETHOD    GetKeyCode(PRUint32* aKeyCode);  \
  NS_IMETHOD    SetKeyCode(PRUint32 aKeyCode);  \
  NS_IMETHOD    GetButton(PRUint32* aButton);  \
  NS_IMETHOD    SetButton(PRUint32 aButton);  \



#define NS_FORWARD_IDOMEVENT(_to)  \
  NS_IMETHOD    GetType(nsString& aType) { return _to##GetType(aType); } \
  NS_IMETHOD    SetType(const nsString& aType) { return _to##SetType(aType); } \
  NS_IMETHOD    GetText(nsString& aText) { return _to##GetText(aText); } \
  NS_IMETHOD    GetCommitText(PRBool* aCommitText) { return _to##GetCommitText(aCommitText); } \
  NS_IMETHOD    SetCommitText(PRBool aCommitText) { return _to##SetCommitText(aCommitText); } \
  NS_IMETHOD    GetTarget(nsIDOMNode** aTarget) { return _to##GetTarget(aTarget); } \
  NS_IMETHOD    SetTarget(nsIDOMNode* aTarget) { return _to##SetTarget(aTarget); } \
  NS_IMETHOD    GetScreenX(PRInt32* aScreenX) { return _to##GetScreenX(aScreenX); } \
  NS_IMETHOD    SetScreenX(PRInt32 aScreenX) { return _to##SetScreenX(aScreenX); } \
  NS_IMETHOD    GetScreenY(PRInt32* aScreenY) { return _to##GetScreenY(aScreenY); } \
  NS_IMETHOD    SetScreenY(PRInt32 aScreenY) { return _to##SetScreenY(aScreenY); } \
  NS_IMETHOD    GetClientX(PRInt32* aClientX) { return _to##GetClientX(aClientX); } \
  NS_IMETHOD    SetClientX(PRInt32 aClientX) { return _to##SetClientX(aClientX); } \
  NS_IMETHOD    GetClientY(PRInt32* aClientY) { return _to##GetClientY(aClientY); } \
  NS_IMETHOD    SetClientY(PRInt32 aClientY) { return _to##SetClientY(aClientY); } \
  NS_IMETHOD    GetAltKey(PRBool* aAltKey) { return _to##GetAltKey(aAltKey); } \
  NS_IMETHOD    SetAltKey(PRBool aAltKey) { return _to##SetAltKey(aAltKey); } \
  NS_IMETHOD    GetCtrlKey(PRBool* aCtrlKey) { return _to##GetCtrlKey(aCtrlKey); } \
  NS_IMETHOD    SetCtrlKey(PRBool aCtrlKey) { return _to##SetCtrlKey(aCtrlKey); } \
  NS_IMETHOD    GetShiftKey(PRBool* aShiftKey) { return _to##GetShiftKey(aShiftKey); } \
  NS_IMETHOD    SetShiftKey(PRBool aShiftKey) { return _to##SetShiftKey(aShiftKey); } \
  NS_IMETHOD    GetMetaKey(PRBool* aMetaKey) { return _to##GetMetaKey(aMetaKey); } \
  NS_IMETHOD    SetMetaKey(PRBool aMetaKey) { return _to##SetMetaKey(aMetaKey); } \
  NS_IMETHOD    GetCancelBubble(PRBool* aCancelBubble) { return _to##GetCancelBubble(aCancelBubble); } \
  NS_IMETHOD    SetCancelBubble(PRBool aCancelBubble) { return _to##SetCancelBubble(aCancelBubble); } \
  NS_IMETHOD    GetCharCode(PRUint32* aCharCode) { return _to##GetCharCode(aCharCode); } \
  NS_IMETHOD    SetCharCode(PRUint32 aCharCode) { return _to##SetCharCode(aCharCode); } \
  NS_IMETHOD    GetKeyCode(PRUint32* aKeyCode) { return _to##GetKeyCode(aKeyCode); } \
  NS_IMETHOD    SetKeyCode(PRUint32 aKeyCode) { return _to##SetKeyCode(aKeyCode); } \
  NS_IMETHOD    GetButton(PRUint32* aButton) { return _to##GetButton(aButton); } \
  NS_IMETHOD    SetButton(PRUint32 aButton) { return _to##SetButton(aButton); } \


extern "C" NS_DOM nsresult NS_InitEventClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptEvent(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMEvent_h__
