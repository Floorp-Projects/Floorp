/* 
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#ifndef NPN_H
#define NPN_H

#include "npapi.h"
#include "npupp.h"

#define NP_EXPORT

////////////////////////////////////////////////////////////////////////
// stub functions that are exported to the 4.x plugin as entry
// points via the CALLBACKS variable.
//
NPError NP_EXPORT
NPN_RequestRead(NPStream *pstream, NPByteRange *rangeList);

NPError NP_EXPORT
NPN_GetURLNotify(NPP npp, const char* relativeURL, const char* target, void* notifyData);

NPError NP_EXPORT
NPN_GetValue(NPP npp, NPNVariable variable, void *r_value);

NPError NP_EXPORT
NPN_SetValue(NPP npp, NPPVariable variable, void *r_value);

NPError NP_EXPORT
NPN_GetURL(NPP npp, const char* relativeURL, const char* target);

NPError NP_EXPORT
NPN_PostURLNotify(NPP npp, const char* relativeURL, const char *target,
                uint32 len, const char *buf, NPBool file, void* notifyData);

NPError NP_EXPORT
NPN_PostURL(NPP npp, const char* relativeURL, const char *target, uint32 len,
          const char *buf, NPBool file);

NPError NP_EXPORT
NPN_NewStream(NPP npp, NPMIMEType type, const char* window, NPStream** pstream);

int32 NP_EXPORT
NPN_Write(NPP npp, NPStream *pstream, int32 len, void *buffer);

NPError NP_EXPORT
NPN_DestroyStream(NPP npp, NPStream *pstream, NPError reason);

void NP_EXPORT
NPN_Status(NPP npp, const char *message);

void* NP_EXPORT
NPN_MemAlloc (uint32 size);

void NP_EXPORT
NPN_MemFree (void *ptr);

uint32 NP_EXPORT
NPN_MemFlush(uint32 size);

void NP_EXPORT
NPN_ReloadPlugins(NPBool reloadPages);

void NP_EXPORT
NPN_InvalidateRect(NPP npp, NPRect *invalidRect);

void NP_EXPORT
NPN_InvalidateRegion(NPP npp, NPRegion invalidRegion);

const char* NP_EXPORT
NPN_UserAgent(NPP npp);

JRIEnv* NP_EXPORT
NPN_GetJavaEnv(void);

jref NP_EXPORT
NPN_GetJavaPeer(NPP npp);

java_lang_Class* NP_EXPORT
NPN_GetJavaClass(void* handle);

void NP_EXPORT
NPN_ForceRedraw(NPP npp);

#endif
