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

#ifndef RegExpObject_h__
#define RegExpObject_h__

#include <stddef.h>
#include "jsobj.h"

#include "js/TemplateLib.h"

#include "yarr/Yarr.h"
#if ENABLE_YARR_JIT
#include "yarr/YarrJIT.h"
#include "yarr/YarrSyntaxChecker.h"
#else
#include "yarr/pcre/pcre.h"
#endif

namespace js {

enum RegExpRunStatus
{
    RegExpRunStatus_Error,
    RegExpRunStatus_Success,
    RegExpRunStatus_Success_NotFound
};

class RegExpObject : public ::JSObject
{
    typedef detail::RegExpPrivate RegExpPrivate;
    typedef detail::RegExpPrivateCode RegExpPrivateCode;

    static const uintN LAST_INDEX_SLOT          = 0;
    static const uintN SOURCE_SLOT              = 1;
    static const uintN GLOBAL_FLAG_SLOT         = 2;
    static const uintN IGNORE_CASE_FLAG_SLOT    = 3;
    static const uintN MULTILINE_FLAG_SLOT      = 4;
    static const uintN STICKY_FLAG_SLOT         = 5;

  public:
    static const uintN RESERVED_SLOTS = 6;

    /*
     * Note: The regexp statics flags are OR'd into the provided flags,
     * so this function is really meant for object creation during code
     * execution, as opposed to during something like XDR.
     */
    static inline RegExpObject *
    create(JSContext *cx, RegExpStatics *res, const jschar *chars, size_t length, RegExpFlag flags,
           TokenStream *tokenStream);

    static inline RegExpObject *
    createNoStatics(JSContext *cx, const jschar *chars, size_t length, RegExpFlag flags,
                    TokenStream *tokenStream);

    static inline RegExpObject *
    createNoStatics(JSContext *cx, JSAtom *atom, RegExpFlag flags, TokenStream *tokenStream);

    /* Note: fallible. */
    JSFlatString *toString(JSContext *cx) const;

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
    RegExpRunStatus execute(JSContext *cx, const jschar *chars, size_t length, size_t *lastIndex,
                            LifoAllocScope &allocScope, MatchPairs **output);

    /* Accessors. */

    const Value &getLastIndex() const {
        return getSlot(LAST_INDEX_SLOT);
    }
    inline void setLastIndex(const Value &v);
    inline void setLastIndex(double d);
    inline void zeroLastIndex();

    JSLinearString *getSource() const {
        return &getSlot(SOURCE_SLOT).toString()->asLinear();
    }
    inline void setSource(JSLinearString *source);

    RegExpFlag getFlags() const {
        uintN flags = 0;
        flags |= global() ? GlobalFlag : 0;
        flags |= ignoreCase() ? IgnoreCaseFlag : 0;
        flags |= multiline() ? MultilineFlag : 0;
        flags |= sticky() ? StickyFlag : 0;
        return RegExpFlag(flags);
    }

    inline bool startsWithAtomizedGreedyStar() const;

    /* JIT only. */

    inline size_t *addressOfPrivateRefCount() const;

    /* Flags. */

    inline void setIgnoreCase(bool enabled);
    inline void setGlobal(bool enabled);
    inline void setMultiline(bool enabled);
    inline void setSticky(bool enabled);
    bool ignoreCase() const { return getSlot(IGNORE_CASE_FLAG_SLOT).toBoolean(); }
    bool global() const     { return getSlot(GLOBAL_FLAG_SLOT).toBoolean(); }
    bool multiline() const  { return getSlot(MULTILINE_FLAG_SLOT).toBoolean(); }
    bool sticky() const     { return getSlot(STICKY_FLAG_SLOT).toBoolean(); }

    inline void finalize(JSContext *cx);

    /* Clear out lazy |RegExpPrivate|. */
    inline void purge(JSContext *x);

    /*
     * Trigger an eager creation of the associated RegExpPrivate. Note
     * that a GC may purge it away.
     */
    bool makePrivateNow(JSContext *cx) {
        return getPrivate() ? true : !!makePrivate(cx);
    }

  private:
    friend class RegExpObjectBuilder;
    friend class RegExpMatcher;

    inline bool init(JSContext *cx, JSLinearString *source, RegExpFlag flags);

    RegExpPrivate *getOrCreatePrivate(JSContext *cx) {
        if (RegExpPrivate *rep = getPrivate())
            return rep;

        return makePrivate(cx);
    }

    /* The |RegExpPrivate| is lazily created at the time of use. */
    RegExpPrivate *getPrivate() const {
        return static_cast<RegExpPrivate *>(JSObject::getPrivate());
    }

    inline void setPrivate(RegExpPrivate *rep);

    /*
     * Precondition: the syntax for |source| has already been validated.
     * Side effect: sets the private field.
     */
    RegExpPrivate *makePrivate(JSContext *cx);

    friend bool ResetRegExpObject(JSContext *, RegExpObject *, JSLinearString *, RegExpFlag);
    friend bool ResetRegExpObject(JSContext *, RegExpObject *, AlreadyIncRefed<RegExpPrivate>);

    /*
     * Compute the initial shape to associate with fresh RegExp objects,
     * encoding their initial properties. Return the shape after
     * changing this regular expression object's last property to it.
     */
    Shape *assignInitialShape(JSContext *cx);

    RegExpObject();
    RegExpObject &operator=(const RegExpObject &reo);
}; /* class RegExpObject */

/* Either builds a new RegExpObject or re-initializes an existing one. */
class RegExpObjectBuilder
{
    typedef detail::RegExpPrivate RegExpPrivate;

    JSContext       *cx;
    RegExpObject    *reobj_;

    bool getOrCreate();
    bool getOrCreateClone(RegExpObject *proto);

    RegExpObject *build(AlreadyIncRefed<RegExpPrivate> rep);

    friend class RegExpMatcher;

  public:
    RegExpObjectBuilder(JSContext *cx, RegExpObject *reobj = NULL)
      : cx(cx), reobj_(reobj)
    { }

    RegExpObject *reobj() { return reobj_; }

    RegExpObject *build(JSLinearString *str, RegExpFlag flags);
    RegExpObject *build(RegExpObject *other);

    /* Perform a VM-internal clone. */
    RegExpObject *clone(RegExpObject *other, RegExpObject *proto);
};

namespace detail {

static const jschar GreedyStarChars[] = {'.', '*'};

/* Abstracts away the gross |RegExpPrivate| backend details. */
class RegExpPrivateCode
{
#if ENABLE_YARR_JIT
    typedef JSC::Yarr::BytecodePattern BytecodePattern;
    typedef JSC::Yarr::ErrorCode ErrorCode;
    typedef JSC::Yarr::JSGlobalData JSGlobalData;
    typedef JSC::Yarr::YarrCodeBlock YarrCodeBlock;
    typedef JSC::Yarr::YarrPattern YarrPattern;

    /* Note: Native code is valid only if |codeBlock.isFallBack() == false|. */
    YarrCodeBlock   codeBlock;
    BytecodePattern *byteCode;
#else
    JSRegExp        *compiled;
#endif

  public:
    RegExpPrivateCode()
      :
#if ENABLE_YARR_JIT
        codeBlock(),
        byteCode(NULL)
#else
        compiled(NULL)
#endif
    { }

    ~RegExpPrivateCode() {
#if ENABLE_YARR_JIT
        codeBlock.release();
        if (byteCode)
            Foreground::delete_<BytecodePattern>(byteCode);
#else
        if (compiled)
            jsRegExpFree(compiled);
#endif
    }

    static bool checkSyntax(JSContext *cx, TokenStream *tokenStream, JSLinearString *source) {
#if ENABLE_YARR_JIT
        ErrorCode error = JSC::Yarr::checkSyntax(*source);
        if (error == JSC::Yarr::NoError)
            return true;

        reportYarrError(cx, tokenStream, error);
        return false;
#else
# error "Syntax checking not implemented for !ENABLE_YARR_JIT"
#endif
    }

#if ENABLE_YARR_JIT
    static inline bool isJITRuntimeEnabled(JSContext *cx);
    static void reportYarrError(JSContext *cx, TokenStream *ts, JSC::Yarr::ErrorCode error);
#else
    static void reportPCREError(JSContext *cx, int error);
#endif

    static size_t getOutputSize(size_t pairCount) {
#if ENABLE_YARR_JIT
        return pairCount * 2;
#else
        return pairCount * 3; /* Should be x2, but PCRE has... needs. */
#endif
    }

    inline bool compile(JSContext *cx, JSLinearString &pattern, TokenStream *ts, uintN *parenCount,
                        RegExpFlag flags);


    inline RegExpRunStatus execute(JSContext *cx, const jschar *chars, size_t length, size_t start,
                                   int *output, size_t outputCount);
};

enum RegExpPrivateCacheKind
{
    RegExpPrivateCache_TestOptimized,
    RegExpPrivateCache_ExecCapable
};

class RegExpPrivateCacheValue
{
    union {
        RegExpPrivate   *rep_;
        uintptr_t       bits;
    };

  public:
    RegExpPrivateCacheValue() : rep_(NULL) {}

    RegExpPrivateCacheValue(RegExpPrivate *rep, RegExpPrivateCacheKind kind) {
        reset(rep, kind);
    }

    RegExpPrivateCacheKind kind() const {
        return (bits & 0x1)
                 ? RegExpPrivateCache_TestOptimized
                 : RegExpPrivateCache_ExecCapable;
    }

    RegExpPrivate *rep() {
        return reinterpret_cast<RegExpPrivate *>(bits & ~uintptr_t(1));
    }

    void reset(RegExpPrivate *rep, RegExpPrivateCacheKind kind) {
        rep_ = rep;
        if (kind == RegExpPrivateCache_TestOptimized)
            bits |= 0x1;
        JS_ASSERT(this->kind() == kind);
    }
};

/*
 * The "meat" of the builtin regular expression objects: it contains the
 * mini-program that represents the source of the regular expression.
 * Excepting refcounts, this is an immutable datastructure after
 * compilation.
 *
 * Non-atomic refcounting is used, so single-thread invariants must be
 * maintained. |RegExpPrivate|s are currently shared within a single
 * |ThreadData|.
 *
 * Note: refCount cannot overflow because that would require more
 * referring regexp objects than there is space for in addressable
 * memory.
 */
class RegExpPrivate
{
    RegExpPrivateCode   code;
    JSLinearString      *source;
    size_t              refCount;
    uintN               parenCount;
    RegExpFlag          flags;

  private:
    RegExpPrivate(JSLinearString *source, RegExpFlag flags)
      : source(source), refCount(1), parenCount(0), flags(flags)
    { }

    JS_DECLARE_ALLOCATION_FRIENDS_FOR_PRIVATE_CONSTRUCTOR;

    bool compile(JSContext *cx, TokenStream *ts);
    static inline void checkMatchPairs(JSString *input, int *buf, size_t matchItemCount);

    static RegExpPrivate *
    createUncached(JSContext *cx, JSLinearString *source, RegExpFlag flags,
                   TokenStream *tokenStream);

    static RegExpPrivateCache *getOrCreateCache(JSContext *cx);
    static bool cacheLookup(JSContext *cx, JSAtom *atom, RegExpFlag flags,
                            RegExpPrivateCacheKind kind, AlreadyIncRefed<RegExpPrivate> *result);
    static bool cacheInsert(JSContext *cx, JSAtom *atom,
                            RegExpPrivateCacheKind kind, RegExpPrivate *priv);

  public:
    static AlreadyIncRefed<RegExpPrivate>
    create(JSContext *cx, JSLinearString *source, RegExpFlag flags, TokenStream *ts);

    static AlreadyIncRefed<RegExpPrivate>
    create(JSContext *cx, JSLinearString *source, JSString *flags, TokenStream *ts);

    static AlreadyIncRefed<RegExpPrivate>
    createTestOptimized(JSContext *cx, JSAtom *originalSource, RegExpFlag flags);

    RegExpRunStatus execute(JSContext *cx, const jschar *chars, size_t length, size_t *lastIndex,
                            LifoAllocScope &allocScope, MatchPairs **output);

    /* Mutators */

    void incref(JSContext *cx);
    void decref(JSContext *cx);

    /* For JIT access. */
    size_t *addressOfRefCount() { return &refCount; }

    /* Accessors */

    JSLinearString *getSource() const   { return source; }
    size_t getParenCount() const        { return parenCount; }

    /* Accounts for the "0" (whole match) pair. */
    size_t pairCount() const            { return parenCount + 1; }

    RegExpFlag getFlags() const         { return flags; }
    bool ignoreCase() const { return flags & IgnoreCaseFlag; }
    bool global() const     { return flags & GlobalFlag; }
    bool multiline() const  { return flags & MultilineFlag; }
    bool sticky() const     { return flags & StickyFlag; }
};

} /* namespace detail */

/*
 * Wraps the RegExpObject's internals in a recount-safe interface for
 * use in RegExp execution. This is used in situations where we'd like
 * to avoid creating a full-fledged RegExpObject. This interface is
 * provided in lieu of exposing the RegExpPrivate directly.
 *
 * Note: this exposes precisely the execute interface of a RegExpObject.
 */
class RegExpMatcher
{
    typedef detail::RegExpPrivate RegExpPrivate;

    JSContext                   *cx;
    AutoRefCount<RegExpPrivate> arc;

  public:
    explicit RegExpMatcher(JSContext *cx)
      : cx(cx), arc(cx)
    { }

    bool null() const {
        return arc.null();
    }
    bool global() const {
        return arc.get()->global();
    }
    bool sticky() const {
        return arc.get()->sticky();
    }

    bool reset(RegExpObject *reobj) {
        RegExpPrivate *priv = reobj->getOrCreatePrivate(cx);
        if (!priv)
            return false;
        arc.reset(NeedsIncRef<RegExpPrivate>(priv));
        return true;
    }

    inline bool reset(JSLinearString *patstr, JSString *opt);

    bool resetWithTestOptimized(RegExpObject *reobj);

    RegExpRunStatus execute(JSContext *cx, const jschar *chars, size_t length, size_t *lastIndex,
                            LifoAllocScope &allocScope, MatchPairs **output) {
        JS_ASSERT(!arc.null());
        return arc.get()->execute(cx, chars, length, lastIndex, allocScope, output);
    }
};

/*
 * Parse regexp flags. Report an error and return false if an invalid
 * sequence of flags is encountered (repeat/invalid flag).
 *
 * N.B. flagStr must be rooted.
 */
bool
ParseRegExpFlags(JSContext *cx, JSString *flagStr, RegExpFlag *flagsOut);

inline bool
ValueIsRegExp(const Value &v);

inline bool
IsRegExpMetaChar(jschar c);

inline bool
CheckRegExpSyntax(JSContext *cx, JSLinearString *str)
{
    return detail::RegExpPrivateCode::checkSyntax(cx, NULL, str);
}

} /* namespace js */

extern JS_FRIEND_API(JSObject *) JS_FASTCALL
js_CloneRegExpObject(JSContext *cx, JSObject *obj, JSObject *proto);

JSBool
js_XDRRegExpObject(JSXDRState *xdr, JSObject **objp);

#endif
