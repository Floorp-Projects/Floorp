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
 * JS/Mozilla data tainting support.
 * XXX replace with lm_sec.c and lm_CheckAccess or similar
 *
 */
#include "lm.h"
#include "xp.h"
#include "mkparse.h"
#include "mkutils.h"
#include "prclist.h"
#include "plhash.h"
#include "prmem.h"
#include "shist.h"
#include "jsapi.h"
#include "jsdbgapi.h"
#include "jscntxt.h"  /* XXX - needed for charSetName */
#include "zip.h"
#include "zig.h"
#include "jri.h"
#include "jsjava.h"
#include "java.h"
#include "jsobj.h"
#include "jsatom.h"
#include "jsscope.h"

/* Needed to access private method; making method public would be
   security hole */
#define IMPLEMENT_netscape_security_PrivilegeManager

#include "netscape_security_Principal.h"
#include "netscape_security_Zig.h"
#ifdef XP_MAC
    #include "n_security_PrivilegeManager.h"
    #include "n_security_PrivilegeTable.h"
#else
    #include "netscape_security_PrivilegeManager.h"
    #include "netscape_security_PrivilegeTable.h"
#endif /* XP_MAC */
#include "netscape_security_Privilege.h"
#include "netscape_security_Target.h"

extern JRIEnv * LJ_JSJ_CurrentEnv(JSContext * cx);
extern char *LJ_GetAppletScriptOrigin(JRIEnv *env);
extern struct java_lang_String *makeJavaString(const char *, int);

char lm_unknown_origin_str[] = "[unknown origin]";

static char file_url_prefix[]    = "file:";
static char access_error_message[] =
    "access disallowed from scripts at %s to documents at another domain";
static char container_error_message[] =
    "script at '%s' is not signed by sufficient principals to access "
    "signed container";

#define FILE_URL_PREFIX_LEN     (sizeof file_url_prefix - 1)
#define WYSIWYG_TYPE_LEN        10      /* wysiwyg:// */

const char *
LM_StripWysiwygURLPrefix(const char *url_string)
{
    switch (NET_URL_Type(url_string)) {
      case WYSIWYG_TYPE_URL:
        return LM_SkipWysiwygURLPrefix(url_string);
      default:
        return url_string;
    }
}

const char *
LM_SkipWysiwygURLPrefix(const char *url_string)
{
    if (XP_STRLEN(url_string) < WYSIWYG_TYPE_LEN)
        return NULL;
    url_string += WYSIWYG_TYPE_LEN;
    url_string = XP_STRCHR(url_string, '/');
    if (!url_string)
        return NULL;
    return url_string + 1;
}


JSPrincipals *
lm_GetCompilationPrincipals(MochaDecoder *decoder,
                            JSPrincipals *layoutPrincipals)
{
    JSContext *cx;
    JSPrincipals *principals;

    cx = decoder->js_context;

    if (decoder->writing_input && lm_writing_context != NULL) {
        /*
         * Compiling a script added due to a document.write.
         * Get principals from stack frame. We can't just use these
         * principals since the document.write code will fail signature
         * verification. So just grab the codebase and form a new set
         * of principals.
         */
        principals = lm_GetPrincipalsFromStackFrame(lm_writing_context);
        principals = LM_NewJSPrincipals(NULL, NULL, principals
						    ? principals->codebase
						    : NULL);
        if (principals == NULL) {
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
        lm_InvalidateCertPrincipals(decoder, principals);
        return principals;
    }

    if (layoutPrincipals) {
        return layoutPrincipals;
    }

    /*
     * Just get principals corresponding to the window or layer we're
     * currently parsing.
     */
    return lm_GetInnermostPrincipals(cx,
                                     lm_GetActiveContainer(decoder),
                                     NULL);
}

static const char *
find_creator_url(MWContext *context)
{
    History_entry *he;
    const char *address;
    JSContext *cx;
    MochaDecoder *decoder;

    he = SHIST_GetCurrent(&context->hist);
    if (he) {
	address = he->wysiwyg_url;
	if (!address)
	    address = he->address;
	switch (NET_URL_Type(address)) {
	  case ABOUT_TYPE_URL:
	  case MOCHA_TYPE_URL:
	    /* These types cannot name the true origin (server) of JS code. */
	    break;
	  case WYSIWYG_TYPE_URL:
	    return LM_SkipWysiwygURLPrefix(address);
	  case VIEW_SOURCE_TYPE_URL:
	    XP_ASSERT(0);
	  default:
	    return address;
	}
    }

    if (context->grid_parent) {
        address = find_creator_url(context->grid_parent);
        if (address)
            return address;
    }

    cx = context->mocha_context;
    if (cx) {
        decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
        if (decoder && decoder->opener) {
            /* self.opener property is valid, check its MWContext. */
            MochaDecoder *opener = JS_GetPrivate(cx, decoder->opener);
            if (!opener->visited) {
                opener->visited = JS_TRUE;
                address = opener->window_context
			? find_creator_url(opener->window_context)
			: NULL;
                opener->visited = JS_FALSE;
                if (address) 
                    return address;
            }
        }
    }

    return NULL;
}

static const char *
find_origin_url(JSContext *cx, MochaDecoder *decoder)
{
    const char *url_string;
    MWContext *context;
    MochaDecoder *running;

    context = decoder->window_context;
    url_string = context ? find_creator_url(context) : NULL;
    if (url_string == NULL) {
        /* Must be a new, empty window?  Use running origin. */
        running = JS_GetPrivate(cx, JS_GetGlobalObject(cx));

        /* Compare running and decoder to avoid infinite recursion. */
        if (running == decoder) {
            url_string = lm_unknown_origin_str;
        } else {
            url_string = lm_GetSubjectOriginURL(cx);
	    if (!url_string)
		return NULL;
        }
    }
    return url_string;
}

static char *
strip_file_double_slash(const char *url_string)
{
    char *new_url_string;

    if (!XP_STRNCASECMP(url_string, file_url_prefix, FILE_URL_PREFIX_LEN) &&
        url_string[FILE_URL_PREFIX_LEN + 0] == '/' &&
        url_string[FILE_URL_PREFIX_LEN + 1] == '/') {
        new_url_string = PR_smprintf("%s%s",
                                     file_url_prefix,
                                     url_string + FILE_URL_PREFIX_LEN + 2);
    } else {
        new_url_string = XP_STRDUP(url_string);
    }
    return new_url_string;
}

static char *
getCanonicalizedOrigin(JSContext *cx, const char *url_string)
{
    const char *origin;

    if (NET_URL_Type(url_string) == WYSIWYG_TYPE_URL)
        url_string = LM_SkipWysiwygURLPrefix(url_string);
    origin = NET_ParseURL(url_string, GET_PROTOCOL_PART | GET_HOST_PART);
    if (!origin) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }
    return (char *) origin;
}

const char *
lm_GetObjectOriginURL(JSContext *cx, JSObject *obj)
{
    JSPrincipals *principals;

    principals = lm_GetInnermostPrincipals(cx, obj, NULL);
    return principals ? principals->codebase : NULL;
}

static JSBool
sameOrigins(JSContext *cx, const char *origin1, const char *origin2)
{
    char *cmp1, *cmp2;
    JSBool ok;

    if (origin1 == NULL || origin2 == NULL)
        return JS_FALSE;
#if 0  /* XXX Need to enable this and test throroughly */
    /* Shouldn't return true if both origin1 and origin2 are
     * lm_unknown_origin_str. */
    if (XP_STRCMP(origin1, lm_unknown_origin_str) == 0)
        return JS_FALSE;
#endif
    if (XP_STRCMP(origin1, origin2) == 0)
        return JS_TRUE;
    cmp1 = getCanonicalizedOrigin(cx, origin1);
    cmp2 = getCanonicalizedOrigin(cx, origin2);
    if (cmp1 && cmp2 &&
        XP_STRNCASECMP(cmp1, file_url_prefix, FILE_URL_PREFIX_LEN) == 0 &&
	XP_STRNCASECMP(cmp2, file_url_prefix, FILE_URL_PREFIX_LEN) == 0)
	return JS_TRUE;
    ok = (JSBool)(cmp1 && cmp2 && XP_STRCMP(cmp1, cmp2) == 0);
    PR_FREEIF(cmp1);
    PR_FREEIF(cmp2);
    return ok;
}

JSBool
lm_CheckPermissions(JSContext *cx, JSObject *obj, JSTarget target)
{
    const char *subjectOrigin, *objectOrigin;
    MochaDecoder *running;
    JSPrincipals *principals;
    JSBool result;

    /* May be in a layer loaded from a different origin.*/
    subjectOrigin = lm_GetSubjectOriginURL(cx);

    /*
     * Hold onto reference to the running decoder's principals
     * in case a call to lm_GetInnermostPrincipals ends up 
     * dropping a reference due to an origin changing 
     * underneath us.
     */
    running = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    principals = running ? running->principals : NULL;
    if (principals)
	JSPRINCIPALS_HOLD(cx, principals);

    objectOrigin = lm_GetObjectOriginURL(cx, obj);

    if (subjectOrigin == NULL || objectOrigin == NULL) {
	result = JS_FALSE;
	goto out;
    }

    /* Now see whether the origin methods and servers match. */
    if (sameOrigins(cx, subjectOrigin, objectOrigin)) {
	result = JS_TRUE;
	goto out;
    }

    /*
     * If we failed the origin tests it still might be the case that we
     *   are a signed script and have permissions to do this operation.
     * Check for that here
     */
    if (target != JSTARGET_MAX && lm_CanAccessTarget(cx, target)) {
	result = JS_TRUE;
	goto out;
    }

    JS_ReportError(cx, access_error_message, subjectOrigin);
    result = JS_FALSE;

out:
    if (principals)
	JSPRINCIPALS_DROP(cx, principals);
    return result;
}

static JSBool
isExternalCaptureEnabled(JSContext *cx, JSPrincipals *principals);

JSBool
lm_CanCaptureEvent(JSContext *cx, JSFunction *fun, JSEvent *event)
{
    JSScript *script;
    JSPrincipals *principals;
    const char *origin;

    script = JS_GetFunctionScript(cx, fun);
    if (script == NULL)
        return JS_FALSE;
    principals = JS_GetScriptPrincipals(cx, script);
    if (principals == NULL)
        return JS_FALSE;
    origin = lm_GetObjectOriginURL(cx, event->object);
    if (origin == NULL)
        return JS_FALSE;
    return (JSBool)(sameOrigins(cx, origin, principals->codebase) ||
           isExternalCaptureEnabled(cx, principals));
}


JSPrincipals *
lm_GetPrincipalsFromStackFrame(JSContext *cx)
{
    /*
     * Get principals from script of innermost interpreted frame.
     */
    JSStackFrame *fp;
    JSScript *script;

    fp = NULL;
    while ((fp = JS_FrameIterator(cx, &fp)) != NULL) {
        script = JS_GetFrameScript(cx, fp);
        if (script) {
            return JS_GetScriptPrincipals(cx, script);
        }
    }
#ifdef JAVA
    if (JSJ_IsCalledFromJava(cx)) {
        return LM_GetJSPrincipalsFromJavaCaller(cx, 0);
    }
#endif

    return NULL;
}

const char *
lm_GetSubjectOriginURL(JSContext *cx)
{
    /*
     * Get origin from script of innermost interpreted frame.
     */
    JSPrincipals *principals;
    JSStackFrame *fp;
    JSScript *script;
    JRIEnv *env;
    char *str;
    MochaDecoder *running;

    fp = NULL;
    while ((fp = JS_FrameIterator(cx, &fp)) != NULL) {
        script = JS_GetFrameScript(cx, fp);
        if (script) {
            principals = JS_GetScriptPrincipals(cx, script);
            return principals
                ? principals->codebase
                : JS_GetScriptFilename(cx, script);
        }
    }

#ifdef JAVA
    /* fell off the js stack, look to see if there's a java
     * classloader above us that has MAYSCRIPT set on it */
    if (JSJ_IsCalledFromJava(cx)) {
        env = LJ_JSJ_CurrentEnv(cx);
        if (!env) {
            return NULL;
        }

        str = LJ_GetAppletScriptOrigin(env);
        if (!str)
	    return NULL;
        return str;
    }
#endif

    /*
     * Not called from either JS or Java. We must be called
     * from the interpreter. Get the origin from the decoder.
     */
    running = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    return lm_GetObjectOriginURL(cx, running->window_object);
}

/*
 * Reference count ZIGs to increase sharing since creating
 * them is so expensive.
 */
typedef struct SharedZig {
    ZIG *zig;
    int32 refCount;
    JRIGlobalRef zigObjectRef;
} SharedZig;

static SharedZig *
newSharedZig(JRIEnv *env, zip_t *zip)
{
#ifdef JAVA
    ZIG *zig;
    SharedZig *result;
    struct netscape_security_Zig *zigObject = NULL;
    JRIGlobalRef zigObjectRef;

    zig = LJ_InitializeZig(zip);
    if (zig == NULL)
        return NULL;

    zigObject = ns_createZigObject(env, zig);
    if (zigObject == NULL) {
        SOB_destroy(zig);
        return NULL;
    }

    /* 
     * From this point on, Java will call SOB_destroy, when java ref 
     * count goes to zero.
     */
    zigObjectRef = JRI_NewGlobalRef(env, zigObject);
    if (zigObjectRef == NULL) {
        return NULL;
    }

    result = (SharedZig *) XP_ALLOC(sizeof(SharedZig));
    if (result == NULL) {
        JRI_DisposeGlobalRef(env, zigObjectRef);
        return NULL;
    }
    result->zig = zig;
    result->refCount = 0;
    result->zigObjectRef = zigObjectRef;
    return result;
#else
    return NULL;
#endif
}

static void
destroySharedZig(JRIEnv *env, SharedZig *sharedZig)
{
#ifdef JAVA
    /* ZigObject will call SOB_destroy when java reference count goes to zero */
    JRI_DisposeGlobalRef(env, sharedZig->zigObjectRef);
    XP_FREE(sharedZig);
#endif
}

static SharedZig *
holdZig(JRIEnv *env, SharedZig *sharedZig)
{
#ifdef JAVA
    if (sharedZig) {
        XP_ASSERT(sharedZig->refCount >= 0);
        /* XXX: Why are you checking this again */
        if (sharedZig)
            sharedZig->refCount++;
    }
    return sharedZig;
#else
    return NULL;
#endif
}

static void
dropZig(JRIEnv *env, SharedZig *sharedZig)
{
#ifdef JAVA
    if (sharedZig) {
        XP_ASSERT(sharedZig->refCount > 0);
        if (--sharedZig->refCount == 0) {
            destroySharedZig(env, sharedZig);
        }
    }
#endif
}

struct JSPrincipalsList {
    JSPrincipals *principals;
    struct JSPrincipalsList *next;
};

void
lm_DestroyPrincipalsList(JSContext *cx, JSPrincipalsList *p)
{
    while (p) {
	JSPrincipalsList *next = p->next;
	if (p->principals)
	    JSPRINCIPALS_DROP(cx, p->principals);
	XP_FREE(p);
	p = next;
    }
}

enum Signedness {
    HAS_NO_SCRIPTS,
    HAS_UNSIGNED_SCRIPTS,
    HAS_SIGNED_SCRIPTS
};

#ifdef DEBUG_norris
static int serial;
#endif

typedef struct JSPrincipalsData {
    JSPrincipals principals;
    SharedZig *sharedZig;
    JRIGlobalRef principalsArrayRef;
    URL_Struct *url_struct;
    char *name;
    zip_t *zip;
    uint32 externalCapturePrincipalsCount;
    char *untransformed;
    char *transformed;
    JSBool needUnlock;
    char *codebaseBeforeSettingDomain;
#ifdef DEBUG_norris
    int serial;
#endif
    enum Signedness signedness;
} JSPrincipalsData;

PR_STATIC_CALLBACK(void)
destroyJSPrincipals(JSContext *cx, JSPrincipals *principals);

static JSBool
principalsCanAccessTarget(JSContext *cx, JSTarget target);

PR_STATIC_CALLBACK(void *)
getPrincipalArray(JSContext *cx, struct JSPrincipals *principals);

PR_STATIC_CALLBACK(JSBool)
globalPrivilegesEnabled(JSContext *cx, JSPrincipals *principals);

static struct netscape_security_PrivilegeManager *
getPrivilegeManager(JRIEnv *env);

static JSPrincipalsData unknownPrincipals = {
    {
        lm_unknown_origin_str,
        getPrincipalArray,
        globalPrivilegesEnabled,
        0,
        destroyJSPrincipals
    },
    NULL
};

static char *
getOriginFromSourceURL(const char *sourceURL)
{
    char *s;
    char *result;
    int urlType;

    if (*sourceURL == '\0' || XP_STRCMP(sourceURL, lm_unknown_origin_str) == 0) {
        return XP_STRDUP(lm_unknown_origin_str);
    }
    urlType = NET_URL_Type(sourceURL);
    if (urlType == WYSIWYG_TYPE_URL) {
        sourceURL = LM_SkipWysiwygURLPrefix(sourceURL);
    } else if (urlType == MOCHA_TYPE_URL) {
        XP_ASSERT(JS_FALSE);    /* this shouldn't occur */
        return XP_STRDUP(lm_unknown_origin_str);
    }
    s = strip_file_double_slash(sourceURL);
    if (s == NULL)
        return NULL;
    result = NET_ParseURL(s, GET_PROTOCOL_PART|GET_HOST_PART|GET_PATH_PART);
    PR_FREEIF(s);
    return result;
}

static char *
getJavaCodebaseFromOrigin(const char *origin)
{
    /* Remove filename part. */
    char *result = XP_STRDUP(origin);
    if (result) {
        char *slash = XP_STRRCHR(result, '/');
        if (slash && slash > result && slash[-1] != '/')
            slash[1] = '\0';
    }
    return result;
}


extern JSPrincipals *
LM_NewJSPrincipals(URL_Struct *archive, char *id, const char *codebase)
{
    JSPrincipalsData *result;
    JSBool needUnlock = JS_FALSE;
    zip_t *zip = NULL;

    if (archive) {
        char *fn = NULL;

        if (NET_IsLocalFileURL(archive->address)) {
            char* pathPart = NET_ParseURL(archive->address, GET_PATH_PART);
            NET_UnEscape(pathPart); /* Handle "file:D%7C/dir/file.zip" */
            fn = WH_FileName(pathPart, xpURL);
            XP_FREE(pathPart);
        } else if (archive->cache_file && NET_ChangeCacheFileLock(archive, TRUE)) {
            fn = WH_FileName(archive->cache_file, xpCache);
            needUnlock = JS_TRUE;
        }

        if (fn) {
#ifdef XP_MAC
            /*
             * Unfortunately, zip_open wants a Unix-style name. Convert Mac path
             * to a Unix-style path. This code is copied from appletStubs.c.
             */
            OSErr ConvertMacPathToUnixPath(const char *macPath, char **unixPath);
            char *unixPath = NULL;

            if (ConvertMacPathToUnixPath(fn, &unixPath) == 0) {
                zip = zip_open(unixPath);
            }
            XP_FREEIF(unixPath);
#else
            zip = zip_open(fn);
#endif
            XP_FREE(fn);
        }
    }

    result = XP_NEW_ZAP(JSPrincipalsData);
    if (result == NULL)
        return NULL;
    result->principals.codebase = codebase
        ? getOriginFromSourceURL(codebase)
        : NULL;
    if (result->principals.codebase == NULL) {
        result->principals.codebase = XP_STRDUP(lm_unknown_origin_str);
        if (result->principals.codebase == NULL) {
            XP_FREE(result);
            return NULL;
        }
    }
    if (id) {
        result->name = XP_STRDUP(id);
        if (result->name == NULL) {
            XP_FREE(result);
            return NULL;
        }
    }
    result->principals.destroy = destroyJSPrincipals;
    result->principals.getPrincipalArray = getPrincipalArray;
    result->principals.globalPrivilegesEnabled = globalPrivilegesEnabled;
    result->url_struct = NET_HoldURLStruct(archive);
    result->zip = zip;
    result->needUnlock = needUnlock;
#ifdef DEBUG_norris
    result->serial = ++serial;
    XP_TRACE(("JSPrincipals #%.4d allocated\n", serial));
#endif

    return (JSPrincipals *) result;
}


PR_STATIC_CALLBACK(void)
destroyJSPrincipals(JSContext *cx, JSPrincipals *principals)
{
    if (principals != NULL &&
        principals != (JSPrincipals *) &unknownPrincipals)
    {
        JSPrincipalsData *data = (JSPrincipalsData *) principals;
#ifdef JAVA
        JRIEnv *env = NULL;
        if (data->sharedZig || data->principalsArrayRef) {
            /* Avoid starting Java if "env" not needed */
            env = LJ_JSJ_CurrentEnv(cx);
        }
#endif /* JAVA */

#ifdef DEBUG_norris
        XP_TRACE(("JSPrincipals #%.4d released\n", data->serial));
#endif
        XP_FREEIF(principals->codebase);
#ifdef JAVA
        if (env && data->sharedZig) {
            dropZig(env, data->sharedZig);
        }
        if (env && (data->principalsArrayRef != NULL)) {
            JRI_DisposeGlobalRef(env, data->principalsArrayRef);
        }
#endif /* JAVA */
        XP_FREEIF(data->name);
        XP_FREEIF(data->untransformed);
        XP_FREEIF(data->transformed);
        if (data->zip)
            zip_close(data->zip);
        if (data->needUnlock)
            NET_ChangeCacheFileLock(data->url_struct, FALSE);
        if (data->url_struct)
            NET_DropURLStruct(data->url_struct);
        XP_FREEIF(data->codebaseBeforeSettingDomain);
        XP_FREE(data);
    }
}

PR_STATIC_CALLBACK(JSBool)
globalPrivilegesEnabled(JSContext *cx, JSPrincipals *principals)
{
    JSPrincipalsData *data = (JSPrincipalsData *) principals;

    return (JSBool)(data->principalsArrayRef != NULL ||
           XP_STRCMP(principals->codebase, lm_unknown_origin_str) != 0);
}

static void
printPrincipalsToConsole(JSContext *cx, JSPrincipals *principals)
{
#ifdef JAVA
    JRIEnv *env;
    jobjectArray principalsArray;
    struct netscape_security_Principal *principal;
    struct java_lang_String *javaString;
    uint32 i, count;
    static char emptyStr[] = "<empty>\n";

    env = LJ_JSJ_CurrentEnv(cx);
    if (env == NULL) {
        return;
    }

    principalsArray = principals->getPrincipalArray(cx, principals);

    if (principalsArray == NULL) {
        PrintToConsole(emptyStr);
        return;
    }

    PrintToConsole("[\n");
    count = JRI_GetObjectArrayLength(env, principalsArray);
    for (i = 0; i < count; i++) {
        principal = JRI_GetObjectArrayElement(env, principalsArray, i);
        javaString = netscape_security_Principal_getVendor(env, principal);
        if (javaString) {
            const char *s = JRI_GetStringPlatformChars(env, javaString,
                                     (const jbyte *)cx->charSetName,
                                     (jint)cx->charSetNameLength);
            if (s == NULL) {
                JS_ReportOutOfMemory(cx);
                return;
            }
            PrintToConsole(s);
        }
        PrintToConsole(",\n");
    }
    PrintToConsole("]\n");
#endif /* JAVA */
}

extern void
lm_InvalidateCertPrincipals(MochaDecoder *decoder, JSPrincipals *principals)
{
    JSPrincipalsData *data = (JSPrincipalsData *) principals;
#ifdef JAVA
    JRIEnv *env;

    if (data->principalsArrayRef) {
        PrintToConsole("Invalidating certificate principals in ");
        printPrincipalsToConsole(decoder->js_context, principals);
        env = LJ_JSJ_CurrentEnv(decoder->js_context);
        if (env != NULL) {
            JRI_DisposeGlobalRef(env, data->principalsArrayRef);
        }
        data->principalsArrayRef = NULL;
    }
#endif /* JAVA */
    data->signedness = HAS_UNSIGNED_SCRIPTS;
}

extern JSBool
lm_SetDocumentDomain(JSContext *cx, JSPrincipals *principals,
                     const char *codebase)
{
    JSPrincipalsData *data;

    if (principals->codebase == codebase)
       return JS_TRUE;
    data = (JSPrincipalsData *) principals;
    if (data->codebaseBeforeSettingDomain == NULL)
        data->codebaseBeforeSettingDomain = principals->codebase;
    else
        XP_FREEIF(principals->codebase);
    principals->codebase = getOriginFromSourceURL(codebase);
    if (principals->codebase == NULL) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
#ifdef JAVA
    if (data->principalsArrayRef != NULL) {
        JRIEnv *env = LJ_JSJ_CurrentEnv(cx);
        if (env == NULL) {
            return JS_FALSE;
        }
        JRI_DisposeGlobalRef(env, data->principalsArrayRef);
        data->principalsArrayRef = NULL;
    }
#endif /* JAVA */
    return JS_TRUE;
}

JSPrincipals *
lm_GetInnermostPrincipals(JSContext *cx, JSObject *container,
                          JSObject **foundIn)
{
    /* Get innermost non-null principals */
    while (container) {
        if (foundIn)
            *foundIn = container;
        if (JS_InstanceOf(cx, container, &lm_layer_class, 0)) {
            JSPrincipals *principals = lm_GetContainerPrincipals(cx, container);
            if (principals)
                return principals;
        } else if (JS_InstanceOf(cx, container, &lm_window_class, 0)) {
            MochaDecoder *decoder = JS_GetInstancePrivate(cx, container,
                                                          &lm_window_class,
                                                          NULL);
            const char *origin_url;

	    /* 
             * We need to check that the origin hasn't changed underneath 
             * us as a result of user navigation. 
             */
            origin_url = find_origin_url(cx, decoder);
            if (!origin_url)
                return NULL;
            if (decoder->principals) {
                JSPrincipalsData *data;

                if (sameOrigins(cx, origin_url, decoder->principals->codebase))
                    return decoder->principals;
                data = (JSPrincipalsData *) decoder->principals;
                if (data->codebaseBeforeSettingDomain &&
                    sameOrigins(cx, origin_url, 
                                data->codebaseBeforeSettingDomain))
                {
                    /* document.domain was set, so principals are okay */
                    return decoder->principals;
                }
                /* Principals have changed underneath us. Remove them. */
                JSPRINCIPALS_DROP(cx, decoder->principals);
                decoder->principals = NULL;
            }
            /* Create new principals and return them. */
            decoder->principals = LM_NewJSPrincipals(NULL, NULL, origin_url);
            if (decoder->principals == NULL) {
                JS_ReportOutOfMemory(cx);
                return NULL;
            }
            JSPRINCIPALS_HOLD(cx, decoder->principals);
            return decoder->principals;
        }
        container = JS_GetParent(cx, container);
    }
    if (foundIn)
        *foundIn = NULL;
    return (JSPrincipals *) &unknownPrincipals;
}

JSBool lm_CheckSetParentSlot(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSObject *newParent;

    if (!JSVAL_IS_OBJECT(*vp))
	return JS_TRUE;
    newParent = JSVAL_TO_OBJECT(*vp);
    if (newParent) {
        const char *oldOrigin = lm_GetObjectOriginURL(cx, obj);
        const char *newOrigin = lm_GetObjectOriginURL(cx, newParent);
        if (!sameOrigins(cx, oldOrigin, newOrigin))
            return JS_TRUE;
    } else {
        if (!JS_InstanceOf(cx, obj, &lm_layer_class, 0) && 
            !JS_InstanceOf(cx, obj, &lm_window_class, 0))
        {
            return JS_TRUE;
        }
        if (lm_GetContainerPrincipals(cx, obj) == NULL) {
            JSPrincipals *principals;
            principals = lm_GetInnermostPrincipals(cx, obj, NULL);
            if (principals == NULL)
                return JS_FALSE;
            lm_SetContainerPrincipals(cx, obj, principals);
        }
    }
    return JS_TRUE;
}

static JSBool
canExtendTrust(JSContext *cx, void *from, void *to)
{
#ifdef JAVA
    JRIEnv *env = LJ_JSJ_CurrentEnv(cx);
    if (env == NULL) {
        return JS_FALSE;
    }
    if (from == NULL || to == NULL) {
        return JS_FALSE;
    }
    return (JSBool)netscape_security_PrivilegeManager_canExtendTrust(
                    env,
                    getPrivilegeManager(env),
                    from,
                    to);
#else
    /* should never be called without signed scripts */
    XP_ASSERT(0);
    return JS_FALSE;
#endif /* JAVA */
}

static JSPrincipals *
newJSPrincipalsFromArray(JSContext *cx, jobjectArray principalsArray);

extern JSBool
lm_CheckContainerAccess(JSContext *cx, JSObject *obj, MochaDecoder *decoder,
                        JSTarget target)
{
    JSPrincipals *principals;
    JSPrincipalsData *data;
    JRIEnv *env;
    JSStackFrame *fp;
    JSScript *script;
    JSPrincipals *subjPrincipals;
    JSPrincipalsList *list;
    const char *fn;

    if(decoder->principals)  {
        principals = lm_GetInnermostPrincipals(decoder->js_context, obj, NULL);
    }  else  {
	principals = NULL;
    }

    if (principals == NULL) {
        /*
         * Attempt to access container before container has any scripts.
         * Most of these accesses come from natives when initializing a
         * window. Check for that by seeing if we have an executing script.
         * If we do, remember the principals of the script that performed
         * the access so we can report an error later if need be.
         */
        fp = NULL;
        subjPrincipals = lm_GetPrincipalsFromStackFrame(cx);
        if (subjPrincipals == NULL) {
            return JS_TRUE;
        }

        /* See if subjPrincipals are already on list */
        list = (JSPrincipalsList *) decoder->early_access_list;
        while (list && list->principals != subjPrincipals) {
            list = list->next;
        }
        if (list == NULL) {
	    list = XP_ALLOC(sizeof(*list));
            if (list == NULL) {
                JS_ReportOutOfMemory(cx);
		return JS_FALSE;
            }
	    list->principals = subjPrincipals;
	    JSPRINCIPALS_HOLD(cx, list->principals);
	    list->next = (JSPrincipalsList *) decoder->early_access_list;
	    decoder->early_access_list = list;
        }
	/*
	 * XXX - Still possible to modify contents of another page
	 * even if cross-origin access is disabled by setting to
	 * about:blank, modifying, and then loading the attackee.
         * Similarly with window.open("").
	 */
        return JS_TRUE;
    }

    /* 
     * If object doesn't have signed scripts and cross-origin access 
     * is enabled, return true. 
     */
    data = (JSPrincipalsData *) principals;
    if (data->signedness != HAS_SIGNED_SCRIPTS && lm_GetCrossOriginEnabled())
        return JS_TRUE;

    /* Check if user requested lower privileges */

    if (data->signedness == HAS_SIGNED_SCRIPTS &&
        !lm_GetPrincipalsCompromise(cx, obj))
    {
        /*
         * We have signed scripts. Must check that the object principals are
         * a subset of the the subject principals.
         */
        env = LJ_JSJ_CurrentEnv(cx);
        if (env == NULL) {
            return JS_FALSE;
        }
        fp = NULL;
        fp = JS_FrameIterator(cx, &fp);
        if (fp == NULL || (script = JS_GetFrameScript(cx, fp)) == NULL) {
            /* haven't begun execution yet; allow the parser to create functions */
            return JS_TRUE;
        }
        subjPrincipals = JS_GetScriptPrincipals(cx, script);
        if (subjPrincipals &&
            canExtendTrust(cx,
                principals->getPrincipalArray(cx, principals),
                subjPrincipals->getPrincipalArray(cx, subjPrincipals)))
        {
            return JS_TRUE;
        }
        fn = lm_GetSubjectOriginURL(cx);
	if (!fn)
	    return JS_FALSE;
        if (subjPrincipals && principals) {
            PrintToConsole("Principals of script: ");
            printPrincipalsToConsole(cx, subjPrincipals);
            PrintToConsole("Principals of signed container: ");
            printPrincipalsToConsole(cx, principals);
        }
        JS_ReportError(cx, container_error_message, fn);
        return JS_FALSE;
    }

    /* The signed script has called compromisePrincipals(), so
     * we do the weaker origin check.
     */
    return lm_CheckPermissions(cx, obj, target);
}

static JSBool
checkEarlyAccess(MochaDecoder *decoder, JSPrincipals *principals)
{
    JSContext *cx;
    JSPrincipalsData *data;
    JSPrincipalsList *p;
    JSBool ok;

    cx = decoder->js_context;
    data = (JSPrincipalsData *) principals;
    ok = JS_TRUE;

    for (p = (JSPrincipalsList *) decoder->early_access_list; p; p = p->next) {
        if (data->signedness == HAS_SIGNED_SCRIPTS) {
            if (!canExtendTrust(cx,
		                principals->getPrincipalArray(cx, principals),
			        p->principals->getPrincipalArray(cx,
                                                                 p->principals)))
            {
                JS_ReportError(cx, container_error_message,
                               p->principals->codebase);
                ok = JS_FALSE;
                break;
            }
        } else {
            if (!sameOrigins(cx, p->principals->codebase,
		             principals->codebase))
            {
                /*
                 * Check to see if early access violated the cross-origin
                 * container check.
                 */
                JS_ReportError(cx, access_error_message,
		               p->principals->codebase);
                ok = JS_FALSE;
                break;
            }
        }
    }
    lm_DestroyPrincipalsList(cx, decoder->early_access_list);
    decoder->early_access_list = NULL;
    return ok;
}

/*
 * Compute the intersection of "principals" and "other", saving in
 * "principals". Return true iff the intersection is nonnull.
 */
static JSBool
intersectPrincipals(MochaDecoder *decoder, JSPrincipals *principals,
                    JSPrincipals *newPrincipals)
{
    JSPrincipalsData *data = (JSPrincipalsData *) principals;
    JSPrincipalsData *newData = (JSPrincipalsData *) newPrincipals;
    JSContext *cx;
#ifdef JAVA
    JRIEnv *env;
    struct netscape_security_PrivilegeManager *privilegeManager;
    jobjectArray principalArray, newPrincipalArray;
    JRIGlobalRef principalsArrayRef;
#endif /* JAVA */

    XP_ASSERT(data->signedness != HAS_NO_SCRIPTS);
    XP_ASSERT(newData->signedness != HAS_NO_SCRIPTS);

    cx = decoder->js_context;
    if (!sameOrigins(cx, principals->codebase, newPrincipals->codebase)) {
        XP_FREEIF(principals->codebase);
        principals->codebase = JS_strdup(cx, lm_unknown_origin_str);
        if (principals->codebase == NULL) {
            return JS_FALSE;
        }
    }

    if (data->signedness == HAS_UNSIGNED_SCRIPTS ||
        newData->signedness == HAS_UNSIGNED_SCRIPTS)
    {
        /*
         * No cert principals. Nonempty only if there is a codebase
         * principal.
         */
        lm_InvalidateCertPrincipals(decoder, principals);
        return JS_TRUE;
    }
#ifdef JAVA 
    /* Compute the intersection. */
    env = LJ_JSJ_CurrentEnv(cx);
    if (env == NULL) {
        lm_InvalidateCertPrincipals(decoder, principals);
        return globalPrivilegesEnabled(cx, principals);
    }
    privilegeManager = getPrivilegeManager(env);
    principalArray = getPrincipalArray(cx, principals);
    newPrincipalArray = getPrincipalArray(cx, newPrincipals);
    if (privilegeManager == NULL || principalArray == NULL
        || newPrincipalArray == NULL)
    {
        lm_InvalidateCertPrincipals(decoder, principals);
        return JS_TRUE;
    }

    principalArray = netscape_security_PrivilegeManager_intersectPrincipalArray(
        env, privilegeManager, principalArray, newPrincipalArray);
    principalsArrayRef = JRI_NewGlobalRef(env, principalArray);

    if (principalArray == NULL) {
        lm_InvalidateCertPrincipals(decoder, principals);
        return JS_TRUE;
    }

    JRI_DisposeGlobalRef(env, data->principalsArrayRef);
    data->principalsArrayRef = principalsArrayRef;
    return JS_TRUE;
#else
    XP_ASSERT(0); /* should never get here without signed scripts */
    return JS_FALSE;
#endif /* JAVA */
}

static uint32
getPrincipalsCount(JSContext *cx, JSPrincipals *principals)
{
#ifdef JAVA
    JRIEnv *env;
    jref principalArray;

    env = LJ_JSJ_CurrentEnv(cx);
    if (env == NULL) {
        return 0;
    }

    /* Get array of principals */
    principalArray = getPrincipalArray(cx, principals);

    return principalArray ? JRI_GetObjectArrayLength(env, principalArray) : 0;
#else
    return 0;
#endif /* JAVA */
}

static JSBool
principalsEqual(JSContext *cx, JSPrincipals *a, JSPrincipals *b)
{
#ifdef JAVA
    JSPrincipalsData *dataA, *dataB;
    jobjectArray arrayA, arrayB;
    JRIEnv *env;

    if (a == b)
        return JS_TRUE;

    dataA = (JSPrincipalsData *) a;
    dataB = (JSPrincipalsData *) b;

    if (dataA->signedness != dataB->signedness)
        return JS_FALSE;

    arrayA = getPrincipalArray(cx, a);
    arrayB = getPrincipalArray(cx, b);

    env = LJ_JSJ_CurrentEnv(cx);
    if (env == NULL) {
        return JS_FALSE;
    }

    return (JSBool)(netscape_security_PrivilegeManager_comparePrincipalArray(env,
                    getPrivilegeManager(env),
                    arrayA,
                    arrayB)
                == netscape_security_PrivilegeManager_EQUAL);
#else
    /* Shouldn't get here without signed scripts */
    XP_ASSERT(0);
    return JS_FALSE;
#endif /* JAVA */
}

/*
 * createPrincipalsArray takes ZIG file information and returns a
 * reference to a Java array of Java Principal objects.
 * It also registers the principals with the PrivilegeManager.
 */
static jref
createPrincipalsArray(JRIEnv *env,
                      struct netscape_security_PrivilegeManager *privilegeManager,
                      JSPrincipals *principals)
{
#ifdef JAVA 
    JSPrincipalsData *data = (JSPrincipalsData *) principals;
    JSBool hasCodebase;
    SOBITEM *item;
    int i;
    ZIG *zig;
    struct java_lang_Class *principalClass;
    unsigned count;
    jref result;
    jref byteArray;
    struct netscape_security_Principal *principal;
    ZIG_Context * zig_cx = NULL;
    void *zigObj;

    if (principals == (JSPrincipals *) &unknownPrincipals)
        return NULL;

    hasCodebase = (JSBool)(principals->codebase && 
                  XP_STRCMP(principals->codebase, lm_unknown_origin_str) != 0);

    /* First count the number of principals */
    count = hasCodebase ? 1 : 0;

    zig = data->signedness == HAS_UNSIGNED_SCRIPTS
        ? NULL
        : (data->sharedZig ? data->sharedZig->zig : NULL);

    if (zig && data->name) {
        /* Make sure file is signed */
        if ((zig_cx = SOB_find(zig, data->name, ZIG_SIGN)) != NULL) {
            int zig_count=0;
            /* count the number of signers */
            while (SOB_find_next(zig_cx, &item) >= 0) {
                zig_count++;
            }
            SOB_find_end(zig_cx);
            count += zig_count;
        } else {
          zig = NULL;
        }
    }

    if (count == 0) {
        return NULL;
    }

    /* Now allocate the array */
    principalClass = class_netscape_security_Principal(env);
    result = JRI_NewObjectArray(env, count, principalClass, NULL);
    if (result == NULL) {
        return NULL;
    }

    if (zig && ((zig_cx = SOB_find(zig, data->name, ZIG_SIGN)) == NULL)) {
        return NULL;
    }

    i = 0;
    if (zig) {
        zigObj = JRI_GetGlobalRef(env, data->sharedZig->zigObjectRef);
    }
    while (zig && SOB_find_next(zig_cx, &item) >= 0) {
        FINGERZIG *fingPrint;

        fingPrint = (FINGERZIG *) item->data;

        /* call java: new Principal(CERT_KEY, fingPrint->key) */
        byteArray = JRI_NewByteArray(env, fingPrint->length,
                                     fingPrint->key);

        principal = netscape_security_Principal_new_5(env,
            principalClass,
            netscape_security_Principal_CERT_KEY,
            byteArray,
            (struct netscape_security_Zig *)zigObj);

        netscape_security_PrivilegeManager_registerPrincipal(env,
            privilegeManager, principal);

        JRI_SetObjectArrayElement(env, result, i++, principal);
    }
    if (zig) {
         SOB_find_end(zig_cx);
    }

    if (hasCodebase) {
        /* Add a codebase principal. */
        char *javaCodebase;
        javaCodebase = getJavaCodebaseFromOrigin(principals->codebase);
        if (javaCodebase == NULL)
            return NULL;
        byteArray = JRI_NewByteArray(env,
                                     XP_STRLEN(javaCodebase),
                                     javaCodebase);
        principal = netscape_security_Principal_new_3(env,
            principalClass,
            netscape_security_Principal_CODEBASE_EXACT,
            byteArray);
        netscape_security_PrivilegeManager_registerPrincipal(env,
            privilegeManager, principal);
        JRI_SetObjectArrayElement(env, result, i++, principal);
        XP_FREE(javaCodebase);
    }

    data->principalsArrayRef = JRI_NewGlobalRef(env, result);

    return result;
#else
    /* This should never be called without signed scripts. */
    XP_ASSERT(0);
    return NULL;
#endif /* JAVA */
}

static JSBool
isExternalCaptureEnabled(JSContext *cx, JSPrincipals *principals)
{
    JSPrincipalsData *data = (JSPrincipalsData *) principals;

    if (data->externalCapturePrincipalsCount == 0) {
        return JS_FALSE;
    } else {
        uint32 count = getPrincipalsCount(cx, principals);
        return (JSBool)(data->externalCapturePrincipalsCount == count);
    }
}

void
lm_SetExternalCapture(JSContext *cx, JSPrincipals *principals,
                      JSBool b)
{
    JSPrincipalsData *data = (JSPrincipalsData *) principals;

    if (b) {
        uint32 count = getPrincipalsCount(cx, principals);
        data->externalCapturePrincipalsCount = count;
    } else {
        data->externalCapturePrincipalsCount = 0;
    }
}


JSBool
lm_CanAccessTarget(JSContext *cx, JSTarget target)
{
    JSPrincipals *principals;

    principals = lm_GetPrincipalsFromStackFrame(cx);
    if (principals && !globalPrivilegesEnabled(cx, principals)) {
        return JS_FALSE;
    }
    if (!principalsCanAccessTarget(cx, target)) {
        return JS_FALSE;
    }
    return JS_TRUE;
}

/* This array must be kept in sync with the JSTarget enum in jsapi.h */
static char *targetStrings[] = {
    "UniversalBrowserRead",
    "UniversalBrowserWrite",
    "UniversalSendMail",
    "UniversalFileRead",
    "UniversalFileWrite",
    "UniversalPreferencesRead",
    "UniversalPreferencesWrite",
    /* See Target.java for more targets */
};

/* get PrivilegeManager object: call static PrivilegeManager.getPrivilegeManager() */
static struct netscape_security_PrivilegeManager *
getPrivilegeManager(JRIEnv *env)
{
#ifdef JAVA
    return netscape_security_PrivilegeManager_getPrivilegeManager(env,
        JRI_FindClass(env,
            classname_netscape_security_PrivilegeManager));
#else
    return NULL;
#endif
}

/*
** If given principals can access the given target, return true. Otherwise return false.
** The script must already have explicitly requested access to the given target.
*/
static JSBool
principalsCanAccessTarget(JSContext *cx, JSTarget target)
{
#ifdef JAVA
    JRIEnv *env;
    struct netscape_security_PrivilegeManager *privilegeManager;
    struct netscape_security_PrivilegeTable *annotation;
    struct netscape_security_Privilege *privilege;
    struct netscape_security_Target *javaTarget;
    struct java_lang_String *javaString;
    struct java_lang_Class *targetClass;
    jint perm;
    JSStackFrame *fp;
    jglobal annotationRef;
    void *principalArray = NULL;

    env = LJ_JSJ_CurrentEnv(cx);
    if (env == NULL) {
        return JS_FALSE;
    }

    privilegeManager = getPrivilegeManager(env);

    /* Map JSTarget to netscape_security_Target */
    XP_ASSERT(target >= 0);
    XP_ASSERT(target < sizeof(targetStrings)/sizeof(targetStrings[0]));
    targetClass = JRI_FindClass(env, classname_netscape_security_Target);
    javaString = makeJavaString(targetStrings[target], strlen(targetStrings[target]));
    javaTarget = netscape_security_Target_findTarget(env, targetClass, javaString);

    /* Find annotation */
    annotationRef = NULL;
    principalArray = NULL;
    fp = NULL;
    while ((fp = JS_FrameIterator(cx, &fp)) != NULL) {
        void *current;
        if (JS_GetFrameScript(cx, fp) == NULL)
            continue;
        current = JS_GetFramePrincipalArray(cx, fp);
        if (current == NULL) {
            return JS_FALSE;
        }
        annotationRef = (jglobal) JS_GetFrameAnnotation(cx, fp);
        if (annotationRef) {
            if (principalArray &&
                !netscape_security_PrivilegeManager_canExtendTrust(
                    env, privilegeManager, current, principalArray))
            {
                return JS_FALSE;
            }
            break;
        }
        principalArray = principalArray
            ? netscape_security_PrivilegeManager_intersectPrincipalArray(
                env, privilegeManager, principalArray, current)
            : current;
    }

    if (annotationRef) {
        annotation = (struct netscape_security_PrivilegeTable *)
            JRI_GetGlobalRef(env, annotationRef);
    } else if (JSJ_IsCalledFromJava(cx)) {
        /*
         * Call from Java into JS. Just call the Java routine for checking
         * privileges.
         */
        if (principalArray) {
            /*
             * Must check that the principals that signed the Java applet are
             * a subset of the principals that signed this script.
             */
            jobjectArray javaPrincipals;

            javaPrincipals =
                netscape_security_PrivilegeManager_getClassPrincipalsFromStack(
                    env,
                    privilegeManager,
                    0);
            if (!canExtendTrust(cx, javaPrincipals, principalArray)) {
                return JS_FALSE;
            }
        }
        return (JSBool)netscape_security_PrivilegeManager_isPrivilegeEnabled(
            env, privilegeManager, javaTarget, 0);
    } else {
        /* No annotation in stack */
        return JS_FALSE;
    }

   /* Now find permission for (annotation, target) pair. */
    privilege = netscape_security_PrivilegeTable_get_1(env,
                                     annotation,
                                     javaTarget);
    if (JRI_ExceptionOccurred(env)) {
        return JS_FALSE;
    }
    XP_ASSERT(privilege);
    perm = netscape_security_Privilege_getPermission(env,
                                               privilege);
    XP_ASSERT(!JRI_ExceptionOccurred(env));

    return (JSBool)(perm == netscape_security_Privilege_ALLOWED);
#else
    return JS_FALSE;
#endif /* JAVA */
}


PR_STATIC_CALLBACK(void *)
getPrincipalArray(JSContext *cx, struct JSPrincipals *principals)
{
#ifdef JAVA
    JSPrincipalsData *data = (JSPrincipalsData *) principals;
    struct netscape_security_PrivilegeManager *privilegeManager;
    JRIEnv *env;

    env = LJ_JSJ_CurrentEnv(cx);
    if (env == NULL) {
        return NULL;
    }

    /* Get array of principals */

    if (data->principalsArrayRef == NULL) {
        privilegeManager = getPrivilegeManager(env);
        if (createPrincipalsArray(env, privilegeManager, principals) == NULL)
            return NULL;
    }

    return JRI_GetGlobalRef(env, data->principalsArrayRef);
#else
    return NULL;
#endif
}


extern char *
LM_ExtractFromPrincipalsArchive(JSPrincipals *principals, char *name,
                                uint *length)
{
    JSPrincipalsData *data = (JSPrincipalsData *) principals;
    char *result;

    result = LJ_LoadFromZipFile(data->zip, name);
    *length = result ? XP_STRLEN(result) : 0;

    return result;
}

extern JSBool
LM_SetUntransformedSource(JSPrincipals *principals, char *original,
                          char *transformed)
{
    JSPrincipalsData *data = (JSPrincipalsData *) principals;

    XP_ASSERT(data->untransformed == NULL);
    data->untransformed = XP_STRDUP(original);
    if (data->untransformed == NULL)
        return JS_FALSE;
    data->transformed = XP_STRDUP(transformed);
    if (data->transformed == NULL)
        return JS_FALSE;
    return JS_TRUE;
}

JSPrincipals * PR_CALLBACK
LM_GetJSPrincipalsFromJavaCaller(JSContext *cx, int callerDepth)
{
#ifdef JAVA
    JRIEnv *env;
    jobjectArray principalsArray;

    env = LJ_JSJ_CurrentEnv(cx);
    if (env == NULL) {
        return NULL;
    }

    principalsArray = native_netscape_security_PrivilegeManager_getClassPrincipalsFromStackUnsafe(
        env,
        getPrivilegeManager(env),
        callerDepth);

    if (principalsArray == NULL)
        return NULL;

    return newJSPrincipalsFromArray(cx, principalsArray);
#else
    return NULL;
#endif
}

static JSPrincipals *
newJSPrincipalsFromArray(JSContext *cx, jobjectArray principalsArray)
{
#ifdef JAVA
    JRIEnv *env;
    JSPrincipals *result;
    struct netscape_security_Principal *principal;
    struct java_lang_String *javaString;
    const char *codebase;
    JSPrincipalsData *data;
    uint32 i, count;

    env = LJ_JSJ_CurrentEnv(cx);
    if (env == NULL) {
        return NULL;
    }

    count = JRI_GetObjectArrayLength(env, principalsArray);
    if (count == 0) {
        JS_ReportError(cx, "No principals found for Java caller");
        return NULL;
    }

    javaString = NULL;
    for (i = count; i > 0; i--) {
        principal = JRI_GetObjectArrayElement(env, principalsArray, i-1);
        if (netscape_security_Principal_isCodebaseExact(env, principal)) {
            javaString = netscape_security_Principal_toString(env, principal);
            break;
        }
    }

    codebase = javaString
        ? JRI_GetStringPlatformChars(env, javaString,
                                     (const jbyte *)cx->charSetName,
                                     (jint)cx->charSetNameLength)
        : NULL;
    result = LM_NewJSPrincipals(NULL, NULL, (char *) codebase);
    if (result == NULL) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }

    data = (JSPrincipalsData *) result;
    data->principalsArrayRef = JRI_NewGlobalRef(env, principalsArray);
    data->signedness = count == 1 && codebase
                       ? HAS_UNSIGNED_SCRIPTS
                       : HAS_SIGNED_SCRIPTS;

    return result;
#else
    /* Should not get here without signed scripts */
    XP_ASSERT(0);
    return NULL;
#endif /* JAVA */
}

static JSBool
verifyPrincipals(MochaDecoder *decoder, JSPrincipals *containerPrincipals,
                 JSPrincipals *principals, char *name, char *src,
                 uint srcSize, JSBool implicitName)
{
#ifdef JAVA
    JSPrincipalsData *data = (JSPrincipalsData *) principals;
    ZIG *zig;
    DIGESTS *dig = NULL;
    JSBool sameName = JS_FALSE;
    int ret;
    JSPrincipalsData *containerData;
    zip_t *containerZip;
    JSBool verified;
    SOBITEM *item;
    ZIG_Context * zig_cx;
    JRIEnv *env;

    if (data->signedness == HAS_UNSIGNED_SCRIPTS)
        return JS_FALSE;

    containerData = (JSPrincipalsData *) containerPrincipals;

    containerZip = containerData && containerData->signedness != HAS_UNSIGNED_SCRIPTS
                   ? containerData->zip
                   : NULL;

    if (data->zip == NULL && containerZip == NULL)
        return JS_FALSE;

    if (data->name && data->signedness == HAS_NO_SCRIPTS) {
        if (XP_STRCMP(name, data->name) == 0) {
            sameName = JS_TRUE;
        } else {
            return JS_FALSE;
        }
    }

    /*
     * Set to the value we want if verification fails, and then
     * reset below.
     */
    verified = JS_FALSE;

    /* Start Java since errors may need to be printed to the console. */
    env = LJ_JSJ_CurrentEnv(decoder->js_context);
    if (env == NULL) {
        return JS_FALSE;
    }

    if (containerData == NULL) {
        /* First script seen; initialize zig. */
        data->sharedZig = holdZig(env, newSharedZig(env, data->zip));
    } else if (data == containerData) {
        /* Already have a zig if there is one; nothing more to do. */
    } else if (data->zip == NULL) {
        /* "Inherit" data->sharedZig from container data. */
        data->sharedZig = holdZig(env, containerData->sharedZig);
    } else if (containerData->url_struct &&
               XP_STRCMP(data->url_struct->address,
                         containerData->url_struct->address) == 0)
    {
        /* Two identical zips. Share the zigs. */
        data->sharedZig = holdZig(env, containerData->sharedZig);
    } else {
        /* Different zips. Must create a new zig. */
        data->sharedZig = holdZig(env, newSharedZig(env, data->zip));
    }

    if (data->sharedZig == NULL)
        return JS_FALSE;

    zig = data->sharedZig->zig;
    dig = SOB_calculate_digest(src, srcSize);
    if (dig == NULL)
        return JS_FALSE;

    zig_cx = NULL;
    ret = SOB_verify_digest(zig, name, dig);
    XP_FREE(dig);
    if ((ret >= 0) &&
        ((zig_cx = SOB_find(zig, name, ZIG_SIGN)) != NULL) &&
        (SOB_find_next(zig_cx, &item) >= 0))
    {
        verified = JS_TRUE;
        if (!sameName) {
            data->name = JS_strdup(decoder->js_context, name);
            if (data->name == NULL)
                return JS_FALSE;
        }
    } else if (!implicitName || ret != ZIG_ERR_PNF) {
        LJ_PrintZigError(ret, zig, "", name, SOB_get_error(ret));
    }
    if (zig_cx) {
        SOB_find_end(zig_cx);
    }
    return verified;
#else
    return JS_FALSE;
#endif /* JAVA */
}



extern JSPrincipals *
LM_RegisterPrincipals(MochaDecoder *decoder, JSPrincipals *principals,
                      char *name, char *src)
{
    JSContext *cx = decoder->js_context;
    JSBool verified;
    JSPrincipalsData *data;
    JSObject *inner, *container;
    JSPrincipals *containerPrincipals;
    JSPrincipalsData *containerData;
    char *untransformed, *implicitName;

    data = (JSPrincipalsData *) principals;
    inner = lm_GetActiveContainer(decoder);
    if (inner == NULL)
        return NULL;
    containerPrincipals = lm_GetInnermostPrincipals(decoder->js_context,
                                                    inner, &container);
    if (containerPrincipals == NULL) {
        /* Out of memory */
        return NULL;
    }
    containerData = (JSPrincipalsData *) containerPrincipals;

    if (name == NULL && principals != containerPrincipals && principals) {
        /*
         * "name" argument omitted since it was specified when "principals"
         * was created. Get it from "principals".
         */
        name = data->name;
    }
    implicitName = NULL;
    if (name == NULL && data && data->signedness == HAS_SIGNED_SCRIPTS) {
        /*
         * Name is unspecified. Use the implicit name formed from the
         * origin URL and the ordinal within the page. For example, the
         * third implicit name on http://www.co.com/dir/mypage.html
         * would be "_mypage2".
         */
        const char *url;
        char *path;

        url = LM_GetSourceURL(decoder);
        if (url == NULL) {
            return NULL;
        }
        path = *url? NET_ParseURL(url, GET_PATH_PART) : NULL;
        if (path && *path) {
            char *s;
            s = XP_STRRCHR(path, '.');
            if (s)
                *s = '\0';
            s = XP_STRRCHR(path, '/');
            implicitName = PR_sprintf_append(NULL, "_%s%d", s ? s+1 : path,
                                             decoder->signature_ordinal++);
            name = implicitName;
        }
        XP_FREEIF(path);
    }

    untransformed = NULL;
    if (data && data->untransformed && !XP_STRCMP(data->transformed, src)) {
        /* Perform verification on original source. */
        src = untransformed = data->untransformed;
        data->untransformed = NULL;
        XP_FREE(data->transformed);
        data->transformed = NULL;
    }

    /* Verify cert principals */
    verified = (JSBool)(principals && name && src &&
               verifyPrincipals(decoder, containerPrincipals, principals, name,
                                src, XP_STRLEN(src), (JSBool)(implicitName != NULL)));

    XP_FREEIF(untransformed);
    src = NULL;
    XP_FREEIF(implicitName);
    name = NULL;

    /*
     * Now that we've attempted verification, we need to set the appropriate
     * level of signedness based on whether verification succeeded.
     * We avoid setting signedness if principals is the same as container
     * principals (i.e., we "inherited" the principals from a script earlier
     * in the page) and we are not in a subcontainer of the container where
     * the principals were found. In that case we will create a new set of
     * principals for the inner container.
     */
    if (data && !(principals == containerPrincipals && container != inner)) {
        data->signedness = verified ? HAS_SIGNED_SCRIPTS : HAS_UNSIGNED_SCRIPTS;
    }

    if (verified && decoder->early_access_list &&
        !checkEarlyAccess(decoder, principals))
    {
        return NULL;
    }

    if (!verified) {
        /* No cert principals; try codebase principal */
        if (principals == NULL || principals == containerPrincipals) {
            if (container == inner ||
                containerData->signedness == HAS_UNSIGNED_SCRIPTS)
            {
                principals = containerPrincipals;
                data = (JSPrincipalsData *) principals;
            } else {
                /* Just put restricted principals in inner */
                principals = LM_NewJSPrincipals(NULL, NULL,
                                                containerPrincipals->codebase);
                if (principals == NULL) {
                    JS_ReportOutOfMemory(cx);
                    return NULL;
                }
                data = (JSPrincipalsData *) principals;
            }
        }
        lm_InvalidateCertPrincipals(decoder, principals);

        if (decoder->early_access_list && !lm_GetCrossOriginEnabled() &&
            !checkEarlyAccess(decoder, principals))
        {
            return NULL;
        }

        if (container == inner) {
            lm_InvalidateCertPrincipals(decoder, containerPrincipals);

            /* compare codebase principals */
            if (!sameOrigins(cx, containerPrincipals->codebase,
                             principals->codebase))
            {
                /* Codebases don't match; evaluate under different
                   principals than container */
                return principals;
            }
            /* Codebases match */
            return containerPrincipals;
        }

        /* Just put restricted principals in inner */
        lm_SetContainerPrincipals(cx, inner, principals);
        return principals;
    }

    if (!principalsEqual(cx, principals, containerPrincipals)) {
        /* We have two unequal sets of principals. */
        if (containerData->signedness == HAS_NO_SCRIPTS &&
            sameOrigins(cx, principals->codebase,
                        containerPrincipals->codebase))
        {
            /*
             * Principals are unequal because we have container principals
             * carrying only a codebase, and the principals of this script
             * that carry cert principals as well.
             */
            lm_SetContainerPrincipals(cx, container, principals);
            return principals;
        }
        if (inner == container) {
            /*
             * Intersect principals and container principals,
             * modifying the container principals.
             */
            PrintToConsole("Intersecting principals ");
            printPrincipalsToConsole(cx, containerPrincipals);
            PrintToConsole("with ");
            printPrincipalsToConsole(cx, principals);
            if (!intersectPrincipals(decoder, containerPrincipals,
                                     principals))
            {
                return NULL;
            }
            PrintToConsole("yielding ");
            printPrincipalsToConsole(cx, containerPrincipals);
        } else {
            /*
             * Store the disjoint set of principals in the
             * innermost container
             */
            lm_SetContainerPrincipals(cx, inner, principals);
            return principals;
        }

    }
    return containerPrincipals;
}

