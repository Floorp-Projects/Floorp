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
 * JS reflection of XML transclusions in the current document.
 *
 * Nisheeth Ranjan, 04-Apr-1998
 */

#ifdef DOM

#include "lm.h"
#include "lo_ele.h"
#include "layout.h"
#include "prtypes.h"


enum transclusion_array_slot {
  TRANSCLUSION_ARRAY_LENGTH = -1
};

static JSPropertySpec transclusion_array_props[] = {
  { lm_length_str, TRANSCLUSION_ARRAY_LENGTH,
    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
  { 0 }
};

extern JSClass lm_transclusion_array_class;

PR_STATIC_CALLBACK(JSBool)
transclusion_array_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  JSObjectArray *array;
  void *xmlTrans;
  MochaDecoder *decoder;
  MWContext *context;
  jsint count, slot;
  int32 active_layer_id;

  /* XP_TRACE (("span_array_getProperty called\n")); */

  if (!JSVAL_IS_INT(id))
	return JS_TRUE;

  slot = JSVAL_TO_INT(id);

  array = JS_GetInstancePrivate(cx, obj, &lm_transclusion_array_class, NULL);
  if (!array)
	return JS_TRUE;
  decoder = array->decoder;
  context = decoder->window_context;
  if (!context)
	return JS_TRUE;

  LO_LockLayout();
  switch (slot) {
  case TRANSCLUSION_ARRAY_LENGTH:
	active_layer_id = LM_GetActiveLayer(context);
	LM_SetActiveLayer(context, array->layer_id);
	count = XMLTransclusionCount(context);
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

	xmlTrans = XMLGetTransclusionByIndex(context, (uint)slot);
	/* XP_TRACE (("span = %p\n", span)); */
	if (xmlTrans) {
      *vp = OBJECT_TO_JSVAL(LM_ReflectTransclusion(context, xmlTrans, array->layer_id, (uint)slot));
	}
	break;
  }
  LO_UnlockLayout();
  return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
transclusion_array_finalize(JSContext *cx, JSObject *obj)
{
  JSObjectArray *array;

  array = JS_GetPrivate(cx, obj);
  if (!array)
    return;
  DROP_BACK_COUNT(array->decoder);
  JS_free(cx, array);
}

JSClass lm_transclusion_array_class = {
  "TransclusionArray", JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub,
  transclusion_array_getProperty, transclusion_array_getProperty, JS_EnumerateStub,
  JS_ResolveStub, JS_ConvertStub, transclusion_array_finalize
};


PR_STATIC_CALLBACK(JSBool)
TransclusionArray(JSContext *cx, JSObject *obj,
          uint argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

static JSObject *
reflect_transclusion_array(MochaDecoder *decoder, JSClass *clasp,
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
                           transclusion_array_props, NULL, NULL, NULL);
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
lm_GetTransclusionArray(MochaDecoder *decoder, JSObject *document)
{
  JSObject *obj;
  JSDocument *doc;

  doc = JS_GetPrivate(decoder->js_context, document);
  if (!doc)
	return NULL;

  obj = doc->transclusions;
  if (obj)
	return obj;
  obj = reflect_transclusion_array(decoder, &lm_transclusion_array_class, TransclusionArray,
                           document);
  doc->transclusions = obj;
  return obj;
}

JSBool PR_CALLBACK transclusion_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
JSBool PR_CALLBACK transclusion_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
void PR_CALLBACK transclusion_Finalize(JSContext *cx, JSObject *obj);

enum {
/*  TRANS_NAME		= -1, */
  TRANS_HREF		= -1,
  TRANS_VISIBILITY	= -2,
  TRANS_DATA		= -3
};

static JSPropertySpec transclusion_props[] =
{
  /* {"name",			TRANS_NAME,			JSPROP_ENUMERATE | JSPROP_READONLY }, */
  {"href",			TRANS_HREF,			JSPROP_ENUMERATE },
  {"visibility",	TRANS_VISIBILITY,	JSPROP_ENUMERATE },
  {"data",			TRANS_DATA,			JSPROP_ENUMERATE },
  { 0 }
};

static JSClass lm_transclusion_class =
{
  "Transclusion", JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub,
  transclusion_getProperty, transclusion_setProperty,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, transclusion_Finalize
};

/* Base transclusion element type. */
typedef struct JSTransclusion {
  MochaDecoder	*decoder;
  int32			layer_id;
  uint			index;
  /* JSString		*name; */
  void			*xmlFile;
} JSTransclusion;

PR_STATIC_CALLBACK(JSBool)
Transclusion(JSContext *cx, JSObject *obj,
     uint argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

/*
JSObject *
LM_ReflectTransclusion(MWContext *context, lo_NameList *name_rec,
               PA_Tag * tag, int32 layer_id, uint index)
*/
JSObject *
LM_ReflectTransclusion(MWContext *context, void *ele, int32 layer_id, uint index)
{
  JSObject *obj, *array_obj, *document;
  MochaDecoder *decoder;
  JSContext *cx;
  JSObjectArray *array;
  JSTransclusion *trans;
  lo_TopState *top_state;
  PRHashTable *map;
  JSString *str;


  obj = XML_GetMochaObject(ele);
  if (obj)
    return obj;

  decoder = LM_GetMochaDecoder(context);
  if (!decoder)
    return NULL;
  cx = decoder->js_context;

 /* top_state = lo_GetMochaTopState(context);
  if (top_state->resize_reload) {
    map = lm_GetIdToObjectMap(decoder);

    if (map)
      obj = (JSObject *)PR_HashTableLookup(map,
                                           LM_GET_MAPPING_KEY(LM_TRANSCLUSIONS,
                                                              layer_id, index));
    if (obj) {
      XML_SetMochaObject(ele, obj);
      LM_PutMochaDecoder(decoder);
      return obj;
    }
  } */

  /* Get the document object that will hold this transclusion */
  document = lm_GetDocumentFromLayerId(decoder, layer_id);
  if (!document) {
	LM_PutMochaDecoder(decoder);
	return NULL;
  }

  array_obj = lm_GetTransclusionArray(decoder, document);
  if (!array_obj) {
    LM_PutMochaDecoder(decoder);
    return NULL;
  }
  array = JS_GetPrivate(cx, array_obj);
  if (!array) {
    LM_PutMochaDecoder(decoder);
	return NULL;
  }

  trans = JS_malloc(cx, sizeof *trans);
  if (!trans) {
    LM_PutMochaDecoder(decoder);
    return NULL;
  }
  XP_BZERO(trans, sizeof *trans);

  obj = JS_NewObject(cx, &lm_transclusion_class, decoder->transclusion_prototype,
                     document);

  if (!obj || !JS_SetPrivate(cx, obj, trans)) {
    JS_free(cx, trans);
    LM_PutMochaDecoder(decoder);
    return NULL;
  }

  /* Put obj into the document.transclusions array. */
  /*
  JS_DefineProperty(cx, array_obj, (char *) name_rec->name,
                    OBJECT_TO_JSVAL(obj), NULL, NULL,
                    JSPROP_ENUMERATE|JSPROP_READONLY);
  JS_AliasElement(cx, array_obj, (char *) name_rec->name, index);
   */

  /* Put it in the index to object hash table */
  map = lm_GetIdToObjectMap(decoder);
  if (map) {
    PR_HashTableAdd(map,
                    LM_GET_MAPPING_KEY(LM_TRANSCLUSIONS, layer_id, index),
                    obj);
  }
  if ((jsint) index >= array->length)
	array->length = index + 1;

  trans->decoder = HOLD_BACK_COUNT(decoder);
  trans->layer_id = layer_id;
  trans->index = index;
  trans->xmlFile = context->xmlfile;
  /*
  str = JS_NewStringCopyZ(cx, (char *) name_rec->name);
  if (!str || !JS_LockGCThing(cx, str)) {
    LM_PutMochaDecoder(decoder);
	return NULL;
  }
  trans->name = str;
  */
  XML_SetMochaObject(ele, obj);

  LM_PutMochaDecoder(decoder);
  return obj;
}

JSBool PR_CALLBACK
transclusion_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  JSTransclusion *trans;
  jsint slot;
  JSBool ok;
  char *url;

  XP_ASSERT(JSVAL_IS_INT(id));
  slot = JSVAL_TO_INT(id);

  trans = JS_GetInstancePrivate(cx, obj, &lm_transclusion_class, NULL);
  if (!trans) 
	{
      return JS_TRUE;
    }
        
  ok = JS_TRUE;

  switch (slot) 
	{
	case TRANS_HREF:
      url = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
      ET_TweakTransclusion(trans->decoder->window_context, trans->xmlFile, url, trans->index, TR_SetHref, trans->decoder->doc_id);
      break;    
	default:
      ok = JS_FALSE;
      break;
	}

  return ok;
}

JSBool PR_CALLBACK
transclusion_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  XP_TRACE (("Transclusion get property called...\n"));
  return JS_FALSE;	
}

void PR_CALLBACK
transclusion_Finalize(JSContext *cx, JSObject *obj)
{
  JSTransclusion *trans;
  MochaDecoder *decoder;
  MWContext *context;

  trans = JS_GetPrivate(cx, obj);
  if (!trans)
    return;
  decoder = trans->decoder;
  context = decoder->window_context;

  
  
  /*
  if (context) {
	LO_LockLayout();
	name_rec = LO_GetSpanByIndex(context, span->layer_id,
                                 span->index);
	if (name_rec && name_rec->mocha_object == obj)
      name_rec->mocha_object = NULL;
	LO_UnlockLayout();
  }
  */
  
  XMLDeleteMochaObjectReference(trans->xmlFile, trans->index);

  DROP_BACK_COUNT(decoder);
  /*
  JS_UnlockGCThing(cx, trans->text);
  JS_UnlockGCThing(cx, trans->name);
  */
  JS_free(cx, trans);
}

JSBool
lm_InitTransclusionClass(MochaDecoder *decoder)
{
  JSContext *cx;
  JSObject *prototype;

  cx = decoder->js_context;
  prototype = JS_InitClass(cx, decoder->window_object,
                           decoder->event_receiver_prototype,
                           &lm_transclusion_class, Transclusion, 0,
                           transclusion_props, NULL, NULL, NULL);
  if (!prototype)
	return JS_FALSE;
  decoder->transclusion_prototype = prototype;
  return JS_TRUE;
}

#endif /* DOM */
