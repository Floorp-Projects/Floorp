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

#include <ctype.h>
#include "jsapi.h"
#include "jsprvtd.h"
#include "jshashtable.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsvalue.h"
#include "jscell.h"

/*
 * JS strings
 *
 * Conceptually, a JS string is just an array of chars and a length. To improve
 * performance of common string operations, the following optimizations are
 * made which affect the engine's representation of strings:
 *
 *  - The plain vanilla representation is a "flat" string which consists of a
 *    string header in the GC heap and a malloc'd null terminated char array.
 *
 *  - To avoid copying a substring of an existing "base" string , a "dependent"
 *    string (JSDependentString) can be created which points into the base
 *    string's char array.
 *
 *  - To avoid O(n^2) char buffer copying, a "rope" node (JSRope) can be created
 *    to represent a delayed string concatenation. Concatenation (called
 *    flattening) is performed if and when a linear char array is requested. In
 *    general, ropes form a binary dag whose internal nodes are JSRope string
 *    headers with no associated char array and whose leaf nodes are either flat
 *    or dependent strings.
 *
 *  - To avoid copying the left-hand side when flattening, the left-hand side's
 *    buffer may be grown to make space for a copy of the right-hand side (see
 *    comment in JSString::flatten). This optimization requires that there are
 *    no external pointers into the char array. We conservatively maintain this
 *    property via a flat string's "extensible" property.
 *
 *  - To avoid allocating small char arrays, short strings can be stored inline
 *    in the string header (JSInlineString). To increase the max size of such
 *    inline strings, extra-large string headers can be used (JSShortString).
 *
 *  - To avoid comparing O(n) string equality comparison, strings can be
 *    canonicalized to "atoms" (JSAtom) such that there is a single atom with a
 *    given (length,chars).
 *
 *  - To avoid dynamic creation of common short strings (e.g., single-letter
 *    alphanumeric strings, numeric strings up to 999) headers and char arrays
 *    for such strings are allocated in static memory (JSStaticAtom) and used
 *    as atoms.
 *
 *  - To avoid copying all strings created through the JSAPI, an "external"
 *    string (JSExternalString) can be created whose chars are managed by the
 *    JSAPI client.
 *
 * Although all strings share the same basic memory layout, we can conceptually
 * arrange them into a hierarchy of operations/invariants and represent this
 * hierarchy in C++ with classes:
 *
 * C++ type                     operations+fields / invariants+properties
 *
 * JSString (abstract)          getCharsZ, getChars, length / -
 *  | \
 *  | JSRope                    leftChild, rightChild / -
 *  |
 * JSLinearString (abstract)    chars / not null-terminated
 *  | \
 *  | JSDependentString         base / -
 *  |
 * JSFlatString (abstract)      chars / not null-terminated
 *  | \
 *  | JSExtensibleString        capacity / no external pointers into char array
 *  |
 * JSFixedString                - / may have external pointers into char array
 *  | \  \
 *  |  \ JSExternalString       - / char array memory managed by embedding
 *  |   \
 *  |   JSInlineString          - / chars stored in header
 *  |     | \
 *  |     | JSShortString       - / header is fat
 *  |     |        |
 * JSAtom |        |            - / string equality === pointer equality
 *  | \   |        |
 *  | JSInlineAtom |            - / atomized JSInlineString
 *  |       \      |
 *  |       JSShortAtom         - / atomized JSShortString
 *  |
 * JSStaticAtom                 - / header and chars statically allocated
 *
 * Classes marked with (abstract) above are not literally C++ Abstract Base
 * Classes (since there are no virtual functions, pure or not, in this
 * hierarchy), but have the same meaning: there are no strings with this type as
 * its most-derived type.
 *
 * Derived string types can be queried from ancestor types via isX() and
 * retrieved with asX() debug-only-checked casts.
 *
 * The ensureX() operations mutate 'this' in place to effectively the type to be
 * at least X (e.g., ensureLinear will change a JSRope to be a JSFlatString).
 */

class JSString : public js::gc::Cell
{
  protected:
    static const size_t NUM_INLINE_CHARS = 2 * sizeof(void *) / sizeof(jschar);

    /* Fields only apply to string types commented on the right. */
    struct Data
    {
        size_t                     lengthAndFlags;      /* JSString */
        union {
            const jschar           *chars;              /* JSLinearString */
            JSString               *left;               /* JSRope */
        } u1;
        union {
            jschar                 inlineStorage[NUM_INLINE_CHARS]; /* JS(Inline|Short)String */
            struct {
                union {
                    JSLinearString *base;               /* JSDependentString */
                    JSString       *right;              /* JSRope */
                    size_t         capacity;            /* JSFlatString (extensible) */
                    size_t         externalType;        /* JSExternalString */
                } u2;
                union {
                    JSString       *parent;             /* JSRope (temporary) */
                    void           *externalClosure;    /* JSExternalString */
                    size_t         reserved;            /* may use for bug 615290 */
                } u3;
            } s;
        };
    } d;

  public:
    /* Flags exposed only for jits */

    static const size_t LENGTH_SHIFT      = 4;
    static const size_t FLAGS_MASK        = JS_BITMASK(LENGTH_SHIFT);
    static const size_t MAX_LENGTH        = JS_BIT(32 - LENGTH_SHIFT) - 1;

    /*
     * The low LENGTH_SHIFT bits of lengthAndFlags are used to encode the type
     * of the string.  The remaining bits store the string length (which must be
     * less or equal than MAX_LENGTH).
     * 
     * Instead of using a dense index to represent the most-derived type, string
     * types are encoded to allow single-op tests for hot queries (isRope,
     * isDependent, isFlat, isAtom, isStaticAtom):
     *
     *   JSRope                xxx1
     *   JSLinearString        xxx0
     *   JSDependentString     xx1x
     *   JSFlatString          xx00
     *   JSExtensibleString    1100
     *   JSFixedString         xy00 where xy != 11
     *   JSInlineString        0100 and chars == inlineStorage
     *   JSShortString         0100 and in FINALIZE_SHORT_STRING arena
     *   JSExternalString      0100 and in FINALIZE_EXTERNAL_STRING arena
     *   JSAtom                x000
     *   JSStaticAtom          0000
     *
     * NB: this scheme takes advantage of the fact that there are no string
     * instances whose most-derived type is JSString, JSLinearString, or
     * JSFlatString.
     */

    static const size_t ROPE_BIT          = JS_BIT(0);

    static const size_t LINEAR_MASK       = JS_BITMASK(1);
    static const size_t LINEAR_FLAGS      = 0x0;

    static const size_t DEPENDENT_BIT     = JS_BIT(1);

    static const size_t FLAT_MASK         = JS_BITMASK(2);
    static const size_t FLAT_FLAGS        = 0x0;

    static const size_t FIXED_FLAGS       = JS_BIT(2);

    static const size_t ATOM_MASK         = JS_BITMASK(3);
    static const size_t ATOM_FLAGS        = 0x0;

    static const size_t STATIC_ATOM_MASK  = JS_BITMASK(4);
    static const size_t STATIC_ATOM_FLAGS = 0x0;

    static const size_t EXTENSIBLE_FLAGS  = JS_BIT(2) | JS_BIT(3);
    static const size_t NON_STATIC_ATOM   = JS_BIT(3);

    size_t buildLengthAndFlags(size_t length, size_t flags) {
        return (length << LENGTH_SHIFT) | flags;
    }

    static void staticAsserts() {
        JS_STATIC_ASSERT(size_t(JSString::MAX_LENGTH) <= size_t(JSVAL_INT_MAX));
        JS_STATIC_ASSERT(JSString::MAX_LENGTH <= JSVAL_INT_MAX);
        JS_STATIC_ASSERT(JS_BITS_PER_WORD >= 32);
        JS_STATIC_ASSERT(((JSString::MAX_LENGTH << JSString::LENGTH_SHIFT) >>
                           JSString::LENGTH_SHIFT) == JSString::MAX_LENGTH);
        JS_STATIC_ASSERT(sizeof(JSString) ==
                         offsetof(JSString, d.inlineStorage) +
                         NUM_INLINE_CHARS * sizeof(jschar));
    }

    /* Avoid lame compile errors in JSRope::flatten */
    friend class JSRope;

  public:
    /* All strings have length. */

    JS_ALWAYS_INLINE
    size_t length() const {
        return d.lengthAndFlags >> LENGTH_SHIFT;
    }

    JS_ALWAYS_INLINE
    bool empty() const {
        return d.lengthAndFlags <= FLAGS_MASK;
    }

    /*
     * All strings have a fallible operation to get an array of chars.
     * getCharsZ additionally ensures the array is null terminated.
     */

    inline const jschar *getChars(JSContext *cx);
    inline const jschar *getCharsZ(JSContext *cx);

    /* Fallible conversions to more-derived string types. */

    inline JSLinearString *ensureLinear(JSContext *cx);
    inline JSFlatString *ensureFlat(JSContext *cx);
    inline JSFixedString *ensureFixed(JSContext *cx);

    /* Type query and debug-checked casts */

    JS_ALWAYS_INLINE
    bool isRope() const {
        bool rope = d.lengthAndFlags & ROPE_BIT;
        JS_ASSERT_IF(rope, (d.lengthAndFlags & FLAGS_MASK) == ROPE_BIT);
        return rope;
    }

    JS_ALWAYS_INLINE
    JSRope &asRope() {
        JS_ASSERT(isRope());
        return *(JSRope *)this;
    }

    JS_ALWAYS_INLINE
    bool isLinear() const {
        return (d.lengthAndFlags & LINEAR_MASK) == LINEAR_FLAGS;
    }

    JS_ALWAYS_INLINE
    JSLinearString &asLinear() {
        JS_ASSERT(isLinear());
        return *(JSLinearString *)this;
    }

    JS_ALWAYS_INLINE
    bool isDependent() const {
        bool dependent = d.lengthAndFlags & DEPENDENT_BIT;
        JS_ASSERT_IF(dependent, (d.lengthAndFlags & FLAGS_MASK) == DEPENDENT_BIT);
        return dependent;
    }

    JS_ALWAYS_INLINE
    JSDependentString &asDependent() {
        JS_ASSERT(isDependent());
        return *(JSDependentString *)this;
    }

    JS_ALWAYS_INLINE
    bool isFlat() const {
        return (d.lengthAndFlags & FLAT_MASK) == FLAT_FLAGS;
    }

    JS_ALWAYS_INLINE
    JSFlatString &asFlat() {
        JS_ASSERT(isFlat());
        return *(JSFlatString *)this;
    }

    JS_ALWAYS_INLINE
    bool isExtensible() const {
        return (d.lengthAndFlags & FLAGS_MASK) == EXTENSIBLE_FLAGS;
    }

    JS_ALWAYS_INLINE
    JSExtensibleString &asExtensible() const {
        JS_ASSERT(isExtensible());
        return *(JSExtensibleString *)this;
    }

#ifdef DEBUG
    bool isShort() const;
    bool isFixed() const;
#endif

    JS_ALWAYS_INLINE
    JSFixedString &asFixed() {
        JS_ASSERT(isFixed());
        return *(JSFixedString *)this;
    }

    bool isExternal() const;

    JS_ALWAYS_INLINE
    JSExternalString &asExternal() {
        JS_ASSERT(isExternal());
        return *(JSExternalString *)this;
    }

    JS_ALWAYS_INLINE
    bool isAtom() const {
        bool atomized = (d.lengthAndFlags & ATOM_MASK) == ATOM_FLAGS;
        JS_ASSERT_IF(atomized, isFlat());
        return atomized;
    }

    JS_ALWAYS_INLINE
    JSAtom &asAtom() const {
        JS_ASSERT(isAtom());
        return *(JSAtom *)this;
    }

    JS_ALWAYS_INLINE
    bool isStaticAtom() const {
        return (d.lengthAndFlags & FLAGS_MASK) == STATIC_ATOM_FLAGS;
    }

    /* Only called by the GC for strings with the FINALIZE_STRING kind. */

    inline void finalize(JSContext *cx);

    /* Called during GC for any string. */

    void mark(JSTracer *trc);

    /* Offsets for direct field from jit code. */

    static size_t offsetOfLengthAndFlags() {
        return offsetof(JSString, d.lengthAndFlags);
    }

    static size_t offsetOfChars() {
        return offsetof(JSString, d.u1.chars);
    }
};

class JSRope : public JSString
{
    friend class JSString;
    JSFlatString *flatten(JSContext *cx);

    void init(JSString *left, JSString *right, size_t length);

  public:
    static inline JSRope *new_(JSContext *cx, JSString *left,
                               JSString *right, size_t length);

    inline JSString *leftChild() const {
        JS_ASSERT(isRope());
        return d.u1.left;
    }

    inline JSString *rightChild() const {
        JS_ASSERT(isRope());
        return d.s.u2.right;
    }
};

JS_STATIC_ASSERT(sizeof(JSRope) == sizeof(JSString));

class JSLinearString : public JSString
{
    friend class JSString;
    void mark(JSTracer *trc);

  public:
    JS_ALWAYS_INLINE
    const jschar *chars() const {
        JS_ASSERT(isLinear());
        return d.u1.chars;
    }
};

JS_STATIC_ASSERT(sizeof(JSLinearString) == sizeof(JSString));

class JSDependentString : public JSLinearString
{
    friend class JSString;
    JSFixedString *undepend(JSContext *cx);

    void init(JSLinearString *base, const jschar *chars, size_t length);

  public:
    static inline JSDependentString *new_(JSContext *cx, JSLinearString *base,
                                          const jschar *chars, size_t length);

    JSLinearString *base() const {
        JS_ASSERT(isDependent());
        return d.s.u2.base;
    }
};

JS_STATIC_ASSERT(sizeof(JSDependentString) == sizeof(JSString));

class JSFlatString : public JSLinearString
{
    friend class JSRope;
    void morphExtensibleIntoDependent(JSLinearString *base) {
        d.lengthAndFlags = buildLengthAndFlags(length(), DEPENDENT_BIT);
        d.s.u2.base = base;
    }

  public:
    JS_ALWAYS_INLINE
    const jschar *charsZ() const {
        JS_ASSERT(isFlat());
        return chars();
    }

    /* Only called by the GC for strings with the FINALIZE_STRING kind. */

    inline void finalize(JSRuntime *rt);
};

JS_STATIC_ASSERT(sizeof(JSFlatString) == sizeof(JSString));

class JSExtensibleString : public JSFlatString
{
  public:
    JS_ALWAYS_INLINE
    size_t capacity() const {
        JS_ASSERT(isExtensible());
        return d.s.u2.capacity;
    }
};

JS_STATIC_ASSERT(sizeof(JSExtensibleString) == sizeof(JSString));

class JSFixedString : public JSFlatString
{
    void init(const jschar *chars, size_t length);

  public:
    static inline JSFixedString *new_(JSContext *cx, const jschar *chars, size_t length);

    /*
     * Once a JSFixedString has been added to the atom state, this operation
     * changes the type (in place, as reflected by the flag bits) of the
     * JSFixedString into a JSAtom.
     */
    inline JSAtom *morphAtomizedStringIntoAtom();
};

JS_STATIC_ASSERT(sizeof(JSFixedString) == sizeof(JSString));

class JSInlineString : public JSFixedString
{
    static const size_t MAX_INLINE_LENGTH = NUM_INLINE_CHARS - 1;

  public:
    static inline JSInlineString *new_(JSContext *cx);

    inline jschar *init(size_t length);

    inline void resetLength(size_t length);

    static bool lengthFits(size_t length) {
        return length <= MAX_INLINE_LENGTH;
    }

};

JS_STATIC_ASSERT(sizeof(JSInlineString) == sizeof(JSString));

class JSShortString : public JSInlineString
{
    /* This can be any value that is a multiple of sizeof(gc::FreeCell). */
    static const size_t INLINE_EXTENSION_CHARS = sizeof(JSString::Data) / sizeof(jschar);

    static void staticAsserts() {
        JS_STATIC_ASSERT(INLINE_EXTENSION_CHARS % sizeof(js::gc::FreeCell) == 0);
        JS_STATIC_ASSERT(MAX_SHORT_LENGTH + 1 ==
                         (sizeof(JSShortString) -
                          offsetof(JSShortString, d.inlineStorage)) / sizeof(jschar));
    }

    jschar inlineStorageExtension[INLINE_EXTENSION_CHARS];

  public:
    static inline JSShortString *new_(JSContext *cx);

    jschar *inlineStorageBeforeInit() {
        return d.inlineStorage;
    }

    inline void initAtOffsetInBuffer(const jschar *chars, size_t length);

    static const size_t MAX_SHORT_LENGTH = JSString::NUM_INLINE_CHARS +
                                           INLINE_EXTENSION_CHARS
                                           -1 /* null terminator */;

    static bool lengthFits(size_t length) {
        return length <= MAX_SHORT_LENGTH;
    }

    /* Only called by the GC for strings with the FINALIZE_EXTERNAL_STRING kind. */

    JS_ALWAYS_INLINE void finalize(JSContext *cx);
};

JS_STATIC_ASSERT(sizeof(JSShortString) == 2 * sizeof(JSString));

/*
 * The externalClosure stored in an external string is a black box to the JS
 * engine; see JS_NewExternalStringWithClosure.
 */
class JSExternalString : public JSFixedString
{
    static void staticAsserts() {
        JS_STATIC_ASSERT(TYPE_LIMIT == 8);
    }

    void init(const jschar *chars, size_t length, intN type, void *closure);

  public:
    static inline JSExternalString *new_(JSContext *cx, const jschar *chars,
                                         size_t length, intN type, void *closure);

    intN externalType() const {
        JS_ASSERT(isExternal());
        JS_ASSERT(d.s.u2.externalType < TYPE_LIMIT);
        return d.s.u2.externalType;
    }

    void *externalClosure() const {
        JS_ASSERT(isExternal());
        return d.s.u3.externalClosure;
    }

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

    /* Only called by the GC for strings with the FINALIZE_EXTERNAL_STRING kind. */

    void finalize(JSContext *cx);
    void finalize();
};

JS_STATIC_ASSERT(sizeof(JSExternalString) == sizeof(JSString));

class JSAtom : public JSFixedString
{
  public:
    /* Exposed only for jits. */

    static const size_t UNIT_STATIC_LIMIT   = 256U;
    static const size_t SMALL_CHAR_LIMIT    = 128U; /* Bigger chars cannot be in a length-2 string. */
    static const size_t NUM_SMALL_CHARS     = 64U;
    static const size_t INT_STATIC_LIMIT    = 256U;
    static const size_t NUM_HUNDRED_STATICS = 156U;

#ifdef __SUNPRO_CC
# pragma align 8 (__1cGJSAtomPunitStaticTable_, __1cGJSAtomSlength2StaticTable_, __1cGJSAtomShundredStaticTable_)
#endif
    static const JSString::Data unitStaticTable[];
    static const JSString::Data length2StaticTable[];
    static const JSString::Data hundredStaticTable[];
    static const JSString::Data *const intStaticTable[];

  private:
    /* Defined in jsgcinlines.h */
    static inline bool isUnitString(const void *ptr);
    static inline bool isLength2String(const void *ptr);
    static inline bool isHundredString(const void *ptr);

    typedef uint8 SmallChar;
    static const SmallChar INVALID_SMALL_CHAR = -1;

    static inline bool fitsInSmallChar(jschar c);

    static const jschar fromSmallChar[];
    static const SmallChar toSmallChar[];

    static void staticAsserts() {
        JS_STATIC_ASSERT(sizeof(JSString::Data) == sizeof(JSString));
    }

    static JSStaticAtom &length2Static(jschar c1, jschar c2);
    static JSStaticAtom &length2Static(uint32 i);

  public:
    /*
     * While this query can be used for any pointer to GC thing, given a
     * JSString 'str', it is more efficient to use 'str->isStaticAtom()'.
     */
    static inline bool isStatic(const void *ptr);

    static inline bool hasIntStatic(int32 i);
    static inline JSStaticAtom &intStatic(jsint i);

    static inline bool hasUnitStatic(jschar c);
    static JSStaticAtom &unitStatic(jschar c);

    /* May not return atom, returns null on (reported) failure. */
    static inline JSLinearString *getUnitStringForElement(JSContext *cx, JSString *str, size_t index);

    /* Return null if no static atom exists for the given (chars, length). */
    static inline JSStaticAtom *lookupStatic(const jschar *chars, size_t length);

    inline void finalize(JSRuntime *rt);
};

JS_STATIC_ASSERT(sizeof(JSAtom) == sizeof(JSString));

class JSInlineAtom : public JSInlineString /*, JSAtom */
{
    /*
     * JSInlineAtom is not explicitly used and is only present for consistency.
     * See Atomize() for how JSInlineStrings get morphed into JSInlineAtoms.
     */
};

JS_STATIC_ASSERT(sizeof(JSInlineAtom) == sizeof(JSInlineString));

class JSShortAtom : public JSShortString /*, JSInlineAtom */
{
    /*
     * JSShortAtom is not explicitly used and is only present for consistency.
     * See Atomize() for how JSShortStrings get morphed into JSShortAtoms.
     */
};

JS_STATIC_ASSERT(sizeof(JSShortAtom) == sizeof(JSShortString));

class JSStaticAtom : public JSAtom
{};

JS_STATIC_ASSERT(sizeof(JSStaticAtom) == sizeof(JSString));

/* Avoid requring jsstrinlines.h just to call getChars. */

JS_ALWAYS_INLINE const jschar *
JSString::getChars(JSContext *cx)
{
    if (JSLinearString *str = ensureLinear(cx))
        return str->chars();
    return NULL;
}

JS_ALWAYS_INLINE const jschar *
JSString::getCharsZ(JSContext *cx)
{
    if (JSFlatString *str = ensureFlat(cx))
        return str->chars();
    return NULL;
}

JS_ALWAYS_INLINE JSLinearString *
JSString::ensureLinear(JSContext *cx)
{
    return isLinear()
           ? &asLinear()
           : asRope().flatten(cx);
}

JS_ALWAYS_INLINE JSFlatString *
JSString::ensureFlat(JSContext *cx)
{
    return isFlat()
           ? &asFlat()
           : isDependent()
             ? asDependent().undepend(cx)
             : asRope().flatten(cx);
}

JS_ALWAYS_INLINE JSFixedString *
JSString::ensureFixed(JSContext *cx)
{
    if (!ensureFlat(cx))
        return NULL;
    if (isExtensible()) {
        JS_ASSERT((d.lengthAndFlags & FLAT_MASK) == 0);
        JS_STATIC_ASSERT(EXTENSIBLE_FLAGS == (JS_BIT(2) | JS_BIT(3)));
        JS_STATIC_ASSERT(FIXED_FLAGS == JS_BIT(2));
        d.lengthAndFlags ^= JS_BIT(3);
    }
    return &asFixed();
}

namespace js {

/* Implemented in jsstrinlines.h */
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

extern JSString * JS_FASTCALL
js_ConcatStrings(JSContext *cx, JSString *s1, JSString *s2);

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

extern const bool js_isidstart[];
extern const bool js_isident[];

static inline bool
JS_ISIDSTART(int c)
{
    unsigned w = c;

    return (w < 128) ? js_isidstart[w] : JS_ISLETTER(c);
}

static inline bool
JS_ISIDENT(int c)
{
    unsigned w = c;

    return (w < 128) ? js_isident[w] : JS_ISIDPART(c);
}

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

extern const bool js_isspace[];

static inline bool
JS_ISSPACE(int c)
{
    unsigned w = c;

    return (w < 128)
           ? js_isspace[w]
           : w == NO_BREAK_SPACE || w == BYTE_ORDER_MARK ||
             (JS_CCODE(w) & 0x00070000) == 0x00040000;
}

static inline bool
JS_ISSPACE_OR_BOM(int c)
{
    unsigned w = c;

    /* Treat little- and big-endian BOMs as whitespace for compatibility. */
    return (w < 128)
           ? js_isspace[w]
           : w == NO_BREAK_SPACE || w == BYTE_ORDER_MARK ||
             (JS_CCODE(w) & 0x00070000) == 0x00040000 || w == 0xfffe || w == 0xfeff;
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
#define JS7_ISDECNZ(c)  ((((unsigned)(c)) - '1') <= 8)
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
extern JSFixedString *
js_NewString(JSContext *cx, jschar *chars, size_t length);

extern JSLinearString *
js_NewDependentString(JSContext *cx, JSString *base, size_t start, size_t length);

/* Copy a counted string and GC-allocate a descriptor for it. */
extern JSFixedString *
js_NewStringCopyN(JSContext *cx, const jschar *s, size_t n);

extern JSFixedString *
js_NewStringCopyN(JSContext *cx, const char *s, size_t n);

/* Copy a C string and GC-allocate a descriptor for it. */
extern JSFixedString *
js_NewStringCopyZ(JSContext *cx, const jschar *s);

extern JSFixedString *
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
inline bool
ValueToStringBuffer(JSContext *cx, const Value &v, StringBuffer &sb);

} /* namespace js */

/*
 * Convert a value to its source expression, returning null after reporting
 * an error, otherwise returning a new string reference.
 */
extern JS_FRIEND_API(JSString *)
js_ValueToSource(JSContext *cx, const js::Value &v);

namespace js {

/*
 * Compute a hash function from str. The caller can call this function even if
 * str is not a GC-allocated thing.
 */
inline uint32
HashChars(const jschar *chars, size_t length)
{
    uint32 h = 0;
    for (; length; chars++, length--)
        h = JS_ROTATE_LEFT32(h, 4) ^ *chars;
    return h;
}

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
 * Some string functions have an optional bool useCESU8 argument.
 * CESU-8 (Compatibility Encoding Scheme for UTF-16: 8-bit) is a
 * variant of UTF-8 that allows us to store any wide character
 * string as a narrow character string. For strings containing
 * mostly ascii, it saves space.
 * http://www.unicode.org/reports/tr26/
 */

/*
 * Inflate bytes to JS chars and vice versa.  Report out of memory via cx and
 * return null on error, otherwise return the jschar or byte vector that was
 * JS_malloc'ed. length is updated to the length of the new string in jschars.
 * Using useCESU8 = true treats 'bytes' as CESU-8.
 */
extern jschar *
js_InflateString(JSContext *cx, const char *bytes, size_t *length, bool useCESU8 = false);

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
 * Same as js_InflateStringToBuffer, but treats 'bytes' as UTF-8 or CESU-8.
 */
extern JSBool
js_InflateUTF8StringToBuffer(JSContext *cx, const char *bytes, size_t length,
                             jschar *chars, size_t *charsLength,
                             bool useCESU8 = false);

/*
 * Get number of bytes in the deflated sequence of characters. Behavior depends
 * on js_CStringsAreUTF8.
 */
extern size_t
js_GetDeflatedStringLength(JSContext *cx, const jschar *chars,
                           size_t charsLength);

/*
 * Same as js_GetDeflatedStringLength, but treats the result as UTF-8 or CESU-8.
 * This function will never fail (return -1) in CESU-8 mode.
 */
extern size_t
js_GetDeflatedUTF8StringLength(JSContext *cx, const jschar *chars,
                               size_t charsLength, bool useCESU8 = false);

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
 * Same as js_DeflateStringToBuffer, but treats 'bytes' as UTF-8 or CESU-8.
 */
extern JSBool
js_DeflateStringToUTF8Buffer(JSContext *cx, const jschar *chars,
                             size_t charsLength, char *bytes, size_t *length,
                             bool useCESU8 = false);

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
