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
 * Declarations of private (internal) functions/data/types for
 * JavaScript <==> Java communication.
 *
 */
 

#ifndef _JSJAVA_PVT_H
#define _JSJAVA_PVT_H

#include "prhash.h"          /* NSPR hash-tables      */
#include "jni.h"             /* Java Native Interface */
#include "jsapi.h"           /* JavaScript engine API */


/*************************** Type Declarations ******************************/

/* Forward type declarations */
typedef struct JavaMemberDescriptor JavaMemberDescriptor;
typedef struct JavaMethodSpec JavaMethodSpec;
typedef struct JavaClassDescriptor JavaClassDescriptor;
typedef struct JavaClassDescriptor JavaSignature;
typedef struct JSJCallbacks JSJCallbacks;
typedef struct CapturedJSError CapturedJSError;
typedef struct JavaPackageDef JavaPackageDef;
typedef struct JSJavaThreadState JSJavaThreadState;
typedef struct JSJavaVM JSJavaVM;
typedef struct JavaMemberVal JavaMemberVal;

/*
 * This enum uses the same character encoding used by the JDK to encode
 * Java type signatures, but the enum is easier to debug/compile with.
 */
typedef enum {
    JAVA_SIGNATURE_ARRAY    =   '[',
    JAVA_SIGNATURE_BYTE     =   'B',
    JAVA_SIGNATURE_CHAR     =   'C',
    JAVA_SIGNATURE_CLASS    =   'L',
    JAVA_SIGNATURE_FLOAT    =   'F',
    JAVA_SIGNATURE_DOUBLE   =   'D',
    JAVA_SIGNATURE_INT      =   'I',
    JAVA_SIGNATURE_LONG     =   'J',
    JAVA_SIGNATURE_SHORT    =   'S',
    JAVA_SIGNATURE_VOID     =   'V',
    JAVA_SIGNATURE_BOOLEAN  =   'Z',
    JAVA_SIGNATURE_UNKNOWN  =    0
} JavaSignatureChar;

/* The signature of a Java method consists of the signatures of all its
   arguments and its return type signature. */
typedef struct JavaMethodSignature {
    jsize                   num_args;               /* Length of arg_signatures array */
    JavaSignature **        arg_signatures;         /* Array of argument signatures */
    JavaSignature *         return_val_signature;   /* Return type signature */
} JavaMethodSignature;

/* A descriptor for the reflection of a single Java field */
typedef struct JavaFieldSpec {
    jfieldID                fieldID;    /* JVM opaque access handle for field */
    JavaSignature *         signature;  /* Java type of field */
    int                     modifiers;  /* Bitfield indicating field qualifiers */
    const char *            name;       /* UTF8; TODO - Should support Unicode field names */
} JavaFieldSpec;

/* A descriptor for the reflection of a single Java method.
   Each overloaded method has a separate corresponding JavaMethodSpec. */
typedef struct JavaMethodSpec {
    jmethodID               methodID;   /* JVM opaque access handle for method */
    JavaMethodSignature     signature;
    const char *            name;       /* UTF8; TODO - Should support Unicode method names */
    JavaMethodSpec *        next;       /* next method in chain of overloaded methods */
} JavaMethodSpec;

/*
 * A descriptor for the reflection of a single member of a Java object.
 * This can represent one or more Java methods and/or a single field.
 * (When there is more than one method attached to a single JavaMemberDescriptor
 * they are overloaded methods sharing the same simple name.)  This same
 * descriptor type is used for both static or instance members.
 */
typedef struct JavaMemberDescriptor {
    const char *            name;       /* simple name of field and/or method */
    jsid                    id;         /* hashed name for quick JS property lookup */
    JavaFieldSpec *         field;      /* field with the given name, if any */
    JavaMethodSpec *        methods;    /* Overloaded methods which share the same name, if any */
    JavaMemberDescriptor *  next;       /* next descriptor in same defining class */
} JavaMemberDescriptor;

/* This is the native portion of a reflected Java class */
typedef struct JavaClassDescriptor {
    const char *            name;       /* Name of class, e.g. "java/lang/Byte" */
    JavaSignatureChar       type;       /* class category: primitive type, object, array */
    jclass                  java_class; /* Opaque JVM handle to corresponding java.lang.Class */
    int                     num_instance_members;
    int                     num_static_members;
    JSBool                  instance_members_reflected;
    JavaMemberDescriptor *  instance_members;
    JSBool                  static_members_reflected;
    JavaMemberDescriptor *  static_members;
    JavaMemberDescriptor *  constructors;
    int                     modifiers;  /* Class declaration qualifiers,
                                           e.g. abstract, private */
    int                     ref_count;  /* # of references to this struct */
    JavaSignature *         array_component_signature; /* Only non-NULL for array classes */
} JavaClassDescriptor;

/* This is the native portion of a reflected Java method or field */
typedef struct JavaMemberVal {
    jsval                   field_val;              /* Captured value of Java field */
    jsval                   invoke_method_func_val; /* JSFunction wrapper around Java method invoker */
    JavaMemberDescriptor *  descriptor;
    JavaMemberVal *         next;
} JavaMemberVal;

/* This is the native portion of a reflected Java object */
typedef struct {
    jobject                 java_obj;           /* Opaque JVM ref to Java object */
    JavaClassDescriptor *   class_descriptor;   /* Java class info */
    JavaMemberVal *         members;            /* Reflected methods and fields */
} JavaObjectWrapper;

/* These are definitions of the Java class/method/field modifier bits.
   These really shouldn't be hard-coded here.  Rather,
   they should be read from java.lang.reflect.Modifier */
#define ACC_PUBLIC          0x0001      /* visible to everyone */
#define ACC_STATIC          0x0008      /* instance variable is static */
#define ACC_FINAL           0x0010      /* no further subclassing,overriding */
#define ACC_INTERFACE       0x0200      /* class is an interface */
#define ACC_ABSTRACT	    0x0400      /* no definition provided */

/* A JSJavaVM structure must be created for each Java VM that is accessed
   via LiveConnect */
typedef struct JSJavaVM {
/* TODO -  all LiveConnect global variables should be migrated into this
           structure in order to allow more than one LiveConnect-enabled
           Java VM to exist within the same process. */
    JavaVM *            java_vm;
    JNIEnv *            main_thread_env;       /* Main-thread Java environment */
    JSBool              jsj_created_java_vm;
    int                 num_attached_threads;
    JSJavaVM *          next;           /* next VM among all created VMs */
} JSJavaVM;

/* Per-thread state that encapsulates the connection to the Java VM */
typedef struct JSJavaThreadState {
    const char *        name;           /* Thread name, for debugging */
    JSJavaVM *          jsjava_vm;      /* All per-JVM state */
    JNIEnv *            jEnv;           /* Per-thread opaque handle to Java VM */
    CapturedJSError *   pending_js_errors; /* JS errors to be thrown as Java exceptions */
    JSContext *         cx;             /* current JS context for thread */
    JSJavaThreadState * next;           /* next thread state among all created threads */
} JSJavaThreadState;

/******************************** Globals ***********************************/

extern JSJCallbacks *JSJ_callbacks;

/* JavaScript classes that reflect Java objects */
extern JSClass JavaObject_class;
extern JSClass JavaArray_class;
extern JSClass JavaClass_class;

/*
 * Opaque JVM handles to Java classes, methods and objects required for
 * Java reflection.  These are computed and cached during initialization.
 * TODO: These should be moved inside the JSJavaVM struct
 */
extern jclass jlObject;                        /* java.lang.Object */
extern jclass jlrConstructor;                  /* java.lang.reflect.Constructor */
extern jclass jlThrowable;                     /* java.lang.Throwable */
extern jclass jlSystem;                        /* java.lang.System */
extern jclass jlClass;                         /* java.lang.Class */
extern jclass jlBoolean;                       /* java.lang.Boolean */
extern jclass jlDouble;                        /* java.lang.Double */
extern jclass jlString;                        /* java.lang.String */
extern jclass njJSObject;                      /* netscape.javascript.JSObject */
extern jclass njJSException;                   /* netscape.javascript.JSException */
extern jclass njJSUtil;                        /* netscape.javascript.JSUtil */

extern jmethodID jlClass_getMethods;           /* java.lang.Class.getMethods() */
extern jmethodID jlClass_getConstructors;      /* java.lang.Class.getConstructors() */
extern jmethodID jlClass_getFields;            /* java.lang.Class.getFields() */
extern jmethodID jlClass_getName;              /* java.lang.Class.getName() */
extern jmethodID jlClass_getComponentType;     /* java.lang.Class.getComponentType() */
extern jmethodID jlClass_getModifiers;         /* java.lang.Class.getModifiers() */
extern jmethodID jlClass_isArray;              /* java.lang.Class.isArray() */

extern jmethodID jlrMethod_getName;            /* java.lang.reflect.Method.getName() */
extern jmethodID jlrMethod_getParameterTypes;  /* java.lang.reflect.Method.getParameterTypes() */
extern jmethodID jlrMethod_getReturnType;      /* java.lang.reflect.Method.getReturnType() */
extern jmethodID jlrMethod_getModifiers;       /* java.lang.reflect.Method.getModifiers() */

extern jmethodID jlrConstructor_getParameterTypes; /* java.lang.reflect.Constructor.getParameterTypes() */
extern jmethodID jlrConstructor_getModifiers;  /* java.lang.reflect.Constructor.getModifiers() */

extern jmethodID jlrField_getName;             /* java.lang.reflect.Field.getName() */
extern jmethodID jlrField_getType;             /* java.lang.reflect.Field.getType() */
extern jmethodID jlrField_getModifiers;        /* java.lang.reflect.Field.getModifiers() */

extern jmethodID jlThrowable_getMessage;       /* java.lang.Throwable.getMessage() */
extern jmethodID jlThrowable_toString;         /* java.lang.Throwable.toString() */

extern jmethodID jlBoolean_Boolean;            /* java.lang.Boolean constructor */
extern jmethodID jlBoolean_booleanValue;       /* java.lang.Boolean.booleanValue() */
extern jmethodID jlDouble_Double;              /* java.lang.Double constructor */
extern jmethodID jlDouble_doubleValue;         /* java.lang.Double.doubleValue() */

extern jmethodID jlSystem_identityHashCode;    /* java.lang.System.identityHashCode() */

extern jobject jlVoid_TYPE;                    /* java.lang.Void.TYPE value */

extern jmethodID njJSException_JSException;    /* netscape.javascipt.JSexception constructor */
extern jmethodID njJSObject_JSObject;          /* netscape.javascript.JSObject constructor */
extern jmethodID njJSUtil_getStackTrace;       /* netscape.javascript.JSUtil.getStackTrace() */
extern jfieldID njJSObject_internal;           /* netscape.javascript.JSObject.internal */
extern jfieldID njJSException_lineno;          /* netscape.javascript.JSException.lineno */
extern jfieldID njJSException_tokenIndex;      /* netscape.javascript.JSException.tokenIndex */
extern jfieldID njJSException_source;          /* netscape.javascript.JSException.source */
extern jfieldID njJSException_filename;        /* netscape.javascript.JSException.filename */


/**************** Java <==> JS conversions and Java types *******************/
extern JSBool
jsj_ComputeJavaClassSignature(JSContext *cx,
                              JavaSignature *signature,
                              jclass java_class);

extern const char *
jsj_ConvertJavaSignatureToString(JSContext *cx, JavaSignature *signature);
extern const char *
jsj_ConvertJavaSignatureToHRString(JSContext *cx,
                                   JavaSignature *signature);
extern JavaMethodSignature *
jsj_InitJavaMethodSignature(JSContext *cx, JNIEnv *jEnv, jobject method,
                            JavaMethodSignature *method_signature);

extern const char *
jsj_ConvertJavaMethodSignatureToString(JSContext *cx,
                                       JavaMethodSignature *method_signature);
extern const char *
jsj_ConvertJavaMethodSignatureToHRString(JSContext *cx,
                                         const char *method_name,
                                         JavaMethodSignature *method_signature);
extern void
jsj_PurgeJavaMethodSignature(JSContext *cx, JNIEnv *jEnv, JavaMethodSignature *signature);

extern JSBool
jsj_ConvertJSValueToJavaValue(JSContext *cx, JNIEnv *jEnv, jsval v, JavaSignature *signature,
			      int *cost, jvalue *java_value, JSBool *is_local_refp);
extern JSBool
jsj_ConvertJSValueToJavaObject(JSContext *cx, JNIEnv *jEnv, jsval v, JavaSignature *signature,
			       int *cost, jobject *java_value, JSBool *is_local_refp);
extern jstring
jsj_ConvertJSStringToJavaString(JSContext *cx, JNIEnv *jEnv, JSString *js_str);

extern JSBool
jsj_ConvertJavaValueToJSValue(JSContext *cx, JNIEnv *jEnv, JavaSignature *signature,
                              jvalue *java_value, jsval *vp);
extern JSBool
jsj_ConvertJavaObjectToJSValue(JSContext *cx, JNIEnv *jEnv,
                               jobject java_obj, jsval *vp);
extern JSBool
jsj_ConvertJavaObjectToJSString(JSContext *cx, JNIEnv *jEnv,
                                JavaClassDescriptor *class_descriptor,
                                jobject java_obj, jsval *vp);
extern JSBool
jsj_ConvertJavaObjectToJSNumber(JSContext *cx, JNIEnv *jEnv,
                                jobject java_obj, jsval *vp);
extern JSBool
jsj_ConvertJavaObjectToJSBoolean(JSContext *cx, JNIEnv *jEnv,
                                 jobject java_obj, jsval *vp);

/************************ Java package reflection **************************/
extern JSBool
jsj_init_JavaPackage(JSContext *, JSObject *,
                     JavaPackageDef *predefined_packages);

/************************* Java class reflection ***************************/
extern JSBool
jsj_init_JavaClass(JSContext *cx, JSObject *global_obj);

extern void
jsj_DiscardJavaClassReflections(JNIEnv *jEnv);

extern const char *
jsj_GetJavaClassName(JSContext *cx, JNIEnv *jEnv, jclass java_class);

extern JavaClassDescriptor *
jsj_GetJavaClassDescriptor(JSContext *cx, JNIEnv *jEnv, jclass java_class);

extern void
jsj_ReleaseJavaClassDescriptor(JSContext *cx, JNIEnv *jEnv, JavaClassDescriptor *class_descriptor);

extern JSObject *
jsj_define_JavaClass(JSContext *cx, JNIEnv *jEnv, JSObject *obj,
                     const char *unqualified_class_name,
                     jclass jclazz);

extern JavaMemberDescriptor *
jsj_GetJavaMemberDescriptor(JSContext *cx,
                            JNIEnv *jEnv,
                            JavaClassDescriptor *class_descriptor,
                            jstring member_name);

extern JavaMemberDescriptor *
jsj_LookupJavaMemberDescriptorById(JSContext *cx, JNIEnv *jEnv,
                                   JavaClassDescriptor *class_descriptor,
                                   jsid id);

extern JavaMemberDescriptor *
jsj_LookupJavaStaticMemberDescriptorById(JSContext *cx, JNIEnv *jEnv,
                                         JavaClassDescriptor *class_descriptor,
                                         jsid id);

extern JavaMemberDescriptor *
jsj_GetJavaStaticMemberDescriptor(JSContext *cx, JNIEnv *jEnv,
                                  JavaClassDescriptor *class_descriptor,
                                  jstring member_name);

extern JavaMemberDescriptor *
jsj_GetJavaClassConstructors(JSContext *cx,
                             JavaClassDescriptor *class_descriptor);

extern JavaMemberDescriptor *
jsj_LookupJavaClassConstructors(JSContext *cx, JNIEnv *jEnv,
                                JavaClassDescriptor *class_descriptor);

extern JavaMemberDescriptor *
jsj_GetClassInstanceMembers(JSContext *cx, JNIEnv *jEnv,
                            JavaClassDescriptor *class_descriptor);

extern JavaMemberDescriptor *
jsj_GetClassStaticMembers(JSContext *cx, JNIEnv *jEnv,
                          JavaClassDescriptor *class_descriptor);

extern JSBool
jsj_InitJavaClassReflectionsTable();

/************************* Java field reflection ***************************/

extern JSBool
jsj_GetJavaFieldValue(JSContext *cx, JNIEnv *jEnv, JavaFieldSpec *field_spec,
                      jobject java_obj, jsval *vp);
extern JSBool
jsj_SetJavaFieldValue(JSContext *cx, JNIEnv *jEnv, JavaFieldSpec *field_spec,
                      jobject java_obj, jsval js_val);
extern JSBool 
jsj_ReflectJavaFields(JSContext *cx, JNIEnv *jEnv,
                      JavaClassDescriptor *class_descriptor,
                      JSBool reflect_only_static_fields);
extern void
jsj_DestroyFieldSpec(JSContext *cx, JNIEnv *jEnv, JavaFieldSpec *field);

/************************* Java method reflection ***************************/
extern JSBool
jsj_JavaInstanceMethodWrapper(JSContext *cx, JSObject *obj,
                              uintN argc, jsval *argv, jsval *vp);
extern JSBool
jsj_JavaStaticMethodWrapper(JSContext *cx, JSObject *obj,
                            uintN argc, jsval *argv, jsval *vp);
extern JSBool
jsj_JavaConstructorWrapper(JSContext *cx, JSObject *obj,
                           uintN argc, jsval *argv, jsval *vp);
extern JSBool 
jsj_ReflectJavaMethods(JSContext *cx, JNIEnv *jEnv,
                       JavaClassDescriptor *class_descriptor,
                       JSBool reflect_only_static_methods);
extern void
jsj_DestroyMethodSpec(JSContext *cx, JNIEnv *jEnv, JavaMethodSpec *method_spec);

/************************* Java member reflection ***************************/
extern JSBool
jsj_init_JavaMember(JSContext *, JSObject *);

extern JSBool
jsj_ReflectJavaMethodsAndFields(JSContext *cx, JavaClassDescriptor *class_descriptor,
                                JSBool reflect_only_statics);

/************************* Java object reflection **************************/
extern JSBool
jsj_init_JavaObject(JSContext *, JSObject *);

extern JSObject *
jsj_WrapJavaObject(JSContext *cx, JNIEnv *jEnv, jobject java_obj, jclass java_class);

extern void
jsj_DiscardJavaObjReflections(JNIEnv *jEnv);

extern JSBool
JavaObject_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp);

extern void
JavaObject_finalize(JSContext *cx, JSObject *obj);

extern JSBool
JavaObject_resolve(JSContext *cx, JSObject *obj, jsval id);

extern JSBool
JavaObject_enumerate(JSContext *cx, JSObject *obj);

extern JSBool
JavaObject_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
JavaObject_getPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

/************************* Java array reflection ***************************/
extern JSBool
jsj_init_JavaArray(JSContext *cx, JSObject *global_obj);

extern JSBool
jsj_GetJavaArrayElement(JSContext *cx, JNIEnv *jEnv, jarray java_array,
                        jsize index, JavaSignature *array_component_signature,
                        jsval *vp);
   
extern JSBool     
jsj_SetJavaArrayElement(JSContext *cx, JNIEnv *jEnv, jarray java_array,
                        jsize index, JavaSignature *array_component_signature,
                        jsval js_val);

/********************* JavaScript object reflection ************************/                        
extern jobject
jsj_WrapJSObject(JSContext *cx, JNIEnv *jEnv, JSObject *js_obj);

extern void
jsj_ClearPendingJSErrors(JSJavaThreadState *jsj_env);

extern JSBool
jsj_ReportUncaughtJSException(JSContext *cx, JNIEnv *jEnv, jthrowable java_exception);

/**************************** Utilities ************************************/
extern void
jsj_ReportJavaError(JSContext *cx, JNIEnv *env, const char *format, ...);

extern void
jsj_UnexpectedJavaError(JSContext *cx, JNIEnv *env, const char *format, ...);

extern const char *
jsj_GetJavaErrorMessage(JNIEnv *env);

extern void
jsj_LogError(const char *error_msg);

PR_CALLBACK prhashcode
jsj_HashJavaObject(const void *key, JNIEnv *jEnv);

PR_CALLBACK intN
jsj_JavaObjectComparator(const void *v1, const void *v2, void *arg);

extern JSJavaThreadState *
jsj_MapJavaThreadToJSJavaThreadState(JNIEnv *jEnv, char **errp);

extern void
jsj_MakeJNIClassname(char *jClassName);

extern const char *
jsj_ClassNameOfJavaObject(JSContext *cx, JNIEnv *jEnv, jobject java_object);

extern jsize
jsj_GetJavaArrayLength(JSContext *cx, JNIEnv *jEnv, jarray java_array);

extern JSBool
JavaStringToId(JSContext *cx, JNIEnv *jEnv, jstring jstr, jsid *idp);

extern const char *
jsj_DupJavaStringUTF(JSContext *cx, JNIEnv *jEnv, jstring jstr);

JSJavaThreadState *
jsj_MapJSContextToJSJThread(JSContext *cx, JNIEnv **envp);

#ifdef DEBUG
#define DEBUG_LOG(args) printf args
#endif

#define JS_FREE_IF(cx, x)                                                   \
    PR_BEGIN_MACRO                                                          \
        if (x)                                                              \
            JS_free(cx, x);                                                 \
    PR_END_MACRO

#endif   /* _JSJAVA_PVT_H */
