/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* The convenience functions below present a complete, but simplified
   LiveConnect API which is designed to handle the special case of a single 
   Java-VM, single-threaded operation, and use of only one JSContext. */

#include "jsjava.h"
#include "jsprf.h"
#include "jsutil.h"

/* We can get away with global variables in our single-threaded,
   single-JSContext case. */
static JSJavaVM *           the_jsj_vm = NULL;
static JSContext *          the_cx = NULL;
static JSJavaThreadState *  the_jsj_thread = NULL;
static JSObject *           the_global_js_obj = NULL;

/* Trivial implementation of callback function */
static JSJavaThreadState *
default_map_js_context_to_jsj_thread(JSContext *cx, char **errp)
{
    return the_jsj_thread;
}

/* Trivial implementation of callback function */
static JSContext *
default_map_jsj_thread_to_js_context(JSJavaThreadState *jsj_env, JNIEnv *jEnv, char **errp)
{
    return the_cx;
}

/* Trivial implementation of callback function */
static JSObject *
default_map_java_object_to_js_object(JNIEnv *jEnv, void *hint, char **errp)
{
    return the_global_js_obj;
}

static JSBool JS_DLL_CALLBACK
default_create_java_vm(SystemJavaVM* *jvm, JNIEnv* *initialEnv, void* initargs)
{
    jint err;
    const char* user_classpath = (const char*)initargs;

    /* No Java VM supplied, so create our own */
    JDK1_1InitArgs vm_args;
        
    /* Magic constant indicates JRE version 1.1 */
    vm_args.version = 0x00010001;
    JNI_GetDefaultJavaVMInitArgs(&vm_args);
        
    /* Prepend the classpath argument to the default JVM classpath */
    if (user_classpath) {
#ifdef XP_UNIX
        const char *full_classpath = JS_smprintf("%s:%s", user_classpath, vm_args.classpath);
#else
        const char *full_classpath = JS_smprintf("%s;%s", user_classpath, vm_args.classpath);
#endif
        if (!full_classpath) {
            return JS_FALSE;
        }
        vm_args.classpath = (char*)full_classpath;
    }

    err = JNI_CreateJavaVM((JavaVM**)jvm, initialEnv, &vm_args);
    return err == 0;
}

static JSBool JS_DLL_CALLBACK
default_destroy_java_vm(SystemJavaVM* jvm, JNIEnv* initialEnv)
{
    JavaVM* java_vm = (JavaVM*)jvm;
    jint err = (*java_vm)->DestroyJavaVM(java_vm);
    return err == 0;
}

static JNIEnv* JS_DLL_CALLBACK
default_attach_current_thread(SystemJavaVM* jvm)
{
    JavaVM* java_vm = (JavaVM*)jvm;
    JNIEnv* env = NULL;
    (*java_vm)->AttachCurrentThread(java_vm, &env, NULL);
    return env;
}

static JSBool JS_DLL_CALLBACK
default_detach_current_thread(SystemJavaVM* jvm, JNIEnv* env)
{
    JavaVM* java_vm = (JavaVM*)jvm;
    /* assert that env is the JNIEnv of the current thread */
    jint err = (*java_vm)->DetachCurrentThread(java_vm);
    return err == 0;
}

static SystemJavaVM* JS_DLL_CALLBACK
default_get_java_vm(JNIEnv* env)
{
    JavaVM* java_vm = NULL;
    (*env)->GetJavaVM(env, &java_vm);
    return (SystemJavaVM*)java_vm;
}

/* Trivial implementations of callback functions */
JSJCallbacks jsj_default_callbacks = {
    default_map_jsj_thread_to_js_context,
    default_map_js_context_to_jsj_thread,
    default_map_java_object_to_js_object,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    default_create_java_vm,
    default_destroy_java_vm,
    default_attach_current_thread,
    default_detach_current_thread,
    default_get_java_vm
};

/*
 * Initialize the provided JSContext by setting up the JS classes necessary for
 * reflection and by defining JavaPackage objects for the default Java packages
 * as properties of global_obj.  If java_vm is NULL, a new Java VM is
 * created, using the provided classpath in addition to any default classpath.
 * The classpath argument is ignored, however, if java_vm is non-NULL.
 */
JSBool
JSJ_SimpleInit(JSContext *cx, JSObject *global_obj, SystemJavaVM *java_vm, const char *classpath)
{
    JNIEnv *jEnv;

    JSJ_Init(&jsj_default_callbacks);

    JS_ASSERT(!the_jsj_vm);
    the_jsj_vm = JSJ_ConnectToJavaVM(java_vm, (void*)classpath);
    if (!the_jsj_vm)
        return JS_FALSE;

    if (!JSJ_InitJSContext(cx, global_obj, NULL))
        goto error;
    the_cx = cx;
    the_global_js_obj = global_obj;

    the_jsj_thread = JSJ_AttachCurrentThreadToJava(the_jsj_vm, "main thread", &jEnv);
    if (!the_jsj_thread)
        goto error;
    JSJ_SetDefaultJSContextForJavaThread(cx, the_jsj_thread);
    return JS_TRUE;

error:
    JSJ_SimpleShutdown();
    return JS_FALSE;
}

/*
 * Free up all LiveConnect resources.  Destroy the Java VM if it was
 * created by LiveConnect.
 */
JS_EXPORT_API(void)
JSJ_SimpleShutdown()
{
    JS_ASSERT(the_jsj_vm);
    JSJ_DisconnectFromJavaVM(the_jsj_vm);
    the_jsj_vm = NULL;
    the_cx = NULL;
    the_global_js_obj = NULL;
    the_jsj_thread = NULL;
}
