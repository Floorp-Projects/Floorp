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

#include "jsregexp.h"
#include "jscntxt.h"
#include "jsobjinlines.h"
#include "assembler/wtf/Platform.h"

#if ENABLE_YARR_JIT
#include "yarr/yarr/RegexJIT.h"
#else
#include "yarr/pcre/pcre.h"
#endif

namespace js {

/*
 * res = RegExp statics.
 */

extern Class regexp_statics_class;

static inline JSObject *
regexp_statics_construct(JSContext *cx, JSObject *parent)
{
    JSObject *obj = NewObject<WithProto::Given>(cx, &regexp_statics_class, NULL, parent);
    if (!obj)
        return NULL;
    RegExpStatics *res = cx->create<RegExpStatics>();
    if (!res)
        return NULL;
    obj->setPrivate(static_cast<void *>(res));
    return obj;
}

/* Defined in the inlines header to avoid Yarr dependency includes in main header. */
class RegExp
{
    jsrefcount                  refCount;
    JSString                    *source;
#if ENABLE_YARR_JIT
    JSC::Yarr::RegexCodeBlock   compiled;
#else
    JSRegExp                    *compiled;
#endif
    unsigned                    parenCount;
    uint32                      flags;

    RegExp(JSString *source, uint32 flags)
      : refCount(1), source(source), compiled(), parenCount(), flags(flags) {}
    bool compileHelper(JSContext *cx, UString &pattern);
    bool compile(JSContext *cx);
    static const uint32 allFlags = JSREG_FOLD | JSREG_GLOB | JSREG_MULTILINE | JSREG_STICKY;
    void handlePCREError(JSContext *cx, int error);
    void handleYarrError(JSContext *cx, int error);
    static inline bool initArena(JSContext *cx);
    static inline void checkMatchPairs(int *buf, size_t matchItemCount);
    JSObject *createResult(JSContext *cx, JSString *input, int *buf, size_t matchItemCount,
                           size_t inputOffset);
    inline bool executeInternal(JSContext *cx, RegExpStatics *res, JSString *input,
                                size_t *lastIndex, bool test, Value *rval);

  public:
    ~RegExp() {
#if !ENABLE_YARR_JIT
        if (compiled)
            jsRegExpFree(compiled);
#endif
    }

    static bool isMetaChar(jschar c);
    static bool hasMetaChars(const jschar *chars, size_t length);

    /*
     * Parse regexp flags. Report an error and return false if an invalid
     * sequence of flags is encountered (repeat/invalid flag).
     */
    static bool parseFlags(JSContext *cx, JSString *flagStr, uint32 &flagsOut);

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

    /* Factories. */
    static RegExp *create(JSContext *cx, JSString *source, uint32 flags);
    static RegExp *createFlagged(JSContext *cx, JSString *source, JSString *flags);
    /*
     * Create an object with new regular expression internals.
     * @note    The context's regexp statics flags are OR'd into the provided flags,
     *          so this function is really meant for object creation during code
     *          execution, as opposed to during something like XDR.
     */
    static JSObject *createObject(JSContext *cx, RegExpStatics *res, const jschar *chars,
                                  size_t length, uint32 flags);
    static JSObject *createObjectNoStatics(JSContext *cx, const jschar *chars, size_t length,
                                           uint32 flags);
    static RegExp *extractFrom(JSObject *obj);
    static RegExp *clone(JSContext *cx, const RegExp &other);

    /* Mutators. */
    void incref(JSContext *cx) { JS_ATOMIC_INCREMENT(&refCount); }
    void decref(JSContext *cx);

    /* Accessors. */
    JSString *getSource() const { return source; }
    size_t getParenCount() const { return parenCount; }
    bool ignoreCase() const { return flags & JSREG_FOLD; }
    bool global() const { return flags & JSREG_GLOB; }
    bool multiline() const { return flags & JSREG_MULTILINE; }
    bool sticky() const { return flags & JSREG_STICKY; }
    const uint32 &getFlags() const { JS_ASSERT((flags & allFlags) == flags); return flags; }
    uint32 flagCount() const;
};

class RegExpMatchBuilder
{
    JSContext   * const cx;
    JSObject    * const array;

  public:
    RegExpMatchBuilder(JSContext *cx, JSObject *array) : cx(cx), array(array) {}

    bool append(int index, JSString *str) {
        JS_ASSERT(str);
        return append(INT_TO_JSID(index), StringValue(str));
    }

    bool append(jsid id, Value val) {
        return !!js_DefineProperty(cx, array, id, &val, js::PropertyStub, js::PropertyStub,
                                   JSPROP_ENUMERATE);
    }

    bool appendIndex(int index) {
        return append(ATOM_TO_JSID(cx->runtime->atomState.indexAtom), Int32Value(index));
    }

    /* Sets the input attribute of the match array. */
    bool appendInput(JSString *str) {
        JS_ASSERT(str);
        return append(ATOM_TO_JSID(cx->runtime->atomState.inputAtom), StringValue(str));
    }
};

/* RegExp inlines. */

inline bool
RegExp::initArena(JSContext *cx)
{
    if (cx->regExpPool.first.next)
        return true;

    /*
     * The regular expression arena pool is special... we want to hang on to it
     * until a GC is performed so rapid subsequent regexp executions don't
     * thrash malloc/freeing arena chunks.
     *
     * Stick a timestamp at the base of that pool.
     */
    int64 *timestamp;
    JS_ARENA_ALLOCATE_CAST(timestamp, int64 *, &cx->regExpPool, sizeof *timestamp);
    if (!timestamp)
        return false;
    *timestamp = JS_Now();
    return true;
}

inline void
RegExp::checkMatchPairs(int *buf, size_t matchItemCount)
{
#if DEBUG
    for (size_t i = 0; i < matchItemCount; i += 2)
        JS_ASSERT(buf[i + 1] >= buf[i]); /* Limit index must be larger than the start index. */
#endif
}

inline JSObject *
RegExp::createResult(JSContext *cx, JSString *input, int *buf, size_t matchItemCount,
                     size_t inputOffset)
{
#define MATCH_VALUE(__index) (buf[(__index)] + inputOffset)
    /*
     * Create the result array for a match. Array contents:
     *  0:              matched string
     *  1..parenCount:  paren matches
     */
    JSObject *array = js_NewSlowArrayObject(cx);
    if (!array)
        return NULL;

    RegExpMatchBuilder builder(cx, array);
    for (size_t i = 0; i < matchItemCount; i += 2) {
        int start = MATCH_VALUE(i);
        int end = MATCH_VALUE(i + 1);

        JSString *captured;
        if (start >= 0) {
            JS_ASSERT(start <= end);
            JS_ASSERT((unsigned) end <= input->length());
            captured = js_NewDependentString(cx, input, start, end - start);
            if (!(captured && builder.append(i / 2, captured)))
                return NULL;
        } else {
            /* Missing parenthesized match. */
            JS_ASSERT(i != 0); /* Since we had a match, first pair must be present. */
            JS_ASSERT(start == end && end == -1);
            if (!builder.append(INT_TO_JSID(i / 2), UndefinedValue()))
                return NULL;
        }
    }

    if (!builder.appendIndex(MATCH_VALUE(0)) ||
        !builder.appendInput(input))
        return NULL;

    return array;
#undef MATCH_VALUE
}

inline bool
RegExp::executeInternal(JSContext *cx, RegExpStatics *res, JSString *input,
                        size_t *lastIndex, bool test, Value *rval)
{
#if !ENABLE_YARR_JIT
    JS_ASSERT(compiled);
#endif
    const size_t pairCount = parenCount + 1;
    const size_t bufCount = pairCount * 3; /* Should be x2, but PCRE has... needs. */
    const size_t matchItemCount = pairCount * 2;

    if (!initArena(cx))
        return false;

    AutoArenaAllocator aaa(&cx->regExpPool);
    int *buf = aaa.alloc<int>(bufCount);
    if (!buf)
        return false;

    /*
     * The JIT regexp procedure doesn't always initialize matchPair values.
     * Maybe we can make this faster by ensuring it does?
     */
    for (int *it = buf; it != buf + matchItemCount; ++it)
        *it = -1;

    const jschar *chars = input->chars();
    size_t len = input->length();
    size_t inputOffset = 0;

    if (sticky()) {
        /* Sticky matches at the last index for the regexp object. */
        chars += *lastIndex;
        len -= *lastIndex;
        inputOffset = *lastIndex;
    }

#if ENABLE_YARR_JIT
    int result = JSC::Yarr::executeRegex(cx, compiled, chars, *lastIndex - inputOffset, len, buf,
                                         bufCount);
#else
    int result = jsRegExpExecute(cx, compiled, chars, len, *lastIndex - inputOffset, buf, 
                                 bufCount) < 0 ? -1 : buf[0];
#endif
    if (result == -1) {
        *rval = NullValue();
        return true;
    }

    checkMatchPairs(buf, matchItemCount);

    if (res) {
        res->aboutToWrite();
        res->input = input;
        if (!res->matchPairs.resizeUninitialized(matchItemCount)) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        for (size_t i = 0; i < matchItemCount; ++i)
            res->matchPairs[i] = buf[i] + inputOffset;
    }

    *lastIndex = buf[1] + inputOffset;

    if (test) {
        *rval = BooleanValue(true);
        return true;
    }

    JSObject *array = createResult(cx, input, buf, matchItemCount, inputOffset);
    if (!array)
        return false;

    *rval = ObjectValue(*array);
    return true;
}

inline RegExp *
RegExp::create(JSContext *cx, JSString *source, uint32 flags)
{
    RegExp *self;
    void *mem = cx->malloc(sizeof(*self));
    if (!mem)
        return NULL;
    self = new (mem) RegExp(source, flags);
    if (!self->compile(cx)) {
        cx->destroy<RegExp>(self);
        return NULL;
    }
    return self;
}

inline JSObject *
RegExp::createObject(JSContext *cx, RegExpStatics *res, const jschar *chars, size_t length,
                     uint32 flags)
{
    uint32 staticsFlags = res->getFlags();
    return createObjectNoStatics(cx, chars, length, flags | staticsFlags);
}

inline JSObject *
RegExp::createObjectNoStatics(JSContext *cx, const jschar *chars, size_t length, uint32 flags)
{
    JS_ASSERT((flags & allFlags) == flags);
    JSString *str = js_NewStringCopyN(cx, chars, length);
    if (!str)
        return NULL;
    RegExp *re = RegExp::create(cx, str, flags);
    if (!re)
        return NULL;
    JSObject *obj = NewBuiltinClassInstance(cx, &js_RegExpClass);
    if (!obj) {
        re->decref(cx);
        return NULL;
    }
    obj->setPrivate(re);
    obj->zeroRegExpLastIndex();
    return obj;
}

#ifdef ANDROID
static bool
YarrJITIsBroken(JSContext *cx)
{
#if defined(JS_TRACER) && defined(JS_METHODJIT)
    /* FIXME/bug 604774: dead code walking.
     *
     * If both JITs are disabled, assume they were disabled because
     * we're running on a blacklisted device.
     */
    return !cx->traceJitEnabled && !cx->methodJitEnabled;
#else
    return false;
#endif
}
#endif  /* ANDROID */

inline bool
RegExp::compileHelper(JSContext *cx, UString &pattern)
{
#if ENABLE_YARR_JIT
    bool fellBack = false;
    int error = 0;
    jitCompileRegex(*cx->runtime->regExpAllocator, compiled, pattern, parenCount, error, fellBack, ignoreCase(), multiline()
#ifdef ANDROID
                    /* Temporary gross hack to work around buggy kernels. */
                    , YarrJITIsBroken(cx)
#endif
);
    if (!error)
        return true;
    if (fellBack)
        handlePCREError(cx, error);
    else
        handleYarrError(cx, error);
    return false;
#else
    int error = 0;
    compiled = jsRegExpCompile(pattern.chars(), pattern.length(),
                               ignoreCase() ? JSRegExpIgnoreCase : JSRegExpDoNotIgnoreCase,
                               multiline() ? JSRegExpMultiline : JSRegExpSingleLine,
                               &parenCount, &error);
    if (!error)
        return true;
    handlePCREError(cx, error);
    return false;
#endif
}

inline bool
RegExp::compile(JSContext *cx)
{
    if (!sticky())
        return compileHelper(cx, *source);
    /*
     * The sticky case we implement hackily by prepending a caret onto the front
     * and relying on |::execute| to pseudo-slice the string when it sees a sticky regexp.
     */
    static const jschar prefix[] = {'^', '(', '?', ':'};
    static const jschar postfix[] = {')'};

    JSCharBuffer cb(cx);
    if (!cb.reserve(JS_ARRAY_LENGTH(prefix) + source->length() + JS_ARRAY_LENGTH(postfix)))
        return false;
    JS_ALWAYS_TRUE(cb.append(prefix, JS_ARRAY_LENGTH(prefix)));
    JS_ALWAYS_TRUE(cb.append(source->chars(), source->length()));
    JS_ALWAYS_TRUE(cb.append(postfix, JS_ARRAY_LENGTH(postfix)));

    JSString *fakeySource = js_NewStringFromCharBuffer(cx, cb);
    if (!fakeySource)
        return false;
    return compileHelper(cx, *fakeySource);
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

inline uint32
RegExp::flagCount() const
{
    uint32 nflags = 0;
    for (uint32 tmpFlags = flags; tmpFlags != 0; tmpFlags &= tmpFlags - 1)
        nflags++;
    return nflags;
}

inline void
RegExp::decref(JSContext *cx)
{
    if (JS_ATOMIC_DECREMENT(&refCount) == 0)
        cx->destroy<RegExp>(this);
}

inline RegExp *
RegExp::extractFrom(JSObject *obj)
{
    JS_ASSERT_IF(obj, obj->isRegExp());
    return static_cast<RegExp *>(obj->getPrivate());
}

inline RegExp *
RegExp::clone(JSContext *cx, const RegExp &other)
{
    return create(cx, other.source, other.flags);
}

/* RegExpStatics inlines. */

inline RegExpStatics *
RegExpStatics::extractFrom(JSObject *global)
{
    Value resVal = global->getReservedSlot(JSRESERVED_GLOBAL_REGEXP_STATICS);
    RegExpStatics *res = static_cast<RegExpStatics *>(resVal.toObject().getPrivate());
    return res;
}

inline bool
RegExpStatics::createDependent(JSContext *cx, size_t start, size_t end, Value *out) const 
{
    JS_ASSERT(start <= end);
    JS_ASSERT(end <= input->length());
    JSString *str = js_NewDependentString(cx, input, start, end - start);
    if (!str)
        return false;
    *out = StringValue(str);
    return true;
}

inline bool
RegExpStatics::createInput(JSContext *cx, Value *out) const
{
    out->setString(input ? input : cx->runtime->emptyString);
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
        JS_ASSERT(end == -1);
        out->setString(cx->runtime->emptyString);
        return true;
    }
    JS_ASSERT(start >= 0 && end >= 0);
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
    return createDependent(cx, matchPairs[1], input->length(), out);
}

inline void
RegExpStatics::getParen(size_t num, JSSubString *out) const
{
    out->chars = input->chars() + get(num + 1, 0);
    out->length = getParenLength(num);
}

inline void
RegExpStatics::getLastMatch(JSSubString *out) const
{
    if (!pairCount()) {
        *out = js_EmptySubString;
        return;
    }
    JS_ASSERT(input);
    out->chars = input->chars() + get(0, 0);
    JS_ASSERT(get(0, 1) >= get(0, 0));
    out->length = get(0, 1) - get(0, 0);
}

inline void
RegExpStatics::getLastParen(JSSubString *out) const
{
    if (!pairCount()) {
        *out = js_EmptySubString;
        return;
    }
    size_t num = pairCount() - 1;
    out->chars = input->chars() + get(num, 0);
    JS_ASSERT(get(num, 1) >= get(num, 0));
    out->length = get(num, 1) - get(num, 0);
}

inline void
RegExpStatics::getLeftContext(JSSubString *out) const
{
    if (!pairCount()) {
        *out = js_EmptySubString;
        return;
    }
    out->chars = input->chars();
    out->length = get(0, 0);
}

inline void
RegExpStatics::getRightContext(JSSubString *out) const
{
    if (!pairCount()) {
        *out = js_EmptySubString;
        return;
    }
    out->chars = input->chars() + get(0, 1);
    JS_ASSERT(get(0, 1) <= int(input->length()));
    out->length = input->length() - get(0, 1);
}

}

#endif /* jsregexpinlines_h___ */
