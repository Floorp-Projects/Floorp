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
 * JS reflection of Navigator Components.
 *
 * Created: Tom Pixley, 4/22/97
 *
 */

#include "lm.h"
#include "prmem.h"
#include "np.h"
#include "net.h"
#include "fe_proto.h"

/*
 * -----------------------------------------------------------------------
 *
 * Data types
 *
 * -----------------------------------------------------------------------
 */

typedef struct JSComponent
{
    MochaDecoder	*decoder;
    JSObject		*obj;
    JSString	        *name;
    ETBoolPtrFunc	 active_callback;
    ETVoidPtrFunc	 startup_callback;
} JSComponent;

typedef struct JSComponentArray
{
    MochaDecoder       *decoder;
    JSObject	       *obj;
    jsint		length;
} JSComponentArray;

typedef struct JSPreDefComponent
{
    const char *name;
    ETVerifyComponentFunc func;
} JSPreDefComponent;

static JSPreDefComponent predef_components[] =
{
    {0}
};

/*
 * -----------------------------------------------------------------------
 *
 * Reflection of an installed component.
 *
 * -----------------------------------------------------------------------
 */

enum component_slot
{
    COMPONENT_NAME        = -1,
    COMPONENT_ACTIVE      = -2
};


static JSPropertySpec component_props[] =
{
    {"name",        COMPONENT_NAME,	JSPROP_ENUMERATE | JSPROP_READONLY},
    {"active",	    COMPONENT_ACTIVE,	JSPROP_ENUMERATE | JSPROP_READONLY},
    {0}
};

extern JSClass lm_component_class;

PR_STATIC_CALLBACK(JSBool)
component_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSComponent *component;
    JSString *str;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    component = JS_GetInstancePrivate(cx, obj, &lm_component_class, NULL);
    if (!component)
	return JS_TRUE;

    switch (slot) {
      case COMPONENT_NAME:
        str = component->name;
	if (str)
	    *vp = STRING_TO_JSVAL(str);
	else
	    *vp = JS_GetEmptyStringValue(cx);
        break;

      case COMPONENT_ACTIVE:
	*vp = JSVAL_FALSE;
	if (ET_moz_CallFunctionBool((ETBoolPtrFunc)component->active_callback, NULL))
	    *vp = JSVAL_TRUE;
        break;

      default:
	break;
    }

    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
component_mozilla_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    char *name, *type_str, *get_str;
    jsval type, func;

    if (!JSVAL_IS_STRING(id))
	return JS_TRUE;

    name = JS_GetStringBytes(JSVAL_TO_STRING(id));

    type_str = JS_malloc(cx, XP_STRLEN(lm_typePrefix_str) + XP_STRLEN(name) + 1);
    get_str = JS_malloc(cx, XP_STRLEN(lm_getPrefix_str) + XP_STRLEN(name) + 1);
    if (!type_str || !get_str)
	return JS_TRUE;

    XP_STRCPY(type_str, lm_typePrefix_str);
    XP_STRCAT(type_str, name);
    XP_STRCPY(get_str, lm_getPrefix_str);
    XP_STRCAT(get_str, name);

    if (!JS_GetProperty(cx, obj, type_str, &type) ||
	!JSVAL_IS_INT(type) ||
	!JS_GetProperty(cx, obj, get_str, &func))
	return JS_TRUE;

    JS_free(cx, type_str);
    JS_free(cx, get_str);

    switch(JSVAL_TO_INT(type)) {
    case ARGTYPE_INT32:
	*vp = INT_TO_JSVAL((int32)ET_moz_CompGetterFunction((ETCompPropGetterFunc)JSVAL_TO_INT(func), name));
	break;
    case ARGTYPE_BOOL:
	*vp = BOOLEAN_TO_JSVAL((JSBool)ET_moz_CompGetterFunction((ETCompPropGetterFunc)JSVAL_TO_INT(func), name));
	break;
    case ARGTYPE_STRING:
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, 
		(char*)ET_moz_CompGetterFunction((ETCompPropGetterFunc)JSVAL_TO_INT(func), name)));
	break;
    }

    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
component_mozilla_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    char *name, *type_str, *set_str, *prop_val;
    jsval type, func;

    if (!JSVAL_IS_STRING(id))
	return JS_TRUE;

    name = JS_GetStringBytes(JSVAL_TO_STRING(id));

    type_str = JS_malloc(cx, XP_STRLEN(lm_typePrefix_str) + XP_STRLEN(name) + 1);
    set_str = JS_malloc(cx, XP_STRLEN(lm_setPrefix_str) + XP_STRLEN(name) + 1);
    if (!type_str || !set_str)
	return JS_TRUE;

    XP_STRCPY(type_str, lm_typePrefix_str);
    XP_STRCAT(type_str, name);
    XP_STRCPY(set_str, lm_setPrefix_str);
    XP_STRCAT(set_str, name);
    if (!JS_GetProperty(cx, obj, type_str, &type) ||
	!JSVAL_IS_INT(type) ||
	!JS_GetProperty(cx, obj, set_str, &func))
	return JS_TRUE;

    JS_free(cx, type_str);
    JS_free(cx, set_str);
  
    switch(JSVAL_TO_INT(type)) {
    case ARGTYPE_INT32:
	if (JSVAL_IS_INT(*vp))
	    ET_moz_CompSetterFunction((ETCompPropSetterFunc)JSVAL_TO_INT(func), name, (void*)JSVAL_TO_INT(*vp));
	break;
    case ARGTYPE_BOOL:
	if (JSVAL_IS_BOOLEAN(*vp))
	    ET_moz_CompSetterFunction((ETCompPropSetterFunc)JSVAL_TO_INT(func), name, (void*)JSVAL_TO_BOOLEAN(*vp));
	break;
    case ARGTYPE_STRING:
	if (JSVAL_IS_STRING(*vp)) {
	    prop_val = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
	    ET_moz_CompSetterFunction((ETCompPropSetterFunc)JSVAL_TO_INT(func), name, (void*)prop_val);
	}
	break;
    }

    return JS_TRUE;
}

void
lm_RegisterComponentProp(const char *comp, const char *targetName, 
			 uint8 retType, ETCompPropSetterFunc setter, 
			 ETCompPropGetterFunc getter)
{
    JSContext *cx;
    JSObject *arrayObj, *obj;
    jsval val;
    char *type, *set, *get;
    
    if (!comp || !targetName || !(cx = lm_crippled_decoder->js_context))
	return;

    arrayObj = lm_DefineComponents(lm_crippled_decoder);
    if (!arrayObj)
	return;

    if (!JS_GetProperty(cx, arrayObj, comp, &val) ||
        !JSVAL_IS_OBJECT(val))
	return;

    obj = JSVAL_TO_OBJECT(val);

    if (!JS_DefineProperty(cx, obj, targetName, JSVAL_VOID, component_mozilla_getProperty, 
		       component_mozilla_setProperty, 0)) {
    }

    type = JS_malloc(cx, XP_STRLEN(lm_typePrefix_str) + XP_STRLEN(targetName) + 1);
    if (type) {
	XP_STRCPY(type, lm_typePrefix_str);
	XP_STRCAT(type, targetName);
	if (!JS_DefineProperty(cx, obj, type,
			   INT_TO_JSVAL((int32)retType), 0, 0, JSPROP_READONLY)) {
	}
	JS_free(cx, type);
    }

    get = JS_malloc(cx, XP_STRLEN(lm_getPrefix_str) + XP_STRLEN(targetName) + 1);
    if (get) {
	XP_STRCPY(get, lm_getPrefix_str);
	XP_STRCAT(get, targetName);
	if (!JS_DefineProperty(cx, obj, get,
			   INT_TO_JSVAL(getter), 0, 0, JSPROP_READONLY)) {
	}
	JS_free(cx, get);
    }

    set = JS_malloc(cx, XP_STRLEN(lm_setPrefix_str) + XP_STRLEN(targetName) + 1);
    if (set) {
	XP_STRCPY(set, lm_setPrefix_str);
	XP_STRCAT(set, targetName);
	if (!JS_DefineProperty(cx, obj, set,
			   INT_TO_JSVAL(setter), 0, 0, JSPROP_READONLY)) {
	}
	JS_free(cx, set);
    }
}

PR_STATIC_CALLBACK(JSBool)
component_resolve_name(JSContext *cx, JSObject *obj, jsval id)
{
    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
component_finalize(JSContext *cx, JSObject *obj)
{
    JSComponent* component;

    component = JS_GetPrivate(cx, obj);
    if (!component)
	return;
    JS_UnlockGCThing(cx, component->name);
    DROP_BACK_COUNT(component->decoder);
    JS_free(cx, component);
}

JSClass lm_component_class =
{
    "Component", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    component_getProperty, component_getProperty, JS_EnumerateStub,
    component_resolve_name, JS_ConvertStub, component_finalize
};

PR_STATIC_CALLBACK(JSBool)
component_activate(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    JSComponent *component;

    component = JS_GetInstancePrivate(cx, obj, &lm_component_class, argv);
    if (!component)
	return JS_FALSE;

    ET_moz_CallFunctionAsync((ETVoidPtrFunc)component->startup_callback, NULL);

    return JS_TRUE;
}

static JSFunctionSpec component_methods[] =
{
    {"activate", component_activate, 0},
    {0}
};

PR_STATIC_CALLBACK(JSBool)
component_mozilla_method_stub(JSContext *cx, JSObject *obj, uint argc, 
			      jsval *argv, jsval *rval)
{
    JSFunction *func;
    JSObject *func_obj;
    jsval type, native;
    uint i;
    JSCompArg *comp_argv;

    func = JS_ValueToFunction(cx, argv[-2]);
    func_obj = JS_GetFunctionObject(func);

    if (!JS_GetProperty(cx, func_obj, lm_typePrefix_str, &type) ||
	!JSVAL_IS_INT(type) ||
	!JS_GetProperty(cx, func_obj, lm_methodPrefix_str, &native) ||
	!(comp_argv = JS_malloc(cx, argc * sizeof(JSCompArg))))
	return JS_TRUE;
  
    for (i=0; i<argc; i++) {
	if (JSVAL_IS_INT(argv[i])) {
	    comp_argv[i].type = ARGTYPE_INT32;
	    comp_argv[i].value.intArg = JSVAL_TO_INT(argv[i]);
	}
	else if (JSVAL_IS_BOOLEAN(argv[i])) {
	    comp_argv[i].type = ARGTYPE_BOOL;
	    comp_argv[i].value.boolArg = JSVAL_TO_BOOLEAN(argv[i]);
	}
	else if (JSVAL_IS_STRING(argv[i])) {
	    comp_argv[i].type = ARGTYPE_STRING;
	    comp_argv[i].value.stringArg = JS_GetStringBytes(JSVAL_TO_STRING(argv[i]));
	}
	else {
	    comp_argv[i].type = ARGTYPE_NULL;
	    comp_argv[i].value.intArg = 0;
	}
    }

    switch(JSVAL_TO_INT(type)) {
    case ARGTYPE_NULL:
	ET_moz_CompMethodFunction((ETCompMethodFunc)JSVAL_TO_INT(native), argc, NULL);
	*rval = 0;
	break;
    case ARGTYPE_INT32:
	*rval = INT_TO_JSVAL((int32)ET_moz_CompMethodFunction((ETCompMethodFunc)JSVAL_TO_INT(native), argc, comp_argv));
	break;
    case ARGTYPE_BOOL:
	*rval = BOOLEAN_TO_JSVAL((JSBool)ET_moz_CompMethodFunction((ETCompMethodFunc)JSVAL_TO_INT(native), argc, comp_argv));
	break;
    case ARGTYPE_STRING:
	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, 
		(char*)ET_moz_CompMethodFunction((ETCompMethodFunc)JSVAL_TO_INT(native), argc, comp_argv)));
	break;
    }
    return JS_TRUE;
}

void
lm_RegisterComponentMethod(const char *comp, const char *targetName, 
			   uint8 retType, ETCompMethodFunc method, int32 argc)
{
    JSContext *cx;
    JSObject *arrayObj, *obj, *func_obj;
    JSFunction *func;
    jsval val;
    
    if (!comp || !targetName || !(cx = lm_crippled_decoder->js_context))
	return;

    arrayObj = lm_DefineComponents(lm_crippled_decoder);
    if (!arrayObj)
	return;

    if (!JS_GetProperty(cx, arrayObj, comp, &val) ||
        !JSVAL_IS_OBJECT(val))
	return;

    obj = JSVAL_TO_OBJECT(val);

    if (!(func = JS_DefineFunction(cx, obj, targetName, component_mozilla_method_stub, argc, 0))){
    }

    func_obj = JS_GetFunctionObject(func);

    if (!JS_DefineProperty(cx, func_obj, lm_typePrefix_str,
		       INT_TO_JSVAL((int32)retType), 0, 0, JSPROP_READONLY))
	return;
    if (!JS_DefineProperty(cx, func_obj, lm_methodPrefix_str,
		       INT_TO_JSVAL(method), 0, 0, JSPROP_READONLY))
	return;
    if (!JS_DefineProperty(cx, func_obj, lm_methodArgc_str,
		       INT_TO_JSVAL(argc), 0, 0, JSPROP_READONLY))
	return;
}

/* Constructor method for a JSComponent object */
static JSComponent*
component_create_self(JSContext *cx, MochaDecoder* decoder, JSComponent *component, const char *name)
{
    JSObject *obj;

    /* JSComponent may be malloc'd previous to this to make it easier
     * to fill in the struct with data from the Mozilla thread
     */
    if (!component) {
	component = JS_malloc(cx, sizeof(JSComponent));
	if (!component)
	    return NULL;
    }

    obj = JS_NewObject(cx, &lm_component_class, NULL, NULL);
    if (!obj || !JS_SetPrivate(cx, obj, component)) {
	JS_free(cx, component);
	return NULL;
    }

    if (!JS_DefineProperties(cx, obj, component_props))
	return NULL;

    if (!JS_DefineFunctions(cx, obj, component_methods))
	return NULL;

    /* Fill out static property fields */
    component->decoder = HOLD_BACK_COUNT(decoder);
    component->obj = obj;

    component->name = JS_NewStringCopyZ(cx, name);
    if (!component->name || !JS_LockGCThing(cx, component->name))
	return NULL;

    return component;
}


/*
 * -----------------------------------------------------------------------
 *
 * Reflection of the list of installed components.
 * The only static property is the array length;
 * the array elements (JSComponents) are added
 * lazily when referenced.
 *
 * -----------------------------------------------------------------------
 */

enum componentarray_slot
{
    COMPONENTLIST_ARRAY_LENGTH = -1
};

static JSPropertySpec componentarray_props[] =
{
    {"length",  COMPONENTLIST_ARRAY_LENGTH,    JSPROP_READONLY},
    {0}
};

/* Look up the component for the specified slot of the plug-in list */
static JSComponent*
componentarray_create_component(JSContext *cx, JSComponentArray *array,
				JSComponent *component, const char *targetName, jsint targetSlot)
{
    component = component_create_self(cx, array->decoder, component, targetName);
    if (component) {
	char *name;
	jsval val;

	name = JS_GetStringBytes(component->name);
	val = OBJECT_TO_JSVAL(component->obj);
	JS_DefineProperty(cx, array->obj, name, val, NULL, NULL,
			  JSPROP_ENUMERATE);
	JS_AliasElement(cx, array->obj, name, targetSlot);
	array->length++;
	return component;
    }
    return NULL;
}

extern JSClass lm_component_array_class;

void
lm_RegisterComponent(const char *targetName, ETBoolPtrFunc active_callback, 
		     ETVoidPtrFunc startup_callback) 
{
    JSContext *cx;
    JSObject *arrayObj;
    JSComponentArray *array;
    JSComponent *component;
    jsval val;

    if (!(cx = lm_crippled_decoder->js_context) || !targetName)
	return;
	
    arrayObj = lm_DefineComponents(lm_crippled_decoder);
    if (!arrayObj)
	return;

    array = JS_GetInstancePrivate(cx, arrayObj, &lm_component_array_class, NULL);
    if (!array)
	return;

    if (JS_GetProperty(cx, arrayObj, targetName, &val) && JSVAL_IS_OBJECT(val)) {
	/* We already have a component by this name.  Update the active and
	 * startup callback funcs in case ours are out of date 
	 */
	component = JS_GetPrivate(cx, JSVAL_TO_OBJECT(val));
	if (!component)
	    return;

	component->active_callback = active_callback;
	component->startup_callback = startup_callback;

	return;
    }

    component = JS_malloc(cx, sizeof(JSComponent));
    if (!component)
	return;

    component->active_callback = active_callback;
    component->startup_callback = startup_callback;

    componentarray_create_component(cx, array, component, targetName, array->length);
}


PR_STATIC_CALLBACK(JSBool)
componentarray_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSComponentArray *array;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    array = JS_GetInstancePrivate(cx, obj, &lm_component_array_class, NULL);
    if (!array)
	return JS_TRUE;

    switch (slot) {
      case COMPONENTLIST_ARRAY_LENGTH:
        *vp = INT_TO_JSVAL(array->length);
        break;

      default:
        /* Don't mess with a user-defined property. */
        if (slot >= 0 && slot < (jsint) array->length) {

	    /* Search for an existing JSComponent for this slot */
	    JSObject* componentObj = NULL;
	    jsval val = *vp;

	    if (JSVAL_IS_OBJECT(val)) {
    		componentObj = JSVAL_TO_OBJECT(val);
	    }
	    else {
		JSComponent* component;
		component = componentarray_create_component(cx, array, NULL,
		                                            NULL, slot);
		if (component)
    		    componentObj = component->obj;
	    }

	    *vp = OBJECT_TO_JSVAL(componentObj);
	    break;
	}
    }

    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
componentarray_finalize(JSContext *cx, JSObject *obj)
{
    JSComponentArray* array;

    array = JS_GetPrivate(cx, obj);
    if (!array)
	return;
    DROP_BACK_COUNT(array->decoder);
    JS_free(cx, array);
}

JSClass lm_component_array_class =
{
    "ComponentArray", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    componentarray_getProperty, componentarray_getProperty, JS_EnumerateStub,
    JS_ResolveStub, JS_ConvertStub, componentarray_finalize
};


JSObject*
lm_DefineComponents(MochaDecoder *decoder)
{
    JSObject *obj;
    JSComponentArray *array;
    JSContext *cx = decoder->js_context;
    JSPreDefComponent def_comps;   
    JSComponent *component;
    jsint slot;

    obj = decoder->components;
    if (obj)
        return obj;

    array = JS_malloc(cx, sizeof(JSComponentArray));
    if (!array)
        return NULL;
    XP_BZERO(array, sizeof *array);

    obj = JS_NewObject(cx, &lm_component_array_class, NULL,
		       decoder->window_object);
    if (!obj || !JS_SetPrivate(cx, obj, array)) {
        JS_free(cx, array);
        return NULL;
    }
    if (!JS_DefineProperties(cx, obj, componentarray_props))
	return NULL; 

    array->decoder = HOLD_BACK_COUNT(decoder);
    array->obj = obj;

    /* Components can be added dynamically but some are predefined */
    slot = 0;
    array->length = 0;
    def_comps = predef_components[slot];
    while (def_comps.name) {
	component = JS_malloc(cx, sizeof(JSComponent));
	if (!component)
	    return NULL;

	if (ET_moz_VerifyComponentFunction(def_comps.func, &(component->active_callback), 
					   &(component->startup_callback))) {
	    componentarray_create_component(cx, array, component, def_comps.name, array->length);
	}
	else {
	    /*Component call failed somewhere.*/
	    JS_free(cx, component);
	}
	def_comps = predef_components[++slot];
    }
    return obj;
}
