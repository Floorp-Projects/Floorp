/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef __TIMER_H
//	Avoid include redundancy
//
#define __TIMER_H

//	Purpose:    Centralize timer calls to one location.
//	Comments:   Destroy all network timer code currently existing.
//	Revision History:
//      03-16-95    created GAB
//

//	Required Includes
//

//	Constants
//

//	Structures
//

//	Global variables
//

//	Macros
//

//	Function declarations
//
void CALLBACK EXPORT FireTimeout(HWND hWnd, UINT uMessage, UINT uTimerID, DWORD dwTime);

// walk down timer list and fire appropriate timers
void wfe_ProcessTimeouts(DWORD dwTickCount = 0);

#endif // __TIMER_H
