/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions

extern size_t stRASENTRY;
extern size_t stRASCONN;
extern size_t stRASCTRYINFO;
extern size_t stRASDIALPARAMS;
extern size_t stRASDEVINFO;

#ifdef WINVER
#undef WINVER
#endif
#define WINVER 0x0401

#include <windows.h>
#include <ras.h>

void SizeofRASNT40()
{
	stRASENTRY = sizeof( RASENTRY );
	stRASCONN = sizeof( RASCONN );
	stRASCTRYINFO = sizeof( RASCTRYINFO );
	stRASDIALPARAMS = sizeof( RASDIALPARAMS );
	stRASDEVINFO = sizeof( RASDEVINFO );
}
