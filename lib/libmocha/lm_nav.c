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
 * JS reflection of the Navigator top-level object.
 *
 * Brendan Eich, 11/16/95
 */
#include "lm.h"
#include "prmem.h"
#ifdef JAVA
#include "java.h"
#endif
#include "gui.h"
#include "prefapi.h"
#include "msgcom.h"

#include "secnav.h"

typedef struct JSNavigator {
    JSString	    *userAgent;
    JSString	    *appCodeName;
    JSString	    *appVersion;
    JSString	    *appName;
    JSString        *appLanguage;
    JSString        *appPlatform;
    JSString        *securityPolicy;
} JSNavigator;

enum nav_slot {
    NAV_USER_AGENT      = -1,
    NAV_APP_CODE_NAME   = -2,
    NAV_APP_VERSION     = -3,
    NAV_APP_NAME        = -4,
    NAV_APP_LANGUAGE    = -5,
    NAV_APP_PLATFORM    = -6,
    NAV_SECURITY_POLICY = -7
};

static JSPropertySpec nav_props[] = {
    {"userAgent",     NAV_USER_AGENT,       JSPROP_ENUMERATE | JSPROP_READONLY},
    {"appCodeName",   NAV_APP_CODE_NAME,    JSPROP_ENUMERATE | JSPROP_READONLY},
    {"appVersion",    NAV_APP_VERSION,      JSPROP_ENUMERATE | JSPROP_READONLY},
    {"appName",       NAV_APP_NAME,         JSPROP_ENUMERATE | JSPROP_READONLY},
    {"language",      NAV_APP_LANGUAGE,     JSPROP_ENUMERATE | JSPROP_READONLY},
    {"platform",      NAV_APP_PLATFORM,     JSPROP_ENUMERATE | JSPROP_READONLY},
    {"securityPolicy", NAV_SECURITY_POLICY, JSPROP_ENUMERATE | JSPROP_READONLY},
    {0}
};

extern JSClass lm_navigator_class;

PR_STATIC_CALLBACK(JSBool)
nav_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSNavigator *nav;
    JSString *str;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    nav = JS_GetInstancePrivate(cx, obj, &lm_navigator_class, NULL);
    if (!nav)
	return JS_TRUE;

    switch (slot) {
      case NAV_USER_AGENT:
        str = nav->userAgent;
        break;
      case NAV_APP_CODE_NAME:
        str = nav->appCodeName;
        break;
      case NAV_APP_VERSION:
        str = nav->appVersion;
        break;
      case NAV_APP_NAME:
        str = nav->appName;
        break;
      case NAV_APP_LANGUAGE:
        str = nav->appLanguage;
        break;
      case NAV_APP_PLATFORM:
        str = nav->appPlatform;
        break;
      case NAV_SECURITY_POLICY:
        str = nav->securityPolicy;
        break;


      default:
        /* Don't mess with user-defined or method properties. */
        return JS_TRUE;
    }

    if (str)
	*vp = STRING_TO_JSVAL(str);
    else
        *vp = JS_GetEmptyStringValue(cx);
    return JS_TRUE;
}


PR_STATIC_CALLBACK(void)
nav_finalize(JSContext *cx, JSObject *obj)
{
    JSNavigator *nav;

    nav = JS_GetPrivate(cx, obj);
    if (!nav)
	return;

    JS_UnlockGCThing(cx, nav->userAgent);
    JS_UnlockGCThing(cx, nav->appCodeName);
    JS_UnlockGCThing(cx, nav->appVersion);
    JS_UnlockGCThing(cx, nav->appName);
    JS_UnlockGCThing(cx, nav->appLanguage);
    JS_UnlockGCThing(cx, nav->appPlatform);
    JS_UnlockGCThing(cx, nav->securityPolicy);
    XP_DELETE(nav);
}

JSClass lm_navigator_class = {
    "Navigator", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, nav_getProperty, nav_getProperty,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nav_finalize
};

PR_STATIC_CALLBACK(JSBool)
Navigator(JSContext *cx, JSObject *obj,
          uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
nav_java_enabled(JSContext *cx, JSObject *obj,
                 uint argc, jsval *argv, jsval *rval)
{
#ifdef JAVA
    *rval = BOOLEAN_TO_JSVAL(LJ_GetJavaEnabled());
#else
    *rval = JSVAL_FALSE;
#endif
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
nav_taint_enabled(JSContext *cx, JSObject *obj,
                 uint argc, jsval *argv, jsval *rval)
{
    *rval = JSVAL_FALSE;
    return JS_TRUE;
}


/*
 * 1 arg == get pref value
 * 2 args == set pref value
 */
PR_STATIC_CALLBACK(JSBool)
nav_prefs(JSContext *cx, JSObject *obj,
          uint argc, jsval *argv, jsval *rval)
{
    JSBool       ok = JS_TRUE;

    if (argc != 1 && argc != 2)
	return JS_TRUE;

    ok = lm_CanAccessTarget(cx, argc == 1
                                ? JSTARGET_UNIVERSAL_PREFERENCES_READ
                                : JSTARGET_UNIVERSAL_PREFERENCES_WRITE);

    if (!ok)
	return JS_FALSE;
    
    return ET_HandlePref(cx, argc, argv, rval);

}

PR_STATIC_CALLBACK(JSBool)
nav_save_prefs(JSContext *cx, JSObject *obj,
          uint argc, jsval *argv, jsval *rval)
{
    JSBool       ok = JS_TRUE;

    ok = lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_PREFERENCES_WRITE);

    if (!ok)
	return JS_FALSE;
    
    ET_moz_CallFunctionAsync((ETVoidPtrFunc)PREF_SavePrefFile, NULL);
    return JS_TRUE;
}

JSBool PR_CALLBACK
nav_is_default(JSContext *cx, JSObject *obj,
          uint argc, jsval *argv, jsval *rval)
{
#ifdef XP_WIN
    *rval = BOOLEAN_TO_JSVAL(
	ET_moz_CallFunctionBool((ETBoolPtrFunc)FE_IsNetscapeDefault, NULL));
#endif
    return JS_TRUE;
}

JSBool PR_CALLBACK
nav_make_default(JSContext *cx, JSObject *obj,
          uint argc, jsval *argv, jsval *rval)
{
#ifdef XP_WIN
    *rval = BOOLEAN_TO_JSVAL(
	ET_moz_CallFunctionBool((ETBoolPtrFunc)FE_MakeNetscapeDefault, NULL));
#endif    
    return JS_TRUE;
}

static JSFunctionSpec nav_methods[] = {
    {"javaEnabled",	    nav_java_enabled,       0},
    {"taintEnabled",	    nav_taint_enabled,      0},
    {"preference",   	    nav_prefs,      	    1},
    {"savePreferences",	    nav_save_prefs,	    0},
    {"isNetscapeDefault",   nav_is_default,	    0},
    {"makeNetscapeDefault", nav_make_default,  	    0},
    {0}
};

/*
 * ------------------------------------------------------------------
 */
static JSClass mail_class =
{
    "Mail", 0 /* no private data, yet */,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

/*
 * Update an imap mail folder
 */
PR_STATIC_CALLBACK(JSBool)
mail_update(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
#ifdef MOZ_MAIL_NEWS
    jsval folder;
    char *folder_url;

    /* make sure we got a string as our input value */
    if (!JSVAL_IS_STRING(argv[0]) &&
        !JS_ConvertValue(cx, argv[0], JSTYPE_STRING, &argv[0])) {
	return JS_FALSE;
    }

    folder_url = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
    XP_TRACE(("Folder URL = '%s'", folder_url));
    /* use et_moz.c stuff to send a message over to the mozilla thread */
    ET_moz_CallFunctionAsync((ETVoidPtrFunc) MSG_IMAPFolderChangedNotification, folder_url);
#endif /* MOZ_MAIL_NEWS */
    return JS_TRUE;

}

static JSFunctionSpec mail_methods[] = {
    {"updateFolder",   	mail_update,      	1},
    {0}
};

static JSObject *
lm_NewMailObject(JSContext * cx)
{
    JSObject *obj;

    obj = JS_NewObject(cx, &mail_class, NULL, NULL);
    if (!obj || !JS_DefineFunctions(cx, obj, mail_methods))
	return NULL;

    return obj;
}

/*
 * ------------------------------------------------------------------
 */

JSObject *
lm_DefineNavigator(MochaDecoder *decoder)
{
    JSObject *obj, *list_obj, *mail_obj;
    JSContext *cx;
    JSNavigator *nav;
    char *userAgent;

    obj = decoder->navigator;
    if (obj)
        return obj;

    cx = decoder->js_context;
    nav = JS_malloc(cx, sizeof *nav);
    if (!nav)
        return NULL;
    XP_BZERO(nav, sizeof *nav);

    obj = JS_InitClass(cx, decoder->window_object, NULL, &lm_navigator_class,
                       Navigator, 0, nav_props, nav_methods, NULL, NULL);
    if (!obj || !JS_SetPrivate(cx, obj, nav)) {
        JS_free(cx, nav);
        return NULL;
    }
    
    if (!JS_DefineProperty(cx, decoder->window_object, "navigator",
			   OBJECT_TO_JSVAL(obj), NULL, NULL,
                           JSPROP_ENUMERATE | JSPROP_READONLY)) {
        return NULL;
    }

    /* XXX bail on null returns from PR_smprintf and JS_NewStringCopyZ */
    userAgent = PR_smprintf("%s/%s", XP_AppCodeName, XP_AppVersion);
    nav->userAgent = JS_NewStringCopyZ(cx, userAgent);
    PR_FREEIF(userAgent);
    JS_LockGCThing(cx, nav->userAgent);

    nav->appCodeName = JS_NewStringCopyZ(cx, XP_AppCodeName);
    JS_LockGCThing(cx, nav->appCodeName);

    nav->appVersion = JS_NewStringCopyZ(cx, XP_AppVersion);
    JS_LockGCThing(cx, nav->appVersion);

    nav->appName = JS_NewStringCopyZ(cx, XP_AppName);
    JS_LockGCThing(cx, nav->appName);

    nav->appLanguage = JS_NewStringCopyZ(cx, XP_AppLanguage);
    JS_LockGCThing(cx, nav->appLanguage);

    nav->appPlatform = JS_NewStringCopyZ(cx, XP_AppPlatform);
    JS_LockGCThing(cx, nav->appPlatform);

    nav->securityPolicy = JS_NewStringCopyZ(cx, SECNAV_GetPolicyNameString());
    JS_LockGCThing(cx, nav->securityPolicy);


    /* Ask lm_plgin.c to create objects for plug-in and MIME-type arrays */

    list_obj = lm_NewPluginList(cx, obj);
    if (!list_obj || 
	!JS_DefineProperty(cx, obj, "plugins",
			   OBJECT_TO_JSVAL(list_obj), 
			   NULL, NULL,
                           JSPROP_ENUMERATE | JSPROP_READONLY)) {
	return NULL;
    }

    list_obj = lm_NewMIMETypeList(cx);
    if (!list_obj ||
	!JS_DefineProperty(cx, obj, "mimeTypes",
			   OBJECT_TO_JSVAL(list_obj), 
			   NULL, NULL,
                           JSPROP_ENUMERATE | JSPROP_READONLY)) {
	return NULL;
    }

    /* mail stuff -- don't make it JSPROP_ENUMERATE for now XXX */
    mail_obj = lm_NewMailObject(cx);
    if (!mail_obj ||
	!JS_DefineProperty(cx, obj, "mail",
			   OBJECT_TO_JSVAL(mail_obj), 
			   NULL, NULL,
                           JSPROP_READONLY)) {
	return NULL;
    }



    decoder->navigator = obj;
    return obj;
}
