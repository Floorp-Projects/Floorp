/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RegExp_h___
#define RegExp_h___

#include "jsprvtd.h"

JSObject *
js_InitRegExpClass(JSContext *cx, JSObject *obj);

/*
 * The following builtin natives are extern'd for pointer comparison in
 * other parts of the engine.
 */

namespace js {

/*
 * |res| may be null if the |RegExpStatics| are not to be updated.
 * |input| may be null if there is no |JSString| corresponding to
 * |chars| and |length|.
 */
bool
ExecuteRegExp(JSContext *cx, RegExpStatics *res, RegExpObject &reobj,
              JSLinearString *input, const jschar *chars, size_t length,
              size_t *lastIndex, RegExpExecType type, Value *rval);

bool
ExecuteRegExp(JSContext *cx, RegExpStatics *res, RegExpShared &shared,
              JSLinearString *input, const jschar *chars, size_t length,
              size_t *lastIndex, RegExpExecType type, Value *rval);

extern JSBool
regexp_exec(JSContext *cx, unsigned argc, Value *vp);

extern JSBool
regexp_test(JSContext *cx, unsigned argc, Value *vp);

} /* namespace js */

#endif
