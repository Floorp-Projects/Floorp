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

#include "prlog.h" // for PR_ASSERT

#include "ns_util.h"

/**

 * a null terminated array of listener interfaces we support.  This is
 * used in NativeEventThread.cpp nativeAddListener,
 * nativeRemoveListener, and in CBrowserContainer.cpp 

 */

const char *gSupportedListenerInterfaces[] = {
    DOCUMENT_LOAD_LISTENER_CLASSNAME_VALUE,
    MOUSE_LISTENER_CLASSNAME_VALUE,
    nsnull
};

void util_PostEvent(WebShellInitContext * initContext, PLEvent * event)
{
    PL_ENTER_EVENT_QUEUE_MONITOR(initContext->actionQueue);
  
    ::PL_PostEvent(initContext->actionQueue, event);
    
    PL_EXIT_EVENT_QUEUE_MONITOR(initContext->actionQueue);
} // PostEvent()


void *util_PostSynchronousEvent(WebShellInitContext * initContext, PLEvent * event)
{
    void    *       voidResult = nsnull;

    PL_ENTER_EVENT_QUEUE_MONITOR(initContext->actionQueue);
    
    voidResult = ::PL_PostSynchronousEvent(initContext->actionQueue, event);
    
    PL_EXIT_EVENT_QUEUE_MONITOR(initContext->actionQueue);
    
    return voidResult;
} // PostSynchronousEvent()          

#ifdef XP_UNIX
jint util_GetGTKWinPtrFromCanvas(JNIEnv *env, jobject browserControlCanvas)
{
    jint result = -1;
#ifdef BAL_INTERFACE
#else
    jclass cls = env->GetObjectClass(browserControlCanvas);  // Get Class for BrowserControlImpl object
    jclass clz = env->FindClass("org/mozilla/webclient/BrowserControlImpl");
    if (nsnull == clz) {
        ::util_ThrowExceptionToJava(env, "Exception: Could not find class for BrowserControlImpl");
        return (jint) 0;
    }
    jboolean ans = env->IsInstanceOf(browserControlCanvas, clz);
    if (JNI_FALSE == ans) {
        ::util_ThrowExceptionToJava(env, "Exception: We have a problem");
        return (jint) 0;
    }
    // Get myCanvas IVar
    jfieldID fid = env->GetFieldID(cls, "myCanvas", "Lorg/mozilla/webclient/BrowserControlCanvas;");
    if (nsnull == fid) {
        ::util_ThrowExceptionToJava(env, "Exception: field myCanvas not found in the jobject for BrowserControlImpl");
        return (jint) 0;
    }
    jobject canvasObj = env->GetObjectField(browserControlCanvas, fid);
    jclass canvasCls = env->GetObjectClass(canvasObj);
    if (nsnull == canvasCls) {
        ::util_ThrowExceptionToJava(env, "Exception: Could Not find Class for CanvasObj");
        return (jint) 0;
    }
    jfieldID gtkfid = env->GetFieldID(canvasCls, "gtkWinPtr", "I");
    if (nsnull == gtkfid) {
        ::util_ThrowExceptionToJava(env, "Exception: field gtkWinPtr not found in the jobject for BrowserControlCanvas");
        return (jint) 0;
    }
    result = env->GetIntField(canvasObj, gtkfid);
#endif
    return result;
}
#endif

//
// Implementations for functions defined in ../src_share/jni_util.h, but not
// implemented there.
// 

void util_LogMessage(int level, const char *fmt)
{
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, level, (fmt));
    }
}

void util_Assert(void *test)
{
    PR_ASSERT(test);
}
