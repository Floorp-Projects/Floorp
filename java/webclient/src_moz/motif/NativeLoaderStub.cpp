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
 * Contributor(s): 
 */

/*
 * BrowserControlNativeShimStub.cpp
 */

// PENDING(mark): I suppose this is where I need to go into my explaination of why
// this file is needed...

// Ashu
#ifdef XP_UNIX
#include <stdlib.h> // for exit(0)
#endif
//

// For loading DLL
#include <dlfcn.h>
// JNI...yada, yada, yada
#include <jni.h>
// JNI Headers
#include "org_mozilla_webclient_impl_wrapper_0005fnative_BookmarksImpl.h"
#include "PreferencesImpl.h"
#include "org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl.h"
#include "org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl.h"
#include "org_mozilla_webclient_impl_wrapper_0005fnative_ISupportsPeer.h"
#include "org_mozilla_webclient_impl_wrapper_0005fnative_NavigationImpl.h"
#include "org_mozilla_webclient_impl_wrapper_0005fnative_RDFEnumeration.h"
#include "org_mozilla_webclient_impl_wrapper_0005fnative_RDFTreeNode.h"
#include "org_mozilla_webclient_impl_wrapper_0005fnative_WindowControlImpl.h"
#include "org_mozilla_webclient_impl_wrapper_0005fnative_WrapperFactoryImpl.h"
#include "org_mozilla_webclient_impl_wrapper_0005fnative_NativeEventThread.h"

// allow code in webclientstub.so to load us
#include "NativeLoaderStub.h"
//#include "MotifBrowserControlCanvas.h"

// Global reference to webclient dll
void * webClientDll;

//Ashu
extern void locateMotifBrowserControlStubFunctions(void *);
//



//modified by Ashu Kulkarni 2/29/2000
/*
void (* nativeInitialize) (JNIEnv *, jobject, jstring);
void (* nativeTerminate) (JNIEnv *, jobject);
void (* nativeSendKeyDownEvent) (JNIEnv *, jobject, jint, jchar, jint, jint, jint);
void (* nativeSendKeyUpEvent) (JNIEnv *, jobject, jint, jchar, jint, jint, jint);
void (* nativeSendMouseEvent) (JNIEnv *, jobject, jint, jint, jint, jint, jint, jint, jint, jint, jint, jint);
void (* nativeIdleEvent) (JNIEnv *, jobject, jint, jint);
void (* nativeUpdateEvent) (JNIEnv *, jobject, jint, jint);
jint (* nativeWidgetCreate) (JNIEnv *, jobject, jint, jint, jint, jint, jint);
void (* nativeWidgetDelete) (JNIEnv *, jobject, jint);
void (* nativeWidgetResize) (JNIEnv *, jobject, jint, jint, jint, jint, jint, jboolean);
void (* nativeWidgetEnable) (JNIEnv *, jobject, jint, jboolean);
void (* nativeWidgetShow) (JNIEnv *, jobject, jint, jboolean);
void (* nativeWidgetInvalidate) (JNIEnv *, jobject, jint, jboolean);
void (* nativeWidgetUpdate) (JNIEnv *, jobject, jint);
void (* nativeProcessEvents) (JNIEnv *, jobject, jint);
jint (* nativeWebShellCreate) (JNIEnv *, jobject, jint, jint, jint, jint, jint);
void (* nativeWebShellDelete) (JNIEnv *, jobject, jint);
void (* nativeWebShellLoadURL) (JNIEnv *, jobject, jint, jstring);
void (* nativeWebShellStop) (JNIEnv *, jobject, jint);
void (* nativeWebShellShow) (JNIEnv *, jobject, jint);
void (* nativeWebShellHide) (JNIEnv *, jobject, jint);
void (* nativeWebShellSetBounds) (JNIEnv *, jobject, jint, jint, jint, jint, jint);
void (* nativeWebShellMoveTo) (JNIEnv *, jobject, jint, jint, jint);
void (* nativeWebShellSetFocus) (JNIEnv *, jobject, jint);
void (* nativeWebShellRemoveFocus) (JNIEnv *, jobject, jint);
void (* nativeWebShellRepaint) (JNIEnv *, jobject, jint, jboolean);
jboolean (* nativeWebShellCanBack) (JNIEnv *, jobject, jint);
jboolean (* nativeWebShellCanForward) (JNIEnv *, jobject, jint);
jboolean (* nativeWebShellBack) (JNIEnv *, jobject, jint);
jboolean (* nativeWebShellForward) (JNIEnv *, jobject, jint);
jboolean (* nativeWebShellGoTo) (JNIEnv *, jobject, jint, jint);
jint (* nativeWebShellGetHistoryLength) (JNIEnv *, jobject, jint);
jint (* nativeWebShellGetHistoryIndex) (JNIEnv *, jobject, jint);
jstring (* nativeWebShellGetURL) (JNIEnv *, jobject, jint, jint);
// added by Mark Goddard OTMP 9/2/1999
jboolean (* nativeWebShellRefresh)(JNIEnv *, jobject, jint);
jboolean (* nativeWebShellAddDocListener) (JNIEnv *, jobject, jint, jobject);
*/
// from org_mozilla_webclient_impl_wrapper_0005fnative_NativeEventThread.h
void (* nativeAddListener) (JNIEnv *, jobject, jint, jobject, jstring);
void (* nativeRemoveListener) (JNIEnv *, jobject, jint, jobject, jstring);
void (* nativeRemoveAllListeners) (JNIEnv *, jobject, jint);
void (* nativeInitialize) (JNIEnv *, jobject, jint);
void (* nativeProcessEvents) (JNIEnv *, jobject, jint);
// from org_mozilla_webclient_impl_wrapper_0005fnative_BookmarksImpl.h
jint (* nativeGetBookmarks) (JNIEnv *, jobject, jint);
jint (* nativeNewRDFNode)  (JNIEnv *, jobject, jint, jstring, jboolean);
// from PreferencesImpl.h
void (* nativeSetUnicharPref) (JNIEnv *, jobject, jint, jstring, jstring);
void (* nativeSetIntPref) (JNIEnv *, jobject, jint, jstring, jint);
void (* nativeSetBoolPref) (JNIEnv *, jobject, jint, jstring, jboolean);
jobject (* nativeGetPrefs) (JNIEnv *env, jobject obj, jint webShellPtr, jobject props);
void (* nativeRegisterPrefChangedCallback) (JNIEnv *env, jobject obj, jint webShellPtr, jobject callback, jstring prefName, jobject closure);
// from org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl.h
void (* nativeCopyCurrentSelectionToSystemClipboard) (JNIEnv *, jobject, jint);
void (* nativeFindInPage) (JNIEnv *, jobject, jint, jstring, jboolean, jboolean);
void (* nativeFindNextInPage) (JNIEnv *, jobject, jint);
jstring (* nativeGetCurrentURL) (JNIEnv *, jobject, jint);
void (* nativeResetFind) (JNIEnv *, jobject, jint);
void (* nativeSelectAll) (JNIEnv *, jobject, jint);
jobject (* nativeGetDOM) (JNIEnv *, jobject, jint);
// from org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl.h
void (* nativeBack) (JNIEnv *, jobject, jint);
jboolean (* nativeCanBack) (JNIEnv *, jobject, jint);
jboolean (* nativeCanForward) (JNIEnv *, jobject, jint);
void (* nativeClearHistory) (JNIEnv *, jobject, jint);
void (* nativeForward) (JNIEnv *, jobject, jint);
jobjectArray (* nativeGetBackList) (JNIEnv *, jobject, jint);
jint (* nativeGetCurrentHistoryIndex) (JNIEnv *, jobject, jint);
jobjectArray (* nativeGetForwardList) (JNIEnv *, jobject, jint);
jobjectArray (* nativeGetHistory) (JNIEnv *, jobject, jint);
jobject (* nativeGetHistoryEntry) (JNIEnv *, jobject, jint, jint);
jint (* nativeGetHistoryLength) (JNIEnv *, jobject, jint);
jstring (* nativeGetURLForIndex) (JNIEnv *, jobject, jint, jint);
void (* nativeSetCurrentHistoryIndex) (JNIEnv *, jobject, jint, jint);
// from org_mozilla_webclient_impl_wrapper_0005fnative_ISupportsPeer.h
void (* nativeAddRef) (JNIEnv *, jobject, jint);
void (* nativeRelease) (JNIEnv *, jobject, jint);
// from org_mozilla_webclient_impl_wrapper_0005fnative_NavigationImpl.h
void (* nativeLoadURL) (JNIEnv *, jobject, jint, jstring);
void (* nativeLoadFromStream) (JNIEnv *, jobject, jint, jobject, jstring, jstring, jint, jobject);
void (* nativeRefresh) (JNIEnv *, jobject, jint, jlong);
void (* nativeStop) (JNIEnv *, jobject, jint);
void (* nativeSetPrompt) (JNIEnv *, jobject, jint, jobject);
// from org_mozilla_webclient_impl_wrapper_0005fnative_RDFEnumeration.h
void (* nativeFinalize) (JNIEnv *, jobject, jint);
jboolean (* nativeHasMoreElements) (JNIEnv *, jobject, jint, jint);
jint (* nativeNextElement) (JNIEnv *, jobject, jint, jint);
// from org_mozilla_webclient_impl_wrapper_0005fnative_RDFTreeNode.h
jint (* nativeGetChildAt) (JNIEnv *, jobject, jint, jint, jint);
jint (* nativeGetChildCount) (JNIEnv *, jobject, jint, jint);
jint (* nativeGetIndex) (JNIEnv *, jobject, jint, jint, jint);
void (* nativeInsertElementAt) (JNIEnv *, jobject, jint, jint, jint, jobject, jint);
jint (* nativeNewFolder) (JNIEnv *, jobject, jint, jint, jobject);
jboolean (* nativeIsContainer) (JNIEnv *, jobject, jint, jint);
jboolean (* nativeIsLeaf) (JNIEnv *, jobject, jint, jint);
jstring (* nativeToString) (JNIEnv *, jobject, jint, jint);
// from org_mozilla_webclient_impl_wrapper_0005fnative_WindowControlImpl.h
jint (* nativeCreateInitContext) (JNIEnv *, jobject, jint, jint, jint, jint, jint, jobject);
void (* nativeMoveWindowTo) (JNIEnv *, jobject, jint, jint, jint);
void (* nativeRemoveFocus) (JNIEnv *, jobject, jint);
void (* nativeRepaint) (JNIEnv *, jobject, jint, jboolean);
void (* nativeSetBounds) (JNIEnv *, jobject, jint, jint, jint, jint, jint);
void (* nativeSetFocus) (JNIEnv *, jobject, jint);
void (* nativeSetVisible) (JNIEnv *, jobject, jint, jboolean);
void (* nativeDestroyInitContext) (JNIEnv *, jobject, jint);
//from org_mozilla_webclient_impl_wrapper_0005fnative_WrapperFactoryImpl.h
jboolean (* nativeDoesImplement) (JNIEnv *, jobject, jstring);
void (* nativeAppInitialize) (JNIEnv *, jobject, jstring);
void (* nativeTerminate) (JNIEnv *, jobject);


void locateBrowserControlStubFunctions(void * dll) {
  nativeDoesImplement = (jboolean (*) (JNIEnv *, jobject, jstring)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WrapperFactoryImpl_nativeDoesImplement");
  if (!nativeDoesImplement) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeAppInitialize = (void (*) (JNIEnv *, jobject, jstring)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WrapperFactoryImpl_nativeAppInitialize");
  if (!nativeAppInitialize) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeTerminate = (void (*) (JNIEnv *, jobject)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WrapperFactoryImpl_nativeTerminate");
  if (!nativeTerminate) {
    printf("got dlsym error %s\n", dlerror());
  }

  nativeDestroyInitContext = (void(*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeDestroyInitContext");
  if (!nativeDestroyInitContext) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeCreateInitContext = (jint (*) (JNIEnv *, jobject, jint, jint, jint, jint, jint, jobject)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeCreateInitContext");
  if (!nativeCreateInitContext) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeMoveWindowTo = (void (*) (JNIEnv *, jobject, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeMoveWindowTo");
  if (!nativeMoveWindowTo) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeRemoveFocus = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeRemoveFocus");
  if (!nativeRemoveFocus) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeRepaint = (void (*) (JNIEnv *, jobject, jint, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeRepaint");
  if (!nativeRepaint) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeSetBounds = (void (*) (JNIEnv *, jobject, jint, jint, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeSetBounds");
  if (!nativeSetBounds) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeSetFocus = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeSetFocus");
  if (!nativeSetFocus) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeSetVisible = (void (*) (JNIEnv *, jobject, jint, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeSetVisible");
  if (!nativeSetVisible) {
    printf("got dlsym error %s\n", dlerror());
  }

  nativeGetChildAt = (jint (*) (JNIEnv *, jobject, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeGetChildAt");
  if (!nativeGetChildAt) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeGetChildCount = (jint (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeGetChildCount");
  if (!nativeGetChildCount) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeGetIndex = (jint (*) (JNIEnv *, jobject, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeGetIndex");
  if (!nativeGetIndex) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeInsertElementAt = (void (*) (JNIEnv *, jobject, jint, jint, jint, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeInsertElementAt");
  if (!nativeInsertElementAt) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeNewFolder = (jint (*) (JNIEnv *, jobject, jint, jint, jobject)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeNewFolder");
  if (!nativeNewFolder) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeIsContainer = (jboolean (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeIsContainer");
  if (!nativeIsContainer) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeIsLeaf = (jboolean (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeIsLeaf");
  if (!nativeIsLeaf) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeToString = (jstring (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeToString");
  if (!nativeToString) {
    printf("got dlsym error %s\n", dlerror());
  }

  nativeFinalize = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFEnumeration_nativeFinalize");
  if (!nativeFinalize) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeHasMoreElements = (jboolean (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFEnumeration_nativeHasMoreElements");
  if (!nativeHasMoreElements) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeNextElement = (jint (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_RDFEnumeration_nativeNextElement");
  if (!nativeNextElement) {
    printf("got dlsym error %s\n", dlerror());
  }

  nativeLoadURL = (void (*) (JNIEnv *, jobject, jint, jstring)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeLoadURL");
  if (!nativeLoadURL) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeLoadFromStream = (void (*) (JNIEnv *, jobject, jint, jobject, jstring, jstring, jint, jobject)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeLoadFromStream");
  if (!nativeLoadFromStream) {
      printf("got dlsym error %s\n", dlerror());
  }
  nativeRefresh = (void (*) (JNIEnv *, jobject, jint, jlong)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeRefresh");
  if (!nativeRefresh) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeStop = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeStop");
  if (!nativeStop) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeSetPrompt = (void (*) (JNIEnv *, jobject, jint, jobject)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeSetPrompt");
  if (!nativeSetPrompt) {
    printf("got dlsym error %s\n", dlerror());
  }

  nativeAddRef = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_ISupportsPeer_nativeAddRef");
  if (!nativeAddRef) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeRelease = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_ISupportsPeer_nativeRelease");
  if (!nativeRelease) {
    printf("got dlsym error %s\n", dlerror());
  }

  nativeBack = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeBack");
  if (!nativeBack) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeCanBack = (jboolean (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeCanBack");
  if (!nativeCanBack) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeCanForward = (jboolean (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeCanForward");
  if (!nativeCanForward) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeClearHistory = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeClearHistory");
  if (!nativeClearHistory) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeForward = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeForward");
  if (!nativeForward) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeGetBackList = (jobjectArray (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetBackList");
  if (!nativeGetBackList) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeGetCurrentHistoryIndex = (jint (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetCurrentHistoryIndex");
  if (!nativeGetCurrentHistoryIndex) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeGetForwardList = (jobjectArray (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetForwardList");
  if (!nativeGetForwardList) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeGetHistory = (jobjectArray (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetHistory");
  if (!nativeGetHistory) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeGetHistoryEntry = (jobject (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetHistoryEntry");
  if (!nativeGetHistoryEntry) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeGetHistoryLength = (jint (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetHistoryLength");
  if (!nativeGetHistoryLength) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeGetURLForIndex = (jstring (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetURLForIndex");
  if (!nativeGetURLForIndex) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeSetCurrentHistoryIndex = (void (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeSetCurrentHistoryIndex");
  if (!nativeSetCurrentHistoryIndex) {
    printf("got dlsym error %s\n", dlerror());
  }

  nativeCopyCurrentSelectionToSystemClipboard = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeCopyCurrentSelectionToSystemClipboard");
  if (!nativeCopyCurrentSelectionToSystemClipboard) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeFindInPage = (void (*) (JNIEnv *, jobject, jint, jstring, jboolean, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeFindInPage");
  if (!nativeFindInPage) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeFindNextInPage = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeFindNextInPage");
  if (!nativeFindNextInPage) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeGetCurrentURL = (jstring (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeGetCurrentURL");
  if (!nativeGetCurrentURL) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeResetFind = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeResetFind");
  if (!nativeResetFind) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeSelectAll = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeSelectAll");
  if (!nativeSelectAll) {
    printf("got dlsym error %s\n", dlerror());
  }

  nativeGetDOM = (jobject (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeGetDOM");
  if (!nativeGetDOM) {
    printf("got dlsym error %s\n", dlerror());
  }

  nativeGetBookmarks = (jint (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_BookmarksImpl_nativeGetBookmarks");
  if (!nativeGetBookmarks) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeNewRDFNode = (jint (*) (JNIEnv *, jobject, jint, jstring, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_BookmarksImpl_nativeNewRDFNode");
  if (!nativeNewRDFNode) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeSetUnicharPref = (void (*) (JNIEnv *, jobject, jint, jstring, jstring)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeSetUnicharPref");
  if (!nativeSetUnicharPref) {
      printf("got dlsym error %s\n", dlerror());
  }
  nativeSetIntPref = (void (*) (JNIEnv *, jobject, jint, jstring, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeSetIntPref");
  if (!nativeSetIntPref) {
      printf("got dlsym error %s\n", dlerror());
  }
  nativeSetBoolPref = (void (*) (JNIEnv *, jobject, jint, jstring, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeSetBoolPref");
  if (!nativeSetBoolPref) {
      printf("got dlsym error %s\n", dlerror());
  }
  nativeGetPrefs = (jobject (*) (JNIEnv *env, jobject obj, jint webShellPtr, jobject props)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeGetPrefs");
  if (!nativeGetPrefs) {
      printf("got dlsym error %s\n", dlerror());
  }
  nativeRegisterPrefChangedCallback = (void (*) (JNIEnv *env, jobject obj, jint webShellPtr, jobject callback, jstring prefName, jobject closure)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_PreferencesImpl_nativeRegisterPrefChangedCallback");
  if (!nativeRegisterPrefChangedCallback) {
      printf("got dlsym error %s\n", dlerror());
  }
  nativeAddListener = (void (*) (JNIEnv *, jobject, jint, jobject, jstring)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeAddListener");
  if (!nativeAddListener) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeRemoveListener = (void (*) (JNIEnv *, jobject, jint, jobject, jstring)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeRemoveListener");
  if (!nativeRemoveListener) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeRemoveAllListeners = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeRemoveAllListeners");
  if (!nativeRemoveAllListeners) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeInitialize = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeInitialize");
  if (!nativeInitialize) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeProcessEvents = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeProcessEvents");
  if (!nativeProcessEvents) {
    printf("got dlsym error %s\n", dlerror());
  }
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



void loadMainDll(void)
{
   printf ("Inside NativeLoaderStub.cpp::loadMainDll\n");
    webClientDll = dlopen("libwebclient.so", RTLD_LAZY | RTLD_GLOBAL);
    if (webClientDll) {
        locateBrowserControlStubFunctions(webClientDll);
        locateMotifBrowserControlStubFunctions(webClientDll);
    } else {
        printf("Got Error: %s\n", dlerror());
    }
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
JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_WrapperFactoryImpl_nativeAppInitialize
(JNIEnv *env, jobject obj, jstring verifiedBinDirAbsolutePath) {
  (* nativeAppInitialize) (env, obj, verifiedBinDirAbsolutePath);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_WrapperFactoryImpl
 * Method:    nativeTerminate
 * Signature: ()V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_WrapperFactoryImpl_nativeTerminate
(JNIEnv * env, jobject obj) {
  (* nativeTerminate) (env, obj);
}



/// Copied from MotifBrowserControlCanvasStub.cpp



// EOF
