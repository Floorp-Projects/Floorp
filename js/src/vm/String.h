/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79 ft=cpp:
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
 * The Original Code is SpiderMonkey JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Luke Wagner <luke@mozilla.com>
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

#ifndef String_h_
#define String_h_

#include "mozilla/Attributes.h"

#include "jsapi.h"
#include "jscell.h"

class JSString;
class JSDependentString;
class JSExtensibleString;
class JSExternalString;
class JSLinearString;
class JSFixedString;
class JSRope;
class JSAtom;

namespace js {

class StaticStrings;
class PropertyName;

/* The buffer length required to contain any unsigned 32-bit integer. */
static const size_t UINT32_CHAR_BUFFER_LENGTH = sizeof("4294967295") - 1;

/* N.B. must correspond to boolean tagging behavior. */
enum InternBehavior
{
    DoNotInternAtom = false,
    InternAtom = true
};

} /* namespace js */

/*
 * Find or create the atom for a string. Return null on failure to allocate
 * memory.
 */
extern JSAtom *
js_AtomizeString(JSContext *cx, JSString *str, js::InternBehavior ib = js::DoNotInternAtom);

/*
 * JavaScript strings
 *
 * Conceptually, a JS string is just an array of chars and a length. This array
 * of chars may or may not be null-terminated and, if it is, the null character
 * is not included in the length.
 *
 * To improve performance of common operations, the following optimizations are
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
 * JSFlatString (abstract)      chars / null-terminated
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
 * js::PropertyName             - / chars don't contain an index (uint32)
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
     * of the string. The remaining bits store the string length (which must be
     * less or equal than MAX_LENGTH).
     *
     * Instead of using a dense index to represent the most-derived type, string
     * types are encoded to allow single-op tests for hot queries (isRope,
     * isDependent, isFlat, isAtom, isStaticAtom) which, in view of subtyping,
     * would require slower (isX() || isY() || isZ()).
     *
     * The string type encoding can be summarized as follows. The "instance
     * encoding" entry for a type specifies the flag bits used to create a
     * string instance of that type. Abstract types have no instances and thus
     * have no such entry. The "subtype predicate" entry for a type specifies
     * the predicate used to query whether a JSString instance is subtype
     * (reflexively) of that type.
     *
     *   string       instance   subtype
     *   type         encoding   predicate
     *
     *   String       -          true
     *   Rope         0001       xxx1
     *   Linear       -          xxx0
     *   Dependent    0010       xx1x
     *   Flat         -          xx00
     *   Extensible   1100       1100
     *   Fixed        0100       isFlat && !isExtensible
     *   Inline       0100       isFixed && (u1.chars == inlineStorage || isShort)
     *   Short        0100       xxxx && header in FINALIZE_SHORT_STRING arena
     *   External     0100       xxxx && header in FINALIZE_EXTERNAL_STRING arena
     *   Atom         1000       x000
     *   InlineAtom   1000       1000 && is Inline
     *   ShortAtom    1000       1000 && is Short
     *   StaticAtom   0000       0000
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

    static const size_t EXTENSIBLE_FLAGS  = JS_BIT(2) | JS_BIT(3);
    static const size_t NON_STATIC_ATOM   = JS_BIT(3);

    size_t buildLengthAndFlags(size_t length, size_t flags) {
        return (length << LENGTH_SHIFT) | flags;
    }

    /*
     * Helper function to validate that a string of a given length is
     * representable by a JSString. An allocation overflow is reported if false
     * is returned.
     */
    static inline bool validateLength(JSContext *cx, size_t length);

    static void staticAsserts() {
        JS_STATIC_ASSERT(JS_BITS_PER_WORD >= 32);
        JS_STATIC_ASSERT(((JSString::MAX_LENGTH << JSString::LENGTH_SHIFT) >>
                           JSString::LENGTH_SHIFT) == JSString::MAX_LENGTH);
        JS_STATIC_ASSERT(sizeof(JSString) ==
                         offsetof(JSString, d.inlineStorage) + NUM_INLINE_CHARS * sizeof(jschar));
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
    JSRope &asRope() const {
        JS_ASSERT(isRope());
        return *(JSRope *)this;
    }

    JS_ALWAYS_INLINE
    bool isLinear() const {
        return (d.lengthAndFlags & LINEAR_MASK) == LINEAR_FLAGS;
    }

    JS_ALWAYS_INLINE
    JSLinearString &asLinear() const {
        JS_ASSERT(JSString::isLinear());
        return *(JSLinearString *)this;
    }

    JS_ALWAYS_INLINE
    bool isDependent() const {
        bool dependent = d.lengthAndFlags & DEPENDENT_BIT;
        JS_ASSERT_IF(dependent, (d.lengthAndFlags & FLAGS_MASK) == DEPENDENT_BIT);
        return dependent;
    }

    JS_ALWAYS_INLINE
    JSDependentString &asDependent() const {
        JS_ASSERT(isDependent());
        return *(JSDependentString *)this;
    }

    JS_ALWAYS_INLINE
    bool isFlat() const {
        return (d.lengthAndFlags & FLAT_MASK) == FLAT_FLAGS;
    }

    JS_ALWAYS_INLINE
    JSFlatString &asFlat() const {
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

    /* For hot code, prefer other type queries. */
    bool isShort() const;
    bool isFixed() const;
    bool isInline() const;

    JS_ALWAYS_INLINE
    JSFixedString &asFixed() const {
        JS_ASSERT(isFixed());
        return *(JSFixedString *)this;
    }

    bool isExternal() const;

    JS_ALWAYS_INLINE
    JSExternalString &asExternal() const {
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

    /* Only called by the GC for strings with the FINALIZE_STRING kind. */

    inline void finalize(JSContext *cx);

    /* Gets the number of bytes that the chars take on the heap. */

    JS_FRIEND_API(size_t) charsHeapSize(JSMallocSizeOfFun mallocSizeOf);

    /* Offsets for direct field from jit code. */

    static size_t offsetOfLengthAndFlags() {
        return offsetof(JSString, d.lengthAndFlags);
    }

    static size_t offsetOfChars() {
        return offsetof(JSString, d.u1.chars);
    }

    static inline void writeBarrierPre(JSString *str);
    static inline void writeBarrierPost(JSString *str, void *addr);
    static inline bool needWriteBarrierPre(JSCompartment *comp);
};

class JSRope : public JSString
{
    enum UsingBarrier { WithBarrier, NoBarrier };
    template<UsingBarrier b>
    JSFlatString *flattenInternal(JSContext *cx);

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

    /* Vacuous and therefore unimplemented. */
    JSLinearString *ensureLinear(JSContext *cx) MOZ_DELETE;
    bool isLinear() const MOZ_DELETE;
    JSLinearString &asLinear() const MOZ_DELETE;

  public:
    void mark(JSTracer *trc);

    JS_ALWAYS_INLINE
    const jschar *chars() const {
        JS_ASSERT(JSString::isLinear());
        return d.u1.chars;
    }
};

JS_STATIC_ASSERT(sizeof(JSLinearString) == sizeof(JSString));

class JSDependentString : public JSLinearString
{
    friend class JSString;
    JSFixedString *undepend(JSContext *cx);

    void init(JSLinearString *base, const jschar *chars, size_t length);

    /* Vacuous and therefore unimplemented. */
    bool isDependent() const MOZ_DELETE;
    JSDependentString &asDependent() const MOZ_DELETE;

  public:
    static inline JSDependentString *new_(JSContext *cx, JSLinearString *base,
                                          const jschar *chars, size_t length);

    JSLinearString *base() const {
        JS_ASSERT(JSString::isDependent());
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

    /* Vacuous and therefore unimplemented. */
    JSFlatString *ensureFlat(JSContext *cx) MOZ_DELETE;
    bool isFlat() const MOZ_DELETE;
    JSFlatString &asFlat() const MOZ_DELETE;

  public:
    JS_ALWAYS_INLINE
    const jschar *charsZ() const {
        JS_ASSERT(JSString::isFlat());
        return chars();
    }

    /*
     * Returns true if this string's characters store an unsigned 32-bit
     * integer value, initializing *indexp to that value if so.  (Thus if
     * calling isIndex returns true, js::IndexToString(cx, *indexp) will be a
     * string equal to this string.)
     */
    bool isIndex(uint32 *indexp) const;

    /*
     * Returns a property name represented by this string, or null on failure.
     * You must verify that this is not an index per isIndex before calling
     * this method.
     */
    inline js::PropertyName *toPropertyName(JSContext *cx);

    /* Only called by the GC for strings with the FINALIZE_STRING kind. */

    inline void finalize(JSRuntime *rt);
};

JS_STATIC_ASSERT(sizeof(JSFlatString) == sizeof(JSString));

class JSExtensibleString : public JSFlatString
{
    /* Vacuous and therefore unimplemented. */
    bool isExtensible() const MOZ_DELETE;
    JSExtensibleString &asExtensible() const MOZ_DELETE;

  public:
    JS_ALWAYS_INLINE
    size_t capacity() const {
        JS_ASSERT(JSString::isExtensible());
        return d.s.u2.capacity;
    }
};

JS_STATIC_ASSERT(sizeof(JSExtensibleString) == sizeof(JSString));

class JSFixedString : public JSFlatString
{
    void init(const jschar *chars, size_t length);

    /* Vacuous and therefore unimplemented. */
    JSFlatString *ensureFixed(JSContext *cx) MOZ_DELETE;
    bool isFixed() const MOZ_DELETE;
    JSFixedString &asFixed() const MOZ_DELETE;

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
    /* This can be any value that is a multiple of Cell::CellSize. */
    static const size_t INLINE_EXTENSION_CHARS = sizeof(JSString::Data) / sizeof(jschar);

    static void staticAsserts() {
        JS_STATIC_ASSERT(INLINE_EXTENSION_CHARS % js::gc::Cell::CellSize == 0);
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

    /* Vacuous and therefore unimplemented. */
    bool isExternal() const MOZ_DELETE;
    JSExternalString &asExternal() const MOZ_DELETE;

  public:
    static inline JSExternalString *new_(JSContext *cx, const jschar *chars,
                                         size_t length, intN type, void *closure);

    intN externalType() const {
        JS_ASSERT(JSString::isExternal());
        JS_ASSERT(d.s.u2.externalType < TYPE_LIMIT);
        return intN(d.s.u2.externalType);
    }

    void *externalClosure() const {
        JS_ASSERT(JSString::isExternal());
        return d.s.u3.externalClosure;
    }

    static const uintN TYPE_LIMIT = 8;
    static JSStringFinalizeOp str_finalizers[TYPE_LIMIT];

    static intN changeFinalizer(JSStringFinalizeOp oldop,
                                JSStringFinalizeOp newop) {
        for (uintN i = 0; i < mozilla::ArrayLength(str_finalizers); i++) {
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
    /* Vacuous and therefore unimplemented. */
    bool isAtom() const MOZ_DELETE;
    JSAtom &asAtom() const MOZ_DELETE;

  public:
    /* Returns the PropertyName for this.  isIndex() must be false. */
    inline js::PropertyName *asPropertyName();

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

namespace js {

class StaticStrings
{
  private:
    bool initialized;

    /* Bigger chars cannot be in a length-2 string. */
    static const size_t SMALL_CHAR_LIMIT    = 128U;
    static const size_t NUM_SMALL_CHARS     = 64U;

    static const size_t INT_STATIC_LIMIT    = 256U;

    JSAtom *length2StaticTable[NUM_SMALL_CHARS * NUM_SMALL_CHARS];
    JSAtom *intStaticTable[INT_STATIC_LIMIT];

  public:
    /* We keep these public for the methodjit. */
    static const size_t UNIT_STATIC_LIMIT   = 256U;
    JSAtom *unitStaticTable[UNIT_STATIC_LIMIT];

    StaticStrings() : initialized(false) {}

    bool init(JSContext *cx);
    void trace(JSTracer *trc);

    static inline bool hasUint(uint32 u);
    inline JSAtom *getUint(uint32 u);

    static inline bool hasInt(int32 i);
    inline JSAtom *getInt(jsint i);

    static inline bool hasUnit(jschar c);
    JSAtom *getUnit(jschar c);

    /* May not return atom, returns null on (reported) failure. */
    inline JSLinearString *getUnitStringForElement(JSContext *cx, JSString *str, size_t index);

    static bool isStatic(JSAtom *atom);

    /* Return null if no static atom exists for the given (chars, length). */
    inline JSAtom *lookup(const jschar *chars, size_t length);

  private:
    typedef uint8 SmallChar;
    static const SmallChar INVALID_SMALL_CHAR = -1;

    static inline bool fitsInSmallChar(jschar c);

    static const SmallChar toSmallChar[];

    JSAtom *getLength2(jschar c1, jschar c2);
    JSAtom *getLength2(uint32 i);
};

/*
 * Represents an atomized string which does not contain an index (that is, an
 * unsigned 32-bit value).  Thus for any PropertyName propname,
 * ToString(ToUint32(propname)) never equals propname.
 *
 * To more concretely illustrate the utility of PropertyName, consider that it
 * is used to partition, in a type-safe manner, the ways to refer to a
 * property, as follows:
 *
 *   - uint32 indexes,
 *   - PropertyName strings which don't encode uint32 indexes, and
 *   - jsspecial special properties (non-ES5 properties like object-valued
 *     jsids, JSID_EMPTY, JSID_VOID, E4X's default XML namespace, and maybe in
 *     the future Harmony-proposed private names).
 */
class PropertyName : public JSAtom
{};

JS_STATIC_ASSERT(sizeof(PropertyName) == sizeof(JSString));

} /* namespace js */

/* Avoid requiring vm/String-inl.h just to call getChars. */

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

inline js::PropertyName *
JSAtom::asPropertyName()
{
#ifdef DEBUG
    uint32 dummy;
    JS_ASSERT(!isIndex(&dummy));
#endif
    return static_cast<js::PropertyName *>(this);
}

#endif
