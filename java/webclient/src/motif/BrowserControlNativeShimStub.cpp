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

// For loading DLL
#include <dlfcn.h>
// JNI...yada, yada, yada
#include <jni.h>
// JNI Header
#include "../BrowserControlNativeShim.h"
// JNI Header
#include "NativeEventThread.h"

// allow code in webclientstub.so to load us
#include "BrowserControlNativeShimStub.h"

// Global reference to webclient dll
void * webClientDll;

extern void locateMotifBrowserControlStubFunctions(void *);

void (* nativeInitialize) (JNIEnv *, jobject, jstring);
void (* nativeTerminate) (JNIEnv *, jobject);
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
void (* processNativeEventQueue) (JNIEnv *, jobject, jint);


void locateBrowserControlStubFunctions(void * dll) {
  nativeInitialize = (void (*) (JNIEnv *, jobject, jstring)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeInitialize");
  if (!nativeInitialize) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeTerminate = (void (*) (JNIEnv *, jobject)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeTerminate");
  if (!nativeTerminate) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellCreate = (jint (*) (JNIEnv *, jobject, jint, jint, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellCreate");
  if (!nativeWebShellCreate) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellDelete = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellDelete");
  if (!nativeWebShellDelete) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellLoadURL = (void (*) (JNIEnv *, jobject, jint, jstring)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellLoadURL");
  if (!nativeWebShellLoadURL) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellStop = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellStop");
  if (!nativeWebShellStop) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellShow = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellShow");
  if (!nativeWebShellShow) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellHide = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellHide");
  if (!nativeWebShellHide) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellSetBounds = (void (*) (JNIEnv *, jobject, jint, jint, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellSetBounds");
  if (!nativeWebShellSetBounds) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellMoveTo = (void (*) (JNIEnv *, jobject, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellMoveTo");
  if (!nativeWebShellMoveTo) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellSetFocus = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellSetFocus");
  if (!nativeWebShellSetFocus) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellRemoveFocus = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellRemoveFocus");
  if (!nativeWebShellRemoveFocus) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellRepaint = (void (*) (JNIEnv *, jobject, jint, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellRepaint");
  if (!nativeWebShellRepaint) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellCanBack = (jboolean (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellCanBack");
  if (!nativeWebShellCanBack) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellCanForward = (jboolean (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellCanForward");
  if (!nativeWebShellCanForward) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellBack = (jboolean (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellBack");
  if (!nativeWebShellBack) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellForward = (jboolean (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellForward");
  if (!nativeWebShellForward) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellGoTo = (jboolean (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGoTo");
  if (!nativeWebShellGoTo) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellGetHistoryLength = (jint (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGetHistoryLength");
  if (!nativeWebShellGetHistoryLength) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellGetHistoryIndex = (jint (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGetHistoryIndex");
  if (!nativeWebShellGetHistoryIndex) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellGetURL = (jstring (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGetURL");
  if (!nativeWebShellGetURL) {
    printf("got dlsym error %s\n", dlerror());
  }
  
  // added by Mark Goddard OTMP 9/2/1999
  nativeWebShellRefresh = (jboolean (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellRefresh");
  if (!nativeWebShellRefresh) {
    printf("got dlsym error %s\n", dlerror());
  }

  nativeWebShellAddDocListener = (jboolean (*) (JNIEnv *, jobject, jint, jobject)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellAddDocListener");
  if (!nativeWebShellAddDocListener) {
      printf("got dlsym error %s\n", dlerror());
  }


  processNativeEventQueue = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_motif_NativeEventThread_processNativeEventQueue");
  if (!processNativeEventQueue) {
    printf("got dlsym error %s\n", dlerror());
  }
}

/*
 * Class:     org_mozilla_webclient_motif_MotifNativeEventQueue
 * Method:    processNativeEventQueue
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_motif_NativeEventThread_processNativeEventQueue
    (JNIEnv * env, jobject obj, jint gtkWinPtr) {
    (* processNativeEventQueue) (env, obj, gtkWinPtr);
}

void loadMainDll(void)
{
    webClientDll = dlopen("libwebclient.so", RTLD_LAZY | RTLD_GLOBAL);
    
    if (webClientDll) {
        locateBrowserControlStubFunctions(webClientDll);
        locateMotifBrowserControlStubFunctions(webClientDll);
    } else {
        printf("Got Error: %s\n", dlerror());
    }
}



/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorInitialize
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeInitialize (
	JNIEnv		*	env,
	jobject			obj, jstring verifiedBinDirAbsolutePath)
{
    (* nativeInitialize) (env, obj, verifiedBinDirAbsolutePath);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeInitialize()



/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorTerminate
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeTerminate (
	JNIEnv	*	env,
	jobject		obj)
{
  (* nativeTerminate) (env, obj);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeTerminate()

/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellCreate
 * Signature: (IIIIII)I
 */
JNIEXPORT jint JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellCreate (
	JNIEnv		*	env,
	jobject			obj,
	jint			windowPtr,
	jint			x,
	jint			y,
	jint			width,
	jint			height)
{
  return (* nativeWebShellCreate) (env, obj, windowPtr, x, y, width, height);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellCreate()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellDelete
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellDelete (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  (* nativeWebShellDelete) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellDelete()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellLoadURL
 * Signature: (ILjava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellLoadURL (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jstring		urlString)
{
  (* nativeWebShellLoadURL) (env, obj, webShellPtr, urlString);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellLoadURL()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellStop
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellStop (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  (* nativeWebShellStop) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellStop()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellShow
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellShow (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  (* nativeWebShellShow) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellShow()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellHide
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellHide (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  (* nativeWebShellHide) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellHide()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellSetBounds
 * Signature: (IIIII)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellSetBounds (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jint		x,
	jint		y,
	jint		width,
	jint		height)
{
  (* nativeWebShellSetBounds) (env, obj, webShellPtr, x, y, width, height);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellSetBounds()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellMoveTo
 * Signature: (III)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellMoveTo (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jint		x,
	jint		y)
{
  (* nativeWebShellMoveTo) (env, obj, webShellPtr, x, y);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellMoveTo()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellSetFocus
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellSetFocus (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  (* nativeWebShellSetFocus) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellSetFocus()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellRemoveFocus
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellRemoveFocus (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  (* nativeWebShellRemoveFocus) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellRemoveFocus()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellRepaint
 * Signature: (IZ)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellRepaint (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jboolean	forceRepaint)
{
  (* nativeWebShellRepaint) (env, obj, webShellPtr, forceRepaint);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellRepaint()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellCanBack
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellCanBack (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  return (* nativeWebShellCanBack) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellCanBack()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellCanForward
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellCanForward (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  return (* nativeWebShellCanForward) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellCanForward()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellBack
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellBack (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  return (* nativeWebShellBack) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellBack()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellForward
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellForward (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  return (* nativeWebShellForward) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellForward()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellGoTo
 * Signature: (II)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGoTo (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jint		historyIndex)
{
  return (* nativeWebShellGoTo) (env, obj, webShellPtr, historyIndex);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGoTo()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellGetHistoryLength
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGetHistoryLength (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  return (* nativeWebShellGetHistoryLength) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGetHistoryLength()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellGetHistoryIndex
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGetHistoryIndex (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  return (* nativeWebShellGetHistoryIndex) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGetHistoryIndex()


/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellGetURL
 * Signature: (II)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGetURL (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jint		historyIndex)
{
  return (* nativeWebShellGetURL) (env, obj, webShellPtr, historyIndex);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGetURL()

// added by Mark Goddard OTMP 9/2/1999
/*
 * Class:     MozWebShellNativeShim
 * Method:    raptorWebShellRefresh
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellRefresh (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  return (* nativeWebShellRefresh) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellRefresh()


/*
 * Class:     BrowserControlNativeShimStub
 * Method:    nativeWebShellAddDocListener
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellAddDocListener (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
    jobject     listener)
{
  return (* nativeWebShellAddDocListener) (env, obj, webShellPtr, listener);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellAddDocListener()
 

// EOF
