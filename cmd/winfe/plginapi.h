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

#ifndef _PLGINAPI_H_
#define _PLGINAPI_H_
/* plginapi.h - Defines the interface between the Navigator and its plugin */

#include <windows.h>
#include "npapi.h"
#include "plginerr.h"

#ifdef _WINDOWS
    #ifndef WIN32
        #define NP_EXPORT _export
    #endif
#endif

#ifndef NP_EXPORT
    #define NP_EXPORT
#endif

typedef unsigned long NPReason;

#ifdef __cplusplus
extern "C" {
#endif

extern NPError NPE_PluginInit();

extern NPError NPE_PluginShutdown();

extern NPError NPE_New(void *pluginType, NPP instance, uint16 mode, 
		               int16 argc, char *argn[],
		               char *argv[], NPSavedData *saved);

extern void NPE_Destroy(NPP instance, NPSavedData **save);

extern NPError NPE_SetWindow(NPP instance, NPWindow *window);

extern NPError NPE_NewStream(NPP instance, NPMIMEType type, NPStream *stream,
                             NPBool seekable, uint16 *stype);

extern int32 NPE_WriteReady(NPP instance, NPStream* stream);

extern int32 NPE_Write(NPP instance, NPStream* stream,
                         int32 offset,
                         int32 len, void *buffer);

extern NPError NPE_DestroyStream(NPP instance,
                                 NPStream *stream, NPReason reason);

extern void NPE_StreamAsFile(NPP instance, NPStream *stream, const char* fname);

extern int16 NPE_HandleEvent(NPP instance, void *event);

extern void NPE_Print(NPP instance, void *platformPrint);

extern NPError NPNE_GetURL(NPP instance, const char *url, const char *window);

extern NPError NP_PostURL(NPP instance, const char *url,
                          uint32 len, const char *buf, NPBool file);

extern NPError NPNE_RequestRead(NPStream* stream, NPByteRange* rangeList);

extern NPError NPNE_NewStream(NPP instance,
                              NPMIMEType type, NPStream *streame);

extern int32 NPNE_Write(NPP instance, NPStream *stream,
                        int32 len, void *buffer);

extern NPError NPNE_DestroyStream(NPP instance,
                                  NPStream* stream, NPError reason);

extern void NPNE_Status(NPP instance, const char *message);

#ifdef __cplusplus
}
#endif

#endif // _PLGINAPI_H_

