/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * It contains the native code implementation of JS's JavaClass class.
 *
 * A JavaClass is JavaScript's representation of a Java class.
 * Its parent JS object is always a JavaPackage object.  A JavaClass is not an
 * exact reflection of Java's corresponding java.lang.Class object.  Rather,
 * the properties of a JavaClass are the static methods and properties of the
 * corresponding Java class.
 *
 * Note that there is no runtime equivalent to the JavaClass class in Java.
 * (Although there are instances of java.lang.String and there are static
 * methods of java.lang.String that can be invoked, there's no such thing as
 * a first-class object that can be referenced simply as "java.lang.String".)
 */

#include <stdlib.h>
#include <string.h>

#include "jsj_private.h"        /* LiveConnect internals */
#include "jscntxt.h"            /* for error reporting */

JS_STATIC_DLL_CALLBACK(JSBool)
JavaClass_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    char *name;
    JSString *str;

    JavaClassDescriptor *class_descriptor;
    
    class_descriptor = JS_GetPrivate(cx, obj);
    if (!class_descriptor)
        return JS_FALSE;

    switch(type) {


    case JSTYPE_STRING:
        /* Convert '/' to '.' so that it looks like Java language syntax. */
        if (!class_descriptor->name)
            break;
        name = JS_smprintf("[JavaClass %s]", class_descriptor->name);
        if (!name) {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
        }

        str = JS_NewString(cx, name, strlen(name));
        if (!str) {
            free(name);
            /* It's not necessary to call JS_ReportOutOfMemory(), as
               JS_NewString() will do so on failure. */
            return JS_FALSE;
        }

        *vp = STRING_TO_JSVAL(str);
        return JS_TRUE;

    default:
      break;
    }
    return JS_TRUE;
}

static JSBool
lookup_static_member_by_id(JSContext *cx, JNIEnv *jEnv, JSObject *obj,
                           JavaClassDescriptor **class_descriptorp,
                           jsid id, JavaMemberDescriptor **memberp)
{
    jsval idval;
    JavaMemberDescriptor *member_descriptor;
    const char *member_name;
    JavaClassDescriptor *class_descriptor;

    class_descriptor = JS_GetPrivate(cx, obj);
    if (!class_descriptor) {
        *class_descriptorp = NULL;
        *memberp = NULL;
        return JS_TRUE;
    }
    
    if (class_descriptorp)
        *class_descriptorp = class_descriptor;
    
    member_descriptor = jsj_LookupJavaStaticMemberDescriptorById(cx, jEnv, class_descriptor, id);
    if (!member_descriptor) {
        JS_IdToValue(cx, id, &idval);
        if (!JSVAL_IS_STRING(idval)) {
            JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                            JSJMSG_BAD_JCLASS_EXPR);
            return JS_FALSE;
        }

        member_name = JS_GetStringBytes(JSVAL_TO_STRING(idval));
        
        /*
         * See if the property looks like the explicit resolution of an
         * overloaded method, e.g. "max(double,double)".
         */
        member_descriptor =
            jsj_ResolveExplicitMethod(cx, jEnv, class_descriptor, id, JS_TRUE);
        if (member_descriptor)
            goto done;

        /* Why do we have to do this ? */
        if (!strcmp(member_name, "prototype")) {
            *memberp = NULL;
            return JS_TRUE;
        }

        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                        JSJMSG_MISSING_NAME,
                       class_descriptor->name, member_name);
        return JS_FALSE;
    }

done:
    if (memberp)
        *memberp = member_descriptor;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaClass_getPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    jsval idval;
    jclass java_class;
    const char *member_name;
    JavaClassDescriptor *class_descriptor;
    JavaMemberDescriptor *member_descriptor;
    JNIEnv *jEnv;
    JSJavaThreadState *jsj_env;
    JSBool result;

    /* printf("In JavaClass_getProperty\n"); */
    
    /* Get the Java per-thread environment pointer for this JSContext */
    jsj_env = jsj_EnterJava(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;

    if (!lookup_static_member_by_id(cx, jEnv, obj, &class_descriptor, id, &member_descriptor)) {
	jsj_ExitJava(jsj_env);
        return JS_FALSE;
    }
    if (!member_descriptor) {
        *vp = JSVAL_VOID;
	jsj_ExitJava(jsj_env);
        return JS_TRUE;
    }

    java_class = class_descriptor->java_class;

    if (member_descriptor->field) {
        if (!member_descriptor->methods) {
            result = jsj_GetJavaFieldValue(cx, jEnv, member_descriptor->field, java_class, vp);
	    jsj_ExitJava(jsj_env);
	    return result;
        } else {
            JS_ASSERT(0);
        }
    } else {
        JSFunction *function;
        
        /* TODO - eliminate JSFUN_BOUND_METHOD */
        if (member_descriptor->methods->is_alias) {
            /* If this is an explicit resolution of an overloaded method,
               use the fully-qualified method name as the name of the
               resulting JS function, i.e. "myMethod(int,long)" */
            JS_IdToValue(cx, id, &idval);
            member_name = JS_GetStringBytes(JSVAL_TO_STRING(idval));
        } else {
            /* Either not explicit resolution of overloaded method or
               explicit resolution was unnecessary because method was
               not overloaded. */
            member_name = member_descriptor->name;
        }
        function = JS_NewFunction(cx, jsj_JavaStaticMethodWrapper, 0,
                                  JSFUN_BOUND_METHOD, obj, member_name);
        if (!function) {
	    jsj_ExitJava(jsj_env);
            return JS_FALSE;
	}

        *vp = OBJECT_TO_JSVAL(JS_GetFunctionObject(function));
    }

    jsj_ExitJava(jsj_env);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaClass_setPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    jclass java_class;
    const char *member_name;
    JavaClassDescriptor *class_descriptor;
    JavaMemberDescriptor *member_descriptor;
    jsval idval;
    JNIEnv *jEnv;
    JSJavaThreadState *jsj_env;
    JSBool result;

    /* printf("In JavaClass_setProperty\n"); */

    /* Get the Java per-thread environment pointer for this JSContext */
    jsj_env = jsj_EnterJava(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;
    
    if (!lookup_static_member_by_id(cx, jEnv, obj, &class_descriptor, id, &member_descriptor)) {
	jsj_ExitJava(jsj_env);
        return JS_FALSE;
    }

    /* Check for the case where there is a method with the given name, but no field
       with that name */
    if (!member_descriptor->field)
        goto no_such_field;

    /* Silently fail if field value is final (immutable), as required by ECMA spec */
    if (member_descriptor->field->modifiers & ACC_FINAL) {
	jsj_ExitJava(jsj_env);
        return JS_TRUE;
    }

    java_class = class_descriptor->java_class;
    result = jsj_SetJavaFieldValue(cx, jEnv, member_descriptor->field, java_class, *vp);
    jsj_ExitJava(jsj_env);
    return result;

no_such_field:
    JS_IdToValue(cx, id, &idval);
    member_name = JS_GetStringBytes(JSVAL_TO_STRING(idval));
    JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                   JSJMSG_MISSING_STATIC,
                   member_name, class_descriptor->name);
    jsj_ExitJava(jsj_env);
    return JS_FALSE;
}

/*
 * Free the private native data associated with the JavaPackage object.
 */
JS_STATIC_DLL_CALLBACK(void)
JavaClass_finalize(JSContext *cx, JSObject *obj)
{
    JNIEnv *jEnv;
    JSJavaThreadState *jsj_env;

    JavaClassDescriptor *class_descriptor = JS_GetPrivate(cx, obj);
    if (!class_descriptor)
        return;
    
    /* Get the Java per-thread environment pointer for this JSContext */
    jsj_env = jsj_EnterJava(cx, &jEnv);
    if (!jEnv)
        return;

    /* printf("Finalizing %s\n", class_descriptor->name); */
    jsj_ReleaseJavaClassDescriptor(cx, jEnv, class_descriptor);
    jsj_ExitJava(jsj_env);
}


JS_STATIC_DLL_CALLBACK(JSBool)
JavaClass_lookupProperty(JSContext *cx, JSObject *obj, jsid id,
                         JSObject **objp, JSProperty **propp
#if defined JS_THREADSAFE && defined DEBUG
                            , const char *file, uintN line
#endif
                            )
{
    JNIEnv *jEnv;
    JSErrorReporter old_reporter;
    JSJavaThreadState *jsj_env;

    /* printf("In JavaClass_lookupProperty()\n"); */
    
    /* Get the Java per-thread environment pointer for this JSContext */
    jsj_env = jsj_EnterJava(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;

    old_reporter = JS_SetErrorReporter(cx, NULL);
    if (lookup_static_member_by_id(cx, jEnv, obj, NULL, id, NULL)) {
        *objp = obj;
        *propp = (JSProperty*)1;
    } else {
        *objp = NULL;
        *propp = NULL;
    }

    JS_SetErrorReporter(cx, old_reporter);
    jsj_ExitJava(jsj_env);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaClass_defineProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                         JSPropertyOp getter, JSPropertyOp setter,
                         uintN attrs, JSProperty **propp)
{
    JavaClassDescriptor *class_descriptor;
    
    class_descriptor = JS_GetPrivate(cx, obj);

    /* Check for prototype JavaClass object */
    if (!class_descriptor)
	return JS_TRUE;

    JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                         JSJMSG_JCLASS_PROP_DEFINE);
    return JS_FALSE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaClass_getAttributes(JSContext *cx, JSObject *obj, jsid id,
                        JSProperty *prop, uintN *attrsp)
{
    /* We don't maintain JS property attributes for Java class members */
    *attrsp = JSPROP_PERMANENT|JSPROP_ENUMERATE;
    return JS_FALSE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaClass_setAttributes(JSContext *cx, JSObject *obj, jsid id,
                        JSProperty *prop, uintN *attrsp)
{
    /* We don't maintain JS property attributes for Java class members */
    if (*attrsp != (JSPROP_PERMANENT|JSPROP_ENUMERATE)) {
        JS_ASSERT(0);
        return JS_FALSE;
    }

    /* Silently ignore all setAttribute attempts */
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaClass_deleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSVersion version = JS_GetVersion(cx);
    
    *vp = JSVAL_FALSE;

    if (!JSVERSION_IS_ECMA(version)) {
        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL,
                                            JSJMSG_JCLASS_PROP_DELETE);
        return JS_FALSE;
    } else {
        /* Attempts to delete permanent properties are silently ignored
           by ECMAScript. */
        return JS_TRUE;
    }
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaClass_defaultValue(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    /* printf("In JavaClass_defaultValue()\n"); */
    return JavaClass_convert(cx, obj, JSTYPE_STRING, vp);
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaClass_newEnumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
                       jsval *statep, jsid *idp)
{
    JavaMemberDescriptor *member_descriptor;
    JavaClassDescriptor *class_descriptor;
    JNIEnv *jEnv;
    JSJavaThreadState *jsj_env;
    
    class_descriptor = JS_GetPrivate(cx, obj);

    /* Check for prototype JavaClass object */
    if (!class_descriptor) {
        *statep = JSVAL_NULL;
        if (idp)
            *idp = INT_TO_JSVAL(0);
        return JS_TRUE;
    }
    
    switch(enum_op) {
    case JSENUMERATE_INIT:
        /* Get the Java per-thread environment pointer for this JSContext */
        jsj_env = jsj_EnterJava(cx, &jEnv);
        if (!jEnv)
            return JS_FALSE;
        member_descriptor = jsj_GetClassStaticMembers(cx, jEnv, class_descriptor);
        *statep = PRIVATE_TO_JSVAL(member_descriptor);
        if (idp)
            *idp = INT_TO_JSVAL(class_descriptor->num_instance_members);
	jsj_ExitJava(jsj_env);
        return JS_TRUE;
        
    case JSENUMERATE_NEXT:
        member_descriptor = JSVAL_TO_PRIVATE(*statep);
        if (member_descriptor) {

            /* Don't enumerate explicit-signature methods, i.e. enumerate toValue,
               but not toValue(int), toValue(double), etc. */
            while (member_descriptor->methods && member_descriptor->methods->is_alias) {
                member_descriptor = member_descriptor->next;
                if (!member_descriptor) {
                    *statep = JSVAL_NULL;
                    return JS_TRUE;
                }
            }

            *idp = member_descriptor->id;
            *statep = PRIVATE_TO_JSVAL(member_descriptor->next);
            return JS_TRUE;
        }
        /* Fall through ... */

    case JSENUMERATE_DESTROY:
        *statep = JSVAL_NULL;
        return JS_TRUE;

    default:
        JS_ASSERT(0);
        return JS_FALSE;
    }
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaClass_checkAccess(JSContext *cx, JSObject *obj, jsid id,
                      JSAccessMode mode, jsval *vp, uintN *attrsp)
{
    switch (mode) {
    case JSACC_WATCH:
        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                            JSJMSG_JCLASS_PROP_WATCH);
        return JS_FALSE;

    case JSACC_IMPORT:
        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                            JSJMSG_JCLASS_PROP_EXPORT);
        return JS_FALSE;

    default:
        return JS_TRUE;
    }
}

/*
 * Implement the JavaScript instanceof operator for JavaClass objects by using
 * the equivalent Java instanceof operation.
 */
JS_STATIC_DLL_CALLBACK(JSBool)
JavaClass_hasInstance(JSContext *cx, JSObject *obj, jsval candidate_jsval,
                      JSBool *has_instancep)
{
    JavaClassDescriptor *class_descriptor;
    JavaObjectWrapper *java_wrapper;
    JSClass *js_class;
    JSBool has_instance;
    JSObject *candidate_obj;
    jclass java_class;
    jobject java_obj;
    JNIEnv *jEnv;
    JSJavaThreadState *jsj_env;
    
    has_instance = JS_FALSE;
    class_descriptor = JS_GetPrivate(cx, obj);
    if (!class_descriptor) {
        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                                    JSJMSG_BAD_OP_JCLASS);
        return JS_FALSE;
    }

    /*
     * Make sure that the thing to the left of the instanceof operator is a
     * Java object.
     */
    if (!JSVAL_IS_OBJECT(candidate_jsval))
        goto done;
    candidate_obj = JSVAL_TO_OBJECT(candidate_jsval);
#ifdef JS_THREADSAFE
    js_class = JS_GetClass(cx, candidate_obj);
#else
    js_class = JS_GetClass(candidate_obj);
#endif
    if ((js_class != &JavaObject_class) && (js_class != &JavaArray_class))
        goto done;

    java_class = class_descriptor->java_class;
    java_wrapper = JS_GetPrivate(cx, candidate_obj);
    if (!java_wrapper) {
        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                                JSJMSG_BAD_OP_PROTO);
        return JS_FALSE;
    }
    java_obj = java_wrapper->java_obj;
    /* Get JNI pointer */
    jsj_env = jsj_EnterJava(cx, &jEnv);
    has_instance = (*jEnv)->IsInstanceOf(jEnv, java_obj, java_class);
    jsj_ExitJava(jsj_env);

done:
    *has_instancep = has_instance;
    return JS_TRUE;
}

JSObjectOps JavaClass_ops = {
    /* Mandatory non-null function pointer members. */
    jsj_wrapper_newObjectMap,       /* newObjectMap */
    jsj_wrapper_destroyObjectMap,   /* destroyObjectMap */
    JavaClass_lookupProperty,
    JavaClass_defineProperty,
    JavaClass_getPropertyById,      /* getProperty */
    JavaClass_setPropertyById,      /* setProperty */
    JavaClass_getAttributes,
    JavaClass_setAttributes,
    JavaClass_deleteProperty,
    JavaClass_defaultValue,
    JavaClass_newEnumerate,
    JavaClass_checkAccess,

    /* Optionally non-null members start here. */
    NULL,                           /* thisObject */
    NULL,                           /* dropProperty */
    jsj_JavaConstructorWrapper,     /* call */
    jsj_JavaConstructorWrapper,     /* construct */
    NULL,                           /* xdrObject */
    JavaClass_hasInstance,          /* hasInstance */
    NULL,                           /* setProto */
    NULL,                           /* setParent */
    NULL,                           /* mark */
    NULL,                           /* clear */
    jsj_wrapper_getRequiredSlot,    /* getRequiredSlot */
    jsj_wrapper_setRequiredSlot     /* setRequiredSlot */
};

JS_STATIC_DLL_CALLBACK(JSObjectOps *)
JavaClass_getObjectOps(JSContext *cx, JSClass *clazz)
{
    return &JavaClass_ops;
}

JSClass JavaClass_class = {
    "JavaClass", JSCLASS_HAS_PRIVATE,
    NULL, NULL, NULL, NULL,
    NULL, NULL, JavaClass_convert, JavaClass_finalize,

    /* Optionally non-null members start here. */
    JavaClass_getObjectOps,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
};

static JSObject *
jsj_new_JavaClass(JSContext *cx, JNIEnv *jEnv, JSObject* parent_obj,
                  JavaClassDescriptor *class_descriptor)
{
    JSObject *JavaClass_obj;

    JavaClass_obj = JS_NewObject(cx, &JavaClass_class, 0, parent_obj);
    if (!JavaClass_obj)
        return NULL;

    JS_SetPrivate(cx, JavaClass_obj, (void *)class_descriptor);

#ifdef DEBUG
    /*    printf("JavaClass \'%s\' created\n", class_descriptor->name); */
#endif

    return JavaClass_obj;
}

JSObject *
jsj_define_JavaClass(JSContext *cx, JNIEnv *jEnv, JSObject* parent_obj,
                     const char *simple_class_name,
                     jclass java_class)
{
    JavaClassDescriptor *class_descriptor;
    JSObject *JavaClass_obj;

    class_descriptor = jsj_GetJavaClassDescriptor(cx, jEnv, java_class);
    if (!class_descriptor)
        return NULL;

    JavaClass_obj = jsj_new_JavaClass(cx, jEnv, parent_obj, class_descriptor);
    if (!JavaClass_obj)
        return NULL;

    if (!JS_DefineProperty(cx, parent_obj, simple_class_name,
                           OBJECT_TO_JSVAL(JavaClass_obj), 0, 0,
                           JSPROP_PERMANENT|JSPROP_READONLY|JSPROP_ENUMERATE))
        return NULL;
    return JavaClass_obj;
}


/*
 * The getClass() native JS method is defined as a property of the global
 * object.  Given a JavaObject it returns the corresponding JavaClass.  This
 * is useful for accessing static methods and fields.
 *
 *    js> getClass(new java.lang.String("foo"))
 *    [JavaClass java.lang.String]
 */
JS_STATIC_DLL_CALLBACK(JSBool)
getClass(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *obj_arg, *JavaClass_obj;
    JavaObjectWrapper *java_wrapper;
    JavaClassDescriptor *class_descriptor;
    JNIEnv *jEnv;
    JSJavaThreadState *jsj_env;

    if (argc != 1 ||
    !JSVAL_IS_OBJECT(argv[0]) ||
    !(obj_arg = JSVAL_TO_OBJECT(argv[0])) ||
    (!JS_InstanceOf(cx, obj_arg, &JavaObject_class, 0) &&
         !JS_InstanceOf(cx, obj_arg, &JavaArray_class, 0))) {
        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                                JSJMSG_NEED_JOBJECT_ARG);
        return JS_FALSE;
    }

    java_wrapper = JS_GetPrivate(cx, obj_arg);
    if (!java_wrapper) {
        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                                JSJMSG_PROTO_GETCLASS);
        return JS_FALSE;
    }

    jsj_env = jsj_EnterJava(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;
    
    class_descriptor = java_wrapper->class_descriptor;

    JavaClass_obj = jsj_new_JavaClass(cx, jEnv, NULL, class_descriptor);
    if (!JavaClass_obj) {
	jsj_ExitJava(jsj_env);
        return JS_FALSE;
    }

    *rval = OBJECT_TO_JSVAL(JavaClass_obj);
    jsj_ExitJava(jsj_env);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaClass_construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *obj_arg, *JavaClass_obj;
    JavaObjectWrapper *java_wrapper;
    JavaClassDescriptor *class_descriptor;
    JNIEnv *jEnv;
    JSJavaThreadState *jsj_env;

    if (argc != 1 ||
	!JSVAL_IS_OBJECT(argv[0]) ||
	!(obj_arg = JSVAL_TO_OBJECT(argv[0])) ||
	!JS_InstanceOf(cx, obj_arg, &JavaObject_class, 0) ||
	((java_wrapper = JS_GetPrivate(cx, obj_arg)) == NULL)) {
        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                             JSJMSG_NEED_JCLASS_ARG);
        return JS_FALSE;
    }

    jsj_env = jsj_EnterJava(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;

    class_descriptor = java_wrapper->class_descriptor;
    if (!(*jEnv)->IsSameObject(jEnv, class_descriptor->java_class, jlClass)) {
	JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                             JSJMSG_NEED_JCLASS_ARG);
	jsj_ExitJava(jsj_env);
        return JS_FALSE;
    }

    class_descriptor = jsj_GetJavaClassDescriptor(cx, jEnv, java_wrapper->java_obj);
    JavaClass_obj = jsj_new_JavaClass(cx, jEnv, NULL, class_descriptor);
    if (!JavaClass_obj) {
	jsj_ExitJava(jsj_env);
        return JS_FALSE;
    }

    *rval = OBJECT_TO_JSVAL(JavaClass_obj);
    jsj_ExitJava(jsj_env);
    return JS_TRUE;
}

extern JS_IMPORT_DATA(JSObjectOps) js_ObjectOps;

JSBool
jsj_init_JavaClass(JSContext *cx, JSObject *global_obj)
{
    /* Define JavaClass class */
    if (!JS_InitClass(cx, global_obj, 0, &JavaClass_class, JavaClass_construct, 0, 0, 0, 0, 0))
        return JS_FALSE;

    if (!JS_DefineFunction(cx, global_obj, "getClass", getClass, 0,
                           JSPROP_READONLY))
        return JS_FALSE;

    return jsj_InitJavaClassReflectionsTable();
}

