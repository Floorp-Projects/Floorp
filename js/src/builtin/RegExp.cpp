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

using namespace js;
using namespace js::types;

using mozilla::ArrayLength;

bool
js::CreateRegExpMatchResult(JSContext *cx, HandleString input, const MatchPairs &matches,
                            MutableHandleValue rval)
{
    JS_ASSERT(input);

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
    JSObject *templateObject = cx->compartment()->regExps.getOrCreateMatchResultTemplateObject(cx);
    if (!templateObject)
        return false;

    size_t numPairs = matches.length();
    JS_ASSERT(numPairs > 0);

    RootedObject arr(cx, NewDenseAllocatedArrayWithTemplate(cx, numPairs, templateObject));
    if (!arr)
        return false;

    /* Store a Value for each pair. */
    for (size_t i = 0; i < numPairs; i++) {
        const MatchPair &pair = matches[i];

        if (pair.isUndefined()) {
            JS_ASSERT(i != 0); /* Since we had a match, first pair must be present. */
            arr->setDenseInitializedLength(i + 1);
            arr->initDenseElement(i, UndefinedValue());
        } else {
            JSLinearString *str = js_NewDependentString(cx, input, pair.start, pair.length());
            if (!str)
                return false;
            arr->setDenseInitializedLength(i + 1);
            arr->initDenseElement(i, StringValue(str));
        }
    }

    /* Set the |index| property. (TemplateObject positions it in slot 0) */
    arr->nativeSetSlot(0, Int32Value(matches[0].start));

    /* Set the |input| property. (TemplateObject positions it in slot 1) */
    arr->nativeSetSlot(1, StringValue(input));

#ifdef DEBUG
    RootedValue test(cx);
    RootedId id(cx, NameToId(cx->names().index));
    if (!baseops::GetProperty(cx, arr, id, &test))
        return false;
    JS_ASSERT(test == arr->nativeGetSlot(0));
    id = NameToId(cx->names().input);
    if (!baseops::GetProperty(cx, arr, id, &test))
        return false;
    JS_ASSERT(test == arr->nativeGetSlot(1));
#endif

    rval.setObject(*arr);
    return true;
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
        if (status == RegExpRunStatus_Success && res) {
            if (!res->updateFromMatchPairs(cx, input, *matches.u.pairs))
                return RegExpRunStatus_Error;
        }
    }

    return status;
}

/* Legacy ExecuteRegExp behavior is baked into the JSAPI. */
bool
js::ExecuteRegExpLegacy(JSContext *cx, RegExpStatics *res, RegExpObject &reobj,
                        Handle<JSLinearString*> input_, const jschar *chars, size_t length,
                        size_t *lastIndex, bool test, MutableHandleValue rval)
{
    RegExpGuard shared(cx);
    if (!reobj.getShared(cx, &shared))
        return false;

    ScopedMatchPairs matches(&cx->tempLifoAlloc());
    MatchConduit conduit(&matches);

    RegExpRunStatus status =
        ExecuteRegExpImpl(cx, res, *shared, input_, chars, length, lastIndex, conduit);

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

    RootedString input(cx, input_);
    if (!input) {
        input = js_NewStringCopyN<CanGC>(cx, chars, length);
        if (!input)
            return false;
    }

    return CreateRegExpMatchResult(cx, input, matches, rval);
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
                    return nullptr;
                sb.infallibleAppend(oldChars, size_t(it - oldChars));
            }
            if (!sb.append('\\'))
                return nullptr;
        }

        if (!sb.empty() && !sb.append(*it))
            return nullptr;
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
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_NEWREGEXP_FLAGGED);
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
        source = ToAtom<CanGC>(cx, sourceValue);
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

    if (!js::RegExpShared::checkSyntax(cx, nullptr, escapedSourceStr))
        return false;

    RegExpStatics *res = cx->global()->getRegExpStatics();
    RegExpObject *reobj = builder.build(escapedSourceStr, RegExpFlag(flags | res->getFlags()));
    if (!reobj)
        return false;

    args.rval().setObject(*reobj);
    return true;
}

MOZ_ALWAYS_INLINE bool
IsRegExp(HandleValue v)
{
    return v.isObject() && v.toObject().is<RegExpObject>();
}

MOZ_ALWAYS_INLINE bool
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

MOZ_ALWAYS_INLINE bool
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
    name(JSContext *cx, unsigned argc, Value *vp)                               \
    {                                                                           \
        CallArgs args = CallArgsFromVp(argc, vp);                               \
        RegExpStatics *res = cx->global()->getRegExpStatics();                  \
        code;                                                                   \
    }

DEFINE_STATIC_GETTER(static_input_getter,        return res->createPendingInput(cx, args.rval()))
DEFINE_STATIC_GETTER(static_multiline_getter,    args.rval().setBoolean(res->multiline());
                                                 return true)
DEFINE_STATIC_GETTER(static_lastMatch_getter,    return res->createLastMatch(cx, args.rval()))
DEFINE_STATIC_GETTER(static_lastParen_getter,    return res->createLastParen(cx, args.rval()))
DEFINE_STATIC_GETTER(static_leftContext_getter,  return res->createLeftContext(cx, args.rval()))
DEFINE_STATIC_GETTER(static_rightContext_getter, return res->createRightContext(cx, args.rval()))

DEFINE_STATIC_GETTER(static_paren1_getter,       return res->createParen(cx, 1, args.rval()))
DEFINE_STATIC_GETTER(static_paren2_getter,       return res->createParen(cx, 2, args.rval()))
DEFINE_STATIC_GETTER(static_paren3_getter,       return res->createParen(cx, 3, args.rval()))
DEFINE_STATIC_GETTER(static_paren4_getter,       return res->createParen(cx, 4, args.rval()))
DEFINE_STATIC_GETTER(static_paren5_getter,       return res->createParen(cx, 5, args.rval()))
DEFINE_STATIC_GETTER(static_paren6_getter,       return res->createParen(cx, 6, args.rval()))
DEFINE_STATIC_GETTER(static_paren7_getter,       return res->createParen(cx, 7, args.rval()))
DEFINE_STATIC_GETTER(static_paren8_getter,       return res->createParen(cx, 8, args.rval()))
DEFINE_STATIC_GETTER(static_paren9_getter,       return res->createParen(cx, 9, args.rval()))

#define DEFINE_STATIC_SETTER(name, code)                                        \
    static bool                                                                 \
    name(JSContext *cx, unsigned argc, Value *vp)                               \
    {                                                                           \
        RegExpStatics *res = cx->global()->getRegExpStatics();                  \
        code;                                                                   \
        return true;                                                            \
    }

static bool
static_input_setter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RegExpStatics *res = cx->global()->getRegExpStatics();

    RootedString str(cx, ToString<CanGC>(cx, args.get(0)));
    if (!str)
        return false;

    res->setPendingInput(str);
    args.rval().setString(str);
    return true;
}

static bool
static_multiline_setter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RegExpStatics *res = cx->global()->getRegExpStatics();

    bool b = ToBoolean(args.get(0));
    res->setMultiline(cx, b);
    args.rval().setBoolean(b);
    return true;
}

static const JSPropertySpec regexp_static_props[] = {
    JS_PSGS("input", static_input_getter, static_input_setter,
            JSPROP_PERMANENT | JSPROP_ENUMERATE),
    JS_PSGS("multiline", static_multiline_getter, static_multiline_setter,
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
    JS_PSGS("$*", static_multiline_getter, static_multiline_setter, JSPROP_PERMANENT),
    JS_PSG("$&", static_lastMatch_getter, JSPROP_PERMANENT),
    JS_PSG("$+", static_lastParen_getter, JSPROP_PERMANENT),
    JS_PSG("$`", static_leftContext_getter, JSPROP_PERMANENT),
    JS_PSG("$'", static_rightContext_getter, JSPROP_PERMANENT),
    JS_PS_END
};

JSObject *
js_InitRegExpClass(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->isNative());

    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());

    RootedObject proto(cx, global->createBlankPrototype(cx, &RegExpObject::class_));
    if (!proto)
        return nullptr;
    proto->setPrivate(nullptr);

    HandlePropertyName empty = cx->names().empty;
    RegExpObjectBuilder builder(cx, &proto->as<RegExpObject>());
    if (!builder.build(empty, RegExpFlag(0)))
        return nullptr;

    if (!DefinePropertiesAndBrand(cx, proto, nullptr, regexp_methods))
        return nullptr;

    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, regexp_construct, cx->names().RegExp, 2);
    if (!ctor)
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    /* Add static properties to the RegExp constructor. */
    if (!JS_DefineProperties(cx, ctor, regexp_static_props))
        return nullptr;

    if (!GlobalObject::initBuiltinConstructor(cx, global, JSProto_RegExp, ctor, proto))
        return nullptr;

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
                         : nullptr;

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
regexp_exec_impl(JSContext *cx, HandleObject regexp, HandleString string,
                 RegExpStaticsUpdate staticsUpdate, MutableHandleValue rval)
{
    /* Execute regular expression and gather matches. */
    ScopedMatchPairs matches(&cx->tempLifoAlloc());
    MatchConduit conduit(&matches);

    RegExpRunStatus status = ExecuteRegExp(cx, regexp, string, conduit, staticsUpdate);

    if (status == RegExpRunStatus_Error)
        return false;

    if (status == RegExpRunStatus_Success_NotFound) {
        rval.setNull();
        return true;
    }

    return CreateRegExpMatchResult(cx, string, matches, rval);
}

static bool
regexp_exec_impl(JSContext *cx, CallArgs args)
{
    RootedObject regexp(cx, &args.thisv().toObject());
    RootedString string(cx, ToString<CanGC>(cx, args.get(0)));
    if (!string)
        return false;

    return regexp_exec_impl(cx, regexp, string, UpdateRegExpStatics, args.rval());
}

bool
js::regexp_exec(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsRegExp, regexp_exec_impl, args);
}

/* Separate interface for use by IonMonkey. */
bool
js::regexp_exec_raw(JSContext *cx, HandleObject regexp, HandleString input, MutableHandleValue output)
{
    return regexp_exec_impl(cx, regexp, input, UpdateRegExpStatics, output);
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

    return regexp_exec_impl(cx, regexp, string, DontUpdateRegExpStatics, args.rval());
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
