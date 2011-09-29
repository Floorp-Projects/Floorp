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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * June 12, 2009.
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Corporation.
 *
 * Contributor(s):
 *   Chris Leary <cdleary@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef jsregexpinlines_h___
#define jsregexpinlines_h___

#include "jscntxt.h"
#include "jsobj.h"
#include "jsregexp.h"
#include "jsscope.h"

#include "jsobjinlines.h"
#include "jsstrinlines.h"

#include "methodjit/MethodJIT.h"
#include "assembler/wtf/Platform.h"
#include "yarr/BumpPointerAllocator.h"

#include "yarr/Yarr.h"
#if ENABLE_YARR_JIT
#include "yarr/YarrJIT.h"
#else
#include "yarr/pcre/pcre.h"
#endif

namespace js {

/*
 * res = RegExp statics.
 */

extern Class regexp_statics_class;

static inline JSObject *
regexp_statics_construct(JSContext *cx, GlobalObject *parent)
{
    JSObject *obj = NewObject<WithProto::Given>(cx, &regexp_statics_class, NULL, parent);
    if (!obj)
        return NULL;
    RegExpStatics *res = cx->new_<RegExpStatics>();
    if (!res)
        return NULL;
    obj->setPrivate(static_cast<void *>(res));
    return obj;
}

/*
 * The "meat" of the builtin regular expression objects: it contains the
 * mini-program that represents the source of the regular expression. Excepting
 * refcounts, this is an immutable datastructure after compilation.
 *
 * Non-atomic refcounting is used, so single-thread invariants must be
 * maintained: we check regexp operations are performed in a single
 * compartment.
 *
 * Note: defined in the inlines header to avoid Yarr dependency includes in
 * main header.
 *
 * Note: refCount cannot overflow because that would require more referring
 * regexp objects than there is space for in addressable memory.
 */
class RegExp
{
#if ENABLE_YARR_JIT
    /* native code is valid only if codeBlock.isFallBack() == false */
    JSC::Yarr::YarrCodeBlock    codeBlock;
    JSC::Yarr::BytecodePattern  *byteCode;
#else
    JSRegExp                    *compiled;
#endif
    JSLinearString              *source;
    size_t                      refCount;
    unsigned                    parenCount; /* Must be |unsigned| to interface with YARR. */
    uint32                      flags;
#ifdef DEBUG
  public:
    JSCompartment               *compartment;

  private:
#endif

    RegExp(JSLinearString *source, uint32 flags, JSCompartment *compartment)
      :
#if ENABLE_YARR_JIT
        codeBlock(),
        byteCode(NULL),
#else
        compiled(NULL),
#endif
        source(source), refCount(1), parenCount(0), flags(flags)
#ifdef DEBUG
        , compartment(compartment)
#endif
    { }

    JS_DECLARE_ALLOCATION_FRIENDS_FOR_PRIVATE_CONSTRUCTOR;

    ~RegExp() {
#if ENABLE_YARR_JIT
        codeBlock.release();
        if (byteCode)
            Foreground::delete_<JSC::Yarr::BytecodePattern>(byteCode);
#else
        if (compiled)
            jsRegExpFree(compiled);
#endif
    }

    bool compileHelper(JSContext *cx, JSLinearString &pattern, TokenStream *ts);
    bool compile(JSContext *cx, TokenStream *ts);
    static const uint32 allFlags = JSREG_FOLD | JSREG_GLOB | JSREG_MULTILINE | JSREG_STICKY;
#if !ENABLE_YARR_JIT
    void reportPCREError(JSContext *cx, int error);
#endif
    void reportYarrError(JSContext *cx, TokenStream *ts, JSC::Yarr::ErrorCode error);
    static inline void checkMatchPairs(JSString *input, int *buf, size_t matchItemCount);
    static JSObject *createResult(JSContext *cx, JSString *input, int *buf, size_t matchItemCount);
    inline bool executeInternal(JSContext *cx, RegExpStatics *res, JSString *input,
                                size_t *lastIndex, bool test, Value *rval);

  public:
    static inline bool isMetaChar(jschar c);
    static inline bool hasMetaChars(const jschar *chars, size_t length);

    /*
     * Parse regexp flags. Report an error and return false if an invalid
     * sequence of flags is encountered (repeat/invalid flag).
     *
     * N.B. flagStr must be rooted.
     */
    static bool parseFlags(JSContext *cx, JSString *flagStr, uintN *flagsOut);

    /*
     * Execute regexp on |input| at |*lastIndex|.
     *
     * On match:    Update |*lastIndex| and RegExp class statics.
     *              Return true if test is true. Place an array in |*rval| if test is false.
     * On mismatch: Make |*rval| null.
     */
    bool execute(JSContext *cx, RegExpStatics *res, JSString *input, size_t *lastIndex, bool test,
                 Value *rval) {
        JS_ASSERT(res);
        return executeInternal(cx, res, input, lastIndex, test, rval);
    }

    bool executeNoStatics(JSContext *cx, JSString *input, size_t *lastIndex, bool test,
                          Value *rval) {
        return executeInternal(cx, NULL, input, lastIndex, test, rval);
    }

    /* Factories */

    static AlreadyIncRefed<RegExp> create(JSContext *cx, JSString *source, uint32 flags, TokenStream *ts);

    /* Would overload |create|, but |0| resolves ambiguously against pointer and uint. */
    static AlreadyIncRefed<RegExp> createFlagged(JSContext *cx, JSString *source, JSString *flags, TokenStream *ts);

    /*
     * Create an object with new regular expression internals.
     * @note    The context's regexp statics flags are OR'd into the provided flags,
     *          so this function is really meant for object creation during code
     *          execution, as opposed to during something like XDR.
     */
    static JSObject *createObject(JSContext *cx, RegExpStatics *res, const jschar *chars,
                                  size_t length, uint32 flags, TokenStream *ts);
    static JSObject *createObjectNoStatics(JSContext *cx, const jschar *chars, size_t length,
                                           uint32 flags, TokenStream *ts);
    static RegExp *extractFrom(JSObject *obj);

    /* Mutators */

    void incref(JSContext *cx);
    void decref(JSContext *cx);

    /* Accessors */

    JSLinearString *getSource() const { return source; }
    size_t getParenCount() const { return parenCount; }
    bool ignoreCase() const { return flags & JSREG_FOLD; }
    bool global() const { return flags & JSREG_GLOB; }
    bool multiline() const { return flags & JSREG_MULTILINE; }
    bool sticky() const { return flags & JSREG_STICKY; }

    const uint32 &getFlags() const {
        JS_ASSERT((flags & allFlags) == flags);
        return flags;
    }
};

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

/* RegExp inlines. */

inline void
RegExp::checkMatchPairs(JSString *input, int *buf, size_t matchItemCount)
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
RegExp::createResult(JSContext *cx, JSString *input, int *buf, size_t matchItemCount)
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

inline bool
RegExp::executeInternal(JSContext *cx, RegExpStatics *res, JSString *inputstr,
                        size_t *lastIndex, bool test, Value *rval)
{
    const size_t pairCount = parenCount + 1;
    const size_t bufCount = pairCount * 3; /* Should be x2, but PCRE has... needs. */
    const size_t matchItemCount = pairCount * 2;

    LifoAllocScope las(&cx->tempLifoAlloc());
    int *buf = cx->tempLifoAlloc().newArray<int>(bufCount);
    if (!buf)
        return false;

    /*
     * The JIT regexp procedure doesn't always initialize matchPair values.
     * Maybe we can make this faster by ensuring it does?
     */
    for (int *it = buf; it != buf + matchItemCount; ++it)
        *it = -1;
    
    JSLinearString *input = inputstr->ensureLinear(cx);
    if (!input)
        return false;

    JS::Anchor<JSString *> anchor(input);
    size_t len = input->length();
    const jschar *chars = input->chars();

    /* 
     * inputOffset emulates sticky mode by matching from this offset into the char buf and
     * subtracting the delta off at the end.
     */
    size_t inputOffset = 0;

    if (sticky()) {
        /* Sticky matches at the last index for the regexp object. */
        chars += *lastIndex;
        len -= *lastIndex;
        inputOffset = *lastIndex;
    }

    int result;
#if ENABLE_YARR_JIT
    if (!codeBlock.isFallBack())
        result = JSC::Yarr::execute(codeBlock, chars, *lastIndex - inputOffset, len, buf);
    else
        result = JSC::Yarr::interpret(byteCode, chars, *lastIndex - inputOffset, len, buf);
#else
    result = jsRegExpExecute(cx, compiled, chars, len, *lastIndex - inputOffset, buf, bufCount);
#endif
    if (result == -1) {
        *rval = NullValue();
        return true;
    }

#if !ENABLE_YARR_JIT
    if (result < 0) {
        reportPCREError(cx, result);
        return false;
    }
#endif

    /* 
     * Adjust buf for the inputOffset. Use of sticky is rare and the matchItemCount is small, so
     * just do another pass.
     */
    if (JS_UNLIKELY(inputOffset)) {
        for (size_t i = 0; i < matchItemCount; ++i)
            buf[i] = buf[i] < 0 ? -1 : buf[i] + inputOffset;
    }

    /* Make sure the populated contents of |buf| are sane values against |input|. */
    checkMatchPairs(input, buf, matchItemCount);

    if (res)
        res->updateFromMatch(cx, input, buf, matchItemCount);

    *lastIndex = buf[1];

    if (test) {
        *rval = BooleanValue(true);
        return true;
    }

    JSObject *array = createResult(cx, input, buf, matchItemCount);
    if (!array)
        return false;

    *rval = ObjectValue(*array);
    return true;
}

inline AlreadyIncRefed<RegExp>
RegExp::create(JSContext *cx, JSString *source, uint32 flags, TokenStream *ts)
{
    typedef AlreadyIncRefed<RegExp> RetType;
    JSLinearString *flatSource = source->ensureLinear(cx);
    if (!flatSource)
        return RetType(NULL);
    RegExp *self = cx->new_<RegExp>(flatSource, flags, cx->compartment);
    if (!self)
        return RetType(NULL);
    if (!self->compile(cx, ts)) {
        Foreground::delete_(self);
        return RetType(NULL);
    }
    return RetType(self);
}

inline JSObject *
RegExp::createObject(JSContext *cx, RegExpStatics *res, const jschar *chars, size_t length,
                     uint32 flags, TokenStream *ts)
{
    uint32 staticsFlags = res->getFlags();
    return createObjectNoStatics(cx, chars, length, flags | staticsFlags, ts);
}

inline JSObject *
RegExp::createObjectNoStatics(JSContext *cx, const jschar *chars, size_t length, uint32 flags, TokenStream *ts)
{
    JS_ASSERT((flags & allFlags) == flags);
    JSString *str = js_NewStringCopyN(cx, chars, length);
    if (!str)
        return NULL;
    /*
     * NewBuiltinClassInstance can GC before we store re in the private field
     * of the object. At that point the only reference to the source string
     * could be from the malloc-allocated GC-invisible re. So we must anchor.
     */
    JS::Anchor<JSString *> anchor(str);
    AlreadyIncRefed<RegExp> re = RegExp::create(cx, str, flags, ts);
    if (!re)
        return NULL;
    JSObject *obj = NewBuiltinClassInstance(cx, &RegExpClass);
    if (!obj || !obj->initRegExp(cx, re.get())) {
        re->decref(cx);
        return NULL;
    }
    return obj;
}

/*
 * This function should be deleted once we can. See bug 604774.
 */
static inline bool
EnableYarrJIT(JSContext *cx)
{
#if defined ANDROID && defined(JS_TRACER) && defined(JS_METHODJIT)
    return cx->traceJitEnabled || cx->methodJitEnabled;
#else
    return true;
#endif
}

inline bool
RegExp::compileHelper(JSContext *cx, JSLinearString &pattern, TokenStream *ts)
{
#if ENABLE_YARR_JIT
    JSC::Yarr::ErrorCode yarrError;
    JSC::Yarr::YarrPattern yarrPattern(pattern, ignoreCase(), multiline(), &yarrError);
    if (yarrError) {
        reportYarrError(cx, ts, yarrError);
        return false;
    }
    parenCount = yarrPattern.m_numSubpatterns;

#if defined(JS_METHODJIT)
    if (EnableYarrJIT(cx) && !yarrPattern.m_containsBackreferences) {
        bool ok = cx->compartment->ensureJaegerCompartmentExists(cx);
        if (!ok)
            return false;
        JSC::Yarr::JSGlobalData globalData(cx->compartment->jaegerCompartment()->execAlloc());
        JSC::Yarr::jitCompile(yarrPattern, &globalData, codeBlock);
        if (!codeBlock.isFallBack())
            return true;
    }
#endif

    codeBlock.setFallBack(true);
    byteCode = JSC::Yarr::byteCompile(yarrPattern, cx->compartment->regExpAllocator).get();
    return true;
#else
    int error = 0;
    compiled = jsRegExpCompile(pattern.chars(), pattern.length(),
        ignoreCase() ? JSRegExpIgnoreCase : JSRegExpDoNotIgnoreCase,
        multiline() ? JSRegExpMultiline : JSRegExpSingleLine,
        &parenCount, &error);
    if (error) {
        reportPCREError(cx, error);
        return false;
    }
    return true;
#endif
}

inline bool
RegExp::compile(JSContext *cx, TokenStream *ts)
{
    /* Flatten source early for the rest of compilation. */
    if (!source->ensureLinear(cx))
        return false;

    if (!sticky())
        return compileHelper(cx, *source, ts);

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
    return compileHelper(cx, *fakeySource, ts);
}

inline bool
RegExp::isMetaChar(jschar c)
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
RegExp::hasMetaChars(const jschar *chars, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        if (isMetaChar(chars[i]))
            return true;
    }
    return false;
}

inline void
RegExp::incref(JSContext *cx)
{
#ifdef DEBUG
    assertSameCompartment(cx, compartment);
#endif
    ++refCount;
}

inline void
RegExp::decref(JSContext *cx)
{
#ifdef DEBUG
    assertSameCompartment(cx, compartment);
#endif
    if (--refCount == 0)
        cx->delete_(this);
}

inline RegExp *
RegExp::extractFrom(JSObject *obj)
{
    JS_ASSERT_IF(obj, obj->isRegExp());
    RegExp *re = static_cast<RegExp *>(obj->getPrivate());
#ifdef DEBUG
    if (re)
        CompartmentChecker::check(obj->getCompartment(), re->compartment);
#endif
    return re;
}

/* RegExpStatics inlines. */

inline RegExpStatics *
RegExpStatics::extractFrom(js::GlobalObject *globalObj)
{
    Value resVal = globalObj->getRegExpStatics();
    RegExpStatics *res = static_cast<RegExpStatics *>(resVal.toObject().getPrivate());
    return res;
}

inline bool
RegExpStatics::createDependent(JSContext *cx, size_t start, size_t end, Value *out) const 
{
    JS_ASSERT(start <= end);
    JS_ASSERT(end <= matchPairsInput->length());
    JSString *str = js_NewDependentString(cx, matchPairsInput, start, end - start);
    if (!str)
        return false;
    *out = StringValue(str);
    return true;
}

inline bool
RegExpStatics::createPendingInput(JSContext *cx, Value *out) const
{
    out->setString(pendingInput ? pendingInput : cx->runtime->emptyString);
    return true;
}

inline bool
RegExpStatics::makeMatch(JSContext *cx, size_t checkValidIndex, size_t pairNum, Value *out) const
{
    if (checkValidIndex / 2 >= pairCount() || matchPairs[checkValidIndex] < 0) {
        out->setString(cx->runtime->emptyString);
        return true;
    }
    return createDependent(cx, get(pairNum, 0), get(pairNum, 1), out);
}

inline bool
RegExpStatics::createLastParen(JSContext *cx, Value *out) const
{
    if (pairCount() <= 1) {
        out->setString(cx->runtime->emptyString);
        return true;
    }
    size_t num = pairCount() - 1;
    int start = get(num, 0);
    int end = get(num, 1);
    if (start == -1) {
        out->setString(cx->runtime->emptyString);
        return true;
    }
    JS_ASSERT(start >= 0 && end >= 0);
    JS_ASSERT(end >= start);
    return createDependent(cx, start, end, out);
}

inline bool
RegExpStatics::createLeftContext(JSContext *cx, Value *out) const
{
    if (!pairCount()) {
        out->setString(cx->runtime->emptyString);
        return true;
    }
    if (matchPairs[0] < 0) {
        *out = UndefinedValue();
        return true;
    }
    return createDependent(cx, 0, matchPairs[0], out);
}

inline bool
RegExpStatics::createRightContext(JSContext *cx, Value *out) const
{
    if (!pairCount()) {
        out->setString(cx->runtime->emptyString);
        return true;
    }
    if (matchPairs[1] < 0) {
        *out = UndefinedValue();
        return true;
    }
    return createDependent(cx, matchPairs[1], matchPairsInput->length(), out);
}

inline void
RegExpStatics::getParen(size_t pairNum, JSSubString *out) const
{
    checkParenNum(pairNum);
    if (!pairIsPresent(pairNum)) {
        *out = js_EmptySubString;
        return;
    }
    out->chars = matchPairsInput->chars() + get(pairNum, 0);
    out->length = getParenLength(pairNum);
}

inline void
RegExpStatics::getLastMatch(JSSubString *out) const
{
    if (!pairCount()) {
        *out = js_EmptySubString;
        return;
    }
    JS_ASSERT(matchPairsInput);
    out->chars = matchPairsInput->chars() + get(0, 0);
    JS_ASSERT(get(0, 1) >= get(0, 0));
    out->length = get(0, 1) - get(0, 0);
}

inline void
RegExpStatics::getLastParen(JSSubString *out) const
{
    size_t pc = pairCount();
    /* Note: the first pair is the whole match. */
    if (pc <= 1) {
        *out = js_EmptySubString;
        return;
    }
    getParen(pc - 1, out);
}

inline void
RegExpStatics::getLeftContext(JSSubString *out) const
{
    if (!pairCount()) {
        *out = js_EmptySubString;
        return;
    }
    out->chars = matchPairsInput->chars();
    out->length = get(0, 0);
}

inline void
RegExpStatics::getRightContext(JSSubString *out) const
{
    if (!pairCount()) {
        *out = js_EmptySubString;
        return;
    }
    out->chars = matchPairsInput->chars() + get(0, 1);
    JS_ASSERT(get(0, 1) <= int(matchPairsInput->length()));
    out->length = matchPairsInput->length() - get(0, 1);
}

} // namespace js

inline bool
JSObject::initRegExp(JSContext *cx, js::RegExp *re)
{
    JS_ASSERT(isRegExp());

    /*
     * It's currently possible to swap RegExp guts. In that case this object
     * will already have the relevant properties, at the relevant locations.
     */
    if (nativeEmpty()) {
        const js::Shape **shapep = &cx->compartment->initialRegExpShape;
        if (!*shapep) {
            *shapep = assignInitialRegExpShape(cx);
            if (!*shapep)
                return false;
        }
        setLastProperty(*shapep);
        JS_ASSERT(!nativeEmpty());
    }

    JS_ASSERT(nativeLookup(cx, ATOM_TO_JSID(cx->runtime->atomState.lastIndexAtom))->slot ==
              JSObject::JSSLOT_REGEXP_LAST_INDEX);
    JS_ASSERT(nativeLookup(cx, ATOM_TO_JSID(cx->runtime->atomState.sourceAtom))->slot ==
              JSObject::JSSLOT_REGEXP_SOURCE);
    JS_ASSERT(nativeLookup(cx, ATOM_TO_JSID(cx->runtime->atomState.globalAtom))->slot ==
              JSObject::JSSLOT_REGEXP_GLOBAL);
    JS_ASSERT(nativeLookup(cx, ATOM_TO_JSID(cx->runtime->atomState.ignoreCaseAtom))->slot ==
              JSObject::JSSLOT_REGEXP_IGNORE_CASE);
    JS_ASSERT(nativeLookup(cx, ATOM_TO_JSID(cx->runtime->atomState.multilineAtom))->slot ==
              JSObject::JSSLOT_REGEXP_MULTILINE);
    JS_ASSERT(nativeLookup(cx, ATOM_TO_JSID(cx->runtime->atomState.stickyAtom))->slot ==
              JSObject::JSSLOT_REGEXP_STICKY);

    setPrivate(re);
    zeroRegExpLastIndex();
    setRegExpSource(re->getSource());
    setRegExpGlobal(re->global());
    setRegExpIgnoreCase(re->ignoreCase());
    setRegExpMultiline(re->multiline());
    setRegExpSticky(re->sticky());
    return true;
}

#endif /* jsregexpinlines_h___ */
