/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Contributor(s): Mark Lin
 *                 Ed Burns <edburns@acm.org>
 */
#include <stdlib.h>

#include <dlfcn.h> // for dlopen
#include <unistd.h> // for getpid

#include "prlog.h"

#include "StubFunctions.h"

//
// Local data
//

PRLogModuleInfo *webclientStubLog = NULL; 
void * webClientDll = NULL;

//
// Function Pointers to functions in WEBCLIENT_DSO
//

/**

* The functions in this file are what is actually called from java for
* the native methods of the class GtkBrowserControlCanvas. <P>

*/

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_gtk_GtkBrowserControlCanvas_loadMainDll
(JNIEnv *, jclass)
{
    pid_t pid = getpid();
    printf("webclient pid: %d\n", pid);
    fflush(stdout);

    int rc = 0;
    PR_ASSERT(NULL== webclientStubLog);
    webclientStubLog = PR_NewLogModule(WEBCLIENTSTUB_LOG_MODULE);

    PR_LOG(webclientStubLog, PR_LOG_DEBUG, 
           ("GtkBrowserControlCanvasStub_loadMainDll: entering\n"));

    webClientDll = dlopen(WEBCLIENT_DSO, RTLD_LAZY | RTLD_GLOBAL);
    if (webClientDll) {
        PR_LOG(webclientStubLog, PR_LOG_DEBUG, 
               ("GtkBrowserControlCanvasStub_loadMainDll: dll loaded\n"));
        rc = locateStubFunctions(webClientDll);
        PR_LOG(webclientStubLog, PR_LOG_DEBUG, 
               ("GtkBrowserControlCanvasStub_loadMainDll: locateStubFunctions returns: %d\n", rc));
        PR_ASSERT(rc == 0);
    } else {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, 
               ("Error opening DSO %s: %s\n", WEBCLIENT_DSO, dlerror()));
        PR_ASSERT(NULL);
    }

    PR_LOG(webclientStubLog, PR_LOG_DEBUG, 
           ("GtkBrowserControlCanvasStub_loadMainDll: exiting\n"));
    
}

int locateStubFunctions(void *dll)
{
    PR_LOG(webclientStubLog, PR_LOG_DEBUG, 
           ("GtkBrowserControlCanvasStub_locateStubFunctions: entering\n"));

    // See PENDING comments in StubFunctions.h

    createTopLevelWindow = (jint (*) (JNIEnv *, jobject)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_gtk_GtkBrowserControlCanvas_createTopLevelWindow");
    if (!createTopLevelWindow) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    getHandleToPeer = (jint (*) (JNIEnv *, jobject)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_gtk_GtkBrowserControlCanvas_getHandleToPeer");
    if (!getHandleToPeer) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    createContainerWindow = (jint (*) (JNIEnv *, jobject, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_gtk_GtkBrowserControlCanvas_createContainerWindow");
    if (!createContainerWindow) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    reparentWindow = (void (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_gtk_GtkBrowserControlCanvas_reparentWindow");
    if (!reparentWindow) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    processEvents = (void (*) (JNIEnv *, jobject)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_gtk_GtkBrowserControlCanvas_processEvents");
    if (!processEvents) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    setGTKWindowSize = (void (*) (JNIEnv *, jobject, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_gtk_GtkBrowserControlCanvas_setGTKWindowSize");
    if (!setGTKWindowSize) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    getGTKWinID = (jint (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_gtk_GtkBrowserControlCanvas_getGTKWinID");
    if (!getGTKWinID) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }

    nativeDoesImplement = (jboolean (*) (JNIEnv *, jobject, jstring)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WrapperFactoryImpl_nativeDoesImplement");
    if (!nativeDoesImplement) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeAppInitialize = (jint (*) (JNIEnv *, jobject, jstring)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WrapperFactoryImpl_nativeAppInitialize");
    if (!nativeAppInitialize) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeTerminate = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WrapperFactoryImpl_nativeTerminate");
    if (!nativeTerminate) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    nativeDestroyInitContext = (void(*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeDestroyInitContext");
    if (!nativeDestroyInitContext) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeCreateInitContext = (jint (*) (JNIEnv *, jobject, jint, jint, jint, jint, jint, jobject)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeCreateInitContext");
    if (!nativeCreateInitContext) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeMoveWindowTo = (void (*) (JNIEnv *, jobject, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeMoveWindowTo");
    if (!nativeMoveWindowTo) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeRemoveFocus = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeRemoveFocus");
    if (!nativeRemoveFocus) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeRepaint = (void (*) (JNIEnv *, jobject, jint, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeRepaint");
    if (!nativeRepaint) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeSetBounds = (void (*) (JNIEnv *, jobject, jint, jint, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeSetBounds");
    if (!nativeSetBounds) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeSetFocus = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeSetFocus");
    if (!nativeSetFocus) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeSetVisible = (void (*) (JNIEnv *, jobject, jint, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeSetVisible");
    if (!nativeSetVisible) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    nativeGetChildAt = (jint (*) (JNIEnv *, jobject, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeGetChildAt");
    if (!nativeGetChildAt) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeGetChildCount = (jint (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeGetChildCount");
    if (!nativeGetChildCount) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeGetIndex = (jint (*) (JNIEnv *, jobject, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeGetIndex");
    if (!nativeGetIndex) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeInsertElementAt = (void (*) (JNIEnv *, jobject, jint, jint, jint, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeInsertElementAt");
    if (!nativeInsertElementAt) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeNewFolder = (jint (*) (JNIEnv *, jobject, jint, jint, jobject)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeNewFolder");
    if (!nativeNewFolder) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeIsContainer = (jboolean (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeIsContainer");
    if (!nativeIsContainer) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeIsLeaf = (jboolean (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeIsLeaf");
    if (!nativeIsLeaf) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeToString = (jstring (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeToString");
    if (!nativeToString) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    nativeFinalize = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFEnumeration_nativeFinalize");
    if (!nativeFinalize) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeHasMoreElements = (jboolean (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFEnumeration_nativeHasMoreElements");
    if (!nativeHasMoreElements) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeNextElement = (jint (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFEnumeration_nativeNextElement");
    if (!nativeNextElement) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    nativeLoadURL = (void (*) (JNIEnv *, jobject, jint, jstring)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeLoadURL");
    if (!nativeLoadURL) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeLoadFromStream = (void (*) (JNIEnv *, jobject, jint, jobject, jstring, jstring, jint, jobject)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeLoadFromStream");
    if (!nativeLoadFromStream) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeRefresh = (void (*) (JNIEnv *, jobject, jint, jlong)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeRefresh");
    if (!nativeRefresh) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeStop = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeStop");
    if (!nativeStop) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeSetPrompt = (void (*) (JNIEnv *, jobject, jint, jobject)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeSetPrompt");
    if (!nativeSetPrompt) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    nativeAddRef = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_ISupportsPeer_nativeAddRef");
    if (!nativeAddRef) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeRelease = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_ISupportsPeer_nativeRelease");
    if (!nativeRelease) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    nativeBack = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeBack");
    if (!nativeBack) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeCanBack = (jboolean (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeCanBack");
    if (!nativeCanBack) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeCanForward = (jboolean (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeCanForward");
    if (!nativeCanForward) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeClearHistory = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeClearHistory");
    if (!nativeClearHistory) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeForward = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeForward");
    if (!nativeForward) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeGetBackList = (jobjectArray (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetBackList");
    if (!nativeGetBackList) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeGetCurrentHistoryIndex = (jint (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetCurrentHistoryIndex");
    if (!nativeGetCurrentHistoryIndex) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeGetForwardList = (jobjectArray (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetForwardList");
    if (!nativeGetForwardList) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeGetHistory = (jobjectArray (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetHistory");
    if (!nativeGetHistory) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeGetHistoryEntry = (jobject (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetHistoryEntry");
    if (!nativeGetHistoryEntry) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeGetHistoryLength = (jint (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetHistoryLength");
    if (!nativeGetHistoryLength) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeGetURLForIndex = (jstring (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetURLForIndex");
    if (!nativeGetURLForIndex) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeSetCurrentHistoryIndex = (void (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeSetCurrentHistoryIndex");
    if (!nativeSetCurrentHistoryIndex) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    nativeCopyCurrentSelectionToSystemClipboard = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeCopyCurrentSelectionToSystemClipboard");
    if (!nativeCopyCurrentSelectionToSystemClipboard) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
  }

    nativeGetSelection = (void (*) (JNIEnv *env, jobject obj, jint webShellPtr, jobject selection)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeGetSelection");
    if (!nativeGetSelection) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }

    nativeHighlightSelection = (void (*) (JNIEnv *env, jobject obj, jint webShellPtr, jobject startContainer, jobject endContainer, jint startOffset, jint endOffset)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeHighlightSelection");
    if (!nativeHighlightSelection) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }

    nativeClearAllSelections = (void (*) (JNIEnv *env, jobject obj, jint webShellPtr)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeClearAllSelections");
    if (!nativeClearAllSelections) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }

    nativeFindInPage = (void (*) (JNIEnv *, jobject, jint, jstring, jboolean, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeFindInPage");
    if (!nativeFindInPage) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeFindNextInPage = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeFindNextInPage");
    if (!nativeFindNextInPage) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeGetCurrentURL = (jstring (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeGetCurrentURL");
    if (!nativeGetCurrentURL) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeResetFind = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeResetFind");
    if (!nativeResetFind) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeSelectAll = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeSelectAll");
    if (!nativeSelectAll) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    nativeGetDOM = (jobject (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeGetDOM");
    if (!nativeGetDOM) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    nativeGetBookmarks = (jint (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_BookmarksImpl_nativeGetBookmarks");
    if (!nativeGetBookmarks) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeNewRDFNode = (jint (*) (JNIEnv *, jobject, jint, jstring, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_BookmarksImpl_nativeNewRDFNode");
    if (!nativeNewRDFNode) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeSetUnicharPref = (void (*) (JNIEnv *, jobject, jint, jstring, jstring)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeSetUnicharPref");
    if (!nativeSetUnicharPref) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeSetIntPref = (void (*) (JNIEnv *, jobject, jint, jstring, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeSetIntPref");
    if (!nativeSetIntPref) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeSetBoolPref = (void (*) (JNIEnv *, jobject, jint, jstring, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeSetBoolPref");
    if (!nativeSetBoolPref) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeGetPrefs = (jobject (*) (JNIEnv *env, jobject obj, jint webShellPtr, jobject props)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeGetPrefs");
    if (!nativeGetPrefs) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeRegisterPrefChangedCallback = (void (*) (JNIEnv *env, jobject obj, jint webShellPtr, jobject callback, jstring prefName, jobject closure)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeRegisterPrefChangedCallback");
    if (!nativeRegisterPrefChangedCallback) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeAddListener = (void (*) (JNIEnv *, jobject, jint, jobject, jstring)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeAddListener");
    if (!nativeAddListener) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeRemoveListener = (void (*) (JNIEnv *, jobject, jint, jobject, jstring)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeRemoveListener");
    if (!nativeRemoveListener) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeRemoveAllListeners = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeRemoveAllListeners");
    if (!nativeRemoveAllListeners) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeInitialize = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeInitialize");
    if (!nativeInitialize) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    nativeProcessEvents = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeProcessEvents");
    if (!nativeProcessEvents) {
        PR_LOG(webclientStubLog, PR_LOG_ERROR, ("got dlsym error %s\n", dlerror()));
        return -1;
    }
    
    PR_LOG(webclientStubLog, PR_LOG_DEBUG, 
           ("GtkBrowserControlCanvasStub_locateStubFunctions: exiting\n"));
    return 0;
}

extern "C" {

//
// JNI functions
//

/*
 * Class:     org_mozilla_webclient_gtk_GtkBrowserControlCanvas
 * Method:    createTopLevelWindow
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_gtk_GtkBrowserControlCanvas_createTopLevelWindow (JNIEnv * env, jobject obj) {
  return (* createTopLevelWindow) (env, obj);
}

/*
 * Class:     org_mozilla_webclient_gtk_GtkBrowserControlCanvas
 * Method:    getHandleToPeer
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_gtk_GtkBrowserControlCanvas_getHandleToPeer (JNIEnv * env, jobject obj) {
  return (* getHandleToPeer) (env, obj);
}

/*
 * Class:     org_mozilla_webclient_gtk_GtkBrowserControlCanvas
 * Method:    createContainerWindow
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_gtk_GtkBrowserControlCanvas_createContainerWindow (JNIEnv * env, jobject obj, jint parent, jint width, jint height) {
  return (* createContainerWindow) (env, obj, parent, width, height);
}

/*
 * Class:     org_mozilla_webclient_gtk_GtkBrowserControlCanvas
 * Method:    getGTKWinID
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_gtk_GtkBrowserControlCanvas_getGTKWinID
(JNIEnv * env, jobject obj, jint gtkWinPtr) {
  return (* getGTKWinID) (env, obj, gtkWinPtr);
}

/*
 * Class:     org_mozilla_webclient_gtk_GtkBrowserControlCanvas
 * Method:    reparentWindow
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_gtk_GtkBrowserControlCanvas_reparentWindow
(JNIEnv * env, jobject obj, jint childID, jint parentID) {
  (* reparentWindow) (env, obj, childID, parentID);
}

/*
 * Class:     org_mozilla_webclient_gtk_GtkBrowserControlCanvas
 * Method:    processEvents
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_gtk_GtkBrowserControlCanvas_processEvents
    (JNIEnv * env, jobject obj) {
    (* processEvents) (env, obj);
}


/*
 * Class:     org_mozilla_webclient_gtk_GtkBrowserControlCanvas
 * Method:    setGTKWindowSize
 * Signature: (III)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_gtk_GtkBrowserControlCanvas_setGTKWindowSize
    (JNIEnv * env, jobject obj, jint xwinID, jint width, jint height) {
    (* setGTKWindowSize) (env, obj, xwinID, width, height);
}

// NativeEventThread
/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_NativeEventThread
 * Method:    nativeInitialize
 * Signature: (I)V
 */

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeInitialize
(JNIEnv *env, jobject obj, jint webShellPtr) {
  (* nativeInitialize) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_NativeEventThread
 * Method:    nativeAddListener
 * Signature: (ILorg/mozilla/webclient/WebclientEventListener;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeAddListener
(JNIEnv *env, jobject obj, jint webShellPtr, jobject typedListener, jstring listenerString) {
  (* nativeAddListener) (env, obj, webShellPtr, typedListener, listenerString);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_NativeEventThread
 * Method:    nativeRemoveListener
 * Signature: (ILorg/mozilla/webclient/WebclientEventListener;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeRemoveListener
(JNIEnv *env, jobject obj, jint webShellPtr, jobject typedListener, jstring listenerString) {
  (* nativeRemoveListener) (env, obj, webShellPtr, typedListener, listenerString);
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeRemoveAllListeners
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    (* nativeRemoveAllListeners) (env, obj, webShellPtr);
}


/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_NativeEventThread
 * Method:    nativeProcessEvents
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeProcessEvents
(JNIEnv *env, jobject obj, jint webShellPtr) {
  (* nativeProcessEvents) (env, obj, webShellPtr);
}


// BookMarksImpl
/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_BookmarksImpl
 * Method:    nativeGetBookmarks
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_BookmarksImpl_nativeGetBookmarks
(JNIEnv * env, jobject obj, jint nativeWebshell) {
  return (* nativeGetBookmarks) (env, obj, nativeWebshell);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_BookmarksImpl
 * Method:    nativeNewRDFNode
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_BookmarksImpl_nativeNewRDFNode
(JNIEnv * env, jobject obj, jint webShellPtr, jstring url, jboolean isFolder) {
  return (* nativeNewRDFNode) (env, obj, webShellPtr, url, isFolder);
}

// PreferencesImpl.h

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeSetUnicharPref
(JNIEnv *env, jobject obj, jint webShellPtr, jstring prefName, 
 jstring prefValue)
{
    (* nativeSetUnicharPref) (env, obj, webShellPtr, prefName, prefValue);
}


JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeSetIntPref
(JNIEnv *env, jobject obj, jint webShellPtr, jstring prefName, jint prefValue)
{
    (* nativeSetIntPref) (env, obj, webShellPtr, prefName, prefValue);
}


JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeSetBoolPref
(JNIEnv *env, jobject obj, jint webShellPtr, jstring prefName, 
 jboolean prefValue)
{
    (* nativeSetBoolPref) (env, obj, webShellPtr, prefName, prefValue);
}

JNIEXPORT jobject JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeGetPrefs
(JNIEnv *env, jobject obj, jint webShellPtr, jobject props)
{
    return (* nativeGetPrefs) (env, obj, webShellPtr, props);
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeRegisterPrefChangedCallback
(JNIEnv *env, jobject obj, jint webShellPtr, 
 jobject callback, jstring prefName, jobject closure)
{
    (* nativeRegisterPrefChangedCallback) (env, obj, webShellPtr, callback,
                                           prefName, closure);
}

// CurrentPageImpl
/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeCopyCurrentSelectionToSystemClipboard
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeCopyCurrentSelectionToSystemClipboard
(JNIEnv * env, jobject obj, jint webShellPtr) {
  (* nativeCopyCurrentSelectionToSystemClipboard) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeGetSelection
 * Signature: (ILorg/mozilla/webclient/Selection;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeGetSelection
(JNIEnv *env, jobject obj, jint webShellPtr, jobject selection) {
    (* nativeGetSelection) (env, obj, webShellPtr, selection);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeHighlightSelection
 * Signature: (ILorg/w3c/dom/Node;Lorg/w3c/dom/Node;II)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeHighlightSelection
(JNIEnv *env, jobject obj, jint webShellPtr, jobject startContainer, jobject endContainer, jint startOffset, jint endOffset) {
    (* nativeHighlightSelection) (env, obj, webShellPtr, startContainer, endContainer, startOffset, endOffset);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeClearAllSelections
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeClearAllSelections
(JNIEnv *env, jobject obj, jint webShellPtr) {
    (* nativeClearAllSelections) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeFindInPage
 * Signature: (Ljava/lang/String;ZZ)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeFindInPage
(JNIEnv * env, jobject obj, jint webShellPtr, jstring mystring, jboolean myboolean, jboolean myboolean1) {
  (* nativeFindInPage) (env, obj, webShellPtr, mystring, myboolean, myboolean1);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeFindNextInPage
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeFindNextInPage
(JNIEnv * env, jobject obj, jint webShellPtr) {
  (* nativeFindNextInPage) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeGetCurrentURL
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeGetCurrentURL
(JNIEnv *env, jobject obj, jint webShellPtr) {
  return (* nativeGetCurrentURL) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeResetFind
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeResetFind
(JNIEnv * env, jobject obj, jint webShellPtr) {
  (* nativeResetFind) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeSelectAll
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeSelectAll
(JNIEnv * env, jobject obj, jint webShellPtr) {
  (* nativeSelectAll) (env, obj, webShellPtr);
}

JNIEXPORT jobject JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeGetDOM
(JNIEnv *env, jobject obj, jint webShellPtr) {
    return (* nativeGetDOM) (env, obj, webShellPtr);
}



// HistoryImpl
/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl
 * Method:    nativeBack
 * Signature: (I)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeBack
(JNIEnv *env, jobject obj, jint webShellPtr) {
  (* nativeBack) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl
 * Method:    nativeCanBack
 * Signature: (I)Z
 */
JNIEXPORT jboolean 
JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeCanBack
(JNIEnv *env, jobject obj, jint webShellPtr) {
  return (* nativeCanBack) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl
 * Method:    nativeCanForward
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeCanForward
(JNIEnv *env, jobject obj, jint webShellPtr) {
  return (* nativeCanForward) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl
 * Method:    nativeClearHistory
 * Signature: (I)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeClearHistory
(JNIEnv *env, jobject obj, jint webShellPtr) {
  (* nativeClearHistory) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl
 * Method:    nativeForward
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeForward
(JNIEnv *env, jobject obj, jint webShellPtr) {
  (* nativeForward) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl
 * Method:    nativeGetBackList
 * Signature: (I)[Lorg/mozilla/webclient/HistoryEntry;
 */
JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetBackList
(JNIEnv *env, jobject obj, jint webShellPtr) {
  return (* nativeGetBackList) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl
 * Method:    nativeGetCurrentHistoryIndex
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetCurrentHistoryIndex
(JNIEnv *env, jobject obj, jint webShellPtr) {
  return (* nativeGetCurrentHistoryIndex) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl
 * Method:    nativeGetForwardList
 * Signature: (I)[Lorg/mozilla/webclient/HistoryEntry;
 */
JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetForwardList
(JNIEnv *env, jobject obj, jint webShellPtr) {
  return (* nativeGetForwardList) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl
 * Method:    nativeGetHistory
 * Signature: (I)[Lorg/mozilla/webclient/HistoryEntry;
 */
JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetHistory
(JNIEnv *env, jobject obj, jint webShellPtr) {
  return (* nativeGetHistory) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl
 * Method:    nativeGetHistoryEntry
 * Signature: (II)Lorg/mozilla/webclient/HistoryEntry;
 */
JNIEXPORT jobject JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetHistoryEntry
(JNIEnv *env, jobject obj, jint webShellPtr, jint historyIndex) {
  return (* nativeGetHistoryEntry) (env, obj, webShellPtr, historyIndex);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl
 * Method:    nativeGetHistoryLength
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetHistoryLength
(JNIEnv *env, jobject obj, jint webShellPtr) {
  return (* nativeGetHistoryLength) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl
 * Method:    nativeGetURLForIndex
 * Signature: (II)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetURLForIndex
(JNIEnv *env, jobject obj, jint webShellPtr, jint historyIndex) {
  return (* nativeGetURLForIndex) (env, obj, webShellPtr, historyIndex);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl
 * Method:    nativeSetCurrentHistoryIndex
 * Signature: (II)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeSetCurrentHistoryIndex
(JNIEnv *env, jobject obj, jint webShellPtr, jint historyIndex) {
  (* nativeSetCurrentHistoryIndex) (env, obj, webShellPtr, historyIndex);
}



// ISupportsPeer
/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_ISupportsPeer
 * Method:    nativeAddRef
 * Signature: (I)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_ISupportsPeer_nativeAddRef
(JNIEnv *env, jobject obj, jint nativeISupportsImpl) {
  (* nativeAddRef) (env, obj, nativeISupportsImpl);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_ISupportsPeer
 * Method:    nativeRelease
 * Signature: (I)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_ISupportsPeer_nativeRelease
(JNIEnv *env, jobject obj, jint nativeISupportsImpl) {
  (* nativeRelease) (env, obj, nativeISupportsImpl);
}



// NavigationImpl
/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_NavigationImpl
 * Method:    nativeLoadURL
 * Signature: (ILjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeLoadURL
(JNIEnv *env, jobject obj, jint webShellPtr, jstring urlString) {
  (* nativeLoadURL) (env, obj, webShellPtr, urlString);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_NavigationImpl
 * Method:    nativeLoadFromStream
 * Signature: (ILjava/io/InputStream;Ljava/lang/String;Ljava/lang/String;ILjava/util/Properties;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeLoadFromStream
  (JNIEnv *env, jobject obj, jint webShellPtr, jobject javaStream, 
   jstring absoluteUrl, jstring contentType, jint contentLength, 
   jobject loadProperties)
{
    (* nativeLoadFromStream) (env, obj, webShellPtr, javaStream, absoluteUrl, 
                              contentType, contentLength, loadProperties);
}
/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_NavigationImpl
 * Method:    nativeRefresh
 * Signature: (IJ)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeRefresh
(JNIEnv *env, jobject obj, jint webShellPtr, jlong loadFlags) {
  (* nativeRefresh) (env, obj, webShellPtr, loadFlags);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_NavigationImpl
 * Method:    nativeStop
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeStop
(JNIEnv *env, jobject obj, jint webShellPtr) {
  (* nativeStop) (env, obj, webShellPtr);
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeSetPrompt
(JNIEnv *env, jobject obj, jint webShellPtr, jobject userPrompt)
{
    (* nativeSetPrompt) (env, obj, webShellPtr, userPrompt);
}


// RDFEnumeration
/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_RDFEnumeration
 * Method:    nativeFinalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_RDFEnumeration_nativeFinalize
(JNIEnv *env, jobject obj, jint webShellPtr) {
  (* nativeFinalize) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_RDFEnumeration
 * Method:    nativeHasMoreElements
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_RDFEnumeration_nativeHasMoreElements
(JNIEnv *env, jobject obj, jint webShellPtr, jint nativeRDFNode) {
  return (* nativeHasMoreElements) (env, obj, webShellPtr, nativeRDFNode);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_RDFEnumeration
 * Method:    nativeNextElement
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_RDFEnumeration_nativeNextElement
(JNIEnv *env, jobject obj, jint webShellPtr, jint nativeRDFNode) {
  return (* nativeNextElement) (env, obj, webShellPtr, nativeRDFNode);
}



// RDFTreeNode
/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_RDFTreeNode
 * Method:    nativeGetChildAt
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeGetChildAt
(JNIEnv *env, jobject obj, jint webShellPtr, jint nativeRDFNode, 
 jint childIndex) {
  return (* nativeGetChildAt) (env, obj, webShellPtr, nativeRDFNode, 
                               childIndex);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_RDFTreeNode
 * Method:    nativeGetChildCount
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeGetChildCount
(JNIEnv *env, jobject obj, jint webShellPtr, jint nativeRDFNode) {
  return (* nativeGetChildCount) (env, obj, webShellPtr, nativeRDFNode);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_RDFTreeNode
 * Method:    nativeGetIndex
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeGetIndex
(JNIEnv *env, jobject obj, jint webShellPtr, jint nativeRDFNode, 
 jint childRDFNode) {
  return (* nativeGetIndex) (env, obj, webShellPtr, nativeRDFNode, 
                             childRDFNode);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_RDFTreeNode
 * Method:    nativeInsertElementAt
 * Signature: (III)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeInsertElementAt
(JNIEnv *env, jobject obj, jint webShellPtr, jint parentRDFNode, 
 jint childRDFNode, jobject childProps, jint childIndex) {
  (* nativeInsertElementAt) (env, obj, webShellPtr, parentRDFNode, 
                             childRDFNode, childProps, childIndex);
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeNewFolder
(JNIEnv *env, jobject obj, jint webShellPtr, jint parentRDFNode, 
 jobject childProps)
{
    return (* nativeNewFolder) (env, obj, webShellPtr, 
                                parentRDFNode, childProps);
}


/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_RDFTreeNode
 * Method:    nativeIsContainer
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeIsContainer
(JNIEnv *env, jobject obj, jint webShellPtr, jint nativeRDFNode) {
  return (* nativeIsContainer) (env, obj, webShellPtr, nativeRDFNode);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_RDFTreeNode
 * Method:    nativeIsLeaf
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeIsLeaf
(JNIEnv *env, jobject obj, jint webShellPtr, jint nativeRDFNode) {
  return (* nativeIsLeaf) (env, obj, webShellPtr, nativeRDFNode);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_RDFTreeNode
 * Method:    nativeToString
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeToString
(JNIEnv *env, jobject obj, jint webShellPtr, jint nativeRDFNode) {
  return (* nativeToString) (env, obj, webShellPtr, nativeRDFNode);
}




// WindowControlImpl
/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_WindowControlImpl
 * Method:    nativeDestroyInitContext
 * Signature: (IIIII)I
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeDestroyInitContext
(JNIEnv *env, jobject obj, jint webShellPtr) {
  (* nativeDestroyInitContext) (env, obj, webShellPtr);
}

// WindowControlImpl
/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_WindowControlImpl
 * Method:    nativeCreateInitContext
 * Signature: (IIIII)I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeCreateInitContext
(JNIEnv *env, jobject obj, jint windowPtr, jint x, jint y, jint width, jint height, jobject aBrowserControlImpl) {
  return (* nativeCreateInitContext) (env, obj, windowPtr, x, y, width, height, aBrowserControlImpl);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_WindowControlImpl
 * Method:    nativeMoveWindowTo
 * Signature: (III)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeMoveWindowTo
(JNIEnv *env, jobject obj, jint webShellPtr, jint x, jint y) {
  (* nativeMoveWindowTo) (env, obj, webShellPtr, x, y);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_WindowControlImpl
 * Method:    nativeRemoveFocus
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeRemoveFocus
(JNIEnv *env, jobject obj, jint webShellPtr) {
  (* nativeRemoveFocus) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_WindowControlImpl
 * Method:    nativeRepaint
 * Signature: (IZ)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeRepaint
(JNIEnv *env, jobject obj, jint webShellPtr, jboolean forceRepaint) {
  (* nativeRepaint) (env, obj, webShellPtr, forceRepaint);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_WindowControlImpl
 * Method:    nativeSetBounds
 * Signature: (IIIII)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeSetBounds
(JNIEnv *env, jobject obj, jint webShellPtr, jint x, jint y, jint w, jint h) {
  (* nativeSetBounds) (env, obj, webShellPtr, x, y, w, h);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_WindowControlImpl
 * Method:    nativeSetFocus
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeSetFocus
(JNIEnv *env, jobject obj, jint webShellPtr) {
  (* nativeSetFocus) (env, obj, webShellPtr);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_WindowControlImpl
 * Method:    nativeSetVisible
 * Signature: (IZ)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeSetVisible
(JNIEnv *env, jobject obj, jint webShellPtr, jboolean newState) {
  (* nativeSetVisible) (env, obj, webShellPtr, newState);
}


// WrapperFactoryImpl
/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_WrapperFactoryImpl
 * Method:    nativeDoesImplement
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_WrapperFactoryImpl_nativeDoesImplement
(JNIEnv *env, jobject obj, jstring interfaceName) {
  return (* nativeDoesImplement) (env, obj, interfaceName);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_WrapperFactoryImpl
 * Method:    nativeInitialize
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_WrapperFactoryImpl_nativeAppInitialize
(JNIEnv *env, jobject obj, jstring verifiedBinDirAbsolutePath) {
    PR_LOG(webclientStubLog, PR_LOG_DEBUG, 
           ("GtkBrowserControlCanvasStub_nativeAppInitialize: entering\n"));
    jint result = 
        (* nativeAppInitialize) (env, obj, verifiedBinDirAbsolutePath);
    PR_LOG(webclientStubLog, PR_LOG_DEBUG, 
           ("GtkBrowserControlCanvasStub_nativeAppInitialize: exiting\n"));
    return result;
    
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_WrapperFactoryImpl
 * Method:    nativeTerminate
 * Signature: ()V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_WrapperFactoryImpl_nativeTerminate
(JNIEnv * env, jobject obj, jint nativeContext) {
  (* nativeTerminate) (env, obj, nativeContext);
}

} // End extern "C"
