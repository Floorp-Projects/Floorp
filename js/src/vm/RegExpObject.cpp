/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Leary <cdleary@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "jsscan.h"

#include "vm/RegExpStatics.h"

#include "jsobjinlines.h"
#include "jsstrinlines.h"

#include "vm/RegExpObject-inl.h"

#ifdef JS_TRACER
#include "jstracer.h"
using namespace nanojit;
#endif

using namespace js;

JS_STATIC_ASSERT(IgnoreCaseFlag == JSREG_FOLD);
JS_STATIC_ASSERT(GlobalFlag == JSREG_GLOB);
JS_STATIC_ASSERT(MultilineFlag == JSREG_MULTILINE);
JS_STATIC_ASSERT(StickyFlag == JSREG_STICKY);

bool
RegExpPrivate::executeInternal(JSContext *cx, RegExpStatics *res, JSString *inputstr,
                               size_t *lastIndex, bool test, Value *rval)
{
    const size_t pairCount = parenCount + 1;
    const size_t bufCount = pairCount * 3; /* Should be x2, but PCRE has... needs. */
    const size_t matchItemCount = pairCount * 2;

    LifoAllocScope las(&cx->tempLifoAlloc());
    int *buf = cx->tempLifoAlloc().newArray<int>(bufCount);
    if (!buf)
        return false;

    /*
     * The JIT regexp procedure doesn't always initialize matchPair values.
     * Maybe we can make this faster by ensuring it does?
     */
    for (int *it = buf; it != buf + matchItemCount; ++it)
        *it = -1;
    
    JSLinearString *input = inputstr->ensureLinear(cx);
    if (!input)
        return false;

    JS::Anchor<JSString *> anchor(input);
    size_t len = input->length();
    const jschar *chars = input->chars();

    /* 
     * inputOffset emulates sticky mode by matching from this offset into the char buf and
     * subtracting the delta off at the end.
     */
    size_t inputOffset = 0;

    if (sticky()) {
        /* Sticky matches at the last index for the regexp object. */
        chars += *lastIndex;
        len -= *lastIndex;
        inputOffset = *lastIndex;
    }

    int result;
#if ENABLE_YARR_JIT
    if (!codeBlock.isFallBack())
        result = JSC::Yarr::execute(codeBlock, chars, *lastIndex - inputOffset, len, buf);
    else
        result = JSC::Yarr::interpret(byteCode, chars, *lastIndex - inputOffset, len, buf);
#else
    result = jsRegExpExecute(cx, compiled, chars, len, *lastIndex - inputOffset, buf, bufCount);
#endif
    if (result == -1) {
        *rval = NullValue();
        return true;
    }

#if !ENABLE_YARR_JIT
    if (result < 0) {
        reportPCREError(cx, result);
        return false;
    }
#endif

    /* 
     * Adjust buf for the inputOffset. Use of sticky is rare and the matchItemCount is small, so
     * just do another pass.
     */
    if (JS_UNLIKELY(inputOffset)) {
        for (size_t i = 0; i < matchItemCount; ++i)
            buf[i] = buf[i] < 0 ? -1 : buf[i] + inputOffset;
    }

    /* Make sure the populated contents of |buf| are sane values against |input|. */
    checkMatchPairs(input, buf, matchItemCount);

    if (res)
        res->updateFromMatch(cx, input, buf, matchItemCount);

    *lastIndex = buf[1];

    if (test) {
        *rval = BooleanValue(true);
        return true;
    }

    JSObject *array = createResult(cx, input, buf, matchItemCount);
    if (!array)
        return false;

    *rval = ObjectValue(*array);
    return true;
}

const Shape *
RegExpObject::assignInitialShape(JSContext *cx)
{
    JS_ASSERT(!cx->compartment->initialRegExpShape);
    JS_ASSERT(isRegExp());
    JS_ASSERT(nativeEmpty());

    JS_STATIC_ASSERT(LAST_INDEX_SLOT == 0);
    JS_STATIC_ASSERT(SOURCE_SLOT == LAST_INDEX_SLOT + 1);
    JS_STATIC_ASSERT(GLOBAL_FLAG_SLOT == SOURCE_SLOT + 1);
    JS_STATIC_ASSERT(IGNORE_CASE_FLAG_SLOT == GLOBAL_FLAG_SLOT + 1);
    JS_STATIC_ASSERT(MULTILINE_FLAG_SLOT == IGNORE_CASE_FLAG_SLOT + 1);
    JS_STATIC_ASSERT(STICKY_FLAG_SLOT == MULTILINE_FLAG_SLOT + 1);

    /* The lastIndex property alone is writable but non-configurable. */
    if (!addDataProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.lastIndexAtom),
                         LAST_INDEX_SLOT, JSPROP_PERMANENT))
    {
        return NULL;
    }

    /* Remaining instance properties are non-writable and non-configurable. */
    if (!addDataProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.sourceAtom),
                         SOURCE_SLOT, JSPROP_PERMANENT | JSPROP_READONLY) ||
        !addDataProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.globalAtom),
                         GLOBAL_FLAG_SLOT, JSPROP_PERMANENT | JSPROP_READONLY) ||
        !addDataProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.ignoreCaseAtom),
                         IGNORE_CASE_FLAG_SLOT, JSPROP_PERMANENT | JSPROP_READONLY) ||
        !addDataProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.multilineAtom),
                         MULTILINE_FLAG_SLOT, JSPROP_PERMANENT | JSPROP_READONLY))
    {
        return NULL;
    }

    return addDataProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.stickyAtom),
                           STICKY_FLAG_SLOT, JSPROP_PERMANENT | JSPROP_READONLY);
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
        RegExpObject *reobj = (*objp)->asRegExp();
        source = reobj->getSource();
        flagsword = reobj->getFlags();
    }
    if (!JS_XDRString(xdr, &source) || !JS_XDRUint32(xdr, &flagsword))
        return false;
    if (xdr->mode == JSXDR_DECODE) {
        JS::Anchor<JSString *> anchor(source);
        const jschar *chars = source->getChars(xdr->cx);
        if (!chars)
            return false;
        size_t len = source->length();
        RegExpObject *reobj = RegExpObject::createNoStatics(xdr->cx, chars, len,
                                                            RegExpFlag(flagsword), NULL);
        if (!reobj)
            return false;

        reobj->clearParent();
        reobj->clearType();
        *objp = reobj;
    }
    return true;
}

#else  /* !JS_HAS_XDR */

#define js_XDRRegExpObject NULL

#endif /* !JS_HAS_XDR */

static void
regexp_finalize(JSContext *cx, JSObject *obj)
{
    if (RegExpPrivate *rep = static_cast<RegExpObject *>(obj)->getPrivate())
        rep->decref(cx);
}

Class js::RegExpClass = {
    js_RegExp_str,
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_RESERVED_SLOTS(RegExpObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_RegExp),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,        /* enumerate */
    JS_ResolveStub,
    JS_ConvertStub,
    regexp_finalize,
    NULL,                    /* reserved0 */
    NULL,                    /* checkAccess */
    NULL,                    /* call */
    NULL,                    /* construct */
    js_XDRRegExpObject,
    NULL,                    /* hasInstance */
    NULL                     /* trace */
};

#if !ENABLE_YARR_JIT
void
RegExpPrivate::reportPCREError(JSContext *cx, int error)
{
#define REPORT(msg_) \
    JS_ReportErrorFlagsAndNumberUC(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL, msg_); \
    return
    switch (error) {
      case -2: REPORT(JSMSG_REGEXP_TOO_COMPLEX);
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
#endif

void
RegExpPrivate::reportYarrError(JSContext *cx, TokenStream *ts, JSC::Yarr::ErrorCode error)
{
    switch (error) {
      case JSC::Yarr::NoError:
        JS_NOT_REACHED("Called reportYarrError with value for no error");
        return;
#define COMPILE_EMSG(__code, __msg)                                                              \
      case JSC::Yarr::__code:                                                                    \
        if (ts)                                                                                  \
            ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR, __msg);                       \
        else                                                                                     \
            JS_ReportErrorFlagsAndNumberUC(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL, __msg); \
        return
      COMPILE_EMSG(PatternTooLarge, JSMSG_REGEXP_TOO_COMPLEX);
      COMPILE_EMSG(QuantifierOutOfOrder, JSMSG_BAD_QUANTIFIER);
      COMPILE_EMSG(QuantifierWithoutAtom, JSMSG_BAD_QUANTIFIER);
      COMPILE_EMSG(MissingParentheses, JSMSG_MISSING_PAREN);
      COMPILE_EMSG(ParenthesesUnmatched, JSMSG_UNMATCHED_RIGHT_PAREN);
      COMPILE_EMSG(ParenthesesTypeInvalid, JSMSG_BAD_QUANTIFIER); /* "(?" with bad next char */
      COMPILE_EMSG(CharacterClassUnmatched, JSMSG_BAD_CLASS_RANGE);
      COMPILE_EMSG(CharacterClassInvalidRange, JSMSG_BAD_CLASS_RANGE);
      COMPILE_EMSG(CharacterClassOutOfOrder, JSMSG_BAD_CLASS_RANGE);
      COMPILE_EMSG(QuantifierTooLarge, JSMSG_BAD_QUANTIFIER);
      COMPILE_EMSG(EscapeUnterminated, JSMSG_TRAILING_SLASH);
#undef COMPILE_EMSG
      default:
        JS_NOT_REACHED("Unknown Yarr error code");
    }
}

bool
js::ParseRegExpFlags(JSContext *cx, JSString *flagStr, RegExpFlag *flagsOut)
{
    size_t n = flagStr->length();
    const jschar *s = flagStr->getChars(cx);
    if (!s)
        return false;

    *flagsOut = RegExpFlag(0);
    for (size_t i = 0; i < n; i++) {
#define HANDLE_FLAG(name_)                                                    \
        JS_BEGIN_MACRO                                                        \
            if (*flagsOut & (name_))                                          \
                goto bad_flag;                                                \
            *flagsOut = RegExpFlag(*flagsOut | (name_));                      \
        JS_END_MACRO
        switch (s[i]) {
          case 'i': HANDLE_FLAG(IgnoreCaseFlag); break;
          case 'g': HANDLE_FLAG(GlobalFlag); break;
          case 'm': HANDLE_FLAG(MultilineFlag); break;
          case 'y': HANDLE_FLAG(StickyFlag); break;
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

AlreadyIncRefed<RegExpPrivate>
RegExpPrivate::createFlagged(JSContext *cx, JSString *str, JSString *opt, TokenStream *ts)
{
    if (!opt)
        return create(cx, str, RegExpFlag(0), ts);

    RegExpFlag flags = RegExpFlag(0);
    if (!ParseRegExpFlags(cx, opt, &flags))
        return AlreadyIncRefed<RegExpPrivate>(NULL);

    return create(cx, str, flags, ts);
}

RegExpObject *
RegExpObject::clone(JSContext *cx, RegExpObject *obj, RegExpObject *proto)
{
    JSObject *clone = NewNativeClassInstance(cx, &RegExpClass, proto, proto->getParent());
    if (!clone)
        return NULL;

    /*
     * This clone functionality does not duplicate the JIT'd code blob,
     * which is necessary for cross-compartment cloning functionality.
     */
    assertSameCompartment(cx, obj, clone);

    RegExpStatics *res = cx->regExpStatics();

    /* 
     * Check that the RegExpPrivate for the original is okay to use in
     * the clone -- if the RegExpStatics provides more flags we'll need
     * a different RegExpPrivate.
     */
    AlreadyIncRefed<RegExpPrivate> rep(NULL);
    {
        RegExpFlag origFlags = obj->getFlags();
        RegExpFlag staticsFlags = res->getFlags();
        if ((origFlags & staticsFlags) != staticsFlags) {
            RegExpFlag newFlags = RegExpFlag(origFlags | staticsFlags);
            rep = RegExpPrivate::create(cx, obj->getSource(), newFlags, NULL);
            if (!rep)
                return NULL;
        } else {
            RegExpPrivate *toShare = obj->getPrivate();
            toShare->incref(cx);
            rep = AlreadyIncRefed<RegExpPrivate>(toShare);
        }
    }

    JS_ASSERT(rep);

    PreInitRegExpObject pireo(clone);
    RegExpObject *reclone = pireo.get();
    if (!ResetRegExpObject(cx, reclone, rep)) {
        pireo.fail();
        return NULL;
    }

    pireo.succeed();
    return reclone;
}

JSObject * JS_FASTCALL
js_CloneRegExpObject(JSContext *cx, JSObject *obj, JSObject *proto)
{
    JS_ASSERT(obj->isRegExp());
    JS_ASSERT(proto->isRegExp());

    return RegExpObject::clone(cx, obj->asRegExp(), proto->asRegExp());
}

#ifdef JS_TRACER
JS_DEFINE_CALLINFO_3(extern, OBJECT, js_CloneRegExpObject, CONTEXT, OBJECT, OBJECT, 0,
                     ACCSET_STORE_ANY)
#endif

JSFlatString *
RegExpObject::toString(JSContext *cx) const
{
    RegExpPrivate *rep = getPrivate();
    if (!rep)
        return cx->runtime->emptyString;

    JSLinearString *src = rep->getSource();
    StringBuffer sb(cx);
    if (size_t len = src->length()) {
        if (!sb.reserve(len + 2))
            return NULL;
        sb.infallibleAppend('/');
        sb.infallibleAppend(src->chars(), len);
        sb.infallibleAppend('/');
    } else {
        if (!sb.append("/(?:)/"))
            return NULL;
    }
    if (rep->global() && !sb.append('g'))
        return NULL;
    if (rep->ignoreCase() && !sb.append('i'))
        return NULL;
    if (rep->multiline() && !sb.append('m'))
        return NULL;
    if (rep->sticky() && !sb.append('y'))
        return NULL;

    return sb.finishString();
}
