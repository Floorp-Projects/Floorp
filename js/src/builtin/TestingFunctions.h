/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_TestingFunctions_h
#define builtin_TestingFunctions_h

#include "jsapi.h"

namespace js {

bool
DefineTestingFunctions(JSContext *cx, HandleObject obj);

JSBool
testingFunc_inParallelSection(JSContext *cx, unsigned argc, jsval *vp);

JSBool
testingFunc_bailout(JSContext *cx, unsigned argc, jsval *vp);

} /* namespace js */

#endif /* builtin_TestingFunctions_h */
