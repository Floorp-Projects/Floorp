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

#ifndef nsIDOMKeyEvent_h__
#define nsIDOMKeyEvent_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMUIEvent.h"

class nsIDOMAbstractView;

#define NS_IDOMKEYEVENT_IID \
 { 0x028e0e6e, 0x8b01, 0x11d3, \
  { 0xaa, 0xe7, 0x00, 0x10, 0x83, 0x8a, 0x31, 0x23 } } 

class nsIDOMKeyEvent : public nsIDOMUIEvent {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMKEYEVENT_IID; return iid; }
  enum {
    DOM_CHAR_UNDEFINED = 65535,
    DOM_VK_0 = 48,
    DOM_VK_1 = 49,
    DOM_VK_2 = 50,
    DOM_VK_3 = 51,
    DOM_VK_4 = 52,
    DOM_VK_5 = 53,
    DOM_VK_6 = 54,
    DOM_VK_7 = 55,
    DOM_VK_8 = 56,
    DOM_VK_9 = 57,
    DOM_VK_A = 65,
    DOM_VK_ACCEPT = 30,
    DOM_VK_ADD = 107,
    DOM_VK_AGAIN = 65481,
    DOM_VK_ALL_CANDIDATES = 256,
    DOM_VK_ALPHANUMERIC = 240,
    DOM_VK_ALT = 18,
    DOM_VK_ALT_GRAPH = 65406,
    DOM_VK_AMPERSAND = 150,
    DOM_VK_ASTERISK = 151,
    DOM_VK_AT = 512,
    DOM_VK_B = 66,
    DOM_VK_BACK_QUOTE = 192,
    DOM_VK_BACK_SLASH = 92,
    DOM_VK_BACK_SPACE = 8,
    DOM_VK_BRACELEFT = 161,
    DOM_VK_BRACERIGHT = 162,
    DOM_VK_C = 67,
    DOM_VK_CANCEL = 3,
    DOM_VK_CAPS_LOCK = 20,
    DOM_VK_CIRCUMFLEX = 514,
    DOM_VK_CLEAR = 12,
    DOM_VK_CLOSE_BRACKET = 93,
    DOM_VK_CODE_INPUT = 258,
    DOM_VK_COLON = 513,
    DOM_VK_COMMA = 44,
    DOM_VK_COMPOSE = 65312,
    DOM_VK_CONTROL = 17,
    DOM_VK_CONVERT = 28,
    DOM_VK_COPY = 65485,
    DOM_VK_CUT = 65489,
    DOM_VK_D = 68,
    DOM_VK_DEAD_ABOVEDOT = 134,
    DOM_VK_DEAD_ABOVERING = 136,
    DOM_VK_DEAD_ACUTE = 129,
    DOM_VK_DEAD_BREVE = 133,
    DOM_VK_DEAD_CARON = 138,
    DOM_VK_DEAD_CEDILLA = 139,
    DOM_VK_DEAD_CIRCUMFLEX = 130,
    DOM_VK_DEAD_DIAERESIS = 135,
    DOM_VK_DEAD_DOUBLEACUTE = 137,
    DOM_VK_DEAD_GRAVE = 128,
    DOM_VK_DEAD_IOTA = 141,
    DOM_VK_DEAD_MACRON = 132,
    DOM_VK_DEAD_OGONEK = 140,
    DOM_VK_DEAD_SEMIVOICED_SOUND = 143,
    DOM_VK_DEAD_TILDE = 131,
    DOM_VK_DEAD_VOICED_SOUND = 142,
    DOM_VK_DECIMAL = 110,
    DOM_VK_DELETE = 127,
    DOM_VK_DIVIDE = 111,
    DOM_VK_DOLLAR = 515,
    DOM_VK_DOWN = 40,
    DOM_VK_E = 69,
    DOM_VK_END = 35,
    DOM_VK_ENTER = 13,
    DOM_VK_EQUALS = 61,
    DOM_VK_ESCAPE = 27,
    DOM_VK_EURO_SIGN = 516,
    DOM_VK_EXCLAMATION_MARK = 517,
    DOM_VK_F = 70,
    DOM_VK_F1 = 112,
    DOM_VK_F10 = 121,
    DOM_VK_F11 = 122,
    DOM_VK_F12 = 123,
    DOM_VK_F13 = 61440,
    DOM_VK_F14 = 61441,
    DOM_VK_F15 = 61442,
    DOM_VK_F16 = 61443,
    DOM_VK_F17 = 61444,
    DOM_VK_F18 = 61445,
    DOM_VK_F19 = 61446,
    DOM_VK_F2 = 113,
    DOM_VK_F20 = 61447,
    DOM_VK_F21 = 61448,
    DOM_VK_F22 = 61449,
    DOM_VK_F23 = 61450,
    DOM_VK_F24 = 61451,
    DOM_VK_F3 = 114,
    DOM_VK_F4 = 115,
    DOM_VK_F5 = 116,
    DOM_VK_F6 = 117,
    DOM_VK_F7 = 118,
    DOM_VK_F8 = 119,
    DOM_VK_F9 = 120,
    DOM_VK_FINAL = 24,
    DOM_VK_FIND = 65488,
    DOM_VK_FULL_WIDTH = 243,
    DOM_VK_G = 71,
    DOM_VK_GREATER = 160,
    DOM_VK_H = 72,
    DOM_VK_HALF_WIDTH = 244,
    DOM_VK_HELP = 156,
    DOM_VK_HIRAGANA = 242,
    DOM_VK_HOME = 36,
    DOM_VK_I = 73,
    DOM_VK_INSERT = 155,
    DOM_VK_INVERTED_EXCLAMATION_MARK = 518,
    DOM_VK_J = 74,
    DOM_VK_JAPANESE_HIRAGANA = 260,
    DOM_VK_JAPANESE_KATAKANA = 259,
    DOM_VK_JAPANESE_ROMAN = 261,
    DOM_VK_K = 75,
    DOM_VK_KANA = 21,
    DOM_VK_KANJI = 25,
    DOM_VK_KATAKANA = 241,
    DOM_VK_KP_DOWN = 225,
    DOM_VK_KP_LEFT = 226,
    DOM_VK_KP_RIGHT = 227,
    DOM_VK_KP_UP = 224,
    DOM_VK_L = 76,
    DOM_VK_LEFT = 37,
    DOM_VK_LEFT_PARENTHESIS = 519,
    DOM_VK_LESS = 153,
    DOM_VK_M = 77,
    DOM_VK_META = 157,
    DOM_VK_MINUS = 45,
    DOM_VK_MODECHANGE = 31,
    DOM_VK_MULTIPLY = 106,
    DOM_VK_N = 78,
    DOM_VK_NONCONVERT = 29,
    DOM_VK_NUM_LOCK = 144,
    DOM_VK_NUMBER_SIGN = 520,
    DOM_VK_NUMPAD0 = 96,
    DOM_VK_NUMPAD1 = 97,
    DOM_VK_NUMPAD2 = 98,
    DOM_VK_NUMPAD3 = 99,
    DOM_VK_NUMPAD4 = 100,
    DOM_VK_NUMPAD5 = 101,
    DOM_VK_NUMPAD6 = 102,
    DOM_VK_NUMPAD7 = 103,
    DOM_VK_NUMPAD8 = 104,
    DOM_VK_NUMPAD9 = 105,
    DOM_VK_O = 79,
    DOM_VK_OPEN_BRACKET = 91,
    DOM_VK_P = 80,
    DOM_VK_PAGE_DOWN = 34,
    DOM_VK_PAGE_UP = 33,
    DOM_VK_PASTE = 65487,
    DOM_VK_PAUSE = 19,
    DOM_VK_PERIOD = 46,
    DOM_VK_PLUS = 521,
    DOM_VK_PREVIOUS_CANDIDATE = 257,
    DOM_VK_PRINTSCREEN = 154,
    DOM_VK_PROPS = 65482,
    DOM_VK_Q = 81,
    DOM_VK_QUOTE = 222,
    DOM_VK_QUOTEDBL = 152,
    DOM_VK_R = 82,
    DOM_VK_RETURN = 14,
    DOM_VK_RIGHT = 39,
    DOM_VK_RIGHT_PARENTHESIS = 522,
    DOM_VK_ROMAN_CHARACTERS = 245,
    DOM_VK_S = 83,
    DOM_VK_SCROLL_LOCK = 145,
    DOM_VK_SEMICOLON = 59,
    DOM_VK_SEPARATER = 108,
    DOM_VK_SHIFT = 16,
    DOM_VK_SLASH = 47,
    DOM_VK_SPACE = 32,
    DOM_VK_STOP = 65480,
    DOM_VK_SUBTRACT = 109,
    DOM_VK_T = 84,
    DOM_VK_TAB = 9,
    DOM_VK_U = 85,
    DOM_VK_UNDEFINED = 0,
    DOM_VK_UNDERSCORE = 523,
    DOM_VK_UNDO = 65483,
    DOM_VK_UP = 38,
    DOM_VK_V = 86,
    DOM_VK_W = 87,
    DOM_VK_X = 88,
    DOM_VK_Y = 89,
    DOM_VK_Z = 90
  };

  NS_IMETHOD    GetCharCode(PRUint32* aCharCode)=0;

  NS_IMETHOD    GetKeyCode(PRUint32* aKeyCode)=0;

  NS_IMETHOD    GetAltKey(PRBool* aAltKey)=0;

  NS_IMETHOD    GetCtrlKey(PRBool* aCtrlKey)=0;

  NS_IMETHOD    GetShiftKey(PRBool* aShiftKey)=0;

  NS_IMETHOD    GetMetaKey(PRBool* aMetaKey)=0;

  NS_IMETHOD    InitKeyEvent(const nsString& aTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg, PRBool aCtrlKeyArg, PRBool aAltKeyArg, PRBool aShiftKeyArg, PRBool aMetaKeyArg, PRUint32 aKeyCodeArg, PRUint32 aCharCodeArg, nsIDOMAbstractView* aViewArg)=0;
};


#define NS_DECL_IDOMKEYEVENT   \
  NS_IMETHOD    GetCharCode(PRUint32* aCharCode);  \
  NS_IMETHOD    GetKeyCode(PRUint32* aKeyCode);  \
  NS_IMETHOD    GetAltKey(PRBool* aAltKey);  \
  NS_IMETHOD    GetCtrlKey(PRBool* aCtrlKey);  \
  NS_IMETHOD    GetShiftKey(PRBool* aShiftKey);  \
  NS_IMETHOD    GetMetaKey(PRBool* aMetaKey);  \
  NS_IMETHOD    InitKeyEvent(const nsString& aTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg, PRBool aCtrlKeyArg, PRBool aAltKeyArg, PRBool aShiftKeyArg, PRBool aMetaKeyArg, PRUint32 aKeyCodeArg, PRUint32 aCharCodeArg, nsIDOMAbstractView* aViewArg);  \



#define NS_FORWARD_IDOMKEYEVENT(_to)  \
  NS_IMETHOD    GetCharCode(PRUint32* aCharCode) { return _to GetCharCode(aCharCode); } \
  NS_IMETHOD    GetKeyCode(PRUint32* aKeyCode) { return _to GetKeyCode(aKeyCode); } \
  NS_IMETHOD    GetAltKey(PRBool* aAltKey) { return _to GetAltKey(aAltKey); } \
  NS_IMETHOD    GetCtrlKey(PRBool* aCtrlKey) { return _to GetCtrlKey(aCtrlKey); } \
  NS_IMETHOD    GetShiftKey(PRBool* aShiftKey) { return _to GetShiftKey(aShiftKey); } \
  NS_IMETHOD    GetMetaKey(PRBool* aMetaKey) { return _to GetMetaKey(aMetaKey); } \
  NS_IMETHOD    InitKeyEvent(const nsString& aTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg, PRBool aCtrlKeyArg, PRBool aAltKeyArg, PRBool aShiftKeyArg, PRBool aMetaKeyArg, PRUint32 aKeyCodeArg, PRUint32 aCharCodeArg, nsIDOMAbstractView* aViewArg) { return _to InitKeyEvent(aTypeArg, aCanBubbleArg, aCancelableArg, aCtrlKeyArg, aAltKeyArg, aShiftKeyArg, aMetaKeyArg, aKeyCodeArg, aCharCodeArg, aViewArg); }  \


extern "C" NS_DOM nsresult NS_InitKeyEventClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptKeyEvent(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMKeyEvent_h__
