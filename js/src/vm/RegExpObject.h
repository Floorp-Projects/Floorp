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

#include "builtin/SelfHostingDefines.h"
#include "gc/Marking.h"
#include "gc/Zone.h"
#include "proxy/Proxy.h"
#include "vm/ArrayObject.h"
#include "vm/Shape.h"

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

struct MatchPair;
class MatchPairs;
class RegExpShared;
class RegExpStatics;

namespace frontend { class TokenStream; }

enum RegExpFlag
{
    IgnoreCaseFlag  = 0x01,
    GlobalFlag      = 0x02,
    MultilineFlag   = 0x04,
    StickyFlag      = 0x08,
    UnicodeFlag     = 0x10,

    NoFlags         = 0x00,
    AllFlags        = 0x1f
};

static_assert(IgnoreCaseFlag == REGEXP_IGNORECASE_FLAG &&
              GlobalFlag == REGEXP_GLOBAL_FLAG &&
              MultilineFlag == REGEXP_MULTILINE_FLAG &&
              StickyFlag == REGEXP_STICKY_FLAG &&
              UnicodeFlag == REGEXP_UNICODE_FLAG,
              "Flag values should be in sync with self-hosted JS");

enum RegExpRunStatus
{
    RegExpRunStatus_Error,
    RegExpRunStatus_Success,
    RegExpRunStatus_Success_NotFound
};

extern RegExpObject*
RegExpAlloc(ExclusiveContext* cx, HandleObject proto = nullptr);

// |regexp| is under-typed because this function's used in the JIT.
extern JSObject*
CloneRegExpObject(JSContext* cx, JSObject* regexp);

/*
 * A RegExpShared is the compiled representation of a regexp. A RegExpShared is
 * potentially pointed to by multiple RegExpObjects. Additionally, C++ code may
 * have pointers to RegExpShareds on the stack. The RegExpShareds are kept in a
 * table so that they can be reused when compiling the same regex string.
 *
 * During a GC, RegExpShared instances are marked and swept like GC things.
 * Usually, RegExpObjects clear their pointers to their RegExpShareds rather
 * than explicitly tracing them, so that the RegExpShared and any jitcode can
 * be reclaimed quicker. However, the RegExpShareds are traced through by
 * objects when we are preserving jitcode in their zone, to avoid the same
 * recompilation inefficiencies as normal Ion and baseline compilation.
 */
class RegExpShared
{
  public:
    enum CompilationMode {
        Normal,
        MatchOnly
    };

    enum ForceByteCodeEnum {
        DontForceByteCode,
        ForceByteCode
    };

  private:
    friend class RegExpCompartment;
    friend class RegExpStatics;

    typedef frontend::TokenStream TokenStream;

    struct RegExpCompilation
    {
        HeapPtr<jit::JitCode*> jitCode;
        uint8_t* byteCode;

        RegExpCompilation() : byteCode(nullptr) {}
        ~RegExpCompilation() { js_free(byteCode); }

        bool compiled(ForceByteCodeEnum force = DontForceByteCode) const {
            return byteCode || (force == DontForceByteCode && jitCode);
        }
    };

    /* Source to the RegExp, for lazy compilation. */
    HeapPtr<JSAtom*>     source;

    RegExpFlag         flags;
    size_t             parenCount;
    bool               canStringMatch;
    bool               marked_;

    RegExpCompilation  compilationArray[4];

    static int CompilationIndex(CompilationMode mode, bool latin1) {
        switch (mode) {
          case Normal:    return latin1 ? 0 : 1;
          case MatchOnly: return latin1 ? 2 : 3;
        }
        MOZ_CRASH();
    }

    // Tables referenced by JIT code.
    Vector<uint8_t*, 0, SystemAllocPolicy> tables;

    /* Internal functions. */
    bool compile(JSContext* cx, HandleLinearString input,
                 CompilationMode mode, ForceByteCodeEnum force);
    bool compile(JSContext* cx, HandleAtom pattern, HandleLinearString input,
                 CompilationMode mode, ForceByteCodeEnum force);

    bool compileIfNecessary(JSContext* cx, HandleLinearString input,
                            CompilationMode mode, ForceByteCodeEnum force);

    const RegExpCompilation& compilation(CompilationMode mode, bool latin1) const {
        return compilationArray[CompilationIndex(mode, latin1)];
    }

    RegExpCompilation& compilation(CompilationMode mode, bool latin1) {
        return compilationArray[CompilationIndex(mode, latin1)];
    }

  public:
    RegExpShared(JSAtom* source, RegExpFlag flags);
    ~RegExpShared();

    // Execute this RegExp on input starting from searchIndex, filling in
    // matches if specified and otherwise only determining if there is a match.
    RegExpRunStatus execute(JSContext* cx, HandleLinearString input, size_t searchIndex,
                            MatchPairs* matches, size_t* endIndex);

    // Register a table with this RegExpShared, and take ownership.
    bool addTable(uint8_t* table) {
        return tables.append(table);
    }

    /* Accessors */

    size_t getParenCount() const {
        MOZ_ASSERT(isCompiled());
        return parenCount;
    }

    /* Accounts for the "0" (whole match) pair. */
    size_t pairCount() const            { return getParenCount() + 1; }

    JSAtom* getSource() const           { return source; }
    RegExpFlag getFlags() const         { return flags; }
    bool ignoreCase() const             { return flags & IgnoreCaseFlag; }
    bool global() const                 { return flags & GlobalFlag; }
    bool multiline() const              { return flags & MultilineFlag; }
    bool sticky() const                 { return flags & StickyFlag; }
    bool unicode() const                { return flags & UnicodeFlag; }

    bool isCompiled(CompilationMode mode, bool latin1,
                    ForceByteCodeEnum force = DontForceByteCode) const {
        return compilation(mode, latin1).compiled(force);
    }
    bool isCompiled() const {
        return isCompiled(Normal, true) || isCompiled(Normal, false)
            || isCompiled(MatchOnly, true) || isCompiled(MatchOnly, false);
    }

    void trace(JSTracer* trc);
    bool needsSweep(JSRuntime* rt);
    void discardJitCode();

    bool marked() const { return marked_; }
    void clearMarked() { marked_ = false; }

    static size_t offsetOfSource() {
        return offsetof(RegExpShared, source);
    }

    static size_t offsetOfFlags() {
        return offsetof(RegExpShared, flags);
    }

    static size_t offsetOfParenCount() {
        return offsetof(RegExpShared, parenCount);
    }

    static size_t offsetOfLatin1JitCode(CompilationMode mode) {
        return offsetof(RegExpShared, compilationArray)
             + (CompilationIndex(mode, true) * sizeof(RegExpCompilation))
             + offsetof(RegExpCompilation, jitCode);
    }
    static size_t offsetOfTwoByteJitCode(CompilationMode mode) {
        return offsetof(RegExpShared, compilationArray)
             + (CompilationIndex(mode, false) * sizeof(RegExpCompilation))
             + offsetof(RegExpCompilation, jitCode);
    }

    size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

#ifdef DEBUG
    bool dumpBytecode(JSContext* cx, bool match_only, HandleLinearString input);
#endif
};

/*
 * Extend the lifetime of a given RegExpShared to at least the lifetime of
 * the guard object. See Regular Expression comment at the top.
 */
class RegExpGuard : public JS::CustomAutoRooter
{
    RegExpShared* re_;

    RegExpGuard(const RegExpGuard&) = delete;
    void operator=(const RegExpGuard&) = delete;

  public:
    explicit RegExpGuard(ExclusiveContext* cx)
      : CustomAutoRooter(cx), re_(nullptr)
    {}

    RegExpGuard(ExclusiveContext* cx, RegExpShared& re)
      : CustomAutoRooter(cx), re_(nullptr)
    {
        init(re);
    }

    ~RegExpGuard() {
        release();
    }

  public:
    void init(RegExpShared& re) {
        MOZ_ASSERT(!initialized());
        re_ = &re;
    }

    void release() {
        re_ = nullptr;
    }

    virtual void trace(JSTracer* trc) {
        if (re_)
            re_->trace(trc);
    }

    bool initialized() const { return !!re_; }
    RegExpShared* re() const { MOZ_ASSERT(initialized()); return re_; }
    RegExpShared* operator->() { return re(); }
    RegExpShared& operator*() { return *re(); }
};

class RegExpCompartment
{
    struct Key {
        JSAtom* atom;
        uint16_t flag;

        Key() {}
        Key(JSAtom* atom, RegExpFlag flag)
          : atom(atom), flag(flag)
        { }
        MOZ_IMPLICIT Key(RegExpShared* shared)
          : atom(shared->getSource()), flag(shared->getFlags())
        { }

        typedef Key Lookup;
        static HashNumber hash(const Lookup& l) {
            return DefaultHasher<JSAtom*>::hash(l.atom) ^ (l.flag << 1);
        }
        static bool match(Key l, Key r) {
            return l.atom == r.atom && l.flag == r.flag;
        }
    };

    /*
     * The set of all RegExpShareds in the compartment. On every GC, every
     * RegExpShared that was not marked is deleted and removed from the set.
     */
    typedef HashSet<RegExpShared*, Key, RuntimeAllocPolicy> Set;
    Set set_;

    /*
     * This is the template object where the result of re.exec() is based on,
     * if there is a result. This is used in CreateRegExpMatchResult to set
     * the input/index properties faster.
     */
    ReadBarriered<ArrayObject*> matchResultTemplateObject_;

    /*
     * The shape of RegExp.prototype object that satisfies following:
     *   * RegExp.prototype.flags getter is not modified
     *   * RegExp.prototype.global getter is not modified
     *   * RegExp.prototype.ignoreCase getter is not modified
     *   * RegExp.prototype.multiline getter is not modified
     *   * RegExp.prototype.sticky getter is not modified
     *   * RegExp.prototype.unicode getter is not modified
     *   * RegExp.prototype.exec is an own data property
     *   * RegExp.prototype[@@match] is an own data property
     *   * RegExp.prototype[@@search] is an own data property
     */
    ReadBarriered<Shape*> optimizableRegExpPrototypeShape_;

    /*
     * The shape of RegExp instance that satisfies following:
     *   * lastProperty is lastIndex
     *   * prototype is RegExp.prototype
     */
    ReadBarriered<Shape*> optimizableRegExpInstanceShape_;

    ArrayObject* createMatchResultTemplateObject(JSContext* cx);

  public:
    explicit RegExpCompartment(JSRuntime* rt);
    ~RegExpCompartment();

    bool init(JSContext* cx);
    void sweep(JSRuntime* rt);

    bool empty() { return set_.empty(); }

    bool get(JSContext* cx, JSAtom* source, RegExpFlag flags, RegExpGuard* g);

    /* Like 'get', but compile 'maybeOpt' (if non-null). */
    bool get(JSContext* cx, HandleAtom source, JSString* maybeOpt, RegExpGuard* g);

    /* Get or create template object used to base the result of .exec() on. */
    ArrayObject* getOrCreateMatchResultTemplateObject(JSContext* cx) {
        if (matchResultTemplateObject_)
            return matchResultTemplateObject_;
        return createMatchResultTemplateObject(cx);
    }

    Shape* getOptimizableRegExpPrototypeShape() {
        return optimizableRegExpPrototypeShape_;
    }
    void setOptimizableRegExpPrototypeShape(Shape* shape) {
        optimizableRegExpPrototypeShape_ = shape;
    }
    Shape* getOptimizableRegExpInstanceShape() {
        return optimizableRegExpInstanceShape_;
    }
    void setOptimizableRegExpInstanceShape(Shape* shape) {
        optimizableRegExpInstanceShape_ = shape;
    }

    static size_t offsetOfOptimizableRegExpPrototypeShape() {
        return offsetof(RegExpCompartment, optimizableRegExpPrototypeShape_);
    }
    static size_t offsetOfOptimizableRegExpInstanceShape() {
        return offsetof(RegExpCompartment, optimizableRegExpInstanceShape_);
    }

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);
};

class RegExpObject : public NativeObject
{
    static const unsigned LAST_INDEX_SLOT          = 0;
    static const unsigned SOURCE_SLOT              = 1;
    static const unsigned FLAGS_SLOT               = 2;

    static_assert(RegExpObject::FLAGS_SLOT == REGEXP_FLAGS_SLOT,
                  "FLAGS_SLOT values should be in sync with self-hosted JS");

  public:
    static const unsigned RESERVED_SLOTS = 3;
    static const unsigned PRIVATE_SLOT = 3;

    static const Class class_;
    static const Class protoClass_;

    // The maximum number of pairs a MatchResult can have, without having to
    // allocate a bigger MatchResult.
    static const size_t MaxPairCount = 14;

    static RegExpObject*
    create(ExclusiveContext* cx, const char16_t* chars, size_t length, RegExpFlag flags,
           frontend::TokenStream* ts, LifoAlloc& alloc);

    static RegExpObject*
    create(ExclusiveContext* cx, HandleAtom atom, RegExpFlag flags,
           frontend::TokenStream* ts, LifoAlloc& alloc);

    /*
     * Compute the initial shape to associate with fresh RegExp objects,
     * encoding their initial properties. Return the shape after
     * changing |obj|'s last property to it.
     */
    static Shape*
    assignInitialShape(ExclusiveContext* cx, Handle<RegExpObject*> obj);

    /* Accessors. */

    static unsigned lastIndexSlot() { return LAST_INDEX_SLOT; }

    static bool isInitialShape(RegExpObject* rx) {
        Shape* shape = rx->lastProperty();
        if (!shape->hasSlot())
            return false;
        if (shape->maybeSlot() != LAST_INDEX_SLOT)
            return false;
        return true;
    }

    const Value& getLastIndex() const { return getSlot(LAST_INDEX_SLOT); }

    void setLastIndex(double d) {
        setSlot(LAST_INDEX_SLOT, NumberValue(d));
    }

    void zeroLastIndex(ExclusiveContext* cx) {
        MOZ_ASSERT(lookupPure(cx->names().lastIndex)->writable(),
                   "can't infallibly zero a non-writable lastIndex on a "
                   "RegExp that's been exposed to script");
        setSlot(LAST_INDEX_SLOT, Int32Value(0));
    }

    JSFlatString* toString(JSContext* cx) const;

    JSAtom* getSource() const { return &getSlot(SOURCE_SLOT).toString()->asAtom(); }

    void setSource(JSAtom* source) {
        setSlot(SOURCE_SLOT, StringValue(source));
    }

    /* Flags. */

    static unsigned flagsSlot() { return FLAGS_SLOT; }

    RegExpFlag getFlags() const {
        return RegExpFlag(getFixedSlot(FLAGS_SLOT).toInt32());
    }
    void setFlags(RegExpFlag flags) {
        setSlot(FLAGS_SLOT, Int32Value(flags));
    }

    bool ignoreCase() const { return getFlags() & IgnoreCaseFlag; }
    bool global() const     { return getFlags() & GlobalFlag; }
    bool multiline() const  { return getFlags() & MultilineFlag; }
    bool sticky() const     { return getFlags() & StickyFlag; }
    bool unicode() const    { return getFlags() & UnicodeFlag; }

    static bool isOriginalFlagGetter(JSNative native, RegExpFlag* mask);

    bool getShared(JSContext* cx, RegExpGuard* g);

    void setShared(RegExpShared& shared) {
        MOZ_ASSERT(!maybeShared());
        NativeObject::setPrivate(&shared);
    }

    static void trace(JSTracer* trc, JSObject* obj);

    void initIgnoringLastIndex(HandleAtom source, RegExpFlag flags);

    // NOTE: This method is *only* safe to call on RegExps that haven't been
    //       exposed to script, because it requires that the "lastIndex"
    //       property be writable.
    void initAndZeroLastIndex(HandleAtom source, RegExpFlag flags, ExclusiveContext* cx);

#ifdef DEBUG
    bool dumpBytecode(JSContext* cx, bool match_only, HandleLinearString input);
#endif

  private:
    /*
     * Precondition: the syntax for |source| has already been validated.
     * Side effect: sets the private field.
     */
    bool createShared(JSContext* cx, RegExpGuard* g);
    RegExpShared* maybeShared() const {
        return static_cast<RegExpShared*>(NativeObject::getPrivate(PRIVATE_SLOT));
    }

    /* Call setShared in preference to setPrivate. */
    void setPrivate(void* priv) = delete;
};

/*
 * Parse regexp flags. Report an error and return false if an invalid
 * sequence of flags is encountered (repeat/invalid flag).
 *
 * N.B. flagStr must be rooted.
 */
bool
ParseRegExpFlags(JSContext* cx, JSString* flagStr, RegExpFlag* flagsOut);

/* Assuming GetBuiltinClass(obj) is ESClass::RegExp, return a RegExpShared for obj. */
inline bool
RegExpToShared(JSContext* cx, HandleObject obj, RegExpGuard* g)
{
    if (obj->is<RegExpObject>())
        return obj->as<RegExpObject>().getShared(cx, g);

    return Proxy::regexp_toShared(cx, obj, g);
}

template<XDRMode mode>
bool
XDRScriptRegExpObject(XDRState<mode>* xdr, MutableHandle<RegExpObject*> objp);

extern JSObject*
CloneScriptRegExpObject(JSContext* cx, RegExpObject& re);

/* Escape all slashes and newlines in the given string. */
extern JSAtom*
EscapeRegExpPattern(JSContext* cx, HandleAtom src);

template <typename CharT>
extern bool
HasRegExpMetaChars(const CharT* chars, size_t length);

extern bool
StringHasRegExpMetaChars(JSLinearString* str);

} /* namespace js */

#endif /* vm_RegExpObject_h */
