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

/*
 * This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * It contains low-level utility code.
 *
 */

#include <stdlib.h>
#include <string.h>

#include "prtypes.h"
#include "prprintf.h"
#include "prassert.h"
#include "prosdep.h"

#include "jsj_private.h"        /* LiveConnect internals */
#include "jsjava.h"             /* External LiveConnect API */


/*
 * This is a hash-table utility routine that computes the hash code of a Java
 * object by calling java.lang.System.identityHashCode()
 */
PR_CALLBACK prhashcode
jsj_HashJavaObject(const void *key)
{
    prhashcode hash_code;
    jobject java_obj;

    java_obj = (jobject)key;
    hash_code = (*jENV)->CallStaticIntMethod(jENV, jlSystem,
                                             jlSystem_identityHashCode, java_obj);
    PR_ASSERT(!(*jENV)->ExceptionOccurred(jENV));
    return hash_code;
}

/* 
 * This is a hash-table utility routine for comparing two Java objects.
 * It's not possible to use the == operator to directly compare two jobject's,
 * since they're opaque references and aren't guaranteed to be simple pointers
 * or handles (though they may be in some JVM implementations).  Instead,
 * use the JNI routine for comparing the two objects.
 */
PR_CALLBACK intN
jsj_JavaObjectComparator(const void *v1, const void *v2)
{
    jobject java_obj1, java_obj2;

    java_obj1 = (jobject)v1;
    java_obj2 = (jobject)v2;

    if (java_obj1 == java_obj2)
        return 1;
    return (*jENV)->IsSameObject(jENV, java_obj1, java_obj2);
}

/*
 * Return a UTF8, null-terminated encoding of a Java string.  The string must
 * be free'ed by the caller.
 *
 * If an error occurs, returns NULL and calls the JS error reporter.
 */
const char *
jsj_DupJavaStringUTF(JSContext *cx, JNIEnv *jEnv, jstring jstr)
{
    const char *str, *retval;

    str = (*jEnv)->GetStringUTFChars(jEnv, jstr, 0);
    if (!str) {
        jsj_UnexpectedJavaError(cx, jEnv, "Can't get UTF8 characters from "
                                          "Java string");
        return NULL;
    }
    retval = JS_strdup(cx, str);
    (*jEnv)->ReleaseStringUTFChars(jEnv, jstr, str);
    return retval;
}

JSBool
JavaStringToId(JSContext *cx, JNIEnv *jEnv, jstring jstr, jsid *idp)
{
    const jschar *ucs2;
    JSString *jsstr;
    jsize ucs2_len;
    jsval val;
    
    ucs2 = (*jEnv)->GetStringChars(jEnv, jstr, 0);
    if (!ucs2) {
        jsj_UnexpectedJavaError(cx, jEnv, "Couldn't obtain Unicode characters"
                                "from Java string");
        return JS_FALSE;
    }
    
    ucs2_len = (*jEnv)->GetStringLength(jEnv, jstr);
    jsstr = JS_InternUCStringN(cx, ucs2, ucs2_len);
    (*jEnv)->ReleaseStringChars(jEnv, jstr, ucs2);
    if (!jsstr)
        return JS_FALSE;

    val = STRING_TO_JSVAL(jsstr);
    JS_ValueToId(cx, STRING_TO_JSVAL(jsstr), idp);
    return JS_TRUE;
}

/* Not used ?
const char *
jsj_ClassNameOfJavaObject(JSContext *cx, JNIEnv *jEnv, jobject java_object)
{
    jobject java_class;

    java_class = (*jEnv)->GetObjectClass(jEnv, java_object);

    if (!java_class) {
        PR_ASSERT(0);
        return NULL;
    }

    return jsj_GetJavaClassName(cx, jEnv, java_class);
}
*/

/*
 * Return, as a C string, the error message associated with a Java exception
 * that occurred as a result of a JNI call, preceded by the class name of
 * the exception.  As a special case, if the class of the exception is
 * netscape.javascript.JSException, the exception class name is omitted.
 *
 * NULL is returned if no Java exception is pending.  The caller is
 * responsible for free'ing the returned string.  On exit, the Java exception
 * is *not* cleared.
 */
const char *
jsj_GetJavaErrorMessage(JNIEnv *jEnv)
{
    const char *java_error_msg;
    char *error_msg = NULL;
    jthrowable exception;
    jstring java_exception_jstring;

    exception = (*jEnv)->ExceptionOccurred(jEnv);
    if (exception) {
        java_exception_jstring =
            (*jEnv)->CallObjectMethod(jEnv, exception, jlThrowable_toString);
        java_error_msg = (*jEnv)->GetStringUTFChars(jEnv, java_exception_jstring, NULL);
        error_msg = strdup((char*)java_error_msg);
        (*jEnv)->ReleaseStringUTFChars(jEnv, java_exception_jstring, java_error_msg);

#ifdef DEBUG
        /* (*jEnv)->ExceptionDescribe(jEnv); */
#endif
    }
    return error_msg;
}    

/*
 * Return, as a C string, the JVM stack trace associated with a Java
 * exception, as would be printed by java.lang.Throwable.printStackTrace().
 * The caller is responsible for free'ing the returned string.
 *
 * Returns NULL if an error occurs.
 */
static const char *
get_java_stack_trace(JSContext *cx, JNIEnv *jEnv, jthrowable java_exception)
{
    const char *backtrace;
    jstring backtrace_jstr;

    backtrace = NULL;
    if (java_exception && njJSUtil_getStackTrace) {
        backtrace_jstr = (*jEnv)->CallStaticObjectMethod(jEnv, njJSUtil,
                                                         njJSUtil_getStackTrace,
                                                         java_exception);
        if (!backtrace_jstr) {
            jsj_UnexpectedJavaError(cx, jEnv, "Unable to get exception stack trace");
            return NULL;
        }
        backtrace = jsj_DupJavaStringUTF(cx, jEnv, backtrace_jstr);
    }
    return backtrace;
} 

/* Full Java backtrace when Java exceptions reported to JavaScript */
#define REPORT_JAVA_EXCEPTION_STACK_TRACE

/*
 * This is a wrapper around JS_ReportError(), useful when an error condition
 * is the result of a JVM failure or exception condition.  It appends the
 * message associated with the pending Java exception to the passed in
 * printf-style format string and arguments.
 */
static void
vreport_java_error(JSContext *cx, JNIEnv *jEnv, const char *format, va_list ap)
{
    char *error_msg, *js_error_msg;
    const char *java_stack_trace;
    const char *java_error_msg;
    jthrowable java_exception;
        
    java_error_msg = NULL;
    java_exception = (*jEnv)->ExceptionOccurred(jEnv);
    if (java_exception && njJSException && 
        (*jEnv)->IsInstanceOf(jEnv, java_exception, njJSException)) {
        (*jEnv)->ExceptionClear(jEnv);
        jsj_ReportUncaughtJSException(cx, jEnv, java_exception);
        return;
    }

    js_error_msg = PR_vsmprintf(format, ap);

    if (!js_error_msg) {
        PR_ASSERT(0);       /* Out-of-memory */
        return;
    }

#ifdef REPORT_JAVA_EXCEPTION_STACK_TRACE

    java_stack_trace = get_java_stack_trace(cx, jEnv, java_exception);
    if (java_stack_trace) {
        error_msg = PR_smprintf("%s\n%s", js_error_msg, java_stack_trace);
        free((char*)java_stack_trace);
        if (!error_msg) {
            PR_ASSERT(0);       /* Out-of-memory */
            return;
        }
    }

#else

    java_error_msg = jsj_GetJavaErrorMessage(jEnv);
    if (java_error_msg) {
        error_msg = PR_smprintf("%s (%s)\n", js_error_msg, java_error_msg);
        free((char*)java_error_msg);
        free(js_error_msg);
    } else {
        error_msg = js_error_msg;
    }
#endif
    
    JS_ReportError(cx, error_msg);
    
    /* Important: the Java exception must not be cleared until the reporter
       has been called, because the capture_js_error_reports_for_java(),
       called from JS_ReportError(), needs to read the exception from the JVM */
    (*jEnv)->ExceptionClear(jEnv);
    free(error_msg);
}

void
jsj_ReportJavaError(JSContext *cx, JNIEnv *env, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vreport_java_error(cx, env, format, ap);
    va_end(ap);
}

/*
 * Same as jsj_ReportJavaError, except "internal error: " is prepended
 * to message.
 */
void
jsj_UnexpectedJavaError(JSContext *cx, JNIEnv *env, const char *format, ...)
{
    va_list ap;
    const char *format2;

    va_start(ap, format);
    format2 = PR_smprintf("internal error: %s", format);
    if (format2)
        vreport_java_error(cx, env, format2, ap);
    free((void*)format2);
    va_end(ap);
}

/*
 * Most LiveConnect errors are signaled by calling JS_ReportError(),
 * but in some circumstances, the target JSContext for such errors
 * is not determinable, e.g. during initialization.  In such cases
 * any error messages are routed to this function.
 */
void
jsj_LogError(const char *error_msg)
{
    if (JSJ_callbacks && JSJ_callbacks->error_print)
        JSJ_callbacks->error_print(error_msg);
    else
        fputs(error_msg, stderr);
}

jsize
jsj_GetJavaArrayLength(JSContext *cx, JNIEnv *jEnv, jarray java_array)
{
    jsize array_length = (*jEnv)->GetArrayLength(jEnv, java_array);
    if ((*jEnv)->ExceptionOccurred(jEnv)) {
        jsj_UnexpectedJavaError(cx, jEnv, "Couldn't obtain array length");
        return -1;
    }
    return array_length;
}

JSJavaThreadState *
jsj_MapJSContextToJSJThread(JSContext *cx, JNIEnv **envp)
{
    JSJavaThreadState *jsj_env;
    char *err_msg;

    *envp = NULL;
    err_msg = NULL;
    jsj_env = JSJ_callbacks->map_js_context_to_jsj_thread(cx, &err_msg);
    if (!jsj_env) {
        if (err_msg) {
            JS_ReportError(cx, err_msg);
            free(err_msg);
        }
        return NULL;
    }
    if (envp)
        *envp = jsj_env->jEnv;
    return jsj_env;
}
