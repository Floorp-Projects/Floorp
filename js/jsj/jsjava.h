/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* ** */
#ifndef _jsjava_h_
#define _jsjava_h_

#include "jsapi.h"

/*
 * Java <-> JavaScript reflection
 */



/* FIXME change everything to JRI style */
#ifndef _OOBJ_H_
typedef struct HObject HObject;
typedef struct ClassClass ClassClass;
typedef struct ExecEnv ExecEnv;
#endif

/* FIXME should be obtained from jri.h */
#ifndef JRI_H
typedef struct JRIEnvInterface  JRIEnvInterface;
typedef const JRIEnvInterface*  JRIEnv;
#endif
#ifndef JNI_H
typedef struct _jobject *jobject;
#endif

/***********************************************************************
 *
 *  this callback table is used to export functions to jsjava.c
 */
typedef struct {
    int         (*isEnabled)(void);
    JRIEnv *	(*currentEnv)(JSContext *);
    JSContext *	(*currentContext)(JRIEnv *, char **errp);
    void	(*enterJS)(void);
    void	(*exitJS)(void);
    void *	(*jsClassLoader)(JSContext *, const char *codebase);
    JSPrincipals *
                (*getJSPrincipalsFromJavaCaller)(JSContext *cx,
                                                 int callerDepth);
    JSObject *	(*getDefaultObject)(JRIEnv *env, jobject hint);
} JSJCallbacks;

/***********************************************************************
 *
 *  these functions are provided by jsjava.c
 */

/* call to initialize a javascript context and toplevel object for use
 * with JSJ */
PR_EXTERN(JSBool)
JSJ_Init(JSJCallbacks *callbacks);

#if defined(JAVA) || defined(OJI)
/* this is called by JSObject.initClass to initialize the default
 * glue if jsjava has been loaded into java.exe as a DLL.  it does
 * nothing if the client or server has already initialized us */
PR_EXTERN(void)
JSJ_InitStandalone();

/* shut down java-javascript reflection */
PR_EXTERN(void)
JSJ_Finish(void);

/* call to initialize a javascript context and toplevel object for use
 * with JSJ */
PR_EXTERN(JSBool)
JSJ_InitContext(JSContext *cx, JSObject *obj);

/* tells whether the js<->java connection can be used */
PR_EXTERN(int)
JSJ_IsEnabled(void);

/* Java threads call this to find the JSContext to use for
 * executing JavaScript */
PR_EXTERN(JSContext *)
JSJ_CurrentContext(JRIEnv *env, char **errp);

/* Java threads call this before executing JS code (to preserve
 * run-to-completion in the client */
PR_EXTERN(void)
JSJ_EnterJS(void);

/* Java threads call this when finished executing JS code */
PR_EXTERN(void)
JSJ_ExitJS(void);

/* gets a default netscape.javascript.JSObject from a java object */
PR_EXTERN(JSObject *)
JSJ_GetDefaultObject(JRIEnv *env, jobject hint);

/*
 * When crawling the Java stack, this indicates that the next frames
 * are in JavaScript
 */
struct methodblock;
PR_EXTERN(JSBool)
JSJ_IsSafeMethod(struct methodblock *mb);

PR_EXTERN(JSBool)
JSJ_IsCalledFromJava(JSContext *cx);

/* find the current JSContext, if possible.  if cxp is null,
 * the caller may attempt to infer the JSContext from the
 * classloader */
PR_EXTERN(void)
JSJ_FindCurrentJSContext(JRIEnv *env, JSContext **cxp, void **loaderp);

/* extract the javascript object from a netscape.javascript.JSObject */
PR_EXTERN(JSObject *)
JSJ_ExtractInternalJSObject(JRIEnv *env, HObject* jso);

/* provided for use by java calling javascript: */

PR_EXTERN(JSObject *)
js_ReflectJObjectToJSObject(JSContext *cx, HObject *jo);

extern JSObject *
js_ReflectJClassToJSObject(JSContext *cx, ClassClass *clazz);

PR_EXTERN(HObject *)
js_ReflectJSObjectToJObject(JSContext *cx, JSObject *mo);

/* this JS error reporter saves the error for JSErrorToJException */
PR_EXTERN(void)
js_JavaErrorReporter(JSContext *cx, const char *message, JSErrorReport *error);

/* if there was an error in JS, throw the corresponding JSException */
PR_EXTERN(void)
js_JSErrorToJException(JSContext *cx, ExecEnv *ee);

PR_EXTERN(void)
js_RemoveReflection(JSContext *cx, JSObject *mo);

PR_EXTERN(JSBool)
js_convertJObjectToJSValue(JSContext *cx, jsval *vp, HObject *ho);

PR_EXTERN(JSBool)
js_convertJSValueToJObject(HObject **objp, JSContext *cx,
			   jsval v, char *sig, ClassClass *fromclass,
			   JSBool checkOnly, int *cost);

PR_EXTERN(JSPrincipals *)
js_GetJSPrincipalsFromJavaCaller(JSContext *cx, int callerDepth);

PR_EXTERN(void)
jsj_ClearSavedErrors(JSContext *cx);

#endif /* defined(JAVA) || defined(OJI) */

#endif /* _jsjava_h_ */
