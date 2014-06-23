/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/RegExpObject.h"

#include "mozilla/MemoryReporting.h"

#include "jsstr.h"

#include "frontend/TokenStream.h"
#include "irregexp/RegExpParser.h"
#include "vm/MatchPairs.h"
#include "vm/RegExpStatics.h"
#include "vm/StringBuffer.h"
#include "vm/TraceLogging.h"
#include "vm/Xdr.h"

#include "jsobjinlines.h"

#include "vm/Shape-inl.h"

using namespace js;

using mozilla::DebugOnly;
using mozilla::Maybe;
using js::frontend::TokenStream;

using JS::AutoCheckCannotGC;

JS_STATIC_ASSERT(IgnoreCaseFlag == JSREG_FOLD);
JS_STATIC_ASSERT(GlobalFlag == JSREG_GLOB);
JS_STATIC_ASSERT(MultilineFlag == JSREG_MULTILINE);
JS_STATIC_ASSERT(StickyFlag == JSREG_STICKY);

/* RegExpObjectBuilder */

RegExpObjectBuilder::RegExpObjectBuilder(ExclusiveContext *cx, RegExpObject *reobj)
  : cx(cx), reobj_(cx, reobj)
{}

bool
RegExpObjectBuilder::getOrCreate()
{
    if (reobj_)
        return true;

    // Note: RegExp objects are always allocated in the tenured heap. This is
    // not strictly required, but simplifies embedding them in jitcode.
    JSObject *obj = NewBuiltinClassInstance(cx, &RegExpObject::class_, TenuredObject);
    if (!obj)
        return false;
    obj->initPrivate(nullptr);

    reobj_ = &obj->as<RegExpObject>();
    return true;
}

bool
RegExpObjectBuilder::getOrCreateClone(HandleTypeObject type)
{
    JS_ASSERT(!reobj_);
    JS_ASSERT(type->clasp() == &RegExpObject::class_);

    JSObject *parent = type->proto().toObject()->getParent();

    // Note: RegExp objects are always allocated in the tenured heap. This is
    // not strictly required, but simplifies embedding them in jitcode.
    JSObject *clone = NewObjectWithType(cx->asJSContext(), type, parent, TenuredObject);
    if (!clone)
        return false;
    clone->initPrivate(nullptr);

    reobj_ = &clone->as<RegExpObject>();
    return true;
}

RegExpObject *
RegExpObjectBuilder::build(HandleAtom source, RegExpShared &shared)
{
    if (!getOrCreate())
        return nullptr;

    if (!reobj_->init(cx, source, shared.getFlags()))
        return nullptr;

    reobj_->setShared(shared);
    return reobj_;
}

RegExpObject *
RegExpObjectBuilder::build(HandleAtom source, RegExpFlag flags)
{
    if (!getOrCreate())
        return nullptr;

    return reobj_->init(cx, source, flags) ? reobj_.get() : nullptr;
}

RegExpObject *
RegExpObjectBuilder::clone(Handle<RegExpObject *> other)
{
    RootedTypeObject type(cx, other->type());
    if (!getOrCreateClone(type))
        return nullptr;

    /*
     * Check that the RegExpShared for the original is okay to use in
     * the clone -- if the |RegExpStatics| provides more flags we'll
     * need a different |RegExpShared|.
     */
    RegExpStatics *res = other->getProto()->getParent()->as<GlobalObject>().getRegExpStatics(cx);
    if (!res)
        return nullptr;

    RegExpFlag origFlags = other->getFlags();
    RegExpFlag staticsFlags = res->getFlags();
    if ((origFlags & staticsFlags) != staticsFlags) {
        RegExpFlag newFlags = RegExpFlag(origFlags | staticsFlags);
        Rooted<JSAtom *> source(cx, other->getSource());
        return build(source, newFlags);
    }

    RegExpGuard g(cx);
    if (!other->getShared(cx->asJSContext(), &g))
        return nullptr;

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

static inline void
MaybeTraceRegExpShared(JSContext *cx, RegExpShared *shared)
{
    Zone *zone = cx->zone();
    if (zone->needsBarrier())
        shared->trace(zone->barrierTracer());
}

bool
RegExpObject::getShared(JSContext *cx, RegExpGuard *g)
{
    if (RegExpShared *shared = maybeShared()) {
        // Fetching a RegExpShared from an object requires a read
        // barrier, as the shared pointer might be weak.
        MaybeTraceRegExpShared(cx, shared);

        g->init(*shared);
        return true;
    }

    return createShared(cx, g);
}

/* static */ void
RegExpObject::trace(JSTracer *trc, JSObject *obj)
{
    RegExpShared *shared = obj->as<RegExpObject>().maybeShared();
    if (!shared)
        return;

    // When tracing through the object normally, we have the option of
    // unlinking the object from its RegExpShared so that the RegExpShared may
    // be collected. To detect this we need to test all the following
    // conditions, since:
    //   1. During TraceRuntime, isHeapBusy() is true, but the tracer might not
    //      be a marking tracer.
    //   2. When a write barrier executes, IS_GC_MARKING_TRACER is true, but
    //      isHeapBusy() will be false.
    if (trc->runtime()->isHeapBusy() &&
        IS_GC_MARKING_TRACER(trc) &&
        !obj->tenuredZone()->isPreservingCode())
    {
        obj->setPrivate(nullptr);
    } else {
        shared->trace(trc);
    }
}

const Class RegExpObject::class_ = {
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
    nullptr,                 /* finalize */
    nullptr,                 /* call */
    nullptr,                 /* hasInstance */
    nullptr,                 /* construct */
    RegExpObject::trace
};

RegExpObject *
RegExpObject::create(ExclusiveContext *cx, RegExpStatics *res, const jschar *chars, size_t length,
                     RegExpFlag flags, TokenStream *tokenStream, LifoAlloc &alloc)
{
    RegExpFlag staticsFlags = res->getFlags();
    return createNoStatics(cx, chars, length, RegExpFlag(flags | staticsFlags), tokenStream, alloc);
}

RegExpObject *
RegExpObject::createNoStatics(ExclusiveContext *cx, const jschar *chars, size_t length, RegExpFlag flags,
                              TokenStream *tokenStream, LifoAlloc &alloc)
{
    RootedAtom source(cx, AtomizeChars(cx, chars, length));
    if (!source)
        return nullptr;

    return createNoStatics(cx, source, flags, tokenStream, alloc);
}

RegExpObject *
RegExpObject::createNoStatics(ExclusiveContext *cx, HandleAtom source, RegExpFlag flags,
                              TokenStream *tokenStream, LifoAlloc &alloc)
{
    Maybe<CompileOptions> dummyOptions;
    Maybe<TokenStream> dummyTokenStream;
    if (!tokenStream) {
        dummyOptions.construct(cx->asJSContext());
        dummyTokenStream.construct(cx, dummyOptions.ref(),
                                   (const jschar *) nullptr, 0,
                                   (frontend::StrictModeGetter *) nullptr);
        tokenStream = dummyTokenStream.addr();
    }

    if (!irregexp::ParsePatternSyntax(*tokenStream, alloc, source))
        return nullptr;

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

    self->setShared(**g);
    return true;
}

Shape *
RegExpObject::assignInitialShape(ExclusiveContext *cx, Handle<RegExpObject*> self)
{
    JS_ASSERT(self->nativeEmpty());

    JS_STATIC_ASSERT(LAST_INDEX_SLOT == 0);
    JS_STATIC_ASSERT(SOURCE_SLOT == LAST_INDEX_SLOT + 1);
    JS_STATIC_ASSERT(GLOBAL_FLAG_SLOT == SOURCE_SLOT + 1);
    JS_STATIC_ASSERT(IGNORE_CASE_FLAG_SLOT == GLOBAL_FLAG_SLOT + 1);
    JS_STATIC_ASSERT(MULTILINE_FLAG_SLOT == IGNORE_CASE_FLAG_SLOT + 1);
    JS_STATIC_ASSERT(STICKY_FLAG_SLOT == MULTILINE_FLAG_SLOT + 1);

    /* The lastIndex property alone is writable but non-configurable. */
    if (!self->addDataProperty(cx, cx->names().lastIndex, LAST_INDEX_SLOT, JSPROP_PERMANENT))
        return nullptr;

    /* Remaining instance properties are non-writable and non-configurable. */
    unsigned attrs = JSPROP_PERMANENT | JSPROP_READONLY;
    if (!self->addDataProperty(cx, cx->names().source, SOURCE_SLOT, attrs))
        return nullptr;
    if (!self->addDataProperty(cx, cx->names().global, GLOBAL_FLAG_SLOT, attrs))
        return nullptr;
    if (!self->addDataProperty(cx, cx->names().ignoreCase, IGNORE_CASE_FLAG_SLOT, attrs))
        return nullptr;
    if (!self->addDataProperty(cx, cx->names().multiline, MULTILINE_FLAG_SLOT, attrs))
        return nullptr;
    return self->addDataProperty(cx, cx->names().sticky, STICKY_FLAG_SLOT, attrs);
}

bool
RegExpObject::init(ExclusiveContext *cx, HandleAtom source, RegExpFlag flags)
{
    Rooted<RegExpObject *> self(cx, this);

    if (!EmptyShape::ensureInitialCustomShape<RegExpObject>(cx, self))
        return false;

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
    self->JSObject::setPrivate(nullptr);

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
            return nullptr;
        sb.infallibleAppend('/');
        if (!sb.append(src))
            return nullptr;
        sb.infallibleAppend('/');
    } else {
        if (!sb.append("/(?:)/"))
            return nullptr;
    }
    if (global() && !sb.append('g'))
        return nullptr;
    if (ignoreCase() && !sb.append('i'))
        return nullptr;
    if (multiline() && !sb.append('m'))
        return nullptr;
    if (sticky() && !sb.append('y'))
        return nullptr;

    return sb.finishString();
}

/* RegExpShared */

RegExpShared::RegExpShared(JSAtom *source, RegExpFlag flags)
  : source(source), flags(flags), parenCount(0), canStringMatch(false), marked_(false),
    byteCodeLatin1(nullptr), byteCodeTwoByte(nullptr)
{}

RegExpShared::~RegExpShared()
{
    js_free(byteCodeLatin1);
    js_free(byteCodeTwoByte);

    for (size_t i = 0; i < tables.length(); i++)
        js_delete(tables[i]);
}

void
RegExpShared::trace(JSTracer *trc)
{
    if (IS_GC_MARKING_TRACER(trc))
        marked_ = true;

    if (source)
        MarkString(trc, &source, "RegExpShared source");

#ifdef JS_ION
    if (jitCodeLatin1)
        MarkJitCode(trc, &jitCodeLatin1, "RegExpShared code Latin1");
    if (jitCodeTwoByte)
        MarkJitCode(trc, &jitCodeTwoByte, "RegExpShared code TwoByte");
#endif
}

bool
RegExpShared::compile(JSContext *cx, HandleLinearString input)
{
    TraceLogger *logger = TraceLoggerForMainThread(cx->runtime());
    AutoTraceLog logCompile(logger, TraceLogger::IrregexpCompile);

    if (!sticky()) {
        RootedAtom pattern(cx, source);
        return compile(cx, pattern, input);
    }

    /*
     * The sticky case we implement hackily by prepending a caret onto the front
     * and relying on |::execute| to pseudo-slice the string when it sees a sticky regexp.
     */
    static const char prefix[] = {'^', '(', '?', ':'};
    static const char postfix[] = {')'};

    using mozilla::ArrayLength;
    StringBuffer sb(cx);
    if (!sb.reserve(ArrayLength(prefix) + source->length() + ArrayLength(postfix)))
        return false;
    sb.infallibleAppend(prefix, ArrayLength(prefix));
    if (!sb.append(source))
        return false;
    sb.infallibleAppend(postfix, ArrayLength(postfix));

    RootedAtom fakeySource(cx, sb.finishAtom());
    if (!fakeySource)
        return false;

    return compile(cx, fakeySource, input);
}

bool
RegExpShared::compile(JSContext *cx, HandleAtom pattern, HandleLinearString input)
{
    if (!ignoreCase() && !StringHasRegExpMetaChars(pattern)) {
        canStringMatch = true;
        parenCount = 0;
        return true;
    }

    CompileOptions options(cx);
    TokenStream dummyTokenStream(cx, options, nullptr, 0, nullptr);

    LifoAllocScope scope(&cx->tempLifoAlloc());

    /* Parse the pattern. */
    irregexp::RegExpCompileData data;
    if (!irregexp::ParsePattern(dummyTokenStream, cx->tempLifoAlloc(), pattern, multiline(), &data))
        return false;

    this->parenCount = data.capture_count;

    irregexp::RegExpCode code = irregexp::CompilePattern(cx, this, &data, input,
                                                         false /* global() */,
                                                         ignoreCase(),
                                                         input->hasLatin1Chars());
    if (code.empty())
        return false;

#ifdef JS_ION
    JS_ASSERT(!code.jitCode || !code.byteCode);
    if (input->hasLatin1Chars())
        jitCodeLatin1 = code.jitCode;
    else
        jitCodeTwoByte = code.jitCode;
#endif

    if (input->hasLatin1Chars())
        byteCodeLatin1 = code.byteCode;
    else
        byteCodeTwoByte = code.byteCode;

    return true;
}

bool
RegExpShared::compileIfNecessary(JSContext *cx, HandleLinearString input)
{
    if (isCompiled(input->hasLatin1Chars()) || canStringMatch)
        return true;
    return compile(cx, input);
}

RegExpRunStatus
RegExpShared::execute(JSContext *cx, HandleLinearString input, size_t *lastIndex,
                      MatchPairs &matches)
{
    TraceLogger *logger = TraceLoggerForMainThread(cx->runtime());

    /* Compile the code at point-of-use. */
    if (!compileIfNecessary(cx, input))
        return RegExpRunStatus_Error;

    /* Ensure sufficient memory for output vector. */
    if (!matches.initArray(pairCount()))
        return RegExpRunStatus_Error;

    /*
     * |displacement| emulates sticky mode by matching from this offset
     * into the char buffer and subtracting the delta off at the end.
     */
    size_t charsOffset = 0;
    size_t length = input->length();
    size_t origLength = length;
    size_t start = *lastIndex;
    size_t displacement = 0;

    if (sticky()) {
        displacement = start;
        charsOffset += displacement;
        length -= displacement;
        start = 0;
    }

    // Reset the Irregexp backtrack stack if it grows during execution.
    irregexp::RegExpStackScope stackScope(cx->runtime());

    if (canStringMatch) {
        int res = StringFindPattern(input, source, start + charsOffset);
        if (res == -1)
            return RegExpRunStatus_Success_NotFound;

        matches[0].start = res;
        matches[0].limit = res + source->length();

        matches.checkAgainst(origLength);
        *lastIndex = matches[0].limit;
        return RegExpRunStatus_Success;
    }

    if (uint8_t *byteCode = maybeByteCode(input->hasLatin1Chars())) {
        AutoTraceLog logInterpreter(logger, TraceLogger::IrregexpExecute);

        AutoStableStringChars inputChars(cx);
        if (!inputChars.init(cx, input))
            return RegExpRunStatus_Error;

        RegExpRunStatus result;
        if (inputChars.isLatin1()) {
            const Latin1Char *chars = inputChars.latin1Range().start().get() + charsOffset;
            result = irregexp::InterpretCode(cx, byteCode, chars, start, length, &matches);
        } else {
            const jschar *chars = inputChars.twoByteRange().start().get() + charsOffset;
            result = irregexp::InterpretCode(cx, byteCode, chars, start, length, &matches);
        }

        if (result == RegExpRunStatus_Success) {
            matches.displace(displacement);
            matches.checkAgainst(origLength);
            *lastIndex = matches[0].limit;
        }
        return result;
    }

#ifdef JS_ION
    while (true) {
        RegExpRunStatus result;
        {
            AutoTraceLog logJIT(logger, TraceLogger::IrregexpExecute);
            AutoCheckCannotGC nogc;
            if (input->hasLatin1Chars()) {
                const Latin1Char *chars = input->latin1Chars(nogc) + charsOffset;
                result = irregexp::ExecuteCode(cx, jitCodeLatin1, chars, start, length, &matches);
            } else {
                const jschar *chars = input->twoByteChars(nogc) + charsOffset;
                result = irregexp::ExecuteCode(cx, jitCodeTwoByte, chars, start, length, &matches);
            }
        }

        if (result == RegExpRunStatus_Error) {
            // The RegExp engine might exit with an exception if an interrupt
            // was requested. Check this case and retry until a clean result is
            // obtained.
            bool interrupted;
            {
                JSRuntime::AutoLockForInterrupt lock(cx->runtime());
                interrupted = cx->runtime()->interrupt;
            }

            if (interrupted) {
                if (!InvokeInterruptCallback(cx))
                    return RegExpRunStatus_Error;
                continue;
            }

            js_ReportOverRecursed(cx);
            return RegExpRunStatus_Error;
        }

        if (result == RegExpRunStatus_Success_NotFound)
            return RegExpRunStatus_Success_NotFound;

        JS_ASSERT(result == RegExpRunStatus_Success);
        break;
    }
#else // JS_ION
    MOZ_CRASH();
#endif // JS_ION

    matches.displace(displacement);
    matches.checkAgainst(origLength);
    *lastIndex = matches[0].limit;
    return RegExpRunStatus_Success;
}

size_t
RegExpShared::sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    size_t n = mallocSizeOf(this);

    if (byteCodeLatin1)
        n += mallocSizeOf(byteCodeLatin1);

    if (byteCodeTwoByte)
        n += mallocSizeOf(byteCodeTwoByte);

    n += tables.sizeOfExcludingThis(mallocSizeOf);
    for (size_t i = 0; i < tables.length(); i++)
        n += mallocSizeOf(tables[i]);

    return n;
}

/* RegExpCompartment */

RegExpCompartment::RegExpCompartment(JSRuntime *rt)
  : set_(rt), matchResultTemplateObject_(nullptr)
{}

RegExpCompartment::~RegExpCompartment()
{
    // Because of stray mark bits being set (see RegExpCompartment::sweep)
    // there might still be RegExpShared instances which haven't been deleted.
    if (set_.initialized()) {
        for (Set::Enum e(set_); !e.empty(); e.popFront()) {
            RegExpShared *shared = e.front();
            js_delete(shared);
        }
    }
}

JSObject *
RegExpCompartment::createMatchResultTemplateObject(JSContext *cx)
{
    JS_ASSERT(!matchResultTemplateObject_);

    /* Create template array object */
    RootedObject templateObject(cx, NewDenseUnallocatedArray(cx, 0, nullptr, TenuredObject));
    if (!templateObject)
        return matchResultTemplateObject_; // = nullptr

    /* Set dummy index property */
    RootedValue index(cx, Int32Value(0));
    if (!baseops::DefineProperty(cx, templateObject, cx->names().index, index,
                                 JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE))
        return matchResultTemplateObject_; // = nullptr

    /* Set dummy input property */
    RootedValue inputVal(cx, StringValue(cx->runtime()->emptyString));
    if (!baseops::DefineProperty(cx, templateObject, cx->names().input, inputVal,
                                 JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE))
        return matchResultTemplateObject_; // = nullptr

    // Make sure that the properties are in the right slots.
    DebugOnly<Shape *> shape = templateObject->lastProperty();
    JS_ASSERT(shape->previous()->slot() == 0 &&
              shape->previous()->propidRef() == NameToId(cx->names().index));
    JS_ASSERT(shape->slot() == 1 &&
              shape->propidRef() == NameToId(cx->names().input));

    matchResultTemplateObject_.set(templateObject);

    return matchResultTemplateObject_;
}

bool
RegExpCompartment::init(JSContext *cx)
{
    if (!set_.init(0)) {
        if (cx)
            js_ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

void
RegExpCompartment::sweep(JSRuntime *rt)
{
    for (Set::Enum e(set_); !e.empty(); e.popFront()) {
        RegExpShared *shared = e.front();

        // Sometimes RegExpShared instances are marked without the
        // compartment being subsequently cleared. This can happen if a GC is
        // restarted while in progress (i.e. performing a full GC in the
        // middle of an incremental GC) or if a RegExpShared referenced via the
        // stack is traced but is not in a zone being collected.
        //
        // Because of this we only treat the marked_ bit as a hint, and destroy
        // the RegExpShared if it was accidentally marked earlier but wasn't
        // marked by the current trace.
        bool keep = shared->marked() && !IsStringAboutToBeFinalized(shared->source.unsafeGet());
#ifdef JS_ION
        if (keep && shared->jitCodeLatin1)
            keep = !IsJitCodeAboutToBeFinalized(shared->jitCodeLatin1.unsafeGet());
        if (keep && shared->jitCodeTwoByte)
            keep = !IsJitCodeAboutToBeFinalized(shared->jitCodeTwoByte.unsafeGet());
#endif
        if (keep) {
            shared->clearMarked();
        } else {
            js_delete(shared);
            e.removeFront();
        }
    }

    if (matchResultTemplateObject_ &&
        IsObjectAboutToBeFinalized(matchResultTemplateObject_.unsafeGet()))
    {
        matchResultTemplateObject_.set(nullptr);
    }
}

bool
RegExpCompartment::get(JSContext *cx, JSAtom *source, RegExpFlag flags, RegExpGuard *g)
{
    Key key(source, flags);
    Set::AddPtr p = set_.lookupForAdd(key);
    if (p) {
        // Trigger a read barrier on existing RegExpShared instances fetched
        // from the table (which only holds weak references).
        MaybeTraceRegExpShared(cx, *p);

        g->init(**p);
        return true;
    }

    ScopedJSDeletePtr<RegExpShared> shared(cx->new_<RegExpShared>(source, flags));
    if (!shared)
        return false;

    if (!set_.add(p, shared)) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    // Trace RegExpShared instances created during an incremental GC.
    MaybeTraceRegExpShared(cx, shared);

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
RegExpCompartment::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    size_t n = 0;
    n += set_.sizeOfExcludingThis(mallocSizeOf);
    for (Set::Enum e(set_); !e.empty(); e.popFront()) {
        RegExpShared *shared = e.front();
        n += shared->sizeOfIncludingThis(mallocSizeOf);
    }
    return n;
}

/* Functions */

JSObject *
js::CloneRegExpObject(JSContext *cx, JSObject *obj_)
{
    RegExpObjectBuilder builder(cx);
    Rooted<RegExpObject*> regex(cx, &obj_->as<RegExpObject>());
    JSObject *res = builder.clone(regex);
    JS_ASSERT_IF(res, res->type() == regex->type());
    return res;
}

static bool
HandleRegExpFlag(RegExpFlag flag, RegExpFlag *flags)
{
    if (*flags & flag)
        return false;
    *flags = RegExpFlag(*flags | flag);
    return true;
}

template <typename CharT>
static size_t
ParseRegExpFlags(const CharT *chars, size_t length, RegExpFlag *flagsOut, jschar *lastParsedOut)
{
    *flagsOut = RegExpFlag(0);

    for (size_t i = 0; i < length; i++) {
        *lastParsedOut = chars[i];
        switch (chars[i]) {
          case 'i':
            if (!HandleRegExpFlag(IgnoreCaseFlag, flagsOut))
                return false;
            break;
          case 'g':
            if (!HandleRegExpFlag(GlobalFlag, flagsOut))
                return false;
            break;
          case 'm':
            if (!HandleRegExpFlag(MultilineFlag, flagsOut))
                return false;
            break;
          case 'y':
            if (!HandleRegExpFlag(StickyFlag, flagsOut))
                return false;
            break;
          default:
            return false;
        }
    }

    return true;
}

bool
js::ParseRegExpFlags(JSContext *cx, JSString *flagStr, RegExpFlag *flagsOut)
{
    JSLinearString *linear = flagStr->ensureLinear(cx);
    if (!linear)
        return false;

    size_t len = linear->length();

    bool ok;
    jschar lastParsed;
    if (linear->hasLatin1Chars()) {
        AutoCheckCannotGC nogc;
        ok = ::ParseRegExpFlags(linear->latin1Chars(nogc), len, flagsOut, &lastParsed);
    } else {
        AutoCheckCannotGC nogc;
        ok = ::ParseRegExpFlags(linear->twoByteChars(nogc), len, flagsOut, &lastParsed);
    }

    if (!ok) {
        char charBuf[2];
        charBuf[0] = char(lastParsed);
        charBuf[1] = '\0';
        JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, nullptr,
                                     JSMSG_BAD_REGEXP_FLAG, charBuf);
        return false;
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
        RegExpObject &reobj = (*objp)->as<RegExpObject>();
        source = reobj.getSource();
        flagsword = reobj.getFlags();
    }
    if (!XDRAtom(xdr, &source) || !xdr->codeUint32(&flagsword))
        return false;
    if (mode == XDR_DECODE) {
        RegExpFlag flags = RegExpFlag(flagsword);
        RegExpObject *reobj = RegExpObject::createNoStatics(xdr->cx(), source, flags, nullptr,
                                                            xdr->cx()->tempLifoAlloc());
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
    return RegExpObject::createNoStatics(cx, source, reobj.getFlags(), nullptr, cx->tempLifoAlloc());
}
