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
 * Below is the code that converts between Java and JavaScript values of all
 * types.
 */

#include <stdlib.h>
#include <string.h>

#ifdef OJI
#include "prtypes.h"          /* Need platform-dependent definition of HAVE_LONG_LONG
                                 for non-standard jri_md.h file */
#endif

#include "jsj_private.h"      /* LiveConnect internals */

/* Floating-point double utilities, stolen from jsnum.h */
#ifdef IS_LITTLE_ENDIAN
#define JSDOUBLE_HI32(x)        (((uint32 *)&(x))[1])
#define JSDOUBLE_LO32(x)        (((uint32 *)&(x))[0])
#else
#define JSDOUBLE_HI32(x)        (((uint32 *)&(x))[0])
#define JSDOUBLE_LO32(x)        (((uint32 *)&(x))[1])
#endif
#define JSDOUBLE_HI32_SIGNBIT   0x80000000
#define JSDOUBLE_HI32_EXPMASK   0x7ff00000
#define JSDOUBLE_HI32_MANTMASK  0x000fffff

#define JSDOUBLE_IS_NaN(x)                                                    \
    ((JSDOUBLE_HI32(x) & JSDOUBLE_HI32_EXPMASK) == JSDOUBLE_HI32_EXPMASK &&   \
     (JSDOUBLE_LO32(x) || (JSDOUBLE_HI32(x) & JSDOUBLE_HI32_MANTMASK)))

#define JSDOUBLE_IS_INFINITE(x)                                               \
    ((JSDOUBLE_HI32(x) & ~JSDOUBLE_HI32_SIGNBIT) == JSDOUBLE_HI32_EXPMASK &&  \
     !JSDOUBLE_LO32(x))

static JSBool
convert_js_obj_to_JSObject_wrapper(JSContext *cx, JNIEnv *jEnv, JSObject *js_obj,
                                   JavaSignature *signature,
                                   int *cost, jobject *java_value)
{
    if (!njJSObject) {
        if (java_value)
            JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                                JSJMSG_CANT_LOAD_JSOBJECT);
        return JS_FALSE;
    }

    if (!(*jEnv)->IsAssignableFrom(jEnv, njJSObject, signature->java_class))
        return JS_FALSE;

    if (!java_value)
        return JS_TRUE;

    *java_value = jsj_WrapJSObject(cx, jEnv, js_obj);

    return (*java_value != NULL);   
}

/* Copy an array from JS to Java;  Create a new Java array and populate its
   elements, one by one, with the result of converting each JS array element
   to the type of the array component. */
static JSBool
convert_js_array_to_java_array(JSContext *cx, JNIEnv *jEnv, JSObject *js_array,
                               JavaSignature *signature,
                               jobject *java_valuep)
{
    jsuint i;
    jsval js_val;
    jsuint length;
    jclass component_class;
    jarray java_array;
    JavaSignature *array_component_signature;

    if (!JS_GetArrayLength(cx, js_array, &length))
        return JS_FALSE;

    /* Get the Java class of each element of the array */
    array_component_signature = signature->array_component_signature;
    component_class = array_component_signature->java_class;

    /* Create a new empty Java array with the same length as the JS array */
    java_array = (*jEnv)->CallStaticObjectMethod(jEnv, jlrArray, jlrArray_newInstance,
                                                 component_class, length);
    if (!java_array) {
        jsj_ReportJavaError(cx, jEnv, "Error while constructing empty array of %s",
                            jsj_GetJavaClassName(cx, jEnv, component_class));
        return JS_FALSE;
    }

    /* Convert each element of the JS array to an element of the Java array.
       If an error occurs, there is no need to worry about releasing the
       individual elements of the Java array - they will eventually be GC'ed
       by the JVM. */
    for (i = 0; i < length; i++) {
        if (!JS_LookupElement(cx, js_array, i, &js_val))
            return JS_FALSE;

        if (!jsj_SetJavaArrayElement(cx, jEnv, java_array, i, array_component_signature, js_val))
            return JS_FALSE;
    }

    /* Return the result array */
    *java_valuep = java_array;
    return JS_TRUE;
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
                               int *cost, jobject *java_value, JSBool *is_local_refp)
{
    JSString *jsstr;
    jclass target_java_class;
    
    JS_ASSERT(IS_REFERENCE_TYPE(signature->type));

    /* Initialize to default case, in which no new Java object is
       synthesized to perform the conversion and, therefore, no JNI local
       references are being held. */
    *is_local_refp = JS_FALSE;

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
            
            /* Fall through, to attempt conversion to a Java string */
            
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
            if (convert_js_obj_to_JSObject_wrapper(cx, jEnv, js_obj, signature, cost, java_value)) {
                if (java_value && *java_value)
                    *is_local_refp = JS_TRUE;
                return JS_TRUE;
            }
            
            /* Fall through, to attempt conversion to a Java string */
            
        } else if (JS_InstanceOf(cx, js_obj, &JavaMember_class, 0)) {

            if (!JS_ConvertValue(cx, v, JSTYPE_OBJECT, &v))
                return JS_FALSE;
            return jsj_ConvertJSValueToJavaObject(cx, jEnv, v, signature, cost,
                                                  java_value, is_local_refp);

        /* JS Arrays are converted, element by element, to Java arrays */
        } else if (JS_IsArrayObject(cx, js_obj) && (signature->type == JAVA_SIGNATURE_ARRAY)) {
            if (convert_js_array_to_java_array(cx, jEnv, js_obj, signature, java_value)) {
                if (java_value && *java_value)
                    *is_local_refp = JS_TRUE;
                return JS_TRUE;
            }
            return JS_FALSE;

        } else {
            /* Otherwise, see if the target type is the  netscape.javascript.JSObject
               wrapper class or one of its subclasses, in which case a
               reference is passed to the original JS object by wrapping it
               inside an instance of netscape.javascript.JSObject */
            if (convert_js_obj_to_JSObject_wrapper(cx, jEnv, js_obj, signature, cost, java_value))             {
                if (java_value && *java_value)
                    *is_local_refp = JS_TRUE;
                return JS_TRUE;
            }
            
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
                if (*java_value) {
                    *is_local_refp = JS_TRUE;
                } else {
                    jsj_UnexpectedJavaError(cx, jEnv,
                        "Couldn't construct instance of java.lang.Double");
                    return JS_FALSE;
                }
            }

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
                if (*java_value) {
                    *is_local_refp = JS_TRUE;
                } else {
                    jsj_UnexpectedJavaError(cx, jEnv, "Couldn't construct instance " 
                        "of java.lang.Boolean");
                    return JS_FALSE;
                }
            }

            return JS_TRUE;
        }
        /* Fall through, to attempt conversion to a java.lang.String ... */
    }
    
    /* If the source JS type is either a string or undefined, or if no conversion
       is possible from a number, boolean or JS object, see if the target type is
       java.lang.String */
    if ((*jEnv)->IsAssignableFrom(jEnv, jlString, target_java_class)) {
        
        /* Convert to JS string, if necessary, and then to a Java Unicode string */
        jsstr = JS_ValueToString(cx, v);
        if (jsstr) {
            if (java_value) {
                *java_value = jsj_ConvertJSStringToJavaString(cx, jEnv, jsstr);
                if (*java_value) {
                    *is_local_refp = JS_TRUE;
                } else {
                    return JS_FALSE;
                }
            }

            return JS_TRUE;
        }
    }
    
conversion_error:
    return JS_FALSE;
}

/* Valid ranges for Java numeric types */
#define jbyte_MAX_VALUE   127.0
#define jbyte_MIN_VALUE  -128.0
#define jchar_MAX_VALUE   65535.0
#define jchar_MIN_VALUE   0.0
#define jshort_MAX_VALUE  32767.0
#define jshort_MIN_VALUE -32768.0
#define jint_MAX_VALUE    2147483647.0
#define jint_MIN_VALUE   -2147483648.0
#define jlong_MAX_VALUE   9223372036854775807.0
#define jlong_MIN_VALUE  -9223372036854775808.0

/* Utility macro for jsj_ConvertJSValueToJavaValue(), below */
#define JSVAL_TO_INTEGRAL_JVALUE(type_name, member_name, member_type, jsval, java_value) \
    if (!JSVAL_IS_NUMBER(v)) {                                           \
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
                goto numeric_conversion_error;                           \
        } else {                                                         \
            jdouble dval = *JSVAL_TO_DOUBLE(v);                          \
                                                                         \
            /* NaN becomes zero when converted to integral value */      \
            if (JSDOUBLE_IS_NaN(dval))                                   \
                goto numeric_conversion_error;                           \
                                                                         \
            /* Unrepresentably large numbers, including infinities, */   \
            /* cause an error. */                                        \
            else if ((dval >= member_type ## _MAX_VALUE + 1) ||          \
                     (dval <= member_type ## _MIN_VALUE - 1)) {          \
                goto numeric_conversion_error;                           \
            } else                                                       \
                member_name = (member_type) dval;                        \
                                                                         \
            /* Don't allow a non-integral number to be converted         \
               to an integral type */                                    \
            /* Actually, we have to allow this for LC1 compatibility */  \
            /* if ((jdouble)member_name != dval)                         \
                (*cost)++; */                                            \
        }                                                                \
        if (java_value)                                                  \
            java_value->member_name = member_name;                       \
    }

#if XP_MAC

/* on MRJ jlong is typedef'd to wide, which is a struct. */
#include <Math64.h>

static jsint jlong_to_jsint(jlong lvalue)
{
    SInt64 val = WideToSInt64(lvalue);
    return S32Set(val);
}

static jlong jsint_to_jlong(jsint ivalue)
{
    SInt64 val = S64Set(ivalue);
    wide wval =SInt64ToWide(val);
    return *(jlong*)&wval;
}

static jdouble jlong_to_jdouble(jlong lvalue)
{
    SInt64 val = WideToSInt64(lvalue);
    return SInt64ToLongDouble(val);
}

static jlong jdouble_to_jlong(jdouble dvalue)
{
    SInt64 val = LongDoubleToSInt64(dvalue);
    wide wval = SInt64ToWide(val);
    return *(jlong*)&wval;
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
                                                                         \
            /* NaN becomes zero when converted to integral value */      \
            if (JSDOUBLE_IS_NaN(dval))                                   \
                goto numeric_conversion_error;                           \
                                                                         \
            /* Unrepresentably large numbers, including infinities, */   \
            /* cause an error. */                                        \
            else if ((dval >= member_type ## _MAX_VALUE + 1) ||          \
                     (dval <= member_type ## _MIN_VALUE - 1)) {          \
                goto numeric_conversion_error;                           \
            } else                                                       \
                member_name = jdouble_to_jlong(dval);                    \
                                                                         \
            /* Don't allow a non-integral number to be converted         \
               to an integral type */                                    \
            /* Actually, we have to allow this for LC1 compatibility */  \
            /*if (jlong_to_jdouble(member_name) != dval)                 \
                (*cost)++;*/                                             \
        }                                                                \
        if (java_value)                                                  \
            java_value->member_name = member_name;                       \
    }

#else
#ifdef XP_OS2

/* OS2 utility macro for jsj_ConvertJSValueToJavaValue(), below             */
/* jlong is a structure, see jri_md.h, where the jlong_ macros are defined. */
#define JSVAL_TO_JLONG_JVALUE(member_name, member_type, jsvalue, java_value) \
   if (!JSVAL_IS_NUMBER(jsvalue)) {                                      \
      if (!JS_ConvertValue(cx, jsvalue, JSTYPE_NUMBER, &jsvalue))        \
         goto conversion_error;                                          \
      (*cost)++;                                                         \
   }                                                                     \
   {                                                                     \
      member_type member_name;                                           \
                                                                         \
      if (JSVAL_IS_INT(jsvalue)) {                                       \
          jsint ival = JSVAL_TO_INT(jsvalue);                            \
          jlong_I2L(member_name,ival);                                   \
                                                                         \
       } else {                                                          \
            jdouble dval = *JSVAL_TO_DOUBLE(jsvalue);                    \
                                                                         \
            /* NaN becomes zero when converted to integral value */      \
            if (JSDOUBLE_IS_NaN(dval))                                   \
                jlong_I2L(member_name,0);                                \
                                                                         \
            /* Unrepresentably large numbers, including infinities, */   \
            /* cause an error. */                                        \
            else if ((dval > member_type ## _MAX_VALUE) ||               \
                     (dval < member_type ## _MIN_VALUE)) {               \
                goto numeric_conversion_error;                           \
            } else                                                       \
                jlong_D2L(member_name,dval);                             \
                                                                         \
            /* Don't allow a non-integral number to be converted         \
               to an integral type */                                    \
            /* Actually, we have to allow this for LC1 compatibility */  \
            /*if (jlong_to_jdouble(member_name) != dval)                 \
                (*cost)++;*/                                             \
        }                                                                \
        if (java_value)                                                  \
            java_value->member_name = member_name;                       \
    }

static jdouble jlong_to_jdouble(jlong lvalue)
{
   jdouble d;
   jlong_L2D(d,lvalue);
   return d;
}

#else

#define jlong_to_jdouble(lvalue) ((jdouble) lvalue)

#endif
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
jsj_ConvertJSValueToJavaValue(JSContext *cx, JNIEnv *jEnv, jsval v_arg,
                              JavaSignature *signature,
                              int *cost, jvalue *java_value, JSBool *is_local_refp)
{
    JavaSignatureChar type;
    jsval v;
    JSBool success = JS_FALSE;

    /* Initialize to default case, in which no new Java object is
       synthesized to perform the conversion and, therefore, no JNI local
       references are being held. */
    *is_local_refp = JS_FALSE;   
    
    type = signature->type;
    v = v_arg;
    switch (type) {
    case JAVA_SIGNATURE_BOOLEAN:
        if (!JSVAL_IS_BOOLEAN(v)) {
            if (!JS_ConvertValue(cx, v, JSTYPE_BOOLEAN, &v))
                goto conversion_error;
            if (JSVAL_IS_VOID(v))
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
#if defined(XP_MAC) || (defined(XP_OS2) && !defined(HAVE_LONG_LONG))
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

    /* Non-primitive (reference) type */
    default:
        JS_ASSERT(IS_REFERENCE_TYPE(type));
        if (!jsj_ConvertJSValueToJavaObject(cx, jEnv, v, signature, cost,
            &java_value->l, is_local_refp))
            goto conversion_error;
        break;

    case JAVA_SIGNATURE_UNKNOWN:
    case JAVA_SIGNATURE_VOID:
        JS_ASSERT(0);
        return JS_FALSE;
    }

    /* Success */
    return JS_TRUE;

numeric_conversion_error:
    success = JS_TRUE;
    /* Fall through ... */

conversion_error:

    if (java_value) {
        const char *jsval_string;
        const char *class_name;
        JSString *jsstr;

        jsval_string = NULL;
        jsstr = JS_ValueToString(cx, v_arg);
        if (jsstr)
            jsval_string = JS_GetStringBytes(jsstr);
        if (!jsval_string)
            jsval_string = "";
        
        class_name = jsj_ConvertJavaSignatureToHRString(cx, signature);
        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                             JSJMSG_CANT_CONVERT_JS, jsval_string,
                             class_name);

        return JS_FALSE;
    }
    return success;
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
    jsize ucs2_str_len;

    ucs2_str_len = (*jEnv)->GetStringLength(jEnv, java_str);
    ucs2_str = (*jEnv)->GetStringChars(jEnv, java_str, &is_copy);
    if (!ucs2_str) {
        jsj_UnexpectedJavaError(cx, jEnv,
                                "Unable to extract native Unicode from Java string");
        return NULL;
    }

    /* The string data passed into JS_NewUCString() is
       not copied, so make a copy of the Unicode character vector. */
    js_str = JS_NewUCStringCopyN(cx, ucs2_str, ucs2_str_len);

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
    jclass java_class;
    
    /* Create a Java string, unless java_obj is already a java.lang.String */
    if ((*jEnv)->IsInstanceOf(jEnv, java_obj, jlString)) {

        /* Extract Unicode from java.lang.String instance and convert to JS string */
        js_str = jsj_ConvertJavaStringToJSString(cx, jEnv, java_obj);
        if (!js_str)
            return JS_FALSE;
        *vp = STRING_TO_JSVAL(js_str);
        return JS_TRUE;
    }
    
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
    
    /* Extract Unicode from java.lang.String instance and convert to JS string */
    js_str = jsj_ConvertJavaStringToJSString(cx, jEnv, java_str);
    if (!js_str) {
        (*jEnv)->DeleteLocalRef(jEnv, java_str);
        return JS_FALSE;
    }

    *vp = STRING_TO_JSVAL(js_str);
    (*jEnv)->DeleteLocalRef(jEnv, java_str);
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
                                JavaClassDescriptor *class_descriptor,
                                jobject java_obj, jsval *vp)
{
    jdouble d;
    jmethodID doubleValue;
    jclass java_class;

    java_class = class_descriptor->java_class;
    doubleValue = (*jEnv)->GetMethodID(jEnv, java_class, "doubleValue", "()D");
    if (!doubleValue) {
        /* There is no doubleValue() method for the object.  Try toString()
           instead and the JS engine will attempt to convert the result to
           a number. */
        (*jEnv)->ExceptionClear(jEnv);
        return jsj_ConvertJavaObjectToJSString(cx, jEnv, class_descriptor,
                                               java_obj, vp);
    }

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
                                 JavaClassDescriptor *class_descriptor,
                                 jobject java_obj, jsval *vp)
{
    jboolean b;
    jmethodID booleanValue;
    jclass java_class;
    
    /* Null converts to false. */
    if (!java_obj) {
        *vp = JSVAL_FALSE;
        return JS_TRUE;
    }
    java_class = class_descriptor->java_class;
    booleanValue = (*jEnv)->GetMethodID(jEnv, java_class, "booleanValue", "()Z");

    /* Non-null Java object does not have a booleanValue() method, so
       it converts to true. */
    if (!booleanValue) {
        (*jEnv)->ExceptionClear(jEnv);
        *vp = JSVAL_TRUE;
        return JS_TRUE;
    }

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
#ifdef PRESERVE_JSOBJECT_IDENTITY
#if JS_BYTES_PER_LONG == 8
        js_obj = (JSObject *)((*jEnv)->GetLongField(jEnv, java_obj, njJSObject_long_internal));
#else
        js_obj = (JSObject *)((*jEnv)->GetIntField(jEnv, java_obj, njJSObject_internal));
#endif
#else
        js_obj = jsj_UnwrapJSObjectWrapper(jEnv, java_obj);
#endif
		/* NULL is actually a valid value. It means 'null'.

        JS_ASSERT(js_obj);
        if (!js_obj)
            goto error;
		*/

        *vp = OBJECT_TO_JSVAL(js_obj);
        goto done;
     }

    /* otherwise, wrap it inside a JavaObject */
    js_obj = jsj_WrapJavaObject(cx, jEnv, java_obj, java_class);
    if (!js_obj)
        goto error;
    *vp = OBJECT_TO_JSVAL(js_obj);

done:
    (*jEnv)->DeleteLocalRef(jEnv, java_class);
    return JS_TRUE;

error:
    (*jEnv)->DeleteLocalRef(jEnv, java_class);
    return JS_FALSE;
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

    case JAVA_SIGNATURE_UNKNOWN:
        JS_ASSERT(0);
        return JS_FALSE;
        
    /* Non-primitive (reference) type */
    default:
        JS_ASSERT(IS_REFERENCE_TYPE(signature->type));
        return jsj_ConvertJavaObjectToJSValue(cx, jEnv, java_value->l, vp);

    }
}

