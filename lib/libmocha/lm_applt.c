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
 * JS reflection of applets in the current document
 */

/* Please leave outside of ifdef for windows precompiled headers */
#include "lm.h"

#ifdef JAVA

#include "xp.h"
#include "layout.h"
#include "java.h"
#include "lj.h"		/* for LJ_InvokeMethod */
#define IMPLEMENT_netscape_applet_MozillaAppletContext
#define IMPLEMENT_netscape_applet_EmbeddedObjectNatives
#ifdef XP_MAC
#include "n_applet_MozillaAppletContext.h"
#include "n_a_EmbeddedObjectNatives.h"
#else
#include "netscape_applet_MozillaAppletContext.h"
#include "netscape_applet_EmbeddedObjectNatives.h"
#endif
#include "jsjava.h"
#include "jsobj.h"
#include "prlog.h"

extern PRLogModuleInfo* Moja;
#define debug	PR_LOG_DEBUG
#define warn	PR_LOG_WARN

JSClass *lm_java_clasp = NULL;

enum applet_array_slot {
    APPLET_ARRAY_LENGTH = -1
};

static JSPropertySpec applet_array_props[] = {
    {lm_length_str, APPLET_ARRAY_LENGTH,
		    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {0}
};

extern JSClass lm_applet_array_class;

PR_STATIC_CALLBACK(JSBool)
applet_array_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSObjectArray *array;
    MochaDecoder *decoder;
    MWContext *context;
    jsint count, slot;
    LO_JavaAppStruct *applet_data;
    int32 active_layer_id;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    array = JS_GetInstancePrivate(cx, obj, &lm_applet_array_class, NULL);
    if (!array)
	return JS_TRUE;
    decoder = array->decoder;
    context = decoder->window_context;

    if (!context) 
	return JS_TRUE;

    if (LM_MOJA_OK != ET_InitMoja(context))
	return JS_FALSE;

    LO_LockLayout();

    switch (slot) {
      case APPLET_ARRAY_LENGTH:
	active_layer_id = LM_GetActiveLayer(context);
	LM_SetActiveLayer(context, array->layer_id);
	count = LO_EnumerateApplets(context, array->layer_id);
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
	applet_data = LO_GetAppletByIndex(context, array->layer_id, 
                                          (uint)slot);
	if (applet_data) {
            JSObject *mo = LM_ReflectApplet(context, applet_data, NULL,
					    array->layer_id, (uint)slot);

            if (!mo) {
                JS_ReportError(cx,
                 "unable to reflect applet \"%s\", index %d - not loaded yet?",
                 applet_data->attr_name, (uint) slot);
                goto err;
            }
            *vp = OBJECT_TO_JSVAL(mo);

            if (slot >= array->length)
                array->length = slot + 1;
	} else {
            JS_ReportError(cx, "no applet with index %d\n");
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
applet_array_finalize(JSContext *cx, JSObject *obj)
{
    JSObjectArray *array;

    array = JS_GetPrivate(cx, obj);
    if (!array)
	return;
    DROP_BACK_COUNT(array->decoder);
    JS_free(cx, array);
}

JSClass lm_applet_array_class = {
    "AppletArray", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    applet_array_getProperty, applet_array_getProperty, JS_EnumerateStub,
    JS_ResolveStub, JS_ConvertStub, applet_array_finalize
};

JSObject *
lm_GetAppletArray(MochaDecoder *decoder, JSObject *document)
{
    JSContext *cx = decoder->js_context;
    JSObject *obj;
    JSObjectArray *array;
    JSDocument *doc;

    doc = JS_GetPrivate(cx, document);
    if (!doc)
	return NULL;

    obj = doc->applets;
    if (obj)
	return obj;

    array = JS_malloc(cx, sizeof *array);
    if (!array)
        return NULL;
    array->decoder = NULL;  /* in case of error below */

    obj = JS_NewObject(cx, &lm_applet_array_class, NULL, document);
    if (!obj || !JS_SetPrivate(cx, obj, array)) {
	JS_free(cx, array);	
	return NULL;
    }

    if (!JS_DefineProperties(cx, obj, applet_array_props))
	return NULL;

    array->decoder = HOLD_BACK_COUNT(decoder);
    array->length = 0;
    array->layer_id = doc->layer_id;
    doc->applets = obj;

    return obj;
}

static JSObject *
lm_ReallyReflectApplet(MWContext *context, LO_JavaAppStruct *lo_applet,
                       int32 layer_id, uint32 index)
{
    JSObject *obj;
    MochaDecoder *decoder;
    JSContext *cx;
    LJAppletData *ad;
    jref japplet;
    lo_TopState *top_state;
    PRHashTable *map;

    PR_LOG(Moja, debug, ("really reflect applet 0x%x\n", lo_applet));

     obj = lo_applet->objTag.mocha_object;
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
                             LM_GET_MAPPING_KEY(LM_APPLETS, layer_id, index));

        if (obj) {
            lo_applet->objTag.mocha_object = obj;
            return obj;
        }
    }

    /* set the element to something bad if we can't get the java obj */
    if (!JSJ_IsEnabled()) {
        PR_LOG(Moja, debug, ("reflected applet 0x%x as null\n",
                             lo_applet));
        return lo_applet->objTag.mocha_object = lm_DummyObject;
    }

    ad = (LJAppletData *) lo_applet->objTag.session_data;

    if (ad) {
        /* MozillaAppletContext.reflectApplet gets the java applet
         * object out of a hash table given the AppletData pointer
         * as an integer */
	if (ad->selector_type != LO_JAVA_SELECTOR_APPLET)
	    japplet = LJ_InvokeMethod(classEmbeddedObjectNatives,
		methodID_netscape_applet_EmbeddedObjectNatives_reflectObject,
		ad->docID, ad);
	else
	    japplet = LJ_InvokeMethod(classMozillaAppletContext,
		methodID_netscape_applet_MozillaAppletContext_reflectApplet_1,
		ad->docID, ad);

        obj = js_ReflectJObjectToJSObject(decoder->js_context,
                                            (HObject *)japplet);

        map = lm_GetIdToObjectMap(decoder);
        if (map)
            PR_HashTableAdd(map, 
                            LM_GET_MAPPING_KEY(LM_APPLETS, layer_id, index),
                            obj);

        /*
          lj_mozilla_ee->js_context = saved_context;
          */

        PR_LOG(Moja, debug, ("reflected applet 0x%x (java 0x%x) to 0x%x ok\n",
                             lo_applet, japplet, obj));

        if (obj)
	    lm_java_clasp = JS_GetClass(obj);
	return lo_applet->objTag.mocha_object = obj;
    } else {
        PR_LOG(Moja, warn, ("failed to reflect applet 0x%x\n", lo_applet));
        return NULL;
    }
}


/* XXX what happens if this is called before java starts up?
 * or if java is disabled? */

JSObject *
LM_ReflectApplet(MWContext *context, LO_JavaAppStruct *applet_data,
                 PA_Tag * tag, int32 layer_id, uint index)
{
    JSObject *obj, *array_obj, *outer_obj, *document;
    MochaDecoder *decoder;
    JSContext *cx;
    char *name;

    obj = applet_data->objTag.mocha_object;
    if (obj)
        return obj;

    decoder = LM_GetMochaDecoder(context);
    if (!decoder)
        return NULL;
    cx = decoder->js_context;

    /* get the name */
    if (applet_data->attr_name) {
        name = JS_strdup(cx, (char *) applet_data->attr_name);
    } else {
        name = 0;
    }

    /* Get the document object that will hold this applet */
    document = lm_GetDocumentFromLayerId(decoder, layer_id);
    if (!document)
        goto out;

    array_obj = lm_GetAppletArray(decoder, document);
    if (!array_obj) {
        obj = 0;
        goto out;
    }

    /* XXX should pass thru ReallyReflectApplet to whatever calls NewObject */
    outer_obj = lm_GetOuterObject(decoder);

    /* this function does the real work */
    obj = lm_ReallyReflectApplet(context, applet_data, layer_id, index);

    if (!obj)
        /* reflection failed.  this usually means that javascript tried
         * to talk to the applet before it was ready.  there's not much
         * we can do about it */
        goto out;

    /* put it in the applet array */
    if (!lm_AddObjectToArray(cx, array_obj, name, index, obj)) {
	obj = NULL;
	goto out;
    }

    /* put it in the document scope */
    if (name) {
        if (!JS_DefineProperty(cx, outer_obj, name, OBJECT_TO_JSVAL(obj),
			       NULL, NULL, 
				JSPROP_ENUMERATE | JSPROP_READONLY)) {
            PR_LOG(Moja, warn, ("failed to define applet 0x%x as %s\n",
                                applet_data, name));
	    /* XXX remove it altogether? */
        }
	JS_free(cx, name);
    }

    /* cache it in layout data structure XXX lm_ReallyReflectApplet did this */
    XP_ASSERT(applet_data->objTag.mocha_object == obj);

  out:
    LM_PutMochaDecoder(decoder);
    return obj;
}

PRIVATE JSObject *
LM_ReflectNamedApplet(MWContext *context, lo_NameList *name_rec, 
                      int32 layer_id, uint index)
{
    return NULL;
}

#else

void *lm_java_clasp = NULL;

#endif /* JAVA */
