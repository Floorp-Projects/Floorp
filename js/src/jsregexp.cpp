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
#include "jsobj.h"
#include "jsregexp.h"
#include "jsstr.h"
#include "jsvector.h"
#include "jsnum.h"

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
    JSContext   * const cx;
    JSObject    * const array;

  public:
    RegExpMatchBuilder(JSContext *cx, JSObject *array) : cx(cx), array(array) {}

    bool append(int index, JSString *str) {
        JS_ASSERT(str);
        return append(INT_TO_JSID(index), STRING_TO_JSVAL(str));
    }

    bool append(jsid id, jsval val) {
        return js_DefineProperty(cx, array, id, val, JS_PropertyStub, JS_PropertyStub,
                                 JSPROP_ENUMERATE);
    }

    bool appendIndex(int index) {
        return append(ATOM_TO_JSID(cx->runtime->atomState.indexAtom), INT_TO_JSVAL(index));
    }

    /* Sets the input attribute of the match array. */
    bool appendInput(JSString *str) {
        JS_ASSERT(str);
        return append(ATOM_TO_JSID(cx->runtime->atomState.inputAtom), STRING_TO_JSVAL(str));
    }
};

static bool
SetRegExpLastIndex(JSContext *cx, JSObject *obj, jsdouble lastIndex)
{
    JS_ASSERT(obj->isRegExp());
    return js_NewNumberInRootedValue(cx, lastIndex, obj->addressOfRegExpLastIndex());
}

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
js_SaveAndClearRegExpStatics(JSContext *cx, js::RegExpStatics *container, js::AutoValueRooter *tvr)
{
    JS_ASSERT(container);
    if (cx->regExpStatics.getInput())
        tvr->setString(cx->regExpStatics.getInput());
    cx->regExpStatics.saveAndClear(*container);
}

void
js_RestoreRegExpStatics(JSContext *cx, js::RegExpStatics *statics, js::AutoValueRooter *tvr)
{
    JS_ASSERT(statics);
    cx->regExpStatics.restore(*statics);
}

JSObject * JS_FASTCALL
js_CloneRegExpObject(JSContext *cx, JSObject *obj, JSObject *proto)
{
    JS_ASSERT(obj->getClass() == &js_RegExpClass);
    JS_ASSERT(proto);
    JS_ASSERT(proto->getClass() == &js_RegExpClass);
    JSObject *clone = NewObjectWithGivenProto(cx, &js_RegExpClass, proto, NULL);
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
                     ACC_STORE_ANY)
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
RegExp::execute(JSContext *cx, JSString *input, size_t *lastIndex, bool test, jsval *rval)
{
    const size_t pairCount = parenCount + 1;
    const size_t bufCount = pairCount * 3; /* Should be x2, but PCRE has... needs. */
    const size_t matchItemCount = pairCount * 2;
    int *buf = static_cast<int *>(cx->malloc(bufCount * sizeof(*buf)));
    if (!buf)
        return false;
    AutoReleasePtr bufWrapper(cx, buf);
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
    bool found = JSC::Yarr::executeRegex(cx, compiled, chars, *lastIndex - inputOffset, len,
                                         buf, bufCount) != -1;
    if (!found) {
        /*
         * FIXME: error conditions will be possible from executeRegex when
         * OOM checks are added in bug 574459.
         */
        *rval = JSVAL_NULL;
        return true;
    }
    RegExpStatics::MatchPairs *matchPairsPtr = cx->regExpStatics.getOrCreateMatchPairs(input);
    if (!matchPairsPtr)
        return false;
    RegExpStatics::MatchPairs &matchPairs = *matchPairsPtr;
    if (!matchPairs.reserve(matchItemCount))
        return false;
    for (size_t idx = 0; idx < matchItemCount; idx += 2) {
        JS_ASSERT(buf[idx + 1] >= buf[idx]);
        JS_ALWAYS_TRUE(matchPairs.append(buf[idx] + inputOffset));
        JS_ALWAYS_TRUE(matchPairs.append(buf[idx + 1] + inputOffset));
    }
    JS_ASSERT(cx->regExpStatics.pairCount() == pairCount);
    *lastIndex = matchPairs[1];
    if (test) {
        *rval = JSVAL_TRUE;
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
    *rval = OBJECT_TO_JSVAL(array);
    RegExpMatchBuilder builder(cx, array);
    for (size_t idx = 0; idx < matchItemCount; idx += 2) {
        int start = matchPairs[idx];
        int end = matchPairs[idx + 1];
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
            if (!builder.append(INT_TO_JSVAL(idx / 2), JSVAL_VOID))
                return false;
        }
    }
    return builder.appendIndex(matchPairs[0]) && builder.appendInput(input);
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
    name(JSContext *cx, JSObject *obj, jsval id, jsval *vp)                    \
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
DEFINE_GETTER(source_getter,     *vp = STRING_TO_JSVAL(re->getSource()))
DEFINE_GETTER(global_getter,     *vp = BOOLEAN_TO_JSVAL(re->global()))
DEFINE_GETTER(ignoreCase_getter, *vp = BOOLEAN_TO_JSVAL(re->ignoreCase()))
DEFINE_GETTER(multiline_getter,  *vp = BOOLEAN_TO_JSVAL(re->multiline()))
DEFINE_GETTER(sticky_getter,     *vp = BOOLEAN_TO_JSVAL(re->sticky()))

static JSBool
lastIndex_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    while (obj->getClass() != &js_RegExpClass) {
        obj = obj->getProto();
        if (!obj)
            return true;
    }
    jsdouble lastIndex;
    if (!JS_ValueToNumber(cx, *vp, &lastIndex))
        return false;
    lastIndex = js_DoubleToInteger(lastIndex);
    return SetRegExpLastIndex(cx, obj, lastIndex);
}

static const struct LazyProp {
    const char *name;
    uint16 atomOffset;
    JSPropertyOp getter;
} lazyRegExpProps[] = {
    { js_source_str,     ATOM_OFFSET(source),     source_getter },
    { js_global_str,     ATOM_OFFSET(global),     global_getter },
    { js_ignoreCase_str, ATOM_OFFSET(ignoreCase), ignoreCase_getter },
    { js_multiline_str,  ATOM_OFFSET(multiline),  multiline_getter },
    { js_sticky_str,     ATOM_OFFSET(sticky),     sticky_getter }
};

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

static JSBool
regexp_resolve(JSContext *cx, JSObject *obj, jsid id, uint32 flags, JSObject **objp)
{
    JS_ASSERT(obj->isRegExp());

    if (!JSID_IS_ATOM(id))
        return JS_TRUE;

    if (id == ATOM_TO_JSID(cx->runtime->atomState.lastIndexAtom)) {
        if (!js_DefineNativeProperty(cx, obj, id, JSVAL_VOID,
                                     lastIndex_getter, lastIndex_setter,
                                     JSPROP_PERMANENT | JSPROP_SHARED, 0, 0, NULL)) {
            return JS_FALSE;
        }
        *objp = obj;
        return JS_TRUE;
    }

    for (size_t i = 0; i < JS_ARRAY_LENGTH(lazyRegExpProps); i++) {
        const LazyProp &lazy = lazyRegExpProps[i];
        JSAtom *atom = OFFSET_TO_ATOM(cx->runtime, lazy.atomOffset);
        if (id == ATOM_TO_JSID(atom)) {
            if (!js_DefineNativeProperty(cx, obj, id, JSVAL_VOID,
                                         lazy.getter, NULL,
                                         JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY,
                                         0, 0, NULL)) {
                return JS_FALSE;
            }
            *objp = obj;
            return JS_TRUE;
        }
    }

    return JS_TRUE;
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
    name(JSContext *cx, JSObject *obj, jsval id, jsval *vp)                     \
    {                                                                           \
        RegExpStatics &statics = cx->regExpStatics;                             \
        code;                                                                   \
    }

DEFINE_STATIC_GETTER(static_input_getter,           return statics.createInput(*vp))
DEFINE_STATIC_GETTER(static_multiline_getter,       *vp = BOOLEAN_TO_JSVAL(statics.multiline());
                                                    return true)
DEFINE_STATIC_GETTER(static_lastMatch_getter,       return statics.createLastMatch(*vp))
DEFINE_STATIC_GETTER(static_lastParen_getter,       return statics.createLastParen(*vp))
DEFINE_STATIC_GETTER(static_leftContext_getter,     return statics.createLeftContext(*vp))
DEFINE_STATIC_GETTER(static_rightContext_getter,    return statics.createRightContext(*vp))
 
DEFINE_STATIC_GETTER(static_paren1_getter,          return statics.createParen(0, *vp))
DEFINE_STATIC_GETTER(static_paren2_getter,          return statics.createParen(1, *vp))
DEFINE_STATIC_GETTER(static_paren3_getter,          return statics.createParen(2, *vp))
DEFINE_STATIC_GETTER(static_paren4_getter,          return statics.createParen(3, *vp))
DEFINE_STATIC_GETTER(static_paren5_getter,          return statics.createParen(4, *vp))
DEFINE_STATIC_GETTER(static_paren6_getter,          return statics.createParen(5, *vp))
DEFINE_STATIC_GETTER(static_paren7_getter,          return statics.createParen(6, *vp))
DEFINE_STATIC_GETTER(static_paren8_getter,          return statics.createParen(7, *vp))
DEFINE_STATIC_GETTER(static_paren9_getter,          return statics.createParen(8, *vp))

#define DEFINE_STATIC_SETTER(name, code)                                        \
    static JSBool                                                               \
    name(JSContext *cx, JSObject *obj, jsval id, jsval *vp)                     \
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
regexp_exec_sub(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                JSBool test, jsval *rval);

static JSBool
regexp_call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    return regexp_exec_sub(cx, JSVAL_TO_OBJECT(argv[-2]), argc, argv, JS_FALSE, rval);
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
        JSObject *obj = NewObject(xdr->cx, &js_RegExpClass, NULL, NULL);
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

JSClass js_RegExpClass = {
    js_RegExp_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE |
    JSCLASS_HAS_RESERVED_SLOTS(JSObject::REGEXP_FIXED_RESERVED_SLOTS) |
    JSCLASS_MARK_IS_TRACE | JSCLASS_HAS_CACHED_PROTO(JSProto_RegExp),
    JS_PropertyStub,    JS_PropertyStub,
    JS_PropertyStub,    JS_PropertyStub,
    regexp_enumerate,   reinterpret_cast<JSResolveOp>(regexp_resolve),
    JS_ConvertStub,     regexp_finalize,
    NULL,               NULL,
    regexp_call,        NULL,
    js_XDRRegExpObject, NULL,
    JS_CLASS_TRACE(regexp_trace), 0
};

/*
 * RegExp instance methods.
 */

JSBool
js_regexp_toString(JSContext *cx, JSObject *obj, jsval *vp)
{
    static const jschar empty_regexp_ucstr[] = {'(', '?', ':', ')', 0};
    if (!JS_InstanceOf(cx, obj, &js_RegExpClass, vp + 2))
        return JS_FALSE;
    JS_LOCK_OBJ(cx, obj);
    RegExp *re = RegExp::extractFrom(obj);
    if (!re) {
        JS_UNLOCK_OBJ(cx, obj);
        *vp = STRING_TO_JSVAL(cx->runtime->emptyString);
        return JS_TRUE;
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
        return JS_FALSE;
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
        return JS_FALSE;
    }
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
regexp_toString(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
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
regexp_compile_sub_tail(JSContext *cx, JSObject *obj, jsval *rval, JSString *str, uint32 flags = 0)
{
    flags |= cx->regExpStatics.getFlags();
    RegExp *re = RegExp::create(cx, str, flags);
    if (!re)
        return JS_FALSE;
    SwapObjectRegExp(cx, obj, *re);
    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

static JSBool
regexp_compile_sub(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (!JS_InstanceOf(cx, obj, &js_RegExpClass, argv))
        return false;
    if (argc == 0)
        return regexp_compile_sub_tail(cx, obj, rval, cx->runtime->emptyString);

    jsval sourceValue = argv[0];
    if (JSVAL_IS_OBJECT(sourceValue) && !JSVAL_IS_NULL(sourceValue) &&
        JSVAL_TO_OBJECT(sourceValue)->getClass() == &js_RegExpClass) {
        /*
         * If we get passed in a RegExp object we construct a new
         * RegExp that is a duplicate of it by re-compiling the
         * original source code. ECMA requires that it be an error
         * here if the flags are specified. (We must use the flags
         * from the original RegExp also).
         */
        JSObject *sourceObj = JSVAL_TO_OBJECT(sourceValue);
        if (argc >= 2 && !JSVAL_IS_VOID(argv[1])) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NEWREGEXP_FLAGGED);
            return false;
        }
        RegExp *clone;
        {
            AutoObjectLocker lock(cx, sourceObj);
            RegExp *re = RegExp::extractFrom(sourceObj);
            if (!re)
                return false;
            clone = RegExp::clone(cx, *re);
        }
        if (!clone)
            return false;
        SwapObjectRegExp(cx, obj, *clone);
        *rval = OBJECT_TO_JSVAL(obj);
        return true;
    }

    /* Coerce to string and compile. */
    JSString *sourceStr = js_ValueToString(cx, sourceValue);
    if (!sourceStr)
        return false;
    argv[0] = STRING_TO_JSVAL(sourceStr);
    uint32 flags = 0;
    if (argc > 1 && !JSVAL_IS_VOID(argv[1])) {
        JSString *flagStr = js_ValueToString(cx, argv[1]);
        if (!flagStr)
            return false;
        argv[1] = STRING_TO_JSVAL(flagStr);
        if (!RegExp::parseFlags(cx, flagStr, flags))
            return false;
    }

    JSString *escapedSourceStr = EscapeNakedForwardSlashes(cx, sourceStr);
    if (!escapedSourceStr)
        return false;
    argv[0] = STRING_TO_JSVAL(escapedSourceStr);
    return regexp_compile_sub_tail(cx, obj, rval, escapedSourceStr, flags);
}

static JSBool
regexp_compile(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    return obj && regexp_compile_sub(cx, obj, argc, vp + 2, vp);
}

static JSBool
regexp_exec_sub(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, JSBool test, jsval *rval)
{
    bool ok = JS_InstanceOf(cx, obj, &js_RegExpClass, argv);
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
        jsval v = obj->getRegExpLastIndex();
        if (JSVAL_IS_INT(v)) {
            lastIndex = JSVAL_TO_INT(v);
        } else {
            JS_ASSERT(JSVAL_IS_DOUBLE(v));
            lastIndex = *JSVAL_TO_DOUBLE(v);
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
        argv[0] = STRING_TO_JSVAL(str);
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
        *rval = JSVAL_NULL;
    } else {
        size_t lastIndexInt = (size_t) lastIndex;
        ok = re->execute(cx, str, &lastIndexInt, test, rval);
        if (ok && (re->global() || (*rval != JSVAL_NULL && re->sticky()))) {
            if (*rval == JSVAL_NULL)
                obj->zeroRegExpLastIndex();
            else
                ok = SetRegExpLastIndex(cx, obj, lastIndexInt);
        }
    }

  out:
    re->decref(cx);
    return ok;
}

static JSBool
regexp_exec(JSContext *cx, uintN argc, jsval *vp)
{
    return regexp_exec_sub(cx, JS_THIS_OBJECT(cx, vp), argc, vp + 2, JS_FALSE, vp);
}

static JSBool
regexp_test(JSContext *cx, uintN argc, jsval *vp)
{
    if (!regexp_exec_sub(cx, JS_THIS_OBJECT(cx, vp), argc, vp + 2, JS_TRUE, vp))
        return JS_FALSE;
    if (*vp != JSVAL_TRUE)
        *vp = JSVAL_FALSE;
    return JS_TRUE;
}

static JSFunctionSpec regexp_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,  regexp_toString,    0,0),
#endif
    JS_FN(js_toString_str,  regexp_toString,    0,0),
    JS_FN("compile",        regexp_compile,     2,0),
    JS_FN("exec",           regexp_exec,        1,0),
    JS_FN("test",           regexp_test,        1,0),
    JS_FS_END
};

static JSBool
regexp_construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (!JS_IsConstructing(cx)) {
        /*
         * If first arg is regexp and no flags are given, just return the arg.
         * (regexp_compile_sub detects the regexp + flags case and throws a
         * TypeError.)  See 10.15.3.1.
         */
        if ((argc < 2 || JSVAL_IS_VOID(argv[1])) &&
            !JSVAL_IS_PRIMITIVE(argv[0]) &&
            JSVAL_TO_OBJECT(argv[0])->getClass() == &js_RegExpClass) {
            *rval = argv[0];
            return JS_TRUE;
        }

        /* Otherwise, replace obj with a new RegExp object. */
        obj = NewBuiltinClassInstance(cx, &js_RegExpClass);
        if (!obj)
            return JS_FALSE;

        /*
         * regexp_compile_sub does not use rval to root its temporaries so we
         * can use it to root obj.
         */
        *rval = OBJECT_TO_JSVAL(obj);
    }
    return regexp_compile_sub(cx, obj, argc, argv, rval);
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
    jsval rval;
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
