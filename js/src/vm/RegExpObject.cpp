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

#include "frontend/TokenStream.h"
#include "vm/RegExpStatics.h"
#include "vm/MatchPairs.h"

#include "jsobjinlines.h"
#include "jsstrinlines.h"

#include "vm/RegExpObject-inl.h"
#include "vm/RegExpStatics-inl.h"

using namespace js;
using js::detail::RegExpPrivate;
using js::detail::RegExpPrivateCode;

JS_STATIC_ASSERT(IgnoreCaseFlag == JSREG_FOLD);
JS_STATIC_ASSERT(GlobalFlag == JSREG_GLOB);
JS_STATIC_ASSERT(MultilineFlag == JSREG_MULTILINE);
JS_STATIC_ASSERT(StickyFlag == JSREG_STICKY);

/* RegExpMatcher */

bool
RegExpMatcher::resetWithTestOptimized(RegExpObject *reobj)
{
    JS_ASSERT(reobj->startsWithAtomizedGreedyStar());

    JSAtom *source = &reobj->getSource()->asAtom();
    AlreadyIncRefed<RegExpPrivate> priv =
        RegExpPrivate::createTestOptimized(cx, source, reobj->getFlags());
    if (!priv)
        return false;

    /*
     * Create a dummy RegExpObject to persist this RegExpPrivate until the next GC.
     * Note that we give the ref we have to this new object.
     */
    RegExpObjectBuilder builder(cx);
    RegExpObject *dummy = builder.build(priv);
    if (!dummy) {
        priv->decref(cx);
        return false;
    }

    arc.reset(NeedsIncRef<RegExpPrivate>(priv.get()));
    return true;
}

/* RegExpObjectBuilder */

bool
RegExpObjectBuilder::getOrCreate()
{
    if (reobj_)
        return true;

    JSObject *obj = NewBuiltinClassInstance(cx, &RegExpClass);
    if (!obj)
        return false;
    obj->setPrivate(NULL);

    reobj_ = obj->asRegExp();
    return true;
}

bool
RegExpObjectBuilder::getOrCreateClone(RegExpObject *proto)
{
    JS_ASSERT(!reobj_);

    JSObject *clone = NewObjectWithGivenProto(cx, &RegExpClass, proto, proto->getParent());
    if (!clone)
        return false;
    clone->setPrivate(NULL);

    reobj_ = clone->asRegExp();
    return true;
}

RegExpObject *
RegExpObjectBuilder::build(AlreadyIncRefed<RegExpPrivate> rep)
{
    if (!getOrCreate()) {
        rep->decref(cx);
        return NULL;
    }

    reobj_->purge(cx);
    if (!reobj_->init(cx, rep->getSource(), rep->getFlags())) {
        rep->decref(cx);
        return NULL;
    }
    reobj_->setPrivate(rep.get());

    return reobj_;
}

RegExpObject *
RegExpObjectBuilder::build(JSLinearString *source, RegExpFlag flags)
{
    if (!getOrCreate())
        return NULL;

    reobj_->purge(cx);
    return reobj_->init(cx, source, flags) ? reobj_ : NULL;
}

RegExpObject *
RegExpObjectBuilder::build(RegExpObject *other)
{
    RegExpPrivate *rep = other->getOrCreatePrivate(cx);
    if (!rep)
        return NULL;

    /* Now, incref it for the RegExpObject being built. */
    rep->incref(cx);
    return build(AlreadyIncRefed<RegExpPrivate>(rep));
}

RegExpObject *
RegExpObjectBuilder::clone(RegExpObject *other, RegExpObject *proto)
{
    if (!getOrCreateClone(proto))
        return NULL;

    /*
     * Check that the RegExpPrivate for the original is okay to use in
     * the clone -- if the |RegExpStatics| provides more flags we'll
     * need a different |RegExpPrivate|.
     */
    RegExpStatics *res = cx->regExpStatics();
    RegExpFlag origFlags = other->getFlags();
    RegExpFlag staticsFlags = res->getFlags();
    if ((origFlags & staticsFlags) != staticsFlags) {
        RegExpFlag newFlags = RegExpFlag(origFlags | staticsFlags);
        return build(other->getSource(), newFlags);
    }

    RegExpPrivate *toShare = other->getOrCreatePrivate(cx);
    if (!toShare)
        return NULL;

    toShare->incref(cx);
    return build(AlreadyIncRefed<RegExpPrivate>(toShare));
}

/* MatchPairs */

MatchPairs *
MatchPairs::create(LifoAlloc &alloc, size_t pairCount, size_t backingPairCount)
{
    void *mem = alloc.alloc(calculateSize(backingPairCount));
    if (!mem)
        return NULL;

    return new (mem) MatchPairs(pairCount);
}

inline void
MatchPairs::checkAgainst(size_t inputLength)
{
#if DEBUG
    for (size_t i = 0; i < pairCount(); ++i) {
        MatchPair p = pair(i);
        p.check();
        if (p.isUndefined())
            continue;
        JS_ASSERT(size_t(p.limit) <= inputLength);
    }
#endif
}

RegExpRunStatus
RegExpPrivate::execute(JSContext *cx, const jschar *chars, size_t length, size_t *lastIndex,
                       LifoAllocScope &allocScope, MatchPairs **output)
{
    const size_t origLength = length;
    size_t backingPairCount = RegExpPrivateCode::getOutputSize(pairCount());

    MatchPairs *matchPairs = MatchPairs::create(allocScope.alloc(), pairCount(), backingPairCount);
    if (!matchPairs)
        return RegExpRunStatus_Error;

    /*
     * |displacement| emulates sticky mode by matching from this offset
     * into the char buffer and subtracting the delta off at the end.
     */
    size_t start = *lastIndex;
    size_t displacement = 0;

    if (sticky()) {
        displacement = *lastIndex;
        chars += displacement;
        length -= displacement;
        start = 0;
    }

    RegExpRunStatus status = code.execute(cx, chars, length, start,
                                          matchPairs->buffer(), backingPairCount);

    switch (status) {
      case RegExpRunStatus_Error:
        return status;
      case RegExpRunStatus_Success_NotFound:
        *output = matchPairs;
        return status;
      default:
        JS_ASSERT(status == RegExpRunStatus_Success);
    }

    matchPairs->displace(displacement);
    matchPairs->checkAgainst(origLength);

    *lastIndex = matchPairs->pair(0).limit;
    *output = matchPairs;

    return RegExpRunStatus_Success;
}

RegExpPrivate *
RegExpObject::makePrivate(JSContext *cx)
{
    JS_ASSERT(!getPrivate());
    AlreadyIncRefed<RegExpPrivate> rep = RegExpPrivate::create(cx, getSource(), getFlags(), NULL);
    if (!rep)
        return NULL;

    setPrivate(rep.get());
    return rep.get();
}

RegExpRunStatus
RegExpObject::execute(JSContext *cx, const jschar *chars, size_t length, size_t *lastIndex,
                      LifoAllocScope &allocScope, MatchPairs **output)
{
    if (!getPrivate() && !makePrivate(cx))
        return RegExpRunStatus_Error;

    return getPrivate()->execute(cx, chars, length, lastIndex, allocScope, output);
}

Shape *
RegExpObject::assignInitialShape(JSContext *cx)
{
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
        JSAtom *atom = js_AtomizeString(xdr->cx, source);
        if (!atom)
            return false;
        RegExpObject *reobj = RegExpObject::createNoStatics(xdr->cx, atom, RegExpFlag(flagsword),
                                                            NULL);
        if (!reobj)
            return false;

        if (!reobj->clearParent(xdr->cx))
            return false;
        if (!reobj->clearType(xdr->cx))
            return false;
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
    obj->asRegExp()->finalize(cx);
}

static void
regexp_trace(JSTracer *trc, JSObject *obj)
{
    if (IS_GC_MARKING_TRACER(trc))
        obj->asRegExp()->purge(trc->context);
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
    regexp_trace
};

#if ENABLE_YARR_JIT
void
RegExpPrivateCode::reportYarrError(JSContext *cx, TokenStream *ts, ErrorCode error)
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

#else /* !ENABLE_YARR_JIT */

void
RegExpPrivateCode::reportPCREError(JSContext *cx, int error)
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

#endif /* ENABLE_YARR_JIT */

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

RegExpPrivate *
RegExpPrivate::createUncached(JSContext *cx, JSLinearString *source, RegExpFlag flags,
                              TokenStream *tokenStream)
{
    RegExpPrivate *priv = cx->new_<RegExpPrivate>(source, flags);
    if (!priv)
        return NULL;

    if (!priv->compile(cx, tokenStream)) {
        Foreground::delete_(priv);
        return NULL;
    }

    return priv;
}

AlreadyIncRefed<RegExpPrivate>
RegExpPrivate::createTestOptimized(JSContext *cx, JSAtom *cacheKey, RegExpFlag flags)
{
    typedef AlreadyIncRefed<RegExpPrivate> RetType;

    RetType cached;
    if (!cacheLookup(cx, cacheKey, flags, RegExpPrivateCache_TestOptimized, &cached))
        return RetType(NULL);

    if (cached)
        return cached;

    /* Strip off the greedy star characters, create a new RegExpPrivate, and cache. */
    JS_ASSERT(cacheKey->length() > JS_ARRAY_LENGTH(GreedyStarChars));
    JSDependentString *stripped =
      JSDependentString::new_(cx, cacheKey, cacheKey->chars() + JS_ARRAY_LENGTH(GreedyStarChars),
                              cacheKey->length() - JS_ARRAY_LENGTH(GreedyStarChars));
    if (!stripped)
        return RetType(NULL);

    RegExpPrivate *priv = createUncached(cx, cacheKey, flags, NULL);
    if (!priv)
        return RetType(NULL);

    if (!cacheInsert(cx, cacheKey, RegExpPrivateCache_TestOptimized, priv)) {
        priv->decref(cx);
        return RetType(NULL);
    }

    return RetType(priv);
}

AlreadyIncRefed<RegExpPrivate>
RegExpPrivate::create(JSContext *cx, JSLinearString *str, JSString *opt, TokenStream *ts)
{
    if (!opt)
        return create(cx, str, RegExpFlag(0), ts);

    RegExpFlag flags = RegExpFlag(0);
    if (!ParseRegExpFlags(cx, opt, &flags))
        return AlreadyIncRefed<RegExpPrivate>(NULL);

    return create(cx, str, flags, ts);
}

JSObject * JS_FASTCALL
js_CloneRegExpObject(JSContext *cx, JSObject *obj, JSObject *proto)
{
    JS_ASSERT(obj->isRegExp());
    JS_ASSERT(proto->isRegExp());

    RegExpObjectBuilder builder(cx);
    return builder.clone(obj->asRegExp(), proto->asRegExp());
}

JSFlatString *
RegExpObject::toString(JSContext *cx) const
{
    JSLinearString *src = getSource();
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
    if (global() && !sb.append('g'))
        return NULL;
    if (ignoreCase() && !sb.append('i'))
        return NULL;
    if (multiline() && !sb.append('m'))
        return NULL;
    if (sticky() && !sb.append('y'))
        return NULL;

    return sb.finishString();
}
