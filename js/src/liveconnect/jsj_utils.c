/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*
 * This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * It contains low-level utility code.
 *
 */

#include <stdlib.h>
#include <string.h>

#include "jsj_private.h"        /* LiveConnect internals */
#include "jsjava.h"             /* External LiveConnect API */


/*
 * This is a hash-table utility routine that computes the hash code of a Java
 * object by calling java.lang.System.identityHashCode()
 */
JSJHashNumber JS_DLL_CALLBACK
jsj_HashJavaObject(const void *key, void* env)
{
    JSHashNumber hash_code;
    jobject java_obj;
    JNIEnv *jEnv;

    java_obj = (jobject)key;
    jEnv = (JNIEnv*) env;
    JS_ASSERT(!(*jEnv)->ExceptionOccurred(jEnv));
    hash_code = (*jEnv)->CallStaticIntMethod(jEnv, jlSystem,
                                             jlSystem_identityHashCode, java_obj);

#ifdef DEBUG
    if ((*jEnv)->ExceptionOccurred(jEnv)) {
        (*jEnv)->ExceptionDescribe(jEnv);
        JS_ASSERT(0);
    }
#endif

    return hash_code;
}

/* 
 * This is a hash-table utility routine for comparing two Java objects.
 * It's not possible to use the == operator to directly compare two jobject's,
 * since they're opaque references and aren't guaranteed to be simple pointers
 * or handles (though they may be in some JVM implementations).  Instead,
 * use the JNI routine for comparing the two objects.
 */
intN JS_DLL_CALLBACK
jsj_JavaObjectComparator(const void *v1, const void *v2, void *arg)
{
    jobject java_obj1, java_obj2;
    JNIEnv *jEnv;

    jEnv = (JNIEnv*)arg;
    java_obj1 = (jobject)v1;
    java_obj2 = (jobject)v2;

    if (java_obj1 == java_obj2)
        return 1;
    return (*jEnv)->IsSameObject(jEnv, java_obj1, java_obj2);
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
    if (exception && jlThrowable_toString) {
        java_exception_jstring =
            (*jEnv)->CallObjectMethod(jEnv, exception, jlThrowable_toString);
        if (!java_exception_jstring)
            goto done;
        java_error_msg = (*jEnv)->GetStringUTFChars(jEnv, java_exception_jstring, NULL);
        if (java_error_msg) {
            error_msg = strdup((char*)java_error_msg);
            (*jEnv)->ReleaseStringUTFChars(jEnv, java_exception_jstring, java_error_msg);
        }
        (*jEnv)->DeleteLocalRef(jEnv, java_exception_jstring);

#ifdef DEBUG
        /* (*jEnv)->ExceptionDescribe(jEnv); */
#endif
    }
done:
    if (exception)
        (*jEnv)->DeleteLocalRef(jEnv, exception);
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
        (*jEnv)->DeleteLocalRef(jEnv, backtrace_jstr);
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
    jobject java_obj;
    jclass java_class;
    JavaClassDescriptor *class_descriptor;
    jthrowable java_exception;
    JSType wrapped_exception_type;
    jsval js_exception;
       
    java_obj = NULL;
    class_descriptor = NULL;

    /* Get the exception out of the java environment. */
    java_exception = (*jEnv)->ExceptionOccurred(jEnv);
    if (!java_exception) {
        JSString *err_jsstr;
        char *err = JS_vsmprintf(format, ap);
        if (!err)
            return;
        err_jsstr = JS_NewString(cx, err, strlen(err));
        if (!err_jsstr)
            return;
        JS_SetPendingException(cx, STRING_TO_JSVAL(err_jsstr));
        return;
    }

    
    (*jEnv)->ExceptionClear(jEnv);
    
    /* Check for JSException */
    if (njJSException && 
        (*jEnv)->IsInstanceOf(jEnv, java_exception, njJSException)) {
        
        wrapped_exception_type = 
            (*jEnv)->GetIntField(jEnv, java_exception,
            njJSException_wrappedExceptionType);
        
        /* (int) to suppress warning */
        if ((int)wrapped_exception_type != JSTYPE_EMPTY) {
            java_obj = 
                (*jEnv)->GetObjectField(jEnv, java_exception, 
                njJSException_wrappedException);
            
            if ((java_obj == NULL) && 
                (wrapped_exception_type == JSTYPE_OBJECT)) {
                js_exception = JSVAL_NULL;
            } else { 
                java_class = (*jEnv)->GetObjectClass(jEnv, java_obj); 
                class_descriptor = jsj_GetJavaClassDescriptor(cx, jEnv, java_class);
                /* OK to delete ref, since above call adds global ref */
                (*jEnv)->DeleteLocalRef(jEnv, java_class);  
                
                /* Convert native JS values back to native types. */
                switch(wrapped_exception_type) {
                case JSTYPE_NUMBER:
                    if (!jsj_ConvertJavaObjectToJSNumber(cx, jEnv,
                        class_descriptor,
                        java_obj, 
                        &js_exception))
                        goto error;
                    break;
                case JSTYPE_BOOLEAN:
                    if (!jsj_ConvertJavaObjectToJSBoolean(cx, jEnv,
                        class_descriptor,
                        java_obj, 
                        &js_exception))
                        goto error;
                    break;
                case JSTYPE_STRING:
                    if (!jsj_ConvertJavaObjectToJSString(cx, jEnv,
                        class_descriptor,
                        java_obj, 
                        &js_exception))
                        goto error;
                    break;
                case JSTYPE_VOID:
                    js_exception = JSVAL_VOID;
                    break;
                case JSTYPE_OBJECT:
                case JSTYPE_FUNCTION:
                default:
                    if ((*jEnv)->IsInstanceOf(jEnv, java_obj, njJSObject)) {
                        js_exception = OBJECT_TO_JSVAL(jsj_UnwrapJSObjectWrapper(jEnv, java_obj));
                        if (!js_exception)
                            goto error;                        
                    } else {
                        if (!jsj_ConvertJavaObjectToJSValue(cx, jEnv, java_obj, 
                            &js_exception)) 
                            goto error;
                    }
                }
            }
        }
        /* Check for internal exception */
    } else {
        if (!JSJ_ConvertJavaObjectToJSValue(cx, java_exception,
            &js_exception)) {
            goto error;
        }
    }
    
    /* Set pending JS exception and clear the java exception. */
    JS_SetPendingException(cx, js_exception);                        
    goto done;

error:
    
    JS_ASSERT(0);
    jsj_LogError("Out of memory while attempting to throw JSException\n");
    
done:
    if (class_descriptor)
        jsj_ReleaseJavaClassDescriptor(cx, jEnv, class_descriptor);
    if (java_obj)
        (*jEnv)->DeleteLocalRef(jEnv, java_obj);
    if (java_exception)
        (*jEnv)->DeleteLocalRef(jEnv, java_exception);
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
    format2 = JS_smprintf("internal error: %s", format);
    if (format2) {
        vreport_java_error(cx, env, format2, ap);
        free((void*)format2);
    }
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

/*
        Error number handling. 

        jsj_ErrorFormatString is an array of format strings mapped
        by error number. It is initialized by the contents of jsj.msg

        jsj_GetErrorMessage is invoked by the engine whenever it wants 
        to convert an error number into an error format string.
*/
/*
        this define needs to go somewhere sensible
*/
#define JSJ_HAS_DFLT_MSG_STRINGS 1

JSErrorFormatString jsj_ErrorFormatString[JSJ_Err_Limit] = {
#if JSJ_HAS_DFLT_MSG_STRINGS
#define MSG_DEF(name, number, count, format) \
    { format, count } ,
#else
#define MSG_DEF(name, number, count, format) \
    { NULL, count } ,
#endif
#include "jsj.msg"
#undef MSG_DEF
};

const JSErrorFormatString *
jsj_GetErrorMessage(void *userRef, const char *locale, const uintN errorNumber)
{
    if ((errorNumber > 0) && (errorNumber < JSJ_Err_Limit))
            return &jsj_ErrorFormatString[errorNumber];
        else
            return NULL;
}

jsize
jsj_GetJavaArrayLength(JSContext *cx, JNIEnv *jEnv, jarray java_array)
{
    jsize array_length = (*jEnv)->GetArrayLength(jEnv, java_array);
    jthrowable java_exception = (*jEnv)->ExceptionOccurred(jEnv);
    if (java_exception) {
        jsj_UnexpectedJavaError(cx, jEnv, "Couldn't obtain array length");
        (*jEnv)->DeleteLocalRef(jEnv, java_exception);
        return -1;
    }
    return array_length;
}

static JSJavaThreadState *the_java_jsj_env = NULL;

JSJavaThreadState *
jsj_EnterJava(JSContext *cx, JNIEnv **envp)
{
    JSJavaThreadState *jsj_env;
    char *err_msg;

    *envp = NULL;
    err_msg = NULL;

    jsj_env = the_java_jsj_env;
    if (jsj_env == NULL && JSJ_callbacks && JSJ_callbacks->map_js_context_to_jsj_thread)
        jsj_env = JSJ_callbacks->map_js_context_to_jsj_thread(cx, &err_msg);
    if (!jsj_env) {
        if (err_msg) {
            JS_ReportError(cx, err_msg);
            free(err_msg);
        }
        return NULL;
    }

    JS_ASSERT((jsj_env->recursion_depth == 0) || (jsj_env->cx == cx));
    jsj_env->recursion_depth++;

    /* bug #60018:  prevent dangling pointer to JSContext */
    if (!jsj_env->cx)
        jsj_env->cx = cx;

    if (envp)
        *envp = jsj_env->jEnv;
    return jsj_env;
}

extern void
jsj_ExitJava(JSJavaThreadState *jsj_env)
{
    JS_ASSERT(jsj_env->recursion_depth > 0);
    if (--jsj_env->recursion_depth == 0)
	jsj_env->cx = NULL;
}

/**
 * Since only one Java thread is allowed to enter JavaScript, this function is
 * used to enforce the use of that thread's state. The static global the_java_jsj_env
 * overrides using JSJ_callbacks->map_js_context_to_jsj_thread, which maps
 * native threads to JSJavaThreadStates. This isn't appropriate when Java calls
 * JavaScript, as there can be a many to one mapping from Java threads to native
 * threads.
 */
JSJavaThreadState *
jsj_SetJavaJSJEnv(JSJavaThreadState* java_jsj_env)
{
    JSJavaThreadState *old_jsj_env = the_java_jsj_env;
    the_java_jsj_env = java_jsj_env;
    return old_jsj_env;
}
