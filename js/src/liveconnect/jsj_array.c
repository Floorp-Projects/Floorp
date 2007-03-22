/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
/*
 * This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * It contains the code for reading and writing elements of a Java array.
 */

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
    JSBool success;

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

    /* Non-primitive (reference) type */
    default:
        JS_ASSERT(component_type >= JAVA_SIGNATURE_ARRAY);
        java_value.l = (*jEnv)->GetObjectArrayElement(jEnv, java_array, index);
        if ((*jEnv)->ExceptionOccurred(jEnv)) {
            jsj_ReportJavaError(cx, jEnv, "Error reading Java object array");
            return JS_FALSE;
        }
        success = jsj_ConvertJavaObjectToJSValue(cx, jEnv, java_value.l, vp);
        (*jEnv)->DeleteLocalRef(jEnv, java_value.l);
        return success;

#undef GET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY
    case JAVA_SIGNATURE_UNKNOWN:
    case JAVA_SIGNATURE_VOID:
        JS_ASSERT(0);        /* Unknown java type signature */
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

    /* Non-primitive (reference) type */
    default:
        JS_ASSERT(IS_REFERENCE_TYPE(component_type));
        (*jEnv)->SetObjectArrayElement(jEnv, java_array, index, java_value.l);
        if (is_local_ref)                                                           \
            (*jEnv)->DeleteLocalRef(jEnv, java_value.l);
        if ((*jEnv)->ExceptionOccurred(jEnv)) {
            jsj_ReportJavaError(cx, jEnv, "Error assigning to Java object array");
            return JS_FALSE;
        }
        break;

#undef SET_ELEMENT_FROM_PRIMITIVE_JAVA_ARRAY
    case JAVA_SIGNATURE_UNKNOWN:
    case JAVA_SIGNATURE_VOID:
        JS_ASSERT(0);        /* Unknown java type signature */
        return JS_FALSE;
    }

    return JS_TRUE;
}

