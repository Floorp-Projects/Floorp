/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * DOM Element implementation.
 */

#include "dom_priv.h"

static JSPropertySpec element_props[] = {
    {"tagName",		DOM_ELEMENT_TAGNAME,	JSPROP_READONLY,	0, 0},
    {0}
};

static JSBool
element_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    intN slot;
    DOM_Element *element;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    /* 
     * Look, ma!  Inheritance!
     * We handle .attributes ourselves because we return something other
     * than null, unlike every other Node subclass.
     */
    if (slot != DOM_NODE_ATTRIBUTES &&
        slot <= DOM_NODE_NODENAME &&
        slot >= DOM_NODE_HASCHILDNODES) {
        return dom_node_getter(cx, obj, id, vp);
    }

    element = (DOM_Element *)JS_GetPrivate(cx, obj);
    if (!element)
        return JS_TRUE;

    if (slot == DOM_ELEMENT_TAGNAME) {
        JSString *tagName;
#ifdef DEBUG_shaver_0
            fprintf(stderr, "getting tagName %s", 
                    element->tagName ? element->tagName : "#tag");
            tagName = JS_InternString(cx, element->tagName ?
                                      element->tagName : "#tag");
            fprintf(stderr, ": %x\n", tagName);
#else
            
            tagName = JS_NewStringCopyZ(cx,
                            element->tagName ? element->tagName : "#tag");
#endif
        if (!tagName)
            return JS_FALSE;
        *vp = STRING_TO_JSVAL(tagName);
    }

    return JS_TRUE;
}

static JSBool
element_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    return JS_TRUE;
}

static JSClass DOM_ElementClass = {
    "Element", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  element_getter, element_setter,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub, dom_node_finalize
};

static JSBool
element_getAttribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                     jsval *rval)
{
    DOM_Element *element;
    JSString *name;
    const char *value;
    JSBool cache;

    if (!JS_ConvertArguments(cx, argc, argv, "S", &name))
        return JS_FALSE;

    element = (DOM_Element *)JS_GetPrivate(cx, obj);
    if (!element)
        return JS_TRUE;

    value = element->ops->getAttribute(cx, element, JS_GetStringBytes(name),
                                       &cache);
    if (value) {
        *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, value));
        if (!JSVAL_TO_STRING(*rval))
            return JS_FALSE;
    } else {
        *rval = STRING_TO_JSVAL(JS_GetEmptyStringValue(cx));
    }

    return JS_TRUE;
}

static JSBool
element_setAttribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                     jsval *rval)
{
    DOM_Element *element;
    JSString *name, *value;

    if (!JS_ConvertArguments(cx, argc, argv, "SS", &name, &value))
        return JS_FALSE;

    element = (DOM_Element *)JS_GetPrivate(cx, obj);
    if (!element)
        return JS_TRUE;

    return element->ops->setAttribute(cx, element, JS_GetStringBytes(name),
                                      JS_GetStringBytes(value));

}

static JSBool
element_removeAttribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                        jsval *rval)
{
    DOM_Element *element;
    JSString *name;

    if (!JS_ConvertArguments(cx, argc, argv, "S", &name))
        return JS_FALSE;

    element = (DOM_Element *)JS_GetPrivate(cx, obj);
    if (!element)
        return JS_TRUE;

    /* NULL setAttribute call indicates removal */
    return element->ops->setAttribute(cx, element, JS_GetStringBytes(name),
                                      NULL);

}

static JSBool
element_getAttributeNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                         jsval *rval)
{
    return JS_TRUE;
}

static JSBool
element_setAttributeNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                         jsval *rval)
{
    return JS_TRUE;
}

static JSBool
element_removeAttributeNode(JSContext *cx, JSObject *obj, uintN argc,
                            jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSFunctionSpec element_methods[] = {
    {"setAttribute",	element_setAttribute, 2},
    {"getAttribute",	element_getAttribute, 1},
    {"removeAttribute", element_removeAttribute, 1},
    {"setAttributeNode", element_setAttributeNode, 1},
    {"getAttributeNode", element_getAttributeNode, 1},
    {"removeAttributeNode", element_removeAttributeNode, 1},
    {0}
};

DOM_Element *
DOM_NewElement(const char *tagName, DOM_ElementOps *eleops, char *name,
               DOM_NodeOps *nodeops, uintN nattrs)
{
    DOM_Node *node;
    DOM_Element *element = XP_NEW_ZAP(DOM_Element);
    if (!element)
        return NULL;
    
    node = (DOM_Node *)element;
    node->type = NODE_TYPE_ELEMENT;
    node->name = name;
    node->ops = nodeops;
    
    element->tagName = tagName;
    element->ops = eleops;
    element->nattrs = nattrs;
    return element;
}

JSObject *
DOM_NewElementObject(JSContext *cx, DOM_Element *element)
{
    JSObject *obj;
    
    obj = JS_ConstructObject(cx, &DOM_ElementClass, NULL, NULL);
    if (!obj)
        return NULL;
    if (!JS_SetPrivate(cx, obj, element))
        return NULL;
    element->node.mocha_object = obj;
    return obj;
}

JSObject *
DOM_ObjectForElement(JSContext *cx, DOM_Element *element)
{
    if (!element)
        return NULL;
    if (element->node.mocha_object)
        return element->node.mocha_object;
    return DOM_NewElementObject(cx, element);
}

static JSBool
Element(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
    return JS_TRUE;
}

JSObject *
dom_ElementInit(JSContext *cx, JSObject *scope, JSObject *node_proto)
{
    JSObject *proto = JS_InitClass(cx, scope, node_proto, &DOM_ElementClass,
				   Element, 0,
				   element_props, element_methods,
				   NULL, NULL);
    if (!JS_DefineProperties(cx, proto, dom_node_props))
        return NULL;
    return proto;
}
