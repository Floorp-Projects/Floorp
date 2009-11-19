/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef jsstr_h___
#define jsstr_h___
/*
 * JS string type implementation.
 *
 * A JS string is a counted array of unicode characters.  To support handoff
 * of API client memory, the chars are allocated separately from the length,
 * necessitating a pointer after the count, to form a separately allocated
 * string descriptor.  String descriptors are GC'ed, while their chars are
 * allocated from the malloc heap.
 */
#include <ctype.h>
#include "jspubtd.h"
#include "jsprvtd.h"
#include "jslock.h"

JS_BEGIN_EXTERN_C

#define JSSTRING_BIT(n)             ((size_t)1 << (n))
#define JSSTRING_BITMASK(n)         (JSSTRING_BIT(n) - 1)

class TraceRecorder;

enum {
    UNIT_STRING_LIMIT        = 256U,
    INT_STRING_LIMIT         = 256U
};

extern jschar *
js_GetDependentStringChars(JSString *str);

JS_STATIC_ASSERT(JS_BITS_PER_WORD >= 32);

/*
 * The GC-thing "string" type.
 *
 * When the DEPENDENT bit of the mLength field is unset, the mChars field
 * points to a flat character array owned by its GC-thing descriptor.  The
 * array is terminated at index length by a zero character and the size of the
 * array in bytes is (length + 1) * sizeof(jschar). The terminator is purely a
 * backstop, in case the chars pointer flows out to native code that requires
 * \u0000 termination.
 *
 * A flat string with the MUTABLE flag means that the string is accessible only
 * from one thread and it is possible to turn it into a dependent string of the
 * same length to optimize js_ConcatStrings. It is also possible to grow such a
 * string, but extreme care must be taken to ensure that no other code relies
 * on the original length of the string.
 *
 * A flat string with the ATOMIZED flag means that the string is hashed as
 * an atom. This flag is used to avoid re-hashing the already-atomized string.
 *
 * Any string with the DEFLATED flag means that the string has an entry in the
 * deflated string cache. The GC uses this flag to optimize string finalization
 * and avoid an expensive cache lookup for strings that were never deflated.
 *
 * When the DEPENDENT flag is set, the string depends on characters of another
 * string strongly referenced by the mBase field. The base member may point to
 * another dependent string if chars() has not been called yet.
 *
 * The PREFIX flag determines the kind of the dependent string. When the flag
 * is unset, the mLength field encodes both starting position relative to the
 * base string and the number of characters in the dependent string, see
 * DEPENDENT_START_MASK and DEPENDENT_LENGTH_MASK below for details.
 *
 * When the PREFIX flag is set, the dependent string is a prefix of the base
 * string. The number of characters in the prefix is encoded using all non-flag
 * bits of the mLength field and spans the same 0 .. SIZE_T_MAX/4 range as the
 * length of the flat string.
 *
 * NB: Always use the length() and chars() accessor methods.
 */
struct JSString {
    friend class TraceRecorder;

    friend JSAtom *
    js_AtomizeString(JSContext *cx, JSString *str, uintN flags);

    friend JSString * JS_FASTCALL
    js_ConcatStrings(JSContext *cx, JSString *left, JSString *right);

    size_t          mLength;
    union {
        jschar      *mChars;
        JSString    *mBase;
    };

    /*
     * Definitions for flags stored in the high order bits of mLength.
     *
     * PREFIX and MUTABLE are two aliases for the same bit.  PREFIX should be
     * used only if DEPENDENT is set and MUTABLE should be used only if the
     * string is flat.
     *
     * ATOMIZED is used only with flat, immutable strings.
     */
    static const size_t DEPENDENT =     JSSTRING_BIT(JS_BITS_PER_WORD - 1);
    static const size_t PREFIX =        JSSTRING_BIT(JS_BITS_PER_WORD - 2);
    static const size_t MUTABLE =       PREFIX;
    static const size_t ATOMIZED =      JSSTRING_BIT(JS_BITS_PER_WORD - 3);
    static const size_t DEFLATED =      JSSTRING_BIT(JS_BITS_PER_WORD - 4);
#if JS_BITS_PER_WORD > 32
    static const size_t LENGTH_BITS =   28;
#else
    static const size_t LENGTH_BITS =   JS_BITS_PER_WORD - 4;
#endif
    static const size_t LENGTH_MASK =   JSSTRING_BITMASK(LENGTH_BITS);
    static const size_t DEPENDENT_LENGTH_BITS = 8;
    static const size_t DEPENDENT_LENGTH_MASK = JSSTRING_BITMASK(DEPENDENT_LENGTH_BITS);
    static const size_t DEPENDENT_START_BITS =  LENGTH_BITS - DEPENDENT_LENGTH_BITS;
    static const size_t DEPENDENT_START_SHIFT = DEPENDENT_LENGTH_BITS;
    static const size_t DEPENDENT_START_MASK =  JSSTRING_BITMASK(DEPENDENT_START_BITS);

    bool hasFlag(size_t flag) const {
        return (mLength & flag) != 0;
    }

  public:
    static const size_t MAX_LENGTH = LENGTH_MASK;
    static const size_t MAX_DEPENDENT_START = DEPENDENT_START_MASK;
    static const size_t MAX_DEPENDENT_LENGTH = DEPENDENT_LENGTH_MASK;

    bool isDependent() const {
        return hasFlag(DEPENDENT);
    }

    bool isFlat() const {
        return !isDependent();
    }

    bool isDeflated() const {
        return hasFlag(DEFLATED);
    }

    void setDeflated() {
        JS_ATOMIC_SET_MASK((jsword *) &mLength, DEFLATED);
    }

    bool isMutable() const {
        return !isDependent() && hasFlag(MUTABLE);
    }

    bool isAtomized() const {
        return !isDependent() && hasFlag(ATOMIZED);
    }

    JS_ALWAYS_INLINE jschar *chars() {
        return isDependent() ? dependentChars() : flatChars();
    }

    JS_ALWAYS_INLINE size_t length() const {
        return isDependent() ? dependentLength() : flatLength();
    }

    JS_ALWAYS_INLINE bool empty() const {
        return length() == 0;
    }

    JS_ALWAYS_INLINE void getCharsAndLength(const jschar *&chars, size_t &length) {
        if (isDependent()) {
            length = dependentLength();
            chars = dependentChars();
        } else {
            length = flatLength();
            chars = flatChars();
        }
    }

    JS_ALWAYS_INLINE void getCharsAndEnd(const jschar *&chars, const jschar *&end) {
        end = isDependent()
              ? dependentLength() + (chars = dependentChars())
              : flatLength() + (chars = flatChars());
    }

    /* Specific flat string initializer and accessor methods. */
    void initFlat(jschar *chars, size_t length) {
        JS_ASSERT(length <= MAX_LENGTH);
        mLength = length;
        mChars = chars;
    }

    jschar *flatChars() const {
        JS_ASSERT(isFlat());
        return mChars;
    }

    size_t flatLength() const {
        JS_ASSERT(isFlat());
        return mLength & LENGTH_MASK;
    }

    /*
     * Special flat string initializer that preserves the JSSTR_DEFLATED flag.
     * Use this method when reinitializing an existing string which may be
     * hashed to its deflated bytes. Newborn strings must use initFlat.
     */
    void reinitFlat(jschar *chars, size_t length) {
        JS_ASSERT(length <= MAX_LENGTH);
        mLength = (mLength & DEFLATED) | (length & ~DEFLATED);
        mChars = chars;
    }

    /*
     * Methods to manipulate atomized and mutable flags of flat strings. It is
     * safe to use these without extra locking due to the following properties:
     *
     *   * We do not have a flatClearAtomized method, as a string remains
     *     atomized until the GC collects it.
     *
     *   * A thread may call flatSetMutable only when it is the only
     *     thread accessing the string until a later call to
     *     flatClearMutable.
     *
     *   * Multiple threads can call flatClearMutable but the function actually
     *     clears the mutable flag only when the flag is set -- in which case
     *     only one thread can access the string (see previous property).
     *
     * Thus, when multiple threads access the string, JSString::flatSetAtomized
     * is the only function that can update the mLength field of the string by
     * changing the mutable bit from 0 to 1. We call the method only after the
     * string has been hashed. When some threads in js_ValueToStringId see that
     * the flag is set, it knows that the string was atomized.
     *
     * On the other hand, if the thread sees that the flag is unset, it could
     * be seeing a stale value when another thread has just atomized the string
     * and set the flag. But this can lead only to an extra call to
     * js_AtomizeString.  This function would find that the string was already
     * hashed and return it with the atomized bit set.
     */
    void flatSetAtomized() {
        JS_ASSERT(isFlat() && !isMutable());
        JS_STATIC_ASSERT(sizeof(mLength) == sizeof(jsword));
        JS_ATOMIC_SET_MASK((jsword *) &mLength, ATOMIZED);
    }

    void flatSetMutable() {
        JS_ASSERT(isFlat() && !isAtomized());
        mLength |= MUTABLE;
    }

    void flatClearMutable() {
        JS_ASSERT(isFlat());
        if (hasFlag(MUTABLE))
            mLength &= ~MUTABLE;
    }

    void initDependent(JSString *bstr, size_t off, size_t len) {
        JS_ASSERT(off <= MAX_DEPENDENT_START);
        JS_ASSERT(len <= MAX_DEPENDENT_LENGTH);
        mLength = DEPENDENT | (off << DEPENDENT_START_SHIFT) | len;
        mBase = bstr;
    }

    /* See JSString::reinitFlat. */
    void reinitDependent(JSString *bstr, size_t off, size_t len) {
        JS_ASSERT(off <= MAX_DEPENDENT_START);
        JS_ASSERT(len <= MAX_DEPENDENT_LENGTH);
        mLength = DEPENDENT | (mLength & DEFLATED) | (off << DEPENDENT_START_SHIFT) | len;
        mBase = bstr;
    }

    JSString *dependentBase() const {
        JS_ASSERT(isDependent());
        return mBase;
    }

    bool dependentIsPrefix() const {
        JS_ASSERT(isDependent());
        return hasFlag(PREFIX);
    }

    JS_ALWAYS_INLINE jschar *dependentChars() {
        return dependentBase()->isDependent()
               ? js_GetDependentStringChars(this)
               : dependentBase()->flatChars() + dependentStart();
    }

    JS_ALWAYS_INLINE size_t dependentStart() const {
        return dependentIsPrefix()
               ? 0
               : ((mLength >> DEPENDENT_START_SHIFT) & DEPENDENT_START_MASK);
    }

    JS_ALWAYS_INLINE size_t dependentLength() const {
        JS_ASSERT(isDependent());
        if (dependentIsPrefix())
            return mLength & LENGTH_MASK;
        return mLength & DEPENDENT_LENGTH_MASK;
    }

    void initPrefix(JSString *bstr, size_t len) {
        JS_ASSERT(len <= MAX_LENGTH);
        mLength = DEPENDENT | PREFIX | len;
        mBase = bstr;
    }

    /* See JSString::reinitFlat. */
    void reinitPrefix(JSString *bstr, size_t len) {
        JS_ASSERT(len <= MAX_LENGTH);
        mLength = DEPENDENT | PREFIX | (mLength & DEFLATED) | len;
        mBase = bstr;
    }

    JSString *prefixBase() const {
        JS_ASSERT(isDependent() && dependentIsPrefix());
        return dependentBase();
    }

    void prefixSetBase(JSString *bstr) {
        JS_ASSERT(isDependent() && dependentIsPrefix());
        mBase = bstr;
    }

    static inline bool isUnitString(void *ptr) {
        jsuword delta = reinterpret_cast<jsuword>(ptr) -
                        reinterpret_cast<jsuword>(unitStringTable);
        if (delta >= UNIT_STRING_LIMIT * sizeof(JSString))
            return false;

        /* If ptr points inside the static array, it must be well-aligned. */
        JS_ASSERT(delta % sizeof(JSString) == 0);
        return true;
    }

    static inline bool isIntString(void *ptr) {
        jsuword delta = reinterpret_cast<jsuword>(ptr) -
                        reinterpret_cast<jsuword>(intStringTable);
        if (delta >= INT_STRING_LIMIT * sizeof(JSString))
            return false;

        /* If ptr points inside the static array, it must be well-aligned. */
        JS_ASSERT(delta % sizeof(JSString) == 0);
        return true;
    }

    static inline bool isStatic(void *ptr) {
        return isUnitString(ptr) || isIntString(ptr);
    }

#ifdef __SUNPRO_CC
#pragma align 8 (__1cIJSStringPunitStringTable_, __1cIJSStringOintStringTable_)
#endif

    static JSString unitStringTable[];
    static JSString intStringTable[];
    static const char *deflatedIntStringTable[];

    static JSString *unitString(jschar c);
    static JSString *getUnitString(JSContext *cx, JSString *str, size_t index);
    static JSString *intString(jsint i);
};

extern const jschar *
js_GetStringChars(JSContext *cx, JSString *str);

extern JSString * JS_FASTCALL
js_ConcatStrings(JSContext *cx, JSString *left, JSString *right);

extern const jschar *
js_UndependString(JSContext *cx, JSString *str);

extern JSBool
js_MakeStringImmutable(JSContext *cx, JSString *str);

extern JSString * JS_FASTCALL
js_toLowerCase(JSContext *cx, JSString *str);

extern JSString * JS_FASTCALL
js_toUpperCase(JSContext *cx, JSString *str);

struct JSSubString {
    size_t          length;
    const jschar    *chars;
};

extern jschar      js_empty_ucstr[];
extern JSSubString js_EmptySubString;

/* Unicode character attribute lookup tables. */
extern const uint8 js_X[];
extern const uint8 js_Y[];
extern const uint32 js_A[];

/* Enumerated Unicode general category types. */
typedef enum JSCharType {
    JSCT_UNASSIGNED             = 0,
    JSCT_UPPERCASE_LETTER       = 1,
    JSCT_LOWERCASE_LETTER       = 2,
    JSCT_TITLECASE_LETTER       = 3,
    JSCT_MODIFIER_LETTER        = 4,
    JSCT_OTHER_LETTER           = 5,
    JSCT_NON_SPACING_MARK       = 6,
    JSCT_ENCLOSING_MARK         = 7,
    JSCT_COMBINING_SPACING_MARK = 8,
    JSCT_DECIMAL_DIGIT_NUMBER   = 9,
    JSCT_LETTER_NUMBER          = 10,
    JSCT_OTHER_NUMBER           = 11,
    JSCT_SPACE_SEPARATOR        = 12,
    JSCT_LINE_SEPARATOR         = 13,
    JSCT_PARAGRAPH_SEPARATOR    = 14,
    JSCT_CONTROL                = 15,
    JSCT_FORMAT                 = 16,
    JSCT_PRIVATE_USE            = 18,
    JSCT_SURROGATE              = 19,
    JSCT_DASH_PUNCTUATION       = 20,
    JSCT_START_PUNCTUATION      = 21,
    JSCT_END_PUNCTUATION        = 22,
    JSCT_CONNECTOR_PUNCTUATION  = 23,
    JSCT_OTHER_PUNCTUATION      = 24,
    JSCT_MATH_SYMBOL            = 25,
    JSCT_CURRENCY_SYMBOL        = 26,
    JSCT_MODIFIER_SYMBOL        = 27,
    JSCT_OTHER_SYMBOL           = 28
} JSCharType;

/* Character classifying and mapping macros, based on java.lang.Character. */
#define JS_CCODE(c)     (js_A[js_Y[(js_X[(uint16)(c)>>6]<<6)|((c)&0x3F)]])
#define JS_CTYPE(c)     (JS_CCODE(c) & 0x1F)

#define JS_ISALPHA(c)   ((((1 << JSCT_UPPERCASE_LETTER) |                     \
                           (1 << JSCT_LOWERCASE_LETTER) |                     \
                           (1 << JSCT_TITLECASE_LETTER) |                     \
                           (1 << JSCT_MODIFIER_LETTER) |                      \
                           (1 << JSCT_OTHER_LETTER))                          \
                          >> JS_CTYPE(c)) & 1)

#define JS_ISALNUM(c)   ((((1 << JSCT_UPPERCASE_LETTER) |                     \
                           (1 << JSCT_LOWERCASE_LETTER) |                     \
                           (1 << JSCT_TITLECASE_LETTER) |                     \
                           (1 << JSCT_MODIFIER_LETTER) |                      \
                           (1 << JSCT_OTHER_LETTER) |                         \
                           (1 << JSCT_DECIMAL_DIGIT_NUMBER))                  \
                          >> JS_CTYPE(c)) & 1)

/* A unicode letter, suitable for use in an identifier. */
#define JS_ISLETTER(c)   ((((1 << JSCT_UPPERCASE_LETTER) |                    \
                            (1 << JSCT_LOWERCASE_LETTER) |                    \
                            (1 << JSCT_TITLECASE_LETTER) |                    \
                            (1 << JSCT_MODIFIER_LETTER) |                     \
                            (1 << JSCT_OTHER_LETTER) |                        \
                            (1 << JSCT_LETTER_NUMBER))                        \
                           >> JS_CTYPE(c)) & 1)

/*
 * 'IdentifierPart' from ECMA grammar, is Unicode letter or combining mark or
 * digit or connector punctuation.
 */
#define JS_ISIDPART(c)  ((((1 << JSCT_UPPERCASE_LETTER) |                     \
                           (1 << JSCT_LOWERCASE_LETTER) |                     \
                           (1 << JSCT_TITLECASE_LETTER) |                     \
                           (1 << JSCT_MODIFIER_LETTER) |                      \
                           (1 << JSCT_OTHER_LETTER) |                         \
                           (1 << JSCT_LETTER_NUMBER) |                        \
                           (1 << JSCT_NON_SPACING_MARK) |                     \
                           (1 << JSCT_COMBINING_SPACING_MARK) |               \
                           (1 << JSCT_DECIMAL_DIGIT_NUMBER) |                 \
                           (1 << JSCT_CONNECTOR_PUNCTUATION))                 \
                          >> JS_CTYPE(c)) & 1)

/* Unicode control-format characters, ignored in input */
#define JS_ISFORMAT(c) (((1 << JSCT_FORMAT) >> JS_CTYPE(c)) & 1)

/*
 * This table is used in JS_ISWORD.  The definition has external linkage to
 * allow the raw table data to be used in the regular expression compiler.
 */
extern const bool js_alnum[];

/*
 * This macro performs testing for the regular expression word class \w, which
 * is defined by ECMA-262 15.10.2.6 to be [0-9A-Z_a-z].  If we want a
 * Unicode-friendlier definition of "word", we should rename this macro to
 * something regexp-y.
 */
#define JS_ISWORD(c)    ((c) < 128 && js_alnum[(c)])

#define JS_ISIDSTART(c) (JS_ISLETTER(c) || (c) == '_' || (c) == '$')
#define JS_ISIDENT(c)   (JS_ISIDPART(c) || (c) == '_' || (c) == '$')

#define JS_ISXMLSPACE(c)        ((c) == ' ' || (c) == '\t' || (c) == '\r' ||  \
                                 (c) == '\n')
#define JS_ISXMLNSSTART(c)      ((JS_CCODE(c) & 0x00000100) || (c) == '_')
#define JS_ISXMLNS(c)           ((JS_CCODE(c) & 0x00000080) || (c) == '.' ||  \
                                 (c) == '-' || (c) == '_')
#define JS_ISXMLNAMESTART(c)    (JS_ISXMLNSSTART(c) || (c) == ':')
#define JS_ISXMLNAME(c)         (JS_ISXMLNS(c) || (c) == ':')

#define JS_ISDIGIT(c)   (JS_CTYPE(c) == JSCT_DECIMAL_DIGIT_NUMBER)

static inline bool
JS_ISSPACE(jschar c)
{
    unsigned w = c;

    if (w < 256)
        return (w <= ' ' && (w == ' ' || (9 <= w && w <= 0xD))) || w == 0xA0;

    return (JS_CCODE(w) & 0x00070000) == 0x00040000;
}

#define JS_ISPRINT(c)   ((c) < 128 && isprint(c))

#define JS_ISUPPER(c)   (JS_CTYPE(c) == JSCT_UPPERCASE_LETTER)
#define JS_ISLOWER(c)   (JS_CTYPE(c) == JSCT_LOWERCASE_LETTER)

#define JS_TOUPPER(c)   ((jschar) ((JS_CCODE(c) & 0x00100000)                 \
                                   ? (c) - ((int32)JS_CCODE(c) >> 22)         \
                                   : (c)))
#define JS_TOLOWER(c)   ((jschar) ((JS_CCODE(c) & 0x00200000)                 \
                                   ? (c) + ((int32)JS_CCODE(c) >> 22)         \
                                   : (c)))

/*
 * Shorthands for ASCII (7-bit) decimal and hex conversion.
 * Manually inline isdigit for performance; MSVC doesn't do this for us.
 */
#define JS7_ISDEC(c)    ((((unsigned)(c)) - '0') <= 9)
#define JS7_UNDEC(c)    ((c) - '0')
#define JS7_ISHEX(c)    ((c) < 128 && isxdigit(c))
#define JS7_UNHEX(c)    (uintN)(JS7_ISDEC(c) ? (c) - '0' : 10 + tolower(c) - 'a')
#define JS7_ISLET(c)    ((c) < 128 && isalpha(c))

/* Initialize per-runtime string state for the first context in the runtime. */
extern JSBool
js_InitRuntimeStringState(JSContext *cx);

extern JSBool
js_InitDeflatedStringCache(JSRuntime *rt);

extern void
js_FinishRuntimeStringState(JSContext *cx);

extern void
js_FinishDeflatedStringCache(JSRuntime *rt);

/* Initialize the String class, returning its prototype object. */
extern JSClass js_StringClass;

extern JSObject *
js_InitStringClass(JSContext *cx, JSObject *obj);

extern const char js_escape_str[];
extern const char js_unescape_str[];
extern const char js_uneval_str[];
extern const char js_decodeURI_str[];
extern const char js_encodeURI_str[];
extern const char js_decodeURIComponent_str[];
extern const char js_encodeURIComponent_str[];

/* GC-allocate a string descriptor for the given malloc-allocated chars. */
extern JSString *
js_NewString(JSContext *cx, jschar *chars, size_t length);

/*
 * GC-allocate a string descriptor and steal the char buffer held by |cb|.
 * This function takes responsibility for adding the terminating '\0' required
 * by js_NewString.
 */
extern JSString *
js_NewStringFromCharBuffer(JSContext *cx, JSCharBuffer &cb);

extern JSString *
js_NewDependentString(JSContext *cx, JSString *base, size_t start,
                      size_t length);

/* Copy a counted string and GC-allocate a descriptor for it. */
extern JSString *
js_NewStringCopyN(JSContext *cx, const jschar *s, size_t n);

/* Copy a C string and GC-allocate a descriptor for it. */
extern JSString *
js_NewStringCopyZ(JSContext *cx, const jschar *s);

/*
 * Convert a value to a printable C string.
 */
typedef JSString *(*JSValueToStringFun)(JSContext *cx, jsval v);

extern JS_FRIEND_API(const char *)
js_ValueToPrintable(JSContext *cx, jsval v, JSValueToStringFun v2sfun);

#define js_ValueToPrintableString(cx,v) \
    js_ValueToPrintable(cx, v, js_ValueToString)

#define js_ValueToPrintableSource(cx,v) \
    js_ValueToPrintable(cx, v, js_ValueToSource)

/*
 * Convert a value to a string, returning null after reporting an error,
 * otherwise returning a new string reference.
 */
extern JS_FRIEND_API(JSString *)
js_ValueToString(JSContext *cx, jsval v);

/*
 * This function implements E-262-3 section 9.8, toString. Convert the given
 * value to a string of jschars appended to the given buffer. On error, the
 * passed buffer may have partial results appended.
 */
extern JS_FRIEND_API(JSBool)
js_ValueToCharBuffer(JSContext *cx, jsval v, JSCharBuffer &cb);

/*
 * Convert a value to its source expression, returning null after reporting
 * an error, otherwise returning a new string reference.
 */
extern JS_FRIEND_API(JSString *)
js_ValueToSource(JSContext *cx, jsval v);

/*
 * Compute a hash function from str. The caller can call this function even if
 * str is not a GC-allocated thing.
 */
extern uint32
js_HashString(JSString *str);

/*
 * Test if strings are equal. The caller can call the function even if str1
 * or str2 are not GC-allocated things.
 */
extern JSBool JS_FASTCALL
js_EqualStrings(JSString *str1, JSString *str2);

/*
 * Return less than, equal to, or greater than zero depending on whether
 * str1 is less than, equal to, or greater than str2.
 */
extern int32 JS_FASTCALL
js_CompareStrings(JSString *str1, JSString *str2);

/*
 * Boyer-Moore-Horspool superlinear search for pat:patlen in text:textlen.
 * The patlen argument must be positive and no greater than sBMHPatLenMax.
 *
 * Return the index of pat in text, or -1 if not found.
 */
static const jsuint sBMHCharSetSize = 256; /* ISO-Latin-1 */
static const jsuint sBMHPatLenMax   = 255; /* skip table element is uint8 */
static const jsint  sBMHBadPattern  = -2;  /* return value if pat is not ISO-Latin-1 */

extern jsint
js_BoyerMooreHorspool(const jschar *text, jsuint textlen,
                      const jschar *pat, jsuint patlen);

extern size_t
js_strlen(const jschar *s);

extern jschar *
js_strchr(const jschar *s, jschar c);

extern jschar *
js_strchr_limit(const jschar *s, jschar c, const jschar *limit);

#define js_strncpy(t, s, n)     memcpy((t), (s), (n) * sizeof(jschar))

/*
 * Return s advanced past any Unicode white space characters.
 */
static inline const jschar *
js_SkipWhiteSpace(const jschar *s, const jschar *end)
{
    JS_ASSERT(s <= end);
    while (s != end && JS_ISSPACE(*s))
        s++;
    return s;
}

/*
 * Inflate bytes to JS chars and vice versa.  Report out of memory via cx
 * and return null on error, otherwise return the jschar or byte vector that
 * was JS_malloc'ed. length is updated with the length of the new string in jschars.
 */
extern jschar *
js_InflateString(JSContext *cx, const char *bytes, size_t *length);

extern char *
js_DeflateString(JSContext *cx, const jschar *chars, size_t length);

/*
 * Inflate bytes to JS chars into a buffer. 'chars' must be large enough for
 * 'length' jschars. The buffer is NOT null-terminated. The destination length
 * must be be initialized with the buffer size and will contain on return the
 * number of copied chars.
 */
extern JSBool
js_InflateStringToBuffer(JSContext *cx, const char *bytes, size_t length,
                         jschar *chars, size_t *charsLength);

/*
 * Get number of bytes in the deflated sequence of characters.
 */
extern size_t
js_GetDeflatedStringLength(JSContext *cx, const jschar *chars,
                           size_t charsLength);

/*
 * Deflate JS chars to bytes into a buffer. 'bytes' must be large enough for
 * 'length chars. The buffer is NOT null-terminated. The destination length
 * must to be initialized with the buffer size and will contain on return the
 * number of copied bytes.
 */
extern JSBool
js_DeflateStringToBuffer(JSContext *cx, const jschar *chars,
                         size_t charsLength, char *bytes, size_t *length);

/*
 * Associate bytes with str in the deflated string cache, returning true on
 * successful association, false on out of memory.
 */
extern JSBool
js_SetStringBytes(JSContext *cx, JSString *str, char *bytes, size_t length);

/*
 * Find or create a deflated string cache entry for str that contains its
 * characters chopped from Unicode code points into bytes.
 */
extern const char *
js_GetStringBytes(JSContext *cx, JSString *str);

/* Remove a deflated string cache entry associated with str if any. */
extern void
js_PurgeDeflatedStringCache(JSRuntime *rt, JSString *str);

/* Export a few natives and a helper to other files in SpiderMonkey. */
extern JSBool
js_str_escape(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval);

extern JSBool
js_str_toString(JSContext *cx, uintN argc, jsval *vp);

/*
 * Convert one UCS-4 char and write it into a UTF-8 buffer, which must be at
 * least 6 bytes long.  Return the number of UTF-8 bytes of data written.
 */
extern int
js_OneUcs4ToUtf8Char(uint8 *utf8Buffer, uint32 ucs4Char);

/*
 * Write str into buffer escaping any non-printable or non-ASCII character.
 * Guarantees that a NUL is at the end of the buffer. Returns the length of
 * the written output, NOT including the NUL. If buffer is null, just returns
 * the length of the output. If quote is not 0, it must be a single or double
 * quote character that will quote the output.
 *
 * The function is only defined for debug builds.
*/
#define js_PutEscapedString(buffer, bufferSize, str, quote)                   \
    js_PutEscapedStringImpl(buffer, bufferSize, NULL, str, quote)

/*
 * Write str into file escaping any non-printable or non-ASCII character.
 * Returns the number of bytes written to file. If quote is not 0, it must
 * be a single or double quote character that will quote the output.
 *
 * The function is only defined for debug builds.
*/
#define js_FileEscapedString(file, str, quote)                                \
    (JS_ASSERT(file), js_PutEscapedStringImpl(NULL, 0, file, str, quote))

extern JS_FRIEND_API(size_t)
js_PutEscapedStringImpl(char *buffer, size_t bufferSize, FILE *fp,
                        JSString *str, uint32 quote);

extern JSBool
js_String(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JS_END_EXTERN_C

#endif /* jsstr_h___ */
