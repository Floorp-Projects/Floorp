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
/* ** */
/*
 *  stand-alone glue for js<->java
 *  this is used as a default if the host does not override
 *   the glue.
 */

#if defined (JAVA)

#include "jsapi.h"
#include "jri.h"
#include "jriext.h"
#include "jsjava.h"
#ifndef NSPR20
#include "prglobal.h"
#endif
#include "prthread.h"
#include "prmon.h"
#include "prlog.h"
#include "prcmon.h"

#ifndef NSPR20
#define WAIT_FOREVER LL_MAXINT
#else
#define WAIT_FOREVER PR_INTERVAL_NO_TIMEOUT
#endif

static JSTaskState *jsjiTask = 0;
static PRMonitor *jsmon = NULL;


/*
 * callbacks to make js<->java glue work
 */

/**
  * Liveconnect callback. 
  * Called to determine if Java is enabled
  *
  */
PR_STATIC_CALLBACK(int)
sajsj_IsEnabled(void)
{
  return (int)PR_TRUE; 
}

static JSClass sajsj_global_class = {
    "global", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

static JSObject *globalObject = NULL;
static JSContext *globalContext = NULL;

/*
 *  callback to find the JSContext for a particular JRIEnv
 *  here, this just returns the only JSContext we have.
 *  in the client, it looks up the java call stack for one?
 */
PR_STATIC_CALLBACK(JSContext *)
sajsj_CurrentContext(JRIEnv *env, char **errp)
{
  *errp = 0;

  return globalContext;

}

PR_STATIC_CALLBACK(JRIEnv *)
sajsj_CurrentEnv(JSContext *cx)
{
    /* FIXME insurance? */
    JRIEnv *env = JRI_GetCurrentEnv();
    PR_ASSERT(env);
    return env;
}

/*
 * callbacks to maintain run-to-completion in the client
 * and thread-safety in the standalone java.
 */

PR_STATIC_CALLBACK(void)
sajsj_EnterJS(void)
{
    /* should ensure that only one thread can use the same
     * context / object space at once */
    PR_EnterMonitor(jsmon);
}

PR_STATIC_CALLBACK(void)
sajsj_ExitJS(void)
{
    PR_ExitMonitor(jsmon);
}


PR_STATIC_CALLBACK(JSPrincipals *)
sajsj_getJSPrincipalsFromJavaCaller(JSContext *cx, int callerDepth)
{
    return NULL;
}

PR_IMPLEMENT(JSObject *)
sajsj_GetDefaultObject(JRIEnv *env, jobject hint)
{
    return globalObject;
}

static JSJCallbacks sajsjCallbacks = {
    sajsj_IsEnabled,
    sajsj_CurrentEnv,
    sajsj_CurrentContext,
    sajsj_EnterJS,
    sajsj_ExitJS,
    NULL,
    sajsj_getJSPrincipalsFromJavaCaller,
    sajsj_GetDefaultObject,
};


static void
sajsj_InitLocked()
{
    JRIEnv *env;

    /* if jsj has already been initialized, we don't do this
     * standalone jsj setup, and none of the stuff in this
     * file gets used */
    if (!JSJ_Init(&sajsjCallbacks))
        return;

    jsjiTask = JS_Init(8L * 1024L * 1024L);
    jsmon = PR_NewMonitor();
    globalContext = JS_NewContext(jsjiTask, 8192);
    PR_ASSERT(globalContext);

    globalObject = NULL;
    JS_AddRoot(globalContext, &globalObject);
    globalObject = JS_NewObject(globalContext, &sajsj_global_class,
                                NULL, NULL);
    if (!globalObject) {
        PR_ASSERT(globalObject);
        goto trash_cx;
    }
    if (!JS_InitStandardClasses(globalContext, globalObject))
        goto trash_cx;
    if (!JSJ_InitContext(globalContext, globalObject))
        goto trash_cx;


    env = JRI_GetCurrentEnv();
    PR_ASSERT(env);


    return;
trash_cx:
    JS_DestroyContext(globalContext);   
    globalContext = NULL;

/* FIXME error codes? */
    return;
}

PR_IMPLEMENT(void)
JSJ_InitStandalone()
{
    static int initialized = -1;

    if (initialized == 1)
        return;

    PR_CEnterMonitor(&initialized);
    switch (initialized) {
      case -1:
	/* we're first */
	initialized = 0; /* in progress */
	PR_CExitMonitor(&initialized);

	/* do it */
        sajsj_InitLocked();

	PR_CEnterMonitor(&initialized);
	initialized = 1;
	PR_CNotifyAll(&initialized);
	break;
      case 0:
	/* in progress */
	PR_CWait(&initialized, WAIT_FOREVER);
	break;
      case 1:
	/* happened since the last check */
	break;
    }
    PR_CExitMonitor(&initialized);

    if (!jsjiTask)
	return;  /* FIXME fail loudly! */

    return;
}

#endif /* defined (JAVA) */