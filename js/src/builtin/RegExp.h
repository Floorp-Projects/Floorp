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
MOZ_MUST_USE bool
ExecuteRegExpLegacy(JSContext* cx, RegExpStatics* res, RegExpObject& reobj,
                    HandleLinearString input, size_t* lastIndex, bool test,
                    MutableHandleValue rval);

/* Translation from MatchPairs to a JS array in regexp_exec()'s output format. */
MOZ_MUST_USE bool
CreateRegExpMatchResult(JSContext* cx, HandleString input, const MatchPairs& matches,
                        MutableHandleValue rval);

extern MOZ_MUST_USE bool
RegExpMatcher(JSContext* cx, unsigned argc, Value* vp);

extern MOZ_MUST_USE bool
RegExpMatcherRaw(JSContext* cx, HandleObject regexp, HandleString input,
                 int32_t lastIndex, MatchPairs* maybeMatches, MutableHandleValue output);

extern MOZ_MUST_USE bool
RegExpSearcher(JSContext* cx, unsigned argc, Value* vp);

extern MOZ_MUST_USE bool
RegExpSearcherRaw(JSContext* cx, HandleObject regexp, HandleString input,
                  int32_t lastIndex, MatchPairs* maybeMatches, int32_t* result);

extern MOZ_MUST_USE bool
RegExpTester(JSContext* cx, unsigned argc, Value* vp);

extern MOZ_MUST_USE bool
RegExpTesterRaw(JSContext* cx, HandleObject regexp, HandleString input,
                int32_t lastIndex, int32_t* endIndex);

extern MOZ_MUST_USE bool
intrinsic_GetElemBaseForLambda(JSContext* cx, unsigned argc, Value* vp);

extern MOZ_MUST_USE bool
intrinsic_GetStringDataProperty(JSContext* cx, unsigned argc, Value* vp);

/*
 * The following functions are for use by self-hosted code.
 */

/*
 * Behaves like regexp.exec(string), but doesn't set RegExp statics.
 *
 * Usage: match = regexp_exec_no_statics(regexp, string)
 */
extern MOZ_MUST_USE bool
regexp_exec_no_statics(JSContext* cx, unsigned argc, Value* vp);

/*
 * Behaves like regexp.test(string), but doesn't set RegExp statics.
 *
 * Usage: does_match = regexp_test_no_statics(regexp, string)
 */
extern MOZ_MUST_USE bool
regexp_test_no_statics(JSContext* cx, unsigned argc, Value* vp);

/*
 * Behaves like RegExp(pattern, flags).
 * |pattern| should be a RegExp object, |flags| should be a raw integer value.
 * Must be called without |new|.
 * Dedicated function for RegExp.prototype[@@split] optimized path.
 */
extern MOZ_MUST_USE bool
regexp_construct_raw_flags(JSContext* cx, unsigned argc, Value* vp);

/*
 * Clone given RegExp object, inheriting pattern and flags, ignoring other
 * properties.
 */
extern MOZ_MUST_USE bool
regexp_clone(JSContext* cx, unsigned argc, Value* vp);

extern MOZ_MUST_USE bool
IsRegExp(JSContext* cx, HandleValue value, bool* result);

extern MOZ_MUST_USE bool
RegExpCreate(JSContext* cx, HandleValue pattern, HandleValue flags, MutableHandleValue rval);

extern MOZ_MUST_USE bool
RegExpPrototypeOptimizable(JSContext* cx, unsigned argc, Value* vp);

extern MOZ_MUST_USE bool
RegExpPrototypeOptimizableRaw(JSContext* cx, JSObject* proto, uint8_t* result);

extern MOZ_MUST_USE bool
RegExpInstanceOptimizable(JSContext* cx, unsigned argc, Value* vp);

extern MOZ_MUST_USE bool
RegExpInstanceOptimizableRaw(JSContext* cx, JSObject* obj, JSObject* proto, uint8_t* result);

extern MOZ_MUST_USE bool
RegExpGetSubstitution(JSContext* cx, HandleLinearString matched, HandleLinearString string,
                      size_t position, HandleObject capturesObj, HandleLinearString replacement,
                      size_t firstDollarIndex, MutableHandleValue rval);

extern MOZ_MUST_USE bool
GetFirstDollarIndex(JSContext* cx, unsigned argc, Value* vp);

extern MOZ_MUST_USE bool
GetFirstDollarIndexRaw(JSContext* cx, HandleString str, int32_t* index);

extern int32_t
GetFirstDollarIndexRawFlat(JSLinearString* text);

// RegExp ClassSpec members used in RegExpObject.cpp.
extern MOZ_MUST_USE bool
regexp_construct(JSContext* cx, unsigned argc, Value* vp);
extern const JSPropertySpec regexp_static_props[];
extern const JSPropertySpec regexp_properties[];
extern const JSFunctionSpec regexp_methods[];

// Used in RegExpObject::isOriginalFlagGetter.
extern MOZ_MUST_USE bool
regexp_global(JSContext* cx, unsigned argc, JS::Value* vp);
extern MOZ_MUST_USE bool
regexp_ignoreCase(JSContext* cx, unsigned argc, JS::Value* vp);
extern MOZ_MUST_USE bool
regexp_multiline(JSContext* cx, unsigned argc, JS::Value* vp);
extern MOZ_MUST_USE bool
regexp_sticky(JSContext* cx, unsigned argc, JS::Value* vp);
extern MOZ_MUST_USE bool
regexp_unicode(JSContext* cx, unsigned argc, JS::Value* vp);

} /* namespace js */

#endif /* builtin_RegExp_h */
