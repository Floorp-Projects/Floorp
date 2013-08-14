/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/RegExp.h"

#include "jscntxt.h"

#include "vm/RegExpStatics.h"
#include "vm/StringBuffer.h"

#include "jsobjinlines.h"

#include "vm/RegExpObject-inl.h"
#include "vm/RegExpStatics-inl.h"

using namespace js;
using namespace js::types;

using mozilla::ArrayLength;

static inline bool
DefinePropertyHelper(JSContext *cx, HandleObject obj, Handle<PropertyName*> name, HandleValue v)
{
    return !!baseops::DefineProperty(cx, obj, name, v,
                                     JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE);
}

bool
js::CreateRegExpMatchResult(JSContext *cx, HandleString input_, const jschar *chars, size_t length,
                            MatchPairs &matches, MutableHandleValue rval)
{
    RootedString input(cx, input_);
    RootedValue undefinedValue(cx, UndefinedValue());

    /*
     * Create the (slow) result array for a match.
     *
     * Array contents:
     *  0:              matched string
     *  1..pairCount-1: paren matches
     *  input:          input string
     *  index:          start index for the match
     */
    if (!input) {
        input = js_NewStringCopyN<CanGC>(cx, chars, length);
        if (!input)
            return false;
    }

    size_t numPairs = matches.length();
    JS_ASSERT(numPairs > 0);

    AutoValueVector elements(cx);
    if (!elements.reserve(numPairs))
        return false;

    /* Accumulate a Value for each pair, in a rooted vector. */
    for (size_t i = 0; i < numPairs; ++i) {
        const MatchPair &pair = matches[i];

        if (pair.isUndefined()) {
            JS_ASSERT(i != 0); /* Since we had a match, first pair must be present. */
            elements.infallibleAppend(undefinedValue);
        } else {
            JSLinearString *str = js_NewDependentString(cx, input, pair.start, pair.length());
            if (!str)
                return false;
            elements.infallibleAppend(StringValue(str));
        }
    }

    /* Copy the rooted vector into the array object. */
    RootedObject array(cx, NewDenseCopiedArray(cx, elements.length(), elements.begin()));
    if (!array)
        return false;

    /* Set the |index| property. */
    RootedValue index(cx, Int32Value(matches[0].start));
    if (!DefinePropertyHelper(cx, array, cx->names().index, index))
        return false;

    /* Set the |input| property. */
    RootedValue inputVal(cx, StringValue(input));
    if (!DefinePropertyHelper(cx, array, cx->names().input, inputVal))
        return false;

    rval.setObject(*array);
    return true;
}

bool
js::CreateRegExpMatchResult(JSContext *cx, HandleString string, MatchPairs &matches,
                            MutableHandleValue rval)
{
    Rooted<JSLinearString*> input(cx, string->ensureLinear(cx));
    if (!input)
        return false;
    return CreateRegExpMatchResult(cx, input, input->chars(), input->length(), matches, rval);
}

static RegExpRunStatus
ExecuteRegExpImpl(JSContext *cx, RegExpStatics *res, RegExpShared &re,
                  Handle<JSLinearString*> input, const jschar *chars, size_t length,
                  size_t *lastIndex, MatchConduit &matches)
{
    RegExpRunStatus status;

    /* Switch between MatchOnly and IncludeSubpatterns modes. */
    if (matches.isPair) {
        size_t lastIndex_orig = *lastIndex;
        /* Only one MatchPair slot provided: execute short-circuiting regexp. */
        status = re.executeMatchOnly(cx, chars, length, lastIndex, *matches.u.pair);
        if (status == RegExpRunStatus_Success && res)
            res->updateLazily(cx, input, &re, lastIndex_orig);
    } else {
        /* Vector of MatchPairs provided: execute full regexp. */
        status = re.execute(cx, chars, length, lastIndex, *matches.u.pairs);
        if (status == RegExpRunStatus_Success && res)
            res->updateFromMatchPairs(cx, input, *matches.u.pairs);
    }

    return status;
}

/* Legacy ExecuteRegExp behavior is baked into the JSAPI. */
bool
js::ExecuteRegExpLegacy(JSContext *cx, RegExpStatics *res, RegExpObject &reobj,
                        Handle<JSLinearString*> input, const jschar *chars, size_t length,
                        size_t *lastIndex, bool test, MutableHandleValue rval)
{
    RegExpGuard shared(cx);
    if (!reobj.getShared(cx, &shared))
        return false;

    ScopedMatchPairs matches(&cx->tempLifoAlloc());
    MatchConduit conduit(&matches);

    RegExpRunStatus status =
        ExecuteRegExpImpl(cx, res, *shared, input, chars, length, lastIndex, conduit);

    if (status == RegExpRunStatus_Error)
        return false;

    if (status == RegExpRunStatus_Success_NotFound) {
        /* ExecuteRegExp() previously returned an array or null. */
        rval.setNull();
        return true;
    }

    if (test) {
        /* Forbid an array, as an optimization. */
        rval.setBoolean(true);
        return true;
    }

    return CreateRegExpMatchResult(cx, input, chars, length, matches, rval);
}

/* Note: returns the original if no escaping need be performed. */
static JSAtom *
EscapeNakedForwardSlashes(JSContext *cx, HandleAtom unescaped)
{
    size_t oldLen = unescaped->length();
    const jschar *oldChars = unescaped->chars();

    JS::Anchor<JSString *> anchor(unescaped);

    /* We may never need to use |sb|. Start using it lazily. */
    StringBuffer sb(cx);

    for (const jschar *it = oldChars; it < oldChars + oldLen; ++it) {
        if (*it == '/' && (it == oldChars || it[-1] != '\\')) {
            /* There's a forward slash that needs escaping. */
            if (sb.empty()) {
                /* This is the first one we've seen, copy everything up to this point. */
                if (!sb.reserve(oldLen + 1))
                    return NULL;
                sb.infallibleAppend(oldChars, size_t(it - oldChars));
            }
            if (!sb.append('\\'))
                return NULL;
        }

        if (!sb.empty() && !sb.append(*it))
            return NULL;
    }

    return sb.empty() ? (JSAtom *)unescaped : sb.finishAtom();
}

/*
 * Compile a new |RegExpShared| for the |RegExpObject|.
 *
 * Per ECMAv5 15.10.4.1, we act on combinations of (pattern, flags) as
 * arguments:
 *
 *  RegExp, undefined => flags := pattern.flags
 *  RegExp, _ => throw TypeError
 *  _ => pattern := ToString(pattern) if defined(pattern) else ''
 *       flags := ToString(flags) if defined(flags) else ''
 */
static bool
CompileRegExpObject(JSContext *cx, RegExpObjectBuilder &builder, CallArgs args)
{
    if (args.length() == 0) {
        RegExpStatics *res = cx->global()->getRegExpStatics();
        Rooted<JSAtom*> empty(cx, cx->runtime()->emptyString);
        RegExpObject *reobj = builder.build(empty, res->getFlags());
        if (!reobj)
            return false;
        args.rval().setObject(*reobj);
        return true;
    }

    RootedValue sourceValue(cx, args[0]);

    /*
     * If we get passed in an object whose internal [[Class]] property is
     * "RegExp", return a new object with the same source/flags.
     */
    if (IsObjectWithClass(sourceValue, ESClass_RegExp, cx)) {
        /*
         * Beware, sourceObj may be a (transparent) proxy to a RegExp, so only
         * use generic (proxyable) operations on sourceObj that do not assume
         * sourceObj.is<RegExpObject>().
         */
        RootedObject sourceObj(cx, &sourceValue.toObject());

        if (args.hasDefined(1)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NEWREGEXP_FLAGGED);
            return false;
        }

        /*
         * Only extract the 'flags' out of sourceObj; do not reuse the
         * RegExpShared since it may be from a different compartment.
         */
        RegExpFlag flags;
        {
            RegExpGuard g(cx);
            if (!RegExpToShared(cx, sourceObj, &g))
                return false;

            flags = g->getFlags();
        }

        /*
         * 'toSource' is a permanent read-only property, so this is equivalent
         * to executing RegExpObject::getSource on the unwrapped object.
         */
        RootedValue v(cx);
        if (!JSObject::getProperty(cx, sourceObj, sourceObj, cx->names().source, &v))
            return false;

        Rooted<JSAtom*> sourceAtom(cx, &v.toString()->asAtom());
        RegExpObject *reobj = builder.build(sourceAtom, flags);
        if (!reobj)
            return false;

        args.rval().setObject(*reobj);
        return true;
    }

    RootedAtom source(cx);
    if (sourceValue.isUndefined()) {
        source = cx->runtime()->emptyString;
    } else {
        /* Coerce to string and compile. */
        JSString *str = ToString<CanGC>(cx, sourceValue);
        if (!str)
            return false;

        source = AtomizeString<CanGC>(cx, str);
        if (!source)
            return false;
    }

    RegExpFlag flags = RegExpFlag(0);
    if (args.hasDefined(1)) {
        RootedString flagStr(cx, ToString<CanGC>(cx, args[1]));
        if (!flagStr)
            return false;
        args[1].setString(flagStr);
        if (!ParseRegExpFlags(cx, flagStr, &flags))
            return false;
    }

    RootedAtom escapedSourceStr(cx, EscapeNakedForwardSlashes(cx, source));
    if (!escapedSourceStr)
        return false;

    if (!js::RegExpShared::checkSyntax(cx, NULL, escapedSourceStr))
        return false;

    RegExpStatics *res = cx->global()->getRegExpStatics();
    RegExpObject *reobj = builder.build(escapedSourceStr, RegExpFlag(flags | res->getFlags()));
    if (!reobj)
        return false;

    args.rval().setObject(*reobj);
    return true;
}

JS_ALWAYS_INLINE bool
IsRegExp(const Value &v)
{
    return v.isObject() && v.toObject().is<RegExpObject>();
}

JS_ALWAYS_INLINE bool
regexp_compile_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsRegExp(args.thisv()));
    RegExpObjectBuilder builder(cx, &args.thisv().toObject().as<RegExpObject>());
    return CompileRegExpObject(cx, builder, args);
}

static bool
regexp_compile(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsRegExp, regexp_compile_impl>(cx, args);
}

static bool
regexp_construct(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.isConstructing()) {
        /*
         * If first arg is regexp and no flags are given, just return the arg.
         * Otherwise, delegate to the standard constructor.
         * See ECMAv5 15.10.3.1.
         */
        if (args.hasDefined(0) &&
            IsObjectWithClass(args[0], ESClass_RegExp, cx) &&
            !args.hasDefined(1))
        {
            args.rval().set(args[0]);
            return true;
        }
    }

    RegExpObjectBuilder builder(cx);
    return CompileRegExpObject(cx, builder, args);
}

JS_ALWAYS_INLINE bool
regexp_toString_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsRegExp(args.thisv()));

    JSString *str = args.thisv().toObject().as<RegExpObject>().toString(cx);
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

static bool
regexp_toString(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsRegExp, regexp_toString_impl>(cx, args);
}

static const JSFunctionSpec regexp_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,  regexp_toString,    0,0),
#endif
    JS_FN(js_toString_str,  regexp_toString,    0,0),
    JS_FN("compile",        regexp_compile,     2,0),
    JS_FN("exec",           regexp_exec,        1,0),
    JS_FN("test",           regexp_test,        1,0),
    JS_FS_END
};

/*
 * RegExp static properties.
 *
 * RegExp class static properties and their Perl counterparts:
 *
 *  RegExp.input                $_
 *  RegExp.multiline            $*
 *  RegExp.lastMatch            $&
 *  RegExp.lastParen            $+
 *  RegExp.leftContext          $`
 *  RegExp.rightContext         $'
 */

#define DEFINE_STATIC_GETTER(name, code)                                        \
    static bool                                                                 \
    name(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)   \
    {                                                                           \
        RegExpStatics *res = cx->global()->getRegExpStatics();                  \
        code;                                                                   \
    }

DEFINE_STATIC_GETTER(static_input_getter,        return res->createPendingInput(cx, vp))
DEFINE_STATIC_GETTER(static_multiline_getter,    vp.setBoolean(res->multiline());
                                                 return true)
DEFINE_STATIC_GETTER(static_lastMatch_getter,    return res->createLastMatch(cx, vp))
DEFINE_STATIC_GETTER(static_lastParen_getter,    return res->createLastParen(cx, vp))
DEFINE_STATIC_GETTER(static_leftContext_getter,  return res->createLeftContext(cx, vp))
DEFINE_STATIC_GETTER(static_rightContext_getter, return res->createRightContext(cx, vp))

DEFINE_STATIC_GETTER(static_paren1_getter,       return res->createParen(cx, 1, vp))
DEFINE_STATIC_GETTER(static_paren2_getter,       return res->createParen(cx, 2, vp))
DEFINE_STATIC_GETTER(static_paren3_getter,       return res->createParen(cx, 3, vp))
DEFINE_STATIC_GETTER(static_paren4_getter,       return res->createParen(cx, 4, vp))
DEFINE_STATIC_GETTER(static_paren5_getter,       return res->createParen(cx, 5, vp))
DEFINE_STATIC_GETTER(static_paren6_getter,       return res->createParen(cx, 6, vp))
DEFINE_STATIC_GETTER(static_paren7_getter,       return res->createParen(cx, 7, vp))
DEFINE_STATIC_GETTER(static_paren8_getter,       return res->createParen(cx, 8, vp))
DEFINE_STATIC_GETTER(static_paren9_getter,       return res->createParen(cx, 9, vp))

#define DEFINE_STATIC_SETTER(name, code)                                        \
    static bool                                                                 \
    name(JSContext *cx, HandleObject obj, HandleId id, bool strict, MutableHandleValue vp)\
    {                                                                           \
        RegExpStatics *res = cx->global()->getRegExpStatics();                  \
        code;                                                                   \
        return true;                                                            \
    }

DEFINE_STATIC_SETTER(static_input_setter,
                     if (!JSVAL_IS_STRING(vp) && !JS_ConvertValue(cx, vp, JSTYPE_STRING, vp.address()))
                         return false;
                     res->setPendingInput(JSVAL_TO_STRING(vp)))
DEFINE_STATIC_SETTER(static_multiline_setter,
                     if (!JSVAL_IS_BOOLEAN(vp) && !JS_ConvertValue(cx, vp, JSTYPE_BOOLEAN, vp.address()))
                         return false;
                     res->setMultiline(cx, !!JSVAL_TO_BOOLEAN(vp)))

const uint8_t REGEXP_STATIC_PROP_ATTRS    = JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_ENUMERATE;
const uint8_t RO_REGEXP_STATIC_PROP_ATTRS = REGEXP_STATIC_PROP_ATTRS | JSPROP_READONLY;

const uint8_t HIDDEN_PROP_ATTRS = JSPROP_PERMANENT | JSPROP_SHARED;
const uint8_t RO_HIDDEN_PROP_ATTRS = HIDDEN_PROP_ATTRS | JSPROP_READONLY;

static const JSPropertySpec regexp_static_props[] = {
    {"input",        0, REGEXP_STATIC_PROP_ATTRS,    JSOP_WRAPPER(static_input_getter),
                                                     JSOP_WRAPPER(static_input_setter)},
    {"multiline",    0, REGEXP_STATIC_PROP_ATTRS,    JSOP_WRAPPER(static_multiline_getter),
                                                     JSOP_WRAPPER(static_multiline_setter)},
    {"lastMatch",    0, RO_REGEXP_STATIC_PROP_ATTRS, JSOP_WRAPPER(static_lastMatch_getter),
                                                     JSOP_NULLWRAPPER},
    {"lastParen",    0, RO_REGEXP_STATIC_PROP_ATTRS, JSOP_WRAPPER(static_lastParen_getter),
                                                     JSOP_NULLWRAPPER},
    {"leftContext",  0, RO_REGEXP_STATIC_PROP_ATTRS, JSOP_WRAPPER(static_leftContext_getter),
                                                     JSOP_NULLWRAPPER},
    {"rightContext", 0, RO_REGEXP_STATIC_PROP_ATTRS, JSOP_WRAPPER(static_rightContext_getter),
                                                     JSOP_NULLWRAPPER},
    {"$1",           0, RO_REGEXP_STATIC_PROP_ATTRS, JSOP_WRAPPER(static_paren1_getter),
                                                     JSOP_NULLWRAPPER},
    {"$2",           0, RO_REGEXP_STATIC_PROP_ATTRS, JSOP_WRAPPER(static_paren2_getter),
                                                     JSOP_NULLWRAPPER},
    {"$3",           0, RO_REGEXP_STATIC_PROP_ATTRS, JSOP_WRAPPER(static_paren3_getter),
                                                     JSOP_NULLWRAPPER},
    {"$4",           0, RO_REGEXP_STATIC_PROP_ATTRS, JSOP_WRAPPER(static_paren4_getter),
                                                     JSOP_NULLWRAPPER},
    {"$5",           0, RO_REGEXP_STATIC_PROP_ATTRS, JSOP_WRAPPER(static_paren5_getter),
                                                     JSOP_NULLWRAPPER},
    {"$6",           0, RO_REGEXP_STATIC_PROP_ATTRS, JSOP_WRAPPER(static_paren6_getter),
                                                     JSOP_NULLWRAPPER},
    {"$7",           0, RO_REGEXP_STATIC_PROP_ATTRS, JSOP_WRAPPER(static_paren7_getter),
                                                     JSOP_NULLWRAPPER},
    {"$8",           0, RO_REGEXP_STATIC_PROP_ATTRS, JSOP_WRAPPER(static_paren8_getter),
                                                     JSOP_NULLWRAPPER},
    {"$9",           0, RO_REGEXP_STATIC_PROP_ATTRS, JSOP_WRAPPER(static_paren9_getter),
                                                     JSOP_NULLWRAPPER},
    {"$_",           0, HIDDEN_PROP_ATTRS,           JSOP_WRAPPER(static_input_getter),
                                                     JSOP_WRAPPER(static_input_setter)},
    {"$*",           0, HIDDEN_PROP_ATTRS,           JSOP_WRAPPER(static_multiline_getter),
                                                     JSOP_WRAPPER(static_multiline_setter)},
    {"$&",           0, RO_HIDDEN_PROP_ATTRS,        JSOP_WRAPPER(static_lastMatch_getter),
                                                     JSOP_NULLWRAPPER},
    {"$+",           0, RO_HIDDEN_PROP_ATTRS,        JSOP_WRAPPER(static_lastParen_getter),
                                                     JSOP_NULLWRAPPER},
    {"$`",           0, RO_HIDDEN_PROP_ATTRS,        JSOP_WRAPPER(static_leftContext_getter),
                                                     JSOP_NULLWRAPPER},
    {"$'",           0, RO_HIDDEN_PROP_ATTRS,        JSOP_WRAPPER(static_rightContext_getter),
                                                     JSOP_NULLWRAPPER},
    {0,0,0,JSOP_NULLWRAPPER,JSOP_NULLWRAPPER}
};

JSObject *
js_InitRegExpClass(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->isNative());

    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());

    RootedObject proto(cx, global->createBlankPrototype(cx, &RegExpObject::class_));
    if (!proto)
        return NULL;
    proto->setPrivate(NULL);

    HandlePropertyName empty = cx->names().empty;
    RegExpObjectBuilder builder(cx, &proto->as<RegExpObject>());
    if (!builder.build(empty, RegExpFlag(0)))
        return NULL;

    if (!DefinePropertiesAndBrand(cx, proto, NULL, regexp_methods))
        return NULL;

    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, regexp_construct, cx->names().RegExp, 2);
    if (!ctor)
        return NULL;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return NULL;

    /* Add static properties to the RegExp constructor. */
    if (!JS_DefineProperties(cx, ctor, regexp_static_props))
        return NULL;

    if (!DefineConstructorAndPrototype(cx, global, JSProto_RegExp, ctor, proto))
        return NULL;

    return proto;
}

RegExpRunStatus
js::ExecuteRegExp(JSContext *cx, HandleObject regexp, HandleString string,
                  MatchConduit &matches, RegExpStaticsUpdate staticsUpdate)
{
    /* Step 1 (b) was performed by CallNonGenericMethod. */
    Rooted<RegExpObject*> reobj(cx, &regexp->as<RegExpObject>());

    RegExpGuard re(cx);
    if (!reobj->getShared(cx, &re))
        return RegExpRunStatus_Error;

    RegExpStatics *res = (staticsUpdate == UpdateRegExpStatics)
                         ? cx->global()->getRegExpStatics()
                         : NULL;

    /* Step 3. */
    Rooted<JSLinearString*> input(cx, string->ensureLinear(cx));
    if (!input)
        return RegExpRunStatus_Error;

    /* Step 4. */
    RootedValue lastIndex(cx, reobj->getLastIndex());
    size_t length = input->length();

    /* Step 5. */
    int i;
    if (lastIndex.isInt32()) {
        /* Aggressively avoid doubles. */
        i = lastIndex.toInt32();
    } else {
        double d;
        if (!ToInteger(cx, lastIndex, &d))
            return RegExpRunStatus_Error;

        /* Inlined steps 6, 7, 9a with doubles to detect failure case. */
        if ((re->global() || re->sticky()) && (d < 0 || d > length)) {
            reobj->zeroLastIndex();
            return RegExpRunStatus_Success_NotFound;
        }

        i = int(d);
    }

    /* Steps 6-7 (with sticky extension). */
    if (!re->global() && !re->sticky())
        i = 0;

    /* Step 9a. */
    if (i < 0 || size_t(i) > length) {
        reobj->zeroLastIndex();
        return RegExpRunStatus_Success_NotFound;
    }

    /* Steps 8-21. */
    const jschar *chars = input->chars();
    size_t lastIndexInt(i);
    RegExpRunStatus status =
        ExecuteRegExpImpl(cx, res, *re, input, chars, length, &lastIndexInt, matches);

    if (status == RegExpRunStatus_Error)
        return RegExpRunStatus_Error;

    /* Steps 9a and 11 (with sticky extension). */
    if (status == RegExpRunStatus_Success_NotFound)
        reobj->zeroLastIndex();
    else if (re->global() || re->sticky())
        reobj->setLastIndex(lastIndexInt);

    return status;
}

/* ES5 15.10.6.2 (and 15.10.6.3, which calls 15.10.6.2). */
static RegExpRunStatus
ExecuteRegExp(JSContext *cx, CallArgs args, MatchConduit &matches)
{
    /* Step 1 (a) was performed by CallNonGenericMethod. */
    RootedObject regexp(cx, &args.thisv().toObject());

    /* Step 2. */
    RootedString string(cx, ToString<CanGC>(cx, args.get(0)));
    if (!string)
        return RegExpRunStatus_Error;

    return ExecuteRegExp(cx, regexp, string, matches, UpdateRegExpStatics);
}

/* ES5 15.10.6.2. */
static bool
regexp_exec_impl(JSContext *cx, CallArgs args, HandleObject regexp, HandleString string,
                 RegExpStaticsUpdate staticsUpdate)
{
    /* Execute regular expression and gather matches. */
    ScopedMatchPairs matches(&cx->tempLifoAlloc());
    MatchConduit conduit(&matches);

    RegExpRunStatus status = ExecuteRegExp(cx, regexp, string, conduit, staticsUpdate);

    if (status == RegExpRunStatus_Error)
        return false;

    if (status == RegExpRunStatus_Success_NotFound) {
        args.rval().setNull();
        return true;
    }

    return CreateRegExpMatchResult(cx, string, matches, args.rval());
}

static bool
regexp_exec_impl(JSContext *cx, CallArgs args)
{
    RootedObject regexp(cx, &args.thisv().toObject());
    RootedString string(cx, ToString<CanGC>(cx, args.get(0)));
    if (!string)
        return false;

    return regexp_exec_impl(cx, args, regexp, string, UpdateRegExpStatics);
}

bool
js::regexp_exec(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsRegExp, regexp_exec_impl, args);
}

bool
js::regexp_exec_no_statics(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 2);
    JS_ASSERT(IsRegExp(args[0]));
    JS_ASSERT(args[1].isString());

    RootedObject regexp(cx, &args[0].toObject());
    RootedString string(cx, args[1].toString());

    return regexp_exec_impl(cx, args, regexp, string, DontUpdateRegExpStatics);
}

/* ES5 15.10.6.3. */
static bool
regexp_test_impl(JSContext *cx, CallArgs args)
{
    MatchPair match;
    MatchConduit conduit(&match);
    RegExpRunStatus status = ExecuteRegExp(cx, args, conduit);
    args.rval().setBoolean(status == RegExpRunStatus_Success);
    return (status != RegExpRunStatus_Error);
}

/* Separate interface for use by IonMonkey. */
bool
js::regexp_test_raw(JSContext *cx, HandleObject regexp, HandleString input, bool *result)
{
    MatchPair match;
    MatchConduit conduit(&match);
    RegExpRunStatus status = ExecuteRegExp(cx, regexp, input, conduit, UpdateRegExpStatics);
    *result = (status == RegExpRunStatus_Success);
    return (status != RegExpRunStatus_Error);
}

bool
js::regexp_test(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsRegExp, regexp_test_impl, args);
}

bool
js::regexp_test_no_statics(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 2);
    JS_ASSERT(IsRegExp(args[0]));
    JS_ASSERT(args[1].isString());

    RootedObject regexp(cx, &args[0].toObject());
    RootedString string(cx, args[1].toString());

    MatchPair match;
    MatchConduit conduit(&match);
    RegExpRunStatus status = ExecuteRegExp(cx, regexp, string, conduit, DontUpdateRegExpStatics);
    args.rval().setBoolean(status == RegExpRunStatus_Success);
    return (status != RegExpRunStatus_Error);
}
