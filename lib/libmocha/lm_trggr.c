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
 * lm_trggr.c
 * 
 * Reflects AutoInstall trigger methods
 * into the global JavaScript config object.
 *
 * See Trigger.java for related functions.
 */

#include "lm.h"
#include "prefapi.h"
#include "VerReg.h"
#ifdef MOZ_SMARTUPDATE
#include "softupdt.h"
#endif
#include "gui.h"	/* XP_AppPlatform */

JSBool PR_CALLBACK asd_Version(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
JSBool PR_CALLBACK asd_get_version(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
JSBool PR_CALLBACK asd_start_update(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
JSBool PR_CALLBACK asd_conditional_update(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
JSBool PR_CALLBACK asd_alert(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
JSBool PR_CALLBACK asd_platform(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);

PRIVATE JSFunctionSpec autoinstall_methods[] = {
    { "getVersion",	asd_get_version,	2 },
    { "startUpdate",	asd_start_update,	2 },
    { "conditionalUpdate",	asd_conditional_update, 4 },
    { "startUpdateComponent",	asd_conditional_update,	4 },	/* old name */
    { NULL,	NULL,	0 }
};

PRIVATE JSFunctionSpec globalconfig_methods[] = {
    { "alert",	asd_alert,	1 },
    { "getPlatform", asd_platform, 0 },
    { NULL,	NULL,	0 }
};

PRIVATE JSClass autoinstall_class = {
    "AutoInstall", 0,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

PRIVATE JSClass version_class = {
    "VersionInfo", 0,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

JSBool lm_DefineTriggers()
{
	JSContext	*globalContext = NULL;
	JSObject	*globalObject = NULL;
	JSObject	*autoInstallObject, *versionObject;

	/* get global mocha context and object */
    PREF_GetConfigContext(&globalContext);
    PREF_GetGlobalConfigObject(&globalObject);

    if (globalContext == NULL || globalObject == NULL)	{
        return JS_FALSE;
    }

	/* define AutoInstall object in global object */
	autoInstallObject = JS_DefineObject(globalContext, globalObject, 
					    "AutoInstall",
					    &autoinstall_class, 
					    NULL, 
					    JSPROP_ENUMERATE|JSPROP_READONLY);
					    
	if (!autoInstallObject)
		return JS_FALSE;
	
	/* define Version class in AutoInstall */
    versionObject = JS_InitClass(globalContext, autoInstallObject, NULL,
		        		&version_class, asd_Version, 0, NULL, NULL, NULL, NULL);
	
	if (!versionObject)
		return JS_FALSE;
    
    /* define some global config utility functions */
    JS_DefineFunctions(globalContext, globalObject, globalconfig_methods);
    
	return JS_DefineFunctions(globalContext, autoInstallObject, autoinstall_methods);
}

/* Convert VERSION type to Version JS object */
static void asd_versToObj(JSContext *cx, VERSION* vers, JSObject* versObj)
{
	jsval val = INT_TO_JSVAL(vers->major);
	JS_SetProperty(cx, versObj, "major", &val);
	val = INT_TO_JSVAL(vers->minor);
	JS_SetProperty(cx, versObj, "minor", &val);
	val = INT_TO_JSVAL(vers->release);
	JS_SetProperty(cx, versObj, "release", &val);
	val = INT_TO_JSVAL(vers->build);
	JS_SetProperty(cx, versObj, "build", &val);
}

/* Convert Version JS object to VERSION type */
static void asd_objToVers(JSContext *cx, JSObject* versObj, VERSION* vers)
{
	jsval val;
	JS_GetProperty(cx, versObj, "major", &val);
	vers->major = JSVAL_TO_INT(val);
	JS_GetProperty(cx, versObj, "minor", &val);
	vers->minor = JSVAL_TO_INT(val);
	JS_GetProperty(cx, versObj, "release", &val);
	vers->release = JSVAL_TO_INT(val);
	JS_GetProperty(cx, versObj, "build", &val);
	vers->build = JSVAL_TO_INT(val);
}

/*
 * ?? -- move this to VR?
 * Returns 0 if versions are equal; < 0 if vers2 is newer; > 0 if vers1 is newer
 */
static int asd_compareVersion(VERSION* vers1, VERSION* vers2)
{
	int diff;
	if (vers1 == NULL)
		diff = -4;
	else if (vers2 == NULL)
		diff = 4;
	else if (vers1->major == vers2->major) {
		if (vers1->minor == vers2->minor) {
			if (vers1->release == vers2->release) {
				if (vers1->build == vers2->build) 
					diff = 0;
				else diff = (vers1->build > vers2->build) ? 1 : -1;
			}
			else diff = (vers1->release > vers2->release) ? 2 : -2;
		}
		else diff = (vers1->minor > vers2->minor) ? 3 : -3;
	}
	else diff = (vers1->major > vers2->major) ? 4 : -4;
	return diff;
}

/* Version constructor
   (accepts up to four int params to initialize fields) */
JSBool PR_CALLBACK asd_Version
	(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
	VERSION vers;
	vers.major = (argc >= 1 && JSVAL_IS_INT(argv[0])) ? JSVAL_TO_INT(argv[0]) : 0;
	vers.minor = (argc >= 2 && JSVAL_IS_INT(argv[1])) ? JSVAL_TO_INT(argv[1]) : 0;
	vers.release = (argc >= 3 && JSVAL_IS_INT(argv[2])) ? JSVAL_TO_INT(argv[2]) : 0;
	vers.build = (argc >= 4 && JSVAL_IS_INT(argv[3])) ? JSVAL_TO_INT(argv[3]) : 0;

	asd_versToObj(cx, &vers, obj);
    return JS_TRUE;
}

/* arguments:
   0. component name
   1. version no. to fill in
   return:
      error status */
JSBool PR_CALLBACK asd_get_version
	(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
	REGERR status = 0;
	if (argc >= 2 && JSVAL_IS_STRING(argv[0])
		&& JSVAL_IS_OBJECT(argv[1]))
	{		
		VERSION vers;
		char* component_path = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
		status = VR_GetVersion(component_path, &vers);

		if (status == REGERR_OK) {
			JSObject* versObj = JSVAL_TO_OBJECT(argv[1]);
			asd_versToObj(cx, &vers, versObj);
		}
	}
	*rval = INT_TO_JSVAL(status);
	return JS_TRUE;
}

/* arguments:
   0. url
   1. flags
   return:
      true if no errors */
JSBool PR_CALLBACK asd_start_update
	(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

#ifdef MOZ_SMARTUPDATE        
	if (argc >= 2 && JSVAL_IS_STRING(argv[0]) &&
		JSVAL_IS_INT(argv[1])) {
		char* url = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
		MWContext* cx = FE_GetInitContext();
		XP_Bool result = SU_StartSoftwareUpdate( cx, url,
			NULL, NULL, NULL, JSVAL_TO_INT(argv[1]) );
		*rval = BOOLEAN_TO_JSVAL(result);
		return JS_TRUE;
	}
#endif
	return JS_TRUE;
}

/* arguments:
   0. url
   1. component name
   2. version no. to compare
   3. flags
   return:
      true if update triggered and no errors */
JSBool PR_CALLBACK asd_conditional_update
	(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
	REGERR status = 0;

    *rval = BOOLEAN_TO_JSVAL(JS_FALSE);	/*INT_TO_JSVAL(status);*/

#ifdef MOZ_SMARTUPDATE
    if (argc >= 4 && JSVAL_IS_STRING(argv[0]) &&
		JSVAL_IS_STRING(argv[1]) &&
		JSVAL_IS_OBJECT(argv[2]) &&
		JSVAL_IS_INT(argv[3]))
	{
		VERSION curr_vers;
		char* component_path = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
		status = VR_GetVersion(component_path, &curr_vers);

		if (status == REGERR_OK) {
			JSObject* versObj = JSVAL_TO_OBJECT(argv[2]);
			VERSION req_vers;
			asd_objToVers(cx, versObj, &req_vers);
			if ( asd_compareVersion(&req_vers, &curr_vers) > 0 ) {
				char* url = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
				MWContext* cx = FE_GetInitContext();
				XP_Bool result = SU_StartSoftwareUpdate( cx, url,
					NULL, NULL, NULL, JSVAL_TO_INT(argv[3]) );
				*rval = BOOLEAN_TO_JSVAL(result);
				return JS_TRUE;
			}
		}
	}
#endif
	return JS_TRUE;
}

JSBool PR_CALLBACK asd_alert
	(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
	if (argc >= 1 && JSVAL_IS_STRING(argv[0])) {
		char* msg = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
		MWContext* ctx = FE_GetInitContext();
		
		if (ctx)
			FE_Alert(ctx, msg);
	}
	return JS_TRUE;
}

JSBool PR_CALLBACK asd_platform
	(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
	*rval = STRING_TO_JSVAL( JS_NewStringCopyZ(cx, XP_AppPlatform) );
	return JS_TRUE;
}

