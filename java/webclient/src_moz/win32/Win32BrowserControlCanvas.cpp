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
 * Contributor(s): Ashu Kulkarni
 *                 Ron Capelli (capelli@us.ibm.com)
 */


/*
 * Win32BrowserControlCanvas.cpp
 */

#include <jni.h>
#include <jawt_md.h>
#include <jawt.h>

typedef jboolean (JNICALL *PJAWT_GETAWT)(JNIEnv*, JAWT*);

#include "org_mozilla_webclient_impl_wrapper_0005fnative_Win32BrowserControlCanvas.h"
#include "jni_util.h" //for throwing Exceptions to Java


/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_Win32BrowserControlCanvas
 * Method:    getHandleToPeer
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_Win32BrowserControlCanvas_getHandleToPeer
  (JNIEnv *env, jobject canvas) {
    JAWT awt;
    JAWT_DrawingSurface* ds;
    JAWT_DrawingSurfaceInfo* dsi;
    JAWT_Win32DrawingSurfaceInfo* dsi_win;
    HWND handle_win;
    HMODULE _hAWT;     // JAWT module handle
    jint lock;

    PJAWT_GETAWT pJAWT_GetAWT;  // JAWT_GetAWT function pointer

    //Get the AWT
    _hAWT = LoadLibrary("jawt.dll");
    if (!_hAWT) {
	 printf(" +++ No jawt.dll... Trying awt.dll +++ \n");
	 _hAWT = LoadLibrary("awt.dll");   // IBM Java 1.3.x packages JAWT_GetAWT in awt.dll
    }
    if (!_hAWT) {
        printf(" +++ JAWT DLL Not Found +++ \n");
	::util_ThrowExceptionToJava(env, "Exception: JAWT DLL Not Found");
	return 0;
    }

    pJAWT_GetAWT = (PJAWT_GETAWT)GetProcAddress(_hAWT, "_JAWT_GetAWT@8");
    if (!pJAWT_GetAWT) {
        printf(" +++ JAWT_GetAWT Entry Not Found +++ \n");
	::util_ThrowExceptionToJava(env, "Exception: JAWT_GetAWT Entry Not Found");
	FreeLibrary(_hAWT);
	return 0;
    }

    awt.version = JAWT_VERSION_1_3;
    if (pJAWT_GetAWT(env, &awt) == JNI_FALSE) {
        printf(" +++ AWT Not Found +++ \n");
        ::util_ThrowExceptionToJava(env, "Exception: AWT Not Found");
	FreeLibrary(_hAWT);
        return 0;
    }

    //Get the Drawing Surface
    ds = awt.GetDrawingSurface(env, canvas);
    if (ds == NULL) {
        printf(" +++ NULL Drawing Surface +++ \n");
        ::util_ThrowExceptionToJava(env, "Exception: Null Drawing Surface");
	FreeLibrary(_hAWT);
        return 0;
    }

    //Lock the Drawing Surface
    lock = ds->Lock(ds);
    if ((lock & JAWT_LOCK_ERROR) != 0) {
        printf(" +++ Error Locking Surface +++ \n");
        ::util_ThrowExceptionToJava(env, "Exception: Error Locking Surface");
        awt.FreeDrawingSurface(ds);
	FreeLibrary(_hAWT);
        return 0;
    }

    //Get the Drawing Surface info
    dsi = ds->GetDrawingSurfaceInfo(ds);
    if (dsi == NULL) {
        printf(" +++ Error Getting Surface Info +++ \n");
        ::util_ThrowExceptionToJava(env, "Exception: Error Getting Surface Info");
        ds->Unlock(ds);
        awt.FreeDrawingSurface(ds);
	FreeLibrary(_hAWT);
        return 0;
    }

    //Get the Platform specific Drawing Info
    dsi_win = (JAWT_Win32DrawingSurfaceInfo*)dsi->platformInfo;

    //Get the Handle to the Native Drawing Surface info
    handle_win = (HWND) dsi_win->hwnd;

    //Clean up after us
    ds->FreeDrawingSurfaceInfo(dsi);
    ds->Unlock(ds);
    awt.FreeDrawingSurface(ds);
    FreeLibrary(_hAWT);

    //return the native peer handle
    return (jint) handle_win;
}
