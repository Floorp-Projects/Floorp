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

#include <jni.h>

#include "BookmarksImpl.h"
#include "PreferencesImpl.h"
#include "CurrentPageImpl.h"
#include "HistoryImpl.h"
#include "ISupportsPeer.h"
#include "NavigationImpl.h"
#include "RDFEnumeration.h"
#include "RDFTreeNode.h"
#include "WindowControlImpl.h"
#include "WrapperFactoryImpl.h"
#include "NativeEventThread.h"

#include "GtkBrowserControlCanvas.h"

#define WEBCLIENTSTUB_LOG_MODULE "webclientstub"
#define WEBCLIENT_DSO "libwebclient.so"

/**

* Hooks up the local function pointers to the symbols in the <b>real</b>
* webclient DSO.

* @return 0 on success, -1 on failure

*/

int locateStubFunctions(void *dll);

// PENDING(edburns): implement a data structure based scheme to ease
// adding functions.

// from GtkBrowserControlCanvas.h

jint (* createTopLevelWindow) (JNIEnv *, jobject);
jint (* createContainerWindow) (JNIEnv *, jobject, jint, jint, jint);
jint (* getGTKWinID) (JNIEnv *, jobject, jint);
void (* reparentWindow) (JNIEnv *, jobject, jint, jint);
void (* processEvents) (JNIEnv *, jobject);
void (* setGTKWindowSize) (JNIEnv *, jobject, jint, jint, jint);
jint (* getHandleToPeer) (JNIEnv *, jobject);

// from NativeEventThread.h
void (* nativeAddListener) (JNIEnv *, jobject, jint, jobject, jstring);
void (* nativeRemoveListener) (JNIEnv *, jobject, jint, jobject, jstring);
void (* nativeRemoveAllListeners) (JNIEnv *, jobject, jint);
void (* nativeInitialize) (JNIEnv *, jobject, jint);
void (* nativeProcessEvents) (JNIEnv *, jobject, jint);
// from BookmarksImpl.h
jint (* nativeGetBookmarks) (JNIEnv *, jobject, jint);
jint (* nativeNewRDFNode)  (JNIEnv *, jobject, jint, jstring, jboolean);
// from PreferencesImpl.h
void (* nativeSetUnicharPref) (JNIEnv *, jobject, jint, jstring, jstring);
void (* nativeSetIntPref) (JNIEnv *, jobject, jint, jstring, jint);
void (* nativeSetBoolPref) (JNIEnv *, jobject, jint, jstring, jboolean);
jobject (* nativeGetPrefs) (JNIEnv *env, jobject obj, jint webShellPtr, jobject props);
void (* nativeRegisterPrefChangedCallback) (JNIEnv *env, jobject obj, jint webShellPtr, jobject callback, jstring prefName, jobject closure);
// from CurrentPageImpl.h
void (* nativeCopyCurrentSelectionToSystemClipboard) (JNIEnv *, jobject, jint);
void (* nativeFindInPage) (JNIEnv *, jobject, jint, jstring, jboolean, jboolean);
void (* nativeFindNextInPage) (JNIEnv *, jobject, jint);
jstring (* nativeGetCurrentURL) (JNIEnv *, jobject, jint);
void (* nativeResetFind) (JNIEnv *, jobject, jint);
void (* nativeSelectAll) (JNIEnv *, jobject, jint);
jobject (* nativeGetDOM) (JNIEnv *, jobject, jint);
// from HistoryImpl.h
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
// from ISupportsPeer.h
void (* nativeAddRef) (JNIEnv *, jobject, jint);
void (* nativeRelease) (JNIEnv *, jobject, jint);
// from NavigationImpl.h
void (* nativeLoadURL) (JNIEnv *, jobject, jint, jstring);
void (* nativeLoadFromStream) (JNIEnv *, jobject, jint, jobject, jstring, jstring, jint, jobject);
void (* nativeRefresh) (JNIEnv *, jobject, jint, jlong);
void (* nativeStop) (JNIEnv *, jobject, jint);
void (* nativeSetPrompt) (JNIEnv *, jobject, jint, jobject);
// from RDFEnumeration.h
void (* nativeFinalize) (JNIEnv *, jobject, jint);
jboolean (* nativeHasMoreElements) (JNIEnv *, jobject, jint, jint);
jint (* nativeNextElement) (JNIEnv *, jobject, jint, jint);
// from RDFTreeNode.h
jint (* nativeGetChildAt) (JNIEnv *, jobject, jint, jint, jint);
jint (* nativeGetChildCount) (JNIEnv *, jobject, jint, jint);
jint (* nativeGetIndex) (JNIEnv *, jobject, jint, jint, jint);
void (* nativeInsertElementAt) (JNIEnv *, jobject, jint, jint, jint, jobject, jint);
jint (* nativeNewFolder) (JNIEnv *, jobject, jint, jint, jobject);
jboolean (* nativeIsContainer) (JNIEnv *, jobject, jint, jint);
jboolean (* nativeIsLeaf) (JNIEnv *, jobject, jint, jint);
jstring (* nativeToString) (JNIEnv *, jobject, jint, jint);
// from WindowControlImpl.h
jint (* nativeCreateInitContext) (JNIEnv *, jobject, jint, jint, jint, jint, jint, jobject);
void (* nativeMoveWindowTo) (JNIEnv *, jobject, jint, jint, jint);
void (* nativeRemoveFocus) (JNIEnv *, jobject, jint);
void (* nativeRepaint) (JNIEnv *, jobject, jint, jboolean);
void (* nativeSetBounds) (JNIEnv *, jobject, jint, jint, jint, jint, jint);
void (* nativeSetFocus) (JNIEnv *, jobject, jint);
void (* nativeSetVisible) (JNIEnv *, jobject, jint, jboolean);
void (* nativeDestroyInitContext) (JNIEnv *, jobject, jint);
//from WrapperFactoryImpl.h
jboolean (* nativeDoesImplement) (JNIEnv *, jobject, jstring);
void (* nativeAppInitialize) (JNIEnv *, jobject, jstring);
void (* nativeTerminate) (JNIEnv *, jobject);

