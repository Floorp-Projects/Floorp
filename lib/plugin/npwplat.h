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

#ifndef _NPWPLAT_H_
#define _NPWPLAT_H_
#include "ntypes.h"
#include "npupp.h"
#include "prlink.h"

typedef unsigned long NPWFEPluginType;
typedef struct _NPPMgtBlk NPPMgtBlk;

typedef struct _NPPMgtBlk {      // a management block based on plugin type
    NPPMgtBlk*      next;
#if !defined(XP_OS2_)
    LPSTR           pPluginFilename;
#else
    PSZ             pPluginFilename;
#endif
#if !defined(XP_OS2)
	PRLibrary*		pLibrary;		// returned by PR_LoadLibrary
#else
	HMODULE			pLibrary;		// returned by PR_LoadLibrary
#endif
    uint16          uRefCount;       // so we know when to call NP_Initialize
                                    // and NP_Shutdown
    NPPluginFuncs*  pPluginFuncs;   // returned by plugin during load
    char*           szMIMEType;
    char*           szFileExtents;  // a ptr to a string of delimited extents,
                                    // or NULL if empty list.
    char*           szFileOpenName; // a ptr to a string to put in FileOpen
                                    // common dialog type description area
    DWORD           versionMS;      // each word is a digit, eg, 3.1
    DWORD           versionLS;      // each word is a digit, eg, 2.66
} NPPMgtBlk;

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif // _NPWPLAT_H_
