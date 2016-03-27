/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_RegExp_h
#define builtin_RegExp_h

#include "vm/RegExpObject.h"

/*
 * The following builtin natives are extern'd for pointer comparison in
 * other parts of the engine.
 */

namespace js {

JSObject*
InitRegExpClass(JSContext* cx, HandleObject obj);

// Whether RegExp statics should be updated with the input and results of a
// regular expression execution.
enum RegExpStaticsUpdate { UpdateRegExpStatics, DontUpdateRegExpStatics };

/*
 * Legacy behavior of ExecuteRegExp(), which is baked into the JSAPI.
 *
 * |res| may be nullptr if the RegExpStatics are not to be updated.
 * |input| may be nullptr if there is no JSString corresponding to
 * |chars| and |length|.
 */
bool
ExecuteRegExpLegacy(JSContext* cx, RegExpStatics* res, RegExpObject& reobj,
                    HandleLinearString input, size_t* lastIndex, bool test,
                    MutableHandleValue rval);

/* Translation from MatchPairs to a JS array in regexp_exec()'s output format. */
bool
CreateRegExpMatchResult(JSContext* cx, HandleString input, const MatchPairs& matches,
                        MutableHandleValue rval);

extern bool
RegExpMatcher(JSContext* cx, unsigned argc, Value* vp);

extern bool
RegExpMatcherRaw(JSContext* cx, HandleObject regexp, HandleString input,
                 uint32_t lastIndex, bool sticky,
                 MatchPairs* maybeMatches, MutableHandleValue output);

extern bool
RegExpTester(JSContext* cx, unsigned argc, Value* vp);

extern bool
RegExpTesterRaw(JSContext* cx, HandleObject regexp, HandleString input,
                uint32_t lastIndex, bool sticky, int32_t* endIndex);

/*
 * The following functions are for use by self-hosted code.
 */

/*
 * Behaves like regexp.exec(string), but doesn't set RegExp statics.
 *
 * Usage: match = regexp_exec_no_statics(regexp, string)
 */
extern bool
regexp_exec_no_statics(JSContext* cx, unsigned argc, Value* vp);

/*
 * Behaves like regexp.test(string), but doesn't set RegExp statics.
 *
 * Usage: does_match = regexp_test_no_statics(regexp, string)
 */
extern bool
regexp_test_no_statics(JSContext* cx, unsigned argc, Value* vp);

/*
 * Behaves like RegExp(string) or RegExp(string, string), for self-hosted JS.
 * pattern and flags should be string, and should be called without |new|.
 *
 * Usage: re = regexp_construct(pattern)
 *        re = regexp_construct(pattern, flags)
 */
extern bool
regexp_construct_self_hosting(JSContext* cx, unsigned argc, Value* vp);

extern bool
IsRegExp(JSContext* cx, HandleValue value, bool* result);

extern bool
RegExpCreate(JSContext* cx, HandleValue pattern, HandleValue flags, MutableHandleValue rval);

extern bool
RegExpPrototypeOptimizable(JSContext* cx, unsigned argc, Value* vp);

extern bool
RegExpPrototypeOptimizableRaw(JSContext* cx, JSObject* proto, uint8_t* result);

extern bool
RegExpInstanceOptimizable(JSContext* cx, unsigned argc, Value* vp);

extern bool
RegExpInstanceOptimizableRaw(JSContext* cx, JSObject* rx, JSObject* proto, uint8_t* result);

// RegExp ClassSpec members used in RegExpObject.cpp.
extern bool
regexp_construct(JSContext* cx, unsigned argc, Value* vp);
extern const JSPropertySpec regexp_static_props[];
extern const JSPropertySpec regexp_properties[];
extern const JSFunctionSpec regexp_methods[];

// Used in RegExpObject::isOriginalFlagGetter.
extern bool
regexp_global(JSContext* cx, unsigned argc, JS::Value* vp);
extern bool
regexp_ignoreCase(JSContext* cx, unsigned argc, JS::Value* vp);
extern bool
regexp_multiline(JSContext* cx, unsigned argc, JS::Value* vp);
extern bool
regexp_sticky(JSContext* cx, unsigned argc, JS::Value* vp);
extern bool
regexp_unicode(JSContext* cx, unsigned argc, JS::Value* vp);

} /* namespace js */

#endif /* builtin_RegExp_h */
