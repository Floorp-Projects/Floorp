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

#include "nsIStringBundle.h"
#include "nsIServiceManager.h"

#include "nsString.h"

#include "NativeBrowserControl.h"

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define kCommonDialogsProperties "chrome://global/locale/commonDialogs.properties"


/**

 * a null terminated array of listener interfaces we support.  This is
 * used in NativeEventThread.cpp nativeAddListener,
 * nativeRemoveListener, and in CBrowserContainer.cpp 

 */

const char *gSupportedListenerInterfaces[] = {
    DOCUMENT_LOAD_LISTENER_CLASSNAME_VALUE,
    MOUSE_LISTENER_CLASSNAME_VALUE,
    NEW_WINDOW_LISTENER_CLASSNAME_VALUE,
    nsnull
};

void util_PostEvent(PLEvent * event)
{
    PL_ENTER_EVENT_QUEUE_MONITOR(NativeWrapperFactory::sActionQueue);
  
    ::PL_PostEvent(NativeWrapperFactory::sActionQueue, event);
    
    PL_EXIT_EVENT_QUEUE_MONITOR(NativeWrapperFactory::sActionQueue);
} // PostEvent()


void *util_PostSynchronousEvent(PLEvent * event)
{
    void    *       voidResult = nsnull;

    PL_ENTER_EVENT_QUEUE_MONITOR(NativeWrapperFactory::sActionQueue);
    
    voidResult = ::PL_PostSynchronousEvent(NativeWrapperFactory::sActionQueue, event);
    
    PL_EXIT_EVENT_QUEUE_MONITOR(NativeWrapperFactory::sActionQueue);
    
    return voidResult;
} // PostSynchronousEvent()          

nsresult util_CreateJstringsFromUnichars(wsStringStruct *strings, 
                                         PRInt32 arrayLen)
{
    PR_ASSERT(strings);
    int i = 0;
    nsAutoString autoStr;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    for (i = 0; i < arrayLen; i++) {
        autoStr = strings[i].uniStr;
        strings[i].jStr = nsnull;
        if (autoStr.get()) {
            strings[i].jStr = ::util_NewString(env, (const jchar *) 
                                               autoStr.get(),
                                               autoStr.Length());
            if (nsnull == strings[i].jStr) {
                return NS_ERROR_NULL_POINTER;
            }
        }
    }
    return NS_OK;
}

nsresult util_DeleteJstringsFromUnichars(wsStringStruct *strings, 
                                         PRInt32 arrayLen)
{
    PR_ASSERT(strings);
    int i = 0;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    
    for (i = 0; i < arrayLen; i++) {
        if (nsnull != strings[i].jStr) {
            ::util_DeleteString(env, strings[i].jStr);
            strings[i].jStr = nsnull;
        }
    }
    return NS_OK;
}

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

nsresult util_GetLocaleString(const char *aKey, PRUnichar **aResult)
{
#ifdef BAL_INTERFACE
#else
  nsresult rv;

  nsCOMPtr<nsIStringBundleService> stringService = do_GetService(kStringBundleServiceCID);
  nsCOMPtr<nsIStringBundle> stringBundle;
 
  rv = stringService->CreateBundle(kCommonDialogsProperties, getter_AddRefs(stringBundle));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  rv = stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2(aKey).get(), aResult);

  return rv;

#endif
}

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
