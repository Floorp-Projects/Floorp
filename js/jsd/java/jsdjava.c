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
* Public functions to reflect JSD into Java 
*/                                          

#include "jsdj.h"

JSDJ_PUBLIC_API(JSDJContext*)
JSDJ_SimpleInitForSingleContextMode(JSDContext* jsdc,
                                    JSDJ_GetJNIEnvProc getEnvProc, void* user)
{
    return jsdj_SimpleInitForSingleContextMode(jsdc, getEnvProc, user);
}

JSDJ_PUBLIC_API(JSBool)
JSDJ_SetSingleContextMode()
{
    return jsdj_SetSingleContextMode();
}

JSDJ_PUBLIC_API(JSDJContext*)
JSDJ_CreateContext()
{
    return jsdj_CreateContext();
}

JSDJ_PUBLIC_API(void)
JSDJ_DestroyContext(JSDJContext* jsdjc)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    jsdj_DestroyContext(jsdjc);
}

JSDJ_PUBLIC_API(void)
JSDJ_SetUserCallbacks(JSDJContext* jsdjc, JSDJ_UserCallbacks* callbacks, 
                      void* user)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    JS_ASSERT(!callbacks ||
              (callbacks->size > 0 && 
               callbacks->size <= sizeof(JSDJ_UserCallbacks)));
    jsdj_SetUserCallbacks(jsdjc, callbacks, user);
}        

JSDJ_PUBLIC_API(void)
JSDJ_SetJNIEnvForCurrentThread(JSDJContext* jsdjc, JNIEnv* env)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    JS_ASSERT(env);
    jsdj_SetJNIEnvForCurrentThread(jsdjc, env);
}

JSDJ_PUBLIC_API(JNIEnv*)
JSDJ_GetJNIEnvForCurrentThread(JSDJContext* jsdjc)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    return jsdj_GetJNIEnvForCurrentThread(jsdjc);
}

JSDJ_PUBLIC_API(void)
JSDJ_SetJSDContext(JSDJContext* jsdjc, JSDContext* jsdc)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    JS_ASSERT(jsdc);
    jsdj_SetJSDContext(jsdjc, jsdc);
}

JSDJ_PUBLIC_API(JSDContext*)
JSDJ_GetJSDContext(JSDJContext* jsdjc)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    return jsdj_GetJSDContext(jsdjc);
}

JSDJ_PUBLIC_API(JSBool)
JSDJ_RegisterNatives(JSDJContext* jsdjc)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    return jsdj_RegisterNatives(jsdjc);
}

/***************************************************************************/
#ifdef JSD_STANDALONE_JAVA_VM

JSDJ_PUBLIC_API(JNIEnv*)
JSDJ_CreateJavaVMAndStartDebugger(JSDJContext* jsdjc)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    return jsdj_CreateJavaVMAndStartDebugger(jsdjc);
}

#endif /* JSD_STANDALONE_JAVA_VM */
/***************************************************************************/
