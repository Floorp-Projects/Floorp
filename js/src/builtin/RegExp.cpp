/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/RegExp.h"

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
using mozilla::Maybe;

/* ES6 21.2.5.2.2 steps 19-29. */
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

    /* Step 19. */
    RootedArrayObject arr(cx, NewDenseFullyAllocatedArrayWithTemplate(cx, numPairs, templateObject));
    if (!arr)
        return false;

    /* Steps 27-28
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

    /* Step 24 (reordered)
     * Set the |index| property. (TemplateObject positions it in slot 0) */
    arr->setSlot(0, Int32Value(matches[0].start));

    /* Step 25 (reordered)
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

    /* Step 29. */
    rval.setObject(*arr);
    return true;
}

/* ES6 21.2.5.2.2 steps 3, 14-17, except 15.a.i-ii, 15.c.i.1-2. */
static RegExpRunStatus
ExecuteRegExpImpl(JSContext* cx, RegExpStatics* res, RegExpShared& re, HandleLinearString input,
                  size_t searchIndex, bool sticky, MatchPairs* matches, size_t* endIndex)
{
    RegExpRunStatus status = re.execute(cx, input, searchIndex, sticky, matches, endIndex);

    /* Out of spec: Update RegExpStatics. */
    if (status == RegExpRunStatus_Success && res) {
        if (matches) {
            if (!res->updateFromMatchPairs(cx, input, *matches))
                return RegExpRunStatus_Error;
        } else {
            res->updateLazily(cx, input, &re, searchIndex, sticky);
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

    RegExpRunStatus status = ExecuteRegExpImpl(cx, res, *shared, input, *lastIndex, reobj.sticky(),
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

enum RegExpSharedUse {
    UseRegExpShared,
    DontUseRegExpShared
};

/*
 * ES 2016 draft Mar 25, 2016 21.2.3.2.2.
 * Because this function only ever returns |obj| in the spec, provided by the
 * user, we omit it and just return the usual success/failure.
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
        CompileOptions options(cx);
        frontend::TokenStream dummyTokenStream(cx, options, nullptr, 0, nullptr);
        if (!irregexp::ParsePatternSyntax(dummyTokenStream, cx->tempLifoAlloc(), pattern,
                                          flags & UnicodeFlag))
        {
            return false;
        }

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
    Rooted<RegExpObject*> regexp(cx, RegExpAlloc(cx, nullptr));
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
    ESClassValue cls;
    if (!GetClassOfValue(cx, value, &cls))
        return false;

    *result = cls == ESClass_RegExp;
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
    ESClassValue cls;
    if (!GetClassOfValue(cx, patternValue, &cls))
        return false;
    if (cls == ESClass_RegExp) {
        // Step 3a.
        if (args.hasDefined(1)) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_NEWREGEXP_FLAGGED);
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

/* ES6 21.2.3.1. */
bool
js::regexp_construct(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Steps 1-2.
    bool patternIsRegExp;
    if (!IsRegExp(cx, args.get(0), &patternIsRegExp))
        return false;


    // We can delay step 3 and step 4a until later, during
    // GetPrototypeFromCallableConstructor calls. Accessing the new.target
    // and the callee from the stack is unobservable.
    if (!args.isConstructing()) {
        // Step 4b.
        if (patternIsRegExp && !args.hasDefined(1)) {
            RootedObject patternObj(cx, &args[0].toObject());

            // Steps 4b.i-ii.
            RootedValue patternConstructor(cx);
            if (!GetProperty(cx, patternObj, patternObj, cx->names().constructor, &patternConstructor))
                return false;

            // Step 4b.iii.
            if (patternConstructor.isObject() && patternConstructor.toObject() == args.callee()) {
                args.rval().set(args[0]);
                return true;
            }
        }
    }

    RootedValue patternValue(cx, args.get(0));

    // Step 5.
    ESClassValue cls;
    if (!GetClassOfValue(cx, patternValue, &cls))
        return false;
    if (cls == ESClass_RegExp) {
        // Beware!  |patternObj| might be a proxy into another compartment, so
        // don't assume |patternObj.is<RegExpObject>()|.  For the same reason,
        // don't reuse the RegExpShared below.
        RootedObject patternObj(cx, &patternValue.toObject());

        // Step 5
        RootedAtom sourceAtom(cx);
        RegExpFlag flags;
        {
            // Step 5.a.
            RegExpGuard g(cx);
            if (!RegExpToShared(cx, patternObj, &g))
                return false;
            sourceAtom = g->getSource();

            if (!args.hasDefined(1)) {
                // Step 5b.
                flags = g->getFlags();
            }
        }

        // Steps 8-9.
        RootedObject proto(cx);
        if (!GetPrototypeFromCallableConstructor(cx, args, &proto))
            return false;

        Rooted<RegExpObject*> regexp(cx, RegExpAlloc(cx, proto));
        if (!regexp)
            return false;

        // Step 10.
        if (args.hasDefined(1)) {
            // Step 5c / 21.2.3.2.2 RegExpInitialize step 5.
            flags = RegExpFlag(0);
            RootedString flagStr(cx, ToString<CanGC>(cx, args[1]));
            if (!flagStr)
                return false;
            if (!ParseRegExpFlags(cx, flagStr, &flags))
                return false;
        }

        regexp->initAndZeroLastIndex(sourceAtom, flags, cx);

        args.rval().setObject(*regexp);
        return true;
    }

    RootedValue P(cx);
    RootedValue F(cx);

    // Step 6.
    if (patternIsRegExp) {
        RootedObject patternObj(cx, &patternValue.toObject());

        // Steps 6a-b.
        if (!GetProperty(cx, patternObj, patternObj, cx->names().source, &P))
            return false;

        // Steps 6c-d.
        F = args.get(1);
        if (F.isUndefined()) {
            if (!GetProperty(cx, patternObj, patternObj, cx->names().flags, &F))
                return false;
        }
    } else {
        // Steps 7a-b.
        P = patternValue;
        F = args.get(1);
    }

    // Steps 8-9.
    RootedObject proto(cx);
    if (!GetPrototypeFromCallableConstructor(cx, args, &proto))
        return false;

    Rooted<RegExpObject*> regexp(cx, RegExpAlloc(cx, proto));
    if (!regexp)
        return false;

    // Step 10.
    if (!RegExpInitializeIgnoringLastIndex(cx, regexp, P, F))
        return false;
    regexp->zeroLastIndex(cx);

    args.rval().setObject(*regexp);
    return true;
}

bool
js::regexp_construct_self_hosting(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    MOZ_ASSERT(args.length() == 1 || args.length() == 2);
    MOZ_ASSERT(args[0].isString());
    MOZ_ASSERT_IF(args.length() == 2, args[1].isString());
    MOZ_ASSERT(!args.isConstructing());

    /* Steps 1-6 are not required since pattern is always string. */

    /* Steps 7-10. */
    Rooted<RegExpObject*> regexp(cx, RegExpAlloc(cx));
    if (!regexp)
        return false;

    if (!RegExpInitializeIgnoringLastIndex(cx, regexp, args[0], args.get(1)))
        return false;
    regexp->zeroLastIndex(cx);

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

/* ES6 21.2.5.2.2 steps 3, 11-17, except 15.a.i-ii, 15.c.i.1-2. */
static RegExpRunStatus
ExecuteRegExp(JSContext* cx, HandleObject regexp, HandleString string,
              int32_t lastIndex, bool sticky,
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

    /* Steps 4-10 performed by the caller. */

    /* Steps 12-13. */
    if (reobj->unicode()) {
        /*
         * ES6 21.2.2.2 step 2.
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

    /* Steps 3, 14-17, except 15.a.i-ii, 15.c.i.1-2. */
    RegExpRunStatus status = ExecuteRegExpImpl(cx, res, *re, input, lastIndex, sticky, matches, endIndex);
    if (status == RegExpRunStatus_Error)
        return RegExpRunStatus_Error;

    /* Steps 15.a.i-ii, 18 are done by Self-hosted function. */

    return status;
}

/* ES6 21.2.5.2.2 steps 3, 11-29, except 15.a.i-ii, 15.c.i.1-2, 18. */
static bool
RegExpMatcherImpl(JSContext* cx, HandleObject regexp, HandleString string,
                  int32_t lastIndex, bool sticky,
                  RegExpStaticsUpdate staticsUpdate, MutableHandleValue rval)
{
    /* Execute regular expression and gather matches. */
    ScopedMatchPairs matches(&cx->tempLifoAlloc());

    /* Steps 3, 11-17, except 15.a.i-ii, 15.c.i.1-2. */
    RegExpRunStatus status = ExecuteRegExp(cx, regexp, string, lastIndex, sticky,
                                           &matches, nullptr, staticsUpdate);
    if (status == RegExpRunStatus_Error)
        return false;

    /* Steps 15.a, 15.c. */
    if (status == RegExpRunStatus_Success_NotFound) {
        rval.setNull();
        return true;
    }

    /* Steps 19-29 */
    return CreateRegExpMatchResult(cx, string, matches, rval);
}

/* ES6 21.2.5.2.2 steps 3, 11-29, except 15.a.i-ii, 15.c.i.1-2, 18. */
bool
js::RegExpMatcher(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 4);
    MOZ_ASSERT(IsRegExpObject(args[0]));
    MOZ_ASSERT(args[1].isString());
    MOZ_ASSERT(args[2].isNumber());
    MOZ_ASSERT(args[3].isBoolean());

    RootedObject regexp(cx, &args[0].toObject());
    RootedString string(cx, args[1].toString());
    RootedValue lastIndexVal(cx, args[2]);
    bool sticky = ToBoolean(args[3]);

    int32_t lastIndex = 0;
    if (!ToInt32(cx, lastIndexVal, &lastIndex))
        return false;

    /* Steps 3, 11-29, except 15.a.i-ii, 15.c.i.1-2, 18. */
    return RegExpMatcherImpl(cx, regexp, string, lastIndex, sticky,
                             UpdateRegExpStatics, args.rval());
}

/* Separate interface for use by IonMonkey.
 * This code cannot re-enter Ion code. */
bool
js::RegExpMatcherRaw(JSContext* cx, HandleObject regexp, HandleString input,
                     uint32_t lastIndex, bool sticky,
                     MatchPairs* maybeMatches, MutableHandleValue output)
{
    MOZ_ASSERT(lastIndex <= INT32_MAX);

    // The MatchPairs will always be passed in, but RegExp execution was
    // successful only if the pairs have actually been filled in.
    if (maybeMatches && maybeMatches->pairsRaw()[0] >= 0)
        return CreateRegExpMatchResult(cx, input, *maybeMatches, output);
    return RegExpMatcherImpl(cx, regexp, input, lastIndex, sticky,
                             UpdateRegExpStatics, output);
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

    return RegExpMatcherImpl(cx, regexp, string, 0, false,
                             DontUpdateRegExpStatics, args.rval());
}

/* ES6 21.2.5.2.2 steps 3, 11-17, except 15.a.i-ii, 15.c.i.1-2. */
bool
js::RegExpTester(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 4);
    MOZ_ASSERT(IsRegExpObject(args[0]));
    MOZ_ASSERT(args[1].isString());
    MOZ_ASSERT(args[2].isNumber());
    MOZ_ASSERT(args[3].isBoolean());

    RootedObject regexp(cx, &args[0].toObject());
    RootedString string(cx, args[1].toString());
    RootedValue lastIndexVal(cx, args[2]);
    bool sticky = ToBoolean(args[3]);

    int32_t lastIndex = 0;
    if (!ToInt32(cx, lastIndexVal, &lastIndex))
        return false;

    /* Steps 3, 11-17, except 15.a.i-ii, 15.c.i.1-2. */
    size_t endIndex = 0;
    RegExpRunStatus status = ExecuteRegExp(cx, regexp, string,
                                           lastIndex, sticky,
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

/* Separate interface for use by IonMonkey.
 * This code cannot re-enter Ion code. */
bool
js::RegExpTesterRaw(JSContext* cx, HandleObject regexp, HandleString input,
                    uint32_t lastIndex, bool sticky, int32_t* endIndex)
{
    MOZ_ASSERT(lastIndex <= INT32_MAX);

    size_t endIndexTmp = 0;
    RegExpRunStatus status = ExecuteRegExp(cx, regexp, input, lastIndex, sticky,
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
    RegExpRunStatus status = ExecuteRegExp(cx, regexp, string, 0, false,
                                           nullptr, &ignored, DontUpdateRegExpStatics);
    args.rval().setBoolean(status == RegExpRunStatus_Success);
    return status != RegExpRunStatus_Error;
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

    // Check if @@match, @@search, and exec are own data properties,
    // those values should be tested in selfhosted JS.
    bool has = false;
    if (!HasOwnDataPropertyPure(cx, proto, SYMBOL_TO_JSID(cx->wellKnownSymbols().match), &has))
        return false;
    if (!has) {
        *result = false;
        return true;
    }

    /*
    if (!HasOwnDataPropertyPure(cx, proto, SYMBOL_TO_JSID(cx->wellKnownSymbols().search), &has))
        return false;
    if (!has) {
        *result = false;
        return true;
    }
    */

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
