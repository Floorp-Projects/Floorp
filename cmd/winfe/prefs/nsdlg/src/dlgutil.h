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

#ifndef __DLGUTIL_H_
#define __DLGUTIL_H_

#ifndef _WIN32

#define DPBCS_PUSHED             0x0001
#define DPBCS_ADJUSTRECT         0x0010

// Simple routine that's similar to DrawFrameControl() except
// this one only works for push button controls
BOOL
DrawPushButtonControl(HDC hdc, LPRECT lprc, UINT nState);
#endif

#endif /* __DLGUTIL_H_ */

