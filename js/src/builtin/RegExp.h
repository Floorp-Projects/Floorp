/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RegExp_h___
#define RegExp_h___

#include "jsprvtd.h"

#include "vm/MatchPairs.h"
#include "vm/RegExpObject.h"

JSObject *
js_InitRegExpClass(JSContext *cx, js::HandleObject obj);

/*
 * The following builtin natives are extern'd for pointer comparison in
 * other parts of the engine.
 */

namespace js {

RegExpRunStatus
ExecuteRegExp(JSContext *cx, HandleObject regexp, HandleString string,
              MatchConduit &matches);

/*
 * Legacy behavior of ExecuteRegExp(), which is baked into the JSAPI.
 *
 * |res| may be NULL if the RegExpStatics are not to be updated.
 * |input| may be NULL if there is no JSString corresponding to
 * |chars| and |length|.
 */
bool
ExecuteRegExpLegacy(JSContext *cx, RegExpStatics *res, RegExpObject &reobj,
                    Handle<JSStableString*> input, StableCharPtr chars, size_t length,
                    size_t *lastIndex, JSBool test, jsval *rval);

/* Translation from MatchPairs to a JS array in regexp_exec()'s output format. */
bool
CreateRegExpMatchResult(JSContext *cx, HandleString string, MatchPairs &matches, Value *rval);

bool
CreateRegExpMatchResult(JSContext *cx, JSString *input_, StableCharPtr chars, size_t length,
                        MatchPairs &matches, Value *rval);

extern JSBool
regexp_exec(JSContext *cx, unsigned argc, Value *vp);

bool
regexp_test_raw(JSContext *cx, HandleObject regexp, HandleString input, JSBool *result);

extern JSBool
regexp_test(JSContext *cx, unsigned argc, Value *vp);

} /* namespace js */

#endif /* RegExp_h__ */
