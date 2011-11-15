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

#include "mozilla/Util.h"

#include "RegExpObject.h"
#include "RegExpStatics.h"

#include "jsobjinlines.h"
#include "jsstrinlines.h"
#include "RegExpStatics-inl.h"

inline js::RegExpObject *
JSObject::asRegExp()
{
    JS_ASSERT(isRegExp());
    return static_cast<js::RegExpObject *>(this);
}

namespace js {

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

inline size_t *
RegExpObject::addressOfPrivateRefCount() const
{
    JS_ASSERT(getPrivate());
    return getPrivate()->addressOfRefCount();
}

inline void
RegExpObject::setPrivate(RegExpPrivate *rep)
{
    JSObject::setPrivate(rep);
}

inline RegExpObject *
RegExpObject::create(JSContext *cx, RegExpStatics *res, const jschar *chars, size_t length,
                     RegExpFlag flags, TokenStream *tokenStream)
{
    RegExpFlag staticsFlags = res->getFlags();
    return createNoStatics(cx, chars, length, RegExpFlag(flags | staticsFlags), tokenStream);
}

inline RegExpObject *
RegExpObject::createNoStatics(JSContext *cx, const jschar *chars, size_t length,
                              RegExpFlag flags, TokenStream *tokenStream)
{
    JSAtom *source = js_AtomizeChars(cx, chars, length);
    if (!source)
        return NULL;

    return createNoStatics(cx, source, flags, tokenStream);
}

inline RegExpObject *
RegExpObject::createNoStatics(JSContext *cx, JSAtom *source, RegExpFlag flags,
                              TokenStream *tokenStream)
{
    if (!RegExpPrivateCode::checkSyntax(cx, tokenStream, source))
        return NULL;

    RegExpObjectBuilder builder(cx);
    return builder.build(source, flags);
}

inline void
RegExpObject::purge(JSContext *cx)
{
    if (RegExpPrivate *rep = getPrivate()) {
        rep->decref(cx);
        privateData = NULL;
    }
}

inline void
RegExpObject::finalize(JSContext *cx)
{
    purge(cx);
#ifdef DEBUG
    setPrivate((RegExpPrivate *) 0x1); /* Non-null but still in the zero page. */
#endif
}

inline bool
RegExpObject::init(JSContext *cx, JSLinearString *source, RegExpFlag flags)
{
    if (nativeEmpty()) {
        const js::Shape *shape = cx->compartment->initialRegExpShape;
        if (!shape) {
            shape = assignInitialShape(cx);
            if (!shape)
                return false;
            cx->compartment->initialRegExpShape = shape;
        }
        setLastProperty(shape);
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

    JS_ASSERT(!getPrivate());
    zeroLastIndex();
    setSource(source);
    setGlobal(flags & GlobalFlag);
    setIgnoreCase(flags & IgnoreCaseFlag);
    setMultiline(flags & MultilineFlag);
    setSticky(flags & StickyFlag);
    return true;
}

inline bool
RegExpMatcher::reset(JSLinearString *patstr, JSString *opt)
{
    AlreadyIncRefed<RegExpPrivate> priv = RegExpPrivate::create(cx, patstr, opt, NULL);
    if (!priv)
        return false;
    arc.reset(priv);
    return true;
}

inline void
RegExpObject::setLastIndex(const Value &v)
{
    setSlot(LAST_INDEX_SLOT, v);
}

inline void
RegExpObject::setLastIndex(double d)
{
    setSlot(LAST_INDEX_SLOT, NumberValue(d));
}

inline void
RegExpObject::zeroLastIndex()
{
    setSlot(LAST_INDEX_SLOT, Int32Value(0));
}

inline void
RegExpObject::setSource(JSLinearString *source)
{
    setSlot(SOURCE_SLOT, StringValue(source));
}

inline void
RegExpObject::setIgnoreCase(bool enabled)
{
    setSlot(IGNORE_CASE_FLAG_SLOT, BooleanValue(enabled));
}

inline void
RegExpObject::setGlobal(bool enabled)
{
    setSlot(GLOBAL_FLAG_SLOT, BooleanValue(enabled));
}

inline void
RegExpObject::setMultiline(bool enabled)
{
    setSlot(MULTILINE_FLAG_SLOT, BooleanValue(enabled));
}

inline void
RegExpObject::setSticky(bool enabled)
{
    setSlot(STICKY_FLAG_SLOT, BooleanValue(enabled));
}

/* RegExpPrivate inlines. */

inline RegExpPrivateCache *
detail::RegExpPrivate::getOrCreateCache(JSContext *cx)
{
    if (RegExpPrivateCache *cache = cx->threadData()->getOrCreateRegExpPrivateCache(cx))
        return cache;

    js_ReportOutOfMemory(cx);
    return NULL;
}

inline bool
detail::RegExpPrivate::cacheLookup(JSContext *cx, JSAtom *atom, RegExpFlag flags,
                                   AlreadyIncRefed<RegExpPrivate> *result)
{
    RegExpPrivateCache *cache = getOrCreateCache(cx);
    if (!cache)
        return false;

    if (RegExpPrivateCache::Ptr p = cache->lookup(atom)) {
        NeedsIncRef<RegExpPrivate> cached(p->value);
        if (cached->getFlags() == flags) {
            cached->incref(cx);
            *result = AlreadyIncRefed<RegExpPrivate>(cached.get());
            return true;
        }
    }

    JS_ASSERT(result->null());
    return true;
}

inline bool
detail::RegExpPrivate::cacheInsert(JSContext *cx, JSAtom *atom, RegExpPrivate *priv)
{
    JS_ASSERT(priv);

    /*
     * Note: allocation performed since lookup may cause a garbage collection,
     * so we have to re-lookup the cache (and inside the cache) after the
     * allocation is performed.
     */
    RegExpPrivateCache *cache = getOrCreateCache(cx);
    if (!cache)
        return false;

    if (RegExpPrivateCache::AddPtr addPtr = cache->lookupForAdd(atom)) {
        /* We clobber existing entries with the same source (but different flags). */
        JS_ASSERT(addPtr->value->getFlags() != priv->getFlags());
        addPtr->value = priv;
    } else {
        if (!cache->add(addPtr, atom, priv)) {
            js_ReportOutOfMemory(cx);
            return false;
        }
    }

    return true;
}

inline AlreadyIncRefed<detail::RegExpPrivate>
detail::RegExpPrivate::create(JSContext *cx, JSLinearString *source, RegExpFlag flags,
                              TokenStream *ts)
{
    typedef AlreadyIncRefed<RegExpPrivate> RetType;

    /*
     * We choose to only cache |RegExpPrivate|s who have atoms as
     * sources, under the unverified premise that non-atoms will have a
     * low hit rate (both hit ratio and absolute number of hits).
     */
    bool cacheable = source->isAtom();
    if (!cacheable)
        return RetType(RegExpPrivate::createUncached(cx, source, flags, ts));

    /*
     * Refcount note: not all |RegExpPrivate|s are cached so we need to
     * keep a refcount. The cache holds a "weak ref", where the
     * |RegExpPrivate|'s deallocation decref will first cause it to
     * remove itself from the cache.
     */

    JSAtom *sourceAtom = &source->asAtom();

    AlreadyIncRefed<RegExpPrivate> cached;
    if (!cacheLookup(cx, sourceAtom, flags, &cached))
        return RetType(NULL);

    if (cached)
        return cached;

    RegExpPrivate *priv = RegExpPrivate::createUncached(cx, source, flags, ts);
    if (!priv)
        return RetType(NULL);

    if (!cacheInsert(cx, sourceAtom, priv))
        return RetType(NULL);

    return RetType(priv);
}

/* This function should be deleted once bad Android platforms phase out. See bug 604774. */
inline bool
detail::RegExpPrivateCode::isJITRuntimeEnabled(JSContext *cx)
{
#if defined(ANDROID) && defined(JS_TRACER) && defined(JS_METHODJIT)
    return cx->traceJitEnabled || cx->methodJitEnabled;
#else
    return true;
#endif
}

inline bool
detail::RegExpPrivateCode::compile(JSContext *cx, JSLinearString &pattern, TokenStream *ts,
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
        JSC::ExecutableAllocator *execAlloc = cx->threadData()->getOrCreateExecutableAllocator(cx);
        if (!execAlloc) {
            js_ReportOutOfMemory(cx);
            return false;
        }

        JSGlobalData globalData(execAlloc);
        jitCompile(yarrPattern, &globalData, codeBlock);
        if (!codeBlock.isFallBack())
            return true;
    }
#endif

    WTF::BumpPointerAllocator *bumpAlloc = cx->threadData()->getOrCreateBumpPointerAllocator(cx);
    if (!bumpAlloc) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    codeBlock.setFallBack(true);
    byteCode = byteCompile(yarrPattern, bumpAlloc).get();
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
detail::RegExpPrivate::compile(JSContext *cx, TokenStream *ts)
{
    if (!sticky())
        return code.compile(cx, *source, ts, &parenCount, getFlags());

    /*
     * The sticky case we implement hackily by prepending a caret onto the front
     * and relying on |::execute| to pseudo-slice the string when it sees a sticky regexp.
     */
    static const jschar prefix[] = {'^', '(', '?', ':'};
    static const jschar postfix[] = {')'};

    using mozilla::ArrayLength;
    StringBuffer sb(cx);
    if (!sb.reserve(ArrayLength(prefix) + source->length() + ArrayLength(postfix)))
        return false;
    sb.infallibleAppend(prefix, ArrayLength(prefix));
    sb.infallibleAppend(source->chars(), source->length());
    sb.infallibleAppend(postfix, ArrayLength(postfix));

    JSLinearString *fakeySource = sb.finishString();
    if (!fakeySource)
        return false;
    return code.compile(cx, *fakeySource, ts, &parenCount, getFlags());
}

inline RegExpRunStatus
detail::RegExpPrivateCode::execute(JSContext *cx, const jschar *chars, size_t length, size_t start,
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
        return RegExpRunStatus_Success_NotFound;

#if !ENABLE_YARR_JIT
    if (result < 0) {
        reportPCREError(cx, result);
        return RegExpRunStatus_Error;
    }
#endif

    JS_ASSERT(result >= 0);
    return RegExpRunStatus_Success;
}

inline void
detail::RegExpPrivate::incref(JSContext *cx)
{
    ++refCount;
}

inline void
detail::RegExpPrivate::decref(JSContext *cx)
{
#ifdef JS_THREADSAFE
    JS_OPT_ASSERT_IF(cx->runtime->gcHelperThread.getThread(),
                     PR_GetCurrentThread() != cx->runtime->gcHelperThread.getThread());
#endif

    if (--refCount != 0)
        return;

    RegExpPrivateCache *cache;
    if (source->isAtom() && (cache = cx->threadData()->getRegExpPrivateCache())) {
        RegExpPrivateCache::Ptr ptr = cache->lookup(&source->asAtom());
        if (ptr && ptr->value == this)
            cache->remove(ptr);
    }

#ifdef DEBUG
    this->~RegExpPrivate();
    memset(this, 0xcd, sizeof(*this));
    cx->free_(this);
#else
    cx->delete_(this);
#endif
}

} /* namespace js */

#endif
