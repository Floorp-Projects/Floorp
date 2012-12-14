/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RegExpObject_h__
#define RegExpObject_h__

#include "mozilla/Attributes.h"

#include <stddef.h>
#include "jscntxt.h"
#include "jsobj.h"

#include "js/TemplateLib.h"

#include "yarr/Yarr.h"
#if ENABLE_YARR_JIT
#include "yarr/YarrJIT.h"
#endif
#include "yarr/YarrSyntaxChecker.h"

/*
 * JavaScript Regular Expressions
 *
 * There are several engine concepts associated with a single logical regexp:
 *
 *   RegExpObject - The JS-visible object whose .[[Class]] equals "RegExp"
 *
 *   RegExpShared - The compiled representation of the regexp.
 *
 *   RegExpCode - The low-level implementation jit details.
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

enum RegExpRunStatus
{
    RegExpRunStatus_Error,
    RegExpRunStatus_Success,
    RegExpRunStatus_Success_NotFound
};

class RegExpObjectBuilder
{
    JSContext             *cx;
    Rooted<RegExpObject*> reobj_;

    bool getOrCreate();
    bool getOrCreateClone(RegExpObject *proto);

  public:
    RegExpObjectBuilder(JSContext *cx, RegExpObject *reobj = NULL);

    RegExpObject *reobj() { return reobj_; }

    RegExpObject *build(HandleAtom source, RegExpFlag flags);
    RegExpObject *build(HandleAtom source, RegExpShared &shared);

    /* Perform a VM-internal clone. */
    RegExpObject *clone(Handle<RegExpObject*> other, Handle<RegExpObject*> proto);
};

JSObject *
CloneRegExpObject(JSContext *cx, JSObject *obj, JSObject *proto);

namespace detail {

class RegExpCode
{
    typedef JSC::Yarr::BytecodePattern BytecodePattern;
    typedef JSC::Yarr::ErrorCode ErrorCode;
    typedef JSC::Yarr::YarrPattern YarrPattern;
#if ENABLE_YARR_JIT
    typedef JSC::Yarr::JSGlobalData JSGlobalData;
    typedef JSC::Yarr::YarrCodeBlock YarrCodeBlock;

    /* Note: Native code is valid only if |codeBlock.isFallBack() == false|. */
    YarrCodeBlock   codeBlock;
#endif
    BytecodePattern *byteCode;

  public:
    RegExpCode()
      :
#if ENABLE_YARR_JIT
        codeBlock(),
#endif
        byteCode(NULL)
    { }

    ~RegExpCode() {
#if ENABLE_YARR_JIT
        codeBlock.release();
#endif
        if (byteCode)
            js_delete<BytecodePattern>(byteCode);
    }

    static bool checkSyntax(JSContext *cx, frontend::TokenStream *tokenStream,
                            JSLinearString *source)
    {
        ErrorCode error = JSC::Yarr::checkSyntax(*source);
        if (error == JSC::Yarr::NoError)
            return true;

        reportYarrError(cx, tokenStream, error);
        return false;
    }

#if ENABLE_YARR_JIT
    static inline bool isJITRuntimeEnabled(JSContext *cx);
#endif
    static void reportYarrError(JSContext *cx, frontend::TokenStream *ts,
                                JSC::Yarr::ErrorCode error);

    static size_t getOutputSize(size_t pairCount) {
        return pairCount * 2;
    }

    bool compile(JSContext *cx, JSLinearString &pattern, unsigned *parenCount, RegExpFlag flags);


    RegExpRunStatus
    execute(JSContext *cx, StableCharPtr chars, size_t length, size_t start,
            int *output, size_t outputCount);
};

}  /* namespace detail */

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
    friend class RegExpGuard;

    detail::RegExpCode code;
    unsigned           parenCount;
    RegExpFlag         flags;
    size_t             activeUseCount;   /* See comment above. */
    uint64_t           gcNumberWhenUsed; /* See comment above. */

    bool compile(JSContext *cx, JSAtom *source);

  public:
    RegExpShared(JSRuntime *rt, RegExpFlag flags);

    /* Called when a RegExpShared is installed into a RegExpObject. */
    inline void prepareForUse(JSContext *cx);

    /* Primary interface: run this regular expression on the given string. */

    RegExpRunStatus
    execute(JSContext *cx, StableCharPtr chars, size_t length, size_t *lastIndex,
            MatchPairs **output);

    /* Accessors */

    size_t getParenCount() const        { return parenCount; }
    void incRef()                       { activeUseCount++; }
    void decRef()                       { JS_ASSERT(activeUseCount > 0); activeUseCount--; }

    /* Accounts for the "0" (whole match) pair. */
    size_t pairCount() const            { return parenCount + 1; }

    RegExpFlag getFlags() const         { return flags; }
    bool ignoreCase() const             { return flags & IgnoreCaseFlag; }
    bool global() const                 { return flags & GlobalFlag; }
    bool multiline() const              { return flags & MultilineFlag; }
    bool sticky() const                 { return flags & StickyFlag; }
};

/*
 * Extend the lifetime of a given RegExpShared to at least the lifetime of
 * the guard object. See Regular Expression comment at the top.
 */
class RegExpGuard
{
    RegExpShared *re_;
    RegExpGuard(const RegExpGuard &) MOZ_DELETE;
    void operator=(const RegExpGuard &) MOZ_DELETE;
  public:
    RegExpGuard() : re_(NULL) {}
    RegExpGuard(RegExpShared &re) : re_(&re) {
        re_->incRef();
    }
    void init(RegExpShared &re) {
        JS_ASSERT(!re_);
        re_ = &re;
        re_->incRef();
    }
    ~RegExpGuard() {
        if (re_)
            re_->decRef();
    }
    bool initialized() const { return !!re_; }
    RegExpShared *re() const { JS_ASSERT(initialized()); return re_; }
    RegExpShared *operator->() { return re(); }
    RegExpShared &operator*() { return *re(); }
};

class RegExpCompartment
{
    enum Type { Normal = 0x0, Hack = 0x1 };

    struct Key {
        JSAtom *atom;
        uint16_t flag;
        uint16_t type;
        Key() {}
        Key(JSAtom *atom, RegExpFlag flag, Type type)
          : atom(atom), flag(flag), type(type) {}
        typedef Key Lookup;
        static HashNumber hash(const Lookup &l) {
            return DefaultHasher<JSAtom *>::hash(l.atom) ^ (l.flag << 1) ^ l.type;
        }
        static bool match(Key l, Key r) {
            return l.atom == r.atom && l.flag == r.flag && l.type == r.type;
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

    bool get(JSContext *cx, JSAtom *key, JSAtom *source, RegExpFlag flags, Type type,
             RegExpGuard *g);

  public:
    RegExpCompartment(JSRuntime *rt);
    ~RegExpCompartment();

    bool init(JSContext *cx);
    void sweep(JSRuntime *rt);

    /* Return a regexp corresponding to the given (source, flags) pair. */
    bool get(JSContext *cx, JSAtom *source, RegExpFlag flags, RegExpGuard *g);

    /* Like 'get', but compile 'maybeOpt' (if non-null). */
    bool get(JSContext *cx, JSAtom *source, JSString *maybeOpt, RegExpGuard *g);

    /*
     * A 'hacked' RegExpShared is one where the input 'source' doesn't match
     * what is actually compiled in the regexp. To compile a hacked regexp,
     * getHack may be called providing both the original 'source' and the
     * 'hackedSource' which should actually be compiled. For a given 'source'
     * there may only ever be one corresponding 'hackedSource'. Thus, we assume
     * there is some single pure function mapping 'source' to 'hackedSource'
     * that is always respected in calls to getHack. Note that this restriction
     * only applies to 'getHack': a single 'source' value may be passed to both
     * 'get' and 'getHack'.
     */
    bool getHack(JSContext *cx, JSAtom *source, JSAtom *hackedSource, RegExpFlag flags,
                 RegExpGuard *g);

    /*
     * To avoid atomizing 'hackedSource', callers may call 'lookupHack',
     * passing only the original 'source'. Due to the abovementioned unique
     * mapping property, 'hackedSource' is unambiguous.
     */
    bool lookupHack(JSAtom *source, RegExpFlag flags, JSContext *cx, RegExpGuard *g);

    size_t sizeOfExcludingThis(JSMallocSizeOfFun mallocSizeOf);
};

class RegExpObject : public JSObject
{
    typedef detail::RegExpCode RegExpCode;

    static const unsigned LAST_INDEX_SLOT          = 0;
    static const unsigned SOURCE_SLOT              = 1;
    static const unsigned GLOBAL_FLAG_SLOT         = 2;
    static const unsigned IGNORE_CASE_FLAG_SLOT    = 3;
    static const unsigned MULTILINE_FLAG_SLOT      = 4;
    static const unsigned STICKY_FLAG_SLOT         = 5;

  public:
    static const unsigned RESERVED_SLOTS = 6;

    /*
     * Note: The regexp statics flags are OR'd into the provided flags,
     * so this function is really meant for object creation during code
     * execution, as opposed to during something like XDR.
     */
    static RegExpObject *
    create(JSContext *cx, RegExpStatics *res, StableCharPtr chars, size_t length,
           RegExpFlag flags, frontend::TokenStream *ts);

    static RegExpObject *
    createNoStatics(JSContext *cx, StableCharPtr chars, size_t length, RegExpFlag flags,
                    frontend::TokenStream *ts);

    static RegExpObject *
    createNoStatics(JSContext *cx, HandleAtom atom, RegExpFlag flags, frontend::TokenStream *ts);

    /*
     * Run the regular expression over the input text.
     *
     * Results are placed in |output| as integer pairs. For eaxmple,
     * |output[0]| and |output[1]| represent the text indices that make
     * up the "0" (whole match) pair. Capturing parens will result in
     * more output.
     *
     * N.B. it's the responsibility of the caller to hook the |output|
     * into the |RegExpStatics| appropriately, if necessary.
     */
    RegExpRunStatus
    execute(JSContext *cx, StableCharPtr chars, size_t length, size_t *lastIndex,
            MatchPairs **output);

    /* Accessors. */

    const Value &getLastIndex() const {
        return getSlot(LAST_INDEX_SLOT);
    }
    inline void setLastIndex(double d);
    inline void zeroLastIndex();

    JSFlatString *toString(JSContext *cx) const;

    JSAtom *getSource() const {
        return &getSlot(SOURCE_SLOT).toString()->asAtom();
    }
    inline void setSource(JSAtom *source);

    RegExpFlag getFlags() const {
        unsigned flags = 0;
        flags |= global() ? GlobalFlag : 0;
        flags |= ignoreCase() ? IgnoreCaseFlag : 0;
        flags |= multiline() ? MultilineFlag : 0;
        flags |= sticky() ? StickyFlag : 0;
        return RegExpFlag(flags);
    }

    /* Flags. */

    inline void setIgnoreCase(bool enabled);
    inline void setGlobal(bool enabled);
    inline void setMultiline(bool enabled);
    inline void setSticky(bool enabled);
    bool ignoreCase() const { return getSlot(IGNORE_CASE_FLAG_SLOT).toBoolean(); }
    bool global() const     { return getSlot(GLOBAL_FLAG_SLOT).toBoolean(); }
    bool multiline() const  { return getSlot(MULTILINE_FLAG_SLOT).toBoolean(); }
    bool sticky() const     { return getSlot(STICKY_FLAG_SLOT).toBoolean(); }

    inline void shared(RegExpGuard *g) const;
    inline bool getShared(JSContext *cx, RegExpGuard *g);
    inline void setShared(JSContext *cx, RegExpShared &shared);

  private:
    friend class RegExpObjectBuilder;

    /*
     * Compute the initial shape to associate with fresh RegExp objects,
     * encoding their initial properties. Return the shape after
     * changing this regular expression object's last property to it.
     */
    UnrootedShape assignInitialShape(JSContext *cx);

    inline bool init(JSContext *cx, HandleAtom source, RegExpFlag flags);

    /*
     * Precondition: the syntax for |source| has already been validated.
     * Side effect: sets the private field.
     */
    bool createShared(JSContext *cx, RegExpGuard *g);
    RegExpShared *maybeShared() const;

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
RegExpToShared(JSContext *cx, JSObject &obj, RegExpGuard *g);

template<XDRMode mode>
bool
XDRScriptRegExpObject(XDRState<mode> *xdr, HeapPtrObject *objp);

extern JSObject *
CloneScriptRegExpObject(JSContext *cx, RegExpObject &re);

} /* namespace js */

#endif
