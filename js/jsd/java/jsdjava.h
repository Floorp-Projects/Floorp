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
* Header for JavaScript Debugger JNI interfaces
*/

#ifndef jsdjava_h___
#define jsdjava_h___

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
#include "jsdebug.h"
#include "jni.h"
JS_END_EXTERN_C


JS_BEGIN_EXTERN_C

/*
 * The linkage of JSDJ API functions differs depending on whether the file is
 * used within the JSDJ library or not.  Any source file within the JSDJ
 * libraray should define EXPORT_JSDJ_API whereas any client of the library
 * should not.
 */
#ifdef EXPORT_JSDJ_API
#define JSDJ_PUBLIC_API(t)    JS_EXPORT_API(t)
#define JSDJ_PUBLIC_DATA(t)   JS_EXPORT_DATA(t)
#else
#define JSDJ_PUBLIC_API(t)    JS_IMPORT_API(t)
#define JSDJ_PUBLIC_DATA(t)   JS_IMPORT_DATA(t)
#endif

#define JSDJ_FRIEND_API(t)    JSDJ_PUBLIC_API(t)
#define JSDJ_FRIEND_DATA(t)   JSDJ_PUBLIC_DATA(t)

/***************************************************************************/
/* Opaque typedefs for handles */

typedef struct JSDJContext        JSDJContext;

/***************************************************************************/
/* High Level functions */

#define JSDJ_START_SUCCESS  1
#define JSDJ_START_FAILURE  2
#define JSDJ_STOP           3

typedef void
(*JSDJ_StartStopProc)(JSDJContext* jsdjc, int event, void *user);

typedef JNIEnv*
(*JSDJ_GetJNIEnvProc)(JSDJContext* jsdjc, void* user);

/* This struct could have more fields in future versions */
typedef struct
{
    uintN              size;       /* size of this struct (init before use)*/
    JSDJ_StartStopProc startStop;
    JSDJ_GetJNIEnvProc getJNIEnv;
} JSDJ_UserCallbacks;

extern JSDJ_PUBLIC_API(JSDJContext*)
JSDJ_SimpleInitForSingleContextMode(JSDContext* jsdc,
                                    JSDJ_GetJNIEnvProc getEnvProc, void* user);

extern JSDJ_PUBLIC_API(JSBool)
JSDJ_SetSingleContextMode();

extern JSDJ_PUBLIC_API(JSDJContext*)
JSDJ_CreateContext();

extern JSDJ_PUBLIC_API(void)
JSDJ_DestroyContext(JSDJContext* jsdjc);

extern JSDJ_PUBLIC_API(void)
JSDJ_SetUserCallbacks(JSDJContext* jsdjc, JSDJ_UserCallbacks* callbacks, 
                      void* user);

extern JSDJ_PUBLIC_API(void)
JSDJ_SetJNIEnvForCurrentThread(JSDJContext* jsdjc, JNIEnv* env);

extern JSDJ_PUBLIC_API(JNIEnv*)
JSDJ_GetJNIEnvForCurrentThread(JSDJContext* jsdjc);

extern JSDJ_PUBLIC_API(void)
JSDJ_SetJSDContext(JSDJContext* jsdjc, JSDContext* jsdc);

extern JSDJ_PUBLIC_API(JSDContext*)
JSDJ_GetJSDContext(JSDJContext* jsdjc);

extern JSDJ_PUBLIC_API(JSBool)
JSDJ_RegisterNatives(JSDJContext* jsdjc);

/***************************************************************************/
#ifdef JSD_STANDALONE_JAVA_VM

extern JSDJ_PUBLIC_API(JNIEnv*)
JSDJ_CreateJavaVMAndStartDebugger(JSDJContext* jsdjc);

#endif /* JSD_STANDALONE_JAVA_VM */
/***************************************************************************/

JS_END_EXTERN_C

#endif /* jsdjava_h___ */

