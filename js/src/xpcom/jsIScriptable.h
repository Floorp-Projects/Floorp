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
 * jsIScriptable.h -- the XPCOM interface to native JavaScript objects.
 */

class jsIScriptable;		/* for mutually-dependent includes */

#ifndef JS_ISCRIPTABLE_H
#define JS_ISCRIPTABLE_H

#include "nsISupports.h"
#include "jsIContext.h"
#include <jsapi.h>

#define JS_ISCRIPTABLE_IID \
    { 0, 0, 0, \
	{0, 0, 0, 0, 0, 0, 0, 0}}

#define JSSCRIPTABLE_NOT_FOUND -1

class jsIScriptable: public nsISupports {
 public:
    virtual JSString *getClassName() = 0;

    /* XXX use jsid for name/index? */
    virtual jsval get(jsval id) = 0;
    virtual JSBool has(jsval id) = 0;
    virtual void put(jsval id, jsval v) = 0;
    virtual void del(jsval id) = 0;

    virtual jsIScriptable *getPrototype() = 0;
    virtual void setPrototype(jsIScriptable *prototype) = 0;
    virtual jsIScriptable *getParentScope() = 0;
    virtual void setParentScope(jsIScriptable *parent) = 0;
    /* virtual JSIdArray *getIds(); */
    virtual jsval getDefaultValue(JSType hint) = 0;
    virtual JSBool hasInstance(jsIScriptable *instance) = 0;

    /**
     * Return a classic JSAPI JSObject * for this object.
     * Return NULL if you don't know how to, and the engine will
     * construct a proxy for you.
     */
    virtual JSObject *getJSObject(jsIContext *) = 0;

    /**
     * Set the JSObject proxy for this object (typically one created
     * by the engine in response to a NULL return fron getJSObject).
     * Context is provided for GC rooting and other tasks.  Be sure
     * to unroot the proxy in your destructor, etc.
     */
    virtual void setJSObject(jsIContext *, JSObject *)= 0;
};

#endif /* JS_ISCRIPTABLE_H */
