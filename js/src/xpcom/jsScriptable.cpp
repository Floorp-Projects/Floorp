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

#include "jsScriptable.h"
#include "jsContext.h"

static NS_DEFINE_IID(kIScriptableIID, JS_ISCRIPTABLE_IID);

NS_IMPL_ISUPPORTS(jsScriptable, kIScriptableIID);

void
jsScriptable::init(jsIContext *icx, JSObject *object)
{
    jsContext *cx = (jsContext *)icx;
#if 0
    /* can't do this yet. see comment in ~jsScriptable. */
    cx->addRoot(&obj);
    cx->addRoot(&className);
#endif
    setJSObject(cx, object);
}

jsScriptable::jsScriptable(jsIContext *cx, JSObject *object)
{
    init(cx, object);
}

jsScriptable::jsScriptable(jsIContext *cx, JSClass *clasp)
{
    jsContext *icx = (jsContext *)cx;
    JSObject *obj = JS_NewObject(icx->cx, clasp, NULL, NULL);
    init(cx, obj);
}

jsScriptable::~jsScriptable()
{
    /* we need to remove the root, but we don't want to cache a
     * JSContext *, since that JSContext could easily go away.
     * We may need to reach into JSContext->rt and cache that, and
     * then find a context on that runtime to use for the call to
     * JS_RemoveRoot.  What do we do if there aren't any contexts on
     * that runtime at destructor time?  Do we create one?
     */
#if 0
    cx->removeRoot(&obj);
    cx->removeRoot(&className);
#endif
}

JSObject *jsScriptable::getJSObject(jsIContext *cx)
{
    return obj;
}

nsresult
jsScriptable::setJSObject(jsIContext *cx, JSObject *object)
{
    obj = object;
    jsContext *icx = (jsContext *)cx;

    className = JS_NewStringCopyZ(icx->cx,
#ifdef JS_THREADSAFE
				  JS_GetClass(icx->cx, object)
#else
				  JS_GetClass(object)
#endif
->name);

    /* XXX update proto and parent */
    return NS_OK;
}

JSString *
jsScriptable::getClassName(jsIContext *cx)
{
    return className;
}

nsresult
jsScriptable::get(jsIContext *cx, const char *name, jsval *vp)
{
    jsContext *icx = (jsContext *)cx;
    if (!JS_GetProperty(icx->cx, obj, name, vp)) {
	return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

nsresult
jsScriptable::has(jsIContext *cx, jsval id, JSBool *bp)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
jsScriptable::put(jsIContext *cx, const char *name, jsval v)
{
    jsContext *icx = (jsContext *)cx;
    if (!JS_SetProperty(icx->cx, obj, name, &v)) {
	return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

nsresult
jsScriptable::del(jsIContext *cx, jsval id)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

jsIScriptable *
jsScriptable::getPrototype(jsIContext *cx)
{
    return proto;
}

nsresult
jsScriptable::setPrototype(jsIContext *cx, jsIScriptable *prototype)
{
    proto = prototype;
    /* set the prototype of the underlying JSObject * */
    return NS_OK;
}

jsIScriptable *
jsScriptable::getParentScope(jsIContext *cx)
{
    return parent;
}

nsresult
jsScriptable::setParentScope(jsIContext *cx, jsIScriptable *parent)
{
    this->parent = parent;
    /* set parent of underlying JSObject */
    return NS_OK;
}

nsresult
jsScriptable::getDefaultValue(jsIContext *cx, JSType hint, jsval *vp)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
