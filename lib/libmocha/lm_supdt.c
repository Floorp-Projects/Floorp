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
 * Sofware Update object
 * This object only exists to pass the data from SoftwareUpdate trigger
 * to softupdate Java class. It has a single property, src, that is read-only
 * It is created as a part of the scope for softupdate
 *
 * Aleks Totic 3/5/97
 *
 */

#include "lm.h"

/* Private object data */
typedef struct JSSoftup	{
	JSString * src;		/* Name of the JAR file */
    JSBool silent;
    JSBool force;
} JSSoftup;

/* Properties */
enum su_slot {
    SU_SRC              = -1,
    SU_SILENT           = -2,
    SU_FORCE            = -3
};

static JSPropertySpec su_props[] = {
    { "src",	SU_SRC,	JSPROP_READONLY },
    { "silent",	SU_SILENT,	JSPROP_READONLY },
    { "force",	SU_FORCE,	JSPROP_READONLY },
	{0}
};

/* Prototypes */
JSBool PR_CALLBACK
su_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
void PR_CALLBACK
su_finalize(JSContext *cx, JSObject *obj);
JSBool PR_CALLBACK
SoftUpdate(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool
su_InitSoftUpClass( MochaDecoder * decoder, JSContext *cx );
char *
DecodeSoftUpJSArgs(const char * jsArgs, JSBool* force, JSBool* silent );

extern JSClass lm_softup_class;

/*
 * su_getProperty
 * 
 */
JSBool PR_CALLBACK
su_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	JSSoftup *softup;
	jsint slot;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	slot = JSVAL_TO_INT(id);

	softup = JS_GetInstancePrivate(cx, obj, &lm_softup_class, NULL);
	XP_ASSERT( softup );
	if (!softup )
		return JS_TRUE;

    switch (slot)
    {
    case SU_SRC:
	    *vp = STRING_TO_JSVAL(softup->src);
        return JS_TRUE;
    case SU_SILENT:
        *vp = BOOLEAN_TO_JSVAL(softup->silent);
        return JS_TRUE;
    case SU_FORCE:
        *vp = BOOLEAN_TO_JSVAL(softup->force);
        return JS_TRUE;
    default:
        return JS_TRUE;
    }
	return JS_TRUE;
}

/*
 * clean up
 */
void PR_CALLBACK
su_finalize(JSContext *cx, JSObject *obj)
{
	JSSoftup *softup;
    softup = JS_GetPrivate(cx, obj);
	if (softup)
	{
    	JS_UnlockGCThing( cx, softup->src );
		XP_FREE(softup);
	}
}

/* Global, referenced in su_trigger.c to check if the object is the
   correct class */

JSClass lm_softup_class = {
    "SoftUpdate", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, su_getProperty, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, su_finalize
};

/*
 * Constructor
 * No one should be able to construct one of these, except as a scope object
 * for softupdate.
 */
JSBool PR_CALLBACK
SoftUpdate(JSContext *cx, JSObject *obj,
       uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

/* 
 * Initialize the class
 */
static JSObject *prototype = NULL;	/* Using this as a class prototype */
									/* and a signal if class has been loaded */

static JSBool
su_InitSoftUpClass(MochaDecoder * decoder, JSContext *cx)
{
    JSObject *prototype;
    prototype = JS_InitClass(	cx, 				/* JSContext *cx */
		    			decoder->window_object, 	/* JSObject *obj */
						NULL, /* decoder->event_receiver_prototype,  JSObject *parent_proto */
						&lm_softup_class,			/* JSClass *clasp */
						SoftUpdate, 	 			/* JSNative constructor */
						0, 							/* PRUintn nargs */
						su_props, 					/* JSPropertySpec *ps */
						0, 							/* JSFunctionSpec *fs */
						0, 							/* JSPropertySpec *static_ps */
						0);							/* JSFunctionSpec *static_fs */
 
    if (!prototype)
		return JS_FALSE;
		
	/* JS_InitStandardClasses makes the "standard" JS functions available 
	if ( !JS_InitStandardClasses(cx, prototype))
		return JS_FALSE;
*/    return JS_TRUE;
}

/* Decodes the JSArgs */
/* see softupdt.c, EncodeSoftUpJSArgs for info on encoding */
/* jsArgs do not have 'autoinstall:' prefix */
char *
DecodeSoftUpJSArgs(const char *jsArgs, JSBool *force, JSBool *silent )
{
    char * fileName;
    int32 length;
    int32 fileNameLength;

    *force = JS_FALSE;
    *silent = JS_FALSE;
    if (jsArgs == NULL)
        return NULL;
    length = XP_STRLEN(jsArgs);
    fileNameLength = length - 4;
    fileName = XP_ALLOC(fileNameLength +1);
    XP_MEMCPY(fileName, jsArgs, fileNameLength);
    fileName[fileNameLength] = 0;
    *force = (JSBool)(jsArgs[length - 1] == 'T');
    *silent = (JSBool)(jsArgs[length - 3] == 'T');
    return fileName;
}

/*
 * Creates a new object
 * Arguments: jarargs jar: followed by name of the JAR file
 */

JSObject *
lm_NewSoftupObject( JSContext *cx, MochaDecoder *decoder, const char *jarargs )
{
	JSObject * obj;
	if (prototype == NULL)
	{
		if ( !su_InitSoftUpClass(decoder, cx ))
			return NULL;
	}

    obj = JS_NewObject(cx, &lm_softup_class, prototype, decoder->window_object);
    if ( obj )
    {
    	JSSoftup * su;
    	su = XP_ALLOC (sizeof (JSSoftup));
    	if ( su )
		{
			char * fileName;
            JSBool silent;
            JSBool force;
            fileName = DecodeSoftUpJSArgs( jarargs, &force, &silent);
            su->src = JS_NewStringCopyZ(cx, fileName);
            if ( fileName )
                XP_FREE(fileName);
            su->force = force;
            su->silent = silent;
			JS_LockGCThing( cx, su->src );
			JS_SetPrivate( cx, obj, su );
		}
	}

    return obj;
}

