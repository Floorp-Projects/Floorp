/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set sw=4 ts=8 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * JS regular expressions, after Perl.
 */
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsregexp.h"
#include "jsstr.h"
#include "jsvector.h"

#include "jsobjinlines.h"
#include "jsregexpinlines.h"

#ifdef JS_TRACER
#include "jstracer.h"
using namespace avmplus;
using namespace nanojit;
#endif

using namespace js;


class RegExpMatchBuilder
{
    JSContext   *cx;
    JSObject    *array;

  public:
    RegExpMatchBuilder(JSContext *cx, JSObject *array) : cx(cx), array(array) {}
    bool append(int index, JSString *str) {
        JS_ASSERT(str);
        return append(INT_TO_JSID(index), StringValue(str));
    }

    bool append(jsid id, Value val) {
        return !!js_DefineProperty(cx, array, id, &val, js::PropertyStub, js::PropertyStub,
                                   JSPROP_ENUMERATE);
    }

    bool appendIndex(int index) {
        return append(ATOM_TO_JSID(cx->runtime->atomState.indexAtom), Int32Value(index));
    }

    /* Sets the input attribute of the match array. */
    bool appendInput(JSString *str) {
        JS_ASSERT(str);
        return append(ATOM_TO_JSID(cx->runtime->atomState.inputAtom), StringValue(str));
    }
};

/*
 * Lock obj and replace its regexp internals with |newRegExp|.
 * Decref the replaced regexp internals.
 */
static void
SwapObjectRegExp(JSContext *cx, JSObject *obj, RegExp &newRegExp)
{
    RegExp *oldRegExp;
    {
        AutoObjectLocker lock(cx, obj);
        oldRegExp = RegExp::extractFrom(obj);
        obj->setPrivate(&newRegExp);
        obj->zeroRegExpLastIndex();
    }
    if (oldRegExp)
        oldRegExp->decref(cx);
}

void
js_SaveAndClearRegExpStatics(JSContext *cx, js::RegExpStatics *statics, js::AutoStringRooter *tvr)
{
    JS_ASSERT(statics);
    statics->clone(cx->regExpStatics);
    if (statics->getInput())
        tvr->setString(statics->getInput());
    cx->regExpStatics.clear();
}

void
js_RestoreRegExpStatics(JSContext *cx, js::RegExpStatics *statics)
{
    JS_ASSERT(statics);
    cx->regExpStatics.clone(*statics);
}

JSObject * JS_FASTCALL
js_CloneRegExpObject(JSContext *cx, JSObject *obj, JSObject *proto)
{
    JS_ASSERT(obj->getClass() == &js_RegExpClass);
    JS_ASSERT(proto);
    JS_ASSERT(proto->getClass() == &js_RegExpClass);
    JSObject *clone = NewNativeClassInstance(cx, &js_RegExpClass, proto, proto->getParent());
    if (!clone)
        return NULL;
    RegExp *re = RegExp::extractFrom(obj);
    {
        uint32 origFlags = re->getFlags();
        uint32 staticsFlags = cx->regExpStatics.getFlags();
        if ((origFlags & staticsFlags) != staticsFlags) {
            /*
             * This regex is lacking flags from the statics, so we must recompile with the new
             * flags instead of increffing.
             */
            re = RegExp::create(cx, re->getSource(), origFlags | staticsFlags);
        } else {
            re->incref(cx);
        }
    }
    clone->setPrivate(re);
    clone->zeroRegExpLastIndex();
    return clone;
}

#ifdef JS_TRACER
JS_DEFINE_CALLINFO_3(extern, OBJECT, js_CloneRegExpObject, CONTEXT, OBJECT, OBJECT, 0,
                     ACCSET_STORE_ANY)
#endif

JSBool
js_ObjectIsRegExp(JSObject *obj)
{
    return obj->isRegExp();
}

/*
 * js::RegExp
 */

bool
RegExp::execute(JSContext *cx, JSString *input, size_t *lastIndex, bool test, Value *rval)
{
#if !ENABLE_YARR_JIT
    JS_ASSERT(compiled);
#endif
    const size_t pairCount = parenCount + 1;
    const size_t bufCount = pairCount * 3; /* Should be x2, but PCRE has... needs. */
    const size_t matchItemCount = pairCount * 2;

    /*
     * The regular expression arena pool is special... we want to hang on to it
     * until a GC is performed so rapid subsequent regexp executions don't
     * thrash malloc/freeing arena chunks.
     *
     * Stick a timestamp at the base of that pool.
     */
    if (!cx->regExpPool.first.next) {
        int64 *timestamp;
        JS_ARENA_ALLOCATE_CAST(timestamp, int64 *, &cx->regExpPool, sizeof *timestamp);
        if (!timestamp)
            return false;
        *timestamp = JS_Now();
    }

    AutoArenaAllocator aaa(&cx->regExpPool);
    int *buf = aaa.alloc<int>(bufCount);
    if (!buf)
        return false;
    /*
     * The JIT regexp procedure doesn't always initialize matchPair values.
     * Maybe we can make this faster by ensuring it does?
     */
    for (int *it = buf; it != buf + matchItemCount; ++it)
        *it = -1;
    const jschar *chars = input->chars();
    size_t len = input->length();
    size_t inputOffset = 0;
    if (sticky()) {
        /* Sticky matches at the last index for the regexp object. */
        chars += *lastIndex;
        len -= *lastIndex;
        inputOffset = *lastIndex;
    }
#if ENABLE_YARR_JIT
    int result = JSC::Yarr::executeRegex(cx, compiled, chars, *lastIndex - inputOffset, len, buf,
                                         bufCount);
#else
    int result = jsRegExpExecute(cx, compiled, chars, len, *lastIndex - inputOffset, buf, 
                                 bufCount) < 0 ? -1 : buf[0];
#endif
    if (result == -1) {
        *rval = NullValue();
        return true;
    }
    RegExpStatics &statics = cx->regExpStatics;
    statics.input = input;
    statics.matchPairs.clear();
    if (!statics.matchPairs.reserve(matchItemCount))
        return false;
    for (size_t idx = 0; idx < matchItemCount; idx += 2) {
        JS_ASSERT(buf[idx + 1] >= buf[idx]);
        if (!statics.matchPairs.append(buf[idx] + inputOffset))
            return false;
        if (!statics.matchPairs.append(buf[idx + 1] + inputOffset))
            return false;
    }
    *lastIndex = statics.matchPairs[1];
    if (test) {
        *rval = BooleanValue(true);
        return true;
    }

    /*
     * Create the return array for a match. Returned array contents:
     *  0:              matched string
     *  1..parenCount:  paren matches
     */
    JSObject *array = js_NewSlowArrayObject(cx);
    if (!array)
        return false;
    *rval = ObjectValue(*array);
    RegExpMatchBuilder builder(cx, array);
    for (size_t idx = 0; idx < matchItemCount; idx += 2) {
        int start = statics.matchPairs[idx];
        int end = statics.matchPairs[idx + 1];
        JSString *captured;
        if (start >= 0) {
            JS_ASSERT(start <= end);
            JS_ASSERT((unsigned) end <= input->length());
            captured = js_NewDependentString(cx, input, start, end - start);
            if (!(captured && builder.append(idx / 2, captured)))
                return false;
        } else {
            /* Missing parenthesized match. */
            JS_ASSERT(idx != 0); /* Since we had a match, first pair must be present. */
            JS_ASSERT(start == end && end == -1);
            if (!builder.append(INT_TO_JSID(idx / 2), UndefinedValue()))
                return false;
        }
    }
    return builder.appendIndex(statics.matchPairs[0]) && builder.appendInput(input);
}

void
RegExp::handleYarrError(JSContext *cx, int error)
{
    /* Hack: duplicated from yarr/yarr/RegexParser.h */
    enum ErrorCode {
        NoError,
        PatternTooLarge,
        QuantifierOutOfOrder,
        QuantifierWithoutAtom,
        MissingParentheses,
        ParenthesesUnmatched,
        ParenthesesTypeInvalid,     /* "(?" with bad next char or end of pattern. */
        CharacterClassUnmatched,
        CharacterClassOutOfOrder,
        QuantifierTooLarge,
        EscapeUnterminated
    };
    switch (error) {
      case NoError:
        JS_NOT_REACHED("Precondition violation: an error must have occurred.");
        return;
#define COMPILE_EMSG(__code, __msg) \
      case __code: \
        JS_ReportErrorFlagsAndNumberUC(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL, __msg); \
        return
      COMPILE_EMSG(PatternTooLarge, JSMSG_REGEXP_TOO_COMPLEX);
      COMPILE_EMSG(QuantifierOutOfOrder, JSMSG_BAD_QUANTIFIER);
      COMPILE_EMSG(QuantifierWithoutAtom, JSMSG_BAD_QUANTIFIER);
      COMPILE_EMSG(MissingParentheses, JSMSG_MISSING_PAREN);
      COMPILE_EMSG(ParenthesesUnmatched, JSMSG_UNMATCHED_RIGHT_PAREN);
      COMPILE_EMSG(ParenthesesTypeInvalid, JSMSG_BAD_QUANTIFIER);
      COMPILE_EMSG(CharacterClassUnmatched, JSMSG_BAD_CLASS_RANGE);
      COMPILE_EMSG(CharacterClassOutOfOrder, JSMSG_BAD_CLASS_RANGE);
      COMPILE_EMSG(EscapeUnterminated, JSMSG_TRAILING_SLASH);
      COMPILE_EMSG(QuantifierTooLarge, JSMSG_BAD_QUANTIFIER);
#undef COMPILE_EMSG
      default:
        JS_NOT_REACHED("Precondition violation: unknown Yarr error code.");
    }
}

void
RegExp::handlePCREError(JSContext *cx, int error)
{
#define REPORT(__msg) \
    JS_ReportErrorFlagsAndNumberUC(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL, __msg); \
    return
    switch (error) {
      case 0: JS_NOT_REACHED("Precondition violation: an error must have occurred.");
      case 1: REPORT(JSMSG_TRAILING_SLASH);
      case 2: REPORT(JSMSG_TRAILING_SLASH);
      case 3: REPORT(JSMSG_REGEXP_TOO_COMPLEX);
      case 4: REPORT(JSMSG_BAD_QUANTIFIER);
      case 5: REPORT(JSMSG_BAD_QUANTIFIER);
      case 6: REPORT(JSMSG_BAD_CLASS_RANGE);
      case 7: REPORT(JSMSG_REGEXP_TOO_COMPLEX);
      case 8: REPORT(JSMSG_BAD_CLASS_RANGE);
      case 9: REPORT(JSMSG_BAD_QUANTIFIER);
      case 10: REPORT(JSMSG_UNMATCHED_RIGHT_PAREN);
      case 11: REPORT(JSMSG_REGEXP_TOO_COMPLEX);
      case 12: REPORT(JSMSG_UNMATCHED_RIGHT_PAREN);
      case 13: REPORT(JSMSG_REGEXP_TOO_COMPLEX);
      case 14: REPORT(JSMSG_MISSING_PAREN);
      case 15: REPORT(JSMSG_BAD_BACKREF);
      case 16: REPORT(JSMSG_REGEXP_TOO_COMPLEX);
      case 17: REPORT(JSMSG_REGEXP_TOO_COMPLEX);
      default:
        JS_NOT_REACHED("Precondition violation: unknown PCRE error code.");
    }
#undef REPORT
}

bool
RegExp::parseFlags(JSContext *cx, JSString *flagStr, uint32 &flagsOut)
{
    const jschar *s;
    size_t n;
    flagStr->getCharsAndLength(s, n);
    flagsOut = 0;
    for (size_t i = 0; i < n; i++) {
#define HANDLE_FLAG(__name)                                             \
        JS_BEGIN_MACRO                                                  \
            if (flagsOut & (__name))                                    \
                goto bad_flag;                                          \
            flagsOut |= (__name);                                       \
        JS_END_MACRO
        switch (s[i]) {
          case 'i': HANDLE_FLAG(JSREG_FOLD); break;
          case 'g': HANDLE_FLAG(JSREG_GLOB); break;
          case 'm': HANDLE_FLAG(JSREG_MULTILINE); break;
          case 'y': HANDLE_FLAG(JSREG_STICKY); break;
          default:
          bad_flag:
          {
            char charBuf[2];
            charBuf[0] = char(s[i]);
            charBuf[1] = '\0';
            JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL,
                                         JSMSG_BAD_REGEXP_FLAG, charBuf);
            return false;
          }
        }
#undef HANDLE_FLAG
    }
    return true;
}

RegExp *
RegExp::createFlagged(JSContext *cx, JSString *str, JSString *opt)
{
    if (!opt)
        return create(cx, str, 0);
    uint32 flags = 0;
    if (!parseFlags(cx, opt, flags))
        return false;
    return create(cx, str, flags);
}

/*
 * RegExp instance properties.
 */
#define DEFINE_GETTER(name, code)                                              \
    static JSBool                                                              \
    name(JSContext *cx, JSObject *obj, jsid id, Value *vp)                     \
    {                                                                          \
        while (obj->getClass() != &js_RegExpClass) {                           \
            obj = obj->getProto();                                             \
            if (!obj)                                                          \
                return true;                                                   \
        }                                                                      \
        AutoObjectLocker(cx, obj);                                             \
        RegExp *re = RegExp::extractFrom(obj);                                 \
        code;                                                                  \
        return true;                                                           \
    }

/* lastIndex is stored in the object, re = re silences the compiler warning. */
DEFINE_GETTER(lastIndex_getter,  re = re; *vp = obj->getRegExpLastIndex())
DEFINE_GETTER(source_getter,     *vp = StringValue(re->getSource()))
DEFINE_GETTER(global_getter,     *vp = BooleanValue(re->global()))
DEFINE_GETTER(ignoreCase_getter, *vp = BooleanValue(re->ignoreCase()))
DEFINE_GETTER(multiline_getter,  *vp = BooleanValue(re->multiline()))
DEFINE_GETTER(sticky_getter,     *vp = BooleanValue(re->sticky()))

static JSBool
lastIndex_setter(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    while (obj->getClass() != &js_RegExpClass) {
        obj = obj->getProto();
        if (!obj)
            return true;
    }
    obj->setRegExpLastIndex(*vp);
    return true;
}

static const struct LazyProp {
    const char *name;
    uint16 atomOffset;
    PropertyOp getter;
} lazyRegExpProps[] = {
    { js_source_str,     ATOM_OFFSET(source),     source_getter },
    { js_global_str,     ATOM_OFFSET(global),     global_getter },
    { js_ignoreCase_str, ATOM_OFFSET(ignoreCase), ignoreCase_getter },
    { js_multiline_str,  ATOM_OFFSET(multiline),  multiline_getter },
    { js_sticky_str,     ATOM_OFFSET(sticky),     sticky_getter }
};

static JSBool
regexp_resolve(JSContext *cx, JSObject *obj, jsid id, uint32 flags, JSObject **objp)
{
    JS_ASSERT(obj->isRegExp());

    if (!JSID_IS_ATOM(id))
        return JS_TRUE;

    if (id == ATOM_TO_JSID(cx->runtime->atomState.lastIndexAtom)) {
        if (!js_DefineNativeProperty(cx, obj, id, UndefinedValue(),
                                     lastIndex_getter, lastIndex_setter,
                                     JSPROP_PERMANENT | JSPROP_SHARED,
                                     0, 0, NULL)) {
            return false;
        }
        *objp = obj;
        return true;
    }

    for (size_t i = 0; i < JS_ARRAY_LENGTH(lazyRegExpProps); i++) {
        const LazyProp &lazy = lazyRegExpProps[i];
        JSAtom *atom = OFFSET_TO_ATOM(cx->runtime, lazy.atomOffset);
        if (id == ATOM_TO_JSID(atom)) {
            if (!js_DefineNativeProperty(cx, obj, id, UndefinedValue(),
                                         lazy.getter, NULL,
                                         JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY,
                                         0, 0, NULL)) {
                return false;
            }
            *objp = obj;
            return true;
        }
    }

    return true;
}

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
    static JSBool                                                               \
    name(JSContext *cx, JSObject *obj, jsid id, jsval *vp)                      \
    {                                                                           \
        RegExpStatics &statics = cx->regExpStatics;                             \
        code;                                                                   \
    }

DEFINE_STATIC_GETTER(static_input_getter,        return statics.createInput(Valueify(vp)))
DEFINE_STATIC_GETTER(static_multiline_getter,    *vp = BOOLEAN_TO_JSVAL(statics.multiline());
                                                 return true)
DEFINE_STATIC_GETTER(static_lastMatch_getter,    return statics.createLastMatch(Valueify(vp)))
DEFINE_STATIC_GETTER(static_lastParen_getter,    return statics.createLastParen(Valueify(vp)))
DEFINE_STATIC_GETTER(static_leftContext_getter,  return statics.createLeftContext(Valueify(vp)))
DEFINE_STATIC_GETTER(static_rightContext_getter, return statics.createRightContext(Valueify(vp)))

DEFINE_STATIC_GETTER(static_paren1_getter,       return statics.createParen(0, Valueify(vp)))
DEFINE_STATIC_GETTER(static_paren2_getter,       return statics.createParen(1, Valueify(vp)))
DEFINE_STATIC_GETTER(static_paren3_getter,       return statics.createParen(2, Valueify(vp)))
DEFINE_STATIC_GETTER(static_paren4_getter,       return statics.createParen(3, Valueify(vp)))
DEFINE_STATIC_GETTER(static_paren5_getter,       return statics.createParen(4, Valueify(vp)))
DEFINE_STATIC_GETTER(static_paren6_getter,       return statics.createParen(5, Valueify(vp)))
DEFINE_STATIC_GETTER(static_paren7_getter,       return statics.createParen(6, Valueify(vp)))
DEFINE_STATIC_GETTER(static_paren8_getter,       return statics.createParen(7, Valueify(vp)))
DEFINE_STATIC_GETTER(static_paren9_getter,       return statics.createParen(8, Valueify(vp)))

#define DEFINE_STATIC_SETTER(name, code)                                        \
    static JSBool                                                               \
    name(JSContext *cx, JSObject *obj, jsid id, jsval *vp)                      \
    {                                                                           \
        RegExpStatics &statics = cx->regExpStatics;                             \
        code;                                                                   \
        return true;                                                            \
    }

DEFINE_STATIC_SETTER(static_input_setter,
                     if (!JSVAL_IS_STRING(*vp) && !JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp))
                         return false;
                     statics.setInput(JSVAL_TO_STRING(*vp)))
DEFINE_STATIC_SETTER(static_multiline_setter,
                     if (!JSVAL_IS_BOOLEAN(*vp) && !JS_ConvertValue(cx, *vp, JSTYPE_BOOLEAN, vp))
                         return false;
                     statics.setMultiline(!!JSVAL_TO_BOOLEAN(*vp)))

const uint8 REGEXP_STATIC_PROP_ATTRS    = JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_ENUMERATE;
const uint8 RO_REGEXP_STATIC_PROP_ATTRS = REGEXP_STATIC_PROP_ATTRS | JSPROP_READONLY;

static JSPropertySpec regexp_static_props[] = {
    {"input",        0, REGEXP_STATIC_PROP_ATTRS,    static_input_getter, static_input_setter},
    {"multiline",    0, REGEXP_STATIC_PROP_ATTRS,    static_multiline_getter,
                                                     static_multiline_setter},
    {"lastMatch",    0, RO_REGEXP_STATIC_PROP_ATTRS, static_lastMatch_getter,    NULL},
    {"lastParen",    0, RO_REGEXP_STATIC_PROP_ATTRS, static_lastParen_getter,    NULL},
    {"leftContext",  0, RO_REGEXP_STATIC_PROP_ATTRS, static_leftContext_getter,  NULL},
    {"rightContext", 0, RO_REGEXP_STATIC_PROP_ATTRS, static_rightContext_getter, NULL},
    {"$1",           0, RO_REGEXP_STATIC_PROP_ATTRS, static_paren1_getter,       NULL},
    {"$2",           0, RO_REGEXP_STATIC_PROP_ATTRS, static_paren2_getter,       NULL},
    {"$3",           0, RO_REGEXP_STATIC_PROP_ATTRS, static_paren3_getter,       NULL},
    {"$4",           0, RO_REGEXP_STATIC_PROP_ATTRS, static_paren4_getter,       NULL},
    {"$5",           0, RO_REGEXP_STATIC_PROP_ATTRS, static_paren5_getter,       NULL},
    {"$6",           0, RO_REGEXP_STATIC_PROP_ATTRS, static_paren6_getter,       NULL},
    {"$7",           0, RO_REGEXP_STATIC_PROP_ATTRS, static_paren7_getter,       NULL},
    {"$8",           0, RO_REGEXP_STATIC_PROP_ATTRS, static_paren8_getter,       NULL},
    {"$9",           0, RO_REGEXP_STATIC_PROP_ATTRS, static_paren9_getter,       NULL},
    {0,0,0,0,0}
};

static void
regexp_finalize(JSContext *cx, JSObject *obj)
{
    RegExp *re = RegExp::extractFrom(obj);
    if (!re)
        return;
    re->decref(cx);
}

/* Forward static prototype. */
static JSBool
regexp_exec_sub(JSContext *cx, JSObject *obj, uintN argc, Value *argv, JSBool test, Value *rval);

static JSBool
regexp_call(JSContext *cx, uintN argc, Value *vp)
{
    return regexp_exec_sub(cx, &JS_CALLEE(cx, vp).toObject(), argc, JS_ARGV(cx, vp), false, vp);
}

#if JS_HAS_XDR

#include "jsxdrapi.h"

JSBool
js_XDRRegExpObject(JSXDRState *xdr, JSObject **objp)
{
    JSString *source = 0;
    uint32 flagsword = 0;

    if (xdr->mode == JSXDR_ENCODE) {
        JS_ASSERT(objp);
        RegExp *re = RegExp::extractFrom(*objp);
        if (!re)
            return false;
        source = re->getSource();
        flagsword = re->getFlags();
    }
    if (!JS_XDRString(xdr, &source) || !JS_XDRUint32(xdr, &flagsword))
        return false;
    if (xdr->mode == JSXDR_DECODE) {
        JSObject *obj = NewBuiltinClassInstance(xdr->cx, &js_RegExpClass);
        if (!obj)
            return false;
        obj->clearParent();
        obj->clearProto();
        RegExp *re = RegExp::create(xdr->cx, source, flagsword);
        if (!re)
            return false;
        obj->setPrivate(re);
        obj->zeroRegExpLastIndex();
        *objp = obj;
    }
    return true;
}

#else  /* !JS_HAS_XDR */

#define js_XDRRegExpObject NULL

#endif /* !JS_HAS_XDR */

static void
regexp_trace(JSTracer *trc, JSObject *obj)
{
    RegExp *re = RegExp::extractFrom(obj);
    if (re && re->getSource())
        JS_CALL_STRING_TRACER(trc, re->getSource(), "source");
}

static JSBool
regexp_enumerate(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isRegExp());

    jsval v;
    if (!JS_LookupPropertyById(cx, obj, ATOM_TO_JSID(cx->runtime->atomState.lastIndexAtom), &v))
        return false;

    for (size_t i = 0; i < JS_ARRAY_LENGTH(lazyRegExpProps); i++) {
        const LazyProp &lazy = lazyRegExpProps[i];
        jsid id = ATOM_TO_JSID(OFFSET_TO_ATOM(cx->runtime, lazy.atomOffset));
        if (!JS_LookupPropertyById(cx, obj, id, &v))
            return false;
    }

    return true;
}

js::Class js_RegExpClass = {
    js_RegExp_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE |
    JSCLASS_HAS_RESERVED_SLOTS(JSObject::REGEXP_CLASS_RESERVED_SLOTS) |
    JSCLASS_MARK_IS_TRACE | JSCLASS_HAS_CACHED_PROTO(JSProto_RegExp),
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    regexp_enumerate,
    reinterpret_cast<JSResolveOp>(regexp_resolve),
    ConvertStub,
    regexp_finalize,
    NULL,           /* reserved0 */
    NULL,           /* checkAccess */
    regexp_call,
    NULL,           /* construct */
    js_XDRRegExpObject,
    NULL,           /* hasInstance */
    JS_CLASS_TRACE(regexp_trace)
};

/*
 * RegExp instance methods.
 */

JSBool
js_regexp_toString(JSContext *cx, JSObject *obj, Value *vp)
{
    static const jschar empty_regexp_ucstr[] = {'(', '?', ':', ')', 0};
    if (!InstanceOf(cx, obj, &js_RegExpClass, vp + 2))
        return false;
    JS_LOCK_OBJ(cx, obj);
    RegExp *re = RegExp::extractFrom(obj);
    if (!re) {
        JS_UNLOCK_OBJ(cx, obj);
        *vp = StringValue(cx->runtime->emptyString);
        return true;
    }

    const jschar *source;
    size_t length;
    re->getSource()->getCharsAndLength(source, length);
    if (length == 0) {
        source = empty_regexp_ucstr;
        length = JS_ARRAY_LENGTH(empty_regexp_ucstr) - 1;
    }
    length += 2;
    uint32 nflags = re->flagCount();
    jschar *chars = (jschar*) cx->malloc((length + nflags + 1) * sizeof(jschar));
    if (!chars) {
        JS_UNLOCK_OBJ(cx, obj);
        return false;
    }

    chars[0] = '/';
    js_strncpy(&chars[1], source, length - 2);
    chars[length - 1] = '/';
    if (nflags) {
        if (re->global())
            chars[length++] = 'g';
        if (re->ignoreCase())
            chars[length++] = 'i';
        if (re->multiline())
            chars[length++] = 'm';
        if (re->sticky())
            chars[length++] = 'y';
    }
    JS_UNLOCK_OBJ(cx, obj);
    chars[length] = 0;

    JSString *str = js_NewString(cx, chars, length);
    if (!str) {
        cx->free(chars);
        return false;
    }
    *vp = StringValue(str);
    return true;
}

static JSBool
regexp_toString(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, Jsvalify(vp));
    return obj && js_regexp_toString(cx, obj, vp);
}

/*
 * Return:
 * - The original if no escaping need be performed.
 * - A new string if escaping need be performed.
 * - NULL on error.
 */
static JSString *
EscapeNakedForwardSlashes(JSContext *cx, JSString *unescaped)
{
    const jschar *oldChars;
    size_t oldLen;
    unescaped->getCharsAndLength(oldChars, oldLen);
    js::Vector<jschar, 128> newChars(cx);
    for (const jschar *it = oldChars; it < oldChars + oldLen; ++it) {
        if (*it == '/' && (it == oldChars || it[-1] != '\\')) {
            if (!newChars.length()) {
                if (!newChars.reserve(oldLen + 1))
                    return NULL;
                newChars.append(oldChars, size_t(it - oldChars));
            }
            newChars.append('\\');
        }

        if (newChars.length())
            newChars.append(*it);
    }

    if (newChars.length()) {
        size_t len = newChars.length();
        jschar *chars = newChars.extractRawBuffer();
        if (!chars)
            return NULL;
        JSString *escaped = js_NewString(cx, chars, len);
        if (!escaped)
            cx->free(chars);
        return escaped;
    }
    return unescaped;
}

static inline JSBool
regexp_compile_sub_tail(JSContext *cx, JSObject *obj, Value *rval, JSString *str, uint32 flags = 0)
{
    flags |= cx->regExpStatics.getFlags();
    RegExp *re = RegExp::create(cx, str, flags);
    if (!re)
        return false;
    SwapObjectRegExp(cx, obj, *re);
    *rval = ObjectValue(*obj);
    return true;
}

static JSBool
regexp_compile_sub(JSContext *cx, JSObject *obj, uintN argc, Value *argv, Value *rval)
{
    if (!InstanceOf(cx, obj, &js_RegExpClass, argv))
        return false;
    if (argc == 0)
        return regexp_compile_sub_tail(cx, obj, rval, cx->runtime->emptyString);

    Value sourceValue = argv[0];
    if (sourceValue.isObject() && sourceValue.toObject().getClass() == &js_RegExpClass) {
        /*
         * If we get passed in a RegExp object we construct a new
         * RegExp that is a duplicate of it by re-compiling the
         * original source code. ECMA requires that it be an error
         * here if the flags are specified. (We must use the flags
         * from the original RegExp also).
         */
        JSObject &sourceObj = sourceValue.toObject();
        if (argc >= 2 && !argv[1].isUndefined()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NEWREGEXP_FLAGGED);
            return false;
        }
        RegExp *clone;
        {
            AutoObjectLocker lock(cx, &sourceObj);
            RegExp *re = RegExp::extractFrom(&sourceObj);
            if (!re)
                return false;
            clone = RegExp::clone(cx, *re);
        }
        if (!clone)
            return false;
        SwapObjectRegExp(cx, obj, *clone);
        *rval = ObjectValue(*obj);
        return true;
    }

    /* Coerce to string and compile. */
    JSString *sourceStr = js_ValueToString(cx, sourceValue);
    if (!sourceStr)
        return false;
    argv[0] = StringValue(sourceStr);
    uint32 flags = 0;
    if (argc > 1 && !argv[1].isUndefined()) {
        JSString *flagStr = js_ValueToString(cx, argv[1]);
        if (!flagStr)
            return false;
        argv[1] = StringValue(flagStr);
        if (!RegExp::parseFlags(cx, flagStr, flags))
            return false;
    }

    JSString *escapedSourceStr = EscapeNakedForwardSlashes(cx, sourceStr);
    if (!escapedSourceStr)
        return false;
    argv[0] = StringValue(escapedSourceStr);
    return regexp_compile_sub_tail(cx, obj, rval, escapedSourceStr, flags);
}

static JSBool
regexp_compile(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, Jsvalify(vp));
    return obj && regexp_compile_sub(cx, obj, argc, vp + 2, vp);
}

static JSBool
regexp_exec_sub(JSContext *cx, JSObject *obj, uintN argc, Value *argv, JSBool test, Value *rval)
{
    bool ok = InstanceOf(cx, obj, &js_RegExpClass, argv);
    if (!ok)
        return JS_FALSE;
    JS_LOCK_OBJ(cx, obj);
    RegExp *re = RegExp::extractFrom(obj);
    if (!re) {
        JS_UNLOCK_OBJ(cx, obj);
        return JS_TRUE;
    }

    /* NB: we must reach out: after this paragraph, in order to drop re. */
    re->incref(cx);
    jsdouble lastIndex;
    if (re->global() || re->sticky()) {
        const Value v = obj->getRegExpLastIndex();
        if (v.isInt32()) {
            lastIndex = v.toInt32();
        } else {
            if (v.isDouble())
                lastIndex = v.toDouble();
            else if (!ValueToNumber(cx, v, &lastIndex))
                return JS_FALSE;
            lastIndex = js_DoubleToInteger(lastIndex);
        }
    } else {
        lastIndex = 0;
    }
    JS_UNLOCK_OBJ(cx, obj);

    /* Now that obj is unlocked, it's safe to (potentially) grab the GC lock. */
    JSString *str;
    if (argc) {
        str = js_ValueToString(cx, argv[0]);
        if (!str) {
            ok = JS_FALSE;
            goto out;
        }
        argv[0] = StringValue(str);
    } else {
        /* Need to grab input from statics. */
        str = cx->regExpStatics.getInput();
        if (!str) {
            const char *sourceBytes = js_GetStringBytes(cx, re->getSource());
            if (sourceBytes) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_INPUT, sourceBytes,
                                     re->global() ? "g" : "",
                                     re->ignoreCase() ? "i" : "",
                                     re->multiline() ? "m" : "",
                                     re->sticky() ? "y" : "");
            }
            ok = false;
            goto out;
        }
    }

    if (lastIndex < 0 || str->length() < lastIndex) {
        obj->zeroRegExpLastIndex();
        *rval = NullValue();
    } else {
        size_t lastIndexInt = (size_t) lastIndex;
        ok = re->execute(cx, str, &lastIndexInt, !!test, rval);
        if (ok && (re->global() || (!rval->isNull() && re->sticky()))) {
            if (rval->isNull())
                obj->zeroRegExpLastIndex();
            else
                obj->setRegExpLastIndex(lastIndexInt);
        }
    }

  out:
    re->decref(cx);
    return ok;
}

JSBool
js_regexp_exec(JSContext *cx, uintN argc, Value *vp)
{
    return regexp_exec_sub(cx, JS_THIS_OBJECT(cx, Jsvalify(vp)), argc, vp + 2, JS_FALSE, vp);
}

JSBool
js_regexp_test(JSContext *cx, uintN argc, Value *vp)
{
    if (!regexp_exec_sub(cx, JS_THIS_OBJECT(cx, Jsvalify(vp)), argc, vp + 2, JS_TRUE, vp))
        return false;
    if (!vp->isTrue())
        vp->setBoolean(false);
    return true;
}

static JSFunctionSpec regexp_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,  regexp_toString,    0,0),
#endif
    JS_FN(js_toString_str,  regexp_toString,    0,0),
    JS_FN("compile",        regexp_compile,     2,0),
    JS_FN("exec",           js_regexp_exec,     1,0),
    JS_FN("test",           js_regexp_test,     1,0),
    JS_FS_END
};

static JSBool
regexp_construct(JSContext *cx, uintN argc, Value *vp)
{
    Value *argv = JS_ARGV(cx, vp);
    if (!IsConstructing(vp)) {
        /*
         * If first arg is regexp and no flags are given, just return the arg.
         * (regexp_compile_sub detects the regexp + flags case and throws a
         * TypeError.)  See 15.10.3.1.
         */
        if (argc >= 1 && argv[0].isObject() && argv[0].toObject().isRegExp() &&
            (argc == 1 || argv[1].isUndefined()))
        {
            *vp = argv[0];
            return true;
        }
    }

    /* Otherwise, replace obj with a new RegExp object. */
    JSObject *obj = NewBuiltinClassInstance(cx, &js_RegExpClass);
    if (!obj)
        return false;

    return regexp_compile_sub(cx, obj, argc, argv, vp);
}

JSObject *
js_InitRegExpClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto = js_InitClass(cx, obj, NULL, &js_RegExpClass, regexp_construct, 1,
                                   NULL, regexp_methods, regexp_static_props, NULL);
    if (!proto)
        return NULL;

    JSObject *ctor = JS_GetConstructor(cx, proto);
    if (!ctor)
        return NULL;

    /* Give RegExp.prototype private data so it matches the empty string. */
    Value rval;
    if (!JS_AliasProperty(cx, ctor, "input",        "$_") ||
        !JS_AliasProperty(cx, ctor, "multiline",    "$*") ||
        !JS_AliasProperty(cx, ctor, "lastMatch",    "$&") ||
        !JS_AliasProperty(cx, ctor, "lastParen",    "$+") ||
        !JS_AliasProperty(cx, ctor, "leftContext",  "$`") ||
        !JS_AliasProperty(cx, ctor, "rightContext", "$'") ||
        !regexp_compile_sub(cx, proto, 0, NULL, &rval)) {
        return NULL;
    }

    return proto;
}
