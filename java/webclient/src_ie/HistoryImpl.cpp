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
 *                 Ron Capelli <capelli@us.ibm.com>
 */

#include "org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl.h"
#include "ie_util.h"
#include "ie_globals.h"

#include "CMyDialog.h"

JNIEXPORT void JNICALL
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeBack
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == NULL) {
        ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to Back");
        return;
    }

    HRESULT hr = PostMessage(initContext->browserHost, WM_BACK, 0, 0);
}

JNIEXPORT jboolean
JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeCanBack
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == NULL) {
        ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to CanBack");
        return JNI_FALSE;
    }

    jboolean result = initContext->browserObject->GetBackState();

    // printf("src_ie/HistoryImpl nativeCanBack returns %d\n", result);
    return result;
}

JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetBackList
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    printf("src_ie/HistoryImpl nativeGetBackList\n");
    jobjectArray result = 0;

    return result;
}

JNIEXPORT void JNICALL
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeClearHistory
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    printf("src_ie/HistoryImpl nativeClearHistory\n");
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeForward
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == NULL) {
        ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to Forward");
    }

    HRESULT hr = PostMessage(initContext->browserHost, WM_FORWARD, 0, 0);
}

JNIEXPORT jboolean JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeCanForward
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == NULL) {
        ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to CanForward");
        return JNI_FALSE;
    }

    jboolean result = initContext->browserObject->GetForwardState();

    // printf("src_ie/HistoryImpl nativeCanForward returns %d\n", result);
    return result;
}

JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetForwardList
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    printf("src_ie/HistoryImpl nativeGetForwardList\n");
    jobjectArray result = 0;

    return result;
}

JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetHistory
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    printf("src_ie/HistoryImpl nativeGetHistory\n");
    jobjectArray result = 0;

    return result;
}

JNIEXPORT jobject JNICALL
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetHistoryEntry
(JNIEnv *env, jobject obj, jint webShellPtr, jint historyIndex)
{
    printf("src_ie/HistoryImpl nativeGetHistoryEntry(%d)\n", historyIndex);
    jobject result = 0;

    return result;
}

JNIEXPORT jint JNICALL
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetCurrentHistoryIndex
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    // printf("src_ie/HistoryImpl nativeGetCurrentHistoryIndex\n");

    DWORD count = -1;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == NULL) {
        ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to GetCurrentHistoryIndex");
    }

    // current index is number of back entries
    initContext->browserObject->m_pTLStg->GetCount(TLEF_RELATIVE_BACK, &count);

    jint result = count;

    // printf("src_ie/HistoryImpl nativeGetCurrentHistoryIndex returns %d\n", result);
    return result;
}

JNIEXPORT void JNICALL
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeSetCurrentHistoryIndex
(JNIEnv *env, jobject obj, jint webShellPtr, jint historyIndex)
{
    // printf("src_ie/HistoryImpl nativeSetCurrentHistoryIndex(%d)\n", historyIndex);

    DWORD count = -1;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == NULL) {
        ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to SetCurrentHistoryIndex");
    }

    // current index is number of back entries
    initContext->browserObject->m_pTLStg->GetCount(TLEF_RELATIVE_BACK, &count);

    // Perform navigation on browser thread
    PostMessage(initContext->browserHost, WM_TRAVELTO, 0, historyIndex - count);
}

JNIEXPORT jint JNICALL
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetHistoryLength
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    // printf("src_ie/HistoryImpl nativeGetHistoryLength\n");

    DWORD count = -1;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == NULL) {
        ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to GetHistoryLength");
    }

    initContext->browserObject->m_pTLStg->GetCount(TLEF_ABSOLUTE, &count);

    jint result = count;

    // printf("src_ie/HistoryImpl nativeHistoryLength returns %d\n", result);
    return result;
}

JNIEXPORT jstring JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetURLForIndex
(JNIEnv *env, jobject obj, jint webShellPtr, jint historyIndex)
{
    // printf("src_ie/HistoryImpl nativeGetURLForIndex(%d)\n", historyIndex);

    ITravelLogEntry *pTLEntry = NULL;
    DWORD count = -1;
    LPOLESTR szURL = NULL;
    jstring urlString = 0;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == NULL) {
        ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to GetURLForIndex");
    }

    // current index is number of back entries
    initContext->browserObject->m_pTLStg->GetCount(TLEF_RELATIVE_BACK, &count);
    // convert index to Travel Log offset and get entry
    initContext->browserObject->m_pTLStg->GetRelativeEntry(historyIndex - count, &pTLEntry);

    // obtain URL for entry
    pTLEntry->GetURL(&szURL);
    pTLEntry->Release();

    if (szURL) {
	urlString = ::util_NewString(env, szURL, wcslen(szURL));
    }
    else {
	::util_ThrowExceptionToJava(env, "Exception: GetURLForIndex returned null string");
    }

    return urlString;
}

