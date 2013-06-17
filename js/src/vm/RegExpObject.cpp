/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/TokenStream.h"
#include "vm/MatchPairs.h"
#include "vm/RegExpStatics.h"
#include "vm/StringBuffer.h"
#include "vm/Xdr.h"

#include "jsobjinlines.h"

#include "vm/RegExpObject-inl.h"
#include "vm/RegExpStatics-inl.h"

using namespace js;
using js::frontend::TokenStream;

JS_STATIC_ASSERT(IgnoreCaseFlag == JSREG_FOLD);
JS_STATIC_ASSERT(GlobalFlag == JSREG_GLOB);
JS_STATIC_ASSERT(MultilineFlag == JSREG_MULTILINE);
JS_STATIC_ASSERT(StickyFlag == JSREG_STICKY);

/* RegExpObjectBuilder */

RegExpObjectBuilder::RegExpObjectBuilder(JSContext *cx, RegExpObject *reobj)
  : cx(cx), reobj_(cx, reobj)
{}

bool
RegExpObjectBuilder::getOrCreate()
{
    if (reobj_)
        return true;

    // Note: RegExp objects are always allocated in the tenured heap. This is
    // not strictly required, but simplifies embedding them in jitcode.
    JSObject *obj = NewBuiltinClassInstance(cx, &RegExpClass, TenuredObject);
    if (!obj)
        return false;
    obj->initPrivate(NULL);

    reobj_ = &obj->asRegExp();
    return true;
}

bool
RegExpObjectBuilder::getOrCreateClone(RegExpObject *proto)
{
    JS_ASSERT(!reobj_);

    // Note: RegExp objects are always allocated in the tenured heap. This is
    // not strictly required, but simplifies embedding them in jitcode.
    JSObject *clone = NewObjectWithGivenProto(cx, &RegExpClass, proto, proto->getParent(),
                                              TenuredObject);
    if (!clone)
        return false;
    clone->initPrivate(NULL);

    reobj_ = &clone->asRegExp();
    return true;
}

RegExpObject *
RegExpObjectBuilder::build(HandleAtom source, RegExpShared &shared)
{
    if (!getOrCreate())
        return NULL;

    if (!reobj_->init(cx, source, shared.getFlags()))
        return NULL;

    reobj_->setShared(cx, shared);
    return reobj_;
}

RegExpObject *
RegExpObjectBuilder::build(HandleAtom source, RegExpFlag flags)
{
    if (!getOrCreate())
        return NULL;

    return reobj_->init(cx, source, flags) ? reobj_.get() : NULL;
}

RegExpObject *
RegExpObjectBuilder::clone(Handle<RegExpObject *> other, Handle<RegExpObject *> proto)
{
    if (!getOrCreateClone(proto))
        return NULL;

    /*
     * Check that the RegExpShared for the original is okay to use in
     * the clone -- if the |RegExpStatics| provides more flags we'll
     * need a different |RegExpShared|.
     */
    RegExpStatics *res = proto->getParent()->asGlobal().getRegExpStatics();
    RegExpFlag origFlags = other->getFlags();
    RegExpFlag staticsFlags = res->getFlags();
    if ((origFlags & staticsFlags) != staticsFlags) {
        RegExpFlag newFlags = RegExpFlag(origFlags | staticsFlags);
        Rooted<JSAtom *> source(cx, other->getSource());
        return build(source, newFlags);
    }

    RegExpGuard g(cx);
    if (!other->getShared(cx, &g))
        return NULL;

    Rooted<JSAtom *> source(cx, other->getSource());
    return build(source, *g);
}

/* MatchPairs */

bool
MatchPairs::initArray(size_t pairCount)
{
    JS_ASSERT(pairCount > 0);

    /* Guarantee adequate space in buffer. */
    if (!allocOrExpandArray(pairCount))
        return false;

    /* Initialize all MatchPair objects to invalid locations. */
    for (size_t i = 0; i < pairCount; i++) {
        pairs_[i].start = -1;
        pairs_[i].limit = -1;
    }

    return true;
}

bool
MatchPairs::initArrayFrom(MatchPairs &copyFrom)
{
    JS_ASSERT(copyFrom.pairCount() > 0);

    if (!allocOrExpandArray(copyFrom.pairCount()))
        return false;

    for (size_t i = 0; i < pairCount_; i++) {
        JS_ASSERT(copyFrom[i].check());
        pairs_[i].start = copyFrom[i].start;
        pairs_[i].limit = copyFrom[i].limit;
    }

    return true;
}

void
MatchPairs::displace(size_t disp)
{
    if (disp == 0)
        return;

    for (size_t i = 0; i < pairCount_; i++) {
        JS_ASSERT(pairs_[i].check());
        pairs_[i].start += (pairs_[i].start < 0) ? 0 : disp;
        pairs_[i].limit += (pairs_[i].limit < 0) ? 0 : disp;
    }
}

bool
ScopedMatchPairs::allocOrExpandArray(size_t pairCount)
{
    /* Array expansion is forbidden, but array reuse is acceptable. */
    if (pairCount_) {
        JS_ASSERT(pairs_);
        JS_ASSERT(pairCount_ == pairCount);
        return true;
    }

    JS_ASSERT(!pairs_);
    pairs_ = (MatchPair *)lifoScope_.alloc().alloc(sizeof(MatchPair) * pairCount);
    if (!pairs_)
        return false;

    pairCount_ = pairCount;
    return true;
}

bool
VectorMatchPairs::allocOrExpandArray(size_t pairCount)
{
    if (!vec_.resizeUninitialized(sizeof(MatchPair) * pairCount))
        return false;

    pairs_ = &vec_[0];
    pairCount_ = pairCount;
    return true;
}

/* RegExpObject */

static void
regexp_trace(JSTracer *trc, JSObject *obj)
{
     /*
      * We have to check both conditions, since:
      *   1. During TraceRuntime, isHeapBusy() is true
      *   2. When a write barrier executes, IS_GC_MARKING_TRACER is true.
      */
    if (trc->runtime->isHeapBusy() && IS_GC_MARKING_TRACER(trc))
        obj->setPrivate(NULL);
}

Class js::RegExpClass = {
    js_RegExp_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(RegExpObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_RegExp),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,        /* enumerate */
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,                    /* finalize */
    NULL,                    /* checkAccess */
    NULL,                    /* call */
    NULL,                    /* construct */
    NULL,                    /* hasInstance */
    regexp_trace
};

RegExpObject *
RegExpObject::create(JSContext *cx, RegExpStatics *res, const jschar *chars, size_t length,
                     RegExpFlag flags, TokenStream *tokenStream)
{
    RegExpFlag staticsFlags = res->getFlags();
    return createNoStatics(cx, chars, length, RegExpFlag(flags | staticsFlags), tokenStream);
}

RegExpObject *
RegExpObject::createNoStatics(JSContext *cx, const jschar *chars, size_t length, RegExpFlag flags,
                              TokenStream *tokenStream)
{
    RootedAtom source(cx, AtomizeChars<CanGC>(cx, chars, length));
    if (!source)
        return NULL;

    return createNoStatics(cx, source, flags, tokenStream);
}

RegExpObject *
RegExpObject::createNoStatics(JSContext *cx, HandleAtom source, RegExpFlag flags,
                              TokenStream *tokenStream)
{
    if (!RegExpShared::checkSyntax(cx, tokenStream, source))
        return NULL;

    RegExpObjectBuilder builder(cx);
    return builder.build(source, flags);
}

bool
RegExpObject::createShared(JSContext *cx, RegExpGuard *g)
{
    Rooted<RegExpObject*> self(cx, this);

    JS_ASSERT(!maybeShared());
    if (!cx->compartment()->regExps.get(cx, getSource(), getFlags(), g))
        return false;

    self->setShared(cx, **g);
    return true;
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

    RootedObject self(cx, this);

    /* The lastIndex property alone is writable but non-configurable. */
    if (!addDataProperty(cx, cx->names().lastIndex, LAST_INDEX_SLOT, JSPROP_PERMANENT))
        return NULL;

    /* Remaining instance properties are non-writable and non-configurable. */
    unsigned attrs = JSPROP_PERMANENT | JSPROP_READONLY;
    if (!self->addDataProperty(cx, cx->names().source, SOURCE_SLOT, attrs))
        return NULL;
    if (!self->addDataProperty(cx, cx->names().global, GLOBAL_FLAG_SLOT, attrs))
        return NULL;
    if (!self->addDataProperty(cx, cx->names().ignoreCase, IGNORE_CASE_FLAG_SLOT, attrs))
        return NULL;
    if (!self->addDataProperty(cx, cx->names().multiline, MULTILINE_FLAG_SLOT, attrs))
        return NULL;
    return self->addDataProperty(cx, cx->names().sticky, STICKY_FLAG_SLOT, attrs);
}

bool
RegExpObject::init(JSContext *cx, HandleAtom source, RegExpFlag flags)
{
    Rooted<RegExpObject *> self(cx, this);

    if (nativeEmpty()) {
        if (isDelegate()) {
            if (!assignInitialShape(cx))
                return false;
        } else {
            RootedShape shape(cx, assignInitialShape(cx));
            if (!shape)
                return false;
            RootedObject proto(cx, self->getProto());
            EmptyShape::insertInitialShape(cx, shape, proto);
        }
        JS_ASSERT(!self->nativeEmpty());
    }

    JS_ASSERT(self->nativeLookup(cx, NameToId(cx->names().lastIndex))->slot() ==
              LAST_INDEX_SLOT);
    JS_ASSERT(self->nativeLookup(cx, NameToId(cx->names().source))->slot() ==
              SOURCE_SLOT);
    JS_ASSERT(self->nativeLookup(cx, NameToId(cx->names().global))->slot() ==
              GLOBAL_FLAG_SLOT);
    JS_ASSERT(self->nativeLookup(cx, NameToId(cx->names().ignoreCase))->slot() ==
              IGNORE_CASE_FLAG_SLOT);
    JS_ASSERT(self->nativeLookup(cx, NameToId(cx->names().multiline))->slot() ==
              MULTILINE_FLAG_SLOT);
    JS_ASSERT(self->nativeLookup(cx, NameToId(cx->names().sticky))->slot() ==
              STICKY_FLAG_SLOT);

    /*
     * If this is a re-initialization with an existing RegExpShared, 'flags'
     * may not match getShared()->flags, so forget the RegExpShared.
     */
    self->JSObject::setPrivate(NULL);

    self->zeroLastIndex();
    self->setSource(source);
    self->setGlobal(flags & GlobalFlag);
    self->setIgnoreCase(flags & IgnoreCaseFlag);
    self->setMultiline(flags & MultilineFlag);
    self->setSticky(flags & StickyFlag);
    return true;
}

JSFlatString *
RegExpObject::toString(JSContext *cx) const
{
    JSAtom *src = getSource();
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

/* RegExpShared */

RegExpShared::RegExpShared(JSRuntime *rt, JSAtom *source, RegExpFlag flags)
  : source(source), flags(flags), parenCount(0),
#if ENABLE_YARR_JIT
    codeBlock(),
#endif
    bytecode(NULL), activeUseCount(0), gcNumberWhenUsed(rt->gcNumber)
{}

RegExpShared::~RegExpShared()
{
#if ENABLE_YARR_JIT
    codeBlock.release();
#endif
    if (bytecode)
        js_delete<BytecodePattern>(bytecode);
}

void
RegExpShared::reportYarrError(JSContext *cx, TokenStream *ts, ErrorCode error)
{
    switch (error) {
      case JSC::Yarr::NoError:
        JS_NOT_REACHED("Called reportYarrError with value for no error");
        return;
#define COMPILE_EMSG(__code, __msg)                                                              \
      case JSC::Yarr::__code:                                                                    \
        if (ts)                                                                                  \
            ts->reportError(__msg);                                                              \
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
RegExpShared::checkSyntax(JSContext *cx, TokenStream *tokenStream, JSLinearString *source)
{
    ErrorCode error = JSC::Yarr::checkSyntax(*source);
    if (error == JSC::Yarr::NoError)
        return true;

    reportYarrError(cx, tokenStream, error);
    return false;
}

bool
RegExpShared::compile(JSContext *cx, bool matchOnly)
{
    if (!sticky())
        return compile(cx, *source, matchOnly);

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

    JSAtom *fakeySource = sb.finishAtom();
    if (!fakeySource)
        return false;

    return compile(cx, *fakeySource, matchOnly);
}

bool
RegExpShared::compile(JSContext *cx, JSLinearString &pattern, bool matchOnly)
{
    /* Parse the pattern. */
    ErrorCode yarrError;
    YarrPattern yarrPattern(pattern, ignoreCase(), multiline(), &yarrError);
    if (yarrError) {
        reportYarrError(cx, NULL, yarrError);
        return false;
    }
    this->parenCount = yarrPattern.m_numSubpatterns;

#if ENABLE_YARR_JIT
    if (isJITRuntimeEnabled(cx) && !yarrPattern.m_containsBackreferences) {
        JSC::ExecutableAllocator *execAlloc = cx->runtime()->getExecAlloc(cx);
        if (!execAlloc)
            return false;

        JSGlobalData globalData(execAlloc);
        YarrJITCompileMode compileMode = matchOnly ? JSC::Yarr::MatchOnly
                                                   : JSC::Yarr::IncludeSubpatterns;

        jitCompile(yarrPattern, JSC::Yarr::Char16, &globalData, codeBlock, compileMode);

        /* Unset iff the Yarr JIT compilation was successful. */
        if (!codeBlock.isFallBack())
            return true;
    }
    codeBlock.setFallBack(true);
#endif

    WTF::BumpPointerAllocator *bumpAlloc = cx->runtime()->getBumpPointerAllocator(cx);
    if (!bumpAlloc) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    bytecode = byteCompile(yarrPattern, bumpAlloc).get();
    return true;
}

bool
RegExpShared::compileIfNecessary(JSContext *cx)
{
    if (hasCode() || hasBytecode())
        return true;
    return compile(cx, false);
}

bool
RegExpShared::compileMatchOnlyIfNecessary(JSContext *cx)
{
    if (hasMatchOnlyCode() || hasBytecode())
        return true;
    return compile(cx, true);
}

RegExpRunStatus
RegExpShared::execute(JSContext *cx, const jschar *chars, size_t length,
                      size_t *lastIndex, MatchPairs &matches)
{
    /* Compile the code at point-of-use. */
    if (!compileIfNecessary(cx))
        return RegExpRunStatus_Error;

    /* Ensure sufficient memory for output vector. */
    if (!matches.initArray(pairCount()))
        return RegExpRunStatus_Error;

    /*
     * |displacement| emulates sticky mode by matching from this offset
     * into the char buffer and subtracting the delta off at the end.
     */
    size_t origLength = length;
    size_t start = *lastIndex;
    size_t displacement = 0;

    if (sticky()) {
        displacement = start;
        chars += displacement;
        length -= displacement;
        start = 0;
    }

    unsigned *outputBuf = matches.rawBuf();
    unsigned result;

#if ENABLE_YARR_JIT
    if (codeBlock.isFallBack())
        result = JSC::Yarr::interpret(cx, bytecode, chars, length, start, outputBuf);
    else
        result = codeBlock.execute(chars, start, length, (int *)outputBuf).start;
#else
    result = JSC::Yarr::interpret(cx, bytecode, chars, length, start, outputBuf);
#endif

    if (result == JSC::Yarr::offsetNoMatch)
        return RegExpRunStatus_Success_NotFound;

    matches.displace(displacement);
    matches.checkAgainst(origLength);
    *lastIndex = matches[0].limit;
    return RegExpRunStatus_Success;
}

RegExpRunStatus
RegExpShared::executeMatchOnly(JSContext *cx, const jschar *chars, size_t length,
                               size_t *lastIndex, MatchPair &match)
{
    /* These chars may be inline in a string. See bug 846011. */
    SkipRoot skipChars(cx, &chars);

    /* Compile the code at point-of-use. */
    if (!compileMatchOnlyIfNecessary(cx))
        return RegExpRunStatus_Error;

#ifdef DEBUG
    const size_t origLength = length;
#endif
    size_t start = *lastIndex;
    size_t displacement = 0;

    if (sticky()) {
        displacement = start;
        chars += displacement;
        length -= displacement;
        start = 0;
    }

#if ENABLE_YARR_JIT
    if (!codeBlock.isFallBack()) {
        MatchResult result = codeBlock.execute(chars, start, length);
        if (!result)
            return RegExpRunStatus_Success_NotFound;

        match = MatchPair(result.start, result.end);
        match.displace(displacement);
        *lastIndex = match.limit;
        return RegExpRunStatus_Success;
    }
#endif

    /*
     * The JIT could not be used, so fall back to the Yarr interpreter.
     * Unfortunately, the interpreter does not have a MatchOnly mode, so a
     * temporary output vector must be provided.
     */
    JS_ASSERT(hasBytecode());
    ScopedMatchPairs matches(&cx->tempLifoAlloc());
    if (!matches.initArray(pairCount()))
        return RegExpRunStatus_Error;

    unsigned result =
        JSC::Yarr::interpret(cx, bytecode, chars, length, start, matches.rawBuf());

    if (result == JSC::Yarr::offsetNoMatch)
        return RegExpRunStatus_Success_NotFound;

    match = MatchPair(result, matches[0].limit);
    match.displace(displacement);

#ifdef DEBUG
    matches.displace(displacement);
    matches.checkAgainst(origLength);
#endif

    *lastIndex = match.limit;
    return RegExpRunStatus_Success;
}

/* RegExpCompartment */

RegExpCompartment::RegExpCompartment(JSRuntime *rt)
  : map_(rt), inUse_(rt)
{}

RegExpCompartment::~RegExpCompartment()
{
    JS_ASSERT(map_.empty());
    JS_ASSERT(inUse_.empty());
}

bool
RegExpCompartment::init(JSContext *cx)
{
    if (!map_.init(0) || !inUse_.init(0)) {
        if (cx)
            js_ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

/* See the comment on RegExpShared lifetime in RegExpObject.h. */
void
RegExpCompartment::sweep(JSRuntime *rt)
{
#ifdef DEBUG
    for (Map::Range r = map_.all(); !r.empty(); r.popFront())
        JS_ASSERT(inUse_.has(r.front().value));
#endif

    map_.clear();

    for (PendingSet::Enum e(inUse_); !e.empty(); e.popFront()) {
        RegExpShared *shared = e.front();
        if (shared->activeUseCount == 0 && shared->gcNumberWhenUsed < rt->gcStartNumber) {
            js_delete(shared);
            e.removeFront();
        }
    }
}

bool
RegExpCompartment::get(JSContext *cx, JSAtom *source, RegExpFlag flags, RegExpGuard *g)
{
    Key key(source, flags);
    Map::AddPtr p = map_.lookupForAdd(key);
    if (p) {
        g->init(*p->value);
        return true;
    }

    ScopedJSDeletePtr<RegExpShared> shared(cx->new_<RegExpShared>(cx->runtime(), source, flags));
    if (!shared)
        return false;

    /* Add to RegExpShared sharing hashmap. */
    if (!map_.add(p, key, shared)) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    /* Add to list of all RegExpShared objects in this RegExpCompartment. */
    if (!inUse_.put(shared)) {
        map_.remove(key);
        js_ReportOutOfMemory(cx);
        return false;
    }

    /* Since error deletes |shared|, only guard |shared| on success. */
    g->init(*shared.forget());
    return true;
}

bool
RegExpCompartment::get(JSContext *cx, HandleAtom atom, JSString *opt, RegExpGuard *g)
{
    RegExpFlag flags = RegExpFlag(0);
    if (opt && !ParseRegExpFlags(cx, opt, &flags))
        return false;

    return get(cx, atom, flags, g);
}

size_t
RegExpCompartment::sizeOfExcludingThis(JSMallocSizeOfFun mallocSizeOf)
{
    size_t n = 0;
    n += map_.sizeOfExcludingThis(mallocSizeOf);
    n += inUse_.sizeOfExcludingThis(mallocSizeOf);
    return n;
}

/* Functions */

JSObject *
js::CloneRegExpObject(JSContext *cx, JSObject *obj_, JSObject *proto_)
{
    RegExpObjectBuilder builder(cx);
    Rooted<RegExpObject*> regex(cx, &obj_->asRegExp());
    Rooted<RegExpObject*> proto(cx, &proto_->asRegExp());
    return builder.clone(regex, proto);
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

template<XDRMode mode>
bool
js::XDRScriptRegExpObject(XDRState<mode> *xdr, HeapPtrObject *objp)
{
    /* NB: Keep this in sync with CloneScriptRegExpObject. */

    RootedAtom source(xdr->cx());
    uint32_t flagsword = 0;

    if (mode == XDR_ENCODE) {
        JS_ASSERT(objp);
        RegExpObject &reobj = (*objp)->asRegExp();
        source = reobj.getSource();
        flagsword = reobj.getFlags();
    }
    if (!XDRAtom(xdr, &source) || !xdr->codeUint32(&flagsword))
        return false;
    if (mode == XDR_DECODE) {
        RegExpFlag flags = RegExpFlag(flagsword);
        RegExpObject *reobj = RegExpObject::createNoStatics(xdr->cx(), source, flags, NULL);
        if (!reobj)
            return false;

        objp->init(reobj);
    }
    return true;
}

template bool
js::XDRScriptRegExpObject(XDRState<XDR_ENCODE> *xdr, HeapPtrObject *objp);

template bool
js::XDRScriptRegExpObject(XDRState<XDR_DECODE> *xdr, HeapPtrObject *objp);

JSObject *
js::CloneScriptRegExpObject(JSContext *cx, RegExpObject &reobj)
{
    /* NB: Keep this in sync with XDRScriptRegExpObject. */

    RootedAtom source(cx, reobj.getSource());
    return RegExpObject::createNoStatics(cx, source, reobj.getFlags(), NULL);
}
