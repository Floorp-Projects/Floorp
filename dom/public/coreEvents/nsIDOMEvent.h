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

#ifndef nsIDOMEvent_h__
#define nsIDOMEvent_h__

#include "nsISupports.h"
class nsIDOMNode;
class nsString;

/*
 * Base DOM event class.
 */
#define NS_IDOMEVENT_IID \
{ /* 9af61790-df03-11d1-bd85-00805f8ae3f4 */ \
0x9af61790, 0xdf03, 0x11d1, \
{0xbd, 0x85, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsIDOMEvent : public nsISupports {

public:
/*
 * Virtual key bindings for keyboard events
 * NOTE: These are repeated in nsGUIEvent.h and must be kept in sync
 */
#define NS_VK_CANCEL         0x03
#define NS_VK_BACK           0x08
#define NS_VK_TAB            0x09
#define NS_VK_CLEAR          0x0C
#define NS_VK_RETURN         0x0D
#define NS_VK_SHIFT          0x10
#define NS_VK_CONTROL        0x11
#define NS_VK_ALT            0x12
#define NS_VK_PAUSE          0x13
#define NS_VK_CAPS_LOCK      0x14
#define NS_VK_ESCAPE         0x1B
#define NS_VK_SPACE          0x20
#define NS_VK_PAGE_UP        0x21
#define NS_VK_PAGE_DOWN      0x22
#define NS_VK_END            0x23
#define NS_VK_HOME           0x24
#define NS_VK_LEFT           0x25
#define NS_VK_UP             0x26
#define NS_VK_RIGHT          0x27
#define NS_VK_DOWN           0x28
#define NS_VK_PRINTSCREEN    0x2C
#define NS_VK_INSERT         0x2D
#define NS_VK_DELETE         0x2E

// NS_VK_0 - NS_VK_9 match their ascii values
#define NS_VK_0              0x30
#define NS_VK_1              0x31
#define NS_VK_2              0x32
#define NS_VK_3              0x33
#define NS_VK_4              0x34
#define NS_VK_5              0x35
#define NS_VK_6              0x36
#define NS_VK_7              0x37
#define NS_VK_8              0x38
#define NS_VK_9              0x39

#define NS_VK_SEMICOLON      0x3B
#define NS_VK_EQUALS         0x3D

// NS_VK_A - NS_VK_Z match their ascii values
#define NS_VK_A              0x41
#define NS_VK_B              0x42
#define NS_VK_C              0x43
#define NS_VK_D              0x44
#define NS_VK_E              0x45
#define NS_VK_F              0x46
#define NS_VK_G              0x47
#define NS_VK_H              0x48
#define NS_VK_I              0x49
#define NS_VK_J              0x4A
#define NS_VK_K              0x4B
#define NS_VK_L              0x4C
#define NS_VK_M              0x4D
#define NS_VK_N              0x4E
#define NS_VK_O              0x4F
#define NS_VK_P              0x50
#define NS_VK_Q              0x51
#define NS_VK_R              0x52
#define NS_VK_S              0x53
#define NS_VK_T              0x54
#define NS_VK_U              0x55
#define NS_VK_V              0x56
#define NS_VK_W              0x57
#define NS_VK_X              0x58
#define NS_VK_Y              0x59
#define NS_VK_Z              0x5A

#define NS_VK_NUMPAD0        0x60
#define NS_VK_NUMPAD1        0x61
#define NS_VK_NUMPAD2        0x62
#define NS_VK_NUMPAD3        0x63
#define NS_VK_NUMPAD4        0x64
#define NS_VK_NUMPAD5        0x65
#define NS_VK_NUMPAD6        0x66
#define NS_VK_NUMPAD7        0x67
#define NS_VK_NUMPAD8        0x68
#define NS_VK_NUMPAD9        0x69
#define NS_VK_MULTIPLY       0x6A
#define NS_VK_ADD            0x6B
#define NS_VK_SEPARATOR      0x6C
#define NS_VK_SUBTRACT       0x6D
#define NS_VK_DECIMAL        0x6E
#define NS_VK_DIVIDE         0x6F
#define NS_VK_F1             0x70
#define NS_VK_F2             0x71
#define NS_VK_F3             0x72
#define NS_VK_F4             0x73
#define NS_VK_F5             0x74
#define NS_VK_F6             0x75
#define NS_VK_F7             0x76
#define NS_VK_F8             0x77
#define NS_VK_F9             0x78
#define NS_VK_F10            0x79
#define NS_VK_F11            0x7A
#define NS_VK_F12            0x7B
#define NS_VK_F13            0x7C
#define NS_VK_F14            0x7D
#define NS_VK_F15            0x7E
#define NS_VK_F16            0x7F
#define NS_VK_F17            0x80
#define NS_VK_F18            0x81
#define NS_VK_F19            0x82
#define NS_VK_F20            0x83
#define NS_VK_F21            0x84
#define NS_VK_F22            0x85
#define NS_VK_F23            0x86
#define NS_VK_F24            0x87

#define NS_VK_NUM_LOCK       0x90
#define NS_VK_SCROLL_LOCK    0x91

#define NS_VK_COMMA          0xBC
#define NS_VK_PERIOD         0xBE
#define NS_VK_SLASH          0xBF
#define NS_VK_BACK_QUOTE     0xC0
#define NS_VK_OPEN_BRACKET   0xDB
#define NS_VK_BACK_SLASH     0xDC
#define NS_VK_CLOSE_BRACKET  0xDD
#define NS_VK_QUOTE          0xDE

NS_IMETHOD GetType(nsString& aType) = 0;

NS_IMETHOD GetTarget(nsIDOMNode** aTarget) = 0;

NS_IMETHOD GetScreenX(PRInt32& aX) = 0;
NS_IMETHOD GetScreenY(PRInt32& aY) = 0;

NS_IMETHOD GetClientX(PRInt32& aX) = 0;
NS_IMETHOD GetClientY(PRInt32& aY) = 0;

NS_IMETHOD GetAltKey(PRBool& aIsDown) = 0;
NS_IMETHOD GetCtrlKey(PRBool& aIsDown) = 0;
NS_IMETHOD GetShiftKey(PRBool& aIsDown) = 0;
NS_IMETHOD GetMetaKey(PRBool& aIsDown) = 0;

NS_IMETHOD GetCharCode(PRUint32& aCharCode) = 0;
NS_IMETHOD GetKeyCode(PRUint32& aKeyCode) = 0;
NS_IMETHOD GetButton(PRUint32& aButton) = 0;

};
#endif // nsIDOMEvent_h__
