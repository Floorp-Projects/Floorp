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
 * DOM Document, DocumentFragment implementation.
 */

#include "dom_priv.h"

typedef struct DOM_DocumentFragmentStruct {
    DOM_Node node;
    void *data;
} DOM_DocumentFragmentStruct;

#if 0
static JSBool
docfrag_masterDoc_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    return JS_TRUE;
}

static JSPropertySpec docfrag_props[] = {
    {"masterDoc",   -1, 0,  docfrag_masterDoc_get},
    {0}
};
#endif

/*
 * Document
 *
 * XXX mush this stuff into existing "document" in lm_doc.c?
 */

typedef struct DOM_DocumentStruct {
    DOM_DocumentFragmentStruct fragment;
} DOM_DocumentStruct;

static void
document_finalize(JSContext *cx, JSObject *obj)
{
    /* chain destructors */
    /* node_finalize(cx, obj); */
}

static JSBool
document_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    return JS_TRUE;
}

static JSBool
document_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    return JS_TRUE;
}

JSClass DOM_DocumentClass = {
    "Document", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub, document_getter, document_setter,
    JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,  document_finalize
};

static JSBool
doc_createEntity(JSContext *cx, JSObject* obj, uintN argc, jsval *argv,
         jsval *rval)
{
    /* new Entity */
    return JS_TRUE;
}

static JSBool
doc_createEntityReference(JSContext *cx, JSObject* obj, uintN argc,
              jsval *argv, jsval *rval)
{
    /* new EntityReference */
    return JS_TRUE;
}

static JSBool
doc_createDocumentFragment(JSContext *cx, JSObject* obj, uintN argc,
               jsval *argv, jsval *rval)
{
    /* new DocumentFragment */
    return JS_TRUE;
}

static JSBool
doc_createElement(JSContext *cx, JSObject* obj, uintN argc, jsval *argv,
          jsval *rval)
{
    JSString *tagName;
    if (!JS_ConvertArguments(cx, argc, argv, "S", &tagName))
        return JS_FALSE;
    /* new Element */
    return JS_TRUE;
}

static JSBool
doc_createTextNode(JSContext *cx, JSObject* obj, uintN argc, jsval *argv,
           jsval *rval)
{
    JSString *text;
    if (!JS_ConvertArguments(cx, argc, argv, "S", &text))
        return JS_FALSE;
    /* new TextNode */
    return JS_TRUE;
}

static JSBool
doc_createComment(JSContext *cx, JSObject* obj, uintN argc, jsval *argv,
          jsval *rval)
{
    JSString *comment;
    if (!JS_ConvertArguments(cx, argc, argv, "S", &comment))
        return JS_FALSE;
    /* new Comment */
    return JS_TRUE;
}

static JSBool
doc_createCDATASection(JSContext *cx, JSObject* obj, uintN argc, jsval *argv,
               jsval *rval)
{
    JSString *cdata;
    if (!JS_ConvertArguments(cx, argc, argv, "S", &cdata))
        return JS_FALSE;
    /* new CDATA */
    return JS_TRUE;
}

static JSBool
doc_createProcessingInstruction(JSContext *cx, JSObject* obj, uintN argc,
                jsval *argv, jsval *rval)
{
    JSString *target, *data;
    if (!JS_ConvertArguments(cx, argc, argv, "SS", &target, &data))
        return JS_FALSE;
    /* new PI */
    return JS_TRUE;
}

static JSBool
doc_createAttribute(JSContext *cx, JSObject* obj, uintN argc, jsval *argv,
            jsval *rval)
{
    JSString *name;
    if (!JS_ConvertArguments(cx, argc, argv, "S", &name))
        return JS_FALSE;
    /* new attr */
    return JS_TRUE;
}

static JSBool
doc_getElementsByTagName(JSContext *cx, JSObject* obj, uintN argc,
             jsval *argv, jsval *rval)
{
    JSString *tagName;
    if (!JS_ConvertArguments(cx, argc, argv, "S", &tagName))
        return JS_FALSE;

    return JS_TRUE;
}

static JSFunctionSpec document_methods[] = {
    {"createEntity",        doc_createEntity,       0},
    {"createEntityReference",   doc_createEntityReference,  0},
    {"createDocumentFragment",  doc_createDocumentFragment, 0},
    {"createElement",       doc_createElement,      1},
    {"createTextNode",      doc_createTextNode,     1},
    {"createComment",       doc_createComment,      1},
    {"createCDATASection",  doc_createCDATASection,     1},
    {"createProcessingInstruction", doc_createProcessingInstruction, 2},
    {"createAttribute",     doc_createAttribute,        1},
    {"getElementsByTagName",    doc_getElementsByTagName,   1},
    {0}
};

enum {
    DOCUMENT_DOCTYPE = -1,
    DOCUMENT_IMPLEMENTATION = -2,
    DOCUMENT_DOCUMENTELEMENT = -3
};

static JSPropertySpec document_props[] = {
    {"doctype",     DOCUMENT_DOCTYPE},
    {"implementation",  DOCUMENT_IMPLEMENTATION},
    {"documentElement", DOCUMENT_DOCUMENTELEMENT},
    {0}
};

static JSBool
Document(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
    return JS_TRUE;
}

JSObject *
DOM_DocumentInit(JSContext *cx, JSObject *scope, JSObject *docfrag_prototype)
{
    JSObject *documentProto;
    documentProto = JS_InitClass(cx, scope, docfrag_prototype,
                                 &DOM_DocumentClass, Document, 0,
                                 document_props, document_methods,
                                 NULL, NULL);
    if (!documentProto)
        return NULL;
    return documentProto;
}
