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
 * Below is the code that converts between Java and JavaScript values of all
 * types.
 */

#include <stdlib.h>
#include <string.h>

#include "prtypes.h"
#include "prprintf.h"
#include "prassert.h"

#include "jsj_private.h"      /* LiveConnect internals */


static JSBool
convert_js_obj_to_JSObject_wrapper(JSContext *cx, JNIEnv *jEnv, JSObject *js_obj,
                                   JavaSignature *signature,
                                   int *cost, jobject *java_value)
{
    if (!njJSObject) {
        if (java_value)
            JS_ReportError(cx, "Couldn't convert JavaScript object to an "
                               "instance of netscape.javascript.JSObject "
                               "because that class could not be loaded.");
        return JS_FALSE;
    }

    if (!(*jEnv)->IsAssignableFrom(jEnv, njJSObject, signature->java_class))
        return JS_FALSE;

    if (!java_value)
        return JS_TRUE;

    *java_value = jsj_WrapJSObject(cx, jEnv, js_obj);

    return (*java_value != NULL);   
}

jstring
jsj_ConvertJSStringToJavaString(JSContext *cx, JNIEnv *jEnv, JSString *js_str)
{
    jstring result;
    result = (*jEnv)->NewString(jEnv, JS_GetStringChars(js_str),
                                JS_GetStringLength(js_str));
    if (!result) {
        jsj_UnexpectedJavaError(cx, jEnv, "Couldn't construct instance "
                                          "of java.lang.String");
    }
    return result;
}

/*
 * Convert a JS value to an instance of java.lang.Object or one of its subclasses, 
 * performing any necessary type coercion.  If non-trivial coercion is required,
 * the cost value is incremented.  If the java_value pass-by-reference argument
 * is non-NULL, the resulting Java value is stored there.
 *
 * Returns JS_TRUE if the conversion is possible, JS_FALSE otherwise
 */
JSBool
jsj_ConvertJSValueToJavaObject(JSContext *cx, JNIEnv *jEnv, jsval v, JavaSignature *signature,
                               int *cost, jobject *java_value)
{
    JSString *jsstr;
    jclass target_java_class;
    
    PR_ASSERT(signature->type == JAVA_SIGNATURE_CLASS ||
        signature->type == JAVA_SIGNATURE_ARRAY);
    
    /* Get the Java type of the target value */
    target_java_class = signature->java_class;
    
    if (JSVAL_IS_OBJECT(v)) {
        JSObject *js_obj = JSVAL_TO_OBJECT(v);
        
        /* JS null is always assignable to a Java object */
        if (!js_obj) {
            if (java_value)
                *java_value = NULL;
            return JS_TRUE;
        }
        
        if (JS_InstanceOf(cx, js_obj, &JavaObject_class, 0) ||
            JS_InstanceOf(cx, js_obj, &JavaArray_class, 0)) {
            
            /* The source value is a Java object wrapped inside a JavaScript
               object.  Unwrap the JS object and return the original Java
               object if it's class makes it assignment-compatible with the
               target class using Java's assignability rules. */
            JavaObjectWrapper *java_wrapper = JS_GetPrivate(cx, js_obj);
            jobject java_obj = java_wrapper->java_obj;
            
            if ((*jEnv)->IsInstanceOf(jEnv, java_obj, target_java_class)) {
                if (java_value)
                    *java_value = java_obj;
                return JS_TRUE;
            }
            
#ifdef LIVECONNECT_IMPROVEMENTS
            /* Don't allow wrapped Java objects to be converted to strings */
            goto conversion_error;
#else
            /* Fall through, to attempt conversion to a Java string */
#endif
            
        } else if (JS_InstanceOf(cx, js_obj, &JavaClass_class, 0)) {
            /* We're dealing with the reflection of a Java class */
            JavaClassDescriptor *java_class_descriptor = JS_GetPrivate(cx, js_obj);
            
            /* Check if target type is java.lang.Class class */
            if ((*jEnv)->IsAssignableFrom(jEnv, jlClass, target_java_class)) {
                if (java_value)
                    *java_value = java_class_descriptor->java_class;
                return JS_TRUE;
            }
            
            /* Check if target type is netscape.javascript.JSObject wrapper class */
            if (convert_js_obj_to_JSObject_wrapper(cx, jEnv, js_obj, signature, cost, java_value))
                return JS_TRUE;
            
            /* Fall through, to attempt conversion to a Java string */
            
        } else if (JS_TypeOfValue(cx, v) == JSTYPE_FUNCTION) {
            /* JS functions can be wrapped as a netscape.javascript.JSObject */
            if (convert_js_obj_to_JSObject_wrapper(cx, jEnv, js_obj, signature, cost, java_value))
                return JS_TRUE;
            
            /* That didn't work, so fall through, to attempt conversion to
               a java.lang.String ... */
            
            /* Check for a Java object wrapped inside a JS object */
        } else {
            /* Otherwise, see if the target type is the  netscape.javascript.JSObject
               wrapper class or one of its subclasses, in which case a
               reference is passed to the original JS object by wrapping it
               inside an instance of netscape.javascript.JSObject */
            if (convert_js_obj_to_JSObject_wrapper(cx, jEnv, js_obj, signature, cost, java_value))
                return JS_TRUE;
            
            /* Fall through, to attempt conversion to a Java string */
        }
        
    } else if (JSVAL_IS_NUMBER(v)) {
        /* JS numbers, integral or not, can be converted to instances of java.lang.Double */
        if ((*jEnv)->IsAssignableFrom(jEnv, jlDouble, target_java_class)) {
            if (java_value) {
                jsdouble d;
                if (!JS_ValueToNumber(cx, v, &d))
                    goto conversion_error;
                *java_value = (*jEnv)->NewObject(jEnv, jlDouble, jlDouble_Double, d);
                if (!*java_value) {
                    jsj_UnexpectedJavaError(cx, jEnv,
                        "Couldn't construct instance of java.lang.Double");
                    return JS_FALSE;
                }
            }
#ifdef LIVECONNECT_IMPROVEMENTS
            (*cost)++;
#endif
            return JS_TRUE;
        }
        /* Fall through, to attempt conversion to a java.lang.String ... */
        
    } else if (JSVAL_IS_BOOLEAN(v)) {
        /* JS boolean values can be converted to instances of java.lang.Boolean */
        if ((*jEnv)->IsAssignableFrom(jEnv, jlBoolean, target_java_class)) {
            if (java_value) {
                JSBool b;
                if (!JS_ValueToBoolean(cx, v, &b))
                    goto conversion_error;
                *java_value =
                    (*jEnv)->NewObject(jEnv, jlBoolean, jlBoolean_Boolean, b);
                if (!*java_value) {
                    jsj_UnexpectedJavaError(cx, jEnv, "Couldn't construct instance " 
                        "of java.lang.Boolean");
                    return JS_FALSE;
                }
            }
#ifdef LIVECONNECT_IMPROVEMENTS
            (*cost)++;
#endif
            return JS_TRUE;
        }
        /* Fall through, to attempt conversion to a java.lang.String ... */
    }
    
    /* If no other conversion is possible, see if the target type is java.lang.String */
    if ((*jEnv)->IsAssignableFrom(jEnv, jlString, target_java_class)) {
        JSBool is_string = JSVAL_IS_STRING(v);
        
        /* Convert to JS string, if necessary, and then to a Java Unicode string */
        jsstr = JS_ValueToString(cx, v);
        if (jsstr) {
            if (java_value) {
                *java_value = jsj_ConvertJSStringToJavaString(cx, jEnv, jsstr);
                if (!*java_value)
                    return JS_FALSE;
            }
#ifdef LIVECONNECT_IMPROVEMENTS
            if (!is_string)
                (*cost)++;
#endif
            return JS_TRUE;
        }
    }
    
conversion_error:
    return JS_FALSE;
}

/* Utility macro for jsj_ConvertJSValueToJavaValue(), below */
#define JSVAL_TO_INTEGRAL_JVALUE(type_name, member_name, member_type, jsval, java_value) \
if (!JSVAL_IS_NUMBER(v)) {                                               \
        if (!JS_ConvertValue(cx, v, JSTYPE_NUMBER, &v))                  \
            goto conversion_error;                                       \
        (*cost)++;                                                       \
    }                                                                    \
    {                                                                    \
        member_type member_name;                                         \
                                                                         \
        if (JSVAL_IS_INT(v)) {                                           \
            jsint ival = JSVAL_TO_INT(v);                                \
            member_name = (member_type) ival;                            \
                                                                         \
            /* Check to see if the jsval's magnitude is too large to be  \
               representable in the target java type */                  \
            if (member_name != ival)                                     \
                goto conversion_error;                                   \
        } else {                                                         \
            jdouble dval = *JSVAL_TO_DOUBLE(v);                          \
            member_name = (member_type) dval;                            \
                                                                         \
            /* Don't allow a non-integral number */                      \
            /* FIXME - should this be an error ? */                      \
            if ((jdouble)member_name != dval)                            \
                (*cost)++;                                               \
        }                                                                \
        if (java_value)                                                  \
            java_value->member_name = member_name;                       \
    }

#if XP_MAC

// on MRJ jlong is typedef'd to wide, which is a struct.
#include <Math64.h>

static jsint jlong_to_jsint(jlong lvalue)
{
    SInt64 val = WideToSInt64(lvalue);
    return S32Set(val);
}

static jlong jsint_to_jlong(jsint ivalue)
{
    SInt64 val = S64Set(ivalue);
    return SInt64ToWide(val);
}

static jdouble jlong_to_jdouble(lvalue)
{
    SInt64 val = WideToSInt64(lvalue);
    return SInt64ToLongDouble(val);
}

static jlong jdouble_to_jlong(jdouble dvalue)
{
    SInt64 val = LongDoubleToSInt64(dvalue);
    return SInt64ToWide(val);
}

/* Mac utility macro for jsj_ConvertJSValueToJavaValue(), below */
#define JSVAL_TO_JLONG_JVALUE(member_name, member_type, jsvalue, java_value) \
if (!JSVAL_IS_NUMBER(jsvalue)) {                                         \
        if (!JS_ConvertValue(cx, jsvalue, JSTYPE_NUMBER, &jsvalue))      \
        goto conversion_error;                                           \
    (*cost)++;                                                           \
    }                                                                    \
    {                                                                    \
        member_type member_name;                                         \
                                                                         \
        if (JSVAL_IS_INT(jsvalue)) {                                     \
            jsint ival = JSVAL_TO_INT(jsvalue);                          \
            member_name = jsint_to_jlong(ival);                          \
                                                                         \
        } else {                                                         \
            jdouble dval = *JSVAL_TO_DOUBLE(jsvalue);                    \
            member_name = jdouble_to_jlong(dval);                        \
                                                                         \
            /* Don't allow a non-integral number */                      \
            /* Should this be an error ? */                              \
            if (jlong_to_jdouble(member_name) != dval)                   \
                (*cost)++;                                               \
        }                                                                \
        if (java_value)                                                  \
            java_value->member_name = member_name;                       \
    }

#else

#define jlong_to_jdouble(lvalue) ((jdouble) lvalue)

#endif

/*
 * Convert a JS value to a Java value of the given type signature.  The cost
 * variable is incremented if coercion is required, e.g. the source value is
 * a string, but the target type is a boolean.
 *
 * Returns JS_FALSE if no conversion is possible, either because the jsval has
 * a type that is wholly incompatible with the Java value, or because a scalar
 * jsval can't be represented in a variable of the target type without loss of
 * precision, e.g. the source value is "4.2" but the destination type is byte.
 * If conversion is not possible and java_value is non-NULL, the JS error
 * reporter is called with an appropriate message.
 */  
JSBool
jsj_ConvertJSValueToJavaValue(JSContext *cx, JNIEnv *jEnv, jsval v,
                              JavaSignature *signature,
                              int *cost, jvalue *java_value)
{
    JavaSignatureChar type = signature->type;

    switch (type) {
    case JAVA_SIGNATURE_BOOLEAN:
        if (!JSVAL_IS_BOOLEAN(v)) {
            if (!JS_ConvertValue(cx, v, JSTYPE_BOOLEAN, &v))
                goto conversion_error;
            (*cost)++;
        }
        if (java_value)
            java_value->z = (jboolean)(JSVAL_TO_BOOLEAN(v) == JS_TRUE);
        break;

    case JAVA_SIGNATURE_SHORT:
        JSVAL_TO_INTEGRAL_JVALUE(short, s, jshort, v, java_value);
        break;

    case JAVA_SIGNATURE_BYTE:
        JSVAL_TO_INTEGRAL_JVALUE(byte, b, jbyte, v, java_value);
        break;

    case JAVA_SIGNATURE_CHAR:
        /* A one-character string can be converted into a character */
        if (JSVAL_IS_STRING(v) && (JS_GetStringLength(JSVAL_TO_STRING(v)) == 1)) {
            v = INT_TO_JSVAL(*JS_GetStringChars(JSVAL_TO_STRING(v)));
        }
        JSVAL_TO_INTEGRAL_JVALUE(char, c, jchar, v, java_value);
        break;

    case JAVA_SIGNATURE_INT:
        JSVAL_TO_INTEGRAL_JVALUE(int, i, jint, v, java_value);
        break;

    case JAVA_SIGNATURE_LONG:
#if XP_MAC
        JSVAL_TO_JLONG_JVALUE(j, jlong, v, java_value);
#else
        JSVAL_TO_INTEGRAL_JVALUE(long, j, jlong, v, java_value);
#endif
        break;
    
    case JAVA_SIGNATURE_FLOAT:
        if (!JSVAL_IS_NUMBER(v)) {
            if (!JS_ConvertValue(cx, v, JSTYPE_NUMBER, &v))
                goto conversion_error;
            (*cost)++;
        }
        if (java_value) {
            if (JSVAL_IS_INT(v))
                java_value->f = (jfloat) JSVAL_TO_INT(v);
            else
                java_value->f = (jfloat) *JSVAL_TO_DOUBLE(v);
        }
        break;

    case JAVA_SIGNATURE_DOUBLE:
        if (!JSVAL_IS_NUMBER(v)) {
            if (!JS_ConvertValue(cx, v, JSTYPE_NUMBER, &v))
                goto conversion_error;
            (*cost)++;
        }
        if (java_value) {
            if (JSVAL_IS_INT(v))
                java_value->d = (jdouble) JSVAL_TO_INT(v);
            else
                java_value->d = (jdouble) *JSVAL_TO_DOUBLE(v);
        }
        break;

    case JAVA_SIGNATURE_CLASS:
    case JAVA_SIGNATURE_ARRAY:
        if (!jsj_ConvertJSValueToJavaObject(cx, jEnv, v, signature, cost, &java_value->l))
            goto conversion_error;
        break;

    default:
        PR_ASSERT(0);
        return JS_FALSE;
    }

    /* Success */
    return JS_TRUE;

conversion_error:

    if (java_value) {
        const char *jsval_string;
        JSString *jsstr;

        jsval_string = NULL;
        jsstr = JS_ValueToString(cx, v);
        if (jsstr)
            jsval_string = JS_GetStringBytes(jsstr);
        if (!jsval_string)
            jsval_string = "";
        
        JS_ReportError(cx, "Unable to convert JavaScript value %s to "
                           "Java value of type %s",
                       jsval_string, signature->name);
    }
    return JS_FALSE;
}

/*
 * A utility routine to create a JavaScript Unicode string from a
 * java.lang.String (Unicode) string.
 */
JSString *
jsj_ConvertJavaStringToJSString(JSContext *cx, JNIEnv *jEnv, jstring java_str)
{
    JSString *js_str;
    jboolean is_copy;
    const jchar *ucs2_str;
    jchar *copy_ucs2_str;
    jsize ucs2_str_len, num_bytes;

    ucs2_str_len = (*jEnv)->GetStringLength(jEnv, java_str);
    ucs2_str = (*jEnv)->GetStringChars(jEnv, java_str, &is_copy);
    if (!ucs2_str) {
        jsj_UnexpectedJavaError(cx, jEnv,
                                "Unable to extract native Unicode from Java string");
        return NULL;
    }

    js_str = NULL;

    /* Unlike JS_NewString(), the string data passed into JS_NewUCString() is
       not copied, so make a copy of the Unicode character vector. */
    num_bytes = ucs2_str_len * sizeof(jchar);
    copy_ucs2_str = (jchar*)JS_malloc(cx, num_bytes);
    if (!copy_ucs2_str)
        goto done;
    memcpy(copy_ucs2_str, ucs2_str, num_bytes);

    js_str = JS_NewUCString(cx, (jschar*)copy_ucs2_str, ucs2_str_len);

done:
    (*jEnv)->ReleaseStringChars(jEnv, java_str, ucs2_str);
    return js_str;
}

/*
 * Attempt to obtain a JS string representation of a Java object.
 * The java_obj argument must be of type java.lang.Object or a subclass.
 * If java_obj is a Java string, it's value is simply extracted and
 * copied into a JS string.  Otherwise, the toString() method is called
 * on java_obj.
 */
JSBool
jsj_ConvertJavaObjectToJSString(JSContext *cx,
                                JNIEnv *jEnv,
                                JavaClassDescriptor *class_descriptor,
                                jobject java_obj, jsval *vp)
{
    JSString *js_str;
    jstring java_str;
    jmethodID toString;
    
    /* Create a Java string, unless java_obj is already a java.lang.String */
    if ((*jEnv)->IsInstanceOf(jEnv, java_obj, jlString)) {
        java_str = java_obj;
    } else {
        jclass java_class;

        java_class = class_descriptor->java_class;
        toString = (*jEnv)->GetMethodID(jEnv, java_class, "toString",
                                        "()Ljava/lang/String;");
        if (!toString) {
            /* All Java objects have a toString method */
            jsj_UnexpectedJavaError(cx, jEnv, "No toString() method for class %s!",
                                    class_descriptor->name);
            return JS_FALSE;
        }
        java_str = (*jEnv)->CallObjectMethod(jEnv, java_obj, toString);
        if (!java_str) {
            jsj_ReportJavaError(cx, jEnv, "toString() method failed");
            return JS_FALSE;
        }
    }

    /* Extract Unicode from java.lang.String instance and convert to JS string */
    js_str = jsj_ConvertJavaStringToJSString(cx, jEnv, java_str);
    if (!js_str)
        return JS_FALSE;

    *vp = STRING_TO_JSVAL(js_str);
    return JS_TRUE;
}

/*
 * Convert a Java object to a number by attempting to call the
 * doubleValue() method on a Java object to get a double result.
 * This usually only works on instances of java.lang.Double, but the code
 * is generalized to work with any Java object that supports this method.
 *
 * Returns JS_TRUE if the call was successful.
 * Returns JS_FALSE if conversion is not possible or an error occurs.
 */
JSBool
jsj_ConvertJavaObjectToJSNumber(JSContext *cx, JNIEnv *jEnv,
                                jobject java_obj, jsval *vp)
{
    jdouble d;
    jmethodID doubleValue;

#ifndef SUN_VM_IS_NOT_GARBAGE
    /* Late breaking news: calling GetMethodID() on an object that doesn't
       contain the given method may cause the Sun VM to crash.  So we only
       call the method on instances of java.lang.Double */
    JSBool is_Double;
    
    /* Make sure that we have a java.lang.Double */
    is_Double = (*jEnv)->IsInstanceOf(jEnv, java_obj, jlDouble);
    if (!is_Double)
        return JS_FALSE;
    doubleValue = jlDouble_doubleValue;
#else
    doubleValue = (*jEnv)->GetMethodID(jEnv, java_obj, "doubleValue", "()D");
    if (!doubleValue)
        return JS_FALSE;
#endif

    d = (*jEnv)->CallDoubleMethod(jEnv, java_obj, doubleValue);
    if ((*jEnv)->ExceptionOccurred(jEnv)) {
        jsj_UnexpectedJavaError(cx, jEnv, "doubleValue() method failed");
        return JS_FALSE;
    }
    return JS_NewDoubleValue(cx, d, vp);
}

/*
 * Convert a Java object to a boolean by attempting to call the
 * booleanValue() method on a Java object to get a boolean result.
 * This usually only works on instances of java.lang.Boolean, but the code
 * is generalized to work with any Java object that supports this method.
 *
 * Returns JS_TRUE if the call was successful.
 * Returns JS_FALSE if conversion is not possible or an error occurs.
 */
extern JSBool
jsj_ConvertJavaObjectToJSBoolean(JSContext *cx, JNIEnv *jEnv,
                                 jobject java_obj, jsval *vp)
{
    jboolean b;
    jmethodID booleanValue;
    
#ifndef SUN_VM_IS_NOT_GARBAGE
    /* Late breaking news: calling GetMethodID() on an object that doesn't
       contain the given method may cause the Sun VM to crash.  So we only
       call the method on instances of java.lang.Boolean */
    
    JSBool is_Boolean;

    /* Make sure that we have a java.lang.Boolean */
    is_Boolean = (*jEnv)->IsInstanceOf(jEnv, java_obj, jlBoolean);
    if (!is_Boolean)
        return JS_FALSE;
    booleanValue = jlBoolean_booleanValue;
#else
    booleanValue = (*jEnv)->GetMethodID(jEnv, java_obj, "booleanValue", "()Z");
    if (!booleanValue)
        return JS_FALSE;
#endif

    b = (*jEnv)->CallBooleanMethod(jEnv, java_obj, booleanValue);
    if ((*jEnv)->ExceptionOccurred(jEnv)) {
        jsj_UnexpectedJavaError(cx, jEnv, "booleanValue() method failed");
        return JS_FALSE;
    }
    *vp = BOOLEAN_TO_JSVAL(b);
    return JS_TRUE;
}

/*
 * Reflect a Java object into a JS value.  The source object, java_obj, must
 * be of type java.lang.Object or a subclass and may, therefore, be an array.
 */
JSBool
jsj_ConvertJavaObjectToJSValue(JSContext *cx, JNIEnv *jEnv,
                               jobject java_obj, jsval *vp)
{
    jclass java_class;
    JSObject *js_obj;

    /* A null in Java-land is also null in JS */
    if (!java_obj) {
        *vp = JSVAL_NULL;
        return JS_TRUE;
    }

    java_class = (*jEnv)->GetObjectClass(jEnv, java_obj);

    /*
     * If it's an instance of netscape.javascript.JSObject, i.e. a wrapper
     * around a JS object that has been passed into the Java world, unwrap
     * it to obtain the original JS object.
     */
     if (njJSObject && (*jEnv)->IsInstanceOf(jEnv, java_obj, njJSObject)) {
        js_obj = (JSObject *)((*jEnv)->GetIntField(jEnv, java_obj, njJSObject_internal));
        PR_ASSERT(js_obj);
        if (!js_obj)
            return JS_FALSE;
        *vp = OBJECT_TO_JSVAL(js_obj);
        return JS_TRUE;
     }

    /*
     * Instances of java.lang.String are wrapped so we can call methods on
     * them, but they convert to a JS string if used in a string context.
     */
    /* TODO - let's get rid of this annoying "feature" */

    /* otherwise, wrap it inside a JavaObject */
    js_obj = jsj_WrapJavaObject(cx, jEnv, java_obj, java_class);
    if (!js_obj)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(js_obj);
    return JS_TRUE;
}

/*
 * Convert a Java value (primitive or object) to a JS value.
 *
 * This is usually an infallible operation, but JS_FALSE is returned
 * on an out-of-memory condition and the error reporter is called.
 */
JSBool
jsj_ConvertJavaValueToJSValue(JSContext *cx, JNIEnv *jEnv,
                              JavaSignature *signature,
                              jvalue *java_value,
                              jsval *vp)
{
    int32 ival32;

    switch (signature->type) {
    case JAVA_SIGNATURE_VOID:
        *vp = JSVAL_VOID;
        return JS_TRUE;

    case JAVA_SIGNATURE_BYTE:
        *vp = INT_TO_JSVAL((jsint)java_value->b);
        return JS_TRUE;

    case JAVA_SIGNATURE_CHAR:
        *vp = INT_TO_JSVAL((jsint)java_value->c);
        return JS_TRUE;

    case JAVA_SIGNATURE_SHORT:
        *vp = INT_TO_JSVAL((jsint)java_value->s);
        return JS_TRUE;

    case JAVA_SIGNATURE_INT:
        ival32 = java_value->i;
        if (INT_FITS_IN_JSVAL(ival32)) {
            *vp = INT_TO_JSVAL((jsint) ival32);
            return JS_TRUE;
        } else {
            return JS_NewDoubleValue(cx, ival32, vp);
        }

    case JAVA_SIGNATURE_BOOLEAN:
        *vp = BOOLEAN_TO_JSVAL((JSBool) java_value->z);
        return JS_TRUE;

    case JAVA_SIGNATURE_LONG:
        return JS_NewDoubleValue(cx, jlong_to_jdouble(java_value->j), vp);
  
    case JAVA_SIGNATURE_FLOAT:
        return JS_NewDoubleValue(cx, java_value->f, vp);

    case JAVA_SIGNATURE_DOUBLE:
        return JS_NewDoubleValue(cx, java_value->d, vp);

    case JAVA_SIGNATURE_CLASS:
    case JAVA_SIGNATURE_ARRAY:
        return jsj_ConvertJavaObjectToJSValue(cx, jEnv, java_value->l, vp);

    default:
        PR_ASSERT(0);
        return JS_FALSE;
    }
}

