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
#include "stdafx.h"

size_t stRASENTRY = 0;
size_t stRASCONN = 0;
size_t stRASCTRYINFO = 0;
size_t stRASDIALPARAMS = 0;
size_t stRASDEVINFO = 0;

#ifdef WINVER
#undef WINVER
#endif
#define WINVER 0x0400

#include <windows.h>
#include <ras.h>

void SizeofRAS95()	{
	stRASENTRY = sizeof(RASENTRY);
	stRASCONN = sizeof(RASCONN);
	stRASCTRYINFO = sizeof(RASCTRYINFO);
	stRASDIALPARAMS = sizeof(RASDIALPARAMS);
	stRASDEVINFO = sizeof(RASDEVINFO);
}
