/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * It contains the code that constructs and manipulates JavaClassDescriptor
 * structs, which are the native wrappers for Java classes.
 * JavaClassDescriptors are used to describe the signatures of methods and
 * fields.  There is a JavaClassDescriptor associated with the reflection of
 * each Java Object.
 */

#include <stdlib.h>
#include <string.h>

#include "jsj_private.h"        /* LiveConnect internals */
#include "jsj_hash.h"           /* Hash tables */

#ifdef JSJ_THREADSAFE
#   include "prmon.h"
#endif

/* A one-to-one mapping between all referenced java.lang.Class objects and
   their corresponding JavaClassDescriptor objects */
static JSJHashTable *java_class_reflections;

#ifdef JSJ_THREADSAFE
static PRMonitor *java_class_reflections_monitor;
#endif

/*
 * Given a JVM handle to a java.lang.Class object, malloc a C-string
 * containing the UTF8 encoding of the fully qualified name of the class.
 * It's the caller's responsibility to free the returned string.
 *
 * If an error occurs, NULL is returned and the error reporter called.
 */
const char *
jsj_GetJavaClassName(JSContext *cx, JNIEnv *jEnv, jclass java_class)
{
    jstring java_class_name_jstr;
    const char *java_class_name;

    /* Get java.lang.String object containing class name */
    java_class_name_jstr =
        (*jEnv)->CallObjectMethod(jEnv, java_class, jlClass_getName);

    if (!java_class_name_jstr)
        goto error;

    /* Convert to UTF8 encoding and copy */
    java_class_name = jsj_DupJavaStringUTF(cx, jEnv, java_class_name_jstr);

    (*jEnv)->DeleteLocalRef(jEnv, java_class_name_jstr);
    return java_class_name;

error:
    jsj_UnexpectedJavaError(cx, jEnv, "Can't get Java class name using"
                                      "java.lang.Class.getName()");
    return NULL;
}

/*
 * Convert in-place a string of the form "java.lang.String" into "java/lang/String".
 * Though the former style is conventionally used by Java programmers, the latter is
 * what the JNI functions require.
 */
void
jsj_MakeJNIClassname(char * class_name)
{
    char * c;
    for (c = class_name; *c; c++)
        if (*c == '.')
            *c = '/';
}

/*
 * Classify an instance of java.lang.Class as either one of the primitive
 * types, e.g. int, char, etc., as an array type or as a non-array object type
 * (subclass of java.lang.Object) by returning the appropriate enum member.
 *
 */
static JavaSignatureChar
get_signature_type(JSContext *cx, JavaClassDescriptor *class_descriptor)
{
    JavaSignatureChar type;
    const char *java_class_name;

    /* Get UTF8 encoding of class name */
    java_class_name = class_descriptor->name;
    JS_ASSERT(java_class_name);
    if (!java_class_name)
        return JAVA_SIGNATURE_UNKNOWN;

    if (!strcmp(java_class_name, "byte"))
        type = JAVA_SIGNATURE_BYTE;
    else if (!strcmp(java_class_name, "char"))
        type = JAVA_SIGNATURE_CHAR;
    else if (!strcmp(java_class_name, "float"))
        type = JAVA_SIGNATURE_FLOAT;
    else if (!strcmp(java_class_name, "double"))
        type = JAVA_SIGNATURE_DOUBLE;
    else if (!strcmp(java_class_name, "int"))
        type = JAVA_SIGNATURE_INT;
    else if (!strcmp(java_class_name, "long"))
        type = JAVA_SIGNATURE_LONG;
    else if (!strcmp(java_class_name, "short"))
        type = JAVA_SIGNATURE_SHORT;
    else if (!strcmp(java_class_name, "boolean"))
        type = JAVA_SIGNATURE_BOOLEAN;
    else if (!strcmp(java_class_name, "void"))
        type = JAVA_SIGNATURE_VOID;
    else if (!strcmp(java_class_name, "java.lang.Boolean"))
        type = JAVA_SIGNATURE_JAVA_LANG_BOOLEAN;
    else if (!strcmp(java_class_name, "java.lang.Double"))
        type = JAVA_SIGNATURE_JAVA_LANG_DOUBLE;
    else if (!strcmp(java_class_name, "java.lang.String"))
        type = JAVA_SIGNATURE_JAVA_LANG_STRING;
    else if (!strcmp(java_class_name, "java.lang.Object"))
        type = JAVA_SIGNATURE_JAVA_LANG_OBJECT;
    else if (!strcmp(java_class_name, "java.lang.Class"))
        type = JAVA_SIGNATURE_JAVA_LANG_CLASS;
    else if (!strcmp(java_class_name, "netscape.javascript.JSObject"))
        type = JAVA_SIGNATURE_NETSCAPE_JAVASCRIPT_JSOBJECT;
    else
        type = JAVA_SIGNATURE_OBJECT;
    return type;
}

static JSBool
is_java_array_class(JNIEnv *jEnv, jclass java_class)
{
    return (*jEnv)->CallBooleanMethod(jEnv, java_class, jlClass_isArray);
}

/*
 * Return the class of a Java array's component type.  This is not the same
 * as the array's element type.  For example, the component type of an array
 * of type SomeType[][][] is SomeType[][], but its element type is SomeType.
 *
 * If an error occurs, NULL is returned and an error reported.
 */
static jclass
get_java_array_component_class(JSContext *cx, JNIEnv *jEnv, jclass java_class)
{
    jclass result;
    result = (*jEnv)->CallObjectMethod(jEnv, java_class, jlClass_getComponentType);
    if (!result) {
        jsj_UnexpectedJavaError(cx, jEnv,
                                "Can't get Java array component class using "
                                "java.lang.Class.getComponentType()");
        return NULL;
    }
    return result;
}

/*
 * Given a Java class, fill in the signature structure that describes the class.
 * If an error occurs, JS_FALSE is returned and the error reporter called.
 */
static JSBool
compute_java_class_signature(JSContext *cx, JNIEnv *jEnv, JavaSignature *signature)
{
    jclass java_class = signature->java_class;

    if (is_java_array_class(jEnv, java_class)) {
        jclass component_class;
        
        signature->type = JAVA_SIGNATURE_ARRAY;

        component_class = get_java_array_component_class(cx, jEnv, java_class);
        if (!component_class)
            return JS_FALSE;

        signature->array_component_signature =
            jsj_GetJavaClassDescriptor(cx, jEnv, component_class);
        if (!signature->array_component_signature) {
            (*jEnv)->DeleteLocalRef(jEnv, component_class);
            return JS_FALSE;
        }
    } else {
        signature->type = get_signature_type(cx, signature);
    }
    return JS_TRUE;
}

/*
 * Convert from JavaSignatureChar enumeration to single-character
 * signature used by the JDK and JNI methods.
 */
static char
get_jdk_signature_char(JavaSignatureChar type)
{
    return "XVZCBSIJFD[LLLLLL"[(int)type];
}

/*
 * Convert a JavaSignature object into a string format as used by
 * the JNI functions, e.g. java.lang.Object ==> "Ljava/lang/Object;"
 * The caller is responsible for freeing the resulting string.
 *
 * If an error is encountered, NULL is returned and an error reported.
 */
const char *
jsj_ConvertJavaSignatureToString(JSContext *cx, JavaSignature *signature)
{
    char *sig;

    if (IS_OBJECT_TYPE(signature->type)) {
        /* A non-array object class */
        sig = JS_smprintf("L%s;", signature->name);
        if (sig)
            jsj_MakeJNIClassname(sig);

    } else if (signature->type == JAVA_SIGNATURE_ARRAY) {
        /* An array class */
        const char *component_signature_string;

        component_signature_string =
            jsj_ConvertJavaSignatureToString(cx, signature->array_component_signature);
        if (!component_signature_string)
            return NULL;
        sig = JS_smprintf("[%s", component_signature_string);
        JS_free(cx, (char*)component_signature_string);

    } else {
        /* A primitive class */
        sig = JS_smprintf("%c", get_jdk_signature_char(signature->type));
    }

    if (!sig) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }
    return sig;
}

/*
 * Convert a JavaSignature object into a human-readable string format as seen
 * in Java source files, e.g. "byte", or "int[][]" or "java.lang.String".
 * The caller is responsible for freeing the resulting string.
 *
 * If an error is encountered, NULL is returned and an error reported.
 */
const char *
jsj_ConvertJavaSignatureToHRString(JSContext *cx,
                                   JavaSignature *signature)
{
    char *sig;
    JavaSignature *acs;

    if (signature->type == JAVA_SIGNATURE_ARRAY) {
        /* An array class */
        const char *component_signature_string;
        acs = signature->array_component_signature;
        component_signature_string =
            jsj_ConvertJavaSignatureToHRString(cx, acs);
        if (!component_signature_string)
            return NULL;
        sig = JS_smprintf("%s[]", component_signature_string);
        JS_free(cx, (char*)component_signature_string);

    } else {
        /* A primitive class or a non-array object class */
        sig = JS_strdup(cx, signature->name);
    }

    if (!sig) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }
    return sig;
}

static void
destroy_java_member_descriptor(JSContext *cx, JNIEnv *jEnv, JavaMemberDescriptor *member_descriptor)
{
    JavaMethodSpec *method, *next_method;
    if (member_descriptor->field)
        jsj_DestroyFieldSpec(cx, jEnv, member_descriptor->field);

    method = member_descriptor->methods;
    while (method) {
        next_method = method->next;
        jsj_DestroyMethodSpec(cx, jEnv, method);
        method = next_method;
    }

    if (member_descriptor->invoke_func_obj)
        JS_RemoveRoot(cx, &member_descriptor->invoke_func_obj);

    JS_FREE_IF(cx, (char *)member_descriptor->name);
    JS_free(cx, member_descriptor);
}

static void
destroy_class_member_descriptors(JSContext *cx, JNIEnv *jEnv, JavaMemberDescriptor *member_descriptor)
{
    JavaMemberDescriptor *next_member;
    
    while (member_descriptor) {
        next_member = member_descriptor->next;
        destroy_java_member_descriptor(cx, jEnv, member_descriptor);
        member_descriptor = next_member;
    }
}

static void
destroy_class_descriptor(JSContext *cx, JNIEnv *jEnv, JavaClassDescriptor *class_descriptor)
{
    JS_FREE_IF(cx, (char *)class_descriptor->name);
    if (class_descriptor->java_class)
        (*jEnv)->DeleteGlobalRef(jEnv, class_descriptor->java_class);

    if (class_descriptor->array_component_signature)
        jsj_ReleaseJavaClassDescriptor(cx, jEnv, class_descriptor->array_component_signature);

    destroy_class_member_descriptors(cx, jEnv, class_descriptor->instance_members);
    destroy_class_member_descriptors(cx, jEnv, class_descriptor->static_members);
    destroy_class_member_descriptors(cx, jEnv, class_descriptor->constructors);
    JS_free(cx, class_descriptor);
}

static JavaClassDescriptor *
new_class_descriptor(JSContext *cx, JNIEnv *jEnv, jclass java_class)
{
    JavaClassDescriptor *class_descriptor;

    class_descriptor = (JavaClassDescriptor *)JS_malloc(cx, sizeof(JavaClassDescriptor));
    if (!class_descriptor)
        return NULL;
    memset(class_descriptor, 0, sizeof(JavaClassDescriptor));

    class_descriptor->name = jsj_GetJavaClassName(cx, jEnv, java_class);
    if (!class_descriptor->name)
        goto error;

    java_class = (*jEnv)->NewGlobalRef(jEnv, java_class);
    if (!java_class) {
        jsj_UnexpectedJavaError(cx, jEnv, "Unable to reference Java class");
        goto error;
    }
    class_descriptor->java_class = java_class;

    if (!compute_java_class_signature(cx, jEnv, class_descriptor))
        goto error;

    class_descriptor->modifiers =
        (*jEnv)->CallIntMethod(jEnv, java_class, jlClass_getModifiers);
    class_descriptor->ref_count = 1;

    if (!JSJ_HashTableAdd(java_class_reflections, java_class, class_descriptor,
                          (void*)jEnv))
        goto error;

    return class_descriptor;

error:
    destroy_class_descriptor(cx, jEnv, class_descriptor);
    return NULL;
}

/* Trivial helper for jsj_DiscardJavaClassReflections(), below */
JS_STATIC_DLL_CALLBACK(JSIntn)
enumerate_remove_java_class(JSJHashEntry *he, JSIntn i, void *arg)
{
    JSJavaThreadState *jsj_env = (JSJavaThreadState *)arg;
    JavaClassDescriptor *class_descriptor;

    class_descriptor = (JavaClassDescriptor*)he->value;

    destroy_class_descriptor(jsj_env->cx, jsj_env->jEnv, class_descriptor);

    return HT_ENUMERATE_REMOVE;
}

/* This shutdown routine discards all JNI references to Java objects
   that have been reflected into JS, even if there are still references
   to them from JS. */
void
jsj_DiscardJavaClassReflections(JNIEnv *jEnv)
{
    JSJavaThreadState *jsj_env;
    char *err_msg;
    JSContext *cx;

    /* Get the per-thread state corresponding to the current Java thread */
    jsj_env = jsj_MapJavaThreadToJSJavaThreadState(jEnv, &err_msg);
    JS_ASSERT(jsj_env);
    if (!jsj_env)
        return;

    /* Get the JSContext that we're supposed to use for this Java thread */
    cx = jsj_env->cx;
    if (!cx) {
        /* We called spontaneously into JS from Java, rather than from JS into
           Java and back into JS.  Invoke a callback to obtain/create a
           JSContext for us to use. */
        if (JSJ_callbacks->map_jsj_thread_to_js_context) {
#ifdef OJI
            cx = JSJ_callbacks->map_jsj_thread_to_js_context(jsj_env,
                                                             NULL, /* FIXME: What should this argument be ? */
                                                             jEnv, &err_msg);
#else
            cx = JSJ_callbacks->map_jsj_thread_to_js_context(jsj_env,
                                                             jEnv, &err_msg);
#endif
            if (!cx)
                return;
        } else {
            err_msg = JS_smprintf("Unable to find/create JavaScript execution "
                                  "context for JNI thread 0x%08x", jEnv);
            jsj_LogError(err_msg);
            free(err_msg);
            return;
        }
    }

    if (java_class_reflections) {
        JSJ_HashTableEnumerateEntries(java_class_reflections,
                                      enumerate_remove_java_class,
                                      (void*)jsj_env);
        JSJ_HashTableDestroy(java_class_reflections);
        java_class_reflections = NULL;
    }
}

extern JavaClassDescriptor *
jsj_GetJavaClassDescriptor(JSContext *cx, JNIEnv *jEnv, jclass java_class)
{
    JavaClassDescriptor *class_descriptor = NULL;

#ifdef JSJ_THREADSAFE
    PR_EnterMonitor(java_class_reflections_monitor);
#endif

    if (java_class_reflections) {
        class_descriptor = JSJ_HashTableLookup(java_class_reflections,
                                               (const void *)java_class,
                                               (void*)jEnv);
    }
    if (!class_descriptor) {
        class_descriptor = new_class_descriptor(cx, jEnv, java_class);

#ifdef JSJ_THREADSAFE
        PR_ExitMonitor(java_class_reflections_monitor);
#endif
        return class_descriptor;
    }

#ifdef JSJ_THREADSAFE
    PR_ExitMonitor(java_class_reflections_monitor);
#endif

    JS_ASSERT(class_descriptor->ref_count > 0);
    class_descriptor->ref_count++;
    return class_descriptor;
}

void
jsj_ReleaseJavaClassDescriptor(JSContext *cx, JNIEnv *jEnv, JavaClassDescriptor *class_descriptor)
{
#if 0
    /* The ref-counting code doesn't work very well because cycles in the data
       structures routinely lead to uncollectible JavaClassDescriptor's.  Skip it. */
    JS_ASSERT(class_descriptor->ref_count >= 1);
    if (!--class_descriptor->ref_count) {
        JSJ_HashTableRemove(java_class_reflections,
                            class_descriptor->java_class, (void*)jEnv);
        destroy_class_descriptor(cx, jEnv, class_descriptor);
    }
#endif
}

#ifdef JSJ_THREADSAFE
static PRMonitor *java_reflect_monitor = NULL;
#endif

static JSBool
reflect_java_methods_and_fields(JSContext *cx,
                                JNIEnv *jEnv,
                                JavaClassDescriptor *class_descriptor,
                                JSBool reflect_statics_only)
{
    JavaMemberDescriptor *member_descriptor;
    JSBool success;

    success = JS_TRUE;  /* optimism */

#ifdef JSJ_THREADSAFE
    PR_EnterMonitor(java_reflect_monitor);
#endif

    /* See if we raced with another thread to reflect members of this class.
       If the status is REFLECT_COMPLETE, another thread beat us to it.  If
       the status is REFLECT_IN_PROGRESS, we've recursively called this
       function within a single thread.  Either way, we're done. */
    if (reflect_statics_only) {
        if (class_descriptor->static_members_reflected != REFLECT_NO)
            goto done;
        class_descriptor->static_members_reflected = REFLECT_IN_PROGRESS;
    } else {
        if (class_descriptor->instance_members_reflected != REFLECT_NO)
            goto done;
        class_descriptor->instance_members_reflected = REFLECT_IN_PROGRESS;
    }
    
    if (!jsj_ReflectJavaMethods(cx, jEnv, class_descriptor, reflect_statics_only))
        goto error;
    if (!jsj_ReflectJavaFields(cx, jEnv, class_descriptor, reflect_statics_only))
        goto error;

    if (reflect_statics_only) {
        member_descriptor = class_descriptor->static_members;
        while (member_descriptor) {
            class_descriptor->num_static_members++;
            member_descriptor = member_descriptor->next;
        }
        class_descriptor->static_members_reflected = REFLECT_COMPLETE;
    } else {
        member_descriptor = class_descriptor->instance_members;
        while (member_descriptor) {
            class_descriptor->num_instance_members++;
            member_descriptor = member_descriptor->next;
        }
        class_descriptor->instance_members_reflected = REFLECT_COMPLETE;
    }

done:
#ifdef JSJ_THREADSAFE
    PR_ExitMonitor(java_reflect_monitor);
#endif
    return success;

error:
    success = JS_FALSE;
    goto done;
}

JavaMemberDescriptor *
jsj_GetClassStaticMembers(JSContext *cx,
                          JNIEnv *jEnv,
                          JavaClassDescriptor *class_descriptor)
{
    if (class_descriptor->static_members_reflected != REFLECT_COMPLETE)
        reflect_java_methods_and_fields(cx, jEnv, class_descriptor, JS_TRUE);
    return class_descriptor->static_members;
}

JavaMemberDescriptor *
jsj_GetClassInstanceMembers(JSContext *cx,
                            JNIEnv *jEnv,
                            JavaClassDescriptor *class_descriptor)
{
    if (class_descriptor->instance_members_reflected != REFLECT_COMPLETE)
        reflect_java_methods_and_fields(cx, jEnv, class_descriptor, JS_FALSE);
    return class_descriptor->instance_members;
}

JavaMemberDescriptor *
jsj_LookupJavaStaticMemberDescriptorById(JSContext *cx,
                                         JNIEnv *jEnv,
                                         JavaClassDescriptor *class_descriptor,
                                         jsid id)
{
    JavaMemberDescriptor *member_descriptor;

    member_descriptor = jsj_GetClassStaticMembers(cx, jEnv, class_descriptor);
    while (member_descriptor) {
        if (id == member_descriptor->id)
            return member_descriptor;
        member_descriptor = member_descriptor->next;
    }
    return NULL;
}

JavaMemberDescriptor *
jsj_GetJavaStaticMemberDescriptor(JSContext *cx,
                                  JNIEnv *jEnv, 
                                  JavaClassDescriptor *class_descriptor,
                                  jstring member_name_jstr)
{
    JavaMemberDescriptor *member_descriptor;
    jsid id;

    if (!JavaStringToId(cx, jEnv, member_name_jstr, &id))
        return NULL;

    member_descriptor = jsj_LookupJavaStaticMemberDescriptorById(cx, jEnv, class_descriptor, id);
    if (member_descriptor)
        return member_descriptor;

    member_descriptor = JS_malloc(cx, sizeof(JavaMemberDescriptor));
    if (!member_descriptor)
        return NULL;
    memset(member_descriptor, 0, sizeof(JavaMemberDescriptor));

    member_descriptor->name = jsj_DupJavaStringUTF(cx, jEnv, member_name_jstr);
    if (!member_descriptor->name) {
        JS_free(cx, member_descriptor);
        return NULL;
    }
    member_descriptor->id = id;

    member_descriptor->next = class_descriptor->static_members;
    class_descriptor->static_members = member_descriptor;

    return member_descriptor;
}

JavaMemberDescriptor *
jsj_GetJavaClassConstructors(JSContext *cx,
                             JavaClassDescriptor *class_descriptor)
{
    JavaMemberDescriptor *member_descriptor;

    if (class_descriptor->constructors)
        return class_descriptor->constructors;

    member_descriptor = JS_malloc(cx, sizeof(JavaMemberDescriptor));
    if (!member_descriptor)
        return NULL;
    memset(member_descriptor, 0, sizeof(JavaMemberDescriptor));

    member_descriptor->name = JS_strdup(cx, "<init>");
    if (!member_descriptor->name) {
        JS_free(cx, member_descriptor);
        return NULL;
    }

    class_descriptor->constructors = member_descriptor;

    return member_descriptor;
}

JavaMemberDescriptor *
jsj_LookupJavaClassConstructors(JSContext *cx, JNIEnv *jEnv,
                                JavaClassDescriptor *class_descriptor)
{
    if (class_descriptor->static_members_reflected != REFLECT_COMPLETE)
        reflect_java_methods_and_fields(cx, jEnv, class_descriptor, JS_TRUE);
    return class_descriptor->constructors;
}

JavaMemberDescriptor *
jsj_LookupJavaMemberDescriptorById(JSContext *cx, JNIEnv *jEnv,
                                   JavaClassDescriptor *class_descriptor,
                                   jsid id)
{
    JavaMemberDescriptor *member_descriptor;

    member_descriptor = jsj_GetClassInstanceMembers(cx, jEnv, class_descriptor);
    while (member_descriptor) {
        if (id == member_descriptor->id)
            return member_descriptor;
        member_descriptor = member_descriptor->next;
    }
    return NULL;
}

JavaMemberDescriptor *
jsj_GetJavaMemberDescriptor(JSContext *cx,
                            JNIEnv *jEnv, 
                            JavaClassDescriptor *class_descriptor,
                            jstring member_name_jstr)
{
    JavaMemberDescriptor *member_descriptor;
    jsid id;

    if (!JavaStringToId(cx, jEnv, member_name_jstr, &id))
        return NULL;

    member_descriptor = jsj_LookupJavaMemberDescriptorById(cx, jEnv, class_descriptor, id);
    if (member_descriptor)
        return member_descriptor;

    member_descriptor = JS_malloc(cx, sizeof(JavaMemberDescriptor));
    if (!member_descriptor)
        return NULL;
    memset(member_descriptor, 0, sizeof(JavaMemberDescriptor));

    member_descriptor->name = jsj_DupJavaStringUTF(cx, jEnv, member_name_jstr);
    if (!member_descriptor->name) {
        JS_free(cx, member_descriptor);
        return NULL;
    }
    member_descriptor->id = id;

    member_descriptor->next = class_descriptor->instance_members;
    class_descriptor->instance_members = member_descriptor;

    return member_descriptor;
}

JSBool
jsj_InitJavaClassReflectionsTable()
{
    if (!java_class_reflections) {
        java_class_reflections =
            JSJ_NewHashTable(64, jsj_HashJavaObject, jsj_JavaObjectComparator,
                            NULL, NULL, NULL);

        if (!java_class_reflections)
            return JS_FALSE;

#ifdef JSJ_THREADSAFE
        java_class_reflections_monitor =
                (struct PRMonitor *) PR_NewMonitor();
        if (!java_class_reflections_monitor)
            return JS_FALSE;

        java_reflect_monitor =
                (struct PRMonitor *) PR_NewMonitor();
        if (!java_reflect_monitor)
            return JS_FALSE;
#endif
    }
    
    return JS_TRUE;
}
