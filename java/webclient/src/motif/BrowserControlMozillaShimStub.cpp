/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * BrowserControlMozillaShimStub.cpp
 */

#include <dlfcn.h>
#include <jni.h>
#include "../BrowserControlMozillaShim.h"

void * mozWebShellDll;

extern void locateMotifBrowserControlStubFunctions(void *);

void (* nativeInitialize) (JNIEnv *, jobject);
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

void locateBrowserControlStubFunctions(void * dll) {
  nativeInitialize = (void (*) (JNIEnv *, jobject)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeInitialize");
  if (!nativeInitialize) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeTerminate = (void (*) (JNIEnv *, jobject)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeTerminate");
  if (!nativeTerminate) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeSendKeyDownEvent = (void (*) (JNIEnv *, jobject, jint, jchar, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeSendKeyDownEvent");
  if (!nativeSendKeyDownEvent) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeSendKeyUpEvent = (void (*) (JNIEnv *, jobject, jint, jchar, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeSendKeyUpEvent");
  if (!nativeSendKeyUpEvent) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeSendMouseEvent = (void (*) (JNIEnv *, jobject, jint, jint, jint, jint, jint, jint, jint, jint,jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeSendMouseEvent");
  if (!nativeSendMouseEvent) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeIdleEvent = (void (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeIdleEvent");
  if (!nativeIdleEvent) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeUpdateEvent = (void (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeUpdateEvent");
  if (!nativeUpdateEvent) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWidgetCreate = (jint (*) (JNIEnv *, jobject, jint, jint, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetCreate");
  if (!nativeWidgetCreate) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWidgetDelete = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetDelete");
  if (!nativeWidgetDelete) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWidgetResize = (void (*) (JNIEnv *, jobject, jint, jint, jint, jint, jint, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetResize");
  if (!nativeWidgetResize) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWidgetEnable = (void (*) (JNIEnv *, jobject, jint, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetEnable");
  if (!nativeWidgetEnable) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWidgetShow = (void (*) (JNIEnv *, jobject, jint, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetShow");
  if (!nativeWidgetShow) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWidgetInvalidate = (void (*) (JNIEnv *, jobject, jint, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetInvalidate");
  if (!nativeWidgetInvalidate) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWidgetUpdate = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetUpdate");
  if (!nativeWidgetUpdate) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeProcessEvents = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeProcessEvents");
  if (!nativeProcessEvents) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellCreate = (jint (*) (JNIEnv *, jobject, jint, jint, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellCreate");
  if (!nativeWebShellCreate) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellDelete = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellDelete");
  if (!nativeWebShellDelete) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellLoadURL = (void (*) (JNIEnv *, jobject, jint, jstring)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellLoadURL");
  if (!nativeWebShellLoadURL) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellStop = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellStop");
  if (!nativeWebShellStop) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellShow = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellShow");
  if (!nativeWebShellShow) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellHide = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellHide");
  if (!nativeWebShellHide) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellSetBounds = (void (*) (JNIEnv *, jobject, jint, jint, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellSetBounds");
  if (!nativeWebShellSetBounds) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellMoveTo = (void (*) (JNIEnv *, jobject, jint, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellMoveTo");
  if (!nativeWebShellMoveTo) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellSetFocus = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellSetFocus");
  if (!nativeWebShellSetFocus) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellRemoveFocus = (void (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellRemoveFocus");
  if (!nativeWebShellRemoveFocus) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellRepaint = (void (*) (JNIEnv *, jobject, jint, jboolean)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellRepaint");
  if (!nativeWebShellRepaint) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellCanBack = (jboolean (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellCanBack");
  if (!nativeWebShellCanBack) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellCanForward = (jboolean (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellCanForward");
  if (!nativeWebShellCanForward) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellBack = (jboolean (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellBack");
  if (!nativeWebShellBack) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellForward = (jboolean (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellForward");
  if (!nativeWebShellForward) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellGoTo = (jboolean (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellGoTo");
  if (!nativeWebShellGoTo) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellGetHistoryLength = (jint (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellGetHistoryLength");
  if (!nativeWebShellGetHistoryLength) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellGetHistoryIndex = (jint (*) (JNIEnv *, jobject, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellGetHistoryIndex");
  if (!nativeWebShellGetHistoryIndex) {
    printf("got dlsym error %s\n", dlerror());
  }
  nativeWebShellGetURL = (jstring (*) (JNIEnv *, jobject, jint, jint)) dlsym(dll, "Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellGetURL");
  if (!nativeWebShellGetURL) {
    printf("got dlsym error %s\n", dlerror());
  }
}


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorInitialize
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeInitialize (
	JNIEnv		*	env,
	jobject			obj)
{
  mozWebShellDll = dlopen("libwebclient.so", RTLD_LAZY | RTLD_GLOBAL);

  if (mozWebShellDll) {
    locateBrowserControlStubFunctions(mozWebShellDll);
    locateMotifBrowserControlStubFunctions(mozWebShellDll);
    
    (* nativeInitialize) (env, obj);
  } else {
    printf("Got Error: %s\n", dlerror());
  }

} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeInitialize()



/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorTerminate
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeTerminate (
	JNIEnv	*	env,
	jobject		obj)
{
  (* nativeTerminate) (env, obj);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeTerminate()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorSendKeyDownEvent
 * Signature: (ICIII)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeSendKeyDownEvent (
	JNIEnv	*	env,
	jobject		obj,
	jint		widgetPtr,
	jchar		keyChar,
	jint		keyCode,
	jint		modifiers,
	jint		eventTime)
{
  (* nativeSendKeyDownEvent) (env, obj, widgetPtr, keyChar, keyCode, modifiers, eventTime);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeSendKeyDownEvent()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorSendKeyUpEvent
 * Signature: (ICIII)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeSendKeyUpEvent (
	JNIEnv	*	env,
	jobject		obj,
	jint		widgetPtr,
	jchar		keyChar,
	jint		keyCode,
	jint		modifiers,
	jint		eventTime)
{
  (* nativeSendKeyUpEvent) (env, obj, widgetPtr, keyChar, keyCode, modifiers, eventTime);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeSendKeyUpEvent()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorSendMouseEvent
 * Signature: (IIIIIIIIIII)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeSendMouseEvent (
	JNIEnv	*		env,
	jobject			obj,
	jint			windowPtr,
	jint			widgetPtr,
	jint			widgetX,
	jint			widgetY,
	jint			windowX,
	jint			windowY,
	jint			mouseMessage,
	jint			numClicks,
	jint			modifiers,
	jint			eventTime)
{
  (* nativeSendMouseEvent) (env, obj, windowPtr, widgetPtr, widgetX, widgetY, windowX, windowY, mouseMessage, numClicks, modifiers, eventTime);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeSendMouseEvent()



/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorIdleEvent
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeIdleEvent (
	JNIEnv	*	env,
	jobject		obj,
	jint		windowPtr,
	jint		eventTime)
{
  (* nativeIdleEvent) (env, obj, windowPtr, eventTime);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeIdleEvent()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorUpdateEvent
 * Signature: (II)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeUpdateEvent (
	JNIEnv	*	env,
	jobject		obj,
	jint		windowPtr,
	jint		eventTime)
{
  (* nativeUpdateEvent) (env, obj, windowPtr, eventTime);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeUpdateEvent()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWidgetCreate
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetCreate (
	JNIEnv	*	env,
	jobject		obj,
	jint		windowPtr,
	jint		x,
	jint		y,
	jint		width,
	jint		height)
{
  return (* nativeWidgetCreate) (env, obj, windowPtr, x, y, width, height);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetCreate()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWidgetDelete
 * Signature: (I)I
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetDelete (
	JNIEnv	*	env,
	jobject		obj,
	jint		widgetPtr)
{
  (* nativeWidgetDelete) (env, obj, widgetPtr);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetDelete()



/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWidgetResize
 * Signature: (IIIZ)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetResize (
	JNIEnv *	env,
	jobject		obj,
	jint		widgetPtr,
	jint		x,
	jint		y,
	jint		width,
	jint		height,
	jboolean	repaint)
{
  (* nativeWidgetResize) (env, obj, widgetPtr, x, y, width, height, repaint);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetResize()



/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWidgetEnable
 * Signature: (IZ)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetEnable (
	JNIEnv	*	env,
	jobject		obj,
	jint		widgetPtr,
	jboolean	enable)
{
  (* nativeWidgetEnable) (env, obj, widgetPtr, enable);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetEnable()



/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWidgetShow
 * Signature: (IZ)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetShow (
	JNIEnv	*	env,
	jobject		obj,
	jint		widgetPtr,
	jboolean	show)
{
  (* nativeWidgetShow) (env, obj, widgetPtr, show);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetShow()



/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWidgetInvalidate
 * Signature: (IZ)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetInvalidate (
	JNIEnv	*	env,
	jobject		obj,
	jint		widgetPtr,
	jboolean	isSynchronous)
{
  (* nativeWidgetInvalidate) (env, obj, widgetPtr, isSynchronous);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetInvalidate()



/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWidgetUpdate
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetUpdate (
	JNIEnv	*	env,
	jobject		obj,
	jint		widgetPtr)
{
  (* nativeWidgetUpdate) (env, obj, widgetPtr);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWidgetUpdate()



JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeProcessEvents (
	JNIEnv*	env,
	jobject obj,
	jint	theWebShell)
{
  (* nativeProcessEvents) (env, obj, theWebShell);
}


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellCreate
 * Signature: (IIIIII)I
 */
JNIEXPORT jint JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellCreate (
	JNIEnv		*	env,
	jobject			obj,
	jint			windowPtr,
	jint			x,
	jint			y,
	jint			width,
	jint			height)
{
  return (* nativeWebShellCreate) (env, obj, windowPtr, x, y, width, height);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellCreate()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellDelete
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellDelete (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  (* nativeWebShellDelete) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellDelete()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellLoadURL
 * Signature: (ILjava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellLoadURL (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jstring		urlString)
{
  (* nativeWebShellLoadURL) (env, obj, webShellPtr, urlString);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellLoadURL()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellStop
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellStop (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  (* nativeWebShellStop) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellStop()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellShow
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellShow (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  (* nativeWebShellShow) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellShow()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellHide
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellHide (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  (* nativeWebShellHide) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellHide()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellSetBounds
 * Signature: (IIIII)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellSetBounds (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jint		x,
	jint		y,
	jint		width,
	jint		height)
{
  (* nativeWebShellSetBounds) (env, obj, webShellPtr, x, y, width, height);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellSetBounds()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellMoveTo
 * Signature: (III)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellMoveTo (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jint		x,
	jint		y)
{
  (* nativeWebShellMoveTo) (env, obj, webShellPtr, x, y);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellMoveTo()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellSetFocus
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellSetFocus (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  (* nativeWebShellSetFocus) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellSetFocus()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellRemoveFocus
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellRemoveFocus (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  (* nativeWebShellRemoveFocus) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellRemoveFocus()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellRepaint
 * Signature: (IZ)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellRepaint (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jboolean	forceRepaint)
{
  (* nativeWebShellRepaint) (env, obj, webShellPtr, forceRepaint);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellRepaint()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellCanBack
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellCanBack (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  return (* nativeWebShellCanBack) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellCanBack()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellCanForward
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellCanForward (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  return (* nativeWebShellCanForward) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellCanForward()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellBack
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellBack (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  return (* nativeWebShellBack) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellBack()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellForward
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellForward (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  return (* nativeWebShellForward) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellForward()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellGoTo
 * Signature: (II)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellGoTo (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jint		historyIndex)
{
  return (* nativeWebShellGoTo) (env, obj, webShellPtr, historyIndex);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellGoTo()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellGetHistoryLength
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellGetHistoryLength (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  return (* nativeWebShellGetHistoryLength) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellGetHistoryLength()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellGetHistoryIndex
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellGetHistoryIndex (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
  return (* nativeWebShellGetHistoryIndex) (env, obj, webShellPtr);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellGetHistoryIndex()


/*
 * Class:     MozWebShellMozillaShim
 * Method:    raptorWebShellGetURL
 * Signature: (II)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellGetURL (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jint		historyIndex)
{
  return (* nativeWebShellGetURL) (env, obj, webShellPtr, historyIndex);
} // Java_org_mozilla_webclient_BrowserControlMozillaShim_nativeWebShellGetURL()



// EOF
