/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
 
/* This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * It contains the implementation of the native methods for the 
 * netscape.javascript.JSObject Java class, which are used for calling into
 * JavaScript from Java.  It also contains the code that handles propagation
 * of exceptions both into and out of Java.
 *
 */

#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "prprintf.h"
#include "prassert.h"

#include "jsj_private.h"
#include "jsjava.h"

#include "jscntxt.h"        /* For js_ReportErrorAgain().
                               TODO - get rid of private header */

#include "netscape_javascript_JSObject.h"   /* javah-generated headers */


/* A captured JavaScript error, created when JS_ReportError() is called while
   running JavaScript code that is itself called from Java. */
struct CapturedJSError {
    char *              message;
    JSErrorReport       report;         /* Line # of error, etc. */
    jthrowable          java_exception; /* Java exception, error, or null */
    CapturedJSError *   next;                   /* Next oldest captured JS error */
};


/*********************** Reflection of JSObjects ****************************/

/*
 * This is a hash table that maps from JS objects to Java objects.
 * It is used to ensure that the same Java object results when a JS
 * object is reflected more than once, so that Java object equality
 * tests work in the expected manner.
 *
 * The entry keys are JSObject pointers and the entry values are Java objects
 * (of type jobject).  When the corresponding Java instance is finalized,
 * the entry is removed from the table, and a JavaScript GC root for the JS
 * object is removed.
 */
static PRHashTable *js_obj_reflections = NULL;

#ifdef JS_THREADSAFE
/*
 * Used to protect the js_obj_reflections hashtable from simultaneous
 * read/write or * write/write access.
 */
static PRMonitor *js_obj_reflections_monitor = NULL;
#endif

static JSBool
init_js_obj_reflections_table()
{
    js_obj_reflections = PR_NewHashTable(128, NULL, PR_CompareValues,
                                         PR_CompareValues, NULL, NULL);
    if (!js_obj_reflections)
        return JS_FALSE;

#ifdef JS_THREADSAFE
    js_obj_reflections_monitor = PR_NewNamedMonitor("js_obj_reflections");
    if (!js_obj_reflections_monitor) {
        PR_HashTableDestroy(js_obj_reflections);
        return JS_FALSE;
    }
#endif

    return JS_TRUE;
}

/*
 * Return a Java object that "wraps" a given JS object by storing the address
 * of the JS object in a private field of the Java object.  A hash table is
 * used to ensure that the mapping of JS objects to Java objects is done on
 * one-to-one basis.
 *
 * If an error occurs, returns NULL and reports an error.
 */
jobject
jsj_WrapJSObject(JSContext *cx, JNIEnv *jEnv, JSObject *js_obj)
{
    jobject java_wrapper_obj;
    PRHashEntry *he, **hep;

    java_wrapper_obj = NULL;

#ifdef JS_THREADSAFE
    PR_EnterMonitor(js_obj_reflections_monitor);
#endif

    /* First, look in the hash table for an existing reflection of the same
       JavaScript object.  If one is found, return it. */
    hep = PR_HashTableRawLookup(js_obj_reflections, (prhashcode)js_obj, js_obj);
    he = *hep;
    if (he) {
        java_wrapper_obj = (jobject)he->value;
        PR_ASSERT(java_wrapper_obj);
        if (java_wrapper_obj)
            goto done;
    }

    /* No existing reflection found, so create a new Java object that wraps
       the JavaScript object by storing its address in a private integer field. */
    java_wrapper_obj =
        (*jEnv)->NewObject(jEnv, njJSObject, njJSObject_JSObject, (jint)js_obj);
    if (!java_wrapper_obj) {
        jsj_UnexpectedJavaError(cx, jEnv, "Couldn't create new instance of "
                                          "netscape.javascript.JSObject");
        goto done;
    }

    /* Add the new reflection to the hash table. */
    he = PR_HashTableRawAdd(js_obj_reflections, hep, (prhashcode)js_obj,
                            js_obj, java_wrapper_obj);
    if (he) {
        /* Tell the JavaScript GC about this object since the only reference
           to it may be in Java-land. */
        JS_AddRoot(cx, (void*)&he->key);
    } else {
        JS_ReportOutOfMemory(cx);
        /* No need to delete java_wrapper_obj because Java GC will reclaim it */
        java_wrapper_obj = NULL;
    }
    
    /*
     * Release local reference to wrapper object, since some JVMs seem reticent
     * about collecting it otherwise.
     */
    (*jEnv)->DeleteLocalRef(jEnv, java_wrapper_obj);

done:
#ifdef JS_THREADSAFE
        PR_ExitMonitor(js_obj_reflections_monitor);
#endif
        
    return java_wrapper_obj;
}

/*
 * Remove the mapping of a JS object from the hash table that maps JS objects
 * to Java objects.  This is called from the finalizer of an instance of
 * netscape.javascript.JSObject.
 */
static JSBool
remove_js_obj_reflection_from_hashtable(JSContext *cx, JSObject *js_obj)
{
    PRHashEntry *he, **hep;
    JSBool success = JS_FALSE;

#ifdef JS_THREADSAFE
    PR_EnterMonitor(js_obj_reflections_monitor);
#endif

    /* Get the hash-table entry for this wrapper object */
    hep = PR_HashTableRawLookup(js_obj_reflections, (prhashcode)js_obj, js_obj);
    he = *hep;

    PR_ASSERT(he);
    if (he) {
        /* Tell the JS GC that Java no longer keeps a reference to this
           JS object. */
        success = JS_RemoveRoot(cx, (void *)&he->key);

        PR_HashTableRawRemove(js_obj_reflections, hep, he);
    }

#ifdef JS_THREADSAFE
    PR_ExitMonitor(js_obj_reflections_monitor);
#endif

    return success;
}

/*************** Handling of Java exceptions in JavaScript ******************/

/*
 * Free up a JavaScript error that has been stored by the
 * capture_js_error_reports_for_java() function.
 */
static CapturedJSError *
destroy_saved_js_error(JNIEnv *jEnv, CapturedJSError *error)
{
    CapturedJSError *next_error;
    if (!error)
        return NULL;
    next_error = error->next;

    if (error->java_exception)
        (*jEnv)->DeleteGlobalRef(jEnv, error->java_exception);
    if (error->message)
        free((char*)error->message);
    if (error->report.filename)
        free((char*)error->report.filename);
    if (error->report.linebuf)
        free((char*)error->report.linebuf);
    free(error);

    return next_error;
}

/*
 * Capture a JS error that has been reported using JS_ReportError() by JS code
 * that has itself been called from Java.  A copy of the JS error data is made
 * and hung off the JSJ environment.  When JS completes and returns to its Java
 * caller, this data is used to synthesize an instance of
 * netscape.javascript.JSException.  If the resulting JSException is not caught
 * within Java, it may be propagated up the stack beyond the Java caller back
 * into JavaScript, in which case the error will be re-reported as a JavaScript
 * error.
 */
PR_STATIC_CALLBACK(void)
capture_js_error_reports_for_java(JSContext *cx, const char *message,
                                  JSErrorReport *report)
{
    CapturedJSError *new_error;
    JSJavaThreadState *jsj_env;
    jthrowable java_exception;
    JNIEnv *jEnv;

    /* Create an empty struct to hold the saved JS error state */
    new_error = malloc(sizeof(CapturedJSError));
    if (!new_error)
        goto out_of_memory;
    memset(new_error, 0, sizeof(CapturedJSError));

    /* Copy all the error info out of the original report into a private copy */
    if (message) {
        new_error->message = strdup(message);
        if (!new_error->message)
            goto out_of_memory;
    }
    if (report) {
        new_error->report.lineno = report->lineno;

        if (report->filename) {
            new_error->report.filename = strdup(report->filename);
            if (!new_error->report.filename)
                goto out_of_memory;
        }

        if (report->linebuf) {
            new_error->report.linebuf = strdup(report->linebuf);
            if (!new_error->report.linebuf)
                goto out_of_memory;
            new_error->report.tokenptr =
                new_error->report.linebuf + (report->tokenptr - report->linebuf);
        }
    }

    /* Get the head of the list of pending JS errors */
    jsj_env = jsj_MapJSContextToJSJThread(cx, &jEnv);
    if (!jsj_env)
        goto abort;

    /* If there's a Java exception associated with this error, save it too. */
    java_exception = (*jEnv)->ExceptionOccurred(jEnv);
    if (java_exception) {
        (*jEnv)->ExceptionClear(jEnv);
        java_exception = (*jEnv)->NewGlobalRef(jEnv, java_exception);
        new_error->java_exception = java_exception;
    }

    /* Push this error onto the list of pending JS errors */
    new_error->next = jsj_env->pending_js_errors;
    jsj_env->pending_js_errors = new_error;
    return;

abort:
out_of_memory:
    /* No recovery action possible */
    PR_ASSERT(0);
    destroy_saved_js_error(jEnv, new_error);
    return;
}

void
jsj_ClearPendingJSErrors(JSJavaThreadState *jsj_env)
{
    while (jsj_env->pending_js_errors)
        jsj_env->pending_js_errors = destroy_saved_js_error(jsj_env->jEnv, jsj_env->pending_js_errors);
}

/*
 * This is called upon returning from JS back into Java.  Any JS errors
 * reported during that time will be converted into Java exceptions.  It's
 * possible that a JS error was actually triggered by Java at some point, in
 * which case the original Java exception is thrown.
 */
static void
throw_any_pending_js_error_as_a_java_exception(JSJavaThreadState *jsj_env)
{
    CapturedJSError *error;
    JNIEnv *jEnv;
    jstring message_jstr, linebuf_jstr, filename_jstr;
    jint index, lineno;
    JSErrorReport *report;
    jthrowable java_exception;

    /* Get the Java JNI environment */
    jEnv = jsj_env->jEnv;

    if (!jsj_env->pending_js_errors) {
#ifdef DEBUG
        /* Any exception should be cleared as soon as it's detected, so there
           shouldn't be any pending. */
        if ((*jEnv)->ExceptionOccurred(jEnv)) {
            /* A Java exception occurred, but nobody in JS-land noticed. */
            PR_ASSERT(0);
            (*jEnv)->ExceptionClear(jEnv);
        }
#endif
        return;
    }

    /* Get the deepest (oldest) saved JS error */
    /* XXX - What's the right thing to do about newer errors ?
       For now we just throw them away */
    error = jsj_env->pending_js_errors;
    while (error->next)
        error = error->next;
    
    /*
     * If the JS error was originally the result of a Java exception, rethrow
     * the original exception.
     */
    if (error->java_exception) {
        (*jEnv)->Throw(jEnv, error->java_exception);
        goto done;
    }

    /* Propagate any JS errors that did not originate as Java exceptions into
       Java as an instance of netscape.javascript.JSException */

    /* First, marshall the arguments to the JSException constructor */
    message_jstr = NULL;
    if (error->message) {
        message_jstr = (*jEnv)->NewStringUTF(jEnv, error->message);
        if (!message_jstr)
            goto out_of_memory;
    }

    report = &error->report;

    filename_jstr = NULL;
    if (report->filename) {
        filename_jstr = (*jEnv)->NewStringUTF(jEnv, report->filename);
        if (!filename_jstr)
            goto out_of_memory;
    }

    linebuf_jstr = NULL;
    if (report->linebuf) {
        linebuf_jstr = (*jEnv)->NewStringUTF(jEnv, report->linebuf);
        if (!linebuf_jstr)
            goto out_of_memory;
    }

    lineno = report->lineno;
    index = report->linebuf ? report->tokenptr - report->linebuf : 0;

    /* Call the JSException constructor */
    java_exception = (*jEnv)->NewObject(jEnv, njJSException, njJSException_JSException,
                                        message_jstr, filename_jstr, lineno, linebuf_jstr, index);
    if (!java_exception)
        goto out_of_memory;

    /* Throw the newly-created JSException */
    if ((*jEnv)->Throw(jEnv, java_exception) < 0) {
        PR_ASSERT(0);
        jsj_LogError("Couldn't throw JSException\n");
        goto done;
    }

    /*
     * Release local references to Java objects, since some JVMs seem reticent
     * about collecting them otherwise.
     */
    (*jEnv)->DeleteLocalRef(jEnv, message_jstr);
    (*jEnv)->DeleteLocalRef(jEnv, filename_jstr);
    (*jEnv)->DeleteLocalRef(jEnv, linebuf_jstr);
    (*jEnv)->DeleteLocalRef(jEnv, java_exception);

    goto done;

out_of_memory:
    /* No recovery possible */
    PR_ASSERT(0);
    jsj_LogError("Out of memory while attempting to throw JSException\n");

done:
    jsj_ClearPendingJSErrors(jsj_env);
}

/*
 * This function is called up returning from Java back into JS as a result of
 * a thrown netscape.javascript.JSException, which itself must have been caused
 * by a JS error when Java called into JS.  The original JS error is
 * reconstituted from the JSException and re-reported as a JS error.
 *
 * Returns JS_FALSE if an internal error occurs, JS_TRUE otherwise.
 */
JSBool
jsj_ReportUncaughtJSException(JSContext *cx, JNIEnv *jEnv, jthrowable java_exception)
{
    JSBool success;
    JSErrorReport report;
    const char *linebuf, *filename, *message, *tokenptr;
    jint lineno, token_index;
    jstring linebuf_jstr, filename_jstr, message_jstr;

    /* Initialize everything to NULL */
    memset(&report, 0, sizeof(JSErrorReport));
    success = JS_FALSE;
    filename_jstr = linebuf_jstr = NULL;
    filename = message = linebuf = tokenptr = NULL;

    lineno = (*jEnv)->GetIntField(jEnv, java_exception, njJSException_lineno);
    report.lineno = lineno;

    filename_jstr = (*jEnv)->GetObjectField(jEnv, java_exception, njJSException_filename);
    if ((*jEnv)->ExceptionOccurred(jEnv)) {
        jsj_UnexpectedJavaError(cx, jEnv, "Unable to access filename field of a JSException");
        goto done;
    }
    if (filename_jstr)
        filename = (*jEnv)->GetStringUTFChars(jEnv, filename_jstr, 0);
    report.filename = filename;

    linebuf_jstr = (*jEnv)->GetObjectField(jEnv, java_exception, njJSException_source);
    if ((*jEnv)->ExceptionOccurred(jEnv)) {
        jsj_UnexpectedJavaError(cx, jEnv, "Unable to access source field of a JSException");
        goto done;
    }
    if (linebuf_jstr)
        linebuf = (*jEnv)->GetStringUTFChars(jEnv, linebuf_jstr, 0);
    report.linebuf = linebuf;

    token_index = (*jEnv)->GetIntField(jEnv, java_exception, njJSException_lineno);
    report.tokenptr = linebuf + token_index;

    message_jstr = (*jEnv)->CallObjectMethod(jEnv, java_exception, jlThrowable_getMessage);
    if ((*jEnv)->ExceptionOccurred(jEnv)) {
        jsj_UnexpectedJavaError(cx, jEnv, "Unable to access message of a JSException");
        goto done;
    }
    if (message_jstr)
        message = (*jEnv)->GetStringUTFChars(jEnv, message_jstr, 0);

    js_ReportErrorAgain(cx, message, &report);

    success = JS_TRUE;

done:

    if (filename_jstr && filename)
        (*jEnv)->ReleaseStringUTFChars(jEnv, filename_jstr, filename);
    if (linebuf_jstr && linebuf)
        (*jEnv)->ReleaseStringUTFChars(jEnv, linebuf_jstr, linebuf);
    if (message_jstr && message)
        (*jEnv)->ReleaseStringUTFChars(jEnv, message_jstr, message);
    return success;
}



/*************** Utilities for calling JavaScript from Java *****************/

/*
 * This routine is called just prior to invoking any JS API function from
 * Java.  It performs a common set of chores, such as obtaining the LiveConnect
 * state for the invoking Java thread and determining the JSContext to be
 * used for this thread.
 *
 * Returns NULL on failure.
 */
JSJavaThreadState *
enter_js(JNIEnv *jEnv, jobject java_wrapper_obj,
         JSContext **cxp, JSObject **js_objp, JSErrorReporter *old_error_reporterp)
{
    JSContext *cx;
    char *err_msg;
    JSObject *js_obj;
    JSJavaThreadState *jsj_env;

    cx = NULL;
    err_msg = NULL;

    /* Invoke callback, presumably used to implement concurrency constraints */
    if (JSJ_callbacks->enter_js_from_java) {
        if (!JSJ_callbacks->enter_js_from_java(&err_msg))
            goto entry_failure;
    }

    /* Check the JSObject pointer in the wrapper object for null while holding
       the JS lock to deal with shutdown issues. */
    if (js_objp) {
        js_obj = (JSObject *)((*jEnv)->GetIntField(jEnv, java_wrapper_obj, njJSObject_internal));
        PR_ASSERT(js_obj);
        if (!js_obj)
            goto error;
        *js_objp = js_obj;
    }

    /* Get the per-thread state corresponding to the current Java thread */
    jsj_env = jsj_MapJavaThreadToJSJavaThreadState(jEnv, &err_msg);
    if (!jsj_env)
        goto error;

    /* Get the JSContext that we're supposed to use for this Java thread */
    cx = jsj_env->cx;
    if (!cx) {
        /* We called spontaneously into JS from Java, rather than from JS into
           Java and back into JS.  Invoke a callback to obtain/create a
           JSContext for us to use. */
        if (JSJ_callbacks->map_jsj_thread_to_js_context) {
            cx = JSJ_callbacks->map_jsj_thread_to_js_context(jsj_env, &err_msg);
            if (!cx)
                goto error;
        } else {
            err_msg = PR_smprintf("Unable to find/create JavaScript execution "
                                  "context for JNI thread 0x%08x", jEnv);
            goto error;
        }
    }
    *cxp = cx;

    /*
     * Capture all JS error reports so that they can be thrown into the Java
     * caller as an instance of netscape.javascript.JSException.
     */
    *old_error_reporterp =
        JS_SetErrorReporter(cx, capture_js_error_reports_for_java);

    return jsj_env;

error:
    /* Invoke callback, presumably used to implement concurrency constraints */
    if (JSJ_callbacks->exit_js)
        JSJ_callbacks->exit_js();

entry_failure:
    if (err_msg) {
        if (cx)
            JS_ReportError(cx, err_msg);
        else
            jsj_LogError(err_msg);
        free(err_msg);
    }

    return NULL;
}

/*
 * This utility function is called just prior to returning into Java from JS.
 */
static JSBool
exit_js(JSContext *cx, JSJavaThreadState *jsj_env, JSErrorReporter original_reporter)
{
    JNIEnv *jEnv;

    /* Restore the JS error reporter */
    JS_SetErrorReporter(cx, original_reporter);

    jEnv = jsj_env->jEnv;

#ifdef DEBUG
    /* Any Java exceptions should have been noticed and reported already */
    if ((*jEnv)->ExceptionOccurred(jEnv)) {
        PR_ASSERT(0);
        jsj_LogError("Unhandled Java exception detected");
        return JS_FALSE;
    }
#endif

    /*
     * Convert reported JS errors to JSExceptions, unless the errors were
     * themselves the result of Java exceptions, in which case the original
     * Java exception is simply propagated.
     */
    throw_any_pending_js_error_as_a_java_exception(jsj_env);

    /* Invoke callback, presumably used to implement concurrency constraints */
    if (JSJ_callbacks->exit_js)
        JSJ_callbacks->exit_js();

    return JS_TRUE;
}


/* Get the JavaClassDescriptor that corresponds to java.lang.Object */
static JavaClassDescriptor *
get_jlObject_descriptor(JSContext *cx, JNIEnv *jEnv)
{
    /* The JavaClassDescriptor for java.lang.Object */
    static JavaClassDescriptor *jlObject_descriptor = NULL;

    if (!jlObject_descriptor)
        jlObject_descriptor = jsj_GetJavaClassDescriptor(cx, jEnv, jlObject);
    return jlObject_descriptor;
}

/****************** Implementation of methods of JSObject *******************/

#ifdef XP_MAC
#pragma export on
#endif

/*
 * Class:     netscape_javascript_JSObject
 * Method:    initClass
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_netscape_javascript_JSObject_initClass(JNIEnv *jEnv, jclass java_class)
{
    init_js_obj_reflections_table();
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    getMember
 * Signature: (Ljava/lang/String;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL
Java_netscape_javascript_JSObject_getMember(JNIEnv *jEnv,
                                            jobject java_wrapper_obj,
                                            jstring property_name_jstr)
{
    JSContext *cx;
    JSObject *js_obj;
    jsval js_val;
    int dummy_cost;
    JSBool dummy_bool;
    const jchar *property_name_ucs2;
    jsize property_name_len;
    JSErrorReporter saved_reporter;
    jobject member;
    jboolean is_copy;
    JSJavaThreadState *jsj_env;
    
    jsj_env = enter_js(jEnv, java_wrapper_obj, &cx, &js_obj, &saved_reporter);
    if (!jsj_env)
        return NULL;

    property_name_ucs2 = NULL;
    if (!property_name_jstr) {
        JS_ReportError(cx, "illegal null member name");
        member = NULL;
        goto done;
    }

    /* Get the Unicode string for the JS property name */
    property_name_ucs2 = (*jEnv)->GetStringChars(jEnv, property_name_jstr, &is_copy);
    if (!property_name_ucs2) {
        PR_ASSERT(0);
        goto done;
    }
    property_name_len = (*jEnv)->GetStringLength(jEnv, property_name_jstr);
    
    if (!JS_GetUCProperty(cx, js_obj, property_name_ucs2, property_name_len, &js_val))
        goto done;

    jsj_ConvertJSValueToJavaObject(cx, jEnv, js_val, get_jlObject_descriptor(cx, jEnv),
                                   &dummy_cost, &member, &dummy_bool);

done:
    if (property_name_ucs2)
        (*jEnv)->ReleaseStringChars(jEnv, property_name_jstr, property_name_ucs2);
    if (!exit_js(cx, jsj_env, saved_reporter))
        return NULL;
    
    return member;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    getSlot
 * Signature: (I)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL
Java_netscape_javascript_JSObject_getSlot(JNIEnv *jEnv,
                                          jobject java_wrapper_obj,
                                          jint slot)
{
    JSContext *cx;
    JSObject *js_obj;
    jsval js_val;
    int dummy_cost;
    JSBool dummy_bool;
    JSErrorReporter saved_reporter;
    jobject member;
    JSJavaThreadState *jsj_env;
    
    jsj_env = enter_js(jEnv, java_wrapper_obj, &cx, &js_obj, &saved_reporter);
    if (!jsj_env)
        return NULL;
    

    if (!JS_GetElement(cx, js_obj, slot, &js_val))
        goto done;
    if (!jsj_ConvertJSValueToJavaObject(cx, jEnv, js_val, get_jlObject_descriptor(cx, jEnv),
                                        &dummy_cost, &member, &dummy_bool))
        goto done;

done:
    if (!exit_js(cx, jsj_env, saved_reporter))
        return NULL;
    
    return member;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    setMember
 * Signature: (Ljava/lang/String;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL
Java_netscape_javascript_JSObject_setMember(JNIEnv *jEnv,
                                            jobject java_wrapper_obj,
                                            jstring property_name_jstr,
                                            jobject java_obj)
{
    JSContext *cx;
    JSObject *js_obj;
    jsval js_val;
    const jchar *property_name_ucs2;
    jsize property_name_len;
    JSErrorReporter saved_reporter;
    jboolean is_copy;
    JSJavaThreadState *jsj_env;
    
    jsj_env = enter_js(jEnv, java_wrapper_obj, &cx, &js_obj, &saved_reporter);
    if (!jsj_env)
        return;
    
    property_name_ucs2 = NULL;
    if (!property_name_jstr) {
        JS_ReportError(cx, "illegal null member name");
        goto done;
    }

    /* Get the Unicode string for the JS property name */
    property_name_ucs2 = (*jEnv)->GetStringChars(jEnv, property_name_jstr, &is_copy);
    if (!property_name_ucs2) {
        PR_ASSERT(0);
        goto done;
    }
    property_name_len = (*jEnv)->GetStringLength(jEnv, property_name_jstr);
    
    if (!jsj_ConvertJavaObjectToJSValue(cx, jEnv, java_obj, &js_val))
        goto done;

    JS_SetUCProperty(cx, js_obj, property_name_ucs2, property_name_len, &js_val);

done:
    if (property_name_ucs2)
        (*jEnv)->ReleaseStringChars(jEnv, property_name_jstr, property_name_ucs2);
    exit_js(cx, jsj_env, saved_reporter);
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    setSlot
 * Signature: (ILjava/lang/Object;)V
 */
JNIEXPORT void JNICALL
Java_netscape_javascript_JSObject_setSlot(JNIEnv *jEnv,
                                          jobject java_wrapper_obj,
                                          jint slot,
                                          jobject java_obj)
{
    JSContext *cx;
    JSObject *js_obj;
    jsval js_val;
    JSErrorReporter saved_reporter;
    JSJavaThreadState *jsj_env;
    
    jsj_env = enter_js(jEnv, java_wrapper_obj, &cx, &js_obj, &saved_reporter);
    if (!jsj_env)
        return;
    
    if (!jsj_ConvertJavaObjectToJSValue(cx, jEnv, java_obj, &js_val))
        goto done;
    JS_SetElement(cx, js_obj, slot, &js_val);

done:
    exit_js(cx, jsj_env, saved_reporter);
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    removeMember
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_netscape_javascript_JSObject_removeMember(JNIEnv *jEnv,
                                               jobject java_wrapper_obj,
                                               jstring property_name_jstr)
{
    JSContext *cx;
    JSObject *js_obj;
    jsval js_val;
    const jchar *property_name_ucs2;
    jsize property_name_len;
    JSErrorReporter saved_reporter;
    jboolean is_copy;
    JSJavaThreadState *jsj_env;
    
    jsj_env = enter_js(jEnv, java_wrapper_obj, &cx, &js_obj, &saved_reporter);
    if (!jsj_env)
        return;
    
    if (!property_name_jstr) {
        JS_ReportError(cx, "illegal null member name");
        goto done;
    }
    /* Get the Unicode string for the JS property name */
    property_name_ucs2 = (*jEnv)->GetStringChars(jEnv, property_name_jstr, &is_copy);
    if (!property_name_ucs2) {
        PR_ASSERT(0);
        goto done;
    }
    property_name_len = (*jEnv)->GetStringLength(jEnv, property_name_jstr);
    
    JS_DeleteUCProperty2(cx, js_obj, property_name_ucs2, property_name_len, &js_val);

    (*jEnv)->ReleaseStringChars(jEnv, property_name_jstr, property_name_ucs2);

done:
    exit_js(cx, jsj_env, saved_reporter);
    return;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    call
 * Signature: (Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL
Java_netscape_javascript_JSObject_call(JNIEnv *jEnv, jobject java_wrapper_obj,
                                       jstring function_name_jstr, jobjectArray java_args)
{
    int i, argc, arg_num;
    jsval *argv;
    JSContext *cx;
    JSObject *js_obj;
    jsval js_val, function_val;
    int dummy_cost;
    JSBool dummy_bool;
    const jchar *function_name_ucs2;
    jsize function_name_len;
    JSErrorReporter saved_reporter;
    jboolean is_copy;
    jobject result;
    JSJavaThreadState *jsj_env;
    
    jsj_env = enter_js(jEnv, java_wrapper_obj, &cx, &js_obj, &saved_reporter);
    if (!jsj_env)
        return NULL;
    
    function_name_ucs2 = NULL;
    result = NULL;
    if (!function_name_jstr) {
        JS_ReportError(cx, "illegal null JavaScript function name");
        goto done;
    }

    /* Get the function name to eval as raw Unicode characters */
    function_name_ucs2 = (*jEnv)->GetStringChars(jEnv, function_name_jstr, &is_copy);
    if (!function_name_ucs2) {
        PR_ASSERT(0);
        goto done;
    }
    function_name_len = (*jEnv)->GetStringLength(jEnv, function_name_jstr);
    
    /* Allocate space for JS arguments */
    if (java_args) {
        argc = (*jEnv)->GetArrayLength(jEnv, java_args);
        argv = (jsval*)JS_malloc(cx, argc * sizeof(jsval));
    } else {
        argc = 0;
        argv = 0;
    }

    /* Convert arguments from Java to JS values */
    for (arg_num = 0; arg_num < argc; arg_num++) {
        jobject arg = (*jEnv)->GetObjectArrayElement(jEnv, java_args, arg_num);

        if (!jsj_ConvertJavaObjectToJSValue(cx, jEnv, arg, &argv[arg_num]))
            goto cleanup_argv;
        JS_AddRoot(cx, &argv[arg_num]);
    }

    if (!JS_GetUCProperty(cx, js_obj, function_name_ucs2, function_name_len,
                          &function_val))
        goto cleanup_argv;

    if (!JS_CallFunctionValue(cx, js_obj, function_val, argc, argv, &js_val))
        goto cleanup_argv;

    jsj_ConvertJSValueToJavaObject(cx, jEnv, js_val, get_jlObject_descriptor(cx, jEnv),
                                   &dummy_cost, &result, &dummy_bool);

cleanup_argv:
    if (argv) {
        for (i = 0; i < arg_num; i++)
            JS_RemoveRoot(cx, &argv[i]);
        JS_free(cx, argv);
    }

done:
    if (function_name_ucs2)
        (*jEnv)->ReleaseStringChars(jEnv, function_name_jstr, function_name_ucs2);
    if (!exit_js(cx, jsj_env, saved_reporter))
        return NULL;
    
    return result;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    eval
 * Signature: (Ljava/lang/String;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL
Java_netscape_javascript_JSObject_eval(JNIEnv *jEnv,
                                       jobject java_wrapper_obj,
                                       jstring eval_jstr)
{
    const char *codebase;
    JSPrincipals *principals;
    JSContext *cx;
    JSBool eval_succeeded;
    JSObject *js_obj;
    jsval js_val;
    int dummy_cost;
    JSBool dummy_bool;
    const jchar *eval_ucs2;
    jsize eval_len;
    JSErrorReporter saved_reporter;
    jboolean is_copy;
    jobject result;
    JSJavaThreadState *jsj_env;
    
    jsj_env = enter_js(jEnv, java_wrapper_obj, &cx, &js_obj, &saved_reporter);
    if (!jsj_env)
        return NULL;
    
    result = NULL;
    eval_ucs2 = NULL;
    if (!eval_jstr) {
        JS_ReportError(cx, "illegal null string eval argument");
        goto done;
    }

    /* Get the string to eval as raw Unicode characters */
    eval_ucs2 = (*jEnv)->GetStringChars(jEnv, eval_jstr, &is_copy);
    if (!eval_ucs2) {
        PR_ASSERT(0);
        goto done;
    }
    eval_len = (*jEnv)->GetStringLength(jEnv, eval_jstr);
    
    /* Set up security stuff */
    principals = NULL;
    if (JSJ_callbacks->get_JSPrincipals_from_java_caller)
        principals = JSJ_callbacks->get_JSPrincipals_from_java_caller(jEnv);
    codebase = principals ? principals->codebase : NULL;

    /* Have the JS engine evaluate the unicode string */
    eval_succeeded = JS_EvaluateUCScriptForPrincipals(cx, js_obj, principals,
                                                      eval_ucs2, eval_len,
                                                      codebase, 0, &js_val);
    if (!eval_succeeded)
        goto done;

    /* Convert result to a subclass of java.lang.Object */
    jsj_ConvertJSValueToJavaObject(cx, jEnv, js_val, get_jlObject_descriptor(cx, jEnv),
                                   &dummy_cost, &result, &dummy_bool);

done:
    if (eval_ucs2)
        (*jEnv)->ReleaseStringChars(jEnv, eval_jstr, eval_ucs2);
    if (!exit_js(cx, jsj_env, saved_reporter))
        return NULL;
    
    return result;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    toString
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_netscape_javascript_JSObject_toString(JNIEnv *jEnv,
                                           jobject java_wrapper_obj)
{
    jstring result;
    JSContext *cx;
    JSObject *js_obj;
    JSString *jsstr;
    JSErrorReporter saved_reporter;
    JSJavaThreadState *jsj_env;
    
    jsj_env = enter_js(jEnv, java_wrapper_obj, &cx, &js_obj, &saved_reporter);
    if (!jsj_env)
        return NULL;
    
    result = NULL;
    jsstr = JS_ValueToString(cx, OBJECT_TO_JSVAL(js_obj));
    if (jsstr)
        result = jsj_ConvertJSStringToJavaString(cx, jEnv, jsstr);
    if (!result)
        result = (*jEnv)->NewStringUTF(jEnv, "*JavaObject*");

    if (!exit_js(cx, jsj_env, saved_reporter))
        return NULL;
    
    return result;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    getWindow
 * Signature: (Ljava/applet/Applet;)Lnetscape/javascript/JSObject;
 */
JNIEXPORT jobject JNICALL
Java_netscape_javascript_JSObject_getWindow(JNIEnv *jEnv,
                                            jclass js_object_class,
                                            jobject java_applet_obj)
{
    char *err_msg;
    JSContext *cx;
    JSObject *js_obj;
    jsval js_val;
    int dummy_cost;
    JSBool dummy_bool;
    JSErrorReporter saved_reporter;
    jobject java_obj;
    JSJavaThreadState *jsj_env;
    
    jsj_env = enter_js(jEnv, NULL, &cx, NULL, &saved_reporter);
    if (!jsj_env)
        return NULL;
    
    err_msg = NULL;
    java_obj = NULL;
    js_obj = JSJ_callbacks->map_java_object_to_js_object(jEnv, java_applet_obj, &err_msg);
    if (!js_obj) {
        if (err_msg) {
            JS_ReportError(cx, err_msg);
            free(err_msg);
        }
        goto done;
    }
    js_val = OBJECT_TO_JSVAL(js_obj);
    jsj_ConvertJSValueToJavaObject(cx, jEnv, js_val, get_jlObject_descriptor(cx, jEnv),
                                   &dummy_cost, &java_obj, &dummy_bool);
done:
    if (!exit_js(cx, jsj_env, saved_reporter))
        return NULL;
    
    return java_obj;
}

/*
 * Class:     netscape_javascript_JSObject
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_netscape_javascript_JSObject_finalize(JNIEnv *jEnv, jobject java_wrapper_obj)
{
    JSContext *cx;
    JSErrorReporter saved_reporter;
    JSJavaThreadState *jsj_env;
    JSObject *js_obj;
    
    jsj_env = enter_js(jEnv, java_wrapper_obj, &cx, &js_obj, &saved_reporter);
    if (!jsj_env)   /* Note: memory leak if we exit here */
        return;
    remove_js_obj_reflection_from_hashtable(cx, js_obj);
    exit_js(cx, jsj_env, saved_reporter);
}

