/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*                        
* Public headers for JSDB 
*/                        

#ifndef jsdb_h___
#define jsdb_h___

/* Get jstypes.h included first. After that we can use PR macros for doing
*  this extern "C" stuff!
*/ 
#ifdef __cplusplus
extern "C"
{
#endif
#include "jstypes.h"
#ifdef __cplusplus
}
#endif

JS_BEGIN_EXTERN_C
#include "jsapi.h"
#include "jsdebug.h"
JS_END_EXTERN_C

JS_BEGIN_EXTERN_C

extern JS_EXPORT_API(JSBool)
JSDB_InitDebugger(JSRuntime* rt, JSDContext* jsdc, int depth);

JS_END_EXTERN_C

#endif /* jsdb_h___ */
