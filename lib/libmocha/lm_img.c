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
 * Image reflection and event notification
 *
 * Scott Furman, 3/30/96
 */

#include "lm.h"

#include "lo_ele.h"
#include "prtypes.h"
#include "pa_tags.h"
#include "layout.h"

#define IL_CLIENT
#include "libimg.h"             /* Image Library public API. */

enum image_array_slot {
    IMAGE_ARRAY_LENGTH = -1
};

static JSPropertySpec image_array_props[] = {
    {lm_length_str, IMAGE_ARRAY_LENGTH,
		    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {0}
};

extern JSClass lm_image_array_class;

PR_STATIC_CALLBACK(JSBool)
image_array_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSObjectArray *array;
    MochaDecoder *decoder;
    MWContext *context;
    jsint count, slot;
    LO_ImageStruct *image;
    int32 active_layer_id;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    array = JS_GetInstancePrivate(cx, obj, &lm_image_array_class, NULL);
    if (!array)
	return JS_TRUE;
    decoder = array->decoder;
    context = decoder->window_context;
    if (!context)
	return JS_TRUE;

    LO_LockLayout();
    switch (slot) {
      case IMAGE_ARRAY_LENGTH:
	active_layer_id = LM_GetActiveLayer(context);
	LM_SetActiveLayer(context, array->layer_id);
	count = LO_EnumerateImages(context, array->layer_id);
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
	image = LO_GetImageByIndex(context, array->layer_id, (uint)slot);
	if (image) {
	    *vp = OBJECT_TO_JSVAL(LM_ReflectImage(context, image, NULL,
						  array->layer_id, 
                                                  (uint)slot));
	}
	break;
    }
    LO_UnlockLayout();
    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
image_array_finalize(JSContext *cx, JSObject *obj)
{
    JSObjectArray *array;

    array = JS_GetPrivate(cx, obj);
    if (!array)
        return;
    DROP_BACK_COUNT(array->decoder);
    JS_free(cx, array);
}

JSClass lm_image_array_class = {
    "ImageArray", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    image_array_getProperty, image_array_getProperty, JS_EnumerateStub,
    JS_ResolveStub, JS_ConvertStub, image_array_finalize
};

enum image_slot {
    IMAGE_NAME              = -2,
    IMAGE_SRC               = -3,
    IMAGE_LOWSRC            = -4,
    IMAGE_X                 = -5,
    IMAGE_Y                 = -6,
    IMAGE_HEIGHT            = -7,
    IMAGE_WIDTH             = -8,
    IMAGE_BORDER            = -9,
    IMAGE_VSPACE            = -10,
    IMAGE_HSPACE            = -11,
    IMAGE_COMPLETE          = -12
};

static JSPropertySpec image_props[] = {
    {"name",                IMAGE_NAME,         JSPROP_ENUMERATE | JSPROP_READONLY},
    {"src",                 IMAGE_SRC,          JSPROP_ENUMERATE},
    {"lowsrc",              IMAGE_LOWSRC,       JSPROP_ENUMERATE},
    {"x",                   IMAGE_X,            JSPROP_ENUMERATE | JSPROP_READONLY},
    {"y",                   IMAGE_Y,            JSPROP_ENUMERATE | JSPROP_READONLY},
    {"height",              IMAGE_HEIGHT,       JSPROP_ENUMERATE | JSPROP_READONLY},
    {"width",               IMAGE_WIDTH,        JSPROP_ENUMERATE | JSPROP_READONLY},
    {"border",              IMAGE_BORDER,       JSPROP_ENUMERATE | JSPROP_READONLY},
    {"vspace",              IMAGE_VSPACE,       JSPROP_ENUMERATE | JSPROP_READONLY},
    {"hspace",              IMAGE_HSPACE,       JSPROP_ENUMERATE | JSPROP_READONLY},
    {"complete",            IMAGE_COMPLETE,     JSPROP_ENUMERATE | JSPROP_READONLY},
    {0}
};

/*
 * Base image element type.
 */
typedef struct JSImage {
    JSEventReceiver	    receiver;
    MochaDecoder           *decoder;
    LO_ImageStruct         *image_data;     /* 0 unless made by new Image() */
    uint8                   pending_events;
    int32                   layer_id;
    uint                    index;
    JSBool            complete;       /* Finished loading or aborted */
    JSString        *name;
} JSImage;

#define GET_IMAGE_DATA(context, image)                                        \
    ((image)->image_data ? (image)->image_data                                \
     : LO_GetImageByIndex((context), (image)->layer_id, (image)->index))

extern JSClass lm_image_class;

/*
 * Force the mozilla event queue to flush to make sure any image-set-src
 *   events have been processed
 */
PR_STATIC_CALLBACK(void)
lm_img_src_sync(void * data)
{
}


PR_STATIC_CALLBACK(JSBool)
image_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSImage *image;
    LO_ImageStruct *image_data;
    enum image_slot image_slot;
    JSString *str;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    image = JS_GetInstancePrivate(cx, obj, &lm_image_class, NULL);
    if (!image)
	return JS_TRUE;
    image_data = GET_IMAGE_DATA(image->decoder->window_context, image);
    if (!image_data)
        return JS_TRUE;      /* Try to handle this case gracefully. */

    image_slot = slot;
    if (image_slot == IMAGE_SRC || image_slot == IMAGE_LOWSRC) {
        if (!lm_CheckPermissions(cx, obj, JSTARGET_UNIVERSAL_BROWSER_READ))
            return JS_FALSE;
    }

    switch (image_slot) {
    case IMAGE_NAME:
	if (image->name)
	    *vp = STRING_TO_JSVAL(image->name);
	else
	    *vp = JSVAL_NULL;
        break;

    case IMAGE_SRC:
	if (image_data->pending_mocha_event) {
	    ET_moz_CallFunction(lm_img_src_sync, NULL);
	    image_data->pending_mocha_event = PR_FALSE;
	}
	str = JS_NewStringCopyZ(cx, (char*)image_data->image_url);
	if (!str)
	    return JS_FALSE;
	*vp = STRING_TO_JSVAL(str);
	break;

    case IMAGE_LOWSRC:
	if (image_data->pending_mocha_event) {
	    ET_moz_CallFunction(lm_img_src_sync, NULL);
	    image_data->pending_mocha_event = PR_FALSE;
	}
	str = JS_NewStringCopyZ(cx, (char*)image_data->lowres_image_url);
	if (!str)
	    return JS_FALSE;
	*vp = STRING_TO_JSVAL(str);
	break;

    case IMAGE_X:
        *vp = INT_TO_JSVAL(image_data->x + image_data->x_offset);
        break;

    case IMAGE_Y:
        *vp = INT_TO_JSVAL(image_data->y + image_data->y_offset);
        break;

    case IMAGE_HEIGHT:
        *vp = INT_TO_JSVAL(image_data->height);
        break;

    case IMAGE_WIDTH:
        *vp = INT_TO_JSVAL(image_data->width);
        break;

    case IMAGE_BORDER:
        *vp = INT_TO_JSVAL(image_data->border_width);
        break;

    case IMAGE_HSPACE:
        *vp = INT_TO_JSVAL(image_data->border_horiz_space);
        break;

    case IMAGE_VSPACE:
        *vp = INT_TO_JSVAL(image_data->border_vert_space);
        break;

    case IMAGE_COMPLETE:
        *vp = BOOLEAN_TO_JSVAL(image->complete);
        break;

    default:
        /* Don't mess with a user-defined or method property. */
        return JS_TRUE;
    }

    return JS_TRUE;
}

static JSBool
image_set_src(JSImage *image, const char *str, LO_ImageStruct *image_data)
{
    MochaDecoder *decoder;
    MWContext *context;
    IL_GroupContext *img_cx;
    
    decoder = image->decoder;
    context = decoder->window_context;
    img_cx = decoder->image_context;

    if (!context) return JS_TRUE;

    image_data->pending_mocha_event = PR_TRUE;
    image_data->image_attr->attrmask |= LO_ATTR_MOCHA_IMAGE;
    
    ET_il_GetImage(str, context, img_cx, image_data, NET_DONT_RELOAD);

    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
image_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSBool ok;
    JSImage *image;
    MochaDecoder *decoder;
    MWContext *context;
    LO_ImageStruct *image_data;
    enum image_slot image_slot;
    const char *url;
    jsint slot;

    image = JS_GetInstancePrivate(cx, obj, &lm_image_class, NULL);
    if (!image)
	return JS_TRUE;
    decoder = image->decoder;
    context = decoder->window_context;
    if (!context) 
	return JS_TRUE;
    
    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    image_data = GET_IMAGE_DATA(context, image);
    if (!image_data)
        return JS_TRUE;      /* Try to handle this case gracefully. */

    image_slot = slot;
    switch (image_slot) {
    case IMAGE_SRC:
    case IMAGE_LOWSRC:
        if (!lm_CheckPermissions(cx, obj, JSTARGET_UNIVERSAL_BROWSER_WRITE))
            return JS_FALSE;

        if (JSVAL_IS_NULL(*vp)) {
            url = NULL;
        } else {
            if (!JSVAL_IS_STRING(*vp) &&
                !JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp))
                return JS_FALSE;
                
            url = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
            url = lm_CheckURL(cx, url, JS_TRUE);  /* will allocate new string */
            if (!url)
                return JS_FALSE;
        }

        if (image_slot == IMAGE_SRC) {
            ok = image_set_src(image, url, image_data);
        } else if (url) {
	    ok = lm_SaveParamString(cx, &image_data->lowres_image_url, url);
        }

	if (url)
	    XP_FREE((void *) url);

        if (!ok)
            return JS_FALSE;

	/*
	 * don't call image_getProperty so that we don't immediately
	 *   turn around and block
	 */
	return JS_TRUE;
        break;

    case IMAGE_NAME:
    case IMAGE_X:
    case IMAGE_Y:
    case IMAGE_HEIGHT:
    case IMAGE_WIDTH:
    case IMAGE_BORDER:
    case IMAGE_VSPACE:
    case IMAGE_HSPACE:
    case IMAGE_COMPLETE:
	/* These are immutable. */
	break;
    }

    return image_getProperty(cx, obj, id, vp);
}

PR_STATIC_CALLBACK(void)
image_finalize(JSContext *cx, JSObject *obj)
{
    JSImage *image;
    LO_ImageStruct *image_data;
    MochaDecoder *decoder;
    MWContext *context;

    image = JS_GetPrivate(cx, obj);
    if (!image)
	return;

    image_data = image->image_data;
    decoder = image->decoder;
    context = decoder->window_context;
    if (image_data) {
        /* If this is a layer background image or a reflection of an
           existing layout image, then layout will take care of
           destroying the image.  For anonymous images, however, 
           we need to handle destruction here. */
        if (!image_data->layer) {
            ET_PostFreeImageElement(context, image_data);
            XP_FREE(image_data->image_attr);
            XP_FREE(image_data);
        }
    } else {
	if (context) {
	    LO_LockLayout();
	    image_data = LO_GetImageByIndex(context, image->layer_id,
                                            image->index);
	    if (image_data && image_data->mocha_object == obj)
		image_data->mocha_object = NULL;
	    LO_UnlockLayout();
	}
    }
    DROP_BACK_COUNT(decoder);

    JS_UnlockGCThing(cx, image->name);
    JS_free(cx, image);
}

JSClass lm_image_class = {
    "Image", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, image_getProperty, image_setProperty,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, image_finalize
};

/* Fill in native, private part of JS image */
static JSImage *
init_image_object(JSContext *cx, JSObject *obj, LO_ImageStruct *image_data)
{
    JSImage *image;
    MochaDecoder *decoder;
    
    image = JS_malloc(cx, sizeof *image);
    if (!image)
        return NULL;
    XP_BZERO(image, sizeof *image);

    image->image_data = image_data;
    decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    image->decoder = HOLD_BACK_COUNT(decoder);
    image_data->mocha_object = obj;

    /* Events are never blocked for anonymous images
       since there is no associated layout. */
    image->pending_events = PR_BIT(LM_IMGUNBLOCK);
    if (!JS_SetPrivate(cx, obj, image))
	return NULL;

    return image;
}

JSObject *
lm_NewImage(JSContext *cx,
            LO_ImageStruct *image_data)
{
    JSObject *obj, *outer_obj;
    MochaDecoder *decoder;
    decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    outer_obj = lm_GetOuterObject(decoder);

    obj = JS_NewObject(cx, &lm_image_class, decoder->image_prototype, 
		       outer_obj);
    if (!init_image_object(cx, obj, image_data))
        return NULL;
    
    return obj;
}

PR_STATIC_CALLBACK(JSBool)
Image(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    jsint width, height;
    LO_ImageStruct *image_data;

    XP_ASSERT(JS_InstanceOf(cx, obj, &lm_image_class, NULL));

    height = width = 0;

    if (argc > 0) {
	if (argc != 2) {
	    JS_ReportError(cx, lm_argc_err_str);
	    return JS_FALSE;
	}
        if (!JSVAL_IS_INT(argv[0]) ||
            !JSVAL_IS_INT(argv[1])) {
            return JS_FALSE;
        }
	width = JSVAL_TO_INT(argv[0]);
	height = JSVAL_TO_INT(argv[1]);
    }

    /* Allocate dummy layout structure.  This is not really
       used by layout, but the front-ends and the imagelib
       need it as a handle on an image instance. */
    image_data = XP_NEW_ZAP(LO_ImageStruct);
    if (!image_data) {
	JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    image_data->image_attr = XP_NEW_ZAP(LO_ImageAttr);
    if (!image_data->image_attr) {
        XP_FREE(image_data);
	JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }

    image_data->type = LO_IMAGE;

    /* Fake layout ID, guaranteed not to match any real layout element */
    image_data->ele_id = -1;

    if (!init_image_object(cx, obj, image_data)) {
	XP_FREE(image_data->image_attr);
	XP_FREE(image_data);
        return JS_FALSE;
    }

    /* Process arguments */

    /* Width & Height */
    if (argc == 2) {
        image_data->width  = (int)width;
        image_data->height = (int)height;
    }

    return JS_TRUE;
}

static JSObject *
reflect_image_array(MochaDecoder *decoder, JSObject *document)
{
    JSContext *cx;
    JSObjectArray *array;
    JSObject *obj;
    JSDocument *doc;

    cx = decoder->js_context;
    doc = JS_GetPrivate(cx, document);
    if (!doc)
	return NULL;

    array = JS_malloc(cx, sizeof *array);
    if (!array)
	return NULL;
    XP_BZERO(array, sizeof *array);

    obj = JS_NewObject(cx, &lm_image_array_class, NULL, document);
    if (!obj || !JS_SetPrivate(cx, obj, array)) {
        LM_PutMochaDecoder(decoder);
        return NULL;
    }

    if (!JS_DefineProperties(cx, obj, image_array_props))
	return NULL;

    array->decoder = HOLD_BACK_COUNT(decoder);
    array->layer_id = doc->layer_id;
    return obj;
}

JSObject *
lm_GetImageArray(MochaDecoder *decoder, JSObject *document)
{
    JSObject *obj;
    JSDocument *doc;

    doc = JS_GetPrivate(decoder->js_context, document);
    if (!doc)
	return NULL;

    obj = doc->images;
    if (obj)
	return obj;
    obj = reflect_image_array(decoder, document);
    doc->images = obj;
    return obj;
}

JSObject *
LM_ReflectImage(MWContext *context, LO_ImageStruct *image_data, 
			    PA_Tag * tag, int32 layer_id, uint index)
{
    JSObject *obj, *array_obj, *outer_obj, *document;
    MochaDecoder *decoder;
    JSContext *cx;
    JSImage *image;
    PA_Block name = NULL;
    lo_TopState *top_state;
    PRHashTable *map;

    image_data = LO_GetImageByIndex(context, layer_id, index);
    XP_ASSERT(image_data);
    if (! image_data)
      return NULL;

    obj = image_data->mocha_object;
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
                             LM_GET_MAPPING_KEY(LM_IMAGES, layer_id, index));
        if (obj) {
            image_data->mocha_object = obj;
            goto done;
        }
    }

    /* Get the document object that will hold this link */
    document = lm_GetDocumentFromLayerId(decoder, layer_id);
    if (!document)
        goto done;

    array_obj = lm_GetImageArray(decoder, document);
    if (!array_obj)
	goto done;

    image = JS_malloc(cx, sizeof *image);
    if (!image)
	goto done;

    XP_BZERO(image, sizeof *image);

    /* if we got a tag passed in try to get the name out of there */
    name = lo_FetchParamValue(context, tag, PARAM_NAME);

    outer_obj = lm_GetOuterObject(decoder);

    obj = JS_NewObject(cx, &lm_image_class, decoder->image_prototype,
		       outer_obj);
    if (!obj || !JS_SetPrivate(cx, obj, image)) {
	JS_free(cx, image);
	goto done;
    }

    if (name) {
	JSObject *doc_obj;
	extern JSClass lm_form_class;

	if (!JS_DefineProperty(cx, outer_obj, (const char *) name, 
			       OBJECT_TO_JSVAL(obj), NULL, NULL,
			       JSPROP_ENUMERATE|JSPROP_READONLY)) {
	    obj = NULL;
	    goto done;
	}
	/* XXX backward compatibility with 3.0 bug: lo_BlockedImageLayout
	       would eagerly reflect images outside of any active form, so
	       they'd end up in document scope. */
	if (JS_GetClass(outer_obj) == &lm_form_class &&
	    (doc_obj = JS_GetParent(cx, outer_obj)) != NULL &&
	    !JS_DefineProperty(cx, doc_obj, (const char *) name, 
			       OBJECT_TO_JSVAL(obj), NULL, NULL,
			       JSPROP_ENUMERATE|JSPROP_READONLY)) {
	    obj = NULL;
	    goto done;
	}
    }

    image->decoder = HOLD_BACK_COUNT(decoder);
    image->index = index;
    image->layer_id = layer_id;
    image->name = JS_NewStringCopyZ(cx, (const char *) name);
    if (!JS_LockGCThing(cx, image->name)) {
	obj = NULL;
	goto done;
    }
    image_data->mocha_object = obj;

    if (!lm_AddObjectToArray(cx, array_obj, (const char *) name, 
			     index, obj)) {
	obj = NULL;
	goto done;
    }

    /* Put it in the index to object hash table */
    map = lm_GetIdToObjectMap(decoder);
    if (map)
        PR_HashTableAdd(map,
                        LM_GET_MAPPING_KEY(LM_IMAGES, layer_id, index),
                        obj);

    /* OK, we've got our image, see if there are any event handlers
     *   defined with it
     */
    if(tag) {
	PA_Block onload   = lo_FetchParamValue(context, tag, PARAM_ONLOAD);
	PA_Block onabort  = lo_FetchParamValue(context, tag, PARAM_ONABORT);
	PA_Block onerror  = lo_FetchParamValue(context, tag, PARAM_ONERROR);
	PA_Block onmousedown = lo_FetchParamValue(context, tag, PARAM_ONMOUSEDOWN);
	PA_Block onmouseup = lo_FetchParamValue(context, tag, PARAM_ONMOUSEUP);
	PA_Block id	  = lo_FetchParamValue(context, tag, PARAM_ID);

	/* don't hold the layout lock across compiles */
	LO_UnlockLayout();

	if (onload != NULL) {
	    (void) lm_CompileEventHandler(decoder, id, tag->data,
					  tag->newline_count, obj,
					  PARAM_ONLOAD, onload);
	    PA_FREE(onload);
	}

	if (onabort != NULL) {
	    (void) lm_CompileEventHandler(decoder, id, tag->data,
					  tag->newline_count, obj,
				          PARAM_ONABORT, onabort);
	    PA_FREE(onabort);
	}

	if (onerror != NULL) {
	    (void) lm_CompileEventHandler(decoder, id, tag->data,
					  tag->newline_count, obj,
					  PARAM_ONERROR, onerror);
	    PA_FREE(onerror);
	}
	if (onmousedown != NULL) {
	    (void) lm_CompileEventHandler(decoder, id, tag->data,
					  tag->newline_count, obj,
					  PARAM_ONMOUSEDOWN, onmousedown);
	    PA_FREE(onmousedown);
	}
	if (onmouseup != NULL) {
	    (void) lm_CompileEventHandler(decoder, id, tag->data,
					  tag->newline_count, obj,
					  PARAM_ONMOUSEUP, onmouseup);
	    PA_FREE(onmouseup);
	}

	if (id)
	    PA_FREE(id);

	LO_LockLayout();
    }

done:

    if(name)
	PA_FREE(name);
    LM_PutMochaDecoder(decoder);

    return obj;
}

void
lm_ProcessImageEvent(MWContext *context, JSObject *obj,
                  LM_ImageEvent event)
{
    uint event_mask;
    jsval result;
    JSImage *image;

    image = JS_GetPrivate(context->mocha_context, obj);
    if (!image)
        return;

    image->pending_events |= PR_BIT(event);

    /* Special event used to trigger deferred events */
    if (! (image->pending_events & PR_BIT(LM_IMGUNBLOCK)))
        return;

    for (event = LM_IMGLOAD; event <= LM_LASTEVENT; event++) {
        event_mask = PR_BIT(event);
        if (image->pending_events & event_mask) {

	    JSEvent *pEvent;
	    pEvent=XP_NEW_ZAP(JSEvent);

            image->pending_events &= ~event_mask;
            switch (event) {
            case LM_IMGLOAD:
                pEvent->type = EVENT_LOAD;
                image->complete = JS_TRUE;
                break;
            case LM_IMGABORT:
                pEvent->type = EVENT_ABORT;
                image->complete = JS_TRUE;
                break;
            case LM_IMGERROR:
                pEvent->type = EVENT_ERROR;
                image->complete = JS_TRUE;
                break;
            default:
                XP_ABORT(("Unknown image event"));
            }

            lm_SendEvent(context, obj, pEvent, &result);
        }
    }
}

JSBool
lm_InitImageClass(MochaDecoder *decoder)
{
    JSContext *cx;
    JSObject *prototype;

    cx = decoder->js_context;
    prototype = JS_InitClass(cx, decoder->window_object, 
			     decoder->event_receiver_prototype, &lm_image_class,
			     Image, 0, image_props, NULL, NULL, NULL);
    if (!prototype)
	return JS_FALSE;
    decoder->image_prototype = prototype;
    return JS_TRUE;
}

/* Create an image context for anonymous images and attach it to the specified
   mocha decoder. */
JSBool
lm_NewImageContext(MWContext *context, MochaDecoder *decoder)
{
    IL_GroupContext *img_cx;
    IMGCB* img_cb;
    JMCException *exc = NULL;
    
    if (!decoder->image_context) {
        img_cb = IMGCBFactory_Create(&exc); /* JMC Module */
        if (exc) {
            JMC_DELETE_EXCEPTION(&exc); /* XXX Should really return
                                           exception */
            JS_ReportOutOfMemory(decoder->js_context);
            return JS_FALSE;
        }

        /* Create an Image Group Context.  IL_NewGroupContext augments the
           reference count for the JMC callback interface.  The opaque argument
           to IL_NewGroupContext is the Front End's display context, which will
           be passed back to all the Image Library's FE callbacks. */
        img_cx = IL_NewGroupContext((void*)context, (IMGCBIF *)img_cb);
        if (!img_cx) {
            JS_ReportOutOfMemory(decoder->js_context);
            return JS_FALSE;
        }

        /* Attach the IL_GroupContext to the mocha decoder. */
        decoder->image_context = img_cx;

		/* Allow the context to observe the decoder's image context. */
		ET_il_SetGroupObserver(context, decoder->image_context, context, JS_TRUE);
    }
    return JS_TRUE;
}
