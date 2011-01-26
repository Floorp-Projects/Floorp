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
#include "jsapi.h"
#include "jsprvtd.h"
#include "jshashtable.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsvalue.h"
#include "jscell.h"

enum {
    UNIT_STRING_LIMIT        = 256U,
    SMALL_CHAR_LIMIT         = 128U, /* Bigger chars cannot be in a length-2 string. */
    NUM_SMALL_CHARS          = 64U,
    INT_STRING_LIMIT         = 256U,
    NUM_HUNDRED_STRINGS      = 156U
};

extern jschar *
js_GetDependentStringChars(JSString *str);

extern JSString * JS_FASTCALL
js_ConcatStrings(JSContext *cx, JSString *left, JSString *right);

JS_STATIC_ASSERT(JS_BITS_PER_WORD >= 32);

struct JSRopeBufferInfo {
    /* Number of jschars we can hold, not including null terminator. */
    size_t capacity;
};

/* Forward declaration for friending. */
namespace js { namespace mjit {
    class Compiler;
}}

struct JSLinearString;

/*
 * The GC-thing "string" type.
 *
 * In FLAT strings, the mChars field  points to a flat character array owned by
 * its GC-thing descriptor. The array is terminated at index length by a zero
 * character and the size of the array in bytes is
 * (length + 1) * sizeof(jschar). The terminator is purely a backstop, in case
 * the chars pointer flows out to native code that requires \u0000 termination.
 *
 * A flat string with the ATOMIZED flag means that the string is hashed as
 * an atom. This flag is used to avoid re-hashing the already-atomized string.
 *
 * A flat string with the EXTENSIBLE flag means that the string may change into
 * a dependent string as part of an optimization with js_ConcatStrings:
 * extending |str1 = "abc"| with the character |str2 = str1 + "d"| will place
 * "d" in the extra capacity from |str1|, make that the buffer for |str2|, and
 * turn |str1| into a dependent string of |str2|.
 *
 * Flat strings without the EXTENSIBLE flag can be safely accessed by multiple
 * threads.
 *
 * When the string is DEPENDENT, the string depends on characters of another
 * string strongly referenced by the base field. The base member may point to
 * another dependent string if chars() has not been called yet.
 *
 * When a string is a ROPE, it represents the lazy concatenation of other
 * strings. In general, the nodes reachable from any rope form a dag.
 *
 * To allow static type-based checking that a given JSString* always points
 * to a flat or non-rope string, the JSFlatString and JSLinearString types may
 * be used. Instead of casting, callers should use ensureX() and assertIsX().
 */
struct JSString
{
    friend class js::TraceRecorder;
    friend class js::mjit::Compiler;

    friend JSAtom *js_AtomizeString(JSContext *cx, JSString *str, uintN flags);

    /*
     * Not private because we want to be able to use static initializers for
     * them. Don't use these directly! FIXME bug 614459.
     */
    size_t                 lengthAndFlags;      /* in all strings */
    union {
        const jschar       *chars;              /* in non-rope strings */
        JSString           *left;               /* in rope strings */
    } u;
    union {
        jschar             inlineStorage[4];    /* in short strings */
        struct {
            union {
                JSString   *right;              /* in rope strings */
                JSString   *base;               /* in dependent strings */
                size_t     capacity;            /* in extensible flat strings */
            };
            union {
                JSString   *parent;             /* temporarily used during flatten */
                size_t     reserved;            /* may use for bug 615290 */
            };
        } s;
        size_t             externalStringType;  /* in external strings */
    };

    /*
     * The lengthAndFlags field in string headers has data arranged in the
     * following way:
     *
     * [ length (bits 4-31) ][ flags (bits 2-3) ][ type (bits 0-1) ]
     *
     * The length is packed in lengthAndFlags, even in string types that don't
     * need 3 other fields, to make the length check simpler.
     *
     * When the string type is FLAT, the flags can contain ATOMIZED or
     * EXTENSIBLE.
     */
    static const size_t TYPE_FLAGS_MASK = JS_BITMASK(4);
    static const size_t LENGTH_SHIFT    = 4;

    static const size_t TYPE_MASK       = JS_BITMASK(2);
    static const size_t FLAT            = 0x0;
    static const size_t DEPENDENT       = 0x1;
    static const size_t ROPE            = 0x2;

    /* Allow checking 1 bit for dependent/rope strings. */
    static const size_t DEPENDENT_BIT   = JS_BIT(0);
    static const size_t ROPE_BIT        = JS_BIT(1);

    static const size_t ATOMIZED        = JS_BIT(2);
    static const size_t EXTENSIBLE      = JS_BIT(3);


    size_t buildLengthAndFlags(size_t length, size_t flags) {
        return (length << LENGTH_SHIFT) | flags;
    }

    inline js::gc::Cell *asCell() {
        return reinterpret_cast<js::gc::Cell *>(this);
    }

    inline js::gc::FreeCell *asFreeCell() {
        return reinterpret_cast<js::gc::FreeCell *>(this);
    }

    /*
     * Generous but sane length bound; the "-1" is there for comptibility with
     * OOM tests.
     */
    static const size_t MAX_LENGTH = (1 << 28) - 1;

    JS_ALWAYS_INLINE bool isDependent() const {
        return lengthAndFlags & DEPENDENT_BIT;
    }

    JS_ALWAYS_INLINE bool isFlat() const {
        return (lengthAndFlags & TYPE_MASK) == FLAT;
    }

    JS_ALWAYS_INLINE bool isExtensible() const {
        JS_ASSERT_IF(lengthAndFlags & EXTENSIBLE, isFlat());
        return lengthAndFlags & EXTENSIBLE;
    }

    JS_ALWAYS_INLINE bool isAtomized() const {
        JS_ASSERT_IF(lengthAndFlags & ATOMIZED, isFlat());
        return lengthAndFlags & ATOMIZED;
    }

    JS_ALWAYS_INLINE bool isRope() const {
        return lengthAndFlags & ROPE_BIT;
    }

    JS_ALWAYS_INLINE size_t length() const {
        return lengthAndFlags >> LENGTH_SHIFT;
    }

    JS_ALWAYS_INLINE bool empty() const {
        return lengthAndFlags <= TYPE_FLAGS_MASK;
    }

    /* This can fail by returning null and reporting an error on cx. */
    JS_ALWAYS_INLINE const jschar *getChars(JSContext *cx) {
        if (isRope())
            return flatten(cx);
        return nonRopeChars();
    }

    /* This can fail by returning null and reporting an error on cx. */
    JS_ALWAYS_INLINE const jschar *getCharsZ(JSContext *cx) {
        if (!isFlat())
            return undepend(cx);
        return flatChars();
    }

    JS_ALWAYS_INLINE void initFlatNotTerminated(jschar *chars, size_t length) {
        JS_ASSERT(length <= MAX_LENGTH);
        JS_ASSERT(!isStatic(this));
        lengthAndFlags = buildLengthAndFlags(length, FLAT);
        u.chars = chars;
    }

    /* Specific flat string initializer and accessor methods. */
    JS_ALWAYS_INLINE void initFlat(jschar *chars, size_t length) {
        initFlatNotTerminated(chars, length);
        JS_ASSERT(chars[length] == jschar(0));
    }

    JS_ALWAYS_INLINE void initShortString(const jschar *chars, size_t length) {
        JS_ASSERT(length <= MAX_LENGTH);
        JS_ASSERT(chars >= inlineStorage && chars < (jschar *)(this + 2));
        JS_ASSERT(!isStatic(this));
        lengthAndFlags = buildLengthAndFlags(length, FLAT);
        u.chars = chars;
    }

    JS_ALWAYS_INLINE void initFlatExtensible(jschar *chars, size_t length, size_t cap) {
        JS_ASSERT(length <= MAX_LENGTH);
        JS_ASSERT(chars[length] == jschar(0));
        JS_ASSERT(!isStatic(this));
        lengthAndFlags = buildLengthAndFlags(length, FLAT | EXTENSIBLE);
        u.chars = chars;
        s.capacity = cap;
    }

    JS_ALWAYS_INLINE JSFlatString *assertIsFlat() {
        JS_ASSERT(isFlat());
        return reinterpret_cast<JSFlatString *>(this);
    }

    JS_ALWAYS_INLINE const jschar *flatChars() const {
        JS_ASSERT(isFlat());
        return u.chars;
    }

    JS_ALWAYS_INLINE size_t flatLength() const {
        JS_ASSERT(isFlat());
        return length();
    }

    inline void flatSetAtomized() {
        JS_ASSERT(isFlat());
        JS_ASSERT(!isStatic(this));
        lengthAndFlags |= ATOMIZED;
    }

    inline void flatClearExtensible() {
        /*
         * N.B. This may be called on static strings, which may be in read-only
         * memory, so we cannot unconditionally apply the mask.
         */
        JS_ASSERT(isFlat());
        if (lengthAndFlags & EXTENSIBLE)
            lengthAndFlags &= ~EXTENSIBLE;
    }

    /*
     * The chars pointer should point somewhere inside the buffer owned by base.
     * The caller still needs to pass base for GC purposes.
     */
    inline void initDependent(JSString *base, const jschar *chars, size_t length) {
        JS_ASSERT(!isStatic(this));
        JS_ASSERT(base->isFlat());
        JS_ASSERT(chars >= base->flatChars() && chars < base->flatChars() + base->length());
        JS_ASSERT(length <= base->length() - (chars - base->flatChars()));
        lengthAndFlags = buildLengthAndFlags(length, DEPENDENT);
        u.chars = chars;
        s.base = base;
    }

    inline JSLinearString *dependentBase() const {
        JS_ASSERT(isDependent());
        return s.base->assertIsLinear();
    }

    JS_ALWAYS_INLINE const jschar *dependentChars() {
        JS_ASSERT(isDependent());
        return u.chars;
    }

    inline size_t dependentLength() const {
        JS_ASSERT(isDependent());
        return length();
    }

    const jschar *undepend(JSContext *cx);

    const jschar *nonRopeChars() const {
        JS_ASSERT(!isRope());
        return u.chars;
    }

    /* Rope-related initializers and accessors. */
    inline void initRopeNode(JSString *left, JSString *right, size_t length) {
        JS_ASSERT(left->length() + right->length() == length);
        lengthAndFlags = buildLengthAndFlags(length, ROPE);
        u.left = left;
        s.right = right;
    }

    inline JSString *ropeLeft() const {
        JS_ASSERT(isRope());
        return u.left;
    }

    inline JSString *ropeRight() const {
        JS_ASSERT(isRope());
        return s.right;
    }

    inline void finishTraversalConversion(JSString *base, const jschar *baseBegin, const jschar *end) {
        JS_ASSERT(baseBegin <= u.chars && u.chars <= end);
        lengthAndFlags = buildLengthAndFlags(end - u.chars, DEPENDENT);
        s.base = base;
    }

    const jschar *flatten(JSContext *maybecx);

    JSLinearString *ensureLinear(JSContext *cx) {
        if (isRope() && !flatten(cx))
            return NULL;
        return reinterpret_cast<JSLinearString *>(this);
    }

    bool isLinear() const {
        return !isRope();
    }

    JSLinearString *assertIsLinear() {
        JS_ASSERT(isLinear());
        return reinterpret_cast<JSLinearString *>(this);
    }

    typedef uint8 SmallChar;

    static inline bool fitsInSmallChar(jschar c) {
        return c < SMALL_CHAR_LIMIT && toSmallChar[c] != INVALID_SMALL_CHAR;
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

    static inline bool isLength2String(void *ptr) {
        jsuword delta = reinterpret_cast<jsuword>(ptr) -
                        reinterpret_cast<jsuword>(length2StringTable);
        if (delta >= NUM_SMALL_CHARS * NUM_SMALL_CHARS * sizeof(JSString))
            return false;

        /* If ptr points inside the static array, it must be well-aligned. */
        JS_ASSERT(delta % sizeof(JSString) == 0);
        return true;
    }

    static inline bool isHundredString(void *ptr) {
        jsuword delta = reinterpret_cast<jsuword>(ptr) -
                        reinterpret_cast<jsuword>(hundredStringTable);
        if (delta >= NUM_HUNDRED_STRINGS * sizeof(JSString))
            return false;

        /* If ptr points inside the static array, it must be well-aligned. */
        JS_ASSERT(delta % sizeof(JSString) == 0);
        return true;
    }

    static inline bool isStatic(void *ptr) {
        return isUnitString(ptr) || isLength2String(ptr) || isHundredString(ptr);
    }

#ifdef __SUNPRO_CC
#pragma align 8 (__1cIJSStringPunitStringTable_, __1cIJSStringSlength2StringTable_, __1cIJSStringShundredStringTable_)
#endif

    static const SmallChar INVALID_SMALL_CHAR = -1;

    static const jschar fromSmallChar[];
    static const SmallChar toSmallChar[];
    static const JSString unitStringTable[];
    static const JSString length2StringTable[];
    static const JSString hundredStringTable[];
    /*
     * Since int strings can be unit strings, length-2 strings, or hundred
     * strings, we keep a table to map from integer to the correct string.
     */
    static const JSString *const intStringTable[];

    static JSFlatString *unitString(jschar c);
    static JSLinearString *getUnitString(JSContext *cx, JSString *str, size_t index);
    static JSFlatString *length2String(jschar c1, jschar c2);
    static JSFlatString *length2String(uint32 i);
    static JSFlatString *intString(jsint i);

    static JSFlatString *lookupStaticString(const jschar *chars, size_t length);

    JS_ALWAYS_INLINE void finalize(JSContext *cx);

    static size_t offsetOfLengthAndFlags() {
        return offsetof(JSString, lengthAndFlags);
    }

    static size_t offsetOfChars() {
        return offsetof(JSString, u.chars);
    }

    static void staticAsserts() {
        JS_STATIC_ASSERT(((JSString::MAX_LENGTH << JSString::LENGTH_SHIFT) >>
                           JSString::LENGTH_SHIFT) == JSString::MAX_LENGTH);
    }
};

/*
 * A "linear" string may or may not be null-terminated, but it provides
 * infallible access to a linear array of characters. Namely, this means the
 * string is not a rope.
 */
struct JSLinearString : JSString
{
    const jschar *chars() const { return JSString::nonRopeChars(); }
};

JS_STATIC_ASSERT(sizeof(JSLinearString) == sizeof(JSString));

/*
 * A linear string where, additionally, chars()[length()] == '\0'. Namely, this
 * means the string is not a dependent string or rope.
 */
struct JSFlatString : JSLinearString
{
    const jschar *charsZ() const { return chars(); }
};

JS_STATIC_ASSERT(sizeof(JSFlatString) == sizeof(JSString));

/*
 * A flat string which has been "atomized", i.e., that is a unique string among
 * other atomized strings and therefore allows equality via pointer comparison.
 */
struct JSAtom : JSFlatString
{
};

struct JSExternalString : JSString
{
    static const uintN TYPE_LIMIT = 8;
    static JSStringFinalizeOp str_finalizers[TYPE_LIMIT];

    static intN changeFinalizer(JSStringFinalizeOp oldop,
                                JSStringFinalizeOp newop) {
        for (uintN i = 0; i != JS_ARRAY_LENGTH(str_finalizers); i++) {
            if (str_finalizers[i] == oldop) {
                str_finalizers[i] = newop;
                return intN(i);
            }
        }
        return -1;
    }

    void finalize(JSContext *cx);
    void finalize();
};

JS_STATIC_ASSERT(sizeof(JSString) == sizeof(JSExternalString));

/*
 * Short strings should be created in cases where it's worthwhile to avoid
 * mallocing the string buffer for a small string. We keep 2 string headers'
 * worth of space in short strings so that more strings can be stored this way.
 */
class JSShortString : public js::gc::Cell
{
    JSString mHeader;
    JSString mDummy;

  public:
    /*
     * Set the length of the string, and return a buffer for the caller to write
     * to. This buffer must be written immediately, and should not be modified
     * afterward.
     */
    inline jschar *init(size_t length) {
        JS_ASSERT(length <= MAX_SHORT_STRING_LENGTH);
        mHeader.initShortString(mHeader.inlineStorage, length);
        return mHeader.inlineStorage;
    }

    inline jschar *getInlineStorageBeforeInit() {
        return mHeader.inlineStorage;
    }

    inline void initAtOffsetInBuffer(jschar *p, size_t length) {
        JS_ASSERT(p >= mHeader.inlineStorage && p < mHeader.inlineStorage + MAX_SHORT_STRING_LENGTH);
        mHeader.initShortString(p, length);
    }

    inline void resetLength(size_t length) {
        mHeader.initShortString(mHeader.flatChars(), length);
    }

    inline JSString *header() {
        return &mHeader;
    }

    static const size_t FREE_STRING_WORDS = 2;

    static const size_t MAX_SHORT_STRING_LENGTH =
            ((sizeof(JSString) + FREE_STRING_WORDS * sizeof(size_t)) / sizeof(jschar)) - 1;

    static inline bool fitsIntoShortString(size_t length) {
        return length <= MAX_SHORT_STRING_LENGTH;
    }

    JS_ALWAYS_INLINE void finalize(JSContext *cx);

    static void staticAsserts() {
        JS_STATIC_ASSERT(offsetof(JSString, inlineStorage) ==
                         sizeof(JSString) - JSShortString::FREE_STRING_WORDS * sizeof(void *));
        JS_STATIC_ASSERT(offsetof(JSShortString, mDummy) == sizeof(JSString));
        JS_STATIC_ASSERT(offsetof(JSString, inlineStorage) +
                         sizeof(jschar) * (JSShortString::MAX_SHORT_STRING_LENGTH + 1) ==
                         sizeof(JSShortString));
    }
};

namespace js {

class StringBuffer;

/*
 * When an algorithm does not need a string represented as a single linear
 * array of characters, this range utility may be used to traverse the string a
 * sequence of linear arrays of characters. This avoids flattening ropes.
 *
 * Implemented in jsstrinlines.h.
 */
class StringSegmentRange;
class MutatingRopeSegmentRange;

/*
 * Utility for building a rope (lazy concatenation) of strings.
 */
class RopeBuilder;

}  /* namespace js */

extern const jschar *
js_GetStringChars(JSContext *cx, JSString *str);

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

const jschar BYTE_ORDER_MARK = 0xFEFF;
const jschar NO_BREAK_SPACE  = 0x00A0;

static inline bool
JS_ISSPACE(jschar c)
{
    unsigned w = c;

    if (w < 256)
        return (w <= ' ' && (w == ' ' || (9 <= w && w <= 0xD))) || w == NO_BREAK_SPACE;

    return w == BYTE_ORDER_MARK || (JS_CCODE(w) & 0x00070000) == 0x00040000;
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

/* Initialize the String class, returning its prototype object. */
extern js::Class js_StringClass;

inline bool
JSObject::isString() const
{
    return getClass() == &js_StringClass;
}

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
extern JSFlatString *
js_NewString(JSContext *cx, jschar *chars, size_t length);

extern JSLinearString *
js_NewDependentString(JSContext *cx, JSString *base, size_t start,
                      size_t length);

/* Copy a counted string and GC-allocate a descriptor for it. */
extern JSFlatString *
js_NewStringCopyN(JSContext *cx, const jschar *s, size_t n);

extern JSFlatString *
js_NewStringCopyN(JSContext *cx, const char *s, size_t n);

/* Copy a C string and GC-allocate a descriptor for it. */
extern JSFlatString *
js_NewStringCopyZ(JSContext *cx, const jschar *s);

extern JSFlatString *
js_NewStringCopyZ(JSContext *cx, const char *s);

/*
 * Convert a value to a printable C string.
 */
extern const char *
js_ValueToPrintable(JSContext *cx, const js::Value &,
                    JSAutoByteString *bytes, bool asSource = false);

/*
 * Convert a value to a string, returning null after reporting an error,
 * otherwise returning a new string reference.
 */
extern JSString *
js_ValueToString(JSContext *cx, const js::Value &v);

namespace js {

/*
 * Most code that calls js_ValueToString knows the value is (probably) not a
 * string, so it does not make sense to put this inline fast path into
 * js_ValueToString.
 */
static JS_ALWAYS_INLINE JSString *
ValueToString_TestForStringInline(JSContext *cx, const Value &v)
{
    if (v.isString())
        return v.toString();
    return js_ValueToString(cx, v);
}

/*
 * This function implements E-262-3 section 9.8, toString. Convert the given
 * value to a string of jschars appended to the given buffer. On error, the
 * passed buffer may have partial results appended.
 */
extern bool
ValueToStringBuffer(JSContext *cx, const Value &v, StringBuffer &sb);

} /* namespace js */

/*
 * Convert a value to its source expression, returning null after reporting
 * an error, otherwise returning a new string reference.
 */
extern JS_FRIEND_API(JSString *)
js_ValueToSource(JSContext *cx, const js::Value &v);

/*
 * Compute a hash function from str. The caller can call this function even if
 * str is not a GC-allocated thing.
 */
inline uint32
js_HashString(JSLinearString *str)
{
    const jschar *s = str->chars();
    size_t n = str->length();
    uint32 h;
    for (h = 0; n; s++, n--)
        h = JS_ROTATE_LEFT32(h, 4) ^ *s;
    return h;
}

namespace js {

/*
 * Test if strings are equal. The caller can call the function even if str1
 * or str2 are not GC-allocated things.
 */
extern bool
EqualStrings(JSContext *cx, JSString *str1, JSString *str2, JSBool *result);

/* EqualStrings is infallible on linear strings. */
extern bool
EqualStrings(JSLinearString *str1, JSLinearString *str2);

/*
 * Return less than, equal to, or greater than zero depending on whether
 * str1 is less than, equal to, or greater than str2.
 */
extern bool
CompareStrings(JSContext *cx, JSString *str1, JSString *str2, int32 *result);

/*
 * Return true if the string matches the given sequence of ASCII bytes.
 */
extern bool
StringEqualsAscii(JSLinearString *str, const char *asciiBytes);

} /* namespacejs */

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

inline void
js_short_strncpy(jschar *dest, const jschar *src, size_t num)
{
    /*
     * It isn't strictly necessary here for |num| to be small, but this function
     * is currently only called on buffers for short strings.
     */
    JS_ASSERT(JSShortString::fitsIntoShortString(num));
    for (size_t i = 0; i < num; i++)
        dest[i] = src[i];
}

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
 * Inflate bytes to JS chars and vice versa.  Report out of memory via cx and
 * return null on error, otherwise return the jschar or byte vector that was
 * JS_malloc'ed. length is updated to the length of the new string in jschars.
 */
extern jschar *
js_InflateString(JSContext *cx, const char *bytes, size_t *length);

extern char *
js_DeflateString(JSContext *cx, const jschar *chars, size_t length);

/*
 * Inflate bytes to JS chars into a buffer. 'chars' must be large enough for
 * 'length' jschars. The buffer is NOT null-terminated. The destination length
 * must be be initialized with the buffer size and will contain on return the
 * number of copied chars. Conversion behavior depends on js_CStringsAreUTF8.
 */
extern JSBool
js_InflateStringToBuffer(JSContext *cx, const char *bytes, size_t length,
                         jschar *chars, size_t *charsLength);

/*
 * Same as js_InflateStringToBuffer, but always treats 'bytes' as UTF-8.
 */
extern JSBool
js_InflateUTF8StringToBuffer(JSContext *cx, const char *bytes, size_t length,
                             jschar *chars, size_t *charsLength);

/*
 * Get number of bytes in the deflated sequence of characters. Behavior depends
 * on js_CStringsAreUTF8.
 */
extern size_t
js_GetDeflatedStringLength(JSContext *cx, const jschar *chars,
                           size_t charsLength);

/*
 * Same as js_GetDeflatedStringLength, but always treats the result as UTF-8.
 */
extern size_t
js_GetDeflatedUTF8StringLength(JSContext *cx, const jschar *chars,
                               size_t charsLength);

/*
 * Deflate JS chars to bytes into a buffer. 'bytes' must be large enough for
 * 'length chars. The buffer is NOT null-terminated. The destination length
 * must to be initialized with the buffer size and will contain on return the
 * number of copied bytes. Conversion behavior depends on js_CStringsAreUTF8.
 */
extern JSBool
js_DeflateStringToBuffer(JSContext *cx, const jschar *chars,
                         size_t charsLength, char *bytes, size_t *length);

/*
 * Same as js_DeflateStringToBuffer, but always treats 'bytes' as UTF-8.
 */
extern JSBool
js_DeflateStringToUTF8Buffer(JSContext *cx, const jschar *chars,
                             size_t charsLength, char *bytes, size_t *length);

/* Export a few natives and a helper to other files in SpiderMonkey. */
extern JSBool
js_str_escape(JSContext *cx, uintN argc, js::Value *argv, js::Value *rval);

/*
 * The String.prototype.replace fast-native entry point is exported for joined
 * function optimization in js{interp,tracer}.cpp.
 */
namespace js {
extern JSBool
str_replace(JSContext *cx, uintN argc, js::Value *vp);
}

extern JSBool
js_str_toString(JSContext *cx, uintN argc, js::Value *vp);

extern JSBool
js_str_charAt(JSContext *cx, uintN argc, js::Value *vp);

extern JSBool
js_str_charCodeAt(JSContext *cx, uintN argc, js::Value *vp);

/*
 * Convert one UCS-4 char and write it into a UTF-8 buffer, which must be at
 * least 6 bytes long.  Return the number of UTF-8 bytes of data written.
 */
extern int
js_OneUcs4ToUtf8Char(uint8 *utf8Buffer, uint32 ucs4Char);

namespace js {

extern size_t
PutEscapedStringImpl(char *buffer, size_t size, FILE *fp, JSLinearString *str, uint32 quote);

/*
 * Write str into buffer escaping any non-printable or non-ASCII character
 * using \escapes for JS string literals.
 * Guarantees that a NUL is at the end of the buffer unless size is 0. Returns
 * the length of the written output, NOT including the NUL. Thus, a return
 * value of size or more means that the output was truncated. If buffer
 * is null, just returns the length of the output. If quote is not 0, it must
 * be a single or double quote character that will quote the output.
*/
inline size_t
PutEscapedString(char *buffer, size_t size, JSLinearString *str, uint32 quote)
{
    size_t n = PutEscapedStringImpl(buffer, size, NULL, str, quote);

    /* PutEscapedStringImpl can only fail with a file. */
    JS_ASSERT(n != size_t(-1));
    return n;
}

/*
 * Write str into file escaping any non-printable or non-ASCII character.
 * If quote is not 0, it must be a single or double quote character that
 * will quote the output.
*/
inline bool
FileEscapedString(FILE *fp, JSLinearString *str, uint32 quote)
{
    return PutEscapedStringImpl(NULL, 0, fp, str, quote) != size_t(-1);
}

} /* namespace js */

extern JSBool
js_String(JSContext *cx, uintN argc, js::Value *vp);

#endif /* jsstr_h___ */
