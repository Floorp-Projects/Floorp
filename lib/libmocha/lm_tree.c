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
 * JS reflection of builtins in the current document.
 *
 * rjc@netscape.com, 9/21/98
 */

#include "lm.h"
#include "xp.h"
#include "layout.h"
#include "prlog.h"
#include "htrdf.h"

/* XXXbe move to layout.h */
extern uint
LO_EnumerateBuiltins(MWContext *context, int32 layer_id);

/* Global XXX #1: don't hold LO_BuiltinStruct pointers, use ids, to avoid
   dangling refs when mocha thread races with doc-discard */

/* Global XXX #2: don't hold the layout lock across JS_* callouts, or if not
   necesary to fondle LO_BuiltinStruct etc. (see above) */

extern PRLogModuleInfo* Moja;
#define warn    PR_LOG_WARN
#define debug   PR_LOG_DEBUG

enum builtins_array_slot {
    BUILTINS_ARRAY_LENGTH = -1
};

enum builtin_element_array_slot {
    BUILTIN_ELEMENT_ARRAY_LENGTH = -1
};

enum builtins_node_array_slot {
    BUILTIN_NODE_ARRAY_LENGTH = -1,
    BUILTINS_NODE_ARRAY_NAME = -2,
    BUILTINS_NODE_ARRAY_URL = -3,
    BUILTINS_NODE_ARRAY_SELECTED = -4,
    BUILTINS_NODE_ARRAY_CONTAINER = -5,
    BUILTINS_NODE_ARRAY_CONTAINER_OPEN = -6,
    BUILTINS_NODE_ARRAY_SEPARATOR = -7,
    BUILTINS_NODE_ARRAY_ENABLED = -8
};

enum builtins_slot {
    BUILTINS_NAME = -1,
    BUILTINS_DATA = -2,
    BUILTINS_TARGET = -3,
    BUILTINS_LENGTH = -4,
    BUILTINS_SELECTEDINDEX = -5,
    BUILTINS_ELEMENTS = -6,
    BUILTINS_NODES = -7
};

enum builtins_element_slot {
    BUILTINS_ELEMENT_NAME = -1,
    BUILTINS_ELEMENT_URL = -2,
    BUILTINS_ELEMENT_SELECTED = -3,
    BUILTINS_ELEMENT_CONTAINER = -4,
    BUILTINS_ELEMENT_CONTAINER_OPEN = -5,
    BUILTINS_ELEMENT_SEPARATOR = -6,
    BUILTINS_ELEMENT_ENABLED = -7
};

enum builtins_node_slot {
    BUILTINS_NODE_NAME = -1,
    BUILTINS_NODE_URL = -2,
    BUILTINS_NODE_SELECTED = -3,
    BUILTINS_NODE_CONTAINER = -4,
    BUILTINS_NODE_CONTAINER_OPEN = -5,
    BUILTINS_NODE_SEPARATOR = -6,
    BUILTINS_NODE_ENABLED = -7
};

static JSPropertySpec builtins_array_props[] = {
    {lm_length_str, BUILTINS_ARRAY_LENGTH,
                    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {0}
};

static JSPropertySpec builtin_element_array_props[] = {
    {lm_length_str, BUILTIN_ELEMENT_ARRAY_LENGTH,
                    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {0}
};

static JSPropertySpec builtin_node_array_props[] = {
    {lm_length_str, BUILTIN_NODE_ARRAY_LENGTH, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"name", BUILTINS_NODE_ARRAY_NAME, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"url", BUILTINS_NODE_ARRAY_URL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"selected", BUILTINS_NODE_ARRAY_SELECTED, JSPROP_ENUMERATE},
    {"container", BUILTINS_NODE_ARRAY_CONTAINER, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"open", BUILTINS_NODE_ARRAY_CONTAINER_OPEN, JSPROP_ENUMERATE},
    {"separator", BUILTINS_NODE_ARRAY_SEPARATOR, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"enabled", BUILTINS_NODE_ARRAY_ENABLED, JSPROP_ENUMERATE},
    {0}
};

static JSPropertySpec builtins_props[] = {
    {"name", BUILTINS_NAME, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"data", BUILTINS_DATA, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"target", BUILTINS_TARGET, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {lm_length_str, BUILTINS_LENGTH, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"selectedIndex", BUILTINS_SELECTEDINDEX, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"elements", BUILTINS_ELEMENTS, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"nodes", BUILTINS_NODES, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {0}
};

static JSPropertySpec builtin_element_props[] = {
    {"name", BUILTINS_ELEMENT_NAME, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"url", BUILTINS_ELEMENT_URL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"selected", BUILTINS_ELEMENT_SELECTED, JSPROP_ENUMERATE},
    {"container", BUILTINS_ELEMENT_CONTAINER, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"open", BUILTINS_ELEMENT_CONTAINER_OPEN, JSPROP_ENUMERATE},
    {"separator", BUILTINS_ELEMENT_SEPARATOR, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"enabled", BUILTINS_ELEMENT_ENABLED, JSPROP_ENUMERATE},
    {0}
};

static JSPropertySpec builtin_node_props[] = {
    {"name", BUILTINS_NODE_NAME, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"url", BUILTINS_NODE_URL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"selected", BUILTINS_NODE_SELECTED, JSPROP_ENUMERATE},
    {"container", BUILTINS_NODE_CONTAINER, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"open", BUILTINS_NODE_CONTAINER_OPEN, JSPROP_ENUMERATE},
    {"separator", BUILTINS_NODE_SEPARATOR, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {"enabled", BUILTINS_NODE_ENABLED, JSPROP_ENUMERATE},
    {0}
};

typedef struct JSBuiltin {
    MochaDecoder           *decoder;
    LO_BuiltinStruct       *builtin;
} JSBuiltin;
typedef struct JSElement {
    MochaDecoder           *decoder;
    LO_BuiltinStruct       *builtin;
    uint32                 slot;
} JSElement;
typedef struct JSNode {
    MochaDecoder           *decoder;
    LO_BuiltinStruct       *builtin;
    HT_Resource            node;
} JSNode;

extern JSClass lm_builtins_array_class;
extern JSClass lm_builtins_class;
extern JSClass lm_builtin_element_array_class;
extern JSClass lm_builtin_element_class;
extern JSClass lm_builtin_node_array_class;
extern JSClass lm_builtin_node_class;

JSObject *
lm_GetBuiltinsArray(MochaDecoder *decoder, JSObject *document)
{
    JSContext *cx = decoder->js_context;
    JSObject *obj;
    JSObjectArray *array;
    JSDocument *doc;

    doc = JS_GetPrivate(cx, document);
    if (!doc)
        return NULL;

    obj = doc->builtins;
    if (obj)
        return obj;

    array = JS_malloc(cx, sizeof *array);
    if (!array)
        return NULL;
    array->decoder = NULL;  /* in case of error below */

    obj = JS_NewObject(cx, &lm_builtins_array_class, NULL, document);
    if (!obj || !JS_SetPrivate(cx, obj, array)) {
        JS_free(cx, array);
        return NULL;
    }

    if (!JS_DefineProperties(cx, obj, builtins_array_props))
        return NULL;

    array->decoder = HOLD_BACK_COUNT(decoder);
    array->length = 0;
    array->layer_id = doc->layer_id;
    doc->builtins = obj;
    return obj;
}

JSObject *
LM_ReflectBuiltin(MWContext *context, LO_BuiltinStruct *lo_builtin,
                  PA_Tag * tag, int32 layer_id, uint index)
{
    JSObject *obj, *array_obj, *outer_obj, *document;
    JSBuiltin *builtin;
    MochaDecoder *decoder;
    JSContext *cx;
    char *name;
    uint32 i;

    obj = lo_builtin->mocha_object;
    if (obj)
        return obj;

    decoder = LM_GetMochaDecoder(context);
    if (!decoder)
        return NULL;

    cx = decoder->js_context;

    /* get the name */
    name = 0;
    for (i = 0; i < lo_builtin->attributes.n; i++) {
        if (!XP_STRCASECMP(lo_builtin->attributes.names[i], "name")) {
            name = strdup(lo_builtin->attributes.values[i]);
            break;
        }
    }

    /* Get the document object that will hold this builtin */
    document = lm_GetDocumentFromLayerId(decoder, layer_id);
    if (!document) {
        LM_PutMochaDecoder(decoder);
        return NULL;
    }

    array_obj = lm_GetBuiltinsArray(decoder, document);
    if (!array_obj) {
        LM_PutMochaDecoder(decoder);
        return NULL;
    }

    /* XXX */
    outer_obj = lm_GetOuterObject(decoder);

    obj = JS_NewObject(cx, &lm_builtins_class, NULL, document);
    if (!obj)
        goto out;

    builtin = JS_malloc(cx, sizeof *builtin);
    if (!builtin)
        goto out;
    builtin->decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    builtin->decoder = HOLD_BACK_COUNT(decoder);
    builtin->builtin = lo_builtin;
    if (!JS_SetPrivate(cx, obj, builtin))
    {
        obj = NULL;
        goto out;
    }

    /* put it in the builtin array */
    if (!lm_AddObjectToArray(cx, array_obj, name, index, obj)) {
        obj = NULL;
        goto out;
    }

    if (!JS_DefineProperties(cx, obj, builtins_props))
    {
        obj = NULL;
        goto out;
    }

    /* put it in the document scope */
    if (name && !JS_DefineProperty(cx, outer_obj, name, OBJECT_TO_JSVAL(obj),
                                   NULL, NULL,
                                   JSPROP_ENUMERATE | JSPROP_READONLY)) {
        PR_LOG(Moja, warn, ("failed to define builtin 0x%x as %s\n",
                            lo_builtin, name));
        /* XXX remove it altogether? */
    }

    /* cache it in layout data structure */
    lo_builtin->mocha_object = obj;

out:
    LM_PutMochaDecoder(decoder);
    return obj;
}

PR_STATIC_CALLBACK(JSBool)
builtins_array_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSObjectArray *array;
    MochaDecoder *decoder;
    MWContext *context;
    jsint count, slot;
    LO_BuiltinStruct *builtin_data;
    JSObject *newobj;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    array = JS_GetPrivate(cx, obj);
    if (!array)
        return JS_TRUE;
    decoder = array->decoder;
    context = decoder->window_context;

    if (!context)
        return JS_TRUE;

    LO_LockLayout();
    switch (slot) {
      case BUILTINS_ARRAY_LENGTH:
        count = LO_EnumerateBuiltins(context, array->layer_id);
        if (count > array->length)
            array->length = count;
        *vp = INT_TO_JSVAL(array->length);
        break;

      default:
        if (slot < 0) {
            /* Don't mess with user-defined or method properties. */
            LO_UnlockLayout();
            return JS_TRUE;
        }
        builtin_data = LO_GetBuiltinByIndex(context, array->layer_id, (uint)slot);
        if (!builtin_data) {
            JS_ReportError(cx, "no builtin with index %d\n");
            goto err;
        }
        newobj = LM_ReflectBuiltin(context, builtin_data, NULL,
                                   array->layer_id, (uint)slot);
        if (!newobj) {
            JS_ReportError(cx,
             "unable to reflect builtin with index %d - not loaded yet?",
             (uint) slot);
            goto err;
        }
        *vp = OBJECT_TO_JSVAL(newobj);
        XP_ASSERT(slot < array->length);
        break;
    }
    LO_UnlockLayout();
    return JS_TRUE;
  err:
    LO_UnlockLayout();
    return JS_FALSE;
}

PR_STATIC_CALLBACK(JSBool)
builtin_element_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSElement *element;
    MochaDecoder *decoder;
    MWContext *context;
    jsint slot;
    HT_Pane pane;
    HT_View view;
    HT_Resource node;

    element = JS_GetPrivate(cx, obj);
    if (!element)
        return JS_TRUE;
    decoder = element->decoder;
    context = decoder->window_context;
    if (!context)
        return JS_TRUE;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;
    slot = JSVAL_TO_INT(id);

    switch (slot) {
      case BUILTINS_ELEMENT_SELECTED:
        if (!JSVAL_IS_BOOLEAN(*vp))
            return JS_TRUE;

        if ((pane = element->builtin->htPane) != NULL)
        {
            if ((view = HT_GetNthView(pane, 0)) != NULL)
            {
                if ((node = HT_GetNthItem(view, element->slot)) != NULL)
                {
                    HT_SetSelectedState(node, JSVAL_TO_BOOLEAN(*vp));
                }
            }
        }
        break;

      case BUILTINS_ELEMENT_CONTAINER_OPEN:
        if (!JSVAL_IS_BOOLEAN(*vp))
            return JS_TRUE;

        if ((pane = element->builtin->htPane) != NULL)
        {
            if ((view = HT_GetNthView(pane, 0)) != NULL)
            {
                if ((node = HT_GetNthItem(view, element->slot)) != NULL)
                {
                    HT_SetOpenState(node, JSVAL_TO_BOOLEAN(*vp));
                }
            }
        }
        break;

      case BUILTINS_ELEMENT_ENABLED:
        if (!JSVAL_IS_BOOLEAN(*vp))
            return JS_TRUE;

        if ((pane = element->builtin->htPane) != NULL)
        {
            if ((view = HT_GetNthView(pane, 0)) != NULL)
            {
                if ((node = HT_GetNthItem(view, element->slot)) != NULL)
                {
                    HT_SetEnabledState(node, JSVAL_TO_BOOLEAN(*vp));
                }
            }
        }
        break;

      default:;
    }
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
builtin_element_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSElement *element;
    MochaDecoder *decoder;
    MWContext *context;
    JSString *jstr;
    jsint slot;
    HT_Pane pane;
    HT_View view;
    HT_Resource node;
    char *name;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    element = JS_GetPrivate(cx, obj);
    if (!element)
        return JS_TRUE;
    decoder = element->decoder;
    context = decoder->window_context;

    if (!context)
        return JS_TRUE;

    LO_LockLayout();
    switch (slot) {
      case BUILTINS_ELEMENT_NAME:
        if ((pane = element->builtin->htPane) != NULL)
        {
            if ((view = HT_GetNthView(pane, 0)) != NULL)
            {
                if ((node = HT_GetNthItem(view, element->slot)) != NULL)
                {
                    if ((name = HT_GetNodeName(node)) != NULL)
                    {
                        if ((jstr = lm_LocalEncodingToStr(context, name)) != NULL)
                        {
                            *vp = STRING_TO_JSVAL(jstr);
                        }
                    }
                }
            }
        }
        break;

      case BUILTINS_ELEMENT_URL:
        if ((pane = element->builtin->htPane) != NULL)
        {
            if ((view = HT_GetNthView(pane, 0)) != NULL)
            {
                if ((node = HT_GetNthItem(view, element->slot)) != NULL)
                {
                    if ((name = HT_GetNodeURL(node)) != NULL)
                    {
                        if ((jstr = lm_LocalEncodingToStr(context, name)) != NULL)
                        {
                            *vp = STRING_TO_JSVAL(jstr);
                        }
                    }
                }
            }
        }
        break;

      case BUILTINS_ELEMENT_SELECTED:
        if ((pane = element->builtin->htPane) != NULL)
        {
            if ((view = HT_GetNthView(pane, 0)) != NULL)
            {
                if ((node = HT_GetNthItem(view, element->slot)) != NULL)
                {
                    *vp = BOOLEAN_TO_JSVAL(HT_IsSelected(node));
                }
            }
        }
        break;

      case BUILTINS_ELEMENT_CONTAINER:
        if ((pane = element->builtin->htPane) != NULL)
        {
            if ((view = HT_GetNthView(pane, 0)) != NULL)
            {
                if ((node = HT_GetNthItem(view, element->slot)) != NULL)
                {
                    *vp = BOOLEAN_TO_JSVAL(HT_IsContainer(node));
                }
            }
        }
        break;

      case BUILTINS_ELEMENT_CONTAINER_OPEN:
        if ((pane = element->builtin->htPane) != NULL)
        {
            if ((view = HT_GetNthView(pane, 0)) != NULL)
            {
                if ((node = HT_GetNthItem(view, element->slot)) != NULL)
                {
                        *vp = BOOLEAN_TO_JSVAL(HT_IsContainerOpen(node));
                }
            }
        }
        break;

      case BUILTINS_ELEMENT_SEPARATOR:
        if ((pane = element->builtin->htPane) != NULL)
        {
            if ((view = HT_GetNthView(pane, 0)) != NULL)
            {
                if ((node = HT_GetNthItem(view, element->slot)) != NULL)
                {
                    *vp = BOOLEAN_TO_JSVAL(HT_IsSeparator(node));
                }
            }
        }
        break;

      case BUILTINS_ELEMENT_ENABLED:
        if ((pane = element->builtin->htPane) != NULL)
        {
            if ((view = HT_GetNthView(pane, 0)) != NULL)
            {
                if ((node = HT_GetNthItem(view, element->slot)) != NULL)
                {
                    *vp = BOOLEAN_TO_JSVAL(HT_IsEnabled(node));
                }
            }
        }
        break;

      default:;
    }
    LO_UnlockLayout();
    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
builtin_element_finalize(JSContext *cx, JSObject *obj)
{
    JSObjectArray *array;

    array = JS_GetPrivate(cx, obj);
    if (!array)
        return;
    DROP_BACK_COUNT(array->decoder);
    JS_free(cx, array);
}

JSClass lm_builtin_element_class = {
    "TreeElement", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    builtin_element_getProperty, builtin_element_setProperty, JS_EnumerateStub,
    JS_ResolveStub, JS_ConvertStub, builtin_element_finalize
};

PR_STATIC_CALLBACK(JSBool)
builtin_element_array_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSBuiltin *builtin;
    JSElement *element;
    JSObject *element_obj;
    MochaDecoder *decoder;
    MWContext *context;
    jsint slot;
    int theIndex;
    HT_Pane pane;
    HT_View view;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    builtin = JS_GetPrivate(cx, obj);
    if (!builtin)
        return JS_TRUE;
    decoder = builtin->decoder;
    context = decoder->window_context;

    if (!context)
        return JS_TRUE;

    LO_LockLayout();
    switch (slot) {
      case BUILTIN_ELEMENT_ARRAY_LENGTH:
        theIndex = 0;
        if ((pane = builtin->builtin->htPane) != NULL)
        {
            if ((view = HT_GetNthView(pane, 0)) != NULL)
            {
                theIndex = HT_GetItemListCount(view);
            }
        }
        *vp = INT_TO_JSVAL(theIndex);
        break;

      default:
        if (slot < 0) {
            /* Don't mess with user-defined or method properties. */
            LO_UnlockLayout();
            return JS_TRUE;
        }
        element = JS_malloc(cx, sizeof *element);
        if (!element)
            goto err;
        element_obj = JS_NewObject(cx, &lm_builtin_element_class,
                                   decoder->builtin_element_prototype, obj);
        if (!element_obj || !JS_SetPrivate(cx, element_obj, element))
        {
            JS_free(cx, element);
            goto err;
        }
        if (!JS_DefineProperties(cx, obj, builtin_element_props))
        {
            JS_free(cx, element);
            goto err;
        }
        element->decoder = HOLD_BACK_COUNT(decoder);
        element->builtin = builtin->builtin;
        element->slot = slot;
        *vp = OBJECT_TO_JSVAL(element_obj);

        break;
    }
    LO_UnlockLayout();
    return JS_TRUE;
  err:
    LO_UnlockLayout();
    return JS_FALSE;
}

PR_STATIC_CALLBACK(JSBool)
builtin_node_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSNode *jsNode;
    MochaDecoder *decoder;
    MWContext *context;
    jsint slot;
    HT_Resource node;

    jsNode = JS_GetPrivate(cx, obj);
    if (!jsNode)
        return JS_TRUE;
    decoder = jsNode->decoder;
    context = decoder->window_context;
    if (!context) return JS_TRUE;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;
    slot = JSVAL_TO_INT(id);

    switch (slot) {
      case BUILTINS_NODE_SELECTED:
        if (!JSVAL_IS_BOOLEAN(*vp))
            return JS_TRUE;

        if ((node = jsNode->node) != NULL)
        {
            HT_SetSelectedState(node, JSVAL_TO_BOOLEAN(*vp));
        }
        break;

      case BUILTINS_NODE_CONTAINER_OPEN:
        if (!JSVAL_IS_BOOLEAN(*vp))
            return JS_TRUE;

        if ((node = jsNode->node) != NULL)
        {
            HT_SetOpenState(node, JSVAL_TO_BOOLEAN(*vp));
        }
        break;

      case BUILTINS_NODE_ENABLED:
        if (!JSVAL_IS_BOOLEAN(*vp))
            return JS_TRUE;

        if ((node = jsNode->node) != NULL)
        {
            HT_SetEnabledState(node, JSVAL_TO_BOOLEAN(*vp));
        }
        break;

      default:;
    }
    return(JS_TRUE);
}

PR_STATIC_CALLBACK(JSBool)
builtin_node_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSNode *jsNode;
    MochaDecoder *decoder;
    MWContext *context;
    JSString *jstr;
    jsint slot;
    HT_Resource node;
    char *name;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    jsNode = JS_GetPrivate(cx, obj);
    if (!jsNode)
        return JS_TRUE;
    decoder = jsNode->decoder;
    context = decoder->window_context;

    if (!context)
        return JS_TRUE;

    LO_LockLayout();
    switch (slot) {
      case BUILTINS_NODE_NAME:
        if ((node = jsNode->node) != NULL)
        {
            if ((name = HT_GetNodeName(node)) != NULL)
            {
                if ((jstr = lm_LocalEncodingToStr(context, name)) != NULL)
                {
                    *vp = STRING_TO_JSVAL(jstr);
                }
            }
        }
        break;

      case BUILTINS_NODE_URL:
        if ((node = jsNode->node) != NULL)
        {
            if ((name = HT_GetNodeURL(node)) != NULL)
            {
                if ((jstr = lm_LocalEncodingToStr(context, name)) != NULL)
                {
                    *vp = STRING_TO_JSVAL(jstr);
                }
            }
        }
        break;

      case BUILTINS_NODE_SELECTED:
        if ((node = jsNode->node) != NULL)
        {
            *vp = BOOLEAN_TO_JSVAL(HT_IsSelected(node));
        }
        break;

      case BUILTINS_NODE_CONTAINER:
        if ((node = jsNode->node) != NULL)
        {
            *vp = BOOLEAN_TO_JSVAL(HT_IsContainer(node));
        }
        break;

      case BUILTINS_NODE_CONTAINER_OPEN:
        if ((node = jsNode->node) != NULL)
        {
            *vp = BOOLEAN_TO_JSVAL(HT_IsContainerOpen(node));
        }
        break;

      case BUILTINS_NODE_SEPARATOR:
        if ((node = jsNode->node) != NULL)
        {
            *vp = BOOLEAN_TO_JSVAL(HT_IsSeparator(node));
        }
        break;

      case BUILTINS_NODE_ENABLED:
        if ((node = jsNode->node) != NULL)
        {
            *vp = BOOLEAN_TO_JSVAL(HT_IsEnabled(node));
        }
        break;

      default:;
    }
    LO_UnlockLayout();
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
builtin_node_children(JSContext *cx, JSObject *obj, uint argc, jsval *argv,
                      jsval *rval)
{
    JSNode      *jsNode, *newJSNode;
    JSObject    *jsnode_obj;
    MochaDecoder *decoder;
    HT_Resource  node;

    if (!JS_InstanceOf(cx, obj, &lm_builtin_node_class, argv))
        return JS_FALSE;
    jsNode = JS_GetPrivate(cx, obj);
    if (!jsNode)
        return JS_TRUE;
    decoder = jsNode->decoder;

    if ((node = jsNode->node) == NULL)
        return JS_TRUE; /* XXXbe no silent error */

    if (!HT_IsContainer(node))
        *rval = JSVAL_NULL;
    else if (!HT_IsContainerOpen(node))
        *rval = JSVAL_NULL;
    else
    {
        newJSNode = JS_malloc(cx, sizeof *newJSNode);
        if (!newJSNode)
            return JS_FALSE;
        jsnode_obj = JS_NewObject(cx, &lm_builtin_node_array_class,
                                  decoder->builtin_node_prototype, obj);
        if (!jsnode_obj || !JS_SetPrivate(cx, jsnode_obj, newJSNode))
        {
            JS_free(cx, newJSNode);
            return JS_FALSE;
        }
        if (!JS_DefineProperties(cx, jsnode_obj, builtin_node_array_props))
        {
            JS_free(cx, newJSNode);
            return JS_FALSE;
        }
        newJSNode->decoder = HOLD_BACK_COUNT(decoder);
        newJSNode->builtin = jsNode->builtin;
        newJSNode->node = node;
        *rval = OBJECT_TO_JSVAL(jsnode_obj);
    }
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
builtin_node_parent(JSContext *cx, JSObject *obj, uint argc, jsval *argv,
                    jsval *rval)
{
    JSNode      *jsNode, *newJSNode;
    JSObject    *jsnode_obj;
    MochaDecoder *decoder;
    HT_Resource  node;

    if (!JS_InstanceOf(cx, obj, &lm_builtin_node_class, argv))
        return JS_FALSE;
    jsNode = JS_GetPrivate(cx, obj);
    if (!jsNode)
        return JS_TRUE;
    decoder = jsNode->decoder;

    if ((node = jsNode->node) == NULL)
        return JS_TRUE; /* XXXbe no silent error */

    newJSNode = JS_malloc(cx, sizeof *newJSNode);
    if (!newJSNode)
        return(JS_FALSE);
    jsnode_obj = JS_NewObject(cx, &lm_builtin_node_array_class,
                              decoder->builtin_node_prototype, obj);
    if (!jsnode_obj || !JS_SetPrivate(cx, jsnode_obj, newJSNode))
    {
        JS_free(cx, newJSNode);
        return JS_FALSE;
    }
    if (!JS_DefineProperties(cx, jsnode_obj, builtin_node_array_props))
    {
        JS_free(cx, newJSNode);
        return JS_FALSE;
    }
    newJSNode->decoder = HOLD_BACK_COUNT(decoder);
    newJSNode->builtin = jsNode->builtin;
    newJSNode->node = HT_GetParent(node);
    *rval = OBJECT_TO_JSVAL(jsnode_obj);
    return(JS_TRUE);
}

PR_STATIC_CALLBACK(JSBool)
builtin_element_parentIndex(JSContext *cx, JSObject *obj,
                            uint argc, jsval *argv, jsval *rval)
{
    JSElement    *element;
    MochaDecoder *decoder;
    HT_Pane        pane;
    HT_View        view;
    HT_Resource        node, parent;
    int32        parentIndex = -1;

    if (!JS_InstanceOf(cx, obj, &lm_builtin_element_class, argv))
        return JS_FALSE;
    if (!(element = JS_GetPrivate(cx, obj)))
        return JS_TRUE;
    decoder = element->decoder;

    if ((pane = element->builtin->htPane) != NULL)
    {
        if ((view = HT_GetNthView(pane, 0)) != NULL)
        {
            if ((node = HT_GetNthItem(view, element->slot)) != NULL)
            {
                if ((parent = HT_GetParent(node)) != NULL)
                {
                    if (parent != HT_TopNode(view))
                    {
                        parentIndex = HT_GetNodeIndex(view, parent);
                    }
                }
            }
        }
    }
    *rval = INT_TO_JSVAL(parentIndex);
    return JS_TRUE;
}

static JSFunctionSpec builtin_element_methods[] = {
    {"parentIndex", builtin_element_parentIndex, 0},
    {0}
};

static JSFunctionSpec builtin_node_methods[] = {
    {"children", builtin_node_children, 0},
    {"parent", builtin_node_parent, 0},
    {0}
};

PR_STATIC_CALLBACK(void)
builtin_node_finalize(JSContext *cx, JSObject *obj)
{
    JSObjectArray *array;

    array = JS_GetPrivate(cx, obj);
    if (!array)
        return;
    DROP_BACK_COUNT(array->decoder);
    JS_free(cx, array);
}

JSClass lm_builtin_node_class = {
    "TreeNode", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    builtin_node_getProperty, builtin_node_setProperty, JS_EnumerateStub,
    JS_ResolveStub, JS_ConvertStub, builtin_node_finalize
};

PR_STATIC_CALLBACK(JSBool)
builtin_node_array_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSNode *jsNode, *newJSNode;
    JSObject *jsnode_obj;
    MochaDecoder *decoder;
    MWContext *context;
    jsint slot;
    JSString *jstr;
    char *name;
    uint32 theCount;
    HT_Resource node;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    jsNode = JS_GetPrivate(cx, obj);
    if (!jsNode)
        return JS_TRUE;
    decoder = jsNode->decoder;
    context = decoder->window_context;

    if (!context)
        return JS_TRUE;

    LO_LockLayout();
    switch (slot) {
      case BUILTIN_NODE_ARRAY_LENGTH:
        if ((node = jsNode->node) != NULL)
        {
            theCount = HT_GetCountDirectChildren(jsNode->node);
            *vp = INT_TO_JSVAL(theCount);
        }
        break;

      case BUILTINS_NODE_ARRAY_NAME:
        if ((node = jsNode->node) != NULL)
        {
            if ((name = HT_GetNodeName(node)) != NULL)
            {
                if ((jstr = lm_LocalEncodingToStr(context, name)) != NULL)
                {
                    *vp = STRING_TO_JSVAL(jstr);
                }
            }
        }
        break;

      case BUILTINS_NODE_ARRAY_URL:
        if ((node = jsNode->node) != NULL)
        {
            if ((name = HT_GetNodeURL(node)) != NULL)
            {
                if ((jstr = lm_LocalEncodingToStr(context, name)) != NULL)
                {
                    *vp = STRING_TO_JSVAL(jstr);
                }
            }
        }
        break;

      case BUILTINS_NODE_ARRAY_SELECTED:
        if ((node = jsNode->node) != NULL)
        {
            *vp = BOOLEAN_TO_JSVAL(HT_IsSelected(node));
        }
        break;

      case BUILTINS_NODE_ARRAY_CONTAINER:
        if ((node = jsNode->node) != NULL)
        {
            *vp = BOOLEAN_TO_JSVAL(HT_IsContainer(node));
        }
        break;

      case BUILTINS_NODE_ARRAY_CONTAINER_OPEN:
        if ((node = jsNode->node) != NULL)
        {
            *vp = BOOLEAN_TO_JSVAL(HT_IsContainerOpen(node));
        }
        break;

      case BUILTINS_NODE_ARRAY_SEPARATOR:
        if ((node = jsNode->node) != NULL)
        {
            *vp = BOOLEAN_TO_JSVAL(HT_IsSeparator(node));
        }
        break;

      case BUILTINS_NODE_ARRAY_ENABLED:
        if ((node = jsNode->node) != NULL)
        {
            *vp = BOOLEAN_TO_JSVAL(HT_IsEnabled(node));
        }
        break;

      default:
        if (slot < 0) {
            /* Don't mess with user-defined or method properties. */
            LO_UnlockLayout();
            return JS_TRUE;
        }
        newJSNode = JS_malloc(cx, sizeof *newJSNode);
        if (!newJSNode)
            goto err;
        jsnode_obj = JS_NewObject(cx, &lm_builtin_node_class,
                                  decoder->builtin_node_prototype, obj);
        if (!jsnode_obj || !JS_SetPrivate(cx, jsnode_obj, newJSNode))
        {
            JS_free(cx, newJSNode);
            goto err;
        }
        if (!JS_DefineProperties(cx, obj, builtin_node_props))
        {
            JS_free(cx, newJSNode);
            goto err;
        }
        newJSNode->decoder = HOLD_BACK_COUNT(decoder);
        newJSNode->builtin = jsNode->builtin;
        newJSNode->node = HT_GetContainerItem(jsNode->node, slot);
        *vp = OBJECT_TO_JSVAL(jsnode_obj);

        break;
    }
    LO_UnlockLayout();
    return JS_TRUE;
  err:
    LO_UnlockLayout();
    return JS_FALSE;
}

PR_STATIC_CALLBACK(void)
builtin_element_array_finalize(JSContext *cx, JSObject *obj)
{
    JSObjectArray *array;

    array = JS_GetPrivate(cx, obj);
    if (!array)
        return;
    DROP_BACK_COUNT(array->decoder);
    JS_free(cx, array);
}

PR_STATIC_CALLBACK(void)
builtin_node_array_finalize(JSContext *cx, JSObject *obj)
{
    JSObjectArray *array;

    array = JS_GetPrivate(cx, obj);
    if (!array)
        return;
    DROP_BACK_COUNT(array->decoder);
    JS_free(cx, array);
}

JSClass lm_builtin_element_array_class = {
    "TreeElementArray", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    builtin_element_array_getProperty, builtin_element_array_getProperty, JS_EnumerateStub,
    JS_ResolveStub, JS_ConvertStub, builtin_element_array_finalize
};

JSClass lm_builtin_node_array_class = {
    "TreeNodeArray", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    builtin_node_array_getProperty, builtin_node_array_getProperty, JS_EnumerateStub,
    JS_ResolveStub, JS_ConvertStub, builtin_node_array_finalize
};

PR_STATIC_CALLBACK(JSBool)
builtin_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSBuiltin *builtin, *element;
    JSNode *jsNode;
    JSObject *element_obj, *jsNode_obj;
    MochaDecoder *decoder;
    MWContext *context;
    jsint slot;
    JSString *jstr;
    uint32 i;
    int theIndex;
    HT_Pane pane;
    HT_View view;
    HT_Resource node = NULL;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    builtin = JS_GetPrivate(cx, obj);
    if (!builtin)
        return JS_TRUE;
    decoder = builtin->decoder;
    context = decoder->window_context;

    if (!context)
        return JS_TRUE;

    LO_LockLayout();
    switch (slot) {
      case BUILTINS_NAME:
        for (i = 0; i < builtin->builtin->attributes.n; i++)
        {
            if (!XP_STRCASECMP(builtin->builtin->attributes.names[i], "name"))
            {
                if ((jstr = lm_LocalEncodingToStr(context,
                        builtin->builtin->attributes.values[i])) != NULL)
                {
                    *vp = STRING_TO_JSVAL(jstr);
                }
                break;
            }
        }
        break;

      case BUILTINS_DATA:
        for (i = 0; i < builtin->builtin->attributes.n; i++)
        {
            if (!XP_STRCASECMP(builtin->builtin->attributes.names[i], "data"))
            {
                if ((jstr = JS_NewStringCopyZ(cx,
                        builtin->builtin->attributes.values[i])) != NULL)
                {
                    *vp = STRING_TO_JSVAL(jstr);
                }
                break;
            }
        }
        break;

      case BUILTINS_TARGET:
        for (i = 0; i < builtin->builtin->attributes.n; i++)
        {
            if (!XP_STRCASECMP(builtin->builtin->attributes.names[i], "target"))
            {
                if ((jstr = JS_NewStringCopyZ(cx,
                        builtin->builtin->attributes.values[i])) != NULL)
                {
                    *vp = STRING_TO_JSVAL(jstr);
                }
                break;
            }
        }
        break;

      case BUILTINS_LENGTH:
        theIndex = 0;
        if ((pane = builtin->builtin->htPane) != NULL)
        {
            if ((view = HT_GetNthView(pane, 0)) != NULL)
            {
                theIndex = HT_GetItemListCount(view);
            }
        }
        *vp = INT_TO_JSVAL(theIndex);
        break;

      case BUILTINS_SELECTEDINDEX:
        theIndex = -1;
        if ((pane = builtin->builtin->htPane) != NULL)
        {
            if ((view = HT_GetNthView(pane, 0)) != NULL)
            {
                node = NULL;
                if ((node = HT_GetNextSelection(view, node)) != NULL)
                {
                    theIndex = HT_GetNodeIndex(view, node);
                }
            }
        }
        *vp = INT_TO_JSVAL(theIndex);
        break;

      case BUILTINS_ELEMENTS:
        element = JS_malloc(cx, sizeof *element);
        if (!element)
            goto err;
        element_obj = JS_NewObject(cx, &lm_builtin_element_array_class, decoder->builtin_prototype, obj);
        if (!element_obj || !JS_SetPrivate(cx, element_obj, element))
        {
            JS_free(cx, element);
            goto err;
        }
        if (!JS_DefineProperties(cx, element_obj, builtin_element_array_props))
        {
            JS_free(cx, element);
            goto err;
        }
        element->decoder = HOLD_BACK_COUNT(decoder);
        element->builtin = builtin->builtin;
        *vp = OBJECT_TO_JSVAL(element_obj);
        break;

      case BUILTINS_NODES:
        jsNode = JS_malloc(cx, sizeof *jsNode);
        if (!jsNode)
            goto err;
        jsNode_obj = JS_NewObject(cx, &lm_builtin_node_array_class, decoder->builtin_node_prototype, obj);
        if (!jsNode_obj || !JS_SetPrivate(cx, jsNode_obj, jsNode))
        {
            JS_free(cx, jsNode);
            goto err;
        }
        if (!JS_DefineProperties(cx, jsNode_obj, builtin_node_array_props))
        {
            JS_free(cx, jsNode);
            goto err;
        }
        jsNode->decoder = HOLD_BACK_COUNT(decoder);
        jsNode->builtin = builtin->builtin;
        jsNode->node = HT_TopNode(HT_GetNthView(builtin->builtin->htPane, 0));
        *vp = OBJECT_TO_JSVAL(jsNode_obj);
        break;

      default:;
    }
    LO_UnlockLayout();
    return JS_TRUE;
  err:
    LO_UnlockLayout();
    return JS_FALSE;
}

PR_STATIC_CALLBACK(void)
builtins_array_finalize(JSContext *cx, JSObject *obj)
{
    JSObjectArray *array;

    array = JS_GetPrivate(cx, obj);
    if (!array)
        return;
    DROP_BACK_COUNT(array->decoder);
    JS_free(cx, array);
}

PR_STATIC_CALLBACK(void)
builtin_finalize(JSContext *cx, JSObject *obj)
{
    JSObjectArray *array;

    array = JS_GetPrivate(cx, obj);
    if (!array)
        return;
    DROP_BACK_COUNT(array->decoder);
    JS_free(cx, array);
}

JSClass lm_builtins_array_class = {
    "TreesArray", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    builtins_array_getProperty, builtins_array_getProperty, JS_EnumerateStub,
    JS_ResolveStub, JS_ConvertStub, builtins_array_finalize
};

JSClass lm_builtins_class = {
    "Trees", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    builtin_getProperty, builtin_getProperty, JS_EnumerateStub,
    JS_ResolveStub, JS_ConvertStub, builtin_finalize
};

PR_STATIC_CALLBACK(JSBool)
Builtin(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
BuiltinElement(JSContext *cx, JSObject *obj, uint argc, jsval *argv,
               jsval *rval)
{
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
BuiltinNode(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

JSBool
lm_InitBuiltinClass(MochaDecoder *decoder)
{
    JSContext *cx;
    JSObject *prototype;

    cx = decoder->js_context;
    prototype = JS_InitClass(cx, decoder->window_object,
                             decoder->event_receiver_prototype, &lm_builtins_class,
                             Builtin, 0, builtins_props, NULL, NULL, NULL);
    if (!prototype)
        return JS_FALSE;
    decoder->builtin_prototype = prototype;

    prototype = JS_InitClass(cx, decoder->window_object,
                             decoder->event_receiver_prototype, &lm_builtin_element_class,
                             BuiltinElement, 0, builtin_element_props, builtin_element_methods, NULL, NULL);
    if (!prototype)
        return JS_FALSE;
    decoder->builtin_element_prototype = prototype;

    prototype = JS_InitClass(cx, decoder->window_object,
                             decoder->event_receiver_prototype, &lm_builtin_node_class,
                             BuiltinNode, 0, builtin_node_props, builtin_node_methods, NULL, NULL);
    if (!prototype)
        return JS_FALSE;
    decoder->builtin_node_prototype = prototype;
    return JS_TRUE;
}
