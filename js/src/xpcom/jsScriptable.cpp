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

jsScriptable::jsScriptable(jsIContext *cx, JSObject *object)
{
    /*    cx->addRoot(&obj); */
    /*    cx->addRoot(&className); */
    setJSObject(cx, object);
}

jsScriptable::~jsScriptable()
{
    /* cx->removeRoot(&obj); */
    /* cx->removeRoot(&className); */
}

JSObject *jsScriptable::getJSObject(jsIContext *cx)
{
    return obj;
}

void
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
}

JSString *
jsScriptable::getClassName()
{
    return className;
}

jsval
jsScriptable::get(jsval id)
{
    return JSVAL_VOID;
}

JSBool
jsScriptable::has(jsval id)
{
    return JS_FALSE;
}

void
jsScriptable::put(jsval id, jsval v)
{
}

void
jsScriptable::del(jsval id)
{
}

jsIScriptable *
jsScriptable::getPrototype()
{
    return proto;
}

void
jsScriptable::setPrototype(jsIScriptable *prototype)
{
    proto = prototype;
    /* set the prototype of the underlying JSObject * */
}

jsIScriptable *
jsScriptable::getParentScope()
{
    return parent;
}

void
jsScriptable::setParentScope(jsIScriptable *parent)
{
    this->parent = parent;
    /* set parent of underlying JSObject */
}

jsval
jsScriptable::getDefaultValue(JSType hint)
{
    return JSVAL_VOID;
}

JSBool
jsScriptable::hasInstance(jsIScriptable *instance)
{
    return JS_FALSE;
}
