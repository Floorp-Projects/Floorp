/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "prclist.h"
#include "plhash.h"
#include "prmem.h"
#include "shist.h"
#include "jsapi.h"
#include "jsdbgapi.h"
#include "jscntxt.h"  /* XXX - needed for charSetName */
#include "nsZip.h"
#include "zig.h"
#include "nsLoadZig.h"
#include "nsCaps.h"
#include "jri.h"
#ifdef JAVA_OR_OJI
#include "jsjava.h"
#endif
#ifdef JAVA
#include "java.h"
#endif
#include "jsobj.h"
#include "jsatom.h"
#include "jsscope.h"

#ifdef OJI
#include "jvmmgr.h"
#endif
#include "nsCaps.h"

extern JRIEnv * LJ_JSJ_CurrentEnv(JSContext * cx);
extern char *LJ_GetAppletScriptOrigin(JRIEnv *env);

char lm_unknown_origin_str[] = "[unknown origin]";

static char file_url_prefix[]    = "file:";
static char access_error_message[] =
    "access disallowed from scripts at %s to documents at another domain";
static char container_error_message[] =
    "script at '%s' is not signed by sufficient principals to access "
    "signed container";
static char enablePrivilegeStr[] =      "enablePrivilege";
static char isPrivilegeEnabledStr[] =   "isPrivilegeEnabled";
static char disablePrivilegeStr[] =     "disablePrivilege";
static char revertPrivilegeStr[] =      "revertPrivilege";

#define FILE_URL_PREFIX_LEN     (sizeof file_url_prefix - 1)
#define WYSIWYG_TYPE_LEN        10      /* wysiwyg:// */

/* XXX: raman: We should set this when JS console is ready */
PRBool lm_console_is_ready = PR_FALSE;

static void lm_PrintToConsole(const char *data);
static void setupJSCapsCallbacks();

/* XXX what about the PREXTERN? */
typedef PRBool
(*nsCapsFn)(void* context, struct nsTarget *target, PRInt32 callerDepth);

static JSBool
callCapsCode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval, nsCapsFn fn, char *name)
{
    JSString *str;
    char *cstr;
    struct nsTarget *target;

    if (argc == 0 || !JSVAL_IS_STRING(argv[0])) {
        JS_ReportError(cx, "String argument expected for %s.", name);
        return JS_FALSE;
    }
    /*
     * We don't want to use JS_ValueToString because we want to be able
     * to have an object to represent a target in subsequent versions.
     * XXX but then use of an object will cause errors here....
     */
    str = JSVAL_TO_STRING(argv[0]);
    if (!str)
        return JS_FALSE;

    cstr = JS_GetStringBytes(str);
    if (cstr == NULL)
        return JS_FALSE;

    target = nsCapsFindTarget(cstr);
    if (target == NULL)
        return JS_FALSE;
    /* stack depth of 1: first frame is for the native function called */
    if (!(*fn)(cx, target, 1)) {
        /* XXX report error, later, throw exception */
        return JS_FALSE;
    }
    return JS_TRUE;
}


JSBool
lm_netscape_security_isPrivilegeEnabled(JSContext *cx, JSObject *obj, uintN argc,
                                        jsval *argv, jsval *rval)
{
    return callCapsCode(cx, obj, argc, argv, rval, nsCapsIsPrivilegeEnabled,
                        isPrivilegeEnabledStr);
}

JSBool
lm_netscape_security_enablePrivilege(JSContext *cx, JSObject *obj, uintN argc,
                                     jsval *argv, jsval *rval)
{
    return callCapsCode(cx, obj, argc, argv, rval, nsCapsEnablePrivilege,
                        enablePrivilegeStr);
}

JSBool
lm_netscape_security_disablePrivilege(JSContext *cx, JSObject *obj, uintN argc,
                                      jsval *argv, jsval *rval)
{
    return callCapsCode(cx, obj, argc, argv, rval, nsCapsDisablePrivilege,
                        disablePrivilegeStr);
}

JSBool
lm_netscape_security_revertPrivilege(JSContext *cx, JSObject *obj, uintN argc,
                                     jsval *argv, jsval *rval)
{
    return callCapsCode(cx, obj, argc, argv, rval, nsCapsRevertPrivilege,
                        revertPrivilegeStr);
}

static JSFunctionSpec PrivilegeManager_static_methods[] = {
    { isPrivilegeEnabledStr, lm_netscape_security_isPrivilegeEnabled,   1},
    { enablePrivilegeStr,    lm_netscape_security_enablePrivilege,      1},
    { disablePrivilegeStr,   lm_netscape_security_disablePrivilege,     1},
    { revertPrivilegeStr,    lm_netscape_security_revertPrivilege,      1},
    {0}
};

JSBool
lm_InitSecurity(MochaDecoder *decoder)
{
    JSContext  *cx;
    JSObject   *obj;
    JSObject   *proto;
    JSClass    *objectClass;
    jsval      v;
    JSObject   *securityObj;

    /*
     * "Steal" calls to netscape.security.PrivilegeManager.enablePrivilege,
     * et. al. so that code that worked with 4.0 can still work.
     */

    /*
     * Find Object.prototype's class by walking up the window object's
     * prototype chain.
     */
    cx = decoder->js_context;
    obj = decoder->window_object;
    while (proto = JS_GetPrototype(cx, obj))
        obj = proto;
    objectClass = JS_GetClass(cx, obj);

    if (!JS_GetProperty(cx, decoder->window_object, "netscape", &v))
        return JS_FALSE;
    if (JSVAL_IS_OBJECT(v)) {
        /*
         * "netscape" property of window object exists; must be LiveConnect
         * package. Get the "security" property.
         */
        obj = JSVAL_TO_OBJECT(v);
        if (!JS_GetProperty(cx, obj, "security", &v) || !JSVAL_IS_OBJECT(v))
            return JS_FALSE;
        securityObj = JSVAL_TO_OBJECT(v);
    } else {
        /* define netscape.security object */
        obj = JS_DefineObject(cx, decoder->window_object, "netscape",
                              objectClass, NULL, 0);
        if (obj == NULL)
            return JS_FALSE;
        securityObj = JS_DefineObject(cx, obj, "security", objectClass,
                                      NULL, 0);
        if (securityObj == NULL)
            return JS_FALSE;
    }

    /* Define PrivilegeManager object with the necessary "static" methods. */
    obj = JS_DefineObject(cx, securityObj, "PrivilegeManager", objectClass,
                          NULL, 0);
    if (obj == NULL)
        return JS_FALSE;

    return JS_DefineFunctions(cx, obj, PrivilegeManager_static_methods);
}


static void
lm_PrintToConsole(const char *data)
{
    if (lm_console_is_ready) {
    /* XXX: raman: We should write to JS console when it is ready */
        /* JS_PrintToConsole(data); */
    } else {
        MWContext* someRandomContext = XP_FindSomeContext();
        FE_Alert(someRandomContext, data);
    }
}

PR_PUBLIC_API(int)
LM_PrintZigError(int status, void *zigPtr, const char *metafile,
                 char *pathname, char *errortext)
{
    ZIG *zig = (ZIG *) zigPtr;
    char* data;
    char* error_fmt = "# Error: %s (%d)\n#\tjar file: %s\n#\tpath:     %s\n";
    char* zig_name = NULL;
    int len;

    XP_ASSERT(errortext);

    if (zig) {
        zig_name = SOB_get_url(zig);
    }

    if (!zig_name) {
        zig_name = "unknown";
    }

    if (!pathname) {
        pathname = "";
    }

    /* Add 16 slop bytes */
    len = strlen(error_fmt) + strlen(zig_name) + strlen(pathname) +
          strlen(errortext) + 32;

    if ((data = malloc(len)) == 0) {
        return 0;
    }
    sprintf(data, error_fmt, errortext, status, zig_name, pathname);

    lm_PrintToConsole(data);
    XP_FREE(data);

    return 0;
}

PR_PUBLIC_API(char *)
LM_LoadFromZipFile(void *zip, char *fn)
{
    struct stat st;
    char* data;

    if (!ns_zip_stat((ns_zip_t *)zip, fn, &st)) {
        return NULL;
    }
    if ((data = malloc((size_t)st.st_size + 1)) == 0) {
        return NULL;
    }
    if (!ns_zip_get((ns_zip_t *)zip, fn, data, st.st_size)) {
        XP_FREE(data);
        return NULL;
    }
    data[st.st_size] = '\0';
    return data;
}


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
          case MOCHA_TYPE_URL:
            /* This type cannot name the true origin (server) of JS code. */
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
        XP_STRNCASECMP(cmp2, file_url_prefix, FILE_URL_PREFIX_LEN) == 0) {
        ok = JS_TRUE;
        goto done;
    }
    ok = (JSBool)(cmp1 && cmp2 && XP_STRCMP(cmp1, cmp2) == 0);

done:
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
    JSStackFrame *pFrameToStartLooking = JVM_GetStartJSFrameFromParallelStack();
    JSStackFrame *pFrameToEndLooking   = JVM_GetEndJSFrameFromParallelStack(pFrameToStartLooking);

    fp = pFrameToStartLooking;
    while ((fp = JS_FrameIterator(cx, &fp)) != pFrameToEndLooking) {
        script = JS_GetFrameScript(cx, fp);
        if (script) {
            return JS_GetScriptPrincipals(cx, script);
        }
    }
#ifdef OJI
        return JVM_GetJavaPrincipalsFromStack(pFrameToStartLooking);
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
    MochaDecoder *running;
#ifdef JAVA
    JRIEnv *env;
    char *str;
#endif

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
} SharedZig;

static SharedZig *
newSharedZig(ns_zip_t *zip)
{
    ZIG *zig;
    SharedZig *result;

    zig = nsInitializeZig(zip,
                          (int (*) (int, ZIG *, const char *,
                                    char *, char *)) LM_PrintZigError);
    if (zig == NULL)
        return NULL;

    result = (SharedZig *) XP_ALLOC(sizeof(SharedZig));
    if (result == NULL) {
        SOB_destroy(zig);
        return NULL;
    }
    result->zig = zig;
    result->refCount = 0;
    return result;
}

static void
destroySharedZig(SharedZig *sharedZig)
{
    SOB_destroy(sharedZig->zig);
    XP_FREE(sharedZig);
}

static SharedZig *
holdZig(SharedZig *sharedZig)
{
    if (sharedZig) {
        XP_ASSERT(sharedZig->refCount >= 0);
        /* XXX: Why are you checking this again */
        if (sharedZig)
            sharedZig->refCount++;
    }
    return sharedZig;
}

static void
dropZig(SharedZig *sharedZig)
{
    if (sharedZig) {
        XP_ASSERT(sharedZig->refCount > 0);
        if (--sharedZig->refCount == 0) {
            destroySharedZig(sharedZig);
        }
    }
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
    ns_zip_t *zip;
    uint32 externalCapturePrincipalsCount;
    char *untransformed;
    char *transformed;
    JSBool needUnlock;
    char *codebaseBeforeSettingDomain;
#ifdef DEBUG_norris
    int serial;
#endif
    enum Signedness signedness;
    void *pNSISecurityContext;
} JSPrincipalsData;

PR_STATIC_CALLBACK(void)
destroyJSPrincipals(JSContext *cx, JSPrincipals *principals);

static JSBool
principalsCanAccessTarget(JSContext *cx, JSTarget target);

PR_STATIC_CALLBACK(void *)
getPrincipalArray(JSContext *cx, struct JSPrincipals *principals);

PR_STATIC_CALLBACK(JSBool)
globalPrivilegesEnabled(JSContext *cx, JSPrincipals *principals);

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
    ns_zip_t *zip = NULL;

    setupJSCapsCallbacks();

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
             * Unfortunately, ns_zip_open wants a Unix-style name. Convert
             * Mac path to a Unix-style path. This code is copied from
             * appletStubs.c.
             */
            OSErr ConvertMacPathToUnixPath(const char *macPath, char **unixPath);
            char *unixPath = NULL;

            if (ConvertMacPathToUnixPath(fn, &unixPath) == 0) {
                zip = ns_zip_open(unixPath);
            }
            XP_FREEIF(unixPath);
#else
            zip = ns_zip_open(fn);
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

#ifdef DEBUG_norris
        XP_TRACE(("JSPrincipals #%.4d released\n", data->serial));
#endif
        XP_FREEIF(principals->codebase);
        if (data->sharedZig) {
            dropZig(data->sharedZig);
        }
        if (data->principalsArrayRef != NULL) {
            /* XXX: raman: Should we free up the principals that are in that array also? */
            nsCapsFreePrincipalArray(data->principalsArrayRef);
        }
        XP_FREEIF(data->name);
        XP_FREEIF(data->untransformed);
        XP_FREEIF(data->transformed);
        if (data->zip)
            ns_zip_close(data->zip);
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
    void *principalsArray;
    struct nsPrincipal *principal;
    const char *vendor;
    uint32 i, count;
    static char emptyStr[] = "<empty>\n";

    principalsArray = principals->getPrincipalArray(cx, principals);

    if (principalsArray == NULL) {
        lm_PrintToConsole(emptyStr);
        return;
    }

    lm_PrintToConsole("[\n");
    count = nsCapsGetPrincipalArraySize(principalsArray);
    for (i = 0; i < count; i++) {
        principal = nsCapsGetPrincipalArrayElement(principalsArray, i);
        vendor = nsCapsPrincipalGetVendor(principal);
        if (vendor == NULL) {
            JS_ReportOutOfMemory(cx);
            return;
        }
        lm_PrintToConsole(vendor);
        lm_PrintToConsole(",\n");
    }
    lm_PrintToConsole("]\n");
}

extern void
lm_InvalidateCertPrincipals(MochaDecoder *decoder, JSPrincipals *principals)
{
    JSPrincipalsData *data = (JSPrincipalsData *) principals;

    if (data->principalsArrayRef) {
        lm_PrintToConsole("Invalidating certificate principals in ");
        printPrincipalsToConsole(decoder->js_context, principals);
        nsCapsFreePrincipalArray(data->principalsArrayRef);
        data->principalsArrayRef = NULL;
    }
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
    if (data->principalsArrayRef != NULL) {
        nsCapsFreePrincipalArray(data->principalsArrayRef);
        data->principalsArrayRef = NULL;
    }
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
    if (from == NULL || to == NULL) {
        return JS_FALSE;
    }
    return (from == to) || (JSBool)nsCapsCanExtendTrust(from, to);
}

static JSPrincipals *
newJSPrincipalsFromArray(JSContext *cx, void *principalsArray, void *pNSISecurityContext);

extern JSBool
lm_CheckContainerAccess(JSContext *cx, JSObject *obj, MochaDecoder *decoder,
                        JSTarget target)
{
    JSPrincipals *principals;
    JSPrincipalsData *data;
    JSStackFrame *fp;
    JSScript *script;
    JSPrincipals *subjPrincipals;
    JSPrincipalsList *list;
    const char *fn;

    if(decoder->principals)  {
        /* The decoder's js_context isn't in a request, so we should put it
         *   in one during this call. */
        JS_BeginRequest(decoder->js_context);
        principals = lm_GetInnermostPrincipals(decoder->js_context, obj, NULL);
        JS_EndRequest(decoder->js_context);
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
            lm_PrintToConsole("Principals of script: ");
            printPrincipalsToConsole(cx, subjPrincipals);
            lm_PrintToConsole("Principals of signed container: ");
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
    void *principalArray;
    void *newPrincipalArray;

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
    /* Compute the intersection. */
    principalArray = getPrincipalArray(cx, principals);
    newPrincipalArray = getPrincipalArray(cx, newPrincipals);
    if (principalArray == NULL
        || newPrincipalArray == NULL)
    {
        lm_InvalidateCertPrincipals(decoder, principals);
        return JS_TRUE;
    }

    principalArray = nsCapsIntersectPrincipalArray(
        principalArray, newPrincipalArray);

    if (principalArray == NULL) {
        lm_InvalidateCertPrincipals(decoder, principals);
        return JS_TRUE;
    }

    data->principalsArrayRef = principalArray;
    return JS_TRUE;
}

static uint32
getPrincipalsCount(JSContext *cx, JSPrincipals *principals)
{
    void *principalArray;

    /* Get array of principals */
    principalArray = getPrincipalArray(cx, principals);

    return principalArray ? nsCapsGetPrincipalArraySize(principalArray) : 0;
}

static JSBool
principalsEqual(JSContext *cx, JSPrincipals *a, JSPrincipals *b)
{
    JSPrincipalsData *dataA, *dataB;
    void *arrayA;
    void *arrayB;

    if (a == b)
        return JS_TRUE;

    dataA = (JSPrincipalsData *) a;
    dataB = (JSPrincipalsData *) b;

    if (dataA->signedness != dataB->signedness)
        return JS_FALSE;

    arrayA = getPrincipalArray(cx, a);
    arrayB = getPrincipalArray(cx, b);

    return (JSBool)(nsCapsComparePrincipalArray(arrayA, arrayB)
                == nsSetComparisonType_Equal);
}

/*
 * createPrincipalsArray takes ZIG file information and returns a
 * pointer to an array of nsPrincipal objects.
 * It also registers the principals with the nsPrivilegeManager.
 */
static jref
createPrincipalsArray(JSPrincipals *principals)
{
    JSPrincipalsData *data = (JSPrincipalsData *) principals;
    JSBool hasCodebase;
    SOBITEM *item;
    int i;
    ZIG *zig;
    unsigned count;
    void *result;
    struct nsPrincipal *principal;
    ZIG_Context * zig_cx = NULL;

    setupJSCapsCallbacks();

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
    result = nsCapsNewPrincipalArray(count);
    if (result == NULL) {
        return NULL;
    }

    if (zig && ((zig_cx = SOB_find(zig, data->name, ZIG_SIGN)) == NULL)) {
        return NULL;
    }

    i = 0;
    while (zig && SOB_find_next(zig_cx, &item) >= 0) {
        FINGERZIG *fingPrint;

        fingPrint = (FINGERZIG *) item->data;

        /* create a  new nsPrincipal(CERT_KEY, fingPrint->key) */
        principal = nsCapsNewPrincipal(nsPrincipalType_CertKey,
                                       fingPrint->key,
                                       fingPrint->length,
                                       zig);
        nsCapsRegisterPrincipal(principal);
        nsCapsSetPrincipalArrayElement(result, i++, principal);
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
        principal = nsCapsNewPrincipal(nsPrincipalType_CodebaseExact,
                                       javaCodebase,
                                       XP_STRLEN(javaCodebase),
                                       NULL);
        nsCapsRegisterPrincipal(principal);
        nsCapsSetPrincipalArrayElement(result, i++, principal);
        XP_FREE(javaCodebase);
    }

    data->principalsArrayRef = result;

    return result;
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

	if ((nsCapsGetRegistrationModeFlag()) && principals && 
        (NET_URL_Type(principals->codebase) == FILE_TYPE_URL))
		return JS_TRUE;

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
    "AccountSetup",
    /* See Target.java for more targets */
};

int
findTarget(const char *target)
{ 
   int i=0;
   for(i=0; i<JSTARGET_MAX; i++)
   {
      if (XP_STRCMP(target, targetStrings[i]) == 0)
      {
         return i;
      }
   }
   return -1;
}
/*
** Exported entry point to support nsISecurityContext::Implies method.
*/
JSBool
LM_CanAccessTargetStr(JSContext *cx, const char *target)
{
    int      intTarget = findTarget(target);
    JSTarget jsTarget;
    if(intTarget < 0)
    {
      return PR_FALSE;
    }
    jsTarget = (JSTarget)intTarget;
    return lm_CanAccessTarget(cx, jsTarget);
}


/*
 * If given principals can access the given target, return true. Otherwise
 * return false.  The script must already have explicitly requested access
 * to the given target.
 */
static JSBool
principalsCanAccessTarget(JSContext *cx, JSTarget target)
{
    struct nsPrivilegeTable *annotation;
    struct nsPrivilege *privilege;
    struct nsTarget *capsTarget;
    nsPermissionState perm;
    JSStackFrame *fp;
    void *annotationRef;
    void *principalArray = NULL;
    JSStackFrame *pFrameToStartLooking = JVM_GetStartJSFrameFromParallelStack();
    JSStackFrame *pFrameToEndLooking   = JVM_GetEndJSFrameFromParallelStack(pFrameToStartLooking);

    setupJSCapsCallbacks();

    /* Map JSTarget to nsTarget */
    XP_ASSERT(target >= 0);
    XP_ASSERT(target < sizeof(targetStrings)/sizeof(targetStrings[0]));
    capsTarget = nsCapsFindTarget(targetStrings[target]);

    /* Find annotation */
    annotationRef = NULL;
    principalArray = NULL;
    fp = pFrameToStartLooking;
    while ((fp = JS_FrameIterator(cx, &fp)) != pFrameToEndLooking) {
        void *current;
        if (JS_GetFrameScript(cx, fp) == NULL)
            continue;
        current = JS_GetFramePrincipalArray(cx, fp);
        if (current == NULL) {
            return JS_FALSE;
        }
        annotationRef = (void *) JS_GetFrameAnnotation(cx, fp);
        if (annotationRef) {
            if (principalArray &&
                !nsCapsCanExtendTrust(current, principalArray))
            {
                return JS_FALSE;
            }
            break;
        }
        principalArray = principalArray
            ? nsCapsIntersectPrincipalArray(principalArray, current)
            : current;
    }

    if (annotationRef) {
        annotation = (struct nsPrivilegeTable *)annotationRef;
    } else {
#ifdef OJI
         /*
          * Call from Java into JS. Just call the Java routine for checking
          * privileges.
          */
          if (principalArray) {
              /*
               * Must check that the principals that signed the Java applet are
               * a subset of the principals that signed this script.
               */
              void *javaPrincipals = JVM_GetJavaPrincipalsFromStack(pFrameToStartLooking);

              if (!canExtendTrust(cx, javaPrincipals, principalArray)) {
                  return JS_FALSE;
              }
          }
         /*
          * XXX sudu: TODO: Setup the parameters representing a target.
          */
          return JVM_NSISecurityContextImplies(pFrameToStartLooking, targetStrings[target], NULL);
#endif /* JAVA */
        /* No annotation in stack */
        return JS_FALSE;
    }

   /* Now find permission for (annotation, target) pair. */
    privilege = nsCapsGetPrivilege(annotation, capsTarget);
    if (privilege == NULL) {
        return JS_FALSE;
    }
    XP_ASSERT(privilege);
    perm = nsCapsGetPermission(privilege);

    return (JSBool)(perm == nsPermissionState_Allowed);
}


PR_STATIC_CALLBACK(void *)
getPrincipalArray(JSContext *cx, struct JSPrincipals *principals)
{
    JSPrincipalsData *data = (JSPrincipalsData *) principals;

    /* Get array of principals */

    if (data->principalsArrayRef == NULL) {
        if (createPrincipalsArray(principals) == NULL)
            return NULL;
    }

    return data->principalsArrayRef;
}


extern char *
LM_ExtractFromPrincipalsArchive(JSPrincipals *principals, char *name,
                                uint *length)
{
    JSPrincipalsData *data = (JSPrincipalsData *) principals;
    char *result = NULL;

    result = LM_LoadFromZipFile(data->zip, name);
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
LM_GetJSPrincipalsFromJavaCaller(JSContext *cx, void *principalsArray, void *pNSISecurityContext)
{
    setupJSCapsCallbacks();
    if (principalsArray == NULL)
        return NULL;

    return newJSPrincipalsFromArray(cx, principalsArray, pNSISecurityContext);
}

static JSPrincipals *
newJSPrincipalsFromArray(JSContext *cx, void *principalsArray, void *pNSISecurityContext)
{
    JSPrincipals *result;
    struct nsPrincipal *principal;
    const char *codebase;
    JSPrincipalsData *data;
    uint32 i, count;

    setupJSCapsCallbacks();

    count = nsCapsGetPrincipalArraySize(principalsArray);
    if (count == 0) {
        JS_ReportError(cx, "No principals found for Java caller");
        return NULL;
    }

    codebase = NULL;
    for (i = count; i > 0; i--) {
        principal = nsCapsGetPrincipalArrayElement(principalsArray, i-1);
        if (nsCapsIsCodebaseExact(principal)) {
            codebase = nsCapsPrincipalToString(principal);
            break;
        }
    }

    result = LM_NewJSPrincipals(NULL, NULL, (char *) codebase);
    if (result == NULL) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }

    data = (JSPrincipalsData *) result;
    data->principalsArrayRef = principalsArray;
    data->pNSISecurityContext = pNSISecurityContext;
    data->signedness = count == 1 && codebase
                       ? HAS_UNSIGNED_SCRIPTS
                       : HAS_SIGNED_SCRIPTS;

    return result;
}

static JSBool
verifyPrincipals(MochaDecoder *decoder, JSPrincipals *containerPrincipals,
                 JSPrincipals *principals, char *name, char *src,
                 uint srcSize, JSBool implicitName)
{
    JSPrincipalsData *data = (JSPrincipalsData *) principals;
    ZIG *zig;
    DIGESTS *dig = NULL;
    JSBool sameName = JS_FALSE;
    int ret;
    JSPrincipalsData *containerData;
    ns_zip_t *containerZip;
    JSBool verified;
    SOBITEM *item;
    ZIG_Context * zig_cx;

    if (data->signedness == HAS_UNSIGNED_SCRIPTS)
        return JS_FALSE;

    containerData = (JSPrincipalsData *) containerPrincipals;

    containerZip =
        (containerData && containerData->signedness != HAS_UNSIGNED_SCRIPTS)
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

    if (containerData == NULL) {
        /* First script seen; initialize zig. */
        data->sharedZig = holdZig(newSharedZig(data->zip));
    } else if (data == containerData) {
        /* Already have a zig if there is one; nothing more to do. */
    } else if (data->zip == NULL) {
        /* "Inherit" data->sharedZig from container data. */
        data->sharedZig = holdZig(containerData->sharedZig);
    } else if (containerData->url_struct &&
               XP_STRCMP(data->url_struct->address,
                         containerData->url_struct->address) == 0)
    {
        /* Two identical zips. Share the zigs. */
        data->sharedZig = holdZig(containerData->sharedZig);
    } else {
        /* Different zips. Must create a new zig. */
        data->sharedZig = holdZig(newSharedZig(data->zip));
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
        LM_PrintZigError(ret, zig, "", name, SOB_get_error(ret));
    }
    if (zig_cx) {
        SOB_find_end(zig_cx);
    }
    return verified;
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
        if (!lm_GetUnsignedExecutionEnabled()) {
            /* Execution of unsigned scripts disabled. Return now. */
            return NULL;
        }
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
            if (containerData->signedness == HAS_NO_SCRIPTS) {
                lm_SetContainerPrincipals(cx, container, principals);
                return principals;
            }
            /*
             * Intersect principals and container principals,
             * modifying the container principals.
             */
            lm_PrintToConsole("Intersecting principals ");
            printPrincipalsToConsole(cx, containerPrincipals);
            lm_PrintToConsole("with ");
            printPrincipalsToConsole(cx, principals);
            if (!intersectPrincipals(decoder, containerPrincipals,
                                     principals))
            {
                return NULL;
            }
            lm_PrintToConsole("yielding ");
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


/*******************************************************************************
 * Glue code for JS stack crawling callbacks
 ******************************************************************************/

typedef struct JSFrameIterator {
    JSStackFrame *fp;
    JSContext *cx;
    JRIEnv *env;
    void *intersect;
    PRBool sawEmptyPrincipals;
} JSFrameIterator;

static JSFrameIterator *
lm_NewJSFrameIterator(void *context)
{
    JSContext *cx = (JSContext *)context;
    JSFrameIterator *result;
    void *array;
    JRIEnv *env = NULL;

    result = XP_ALLOC(sizeof(JSFrameIterator));
    if (result == NULL) {
        return NULL;
    }

    if (cx == NULL) {
        return NULL;
    }

    result->env = env;
    result->fp = NULL;
    result->cx = cx;
    result->fp = JS_FrameIterator(cx, &result->fp);
    array = result->fp
        ? JS_GetFramePrincipalArray(cx, result->fp)
        : NULL;
    result->intersect = array;
    result->sawEmptyPrincipals =
        (result->intersect == NULL && result->fp &&
         JS_GetFrameScript(cx, result->fp))
        ? PR_TRUE : PR_FALSE;

    return result;
}


static PRBool
lm_NextJSJavaFrame(struct JSFrameIterator *iterator)
{
    void *current;
    void *previous;

    if (iterator->fp == 0) {
        return PR_FALSE;
    }

    current = JS_GetFramePrincipalArray(iterator->cx, iterator->fp);
    if (current == NULL) {
        if (JS_GetFrameScript(iterator->cx, iterator->fp))
            iterator->sawEmptyPrincipals = PR_TRUE;
    } else {
        if (iterator->intersect) {
            previous = iterator->intersect;
            current = nsCapsIntersectPrincipalArray(current, previous);
            /* XXX: raman: should we do a free the previous principal Array */
            nsCapsFreePrincipalArray(iterator->intersect);
        }
        iterator->intersect = current;
    }
    iterator->fp = JS_FrameIterator(iterator->cx, &iterator->fp);
    return iterator->fp != NULL;
}

static PRBool
nextJSFrame(struct JSFrameIterator **iteratorp)
{
    JSFrameIterator *iterator = *iteratorp;
    PRBool result = lm_NextJSJavaFrame(iterator);
    if (!result) {
        if (iterator->intersect)
            nsCapsFreePrincipalArray(iterator->intersect);
        XP_FREE(iterator);
        *iteratorp = NULL;
    }
    return result;
}

/*
 *
 *  CALLBACKS to walk the stack
 *
 */

typedef struct NSJSJavaFrameWrapper {
    struct JSFrameIterator *iterator;
} NSJSJavaFrameWrapper;

struct NSJSJavaFrameWrapper *
lm_NewNSJSJavaFrameWrapperCB(void *context)
{
    struct NSJSJavaFrameWrapper *result;

    result = (struct NSJSJavaFrameWrapper *)PR_CALLOC(sizeof(struct NSJSJavaFrameWrapper));
    if (result == NULL) {
        return NULL;
    }

    result->iterator = lm_NewJSFrameIterator(context);
    return result;
}

void lm_FreeNSJSJavaFrameWrapperCB(struct NSJSJavaFrameWrapper *wrapper)
{
    PR_FREEIF(wrapper);
}

void lm_GetStartFrameCB(struct NSJSJavaFrameWrapper *wrapper)
{
}

PRBool lm_IsEndOfFrameCB(struct NSJSJavaFrameWrapper *wrapper)
{
    if ((wrapper == NULL) || (wrapper->iterator == NULL))
        return PR_TRUE;
    return PR_FALSE;
}

PRBool lm_IsValidFrameCB(struct NSJSJavaFrameWrapper *wrapper)
{
    return (wrapper->iterator != NULL);
}

void *lm_GetNextFrameCB(struct NSJSJavaFrameWrapper *wrapper, int *depth)
{
    if ((wrapper->iterator == NULL) ||
        (!nextJSFrame(&(wrapper->iterator)))) {
        return NULL;
    }
    (*depth)++;
    return wrapper->iterator;
}

void * lm_GetPrincipalArrayCB(struct NSJSJavaFrameWrapper *wrapper)
{
    JSFrameIterator *iterator;
    if (wrapper->iterator == NULL)
        return NULL;
    iterator = wrapper->iterator;
    return JS_GetFramePrincipalArray(iterator->cx, iterator->fp);
}

void * lm_GetAnnotationCB(struct NSJSJavaFrameWrapper *wrapper)
{
    JSFrameIterator *iterator;
    void *annotaion;
    void *current;

    if (wrapper->iterator == NULL) {
        return NULL;
    }
    iterator = wrapper->iterator;

    annotaion = JS_GetFrameAnnotation(iterator->cx, iterator->fp);
    if (annotaion == NULL)
        return NULL;

    current = JS_GetFramePrincipalArray(iterator->cx, iterator->fp);

    if (iterator->sawEmptyPrincipals || current == NULL ||
        (iterator->intersect &&
         !canExtendTrust(iterator->cx, current, iterator->intersect)))
        return NULL;

    return annotaion;
}

void * lm_SetAnnotationCB(struct NSJSJavaFrameWrapper *wrapper, void *privTable)
{
    if (wrapper->iterator) {
        JSFrameIterator *iterator = wrapper->iterator;
        JS_SetFrameAnnotation(iterator->cx, iterator->fp, privTable);
    }
    return privTable;
}

/* End of Callbacks */

static PRBool privManagerInited = PR_FALSE;

static void
setupJSCapsCallbacks()
{
    if (privManagerInited)
        return;
    privManagerInited = TRUE;

    nsCapsInitialize();
    setNewNSJSJavaFrameWrapperCallback(lm_NewNSJSJavaFrameWrapperCB);
    setFreeNSJSJavaFrameWrapperCallback(lm_FreeNSJSJavaFrameWrapperCB);
    setGetStartFrameCallback(lm_GetStartFrameCB);
    setIsEndOfFrameCallback(lm_IsEndOfFrameCB);
    setIsValidFrameCallback(lm_IsValidFrameCB);
    setGetNextFrameCallback(lm_GetNextFrameCB);
    setOJIGetPrincipalArrayCallback(lm_GetPrincipalArrayCB);
    setOJIGetAnnotationCallback(lm_GetAnnotationCB);
    setOJISetAnnotationCallback(lm_SetAnnotationCB);
}
