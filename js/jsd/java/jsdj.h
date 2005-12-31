/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
* Header for JavaScript Debugger JNI support (internal functions)
*/

#ifndef jsdj_h___
#define jsdj_h___

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
#include "jsutil.h" /* Added by JSIFY */
#include "jshash.h" /* Added by JSIFY */
#include "jsdjava.h"
#include "jsobj.h"
#include "jsfun.h"
#include "jsdbgapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
JS_END_EXTERN_C

JS_BEGIN_EXTERN_C

/***************************************************************************/
/* defines copied from Java sources.
** NOTE: javah used to put these in the h files, but with JNI does not seem
** to do this anymore. Be careful with synchronization of these
**
*/

/* From: ThreadStateBase.java  */

#define THR_STATUS_UNKNOWN      0x01
#define THR_STATUS_ZOMBIE       0x02
#define THR_STATUS_RUNNING      0x03
#define THR_STATUS_SLEEPING     0x04
#define THR_STATUS_MONWAIT      0x05
#define THR_STATUS_CONDWAIT     0x06
#define THR_STATUS_SUSPENDED    0x07
#define THR_STATUS_BREAK        0x08

#define DEBUG_STATE_DEAD        0x01
#define DEBUG_STATE_RUN         0x02
#define DEBUG_STATE_RETURN      0x03
#define DEBUG_STATE_THROW       0x04

/***************************************************************************/
/* Our structures */

typedef struct JSDJContext
{
    JSDContext*           jsdc;
    JSHashTable*          envTable;
    jobject               controller;
    JSDJ_UserCallbacks    callbacks;
    void*                 user;
    JSBool                ownJSDC;
} JSDJContext;

/***************************************************************************/
/* Code validation support */

#ifdef DEBUG
extern void JSDJ_ASSERT_VALID_CONTEXT(JSDJContext* jsdjc);
#else
#define JSDJ_ASSERT_VALID_CONTEXT(x)     ((void)0)
#endif

/***************************************************************************/
/* higher level functions */

extern JSDJContext*
jsdj_SimpleInitForSingleContextMode(JSDContext* jsdc,
                                    JSDJ_GetJNIEnvProc getEnvProc, void* user);
extern JSBool
jsdj_SetSingleContextMode();

extern JSDJContext*
jsdj_CreateContext();

extern void
jsdj_DestroyContext(JSDJContext* jsdjc);

extern void
jsdj_SetUserCallbacks(JSDJContext* jsdjc, JSDJ_UserCallbacks* callbacks, 
                      void* user);
extern void
jsdj_SetJNIEnvForCurrentThread(JSDJContext* jsdjc, JNIEnv* env);

extern JNIEnv*
jsdj_GetJNIEnvForCurrentThread(JSDJContext* jsdjc);

extern void
jsdj_SetJSDContext(JSDJContext* jsdjc, JSDContext* jsdc);

extern JSDContext*
jsdj_GetJSDContext(JSDJContext* jsdjc);

extern JSBool
jsdj_RegisterNatives(JSDJContext* jsdjc);

/***************************************************************************/
#ifdef JSD_STANDALONE_JAVA_VM

extern JNIEnv*
jsdj_CreateJavaVMAndStartDebugger(JSDJContext* jsdjc);

/**
* extern JNIEnv*
* jsdj_CreateJavaVM(JSDContext* jsdc);
*/

#endif /* JSD_STANDALONE_JAVA_VM */
/***************************************************************************/

JS_END_EXTERN_C

#endif /* jsdj_h___ */

