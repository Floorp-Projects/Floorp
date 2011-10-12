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

#ifndef RegExpObject_inl_h___
#define RegExpObject_inl_h___

#include "RegExpObject.h"
#include "RegExpStatics.h"

#include "jsobjinlines.h"
#include "jsstrinlines.h"
#include "RegExpStatics-inl.h"

inline js::RegExpObject *
JSObject::asRegExp()
{
    JS_ASSERT(isRegExp());
    js::RegExpObject *reobj = static_cast<js::RegExpObject *>(this);
    JS_ASSERT(reobj->getPrivate());
    return reobj;
}

namespace js {

/*
 * Maintains the post-initialization invariant of having a RegExpPrivate.
 *
 * N.B. If initialization fails, the |RegExpPrivate| will be null, so
 * finalization must consider that as a possibility.
 */
class PreInitRegExpObject
{
    RegExpObject    *reobj;
    DebugOnly<bool> gotResult;

  public:
    explicit PreInitRegExpObject(JSObject *obj) {
        JS_ASSERT(obj->isRegExp());
        reobj = static_cast<RegExpObject *>(obj);
        gotResult = false;
    }

    ~PreInitRegExpObject() {
        JS_ASSERT(gotResult);
    }

    RegExpObject *get() { return reobj; }

    void succeed() {
        JS_ASSERT(!gotResult);
        JS_ASSERT(reobj->getPrivate());
        gotResult = true;
    }

    void fail() {
        JS_ASSERT(!gotResult);
        gotResult = true;
    }
};

inline bool
ValueIsRegExp(const Value &v)
{
    return !v.isPrimitive() && v.toObject().isRegExp();
}

inline bool
IsRegExpMetaChar(jschar c)
{
    switch (c) {
      /* Taken from the PatternCharacter production in 15.10.1. */
      case '^': case '$': case '\\': case '.': case '*': case '+':
      case '?': case '(': case ')': case '[': case ']': case '{':
      case '}': case '|':
        return true;
      default:
        return false;
    }
}

inline bool
HasRegExpMetaChars(const jschar *chars, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        if (IsRegExpMetaChar(chars[i]))
            return true;
    }
    return false;
}

inline bool
ResetRegExpObject(JSContext *cx, RegExpObject *reobj, JSString *str, RegExpFlag flags)
{
    AlreadyIncRefed<RegExpPrivate> rep = RegExpPrivate::create(cx, str, flags, NULL);
    if (!rep)
        return false;

    return reobj->reset(cx, rep);
}

inline bool
ResetRegExpObject(JSContext *cx, RegExpObject *reobj, AlreadyIncRefed<RegExpPrivate> rep)
{
    return reobj->reset(cx, rep);
}

inline RegExpObject *
RegExpObject::create(JSContext *cx, RegExpStatics *res, const jschar *chars, size_t length,
                     RegExpFlag flags, TokenStream *ts)
{
    RegExpFlag staticsFlags = res->getFlags();
    return createNoStatics(cx, chars, length, RegExpFlag(flags | staticsFlags), ts);
}

inline RegExpObject *
RegExpObject::createNoStatics(JSContext *cx, const jschar *chars, size_t length,
                              RegExpFlag flags, TokenStream *ts)
{
    JSString *str = js_NewStringCopyN(cx, chars, length);
    if (!str)
        return NULL;

    /*
     * |NewBuiltinClassInstance| can GC before we store |re| in the
     * private field of the object. At that point the only reference to
     * the source string could be from the malloc-allocated GC-invisible
     * |re|. So we must anchor.
     */
    JS::Anchor<JSString *> anchor(str);
    AlreadyIncRefed<RegExpPrivate> rep = RegExpPrivate::create(cx, str, flags, ts);
    if (!rep)
        return NULL;

    JSObject *obj = NewBuiltinClassInstance(cx, &RegExpClass);
    if (!obj)
        return NULL;

    PreInitRegExpObject pireo(obj);
    RegExpObject *reobj = pireo.get();
    if (!ResetRegExpObject(cx, reobj, rep)) {
        rep->decref(cx);
        pireo.fail();
        return NULL;
    }

    pireo.succeed();
    return reobj;
}

inline bool
RegExpObject::reset(JSContext *cx, AlreadyIncRefed<RegExpPrivate> rep)
{
    if (nativeEmpty()) {
        const js::Shape **shapep = &cx->compartment->initialRegExpShape;
        if (!*shapep) {
            *shapep = assignInitialShape(cx);
            if (!*shapep)
                return false;
        }
        setLastProperty(*shapep);
        JS_ASSERT(!nativeEmpty());
    }

    DebugOnly<JSAtomState *> atomState = &cx->runtime->atomState;
    JS_ASSERT(nativeLookup(cx, ATOM_TO_JSID(atomState->lastIndexAtom))->slot == LAST_INDEX_SLOT);
    JS_ASSERT(nativeLookup(cx, ATOM_TO_JSID(atomState->sourceAtom))->slot == SOURCE_SLOT);
    JS_ASSERT(nativeLookup(cx, ATOM_TO_JSID(atomState->globalAtom))->slot == GLOBAL_FLAG_SLOT);
    JS_ASSERT(nativeLookup(cx, ATOM_TO_JSID(atomState->ignoreCaseAtom))->slot ==
                                 IGNORE_CASE_FLAG_SLOT);
    JS_ASSERT(nativeLookup(cx, ATOM_TO_JSID(atomState->multilineAtom))->slot ==
                                 MULTILINE_FLAG_SLOT);
    JS_ASSERT(nativeLookup(cx, ATOM_TO_JSID(atomState->stickyAtom))->slot == STICKY_FLAG_SLOT);

    setPrivate(rep.get());
    zeroLastIndex();
    setSource(rep->getSource());
    setGlobal(rep->global());
    setIgnoreCase(rep->ignoreCase());
    setMultiline(rep->multiline());
    setSticky(rep->sticky());
    return true;
}

/* RegExpPrivate inlines. */

class RegExpMatchBuilder
{
    JSContext   * const cx;
    JSObject    * const array;

    bool setProperty(JSAtom *name, Value v) {
        return !!js_DefineProperty(cx, array, ATOM_TO_JSID(name), &v,
                                   JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE);
    }

  public:
    RegExpMatchBuilder(JSContext *cx, JSObject *array) : cx(cx), array(array) {}

    bool append(uint32 index, Value v) {
        JS_ASSERT(!array->getOps()->getElement);
        return !!js_DefineElement(cx, array, index, &v, JS_PropertyStub, JS_StrictPropertyStub,
                                  JSPROP_ENUMERATE);
    }

    bool setIndex(int index) {
        return setProperty(cx->runtime->atomState.indexAtom, Int32Value(index));
    }

    bool setInput(JSString *str) {
        JS_ASSERT(str);
        return setProperty(cx->runtime->atomState.inputAtom, StringValue(str));
    }
};

inline void
RegExpPrivate::checkMatchPairs(JSString *input, int *buf, size_t matchItemCount)
{
#if DEBUG
    size_t inputLength = input->length();
    for (size_t i = 0; i < matchItemCount; i += 2) {
        int start = buf[i];
        int limit = buf[i + 1];
        JS_ASSERT(limit >= start); /* Limit index must be larger than the start index. */
        if (start == -1)
            continue;
        JS_ASSERT(start >= 0);
        JS_ASSERT(size_t(limit) <= inputLength);
    }
#endif
}

inline JSObject *
RegExpPrivate::createResult(JSContext *cx, JSString *input, int *buf, size_t matchItemCount)
{
    /*
     * Create the result array for a match. Array contents:
     *  0:              matched string
     *  1..pairCount-1: paren matches
     */
    JSObject *array = NewSlowEmptyArray(cx);
    if (!array)
        return NULL;

    RegExpMatchBuilder builder(cx, array);
    for (size_t i = 0; i < matchItemCount; i += 2) {
        int start = buf[i];
        int end = buf[i + 1];

        JSString *captured;
        if (start >= 0) {
            JS_ASSERT(start <= end);
            JS_ASSERT(unsigned(end) <= input->length());
            captured = js_NewDependentString(cx, input, start, end - start);
            if (!captured || !builder.append(i / 2, StringValue(captured)))
                return NULL;
        } else {
            /* Missing parenthesized match. */
            JS_ASSERT(i != 0); /* Since we had a match, first pair must be present. */
            if (!builder.append(i / 2, UndefinedValue()))
                return NULL;
        }
    }

    if (!builder.setIndex(buf[0]) || !builder.setInput(input))
        return NULL;

    return array;
}

inline AlreadyIncRefed<RegExpPrivate>
RegExpPrivate::create(JSContext *cx, JSString *source, RegExpFlag flags, TokenStream *ts)
{
    typedef AlreadyIncRefed<RegExpPrivate> RetType;

    JSLinearString *flatSource = source->ensureLinear(cx);
    if (!flatSource)
        return RetType(NULL);

    RegExpPrivate *self = cx->new_<RegExpPrivate>(flatSource, flags, cx->compartment);
    if (!self)
        return RetType(NULL);

    if (!self->compile(cx, ts)) {
        Foreground::delete_(self);
        return RetType(NULL);
    }

    return RetType(self);
}

/* This function should be deleted once bad Android platforms phase out. See bug 604774. */
inline bool
RegExpPrivateCode::isJITRuntimeEnabled(JSContext *cx)
{
#if defined(ANDROID) && defined(JS_TRACER) && defined(JS_METHODJIT)
    return cx->traceJitEnabled || cx->methodJitEnabled;
#else
    return true;
#endif
}

inline bool
RegExpPrivateCode::compile(JSContext *cx, JSLinearString &pattern, TokenStream *ts,
                           uintN *parenCount, RegExpFlag flags)
{
#if ENABLE_YARR_JIT
    /* Parse the pattern. */
    ErrorCode yarrError;
    YarrPattern yarrPattern(pattern, bool(flags & IgnoreCaseFlag), bool(flags & MultilineFlag),
                            &yarrError);
    if (yarrError) {
        reportYarrError(cx, ts, yarrError);
        return false;
    }
    *parenCount = yarrPattern.m_numSubpatterns;

    /*
     * The YARR JIT compiler attempts to compile the parsed pattern. If
     * it cannot, it informs us via |codeBlock.isFallBack()|, in which
     * case we have to bytecode compile it.
     */

#ifdef JS_METHODJIT
    if (isJITRuntimeEnabled(cx) && !yarrPattern.m_containsBackreferences) {
        if (!cx->compartment->ensureJaegerCompartmentExists(cx))
            return false;

        JSGlobalData globalData(cx->compartment->jaegerCompartment()->execAlloc());
        jitCompile(yarrPattern, &globalData, codeBlock);
        if (!codeBlock.isFallBack())
            return true;
    }
#endif

    codeBlock.setFallBack(true);
    byteCode = byteCompile(yarrPattern, cx->compartment->regExpAllocator).get();
    return true;
#else /* !defined(ENABLE_YARR_JIT) */
    int error = 0;
    compiled = jsRegExpCompile(pattern.chars(), pattern.length(),
                  ignoreCase() ? JSRegExpIgnoreCase : JSRegExpDoNotIgnoreCase,
                  multiline() ? JSRegExpMultiline : JSRegExpSingleLine,
                  parenCount, &error);
    if (error) {
        reportPCREError(cx, error);
        return false;
    }
    return true;
#endif
}

inline bool
RegExpPrivate::compile(JSContext *cx, TokenStream *ts)
{
    /* Flatten source early for the rest of compilation. */
    if (!source->ensureLinear(cx))
        return false;

    if (!sticky())
        return code.compile(cx, *source, ts, &parenCount, getFlags());

    /*
     * The sticky case we implement hackily by prepending a caret onto the front
     * and relying on |::execute| to pseudo-slice the string when it sees a sticky regexp.
     */
    static const jschar prefix[] = {'^', '(', '?', ':'};
    static const jschar postfix[] = {')'};

    StringBuffer sb(cx);
    if (!sb.reserve(JS_ARRAY_LENGTH(prefix) + source->length() + JS_ARRAY_LENGTH(postfix)))
        return false;
    sb.infallibleAppend(prefix, JS_ARRAY_LENGTH(prefix));
    sb.infallibleAppend(source->chars(), source->length());
    sb.infallibleAppend(postfix, JS_ARRAY_LENGTH(postfix));

    JSLinearString *fakeySource = sb.finishString();
    if (!fakeySource)
        return false;
    return code.compile(cx, *fakeySource, ts, &parenCount, getFlags());
}

inline RegExpPrivateCode::ExecuteResult
RegExpPrivateCode::execute(JSContext *cx, const jschar *chars, size_t start, size_t length,
                           int *output, size_t outputCount)
{
    int result;
#if ENABLE_YARR_JIT
    (void) cx; /* Unused. */
    if (codeBlock.isFallBack())
        result = JSC::Yarr::interpret(byteCode, chars, start, length, output);
    else
        result = JSC::Yarr::execute(codeBlock, chars, start, length, output);
#else
    result = jsRegExpExecute(cx, compiled, chars, length, start, output, outputCount);
#endif

    if (result == -1)
        return Success_NotFound;

#if !ENABLE_YARR_JIT
    if (result < 0) {
        reportPCREError(cx, result);
        return Error;
    }
#endif

    JS_ASSERT(result >= 0);
    return Success;
}

inline void
RegExpPrivate::incref(JSContext *cx)
{
#ifdef DEBUG
    assertSameCompartment(cx, compartment);
#endif
    ++refCount;
}

inline void
RegExpPrivate::decref(JSContext *cx)
{
#ifdef DEBUG
    assertSameCompartment(cx, compartment);
#endif
    if (--refCount == 0)
        cx->delete_(this);
}

inline RegExpPrivate *
RegExpObject::getPrivate() const
{
    RegExpPrivate *rep = static_cast<RegExpPrivate *>(JSObject::getPrivate());
#ifdef DEBUG
    if (rep)
        CompartmentChecker::check(compartment(), rep->compartment);
#endif
    return rep;
}

} /* namespace js */

#endif
