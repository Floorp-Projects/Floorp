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
