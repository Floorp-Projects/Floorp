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

#ifndef _PLGINDLL_H_
#define _PLGINDLL_H_
/* Defines the interface between the Navigator and its plugin */

#include <windows.h>
#include "npapi.h"
#include "npupp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* plugin meta and instance member functions */
typedef NPError (WINAPI* NP_GETENTRYPOINTS)(NPPluginFuncs* pFuncs);

NPError WINAPI NP_GetEntryPoints(NPPluginFuncs* pFuncs);

typedef NPError (WINAPI* NP_PLUGININIT)(NPNetscapeFuncs* pFuncs);

NPError WINAPI NP_PluginInit(NPNetscapeFuncs* pFuncs);

typedef NPError (WINAPI* NP_PLUGINSHUTDOWN)();

NPError WINAPI NP_PluginShutdown();

#ifdef __cplusplus
}
#endif

#endif // _PLGINDLL_H_

