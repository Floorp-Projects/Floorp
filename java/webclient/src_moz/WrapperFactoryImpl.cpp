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
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Ann Sunhachawee
 */

#include "WrapperFactoryImpl.h"

#include "ns_util.h"
#include "nsCRT.h" // for nsCRT::strcmp


const char * gBinDir; 

PRLogModuleInfo *prLogModuleInfo = NULL; // declared in ns_globals.h

const char *gImplementedInterfaces[] = {
        "webclient.WindowControl",
        "webclient.Navigation",
        "webclient.CurrentPage",
        "webclient.History",
        "webclient.EventRegistration",
        "webclient.Bookmarks",
        "webclient.Preferences",
        nsnull
        };

//
// global data
//


//
// Functions to hook into mozilla
// 

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_WrapperFactoryImpl_nativeAppInitialize(
										JNIEnv *env, jobject obj, jstring verifiedBinDirAbsolutePath)
{

    const char *nativePath = (const char *) ::util_GetStringUTFChars(env, 
                                                                     verifiedBinDirAbsolutePath);
    gBinDir = PL_strdup(nativePath);
    ::util_ReleaseStringUTFChars(env, verifiedBinDirAbsolutePath, nativePath);
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_WrapperFactoryImpl_nativeTerminate
(JNIEnv *env, jobject obj)
{
    gComponentManager = nsnull;
    nsCRT::free((char *) gBinDir);
    gBinDir = nsnull;
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
        if (0 == nsCRT::strcmp(gImplementedInterfaces[i++], iName)) {
            result = JNI_TRUE;
            break;
        }
    }
    ::util_ReleaseStringUTFChars(env, interfaceName, iName);
    
    return result;
}
