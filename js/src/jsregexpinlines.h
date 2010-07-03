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
#include "yarr/yarr/RegexJIT.h"
#include "nanojit/avmplus.h"

namespace js {

/* Defined in the inlines header to avoid Yarr dependency includes in main header. */
class RegExp
{
    jsrefcount                  refCount;
    JSString                    *source;
    JSC::Yarr::RegexCodeBlock   compiled;
    unsigned                    parenCount;
    uint32                      flags;

    RegExp(JSString *source, uint32 flags)
      : refCount(1), source(source), compiled(), parenCount(), flags(flags) {}
    bool compileHelper(JSContext *cx, const UString &pattern);
    bool compile(JSContext *cx);
    static const uint32 allFlags = JSREG_FOLD | JSREG_GLOB | JSREG_MULTILINE | JSREG_STICKY;
    void handlePCREError(JSContext *cx, int error);
    void handleYarrError(JSContext *cx, int error);

  public:
    ~RegExp() {}
    static bool hasMetaChars(const jschar *chars, size_t length);
    /*
     * Parse regexp flags. Report an error and return false if an invalid
     * sequence of flags is encountered (repeat/invalid flag).
     */
    static bool parseFlags(JSContext *cx, JSString *flagStr, uint32 &flagsOut);

    /* Factories. */

    static RegExp *create(JSContext *cx, JSString *source, uint32 flags);
    static RegExp *createFlagged(JSContext *cx, JSString *source, JSString *flags);
    /*
     * Create an object with new regular expression internals.
     * @note    The context's regexp statics flags are OR'd into the provided flags,
     *          so this function is really meant for object creation during code
     *          execution, as opposed to during something like XDR.
     */
    static JSObject *createObject(JSContext *cx, const jschar *chars, size_t length, uint32 flags);
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

    /*
     * Execute regexp on |input| at |*lastIndex|.
     *
     * On match:    Update |*lastIndex| and RegExp class statics.
     *              Return true if test is true. Place an array in |*rval| if test is false.
     * On mismatch: Make |*rval| null.
     */
    bool execute(JSContext *cx, JSString *input, size_t *lastIndex, bool test, jsval *rval);
};

/* RegExp inlines. */

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
RegExp::createObject(JSContext *cx, const jschar *chars, size_t length, uint32 flags)
{
    JS_ASSERT((flags & allFlags) == flags);
    JSString *str = js_NewStringCopyN(cx, chars, length);
    if (!str)
        return NULL;
    AutoValueRooter tvr(cx, str);
    uint32 staticsFlags = cx->regExpStatics.getFlags();
    JS_ASSERT((staticsFlags & allFlags) == staticsFlags);
    RegExp *re = RegExp::create(cx, str, flags | staticsFlags);
    if (!re)
        return NULL;
    JSObject *obj = NewObject(cx, &js_RegExpClass, NULL, NULL);
    if (!obj) {
        re->decref(cx);
        return NULL;
    }
    obj->setPrivate(re);
    obj->zeroRegExpLastIndex();
    return obj;
}

inline bool
RegExp::compileHelper(JSContext *cx, const UString &pattern)
{
    bool fellBack = false;
    int error = 0;
    jitCompileRegex(*cx->runtime->regExpAllocator, compiled, pattern, parenCount, error, fellBack, ignoreCase(), multiline());
    if (!error)
        return true;
    if (fellBack)
        handlePCREError(cx, error);
    else
        handleYarrError(cx, error);
    return false;
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
    JSString *fakeySource = js_ConcatStringsZ(cx, "^", source);
    AutoValueRooter rooter(cx, fakeySource);
    return compileHelper(cx, *fakeySource);
}

inline bool
RegExp::hasMetaChars(const jschar *chars, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        jschar c = chars[i];
        switch (c) {
          /* Taken from the PatternCharacter production in 15.10.1. */
          case '^': case '$': case '\\': case '.': case '*': case '+':
          case '?': case '(': case ')': case '[': case ']': case '{':
          case '}': case '|':
            return true;
          default:;
        }
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

inline RegExpStatics::MatchPairs *
RegExpStatics::getOrCreateMatchPairs(JSString *newInput)
{
    input = newInput;
    if (ownsMatchPairs) {
        if (matchPairs)
            matchPairs->clear();
        else
            matchPairs = cx->create<MatchPairs>(cx);
    } else {
        matchPairs = cx->create<MatchPairs>(cx);
        ownsMatchPairs = false;
    }
    checkInvariants();
    return matchPairs;
}

inline void
RegExpStatics::restore(RegExpStatics &container)
{
    checkInvariants();
    input = container.input;
    flags = container.flags;
    JS_ASSERT((flags & allFlags) == flags);
    matchPairs = container.matchPairs;
    ownsMatchPairs = container.ownsMatchPairs;
    container.ownsMatchPairs = false;
    checkInvariants();
}

inline void
RegExpStatics::save(RegExpStatics &container)
{
    checkInvariants();
    JS_ASSERT(this != &container);
    container.input = input;
    container.flags = flags;
    JS_ASSERT((flags & allFlags) == flags);
    container.matchPairs = matchPairs;
    JS_ASSERT_IF(matchPairs, matchPairs->length() == container.matchPairs->length());
    container.ownsMatchPairs = ownsMatchPairs;
    ownsMatchPairs = false;
    checkInvariants();
}

inline void
RegExpStatics::saveAndClear(RegExpStatics &container)
{
    save(container);
    clear();
    checkInvariants();
}

inline bool
RegExpStatics::createDependent(size_t start, size_t end, jsval &out) const 
{
    JS_ASSERT(start <= end);
    JS_ASSERT(end <= input->length());
    JSString *str = js_NewDependentString(cx, input, start, end - start);
    if (!str)
        return false;
    out = STRING_TO_JSVAL(str);
    return true;
}

inline bool
RegExpStatics::createInput(jsval &out) const
{
    out = input ? STRING_TO_JSVAL(input) : JS_GetEmptyStringValue(cx);
    return true;
}

inline bool
RegExpStatics::makeMatch(size_t checkValidIndex, size_t pairNum, jsval &out) const
{
    if (checkValidIndex / 2 >= pairCount() || (*matchPairs)[checkValidIndex] < 0) {
        out = JS_GetEmptyStringValue(cx);
        return true;
    }
    return createDependent(get(pairNum, 0), get(pairNum, 1), out);
}

inline bool
RegExpStatics::createLastParen(jsval &out) const
{
    if (pairCount() <= 1) {
        out = JS_GetEmptyStringValue(cx);
        return true;
    }
    size_t num = pairCount() - 1;
    int start = get(num, 0);
    int end = get(num, 1);
    if (start == -1) {
        JS_ASSERT(end == -1);
        out = JS_GetEmptyStringValue(cx);
        return true;
    }
    JS_ASSERT(start >= 0 && end >= 0);
    return createDependent(start, end, out);
}

inline bool
RegExpStatics::createLeftContext(jsval &out) const
{
    if (!pairCount()) {
        out = JS_GetEmptyStringValue(cx);
        return true;
    }
    if ((*matchPairs)[0] < 0) {
        out = JSVAL_VOID;
        return true;
    }
    return createDependent(0, (*matchPairs)[0], out);
}

inline bool
RegExpStatics::createRightContext(jsval &out) const
{
    if (!pairCount()) {
        out = JS_GetEmptyStringValue(cx);
        return true;
    }
    if ((*matchPairs)[1] < 0) {
        out = JSVAL_VOID;
        return true;
    }
    return createDependent((*matchPairs)[1], input->length(), out);
}

inline void
RegExpStatics::getParen(size_t num, JSSubString &out) const
{
    out.chars = input->chars() + get(num + 1, 0);
    out.length = getParenLength(num);
}

inline void
RegExpStatics::getLastMatch(JSSubString &out) const
{
    if (!pairCount()) {
        out = js_EmptySubString;
        return;
    }
    JS_ASSERT(input);
    out.chars = input->chars() + get(0, 0);
    JS_ASSERT(get(0, 1) >= get(0, 0));
    out.length = get(0, 1) - get(0, 0);
}

inline void
RegExpStatics::getLastParen(JSSubString &out) const
{
    if (!pairCount()) {
        out = js_EmptySubString;
        return;
    }
    size_t num = pairCount() - 1;
    out.chars = input->chars() + get(num, 0);
    JS_ASSERT(get(num, 1) >= get(num, 0));
    out.length = get(num, 1) - get(num, 0);
}

inline void
RegExpStatics::getLeftContext(JSSubString &out) const
{
    if (!pairCount()) {
        out = js_EmptySubString;
        return;
    }
    out.chars = input->chars();
    out.length = get(0, 0);
}

inline void
RegExpStatics::getRightContext(JSSubString &out) const
{
    if (!pairCount()) {
        out = js_EmptySubString;
        return;
    }
    out.chars = input->chars() + get(0, 1);
    JS_ASSERT(get(0, 1) <= int(input->length()));
    out.length = input->length() - get(0, 1);
}

}

#endif /* jsregexpinlines_h___ */
