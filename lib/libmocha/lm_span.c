/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * JS reflection of the named SPANS in the current document.
 *
 * Chris Toshok, 26-Feb-1998
 */

#ifdef DOM

#include "lm.h"

#include "lo_ele.h"
#include "prtypes.h"
#include "pa_tags.h"
#include "layout.h"

enum span_array_slot {
  SPAN_ARRAY_LENGTH = -1
};

static JSPropertySpec span_array_props[] = {
  { lm_length_str, SPAN_ARRAY_LENGTH,
    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
  { 0 }
};

extern JSClass lm_span_array_class;

PR_STATIC_CALLBACK(JSBool)
span_array_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  JSObjectArray *array;
  MochaDecoder *decoder;
  MWContext *context;
  jsint count, slot;
  lo_NameList *span;
  int32 active_layer_id;

  /* XP_TRACE (("span_array_getProperty called\n")); */

  if (!JSVAL_IS_INT(id))
	return JS_TRUE;

  slot = JSVAL_TO_INT(id);

  array = JS_GetInstancePrivate(cx, obj, &lm_span_array_class, NULL);
  if (!array)
	return JS_TRUE;
  decoder = array->decoder;
  context = decoder->window_context;
  if (!context)
	return JS_TRUE;

  LO_LockLayout();
  switch (slot) {
  case SPAN_ARRAY_LENGTH:
	active_layer_id = LM_GetActiveLayer(context);
	LM_SetActiveLayer(context, array->layer_id);
	count = LO_EnumerateSpans(context, array->layer_id);
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
	span = LO_GetSpanByIndex(context, array->layer_id, (uint)slot);
	XP_TRACE (("span = %p\n", span));
	if (span) {
      *vp = OBJECT_TO_JSVAL(LM_ReflectSpan(context, span, NULL,
                                           array->layer_id, (uint)slot));
	}
	break;
  }
  LO_UnlockLayout();
  return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
span_array_finalize(JSContext *cx, JSObject *obj)
{
  JSObjectArray *array;

  array = JS_GetPrivate(cx, obj);
  if (!array)
    return;
  DROP_BACK_COUNT(array->decoder);
  JS_free(cx, array);
}

JSClass lm_span_array_class = {
  "SpanArray", JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub,
  span_array_getProperty, span_array_getProperty, JS_EnumerateStub,
  JS_ResolveStub, JS_ConvertStub, span_array_finalize
};


PR_STATIC_CALLBACK(JSBool)
SpanArray(JSContext *cx, JSObject *obj,
          uint argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSObject *
reflect_span_array(MochaDecoder *decoder, JSClass *clasp,
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
                           span_array_props, NULL, NULL, NULL);
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
lm_GetSpanArray(MochaDecoder *decoder, JSObject *document)
{
  JSObject *obj;
  JSDocument *doc;

  doc = JS_GetPrivate(decoder->js_context, document);
  if (!doc)
	return NULL;

  obj = doc->spans;
  if (obj)
	return obj;
  obj = reflect_span_array(decoder, &lm_span_array_class, SpanArray,
                           document);
  doc->spans = obj;
  return obj;
}

JSBool PR_CALLBACK span_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
JSBool PR_CALLBACK span_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
void PR_CALLBACK span_Finalize(JSContext *cx, JSObject *obj);

enum {
  SPAN_NAME			= -1,
  SPAN_FONTFAMILY	= -2,
  SPAN_FONTWEIGHT	= -3,
  SPAN_FONTSIZE		= -4,
  SPAN_FONTSLANT	= -5,
  SPAN_COLOR		= -6,
  SPAN_BGCOLOR		= -7
};

static JSPropertySpec span_props[] =
{
  {"name",			SPAN_NAME,			JSPROP_ENUMERATE | JSPROP_READONLY },
  {"fontFamily",	SPAN_FONTFAMILY,	JSPROP_ENUMERATE },
  {"fontWeight",	SPAN_FONTWEIGHT,	JSPROP_ENUMERATE },
  {"fontSize",		SPAN_FONTSIZE,		JSPROP_ENUMERATE },
  {"fontSlant",		SPAN_FONTSLANT,		JSPROP_ENUMERATE },
  {"color",			SPAN_COLOR,			JSPROP_ENUMERATE },
  {"bgcolor",		SPAN_BGCOLOR,		JSPROP_ENUMERATE },
  { 0 }
};

static JSClass lm_span_class =
{
  "Span", JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub,
  span_getProperty, span_setProperty,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, span_Finalize
};

/* Base span element type. */
typedef struct JSSpan {
  MochaDecoder   *decoder;
  int32           layer_id;
  uint            index;
  JSString       *text;
  JSString       *name;
} JSSpan;

PR_STATIC_CALLBACK(JSBool)
Span(JSContext *cx, JSObject *obj,
     uint argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

JSObject *
LM_ReflectSpan(MWContext *context, lo_NameList *name_rec,
               PA_Tag * tag, int32 layer_id, uint index)
{
  JSObject *obj, *array_obj, *document;
  MochaDecoder *decoder;
  JSContext *cx;
  JSObjectArray *array;
  JSSpan *named_span;
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
                                           LM_GET_MAPPING_KEY(LM_SPANS,
                                                              layer_id, index));
    if (obj) {
      name_rec->mocha_object = obj;
      LM_PutMochaDecoder(decoder);
      return obj;
    }
  }

  /* Get the document object that will hold this span */
  document = lm_GetDocumentFromLayerId(decoder, layer_id);
  if (!document) {
	LM_PutMochaDecoder(decoder);
	return NULL;
  }

  array_obj = lm_GetSpanArray(decoder, document);
  if (!array_obj) {
    LM_PutMochaDecoder(decoder);
    return NULL;
  }
  array = JS_GetPrivate(cx, array_obj);
  if (!array) {
    LM_PutMochaDecoder(decoder);
	return NULL;
  }

  named_span = JS_malloc(cx, sizeof *named_span);
  if (!named_span) {
    LM_PutMochaDecoder(decoder);
    return NULL;
  }
  XP_BZERO(named_span, sizeof *named_span);

  obj = JS_NewObject(cx, &lm_span_class, decoder->span_prototype,
                     document);

  if (!obj || !JS_SetPrivate(cx, obj, named_span)) {
    JS_free(cx, named_span);
    LM_PutMochaDecoder(decoder);
    return NULL;
  }

  /* Put obj into the document.spans array. */
  JS_DefineProperty(cx, array_obj, (char *) name_rec->name,
                    OBJECT_TO_JSVAL(obj), NULL, NULL,
                    JSPROP_ENUMERATE|JSPROP_READONLY);
  JS_AliasElement(cx, array_obj, (char *) name_rec->name, index);

  /* Put it in the index to object hash table */
  map = lm_GetIdToObjectMap(decoder);
  if (map) {
    PR_HashTableAdd(map,
                    LM_GET_MAPPING_KEY(LM_SPANS, layer_id, index),
                    obj);
  }
  if ((jsint) index >= array->length)
	array->length = index + 1;

  named_span->decoder = HOLD_BACK_COUNT(decoder);
  named_span->layer_id = layer_id;
  named_span->index = index;
  if (name_rec->element && name_rec->element->type == LO_TEXT) {
	str = lm_LocalEncodingToStr(context, 
                                (char *) name_rec->element->lo_text.text);
	if (!str || !JS_LockGCThing(cx, str)) {
      LM_PutMochaDecoder(decoder);
      return NULL;
	}
	named_span->text = str;
  }
  str = JS_NewStringCopyZ(cx, (char *) name_rec->name);
  if (!str || !JS_LockGCThing(cx, str)) {
    LM_PutMochaDecoder(decoder);
	return NULL;
  }
  named_span->name = str;

  name_rec->mocha_object = obj;

  /* see if there are any attributes for event handlers */
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
                                    tag->newline_count, obj,
                                    PARAM_ONCLICK, onclick);
      PA_FREE(onclick);
#if 0
      anchor_data->event_handler_present = TRUE;
#endif
	}
	if (onmouseover) {
      (void) lm_CompileEventHandler(decoder, id, tag->data,
                                    tag->newline_count, obj,
                                    PARAM_ONMOUSEOVER, onmouseover);
      PA_FREE(onmouseover);
#if 0
      anchor_data->event_handler_present = TRUE;
#endif
	}
	if (onmouseout) {
      (void) lm_CompileEventHandler(decoder, id, tag->data,
                                    tag->newline_count, obj,
                                    PARAM_ONMOUSEOUT, onmouseout);
      PA_FREE(onmouseout);
#if 0
      anchor_data->event_handler_present = TRUE;
#endif
	}
	if (onmousedown) {
      (void) lm_CompileEventHandler(decoder, id, tag->data,
                                    tag->newline_count, obj,
                                    PARAM_ONMOUSEDOWN, onmousedown);
      PA_FREE(onmousedown);
#if 0
      anchor_data->event_handler_present = TRUE;
#endif
	}
	if (onmouseup) {
      (void) lm_CompileEventHandler(decoder, id, tag->data,
                                    tag->newline_count, obj,
                                    PARAM_ONMOUSEUP, onmouseup);
      PA_FREE(onmouseup);
#if 0
      anchor_data->event_handler_present = TRUE;
#endif
	}
	if (ondblclick) {
      (void) lm_CompileEventHandler(decoder, id, tag->data,
                                    tag->newline_count, obj,
                                    PARAM_ONDBLCLICK, ondblclick);
      PA_FREE(ondblclick);
#if 0
      anchor_data->event_handler_present = TRUE;
#endif
	}

	if (id)
	  PA_FREE(id);
	LO_LockLayout();
  }

  LM_PutMochaDecoder(decoder);
  return obj;
}

JSBool PR_CALLBACK
span_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  JSSpan *span;
  lo_NameList *name_rec;
  jsint slot;
  JSBool ok;
  LO_Color *rgb;
  char *font_family, *font_weight, *font_slant;
  int32 font_size;

  if (!JSVAL_IS_INT(id)) {
    return JS_TRUE;
  }

  slot = JSVAL_TO_INT(id);

  span = JS_GetInstancePrivate(cx, obj, &lm_span_class, NULL);
  if (!span) 
	{
      return JS_TRUE;
    }
        
  LO_LockLayout();
  name_rec = LO_GetSpanByIndex(span->decoder->window_context,
                               span->layer_id,
                               span->index);
  if (!name_rec) 
	{
      LO_UnlockLayout();
      return JS_TRUE;      /* Try to handle this case gracefully. */
    }

  ok = JS_TRUE;

  switch (slot) 
	{
	case SPAN_COLOR:
      LO_UnlockLayout();
      if (!lm_jsval_to_rgb(cx, vp, &rgb))			
        return JS_FALSE;
      ET_TweakSpan(span->decoder->window_context, name_rec,
                   rgb, 0, SP_SetColor, span->decoder->doc_id);
      break;
	case SPAN_BGCOLOR:
      LO_UnlockLayout();
      if (!lm_jsval_to_rgb(cx, vp, &rgb))			
        return JS_FALSE;
      ET_TweakSpan(span->decoder->window_context, name_rec,
                   rgb, 0, SP_SetBackground, span->decoder->doc_id);
      break;
    case SPAN_FONTFAMILY:
      LO_UnlockLayout();
      font_family = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
      ET_TweakSpan(span->decoder->window_context, name_rec,
                   font_family, 0, SP_SetFontFamily, span->decoder->doc_id);
      break;
    case SPAN_FONTWEIGHT:
      LO_UnlockLayout();
      font_weight = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
      ET_TweakSpan(span->decoder->window_context, name_rec,
                   font_weight, 0, SP_SetFontWeight, span->decoder->doc_id);
      break;
    case SPAN_FONTSIZE:
      LO_UnlockLayout();
      font_size = JSVAL_TO_INT(*vp);
      ET_TweakSpan(span->decoder->window_context, name_rec,
                   NULL, font_size, SP_SetFontSize, span->decoder->doc_id);
      break;
    case SPAN_FONTSLANT:
      LO_UnlockLayout();
      font_slant = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
      ET_TweakSpan(span->decoder->window_context, name_rec,
                   font_slant, 0, SP_SetFontSlant, span->decoder->doc_id);
      break;
	default:
      ok = JS_FALSE;
      break;
	}

  return ok;
}

JSBool PR_CALLBACK
span_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  XP_TRACE (("Span get property called...\n"));
  return JS_FALSE;	
}

void PR_CALLBACK
span_Finalize(JSContext *cx, JSObject *obj)
{
  JSSpan *span;
  MochaDecoder *decoder;
  MWContext *context;
  lo_NameList *name_rec;

  span = JS_GetPrivate(cx, obj);
  if (!span)
    return;
  decoder = span->decoder;
  context = decoder->window_context;
  if (context) {
	LO_LockLayout();
	name_rec = LO_GetSpanByIndex(context, span->layer_id,
                                 span->index);
	if (name_rec && name_rec->mocha_object == obj)
      name_rec->mocha_object = NULL;
	LO_UnlockLayout();
  }
  DROP_BACK_COUNT(decoder);
  JS_UnlockGCThing(cx, span->text);
  JS_UnlockGCThing(cx, span->name);
  JS_free(cx, span);
}

JSBool
lm_InitSpanClass(MochaDecoder *decoder)
{
  JSContext *cx;
  JSObject *prototype;

  cx = decoder->js_context;
  prototype = JS_InitClass(cx, decoder->window_object,
                           decoder->event_receiver_prototype,
                           &lm_span_class, Span, 0,
                           span_props, NULL, NULL, NULL);
  if (!prototype)
	return JS_FALSE;
  decoder->span_prototype = prototype;
  return JS_TRUE;
}

#endif /* DOM */
