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

#include "dom_priv.h"

/*
 * DOM CharacterData, Text and Comment implementations.
 */

/* must start after the DOM_NODE tinyids */
enum {
    DOM_CDATA_DATA = -14,
    DOM_CDATA_LENGTH = -15
};

static JSBool
cdata_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    intN slot;
    DOM_CharacterData *cdata;
    JSString *str;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;
    
    slot = JSVAL_TO_INT(id);
    /* Look, ma!  Inheritance! */
    if (slot <= DOM_NODE_NODENAME &&
        slot >= DOM_NODE_HASCHILDNODES) {
        return dom_node_getter(cx, obj, id, vp);
    }
    
    cdata = (DOM_CharacterData *)JS_GetPrivate(cx, obj);
    if (!cdata)
        return JS_TRUE;

    switch (slot) {
      case DOM_CDATA_DATA:
        str = JS_NewStringCopyN(cx, cdata->data, cdata->len);
        if (!str)
            return JS_FALSE;
        *vp = STRING_TO_JSVAL(str);
        /* XXX cache the string */
        break;
      case DOM_CDATA_LENGTH:
        /* XXX it's a long, see... */
        *vp = INT_TO_JSVAL(cdata->len);
        break;
      default:;
    }

    return JS_TRUE;
}

static JSBool
cdata_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    return JS_TRUE;
}

static JSClass DOM_CDataClass = {
    "CharacterData", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub, cdata_getter,     cdata_setter,
    JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,   JS_FinalizeStub
};

static JSPropertySpec cdata_props[] = {
    {"data",	DOM_CDATA_DATA,		JSPROP_ENUMERATE | JSPROP_READONLY, 
     0, 0},
    {"length",	DOM_CDATA_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY,
     0, 0},
    {0}
};

static JSFunctionSpec cdata_methods[] = {
    {0}
};

static JSBool
CharacterData(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
    return JS_TRUE;
}

JSObject *
dom_CharacterDataInit(JSContext *cx, JSObject *scope, JSObject *node_proto)
{
    JSObject *proto;
    proto = JS_InitClass(cx, scope, node_proto, &DOM_CDataClass,
			 CharacterData, 0,
			 cdata_props, cdata_methods,
			 NULL, NULL);
    return proto;
}

/*
 * Text
 */

static JSClass DOM_TextClass = {
    "Text", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub, cdata_getter,      cdata_setter,
    JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,   JS_FinalizeStub
};

static JSFunctionSpec text_methods[] = {
    {0}
};

JSObject *
DOM_NewTextObject(JSContext *cx, DOM_Text *text)
{
    DOM_Node *node = (DOM_Node *)text;
    JSObject *obj;

    obj = JS_ConstructObject(cx, &DOM_TextClass, NULL, NULL);
    if (!obj)
        return NULL;

    if (!JS_SetPrivate(cx, obj, text)) {
        return NULL;
    }

    node->mocha_object = obj;
    return obj;
}

DOM_Text *
DOM_NewText(const char *data, int64 length)
{
    XP_ASSERT((0 && "DOM_NewText NYI"));
    return NULL;
}

JSObject *
DOM_ObjectForText(JSContext *cx, DOM_Text *text)
{
    if (!text)
        return NULL;

    if (text->cdata.node.mocha_object)
        return text->cdata.node.mocha_object;
    
    return DOM_NewTextObject(cx, text);
}

static JSBool
Text(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

JSObject *
dom_TextInit(JSContext *cx, JSObject *scope, JSObject *cdata_proto)
{
    JSObject *proto;
    proto = JS_InitClass(cx, scope, cdata_proto, &DOM_TextClass,
			 Text, 0,
			 NULL, text_methods,
			 NULL, NULL);
    return proto;
}

/*
 * Comment
 */

JSObject *
dom_CommentInit(JSContext *cx, JSObject *scope, JSObject *cdata_proto)
{
    /* Do we need a custom proto here? */
    return cdata_proto;
}
