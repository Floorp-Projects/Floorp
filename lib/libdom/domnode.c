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
 * DOM Node, NodeList, NamedNodeMap implementation.
 */

#include "dom_priv.h"

#ifdef DEBUG_shaver
int DOM_node_indent = 0;
#endif

DOM_Node *
DOM_PopNode(DOM_Node *node)
{
    return node->parent;
}

JSBool
DOM_PushNode(DOM_Node *node, DOM_Node *parent)
{
    DOM_Node *iter;
    node->parent = parent;
    node->sibling = NULL;
    node->prev_sibling = NULL;
    node->child = NULL;

    /* First child */
    if (!parent->child) {
        parent->child = node;
        return JS_TRUE;
    }
    /*
     * XXX optimize by using parent->mocha_object to cache last child, and
     * XXX NULLing parent->mocha_object on PopNode?
     */
    for (iter = parent->child; iter->sibling; iter = iter->sibling)
        ;                       /* empty */
    XP_ASSERT(iter);
    iter->sibling = node;
    node->prev_sibling = iter;
    return JS_TRUE;
}

JSObject *
DOM_ObjectForNode(JSContext *cx, DOM_Node *node)
{
    if (!node)
        return NULL;
    if (node->mocha_object)
        return node->mocha_object;

    return DOM_NewNodeObject(cx, node);
}

JSBool
dom_node_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    intN slot;
    DOM_Node *node;
    JSString *str;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    node = (DOM_Node *)JS_GetPrivate(cx, obj);
    if (!node)
        return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    switch(slot) {
      case DOM_NODE_NODENAME:
        if (!node->name) {
            *vp = JSVAL_NULL; /* XXX '#nameless' or some such? */
            return JS_TRUE;
        }
#ifdef DEBUG_shaver_0
        str = JS_InternString(cx, node->name);
#else
        str = JS_NewStringCopyZ(cx, node->name);
#endif
        if (!str)
            return JS_FALSE;
        *vp = STRING_TO_JSVAL(str);
        return JS_TRUE;
      case DOM_NODE_NODETYPE:
        *vp = INT_TO_JSVAL(node->type);
        return JS_TRUE;
      case DOM_NODE_FIRSTCHILD:
        *vp = OBJECT_TO_JSVAL(DOM_ObjectForNodeDowncast(cx, node->child));
        return JS_TRUE;
      case DOM_NODE_NEXTSIBLING:
        *vp = OBJECT_TO_JSVAL(DOM_ObjectForNodeDowncast(cx, node->sibling));
        return JS_TRUE;
      case DOM_NODE_PREVIOUSSIBLING:
        *vp = OBJECT_TO_JSVAL(DOM_ObjectForNodeDowncast(cx,
                                                        node->prev_sibling));
        return JS_TRUE;
      case DOM_NODE_PARENTNODE:
        *vp = OBJECT_TO_JSVAL(DOM_ObjectForNodeDowncast(cx, node->parent));
        return JS_TRUE;
      case DOM_NODE_HASCHILDNODES:
        *vp = BOOLEAN_TO_JSVAL((JSBool)(node->child != NULL));
        return JS_TRUE;
      case DOM_NODE_ATTRIBUTES:
        /* only elements have non-null attributes */
        *vp = JSVAL_NULL;
        return JS_TRUE;
      default:
        XP_ASSERT(0);
        break;
    }
    return JS_TRUE;
}

JSBool
dom_node_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    return JS_TRUE;
}

void
dom_node_finalize(JSContext *cx, JSObject *obj)
{
    DOM_Node *priv = (DOM_Node *)JS_GetPrivate(cx, obj);
    if (!priv)
        return;
    priv->mocha_object = NULL;
    DOM_DestroyTree(cx, priv);
}

static JSClass DOM_NodeClass = {
    "Node", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub, dom_node_getter, dom_node_setter,
    JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,  dom_node_finalize
};

JSObject *
DOM_NewNodeObject(JSContext *cx, DOM_Node *node)
{
    JSObject *obj;

    obj = JS_ConstructObject(cx, &DOM_NodeClass, NULL, NULL);
    if (!obj)
        return NULL;

    if (!JS_SetPrivate(cx, obj, node)) {
        return NULL;
    }

    node->mocha_object = obj;

    return obj;
}

void
DOM_DestroyNode(JSContext *cx, DOM_Node *node)
{
    XP_ASSERT(!node->mocha_object);
    if (node->ops && node->ops->destroyNode)
        node->ops->destroyNode(cx, node);
    if (node->name && node->type != NODE_TYPE_TEXT)
        JS_free(cx, node->name);
    JS_free(cx, node);
}

#define REMOVE_FROM_TREE(node)                                                \
PR_BEGIN_MACRO                                                                \
if (node->prev_sibling)                                                       \
    node->prev_sibling->sibling = node->sibling;                              \
if (node->sibling)                                                            \
    node->sibling->prev_sibling = node->prev_sibling;                         \
node->sibling = node->prev_sibling = node->parent = NULL;                     \
PR_END_MACRO

#define FAIL_UNLESS_CHILD(node, refNode)                                      \
PR_BEGIN_MACRO                                                                \
if (refNode->parent != node) {                                                \
    /* XXX walk the tree looking for it? */                                   \
    DOM_SignalException(cx, DOM_NOT_FOUND_ERR);                               \
    return JS_FALSE;                                                          \
}                                                                             \
PR_END_MACRO

static JSBool
IsLegalChild(DOM_Node *node, DOM_Node *child)
{
    switch(node->type) {
      case NODE_TYPE_ATTRIBUTE:
        if (child->type == NODE_TYPE_TEXT ||
            child->type == NODE_TYPE_ENTITY_REF)
            return JS_TRUE;
        return JS_FALSE;
        if (child->type == NODE_TYPE_ELEMENT ||
            child->type == NODE_TYPE_COMMENT ||
            child->type == NODE_TYPE_TEXT ||
            child->type == NODE_TYPE_PI ||
            child->type == NODE_TYPE_CDATA ||
            child->type == NODE_TYPE_ENTITY_REF)
            return JS_TRUE;
        return JS_FALSE;
      case NODE_TYPE_ELEMENT:
      case NODE_TYPE_DOCFRAGMENT:
      case NODE_TYPE_ENTITY_REF:
        if (child->type == NODE_TYPE_ELEMENT ||
            child->type == NODE_TYPE_PI ||
            child->type == NODE_TYPE_COMMENT ||
            child->type == NODE_TYPE_TEXT ||
            child->type == NODE_TYPE_CDATA ||
            child->type == NODE_TYPE_ENTITY_REF)
            return JS_TRUE;
        return JS_FALSE;
      case NODE_TYPE_DOCUMENT:
        if(child->type == NODE_TYPE_PI ||
           child->type == NODE_TYPE_COMMENT ||
           child->type == NODE_TYPE_DOCTYPE)
            return JS_TRUE;
        if (child->type == NODE_TYPE_ELEMENT)
            /* XXX check to make sure it's the only one */
            return JS_TRUE;
        return JS_FALSE;
      case NODE_TYPE_DOCTYPE:
        if (child->type == NODE_TYPE_NOTATION ||
            child->type == NODE_TYPE_ENTITY)
            return JS_TRUE;
        return JS_FALSE;
      default:               /* PI, COMMENT, TEXT, CDATA, ENTITY, NOTATION */
        return JS_FALSE;
    }
}

#define CHECK_LEGAL_CHILD(node, child)                                        \
PR_BEGIN_MACRO                                                                \
    if (!IsLegalChild(node, child)) {                                         \
        DOM_SignalException(cx, DOM_HIERARCHY_REQUEST_ERR);                   \
        return JS_FALSE;                                                      \
    }                                                                         \
PR_END_MACRO

static JSBool
node_insertBefore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *vp)
{
    JSObject *newChild, *refChild;
    DOM_Node *newNode, *refNode, *node;

    if (!JS_ConvertArguments(cx, argc, argv, "oo", &newChild, &refChild))
        return JS_FALSE;

    node = (DOM_Node *)JS_GetPrivate(cx, obj);
    if (!node)
        return JS_TRUE;
    newNode = (DOM_Node *)JS_GetPrivate(cx, newChild);
    refNode = (DOM_Node *)JS_GetPrivate(cx, refChild);

    *vp = argv[0];              /* newChild */
    if (!newNode || !refNode) {
        return JS_TRUE;
    }

    CHECK_LEGAL_CHILD(node, newNode);
    FAIL_UNLESS_CHILD(node, refNode);
    if (!node->ops->insertBefore(cx, node, newNode, refNode, JS_TRUE))
        return JS_FALSE;
    REMOVE_FROM_TREE(newNode);

    newNode->parent = node;

    newNode->sibling = refNode;
    newNode->prev_sibling = refNode->prev_sibling;

    refNode->prev_sibling = newNode;

    return node->ops->insertBefore(cx, node, newNode, refNode, JS_TRUE);
}

static JSBool
node_replaceChild(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *vp)
{
    JSObject *oldChild, *newChild;
    DOM_Node *oldNode, *newNode, *node;

    if (!JS_ConvertArguments(cx, argc, argv, "oo", &newChild, &oldChild))
        return JS_FALSE;

    node = (DOM_Node *)JS_GetPrivate(cx, obj);
    if (!node)
        return JS_TRUE;
    newNode = (DOM_Node *)JS_GetPrivate(cx, newChild);
    oldNode = (DOM_Node *)JS_GetPrivate(cx, oldChild);

    *vp = argv[1];              /* oldChild */
    if (newNode == oldNode ||
        !newNode || !oldNode)
        return JS_TRUE;

    CHECK_LEGAL_CHILD(node, newNode);
    FAIL_UNLESS_CHILD(node, oldNode);
    if (!node->ops->replaceChild(cx, node, newNode, oldNode, JS_TRUE))
        return JS_FALSE;
    REMOVE_FROM_TREE(newNode);

    newNode->parent = node;
    oldNode->sibling->prev_sibling = newNode;
    oldNode->prev_sibling->sibling = newNode;

    oldNode->parent = oldNode->sibling = oldNode->prev_sibling = NULL;

    return node->ops->replaceChild(cx, node, newNode, oldNode, JS_FALSE);
}

static JSBool
node_removeChild(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *vp)
{
    JSObject *deadChild;
    DOM_Node *deadNode, *node;

    if (!JS_ConvertArguments(cx, argc, argv, "o", &deadChild))
        return JS_FALSE;

    *vp = argv[0];              /* deadChild */
    node = (DOM_Node *)JS_GetPrivate(cx, obj);
    deadNode = (DOM_Node *)JS_GetPrivate(cx, deadChild);
    if (!deadNode || !node)
        return JS_TRUE;

    FAIL_UNLESS_CHILD(node, deadNode);
    if (!node->ops->removeChild(cx, node, deadNode, JS_TRUE))
        return JS_FALSE;

    REMOVE_FROM_TREE(deadNode);

    return node->ops->removeChild(cx, node, deadNode, JS_FALSE);
}

static JSBool
node_appendChild(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *vp)
{
    JSObject *newChild;
    DOM_Node *newNode, *node, *iter;

    if (!JS_ConvertArguments(cx, argc, argv, "o", &newChild))
        return JS_FALSE;

    *vp = argv[0];              /* newChild */
    node = (DOM_Node *)JS_GetPrivate(cx, obj);
    newNode = (DOM_Node *)JS_GetPrivate(cx, obj);
    if (!node || !newNode)
        return JS_TRUE;

    CHECK_LEGAL_CHILD(node, newNode);
    if (node->ops->appendChild(cx, node, newNode, JS_TRUE))
    REMOVE_FROM_TREE(newNode);

    newNode->parent = node;

    iter = node->child;
    if (iter) {
        while (iter->sibling)
            iter = iter->sibling;
        iter->sibling = newNode;
        newNode->prev_sibling = iter;
    } else {
        node->child = newNode;
    }

    return node->ops->appendChild(cx, node, newNode, JS_FALSE);
}

static JSBool
node_cloneNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *vp)
{
    JSBool deep;
    if (!JS_ConvertArguments(cx, argc, argv, "b", &deep))
        return JS_FALSE;

    DOM_SignalException(cx, DOM_NOT_SUPPORTED_ERR);
    /* clone */
    return JS_TRUE;
}

static JSBool
node_equals(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
    JSObject *node;
    DOM_Node *node1, *node2;
    if (!JS_ConvertArguments(cx, argc, argv, "o", &node))
        return JS_FALSE;

    node1 = (DOM_Node *)JS_GetPrivate(cx, obj);
    node2 = (DOM_Node *)JS_GetPrivate(cx, node);
    *vp = (JSBool)(node1 == node2);

    return JS_TRUE;
}

static JSFunctionSpec node_methods[] = {
    {"insertBefore",    node_insertBefore,      2},
    {"replaceChild",    node_replaceChild,      2},
    {"removeChild",     node_removeChild,       1},
    {"appendChild",     node_appendChild,       1},
    {"cloneNode",       node_cloneNode,         1},
    {"equals",          node_equals,            2},
    {0}
};

JSPropertySpec dom_node_props[] = {
    {"nodeName",        DOM_NODE_NODENAME},
    {"nodeValue",       DOM_NODE_NODEVALUE},
    {"nodeType",        DOM_NODE_NODETYPE},
    {"parentNode",      DOM_NODE_PARENTNODE},
    {"childNodes",      DOM_NODE_CHILDNODES},
    {"firstChild",      DOM_NODE_FIRSTCHILD},
    {"lastChild",       DOM_NODE_LASTCHILD},
    {"previousSibling", DOM_NODE_PREVIOUSSIBLING},
    {"nextSibling",     DOM_NODE_NEXTSIBLING},
    {"attributes",      DOM_NODE_ATTRIBUTES},
    {"hasChildNodes",   DOM_NODE_HASCHILDNODES},
    {0}
};

static JSConstDoubleSpec node_static_props[] = {
    {NODE_TYPE_ELEMENT,         "ELEMENT"},
    {NODE_TYPE_ATTRIBUTE,       "ATTRIBUTE"},
    {NODE_TYPE_PI,              "PROCESSING_INSTRUCTION"},
    {NODE_TYPE_CDATA,           "CDATA_SECTION"},
    {NODE_TYPE_TEXT,            "TEXT"},
    {NODE_TYPE_ENTITY_REF,      "ENTITY_REFERENCE"},
    {NODE_TYPE_ENTITY,          "ENTITY"},
    {NODE_TYPE_COMMENT,         "COMMENT"},
    {NODE_TYPE_DOCUMENT,        "DOCUMENT"},
    {NODE_TYPE_DOCTYPE,         "DOCUMENT_TYPE"},
    {NODE_TYPE_DOCFRAGMENT,     "DOCUMENT_FRAGMENT"},
    {NODE_TYPE_NOTATION,        "NOTATION"},
    {0}
};

static JSBool
Node(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
    return JS_TRUE;
}

JSObject *
dom_NodeInit(JSContext *cx, JSObject *scope)
{
    JSObject *proto, *ctor;
    proto = JS_InitClass(cx, scope, NULL, &DOM_NodeClass,
                         Node, 0,
                         dom_node_props, node_methods,
                         NULL, NULL);
    if (!proto || !(ctor = JS_GetConstructor(cx, proto)))
        return NULL;
    if (!JS_DefineConstDoubles(cx, ctor, node_static_props))
        return NULL;
    return proto;
}

/*
 * NodeList
 *
 * Basically an Array with magic methods.
 */

#if 0

static JSClass DOM_NodeListClass = {
    "NodeList", 0,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,  JS_FinalizeStub
};

static JSBool
nodelist_item(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
          jsval *rval)
{
    uint32 index;
    if (!JS_ConvertArguments(cx, argc, argv, "j", &index))
        return JS_TRUE;
    /* JS_GetElement */
    return JS_TRUE;
}

static JSFunctionSpec nodelist_methods[] = {
    {"item",    nodelist_item,  1},
    {0},
};

static JSBool
nodelist_size_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    return JS_TRUE;
}

static JSPropertySpec nodelist_props[] = {
    {"size",    -1,     0,      nodelist_size_get},
    {0}
};

/*
 * NamedNodeMap
 *
 * Basically an Object with magic resolve and helper methods.
 */

static JSBool
nnm_resolve(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    return JS_TRUE;
}

static JSClass DOM_NamedNodeMapClass = {
    "NamedNodeMap", 0,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,  JS_FinalizeStub
};

static JSBool
nnm_getNamedItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
         jsval *rval)
{
    /* JS_GetProperty */
    return JS_TRUE;
}

static JSBool
nnm_setNamedItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
         jsval *rval)
{
    return JS_TRUE;
    /* JS_SetProperty */
}

static JSBool
nnm_removeNamedItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
            jsval *rval)
{
    /* JS_DeleteProperty */
    return JS_TRUE;
}

static JSBool
nnm_item(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
     jsval *rval)
{
    /* return given slot -- OBJ_GET_SLOT? */
    return JS_TRUE;
}

static JSFunctionSpec nnm_methods[] = {
    {"getNamedItem",    nnm_getNamedItem,   1},
    {"setNamedItem",    nnm_setNamedItem,   1},
    {"removeNamedItem", nnm_removeNamedItem,1},
    {"item",            nnm_item,           1},
    {0}
};

static JSBool
nnm_get_size(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    /* return __count__ */
    return JS_FALSE;
}

enum {
    NNM_SIZE = -1
};

static JSPropertySpec nnm_props[] = {
    {"size",        NNM_SIZE,   0,  nnm_get_size},
    {0}
};

#endif /* 0 */
