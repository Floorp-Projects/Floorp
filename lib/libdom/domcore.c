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
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * Core DOM stuff -- initialization, stub functions, DOMImplementation, etc.
 */

#include "dom_priv.h"

JSBool
DOM_Init(JSContext *cx, JSObject *scope) {
    JSObject *node, *cdata;
    JSBool ok = (( node = dom_NodeInit(cx, scope)) &&
                 dom_AttributeInit(cx, scope, node) &&
                 dom_ElementInit(cx, scope, node) &&
                 ( cdata = dom_CharacterDataInit(cx, scope, node)) &&
                 dom_TextInit(cx, scope, cdata) &&
                 dom_CommentInit(cx, scope, cdata));
#ifdef PERIGNON
    ok &= (dom_StyleSelectorInit(cx, scope) &&
           dom_TagsObjectInit(cx, scope));
#endif
    return ok;
}

static char *exception_names[] = {
    "NO_ERR",
    "INDEX_SIZE_ERR",
    "WSTRING_SIZE_ERR",
    "HIERARCHY_REQUEST_ERR",
    "WRONG_DOCUMENT_ERR",
    "INVALID_NAME_ERR",
    "NO_DATA_ALLOWED_ERR",
    "NO_MODIFICATION_ALLOWED_ERR",
    "NOT_FOUND_ERR",
    "NOT_SUPPORTED_ERR",
    "INUSE_ATTRIBUTE_ERR",
    "UNSUPPORTED_DOCUMENT_ERR"
};

JSBool
DOM_SignalException(JSContext *cx, DOM_ExceptionCode exception)
{
    JS_ReportError(cx, "DOM Exception: %s", exception_names[exception]);
    return JS_TRUE;
}

JSBool
DOM_InsertBeforeStub(JSContext *cx, DOM_Node *node, DOM_Node *child,
                     DOM_Node *ref, JSBool before)
{
    return JS_TRUE;
}

JSBool
DOM_ReplaceChildStub(JSContext *cx, DOM_Node *node, DOM_Node *child,
                     DOM_Node *old, JSBool before)
{
    return JS_TRUE;
}

JSBool
DOM_RemoveChildStub(JSContext *cx, DOM_Node *node, DOM_Node *old,
                    JSBool before)
{
    return JS_TRUE;
}

JSBool
DOM_AppendChildStub(JSContext *cx, DOM_Node *node, DOM_Node *child,
                    JSBool before)
{
    return JS_TRUE;
}

void
DOM_DestroyNodeStub(JSContext *cx, DOM_Node *node)
{
    if (node->data)
        JS_free(cx, node->data);
}

JSObject *
DOM_ReflectNodeStub(JSContext *cx, DOM_Node *node)
{
    return NULL;
}

JSBool
DOM_SetAttributeStub(JSContext *cx, DOM_Element *element, const char *name,
                     const char *value)
{
    return JS_TRUE;
}

const char *
DOM_GetAttributeStub(JSContext *cx, DOM_Element *element, const char *name,
                     JSBool *cacheable)
{
    *cacheable = JS_FALSE;
    return "#none";
}

intN
DOM_GetNumAttrsStub(JSContext *cx, DOM_Element *element, JSBool *cacheable)
{
    *cacheable = JS_FALSE;
    return -1;
}

#ifdef DEBUG_shaver
int LM_Node_indent = 0;
#endif

JSObject *
DOM_ObjectForNodeDowncast(JSContext *cx, DOM_Node *node)
{
    if (!node)
        return NULL;

    if (!node->mocha_object)
        node->mocha_object = node->ops->reflectNode(cx, node);
    return node->mocha_object;
}

void
DOM_DestroyTree(JSContext *cx, DOM_Node *top)
{
    DOM_Node *iter, *next;
    for (iter = top->child; iter; iter = next) {
        next = iter->sibling;
        if (iter->mocha_object) {
            iter->prev_sibling = iter->parent = iter->sibling = 
                iter->child = NULL;
#ifdef DEBUG_shaver
            fprintf(stderr, "node %s type %d has mocha_object\n",
                    iter->name ? iter->name : "<none>", iter->type);
#endif
        } else {
            DOM_DestroyTree(cx, iter);
        }
    }
    DOM_DestroyNode(cx, top);
}

/*
 * DOMImplementation
 */

static void
dom_finalize(JSContext *cx, JSObject *obj)
{
}

JSClass DOM_DOMClass = {
    "DOM", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,  dom_finalize
};

#if 0
static JSBool
dom_hasFeature(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
           jsval *rval)
{
    char *feature, *version;
    if (!JS_ConvertArguments(cx, argc, argv, "ss", &feature, &version))
    return JS_TRUE;

    if (!strcmp(feature, "HTML") &&
    !strcmp(version, "1"))
        *rval = JSVAL_TRUE;
    else
    *rval = JSVAL_FALSE;
    return JS_TRUE;
}

static JSFunctionSpec dom_methods[] = {
    {"hasFeature",  dom_hasFeature,     2},
    {0}
};
#endif

