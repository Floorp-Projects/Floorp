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

#ifndef nsINSEvent_h__
#define nsINSEvent_h__

#include "nsISupports.h"
class nsIDOMNode;

/*
 * Base Netscape DOM event class.
 */
#define NS_INSEVENT_IID \
{ /* 64287f80-eb6a-11d1-bd85-00805f8ae3f4 */ \
0x64287f80, 0xeb6a, 0x11d1, \
{0xbd, 0x85, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsINSEvent : public nsISupports {

public:
/*
 * Event mask provided for backwards compatibility with 4.0 
 */
#define EVENT_MOUSEDOWN     0x00000001
#define EVENT_MOUSEUP       0x00000002
#define EVENT_MOUSEOVER     0x00000004
#define EVENT_MOUSEOUT      0x00000008
#define EVENT_MOUSEMOVE     0x00000010  
#define EVENT_MOUSEDRAG     0x00000020
#define EVENT_CLICK         0x00000040
#define EVENT_DBLCLICK      0x00000080
#define EVENT_KEYDOWN       0x00000100
#define EVENT_KEYUP         0x00000200
#define EVENT_KEYPRESS	    0x00000400
#define EVENT_DRAGDROP      0x00000800
#define EVENT_FOCUS         0x00001000
#define EVENT_BLUR          0x00002000
#define EVENT_SELECT        0x00004000
#define EVENT_CHANGE        0x00008000
#define EVENT_RESET         0x00010000
#define EVENT_SUBMIT        0x00020000
#define EVENT_SCROLL        0x00040000
#define EVENT_LOAD          0x00080000
#define EVENT_UNLOAD        0x00100000
#define EVENT_XFER_DONE	    0x00200000
#define EVENT_ABORT         0x00400000
#define EVENT_ERROR         0x00800000
#define EVENT_LOCATE	      0x01000000
#define EVENT_MOVE	        0x02000000
#define EVENT_RESIZE        0x04000000
#define EVENT_FORWARD       0x08000000
#define EVENT_HELP	        0x10000000
#define EVENT_BACK          0x20000000

#define	EVENT_ALT_MASK	    0x00000001
#define	EVENT_CONTROL_MASK  0x00000002
#define	EVENT_SHIFT_MASK    0x00000004
#define	EVENT_META_MASK	    0x00000008

  NS_IMETHOD GetLayerX(PRInt32& aX) = 0;
  NS_IMETHOD GetLayerY(PRInt32& aY) = 0;

};
#endif // nsINSEvent_h__
