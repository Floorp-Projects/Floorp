/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * DOM Attribute implementation.
 */

#include "dom_priv.h"

static JSBool
attribute_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    return JS_TRUE;
}

static JSBool
attribute_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    return JS_TRUE;
}

static JSClass DOM_AttributeClass = {
    "Attribute", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub, attribute_getter, attribute_setter,
    JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,   JS_FinalizeStub
};

/* must start after the DOM_NODE tinyids */
enum {
    ATTRIBUTE_SPECIFIED = -1,
    ATTRIBUTE_NAME = -2,
    ATTRIBUTE_VALUE = -3,
};

static JSPropertySpec attribute_props[] = {
    {"specified",       ATTRIBUTE_SPECIFIED,    JSPROP_ENUMERATE, 0, 0},
    {"name",            ATTRIBUTE_NAME,         JSPROP_ENUMERATE, 0, 0},
    {"value",           ATTRIBUTE_VALUE,        JSPROP_ENUMERATE, 0, 0},
    {0}
};

JSObject *
DOM_NewAttributeObject(JSContext *cx, DOM_Attribute *attr)
{
    DOM_Node *node = (DOM_Node *)attr;
    JSObject *obj;
    JSString *str;
    jsval v;

    obj = JS_ConstructObject(cx, &DOM_AttributeClass, NULL, NULL);
    if (!obj)
        return NULL;

    if (!JS_SetPrivate(cx, obj, attr))
        return NULL;

    str = JS_NewStringCopyZ(cx, node->name);
    v = STRING_TO_JSVAL(str);
    if (!str ||
        !JS_SetProperty(cx, obj, "name", &v))
        return NULL;

    v = JSVAL_TRUE;
    if (!JS_SetProperty(cx, obj, "specified", &v))
        return NULL;
    node->mocha_object = obj;
    return obj;
}

DOM_Attribute *
DOM_NewAttribute(const char *name, const char *value, DOM_Element *element)
{
    DOM_Attribute *attr;
    attr = XP_NEW_ZAP(DOM_Attribute);
    if (!attr)
        return NULL;

    attr->node.name = XP_STRDUP(name);
    attr->element = element;
    return attr;
}

JSObject *
DOM_ObjectForAttribute(JSContext *cx, DOM_Attribute *attr)
{
    if (!attr)
        return NULL;

    if (attr->node.mocha_object)
        return attr->node.mocha_object;

    return DOM_NewAttributeObject(cx, attr);
}

static JSBool
Attribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

JSObject *
dom_AttributeInit(JSContext *cx, JSObject *scope, JSObject *node_proto)
{
    JSObject *proto;
    proto = JS_InitClass(cx, scope, node_proto, &DOM_AttributeClass,
                         Attribute, 0,
                         attribute_props, NULL,
                         NULL, NULL);
    return proto;
}
