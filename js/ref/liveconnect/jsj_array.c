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
 * It contains the code for reading and writing elements of a Java array.
 */

#include "prtypes.h"
#include "prassert.h"

#include "jsj_private.h"      /* LiveConnect internals */

/*
 * Read the Java value at a given index into a Java array and convert it
 * to a JS value.  The array_component_signature describes the type of
 * the resulting Java value, which can be a primitive type or an object type.
 * More specifically it can be an array type in the case of multidimensional
 * arrays.
 */
JSBool
jsj_GetJavaArrayElement(JSContext *cx, JNIEnv *jEnv, jarray java_array, jsize index,
                        JavaSignature *array_component_signature,
                        jsval *vp)
{
    jvalue java_value;
    JavaSignatureChar component_type;

#define GET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Type,member)                   \
    (*jEnv)->Get##Type##ArrayRegion(jEnv, java_array, index, 1,              \
                                    &java_value.member);                     \
    if ((*jEnv)->ExceptionOccurred(jEnv)) {                                  \
        jsj_ReportJavaError(cx, jEnv, "Error reading element of "            \
                                      "Java primitive array");               \
        return JS_FALSE;                                                     \
    }

    component_type = array_component_signature->type;
    switch(component_type) {
    case JAVA_SIGNATURE_BYTE:
        GET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Byte,b);
        break;

    case JAVA_SIGNATURE_CHAR:
        GET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Char,c);
        break;

    case JAVA_SIGNATURE_SHORT:
        GET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Short,s);
        break;

    case JAVA_SIGNATURE_INT:
        GET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Int,i);
        break;

    case JAVA_SIGNATURE_BOOLEAN:
        GET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Boolean,z);
        break;

    case JAVA_SIGNATURE_LONG:
        GET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Long,j);
        break;
  
    case JAVA_SIGNATURE_FLOAT:
        GET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Float,f);
        break;

    case JAVA_SIGNATURE_DOUBLE:
        GET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Double,d);
        break;

    case JAVA_SIGNATURE_CLASS:
    case JAVA_SIGNATURE_ARRAY:
        java_value.l = (*jEnv)->GetObjectArrayElement(jEnv, java_array, index);
        if ((*jEnv)->ExceptionOccurred(jEnv)) {
            jsj_ReportJavaError(cx, jEnv, "Error reading Java object array");
            return JS_FALSE;
        }
        break;

#undef GET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY
    default:
        PR_ASSERT(0);        /* Unknown java type signature */
        return JS_FALSE;
    }

    return jsj_ConvertJavaValueToJSValue(cx, jEnv, array_component_signature, &java_value, vp);
}

JSBool
jsj_SetJavaArrayElement(JSContext *cx, JNIEnv *jEnv, jarray java_array, jsize index,
                        JavaSignature *array_component_signature,
                        jsval js_val)
{
    int dummy_cost;
    jvalue java_value;
    JavaSignatureChar component_type;
    JSBool is_local_ref;

    if (!jsj_ConvertJSValueToJavaValue(cx, jEnv, js_val, array_component_signature,
                                       &dummy_cost, &java_value, &is_local_ref))
        return JS_FALSE;

#define SET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Type,member)                   \
    (*jEnv)->Set##Type##ArrayRegion(jEnv, java_array, index, 1,              \
                                    &java_value.member);                     \
    if ((*jEnv)->ExceptionOccurred(jEnv)) {                                  \
        jsj_ReportJavaError(cx, jEnv, "Error assigning to element of "       \
                                      "Java primitive array");               \
        return JS_FALSE;                                                     \
    }

    component_type = array_component_signature->type;
    switch(component_type) {
    case JAVA_SIGNATURE_BYTE:
        SET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Byte,b);
        break;

    case JAVA_SIGNATURE_CHAR:
        SET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Char,c);
        break;

    case JAVA_SIGNATURE_SHORT:
        SET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Short,s);
        break;

    case JAVA_SIGNATURE_INT:
        SET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Int,i);
        break;

    case JAVA_SIGNATURE_BOOLEAN:
        SET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Boolean,z);
        break;

    case JAVA_SIGNATURE_LONG:
        SET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Long,j);
        break;
  
    case JAVA_SIGNATURE_FLOAT:
        SET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Float,f);
        break;

    case JAVA_SIGNATURE_DOUBLE:
        SET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY(Double,d);
        break;

    case JAVA_SIGNATURE_CLASS:
    case JAVA_SIGNATURE_ARRAY:
        (*jEnv)->SetObjectArrayElement(jEnv, java_array, index, java_value.l);
        if (is_local_ref)                                                           \
            (*jEnv)->DeleteLocalRef(jEnv, java_value.l);
        if ((*jEnv)->ExceptionOccurred(jEnv)) {
            jsj_ReportJavaError(cx, jEnv, "Error assigning to Java object array");
            return JS_FALSE;
        }
        break;

#undef SET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY
    default:
        PR_ASSERT(0);        /* Unknown java type signature */
        return JS_FALSE;
    }

    return JS_TRUE;
}

