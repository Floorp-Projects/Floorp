/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
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

 /* -*- Mode: C; tab-width: 8 -*-
 * Copyright (C) 1998-1999 Netscape Communications Corporation, All Rights Reserved.
 */

/*
 * This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * It contains the native code implementation of JS's JavaMember class.
 * JavaMember's are a strange beast required only to handle the special case
 * of a public field and a public method that appear in the same class and
 * have the same name.  When such a field/method is used in Java, the compiler
 * can statically determine from context whether the field or the method is
 * being referenced, but that is not possible with JavaScript.  For example:
 *
 * ambiguousVal = javaObj.fieldOrMethod; // ambiguousVal is a JavaMember object
 * a = ambiguousVal();                   // ambiguousVal used as a method value
 * b = ambiguousVal + 4;                 // ambiguousVal used as a field value
 *
 * A JavaMember instance carries both the captured value of the Java field and
 * the method value until the context that the value is to be used in is known,
 * at which point conversion forces the use of one or the other.
 */

#include <stdlib.h>
#include <string.h>

#include "jsj_private.h"      /* LiveConnect internals */

/* Private, native portion of a JavaMember */
typedef struct JavaMethodOrFieldValue {
    jsval method_val;
    jsval field_val;
} JavaMethodOrFieldValue;

JSObject *
jsj_CreateJavaMember(JSContext *cx, jsval method_val, jsval field_val)
{
    JavaMethodOrFieldValue *member_val;
    JSObject *JavaMember_obj;
    
    member_val = (JavaMethodOrFieldValue *)JS_malloc(cx, sizeof(*member_val));
    if (!member_val)
        return NULL;
    
    JavaMember_obj = JS_NewObject(cx, &JavaMember_class, 0, 0);
    if (!JavaMember_obj) {
        JS_free(cx, member_val);
        return NULL;
    }

    JS_SetPrivate(cx, JavaMember_obj, (void *)member_val);
    member_val->method_val = method_val;
    JS_AddNamedRoot(cx, &member_val->method_val, "&member_val->method_val");
    member_val->field_val = field_val;
    if (JSVAL_IS_GCTHING(field_val))
        JS_AddNamedRoot(cx, &member_val->field_val, "&member_val->field_val");

    return JavaMember_obj;
}

JS_STATIC_DLL_CALLBACK(void)
JavaMember_finalize(JSContext *cx, JSObject *obj)
{
    JavaMethodOrFieldValue *member_val;

    member_val = JS_GetPrivate(cx, obj);
    if (!member_val)
        return;

    JS_RemoveRoot(cx, &member_val->method_val);
    if (JSVAL_IS_GCTHING(member_val->field_val))
        JS_RemoveRoot(cx, &member_val->field_val);
    JS_free(cx, member_val);
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaMember_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    JavaMethodOrFieldValue *member_val;
        
    member_val = JS_GetPrivate(cx, obj);
    if (!member_val) {
        if (type == JSTYPE_OBJECT) {
            *vp = OBJECT_TO_JSVAL(obj);
            return JS_TRUE;
        }
        
        JS_ReportError(cx, "illegal operation on JavaObject prototype object");
        return JS_FALSE;
    }

    switch (type) {
    case JSTYPE_VOID:
    case JSTYPE_STRING:
    case JSTYPE_NUMBER:
    case JSTYPE_BOOLEAN:
    case JSTYPE_OBJECT:
        *vp = member_val->field_val;
        return JS_TRUE;

    case JSTYPE_FUNCTION:
        *vp = member_val->method_val;
        return JS_TRUE;

    default:
        JS_ASSERT(0);
        return JS_FALSE;
    }
}

/*
 * This function exists only to make JavaMember's Call'able.  The way the JS
 * engine is written now, it's never actually called because when a JavaMember
 * is invoked, it's converted to a JS function via JavaMember_convert().
 */
JS_STATIC_DLL_CALLBACK(JSBool)
JavaMember_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JS_ASSERT(0);
    return JS_TRUE;
}

JSClass JavaMember_class = {
    "JavaMember", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, 
    JavaMember_convert, JavaMember_finalize,

    NULL, /* getObjectOps */
    NULL, /* checkAccess */
    JavaMember_Call,
    NULL, /* construct */
    NULL, /* xdrObject */
    NULL, /* hasInstance */
    NULL, /* mark */
    0,    /* spare */
};

JSBool
jsj_init_JavaMember(JSContext *cx, JSObject *global_obj)
{
    if (!JS_InitClass(cx, global_obj, 
        0, &JavaMember_class, 0, 0,
        0, 0,
        0, 0))
        return JS_FALSE;

    return JS_TRUE;
}
