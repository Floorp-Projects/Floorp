/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Glenn Barney <gbarney@uiuc.edu>
 */

#include "WrapperFactoryImpl.h"
#include "ie_util.h"
#include "ie_globals.h"


#include "prenv.h"


#ifdef XP_PC


// All this stuff is needed to initialize the history

#define APPSHELL_DLL "appshell.dll"
#define BROWSER_DLL  "nsbrowser.dll"
#define EDITOR_DLL "ender.dll"

#else

#ifdef XP_MAC

#define APPSHELL_DLL "APPSHELL_DLL"
#define EDITOR_DLL  "ENDER_DLL"

#else

// XP_UNIX || XP_BEOS
#define APPSHELL_DLL  "libnsappshell"MOZ_DLL_SUFFIX
#define APPCORES_DLL  "libappcores"MOZ_DLL_SUFFIX
#define EDITOR_DLL  "libender"MOZ_DLL_SUFFIX

#endif // XP_MAC

#endif // XP_PC

//
// file data
// 

const char *gImplementedInterfaces[] = {
        "webclient.WindowControl",
        "webclient.Navigation",
        "webclient.CurrentPage",
        "webclient.History",
        "webclient.EventRegistration",
        0
        };

//
// global data
//


//
// Functions to hook into mozilla
// 


JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_WrapperFactoryImpl_nativeAppInitialize
(JNIEnv *env, jobject obj, jstring verifiedBinDirAbsolutePath)
{


}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_WrapperFactoryImpl_nativeTerminate
(JNIEnv *env, jobject obj)
{
    
}

JNIEXPORT jboolean JNICALL 
Java_org_mozilla_webclient_wrapper_1native_WrapperFactoryImpl_nativeDoesImplement
(JNIEnv *env, jobject obj, jstring interfaceName)
{

    const char *iName = (const char *) ::util_GetStringUTFChars(env, 
                                                                interfaceName);
    jboolean result = JNI_FALSE;
    
    int i = 0;
    
    if (nsnull == iName) {
        return result;
    }
    
    while (nsnull != gImplementedInterfaces[i]) {
        if (0 == strcmp(gImplementedInterfaces[i++], iName)) {
            result = JNI_TRUE;
            break;
        }
    }
    ::util_ReleaseStringUTFChars(env, interfaceName, iName);
    
    return result;
}
