/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "lm.h"
#include "libi18n.h"

typedef struct JSEnvironment {
    MochaDecoder    *decoder;
} JSEnvironment;

enum environment_tinyid {
    ENVIRONMENT_LANGUAGE			= -1,
    ENVIRONMENT_LANGUAGE_COLLATE	= -2,
    ENVIRONMENT_LANGUAGE_MONETARY	= -3,
    ENVIRONMENT_LANGUAGE_NUMERIC	= -4,
    ENVIRONMENT_LANGUAGE_TIME		= -5,
    ENVIRONMENT_COUNTRY				= -6,
    ENVIRONMENT_COUNTRY_COLLATE		= -7,
    ENVIRONMENT_COUNTRY_MONETARY	= -8,
    ENVIRONMENT_COUNTRY_NUMERIC		= -9,
    ENVIRONMENT_COUNTRYE_TIME		= -10,
    ENVIRONMENT_LOCALES				= -11
};

static JSPropertySpec environment_props[] = {
    {"language",			ENVIRONMENT_LANGUAGE,			JSPROP_ENUMERATE | JSPROP_READONLY},
    {"languageCollate",		ENVIRONMENT_LANGUAGE_COLLATE,	JSPROP_ENUMERATE | JSPROP_READONLY},
    {"languageMonetary",	ENVIRONMENT_LANGUAGE_MONETARY,	JSPROP_ENUMERATE | JSPROP_READONLY},
    {"languageNumeric",		ENVIRONMENT_LANGUAGE_NUMERIC,	JSPROP_ENUMERATE | JSPROP_READONLY},
    {"languageTime",		ENVIRONMENT_LANGUAGE_TIME,		JSPROP_ENUMERATE | JSPROP_READONLY},
    {"country",				ENVIRONMENT_COUNTRY,			JSPROP_ENUMERATE | JSPROP_READONLY},
    {"countryCollate",		ENVIRONMENT_COUNTRY_COLLATE,	JSPROP_ENUMERATE | JSPROP_READONLY},
    {"countryMonetary",		ENVIRONMENT_COUNTRY_MONETARY,	JSPROP_ENUMERATE | JSPROP_READONLY},
    {"countryNumeric",		ENVIRONMENT_COUNTRY_NUMERIC,	JSPROP_ENUMERATE | JSPROP_READONLY},
    {"countryTime",			ENVIRONMENT_COUNTRYE_TIME,		JSPROP_ENUMERATE | JSPROP_READONLY},
    {"locales",				ENVIRONMENT_LOCALES,			JSPROP_ENUMERATE | JSPROP_READONLY},
    {0}
};

extern JSClass lm_environment_class;


PR_STATIC_CALLBACK(JSBool)
environment_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSEnvironment *environment;
    jsint slot;
    char * arch = NULL;

    if (!JSVAL_IS_INT(id))
		return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    environment = JS_GetInstancePrivate(cx, obj, &lm_environment_class, NULL);
    if (!environment)
		return JS_TRUE;

	/* Temporary using JSTARGET_UNIVERSAL_BROWSER_READ, change this to a real one before ship. */
	if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ))
		return TRUE;

    switch (slot) {
		case ENVIRONMENT_LANGUAGE:
			arch = INTL_GetLanguageCountry(INTL_LanguageSel);
			break;
		case ENVIRONMENT_LANGUAGE_COLLATE:
			arch = INTL_GetLanguageCountry(INTL_LanguageCollateSel);
			break;
		case ENVIRONMENT_LANGUAGE_MONETARY:
			arch = INTL_GetLanguageCountry(INTL_LanguageMonetarySel);
			break;
		case ENVIRONMENT_LANGUAGE_NUMERIC:
			arch = INTL_GetLanguageCountry(INTL_LanguageNumericSel);
			break;
		case ENVIRONMENT_LANGUAGE_TIME:
			arch = INTL_GetLanguageCountry(INTL_LanguageTimeSel);
			break;
		case ENVIRONMENT_COUNTRY:
			arch = INTL_GetLanguageCountry(INTL_CountrySel);
			break;
		case ENVIRONMENT_COUNTRY_COLLATE:
			arch = INTL_GetLanguageCountry(INTL_CountryCollateSel);
			break;
		case ENVIRONMENT_COUNTRY_MONETARY:
			arch = INTL_GetLanguageCountry(INTL_CountryMonetarySel);
			break;
		case ENVIRONMENT_COUNTRY_NUMERIC:
			arch = INTL_GetLanguageCountry(INTL_CountryNumericSel);
			break;
		case ENVIRONMENT_COUNTRYE_TIME:
			arch = INTL_GetLanguageCountry(INTL_CountryTimeSel);
			break;
		case ENVIRONMENT_LOCALES:
			arch = INTL_GetLanguageCountry(INTL_ALL_LocalesSel);
			break;
		default:
			return JS_TRUE;
    }

	if (arch != NULL)
	{
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, arch));
		XP_FREE(arch);
	}

    return JS_TRUE;
}


PR_STATIC_CALLBACK(void)
environment_finalize(JSContext *cx, JSObject *obj)
{
    JSEnvironment *environment;

    environment = JS_GetPrivate(cx, obj);
    if (!environment)
		return;
    DROP_BACK_COUNT(environment->decoder);
    JS_free(cx, environment);
}

JSClass lm_environment_class = {
    "Environment", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, environment_getProperty, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, environment_finalize
};

PR_STATIC_CALLBACK(JSBool)
Environment(JSContext *cx, JSObject *obj,
          uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

JSObject *
lm_DefineEnvironment(MochaDecoder *decoder)
{
    JSObject *obj;
    JSContext *cx;
    JSEnvironment *environment;

    obj = decoder->environment;
    if (obj)
		return obj;

    cx = decoder->js_context;
    if (!(environment = JS_malloc(cx, sizeof(JSEnvironment))))
		return NULL;
    environment->decoder = NULL; /* in case of error below */

    obj = JS_InitClass(cx, decoder->window_object, NULL, &lm_environment_class,
                       Environment, 0, environment_props, NULL, NULL, NULL);

    if (!obj || !JS_SetPrivate(cx, obj, environment)) {
        JS_free(cx, environment);
        return NULL;
    }
    
    if (!JS_DefineProperty(cx, decoder->window_object, "environment",
			   OBJECT_TO_JSVAL(obj), NULL, NULL,
                           JSPROP_ENUMERATE | JSPROP_READONLY)) {
        return NULL;
    }

    environment->decoder = HOLD_BACK_COUNT(decoder);
    decoder->environment = obj;

    return obj;
}

