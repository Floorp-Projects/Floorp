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

#include "prtypes.h"
#include "prprintf.h"
#include "prassert.h"

#include "jsj_private.h"        /* LiveConnect internals */

static JSBool
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
        name = PR_smprintf("[JavaClass %s]", class_descriptor->name);
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
        return JS_TRUE;
    }
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
            JS_ReportError(cx, "invalid JavaClass property expression. "
                "(methods and fields of a JavaClass object can only be identified by their name)");
            return JS_FALSE;
        }

        member_name = JS_GetStringBytes(JSVAL_TO_STRING(idval));

        /* Why do we have to do this ? */
        if (!strcmp(member_name, "prototype")) {
            *memberp = NULL;
            return JS_TRUE;
        }

        JS_ReportError(cx, "Java class %s has no public static field or method named \"%s\"",
                       class_descriptor->name, member_name);
        return JS_FALSE;
    }
    if (memberp)
        *memberp = member_descriptor;
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
JavaClass_getPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    jsval idval;
    jclass java_class;
    const char *member_name;
    JavaClassDescriptor *class_descriptor;
    JavaMemberDescriptor *member_descriptor;
    JNIEnv *jEnv;

    /* printf("In JavaClass_getProperty\n"); */
    
    /* Get the Java per-thread environment pointer for this JSContext */
    jsj_MapJSContextToJSJThread(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;

    if (!lookup_static_member_by_id(cx, jEnv, obj, &class_descriptor, id, &member_descriptor))
        return JS_FALSE;
    if (!member_descriptor) {
        *vp = JSVAL_VOID;
        return JS_TRUE;
    }

    java_class = class_descriptor->java_class;

    if (member_descriptor->field) {
        if (!member_descriptor->methods) {
            return jsj_GetJavaFieldValue(cx, jEnv, member_descriptor->field, java_class, vp);
        } else {
            PR_ASSERT(0);
        }
    } else {
        JSFunction *function;
        
        /* FIXME - eliminate JSFUN_BOUND_METHOD */
        JS_IdToValue(cx, id, &idval);
        member_name = JS_GetStringBytes(JSVAL_TO_STRING(idval));
        function = JS_NewFunction(cx, jsj_JavaStaticMethodWrapper, 0,
                                  JSFUN_BOUND_METHOD, obj, member_name);
        if (!function)
            return JS_FALSE;

        *vp = OBJECT_TO_JSVAL(JS_GetFunctionObject(function));
        return JS_TRUE;
    }
}

PR_STATIC_CALLBACK(JSBool)
JavaClass_setPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    jclass java_class;
    const char *member_name;
    JavaClassDescriptor *class_descriptor;
    JavaMemberDescriptor *member_descriptor;
    jsval idval;
    JNIEnv *jEnv;

    /* printf("In JavaClass_setProperty\n"); */

    /* Get the Java per-thread environment pointer for this JSContext */
    jsj_MapJSContextToJSJThread(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;
    
    if (!lookup_static_member_by_id(cx, jEnv, obj, &class_descriptor, id, &member_descriptor))
        return JS_FALSE;

    /* Check for the case where there is a method with the given name, but no field
       with that name */
    if (!member_descriptor->field)
        goto no_such_field;

    /* Silently fail if field value is final (immutable), as required by ECMA spec */
    if (member_descriptor->field->modifiers & ACC_FINAL)
        return JS_TRUE;

    java_class = class_descriptor->java_class;
    return jsj_SetJavaFieldValue(cx, jEnv, member_descriptor->field, java_class, *vp);

no_such_field:
    JS_IdToValue(cx, id, &idval);
    member_name = JS_GetStringBytes(JSVAL_TO_STRING(idval));
    JS_ReportError(cx, "No static field named \"%s\" in Java class %s",
                   member_name, class_descriptor->name);
    return JS_FALSE;
}

/*
 * Free the private native data associated with the JavaPackage object.
 */
PR_STATIC_CALLBACK(void)
JavaClass_finalize(JSContext *cx, JSObject *obj)
{
    JNIEnv *jEnv;

    JavaClassDescriptor *class_descriptor = JS_GetPrivate(cx, obj);
    if (!class_descriptor)
        return;
    
    /* Get the Java per-thread environment pointer for this JSContext */
    jsj_MapJSContextToJSJThread(cx, &jEnv);
    if (!jEnv)
        return;

    printf("Finalizing %s\n", class_descriptor->name);
    jsj_ReleaseJavaClassDescriptor(cx, jEnv, class_descriptor);
}


static JSBool
JavaClass_lookupProperty(JSContext *cx, JSObject *obj, jsid id,
                         JSObject **objp, JSProperty **propp
#if defined JS_THREADSAFE && defined DEBUG
                            , const char *file, uintN line
#endif
                            )
{
    JNIEnv *jEnv;

    /* printf("In JavaClass_lookupProperty()\n"); */

    /* Get the Java per-thread environment pointer for this JSContext */
    jsj_MapJSContextToJSJThread(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;

    if (!lookup_static_member_by_id(cx, jEnv, obj, NULL, id, NULL))
        return JS_FALSE;
    *objp = obj;
    *propp = (JSProperty*)1;
    return JS_TRUE;
}

static JSBool
JavaClass_defineProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                         JSPropertyOp getter, JSPropertyOp setter,
                         uintN attrs, JSProperty **propp)
{
    JS_ReportError(cx, "Properties of JavaClass objects may not be deleted");
    return JS_FALSE;
}

static JSBool
JavaClass_getAttributes(JSContext *cx, JSObject *obj, jsid id,
                        JSProperty *prop, uintN *attrsp)
{
    /* We don't maintain JS property attributes for Java class members */
    *attrsp = JSPROP_PERMANENT|JSPROP_ENUMERATE;
    return JS_FALSE;
}

static JSBool
JavaClass_setAttributes(JSContext *cx, JSObject *obj, jsid id,
                        JSProperty *prop, uintN *attrsp)
{
    /* We don't maintain JS property attributes for Java class members */
    if (*attrsp != JSPROP_PERMANENT|JSPROP_ENUMERATE) {
        PR_ASSERT(0);
        return JS_FALSE;
    }

    /* Silently ignore all setAttribute attempts */
    return JS_TRUE;
}

static JSBool
JavaClass_deleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JS_ReportError(cx, "Properties of JavaClass objects may not be deleted");
    return JS_FALSE;
}

static JSBool
JavaClass_defaultValue(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    /* printf("In JavaClass_defaultValue()\n"); */
    return JavaClass_convert(cx, obj, JSTYPE_STRING, vp);
}

static JSBool
JavaClass_newEnumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
                       jsval *statep, jsid *idp)
{
    JavaMemberDescriptor *member_descriptor;
    JavaClassDescriptor *class_descriptor;
    JNIEnv *jEnv;
    
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
        jsj_MapJSContextToJSJThread(cx, &jEnv);
        if (!jEnv)
            return JS_FALSE;
        member_descriptor = jsj_GetClassInstanceMembers(cx, jEnv, class_descriptor);
        *statep = PRIVATE_TO_JSVAL(member_descriptor);
        if (idp)
            *idp = INT_TO_JSVAL(class_descriptor->num_instance_members);
        return JS_TRUE;
        
    case JSENUMERATE_NEXT:
        member_descriptor = JSVAL_TO_PRIVATE(*statep);
        if (member_descriptor) {
            *idp = member_descriptor->id;
            *statep = PRIVATE_TO_JSVAL(member_descriptor->next);
            return JS_TRUE;
        }
        /* Fall through ... */

    case JSENUMERATE_DESTROY:
        *statep = JSVAL_NULL;
        return JS_TRUE;

    default:
        PR_ASSERT(0);
        return JS_FALSE;
    }
}

static JSBool
JavaClass_checkAccess(JSContext *cx, JSObject *obj, jsid id,
                      JSAccessMode mode, jsval *vp, uintN *attrsp)
{
    switch (mode) {
    case JSACC_WATCH:
        JS_ReportError(cx, "Cannot place watchpoints on JavaClass object properties");
        return JS_FALSE;

    case JSACC_IMPORT:
        JS_ReportError(cx, "Cannot export a JavaClass object's properties");
        return JS_FALSE;

    default:
        return JS_TRUE;
    }
}

/*
 * Implement the JavaScript instanceof operator for JavaClass objects by using
 * the equivalent Java instanceof operation.
 */
static JSBool
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
    
    has_instance = JS_FALSE;
    class_descriptor = JS_GetPrivate(cx, obj);
    if (!class_descriptor) {
        JS_ReportError(cx, "illegal operation on JavaClass prototype object");
        return JS_FALSE;
    }

    /*
     * Make sure that the thing to the left of the instanceof operator is a
     * Java object.
     */
    if (!JSVAL_IS_OBJECT(candidate_jsval))
        goto done;
    candidate_obj = JSVAL_TO_OBJECT(candidate_jsval);
    js_class = JS_GetClass(candidate_obj);
    if ((js_class != &JavaObject_class) && (js_class != &JavaArray_class))
        goto done;

    java_class = class_descriptor->java_class;
    java_wrapper = JS_GetPrivate(cx, candidate_obj);
    if (!java_wrapper) {
        JS_ReportError(cx, "illegal operation on prototype object");
        return JS_FALSE;
    }
    java_obj = java_wrapper->java_obj;
    /* Get JNI pointer */
    jsj_MapJSContextToJSJThread(cx, &jEnv);
    has_instance = (*jEnv)->IsInstanceOf(jEnv, java_obj, java_class);

done:
    *has_instancep = has_instance;
    return JS_TRUE;
}

JSObjectOps JavaClass_ops = {
    /* Mandatory non-null function pointer members. */
    NULL,                       /* newObjectMap */
    NULL,                       /* destroyObjectMap */
    JavaClass_lookupProperty,
    JavaClass_defineProperty,
    JavaClass_getPropertyById,  /* getProperty */
    JavaClass_setPropertyById,  /* setProperty */
    JavaClass_getAttributes,
    JavaClass_setAttributes,
    JavaClass_deleteProperty,
    JavaClass_defaultValue,
    JavaClass_newEnumerate,
    JavaClass_checkAccess,

    /* Optionally non-null members start here. */
    NULL,                       /* thisObject */
    NULL,                       /* dropProperty */
    jsj_JavaConstructorWrapper, /* call */
    jsj_JavaConstructorWrapper, /* construct */
    NULL,                       /* xdrObject */
    JavaClass_hasInstance,      /* hasInstance */
};

JSObjectOps *
JavaClass_getObjectOps(JSContext *cx, JSClass *clazz)
{
    return &JavaClass_ops;
}

JSClass JavaClass_class = {
    "JavaClass", JSCLASS_HAS_PRIVATE,
    NULL, NULL, NULL, NULL,
    NULL, NULL, JavaClass_convert, JavaClass_finalize,
    JavaClass_getObjectOps,
};

JSObject *
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
static JSBool
getClass(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *obj_arg, *JavaClass_obj;
    JavaObjectWrapper *java_wrapper;
    JavaClassDescriptor *class_descriptor;
    JNIEnv *jEnv;

    jsj_MapJSContextToJSJThread(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;

    if (argc != 1 ||
	!JSVAL_IS_OBJECT(argv[0]) ||
	!(obj_arg = JSVAL_TO_OBJECT(argv[0])) ||
	(!JS_InstanceOf(cx, obj_arg, &JavaObject_class, 0) &&
         !JS_InstanceOf(cx, obj_arg, &JavaArray_class, 0))) {
        JS_ReportError(cx, "getClass expects a Java object argument");
	return JS_FALSE;
    }

    java_wrapper = JS_GetPrivate(cx, obj_arg);
    if (!java_wrapper) {
        JS_ReportError(cx, "getClass called on prototype object");
        return JS_FALSE;
    }

    class_descriptor = java_wrapper->class_descriptor;

    JavaClass_obj = jsj_new_JavaClass(cx, jEnv, NULL, class_descriptor);
    if (!JavaClass_obj)
        return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(JavaClass_obj);
    return JS_TRUE;
}

extern PR_IMPORT_DATA(JSObjectOps) js_ObjectOps;

JSBool
jsj_init_JavaClass(JSContext *cx, JSObject *global_obj)
{
    JavaClass_ops.newObjectMap = js_ObjectOps.newObjectMap;
    JavaClass_ops.destroyObjectMap = js_ObjectOps.destroyObjectMap;
    
    /* Define JavaClass class */
    if (!JS_InitClass(cx, global_obj, 0, &JavaClass_class, 0, 0, 0, 0, 0, 0))
        return JS_FALSE;

    if (!JS_DefineFunction(cx, global_obj, "getClass", getClass, 0,
                           JSPROP_READONLY))
        return JS_FALSE;

    return jsj_InitJavaClassReflectionsTable();
}

