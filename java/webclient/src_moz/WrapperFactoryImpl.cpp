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
 *               Ann Sunhachawee
 */

#include "WrapperFactoryImpl.h"

#include "jni_util.h"

#include "nsIServiceManager.h"  // for NS_InitXPCOM
#include "nsAppShellCIDs.h" // for NS_SESSIONHISTORY_CID
#include "nsCRT.h" // for nsCRT::strcmp

#include "prenv.h"

static NS_DEFINE_CID(kSessionHistoryCID, NS_SESSIONHISTORY_CID);

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


static nsFileSpec gBinDir; 

PRLogModuleInfo *prLogModuleInfo = NULL; // declared in jni_util.h

const char *gImplementedInterfaces[] = {
        "webclient.WindowControl",
        "webclient.Navigation",
        "webclient.CurrentPage",
        "webclient.History",
        "webclient.EventRegistration",
        "webclient.Bookmarks",
        nsnull
        };

//
// global data
//

nsIComponentManager *gComponentManager = nsnull;

//
// Functions to hook into mozilla
// 

extern "C" void NS_SetupRegistry();
extern nsresult NS_AutoregisterComponents();

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_WrapperFactoryImpl_nativeAppInitialize(
										JNIEnv *env, jobject obj, jstring verifiedBinDirAbsolutePath)
{
  static PRBool	gFirstTime = PR_TRUE;
  nsresult rv;
  
  if (gFirstTime) {
    const char *nativePath = (const char *) ::util_GetStringUTFChars(env, 
                                                                     verifiedBinDirAbsolutePath);
    gBinDir = nativePath;
    
    // It is vitally important to call NS_InitXPCOM before calling
    // anything else.
    NS_InitXPCOM(nsnull, &gBinDir);
    NS_SetupRegistry();
    rv = NS_GetGlobalComponentManager(&gComponentManager);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "NS_GetGlobalComponentManager() failed.");
        ::util_ReleaseStringUTFChars(env, verifiedBinDirAbsolutePath, 
                                     nativePath);
        return;
    }
    prLogModuleInfo = PR_NewLogModule("webclient");
    const char *webclientLogFile = PR_GetEnv("WEBCLIENT_LOG_FILE");
    if (nsnull != webclientLogFile) {
        PR_SetLogFile(webclientLogFile);
        // If this fails, it just goes to stdout/stderr
    }

    gComponentManager->RegisterComponentLib(kSessionHistoryCID, nsnull, 
                                            nsnull, APPSHELL_DLL, 
                                            PR_FALSE, PR_FALSE);
    NS_AutoregisterComponents();
    gFirstTime = PR_FALSE;
    ::util_ReleaseStringUTFChars(env, verifiedBinDirAbsolutePath, nativePath);

  }
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_WrapperFactoryImpl_nativeTerminate
(JNIEnv *env, jobject obj)
{
    gComponentManager = nsnull;
    gHistory = nsnull;
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
