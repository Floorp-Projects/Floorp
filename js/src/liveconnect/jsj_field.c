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
 
/*
 * This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * It contains the code used to reflect Java fields as properties of
 * JavaObject objects and the code to access those fields.
 *
 */

#include <stdlib.h>

#include "jsj_private.h"      /* LiveConnect internals */

/*
 * Add a single field, described by java_field, to the JavaMemberDescriptor
 * named by field_name within the given JavaClassDescriptor.
 *
 * Returns JS_TRUE on success. Otherwise, returns JS_FALSE and reports an error.
 */
static JSBool
add_java_field_to_class_descriptor(JSContext *cx,
                                   JNIEnv *jEnv,
                                   JavaClassDescriptor *class_descriptor, 
                                   jstring field_name_jstr,
                                   jobject java_field,  /* a java.lang.reflect.Field */
                                   jint modifiers)
{
    jclass fieldType;
    jfieldID fieldID;
    jclass java_class;

    JSBool is_static_field;
    JavaMemberDescriptor *member_descriptor = NULL;
    const char *sig_cstr = NULL;
    const char *field_name = NULL;
    JavaSignature *signature = NULL;
    JavaFieldSpec *field_spec = NULL;
        
    is_static_field = modifiers & ACC_STATIC;
    if (is_static_field) {
        member_descriptor = jsj_GetJavaStaticMemberDescriptor(cx, jEnv, class_descriptor, field_name_jstr);
    } else {
        member_descriptor = jsj_GetJavaMemberDescriptor(cx, jEnv, class_descriptor, field_name_jstr);
    }
    if (!member_descriptor)
        goto error;
    
    field_spec = (JavaFieldSpec*)JS_malloc(cx, sizeof(JavaFieldSpec));
    if (!field_spec)
        goto error;

    field_spec->modifiers = modifiers;

    /* Get the Java class corresponding to the type of the field */
    fieldType = (*jEnv)->CallObjectMethod(jEnv, java_field, jlrField_getType);
    if (!fieldType) {
        jsj_UnexpectedJavaError(cx, jEnv,
                                "Unable to determine type of field using"
                                " java.lang.reflect.Field.getType()");
        goto error;
    }
    
    signature = jsj_GetJavaClassDescriptor(cx, jEnv, fieldType);
    (*jEnv)->DeleteLocalRef(jEnv, fieldType);
    if (!signature)
        goto error;
    field_spec->signature = signature;

    field_name = jsj_DupJavaStringUTF(cx, jEnv, field_name_jstr);
    if (!field_name)
        goto error;
    field_spec->name = field_name;

    /* Compute the JNI-style (string-based) signature of the field type */
    sig_cstr = jsj_ConvertJavaSignatureToString(cx, signature);
    if (!sig_cstr)
        goto error;

    /* Compute the JNI fieldID and cache it for quick field access */
    java_class = class_descriptor->java_class;
    if (is_static_field)
        fieldID = (*jEnv)->GetStaticFieldID(jEnv, java_class, field_name, sig_cstr);
    else
        fieldID = (*jEnv)->GetFieldID(jEnv, java_class, field_name, sig_cstr);
    if (!fieldID) {
       jsj_UnexpectedJavaError(cx, jEnv,
                           "Can't get Java field ID for class %s, field %s (sig=%s)",
                           class_descriptor->name, field_name, sig_cstr);
       goto error;
    }
    field_spec->fieldID = fieldID;
    
    JS_free(cx, (char*)sig_cstr);
    
    member_descriptor->field = field_spec;

    /* Success */
    return JS_TRUE;

error:
    if (field_spec) {
        JS_FREE_IF(cx, (char*)field_spec->name);
        JS_free(cx, field_spec);
    }
    JS_FREE_IF(cx, (char*)sig_cstr);
    if (signature)
        jsj_ReleaseJavaClassDescriptor(cx, jEnv, signature);
    return JS_FALSE;
}

/*
 * Free up a JavaFieldSpec and all its resources.
 */
void
jsj_DestroyFieldSpec(JSContext *cx, JNIEnv *jEnv, JavaFieldSpec *field)
{
    JS_FREE_IF(cx, (char*)field->name);
    jsj_ReleaseJavaClassDescriptor(cx, jEnv, field->signature);
    JS_free(cx, field);
}

/*
 * Add a JavaMemberDescriptor to the collection of members in class_descriptor
 * for every public field of the identified Java class.  (A separate collection
 * is kept in class_descriptor for static and instance members.)
 * If reflect_only_static_fields is set, instance fields are not reflected.  If
 * it isn't set, only instance fields are reflected and static fields are not
 * reflected.
 *
 * Returns JS_TRUE on success. Otherwise, returns JS_FALSE and reports an error.
 */
JSBool 
jsj_ReflectJavaFields(JSContext *cx, JNIEnv *jEnv, JavaClassDescriptor *class_descriptor,
                      JSBool reflect_only_static_fields)
{
    int i;
    JSBool ok;
    jint modifiers;
    jobject java_field;
    jstring field_name_jstr;
    jarray joFieldArray;
    jsize num_fields;
    jclass java_class;

    /* Get a java array of java.lang.reflect.Field objects, by calling
       java.lang.Class.getFields(). */
    java_class = class_descriptor->java_class;
    joFieldArray = (*jEnv)->CallObjectMethod(jEnv, java_class, jlClass_getFields);
    if (!joFieldArray) {
        jsj_UnexpectedJavaError(cx, jEnv,
                                "Can't determine Java object's fields "
                                "using java.lang.Class.getFields()");
        return JS_FALSE;
    }

    /* Iterate over the class fields */
    num_fields = (*jEnv)->GetArrayLength(jEnv, joFieldArray);
    for (i = 0; i < num_fields; i++) {
       
        /* Get the i'th reflected field */
        java_field = (*jEnv)->GetObjectArrayElement(jEnv, joFieldArray, i);
        if (!java_field) {
            jsj_UnexpectedJavaError(cx, jEnv, "Can't access a Field[] array");
            return JS_FALSE;
        }

        /* Get the field modifiers, e.g. static, public, private, etc. */
        modifiers = (*jEnv)->CallIntMethod(jEnv, java_field, jlrField_getModifiers);
        if ((*jEnv)->ExceptionOccurred(jEnv)) {
            jsj_UnexpectedJavaError(cx, jEnv,
                                    "Can't access a Field's modifiers using"
                                    "java.lang.reflect.Field.getModifiers()");
            return JS_FALSE;
        }

        /* Don't allow access to private or protected Java fields. */
        if (!(modifiers & ACC_PUBLIC))
            goto no_reflect;

        /* Reflect all instance fields or all static fields, but not both */
        if (reflect_only_static_fields != ((modifiers & ACC_STATIC) != 0))
            goto no_reflect;

        /* Determine the unqualified name of the field */
        field_name_jstr = (*jEnv)->CallObjectMethod(jEnv, java_field, jlrField_getName);
        if (!field_name_jstr) {
            jsj_UnexpectedJavaError(cx, jEnv,
                                    "Can't obtain a Field's name"
                                    "java.lang.reflect.Field.getName()");
            return JS_FALSE;
        }
        
        /* Add a JavaFieldSpec object to the JavaClassDescriptor */
        ok = add_java_field_to_class_descriptor(cx, jEnv, class_descriptor, field_name_jstr,
                                                java_field, modifiers);
        if (!ok)
            return JS_FALSE;

        (*jEnv)->DeleteLocalRef(jEnv, field_name_jstr);
        field_name_jstr = NULL;

no_reflect:
        (*jEnv)->DeleteLocalRef(jEnv, java_field);
        java_field = NULL;
    }

    (*jEnv)->DeleteLocalRef(jEnv, joFieldArray);

    /* Success */
    return JS_TRUE;
}

/*
 * Read the value of a Java field and return it as a JavaScript value.
 * If the field is static, then java_obj is a Java class, otherwise
 * it's a Java instance object.
 *
 * Returns JS_TRUE on success. Otherwise, returns JS_FALSE and reports an error.
 */
JSBool
jsj_GetJavaFieldValue(JSContext *cx, JNIEnv *jEnv, JavaFieldSpec *field_spec,
                      jobject java_obj, jsval *vp)
{
    JSBool is_static_field, success;
    jvalue java_value;
    JavaSignature *signature;
    JavaSignatureChar field_type;
    jfieldID fieldID = field_spec->fieldID;

    is_static_field = field_spec->modifiers & ACC_STATIC;

#define GET_JAVA_FIELD(Type,member)                                          \
    JS_BEGIN_MACRO                                                           \
    if (is_static_field)                                                     \
        java_value.member =                                                  \
            (*jEnv)->GetStatic##Type##Field(jEnv, java_obj, fieldID);        \
    else                                                                     \
        java_value.member =                                                  \
            (*jEnv)->Get##Type##Field(jEnv, java_obj, fieldID);              \
    if ((*jEnv)->ExceptionOccurred(jEnv)) {                                  \
        jsj_UnexpectedJavaError(cx, jEnv, "Error reading Java field");           \
        return JS_FALSE;                                                     \
    }                                                                        \
    JS_END_MACRO

    signature = field_spec->signature;
    field_type = signature->type;
    switch(field_type) {
    case JAVA_SIGNATURE_BYTE:
        GET_JAVA_FIELD(Byte,b);
        break;

    case JAVA_SIGNATURE_CHAR:
        GET_JAVA_FIELD(Char,c);
        break;

    case JAVA_SIGNATURE_SHORT:
        GET_JAVA_FIELD(Short,s);
        break;

    case JAVA_SIGNATURE_INT:
        GET_JAVA_FIELD(Int,i);
        break;

    case JAVA_SIGNATURE_BOOLEAN:
        GET_JAVA_FIELD(Boolean,z);
        break;

    case JAVA_SIGNATURE_LONG:
        GET_JAVA_FIELD(Long,j);
        break;
  
    case JAVA_SIGNATURE_FLOAT:
        GET_JAVA_FIELD(Float,f);
        break;

    case JAVA_SIGNATURE_DOUBLE:
        GET_JAVA_FIELD(Double,d);
        break;
     
    case JAVA_SIGNATURE_UNKNOWN:
    case JAVA_SIGNATURE_VOID:
        JS_ASSERT(0);        /* Unknown java type signature */
        return JS_FALSE;
        
    /* Non-primitive (reference) type */
    default:
        JS_ASSERT(IS_REFERENCE_TYPE(field_type));
        GET_JAVA_FIELD(Object,l);
        success = jsj_ConvertJavaObjectToJSValue(cx, jEnv, java_value.l, vp);
        (*jEnv)->DeleteLocalRef(jEnv, java_value.l);
        return success;
    }
    
#undef GET_JAVA_FIELD

    return jsj_ConvertJavaValueToJSValue(cx, jEnv, signature, &java_value, vp);
}

JSBool
jsj_SetJavaFieldValue(JSContext *cx, JNIEnv *jEnv, JavaFieldSpec *field_spec,
                      jclass java_obj, jsval js_val)
{
    JSBool is_static_field, is_local_ref;
    int dummy_cost;
    jvalue java_value;
    JavaSignature *signature;
    JavaSignatureChar field_type;
    jfieldID fieldID = field_spec->fieldID;

    is_static_field = field_spec->modifiers & ACC_STATIC;

#define SET_JAVA_FIELD(Type,member)                                          \
    JS_BEGIN_MACRO                                                           \
    if (is_static_field) {                                                   \
        (*jEnv)->SetStatic##Type##Field(jEnv, java_obj, fieldID,             \
                                        java_value.member);                  \
    } else {                                                                 \
        (*jEnv)->Set##Type##Field(jEnv, java_obj, fieldID,java_value.member);\
    }                                                                        \
    if ((*jEnv)->ExceptionOccurred(jEnv)) {                                  \
        jsj_UnexpectedJavaError(cx, jEnv, "Error assigning to Java field");      \
        return JS_FALSE;                                                     \
    }                                                                        \
    JS_END_MACRO

    signature = field_spec->signature;
    if (!jsj_ConvertJSValueToJavaValue(cx, jEnv, js_val, signature, &dummy_cost,
                                       &java_value, &is_local_ref))
        return JS_FALSE;

    field_type = signature->type;
    switch(field_type) {
    case JAVA_SIGNATURE_BYTE:
        SET_JAVA_FIELD(Byte,b);
        break;

    case JAVA_SIGNATURE_CHAR:
        SET_JAVA_FIELD(Char,c);
        break;

    case JAVA_SIGNATURE_SHORT:
        SET_JAVA_FIELD(Short,s);
        break;

    case JAVA_SIGNATURE_INT:
        SET_JAVA_FIELD(Int,i);
        break;

    case JAVA_SIGNATURE_BOOLEAN:
        SET_JAVA_FIELD(Boolean,z);
        break;

    case JAVA_SIGNATURE_LONG:
        SET_JAVA_FIELD(Long,j);
        break;
  
    case JAVA_SIGNATURE_FLOAT:
        SET_JAVA_FIELD(Float,f);
        break;

    case JAVA_SIGNATURE_DOUBLE:
        SET_JAVA_FIELD(Double,d);
        break;
        
    /* Non-primitive (reference) type */
    default:
        JS_ASSERT(IS_REFERENCE_TYPE(field_type));
        SET_JAVA_FIELD(Object,l);
        if (is_local_ref)
            (*jEnv)->DeleteLocalRef(jEnv, java_value.l);
        break;

    case JAVA_SIGNATURE_UNKNOWN:
    case JAVA_SIGNATURE_VOID:
        JS_ASSERT(0);        /* Unknown java type signature */
        return JS_FALSE;
    }

#undef SET_JAVA_FIELD
    
    return JS_TRUE;
}
