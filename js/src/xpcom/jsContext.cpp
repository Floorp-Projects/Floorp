/* -*- Mode: cc; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * jsContext.cpp -- implementation of the jsIContext interface for the JSAPI.
 */

#include "jsIScriptable.h"
#include "jsContext.h"
#include "JSWrapper.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIContextIID, JS_ICONTEXT_IID);

NS_IMPL_ISUPPORTS(jsContext, kIContextIID);

static void
ErrorReporterHandler(JSContext *cx, const char* message,
		     JSErrorReport *report)
{
    JSString *str;
    jsContext *icx = (jsContext *)JS_GetContextPrivate(cx);
    if (!icx)
	/* well, we can't exactly report an error... */
	return;
    str = icx->newStringCopyZ(message);
    if (!str)
	return;
    icx->reporter->reportError(icx, str, report);
}

jsContext::jsContext (JSRuntime *rt, uintN stacksize)
{
    cx = JS_NewContext(rt, stacksize);
    reporter = NULL;
    if (cx) {
	JS_SetErrorReporter(cx, ::ErrorReporterHandler);
	JS_SetContextPrivate(cx, (void *)this);
    }
    /*
     * jsRuntime checks this->cx to see if we're valid when creating
     * contexts via the factory method jsRuntime::newContext.
     */
}

jsContext::~jsContext()
{
    JS_DestroyContext(cx);
}

JSContext *
jsContext::GetJS()
{
    return cx;
}

jsIFunction *
jsContext::compileFunction(jsIScriptable *scope,
			   JSString *source,
			   JSString *sourceName,
			   int lineno)
{
    /* NS_ERROR_NOT_IMPLEMENTED */
    return NULL;
}

JSString *
jsContext::decompileScript(jsIScript *script,
			   jsIScriptable *scope,
			   int indent)
{
    /* NS_ERROR_NOT_IMPLEMENTED */
    return NULL;
}

JSString *
jsContext::decompileFunction(jsIFunction *fun,
			     int indent)
{
    /* NS_ERROR_NOT_IMPLEMENTED */
    return NULL;
}

JSString *
jsContext::decompileFunctionBody(jsIFunction *fun,
				 int indent)
{
    /* NS_ERROR_NOT_IMPLEMENTED */
    return NULL;
}

jsIScriptable *
jsContext::newObject(jsIScriptable *scope)
{
    return newObject(scope, NULL, NULL, 0);
}

jsIScriptable *
jsContext::newObject(jsIScriptable *scope,
		     JSString *constructorName)
{
    return newObject(scope, constructorName, NULL, 0);
}

jsIScriptable *
jsContext::newObject(jsIScriptable *scope,
		     JSString *constructorName,
		     jsval *argv,
		     uintN argc)
{
    /* NS_ERROR_NOT_IMPLEMENTED */
    return NULL;
}

jsIScriptable *
jsContext::newArray(jsIScriptable *scope)
{
    return newArray(scope, NULL, 0);
}

jsIScriptable *
jsContext::newArray(jsIScriptable *scope,
		    uintN length)
{
    return newArray(scope, NULL, length);
}

jsIScriptable *
jsContext::newArray(jsIScriptable *scope,
		    jsval *elements,
		    uintN length)
{
    /* NS_ERROR_NOT_IMPLEMENTED */
    return NULL;
}

nsresult
jsContext::toBoolean(jsval v, JSBool *bp)
{
    if (!JS_ValueToBoolean(cx, v, bp))
	return NS_ERROR_FAILURE;
    return NS_OK;
}

jsdouble *
jsContext::toNumber(jsval v)
{
    jsdouble d;
    if (!JS_ValueToNumber(cx, v, &d))
	return NULL;
    return JS_NewDouble(cx, d);
}

JSString *
jsContext::toString(jsval v)
{
    return JS_ValueToString(cx, v);
}

jsIScriptable *
jsContext::toScriptable(jsval v, jsIScriptable *scope)
{
    /* NS_ERROR_NOT_IMPLEMENTED */
    return NULL;
}

JSObject *jsScriptableCreateJSObjectProxy(jsIContext *cx, jsIScriptable *scope)
{
    JSObject *obj = scope->GetJS();
    if (!obj) {
	/* XXX construct a proxy object */
	return NULL;
    }
    return obj;
}

jsIScriptable *jsScriptableCreateFromJSObject(jsIContext *cx, JSObject *obj)
{
    return new JSWrapper(cx->GetJS(), obj);
}

nsresult
jsContext::evaluateString(jsIScriptable *scope,
			  JSString *source,
			  JSString *sourceName,
			  uintN lineno,
			  jsval *rval)
{
    JSObject *obj = scope->GetJS();
    if (!obj) 
	return NS_ERROR_FAILURE;
    if (!JS_EvaluateScript(cx, obj, JS_GetStringBytes(source), 
			   JS_GetStringLength(source),
			   JS_GetStringBytes(sourceName),
			   JS_GetStringLength(sourceName),
			   rval))
	return NS_ERROR_FAILURE;
    return NS_OK;
}

nsresult
jsContext::initStandardObjects(jsIScriptable *scope)
{
    JSObject *obj = scope->GetJS();
    if (!obj) 
	return NS_ERROR_FAILURE;
    if (!JS_InitStandardClasses(cx, obj))
	return NS_ERROR_FAILURE;
    return NS_OK;
}

void
jsContext::reportWarning(JSString *message)
{
    /* XXX fill in report, make varargs */
    if (reporter)
	reporter->reportWarning(this, message, NULL);
};

void
jsContext::reportError(JSString *message)
{
    /* XXX */
    JS_ReportError(cx, JS_GetStringBytes(message));
#if 0
    if (reporter)
	reporter->reportError(message, NULL);
#endif
}

jsIErrorReporter *
jsContext::setErrorReporter(jsIErrorReporter *er)
{
    jsIErrorReporter *older = reporter;
    reporter = er;
    return older;
}

uintN
jsContext::setLanguageVersion(uintN version)
{
    return (uintN)JS_SetVersion(cx, (JSVersion)version);
}

uintN
jsContext::getLanguageVersion()
{
    return JS_GetVersion(cx);
}

void
jsContext::enter()
{
#ifdef JS_THREADSAFE
    (void)JS_SetContextThread(cx);
#endif
}

void
jsContext::exit()
{
#ifdef JS_THREADSAFE
    (void)JS_ClearContextThread(cx);
#endif
}

JSString *
jsContext::newStringCopyZ(const char *string)
{
    return JS_NewStringCopyZ(cx, string);
}

JSString *
jsContext::newUCStringCopyZ(const jschar *string)
{
    return JS_NewUCStringCopyZ(cx, string);
}

JSString *
jsContext::newStringCopyN(const char *string, size_t len)
{
    return JS_NewStringCopyN(cx, string, len);
}

JSString *
jsContext::newUCStringCopyN(const jschar *string, size_t len)
{
    return JS_NewUCStringCopyN(cx, string, len);
}

JSString *
jsContext::newString(char *string, size_t len)
{
    return JS_NewString(cx, string, len);
}

JSString *
jsContext::newUCString(jschar *string, size_t len)
{
    return JS_NewUCString(cx, string, len);
}

JSBool
jsContext::addRoot(void *root)
{
    return JS_AddRoot(cx, root);
}

JSBool
jsContext::addNamedRoot(void *root, const char *name)
{
    return JS_AddNamedRoot(cx, root, name);
}

JSBool
jsContext::removeRoot(void *root)
{
    return JS_RemoveRoot(cx, root);
}
