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

#include "nsKeyCode.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>

#include "nsIDOMKeyEvent.h"

struct nsKeyConverter 
{
  int          vkCode; // Platform independent key code
  unsigned int keysym; // XK keysym key code
};

struct nsKeyConverter nsKeycodes[] =
{
  { nsIDOMKeyEvent::DOM_VK_CANCEL,        XK_Cancel},
  { nsIDOMKeyEvent::DOM_VK_BACK_SPACE,    XK_BackSpace},
  { nsIDOMKeyEvent::DOM_VK_TAB,           XK_Tab},
#ifdef XK_ISO_Left_Tab
  { nsIDOMKeyEvent::DOM_VK_TAB,           XK_ISO_Left_Tab }, // SunOS 5.5.1 doesn't have it.
#endif
  { nsIDOMKeyEvent::DOM_VK_CLEAR,         XK_Clear},
  { nsIDOMKeyEvent::DOM_VK_RETURN,        XK_Return},
  { nsIDOMKeyEvent::DOM_VK_ENTER,         XK_KP_Enter},
  { nsIDOMKeyEvent::DOM_VK_SHIFT,         XK_Shift_L},
  { nsIDOMKeyEvent::DOM_VK_SHIFT,         XK_Shift_R},
  { nsIDOMKeyEvent::DOM_VK_CONTROL,       XK_Control_L},
  { nsIDOMKeyEvent::DOM_VK_CONTROL,       XK_Control_R},
  { nsIDOMKeyEvent::DOM_VK_ALT,           XK_Alt_L},
  { nsIDOMKeyEvent::DOM_VK_ALT,           XK_Alt_R},
  { nsIDOMKeyEvent::DOM_VK_PAUSE,         XK_Pause},
  { nsIDOMKeyEvent::DOM_VK_CAPS_LOCK,     XK_Caps_Lock},
  { nsIDOMKeyEvent::DOM_VK_ESCAPE,        XK_Escape},
  { nsIDOMKeyEvent::DOM_VK_SPACE,         XK_KP_Space},
  { nsIDOMKeyEvent::DOM_VK_PAGE_UP,       XK_Page_Up},
  { nsIDOMKeyEvent::DOM_VK_PAGE_UP,       XK_KP_Page_Up},
  { nsIDOMKeyEvent::DOM_VK_PAGE_DOWN,     XK_Page_Down},
  { nsIDOMKeyEvent::DOM_VK_PAGE_DOWN,     XK_KP_Page_Down},
  { nsIDOMKeyEvent::DOM_VK_END,           XK_End},
  { nsIDOMKeyEvent::DOM_VK_END,           XK_KP_End},
  { nsIDOMKeyEvent::DOM_VK_HOME,          XK_Home},
  { nsIDOMKeyEvent::DOM_VK_HOME,          XK_KP_Home},
  { nsIDOMKeyEvent::DOM_VK_LEFT,          XK_Left},
  { nsIDOMKeyEvent::DOM_VK_LEFT,          XK_KP_Left},
  { nsIDOMKeyEvent::DOM_VK_UP,            XK_Up},
  { nsIDOMKeyEvent::DOM_VK_UP,            XK_KP_Up},
  { nsIDOMKeyEvent::DOM_VK_RIGHT,         XK_Right},
  { nsIDOMKeyEvent::DOM_VK_RIGHT,         XK_KP_Right},
  { nsIDOMKeyEvent::DOM_VK_DOWN,          XK_Down},
  { nsIDOMKeyEvent::DOM_VK_DOWN,          XK_KP_Down},
  { nsIDOMKeyEvent::DOM_VK_PRINTSCREEN,   XK_Print},
  { nsIDOMKeyEvent::DOM_VK_INSERT,        XK_Insert},
  { nsIDOMKeyEvent::DOM_VK_INSERT,        XK_KP_Insert},
  { nsIDOMKeyEvent::DOM_VK_DELETE,        XK_Delete},
  { nsIDOMKeyEvent::DOM_VK_DELETE,        XK_KP_Delete},
  { nsIDOMKeyEvent::DOM_VK_0,             XK_0},
  { nsIDOMKeyEvent::DOM_VK_1,             XK_1},
  { nsIDOMKeyEvent::DOM_VK_2,             XK_2},
  { nsIDOMKeyEvent::DOM_VK_3,             XK_3},
  { nsIDOMKeyEvent::DOM_VK_4,             XK_4},
  { nsIDOMKeyEvent::DOM_VK_5,             XK_5},
  { nsIDOMKeyEvent::DOM_VK_6,             XK_6},
  { nsIDOMKeyEvent::DOM_VK_7,             XK_7},
  { nsIDOMKeyEvent::DOM_VK_8,             XK_8},
  { nsIDOMKeyEvent::DOM_VK_9,             XK_9},
  { nsIDOMKeyEvent::DOM_VK_SEMICOLON,     XK_semicolon},
  { nsIDOMKeyEvent::DOM_VK_EQUALS,        XK_equal},
  { nsIDOMKeyEvent::DOM_VK_NUMPAD0,       XK_KP_0},
  { nsIDOMKeyEvent::DOM_VK_NUMPAD1,       XK_KP_1},
  { nsIDOMKeyEvent::DOM_VK_NUMPAD2,       XK_KP_2},
  { nsIDOMKeyEvent::DOM_VK_NUMPAD3,       XK_KP_3},
  { nsIDOMKeyEvent::DOM_VK_NUMPAD4,       XK_KP_4},
  { nsIDOMKeyEvent::DOM_VK_NUMPAD5,       XK_KP_5},
  { nsIDOMKeyEvent::DOM_VK_NUMPAD6,       XK_KP_6},
  { nsIDOMKeyEvent::DOM_VK_NUMPAD7,       XK_KP_7},
  { nsIDOMKeyEvent::DOM_VK_NUMPAD8,       XK_KP_8},
  { nsIDOMKeyEvent::DOM_VK_NUMPAD9,       XK_KP_9},
  { nsIDOMKeyEvent::DOM_VK_MULTIPLY,      XK_KP_Multiply},
  { nsIDOMKeyEvent::DOM_VK_ADD,           XK_KP_Add},
  { nsIDOMKeyEvent::DOM_VK_SEPARATOR,     XK_KP_Separator},
  { nsIDOMKeyEvent::DOM_VK_SUBTRACT,      XK_KP_Subtract},
  { nsIDOMKeyEvent::DOM_VK_DECIMAL,       XK_KP_Decimal},
  { nsIDOMKeyEvent::DOM_VK_DIVIDE,        XK_KP_Divide},
  { nsIDOMKeyEvent::DOM_VK_F1,            XK_F1},
  { nsIDOMKeyEvent::DOM_VK_F2,            XK_F2},
  { nsIDOMKeyEvent::DOM_VK_F3,            XK_F3},
  { nsIDOMKeyEvent::DOM_VK_F4,            XK_F4},
  { nsIDOMKeyEvent::DOM_VK_F5,            XK_F5},
  { nsIDOMKeyEvent::DOM_VK_F6,            XK_F6},
  { nsIDOMKeyEvent::DOM_VK_F7,            XK_F7},
  { nsIDOMKeyEvent::DOM_VK_F8,            XK_F8},
  { nsIDOMKeyEvent::DOM_VK_F9,            XK_F9},
  { nsIDOMKeyEvent::DOM_VK_F10,           XK_F10},
  { nsIDOMKeyEvent::DOM_VK_F11,           XK_F11},
  { nsIDOMKeyEvent::DOM_VK_F12,           XK_F12},
  { nsIDOMKeyEvent::DOM_VK_F13,           XK_F13},
  { nsIDOMKeyEvent::DOM_VK_F14,           XK_F14},
  { nsIDOMKeyEvent::DOM_VK_F15,           XK_F15},
  { nsIDOMKeyEvent::DOM_VK_F16,           XK_F16},
  { nsIDOMKeyEvent::DOM_VK_F17,           XK_F17},
  { nsIDOMKeyEvent::DOM_VK_F18,           XK_F18},
  { nsIDOMKeyEvent::DOM_VK_F19,           XK_F19},
  { nsIDOMKeyEvent::DOM_VK_F20,           XK_F20},
  { nsIDOMKeyEvent::DOM_VK_F21,           XK_F21},
  { nsIDOMKeyEvent::DOM_VK_F22,           XK_F22},
  { nsIDOMKeyEvent::DOM_VK_F23,           XK_F23},
  { nsIDOMKeyEvent::DOM_VK_F24,           XK_F24},
  { nsIDOMKeyEvent::DOM_VK_NUM_LOCK,      XK_Num_Lock},
  { nsIDOMKeyEvent::DOM_VK_SCROLL_LOCK,   XK_Scroll_Lock},
  { nsIDOMKeyEvent::DOM_VK_COMMA,         XK_comma},
  { nsIDOMKeyEvent::DOM_VK_PERIOD,        XK_period},
  { nsIDOMKeyEvent::DOM_VK_SLASH,         XK_slash},
  { nsIDOMKeyEvent::DOM_VK_BACK_QUOTE,    XK_grave},
  { nsIDOMKeyEvent::DOM_VK_OPEN_BRACKET,  XK_bracketleft},
  { nsIDOMKeyEvent::DOM_VK_BACK_SLASH,    XK_backslash},
  { nsIDOMKeyEvent::DOM_VK_CLOSE_BRACKET, XK_bracketright},
  { nsIDOMKeyEvent::DOM_VK_QUOTE,         XK_apostrophe}
};

PRInt32
nsKeyCode::ConvertKeySymToVirtualKey(KeySym keysym)
{
  unsigned int i;
  unsigned int length = sizeof(nsKeycodes) / sizeof(struct nsKeyConverter);

  if (keysym >= XK_a && keysym <= XK_z)
    return keysym - XK_a + nsIDOMKeyEvent::DOM_VK_A;
  if (keysym >= XK_A && keysym <= XK_Z)
    return keysym - XK_A + nsIDOMKeyEvent::DOM_VK_A;

  for (i = 0; i < length; i++) 
  {
    if (nsKeycodes[i].keysym == keysym)
    {
      return(nsKeycodes[i].vkCode);
    }
  }
  
  return((int)0);
}

//////////////////////////////////////////////////////////////////////////
/* static */ PRBool
nsKeyCode::KeyCodeIsModifier(KeyCode aKeyCode)
{
  if (aKeyCode == XK_Shift_L ||
	  aKeyCode == XK_Shift_R ||
	  aKeyCode == XK_Control_L ||
	  aKeyCode == XK_Control_R ||
	  aKeyCode == XK_Caps_Lock ||
	  aKeyCode == XK_Shift_Lock ||
	  aKeyCode == XK_Meta_L ||
	  aKeyCode == XK_Meta_R ||
	  aKeyCode == XK_Alt_L ||
	  aKeyCode == XK_Alt_R)
  {
	return PR_TRUE;
  }

  return PR_FALSE;
}
//////////////////////////////////////////////////////////////////////////
/* static */ KeySym
nsKeyCode::ConvertKeyCodeToKeySym(Display * aDisplay,
								  KeyCode   aKeyCode)
{
  KeySym keysym = 0;

  keysym = XKeycodeToKeysym(aDisplay, aKeyCode, 0);

  return keysym;
}
//////////////////////////////////////////////////////////////////////////
