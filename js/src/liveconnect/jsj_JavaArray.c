/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * It contains the definition of the JavaScript JavaArray class.
 * Instances of JavaArray are used to reflect Java arrays.
 */

#include <stdlib.h>
#include <string.h>

#include "jsj_private.h"      /* LiveConnect internals */

/* Shorthands for ASCII (7-bit) decimal and hex conversion. */
#define JS7_ISDEC(c)    (((c) >= '0') && ((c) <= '9'))
#define JS7_UNDEC(c)    ((c) - '0')

/*
 * Convert any jsval v to an integer jsval if ToString(v)
 * contains a base-10 integer that fits into 31 bits.
 * Otherwise return v.
 */
static jsval
try_convert_to_jsint(JSContext *cx, jsval idval)
{
    const jschar *cp;
    JSString *jsstr;
    
    jsstr = JS_ValueToString(cx, idval);
    if (!jsstr)
        return idval;

    cp = JS_GetStringChars(jsstr);
    if (JS7_ISDEC(*cp)) {
        jsuint index = JS7_UNDEC(*cp++);
        jsuint oldIndex = 0;
        jsuint c = 0;
        if (index != 0) {
            while (JS7_ISDEC(*cp)) {
                oldIndex = index;
                c = JS7_UNDEC(*cp);
                index = 10*index + c;
                cp++;
            }
        }
        if (*cp == 0 &&
            (oldIndex < (JSVAL_INT_MAX / 10) ||
            (oldIndex == (JSVAL_INT_MAX / 10) && c < (JSVAL_INT_MAX % 10)))) {
            return INT_TO_JSVAL(index);
        }
    }
    return idval;
}


static JSBool
access_java_array_element(JSContext *cx,
                          JNIEnv *jEnv,
                          JSObject *obj,
                          jsid id,
                          jsval *vp,
                          JSBool do_assignment)
{
    jsval idval;
    jarray java_array;
    JavaClassDescriptor *class_descriptor;
    JavaObjectWrapper *java_wrapper;
    jsize array_length, index;
    JavaSignature *array_component_signature;
    
    /* printf("In JavaArray_getProperty\n"); */
    
    java_wrapper = JS_GetPrivate(cx, obj);
    if (!java_wrapper) {
        const char *property_name;
        if (JS_IdToValue(cx, id, &idval) && JSVAL_IS_STRING(idval) &&
            (property_name = JS_GetStringBytes(JSVAL_TO_STRING(idval))) != NULL) {
            if (!strcmp(property_name, "constructor")) {
                if (vp)
                    *vp = JSVAL_VOID;
                return JS_TRUE;
            }
        }
        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                                JSJMSG_BAD_OP_JARRAY);
        return JS_FALSE;
    }
    class_descriptor = java_wrapper->class_descriptor;
    java_array = java_wrapper->java_obj;
    
    JS_ASSERT(class_descriptor->type == JAVA_SIGNATURE_ARRAY);

    JS_IdToValue(cx, id, &idval);

    if (!JSVAL_IS_INT(idval))
        idval = try_convert_to_jsint(cx, idval);

    if (!JSVAL_IS_INT(idval)) {
        /*
         * Usually, properties of JavaArray objects are indexed by integers, but
         * Java arrays also inherit all the methods of java.lang.Object, so a
         * string-valued property is also possible.
         */
        if (JSVAL_IS_STRING(idval)) {
            const char *member_name;
            
            member_name = JS_GetStringBytes(JSVAL_TO_STRING(idval));
            
            if (do_assignment) {
                JSVersion version = JS_GetVersion(cx);

                if (!JSVERSION_IS_ECMA(version)) {
 
                    JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                        JSJMSG_CANT_WRITE_JARRAY, member_name);
                    return JS_FALSE;
                } else {
                    if (vp)
                        *vp = JSVAL_VOID;
                    return JS_TRUE;
                }
            } else {
                if (!strcmp(member_name, "length")) {
                    array_length = jsj_GetJavaArrayLength(cx, jEnv, java_array);
                    if (array_length < 0)
                        return JS_FALSE;
                    if (vp)
                        *vp = INT_TO_JSVAL(array_length);
                    return JS_TRUE;
                }
                
                /* Check to see if we're reflecting a Java array method */
                return JavaObject_getPropertyById(cx, obj, id, vp);
            }
        }

        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                            JSJMSG_BAD_INDEX_EXPR);
        return JS_FALSE;
    }
    
    index = JSVAL_TO_INT(idval);

#if 0
    array_length = jsj_GetJavaArrayLength(cx, jEnv, java_array);
    if (array_length < 0)
        return JS_FALSE;

    /* Just let Java throw an exception instead of checking array bounds here */
    if (index < 0 || index >= array_length) {
        char numBuf[12];
        sprintf(numBuf, "%d", index);
        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL,
                                            JSJMSG_BAD_JARRAY_INDEX, numBuf);
        return JS_FALSE;
    }
#endif

    array_component_signature = class_descriptor->array_component_signature;

    if (!vp)
        return JS_TRUE;

    if (do_assignment) {
        return jsj_SetJavaArrayElement(cx, jEnv, java_array, index,
                                       array_component_signature, *vp);
    } else {
        return jsj_GetJavaArrayElement(cx, jEnv, java_array, index,
                                       array_component_signature, vp);
    }
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaArray_getPropertyById(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JNIEnv *jEnv;
    JSJavaThreadState *jsj_env;
    JSBool result;

    jsj_env = jsj_EnterJava(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;
    result = access_java_array_element(cx, jEnv, obj, id, vp, JS_FALSE);
    jsj_ExitJava(jsj_env);
    return result;
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaArray_setPropertyById(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JNIEnv *jEnv;
    JSJavaThreadState *jsj_env;
    JSBool result;
    
    jsj_env = jsj_EnterJava(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;
    result = access_java_array_element(cx, jEnv, obj, id, vp, JS_TRUE);
    jsj_ExitJava(jsj_env);
    return result;
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaArray_lookupProperty(JSContext *cx, JSObject *obj, jsid id,
                         JSObject **objp, JSProperty **propp
#if defined JS_THREADSAFE && defined DEBUG
                            , const char *file, uintN line
#endif
                            )
{
    JNIEnv *jEnv;
    JSErrorReporter old_reporter;
    JSJavaThreadState *jsj_env;

    jsj_env = jsj_EnterJava(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;

    old_reporter = JS_SetErrorReporter(cx, NULL);
    if (access_java_array_element(cx, jEnv, obj, id, NULL, JS_FALSE)) {
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
JavaArray_defineProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                         JSPropertyOp getter, JSPropertyOp setter,
                         uintN attrs, JSProperty **propp)
{
    jsval *vp = &value;
    if (propp)
        return JS_FALSE;
    if (attrs & ~(JSPROP_PERMANENT|JSPROP_ENUMERATE))
        return JS_FALSE;

    return JavaArray_setPropertyById(cx, obj, id, vp);
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaArray_getAttributes(JSContext *cx, JSObject *obj, jsid id,
                        JSProperty *prop, uintN *attrsp)
{
    /* We don't maintain JS property attributes for Java class members */
    *attrsp = JSPROP_PERMANENT|JSPROP_ENUMERATE;
    return JS_FALSE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaArray_setAttributes(JSContext *cx, JSObject *obj, jsid id,
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
JavaArray_deleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSVersion version = JS_GetVersion(cx);

    *vp = JSVAL_FALSE;
    
    if (!JSVERSION_IS_ECMA(version)) {
        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                            JSJMSG_JARRAY_PROP_DELETE);
        return JS_FALSE;
    } else {
        /* Attempts to delete permanent properties are silently ignored
           by ECMAScript. */
        return JS_TRUE;
    }
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaArray_defaultValue(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    /* printf("In JavaArray_defaultValue()\n"); */
    return JavaObject_convert(cx, obj, JSTYPE_STRING, vp);
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaArray_newEnumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
                       jsval *statep, jsid *idp)
{
    JavaObjectWrapper *java_wrapper;
    JSJavaThreadState *jsj_env;
    JNIEnv *jEnv;
    jsize array_length, index;

    java_wrapper = JS_GetPrivate(cx, obj);
    /* Check for prototype object */
    if (!java_wrapper) {
        *statep = JSVAL_NULL;
        if (idp)
            *idp = INT_TO_JSVAL(0);
        return JS_TRUE;
    }
        
    /* Get the Java per-thread environment pointer for this JSContext */
    jsj_env = jsj_EnterJava(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;

    array_length = jsj_GetJavaArrayLength(cx, jEnv, java_wrapper->java_obj);
    if (array_length < 0) {
	jsj_ExitJava(jsj_env);
        return JS_FALSE;
    }

    switch(enum_op) {
    case JSENUMERATE_INIT:
        *statep = INT_TO_JSVAL(0);

        if (idp)
            *idp = INT_TO_JSVAL(array_length);
	jsj_ExitJava(jsj_env);
        return JS_TRUE;
        
    case JSENUMERATE_NEXT:
        index = JSVAL_TO_INT(*statep);
        if (index < array_length) {
            JS_ValueToId(cx, INT_TO_JSVAL(index), idp);
            index++;
            *statep = INT_TO_JSVAL(index);
            return JS_TRUE;
        }

        /* Fall through ... */

    case JSENUMERATE_DESTROY:
        *statep = JSVAL_NULL;
	jsj_ExitJava(jsj_env);
        return JS_TRUE;

    default:
        JS_ASSERT(0);
	jsj_ExitJava(jsj_env);
        return JS_FALSE;
    }
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaArray_checkAccess(JSContext *cx, JSObject *obj, jsid id,
                      JSAccessMode mode, jsval *vp, uintN *attrsp)
{
    switch (mode) {
    case JSACC_WATCH:
        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                            JSJMSG_JARRAY_PROP_WATCH);
        return JS_FALSE;

    case JSACC_IMPORT:
        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                            JSJMSG_JARRAY_PROP_EXPORT);
        return JS_FALSE;

    default:
        return JS_TRUE;
    }
}

JSObjectOps JavaArray_ops = {
    /* Mandatory non-null function pointer members. */
    jsj_wrapper_newObjectMap,       /* newObjectMap */
    jsj_wrapper_destroyObjectMap,   /* destroyObjectMap */
    JavaArray_lookupProperty,
    JavaArray_defineProperty,
    JavaArray_getPropertyById,      /* getProperty */
    JavaArray_setPropertyById,      /* setProperty */
    JavaArray_getAttributes,
    JavaArray_setAttributes,
    JavaArray_deleteProperty,
    JavaArray_defaultValue,
    JavaArray_newEnumerate,
    JavaArray_checkAccess,

    /* Optionally non-null members start here. */
    NULL,                           /* thisObject */
    NULL,                           /* dropProperty */
    NULL,                           /* call */
    NULL,                           /* construct */
    NULL,                           /* xdrObject */
    NULL,                           /* hasInstance */
    NULL,                           /* setProto */
    NULL,                           /* setParent */
    NULL,                           /* mark */
    NULL,                           /* clear */
    jsj_wrapper_getRequiredSlot,    /* getRequiredSlot */
    jsj_wrapper_setRequiredSlot     /* setRequiredSlot */
};

JS_STATIC_DLL_CALLBACK(JSObjectOps *)
JavaArray_getObjectOps(JSContext *cx, JSClass *clazz)
{
    return &JavaArray_ops;
}

JSClass JavaArray_class = {
    "JavaArray", JSCLASS_HAS_PRIVATE,
    NULL, NULL, NULL, NULL,
    NULL, NULL, JavaObject_convert, JavaObject_finalize,

    /* Optionally non-null members start here. */
    JavaArray_getObjectOps,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
};

extern JS_IMPORT_DATA(JSObjectOps) js_ObjectOps;


/* Initialize the JS JavaArray class */
JSBool
jsj_init_JavaArray(JSContext *cx, JSObject *global_obj)
{
    if (!JS_InitClass(cx, global_obj, 
        0, &JavaArray_class, 0, 0,
        0, 0, 0, 0))
        return JS_FALSE;
    
    return JS_TRUE;
}

