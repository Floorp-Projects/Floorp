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

#ifndef JS_RUNTIME_H
#define JS_RUNTIME_H

#include "jsIRuntime.h"
#include "jsIContext.h"
extern "C" {
#include <jsapi.h>
}

#include "jsContext.h"
class jsRuntime: public jsIRuntime {
    JSRuntime *rt;
    static int runtimeCount;

public:
    jsRuntime(uint32 maxbytes);
    ~jsRuntime();
    jsIContext *newContext(size_t stacksize);
    NS_DECL_ISUPPORTS
};

#endif /* JS_RUNTIME_H */
