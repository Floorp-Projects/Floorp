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
 * It contains the top-level initialization code and the implementation of the
 * public API.
 *
 */

#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "prassert.h"
#include "prprintf.h"
#include "prosdep.h"

#include "jsj_private.h"        /* LiveConnect internals */
#include "jsjava.h"             /* LiveConnect external API */

/*
 * At certain times during initialization, there may be no JavaScript context
 * available to direct error reports to, in which case the error messages
 * are sent to this function.  The caller is responsible for free'ing
 * the js_error_msg argument.
 */
static void
report_java_initialization_error(JNIEnv *jEnv, const char *js_error_msg)
{
    const char *error_msg, *java_error_msg;

    java_error_msg = NULL;

    if (jEnv) {
        java_error_msg = jsj_GetJavaErrorMessage(jEnv);
        (*jEnv)->ExceptionClear(jEnv);
    }

    if (java_error_msg) { 
        error_msg = PR_smprintf("initialization error: %s (%s)\n",
                                js_error_msg, java_error_msg);
        free((void*)java_error_msg);
    } else {
        error_msg = PR_smprintf("initialization error: %s\n",
                                js_error_msg);
    }

    jsj_LogError(error_msg);
}

/*
 * Opaque JVM handles to Java classes and methods required for Java reflection.
 * These are computed and cached during initialization.
 */

jclass jlObject;                        /* java.lang.Object */
jclass jlrMethod;                       /* java.lang.reflect.Method */
jclass jlrField;                        /* java.lang.reflect.Field */
jclass jlVoid;                          /* java.lang.Void */
jclass jlrConstructor;                  /* java.lang.reflect.Constructor */
jclass jlThrowable;                     /* java.lang.Throwable */
jclass jlSystem;                        /* java.lang.System */
jclass jlClass;                         /* java.lang.Class */
jclass jlBoolean;                       /* java.lang.Boolean */
jclass jlDouble;                        /* java.lang.Double */
jclass jlString;                        /* java.lang.String */
jclass njJSObject;                      /* netscape.javascript.JSObject */
jclass njJSException;                   /* netscape.javascript.JSException */
jclass njJSUtil;                        /* netscape.javascript.JSUtil */

jmethodID jlClass_getMethods;           /* java.lang.Class.getMethods() */
jmethodID jlClass_getConstructors;      /* java.lang.Class.getConstructors() */
jmethodID jlClass_getFields;            /* java.lang.Class.getFields() */
jmethodID jlClass_getName;              /* java.lang.Class.getName() */
jmethodID jlClass_getComponentType;     /* java.lang.Class.getComponentType() */
jmethodID jlClass_getModifiers;         /* java.lang.Class.getModifiers() */
jmethodID jlClass_isArray;              /* java.lang.Class.isArray() */

jmethodID jlrMethod_getName;            /* java.lang.reflect.Method.getName() */
jmethodID jlrMethod_getParameterTypes;  /* java.lang.reflect.Method.getParameterTypes() */
jmethodID jlrMethod_getReturnType;      /* java.lang.reflect.Method.getReturnType() */
jmethodID jlrMethod_getModifiers;       /* java.lang.reflect.Method.getModifiers() */

jmethodID jlrConstructor_getParameterTypes; /* java.lang.reflect.Constructor.getParameterTypes() */
jmethodID jlrConstructor_getModifiers;  /* java.lang.reflect.Constructor.getModifiers() */

jmethodID jlrField_getName;             /* java.lang.reflect.Field.getName() */
jmethodID jlrField_getType;             /* java.lang.reflect.Field.getType() */
jmethodID jlrField_getModifiers;        /* java.lang.reflect.Field.getModifiers() */

jmethodID jlBoolean_Boolean;            /* java.lang.Boolean constructor */
jmethodID jlBoolean_booleanValue;       /* java.lang.Boolean.booleanValue() */
jmethodID jlDouble_Double;              /* java.lang.Double constructor */
jmethodID jlDouble_doubleValue;         /* java.lang.Double.doubleValue() */

jmethodID jlThrowable_toString;         /* java.lang.Throwable.toString() */
jmethodID jlThrowable_getMessage;       /* java.lang.Throwable.getMessage() */

jmethodID jlSystem_identityHashCode;    /* java.lang.System.identityHashCode() */

jobject jlVoid_TYPE;                    /* java.lang.Void.TYPE value */

jmethodID njJSException_JSException;    /* netscape.javascript.JSexception constructor */
jmethodID njJSObject_JSObject;          /* netscape.javascript.JSObject constructor */
jmethodID njJSUtil_getStackTrace;       /* netscape.javascript.JSUtil.getStackTrace() */
jfieldID njJSObject_internal;           /* netscape.javascript.JSObject.internal */
jfieldID njJSException_lineno;          /* netscape.javascript.JSException.lineno */
jfieldID njJSException_tokenIndex;      /* netscape.javascript.JSException.tokenIndex */
jfieldID njJSException_source;          /* netscape.javascript.JSException.source */
jfieldID njJSException_filename;        /* netscape.javascript.JSException.filename */

/* Obtain a reference to a Java class */
#define LOAD_CLASS(qualified_name, class)                                    \
    {                                                                        \
        jclass _##class = (*jEnv)->FindClass(jEnv, #qualified_name);         \
        if (_##class == 0) {                                                 \
            report_java_initialization_error(jEnv,                           \
                "Can't load class " #qualified_name);                        \
            return JS_FALSE;                                                 \
        }                                                                    \
        class = (*jEnv)->NewGlobalRef(jEnv, _##class);                       \
    }

/* Obtain a methodID reference to a Java method or constructor */
#define _LOAD_METHOD(qualified_class, method, mvar, signature, class, is_static)\
    if (is_static) {                                                         \
        class##_##mvar =                                                     \
            (*jEnv)->GetStaticMethodID(jEnv, class, #method, signature);     \
    } else {                                                                 \
        class##_##mvar =                                                     \
            (*jEnv)->GetMethodID(jEnv, class, #method, signature);           \
    }                                                                        \
    if (class##_##mvar == 0) {                                               \
        report_java_initialization_error(jEnv,                               \
               "Can't get mid for " #qualified_class "." #method "()");      \
        return JS_FALSE;                                                     \
    }

/* Obtain a methodID reference to a Java instance method */
#define LOAD_METHOD(qualified_class, method, signature, class)               \
    _LOAD_METHOD(qualified_class, method, method, signature, class, JS_FALSE)

/* Obtain a methodID reference to a Java static method */
#define LOAD_STATIC_METHOD(qualified_class, method, signature, class)        \
    _LOAD_METHOD(qualified_class, method, method, signature, class, JS_TRUE)

/* Obtain a methodID reference to a Java constructor */
#define LOAD_CONSTRUCTOR(qualified_class, method, signature, class)          \
    _LOAD_METHOD(qualified_class,<init>, method, signature, class, JS_FALSE)

/* Obtain a fieldID reference to a Java instance or static field */
#define _LOAD_FIELDID(qualified_class, field, signature, class, is_static)   \
    if (is_static) {                                                         \
        class##_##field = (*jEnv)->GetStaticFieldID(jEnv, class, #field, signature);\
    } else {                                                                 \
        class##_##field = (*jEnv)->GetFieldID(jEnv, class, #field, signature);\
    }                                                                        \
    if (class##_##field == 0) {                                              \
        report_java_initialization_error(jEnv,                               \
                "Can't get fid for " #qualified_class "." #field);           \
        return JS_FALSE;                                                     \
    }

/* Obtain a fieldID reference to a Java instance field */
#define LOAD_FIELDID(qualified_class, field, signature, class)               \
    _LOAD_FIELDID(qualified_class, field, signature, class, JS_FALSE)

/* Obtain the value of a static field in a Java class */
#define LOAD_FIELD_VAL(qualified_class, field, signature, class, type)       \
    {                                                                        \
        jfieldID field_id;                                                   \
        field_id = (*jEnv)->GetStaticFieldID(jEnv, class, #field, signature);\
        if (field_id == 0) {                                                 \
            report_java_initialization_error(jEnv,                           \
                "Can't get fid for " #qualified_class "." #field);           \
            return JS_FALSE;                                                 \
        }                                                                    \
        class##_##field =                                                    \
            (*jEnv)->GetStatic##type##Field(jEnv, class, field_id);          \
        if (class##_##field == 0) {                                          \
            report_java_initialization_error(jEnv,                           \
                "Can't read static field " #qualified_class "." #field);     \
            return JS_FALSE;                                                 \
        }                                                                    \
    }

/* Obtain the value of a static field in a Java class, which is known to
   contain an object value. */
#define LOAD_FIELD_OBJ(qualified_class, field, signature, class)             \
    LOAD_FIELD_VAL(qualified_class, field, signature, class, Object);        \
    class##_##field = (*jEnv)->NewGlobalRef(jEnv, class##_##field);

/*
 * Load the Java classes, and the method and field descriptors required for Java reflection.
 * Returns JS_TRUE on success, JS_FALSE on failure.
 */
static JSBool
init_java_VM_reflection(JSJavaVM *jsjava_vm, JNIEnv *jEnv)
{
    /* Load Java system classes and method, including java.lang.reflect classes */
    LOAD_CLASS(java/lang/Object,                jlObject);
    LOAD_CLASS(java/lang/Class,                 jlClass);
    LOAD_CLASS(java/lang/reflect/Method,        jlrMethod);
    LOAD_CLASS(java/lang/reflect/Constructor,   jlrConstructor);
    LOAD_CLASS(java/lang/reflect/Field,         jlrField);
    LOAD_CLASS(java/lang/Throwable,             jlThrowable);
    LOAD_CLASS(java/lang/System,                jlSystem);
    LOAD_CLASS(java/lang/Boolean,               jlBoolean);
    LOAD_CLASS(java/lang/Double,                jlDouble);
    LOAD_CLASS(java/lang/String,                jlString);
    LOAD_CLASS(java/lang/Void,                  jlVoid);

    LOAD_METHOD(java.lang.Class,            getMethods,         "()[Ljava/lang/reflect/Method;",jlClass);
    LOAD_METHOD(java.lang.Class,            getConstructors,    "()[Ljava/lang/reflect/Constructor;",jlClass);
    LOAD_METHOD(java.lang.Class,            getFields,          "()[Ljava/lang/reflect/Field;", jlClass);
    LOAD_METHOD(java.lang.Class,            getName,            "()Ljava/lang/String;",         jlClass);
    LOAD_METHOD(java.lang.Class,            isArray,            "()Z",                          jlClass);
    LOAD_METHOD(java.lang.Class,            getComponentType,   "()Ljava/lang/Class;",          jlClass);
    LOAD_METHOD(java.lang.Class,            getModifiers,       "()I",                          jlClass);

    LOAD_METHOD(java.lang.reflect.Method,   getName,            "()Ljava/lang/String;",         jlrMethod);
    LOAD_METHOD(java.lang.reflect.Method,   getParameterTypes,  "()[Ljava/lang/Class;",         jlrMethod);
    LOAD_METHOD(java.lang.reflect.Method,   getReturnType,      "()Ljava/lang/Class;",          jlrMethod);
    LOAD_METHOD(java.lang.reflect.Method,   getModifiers,       "()I",                          jlrMethod);

    LOAD_METHOD(java.lang.reflect.Constructor,  getParameterTypes,  "()[Ljava/lang/Class;",     jlrConstructor);
    LOAD_METHOD(java.lang.reflect.Constructor,  getModifiers,       "()I",                      jlrConstructor);
    
    LOAD_METHOD(java.lang.reflect.Field,    getName,            "()Ljava/lang/String;",         jlrField);
    LOAD_METHOD(java.lang.reflect.Field,    getType,            "()Ljava/lang/Class;",          jlrField);
    LOAD_METHOD(java.lang.reflect.Field,    getModifiers,       "()I",                          jlrField);

    LOAD_METHOD(java.lang.Throwable,        toString,           "()Ljava/lang/String;",         jlThrowable);
    LOAD_METHOD(java.lang.Throwable,        getMessage,         "()Ljava/lang/String;",         jlThrowable);

    LOAD_METHOD(java.lang.Double,           doubleValue,        "()D",                          jlDouble);

    LOAD_METHOD(java.lang.Boolean,          booleanValue,       "()Z",                          jlBoolean);
    
    LOAD_STATIC_METHOD(java.lang.System,    identityHashCode,   "(Ljava/lang/Object;)I",        jlSystem);

    LOAD_CONSTRUCTOR(java.lang.Boolean,     Boolean,            "(Z)V",                         jlBoolean);
    LOAD_CONSTRUCTOR(java.lang.Double,      Double,             "(D)V",                         jlDouble);

    LOAD_FIELD_OBJ(java.lang.Void,          TYPE,               "Ljava/lang/Class;",            jlVoid);
  
}

/* Load Netscape-specific Java extension classes, methods, and fields */
static JSBool
init_netscape_java_classes(JSJavaVM *jsjava_vm, JNIEnv *jEnv)
{
    LOAD_CLASS(netscape/javascript/JSObject,    njJSObject);
    LOAD_CLASS(netscape/javascript/JSException, njJSException);
    LOAD_CLASS(netscape/javascript/JSUtil,      njJSUtil);

    LOAD_CONSTRUCTOR(netscape.javascript.JSObject,
                                            JSObject,           "(I)V",                         njJSObject);
    LOAD_CONSTRUCTOR(netscape.javascript.JSException,
                                            JSException,        "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;I)V",
                                                                                                njJSException);
    LOAD_FIELDID(netscape.javascript.JSObject,  
                                            internal,           "I",                            njJSObject);
    LOAD_FIELDID(netscape.javascript.JSException,  
                                            lineno,             "I",                            njJSException);
    LOAD_FIELDID(netscape.javascript.JSException,  
                                            tokenIndex,         "I",                            njJSException);
    LOAD_FIELDID(netscape.javascript.JSException,  
                                            source,             "Ljava/lang/String;",           njJSException);
    LOAD_FIELDID(netscape.javascript.JSException,  
                                            filename,           "Ljava/lang/String;",           njJSException);

    LOAD_STATIC_METHOD(netscape.javascript.JSUtil,
                                            getStackTrace,      "(Ljava/lang/Throwable;)Ljava/lang/String;",
                                                                                                njJSUtil);

    return JS_TRUE;
}

JSJavaVM *jsjava_vm_list = NULL;

/*
 * Called once per Java VM, this function initializes the classes, fields, and
 * methods required for Java reflection.  If java_vm is NULL, a new Java VM is
 * created, using the provided classpath in addition to any default classpath.
 * The classpath argument is ignored, however, if java_vm_arg is non-NULL.
 */
JSJavaVM *
JSJ_ConnectToJavaVM(JavaVM *java_vm_arg, const char *user_classpath)
{
    JavaVM *java_vm;
    JSJavaVM *jsjava_vm;
    const char *full_classpath;
    JNIEnv *jEnv;

    jsjava_vm = (JSJavaVM*)malloc(sizeof(JSJavaVM));
    if (!jsjava_vm)
        return NULL;
    memset(jsjava_vm, 0, sizeof(JSJavaVM));

    java_vm = java_vm_arg;

    /* If a Java VM was passed in, try to attach to it on the current thread. */
    if (java_vm) {
        if ((*java_vm)->AttachCurrentThread(java_vm, &jEnv, NULL) < 0) {
            jsj_LogError("Failed to attach to Java VM thread\n");
            free(jsjava_vm);
            return NULL;
        }
    } else {
        /* No Java VM supplied, so create our own */
        JDK1_1InitArgs vm_args;
        
        /* Magic constant indicates JRE version 1.1 */
        vm_args.version = 0x00010001;
        JNI_GetDefaultJavaVMInitArgs(&vm_args);
        
        /* Prepend the classpath argument to the default JVM classpath */
        if (user_classpath) {
            full_classpath = PR_smprintf("%s;%s", user_classpath, vm_args.classpath);
            if (!full_classpath) {
                free(jsjava_vm);
                return NULL;
            }
            vm_args.classpath = (char*)full_classpath;
        }

        /* Attempt to create our own VM */
        if (JNI_CreateJavaVM(&java_vm, &jEnv, &vm_args) < 0) {
            jsj_LogError("Failed to create Java VM\n");
            free(jsjava_vm);
            return NULL;
        }

        /* Remember that we created the VM so that we know to destroy it later */
        jsjava_vm->jsj_created_java_vm = JS_TRUE;
    }
    jsjava_vm->java_vm = java_vm;
    jsjava_vm->main_thread_env = jEnv;
    
    /* Load the Java classes, and the method and field descriptors required for
       Java reflection. */
    if (!init_java_VM_reflection(jsjava_vm, jEnv)) {
        JSJ_DisconnectFromJavaVM(jsjava_vm);
        return NULL;
    }

    /*
     * JVM initialization for netscape.javascript.JSObject is performed
     * independently of the other classes that are initialized in
     * init_java_VM_reflection, because we allow it to fail.  In the case
     * of failure, LiveConnect is still operative, but only when calling
     * from JS to Java and not vice-versa.
     */
    init_netscape_java_classes(jsjava_vm, jEnv);

    /* Put this VM on the list of all created VMs */
    jsjava_vm->next = jsjava_vm_list;
    jsjava_vm_list = jsjava_vm;

    return jsjava_vm;
}

JSJCallbacks *JSJ_callbacks = NULL;

/* Called once to set up callbacks for all instances of LiveConnect */
void
JSJ_Init(JSJCallbacks *callbacks)
{
    PR_ASSERT(callbacks);
    JSJ_callbacks = callbacks;
}

/*
 * Initialize the provided JSContext by setting up the JS classes necessary for
 * reflection and by defining JavaPackage objects for the default Java packages
 * as properties of global_obj.  Additional packages may be pre-defined by
 * setting the predefined_packages argument.  (Pre-defining a Java package at
 * initialization time is not necessary, but it will make package lookup faster
 * and, more importantly, will avoid unnecessary network accesses if classes
 * are being loaded over the network.)
 */
JSBool
JSJ_InitJSContext(JSContext *cx, JSObject *global_obj,
                  JavaPackageDef *predefined_packages)
{
    /* Initialize the JavaScript classes used for reflection */
    if (!jsj_init_JavaObject(cx, global_obj))
        return JS_FALSE;
    
/*    if (!jsj_init_JavaMember(cx, global_obj))
        return JS_FALSE; */
    
    if (!jsj_init_JavaPackage(cx, global_obj, predefined_packages))
        return JS_FALSE;

    if (!jsj_init_JavaClass(cx, global_obj))
        return JS_FALSE;

    if (!jsj_init_JavaArray(cx, global_obj))
        return JS_FALSE;

    return JS_TRUE;
}

/* Eliminate a reference to a Java class */
#define UNLOAD_CLASS(qualified_name, class)                                  \
    if (class) {                                                             \
        (*jEnv)->DeleteGlobalRef(jEnv, class);                               \
        class = NULL;                                                        \
    }

/*
 * This routine severs the connection to a Java VM, freeing all related resources.
 * It shouldn't be called until the global scope has been cleared in all related
 * JSContexts (so that all LiveConnect objects are finalized) and a JavaScript
 * GC is performed.  Otherwise, accessed to free'ed memory could result.
 */
void
JSJ_DisconnectFromJavaVM(JSJavaVM *jsjava_vm)
{
    JNIEnv *jEnv;
    JavaVM *java_vm;
    
    java_vm = jsjava_vm->java_vm;
    (*java_vm)->AttachCurrentThread(java_vm, &jEnv, NULL);

    /* Drop all references to Java objects and classes */
    jsj_DiscardJavaObjReflections(jEnv);
    jsj_DiscardJavaClassReflections(jEnv);

    if (jsjava_vm->jsj_created_java_vm) { 
        (*java_vm)->DestroyJavaVM(java_vm);
    } else {
        UNLOAD_CLASS(java/lang/Object,                jlObject);
        UNLOAD_CLASS(java/lang/Class,                 jlClass);
        UNLOAD_CLASS(java/lang/reflect/Method,        jlrMethod);
        UNLOAD_CLASS(java/lang/reflect/Constructor,   jlrConstructor);
        UNLOAD_CLASS(java/lang/reflect/Field,         jlrField);
        UNLOAD_CLASS(java/lang/Throwable,             jlThrowable);
        UNLOAD_CLASS(java/lang/System,                jlSystem);
        UNLOAD_CLASS(java/lang/Boolean,               jlBoolean);
        UNLOAD_CLASS(java/lang/Double,                jlDouble);
        UNLOAD_CLASS(java/lang/String,                jlString);
        UNLOAD_CLASS(java/lang/Void,                  jlVoid);
        UNLOAD_CLASS(netscape/javascript/JSObject,    njJSObject);
        UNLOAD_CLASS(netscape/javascript/JSException, njJSException);
        UNLOAD_CLASS(netscape/javascript/JSUtil,      njJSUtil);
    }
}

static JSJavaThreadState *thread_list = NULL;

static JSJavaThreadState *
new_jsjava_thread_state(JSJavaVM *jsjava_vm, const char *thread_name, JNIEnv *jEnv)
{
    JSJavaThreadState *jsj_env;

    jsj_env = (JSJavaThreadState *)malloc(sizeof(JSJavaThreadState));
    if (!jsj_env)
        return NULL;
    memset(jsj_env, 0, sizeof(JSJavaThreadState));

    jsj_env->jEnv = jEnv;
    jsj_env->jsjava_vm = jsjava_vm;
    if (thread_name)
        jsj_env->name = strdup(thread_name);

    /* THREADSAFETY - need to protect against races */
    jsj_env->next = thread_list;
    thread_list = jsj_env;

    return jsj_env;
}

static JSJavaThreadState *
find_jsjava_thread(JNIEnv *jEnv)
{
    JSJavaThreadState *e, **p, *jsj_env;
    jsj_env = NULL;

    /* THREADSAFETY - need to protect against races in manipulating the thread list */

    /* Search for the thread state among the list of all created
       LiveConnect threads */
    for (p = &thread_list; (e = *p) != NULL; p = &(e->next)) {
        if (e->jEnv == jEnv) {
            jsj_env = e;
            *p = jsj_env->next;
            break;
        }
    }

    /* Move a found thread to head of list for faster search next time. */
    if (jsj_env)
        thread_list = jsj_env;
    
    return jsj_env;
}

PR_PUBLIC_API(JSJavaThreadState *)
JSJ_AttachCurrentThreadToJava(JSJavaVM *jsjava_vm, const char *name, JNIEnv **java_envp)
{
    JNIEnv *jEnv;
    JavaVM *java_vm;
    JSJavaThreadState *jsj_env;

    /* Try to attach a Java thread to the current native thread */
    java_vm = jsjava_vm->java_vm;
    if ((*java_vm)->AttachCurrentThread(java_vm, &jEnv, NULL) < 0)
        return NULL;

    /* If we found an existing thread state, just return it. */
    jsj_env = find_jsjava_thread(jEnv);
    if (jsj_env)
        return jsj_env;

    /* Create a new wrapper around the thread/VM state */
    jsj_env = new_jsjava_thread_state(jsjava_vm, name, jEnv);

    if (java_envp)
        *java_envp = jEnv;
    return jsj_env;
}

static JSJavaVM *
map_java_vm_to_jsjava_vm(JavaVM *java_vm)
{
    JSJavaVM *v;
    for (v = jsjava_vm_list; v; v = v->next) {
        if (v->java_vm == java_vm)
            return v;
    }
    return NULL;
}

/*
 * Unfortunately, there's no standard means to associate any private data with
 * a JNI thread environment, so we need to use the Java environment pointer as
 * the key in a lookup table that maps it to a JSJavaThreadState structure, 
 * where we store all our per-thread private data.  If no existing thread state
 * is found, a new one is created.
 *
 * If an error occurs, returns NULL and sets the errp argument to an error
 * message, which the caller is responsible for free'ing.
 */
JSJavaThreadState *
jsj_MapJavaThreadToJSJavaThreadState(JNIEnv *jEnv, char **errp)
{
    JSJavaThreadState *jsj_env;
    JavaVM *java_vm;
    JSJavaVM *jsjava_vm;

    /* If we found an existing thread state, just return it. */
    jsj_env = find_jsjava_thread(jEnv);
    if (jsj_env)
        return jsj_env;

    /* No one set up a LiveConnect thread state for a given Java thread.
       Invoke the callback to create one on-the-fly. */

    /* First, figure out which Java VM is calling us */
    if ((*jEnv)->GetJavaVM(jEnv, &java_vm) < 0)
        return NULL;

    /* Get our private JavaVM data */
    jsjava_vm = map_java_vm_to_jsjava_vm(java_vm);
    if (!jsjava_vm) {
        *errp = PR_smprintf("Total weirdness:   No JSJavaVM wrapper ever created "
                            "for JavaVM 0x%08x", java_vm);
        return NULL;
    }

    jsj_env = new_jsjava_thread_state(jsjava_vm, NULL, jEnv);
    if (!jsj_env)
        return NULL;

    return jsj_env;
}

/*
 * This function is used to specify a particular JSContext as *the* JavaScript
 * execution environment to be used when LiveConnect is accessed from the given
 * Java thread, i.e. by using one of the methods of netscape.javascript.JSObject.
 * (There can only be one such JS context for a given Java thread.  To
 * multiplex JSContexts among a single thread, this function must be called
 * before Java is invoked on that thread.)  The return value is the previous
 * context associated with the given Java thread.
 */
PR_PUBLIC_API(JSContext *)
JSJ_SetDefaultJSContextForJavaThread(JSContext *cx, JSJavaThreadState *jsj_env)
{
    JSContext *old_context;
    old_context = jsj_env->cx;
    jsj_env->cx = cx;
    return old_context;
}

PR_PUBLIC_API(JSBool)
JSJ_DetachCurrentThreadFromJava(JSJavaThreadState *jsj_env)
{
    JavaVM *java_vm;
    JSJavaThreadState *e, **p;

    /* Disassociate the current native thread from its corresponding Java thread */
    java_vm = jsj_env->jsjava_vm->java_vm;
    if ((*java_vm)->DetachCurrentThread(java_vm) < 0)
        return JS_FALSE;

    /* Destroy the LiveConnect execution environment passed in */
    jsj_ClearPendingJSErrors(jsj_env);

    /* THREADSAFETY - need to protect against races */
    for (p = &thread_list; (e = *p) != NULL; p = &(e->next)) {
        if (e == jsj_env) {
            *p = jsj_env->next;
            break;
        }
    }

    free(jsj_env);
    return JS_TRUE;
}

JSBool
JSJ_ConvertJavaObjectToJSValue(JSContext *cx, jobject java_obj, jsval *vp)
{
    JNIEnv *jEnv;
            
    /* Get the Java per-thread environment pointer for this JSContext */
    jsj_MapJSContextToJSJThread(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;

    jsj_ConvertJavaObjectToJSValue(cx, jEnv, java_obj, vp);
}

/*===========================================================================*/

/* The convenience functions below present a complete, but simplified
   LiveConnect API which is designed to handle the special case of a single 
   Java-VM, single-threaded operation, and use of only one JSContext. */

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
default_map_jsj_thread_to_js_context(JSJavaThreadState *jsj_env, char **errp)
{
    return the_cx;
}

/* Trivial implementation of callback function */
static JSObject *
default_map_java_object_to_js_object(JNIEnv *jEnv, jobject hint, char **errp)
{
    return the_global_js_obj;
}

/* Trivial implementations of callback functions */
JSJCallbacks jsj_default_callbacks = {
    default_map_jsj_thread_to_js_context,
    default_map_js_context_to_jsj_thread,
    default_map_java_object_to_js_object
};

/*
 * Initialize the provided JSContext by setting up the JS classes necessary for
 * reflection and by defining JavaPackage objects for the default Java packages
 * as properties of global_obj.  If java_vm is NULL, a new Java VM is
 * created, using the provided classpath in addition to any default classpath.
 * The classpath argument is ignored, however, if java_vm is non-NULL.
 */
JSBool
JSJ_SimpleInit(JSContext *cx, JSObject *global_obj, JavaVM *java_vm, const char *classpath)
{
    JNIEnv *jEnv;

    PR_ASSERT(!the_jsj_vm);
    the_jsj_vm = JSJ_ConnectToJavaVM(java_vm, classpath);
    if (!the_jsj_vm)
        return JS_FALSE;

    JSJ_Init(&jsj_default_callbacks);

    if (!JSJ_InitJSContext(cx, global_obj, NULL))
        goto error;
    the_cx = cx;
    the_global_js_obj = global_obj;

    the_jsj_thread = JSJ_AttachCurrentThreadToJava(the_jsj_vm, "main thread", &jEnv);
    if (!the_jsj_thread)
        goto error;

    return JS_TRUE;

error:
    JSJ_SimpleShutdown();
    return JS_FALSE;
}

/*
 * Free up all LiveConnect resources.  Destroy the Java VM if it was
 * created by LiveConnect.
 */
PR_PUBLIC_API(void)
JSJ_SimpleShutdown()
{
    PR_ASSERT(the_jsj_vm);
    JSJ_DisconnectFromJavaVM(the_jsj_vm);
    the_jsj_vm = NULL;
    the_cx = NULL;
    the_global_js_obj = NULL;
    the_jsj_thread = NULL;
}

