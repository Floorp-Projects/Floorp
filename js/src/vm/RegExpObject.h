/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_RegExpObject_h
#define vm_RegExpObject_h

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"

#include "jscntxt.h"
#include "jsproxy.h"

#include "gc/Marking.h"
#include "gc/Zone.h"
#include "vm/Shape.h"
#if ENABLE_YARR_JIT
#include "yarr/YarrJIT.h"
#else
#include "yarr/YarrInterpreter.h"
#endif

/*
 * JavaScript Regular Expressions
 *
 * There are several engine concepts associated with a single logical regexp:
 *
 *   RegExpObject - The JS-visible object whose .[[Class]] equals "RegExp"
 *
 *   RegExpShared - The compiled representation of the regexp.
 *
 *   RegExpCompartment - Owns all RegExpShared instances in a compartment.
 *
 * To save memory, a RegExpShared is not created for a RegExpObject until it is
 * needed for execution. When a RegExpShared needs to be created, it is looked
 * up in a per-compartment table to allow reuse between objects. Lastly, on
 * GC, every RegExpShared (that is not active on the callstack) is discarded.
 * Because of the last point, any code using a RegExpShared (viz., by executing
 * a regexp) must indicate the RegExpShared is active via RegExpGuard.
 */
namespace js {

class MatchConduit;
class MatchPair;
class MatchPairs;
class RegExpShared;

namespace frontend { class TokenStream; }

enum RegExpFlag
{
    IgnoreCaseFlag  = 0x01,
    GlobalFlag      = 0x02,
    MultilineFlag   = 0x04,
    StickyFlag      = 0x08,

    NoFlags         = 0x00,
    AllFlags        = 0x0f
};

enum RegExpRunStatus
{
    RegExpRunStatus_Error,
    RegExpRunStatus_Success,
    RegExpRunStatus_Success_NotFound
};

class RegExpObjectBuilder
{
    ExclusiveContext *cx;
    Rooted<RegExpObject*> reobj_;

    bool getOrCreate();
    bool getOrCreateClone(RegExpObject *proto);

  public:
    RegExpObjectBuilder(ExclusiveContext *cx, RegExpObject *reobj = nullptr);

    RegExpObject *reobj() { return reobj_; }

    RegExpObject *build(HandleAtom source, RegExpFlag flags);
    RegExpObject *build(HandleAtom source, RegExpShared &shared);

    /* Perform a VM-internal clone. */
    RegExpObject *clone(Handle<RegExpObject*> other, Handle<RegExpObject*> proto);
};

JSObject *
CloneRegExpObject(JSContext *cx, JSObject *obj, JSObject *proto);

/*
 * A RegExpShared is the compiled representation of a regexp. A RegExpShared is
 * potentially pointed to by multiple RegExpObjects. Additionally, C++ code may
 * have pointers to RegExpShareds on the stack. The RegExpShareds are kept in a
 * cache so that they can be reused when compiling the same regex string.
 *
 * During a GC, the trace hook for RegExpObject clears any pointers to
 * RegExpShareds so that there will be no dangling pointers when they are
 * deleted. However, some RegExpShareds are not deleted:
 *
 *   1. Any RegExpShared with pointers from the C++ stack is not deleted.
 *   2. Any RegExpShared which has been embedded into jitcode is not deleted.
 *      This rarely comes into play, as jitcode is usually purged before the
 *      RegExpShared are sweeped.
 *   3. Any RegExpShared that was installed in a RegExpObject during an
 *      incremental GC is not deleted. This is because the RegExpObject may have
 *      been traced through before the new RegExpShared was installed, in which
 *      case deleting the RegExpShared would turn the RegExpObject's reference
 *      into a dangling pointer
 *
 * The activeUseCount and gcNumberWhenUsed fields are used to track these
 * conditions.
 *
 * There are two tables used to track RegExpShareds. map_ implements the cache
 * and is cleared on every GC. inUse_ logically owns all RegExpShareds in the
 * compartment and attempts to delete all RegExpShareds that aren't kept alive
 * by the above conditions on every GC sweep phase. It is necessary to use two
 * separate tables since map_ *must* be fully cleared on each GC since the Key
 * points to a JSAtom that can become garbage.
 */
class RegExpShared
{
    friend class RegExpCompartment;
    friend class RegExpStatics;
    friend class RegExpGuard;

    typedef frontend::TokenStream TokenStream;
    typedef JSC::Yarr::BytecodePattern BytecodePattern;
    typedef JSC::Yarr::ErrorCode ErrorCode;
    typedef JSC::Yarr::YarrPattern YarrPattern;
#if ENABLE_YARR_JIT
    typedef JSC::Yarr::JSGlobalData JSGlobalData;
    typedef JSC::Yarr::YarrCodeBlock YarrCodeBlock;
    typedef JSC::Yarr::YarrJITCompileMode YarrJITCompileMode;
#endif

    /*
     * Source to the RegExp, for lazy compilation.
     * The source must be rooted while activeUseCount is non-zero
     * via RegExpGuard or explicit calls to trace().
     */
    JSAtom *           source;

    RegExpFlag         flags;
    unsigned           parenCount;

#if ENABLE_YARR_JIT
    /* Note: Native code is valid only if |codeBlock.isFallBack() == false|. */
    YarrCodeBlock   codeBlock;
#endif
    BytecodePattern *bytecode;

    /* Lifetime-preserving variables: see class-level comment above. */
    size_t             activeUseCount;
    uint64_t           gcNumberWhenUsed;

    /* Internal functions. */
    bool compile(JSContext *cx, bool matchOnly);
    bool compile(JSContext *cx, JSLinearString &pattern, bool matchOnly);

    bool compileIfNecessary(JSContext *cx);
    bool compileMatchOnlyIfNecessary(JSContext *cx);

  public:
    RegExpShared(JSAtom *source, RegExpFlag flags, uint64_t gcNumber);
    ~RegExpShared();

    /* Explicit trace function for use by the RegExpStatics and JITs. */
    void trace(JSTracer *trc) {
        MarkStringUnbarriered(trc, &source, "regexpshared source");
    }

    /* Static functions to expose some Yarr logic. */

    // This function should be deleted once bad Android platforms phase out. See bug 604774.
    static bool isJITRuntimeEnabled(JSContext *cx) {
        #if ENABLE_YARR_JIT
        # if defined(ANDROID)
            return !cx->jitIsBroken;
        # else
            return true;
        # endif
        #else
            return false;
        #endif
    }
    static void reportYarrError(ExclusiveContext *cx, TokenStream *ts, ErrorCode error);
    static bool checkSyntax(ExclusiveContext *cx, TokenStream *tokenStream, JSLinearString *source);

    /* Called when a RegExpShared is installed into a RegExpObject. */
    void prepareForUse(ExclusiveContext *cx) {
        gcNumberWhenUsed = cx->zone()->gcNumber();
    }

    /* Primary interface: run this regular expression on the given string. */
    RegExpRunStatus execute(JSContext *cx, const jschar *chars, size_t length,
                            size_t *lastIndex, MatchPairs &matches);

    /* Run the regular expression without collecting matches, for test(). */
    RegExpRunStatus executeMatchOnly(JSContext *cx, const jschar *chars, size_t length,
                                     size_t *lastIndex, MatchPair &match);

    /* Accessors */

    size_t getParenCount() const        { JS_ASSERT(isCompiled()); return parenCount; }
    void incRef()                       { activeUseCount++; }
    void decRef()                       { JS_ASSERT(activeUseCount > 0); activeUseCount--; }

    /* Accounts for the "0" (whole match) pair. */
    size_t pairCount() const            { return getParenCount() + 1; }

    RegExpFlag getFlags() const         { return flags; }
    bool ignoreCase() const             { return flags & IgnoreCaseFlag; }
    bool global() const                 { return flags & GlobalFlag; }
    bool multiline() const              { return flags & MultilineFlag; }
    bool sticky() const                 { return flags & StickyFlag; }

#ifdef ENABLE_YARR_JIT
    bool hasCode() const                { return codeBlock.has16BitCode(); }
    bool hasMatchOnlyCode() const       { return codeBlock.has16BitCodeMatchOnly(); }
#else
    bool hasCode() const                { return false; }
    bool hasMatchOnlyCode() const       { return false; }
#endif
    bool hasBytecode() const            { return bytecode != nullptr; }
    bool isCompiled() const             { return hasBytecode() || hasCode() || hasMatchOnlyCode(); }
};

/*
 * Extend the lifetime of a given RegExpShared to at least the lifetime of
 * the guard object. See Regular Expression comment at the top.
 */
class RegExpGuard
{
    RegExpShared *re_;

    /*
     * Prevent the RegExp source from being collected:
     * because RegExpShared objects compile at execution time, the source
     * must remain rooted for the active lifetime of the RegExpShared.
     */
    RootedAtom source_;

    RegExpGuard(const RegExpGuard &) MOZ_DELETE;
    void operator=(const RegExpGuard &) MOZ_DELETE;

  public:
    RegExpGuard(ExclusiveContext *cx)
      : re_(nullptr), source_(cx)
    {}

    RegExpGuard(ExclusiveContext *cx, RegExpShared &re)
      : re_(&re), source_(cx, re.source)
    {
        re_->incRef();
    }

    ~RegExpGuard() {
        release();
    }

  public:
    void init(RegExpShared &re) {
        JS_ASSERT(!initialized());
        re_ = &re;
        re_->incRef();
        source_ = re_->source;
    }

    void release() {
        if (re_) {
            re_->decRef();
            re_ = nullptr;
            source_ = nullptr;
        }
    }

    bool initialized() const { return !!re_; }
    RegExpShared *re() const { JS_ASSERT(initialized()); return re_; }
    RegExpShared *operator->() { return re(); }
    RegExpShared &operator*() { return *re(); }
};

class RegExpCompartment
{
    struct Key {
        JSAtom *atom;
        uint16_t flag;

        Key() {}
        Key(JSAtom *atom, RegExpFlag flag)
          : atom(atom), flag(flag)
        { }

        typedef Key Lookup;
        static HashNumber hash(const Lookup &l) {
            return DefaultHasher<JSAtom *>::hash(l.atom) ^ (l.flag << 1);
        }
        static bool match(Key l, Key r) {
            return l.atom == r.atom && l.flag == r.flag;
        }
    };

    /*
     * Cache to reuse RegExpShareds with the same source/flags/etc. The cache
     * is entirely cleared on each GC.
     */
    typedef HashMap<Key, RegExpShared *, Key, RuntimeAllocPolicy> Map;
    Map map_;

    /*
     * The set of all RegExpShareds in the compartment. On every GC, every
     * RegExpShared that is not actively being used is deleted and removed from
     * the set.
     */
    typedef HashSet<RegExpShared *, DefaultHasher<RegExpShared*>, RuntimeAllocPolicy> PendingSet;
    PendingSet inUse_;

  public:
    RegExpCompartment(JSRuntime *rt);
    ~RegExpCompartment();

    bool init(JSContext *cx);
    void sweep(JSRuntime *rt);
    void clearTables();

    bool get(ExclusiveContext *cx, JSAtom *source, RegExpFlag flags, RegExpGuard *g);

    /* Like 'get', but compile 'maybeOpt' (if non-null). */
    bool get(JSContext *cx, HandleAtom source, JSString *maybeOpt, RegExpGuard *g);

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);
};

class RegExpObject : public JSObject
{
    static const unsigned LAST_INDEX_SLOT          = 0;
    static const unsigned SOURCE_SLOT              = 1;
    static const unsigned GLOBAL_FLAG_SLOT         = 2;
    static const unsigned IGNORE_CASE_FLAG_SLOT    = 3;
    static const unsigned MULTILINE_FLAG_SLOT      = 4;
    static const unsigned STICKY_FLAG_SLOT         = 5;

  public:
    static const unsigned RESERVED_SLOTS = 6;

    static const Class class_;

    /*
     * Note: The regexp statics flags are OR'd into the provided flags,
     * so this function is really meant for object creation during code
     * execution, as opposed to during something like XDR.
     */
    static RegExpObject *
    create(ExclusiveContext *cx, RegExpStatics *res, const jschar *chars, size_t length,
           RegExpFlag flags, frontend::TokenStream *ts);

    static RegExpObject *
    createNoStatics(ExclusiveContext *cx, const jschar *chars, size_t length, RegExpFlag flags,
                    frontend::TokenStream *ts);

    static RegExpObject *
    createNoStatics(ExclusiveContext *cx, HandleAtom atom, RegExpFlag flags, frontend::TokenStream *ts);

    /* Accessors. */

    static unsigned lastIndexSlot() { return LAST_INDEX_SLOT; }

    const Value &getLastIndex() const { return getSlot(LAST_INDEX_SLOT); }

    void setLastIndex(double d) {
        setSlot(LAST_INDEX_SLOT, NumberValue(d));
    }

    void zeroLastIndex() {
        setSlot(LAST_INDEX_SLOT, Int32Value(0));
    }

    JSFlatString *toString(JSContext *cx) const;

    JSAtom *getSource() const { return &getSlot(SOURCE_SLOT).toString()->asAtom(); }

    void setSource(JSAtom *source) {
        setSlot(SOURCE_SLOT, StringValue(source));
    }

    RegExpFlag getFlags() const {
        unsigned flags = 0;
        flags |= global() ? GlobalFlag : 0;
        flags |= ignoreCase() ? IgnoreCaseFlag : 0;
        flags |= multiline() ? MultilineFlag : 0;
        flags |= sticky() ? StickyFlag : 0;
        return RegExpFlag(flags);
    }

    /* Flags. */

    void setIgnoreCase(bool enabled) {
        setSlot(IGNORE_CASE_FLAG_SLOT, BooleanValue(enabled));
    }

    void setGlobal(bool enabled) {
        setSlot(GLOBAL_FLAG_SLOT, BooleanValue(enabled));
    }

    void setMultiline(bool enabled) {
        setSlot(MULTILINE_FLAG_SLOT, BooleanValue(enabled));
    }

    void setSticky(bool enabled) {
        setSlot(STICKY_FLAG_SLOT, BooleanValue(enabled));
    }

    bool ignoreCase() const { return getFixedSlot(IGNORE_CASE_FLAG_SLOT).toBoolean(); }
    bool global() const     { return getFixedSlot(GLOBAL_FLAG_SLOT).toBoolean(); }
    bool multiline() const  { return getFixedSlot(MULTILINE_FLAG_SLOT).toBoolean(); }
    bool sticky() const     { return getFixedSlot(STICKY_FLAG_SLOT).toBoolean(); }

    void shared(RegExpGuard *g) const {
        JS_ASSERT(maybeShared() != nullptr);
        g->init(*maybeShared());
    }

    bool getShared(ExclusiveContext *cx, RegExpGuard *g) {
        if (RegExpShared *shared = maybeShared()) {
            g->init(*shared);
            return true;
        }
        return createShared(cx, g);
    }

    void setShared(ExclusiveContext *cx, RegExpShared &shared) {
        shared.prepareForUse(cx);
        JSObject::setPrivate(&shared);
    }

  private:
    friend class RegExpObjectBuilder;

    /* For access to assignInitialShape. */
    friend bool
    EmptyShape::ensureInitialCustomShape<RegExpObject>(ExclusiveContext *cx,
                                                       Handle<RegExpObject*> obj);

    /*
     * Compute the initial shape to associate with fresh RegExp objects,
     * encoding their initial properties. Return the shape after
     * changing |obj|'s last property to it.
     */
    static Shape *
    assignInitialShape(ExclusiveContext *cx, Handle<RegExpObject*> obj);

    bool init(ExclusiveContext *cx, HandleAtom source, RegExpFlag flags);

    /*
     * Precondition: the syntax for |source| has already been validated.
     * Side effect: sets the private field.
     */
    bool createShared(ExclusiveContext *cx, RegExpGuard *g);
    RegExpShared *maybeShared() const {
        return static_cast<RegExpShared *>(JSObject::getPrivate());
    }

    /* Call setShared in preference to setPrivate. */
    void setPrivate(void *priv) MOZ_DELETE;
};

/*
 * Parse regexp flags. Report an error and return false if an invalid
 * sequence of flags is encountered (repeat/invalid flag).
 *
 * N.B. flagStr must be rooted.
 */
bool
ParseRegExpFlags(JSContext *cx, JSString *flagStr, RegExpFlag *flagsOut);

/*
 * Assuming ObjectClassIs(obj, ESClass_RegExp), return obj's RegExpShared.
 *
 * Beware: this RegExpShared can be owned by a compartment other than
 * cx->compartment. Normal RegExpGuard (which is necessary anyways)
 * will protect the object but it is important not to assign the return value
 * to be the private of any RegExpObject.
 */
inline bool
RegExpToShared(JSContext *cx, HandleObject obj, RegExpGuard *g)
{
    if (obj->is<RegExpObject>())
        return obj->as<RegExpObject>().getShared(cx, g);
    return Proxy::regexp_toShared(cx, obj, g);
}

template<XDRMode mode>
bool
XDRScriptRegExpObject(XDRState<mode> *xdr, HeapPtrObject *objp);

extern JSObject *
CloneScriptRegExpObject(JSContext *cx, RegExpObject &re);

} /* namespace js */

#endif /* vm_RegExpObject_h */
