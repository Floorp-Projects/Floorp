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
 * JS reflection of the HTML FORM elements.
 *
 * Brendan Eich, 9/27/95
 */
#include "lm.h"
#include "lo_ele.h"
#include "mkutils.h"
#include "layout.h"
#include "pa_tags.h"
#include "shist.h"

enum form_array_slot {
    FORM_ARRAY_LENGTH = -1
};

static JSPropertySpec form_array_props[] = {
    {lm_length_str, FORM_ARRAY_LENGTH,
		    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {0}
};

extern JSClass lm_form_array_class;

PR_STATIC_CALLBACK(JSBool)
form_array_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSObjectArray *array;
    MochaDecoder *decoder;
    MWContext *context;
    jsint count, slot;
    lo_FormData *form_data;
    int32 active_layer_id;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    array = JS_GetInstancePrivate(cx, obj, &lm_form_array_class, NULL);
    if (!array)
	return JS_TRUE;
    decoder = array->decoder;
    context = decoder->window_context;
    if (!context) return JS_TRUE;

    LO_LockLayout();
    switch (slot) {
      case FORM_ARRAY_LENGTH:
	active_layer_id = LM_GetActiveLayer(context);
	LM_SetActiveLayer(context, array->layer_id);
        count = LO_EnumerateForms(context, array->layer_id);
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
        if (slot >= array->length)
	    array->length = slot + 1;
        /* NB: form IDs start at 1, not 0. */
        form_data = LO_GetFormDataByID(context, array->layer_id, slot + 1);
        if (form_data)
	    *vp = OBJECT_TO_JSVAL(LM_ReflectForm(context, form_data, NULL,
                                                 array->layer_id, 0));
        break;
    }
    LO_UnlockLayout();
    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
form_array_finalize(JSContext *cx, JSObject *obj)
{
    JSObjectArray *array;

    array = JS_GetPrivate(cx, obj);
    if (!array)
	return;
    DROP_BACK_COUNT(array->decoder);
    JS_free(cx, array);
}

JSClass lm_form_array_class = {
    "FormArray", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    form_array_getProperty, form_array_getProperty, JS_EnumerateStub,
    JS_ResolveStub, JS_ConvertStub, form_array_finalize
};

PR_STATIC_CALLBACK(JSBool)
FormArray(JSContext *cx, JSObject *obj,
	  uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

JSObject *
lm_GetFormArray(MochaDecoder *decoder, JSObject *document)
{
    JSObject *obj;
    JSContext *cx;
    JSObjectArray *array;
    JSDocument *doc;

    cx = decoder->js_context;
    doc = JS_GetPrivate(cx, document);
    if (!doc)
	return NULL;

    obj = doc->forms;
    if (obj)
	return obj;

    array = JS_malloc(cx, sizeof *array);
    if (!array)
	return NULL;
    XP_BZERO(array, sizeof *array);

    obj = JS_NewObject(cx, &lm_form_array_class, NULL, document);
    if (!obj || !JS_SetPrivate(cx, obj, array)) {
	JS_free(cx, array);
	return NULL;
    }

    if (!JS_DefineProperties(cx, obj, form_array_props))
	return NULL;

    array->decoder = HOLD_BACK_COUNT(decoder);
    array->layer_id = doc->layer_id;
    doc->forms = obj;
    return obj;
}

/*
 * Forms can be treated as arrays of their elements, so all named properties
 * have negative slot numbers < -1.
 */
enum form_slot {
    FORM_LENGTH         = -1,
    FORM_NAME           = -2,
    FORM_ELEMENTS       = -3,
    FORM_METHOD         = -4,
    FORM_ACTION         = -5,
    FORM_ENCODING       = -6,
    FORM_TARGET         = -7
};

static char form_action_str[] = "action";

static JSPropertySpec form_props[] = {
    {"length",          FORM_LENGTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
    {"name",            FORM_NAME,      JSPROP_ENUMERATE},
    {"elements",        FORM_ELEMENTS,  JSPROP_ENUMERATE | JSPROP_READONLY},
    {"method",          FORM_METHOD,    JSPROP_ENUMERATE},
    {form_action_str,   FORM_ACTION,    JSPROP_ENUMERATE},
    {"encoding",        FORM_ENCODING,  JSPROP_ENUMERATE},
    {"target",          FORM_TARGET,    JSPROP_ENUMERATE},
    {0}
};

typedef struct JSForm {
    JSObjectArray   object_array;
    JSObject	    *form_object;
    uint32	    form_event_mask;
    int32           layer_id;
    intn            form_id;
    JSString        *name;
    PRHashTable     *form_element_map;   /* Map from element id to object */
} JSForm;

#define form_decoder	object_array.decoder
#define form_length     object_array.length

typedef struct FormMethodMap {
    char                *name;
    uint                code;
} FormMethodMap;

static FormMethodMap form_method_map[] = {
    {"get",             FORM_METHOD_GET},
    {"post",            FORM_METHOD_POST},
    {0}
};

static char *
form_method_name(uint code)
{
    FormMethodMap *mm;

    for (mm = form_method_map; mm->name; mm++)
	if (mm->code == code)
	    return mm->name;
    return "unknown";
}

static int
form_method_code(const char *name)
{
    FormMethodMap *mm;

    for (mm = form_method_map; mm->name; mm++)
	if (XP_STRCASECMP(mm->name, name) == 0)
	    return mm->code;
    return -1;
}

extern JSClass lm_form_class;

PR_STATIC_CALLBACK(JSBool)
form_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSForm *form;
    MWContext *context;
    lo_FormData *form_data;
    jsint count;
    JSString *str;
    LO_Element **ele_list;
    LO_FormElementStruct *first_ele;
    uint first_index;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    form = JS_GetInstancePrivate(cx, obj, &lm_form_class, NULL);
    if (!form)
	return JS_TRUE;
    context = form->form_decoder->window_context;
    if (!context) return JS_TRUE;

    LO_LockLayout();
    form_data = LO_GetFormDataByID(context, form->layer_id, form->form_id);
    if (!form_data)
	goto good;

    switch (slot) {
      case FORM_LENGTH:
        count = LO_EnumerateFormElements(context, form_data);
        if (count > form->form_length)
            form->form_length = count;
        *vp = INT_TO_JSVAL(form->form_length);
        goto good;

      case FORM_NAME:
	str = lm_LocalEncodingToStr(context, (char *)form_data->name);
        break;

      case FORM_ELEMENTS:
        *vp = OBJECT_TO_JSVAL(form->form_object);
        goto good;

      case FORM_METHOD:
        str = JS_NewStringCopyZ(cx, form_method_name(form_data->method));
        break;

      case FORM_ACTION:
        str = JS_NewStringCopyZ(cx, (char *)form_data->action);
        break;

      case FORM_ENCODING:
        str = JS_NewStringCopyZ(cx, (char *)form_data->encoding);
        break;

      case FORM_TARGET:
        str = JS_NewStringCopyZ(cx, (char *)form_data->window_target);
        break;

      default:
        if ((uint)slot >= (uint)form_data->form_ele_cnt) {
	    /* Don't mess with a user-defined or method property. */
	    goto good;
        }

	PA_LOCK(ele_list, LO_Element **, form_data->form_elements);
	first_ele = (LO_FormElementStruct *)ele_list[0];
	first_index = (uint)first_ele->element_index;
	PA_UNLOCK(form_data->form_elements);

	*vp = OBJECT_TO_JSVAL(LM_ReflectFormElement(context, form->layer_id,
                                                    form->form_id,
                                                    first_index + slot, NULL));
        goto good;
    }

    LO_UnlockLayout();

    /* Common tail code for string-type properties. */
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;

good:
    LO_UnlockLayout();
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
form_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSForm *form;
    MWContext *context;
    lo_FormData *form_data;
    const char *value;
    jsint slot;

    form = JS_GetInstancePrivate(cx, obj, &lm_form_class, NULL);
    if (!form)
	return JS_TRUE;
    context = form->form_decoder->window_context;
    if (!context)
	return JS_TRUE;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    LO_LockLayout();
    form_data = LO_GetFormDataByID(context, form->layer_id, form->form_id);
    if (!form_data)
	goto good;

    if (!JSVAL_IS_STRING(*vp) &&
	!JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp)) {
	goto bad;
    }

    value = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
    switch (slot) {
      case FORM_METHOD:
	form_data->method = form_method_code(value);
	break;

      case FORM_ACTION:
	value = lm_CheckURL(cx, value, JS_TRUE);
	if (!value)
	    goto bad;
	if (!lm_SaveParamString(cx, &form_data->action, value))
	    goto bad;
	XP_FREE((char *)value);
	break;

      case FORM_ENCODING:
	if (!lm_SaveParamString(cx, &form_data->encoding, value))
	    goto bad;
	break;

      case FORM_TARGET:
	if (!lm_CheckWindowName(cx, value))
	    goto bad;
	if (!lm_SaveParamString(cx, &form_data->window_target, value))
	    goto bad;
	break;
    }

    LO_UnlockLayout();
    return form_getProperty(cx, obj, id, vp);
good:
    LO_UnlockLayout();
    return JS_TRUE;
bad:
    LO_UnlockLayout();
    return JS_FALSE;

}

PR_STATIC_CALLBACK(JSBool)
form_list_properties(JSContext *cx, JSObject *obj)
{
    JSForm *form;
    MWContext *context;
    lo_FormData *form_data;

    form = JS_GetPrivate(cx, obj);
    if (!form)
	return JS_TRUE;
    context = form->form_decoder->window_context;
    if (!context)
	return JS_TRUE;

    LO_LockLayout();
    form_data = LO_GetFormDataByID(context, form->layer_id, form->form_id);
    if (!form_data) {
	LO_UnlockLayout();
	return JS_TRUE;
    }
    /* XXX should return FALSE on reflection error */
    (void) LO_EnumerateFormElements(context, form_data);
    LO_UnlockLayout();
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
form_resolve_name(JSContext *cx, JSObject *obj, jsval id)
{
    JSForm *form;
    MWContext *context;
    lo_FormData *form_data;
    LO_Element **ele_list;
    LO_FormElementStruct *form_ele;
    lo_FormElementMinimalData *min_data;
    int32 i;
    const char * name;

    if (!JSVAL_IS_STRING(id))
	return JS_TRUE;

    name = JS_GetStringBytes(JSVAL_TO_STRING(id));

    form = JS_GetPrivate(cx, obj);
    if (!form)
	return JS_TRUE;
    context = form->form_decoder->window_context;
    if (!context)
	return JS_TRUE;

    LO_LockLayout();
    form_data = LO_GetFormDataByID(context, form->layer_id, form->form_id);
    if (!form_data) {
	LO_UnlockLayout();
	return JS_TRUE;
    }

    PA_LOCK(ele_list, LO_Element **, form_data->form_elements);
    for (i = 0; i < form_data->form_ele_cnt; i++) {
	if (ele_list[i]->type != LO_FORM_ELE)
	    continue;
	form_ele = (LO_FormElementStruct *)ele_list[i];
	if (!form_ele->element_data)
	    continue;
	min_data = &form_ele->element_data->ele_minimal;
	if (min_data->name && XP_STRCMP((char *)min_data->name, name) == 0)
	    (void) LM_ReflectFormElement(context, form->layer_id,
                                         form->form_id,
					 form_ele->element_index, NULL);
    }

    PA_UNLOCK(form_data->form_elements);
    LO_UnlockLayout();
    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
form_finalize(JSContext *cx, JSObject *obj)
{
    JSForm *form;
    MochaDecoder *decoder;
    MWContext *context;
    lo_FormData *form_data;

    form = JS_GetPrivate(cx, obj);
    if (!form)
	return;
    decoder = form->form_decoder;
    context = decoder->window_context;
    if (context) {
	LO_LockLayout();
	form_data = LO_GetFormDataByID(context, form->layer_id, form->form_id);
	if (form_data && form_data->mocha_object == obj)
	    form_data->mocha_object = NULL;
	LO_UnlockLayout();
    }
    DROP_BACK_COUNT(decoder);
    if (form->form_element_map)
        PR_HashTableDestroy(form->form_element_map);
    JS_free(cx, form);
}

JSClass lm_form_class = {
    "Form", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, form_getProperty, form_setProperty,
    form_list_properties, form_resolve_name, JS_ConvertStub, form_finalize
};

PR_STATIC_CALLBACK(JSBool)
Form(JSContext *cx, JSObject *obj,
     uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool
form_native_prolog(JSContext *cx, JSObject *obj, jsval *argv,
		   JSForm **formp, lo_FormData **form_datap)
{
    JSForm *form;
    MochaDecoder *decoder;
    lo_FormData *form_data;

    if (!JS_InstanceOf(cx, obj, &lm_form_class, argv))
        return JS_FALSE;
    form = JS_GetPrivate(cx, obj);
    if (!form) {
	*formp = NULL;
	*form_datap = NULL;
	return JS_TRUE;
    }
    decoder = form->form_decoder;
    form_data = decoder->window_context
		? LO_GetFormDataByID(decoder->window_context,
                                     form->layer_id,
                                     form->form_id)
		: 0;
    *formp = form;
    *form_datap = form_data;
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
form_reset(JSContext *cx, JSObject *obj,
	   uint argc, jsval *argv, jsval *rval)
{
    JSForm *form;
    lo_FormData *form_data;
    LO_Element **ele_list;

    if (!form_native_prolog(cx, obj, argv, &form, &form_data))
        return JS_FALSE;
    if (form_data && form_data->form_ele_cnt > 0) {
	/* There is no form LO_Element; use the first thing in the form. */
	PA_LOCK(ele_list, LO_Element **, form_data->form_elements);
	ET_lo_ResetForm(form->form_decoder->window_context, ele_list[0]);
	PA_UNLOCK(form_data->form_elements);
    }
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
form_submit(JSContext *cx, JSObject *obj,
	    uint argc, jsval *argv, jsval *rval)
{
    JSForm *form;
    lo_FormData *form_data;
    JSBool ok;
    MochaDecoder *decoder;
    MWContext *context;
    LO_Element **ele_list, *element;

    if (!form_native_prolog(cx, obj, argv, &form, &form_data))
        return JS_FALSE;
    if (!form_data)
        return JS_TRUE;

    switch (NET_URL_Type((const char *) form_data->action)) {
      case MAILTO_TYPE_URL:
      case NEWS_TYPE_URL:
	/* only OK if we are a signed script */
	ok = lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_SEND_MAIL);
	break;
      default:
	ok = JS_TRUE;
	break;
    }

    if (!ok) {
	/* XXX silently fail this mailto: or news: form post. */
	return JS_TRUE;
    }

    if (form_data->form_ele_cnt > 0) {
	decoder = form->form_decoder;
	context = decoder->window_context;
	PA_LOCK(ele_list, LO_Element **, form_data->form_elements);
	element = ele_list[0];
	PA_UNLOCK(form_data->form_elements);
	ET_fe_SubmitInputElement(context, element);

    }

    return JS_TRUE;
}

static JSFunctionSpec form_methods[] = {
    {"reset",	form_reset,	0},
    {"submit",	form_submit,	0},
    {0}
};

static JSBool
form_event(MWContext *context, LO_Element *element, JSEvent *event, char *method)
{
    lo_FormData *form_data;
    JSObject *obj;
    JSForm *form;
    JSBool ok;
    jsval rval;

    if (element->type != LO_FORM_ELE &&
	(element->type != LO_IMAGE || !element->lo_image.image_attr)) {
        LO_UnlockLayout();
	return JS_TRUE;
    }
    form_data = LO_GetFormDataByID(context,
				   (element->type == LO_IMAGE)
                                   ? element->lo_image.image_attr->layer_id
                                   : element->lo_form.layer_id,
				   (element->type == LO_IMAGE)
				   ? element->lo_image.image_attr->form_id
				   : element->lo_form.form_id);
    if (!form_data || !(obj = form_data->mocha_object)) {
        LO_UnlockLayout();
	return JS_TRUE;
    }

    
    LO_UnlockLayout();

    form = JS_GetPrivate(context->mocha_context, obj);
    if (!form) 
	return JS_TRUE;

    if (form->form_event_mask & event->type)
	return JS_TRUE;

    form->form_event_mask |= event->type;
    ok = lm_SendEvent(context, obj, event, &rval);
    form->form_event_mask &= ~event->type;

    if (ok && rval == JSVAL_FALSE)
	return JS_FALSE;
    return JS_TRUE;
}

JSBool
lm_SendOnReset(MWContext *context, JSEvent *event, LO_Element *element)
{
    return form_event(context, element, event, lm_onReset_str);
}

JSBool
lm_SendOnSubmit(MWContext *context, JSEvent *event, LO_Element *element)
{
    return form_event(context, element, event, lm_onSubmit_str);
}

#ifdef DEBUG_brendan
static char *
form_name(lo_FormData *form_data)
{
    static char buf[20];

    if (form_data->name)
	return (char *)form_data->name;
    PR_snprintf(buf, sizeof buf, "$form%d", form_data->id - 1);
    return buf;
}
#endif

JSObject *
LM_ReflectForm(MWContext *context, lo_FormData *form_data, PA_Tag * tag,
               int32 layer_id, uint index)
{
    JSObject *obj, *array_obj, *prototype, *document;
    MochaDecoder *decoder;
    JSContext *cx;
    JSForm *form;
    JSBool ok;
    lo_TopState *top_state;
    PRHashTable *map;

    /* if we got passed an index get the form data by index and don't
     *   trust the lo_FormData that was passed in
     */
    if (index)
	form_data = LO_GetFormDataByID(context, layer_id, index);

    if (!form_data)
	return NULL;

    obj = form_data->mocha_object;
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
                             LM_GET_MAPPING_KEY(LM_FORMS, layer_id, index));
        if (obj) {
            form_data->mocha_object = obj;
            goto out;
        }
    }

    /* Get the document object that will hold this form */
    document = lm_GetDocumentFromLayerId(decoder, layer_id);
    if (!document)
        goto out;

    array_obj = lm_GetFormArray(decoder, document);
    if (!array_obj)
	goto out;

    prototype = decoder->form_prototype;
    if (!prototype) {
        prototype = JS_InitClass(cx, decoder->window_object,
				 decoder->event_receiver_prototype,
				 &lm_form_class,
				 Form, 0, form_props, form_methods,
				 NULL, NULL);
        if (!prototype)
	    goto out;
        decoder->form_prototype = prototype;
    }

    form = JS_malloc(cx, sizeof *form);
    if (!form)
	goto out;
    XP_BZERO(form, sizeof *form);

    obj = JS_NewObject(cx, &lm_form_class, prototype, document);
    if (!obj || !JS_SetPrivate(cx, obj, form)) {
        JS_free(cx, form);
        obj = NULL;
        goto out;
    }
    if (form_data->name) {
        ok = JS_DefineProperty(cx, document,
			       (char *) form_data->name,
			       OBJECT_TO_JSVAL(obj),
			       NULL, NULL,
			       JSPROP_ENUMERATE | JSPROP_READONLY);
        if (!ok) {
	    obj = NULL;
	    goto out;
        }
    }

    /* put it in the form array */
    if (!lm_AddObjectToArray(cx, array_obj, (char *) form_data->name,
			     index - 1, obj)) {
	obj = NULL;
	goto out;
    }

    /* Put it in the index to object hash table */
    map = lm_GetIdToObjectMap(decoder);
    if (map)
        PR_HashTableAdd(map, LM_GET_MAPPING_KEY(LM_FORMS, layer_id, index),
                        obj);

    form->form_decoder = HOLD_BACK_COUNT(decoder);
    form->form_element_map = NULL;
    form->form_object = obj;
    form->form_id = form_data->id;
    form->layer_id = layer_id;
    form_data->mocha_object = obj;

    /* see if there are any other attributes that we should be
     *  adding to this object
     */
    if(tag) {
        PA_Block onreset, onsubmit, id;

	/* don't hold the layout lock across compiles */
	LO_UnlockLayout();

        onreset  = lo_FetchParamValue(context, tag, PARAM_ONRESET);
        onsubmit = lo_FetchParamValue(context, tag, PARAM_ONSUBMIT);
        id = lo_FetchParamValue(context, tag, PARAM_ID);

	ok = JS_TRUE;
	if (onsubmit) {
	    ok = lm_CompileEventHandler(decoder, id, tag->data,
				        tag->newline_count, obj,
				        PARAM_ONSUBMIT, onsubmit);
	    PA_FREE(onsubmit);
	}
	if (onreset) {
	    ok &= lm_CompileEventHandler(decoder, id, tag->data,
					 tag->newline_count, obj,
				         PARAM_ONRESET, onreset);
	    PA_FREE(onreset);
	}
	if (!ok)
	    obj = NULL;
	if (id)
	    PA_FREE(id);
        
	LO_LockLayout();
    }

out:
    LM_PutMochaDecoder(decoder);
    return obj;
}

JSObject *
lm_GetFormObjectByID(MWContext * context, int32 layer_id, uint form_id)
{
    lo_FormData *form_data;

    form_data = LO_GetFormDataByID(context, layer_id, form_id);
    if (!form_data)
	return NULL;
    return form_data->mocha_object;
}

LO_FormElementStruct *
lm_GetFormElementByIndex(JSContext * cx, JSObject *form_obj, int32 index)
{
    JSForm *form;
    MWContext *context;
    lo_FormData *form_data;

    if (!form_obj)
	return NULL;
    form = JS_GetPrivate(cx, form_obj);
    if (!form)
	return NULL;
    context = form->form_decoder->window_context;
    if (!context)
	return NULL;
    form_data = LO_GetFormDataByID(context, form->layer_id, form->form_id);
    if (!form_data)
	return NULL;
    return LO_GetFormElementByIndex(form_data, index);
}

PRIVATE JSBool
lm_normalize_element_index(lo_FormData *form_data, uint *index)
{
    LO_Element **ele_list;
    LO_FormElementStruct *first_ele;
    uint first_index;

    /* XXX confine this to laymocha.c where LO_GetFormByElementIndex() lives */
    if (form_data->form_ele_cnt == 0) {
	*index = 0;
    } else {
	ele_list = (LO_Element **) form_data->form_elements;
	first_ele = (LO_FormElementStruct *)ele_list[0];
	first_index = (uint)first_ele->element_index;
	XP_ASSERT(*index >= first_index);
	if (*index < first_index)
	    return JS_FALSE;
	*index -= first_index;
    }

    return JS_TRUE;
}

JSBool
lm_AddFormElement(JSContext *cx, JSObject *form_obj, JSObject *ele_obj,
		  char *name, uint index)
{
    JSForm *form;
    MWContext *context;
    lo_FormData *form_data;

    form = JS_GetPrivate(cx, form_obj);
    if (!form)
	return JS_TRUE;
    context = form->form_decoder->window_context;
    if (!context)
	return JS_FALSE;
    form_data = LO_GetFormDataByID(context, form->layer_id, form->form_id);
    if (!form_data)
	return JS_FALSE;

    if (!lm_normalize_element_index(form_data, &index))
        return JS_FALSE;

    if (!form->form_element_map)
        form->form_element_map = PR_NewHashTable(LM_FORM_ELEMENT_MAP_SIZE,
                                                 lm_KeyHash,
                                                 PR_CompareValues,
                                                 PR_CompareValues,
                                                 NULL, NULL);
    if (!form->form_element_map)
        return JS_FALSE;

    PR_HashTableAdd(form->form_element_map, (void *)index, ele_obj);

    /* put it in the form elememt array */
    return (lm_AddObjectToArray(cx, form_obj, name, index, ele_obj));

}

JSObject *
lm_GetFormElementFromMapping(JSContext *cx, JSObject *form_obj, uint32 index)
{
    JSForm *form;
    MWContext *context;
    lo_FormData *form_data;

    form = JS_GetPrivate(cx, form_obj);
    if (!form || !form->form_element_map)
        return NULL;

    context = form->form_decoder->window_context;
    if (!context)
	return NULL;
    form_data = LO_GetFormDataByID(context, form->layer_id, form->form_id);
    if (!form_data)
	return NULL;

    /*
     * This converts element_id to index within this form. Need to do
     * this here, because it also happens when we put it into the hash
     * table.
     */
    if (!lm_normalize_element_index(form_data, (uint *)&index))
        return NULL;

    return (JSObject *)PR_HashTableLookup(form->form_element_map,
                                          (void *)index);
}


JSBool
lm_ReflectRadioButtonArray(MWContext *context, int32 layer_id, intn form_id,
                           const char *name, PA_Tag * tag)
{
    lo_FormData *form_data;
    JSBool ok;
    LO_Element **ele_list;
    int32 i, element_index;
    LO_FormElementStruct *form_ele;
    LO_FormElementData *data;

    form_data = LO_GetFormDataByID(context, layer_id, form_id);
    if (!form_data)
        return JS_FALSE;
    ok = JS_TRUE;
    for (i = 0; i < form_data->form_ele_cnt; i++) {
	/*
	 * Resample since the reflect call may release the layout
	 *   lock and the element list might get reallocated
	 */
        ele_list = (LO_Element **) form_data->form_elements;
        form_ele = (LO_FormElementStruct *)ele_list[i];

	/*
	 * If both the current element name and the passed in name are
	 *   non-NULL they must be equal.  If name is NULL reflect all
	 *   radio buttons
	 */
	data = form_ele->element_data;
        if (data &&
            ((data->ele_minimal.name && name) ?
	     !XP_STRCMP((char *)data->ele_minimal.name, name) :
	     data->type == FORM_TYPE_RADIO) &&
            form_ele->mocha_object == NULL) {

	    element_index = form_ele->element_index;
	    ok = (JSBool)
		 (LM_ReflectFormElement(context, layer_id, form_id,
					element_index,
					(tag->lo_data == (void *)element_index)
					? tag : NULL)
		  != NULL);
            if (!ok)
                break;
        }
    }
    return ok;
}
