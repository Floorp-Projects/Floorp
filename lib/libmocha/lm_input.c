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
 * JS input focus and event notifiers.
 *
 * Brendan Eich, 9/27/95
 *
 * XXX SIZE, MAXLENGTH attributes
 */
#include "lm.h"
#include "xp.h"
#include "lo_ele.h"
#include "pa_tags.h"
#include "layout.h"
#include "prmem.h"

enum input_slot {
    INPUT_TYPE              = -1,
    INPUT_NAME              = -2,
    INPUT_FORM              = -3,
    INPUT_VALUE             = -4,
    INPUT_DEFAULT_VALUE     = -5,
    INPUT_LENGTH            = -6,
    INPUT_OPTIONS           = -7,
    INPUT_SELECTED_INDEX    = -8,
    INPUT_STATUS            = -9,
    INPUT_DEFAULT_STATUS    = -10
#if DISABLED_READONLY_SUPPORT
    INPUT_DISABLED          = -11,
    INPUT_READONLY          = -12
#endif
};

static char lm_options_str[] = "options";

static JSPropertySpec input_props[] = {
    {"type",            INPUT_TYPE,           JSPROP_ENUMERATE|JSPROP_READONLY},
    {"name",            INPUT_NAME,           JSPROP_ENUMERATE},
    {"form",            INPUT_FORM,           JSPROP_ENUMERATE|JSPROP_READONLY},
    {"value",           INPUT_VALUE,          JSPROP_ENUMERATE},
    {"defaultValue",    INPUT_DEFAULT_VALUE,  JSPROP_ENUMERATE},
    {lm_length_str,     INPUT_LENGTH,         JSPROP_ENUMERATE},
    {lm_options_str,    INPUT_OPTIONS,        JSPROP_ENUMERATE|JSPROP_READONLY},
    {"selectedIndex",   INPUT_SELECTED_INDEX, JSPROP_ENUMERATE},
    {"status",          INPUT_STATUS,         0},
    {"defaultStatus",   INPUT_DEFAULT_STATUS, 0},
    {PARAM_CHECKED,     INPUT_STATUS,         JSPROP_ENUMERATE},
    {"defaultChecked",  INPUT_DEFAULT_STATUS, JSPROP_ENUMERATE},
#if DISABLED_READONLY_SUPPORT
    {"disabled",	INPUT_DISABLED,       JSPROP_ENUMERATE},
    {"readonly",	INPUT_READONLY,       JSPROP_ENUMERATE},
#endif
    {0}
};

/*
 * Base input element type.
 */
typedef struct JSInput {
    JSInputHandler          handler;
    int32                   index;
} JSInput;

#define input_decoder       handler.base_decoder
#define input_type          handler.base_type
#define input_object        handler.object
#define input_event_mask    handler.event_mask

/*
 * Text and textarea input type.
 */
typedef struct JSTextInput {
    JSInput              input;
} JSTextInput;

/*
 * Select option tag reflected type.
 */
enum option_slot {
    OPTION_INDEX            = -1,
    OPTION_TEXT             = -2,
    OPTION_VALUE            = -3,
    OPTION_DEFAULT_SELECTED = -4,
    OPTION_SELECTED         = -5
};

static JSPropertySpec option_props[] = {
    {"index",           OPTION_INDEX,            JSPROP_ENUMERATE|JSPROP_READONLY},
    {"text",            OPTION_TEXT,             JSPROP_ENUMERATE},
    {"value",           OPTION_VALUE,            JSPROP_ENUMERATE},
    {"defaultSelected", OPTION_DEFAULT_SELECTED, JSPROP_ENUMERATE},
    {"selected",        OPTION_SELECTED,         JSPROP_ENUMERATE},
    {0}
};

typedef struct JSSelectOption {
    MochaDecoder            *decoder;
    JSObject                *object;
    uint32                  index;
    int32                   indexInForm;
    lo_FormElementOptionData *data;
} JSSelectOption;

extern JSClass lm_option_class;

PR_STATIC_CALLBACK(JSBool)
option_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSSelectOption *option;
    lo_FormElementOptionData *optionData;
    lo_FormElementSelectData *selectData;
    LO_FormElementStruct *form_element;
    enum option_slot option_slot;
    JSString *str;
    char *value;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    option = JS_GetInstancePrivate(cx, obj, &lm_option_class, NULL);
    if (!option)
	return JS_TRUE;

    LO_LockLayout();
    optionData = option->data;
    if (optionData) {
	selectData = 0;
	form_element = 0;
    } else {
	JSObject * parent = JS_GetParent(cx, obj);
	if (!parent)
	    goto good;
	form_element = lm_GetFormElementByIndex(cx, JS_GetParent(cx, parent),
						option->indexInForm);
	if (!form_element)
	    goto good;
	selectData = &form_element->element_data->ele_select;
    }
    option_slot = slot;
    switch (option_slot) {
      case OPTION_INDEX:
	*vp = INT_TO_JSVAL(option->index);
	break;

      case OPTION_TEXT:
      case OPTION_VALUE:
	if (selectData)
	    optionData = (lo_FormElementOptionData *) selectData->options;
	if (slot == OPTION_TEXT)
	    value = (char *)optionData[option->index].text_value;
	else
	    value = (char *)optionData[option->index].value;
	str = lm_LocalEncodingToStr(option->decoder->window_context, 
				    value);
	if (!str)
	    goto bad;
	*vp = STRING_TO_JSVAL(str);
	break;

      case OPTION_DEFAULT_SELECTED:
      case OPTION_SELECTED:
	if (selectData)
	    optionData = (lo_FormElementOptionData *) selectData->options;
	*vp = BOOLEAN_TO_JSVAL((option_slot == OPTION_DEFAULT_SELECTED)
			 ? optionData[option->index].def_selected
			 : optionData[option->index].selected);
	break;
      default:
	/* Don't mess with a user-defined or method property. */
	break;
    }

good:
    LO_UnlockLayout();
    return JS_TRUE;
bad:
    LO_UnlockLayout();
    return JS_FALSE;
}

PR_STATIC_CALLBACK(JSBool)
option_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSSelectOption *option;
    lo_FormElementOptionData *optionData;
    lo_FormElementSelectData *selectData;
    LO_FormElementStruct *form_element;
    enum option_slot option_slot;
    JSBool showChange;
    int32 i;
    jsint slot;
    char * value = NULL;
    MWContext * context;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    option = JS_GetInstancePrivate(cx, obj, &lm_option_class, NULL);
    if (!option)
	return JS_TRUE;

    context = option->decoder->window_context;
    optionData = option->data;
    LO_LockLayout();
    if (optionData) {
	selectData = 0;
	form_element = 0;
    } else {
	JSObject * parent = JS_GetParent(cx, obj);
	if (!parent)
	    goto good;
	form_element = lm_GetFormElementByIndex(cx, JS_GetParent(cx, parent),
						option->indexInForm);
	if (!form_element)
	    goto good;
	selectData = &form_element->element_data->ele_select;
    }

    if (selectData && option->index >= (uint32) selectData->option_cnt)
	goto good;

    option_slot = slot;
    showChange = JS_FALSE;
    switch (option_slot) {
      case OPTION_TEXT:
      case OPTION_VALUE:
	if (!JSVAL_IS_STRING(*vp) &&
	    !JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp)) {
	    goto bad;
	}
	if (selectData)
	    optionData = (lo_FormElementOptionData *) selectData->options;

	value = lm_StrToLocalEncoding(context, JSVAL_TO_STRING(*vp));
	if (!value)
	    goto bad;

	if (option_slot == OPTION_TEXT) {
	    if (!lm_SaveParamString(cx, &optionData[option->index].text_value,
				    value)) {
		goto bad;
	    }
	    showChange = JS_TRUE;
	} else {
	    if (!lm_SaveParamString(cx, &optionData[option->index].value,
				    value)) {
		goto bad;
	    }
	}
	XP_FREE(value);
	break;

      case OPTION_DEFAULT_SELECTED:
      case OPTION_SELECTED:
	if (!JSVAL_IS_BOOLEAN(*vp) &&
	    !JS_ConvertValue(cx, *vp, JSTYPE_BOOLEAN, vp)) {
	    goto bad;
	}

	if (selectData)
	    optionData = (lo_FormElementOptionData *) selectData->options;
	if (option_slot == OPTION_DEFAULT_SELECTED)
	    optionData[option->index].def_selected = JSVAL_TO_BOOLEAN(*vp);
	else
	    optionData[option->index].selected = JSVAL_TO_BOOLEAN(*vp);
	if (selectData) {
	    if (JSVAL_TO_BOOLEAN(*vp) && !selectData->multiple) {
		/* Clear all the others. */
		for (i = 0; i < selectData->option_cnt; i++) {
		    if ((uint32)i == option->index)
			continue;
		    if (option_slot == OPTION_DEFAULT_SELECTED)
			optionData[i].def_selected = FALSE;
		    else
			optionData[i].selected = FALSE;
		}
	    }
	}

	if (option_slot == OPTION_SELECTED)
	    showChange = JS_TRUE;
	break;

      default:
	/* Don't mess with a user-defined property. */
	goto good;
    }

    if (showChange && context && form_element) {
	ET_PostManipulateForm(context,
			      (LO_Element *)form_element, 
			      EVENT_CHANGE);
    }

good:
    LO_UnlockLayout();
    return JS_TRUE;
bad:
    XP_FREEIF(value);
    LO_UnlockLayout();
    return JS_FALSE;
}

PR_STATIC_CALLBACK(void)
option_finalize(JSContext *cx, JSObject *obj)
{
    JSSelectOption *option;
    lo_FormElementOptionData *optionData;

    option = JS_GetPrivate(cx, obj);
    if (!option)
	return;
    optionData = option->data;
    if (optionData) {
	if (optionData->text_value)
	    JS_free(cx, optionData->text_value);
	if (optionData->value)
	    JS_free(cx, optionData->value);
	JS_free(cx, optionData);
    }
    DROP_BACK_COUNT(option->decoder);
    JS_free(cx, option);
}

JSClass lm_option_class = {
    "Option", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, option_getProperty, option_setProperty,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, option_finalize
};

/*
 * Select option constructor, can be called any of these ways:
 *  opt = new Option()
 *  opt = new Option(text)
 *  opt = new Option(text, value)
 *  opt = new Option(text, value, defaultSelected)
 *  opt = new Option(text, value, defaultSelected, selected)
 * Where opt can be selectData.options[i] for any nonnegative integer i.
 */
PR_STATIC_CALLBACK(JSBool)
Option(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    JSSelectOption *option;
    lo_FormElementOptionData *optionData;
    JSString *str;
    JSBool bval;
    MWContext *context;

    XP_ASSERT(JS_InstanceOf(cx, obj, &lm_option_class, NULL));

    decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    context = decoder->window_context;
    option = JS_malloc(cx, sizeof *option);
    if (!option)
	return JS_TRUE;
    XP_BZERO(option, sizeof *option);
    if (!JS_SetPrivate(cx, obj, option)) {
	JS_free(cx, option);
	return JS_FALSE;
    }

    optionData = JS_malloc(cx, sizeof *optionData);
    if (!optionData)
	return JS_FALSE;
    XP_BZERO(optionData, sizeof *optionData);
    option->data = optionData;

    if (argc >= 4) {
	if (!JSVAL_IS_BOOLEAN(argv[3]) &&
	    !JS_ValueToBoolean(cx, argv[3], &bval)) {
	    return JS_FALSE;
	}
	optionData->selected = bval;
    }
    if (argc >= 3) {
	if (!JSVAL_IS_BOOLEAN(argv[2]) &&
	    !JS_ValueToBoolean(cx, argv[2], &bval)) {
	    return JS_FALSE;
	}
	optionData->def_selected = bval;
    }
    if (argc >= 2) {
	if (JSVAL_IS_STRING(argv[1]))
	    str = JSVAL_TO_STRING(argv[1]);
	else if (!(str = JS_ValueToString(cx, argv[1])))
	    return JS_FALSE;
	optionData->value = 
		    (PA_Block)lm_StrToLocalEncoding(context, str);
	if (!optionData->value)
	    return JS_FALSE;
    }
    if (argc >= 1) {
	if (JSVAL_IS_STRING(argv[0]))
	    str = JSVAL_TO_STRING(argv[0]);
	else if (!(str = JS_ValueToString(cx, argv[0])))
	    return JS_FALSE;
	optionData->text_value =
		    (PA_Block)lm_StrToLocalEncoding(context, str);
	if (!optionData->text_value)
	    return JS_FALSE;
    }

    option->decoder = HOLD_BACK_COUNT(decoder);
    option->object = obj;
    option->index = 0;		/* so option->data[option->index] works */
    option->indexInForm = -1;
    return JS_TRUE;
}

static char *typenames[] = {
    "none",
    S_FORM_TYPE_TEXT,
    S_FORM_TYPE_RADIO,
    S_FORM_TYPE_CHECKBOX,
    S_FORM_TYPE_HIDDEN,
    S_FORM_TYPE_SUBMIT,
    S_FORM_TYPE_RESET,
    S_FORM_TYPE_PASSWORD,
    S_FORM_TYPE_BUTTON,
    S_FORM_TYPE_JOT,
    "select-one",
    "select-multiple",
    "textarea",
    "isindex",
    S_FORM_TYPE_IMAGE,
    S_FORM_TYPE_FILE,
    "keygen",
    S_FORM_TYPE_READONLY
};

extern JSClass lm_input_class;

/*
 * Note early returns below, to avoid common string-valued property code at
 * the bottom of the function.
 */
PR_STATIC_CALLBACK(JSBool)
input_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSInput *input;
    MWContext *context;
    enum input_slot input_slot;
    LO_FormElementStruct *form_element;
    JSObject *option_obj;
    JSString *str;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    input = JS_GetInstancePrivate(cx, obj, &lm_input_class, NULL);
    if (!input)
	return JS_TRUE;
    input_slot = slot;
    if (input_slot == INPUT_FORM) {
	/* Each input in a form has a back-pointer to its form. */
	*vp = OBJECT_TO_JSVAL(JS_GetParent(cx, obj));
	return JS_TRUE;
    }

    LO_LockLayout();

    form_element = lm_GetFormElementByIndex(cx, JS_GetParent(cx, obj),
					    input->index);
    if (!form_element)
	goto good;

    if (input_slot == INPUT_TYPE) {
	uint type_index;

	type_index = form_element->element_data->type;
	if (type_index >= sizeof typenames / sizeof typenames[0]) {
	    JS_ReportError(cx, "unknown form element type %u", type_index);
	    goto bad;
	}
	str = JS_NewStringCopyZ(cx, typenames[type_index]);
	if (!str)
	    goto bad;
	*vp = STRING_TO_JSVAL(str);
	goto good;
    }

    context = input->input_decoder->window_context;
    switch (form_element->element_data->type) {
      case FORM_TYPE_TEXT:
      case FORM_TYPE_TEXTAREA:	/* XXX we ASSUME common struct prefixes */
      case FORM_TYPE_FILE:	/* XXX as above, also get-only without signing */
      case FORM_TYPE_PASSWORD:
	{
	    lo_FormElementTextData *text;

	    text = &form_element->element_data->ele_text;
	    switch (input_slot) {
	      case INPUT_NAME:
		str = lm_LocalEncodingToStr(context, 
					    (char *)text->name);
		break;
	      case INPUT_VALUE:
		str = lm_LocalEncodingToStr(context, 
					    (char *)text->current_text);
		break;
	      case INPUT_DEFAULT_VALUE:
		str = lm_LocalEncodingToStr(context, 
					    (char *)text->default_text);
		break;
#if DISABLED_READONLY_SUPPORT
	      case INPUT_DISABLED:
	        *vp = BOOLEAN_TO_JSVAL(text->disabled);
		goto good;
	      case INPUT_READONLY:
		*vp = BOOLEAN_TO_JSVAL(text->readonly);
		goto good;
#endif
	      default:
		/* Don't mess with a user-defined property. */
		goto good;
	    }
	}
	break;

      case FORM_TYPE_SELECT_ONE:
      case FORM_TYPE_SELECT_MULT:
	{
	    lo_FormElementSelectData *selectData;
	    lo_FormElementOptionData *optionData;
	    int32 i;
	    JSSelectOption *option;

	    selectData = &form_element->element_data->ele_select;
	    switch (input_slot) {
	      case INPUT_NAME:
		str = lm_LocalEncodingToStr(context,
					    (char *)selectData->name);
		break;

	      case INPUT_LENGTH:
		*vp = INT_TO_JSVAL(selectData->option_cnt);
		goto good;

	      case INPUT_OPTIONS:
		*vp = OBJECT_TO_JSVAL(input->input_object);
		goto good;

	      case INPUT_SELECTED_INDEX:
		*vp = INT_TO_JSVAL(-1);
		optionData = (lo_FormElementOptionData *) selectData->options;
		for (i = 0; i < selectData->option_cnt; i++) {
		    if (optionData[i].selected) {
			*vp = INT_TO_JSVAL(i);
			break;
		    }
		}
		goto good;

#if DISABLED_READONLY_SUPPORT
	      case INPUT_DISABLED:
	        *vp = BOOLEAN_TO_JSVAL(selectData->disabled);
		goto good;
	      case INPUT_READONLY:
		*vp = BOOLEAN_TO_JSVAL(FALSE);
		goto good;
#endif
	      default:
		if ((uint32)slot >= (uint32)selectData->option_cnt) {
		    *vp = JSVAL_NULL;
		    goto good;
		}

		if (JSVAL_IS_OBJECT(*vp) && JSVAL_TO_OBJECT(*vp)) {
		    XP_ASSERT(JS_InstanceOf(cx, JSVAL_TO_OBJECT(*vp),
					    &lm_option_class, NULL));
		    goto good;
		}

		option = JS_malloc(cx, sizeof *option);
		if (!option)
		    goto bad;

		option_obj =
		    JS_NewObject(cx, &lm_option_class,
				 input->input_decoder->option_prototype, obj);

		if (!option_obj || !JS_SetPrivate(cx, option_obj, option)) {
		    JS_free(cx, option);
		    goto bad;
		}
		option->decoder = HOLD_BACK_COUNT(input->input_decoder);
		option->object = option_obj;
		option->index = (uint32)slot;
		option->indexInForm = form_element->element_index;
		option->data = NULL;
		*vp = OBJECT_TO_JSVAL(option_obj);
		goto good;
	    }
	}
	break;

      case FORM_TYPE_RADIO:
      case FORM_TYPE_CHECKBOX:
	{
	    lo_FormElementToggleData *toggle;

	    toggle = &form_element->element_data->ele_toggle;
	    switch (input_slot) {
	      case INPUT_NAME:
		str = lm_LocalEncodingToStr(context, 
					    (char *)toggle->name);
		break;
	      case INPUT_VALUE:
		str = lm_LocalEncodingToStr(context, 
					    (char *)toggle->value);
		break;
	      case INPUT_STATUS:
		*vp = BOOLEAN_TO_JSVAL(toggle->toggled);
		goto good;
	      case INPUT_DEFAULT_STATUS:
		*vp = BOOLEAN_TO_JSVAL(toggle->default_toggle);
		goto good;

#if DISABLED_READONLY_SUPPORT
	      case INPUT_DISABLED:
	        *vp = BOOLEAN_TO_JSVAL(toggle->disabled);
		goto good;
	      case INPUT_READONLY:
		*vp = BOOLEAN_TO_JSVAL(FALSE);
		goto good;
#endif
	      default:
		/* Don't mess with a user-defined property. */
		goto good;
	    }
	}
	break;

      default:
	{
	    lo_FormElementMinimalData *minimal;

	    minimal = &form_element->element_data->ele_minimal;
	    switch (input_slot) {
	      case INPUT_NAME:
		str = lm_LocalEncodingToStr(context, 
					    (char *)minimal->name);
		break;
	      case INPUT_VALUE:
		str = lm_LocalEncodingToStr(context, 
					    (char *)minimal->value);
		break;
#if DISABLED_READONLY_SUPPORT
	      case INPUT_DISABLED:
	        *vp = BOOLEAN_TO_JSVAL(minimal->disabled);
		goto good;
	      case INPUT_READONLY:
		*vp = BOOLEAN_TO_JSVAL(FALSE); /* minimal elements don't have the readonly attribute. */
		goto good;
#endif
	      default:
		/* Don't mess with a user-defined property. */
		goto good;
	    }
	}
	break;
    }

    if (!str)
	goto bad;
    *vp = STRING_TO_JSVAL(str);

good:
    LO_UnlockLayout();
    return JS_TRUE;

bad:
    LO_UnlockLayout();
    return JS_FALSE;

}

char *
lm_FixNewlines(JSContext *cx, const char *value, JSBool formElement)
{
    size_t size;
    const char *cp;
    char *tp, *new_value;

#if defined XP_PC
    size = 1;
    for (cp = value; *cp != '\0'; cp++) {
	switch (*cp) {
	  case '\r':
	    if (cp[1] != '\n')
		size++;
	    break;
	  case '\n':
	    if (cp > value && cp[-1] != '\r')
		size++;
	    break;
	}
    }
    size += cp - value;
#else
    size = XP_STRLEN(value) + 1;
#endif
    new_value = JS_malloc(cx, size);
    if (!new_value)
	return NULL;
    for (cp = value, tp = new_value; *cp != '\0'; cp++) {
#if defined XP_MAC
	if (*cp == '\n') {
	    if (cp > value && cp[-1] != '\r')
		*tp++ = '\r';
	} else {
	    *tp++ = *cp;
	}
#elif defined XP_PC
	switch (*cp) {
	  case '\r':
	    *tp++ = '\r';
	    if (cp[1] != '\n' && formElement)
		*tp++ = '\n';
	    break;
	  case '\n':
	    if (cp > value && cp[-1] != '\r' && formElement)
		*tp++ = '\r';
	    *tp++ = '\n';
	    break;
	  default:
	    *tp++ = *cp;
	    break;
	}
#else /* XP_UNIX */
	if (*cp == '\r') {
	    if (cp[1] != '\n')
		*tp++ = '\n';
	} else {
	    *tp++ = *cp;
	}
#endif
    }
    *tp = '\0';
    return new_value;
}

PR_STATIC_CALLBACK(JSBool)
input_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSInput *input;
    enum input_slot input_slot;
    const char *prop_name;
    char *value = NULL;
    LO_FormElementStruct *form_element;
    MochaDecoder *decoder;
    MWContext *context;
    int32 intval;
    jsint slot;

    input = JS_GetInstancePrivate(cx, obj, &lm_input_class, NULL);
    if (!input)
	return JS_TRUE;

    /* If the property is seting a key handler we find out now so
     * that we can tell the front end to send the event. */
    if (JSVAL_IS_STRING(id)) {
	prop_name = JS_GetStringBytes(JSVAL_TO_STRING(id));
	/* XXX use lm_onKeyDown_str etc. initialized by PARAM_ONKEYDOWN */
	if (XP_STRCASECMP(prop_name, "onkeydown") == 0 ||
	    XP_STRCASECMP(prop_name, "onkeyup") == 0 ||
	    XP_STRCASECMP(prop_name, "onkeypress") == 0) {
	    form_element = lm_GetFormElementByIndex(cx, JS_GetParent(cx, obj),
						    input->index);
	    form_element->event_handler_present = TRUE;
	}
	return JS_TRUE;
    }

    XP_ASSERT(JSVAL_IS_INT(id));
    slot = JSVAL_TO_INT(id);

    decoder = input->input_decoder;
    context = decoder->window_context;
    input_slot = slot;
    switch (input_slot) {
      case INPUT_TYPE:
      case INPUT_FORM:
      case INPUT_OPTIONS:
	/* These are immutable. */
	break;
      case INPUT_NAME:
      case INPUT_VALUE:
      case INPUT_DEFAULT_VALUE:
	/* These are string-valued. */
	if (!JSVAL_IS_STRING(*vp) &&
	    !JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp)) {
	    return JS_FALSE;
	}
	value = lm_StrToLocalEncoding(context, JSVAL_TO_STRING(*vp));
	break;
      case INPUT_STATUS:
      case INPUT_DEFAULT_STATUS:
#if DISABLED_READONLY_SUPPORT
      case INPUT_READONLY:
      case INPUT_DISABLED:
#endif
	/* These must be Booleans. */
	if (!JSVAL_IS_BOOLEAN(*vp) &&
	    !JS_ConvertValue(cx, *vp, JSTYPE_BOOLEAN, vp)) {
	    return JS_FALSE;
	}
	break;
      case INPUT_LENGTH:
      case INPUT_SELECTED_INDEX:
	/* These should be integers. */
	if (JSVAL_IS_INT(*vp))
	    intval = JSVAL_TO_INT(*vp);
	else if (!JS_ValueToInt32(cx, *vp, &intval)) {
	    return JS_FALSE;
	}
	break;
    }

    LO_LockLayout();

    form_element = lm_GetFormElementByIndex(cx, JS_GetParent(cx, obj),
					    input->index);
    if (!form_element)
	goto good;

    switch (form_element->element_data->type) {
      case FORM_TYPE_FILE:
	/* if we try to set a file upload widget we better be a signed script */
	if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_FILE_READ))
	    break;
	/* else fall through... */

      case FORM_TYPE_TEXT:
      case FORM_TYPE_TEXTAREA:	/* XXX we ASSUME common struct prefixes */
      case FORM_TYPE_PASSWORD:
	{
	    lo_FormElementTextData *text;
	    JSBool ok;
	    char * fixed_string;

	    text = &form_element->element_data->ele_text;
	    switch (input_slot) {
	      case INPUT_NAME:
		if (!lm_SaveParamString(cx, &text->name, value))
		    goto bad;
		break;
	      case INPUT_VALUE:
	      case INPUT_DEFAULT_VALUE:
		fixed_string = lm_FixNewlines(cx, value, JS_TRUE);
		if (!fixed_string)
		    goto bad;
		ok = (input_slot == INPUT_VALUE)
		     ? lm_SaveParamString(cx, &text->current_text, fixed_string)
		     : lm_SaveParamString(cx, &text->default_text, fixed_string);

		JS_free(cx, (char *)fixed_string);
		if (!ok)
		    goto bad;
		if (input_slot == INPUT_VALUE && context) {
		    ET_PostManipulateForm(context, (LO_Element *)form_element,
                                  EVENT_CHANGE);
		}
		break;
#if DISABLED_READONLY_SUPPORT
	      case INPUT_DISABLED:
		text->disabled = JSVAL_TO_BOOLEAN(*vp);
		if (context) {
		  ET_PostManipulateForm(context, (LO_Element *)form_element,
					EVENT_CHANGE);
		}
		break;
	      case INPUT_READONLY:
		if (form_element->element_data->type == FORM_TYPE_FILE)
		  break;
		text->readonly = JSVAL_TO_BOOLEAN(*vp);
		if (context) {
		  ET_PostManipulateForm(context, (LO_Element *)form_element,
					EVENT_CHANGE);
		}
		break;
#endif
	      default:
		/* Don't mess with option or user-defined property. */
		goto good;
	    }
	}
	break;

      case FORM_TYPE_SELECT_ONE:
      case FORM_TYPE_SELECT_MULT:
	{
	    lo_FormElementSelectData *selectData;
	    lo_FormElementOptionData *optionData;
	    JSSelectOption *option;
	    int32 i, new_option_cnt, old_option_cnt;

	    selectData = &form_element->element_data->ele_select;
	    switch (slot) {
	      case INPUT_NAME:
		if (!lm_SaveParamString(cx, &selectData->name, value))
		    goto bad;
		break;

	      case INPUT_LENGTH:
		new_option_cnt = intval;
		old_option_cnt = selectData->option_cnt;
		optionData = (lo_FormElementOptionData *) selectData->options;

		/* Remove truncated slots, or clear extended element data. */
		if (new_option_cnt < old_option_cnt) {
		    /*
		     * Make truncated options stand alone in case someone else
		     *   in case someone else has a reference to one.
		     */
		    for (i = new_option_cnt; i < old_option_cnt; i++) {
			jsval oval;
			JSObject * option_obj;

			if (!JS_LookupElement(cx, obj, i, &oval))
			    goto bad;
			if (JSVAL_IS_OBJECT(oval) &&
			    (option_obj = JSVAL_TO_OBJECT(oval))) {
			    lo_FormElementOptionData *myData;

			    myData =
				JS_malloc(cx, sizeof(lo_FormElementOptionData));
			    if (!myData)
				goto bad;
			    XP_MEMCPY(myData, &optionData[i],
				      sizeof(lo_FormElementOptionData));
			    option = JS_GetPrivate(cx, option_obj);
			    option->data = myData;
			}
			JS_DeleteElement(cx, obj, i);
		    }
		}

		/* Get layout to reallocate the options array. */
		selectData->option_cnt = new_option_cnt;
		if (!LO_ResizeSelectOptions(selectData)) {
		    selectData->option_cnt = old_option_cnt;
		    JS_ReportOutOfMemory(cx);
		    goto bad;
		}

		/* Handle the grow case by clearing the new options. */
		if (new_option_cnt > old_option_cnt) {
		    XP_BZERO(&optionData[old_option_cnt],
			     (new_option_cnt - old_option_cnt)
			     * sizeof *optionData);
		}

		/* Tell the FE about it. */
		if (context) {
		    ET_PostManipulateForm(context, (LO_Element *)form_element,
                                  EVENT_CHANGE);
		}
		break;

	      case INPUT_OPTIONS:
		break;

	      case INPUT_SELECTED_INDEX:
		optionData = (lo_FormElementOptionData *)
						    selectData->options;
		for (i = 0; i < selectData->option_cnt; i++)
		    optionData[i].selected = (i == intval);

		/* Tell the FE about it. */
		if (context)
		    ET_PostManipulateForm(context, (LO_Element *)form_element,
                                  EVENT_CHANGE);
		break;

#if DISABLED_READONLY_SUPPORT
	      case INPUT_DISABLED:
		selectData->disabled = JSVAL_TO_BOOLEAN(*vp);
		if (context) {
		  ET_PostManipulateForm(context, (LO_Element *)form_element,
					EVENT_CHANGE);
		}
		break;
	      case INPUT_READONLY:
		/* silenty ignore updates to the READONLY attribute. */
		break;
#endif
	      default:
		if (slot < 0) {
		    /* Don't mess with a user-defined, named property. */
		    goto good;
		}

		/* The vp arg must refer to an object of the right class. */
		if (!JSVAL_IS_OBJECT(*vp) &&
		    !JS_ConvertValue(cx, *vp, JSTYPE_OBJECT, vp)) {
		    goto bad;
		}

		if (JSVAL_IS_NULL(*vp)) {
		    int32 count, limit;
		    JSBool ok = JS_TRUE;

		    if (slot >= selectData->option_cnt)
			goto good;

		    /* Clear the option and compress the options array. */
		    optionData = (lo_FormElementOptionData *)
						    selectData->options;
		    count = selectData->option_cnt - (slot + 1);
		    if (count > 0) {
			/* 
			 * Move down the options that were after the option 
			 *   we are deleting.  Note, the JS_GetElement() 
			 *   and SetElement() calls will make sure the 
			 *   layout-based data gets copied too.
			 */
			for (limit = slot + count; slot < limit; slot++) {
			    jsval v;
			    ok = JS_GetElement(cx, obj, slot + 1, &v);
			    if (!ok)
				break;
			    JS_SetElement(cx, obj, slot, &v);

			    /* Fix each option's index-in-select property. */
			    XP_ASSERT(JSVAL_IS_OBJECT(v));
			    option = JS_GetPrivate(cx, JSVAL_TO_OBJECT(v));
			    option->index = slot;
			}
			if (ok)
			    JS_DeleteElement(cx, obj, slot);
		    }

		    /* Shrink the select element data's options array. */
		    if (ok) {
			selectData->option_cnt--;
			ok = (JSBool)LO_ResizeSelectOptions(selectData);
			if (!ok) {
			    JS_ReportOutOfMemory(cx);
			} else if (context) {
			    ET_PostManipulateForm(context,
						  (LO_Element *)form_element,
						  EVENT_CHANGE);
			}
		    }
		    LO_UnlockLayout();
		    return ok;
		}

		if (!JS_InstanceOf(cx, JSVAL_TO_OBJECT(*vp), &lm_option_class,
				   NULL)) {
		    JS_ReportError(cx, "cannot set %s.%s to incompatible %s",
				   JS_GetClass(obj)->name, lm_options_str,
				   JS_GetClass(JSVAL_TO_OBJECT(*vp))->name);
		    goto bad;
		}
		option = JS_GetPrivate(cx, JSVAL_TO_OBJECT(*vp));
		if (!option)
    		    goto good;

		if (!option->data && 
		     JS_GetParent(cx, option->object) != obj) {
		    JS_ReportError(cx, "can't share options between select elements");
		    goto bad;
		}

		/* Grow the option array if necessary. */
		old_option_cnt = selectData->option_cnt;
		if (slot >= old_option_cnt) {
		    selectData->option_cnt = slot + 1;
		    if (!LO_ResizeSelectOptions(selectData)) {
			selectData->option_cnt = old_option_cnt;
			JS_ReportOutOfMemory(cx);
			goto bad;
		    }
		}

		/* Clear any option structs in the gap, then set slot. */
		optionData = (lo_FormElementOptionData *) selectData->options;
		if (slot > old_option_cnt) {
		    XP_BZERO(&optionData[old_option_cnt],
			     (slot - old_option_cnt) * sizeof *optionData);
		}
		if (option->data) {
		    XP_MEMCPY(&optionData[slot], option->data,
			      sizeof(lo_FormElementOptionData));
		} else if ((uint32)slot != option->index) {
		    XP_MEMCPY(&optionData[slot], &optionData[option->index],
			      sizeof(lo_FormElementOptionData));
		}

		/* Update the option to point at its form and form element. */
		JS_SetParent(cx, JSVAL_TO_OBJECT(*vp), obj);
		option->index = (uint32)slot;
		option->indexInForm = form_element->element_index;
		if (option->data) {
		    JS_free(cx, option->data);
		    option->data = NULL;
		}

		/* Tell the FE about it. */
		if (context)
		    ET_PostManipulateForm(context, (LO_Element *)form_element,
                                  EVENT_CHANGE);
		break;
	    }
	}
	break;

      case FORM_TYPE_RADIO:
      case FORM_TYPE_CHECKBOX:
	{
	    lo_FormElementToggleData *toggle;

	    toggle = &form_element->element_data->ele_toggle;
	    switch (input_slot) {
	      case INPUT_NAME:
		if (!lm_SaveParamString(cx, &toggle->name, value))
		    goto bad;
		break;
	      case INPUT_VALUE:
		if (!lm_SaveParamString(cx, &toggle->value, value))
		    goto bad;
		break;
	      case INPUT_STATUS:
		if (JSVAL_IS_BOOLEAN(*vp))
		    toggle->toggled = JSVAL_TO_BOOLEAN(*vp);

		/* Tell the FE about it (the FE keeps radio-sets consistent). */
		if (context)
		    ET_PostManipulateForm(context, (LO_Element *)form_element,
                                  EVENT_CHANGE);
		break;
	      case INPUT_DEFAULT_STATUS:
		if (JSVAL_IS_BOOLEAN(*vp))
		    toggle->default_toggle = JSVAL_TO_BOOLEAN(*vp);
		break;
#if DISABLED_READONLY_SUPPORT
	      case INPUT_DISABLED:
		toggle->disabled = JSVAL_TO_BOOLEAN(*vp);
		if (context) {
		  ET_PostManipulateForm(context, (LO_Element *)form_element,
					EVENT_CHANGE);
		}
		break;
	      case INPUT_READONLY:
		/* silenty ignore updates to the READONLY attribute. */
		break;
#endif
	      default:
		/* Don't mess with a user-defined property. */
		goto good;
	    }
	}
	break;

      case FORM_TYPE_READONLY:
	/* Don't allow modification of readonly fields. */
	break;

      default:
	{
	    lo_FormElementMinimalData *minimal;

	    minimal = &form_element->element_data->ele_minimal;
	    switch (input_slot) {
	      case INPUT_NAME:
		if (!lm_SaveParamString(cx, &minimal->name, value))
		    goto bad;
		break;
	      case INPUT_VALUE:
		if (!lm_SaveParamString(cx, &minimal->value, value))
		    goto bad;
		if (context) {
		    ET_PostManipulateForm(context, (LO_Element *)form_element,
                                  EVENT_CHANGE);
		}
		break;
#if DISABLED_READONLY_SUPPORT
	      case INPUT_DISABLED:
		minimal->disabled = JSVAL_TO_BOOLEAN(*vp);
		if (context) {
		  ET_PostManipulateForm(context, (LO_Element *)form_element,
					EVENT_CHANGE);
		}
		break;
	      case INPUT_READONLY:
		/* silenty ignore updates to the READONLY attribute. */
		break;
#endif
	      default:
		/* Don't mess with a user-defined property. */
		goto good;
	    }
	}
	break;
    }

good:
    XP_FREEIF(value);
    LO_UnlockLayout();
    return JS_TRUE;

bad:
    XP_FREEIF(value);
    LO_UnlockLayout();
    return JS_FALSE;

}

PR_STATIC_CALLBACK(void)
input_finalize(JSContext *cx, JSObject *obj)
{
    JSInput *input;
    LO_FormElementStruct *form_element;

    input = JS_GetPrivate(cx, obj);
    if (!input)
	return;

    LO_LockLayout();
    form_element = lm_GetFormElementByIndex(cx, JS_GetParent(cx, obj),
					    input->index);
    if (form_element && form_element->mocha_object == obj)
	form_element->mocha_object = NULL;
    LO_UnlockLayout();
    DROP_BACK_COUNT(input->input_decoder);
    JS_free(cx, input);
}

JSClass lm_input_class = {
    "Input", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, input_getProperty, input_setProperty,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, input_finalize
};

PR_STATIC_CALLBACK(JSBool)
Input(JSContext *cx, JSObject *obj,
      uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
input_toString(JSContext *cx, JSObject *obj,
		uint argc, jsval *argv, jsval *rval)
{
    JSInput *input;
    LO_FormElementStruct *form_element;
    uint type;
    char *typename, *string, *value;
    size_t length;
    long truelong;
    jsval result;
    JSString *str;

    if (!JS_InstanceOf(cx, obj, &lm_input_class, argv))
        return JS_FALSE;
    input = JS_GetPrivate(cx, obj);
    if (!input)
	return JS_TRUE;

    LO_LockLayout();
    form_element = lm_GetFormElementByIndex(cx, JS_GetParent(cx, obj),
					    input->index);
    if (!form_element) {
	*rval = JS_GetEmptyStringValue(cx);
	goto bad;
    }

    type = form_element->element_data->type;
    if (type >= sizeof typenames / sizeof typenames[0]) {
	JS_ReportError(cx, "unknown form element type %u", type);
	goto bad;
    }
    typename = typenames[type];
    string = PR_sprintf_append(0, "<");
    switch (type) {
      case FORM_TYPE_TEXT:
	{
	    lo_FormElementTextData *text;

	    text = &form_element->element_data->ele_text;
	    string = PR_sprintf_append(string, "%s %s=\"%s\"",
				       PT_INPUT, PARAM_TYPE, typename);
	    if (text->name) {
		string = PR_sprintf_append(string, " %s=\"%s\"",
					   PARAM_NAME, (char *)text->name);
	    }
	    if (text->default_text) {
		string = PR_sprintf_append(string, " %s=\"%s\"",
					   PARAM_VALUE,
					   (char *)text->default_text);
	    }
	    if (text->size) {
		truelong = text->size;
		string = PR_sprintf_append(string, " %s=%ld\"",
					   PARAM_SIZE, truelong);
	    }
	    if (text->max_size) {
		truelong = text->max_size;
		string = PR_sprintf_append(string, " %s=%ld\"",
					   PARAM_MAXLENGTH, truelong);
	    }
	}
	break;

      case FORM_TYPE_TEXTAREA:	/* XXX we ASSUME common struct prefixes */
	{
	    lo_FormElementTextareaData *textarea;

	    textarea = &form_element->element_data->ele_textarea;
	    string = PR_sprintf_append(string, PT_TEXTAREA);
	    if (textarea->name) {
		string = PR_sprintf_append(string, " %s=\"%s\"",
					   PARAM_NAME, (char *)textarea->name);
	    }
	    if (textarea->default_text) {
		string = PR_sprintf_append(string, " %s=\"%s\"",
					   PARAM_VALUE,
					   (char *)textarea->default_text);
	    }
	    if (textarea->rows) {
		truelong = textarea->rows;
		string = PR_sprintf_append(string, " %s=%ld\"",
					   PARAM_SIZE, truelong);
	    }
	    if (textarea->cols) {
		truelong = textarea->cols;
		string = PR_sprintf_append(string, " %s=%ld\"",
					   PARAM_SIZE, truelong);
	    }
	    if (textarea->auto_wrap) {
		switch (textarea->auto_wrap) {
		  case TEXTAREA_WRAP_OFF:
		    value = "off";
		    break;
		  case TEXTAREA_WRAP_HARD:
		    value = "hard";
		    break;
		  case TEXTAREA_WRAP_SOFT:
		    value = "soft";
		    break;
		  default:
		    value = "unknown";
		    break;
		}
		string = PR_sprintf_append(string, " %s=\"%s\"",
					   PARAM_WRAP, value);
	    }
	}
	break;

      case FORM_TYPE_SELECT_ONE:
      case FORM_TYPE_SELECT_MULT:
	{
	    lo_FormElementSelectData *selectData;
	    lo_FormElementOptionData *optionData;
	    int32 i;

	    selectData = &form_element->element_data->ele_select;
	    string = PR_sprintf_append(string, PT_SELECT);
	    if (selectData->name) {
		string = PR_sprintf_append(string, " %s=\"%s\"",
					   PARAM_NAME,
					   (char *)selectData->name);
	    }
	    if (selectData->size) {
		truelong = selectData->size;
		string = PR_sprintf_append(string, " %s=%ld\"",
					   PARAM_SIZE, truelong);
	    }
	    if (selectData->multiple) {
		string = PR_sprintf_append(string, " %s", PARAM_MULTIPLE);
	    }

	    string = PR_sprintf_append(string, ">\n");
	    PA_LOCK(optionData, lo_FormElementOptionData *,
		    selectData->options);
	    for (i = 0; i < selectData->option_cnt; i++) {
		string = PR_sprintf_append(string, "<%s", PT_OPTION);
		if (optionData[i].value) {
		    string = PR_sprintf_append(string, " %s=\"%s\"",
					       PARAM_VALUE,
					       optionData[i].value);
		}
		if (optionData[i].def_selected)
		    string = PR_sprintf_append(string, " %s", PARAM_SELECTED);
		string = PR_sprintf_append(string, ">");
		if (optionData[i].text_value) {
		    string = PR_sprintf_append(string, "%s",
					       optionData[i].text_value);
		}
		string = PR_sprintf_append(string, "\n");
	    }
	    PA_UNLOCK(selectData->options);

	    string = PR_sprintf_append(string, "</%s", PT_SELECT);
	}
	break;

      case FORM_TYPE_RADIO:
      case FORM_TYPE_CHECKBOX:
	{
	    lo_FormElementToggleData *toggle;

	    toggle = &form_element->element_data->ele_toggle;
	    string = PR_sprintf_append(string, "%s %s=\"%s\"",
				       PT_INPUT, PARAM_TYPE, typename);
	    if (toggle->name) {
		string = PR_sprintf_append(string, " %s=\"%s\"",
					   PARAM_NAME, (char *)toggle->name);
	    }
	    if (toggle->value) {
		string = PR_sprintf_append(string, " %s=\"%s\"",
					   PARAM_VALUE, (char *)toggle->value);
	    }
	    if (toggle->default_toggle)
		string = PR_sprintf_append(string, " %s", PARAM_CHECKED);
	}
	break;

      default:
	{
	    lo_FormElementMinimalData *minimal;

	    minimal = &form_element->element_data->ele_minimal;
	    string = PR_sprintf_append(string, "%s %s=\"%s\"",
				       PT_INPUT, PARAM_TYPE, typename);
	    if (minimal->name) {
		string = PR_sprintf_append(string, " %s=\"%s\"",
					   PARAM_NAME, (char *)minimal->name);
	    }
	    if (minimal->value) {
		string = PR_sprintf_append(string, " %s=\"%s\"",
					   PARAM_VALUE, (char *)minimal->value);
	    }
	}
	break;
    }

#define FROB(param) {                                                         \
    if (!JS_LookupProperty(cx, input->input_object, param, &result)) {        \
	PR_FREEIF(string);                                                    \
	return JS_FALSE;						      \
    }                                                                         \
    if (JS_TypeOfValue(cx, result) == JSTYPE_FUNCTION) {                      \
	JSFunction *fun = JS_ValueToFunction(cx, result);                     \
	if (!fun) {							      \
	    PR_FREEIF(string);                                                \
	    return JS_FALSE;						      \
	}                                                                     \
	str = JS_DecompileFunctionBody(cx, fun, 0);			      \
	value = JS_GetStringBytes(str);					      \
	length = strlen(value);                                               \
	if (length && value[length-1] == '\n') length--;                      \
	string = PR_sprintf_append(string," %s='%.*s'", param, length, value);\
    }                                                                         \
}

    FROB(lm_onFocus_str);
    FROB(lm_onBlur_str);
    FROB(lm_onSelect_str);
    FROB(lm_onChange_str);
    FROB(lm_onClick_str);
    FROB(lm_onScroll_str);
#undef FROB

    LO_UnlockLayout();

    string = PR_sprintf_append(string, ">");
    if (!string) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
    str = lm_LocalEncodingToStr(input->input_decoder->window_context, 
				string);
    XP_FREE(string);
    if (!str)
	return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;

bad:
    LO_UnlockLayout();
    return JS_FALSE;
}

static JSBool
input_method(JSContext *cx, JSObject *obj, jsval *argv,
	     uint32 event)
{
    JSInput *input;
    MWContext *context;
    LO_FormElementStruct *form_element;

    if (!JS_InstanceOf(cx, obj, &lm_input_class, argv))
        return JS_FALSE;
    input = JS_GetPrivate(cx, obj);
    if (!input)
	return JS_TRUE;
    context = input->input_decoder->window_context;
    if (!context)
	return JS_TRUE;
    LO_LockLayout();
    form_element = lm_GetFormElementByIndex(cx, JS_GetParent(cx, obj),
					    input->index);
    if (!form_element) {
	LO_UnlockLayout();
	return JS_TRUE;
    }
    input->input_event_mask |= event;
    ET_PostManipulateForm(context, (LO_Element *)form_element, event);
    input->input_event_mask &= ~event;
    LO_UnlockLayout();
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
input_focus(JSContext *cx, JSObject *obj,
	    uint argc, jsval *argv, jsval *rval)
{
    return input_method(cx, obj, argv, EVENT_FOCUS);
}

PR_STATIC_CALLBACK(JSBool)
input_blur(JSContext *cx, JSObject *obj,
	   uint argc, jsval *argv, jsval *rval)
{
    return input_method(cx, obj, argv, EVENT_BLUR);
}

PR_STATIC_CALLBACK(JSBool)
input_select(JSContext *cx, JSObject *obj,
	     uint argc, jsval *argv, jsval *rval)
{
    return input_method(cx, obj, argv, EVENT_SELECT);
}

PR_STATIC_CALLBACK(JSBool)
input_click(JSContext *cx, JSObject *obj,
	     uint argc, jsval *argv, jsval *rval)
{
    return input_method(cx, obj, argv, EVENT_CLICK);
}

#ifdef NOTYET
PR_STATIC_CALLBACK(JSBool)
input_enable(JSContext *cx, JSObject *obj,
	     uint argc, jsval *argv, jsval *rval)
{
    return input_method(cx, obj, argv, EVENT_ENABLE);
}

PR_STATIC_CALLBACK(JSBool)
input_disable(JSContext *cx, JSObject *obj,
	      uint argc, jsval *argv, jsval *rval)
{
    return input_method(cx, obj, argv, EVENT_DISABLE);
}
#endif /* NOTYET */

static JSFunctionSpec input_methods[] = {
    {lm_toString_str,   input_toString,        0},
    {"focus",           input_focus,            0},
    {"blur",            input_blur,             0},
    {"select",          input_select,           0},
    {"click",           input_click,            0},
#ifdef NOTYET
    {"enable",          input_enable,           0},
    {"disable",         input_disable,          0},
#endif /* NOTYET */
    {0}
};

/*
 * XXX move me somewhere else...
 */
enum input_array_slot {
    INPUT_ARRAY_LENGTH = -1
};

static JSPropertySpec input_array_props[] = {
    {lm_length_str, INPUT_ARRAY_LENGTH,
		    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT},
    {0}
};

typedef struct JSInputArray {
    JSInputBase	    base;
    uint            length;
} JSInputArray;

extern JSClass lm_input_array_class;

PR_STATIC_CALLBACK(JSBool)
input_array_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSInputArray *array;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    array = JS_GetInstancePrivate(cx, obj, &lm_input_array_class, NULL);
    if (!array)
	return JS_TRUE;
    switch (slot) {
      case INPUT_ARRAY_LENGTH:
	*vp = INT_TO_JSVAL(array->length);
	break;
    }
    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
input_array_finalize(JSContext *cx, JSObject *obj)
{
    JSInputArray *array;

    array = JS_GetPrivate(cx, obj);
    if (!array)
        return;
    DROP_BACK_COUNT(array->base_decoder);
    JS_free(cx, array);
}

JSClass lm_input_array_class = {
    "InputArray", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    input_array_getProperty, input_array_getProperty, JS_EnumerateStub,
    JS_ResolveStub, JS_ConvertStub, input_array_finalize
};

#ifdef NOTYET
PR_STATIC_CALLBACK(JSBool)
InputArray(JSContext *cx, JSObject *obj,
	   uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}
#endif


static void
lm_compile_event_handlers(MochaDecoder * decoder, 
			  LO_FormElementStruct * form_element, 
			  JSObject *obj,
			  PA_Tag *tag)
{
    MWContext * context = decoder->window_context;
    PA_Block method, id, keydown, keypress, keyup;
    JSInputBase *base;
    JSContext *cx;

    cx = decoder->js_context;
    base = JS_GetPrivate(cx, obj);

    keydown = lo_FetchParamValue(context, tag, PARAM_ONKEYDOWN);
    keypress = lo_FetchParamValue(context, tag, PARAM_ONKEYPRESS);
    keyup = lo_FetchParamValue(context, tag, PARAM_ONKEYUP);

    /* Text fields need this info. */
    if (keydown || keypress || keyup)
	form_element->event_handler_present = TRUE;

    LO_UnlockLayout();

    id = lo_FetchParamValue(context, tag, PARAM_ID);
    method = lo_FetchParamValue(context, tag, PARAM_ONCLICK);
    if (method) {
	(void) lm_CompileEventHandler(decoder, id, tag->data,
				      tag->newline_count, obj,
				      PARAM_ONCLICK, method);
	base->handlers |= HANDLER_ONCLICK;
	PA_FREE(method);
    }
    method = lo_FetchParamValue(context, tag, PARAM_ONFOCUS);
    if (method) {
	(void) lm_CompileEventHandler(decoder, id, tag->data,
				      tag->newline_count, obj,
				      PARAM_ONFOCUS, method);
	base->handlers |= HANDLER_ONFOCUS;
	PA_FREE(method);
    }
    method = lo_FetchParamValue(context, tag, PARAM_ONBLUR);
    if (method) {
	(void) lm_CompileEventHandler(decoder, id, tag->data,
				      tag->newline_count, obj,
				      PARAM_ONBLUR, method);
	base->handlers |= HANDLER_ONBLUR;
	PA_FREE(method);
    }
    method = lo_FetchParamValue(context, tag, PARAM_ONCHANGE);
    if (method) {
	(void) lm_CompileEventHandler(decoder, id, tag->data,
				      tag->newline_count, obj,
				      PARAM_ONCHANGE, method);
	base->handlers |= HANDLER_ONCHANGE;
	PA_FREE(method);
    }
    method = lo_FetchParamValue(context, tag, PARAM_ONSELECT);
    if (method) {
	(void) lm_CompileEventHandler(decoder, id, tag->data,
				      tag->newline_count, obj,
				      PARAM_ONSELECT, method);
	base->handlers |= HANDLER_ONSELECT;
	PA_FREE(method);
    }
    method = lo_FetchParamValue(context, tag, PARAM_ONSCROLL);
    if (method) {
	(void) lm_CompileEventHandler(decoder, id, tag->data,
				      tag->newline_count, obj,
				      PARAM_ONSCROLL, method);
	base->handlers |= HANDLER_ONSCROLL;
	PA_FREE(method);
    }
    method = lo_FetchParamValue(context, tag, PARAM_ONMOUSEDOWN);
    if (method) {
	(void) lm_CompileEventHandler(decoder, id, tag->data,
				      tag->newline_count, obj,
				      PARAM_ONMOUSEDOWN, method);
	base->handlers |= HANDLER_ONMOUSEDOWN;
	PA_FREE(method);
    }
    method = lo_FetchParamValue(context, tag, PARAM_ONMOUSEUP);
    if (method) {
	(void) lm_CompileEventHandler(decoder, id, tag->data,
				      tag->newline_count, obj,
				      PARAM_ONMOUSEUP, method);
	base->handlers |= HANDLER_ONMOUSEUP;
	PA_FREE(method);
    }
    if (keydown) {
	(void) lm_CompileEventHandler(decoder, id, tag->data,
				      tag->newline_count, obj,
				      PARAM_ONKEYDOWN, keydown);
	base->handlers |= HANDLER_ONKEYDOWN;
	PA_FREE(keydown);
    }
    if (keyup) {
	(void) lm_CompileEventHandler(decoder, id, tag->data,
				      tag->newline_count, obj,
				      PARAM_ONKEYUP, keyup);
	base->handlers |= HANDLER_ONKEYUP;
	PA_FREE(keyup);
    }
    if (keypress) {
	(void) lm_CompileEventHandler(decoder, id, tag->data,
				      tag->newline_count, obj,
				      PARAM_ONKEYPRESS, keypress);
	base->handlers |= HANDLER_ONKEYPRESS;
	PA_FREE(keypress);
    }
    method = lo_FetchParamValue(context, tag, PARAM_ONDBLCLICK);
    if (method) {
	(void) lm_CompileEventHandler(decoder, id, tag->data,
				      tag->newline_count, obj,
				      PARAM_ONDBLCLICK, method);
	base->handlers |= HANDLER_ONDBLCLICK;
	PA_FREE(method);
    }
    if (id)
	PA_FREE(id);
    LO_LockLayout();
}

#define ANTI_RECURSIVE_KLUDGE	((JSObject *)1)

/*
 * Reflect a bunch of different types of form elements into JS.
 */
JSObject *
LM_ReflectFormElement(MWContext *context, int32 layer_id, int32 form_id,
                      int32 element_id, PA_Tag * tag)
{
    JSObject *obj, *form_obj, *prototype, *old_obj, *array_obj;
    LO_FormElementData *data;
    LO_FormElementStruct *form_element;
    MochaDecoder *decoder;
    JSContext *cx;
    int32 type;
    char *name = NULL;
    JSBool ok;
    size_t size;
    JSInput *input;
    JSClass *clasp;
    JSInputBase *base;
    JSInputArray *array;
    jsval val;
    static uint recurring;	/* XXX thread-unsafe */
    lo_FormData * form_data;
    lo_TopState *top_state;
    int32 element_index;

    /* reflect the form */
    if (!LM_ReflectForm(context, NULL, NULL, layer_id, form_id)) 
	return NULL;

    /* make the form the active form */
    decoder = LM_GetMochaDecoder(context);
    if (!decoder)
	return NULL;
    
    LM_PutMochaDecoder(decoder);

    /* if this is a radio button we're gonna need to get this later */
    if (tag)
	((PA_Tag *)tag)->lo_data = (void*)element_id;

    form_data = LO_GetFormDataByID(context, layer_id, form_id);
    if (!form_data || !form_data->mocha_object)
	return NULL;

    form_obj = form_data->mocha_object;

    form_element = LO_GetFormElementByIndex(form_data, element_id);
    if (!form_element || !form_element->element_data)
	return NULL;
    data = form_element->element_data;

    /* see if we've already reflected it (or are reflecting it) */
    obj = form_element->mocha_object;
    if (obj) {
        if (obj == ANTI_RECURSIVE_KLUDGE)
	    return NULL;

	/*
	 * This object might have already gotten reflected but it might
	 *   not have had its tag (and thus event handlers) at the time
	 *   it was reflected
	 */
	if (tag)
    	    lm_compile_event_handlers(decoder, form_element, obj, tag);

	return obj;
    }

    decoder = LM_GetMochaDecoder(context);
    if (!decoder)
        return NULL;

    cx = decoder->js_context;

    top_state = lo_GetMochaTopState(context);
    if (top_state->resize_reload) {
        obj = lm_GetFormElementFromMapping(cx, form_obj, element_id);
        if (obj) {
            form_element->mocha_object = obj;
            LM_PutMochaDecoder(decoder);
            return obj;
        }
    }

    prototype = decoder->input_prototype;

    type = data->type;
    if ((char *)data->ele_minimal.name)
	name = XP_STRDUP((char *)data->ele_minimal.name);
    switch (type) {
      case FORM_TYPE_TEXT:
      case FORM_TYPE_TEXTAREA:
        size = sizeof(JSTextInput);
        break;

      case FORM_TYPE_RADIO:
        if (!recurring) {
	    recurring++;
	    ok = lm_ReflectRadioButtonArray(context, layer_id,
                                            form_element->form_id,
                                            name, tag);
	    recurring--;
	    obj = form_element->mocha_object;
	    if (obj) {
		LM_PutMochaDecoder(decoder);
		return obj;
	    }
        }
        /* FALL THROUGH */

      default:
        size = sizeof(JSInput);
        break;
    }

    input = JS_malloc(cx, size);
    if (!input)
	goto fail;
    XP_BZERO(input, size);

    obj = JS_NewObject(cx, &lm_input_class, prototype, form_obj);
    if (!obj || !JS_SetPrivate(cx, obj, input)) {
        JS_free(cx, input);
	goto fail;
    }

    /* 
     * get val before we lose the form_element since 
     * lm_compile_event_handlers() is going to lose the layout lock 
     */
    if (name) {
	form_element->mocha_object = ANTI_RECURSIVE_KLUDGE;
	ok = JS_LookupProperty(cx, form_obj, name, &val);
	form_element->mocha_object = NULL;
	if (!ok) {
	    LM_PutMochaDecoder(decoder);
	    return NULL;
	}
    }

    element_index = form_element->element_index;

    /* see if there are any event handlers we need to compile */
    if (tag)
	lm_compile_event_handlers(decoder, form_element, obj, tag);

    /*
     * In 3.0 we would reflect hidden elements only if they had event
     *   handlers, not just a name attribute.
     */
    if (type == FORM_TYPE_HIDDEN && JS_GetVersion(cx) < JSVERSION_1_2) {
	base = JS_GetPrivate(cx, obj);
	if (!base || (!name && !base->handlers)) 
	    goto fail;
    }

    array_obj = NULL;

    if (name) {
	old_obj = JSVAL_IS_OBJECT(val) ? JSVAL_TO_OBJECT(val) : NULL;
	if (old_obj) {
	    clasp = JS_GetClass(old_obj);
	    if (clasp != &lm_input_class && clasp != &lm_input_array_class)
	    	old_obj = NULL;
	}

	if (old_obj) {
	    base = JS_GetPrivate(cx, old_obj);
	    if (!base) 
		goto fail;

	    if (JS_GetVersion(cx) < JSVERSION_1_2 &&
		base->type == FORM_TYPE_HIDDEN) {
		/*
		 * We have two or more elements of the form with the same name.
		 * For JavaScript1.1 or	earlier some peculiarities apply to a
		 * set of form elements with the same name. If any elements in
		 * the set had handlers, then only those elements with handlers
		 * would be reflected. Otherwise, all form elements in the set
		 * are reflected.
		 */
		JSObject *temp_obj;
		jsval result;
		JSBool currentHasHandler;
		JSBool accumulatedHasHandlers;
		JSInputBase *currentBase;

		currentBase = JS_GetPrivate(cx, obj);
		if (!currentBase)
		    goto fail;

		currentHasHandler = (JSBool)(currentBase->handlers != 0);
		temp_obj = old_obj;
		if (clasp == &lm_input_array_class) {
   		    JS_GetElement(cx, old_obj, 0, &result);
		    temp_obj = JSVAL_TO_OBJECT(result);
		}
		accumulatedHasHandlers = (JSBool)(
		  (JS_LookupProperty(cx, temp_obj, lm_onClick_str, &result) &&
		   JS_TypeOfValue(cx, result) == JSTYPE_FUNCTION) ||
		  (JS_LookupProperty(cx, temp_obj, lm_onFocus_str, &result) &&
		   JS_TypeOfValue(cx, result) == JSTYPE_FUNCTION) ||
		  (JS_LookupProperty(cx, temp_obj, lm_onBlur_str, &result) &&
		   JS_TypeOfValue(cx, result) == JSTYPE_FUNCTION) ||
		  (JS_LookupProperty(cx, temp_obj, lm_onChange_str, &result) &&
		   JS_TypeOfValue(cx, result) == JSTYPE_FUNCTION) ||
		  (JS_LookupProperty(cx, temp_obj, lm_onSelect_str, &result) &&
		   JS_TypeOfValue(cx, result) == JSTYPE_FUNCTION) ||
		  (JS_LookupProperty(cx, temp_obj, lm_onScroll_str, &result) &&
		   JS_TypeOfValue(cx, result) == JSTYPE_FUNCTION));

		if (currentHasHandler && !accumulatedHasHandlers) {
		    /*
		     * Replace the accumulated form elements with this one.
		     * That way, we will create an array for form elements with
		     * the same name, adding only those elements that have
		     * handlers, unless no elements have handlers, in which
		     * case all are reflected.
		     */
		    JS_DeleteProperty(cx, form_obj, name);
		    old_obj = NULL;
		} else if (!currentHasHandler && accumulatedHasHandlers) {
		    /* Don't add the current form element to the array. */
		    goto fail;
		}
	    }
	}

	if (old_obj) {
	    if (clasp == &lm_input_class) {
		/* Make an array out of the previous element and this one. */
		array = JS_malloc(cx, sizeof *array);
		if (!array) 
		    goto fail;
		XP_BZERO(array, sizeof *array);

		/*
		 * Lock old_obj temporarily until we remove it from form_obj
		 * and add it as a property of the radio button array.
		 */
		JS_LockGCThing(cx, old_obj);
		JS_DeleteProperty(cx, form_obj, name);

		/* XXXbe use JS_InitClass instead of this! */
		array_obj = JS_DefineObject(cx, form_obj, name,
					    &lm_input_array_class,
					    NULL,
					    JSPROP_ENUMERATE|JSPROP_READONLY);

		if (array_obj && !JS_SetPrivate(cx, array_obj, array))
		    array_obj = NULL;

		if (array_obj &&
		    !JS_DefineProperties(cx, array_obj, input_array_props)) {
		    array_obj = NULL;
		}

		if (!array_obj) {
		    JS_UnlockGCThing(cx, old_obj);
		    JS_free(cx, array);
		    goto fail;
		}
		array->base_decoder = HOLD_BACK_COUNT(decoder);
		array->base_type = base->type;

		/* Insert old_obj (referred to by val) into the array. */
		if (!JS_DefineElement(cx, array_obj, (jsint) array->length,
				      val, NULL, NULL,
				      JSPROP_ENUMERATE|JSPROP_READONLY)) {
    		    JS_UnlockGCThing(cx, old_obj);
		    goto fail;
		}
		array->length++;
		JS_UnlockGCThing(cx, old_obj);

	    } else {
		array_obj = old_obj;
		array = (JSInputArray *)base;
	    }

	    /* ugly hack to prevent rebinding in lm_AddFormElement */
	    name = NULL;

	    if (!JS_DefineElement(cx, array_obj, (jsint) array->length,
				  OBJECT_TO_JSVAL(obj), NULL, NULL,
				  JSPROP_ENUMERATE|JSPROP_READONLY)) {
		goto fail;
	    }
	    array->length++;
	}
    }

    input->input_decoder = HOLD_BACK_COUNT(decoder);
    input->input_type = type;
    input->input_object = obj;
    input->index = element_index;

    /* 
     * get the form_element again incase it changed when we release the
     *   layout lock
     */
    form_element = LO_GetFormElementByIndex(form_data, element_id);
    if (form_element)
	form_element->mocha_object = obj;

    if (!lm_AddFormElement(cx, form_obj, obj, name, input->index)) {
	/* XXX undefine name if it's non-null? */
    }

    LM_PutMochaDecoder(decoder);
    XP_FREEIF(name);
    return obj;

fail:
    LM_PutMochaDecoder(decoder);
    XP_FREEIF(name);
    return NULL;

}

JSBool
lm_InitInputClasses(MochaDecoder *decoder)
{
    JSContext *cx;
    JSObject *prototype;

    cx = decoder->js_context;
    prototype = JS_InitClass(cx, decoder->window_object,
			     decoder->event_receiver_prototype, &lm_input_class,
			     Input, 0, input_props, input_methods, NULL, NULL);
    if (!prototype)
	return JS_FALSE;
    decoder->input_prototype = prototype;

    prototype = JS_InitClass(cx, decoder->window_object, NULL, &lm_option_class,
			     Option, 0, option_props, NULL, NULL, NULL);
    if (!prototype)
	return JS_FALSE;
    decoder->option_prototype = prototype;
    return JS_TRUE;
}

#define MAX_KEY_NUM 256

#define KEY_STATE_DOWN	    0x00000001
#define KEY_STATE_UP	    0x00000002
#define KEY_STATE_PRESS	    0x00000004      /* user is mousing over a link */
#define KEY_STATE_CANCEL    0x00000008      /* user is mousing out of a link */

static uint8 key_state[MAX_KEY_NUM];

/* We need to look here to see if any KEYPRESS events coming in were cancelled
 * at the KEYDOWN phase and should be blocked.  We also need to use the KEYDOWN
 * and KEYUP messages to update this state.  After this we can normally process
 * through lm_InputEvent.
 */

JSBool
lm_KeyInputEvent(MWContext *context, LO_Element *element, JSEvent *pEvent, jsval *rval)
{
    JSBool ok = JS_TRUE;

    if (pEvent->which > 255)
        return lm_InputEvent(context, element, pEvent, rval);

    switch (pEvent->type) {
	case EVENT_KEYDOWN:
	    key_state[pEvent->which] = (uint8)KEY_STATE_DOWN;
	    break;

	case EVENT_KEYPRESS:
	    if (key_state[pEvent->which] == KEY_STATE_CANCEL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		LO_UnlockLayout();
		return JS_TRUE;
	    }
	    key_state[pEvent->which] = (uint8)KEY_STATE_PRESS;
	    break;

	case EVENT_KEYUP:
	    key_state[pEvent->which] = (uint8)KEY_STATE_UP;
	    break;
	default:
	    break;
    }
    ok = lm_InputEvent(context, element, pEvent, rval);

    if (pEvent->type == EVENT_KEYDOWN && *rval == JSVAL_FALSE)
	key_state[pEvent->which] = (uint8)KEY_STATE_CANCEL;
    return ok;
}

#define MAX_MOUSE_NUM 4

#define MOUSE_STATE_DOWN    0x00000001
#define MOUSE_STATE_UP	    0x00000002
#define MOUSE_STATE_CANCEL  0x00000004
#define MOUSE_STATE_DBLCLICK 0x00000008

static uint8 mouse_state[MAX_MOUSE_NUM];

/* If a mousedown is cancelled we do not allow mouseups to do anything.  This is
 * because all Navigator mouse responses are based on a down followed by an up.
 * This has the net effect of returning false from all mouseups if the previous
 * mousedown was cancelled.
 */

JSBool
lm_MouseInputEvent(MWContext *context, LO_Element *element, JSEvent *pEvent, jsval *rval)
{
    JSBool ok = JS_TRUE;
    JSEvent *dcEvent;

    switch (pEvent->type) {
	case EVENT_MOUSEDOWN:
	    mouse_state[pEvent->which] = (uint8)MOUSE_STATE_DOWN;
	    break;
	case EVENT_DBLCLICK:
	    mouse_state[pEvent->which] = (uint8)MOUSE_STATE_DBLCLICK;
	    pEvent->type = EVENT_MOUSEDOWN;
	    break;
	default:
	    break;
    }

    ok = lm_InputEvent(context, element, pEvent, rval);

    switch (pEvent->type) {
	case EVENT_MOUSEDOWN:
	    if (*rval == JSVAL_FALSE)
		mouse_state[pEvent->which] = (uint8)MOUSE_STATE_CANCEL;
	    break;
	case EVENT_MOUSEUP:
	    if (mouse_state[pEvent->which] == MOUSE_STATE_CANCEL) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	    }
	    else if (*rval != JSVAL_FALSE &&
		     mouse_state[pEvent->which] == MOUSE_STATE_DBLCLICK) {
		dcEvent = XP_NEW_ZAP(JSEvent);
		dcEvent->type = EVENT_DBLCLICK;
		dcEvent->x = pEvent->x;
		dcEvent->y = pEvent->y;
		dcEvent->docx = pEvent->docx;
		dcEvent->docy = pEvent->docy;
		dcEvent->screenx = pEvent->screenx;
		dcEvent->screeny = pEvent->screeny;
		dcEvent->which = pEvent->which;
		dcEvent->modifiers = pEvent->modifiers;
		dcEvent->layer_id = pEvent->layer_id;

		LO_LockLayout();
		ok = lm_InputEvent(context, element, dcEvent, rval);

		if (!dcEvent->saved)
		    XP_FREE(dcEvent);
	    }
	    mouse_state[pEvent->which] = (uint8)MOUSE_STATE_UP;
	    break;
	default:
	    break;
    }
    return ok;
}

/*
 * OK, we assume our caller has locked layout so that we can hold
 *   on to the element pointer.  As soon as we are done with the
 *   element pointer it is up to us to make sure we unlock layout.
 * Unlock layout before we call lm_SendEvent() so that we don't go
 *   re-entrant into the mozilla thread (and also so we hold the
 *   lock for as little time as possible)
 */
JSBool
lm_InputEvent(MWContext *context, LO_Element *element, JSEvent *pEvent,
            jsval *rval)
{
    JSContext *cx;
    MochaDecoder *decoder = NULL;
    JSBool ok;
    LO_AnchorData *anchor;
    JSObject *obj;
    JSDocument *doc;
    JSEventCapturer *cap;
    JSEventReceiver *rec;
    JSInputHandler *handler;
    LO_FormElementData *data;
    lo_FormData *form_data;
    JSString *str;
    char *re_input_bytes = NULL;
    JSBool multiline = JS_FALSE;
    int16 type;
    JSBool event_receiver_type = JS_FALSE;
    int32 layer_id, active_layer_id;

    *rval = JSVAL_VOID;
    cx = context->mocha_context;
    if (!cx) {
	LO_UnlockLayout();
	return JS_TRUE;
    }

    /*
     * If the event is has no element and is one of the event types listed
     * in the if statement it is being sent to the layer or window.  Handle
     * these first.
     */
    if (!element && (pEvent->type == EVENT_FOCUS || pEvent->type == EVENT_BLUR ||
        pEvent->type == EVENT_MOUSEOVER ||
        pEvent->type == EVENT_MOUSEOUT)) {

	if (pEvent->layer_id == LO_DOCUMENT_LAYER_ID) {

	    decoder = LM_GetMochaDecoder(context);
	    if (!decoder) {
		LO_UnlockLayout();
		return JS_FALSE;
	    }

	    /* Send event to the window. */
            obj = decoder->window_object;

	    LO_UnlockLayout();

	    if (decoder->event_mask & pEvent->type) {
		ok = JS_TRUE;
	    }
	    else {
		decoder->event_mask |= pEvent->type;
		ok = lm_SendEvent(context, obj, pEvent, rval);
		decoder->event_mask &= ~pEvent->type;
	    }
	    LM_PutMochaDecoder(decoder);
	}
        else {
	    /* Send event to the layer matching the layer_id. */
            obj = LO_GetLayerMochaObjectFromId(context, pEvent->layer_id);

	    LO_UnlockLayout();

	    if (!obj)
		return JS_FALSE;

	    cap = JS_GetPrivate(cx, obj);
	    if (!cap)
		return JS_FALSE;

	    if (cap->base.event_mask & pEvent->type){
		ok = JS_TRUE;
	    }
	    else {
		cap->base.event_mask |= pEvent->type;
		ok = lm_SendEvent(context, obj, pEvent, rval);
		cap->base.event_mask &= ~pEvent->type;
	    }
	}

        return ok;
    }

    type = element ? element->type : LO_NONE;

    /* If we're over plain text its easier to do this now than in the switch */
    if (type == LO_TEXT && !element->lo_text.anchor_href && 
		    LM_EventCaptureCheck(context, pEvent->type))	{
	type = LO_NONE;
    }

    switch (type) {
      case LO_TEXT:
	anchor = element->lo_text.text ? element->lo_text.anchor_href : 0;
	obj = anchor ? anchor->mocha_object : 0;
#ifdef DOM
	/* If this layout element is within a span, set the mocha object to 
	   the containing SPAN's mocha object */
	if (LO_IsWithinSpan( element ))
	{
		obj = LO_GetMochaObjectOfParentSpan( element );
	}
#endif
	if (!obj) {
	    if (!LM_EventCaptureCheck(context, pEvent->type) || !anchor) {
	        LO_UnlockLayout();
		return JS_TRUE;
	    }
	    /* Reflect the anchor now because someone is capturing */
	    layer_id = LO_GetIdFromLayer(context, anchor->layer);
	    active_layer_id = LM_GetActiveLayer(context);
	    LM_SetActiveLayer(context, pEvent->layer_id);
	    LO_EnumerateLinks(context, pEvent->layer_id);
	    LM_SetActiveLayer(context, active_layer_id);
	    obj = anchor->mocha_object;
	}
	re_input_bytes = (char *)element->lo_text.text;
	multiline = JS_TRUE;
	break;
      case LO_IMAGE:
	anchor = element->lo_image.image_attr ? element->lo_image.anchor_href : 0;
	if (anchor) {
	    obj = anchor->mocha_object;
	}
	else {
	    obj = element->lo_image.image_attr ? element->lo_image.mocha_object : 0;
	    event_receiver_type = JS_TRUE;
	}
	if (!obj) {
	    if (!LM_EventCaptureCheck(context, pEvent->type) || 
					    !element->lo_image.image_attr) {
	        LO_UnlockLayout();
		return JS_TRUE;
	    }
	    /* Reflect the object now because someone is capturing */
	    if (anchor) {
		layer_id = LO_GetIdFromLayer(context, anchor->layer);
		active_layer_id = LM_GetActiveLayer(context);
		LM_SetActiveLayer(context, layer_id);
		LO_EnumerateLinks(context, layer_id);
		LM_SetActiveLayer(context, active_layer_id);
		obj = anchor->mocha_object;
	    }
	    else {
		active_layer_id = LM_GetActiveLayer(context);
		LM_SetActiveLayer(context, element->lo_image.layer_id);
		LO_EnumerateImages(context, element->lo_image.layer_id);
		LM_SetActiveLayer(context, active_layer_id);
		obj = element->lo_image.mocha_object;
		event_receiver_type = JS_TRUE;		
	    }
	}
	break;
      case LO_FORM_ELE:
	obj = element->lo_form.element_data ? element->lo_form.mocha_object:0;
	if (!obj) {
	    if (!LM_EventCaptureCheck(context, pEvent->type) ||
				    !element->lo_form.element_data) {
	        LO_UnlockLayout();
		return JS_TRUE;
	    }
	    /* Reflect the object now because someone is capturing */
	    active_layer_id = LM_GetActiveLayer(context);
	    LM_SetActiveLayer(context, element->lo_form.layer_id);
	    LO_EnumerateForms(context, element->lo_form.layer_id);
	    form_data = LO_GetFormDataByID(context, element->lo_form.layer_id, 
						element->lo_form.form_id);
	    if (!form_data)	{
		LM_SetActiveLayer(context, active_layer_id);
		LO_UnlockLayout();
		return JS_TRUE;
	    }
	    LO_EnumerateFormElements(context, form_data);
	    LM_SetActiveLayer(context, active_layer_id);
	    obj = element->lo_form.mocha_object;
	}
	data = element->lo_form.element_data;
	switch (data->type) {
	  case FORM_TYPE_TEXT:
	    re_input_bytes = (char *)data->ele_text.current_text;
	    break;
	  case FORM_TYPE_TEXTAREA:
	    re_input_bytes = (char *)data->ele_textarea.current_text;
	    multiline = JS_TRUE;
	    break;
	  case FORM_TYPE_SELECT_ONE:
	  case FORM_TYPE_SELECT_MULT:
	    {
		lo_FormElementSelectData *selectData;
		lo_FormElementOptionData *optionData;
		int32 i;

		selectData = &data->ele_select;
		optionData = (lo_FormElementOptionData *) selectData->options;
		for (i = 0; i < selectData->option_cnt; i++) {
		    if (optionData[i].selected) {
			re_input_bytes = (char *)optionData[i].text_value;
			break;
		    }
		}
	    }
	    break;
	}
	break;
      default:
	/* Any event over nothing or a non-reflectable layout element (linefeeds,
	 * horizontal rules, etc) goes to the main document or layer document.
	 */
	decoder = LM_GetMochaDecoder(context);
	if (!decoder) {
	    LO_UnlockLayout();
	    return JS_FALSE;
	}

	obj = lm_GetDocumentFromLayerId(decoder, pEvent->layer_id);
        LO_UnlockLayout();
	LM_PutMochaDecoder(decoder);

	if (!obj)
	    return JS_FALSE;

	doc = JS_GetPrivate(cx, obj);
	if (!doc) 
	    return JS_FALSE;

	if (doc->capturer.base.event_mask & pEvent->type) {
	    ok = JS_TRUE;
	}
	else {
	    doc->capturer.base.event_mask |= pEvent->type;
	    ok = lm_SendEvent(context, obj, pEvent, rval);
	    doc->capturer.base.event_mask &= ~pEvent->type;
	}
	return ok;
    }

    /* whether we got an object or not we are done with the element ptr */
    LO_UnlockLayout();

    if (!obj) {
	XP_ASSERT(0);
	return JS_FALSE;
    }

    /* Images do not have the same base private data structure as the 
     * other input elements do so we must use a different private data
     * structs.  Eventually these should be unified for all event receivers.
     */
    if (event_receiver_type) {
	rec = JS_GetPrivate(cx, obj);
	if (!rec || rec->event_mask & pEvent->type)
    	    return JS_FALSE;
    }
    else {
	handler = JS_GetPrivate(cx, obj);
	if (!handler || handler->event_mask & pEvent->type) 
    	    return JS_FALSE;
    }

    decoder = LM_GetMochaDecoder(context);
    if (!decoder)
	return JS_FALSE;
    decoder->event_receiver = obj;
    LM_PutMochaDecoder(decoder);

    if (re_input_bytes) {
	str = lm_LocalEncodingToStr(context, re_input_bytes);
	if (!str)
	    return JS_FALSE;
	JS_SetRegExpInput(cx, str, multiline);
    }

    if (event_receiver_type)
	rec->event_mask |= pEvent->type;
    else
	handler->event_mask |= pEvent->type;

    ok = lm_SendEvent(context, obj, pEvent, rval);

    if (event_receiver_type)
	rec->event_mask &= ~pEvent->type;
    else
	handler->event_mask &= ~pEvent->type;
       
    if (re_input_bytes)
	JS_ClearRegExpStatics(cx);
    return ok;
}
