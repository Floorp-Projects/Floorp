/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * JS reflection of embeds in the current document.  The
 * reflected object is the java object associated with the
 * embed... if there is none then the reflection is JSVAL_NULL.
 */

/* Please leave outside of ifdef for windows precompiled headers */
#include "lm.h"

#ifdef JAVA

#include "xp.h"
#include "layout.h"
#include "np.h"
#include "nppriv.h"
#include "jri.h"
#include "jsjava.h"
#include "jsobj.h"
#include "prlog.h"

extern PRLogModuleInfo* Moja;
#define warn	PR_LOG_WARN
#define debug	PR_LOG_DEBUG

enum embed_array_slot {
    EMBED_ARRAY_LENGTH = -1
};

static JSPropertySpec embed_array_props[] = {
    {lm_length_str, EMBED_ARRAY_LENGTH,
		    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {0}
};

extern JSClass lm_embed_array_class;

PR_STATIC_CALLBACK(JSBool)
embed_array_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSObjectArray *array;
    MochaDecoder *decoder;
    MWContext *context;
    jsint count, slot;
    LO_EmbedStruct *embed_data;
    int32 active_layer_id;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    array = JS_GetInstancePrivate(cx, obj, &lm_embed_array_class, NULL);
    if (!array)
	return JS_TRUE;
    decoder = array->decoder;
    context = decoder->window_context;

    if (!context) return JS_TRUE;

    if (LM_MOJA_OK != ET_InitMoja(context))
        return JS_FALSE;

    LO_LockLayout();
    switch (slot) {
      case EMBED_ARRAY_LENGTH:
	active_layer_id = LM_GetActiveLayer(context);
	LM_SetActiveLayer(context, array->layer_id);
	count = LO_EnumerateEmbeds(context, array->layer_id);
	LM_SetActiveLayer(context, active_layer_id);
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
	embed_data = LO_GetEmbedByIndex(context, array->layer_id, (uint)slot);
	if (embed_data) {
            JSObject *mo = LM_ReflectEmbed(context, embed_data, NULL,
					   array->layer_id, (uint)slot);
            if (!mo) {
                JS_ReportError(cx,
                 "unable to reflect embed with index %d - not loaded yet?",
                 (uint) slot);
                goto err;
            }
            *vp = OBJECT_TO_JSVAL(mo);
            XP_ASSERT(slot < array->length);
	} else {
            JS_ReportError(cx, "no embed with index %d\n");
            goto err;
        }
	break;
    }
    LO_UnlockLayout();
    return JS_TRUE;
  err:
    LO_UnlockLayout();
    return JS_FALSE;
}

PR_STATIC_CALLBACK(void)
embed_array_finalize(JSContext *cx, JSObject *obj)
{
    JSObjectArray *array;

    array = JS_GetPrivate(cx, obj);
    if (!array)
	return;
    DROP_BACK_COUNT(array->decoder);
    JS_free(cx, array);
}

JSClass lm_embed_array_class = {
    "EmbedArray", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    embed_array_getProperty, embed_array_getProperty, JS_EnumerateStub,
    JS_ResolveStub, JS_ConvertStub, embed_array_finalize
};

JSObject *
lm_GetEmbedArray(MochaDecoder *decoder, JSObject *document)
{
    JSContext *cx = decoder->js_context;
    JSObject *obj;
    JSObjectArray *array;
    JSDocument *doc;

    doc = JS_GetPrivate(cx, document);
    if (!doc)
	return NULL;

    obj = doc->embeds;
    if (obj)
	return obj;

    array = JS_malloc(cx, sizeof *array);
    if (!array)
        return NULL;
    array->decoder = NULL;  /* in case of error below */

    obj = JS_NewObject(cx, &lm_embed_array_class, NULL, document);
    if (!obj || !JS_SetPrivate(cx, obj, array)) {
	JS_free(cx, array);
	return NULL;
    }

    if (!JS_DefineProperties(cx, obj, embed_array_props))
	return NULL;

    array->decoder = HOLD_BACK_COUNT(decoder);
    array->length = 0;
    array->layer_id = doc->layer_id;
    doc->embeds = obj;
    return obj;
}

/* this calls MozillaEmbedContext to reflect the embed by
 * calling into mocha... yow! */
static JSObject *
lm_ReallyReflectEmbed(MWContext *context, LO_EmbedStruct *lo_embed,
                      int32 layer_id, uint32 index)
{
    JSObject *obj;
    MochaDecoder *decoder;
    JSContext *cx;
    jref jembed;
    NPEmbeddedApp* embed;
    lo_TopState *top_state;
    PRHashTable *map;
    PR_LOG(Moja, debug, ("really reflect embed 0x%x\n", lo_embed));

    if ((obj = lo_embed->mocha_object) != NULL)
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
                             LM_GET_MAPPING_KEY(LM_EMBEDS, layer_id, index));
        if (obj) {
            lo_embed->mocha_object = obj;
            return obj;
        }
    }

    /* set the element to something bad if we can't get the java obj */
    if (!JSJ_IsEnabled()) {
        PR_LOG(Moja, debug, ("reflected embed 0x%x as null\n",
                             lo_embed));
        return lo_embed->mocha_object = lm_DummyObject;
    }

    embed = (NPEmbeddedApp*) lo_embed->FE_Data;
    if (embed) {
        np_data *ndata = (np_data*) embed->np_data;
        np_instance *instance;

        /* XXX this comes from npglue.c, it should be put
         * in one of the plugin header files */
        extern jref npn_getJavaPeer(NPP npp);

        if (!ndata || ndata->state == NPDataSaved) {
            PR_LOG(Moja, warn, ("embed 0x%x is missing or suspended\n",
                                lo_embed));
            return NULL;
        }
        instance = ndata->instance;
	if (!instance) return NULL;
        jembed = npn_getJavaPeer(instance->npp);

        obj = js_ReflectJObjectToJSObject(decoder->js_context,
				          (HObject *)jembed);
        PR_LOG(Moja, debug, ("reflected embed 0x%x (java 0x%x) to 0x%x ok\n",
                             lo_embed, jembed, obj));

        map = lm_GetIdToObjectMap(decoder);
        if (map)
            PR_HashTableAdd(map, 
                            LM_GET_MAPPING_KEY(LM_EMBEDS, layer_id, index),
                            obj);

        return lo_embed->mocha_object = obj;
    } else {
        PR_LOG(Moja, warn, ("failed to reflect embed 0x%x\n", lo_embed));
        return NULL;
    }
}


JSObject *
LM_ReflectEmbed(MWContext *context, LO_EmbedStruct *lo_embed,
                PA_Tag * tag, int32 layer_id, uint index)
{
    JSObject *obj, *array_obj, *outer_obj, *document;
    MochaDecoder *decoder;
    JSContext *cx;
    char *name;
    int i;

    obj = lo_embed->mocha_object;
    if (obj)
        return obj;

    decoder = LM_GetMochaDecoder(context);
    if (!decoder)
        return NULL;

    cx = decoder->js_context;

    /* get the name */
    name = 0;
    for (i = 0; i < lo_embed->attribute_cnt; i++) {
        if (!XP_STRCASECMP(lo_embed->attribute_list[i], "name")) {
            name = strdup(lo_embed->value_list[i]);
            break;
        }
    }

    /* Get the document object that will hold this applet */
    document = lm_GetDocumentFromLayerId(decoder, layer_id);
    if (!document) {
        LM_PutMochaDecoder(decoder);
        return NULL;
    }
    
    array_obj = lm_GetEmbedArray(decoder, document);
    if (!array_obj) {
        LM_PutMochaDecoder(decoder);
        return NULL;
    }

    /* XXX should pass thru ReallyReflectApplet to whatever calls NewObject */
    outer_obj = lm_GetOuterObject(decoder);

    /* this function does the real work */
    obj = lm_ReallyReflectEmbed(context, lo_embed, layer_id, index);

    if (!obj)
        goto out;

    /* put it in the embed array */
    if (!lm_AddObjectToArray(cx, array_obj, name, index, obj)) {
	obj = NULL;
	goto out;
    }

    /* put it in the document scope */
    if (name && !JS_DefineProperty(cx, outer_obj, name, OBJECT_TO_JSVAL(obj),
				   NULL, NULL,
				   JSPROP_ENUMERATE | JSPROP_READONLY)) {
	PR_LOG(Moja, warn, ("failed to define embed 0x%x as %s\n",
			    lo_embed, name));
	/* XXX remove it altogether? */
    }

    /* cache it in layout data structure */
    lo_embed->mocha_object = obj;

out:
    LM_PutMochaDecoder(decoder);
    return obj;
}

PRIVATE JSObject *
LM_ReflectNamedEmbed(MWContext *context, lo_NameList *name_rec, 
                     int32 layer_id, uint index)
{
    return NULL;
}

#endif /* JAVA */
