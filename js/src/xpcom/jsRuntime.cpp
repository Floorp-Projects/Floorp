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
 * jsRuntime.cpp -- implementation of the jsIContext interface for the JSAPI.
 */

#include "jsRuntime.h"

static NS_DEFINE_IID(kIRuntime, JS_IRUNTIME_IID);

int jsRuntime::runtimeCount = 0;

jsRuntime::jsRuntime(uint32 maxbytes)
{
    rt = JS_NewRuntime(maxbytes);
    if (rt)
	runtimeCount++;
    /* and what if it _doesn't_ work? */
}

jsRuntime::~jsRuntime()
{
    JS_DestroyRuntime(rt);
    if (!--runtimeCount) {
	JS_ShutDown();
    }
}

NS_IMPL_ISUPPORTS(jsRuntime, kIRuntime);

jsIContext *jsRuntime::newContext(size_t stacksize)
{
    jsContext *cx = new jsContext(rt, stacksize);
    if (cx->cx)
	return cx;
    return NULL;
}
