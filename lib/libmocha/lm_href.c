/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * JS reflection of the HREF and NAME Anchors in the current document.
 *
 * Brendan Eich, 9/8/95
 */
#include "lm.h"
#include "pa_tags.h"
#include "layout.h"	/* XXX for lo_NameList only */

enum anchor_array_slot {
    ANCHOR_ARRAY_LENGTH = -1
};

static JSPropertySpec anchor_array_props[] = {
    {lm_length_str, ANCHOR_ARRAY_LENGTH,
		    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {0}
};

extern JSClass lm_link_array_class;

PR_STATIC_CALLBACK(JSBool)
link_array_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSObjectArray *array;
    MochaDecoder *decoder;
    MWContext *context;
    jsint count, slot;
    LO_AnchorData *anchor_data;
    int32 active_layer_id;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    array = JS_GetInstancePrivate(cx, obj, &lm_link_array_class, NULL);
    if (!array)
	return JS_TRUE;
    decoder = array->decoder;
    context = decoder->window_context;
    if (!context) return JS_TRUE;

    LO_LockLayout();
    switch (slot) {
      case ANCHOR_ARRAY_LENGTH:
	active_layer_id = LM_GetActiveLayer(context);
	LM_SetActiveLayer(context, array->layer_id);
	count = LO_EnumerateLinks(context, array->layer_id);
	LM_SetActiveLayer(context, active_layer_id);
	if (count > array->length)
	    array->length = count;
	*vp = INT_TO_JSVAL(count);
	break;

      default:
	if (slot < 0) {
	    /* Don't mess with user-defined or method properties. */
	    LO_UnlockLayout();
	    return JS_TRUE;
	}
	if (slot >= array->length)
	    array->length = slot + 1;
	anchor_data = LO_GetLinkByIndex(context, array->layer_id, (uint)slot);
	if (anchor_data) {
	    *vp = OBJECT_TO_JSVAL(LM_ReflectLink(context, anchor_data, NULL,
						 array->layer_id, (uint)slot));
	}
	break;
    }
    LO_UnlockLayout();
    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
anchor_array_finalize(JSContext *cx, JSObject *obj)
{
    JSObjectArray *array;

    array = JS_GetPrivate(cx, obj);
    if (!array)
        return;
    DROP_BACK_COUNT(array->decoder);
    JS_free(cx, array);
}

JSClass lm_link_array_class = {
    "LinkArray", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    link_array_getProperty, link_array_getProperty, JS_EnumerateStub,
    JS_ResolveStub, JS_ConvertStub, anchor_array_finalize
};

PR_STATIC_CALLBACK(JSBool)
LinkArray(JSContext *cx, JSObject *obj,
	  uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

extern JSClass lm_anchor_array_class;

PR_STATIC_CALLBACK(JSBool)
anchor_array_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSObjectArray *array;
    MochaDecoder *decoder;
    MWContext *context;
    jsint count, slot;
    lo_NameList *name_rec;
    int32 active_layer_id;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    array = JS_GetInstancePrivate(cx, obj, &lm_anchor_array_class, NULL);
    if (!array)
	return JS_TRUE;
    decoder = array->decoder;
    context = decoder->window_context;
    if (!context) return JS_TRUE;
    LO_LockLayout();
    switch (slot) {
      case ANCHOR_ARRAY_LENGTH:
	active_layer_id = LM_GetActiveLayer(context);
	LM_SetActiveLayer(context, array->layer_id);
	count = LO_EnumerateNamedAnchors(context, array->layer_id);
	LM_SetActiveLayer(context, active_layer_id);
	if (count > array->length)
	    array->length = count;
	*vp = INT_TO_JSVAL(count);
	break;

      default:
	if (slot >= array->length)
	    array->length = slot + 1;
	name_rec = LO_GetNamedAnchorByIndex(context, array->layer_id,
                                            (uint)slot);
	if (name_rec) {
	    *vp = OBJECT_TO_JSVAL(LM_ReflectNamedAnchor(context, name_rec,
							NULL, array->layer_id,
                                                        (uint)slot));
	}
	break;
    }
    LO_UnlockLayout();
    return JS_TRUE;
}

JSClass lm_anchor_array_class = {
    "AnchorArray", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    anchor_array_getProperty, anchor_array_getProperty, JS_EnumerateStub,
    JS_ResolveStub, JS_ConvertStub, anchor_array_finalize
};

PR_STATIC_CALLBACK(JSBool)
AnchorArray(JSContext *cx, JSObject *obj,
	    uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSObject *
reflect_anchor_array(MochaDecoder *decoder, JSClass *clasp,
		     JSNative constructor, JSObject *document)
{
    JSContext *cx;
    JSObject *obj, *prototype;
    JSObjectArray *array;
    JSDocument *doc;

    cx = decoder->js_context;
    doc = JS_GetPrivate(cx, document);
    if (!doc)
	return NULL;

    prototype = JS_InitClass(cx, decoder->window_object, NULL,
			     clasp, constructor, 0,
			     anchor_array_props, NULL, NULL, NULL);
    if (!prototype)
	return NULL;
    array = JS_malloc(cx, sizeof *array);
    if (!array)
	return NULL;
    XP_BZERO(array, sizeof *array);
    obj = JS_NewObject(cx, clasp, prototype, document);
    if (!obj || !JS_SetPrivate(cx, obj, array)) {
	JS_free(cx, array);
	return NULL;
    }
    array->decoder = HOLD_BACK_COUNT(decoder);
    array->layer_id = doc->layer_id;
    return obj;
}

JSObject *
lm_GetLinkArray(MochaDecoder *decoder, JSObject *document)
{
    JSObject *obj;
    JSDocument *doc;

    doc = JS_GetPrivate(decoder->js_context, document);
    if (!doc)
	return NULL;

    obj = doc->links;
    if (obj)
	return obj;
    obj = reflect_anchor_array(decoder, &lm_link_array_class, LinkArray,
                               document);
    doc->links = obj;
    return obj;
}

JSObject *
lm_GetNameArray(MochaDecoder *decoder, JSObject *document)
{
    JSObject *obj;
    JSDocument *doc;

    doc = JS_GetPrivate(decoder->js_context, document);
    if (!doc)
	return NULL;

    obj = doc->anchors;
    if (obj)
	return obj;
    obj = reflect_anchor_array(decoder, &lm_anchor_array_class, AnchorArray,
                               document);
    doc->anchors = obj;
    return obj;
}

JSObject *
LM_ReflectLink(MWContext *context, LO_AnchorData *anchor_data, PA_Tag * tag,
               int32 layer_id, uint index)
{
    JSObject *obj, *array_obj, *document;
    MochaDecoder *decoder;
    JSContext *cx;
    JSURL *url;
    lo_TopState *top_state;
    PRHashTable *map;

    anchor_data = LO_GetLinkByIndex(context, layer_id, index);
    if (!anchor_data)
	return NULL;

    obj = anchor_data->mocha_object;
    if (obj)
	return obj;

    decoder = LM_GetMochaDecoder(context);
    if (!decoder)
	return NULL;
    cx = decoder->js_context;

    top_state = lo_GetMochaTopState(context);
    if (top_state->resize_reload) {
        map = lm_GetIdToObjectMap(decoder);

        if (map)
            obj = (JSObject *)PR_HashTableLookup(map,
                             LM_GET_MAPPING_KEY(LM_LINKS, layer_id, index));
        if (obj) {
            anchor_data->mocha_object = obj;
            LM_PutMochaDecoder(decoder);
            return obj;
        }
    }

    /* Get the document object that will hold this link */
    document = lm_GetDocumentFromLayerId(decoder, layer_id);
    if (!document) {
	LM_PutMochaDecoder(decoder);
	return NULL;
    }

    array_obj = lm_GetLinkArray(decoder, document);
    if (!array_obj) {
	LM_PutMochaDecoder(decoder);
	return NULL;
    }

    url = lm_NewURL(cx, decoder, anchor_data, document);

    if (!url) {
	LM_PutMochaDecoder(decoder);
	return NULL;
    }
    url->index = index;
    url->layer_id = layer_id;
    obj = url->url_object;

    /* XXX should find a name/id to pass in */
    /* put it in the links array */
    if (!lm_AddObjectToArray(cx, array_obj, NULL, index, obj)) {
	LM_PutMochaDecoder(decoder);
    	return NULL;
    }

    /* Put it in the index to object hash table */
    map = lm_GetIdToObjectMap(decoder);
    if (map)
        PR_HashTableAdd(map, LM_GET_MAPPING_KEY(LM_LINKS, layer_id, index),
                        obj);

    anchor_data->mocha_object = obj;
    LM_PutMochaDecoder(decoder);

    /* see if there are any event handlers we need to reflect */
    if(tag) {

	PA_Block onclick, onmouseover, onmouseout, onmousedown, onmouseup, ondblclick, id;

	/* don't hold the layout lock across compiles */
	LO_UnlockLayout();

	onclick = lo_FetchParamValue(context, tag, PARAM_ONCLICK);
	onmouseover = lo_FetchParamValue(context, tag, PARAM_ONMOUSEOVER);
	onmouseout  = lo_FetchParamValue(context, tag, PARAM_ONMOUSEOUT);
	onmousedown = lo_FetchParamValue(context, tag, PARAM_ONMOUSEDOWN);
	onmouseup   = lo_FetchParamValue(context, tag, PARAM_ONMOUSEUP);
	ondblclick  = lo_FetchParamValue(context, tag, PARAM_ONDBLCLICK);
        id = lo_FetchParamValue(context, tag, PARAM_ID);

	if (onclick) {
	    (void) lm_CompileEventHandler(decoder, id, tag->data,
					  tag->newline_count, url->url_object,
				          PARAM_ONCLICK, onclick);
	    PA_FREE(onclick);
	    anchor_data->event_handler_present = TRUE;
	}
	if (onmouseover) {
	    (void) lm_CompileEventHandler(decoder, id, tag->data,
					  tag->newline_count, url->url_object,
					  PARAM_ONMOUSEOVER, onmouseover);
	    PA_FREE(onmouseover);
	    anchor_data->event_handler_present = TRUE;
	}
	if (onmouseout) {
	    (void) lm_CompileEventHandler(decoder, id, tag->data,
					  tag->newline_count, url->url_object,
					  PARAM_ONMOUSEOUT, onmouseout);
	    PA_FREE(onmouseout);
	    anchor_data->event_handler_present = TRUE;
	}
	if (onmousedown) {
	    (void) lm_CompileEventHandler(decoder, id, tag->data,
					  tag->newline_count, url->url_object,
					  PARAM_ONMOUSEDOWN, onmousedown);
	    PA_FREE(onmousedown);
	    anchor_data->event_handler_present = TRUE;
	}
	if (onmouseup) {
	    (void) lm_CompileEventHandler(decoder, id, tag->data,
					  tag->newline_count, url->url_object,
					  PARAM_ONMOUSEUP, onmouseup);
	    PA_FREE(onmouseup);
	    anchor_data->event_handler_present = TRUE;
	}
	if (ondblclick) {
	    (void) lm_CompileEventHandler(decoder, id, tag->data,
					  tag->newline_count, url->url_object,
					  PARAM_ONDBLCLICK, ondblclick);
	    PA_FREE(ondblclick);
	    anchor_data->event_handler_present = TRUE;
	}
	if (id)
	    PA_FREE(id);
        LO_LockLayout();
    }

    return obj;
}

/*
 * XXX chouck - the onLocate stuff hasn't been implemented yet
 *              so just return JS_TRUE and go away
 */
JSBool
LM_SendOnLocate(MWContext *context, lo_NameList *name_rec)
{
    JSBool ok;
    JSObject *obj;
    jsval result;
    JSEvent *event;

    return JS_TRUE;

    obj = name_rec->mocha_object;
    if (!obj)
        return JS_FALSE;

    event = XP_NEW_ZAP(JSEvent);
    event->type = EVENT_LOCATE;
    ok = lm_SendEvent(context, obj, event, &result);
    return ok;
}

/* Anchor property slots. */
enum anchor_slot {
    ANCHOR_TEXT     = -1,
    ANCHOR_NAME     = -2,
    ANCHOR_X        = -3,
    ANCHOR_Y        = -4
};

/* Static anchor properties. */
static JSPropertySpec anchor_props[] = {
    {"text",        ANCHOR_TEXT,         JSPROP_ENUMERATE | JSPROP_READONLY},
    {"name",        ANCHOR_NAME,         JSPROP_ENUMERATE | JSPROP_READONLY},
    {"x",           ANCHOR_X,            JSPROP_ENUMERATE | JSPROP_READONLY},
    {"y",           ANCHOR_Y,            JSPROP_ENUMERATE | JSPROP_READONLY},
    {0}
};

/* Base anchor element type. */
typedef struct JSAnchor {
    MochaDecoder   *decoder;
    int32           layer_id;
    uint            index;
    JSString       *text;
    JSString       *name;
} JSAnchor;

extern JSClass lm_anchor_class;

PR_STATIC_CALLBACK(JSBool)
anchor_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSAnchor *named_anchor;
    lo_NameList *name_rec;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    named_anchor = JS_GetInstancePrivate(cx, obj, &lm_anchor_class, NULL);
    if (!named_anchor)
	return JS_TRUE;

    LO_LockLayout();
    name_rec = LO_GetNamedAnchorByIndex(named_anchor->decoder->window_context,
                                        named_anchor->layer_id,
                                        named_anchor->index);
    if (!name_rec) {
	LO_UnlockLayout();
        return JS_TRUE;      /* Try to handle this case gracefully. */
    }

    switch (slot) {
    case ANCHOR_TEXT:
        if (named_anchor->text)
            *vp = STRING_TO_JSVAL(named_anchor->text);
	else
	    *vp = JSVAL_NULL;
        break;

    case ANCHOR_NAME:
        if (named_anchor->name)
            *vp = STRING_TO_JSVAL(named_anchor->name);
	else
	    *vp = JSVAL_NULL;
        break;

    case ANCHOR_X:
        *vp = INT_TO_JSVAL(name_rec->element->lo_any.x);
        break;

    case ANCHOR_Y:
	*vp = INT_TO_JSVAL(name_rec->element->lo_any.y);
        break;

    default:
        /* Don't mess with a user-defined or method property. */
        break;
    }
    LO_UnlockLayout();
    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
anchor_finalize(JSContext *cx, JSObject *obj)
{
    JSAnchor *named_anchor;
    MochaDecoder *decoder;
    MWContext *context;
    lo_NameList *name_rec;

    named_anchor = JS_GetPrivate(cx, obj);
    if (!named_anchor)
        return;
    decoder = named_anchor->decoder;
    context = decoder->window_context;
    if (context) {
	LO_LockLayout();
	name_rec = LO_GetNamedAnchorByIndex(context, named_anchor->layer_id,
                                            named_anchor->index);
	if (name_rec && name_rec->mocha_object == obj)
	    name_rec->mocha_object = NULL;
	LO_UnlockLayout();
    }
    DROP_BACK_COUNT(decoder);
    JS_UnlockGCThing(cx, named_anchor->text);
    JS_UnlockGCThing(cx, named_anchor->name);
    JS_free(cx, named_anchor);
}


/* Named anchors are read only, so there is no anchor_setProperty. */
JSClass lm_anchor_class = {
    "Anchor", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, anchor_getProperty, anchor_getProperty,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, anchor_finalize
};

PR_STATIC_CALLBACK(JSBool)
Anchor(JSContext *cx, JSObject *obj,
     uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

JSObject *
LM_ReflectNamedAnchor(MWContext *context, lo_NameList *name_rec,
                      PA_Tag * tag, int32 layer_id, uint index)
{
    JSObject *obj, *array_obj, *document;
    MochaDecoder *decoder;
    JSContext *cx;
    JSObjectArray *array;
    JSAnchor *named_anchor;
    lo_TopState *top_state;
    PRHashTable *map;
    JSString *str;

    obj = name_rec->mocha_object;
    if (obj)
        return obj;

    decoder = LM_GetMochaDecoder(context);
    if (!decoder)
        return NULL;
    cx = decoder->js_context;

    top_state = lo_GetMochaTopState(context);
    if (top_state->resize_reload) {
        map = lm_GetIdToObjectMap(decoder);

        if (map)
            obj = (JSObject *)PR_HashTableLookup(map,
                                         LM_GET_MAPPING_KEY(LM_NAMEDANCHORS,
                                                            layer_id, index));
        if (obj) {
            name_rec->mocha_object = obj;
            LM_PutMochaDecoder(decoder);
            return obj;
        }
    }

    /* Get the document object that will hold this anchor */
    document = lm_GetDocumentFromLayerId(decoder, layer_id);
    if (!document) {
	LM_PutMochaDecoder(decoder);
	return NULL;
    }

    array_obj = lm_GetNameArray(decoder, document);
    if (!array_obj) {
        LM_PutMochaDecoder(decoder);
        return NULL;
    }
    array = JS_GetPrivate(cx, array_obj);
    if (!array) {
        LM_PutMochaDecoder(decoder);
	return NULL;
    }

    named_anchor = JS_malloc(cx, sizeof *named_anchor);
    if (!named_anchor) {
        LM_PutMochaDecoder(decoder);
        return NULL;
    }
    XP_BZERO(named_anchor, sizeof *named_anchor);

    obj = JS_NewObject(cx, &lm_anchor_class, decoder->anchor_prototype,
		       document);

    if (!obj || !JS_SetPrivate(cx, obj, named_anchor)) {
        JS_free(cx, named_anchor);
        LM_PutMochaDecoder(decoder);
        return NULL;
    }

    /* Put obj into the document.anchors array. */
    JS_DefineProperty(cx, array_obj, (char *) name_rec->name,
		      OBJECT_TO_JSVAL(obj), NULL, NULL,
		      JSPROP_ENUMERATE|JSPROP_READONLY);
    JS_AliasElement(cx, array_obj, (char *) name_rec->name, index);

    /* Put it in the index to object hash table */
    map = lm_GetIdToObjectMap(decoder);
    if (map) {
        PR_HashTableAdd(map,
                        LM_GET_MAPPING_KEY(LM_NAMEDANCHORS, layer_id, index),
                        obj);
    }
    if ((jsint) index >= array->length)
	array->length = index + 1;

    named_anchor->decoder = HOLD_BACK_COUNT(decoder);
    named_anchor->layer_id = layer_id;
    named_anchor->index = index;
    if (name_rec->element && name_rec->element->type == LO_TEXT) {
	str = lm_LocalEncodingToStr(context, 
				    (char *) name_rec->element->lo_text.text);
	if (!str || !JS_LockGCThing(cx, str)) {
	    LM_PutMochaDecoder(decoder);
	    return NULL;
	}
	named_anchor->text = str;
    }
    str = JS_NewStringCopyZ(cx, (char *) name_rec->name);
    if (!str || !JS_LockGCThing(cx, str)) {
        LM_PutMochaDecoder(decoder);
	return NULL;
    }
    named_anchor->name = str;

    name_rec->mocha_object = obj;

    /* see if there are any attributes for event handlers */
    if(tag) {
        PA_Block onlocate, id;

	/* don't hold the layout lock across compiles */
	LO_UnlockLayout();

        onlocate = lo_FetchParamValue(context, tag, PARAM_ONLOCATE);
        id = lo_FetchParamValue(context, tag, PARAM_ID);
        if (onlocate) {
	    (void) lm_CompileEventHandler(decoder, id, tag->data,
					  tag->newline_count, obj,
					  PARAM_ONLOCATE, onlocate);
            PA_FREE(onlocate);
        }
	if (id)
	    PA_FREE(id);
	LO_LockLayout();
    }

    LM_PutMochaDecoder(decoder);
    return obj;
}


JSBool
lm_InitAnchorClass(MochaDecoder *decoder)
{
    JSContext *cx;
    JSObject *prototype;

    cx = decoder->js_context;
    prototype = JS_InitClass(cx, decoder->window_object,
			     decoder->event_receiver_prototype,
			     &lm_anchor_class, Anchor, 0,
			     anchor_props, NULL, NULL, NULL);
    if (!prototype)
	return JS_FALSE;
    decoder->anchor_prototype = prototype;
    return JS_TRUE;
}
