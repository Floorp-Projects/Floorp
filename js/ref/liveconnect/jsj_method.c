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
 * It contains the code used to reflect Java methods as properties of
 * JavaObject objects and the code to invoke those methods.
 *
 */

#include <stdlib.h>
#include <string.h>

#include "prtypes.h"
#include "prprintf.h"
#include "prassert.h"
#include "prosdep.h"

#include "jsj_private.h"        /* LiveConnect internals */
#include "jsjava.h"             /* LiveConnect external API */

/*
 * A helper function for jsj_ConvertJavaMethodSignatureToString():
 * Compute JNI-style (string) signatures for an array of JavaSignature objects
 * and concatenate the results into a single string.
 *
 * If an error is encountered, NULL is returned and the error reporter is called.
 */
static const char *
convert_java_method_arg_signatures_to_string(JSContext *cx,
                                             JavaSignature **arg_signatures,
                                             int num_args)
{
    const char *first_arg_signature, *rest_arg_signatures, *sig;
    JavaSignature **rest_args;

    /* Convert the first method argument in the array to a string */
    first_arg_signature = jsj_ConvertJavaSignatureToString(cx, arg_signatures[0]);
    if (!first_arg_signature)
        return NULL;

    /* If this is the last method argument in the array, we're done. */
    if (num_args == 1)
        return first_arg_signature;

    /* Convert the remaining method arguments to a string */
    rest_args = &arg_signatures[1];
    rest_arg_signatures =
        convert_java_method_arg_signatures_to_string(cx, rest_args, num_args - 1);
    if (!rest_arg_signatures) {
        free((void*)first_arg_signature);
        return NULL;
    }

    /* Concatenate the signature string of this argument with the signature
       strings of all the remaining arguments. */
    sig = PR_smprintf("%s%s", first_arg_signature, rest_arg_signatures);
    free((void*)first_arg_signature);
    free((void*)rest_arg_signatures);
    if (!sig) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }

    return sig;
}

/*
 * Compute a JNI-style (string) signature for the given method, e.g. the method
 * "String MyFunc(int, byte)" is translated to "(IB)Ljava/lang/String;".
 *
 * If an error is encountered, NULL is returned and the error reporter is called.
 */
const char *
jsj_ConvertJavaMethodSignatureToString(JSContext *cx,
                                       JavaMethodSignature *method_signature)
{
    JavaSignature **arg_signatures, *return_val_signature;
    const char *arg_sigs_cstr;
    const char *return_val_sig_cstr;
    const char *sig_cstr;

    arg_signatures = method_signature->arg_signatures;
    return_val_signature = method_signature->return_val_signature;

    /* Convert the method argument signatures to a C-string */
    arg_sigs_cstr = NULL;
    if (arg_signatures) {
        arg_sigs_cstr =
            convert_java_method_arg_signatures_to_string(cx, arg_signatures,
                                                         method_signature->num_args);
        if (!arg_sigs_cstr)
            return NULL;
    }

    /* Convert the method return value signature to a C-string */
    return_val_sig_cstr = jsj_ConvertJavaSignatureToString(cx, return_val_signature);
    if (!return_val_sig_cstr) {
        free((void*)arg_sigs_cstr);
        return NULL;
    }

    /* Compose method arg signatures string and return val signature string */
    if (arg_sigs_cstr) {
        sig_cstr = PR_smprintf("(%s)%s", arg_sigs_cstr, return_val_sig_cstr);
        free((void*)arg_sigs_cstr);
    } else {
        sig_cstr = PR_smprintf("()%s", return_val_sig_cstr);
    }

    free((void*)return_val_sig_cstr);

    if (!sig_cstr) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }
    return sig_cstr;
}

/*
 * A helper function for jsj_ConvertJavaMethodSignatureToHRString():
 * Compute human-readable string signatures for an array of JavaSignatures
 * and concatenate the results into a single string.
 *
 * If an error is encountered, NULL is returned and the error reporter is called.
 */
static const char *
convert_java_method_arg_signatures_to_hr_string(JSContext *cx,
                                                JavaSignature **arg_signatures,
                                                int num_args)
{
    const char *first_arg_signature, *rest_arg_signatures, *sig;
    JavaSignature **rest_args;

    /* Convert the first method argument in the array to a string */
    first_arg_signature = jsj_ConvertJavaSignatureToHRString(cx, arg_signatures[0]);
    if (!first_arg_signature)
        return NULL;

    /* If this is the last method argument in the array, we're done. */
    if (num_args == 1)
        return first_arg_signature;

    /* Convert the remaining method arguments to a string */
    rest_args = &arg_signatures[1];
    rest_arg_signatures =
        convert_java_method_arg_signatures_to_hr_string(cx, rest_args, num_args - 1);
    if (!rest_arg_signatures) {
        free((void*)first_arg_signature);
        return NULL;
    }

    /* Concatenate the signature string of this argument with the signature
       strings of all the remaining arguments. */
    sig = PR_smprintf("%s, %s", first_arg_signature, rest_arg_signatures);
    free((void*)first_arg_signature);
    free((void*)rest_arg_signatures);
    if (!sig) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }

    return sig;
}
/*
 * Compute a string signature for the given method using the same type names
 * that appear in Java source files, e.g. "int[][]", rather than the JNI-style
 * type strings that are provided by the functions above.  An example return
 * value might be "String MyFunc(int, byte, char[][], java.lang.String)".
 *
 * If an error is encountered, NULL is returned and the error reporter is called.
 */
const char *
jsj_ConvertJavaMethodSignatureToHRString(JSContext *cx,
                                         const char *method_name,
                                         JavaMethodSignature *method_signature)
{
    JavaSignature **arg_signatures, *return_val_signature;
    const char *arg_sigs_cstr;
    const char *return_val_sig_cstr;
    const char *sig_cstr;

    arg_signatures = method_signature->arg_signatures;
    return_val_signature = method_signature->return_val_signature;

    /* Convert the method argument signatures to a C-string */
    arg_sigs_cstr = NULL;
    if (arg_signatures) {
        arg_sigs_cstr =
            convert_java_method_arg_signatures_to_hr_string(cx, arg_signatures,
                                                            method_signature->num_args);
        if (!arg_sigs_cstr)
            return NULL;
    }

    /* Convert the method return value signature to a C-string */
    return_val_sig_cstr = jsj_ConvertJavaSignatureToHRString(cx, return_val_signature);
    if (!return_val_sig_cstr) {
        free((void*)arg_sigs_cstr);
        return NULL;
    }

    /* Compose method arg signatures string and return val signature string */
    if (arg_sigs_cstr) {
        sig_cstr = PR_smprintf("%s %s(%s)", return_val_sig_cstr, method_name, arg_sigs_cstr);
        free((void*)arg_sigs_cstr);
    } else {
        sig_cstr = PR_smprintf("%s %s()", return_val_sig_cstr, method_name);
    }

    free((void*)return_val_sig_cstr);

    if (!sig_cstr) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }
    return sig_cstr;
}
/*
 * Destroy the sub-structure of a JavaMethodSignature object without free'ing
 * the method signature itself.
 */
void
jsj_PurgeJavaMethodSignature(JSContext *cx, JNIEnv *jEnv, JavaMethodSignature *method_signature)
{
    int i, num_args;
    JavaSignature **arg_signatures;

    if (!method_signature)  /* Paranoia */
        return;

    /* Free the method argument signatures */
    num_args = method_signature->num_args;
    arg_signatures = method_signature->arg_signatures;
    for (i = 0; i < num_args; i++)
        jsj_ReleaseJavaClassDescriptor(cx, jEnv, arg_signatures[i]);
    if (arg_signatures)
        JS_free(cx, arg_signatures);

    /* Free the return type signature */
    if (method_signature->return_val_signature)
        jsj_ReleaseJavaClassDescriptor(cx, jEnv, method_signature->return_val_signature);
}

/*
 * Fill in the members of a JavaMethodSignature object using the method
 * argument, which can be either an instance of either java.lang.reflect.Method
 * or java.lang.reflect.Constructor.
 *
 * With normal completion, return the original method_signature argument.
 * If an error occurs, return NULL and call the error reporter.
 */
JavaMethodSignature *
jsj_InitJavaMethodSignature(JSContext *cx, JNIEnv *jEnv,
                           jobject method,
                           JavaMethodSignature *method_signature)
{
    int i;
    jboolean is_constructor;
    jclass return_val_class;
    jsize num_args;
    JavaSignature *return_val_signature;
    jarray arg_classes;
    jmethodID getParameterTypes;

    memset(method_signature, 0, sizeof (JavaMethodSignature));
    
    is_constructor = (*jEnv)->IsInstanceOf(jEnv, method, jlrConstructor);

    /* Get a Java array that lists the class of each of the method's arguments */
    if  (is_constructor)
        getParameterTypes = jlrConstructor_getParameterTypes;
    else
        getParameterTypes = jlrMethod_getParameterTypes;
    arg_classes = (*jEnv)->CallObjectMethod(jEnv, method, getParameterTypes);
    if (!arg_classes) {
        jsj_UnexpectedJavaError(cx, jEnv,
                                "Can't determine argument signature of method");
        goto error;
    }

    /* Compute the number of method arguments */
    num_args = jsj_GetJavaArrayLength(cx, jEnv, arg_classes);
    if (num_args < 0)
        goto error;
    method_signature->num_args = num_args;

    /* Build a JavaSignature array corresponding to the method's arguments */
    if (num_args) {
        JavaSignature **arg_signatures;

        /* Allocate an array of JavaSignatures, one for each method argument */
        size_t arg_signatures_size = num_args * sizeof(JavaSignature *);
        arg_signatures = (JavaSignature **)JS_malloc(cx, arg_signatures_size);
        if (!arg_signatures)
            goto error;
        memset(arg_signatures, 0, arg_signatures_size);
        method_signature->arg_signatures = arg_signatures;
        
        /* Build an array of JavaSignature objects, each of which corresponds
           to the type of an individual method argument. */
        for (i = 0; i < num_args; i++) {
            JavaSignature *a;
            jclass arg_class = (*jEnv)->GetObjectArrayElement(jEnv, arg_classes, i);
            
            a = arg_signatures[i] = jsj_GetJavaClassDescriptor(cx, jEnv, arg_class);
            if (!a) {
                jsj_ReportJavaError(cx, jEnv, "Could not determine Java class "
                                              "signature using java.lang.reflect");
                goto error;
            }
        }
    }

    /* Get the Java class of the method's return value */
    if (is_constructor) {
        /* Constructors always have a "void" return type */
        return_val_class = jlVoid_TYPE;
    } else {
        return_val_class =
            (*jEnv)->CallObjectMethod(jEnv, method, jlrMethod_getReturnType);
        if (!return_val_class) {
            jsj_UnexpectedJavaError(cx, jEnv,
                                    "Can't determine return type of method "
                                    "using java.lang.reflect.Method.getReturnType()");
            goto error;
        }
    }

    /* Build a JavaSignature object corresponding to the method's return value */
    return_val_signature = jsj_GetJavaClassDescriptor(cx, jEnv, return_val_class);
    if (!return_val_signature)
        goto error;
    method_signature->return_val_signature = return_val_signature;

    return method_signature;

error:

    jsj_PurgeJavaMethodSignature(cx, jEnv, method_signature);
    return NULL;
}

static JSBool
add_java_method_to_class_descriptor(JSContext *cx, JNIEnv *jEnv,
                                    JavaClassDescriptor *class_descriptor, 
                                    jstring method_name_jstr,
                                    jobject java_method,
                                    JSBool is_static_method,
                                    JSBool is_constructor)
{
    jmethodID methodID;
    JSFunction *fun;
    jclass java_class = class_descriptor->java_class;

    JavaMemberDescriptor *member_descriptor = NULL;
    const char *sig_cstr = NULL;
    const char *method_name = NULL;
    JavaMethodSignature *signature = NULL;
    JavaMethodSpec **specp, *method_spec = NULL;
            
    if (is_constructor) {
        member_descriptor = jsj_GetJavaClassConstructors(cx, class_descriptor);
    } else {
        if (is_static_method) {
            member_descriptor = jsj_GetJavaStaticMemberDescriptor(cx, jEnv, class_descriptor, method_name_jstr);
        } else {
            member_descriptor = jsj_GetJavaMemberDescriptor(cx, jEnv, class_descriptor, method_name_jstr);
            fun = JS_NewFunction(cx, jsj_JavaInstanceMethodWrapper, 0,
                                 JSFUN_BOUND_METHOD, NULL, member_descriptor->name);
            member_descriptor->invoke_func_obj = JS_GetFunctionObject(fun);
            JS_AddRoot(cx, &member_descriptor->invoke_func_obj);
        }
    }
    if (!member_descriptor)
        return JS_FALSE;
    
    method_spec = (JavaMethodSpec*)JS_malloc(cx, sizeof(JavaMethodSpec));
    if (!method_spec)
        goto error;
    memset(method_spec, 0, sizeof(JavaMethodSpec));

    signature = jsj_InitJavaMethodSignature(cx, jEnv, java_method, &method_spec->signature);
    if (!signature)
        goto error;

    method_name = JS_strdup(cx, member_descriptor->name);
    if (!method_name)
        goto error;
    method_spec->name = method_name;
    
    sig_cstr = jsj_ConvertJavaMethodSignatureToString(cx, signature);
    if (!sig_cstr)
        goto error;

    if (is_static_method)
        methodID = (*jEnv)->GetStaticMethodID(jEnv, java_class, method_name, sig_cstr);
    else
        methodID = (*jEnv)->GetMethodID(jEnv, java_class, method_name, sig_cstr);
    method_spec->methodID = methodID;
    
    if (!methodID) {
        jsj_UnexpectedJavaError(cx, jEnv,
                                "Can't get Java method ID for %s.%s() (sig=%s)",
                                class_descriptor->name, method_name, sig_cstr);
        goto error;
    }
    
    JS_free(cx, (char*)sig_cstr);

    /* Add method to end of list of overloaded methods for this class member */ 
    specp = &member_descriptor->methods;
    while (*specp) {
        specp = &(*specp)->next;
    }
    *specp = method_spec;

    return JS_TRUE;

error:
    if (method_spec)
        JS_FREE_IF(cx, (char*)method_spec->name);
    JS_FREE_IF(cx, (char*)sig_cstr);
    if (signature)
        jsj_PurgeJavaMethodSignature(cx, jEnv, signature);
    JS_FREE_IF(cx, method_spec);
    return JS_FALSE;
}

JSBool 
jsj_ReflectJavaMethods(JSContext *cx, JNIEnv *jEnv,
                       JavaClassDescriptor *class_descriptor,
                       JSBool reflect_only_static_methods)
{
    jarray joMethodArray, joConstructorArray;
    jsize num_methods, num_constructors;
    int i;
    jclass java_class;
    JSBool ok, reflect_only_instance_methods;

    reflect_only_instance_methods = !reflect_only_static_methods;

    /* Get a java array of java.lang.reflect.Method objects, by calling
       java.lang.Class.getMethods(). */
    java_class = class_descriptor->java_class;
    joMethodArray = (*jEnv)->CallObjectMethod(jEnv, java_class, jlClass_getMethods);
    if (!joMethodArray) {
        jsj_UnexpectedJavaError(cx, jEnv,
                                "Can't determine Java object's methods "
                                "using java.lang.Class.getMethods()");
        return JS_FALSE;
    }

    /* Iterate over the class methods */
    num_methods = (*jEnv)->GetArrayLength(jEnv, joMethodArray);
    for (i = 0; i < num_methods; i++) {
        jstring method_name_jstr;
        
        /* Get the i'th reflected method */
        jobject java_method = (*jEnv)->GetObjectArrayElement(jEnv, joMethodArray, i);

        /* Get the method modifiers, eg static, public, private, etc. */
        jint modifiers = (*jEnv)->CallIntMethod(jEnv, java_method, jlrMethod_getModifiers);

        /* Don't allow access to private or protected Java methods. */
        if (!(modifiers & ACC_PUBLIC))
            continue;

        /* Abstract methods can't be invoked */
        if (modifiers & ACC_ABSTRACT)
            continue;

        /* Reflect all instance methods or all static methods, but not both */
        if (reflect_only_static_methods != ((modifiers & ACC_STATIC) != 0))
            continue;

        
        /* Add a JavaMethodSpec object to the JavaClassDescriptor */
        method_name_jstr = (*jEnv)->CallObjectMethod(jEnv, java_method, jlrMethod_getName);
        ok = add_java_method_to_class_descriptor(cx, jEnv, class_descriptor, method_name_jstr, java_method,
                                                 reflect_only_static_methods, JS_FALSE);
        if (!ok)
            return JS_FALSE;
    }

    if (reflect_only_instance_methods)
        return JS_TRUE;
        
    joConstructorArray = (*jEnv)->CallObjectMethod(jEnv, java_class, jlClass_getConstructors);
    if (!joConstructorArray) {
        jsj_UnexpectedJavaError(cx, jEnv, "internal error: "
                                "Can't determine Java class's constructors "
                                "using java.lang.Class.getConstructors()");
        return JS_FALSE;
    }

    /* Iterate over the class constructors */
    num_constructors = (*jEnv)->GetArrayLength(jEnv, joConstructorArray);
    for (i = 0; i < num_constructors; i++) {
        /* Get the i'th reflected constructor */
        jobject java_constructor =
            (*jEnv)->GetObjectArrayElement(jEnv, joConstructorArray, i);

        /* Get the method modifiers, eg public, private, etc. */
        jint modifiers = (*jEnv)->CallIntMethod(jEnv, java_constructor,
                                                jlrConstructor_getModifiers);

        /* Don't allow access to private or protected Java methods. */
        if (!(modifiers & ACC_PUBLIC))
            continue;
        
        /* Add a JavaMethodSpec object to the JavaClassDescriptor */
        ok = add_java_method_to_class_descriptor(cx, jEnv, class_descriptor, NULL,
                                                 java_constructor,
                                                 JS_FALSE, JS_TRUE);
        if (!ok)
            return JS_FALSE;
    }
    return JS_TRUE;
}

/*
 * Free up a JavaMethodSpec and all its resources.
 */
void
jsj_DestroyMethodSpec(JSContext *cx, JNIEnv *jEnv, JavaMethodSpec *method_spec)
{
    JS_FREE_IF(cx, (char*)method_spec->name);
    jsj_PurgeJavaMethodSignature(cx, jEnv, &method_spec->signature);
    JS_free(cx, method_spec);
}

static JSBool
method_signature_matches_JS_args(JSContext *cx, JNIEnv *jEnv, uintN argc, jsval *argv,
                                 JavaMethodSignature *method_signature, int *cost)
{
    uintN i;
    JavaSignature *arg_signature;
    JSBool dummy_bool;

    if (argc != (uintN)method_signature->num_args)
        return JS_FALSE;

    for (i = 0; i < argc; i++) {
        arg_signature = method_signature->arg_signatures[i];
        if (!jsj_ConvertJSValueToJavaValue(cx, jEnv, argv[i], arg_signature, cost,
                                           NULL, &dummy_bool))
            return JS_FALSE;
    }
    return JS_TRUE;
}

/*
 * Return the JavaScript types that a JavaScript method was invoked with
 * as a string, e.g. a JS method invoked like foo(3, 'hey', 'you', true)
 * would cause a return value of "(number, string, string, boolean)".
 * The returned string must be free'ed by the caller.
 * Returns NULL and reports an error if out-of-memory.
 */
static const char *
get_js_arg_types_as_string(JSContext *cx, uintN argc, jsval *argv)
{
    uintN i;
    const char *arg_type, *arg_string, *tmp;

    if (argc == 0)
        return strdup("()");
    
    arg_string = strdup("(");
    if (!arg_string)
        goto out_of_memory;
    for (i = 0; i < argc; i++) {
        arg_type = JS_GetTypeName(cx, JS_TypeOfValue(cx, argv[i]));
        tmp = PR_smprintf("%s%s%s%s", arg_string,  i ? ", " : "", arg_type,
                         (i == argc-1) ? ")" : "");
        free((char*)arg_string);
        if (!tmp)
            goto out_of_memory;
        arg_string = tmp;
    }

    return arg_string;

out_of_memory:
    JS_ReportOutOfMemory(cx);
    return NULL;
}

  
static void
report_method_match_failure(JSContext *cx,
                            JavaMemberDescriptor *member_descriptor,
                            JavaClassDescriptor *class_descriptor,
                            JSBool is_static_method,
                            uintN argc, jsval *argv)
{
    const char *err, *js_arg_string, *tmp, *method_str, *method_name;
    JSBool is_constructor;
    JavaMethodSpec *method;

    err = NULL;
    is_constructor = (!strcmp(member_descriptor->name, "<init>"));

    js_arg_string = get_js_arg_types_as_string(cx, argc, argv);
    if (!js_arg_string)
        goto out_of_memory;

    if (is_constructor) {
        err =  PR_smprintf("There is no Java constructor for class %s that matches "
                           "JavaScript argument types %s.\n", class_descriptor->name,
                           js_arg_string);
        method_name = class_descriptor->name;
    } else {
        err =  PR_smprintf("There is no %sJava method %s.%s that matches "
                           "JavaScript argument types %s.\n",
                           is_static_method ? "static ": "",
                           class_descriptor->name, member_descriptor->name, js_arg_string);
        method_name = member_descriptor->name;
    }
    if (!err)
        goto out_of_memory;

    tmp = PR_smprintf("%sCandidate methods with the same name are:\n", err);
    if (!tmp)
        goto out_of_memory;
    err = tmp;

    method = member_descriptor->methods;
    while (method) {
        method_str =
            jsj_ConvertJavaMethodSignatureToHRString(cx, method_name, &method->signature);
        if (!method_str)
            goto out_of_memory;
        tmp = PR_smprintf("%s   %s\n", err, method_str);
        free((char*)method_str);
        if (!tmp)
            goto out_of_memory;
        err = tmp;

        method = method->next;
    }
    
    JS_ReportError(cx, err);
    return;

out_of_memory:
    if (js_arg_string)
        free((char*)js_arg_string);
    if (err)
        free((char*)err);
}

static JavaMethodSpec *
resolve_overloaded_method(JSContext *cx, JNIEnv *jEnv, JavaMemberDescriptor *member_descriptor,
                          JavaClassDescriptor *class_descriptor,
                          JSBool is_static_method,
                          uintN argc, jsval *argv)
{
    int cost, lowest_cost, num_method_matches;
    JavaMethodSpec *best_method_match, *method;

    num_method_matches = 0;
    lowest_cost = 10000;
    best_method_match = NULL;

    for (method = member_descriptor->methods; method; method = method->next) {
        cost = 0;
        if (!method_signature_matches_JS_args(cx, jEnv, argc, argv, &method->signature, &cost))
            continue;

#if 1
        if (cost < lowest_cost) {
            lowest_cost = cost;
            best_method_match = method;
            num_method_matches++;
        }
#else
        best_method_match = method;
        break;
#endif
    }

    if (!best_method_match)
        report_method_match_failure(cx, member_descriptor, class_descriptor,
                                    is_static_method, argc, argv);

    /*
#ifdef LIVECONNECT_IMPROVEMENTS
    if (num_method_matches > 1)
        return NULL;
#else
    if (lowest_cost != 0)
        return NULL;
#endif
*/
    return best_method_match;
}

static jvalue *
convert_JS_method_args_to_java_argv(JSContext *cx, JNIEnv *jEnv, jsval *argv,
			            JavaMethodSpec *method, JSBool **localvp)
{
    jvalue *jargv;
    JSBool ok, *localv;
    uintN i, argc;
    JavaSignature **arg_signatures;
    JavaMethodSignature *signature;
    
    
    signature = &method->signature;
    argc = signature->num_args;
    PR_ASSERT(argc != 0);
    arg_signatures = signature->arg_signatures;
    
    jargv = (jvalue *)JS_malloc(cx, sizeof(jvalue) * argc);
    
    if (!jargv)
        return NULL;

    /*
     * Allocate an array that contains a flag for each argument, indicating whether
     * or not the conversion from a JS value to a Java value resulted in a new
     * JNI local reference.
     */
    localv = (JSBool *)JS_malloc(cx, sizeof(JSBool) * argc);
    *localvp = localv;
    if (!localv) {
        JS_free(cx, jargv);
        return NULL;
    }
 
    for (i = 0; i < argc; i++) {
        int dummy_cost;
        
        ok = jsj_ConvertJSValueToJavaValue(cx, jEnv, argv[i], arg_signatures[i],
                                           &dummy_cost, &jargv[i], &localv[i]);
        if (!ok) {
            JS_ReportError(cx, "Internal error: can't convert JS value to Java value");
            JS_free(cx, jargv);
            JS_free(cx, localv);
            *localvp = NULL;
            return NULL;
        }
    }

    return jargv;
}  

static JSBool
invoke_java_method(JSContext *cx, JSJavaThreadState *jsj_env,
                   jobject java_class_or_instance,
                   JavaClassDescriptor *class_descriptor,
		   JavaMethodSpec *method,
                   JSBool is_static_method,
		   jsval *argv, jsval *vp)
{
    jvalue java_value;
    jvalue *jargv;
    uintN argc, i;
    jobject java_object;
    jclass java_class;
    jmethodID methodID;
    JavaMethodSignature *signature;
    JavaSignature *return_val_signature;
    JSContext *old_cx;
    JNIEnv *jEnv;
    JSBool *localv, error_occurred;

    error_occurred = JS_FALSE;

    methodID = method->methodID;
    signature = &method->signature;
    argc = signature->num_args;

    jEnv = jsj_env->jEnv;

    if (is_static_method) {
        java_object = NULL;
        java_class = java_class_or_instance;
    } else {
        java_object = java_class_or_instance;
        java_class = NULL;
    }

    old_cx = JSJ_SetDefaultJSContextForJavaThread(cx, jsj_env);
    if (old_cx) {
        JS_ReportError(cx, "Java thread in simultaneous use by more than "
                           "one JSContext ?");
    }

    jargv = NULL;
    localv = NULL;
    if (argc) {
        jargv = convert_JS_method_args_to_java_argv(cx, jEnv, argv, method, &localv);
        if (!jargv) {
            error_occurred = JS_TRUE;
            goto out;
        }
    }

#define CALL_JAVA_METHOD(type, member)                                       \
    PR_BEGIN_MACRO                                                           \
    if (is_static_method) {                                                  \
        java_value.member = (*jEnv)->CallStatic##type##MethodA(jEnv, java_class, methodID, jargv);\
    } else {                                                                 \
        java_value.member = (*jEnv)->Call##type##MethodA(jEnv, java_object, methodID, jargv);\
    }                                                                        \
    if ((*jEnv)->ExceptionOccurred(jEnv)) {                                  \
        jsj_ReportJavaError(cx, jEnv, "Error calling method %s.%s()",        \
                            class_descriptor->name, method->name);           \
        error_occurred = JS_TRUE;                                            \
        goto out;                                                            \
    }                                                                        \
    PR_END_MACRO

    return_val_signature = signature->return_val_signature;
    switch(return_val_signature->type) {
    case JAVA_SIGNATURE_BYTE:
        CALL_JAVA_METHOD(Byte, b);
        break;

    case JAVA_SIGNATURE_CHAR:
        CALL_JAVA_METHOD(Char, c);
        break;
    
    case JAVA_SIGNATURE_FLOAT:
        CALL_JAVA_METHOD(Float, f);
        break;
    
    case JAVA_SIGNATURE_DOUBLE:
        CALL_JAVA_METHOD(Double, d);
        break;
    
    case JAVA_SIGNATURE_INT:
        CALL_JAVA_METHOD(Int, i);
        break;
    
    case JAVA_SIGNATURE_LONG:
        CALL_JAVA_METHOD(Long, j);
        break;
    
    case JAVA_SIGNATURE_SHORT:
        CALL_JAVA_METHOD(Short, s);
        break;
    
    case JAVA_SIGNATURE_BOOLEAN:
        CALL_JAVA_METHOD(Boolean, z);
        break;
    
    case JAVA_SIGNATURE_ARRAY:
    case JAVA_SIGNATURE_CLASS:
        CALL_JAVA_METHOD(Object, l);
        break;
    
    case JAVA_SIGNATURE_VOID:
        if (is_static_method)
            (*jEnv)->CallStaticVoidMethodA(jEnv, java_class, methodID, jargv);
        else
            (*jEnv)->CallVoidMethodA(jEnv, java_object, methodID, jargv);
        if ((*jEnv)->ExceptionOccurred(jEnv)) {
            jsj_ReportJavaError(cx, jEnv, "Error calling method %s.%s()",
                                class_descriptor->name, method->name);
            error_occurred = JS_TRUE;
            goto out;
        }
        break;
        
    default:
        PR_ASSERT(0);
        return JS_FALSE;
    }

out:
    JSJ_SetDefaultJSContextForJavaThread(old_cx, jsj_env);

    if (localv) {
        for (i = 0; i < argc; i++) {
            if (localv[i])
                (*jEnv)->DeleteLocalRef(jEnv, jargv[i].l);
        }
        JS_free(cx, localv);
    }
    if (jargv)
       JS_free(cx, jargv);

    if (error_occurred)
        return JS_FALSE;
    else
        return jsj_ConvertJavaValueToJSValue(cx, jEnv, return_val_signature, &java_value, vp);
}

static JSBool
invoke_overloaded_java_method(JSContext *cx, JSJavaThreadState *jsj_env,
                              JavaMemberDescriptor *member,
                              JSBool is_static_method,
                              jobject java_class_or_instance,
                              JavaClassDescriptor *class_descriptor,
                              uintN argc, jsval *argv,
                              jsval *vp)
{
    JavaMethodSpec *method;
    JNIEnv *jEnv;

    jEnv = jsj_env->jEnv;

    method = resolve_overloaded_method(cx, jEnv, member, class_descriptor,
                                       is_static_method, argc, argv);
    if (!method)
        return JS_FALSE;

    return invoke_java_method(cx, jsj_env, java_class_or_instance, class_descriptor, 
                              method, is_static_method, argv, vp);
}

PR_CALLBACK JSBool
jsj_JavaStaticMethodWrapper(JSContext *cx, JSObject *obj,
                            uintN argc, jsval *argv, jsval *vp)
{
    JSFunction *function;
    JavaMemberDescriptor *member_descriptor;
    JavaClassDescriptor *class_descriptor;
    jsid id;
    jsval idval;
    JNIEnv *jEnv;
    JSJavaThreadState *jsj_env;
    jobject java_class;

    /* Get the Java per-thread environment pointer for this JSContext */
    jsj_env = jsj_MapJSContextToJSJThread(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;
    
    class_descriptor = JS_GetPrivate(cx, obj);
    if (!class_descriptor)
        return JS_FALSE;
    java_class = class_descriptor->java_class;
    
    PR_ASSERT(JS_TypeOfValue(cx, argv[-2]) == JSTYPE_FUNCTION);
    function = JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[-2]));
    idval = STRING_TO_JSVAL(JS_InternString(cx, JS_GetFunctionName(function)));
    JS_ValueToId(cx, idval, &id);
    
    member_descriptor = jsj_LookupJavaStaticMemberDescriptorById(cx, jEnv, class_descriptor, id);
    if (!member_descriptor) {
        PR_ASSERT(0);
        return JS_FALSE;
    }

    return invoke_overloaded_java_method(cx, jsj_env, member_descriptor, JS_TRUE, 
                                         java_class, class_descriptor, argc, argv, vp);
}

PR_CALLBACK JSBool
jsj_JavaInstanceMethodWrapper(JSContext *cx, JSObject *obj,
                              uintN argc, jsval *argv, jsval *vp)
{
    JSFunction *function;
    JavaMemberDescriptor *member_descriptor;
    JavaObjectWrapper *java_wrapper;
    JavaClassDescriptor *class_descriptor;
    jsint id;
    jsval idval;
    JSJavaThreadState *jsj_env;
    JNIEnv *jEnv;
    jobject java_obj, java_class;

    /* Get the Java per-thread environment pointer for this JSContext */
    jsj_env = jsj_MapJSContextToJSJThread(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;
    
    java_wrapper = JS_GetPrivate(cx, obj);
    if (!java_wrapper)
        return JS_FALSE;
    java_obj = java_wrapper->java_obj;
    
    PR_ASSERT(JS_TypeOfValue(cx, argv[-2]) == JSTYPE_FUNCTION);
    function = JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[-2]));
    idval = STRING_TO_JSVAL(JS_InternString(cx, JS_GetFunctionName(function)));
    JS_ValueToId(cx, idval, &id);
    class_descriptor = java_wrapper->class_descriptor;
    member_descriptor = jsj_LookupJavaMemberDescriptorById(cx, jEnv, class_descriptor, id);

    /* If no instance method was found, try for a static method */
    if (!member_descriptor) {
        member_descriptor = jsj_LookupJavaStaticMemberDescriptorById(cx, jEnv, class_descriptor, id);
        if (!member_descriptor) {
            PR_ASSERT(0);
            return JS_FALSE;
        }
        java_class = class_descriptor->java_class;
        return invoke_overloaded_java_method(cx, jsj_env, member_descriptor, JS_TRUE, 
                                             java_class, class_descriptor, argc, argv, vp);
    }
    
    return invoke_overloaded_java_method(cx, jsj_env, member_descriptor,
                                         JS_FALSE, java_obj, 
                                         class_descriptor, argc, argv, vp);
}

static JSBool
invoke_java_constructor(JSContext *cx,
                        JSJavaThreadState *jsj_env,
                        jclass java_class,
		        JavaMethodSpec *method,
		        jsval *argv, jsval *vp)
{
    jvalue *jargv;
    uintN argc, i;
    jobject java_object;
    jmethodID methodID;
    JavaMethodSignature *signature;
    JSContext *old_cx;
    JNIEnv *jEnv;
    JSBool *localv;
    JSBool error_occurred = JS_FALSE;
    
    methodID = method->methodID;
    signature = &method->signature;
    argc = signature->num_args;

    jEnv = jsj_env->jEnv;

    jargv = NULL;
    localv = NULL;
    if (argc) {
        jargv = convert_JS_method_args_to_java_argv(cx, jEnv, argv, method, &localv);
        if (!jargv) {
            error_occurred = JS_TRUE;
            goto out;
        }
    }

    old_cx = JSJ_SetDefaultJSContextForJavaThread(cx, jsj_env);
    if (old_cx) {
        JS_ReportError(cx, "Java thread in simultaneous use by more than "
                           "one JSContext ?");
    }

    /* Call the constructor */
    java_object = (*jEnv)->NewObjectA(jEnv, java_class, methodID, jargv);

    JSJ_SetDefaultJSContextForJavaThread(old_cx, jsj_env);

    if (!java_object) {
        jsj_ReportJavaError(cx, jEnv, "Error while constructing instance of %s",
                            jsj_GetJavaClassName(cx, jEnv, java_class));
        error_occurred = JS_TRUE;
        goto out;
    }

out:
    for (i = 0; i < argc; i++) {
        if (localv[i])
            (*jEnv)->DeleteLocalRef(jEnv, jargv[i].l);
    }
    if (jargv)
       JS_free(cx, jargv);
    if (localv)
        JS_free(cx, localv);
    if (error_occurred)
        return JS_FALSE;
    else
        return jsj_ConvertJavaObjectToJSValue(cx, jEnv, java_object, vp);
}

static JSBool
invoke_overloaded_java_constructor(JSContext *cx,
                                   JSJavaThreadState *jsj_env,
                                   JavaMemberDescriptor *member,
                                   JSObject *obj, uintN argc, jsval *argv,
                                   jsval *vp)
{
    jclass java_class;
    JavaMethodSpec *method;
    JavaClassDescriptor *class_descriptor;
    JNIEnv *jEnv;

    jEnv = jsj_env->jEnv;

    class_descriptor = JS_GetPrivate(cx, obj);
    PR_ASSERT(class_descriptor);
    if (!class_descriptor)
        return JS_FALSE;


    method = resolve_overloaded_method(cx, jEnv, member, class_descriptor, JS_TRUE, 
                                       argc, argv);
    if (!method)
        return JS_FALSE;

    java_class = class_descriptor->java_class;
    return invoke_java_constructor(cx, jsj_env, java_class, method, argv, vp);
}


PR_CALLBACK JSBool
jsj_JavaConstructorWrapper(JSContext *cx, JSObject *obj,
                           uintN argc, jsval *argv, jsval *vp)
{
    jint modifiers;
    JavaMemberDescriptor *member_descriptor;
    JavaClassDescriptor *class_descriptor;
    JSJavaThreadState *jsj_env;
    JNIEnv *jEnv;

    /* Get the Java per-thread environment pointer for this JSContext */
    jsj_env = jsj_MapJSContextToJSJThread(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;
    
    obj = JSVAL_TO_OBJECT(argv[-2]);
    class_descriptor = JS_GetPrivate(cx, obj);
    if (!class_descriptor)
        return JS_FALSE;

    /* Get class/interface flags and check them */
    modifiers = class_descriptor->modifiers;
    if (modifiers & ACC_ABSTRACT) {
        JS_ReportError(cx, "Java class %s is abstract and therefore may not "
                           "be instantiated", class_descriptor->name);
        return JS_FALSE;
    }
    if (modifiers & ACC_INTERFACE) {
        JS_ReportError(cx, "%s is a Java interface and therefore may not "
                           "be instantiated", class_descriptor->name);
        return JS_FALSE;
    }
    if ( !(modifiers & ACC_PUBLIC) ) {
        JS_ReportError(cx, "Java class %s is not public and therefore may not "
                           "be instantiated", class_descriptor->name);
        return JS_FALSE;
    }
    
    member_descriptor = jsj_LookupJavaClassConstructors(cx, jEnv, class_descriptor);
    if (!member_descriptor) {
        JS_ReportError(cx, "No public constructors defined for Java class %s",
                       class_descriptor->name);
        return JS_FALSE;
    }

    return invoke_overloaded_java_constructor(cx, jsj_env, member_descriptor, obj, argc, argv, vp);
}

