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
/* FIXME remove when pure JRI if possible */
#include "oobj.h"
#include "interpreter.h"
#include "tree.h"

#include "jsapi.h"
#include "jsjava.h"
#include "jscntxt.h"    /* for cx->savedErrors */

#include "jri.h"

#include "prmon.h"	/* for PR_XLock and PR_XUnlock */
#include "prprf.h"	/* for PR_snprintf */
#include "prthread.h"
#include "prlog.h"

#define IMPLEMENT_netscape_javascript_JSObject
#include "netscape_javascript_JSObject.h"
#ifndef XP_MAC
#include "netscape_javascript_JSException.h"
#else
#include "n_javascript_JSException.h"
#endif


#ifdef NSPR20
extern PRLogModuleInfo* Moja;
#define debug	PR_LOG_DEBUG
#endif /* NSPR20 */

#define JAVA_JS_STACK_SIZE 8192

typedef struct {
    JSErrorReporter errReporter;
} JSSavedState;

static void
js_throwJSException(JRIEnv *env, char *detail)
{
#ifdef JAVA
    struct java_lang_String* message =
	JRI_NewStringPlatform(env,
			      (const jbyte *) detail,
			      (jint) strlen(detail),
			      (const jbyte *) NULL,
			      (jint) 0);
    struct netscape_javascript_JSException* exc =
      netscape_javascript_JSException_new_1(env,
                         class_netscape_javascript_JSException(env),
                         /*Java_getGlobalRef(env, js_JSExceptionClass),*/
                         message);

    JRI_Throw(env, (struct java_lang_Throwable*)exc);
#endif /* JAVA */
}

/***********************************************************************/

static bool_t
enterJS(JRIEnv *env,
        struct netscape_javascript_JSObject* self,
        JSContext **cxp, JSObject **jsop, JSSavedState *saved)
{
#ifndef JAVA
    return JS_FALSE;
#else
    char *err;

    JSJ_EnterJS();

    /* check the internal pointer for null while holding the
     * js lock to deal with shutdown issues */
    if (jsop &&
        (! self ||
         ! (*jsop = (JSObject *)
            get_netscape_javascript_JSObject_internal(env, self)))) {

        /* FIXME use a better exception */
        JRI_ThrowNew(env, JRI_FindClass(env, "java/lang/Exception"),
                     "not a JavaScript object");
        JSJ_ExitJS();
        return FALSE;
    }

    /* PR_LOG(Moja, debug, ("clearing exceptions in ee=0x%x", env)); */

    exceptionClear((ExecEnv *)env);

    if (! (*cxp = JSJ_CurrentContext(env, &err))) {
        JSJ_ExitJS();
        if (err) {
            js_throwJSException(env, err);
            free(err);
        }
        return FALSE;
    }

    jsj_ClearSavedErrors(*cxp);

    saved->errReporter = JS_SetErrorReporter(*cxp, js_JavaErrorReporter);
    return TRUE;
#endif
}

static bool_t
exitJS(JRIEnv *env,
       struct netscape_javascript_JSObject* self,
       JSContext *cx, JSObject *jso, JSSavedState *saved)
{
#ifndef JAVA
	return JS_FALSE;
#else
    /* restore saved info */
    JS_SetErrorReporter(cx, saved->errReporter);

    js_JSErrorToJException(cx, (ExecEnv*)env);

    JSJ_ExitJS();

    if (exceptionOccurred((ExecEnv*)env))
        return FALSE;

    return TRUE;
#endif
}

/***********************************************************************/

JRI_PUBLIC_API(struct java_lang_Object *)
native_netscape_javascript_JSObject_getMember(
    JRIEnv* env,
    struct netscape_javascript_JSObject* self,
    struct java_lang_String *name)
{
#ifndef JAVA
	return NULL;
#else
    JSContext *cx;
    JSObject *jso;
    JSSavedState saved;
    const char *cstr;
    struct java_lang_Object *member;
    jsval val;
    int cost = 0;

    if (!enterJS(env, self, &cx, &jso, &saved))
        return NULL;

    if (! name ||
        ! (cstr = JRI_GetStringPlatformChars(env, name,
                                             NULL, 0))) {

    /* XXX - temporarily replace arguments so we can compile
       (const jbyte *) cx->charSetName,
       (jint) cx->charSetNameLength))) {
    */

        /* FIXME this should be an error of some sort */
        js_throwJSException(env, "illegal member name");
        member = NULL;
	goto do_exit;
    }

    if (! JS_GetProperty(cx, jso, cstr, &val) ||
        ! js_convertJSValueToJObject(/*FIXME*/(HObject**)&member, cx, val,
                                    0, 0, JS_FALSE, &cost))
        member = NULL;

  do_exit:
    if (!exitJS(env, self, cx, jso, &saved))
        return NULL;

    return member;
#endif
}

/***********************************************************************/

/* public native getSlot(I)Ljava/lang/Object; */
JRI_PUBLIC_API(struct java_lang_Object *)
native_netscape_javascript_JSObject_getSlot(
    JRIEnv* env,
    struct netscape_javascript_JSObject* self,
    jint slot)
{
#ifndef JAVA
	return NULL;
#else
    JSContext *cx;
    JSObject *jso;
    JSSavedState saved;
    struct java_lang_Object *member;
    jsval val;
    int cost = 0;

    if (!enterJS(env, self, &cx, &jso, &saved))
        return NULL;

    if (! JS_GetElement(cx, jso, slot, &val) ||
        ! js_convertJSValueToJObject(/*FIXME*/(HObject**)&member, cx, val,
                                     0, 0, JS_FALSE, &cost))
        member = NULL;

    if (!exitJS(env, self, cx, jso, &saved))
        return NULL;

    return member;
#endif
}

/***********************************************************************/

JRI_PUBLIC_API(void)
native_netscape_javascript_JSObject_setMember(
    JRIEnv* env,
    struct netscape_javascript_JSObject* self,
    struct java_lang_String *name,
    struct java_lang_Object *value)
{
#ifdef JAVA
    JSContext *cx;
    JSObject *jso;
    JSSavedState saved;
    const char *cstr;
    jsval val;

    if (!enterJS(env, self, &cx, &jso, &saved))
        return;

    if (! name ||
        ! (cstr = JRI_GetStringPlatformChars(env, name,
                                             NULL, 0))) {

    /* XXX - temporarily replace arguments so we can compile
       (const jbyte *) cx->charSetName,
       (jint) cx->charSetNameLength))) {
    */

        js_throwJSException(env, "illegal member name");
        goto do_exit;
    }

    if (js_convertJObjectToJSValue(cx, &val, (HObject *)value) &&
        JS_SetProperty(cx, jso, cstr, &val)) {
    }

    /* PR_LOG(Moja, debug,
	   ("JSObject.setMember(%s, 0x%x)\n", cstr, value)); */

  do_exit:
    exitJS(env, self, cx, jso, &saved);
    return;
#endif
}

/***********************************************************************/

JRI_PUBLIC_API(void)
native_netscape_javascript_JSObject_setSlot(
    JRIEnv* env,
    struct netscape_javascript_JSObject* self,
    jint slot,
    struct java_lang_Object *value)
{
#ifdef JAVA
    JSContext *cx;
    JSObject *jso;
    JSSavedState saved;
    jsval val;

    if (!enterJS(env, self, &cx, &jso, &saved))
        return;

    if (js_convertJObjectToJSValue(cx, &val, (HObject *)value) &&
        JS_SetElement(cx, jso, slot, &val)) {
    }

    /* PR_LOG(Moja, debug,
           ("JSObject.setSlot(%d, 0x%x)\n", slot, value)); */

    exitJS(env, self, cx, jso, &saved);
    return;
#endif
}

/***********************************************************************/

JRI_PUBLIC_API(void)
native_netscape_javascript_JSObject_removeMember(
    JRIEnv* env,
    struct netscape_javascript_JSObject* self,
    struct java_lang_String * name)
{
    JSContext *cx;
    JSObject *jso;
    JSSavedState saved;
#ifdef JAVA /* only referenced if defined */
    const char *cstr;
#endif

    if (!enterJS(env, self, &cx, &jso, &saved))
        return;
#ifdef JAVA
    if (! name ||
        ! (cstr = JRI_GetStringPlatformChars(env, name,
                                             NULL, 0))) {

    /* XXX - temporarily replace arguments so we can compile
       (const jbyte *) cx->charSetName,
       (jint) cx->charSetNameLength);
    */
        /* FIXME this should be an error of some sort */
        js_throwJSException(env, "illegal member name");
        goto do_exit;
    }

    JS_DeleteProperty(cx, jso, cstr);
#endif

    /* PR_LOG(Moja, debug,
           ("JSObject.removeMember(%s)\n", cstr)); */
#ifdef JAVA /* only referenced if defined */
  do_exit:
#endif
    exitJS(env, self, cx, jso, &saved);
    return;
}

/***********************************************************************/

JRI_PUBLIC_API(struct java_lang_Object *)
native_netscape_javascript_JSObject_call(
    JRIEnv* env,
    struct netscape_javascript_JSObject* self,
    struct java_lang_String *method,
    jobjectArray args)
{
#ifndef JAVA
	return NULL;
#else
    JSContext *cx;
    JSObject *jso;
    JSSavedState saved;
    const char *cstr;
    struct java_lang_Object *ret;
    int total_argc, argc, i;
    jsval *argv;
    jsval rval;
    int cost = 0;

    if (!enterJS(env, self, &cx, &jso, &saved))
        return NULL;

    if (! method ||
        ! (cstr = JRI_GetStringPlatformChars(env, method,
                                             NULL, 0))) {

    /* XXX - temporarily replace arguments so we can compile
       (const jbyte *) cx->charSetName,
       (jint) cx->charSetNameLength))) {
    */
        /* FIXME this should be an error of some sort */
        js_throwJSException(env, "illegal member name");
        ret = NULL;
	goto do_exit;
    }

    if (args) {
        total_argc = JRI_GetObjectArrayLength(env, args);
        argv = sysMalloc(total_argc * sizeof(jsval));
    } else {
        total_argc = 0;
        argv = 0;
    }

    for (argc = 0; argc < total_argc; argc++) {
        jref arg = JRI_GetObjectArrayElement(env, args, argc);

        if (!js_convertJObjectToJSValue(cx, argv+argc, (HObject*)arg))
            goto cleanup_argv;
        JS_AddRoot(cx, argv+argc);
    }

    if (! JS_CallFunctionName(cx, jso, cstr, argc, argv, &rval) ||
        ! js_convertJSValueToJObject((HObject **) &ret, cx, rval,
                                     0, 0, JS_FALSE, &cost)) {
        ret = NULL;
    }

  cleanup_argv:
    for (i = 0; i < argc; i++)
        JS_RemoveRoot(cx, argv+i);
    sysFree(argv);

  do_exit:
    if (!exitJS(env, self, cx, jso, &saved))
        return NULL;

    return ret;
#endif
}

/***********************************************************************/

JRI_PUBLIC_API(struct java_lang_Object *)
native_netscape_javascript_JSObject_eval(
    JRIEnv* env,
    struct netscape_javascript_JSObject* self,
    struct java_lang_String *s)
{
#ifndef JAVA
    return NULL;
#else
    JSContext *cx;
    JSObject *jso;
    JSSavedState saved;
    const char *cstr;
    int cstrlen;
    struct java_lang_Object *ret;
    jsval rval;
    int cost = 0;
    JSPrincipals *principals;
    char *codebase;

    if (!enterJS(env, self, &cx, &jso, &saved))
        return NULL;

    if (! s ||
        ! (cstr = JRI_GetStringPlatformChars(env, s,
                                             NULL, 0))) {

    /* XXX - temporarily replace arguments so we can compile
       (const jbyte *) cx->charSetName,
       (jint) cx->charSetNameLength))) {
    */
        /* FIXME this should be an error of some sort */
        js_throwJSException(env, "illegal eval string");
        ret = NULL;
	goto do_exit;
    }

    /* subtract 1 for the NUL */
    /*
     * FIXME There is no JRI_GetStringPlatformLength (what a misnomer, if there
     * were!), but JRI_GetStringPlatformChars does NUL-terminate the encoded
     * string, so we trust that no charset encoding ever uses a NUL byte as
     * part of a multibyte sequence and just call strlen.
     */
    cstrlen = strlen(cstr);

    principals = js_GetJSPrincipalsFromJavaCaller(cx, 0);
    codebase = principals ? principals->codebase : NULL;
    if (! JS_EvaluateScriptForPrincipals(cx, jso, principals, cstr, cstrlen,
                                         codebase, 0, &rval) ||
        ! js_convertJSValueToJObject((HObject**)&ret, cx, rval,
                                     0, 0, JS_FALSE, &cost)) {
        ret = NULL;
    }

  do_exit:
    if (!exitJS(env, self, cx, jso, &saved))
        return NULL;

    return ret;
#endif
}

/***********************************************************************/

JRI_PUBLIC_API(void)
native_netscape_javascript_JSObject_finalize(
    JRIEnv* env,
    struct netscape_javascript_JSObject* self)
{
#ifdef JAVA
    JSContext *cx;
    JSObject *jso;
    JSSavedState saved;

    /* FIXME if enterJS fails here we have a memory leak! */

    if (!enterJS(env, self, &cx, &jso, &saved))
        return;

    js_RemoveReflection(cx, jso);

    if (!exitJS(env, self, cx, jso, &saved))
        return;

    return;
#endif
}

/***********************************************************************/

JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_javascript_JSObject_toString(
    JRIEnv* env,
    struct netscape_javascript_JSObject* self)
{
    JSContext *cx;
    JSObject *jso;
    JSSavedState saved;
#ifdef JAVA /* only referenced if defined */
    JSString *jsstr;
    char *cstr;
#endif
    struct java_lang_String *ret = NULL;

    if (!enterJS(env, self, &cx, &jso, &saved))
        return NULL;

#ifdef JAVA
    if ((jsstr = JS_ValueToString(cx, OBJECT_TO_JSVAL(jso))) &&
        (cstr = JS_GetStringBytes(jsstr))) {
        ret = JRI_NewStringPlatform(env,
				    (const jbyte *) cstr,
				    (jint) JS_GetStringLength(jsstr),
                                    NULL, 0);

    /* XXX - temporarily replace arguments so we can compile
       (const jbyte *) cx->charSetName,
       (jint) cx->charSetNameLength);
    */

    } else {
        /* FIXME could grab the clazz name from the JSObject */
        ret = JRI_NewStringPlatform(env,
				    (const jbyte *) "*JSObject*",
				    (jint) strlen("*JSObject*"),
                                    NULL, 0);

    /* XXX - temporarily replace arguments so we can compile
       (const jbyte *) cx->charSetName,
       (jint) cx->charSetNameLength);
    */
    }
#endif

    if (!exitJS(env, self, cx, jso, &saved))
        return NULL;

    return ret;
}

/***********************************************************************/

JRI_PUBLIC_API(struct netscape_javascript_JSObject*)
native_netscape_javascript_JSObject_getWindow(
    JRIEnv* env,
    struct java_lang_Class* clazz,
    struct java_applet_Applet* hint)
{
#ifndef JAVA
    return NULL;
#else
    struct netscape_javascript_JSObject *ret = NULL;
    JSContext *cx;
    JSSavedState saved;
    JSObject *jso;

    if (!JSJ_IsEnabled()) {
        js_throwJSException(env, "Java/JavaScript communication is disabled");
        return 0;
    }

    if (!enterJS(env, NULL, &cx, NULL, &saved))
        return NULL;

    jso = JSJ_GetDefaultObject(env, (jobject) hint);
    if (exceptionOccurred((ExecEnv *)env))
        goto out;

    ret = (struct netscape_javascript_JSObject *)
        js_ReflectJSObjectToJObject(cx, jso);

  out:
    if (!exitJS(env, NULL, cx, NULL, &saved))
        return NULL;
    return ret;
#endif
}

/***********************************************************************/

JRI_PUBLIC_API(void)
native_netscape_javascript_JSObject_initClass(
    JRIEnv* env,
    struct java_lang_Class* myClazz)
{
#ifdef JAVA
    struct java_lang_Class* clazz;
    /* this is to initialize the field lookup indices, and protect
     * the classes from gc.  */

    clazz = use_netscape_javascript_JSObject(env);
    MakeClassSticky((struct Hjava_lang_Class*)clazz);

    clazz = use_netscape_javascript_JSException(env);
    MakeClassSticky((struct Hjava_lang_Class*)clazz);

    /* this does nothing if somebody else has already set
     * stuff up (i.e. client or server) */
    JSJ_InitStandalone();
#endif
}

