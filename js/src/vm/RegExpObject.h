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

#include "mozilla/Attributes.h"

#include <stddef.h>
#include "jscntxt.h"
#include "jsobj.h"

#include "js/TemplateLib.h"

#include "yarr/Yarr.h"
#if ENABLE_YARR_JIT
#include "yarr/YarrJIT.h"
#include "yarr/YarrSyntaxChecker.h"
#else
#include "yarr/pcre/pcre.h"
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
    JSContext               *cx;
    RootedVar<RegExpObject*> reobj_;

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
    RegExpCode()
      :
#if ENABLE_YARR_JIT
        codeBlock(),
        byteCode(NULL)
#else
        compiled(NULL)
#endif
    { }

    ~RegExpCode() {
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

    bool compile(JSContext *cx, JSLinearString &pattern, unsigned *parenCount, RegExpFlag flags);


    RegExpRunStatus
    execute(JSContext *cx, const jschar *chars, size_t length, size_t start,
            int *output, size_t outputCount);
};

}  /* namespace detail */

/*
 * A RegExpShared is the compiled representation of a regexp. A RegExpShared is
 * pointed to by potentially multiple RegExpObjects. Additionally, C++ code may
 * have pointers to RegExpShareds on the stack. The RegExpShareds are tracked in
 * a RegExpCompartment hashtable, and most are destroyed on every GC.
 *
 * During a GC, the trace hook for RegExpObject clears any pointers to
 * RegExpShareds so that there will be no dangling pointers when they are
 * deleted. However, some RegExpShareds are not deleted:
 *
 *   1. Any RegExpShared with pointers from the C++ stack is not deleted.
 *   2. Any RegExpShared that was installed in a RegExpObject during an
 *      incremental GC is not deleted. This is because the RegExpObject may have
 *      been traced through before the new RegExpShared was installed, in which
 *      case deleting the RegExpShared would turn the RegExpObject's reference
 *      into a dangling pointer
 *
 * The activeUseCount and gcNumberWhenUsed fields are used to track these two
 * conditions.
 */
class RegExpShared
{
    friend class RegExpCompartment;
    friend class RegExpGuard;

    detail::RegExpCode code;
    unsigned              parenCount;
    RegExpFlag         flags;
    size_t             activeUseCount;   /* See comment above. */
    uint64_t           gcNumberWhenUsed; /* See comment above. */

    bool compile(JSContext *cx, JSAtom *source);

    RegExpShared(JSRuntime *rt, RegExpFlag flags);
    JS_DECLARE_ALLOCATION_FRIENDS_FOR_PRIVATE_CONSTRUCTOR;

  public:

    /* Called when a RegExpShared is installed into a RegExpObject. */
    inline void prepareForUse(JSContext *cx);

    /* Primary interface: run this regular expression on the given string. */

    RegExpRunStatus
    execute(JSContext *cx, const jschar *chars, size_t length, size_t *lastIndex,
            MatchPairs **output);

    /* Accessors */

    size_t getParenCount() const        { return parenCount; }

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
        re_->activeUseCount++;
    }
    void init(RegExpShared &re) {
        JS_ASSERT(!re_);
        re_ = &re;
        re_->activeUseCount++;
    }
    ~RegExpGuard() {
        if (re_) {
            JS_ASSERT(re_->activeUseCount > 0);
            re_->activeUseCount--;
        }
    }
    bool initialized() const { return !!re_; }
    RegExpShared *operator->() { JS_ASSERT(initialized()); return re_; }
    RegExpShared &operator*() { JS_ASSERT(initialized()); return *re_; }
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

    typedef HashMap<Key, RegExpShared *, Key, RuntimeAllocPolicy> Map;
    Map map_;

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
    create(JSContext *cx, RegExpStatics *res, const jschar *chars, size_t length,
           RegExpFlag flags, TokenStream *ts);

    static RegExpObject *
    createNoStatics(JSContext *cx, const jschar *chars, size_t length, RegExpFlag flags,
                    TokenStream *ts);

    static RegExpObject *
    createNoStatics(JSContext *cx, HandleAtom atom, RegExpFlag flags, TokenStream *ts);

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
    execute(JSContext *cx, const jschar *chars, size_t length, size_t *lastIndex,
            MatchPairs **output);

    /* Accessors. */

    const Value &getLastIndex() const {
        return getSlot(LAST_INDEX_SLOT);
    }
    inline void setLastIndex(const Value &v);
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
    Shape *assignInitialShape(JSContext *cx);

    inline bool init(JSContext *cx, HandleAtom source, RegExpFlag flags);

    /*
     * Precondition: the syntax for |source| has already been validated.
     * Side effect: sets the private field.
     */
    bool createShared(JSContext *cx, RegExpGuard *g);
    RegExpShared *maybeShared() const;

    RegExpObject() MOZ_DELETE;
    RegExpObject &operator=(const RegExpObject &reo) MOZ_DELETE;

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

} /* namespace js */

#endif
