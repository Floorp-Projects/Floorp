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
 * JS in the Navigator top-level stuff.
 *
 * Brendan Eich, 9/8/95
 */
#include "lm.h"
#include "xp.h"
#include "net.h"
#include "structs.h"
#include "layout.h"     /* for lo_FormData */
#include "pa_tags.h"    /* included via -I../libparse */
#include "prmem.h"
#include "prthread.h"
#include "prmon.h"
#ifdef XP_MAC
#include "pprthred.h"   /* for PR_CreateThreadGCAble */
#else
#include "private/pprthred.h"
#endif

#include "jri.h"
#include "jriext.h"
#ifdef JAVA
#include "java.h"
#endif
#include "prefapi.h"
#include "libi18n.h"
#include "intl_csi.h"

#define UNICODE /* remove after working everywhere */

extern PRThread *mozilla_thread;
extern QueueStackElement    *et_TopQueue;

char js_language_name[]  = "JavaScript";
char js_content_type[]   = APPLICATION_JAVASCRIPT;

char lm_argc_err_str[]   = "incorrect number of arguments";

char lm_onLoad_str[]        = PARAM_ONLOAD;
char lm_onUnload_str[]      = PARAM_ONUNLOAD;
char lm_onAbort_str[]       = PARAM_ONABORT;
char lm_onError_str[]       = PARAM_ONERROR;
char lm_onScroll_str[]      = PARAM_ONSCROLL;
char lm_onFocus_str[]       = PARAM_ONFOCUS;
char lm_onBlur_str[]        = PARAM_ONBLUR;
char lm_onSelect_str[]      = PARAM_ONSELECT;
char lm_onChange_str[]      = PARAM_ONCHANGE;
char lm_onReset_str[]       = PARAM_ONRESET;
char lm_onSubmit_str[]      = PARAM_ONSUBMIT;
char lm_onClick_str[]       = PARAM_ONCLICK;
char lm_onMouseDown_str[]   = PARAM_ONMOUSEDOWN;
char lm_onMouseOver_str[]   = PARAM_ONMOUSEOVER;
char lm_onMouseOut_str[]    = PARAM_ONMOUSEOUT;
char lm_onMouseUp_str[]     = PARAM_ONMOUSEUP;
char lm_onLocate_str[]      = PARAM_ONLOCATE;
char lm_onHelp_str[]        = PARAM_ONHELP;

char lm_focus_str[]         = "focus";
char lm_blur_str[]          = "blur";
char lm_select_str[]        = "select";
char lm_click_str[]         = "click";
char lm_scroll_str[]        = "scroll";
char lm_enable_str[]        = "enable";
char lm_disable_str[]       = "disable";

char lm_toString_str[]      = "toString";
char lm_length_str[]        = "length";
char lm_document_str[]      = "document";
char lm_forms_str[]         = "forms";
char lm_links_str[]         = "links";
char lm_anchors_str[]       = "anchors";
char lm_plugins_str[]       = "plugins";
char lm_applets_str[]       = "applets";
char lm_embeds_str[]        = "embeds";
char lm_images_str[]        = "images";
char lm_layers_str[]        = "layers";
#ifdef DOM
char lm_spans_str[]			= "spans";
char lm_transclusions_str[] = "transclusions";
#endif
char lm_location_str[]      = "location";
char lm_navigator_str[]     = "navigator";
char lm_netcaster_str[]     = "netcaster";
char lm_components_str[]    = "components";

char lm_parentLayer_str[]   = "parentLayer";
char lm_opener_str[]        = "opener";
char lm_closed_str[]        = "closed";
char lm_assign_str[]        = "assign";
char lm_reload_str[]        = "reload";
char lm_replace_str[]       = "replace";
char lm_event_str[]         = "event";
char lm_methodPrefix_str[]  = "#method";
char lm_methodArgc_str[]    = "#margc";
char lm_methodArgv_str[]    = "#margv";
char lm_getPrefix_str[]     = "#get_";
char lm_setPrefix_str[]     = "#set_";
char lm_typePrefix_str[]    = "#type_";
const char *lm_event_argv[] = {lm_event_str};

JSRuntime       *lm_runtime;
static uint32   lm_max_gc_bytes = 4L * 1024L * 1024L;   /* XXX use a pref */

MochaDecoder    *lm_crippled_decoder;
JSContext       *lm_crippled_context;   /* exported to jsStubs.c */

PRThread        *lm_InterpretThread;    /* interpreter now in its own thread */
JRIEnv          *lm_JSEnv;              /* Java env for lm_InterpretThread */
PREventQueue    *lm_InterpretQueue;     /* for "normal" event messages */
PREventQueue    *lm_PriorityQueue;      /* for stop and death messages */

/*
 * This is the big lock that ensures serialization of mocha execution.
 * It must be acquired before executing any mocha code in order to
 * preserve run-to-completion semantics.
 */
static PRThread *lm_owner;        /* the only thread running mocha */
PRMonitor *lm_owner_mon = NULL;   /* lock protecting lm_owner */
static MWContext *lm_owner_context;
static int32      lm_owner_count;

#ifdef JAVA
extern LJ_JSJ_Init(void);
#endif

static JSClass lm_dummy_class = {
    "Dummy", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};
JSObject *lm_DummyObject;

PR_STATIC_CALLBACK(JSBool)
lm_alert(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    MWContext *context;
    JSString *arg;

    decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv);
    if (!decoder)
        return JS_TRUE;
    if (!(arg = JS_ValueToString(cx, argv[0])))
        return JS_FALSE;
    context = decoder->window_context;
    if (context) {
	char * message;
	message = lm_StrToLocalEncoding(context, arg);
        ET_PostMessageBox(context, message, JS_FALSE);
	XP_FREEIF(message);
    }
    return JS_TRUE;
}

#ifdef DEBUG

#include "jscntxt.h"

PR_STATIC_CALLBACK(JSBool)
lm_tracing(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    JSBool bval;

    if (argc == 0) {
        *rval = BOOLEAN_TO_JSVAL(cx->tracefp != 0);
        return JS_TRUE;
    }

    if (JSVAL_IS_INT(argv[0])) {
        if (JSVAL_TO_INT(argv[0]) != 0)
            bval = JS_TRUE;
    } else if (JSVAL_IS_BOOLEAN(argv[0])) {
        bval = JSVAL_TO_BOOLEAN(argv[0]);
    } else {
        return JS_TRUE; /* XXX should be error */
    }
    if (cx->tracefp)
        fclose(cx->tracefp);
    cx->tracefp = bval ? fopen("/dev/tty", "w") : 0;
    return JS_TRUE;
}

#endif /* DEBUG */

static JSFunctionSpec lm_global_functions[] = {
    {"alert",           lm_alert,       0},
#ifdef DEBUG
    {"tracing",         lm_tracing,     0},
#endif
    {0}
};

/*
 * If we don't get passed an object, assume we are supposed to use
 *   the main window object
 */
JSBool
lm_CompileEventHandler(MochaDecoder * decoder, PA_Block id, PA_Block all, 
                       int newline_count, JSObject *object,
                       const char *name, PA_Block block)
{
    JSPrincipals *principals, *registered;
    JSContext *cx;
    char *body, *p;
    JSBool ok;
    JSString *unicode;

    cx = decoder->js_context;
    if (object == NULL)
        object = decoder->window_object;

    principals = lm_GetCompilationPrincipals(decoder, NULL);
    if (principals == NULL)
        return JS_FALSE;

    JSPRINCIPALS_HOLD(cx, principals);

    body = (char *) block;

    /* Find block in all and subtract newlines in and after block
           from newline_count */
    if (all) {
        /* 
         * XXX - We really should fix it so that "all" is always
         * nonnull.
         */
        for (p=(char *) all; *p; p++) {
            if (*p == '\r' || *p == '\n') {
                p = XP_STRSTR((char *) all, body);
                if (p == NULL)
                    break;
                /* 
                 * XXX - doesn't handle case where there are two 
                 * handlers with the same text
                 */
                while (*p) {
                    switch (*p) {
                    case '\r':
                        if (p[1] == '\n')
                            p++;
                        /* fall through */
                    case '\n':
                        newline_count--;
                        break;
                    default:
                        break;
                    }
                    p++;
                }
                break;
            }
        }
    }

    registered = LM_RegisterPrincipals(decoder, principals, 
                                       (char *) id, (char *) all);

    unicode = lm_LocalEncodingToStr(decoder->window_context, body);
    JS_LockGCThing(cx, unicode);
    ok = (JSBool)(registered && 
         JS_CompileUCFunctionForPrincipals(cx, object,
            registered,
            name, 1, lm_event_argv,
            JS_GetStringChars(unicode), JS_GetStringLength(unicode), 
            LM_GetSourceURL(decoder),
            newline_count + 1)
         != NULL);

    JSPRINCIPALS_DROP(cx, principals);
    JS_UnlockGCThing(cx, unicode);
    return ok;
}

#define INTERRUPT_BRANCH_COUNT_MASK     255
#define MAYBE_GC_BRANCH_COUNT_MASK      4095

#ifdef XP_UNIX
static uint32 lm_handling_event;
#endif

JSBool PR_CALLBACK
lm_BranchCallback(JSContext *cx, JSScript *script)
{
    static uint32 count = 0;

    /*
     * If we have been running for a while yield and see if anyone
     *   else is waiting for a time slice or is trying to stop us.
     * At a much less frequent interval, force a GC to catch any garbage
     *   created by long-running or long-resident scripts.
     */
    if ((++count & INTERRUPT_BRANCH_COUNT_MASK) == 0) {
        MochaDecoder *decoder;
	MWContext *context;

        PR_Sleep(PR_INTERVAL_NO_WAIT);

        /* check to see if we've been stopped */
        decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
	context = decoder->window_context;
        if (!ET_ContinueProcessing(context)) {
#ifdef DEBUG_chouck
            XP_TRACE(("Interrupted in branch callback"));
#endif
            return JS_FALSE;
        }

        /* if not stopped, go look for garbage */
        if ((count & MAYBE_GC_BRANCH_COUNT_MASK) == 0) {
#ifdef XP_UNIX
	    /* X lacks an idle loop from which to do this opportunistically. */
	    if (lm_handling_event && context)
		ET_moz_CallFunction((ETVoidPtrFunc)FE_UpdateStopState, context);
#endif /* XP_UNIX */

            JS_MaybeGC(cx);
	}
    }
    return JS_TRUE;
}

#define ERROR_REPORT_LIMIT      10

static JSBool
lm_lookup_event_handler(JSContext *cx, JSObject *obj, uint32 type, jsval *fvp);

void PR_CALLBACK
lm_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    JSObject *obj;
    MochaDecoder *decoder;
    jsval fval, rval, argv[3];
    JSString *str;
    JSBool ok;
    char *last;
    int i, j, k, n;
    const char *s, *t;
#ifdef JAVA
    JSBool java_errors = JS_TRUE;
#else
    JSBool java_errors = JS_FALSE;
#endif /* JAVA */
    MWContext *context;

#ifdef XP_WIN16
	java_errors = JS_FALSE;
	return;
#endif

    obj = JS_GetGlobalObject(cx);
    decoder = JS_GetPrivate(cx, obj);
    if (!decoder)
        return;

    context = decoder->window_context;
    if (context && context->type == MWContextPrint)
	return;

    JS_SetErrorReporter(cx, NULL);
    if (lm_lookup_event_handler(cx, obj, EVENT_ERROR, &fval)) {
        if (fval == JSVAL_NULL) {
            /* User has set onerror to null, so cancel this report. */
            goto out;
        }
        if (JS_TypeOfValue(cx, fval) == JSTYPE_FUNCTION) {
            str = JS_NewStringCopyZ(cx, message);
            if (!str)
                goto out;
            argv[0] = STRING_TO_JSVAL(str);
            str = JS_NewStringCopyZ(cx, report ? report->filename : "");
            if (!str)
                goto out;
            argv[1] = STRING_TO_JSVAL(str);
            argv[2] = INT_TO_JSVAL(report ? report->lineno : 0);
#ifdef XP_UNIX
	    lm_handling_event++;
#endif
            ok = JS_CallFunctionValue(cx, obj, fval, 3, argv, &rval);
#ifdef XP_UNIX
	    lm_handling_event--;
#endif
            if (!ok) {
                /* Error during onerror, message is probably free now. */
                goto out;
            }
            if (rval == JSVAL_TRUE) {
                /* True return value means the function reported this error. */
                goto out;
            }
        }
    }

    decoder->error_count++;
    if (decoder->error_count >= ERROR_REPORT_LIMIT) {
        if (decoder->error_count == ERROR_REPORT_LIMIT) {
            last = PR_smprintf("too many %s errors", js_language_name);
	    if (last) {
		ET_PostMessageBox(decoder->window_context, last, JS_FALSE);
		XP_FREE(last);
            }
        }
        goto out;
    }

    last = PR_sprintf_append(NULL,
			     java_errors
			     ? "java.lang.System.err.println(\"%s Error: "
			     : "<FONT SIZE=4>\\\n<B>%s Error:</B> ",
			     js_language_name);

    if (!report) {
        last = PR_sprintf_append(last, 
				 java_errors 
				 ? "%s\\\n"
				 : "<B>%s</B>\n", 
				 message);
    }
    else {
        if (report->filename)
            last = PR_sprintf_append(last, "%s, ", report->filename);
        if (report->lineno) {
	    last = PR_sprintf_append(last, 
				     java_errors 
				     ? "line %u:\\\n" 
				     : "<B>line %u:</B>", 
				     report->lineno);
	}
	last = PR_sprintf_append(last,
				 java_errors
				 ? "  %s."
				 : "<BR><BR>%s.</FONT><PRE><FONT SIZE=4>",
				 message);
        if (report->linebuf) {
	    if (java_errors)
		last = PR_sprintf_append(last, "\\\n");
            for (s = t = report->linebuf; *s != '\0'; s = t) {
                for (; t != report->tokenptr && *t != '<' && *t != '\0'; t++)
                    ;
                last = PR_sprintf_append(last, "%.*s", t - s, s);
                if (*t == '\0')
                    break;
		if (java_errors) {
		    last = PR_sprintf_append(last, (*t == '\"') ? "\\\"" : "%c", *t);
		}
		else {
		    if (t == report->tokenptr) {
			last = PR_sprintf_append(last,
						 "</FONT>"
						 "<FONT SIZE=4 COLOR=#ff2020>");
		    }
		    last = PR_sprintf_append(last, (*t == '<') ? "&lt;" : "%c", *t);
                }
		t++;
            }
            if (java_errors) {
		last = PR_sprintf_append(last, "\\\n");
	    }
	    else {
		last = PR_sprintf_append(last, "</FONT><FONT SIZE=4>\n");
		n = report->tokenptr - report->linebuf;
		for (i = j = 0; i < n; i++) {
		if (report->linebuf[i] == '\t') {
		    for (k = (j + 8) & ~7; j < k; j++)
			last = PR_sprintf_append(last, ".");
		    continue;
		}
		    last = PR_sprintf_append(last, ".");
		    j++;
		}
		last = PR_sprintf_append(last, java_errors ? "^" : "<B>^</B>");
	    }
        }
	if (!java_errors)
	    last = PR_sprintf_append(last, "\n</FONT></PRE>");
    }

    if (java_errors)
	last = PR_sprintf_append(last, "\");");

    if (last) {
	if (java_errors) {
	    JS_EvaluateScript(lm_crippled_context, 
			      JS_GetGlobalObject(lm_crippled_context),
			      last, strlen(last), NULL, 0, &rval);
	}
	else {
	    if (context)
		ET_MakeHTMLAlert(context, last);
	}

	XP_FREE(last);
    }

out:
    JS_SetErrorReporter(cx, lm_ErrorReporter);
}

static MochaDecoder *
lm_new_decoder(JSRuntime *rt, JSClass *clasp)
{
    JSContext *cx;
    MochaDecoder *decoder;
    JSObject *obj;

    decoder = XP_NEW_ZAP(MochaDecoder);
    if (!decoder)
        return NULL;

/* XXX begin common */
    cx = JS_NewContext(rt, LM_STACK_SIZE);
    if (!cx) {
        XP_DELETE(decoder);
        return NULL;
    }

    decoder->forw_count = 1;
    decoder->js_context = cx;
    JS_SetBranchCallback(cx, lm_BranchCallback);
    JS_SetErrorReporter(cx, lm_ErrorReporter);

    obj = JS_NewObject(cx, clasp, NULL, NULL);
    if (!obj || !JS_SetPrivate(cx, obj, decoder)) {
        JS_DestroyContext(cx);
        XP_DELETE(decoder);
        return NULL;
    }

    /* If this decoder ever needs to hold anonymous images, then call
       lm_NewImageContext with a NULL context and use IL_SetDisplayMode to
       set the context later. */
    decoder->image_context = 0;
    decoder->window_context = 0;
    decoder->window_object = obj;
    JS_AddNamedRoot(cx, &decoder->window_object, "window_object");

    JS_SetGlobalObject(cx, obj);
/* XXX end common */

    JS_DefineFunctions(cx, obj, lm_global_functions);

    return decoder;
}

/*
 * Enable or disable local JS decoding.
 */
static XP_Bool lm_enabled = TRUE;
static XP_Bool lm_enabledMailNews = TRUE;
static XP_Bool lm_enabledSigning = TRUE;
static XP_Bool lm_enabledCrossOrigin = TRUE;

/*
 * Is this window enbled to do JS?
 */
JSBool
LM_CanDoJS(MWContext *context)
{
    PRBool forceJSEnabled;
    if (context) {
        forceJSEnabled = (JSBool)context->forceJSEnabled;
    } else {
        forceJSEnabled = PR_FALSE;
    }
    
    /* No JS for Editor unless forced on 
     * (e.g., for Composer Plugins, which uses JS to access preferences)
    */
    if (!forceJSEnabled &&
        (!LM_GetMochaEnabled() || EDT_IS_EDITOR(context))) {
         return JS_FALSE;
    }

    switch (context->type) {
      case MWContextBrowser:
      case MWContextDialog:
      case MWContextPane:
/* DSR101097 - OS/2 must follow NT w/ this or hangs printing large pages w/ JS */ 
#if defined(XP_WIN) || defined(XP_MAC) || defined(XP_OS2)
      /* only alow JS to run in windows print mode
       * don't know about the safty of other platforms yet
       */
      case MWContextPrint:
#endif /* XP_WIN */
        return JS_TRUE;
      case MWContextMail:
      case MWContextNews:
      case MWContextMailMsg:
      case MWContextNewsMsg:
      case MWContextMessageComposition:
        return (JSBool) lm_enabledMailNews;
      default:
        return JS_FALSE;
    }
}

/*
 * Is JS globally enabled?
 */
JSBool
LM_GetMochaEnabled(void)
{
    return (JSBool) lm_enabled;
}

/*
 * Is cross-origin access enabled?
 */
JSBool
lm_GetCrossOriginEnabled()
{
    return (JSBool) lm_enabledCrossOrigin;
}

static char lm_jsEnabled[] = "javascript.enabled";
static char lm_jsEnabledMN[] = "javascript.allow.mailnews";
static char lm_jsEnabledSigning[] = "javascript.allow.signing";
static char lm_jsEnabledCrossOrigin[] = "javascript.allow.crossOrigin";

PR_STATIC_CALLBACK(int)
lm_PrefChangedFunc(const char *pref, void *data)
{
    PREF_GetBoolPref(lm_jsEnabled, &lm_enabled);
    PREF_GetBoolPref(lm_jsEnabledMN, &lm_enabledMailNews);
    PREF_GetBoolPref(lm_jsEnabledSigning, &lm_enabledSigning);
    PREF_GetBoolPref(lm_jsEnabledCrossOrigin, &lm_enabledCrossOrigin);

    /*
     * If we started up w/ JS turned off we will have not bothered
     *   the separate thread and event queues, do it now
     */
    if (lm_owner_mon == NULL && lm_enabled)
        LM_InitMocha();

    return PREF_NOERROR;
}

static JSBool mochaInited = JS_FALSE;

/* nix, where is this */
extern void 
JSJ_InitContext(JSContext *context, JSObject *obj); 

/*
 * create the mocha thread, event queues, and stream converters
 */
static void
lm_ReallyInitMocha(void)
{
    int priority;

    /* register callback incase pref changes while we're running */
    PREF_RegisterCallback(lm_jsEnabled, lm_PrefChangedFunc, NULL);
    PREF_RegisterCallback(lm_jsEnabledMN, lm_PrefChangedFunc, NULL);
    PREF_RegisterCallback(lm_jsEnabledSigning, lm_PrefChangedFunc, NULL);
    PREF_RegisterCallback(lm_jsEnabledCrossOrigin, lm_PrefChangedFunc, NULL);

    if ( mochaInited ) {
        return;
    }
    
    lm_runtime = JS_Init(lm_max_gc_bytes);
    if (!lm_runtime)
        return;

    mochaInited = JS_TRUE;
    
    /* Initialize a crippled decoder/context for use by Java. */
    lm_crippled_decoder = lm_new_decoder(lm_runtime, &lm_dummy_class);
    lm_crippled_context = lm_crippled_decoder->js_context;

#ifdef JAVA
    LJ_JSJ_Init();

    /* 
     * Get liveconnect functions defined for the crippled context
     *   so we can pass error messages to the JavaConsole
     */
    JSJ_InitContext(lm_crippled_context, JS_GetGlobalObject(lm_crippled_context));
#endif

    /* Initialize a dummy object used for unreflectable applets and embeds. */
    lm_DummyObject = lm_crippled_decoder->window_object;

    /* Associate a JS netlib "converter" with our mime type. */
    /* Cool, we are still in the mozilla thread at this point so
       we don't have to make this call into an event passage */
    NET_RegisterContentTypeConverter(js_content_type, FO_PRESENT, 0,
                                     NET_CreateMochaConverter);
    NET_RegisterContentTypeConverter(TEXT_CSS, FO_PRESENT, 0,
                                     NET_CreateMochaConverter);
    NET_RegisterContentTypeConverter(TEXT_JSSS, FO_PRESENT, 0,
                                     NET_CreateMochaConverter);

    /* Associate a JS netlib "converter" with our CSS too. */
    NET_RegisterContentTypeConverter(TEXT_CSS, FO_PRESENT, 0,
                                     NET_CreateMochaConverter);

    /* Make sure the mozilla event queue is around */
    XP_ASSERT(mozilla_event_queue != NULL);

    /* run at slightly lower priority than the mozilla thread */
    priority = PR_GetThreadPriority(PR_CurrentThread());
    PR_ASSERT(priority >= PR_PRIORITY_FIRST && priority <= PR_PRIORITY_LAST);

    if (priority == PR_PRIORITY_NORMAL)
	priority = PR_PRIORITY_LOW;
    else if (priority == PR_PRIORITY_HIGH)
	priority = PR_PRIORITY_NORMAL;
    else if (priority == PR_PRIORITY_URGENT)
	priority = PR_PRIORITY_HIGH;
    else
	priority = PR_PRIORITY_LOW;	

    /* create the monitor for the owner */
    if (lm_owner_mon == NULL)
        lm_owner_mon = PR_NewMonitor();

    /* create the interpreter thread. */
    /*
     * Grab the lm_owner_mon until we are done with thread creation and
     *   initialization.  The thread will immediately try to grab the 
     *   same monitor when it starts up, so if its able to get the monitor
     *   it can be certain that all of the thread state has been initialized
     */
    PR_EnterMonitor(lm_owner_mon);
    lm_InterpretThread = PR_CreateThreadGCAble(PR_USER_THREAD, 
					       lm_wait_for_events, NULL,
					       priority, PR_LOCAL_THREAD, 
					       PR_UNJOINABLE_THREAD,
					       0);  /* default stack size */

    lm_InterpretQueue = PR_CreateEventQueue("mocha-event-queue",
					    lm_InterpretThread);

    PR_Notify(lm_owner_mon);
    PR_ExitMonitor(lm_owner_mon);

    /* AutoInstall trigger functions for JS config object */
    lm_DefineTriggers();

#ifdef JSDEBUGGER
    lm_InitJSDebug(lm_runtime);
#endif

    return;
}

void
LM_ForceJSEnabled(MWContext *cx)
{
    lm_ReallyInitMocha();
    cx->forceJSEnabled = PR_TRUE;
    return;
}

/* XXX return boolean to propagate errors. */
void
LM_InitMocha(void)
{
    /* register callback incase pref changes while we're running */
    PREF_RegisterCallback(lm_jsEnabled, lm_PrefChangedFunc, NULL);
    PREF_RegisterCallback(lm_jsEnabledMN, lm_PrefChangedFunc, NULL);
    PREF_RegisterCallback(lm_jsEnabledSigning, lm_PrefChangedFunc, NULL);
    PREF_RegisterCallback(lm_jsEnabledCrossOrigin, lm_PrefChangedFunc, NULL);

    /* get our enabled-ness states */
    PREF_GetBoolPref(lm_jsEnabled, &lm_enabled);
    PREF_GetBoolPref(lm_jsEnabledMN, &lm_enabledMailNews);
    PREF_GetBoolPref(lm_jsEnabledSigning, &lm_enabledSigning);
    PREF_GetBoolPref(lm_jsEnabledCrossOrigin, &lm_enabledCrossOrigin);

    /* set up the initial queue stack pointers */
    if (!et_TopQueue) {
        et_TopQueue = XP_NEW_ZAP(QueueStackElement);
        if (!et_TopQueue)
            return;
    }

    if (!lm_enabled)
        return;

    lm_ReallyInitMocha();
}

static int lm_moja_initialized = LM_MOJA_UNINITIALIZED;

PRLogModuleInfo* Moja;

void MojaLogModuleInit()
{
    Moja = PR_NewLogModule("Moja");
}

/* returns FALSE if already done or TRUE for success */
int
LM_InitMoja()
{
    /* XXX assert mozilla thread */
    /* this stuff should only be done once.  since it is always
     * called on the moz thread we can do it the easy way */
    if (lm_moja_initialized != LM_MOJA_UNINITIALIZED)
        return lm_moja_initialized;

#ifdef JAVA
    /* initialize the java env associated with the mocha thread */
    lm_JSEnv = LJ_EnsureJavaEnv(lm_InterpretThread);
    if (lm_JSEnv == NULL) {
        lm_moja_initialized = LM_MOJA_JAVA_FAILED;
        return lm_moja_initialized;
    }

    lm_moja_initialized = LM_MOJA_OK;
#else
    lm_moja_initialized = LM_MOJA_JAVA_FAILED;
#endif

    MojaLogModuleInit();

    return lm_moja_initialized;
}

/* returns FALSE if already done or TRUE for success */
int
LM_IsMojaInitialized()
{
    return lm_moja_initialized;
}

void
LM_FinishMoja()
{
    /* XXX write me */
}


PRBool PR_CALLBACK
LM_HandOffJSLock(PRThread * oldOwner, PRThread *newOwner)
{
        /* XXX note this doesn't worry about the lm_owner_count,
         * assumes it's pushed/popped properly */
        PRBool didHandOff = PR_FALSE;
        PR_EnterMonitor(lm_owner_mon);
        if (lm_owner == oldOwner) {
                lm_owner = newOwner;
                didHandOff = PR_TRUE;
        }
        PR_Notify(lm_owner_mon);
        PR_ExitMonitor(lm_owner_mon);
        return didHandOff;
}

static PRBool            mozWantsJSLock  = PR_FALSE;
static PRBool            mozGotJSLock    = PR_FALSE;

typedef struct lm_lock_waiter {
    JSLockReleaseFunc       fn;
    void                  * data;
    struct lm_lock_waiter * next;
} lm_lock_waiter;

static lm_lock_waiter * lm_waiting_list = NULL;

/*
 * push the mozilla event loop until the jslock is obtained
 */
static void
LM_WaitForJSLock(void)
{
    PREventQueue *q = mozilla_event_queue;
    PRMonitor *mon = PR_GetEventQueueMonitor(q);
    PRBool hadLayoutLock;

    XP_ASSERT(PR_CurrentThread() == mozilla_thread);

    hadLayoutLock = (JSBool)(!LO_VerifyUnlockedLayout());
    if (hadLayoutLock)
        LO_UnlockLayout();
    while (!mozGotJSLock) {
        PR_EnterMonitor(mon);
        if (!PR_EventAvailable(q))
            PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
        PR_ExitMonitor(mon);
        PR_ProcessPendingEvents(q);
    }
    if (hadLayoutLock)
        LO_LockLayout();
}

/*
 * Wait until we get the JSLock
 */
void PR_CALLBACK
LM_LockJS()
{
    PRThread *t = PR_CurrentThread();

    XP_ASSERT(lm_owner_mon != NULL);

    PR_EnterMonitor(lm_owner_mon);

    while (lm_owner != t) {
        if (lm_owner == NULL) {
            lm_owner = t;
            break;
        }
        if (PR_CurrentThread() == mozilla_thread) {
            /* blocking here could deadlock, because the owner
             * of the js thread may make synchronous event calls
             * to the moz thread through et_moz.c or others. */

            /* while we wait, run another event loop */
            mozWantsJSLock = PR_TRUE;
            mozGotJSLock = PR_FALSE;
            PR_ExitMonitor(lm_owner_mon);
            LM_WaitForJSLock();
            PR_EnterMonitor(lm_owner_mon);
            XP_ASSERT(lm_owner == mozilla_thread);
            mozWantsJSLock = PR_FALSE;
            mozGotJSLock = PR_FALSE;
        } else {
            PR_Wait(lm_owner_mon, PR_INTERVAL_NO_TIMEOUT);
        }
    }
    lm_owner_count++;
    PR_ExitMonitor(lm_owner_mon);
}

/*
 * Try to get the JSLock but just return JS_FALSE if we can't
 *   get it, don't wait since we could deadlock
 */
JSBool PR_CALLBACK
LM_AttemptLockJS(JSLockReleaseFunc fn, void * data)
{
    PRThread *t = PR_CurrentThread();

    /*
     * If javascript is disabled this might have never been
     *   created.  In that case its never possible to get the
     *   js lock.
     */
    if (!lm_owner_mon)
       return JS_FALSE;

    PR_EnterMonitor(lm_owner_mon);
    if (lm_owner == NULL || lm_owner == t) {
        lm_owner = t;
        lm_owner_count++;
        PR_ExitMonitor(lm_owner_mon);
        return JS_TRUE;
    }

    if (fn) {           /* XXX - Only one waitor function allowed at a time */
        lm_lock_waiter ** p;
        lm_lock_waiter * waiter = XP_NEW_ZAP(lm_lock_waiter);
        if (!waiter) {
            PR_ExitMonitor(lm_owner_mon);
            return JS_FALSE;
        }

        waiter->fn = fn;
        waiter->data = data;

        /* double indirection! */
        for (p = &lm_waiting_list; *p; p = &(*p)->next)
            ;

        *p = waiter;
    }

    PR_ExitMonitor(lm_owner_mon);
    return JS_FALSE;

}

JSBool PR_CALLBACK
LM_ClearAttemptLockJS(JSLockReleaseFunc fn, void * data)
{
    lm_lock_waiter ** p;
    lm_lock_waiter * waiter;

    for (p = &lm_waiting_list; (waiter = *p) != NULL; p = &waiter->next) {
	if (waiter->fn == fn && waiter->data == data) {
	    *p = waiter->next;
            XP_FREE(waiter);
	    return JS_TRUE;
	}
    }
    return JS_FALSE;
}

PR_STATIC_CALLBACK(void)
lm_MozGotJSLock(void *data)
{
    /* does nothing - signalling is done when the lock
     * is transferred over.  the event that calls this
     * function is only sent to wake up the nested event
     * loop */
}

/*
 * Release the JSLock
 */
void PR_CALLBACK
LM_UnlockJS()
{
    XP_ASSERT(PR_CurrentThread() == lm_owner);
    PR_EnterMonitor(lm_owner_mon);
    XP_ASSERT(lm_owner_count > 0);
    if (--lm_owner_count <= 0) {
        lm_owner_count = 0;
        lm_owner = NULL;
        lm_owner_context = NULL;

        /* was anyone waiting for us to release the JSLock? */

        /* moz gets priority, and we hand the lock off immediately */
        if (mozWantsJSLock) {
            lm_owner = mozilla_thread;
            lm_owner_count = 0;
            mozWantsJSLock = PR_FALSE;
            mozGotJSLock = PR_TRUE;
            ET_moz_CallFunctionAsync(lm_MozGotJSLock, NULL);
        } else if (lm_waiting_list) {
            lm_lock_waiter * waiter = lm_waiting_list;
            lm_waiting_list = waiter->next;
            ET_moz_CallFunctionAsync(waiter->fn, waiter->data);
            XP_FREE(waiter);
        }

        PR_Notify(lm_owner_mon);
    }
    PR_ExitMonitor(lm_owner_mon);

}

/*
 * keep track of the current context that owns the JS lock in
 *   case we get an interrupt event and need to decide whether
 *   to interrupt the current operation or not
 */
void
LM_JSLockSetContext(MWContext * context)
{
    XP_ASSERT(lm_owner_mon != NULL);
    PR_EnterMonitor(lm_owner_mon);
    XP_ASSERT(lm_owner == PR_CurrentThread());
    lm_owner_context = context;
    PR_ExitMonitor(lm_owner_mon);
}

/* this could race.  does it matter ? */
MWContext *
LM_JSLockGetContext()
{
    return lm_owner_context;
}


MochaDecoder *
LM_GetMochaDecoder(MWContext *context)
{
    JSContext *cx;
    MochaDecoder *decoder;

    if (!LM_CanDoJS(context))
        return NULL;

    /* Get the context's JS decoder, creating one if necessary. */
    cx = context->mocha_context;
    if (cx) {
        decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    } else {
        decoder = lm_NewWindow(context);
        if (!decoder)
            return NULL;
        cx = decoder->js_context;
    }
    if (!decoder->document && !lm_InitWindowContent(decoder))
        return NULL;
    /* The decoder has at least one forward ref from context. */
    XP_ASSERT(decoder->forw_count > 0);
    decoder->forw_count++;
    return decoder;
}

void
LM_PutMochaDecoder(MochaDecoder *decoder)
{
    JSContext * cx;

    XP_ASSERT(decoder->forw_count > 0);
    if (--decoder->forw_count <= 0) {
        decoder->forw_count = 0;
        if (decoder->window_context)
            decoder->window_context->mocha_context = NULL;

#define CLEAR(obj)      obj = 0

        /* Clear all object prototype refs. */
        CLEAR(decoder->anchor_prototype);
        CLEAR(decoder->bar_prototype);
        CLEAR(decoder->document_prototype);
        CLEAR(decoder->event_prototype);
        CLEAR(decoder->event_capturer_prototype);
        CLEAR(decoder->event_receiver_prototype);
        CLEAR(decoder->form_prototype);
        CLEAR(decoder->image_prototype);
        CLEAR(decoder->input_prototype);
        CLEAR(decoder->layer_prototype);
        CLEAR(decoder->option_prototype);
        CLEAR(decoder->rect_prototype);
        CLEAR(decoder->url_prototype);

        /* Clear window sub-object refs. */
        if (decoder->document)
            lm_CleanUpDocumentRoots(decoder, decoder->document);
        CLEAR(decoder->document);
        CLEAR(decoder->history);
        CLEAR(decoder->location);
        CLEAR(decoder->navigator);
        CLEAR(decoder->components);
        CLEAR(decoder->crypto);
        CLEAR(decoder->screen);
        CLEAR(decoder->hardware);
        CLEAR(decoder->pkcs11);

        /* Clear ad-hoc GC roots. */
        CLEAR(decoder->event_receiver);
        CLEAR(decoder->opener);

        /* Clear the root of this window's object tree. */
        CLEAR(decoder->window_object);

#undef CLEAR

        /* Don't forget to clear GC roots in the context. */
        cx = decoder->js_context;
        JS_ClearRegExpRoots(cx);

        /* Reset these in case a backward reference to decoder lingers. */
        decoder->firstVersion = JSVERSION_UNKNOWN;
        decoder->principals = NULL;

        /* Hold an extra back ref to keep cx and decoder alive. */
        HOLD_BACK_COUNT(decoder);
        JS_SetGlobalObject(cx, NULL);
        JS_GC(cx);

        /*
         * If the finalizer ran, and there are no back refs from private
         * data of objects held by other windows' properties, then this drop
         * will destroy the decoder.  Otherwise it must live on until its
         * runaway kids are all finalized.
         */
        DROP_BACK_COUNT(decoder);
    }
}

JSBool
LM_IsActive(MWContext *context)
{
    JSContext *cx = context->mocha_context;
    if (!cx)
        return JS_FALSE;
    
    return (JSBool)(JS_IsRunning(cx) || (context->js_timeouts_pending > 0));
}

const char *
LM_GetSourceURL(MochaDecoder *decoder)
{
    JSContext *cx;
    const char *originURL;

    if (decoder->nesting_url)
        return decoder->nesting_url->str;
    if (decoder->source_url)
        return decoder->source_url;
    cx = decoder->js_context;
    originURL = lm_GetObjectOriginURL(cx, decoder->window_object);
    if (!originURL)
        return NULL;
    decoder->source_url = JS_strdup(cx, originURL);
    return decoder->source_url;
}

static const char AUTOINSTALL_PREFIX[] = "autoinstall:";
static size_t AUTOINSTALL_PREFIX_LENGTH = sizeof AUTOINSTALL_PREFIX - 1;

static JSObject *
lm_get_scope_from_string(MochaDecoder *decoder, JSContext *cx,
                         const char *source_url, uint lineno, 
                         char *scope_to, JSPrincipals *principals)
{
    jsval result;
    JSBool ok;
    JSObject *scope;

    if (!scope_to)
        return NULL;

    /*
     * Hack for ASD -- create a new object with environment from scope_to.
     * If we ever change autoinstall:, change the define MOCHA_CONTEXT_PREFIX
     * in softupdt.c  XXXbe how about a single definition in a .h file?
     */
    if (!XP_STRNCMP(scope_to, AUTOINSTALL_PREFIX, AUTOINSTALL_PREFIX_LENGTH)) {
        return lm_NewSoftupObject(cx, decoder,
                                  scope_to + AUTOINSTALL_PREFIX_LENGTH);
    }


    if (decoder->active_layer_id == LO_DOCUMENT_LAYER_ID) {
        scope = decoder->window_object;
    } else {
        LO_LockLayout();
        scope = LO_GetLayerMochaObjectFromId(decoder->window_context,
                                             decoder->active_layer_id);
        LO_UnlockLayout();
    }

    /* pass the scope as a script to return the scope object */
    ok = JS_EvaluateScriptForPrincipals(cx,
                                        scope,
                                        principals,
                                        scope_to,
                                        strlen(scope_to),
                                        source_url,
                                        lineno,
                                        &result);

    if (ok) {
        XP_ASSERT(JSVAL_IS_OBJECT(result));
        if (JSVAL_IS_OBJECT(result))
            return JSVAL_TO_OBJECT(result);
    }

    return NULL;
}

JSBool
LM_EvaluateBuffer(MochaDecoder *decoder, void *base, size_t length,
                  uint lineno, char *scope_to, JSPrincipals *principals,
                  JSBool unicode, jsval *result)
{
    JSContext *cx;
    const char *source_url;
    JSBool ok;
    JSObject *scope;

    /* XXX if lineno == 0, do something smart */
    source_url = LM_GetSourceURL(decoder);
    if (!source_url)
        return JS_FALSE;
    cx = decoder->js_context;

    principals = LM_RegisterPrincipals(decoder, principals, NULL, base);
    if (!principals)
        return JS_FALSE;

    scope = lm_get_scope_from_string(decoder, cx, source_url, lineno, 
                                     scope_to, principals);
    if (!scope) {
        if (decoder->active_layer_id == LO_DOCUMENT_LAYER_ID) {
            scope = decoder->window_object;
        } else {
            const char *scope_source_url;

            LO_LockLayout();
            scope = LO_GetLayerMochaObjectFromId(decoder->window_context,
                                                 decoder->active_layer_id);
            LO_UnlockLayout();
	    if (!scope)
		return JS_FALSE;
            scope_source_url = lm_GetLayerOriginURL(cx, scope);
            if (scope_source_url)
                source_url = scope_source_url;
        }
    }

    if (unicode)
	ok = JS_EvaluateUCScriptForPrincipals(cx, scope, principals, base,
					      length, source_url, lineno, 
					      result);
    else
	ok = JS_EvaluateScriptForPrincipals(cx, scope, principals, base, 
					    length, source_url, lineno, 
					    result);
    return ok;
}

char *
LM_EvaluateAttribute(MWContext *context, char *expr, uint lineno)
{
    char *bytes;
    MochaDecoder *decoder;
    jsval result;
    JSContext *cx;
    JSPrincipals *principals;

    bytes = 0;
    if (!expr)
        return bytes;
    decoder = LM_GetMochaDecoder(context);
    if (!decoder)
        return bytes;
    
    /* 
     * Since this called directly from the mozilla thread, we know that
     * it's safe to sample the doc_id. In fact, we have to do that,
     * otherwise the doc_id check could fail for any entities processed.
     */
    decoder->doc_id = XP_DOCID(context);

    cx = decoder->js_context;
    if (!JS_AddRoot(cx, &result)) {   /* XXX chouck - can we do a lockGCThing here */
        LM_PutMochaDecoder(decoder);
        return bytes;
    }
    principals = lm_GetCompilationPrincipals(decoder, NULL);
    if (principals) {
        JSPRINCIPALS_HOLD(cx, principals);
        if (LM_EvaluateBuffer(decoder, expr, XP_STRLEN(expr), lineno, NULL,
                              principals, JS_FALSE, &result)) {
            bytes = lm_StrToLocalEncoding(context,
					  JS_ValueToString(cx, result));
        }
        JSPRINCIPALS_DROP(cx, principals);
    }
    JS_RemoveRoot(cx, &result);
    LM_PutMochaDecoder(decoder);
    return bytes;
}

static JSObject*
lm_get_layer_parent(JSContext *cx, JSObject *obj) 
{
    JSObject *save = obj;
    jsval val;

    if (!JS_InstanceOf(cx, obj, &lm_layer_class, NULL))
        return NULL;

    while (obj) {
        if (JS_InstanceOf(cx, obj, &lm_document_class, NULL)) {
            return obj;
        }
        obj = JS_GetParent(cx, obj);
    }

    /* Should only get here if the layer's parent has been severed
     * Don't want to use this method in general because it could
     * cause a security violation on the layer container check.
     */
    if (!JS_GetProperty(cx, save, lm_parentLayer_str, &val))
        return NULL;
    if (val != JSVAL_NULL && JSVAL_IS_OBJECT(val)) {
        obj = JSVAL_TO_OBJECT(val);
        if (!JS_GetProperty(cx, obj, lm_document_str, &val))
            return NULL;

        if (val != JSVAL_NULL && JSVAL_IS_OBJECT(val))
            return JSVAL_TO_OBJECT(val);
    }

    return NULL;
}


static JSBool
lm_lookup_event_handler(JSContext *cx, JSObject *obj, uint32 type, jsval *fvp)
{
    JSEventNames *names;
    char name[32];
    JSBool ok;

    names = lm_GetEventNames(type);
    if (!names) {
        *fvp = JSVAL_VOID;
        return JS_TRUE;
    }
    PR_snprintf(name, sizeof name, "on%s", names->lowerName);
    ok = JS_LookupProperty(cx, obj, name, fvp);
    if (ok && JSVAL_IS_VOID(*fvp)) {
        PR_snprintf(name, sizeof name, "on%s", names->mixedName);
        ok = JS_LookupProperty(cx, obj, name, fvp);
    }
    return ok;
}

JSBool
lm_SendEvent(MWContext *context, JSObject *obj, JSEvent *event, jsval *result)
{
    JSContext *cx;
    JSObject *eventObj;
    MochaDecoder *decoder;
    JSBool ok = JS_TRUE;
    jsval funval, argv[1];
    MWContext *mwcx;
    uint32 event_capture_bit = 0;

    decoder = LM_GetMochaDecoder(context);
    if (!decoder)
        return JS_FALSE;

    cx = decoder->js_context;

    if (!event->object)
        event->object = obj;

    event_capture_bit |= context->event_bit;

    if (context->is_grid_cell) {
        mwcx = context;
        while (mwcx->grid_parent) {
            mwcx = mwcx->grid_parent;
            event_capture_bit |= mwcx->event_bit;
        }
    }

    ok = lm_lookup_event_handler(cx, obj, event->type, &funval);
    if (!ok ||
        (JS_TypeOfValue(cx, funval) != JSTYPE_FUNCTION &&
         !(event_capture_bit & event->type))) {
        goto out;
    }

    eventObj = lm_NewEventObject(decoder, event);
    if (!eventObj) {
        ok = JS_FALSE;
        goto out;
    }

    argv[0] = OBJECT_TO_JSVAL(eventObj);
    ok = lm_FindEventHandler(context, obj, eventObj, funval, result);

out:
    LM_PutMochaDecoder(decoder);
    return ok;
}

JSBool
lm_FindEventHandler(MWContext *context, JSObject *obj, JSObject *eventObj,
                    jsval funval, jsval *result)
{

    MochaDecoder *decoder;
    JSContext *cx;
    JSBool ok, is_window;
    JSEventCapturer *js_cap;
    JSEvent *event;
    MWContext *mwcx;
    uint32 event_capture_bit = 0;

    if (!obj)
        return JS_TRUE;
    
    decoder = LM_GetMochaDecoder(context);
    if (!decoder)
        return JS_FALSE;

    cx = decoder->js_context;

    /*  Fun, fun.  Time for the reverse architecture event capturing.
     *  First, let's see if we're capturing at all.
     */
    event_capture_bit |= context->event_bit;

    if (context->is_grid_cell) {
        mwcx = context;
        while (mwcx->grid_parent) {
            mwcx = mwcx->grid_parent;
            event_capture_bit |= mwcx->event_bit;
        }
    }

    if (!(event = JS_GetInstancePrivate(cx, eventObj, &lm_event_class, NULL))) {
        LM_PutMochaDecoder(decoder);
        return JS_FALSE;
    }

    if (!(event_capture_bit & event->type)) {
        /*No capturing going on.  Just call the function.*/
        LM_PutMochaDecoder(decoder);
        return lm_HandleEvent(cx, obj, eventObj, funval, result);

    }

    event->event_handled = JS_FALSE;

    /*Somebody wants it.  Let's go!  Time for recursion!*/

    is_window = JS_InstanceOf(cx, obj, &lm_window_class, NULL);
    if (!is_window || context->grid_parent) {
        if (is_window && context->is_grid_cell) {
            if (context->grid_parent->mocha_context) {
                ok = lm_FindEventHandler(context->grid_parent, 
                        JS_GetGlobalObject(context->grid_parent->mocha_context),
                        eventObj, 0, result);
            } else {
                ok = JS_TRUE;
            }
        } else if (JS_InstanceOf(cx, obj, &lm_layer_class, NULL)) {
            ok = lm_FindEventHandler(context, lm_get_layer_parent(cx, obj), 
                                     eventObj, 0, result);
        } else {
            ok = lm_FindEventHandler(context, JS_GetParent(cx, obj),
                                     eventObj, 0, result);
        }
    }
    
    if (!event->event_handled) {

        /*We unfortunately have to check versus different type here.*/
        if (JS_InstanceOf(cx, obj, &lm_window_class, NULL)) {

            if (decoder->event_bit & event->type &&
                !(decoder->event_mask & event->type))
            {
                decoder->event_mask |= event->type;
                ok = lm_HandleEvent(cx, obj, eventObj, funval, result);
                decoder->event_mask &= ~event->type;
                LM_PutMochaDecoder(decoder);
                event->event_handled = ok;
                return ok;
            }
        }

        if (JS_InstanceOf(cx, obj, &lm_document_class, NULL) || 
            JS_InstanceOf(cx, obj, &lm_layer_class, NULL)) {
            js_cap = JS_GetPrivate(cx, obj);

            if (js_cap && js_cap->event_bit & event->type &&
                    !(js_cap->base.event_mask & event->type))
            {
                XP_ASSERT(cx == event->decoder->js_context);
                js_cap->base.event_mask |= event->type;
                ok = lm_HandleEvent(cx, obj, eventObj, funval, result);
                js_cap->base.event_mask &= ~event->type;
                LM_PutMochaDecoder(decoder);
                event->event_handled = ok;
                return ok;
            }
        }

        /* If we get this far we should be back at the original object and it
         * needs to have its eventhandler called.
         */
        if (obj == event->object) {
            ok = lm_HandleEvent(cx, obj, eventObj, funval, result);
        }
    }
    LM_PutMochaDecoder(decoder);
    return ok;
}

JSBool
lm_HandleEvent(JSContext *cx, JSObject *obj, JSObject *eventObj,
                jsval funval, jsval *result)
{
    JSEvent *event;
    JSBool ok = JS_TRUE;
    jsval argv[1];
    char name[32];
    JSFunction *fun;

    if (JS_TypeOfValue(cx, funval) != JSTYPE_FUNCTION) {
        event = JS_GetPrivate(cx, eventObj);
        if (!event) {
            ok = JS_FALSE;
            goto out;
        }

        PR_snprintf(name, sizeof name, "on%s", lm_EventName(event->type));
        if (!JS_LookupProperty(cx, obj, name, result)) {
            ok = JS_FALSE;
            goto out;
        }

        ok = lm_lookup_event_handler(cx, obj, event->type, &funval);
        if (!ok || JS_TypeOfValue(cx, funval) != JSTYPE_FUNCTION)
            goto out;

        fun = JS_ValueToFunction(cx, funval);
        if (fun == NULL) {
            ok = JS_FALSE;
            goto out;
        }

        if (!lm_CanCaptureEvent(cx, fun, event)) {
            ok = JS_FALSE;
            goto out;
        }
    }


    argv[0] = OBJECT_TO_JSVAL(eventObj);
#ifdef XP_UNIX
    lm_handling_event++;
#endif
    ok = JS_CallFunctionValue(cx, obj, funval, 1, argv, result);
#ifdef XP_UNIX
    lm_handling_event--;
#endif

out:
    return ok;
}

/*
 * Wrapper for the common case of decoder's stream being set by code running
 * on decoder's JS context.
 */
JSBool
LM_SetDecoderStream(MWContext * context, NET_StreamClass *stream,
                    URL_Struct *url_struct, JSBool free_stream_on_close)
{
    MochaDecoder *decoder = LM_GetMochaDecoder(context);
    JSBool rv;

    if (!decoder)
        return (JS_FALSE);

    rv = lm_SetInputStream(decoder->js_context, decoder, stream,
                           url_struct, free_stream_on_close);

    LM_PutMochaDecoder(decoder);
    return(rv);

}

void
lm_SetActiveForm(MWContext * context, int32 id)
{
    MochaDecoder *decoder = LM_GetMochaDecoder(context);

    if (!decoder)
        return;

    decoder->active_form_id = id;
    LM_PutMochaDecoder(decoder);

}

void
LM_SetActiveLayer(MWContext * context, int32 layer_id)
{
    MochaDecoder *decoder = LM_GetMochaDecoder(context);
    if (!decoder)
        return;

    decoder->active_layer_id = layer_id;

    LM_PutMochaDecoder(decoder);
}

int32
LM_GetActiveLayer(MWContext * context)
{
    int32 layer_id;
    
    MochaDecoder *decoder = LM_GetMochaDecoder(context);
    if (!decoder)
        return LO_DOCUMENT_LAYER_ID;

    layer_id = decoder->active_layer_id;

    LM_PutMochaDecoder(decoder);

    return layer_id;
}

JSBool
lm_SetInputStream(JSContext *cx, MochaDecoder *decoder,
                  NET_StreamClass *stream, URL_Struct *url_struct,
                  JSBool free_stream_on_close)
{
    const char *origin_url;
    JSObject *container;
    JSPrincipals *principals;

    /*
     * If the stream is NULL don't clobber the old value
     */
    if (stream) {
        decoder->stream = stream;
        decoder->free_stream_on_close = free_stream_on_close;
    }
    
    /*
     * If we're still in the process of loading the main parser stream
     * (i.e. the window's load event hasn't been sent), the window is the 
     * owner of the stream.
     */
    if (!decoder->load_event_sent)
        decoder->stream_owner = LO_DOCUMENT_LAYER_ID;
    decoder->url_struct = NET_HoldURLStruct(url_struct);

    /* Don't update origin for <SCRIPT SRC="URL"> URLs. */
    if (decoder->nesting_url)
        return JS_TRUE;

    /* Update the origin URL associated with the document in decoder. */
    if (!decoder->writing_input) {
        switch (NET_URL_Type(url_struct->address)) {
          case ABOUT_TYPE_URL:
          case MOCHA_TYPE_URL:
            return JS_TRUE;
        }
        origin_url = url_struct->address;
    } else {
        origin_url = lm_GetSubjectOriginURL(cx);
	if (!origin_url)
	    return JS_FALSE;
    }

    container = lm_GetActiveContainer(decoder);
    if (container == NULL)
        return JS_FALSE;
    principals = lm_GetInnermostPrincipals(cx, container, NULL);
    if (principals == NULL)
        return JS_FALSE;
    if (XP_STRCMP(principals->codebase, origin_url) == 0) {
	/* XXXbe Don't want to invalidate self-modifying doc's cert principals
		 pessimisticly (for fear of a written unsigned <script> tag).
		 Rather, let LM_RegisterPrincipals handle that case only when
		 it arises.  Note that overwrite of a closed doc drops any old
		 principals via LM_ReleaseDocument/lm_FreeWindowContent.
	lm_InvalidateCertPrincipals(decoder, principals);
	*/
	return JS_TRUE;
    }
    principals = LM_NewJSPrincipals(NULL, NULL, origin_url);
    if (principals == NULL)
	return JS_FALSE;
    lm_SetContainerPrincipals(cx, container, principals);
    return JS_TRUE;
}

NET_StreamClass *
lm_ClearDecoderStream(MochaDecoder *decoder, JSBool fromDiscard)
{
    NET_StreamClass *stream;
    URL_Struct *url_struct;

    stream = decoder->stream;
    url_struct = decoder->url_struct;
    decoder->stream = 0;
    decoder->stream_owner = LO_DOCUMENT_LAYER_ID;
    decoder->url_struct = 0;
    if (decoder->writing_input) {
        decoder->writing_input = JS_FALSE;
        if (stream) {
            /*
             * Complete the stream before freeing the URL struct to which it
             * may hold a private pointer.
             */
            if (!fromDiscard) {
                if (decoder->window_context && 
                    XP_DOCID(decoder->window_context) != -1)
                    ET_moz_CallFunction( (ETVoidPtrFunc) stream->complete, (void *)stream);
                if (decoder->free_stream_on_close)
                    XP_DELETE(stream);
            }
            stream = NULL;
        }
        if (url_struct) {
            /* we should no longer be setting this */
            XP_ASSERT(url_struct->pre_exit_fn == NULL);
        }
    }

    /*
     * whether we were writing or not we were holding a reference
     * to this url_struct.  Let go of it now
     */ 
    if (url_struct)
        NET_DropURLStruct(url_struct);
    decoder->free_stream_on_close = JS_FALSE;
    return(stream);
}

PRIVATE void
LM_ClearContextStream(MWContext *context)
{
    MochaDecoder *decoder;

    if (!context->mocha_context) {
        /* Don't impose cost on non-JS contexts. */
        return;
    }
    decoder = LM_GetMochaDecoder(context);
    if (!decoder) {
        /* XXX how can this happen?  If lm_InitWindow fails or if the user
               turns off JS in the middle of a load.  Return false! */
        return;
    }
    lm_ClearDecoderStream(decoder, JS_FALSE);
    LM_PutMochaDecoder(decoder);
}

JSBool
lm_SaveParamString(JSContext *cx, PA_Block *bp, const char *str)
{
    if (*bp)
        PA_FREE(*bp);

    *bp = (PA_Block) strdup(str);
    if (!*bp) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    return JS_TRUE;
}

JSObject *
lm_GetOuterObject(MochaDecoder * decoder)
{
    lo_FormData *form_data;
    JSObject * rv = NULL;

    if (decoder->active_form_id) {
        LO_LockLayout();
        form_data =
            LO_GetFormDataByID(decoder->window_context,
                               decoder->active_layer_id,
                               decoder->active_form_id);
        if (form_data)
            rv = form_data->mocha_object;

        LO_UnlockLayout();
    }
    else
        rv = lm_GetDocumentFromLayerId(decoder, decoder->active_layer_id);

    return rv;
}

/*
 * Add an object to an array
 */
JSBool
lm_AddObjectToArray(JSContext * cx, JSObject * array_obj,
                    const char * name, jsint index, JSObject * obj)
{
    JSObjectArray *array;

    array = JS_GetPrivate(cx, array_obj);
    if (!array)
        return JS_TRUE;

    if (name) {
        if (!JS_DefineProperty(cx, array_obj, name, OBJECT_TO_JSVAL(obj),
                               NULL, NULL,
                               JSPROP_ENUMERATE | JSPROP_READONLY)) {
            return JS_FALSE;
        }
    }

    if (!JS_DefineElement(cx, array_obj, (jsint) index,
                          OBJECT_TO_JSVAL(obj),
                          NULL, NULL,
                          JSPROP_ENUMERATE | JSPROP_READONLY)) {
        return JS_FALSE;
    }

    if (index >= array->length)
        array->length = index + 1;

    return JS_TRUE;

}

void
lm_SetVersion(MochaDecoder *decoder, JSVersion version) {
    if (version == JSVERSION_UNKNOWN) {
        version = (decoder->firstVersion == JSVERSION_UNKNOWN)
            ? JSVERSION_DEFAULT
            : decoder->firstVersion;
    }
    if (decoder->firstVersion == JSVERSION_UNKNOWN) {
        decoder->firstVersion = version;
    }
    JS_SetVersion(decoder->js_context, version);
}

/*
 * Convert a locally encoded string into a 16-bit unicode string to pass
 *   to the JS runtime.  Allow cx to be NULL
 */
char *
lm_StrToLocalEncoding(MWContext * context, JSString * str)
{
    INTL_Encoding_ID charset;
    INTL_Unicode * src = JS_GetStringChars(str);
    uint32 srclen = JS_GetStringLength(str);
    uint32 destlen;
    char * dest;

#ifdef UNICODE
    if (!str)
	return NULL;
/*	XP_ASSERT(CS_DEFAULT != INTL_GetCSIMetaDocCSID(LO_GetDocumentCharacterSetInfo(context))); */
    
    charset = INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(context));
    destlen = INTL_UnicodeToStrLen(charset, src, srclen);
    dest = XP_ALLOC(destlen);
    if (!dest) {
	JS_ReportOutOfMemory(context->mocha_context);
	return NULL;
    }

    INTL_UnicodeToStr(charset, src, srclen, (unsigned char *) dest, destlen);
    return dest;
#else
    return strdup(JS_GetStringBytes(str);
#endif

}

JSString *
lm_LocalEncodingToStr(MWContext * context, char * bytes)
{
    uint32 srclen, destlen;
    uint16 charset;
    INTL_Unicode * unicode = NULL;

#ifdef UNICODE
    /* return NULL or empty string? */
    if (!bytes)
	return JS_NewStringCopyZ(context->mocha_context, NULL); 

    srclen = XP_STRLEN(bytes);
    charset = INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(context));

    /* find out how many unicode characters we'll end up with */
    destlen = INTL_StrToUnicodeLen(charset, (unsigned char *) bytes);
    unicode = XP_ALLOC(sizeof(INTL_Unicode) * destlen);
    if (!unicode)
	return NULL;

    /* do the conversion */
    destlen = INTL_StrToUnicode(charset, (unsigned char *) bytes, 
			        unicode, destlen);
    return JS_NewUCString(context->mocha_context, (jschar *) unicode, destlen);
#else
    return JS_NewStringCopyZ(context->mocha_context, bytes);
#endif

}
