/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/RegExp.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/TypeTraits.h"

#include "jscntxt.h"

#include "irregexp/RegExpParser.h"
#include "jit/InlinableNatives.h"
#include "vm/RegExpStatics.h"
#include "vm/StringBuffer.h"
#include "vm/Unicode.h"

#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::unicode;

using mozilla::ArrayLength;
using mozilla::CheckedInt;
using mozilla::Maybe;

/*
 * ES 2017 draft rev 6a13789aa9e7c6de4e96b7d3e24d9e6eba6584ad 21.2.5.2.2
 * steps 3, 16-25.
 */
bool
js::CreateRegExpMatchResult(JSContext* cx, HandleString input, const MatchPairs& matches,
                            MutableHandleValue rval)
{
    MOZ_ASSERT(input);

    /*
     * Create the (slow) result array for a match.
     *
     * Array contents:
     *  0:              matched string
     *  1..pairCount-1: paren matches
     *  input:          input string
     *  index:          start index for the match
     */

    /* Get the templateObject that defines the shape and type of the output object */
    JSObject* templateObject = cx->compartment()->regExps.getOrCreateMatchResultTemplateObject(cx);
    if (!templateObject)
        return false;

    size_t numPairs = matches.length();
    MOZ_ASSERT(numPairs > 0);

    /* Step 17. */
    RootedArrayObject arr(cx, NewDenseFullyAllocatedArrayWithTemplate(cx, numPairs, templateObject));
    if (!arr)
        return false;

    /* Steps 22-24.
     * Store a Value for each pair. */
    for (size_t i = 0; i < numPairs; i++) {
        const MatchPair& pair = matches[i];

        if (pair.isUndefined()) {
            MOZ_ASSERT(i != 0); /* Since we had a match, first pair must be present. */
            arr->setDenseInitializedLength(i + 1);
            arr->initDenseElement(i, UndefinedValue());
        } else {
            JSLinearString* str = NewDependentString(cx, input, pair.start, pair.length());
            if (!str)
                return false;
            arr->setDenseInitializedLength(i + 1);
            arr->initDenseElement(i, StringValue(str));
        }
    }

    /* Step 20 (reordered).
     * Set the |index| property. (TemplateObject positions it in slot 0) */
    arr->setSlot(0, Int32Value(matches[0].start));

    /* Step 21 (reordered).
     * Set the |input| property. (TemplateObject positions it in slot 1) */
    arr->setSlot(1, StringValue(input));

#ifdef DEBUG
    RootedValue test(cx);
    RootedId id(cx, NameToId(cx->names().index));
    if (!NativeGetProperty(cx, arr, id, &test))
        return false;
    MOZ_ASSERT(test == arr->getSlot(0));
    id = NameToId(cx->names().input);
    if (!NativeGetProperty(cx, arr, id, &test))
        return false;
    MOZ_ASSERT(test == arr->getSlot(1));
#endif

    /* Step 25. */
    rval.setObject(*arr);
    return true;
}

static int32_t
CreateRegExpSearchResult(JSContext* cx, const MatchPairs& matches)
{
    /* Fit the start and limit of match into a int32_t. */
    uint32_t position = matches[0].start;
    uint32_t lastIndex = matches[0].limit;
    MOZ_ASSERT(position < 0x8000);
    MOZ_ASSERT(lastIndex < 0x8000);
    return position | (lastIndex << 15);
}

/*
 * ES 2017 draft rev 6a13789aa9e7c6de4e96b7d3e24d9e6eba6584ad 21.2.5.2.2
 * steps 3, 9-14, except 12.a.i, 12.c.i.1.
 */
static RegExpRunStatus
ExecuteRegExpImpl(JSContext* cx, RegExpStatics* res, RegExpShared& re, HandleLinearString input,
                  size_t searchIndex, MatchPairs* matches, size_t* endIndex)
{
    RegExpRunStatus status = re.execute(cx, input, searchIndex, matches, endIndex);

    /* Out of spec: Update RegExpStatics. */
    if (status == RegExpRunStatus_Success && res) {
        if (matches) {
            if (!res->updateFromMatchPairs(cx, input, *matches))
                return RegExpRunStatus_Error;
        } else {
            res->updateLazily(cx, input, &re, searchIndex);
        }
    }
    return status;
}

/* Legacy ExecuteRegExp behavior is baked into the JSAPI. */
bool
js::ExecuteRegExpLegacy(JSContext* cx, RegExpStatics* res, RegExpObject& reobj,
                        HandleLinearString input, size_t* lastIndex, bool test,
                        MutableHandleValue rval)
{
    RegExpGuard shared(cx);
    if (!reobj.getShared(cx, &shared))
        return false;

    ScopedMatchPairs matches(&cx->tempLifoAlloc());

    RegExpRunStatus status = ExecuteRegExpImpl(cx, res, *shared, input, *lastIndex,
                                               &matches, nullptr);
    if (status == RegExpRunStatus_Error)
        return false;

    if (status == RegExpRunStatus_Success_NotFound) {
        /* ExecuteRegExp() previously returned an array or null. */
        rval.setNull();
        return true;
    }

    *lastIndex = matches[0].limit;

    if (test) {
        /* Forbid an array, as an optimization. */
        rval.setBoolean(true);
        return true;
    }

    return CreateRegExpMatchResult(cx, input, matches, rval);
}

static bool
CheckPatternSyntax(JSContext* cx, HandleAtom pattern, RegExpFlag flags)
{
    CompileOptions options(cx);
    frontend::TokenStream dummyTokenStream(cx, options, nullptr, 0, nullptr);
    return irregexp::ParsePatternSyntax(dummyTokenStream, cx->tempLifoAlloc(), pattern,
                                        flags & UnicodeFlag);
}

enum RegExpSharedUse {
    UseRegExpShared,
    DontUseRegExpShared
};

/*
 * ES 2016 draft Mar 25, 2016 21.2.3.2.2.
 *
 * Steps 14-15 set |obj|'s "lastIndex" property to zero.  Some of
 * RegExpInitialize's callers have a fresh RegExp not yet exposed to script:
 * in these cases zeroing "lastIndex" is infallible.  But others have a RegExp
 * whose "lastIndex" property might have been made non-writable: here, zeroing
 * "lastIndex" can fail.  We efficiently solve this problem by completely
 * removing "lastIndex" zeroing from the provided function.
 *
 * CALLERS MUST HANDLE "lastIndex" ZEROING THEMSELVES!
 *
 * Because this function only ever returns a user-provided |obj| in the spec,
 * we omit it and just return the usual success/failure.
 */
static bool
RegExpInitializeIgnoringLastIndex(JSContext* cx, Handle<RegExpObject*> obj,
                                  HandleValue patternValue, HandleValue flagsValue,
                                  RegExpSharedUse sharedUse = DontUseRegExpShared)
{
    RootedAtom pattern(cx);
    if (patternValue.isUndefined()) {
        /* Step 1. */
        pattern = cx->names().empty;
    } else {
        /* Step 2. */
        pattern = ToAtom<CanGC>(cx, patternValue);
        if (!pattern)
            return false;
    }

    /* Step 3. */
    RegExpFlag flags = RegExpFlag(0);
    if (!flagsValue.isUndefined()) {
        /* Step 4. */
        RootedString flagStr(cx, ToString<CanGC>(cx, flagsValue));
        if (!flagStr)
            return false;

        /* Step 5. */
        if (!ParseRegExpFlags(cx, flagStr, &flags))
            return false;
    }

    if (sharedUse == UseRegExpShared) {
        /* Steps 7-8. */
        RegExpGuard re(cx);
        if (!cx->compartment()->regExps.get(cx, pattern, flags, &re))
            return false;

        /* Steps 9-12. */
        obj->initIgnoringLastIndex(pattern, flags);

        obj->setShared(*re);
    } else {
        /* Steps 7-8. */
        if (!CheckPatternSyntax(cx, pattern, flags))
            return false;

        /* Steps 9-12. */
        obj->initIgnoringLastIndex(pattern, flags);
    }

    return true;
}

/* ES 2016 draft Mar 25, 2016 21.2.3.2.3. */
bool
js::RegExpCreate(JSContext* cx, HandleValue patternValue, HandleValue flagsValue,
                 MutableHandleValue rval)
{
    /* Step 1. */
    Rooted<RegExpObject*> regexp(cx, RegExpAlloc(cx));
    if (!regexp)
         return false;

    /* Step 2. */
    if (!RegExpInitializeIgnoringLastIndex(cx, regexp, patternValue, flagsValue, UseRegExpShared))
        return false;
    regexp->zeroLastIndex(cx);

    rval.setObject(*regexp);
    return true;
}

MOZ_ALWAYS_INLINE bool
IsRegExpObject(HandleValue v)
{
    return v.isObject() && v.toObject().is<RegExpObject>();
}

/* ES6 draft rc3 7.2.8. */
bool
js::IsRegExp(JSContext* cx, HandleValue value, bool* result)
{
    /* Step 1. */
    if (!value.isObject()) {
        *result = false;
        return true;
    }
    RootedObject obj(cx, &value.toObject());

    /* Steps 2-3. */
    RootedValue isRegExp(cx);
    RootedId matchId(cx, SYMBOL_TO_JSID(cx->wellKnownSymbols().match));
    if (!GetProperty(cx, obj, obj, matchId, &isRegExp))
        return false;

    /* Step 4. */
    if (!isRegExp.isUndefined()) {
        *result = ToBoolean(isRegExp);
        return true;
    }

    /* Steps 5-6. */
    ESClass cls;
    if (!GetClassOfValue(cx, value, &cls))
        return false;

    *result = cls == ESClass::RegExp;
    return true;
}

/* ES6 B.2.5.1. */
MOZ_ALWAYS_INLINE bool
regexp_compile_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsRegExpObject(args.thisv()));

    Rooted<RegExpObject*> regexp(cx, &args.thisv().toObject().as<RegExpObject>());

    // Step 3.
    RootedValue patternValue(cx, args.get(0));
    ESClass cls;
    if (!GetClassOfValue(cx, patternValue, &cls))
        return false;
    if (cls == ESClass::RegExp) {
        // Step 3a.
        if (args.hasDefined(1)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NEWREGEXP_FLAGGED);
            return false;
        }

        // Beware!  |patternObj| might be a proxy into another compartment, so
        // don't assume |patternObj.is<RegExpObject>()|.  For the same reason,
        // don't reuse the RegExpShared below.
        RootedObject patternObj(cx, &patternValue.toObject());

        RootedAtom sourceAtom(cx);
        RegExpFlag flags;
        {
            // Step 3b.
            RegExpGuard g(cx);
            if (!RegExpToShared(cx, patternObj, &g))
                return false;

            sourceAtom = g->getSource();
            flags = g->getFlags();
        }

        // Step 5, minus lastIndex zeroing.
        regexp->initIgnoringLastIndex(sourceAtom, flags);
    } else {
        // Step 4.
        RootedValue P(cx, patternValue);
        RootedValue F(cx, args.get(1));

        // Step 5, minus lastIndex zeroing.
        if (!RegExpInitializeIgnoringLastIndex(cx, regexp, P, F))
            return false;
    }

    // The final niggling bit of step 5.
    //
    // |regexp| is user-exposed, but if its "lastIndex" property hasn't been
    // made non-writable, we can still use a fast path to zero it.
    if (regexp->lookupPure(cx->names().lastIndex)->writable()) {
        regexp->zeroLastIndex(cx);
    } else {
        RootedValue zero(cx, Int32Value(0));
        if (!SetProperty(cx, regexp, cx->names().lastIndex, zero))
            return false;
    }

    args.rval().setObject(*regexp);
    return true;
}

static bool
regexp_compile(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Steps 1-2. */
    return CallNonGenericMethod<IsRegExpObject, regexp_compile_impl>(cx, args);
}

/*
 * ES 2017 draft rev 6a13789aa9e7c6de4e96b7d3e24d9e6eba6584ad 21.2.3.1.
 */
bool
js::regexp_construct(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Steps 1.
    bool patternIsRegExp;
    if (!IsRegExp(cx, args.get(0), &patternIsRegExp))
        return false;

    // We can delay step 3 and step 4a until later, during
    // GetPrototypeFromCallableConstructor calls. Accessing the new.target
    // and the callee from the stack is unobservable.
    if (!args.isConstructing()) {
        // Step 3.b.
        if (patternIsRegExp && !args.hasDefined(1)) {
            RootedObject patternObj(cx, &args[0].toObject());

            // Step 3.b.i.
            RootedValue patternConstructor(cx);
            if (!GetProperty(cx, patternObj, patternObj, cx->names().constructor, &patternConstructor))
                return false;

            // Step 3.b.ii.
            if (patternConstructor.isObject() && patternConstructor.toObject() == args.callee()) {
                args.rval().set(args[0]);
                return true;
            }
        }
    }

    RootedValue patternValue(cx, args.get(0));

    // Step 4.
    ESClass cls;
    if (!GetClassOfValue(cx, patternValue, &cls))
        return false;
    if (cls == ESClass::RegExp) {
        // Beware!  |patternObj| might be a proxy into another compartment, so
        // don't assume |patternObj.is<RegExpObject>()|.  For the same reason,
        // don't reuse the RegExpShared below.
        RootedObject patternObj(cx, &patternValue.toObject());

        RootedAtom sourceAtom(cx);
        RegExpFlag flags;
        {
            // Step 4.a.
            RegExpGuard g(cx);
            if (!RegExpToShared(cx, patternObj, &g))
                return false;
            sourceAtom = g->getSource();

            // Step 4.b.
            // Get original flags in all cases, to compare with passed flags.
            flags = g->getFlags();
        }

        // Step 7.
        RootedObject proto(cx);
        if (!GetPrototypeFromCallableConstructor(cx, args, &proto))
            return false;

        Rooted<RegExpObject*> regexp(cx, RegExpAlloc(cx, proto));
        if (!regexp)
            return false;

        // Step 8.
        if (args.hasDefined(1)) {
            // Step 4.c / 21.2.3.2.2 RegExpInitialize step 4.
            RegExpFlag flagsArg = RegExpFlag(0);
            RootedString flagStr(cx, ToString<CanGC>(cx, args[1]));
            if (!flagStr)
                return false;
            if (!ParseRegExpFlags(cx, flagStr, &flagsArg))
                return false;

            if (!(flags & UnicodeFlag) && flagsArg & UnicodeFlag) {
                // Have to check syntax again when adding 'u' flag.

                // ES 2017 draft rev 9b49a888e9dfe2667008a01b2754c3662059ae56
                // 21.2.3.2.2 step 7.
                if (!CheckPatternSyntax(cx, sourceAtom, flagsArg))
                    return false;
            }
            flags = flagsArg;
        }

        regexp->initAndZeroLastIndex(sourceAtom, flags, cx);

        args.rval().setObject(*regexp);
        return true;
    }

    RootedValue P(cx);
    RootedValue F(cx);

    // Step 5.
    if (patternIsRegExp) {
        RootedObject patternObj(cx, &patternValue.toObject());

        // Step 5.a.
        if (!GetProperty(cx, patternObj, patternObj, cx->names().source, &P))
            return false;

        // Step 5.b.
        F = args.get(1);
        if (F.isUndefined()) {
            if (!GetProperty(cx, patternObj, patternObj, cx->names().flags, &F))
                return false;
        }
    } else {
        // Steps 6.a-b.
        P = patternValue;
        F = args.get(1);
    }

    // Step 7.
    RootedObject proto(cx);
    if (!GetPrototypeFromCallableConstructor(cx, args, &proto))
        return false;

    Rooted<RegExpObject*> regexp(cx, RegExpAlloc(cx, proto));
    if (!regexp)
        return false;

    // Step 8.
    if (!RegExpInitializeIgnoringLastIndex(cx, regexp, P, F))
        return false;
    regexp->zeroLastIndex(cx);

    args.rval().setObject(*regexp);
    return true;
}

/*
 * ES 2017 draft rev 6a13789aa9e7c6de4e96b7d3e24d9e6eba6584ad 21.2.3.1
 * steps 4, 7-8.
 * Ignore sticky flag of flags argument, for optimized path in @@split.
 */
bool
js::regexp_construct_no_sticky(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(!args.isConstructing());

    Rooted<RegExpObject*> rx(cx, &args[0].toObject().as<RegExpObject>());

    // Step 4.a.
    RootedAtom sourceAtom(cx, rx->getSource());

    // Step 4.c.
    RootedString flagStr(cx, args[1].toString());
    RegExpFlag flags = RegExpFlag(0);
    if (!ParseRegExpFlags(cx, flagStr, &flags))
        return false;

    // Ignore sticky flag.
    flags = RegExpFlag(flags & ~StickyFlag);

    // Step 7.
    Rooted<RegExpObject*> regexp(cx, RegExpAlloc(cx));
    if (!regexp)
        return false;

    // Step 8.
    regexp->initAndZeroLastIndex(sourceAtom, flags, cx);
    args.rval().setObject(*regexp);
    return true;
}

/* ES6 draft rev32 21.2.5.4. */
MOZ_ALWAYS_INLINE bool
regexp_global_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsRegExpObject(args.thisv()));
    Rooted<RegExpObject*> reObj(cx, &args.thisv().toObject().as<RegExpObject>());

    /* Steps 4-6. */
    args.rval().setBoolean(reObj->global());
    return true;
}

bool
js::regexp_global(JSContext* cx, unsigned argc, JS::Value* vp)
{
    /* Steps 1-3. */
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsRegExpObject, regexp_global_impl>(cx, args);
}

/* ES6 draft rev32 21.2.5.5. */
MOZ_ALWAYS_INLINE bool
regexp_ignoreCase_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsRegExpObject(args.thisv()));
    Rooted<RegExpObject*> reObj(cx, &args.thisv().toObject().as<RegExpObject>());

    /* Steps 4-6. */
    args.rval().setBoolean(reObj->ignoreCase());
    return true;
}

bool
js::regexp_ignoreCase(JSContext* cx, unsigned argc, JS::Value* vp)
{
    /* Steps 1-3. */
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsRegExpObject, regexp_ignoreCase_impl>(cx, args);
}

/* ES6 draft rev32 21.2.5.7. */
MOZ_ALWAYS_INLINE bool
regexp_multiline_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsRegExpObject(args.thisv()));
    Rooted<RegExpObject*> reObj(cx, &args.thisv().toObject().as<RegExpObject>());

    /* Steps 4-6. */
    args.rval().setBoolean(reObj->multiline());
    return true;
}

bool
js::regexp_multiline(JSContext* cx, unsigned argc, JS::Value* vp)
{
    /* Steps 1-3. */
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsRegExpObject, regexp_multiline_impl>(cx, args);
}

/* ES6 draft rev32 21.2.5.10. */
MOZ_ALWAYS_INLINE bool
regexp_source_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsRegExpObject(args.thisv()));
    Rooted<RegExpObject*> reObj(cx, &args.thisv().toObject().as<RegExpObject>());

    /* Step 5. */
    RootedAtom src(cx, reObj->getSource());
    if (!src)
        return false;

    /* Step 7. */
    RootedString str(cx, EscapeRegExpPattern(cx, src));
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

static bool
regexp_source(JSContext* cx, unsigned argc, JS::Value* vp)
{
    /* Steps 1-4. */
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsRegExpObject, regexp_source_impl>(cx, args);
}

/* ES6 draft rev32 21.2.5.12. */
MOZ_ALWAYS_INLINE bool
regexp_sticky_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsRegExpObject(args.thisv()));
    Rooted<RegExpObject*> reObj(cx, &args.thisv().toObject().as<RegExpObject>());

    /* Steps 4-6. */
    args.rval().setBoolean(reObj->sticky());
    return true;
}

bool
js::regexp_sticky(JSContext* cx, unsigned argc, JS::Value* vp)
{
    /* Steps 1-3. */
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsRegExpObject, regexp_sticky_impl>(cx, args);
}

/* ES6 21.2.5.15. */
MOZ_ALWAYS_INLINE bool
regexp_unicode_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsRegExpObject(args.thisv()));
    /* Steps 4-6. */
    args.rval().setBoolean(args.thisv().toObject().as<RegExpObject>().unicode());
    return true;
}

bool
js::regexp_unicode(JSContext* cx, unsigned argc, JS::Value* vp)
{
    /* Steps 1-3. */
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsRegExpObject, regexp_unicode_impl>(cx, args);
}

const JSPropertySpec js::regexp_properties[] = {
    JS_SELF_HOSTED_GET("flags", "RegExpFlagsGetter", 0),
    JS_PSG("global", regexp_global, 0),
    JS_PSG("ignoreCase", regexp_ignoreCase, 0),
    JS_PSG("multiline", regexp_multiline, 0),
    JS_PSG("source", regexp_source, 0),
    JS_PSG("sticky", regexp_sticky, 0),
    JS_PSG("unicode", regexp_unicode, 0),
    JS_PS_END
};

const JSFunctionSpec js::regexp_methods[] = {
#if JS_HAS_TOSOURCE
    JS_SELF_HOSTED_FN(js_toSource_str, "RegExpToString", 0, 0),
#endif
    JS_SELF_HOSTED_FN(js_toString_str, "RegExpToString", 0, 0),
    JS_FN("compile",        regexp_compile,     2,0),
    JS_SELF_HOSTED_FN("exec", "RegExp_prototype_Exec", 1,0),
    JS_SELF_HOSTED_FN("test", "RegExpTest" ,    1,0),
    JS_SELF_HOSTED_SYM_FN(match, "RegExpMatch", 1,0),
    JS_SELF_HOSTED_SYM_FN(replace, "RegExpReplace", 2,0),
    JS_SELF_HOSTED_SYM_FN(search, "RegExpSearch", 1,0),
    JS_SELF_HOSTED_SYM_FN(split, "RegExpSplit", 2,0),
    JS_FS_END
};

#define STATIC_PAREN_GETTER_CODE(parenNum)                                      \
    if (!res->createParen(cx, parenNum, args.rval()))                           \
        return false;                                                           \
    if (args.rval().isUndefined())                                              \
        args.rval().setString(cx->runtime()->emptyString);                      \
    return true

/*
 * RegExp static properties.
 *
 * RegExp class static properties and their Perl counterparts:
 *
 *  RegExp.input                $_
 *  RegExp.lastMatch            $&
 *  RegExp.lastParen            $+
 *  RegExp.leftContext          $`
 *  RegExp.rightContext         $'
 */

#define DEFINE_STATIC_GETTER(name, code)                                        \
    static bool                                                                 \
    name(JSContext* cx, unsigned argc, Value* vp)                               \
    {                                                                           \
        CallArgs args = CallArgsFromVp(argc, vp);                               \
        RegExpStatics* res = cx->global()->getRegExpStatics(cx);                \
        if (!res)                                                               \
            return false;                                                       \
        code;                                                                   \
    }

DEFINE_STATIC_GETTER(static_input_getter,        return res->createPendingInput(cx, args.rval()))
DEFINE_STATIC_GETTER(static_lastMatch_getter,    return res->createLastMatch(cx, args.rval()))
DEFINE_STATIC_GETTER(static_lastParen_getter,    return res->createLastParen(cx, args.rval()))
DEFINE_STATIC_GETTER(static_leftContext_getter,  return res->createLeftContext(cx, args.rval()))
DEFINE_STATIC_GETTER(static_rightContext_getter, return res->createRightContext(cx, args.rval()))

DEFINE_STATIC_GETTER(static_paren1_getter,       STATIC_PAREN_GETTER_CODE(1))
DEFINE_STATIC_GETTER(static_paren2_getter,       STATIC_PAREN_GETTER_CODE(2))
DEFINE_STATIC_GETTER(static_paren3_getter,       STATIC_PAREN_GETTER_CODE(3))
DEFINE_STATIC_GETTER(static_paren4_getter,       STATIC_PAREN_GETTER_CODE(4))
DEFINE_STATIC_GETTER(static_paren5_getter,       STATIC_PAREN_GETTER_CODE(5))
DEFINE_STATIC_GETTER(static_paren6_getter,       STATIC_PAREN_GETTER_CODE(6))
DEFINE_STATIC_GETTER(static_paren7_getter,       STATIC_PAREN_GETTER_CODE(7))
DEFINE_STATIC_GETTER(static_paren8_getter,       STATIC_PAREN_GETTER_CODE(8))
DEFINE_STATIC_GETTER(static_paren9_getter,       STATIC_PAREN_GETTER_CODE(9))

#define DEFINE_STATIC_SETTER(name, code)                                        \
    static bool                                                                 \
    name(JSContext* cx, unsigned argc, Value* vp)                               \
    {                                                                           \
        RegExpStatics* res = cx->global()->getRegExpStatics(cx);                \
        if (!res)                                                               \
            return false;                                                       \
        code;                                                                   \
        return true;                                                            \
    }

static bool
static_input_setter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RegExpStatics* res = cx->global()->getRegExpStatics(cx);
    if (!res)
        return false;

    RootedString str(cx, ToString<CanGC>(cx, args.get(0)));
    if (!str)
        return false;

    res->setPendingInput(str);
    args.rval().setString(str);
    return true;
}

const JSPropertySpec js::regexp_static_props[] = {
    JS_PSGS("input", static_input_getter, static_input_setter,
            JSPROP_PERMANENT | JSPROP_ENUMERATE),
    JS_PSG("lastMatch", static_lastMatch_getter, JSPROP_PERMANENT | JSPROP_ENUMERATE),
    JS_PSG("lastParen", static_lastParen_getter, JSPROP_PERMANENT | JSPROP_ENUMERATE),
    JS_PSG("leftContext",  static_leftContext_getter, JSPROP_PERMANENT | JSPROP_ENUMERATE),
    JS_PSG("rightContext", static_rightContext_getter, JSPROP_PERMANENT | JSPROP_ENUMERATE),
    JS_PSG("$1", static_paren1_getter, JSPROP_PERMANENT | JSPROP_ENUMERATE),
    JS_PSG("$2", static_paren2_getter, JSPROP_PERMANENT | JSPROP_ENUMERATE),
    JS_PSG("$3", static_paren3_getter, JSPROP_PERMANENT | JSPROP_ENUMERATE),
    JS_PSG("$4", static_paren4_getter, JSPROP_PERMANENT | JSPROP_ENUMERATE),
    JS_PSG("$5", static_paren5_getter, JSPROP_PERMANENT | JSPROP_ENUMERATE),
    JS_PSG("$6", static_paren6_getter, JSPROP_PERMANENT | JSPROP_ENUMERATE),
    JS_PSG("$7", static_paren7_getter, JSPROP_PERMANENT | JSPROP_ENUMERATE),
    JS_PSG("$8", static_paren8_getter, JSPROP_PERMANENT | JSPROP_ENUMERATE),
    JS_PSG("$9", static_paren9_getter, JSPROP_PERMANENT | JSPROP_ENUMERATE),
    JS_PSGS("$_", static_input_getter, static_input_setter, JSPROP_PERMANENT),
    JS_PSG("$&", static_lastMatch_getter, JSPROP_PERMANENT),
    JS_PSG("$+", static_lastParen_getter, JSPROP_PERMANENT),
    JS_PSG("$`", static_leftContext_getter, JSPROP_PERMANENT),
    JS_PSG("$'", static_rightContext_getter, JSPROP_PERMANENT),
    JS_SELF_HOSTED_SYM_GET(species, "RegExpSpecies", 0),
    JS_PS_END
};

JSObject*
js::CreateRegExpPrototype(JSContext* cx, JSProtoKey key)
{
    MOZ_ASSERT(key == JSProto_RegExp);

    Rooted<RegExpObject*> proto(cx, cx->global()->createBlankPrototype<RegExpObject>(cx));
    if (!proto)
        return nullptr;
    proto->NativeObject::setPrivate(nullptr);

    if (!RegExpObject::assignInitialShape(cx, proto))
        return nullptr;

    RootedAtom source(cx, cx->names().empty);
    proto->initAndZeroLastIndex(source, RegExpFlag(0), cx);

    return proto;
}

template <typename CharT>
static bool
IsTrailSurrogateWithLeadSurrogateImpl(JSContext* cx, HandleLinearString input, size_t index)
{
    JS::AutoCheckCannotGC nogc;
    MOZ_ASSERT(index > 0 && index < input->length());
    const CharT* inputChars = input->chars<CharT>(nogc);

    return unicode::IsTrailSurrogate(inputChars[index]) &&
           unicode::IsLeadSurrogate(inputChars[index - 1]);
}

static bool
IsTrailSurrogateWithLeadSurrogate(JSContext* cx, HandleLinearString input, int32_t index)
{
    if (index <= 0 || size_t(index) >= input->length())
        return false;

    return input->hasLatin1Chars()
           ? IsTrailSurrogateWithLeadSurrogateImpl<Latin1Char>(cx, input, index)
           : IsTrailSurrogateWithLeadSurrogateImpl<char16_t>(cx, input, index);
}

/*
 * ES 2017 draft rev 6a13789aa9e7c6de4e96b7d3e24d9e6eba6584ad 21.2.5.2.2
 * steps 3, 9-14, except 12.a.i, 12.c.i.1.
 */
static RegExpRunStatus
ExecuteRegExp(JSContext* cx, HandleObject regexp, HandleString string,
              int32_t lastIndex,
              MatchPairs* matches, size_t* endIndex, RegExpStaticsUpdate staticsUpdate)
{
    /*
     * WARNING: Despite the presence of spec step comment numbers, this
     *          algorithm isn't consistent with any ES6 version, draft or
     *          otherwise.  YOU HAVE BEEN WARNED.
     */

    /* Steps 1-2 performed by the caller. */
    Rooted<RegExpObject*> reobj(cx, &regexp->as<RegExpObject>());

    RegExpGuard re(cx);
    if (!reobj->getShared(cx, &re))
        return RegExpRunStatus_Error;

    RegExpStatics* res;
    if (staticsUpdate == UpdateRegExpStatics) {
        res = cx->global()->getRegExpStatics(cx);
        if (!res)
            return RegExpRunStatus_Error;
    } else {
        res = nullptr;
    }

    RootedLinearString input(cx, string->ensureLinear(cx));
    if (!input)
        return RegExpRunStatus_Error;

    /* Handled by caller */
    MOZ_ASSERT(lastIndex >= 0 && size_t(lastIndex) <= input->length());

    /* Steps 4-8 performed by the caller. */

    /* Step 10. */
    if (reobj->unicode()) {
        /*
         * ES 2017 draft rev 6a13789aa9e7c6de4e96b7d3e24d9e6eba6584ad
         * 21.2.2.2 step 2.
         *   Let listIndex be the index into Input of the character that was
         *   obtained from element index of str.
         *
         * In the spec, pattern match is performed with decoded Unicode code
         * points, but our implementation performs it with UTF-16 encoded
         * string.  In step 2, we should decrement lastIndex (index) if it
         * points the trail surrogate that has corresponding lead surrogate.
         *
         *   var r = /\uD83D\uDC38/ug;
         *   r.lastIndex = 1;
         *   var str = "\uD83D\uDC38";
         *   var result = r.exec(str); // pattern match starts from index 0
         *   print(result.index);      // prints 0
         *
         * Note: this doesn't match the current spec text and result in
         * different values for `result.index` under certain conditions.
         * However, the spec will change to match our implementation's
         * behavior. See https://github.com/tc39/ecma262/issues/128.
         */
        if (IsTrailSurrogateWithLeadSurrogate(cx, input, lastIndex))
            lastIndex--;
    }

    /* Steps 3, 11-14, except 12.a.i, 12.c.i.1. */
    RegExpRunStatus status = ExecuteRegExpImpl(cx, res, *re, input, lastIndex, matches, endIndex);
    if (status == RegExpRunStatus_Error)
        return RegExpRunStatus_Error;

    /* Steps 12.a.i, 12.c.i.i, 15 are done by Self-hosted function. */

    return status;
}

/*
 * ES 2017 draft rev 6a13789aa9e7c6de4e96b7d3e24d9e6eba6584ad 21.2.5.2.2
 * steps 3, 9-25, except 12.a.i, 12.c.i.1, 15.
 */
static bool
RegExpMatcherImpl(JSContext* cx, HandleObject regexp, HandleString string,
                  int32_t lastIndex, RegExpStaticsUpdate staticsUpdate, MutableHandleValue rval)
{
    /* Execute regular expression and gather matches. */
    ScopedMatchPairs matches(&cx->tempLifoAlloc());

    /* Steps 3, 9-14, except 12.a.i, 12.c.i.1. */
    RegExpRunStatus status = ExecuteRegExp(cx, regexp, string, lastIndex,
                                           &matches, nullptr, staticsUpdate);
    if (status == RegExpRunStatus_Error)
        return false;

    /* Steps 12.a, 12.c. */
    if (status == RegExpRunStatus_Success_NotFound) {
        rval.setNull();
        return true;
    }

    /* Steps 16-25 */
    return CreateRegExpMatchResult(cx, string, matches, rval);
}

/*
 * ES 2017 draft rev 6a13789aa9e7c6de4e96b7d3e24d9e6eba6584ad 21.2.5.2.2
 * steps 3, 9-25, except 12.a.i, 12.c.i.1, 15.
 */
bool
js::RegExpMatcher(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);
    MOZ_ASSERT(IsRegExpObject(args[0]));
    MOZ_ASSERT(args[1].isString());
    MOZ_ASSERT(args[2].isNumber());

    RootedObject regexp(cx, &args[0].toObject());
    RootedString string(cx, args[1].toString());
    RootedValue lastIndexVal(cx, args[2]);

    int32_t lastIndex = 0;
    if (!ToInt32(cx, lastIndexVal, &lastIndex))
        return false;

    /* Steps 3, 9-25, except 12.a.i, 12.c.i.1, 15. */
    return RegExpMatcherImpl(cx, regexp, string, lastIndex,
                             UpdateRegExpStatics, args.rval());
}

/*
 * Separate interface for use by IonMonkey.
 * This code cannot re-enter Ion code.
 */
bool
js::RegExpMatcherRaw(JSContext* cx, HandleObject regexp, HandleString input,
                     int32_t lastIndex,
                     MatchPairs* maybeMatches, MutableHandleValue output)
{
    MOZ_ASSERT(lastIndex >= 0);

    // The MatchPairs will always be passed in, but RegExp execution was
    // successful only if the pairs have actually been filled in.
    if (maybeMatches && maybeMatches->pairsRaw()[0] >= 0)
        return CreateRegExpMatchResult(cx, input, *maybeMatches, output);
    return RegExpMatcherImpl(cx, regexp, input, lastIndex,
                             UpdateRegExpStatics, output);
}

/*
 * ES 2017 draft rev 6a13789aa9e7c6de4e96b7d3e24d9e6eba6584ad 21.2.5.2.2
 * steps 3, 9-25, except 12.a.i, 12.c.i.1, 15.
 * This code is inlined in CodeGenerator.cpp generateRegExpSearcherStub,
 * changes to this code need to get reflected in there too.
 */
static bool
RegExpSearcherImpl(JSContext* cx, HandleObject regexp, HandleString string,
                   int32_t lastIndex, RegExpStaticsUpdate staticsUpdate, int32_t* result)
{
    /* Execute regular expression and gather matches. */
    ScopedMatchPairs matches(&cx->tempLifoAlloc());

    /* Steps 3, 9-14, except 12.a.i, 12.c.i.1. */
    RegExpRunStatus status = ExecuteRegExp(cx, regexp, string, lastIndex,
                                           &matches, nullptr, staticsUpdate);
    if (status == RegExpRunStatus_Error)
        return false;

    /* Steps 12.a, 12.c. */
    if (status == RegExpRunStatus_Success_NotFound) {
        *result = -1;
        return true;
    }

    /* Steps 16-25 */
    *result = CreateRegExpSearchResult(cx, matches);
    return true;
}

/*
 * ES 2017 draft rev 6a13789aa9e7c6de4e96b7d3e24d9e6eba6584ad 21.2.5.2.2
 * steps 3, 9-25, except 12.a.i, 12.c.i.1, 15.
 */
bool
js::RegExpSearcher(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);
    MOZ_ASSERT(IsRegExpObject(args[0]));
    MOZ_ASSERT(args[1].isString());
    MOZ_ASSERT(args[2].isNumber());

    RootedObject regexp(cx, &args[0].toObject());
    RootedString string(cx, args[1].toString());
    RootedValue lastIndexVal(cx, args[2]);

    int32_t lastIndex = 0;
    if (!ToInt32(cx, lastIndexVal, &lastIndex))
        return false;

    /* Steps 3, 9-25, except 12.a.i, 12.c.i.1, 15. */
    int32_t result = 0;
    if (!RegExpSearcherImpl(cx, regexp, string, lastIndex, UpdateRegExpStatics, &result))
        return false;

    args.rval().setInt32(result);
    return true;
}

/*
 * Separate interface for use by IonMonkey.
 * This code cannot re-enter Ion code.
 */
bool
js::RegExpSearcherRaw(JSContext* cx, HandleObject regexp, HandleString input,
                      int32_t lastIndex, MatchPairs* maybeMatches, int32_t* result)
{
    MOZ_ASSERT(lastIndex >= 0);

    // The MatchPairs will always be passed in, but RegExp execution was
    // successful only if the pairs have actually been filled in.
    if (maybeMatches && maybeMatches->pairsRaw()[0] >= 0) {
        *result = CreateRegExpSearchResult(cx, *maybeMatches);
        return true;
    }
    return RegExpSearcherImpl(cx, regexp, input, lastIndex,
                              UpdateRegExpStatics, result);
}

bool
js::regexp_exec_no_statics(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(IsRegExpObject(args[0]));
    MOZ_ASSERT(args[1].isString());

    RootedObject regexp(cx, &args[0].toObject());
    RootedString string(cx, args[1].toString());

    return RegExpMatcherImpl(cx, regexp, string, 0,
                             DontUpdateRegExpStatics, args.rval());
}

/*
 * ES 2017 draft rev 6a13789aa9e7c6de4e96b7d3e24d9e6eba6584ad 21.2.5.2.2
 * steps 3, 9-14, except 12.a.i, 12.c.i.1.
 */
bool
js::RegExpTester(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);
    MOZ_ASSERT(IsRegExpObject(args[0]));
    MOZ_ASSERT(args[1].isString());
    MOZ_ASSERT(args[2].isNumber());

    RootedObject regexp(cx, &args[0].toObject());
    RootedString string(cx, args[1].toString());
    RootedValue lastIndexVal(cx, args[2]);

    int32_t lastIndex = 0;
    if (!ToInt32(cx, lastIndexVal, &lastIndex))
        return false;

    /* Steps 3, 9-14, except 12.a.i, 12.c.i.1. */
    size_t endIndex = 0;
    RegExpRunStatus status = ExecuteRegExp(cx, regexp, string, lastIndex,
                                           nullptr, &endIndex, UpdateRegExpStatics);

    if (status == RegExpRunStatus_Error)
        return false;

    if (status == RegExpRunStatus_Success) {
        MOZ_ASSERT(endIndex <= INT32_MAX);
        args.rval().setInt32(int32_t(endIndex));
    } else {
        args.rval().setInt32(-1);
    }
    return true;
}

/*
 * Separate interface for use by IonMonkey.
 * This code cannot re-enter Ion code.
 */
bool
js::RegExpTesterRaw(JSContext* cx, HandleObject regexp, HandleString input,
                    int32_t lastIndex, int32_t* endIndex)
{
    MOZ_ASSERT(lastIndex >= 0);

    size_t endIndexTmp = 0;
    RegExpRunStatus status = ExecuteRegExp(cx, regexp, input, lastIndex,
                                           nullptr, &endIndexTmp, UpdateRegExpStatics);

    if (status == RegExpRunStatus_Success) {
        MOZ_ASSERT(endIndexTmp <= INT32_MAX);
        *endIndex = int32_t(endIndexTmp);
        return true;
    }
    if (status == RegExpRunStatus_Success_NotFound) {
        *endIndex = -1;
        return true;
    }

    return false;
}

bool
js::regexp_test_no_statics(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(IsRegExpObject(args[0]));
    MOZ_ASSERT(args[1].isString());

    RootedObject regexp(cx, &args[0].toObject());
    RootedString string(cx, args[1].toString());

    size_t ignored = 0;
    RegExpRunStatus status = ExecuteRegExp(cx, regexp, string, 0,
                                           nullptr, &ignored, DontUpdateRegExpStatics);
    args.rval().setBoolean(status == RegExpRunStatus_Success);
    return status != RegExpRunStatus_Error;
}

static void
GetParen(JSLinearString* matched, JS::Value capture, JSSubString* out)
{
    if (capture.isUndefined()) {
        out->initEmpty(matched);
        return;
    }
    JSLinearString& captureLinear = capture.toString()->asLinear();
    out->init(&captureLinear, 0, captureLinear.length());
}

template <typename CharT>
static bool
InterpretDollar(JSLinearString* matched, JSLinearString* string, size_t position, size_t tailPos,
                MutableHandle<GCVector<Value>> captures, JSLinearString* replacement,
                const CharT* replacementBegin, const CharT* currentDollar,
                const CharT* replacementEnd,
                JSSubString* out, size_t* skip)
{
    MOZ_ASSERT(*currentDollar == '$');

    /* If there is only a dollar, bail now. */
    if (currentDollar + 1 >= replacementEnd)
        return false;

    /* ES 2016 draft Mar 25, 2016 Table 46. */
    char16_t c = currentDollar[1];
    if (JS7_ISDEC(c)) {
        /* $n, $nn */
        unsigned num = JS7_UNDEC(c);
        if (num > captures.length()) {
            // The result is implementation-defined, do not substitute.
            return false;
        }

        const CharT* currentChar = currentDollar + 2;
        if (currentChar < replacementEnd && (c = *currentChar, JS7_ISDEC(c))) {
            unsigned tmpNum = 10 * num + JS7_UNDEC(c);
            // If num > captures.length(), the result is implementation-defined.
            // Consume next character only if num <= captures.length().
            if (tmpNum <= captures.length()) {
                currentChar++;
                num = tmpNum;
            }
        }
        if (num == 0) {
            // The result is implementation-defined.
            // Do not substitute.
            return false;
        }

        *skip = currentChar - currentDollar;

        MOZ_ASSERT(num <= captures.length());

        GetParen(matched, captures[num -1], out);
        return true;
    }

    *skip = 2;
    switch (c) {
      default:
        return false;
      case '$':
        out->init(replacement, currentDollar - replacementBegin, 1);
        break;
      case '&':
        out->init(matched, 0, matched->length());
        break;
      case '+':
        // SpiderMonkey extension
        if (captures.length() == 0)
            out->initEmpty(matched);
        else
            GetParen(matched, captures[captures.length() - 1], out);
        break;
      case '`':
        out->init(string, 0, position);
        break;
      case '\'':
        out->init(string, tailPos, string->length() - tailPos);
        break;
    }
    return true;
}

template <typename CharT>
static bool
FindReplaceLengthString(JSContext* cx, HandleLinearString matched, HandleLinearString string,
                        size_t position, size_t tailPos, MutableHandle<GCVector<Value>> captures,
                        HandleLinearString replacement, size_t firstDollarIndex, size_t* sizep)
{
    CheckedInt<uint32_t> replen = replacement->length();

    JS::AutoCheckCannotGC nogc;
    MOZ_ASSERT(firstDollarIndex < replacement->length());
    const CharT* replacementBegin = replacement->chars<CharT>(nogc);
    const CharT* currentDollar = replacementBegin + firstDollarIndex;
    const CharT* replacementEnd = replacementBegin + replacement->length();
    do {
        JSSubString sub;
        size_t skip;
        if (InterpretDollar(matched, string, position, tailPos, captures, replacement,
                            replacementBegin, currentDollar, replacementEnd, &sub, &skip))
        {
            if (sub.length > skip)
                replen += sub.length - skip;
            else
                replen -= skip - sub.length;
            currentDollar += skip;
        } else {
            currentDollar++;
        }

        currentDollar = js_strchr_limit(currentDollar, '$', replacementEnd);
    } while (currentDollar);

    if (!replen.isValid()) {
        ReportAllocationOverflow(cx);
        return false;
    }

    *sizep = replen.value();
    return true;
}

static bool
FindReplaceLength(JSContext* cx, HandleLinearString matched, HandleLinearString string,
                  size_t position, size_t tailPos, MutableHandle<GCVector<Value>> captures,
                  HandleLinearString replacement, size_t firstDollarIndex, size_t* sizep)
{
    return replacement->hasLatin1Chars()
           ? FindReplaceLengthString<Latin1Char>(cx, matched, string, position, tailPos, captures,
                                                 replacement, firstDollarIndex, sizep)
           : FindReplaceLengthString<char16_t>(cx, matched, string, position, tailPos, captures,
                                               replacement, firstDollarIndex, sizep);
}

/*
 * Precondition: |sb| already has necessary growth space reserved (as
 * derived from FindReplaceLength), and has been inflated to TwoByte if
 * necessary.
 */
template <typename CharT>
static void
DoReplace(HandleLinearString matched, HandleLinearString string,
          size_t position, size_t tailPos, MutableHandle<GCVector<Value>> captures,
          HandleLinearString replacement, size_t firstDollarIndex, StringBuffer &sb)
{
    JS::AutoCheckCannotGC nogc;
    const CharT* replacementBegin = replacement->chars<CharT>(nogc);
    const CharT* currentChar = replacementBegin;

    MOZ_ASSERT(firstDollarIndex < replacement->length());
    const CharT* currentDollar = replacementBegin + firstDollarIndex;
    const CharT* replacementEnd = replacementBegin + replacement->length();
    do {
        /* Move one of the constant portions of the replacement value. */
        size_t len = currentDollar - currentChar;
        sb.infallibleAppend(currentChar, len);
        currentChar = currentDollar;

        JSSubString sub;
        size_t skip;
        if (InterpretDollar(matched, string, position, tailPos, captures, replacement,
                            replacementBegin, currentDollar, replacementEnd, &sub, &skip))
        {
            sb.infallibleAppendSubstring(sub.base, sub.offset, sub.length);
            currentChar += skip;
            currentDollar += skip;
        } else {
            currentDollar++;
        }

        currentDollar = js_strchr_limit(currentDollar, '$', replacementEnd);
    } while (currentDollar);
    sb.infallibleAppend(currentChar, replacement->length() - (currentChar - replacementBegin));
}

static bool
NeedTwoBytes(HandleLinearString string, HandleLinearString replacement,
             HandleLinearString matched, Handle<GCVector<Value>> captures)
{
    if (string->hasTwoByteChars())
        return true;
    if (replacement->hasTwoByteChars())
        return true;
    if (matched->hasTwoByteChars())
        return true;

    for (size_t i = 0, len = captures.length(); i < len; i++) {
        Value capture = captures[i];
        if (capture.isUndefined())
            continue;
        if (capture.toString()->hasTwoByteChars())
            return true;
    }

    return false;
}

/* ES 2016 draft Mar 25, 2016 21.1.3.14.1. */
bool
js::RegExpGetSubstitution(JSContext* cx, HandleLinearString matched, HandleLinearString string,
                          size_t position, HandleObject capturesObj, HandleLinearString replacement,
                          size_t firstDollarIndex, MutableHandleValue rval)
{
    MOZ_ASSERT(firstDollarIndex < replacement->length());

    // Step 1 (skipped).

    // Step 2.
    size_t matchLength = matched->length();

    // Steps 3-5 (skipped).

    // Step 6.
    MOZ_ASSERT(position <= string->length());

    // Step 10 (reordered).
    uint32_t nCaptures;
    if (!GetLengthProperty(cx, capturesObj, &nCaptures))
        return false;

    Rooted<GCVector<Value>> captures(cx, GCVector<Value>(cx));
    if (!captures.reserve(nCaptures))
        return false;

    // Step 7.
    RootedValue capture(cx);
    for (uint32_t i = 0; i < nCaptures; i++) {
        if (!GetElement(cx, capturesObj, capturesObj, i, &capture))
            return false;

        if (capture.isUndefined()) {
            captures.infallibleAppend(capture);
            continue;
        }

        MOZ_ASSERT(capture.isString());
        RootedLinearString captureLinear(cx, capture.toString()->ensureLinear(cx));
        if (!captureLinear)
            return false;
        captures.infallibleAppend(StringValue(captureLinear));
    }

    // Step 8 (skipped).

    // Step 9.
    CheckedInt<uint32_t> checkedTailPos(0);
    checkedTailPos += position;
    checkedTailPos += matchLength;
    if (!checkedTailPos.isValid()) {
        ReportAllocationOverflow(cx);
        return false;
    }
    uint32_t tailPos = checkedTailPos.value();

    // Step 11.
    size_t reserveLength;
    if (!FindReplaceLength(cx, matched, string, position, tailPos, &captures, replacement,
                           firstDollarIndex, &reserveLength))
    {
        return false;
    }

    StringBuffer result(cx);
    if (NeedTwoBytes(string, replacement, matched, captures)) {
        if (!result.ensureTwoByteChars())
            return false;
    }

    if (!result.reserve(reserveLength))
        return false;

    if (replacement->hasLatin1Chars()) {
        DoReplace<Latin1Char>(matched, string, position, tailPos, &captures,
                              replacement, firstDollarIndex, result);
    } else {
        DoReplace<char16_t>(matched, string, position, tailPos, &captures,
                            replacement, firstDollarIndex, result);
    }

    // Step 12.
    JSString* resultString = result.finishString();
    if (!resultString)
        return false;

    rval.setString(resultString);
    return true;
}

bool
js::GetFirstDollarIndex(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    RootedString str(cx, args[0].toString());

    // Should be handled in different path.
    MOZ_ASSERT(str->length() != 0);

    int32_t index = -1;
    if (!GetFirstDollarIndexRaw(cx, str, &index))
        return false;

    args.rval().setInt32(index);
    return true;
}

template <typename TextChar>
static MOZ_ALWAYS_INLINE int
GetFirstDollarIndexImpl(const TextChar* text, uint32_t textLen)
{
    const TextChar* end = text + textLen;
    for (const TextChar* c = text; c != end; ++c) {
        if (*c == '$')
            return c - text;
    }
    return -1;
}

int32_t
js::GetFirstDollarIndexRawFlat(JSLinearString* text)
{
    uint32_t len = text->length();

    JS::AutoCheckCannotGC nogc;
    if (text->hasLatin1Chars())
        return GetFirstDollarIndexImpl(text->latin1Chars(nogc), len);

    return  GetFirstDollarIndexImpl(text->twoByteChars(nogc), len);
}

bool
js::GetFirstDollarIndexRaw(JSContext* cx, HandleString str, int32_t* index)
{
    JSLinearString* text = str->ensureLinear(cx);
    if (!text)
        return false;

    *index = GetFirstDollarIndexRawFlat(text);
    return true;
}

bool
js::RegExpPrototypeOptimizable(JSContext* cx, unsigned argc, Value* vp)
{
    // This can only be called from self-hosted code.
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);

    uint8_t result = false;
    if (!RegExpPrototypeOptimizableRaw(cx, &args[0].toObject(), &result))
        return false;

    args.rval().setBoolean(result);
    return true;
}

bool
js::RegExpPrototypeOptimizableRaw(JSContext* cx, JSObject* proto, uint8_t* result)
{
    JS::AutoCheckCannotGC nogc;
    if (!proto->isNative()) {
        *result = false;
        return true;
    }

    NativeObject* nproto = static_cast<NativeObject*>(proto);

    Shape* shape = cx->compartment()->regExps.getOptimizableRegExpPrototypeShape();
    if (shape == nproto->lastProperty()) {
        *result = true;
        return true;
    }

    JSNative globalGetter;
    if (!GetOwnNativeGetterPure(cx, proto, NameToId(cx->names().global), &globalGetter))
        return false;

    if (globalGetter != regexp_global) {
        *result = false;
        return true;
    }

    JSNative stickyGetter;
    if (!GetOwnNativeGetterPure(cx, proto, NameToId(cx->names().sticky), &stickyGetter))
        return false;

    if (stickyGetter != regexp_sticky) {
        *result = false;
        return true;
    }

    JSNative unicodeGetter;
    if (!GetOwnNativeGetterPure(cx, proto, NameToId(cx->names().unicode), &unicodeGetter))
        return false;

    if (unicodeGetter != regexp_unicode) {
        *result = false;
        return true;
    }

    // Check if @@match, @@search, and exec are own data properties,
    // those values should be tested in selfhosted JS.
    bool has = false;
    if (!HasOwnDataPropertyPure(cx, proto, SYMBOL_TO_JSID(cx->wellKnownSymbols().match), &has))
        return false;
    if (!has) {
        *result = false;
        return true;
    }

    if (!HasOwnDataPropertyPure(cx, proto, SYMBOL_TO_JSID(cx->wellKnownSymbols().search), &has))
        return false;
    if (!has) {
        *result = false;
        return true;
    }

    if (!HasOwnDataPropertyPure(cx, proto, NameToId(cx->names().exec), &has))
        return false;
    if (!has) {
        *result = false;
        return true;
    }

    cx->compartment()->regExps.setOptimizableRegExpPrototypeShape(nproto->lastProperty());
    *result = true;
    return true;
}

bool
js::RegExpInstanceOptimizable(JSContext* cx, unsigned argc, Value* vp)
{
    // This can only be called from self-hosted code.
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);

    uint8_t result = false;
    if (!RegExpInstanceOptimizableRaw(cx, &args[0].toObject(), &args[1].toObject(), &result))
        return false;

    args.rval().setBoolean(result);
    return true;
}

bool
js::RegExpInstanceOptimizableRaw(JSContext* cx, JSObject* rx, JSObject* proto, uint8_t* result)
{
    JS::AutoCheckCannotGC nogc;
    if (!rx->isNative()) {
        *result = false;
        return true;
    }

    NativeObject* nobj = static_cast<NativeObject*>(rx);

    Shape* shape = cx->compartment()->regExps.getOptimizableRegExpInstanceShape();
    if (shape == nobj->lastProperty()) {
        *result = true;
        return true;
    }

    if (!rx->hasStaticPrototype()) {
        *result = false;
        return true;
    }

    if (rx->staticPrototype() != proto) {
        *result = false;
        return true;
    }

    if (!RegExpObject::isInitialShape(nobj)) {
        *result = false;
        return true;
    }

    cx->compartment()->regExps.setOptimizableRegExpInstanceShape(nobj->lastProperty());
    *result = true;
    return true;
}

/*
 * Pattern match the script to check if it is is indexing into a particular
 * object, e.g. 'function(a) { return b[a]; }'. Avoid calling the script in
 * such cases, which are used by javascript packers (particularly the popular
 * Dean Edwards packer) to efficiently encode large scripts. We only handle the
 * code patterns generated by such packers here.
 */
bool
js::intrinsic_GetElemBaseForLambda(JSContext* cx, unsigned argc, Value* vp)
{
    // This can only be called from self-hosted code.
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);

    JSObject& lambda = args[0].toObject();
    args.rval().setUndefined();

    if (!lambda.is<JSFunction>())
        return true;

    RootedFunction fun(cx, &lambda.as<JSFunction>());
    if (!fun->isInterpreted() || fun->isClassConstructor())
        return true;

    JSScript* script = fun->getOrCreateScript(cx);
    if (!script)
        return false;

    jsbytecode* pc = script->code();

    /*
     * JSOP_GETALIASEDVAR tells us exactly where to find the base object 'b'.
     * Rule out the (unlikely) possibility of a function with environment
     * objects since it would make our environment walk off.
     */
    if (JSOp(*pc) != JSOP_GETALIASEDVAR || fun->needsSomeEnvironmentObject())
        return true;
    EnvironmentCoordinate ec(pc);
    EnvironmentObject* env = &fun->environment()->as<EnvironmentObject>();
    for (unsigned i = 0; i < ec.hops(); ++i)
        env = &env->enclosingEnvironment().as<EnvironmentObject>();
    Value b = env->aliasedBinding(ec);
    pc += JSOP_GETALIASEDVAR_LENGTH;

    /* Look for 'a' to be the lambda's first argument. */
    if (JSOp(*pc) != JSOP_GETARG || GET_ARGNO(pc) != 0)
        return true;
    pc += JSOP_GETARG_LENGTH;

    /* 'b[a]' */
    if (JSOp(*pc) != JSOP_GETELEM)
        return true;
    pc += JSOP_GETELEM_LENGTH;

    /* 'return b[a]' */
    if (JSOp(*pc) != JSOP_RETURN)
        return true;

    /* 'b' must behave like a normal object. */
    if (!b.isObject())
        return true;

    JSObject& bobj = b.toObject();
    const Class* clasp = bobj.getClass();
    if (!clasp->isNative() || clasp->getOpsLookupProperty() || clasp->getOpsGetProperty())
        return true;

    args.rval().setObject(bobj);
    return true;
}

/*
 * Emulates `b[a]` property access, that is detected in GetElemBaseForLambda.
 * It returns the property value only if the property is data property and the
 * propety value is a string.  Otherwise it returns undefined.
 */
bool
js::intrinsic_GetStringDataProperty(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);

    RootedObject obj(cx, &args[0].toObject());
    if (!obj->isNative()) {
        // The object is already checked to be native in GetElemBaseForLambda,
        // but it can be swapped to the other class that is non-native.
        // Return undefined to mark failure to get the property.
        args.rval().setUndefined();
        return true;
    }

    RootedNativeObject nobj(cx, &obj->as<NativeObject>());
    RootedString name(cx, args[1].toString());

    RootedAtom atom(cx, AtomizeString(cx, name));
    if (!atom)
        return false;

    RootedValue v(cx);
    if (HasDataProperty(cx, nobj, AtomToId(atom), v.address()) && v.isString())
        args.rval().set(v);
    else
        args.rval().setUndefined();

    return true;
}
