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
 * Compositor layer reflection and event notification
 *
 * Scott Furman, 6/20/96
 *
 */

#include "lm.h"  /* moved out of ifdef for pre compiled headers */
#include "xp.h"
#include "lo_ele.h"
#include "prtypes.h"
#include "pa_tags.h"
#include "layout.h"
#include "jsdbgapi.h"

#include "layers.h"

/* Forward declarations. Can these be static? */
PR_STATIC_CALLBACK(JSBool)
rect_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
PR_STATIC_CALLBACK(JSBool)
rect_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);


enum layer_array_slot {
    LAYER_ARRAY_LENGTH = -1
};

/* Native part of mocha object reflecting children layers of another layer */
typedef struct JSLayerArray {
    MochaDecoder        *decoder;   /* prefix must match JSObjectArray */
    jsint                length;    /* # of entries in array */
    int32                parent_layer_id;
} JSLayerArray;

extern JSClass lm_layer_array_class;

PR_STATIC_CALLBACK(JSBool)
layer_array_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSLayerArray *array;
    MochaDecoder *decoder;
    MWContext *context;
    jsint count, slot;
    CL_Layer *layer, *parent_layer;
    int32 layer_id;

    while (!(array = JS_GetInstancePrivate(cx, obj, &lm_layer_array_class, 
                                           NULL))) {
        obj = JS_GetPrototype(cx, obj);
        if (!obj)
            return JS_TRUE;
    }
    decoder = array->decoder;

    if (!lm_CheckContainerAccess(cx, obj, decoder, 
                                 JSTARGET_UNIVERSAL_BROWSER_READ)) {
        return JS_FALSE;
    }

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    context = decoder->window_context;
    if (!context) 
        return JS_TRUE;

    LO_LockLayout();

    if(decoder->doc_id != XP_DOCID(context)) {
        LO_UnlockLayout();
        return JS_FALSE;
    }

    parent_layer = LO_GetLayerFromId(context, array->parent_layer_id);
    if (!parent_layer) {
        LO_UnlockLayout();
        return JS_TRUE;
    }

    switch (slot) {
      case LAYER_ARRAY_LENGTH:
        count = CL_GetLayerChildCount(parent_layer);
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
        layer = CL_GetLayerChildByIndex(parent_layer, slot);
        if (!layer) {
            JS_ReportError(cx,
                           "Attempt to access nonexistent slot %d "
                           "of layers[] array", slot);
            LO_UnlockLayout();
            return JS_FALSE;
        }
        layer_id = LO_GetIdFromLayer(context, layer);
        *vp = OBJECT_TO_JSVAL(LM_ReflectLayer(context, layer_id, 
                                              array->parent_layer_id,
                                              NULL));
        break;
    }

    LO_UnlockLayout();
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
layer_array_resolve_name(JSContext *cx, JSObject *obj, jsval id)
{
    JSLayerArray *array;
    MochaDecoder *decoder;
    const char * name;
    JSObject *layer_obj;

    if (!JSVAL_IS_STRING(id))
        return JS_TRUE;

    name = JS_GetStringBytes(JSVAL_TO_STRING(id));
    if (!name)
        return JS_TRUE;

    array = JS_GetPrivate(cx, obj);
    if (!array)
        return JS_TRUE;
    decoder = array->decoder;

    /* 
     * If the layer exists, we don't have to define the property here,
     * since the reflection code will define it for us.
     */
    layer_obj = lm_GetNamedLayer(decoder, array->parent_layer_id, name);

    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
layer_array_finalize(JSContext *cx, JSObject *obj)
{
    JSLayerArray *array;

    array = JS_GetPrivate(cx, obj);
    if (!array)
        return;
    DROP_BACK_COUNT(array->decoder);
    JS_free(cx, array);
}

JSClass lm_layer_array_class = {
    "LayerArray", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    layer_array_getProperty, layer_array_getProperty, JS_EnumerateStub,
    layer_array_resolve_name, JS_ConvertStub, layer_array_finalize
};

static JSPropertySpec layer_array_props[] = {
    {lm_length_str, LAYER_ARRAY_LENGTH,
                    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {0}
};


/* Native part of a mocha object that reflects a single compositor layer
 */
typedef struct JSLayer {
    JSEventCapturer        capturer;
    MochaDecoder           *decoder;
    int32                   layer_id;
    JSString               *name;
    JSObject               *child_layers_array_obj;
    char                   *source_url;
    /* 
     * Indicates which properties have been modified and should be
     * saved across a resize relayout. 
     */
    uint32                  modification_mask; 
    /* 
     * The following are properties whose saved values are of a different
     * type than the property itself.
     */
    JSString               *sibling_above;
    JSString               *sibling_below;
    int32                   width;
    PRPackedBool            properties_locked;
    PRPackedBool            principals_compromised;
    JSPrincipals           *principals;
} JSLayer;


static JSObject *
reflect_layer_array(MochaDecoder *decoder,
                    int32 parent_layer_id)
{
    JSContext *cx;
    JSLayer *js_layer_parent;
    JSLayerArray *array;
    JSObject *obj, *layer_obj, *parent_doc_obj;
    JSClass *clasp;
    CL_Layer *parent_layer;

    cx = decoder->js_context;
    layer_obj = LO_GetLayerMochaObjectFromId(decoder->window_context, 
                                             parent_layer_id);
    parent_layer = LO_GetLayerFromId(decoder->window_context, parent_layer_id);
    parent_doc_obj = lm_GetDocumentFromLayerId(decoder, parent_layer_id);
    if (! layer_obj || !parent_layer || !parent_doc_obj) /* paranoia */
        return NULL;
    js_layer_parent = JS_GetPrivate(cx, layer_obj);
    if (!js_layer_parent)
        return NULL;
    obj = js_layer_parent->child_layers_array_obj;

    if (obj)                    /* Are layer children already reflected ? */
        return obj;
    
    clasp = &lm_layer_array_class;
    
    array = JS_malloc(cx, sizeof *array);
    if (!array)
        return NULL;
    XP_BZERO(array, sizeof *array);
    
    obj = JS_NewObject(cx, clasp, NULL, parent_doc_obj);
    if (!obj || !JS_SetPrivate(cx, obj, array)) {
        JS_free(cx, array);
        return NULL;
    }

    if (!JS_DefineProperties(cx, obj, layer_array_props))
        return NULL;

    array->decoder = HOLD_BACK_COUNT(decoder);
    array->parent_layer_id = parent_layer_id;
    return obj;
}

/* Given the mocha object reflecting a compositor layer, return
   the mocha object that reflects its child layers in an array. */
PRIVATE JSObject *
lm_GetLayerArray(MochaDecoder *decoder,
                 JSObject  *parent_js_layer_obj)
{
    JSLayer *js_layer_parent;
    JSObject *obj;
    JSContext *cx;
    
    if (! parent_js_layer_obj)
        return NULL;
    cx = decoder->js_context;
    js_layer_parent = JS_GetPrivate(cx, parent_js_layer_obj);
    if (!js_layer_parent)
        return NULL;
    obj = js_layer_parent->child_layers_array_obj;
    if (obj)
        return obj;

    obj = reflect_layer_array(decoder, js_layer_parent->layer_id);

    if (obj && !JS_DefineProperty(cx, parent_js_layer_obj, "layers", 
				  OBJECT_TO_JSVAL(obj), NULL, NULL,
				  JSPROP_ENUMERATE|JSPROP_READONLY|
                                  JSPROP_PERMANENT)) {
	return NULL;
    }

    js_layer_parent->child_layers_array_obj = obj;
    return obj;
}


/* The top-level document object contains the distinguished _DOCUMENT
   layer.  Reflect the array containing children of the _DOCUMENT layer. */
JSObject *
lm_GetDocumentLayerArray(MochaDecoder *decoder, JSObject *document)
{
    MWContext *context;
    JSObject *obj;
    JSLayerArray *array;
    JSClass *clasp;
    JSContext *cx;
    JSDocument *doc;

    cx = decoder->js_context;
    doc = JS_GetPrivate(cx, document);
    if (!doc)
        return NULL;

    obj = doc->layers;
    if (obj)
        return obj;
    context = decoder->window_context;

    /* If this is a layer's document, return the layer's child array */
    if (doc->layer_id != LO_DOCUMENT_LAYER_ID) {
        JSObject *layer_obj;
        
        layer_obj = LO_GetLayerMochaObjectFromId(context, doc->layer_id);
        if (!layer_obj)
            return NULL;
        doc->layers = lm_GetLayerArray(decoder, layer_obj);
        
        return doc->layers;
    }
    
    clasp = &lm_layer_array_class;
    
    array = JS_malloc(cx, sizeof *array);
    if (!array)
        return NULL;
    XP_BZERO(array, sizeof *array);
    
    obj = JS_NewObject(cx, clasp, NULL, document);
    if (!obj || !JS_SetPrivate(cx, obj, array)) {
        JS_free(cx, array);
        return NULL;
    }

    if (!JS_DefineProperties(cx, obj, layer_array_props)) {
        JS_free(cx, array);
        return NULL;
    }

    array->decoder = HOLD_BACK_COUNT(decoder);
    array->parent_layer_id = doc->layer_id;

    doc->layers = obj;
    return obj;
}

#define LM_SET_LAYER_MODIFICATION(layer, bit)        \
          (layer)->modification_mask |= (1 << (bit))
#define LM_CLEAR_LAYER_MODIFICATION(layer, bit)      \
          (layer)->modification_mask &= ~(1 << (bit))
#define LM_CHECK_LAYER_MODIFICATION(layer, bit)      \
          (((layer)->modification_mask & (1 << (bit))) != 0)

enum layer_prop_modification_bits {
    LAYER_MOD_LEFT = 0,
    LAYER_MOD_TOP,
    LAYER_MOD_VISIBILITY,
    LAYER_MOD_SRC,
    LAYER_MOD_ZINDEX,
    LAYER_MOD_BGCOLOR,
    LAYER_MOD_BACKGROUND,
    LAYER_MOD_PARENT,
    LAYER_MOD_SIB_ABOVE,
    LAYER_MOD_SIB_BELOW,
    LAYER_MOD_CLIP_LEFT,
    LAYER_MOD_CLIP_RIGHT,
    LAYER_MOD_CLIP_TOP,
    LAYER_MOD_CLIP_BOTTOM,
    LAYER_MOD_WIDTH
};

/* Static compositor layer property slots */
enum layer_slot {
    LAYER_WINDOW            = -1,
    LAYER_NAME              = -2,
    LAYER_LEFT              = -3,
    LAYER_TOP               = -4,
    LAYER_X                 = -5,
    LAYER_Y                 = -6,
    LAYER_HIDDEN            = -7,
    LAYER_SIB_ABOVE         = -8,
    LAYER_SIB_BELOW         = -9,
    LAYER_PARENT            = -10,
    LAYER_CHILDREN          = -11,
    LAYER_SRC               = -12,
    LAYER_VISIBILITY        = -13,
    LAYER_ABOVE             = -14,
    LAYER_BELOW             = -15,
    LAYER_ZINDEX            = -16,
    LAYER_BGCOLOR           = -17
};

char lm_left_str[] = "left";
char lm_top_str[] = "top";
char lm_right_str[] = "right";
char lm_bottom_str[] = "bottom";
char lm_src_str[] = "src";
char lm_visibility_str[] = "visibility";
char lm_zindex_str[] = "zIndex";
char lm_bgcolor_str[] = "bgColor";
char lm_background_str[] = "background";
char lm_clip_str[] = "clip";

/* Static compositor layer properties */
static JSPropertySpec layer_props[] = {
    {"window",              LAYER_WINDOW,       JSPROP_ENUMERATE | JSPROP_READONLY},
    {"id",                  LAYER_NAME,         JSPROP_ENUMERATE | JSPROP_READONLY},
    {"name",                LAYER_NAME,         JSPROP_ENUMERATE},
    {lm_left_str,           LAYER_LEFT,         JSPROP_ENUMERATE},
    {"x",                   LAYER_LEFT,         JSPROP_ENUMERATE}, /* Synonym for left */
    {lm_top_str,            LAYER_TOP,          JSPROP_ENUMERATE},
    {"y",                   LAYER_TOP,          JSPROP_ENUMERATE}, /* Synonym for top */
    {"pageX",               LAYER_X,            JSPROP_ENUMERATE},
    {"pageY",               LAYER_Y,            JSPROP_ENUMERATE},
    {"hidden",              LAYER_HIDDEN,       JSPROP_ENUMERATE},
    {"layers",              LAYER_CHILDREN,     JSPROP_ENUMERATE | JSPROP_READONLY},
    {"siblingAbove",        LAYER_SIB_ABOVE,    JSPROP_ENUMERATE | JSPROP_READONLY}, /* FIXME - should be writeable */
    {"siblingBelow",        LAYER_SIB_BELOW,    JSPROP_ENUMERATE | JSPROP_READONLY}, /* FIXME - should be writeable */
    {lm_parentLayer_str,    LAYER_PARENT,       JSPROP_ENUMERATE | JSPROP_READONLY},
    {lm_src_str,            LAYER_SRC,          JSPROP_ENUMERATE},
    {lm_visibility_str,     LAYER_VISIBILITY,   JSPROP_ENUMERATE},
    {"above",               LAYER_ABOVE,        JSPROP_ENUMERATE | JSPROP_READONLY},
    {"below",               LAYER_BELOW,        JSPROP_ENUMERATE | JSPROP_READONLY},
    {lm_zindex_str,         LAYER_ZINDEX,       JSPROP_ENUMERATE},
    {lm_bgcolor_str,        LAYER_BGCOLOR,      JSPROP_ENUMERATE},
    {0}
};

/* 
 * Static compositor rect property slots. Declared here since we 
 * need the ids in some of the methods.
 */
enum rect_slot {
    RECT_LEFT              = -1,
    RECT_TOP               = -2,
    RECT_RIGHT             = -3,
    RECT_BOTTOM            = -4,
    RECT_WIDTH             = -5,
    RECT_HEIGHT            = -6
};

extern JSClass lm_layer_class;

PR_STATIC_CALLBACK(JSBool)
layer_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSLayer *js_layer;
    MochaDecoder *decoder;
    MWContext *context;
    JSString *str;
    CL_Layer *layer, *layer_above, *layer_below, *layer_parent;
    LO_Color *bg_color;
    jsint slot;
    char *visibility;
    uint32 flags;

    while (!(js_layer = JS_GetInstancePrivate(cx, obj, &lm_layer_class, NULL))) {
        obj = JS_GetPrototype(cx, obj);
        if (!obj)
            return JS_TRUE;
    }

    decoder = js_layer->decoder;

    if (!lm_CheckContainerAccess(cx, obj, decoder, 
                                 JSTARGET_UNIVERSAL_BROWSER_READ)) {
        return JS_FALSE;
    }

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    context = decoder->window_context;
    if (!context)
        return JS_TRUE;

    /*
     * Not sure if this is enough protection or not...
     */
    LO_LockLayout();

    if(decoder->doc_id != XP_DOCID(context)) {
        LO_UnlockLayout();
        return JS_FALSE;
    }

    layer = LO_GetLayerFromId(context, js_layer->layer_id);
    if (!layer) {
        LO_UnlockLayout();
        return JS_TRUE;
    }
    
    switch (slot) {
    case LAYER_WINDOW:
        *vp = OBJECT_TO_JSVAL(decoder->window_object);
        break;

    case LAYER_NAME:
        if (js_layer->name)
            *vp = STRING_TO_JSVAL(js_layer->name);
        else
            *vp = JSVAL_NULL;
        break;

    case LAYER_HIDDEN:
        *vp = BOOLEAN_TO_JSVAL(CL_GetLayerHidden(layer));
        break;

    case LAYER_VISIBILITY:
        flags = CL_GetLayerFlags(layer);
        if (flags & CL_HIDDEN)
            visibility = "hide";
        else if (flags & CL_OVERRIDE_INHERIT_VISIBILITY)
            visibility = "show";
        else
            visibility = "inherit";
        
        str = JS_NewStringCopyZ(cx, visibility);
        if (!str) {
            LO_UnlockLayout();
            return JS_FALSE;
        }
        *vp = STRING_TO_JSVAL(str);
        break;

    case LAYER_LEFT:
      *vp = INT_TO_JSVAL(LO_GetLayerXOffset(layer));
      break;

    case LAYER_TOP:
      *vp = INT_TO_JSVAL(LO_GetLayerYOffset(layer));
      break;

    case LAYER_X:
      *vp = INT_TO_JSVAL(CL_GetLayerXOrigin(layer));
      break;

    case LAYER_Y:
      *vp = INT_TO_JSVAL(CL_GetLayerYOrigin(layer));
      break;

    case LAYER_CHILDREN: 
        *vp = OBJECT_TO_JSVAL(lm_GetLayerArray(js_layer->decoder, obj));
        break;
        
    case LAYER_SIB_ABOVE:
        layer_above = CL_GetLayerSiblingAbove(layer);
        if (layer_above)
            *vp = OBJECT_TO_JSVAL(LO_GetLayerMochaObjectFromLayer(context, 
                                                               layer_above));
        else
            *vp = JSVAL_NULL;
        break;

    case LAYER_SIB_BELOW:
        layer_below = CL_GetLayerSiblingBelow(layer);
        if (layer_below) 
            *vp = OBJECT_TO_JSVAL(LO_GetLayerMochaObjectFromLayer(context, 
                                                               layer_below));
        else
            *vp = JSVAL_NULL;
        break;

    case LAYER_PARENT:
        layer_parent = CL_GetLayerParent(layer);

        /* 
         * XXX This is a bit controversial - should the parent layer of
         * a top-level layer be the window??
         */
        if (layer_parent) {
            if (CL_GetLayerFlags(layer_parent) & CL_DONT_ENUMERATE)
                *vp = OBJECT_TO_JSVAL(decoder->window_object);
            else
                *vp = OBJECT_TO_JSVAL(LO_GetLayerMochaObjectFromLayer(context, 
                                                                layer_parent));
        }
        else
            *vp = JSVAL_NULL;
        break;

    case LAYER_ZINDEX:
        *vp = INT_TO_JSVAL(CL_GetLayerZIndex(layer));
      break;

    case LAYER_ABOVE:
        layer_above = CL_GetLayerAbove(layer);
        if (layer_above) 
            *vp = OBJECT_TO_JSVAL(LO_GetLayerMochaObjectFromLayer(context, 
                                                               layer_above));
        else
            *vp = JSVAL_NULL;
        break;

    case LAYER_BELOW:
        layer_below = CL_GetLayerBelow(layer);
        if (layer_below) 
            *vp = OBJECT_TO_JSVAL(LO_GetLayerMochaObjectFromLayer(context, 
                                                               layer_below));
        else
            *vp = JSVAL_NULL;
        break;

    case LAYER_BGCOLOR:
        bg_color = LO_GetLayerBgColor(layer);
        if (bg_color) {
            uint32 packed_color =
                (bg_color->red << 16) | (bg_color->green << 8) | (bg_color->blue);
            *vp = INT_TO_JSVAL(packed_color);
        } else
            *vp = JSVAL_NULL;
        break;
        
    case LAYER_SRC:
        if (!lm_CheckPermissions(cx, obj, JSTARGET_UNIVERSAL_BROWSER_READ))
	    return JS_FALSE;
	if (!js_layer->source_url) {
            *vp = JSVAL_NULL;
        } else {
            JSString *url;
            
            url = JS_NewStringCopyZ(cx, js_layer->source_url);
            if (!url) {
                LO_UnlockLayout();
                return JS_FALSE;
            }
            *vp = STRING_TO_JSVAL(url);
        }
        break;
    default:
        /* Don't mess with a user-defined or method property. */
        break;
    }

    LO_UnlockLayout();

    return JS_TRUE;
}

JSBool
lm_jsval_to_rgb(JSContext *cx, jsval *vp, LO_Color **rgbp)
{
    LO_Color *rgb = NULL;
    int32 color;

    if (JSVAL_IS_NUMBER(*vp)) {
        if (!JS_ValueToInt32(cx, *vp, &color))
            return JS_FALSE;
        if ((color >> 24) != 0)
            return JS_FALSE;

        rgb = XP_NEW(LO_Color);
        if (!rgb)
            return JS_FALSE;

        rgb->red = (uint8) (color >> 16);
        rgb->green = (uint8) ((color >> 8) & 0xff);
        rgb->blue = (uint8) (color & 0xff);
    } else {
        switch(JS_TypeOfValue(cx, *vp)) {

        case JSTYPE_OBJECT:
            /* Check for null (transparent) bgcolor */
            if (JSVAL_IS_NULL(*vp)) {
                rgb = NULL;
                break;
            }
            /* FALL THROUGH */
            
        default:
            if (!JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp))
                return JS_FALSE;
            /* FALL THROUGH */

        case JSTYPE_STRING:
            rgb = XP_NEW(LO_Color);
            if (!rgb)
                return JS_FALSE;

            if (!LO_ParseRGB((char *)JS_GetStringBytes(JSVAL_TO_STRING(*vp)),
                             &rgb->red, &rgb->green, &rgb->blue)) {
		JS_ReportError(cx, "Invalid color specification %s",
			       (char *)JS_GetStringBytes(JSVAL_TO_STRING(*vp)));
		XP_FREE(rgb);
		return JS_FALSE;
            }
            break;
        }
    }
    *rgbp = rgb;
    return JS_TRUE;
    
}


PR_STATIC_CALLBACK(JSBool)
layer_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    int32 val;
    int32 val32;
    JSBool hidden;
    PRBool properties_locked;
    JSLayer *js_layer;
    MochaDecoder *decoder;
    MWContext *context;
    CL_Layer *layer, *parent;
    char *url;
    LO_Color *rgb;
    jsint slot;
    jsval js_val;
    const char *referer;
    JSBool unlockp = JS_TRUE;

    while (!(js_layer = JS_GetInstancePrivate(cx, obj, &lm_layer_class, NULL))) {
        obj = JS_GetPrototype(cx, obj);
        if (!obj)
            return JS_TRUE;
    }
    properties_locked = (PRBool)js_layer->properties_locked;

    decoder = js_layer->decoder;

    if (!lm_CheckContainerAccess(cx, obj, decoder, 
                                 JSTARGET_UNIVERSAL_BROWSER_WRITE)) {
        return JS_FALSE;
    }

    context = decoder->window_context;
    if (!context) 
        return JS_TRUE;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    LO_LockLayout();
    if(decoder->doc_id != XP_DOCID(context)) {
        LO_UnlockLayout();
        return JS_FALSE;
    }

    layer = LO_GetLayerFromId(context, js_layer->layer_id);
    if (!layer) {
        LO_UnlockLayout();
        return JS_TRUE;
    }

    /* If a layer is dynamically changing, it will probably look
       better if it utilizes offscreen compositing. */
    CL_ChangeLayerFlag(layer, CL_PREFER_DRAW_OFFSCREEN, PR_TRUE);

    switch (slot) {
    case LAYER_HIDDEN:
        if (!JS_ValueToBoolean(cx, *vp, &hidden))
            goto error_exit;
        js_val = BOOLEAN_TO_JSVAL(!hidden);
        JS_SetProperty(cx, obj, lm_visibility_str, &js_val);
        break;
        
    case LAYER_VISIBILITY:
    {
        JSBool hidden, inherit;

        if (JSVAL_IS_BOOLEAN(*vp)) {
            hidden = (JSBool)(!JSVAL_TO_BOOLEAN(*vp));
            CL_ChangeLayerFlag(layer, CL_HIDDEN, (PRBool)hidden);
        } else {
            JSString *str;
            const char *visibility;
            
            if (!(str = JS_ValueToString(cx, *vp)))
                goto error_exit;
            visibility = JS_GetStringBytes(str);
            /* Accept "hidden" or "hide" */
            hidden = (JSBool)(!XP_STRNCASECMP(visibility, "hid", 3));
            inherit = (JSBool)(!XP_STRCASECMP(visibility, "inherit"));

            if (!hidden && !inherit &&
                XP_STRCASECMP(visibility, "show") &&
                XP_STRCASECMP(visibility, "visible")) {
                JS_ReportError(cx,
                               "Layer visibility property must be set to "
                               "one of 'hide', 'show' or 'inherit'");
            }
            CL_ChangeLayerFlag(layer, CL_HIDDEN, (PRBool)hidden);
            CL_ChangeLayerFlag(layer, CL_OVERRIDE_INHERIT_VISIBILITY,
                               (PRBool)!inherit);
        }
        LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_VISIBILITY);
        break;
    }

    case LAYER_LEFT:
        /* Some layers have mostly read-only properties because we
           don't want a malicious/careless JS author to modify them,
           e.g.  for layers that are used to encapulate a mail
           message. In such cases, we fail silently. */
        if (properties_locked)
            break;
        if (!JS_ValueToInt32(cx, *vp, &val))
            goto error_exit;
        LO_MoveLayer(layer, (int32)val, LO_GetLayerYOffset(layer));
        LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_LEFT);
        break;
            
    case LAYER_TOP:
        /* Some layers have mostly read-only properties because we
           don't want a malicious/careless JS author to modify them,
           e.g.  for layers that are used to encapulate a mail
           message. In such cases, we fail silently. */
        if (properties_locked)
            break;
        if (!JS_ValueToInt32(cx, *vp, &val))
            goto error_exit;
        LO_MoveLayer(layer, LO_GetLayerXOffset(layer), (int32)val);
        LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_TOP);
        break;

    case LAYER_X:
        /* Some layers have mostly read-only properties because we
           don't want a malicious/careless JS author to modify them,
           e.g.  for layers that are used to encapulate a mail
           message. In such cases, we fail silently. */
        if (properties_locked)
            break;
        if (!JS_ValueToInt32(cx, *vp, &val32))
            goto error_exit;
        parent = CL_GetLayerParent(layer);
        XP_ASSERT(parent);
        if (parent)
            val32 -= CL_GetLayerXOrigin(parent);
        js_val = INT_TO_JSVAL(val32);
        JS_SetProperty(cx, obj, lm_left_str, &js_val);
        break;
                           
    case LAYER_Y:
        /* Some layers have mostly read-only properties because we
           don't want a malicious/careless JS author to modify them,
           e.g.  for layers that are used to encapulate a mail
           message. In such cases, we fail silently. */
        if (properties_locked)
            break;
        if (!JS_ValueToInt32(cx, *vp, &val32))
            goto error_exit;
        parent = CL_GetLayerParent(layer);
        XP_ASSERT(parent);
        if (parent)
            val32 -= CL_GetLayerYOrigin(parent);
        js_val = INT_TO_JSVAL(val32);
        JS_SetProperty(cx, obj, lm_top_str, &js_val);
        break;

    case LAYER_SRC:
        /* Some layers have mostly read-only properties because we
           don't want a malicious/careless JS author to modify them,
           e.g.  for layers that are used to encapulate a mail
           message. In such cases, we fail silently. */
        if (properties_locked)
            break;
        if (!JSVAL_IS_STRING(*vp) &&
            !JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp)) {
            goto error_exit;
        }
        
        url = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
        LO_UnlockLayout();

        url = (char *)lm_CheckURL(cx, url, JS_TRUE);
        if (! url)
	    return JS_FALSE;

	referer = lm_GetSubjectOriginURL(cx);
	if (! referer) {
            XP_FREE(url);
	    return JS_FALSE;
	}

	if (ET_TweakLayer(decoder->window_context, layer, 0, 0,
			  (void *)url, js_layer->layer_id, 
			  CL_SetSrc, referer, decoder->doc_id)) {
	    lm_NewLayerDocument(decoder, js_layer->layer_id);
	    XP_FREEIF(js_layer->source_url);
	    js_layer->source_url = url;
	    if (js_layer->principals) {
		JSPRINCIPALS_DROP(cx, js_layer->principals);
	    }
	    js_layer->principals = LM_NewJSPrincipals(NULL, NULL, url);
	    if (js_layer->principals == NULL) {
		XP_FREE(url);
		return JS_FALSE;
	    }
	    JSPRINCIPALS_HOLD(cx, js_layer->principals);
	    LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_SRC);
	    decoder->stream_owner = js_layer->layer_id;
	}
	else {
            XP_FREE(url);
	}

        /* Note that the name will be deallocated at the other end
           of this call. */

	/* Return true here to avoid passing through the getter and 
	 * hitting the additional security checks there.  Return
	 * value has already been stringized here. */
	return JS_TRUE;

        break;

    case LAYER_ZINDEX:
        if (!JS_ValueToInt32(cx, *vp, &val))
            goto error_exit;
        parent = CL_GetLayerParent(layer);
        CL_RemoveChild(parent, layer);
        CL_InsertChildByZ(parent, layer, (int32)val);
        LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_ZINDEX);
        LM_CLEAR_LAYER_MODIFICATION(js_layer, LAYER_MOD_SIB_ABOVE);
        LM_CLEAR_LAYER_MODIFICATION(js_layer, LAYER_MOD_SIB_BELOW);        
        break;

    case LAYER_BGCOLOR:
        LO_UnlockLayout();
        unlockp = JS_FALSE;
        if (!lm_jsval_to_rgb(cx, vp, &rgb))
            return JS_FALSE;
        ET_TweakLayer(decoder->window_context, layer, 0, 0,
                      rgb, 0, CL_SetBgColor, NULL, decoder->doc_id);
        LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_BGCOLOR);
        break;

    case LAYER_SIB_ABOVE:
    case LAYER_SIB_BELOW:
        /* FIXME - these should be writeable */
        break;

    /* These are immutable. */
    case LAYER_NAME:
    case LAYER_CHILDREN:
        break;

    default:
        break;
    }

    if (unlockp)
        LO_UnlockLayout();
    return layer_getProperty(cx, obj, id, vp);

  error_exit:
    LO_UnlockLayout();
    return JS_FALSE;
}

JSBool
layer_setBgColorProperty(JSContext *cx, JSObject *obj, jsval *vp)
{
    return layer_setProperty(cx, obj, INT_TO_JSVAL(LAYER_BGCOLOR), vp);
}

/* Lazily synthesize an Image object for the layer's 'background' property */
PR_STATIC_CALLBACK(JSBool)
layer_resolve_name(JSContext *cx, JSObject *obj, jsval id)
{
    JSLayer *js_layer;
    MochaDecoder *decoder;
    const char * name;

    js_layer = JS_GetPrivate(cx, obj);
    if (!js_layer)
        return JS_TRUE;
    decoder = js_layer->decoder;

    if (!JSVAL_IS_STRING(id))
        return JS_TRUE;
    name = JS_GetStringBytes(JSVAL_TO_STRING(id));

    if (!XP_STRCMP(name, lm_background_str)) {
        JSObject *image_obj;
        CL_Layer *layer;
        
        layer = LO_GetLayerFromId(decoder->window_context, js_layer->layer_id);
        if (!layer)
            return JS_FALSE;
        image_obj = lm_NewImage(cx, LO_GetLayerBackdropImage(layer));
        if (!image_obj)
            return JS_FALSE;
        
        return JS_DefineProperty(cx, obj, name, OBJECT_TO_JSVAL(image_obj),
                                 NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
    }
    return lm_ResolveWindowProps(cx, decoder, obj, id);
}

PR_STATIC_CALLBACK(void)
layer_finalize(JSContext *cx, JSObject *obj)
{
    JSLayer *js_layer;
	PRHashTable *map;

    js_layer = JS_GetPrivate(cx, obj);
    if (!js_layer)
        return;

/* XXX js_layer->layer is sometimes freed by the time the GC runs
    CL_SetLayerMochaObject(js_layer->layer, NULL);
 */
    DROP_BACK_COUNT(js_layer->decoder);
    JS_UnlockGCThing(cx, js_layer->name);

	map = lm_GetIdToObjectMap(js_layer->decoder);
    if (map)
        PR_HashTableRemove(map, LM_GET_MAPPING_KEY(LM_LAYERS, 0, js_layer->layer_id));

    JS_RemoveRoot(cx, &js_layer->sibling_above);
    JS_RemoveRoot(cx, &js_layer->sibling_below);
    XP_FREEIF(js_layer->source_url);
    if (js_layer->principals) {
        JSPRINCIPALS_DROP(cx, js_layer->principals);
    }
    JS_free(cx, js_layer);
}

JSBool layer_check_access(JSContext *cx, JSObject *obj, jsval id,
                          JSAccessMode mode, jsval *vp)
{
    if(mode == JSACC_PARENT)  {
        return lm_CheckSetParentSlot(cx, obj, id, vp);
    }
    return JS_TRUE;
}

JSClass lm_layer_class = {
    "Layer", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, layer_getProperty, layer_setProperty,
    JS_EnumerateStub, layer_resolve_name, JS_ConvertStub, layer_finalize,
    NULL, layer_check_access
};

/* JS native method:

   Translate layer to given XY coordinates, e.g.
   document.layers[0].moveto(1, 3); */
static JSBool
move_layer(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval, PRBool is_absolute)
{
    int32 x, y;
    JSLayer *js_layer;
    MochaDecoder *decoder;
    jsval val;
    CL_Layer *layer;
    
    if (!(js_layer = JS_GetInstancePrivate(cx, obj, &lm_layer_class, argv)))
        return JS_FALSE;

    /* Some layers have mostly read-only properties because we
       don't want a malicious/careless JS author to modify them,
       e.g.  for layers that are used to encapulate a mail
       message. In such cases, we fail silently. */
    if (js_layer->properties_locked)
        return JS_TRUE;
    
    decoder = js_layer->decoder;

    LO_LockLayout();
    if(!decoder->window_context || 
	decoder->doc_id != XP_DOCID(decoder->window_context)) {
        LO_UnlockLayout();
        return JS_FALSE;
    }
    
    layer = LO_GetLayerFromId(decoder->window_context, js_layer->layer_id);
    if (!layer) 
        goto error_exit;
    if (argc != 2) {
        JS_ReportError(cx, lm_argc_err_str);
        goto error_exit;
    }
    if (!JS_ValueToInt32(cx, argv[0], &x) ||
        !JS_ValueToInt32(cx, argv[1], &y)) {
        goto error_exit;
    }

    if (is_absolute) {
        CL_Layer *parent_layer = CL_GetLayerParent(layer);
        x -= CL_GetLayerXOrigin(parent_layer);
        y -= CL_GetLayerYOrigin(parent_layer);
    }

    /* If a layer is moving, it will probably look better if it
       utilizes offscreen compositing. */
    CL_ChangeLayerFlag(layer, CL_PREFER_DRAW_OFFSCREEN, PR_TRUE);
    LO_MoveLayer(layer, x, y);

    /*
     * Record that we've side-effected left and top. We do a DefineProperty
     * to mutate the property and ensure that we have a unique slot per-object.
     */
    if (!LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_LEFT)) {
        JS_DefinePropertyWithTinyId(cx, obj, lm_left_str, LAYER_LEFT,
                                    INT_TO_JSVAL(x), layer_getProperty, 
                                    layer_setProperty, JSPROP_ENUMERATE);
        LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_LEFT);
    }
    else
        JS_GetProperty(cx, obj, lm_left_str, &val);
    if (!LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_TOP)) {
        LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_TOP);
        JS_DefinePropertyWithTinyId(cx, obj, lm_top_str, LAYER_TOP,
                                    INT_TO_JSVAL(y), layer_getProperty, 
                                    layer_setProperty, JSPROP_ENUMERATE);
    }
    else
        JS_GetProperty(cx, obj, lm_top_str, &val);

    LO_UnlockLayout();
    return JS_TRUE;

  error_exit:
    LO_UnlockLayout();
    return JS_FALSE;
}

/* JS native method:

   Translate layer to given XY coordinates relative to BODY document origin, e.g.
   document.layers[0].moveto(1, 3); */
PR_STATIC_CALLBACK(JSBool)
move_layer_abs(JSContext *cx, JSObject *obj,
               uint argc, jsval *argv, jsval *rval)
{
    return move_layer(cx, obj, argc, argv, rval, PR_TRUE);
}

/* JS native method:

   Translate layer to given XY coordinates relative to parent_layer, e.g.
   document.layers[0].moveto(1, 3); */
PR_STATIC_CALLBACK(JSBool)
move_layer_rel(JSContext *cx, JSObject *obj,
               uint argc, jsval *argv, jsval *rval)
{
    return move_layer(cx, obj, argc, argv, rval, PR_FALSE);
}

/* JS native method:

   Stack layer above argument layer, e.g.
   document.layers[0].moveAbove(document.layers[2]); */
static JSBool
change_layer_stacking(JSContext *cx, JSObject *obj,
                      uint argc, jsval *argv, jsval *rval,
                      CL_LayerPosition position)
{
    int32 sibling_parent_layer_id;
    JSLayer *js_layer, *sibling_js_layer;
    JSObject *sibling_obj;
    MochaDecoder *decoder;
    CL_Layer *layer, *sibling_layer, *parent, *sibling_layer_parent;
    char *new_sibling_name;

    if (!(js_layer = JS_GetInstancePrivate(cx, obj, &lm_layer_class, argv)))
        return JS_FALSE;
    decoder = js_layer->decoder;

    if (argc != 1) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }
    if (!JS_ValueToObject(cx, argv[0], &sibling_obj))
        return JS_FALSE;

    /* no-op */
    if (!sibling_obj)
        return JS_TRUE;

    /* could be an object of another class. */
    if (!JS_InstanceOf(cx, sibling_obj, &lm_layer_class, argv))
        return JS_FALSE;

    sibling_js_layer = JS_GetPrivate(cx, sibling_obj);
    if (!sibling_js_layer)
        return JS_TRUE;

    LO_LockLayout();
    if(decoder->doc_id != XP_DOCID(decoder->window_context)) {
        LO_UnlockLayout();
        return JS_FALSE;
    }
    
    layer = LO_GetLayerFromId(decoder->window_context, js_layer->layer_id);
    sibling_layer = LO_GetLayerFromId(decoder->window_context, 
                                      sibling_js_layer->layer_id);
    parent = CL_GetLayerParent(layer);
    sibling_layer_parent = CL_GetLayerParent(sibling_layer);

    sibling_parent_layer_id = LO_GetIdFromLayer(decoder->window_context,
                                                sibling_layer_parent);
    if (IS_MESSAGE_WINDOW(decoder->window_context) &&
        (sibling_parent_layer_id == LO_DOCUMENT_LAYER_ID)) {
        LO_UnlockLayout();
        JS_ReportError(cx, 
                       "Disallowed attempt to manipulate top-level layer"
                       " in a message window");
        return JS_FALSE;
    }

    if (layer == sibling_layer) {
        LO_UnlockLayout();
        JS_ReportError(cx, "Cannot stack a Layer above or beneath itself");
        return JS_FALSE;
    }

    /* It shouldn't be possible to be passed the 
       compositor's root layer. */
    XP_ASSERT(parent);
    XP_ASSERT(sibling_layer_parent);
    if (!sibling_layer_parent || !parent) {
        LO_UnlockLayout();
        return JS_FALSE;
    }
            
    /* If a layer is dynamically changing, it will probably look
       better if it utilizes offscreen compositing. */
    CL_ChangeLayerFlag(layer, CL_PREFER_DRAW_OFFSCREEN, PR_TRUE);

    CL_RemoveChild(parent, layer);
    CL_InsertChild(sibling_layer_parent, layer, 
                   sibling_layer, position);

    /* 
     * Now store the sibling's name. Note that preservation of zindex,
     * sibling_above and sibling_below are mutually exclusive, so if
     * we set one, the others are cleared.
     */
    LM_CLEAR_LAYER_MODIFICATION(js_layer, LAYER_MOD_ZINDEX); 
    if (position == CL_ABOVE) {
        LM_CLEAR_LAYER_MODIFICATION(js_layer, LAYER_MOD_SIB_ABOVE);        
        new_sibling_name = CL_GetLayerName(sibling_layer);
        if (new_sibling_name) {
            LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_SIB_BELOW);
            js_layer->sibling_below = JS_NewStringCopyZ(cx, new_sibling_name);
        }
    }

    if (position == CL_BELOW) {
        LM_CLEAR_LAYER_MODIFICATION(js_layer, LAYER_MOD_SIB_BELOW);        
        new_sibling_name = CL_GetLayerName(sibling_layer);
        if (new_sibling_name) {
            LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_SIB_ABOVE);
            js_layer->sibling_above = JS_NewStringCopyZ(cx, new_sibling_name);
        }
    }
    
    LO_UnlockLayout();
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
move_above(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    return change_layer_stacking(cx, obj, argc, argv, rval, CL_ABOVE);
}

PR_STATIC_CALLBACK(JSBool)
move_below(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    return change_layer_stacking(cx, obj, argc, argv, rval, CL_BELOW);
}

/* JS native method:

   Translate layer by given XY offset, e.g.
   document.layers[0].offset(1, 3); */
PR_STATIC_CALLBACK(JSBool)
offset_layer(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    int32 x, y;
    JSLayer *js_layer;
    MochaDecoder *decoder;
    CL_Layer *layer;
    jsval val;

    if (!(js_layer = JS_GetInstancePrivate(cx, obj, &lm_layer_class, argv)))
        return JS_FALSE;
    decoder = js_layer->decoder;

    /* Some layers have mostly read-only properties because we
       don't want a malicious/careless JS author to modify them,
       e.g.  for layers that are used to encapulate a mail
       message. In such cases, we fail silently. */
    if (js_layer->properties_locked)
        return JS_TRUE;
    
    LO_LockLayout();
    if(!decoder->window_context ||
	decoder->doc_id != XP_DOCID(decoder->window_context)) {
        LO_UnlockLayout();
        return JS_FALSE;
    }
    
    layer = LO_GetLayerFromId(decoder->window_context, js_layer->layer_id);
    if (!layer)
        goto error_exit;
    
    if (argc != 2) {
        JS_ReportError(cx,  lm_argc_err_str);
        goto error_exit;
    }
    if (!JS_ValueToInt32(cx, argv[0], &x) ||
        !JS_ValueToInt32(cx, argv[1], &y)) {
        goto error_exit;
    }
    
    /* If a layer is dynamically changing, it will probably look
       better if it utilizes offscreen compositing. */
    CL_ChangeLayerFlag(layer, CL_PREFER_DRAW_OFFSCREEN, PR_TRUE);

    CL_OffsetLayer(layer, (int32) x, (int32) y);

    /* We record that we've mutated left and top */
    if (!LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_LEFT)) {
        LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_LEFT);
        JS_GetProperty(cx, obj, lm_left_str, &val);
        JS_DefinePropertyWithTinyId(cx, obj, lm_left_str, LAYER_LEFT, val, 
                                    layer_getProperty, layer_setProperty, 
                                    JSPROP_ENUMERATE);
    }
    else
        JS_GetProperty(cx, obj, lm_left_str, &val);
    if (!LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_TOP)) {
        LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_TOP);
        JS_GetProperty(cx, obj, lm_top_str, &val);
        JS_DefinePropertyWithTinyId(cx, obj, lm_top_str, LAYER_TOP, val, 
                                    layer_getProperty, layer_setProperty, 
                                    JSPROP_ENUMERATE);
    }
    else
        JS_GetProperty(cx, obj, lm_top_str, &val);

    LO_UnlockLayout();
    return JS_TRUE;

  error_exit:
    LO_UnlockLayout();
    return JS_FALSE;
}

#define CLEAR_LAYER_EXPANSION_POLICY(layer, bits)                             \
 lo_SetLayerClipExpansionPolicy(layer,                                        \
                                lo_GetLayerClipExpansionPolicy(layer) & ~(bits))

PRIVATE JSBool
resize_layer_common(JSContext *cx, JSObject *obj, CL_Layer *layer,
                    int32 width, int32 height)
{
    JSLayer *js_layer;
    MochaDecoder * decoder;
    JSObject *clip;
    jsval val;

    if (!(js_layer = JS_GetPrivate(cx, obj)))
        return JS_FALSE;

    /* Some layers have mostly read-only properties because we
       don't want a malicious/careless JS author to modify them,
       e.g.  for layers that are used to encapulate a mail
       message. In such cases, we fail silently. */
    if (js_layer->properties_locked)
        return JS_TRUE;
    
    decoder = js_layer->decoder;

    /* If a layer is dynamically changing, it will probably look
       better if it utilizes offscreen compositing. */
    CL_ChangeLayerFlag(layer, CL_PREFER_DRAW_OFFSCREEN, PR_TRUE);

    CL_ResizeLayer(layer, (int32) width, (int32) height);

    CLEAR_LAYER_EXPANSION_POLICY(layer, 
                  LO_AUTO_EXPAND_CLIP_BOTTOM | LO_AUTO_EXPAND_CLIP_RIGHT);
    if (JS_LookupProperty(cx, obj, lm_clip_str, &val) &&
        JSVAL_IS_OBJECT(val)) {
        clip = JSVAL_TO_OBJECT(val);
        if (!LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_RIGHT)) {
            LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_RIGHT);
            JS_DefinePropertyWithTinyId(cx, clip, lm_right_str, RECT_RIGHT,
                                        INT_TO_JSVAL(width), rect_getProperty,
                                        rect_setProperty, JSPROP_ENUMERATE);
        }
        else {
            JS_GetProperty(cx, clip, lm_right_str, &val);
        }

        if (!LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_BOTTOM)) {
            LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_BOTTOM);
            JS_DefinePropertyWithTinyId(cx, clip, lm_bottom_str, RECT_BOTTOM,
                                        INT_TO_JSVAL(height), rect_getProperty,
                                        rect_setProperty, JSPROP_ENUMERATE);
        }
        else {
            JS_GetProperty(cx, clip, lm_bottom_str, &val);
        }       
    }

    return JS_TRUE;
}


PR_STATIC_CALLBACK(JSBool)
resize_layer_to(JSContext *cx, JSObject *obj,
                uint argc, jsval *argv, jsval *rval)
{
    int32 x, y;
    JSLayer *js_layer;
    MochaDecoder * decoder;
    CL_Layer *layer;
    JSBool ret;

    if (!(js_layer = JS_GetInstancePrivate(cx, obj, &lm_layer_class, argv)))
        return JS_FALSE;
    decoder = js_layer->decoder;
    
    LO_LockLayout();
    if(!decoder->window_context || 
	decoder->doc_id != XP_DOCID(decoder->window_context)) {
        LO_UnlockLayout();
        return JS_FALSE;
    }
    
    layer = LO_GetLayerFromId(decoder->window_context, js_layer->layer_id);
    if (!layer)
        goto error_exit;

    if (argc != 2) {
        JS_ReportError(cx, lm_argc_err_str);
        goto error_exit;
    }
    if (!JS_ValueToInt32(cx, argv[0], &x) ||
        !JS_ValueToInt32(cx, argv[1], &y)) {
        goto error_exit;
    }
    
    ret = resize_layer_common(cx, obj, layer, x, y);
    
    LO_UnlockLayout();
    return ret;
    
  error_exit:
    LO_UnlockLayout();
    return JS_FALSE;
}

PR_STATIC_CALLBACK(JSBool)
resize_layer_by(JSContext *cx, JSObject *obj,
                uint argc, jsval *argv, jsval *rval)
{
    int32 x, y;
    JSLayer *js_layer;
    MochaDecoder * decoder;
    CL_Layer *layer;
    XP_Rect bbox;
    JSBool ret;
    
    if (!(js_layer = JS_GetInstancePrivate(cx, obj, &lm_layer_class, argv)))
        return JS_FALSE;
    decoder = js_layer->decoder;
    
    LO_LockLayout();
    if(!decoder->window_context ||
	decoder->doc_id != XP_DOCID(decoder->window_context)) {
        LO_UnlockLayout();
        return JS_FALSE;
    }
    
    layer = LO_GetLayerFromId(decoder->window_context, js_layer->layer_id);
    if (!layer)
        goto error_exit;
    if (argc != 2) {
        JS_ReportError(cx, lm_argc_err_str);
        goto error_exit;
    }
    if (!JS_ValueToInt32(cx, argv[0], &x) ||
        !JS_ValueToInt32(cx, argv[1], &y)) {
        goto error_exit;
    }

    CL_GetLayerBbox(layer, &bbox);
    
    ret = resize_layer_common(cx, obj, layer,
                              (bbox.right - bbox.left + x),
                              (bbox.bottom - bbox.top + y));

    LO_UnlockLayout();
    return ret;

  error_exit:
    LO_UnlockLayout();
    return JS_FALSE;
}


PR_STATIC_CALLBACK(JSBool)
load_layer(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    jsdouble width;
    JSLayer *js_layer;
    MochaDecoder * decoder;
    char *url;
    JSBool ret;
    const char *referer;

    if (!(js_layer = JS_GetInstancePrivate(cx, obj, &lm_layer_class, argv)))
        return JS_FALSE;

    /* Some layers have mostly read-only properties because we
       don't want a malicious/careless JS author to modify them,
       e.g.  for layers that are used to encapulate a mail
       message. In such cases, we fail silently. */
    if (js_layer->properties_locked)
        return JS_TRUE;

    decoder = js_layer->decoder;
    if (argc != 2) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }

    if ((!JSVAL_IS_STRING(argv[0]) &&
        !JS_ConvertValue(cx, argv[0], JSTYPE_STRING, &argv[0])) ||
        !JS_ValueToNumber(cx, argv[1], &width))
        return JS_FALSE;

    url = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
    url = (char *)lm_CheckURL(cx, url, JS_TRUE);
    if (! url)
        return JS_FALSE;

    referer = lm_GetSubjectOriginURL(cx);
    if (! referer) {
	XP_FREE(url);
        return JS_FALSE;
    }

    ret = (JSBool)ET_TweakLayer(decoder->window_context, NULL, 
                                (int32)width, 0,
                                (void *)url, js_layer->layer_id, 
                                CL_SetSrcWidth, referer,
				decoder->doc_id);

    *rval = BOOLEAN_TO_JSVAL(ret);

    if (ret) {
	lm_NewLayerDocument(decoder, js_layer->layer_id);
        LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_WIDTH);
        js_layer->width = (int32)width;
        LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_SRC);
        XP_FREEIF(js_layer->source_url);
        js_layer->source_url = url;
        decoder->stream_owner = js_layer->layer_id;
        if (js_layer->principals)
            JSPRINCIPALS_DROP(cx, js_layer->principals);
        js_layer->principals = LM_NewJSPrincipals(NULL, NULL, url);
        if (js_layer->principals == NULL) {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
        }
        JSPRINCIPALS_HOLD(cx, js_layer->principals);
    }

    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
layer_capture(JSContext *cx, JSObject *obj,
              uint argc, jsval *argv, jsval *rval)
{
    JSLayer *js_layer;
    MochaDecoder * decoder;
    jsdouble d;

    if (!(js_layer = JS_GetInstancePrivate(cx, obj, &lm_layer_class, argv)))
        return JS_FALSE;
    decoder = js_layer->decoder;

    if (!decoder->window_context)
        return JS_TRUE;

    if (argc != 1)
        return JS_TRUE;

    if (!JS_ValueToNumber(cx, argv[0], &d)) 
        return JS_FALSE;

    js_layer->capturer.event_bit |= (uint32)d;
    decoder->window_context->event_bit |= (uint32)d;

    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
layer_release(JSContext *cx, JSObject *obj,
              uint argc, jsval *argv, jsval *rval)
{
    JSLayer *js_layer;
    JSEventCapturer *cap;
    MochaDecoder *decoder;
    jsdouble d;
    jsint layer_index;
    jsint max_layer_num;
    JSObject *cap_obj;

    if (!(js_layer = JS_GetInstancePrivate(cx, obj, &lm_layer_class, argv)))
        return JS_FALSE;
    decoder = js_layer->decoder;

    if (!decoder->window_context)
        return JS_TRUE;

    if (argc != 1)
        return JS_TRUE;

    if (!JS_ValueToNumber(cx, argv[0], &d)) 
        return JS_FALSE;


    js_layer->capturer.event_bit &=  ~(uint32)d;
    decoder->window_context->event_bit &= ~(uint32)d;
    
    /*Now we have to see if anyone else wanted that bit set.  Joy!*/
    /*First we check versus window */
    decoder->window_context->event_bit |= decoder->event_bit;

    /*Now we check versus layers */
    max_layer_num = LO_GetNumberOfLayers(decoder->window_context);
    
    for (layer_index=0; layer_index <= max_layer_num; layer_index++) {
        cap_obj = LO_GetLayerMochaObjectFromId(decoder->window_context, layer_index);
         if (cap_obj && (cap = JS_GetPrivate(cx, cap_obj)) != NULL)
            decoder->window_context->event_bit |= cap->event_bit;
        
        cap_obj = lm_GetDocumentFromLayerId(decoder, layer_index);
        if (cap_obj && (cap = JS_GetPrivate(cx, cap_obj)) != NULL)
            decoder->window_context->event_bit |= cap->event_bit;
    }
    
    return JS_TRUE;
}

static PRBool
setExternalCapture(JSContext *cx, JSObject *obj, 
                   uint argc, jsval *argv, JSBool val)
{
    JSLayer *js_layer;

    if (!(js_layer = JS_GetInstancePrivate(cx, obj, &lm_layer_class, argv)))
        return JS_FALSE;

    if (argc != 0)
        return JS_TRUE;

    if (lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE)) {
        JSPrincipals *principals;
        
        principals = lm_GetInnermostPrincipals(cx, obj, NULL);
        if (principals == NULL)
            return JS_FALSE;

        lm_SetExternalCapture(cx, principals, val);
    }

    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
layer_enable_external_capture(JSContext *cx, JSObject *obj,
                              uint argc, jsval *argv, jsval *rval)
{
    return setExternalCapture(cx, obj, argc, argv, JS_TRUE);
}

PR_STATIC_CALLBACK(PRBool)
layer_disable_external_capture(JSContext *cx, JSObject *obj,
                               uint argc, jsval *argv, jsval *rval)
{
    return setExternalCapture(cx, obj, argc, argv, JS_FALSE);
}

PR_STATIC_CALLBACK(PRBool)
layer_compromise_principals(JSContext *cx, JSObject *obj,
                            uint argc, jsval *argv, jsval *rval)
{
    JSLayer *js_layer;

    if (!(js_layer = JS_GetInstancePrivate(cx, obj, &lm_layer_class, argv)))
        return JS_FALSE;

    js_layer->principals_compromised = JS_TRUE;
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
layer_downgrade_principals(JSContext *cx, JSObject *obj,
                           uint argc, jsval *argv, jsval *rval)
{
    JSLayer *js_layer;

    if (!(js_layer = JS_GetInstancePrivate(cx, obj, &lm_layer_class, argv)))
        return JS_FALSE;

    if (js_layer->principals)
        lm_InvalidateCertPrincipals(js_layer->decoder, js_layer->principals);

    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
layer_initStandardObjects(JSContext *cx, JSObject *obj, uint argc, jsval *argv,
                          jsval *rval)
{
    JSLayer *js_layer;

    if (!(js_layer = JS_GetInstancePrivate(cx, obj, &lm_layer_class, argv)))
        return JS_FALSE;
    return (JSBool)(JS_InitStandardClasses(cx, obj) &&
           lm_DefineWindowProps(cx, js_layer->decoder));
}

/* The minimum number of arguments for each of the functions in this
   table is set to zero, so that argc is the actual number of arguments
   passed by the user. */
static JSFunctionSpec layer_methods[] = {
    {"offset",         offset_layer,    0},
    {"moveBy",         offset_layer,    0},
    {"moveTo",         move_layer_rel,  0},
    {"moveToAbsolute", move_layer_abs,  0},
    {"resize",         resize_layer_to, 0},
    {"resizeTo",       resize_layer_to, 0},
    {"resizeBy",       resize_layer_by, 0},
    {"moveAbove",      move_above,      0},
    {"moveBelow",      move_below,      0},
    {"load",           load_layer,      0},
    {"captureEvents",  layer_capture,   0},
    {"releaseEvents",  layer_release,   0},
    {"enableExternalCapture",   layer_enable_external_capture,  0 },
    {"disableExternalCapture",  layer_disable_external_capture, 0 },
    {"compromisePrincipals",    layer_compromise_principals,    0 },
    {"downgradePrincipals",     layer_downgrade_principals,     0 },
    {"initStandardObjects", layer_initStandardObjects, 0},
    {0}
};

static JSObject *
lm_reflect_layer_using_existing_obj(MWContext *context,
                                    int32 layer_id,
                                    int32 parent_layer_id, 
                                    PA_Tag *tag,
                                    JSObject *obj);

PR_STATIC_CALLBACK(JSBool)
Layer(JSContext *cx, JSObject *obj,
      uint argc, jsval *argv, jsval *rval)
{
    JSObject *parent_layer_obj = NULL, *window_obj, *enclosing_obj;
    JSLayer *js_layer_parent;
    int32 wrap_width = 0;
    int32 parent_layer_id, layer_id;
    MochaDecoder *decoder;
    MWContext *context;

    XP_ASSERT(JS_InstanceOf(cx, obj, &lm_layer_class, NULL));

    /* XXX - Hack to force constructor not to allocate any layers */
    if (argc == 0)
        return JS_TRUE;

    if (argc < 1) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }

    /* Mandatory first argument is layer's wrap width. */
    if (!JS_ValueToInt32(cx, argv[0], &wrap_width))
        return JS_FALSE;

    /* By default, we're creating the layer for the current window. */ 
    window_obj = JS_GetGlobalObject(cx); 

    /* Get enclosing window or layer of the code that calls the constructor. */
    parent_layer_id = LO_DOCUMENT_LAYER_ID; /* Default */
    enclosing_obj = JS_GetScopeChain(cx);
    while (enclosing_obj) {
        if (JS_InstanceOf(cx, enclosing_obj, &lm_layer_class, 0)) {
            js_layer_parent = JS_GetPrivate(cx, enclosing_obj);
            parent_layer_id = js_layer_parent->layer_id;
            break;
        }
        if (JS_InstanceOf(cx, enclosing_obj, &lm_window_class, 0))
            break;
        enclosing_obj = JS_GetParent(cx, enclosing_obj);
    }
    /* Optional second argument is parent layer */
    if (argc >= 2) {
        /* The parent has to be an object */
        if ((JS_TypeOfValue(cx, argv[1]) == JSTYPE_OBJECT) &&
            ((parent_layer_obj = JSVAL_TO_OBJECT(argv[1])) != NULL)) { 
            /* If the parent is a layer, we get its layer_id */
            if (JS_InstanceOf(cx, parent_layer_obj, &lm_layer_class, 0)) { 
                js_layer_parent = JS_GetPrivate(cx, parent_layer_obj); 
                parent_layer_id = js_layer_parent->layer_id;
				decoder = js_layer_parent->decoder;
				window_obj = decoder->window_object;
            } 
            /* 
             * If the parent is another window, the new layer is created
             * in that window's context as a top-level layer.
             */ 
            else if (JS_InstanceOf(cx, parent_layer_obj, 
                                   &lm_window_class, 0)) { 
                parent_layer_id = LO_DOCUMENT_LAYER_ID; 
                window_obj = parent_layer_obj; 
            } 
            else 
                return JS_FALSE; 
        } else {
            return JS_FALSE;
        }
    }

    decoder = JS_GetPrivate(cx, window_obj); 
    context = decoder->window_context;

    /* From a security standpoint, it's dangerous to allow new
       top-level layers to be created in a mail/news window. */
    if (IS_MESSAGE_WINDOW(context) &&
        (parent_layer_id == LO_DOCUMENT_LAYER_ID)) {
        JS_ReportError(cx,
                       "Disallowed attempt to create new top-level layer"
                       " in a message window");
        return JS_FALSE;
    }

    layer_id = ET_PostCreateLayer(context, wrap_width, parent_layer_id);

    /* Zero-valued return means layout/parser stream is busy. A -1
       return indicates error. */
    if (!layer_id || layer_id == -1)
        return JS_FALSE;

    /* Reflect the layer, but don't create a new object parented by
       the layers array.  Rather, use the object that we're
       constructing now. */
    obj = lm_reflect_layer_using_existing_obj(context, layer_id,
                                              parent_layer_id, NULL, obj);
    if (!obj)
        return JS_FALSE;
    return JS_TRUE;
}

static JSObject *
lm_init_layer_clip(MochaDecoder *decoder,
                   int32 layer_id,
                   JSObject *parent_layer_obj);

void
lm_RestoreLayerState(MWContext *context, int32 layer_id, 
                     LO_BlockInitializeStruct *param)
{
    PRHashTable *map;
    MochaDecoder *decoder;
    JSContext *cx;
    JSLayer *js_layer;
    JSObject *obj, *clip_obj;
    jsval val;

    decoder = LM_GetMochaDecoder(context);
    if (!decoder)               /* Paranoia */
        return;
    cx = decoder->js_context;

    /* 
     * Get the mocha object associated with this layer, since it's been
     * preserved across a resize_relayout.
     */
    map = lm_GetIdToObjectMap(decoder);
    if (map)
        obj = (JSObject *)PR_HashTableLookup(map,
                                  LM_GET_MAPPING_KEY(LM_LAYERS, 0, layer_id));
    if (!obj)
        goto out;
    
    js_layer = JS_GetPrivate(cx, obj);
    if (!js_layer)
        goto out;

    /* 
     * For each property that was modified, set the value for the
     * corresponding attribute in the param struct.
     */
    if (LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_LEFT) &&
        JS_LookupProperty(cx, obj, lm_left_str, &val)) {
        param->has_left = TRUE;
        param->left = JSVAL_TO_INT(val);
    }

    if (LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_TOP) &&
        JS_LookupProperty(cx, obj, lm_top_str, &val)) {
        param->has_top = TRUE;
        param->top = JSVAL_TO_INT(val);
    }
    
    if (LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_WIDTH)) {
        param->has_width = TRUE;
        param->width = js_layer->width;
    }

    if (LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_SIB_ABOVE)) {
        XP_FREEIF(param->above);
        param->above = XP_STRDUP(JS_GetStringBytes(js_layer->sibling_above));
    }

    if (LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_SIB_BELOW)) {
        XP_FREEIF(param->below);
        param->below = XP_STRDUP(JS_GetStringBytes(js_layer->sibling_below));
    }
    
    if (LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_ZINDEX) &&
        JS_LookupProperty(cx, obj, lm_zindex_str, &val)) {
        param->has_zindex = TRUE;
        param->zindex = JSVAL_TO_INT(val);
    }

    if (LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_VISIBILITY) &&
        JS_LookupProperty(cx, obj, lm_visibility_str, &val)) {
        XP_FREEIF(param->visibility);
        param->visibility = XP_STRDUP(JS_GetStringBytes(JSVAL_TO_STRING(val)));
    }

    if (LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_BGCOLOR) &&
        JS_LookupProperty(cx, obj, lm_bgcolor_str, &val)) {
        XP_FREEIF(param->bgcolor);
        param->is_style_bgcolor = FALSE;
        param->bgcolor = XP_ALLOC(10);
        PR_snprintf(param->bgcolor, 10, "%x", JSVAL_TO_INT(val));
    }

    if (JS_LookupProperty(cx, obj, lm_background_str, &val) &&
        JSVAL_IS_OBJECT(val)) {
        JSObject *background;

        if (JS_ValueToObject(cx, val, &background) && background &&
            JS_LookupProperty(cx, background, "src", &val) &&
            JSVAL_IS_STRING(val)) {
            XP_FREEIF(param->bgimage);
            param->bgimage = XP_STRDUP(JS_GetStringBytes(JSVAL_TO_STRING(val)));
        }
    }

    if (LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_SRC) &&
        js_layer->source_url) {
        XP_FREEIF(param->src);
        param->src = XP_STRDUP(js_layer->source_url);
    }
    
    if (JS_LookupProperty(cx, obj, lm_clip_str, &val) &&
        JSVAL_IS_OBJECT(val)) {
        clip_obj = JSVAL_TO_OBJECT(val);
        if (!clip_obj)
            goto out;

        if (!param->clip && 
            (LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_LEFT) ||
             LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_RIGHT) ||
             LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_BOTTOM) ||
             LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_TOP))) {
            param->clip = XP_NEW_ZAP(XP_Rect);
            if (!param->clip)
                goto out;
        }
        
        if (LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_LEFT) &&
            JS_LookupProperty(cx, clip_obj, lm_left_str, &val)) {
            param->clip->left = JSVAL_TO_INT(val);
            param->clip_expansion_policy &= ~LO_AUTO_EXPAND_CLIP_LEFT;
        }
        
        if (LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_RIGHT) &&
            JS_LookupProperty(cx, clip_obj, lm_right_str, &val)) {
            param->clip->right = JSVAL_TO_INT(val);
            param->clip_expansion_policy &= ~LO_AUTO_EXPAND_CLIP_RIGHT;
        }

        if (LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_TOP) &&
            JS_LookupProperty(cx, clip_obj, lm_top_str, &val)) {
            param->clip->top = JSVAL_TO_INT(val);
            param->clip_expansion_policy &= ~LO_AUTO_EXPAND_CLIP_TOP;
        }
        
        if (LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_BOTTOM) &&
            JS_LookupProperty(cx, clip_obj, lm_bottom_str, &val)) {
            param->clip->bottom = JSVAL_TO_INT(val);
            param->clip_expansion_policy &= ~LO_AUTO_EXPAND_CLIP_BOTTOM;
        }
    }

out:
    LM_PutMochaDecoder(decoder);
    return;
}

static void
lm_process_layer_tag(MochaDecoder *decoder, PA_Tag *tag, JSObject *obj)
{
    MWContext *context = decoder->window_context;
    PA_Block onload, onunload, onmouseover, onmouseout, onblur, onfocus, id;
    PA_Block properties_locked;
    JSLayer *js_layer;
    JSContext *cx;
    char *source_url;
    
    cx = decoder->js_context;
    js_layer = JS_GetPrivate(cx, obj);
    if (!js_layer)
        return;
    
    if (tag) {
        onload   = lo_FetchParamValue(context, tag, PARAM_ONLOAD);
        onunload  = lo_FetchParamValue(context, tag, PARAM_ONUNLOAD);
        onmouseover  = lo_FetchParamValue(context, tag, PARAM_ONMOUSEOVER);
        onmouseout  = lo_FetchParamValue(context, tag, PARAM_ONMOUSEOUT);
        onblur  = lo_FetchParamValue(context, tag,  PARAM_ONBLUR);
        onfocus  = lo_FetchParamValue(context, tag, PARAM_ONFOCUS);
        id  = lo_FetchParamValue(context, tag, PARAM_ID);
        properties_locked = lo_FetchParamValue(context, tag, PARAM_LOCKED);

	/* don't hold the layout lock across compiles */
	LO_UnlockLayout();

        if (properties_locked) {
            js_layer->properties_locked = PR_TRUE;
            PA_FREE(properties_locked);
        }

        if (onload != NULL) {
            (void) lm_CompileEventHandler(decoder, id, tag->data,
                                          tag->newline_count, obj,
                                          PARAM_ONLOAD, onload);
            PA_FREE(onload);
        }

        if (onunload != NULL) {
            (void) lm_CompileEventHandler(decoder, id, tag->data,
                                          tag->newline_count, obj,
                                          PARAM_ONUNLOAD, onunload);
            PA_FREE(onunload);
        }

        if (onmouseover != NULL) {
            (void) lm_CompileEventHandler(decoder, id, tag->data,
                                          tag->newline_count, obj,
                                          PARAM_ONMOUSEOVER, onmouseover);
            PA_FREE(onmouseover);
        }

        if (onmouseout != NULL) {
            (void) lm_CompileEventHandler(decoder, id, tag->data,
                                          tag->newline_count, obj,
                                          PARAM_ONMOUSEOUT, onmouseout);
            PA_FREE(onmouseout);
        }

        if (onblur != NULL) {
            (void) lm_CompileEventHandler(decoder, id, tag->data,
                                          tag->newline_count, obj,
                                          PARAM_ONBLUR, onblur);
            PA_FREE(onblur);
        }

        if (onfocus != NULL) {
            (void) lm_CompileEventHandler(decoder, id, tag->data,
                                          tag->newline_count, obj,
                                          PARAM_ONFOCUS, onfocus);
            PA_FREE(onfocus);
        }

        if (id)
            PA_FREE(id);

	LO_LockLayout();

        source_url = (char*)lo_FetchParamValue(context, tag, 
                                               PARAM_SRC);

        if (source_url) {
	    /*
	     * Temporarily unlock layout so that we don't hold the lock
	     * across a call (lm_CheckURL) that may result in 
	     * synchronous event handling.
	     */
	    LO_UnlockLayout();
            js_layer->source_url = (char *)lm_CheckURL(cx, source_url, 
                                                       JS_TRUE);
            if (js_layer->principals)
                JSPRINCIPALS_DROP(cx, js_layer->principals);
            js_layer->principals = LM_NewJSPrincipals(NULL, NULL, 
                                                      js_layer->source_url);
            if (js_layer->principals)
                JSPRINCIPALS_HOLD(cx, js_layer->principals);
	    LO_LockLayout();
	}
    }
}


static JSObject *
lm_reflect_layer_using_existing_obj(MWContext *context,
                                    int32 layer_id,
                                    int32 parent_layer_id, 
                                    PA_Tag *tag,
                                    JSObject *obj)
{
    JSObject *array_obj, *clip_obj, *doc_obj;
    MochaDecoder *decoder;
    JSContext *cx;
    JSLayer *js_layer;
    char *name = NULL;    
    uint flags = JSPROP_READONLY;
    JSBool bFreeName = JS_FALSE;
    static fake_layer_count = 0;
    CL_Layer *layer;
    lo_TopState *top_state;
    PRHashTable *map;

    decoder = LM_GetMochaDecoder(context);
    if (!decoder)
        return NULL;

    if (!obj) {
        obj = LO_GetLayerMochaObjectFromId(context, layer_id);
        if (obj) {                    /* Already reflected ? */
            /* 
             * We might already have been reflected by someone other than the
             * layer tag processing code (e.g. as a result of entity 
             * processing). In that case, we just process the tag if it 
             * exists.
             */
            if (tag)
                lm_process_layer_tag(decoder, tag, obj);
            LM_PutMochaDecoder(decoder);
            return obj;
        }
    }

    cx = decoder->js_context;

    top_state = lo_GetMochaTopState(context);
    if (!obj && top_state->resize_reload && !decoder->load_event_sent) {
        map = lm_GetIdToObjectMap(decoder);
        
        if (map)
            obj = (JSObject *)PR_HashTableLookup(map,
                                  LM_GET_MAPPING_KEY(LM_LAYERS, 0, layer_id));
        if (obj) {
            LO_SetLayerMochaObject(context, layer_id, obj);
            LM_PutMochaDecoder(decoder);
            return obj;
        }
    }

    /* Layer is accessible by name in child array of parent layer.
       For example a layer named "Fred" could be accessed as
       document.layers["Fred"] if it is a child of the distinguished
       _DOCUMENT layer. */
    if (parent_layer_id == LO_DOCUMENT_LAYER_ID)
        array_obj = lm_GetDocumentLayerArray(decoder, decoder->document);
    else
        array_obj =
            lm_GetLayerArray(decoder, 
                             LO_GetLayerMochaObjectFromId(context, 
                                                          parent_layer_id));
    if (!array_obj) {
        LM_PutMochaDecoder(decoder);
        return NULL;
    }

    js_layer = JS_malloc(cx, sizeof *js_layer);
    if (!js_layer) {
        LM_PutMochaDecoder(decoder);
        return NULL;
    }
    XP_BZERO(js_layer, sizeof *js_layer);

    if (!obj) {
        obj = JS_NewObject(cx, &lm_layer_class, decoder->layer_prototype, 
                           array_obj);
    } else {
        /* Anonymous layers, created with the "new" operator, are
           scoped by their parent layer's document */
        JS_SetParent(cx, obj, array_obj);
    }
    
    LO_SetLayerMochaObject(context, layer_id, obj);

    /* Put it in the index to object hash table */
    map = lm_GetIdToObjectMap(decoder);
    if (map)
        PR_HashTableAdd(map,
                        LM_GET_MAPPING_KEY(LM_LAYERS, 0, layer_id),
                        obj);

    if (obj) 
        clip_obj = lm_init_layer_clip(decoder, layer_id, obj);

    if (obj && clip_obj)
        doc_obj = lm_DefineDocument(decoder, layer_id);

    if (!obj || !clip_obj || !doc_obj ||  !JS_SetPrivate(cx, obj, js_layer)) {
        JS_free(cx, js_layer);
        LM_PutMochaDecoder(decoder);
        return NULL;
    }

    JS_AddNamedRoot(cx, &js_layer->sibling_above, "layer.sibling_above");
    JS_AddNamedRoot(cx, &js_layer->sibling_below, "layer.sibling_below");

    layer = LO_GetLayerFromId(context, layer_id);
    if (layer)
        name = CL_GetLayerName(layer);
    if (name) {
        flags |= JSPROP_ENUMERATE;
    } else {
        name = PR_smprintf("_js_layer_%d", fake_layer_count++);
        bFreeName = JS_TRUE;
        if (!name) {
            LM_PutMochaDecoder(decoder);
            return NULL;
        }
    }

    if (name && 
        !JS_DefineProperty(cx, array_obj, name, OBJECT_TO_JSVAL(obj),
                           NULL, NULL, flags)) {
        LM_PutMochaDecoder(decoder);
        if (bFreeName) 
            XP_FREE(name);
        return NULL;
    }

    js_layer->decoder = HOLD_BACK_COUNT(decoder);
    js_layer->layer_id = layer_id;
    js_layer->name = JS_NewStringCopyZ(cx, name);
    js_layer->source_url = NULL;
    if (!JS_LockGCThing(cx, js_layer->name)) {
        LM_PutMochaDecoder(decoder);
        if (bFreeName)
            XP_FREE(name);
        return NULL;
    }

    if(tag)
        lm_process_layer_tag(decoder, tag, obj);

    LM_PutMochaDecoder(decoder);

    if (bFreeName)
        XP_FREE(name);

    return obj;
}

JSObject *
LM_ReflectLayer(MWContext *context, int32 layer_id, int32 parent_layer_id, 
                PA_Tag *tag)
{
    return lm_reflect_layer_using_existing_obj(context, layer_id,
                                               parent_layer_id, tag, NULL);
}

JSObject *
lm_GetNamedLayer(MochaDecoder *decoder, int32 parent_layer_id, 
                 const char *name)
{
    MWContext *context = decoder->window_context;
    lo_TopState *top_state;    
    CL_Layer *layer, *parent_layer;
    int32 layer_id; 
    JSObject *layer_obj = NULL;

    LO_LockLayout();

    top_state = lo_GetMochaTopState(context);
    if (top_state == NULL)
        goto done;
    
    if(!context || decoder->doc_id != XP_DOCID(context))
        goto done;

    /* Get the parent layer (the owner of this array) */
    parent_layer = LO_GetLayerFromId(context, parent_layer_id);
    if (!parent_layer) 
        goto done;

    /* Search for the child layer by name */
    layer = CL_GetLayerChildByName(parent_layer, (char *)name);
    if (!layer) 
        goto done;
    
    /* Reflect the child layer if it hasn't been reflected yet */
    layer_id = LO_GetIdFromLayer(context, layer);
    layer_obj = LM_ReflectLayer(context, layer_id, 
                                parent_layer_id,
                                NULL);
  done:
    LO_UnlockLayout();
    return layer_obj;
}

void
lm_NewLayerDocument(MochaDecoder *decoder, int32 layer_id)
{
    JSObject *layer_obj, *doc_obj, *layer_array_obj;
    JSLayer *js_layer;
    JSContext *cx;
    jsval layer_array_jsval;
    
    cx = decoder->js_context;

    LO_LockLayout();
    
    layer_obj = LO_GetLayerMochaObjectFromId(decoder->window_context,
                                             layer_id);
    if (!layer_obj) {
        LO_UnlockLayout();
        return;
    }

    js_layer = JS_GetPrivate(cx, layer_obj);
    if (!js_layer) {
        LO_UnlockLayout();
        return;
    }

    /* Throw away the child_layers_array */
    js_layer->child_layers_array_obj = NULL;

    /* Reset security status, clear watchpoints to close security holes. */
    /* XXX must clear scope or evil.org's functions/vars persist into signed
	   script doc loaded from victim.com! */
    js_layer->principals_compromised = JS_FALSE;
    JS_ClearWatchPointsForObject(cx, layer_obj);

    /* 
     * Clean out a document i.e. clean out the arrays associated with
     * the document, but not all its properties.
     */
    doc_obj = lm_GetDocumentFromLayerId(decoder, layer_id);
    lm_CleanUpDocument(decoder, doc_obj);

    /* We're about to replace the contents of this layer, so sever
       all the references to the layer's children, so that they
       can be GC'ed. */
    if (JS_LookupProperty(cx, layer_obj, "layers", &layer_array_jsval) &&
	JSVAL_IS_OBJECT(layer_array_jsval)) {
        layer_array_obj = JSVAL_TO_OBJECT(layer_array_jsval);
	JS_ClearScope(cx, layer_array_obj);
	JS_DefineProperties(cx, layer_array_obj, layer_array_props);
    }
    LO_UnlockLayout();
}

void
lm_SendLayerLoadEvent(MWContext *context, int32 event, int32 layer_id, 
                      JSBool resize_reload)
{
    MochaDecoder *decoder;
    JSObject *obj;
    JSEvent *pEvent;
    jsval rval;
    
    decoder = context->mocha_context ? LM_GetMochaDecoder(context) : 0;
    if (resize_reload)
        goto out;

    LO_LockLayout();
    obj = LO_GetLayerMochaObjectFromId(context, layer_id);
    LO_UnlockLayout();
    if (!obj)
        goto out;

    /* 
     * XXX Right now, we send the load event when the layer is finished
     * laying out, not when all the streams associated with a layer
     * are completed. 
     */
    switch (event) {
      case EVENT_LOAD:
      case EVENT_UNLOAD:
          pEvent = XP_NEW_ZAP(JSEvent);
          if (!pEvent)
              goto out;
          
          pEvent->type = event;
          
          (void) lm_SendEvent(context, obj, pEvent, &rval);

          if (!pEvent->saved)
              XP_FREE(pEvent);
          break;
      case EVENT_ABORT:
          break;
      default: ;
    }

out:
    if (decoder)
        LM_PutMochaDecoder(decoder);
    return;
}

void
lm_DestroyLayer(MWContext *context, JSObject *obj)
{
    jsval val;
    JSObject *child_doc_obj, *parent_doc_obj, *layer_array_obj;
    JSLayer *js_layer;
    JSBool ok;
    MochaDecoder *decoder;
    JSContext *cx;
    char *layer_name;

    decoder = LM_GetMochaDecoder(context);
    if (!decoder)
        return;
    
    cx = decoder->js_context;
    
    if (!JS_GetProperty(cx, obj, lm_document_str, &val) ||
        !JSVAL_IS_OBJECT(val)) {
        goto end;
    }

    child_doc_obj = JSVAL_TO_OBJECT(val);
    if (!child_doc_obj)
        goto end;

    /* If we had a layer called "foo", then there was a "document.foo"
       property created in the parent document at the time this layer
       was reflected.  Get rid of it, so that we can GC the layer
       object. */
    layer_array_obj = JS_GetParent(cx, obj);
    if (!layer_array_obj)
        goto end;
    
    parent_doc_obj = JS_GetParent(cx, layer_array_obj);
    if (!parent_doc_obj)
        goto end;
    
    js_layer = JS_GetPrivate(cx, obj);    
    layer_name = (char *)JS_GetStringBytes(js_layer->name);
    ok = JS_DeleteProperty(cx, parent_doc_obj, layer_name);
    XP_ASSERT(ok);

    lm_CleanUpDocumentRoots(decoder, child_doc_obj);

  end:
    LM_PutMochaDecoder(decoder);
}


/* 
 * Called to set the source URL as a result of a document.open() on the
 * layer's document.
 */
JSBool
lm_SetLayerSourceURL(MochaDecoder *decoder, int32 layer_id, char *url)
{
    MWContext *context;
    JSObject *layer_obj;
    JSLayer *js_layer;

    context = decoder->window_context;
    if (!context)
        return JS_FALSE;
    
    LO_LockLayout();
    layer_obj = LO_GetLayerMochaObjectFromId(context, layer_id);
    LO_UnlockLayout();
    if (!layer_obj)
        return JS_FALSE;
    
    js_layer = JS_GetPrivate(decoder->js_context, layer_obj);
    if (!js_layer)
        return JS_FALSE;

    XP_FREEIF(js_layer->source_url);
    js_layer->source_url = XP_STRDUP(url);
    LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_SRC);
    decoder->stream_owner = layer_id;

    return JS_TRUE;
}

const char *
lm_GetLayerOriginURL(JSContext *cx, JSObject *obj) {
    JSLayer *js_layer = JS_GetPrivate(cx, obj);
    if (js_layer == NULL || js_layer->source_url == NULL) {
        return NULL;
    }
    return js_layer->source_url;
    
}

JSBool
lm_InitLayerClass(MochaDecoder *decoder)
{
    JSContext *cx;
    JSObject *prototype;

    cx = decoder->js_context;
    prototype = JS_InitClass(cx, decoder->window_object,
			     decoder->event_capturer_prototype, &lm_layer_class,
                             Layer, 1, layer_props, layer_methods, NULL, NULL);
    if (!prototype)
        return JS_FALSE;
    decoder->layer_prototype = prototype;
    return JS_TRUE;
}

/* ======================== Rect Class definition ======================= */

typedef struct JSRect {
    MochaDecoder           *decoder;
    int32                   layer_id;
} JSRect;


/* Static compositor rect properties */
static JSPropertySpec rect_props[] = {
    {lm_left_str,           RECT_LEFT,         JSPROP_ENUMERATE},
    {lm_top_str,            RECT_TOP,          JSPROP_ENUMERATE},
    {lm_right_str,          RECT_RIGHT,        JSPROP_ENUMERATE},
    {lm_bottom_str,         RECT_BOTTOM,       JSPROP_ENUMERATE},
    {"height",              RECT_HEIGHT,       JSPROP_ENUMERATE},
    {"width",               RECT_WIDTH,        JSPROP_ENUMERATE},
    {0}
};

PR_STATIC_CALLBACK(JSBool)
rect_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    MochaDecoder *decoder;
    MWContext *context;
    JSRect *js_rect;
    XP_Rect bbox;
    jsint slot;
    CL_Layer *layer;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    js_rect = JS_GetPrivate(cx, obj);
    if (!js_rect)
        return JS_TRUE;

    decoder = js_rect->decoder;
    context = decoder->window_context;

    LO_LockLayout();
    if(!context || decoder->doc_id != XP_DOCID(context)) {
        LO_UnlockLayout();
        return JS_FALSE;
    }

    layer = LO_GetLayerFromId(context, js_rect->layer_id);
    if (!layer) {
        LO_UnlockLayout();
        return JS_TRUE;
    }

    CL_GetLayerBbox(layer, &bbox);

    switch (slot) {
    case RECT_LEFT:
        *vp = INT_TO_JSVAL(bbox.left);
        break;

    case RECT_RIGHT:
        *vp = INT_TO_JSVAL(bbox.right);
        break;

    case RECT_TOP:
        *vp = INT_TO_JSVAL(bbox.top);
        break;

    case RECT_BOTTOM:
        *vp = INT_TO_JSVAL(bbox.bottom);
        break;

    case RECT_WIDTH:
        *vp = INT_TO_JSVAL(bbox.right - bbox.left);
        break;

    case RECT_HEIGHT:
        *vp = INT_TO_JSVAL(bbox.bottom - bbox.top);
        break;

    default:
        break;
    }

    LO_UnlockLayout();
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
rect_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSRect *js_rect;
    MochaDecoder *decoder;
    MWContext *context;
    CL_Layer *layer;
    int32 val;
    enum rect_slot rect_slot;
    jsint slot;
    JSObject *layer_obj;
    JSLayer *js_layer;
    jsval js_val;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    rect_slot = slot = JSVAL_TO_INT(id);

    js_rect = JS_GetPrivate(cx, obj);
    if (!js_rect)
        return JS_TRUE;

    decoder = js_rect->decoder;
    layer = LO_GetLayerFromId(decoder->window_context, js_rect->layer_id);
    if (!layer)
        return JS_TRUE;
    layer_obj = LO_GetLayerMochaObjectFromId(decoder->window_context,
                                             js_rect->layer_id);
    if (!layer_obj)
        return JS_TRUE;
    js_layer = JS_GetPrivate(cx, layer_obj);
    if (!js_layer)
        return JS_TRUE;
    /* Some layers have mostly read-only properties because we
       don't want a malicious/careless JS author to modify them,
       e.g.  for layers that are used to encapulate a mail
       message. In such cases, we fail silently. */
    if (js_layer->properties_locked)
        return JS_TRUE;
    
    context = decoder->window_context;

    if (rect_slot == RECT_LEFT   ||
        rect_slot == RECT_RIGHT  ||
        rect_slot == RECT_TOP    ||
        rect_slot == RECT_BOTTOM ||
        rect_slot == RECT_WIDTH  ||
        rect_slot == RECT_HEIGHT) {

        XP_Rect bbox;

        if (!JS_ValueToInt32(cx, *vp, &val))
            return JS_FALSE;

        LO_LockLayout();
        if(!context || decoder->doc_id != XP_DOCID(context)) {
            LO_UnlockLayout();
            return JS_FALSE;
        }

        CL_GetLayerBbox(layer, &bbox);

        /* If a layer is dynamically changing, it will probably look
           better if it utilizes offscreen compositing. */
        CL_ChangeLayerFlag(layer, CL_PREFER_DRAW_OFFSCREEN, PR_TRUE);

        switch (rect_slot) {
        case RECT_LEFT:
            bbox.left = (int32)val;
            LO_SetLayerBbox(layer, &bbox);
            CLEAR_LAYER_EXPANSION_POLICY(layer, LO_AUTO_EXPAND_CLIP_LEFT);
            LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_LEFT);
            break;

        case RECT_RIGHT:
            bbox.right = (int32)val;
            LO_SetLayerBbox(layer, &bbox);
            CLEAR_LAYER_EXPANSION_POLICY(layer, LO_AUTO_EXPAND_CLIP_RIGHT);
            LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_RIGHT);
            break;

        case RECT_TOP:
            bbox.top = (int32)val;
            LO_SetLayerBbox(layer, &bbox);
            CLEAR_LAYER_EXPANSION_POLICY(layer, LO_AUTO_EXPAND_CLIP_TOP);
            LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_TOP);
            break;

        case RECT_BOTTOM:
            bbox.bottom = (int32)val;
            LO_SetLayerBbox(layer, &bbox);
            CLEAR_LAYER_EXPANSION_POLICY(layer, LO_AUTO_EXPAND_CLIP_BOTTOM);
            LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_BOTTOM);
            break;

        case RECT_WIDTH:
            bbox.right = bbox.left + (int32)val;
            LO_SetLayerBbox(layer, &bbox);
            CLEAR_LAYER_EXPANSION_POLICY(layer, LO_AUTO_EXPAND_CLIP_RIGHT);
            if (!LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_RIGHT)) {
                LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_RIGHT);
                JS_DefinePropertyWithTinyId(cx, obj, lm_right_str, RECT_RIGHT,
                                            INT_TO_JSVAL(bbox.right), 
                                            rect_getProperty, 
                                            rect_setProperty, 
                                            JSPROP_ENUMERATE);
            }
            else
                JS_GetProperty(cx, obj, lm_right_str, &js_val);
            break;

        case RECT_HEIGHT:
            bbox.bottom = bbox.top + (int32)val;
            LO_SetLayerBbox(layer, &bbox);
            CLEAR_LAYER_EXPANSION_POLICY(layer, LO_AUTO_EXPAND_CLIP_BOTTOM);
            if (!LM_CHECK_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_BOTTOM)) {
                LM_SET_LAYER_MODIFICATION(js_layer, LAYER_MOD_CLIP_BOTTOM);
                JS_DefinePropertyWithTinyId(cx, obj, lm_bottom_str,
                                            RECT_BOTTOM, 
                                            INT_TO_JSVAL(bbox.bottom),
                                            rect_getProperty, 
                                            rect_setProperty, 
                                            JSPROP_ENUMERATE);
            }
            else
                JS_GetProperty(cx, obj, lm_bottom_str, &js_val);
            break;
        }
    }
            
    LO_UnlockLayout();
    return rect_getProperty(cx, obj, id, vp);
}

PR_STATIC_CALLBACK(void)
rect_finalize(JSContext *cx, JSObject *obj)
{
    JSRect *js_rect;

    js_rect = JS_GetPrivate(cx, obj);
    if (!js_rect)
        return;
    DROP_BACK_COUNT(js_rect->decoder);
    JS_free(cx, js_rect);
}

static JSClass rect_class = {
    "Rect", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, rect_getProperty, rect_setProperty,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, rect_finalize
};

PR_STATIC_CALLBACK(JSBool)
RectConstructor(JSContext *cx, JSObject *obj,
     uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

/* Initialize the 'clip' property of a layer to be a Rect object */
static JSObject *
lm_init_layer_clip(MochaDecoder *decoder,
                   int32 layer_id,
                   JSObject *parent_layer_obj)
{
    JSObject *obj;
    JSContext *cx;
    JSRect *js_rect;

    cx = decoder->js_context;

    js_rect = JS_malloc(cx, sizeof *js_rect);
    if (!js_rect)
        return NULL;
    js_rect->decoder = NULL;    /* in case of error below */

    obj = JS_NewObject(cx, &rect_class, decoder->rect_prototype, 
                       parent_layer_obj);
    if (!obj || !JS_SetPrivate(cx, obj, js_rect)) {
        JS_free(cx, js_rect);
        return NULL;
    }
    if (!JS_DefineProperty(cx, parent_layer_obj, lm_clip_str, 
                           OBJECT_TO_JSVAL(obj),
                           NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY)) {
        return NULL;
    }

    js_rect->decoder = HOLD_BACK_COUNT(decoder);
    js_rect->layer_id = layer_id;
    return obj;
}

JSBool
lm_InitRectClass(MochaDecoder *decoder)
{
    JSContext *cx;
    JSObject *prototype;

    cx = decoder->js_context;
    prototype = JS_InitClass(cx, decoder->window_object, NULL, &rect_class,
                             RectConstructor, 0, rect_props, NULL, NULL, NULL);
    if (!prototype)
        return JS_FALSE;
    decoder->rect_prototype = prototype;
    return JS_TRUE;
}


static JSPrincipals **
getContainerPrincipalsAddress(JSContext *cx, JSObject *container)
{
    JSClass *classp = JS_GetClass(container);

    if (classp == &lm_window_class) {
        MochaDecoder *decoder = JS_GetPrivate(cx, container);
        return decoder ? &decoder->principals : NULL;
    } else if (classp == &lm_layer_class) {
        JSLayer *layer = JS_GetPrivate(cx, container);
        return layer ? &layer->principals : NULL;
    } else {
        return NULL;
    }
}

extern JSPrincipals *
lm_GetContainerPrincipals(JSContext *cx, JSObject *container)
{
    JSPrincipals **p = getContainerPrincipalsAddress(cx, container);
    return p ? *p : NULL;
}

extern void
lm_SetContainerPrincipals(JSContext *cx, JSObject *container, 
                          JSPrincipals *principals)
{
    JSPrincipals **p = getContainerPrincipalsAddress(cx, container);
    if (p && *p != principals) {
        if (*p) {
            JSPRINCIPALS_DROP(cx, *p);
        }
        *p = principals;
        JSPRINCIPALS_HOLD(cx, *p);
    }
}


extern JSObject *
lm_GetActiveContainer(MochaDecoder *decoder)
{
    if (decoder->active_layer_id == LO_DOCUMENT_LAYER_ID) {
        /* immediate container is the window */
        return decoder->window_object;
    } else {
        /* immediate container is a layer */
        JSObject *result;
        LO_LockLayout();
        result = LO_GetLayerMochaObjectFromId(decoder->window_context, 
                                              decoder->active_layer_id);
        LO_UnlockLayout();
        return result;
    }
}

extern JSBool
lm_GetPrincipalsCompromise(JSContext *cx, JSObject *obj)
{
    JSClass *clasp;
    MochaDecoder *decoder;
    JSLayer *js_layer;
    JSBool flag;

    clasp = JS_GetClass(obj);
    if (clasp == &lm_window_class) {
	decoder = JS_GetPrivate(cx, obj);
	flag = (JSBool)decoder->principals_compromised;
    } else if (clasp == &lm_layer_class) {
	js_layer = JS_GetPrivate(cx, obj);
	flag = (JSBool)js_layer->principals_compromised;
    } else {
	flag = JS_FALSE;
    }
    return flag;
}
